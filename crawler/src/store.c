#include "crawler.h"

/* statistics, records, JSON/SQLite writers, checkpoint, ntfy */
/* ==========================================================================
 * Section 14 - statistics, record, writers, checkpoint, ntfy, frontier
 * ========================================================================== */



void stats_init(stats_t *s)
{
    memset(s, 0, sizeof *s);
    map_init(&s->status);
    map_init(&s->domains);
    map_init(&s->languages);
    map_init(&s->types);
    map_init(&s->errors);
    vec_init(&s->failed_urls);
    s->start = now_sec();
}

void stats_free(stats_t *s)
{
    map_free(&s->status, NULL);
    map_free(&s->domains, NULL);
    map_free(&s->languages, NULL);
    map_free(&s->types, NULL);
    map_free(&s->errors, NULL);
    for (size_t i = 0; i < s->failed_urls.len; i++) {
        failed_url_t *fu = s->failed_urls.items[i];
        free(fu->url);
        free(fu->reason);
        free(fu);
    }
    vec_free(&s->failed_urls, NULL);
}

void stats_record(stats_t *s, fetch_result *r)
{
    char key[32];
    snprintf(key, sizeof key, "%ld", r->status);
    map_incr(&s->status, key, 1);
    if (r->content_type && *r->content_type) map_incr(&s->types, r->content_type, 1);
    s->total_bytes += (long long)r->html_bytes;
    if (r->language && *r->language) map_incr(&s->languages, r->language, 1);
}

void stats_record_fail(stats_t *s, const char *url, const char *reason)
{
    char *trimmed = trunc_chars(reason ? reason : "", 80);
    map_incr(&s->errors, trimmed, 1);
    free(trimmed);
    if (s->failed_urls.len < 500) {
        failed_url_t *fu = xcalloc(1, sizeof(failed_url_t));
        fu->url = xstrdup(url);
        fu->reason = trunc_chars(reason ? reason : "", 200);
        vec_push(&s->failed_urls, fu);
    }
}

/* pairs sorted by count desc (ties broken by key for stable output) */

int kv_cmp(const void *a, const void *b)
{
    const kv_t *x = a, *y = b;
    if (x->count != y->count) return x->count > y->count ? -1 : 1;
    return strcmp(x->key, y->key);
}

kv_t *map_top(map_t *m, size_t *out_n)
{
    size_t n = 0;
    kv_t *arr = xmalloc((m->live + 1) * sizeof(kv_t));
    for (size_t i = 0; i < m->cap; i++) {
        if (m->ent[i].state == MAP_USED) {
            arr[n].key = m->ent[i].key;
            arr[n].count = (long)(intptr_t)m->ent[i].val;
            n++;
        }
    }
    if (n > 1) qsort(arr, n, sizeof(kv_t), kv_cmp);
    *out_n = n;
    return arr;
}

void json_write_map(sb_t *b, map_t *m, size_t limit)
{
    size_t n = 0;
    kv_t *arr = map_top(m, &n);
    if (limit && n > limit) n = limit;
    sb_putc(b, '{');
    for (size_t i = 0; i < n; i++) {
        if (i) sb_append(b, ", ");
        json_write_string(b, arr[i].key);
        sb_append(b, ": ");
        sb_appendf(b, "%ld", arr[i].count);
    }
    sb_putc(b, '}');
    free(arr);
}

void stats_to_json(sb_t *b, stats_t *s, int include_failed)
{
    sb_append(b, "{\"status\": ");
    json_write_map(b, &s->status, 0);
    sb_append(b, ", \"domains\": ");
    json_write_map(b, &s->domains, 200);
    sb_append(b, ", \"languages\": ");
    json_write_map(b, &s->languages, 0);
    sb_append(b, ", \"types\": ");
    json_write_map(b, &s->types, 0);
    sb_append(b, ", \"errors\": ");
    json_write_map(b, &s->errors, 0);
    sb_append(b, ", \"failed_urls\": ");
    if (include_failed) {
        sb_putc(b, '[');
        for (size_t i = 0; i < s->failed_urls.len; i++) {
            failed_url_t *fu = s->failed_urls.items[i];
            if (i) sb_append(b, ", ");
            sb_append(b, "{\"url\": ");
            json_write_string(b, fu->url);
            sb_append(b, ", \"reason\": ");
            json_write_string(b, fu->reason);
            sb_append(b, "}");
        }
        sb_putc(b, ']');
    } else {
        sb_append(b, "[]");
    }
    sb_appendf(b, ", \"total_bytes\": %lld}", s->total_bytes);
}

