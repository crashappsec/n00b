#define N00B_USE_INTERNAL_API
#include "n00b.h"

n00b_stream_t *
n00b_callback_open(n00b_io_callback_fn cb, void *aux)
{
    n00b_callback_cookie_t *c = n00b_gc_alloc_mapped(n00b_callback_cookie_t,
                                                     N00B_GC_SCAN_ALL);
    c->fn                     = cb;
    c->aux                    = aux;

    return n00b_alloc_party(&n00b_callback_impl,
                            c,
                            n00b_io_perm_w,
                            n00b_io_ev_callback);
}

static void *
n00b_callback_write(n00b_stream_t *stream, void *msg)
{
    n00b_iocb_info_t *callback_info = n00b_gc_alloc_mapped(n00b_iocb_info_t,
                                                           N00B_GC_SCAN_ALL);
    callback_info->stream           = stream;
    callback_info->msg              = msg;
    callback_info->cookie           = stream->cookie;

    n00b_ioqueue_enqueue_callback(callback_info);

    return NULL;
}

static n00b_string_t *
n00b_callback_repr(n00b_stream_t *ev)
{
    n00b_callback_cookie_t *c = (n00b_callback_cookie_t *)ev->cookie;

    return n00b_cformat("callback @{#:p}", c->fn);
}

n00b_io_impl_info_t n00b_callback_impl = {
    .open_impl  = (void *)n00b_callback_open,
    .write_impl = n00b_callback_write,
    .repr_impl  = n00b_callback_repr,
};
