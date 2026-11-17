#include "crawler.h"

/* memory helpers, string builder, vectors, hash maps, LRU, heap */
/* ==========================================================================
 * Section 1 - small runtime: strings, vectors, hash maps, LRU, heap, log
 * ========================================================================== */

void *xmalloc(size_t n)
{
    void *p = malloc(n ? n : 1);
    if (!p) { fprintf(stderr, "out of memory\n"); exit(1); }
    return p;
}

void *xcalloc(size_t n, size_t s)
{
    void *p = calloc(n ? n : 1, s ? s : 1);
    if (!p) { fprintf(stderr, "out of memory\n"); exit(1); }
    return p;
}

void *xrealloc(void *p, size_t n)
{
    void *q = realloc(p, n ? n : 1);
    if (!q) { fprintf(stderr, "out of memory\n"); exit(1); }
    return q;
}

char *xstrdup(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *d = xmalloc(n);
    memcpy(d, s, n);
    return d;
}

char *xstrndup(const char *s, size_t n)
{
    char *d = xmalloc(n + 1);
    memcpy(d, s, n);
    d[n] = '\0';
    return d;
}

/* -- growable string buffer ------------------------------------------------- */

void sb_init(sb_t *b)
{
    b->cap = 64;
    b->len = 0;
    b->buf = xmalloc(b->cap);
    b->buf[0] = '\0';
}

void sb_free(sb_t *b)
{
    free(b->buf);
    b->buf = NULL;
    b->len = b->cap = 0;
}

void sb_reserve(sb_t *b, size_t need)
{
    if (b->len + need + 1 <= b->cap) return;
    size_t cap = b->cap ? b->cap : 64;
    while (cap < b->len + need + 1) cap *= 2;
    b->buf = xrealloc(b->buf, cap);
    b->cap = cap;
}

void sb_putc(sb_t *b, char c)
{
    sb_reserve(b, 1);
    b->buf[b->len++] = c;
    b->buf[b->len] = '\0';
}

void sb_append(sb_t *b, const char *s)
{
    if (!s) return;
    size_t n = strlen(s);
    sb_reserve(b, n);              /* guarantees n + 1 writable bytes */
    char *dst = b->buf + b->len;   /* copy through a fresh pointer: keeps GCC's
                                      bounds analysis from guessing wrong */
    memcpy(dst, s, n);
    dst[n] = '\0';
    b->len += n;
}

void sb_appendn(sb_t *b, const char *s, size_t n)
{
    if (!s || !n) return;
    sb_reserve(b, n);
    char *dst = b->buf + b->len;
    memcpy(dst, s, n);
    b->len += n;
    b->buf[b->len] = '\0';
}

void sb_appendf(sb_t *b, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    va_list ap2;
    va_copy(ap2, ap);
    int need = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (need <= 0) { va_end(ap2); return; }
    sb_reserve(b, (size_t)need);
    vsnprintf(b->buf + b->len, (size_t)need + 1, fmt, ap2);
    va_end(ap2);
    b->len += (size_t)need;
}

void sb_clear(sb_t *b)
{
    b->len = 0;
    if (b->buf) b->buf[0] = '\0';
}

char *sb_detach(sb_t *b)
{
    char *p = b->buf;
    if (!p) return xstrdup("");
    b->buf = NULL;
    b->len = b->cap = 0;
    return p;
}

/* -- vector of owned pointers ---------------------------------------------- */

void vec_init(vec_t *v) { v->items = NULL; v->len = v->cap = 0; }

void vec_push(vec_t *v, void *item)
{
    if (v->len == v->cap) {
        v->cap = v->cap ? v->cap * 2 : 16;
        v->items = xrealloc(v->items, v->cap * sizeof(void *));
    }
    v->items[v->len++] = item;
}

void *vec_pop(vec_t *v)
{
    if (!v->len) return NULL;
    return v->items[--v->len];
}

void vec_free(vec_t *v, void (*fn)(void *))
{
    if (fn) for (size_t i = 0; i < v->len; i++) fn(v->items[i]);
    free(v->items);
    v->items = NULL;
    v->len = v->cap = 0;
}

/* -- string -> void* hash map (open addressing, FNV-1a, tombstones) --------- */



uint64_t fnv1a(const char *s, size_t n)
{
    uint64_t h = 1469598103934665603ULL;
    for (size_t i = 0; i < n; i++) {
        h ^= (unsigned char)s[i];
        h *= 1099511628211ULL;
    }
    return h;
}

