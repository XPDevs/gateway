#include "crawler.h"

/* the GatewayCrawler itself: frontier, filtering, crawl loop */
/* ==========================================================================
 * Section 16 - GatewayCrawler
 * ========================================================================== */




crawler_t *g_crawler = NULL;

void crawler_on_signal(int signum)
{
    crawler_t *c = g_crawler;
    if (!c) return;
    if (c->stop) {
        /* second signal: force the exit path */
        signal(signum, SIG_DFL);
        raise(signum);
        return;
    }
    c->stop = 1;
    if (c->log) log_warn(c->log, "signal %d received; finishing current request", signum);
}

double *double_new(double v)
{
    double *d = xmalloc(sizeof(double));
    *d = v;
    return d;
}

void double_free(void *p) { free(p); }

void regex_vec_free(vec_t *v)
{
    for (size_t i = 0; i < v->len; i++) {
        regex_t *r = v->items[i];
        regfree(r);
        free(r);
    }
    vec_free(v, NULL);
}

int regex_search(regex_t *re, const char *s)
{
    return regexec(re, s, 0, NULL, 0) == 0;
}

void crawler_compile_regexes(vec_t *dst, char **patterns, int count)
{
    for (int i = 0; i < count; i++) {
        if (!patterns[i] || !*patterns[i]) continue;
        regex_t *re = xcalloc(1, sizeof(regex_t));
        if (regcomp(re, patterns[i], REG_EXTENDED | REG_NOSUB) != 0) {
            free(re);
            fprintf(stderr, "[WARN] invalid regex ignored: %s\n", patterns[i]);
            continue;
        }
        vec_push(dst, re);
    }
}

/* the media/asset filter from the Python source */
int url_has_media_extension(const char *url)
{
    static const char *EXT[] = {
        ".jpg", ".jpeg", ".png", ".gif", ".webp", ".pdf", ".zip", ".tar", ".gz",
        ".tgz", ".mp4", ".mp3", ".avi", ".mov", ".exe", ".dmg", ".iso", ".css",
        ".js", ".woff", ".woff2", ".ttf", ".eot", ".svg", NULL
    };
    size_t n = strlen(url);
    for (int i = 0; EXT[i]; i++) {
        size_t el = strlen(EXT[i]);
        for (size_t p = 0; p + el <= n; p++) {
            if (strncasecmp(url + p, EXT[i], el) != 0) continue;
            int start_ok = (p == 0) || (url[p - 1] == '.') || (url[p - 1] == '/') ||
                           (url[p - 1] == '-') || (url[p - 1] == '_') || 1;
            char next = url[p + el];
            if (start_ok && (next == '\0' || next == '?')) return 1;
        }
    }
    return 0;
}

void crawler_notify(crawler_t *c, const char *event, const char *message,
                           const char *title, int priority, const char *tags,
                           const char *click)
{
    if (!c->notifier) return;
    if (!map_has(&c->ntfy_events, event)) return;
    ntfy_send(c->notifier, message, title, priority, tags, click);
}

/* -- polite wait ----------------------------------------------------------- */
int crawler_tui_pump(crawler_t *c);

void crawler_polite_wait(crawler_t *c, const char *url)
{
    char *host = get_domain(url);
    double delay = c->delay;
    double cd = robots_cache_crawl_delay(&c->robots, url);
    if (cd >= 0 && cd > delay) delay = cd;
    double *last = lru_get(&c->last_request, host);
    double now = now_sec();
    double elapsed = now - (last ? *last : 0.0);
    double remaining = delay - elapsed;
    if (remaining > 0) {
        double deadline = now_sec() + remaining;
        while (now_sec() < deadline) {
            if (c->stop) { free(host); return; }
            if (c->tui && tui_enabled(c->tui)) {
                if (crawler_tui_pump(c)) { free(host); return; }
            }
            sleep_ms(100);
        }
    }
    lru_put(&c->last_request, host, double_new(now_sec()));
    free(host);
}

int crawler_url_excluded(crawler_t *c, const char *url)
{
    char *d = get_domain(url);
    int excluded = 0;
    for (size_t i = 0; i < c->exclude_domains.len && !excluded; i++)
        if (regex_search(c->exclude_domains.items[i], d)) excluded = 1;
    free(d);
    for (size_t i = 0; i < c->exclude_patterns.len && !excluded; i++)
        if (regex_search(c->exclude_patterns.items[i], url)) excluded = 1;
    return excluded;
}

int crawler_domain_capped(crawler_t *c, const char *url)
{
    char *d = get_domain(url);
    long n = (long)(intptr_t)map_get(&c->domain_pages, d);
    free(d);
    return n >= c->max_per_domain;
}

/* -- JSON-LD key extraction (jsonld_extract) ------------------------------- */
const char *JSONLD_KEYS[] = {
    "@type", "author", "headline", "datePublished", "dateModified",
    "publisher", "description", NULL
};

int jsonld_key_wanted(const char *k)
{
    for (int i = 0; JSONLD_KEYS[i]; i++) if (!strcmp(JSONLD_KEYS[i], k)) return 1;
    return 0;
}

char *json_scalar_to_str(const json_val *v)
{
    if (!v) return NULL;
    switch (v->t) {
    case JSON_STR: return xstrdup(v->str);
    case JSON_NUM: return num_to_pystr(v->num, v->is_int, v->ival);
    case JSON_BOOL: return xstrdup(v->bval ? "True" : "False");
    default: return NULL;
    }
}

void jsonld_extract(vec_t *raws, map_t *found)
{
    if (!raws->len) return;
    vec_t roots;
    vec_init(&roots);
    for (size_t i = 0; i < raws->len; i++) {
        json_val *v = json_parse(raws->items[i]);
        if (v) vec_push(&roots, v);
    }
    if (!roots.len) { vec_free(&roots, NULL); return; }

    vec_t stack;
    vec_init(&stack);
    for (size_t i = 0; i < roots.len; i++) vec_push(&stack, roots.items[i]);
    while (stack.len) {
        json_val *o = vec_pop(&stack);
        if (!o) continue;
        if (o->t == JSON_OBJ) {
            for (size_t i = 0; i < o->n; i++) {
                const char *k = o->keys[i];
                if (jsonld_key_wanted(k) && !map_has(found, k)) {
                    char *val = json_scalar_to_str(o->items[i]);
                    if (val) map_put(found, k, val);
                }
            }
            for (size_t i = 0; i < o->n; i++) {
                json_val *v = o->items[i];
                if (v && (v->t == JSON_OBJ || v->t == JSON_ARR)) vec_push(&stack, v);
            }
        } else if (o->t == JSON_ARR) {
            for (size_t i = 0; i < o->n; i++) vec_push(&stack, o->items[i]);
        }
    }
    vec_free(&stack, NULL);
    for (size_t i = 0; i < roots.len; i++) json_free(roots.items[i]);
    vec_free(&roots, NULL);
}

/* -- result construction --------------------------------------------------- */
/* -- favicon discovery -----------------------------------------------------
 * One favicon per site: the first page seen for a domain resolves the icon
 * (declared <link rel="icon"> first, else the conventional /favicon.ico) and
 * every later page on that domain reuses the cached value without redoing
 * the work. Absolute URL, so it can be rendered straight from the record. */
int is_http_url(const char *u)
{
    return u && (!strncmp(u, "http://", 7) || !strncmp(u, "https://", 8));
}

