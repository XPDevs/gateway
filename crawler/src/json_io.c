#include "crawler.h"

/* JSON writer and reader (json.dumps / json.loads equivalents) */
/* ==========================================================================
 * Section 4 - JSON writing (json.dumps compatible) and reading
 * ========================================================================== */

/* json.dumps(s, ensure_ascii=False) for a string */
void json_write_string(sb_t *b, const char *s)
{
    sb_putc(b, '"');
    for (const unsigned char *p = (const unsigned char *)(s ? s : ""); *p; p++) {
        switch (*p) {
        case '"':  sb_append(b, "\\\""); break;
        case '\\': sb_append(b, "\\\\"); break;
        case '\n': sb_append(b, "\\n");  break;
        case '\r': sb_append(b, "\\r");  break;
        case '\t': sb_append(b, "\\t");  break;
        case '\b': sb_append(b, "\\b");  break;
        case '\f': sb_append(b, "\\f");  break;
        default:
            if (*p < 0x20) sb_appendf(b, "\\u%04x", *p);
            else sb_putc(b, (char)*p);
        }
    }
    sb_putc(b, '"');
}



void json_write_double(sb_t *b, double v)
{
    char tmp[40];
    if (v == (double)(long long)v && fabs(v) < 1e15) { sb_appendf(b, "%lld", (long long)v); return; }
    snprintf(tmp, sizeof tmp, "%.17g", v);
    sb_append(b, tmp);
}

/* -- generic JSON value tree ---------------------------------------------- */


json_val *json_new(json_type t)
{
    json_val *v = xcalloc(1, sizeof(json_val));
    v->t = t;
    return v;
}

void json_free(json_val *v)
{
    if (!v) return;
    switch (v->t) {
    case JSON_STR: free(v->str); break;
    case JSON_ARR:
        for (size_t i = 0; i < v->n; i++) json_free(v->items[i]);
        free(v->items);
        break;
    case JSON_OBJ:
        for (size_t i = 0; i < v->n; i++) {
            free(v->keys[i]);
            json_free(v->items[i]);
        }
        free(v->keys);
        free(v->items);
        break;
    default: break;
    }
    free(v);
}


void jp_ws(jparser *j)
{
    while (*j->p && (*j->p == ' ' || *j->p == '\t' || *j->p == '\n' || *j->p == '\r')) j->p++;
}

json_val *jp_value(jparser *j, int depth);

