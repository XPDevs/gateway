#include "crawler.h"

/* HTML tokenizer, entity decoding, metadata and visible text */
/* ==========================================================================
 * Section 6 - HTML tokenizer (html.parser equivalent) and entity decoding
 * ========================================================================== */



const char *attrs_get(const attrs_t *a, const char *name)
{
    /* html.parser keeps the last duplicate through dict(attrs) */
    const char *found = NULL;
    for (size_t i = 0; i < a->n; i++)
        if (strcmp(a->v[i].name, name) == 0) found = a->v[i].val;
    return found;
}

void attrs_free(attrs_t *a)
{
    for (size_t i = 0; i < a->n; i++) { free(a->v[i].name); free(a->v[i].val); }
    free(a->v);
    a->v = NULL;
    a->n = 0;
}


/* -- named character references (subset of HTML5, covers real-world markup) - */

const ent_t ENTITIES[] = {
    {"amp", "&"}, {"lt", "<"}, {"gt", ">"}, {"quot", "\""}, {"apos", "'"},
    {"nbsp", "\xc2\xa0"}, {"ensp", "\xe2\x80\x82"}, {"emsp", "\xe2\x80\x83"},
    {"thinsp", "\xe2\x80\x89"}, {"zwnj", "\xe2\x80\x8c"}, {"zwj", "\xe2\x80\x8d"},
    {"lrm", "\xe2\x80\x8e"}, {"rlm", "\xe2\x80\x8f"}, {"shy", "\xc2\xad"},
    {"copy", "\xc2\xa9"}, {"reg", "\xc2\xae"}, {"trade", "\xe2\x84\xa2"},
    {"deg", "\xc2\xb0"}, {"plusmn", "\xc2\xb1"}, {"sup2", "\xc2\xb2"},
    {"sup3", "\xc2\xb3"}, {"micro", "\xc2\xb5"}, {"para", "\xc2\xb6"},
    {"middot", "\xc2\xb7"}, {"frac14", "\xc2\xbc"}, {"frac12", "\xc2\xbd"},
    {"frac34", "\xc2\xbe"}, {"iquest", "\xc2\xbf"}, {"times", "\xc3\x97"},
    {"divide", "\xc3\xb7"}, {"iexcl", "\xc2\xa1"}, {"cent", "\xc2\xa2"},
    {"pound", "\xc2\xa3"}, {"curren", "\xc2\xa4"}, {"yen", "\xc2\xa5"},
    {"sect", "\xc2\xa7"}, {"uml", "\xc2\xa8"}, {"ordf", "\xc2\xaa"},
    {"laquo", "\xc2\xab"}, {"not", "\xc2\xac"}, {"macr", "\xc2\xaf"},
    {"acute", "\xc2\xb4"}, {"cedil", "\xc2\xb8"}, {"ordm", "\xc2\xba"},
    {"raquo", "\xc2\xbb"}, {"szlig", "\xc3\x9f"}, {"agrave", "\xc3\xa0"},
    {"aacute", "\xc3\xa1"}, {"acirc", "\xc3\xa2"}, {"atilde", "\xc3\xa3"},
    {"auml", "\xc3\xa4"}, {"aring", "\xc3\xa5"}, {"aelig", "\xc3\xa6"},
    {"ccedil", "\xc3\xa7"}, {"egrave", "\xc3\xa8"}, {"eacute", "\xc3\xa9"},
    {"ecirc", "\xc3\xaa"}, {"euml", "\xc3\xab"}, {"igrave", "\xc3\xac"},
    {"iacute", "\xc3\xad"}, {"icirc", "\xc3\xae"}, {"iuml", "\xc3\xaf"},
    {"ntilde", "\xc3\xb1"}, {"ograve", "\xc3\xb2"}, {"oacute", "\xc3\xb3"},
    {"ocirc", "\xc3\xb4"}, {"otilde", "\xc3\xb5"}, {"ouml", "\xc3\xb6"},
    {"oslash", "\xc3\xb8"}, {"ugrave", "\xc3\xb9"}, {"uacute", "\xc3\xba"},
    {"ucirc", "\xc3\xbb"}, {"uuml", "\xc3\xbc"}, {"yacute", "\xc3\xbd"},
    {"yuml", "\xc3\xbf"}, {"Agrave", "\xc3\x80"}, {"Aacute", "\xc3\x81"},
    {"Acirc", "\xc3\x82"}, {"Auml", "\xc3\x84"}, {"Ccedil", "\xc3\x87"},
    {"Egrave", "\xc3\x88"}, {"Eacute", "\xc3\x89"}, {"Ouml", "\xc3\x96"},
    {"Uuml", "\xc3\x9c"}, {"mdash", "\xe2\x80\x94"}, {"ndash", "\xe2\x80\x93"},
    {"horbar", "\xe2\x80\x95"}, {"lsquo", "\xe2\x80\x98"}, {"rsquo", "\xe2\x80\x99"},
    {"sbquo", "\xe2\x80\x9a"}, {"ldquo", "\xe2\x80\x9c"}, {"rdquo", "\xe2\x80\x9d"},
    {"bdquo", "\xe2\x80\x9e"}, {"dagger", "\xe2\x80\xa0"}, {"Dagger", "\xe2\x80\xa1"},
    {"bull", "\xe2\x80\xa2"}, {"hellip", "\xe2\x80\xa6"}, {"permil", "\xe2\x80\xb0"},
    {"prime", "\xe2\x80\xb2"}, {"Prime", "\xe2\x80\xb3"}, {"lsaquo", "\xe2\x80\xb9"},
    {"rsaquo", "\xe2\x80\xba"}, {"oline", "\xe2\x80\xbe"}, {"frasl", "\xe2\x81\x84"},
    {"euro", "\xe2\x82\xac"}, {"image", "\xe2\x84\x91"}, {"weierp", "\xe2\x84\x98"},
    {"real", "\xe2\x84\x9c"}, {"alefsym", "\xe2\x84\xb5"}, {"larr", "\xe2\x86\x90"},
    {"uarr", "\xe2\x86\x91"}, {"rarr", "\xe2\x86\x92"}, {"darr", "\xe2\x86\x93"},
    {"harr", "\xe2\x86\x94"}, {"crarr", "\xe2\x86\xb5"}, {"lArr", "\xe2\x87\x90"},
    {"rArr", "\xe2\x87\x92"}, {"hArr", "\xe2\x87\x94"}, {"forall", "\xe2\x88\x80"},
    {"part", "\xe2\x88\x82"}, {"exist", "\xe2\x88\x83"}, {"empty", "\xe2\x88\x85"},
    {"nabla", "\xe2\x88\x87"}, {"isin", "\xe2\x88\x88"}, {"notin", "\xe2\x88\x89"},
    {"ni", "\xe2\x88\x8b"}, {"prod", "\xe2\x88\x8f"}, {"sum", "\xe2\x88\x91"},
    {"minus", "\xe2\x88\x92"}, {"lowast", "\xe2\x88\x97"}, {"radic", "\xe2\x88\x9a"},
    {"prop", "\xe2\x88\x9d"}, {"infin", "\xe2\x88\x9e"}, {"ang", "\xe2\x88\xa0"},
    {"and", "\xe2\x88\xa7"}, {"or", "\xe2\x88\xa8"}, {"cap", "\xe2\x88\xa9"},
    {"cup", "\xe2\x88\xaa"}, {"int", "\xe2\x88\xab"}, {"there4", "\xe2\x88\xb4"},
    {"sim", "\xe2\x88\xbc"}, {"cong", "\xe2\x89\x85"}, {"asymp", "\xe2\x89\x88"},
    {"ne", "\xe2\x89\xa0"}, {"equiv", "\xe2\x89\xa1"}, {"le", "\xe2\x89\xa4"},
    {"ge", "\xe2\x89\xa5"}, {"sub", "\xe2\x8a\x82"}, {"sup", "\xe2\x8a\x83"},
    {"nsub", "\xe2\x8a\x84"}, {"sube", "\xe2\x8a\x86"}, {"supe", "\xe2\x8a\x87"},
    {"oplus", "\xe2\x8a\x95"}, {"otimes", "\xe2\x8a\x97"}, {"perp", "\xe2\x8a\xa5"},
    {"sdot", "\xe2\x8b\x85"}, {"lceil", "\xe2\x8c\x88"}, {"rceil", "\xe2\x8c\x89"},
    {"lfloor", "\xe2\x8c\x8a"}, {"rfloor", "\xe2\x8c\x8b"}, {"lang", "\xe2\x8c\xa9"},
    {"rang", "\xe2\x8c\xaa"}, {"loz", "\xe2\x97\x8a"}, {"spades", "\xe2\x99\xa0"},
    {"clubs", "\xe2\x99\xa3"}, {"hearts", "\xe2\x99\xa5"}, {"diams", "\xe2\x99\xa6"},
    {"OElig", "\xc5\x92"}, {"oelig", "\xc5\x93"}, {"Scaron", "\xc5\xa0"},
    {"scaron", "\xc5\xa1"}, {"Yuml", "\xc5\xb8"}, {"fnof", "\xc6\x92"},
    {"circ", "\xcb\x86"}, {"tilde", "\xcb\x9c"}, {"Alpha", "\xce\x91"},
    {"Beta", "\xce\x92"}, {"Gamma", "\xce\x93"}, {"Delta", "\xce\x94"},
    {"alpha", "\xce\xb1"}, {"beta", "\xce\xb2"}, {"gamma", "\xce\xb3"},
    {"delta", "\xce\xb4"}, {"epsilon", "\xce\xb5"}, {"pi", "\xcf\x80"},
    {"sigma", "\xcf\x83"}, {"omega", "\xcf\x89"}, {"Omega", "\xce\xa9"},
    {NULL, NULL}
};

