(function() {
    'use strict';

    const CACHE_TTL = 600000;
    const _cache = new Map();
    const LS_PREFIX = 'gw_';

    function _cached(k) {
        const e = _cache.get(k);
        if (e && Date.now() - e.t < CACHE_TTL) return e.d;
        _cache.delete(k);
        try {
            const raw = localStorage.getItem(LS_PREFIX + k);
            if (raw) {
                const parsed = JSON.parse(raw);
                if (Date.now() - parsed.t < CACHE_TTL) {
                    _cache.set(k, parsed);
                    return parsed.d;
                }
                localStorage.removeItem(LS_PREFIX + k);
            }
        } catch(_) {}
        return null;
    }
    function _store(k, d) {
        _cache.set(k, { d, t: Date.now() });
        try {
            localStorage.setItem(LS_PREFIX + k, JSON.stringify({ d, t: Date.now() }));
        } catch(_) {}
    }
    function _evictOldCache() {
        try {
            const keys = [];
            for (let i = 0; i < localStorage.length; i++) {
                const key = localStorage.key(i);
                if (key && key.startsWith(LS_PREFIX)) keys.push(key);
            }
            if (keys.length > 30) {
                const entries = keys.map(k => ({ k, t: JSON.parse(localStorage.getItem(k) || '{}').t || 0 }))
                    .sort((a, b) => a.t - b.t);
                for (let i = 0; i < entries.length - 30; i++) {
                    localStorage.removeItem(entries[i].k);
                }
            }
        } catch(_) {}
    }
    _evictOldCache();

    const DANGEROUS_DOMAINS = new Set([
        '4chan.org','8kun.top','bestgore.com','liveleak.com','kiwifarms.net',
        'freenode.net','anon-ib.com','bitcoinmix.org','hydramarket.org',
        'tor2web.org','onion.city','onion.to','onion.cab','onion.sh',
        'onion.link','onion.guide','exe.io','shorte.st','adf.ly',
        'bit.ly','tinyurl.com','ow.ly','goo.gl','is.gd','buff.ly',
        'tiny.cc','tr.im','x.co','short.cm'
    ]);

    const DANGEROUS_KEYWORDS = [
        /malware/i, /virus/i, /exploit/i, /crack\b/i, /keygen/i,
        /warez/i, /hack/i, /phish/i, /ransomware/i, /trojan/i
    ];

    function isSafeDomain(domain) {
        if (!domain) return true;
        const d = domain.toLowerCase().replace(/^www\./, '');
        if (DANGEROUS_DOMAINS.has(d)) return false;
        if (d.split('.').length > 3) return false;
        return true;
    }

    function isSafeResult(r) {
        if (!r.domain) return true;
        if (!isSafeDomain(r.domain)) return false;
        const text = (r.title + ' ' + (r.description || '')).toLowerCase();
        for (const re of DANGEROUS_KEYWORDS) {
            if (re.test(text)) return false;
        }
        return true;
    }

    function cleanTracking(urlStr) {
        if (!urlStr) return urlStr;
        try {
            const u = new URL(urlStr);
            const track = new Set(['utm_source','utm_medium','utm_campaign','utm_term','utm_content','fbclid','gclid','ref','source','mc_cid','mc_eid']);
            for (const k of u.searchParams.keys()) {
                if (track.has(k)) u.searchParams.delete(k);
            }
            return u.toString();
        } catch(_) { return urlStr; }
    }

    function _wikiDomain() {
        const lang = (window.gatewayLang || 'en').split('-')[0];
        const map = { en:'en',es:'es',fr:'fr',de:'de',it:'it',pt:'pt',ru:'ru',
            ja:'ja','zh':'zh',ko:'ko',ar:'ar',hi:'hi',bn:'bn',
            tr:'tr',nl:'nl',pl:'pl',sv:'sv',da:'da',fi:'fi',
            no:'no',cs:'cs',ro:'ro',hu:'hu',el:'el' };
        return (map[lang] || 'en') + '.wikipedia.org';
    }

    async function _fetch(url, timeoutMs) {
        const ac = new AbortController();
        const timer = setTimeout(() => ac.abort(), timeoutMs || 4000);
        try {
            const r = await fetch(url, { signal: ac.signal });
            if (!r.ok) throw new Error(String(r.status));
            return await r.json();
        } finally {
            clearTimeout(timer);
        }
    }

    let _indexData = null;

    function _openDB() {
        return new Promise(function(resolve, reject) {
            var req = indexedDB.open('GatewayIndex', 1);
            req.onupgradeneeded = function() {
                req.result.createObjectStore('index', { keyPath: 'id' });
            };
            req.onsuccess = function() { resolve(req.result); };
            req.onerror = function() { reject(req.error); };
        });
    }

    async function _loadIndexFromDB() {
        try {
            var db = await _openDB();
            return new Promise(function(resolve) {
                var tx = db.transaction('index', 'readonly');
                var store = tx.objectStore('index');
                var get = store.get('data');
                get.onsuccess = function() { resolve(get.result ? get.result.entries : null); };
                get.onerror = function() { resolve(null); };
            });
        } catch(_) { return null; }
    }

    async function _saveIndexToDB(data) {
        try {
            var db = await _openDB();
            return new Promise(function(resolve) {
                var tx = db.transaction('index', 'readwrite');
                var store = tx.objectStore('index');
                store.put({ id: 'data', entries: data });
                tx.oncomplete = function() { resolve(); };
                tx.onerror = function() { resolve(); };
            });
        } catch(_) {}
    }

    (async function() {
        if (window._GATEWAY_INDEX_DATA) {
            _indexData = window._GATEWAY_INDEX_DATA;
            await _saveIndexToDB(_indexData);
        } else {
            var saved = await _loadIndexFromDB();
            if (saved) _indexData = saved;
        }
    })();

    window.gatewaySetIndex = async function(data) {
        _indexData = data;
        await _saveIndexToDB(data);
    };

    function _mapIndexEntry(e, query) {
        const ql = query.toLowerCase();
        let score = 15;
        const text = (e.t + ' ' + e.d).toLowerCase();
        const terms = ql.split(/\s+/).filter(Boolean);
        for (const t of terms) {
            try {
                const re = new RegExp(t.replace(/[.*+?^${}()|[\]\\]/g, '\\$&'), 'g');
                score += ((text.match(re) || []).length) * 3;
            } catch(_) {}
        }
        if (e.t.toLowerCase() === ql) score += 500;
        else if (e.t.toLowerCase().startsWith(ql)) score += 200;
        return {
            title: e.t,
            url: e.u,
            description: e.d,
            fullSnippet: e.d,
            extract: e.d,
            thumbnail: null,
            source: 'index',
            sourceLabel: 'Index',
            resultType: 'web',
            score,
            domain: e.s,
            suggestion: null
        };
    }

    async function searchIndex(term) {
        const idx = _indexData;
        if (!idx) return [];
        const ql = term.toLowerCase();
        const terms = ql.split(/\s+/).filter(Boolean);
        if (!terms.length) return [];
        const matched = [];
        for (const e of idx) {
            const text = (e.t + ' ' + e.d).toLowerCase();
            if (terms.every(t => text.includes(t))) {
                matched.push(_mapIndexEntry(e, term));
                if (matched.length >= 20) break;
            }
        }
        return matched;
    }

    async function gatewayCrawl(term) {
        if (!term) return [];
        const key = 'full:' + term;
        const cached = _cached(key);
        if (cached) {
            const safe = cached.filter(isSafeResult);
            return safe.length ? safe : cached;
        }

        let indexed = await searchIndex(term);
        indexed.sort((a, b) => b.score - a.score);
        const seen = new Set();
        const deduped = [];
        for (const r of indexed) {
            const urlKey = r.url;
            if (seen.has(urlKey)) continue;
            seen.add(urlKey);
            deduped.push(r);
        }
        if (deduped.length) _store(key, deduped);
        return deduped;
    }

    async function getSuggestions(term) {
        if (!term || term.length < 2) return [];
        const key = 'sug:' + term;
        const cached = _cached(key);
        if (cached) return cached;

        try {
            const wikiDomain = _wikiDomain();
            const data = await _fetch(`https://${wikiDomain}/w/api.php?action=opensearch&format=json&origin=*&search=${encodeURIComponent(term)}&limit=8&namespace=0`, 2000);
            const suggestions = Array.isArray(data[1]) ? data[1] : [];
            _store(key, suggestions);
            return suggestions;
        } catch (_) {
            return [];
        }
    }

    async function spellCheck(term) {
        if (!term) return null;
        const key = 'spell:' + term;
        const cached = _cached(key);
        if (cached) return cached;

        try {
            const wikiDomain = _wikiDomain();
            const data = await _fetch(`https://${wikiDomain}/w/api.php?action=query&format=json&origin=*&list=search&srsearch=${encodeURIComponent(term)}&srlimit=1`, 2000);
            const s = data?.query?.searchinfo?.suggestion || null;
            if (s && s.toLowerCase() !== term.toLowerCase()) {
                _store(key, s);
                return s;
            }
            return null;
        } catch (_) {
            return null;
        }
    }

    window.gatewayCrawl = gatewayCrawl;
    window.getSuggestions = getSuggestions;
    window.gatewaySpellCheck = spellCheck;
})();
