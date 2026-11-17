#include "crawler.h"

/* fetcher retries, response parsing, robots cache */
/* ==========================================================================
 * Section 13 - Fetcher (retries, parsing) and RobotsCache
 * ========================================================================== */


void page_content_init(page_content *c) { memset(c, 0, sizeof *c); vec_init(&c->paragraphs); c->text = xstrdup(""); }

void page_content_free(page_content *c)
{
    free(c->text);
    vec_free(&c->paragraphs, free);
    memset(c, 0, sizeof *c);
}


void fetch_result_init(fetch_result *r)
{
    memset(r, 0, sizeof *r);
    r->final_url = xstrdup("");
    r->content_type = xstrdup("");
    r->language = xstrdup("");
    r->error = xstrdup("");
    r->x_robots_tag = xstrdup("");
    page_content_init(&r->content);
}

void fetch_result_free(fetch_result *r)
{
    free(r->final_url);
    free(r->content_type);
    free(r->language);
    free(r->error);
    free(r->x_robots_tag);
    free(r->charset);
    free(r->body);
    if (r->meta) { meta_parser_free(r->meta); free(r->meta); }
    page_content_free(&r->content);
    memset(r, 0, sizeof *r);
}


void cond_free(void *p)
{
    cond_t *c = p;
    if (!c) return;
    free(c->etag);
    free(c->lm);
    free(c);
}


void fetcher_init(fetcher_t *f, const char *ua, double timeout, int max_retries,
                         size_t page_max_bytes, size_t chunk, logger_t *log)
{
    memset(f, 0, sizeof *f);
    f->ua = xstrdup(ua);
    f->timeout = timeout;
    f->max_retries = max_retries;
    f->page_max_bytes = page_max_bytes;
    f->chunk = chunk;
    f->log = log;
    lru_init(&f->conditional, 10000, cond_free);
    f->accept = xstrdup("text/html,application/xhtml+xml;q=0.9,*/*;q=0.5");
    f->accept_language = xstrdup("en-US,en;q=0.5");
    f->accept_encoding = xstrdup("gzip, deflate");
}

void fetcher_free(fetcher_t *f)
{
    free(f->ua);
    free(f->accept);
    free(f->accept_language);
    free(f->accept_encoding);
    lru_free(&f->conditional);
}

/* returns: 0 ok, 1 HTTP error, 2 URL error */
int fetcher_do_request(fetcher_t *f, const char *url, fetch_result *r,
                              long *err_code, double *retry_after, char **err_msg,
                              int conditional_header)
{
    log_debug(f->log, "request preparation: %s", url);
    char *reason = NULL;
    if (!is_safe_to_fetch(url, &reason)) {
        sb_t m;
        sb_init(&m);
        sb_appendf(&m, "unexpected: ssrf blocked: %s", reason ? reason : "");
        if (err_msg) *err_msg = sb_detach(&m); else sb_free(&m);
        free(reason);
        return 2;
    }
    free(reason);

    http_opts o;
    memset(&o, 0, sizeof o);
    o.ua = f->ua;
    o.accept = f->accept;
    o.accept_language = f->accept_language;
    o.accept_encoding = f->accept_encoding;
    o.timeout = f->timeout;
    o.limit = f->page_max_bytes;
    o.method = "GET";
    cond_t *c = lru_get(&f->conditional, url);
    if (conditional_header && c) {
        o.if_none_match = c->etag;
        o.if_modified_since = c->lm;
    }

    http_response resp;
    http_response_init(&resp);
    int ok = http_perform(url, &o, &resp);
    if (!ok) {
        if (err_msg) {
            sb_t m;
            sb_init(&m);
            sb_appendf(&m, "%s", resp.error ? resp.error : "transport error");
            *err_msg = sb_detach(&m);
        }
        http_response_free(&resp);
        return 2;
    }

    if (resp.status == 304) {
        free(r->final_url);
        r->final_url = xstrdup(url);
        r->status = 304;
        r->from_cache = 1;
        http_response_free(&resp);
        return 0;
    }
    if (resp.status >= 400) {
        long code = resp.status;
        double ra = -1;
        if (resp.retry_after && *resp.retry_after) {
            char *endp = NULL;
            double v = strtod(resp.retry_after, &endp);
            if (endp && endp != resp.retry_after) ra = v;
        }
        *err_code = code;
        *retry_after = ra;
        http_response_free(&resp);
        return 1;
    }

    free(r->final_url);
    r->final_url = xstrdup(resp.final_url ? resp.final_url : url);
    r->status = resp.status;
    r->body = resp.body;
    r->body_len = resp.body_len;
    resp.body = NULL;
    r->html_bytes = resp.body_len;

    /* Content-Type: the part before ';', lowercased and stripped */
    {
        char *ct = xstrdup(resp.content_type ? resp.content_type : "");
        char *semi = strchr(ct, ';');
        if (semi) *semi = '\0';
        char *trimmed = str_strip(ct);
        char *low = str_lower(trimmed);
        free(trimmed);
        free(ct);
        free(r->content_type);
        r->content_type = low;
    }
    free(r->language);
    {
        char *lang = resp.content_language ? str_strip(resp.content_language) : xstrdup("");
        r->language = lang;
    }
    free(r->x_robots_tag);
    r->x_robots_tag = resp.x_robots_tag ? str_strip(resp.x_robots_tag) : xstrdup("");
    r->charset = resp.charset ? xstrdup(resp.charset) : NULL;

    log_debug(f->log, "response received: status=%ld, bytes=%zu, type=%s, url=%s",
              r->status, r->html_bytes,
              (r->content_type && *r->content_type) ? r->content_type : "(unknown)",
              r->final_url);

    if (resp.etag || resp.last_modified) {
        cond_t *entry = xcalloc(1, sizeof(cond_t));
        entry->etag = resp.etag ? xstrdup(resp.etag) : NULL;
        entry->lm = resp.last_modified ? xstrdup(resp.last_modified) : NULL;
        lru_put(&f->conditional, url, entry);
        /* the response strings are freed below, so keep the entry copies */
    }
    http_response_free(&resp);
    return 0;
}

