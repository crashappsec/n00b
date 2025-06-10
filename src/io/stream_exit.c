#define N00B_USE_INTERNAL_API
#include "n00b.h"

static void *
launch_wait(n00b_stream_t *stream)
{
    bool err;

    n00b_exit_info_t *c = n00b_get_stream_cookie(stream);

    N00B_DBG_CALL(n00b_thread_suspend);
    int r = wait4(c->pid, &c->stats, 0, &c->usage);
    N00B_DBG_CALL(n00b_thread_resume);
    if (r == -1) {
        c->err = true;
    }

    c->exited = true;

    n00b_cache_read(stream, c);

    // Don't care about the result of err, just need to pass it.
    n00b_io_dispatcher_process_read_queue(stream, &err);

    return NULL;
}

static int
exit_stream_init(n00b_stream_t *stream, n00b_list_t *args)
{
    n00b_exit_info_t *c = (n00b_exit_info_t *)n00b_get_stream_cookie(stream);
    c->pid              = (int64_t)n00b_private_list_pop(args);
    c->waiter           = n00b_thread_spawn((void *)launch_wait, stream);
    stream->name        = n00b_cformat("pid(exit): [=#=]", c->pid);

    return O_RDONLY;
}

static void *
exit_stream_read(n00b_stream_t *stream, bool *err)
{
    n00b_exit_info_t *c = (n00b_exit_info_t *)n00b_get_stream_cookie(stream);

    if (!c->exited) {
        *err = true;
        return NULL;
    }

    *err = false;
    return c;
}

static bool
exit_stream_close(n00b_stream_t *stream)
{
    n00b_exit_info_t *c = (n00b_exit_info_t *)n00b_get_stream_cookie(stream);

    if (!c->exited) {
        kill(c->pid, SIGKILL);
    }

    return true;
}

// If we get a subscriber, make sure they get the message.  TODO: add
// an option to get EVERY subscriber, not just the first.
void
exit_stream_on_first_subscriber(n00b_stream_t *s, n00b_exit_info_t *info)
{
    bool err;

    if (info->exited) {
        n00b_cache_read(s, info);
        n00b_io_dispatcher_process_read_queue(s, &err);
    }
}

static n00b_stream_impl exit_stream_impl = {
    .cookie_size             = sizeof(n00b_exit_info_t),
    .init_impl               = (void *)exit_stream_init,
    .read_impl               = (void *)exit_stream_read,
    .close_impl              = (void *)exit_stream_close,
    .read_subscribe_cb       = (void *)exit_stream_on_first_subscriber,
    .poll_for_blocking_reads = true,
};

n00b_stream_t *
n00b_new_exit_stream(int64_t pid)
{
    n00b_list_t *args = n00b_list(n00b_type_ref());
    n00b_list_append(args, (void *)(int64_t)pid);

    return n00b_new(n00b_type_stream(), &exit_stream_impl, args);
}
