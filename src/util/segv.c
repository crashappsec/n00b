#include "n00b.h"

extern n00b_vmthread_t *n00b_thread_runtime_acquire();
extern n00b_grid_t     *n00b_get_backtrace(n00b_vmthread_t *);
extern int16_t         *n00b_calculate_col_widths(n00b_grid_t *, int16_t, int16_t *);

static void (*n00b_crash_callback)()   = NULL;
static bool n00b_show_c_trace_on_crash = true;

void
n00b_set_crash_callback(void (*cb)())
{
    n00b_crash_callback = cb;
}

void
n00b_set_show_c_trace_on_crash(bool n)
{
    n00b_show_c_trace_on_crash = n;
}

static void
crash_print_trace(char *title, n00b_grid_t *g)
{
    n00b_utf8_t *t = n00b_rich_lit(title);
    cprintf("%s\n%s\n", t, g);
}

static void
n00b_crash_handler(int n)
{
    n00b_vmthread_t *runtime    = n00b_thread_runtime_acquire();
    n00b_grid_t     *n00b_trace = NULL;
    n00b_grid_t     *ct         = NULL;

    if (runtime && runtime->running) {
        n00b_trace = n00b_get_backtrace(runtime);
    }

    n00b_utf8_t *s = n00b_rich_lit("[h5]Program crashed due to SIGSEGV.");
    cprintf("%s", s);

#if defined(N00B_BACKTRACE_SUPPORTED)
    if (n00b_show_c_trace_on_crash) {
        ct = n00b_get_c_backtrace(1);

        if (runtime && runtime->running) {
            int16_t  tmp;
            int16_t *widths1 = n00b_calculate_col_widths(ct,
                                                         N00B_GRID_TERMINAL_DIM,
                                                         &tmp);
            int16_t *widths2 = n00b_calculate_col_widths(n00b_trace,
                                                         N00B_GRID_TERMINAL_DIM,
                                                         &tmp);

            for (int i = 0; i < 3; i++) {
                int w = n00b_max(widths1[i], widths2[i]);

                n00b_render_style_t *s = n00b_new(n00b_type_render_style(),
                                                  n00b_kw("min_size",
                                                          n00b_ka(w),
                                                          "max_size",
                                                          n00b_ka(w),
                                                          "left_pad",
                                                          n00b_ka(1),
                                                          "right_pad",
                                                          n00b_ka(1)));
                n00b_set_column_props(ct, i, s);
                n00b_set_column_props(n00b_trace, i, s);
            }
        }
    }

#endif
    if (runtime && runtime->running) {
        crash_print_trace("[h6]N00b stack trace:", n00b_trace);
    }

#if defined(N00B_BACKTRACE_SUPPORTED)
    if (n00b_show_c_trace_on_crash) {
        crash_print_trace("[h6]C stack trace:", ct);
    }
#endif

    if (n00b_crash_callback) {
        (*n00b_crash_callback)();
    }

    n00b_exit(139);
}

void
n00b_crash_init()
{
    signal(SIGSEGV, n00b_crash_handler);
    signal(SIGBUS, n00b_crash_handler);
}