void html_unescape_into(sb_t *out, const char *s, size_t n)
{
    size_t i = 0;
    while (i < n) {
        if (s[i] != '&') { sb_putc(out, s[i++]); continue; }
        /* numeric reference? "&#123;" or "&#x27;" */
        size_t j = i + 1;
        int base = 10;
        if (j < n && s[j] == '#') {
            j++;
            if (j < n && (s[j] == 'x' || s[j] == 'X')) { base = 16; j++; }
        } else {
            j = i + 1;      /* named reference path below */
            base = 0;
        }
        size_t digits_start = j;
        unsigned long cp = 0;
        int overflow = 0;
        while (base != 0 && j < n) {
            int c = (unsigned char)s[j];
            int d;
            if (c >= '0' && c <= '9') d = c - '0';
            else if (base == 16 && c >= 'a' && c <= 'f') d = c - 'a' + 10;
            else if (base == 16 && c >= 'A' && c <= 'F') d = c - 'A' + 10;
            else break;
            cp = cp * (unsigned)base + (unsigned)d;
            if (cp > 0x10FFFF) { overflow = 1; cp = 0x10FFFF; }
            j++;
        }
        if (base != 0 && j > digits_start && j < n && s[j] == ';') {
            (void)overflow;
            if (cp >= 1 && cp <= 0x10FFFF) utf8_put(out, (uint32_t)cp);
            i = j + 1;
            continue;
        }
        /* named reference? longest match, terminated by ';' */
        size_t semi = i + 1;
        while (semi < n && semi - i <= 32 && s[semi] != ';' && s[semi] != '&' &&
               s[semi] != ' ' && s[semi] != '<') semi++;
        int matched = 0;
        if (semi < n && s[semi] == ';' && semi > i + 1) {
            char name[34];
            size_t ln = semi - (i + 1);
            memcpy(name, s + i + 1, ln);
            name[ln] = '\0';
            for (int k = 0; ENTITIES[k].name; k++) {
                if (!strcmp(ENTITIES[k].name, name)) {
                    sb_append(out, ENTITIES[k].utf8);
                    i = semi + 1;
                    matched = 1;
                    break;
                }
            }
        }
        if (!matched) sb_putc(out, s[i++]);
    }
}

