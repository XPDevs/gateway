#include "crawler.h"

/* RSS sampling and the ANSI terminal UI */
/* ==========================================================================
 * Section 15 - RSS helpers and the ANSI terminal UI
 * ========================================================================== */

double get_peak_rss_mb(void)
{
    struct rusage ru;
    if (getrusage(RUSAGE_SELF, &ru) != 0) return -1;
    double rss = (double)ru.ru_maxrss;
#ifdef __APPLE__
    return rss / (1024.0 * 1024.0);
#else
    return rss / 1024.0;
#endif
}

double get_current_rss_mb(void)
{
    FILE *fp = fopen("/proc/self/status", "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof line, fp)) {
            if (!strncmp(line, "VmRSS:", 6)) {
                long kb = 0;
                if (sscanf(line + 6, "%ld", &kb) == 1) {
                    fclose(fp);
                    return (double)kb / 1024.0;
                }
            }
        }
        fclose(fp);
    }
    return get_peak_rss_mb();
}

/* -- unicode / env helpers ------------------------------------------------- */
int stdout_supports_unicode(void)
{
    const char *lang = getenv("LC_ALL");
    if (!lang || !*lang) lang = getenv("LC_CTYPE");
    if (!lang || !*lang) lang = getenv("LANG");
    if (!lang) return 0;
    char *low = str_lower(lang);
    int ok = strstr(low, "utf-8") != NULL || strstr(low, "utf8") != NULL;
    free(low);
    return ok;
}

int env_flag(const char *name, int def)
{
    const char *v = getenv(name);
    if (!v) return def;
    char *low = str_lower(v);
    char *t = str_strip_inplace(low);
    int on = !strcmp(t, "1") || !strcmp(t, "true") || !strcmp(t, "yes") ||
             !strcmp(t, "on") || !strcmp(t, "y") || !strcmp(t, "t");
    free(t);
    return on;
}

/* -- key reader ------------------------------------------------------------ */

void key_reader_init(key_reader_t *k)
{
    memset(k, 0, sizeof *k);
    if (!isatty(STDIN_FILENO)) return;
    if (tcgetattr(STDIN_FILENO, &k->old) != 0) return;
    struct termios raw = k->old;
    raw.c_lflag &= (tcflag_t)~(ICANON | ECHO);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) == 0) {
        k->ok = 1;
        k->have_old = 1;
    }
}

int key_reader_poll(key_reader_t *k)
{
    if (!k->ok) return -1;
    fd_set rs;
    FD_ZERO(&rs);
    FD_SET(STDIN_FILENO, &rs);
    struct timeval tv = { 0, 0 };
    int rc = select(STDIN_FILENO + 1, &rs, NULL, NULL, &tv);
    if (rc > 0) {
        unsigned char c = 0;
        if (read(STDIN_FILENO, &c, 1) == 1) return (int)c;
    }
    return -1;
}

void key_reader_close(key_reader_t *k)
{
    if (k->have_old) tcsetattr(STDIN_FILENO, TCSADRAIN, &k->old);
    k->ok = 0;
    k->have_old = 0;
}

/* -- crawl state snapshot -------------------------------------------------- */



int tui_enabled(tui_t *t) { return t && t->enabled; }

tui_t *tui_new(double refresh_rate, int force)
{
    tui_t *t = xcalloc(1, sizeof(tui_t));
    int no_tui = env_flag("NO_TUI", 0);
    t->enabled = (isatty(STDOUT_FILENO) && !no_tui) || force;
    t->unicode = stdout_supports_unicode();
    t->refresh_rate = refresh_rate;
    vec_init(&t->log_lines);
    t->start = now_sec();
    t->current = xstrdup("");
    return t;
}

void tui_free(tui_t *t)
{
    if (!t) return;
    for (size_t i = 0; i < t->log_lines.len; i++) {
        logline_t *l = t->log_lines.items[i];
        free(l->level);
        free(l->msg);
        free(l);
    }
    vec_free(&t->log_lines, NULL);
    free(t->current);
    free(t);
}

