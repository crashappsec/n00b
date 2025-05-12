#include "n00b.h"

#if defined(N00B_DEBUG) && !defined(N00B_BACKTRACE_SUPPORTED)
#warning "Cannot add debug stack traces to exceptions: libbacktrace not supported"
#endif

#ifdef N00B_BACKTRACE_SUPPORTED

static void
n00b_bt_err(void *data, const char *msg, int errnum)
{
    fprintf(stderr, "ERROR: %s (%d)", msg, errnum);
}

struct backtrace_state *btstate;

static int
n00b_bt_create_backtrace(void       *data,
                         uintptr_t   pc,
                         const char *pathname,
                         int         line_number,
                         const char *function)
{
    n00b_string_t *file;
    n00b_string_t *fn;

    if (pathname == NULL) {
        file = n00b_crich("«em»??«/» (unavailable)");
    }
    else {
        char *filename = rindex(pathname, '/');
        if (filename) {
            filename++;
        }
        else {
            filename = (char *)pathname;
        }

        file = n00b_cformat("«#»:«#»",
                            n00b_cstring(filename),
                            (int64_t)line_number);
    }

    if (function == NULL) {
        fn = n00b_crich("«em2»??«/» (unavailable)");
    }
    else {
        fn = n00b_cstring(function);
    }

    n00b_list_t *x = n00b_list(n00b_type_string());

    n00b_list_append(x, n00b_cformat("«#:p»", pc));
    n00b_list_append(x, file);
    n00b_list_append(x, fn);
    n00b_table_add_row(n00b_get_tsi_ptr()->trace_table, x);

    return 0;
};

static int
n00b_bt_static_backtrace(void       *data,
                         uintptr_t   pc,
                         const char *pathname,
                         int         n,
                         const char *function)
{
    static const char *unavailable = "?? (unavailable)";

    char *path;
    char *fn = (char *)function;
    if (pathname == NULL) {
        path = (char *)unavailable;
    }
    else {
        path = rindex(pathname, '/');
        if (path) {
            path++;
        }
        else {
            path = (char *)pathname;
        }
    }

    if (fn == NULL) {
        fn = (char *)unavailable;
    }

    fprintf(stderr, "pc: %p: file: %s:%d; func: %s\n", (void *)pc, path, n, fn);

    return 0;
}

static int
n00b_bt_utf8(void       *data,
             uintptr_t   pc,
             const char *pathname,
             int         n,
             const char *function)
{
    static const char *unavailable = "?? (unavailable)";
    n00b_tsi_t        *tsi         = n00b_get_tsi_ptr();

    char *path;
    char *fn = (char *)function;

    if (pathname == NULL) {
        path = (char *)unavailable;
    }
    else {
        path = rindex(pathname, '/');
        if (path) {
            path++;
        }
        else {
            path = (char *)pathname;
        }
    }

    if (fn == NULL) {
        fn = (char *)unavailable;
    }

    char *s1 = "pc: ";
    char *s2 = n00b_cformat("«#:x»", pc)->data;
    char *s3 = " file:";
    char *s4 = path;
    char *s5 = ":";
    char *s6 = n00b_string_from_int(n)->data;
    char *s7 = "; func: ";
    char *s8 = fn;
    char *s9 = "\n";

    int l = strlen(tsi->bt_utf8_result) + strlen(s1) + strlen(s2) + strlen(s3)
          + strlen(s4) + strlen(s5) + strlen(s6) + strlen(s7) + strlen(s8)
          + strlen(s9);

    char *new = n00b_gc_raw_alloc(l + 1, NULL);
    char *p;

    strcpy(new, tsi->bt_utf8_result);
    p = new + strlen(tsi->bt_utf8_result);
    strcpy(p, s1);
    p += strlen(s1);
    strcpy(p, s2);
    p += strlen(s2);
    strcpy(p, s3);
    p += strlen(s3);
    strcpy(p, s4);
    p += strlen(s4);
    strcpy(p, s5);
    p += strlen(s5);
    strcpy(p, s6);
    p += strlen(s6);
    strcpy(p, s7);
    p += strlen(s7);
    strcpy(p, s8);
    p += strlen(s8);
    strcpy(p, s9);
    p += strlen(s9);
    *p = 0;

    tsi->bt_utf8_result = new;

    return 0;
}

#define backtrace_core(nframes)                                \
    {                                                          \
        n00b_tsi_t   *tsi = n00b_get_tsi_ptr();                \
        n00b_table_t *tbl = n00b_table("columns", n00b_ka(3)); \
                                                               \
        tsi->trace_table = tbl;                                \
                                                               \
        n00b_table_add_cell(tbl, n00b_cstring("PC"));          \
        n00b_table_add_cell(tbl, n00b_cstring("Location"));    \
        n00b_table_add_cell(tbl, n00b_cstring("Function"));    \
                                                               \
        backtrace_full(btstate,                                \
                       nframes,                                \
                       n00b_bt_create_backtrace,               \
                       n00b_bt_err,                            \
                       NULL);                                  \
    }

void
n00b_print_c_backtrace()
{
    backtrace_core(1);
    n00b_printf("«em6»C Stack Trace:");
    n00b_print(n00b_get_tsi_ptr()->trace_table);
}

void
n00b_backtrace_init(char *fname)
{
    btstate = backtrace_create_state(fname, 1, n00b_bt_err, NULL);
}

n00b_table_t *
n00b_get_c_backtrace(int skips)
{
    backtrace_core(skips);
    return n00b_get_tsi_ptr()->trace_table;
}

void
n00b_static_c_backtrace(void)
{
    backtrace_full(btstate, 0, n00b_bt_static_backtrace, n00b_bt_err, NULL);
}

n00b_string_t *
n00b_backtrace_utf8(void)
{
    n00b_tsi_t *tsi = n00b_get_tsi_ptr();

    if (tsi->u8_backtracing) {
        return NULL;
    }
    tsi->u8_backtracing = true;

    tsi->bt_utf8_result = "Backtrace:\n";
    backtrace_full(btstate, 0, n00b_bt_utf8, n00b_bt_err, NULL);

    tsi->u8_backtracing = false;

    n00b_string_t *res = n00b_cstring(tsi->bt_utf8_result);

    return res;
}

#else
n00b_table_t *
n00b_get_c_backtrace(int skips)
{
    return n00b_callout(n00b_cstring("Stack traces not enabled."));
}

void
n00b_static_c_backtrace()
{
    printf("Stack traces not enabled.\n");
}

#endif
