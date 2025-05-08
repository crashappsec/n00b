#define N00B_USE_INTERNAL_API
#include "n00b.h"

// Since, when we subscribe to something, our 'write' path gets triggered,
// the proxy channel doesn't actually subscribe itself, it subscribes
// callback objects.

static void *
on_target_read(void *msg, n00b_channel_t *proxy)
{
    bool err;

    n00b_list_append(proxy->read_cache, msg);
    return n00b_io_dispatcher_process_read_queue(proxy, &err);
}

static void *
on_target_close(n00b_channel_t *target, n00b_channel_t *proxy)
{
    n00b_proxy_info_t *info = n00b_get_channel_cookie(proxy);

    if (info->read_cb) {
        n00b_channel_close(info->read_cb);
        info->read_cb = NULL;
    }
    n00b_channel_close(info->close_cb);
    info->close_cb = NULL;

    n00b_channel_close(proxy);

    return NULL;
}

static int
proxy_init(n00b_channel_t *stream, n00b_channel_t *target)
{
    n00b_proxy_info_t *info = n00b_get_channel_cookie(stream);
    // Since we don't have a pointer to our parent object here, given
    // we will need to reference it to set up the callbacks (since the
    // callback needs our top-level object to enqueue stuff or to
    // close itself), we set up subscriptions in the instantiation
    // function, instead of scanning back for our memory allocation.
    // Probably these init functions should be refactored to give you
    // the top-level object instead.

    stream->name = n00b_cformat("proxy:[|#|]", target->name);

    if (target->r) {
        info->read_cb = n00b_new_callback_channel(on_target_read, stream);
        n00b_channel_subscribe_read(target, info->read_cb, false);
    }

    info->close_cb = n00b_new_callback_channel(on_target_close, stream);

    n00b_channel_subscribe_close(target, info->close_cb);

    info->target = target;

    int x = ((int)target->w) << 1;
    x |= ((int)target->r);

    switch (x) {
    case 0:
        N00B_CRAISE("Target is closed");
    case 1:
        return O_RDONLY;
    case 2:
        return O_WRONLY;
    case 3:
        return O_RDWR;
    default:
        n00b_unreachable();
    }
}

static void *
proxy_read(n00b_channel_t *stream, bool *err)
{
    n00b_proxy_info_t *info = n00b_get_channel_cookie(stream);
    return n00b_io_dispatcher_process_read_queue(info->target, err);
}

static void
proxy_write(n00b_channel_t *stream, void *msg, bool block)
{
    n00b_proxy_info_t *info = n00b_get_channel_cookie(stream);
    if (block) {
        n00b_channel_write(info->target, msg);
    }
    else {
        n00b_channel_write(info->target, msg);
    }
}

static n00b_chan_impl proxy_impl = {
    .cookie_size = sizeof(n00b_proxy_info_t),
    .init_impl   = (void *)proxy_init,
    .read_impl   = (void *)proxy_read,
    .write_impl  = (void *)proxy_write,
};

n00b_channel_t *
_n00b_new_channel_proxy(n00b_channel_t *target, ...)
{
    n00b_list_t *filters;
    n00b_build_filter_list(filters, target);

    return n00b_new(n00b_type_channel(), &proxy_impl, target, filters);
}
