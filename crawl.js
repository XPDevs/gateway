/**
 * Gateway — Search Engine
 * XPDevs — https://xpdevs.github.io
 * Aggregates Wikipedia pages + real website links from references
 */
(function() {
    'use strict';

    const CACHE_TTL = 300000;
    const _cache = new Map();

    function _cached(k) {
        const e = _cache.get(k);
        if (e && Date.now() - e.t < CACHE_TTL) return e.d;
        _cache.delete(k);
        return null;
    }
    function _store(k, d) { _cache.set(k, { d, t: Date.now() }); }

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

    async function _fetch(url) {
        const r = await fetch(url);
        if (!r.ok) throw new Error(String(r.status));
        return r.json();
    }

    async function gatewayCrawl(term) {
        if (!term) return [];

        const key = 'full:' + term;
        const cached = _cached(key);
        if (cached) return cached;

        const results = [];
        try {
            const kw = refineQuery(term) || term;

            const sr = await _fetch(
                `https://en.wikipedia.org/w/api.php?action=query&format=json&origin=*&list=search&srsearch=${encodeURIComponent(kw)}&srlimit=30&srinfo=suggestion`
            );
            if (!sr.query?.search?.length) return results;

            const suggestion = sr.query?.searchinfo?.suggestion || null;
            const titles = sr.query.search.slice(0, 15).map(s => s.title);

            const dt = await _fetch(
                `https://en.wikipedia.org/w/api.php?action=query&format=json&origin=*&prop=info|extracts|pageimages|extlinks&exintro&explaintext&exsentences=3&titles=${encodeURIComponent(titles.join('|'))}&inprop=url&piprop=thumbnail&pithumbsize=200&ellimit=10`
            );

            for (const id in dt.query.pages) {
                const p = dt.query.pages[id];
                if (p.missing) continue;
                const ext = p.extract || '';

                results.push({
                    title: p.title,
                    url: p.fullurl,
                    description: ext ? ext.substring(0, 280) + (ext.length > 280 ? '...' : '') : 'Wikipedia entry',
                    fullSnippet: ext,
                    extract: ext,
                    thumbnail: p.thumbnail?.source || null,
                    source: 'wikipedia',
                    sourceLabel: 'Wikipedia',
                    resultType: 'wiki',
                    score: 10,
                    domain: 'en.wikipedia.org',
                    suggestion
                });

                if (p.extlinks) {
                    const seenDomains = new Set();
                    for (const linkObj of p.extlinks) {
                        const link = linkObj['*'];
                        try {
                            const urlObj = new URL(link);
                            const domain = urlObj.hostname.replace(/^www\./, '');
                            if (domain.includes('wikipedia.org') || domain.includes('wikimedia') ||
                                domain.includes('doi.org') || domain.includes('creativecommons') ||
                                domain.includes('mediawiki') || seenDomains.has(domain) ||
                                urlObj.pathname === '/' || urlObj.pathname === '') continue;
                            seenDomains.add(domain);
                            results.push({
                                title: domain,
                                url: link,
                                description: ext
                                    ? `Referenced by "${p.title}". ${ext.substring(0, 160)}`
                                    : `External reference from Wikipedia article "${p.title}".`,
                                fullSnippet: ext || '',
                                extract: ext || '',
                                thumbnail: null,
                                source: 'web',
                                sourceLabel: domain,
                                resultType: 'web',
                                score: 8,
                                domain,
                                refersTo: p.title,
                                suggestion: null
                            });
                        } catch(_) {}
                        if (seenDomains.size >= 5) break;
                    }
                }
            }

            const ql = term.toLowerCase();
            for (const r of results) {
                const terms = ql.split(/\s+/).filter(Boolean);
                let s = r.score;
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

            results.sort((a, b) => b.score - a.score);

            const seen = new Set();
            const deduped = [];
            for (const r of results) {
                const key = r.url;
                if (seen.has(key)) continue;
                seen.add(key);
                deduped.push(r);
            }

            _store(key, deduped);
            return deduped;

        } catch (_) {
            return [];
        }
    }

    async function getSuggestions(term) {
        if (!term || term.length < 2) return [];
        const key = 'sug:' + term;
        const cached = _cached(key);
        if (cached) return cached;

        try {
            const data = await _fetch(
                `https://en.wikipedia.org/w/api.php?action=opensearch&format=json&origin=*&search=${encodeURIComponent(term)}&limit=8&namespace=0`
            );
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
            const data = await _fetch(
                `https://en.wikipedia.org/w/api.php?action=query&format=json&origin=*&list=search&srsearch=${encodeURIComponent(term)}&srlimit=1`
            );
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