/* -- crawl record ---------------------------------------------------------- */

void record_init(record_t *r) { memset(r, 0, sizeof *r); }

void record_free(record_t *r)
{
    free(r->id); free(r->t); free(r->u); free(r->d); free(r->s);
    free(r->content_type); free(r->lang); free(r->author);
    free(r->pub_date); free(r->mod_date); free(r->canonical);
    free(r->favicon);
    free(r->content_hash);
    free(r->sig);
    memset(r, 0, sizeof *r);
}

/* Deep copy, used by --clean to stage records while they can still be replaced */
record_t *record_clone(const record_t *r)
{
    record_t *c = xcalloc(1, sizeof *c);
    c->id = r->id ? xstrdup(r->id) : NULL;
    c->t = r->t ? xstrdup(r->t) : NULL;
    c->u = r->u ? xstrdup(r->u) : NULL;
    c->d = r->d ? xstrdup(r->d) : NULL;
    c->s = r->s ? xstrdup(r->s) : NULL;
    c->status = r->status;
    c->content_type = r->content_type ? xstrdup(r->content_type) : NULL;
    c->lang = r->lang ? xstrdup(r->lang) : NULL;
    c->author = r->author ? xstrdup(r->author) : NULL;
    c->pub_date = r->pub_date ? xstrdup(r->pub_date) : NULL;
    c->mod_date = r->mod_date ? xstrdup(r->mod_date) : NULL;
    c->canonical = r->canonical ? xstrdup(r->canonical) : NULL;
    c->favicon = r->favicon ? xstrdup(r->favicon) : NULL;
    c->word_count = r->word_count;
    c->reading_minutes = r->reading_minutes;
    c->content_hash = r->content_hash ? xstrdup(r->content_hash) : NULL;
    c->sig = r->sig ? xstrdup(r->sig) : NULL;
    return c;
}

/* The de-duplication key: the near-duplicate signature, or the URL for pages
 * with no text to compare (PDFs, images, empty bodies). */
char *record_dedupe_key(const record_t *r)
{
    if (r->sig && *r->sig) return xstrdup(r->sig);
    sb_t b;
    sb_init(&b);
    sb_append(&b, "url:");
    sb_append(&b, (r->u && *r->u) ? r->u : "");
    return sb_detach(&b);
}

/* Which of two records should be the one written?
 * Returns <0 when a is the better record, >0 when b is, 0 when they tie.
 * Order: more words, then a longer description, then a longer title, then the
 * shorter URL (less query junk), then the lexicographically smaller URL so the
 * result is stable across runs. */
int record_quality_cmp(const record_t *a, const record_t *b)
{
    if (a->word_count != b->word_count) return a->word_count > b->word_count ? -1 : 1;

    size_t la = a->d ? strlen(a->d) : 0, lb = b->d ? strlen(b->d) : 0;
    if (la != lb) return la > lb ? -1 : 1;

    la = a->t ? strlen(a->t) : 0; lb = b->t ? strlen(b->t) : 0;
    if (la != lb) return la > lb ? -1 : 1;

    la = a->u ? strlen(a->u) : 0; lb = b->u ? strlen(b->u) : 0;
    if (la != lb) return la < lb ? -1 : 1;

    return strcmp(a->u ? a->u : "", b->u ? b->u : "");
}

