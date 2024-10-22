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

static thread_local n00b_grid_t *n00b_trace_grid;

struct backtrace_state *btstate;

static int
n00b_bt_create_backtrace(void       *data,
                        uintptr_t   pc,
                        const char *pathname,
                        int         line_number,
                        const char *function)
{
    n00b_utf8_t *file;
    n00b_utf8_t *fn;

    if (pathname == NULL) {
        file = n00b_rich_lit("[em]??[/] (unavailable)");
    }
    else {
        char *filename = rindex(pathname, '/');
        if (filename) {
            filename++;
        }
        else {
            filename = (char *)pathname;
        }

        file = n00b_cstr_format("{}:{}",
                               n00b_new_utf8(filename),
                               n00b_box_u64(line_number));
    }

    if (function == NULL) {
        fn = n00b_rich_lit("[em]??[/] (unavailable)");
    }
    else {
        fn = n00b_new_utf8(function);
    }

    n00b_list_t *x = n00b_list(n00b_type_utf8());

    n00b_list_append(x, n00b_cstr_format("{:18x}", n00b_box_u64(pc)));
    n00b_list_append(x, file);
    n00b_list_append(x, fn);
    n00b_grid_add_row(n00b_trace_grid, x);

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

#define backtrace_core(nframes)                                     \
    n00b_trace_grid = n00b_new(n00b_type_grid(),                       \
                             n00b_kw("start_cols",                   \
                                    n00b_ka(3),                      \
                                    "start_rows",                   \
                                    n00b_ka(2),                      \
                                    "header_rows",                  \
                                    n00b_ka(1),                      \
                                    "container_tag",                \
                                    n00b_ka(n00b_new_utf8("table2")), \
                                    "stripe",                       \
                                    n00b_ka(true)));                 \
                                                                    \
    n00b_list_t *x = n00b_list(n00b_type_utf8());                      \
    n00b_list_append(x, n00b_new_utf8("PC"));                         \
    n00b_list_append(x, n00b_new_utf8("Location"));                   \
    n00b_list_append(x, n00b_new_utf8("Function"));                   \
                                                                    \
    n00b_grid_add_row(n00b_trace_grid, x);                            \
                                                                    \
    n00b_snap_column(n00b_trace_grid, 0);                             \
    n00b_snap_column(n00b_trace_grid, 1);                             \
    n00b_snap_column(n00b_trace_grid, 2);                             \
                                                                    \
    backtrace_full(btstate, nframes, n00b_bt_create_backtrace, n00b_bt_err, NULL);

void
n00b_print_c_backtrace()
{
    backtrace_core(1);
    n00b_printf("[h6]C Stack Trace:");
    n00b_print(n00b_trace_grid);
}

void
n00b_backtrace_init(char *fname)
{
    btstate = backtrace_create_state(fname, 1, n00b_bt_err, NULL);
}

n00b_grid_t *
n00b_get_c_backtrace(int skips)
{
    backtrace_core(skips);
    return n00b_trace_grid;
}

void
n00b_static_c_backtrace()
{
    backtrace_full(btstate, 0, n00b_bt_static_backtrace, n00b_bt_err, NULL);
}

#else
n00b_grid_t *
n00b_get_c_backtrace(int skips)
{
    return n00b_callout(n00b_new_utf8("Stack traces not enabled."));
}

void
n00b_static_c_backtrace()
{
    printf("Stack traces not enabled.\n");
}

#endif