char *site_origin(const char *url)
{
    urlparts u;
    url_parse_parts(url ? url : "", &u);
    sb_t b;
    sb_init(&b);
    if (u.scheme && *u.scheme && u.netloc && *u.netloc)
        sb_appendf(&b, "%s://%s", u.scheme, u.netloc);
    urlparts_free(&u);
    return sb_detach(&b);
}

char *crawler_favicon_for(crawler_t *c, const char *page_url, const char *declared)
{
    if (!c || !c->use_favicons) return NULL;
    char *dom = get_domain(page_url);
    if (!*dom) { free(dom); return NULL; }

    char *cached = map_get(&c->favicon_cache, dom);
    if (cached) {
        free(dom);
        return *cached ? xstrdup(cached) : NULL;
    }

    char *chosen = NULL;
    if (declared && *declared) {
        char *resolved = normalize_url_base(declared, page_url);
        if (is_http_url(resolved)) chosen = resolved;
        else free(resolved);
    }
    if (!chosen) {
        char *origin = site_origin(page_url);
        if (is_http_url(origin)) {
            sb_t b;
            sb_init(&b);
            sb_appendf(&b, "%s/favicon.ico", origin);
            chosen = sb_detach(&b);
        }
        free(origin);
    }
    if (chosen && *chosen) {
        map_put(&c->favicon_cache, dom, xstrdup(chosen));
        c->favicons_found++;
        log_debug(c->log, "  favicon: %s -> %s", dom, chosen);
    } else {
        free(chosen);
        chosen = NULL;
    }
    free(dom);
    return chosen;
}

void build_result(crawler_t *c, const char *url, fetch_result *res,
                         record_t *rec, vec_t **links_out, char **robots_meta_out)
{
    (void)c;
    meta_parser *meta = res->meta;
    page_content *content = &res->content;
    char *final_url = xstrdup((res->final_url && *res->final_url) ? res->final_url : url);

    if (meta->canonical && *meta->canonical) {
        char *canon = normalize_url_base(meta->canonical, final_url);
        if (canon) {
            free(final_url);
            final_url = canon;
        }
    }

    char *description = pick_description(meta, &content->paragraphs);

    char *title = collapse_ws_trim(meta->title.buf ? meta->title.buf : "");
    if (!*title) {
        free(title);
        title = xstrdup(final_url);
    }
    if (utf8_len(title) > 200) {
        char *cut = trunc_chars(title, 197);
        char *with_dots = xmalloc(strlen(cut) + 4);
        strcpy(with_dots, cut);
        strcat(with_dots, "...");
        free(cut);
        free(title);
        title = with_dots;
    }

    char *lang = str_lower((meta->lang && *meta->lang) ? meta->lang : res->language);
    if (!*lang) {
        free(lang);
        lang = detect_language_from_text(content->text, LANG_SAMPLE);
    }

    map_t ld;
    map_init(&ld);
    jsonld_extract(&meta->jsonld, &ld);

    const char *ld_author = map_get(&ld, "author");
    const char *ld_pub = map_get(&ld, "datePublished");
    const char *ld_mod = map_get(&ld, "dateModified");

    record_init(rec);
    rec->id = stable_id(final_url);
    rec->t = title;
    rec->u = xstrdup(final_url);
    rec->d = description;
    rec->s = get_domain(final_url);
    rec->status = res->status;
    rec->content_type = xstrdup(res->content_type ? res->content_type : "");
    rec->lang = lang;
    rec->author = xstrdup((meta->author && *meta->author) ? meta->author : (ld_author ? ld_author : ""));
    rec->pub_date = xstrdup((meta->pub_date && *meta->pub_date) ? meta->pub_date : (ld_pub ? ld_pub : ""));
    rec->mod_date = xstrdup((meta->mod_date && *meta->mod_date) ? meta->mod_date : (ld_mod ? ld_mod : ""));
    rec->canonical = xstrdup(final_url);
    rec->word_count = content->word_count;
    rec->reading_minutes = content->reading_minutes;
    rec->content_hash = (content->text && *content->text) ? content_hash(content->text) : xstrdup("");
    rec->sig = (content->text && *content->text) ? content_sig(content->text) : xstrdup("");

    if (links_out) *links_out = &meta->links;
    if (robots_meta_out) *robots_meta_out = meta->robots_meta.buf ? meta->robots_meta.buf : "";

    /* values copied out of ld, so the map can go */
    map_free(&ld, free);
    free(final_url);
}

/* -- --clean: decide which copy of a duplicate gets written ---------------- */

void crawler_clean_push(crawler_t *c, record_t *rec)
{
    char *key = record_dedupe_key(rec);

    if (map_has(&c->clean_written, key)) {
        /* a copy of this content is already in the output: this one is discarded */
        c->clean_duplicates++;
        log_debug(c->log, "clean: duplicate discarded (already written): %s", rec->u);
        free(key);
        return;
    }

    void *slot = map_get(&c->clean_pending, key);
    if (slot) {
        size_t idx = (size_t)(intptr_t)slot - 1;
        record_t *held = (record_t *)c->clean_stage.items[idx];
        if (record_quality_cmp(rec, held) < 0) {
            /* the newcomer is the better record: it takes the slot */
            char *old_url = xstrdup(held->u ? held->u : "?");
            record_free(held);
            free(held);
            c->clean_stage.items[idx] = record_clone(rec);
            c->clean_replaced++;
            log_debug(c->log, "clean: better copy wins, replacing %s with %s",
                      old_url, rec->u);
            free(old_url);
        } else {
            log_debug(c->log, "clean: duplicate discarded (weaker copy): %s", rec->u);
        }
        c->clean_duplicates++;
        free(key);
        return;
    }

    record_t *copy = record_clone(rec);
    vec_push(&c->clean_stage, copy);
    map_incr(&c->clean_pending, key, (long)c->clean_stage.len); /* index + 1 */
    free(key);
    c->page_count++;

    if (c->clean_window > 0 && (long)c->clean_stage.len >= c->clean_window)
        crawler_clean_flush(c);
}

void crawler_clean_flush(crawler_t *c)
{
    if (!c->clean_stage.len) return;
    for (size_t i = 0; i < c->clean_stage.len; i++) {
        record_t *r = (record_t *)c->clean_stage.items[i];
        sb_t json;
        sb_init(&json);
        record_to_json(&json, r);
        c->writer->write(c->writer, json.buf);
        sb_free(&json);
        char *key = record_dedupe_key(r);
        map_incr(&c->clean_written, key, 1);
        free(key);
        c->clean_kept++;
        record_free(r);
        free(r);
    }
    c->clean_stage.len = 0;
    map_free(&c->clean_pending, NULL);
    map_init(&c->clean_pending);
    c->clean_flushes++;
}

void crawler_clean_free(crawler_t *c)
{
    for (size_t i = 0; i < c->clean_stage.len; i++) {
        record_t *r = (record_t *)c->clean_stage.items[i];
        record_free(r);
        free(r);
    }
    vec_free(&c->clean_stage, NULL);
    map_free(&c->clean_pending, NULL);
    map_free(&c->clean_written, NULL);
    c->clean_stage.items = NULL;
    c->clean_stage.len = c->clean_stage.cap = 0;
}

