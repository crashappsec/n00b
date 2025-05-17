#define N00B_USE_INTERNAL_API
#include "n00b.h"

static int
cbchan_init(n00b_channel_t *stream, n00b_list_t *args)
{
    n00b_callback_channel_t *c = n00b_get_channel_cookie(stream);

    c->params    = n00b_private_list_pop(args);
    c->cb        = n00b_private_list_pop(args);
    stream->name = n00b_cformat("Callback @[|#:p|]", c->cb);

    return O_RDWR;
}

// Since callbacks do not push messages, this gets called when there's
// no waiting message, yet the previous message didn't make it through
// all our filters.
//
// We don't want to yield any more messages.
static void *
cbchan_read(n00b_channel_t *stream, bool *err)
{
    *err = true;
    return NULL;
}

typedef struct {
    n00b_channel_t *s;
    void           *msg;
} cbchan_info_t;

static void cbchan_write(n00b_channel_t *,
                         void *,
                         bool);
static void *
async_callback_runner(cbchan_info_t *info)
{
    cbchan_write(info->s, info->msg, true);
    return NULL;
}

static void
cbchan_write(n00b_channel_t *stream, void *msg, bool block)
{
    n00b_callback_channel_t *c = n00b_get_channel_cookie(stream);

    if (!block) {
        // We spawn a thread to call ourselves, because this is called
        // (indirectly) from the main IO polling loop, and we do not
        // want user code potentially blocking.
        cbchan_info_t *info = n00b_gc_alloc_mapped(cbchan_info_t,
                                                   N00B_GC_SCAN_ALL);
        info->s             = stream;
        info->msg           = msg;

        n00b_thread_spawn((void *)async_callback_runner, info);
        return;
    }

    void *cb_res = (*c->cb)(msg, c->params);

    // Generally, this is how the pub/sub bus gets messages to publish
    // to readers.  Without this, we will hang in certain situations where
    // we've subscribed an fd to read a callback.
    bool err;

    n00b_list_append(stream->read_cache, cb_res);
    n00b_io_dispatcher_process_read_queue(stream, &err);
}

static n00b_chan_impl n00b_cbchan_impl = {
    .cookie_size = sizeof(n00b_callback_channel_t),
    .init_impl   = (void *)cbchan_init,
    .read_impl   = (void *)cbchan_read,
    .write_impl  = (void *)cbchan_write,
};

n00b_channel_t *
_n00b_new_callback_channel(n00b_channel_cb_t *cb, void *params, ...)
{
    n00b_list_t *args = n00b_list(n00b_type_ref());
    n00b_list_t *filters;

    n00b_list_append(args, cb);
    n00b_list_append(args, params);

    n00b_build_filter_list(filters, cb);

    return n00b_new(n00b_type_channel(), &n00b_cbchan_impl, args, filters);
}
