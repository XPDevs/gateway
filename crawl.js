(function() {
    'use strict';

    const CACHE_TTL = 600000;
    const MAX_RESULTS = 100;
    const MAX_INDEX_RESULTS = 120;
    const MAX_WEB_LINKS = 30;
    const _cache = new Map();
    const _inflight = new Map();
    // Bump the prefix when result-shaping or cache semantics change so old
    // browser data cannot be mistaken for the current format.
    const LS_PREFIX = 'gw_v4_';
    let _indexRevision = 'bundled';
    let _indexOverridden = false;

    const STOPWORDS = new Set('a an and as at be but by for from had has have he her his how i if in is it its of'
        + ' on or she so that the their them they this to was we were what when where who will with you your do does'
        + ' did not no are can could would should may might tell me show find about release date released launch launched'
        .split(' '));

    const MONTHS = 'January|February|March|April|May|June|July|August|September|October|November|December'
        + '|Jan(?:uary)?|Feb(?:ruary)?|Mar(?:ch)?|Apr(?:il)?|Jun(?:e)?|Jul(?:y)?|Aug(?:ust)?'
        + '|Sep(?:tember)?|Sept(?:ember)?|Oct(?:ober)?|Nov(?:ember)?|Dec(?:ember)?';
    const DATE_RE = new RegExp('(?:(?:' + MONTHS + ')\\s+\\d{1,2}(?:st|nd|rd|th)?(?:,?\\s+|\\s+)\\d{4}'
        + '|\\d{1,2}\\s+(?:' + MONTHS + ')(?:,?\\s+)\\d{4}'
        + '|(?:' + MONTHS + ')\\s+\\d{4}'
        + '|\\d{1,2}[/-]\\d{1,2}[/-]\\d{2,4}'
        + '|\\d{4}-\\d{2}-\\d{2}'
        + '|(?:18|19|20)\\d{2})', 'gi');

    const DATE_INTENT_IDS = new Set(['release', 'founded', 'published', 'discovered', 'born', 'died']);
    const QUICK_INTENTS = [
        { id: 'release', label: 'Release date', properties: ['P577', 'P571', 'P1191', 'P1619'],
            keywords: /\b(release date|released?|launch(?:ed)?|come out|unveil(?:ed)?|debut(?:ed)?|issu(?:ed)?)\b/i },
        { id: 'founded', label: 'Founded', properties: ['P571'],
            keywords: /\b(founded|established|formed|created|built)\b/i },
        { id: 'published', label: 'Publication date', properties: ['P577'],
            keywords: /\b(publish(?:ed)?|publication date)\b/i },
        { id: 'discovered', label: 'Date of discovery', properties: ['P575'],
            keywords: /\b(discover(?:ed)?|invent(?:ed)?)\b/i },
        { id: 'born', label: 'Date of birth', properties: ['P569'],
            keywords: /\b(born|birth date|date of birth)\b/i },
        { id: 'died', label: 'Date of death', properties: ['P570'],
            keywords: /\b(died|death date|date of death)\b/i },
        { id: 'capital', label: 'Capital', properties: ['P36'],
            keywords: /\b(capital)\b/i },
        { id: 'population', label: 'Population', properties: ['P1082'],
            keywords: /\b(population|how many people)\b/i },
        { id: 'headquarters', label: 'Headquarters', properties: ['P159'],
            keywords: /\b(headquarters|headquartered)\b/i },
        { id: 'founder', label: 'Founder', properties: ['P112'],
            keywords: /\b(founder|founded by)\b/i },
        { id: 'creator', label: 'Creator', properties: ['P170', 'P112'],
            keywords: /\b(creator|created by|designed by|developer)\b/i },
        { id: 'director', label: 'Director', properties: ['P57'],
            keywords: /\b(director|directed by)\b/i },
        { id: 'author', label: 'Author', properties: ['P50'],
            keywords: /\b(author|written by)\b/i },
        { id: 'manufacturer', label: 'Manufacturer', properties: ['P176'],
            keywords: /\b(manufacturer|manufactured by|made by)\b/i },
        { id: 'publisher', label: 'Publisher', properties: ['P123'],
            keywords: /\b(publisher|published by)\b/i },
        { id: 'location', label: 'Location', properties: ['P131', 'P17', 'P276', 'P495'],
            keywords: /\b(where is|where was|located|location|country| situated)\b/i },
        { id: 'language', label: 'Language', properties: ['P293', 'P37'],
            keywords: /\b(language|official language)\b/i },
        { id: 'genre', label: 'Genre', properties: ['P136'],
            keywords: /\b(genre)\b/i }
    ];

    function _normalize(s) {
        return String(s == null ? '' : s)
            .toLowerCase()
            .normalize('NFKC')
            .replace(/[\u2018\u2019\u201c\u201d\u2013\u2014]/g, '')
            .normalize('NFD').replace(/[\u0300-\u036f]/g, '')
            .normalize('NFC')
            .replace(/[^\p{L}\p{N}\s]/gu, ' ')
            .replace(/\s+/g, ' ').trim();
    }

    function _unique(values) {
        return Array.from(new Set(values.filter(Boolean)));
    }

    function _firstSentences(text, maxChars) {
        const clean = (text || '').replace(/\s+/g, ' ').trim();
        if (!clean) return '';
        if (clean.length <= maxChars) return clean;
        const part = clean.slice(0, maxChars);
        const sentenceEnd = Math.max(part.lastIndexOf('. '), part.lastIndexOf('! '), part.lastIndexOf('? '));
        return (sentenceEnd > maxChars * 0.45 ? part.slice(0, sentenceEnd + 1) : part).trim() + '…';
    }

    function _intentById(id) {
        return QUICK_INTENTS.find(item => item.id === id) || null;
    }

    function _detectIntent(query) {
        const q = String(query || '');

        if (/^\s*(?:where|what\s+country)\b/i.test(q)) return _intentById('location');

        // Questions such as "who founded X" and "who is the founder of X"
        // ask for a person or organisation, even though the same words are
        // also used when asking for a founding date. Resolve that ambiguity
        // from the question form before falling back to keyword matching.
        const roleMatch = q.match(/\b(founder|creator|director|author|manufacturer|publisher)\b/i);
        if (roleMatch) {
            const role = roleMatch[1].toLowerCase();
            const roleIntents = {
                founder: 'founder', creator: 'creator', director: 'director',
                author: 'author', manufacturer: 'manufacturer', publisher: 'publisher'
            };
            if (/^\s*(?:who|what|whose)\b/i.test(q) || /\b(?:is|was|are|were)\b/i.test(q)) {
                return _intentById(roleIntents[role]);
            }
        }

        const actorMatch = q.match(/^\s*(?:who|what)\s+(founded|formed|built|created|developed|discovered|invented)\b/i);
        if (actorMatch) {
            return _intentById(/founded|formed/.test(actorMatch[1]) ? 'founder' : 'creator');
        }

        // A date question should use the date intent, even when the subject
        // also contains a word such as "published" or "created".
        if (/^\s*(?:when|what\s+(?:date|day|year)|date\s+of)\b/i.test(q)) {
            if (/\b(?:release|released|launch|launched|come\s+out|unveil|unveiled|debut|debuted|issue|issued)\b/i.test(q)) {
                return _intentById('release');
            }
            if (/\b(?:publish|published)\b/i.test(q)) return _intentById('published');
            if (/\b(?:found|founded|establish|established|form|formed|create|created|built)\b/i.test(q)) {
                return _intentById('founded');
            }
            if (/\b(?:discover|discovered|invent|invented)\b/i.test(q)) return _intentById('discovered');
            if (/\b(?:born|birth)\b/i.test(q)) return _intentById('born');
            if (/\b(?:died|death)\b/i.test(q)) return _intentById('died');
        }

        return QUICK_INTENTS.find(item => item.keywords.test(q)) || null;
    }

    function _cleanSubject(value) {
        return String(value == null ? '' : value)
            .replace(/^[\s"'“”‘’「」『』]+|[\s.!?。！？]+$/gu, '')
            .replace(/^(?:the|a|an)\s+/i, '')
            .replace(/\s+/g, ' ')
            .trim();
    }

    function parseQuickWikiQuery(query) {
        const original = (query || '').trim();
        const q = original.replace(/\s+/g, ' ');
        let subject = '';
        let role = '';

        // Handle the natural "who/what is the <role> of X" form separately so
        // the role is not mistaken for part of the subject.
        const roleMatch = q.match(/^(?:who|what)\s+(?:is|was|are|were)\s+(?:the\s+)?(founder|creator|director|author|manufacturer|publisher)\s+of\s+(.+)$/i);
        if (roleMatch) {
            role = roleMatch[1].toLowerCase();
            subject = _cleanSubject(roleMatch[2]);
        }

        const patterns = [
            /^(?:what|which)\s+(?:is|was)\s+(?:the\s+)?(?:capital|population|official\s+language|language|genre|headquarters)\s+of\s+(.+)$/i,
            /^(?:what|which)\s+country\s+(?:was|is)\s+(.+?)\s+(?:founded|created|formed|established)\s+in\s*$/i,
            /^(?:tell me about|what do you know about|give me information about)\s+(.+)$/i,
            /^(?:how many people\s+(?:live|lived)\s+in|what is the population of)\s+(.+)$/i,
            /^(?:what(?:'s| is| was)?\s+)?(?:the\s+)?release\s+(?:date|dat)\s+(?:of|for)\s+(.+)$/i,
            /^(?:on\s+)?what\s+(?:date|day|year)\s+(?:was|did|were)?\s*(?:the\s+)?(.+?)\s+(?:released?|launch(?:ed)?|come\s+out|unveil(?:ed)?|debut(?:ed)?|publish(?:ed)?|issued?)$/i,
            /^when\s+(?:was|were|did|is|will)?\s*(?:the\s+)?(.+?)\s+(?:released?|launch(?:ed)?|come\s+out|unveil(?:ed)?|debut(?:ed)?|publish(?:ed)?|issued?|get\s+released)$/i,
            /^(?:when|where|who)\s+(?:was|were|did|is|are)?\s*(?:the\s+)?(.+?)\s+(?:born|die[d]?|founded|formed|created|discovered|invented|located|headquartered)$/i,
            /^(?:who|what)\s+(?:is|was|are|were)\s+(?:the\s+)?(.+?)\s+(?:founder|creator|director|author|manufacturer|publisher)$/i,
            /^who\s+(?:founded|created|formed|built|developed|discovered|invented)\s+(.+)$/i,
            /^(?:what|which)\s+(?:company|organization|organisation|person|people)s?\s+(?:founded|created|formed|built|developed)\s+(.+)$/i,
            /^(.+?)\s+(?:was|were)\s+(?:founded|created|formed|built)\s+by\s+(?:whom|who)$/i,
            /^(?:what|which)\s+(?:is|was)\s+(?:the\s+)?(.+?)\s+(?:capital|population|official language|language|genre)$/i,
            /^(?:what|who)\s+(?:is|are|was|were)\s+(?:the\s+)?(.+?)$/i,
            /^(?:where|what country)\s+(?:is|was)\s+(.+?)(?:\s+located)?$/i,
            /^what\s+country\s+is\s+(.+?)(?:\s+located)?(?:\s+in)?$/i
        ];

        if (!subject) {
            for (const pattern of patterns) {
                const match = q.match(pattern);
                if (match && match[1]) {
                    subject = _cleanSubject(match[1]);
                    break;
                }
            }
        }

        if (!subject) subject = _cleanSubject(q);
        const roleIntent = role && _intentById({
            founder: 'founder', creator: 'creator', director: 'director',
            author: 'author', manufacturer: 'manufacturer', publisher: 'publisher'
        }[role]);
        return { original, subject, intent: roleIntent || _detectIntent(q) };
    }

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

    function _clearSearchCaches() {
        _cache.clear();
        try {
            const keys = [];
            for (let i = 0; i < localStorage.length; i++) {
                const key = localStorage.key(i);
                if (key && key.startsWith(LS_PREFIX)) keys.push(key);
            }
            for (const key of keys) localStorage.removeItem(key);
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
                const entries = [];
                for (const key of keys) {
                    try {
                        const value = JSON.parse(localStorage.getItem(key) || '{}');
                        entries.push({ key, t: value.t || 0 });
                    } catch(_) {
                        localStorage.removeItem(key);
                    }
                }
                entries.sort((a, b) => a.t - b.t);
                for (let i = 0; i < entries.length - 30; i++) localStorage.removeItem(entries[i].key);
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

    const INDEX_META_DOMAINS = [
        'wikipedia.org', 'wikidata.org', 'wikimedia.org', 'mediawiki.org',
        'web.archive.org', 'archive.org', 'schema.org', 'w3.org'
    ];

    function _normaliseDomain(domain) {
        return (domain || '').toLowerCase().replace(/^www\./, '').replace(/\.$/, '');
    }

    function isSafeDomain(domain) {
        if (!domain) return true;
        const d = _normaliseDomain(domain);
        if (DANGEROUS_DOMAINS.has(d)) return false;
        if (d === 'localhost' || d.endsWith('.local') || d.endsWith('.internal')) return false;
        if (/^(?:127(?:\.\d{1,3}){3}|10(?:\.\d{1,3}){3}|192\.168(?:\.\d{1,3}){2}|172\.(?:1[6-9]|2\d|3[0-1])(?:\.\d{1,3}){2}|169\.254(?:\.\d{1,3}){2}|0\.0\.0\.0)$/i.test(d)) return false;
        if (d.includes(':')) return false;
        if (d.split('.').length > 4) return false;
        return true;
    }

    function isSafeResult(r) {
        if (!r || !r.url) return false;
        try {
            const parsed = new URL(r.url);
            if (!['http:', 'https:'].includes(parsed.protocol)) return false;
            if (parsed.username || parsed.password) return false;
            if (!isSafeDomain(parsed.hostname)) return false;
        } catch(_) { return false; }
        if (r.domain && !isSafeDomain(r.domain)) return false;
        const text = ((r.title || '') + ' ' + (r.description || '')).toLowerCase();
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
            for (const k of Array.from(u.searchParams.keys())) {
                if (track.has(k.toLowerCase())) u.searchParams.delete(k);
            }
            u.hash = '';
            return u.toString();
        } catch(_) { return urlStr; }
    }

    function _domainFromUrl(urlStr) {
        try { return _normaliseDomain(new URL(urlStr).hostname); }
        catch(_) { return ''; }
    }

    function _urlKey(urlStr) {
        try {
            const u = new URL(cleanTracking(urlStr));
            const host = _normaliseDomain(u.hostname);
            const path = u.pathname.replace(/\/$/, '') || '/';
            return host + path + (u.search || '');
        } catch(_) { return String(urlStr || '').toLowerCase(); }
    }

    function _isReferenceUrl(urlStr) {
        const domain = _domainFromUrl(urlStr);
        if (!domain || INDEX_META_DOMAINS.some(item => domain === item || domain.endsWith('.' + item))) return false;
        try {
            const path = new URL(urlStr).pathname.toLowerCase();
            return !/\/(login|signin|account|special|search|cart|checkout)(\/|$)/.test(path);
        } catch(_) { return false; }
    }

    function _wikiDomain() {
        const lang = (window.gatewayLang || 'en').split('-')[0];
        const map = { en:'en',es:'es',fr:'fr',de:'de',it:'it',pt:'pt',ru:'ru',
            ja:'ja','zh':'zh',ko:'ko',ar:'ar',hi:'hi',bn:'bn',
            tr:'tr',nl:'nl',pl:'pl',sv:'sv',da:'da',fi:'fi',
            no:'no',cs:'cs',ro:'ro',hu:'hu',el:'el' };
        return (map[lang] || 'en') + '.wikipedia.org';
    }

    function _wikiLanguage() {
        const lang = window.gatewayLang || 'en';
        if (/^zh/i.test(lang)) return lang.replace('-', '-');
        return lang.split('-')[0];
    }

    async function _fetch(url, timeoutMs) {
        const ac = new AbortController();
        const timer = setTimeout(() => ac.abort(), timeoutMs || 4500);
        try {
            const r = await fetch(url, { signal: ac.signal });
            if (!r.ok) throw new Error(String(r.status));
            return await r.json();
        } finally {
            clearTimeout(timer);
        }
    }

    function _mediaWikiUrl(domain, params) {
        const url = new URL('https://' + domain + '/w/api.php');
        url.searchParams.set('format', 'json');
        url.searchParams.set('origin', '*');
        for (const key in params) {
            if (params[key] !== undefined && params[key] !== null) url.searchParams.set(key, String(params[key]));
        }
        return url.toString();
    }

    function _once(key, producer) {
        if (_inflight.has(key)) return _inflight.get(key);
        const promise = Promise.resolve().then(producer).finally(() => _inflight.delete(key));
        _inflight.set(key, promise);
        return promise;
    }

    let _indexData = null;
    let _indexReady = null;
    let _preparedIndex = null;
    let _preparedSource = null;

    function _openDB() {
        return new Promise(function(resolve, reject) {
            const req = indexedDB.open('GatewayIndex', 1);
            req.onupgradeneeded = function() {
                if (!req.result.objectStoreNames.contains('index')) req.result.createObjectStore('index', { keyPath: 'id' });
            };
            req.onsuccess = function() { resolve(req.result); };
            req.onerror = function() { reject(req.error); };
        });
    }

    async function _readDBRecord(id) {
        try {
            const db = await _openDB();
            return await new Promise(function(resolve) {
                const tx = db.transaction('index', 'readonly');
                const get = tx.objectStore('index').get(id);
                get.onsuccess = function() { resolve(get.result || null); };
                get.onerror = function() { resolve(null); };
            });
        } catch(_) { return null; }
    }

    async function _writeDBRecord(record) {
        try {
            const db = await _openDB();
            await new Promise(function(resolve) {
                const tx = db.transaction('index', 'readwrite');
                tx.objectStore('index').put(record);
                tx.oncomplete = function() { resolve(); };
                tx.onerror = function() { resolve(); };
                tx.onabort = function() { resolve(); };
            });
        } catch(_) {}
    }

    async function _loadBundledIndex() {
        const savedData = await _readDBRecord('data');
        const savedMetaRecord = await _readDBRecord('meta');
        const savedMeta = savedMetaRecord && savedMetaRecord.meta;
        let remoteMeta = null;

        try {
            const metaResponse = await fetch('index-meta.json', { cache: 'no-cache' });
            if (metaResponse.ok) remoteMeta = await metaResponse.json();
        } catch(_) {}

        if (savedData && savedMeta && savedMeta.source === 'upload') {
            _indexRevision = String(savedMeta.version || 'upload-legacy');
            return Array.isArray(savedData.entries) ? savedData.entries : [];
        }
        if (savedData && remoteMeta && savedMeta && savedMeta.version === remoteMeta.version) {
            _indexRevision = String(remoteMeta.version || 'bundled');
            return Array.isArray(savedData.entries) ? savedData.entries : [];
        }
        if (savedData && !remoteMeta && !savedMeta) {
            _indexRevision = 'legacy';
            return Array.isArray(savedData.entries) ? savedData.entries : [];
        }

        try {
            const response = await fetch('index.json', { cache: 'no-cache' });
            if (!response.ok) throw new Error(String(response.status));
            const data = await response.json();
            if (Array.isArray(data) && data.length) {
                const version = remoteMeta && remoteMeta.version || 'legacy';
                await _writeDBRecord({ id: 'data', entries: data });
                await _writeDBRecord({ id: 'meta', meta: { source: 'bundled', version } });
                _indexRevision = String(version);
                return data;
            }
        } catch(_) {}

        if (savedData && savedMeta && savedMeta.version) _indexRevision = String(savedMeta.version);
        return savedData && Array.isArray(savedData.entries) ? savedData.entries : [];
    }

    _indexReady = _loadBundledIndex().then(data => {
        // An upload may complete while the bundled index is still loading.
        // Never let the older asynchronous load replace the user's index.
        if (!_indexOverridden) {
            _indexData = data;
            return data;
        }
        return _indexData;
    });

    window.gatewaySetIndex = async function(data) {
        if (!Array.isArray(data) || !data.length) throw new Error('Invalid index');
        const safeData = [];
        for (const entry of data) {
            if (!entry || typeof entry !== 'object' || typeof entry.t !== 'string' || typeof entry.u !== 'string') continue;
            const title = entry.t.trim().slice(0, 240);
            const description = typeof entry.d === 'string' ? entry.d.trim().slice(0, 1200) : '';
            let url;
            try { url = cleanTracking(entry.u.trim()); } catch (_) { continue; }
            if (!title || !url || !isSafeResult({ url, domain: _domainFromUrl(url), title, description })) continue;
            safeData.push({ t: title, u: url, d: description, s: _domainFromUrl(url) });
            if (safeData.length >= 250000) break;
        }
        if (!safeData.length) throw new Error('Index contains no valid public web entries');
        const version = 'upload-' + Date.now().toString(36) + '-' + Math.random().toString(36).slice(2, 8);
        _indexOverridden = true;
        _indexRevision = version;
        _indexData = safeData;
        _preparedIndex = null;
        _preparedSource = null;
        _indexReady = Promise.resolve(safeData);
        _clearSearchCaches();
        await Promise.all([
            _writeDBRecord({ id: 'data', entries: safeData }),
            _writeDBRecord({ id: 'meta', meta: { source: 'upload', version } })
        ]);
    };

    window.gatewayIndexSize = async function() {
        await _indexReady;
        return _indexData && _indexData.length || 0;
    };

    function _prepareIndex(idx) {
        if (_preparedSource === idx && _preparedIndex) return _preparedIndex;
        _preparedIndex = idx.map(entry => ({
            entry,
            title: _normalize(entry.t),
            description: _normalize(entry.d),
            titleSpace: ' ' + _normalize(entry.t) + ' ',
            descriptionSpace: ' ' + _normalize(entry.d) + ' '
        }));
        _preparedSource = idx;
        return _preparedIndex;
    }

    function _mapIndexEntry(prepared, score) {
        const e = prepared.entry;
        const url = cleanTracking(e.u || '');
        const domain = _normaliseDomain(e.s || _domainFromUrl(url));
        return {
            title: e.t || domain,
            url,
            description: e.d || '',
            fullSnippet: e.d || '',
            extract: e.d || '',
            thumbnail: null,
            source: 'gateway-index',
            sourceLabel: 'Gateway Index',
            resultType: domain.endsWith('wikipedia.org') ? 'wiki' : 'web',
            score,
            domain,
            suggestion: null
        };
    }

    async function searchIndex(term) {
        if (_indexReady) await _indexReady;
        const idx = _indexData;
        if (!Array.isArray(idx) || !idx.length) return [];

        const q = _normalize(term);
        const terms = _unique(q.split(/\s+/).filter(t => t.length > 1 && !STOPWORDS.has(t)));
        if (!terms.length) return [];

        const prepared = _prepareIndex(idx);
        const matched = [];

        for (let i = 0; i < prepared.length; i++) {
            const item = prepared[i];
            let score = 10;
            let matchedTerms = 0;
            let titleHits = 0;

            for (const t of terms) {
                let termScore = 0;
                if (item.titleSpace.includes(' ' + t + ' ')) { termScore += 55; titleHits++; }
                else if (item.titleSpace.includes(t)) { termScore += 32; titleHits++; }
                if (item.descriptionSpace.includes(' ' + t + ' ')) termScore += 24;
                else if (item.descriptionSpace.includes(t)) termScore += 9;
                if (termScore > 0) {
                    matchedTerms++;
                    score += termScore;
                }
            }

            if (!matchedTerms) continue;
            if (terms.length > 1 && matchedTerms < Math.ceil(terms.length * 0.6)) continue;

            const coverage = matchedTerms / terms.length;
            score += Math.round(coverage * 150);
            if (titleHits === terms.length) score += 90;
            if (item.title === q) score += 700;
            else if (item.title.startsWith(q)) score += 380;
            else if (item.titleSpace.includes(' ' + q + ' ')) score += 260;
            if (item.descriptionSpace.includes(' ' + q + ' ')) score += 45;
            matched.push(_mapIndexEntry(item, score));
        }

        matched.sort((a, b) => b.score - a.score || a.title.localeCompare(b.title));
        return matched.slice(0, MAX_INDEX_RESULTS);
    }

    function _pageList(data) {
        if (!data || !data.query || !data.query.pages) return [];
        return Object.values(data.query.pages).filter(page => page && page.pageid && page.pageid !== -1);
    }

    function _pickWikiPage(pages, subject) {
        if (!pages.length) return null;
        const wanted = _normalize(subject);
        if (!wanted) return null;
        const exact = pages.find(page => _normalize(page.title) === wanted);
        if (exact) return exact;

        const wantedWords = _unique(wanted.split(/\s+/).filter(word => word.length > 1 && !STOPWORDS.has(word)));
        if (!wantedWords.length) return null;
        let best = null;
        let bestScore = 0;
        for (const page of pages) {
            const title = _normalize(page.title);
            if (!title) continue;
            const titleWords = _unique(title.split(/\s+/).filter(Boolean));
            const overlap = wantedWords.filter(word => titleWords.includes(word)).length / wantedWords.length;
            let score = overlap;
            // A complete word-sequence match is strong even when the article
            // has a disambiguator or a longer title. Do not use raw substring
            // containment: "cat" must not make "education" a confident match.
            if (titleWords.length >= wantedWords.length
                && wantedWords.every(word => titleWords.includes(word))
                && (title.startsWith(wanted + ' ') || title.endsWith(' ' + wanted)
                    || title.includes(' ' + wanted + ' '))) {
                score = 1;
            }
            if (score > bestScore) {
                best = page;
                bestScore = score;
            }
        }
        // Do not award a best-page boost to an unrelated first result. A
        // partial match is useful for ordinary searches, but it must cover at
        // least half of the meaningful subject words.
        return bestScore >= 0.5 ? best : null;
    }

    async function _searchWikipedia(subject, limit) {
        const safeSubject = (subject || '').trim();
        if (!safeSubject) return [];
        const domain = _wikiDomain();
        const language = _wikiLanguage();
        const requestedCount = Math.min(Math.max(limit || 12, 1), 12);
        // Cache the full 12-page response so Quick Wiki and the result list
        // can share one request even when they ask for different limits.
        const count = 12;
        const key = 'wikisearch:' + language + ':' + _normalize(safeSubject) + ':' + count;
        const cached = _cached(key);
        if (Array.isArray(cached)) return cached.slice(0, requestedCount);

        return _once(key, async () => {
            let titles = [];
            try {
                const openData = await _fetch(_mediaWikiUrl(domain, {
                    action: 'opensearch', search: safeSubject, limit: 12, namespace: 0
                }), 3500);
                titles = Array.isArray(openData[1]) ? openData[1].filter(Boolean) : [];
            } catch(_) {}

            if (!titles.length) {
                try {
                    const searchData = await _fetch(_mediaWikiUrl(domain, {
                        action: 'query', list: 'search', srsearch: safeSubject, srlimit: count, srnamespace: 0
                    }), 3500);
                    titles = ((searchData.query && searchData.query.search) || []).map(item => item.title);
                } catch(_) { return []; }
            }

            const wanted = _normalize(safeSubject);
            titles.sort((a, b) => {
                const an = _normalize(a) === wanted ? 1 : 0;
                const bn = _normalize(b) === wanted ? 1 : 0;
                return bn - an;
            });
            titles = titles.slice(0, count);
            if (!titles.length) return [];

            let detailData = null;
            try {
                detailData = await _fetch(_mediaWikiUrl(domain, {
                    action: 'query', titles: titles.join('|'), redirects: 1,
                    prop: 'extracts|pageimages|pageprops|info',
                    exintro: 1, explaintext: 1, exlimit: 'max',
                    inprop: 'url', piprop: 'thumbnail', pithumbsize: 160
                }), 5000);
            } catch(_) { return []; }

            const order = new Map(titles.map((title, index) => [_normalize(title), index]));
            const pages = _pageList(detailData).map(page => {
                const title = page.title || '';
                const url = cleanTracking(page.fullurl || page.canonicalurl || ('https://' + domain + '/wiki/' + encodeURIComponent(title.replace(/ /g, '_'))));
                return {
                    pageid: page.pageid,
                    title,
                    url,
                    extract: page.extract || '',
                    description: _firstSentences(page.extract || '', 360),
                    qid: page.pageprops && page.pageprops.wikibase_item || null,
                    thumbnail: page.thumbnail && page.thumbnail.source || null,
                    extlinks: [],
                    order: order.has(_normalize(title)) ? order.get(_normalize(title)) : 999
                };
            }).sort((a, b) => a.order - b.order);

            const bestPage = _pickWikiPage(pages, safeSubject);
            if (bestPage) {
                try {
                    const linksData = await _fetch(_mediaWikiUrl(domain, {
                        action: 'query', titles: bestPage.title, redirects: 1,
                        prop: 'extlinks', ellimit: 24
                    }), 3500);
                    const linkPage = _pageList(linksData)[0];
                    bestPage.extlinks = ((linkPage && linkPage.extlinks) || [])
                        .map(link => link['*'] || link).filter(Boolean).map(cleanTracking);
                } catch(_) {}
            }

            _store(key, pages);
            return pages.slice(0, requestedCount);
        });
    }

    async function _wikidataClaims(qid, property) {
        if (!qid || !property) return [];
        const key = 'claim:' + qid + ':' + property;
        const cached = _cached(key);
        if (cached) return cached;
        return _once(key, async () => {
            try {
                const data = await _fetch('https://www.wikidata.org/w/api.php?action=wbgetclaims&format=json&origin=*'
                    + '&entity=' + encodeURIComponent(qid) + '&property=' + encodeURIComponent(property), 3500);
                const claims = data && data.claims && data.claims[property] || [];
                _store(key, claims);
                return claims;
            } catch(_) { return []; }
        });
    }

    function _claimValue(claim) {
        const value = claim && claim.mainsnak && claim.mainsnak.datavalue && claim.mainsnak.datavalue.value;
        if (!value) return null;
        if (typeof value === 'string') return value;
        if (value.id) return value.id;
        if (value.time) return value.time;
        if (value.amount) return value.amount;
        return null;
    }

    function _rankWeight(claim) {
        if (!claim || claim.rank === 'deprecated') return -1;
        return claim.rank === 'preferred' ? 2 : claim.rank === 'normal' ? 1 : 0;
    }

    function _formatWikidataTime(raw) {
        if (!raw) return '';
        const match = /([+-])(\d{1,})-(\d{2})(?:-(\d{2}))?/.exec(raw);
        if (!match) return String(raw);
        const sign = match[1] === '-' ? -1 : 1;
        const year = sign * Number(match[2]);
        const month = Number(match[3]);
        const day = Number(match[4] || 0);
        const precision = day ? 'day' : month ? 'month' : 'year';
        try {
            if (precision === 'year') return String(year);
            const date = new Date(Date.UTC(2000, month - 1, day || 1));
            date.setUTCFullYear(year);
            return new Intl.DateTimeFormat(window.gatewayLang || 'en', {
                year: 'numeric', month: precision === 'month' ? 'long' : 'long', day: precision === 'day' ? 'numeric' : undefined,
                timeZone: 'UTC'
            }).format(date);
        } catch(_) { return raw.replace(/^[+-]/, '').split('T')[0]; }
    }

    async function _labelsForClaims(claims) {
        const ids = _unique(claims.map(_claimValue).filter(value => /^Q\d+$/.test(String(value)))).slice(0, 8);
        if (!ids.length) return [];
        const key = 'labels:' + _wikiLanguage() + ':' + ids.join('|');
        const cached = _cached(key);
        if (cached) return cached;
        return _once(key, async () => {
            try {
                const url = new URL('https://www.wikidata.org/w/api.php');
                url.searchParams.set('action', 'wbgetentities');
                url.searchParams.set('ids', ids.join('|'));
                url.searchParams.set('props', 'labels');
                url.searchParams.set('languages', _wikiLanguage() + '|en');
                url.searchParams.set('languagefallback', '1');
                url.searchParams.set('format', 'json');
                url.searchParams.set('origin', '*');
                const data = await _fetch(url.toString(), 3500);
                const entities = data.entities || {};
                const labels = ids.map(id => entities[id] && entities[id].labels
                    && ((entities[id].labels[_wikiLanguage()] || {}).value || (entities[id].labels.en || {}).value) || id);
                _store(key, labels);
                return labels;
            } catch(_) { return []; }
        });
    }

    function _extractDateAnswer(text, subject, intent) {
        if (!text || !intent || !DATE_INTENT_IDS.has(intent.id)) return null;
        const sentences = text.replace(/\s+/g, ' ').split(/(?<=[.!?])\s+/);
        const subjectPart = _normalize(subject).split(' ').filter(t => t.length > 2).slice(0, 2).join(' ');
        let best = null;
        for (const sentence of sentences) {
            if (!intent.keywords.test(sentence)) continue;
            if (subjectPart && _normalize(sentence).indexOf(subjectPart) < 0) continue;
            const keyword = intent.keywords.exec(sentence);
            const matches = [];
            DATE_RE.lastIndex = 0;
            let match;
            while ((match = DATE_RE.exec(sentence))) {
                matches.push({ value: match[0], index: match.index });
                if (matches.length >= 8) break;
            }
            if (!matches.length) continue;
            let chosen = matches[0];
            if (keyword) {
                const after = matches.filter(item => item.index >= keyword.index).sort((a, b) => a.index - b.index);
                if (after.length) chosen = after[0];
            }
            best = { answer: chosen.value, note: 'Matched directly in the Wikipedia article text.' };
            break;
        }
        return best;
    }

    async function _officialUrls(page) {
        if (!page || !page.qid) return [];
        const claims = await _wikidataClaims(page.qid, 'P856');
        return claims
            .filter(claim => _rankWeight(claim) > 0)
            .sort((a, b) => _rankWeight(b) - _rankWeight(a))
            .map(_claimValue)
            .filter(value => typeof value === 'string' && /^https?:\/\//i.test(value))
            .map(cleanTracking)
            .filter(url => isSafeResult({ url, domain: _domainFromUrl(url), title: '', description: '' }))
            .filter((url, index, values) => values.indexOf(url) === index)
            .slice(0, 2);
    }

    function _sourceLinks(page) {
        const links = [];
        for (const url of (page.extlinks || []).slice(0, 12)) {
            if (!_isReferenceUrl(url)) continue;
            if (!isSafeResult({ url, domain: _domainFromUrl(url), title: page.title, description: page.description })) continue;
            links.push({ url, title: page.title, domain: _domainFromUrl(url) });
            if (links.length >= 6) break;
        }
        return links;
    }

    function _dedupeSourceLinks(links, limit) {
        const seenUrls = new Set();
        const seenDomains = new Set();
        const result = [];
        for (const link of links || []) {
            if (!link || !link.url) continue;
            const key = _urlKey(link.url);
            const domain = _normaliseDomain(link.domain || _domainFromUrl(link.url));
            if (!key || !domain || seenUrls.has(key) || seenDomains.has(domain)) continue;
            seenUrls.add(key);
            seenDomains.add(domain);
            result.push({ url: link.url, title: link.title || domain, domain });
            if (result.length >= limit) break;
        }
        return result;
    }

    async function quickWiki(query) {
        const parsed = parseQuickWikiQuery(query);
        if (!parsed.subject || parsed.subject.length < 2) return null;
        const key = 'quickwiki:' + _wikiLanguage() + ':' + _normalize(parsed.original);
        const cached = _cached(key);
        if (cached) return cached;

        return _once(key, async () => {
            const pages = await _searchWikipedia(parsed.subject, 8);
            const page = _pickWikiPage(pages, parsed.subject);
            if (!page || !page.extract) return null;

            let fact = null;
            if (parsed.intent && page.qid) {
                for (const property of parsed.intent.properties) {
                    const claims = await _wikidataClaims(page.qid, property);
                    const values = claims.filter(claim => _rankWeight(claim) >= 0 && _claimValue(claim) !== null);
                    if (!values.length) continue;
                    values.sort((a, b) => _rankWeight(b) - _rankWeight(a));

                    if (/^P(569|570|571|575|577|1191|1619)$/.test(property)) {
                        const times = values.map(claim => ({ claim, raw: _claimValue(claim), time: _claimValue(claim) }))
                            .filter(item => typeof item.time === 'string' && /^[+-]\d{4,}/.test(item.time))
                            .sort((a, b) => _rankWeight(b.claim) - _rankWeight(a.claim) || a.time.localeCompare(b.time));
                        if (times.length) {
                            const answer = _formatWikidataTime(times[0].raw);
                            fact = {
                                answer,
                                label: parsed.intent.label,
                                note: times.length > 1 ? 'Earliest of ' + times.length + ' structured dates listed.' : 'Matched to a structured Wikidata date.',
                                property
                            };
                            break;
                        }
                    }

                    const labels = await _labelsForClaims(values);
                    const answerValues = labels.length ? labels : values.map(_claimValue).filter(Boolean).map(String);
                    if (answerValues.length) {
                        fact = {
                            answer: _unique(answerValues).slice(0, 4).join(', '),
                            label: parsed.intent.label,
                            note: 'Matched to structured Wikidata data.',
                            property
                        };
                        break;
                    }
                }
            }

            if (!fact) fact = _extractDateAnswer(page.extract, page.title, parsed.intent);
            if (!fact) {
                fact = {
                    answer: _firstSentences(page.extract, 420),
                    label: parsed.intent ? parsed.intent.label + ' summary' : 'Article summary',
                    note: parsed.intent
                        ? 'The structured fact was unavailable, so the sourced article summary is shown.'
                        : 'A direct extract from Wikipedia; no generative AI was used.',
                    property: null
                };
            }

            const officialUrls = await _officialUrls(page);
            const officialLinks = officialUrls.map(url => ({ url, title: _domainFromUrl(url), domain: _domainFromUrl(url) }));
            const sourceLinks = _dedupeSourceLinks(officialLinks.concat(_sourceLinks(page)), 8);
            const result = {
                title: page.title,
                url: page.url,
                answer: fact.answer,
                label: fact.label,
                note: fact.note,
                property: fact.property,
                description: _firstSentences(page.extract, 520),
                extract: page.extract,
                source: 'wikipedia',
                sourceLabel: 'Wikipedia',
                sourceLinks,
                isSummary: !fact.property
            };
            _store(key, result);
            return result;
        });
    }

    function _dedupeAndRank(results) {
        const byUrl = new Map();
        for (const result of results) {
            if (!result || !result.url) continue;
            const key = _urlKey(result.url);
            const current = byUrl.get(key);
            const candidateHttps = /^https:/i.test(String(result.url || ''));
            const currentHttp = /^http:/i.test(String(current && current.url || ''));
            if (!current || (result.score || 0) > (current.score || 0) || (candidateHttps && currentHttp)) {
                byUrl.set(key, result);
            }
        }
        return Array.from(byUrl.values())
            .filter(isSafeResult)
            .sort((a, b) => (b.score || 0) - (a.score || 0) || String(a.title).localeCompare(String(b.title)))
            .slice(0, MAX_RESULTS);
    }

    async function gatewayCrawl(term) {
        if (!term) return [];
        const key = 'full:' + _indexRevision + ':' + _wikiLanguage() + ':' + _normalize(term);
        const cached = _cached(key);
        if (cached) return _dedupeAndRank(cached);

        const parsed = parseQuickWikiQuery(term);
        const searchTerms = _unique([term, parsed.subject]);
        try {
            const [indexedGroups, pages] = await Promise.all([
                Promise.all(searchTerms.map(searchIndex)),
                _searchWikipedia(parsed.subject, 12)
            ]);
            const results = indexedGroups.flat();

            const bestPage = _pickWikiPage(pages, parsed.subject);
            for (let i = 0; i < pages.length; i++) {
                const page = pages[i];
                if (!page.url || !page.extract) continue;
                const isBest = page === bestPage;
                results.push({
                    title: page.title,
                    url: page.url,
                    description: page.description || _firstSentences(page.extract, 320),
                    fullSnippet: page.extract,
                    extract: page.extract,
                    thumbnail: page.thumbnail,
                    source: 'wikipedia-live',
                    sourceLabel: 'Wikipedia',
                    resultType: 'wiki',
                    // Only a title-confirmed article receives the large
                    // Wikipedia boost. An API's first result is not proof of
                    // relevance when the search subject did not match it.
                    score: (isBest ? 2000 : 280) - i * 12,
                    domain: _domainFromUrl(page.url),
                    suggestion: null
                });
            }

            const officialLists = await Promise.all(
                pages.slice(0, 4).map(async page => ({
                    page,
                    urls: await _officialUrls(page).catch(() => [])
                }))
            );
            officialLists.forEach(({ page, urls }, pageIndex) => {
                urls.forEach((url, urlIndex) => {
                    const domain = _domainFromUrl(url);
                    if (!domain) return;
                    results.push({
                        title: page.title + ' - official website',
                        url,
                        description: 'Official website listed for ' + page.title + '. ' + (page.description || ''),
                        fullSnippet: page.extract,
                        extract: page.extract,
                        thumbnail: page.thumbnail,
                        source: 'official-web',
                        sourceLabel: domain,
                        resultType: 'web',
                        score: (page === bestPage ? 1800 : 360) - pageIndex * 18 - urlIndex * 10,
                        domain,
                        suggestion: null
                    });
                });
            });

            const articleBest = bestPage;
            for (const page of pages.slice(0, 4)) {
                for (const url of page.extlinks || []) {
                    if (!_isReferenceUrl(url)) continue;
                    const domain = _domainFromUrl(url);
                    const webResult = {
                        title: page.title + ' — ' + domain,
                        url,
                        description: 'Referenced by the Wikipedia article “' + page.title + '”. ' + (page.description || ''),
                        fullSnippet: page.extract,
                        extract: page.extract,
                        thumbnail: page.thumbnail,
                        source: 'open-web',
                        sourceLabel: domain,
                        resultType: 'web',
                        score: (page === articleBest ? 930 : 260) - results.length * 0.01,
                        domain,
                        suggestion: null
                    };
                    if (!isSafeResult(webResult)) continue;
                    results.push(webResult);
                    if (results.filter(item => item.source === 'open-web').length >= MAX_WEB_LINKS) break;
                }
                if (results.filter(item => item.source === 'open-web').length >= MAX_WEB_LINKS) break;
            }

            const ranked = _dedupeAndRank(results);
            if (ranked.length) _store(key, ranked);
            return ranked;
        } catch(_) {
            return _dedupeAndRank(await searchIndex(term));
        }
    }

    async function getSuggestions(term) {
        if (!term || term.length < 2) return [];
        const key = 'sug:' + _indexRevision + ':' + _wikiLanguage() + ':' + _normalize(term);
        const cached = _cached(key);
        if (cached) return cached;

        const local = [];
        try {
            await _indexReady;
            const wanted = _normalize(term);
            if (_indexData) {
                for (const item of _prepareIndex(_indexData)) {
                    if (item.title.startsWith(wanted) || (wanted.length >= 3 && item.title.includes(wanted))) {
                        local.push(item.entry.t);
                        if (local.length >= 5) break;
                    }
                }
            }
        } catch(_) {}

        let remote = [];
        try {
            const data = await _fetch(_mediaWikiUrl(_wikiDomain(), {
                action: 'opensearch', search: term, limit: 7, namespace: 0
            }), 2200);
            remote = Array.isArray(data[1]) ? data[1] : [];
        } catch(_) {}

        const suggestions = _unique(local.concat(remote)).slice(0, 8);
        if (suggestions.length) _store(key, suggestions);
        return suggestions;
    }

    async function spellCheck(term) {
        if (!term) return null;
        const key = 'spell:' + _wikiLanguage() + ':' + _normalize(term);
        const cached = _cached(key);
        if (cached) return cached;

        try {
            const data = await _fetch(_mediaWikiUrl(_wikiDomain(), {
                action: 'query', list: 'search', srsearch: term, srlimit: 1
            }), 2500);
            const suggestion = data && data.query && data.query.searchinfo && data.query.searchinfo.suggestion || null;
            if (suggestion && suggestion.toLowerCase() !== term.toLowerCase()) {
                _store(key, suggestion);
                return suggestion;
            }
            return null;
        } catch(_) { return null; }
    }

    window.gatewayCrawl = gatewayCrawl;
    window.gatewayQuickWiki = quickWiki;
    window.gatewayParseQuickWiki = parseQuickWikiQuery;
    window.gatewayIndexSize = window.gatewayIndexSize;
    window.getSuggestions = getSuggestions;
    window.gatewaySpellCheck = spellCheck;
})();
