#include "crawler.h"

/* URL parsing/joining/normalising, IDs, SSRF guard */
/* ==========================================================================
 * Section 5 - URLs (urllib.parse equivalents), ids, SSRF guard
 * ========================================================================== */


void urlparts_free(urlparts *u)
{
    free(u->scheme); free(u->netloc); free(u->path); free(u->query);
    u->scheme = u->netloc = u->path = u->query = NULL;
}

int scheme_char(int c)
{
    return isalnum(c) || c == '+' || c == '-' || c == '.';
}

void url_parse_parts(const char *url, urlparts *u)
{
    u->scheme = xstrdup("");
    u->netloc = xstrdup("");
    u->path = xstrdup("");
    u->query = NULL;
    if (!url) return;

    const char *rest = url;
    const char *colon = NULL;
    for (const char *p = url; *p; p++) {
        if (*p == ':') { colon = p; break; }
        if (*p == '/' || *p == '?' || *p == '#') break;
    }
    if (colon && colon != url && isalpha((unsigned char)url[0])) {
        int ok = 1;
        for (const char *p = url; p < colon; p++)
            if (!scheme_char((unsigned char)*p)) { ok = 0; break; }
        if (ok) {
            free(u->scheme);
            u->scheme = xstrndup(url, (size_t)(colon - url));
            char *low = str_lower(u->scheme);
            free(u->scheme);
            u->scheme = low;
            rest = colon + 1;
        }
    }

    /* drop the fragment */
    const char *hash = strchr(rest, '#');
    size_t restlen = hash ? (size_t)(hash - rest) : strlen(rest);
    /* split the query */
    const char *qmark = NULL;
    for (size_t i = 0; i < restlen; i++)
        if (rest[i] == '?') { qmark = rest + i; break; }
    size_t pathlen = qmark ? (size_t)(qmark - rest) : restlen;
    if (qmark) u->query = xstrndup(qmark + 1, restlen - (pathlen + 1));

    if (pathlen >= 2 && rest[0] == '/' && rest[1] == '/') {
        size_t i = 2;
        while (i < pathlen && rest[i] != '/') i++;
        free(u->netloc);
        u->netloc = xstrndup(rest + 2, i - 2);
        free(u->path);
        u->path = xstrndup(rest + i, pathlen - i);
    } else {
        free(u->path);
        u->path = xstrndup(rest, pathlen);
    }
}

char *url_unparse(const urlparts *u)
{
    sb_t b;
    sb_init(&b);
    if (u->scheme && *u->scheme) {
        sb_append(&b, u->scheme);
        sb_putc(&b, ':');
    }
    if (u->netloc && *u->netloc) {
        sb_append(&b, "//");
        sb_append(&b, u->netloc);
    }
    sb_append(&b, u->path ? u->path : "");
    if (u->query) {
        sb_putc(&b, '?');
        sb_append(&b, u->query);
    }
    return sb_detach(&b);
}

/* urllib.parse.urlparse(...).hostname, lowercased */
char *url_hostname(const char *url)
{
    urlparts u;
    url_parse_parts(url, &u);
    char *netloc = xstrdup(u.netloc);
    urlparts_free(&u);
    char *at = strrchr(netloc, '@');
    char *host;
    if (at) host = xstrdup(at + 1); else host = xstrdup(netloc);
    free(netloc);
    if (host[0] == '[') {
        char *close = strchr(host, ']');
        if (close) {
            char *h = xstrndup(host + 1, (size_t)(close - host - 1));
            free(host);
            host = h;
        }
    } else {
        char *lastcolon = strrchr(host, ':');
        if (lastcolon) {
            int digits = 1;
            for (char *p = lastcolon + 1; *p; p++)
                if (!isdigit((unsigned char)*p)) { digits = 0; break; }
            if (digits) *lastcolon = '\0';
        }
    }
    char *low = str_lower(host);
    free(host);
    return low;
}

