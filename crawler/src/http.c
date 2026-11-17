#include "crawler.h"

/* libcurl transport, redirect revalidation, gzip/deflate */
/* ==========================================================================
 * Section 12 - HTTP transport (urllib.request + SafeRedirectHandler)
 * ========================================================================== */


void http_response_init(http_response *r) { memset(r, 0, sizeof *r); }

void http_response_free(http_response *r)
{
    free(r->final_url);
    free(r->content_type);
    free(r->content_language);
    free(r->x_robots_tag);
    free(r->etag);
    free(r->last_modified);
    free(r->retry_after);
    free(r->charset);
    free(r->location);
    free(r->body);
    free(r->error);
    memset(r, 0, sizeof *r);
}

/* streaming body sink with optional gzip/deflate inflation, hard byte cap */

void body_buf_init(body_buf *b, size_t limit)
{
    memset(b, 0, sizeof *b);
    b->limit = limit;
    b->cap = 64 * 1024;
    b->out = xmalloc(b->cap);
}

void body_buf_set_mode(body_buf *b, int mode)
{
    if (b->zinit || !mode) return;
    memset(&b->zs, 0, sizeof b->zs);
    int wbits = (mode == 1) ? (16 + MAX_WBITS) : MAX_WBITS;
    if (inflateInit2(&b->zs, wbits) == Z_OK) {
        b->zinit = 1;
        b->zmode = mode;
    }
}

void body_buf_free(body_buf *b)
{
    if (b->zinit) inflateEnd(&b->zs);
    free(b->out);
    b->out = NULL;
    b->len = b->cap = 0;
}

void body_buf_reserve(body_buf *b, size_t need)
{
    if (b->len + need <= b->cap) return;
    size_t cap = b->cap ? b->cap : 65536;
    while (cap < b->len + need) cap *= 2;
    b->out = xrealloc(b->out, cap);
    b->cap = cap;
}

size_t body_buf_write_raw(body_buf *b, const void *data, size_t n)
{
    if (b->len >= b->limit) { b->reached_limit = 1; return 0; }
    if (b->len + n > b->limit) { n = b->limit - b->len; b->reached_limit = 1; }
    body_buf_reserve(b, n);
    memcpy(b->out + b->len, data, n);
    b->len += n;
    return n;
}

size_t body_buf_write(body_buf *b, const char *data, size_t n)
{
    if (!b->zmode || !b->zinit) return body_buf_write_raw(b, data, n);

    b->zs.next_in = (Bytef *)data;
    b->zs.avail_in = (uInt)n;
    while (b->zs.avail_in > 0) {
        if (b->len >= b->limit) { b->reached_limit = 1; return 0; }
        size_t space = b->cap - b->len;
        if (space < 4096) {
            body_buf_reserve(b, 65536);
            space = b->cap - b->len;
        }
        if (b->len + space > b->limit) space = b->limit - b->len;
        b->zs.next_out = b->out + b->len;
        b->zs.avail_out = (uInt)space;
        int rc = inflate(&b->zs, Z_NO_FLUSH);
        size_t produced = space - b->zs.avail_out;
        b->len += produced;
        if (b->len >= b->limit) { b->reached_limit = 1; return 0; }
        if (rc == Z_STREAM_END) break;
        if (rc != Z_OK && rc != Z_BUF_ERROR) { b->zerror = 1; break; }
        if (produced == 0 && rc == Z_BUF_ERROR && b->zs.avail_in == 0) break;
    }
    return n;
}

/* decompressobj.flush() equivalent: recover the tail of the stream */
void body_buf_flush_tail(body_buf *b)
{
    if (!b->zmode || !b->zinit || b->reached_limit || b->zerror) return;
    for (;;) {
        size_t space = b->cap - b->len;
        if (space < 4096) {
            body_buf_reserve(b, 65536);
            space = b->cap - b->len;
        }
        if (b->len + space > b->limit) space = b->limit - b->len;
        if (space == 0) return;
        b->zs.next_in = Z_NULL;
        b->zs.avail_in = 0;
        b->zs.next_out = b->out + b->len;
        b->zs.avail_out = (uInt)space;
        int rc = inflate(&b->zs, Z_FINISH);
        size_t produced = space - b->zs.avail_out;
        b->len += produced;
        if (rc == Z_STREAM_END || produced == 0) return;
        if (rc != Z_OK) return;
    }
}



size_t curl_write_cb(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    write_ctx *w = userdata;
    return body_buf_write(w->body, ptr, size * nmemb);
}