void tui_out(const char *s)
{
    fputs(s, stdout);
}

void tui_enter(tui_t *t)
{
    if (!t->enabled) return;
    key_reader_init(&t->keys);
    tui_out("\033[?1049h");
    tui_out("\033[?25l");
    tui_out("\033[2J\033[H");
    fflush(stdout);
}

void tui_exit(tui_t *t)
{
    if (!t->enabled) return;
    key_reader_close(&t->keys);
    tui_out("\033[?25h");
    tui_out("\033[?1049l");
    fflush(stdout);
}

void tui_add_log(tui_t *t, const char *level, const char *msg)
{
    if (!t) return;
    char *clean = xstrdup(msg ? msg : "");
    size_t o = 0;
    for (size_t i = 0; clean[i]; i++) {
        unsigned char c = (unsigned char)clean[i];
        if (c <= 0x08 || (c >= 0x0b && c <= 0x1f) || c == 0x7f) continue;
        clean[o++] = (char)c;
    }
    clean[o] = '\0';
    logline_t *l = xcalloc(1, sizeof(logline_t));
    l->level = xstrdup(level ? level : "INFO");
    l->msg = clean;
    l->ts = now_sec();
    vec_push(&t->log_lines, l);
    while (t->log_lines.len > TUI_LOG_LINES) {
        logline_t *old = t->log_lines.items[0];
        free(old->level);
        free(old->msg);
        free(old);
        memmove(t->log_lines.items, t->log_lines.items + 1,
                (t->log_lines.len - 1) * sizeof(void *));
        t->log_lines.len--;
    }
}

void tui_clear_log(tui_t *t)
{
    for (size_t i = 0; i < t->log_lines.len; i++) {
        logline_t *l = t->log_lines.items[i];
        free(l->level);
        free(l->msg);
        free(l);
    }
    t->log_lines.len = 0;
}

void tui_toggle_info(tui_t *t) { t->info_mode = !t->info_mode; }

int tui_poll_key(tui_t *t) { return key_reader_poll(&t->keys); }

void tui_set_state(tui_t *t, const crawl_state_t *s) { t->state = *s; }

void tui_set_current(tui_t *t, const char *url)
{
    free(t->current);
    t->current = xstrdup(url ? url : "");
}

/* -- frame helpers --------------------------------------------------------- */
const char *C_RESET = "\033[0m";

char *c_color(const char *fg, int bold, int dim)
{
    sb_t b;
    sb_init(&b);
    sb_append(&b, "\033[");
    int first = 1;
    if (bold) { sb_append(&b, "1"); first = 0; }
    if (dim) { if (!first) sb_putc(&b, ';'); sb_append(&b, "2"); first = 0; }
    if (fg) { if (!first) sb_putc(&b, ';'); sb_append(&b, fg); }
    sb_append(&b, "m");
    return sb_detach(&b);
}

size_t ansi_vlen(const char *s)
{
    size_t n = 0;
    for (size_t i = 0; s && s[i]; ) {
        if (s[i] == '\033' && s[i + 1] == '[') {
            i += 2;
            while (s[i] && !(s[i] >= '@' && s[i] <= '~')) i++;
            if (s[i]) i++;
            continue;
        }
        n++;
        i++;
    }
    return n;
}

char *strip_ansi(const char *s)
{
    sb_t b;
    sb_init(&b);
    for (size_t i = 0; s && s[i]; ) {
        if (s[i] == '\033' && s[i + 1] == '[') {
            i += 2;
            while (s[i] && !(s[i] >= '@' && s[i] <= '~')) i++;
            if (s[i]) i++;
            continue;
        }
        sb_putc(&b, s[i++]);
    }
    return sb_detach(&b);
}

/* TUI._trunc: keep the styled string when it fits, else cut the plain text */
char *tui_trunc(const char *s, size_t n, int unicode)
{
    if (n == 0) return xstrdup("");
    char *plain = strip_ansi(s);
    size_t len = utf8_len(plain);
    if (len <= n) { free(plain); return xstrdup(s); }
    char *cut = trunc_chars_add(plain, n, unicode, "\xe2\x80\xa6", ".");
    free(plain);
    return cut;
}

