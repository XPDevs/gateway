/**
 * Gateway | Shared index format
 *
 * This single file defines the on-disk index format and the tokenizer.
 * It is loaded by the browser search worker (importScripts) and by the Node
 * build tools (require/import), so build-time and query-time tokenisation can
 * never drift apart.
 *
 * Index layout
 * ------------
 *   index/manifest.json          counts + shard table
 *   index/shard-0000.tok         token dictionary + postings, one shard per
 *                                 tokenShards in the manifest (uncompressed)
 *   index/shard-0001.doc.gz      document store, one shard per contiguous id
 *                                 range listed in the manifest (gzipped)
 *   index/tier.tok, tier.doc.gz  the hot tier: the first tierDocs documents
 *   index/core.json              small hot set for the first paint
 *
 * How the shard count is decided: 03-build-index.mjs picks it with
 * --doc-shards (1024 for the current build) and writes it into the manifest.
 * Everything that reads an index takes the count from the manifest rather than
 * from a constant here, because a constant that disagrees with the manifest
 * sends a query to the wrong shard and silently returns nothing.
 *
 * A token is assigned to a token shard by FNV-1a(token) % tokenShards.
 * A document is assigned to a doc shard by its id, through the contiguous id
 * ranges the manifest publishes, not by hashing the domain.
 * Token postings store global document ids, so the doc shard for any id is
 * found through that range.
 */