void fetch_parse(fetcher_t *f, fetch_result *r)
{
    const char *ct = r->content_type ? r->content_type : "";
    int html_like = (strstr(ct, "html") != NULL) ||
                    (!strcmp(ct, "") || !strcmp(ct, "text/plain"));
    if (!html_like) return;

    const char *dumpbody = getenv("GWC_BODY_DUMP");
    if (dumpbody && *dumpbody) {
        FILE *df = fopen(dumpbody, "wb");
        if (df) { fwrite(r->body, 1, r->body_len, df); fclose(df); }
    }
    char *enc = detect_encoding(r->body, r->body_len, r->charset);
    char *text = decode_body(r->body, r->body_len, enc);
    free(enc);

    meta_parser *meta = xcalloc(1, sizeof(meta_parser));
    meta_parser_init(meta);
    text_parser vis;
    tp_init(&vis, VIS_MAX_BYTES);

    html_sink msink = { mp_starttag, mp_endtag, mp_data };
    html_parse(text, strlen(text), &msink, meta);
    html_sink tsink = { tp_starttag, tp_endtag, tp_data };
    html_parse(text, strlen(text), &tsink, &vis);

    r->meta = meta;
    free(r->content.text);
    r->content.text = tp_finish_text(&vis);
    /* move paragraphs out of the extractor */
    vec_free(&r->content.paragraphs, free);
    r->content.paragraphs = vis.paragraphs;
    vec_init(&vis.paragraphs);
    dedupe_paragraphs(&r->content.paragraphs, PARA_MAX_KEEP);
    r->content.word_count = count_words(r->content.text);
    r->content.reading_minutes = reading_time_minutes(r->content.word_count, 200);
    tp_free(&vis);
    free(text);

    log_debug(f->log, "parse complete: title='%s', words=%d",
              meta->title.buf ? meta->title.buf : "", r->content.word_count);
}

void fetcher_fetch(fetcher_t *f, const char *url, fetch_result *out)
{
    int attempt = 0;
    for (;;) {
        attempt++;
        long err_code = 0;
        double retry_after = -1;
        char *err_msg = NULL;
        int rc = fetcher_do_request(f, url, out, &err_code, &retry_after, &err_msg, 1);

        if (rc == 0) {
            if (out->from_cache) return;
            if (!out->body) {
                free(out->error);
                out->error = xstrdup("no body");
                return;
            }
            log_debug(f->log, "parsing response: %zu bytes from %s", out->body_len, out->final_url);
            fetch_parse(f, out);
            return;
        }
        if (rc == 1) {
            int transient = (err_code == 429 || err_code == 500 || err_code == 502 ||
                             err_code == 503 || err_code == 504);
            if (transient && attempt <= f->max_retries) {
                double delay = (retry_after >= 0) ? retry_after : pow(2.0, attempt);
                if (delay > 60) delay = 60;
                log_debug(f->log, "retry in %.1fs on HTTP %ld: %s", delay, err_code, url);
                double deadline = now_sec() + delay;
                while (now_sec() < deadline) sleep_ms(100);
                continue;
            }
            free(out->final_url);
            out->final_url = xstrdup(url);
            out->status = err_code;
            free(out->error);
            out->error = xstrdup("");
            sb_t m;
            sb_init(&m);
            sb_appendf(&m, "HTTP %ld", err_code);
            free(out->error);
            out->error = sb_detach(&m);
            return;
        }
        /* URL / transport error */
        if (attempt <= f->max_retries) {
            double delay = pow(2.0, attempt);
            if (delay > 30) delay = 30;
            double deadline = now_sec() + delay;
            while (now_sec() < deadline) sleep_ms(100);
            free(err_msg);
            continue;
        }
        free(out->final_url);
        out->final_url = xstrdup(url);
        free(out->error);
        out->error = err_msg ? err_msg : xstrdup("error");
        err_msg = NULL;
        return;
    }
}