uint64_t fnv1a_str(const char *s) { return fnv1a(s, strlen(s)); }

void map_init(map_t *m)
{
    m->cap = 16;
    m->live = m->dead = 0;
    m->ent = xcalloc(m->cap, sizeof(ment_t));
}

void map_free(map_t *m, void (*valfree)(void *))
{
    if (!m->ent) return;
    for (size_t i = 0; i < m->cap; i++) {
        if (m->ent[i].state == MAP_USED) {
            free(m->ent[i].key);
            if (valfree) valfree(m->ent[i].val);
        }
    }
    free(m->ent);
    m->ent = NULL;
    m->cap = m->live = m->dead = 0;
}

ment_t *map_slot(map_t *m, const char *key, uint64_t h)
{
    size_t mask = m->cap - 1;
    size_t i = (size_t)(h & mask);
    ment_t *first_del = NULL;
    for (;;) {
        ment_t *e = &m->ent[i];
        if (e->state == MAP_EMPTY)
            return first_del ? first_del : e;
        if (e->state == MAP_DEL) {
            if (!first_del) first_del = e;
        } else if (e->hash == h && strcmp(e->key, key) == 0) {
            return e;
        }
        i = (i + 1) & mask;
    }
}

void map_grow(map_t *m)
{
    size_t ncap = m->cap * 2;
    ment_t *old = m->ent;
    size_t ocap = m->cap;
    m->ent = xcalloc(ncap, sizeof(ment_t));
    m->cap = ncap;
    m->live = 0;
    m->dead = 0;
    for (size_t i = 0; i < ocap; i++) {
        if (old[i].state == MAP_USED) {
            ment_t *slot = map_slot(m, old[i].key, old[i].hash);
            slot->state = MAP_USED;
            slot->key = old[i].key;
            slot->val = old[i].val;
            slot->hash = old[i].hash;
            m->live++;
        }
    }
    free(old);
}

/* Returns previous value or NULL. Key is copied. */
void *map_put(map_t *m, const char *key, void *val)
{
    if ((m->live + m->dead) * 4 >= m->cap * 3) {
        if (m->dead > m->live) map_grow(m); else map_grow(m);
    }
    uint64_t h = fnv1a_str(key);
    ment_t *e = map_slot(m, key, h);
    if (e->state == MAP_USED) {
        void *old = e->val;
        e->val = val;
        return old;
    }
    if (e->state == MAP_DEL) m->dead--;
    e->state = MAP_USED;
    e->hash = h;
    e->key = xstrdup(key);
    e->val = val;
    m->live++;
    return NULL;
}

void *map_get(map_t *m, const char *key)
{
    if (!m->ent || !m->live) return NULL;
    uint64_t h = fnv1a_str(key);
    ment_t *e = map_slot(m, key, h);
    return e->state == MAP_USED ? e->val : NULL;
}

int map_has(map_t *m, const char *key)
{
    if (!m->ent || !m->live) return 0;
    ment_t *e = map_slot(m, key, fnv1a_str(key));
    return e->state == MAP_USED;
}

/* Increment a counter held directly in the value slot. */
long map_incr(map_t *m, const char *key, long by)
{
    uint64_t h = fnv1a_str(key);
    if ((m->live + m->dead) * 4 >= m->cap * 3) map_grow(m);
    ment_t *e = map_slot(m, key, h);
    long cur;
    if (e->state == MAP_USED) {
        cur = (long)(intptr_t)e->val;
    } else {
        if (e->state == MAP_DEL) m->dead--;
        e->state = MAP_USED;
        e->hash = h;
        e->key = xstrdup(key);
        cur = 0;
        m->live++;
    }
    cur += by;
    e->val = (void *)(intptr_t)cur;
    return cur;
}

int map_del(map_t *m, const char *key)
{
    if (!m->ent || !m->live) return 0;
    ment_t *e = map_slot(m, key, fnv1a_str(key));
    if (e->state != MAP_USED) return 0;
    free(e->key);
    e->key = NULL;
    e->val = NULL;
    e->state = MAP_DEL;
    m->live--;
    m->dead++;
    return 1;
}

/* -- LRU (bounded dict) ---------------------------------------------------- */


void lru_init(lru_t *l, size_t maxsize, void (*valfree)(void *))
{
    map_init(&l->index);
    l->head = l->tail = NULL;
    l->maxsize = maxsize;
    l->len = 0;
    l->valfree = valfree;
}