char *html_unescape(const char *s, size_t n)
{
    sb_t b;
    sb_init(&b);
    html_unescape_into(&b, s, n);
    return sb_detach(&b);
}

int tag_is_ws(int c) { return is_space_byte(c) || c == '\f'; }

void emit_data(const html_sink *sink, void *ctx, const char *p, size_t n, int decode)
{
    if (!n) return;
    if (decode && memchr(p, '&', n)) {
        char *d = html_unescape(p, n);
        sink->data(ctx, d, strlen(d));
        free(d);
    } else {
        sink->data(ctx, p, n);
    }
}

void html_parse(const char *src, size_t len, const html_sink *sink, void *ctx)
{
    size_t i = 0;
    size_t text_start = 0;
    while (i < len) {
        if (src[i] != '<') { i++; continue; }
        /* comment / doctype / processing instruction */
        if (i + 4 <= len && !strncmp(src + i, "<!--", 4)) {
            if (i > text_start) emit_data(sink, ctx, src + text_start, i - text_start, 1);
            const char *end = NULL;
            for (size_t k = i + 4; k + 3 <= len; k++) {
                if (!strncmp(src + k, "-->", 3)) { end = src + k + 3; break; }
            }
            i = end ? (size_t)(end - src) : len;
            text_start = i;
            continue;
        }
        if (i + 2 <= len && src[i + 1] == '!') {
            if (i > text_start) emit_data(sink, ctx, src + text_start, i - text_start, 1);
            const char *gt = memchr(src + i, '>', len - i);
            i = gt ? (size_t)(gt - src + 1) : len;
            text_start = i;
            continue;
        }
        if (i + 2 <= len && src[i + 1] == '?') {
            if (i > text_start) emit_data(sink, ctx, src + text_start, i - text_start, 1);
            const char *gt = memchr(src + i, '>', len - i);
            i = gt ? (size_t)(gt - src + 1) : len;
            text_start = i;
            continue;
        }
        /* end tag */
        if (i + 2 <= len && src[i + 1] == '/' && isalpha((unsigned char)src[i + 2])) {
            if (i > text_start) emit_data(sink, ctx, src + text_start, i - text_start, 1);
            size_t j = i + 2;
            size_t name_start = j;
            while (j < len && !tag_is_ws((unsigned char)src[j]) && src[j] != '>') j++;
            char *name = str_lower_heap(xstrndup(src + name_start, j - name_start));
            const char *gt = memchr(src + j, '>', len - j);
            i = gt ? (size_t)(gt - src + 1) : len;
            sink->endtag(ctx, name);
            free(name);
            text_start = i;
            continue;
        }
        /* start tag */
        if (i + 1 < len && isalpha((unsigned char)src[i + 1])) {
            if (i > text_start) emit_data(sink, ctx, src + text_start, i - text_start, 1);
            size_t j = i + 1;
            size_t name_start = j;
            while (j < len && !tag_is_ws((unsigned char)src[j]) && src[j] != '>' && src[j] != '/') j++;
            char *name = str_lower_heap(xstrndup(src + name_start, j - name_start));

            attrs_t attrs = { NULL, 0 };
            size_t cap = 0;
            int self_closed = 0;
            for (;;) {
                while (j < len && tag_is_ws((unsigned char)src[j])) j++;
                if (j >= len) break;
                if (src[j] == '>') { j++; break; }
                if (src[j] == '/') {
                    if (j + 1 < len && src[j + 1] == '>') { self_closed = 1; j += 2; break; }
                    j++;
                    continue;
                }
                size_t an = j;
                while (j < len && src[j] != '=' && src[j] != '>' &&
                       !tag_is_ws((unsigned char)src[j]) && src[j] != '/') j++;
                char *aname = str_lower_heap(xstrndup(src + an, j - an));
                while (j < len && tag_is_ws((unsigned char)src[j])) j++;
                char *aval = NULL;
                if (j < len && src[j] == '=') {
                    j++;
                    while (j < len && tag_is_ws((unsigned char)src[j])) j++;
                    if (j < len && (src[j] == '"' || src[j] == '\'')) {
                        char q = src[j++];
                        size_t vs = j;
                        while (j < len && src[j] != q) j++;
                        aval = html_unescape(src + vs, j - vs);
                        if (j < len) j++;
                    } else {
                        size_t vs = j;
                        while (j < len && !tag_is_ws((unsigned char)src[j]) && src[j] != '>') j++;
                        aval = html_unescape(src + vs, j - vs);
                    }
                } else {
                    aval = xstrdup("");
                }
                if (attrs.n == cap) {
                    cap = cap ? cap * 2 : 8;
                    attrs.v = xrealloc(attrs.v, cap * sizeof(attr_t));
                }
                attrs.v[attrs.n].name = aname;
                attrs.v[attrs.n].val = aval;
                attrs.n++;
            }

            /* raw-text elements swallow their content as data */
            int raw = !self_closed && (!strcmp(name, "script") || !strcmp(name, "style"));
            sink->starttag(ctx, name, &attrs);
            i = j;
            text_start = i;
            if (raw) {
                char close_tag[16];
                snprintf(close_tag, sizeof close_tag, "</%s", name);
                size_t cl = strlen(close_tag);
                size_t k = i;
                size_t found = (size_t)-1;
                while (k + cl <= len) {
                    if (src[k] == '<' && !strncasecmp(src + k, close_tag, cl) &&
                        (k + cl == len || src[k + cl] == '>' ||
                         tag_is_ws((unsigned char)src[k + cl]))) {
                        found = k;
                        break;
                    }
                    k++;
                }
                size_t content_end = (found == (size_t)-1) ? len : found;
                if (content_end > i) emit_data(sink, ctx, src + i, content_end - i, 0);
                if (found == (size_t)-1) {
                    i = len;
                } else {
                    const char *gt = memchr(src + found, '>', len - found);
                    i = gt ? (size_t)(gt - src + 1) : len;
                    sink->endtag(ctx, name);
                }
                text_start = i;
            }
            attrs_free(&attrs);
            free(name);
            continue;
        }
        /* a lone '<' is data */
        i++;
    }
    if (len > text_start) emit_data(sink, ctx, src + text_start, len - text_start, 1);
}

