#include "crawler.h"

/* command line interface (argparse equivalent) and main() */
/* ==========================================================================
 * Section 17 - command line interface (argparse equivalent) and main()
 * ========================================================================== */


void cli_defaults(cli_args *a)
{
    memset(a, 0, sizeof *a);
    vec_init(&a->seeds);
    vec_init(&a->exclude_domain);
    vec_init(&a->exclude_url);
    vec_init(&a->languages);
    a->output = xstrdup(DEFAULT_OUTPUT);
    a->delay = DEFAULT_DELAY;
    a->timeout = DEFAULT_TIMEOUT;
    a->user_agent = xstrdup(DEFAULT_USER_AGENT);
    a->max_depth = DEFAULT_MAX_DEPTH;
    a->max_per_domain = DEFAULT_MAX_PER_DOM;
    a->max_links_per_page = DEFAULT_MAX_LINKS;
    a->max_queue_size = DEFAULT_MAX_QUEUE;
    a->bloom_capacity = DEFAULT_BLOOM_CAP;
    a->bloom_fp = DEFAULT_BLOOM_FP;
    a->max_page_bytes = DEFAULT_PAGE_BYTES;
    a->chunk_size = DEFAULT_CHUNK;
    a->max_retries = DEFAULT_MAX_RETRIES;
    a->checkpoint = xstrdup(DEFAULT_CHECKPOINT);
    a->debug_log = xstrdup("crawl_debug.log");
    a->log_format = xstrdup("text");
    a->refresh_rate = TUI_REFRESH_DEFAULT;
    a->ntfy_topic = xstrdup("");
    a->ntfy_events = xstrdup(DEFAULT_NTFY_EVENTS);
    a->bench_iterations = 200;
}