void utf8_put(sb_t *b, uint32_t cp)
{
    if (cp < 0x80) sb_putc(b, (char)cp);
    else if (cp < 0x800) {
        sb_putc(b, (char)(0xC0 | (cp >> 6)));
        sb_putc(b, (char)(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        sb_putc(b, (char)(0xE0 | (cp >> 12)));
        sb_putc(b, (char)(0x80 | ((cp >> 6) & 0x3F)));
        sb_putc(b, (char)(0x80 | (cp & 0x3F)));
    } else {
        sb_putc(b, (char)(0xF0 | (cp >> 18)));
        sb_putc(b, (char)(0x80 | ((cp >> 12) & 0x3F)));
        sb_putc(b, (char)(0x80 | ((cp >> 6) & 0x3F)));
        sb_putc(b, (char)(0x80 | (cp & 0x3F)));
    }
}

int hex4(const char *p, uint32_t *out)
{
    uint32_t v = 0;
    for (int i = 0; i < 4; i++) {
        char c = p[i];
        v <<= 4;
        if (c >= '0' && c <= '9') v |= (uint32_t)(c - '0');
        else if (c >= 'a' && c <= 'f') v |= (uint32_t)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') v |= (uint32_t)(c - 'A' + 10);
        else return 0;
    }
    *out = v;
    return 1;
}

char *jp_string(jparser *j)
{
    if (*j->p != '"') { j->err = 1; return NULL; }
    j->p++;
    sb_t b;
    sb_init(&b);
    while (*j->p && *j->p != '"') {
        if (*j->p == '\\') {
            j->p++;
            char c = *j->p;
            switch (c) {
            case '"': sb_putc(&b, '"'); j->p++; break;
            case '\\': sb_putc(&b, '\\'); j->p++; break;
            case '/': sb_putc(&b, '/'); j->p++; break;
            case 'b': sb_putc(&b, '\b'); j->p++; break;
            case 'f': sb_putc(&b, '\f'); j->p++; break;
            case 'n': sb_putc(&b, '\n'); j->p++; break;
            case 'r': sb_putc(&b, '\r'); j->p++; break;
            case 't': sb_putc(&b, '\t'); j->p++; break;
            case 'u': {
                uint32_t cp;
                if (!hex4(j->p + 1, &cp)) { j->err = 1; sb_free(&b); return NULL; }
                j->p += 5;
                if (cp >= 0xD800 && cp <= 0xDBFF && j->p[0] == '\\' && j->p[1] == 'u') {
                    uint32_t lo;
                    if (hex4(j->p + 2, &lo) && lo >= 0xDC00 && lo <= 0xDFFF) {
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        j->p += 6;
                    }
                }
                utf8_put(&b, cp);
                break;
            }
            default: j->err = 1; sb_free(&b); return NULL;
            }
        } else {
            sb_putc(&b, *j->p++);
        }
    }
    if (*j->p != '"') { j->err = 1; sb_free(&b); return NULL; }
    j->p++;
    return sb_detach(&b);
}

json_val *jp_value(jparser *j, int depth)
{
    if (depth > 512) { j->err = 1; return NULL; }
    jp_ws(j);
    char c = *j->p;
    if (c == '{') {
        j->p++;
        json_val *v = json_new(JSON_OBJ);
        size_t cap = 0;
        jp_ws(j);
        if (*j->p == '}') { j->p++; return v; }
        for (;;) {
            jp_ws(j);
            char *key = jp_string(j);
            if (!key) { json_free(v); return NULL; }
            jp_ws(j);
            if (*j->p != ':') { free(key); j->err = 1; json_free(v); return NULL; }
            j->p++;
            json_val *val = jp_value(j, depth + 1);
            if (!val) { free(key); json_free(v); return NULL; }
            if (v->n == cap) {
                cap = cap ? cap * 2 : 8;
                v->items = xrealloc(v->items, cap * sizeof(void *));
                v->keys = xrealloc(v->keys, cap * sizeof(char *));
            }
            v->keys[v->n] = key;
            v->items[v->n] = val;
            v->n++;
            jp_ws(j);
            if (*j->p == ',') { j->p++; continue; }
            if (*j->p == '}') { j->p++; break; }
            j->err = 1;
            json_free(v);
            return NULL;
        }
        return v;
    }
    if (c == '[') {
        j->p++;
        json_val *v = json_new(JSON_ARR);
        size_t cap = 0;
        jp_ws(j);
        if (*j->p == ']') { j->p++; return v; }
        for (;;) {
            json_val *val = jp_value(j, depth + 1);
            if (!val) { json_free(v); return NULL; }
            if (v->n == cap) {
                cap = cap ? cap * 2 : 8;
                v->items = xrealloc(v->items, cap * sizeof(void *));
            }
            v->items[v->n++] = val;
            jp_ws(j);
            if (*j->p == ',') { j->p++; continue; }
            if (*j->p == ']') { j->p++; break; }
            j->err = 1;
            json_free(v);
            return NULL;
        }
        return v;
    }
    if (c == '"') {
        char *s = jp_string(j);
        if (!s) return NULL;
        json_val *v = json_new(JSON_STR);
        v->str = s;
        return v;
    }
    if (!strncmp(j->p, "true", 4)) { j->p += 4; json_val *v = json_new(JSON_BOOL); v->bval = 1; return v; }
    if (!strncmp(j->p, "false", 5)) { j->p += 5; json_val *v = json_new(JSON_BOOL); v->bval = 0; return v; }
    if (!strncmp(j->p, "null", 4)) { j->p += 4; return json_new(JSON_NULL); }
    if (c == '-' || (c >= '0' && c <= '9')) {
        const char *start = j->p;
        if (*j->p == '-') j->p++;
        while (*j->p >= '0' && *j->p <= '9') j->p++;
        int is_int = 1;
        if (*j->p == '.') { is_int = 0; j->p++; while (*j->p >= '0' && *j->p <= '9') j->p++; }
        if (*j->p == 'e' || *j->p == 'E') {
            is_int = 0;
            j->p++;
            if (*j->p == '+' || *j->p == '-') j->p++;
            while (*j->p >= '0' && *j->p <= '9') j->p++;
        }
        char *tmp = xstrndup(start, (size_t)(j->p - start));
        json_val *v = json_new(JSON_NUM);
        v->num = strtod(tmp, NULL);
        v->is_int = is_int;
        v->ival = strtoll(tmp, NULL, 10);
        free(tmp);
        return v;
    }
    j->err = 1;
    return NULL;
}

json_val *json_parse(const char *text)
{
    jparser j = { text, 0 };
    json_val *v = jp_value(&j, 0);
    if (!v) return NULL;
    if (j.err) { json_free(v); return NULL; }
    return v;
}

json_val *json_obj_get(const json_val *o, const char *key)
{
    if (!o || o->t != JSON_OBJ) return NULL;
    for (size_t i = 0; i < o->n; i++)
        if (strcmp(o->keys[i], key) == 0) return o->items[i];
    return NULL;
}

const char *json_as_str(const json_val *v)
{
    return (v && v->t == JSON_STR) ? v->str : NULL;
}
