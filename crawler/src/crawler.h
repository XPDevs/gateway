/* ==========================================================================
 * Gateway Crawler 3.1.0 - shared declarations
 *
 * C11 port of gateway_crawler.py v3.1.0, split across the modules in src/.
 * Every module includes this header and the build links them into one
 * executable called "crawler" (see the Makefile).
 * ========================================================================== */

#ifndef GATEWAY_CRAWLER_H
#define GATEWAY_CRAWLER_H

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <ctype.h>
#include <math.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/statvfs.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <termios.h>
#include <iconv.h>
#include <curl/curl.h>
#include <zlib.h>
#include <sqlite3.h>
#define VERSION "3.1.0"

/* --clean defaults */
#define CLEAN_DEFAULT_WINDOW 20000
#define GW_STR2(x) #x
#define GW_STR(x) GW_STR2(x)
#define DEFAULT_USER_AGENT   "GatewayCrawler/3.1 (+https://github.com/XPDevs/gateway)"
#define DEFAULT_DELAY        1.0
#define DEFAULT_TIMEOUT      10.0
#define DEFAULT_OUTPUT       "gateway_index.json"
#define DEFAULT_MAX_QUEUE    20000
#define DEFAULT_BLOOM_CAP    1000000
#define DEFAULT_BLOOM_FP     0.01
#define DEFAULT_PAGE_BYTES   (2 * 1024 * 1024)
#define DEFAULT_CHUNK        (64 * 1024)
#define DEFAULT_ROBOTS_CAP   200
#define DEFAULT_ROBOTS_BYTES (100 * 1024)
#define DEFAULT_RATELIM_CAP  2000
#define DEFAULT_MAX_RETRIES  3
#define DEFAULT_MAX_DEPTH    5
#define DEFAULT_MAX_PER_DOM  5000
#define DEFAULT_MAX_LINKS    200
#define DEFAULT_CHECKPOINT   ".gateway_checkpoint.json"
#define DEFAULT_NTFY_SERVER  "https://ntfy.sh"
#define DEFAULT_NTFY_EVENTS  "started,finished"
#define DEFAULT_NTFY_TIMEOUT 5.0
#define TUI_REFRESH_DEFAULT  0.5
#define TUI_LOG_LINES        300
#define TITLE_MAX_CHARS      500
#define HEADING_MAX_PARTS    16
#define HEADING_MAX_CHARS    300
#define VIS_MAX_BYTES        200000
#define PARA_MIN_CHARS       40
#define PARA_MAX_KEEP        50
#define DESC_MAX_LEN         300
#define LANG_SAMPLE          500

