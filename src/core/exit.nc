#define N00B_USE_INTERNAL_API
#include "n00b.h"

static bool             exiting         = false;
static int              saved_exit_code = 0;
extern n00b_condition_t n00b_io_exit_request;
extern bool             n00b_io_exited;
bool                    n00b_abort_signal = false;

#ifdef N00B_DEBUG_EXIT_TRACE
#define show_trace() n00b_static_c_backtrace()
#else
#define show_trace()
#endif

// Most of the work happens in tsi by the pthread destructor (n00b_tsi_cleanup)
_Noreturn void
n00b_thread_exit(void *result)
{
    n00b_tsi_t *tsi = n00b_get_tsi_ptr();

    n00b_dlog_thread1("n00b_thread_exit() called.");
    // This has a potential race condition, but given we always should
    // have an IO thread running until actual shutdown, I think it's
    // okay.
    if (!n00b_thread_run_count()) {
        n00b_dlog_thread1("Last thread exiting.");

        if (!tsi->thread_id) {
            n00b_post_thread_cleanup(tsi);
        }
        exit(saved_exit_code);
    }

    n00b_dlog_thread1("After exit, %d threads remain.", n00b_thread_run_count());
    if (!tsi->thread_id) {
        n00b_post_thread_cleanup(tsi);
    }

    pthread_exit(result);
}

#if 0
static void
n00b_wait_on_io_shutdown(void)
{
    n00b_condition_t *c = &n00b_io_exit_request;

    while (!n00b_io_exited) {
        n00b_condition_lock(c);
        n00b_condition_wait(c, auto_unlock: true, timeout: 100000);
    }
}
#endif

_Noreturn void
n00b_exit(int code)
{
    n00b_dlog_thread1("n00b_exit() called.");

    // Wake any long-term blocked threads.
    kill(getpid(), SIGINT);
    saved_exit_code = code;
    exiting         = true;
    n00b_thread_cancel_other_threads();
    //    n00b_wait_on_io_shutdown();
    n00b_thread_exit(NULL);
}

_Noreturn void
n00b_abort(void)
{
    n00b_dlog_thread1("n00b_abort() called.");
    saved_exit_code   = 139;
    exiting           = true;
    n00b_abort_signal = true;
    //    n00b_wait_on_io_shutdown();
    n00b_thread_cancel_other_threads();
    abort();
}

bool
n00b_current_process_is_exiting(void)
{
    return exiting;
}
