#pragma once
#include "n00b.h"

typedef struct n00b_stream_t n00b_stream_t;

typedef struct {
    void   *payload;
    char   *file;
    int64_t id;
    int32_t nitems;
    int32_t line;
} n00b_stream_msg_t;

typedef int (*n00b_stream_init_fn)(n00b_stream_t *, void *);
typedef void *(*n00b_stream_r_fn)(n00b_stream_t *, bool *);
typedef void (*n00b_stream_w_fn)(n00b_stream_t *, n00b_stream_msg_t *, bool);
typedef bool (*n00b_stream_spos_fn)(n00b_stream_t *, int, bool);
typedef int (*n00b_stream_gpos_fn)(n00b_stream_t *);
typedef void (*n00b_stream_close_fn)(n00b_stream_t *);
typedef n00b_string_t *(*n00b_stream_repr_fn)(n00b_stream_t *);
typedef void (*n00b_stream_sub_fn)(n00b_stream_t *, void *);
typedef bool (*n00b_stream_eof_fn)(n00b_stream_t *);

typedef struct {
    int                  cookie_size;
    n00b_stream_init_fn  init_impl;
    n00b_stream_r_fn     read_impl;
    n00b_stream_w_fn     write_impl;
    n00b_stream_spos_fn  spos_impl; // Set position
    n00b_stream_gpos_fn  gpos_impl; // Get position
    n00b_stream_close_fn close_impl;
    n00b_stream_repr_fn  repr_impl;
    n00b_stream_eof_fn   eof_impl;
    n00b_stream_sub_fn   read_subscribe_cb;
    n00b_stream_sub_fn   read_unsubscribe_cb;
    bool                 poll_for_blocking_reads;
    // This should only be set for data objects that are non-mutable,
    // such as strings.
    bool                 disallow_blocking_reads;
} n00b_stream_impl;

#define N00B_LOCKRD_IX        0
#define N00B_LOCKWR_IX        1
#define N00B_LOCKSUB_IX       2
#define N00B_STREAM_LOCKSLOTS 3

struct n00b_stream_t {
    // This is in both raw streams and filters in case we want to do
    // notifications per filter level.
    n00b_observable_t pub_info;
    n00b_list_t      *my_subscriptions;
    n00b_string_t    *name;
    n00b_list_t      *read_cache;
    n00b_stream_impl *impl;
    n00b_filter_t    *write_top;
    n00b_filter_t    *read_top;
    int               stream_id; // unique number for debugging.

    // Note that individual r/w requests may be 'blocking'; for a
    // read, the block lasts until the end stream operates, AND the
    // expected number of objects is yielded by the top filter.

    // The stream implementation must block if there's nothing
    // available to fulfill a request. And if filters buffer bits when
    // an operation blocks, they pass down another read request, until
    // they can satisfy the input constraint. That can easily result
    // in the stream blocking.
    //
    // For blocking writes, if the filter choose to buffer or drop the
    // message, that concludes our blocking. If we make it all the way
    // down to the implementation, each filter needs to yield info on
    // when top-level inputs (by index) are fully written. We notify
    // when the final filter AND the implementation considers a
    // message written, unless the user requests us to only stall if
    // some or all of the message makes it down to the filter.
    //
    // Of course, if any filter DROPS a blocking write, the pipeline
    // should return as soon as the message is fully processed by the
    // pipeline, to the extent it's going to do so.
    //
    // For this approach to work easily, we require people to not
    // inject into the middle of a filtering pipeline. Therefore, the
    // API allows you to chain a bunch of filters to a stream, but
    // not to get a reference to the individual filters, just the top
    // one.
    //
    // These are only allowed to be set for filters, but can point to
    // an end-stream. And they only ever apply to r/w ops. Errors can
    // be subscribed to for any stream type, but do not support
    // filtering; they're considered out-of-band, and don't go through
    // the 'implementation'.

    // These bits indicate whether the user can make a
    // stream_write(), streamread() The stream might be closed, but
    // the implementation should detect and return the indication to
    // the caller.
    unsigned int w           : 1;
    unsigned int r           : 1;
    unsigned int has_rsubs   : 1;
    unsigned int has_rawsubs : 1;
    unsigned int fd_backed   : 1;

    n00b_list_t  *blocked_readers;
    n00b_mutex_t *locks[N00B_STREAM_LOCKSLOTS];
    char          cookie[];
};

enum {
    N00B_CT_R,
    N00B_CT_Q,
    N00B_CT_W,
    N00B_CT_RAW,
    N00B_CT_CLOSE,
    N00B_CT_ERROR,
    N00B_CT_NUM_TOPICS,
};

