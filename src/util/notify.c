#include "n00b.h"

n00b_notifier_t *
n00b_new_notifier(void)
{
    n00b_notifier_t *result = n00b_gc_alloc_mapped(n00b_notifier_t,
                                                   N00B_GC_SCAN_NONE);

    pipe(result->pipe);
    int flags = fcntl(result->pipe[1], F_GETFL);

#if !defined(__linux__)
    fcntl(result->pipe[1], F_SETNOSIGPIPE, 1);
#endif
    fcntl(result->pipe[1], F_SETFL, flags | O_NONBLOCK);

    return result;
}

int64_t
n00b_wait(n00b_notifier_t *n, int ms_timeout)
{
    uint64_t result;
    ssize_t  err;

    if (ms_timeout == 0) {
        ms_timeout = -1;
    }

    struct pollfd ctx = {
        .fd     = n->pipe[0],
        .events = POLLIN | POLLHUP | POLLERR,
    };

    n00b_gts_suspend();

    if (poll(&ctx, 1, ms_timeout) != 1) {
        n00b_gts_resume();
        return -1LL;
    }

    n00b_gts_resume();

    if (ctx.revents & POLLHUP) {
        return 0LL;
    }

    do {
        err = read(n->pipe[0], &result, sizeof(uint64_t));
    } while ((err < 1) && (errno == EAGAIN || errno == EINTR));

    if (err < 1) {
        n00b_raise_errno();
    }

    return result;
}