(function(root, factory) {
    const api = factory();
    if (typeof module === 'object' && module.exports) module.exports = api;
    root.GWFormat = api;
})(typeof self !== 'undefined' ? self : globalThis, function() {
    'use strict';

    // The number of ingest partitions to spread domains over. This is unrelated
    // to the index's shard count, which the build chooses and the manifest
    // publishes. It used to be called DOC_SHARDS and sat next to a TOKEN_SHARDS
    // that was never updated when the shard count changed to 1024.
    const INGEST_PARTITIONS = 512;

    // Field ids. Weights are the maximum score a token can contribute per
    // field; they are baked into the postings at build time so query time
    // only has to add bytes up.
    const FIELD_TITLE = 1;
    const FIELD_DOMAIN = 2;
    const FIELD_PATH = 4;
    const FIELD_DESC = 8;
    const FIELD_WEIGHT = [0, 96, 72, 40, 56, 0, 0, 0, 24, 0, 0, 0, 0, 0, 0, 0];

    // Provenance of a document's text. Surfaced in the UI so a reader can
    // always tell fetched page text from URL-derived text.
    const PROV_PAGE = 0;   // title/description read out of this exact page
    const PROV_SITE = 1;   // description read out of the site's own home page
    const PROV_URL = 2;    // no page fetch; text derived from the real URL

    const MAX_TOKEN_LEN = 32;
    const MIN_TOKEN_LEN = 2;
    const MAX_TITLE = 180;
    const MAX_DESC = 300;

    /** FNV-1a 32 bit. Same function on both sides of the wire. */
    function hash32(str) {
        let h = 0x811c9dc5;
        for (let i = 0; i < str.length; i++) {
            h ^= str.charCodeAt(i);
            h = (h + ((h << 1) + (h << 4) + (h << 7) + (h << 8) + (h << 24))) >>> 0;
        }
        return h >>> 0;
    }

    /**
     * Which ingest partition a domain belongs to. Build-time only, and stable
     * across runs so re-ingesting the same crawl rewrites the same partitions.
     */
    function ingestPartition(domain) {
        return hash32(domain) % INGEST_PARTITIONS;
    }

    /** @deprecated kept as the old name for ingestPartition */
    function docShard(domain) {
        return ingestPartition(domain);
    }

    /**
     * Split arbitrary text into index tokens.
     * Lowercased. Any character outside a-z, 0-9 and the apostrophe is a
     * separator, so accents are dropped by being treated as separators: "über"
     * yields ["ber"]. Build and query share this function, so it cannot cause a
     * mismatch, but do not "fix" one side only.
     */
    function tokenize(text) {
        const out = [];
        if (!text) return out;
        const s = String(text).toLowerCase();
        let start = -1;
        for (let i = 0; i <= s.length; i++) {
            const c = i < s.length ? s.charCodeAt(i) : 32;
            const isWord = (c >= 97 && c <= 122) || (c >= 48 && c <= 57) || c === 39;
            if (isWord) {
                if (start < 0) start = i;
            } else if (start >= 0) {
                pushToken(s, start, i, out);
                start = -1;
            }
        }
        return out;
    }

    function pushToken(s, start, end, out) {
        // trim leading/trailing apostrophes
        while (end > start && s.charCodeAt(end - 1) === 39) end--;
        while (start < end && s.charCodeAt(start) === 39) start++;
        const len = end - start;
        if (len < MIN_TOKEN_LEN) return;
        out.push(len > MAX_TOKEN_LEN ? s.slice(start, start + MAX_TOKEN_LEN) : s.slice(start, end));
    }

    /**
     * Query terms, de-duplicated, order preserved.
     *
     * There is no stop word list here. The English stop words live in crawl.js
     * and are only applied on the bundled JSON path; the sharded path searches
     * the dictionary as built, so removing words at query time would not match
     * how the index was written.
     */
    function queryTokens(text) {
        const seen = new Set();
        const out = [];
        for (const t of tokenize(text)) {
            if (t.length < MIN_TOKEN_LEN) continue;
            if (seen.has(t)) continue;
            seen.add(t);
            out.push(t);
        }
        return out;
    }

    // ---------------------------------------------------------------- varint

    function writeVarint(buf, value) {
        let v = value >>> 0;
        while (v >= 0x80) {
            buf.push((v & 0x7f) | 0x80);
            v = v >>> 7;
        }
        buf.push(v & 0x7f);
    }

    function readVarint(bytes, pos) {
        let result = 0;
        let shift = 0;
        while (pos < bytes.length) {
            const b = bytes[pos++];
            result |= (b & 0x7f) << shift;
            if ((b & 0x80) === 0) return [result >>> 0, pos];
            shift += 7;
            if (shift > 35) return [0, pos];
        }
        return [0, pos];
    }

    // ------------------------------------------------------------ doc store

    /**
     * Encode one document. Field order is fixed:
     *   varint domainLen, domain, varint pathLen, path,
     *   varint titleLen, title, varint descLen, desc,
     *   varint metaLen, meta (favicon url + image url, newline separated),
     *   byte prov, varint timestamp (days since epoch)
     *
     * The provenance byte is three bits, so the crawler scheme rides along in
     * bit 2 rather than adding a field. About 5% of Common Crawl urls are plain
     * http, and silently rewriting them to https turns those results into dead
     * links on sites that never got a certificate.
     */
    function encodeDoc(doc) {
        const buf = new Uint8Array(docSize(doc));
        encodeDocInto(buf, doc, 0);
        return buf;
    }

    /** Write a varint straight into a buffer, with no temporary array. */
    function writeVarintAt(buf, p, v) {
        v = v >>> 0;
        while (v >= 0x80) {
            buf[p++] = (v & 0x7f) | 0x80;
            v = Math.floor(v / 128);
        }
        buf[p++] = v;
        return p;
    }

    /** UTF-8 length of a string, counting a surrogate pair as the four bytes it takes. */
    function utf8Len(str) {
        let n = 0;
        for (let i = 0; i < str.length; i++) {
            const c = str.charCodeAt(i);
            if (c < 0x80) n += 1;
            else if (c < 0x800) n += 2;
            else if (c >= 0xd800 && c <= 0xdbff && i + 1 < str.length &&
                str.charCodeAt(i + 1) >= 0xdc00 && str.charCodeAt(i + 1) <= 0xdfff) { n += 4; i++; }
            else n += 3;
        }
        return n;
    }

    /** Bytes a varint needs for this value, up to the full 32 bit range. */
    function varintLen(v) {
        v = v >>> 0;
        return v < 0x80 ? 1 : v < 0x4000 ? 2 : v < 0x200000 ? 3 : v < 0x10000000 ? 4 : 5;
    }

    /** Exact encoded size of a document record. */
    function docSize(doc) {
        const fields = [doc.domain, doc.path, doc.title, doc.desc, doc.meta];
        let n = 1;
        for (let i = 0; i < fields.length; i++) {
            const l = utf8Len(fields[i] || '');
            n += varintLen(l) + l;
        }
        return n + varintLen(doc.ts >>> 0);
    }

    /**
     * Write one document into an existing buffer at an offset.
     *
     * encodeDoc builds a JavaScript array with one entry per byte, which for
     * twenty million documents is tens of gigabytes of short lived arrays. The
     * builder calls this instead and reuses a single buffer, so the whole build
     * stops allocating per document.
     */
    function encodeDocInto(buf, doc, off) {
        const fields = [doc.domain, doc.path, doc.title, doc.desc, doc.meta];
        let p = off;
        for (let i = 0; i < fields.length; i++) {
            const s = fields[i] || '';
            const len = utf8Len(s);
            p = writeVarintAt(buf, p, len);
            for (let k = 0; k < s.length; k++) {
                const c = s.charCodeAt(k);
                if (c < 0x80) {
                    buf[p++] = c;
                } else if (c < 0x800) {
                    buf[p++] = 0xc0 | (c >> 6);
                    buf[p++] = 0x80 | (c & 0x3f);
                } else if (c >= 0xd800 && c <= 0xdbff && k + 1 < s.length &&
                    s.charCodeAt(k + 1) >= 0xdc00 && s.charCodeAt(k + 1) <= 0xdfff) {
                    const cp = 0x10000 + ((c - 0xd800) << 10) + (s.charCodeAt(++k) - 0xdc00);
                    buf[p++] = 0xf0 | (cp >> 18);
                    buf[p++] = 0x80 | ((cp >> 12) & 0x3f);
                    buf[p++] = 0x80 | ((cp >> 6) & 0x3f);
                    buf[p++] = 0x80 | (cp & 0x3f);
                } else {
                    buf[p++] = 0xe0 | (c >> 12);
                    buf[p++] = 0x80 | ((c >> 6) & 0x3f);
                    buf[p++] = 0x80 | (c & 0x3f);
                }
            }
        }
        buf[p++] = (doc.prov & 3) | (doc.insecure ? 4 : 0);
        return writeVarintAt(buf, p, doc.ts >>> 0);
    }

    function decodeDoc(bytes, pos) {
        let p = pos;
        let r;
        [r, p] = readVarint(bytes, p); const domain = utf8Decode(bytes, p, r); p += r;
        [r, p] = readVarint(bytes, p); const path = utf8Decode(bytes, p, r); p += r;
        [r, p] = readVarint(bytes, p); const title = utf8Decode(bytes, p, r); p += r;
        [r, p] = readVarint(bytes, p); const desc = utf8Decode(bytes, p, r); p += r;
        [r, p] = readVarint(bytes, p); const meta = utf8Decode(bytes, p, r); p += r;
        const flags = bytes[p++] & 7;
        const prov = flags & 3;
        [r, p] = readVarint(bytes, p);
        return { domain, path, title, desc, meta, prov, insecure: (flags & 4) !== 0, ts: r, next: p };
    }

    // ----------------------------------------------------------------- utf8

    const ENCODER = typeof TextEncoder !== 'undefined' ? new TextEncoder() : null;
    const DECODER = typeof TextDecoder !== 'undefined' ? new TextDecoder() : null;

    function utf8(str) {
        if (ENCODER) return ENCODER.encode(str);
        const b = [];
        for (let i = 0; i < str.length; i++) {
            let c = str.charCodeAt(i);
            if (c < 0x80) b.push(c);
            else if (c < 0x800) b.push(0xc0 | (c >> 6), 0x80 | (c & 63));
            else b.push(0xe0 | (c >> 12), 0x80 | ((c >> 6) & 63), 0x80 | (c & 63));
        }
        return Uint8Array.from(b);
    }

    function utf8Decode(bytes, start, length) {
        if (DECODER) return DECODER.decode(bytes.subarray(start, start + length));
        let s = '';
        for (let i = start; i < start + length; i++) s += String.fromCharCode(bytes[i]);
        return s;
    }

    return {
        INGEST_PARTITIONS,
        FIELD_TITLE,
        FIELD_DOMAIN,
        FIELD_PATH,
        FIELD_DESC,
        FIELD_WEIGHT,
        PROV_PAGE,
        PROV_SITE,
        PROV_URL,
        MAX_TOKEN_LEN,
        MIN_TOKEN_LEN,
        MAX_TITLE,
        MAX_DESC,
        hash32,
        ingestPartition,
        docShard,
        tokenize,
        queryTokens,
        writeVarint,
        readVarint,
        encodeDoc,
        encodeDocInto,
        docSize,
        decodeDoc,
        utf8,
        utf8Decode
    };
});