char *header_value(const char *line, size_t len, const char *name)
{
    size_t nl = strlen(name);
    if (len <= nl + 1) return NULL;
    if (strncasecmp(line, name, nl) != 0) return NULL;
    if (line[nl] != ':') return NULL;
    size_t vs = nl + 1;
    while (vs < len && (line[vs] == ' ' || line[vs] == '\t')) vs++;
    size_t ve = len;
    while (ve > vs && (line[ve - 1] == '\r' || line[ve - 1] == '\n' ||
                       line[ve - 1] == ' ' || line[ve - 1] == '\t')) ve--;
    return xstrndup(line + vs, ve - vs);
}

void set_once(char **slot, char *value)
{
    if (!value) return;
    if (*slot) { free(value); return; }
    *slot = value;
}

size_t curl_header_cb(char *buffer, size_t size, size_t nitems, void *userdata)
{
    write_ctx *w = userdata;
    http_response *r = w->resp;
    size_t len = size * nitems;
    const char *line = buffer;

    char *v;
    if ((v = header_value(line, len, "Content-Type"))) set_once(&r->content_type, v);
    if ((v = header_value(line, len, "Content-Language"))) set_once(&r->content_language, v);
    if ((v = header_value(line, len, "X-Robots-Tag"))) set_once(&r->x_robots_tag, v);
    if ((v = header_value(line, len, "ETag"))) set_once(&r->etag, v);
    if ((v = header_value(line, len, "Last-Modified"))) set_once(&r->last_modified, v);
    if ((v = header_value(line, len, "Retry-After"))) set_once(&r->retry_after, v);
    if ((v = header_value(line, len, "Location"))) set_once(&r->location, v);
    if ((v = header_value(line, len, "Content-Encoding"))) {
        char *low = str_lower(v);
        if (strstr(low, "gzip")) body_buf_set_mode(w->body, 1);
        else if (strstr(low, "deflate")) body_buf_set_mode(w->body, 2);
        free(low);
        free(v);
    }
    return len;
}

char *charset_from_content_type(const char *ct)
{
    if (!ct) return NULL;
    const char *p = ct;
    while ((p = strcasestr(p, "charset"))) {
        const char *q = p + 7;
        while (*q == ' ' || *q == '\t') q++;
        if (*q != '=') { p += 7; continue; }
        q++;
        while (*q == ' ' || *q == '\t' || *q == '"' || *q == '\'') q++;
        const char *e = q;
        while (*e && *e != ';' && *e != '"' && *e != '\'' && *e != ' ' && *e != '\t') e++;
        if (e > q) return xstrndup(q, (size_t)(e - q));
        p += 7;
    }
    return NULL;
}

int is_redirect_code(long code)
{
    return code == 301 || code == 302 || code == 303 || code == 307 || code == 308;
}

void http_response_reset_headers(http_response *r)
{
    free(r->content_type); r->content_type = NULL;
    free(r->content_language); r->content_language = NULL;
    free(r->x_robots_tag); r->x_robots_tag = NULL;
    free(r->etag); r->etag = NULL;
    free(r->last_modified); r->last_modified = NULL;
    free(r->retry_after); r->retry_after = NULL;
    free(r->location); r->location = NULL;
    free(r->charset); r->charset = NULL;
}

