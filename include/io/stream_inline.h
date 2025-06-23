#pragma once
#include "n00b.h"

static inline void
n00b_stream_set_name(n00b_stream_t *stream, n00b_string_t *s)
{
    stream->name = s;
}

static inline n00b_string_t *
n00b_stream_get_name(n00b_stream_t *stream)
{
    return stream->name;
}

static inline bool
n00b_stream_can_read(n00b_stream_t *stream)
{
    return stream->r;
}

static inline bool
n00b_stream_can_write(n00b_stream_t *stream)
{
    return stream->w;
}

static inline void
_n00b_write_memory(n00b_stream_t *s, void *mem, int n, char *f, int l)
{
    _n00b_write(s, n00b_buffer_from_bytes(mem, n), f, l);
}

static inline n00b_buf_t *
n00b_stream_extract_buffer(n00b_stream_t *s)
{
    n00b_buffer_cookie_t *c = (void *)s->cookie;

    if (!n00b_type_is_buffer(n00b_get_my_type(c->buffer))) {
        return NULL;
    }

    return c->buffer;
}

static inline n00b_stream_t *
n00b_name_cb_hack(n00b_stream_t *stream, char *s)
{
    stream->name = n00b_cformat("Callback: «#»", n00b_cstring(s));
    return stream;
}

static inline int
n00b_stream_fileno(n00b_stream_t *s)
{
    if (!s->fd_backed) {
        return -1;
    }

    n00b_fd_cookie_t *cookie = (void *)s->cookie;
    return cookie->stream->fd;
}

static inline n00b_net_addr_t *
n00b_stream_net_address(n00b_stream_t *s)
{
    if (!s->fd_backed) {
        return NULL;
    }

    n00b_fd_cookie_t *cookie = (void *)s->cookie;
    return cookie->addr;
}

#ifdef N00B_USE_INTERNAL_API
// streams will proxy the topic name that are read / written to its
// observers the topic information on a write always gets passed to
// the implementation (but is usually just '__write'; the topic()
// interface allows for custom topics). __ topics are reserved (but
// generally passed to the implementation). Reserved topics are
// currently: __read, __write, __log, __exception, __close __read and
// __write are the only two that can be accepted by filters. Other
// topics get passed directly to the underlying stream's
// implementation function.

// Implementations of core streams are expect to call this when done
// writing.
static inline bool
n00b_stream_notify(n00b_stream_t *stream, void *msg, int64_t n)
{
    return n00b_observable_post(&stream->pub_info, (void *)n, msg) != 0;
}

static inline bool
n00b_cnotify_r(n00b_stream_t *stream, void *msg)
{
    return n00b_stream_notify(stream, msg, N00B_CT_R);
}

static inline bool
n00b_cnotify_q(n00b_stream_t *stream, void *msg)
{
    return n00b_stream_notify(stream, msg, N00B_CT_Q);
}

static inline bool
n00b_cnotify_w(n00b_stream_t *stream, void *msg)
{
    return n00b_stream_notify(stream, msg, N00B_CT_W);
}

static inline bool
n00b_cnotify_raw(n00b_stream_t *stream, void *msg)
{
    return n00b_stream_notify(stream, msg, N00B_CT_RAW);
}

static inline bool
n00b_cnotify_close(n00b_stream_t *stream, void *msg)
{
    return n00b_stream_notify(stream, msg, N00B_CT_CLOSE);
}

static inline bool
n00b_cnotify_error(n00b_stream_t *stream, void *msg)
{
    return n00b_stream_notify(stream, msg, N00B_CT_ERROR);
}

static inline void *
n00b_get_stream_cookie(n00b_stream_t *stream)
{
    return (void *)stream->cookie;
}

static inline n00b_string_t *
n00b_get_fd_repr(n00b_stream_t *stream)
{
    if (!stream->fd_backed) {
        return NULL;
    }
    n00b_fd_cookie_t *c = n00b_get_stream_cookie(stream);
    return c->stream->name;
}

#endif