/* RFC 3986 remove_dot_segments */
char *remove_dot_segments(const char *path)
{
    sb_t out;
    sb_init(&out);
    char *in = xstrdup(path ? path : "");
    size_t n = strlen(in);
    size_t pos = 0;
    while (pos < n) {
        if (!strncmp(in + pos, "../", 3)) { pos += 3; continue; }
        if (!strncmp(in + pos, "./", 2)) { pos += 2; continue; }
        if (!strncmp(in + pos, "/./", 3)) { in[pos + 1] = '/'; pos += 2; continue; }
        if (!strcmp(in + pos, "/.")) { in[pos + 1] = '\0'; n = pos + 1; break; }
        if (!strncmp(in + pos, "/../", 4) || !strcmp(in + pos, "/..")) {
            /* remove last segment from output */
            if (out.len == 0) { out.buf[0] = '\0'; }
            else {
                size_t i = out.len;
                while (i > 0 && out.buf[i - 1] != '/') i--;
                if (i > 0) i--;
                out.len = i;
                out.buf[out.len] = '\0';
            }
            if (!strcmp(in + pos, "/..")) { sb_append(&out, "/"); pos = n; n = pos; break; }
            pos += 3;
            continue;
        }
        if (!strcmp(in + pos, ".")) { pos += 1; continue; }
        if (!strcmp(in + pos, "..")) { pos += 2; continue; }
        /* move the next path segment */
        size_t start = pos;
        if (in[pos] == '/') pos++;
        while (pos < n && in[pos] != '/') pos++;
        sb_appendn(&out, in + start, pos - start);
    }
    free(in);
    return sb_detach(&out);
}

/* urllib.parse.urljoin */
char *url_join(const char *base, const char *rel)
{
    urlparts b, r;
    url_parse_parts(base ? base : "", &b);
    url_parse_parts(rel ? rel : "", &r);

    char *result = NULL;
    if (r.scheme && *r.scheme) {
        if (strcmp(r.scheme, b.scheme) != 0 || !*r.netloc) {
            char *path = remove_dot_segments(r.path);
            char *tmp = r.path;
            r.path = path;
            free(tmp);
            result = url_unparse(&r);
            urlparts_free(&b); urlparts_free(&r);
            return result;
        }
    }

    urlparts out;
    out.scheme = xstrdup(b.scheme);

    if (r.netloc && *r.netloc) {
        out.netloc = xstrdup(r.netloc);
        out.path = remove_dot_segments(r.path);
        out.query = r.query ? xstrdup(r.query) : NULL;
    } else {
        out.netloc = xstrdup(b.netloc);
        if (!r.path || !*r.path) {
            out.path = xstrdup(b.path ? b.path : "");
            if (r.query) out.query = xstrdup(r.query);
            else out.query = b.query ? xstrdup(b.query) : NULL;
        } else if (r.path[0] == '/') {
            out.path = remove_dot_segments(r.path);
            out.query = r.query ? xstrdup(r.query) : NULL;
        } else {
            sb_t merged;
            sb_init(&merged);
            if (b.netloc && *b.netloc && (!b.path || !*b.path)) {
                sb_append(&merged, "/");
                sb_append(&merged, r.path);
            } else {
                const char *slash = b.path ? strrchr(b.path, '/') : NULL;
                if (slash) {
                    sb_appendn(&merged, b.path, (size_t)(slash - b.path) + 1);
                }
                sb_append(&merged, r.path);
            }
            char *m = sb_detach(&merged);
            out.path = remove_dot_segments(m);
            free(m);
            out.query = r.query ? xstrdup(r.query) : NULL;
        }
    }
    result = url_unparse(&out);
    urlparts_free(&b);
    urlparts_free(&r);
    urlparts_free(&out);
    return result;
}

/* normalize_url(url, base) */
char *normalize_url_base(const char *url, const char *base)
{
    if (!url) return NULL;
    char *joined = base ? url_join(base, url) : xstrdup(url);
    if (!base && !strstr(joined, "://")) {
        char *tmp = xmalloc(strlen(joined) + 9);
        strcpy(tmp, "https://");
        strcat(tmp, joined);
        free(joined);
        joined = tmp;
    }
    urlparts u;
    url_parse_parts(joined, &u);
    free(joined);
    if (strcmp(u.scheme, "http") != 0 && strcmp(u.scheme, "https") != 0) {
        urlparts_free(&u);
        return NULL;
    }
    /* path.rstrip("/") or "/" */
    size_t n = strlen(u.path);
    while (n > 0 && u.path[n - 1] == '/') n--;
    char *newpath = n ? xstrndup(u.path, n) : xstrdup("/");
    free(u.path);
    u.path = newpath;
    char *out = url_unparse(&u);
    urlparts_free(&u);
    return out;
}

