#define N00B_USE_INTERNAL_API
#include "n00b.h"

static int
buffer_stream_init(n00b_stream_t *stream, n00b_list_t *args)
{
    n00b_buffer_cookie_t *c   = n00b_get_stream_cookie(stream);
    int                    ret = (int64_t)n00b_list_pop(args);

    c->buffer = n00b_list_pop(args);

    if (ret < 0 || ret > O_RDWR) {
        N00B_CRAISE("Invalid parameters for buffer");
    }

    stream->name = n00b_cformat("Buffer @[|#:p|]", c->buffer);

    if (n00b_list_pop(args)) {
        c->wposition = c->buffer->byte_len;
    }

    return ret;
}

static n00b_buf_t *
buffer_stream_read(n00b_stream_t *stream, bool *err)
{
    n00b_buffer_cookie_t *c   = n00b_get_stream_cookie(stream);
    n00b_buf_t            *src = c->buffer;
    n00b_buf_t            *result;

    while (true) {
        _n00b_buffer_acquire_r(src);

        if (src->byte_len < c->rposition) {
            result       = n00b_buffer_from_bytes(src->data + c->rposition,
                                            src->byte_len - c->rposition);
            c->rposition = src->byte_len;
            n00b_buffer_release(src);
            *err = false;
            return result;
        }

        n00b_buffer_release(src);

        *err = true;
        return NULL;
    }
}

static void
buffer_stream_write(n00b_stream_t *stream, void *msg, bool blocking)
{
    n00b_buffer_cookie_t *c      = n00b_get_stream_cookie(stream);
    n00b_buf_t            *target = c->buffer;
    n00b_buf_t            *src    = msg;

    defer_on();
    n00b_buffer_acquire_w(target);
    n00b_buffer_acquire_r(src);

    if (c->wposition > c->buffer->byte_len) {
        c->wposition = c->buffer->byte_len;
    }

    int   free_space = target->alloc_len - c->wposition;
    int   needed     = src->byte_len - free_space;
    char *p          = target->data + c->wposition;

    if (needed <= 0) {
start_copying:
        if (src->byte_len) {
            memcpy(p, src->data, src->byte_len);
        }
        c->wposition += src->byte_len;
        p += src->byte_len;
        target->byte_len = p - target->data;

        Return;
    }

    target->alloc_len += needed;
    target->alloc_len = hatrack_round_up_to_power_of_2(target->alloc_len);

    char *new_buf = n00b_gc_raw_alloc(target->alloc_len,
                                      N00B_GC_SCAN_NONE);

    if (c->wposition) {
        memcpy(new_buf, target->data, c->wposition);
    }
    target->data = new_buf;
    p            = new_buf + c->wposition;

    goto start_copying;

    Return;
    defer_func_end();
}

static bool
buffer_stream_set_position(n00b_stream_t *stream, int pos, bool relative)
{
    // For a rw buffer, sets the READ position only; will provide
    // another API if needed for the write position.

    n00b_buffer_cookie_t *c = n00b_get_stream_cookie(stream);

    int *ptr;
    if (!stream->r) {
        ptr = &c->wposition;
    }
    else {
        ptr = &c->rposition;
    }

    defer_on();
    n00b_buffer_acquire_r(c->buffer);

    int w;

    if (relative) {
        w = *ptr + pos;
    }
    else {
        if (pos < 0) {
            pos++;
            w = c->buffer->byte_len;
        }
        else {
            w = 0;
        }
    }

    w += pos;

    if (w < 0 || w > c->buffer->byte_len) {
        Return false;
    }

    *ptr = w;
    Return true;
    defer_func_end();
}

static int
buffer_stream_get_position(n00b_stream_t *stream)
{
    n00b_buffer_cookie_t *c = n00b_get_stream_cookie(stream);

    if (!stream->r) {
        return c->wposition;
    }
    else {
        return c->rposition;
    }
}

static bool
buffer_stream_eof(n00b_stream_t *stream)
{
    n00b_buffer_cookie_t *c = n00b_get_stream_cookie(stream);

    if (!stream->r) {
        return c->wposition == c->buffer->byte_len;
    }
    else {
        return c->rposition == c->buffer->byte_len;
    }
}

static n00b_stream_impl n00b_bufchan_impl = {
    .cookie_size             = sizeof(n00b_buffer_cookie_t),
    .init_impl               = (void *)buffer_stream_init,
    .read_impl               = (void *)buffer_stream_read,
    .write_impl              = (void *)buffer_stream_write,
    .spos_impl               = (void *)buffer_stream_set_position,
    .gpos_impl               = (void *)buffer_stream_get_position,
    .eof_impl                = (void *)buffer_stream_eof,
    .poll_for_blocking_reads = true,
};

n00b_stream_t *
n00b_stream_from_buffer(n00b_buf_t  *b,
                         int64_t      mode,
                         n00b_list_t *filters,
                         bool         end)
{
    n00b_list_t *args = n00b_list(n00b_type_ref());

    n00b_list_append(args, (void *)(int64_t)end);
    n00b_list_append(args, b);
    n00b_list_append(args, (void *)mode);

    return n00b_new(n00b_type_stream(), &n00b_bufchan_impl, args, filters);
}

n00b_stream_t *
_n00b_in_buf_stream(n00b_buf_t *b, ...)
{
    n00b_list_t *filters;
    n00b_build_filter_list(filters, b);

    return n00b_stream_from_buffer(b, O_RDONLY, filters, false);
}

n00b_stream_t *
_n00b_out_buf_stream(n00b_buf_t *b, bool end, ...)
{
    n00b_list_t *filters;
    n00b_build_filter_list(filters, b);

    return n00b_stream_from_buffer(b, O_WRONLY, filters, end);
}

n00b_stream_t *
_n00b_io_buf_stream(n00b_buf_t *b, bool end, ...)
{
    n00b_list_t *filters;
    n00b_build_filter_list(filters, b);

    return n00b_stream_from_buffer(b, O_RDWR, filters, end);
}
