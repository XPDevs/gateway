# Gateway Crawler

`crawler/crawler.py` is the standalone web crawler that ships with Gateway and was used to generate the bundled index. It walks the web politely, respects `robots.txt`, and writes a single JSON array in the same `{t, u, d, s}` shape that Settings then Upload Index expects. This document is its reference.

Version 3.1.0.

## Contents

1. Overview
2. Requirements
3. Quick start
4. Output format
5. Command line reference
6. Terminal UI
7. Examples
8. Resume and checkpoints
9. ntfy.sh notifications
10. Safety
11. Limitations and behaviour notes
12. Version history

## Overview

Gateway itself does not need a crawler. The bundled index in `index/index.json` is complete and the engine works straight from a static server. The crawler exists for three use cases.

- Refreshing or extending the bundled index from seeds you control.
- Building a small index from scratch on a specific topic with its own seed URLs.
- Producing a larger or fresher custom index that you then load through Settings then Upload Index.

The crawler is designed to run by one person on one machine, unattended, with predictable memory use and a clean shutdown at any moment. It is memory bounded by construction. The frontier is capped, deduplication uses a Bloom filter with a configurable capacity and false positive rate, and each response body is read through a streaming decoder with a hard byte limit. Nothing about it needs a server, a database service or an account.

Two properties are worth stating up front.

- Every URL is checked against an SSRF filter before any request, including every redirect hop. Metadata endpoints such as `169.254.169.254`, localhost, private IP ranges and non HTTP schemes are rejected.
- `robots.txt` is fetched and respected by default. You can disable it with `--ignore-robots`, but only do that on sites you own or are authorised to crawl.

The output is always a single JSON array file. The crawler never writes JSONL or NDJSON. If it finishes with nothing to write, the file contains `[]`. The file is written atomically through a `.part` temporary, an `fsync`, then `os.replace`, so an interrupted crawl never leaves a half written `index.json` behind.

## Requirements

Python 3.8 or later. No `pip install` and no build step. The script uses only the Python standard library. A terminal that supports ANSI escape codes is needed for the full screen UI. On Windows, `msvcrt` supplies key input.

The script is cross platform and runs on Linux, macOS and Windows. Signal handling is skipped gracefully when the crawler is imported and used as a library from a worker thread.

There must be enough free disk for the output file. The crawler refuses to continue when free space on the output volume falls below 50 MB.

## Quick start

Smallest useful run.

```bash
python3 crawler/crawler.py --seed https://example.com --max-pages 500 --output my_index.json
```

That crawls up to 500 pages starting from `example.com`, follows links on the same host at a polite one second delay per host, and writes `my_index.json` as a JSON array. Load it through Settings then Upload Index in Gateway.

Crawl from a list of seeds.

```bash
python3 crawler/crawler.py --seed-file seeds.txt --max-pages 5000 --output my_index.json
```

`seeds.txt` contains one URL per line. Blank lines and lines starting with `#` are ignored.

Crawl until the frontier empties.

```bash
python3 crawler/crawler.py --seed-file seeds.txt --forever --output my_index.json
```

## Output format

The output is one JSON array at `--output`, for example `gateway_index.json` by default. Every element has the following fields. Only `t`, `u`, `d` and `s` are required by the search engine. The rest is metadata that Gateway ignores but that helps with auditing, re ranking or downstream tooling.

| Field | Type | Meaning |
| --- | --- | --- |
| `id` | string | Stable 16 character hex identifier derived from the URL |
| `t` | string | Page title, cleaned and truncated to 200 characters |
| `u` | string | Final URL, tracking parameters stripped, canonical applied |
| `d` | string | Description, from meta description, `og:description`, `twitter:description` or the first substantial paragraph |
| `s` | string | Normalised domain with no `www.` prefix |
| `status` | int | HTTP status code of the final response |
| `content_type` | string | Response MIME type, lowercase, no charset |
| `lang` | string | Language code from `Content-Language`, `<html lang>`, or a stopword heuristic |
| `author` | string | Author from meta, Open Graph or JSON-LD |
| `pub_date` | string | Published date from meta, Open Graph or JSON-LD |
| `mod_date` | string | Modified date from meta, Open Graph or JSON-LD |
| `canonical` | string | Final URL after canonical resolution |
| `word_count` | int | Approximate visible word count |
| `reading_minutes` | int | Approximate reading time in minutes at 200 wpm |
| `content_hash` | string | SHA 256 of the extracted visible text, used for cross page deduplication |

