#define N00B_USE_INTERNAL_API
#include "n00b.h"

static bool   exiting                = false;
__thread bool n00b_thread_has_exited = false;
static int    saved_exit_code        = 0;

#ifdef N00B_DEBUG_EXIT_TRACE
#define show_trace() n00b_static_c_backtrace()
#else
#define show_trace()
#endif

void
n00b_exit(int code)
{
    saved_exit_code = code;
    exiting         = true;
    n00b_io_begin_shutdown();
    n00b_thread_exit(NULL);
}

extern __thread n00b_heap_t *n00b_string_heap;
void
n00b_thread_exit(void *result)
{
    N00B_DEBUG_HELD_LOCKS();
    // Launch a new callback thread if someone exited it.
    n00b_ioqueue_dont_block_callbacks();
    n00b_thread_unregister(NULL);
    n00b_thread_has_exited = true;
    show_trace();

    if (n00b_string_heap) {
        n00b_delete_heap(n00b_string_heap);
    }

    // This has a potential race condition, but given we always should
    // have an IO thread running until actual shutdown, I think it's
    // okay.
    if (!n00b_thread_run_count()) {
        exit(saved_exit_code);
    }

    pthread_exit(result);
}

void
n00b_abort(void)
{
    n00b_ioqueue_dont_block_callbacks();
    n00b_thread_unregister(NULL);
    n00b_thread_has_exited = true;
    show_trace();
    exiting = true;
    n00b_io_begin_shutdown();
    n00b_wait_for_io_shutdown();
    n00b_thread_unregister(NULL);

    abort();
}

bool
n00b_current_process_is_exiting(void)
{
    return exiting;
}
