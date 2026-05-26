/**
 * Gateway — Multi-Source Search Engine
 * XPDevs — https://xpdevs.github.io
 * Aggregates from Wikipedia, DuckDuckGo, OpenLibrary, Wikimedia Commons, and Web search
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

    function _wikiDomain() {
        const lang = (window.gatewayLang || 'en').split('-')[0];
        const map = {
            en:'en',es:'es',fr:'fr',de:'de',it:'it',pt:'pt',ru:'ru',
            ja:'ja','zh':'zh',ko:'ko',ar:'ar',hi:'hi',bn:'bn',
            tr:'tr',nl:'nl',pl:'pl',sv:'sv',da:'da',fi:'fi',
            no:'no',cs:'cs',ro:'ro',hu:'hu',el:'el'
        };
        const sub = map[lang] || 'en';
        return sub + '.wikipedia.org';
    }

    async function _fetch(url) {
        const r = await fetch(url);
        if (!r.ok) throw new Error(String(r.status));
        return r.json();
    }

    async function searchWikipedia(term) {
        const key = 'wiki:' + term;
        const cached = _cached(key);
        if (cached) return cached;

        const results = [];
        try {
            const wikiDomain = _wikiDomain();
            const kw = refineQuery(term) || term;
            const sr = await _fetch(`https://${wikiDomain}/w/api.php?action=query&format=json&origin=*&list=search&srsearch=${encodeURIComponent(kw)}&srlimit=30&srinfo=suggestion`);
            if (!sr.query?.search?.length) return results;

            const suggestion = sr.query?.searchinfo?.suggestion || null;
            const titles = sr.query.search.slice(0, 15).map(s => s.title);

            const dt = await _fetch(`https://${wikiDomain}/w/api.php?action=query&format=json&origin=*&prop=info|extracts|pageimages|extlinks&exintro&explaintext&exsentences=3&titles=${encodeURIComponent(titles.join('|'))}&inprop=url&piprop=thumbnail&pithumbsize=200&ellimit=10`);

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
                    domain: wikiDomain,
                    suggestion
                });

                if (p.extlinks) {
                    const seenDomains = new Set();
                    for (const linkObj of p.extlinks.slice(0, 5)) {
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
                                description: ext ? `Referenced by "${p.title}". ${ext.substring(0, 160)}` : `External link from Wikipedia article "${p.title}".`,
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
            const data = await _fetch(`https://api.duckduckgo.com/?q=${encodeURIComponent(term)}&format=json&no_html=1&skip_disambig=1`);
            if (data.AbstractText) {
                results.push({
                    title: data.Heading || data.AbstractSource || 'Instant Answer',
                    url: data.AbstractURL || `https://duckduckgo.com/?q=${encodeURIComponent(term)}`,
                    description: data.AbstractText.substring(0, 280),
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
                for (const t of data.RelatedTopics.slice(0, 20)) {
                    if (t.Text) {
                        let url = t.FirstURL;
                        let domain = 'duckduckgo.com';
                        try { if (url) domain = new URL(url).hostname.replace(/^www\./, ''); } catch(_) {}
                        results.push({
                            title: t.Text.split(' - ')[0] || url,
                            url: url || `https://duckduckgo.com/?q=${encodeURIComponent(t.Text)}`,
                            description: t.Text.substring(0, 280),
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

    async function searchOpenLibrary(term) {
        const key = 'ol:' + term;
        const cached = _cached(key);
        if (cached) return cached;

        const results = [];
        try {
            const kw = refineQuery(term) || term;
            const data = await _fetch(`https://openlibrary.org/search.json?q=${encodeURIComponent(kw)}&limit=12`);
            if (data.docs) {
                for (const doc of data.docs) {
                    const authors = doc.author_name || [];
                    const desc = (authors.length ? `By ${authors.join(', ')}. ` : '') + (doc.first_publish_year ? `Published ${doc.first_publish_year}. ` : '') + (doc.subject?.slice(0, 3).join(', ') || '');
                    results.push({
                        title: doc.title,
                        url: `https://openlibrary.org${doc.key || '/works/' + doc.cover_edition_key}`,
                        description: desc || doc.title,
                        fullSnippet: doc.description?.value || desc || doc.title,
                        extract: doc.description?.value || desc || doc.title,
                        thumbnail: doc.cover_i ? `https://covers.openlibrary.org/b/id/${doc.cover_i}-M.jpg` : null,
                        source: 'openlibrary',
                        sourceLabel: 'Books',
                        resultType: 'book',
                        score: 5,
                        domain: 'openlibrary.org',
                        suggestion: null
                    });
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
            const sr = await _fetch(`https://commons.wikimedia.org/w/api.php?action=query&format=json&origin=*&list=search&srsearch=${encodeURIComponent(kw)}&srnamespace=6&srlimit=20`);
            if (!sr.query?.search) return results;

            const titles = sr.query.search.map(s => s.title);
            const dt = await _fetch(`https://commons.wikimedia.org/w/api.php?action=query&format=json&origin=*&prop=imageinfo&iiprop=url|size|extmetadata|mime&titles=${encodeURIComponent(titles.join('|'))}`);

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
            const r = await fetch('https://html.duckduckgo.com/html/', {
                method: 'POST',
                headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                body: 'q=' + encodeURIComponent(term)
            });
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
                seen.add(url);

                const title = titleEl.textContent.trim();
                const snippet = snippetEl ? snippetEl.textContent.trim() : '';
                const domain = url ? new URL(url).hostname.replace(/^www\./, '') : '';

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

                if (results.length >= 15) break;
            }
        } catch (_) {}

        _store(key, results);
        return results;
    }

    async function gatewayCrawl(term) {
        if (!term) return [];
        const key = 'full:' + term;
        const cached = _cached(key);
        if (cached) return cached;

        const sources = [
            searchWikipedia(term),
            searchDuckDuckGo(term),
            searchOpenLibrary(term),
            searchCommons(term),
            searchWeb(term)
        ];

        const all = await Promise.allSettled(sources);
        let merged = all.flatMap(p => p.status === 'fulfilled' ? p.value : []);

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

        _store(key, deduped);
        return deduped;
    }

    async function getSuggestions(term) {
        if (!term || term.length < 2) return [];
        const key = 'sug:' + term;
        const cached = _cached(key);
        if (cached) return cached;

        try {
            const wikiDomain = _wikiDomain();
            const data = await _fetch(`https://${wikiDomain}/w/api.php?action=opensearch&format=json&origin=*&search=${encodeURIComponent(term)}&limit=8&namespace=0`);
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
            const data = await _fetch(`https://${wikiDomain}/w/api.php?action=query&format=json&origin=*&list=search&srsearch=${encodeURIComponent(term)}&srlimit=1`);
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