void lru_unlink(lru_t *l, lru_node *n)
{
    if (n->prev) n->prev->next = n->next; else l->head = n->next;
    if (n->next) n->next->prev = n->prev; else l->tail = n->prev;
    n->prev = n->next = NULL;
}

void lru_splice_head(lru_t *l, lru_node *n)
{
    n->prev = NULL;
    n->next = l->head;
    if (l->head) l->head->prev = n;
    l->head = n;
    if (!l->tail) l->tail = n;
}

void lru_free(lru_t *l)
{
    lru_node *n = l->head;
    while (n) {
        lru_node *nx = n->next;
        free(n->key);
        if (l->valfree) l->valfree(n->val);
        free(n);
        n = nx;
    }
    l->head = l->tail = NULL;
    l->len = 0;
    map_free(&l->index, NULL);
}

lru_node *lru_touch(lru_t *l, const char *key)
{
    lru_node *n = map_get(&l->index, key);
    if (!n) return NULL;
    lru_unlink(l, n);
    lru_splice_head(l, n);
    return n;
}

void *lru_get(lru_t *l, const char *key)
{
    lru_node *n = lru_touch(l, key);
    return n ? n->val : NULL;
}

/* Insert or replace. Takes ownership of val. */
void lru_put(lru_t *l, const char *key, void *val)
{
    lru_node *n = map_get(&l->index, key);
    if (n) {
        if (l->valfree) l->valfree(n->val);
        n->val = val;
        lru_unlink(l, n);
        lru_splice_head(l, n);
        return;
    }
    n = xcalloc(1, sizeof(lru_node));
    n->key = xstrdup(key);
    n->val = val;
    lru_splice_head(l, n);
    map_put(&l->index, key, n);
    l->len++;
    while (l->len > l->maxsize && l->tail) {
        lru_node *victim = l->tail;
        lru_unlink(l, victim);
        map_del(&l->index, victim->key);
        free(victim->key);
        if (l->valfree) l->valfree(victim->val);
        free(victim);
        l->len--;
    }
}

/* -- misc string helpers --------------------------------------------------- */
/* lowercase a heap string in place (takes ownership) */
char *str_lower_heap(char *s)
{
    if (!s) return NULL;
    for (char *p = s; *p; p++) *p = (char)tolower((unsigned char)*p);
    return s;
}

char *str_lower(const char *s)
{
    if (!s) return NULL;
    char *d = xstrdup(s);
    for (char *p = d; *p; p++) *p = (char)tolower((unsigned char)*p);
    return d;
}

