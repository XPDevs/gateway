/**
 * Gateway search worker.
 *
 * Runs the sharded index off the main thread so typing stays responsive. It
 * answers in two rounds: core.json is already in memory, so the first batch of
 * results needs no network at all, and the shard fetches for the full index
 * follow. Results are posted as soon as each round finishes instead of waiting
 * for the slowest one.
 *
 * Shard files are kept in the browser Cache API, so a second visit reads them
 * from disk and the index warms up without touching the network.
 */
/* global importScripts */
importScripts('index-format.js', 'index-reader.js');

// index-format.js is still needed: index-reader.js uses it for the tokenizer,
// so it has to be in the worker's global scope before the reader is evaluated.
const { Index } = self.GWReader;
const CACHE_PREFIX = 'gw-index-';
// Per query timings, useful in devtools and nowhere else. Off by default so
// nothing is formatted for a log nobody reads.
const DEBUG = false;

let index = null;
let ready = false;
let pending = null;

/**
 * Open the cache for this exact build, and throw away caches from other builds.
 *
 * The version has to be part of the cache name. The shard file names are stable
 * across rebuilds, so keying on the file url alone would either serve postings
 * from an older index forever or, if the url were tested for the version and it
 * never is, throw the whole cache away on every single visit.
 */
async function openCache(version) {
    if (typeof caches === 'undefined') return null;
    try {
        const name = CACHE_PREFIX + version;
        if (caches.keys) {
            const names = await caches.keys();
            for (const other of names) {
                if (other !== name && other.indexOf(CACHE_PREFIX) === 0) {
                    await caches.delete(other);
                }
            }
        }
        return await caches.open(name);
    } catch (_) {
        return null;
    }
}

async function init(base) {
    const absolute = new URL(base || 'index/', self.location.href).href;
    index = new Index({
        base: absolute,
        // The hot tier and a couple of shards are enough for the first paint.
        // Beyond that the budget is spent on whatever the user is searching for.
        tokenBudget: 16 * 1024 * 1024,
        docBudget: 24 * 1024 * 1024
    });
    // The manifest is read before the cache exists, because the manifest is
    // what says which cache to use.
    await index.load('manifest.json');
    index.cache = await openCache(index.manifest.version);

    ready = true;
    self.postMessage({
        type: 'ready',
        version: index.manifest.version,
        source: index.manifest.source,
        docs: index.manifest.docs,
        tierDocs: index.manifest.tierDocs,
        coreDocs: index.core.docs.length,
        tokenShards: index.manifest.tokenShards,
        bytes: index.manifest.bytes
    });

    // Anything typed while the index was loading runs now.
    const queued = pending || [];
    pending = null;
    for (const msg of queued) run(msg.q, msg.queryId, msg.limit || 30);
}

/** Round one: core.json only. No fetch, no decompression, no shard lookup. */
function coreRound(q, queryId, limit) {
    const started = Date.now();
    const results = index.searchCore(q, limit);
    if (DEBUG) console.log('[gateway] core', queryId, Date.now() - started + 'ms', results.length + ' hits');
    self.postMessage({ type: 'core', queryId, results });
    return results;
}

/** Round two: the hot tier in memory, then one token shard per query term. */
async function tailRound(q, queryId, limit, coreIds) {
    const started = Date.now();
    const res = await index.search(q, limit * 2);
    const seen = new Set(coreIds);
    const results = [];
    for (const hit of res.results) {
        if (seen.has(hit.id)) continue;
        seen.add(hit.id);
        results.push(hit);
        if (results.length >= limit) break;
    }
    // The counts stay here rather than riding along on the message: the page
    // never read them, and every field on a hot path message is work.
    if (DEBUG) {
        console.log('[gateway] tail', JSON.stringify({
            queryId,
            ms: Date.now() - started,
            shards: res.shards.length,
            tokens: res.tokens,
            bytesFetched: index.stats.bytes,
            cacheHits: index.stats.cacheHits
        }));
    }
    self.postMessage({ type: 'tail', queryId, results });
    return results;
}

async function run(q, queryId, limit) {
    try {
        const core = coreRound(q, queryId, limit);
        const coreIds = core.map(r => r.id);
        await tailRound(q, queryId, limit, coreIds);
    } catch (err) {
        self.postMessage({ type: 'error', queryId, message: String(err && err.message || err) });
    }
}

self.onmessage = function (event) {
    const msg = event.data || {};
    if (msg.cmd === 'init') {
        init(msg.base).catch(err => self.postMessage({ type: 'error', message: String(err && err.message || err) }));
        return;
    }
    if (msg.cmd === 'query') {
        if (!ready) {
            pending = pending || [];
            pending.push(msg);
            return;
        }
        run(msg.q, msg.queryId, msg.limit || 30);
        return;
    }
};
