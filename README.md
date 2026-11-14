# Gateway Search

A fast, private, multi source search engine by [XPDevs](https://xpdevs.github.io). Gateway searches a bundled local web index of 50,000 real websites across 50,000 domains, plus live Wikipedia and Wikidata open data. It requires no API keys, no build step and no generative AI.

Live behaviour. Main results paint from the local index first, typically in milliseconds. Quick Wiki, spell check and web enrichment then resolve asynchronously afterwards.

Your searches are recorded to a local only [search history](history/) you can replay, filter, switch off or delete. The history never leaves the browser and never affects ranking.

## Features

**Instant main results, faster than Google for the common case**

Two stage search. Stage 1 renders `gatewaySearchLocal()` using only the local IndexedDB or memory index, with no network, inside `requestAnimationFrame`. Stage 2 enriches with `gatewayCrawl()` from live Wikipedia, official sites and cited web links without replacing the visible list.

Result stats show measured time, for example `120 ranked results (0.03 seconds)`.

`content-visibility: auto` on `.result-item`, 20 per page pagination, in memory plus `localStorage` plus IndexedDB caches, and request deduplication via `_once()` and `_inflight`.

**Quick Wiki, lazy and sourced with an image**

Deterministic answers from Wikipedia extracts and Wikidata claims such as `P577`, `P571`, `P36` and `P1082`. Date intent parsing is supported, and no generative AI is used.

Quick Wiki renders after the main list. A skeleton placeholder appears first and is replaced asynchronously when `gatewayQuickWiki()` resolves. Failures, or queries Wikipedia cannot answer, clear the skeleton and show no box at all, without touching results. Quick Wiki can be disabled entirely in Settings via `toggleQuickWiki()`, `setQuickWikiEnabled()` and `gatewayQuickWikiEnabled()`.

Summary only answers are gated on a strong title match, either exact or at least 75% subject word overlap. Weak matches return `null`.

Answer and context duplicates are eliminated. `_dedupeAnswerContext()` strips any context that repeats the answer, and a render side guard ensures the answer sentence never appears twice.

A representative image is taken from `pageimages` `thumbnail` or `original` at 500px, with a sibling article fallback. It floats right so body text wraps around it and continues underneath. The image uses `<img loading="lazy" decoding="async" fetchpriority="low">` with a caption and an `onerror` collapse that stacks full width on mobile.

The header badge reads `No generative AI`. A property chip shows the claim identifier, for example `P577`. Source links are official and cited, deduplicated per domain.

**XPDevs profile, local and deterministic**

Searching `XPDevs`, or any query containing the token such as `xpdevs`, `xp dev`, `xp-dev`, `xpdev` or `gateway search`, is answered entirely from `XPDEV_PROFILE` in `main.js` with no network call and no Wikipedia round trip.

Quick Wiki shows the profile answer, the context line, the repository count, source links for the website, GitHub, DoorsOS, ExamOS and Genesis AI, and the project logo `https://xpdevs.github.io/logo/XPDevs.png` with an `XPDevs` source badge. It never shows a false Wikipedia badge or caption.

The same query pins 62 rows above every ranked result. These are the official site, the GitHub organisation, Gateway and all 59 public repositories at `github.com/XPDevs/<name>`. Each repository row includes language, stars, last update and a `fork of an upstream project` flag where applicable. An XPDevs query renders the whole set in one page instead of 20 row pages, so no repository is hidden behind a Show more results control.

Facts are sourced from the official site at `xpdevs.github.io` covering operating systems, United Kingdom and Genesis AI focus, and from the GitHub API. Every other query path is untouched.

**Favicons on every web URL**

Each result row and each Quick Wiki source link shows a favicon. Google S2 is primary at `s2/favicons?domain=…&sz=32`, DuckDuckGo at `icons.duckduckgo.com/ip3/…` is the fallback, and a letter avatar is the final fallback, so no row lacks an icon.

Images use `loading="lazy"` and `decoding="async"`.

**Unique descriptions**

All 50,000 `index.json` descriptions are unique. Verified counts are 50,000 unique descriptions out of 50,000 and 50,000 unique URLs out of 50,000.

All 50,000 entries are real websites, one per domain, sourced from Wikidata and confirmed to answer over HTTP at build time.

**Search history, local and reversible**

Every search is recorded to `localStorage` under `gw-history` as `{q, t, n, ms}`. The fields are query, time, result count and duration. Entries are newest first and capped at 200.

Recording happens once in `performSearch()` in `main.js` after the fast local stage paints, so the stored count and duration match what the user actually saw. The Stage 2 enrichment pass corrects the count in place rather than logging a second entry.

`/history/` at `history/index.html` is a standalone static page. It cannot load `main.js`, so it implements the same reader against the same key. Entries are grouped by day, filterable, and each row replays the search via `../?query=…`. That query parameter is already honoured on load by `main.js:1108`.

Controls include delete a single search, delete all, export the list as JSON and a switch to stop recording new searches via `gw-history-optout`. Recording is on by default.

Nothing is uploaded. History is never attached to a search, never included in a custom or exported index, and never used for ranking or profiling. Storage failures such as quota limits or private mode degrade silently and never break a search.

Warning. The storage key is deliberately not named `history`. `performSearch()` uses `window.history.replaceState` from the History API, and a global of that name would clobber it. The API surface is `window.gatewaySearchHistory`.

**BM25 hybrid ranking**

See the Ranking section below. The implementation uses IDF with additive smoothing, `IDF^1.5` term weighting, Okapi BM25 and a static authority prior.

**Plus**

25 language UI, dark mode, autocomplete via `getSuggestions`, Did you mean spell check, I'm Feeling Lucky, uploadable custom index, safe domain filtering and tracking parameter stripping.

## Project structure

```
gateway-main/
├── index.html       # Home and results UI, settings modal
├── ui.css           # Theme, results, Quick Wiki, favicon, responsive styles
├── main.js          # UI, staged search orchestration, rendering, favicons, Quick Wiki image
├── crawl.js         # Index engine, Wikipedia and Wikidata fetchers, ranking, caches
├── index/
│   ├── index.json       # Bundled local index: [{t, u, d, s}] with 50,000 unique description entries
│   └── index-meta.json  # Index version and entry count for cache busting
├── about/ advertising/ business/ how-search-works/ history/ privacy/ terms/  # Static pages
├── favicon.ico
└── 404.html
```

## Setup

No build and no dependencies. Any static server works.

```bash
cd gateway-main
python3 -m http.server 8000
# open http://localhost:8000/
```

You can also use `npx serve .`, GitHub Pages, Nginx or any other static host. Use `http` or `https`, not `file`, because fetch and IndexedDB require it.

Custom index is optional. Go to Settings, then Upload Index, and select a JSON array of `{t, u, d, s}`. The index is validated, persisted to IndexedDB and versioned so old caches are invalidated.

Settings, opened with the gear icon, include language, dark mode, Quick Wiki on or off and index upload. Quick Wiki is persisted as `gw-quickwiki` in `localStorage` and defaults to on. When Quick Wiki is off, no Quick Wiki box is fetched or rendered. The `Try "release date…"` example enables it again.

## Architecture

```
User query
  ├─ Stage 1, sync fast: gatewaySearchLocal() → prepared normalised index → score → _boostXpdevs → paint via rAF and timing
  ├─ Stage 2a, async: gatewayCrawl() → local plus Wikipedia search plus official URLs plus extlinks → merge new URLs only, sort again, update count
  ├─ Stage 2b, async and lazy: gatewayQuickWiki() → parse intent → wiki search → best page match → Wikidata claims → image → renderQuickWiki()
  └─ Stage 2c, async: gatewaySpellCheck() → renderDidYouMean()
```

Key modules.

`crawl.js` contains `_normalize`, `_prepareIndex` with term frequencies, document frequency, `avgdl` and authority, `searchIndex`, `gatewaySearchLocal`, `gatewayCrawl`, `_searchWikipedia` with opensearch to extracts, pageimages, pageprops and info, plus extlinks for the best page. It also contains `_wikidataClaims`, `_labelsForClaims`, `_formatWikidataTime`, `quickWiki` with `_dedupeAnswerContext` and `_subjectMatchRatio` gating, `getSuggestions`, `spellCheck`, `_dedupeAndRank`, `isSafeResult`, `cleanTracking`, and a tiered cache from memory to `localStorage` `gw_v6_` to IndexedDB `GatewayIndex`. It loads the bundled index from `index/index.json` and `index/index-meta.json` via `INDEX_DIR`. Introspection helpers are `gatewayRankDebug(query)` and `gatewayAuthorityScore(domain)`.

`main.js` contains `performSearch` which is staged, `_gwRender` which renders stats with seconds, `renderDidYouMean`, `renderQuickWiki` and paginated `renderItem`, `_faviconHtml` with S2, DDG and letter fallback, `_answerLinkHtml`, translations for 25 locales, dark mode and autocomplete wiring.

## Ranking

Every result, whether from the bundled index, live Wikipedia, official sites or cited web links, is scored with one hybrid function so all sources compete on identical terms.

```
FinalScore = (BM25_Score * 0.8) + (Authority_Score * 0.2)
```

1. IDF with additive smoothing. `IDF(q_i) = ln(1 + (N - n(q_i) + 0.5) / (n(q_i) + 0.5))`.
   `N = 20,000,000,000` at web corpus scale via `RANK_CORPUS_SIZE`, which can be overridden with `window.GATEWAY_RANK_CORPUS` for self hosted corpora. `n(q_i)` is document frequency, counted at index load.
2. Dynamic query term weighting. `W(q_i) = IDF(q_i) ^ 1.5`. Rare entities are boosted and common terms are damped.
3. Okapi BM25. `BM25(D, Q) = Σ W(q_i) · (f(q_i,D) · (k1 + 1)) / (f(q_i,D) + k1 · (1 - b + b · (|D| / avgdl)))` with `k1 = 1.2` and `b = 0.75`.
   Documents are title and description. `|D|` is the word count. `avgdl` is the mean across the whole index, 12.73 words for the bundled set, from 548,586 tokens over 50,000 documents and 79,765 distinct terms.
4. Authority. A pre calculated static float in the range 0.0 to 10.0, PageRank style, resolved per domain. Resolution goes from an exact table to a parent domain walk such as `en.wikipedia.org` to `wikipedia.org`, then to public sector and academic host shapes such as `.gov`, `gov.uk`, `gob.ve`, `.edu` and `ac.uk`, and finally to a deterministic 2.0 to 6.0 hash band for everything else. It never varies with the query. An index entry may ship its own value in `a`, which always wins.

Because authority contributes at most 2.0 points, relevance dominates and authority only breaks ties. This is the intended split of 80 percent relevance and 20 percent authority.

Tokenisation. Query terms are stopword filtered. Single digits are kept, as in `PlayStation 5` and `World War 2`, while lone letters are dropped. Document text keeps every token, so stopwords still count toward `f` and `|D|`. Matching is whole token, not substring.

Removed in this revision. The old hand tuned constants are gone, including exact title `+700`, title prefix `+380`, per field `+55`, `+32`, `+24` and `+9`, the coverage bonus, and the fixed Wikipedia, official and web boosts of `2000`, `1800`, `930`, `280`, `360` and `260`. A latent `STOPWORDS` bug was also fixed. The old `.split(' ')` bound only to the last string literal, so the set held 20 stray characters and no stopword was ever filtered.

Verified by recalculating IDF, `W`, BM25 and `FinalScore` from the raw index for more than 150 results across 6 queries. Maximum drift is `0.0`. Corpus stats, with `avgdl` at 12.7286 and vocabulary at 79,765, also match an independent count. Warm search averages about 3 ms over 50,000 documents, with a median of 2.6 ms and a p95 of 21.1 ms across 600 timed `gatewaySearchLocal()` calls. One time index preparation costs about 1.3 seconds and is cached in memory.

## Performance notes

First paint never awaits network, and network failures cannot blank results.

`MAX_INDEX_RESULTS=120`, `MAX_RESULTS=100` and `PER_PAGE=20`. Each search walks only the documents its query terms actually match in a prebuilt inverted index. Warm median is about 2.6 ms measured over the bundled 50,000 entry index, plus a one time `df`, `avgdl` and authority precompute of about 1.3 seconds that is cached in memory.

To verify, search anything and read `result-stats`, for example `(0.04 seconds)`. Compare against Google's typical 0.3 to 0.6 second SERP time.

## Data notes

`index/index.json` format uses `t` for title, `u` for URL with tracking parameters stripped, `d` for unique description, `s` for normalised domain, and an optional `a` for pre calculated authority from 0.0 to 10.0. When `a` is absent, the domain table is used.

Regeneration check.

```bash
python3 -c "import json; d=json.load(open('index/index.json')); print(len(d), len(set(x['d'] for x in d)))"
```

This prints `50000 50000`.

Local keys written by the browser, all removable via site data. Preferences are `gw-lang`, `gw-dark` and `gw-quickwiki`. The search cache is `gw_v6_` with a 10 minute lifetime and a maximum of 30 keys. Search history uses `gw-history` and `gw-history-optout` with a maximum of 200 entries. Nothing else is persisted, and none of it is ever transmitted.

## Security and privacy

Dangerous domains, private IP hostnames, non HTTP or HTTPS URLs, and malware keyword hits are filtered by `isSafeDomain`, `isSafeResult` and `_safeHref`.

Tracking parameters such as `utm_*`, `fbclid` and `gclid` are stripped, along with URL hashes. There is no third party analytics. Favicon and image loads are the only external requests besides the Wikipedia and Wikidata APIs.

## Screenshots

### Homepage

![Gateway Search Homepage](images/hompage_example.png)

Clean and minimal interface with instant local search.

### Search Results

![Gateway Search Results for Costa Coffee](images/search_results%20example.png)

Results for Costa Coffee, featuring the Quick Wiki box, result stats and favicons.