void print_usage(FILE *out)
{
    fprintf(out,
        "usage: gateway_crawler [-h] [--seed SEED] [--seed-file SEED_FILE]\n"
        "                       [--max-pages MAX_PAGES] [--forever] [--output OUTPUT]\n"
        "                       [--storage {json,sqlite}] [--delay DELAY] [--timeout TIMEOUT]\n"
        "                       [--user-agent USER_AGENT] [--config CONFIG] [--same-host]\n"
        "                       [--exclude-domain EXCLUDE_DOMAIN] [--exclude-url EXCLUDE_URL]\n"
        "                       [--language LANGUAGE] [--max-depth MAX_DEPTH]\n"
        "                       [--max-per-domain MAX_PER_DOMAIN]\n"
        "                       [--max-links-per-page MAX_LINKS_PER_PAGE]\n"
        "                       [--max-queue-size MAX_QUEUE_SIZE]\n"
        "                       [--bloom-capacity BLOOM_CAPACITY] [--bloom-fp BLOOM_FP]\n"
        "                       [--max-page-bytes MAX_PAGE_BYTES] [--chunk-size CHUNK_SIZE]\n"
        "                       [--max-memory-mb MAX_MEMORY_MB] [--max-retries MAX_RETRIES]\n"
        "                       [--ignore-robots] [--verbose-robots] [--no-respect-noindex]\n"
        "                       [--use-sitemaps] [--use-feeds] [--checkpoint CHECKPOINT]\n"
        "                       [--resume] [--debug] [--debug-log DEBUG_LOG] [--quiet]\n"
        "                       [--log-format {text,json}] [--dry-run] [--no-tui]\n"
        "                       [--refresh-rate REFRESH_RATE] [--ntfy-topic TOPIC]\n"
        "                       [--ntfy-events LIST] [--ntfy-progress-every N]\n\n"
        "Crawl the web into an XPDevs Gateway index (bounded RAM, TUI).\n\n"
        "options:\n"
        "  -h, --help            show this help message and exit\n"
        "  --seed SEED           Seed URL (repeatable).\n"
        "  --seed-file SEED_FILE File with one seed URL per line.\n"
        "  --max-pages MAX_PAGES Stop after N pages (omit for infinite).\n"
        "  --forever             Ignore --max-pages; run until frontier empties.\n"
        "  --output OUTPUT       Output path (default: %s).\n"
        "  --storage {json,sqlite}\n"
        "                        Output format (default: json array).\n"
        "  --delay DELAY         Per-host delay seconds (default: %.1f).\n"
        "  --timeout TIMEOUT     Request timeout seconds (default: %.1f).\n"
        "  --user-agent USER_AGENT\n"
        "                        User-Agent string.\n"
        "  --config CONFIG       JSON config file with defaults (CLI overrides).\n"
        "  --version             show program's version number and exit\n"
        "  --same-host           Only follow links on the seed domain(s).\n"
        "  --exclude-domain EXCLUDE_DOMAIN\n"
        "                        Exclude domains matching this regex (repeatable).\n"
        "  --exclude-url EXCLUDE_URL\n"
        "                        Exclude URLs matching this regex (repeatable).\n"
        "  --language LANGUAGE   Restrict to these languages (ISO codes, repeatable).\n"
        "  --max-depth MAX_DEPTH Max crawl depth (default: %d).\n"
        "  --max-per-domain MAX_PER_DOMAIN\n"
        "                        Max pages per domain (default: %d).\n"
        "  --max-links-per-page MAX_LINKS_PER_PAGE\n"
        "                        Max links extracted per page (default: %d).\n"
        "  --max-queue-size MAX_QUEUE_SIZE\n"
        "                        Frontier capacity (default: %d).\n"
        "  --bloom-capacity BLOOM_CAPACITY\n"
        "                        Bloom filter capacity (default: %d).\n"
        "  --bloom-fp BLOOM_FP   Bloom false-positive rate (default: %.2f).\n"
        "  --max-page-bytes MAX_PAGE_BYTES\n"
        "                        Max bytes per page (default: %d).\n"
        "  --chunk-size CHUNK_SIZE\n"
        "                        Streaming chunk size (default: %d).\n"
        "  --max-memory-mb MAX_MEMORY_MB\n"
        "                        Abort if current RSS exceeds this many MB.\n"
        "  --max-retries MAX_RETRIES\n"
        "                        Retry attempts on transient failures (default: %d).\n"
        "  --ignore-robots       Skip robots.txt checks (own sites only).\n"
        "  --verbose-robots      Explain robots.txt blocks.\n"
        "  --no-respect-noindex  Ignore noindex / X-Robots-Tag rules.\n"
        "  --no-favicons         Omit favicon discovery and the favicon field\n"
        "                        (output then matches the Python version byte for byte).\n"
        "  --clean               De-duplicate before writing. Pages whose opening text\n"
        "                        matches an entry already written are dropped; among\n"
        "                        near-duplicates found together the fullest record wins\n"
        "                        (most words, then longest description, then longest\n"
        "                        title, then shortest URL). The winner is written once\n"
        "                        and every later duplicate is discarded.\n"
        "  --clean-window N      Records held before a --clean flush (default: "
        GW_STR(CLEAN_DEFAULT_WINDOW) ").\n"
        "                        Larger windows resolve duplicates seen further apart.\n"
        "  --use-sitemaps        Discover and parse sitemap.xml / sitemap.txt.\n"
        "  --use-feeds           Discover and parse RSS/Atom feeds.\n"
        "  --checkpoint CHECKPOINT\n"
        "                        Checkpoint file (default: %s).\n"
        "  --resume              Resume from checkpoint if present.\n"
        "  --debug               Enable detailed tracing and write every event to crawl_debug.log.\n"
        "  --debug-log DEBUG_LOG Debug log path (used with --debug; default: crawl_debug.log).\n"
        "  --quiet               Suppress INFO/DEBUG logs (TUI still shows progress).\n"
        "  --log-format {text,json}\n"
        "                        Log format when TUI is disabled (default: text).\n"
        "  --dry-run             Fetch and parse but do not write output.\n"
        "  --no-tui              Disable full-screen TUI (logs to stderr).\n"
        "  --refresh-rate REFRESH_RATE\n"
        "                        TUI refresh interval in seconds (default: %.1f).\n\n"
        "ntfy.sh notifications:\n"
        "  --ntfy-topic TOPIC    Push notifications to https://ntfy.sh/<TOPIC>. Pass a full\n"
        "                        URL (https://...) to use a self-hosted server. Requires no\n"
        "                        account or token. Disabled by default.\n"
        "  --ntfy-events LIST    Comma-separated events to notify: started,progress,finished.\n"
        "                        Default: %s\n"
        "  --ntfy-progress-every N\n"
        "                        Send a progress notification every N pages. 0 disables\n"
        "                        progress notifications (default). Only takes effect if\n"
        "                        'progress' is in --ntfy-events.\n",
        DEFAULT_OUTPUT, DEFAULT_DELAY, DEFAULT_TIMEOUT, DEFAULT_MAX_DEPTH,
        DEFAULT_MAX_PER_DOM, DEFAULT_MAX_LINKS, DEFAULT_MAX_QUEUE, DEFAULT_BLOOM_CAP,
        DEFAULT_BLOOM_FP, DEFAULT_PAGE_BYTES, DEFAULT_CHUNK, DEFAULT_MAX_RETRIES,
        DEFAULT_CHECKPOINT, TUI_REFRESH_DEFAULT, DEFAULT_NTFY_EVENTS);
}

