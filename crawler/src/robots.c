#include "crawler.h"

/* robots.txt engine (urllib.robotparser 3.11 semantics) */
/* ==========================================================================
 * Section 11 - robots.txt engine (exact port of urllib.robotparser 3.11)
 * ========================================================================== */




void rule_free(void *p)
{
    rule_t *r = p;
    if (!r) return;
    free(r->path);
    free(r);
}

void robogroup_free(robogroup_t *g)
{
    if (!g) return;
    vec_free(&g->agents, free);
    vec_free(&g->rules, rule_free);
    free(g);
}

void robotsfile_free(robotsfile *rf)
{
    for (size_t i = 0; i < rf->entries.len; i++) robogroup_free(rf->entries.items[i]);
    vec_free(&rf->entries, NULL);
    /* the first "*" group is kept aside, not in entries */
    robogroup_free(rf->default_entry);
    rf->default_entry = NULL;
}

/* urllib.parse.unquote */
char *py_unquote(const char *s)
{
    sb_t out;
    sb_init(&out);
    for (size_t i = 0; s[i]; i++) {
        if (s[i] == '%' && isxdigit((unsigned char)s[i + 1]) && isxdigit((unsigned char)s[i + 2])) {
            char hex[3] = { s[i + 1], s[i + 2], 0 };
            sb_putc(&out, (char)strtol(hex, NULL, 16));
            i += 2;
        } else {
            sb_putc(&out, s[i]);
        }
    }
    return sb_detach(&out);
}

/* urllib.parse.quote(s, safe='/') */
char *py_quote_path(const char *s)
{
    sb_t out;
    sb_init(&out);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        int c = *p;
        if (isalnum(c) || c == '_' || c == '.' || c == '-' || c == '~' || c == '/')
            sb_putc(&out, (char)c);
        else
            sb_appendf(&out, "%%%02X", c);
    }
    return sb_detach(&out);
}

void robotsfile_add_entry(robotsfile *rf, robogroup_t *entry)
{
    int is_default = 0;
    for (size_t i = 0; i < entry->agents.len; i++) {
        if (!strcmp((char *)entry->agents.items[i], "*")) { is_default = 1; break; }
    }
    if (is_default) {
        if (!rf->default_entry) rf->default_entry = entry;
        else robogroup_free(entry);        /* the first default entry wins */
    } else {
        vec_push(&rf->entries, entry);
    }
}

robogroup_t *robogroup_new(void)
{
    robogroup_t *g = xcalloc(1, sizeof(robogroup_t));
    vec_init(&g->agents);
    vec_init(&g->rules);
    g->delay = -1;
    return g;
}

int all_digits(const char *s)
{
    if (!*s) return 0;
    for (const char *p = s; *p; p++) if (!isdigit((unsigned char)*p)) return 0;
    return 1;
}