/* ---------- shared types, in dependency order ---------- */
typedef struct {
    char  *buf;
    size_t len;
    size_t cap;
} sb_t;
typedef struct {
    void **items;
    size_t len;
    size_t cap;
} vec_t;
#define MAP_EMPTY 0
#define MAP_USED  1
#define MAP_DEL   2
typedef struct {
    char    *key;
    void    *val;
    uint64_t hash;
    uint8_t  state;
} ment_t;
typedef struct {
    ment_t *ent;
    size_t  cap;
    size_t  live;
    size_t  dead;
} map_t;
typedef struct lru_node {
    char *key;
    void *val;
    struct lru_node *prev, *next;
} lru_node;
typedef struct {
    map_t     index;      /* key -> lru_node* */
    lru_node *head;       /* most recently used */
    lru_node *tail;       /* least recently used */
    size_t    maxsize;
    size_t    len;
    void    (*valfree)(void *);
} lru_t;
typedef struct {
    uint32_t a, b, c, d;
    uint64_t len;
    unsigned char buf[64];
    size_t buflen;
} md5_ctx;
typedef struct {
    uint32_t h[5];
    uint64_t len;
    unsigned char buf[64];
    size_t buflen;
} sha1_ctx;
typedef struct {
    uint32_t h[8];
    uint64_t len;
    unsigned char buf[64];
    size_t buflen;
} sha256_ctx;
typedef struct {
    uint8_t *bits;
    uint64_t num_bits;
    int      num_hashes;
    uint64_t capacity;
    double   error_rate;
    uint64_t added;
} bloom_t;
#define B2(n) n, n+1, n+1, n+2
#define B4(n) B2(n), B2(n+1), B2(n+1), B2(n+2)
#define B6(n) B4(n), B4(n+1), B4(n+1), B4(n+2)
typedef enum { JSON_NULL, JSON_BOOL, JSON_NUM, JSON_STR, JSON_ARR, JSON_OBJ } json_type;
typedef struct json_val {
    json_type t;
    int       bval;
    int       is_int;
    long long ival;
    double    num;
    char     *str;
    struct json_val **items;
    char    **keys;
    size_t    n;
} json_val;
typedef struct {
    const char *p;
    int         err;
} jparser;
typedef struct {
    char *scheme;
    char *netloc;
    char *path;
    char *query;     /* NULL when absent */
} urlparts;
typedef struct {
    char *name;
    char *val;
} attr_t;
typedef struct {
    attr_t *v;
    size_t  n;
} attrs_t;
typedef struct {
    void (*starttag)(void *ctx, const char *tag, const attrs_t *attrs);
    void (*endtag)(void *ctx, const char *tag);
    void (*data)(void *ctx, const char *text, size_t len);
} html_sink;
typedef struct { const char *name; const char *utf8; } ent_t;
typedef struct { int level; char *text; } heading_t;
#define SKIP_TAGS_COUNT 9
#define PARA_TAGS_COUNT 7
typedef struct {
    sb_t   title;
    size_t title_len;
    char  *description;
    char  *og_description;
    char  *twitter_description;
    char  *canonical;
    char  *author;
    char  *pub_date;
    char  *mod_date;
    char  *lang;
    char  *favicon;       /* <link rel="icon"> href, resolved later */
    int    favicon_pri;   /* 0 = icon, 1 = apple-touch-icon, 9 = none yet */
    sb_t   robots_meta;
    vec_t  headings;      /* heading_t* */
    vec_t  links;         /* char* */
    vec_t  jsonld;        /* char* raw json */
    int    in_title;
    int    in_jsonld;
    int    heading_level;
    sb_t   heading_parts;
    size_t heading_parts_count;
    sb_t   jsonld_parts;
} meta_parser;
typedef struct {
    vec_t  text_parts;
    vec_t  paragraphs;
    sb_t   buf;
    int    skip_depth;
    int    para_depth;
    size_t bytes;
    size_t max_bytes;
} text_parser;
typedef struct tui_s tui_t;
typedef struct {
    int     verbose;
    int     quiet;
    char   *fmt;
    tui_t  *tui;
    double  t0;
    FILE   *file;
} logger_t;
typedef struct {
    const char *s;
    size_t      n;
    size_t      i;
    int         kind;     /* 0 eof, 1 start, 2 end, 3 text */
    char       *name;     /* local name, lowercased */
    attrs_t     attrs;
    char       *text;
} xml_scan;
typedef struct { char *kind; char *url; } sitemap_entry;
typedef struct { char *title; char *link; } feed_item;
typedef struct {
    char *path;
    int   allow;
} rule_t;
typedef struct {
    vec_t agents;      /* char* */
    vec_t rules;       /* rule_t* */
    int   delay;
    int   has_delay;
} robogroup_t;
typedef struct {
    vec_t       entries;        /* robogroup_t* */
    robogroup_t *default_entry; /* first group whose agents include "*" */
} robotsfile;
typedef struct {
    long   status;
    char  *final_url;
    char  *content_type;
    char  *content_language;
    char  *x_robots_tag;
    char  *etag;
    char  *last_modified;
    char  *retry_after;
    char  *charset;
    char  *location;
    unsigned char *body;
    size_t body_len;
    int    truncated;
    int    transport_error;
    char  *error;
} http_response;
typedef struct {
    z_stream       zs;
    int            zinit;
    int            zmode;        /* 0 raw, 1 gzip, 2 deflate */
    unsigned char *out;
    size_t         len;
    size_t         cap;
    size_t         limit;
    int            reached_limit;
    int            zerror;
} body_buf;
typedef struct {
    char *ua;
    char *accept;
    char *accept_language;
    char *accept_encoding;
    char *if_none_match;
    char *if_modified_since;
    double timeout;
    size_t limit;
    const char *method;
    const unsigned char *post_body;
    size_t post_len;
    char **extra_names;
    char **extra_values;
} http_opts;
typedef struct {
    http_response *resp;
    body_buf      *body;
} write_ctx;
typedef struct {
    char *text;
    vec_t paragraphs;      /* char* */
    int   word_count;
    int   reading_minutes;
} page_content;
typedef struct {
    char        *final_url;
    long         status;
    char        *content_type;
    char        *language;
    size_t       html_bytes;
    meta_parser *meta;          /* NULL when the response was not HTML */
    page_content content;
    char        *error;
    int          from_cache;
    char        *x_robots_tag;
    char        *charset;
    unsigned char *body;
    size_t       body_len;
} fetch_result;
typedef struct {
    char *etag;
    char *lm;
} cond_t;
typedef struct {
    char     *ua;
    double    timeout;
    int       max_retries;
    size_t    page_max_bytes;
    size_t    chunk;
    logger_t *log;
    lru_t     conditional;
    char     *accept;
    char     *accept_language;
    char     *accept_encoding;
} fetcher_t;
#define ROBOTS_OK 1
#define ROBOTS_ERROR 0
#define ROBOTS_DISALLOW_ALL 2
#define ROBOTS_ALLOW_ALL 3
typedef struct {
    int         status;
    robotsfile *rf;
    double      delay;
    int         has_delay;
} robots_entry;
typedef struct {
    char   *ua;
    double  timeout;
    lru_t   cache;
} robots_cache;
typedef struct { char *url; char *reason; } failed_url_t;
typedef struct {
    map_t     status;       /* "200" -> count */
    map_t     domains;
    map_t     languages;
    map_t     types;
    map_t     errors;
    vec_t     failed_urls;  /* failed_url_t* */
    long long total_bytes;
    double    start;
} stats_t;
typedef struct { const char *key; long count; } kv_t;
typedef struct {
    char *id;
    char *t;
    char *u;
    char *d;
    char *s;
    long  status;
    char *content_type;
    char *lang;
    char *author;
    char *pub_date;
    char *mod_date;
    char *canonical;
    char *favicon;      /* NULL => field omitted (--no-favicons) */
    int   word_count;
    int   reading_minutes;
    char *content_hash;
    char *sig;          /* near-duplicate signature: internal, never serialised */
} record_t;
typedef struct writer writer_t;
struct writer {
    void (*write)(writer_t *, const char *);
    void (*close)(writer_t *);
    void  *impl;
};
typedef struct {
    char  *path;
    char  *tmp;
    FILE  *fh;
    int    first;
    long   count;
    int    closed;
} json_writer_t;
typedef struct {
    sqlite3 *db;
    sqlite3_stmt *ins;
    long count;
} sqlite_writer_t;
typedef struct {
    char     *url;
    logger_t *log;
    double    timeout;
    char     *last_error;
} ntfy_t;
typedef struct {
    int            prio;
    unsigned long  counter;
    char          *url;
    int            depth;
    char          *base;
} fitem;
typedef struct {
    size_t        maxsize;
    fitem        *heap;
    size_t        len;
    size_t        cap;
    map_t         seen;      /* url -> 1 */
    unsigned long counter;
    long          dropped;
} frontier_t;
typedef struct {
    int            ok;
    struct termios old;
    int            have_old;
} key_reader_t;
typedef struct {
    const char *status;
    long        pages;
    long        max_pages;      /* <= 0 means no limit */
    long        queue;
    long long   seen;
    double      rate;
    double      rss;
    double      bloom;
    long        errors;
    long        robots;
    long        dupes;
    double      elapsed;
    map_t      *status_breakdown;
    map_t      *content_types;
    map_t      *languages;
    map_t      *top_domains;
} crawl_state_t;
typedef struct { char *level; char *msg; double ts; } logline_t;
struct tui_s {
    int            enabled;
    int            unicode;
    double         refresh_rate;
    vec_t          log_lines;     /* logline_t* */
    double         start;
    double         last_render;
    int            info_mode;
    char          *current;
    crawl_state_t  state;
    key_reader_t   keys;
};
#include <regex.h>
typedef struct crawler crawler_t;
struct crawler {
    vec_t   seeds;              /* char* */
    int     has_max_pages;
    long    max_pages;
    double  delay;
    char   *output_path;
    char   *user_agent;
    double  timeout;
    int     same_host_only;
    size_t  max_page_bytes;
    size_t  chunk_size;
    int     has_max_memory;
    double  max_memory_mb;
    int     ignore_robots;
    int     verbose_robots;
    int     max_depth;
    long    max_per_domain;
    long    max_links_per_page;
    vec_t   exclude_domains;    /* regex_t* */
    vec_t   exclude_patterns;   /* regex_t* */
    int     use_sitemaps;
    int     use_feeds;
    int     storage_sqlite;
    char   *checkpoint_path;
    int     resume;
    int     dry_run;
    map_t   language_filter;
    int     respect_noindex;
    int     use_favicons;              /* discover one favicon per domain */
    map_t   favicon_cache;             /* domain -> char* favicon url */
    long    favicons_found;
    tui_t  *tui;