// streams will proxy the topic name that are read / written to its
// observers the topic information on a write always gets passed to
// the implementation (but is usually just '__write'; the topic()
// interface allows for custom topics). __ topics are reserved (but
// generally passed to the implementation). Reserved topics are
// currently: __read, __write, __log, __exception, __close __read and
// __write are the only two that can be accepted by filters. Other
// topics get passed directly to the underlying stream's
// implementation function.
#ifdef N00B_USE_INTERNAL_API

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

static inline bool
n00b_stream_is_tty(n00b_stream_t *stream)
{
    if (!stream->fd_backed) {
        return false;
    }
    n00b_fd_stream_t *cookie = (void *)stream->cookie;

    return cookie->tty;
}

#endif

extern n00b_observer_t *n00b_stream_subscribe_read(n00b_stream_t *,
                                                   n00b_stream_t *,
                                                   bool);
extern n00b_observer_t *n00b_stream_subscribe_raw(n00b_stream_t *,
                                                  n00b_stream_t *,
                                                  bool);
extern n00b_observer_t *n00b_stream_subscribe_queue(n00b_stream_t *,
                                                    n00b_stream_t *,
                                                    bool);
extern n00b_observer_t *n00b_stream_subscribe_write(n00b_stream_t *,
                                                    n00b_stream_t *,
                                                    bool);
extern n00b_observer_t *n00b_stream_subscribe_close(n00b_stream_t *,
                                                    n00b_stream_t *);
extern n00b_observer_t *n00b_stream_subscribe_error(n00b_stream_t *,
                                                    n00b_stream_t *,
                                                    bool);
#define n00b_stream_unsubscribe(x) n00b_observer_unsubscribe(x)

extern bool n00b_stream_eof(n00b_stream_t *);
extern void _n00b_write(n00b_stream_t *, void *, char *, int);
extern void _n00b_queue(n00b_stream_t *, void *, char *, int);

#define n00b_write(stream, msg) \
    _n00b_write(stream, msg, __FILE__, __LINE__)

#define n00b_queue(stream, msg) \
    _n00b_queue(stream, msg, __FILE__, __LINE__)

static inline void
_n00b_write_memory(n00b_stream_t *s, void *mem, int n, char *f, int l)
{
    _n00b_write(s, n00b_buffer_from_bytes(mem, n), f, l);
}

#define n00b_write_memory(s, mem, n) \
    _n00b_write_memory(s, mem, n, __FILE__, __LINE__)

extern void n00b_putc(n00b_stream_t *, n00b_codepoint_t);

// Blocking read functions
extern void *n00b_stream_read(n00b_stream_t *, int, bool *);
extern void *n00b_read_all(n00b_stream_t *, int);

#define n00b_read(c)                    n00b_stream_read(c, 0, NULL)
#define n00b_read_with_timeout(c, tout) n00b_stream_read(c, tout, NULL)

// This is really a non-blocking read that returns a message IF
// one is available.
extern void *n00b_io_dispatcher_process_read_queue(n00b_stream_t *, bool *);

extern int         n00b_stream_get_position(n00b_stream_t *);
extern bool        n00b_stream_set_relative_position(n00b_stream_t *, int);
extern bool        n00b_stream_set_absolute_position(n00b_stream_t *, int);
extern void        n00b_close(n00b_stream_t *);
extern n00b_buf_t *n00b_stream_unfiltered_read(n00b_stream_t *, int);
extern bool        n00b_stream_unfiltered_write(n00b_stream_t *, n00b_buf_t *);

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

#ifdef N00B_USE_INTERNAL_API
static inline void *
n00b_get_stream_cookie(n00b_stream_t *stream)
{
    return (void *)stream->cookie;
}

#define n00b_build_filter_list(output, last)         \
    {                                                \
        output = NULL;                               \
                                                     \
        va_list args;                                \
        void   *item;                                \
                                                     \
        va_start(args, last);                        \
                                                     \
        item = va_arg(args, void *);                 \
        if (item && n00b_in_heap(item)) {            \
            if (n00b_type_is_list(item)) {           \
                output = item;                       \
            }                                        \
            else {                                   \
                output = n00b_list(n00b_type_ref()); \
                while (item) {                       \
                    n00b_list_append(output, item);  \
                    item = va_arg(args, void *);     \
                }                                    \
            }                                        \
        }                                            \
        va_end(args);                                \
    }

extern void n00b_cache_read(n00b_stream_t *, void *);

#endif

// Implementations live in filter.c, but since they take stream_t
// parameters, these come after the above.
extern bool         n00b_filter_add(n00b_stream_t *, n00b_filter_spec_t *);
extern n00b_list_t *n00b_filter_writes(n00b_stream_t *, n00b_stream_msg_t *);
extern n00b_list_t *n00b_filter_reads(n00b_stream_t *, n00b_stream_msg_t *);
extern void         n00b_flush(n00b_stream_t *);