An empty crawl writes `[]`.

When you upload this file through Settings then Upload Index, Gateway validates that each entry has a non empty `t`, `u`, `d` and `s`, persists it to IndexedDB and uses it exactly like the bundled index. The extra fields are ignored by the ranking engine.

## Command line reference

All flags are optional except that you must supply at least one `--seed` or a `--seed-file`.

### Seeds and input

| Flag | Description |
| --- | --- |
| `--seed URL` | Add a seed URL. Repeatable. |
| `--seed-file PATH` | Read seed URLs from a file, one per line. `#` starts a comment. |

### Output

| Flag | Description |
| --- | --- |
| `--output PATH` | Output path. Default `gateway_index.json`. `.json` is strongly recommended. |
| `--storage {json,sqlite}` | Output format. Default `json`, which produces the single JSON array file. `sqlite` writes a `pages` table for downstream tooling and is not intended for direct upload to Gateway. |
| `--dry-run` | Fetch and parse everything but do not write the output file. Useful for probing a site before committing to a run. |

### Politeness and identity

| Flag | Description |
| --- | --- |
| `--delay SECONDS` | Minimum delay between requests to the same host. Default `1.0`. Honours `Crawl-delay` from `robots.txt` when larger. |
| `--timeout SECONDS` | Per request timeout. Default `10.0`. |
| `--user-agent STRING` | User agent string. Default `GatewayCrawler/3.1 (+https://github.com/XPDevs/gateway)`. Please do not spoof a browser UA when crawling sites you do not own. |
| `--same-host` | Only follow links whose domain matches one of the seed domains. Recommended for single site runs. |
| `--max-retries N` | Retry count on `429`, `500`, `502`, `503` and `504`, with exponential backoff and `Retry-After` support. Default `3`. |

### Limits

| Flag | Description |
| --- | --- |
| `--max-pages N` | Stop after N successfully written pages. Omit for no limit. |
| `--forever` | Ignore `--max-pages` and run until the frontier empties. |
| `--max-depth N` | Maximum link depth from a seed. Default `5`. |
| `--max-per-domain N` | Maximum pages per domain. Default `5000`. |
| `--max-links-per-page N` | Maximum links extracted from a single page. Default `200`. |
| `--max-queue-size N` | Frontier capacity. Default `20000`. URLs beyond this cap are dropped and counted. |
| `--bloom-capacity N` | Bloom filter capacity for deduplication. Default `1000000`. |
| `--bloom-fp FLOAT` | Bloom filter false positive rate. Default `0.01`. |
| `--max-page-bytes N` | Hard cap on bytes read from a single response. Default `2097152` (2 MB). |
| `--chunk-size N` | Streaming read chunk size. Default `65536` (64 KB). |
| `--max-memory-mb MB` | Abort if current RSS exceeds this many megabytes. Checked periodically, not on every loop. Off by default. |

### Filtering and content rules

| Flag | Description |
| --- | --- |
| `--exclude-domain REGEX` | Skip any domain matching this regex. Repeatable. Matched against the normalised domain only. |
| `--exclude-url REGEX` | Skip any URL matching this regex. Repeatable. Matched against the full URL. |
| `--language CODE` | Restrict to these language codes. Repeatable. A page with no detectable language is still accepted. |
| `--ignore-robots` | Skip `robots.txt`. Only for sites you own. |
| `--verbose-robots` | Log a one line explanation the first time a domain is blocked by `robots.txt`. |
| `--no-respect-noindex` | Ignore `<meta name="robots" content="noindex">` and the `X-Robots-Tag` header. |

### Discovery

| Flag | Description |
| --- | --- |
| `--use-sitemaps` | Try `/sitemap.xml`, `/sitemap_index.xml` and `/sitemap.txt` on each seed domain, and follow sitemap indexes up to depth 2. |
| `--use-feeds` | Try `/feed`, `/rss`, `/rss.xml`, `/atom.xml` and `/feed.xml` on each seed domain and enqueue their items. |

Both discovery modes honour `robots.txt` for the sitemap or feed file itself, and both honour `--same-host` when it is set. That prevents a third party feed from injecting off site URLs into a single site crawl.

### Checkpoint and resume

| Flag | Description |
| --- | --- |
| `--checkpoint PATH` | Checkpoint file. Default `.gateway_checkpoint.json`. |
| `--resume` | If the checkpoint exists, restore the Bloom filter, frontier, page count, per domain counters and statistics. |