/* -- robots cache ---------------------------------------------------------- */


void robots_entry_free(void *p)
{
    robots_entry *e = p;
    if (!e) return;
    if (e->rf) { robotsfile_free(e->rf); free(e->rf); }
    free(e);
}


void robots_cache_init(robots_cache *rc, const char *ua, double timeout, size_t maxsize)
{
    memset(rc, 0, sizeof *rc);
    rc->ua = xstrdup(ua);
    rc->timeout = timeout;
    lru_init(&rc->cache, maxsize, robots_entry_free);
}

void robots_cache_free(robots_cache *rc)
{
    free(rc->ua);
    lru_free(&rc->cache);
}

char *robots_url_for(const char *url)
{
    urlparts u;
    url_parse_parts(url, &u);
    sb_t b;
    sb_init(&b);
    sb_appendf(&b, "%s://%s/robots.txt", u.scheme ? u.scheme : "https", u.netloc ? u.netloc : "");
    urlparts_free(&u);
    return sb_detach(&b);
}

/* returns text (owned) plus status code */
char *fetch_robots_txt(const char *url, const char *ua, double timeout, int *status)
{
    http_opts o;
    memset(&o, 0, sizeof o);
    o.ua = (char *)ua;
    o.accept_encoding = (char *)"gzip, deflate";
    o.timeout = timeout;
    o.limit = DEFAULT_ROBOTS_BYTES;
    o.method = "GET";

    http_response resp;
    http_response_init(&resp);
    if (!http_perform(url, &o, &resp)) {
        *status = ROBOTS_ERROR;
        http_response_free(&resp);
        return NULL;
    }
    if (resp.status >= 400) {
        long code = resp.status;
        http_response_free(&resp);
        if (code == 401 || code == 403) { *status = ROBOTS_DISALLOW_ALL; return NULL; }
        if (code >= 400 && code < 500) { *status = ROBOTS_ALLOW_ALL; return NULL; }
        *status = ROBOTS_ERROR;
        return NULL;
    }
    char *enc = resp.charset ? xstrdup(resp.charset) : xstrdup("utf-8");
    char *text = decode_body(resp.body ? resp.body : (const unsigned char *)"", resp.body_len, enc);
    free(enc);
    http_response_free(&resp);
    *status = ROBOTS_OK;
    return text;
}

robots_entry *robots_cache_get(robots_cache *rc, const char *url)
{
    char *key = robots_url_for(url);
    robots_entry *e = lru_get(&rc->cache, key);
    if (e) { free(key); return e; }

    int status = ROBOTS_ERROR;
    char *text = fetch_robots_txt(key, rc->ua, rc->timeout, &status);
    e = xcalloc(1, sizeof(robots_entry));
    e->status = status;
    e->delay = -1;
    if (status == ROBOTS_OK && text) {
        robotsfile *rf = xcalloc(1, sizeof(robotsfile));
        vec_init(&rf->entries);
        robotsfile_parse(rf, text);
        e->rf = rf;
        double d = robots_crawl_delay(rf, rc->ua);
        if (d >= 0) { e->delay = d; e->has_delay = 1; }
    }
    free(text);
    lru_put(&rc->cache, key, e);
    free(key);
    return e;
}

int robots_cache_can_fetch(robots_cache *rc, const char *url)
{
    robots_entry *e = robots_cache_get(rc, url);
    if (e->status == ROBOTS_DISALLOW_ALL) return 0;
    if (e->status == ROBOTS_ALLOW_ALL || e->status == ROBOTS_ERROR) return 1;
    if (!e->rf) return 1;
    return robots_can_fetch(e->rf, rc->ua, url);
}

double robots_cache_crawl_delay(robots_cache *rc, const char *url)
{
    robots_entry *e = robots_cache_get(rc, url);
    return e->has_delay ? e->delay : -1.0;
}
