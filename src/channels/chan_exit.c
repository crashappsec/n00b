#define N00B_USE_INTERNAL_API
#include "n00b.h"

static void *
launch_wait(n00b_exit_info_t *c)
{
    bool err;

    if (!c->cref->read_cache) {
        c->cref->read_cache = n00b_list(n00b_type_ref());
    }

    n00b_thread_async_cancelable();
    n00b_gts_suspend();
    int r = wait4(c->pid, &c->stats, 0, &c->usage);
    n00b_gts_resume();
    if (r == -1) {
        c->err = true;
    }

    c->exited = true;

    n00b_list_append(c->cref->read_cache, c);

    // Don't care about the result of err, just need to pass it.
    n00b_io_dispatcher_process_read_queue(c->cref, &err);

    return NULL;
}

static int
exitchan_init(n00b_channel_t *stream, n00b_list_t *args)
{
    n00b_exit_info_t *c = (n00b_exit_info_t *)n00b_get_channel_cookie(stream);
    c->pid              = (int64_t)n00b_private_list_pop(args);
    c->waiter           = n00b_thread_spawn((void *)launch_wait, c);
    stream->name        = n00b_cformat("pid(exit): [|#|]", c->pid);

    return O_RDONLY;
}

static void *
exitchan_read(n00b_channel_t *stream, bool *err)
{
    n00b_exit_info_t *c = (n00b_exit_info_t *)n00b_get_channel_cookie(stream);

    if (!c->exited) {
        *err = true;
        return NULL;
    }

    *err = false;
    return c;
}

static bool
exitchan_close(n00b_channel_t *stream)
{
    n00b_exit_info_t *c = (n00b_exit_info_t *)n00b_get_channel_cookie(stream);

    if (!c->exited) {
        kill(c->pid, SIGKILL);
    }

    return true;
}

// If we get a subscriber, make sure they get the message.  TODO: add
// an option to get EVERY subscriber, not just the first.
void
exitchan_on_first_subscriber(n00b_channel_t *c, n00b_exit_info_t *info)
{
    bool err;

    if (info->exited) {
        n00b_list_append(c->read_cache, info);
        n00b_io_dispatcher_process_read_queue(c, &err);
    }
}

static n00b_chan_impl exitchan_impl = {
    .cookie_size             = sizeof(n00b_exit_info_t),
    .init_impl               = (void *)exitchan_init,
    .read_impl               = (void *)exitchan_read,
    .close_impl              = (void *)exitchan_close,
    .read_subscribe_cb       = (void *)exitchan_on_first_subscriber,
    .poll_for_blocking_reads = true,
};

n00b_channel_t *
n00b_new_exit_channel(int64_t pid)
{
    n00b_list_t *args = n00b_list(n00b_type_ref());
    n00b_list_append(args, (void *)(int64_t)pid);

    return n00b_new(n00b_type_channel(), &exitchan_impl, args);
}