/* ==========================================================================
 * Section 7 - MetadataParser and VisibleTextExtractor (same fields, same rules)
 * ========================================================================== */


const char *SKIP_TAGS[SKIP_TAGS_COUNT] = {
    "script", "style", "noscript", "template", "nav", "header", "footer", "aside", "form"
};
const char *PARA_TAGS[PARA_TAGS_COUNT] = {
    "p", "article", "main", "section", "div", "li", "blockquote"
};

int in_list(const char **list, size_t n, const char *tag)
{
    for (size_t i = 0; i < n; i++) if (!strcmp(list[i], tag)) return 1;
    return 0;
}


void meta_parser_init(meta_parser *m)
{
    memset(m, 0, sizeof *m);
    sb_init(&m->title);
    sb_init(&m->robots_meta);
    sb_init(&m->heading_parts);
    sb_init(&m->jsonld_parts);
    vec_init(&m->headings);
    vec_init(&m->links);
    vec_init(&m->jsonld);
    m->description = xstrdup("");
    m->og_description = xstrdup("");
    m->twitter_description = xstrdup("");
    m->canonical = xstrdup("");
    m->author = xstrdup("");
    m->pub_date = xstrdup("");
    m->mod_date = xstrdup("");
    m->lang = xstrdup("");
    m->favicon = xstrdup("");
    m->favicon_pri = 9;
}

