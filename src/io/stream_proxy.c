#define N00B_USE_INTERNAL_API
#include "n00b.h"

// Since, when we subscribe to something, our 'write' path gets triggered,
// the proxy stream doesn't actually subscribe itself, it subscribes
// callback objects.

static void *
on_target_read(void *msg, n00b_stream_t *proxy)
{
    bool err;

    n00b_list_append(proxy->read_cache, msg);
    return n00b_io_dispatcher_process_read_queue(proxy, &err);
}

static void *
on_target_close(n00b_stream_t *target, n00b_stream_t *proxy)
{
    n00b_proxy_info_t *info = n00b_get_stream_cookie(proxy);

    if (info->read_cb) {
        n00b_close(info->read_cb);
        info->read_cb = NULL;
    }
    n00b_close(info->close_cb);
    info->close_cb = NULL;

    n00b_close(proxy);

    return NULL;
}

static int
proxy_init(n00b_stream_t *stream, n00b_stream_t *target)
{
    n00b_proxy_info_t *info = n00b_get_stream_cookie(stream);
    // Since we don't have a pointer to our parent object here, given
    // we will need to reference it to set up the callbacks (since the
    // callback needs our top-level object to enqueue stuff or to
    // close itself), we set up subscriptions in the instantiation
    // function, instead of scanning back for our memory allocation.
    // Probably these init functions should be refactored to give you
    // the top-level object instead.

    stream->name = n00b_cformat("proxy:[|#|]", target->name);

    if (target->r) {
        info->read_cb = n00b_new_callback_stream(on_target_read, stream);
        n00b_stream_subscribe_read(target, info->read_cb, false);
    }

    info->close_cb = n00b_new_callback_stream(on_target_close, stream);

    n00b_stream_subscribe_close(target, info->close_cb);

    info->target = target;

    return O_RDWR;
}

static void *
proxy_read(n00b_stream_t *stream, bool *err)
{
    n00b_proxy_info_t *info = n00b_get_stream_cookie(stream);
    return n00b_io_dispatcher_process_read_queue(info->target, err);
}

static void
proxy_write(n00b_stream_t *stream, void *msg, bool block)
{
    n00b_proxy_info_t *info = n00b_get_stream_cookie(stream);
    if (block) {
        n00b_write(info->target, msg);
    }
    else {
        n00b_write(info->target, msg);
    }
}

static n00b_stream_impl proxy_impl = {
    .cookie_size = sizeof(n00b_proxy_info_t),
    .init_impl   = (void *)proxy_init,
    .read_impl   = (void *)proxy_read,
    .write_impl  = (void *)proxy_write,
};

n00b_stream_t *
_n00b_new_stream_proxy(n00b_stream_t *target, ...)
{
    n00b_list_t *filters;
    n00b_build_filter_list(filters, target);

    return n00b_new(n00b_type_stream(), &proxy_impl, target, filters);
}
