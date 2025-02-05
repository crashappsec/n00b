#include "n00b.h"

extern n00b_table_t *n00b_get_backtrace(n00b_vmthread_t *);
extern int16_t      *n00b_calculate_col_widths(n00b_table_t *,
                                               int16_t,
                                               int16_t *);

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
crash_print_trace(char *title, n00b_table_t *tbl)
{
    n00b_string_t *t = n00b_crich(title);
    cprintf("%s\n%s\n", t, n00b_to_string(tbl));
}

static void
n00b_crash_handler(int n)
{
    n00b_vmthread_t *runtime    = n00b_thread_runtime_acquire();
    n00b_table_t    *n00b_trace = NULL;
    n00b_table_t    *ct         = NULL;

    if (runtime && runtime->running) {
        n00b_trace = n00b_get_backtrace(runtime);
    }

    n00b_string_t *s = n00b_crich("«em5»Program crashed due to SIGSEGV.");
    cprintf("%s", s);

#if defined(N00B_BACKTRACE_SUPPORTED)
    if (n00b_show_c_trace_on_crash) {
        ct = n00b_get_c_backtrace(1);
    }
#endif
    if (runtime && runtime->running) {
        crash_print_trace("«em6»N00b stack trace:", n00b_trace);
    }

#if defined(N00B_BACKTRACE_SUPPORTED)
    if (n00b_show_c_trace_on_crash) {
        crash_print_trace("«em6»C stack trace:", ct);
    }
#endif

    if (n00b_crash_callback) {
        (*n00b_crash_callback)();
    }

    n00b_abort();
}

void
n00b_crash_init()
{
    signal(SIGSEGV, n00b_crash_handler);
    signal(SIGBUS, n00b_crash_handler);
}