int need_value(int argc, int i, const char *name)
{
    if (i + 1 >= argc) {
        fprintf(stderr, "argument %s: expected one argument\n", name);
        exit(2);
    }
    return i + 1;
}

void cli_parse(cli_args *a, int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        char *arg = argv[i];
        char *eq = NULL;
        char namebuf[128];
        if (!strncmp(arg, "--", 2)) {
            eq = strchr(arg, '=');
            if (eq && (size_t)(eq - arg) < sizeof namebuf) {
                memcpy(namebuf, arg, (size_t)(eq - arg));
                namebuf[eq - arg] = '\0';
                arg = namebuf;
            }
        }
        const char *value_at = eq ? eq + 1 : NULL;
        if (!strcmp(arg, "-h") || !strcmp(arg, "--help")) {
            print_usage(stdout);
            exit(0);
        } else if (!strcmp(arg, "--version")) {
            printf("gateway_crawler %s\n", VERSION);
            exit(0);
        } else if (!strcmp(arg, "--seed")) {
            vec_push(&a->seeds, TAKE_VALUE());
        } else if (!strcmp(arg, "--seed-file")) {
            SET_STR(seed_file, TAKE_VALUE());
        } else if (!strcmp(arg, "--max-pages")) {
            { char *__v = TAKE_VALUE(); a->max_pages = strtol(__v, NULL, 10); free(__v); }
            a->has_max_pages = 1;
        } else if (!strcmp(arg, "--forever")) {
            a->forever = 1;
        } else if (!strcmp(arg, "--output")) {
            SET_STR(output, TAKE_VALUE());
        } else if (!strcmp(arg, "--storage")) {
            char *v = TAKE_VALUE();
            if (!strcmp(v, "sqlite")) a->storage_sqlite = 1;
            else if (!strcmp(v, "json")) a->storage_sqlite = 0;
            else { fprintf(stderr, "argument --storage: invalid choice: '%s'\n", v); exit(2); }
            free(v);
        } else if (!strcmp(arg, "--delay")) {
            { char *__v = TAKE_VALUE(); a->delay = strtod(__v, NULL); free(__v); }
        } else if (!strcmp(arg, "--timeout")) {
            { char *__v = TAKE_VALUE(); a->timeout = strtod(__v, NULL); free(__v); }
        } else if (!strcmp(arg, "--user-agent")) {
            SET_STR(user_agent, TAKE_VALUE());
        } else if (!strcmp(arg, "--config")) {
            SET_STR(config, TAKE_VALUE());
        } else if (!strcmp(arg, "--same-host")) {
            a->same_host = 1;
        } else if (!strcmp(arg, "--exclude-domain")) {
            vec_push(&a->exclude_domain, xstrdup(TAKE_VALUE()));
        } else if (!strcmp(arg, "--exclude-url")) {
            vec_push(&a->exclude_url, xstrdup(TAKE_VALUE()));
        } else if (!strcmp(arg, "--language")) {
            vec_push(&a->languages, xstrdup(TAKE_VALUE()));
        } else if (!strcmp(arg, "--max-depth")) {
            { char *__v = TAKE_VALUE(); a->max_depth = (int)strtol(__v, NULL, 10); free(__v); }
        } else if (!strcmp(arg, "--max-per-domain")) {
            { char *__v = TAKE_VALUE(); a->max_per_domain = strtol(__v, NULL, 10); free(__v); }
        } else if (!strcmp(arg, "--max-links-per-page")) {
            { char *__v = TAKE_VALUE(); a->max_links_per_page = strtol(__v, NULL, 10); free(__v); }
        } else if (!strcmp(arg, "--max-queue-size")) {
            { char *__v = TAKE_VALUE(); a->max_queue_size = (size_t)strtoull(__v, NULL, 10); free(__v); }
        } else if (!strcmp(arg, "--bloom-capacity")) {
            { char *__v = TAKE_VALUE(); a->bloom_capacity = strtoull(__v, NULL, 10); free(__v); }
        } else if (!strcmp(arg, "--bloom-fp")) {
            { char *__v = TAKE_VALUE(); a->bloom_fp = strtod(__v, NULL); free(__v); }
        } else if (!strcmp(arg, "--max-page-bytes")) {
            { char *__v = TAKE_VALUE(); a->max_page_bytes = (size_t)strtoull(__v, NULL, 10); free(__v); }
        } else if (!strcmp(arg, "--chunk-size")) {
            { char *__v = TAKE_VALUE(); a->chunk_size = (size_t)strtoull(__v, NULL, 10); free(__v); }
        } else if (!strcmp(arg, "--max-memory-mb")) {
            { char *__v = TAKE_VALUE(); a->max_memory_mb = strtod(__v, NULL); free(__v); }
            a->has_max_memory = 1;
        } else if (!strcmp(arg, "--max-retries")) {
            { char *__v = TAKE_VALUE(); a->max_retries = (int)strtol(__v, NULL, 10); free(__v); }
        } else if (!strcmp(arg, "--ignore-robots")) {
            a->ignore_robots = 1;
        } else if (!strcmp(arg, "--verbose-robots")) {
            a->verbose_robots = 1;
        } else if (!strcmp(arg, "--no-respect-noindex")) {
            a->no_respect_noindex = 1;
        } else if (!strcmp(arg, "--no-favicons")) {
            a->no_favicons = 1;
        } else if (!strcmp(arg, "--clean")) {
            a->clean = 1;
        } else if (!strcmp(arg, "--clean-window")) {
            { char *__v = TAKE_VALUE(); a->clean_window = strtol(__v, NULL, 10); free(__v); }
        } else if (!strcmp(arg, "--use-sitemaps")) {
            a->use_sitemaps = 1;
        } else if (!strcmp(arg, "--use-feeds")) {
            a->use_feeds = 1;
        } else if (!strcmp(arg, "--checkpoint")) {
            SET_STR(checkpoint, TAKE_VALUE());
        } else if (!strcmp(arg, "--resume")) {
            a->resume = 1;
        } else if (!strcmp(arg, "--debug")) {
            a->debug = 1;
        } else if (!strcmp(arg, "--debug-log")) {
            SET_STR(debug_log, TAKE_VALUE());
        } else if (!strcmp(arg, "--quiet")) {
            a->quiet = 1;
        } else if (!strcmp(arg, "--log-format")) {
            char *v = TAKE_VALUE();
            if (!strcmp(v, "text") || !strcmp(v, "json")) {
                free(a->log_format);
                a->log_format = xstrdup(v);
            }
            else { fprintf(stderr, "argument --log-format: invalid choice: '%s'\n", v); exit(2); }
            free(v);
        } else if (!strcmp(arg, "--dry-run")) {
            a->dry_run = 1;
        } else if (!strcmp(arg, "--no-tui")) {
            a->no_tui = 1;
        } else if (!strcmp(arg, "--refresh-rate")) {
            { char *__v = TAKE_VALUE(); a->refresh_rate = strtod(__v, NULL); free(__v); }
        } else if (!strcmp(arg, "--ntfy-topic")) {
            SET_STR(ntfy_topic, TAKE_VALUE());
        } else if (!strcmp(arg, "--ntfy-events")) {
            SET_STR(ntfy_events, TAKE_VALUE());
        } else if (!strcmp(arg, "--ntfy-progress-every")) {
            { char *__v = TAKE_VALUE(); a->ntfy_progress_every = strtol(__v, NULL, 10); free(__v); }
        } else if (!strcmp(arg, "--bench-html")) {
            SET_STR(bench_html, TAKE_VALUE());
        } else if (!strcmp(arg, "--bench-iterations")) {
            { char *__v = TAKE_VALUE(); a->bench_iterations = strtol(__v, NULL, 10); free(__v); }
        } else {
            fprintf(stderr, "unrecognized arguments: %s\n", argv[i]);
            exit(2);
        }
#undef TAKE_VALUE
    }
}