void meta_parser_free(meta_parser *m)
{
    sb_free(&m->title);
    sb_free(&m->robots_meta);
    sb_free(&m->heading_parts);
    sb_free(&m->jsonld_parts);
    free(m->description);
    free(m->og_description);
    free(m->twitter_description);
    free(m->canonical);
    free(m->author);
    free(m->pub_date);
    free(m->mod_date);
    free(m->lang);
    free(m->favicon);
    for (size_t i = 0; i < m->headings.len; i++) {
        heading_t *h = m->headings.items[i];
        free(h->text);
        free(h);
    }
    vec_free(&m->headings, NULL);
    vec_free(&m->links, free);
    vec_free(&m->jsonld, free);
}

void mp_set_once(char **slot, const char *val)
{
    if (*slot && **slot) return;
    if (!val || !*val) return;
    free(*slot);
    *slot = xstrdup(val);
}

void mp_starttag(void *ctx, const char *tag, const attrs_t *a)
{
    meta_parser *m = ctx;
    if (!strcmp(tag, "title")) {
        m->in_title = 1;
        return;
    }
    if (!strcmp(tag, "meta")) {
        const char *n = attrs_get(a, "name");
        const char *p = attrs_get(a, "property");
        const char *c = attrs_get(a, "content");
        char *nl = n ? str_lower(n) : NULL;
        char *pl = p ? str_lower(p) : NULL;
        char *cs = c ? str_strip(c) : NULL;
        if (nl && !strcmp(nl, "robots") && cs && *cs) {
            sb_putc(&m->robots_meta, ' ');
            char *low = str_lower(cs);
            sb_append(&m->robots_meta, low);
            free(low);
        } else if (nl && !strcmp(nl, "description") && cs && *cs) {
            mp_set_once(&m->description, cs);
        } else if (nl && !strcmp(nl, "author") && cs && *cs) {
            mp_set_once(&m->author, cs);
        } else if (pl && !strcmp(pl, "og:description") && cs && *cs) {
            mp_set_once(&m->og_description, cs);
        } else if (nl && !strcmp(nl, "twitter:description") && cs && *cs) {
            mp_set_once(&m->twitter_description, cs);
        } else if (pl && (!strcmp(pl, "article:published_time") ||
                          !strcmp(pl, "og:published_time")) && cs && *cs) {
            mp_set_once(&m->pub_date, cs);
        } else if (pl && (!strcmp(pl, "article:modified_time") ||
                          !strcmp(pl, "og:updated_time")) && cs && *cs) {
            mp_set_once(&m->mod_date, cs);
        }
        free(nl); free(pl); free(cs);
        return;
    }
    if (!strcmp(tag, "link")) {
        const char *rel = attrs_get(a, "rel");
        if (rel) {
            char *rl = str_lower(rel);
            if (strstr(rl, "canonical")) {
                const char *href = attrs_get(a, "href");
                char *h = href ? str_strip(href) : xstrdup("");
                free(m->canonical);
                m->canonical = h;
            } else if (strstr(rl, "icon")) {
                /* Favicon discovery: rel="icon" / "shortcut icon" beats
                 * apple-touch-icon, and the first declared one wins. */
                int pri = strstr(rl, "apple-touch-icon") ? 1 : 0;
                const char *href = attrs_get(a, "href");
                if (href && pri < m->favicon_pri) {
                    char *h = str_strip(href);
                    if (*h) {
                        free(m->favicon);
                        m->favicon = h;
                        m->favicon_pri = pri;
                    } else {
                        free(h);
                    }
                }
            }
            free(rl);
        }
        return;
    }
    if (!strcmp(tag, "html")) {
        const char *lg = attrs_get(a, "lang");
        if ((!m->lang || !*m->lang) && lg) {
            char *h = str_strip(lg);
            free(m->lang);
            m->lang = h;
        }
        return;
    }
    if (!strcmp(tag, "h1") || !strcmp(tag, "h2") || !strcmp(tag, "h3")) {
        m->heading_level = tag[1] - '0';
        sb_clear(&m->heading_parts);
        m->heading_parts_count = 0;
        return;
    }
    if (!strcmp(tag, "a")) {
        const char *href = attrs_get(a, "href");
        if (href) {
            char *h = str_strip(href);
            if (*h) vec_push(&m->links, h); else free(h);
        }
        return;
    }
    if (!strcmp(tag, "script")) {
        const char *type = attrs_get(a, "type");
        if (type) {
            char *tl = str_lower(type);
            if (!strcmp(tl, "application/ld+json")) {
                m->in_jsonld = 1;
                sb_clear(&m->jsonld_parts);
            }
            free(tl);
        }
        return;
    }
}