/* RobotFileParser.parse(lines) */
void robotsfile_parse(robotsfile *rf, const char *text)
{
    if (!text) return;
    int state = 0;
    robogroup_t *entry = robogroup_new();

    const char *p = text;
    for (;;) {
        const char *eol = strchr(p, '\n');
        size_t len = eol ? (size_t)(eol - p) : strlen(p);
        char *raw = xstrndup(p, len);
        /* drop trailing \r */
        size_t rl = strlen(raw);
        while (rl && (raw[rl - 1] == '\r')) raw[--rl] = '\0';

        if (!*raw) {
            if (state == 1) {
                robogroup_free(entry);
                entry = robogroup_new();
                state = 0;
            } else if (state == 2) {
                robotsfile_add_entry(rf, entry);
                entry = robogroup_new();
                state = 0;
            }
        }
        char *hash = strchr(raw, '#');
        if (hash) *hash = '\0';
        char *line = str_strip(raw);
        free(raw);
        if (*line) {
            char *colon = strchr(line, ':');
            if (colon) {
                *colon = '\0';
                char *key = str_lower(line);
                char *val = str_strip(colon + 1);
                char *unq = py_unquote(val);
                free(val);
                if (!strcmp(key, "user-agent")) {
                    if (state == 2) {
                        robotsfile_add_entry(rf, entry);
                        entry = robogroup_new();
                    }
                    vec_push(&entry->agents, xstrdup(unq));
                    state = 1;
                } else if (!strcmp(key, "disallow")) {
                    if (state != 0) {
                        rule_t *r = xcalloc(1, sizeof(rule_t));
                        int allow = 0;
                        if (!*unq) allow = 1;   /* empty disallow means allow all */
                        urlparts up;
                        url_parse_parts(unq, &up);
                        char *reassembled = url_unparse(&up);
                        urlparts_free(&up);
                        r->path = py_quote_path(reassembled);
                        free(reassembled);
                        r->allow = allow;
                        vec_push(&entry->rules, r);
                        state = 2;
                    }
                } else if (!strcmp(key, "allow")) {
                    if (state != 0) {
                        rule_t *r = xcalloc(1, sizeof(rule_t));
                        urlparts up;
                        url_parse_parts(unq, &up);
                        char *reassembled = url_unparse(&up);
                        urlparts_free(&up);
                        r->path = py_quote_path(reassembled);
                        free(reassembled);
                        r->allow = 1;
                        vec_push(&entry->rules, r);
                        state = 2;
                    }
                } else if (!strcmp(key, "crawl-delay")) {
                    if (state != 0) {
                        char *trimmed = str_strip(unq);
                        if (all_digits(trimmed)) {
                            entry->delay = atoi(trimmed);
                            entry->has_delay = 1;
                        }
                        free(trimmed);
                        state = 2;
                    }
                }
                free(key);
                free(unq);
            }
            free(line);
        } else {
            free(line);
        }
        if (!eol) break;
        p = eol + 1;
    }
    if (state == 2) robotsfile_add_entry(rf, entry);
    else robogroup_free(entry);
}

/* Entry.applies_to */
int robogroup_applies(const robogroup_t *g, const char *useragent)
{
    char *low = str_lower(useragent ? useragent : "");
    char *slash = strchr(low, '/');
    if (slash) *slash = '\0';
    int applies = 0;
    for (size_t i = 0; i < g->agents.len; i++) {
        const char *agent = g->agents.items[i];
        if (!strcmp(agent, "*")) { applies = 1; break; }
        char *al = str_lower(agent);
        if (strstr(low, al)) { free(al); applies = 1; break; }
        free(al);
    }
    free(low);
    return applies;
}

/* Entry.allowance */
int robogroup_allowance(const robogroup_t *g, const char *filename)
{
    for (size_t i = 0; i < g->rules.len; i++) {
        rule_t *r = g->rules.items[i];
        if (!strcmp(r->path, "*") || !strncmp(filename, r->path, strlen(r->path)))
            return r->allow;
    }
    return 1;
}

int robots_can_fetch(robotsfile *rf, const char *ua, const char *url)
{
    /* urlparse(unquote(url)) -> urlunparse(('','',path,params,query,fragment)) */
    char *unq = py_unquote(url);
    urlparts up;
    url_parse_parts(unq, &up);
    free(unq);
    sb_t tail;
    sb_init(&tail);
    sb_append(&tail, (up.path && *up.path) ? up.path : "");
    if (up.query) { sb_putc(&tail, '?'); sb_append(&tail, up.query); }
    urlparts_free(&up);
    char *prequoted = sb_detach(&tail);
    char *filename = py_quote_path(prequoted);
    free(prequoted);
    if (!*filename) {
        free(filename);
        filename = xstrdup("/");
    }
    int allowed = 1;
    int decided = 0;
    for (size_t i = 0; i < rf->entries.len && !decided; i++) {
        robogroup_t *g = rf->entries.items[i];
        if (robogroup_applies(g, ua)) {
            allowed = robogroup_allowance(g, filename);
            decided = 1;
        }
    }
    if (!decided && rf->default_entry && robogroup_applies(rf->default_entry, ua)) {
        allowed = robogroup_allowance(rf->default_entry, filename);
        decided = 1;
    }
    free(filename);
    return allowed;
}

/* crawl_delay: -1 when the parser has no value for this agent */
double robots_crawl_delay(robotsfile *rf, const char *ua)
{
    for (size_t i = 0; i < rf->entries.len; i++) {
        robogroup_t *g = rf->entries.items[i];
        if (robogroup_applies(g, ua)) return g->has_delay ? (double)g->delay : -1.0;
    }
    if (rf->default_entry && robogroup_applies(rf->default_entry, ua))
        return rf->default_entry->has_delay ? (double)rf->default_entry->delay : -1.0;
    return -1.0;
}