/* load_config_file: parsed for validation, exactly like the Python original
 * (whose config values are never applied to the crawler) */
void load_config_file(const char *path)
{
    if (!path || !*path) return;
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "[WARN] config load failed: cannot open %s\n", path);
        return;
    }
    fseek(fp, 0, SEEK_END);
    long n = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (n < 0) { fclose(fp); fprintf(stderr, "[WARN] config load failed\n"); return; }
    char *buf = xmalloc((size_t)n + 1);
    size_t got = fread(buf, 1, (size_t)n, fp);
    fclose(fp);
    buf[got] = '\0';
    json_val *v = json_parse(buf);
    if (!v) fprintf(stderr, "[WARN] config load failed: invalid JSON in %s\n", path);
    else json_free(v);
    free(buf);
}

/* --------------------------------------------------------------------------
 * Port-only helper: measurable HTML pipeline benchmark (--bench-html FILE).
 * Runs exactly the same work the fetcher does per page (encoding detection,
 * decode, both HTML passes, paragraph dedupe, hashing, JSON record build) so
 * the C pipeline can be timed against the Python one on identical input.
 * -------------------------------------------------------------------------- */
int run_html_bench(const char *path, long iterations)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "[ERROR] cannot open %s\n", path);
        return 2;
    }
    fseek(fp, 0, SEEK_END);
    long n = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    unsigned char *body = xmalloc((size_t)n + 1);
    size_t got = fread(body, 1, (size_t)n, fp);
    fclose(fp);
    body[got] = '\0';

    logger_t *log = xcalloc(1, sizeof(logger_t));
    logger_init(log, 0, 1, "text", NULL, NULL);
    fetcher_t f;
    fetcher_init(&f, DEFAULT_USER_AGENT, DEFAULT_TIMEOUT, 0, DEFAULT_PAGE_BYTES,
                 DEFAULT_CHUNK, log);

    double t0 = now_sec();
    long words_total = 0;
    size_t json_bytes = 0;
    char *first_title = NULL;
    for (long i = 0; i < iterations; i++) {
        fetch_result res;
        fetch_result_init(&res);
        free(res.final_url);
        res.final_url = xstrdup("http://bench.local/page");
        free(res.content_type);
        res.content_type = xstrdup("text/html");
        res.status = 200;
        res.body = body;
        res.body_len = got;
        res.html_bytes = got;
        fetch_parse(&f, &res);
        record_t rec;
        vec_t *links = NULL;
        char *rm = NULL;
        build_result(NULL, res.final_url, &res, &rec, &links, &rm);
        sb_t json;
        sb_init(&json);
        record_to_json(&json, &rec);
        json_bytes = json.len;
        if (!first_title) first_title = xstrdup(rec.t);
        const char *dump = getenv("GWC_BENCH_DUMP");
        if (dump && *dump && i == iterations - 1) {
            FILE *df = fopen(dump, "w");
            if (df) { fputs(res.content.text, df); fclose(df); }
        }
        words_total += rec.word_count;
        sb_free(&json);
        record_free(&rec);
        res.body = NULL;
        fetch_result_free(&res);
    }
    double elapsed = now_sec() - t0;
    printf("bench: %s (%zu bytes) x %ld iterations\n", path, got, iterations);
    printf("total_seconds: %.4f\n", elapsed);
    printf("ms_per_page: %.3f\n", elapsed * 1000.0 / (double)iterations);
    printf("pages_per_second: %.1f\n", (double)iterations / elapsed);
    printf("word_count: %ld\n", words_total / (iterations ? iterations : 1));
    printf("record_json_bytes: %zu\n", json_bytes);
    printf("title: %s\n", first_title ? first_title : "");
    free(first_title);
    fetcher_free(&f);
    logger_close(log);
    free(log);
    free(body);
    return 0;
}