void tui_size(int *cols, int *rows)
{
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
    } else {
        *cols = 100;
        *rows = 30;
    }
}

void fmt_time_short(double seconds, char *out, size_t n)
{
    long s = (long)((seconds > 0) ? seconds : 0);
    long h = s / 3600, rem = s % 3600, m = rem / 60, sec = rem % 60;
    if (h) snprintf(out, n, "%02ld:%02ld:%02ld", h, m, sec);
    else snprintf(out, n, "%02ld:%02ld", m, sec);
}

const char *level_icon(const char *level, int unicode)
{
    if (!strcmp(level, "INFO")) return unicode ? "\033[1;92m\xe2\x9c\x93\033[0m" : "\033[1;92m+\033[0m";
    if (!strcmp(level, "WARN")) return unicode ? "\033[1;93m\xe2\x9a\xa0\033[0m" : "\033[1;93m!\033[0m";
    if (!strcmp(level, "ERROR")) return unicode ? "\033[1;91m\xe2\x9c\x97\033[0m" : "\033[1;91mx\033[0m";
    return unicode ? "\033[90m\xc2\xb7\033[0m" : "\033[90m.\033[0m";
}

void bar_into(sb_t *b, double pct, int width, int unicode)
{
    if (pct < 0) pct = 0;
    if (pct > 1) pct = 1;
    int filled = (int)(width * pct);
    int empty = width - filled;
    if (empty < 0) empty = 0;
    sb_append(b, "[");
    sb_append(b, "\033[92m");
    for (int i = 0; i < filled; i++) sb_append(b, unicode ? "\xe2\x96\x88" : "#");
    sb_append(b, "\033[90m");
    for (int i = 0; i < empty; i++) sb_append(b, unicode ? "\xe2\x96\x91" : "-");
    sb_append(b, C_RESET);
    sb_append(b, "]");
}

/* pad a plain string to width with spaces (f"{s:<w}") */
char *pad_right(const char *s, int width)
{
    sb_t b;
    sb_init(&b);
    sb_append(&b, s ? s : "");
    int len = (int)utf8_len(s ? s : "");
    for (int i = len; i < width; i++) sb_putc(&b, ' ');
    return sb_detach(&b);
}

void frame_kv_lines(sb_t *out, kv_t *pairs, size_t n, int half, int unicode)
{
    for (size_t i = 0; i < n; i++) {
        sb_t line;
        sb_init(&line);
        sb_append(&line, "  \033[36m");
        char *k = pad_right(pairs[i].key, 14);
        sb_append(&line, k);
        free(k);
        sb_append(&line, C_RESET);
        sb_append(&line, "\033[1;97m");
        char num[32];
        snprintf(num, sizeof num, "%ld", pairs[i].count);
        char *v = pad_right(num, 12);
        sb_append(&line, v);
        free(v);
        sb_append(&line, C_RESET);
        char *cut = tui_trunc(line.buf, (size_t)half, unicode);
        sb_append(out, cut);
        sb_append(out, "\n");
        free(cut);
        sb_free(&line);
    }
}