int is_space_byte(int c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* strip() - ASCII whitespace plus the UTF-8 whitespace Python's str.strip()
 * honours. Returns a freshly allocated copy. */
size_t utf8_space_len(const char *s)
{
    unsigned char c = (unsigned char)s[0];
    if (c == 0) return 0;
    if (c < 0x80) return is_space_byte(c) ? 1 : 0;
    unsigned char b = (unsigned char)s[1];
    if (!b) return 0;
    if (c == 0xC2) return (b == 0xA0) ? 2 : 0;                    /* NBSP */
    unsigned char d = (unsigned char)s[2];
    if (!d) return 0;
    if (c == 0xE1 && b == 0x9A && d == 0x80) return 3;
    if (c == 0xE2 && b == 0x80) {
        if (d == 0x80 || d == 0x81 || d == 0x82 || d == 0x83 || d == 0x84 ||
            d == 0x85 || d == 0x86 || d == 0x87 || d == 0x88 || d == 0x89 ||
            d == 0x8A || d == 0x8B || d == 0xA8 || d == 0xA9 || d == 0xAF)
            return 3;
        if (d >= 0x9F && d <= 0xAF) return 3;
    }
    if (c == 0xE3 && b == 0x80 && d == 0x80) return 3;
    return 0;
}

/* Python str.strip() over ASCII + the common Unicode whitespace */
char *str_strip(const char *s)
{
    if (!s) return xstrdup("");
    size_t n = strlen(s);
    size_t start = 0;
    for (;;) {
        if (start >= n) break;
        unsigned char c = (unsigned char)s[start];
        if (c < 0x80) {
            if (is_space_byte(c)) { start++; continue; }
            break;
        }
        size_t l = utf8_space_len(s + start);
        if (l && start + l <= n) { start += l; continue; }
        break;
    }
    size_t end = n;
    while (end > start) {
        unsigned char c = (unsigned char)s[end - 1];
        if (c < 0x80) {
            if (is_space_byte(c)) { end--; continue; }
            break;
        }
        int trimmed = 0;
        size_t lo = (end >= start + 4) ? end - 4 : start;
        for (size_t i = lo; i < end; i++) {
            size_t l = utf8_space_len(s + i);
            if (l && i + l == end) { end = i; trimmed = 1; break; }
        }
        if (!trimmed) break;
    }
    if (end < start) end = start;
    return xstrndup(s + start, end - start);
}

char *str_strip_inplace(char *s)
{
    if (!s) return NULL;
    char *t = str_strip(s);
    free(s);
    return t;
}

/* re.sub(r"\s+", " ", s) */
char *collapse_ws(const char *s)
{
    if (!s) return xstrdup("");
    sb_t out;
    sb_init(&out);
    int pending_space = 0;
    const char *p = s;
    int wrote = 0;
    while (*p) {
        size_t l = utf8_space_len(p);
        if (l) { pending_space = 1; p += l; continue; }
        if (pending_space && wrote) sb_putc(&out, ' ');
        pending_space = 0;
        sb_putc(&out, *p);
        wrote = 1;
        p++;
    }
    return sb_detach(&out);
}

char *collapse_ws_trim(const char *s)
{
    char *c = collapse_ws(s);
    return str_strip_inplace(c);
}

/* Count UTF-8 codepoints. */
size_t utf8_len(const char *s)
{
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        if ((*p & 0xC0) != 0x80) n++;
    return n;
}

/* Truncate to at most n codepoints, appending tail when truncated.
 * Python: plain[:max(0, n-1)] + ("…" if unicode else ".") */
char *trunc_chars_add(const char *s, size_t n, int unicode, const char *tail_uni,
                             const char *tail_ascii)
{
    if (n == 0) return xstrdup("");
    size_t count = 0;
    const char *p = s;
    while (*p) {
        if ((*(const unsigned char *)p & 0xC0) != 0x80) {
            if (count == n) break;
            count++;
        }
        p++;
    }
    if (!*p) return xstrdup(s);
    size_t keep = n > 0 ? n - 1 : 0;
    size_t cnt = 0;
    const char *q = s;
    while (*q && cnt < keep) {
        q++;
        while ((*(const unsigned char *)q & 0xC0) == 0x80) q++;
        cnt++;
    }
    sb_t out;
    sb_init(&out);
    sb_appendn(&out, s, (size_t)(q - s));
    sb_append(&out, unicode ? tail_uni : tail_ascii);
    return sb_detach(&out);
}

char *trunc_chars(const char *s, size_t n)
{
    if (!s) return xstrdup("");
    size_t count = 0;
    const char *p = s;
    while (*p) {
        if ((*(const unsigned char *)p & 0xC0) != 0x80) {
            if (count == n) return xstrndup(s, (size_t)(p - s));
            count++;
        }
        p++;
    }
    return xstrdup(s);
}

/* Python round() - banker's rounding (half to even). */
double py_round(double x)
{
    double f = floor(x);
    double d = x - f;
    if (d > 0.5) return f + 1;
    if (d < 0.5) return f;
    /* exactly .5 */
    return (fmod(f, 2.0) == 0.0) ? f : f + 1;
}

long py_round_long(double x) { return (long)py_round(x); }

/* f"{n:,}" - thousands separators. */
void sb_append_thousands(sb_t *b, long long v)
{
    char tmp[32];
    snprintf(tmp, sizeof tmp, "%lld", v < 0 ? -(long long)v : (long long)v);
    if (v < 0) sb_putc(b, '-');
    size_t n = strlen(tmp);
    for (size_t i = 0; i < n; i++) {
        if (i && (n - i) % 3 == 0) sb_putc(b, ',');
        sb_putc(b, tmp[i]);
    }
}

char *fmt_thousands(long long v)
{
    sb_t b;
    sb_init(&b);
    sb_append_thousands(&b, v);
    return sb_detach(&b);
}

/* strftime-like wall clock for log headers. */
void now_string(char *out, size_t n)
{
    time_t t = time(NULL);
    struct tm tmv;
    localtime_r(&t, &tmv);
    strftime(out, n, "%Y-%m-%d %H:%M:%S", &tmv);
}