int http_perform(const char *url, const http_opts *o, http_response *out)
{
    char *current = xstrdup(url);
    const char *method = o->method ? o->method : "GET";
    const unsigned char *post_body = o->post_body;
    size_t post_len = o->post_len;

    for (int hop = 0; hop <= 10; hop++) {
        if (hop > 0) {
            char *reason = NULL;
            if (!is_safe_to_fetch(current, &reason)) {
                out->transport_error = 1;
                sb_t msg;
                sb_init(&msg);
                sb_appendf(&msg, "redirect blocked: %s", reason ? reason : "");
                out->error = sb_detach(&msg);
                free(reason);
                free(current);
                return 0;
            }
            free(reason);
        }

        CURL *h = curl_easy_init();
        if (!h) {
            out->transport_error = 1;
            out->error = xstrdup("cannot init curl");
            free(current);
            return 0;
        }
        body_buf body;
        body_buf_init(&body, o->limit);
        write_ctx w = { out, &body };

        struct curl_slist *hdrs = NULL;
        sb_t line;
        sb_init(&line);
        sb_appendf(&line, "User-Agent: %s", o->ua ? o->ua : DEFAULT_USER_AGENT);
        hdrs = curl_slist_append(hdrs, line.buf);
        sb_clear(&line);
        if (o->accept) {
            sb_appendf(&line, "Accept: %s", o->accept);
            hdrs = curl_slist_append(hdrs, line.buf);
            sb_clear(&line);
        }
        if (o->accept_language) {
            sb_appendf(&line, "Accept-Language: %s", o->accept_language);
            hdrs = curl_slist_append(hdrs, line.buf);
            sb_clear(&line);
        }
        if (o->accept_encoding) {
            sb_appendf(&line, "Accept-Encoding: %s", o->accept_encoding);
            hdrs = curl_slist_append(hdrs, line.buf);
            sb_clear(&line);
        }
        sb_append(&line, "Connection: close");
        hdrs = curl_slist_append(hdrs, line.buf);
        sb_clear(&line);
        if (o->if_none_match) {
            sb_appendf(&line, "If-None-Match: %s", o->if_none_match);
            hdrs = curl_slist_append(hdrs, line.buf);
            sb_clear(&line);
        }
        if (o->if_modified_since) {
            sb_appendf(&line, "If-Modified-Since: %s", o->if_modified_since);
            hdrs = curl_slist_append(hdrs, line.buf);
            sb_clear(&line);
        }
        if (o->extra_names && o->extra_values) {
            for (int i = 0; o->extra_names[i]; i++) {
                sb_appendf(&line, "%s: %s", o->extra_names[i], o->extra_values[i]);
                hdrs = curl_slist_append(hdrs, line.buf);
                sb_clear(&line);
            }
        }
        sb_free(&line);

        curl_easy_setopt(h, CURLOPT_URL, current);
        curl_easy_setopt(h, CURLOPT_HTTPHEADER, hdrs);
        curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, curl_write_cb);
        curl_easy_setopt(h, CURLOPT_WRITEDATA, &w);
        curl_easy_setopt(h, CURLOPT_HEADERFUNCTION, curl_header_cb);
        curl_easy_setopt(h, CURLOPT_HEADERDATA, &w);
        curl_easy_setopt(h, CURLOPT_TIMEOUT_MS, (long)(o->timeout * 1000.0));
        curl_easy_setopt(h, CURLOPT_CONNECTTIMEOUT_MS, (long)(o->timeout * 1000.0));
        curl_easy_setopt(h, CURLOPT_FOLLOWLOCATION, 0L);
        curl_easy_setopt(h, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(h, CURLOPT_FAILONERROR, 1L);
        curl_easy_setopt(h, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(h, CURLOPT_SSL_VERIFYHOST, 2L);
        if (!strcmp(method, "POST")) {
            curl_easy_setopt(h, CURLOPT_POST, 1L);
            curl_easy_setopt(h, CURLOPT_POSTFIELDS, post_body ? (const char *)post_body : "");
            curl_easy_setopt(h, CURLOPT_POSTFIELDSIZE, (long)post_len);
        } else if (!strcmp(method, "HEAD")) {
            curl_easy_setopt(h, CURLOPT_NOBODY, 1L);
        }

        CURLcode rc = curl_easy_perform(h);
        long status = 0;
        curl_easy_getinfo(h, CURLINFO_RESPONSE_CODE, &status);
        out->status = status;
        char *eff_url = NULL;
        curl_easy_getinfo(h, CURLINFO_EFFECTIVE_URL, &eff_url);
        body_buf_flush_tail(&body);

        free(out->final_url);
        out->final_url = xstrdup(eff_url ? eff_url : current);

        if (rc == CURLE_OK || rc == CURLE_HTTP_RETURNED_ERROR || rc == CURLE_WRITE_ERROR) {
            if (is_redirect_code(status) && out->location && hop < 10) {
                char *next = url_join(current, out->location);
                http_response_reset_headers(out);
                if (!next) {
                    out->transport_error = 1;
                    out->error = xstrdup("bad redirect target");
                    body_buf_free(&body);
                    curl_slist_free_all(hdrs);
                    curl_easy_cleanup(h);
                    free(current);
                    return 0;
                }
                if (!strcmp(method, "POST") && status != 307 && status != 308) {
                    method = "GET";
                    post_body = NULL;
                    post_len = 0;
                }
                free(current);
                current = next;
                body_buf_free(&body);
                curl_slist_free_all(hdrs);
                curl_easy_cleanup(h);
                continue;
            }

            out->body = body.out;
            out->body_len = body.len;
            out->truncated = body.reached_limit;
            body.out = NULL;
            out->charset = charset_from_content_type(out->content_type);
            body_buf_free(&body);
            curl_slist_free_all(hdrs);
            curl_easy_cleanup(h);
            free(current);
            if (rc == CURLE_HTTP_RETURNED_ERROR) {
                /* urllib raises HTTPError here: no body is handed back */
                free(out->body);
                out->body = NULL;
                out->body_len = 0;
            }
            return 1;
        }

        out->transport_error = 1;
        out->error = xstrdup(curl_easy_strerror(rc));
        body_buf_free(&body);
        curl_slist_free_all(hdrs);
        curl_easy_cleanup(h);
        free(current);
        return 0;
    }

    free(current);
    out->transport_error = 1;
    out->error = xstrdup("too many redirects");
    return 0;
}