void cli_free(cli_args *a)
{
    vec_free(&a->seeds, free);
    vec_free(&a->exclude_domain, free);
    vec_free(&a->exclude_url, free);
    vec_free(&a->languages, free);
    free(a->seed_file);
    free(a->output);
    free(a->user_agent);
    free(a->config);
    free(a->checkpoint);
    free(a->debug_log);
    free(a->log_format);
    free(a->ntfy_topic);
    free(a->ntfy_events);
    free(a->bench_html);
}

int main(int argc, char **argv)
{
    cli_args a;
    cli_defaults(&a);
    cli_parse(&a, argc, argv);

    load_config_file(a.config);

    if (a.bench_html)
        return run_html_bench(a.bench_html, a.bench_iterations);

    /* seeds from --seed-file */
    if (a.seed_file) {
        FILE *fp = fopen(a.seed_file, "r");
        if (!fp) {
            fprintf(stderr, "[ERROR] cannot read seed file: %s\n", a.seed_file);
            return 2;
        }
        char line[8192];
        while (fgets(line, sizeof line, fp)) {
            char *trimmed = str_strip(line);
            if (*trimmed && trimmed[0] != '#') vec_push(&a.seeds, xstrdup(trimmed));
            free(trimmed);
        }
        fclose(fp);
    }
    if (!a.seeds.len) {
        fprintf(stderr, "[ERROR] provide at least one --seed or --seed-file\n");
        return 2;
    }

    if (!a.storage_sqlite) {
        size_t ol = strlen(a.output);
        if (ol < 5 || strcasecmp(a.output + ol - 5, ".json") != 0) {
            fprintf(stderr, "[WARN] --output '%s' does not end with '.json'. "
                            "The file will still contain a valid JSON array, but the "
                            "extension is misleading.\n", a.output);
        }
    }

    if (a.forever && a.has_max_pages) {
        fprintf(stderr, "[WARN] --forever overrides --max-pages\n");
    }
    int has_max_pages = !(a.forever || !a.has_max_pages);

    tui_t *tui = NULL;
    if (!a.no_tui && !a.debug) {
        tui = tui_new(a.refresh_rate, 0);
        if (!tui->enabled) {
            tui_free(tui);
            tui = NULL;
        }
    }

    char **events = NULL;
    int n_events = 0;
    if (a.ntfy_topic && *a.ntfy_topic) {
        events = xcalloc(64, sizeof(char *));
        char *copy = xstrdup(a.ntfy_events);
        char *save = copy;
        char *part;
        while ((part = strsep(&save, ",")) && n_events < 63) {
            char *t = str_strip(part);
            if (*t) events[n_events++] = xstrdup(t);
            free(t);
        }
        free(copy);
    }

    crawler_cfg cfg;
    crawler_cfg_init(&cfg);
    for (size_t i = 0; i < a.seeds.len; i++) vec_push(&cfg.seeds, xstrdup(a.seeds.items[i]));
    cfg.has_max_pages = has_max_pages;
    cfg.max_pages = a.max_pages;
    cfg.delay = a.delay;
    cfg.output_path = a.output;
    cfg.user_agent = a.user_agent;
    cfg.timeout = a.timeout;
    cfg.same_host_only = a.same_host;
    cfg.quiet = a.quiet;
    cfg.max_page_bytes = a.max_page_bytes;
    cfg.chunk_size = a.chunk_size;
    cfg.has_max_memory = a.has_max_memory;
    cfg.max_memory_mb = a.max_memory_mb;
    cfg.ignore_robots = a.ignore_robots;
    cfg.verbose_robots = a.verbose_robots;
    cfg.max_depth = a.max_depth;
    cfg.max_per_domain = a.max_per_domain;
    cfg.max_links_per_page = a.max_links_per_page;
    cfg.exclude_domains = (char **)a.exclude_domain.items;
    cfg.n_exclude_domains = (int)a.exclude_domain.len;
    cfg.exclude_patterns = (char **)a.exclude_url.items;
    cfg.n_exclude_patterns = (int)a.exclude_url.len;
    cfg.use_sitemaps = a.use_sitemaps;
    cfg.use_feeds = a.use_feeds;
    cfg.storage_sqlite = a.storage_sqlite;
    cfg.checkpoint_path = a.checkpoint;
    cfg.resume = a.resume;
    cfg.dry_run = a.dry_run;
    cfg.max_retries = a.max_retries;
    cfg.log_fmt = a.log_format;
    cfg.max_queue_size = a.max_queue_size;
    cfg.bloom_capacity = a.bloom_capacity;
    cfg.bloom_fp = a.bloom_fp;
    cfg.languages = (char **)a.languages.items;
    cfg.n_languages = (int)a.languages.len;
    cfg.respect_noindex = !a.no_respect_noindex;
    cfg.use_favicons = !a.no_favicons;
    cfg.clean_mode = a.clean;
    cfg.clean_window = a.clean_window;
    cfg.tui = tui;
    cfg.debug_log = a.debug ? a.debug_log : NULL;
    cfg.ntfy_topic = a.ntfy_topic;
    cfg.ntfy_events = events;
    cfg.n_ntfy_events = n_events;
    cfg.ntfy_progress_every = a.ntfy_progress_every;

    curl_global_init(CURL_GLOBAL_DEFAULT);

    crawler_t *crawler = crawler_new(&cfg);
    vec_free(&cfg.seeds, free);
    crawler_run(crawler);
    crawler_free(crawler);
    logger_t *log = NULL;
    (void)log;

    curl_global_cleanup();
    if (tui) tui_free(tui);
    for (int i = 0; i < n_events; i++) free(events[i]);
    free(events);
    cli_free(&a);
    return 0;
}
