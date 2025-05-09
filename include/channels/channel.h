#pragma once
#include "n00b.h"

typedef struct n00b_channel_t n00b_channel_t;

typedef struct {
    void   *payload;
    char   *file;
    int64_t id;
    int32_t nitems;
    int32_t line;
} n00b_cmsg_t;

typedef int (*n00b_chan_init_fn)(n00b_channel_t *, void *);
typedef void *(*n00b_chan_r_fn)(n00b_channel_t *, bool *);
typedef void (*n00b_chan_w_fn)(n00b_channel_t *, n00b_cmsg_t *, bool);
typedef bool (*n00b_chan_spos_fn)(n00b_channel_t *, int, bool);
typedef int (*n00b_chan_gpos_fn)(n00b_channel_t *);
typedef void (*n00b_chan_close_fn)(n00b_channel_t *);
typedef n00b_string_t *(*n00b_chan_repr_fn)(n00b_channel_t *);
typedef void (*n00b_chan_sub_fn)(n00b_channel_t *, void *);

typedef struct {
    int                cookie_size;
    n00b_chan_init_fn  init_impl;
    n00b_chan_r_fn     read_impl;
    n00b_chan_w_fn     write_impl;
    n00b_chan_spos_fn  spos_impl; // Set position
    n00b_chan_gpos_fn  gpos_impl; // Get position
    n00b_chan_close_fn close_impl;
    n00b_chan_repr_fn  repr_impl;
    n00b_chan_sub_fn   read_subscribe_cb;
    n00b_chan_sub_fn   read_unsubscribe_cb;
    bool               poll_for_blocking_reads;
    // This should only be set for data objects that are non-mutable,
    // such as strings.
    bool               disallow_blocking_reads;
} n00b_chan_impl;

#define N00B_LOCKRD_IX      0
#define N00B_LOCKWR_IX      1
#define N00B_LOCKSUB_IX     2
#define N00B_CHAN_LOCKSLOTS 3

struct n00b_channel_t {
    // This is in both raw channels and filters in case we want to do
    // notifications per filter level.
    n00b_observable_t pub_info;
    n00b_string_t    *name;
    n00b_list_t      *read_cache;
    n00b_chan_impl   *impl;
    n00b_filter_t    *write_top;
    n00b_filter_t    *read_top;

    // Note that individual r/w requests may be 'blocking'; for a
    // read, the block lasts until the end channel operates, AND the
    // expected number of objects is yielded by the top filter.

    // The channel implementation must block if there's nothing
    // available to fulfill a request. And if filters buffer bits when
    // an operation blocks, they pass down another read request, until
    // they can satisfy the input constraint. That can easily result
    // in the channel blocking.
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
    // API allows you to chain a bunch of filters to a channel, but
    // not to get a reference to the individual filters, just the top
    // one.
    //
    // These are only allowed to be set for filters, but can point to
    // an end-channel. And they only ever apply to r/w ops. Errors can
    // be subscribed to for any channel type, but do not support
    // filtering; they're considered out-of-band, and don't go through
    // the 'implementation'.

    // These bits indicate whether the user can make a
    // channel_write(), chanel_read()The channel might be closed, but
    // the implementation should detect and return the indication to
    // the caller.
    unsigned int w           : 1;
    unsigned int r           : 1;
    unsigned int has_rsubs   : 1;
    unsigned int has_rawsubs : 1;
    unsigned int fd_backed   : 1;