void record_to_json(sb_t *b, const record_t *r)
{
    sb_append(b, "{\"id\": ");
    json_write_string(b, r->id);
    sb_append(b, ", \"t\": ");
    json_write_string(b, r->t);
    sb_append(b, ", \"u\": ");
    json_write_string(b, r->u);
    sb_append(b, ", \"d\": ");
    json_write_string(b, r->d);
    sb_append(b, ", \"s\": ");
    json_write_string(b, r->s);
    sb_appendf(b, ", \"status\": %ld, \"content_type\": ", r->status);
    json_write_string(b, r->content_type);
    sb_append(b, ", \"lang\": ");
    json_write_string(b, r->lang);
    sb_append(b, ", \"author\": ");
    json_write_string(b, r->author);
    sb_append(b, ", \"pub_date\": ");
    json_write_string(b, r->pub_date);
    sb_append(b, ", \"mod_date\": ");
    json_write_string(b, r->mod_date);
    sb_append(b, ", \"canonical\": ");
    json_write_string(b, r->canonical);
    if (r->favicon) {
        sb_append(b, ", \"favicon\": ");
        json_write_string(b, r->favicon);
    }
    sb_appendf(b, ", \"word_count\": %d, \"reading_minutes\": %d, \"content_hash\": ",
               r->word_count, r->reading_minutes);
    json_write_string(b, r->content_hash);
    sb_putc(b, '}');
}

/* -- writers --------------------------------------------------------------- */


void json_writer_write(writer_t *w, const char *json)
{
    json_writer_t *jw = w->impl;
    if (jw->closed || !jw->fh) return;
    if (jw->first) fputs("  ", jw->fh);
    else fputs(",\n  ", jw->fh);
    fputs(json, jw->fh);
    fflush(jw->fh);
    jw->first = 0;
    jw->count++;
}

void json_writer_close(writer_t *w)
{
    json_writer_t *jw = w->impl;
    if (jw->closed) return;
    jw->closed = 1;
    if (jw->fh) {
        fputs(jw->count ? "\n]\n" : "]\n", jw->fh);
        fflush(jw->fh);
        fsync(fileno(jw->fh));
        fclose(jw->fh);
        jw->fh = NULL;
    }
    if (rename(jw->tmp, jw->path) != 0) {
        /* fall back to a copy */
        FILE *in = fopen(jw->tmp, "rb");
        if (in) {
            FILE *out = fopen(jw->path, "wb");
            if (out) {
                char buf[65536];
                size_t n;
                while ((n = fread(buf, 1, sizeof buf, in)) > 0) fwrite(buf, 1, n, out);
                fclose(out);
            }
            fclose(in);
            remove(jw->tmp);
        }
    }
    free(jw->path);
    free(jw->tmp);
    free(jw);
    free(w);
}

writer_t *json_writer_new(const char *path)
{
    writer_t *w = xcalloc(1, sizeof(writer_t));
    json_writer_t *jw = xcalloc(1, sizeof(json_writer_t));
    jw->path = xstrdup(path);
    size_t n = strlen(path);
    jw->tmp = xmalloc(n + 6);
    snprintf(jw->tmp, n + 6, "%s.part", path);
    jw->fh = fopen(jw->tmp, "w");
    if (jw->fh) {
        fputs("[\n", jw->fh);
        fflush(jw->fh);
    }
    jw->first = 1;
    w->write = json_writer_write;
    w->close = json_writer_close;
    w->impl = jw;
    return w;
}

void null_writer_write(writer_t *w, const char *json) { (void)w; (void)json; }
void null_writer_close(writer_t *w) { free(w); }

writer_t *null_writer_new(void)
{
    writer_t *w = xcalloc(1, sizeof(writer_t));
    w->write = null_writer_write;
    w->close = null_writer_close;
    return w;
}

#ifdef HAVE_SQLITE3

