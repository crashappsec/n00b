#define N00B_USE_INTERNAL_API
#include "n00b.h"

static int
bufchan_init(n00b_buffer_channel_t *c, n00b_list_t *args)
{
    int ret = O_RDWR;
    int n   = n00b_list_len(args);

    if (n == 2) {
        ret = (int64_t)n00b_list_get(args, 1, NULL);
    }

    if (n > 2 || n == 0 || ret < 0 || ret > O_RDWR) {
        N00B_CRAISE("Invalid parameters for buffer");
    }

    return ret;
}

static n00b_buf_t *
bufchan_read(n00b_buffer_channel_t *c,
             bool                  *success)
{
    n00b_buf_t *src = c->buffer;
    n00b_buf_t *result;

    while (true) {
        _n00b_buffer_acquire_r(src);

        if (src->byte_len < c->position) {
            result      = n00b_buffer_from_bytes(src->data + c->position,
                                            src->byte_len - c->position);
            c->position = src->byte_len;
            n00b_buffer_release(src);
            *success = true;
            return result;
        }

        n00b_buffer_release(src);

        *success = false;
        return NULL;
    }
}

static void
bufchan_write(n00b_buffer_channel_t *c, void *msg, bool blocking)
{
    defer_on();
    n00b_buffer_acquire_w(c->buffer);
    n00b_buf_t *b = msg;
    n00b_buffer_acquire_r(b);

    if (c->position > c->buffer->byte_len) {
        c->position = c->buffer->byte_len;
    }

    int   free_space = c->buffer->alloc_len - c->position;
    int   needed     = b->byte_len - free_space;
    char *p          = c->buffer->data + c->position;

    if (needed <= 0) {
        memcpy(p, b->data, b->byte_len);
        c->position += b->byte_len;
        p += b->byte_len;
        c->buffer->byte_len = p - c->buffer->data;
        Return;
    }

    char *new_buf = n00b_gc_raw_alloc(c->buffer->alloc_len + needed,
                                      N00B_GC_SCAN_NONE);
    memcpy(new_buf, c->buffer->data, c->position);
    memcpy(p, b->data, b->byte_len);
    c->buffer->data = new_buf;
    c->position += b->byte_len;
    c->buffer->byte_len = c->position;
    Return;
    defer_func_end();
}

static bool
bufchan_set_pos(n00b_buffer_channel_t *c, int pos, bool relative)
{
    defer_on();
    n00b_buffer_acquire_r(c->buffer);

    int w;

    if (relative) {
        w = c->position + pos;
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

    c->position = w;
    Return true;
    defer_func_end();
}

static int
bufchan_get_pos(n00b_buffer_channel_t *c)
{
    return c->position;
}

static n00b_chan_impl n00b_bufchan_impl = {
    .cookie_size             = sizeof(n00b_buffer_channel_t),
    .init_impl               = (void *)bufchan_init,
    .read_impl               = (void *)bufchan_read,
    .write_impl              = (void *)bufchan_write,
    .spos_impl               = (void *)bufchan_set_pos,
    .gpos_impl               = (void *)bufchan_get_pos,
    .poll_for_blocking_reads = true,
};

n00b_channel_t *
n00b_channel_from_buffer(n00b_buf_t *b, int64_t mode, va_list alist)
{
    n00b_list_t *args    = n00b_list(n00b_type_ref());
    n00b_list_t *filters = n00b_list(n00b_type_ref());

    n00b_list_append(args, b);
    n00b_list_append(args, (void *)mode);

    int nargs = va_arg(alist, int) - 1;

    while (nargs--) {
        n00b_list_append(filters, va_arg(alist, void *));
    }

    va_end(alist);

    n00b_channel_t *result = n00b_channel_create(&n00b_bufchan_impl,
                                                 args,
                                                 filters);
    result->name           = n00b_cformat("Buffer @[|#:p|]", b);

    return result;
}

n00b_channel_t *
_n00b_in_buf_channel(n00b_buf_t *b, ...)
{
    va_list alist;
    va_start(alist, b);

    return n00b_channel_from_buffer(b, O_RDONLY, alist);
}

n00b_channel_t *
_n00b_out_buf_channel(n00b_buf_t *b, ...)
{
    va_list alist;
    va_start(alist, b);

    return n00b_channel_from_buffer(b, O_WRONLY, alist);
}

n00b_channel_t *
_n00b_io_buf_channel(n00b_buf_t *b, ...)
{
    va_list alist;
    va_start(alist, b);

    return n00b_channel_from_buffer(b, O_RDWR, alist);
}
