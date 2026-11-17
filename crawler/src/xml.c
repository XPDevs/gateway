#include "crawler.h"

/* small XML pull scanner for sitemaps and feeds */
/* ==========================================================================
 * Section 10 - tiny XML pull scanner (ElementTree substitute for sitemaps/feeds)
 * ========================================================================== */


void xml_scan_free_event(xml_scan *x)
{
    free(x->name);
    x->name = NULL;
    attrs_free(&x->attrs);
    free(x->text);
    x->text = NULL;
}

int xml_scan_next(xml_scan *x)
{
    xml_scan_free_event(x);
    if (x->i >= x->n) { x->kind = 0; return 0; }

    if (x->s[x->i] != '<') {
        size_t start = x->i;
        while (x->i < x->n && x->s[x->i] != '<') x->i++;
        x->text = html_unescape(x->s + start, x->i - start);
        x->kind = 3;
        return 1;
    }
    /* markup */
    if (x->i + 4 <= x->n && !strncmp(x->s + x->i, "<!--", 4)) {
        size_t k = x->i + 4;
        while (k + 3 <= x->n && strncmp(x->s + k, "-->", 3)) k++;
        x->i = (k + 3 <= x->n) ? k + 3 : x->n;
        return xml_scan_next(x);
    }
    if (x->i + 9 <= x->n && !strncmp(x->s + x->i, "<![CDATA[", 9)) {
        size_t k = x->i + 9;
        size_t start = k;
        while (k + 3 <= x->n && strncmp(x->s + k, "]]>", 3)) k++;
        x->text = xstrndup(x->s + start, k - start);
        x->i = (k + 3 <= x->n) ? k + 3 : x->n;
        x->kind = 3;
        return 1;
    }
    if (x->i + 2 <= x->n && (x->s[x->i + 1] == '?' || x->s[x->i + 1] == '!')) {
        size_t k = x->i + 2;
        int in_subset = 0;
        while (k < x->n) {
            if (x->s[k] == '[') in_subset = 1;
            else if (x->s[k] == ']') in_subset = 0;
            else if (x->s[k] == '>' && !in_subset) { k++; break; }
            k++;
        }
        x->i = k;
        return xml_scan_next(x);
    }
    if (x->i + 2 <= x->n && x->s[x->i + 1] == '/') {
        size_t k = x->i + 2;
        size_t ns = k;
        while (k < x->n && x->s[k] != '>') k++;
        char *raw = xstrndup(x->s + ns, k - ns);
        char *trimmed = str_strip(raw);
        free(raw);
        char *colon = strrchr(trimmed, ':');
        char *local = str_lower(colon ? colon + 1 : trimmed);
        free(trimmed);
        x->name = local;
        x->i = (k < x->n) ? k + 1 : x->n;
        x->kind = 2;
        return 1;
    }
    /* start tag */
    size_t k = x->i + 1;
    size_t ns = k;
    while (k < x->n && !is_space_byte(x->s[k]) && x->s[k] != '>' && x->s[k] != '/') k++;
    char *raw = xstrndup(x->s + ns, k - ns);
    char *colon = strrchr(raw, ':');
    x->name = str_lower(colon ? colon + 1 : raw);
    free(raw);

    size_t cap = 0;
    int self_closing = 0;
    for (;;) {
        while (k < x->n && is_space_byte(x->s[k])) k++;
        if (k >= x->n) break;
        if (x->s[k] == '>') { k++; break; }
        if (x->s[k] == '/') {
            if (k + 1 < x->n && x->s[k + 1] == '>') { self_closing = 1; k += 2; break; }
            k++;
            continue;
        }
        size_t an = k;
        while (k < x->n && x->s[k] != '=' && x->s[k] != '>' &&
               !is_space_byte(x->s[k]) && x->s[k] != '/') k++;
        char *aname = str_lower_heap(xstrndup(x->s + an, k - an));
        while (k < x->n && is_space_byte(x->s[k])) k++;
        char *aval = xstrdup("");
        if (k < x->n && x->s[k] == '=') {
            k++;
            while (k < x->n && is_space_byte(x->s[k])) k++;
            if (k < x->n && (x->s[k] == '"' || x->s[k] == '\'')) {
                char q = x->s[k++];
                size_t vs = k;
                while (k < x->n && x->s[k] != q) k++;
                free(aval);
                aval = html_unescape(x->s + vs, k - vs);
                if (k < x->n) k++;
            } else {
                size_t vs = k;
                while (k < x->n && !is_space_byte(x->s[k]) && x->s[k] != '>') k++;
                free(aval);
                aval = html_unescape(x->s + vs, k - vs);
            }
        }
        if (x->attrs.n == cap) {
            cap = cap ? cap * 2 : 8;
            x->attrs.v = xrealloc(x->attrs.v, cap * sizeof(attr_t));
        }
        x->attrs.v[x->attrs.n].name = aname;
        x->attrs.v[x->attrs.n].val = aval;
        x->attrs.n++;
    }
    x->i = (k <= x->n) ? k : x->n;
    if (self_closing) {
        /* emit start now, remember the synthetic end for the next call */
    }
    x->kind = 1;
    return 1;
}