void crawler_write(crawler_t *c, record_t *rec)
{
    if (c->dry_run) return;
    if (c->clean_mode) {
        crawler_clean_push(c, rec);
        return;
    }
    sb_t json;
    sb_init(&json);
    record_to_json(&json, rec);
    c->writer->write(c->writer, json.buf);
    sb_free(&json);
    c->page_count++;
}

void crawler_enqueue(crawler_t *c, const char *url, int depth, const char *base_url)
{
    if (bloom_contains(&c->seen, url)) return;
    if (crawler_url_excluded(c, url)) { c->filtered++; return; }
    if (c->same_host_only && base_url && *base_url && !is_same_host(base_url, url)) return;
    if (depth > c->max_depth) return;
    if (crawler_domain_capped(c, url)) return;
    if (url_has_media_extension(url)) return;
    if (frontier_push(&c->frontier, url, depth, base_url)) {
        bloom_add(&c->seen, url);
        log_debug(c->log, "queued link: depth=%d, url=%s", depth, url);
    }
}

/* -- sitemaps and feeds ---------------------------------------------------- */
void crawler_ingest_sitemap(crawler_t *c, const char *url, int depth);

void crawler_discover_sitemaps(crawler_t *c)
{
    if (!c->use_sitemaps) return;
    map_t visited;
    map_init(&visited);
    for (size_t i = 0; i < c->seeds.len; i++) {
        const char *seed = c->seeds.items[i];
        char *domain = get_domain(seed);
        if (map_has(&visited, domain)) { free(domain); continue; }
        map_put(&visited, domain, (void *)1);
        free(domain);
        urlparts u;
        url_parse_parts(seed, &u);
        sb_t base;
        sb_init(&base);
        sb_appendf(&base, "%s://%s", u.scheme, u.netloc);
        urlparts_free(&u);
        const char *paths[] = { "/sitemap.xml", "/sitemap_index.xml", "/sitemap.txt", NULL };
        for (int k = 0; paths[k]; k++) {
            sb_t full;
            sb_init(&full);
            sb_append(&full, base.buf);
            sb_append(&full, paths[k]);
            crawler_ingest_sitemap(c, full.buf, 0);
            sb_free(&full);
        }
        sb_free(&base);
    }
    map_free(&visited, NULL);
}

int seeds_same_host(vec_t *seeds, const char *url)
{
    for (size_t i = 0; i < seeds->len; i++)
        if (is_same_host(seeds->items[i], url)) return 1;
    return 0;
}

void crawler_ingest_sitemap(crawler_t *c, const char *url, int depth)
{
    if (!c->ignore_robots && !robots_cache_can_fetch(&c->robots, url)) {
        c->robots_blocks++;
        log_debug(c->log, "sitemap blocked by robots.txt: %s", url);
        return;
    }
    log_info(c->log, "sitemap: %s", url);
    char *reason = NULL;
    if (!is_safe_to_fetch(url, &reason)) { free(reason); return; }
    free(reason);

    http_opts o;
    memset(&o, 0, sizeof o);
    o.ua = c->user_agent;
    o.timeout = c->timeout;
    o.limit = 2000000;
    o.method = "GET";
    http_response resp;
    http_response_init(&resp);
    if (!http_perform(url, &o, &resp) || resp.status >= 400 || !resp.body) {
        log_debug(c->log, "sitemap fetch failed: %s", resp.error ? resp.error : "no body");
        http_response_free(&resp);
        return;
    }
    const char *data = (const char *)resp.body;
    size_t len = resp.body_len;
    int has_marker = memmem(data, len, "<sitemapindex", 13) != NULL ||
                     memmem(data, len, "<urlset", 7) != NULL;

    if (has_marker) {
        vec_t entries;
        vec_init(&entries);
        sitemap_collect(data, len, &entries, 2, depth);
        for (size_t i = 0; i < entries.len; i++) {
            sitemap_entry *e = entries.items[i];
            if (!strcmp(e->kind, "index")) {
                if (depth < 2) crawler_ingest_sitemap(c, e->url, depth + 1);
            } else {
                char *norm = normalize_url(e->url);
                if (norm && !bloom_contains(&c->seen, norm)) {
                    if (c->same_host_only && !seeds_same_host(&c->seeds, norm)) {
                        free(norm);
                    } else {
                        bloom_add(&c->seen, norm);
                        frontier_push(&c->frontier, norm, 0, NULL);
                        free(norm);
                    }
                } else {
                    free(norm);
                }
            }
            free(e->kind);
            free(e->url);
            free(e);
        }
        vec_free(&entries, NULL);
    } else {
        size_t probe = len < 50 ? len : 50;
        size_t skip = 0;
        while (skip < probe && is_space_byte(data[skip])) skip++;
        if (probe - skip >= 4 && !strncmp(data + skip, "http", 4)) {
            /* text sitemap: one URL per line */
            char *text = xstrndup(data, len);
            char *save = text;
            char *line;
            while ((line = strsep(&save, "\n"))) {
                char *trimmed = str_strip(line);
                if (*trimmed && (!strncmp(trimmed, "http://", 7) || !strncmp(trimmed, "https://", 8))) {
                    char *norm = normalize_url(trimmed);
                    if (norm && !bloom_contains(&c->seen, norm)) {
                        if (c->same_host_only && !seeds_same_host(&c->seeds, norm)) {
                            free(norm);
                        } else {
                            bloom_add(&c->seen, norm);
                            frontier_push(&c->frontier, norm, 0, NULL);
                            free(norm);
                        }
                    } else {
                        free(norm);
                    }
                }
                free(trimmed);
            }
            free(text);
        }
    }
    http_response_free(&resp);
}

void crawler_discover_feeds(crawler_t *c)
{
    if (!c->use_feeds) return;
    map_t seen;
    map_init(&seen);
    for (size_t i = 0; i < c->seeds.len; i++) {
        const char *seed = c->seeds.items[i];
        urlparts u;
        url_parse_parts(seed, &u);
        sb_t base;
        sb_init(&base);
        sb_appendf(&base, "%s://%s", u.scheme, u.netloc);
        urlparts_free(&u);
        if (map_has(&seen, base.buf)) { sb_free(&base); continue; }
        map_put(&seen, base.buf, (void *)1);

        const char *paths[] = { "/feed", "/rss", "/rss.xml", "/atom.xml", "/feed.xml", NULL };
        for (int k = 0; paths[k]; k++) {
            sb_t full;
            sb_init(&full);
            sb_append(&full, base.buf);
            sb_append(&full, paths[k]);
            const char *url = full.buf;
            if (!c->ignore_robots && !robots_cache_can_fetch(&c->robots, url)) {
                c->robots_blocks++;
                sb_free(&full);
                continue;
            }
            char *reason = NULL;
            if (!is_safe_to_fetch(url, &reason)) { free(reason); sb_free(&full); continue; }
            free(reason);

            http_opts o;
            memset(&o, 0, sizeof o);
            o.ua = c->user_agent;
            o.timeout = c->timeout;
            o.limit = 2000000;
            o.method = "GET";
            http_response resp;
            http_response_init(&resp);
            if (!http_perform(url, &o, &resp) || resp.status >= 400 || !resp.body) {
                http_response_free(&resp);
                sb_free(&full);
                continue;
            }
            vec_t items;
            vec_init(&items);
            feed_collect((const char *)resp.body, resp.body_len, &items);
            if (items.len) {
                log_info(c->log, "feed: %s (%zu items)", url, items.len);
                for (size_t j = 0; j < items.len; j++) {
                    feed_item *fi = items.items[j];
                    char *norm = normalize_url(fi->link);
                    if (norm && !bloom_contains(&c->seen, norm)) {
                        if (c->same_host_only && !seeds_same_host(&c->seeds, norm)) {
                            free(norm);
                        } else {
                            bloom_add(&c->seen, norm);
                            frontier_push(&c->frontier, norm, 0, NULL);
                            free(norm);
                        }
                    } else {
                        free(norm);
                    }
                    free(fi->title);
                    free(fi->link);
                    free(fi);
                }
            }
            vec_free(&items, NULL);
            http_response_free(&resp);
            sb_free(&full);
        }
        sb_free(&base);
    }
    map_free(&seen, NULL);
}

