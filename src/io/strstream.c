#define N00B_USE_INTERNAL_API
#include "n00b.h"

n00b_stream_t *
n00b_stream_string(n00b_string_t *s)
{
    n00b_string_cookie_t *c      = n00b_gc_alloc_mapped(n00b_string_cookie_t,
                                                N00B_GC_SCAN_NONE);
    n00b_stream_t     *result = n00b_alloc_party(&n00b_strstream_impl,
                                             c,
                                             n00b_io_perm_r,
                                             n00b_io_ev_string);

    c->s = s;

    return result;
}

static bool
n00b_strstream_read_fn(n00b_stream_t *stream, uint64_t ureq)
{
    int64_t req = (int64_t)ureq;

    if (!req) {
        req = -1;
    }

    n00b_string_cookie_t *c = stream->cookie;

    n00b_string_t *msg   = n00b_slice_get(c->s, c->position, c->position + req);
    int            nread = n00b_string_codepoint_len(msg);

    if (!nread) {
        return false;
    }

    c->position += nread;

    n00b_list_t *l = n00b_handle_read_operation(stream, msg);

    if (!l) {
        return false;
    }

    int n = n00b_list_len(l);

    for (int i = 0; i < n; i++) {
        void *item = n00b_list_get(l, i, NULL);
        n00b_post_to_subscribers(stream, item, n00b_io_sk_read);
    }

    return true;
}

bool
n00b_strstream_subscribe(n00b_stream_sub_t        *sub,
                         n00b_io_subscription_kind perms)
{
    n00b_stream_t *stream = sub->source;

    if (n00b_len(stream->read_subs) != 1) {
        return true;
    }

    n00b_string_cookie_t *cookie = stream->cookie;

    if (!cookie->position) {
        n00b_read(stream, 0, NULL);
    }

    return true;
}

static n00b_string_t *
n00b_strstream_repr(n00b_stream_t *stream)
{
    return n00b_cformat("str stream (@«#»)", (int64_t)stream);
}

n00b_io_impl_info_t n00b_strstream_impl = {
    .open_impl      = (void *)n00b_stream_string,
    .read_impl      = n00b_strstream_read_fn,
    .subscribe_impl = n00b_strstream_subscribe,
    .repr_impl      = n00b_strstream_repr,
    .byte_oriented  = true,
};
