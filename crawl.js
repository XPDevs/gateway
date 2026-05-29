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

    const STOPWORDS = new Set([
        'what','is','the','how','do','i','who','where','can','you','tell','me','about',
        'a','an','of','in','to','for','on','with','at','by','this','that','are','was',
        'were','will','have','has','had','does','did','would','could','should','may',
        'might','shall','be','been','being','get','got','gets','getting','make','made',
        'makes','making','use','used','uses','using','know','known','knows','knowing',
        'want','wants','wanted','wanting','need','needs','needed','needing','like',
        'likes','liked','liking','find','finds','found','finding','give','gives','gave',
        'given','giving','take','takes','took','taking','see','sees','saw','seeing',
        'come','comes','came','coming','go','goes','went','gone','going','also','just',
        'now','then','than','more','most','some','any','all','each','every','own','same',
        'so','too','very','can','will','not','no','nor','but','or','if','as','up','down',
        'out','off','over','under','again','further','once','here','there','when','why'
    ]);

    function refineQuery(q) {
        return q.toLowerCase()
            .replace(/[?.,!;:()'"\u2018\u2019\u201c\u201d]/g, '')
            .split(/\s+/)
            .filter(w => w && w.length > 1 && !STOPWORDS.has(w))
            .join(' ');
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

    async function searchWikipedia(term) {
        const key = 'wiki:' + term;
        const cached = _cached(key);
        if (cached) return cached;

        const results = [];
        try {
            const wikiDomain = _wikiDomain();
            const kw = refineQuery(term) || term;
            const sr = await _fetch(`https://${wikiDomain}/w/api.php?action=query&format=json&origin=*&list=search&srsearch=${encodeURIComponent(kw)}&srlimit=10&srinfo=suggestion`, 3000);
            if (!sr.query?.search?.length) return results;

            const suggestion = sr.query?.searchinfo?.suggestion || null;
            const titles = sr.query.search.slice(0, 5).map(s => s.title);

            const dt = await _fetch(`https://${wikiDomain}/w/api.php?action=query&format=json&origin=*&prop=info|extracts|pageimages|extlinks&exintro&explaintext&exsentences=2&titles=${encodeURIComponent(titles.join('|'))}&inprop=url&piprop=thumbnail&pithumbsize=200&ellimit=5`, 3000);

            for (const id in dt.query.pages) {
                const p = dt.query.pages[id];
                if (p.missing) continue;
                const ext = p.extract || '';

                results.push({
                    title: p.title,
                    url: p.fullurl || `https://${wikiDomain}/wiki/${encodeURIComponent(p.title.replace(/ /g, '_'))}`,
                    description: ext ? ext.substring(0, 200) + (ext.length > 200 ? '...' : '') : 'Wikipedia entry',
                    fullSnippet: ext,
                    extract: ext,
                    thumbnail: p.thumbnail?.source || null,
                    source: 'wikipedia',
                    sourceLabel: 'Wikipedia',
                    resultType: 'wiki',
                    score: 10,
                    domain: wikiDomain,
                    suggestion
                });

                if (p.extlinks) {
                    const seenDomains = new Set();
                    for (const linkObj of p.extlinks.slice(0, 3)) {
                        const link = cleanTracking(linkObj['*']);
                        try {
                            const urlObj = new URL(link);
                            const domain = urlObj.hostname.replace(/^www\./, '');
                            if (domain.includes('wikipedia.org') || domain.includes('wikimedia') ||
                                domain.includes('doi.org') || domain.includes('creativecommons') ||
                                domain.includes('mediawiki') || seenDomains.has(domain) ||
                                !isSafeDomain(domain) ||
                                urlObj.pathname === '/' || urlObj.pathname === '') continue;
                            seenDomains.add(domain);
                            results.push({
                                title: domain,
                                url: link,
                                description: ext ? `Referenced by "${p.title}". ${ext.substring(0, 100)}` : `External link from Wikipedia article "${p.title}".`,
                                fullSnippet: ext || '',
                                extract: ext || '',
                                thumbnail: null,
                                source: 'web',
                                sourceLabel: domain,
                                resultType: 'web',
                                score: 8,
                                domain: domain,
                                refersTo: p.title,
                                suggestion: null
                            });
                        } catch(_) {}
                    }
                }
            }
        } catch (_) {}

        _store(key, results);
        return results;
    }

    async function searchDuckDuckGo(term) {
        const key = 'ddg:' + term;
        const cached = _cached(key);
        if (cached) return cached;

        const results = [];
        try {
            const data = await _fetch(`https://api.duckduckgo.com/?q=${encodeURIComponent(term)}&format=json&no_html=1&skip_disambig=1`, 3000);
            if (data.AbstractText) {
                results.push({
                    title: data.Heading || data.AbstractSource || 'Instant Answer',
                    url: data.AbstractURL || `https://duckduckgo.com/?q=${encodeURIComponent(term)}`,
                    description: data.AbstractText.substring(0, 200),
                    fullSnippet: data.AbstractText,
                    extract: data.AbstractText,
                    thumbnail: data.Image ? `https://api.duckduckgo.com${data.Image}` : null,
                    source: 'duckduckgo',
                    sourceLabel: 'DuckDuckGo',
                    resultType: 'web',
                    score: 15,
                    domain: 'duckduckgo.com',
                    isAnswer: true,
                    suggestion: null
                });
            }
            if (data.RelatedTopics) {
                for (const t of data.RelatedTopics.slice(0, 12)) {
                    if (t.Text) {
                        let url = t.FirstURL;
                        let domain = 'duckduckgo.com';
                        try { if (url) domain = new URL(url).hostname.replace(/^www\./, ''); } catch(_) {}
                        results.push({
                            title: t.Text.split(' - ')[0] || url,
                            url: url || `https://duckduckgo.com/?q=${encodeURIComponent(t.Text)}`,
                            description: t.Text.substring(0, 200),
                            fullSnippet: t.Text,
                            extract: t.Text,
                            thumbnail: t.Icon?.URL ? `https://api.duckduckgo.com${t.Icon.URL}` : null,
                            source: 'duckduckgo',
                            sourceLabel: domain,
                            resultType: 'web',
                            score: 14,
                            domain,
                            suggestion: null
                        });
                    }
                }
            }
        } catch (_) {}

        _store(key, results);
        return results;
    }

    async function searchCommons(term) {
        const key = 'commons:' + term;
        const cached = _cached(key);
        if (cached) return cached;

        const results = [];
        try {
            const kw = refineQuery(term) || term;
            const sr = await _fetch(`https://commons.wikimedia.org/w/api.php?action=query&format=json&origin=*&list=search&srsearch=${encodeURIComponent(kw)}&srnamespace=6&srlimit=10`, 3000);
            if (!sr.query?.search) return results;

            const titles = sr.query.search.map(s => s.title);
            const dt = await _fetch(`https://commons.wikimedia.org/w/api.php?action=query&format=json&origin=*&prop=imageinfo&iiprop=url|size|extmetadata|mime&titles=${encodeURIComponent(titles.join('|'))}`, 3000);

            for (const id in dt.query.pages) {
                const p = dt.query.pages[id];
                if (p.missing || !p.imageinfo) continue;
                const ii = p.imageinfo[0];
                const mime = (ii.mime || '').toLowerCase();
                if (!mime.startsWith('image/')) continue;
                results.push({
                    title: p.title.replace(/^File:/, ''),
                    url: ii.descriptionurl || `https://commons.wikimedia.org/wiki/${encodeURIComponent(p.title)}`,
                    thumbnail: ii.thumburl || ii.url,
                    description: ii.extmetadata?.ImageDescription?.value || 'Wikimedia Commons Image',
                    fullSnippet: ii.extmetadata?.ImageDescription?.value || '',
                    extract: ii.extmetadata?.ImageDescription?.value || '',
                    source: 'commons',
                    sourceLabel: 'Images',
                    resultType: 'image',
                    score: 5,
                    domain: 'commons.wikimedia.org',
                    suggestion: null
                });
            }
        } catch (_) {}

        _store(key, results);
        return results;
    }

    async function searchWeb(term) {
        const key = 'websearch:' + term;
        const cached = _cached(key);
        if (cached) return cached;

        const results = [];
        try {
            const ac = new AbortController();
            const timer = setTimeout(() => ac.abort(), 4500);
            const r = await fetch('https://html.duckduckgo.com/html/', {
                method: 'POST',
                headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                body: 'q=' + encodeURIComponent(term),
                signal: ac.signal
            });
            clearTimeout(timer);
            const html = await r.text();
            const div = document.createElement('div');
            div.innerHTML = html;

            const resultItems = div.querySelectorAll('.result');
            const seen = new Set();
            for (const item of resultItems) {
                const titleEl = item.querySelector('.result__title a');
                const snippetEl = item.querySelector('.result__snippet');
                if (!titleEl) continue;

                let url = titleEl.getAttribute('href');
                if (!url || seen.has(url)) continue;
                url = cleanTracking(url);
                seen.add(url);

                const domain = url ? new URL(url).hostname.replace(/^www\./, '') : '';
                if (!isSafeDomain(domain)) continue;

                const title = titleEl.textContent.trim();
                const snippet = snippetEl ? snippetEl.textContent.trim() : '';

                results.push({
                    title,
                    url,
                    description: snippet,
                    fullSnippet: snippet,
                    extract: snippet,
                    thumbnail: null,
                    source: 'web-search',
                    sourceLabel: domain || 'Web',
                    resultType: 'web',
                    score: 18,
                    domain,
                    suggestion: null
                });

                if (results.length >= 10) break;
            }
        } catch (_) {}

        _store(key, results);
        return results;
    }

    async function gatewayCrawl(term) {
        if (!term) return [];
        const key = 'full:' + term;
        const cached = _cached(key);
        if (cached) {
            const safe = cached.filter(isSafeResult);
            return safe.length ? safe : cached;
        }

        const sources = [
            searchWikipedia(term),
            searchDuckDuckGo(term),
            searchCommons(term),
            searchWeb(term)
        ];

        const all = await Promise.allSettled(sources);
        let merged = all.flatMap(p => p.status === 'fulfilled' ? p.value : []);

        merged = merged.filter(isSafeResult);

        const ql = term.toLowerCase();
        for (const r of merged) {
            const terms = ql.split(/\s+/).filter(Boolean);
            let s = r.score || 0;
            const text = (r.title + ' ' + (r.description || '')).toLowerCase();
            for (const t of terms) {
                try {
                    const re = new RegExp(t.replace(/[.*+?^${}()|[\]\\]/g, '\\$&'), 'g');
                    s += ((text.match(re) || []).length) * 2;
                } catch(_) {}
            }
            if (r.title.toLowerCase() === ql) s += 500;
            else if (r.title.toLowerCase().startsWith(ql)) s += 200;
            if (r.domain && r.domain.includes(ql)) s += 50;
            r.score = s;
        }

        merged.sort((a, b) => b.score - a.score);

        const seen = new Set();
        const deduped = [];
        for (const r of merged) {
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
