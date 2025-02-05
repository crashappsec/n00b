#define N00B_USE_INTERNAL_API
#include "n00b.h"

static bool exiting         = false;
static int  saved_exit_code = 0;

#ifdef N00B_DEBUG_EXIT_TRACE
#define show_trace() n00b_static_c_backtrace()
#else
#define show_trace()
#endif

// Most of the work happens in tsi by the pthread destructor (n00b_tsi_cleanup)
void
n00b_thread_exit(void *result)
{
    if (!exiting) {
        n00b_ioqueue_dont_block_callbacks();
    }

    // This has a potential race condition, but given we always should
    // have an IO thread running until actual shutdown, I think it's
    // okay.
    if (n00b_thread_run_count() == 1) {
        exit(saved_exit_code);
    }

    pthread_exit(result);
}

void
n00b_exit(int code)
{
    saved_exit_code = code;
    exiting         = true;
    n00b_io_begin_shutdown();
    n00b_thread_exit(NULL);
}

void
n00b_abort(void)
{
    saved_exit_code = 139;
    exiting         = true;
    n00b_gts_notify_abort();
    n00b_io_begin_shutdown();
    n00b_wait_for_io_shutdown();
    abort();
}

bool
n00b_current_process_is_exiting(void)
{
    return exiting;
}