### Debugging and UI

| Flag | Description |
| --- | --- |
| `--quiet` | Suppress `INFO` and `DEBUG` lines on stderr. The TUI still updates. |
| `--log-format {text,json}` | Log format when the TUI is disabled. Default `text`. `json` writes one JSON object per line. |
| `--debug` | Enable debug level logs and write every event to the debug log file. Implies `--no-tui`. |
| `--debug-log PATH` | Debug log path when `--debug` is set. Default `crawl_debug.log`. |
| `--no-tui` | Disable the full screen UI and log to stderr instead. |
| `--refresh-rate SECONDS` | TUI refresh interval. Default `0.5`. |

### Configuration file

| Flag | Description |
| --- | --- |
| `--config PATH` | Read a JSON file of defaults. CLI flags override file values. |

The config file only needs to contain the keys you want to override. Unknown keys are ignored rather than erroring, so an old config keeps working after an upgrade.

### ntfy.sh notifications

| Flag | Description |
| --- | --- |
| `--ntfy-topic TOPIC` | Push notifications to `https://ntfy.sh/<TOPIC>`. Pass a full URL to use a self hosted server. No account or token is needed. |
| `--ntfy-events LIST` | Comma separated list of events to notify on. Default `started,finished`. Supported events are `started`, `progress` and `finished`. |
| `--ntfy-progress-every N` | Send a progress notification every N pages. `0` disables it. Only takes effect if `progress` is in `--ntfy-events`. |

See the ntfy.sh notifications section for message contents.

### Version

| Flag | Description |
| --- | --- |
| `--version` | Print the crawler version and exit. |

## Terminal UI

When stdout is a TTY and neither `--no-tui` nor `--debug` is set, the crawler runs inside a full screen terminal UI. The layout is:

- A title bar with the status (RUNNING, PAUSED, FETCHING, STOPPED) and elapsed time.
- A progress bar showing pages over max pages, or an unbounded counter when no limit is set.
- A stats line with pages, queue, seen, rate, RSS, Bloom saturation, errors, robots blocks and duplicate content.
- The current URL.
- A scrolling activity panel of the last 300 log lines.
- A footer with the key hints.

Keys.

| Key | Action |
| --- | --- |
| `q` | Quit. Sets the stop reason to `user-quit` and finishes the current request before exiting. |
| `p` | Pause or resume the main loop. The in flight request is not interrupted. |
| `i` | Toggle the info panel, which shows HTTP status breakdown, content types, top languages and top domains. |
| `c` | Clear the activity panel. |

On non TTY stdout (CI, cron, redirected output) the TUI disables itself and the crawler logs to stderr in text or JSON. The `NO_TUI` environment variable is honoured. `NO_TUI=0` and `NO_TUI=false` are correctly treated as false, and `1`, `true`, `yes`, `on`, `y` and `t` are truthy.

Terminal width below 60 columns or height below 16 rows shows a short warning and pauses rendering until the window is resized.

## Examples

Crawl one site to a hard cap, same host only.

```bash
python3 crawler/crawler.py \
    --seed https://example.com \
    --same-host \
    --max-pages 2000 \
    --delay 1.5 \
    --output index/example.json
```

Crawl from a large seed list with sitemaps and feeds, no depth limit, memory capped.

```bash
python3 crawler/crawler.py \
    --seed-file seeds.txt \
    --forever \
    --use-sitemaps \
    --use-feeds \
    --max-depth 10 \
    --max-page-bytes 4000000 \
    --max-memory-mb 600 \
    --output index/crawled.json
```

Resume a long crawl after an interruption.

```bash
python3 crawler/crawler.py \
    --seed-file seeds.txt \
    --resume \
    --forever \
    --output index/crawled.json
```

Same crawl, but push start, progress and finish messages to your phone via ntfy.

```bash
python3 crawler/crawler.py \
    --seed-file seeds.txt \
    --max-pages 50000 \
    --output index/crawled.json \
    --ntfy-topic gateway-crawl-2026 \
    --ntfy-events started,progress,finished \
    --ntfy-progress-every 1000
```

A dry run to check which URLs a site exposes without touching disk.

```bash
python3 crawler/crawler.py \
    --seed https://example.com \
    --max-pages 100 \
    --dry-run \
    --no-tui
```

Crawl only English pages, skipping anything under `/account` and any subdomain of `example.net`.