int crawler_disk_ok(crawler_t *c)
{
    char *path = xstrdup(c->output_path);
    char *slash = strrchr(path, '/');
    if (slash) *slash = '\0'; else { free(path); path = xstrdup("."); }
    struct statvfs sv;
    int ok = 1;
    if (statvfs(path, &sv) == 0) {
        unsigned long long freeb = (unsigned long long)sv.f_bavail * sv.f_frsize;
        ok = freeb > 50ULL * 1024ULL * 1024ULL;
    }
    free(path);
    return ok;
}

int crawler_validate_output(crawler_t *c, char **reason)
{
    if (access(c->output_path, F_OK) != 0) {
        *reason = xstrdup("file does not exist");
        return 0;
    }
    struct stat st;
    if (stat(c->output_path, &st) != 0) {
        *reason = xstrdup("cannot stat output file");
        return 0;
    }
    if (st.st_size < 2) {
        sb_t b; sb_init(&b);
        sb_appendf(&b, "file too small (%lld bytes)", (long long)st.st_size);
        *reason = sb_detach(&b);
        return 0;
    }
    FILE *fp = fopen(c->output_path, "rb");
    if (!fp) { *reason = xstrdup("cannot open output file"); return 0; }
    int first = fgetc(fp);
    if (first != '[') {
        sb_t b; sb_init(&b);
        sb_appendf(&b, "first byte is %d, expected '['", first);
        *reason = sb_detach(&b);
        fclose(fp);
        return 0;
    }
    long off = st.st_size > 128 ? st.st_size - 128 : 0;
    fseek(fp, off, SEEK_SET);
    char tail[256];
    size_t n = fread(tail, 1, sizeof tail - 1, fp);
    fclose(fp);
    tail[n] = '\0';
    size_t end = n;
    while (end > 0 && is_space_byte(tail[end - 1])) end--;
    if (end == 0 || tail[end - 1] != ']') {
        sb_t b; sb_init(&b);
        sb_appendf(&b, "last non-whitespace byte is '%c', expected ']'",
                   end ? tail[end - 1] : '?');
        *reason = sb_detach(&b);
        return 0;
    }
    *reason = xstrdup("ok");
    return 1;
}

/* -- construction ---------------------------------------------------------- */

void crawler_cfg_init(crawler_cfg *cfg)
{
    memset(cfg, 0, sizeof *cfg);
    vec_init(&cfg->seeds);
    cfg->delay = DEFAULT_DELAY;
    cfg->output_path = (char *)DEFAULT_OUTPUT;
    cfg->user_agent = (char *)DEFAULT_USER_AGENT;
    cfg->timeout = DEFAULT_TIMEOUT;
    cfg->max_page_bytes = DEFAULT_PAGE_BYTES;
    cfg->chunk_size = DEFAULT_CHUNK;
    cfg->max_depth = DEFAULT_MAX_DEPTH;
    cfg->max_per_domain = DEFAULT_MAX_PER_DOM;
    cfg->max_links_per_page = DEFAULT_MAX_LINKS;
    cfg->checkpoint_path = (char *)DEFAULT_CHECKPOINT;
    cfg->max_retries = DEFAULT_MAX_RETRIES;
    cfg->log_fmt = (char *)"text";
    cfg->respect_noindex = 1;
    cfg->use_favicons = 1;
    cfg->max_queue_size = DEFAULT_MAX_QUEUE;
    cfg->bloom_capacity = DEFAULT_BLOOM_CAP;
    cfg->bloom_fp = DEFAULT_BLOOM_FP;
}

void crawler_restore(crawler_t *c)
{
    json_val *cp = checkpoint_load(c->checkpoint_path);
    if (!cp) {
        log_info(c->log, "no checkpoint found; starting fresh");
        return;
    }
    json_val *bloom_v = json_obj_get(cp, "bloom");
    json_val *frontier_v = json_obj_get(cp, "frontier");
    json_val *pc = json_obj_get(cp, "page_count");
    json_val *stats_v = json_obj_get(cp, "stats");
    json_val *dp = json_obj_get(cp, "domain_pages");

    if (bloom_v && bloom_v->t == JSON_OBJ) {
        json_val *nb = json_obj_get(bloom_v, "num_bits");
        json_val *nh = json_obj_get(bloom_v, "num_hashes");
        json_val *cap = json_obj_get(bloom_v, "capacity");
        json_val *er = json_obj_get(bloom_v, "error_rate");
        json_val *b64 = json_obj_get(bloom_v, "bits_b64");
        json_val *add = json_obj_get(bloom_v, "added");
        const char *bits = json_as_str(b64);
        if (nb && nh && cap && er && bits) {
            bloom_free(&c->seen);
            memset(&c->seen, 0, sizeof(bloom_t));
            c->seen.num_bits = (uint64_t)nb->num;
            c->seen.num_hashes = (int)nh->num;
            c->seen.capacity = (uint64_t)cap->num;
            c->seen.error_rate = er->num;
            c->seen.added = add ? (uint64_t)add->num : 0;
            size_t blen = 0;
            unsigned char *raw = base64_decode(bits, &blen);
            c->seen.bits = xcalloc((size_t)(c->seen.num_bits / 8) + 1, 1);
            size_t copy = blen < (size_t)(c->seen.num_bits / 8) ? blen : (size_t)(c->seen.num_bits / 8);
            memcpy(c->seen.bits, raw, copy);
            free(raw);
        }
    }

    frontier_free(&c->frontier);
    frontier_init(&c->frontier, c->frontier.maxsize ? c->frontier.maxsize : DEFAULT_MAX_QUEUE);
    if (frontier_v && frontier_v->t == JSON_ARR) {
        for (size_t i = 0; i < frontier_v->n; i++) {
            json_val *entry = frontier_v->items[i];
            if (!entry || entry->t != JSON_ARR || entry->n < 2) continue;
            const char *url = json_as_str(entry->items[0]);
            int depth = (int)entry->items[1]->num;
            const char *base = entry->n > 2 ? json_as_str(entry->items[2]) : NULL;
            if (url) frontier_push(&c->frontier, url, depth, base);
        }
    }
    if (pc) c->page_count = (long)pc->num;
    if (dp && dp->t == JSON_OBJ) {
        for (size_t i = 0; i < dp->n; i++)
            map_incr(&c->domain_pages, dp->keys[i], (long)dp->items[i]->num);
    }
    if (stats_v) {
        json_val *v;
        if ((v = json_obj_get(stats_v, "status")) && v->t == JSON_OBJ)
            for (size_t i = 0; i < v->n; i++) map_incr(&c->stats.status, v->keys[i], (long)v->items[i]->num);
        if ((v = json_obj_get(stats_v, "domains")) && v->t == JSON_OBJ)
            for (size_t i = 0; i < v->n; i++) map_incr(&c->stats.domains, v->keys[i], (long)v->items[i]->num);
        if ((v = json_obj_get(stats_v, "languages")) && v->t == JSON_OBJ)
            for (size_t i = 0; i < v->n; i++) map_incr(&c->stats.languages, v->keys[i], (long)v->items[i]->num);
        if ((v = json_obj_get(stats_v, "types")) && v->t == JSON_OBJ)
            for (size_t i = 0; i < v->n; i++) map_incr(&c->stats.types, v->keys[i], (long)v->items[i]->num);
        if ((v = json_obj_get(stats_v, "errors")) && v->t == JSON_OBJ)
            for (size_t i = 0; i < v->n; i++) map_incr(&c->stats.errors, v->keys[i], (long)v->items[i]->num);
        if ((v = json_obj_get(stats_v, "total_bytes")) && v->t == JSON_NUM)
            c->stats.total_bytes = (long long)v->num;
        if ((v = json_obj_get(stats_v, "failed_urls")) && v->t == JSON_ARR) {
            for (size_t i = 0; i < v->n && i < 500; i++) {
                json_val *fu = v->items[i];
                if (!fu || fu->t != JSON_OBJ) continue;
                failed_url_t *f = xcalloc(1, sizeof(failed_url_t));
                const char *u = json_as_str(json_obj_get(fu, "url"));
                const char *rs = json_as_str(json_obj_get(fu, "reason"));
                f->url = xstrdup(u ? u : "");
                f->reason = xstrdup(rs ? rs : "");
                vec_push(&c->stats.failed_urls, f);
            }
        }
    }
    log_info(c->log, "resumed from checkpoint: %ld pages, %zu URLs in frontier",
             c->page_count, c->frontier.len);
    json_free(cp);
}

