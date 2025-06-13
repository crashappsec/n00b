#define N00B_USE_INTERNAL_API
#include "n00b.h"

// Timers are monitored from within the event loop.
n00b_timer_t *
_n00b_add_timer(n00b_duration_t *time,
                n00b_timer_cb    action,
                ...)

{
    va_list            args;
    void              *thunk = NULL;
    n00b_event_loop_t *loop  = NULL;

    va_start(args, action);
    int nargs = va_arg(args, int);

    if (nargs) {
        thunk = va_arg(args, void *);

        if (nargs != 1) {
            loop = va_arg(args, void *);
        }
    }

    n00b_timer_t *result = n00b_gc_alloc_mapped(n00b_timer_t,
                                                N00B_GC_SCAN_ALL);

    if (!loop) {
        loop = n00b_system_dispatcher;
    }

    result->thunk     = thunk;
    result->stop_time = n00b_duration_add(n00b_now(), time);
    result->action    = action;
    result->loop      = loop;

    n00b_list_append(loop->timers, result);

    return result;
}

void
n00b_remove_timer(n00b_timer_t *timer)
{
    n00b_list_remove_item(timer->loop->timers, timer);
}