    logger_t *log;
    int       owns_log;

    fetcher_t     fetcher;
    robots_cache  robots;
    lru_t         last_request;        /* host -> double* */
    map_t         domain_pages;
    lru_t         robots_explained;

    bloom_t    seen;
    bloom_t    seen_content;
    frontier_t frontier;

    long page_count;
    long robots_blocks;
    long network_errors;
    long duplicate_content;
    long filtered;
    long noindex_skipped;

    /* --clean: write-time de-duplication with a winner rule */
    int    clean_mode;
    long   clean_window;        /* staged records before a flush */
    vec_t  clean_stage;         /* record_t* waiting to be written */
    map_t  clean_pending;       /* dedupe key -> index+1 in clean_stage */
    map_t  clean_written;       /* dedupe key -> 1 (already written) */
    long   clean_duplicates;    /* duplicates dropped or merged */
    long   clean_kept;          /* records written through the clean path */
    long   clean_replaced;      /* duplicates that won and replaced the kept copy */
    long   clean_flushes;
    volatile sig_atomic_t stop;
    char  *stop_reason;
    int    paused;
    stats_t stats;
    writer_t *writer;

    ntfy_t *notifier;
    map_t   ntfy_events;
    long    ntfy_progress_every;
};
typedef struct {
    vec_t   seeds;              /* char* (owned) */
    int     has_max_pages;
    long    max_pages;
    double  delay;
    char   *output_path;
    char   *user_agent;
    double  timeout;
    int     same_host_only;
    int     quiet;
    size_t  max_page_bytes;
    size_t  chunk_size;
    int     has_max_memory;
    double  max_memory_mb;
    int     ignore_robots;
    int     verbose_robots;
    int     max_depth;
    long    max_per_domain;
    long    max_links_per_page;
    char  **exclude_domains;
    int     n_exclude_domains;
    char  **exclude_patterns;
    int     n_exclude_patterns;
    int     use_sitemaps;
    int     use_feeds;
    int     storage_sqlite;
    char   *checkpoint_path;
    int     resume;
    int     dry_run;
    int     max_retries;
    char   *log_fmt;
    size_t  max_queue_size;
    uint64_t bloom_capacity;
    double  bloom_fp;
    char  **languages;
    int     n_languages;
    int     respect_noindex;
    int     use_favicons;
    int     clean_mode;
    long    clean_window;
    tui_t  *tui;
    char   *debug_log;
    char   *ntfy_topic;
    char  **ntfy_events;
    int     n_ntfy_events;
    long    ntfy_progress_every;
} crawler_cfg;
typedef struct {
    vec_t  seeds;
    char  *seed_file;
    int    has_max_pages;
    long   max_pages;
    int    forever;
    char  *output;
    int    storage_sqlite;
    double delay;
    double timeout;
    char  *user_agent;
    char  *config;
    int    same_host;
    vec_t  exclude_domain;
    vec_t  exclude_url;
    vec_t  languages;
    int    max_depth;
    long   max_per_domain;
    long   max_links_per_page;
    size_t max_queue_size;
    uint64_t bloom_capacity;
    double bloom_fp;
    size_t max_page_bytes;
    size_t chunk_size;
    int    has_max_memory;
    double max_memory_mb;
    int    max_retries;
    int    ignore_robots;
    int    verbose_robots;
    int    no_respect_noindex;
    int    no_favicons;
    int    clean;
    long   clean_window;
    int    use_sitemaps;
    int    use_feeds;
    char  *checkpoint;
    int    resume;
    int    debug;
    char  *debug_log;
    int    quiet;
    char  *log_format;
    int    dry_run;
    int    no_tui;
    double refresh_rate;
    char  *ntfy_topic;
    char  *ntfy_events;
    long   ntfy_progress_every;
    char  *bench_html;
    long   bench_iterations;
} cli_args;
#define TAKE_VALUE() (xstrdup(value_at ? value_at : argv[need_value(argc, i++, arg)]))
#define SET_STR(field, val) do { free(a->field); a->field = (val); } while (0)