crawler_t *crawler_new(crawler_cfg *cfg)
{
    crawler_t *c = xcalloc(1, sizeof(crawler_t));
    vec_init(&c->seeds);
    for (size_t i = 0; i < cfg->seeds.len; i++) {
        char *norm = normalize_url(cfg->seeds.items[i]);
        if (norm && *norm) vec_push(&c->seeds, norm);
        else free(norm);
    }
    if (!c->seeds.len) {
        fprintf(stderr, "[ERROR] No valid seed URLs provided.\n");
        exit(2);
    }
    c->has_max_pages = cfg->has_max_pages;
    c->max_pages = cfg->max_pages;
    c->delay = cfg->delay;
    c->output_path = xstrdup(cfg->output_path);
    c->user_agent = xstrdup(cfg->user_agent);
    c->timeout = cfg->timeout;
    c->same_host_only = cfg->same_host_only;
    c->max_page_bytes = cfg->max_page_bytes;
    c->chunk_size = cfg->chunk_size;
    c->has_max_memory = cfg->has_max_memory;
    c->max_memory_mb = cfg->max_memory_mb;
    c->ignore_robots = cfg->ignore_robots;
    c->verbose_robots = cfg->verbose_robots;
    c->max_depth = cfg->max_depth;
    c->max_per_domain = cfg->max_per_domain;
    c->max_links_per_page = cfg->max_links_per_page;
    vec_init(&c->exclude_domains);
    vec_init(&c->exclude_patterns);
    crawler_compile_regexes(&c->exclude_domains, cfg->exclude_domains, cfg->n_exclude_domains);
    crawler_compile_regexes(&c->exclude_patterns, cfg->exclude_patterns, cfg->n_exclude_patterns);
    c->use_sitemaps = cfg->use_sitemaps;
    c->use_feeds = cfg->use_feeds;
    c->storage_sqlite = cfg->storage_sqlite;
    c->checkpoint_path = xstrdup(cfg->checkpoint_path);
    c->resume = cfg->resume;
    c->dry_run = cfg->dry_run;
    c->respect_noindex = cfg->respect_noindex;
    c->use_favicons = cfg->use_favicons;
    c->clean_mode = cfg->clean_mode;
    c->clean_window = (cfg->clean_window > 0) ? cfg->clean_window : CLEAN_DEFAULT_WINDOW;
    if (c->clean_mode) {
        vec_init(&c->clean_stage);
        map_init(&c->clean_pending);
        map_init(&c->clean_written);
    }
    c->tui = cfg->tui;
    map_init(&c->language_filter);
    for (int i = 0; i < cfg->n_languages; i++) {
        char *low = str_lower(cfg->languages[i]);
        map_put(&c->language_filter, low, (void *)1);
        free(low);
    }
    map_init(&c->ntfy_events);
    for (int i = 0; i < cfg->n_ntfy_events; i++) {
        char *low = str_lower(cfg->ntfy_events[i]);
        char *trimmed = str_strip_inplace(low);
        if (*trimmed) map_put(&c->ntfy_events, trimmed, (void *)1);
        free(trimmed);
    }
    c->ntfy_progress_every = cfg->ntfy_progress_every;

    logger_t *log = xcalloc(1, sizeof(logger_t));
    logger_init(log, 1, cfg->quiet, cfg->log_fmt, cfg->tui, cfg->debug_log);
    c->log = log;
    c->owns_log = 1;
    if (cfg->debug_log && *cfg->debug_log)
        log_info(c->log, "Detailed log file: %s", cfg->debug_log);

    if (cfg->ntfy_topic && *cfg->ntfy_topic) {
        c->notifier = ntfy_new(cfg->ntfy_topic, c->log);
    }

    fetcher_init(&c->fetcher, cfg->user_agent, cfg->timeout, cfg->max_retries,
                 cfg->max_page_bytes, cfg->chunk_size, c->log);
    robots_cache_init(&c->robots, cfg->user_agent, cfg->timeout, DEFAULT_ROBOTS_CAP);
    lru_init(&c->last_request, DEFAULT_RATELIM_CAP, double_free);
    map_init(&c->domain_pages);
    lru_init(&c->robots_explained, 500, NULL);

    bloom_init(&c->seen, cfg->bloom_capacity, cfg->bloom_fp);
    bloom_init(&c->seen_content, cfg->bloom_capacity, cfg->bloom_fp);
    frontier_init(&c->frontier, cfg->max_queue_size);

    for (size_t i = 0; i < c->seeds.len; i++) {
        const char *s = c->seeds.items[i];
        if (!bloom_contains(&c->seen, s)) {
            bloom_add(&c->seen, s);
            frontier_push(&c->frontier, s, 0, NULL);
        }
    }

    stats_init(&c->stats);
    map_init(&c->favicon_cache);
    c->stop_reason = xstrdup("unknown");

    if (c->dry_run) c->writer = null_writer_new();
    else if (c->storage_sqlite) {
#ifdef HAVE_SQLITE3
        c->writer = sqlite_writer_new(c->output_path);
#else
        fprintf(stderr, "[ERROR] SQLite storage requested but this build has no SQLite support.\n");
        exit(2);
#endif
    } else {
        c->writer = json_writer_new(c->output_path);
    }

    g_crawler = c;
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = crawler_on_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    if (c->resume) crawler_restore(c);
    return c;
}

