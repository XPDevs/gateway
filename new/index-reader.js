/**
 * Gateway search index reader.
 *
 * Works in the browser and in Node. The index is a set of sharded binary
 * files described by manifest.json:
 *
 *   core.json       the highest scoring documents, always in memory
 *   tier.tok        postings for the hot tier, always in memory
 *   tier.doc.gz     documents for the hot tier, fetched once
 *   shard-NNNN.tok  one token shard per hash bucket, fetched on demand
 *   shard-NNNN.doc.gz documents for a range of document ids, fetched on demand
 *
 * Token shards and document shards are cached in memory behind a byte budget
 * and, in the browser, in the Cache API so a second visit skips the network.
 * Nothing here blocks on layout or paint, so it is safe to run in a worker.
 */
(function (root, factory) {
    if (typeof module === 'object' && module.exports) module.exports = factory(require('./index-format.js'));
    else root.GWReader = factory(root.GWFormat);
})(typeof self !== 'undefined' ? self : this, function (GW) {
    'use strict';

    const FETCH = typeof fetch === 'function' ? fetch : null;

    // LRU with a byte budget. Both caches are small on purpose: the point is to
    // answer the first query without downloading the whole index.
    class Lru {
        constructor(budget) {
            this.budget = budget;
            this.used = 0;
            this.map = new Map();
        }
        get(key) {
            const hit = this.map.get(key);
            if (hit === undefined) return undefined;
            this.map.delete(key);
            this.map.set(key, hit);
            return hit.value;
        }
        set(key, value, bytes) {
            if (this.map.has(key)) this.used -= this.map.get(key).bytes;
            this.map.delete(key);
            this.map.set(key, { value, bytes });
            this.used += bytes;
            while (this.used > this.budget && this.map.size > 1) {
                const oldest = this.map.keys().next().value;
                this.used -= this.map.get(oldest).bytes;
                this.map.delete(oldest);
            }
        }
        clear() { this.map.clear(); this.used = 0; }
    }

    async function gunzip(buffer) {
        if (typeof DecompressionStream === 'undefined') {
            const zlib = typeof require === 'function' ? require('zlib') : null;
            if (zlib) return new Uint8Array(zlib.gunzipSync(Buffer.from(buffer)));
            throw new Error('no gzip decoder available');
        }
        const stream = new Blob([buffer]).stream().pipeThrough(new DecompressionStream('gzip'));
        return new Uint8Array(await new Response(stream).arrayBuffer());
    }

    // ------------------------------------------------------------- token shard

    const DAY_MS = 86400000;

    /**
     * Put a document from a binary shard into the same shape core.json uses.
     *
     * The shards pack favicon, image, site name and language into one pipe
     * joined `meta` string to keep the record small, and store the crawl date as
     * a day number. core.json holds the same information already unpacked, with
     * the date in milliseconds. The reader used to hand out the raw shard shape
     * and leave the caller to know which round a result came from, which meant
     * favicon and date coverage silently differed between the first and the
     * second round of the same search.
     */
    function shapeDoc(d) {
        const parts = (d.meta || '').split('|');
        d.favicon = parts[0] || '';
        d.image = parts[1] || '';
        d.siteName = parts[2] || '';
        d.lang = parts[3] || '';
        d.ts = d.ts ? d.ts * DAY_MS : 0;
        return d;
    }

    /**
     * "GWT1" | u32 tokens | u32 dictBytes | u32 postingBytes | u32 idBase
     * u32 dictOffset[tokens]
     * u32 postOffset[tokens]
     * dictionary: repeated [varint len][token bytes]
     * postings:   repeated [varint count][ varint docIdDelta, u8 weight ]
     */
    function parseTokenShard(bytes) {
        const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
        if (String.fromCharCode(bytes[0], bytes[1], bytes[2], bytes[3]) !== 'GWT1') throw new Error('bad token shard');
        const tokens = view.getUint32(4, true);
        const dictBytes = view.getUint32(8, true);
        const postBytes = view.getUint32(12, true);
        // Two u32 tables: dictionary offsets then posting offsets.
        const dictTable = 20;
        const postTable = dictTable + tokens * 4;
        const dictAt = postTable + tokens * 4;
        const postAt = dictAt + dictBytes;
        return {
            bytes, view, tokens, dictAt, postAt, postEnd: postAt + postBytes,
            dictTable, postTable,
            idBase: view.getUint32(16, true)
        };
    }

    function dictToken(shard, i) {
        const off = shard.view.getUint32(shard.dictTable + i * 4, true);
        let p = shard.dictAt + off;
        let len = 0, shift = 0, b;
        do { b = shard.bytes[p++]; len += (b & 0x7f) << shift; shift += 7; } while (b & 0x80);
        return String.fromCharCode.apply(null, shard.bytes.subarray(p, p + len));
    }

    function lookupToken(shard, token) {
        let lo = 0;
        let hi = shard.tokens - 1;
        while (lo <= hi) {
            const mid = (lo + hi) >> 1;
            const probe = dictToken(shard, mid);
            if (probe === token) return mid;
            if (probe < token) lo = mid + 1; else hi = mid - 1;
        }
        return -1;
    }

    /**
     * Returns a flat [docId, weight, docId, weight, ...] list for a token.
     *
     * A query token is matched exactly first, then against dictionary entries
     * that start with it, so "drone" finds "dronemasters" without shipping a
     * stemmer or a full term prefix index. The expansion is bounded so one
     * short token cannot drag in a whole shard.
     */
    /** First dictionary index whose token is >= the given prefix. */
    function lowerBound(shard, prefix) {
        let lo = 0;
        let hi = shard.tokens;
        while (lo < hi) {
            const mid = (lo + hi) >> 1;
            if (dictToken(shard, mid) < prefix) lo = mid + 1; else hi = mid;
        }
        return lo;
    }

    /**
     * Returns a flat [docId, weight, docId, weight, ...] list for a token.
     *
     * A query token is matched exactly first, then against dictionary entries
     * that start with it, so "drone" finds "dronemasters" without shipping a
     * stemmer or a separate prefix index. The expansion is bounded so one short
     * token cannot drag in a whole shard.
     */
    function postings(shard, token, limit) {
        const max = limit || 6;
        const hits = [];
        let i = lowerBound(shard, token);
        for (let n = 0; i < shard.tokens && n < max; i++, n++) {
            const candidate = dictToken(shard, i);
            if (candidate.lastIndexOf(token, 0) !== 0) break;
            hits.push(i);
        }
        if (!hits.length) return null;
        if (hits.length === 1) return postingsAt(shard, hits[0]);
        return mergePostings(hits.map(k => postingsAt(shard, k)));
    }

    function mergePostings(lists) {
        let total = 0;
        for (const l of lists) if (l) total += l.length / 2;
        const out = new Uint32Array(total * 2);
        let n = 0;
        for (const l of lists) {
            if (!l) continue;
            for (let i = 0; i < l.length; i++) out[n++] = l[i];
        }
        return out;
    }

    function postingsAt(shard, at) {
        let p = shard.dictAt + shard.view.getUint32(shard.dictTable + at * 4, true);
        let len = 0, shift = 0, b;
        do { b = shard.bytes[p++]; len += (b & 0x7f) << shift; shift += 7; } while (b & 0x80);
        p = shard.postAt + shard.view.getUint32(shard.postTable + at * 4, true);
        let count = 0;
        shift = 0;
        do { b = shard.bytes[p++]; count += (b & 0x7f) << shift; shift += 7; } while (b & 0x80);
        const out = new Uint32Array(count * 2);
        // Deltas are stored per token, so the running id starts at the first
        // document id in the shard rather than at zero.
        let id = shard.idBase;
        for (let i = 0; i < count; i++) {
            let delta = 0;
            shift = 0;
            do { b = shard.bytes[p++]; delta += (b & 0x7f) << shift; shift += 7; } while (b & 0x80);
            id += delta;
            out[i * 2] = id;
            out[i * 2 + 1] = shard.bytes[p++];
        }
        return out;
    }

    /** Scan every token in a shard once, used to build a small hot dictionary. */
    function eachToken(shard, visit) {
        for (let i = 0; i < shard.tokens; i++) visit(dictToken(shard, i), i);
    }

    // ------------------------------------------------------------------ index

    class Index {
        constructor(options) {
            const opts = options || {};
            this.base = (opts.base || '') ;
            this.tokenBudget = opts.tokenBudget || 24 * 1024 * 1024;
            this.docBudget = opts.docBudget || 24 * 1024 * 1024;
            this.tokens = new Lru(this.tokenBudget);
            this.docs = new Lru(this.docBudget);
            this.cache = opts.cache || null;
            // Tests and offline builds can supply their own byte loader
            // instead of going through fetch.
            this.loadBytes = opts.loadBytes || null;
            this.manifest = null;
            this.core = null;
            this.tier = null;
            this.docShards = [];
            this.stats = { tokensFetched: 0, docShardsFetched: 0, bytes: 0, cacheHits: 0 };
        }

        async load(manifestUrl) {
            // Every other fetch here prefixes this.base. This one did not, and a
            // worker resolves relative URLs against its own script URL, so this
            // asked the site root for manifest.json instead of index/manifest.json
            // and got a 404. That one missing prefix left the whole sharded index
            // unreachable from the browser while Node-side tools, which build
            // their own paths, kept working.
            const res = await this._raw(this.base + manifestUrl);
            this.manifest = JSON.parse(new TextDecoder().decode(res));
            const m = this.manifest;
            this.tierDocs = m.tierDocs || 0;
            this.docShards = m.docShards || [];
            this.tokenShards = m.tokenShards || 1;
            this.totalDocs = m.docs || 0;
            const coreRes = await this._raw(this.base + (m.coreFile || 'core.json'));
            this.core = JSON.parse(new TextDecoder().decode(coreRes));
            const tierRes = await this._raw(this.base + m.tierFile);
            this.tier = parseTokenShard(new Uint8Array(tierRes));
            return this;
        }

        async _raw(url) {
            if (this.loadBytes) return this.loadBytes(url);
            if (this.cache) {
                const hit = await this.cache.match(url);
                if (hit) { this.stats.cacheHits++; return new Uint8Array(await hit.arrayBuffer()); }
            }
            if (!FETCH) throw new Error('no fetch available');
            const res = await FETCH(url);
            if (!res.ok) throw new Error(url + ' -> ' + res.status);
            const buf = new Uint8Array(await res.arrayBuffer());
            this.stats.bytes += buf.length;
            if (this.cache) await this.cache.put(url, new Response(buf, {
                headers: { 'content-type': 'application/octet-stream' }
            }));
            return buf;
        }

        async tokenShard(shard) {
            const key = 'tok' + shard;
            const hit = this.tokens.get(key);
            if (hit) return hit;
            const file = 'shard-' + String(shard).padStart(4, '0') + '.tok';
            let bytes;
            try {
                bytes = await this._raw(this.base + file);
            } catch (e) {
                // Only a 404 means an empty bucket. A hash bucket that held no
                // terms has no file, and the query should get no hits from it and
                // move on. Anything else - a 500, a truncated download, a renamed
                // file - is a real failure, and swallowing it here would silently
                // truncate the result set instead of saying so.
                if (!/\s->\s404$/.test(String(e && e.message))) throw e;
                const empty = { dictCount: 0, tokens: [], dict: new Uint8Array(0), post: new Uint8Array(0) };
                this.tokens.set(key, empty, 0);
                return empty;
            }
            const parsed = parseTokenShard(bytes);
            this.stats.tokensFetched++;
            this.tokens.set(key, parsed, bytes.length);
            return parsed;
        }

        docShardIndex(docId) {
            let lo = 0;
            let hi = this.docShards.length - 1;
            while (lo <= hi) {
                const mid = (lo + hi) >> 1;
                const s = this.docShards[mid];
                if (docId < s.start) hi = mid - 1;
                else if (docId >= s.start + s.docs) lo = mid + 1;
                else return mid;
            }
            return -1;
        }

        async _docBytes(docId) {
            if (docId < this.tierDocs) {
                const key = 'tier';
                let hit = this.docs.get(key);
                if (!hit) {
                    hit = { byId: new Map(), order: [] };
                    const raw = await gunzip(await this._raw(this.base + this.manifest.tierDocFile));
                    let p = 0;
                    let id = 0;
                    while (p < raw.length) {
                        const d = shapeDoc(GW.decodeDoc(raw, p));
                        p = d.next;
                        hit.byId.set(id++, d);
                    }
                    this.stats.docShardsFetched++;
                    this.docs.set(key, hit, raw.length);
                }
                return hit.byId.get(docId);
            }
            const at = this.docShardIndex(docId);
            if (at < 0) return undefined;
            const file = this.docShards[at];
            const key = 'doc' + file.id;
            let hit = this.docs.get(key);
            if (!hit) {
                hit = { byId: new Map() };
                const raw = await gunzip(await this._raw(this.base + file.file));
                const start = file.start;
                let p = 0;
                let id = start;
                while (p < raw.length) {
                    const d = shapeDoc(GW.decodeDoc(raw, p));
                    p = d.next;
                    hit.byId.set(id++, d);
                }
                this.stats.docShardsFetched++;
                this.docs.set(key, hit, raw.length);
            }
            return hit.byId.get(docId);
        }

        /** Decode a document, using the core list first because it is already in memory. */
        async doc(docId) {
            if (docId < this.core.docs.length) return this.core.docs[docId];
            return this._docBytes(docId);
        }

        /**
         * Score with a saturating tf-idf term score. Exact BM25 needs document
         * frequencies per token, which the shard header does not carry, so this
         * weights rare tokens higher using the posting list length as a proxy:
         * a token in two documents is far more informative than one in a
         * million. That keeps the hot tier and the 20M doc tail comparable
         * without storing per token statistics.
         */
        static score(postings, tokenWeight, totalDocs) {
            const n = postings.length / 2;
            // An empty Map, not 0. Every caller iterates the result, so returning
            // a number threw "part is not iterable" and killed the whole search
            // whenever one query term happened to be absent from the hot tier.
            if (n === 0) return new Map();
            // The document count comes from the manifest. It was hardcoded to
            // 20,000,000, which only affects how sharply rare tokens are
            // weighted and was quietly wrong for any other size.
            const idf = Math.log(1 + (totalDocs || 20000000) / n);
            const out = new Map();
            for (let i = 0; i < n; i++) {
                const id = postings[i * 2];
                const w = postings[i * 2 + 1];
                const tf = 1 + Math.log(w * tokenWeight / 64 + 1);
                out.set(id, (out.get(id) || 0) + tf * idf);
            }
            return out;
        }

        /**
         * Score the in memory core set with no network at all.
         *
         * Every core document carries the same token set the index was built
         * from, so a query can be answered from core.json alone. This is the
         * path that paints the first results, and it stays usable when the
         * shard files are still downloading.
         */
        searchCore(query, limit) {
            const wanted = GW.queryTokens(query);
            if (!wanted.length || !this.core) return [];
            const scores = new Map();
            const t0 = Date.now();
            for (let i = 0; i < this.core.docs.length; i++) {
                const doc = this.core.docs[i];
                const hay = (doc.k || (doc.d + ' ' + doc.p + ' ' + doc.t + ' ' + (doc.x || ''))).toLowerCase();
                let score = 0;
                for (const token of wanted) {
                    // Token boundaries, the same rule the shard round uses when
                    // it looks a token up in a dictionary. This used to accept a
                    // bare substring, so a search for "ac" matched a page titled
                    // "effeff.ac" in the core round and nothing like it in the
                    // second, and the two halves of one result list disagreed
                    // about what a match was.
                    const whole = new RegExp('(^|[^a-z0-9])' + token + '([^a-z0-9]|$)').test(hay);
                    if (!whole) continue;
                    // Require every query token before ranking a document highly,
                    // so a one word match cannot outrank a full match.
                    score += 1;
                }
                if (score) scores.set(i, score / wanted.length);
            }
            const top = Array.from(scores.entries())
                .sort((a, b) => b[1] - a[1] || a[0] - b[0])
                .slice(0, limit || 50);
            this.stats.coreMs = (this.stats.coreMs || 0) + (Date.now() - t0);
            return top.map(([id, score]) => ({
                id,
                score: Math.round(score * 1000) / 1000,
                doc: this.core.docs[id],
                tier: 'core'
            }));
        }

        /** Score the hot tier, which is in memory and needs no fetch. */
        searchTier(query, limit) {
            const wanted = GW.queryTokens(query);
            if (!wanted.length || !this.tier) return [];
            const scores = new Map();
            for (const token of wanted) {
                const part = Index.score(postings(this.tier, token) || new Uint32Array(0), 64, this.totalDocs);
                for (const [id, score] of part) scores.set(id, (scores.get(id) || 0) + score);
            }
            return this._top(this.tier, scores, limit);
        }

        async _top(shard, scores, limit) {
            const top = Array.from(scores.entries())
                .sort((a, b) => b[1] - a[1])
                .slice(0, limit || 50);
            const out = [];
            for (const [id, score] of top) {
                const doc = await this.doc(id);
                if (!doc) continue;
                out.push({ id, score: Math.round(score * 100) / 100, doc, tier: 'index' });
            }
            return out;
        }

        /**
         * Search the sharded tail. The hot tier is folded in from memory and
         * the caller can merge these results with searchCore, which needs no
         * network at all.
         */
        async search(query, limit) {
            const wanted = GW.queryTokens(query);
            if (!wanted.length) return { results: [], tokens: [], shards: [] };
            const scores = new Map();
            for (const token of wanted) {
                const part = Index.score(postings(this.tier, token) || new Uint32Array(0), 64, this.totalDocs);
                for (const [id, score] of part) scores.set(id, (scores.get(id) || 0) + score);
            }
            const shards = new Set();
            for (const token of wanted) shards.add(GW.hash32(token) % this.tokenShards);
            const loaded = new Map();
            for (const shard of shards) {
                const parsed = await this.tokenShard(shard);
                loaded.set(shard, parsed);
                for (const token of wanted) {
                    const list = postings(parsed, token);
                    if (!list) continue;
                    const part = Index.score(list, 64, this.totalDocs);
                    for (const [id, score] of part) {
                        if (id < this.tierDocs) continue;
                        scores.set(id, (scores.get(id) || 0) + score);
                    }
                }
            }
            const results = await this._top(null, scores, limit || 50);
            return { results, tokens: wanted, shards: Array.from(shards) };
        }
    }

    return { Index, Lru, parseTokenShard, postings, lookupToken, eachToken, dictToken, gunzip };
});