char *normalize_url(const char *url) { return normalize_url_base(url, NULL); }

/* get_domain(url) */
char *get_domain(const char *url)
{
    urlparts u;
    url_parse_parts(url ? url : "", &u);
    char *d = xstrdup(u.netloc);
    urlparts_free(&u);
    char *low = str_lower(d);
    free(d);
    if (!strncmp(low, "www.", 4)) {
        char *s = xstrdup(low + 4);
        free(low);
        low = s;
    }
    return low;
}

int is_same_host(const char *u1, const char *u2)
{
    char *a = get_domain(u1);
    char *b = get_domain(u2);
    int same = strcmp(a, b) == 0;
    free(a); free(b);
    return same;
}

/* stable_id: sha1(url)[:16] */
char *stable_id(const char *url)
{
    char hex[41];
    sha1_hex(url, strlen(url), hex);
    return xstrndup(hex, 16);
}

char *content_hash(const char *text)
{
    char hex[65];
    sha256_hex(text, strlen(text), hex);
    return xstrdup(hex);
}

/* content_sig: near-duplicate signature, used only by --clean.
 *
 * Skips the opening of the page (title, byline, navigation) and hashes a fixed
 * block of text after it. The window is a fixed number of characters, so two
 * copies of the same article share a key even when one is longer, when their
 * titles differ, or when their surrounding chrome does. A page too short to
 * reach the window is hashed whole. */
#define CLEAN_SIG_SKIP_WORDS 150
#define CLEAN_SIG_WINDOW     900

char *content_sig(const char *text)
{
    if (!text || !*text) return xstrdup("");

    /* normalise: collapse whitespace, lowercase ASCII */
    sb_t norm;
    sb_init(&norm);
    int pending_space = 0;
    for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
        unsigned char ch = *p;
        if (ch < 0x80 && isspace(ch)) { pending_space = 1; continue; }
        if (pending_space && norm.len) sb_putc(&norm, ' ');
        pending_space = 0;
        sb_putc(&norm, (char)((ch < 0x80) ? tolower(ch) : ch));
    }

    size_t len = norm.len;
    size_t from = 0;
    size_t words = 0;
    for (size_t i = 0; i < len; i++) {
        if (norm.buf[i] == ' ') {
            words++;
            if (words == CLEAN_SIG_SKIP_WORDS) { from = i + 1; break; }
        }
    }
    size_t take = len - from;
    if (take > CLEAN_SIG_WINDOW) take = CLEAN_SIG_WINDOW;

    char hex[41];
    sha1_hex(norm.buf ? norm.buf + from : "", take, hex);
    sb_free(&norm);
    return xstrdup(hex);
}

/* -- SSRF ------------------------------------------------------------------ */

int ip4_forbidden(uint32_t ip)
{
    /* ip is host byte order */
    uint32_t a = (ip >> 24) & 0xff, b = (ip >> 16) & 0xff, c = (ip >> 8) & 0xff, d = ip & 0xff;
    if (a == 0) return 1;                                    /* 0.0.0.0/8 */
    if (a == 10) return 1;                                   /* 10/8 */
    if (a == 127) return 1;                                  /* loopback */
    if (a == 169 && b == 254) return 1;                      /* link local */
    if (a == 172 && b >= 16 && b <= 31) return 1;            /* 172.16/12 */
    if (a == 192 && b == 0 && c == 0) return 1;              /* 192.0.0.0/24 */
    if (a == 192 && b == 0 && c == 2) return 1;              /* TEST-NET-1 */
    if (a == 192 && b == 168) return 1;                      /* 192.168/16 */
    if (a == 198 && (b == 18 || b == 19)) return 1;          /* benchmarking */
    if (a == 198 && b == 51 && c == 100) return 1;           /* TEST-NET-2 */
    if (a == 203 && b == 0 && c == 113) return 1;            /* TEST-NET-3 */
    if (a == 100 && b >= 64 && b <= 127) return 1;           /* CGNAT */
    if (a >= 224) return 1;                                  /* multicast + reserved */
    (void)d;
    return 0;
}

