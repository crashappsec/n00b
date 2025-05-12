#define N00B_USE_INTERNAL_API
#include "n00b.h"

static bool                        exiting         = false;
static int                         saved_exit_code = 0;
extern _Atomic(n00b_condition_t *) n00b_io_exit_request;
extern bool                        n00b_io_exited;

#ifdef N00B_DEBUG_EXIT_TRACE
#define show_trace() n00b_static_c_backtrace()
#else
#define show_trace()
#endif

// Most of the work happens in tsi by the pthread destructor (n00b_tsi_cleanup)
_Noreturn void
n00b_thread_exit(void *result)
{
    // This has a potential race condition, but given we always should
    // have an IO thread running until actual shutdown, I think it's
    // okay.
    if (n00b_thread_run_count() == 1) {
        exit(saved_exit_code);
    }

    n00b_gts_suspend();
    pthread_exit(result);
}

static void
n00b_wait_on_io_shutdown(void)
{
    n00b_gts_suspend();
    n00b_condition_t *c = atomic_read(&n00b_io_exit_request);

    if (!c) {
        n00b_condition_t *req = n00b_new(n00b_type_condition());
        if (!CAS(&n00b_io_exit_request, &c, req)) {
            req = c;
        }
    }

    n00b_condition_lock_acquire(n00b_io_exit_request);
    if (!n00b_io_exited) {
        n00b_condition_wait(n00b_io_exit_request);
    }
    n00b_condition_lock_release(n00b_io_exit_request);
}

_Noreturn void
n00b_exit(int code)
{
    saved_exit_code = code;
    exiting         = true;
    n00b_wait_on_io_shutdown();
    n00b_thread_cancel_other_threads();
    n00b_thread_exit(NULL);
}

_Noreturn void
n00b_abort(void)
{
    saved_exit_code = 139;
    exiting         = true;
    n00b_gts_notify_abort();
    n00b_wait_on_io_shutdown();
    n00b_thread_cancel_other_threads();
    abort();
}

bool
n00b_current_process_is_exiting(void)
{
    return exiting;
}