    n00b_list_t  *blocked_readers;
    n00b_mutex_t *locks[N00B_CHAN_LOCKSLOTS];
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

// Channels will proxy the topic name that are read / written to its
// observers the topic information on a write always gets passed to
// the implementation (but is usually just '__write'; the topic()
// interface allows for custom topics). __ topics are reserved (but
// generally passed to the implementation). Reserved topics are
// currently: __read, __write, __log, __exception, __close __read and
// __write are the only two that can be accepted by filters. Other
// topics get passed directly to the underlying channel's
// implementation function.
#ifdef N00B_USE_INTERNAL_API

// Implementations of core channels are expect to call this when done
// writing.
static inline void
n00b_channel_notify(n00b_channel_t *channel, void *msg, int64_t n)
{
    n00b_observable_post(&channel->pub_info, (void *)n, msg);
}

static inline void
n00b_cnotify_r(n00b_channel_t *channel, void *msg)
{
    n00b_channel_notify(channel, msg, N00B_CT_R);
}

static inline void
n00b_cnotify_q(n00b_channel_t *channel, void *msg)
{
    n00b_channel_notify(channel, msg, N00B_CT_Q);
}

static inline void
n00b_cnotify_w(n00b_channel_t *channel, void *msg)
{
    n00b_channel_notify(channel, msg, N00B_CT_W);
}

static inline void
n00b_cnotify_raw(n00b_channel_t *channel, void *msg)
{
    n00b_channel_notify(channel, msg, N00B_CT_RAW);
}

static inline void
n00b_cnotify_close(n00b_channel_t *channel, void *msg)
{
    n00b_channel_notify(channel, msg, N00B_CT_CLOSE);
}

static inline void
n00b_cnotify_error(n00b_channel_t *channel, void *msg)
{
    n00b_channel_notify(channel, msg, N00B_CT_ERROR);
}

#endif

extern n00b_observer_t *n00b_channel_subscribe_read(n00b_channel_t *,
                                                    n00b_channel_t *,
                                                    bool);
extern n00b_observer_t *n00b_channel_subscribe_raw(n00b_channel_t *,
                                                   n00b_channel_t *,
                                                   bool);
extern n00b_observer_t *n00b_channel_subscribe_queue(n00b_channel_t *,
                                                     n00b_channel_t *,
                                                     bool);
extern n00b_observer_t *n00b_channel_subscribe_write(n00b_channel_t *,
                                                     n00b_channel_t *,
                                                     bool);
extern n00b_observer_t *n00b_channel_subscribe_close(n00b_channel_t *,
                                                     n00b_channel_t *);
extern n00b_observer_t *n00b_channel_subscribe_error(n00b_channel_t *,
                                                     n00b_channel_t *,
                                                     bool);
#define n00b_channel_unsubscribe(x) n00b_observer_unsubscribe(x)

extern void _n00b_channel_write(n00b_channel_t *, void *, char *, int);
extern void _n00b_channel_queue(n00b_channel_t *, void *, char *, int);

#define n00b_channel_write(chan, msg) \
    _n00b_channel_write(chan, msg, __FILE__, __LINE__)

#define n00b_channel_queue(chan, msg) \
    _n00b_channel_queue(chan, msg, __FILE__, __LINE__)

// Blocking read
extern void *n00b_channel_read(n00b_channel_t *, int, bool *);
// This is really a non-blocking read that returns a message IF
// one is available.
extern void *n00b_io_dispatcher_process_read_queue(n00b_channel_t *, bool *);

extern int  n00b_channel_get_position(n00b_channel_t *);
extern bool n00b_channel_set_relative_position(n00b_channel_t *, int);
extern bool n00b_channel_set_absolute_position(n00b_channel_t *, int);
extern void n00b_channel_close(n00b_channel_t *);

static inline void
n00b_channel_set_name(n00b_channel_t *stream, n00b_string_t *s)
{
    stream->name = s;
}

static inline bool
n00b_channel_can_read(n00b_channel_t *stream)
{
    return stream->r;
}

static inline bool
n00b_channel_can_write(n00b_channel_t *stream)
{
    return stream->w;
}

#ifdef N00B_USE_INTERNAL_API
static inline void *
n00b_get_channel_cookie(n00b_channel_t *stream)
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
        if (item) {                                  \
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

#endif

// Implementations live in filter.c, but since they take channel_t
// parameters, these come after the above.
extern bool         n00b_filter_add(n00b_channel_t *, n00b_filter_impl *, int);
extern n00b_list_t *n00b_filter_writes(n00b_channel_t *, n00b_cmsg_t *);
extern n00b_list_t *n00b_filter_reads(n00b_channel_t *, n00b_cmsg_t *);