int ip6_forbidden(const unsigned char *ip)
{
    static const unsigned char zero[16] = {0};
    if (!memcmp(ip, zero, 16)) return 1;                              /* :: */
    int loop = 1;
    for (int i = 0; i < 15; i++) if (ip[i]) { loop = 0; break; }
    if (loop && ip[15] == 1) return 1;                                 /* ::1 */
    if ((ip[0] & 0xfe) == 0xfc) return 1;                              /* fc00::/7 ULA */
    if (ip[0] == 0xfe && (ip[1] & 0xc0) == 0x80) return 1;             /* fe80::/10 */
    if (ip[0] == 0xff) return 1;                                       /* multicast */
    /* IPv4-mapped ::ffff:a.b.c.d */
    int mapped = 1;
    for (int i = 0; i < 10; i++) if (ip[i]) { mapped = 0; break; }
    if (mapped && ip[10] == 0xff && ip[11] == 0xff) {
        uint32_t v4 = ((uint32_t)ip[12] << 24) | ((uint32_t)ip[13] << 16) |
                      ((uint32_t)ip[14] << 8) | (uint32_t)ip[15];
        return ip4_forbidden(v4);
    }
    return 0;
}

int ip_is_forbidden(const char *ip)
{
    struct in_addr a4;
    struct in6_addr a6;
    if (inet_pton(AF_INET, ip, &a4) == 1) return ip4_forbidden(ntohl(a4.s_addr));
    if (inet_pton(AF_INET6, ip, &a6) == 1) return ip6_forbidden(a6.s6_addr);
    return 1;
}

const char *METADATA_HOSTS[] = {
    "169.254.169.254", "metadata.google.internal", "metadata.goog", "100.100.100.200", NULL
};

/* is_safe_to_fetch(url, char **reason) -> 1 safe / 0 blocked */
int is_safe_to_fetch(const char *url, char **reason)
{
    sb_t why;
    sb_init(&why);
    urlparts u;
    url_parse_parts(url ? url : "", &u);
    if (strcmp(u.scheme, "http") != 0 && strcmp(u.scheme, "https") != 0) {
        sb_appendf(&why, "scheme '%s' not allowed", u.scheme);
        urlparts_free(&u);
        goto blocked;
    }
    char *host = url_hostname(url);
    if (!host || !*host) {
        sb_append(&why, "no host");
        free(host);
        urlparts_free(&u);
        goto blocked;
    }
    for (int i = 0; METADATA_HOSTS[i]; i++) {
        if (strcmp(host, METADATA_HOSTS[i]) == 0) {
            sb_append(&why, "metadata endpoint");
            free(host);
            urlparts_free(&u);
            goto blocked;
        }
    }
    if (!strcmp(host, "localhost") || !strcmp(host, "localhost.localdomain")) {
        sb_append(&why, "localhost");
        free(host);
        urlparts_free(&u);
        goto blocked;
    }
    size_t hl = strlen(host);
    if (hl > 10 && !strcmp(host + hl - 10, ".localhost")) {
        sb_append(&why, "localhost");
        free(host);
        urlparts_free(&u);
        goto blocked;
    }

    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    int rc = getaddrinfo(host, NULL, &hints, &res);
    if (rc != 0) {
        sb_appendf(&why, "dns failure: %s", gai_strerror(rc));
        free(host);
        urlparts_free(&u);
        goto blocked;
    }
    int bad = 0;
    for (struct addrinfo *ai = res; ai; ai = ai->ai_next) {
        char ip[INET6_ADDRSTRLEN] = {0};
        if (ai->ai_family == AF_INET)
            inet_ntop(AF_INET, &((struct sockaddr_in *)ai->ai_addr)->sin_addr, ip, sizeof ip);
        else if (ai->ai_family == AF_INET6)
            inet_ntop(AF_INET6, &((struct sockaddr_in6 *)ai->ai_addr)->sin6_addr, ip, sizeof ip);
        else continue;
        if (ip_is_forbidden(ip)) {
            sb_appendf(&why, "forbidden ip %s", ip);
            bad = 1;
            break;
        }
    }
    freeaddrinfo(res);
    free(host);
    urlparts_free(&u);
    if (bad) goto blocked;
    if (reason) *reason = xstrdup("ok");
    sb_free(&why);
    return 1;

blocked:
    if (reason) *reason = sb_detach(&why);
    else sb_free(&why);
    return 0;
}