void mp_endtag(void *ctx, const char *tag)
{
    meta_parser *m = ctx;
    if (!strcmp(tag, "title")) {
        m->in_title = 0;
        char *full = str_strip(m->title.buf ? m->title.buf : "");
        char *cut = trunc_chars(full, TITLE_MAX_CHARS);
        free(full);
        sb_clear(&m->title);
        sb_append(&m->title, cut);
        free(cut);
        return;
    }
    if ((!strcmp(tag, "h1") || !strcmp(tag, "h2") || !strcmp(tag, "h3")) && m->heading_level) {
        char *text = str_strip(m->heading_parts.buf ? m->heading_parts.buf : "");
        if (*text) {
            heading_t *h = xcalloc(1, sizeof(heading_t));
            h->level = m->heading_level;
            h->text = trunc_chars(text, HEADING_MAX_CHARS);
            vec_push(&m->headings, h);
        }
        free(text);
        m->heading_level = 0;
        sb_clear(&m->heading_parts);
        m->heading_parts_count = 0;
        return;
    }
    if (!strcmp(tag, "script") && m->in_jsonld) {
        m->in_jsonld = 0;
        char *raw = str_strip(m->jsonld_parts.buf ? m->jsonld_parts.buf : "");
        if (*raw) {
            json_val *probe = json_parse(raw);
            if (probe) {
                json_free(probe);
                vec_push(&m->jsonld, raw);
            } else {
                free(raw);
            }
        } else {
            free(raw);
        }
        sb_clear(&m->jsonld_parts);
        return;
    }
}

