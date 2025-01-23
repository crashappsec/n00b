#define N00B_USE_INTERNAL_API
#include "n00b.h"

n00b_stream_t *
n00b_buffer_stream_open(n00b_buf_t *b, n00b_io_permission_t mode)
{
    n00b_bio_cookie_t *cookie = n00b_gc_alloc_mapped(n00b_bio_cookie_t,
                                                     N00B_GC_SCAN_NONE);
    n00b_stream_t     *result = n00b_alloc_party(&n00b_bufferio_impl,
                                             cookie,
                                             mode,
                                             n00b_io_ev_buffer);
    cookie->b                 = b;
    cookie->read_position     = 0;
    cookie->write_position    = N00B_SEEK_END;

    return result;
}

static void
internal_read_buf(n00b_stream_t *party, n00b_buf_t *b)
{
    if (!n00b_buffer_len(b)) {
        return;
    }

    n00b_list_t *l = n00b_handle_read_operation(party, b);

    if (!l) {
        return;
    }

    int n = n00b_list_len(l);
    for (int i = 0; i < n; i++) {
        void *item = n00b_list_get(l, i, NULL);
        n00b_post_to_subscribers(party, item, n00b_io_sk_read);
    }
}

static bool
n00b_buf_read_fn(n00b_stream_t *e, uint64_t nbytes)
{
    n00b_bio_cookie_t *cookie = e->cookie;
    n00b_buffer_acquire_r(cookie->b);

    int l = n00b_buffer_len(cookie->b);

    if (cookie->read_position < l && cookie->read_position >= 0) {
        n00b_buf_t *slice     = n00b_slice_get(cookie->b,
                                           cookie->read_position,
                                           l);
        cookie->read_position = l;
        internal_read_buf(e, slice);
    }

    n00b_buffer_release(cookie->b);

    return true;
}

static void
n00b_buf_write_fn(n00b_stream_t *e, n00b_buf_t *b)
{
    n00b_bio_cookie_t *cookie = e->cookie;
    n00b_buffer_acquire_w(cookie->b);

    n00b_buf_t *tmp;

    n00b_buffer_acquire_r(b);

    int64_t len_added = n00b_buffer_len(b);

    if (len_added <= 0) {
        goto done;
    }

    tmp                  = n00b_buffer_add(cookie->b, b);
    cookie->b->data      = tmp->data;
    cookie->b->byte_len  = tmp->byte_len;
    cookie->b->alloc_len = tmp->alloc_len;

done:
    n00b_buffer_release(cookie->b);
    n00b_buffer_release(b);
}

bool
n00b_buf_subscribe(n00b_stream_sub_t *sub, n00b_io_subscription_kind perms)
{
    n00b_stream_t *stream = sub->source;

    if (perms != n00b_io_perm_r && n00b_len(stream->read_subs) == 1) {
        n00b_bio_cookie_t *cookie = stream->cookie;
        if (!cookie->read_position) {
            n00b_buf_read_fn(stream, n00b_buffer_len(cookie->b));
        }
    }

    return true;
}

static bool
n00b_buf_at_eof(n00b_stream_t *e)
{
    bool result;

    n00b_bio_cookie_t *cookie = e->cookie;
    n00b_buffer_acquire_r(cookie->b);

    result = cookie->read_position >= n00b_len(cookie->b);

    n00b_buffer_release(cookie->b);
    return result;
}

static n00b_utf8_t *
n00b_io_buf_repr(n00b_stream_t *stream)
{
    return n00b_cstr_format("io buffer (@{})", n00b_box_u64((int64_t)stream));
}

n00b_io_impl_info_t n00b_bufferio_impl = {
    .open_impl      = (void *)n00b_buffer_stream_open,
    .read_impl      = n00b_buf_read_fn,
    .subscribe_impl = n00b_buf_subscribe,
    .write_impl     = (void *)n00b_buf_write_fn,
    .repr_impl      = n00b_io_buf_repr,
    .eof_impl       = n00b_buf_at_eof,
    .byte_oriented  = true,
};