/* sitemap: collect ("index"|"url", loc) entries */

void sitemap_collect(const char *data, size_t len, vec_t *out, int max_depth, int depth)
{
    if (depth > max_depth || !len) return;
    xml_scan x;
    memset(&x, 0, sizeof x);
    x.s = data;
    x.n = len;
    char *root = NULL;
    int in_loc = 0;
    sb_t text;
    sb_init(&text);
    vec_t stack;
    vec_init(&stack);
    while (xml_scan_next(&x)) {
        if (x.kind == 1) {
            char *nm = xstrdup(x.name);
            vec_push(&stack, nm);
            if (!root && stack.len == 1) root = xstrdup(x.name);
            if (!strcmp(x.name, "loc")) { in_loc = 1; sb_clear(&text); }
        } else if (x.kind == 2) {
            if (!strcmp(x.name, "loc") && in_loc) {
                char *loc = str_strip(text.buf ? text.buf : "");
                if (*loc && root) {
                    sitemap_entry *e = xcalloc(1, sizeof(sitemap_entry));
                    e->kind = xstrdup(!strcmp(root, "sitemapindex") ? "index" : "url");
                    e->url = loc;
                    vec_push(out, e);
                } else {
                    free(loc);
                }
                in_loc = 0;
                sb_clear(&text);
            }
            if (stack.len) {
                char *nm = vec_pop(&stack);
                free(nm);
            }
        } else if (x.kind == 3) {
            if (in_loc) sb_append(&text, x.text);
        }
    }
    sb_free(&text);
    vec_free(&stack, free);
    free(root);
    xml_scan_free_event(&x);
}


void feed_collect(const char *data, size_t len, vec_t *out)
{
    xml_scan x;
    memset(&x, 0, sizeof x);
    x.s = data;
    x.n = len;
    char *root = NULL;
    vec_t stack;
    vec_init(&stack);
    int item_depth = 0;          /* >0 while inside item/entry */
    char *cur_title = NULL, *cur_link = NULL;
    sb_t text;
    sb_init(&text);
    int capture = 0;
    char *link_href = NULL, *link_rel = NULL;

    while (xml_scan_next(&x)) {
        if (x.kind == 1) {
            if (!root) root = xstrdup(x.name);
            vec_push(&stack, xstrdup(x.name));
            if (!strcmp(x.name, "item") || !strcmp(x.name, "entry")) {
                item_depth++;
                free(cur_title); cur_title = NULL;
                free(cur_link); cur_link = NULL;
                free(link_href); link_href = NULL;
                free(link_rel); link_rel = NULL;
            } else if (item_depth) {
                if (!strcmp(x.name, "title")) { capture = 1; sb_clear(&text); }
                else if (!strcmp(x.name, "link")) {
                    const char *href = attrs_get(&x.attrs, "href");
                    const char *rel = attrs_get(&x.attrs, "rel");
                    free(link_href);
                    link_href = href ? xstrdup(href) : NULL;
                    free(link_rel);
                    link_rel = rel ? xstrdup(rel) : NULL;
                    capture = 1;
                    sb_clear(&text);
                }
            }
        } else if (x.kind == 2) {
            if (item_depth) {
                if (!strcmp(x.name, "title") && capture) {
                    char *t = str_strip(text.buf ? text.buf : "");
                    if (*t) { free(cur_title); cur_title = t; } else free(t);
                    capture = 0;
                } else if (!strcmp(x.name, "link") && capture) {
                    /* RSS: <link>text</link>; Atom: <link href="..."/> */
                    char *t = str_strip(text.buf ? text.buf : "");
                    if (*t && !link_href) {
                        free(cur_link);
                        cur_link = t;
                    } else {
                        free(t);
                        if (link_href) {
                            const char *rel = link_rel ? link_rel : "alternate";
                            if ((!strcmp(rel, "alternate") || !*rel) && !cur_link)
                                cur_link = xstrdup(link_href);
                        }
                    }
                    capture = 0;
                    free(link_href); link_href = NULL;
                    free(link_rel); link_rel = NULL;
                }
            }
            if (!strcmp(x.name, "item") || !strcmp(x.name, "entry")) {
                if (item_depth > 0) item_depth--;
                if (item_depth == 0 && cur_link) {
                    feed_item *fi = xcalloc(1, sizeof(feed_item));
                    fi->title = cur_title ? cur_title : xstrdup("");
                    fi->link = cur_link;
                    vec_push(out, fi);
                    cur_title = NULL;
                    cur_link = NULL;
                }
            }
            if (stack.len) free(vec_pop(&stack));
        } else if (x.kind == 3) {
            if (capture) sb_append(&text, x.text);
        }
    }
    free(cur_title);
    free(cur_link);
    free(link_href);
    free(link_rel);
    sb_free(&text);
    vec_free(&stack, free);
    free(root);
    xml_scan_free_event(&x);
}