void crawler_free(crawler_t *c)
{
    if (!c) return;
    vec_free(&c->seeds, free);
    free(c->output_path);
    free(c->user_agent);
    free(c->checkpoint_path);
    regex_vec_free(&c->exclude_domains);
    regex_vec_free(&c->exclude_patterns);
    map_free(&c->language_filter, NULL);
    map_free(&c->ntfy_events, NULL);
    fetcher_free(&c->fetcher);
    robots_cache_free(&c->robots);
    lru_free(&c->last_request);
    map_free(&c->domain_pages, NULL);
    map_free(&c->favicon_cache, free);
    lru_free(&c->robots_explained);
    bloom_free(&c->seen);
    bloom_free(&c->seen_content);
    frontier_free(&c->frontier);
    if (c->clean_mode) crawler_clean_free(c);
    stats_free(&c->stats);
    ntfy_free(c->notifier);
    if (c->writer) {
        if (c->writer->close) c->writer->close(c->writer);
        c->writer = NULL;
    }
    free(c->stop_reason);
    if (c->owns_log) {
        logger_close(c->log);
        free(c->log);
    }
    free(c);
}

/* -- TUI glue -------------------------------------------------------------- */
void crawler_get_state(crawler_t *c, crawl_state_t *st, const char *status)
{
    memset(st, 0, sizeof *st);
    double elapsed = now_sec() - c->stats.start;
    if (status) st->status = status;
    else if (c->stop) st->status = "STOPPED";
    else if (c->paused) st->status = "PAUSED";
    else st->status = "RUNNING";
    st->pages = c->page_count;
    st->max_pages = c->has_max_pages ? c->max_pages : 0;
    st->queue = (long)c->frontier.len;
    st->seen = (long long)c->seen.added;
    st->rate = elapsed > 0 ? (double)c->page_count / elapsed * 60.0 : 0.0;
    st->rss = get_current_rss_mb();
    if (st->rss < 0) st->rss = 0;
    st->bloom = bloom_saturation(&c->seen);
    st->errors = c->network_errors;
    st->robots = c->robots_blocks;
    st->dupes = c->duplicate_content;
    st->elapsed = elapsed;
    st->status_breakdown = &c->stats.status;
    st->content_types = &c->stats.types;
    st->languages = &c->stats.languages;
    st->top_domains = &c->stats.domains;
}

void crawler_tui_render(crawler_t *c, const char *status)
{
    if (!c->tui) return;
    crawl_state_t st;
    crawler_get_state(c, &st, status);
    tui_set_state(c->tui, &st);
    tui_render(c->tui);
}

int crawler_tui_pump(crawler_t *c)
{
    if (!c->tui) return 0;
    int key = tui_poll_key(c->tui);
    if (key < 0) return 0;
    switch (key) {
    case 'q':
        free(c->stop_reason);
        c->stop_reason = xstrdup("user-quit");
        c->stop = 1;
        return 1;
    case 'p':
        c->paused = !c->paused;
        crawler_tui_render(c, NULL);
        break;
    case 'i':
        tui_toggle_info(c->tui);
        crawler_tui_render(c, NULL);
        break;
    case 'c':
        tui_clear_log(c->tui);
        crawler_tui_render(c, NULL);
        break;
    default:
        break;
    }
    return 0;
}

/* -- checkpoint ------------------------------------------------------------ */
void crawler_checkpoint(crawler_t *c)
{
    if (c->clean_mode) crawler_clean_flush(c);
    checkpoint_save(c->checkpoint_path, &c->seen, &c->frontier, c->page_count,
                    &c->stats, &c->seeds, &c->domain_pages, c->log);
}

/* -- noindex token handling ------------------------------------------------ */
int robots_tokens_have(const char *a, const char *b, const char *token)
{
    sb_t combined;
    sb_init(&combined);
    sb_append(&combined, a ? a : "");
    sb_putc(&combined, ' ');
    sb_append(&combined, b ? b : "");
    char *low = str_lower(combined.buf);
    sb_free(&combined);
    int found = 0;
    char *save = low;
    char *part;
    while ((part = strsep(&save, ",; \t\n\r\f\v"))) {
        char *t = str_strip(part);
        if (*t && !strcmp(t, token)) { found = 1; free(t); break; }
        free(t);
    }
    free(low);
    return found;
}

/* -- python-ish repr helpers for the report -------------------------------- */
char *py_dict_repr(map_t *m, size_t limit)
{
    size_t n = 0;
    kv_t *arr = map_top(m, &n);
    if (limit && n > limit) n = limit;
    sb_t b;
    sb_init(&b);
    sb_putc(&b, '{');
    for (size_t i = 0; i < n; i++) {
        if (i) sb_append(&b, ", ");
        sb_putc(&b, '\'');
        sb_append(&b, arr[i].key);
        sb_append(&b, "': ");
        sb_appendf(&b, "%ld", arr[i].count);
    }
    sb_putc(&b, '}');
    free(arr);
    return sb_detach(&b);
}

void report_line(logger_t *log, const char *key, const char *value)
{
    log_info(log, "  %s: %s", key, value);
}

void crawler_cleanup(crawler_t *c)
{
    if (c->clean_mode) crawler_clean_flush(c);
    if (c->writer) {
        c->writer->close(c->writer);
        c->writer = NULL;
    }
    if (c->page_count && !c->dry_run) crawler_checkpoint(c);
}