/* ---------- shared globals ---------- */
extern const char *C_RESET;
extern crawler_t *g_crawler;

/* ---------- function prototypes ---------- */
void *xmalloc(size_t n);
void *xcalloc(size_t n, size_t s);
void *xrealloc(void *p, size_t n);
char *xstrdup(const char *s);
char *xstrndup(const char *s, size_t n);
void sb_init(sb_t *b);
void sb_free(sb_t *b);
void sb_reserve(sb_t *b, size_t need);
void sb_putc(sb_t *b, char c);
void sb_append(sb_t *b, const char *s);
void sb_appendn(sb_t *b, const char *s, size_t n);
void sb_appendf(sb_t *b, const char *fmt, ...);
void sb_clear(sb_t *b);
char *sb_detach(sb_t *b);
void vec_init(vec_t *v);
void vec_push(vec_t *v, void *item);
void *vec_pop(vec_t *v);
void vec_free(vec_t *v, void (*fn)(void *));
uint64_t fnv1a(const char *s, size_t n);
uint64_t fnv1a_str(const char *s);
void map_init(map_t *m);
void map_free(map_t *m, void (*valfree)(void *));
ment_t *map_slot(map_t *m, const char *key, uint64_t h);
void map_grow(map_t *m);
void *map_put(map_t *m, const char *key, void *val);
void *map_get(map_t *m, const char *key);
int map_has(map_t *m, const char *key);
long map_incr(map_t *m, const char *key, long by);
int map_del(map_t *m, const char *key);
void lru_init(lru_t *l, size_t maxsize, void (*valfree)(void *));
void lru_unlink(lru_t *l, lru_node *n);
void lru_splice_head(lru_t *l, lru_node *n);
void lru_free(lru_t *l);
lru_node *lru_touch(lru_t *l, const char *key);
void *lru_get(lru_t *l, const char *key);
void lru_put(lru_t *l, const char *key, void *val);
char *str_lower_heap(char *s);
char *str_lower(const char *s);
int is_space_byte(int c);
size_t utf8_space_len(const char *s);
char *str_strip(const char *s);
char *str_strip_inplace(char *s);
char *collapse_ws(const char *s);
char *collapse_ws_trim(const char *s);
size_t utf8_len(const char *s);
char *trunc_chars_add(const char *s, size_t n, int unicode, const char *tail_uni, const char *tail_ascii);
char *trunc_chars(const char *s, size_t n);
double py_round(double x);
long py_round_long(double x);
void sb_append_thousands(sb_t *b, long long v);
char *fmt_thousands(long long v);
void now_string(char *out, size_t n);
void md5_init(md5_ctx *c);
uint32_t rotl32(uint32_t x, unsigned n);
void md5_block(md5_ctx *c, const unsigned char *p);
void md5_update(md5_ctx *c, const void *data, size_t n);
void md5_final(md5_ctx *c, unsigned char out[16]);
void md5_hex(const void *data, size_t n, char out[33]);
void sha1_init(sha1_ctx *c);
void sha1_block(sha1_ctx *c, const unsigned char *p);
void sha1_update(sha1_ctx *c, const void *data, size_t n);
void sha1_final(sha1_ctx *c, unsigned char out[20]);
void sha1_hex(const void *data, size_t n, char *out /* >=41 */);
void sha256_init(sha256_ctx *c);
uint32_t rotr32(uint32_t x, unsigned n);
void sha256_block(sha256_ctx *c, const unsigned char *p);
void sha256_update(sha256_ctx *c, const void *data, size_t n);
void sha256_final(sha256_ctx *c, unsigned char out[32]);
void sha256_hex(const void *data, size_t n, char out[65]);
char *base64_encode(const unsigned char *data, size_t n);
unsigned char *base64_decode(const char *src, size_t *outlen);
char *num_to_pystr(double v, int is_int, long long iv);
void bloom_init(bloom_t *b, uint64_t capacity, double error_rate);
void bloom_free(bloom_t *b);
void bloom_positions(const char *item, uint64_t *h1, uint64_t *h2);
uint64_t bloom_pos_mod(uint64_t h1, uint64_t index, uint64_t h2, uint64_t m);
void bloom_add(bloom_t *b, const char *item);
int bloom_contains(const bloom_t *b, const char *item);
double bloom_saturation(const bloom_t *b);
size_t bloom_bytes_used(const bloom_t *b);
void json_write_string(sb_t *b, const char *s);
void json_write_double(sb_t *b, double v);
json_val *json_new(json_type t);
void json_free(json_val *v);
void jp_ws(jparser *j);
void utf8_put(sb_t *b, uint32_t cp);
int hex4(const char *p, uint32_t *out);
char *jp_string(jparser *j);
json_val *jp_value(jparser *j, int depth);
json_val *json_parse(const char *text);
json_val *json_obj_get(const json_val *o, const char *key);
const char *json_as_str(const json_val *v);
void urlparts_free(urlparts *u);
int scheme_char(int c);
void url_parse_parts(const char *url, urlparts *u);
char *url_unparse(const urlparts *u);
char *url_hostname(const char *url);
char *remove_dot_segments(const char *path);
char *url_join(const char *base, const char *rel);
char *normalize_url_base(const char *url, const char *base);
char *normalize_url(const char *url);
char *get_domain(const char *url);
int is_same_host(const char *u1, const char *u2);
char *stable_id(const char *url);
char *content_hash(const char *text);
char *content_sig(const char *text);
int ip4_forbidden(uint32_t ip);
int ip6_forbidden(const unsigned char *ip);
int ip_is_forbidden(const char *ip);
int is_safe_to_fetch(const char *url, char **reason);
const char *attrs_get(const attrs_t *a, const char *name);
void attrs_free(attrs_t *a);
void html_unescape_into(sb_t *out, const char *s, size_t n);
char *html_unescape(const char *s, size_t n);
int tag_is_ws(int c);
void emit_data(const html_sink *sink, void *ctx, const char *p, size_t n, int decode);
void html_parse(const char *src, size_t len, const html_sink *sink, void *ctx);
int in_list(const char **list, size_t n, const char *tag);
void meta_parser_init(meta_parser *m);
void meta_parser_free(meta_parser *m);
void mp_set_once(char **slot, const char *val);
void mp_starttag(void *ctx, const char *tag, const attrs_t *a);
void mp_endtag(void *ctx, const char *tag);
void mp_data(void *ctx, const char *text, size_t len);
void tp_init(text_parser *t, size_t max_bytes);
void tp_free(text_parser *t);
void tp_starttag(void *ctx, const char *tag, const attrs_t *a);
void tp_endtag(void *ctx, const char *tag);
void tp_data(void *ctx, const char *text, size_t len);
void tp_flush_para(text_parser *t);
char *tp_finish_text(text_parser *t);
void dedupe_paragraphs(vec_t *paras, size_t max_keep);
int cp_is_word(uint32_t cp);
uint32_t utf8_next(const unsigned char **pp);
int count_words(const char *text);
int reading_time_minutes(int wc, int wpm);
void stopword_set_init(map_t *m);
char *detect_language_from_text(const char *text, size_t sample);
char *pick_description(meta_parser *m, vec_t *paragraphs);
const char *iconv_alias(const char *enc);
int encoding_known(const char *enc);
char *decode_body(const unsigned char *body, size_t len, const char *enc);
char *detect_encoding(const unsigned char *body, size_t len, const char *header_charset);
double now_sec(void);
void sleep_ms(long ms);
void logger_init(logger_t *l, int verbose, int quiet, const char *fmt, tui_t *tui, const char *log_file);
void logger_close(logger_t *l);
void logger_emit(logger_t *l, const char *level, const char *msg);
void log_info(logger_t *l, const char *fmt, ...);
void log_warn(logger_t *l, const char *fmt, ...);
void log_debug(logger_t *l, const char *fmt, ...);
void xml_scan_free_event(xml_scan *x);
int xml_scan_next(xml_scan *x);
void sitemap_collect(const char *data, size_t len, vec_t *out, int max_depth, int depth);
void feed_collect(const char *data, size_t len, vec_t *out);
void rule_free(void *p);
void robogroup_free(robogroup_t *g);
void robotsfile_free(robotsfile *rf);
char *py_unquote(const char *s);
char *py_quote_path(const char *s);
void robotsfile_add_entry(robotsfile *rf, robogroup_t *entry);
robogroup_t *robogroup_new(void);
int all_digits(const char *s);
void robotsfile_parse(robotsfile *rf, const char *text);
int robogroup_applies(const robogroup_t *g, const char *useragent);
int robogroup_allowance(const robogroup_t *g, const char *filename);
int robots_can_fetch(robotsfile *rf, const char *ua, const char *url);
double robots_crawl_delay(robotsfile *rf, const char *ua);
void http_response_init(http_response *r);
void http_response_free(http_response *r);
void body_buf_init(body_buf *b, size_t limit);
void body_buf_set_mode(body_buf *b, int mode);
void body_buf_free(body_buf *b);
void body_buf_reserve(body_buf *b, size_t need);
size_t body_buf_write_raw(body_buf *b, const void *data, size_t n);
size_t body_buf_write(body_buf *b, const char *data, size_t n);
void body_buf_flush_tail(body_buf *b);
size_t curl_write_cb(char *ptr, size_t size, size_t nmemb, void *userdata);
char *header_value(const char *line, size_t len, const char *name);
void set_once(char **slot, char *value);
size_t curl_header_cb(char *buffer, size_t size, size_t nitems, void *userdata);
char *charset_from_content_type(const char *ct);
int is_redirect_code(long code);
void http_response_reset_headers(http_response *r);
int http_perform(const char *url, const http_opts *o, http_response *out);
void page_content_init(page_content *c);
void page_content_free(page_content *c);
void fetch_result_init(fetch_result *r);
void fetch_result_free(fetch_result *r);
void cond_free(void *p);
void fetcher_init(fetcher_t *f, const char *ua, double timeout, int max_retries, size_t page_max_bytes, size_t chunk, logger_t *log);
void fetcher_free(fetcher_t *f);
int fetcher_do_request(fetcher_t *f, const char *url, fetch_result *r, long *err_code, double *retry_after, char **err_msg, int conditional_header);
void fetch_parse(fetcher_t *f, fetch_result *r);
void fetcher_fetch(fetcher_t *f, const char *url, fetch_result *out);
void robots_entry_free(void *p);
void robots_cache_init(robots_cache *rc, const char *ua, double timeout, size_t maxsize);
void robots_cache_free(robots_cache *rc);
char *robots_url_for(const char *url);
char *fetch_robots_txt(const char *url, const char *ua, double timeout, int *status);
robots_entry *robots_cache_get(robots_cache *rc, const char *url);
int robots_cache_can_fetch(robots_cache *rc, const char *url);
double robots_cache_crawl_delay(robots_cache *rc, const char *url);
void stats_init(stats_t *s);
void stats_free(stats_t *s);
void stats_record(stats_t *s, fetch_result *r);
void stats_record_fail(stats_t *s, const char *url, const char *reason);
int kv_cmp(const void *a, const void *b);
kv_t *map_top(map_t *m, size_t *out_n);
void json_write_map(sb_t *b, map_t *m, size_t limit);
void stats_to_json(sb_t *b, stats_t *s, int include_failed);
void record_init(record_t *r);
record_t *record_clone(const record_t *r);
char *record_dedupe_key(const record_t *r);
int record_quality_cmp(const record_t *a, const record_t *b);
void crawler_clean_push(crawler_t *c, record_t *rec);
void crawler_clean_flush(crawler_t *c);
void crawler_clean_free(crawler_t *c);
void record_free(record_t *r);
void record_to_json(sb_t *b, const record_t *r);
void json_writer_write(writer_t *w, const char *json);
void json_writer_close(writer_t *w);
writer_t *json_writer_new(const char *path);
void null_writer_write(writer_t *w, const char *json);
void null_writer_close(writer_t *w);
writer_t *null_writer_new(void);
void sqlite_writer_write(writer_t *w, const char *json);
void sqlite_writer_close(writer_t *w);
writer_t *sqlite_writer_new(const char *path);
ntfy_t *ntfy_new(const char *topic, logger_t *log);
void ntfy_free(ntfy_t *n);
int ntfy_send(ntfy_t *n, const char *message, const char *title, int priority, const char *tags, const char *click);
void frontier_init(frontier_t *f, size_t maxsize);
void frontier_free(frontier_t *f);
int fitem_less(const fitem *a, const fitem *b);
void frontier_sift_up(frontier_t *f, size_t i);
void frontier_sift_down(frontier_t *f, size_t i);
int frontier_push(frontier_t *f, const char *url, int depth, const char *base);
int frontier_pop(frontier_t *f, char **url, int *depth, char **base);
int checkpoint_save(const char *path, bloom_t *seen, frontier_t *frontier, long page_count, stats_t *stats, vec_t *seeds, map_t *domain_pages, logger_t *log);
json_val *checkpoint_load(const char *path);
double get_peak_rss_mb(void);
double get_current_rss_mb(void);
int stdout_supports_unicode(void);
int env_flag(const char *name, int def);
void key_reader_init(key_reader_t *k);
int key_reader_poll(key_reader_t *k);
void key_reader_close(key_reader_t *k);
int tui_enabled(tui_t *t);
tui_t *tui_new(double refresh_rate, int force);
void tui_free(tui_t *t);
void tui_out(const char *s);
void tui_enter(tui_t *t);
void tui_exit(tui_t *t);
void tui_add_log(tui_t *t, const char *level, const char *msg);
void tui_clear_log(tui_t *t);
void tui_toggle_info(tui_t *t);
int tui_poll_key(tui_t *t);
void tui_set_state(tui_t *t, const crawl_state_t *s);
void tui_set_current(tui_t *t, const char *url);
char *c_color(const char *fg, int bold, int dim);
size_t ansi_vlen(const char *s);
char *strip_ansi(const char *s);
char *tui_trunc(const char *s, size_t n, int unicode);
void tui_size(int *cols, int *rows);
void fmt_time_short(double seconds, char *out, size_t n);
const char *level_icon(const char *level, int unicode);
void bar_into(sb_t *b, double pct, int width, int unicode);
char *pad_right(const char *s, int width);
void frame_kv_lines(sb_t *out, kv_t *pairs, size_t n, int half, int unicode);
void build_info_panel(sb_t *out, crawl_state_t *s, int cols, int rows, int unicode);
void tui_render(tui_t *t);
void crawler_on_signal(int signum);
double *double_new(double v);
void double_free(void *p);
void regex_vec_free(vec_t *v);
int regex_search(regex_t *re, const char *s);
void crawler_compile_regexes(vec_t *dst, char **patterns, int count);
int url_has_media_extension(const char *url);
void crawler_notify(crawler_t *c, const char *event, const char *message, const char *title, int priority, const char *tags, const char *click);
void crawler_polite_wait(crawler_t *c, const char *url);
int crawler_url_excluded(crawler_t *c, const char *url);
int crawler_domain_capped(crawler_t *c, const char *url);
int jsonld_key_wanted(const char *k);
char *json_scalar_to_str(const json_val *v);
void jsonld_extract(vec_t *raws, map_t *found);
int is_http_url(const char *u);
char *site_origin(const char *url);
char *crawler_favicon_for(crawler_t *c, const char *page_url, const char *declared);
void build_result(crawler_t *c, const char *url, fetch_result *res, record_t *rec, vec_t **links_out, char **robots_meta_out);
void crawler_write(crawler_t *c, record_t *rec);
void crawler_enqueue(crawler_t *c, const char *url, int depth, const char *base_url);
void crawler_discover_sitemaps(crawler_t *c);
int seeds_same_host(vec_t *seeds, const char *url);
void crawler_ingest_sitemap(crawler_t *c, const char *url, int depth);
void crawler_discover_feeds(crawler_t *c);
int crawler_disk_ok(crawler_t *c);
int crawler_validate_output(crawler_t *c, char **reason);
void crawler_cfg_init(crawler_cfg *cfg);
void crawler_restore(crawler_t *c);
crawler_t *crawler_new(crawler_cfg *cfg);
void crawler_free(crawler_t *c);
void crawler_get_state(crawler_t *c, crawl_state_t *st, const char *status);
void crawler_tui_render(crawler_t *c, const char *status);
int crawler_tui_pump(crawler_t *c);
void crawler_checkpoint(crawler_t *c);
int robots_tokens_have(const char *a, const char *b, const char *token);
char *py_dict_repr(map_t *m, size_t limit);
void report_line(logger_t *log, const char *key, const char *value);
void crawler_cleanup(crawler_t *c);
void crawler_final_report(crawler_t *c);
void crawler_run_loop(crawler_t *c);
void crawler_run(crawler_t *c);
void cli_defaults(cli_args *a);
void print_usage(FILE *out);
int need_value(int argc, int i, const char *name);
void cli_parse(cli_args *a, int argc, char **argv);
void load_config_file(const char *path);
int run_html_bench(const char *path, long iterations);
void cli_free(cli_args *a);
int main(int argc, char **argv);

#endif /* GATEWAY_CRAWLER_H */