void sqlite_writer_write(writer_t *w, const char *json)
{
    sqlite_writer_t *sw = w->impl;
    json_val *o = json_parse(json);
    if (!o) return;
    const char *id = json_as_str(json_obj_get(o, "id"));
    const char *u = json_as_str(json_obj_get(o, "u"));
    const char *t = json_as_str(json_obj_get(o, "t"));
    const char *d = json_as_str(json_obj_get(o, "d"));
    const char *s = json_as_str(json_obj_get(o, "s"));
    json_val *st = json_obj_get(o, "status");
    const char *ct = json_as_str(json_obj_get(o, "content_type"));
    const char *lang = json_as_str(json_obj_get(o, "lang"));
    const char *author = json_as_str(json_obj_get(o, "author"));
    const char *pd = json_as_str(json_obj_get(o, "pub_date"));
    const char *canon = json_as_str(json_obj_get(o, "canonical"));
    json_val *wc = json_obj_get(o, "word_count");
    const char *ch = json_as_str(json_obj_get(o, "content_hash"));
    const char *favicon = json_as_str(json_obj_get(o, "favicon"));
    sqlite3_bind_text(sw->ins, 1, id ? id : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(sw->ins, 2, u ? u : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(sw->ins, 3, t ? t : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(sw->ins, 4, d ? d : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(sw->ins, 5, s ? s : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(sw->ins, 6, st ? (int)st->num : 0);
    sqlite3_bind_text(sw->ins, 7, ct ? ct : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(sw->ins, 8, lang ? lang : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(sw->ins, 9, author ? author : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(sw->ins, 10, pd ? pd : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(sw->ins, 11, canon ? canon : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(sw->ins, 12, wc ? (int)wc->num : 0);
    sqlite3_bind_text(sw->ins, 13, ch ? ch : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(sw->ins, 14, now_sec());
    sqlite3_bind_text(sw->ins, 15, favicon ? favicon : "", -1, SQLITE_TRANSIENT);
    sqlite3_step(sw->ins);
    sqlite3_reset(sw->ins);
    sw->count++;
    json_free(o);
}

void sqlite_writer_close(writer_t *w)
{
    sqlite_writer_t *sw = w->impl;
    if (sw->ins) sqlite3_finalize(sw->ins);
    if (sw->db) sqlite3_close(sw->db);
    free(sw);
    free(w);
}

writer_t *sqlite_writer_new(const char *path)
{
    writer_t *w = xcalloc(1, sizeof(writer_t));
    sqlite_writer_t *sw = xcalloc(1, sizeof(sqlite_writer_t));
    if (sqlite3_open(path, &sw->db) != SQLITE_OK) {
        fprintf(stderr, "[ERROR] cannot open sqlite database %s\n", path);
        exit(2);
    }
    const char *ddl =
        "CREATE TABLE IF NOT EXISTS pages ("
        " id TEXT PRIMARY KEY, url TEXT NOT NULL, title TEXT,"
        " description TEXT, domain TEXT, status INTEGER,"
        " content_type TEXT, language TEXT, author TEXT,"
        " pub_date TEXT, canonical TEXT, word_count INTEGER,"
        " content_hash TEXT, crawled_at REAL, favicon TEXT)";
    sqlite3_exec(sw->db, ddl, NULL, NULL, NULL);
    /* older databases from before favicon support: add the column if missing */
    sqlite3_exec(sw->db, "ALTER TABLE pages ADD COLUMN favicon TEXT", NULL, NULL, NULL);
    sqlite3_exec(sw->db, "CREATE INDEX IF NOT EXISTS idx_domain ON pages(domain)", NULL, NULL, NULL);
    sqlite3_exec(sw->db, "CREATE INDEX IF NOT EXISTS idx_lang ON pages(language)", NULL, NULL, NULL);
    sqlite3_prepare_v2(sw->db,
        "INSERT OR REPLACE INTO pages VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
        -1, &sw->ins, NULL);
    w->write = sqlite_writer_write;
    w->close = sqlite_writer_close;
    w->impl = sw;
    return w;
}
#endif

/* -- ntfy notifier --------------------------------------------------------- */

ntfy_t *ntfy_new(const char *topic, logger_t *log)
{
    if (!topic || !*topic) return NULL;
    ntfy_t *n = xcalloc(1, sizeof(ntfy_t));
    char *trimmed = str_strip(topic);
    if (!strncmp(trimmed, "http://", 7) || !strncmp(trimmed, "https://", 8)) {
        n->url = xstrdup(trimmed);
    } else {
        sb_t b;
        sb_init(&b);
        sb_appendf(&b, "%s/%s", DEFAULT_NTFY_SERVER, trimmed);
        n->url = sb_detach(&b);
    }
    free(trimmed);
    n->log = log;
    n->timeout = DEFAULT_NTFY_TIMEOUT;
    return n;
}

void ntfy_free(ntfy_t *n)
{
    if (!n) return;
    free(n->url);
    free(n->last_error);
    free(n);
}

int ntfy_send(ntfy_t *n, const char *message, const char *title,
                     int priority, const char *tags, const char *click)
{
    if (!n) return 0;
    char pr[8];
    snprintf(pr, sizeof pr, "%d", priority);
    char *names[5];
    char *values[5];
    int idx = 0;
    names[idx] = (char *)"Content-Type";
    values[idx++] = (char *)"text/plain; charset=utf-8";
    names[idx] = (char *)"Priority";
    values[idx++] = pr;
    if (title && *title) { names[idx] = (char *)"Title"; values[idx++] = (char *)title; }
    if (tags && *tags) { names[idx] = (char *)"Tags"; values[idx++] = (char *)tags; }
    if (click && *click) { names[idx] = (char *)"Click"; values[idx++] = (char *)click; }
    names[idx] = NULL;
    values[idx] = NULL;

    http_opts o;
    memset(&o, 0, sizeof o);
    o.ua = DEFAULT_USER_AGENT;
    o.timeout = n->timeout;
    o.limit = 65536;
    o.method = "POST";
    o.post_body = (const unsigned char *)(message ? message : "");
    o.post_len = message ? strlen(message) : 0;
    o.extra_names = names;
    o.extra_values = values;

    http_response resp;
    http_response_init(&resp);
    int ok = http_perform(n->url, &o, &resp);
    int success = ok && resp.status >= 200 && resp.status < 300;
    if (ok && !success && n->log)
        log_warn(n->log, "ntfy returned HTTP %ld", resp.status);
    if (!ok) {
        free(n->last_error);
        n->last_error = xstrdup(resp.error ? resp.error : "transport error");
        if (n->log) log_warn(n->log, "ntfy notify failed: %s", n->last_error);
    }
    http_response_free(&resp);
    return success;
}

/* -- frontier (heap ordered by (priority, insertion counter)) -------------- */


void frontier_init(frontier_t *f, size_t maxsize)
{
    memset(f, 0, sizeof *f);
    f->maxsize = maxsize;
    map_init(&f->seen);
}

void frontier_free(frontier_t *f)
{
    for (size_t i = 0; i < f->len; i++) {
        free(f->heap[i].url);
        free(f->heap[i].base);
    }
    free(f->heap);
    f->heap = NULL;
    f->len = f->cap = 0;
    map_free(&f->seen, NULL);
}

int fitem_less(const fitem *a, const fitem *b)
{
    if (a->prio != b->prio) return a->prio < b->prio;
    return a->counter < b->counter;
}

void frontier_sift_up(frontier_t *f, size_t i)
{
    while (i > 0) {
        size_t parent = (i - 1) / 2;
        if (!fitem_less(&f->heap[i], &f->heap[parent])) break;
        fitem tmp = f->heap[i];
        f->heap[i] = f->heap[parent];
        f->heap[parent] = tmp;
        i = parent;
    }
}

void frontier_sift_down(frontier_t *f, size_t i)
{
    for (;;) {
        size_t l = 2 * i + 1, r = 2 * i + 2, best = i;
        if (l < f->len && fitem_less(&f->heap[l], &f->heap[best])) best = l;
        if (r < f->len && fitem_less(&f->heap[r], &f->heap[best])) best = r;
        if (best == i) break;
        fitem tmp = f->heap[i];
        f->heap[i] = f->heap[best];
        f->heap[best] = tmp;
        i = best;
    }
}

int frontier_push(frontier_t *f, const char *url, int depth, const char *base)
{
    if (map_has(&f->seen, url)) return 0;
    if (f->len >= f->maxsize) { f->dropped++; return 0; }
    f->counter++;
    int prio = depth * 10;
    if (base && *base && is_same_host(url, base)) prio -= 3;
    if (f->len == f->cap) {
        f->cap = f->cap ? f->cap * 2 : 128;
        f->heap = xrealloc(f->heap, f->cap * sizeof(fitem));
    }
    fitem *it = &f->heap[f->len++];
    it->prio = prio;
    it->counter = f->counter;
    it->url = xstrdup(url);
    it->depth = depth;
    it->base = (base && *base) ? xstrdup(base) : NULL;
    frontier_sift_up(f, f->len - 1);
    map_put(&f->seen, url, (void *)1);
    return 1;
}

/* returns 1 and fills out on success */
int frontier_pop(frontier_t *f, char **url, int *depth, char **base)
{
    if (!f->len) return 0;
    *url = f->heap[0].url;
    *depth = f->heap[0].depth;
    *base = f->heap[0].base;
    map_del(&f->seen, *url);
    f->len--;
    if (f->len) {
        f->heap[0] = f->heap[f->len];
        frontier_sift_down(f, 0);
    }
    return 1;
}

/* -- checkpoint ------------------------------------------------------------ */
int checkpoint_save(const char *path, bloom_t *seen, frontier_t *frontier,
                           long page_count, stats_t *stats, vec_t *seeds,
                           map_t *domain_pages, logger_t *log)
{
    sb_t b;
    sb_init(&b);
    sb_appendf(&b, "{\"version\": \"%s\", \"bloom\": {\"num_bits\": %llu, \"num_hashes\": %d, "
                   "\"capacity\": %llu, \"error_rate\": ",
               VERSION, (unsigned long long)seen->num_bits, seen->num_hashes,
               (unsigned long long)seen->capacity);
    json_write_double(&b, seen->error_rate);
    sb_appendf(&b, ", \"added\": %llu, \"bits_b64\": ", (unsigned long long)seen->added);
    char *b64 = base64_encode(seen->bits, (size_t)(seen->num_bits / 8));
    json_write_string(&b, b64);
    free(b64);
    sb_append(&b, "}, \"frontier\": [");
    for (size_t i = 0; i < frontier->len; i++) {
        if (i) sb_append(&b, ", ");
        sb_append(&b, "[");
        json_write_string(&b, frontier->heap[i].url);
        sb_appendf(&b, ", %d, ", frontier->heap[i].depth);
        if (frontier->heap[i].base) json_write_string(&b, frontier->heap[i].base);
        else sb_append(&b, "null");
        sb_append(&b, "]");
    }
    sb_appendf(&b, "], \"page_count\": %ld, \"stats\": ", page_count);
    stats_to_json(&b, stats, 1);
    sb_append(&b, ", \"seeds\": [");
    for (size_t i = 0; i < seeds->len; i++) {
        if (i) sb_append(&b, ", ");
        json_write_string(&b, seeds->items[i]);
    }
    sb_append(&b, "], \"domain_pages\": ");
    json_write_map(&b, domain_pages, 0);
    sb_append(&b, "}");

    char *tmp = xmalloc(strlen(path) + 5);
    snprintf(tmp, strlen(path) + 5, "%s.tmp", path);
    FILE *fp = fopen(tmp, "w");
    int ok = 0;
    if (fp) {
        fputs(b.buf, fp);
        fclose(fp);
        ok = rename(tmp, path) == 0;
        if (!ok) remove(tmp);
    }
    if (!ok && log) log_warn(log, "checkpoint save failed: cannot write %s", path);
    free(tmp);
    sb_free(&b);
    return ok;
}

json_val *checkpoint_load(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;
    fseek(fp, 0, SEEK_END);
    long n = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (n < 0) { fclose(fp); return NULL; }
    char *buf = xmalloc((size_t)n + 1);
    size_t got = fread(buf, 1, (size_t)n, fp);
    fclose(fp);
    buf[got] = '\0';
    json_val *v = json_parse(buf);
    free(buf);
    return v;
}