void build_info_panel(sb_t *out, crawl_state_t *s, int cols, int rows, int unicode)
{
    sb_append(out, "\033[1;96m Detailed Stats\033[0m\n");
    int half = (cols - 2) / 2;
    if (half < 20) half = 20;

    size_t nsb = 0;
    kv_t *sb_pairs = map_top(s->status_breakdown, &nsb);
    /* status breakdown keys are numeric codes: present as "HTTP <code>" */
    for (size_t i = 0; i < nsb; i++) {
        char key[32];
        snprintf(key, sizeof key, "HTTP %s", sb_pairs[i].key);
        sb_pairs[i].key = xstrdup(key);
    }
    size_t ntype = 0;
    kv_t *type_pairs = map_top(s->content_types, &ntype);
    if (ntype > 6) ntype = 6;
    size_t nlang = 0;
    kv_t *lang_pairs = map_top(s->languages, &nlang);
    if (nlang > 6) nlang = 6;
    size_t ndom = 0;
    kv_t *dom_pairs = map_top(s->top_domains, &ndom);
    if (ndom > 10) ndom = 10;

    sb_t left, right;
    sb_init(&left);
    sb_init(&right);
    if (nsb) frame_kv_lines(&left, sb_pairs, nsb, half, unicode);
    else sb_append(&left, "  (no responses yet)\n");
    if (ntype) frame_kv_lines(&right, type_pairs, ntype, half, unicode);
    else sb_append(&right, "  (none)\n");

    /* zip the two columns line by line */
    vec_t ll, rl;
    vec_init(&ll);
    vec_init(&rl);
    {
        char *tmp = xstrdup(left.buf);
        char *save = tmp;
        char *line;
        while ((line = strsep(&save, "\n"))) if (*line) vec_push(&ll, xstrdup(line));
        free(tmp);
        tmp = xstrdup(right.buf);
        save = tmp;
        while ((line = strsep(&save, "\n"))) if (*line) vec_push(&rl, xstrdup(line));
        free(tmp);
    }
    size_t maxr = ll.len > rl.len ? ll.len : rl.len;
    for (size_t i = 0; i < maxr; i++) {
        sb_t line;
        sb_init(&line);
        if (i < ll.len) sb_append(&line, ll.items[i]);
        if (i < rl.len) {
            if (i < ll.len) {
                size_t cur = utf8_len(line.buf);
                while ((int)cur < half) { sb_putc(&line, ' '); cur++; }
                sb_append(&line, "  ");
            } else {
                for (int k = 0; k < half; k++) sb_putc(&line, ' ');
            }
            sb_append(&line, rl.items[i]);
        }
        char *cut = tui_trunc(line.buf, (size_t)cols, unicode);
        sb_append(out, cut);
        sb_append(out, "\n");
        free(cut);
        sb_free(&line);
    }
    vec_free(&ll, free);
    vec_free(&rl, free);

    sb_append(out, "\n\033[1;96m Languages\033[0m\n");
    if (nlang) {
        sb_t line;
        sb_init(&line);
        sb_append(&line, "  ");
        for (size_t i = 0; i < nlang; i++) {
            if (i) sb_append(&line, "  ");
            sb_appendf(&line, "\033[36m%s\033[0m=\033[1;97m%ld\033[0m",
                       lang_pairs[i].key, lang_pairs[i].count);
        }
        char *cut = tui_trunc(line.buf, (size_t)cols, unicode);
        sb_append(out, cut);
        sb_append(out, "\n");
        free(cut);
        sb_free(&line);
    } else {
        sb_append(out, "  (none)\n");
    }

    sb_append(out, "\n\033[1;96m Top Domains\033[0m\n");
    if (ndom) {
        for (size_t i = 0; i < ndom; i++) {
            sb_t line;
            sb_init(&line);
            sb_append(&line, "  \033[36m");
            char *d = pad_right(dom_pairs[i].key, 40);
            sb_append(&line, d);
            free(d);
            sb_appendf(&line, "\033[0m  \033[1;97m%ld\033[0m", dom_pairs[i].count);
            char *cut = tui_trunc(line.buf, (size_t)cols, unicode);
            sb_append(out, cut);
            sb_append(out, "\n");
            free(cut);
            sb_free(&line);
        }
    } else {
        sb_append(out, "  (none)\n");
    }

    free(sb_pairs);
    free(type_pairs);
    free(lang_pairs);
    free(dom_pairs);
    sb_free(&left);
    sb_free(&right);
    (void)rows;
}