void crawler_final_report(crawler_t *c)
{
    double peak = get_peak_rss_mb();
    double elapsed = now_sec() - c->stats.start;
    if (elapsed < 0.001) elapsed = 0.001;
    double avg_size = c->stats.total_bytes / (double)(c->page_count > 0 ? c->page_count : 1);

    log_info(c->log, "------------------------------------------------------------");
    log_info(c->log, "stopped: %s", c->stop_reason);

    char num[64];
    snprintf(num, sizeof num, "%ld", c->page_count);
    report_line(c->log, "pages", num);
    snprintf(num, sizeof num, "%.1f", py_round(elapsed * 10.0) / 10.0);
    report_line(c->log, "elapsed_seconds", num);
    snprintf(num, sizeof num, "%.1f", py_round((double)c->page_count / elapsed * 60.0 * 10.0) / 10.0);
    report_line(c->log, "pages_per_minute", num);
    snprintf(num, sizeof num, "%lld", (long long)avg_size);
    report_line(c->log, "avg_page_bytes", num);
    {
        char *s = py_dict_repr(&c->stats.status, 0);
        report_line(c->log, "status_breakdown", s);
        free(s);
        s = py_dict_repr(&c->stats.types, 0);
        report_line(c->log, "content_types", s);
        free(s);
        s = py_dict_repr(&c->stats.languages, 0);
        report_line(c->log, "languages", s);
        free(s);
        s = py_dict_repr(&c->stats.domains, 10);
        report_line(c->log, "top_domains", s);
        free(s);
        s = py_dict_repr(&c->stats.errors, 0);
        report_line(c->log, "errors", s);
        free(s);
        snprintf(num, sizeof num, "%zu", c->stats.failed_urls.len);
        report_line(c->log, "failed_url_count", num);
    }
    log_info(c->log, "  robots_blocks: %ld", c->robots_blocks);
    log_info(c->log, "  network_errors: %ld", c->network_errors);
    log_info(c->log, "  duplicate_content: %ld", c->duplicate_content);
    if (c->clean_mode) {
        log_info(c->log, "  clean_mode: on (window %ld)", c->clean_window);
        log_info(c->log, "  clean_duplicates: %ld", c->clean_duplicates);
        log_info(c->log, "  clean_replaced: %ld", c->clean_replaced);
        log_info(c->log, "  clean_written: %ld", c->clean_kept);
        log_info(c->log, "  clean_flushes: %ld", c->clean_flushes);
    }
    log_info(c->log, "  noindex_skipped: %ld", c->noindex_skipped);
    log_info(c->log, "  favicons_found: %ld", c->favicons_found);
    log_info(c->log, "  filtered: %ld", c->filtered);
    log_info(c->log, "  frontier_dropped: %ld", c->frontier.dropped);
    log_info(c->log, "  bloom_saturation: %.1f%%", bloom_saturation(&c->seen) * 100.0);
    if (peak >= 0) log_info(c->log, "  peak_rss_mb: %.1f", peak);

    char *output_info = xstrdup(c->output_path);
    int validation_failed = 0;
    if (!c->storage_sqlite && !c->dry_run) {
        char *reason = NULL;
        if (crawler_validate_output(c, &reason)) {
            log_info(c->log, "  output_validation: OK (valid JSON array)");
            free(output_info);
            output_info = NULL;
            sb_t b; sb_init(&b);
            sb_appendf(&b, "%s (valid JSON array)", c->output_path);
            output_info = sb_detach(&b);
        } else {
            log_warn(c->log, "  output_validation: FAILED (%s)", reason ? reason : "?");
            free(output_info);
            output_info = NULL;
            sb_t b; sb_init(&b);
            sb_appendf(&b, "%s (validation FAILED: %s)", c->output_path,
                       reason ? reason : "?");
            output_info = sb_detach(&b);
            validation_failed = 1;
        }
        free(reason);
        log_info(c->log, "  output_file: %s", c->output_path);
    } else if (c->dry_run) {
        log_info(c->log, "  output_file: (dry run; nothing written)");
        free(output_info);
        output_info = xstrdup("(dry run; nothing written)");
    } else {
        log_info(c->log, "  output_file: %s", c->output_path);
    }

    const char *reason = c->stop_reason ? c->stop_reason : "";
    int priority = 3;
    const char *tags = "checkered_flag";
    const char *title = "Crawl finished";
    if (!strncmp(reason, "error", 5)) {
        priority = 5; tags = "rotating_light"; title = "Crawler crashed";
    } else if (validation_failed) {
        priority = 4; tags = "warning"; title = "Crawl finished (validation FAILED)";
    } else if (!strcmp(reason, "interrupted") || !strcmp(reason, "ctrl-c") ||
               !strcmp(reason, "user-quit")) {
        priority = 4; tags = "warning"; title = "Crawl interrupted";
    } else if (!strcmp(reason, "disk-full") || !strcmp(reason, "memory-limit")) {
        priority = 4; tags = "warning"; title = "Crawl stopped";
    }

    sb_t msg;
    sb_init(&msg);
    sb_appendf(&msg,
               "reason: %s\npages: %ld\nerrors: %ld  duplicates: %ld\n"
               "elapsed: %.1fs  rate: %.1f/min\noutput: %s",
               reason, c->page_count, c->network_errors, c->duplicate_content,
               py_round(elapsed * 10.0) / 10.0,
               py_round((double)c->page_count / elapsed * 60.0 * 10.0) / 10.0,
               output_info);
    crawler_notify(c, "finished", msg.buf, title, priority, tags,
                   (!c->storage_sqlite && !c->dry_run) ? c->output_path : NULL);
    sb_free(&msg);
    free(output_info);
}