```bash
python3 crawler/crawler.py \
    --seed https://example.com \
    --language en \
    --exclude-url '/account' \
    --exclude-domain 'example\.net$' \
    --same-host \
    --output index/example_en.json
```

## Resume and checkpoints

Checkpoints are written every 200 successfully written pages to `--checkpoint` (default `.gateway_checkpoint.json`). Each checkpoint stores:

- The Bloom filter, serialised and base64 encoded.
- The current frontier, as a list of `(url, depth, base_url)` tuples in priority order.
- The page count.
- The stats snapshot, including HTTP status breakdown, top domains, top languages and failed URLs.
- The seed list.
- The per domain page counts, so `--max-per-domain` still applies correctly across resumes.

On start, if `--resume` is set and the checkpoint file exists, all of the above is restored. The frontier is rebuilt, the Bloom filter is rehydrated, and the per domain and stats counters continue where they left off. A missing or corrupt checkpoint logs a warning and starts fresh. Nothing is ever deleted by the crawler during resume.

The final checkpoint is also written at clean exit, after the writer has flushed and closed. If you abort with Ctrl-C, the current request finishes first, the JSON array is closed with `]`, and only then does the checkpoint get updated. If you abort with a second Ctrl-C, the process exits immediately and the output is left in its last valid atomic state, which is either the previous file or the newly closed one.

## ntfy.sh notifications

Notifications are opt in. When `--ntfy-topic` is set, the crawler sends HTTP POST requests to `https://ntfy.sh/<TOPIC>` (or a self hosted URL). No account or token is needed. The topic is the shared secret, so choose something long and random and treat it like a password.

The three events.

**started** is sent when the main loop begins. It contains the version, number of seeds, output path, page cap and depth cap. Priority is 3 and the tag is `arrow_forward`.

**progress** is sent every `--ntfy-progress-every` written pages when `progress` is in `--ntfy-events`. It contains pages, queue length, errors, robots blocks and rate. Priority is 2 and the tag is `bar_chart`. A percentage is added when `--max-pages` is set.

**finished** is sent in every exit path. The contents depend on how the crawl stopped.

| Stop reason | Priority | Tag | Title |
| --- | --- | --- | --- |
| Normal finish, page limit, or frontier empty | 3 | `checkered_flag` | Crawl finished |
| Interrupted by Ctrl-C, user quit, or signal | 4 | `warning` | Crawl interrupted |
| Disk full or memory limit | 4 | `warning` | Crawl stopped: reason |
| Output JSON validation failed | 4 | `warning` | Crawl finished (validation FAILED) |
| Unhandled error | 5 | `rotating_light` | Crawler crashed |

The finished message contains the stop reason, page count, errors, duplicate count, elapsed time, rate and the output path. When storage is `json` and the run was not a dry run, the `Click` header points at the output file so a tap on the phone opens it.

Failure to send a notification is logged as a warning and never breaks the crawl. The ntfy client has a five second timeout.

## Safety

**SSRF filter.** Every URL is checked before any request with `is_safe_to_fetch()`. The following are rejected.

- Any scheme other than `http` or `https`.
- `localhost`, `localhost.localdomain` and any host ending in `.localhost`.
- The well known cloud metadata hostnames: `169.254.169.254`, `metadata.google.internal`, `metadata.goog` and `100.100.100.200`.
- Any hostname that resolves to a private, loopback, link local, reserved, multicast or unspecified IP address.

The check is re applied on every redirect hop through a custom `SafeRedirectHandler`. Without it, a 30x response could point at an internal endpoint that would otherwise be fetched silently. A blocked redirect raises an HTTP error that the crawler records as a failed URL and moves on from.

Note that DNS rebinding (a hostname that resolves to a public address on the first check and a private one on the second) is out of scope. Fully closing that requires pinning the resolved address and connecting to the IP directly, which is not what this script does. Do not point the crawler at hostnames you do not trust.

**robots.txt.** `robots.txt` is fetched once per domain, capped at 100 KB, and cached in a bounded LRU. A 401 or 403 response is treated as disallow all. Any other 4xx is treated as allow all. A 5xx or network error is treated as allow all. `Crawl-delay` is honoured when present and larger than `--delay`. Sitemaps and feeds are also checked against robots before being fetched.