void mp_data(void *ctx, const char *text, size_t len)
{
    meta_parser *m = ctx;
    if (m->in_title) {
        if (m->title_len < TITLE_MAX_CHARS) {
            size_t remaining = TITLE_MAX_CHARS - m->title_len;
            size_t chars = 0;
            size_t i = 0;
            while (i < len) {
                if (chars == remaining) break;
                i++;
                while (i < len && ((unsigned char)text[i] & 0xC0) == 0x80) i++;
                chars++;
            }
            sb_appendn(&m->title, text, i);
            m->title_len += chars;
        }
        return;
    }
    if (m->in_jsonld) {
        sb_appendn(&m->jsonld_parts, text, len);
        return;
    }
    if (m->heading_level && m->heading_parts_count < HEADING_MAX_PARTS) {
        sb_appendn(&m->heading_parts, text, len);
        m->heading_parts_count++;
    }
}

/* -- visible text ---------------------------------------------------------- */

void tp_init(text_parser *t, size_t max_bytes)
{
    memset(t, 0, sizeof *t);
    vec_init(&t->text_parts);
    vec_init(&t->paragraphs);
    sb_init(&t->buf);
    t->max_bytes = max_bytes;
}

void tp_flush_para(text_parser *t);

void tp_free(text_parser *t)
{
    vec_free(&t->text_parts, free);
    vec_free(&t->paragraphs, free);
    sb_free(&t->buf);
}

void tp_starttag(void *ctx, const char *tag, const attrs_t *a)
{
    (void)a;
    text_parser *t = ctx;
    if (in_list(SKIP_TAGS, SKIP_TAGS_COUNT, tag)) {
        t->skip_depth++;
    } else if (in_list(PARA_TAGS, PARA_TAGS_COUNT, tag)) {
        t->para_depth++;
        if (t->buf.len) tp_flush_para(t);
    }
}

void tp_endtag(void *ctx, const char *tag)
{
    text_parser *t = ctx;
    if (in_list(SKIP_TAGS, SKIP_TAGS_COUNT, tag)) {
        if (t->skip_depth > 0) t->skip_depth--;
    } else if (in_list(PARA_TAGS, PARA_TAGS_COUNT, tag)) {
        if (t->para_depth > 0) t->para_depth--;
        if (t->buf.len) tp_flush_para(t);
    }
}

void tp_data(void *ctx, const char *text, size_t len)
{
    text_parser *t = ctx;
    if (t->skip_depth > 0 || t->bytes >= t->max_bytes) return;
    char *chunk = xstrndup(text, len);
    char *s = str_strip(chunk);
    free(chunk);
    if (!*s) { free(s); return; }
    t->bytes += utf8_len(s);
    vec_push(&t->text_parts, s);
    sb_append(&t->buf, s);
    sb_putc(&t->buf, ' ');
}

void tp_flush_para(text_parser *t)
{
    char *text = collapse_ws_trim(t->buf.buf ? t->buf.buf : "");
    sb_clear(&t->buf);
    if (utf8_len(text) >= PARA_MIN_CHARS) vec_push(&t->paragraphs, text);
    else free(text);
}

char *tp_finish_text(text_parser *t)
{
    if (t->buf.len) tp_flush_para(t);
    sb_t joined;
    sb_init(&joined);
    for (size_t i = 0; i < t->text_parts.len; i++) {
        if (i) sb_putc(&joined, ' ');
        sb_append(&joined, t->text_parts.items[i]);
    }
    char *raw = sb_detach(&joined);
    char *text = collapse_ws_trim(raw);
    free(raw);
    return text;
}
