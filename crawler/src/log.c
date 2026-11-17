#include "crawler.h"

/* clock helpers and the logger */
/* ==========================================================================
 * Section 9 - clock, logger
 * ========================================================================== */

double now_sec(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1e6;
}

void sleep_ms(long ms)
{
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}



void tui_add_log(tui_t *t, const char *level, const char *msg);
int  tui_enabled(tui_t *t);

void logger_init(logger_t *l, int verbose, int quiet, const char *fmt,
                        tui_t *tui, const char *log_file)
{
    memset(l, 0, sizeof *l);
    l->verbose = verbose;
    l->quiet = quiet;
    l->fmt = xstrdup(fmt ? fmt : "text");
    l->tui = tui;
    l->t0 = now_sec();
    if (log_file && *log_file) {
        l->file = fopen(log_file, "w");
        if (l->file) {
            char stamp[32];
            now_string(stamp, sizeof stamp);
            fprintf(l->file, "Gateway crawler debug log started: %s\n", stamp);
            fflush(l->file);
        }
    }
}

void logger_close(logger_t *l)
{
    if (l->file) {
        fprintf(l->file, "{\"t\": %.3f, \"level\": \"INFO\", \"msg\": \"debug log closed\"}\n",
                now_sec() - l->t0);
        fclose(l->file);
        l->file = NULL;
    }
    free(l->fmt);
    l->fmt = NULL;
}

void logger_emit(logger_t *l, const char *level, const char *msg)
{
    double elapsed = py_round((now_sec() - l->t0) * 1000.0) / 1000.0;
    if (l->file) {
        sb_t rec;
        sb_init(&rec);
        sb_appendf(&rec, "{\"t\": %.3f, \"level\": ", elapsed);
        json_write_string(&rec, level);
        sb_append(&rec, ", \"msg\": ");
        json_write_string(&rec, msg ? msg : "");
        sb_append(&rec, "}");
        fprintf(l->file, "%s\n", rec.buf);
        fflush(l->file);
        sb_free(&rec);
    }
    if (l->quiet && (!strcmp(level, "INFO") || !strcmp(level, "DEBUG"))) return;
    if (!strcmp(level, "DEBUG") && !l->verbose) return;
    if (l->tui && tui_enabled(l->tui)) {
        tui_add_log(l->tui, level, msg);
        return;
    }
    if (!strcmp(l->fmt, "json")) {
        sb_t rec;
        sb_init(&rec);
        sb_appendf(&rec, "{\"t\": %.3f, \"level\": ", elapsed);
        json_write_string(&rec, level);
        sb_append(&rec, ", \"msg\": ");
        json_write_string(&rec, msg ? msg : "");
        sb_append(&rec, "}");
        fprintf(stderr, "%s\n", rec.buf);
        fflush(stderr);
        sb_free(&rec);
    } else {
        fprintf(stderr, "[%s] %s\n", level, msg ? msg : "");
        fflush(stderr);
    }
}

void log_info(logger_t *l, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    char *msg = NULL;
    if (vasprintf(&msg, fmt, ap) < 0) msg = xstrdup("");
    va_end(ap);
    logger_emit(l, "INFO", msg);
    free(msg);
}

void log_warn(logger_t *l, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    char *msg = NULL;
    if (vasprintf(&msg, fmt, ap) < 0) msg = xstrdup("");
    va_end(ap);
    logger_emit(l, "WARN", msg);
    free(msg);
}

void log_debug(logger_t *l, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    char *msg = NULL;
    if (vasprintf(&msg, fmt, ap) < 0) msg = xstrdup("");
    va_end(ap);
    logger_emit(l, "DEBUG", msg);
    free(msg);
}