**noindex and nofollow.** By default, pages with `<meta name="robots" content="noindex">` or an `X-Robots-Tag` header containing `noindex` are skipped and counted. Pages with `nofollow` do not contribute links to the frontier. Both rules can be turned off with `--no-respect-noindex` and by leaving the default on.

**Atomic writes.** The JSON array is written to `<path>.part`, flushed, `fsync`ed, and then `os.replace`d onto the real path. An interrupted write never leaves a corrupt file behind, and the final output is validated by checking that the first byte is `[` and the last non whitespace byte is `]`.

**Signals.** `SIGINT` and `SIGTERM` set a stop flag. The current request finishes, the output is closed cleanly and the checkpoint is written. A second `SIGINT` forces exit after best effort cleanup. Signal installation is skipped when the crawler runs in a worker thread.

**Resource caps.** Free disk is checked before each fetch and the crawl stops below 50 MB. RSS is checked periodically and the crawl stops above `--max-memory-mb` if set. The frontier is capped by `--max-queue-size`. Bloom filter saturation is reported every 200 pages and a warning is logged above 70 percent.

## Limitations and behaviour notes

- Only HTML and `text/plain` responses are parsed into records. PDFs, images, archives and other binary types are skipped and counted as filtered. They are not written as empty entries.
- Only `gzip` and `deflate` content encodings are decoded. `br` (Brotli) is not supported by the standard library and would need a dependency to add. Servers almost always fall back to `gzip` when `Accept-Encoding` does not advertise `br`, so this rarely matters in practice.
- Only `h1`, `h2` and `h3` headings are extracted. Larger heading levels are ignored.
- JSON-LD is scanned for `author`, `headline`, `datePublished`, `dateModified`, `publisher` and `description`. Other properties are ignored.
- `--language` accepts ISO 639-1 codes. A page with no detectable language is accepted rather than rejected, since otherwise pages that omit both `Content-Language` and `<html lang>` would be silently dropped.
- The Bloom filter is probabilistic. With the default `--bloom-fp 0.01`, around 1 percent of unique URLs may be treated as already seen and skipped. Increase `--bloom-capacity` and lower `--bloom-fp` when you need to be sure every URL is fetched exactly once.
- The frontier priority is computed from depth and whether the URL shares a host with its base URL. It does not consider page importance beyond that. If you need a smarter frontier you should feed the crawler a curated seed file rather than a single broad seed.
- The crawler does not respect `robots.txt` `Request-rate` or `Visit-time` directives. It only honours `Disallow` and `Crawl-delay`.
- The crawler does not render JavaScript. Single page apps that serve an empty `<body>` on the initial request will produce a record with an empty title and empty description. The record is still written unless `noindex` is set.
- The `sqlite` storage backend writes a `pages` table that is not uploadable to Gateway. It exists for downstream tooling. Always use `--storage json` when you want a file you can upload.

## Version history

**3.1.0**

- FIX: `Frontier._seen` no longer leaks memory, it discards each popped URL. Deduplication is still enforced by the Bloom filter in the crawler.
- FIX: SSRF via HTTP redirect is now blocked by `SafeRedirectHandler`. Every redirect hop is validated with the same filter used on the initial URL.
- FIX: non HTML responses such as PDF, zip, images and archives are no longer written as empty records. They are counted as filtered.
- FIX: resume now restores `_domain_pages` and the stats counters from the checkpoint, so `--max-per-domain` behaves correctly across resumes and the final report is accurate.
- FIX: `HTTPResponse` attributes are read before `close()`, so status, headers and final URL are always available.
- FIX: `signal.signal()` no longer raises when the crawler is constructed off the main thread.
- FIX: gzip and deflate decompression no longer drops the tail. `flush()` is called at the end of the stream and any remaining bytes are appended.
- FIX: `NO_TUI=0` and `NO_TUI=false` no longer disable the TUI. The environment flag parser is shared and correct.
- FIX: sitemap and feed URLs now honour `--same-host` and `robots.txt`.
- FIX: TUI key polling continues during the per host polite wait, so `q`, `p`, `i` and `c` respond during pauses.
- FIX: `<title>` length limit is now by character rather than by HTMLParser chunk, so long titles truncate cleanly at 500 characters.
- NEW: ntfy.sh push notifications via `--ntfy-topic`. No token needed.
- NEW: `--ntfy-events` and `--ntfy-progress-every` for choosing which events fire and how often progress messages are sent.

**Before 3.1.0**

The crawler existed as an internal tool. Version 3.1.0 is the first release with this documentation and the flags described above.