void tui_render(tui_t *t)
{
    if (!t->enabled) return;
    double now = now_sec();
    if (now - t->last_render < t->refresh_rate) return;
    t->last_render = now;

    int cols, rows;
    tui_size(&cols, &rows);
    if (cols < 60 || rows < 16) {
        printf("\033[H\033[K");
        printf("Terminal too small (%dx%d); need 60x16", cols, rows);
        fflush(stdout);
        return;
    }

    crawl_state_t *s = &t->state;
    const char *status = s->status ? s->status : "RUNNING";
    const char *sc = "97";
    if (!strcmp(status, "RUNNING")) sc = "92";
    else if (!strcmp(status, "PAUSED")) sc = "93";
    else if (!strcmp(status, "FETCHING")) sc = "96";
    else if (!strncmp(status, "STOP", 4)) sc = "91";

    sb_t frame;
    sb_init(&frame);

    /* colour strings live for the whole frame (no use-after-free) */
    char *c36 = c_color("36", 0, 0);
    char *c97b = c_color("97", 1, 0);
    char *c37d = c_color("37", 0, 1);
    char *c37 = c_color("37", 0, 0);
    char *c97 = c_color("97", 1, 0);

    /* border + title */
    sb_t sepb;
    sb_init(&sepb);
    sb_append(&sepb, "\033[90m");
    for (int i = 0; i < cols; i++) sb_append(&sepb, t->unicode ? "\xe2\x94\x80" : "-");
    sb_append(&sepb, C_RESET);
    char *SEP = sb_detach(&sepb);

    sb_t title;
    sb_init(&title);
    char *c96 = c_color("96", 1, 0);
    char *csc = c_color(sc, 1, 0);
    char tstr[32];
    snprintf(tstr, sizeof tstr, " Gateway Crawler v%s ", VERSION);
    sb_append(&title, c96);
    sb_append(&title, tstr);
    sb_append(&title, C_RESET);
    char status_plain[64], tfmt[32];
    fmt_time_short(s->elapsed, tfmt, sizeof tfmt);
    snprintf(status_plain, sizeof status_plain, "%s   %s ", status, tfmt);
    int gap = cols - (int)strlen(tstr) - (int)strlen(status_plain) - 2;
    if (gap < 1) gap = 1;
    for (int i = 0; i < gap; i++) sb_putc(&title, ' ');
    sb_append(&title, csc);
    sb_append(&title, status);
    sb_append(&title, C_RESET);
    sb_append(&title, "   ");
    sb_append(&title, c37d);
    sb_append(&title, tfmt);
    sb_append(&title, " ");
    sb_append(&title, C_RESET);
    char *cut = tui_trunc(title.buf, (size_t)cols, t->unicode);
    sb_append(&frame, cut);
    sb_append(&frame, "\n");
    free(cut);
    sb_free(&title);
    free(c96); free(csc);

    sb_append(&frame, SEP);
    sb_append(&frame, "\n");

    /* progress line */
    sb_t prog;
    sb_init(&prog);
    if (s->max_pages > 0) {
        double pct = (double)s->pages / (double)s->max_pages;
        int bar_w = cols - 30;
        if (bar_w < 10) bar_w = 10;
        sb_append(&prog, " ");
        sb_append(&prog, c37);
        sb_append(&prog, "Progress ");
        sb_append(&prog, C_RESET);
        bar_into(&prog, pct, bar_w, t->unicode);
        sb_append(&prog, " ");
        sb_append(&prog, c97);
        char nums[96];
        char *a = fmt_thousands(s->pages);
        char *b = fmt_thousands(s->max_pages);
        snprintf(nums, sizeof nums, "%s/%s", a, b);
        free(a); free(b);
        sb_append(&prog, nums);
        sb_append(&prog, C_RESET);
        sb_append(&prog, " ");
        sb_append(&prog, c37d);
        sb_appendf(&prog, "(%.1f%%)", pct * 100.0);
        sb_append(&prog, C_RESET);
    } else {
        char *pages_s = fmt_thousands(s->pages);
        sb_append(&prog, " ");
        sb_append(&prog, c37);
        sb_append(&prog, "Pages ");
        sb_append(&prog, C_RESET);
        sb_append(&prog, c97);
        sb_append(&prog, pages_s);
        sb_append(&prog, C_RESET);
        sb_append(&prog, "  ");
        sb_append(&prog, c37d);
        sb_append(&prog, "(no limit)");
        sb_append(&prog, C_RESET);
        free(pages_s);
    }
    cut = tui_trunc(prog.buf, (size_t)cols, t->unicode);
    sb_append(&frame, cut);
    sb_append(&frame, "\n");
    free(cut);
    sb_free(&prog);

    sb_append(&frame, "\n");

    /* stats rows */
    sb_t r1;
    sb_init(&r1);
    {
        char *pages_s = fmt_thousands(s->pages);
        char *queue_s = fmt_thousands(s->queue);
        char *seen_s = fmt_thousands((long long)s->seen);
        sb_append(&r1, " ");
        sb_append(&r1, c36); sb_append(&r1, "Pages "); sb_append(&r1, C_RESET);
        sb_append(&r1, c97b); sb_append(&r1, pages_s); sb_append(&r1, C_RESET);
        sb_append(&r1, "   ");
        sb_append(&r1, c36); sb_append(&r1, "Queue "); sb_append(&r1, C_RESET);
        sb_append(&r1, c97b); sb_append(&r1, queue_s); sb_append(&r1, C_RESET);
        sb_append(&r1, "   ");
        sb_append(&r1, c36); sb_append(&r1, "Seen "); sb_append(&r1, C_RESET);
        sb_append(&r1, c97b); sb_append(&r1, seen_s); sb_append(&r1, C_RESET);
        sb_append(&r1, "   ");
        sb_append(&r1, c36); sb_append(&r1, "Rate "); sb_append(&r1, C_RESET);
        sb_append(&r1, c97b); sb_appendf(&r1, "%.1f/min", s->rate); sb_append(&r1, C_RESET);
        free(pages_s); free(queue_s); free(seen_s);
    }
    cut = tui_trunc(r1.buf, (size_t)cols, t->unicode);
    sb_append(&frame, cut);
    sb_append(&frame, "\n");
    free(cut);
    sb_free(&r1);

    sb_t r2;
    sb_init(&r2);
    {
        const char *rss_c = (s->rss < 80) ? "92" : (s->rss < 100 ? "93" : "91");
        char *crss = c_color(rss_c, 0, 0);
        char *err_s = fmt_thousands(s->errors);
        char *rob_s = fmt_thousands(s->robots);
        char *dup_s = fmt_thousands(s->dupes);
        sb_append(&r2, " ");
        sb_append(&r2, c36); sb_append(&r2, "RSS "); sb_append(&r2, C_RESET);
        sb_append(&r2, crss); sb_appendf(&r2, "%.1fMB", s->rss); sb_append(&r2, C_RESET);
        sb_append(&r2, "   ");
        sb_append(&r2, c36); sb_append(&r2, "Bloom "); sb_append(&r2, C_RESET);
        sb_append(&r2, c97b); sb_appendf(&r2, "%.2f%%", s->bloom * 100.0); sb_append(&r2, C_RESET);
        sb_append(&r2, "   ");
        sb_append(&r2, c36); sb_append(&r2, "Err "); sb_append(&r2, C_RESET);
        sb_append(&r2, c97b); sb_append(&r2, err_s); sb_append(&r2, C_RESET);
        sb_append(&r2, "   ");
        sb_append(&r2, c36); sb_append(&r2, "Robots "); sb_append(&r2, C_RESET);
        sb_append(&r2, c97b); sb_append(&r2, rob_s); sb_append(&r2, C_RESET);
        sb_append(&r2, "   ");
        sb_append(&r2, c36); sb_append(&r2, "Dupes "); sb_append(&r2, C_RESET);
        sb_append(&r2, c97b); sb_append(&r2, dup_s); sb_append(&r2, C_RESET);
        free(crss); free(err_s); free(rob_s); free(dup_s);
    }
    cut = tui_trunc(r2.buf, (size_t)cols, t->unicode);
    sb_append(&frame, cut);
    sb_append(&frame, "\n");
    free(cut);
    sb_free(&r2);

    sb_append(&frame, SEP);
    sb_append(&frame, "\n");

    /* current URL */
    if (t->current && *t->current) {
        sb_t cur;
        sb_init(&cur);
        sb_append(&cur, " ");
        sb_append(&cur, c36);
        sb_append(&cur, "Current ");
        sb_append(&cur, C_RESET);
        sb_append(&cur, c37d);
        char *c = tui_trunc(t->current, (size_t)(cols - 10), t->unicode);
        sb_append(&cur, c);
        free(c);
        sb_append(&cur, C_RESET);
        cut = tui_trunc(cur.buf, (size_t)cols, t->unicode);
        sb_append(&frame, cut);
        sb_append(&frame, "\n");
        free(cut);
        sb_free(&cur);
    } else {
        sb_append(&frame, "\n");
    }

    int reserved = 11;
    int panel_rows = rows - reserved;
    if (panel_rows < 3) panel_rows = 3;
    if (panel_rows > rows - reserved) panel_rows = rows - reserved;

    if (t->info_mode) {
        build_info_panel(&frame, s, cols, panel_rows, t->unicode);
    } else {
        sb_append(&frame, "\033[1;96m Recent Activity\033[0m\n");
        size_t avail = (panel_rows > 1) ? (size_t)(panel_rows - 1) : 0;
        size_t start = t->log_lines.len > avail ? t->log_lines.len - avail : 0;
        for (size_t i = start; i < t->log_lines.len; i++) {
            logline_t *l = t->log_lines.items[i];
            sb_t line;
            sb_init(&line);
            sb_append(&line, " ");
            sb_append(&line, level_icon(l->level, t->unicode));
            sb_append(&line, " ");
            size_t prefix_len = ansi_vlen(" ") + 1 + ansi_vlen(" ");
            if ((size_t)cols > prefix_len + 1) {
                char *body = tui_trunc(l->msg, (size_t)cols - prefix_len - 1, t->unicode);
                sb_append(&line, body);
                free(body);
            }
            cut = tui_trunc(line.buf, (size_t)cols, t->unicode);
            sb_append(&frame, cut);
            sb_append(&frame, "\n");
            free(cut);
            sb_free(&line);
        }
    }

    sb_append(&frame, SEP);
    sb_append(&frame, "\n");

    sb_t hint;
    sb_init(&hint);
    sb_append(&hint, " ");
    sb_append(&hint, c36); sb_append(&hint, "[q]"); sb_append(&hint, C_RESET);
    sb_append(&hint, "uit  ");
    sb_append(&hint, c36); sb_append(&hint, "[p]"); sb_append(&hint, C_RESET);
    sb_append(&hint, "ause  ");
    sb_append(&hint, c36); sb_append(&hint, "[i]"); sb_append(&hint, C_RESET);
    sb_append(&hint, "nfo  ");
    sb_append(&hint, c36); sb_append(&hint, "[c]"); sb_append(&hint, C_RESET);
    sb_append(&hint, "lear");
    if (t->info_mode) {
        sb_append(&hint, "   \033[93m(info mode)\033[0m");
    }
    cut = tui_trunc(hint.buf, (size_t)cols, t->unicode);
    sb_append(&frame, cut);
    sb_append(&frame, "\n");
    free(cut);
    sb_free(&hint);
    free(c36); free(c97b); free(c37d); free(c37); free(c97);

    /* emit: clear to end of each line, pad the rest of the screen */
    tui_out("\033[H");
    char *save = frame.buf;
    char *line;
    int emitted = 0;
    while ((line = strsep(&save, "\n")) != NULL) {
        if (!*line && save == NULL) break;   /* trailing token after the last newline */
        tui_out(line);
        tui_out("\033[K\r\n");
        emitted++;
    }
    for (int i = emitted; i < rows; i++) tui_out("\033[K\r\n");
    fflush(stdout);
    sb_free(&frame);
    free(SEP);
}