/* -- the crawl loop -------------------------------------------------------- */
void crawler_run_loop(crawler_t *c)
{
    log_info(c->log, "Gateway crawler v%s", VERSION);
    log_info(c->log, "  Output:         %s", c->output_path);
    if (!c->storage_sqlite) log_info(c->log, "  Output format:  JSON array (single .json file)");
    else log_info(c->log, "  Output format:  SQLite database");
    if (c->has_max_pages)
        log_info(c->log, "  Max pages:      %ld", c->max_pages);
    else
        log_info(c->log, "  Max pages:      infinity");
    log_info(c->log, "  Max depth:      %d", c->max_depth);
    {
        char *q = fmt_thousands((long long)c->frontier.maxsize);
        log_info(c->log, "  Frontier cap:   %s", q);
        free(q);
    }
    {
        char *bits = fmt_thousands((long long)c->seen.num_bits);
        log_info(c->log, "  Bloom:          %s bits (%.1f KB)", bits,
                 (double)bloom_bytes_used(&c->seen) / 1024.0);
        free(bits);
    }
    if (c->has_max_memory) log_info(c->log, "  RSS cap:        %.1f MB", c->max_memory_mb);
    else log_info(c->log, "  RSS cap:        infinity MB");
    log_info(c->log, "  Sitemaps/Feeds: %s/%s", c->use_sitemaps ? "on" : "off",
             c->use_feeds ? "on" : "off");
    if (c->notifier) {
        sb_t ev;
        sb_init(&ev);
        size_t n = 0;
        kv_t *arr = map_top(&c->ntfy_events, &n);
        for (size_t i = 0; i < n; i++) {
            if (i) sb_putc(&ev, ',');
            sb_append(&ev, arr[i].key);
        }
        free(arr);
        log_info(c->log, "  ntfy:           %s  events=%s  progress_every=%s",
                 c->notifier->url, n ? ev.buf : "(none)",
                 c->ntfy_progress_every ? "on" : "off");
        sb_free(&ev);
    }
    log_info(c->log, "------------------------------------------------------------");

    {
        sb_t msg;
        sb_init(&msg);
        sb_appendf(&msg,
                   "Gateway crawler v%s started\nseeds: %zu\noutput: %s\nmax_pages: %s\nmax_depth: %d",
                   VERSION, c->seeds.len, c->output_path,
                   c->has_max_pages ? "limited" : "inf", c->max_depth);
        crawler_notify(c, "started", msg.buf, "Gateway crawler started", 3,
                       "arrow_forward", NULL);
        sb_free(&msg);
    }

    crawler_discover_sitemaps(c);
    crawler_discover_feeds(c);

    int mem_check_counter = 0;

    while (c->frontier.len && !c->stop) {
        if (c->tui && tui_enabled(c->tui)) {
            if (crawler_tui_pump(c)) break;
            if (c->paused) {
                crawler_tui_render(c, "PAUSED");
                sleep_ms(150);
                continue;
            }
        }
        if (c->has_max_pages && c->page_count >= c->max_pages) {
            free(c->stop_reason);
            c->stop_reason = xstrdup("page-limit");
            break;
        }
        if (!crawler_disk_ok(c)) {
            log_warn(c->log, "disk space below 50 MB; stopping");
            free(c->stop_reason);
            c->stop_reason = xstrdup("disk-full");
            break;
        }
        mem_check_counter++;
        if (c->has_max_memory && mem_check_counter >= 10) {
            mem_check_counter = 0;
            double cur = get_current_rss_mb();
            if (cur >= 0 && cur > c->max_memory_mb) {
                log_warn(c->log, "RSS %.1f MB > %.1f MB; stopping", cur, c->max_memory_mb);
                free(c->stop_reason);
                c->stop_reason = xstrdup("memory-limit");
                break;
            }
        }

        char *url = NULL, *base_url = NULL;
        int depth = 0;
        if (!frontier_pop(&c->frontier, &url, &depth, &base_url)) break;

        if (crawler_url_excluded(c, url)) {
            c->filtered++;
            free(url); free(base_url);
            continue;
        }
        if (crawler_domain_capped(c, url)) {
            free(url); free(base_url);
            continue;
        }
        if (!c->ignore_robots && !robots_cache_can_fetch(&c->robots, url)) {
            c->robots_blocks++;
            if (c->verbose_robots) {
                char *d = get_domain(url);
                if (!lru_get(&c->robots_explained, d)) {
                    lru_put(&c->robots_explained, d, (void *)1);
                    log_warn(c->log, "robots blocks %s", url);
                }
                free(d);
            }
            free(url); free(base_url);
            continue;
        }

        crawler_polite_wait(c, url);
        if (c->stop) { free(url); free(base_url); break; }

        if (c->tui) {
            tui_set_current(c->tui, url);
            crawler_tui_render(c, "FETCHING");
        }

        log_info(c->log, "[%ld] %s", c->page_count + 1, url);

        fetch_result res;
        fetch_result_init(&res);
        fetcher_fetch(&c->fetcher, url, &res);
        stats_record(&c->stats, &res);

        if (res.error && *res.error) {
            log_warn(c->log, "  error: %s", res.error);
            c->network_errors++;
            stats_record_fail(&c->stats, url, res.error);
            fetch_result_free(&res);
            crawler_tui_render(c, NULL);
            free(url); free(base_url);
            continue;
        }
        if (res.status == 304) {
            log_debug(c->log, "  304 not modified: %s", url);
            fetch_result_free(&res);
            crawler_tui_render(c, NULL);
            free(url); free(base_url);
            continue;
        }
        if (res.status >= 400) {
            c->network_errors++;
            sb_t reason;
            sb_init(&reason);
            sb_appendf(&reason, "HTTP %ld", res.status);
            stats_record_fail(&c->stats, url, reason.buf);
            sb_free(&reason);
            fetch_result_free(&res);
            crawler_tui_render(c, NULL);
            free(url); free(base_url);
            continue;
        }
        /* FIX (#3): non-HTML responses are never written as empty records */
        if (!res.meta) {
            c->filtered++;
            log_debug(c->log, "  skipped (content-type: %s)",
                      (res.content_type && *res.content_type) ? res.content_type : "unknown");
            fetch_result_free(&res);
            crawler_tui_render(c, NULL);
            free(url); free(base_url);
            continue;
        }

        if (c->language_filter.live) {
            char *lang = str_lower((res.language && *res.language) ? res.language
                                   : (res.meta->lang ? res.meta->lang : ""));
            int reject = (*lang && !map_has(&c->language_filter, lang));
            free(lang);
            if (reject) {
                c->filtered++;
                fetch_result_free(&res);
                crawler_tui_render(c, NULL);
                free(url); free(base_url);
                continue;
            }
        }

        record_t rec;
        vec_t *links = NULL;
        char *robots_meta = NULL;
        build_result(c, url, &res, &rec, &links, &robots_meta);

        int noindex = c->respect_noindex &&
                      robots_tokens_have(robots_meta, res.x_robots_tag, "noindex");
        if (noindex) {
            c->noindex_skipped++;
            log_debug(c->log, "  noindex: %s", url);
            record_free(&rec);
            fetch_result_free(&res);
            crawler_tui_render(c, NULL);
            free(url); free(base_url);
            continue;
        }

        if (!c->clean_mode && rec.content_hash && *rec.content_hash) {
            if (bloom_contains(&c->seen_content, rec.content_hash)) {
                c->duplicate_content++;
                record_free(&rec);
                fetch_result_free(&res);
                crawler_tui_render(c, NULL);
                free(url); free(base_url);
                continue;
            }
            bloom_add(&c->seen_content, rec.content_hash);
        }

        rec.favicon = crawler_favicon_for(c, rec.u, res.meta ? res.meta->favicon : NULL);
        crawler_write(c, &rec);
        {
            char *d = get_domain(url);
            map_incr(&c->domain_pages, d, 1);
            map_incr(&c->stats.domains, d, 1);
            free(d);
        }
        if (rec.lang && *rec.lang) map_incr(&c->stats.languages, rec.lang, 1);

        {
            char *short_title = trunc_chars(rec.t, 60);
            log_info(c->log, "  saved: %s (%.1fKB)", short_title, (double)res.html_bytes / 1024.0);
            free(short_title);
        }

        if (c->notifier && map_has(&c->ntfy_events, "progress") &&
            c->ntfy_progress_every > 0 &&
            c->page_count % c->ntfy_progress_every == 0) {
            sb_t pct;
            sb_init(&pct);
            if (c->has_max_pages && c->max_pages > 0)
                sb_appendf(&pct, " (%.0f%%)", (double)c->page_count * 100.0 / (double)c->max_pages);
            double el = now_sec() - c->stats.start;
            if (el < 0.001) el = 0.001;
            sb_t msg;
            sb_init(&msg);
            sb_appendf(&msg, "Pages: %ld%s\nQueue: %zu  |  Errors: %ld  |  Robots blocks: %ld\nRate: %.1f/min",
                       c->page_count, pct.buf, c->frontier.len, c->network_errors,
                       c->robots_blocks, (double)c->page_count / el * 60.0);
            char *title = NULL;
            if (asprintf(&title, "Progress: %ld pages", c->page_count) < 0) title = NULL;
            crawler_notify(c, "progress", msg.buf, title, 2, "bar_chart", NULL);
            free(title);
            sb_free(&msg);
            sb_free(&pct);
        }

        int nofollow = robots_tokens_have(robots_meta, res.x_robots_tag, "nofollow");
        if (!nofollow && links) {
            long limit = c->max_links_per_page;
            for (size_t i = 0; i < links->len && (long)i < limit; i++) {
                char *href = links->items[i];
                char *absolute = normalize_url_base(href, (res.final_url && *res.final_url) ? res.final_url : url);
                if (absolute) {
                    crawler_enqueue(c, absolute, depth + 1,
                                    (res.final_url && *res.final_url) ? res.final_url : url);
                    free(absolute);
                }
            }
        }

        if (c->page_count && c->page_count % 200 == 0) {
            crawler_checkpoint(c);
            double sat = bloom_saturation(&c->seen);
            if (sat > 0.7) {
                log_warn(c->log, "bloom filter %.0f%% saturated (increase --bloom-capacity)",
                         sat * 100.0);
            }
        }

        record_free(&rec);
        fetch_result_free(&res);
        crawler_tui_render(c, NULL);
        free(url);
        free(base_url);
    }

    if (!strcmp(c->stop_reason, "unknown")) {
        free(c->stop_reason);
        if (c->stop) c->stop_reason = xstrdup("interrupted");
        else if (!c->frontier.len) c->stop_reason = xstrdup("frontier-empty");
        else c->stop_reason = xstrdup("stopped");
    }
}

void crawler_run(crawler_t *c)
{
    if (c->tui) tui_enter(c->tui);
    crawler_run_loop(c);
    if (c->tui) tui_exit(c->tui);
    crawler_cleanup(c);
    crawler_final_report(c);
}
