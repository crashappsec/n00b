#pragma once
#include "n00b.h"
#include "io/observers.h"
#include "io/fd3.h"

typedef struct n00b_channel_t     n00b_channel_t;
typedef struct n00b_filter_step_t n00b_filter_step_t;

typedef enum {
    // Filter whenever a read operation occurs. This occurs whenever
    // the read implementation function is invoked, which is either
    // when the user explicitly calls a read() function, or when the
    // base implementation is event-driven and has read subscribers.
    N00B_CHAN_FILTER_READS,
    // For the other two, any underlying implementation requires
    // something calling n00b_chan_write() or n00b_chan_read().
    // The filtering occurs before the underlying implementation is called.
    N00B_CHAN_FILTER_WRITES,
} n00b_chan_filter_policy;

typedef enum {
    N00B_CHAN_FD,
    N00B_CHAN_CALLBACK,
    N00B_CHAN_PROXY,
    N00B_CHAN_BUFFER,
    N00B_CHAN_STRING,
    N00B_CHAN_TOPIC,
    N00B_CHAN_CUSTOM,
    N00B_CHAN_FILTER,
} n00b_chan_kind;

typedef struct {
    void   *payload;
    char   *file;
    int64_t id;
    int32_t nitems;
    int32_t line;
} n00b_cmsg_t;

typedef int (*n00b_chan_init_fn)(void *, void *);
typedef void *(*n00b_chan_r_fn)(void *, bool, bool *, int);
typedef void (*n00b_chan_w_fn)(void *, n00b_cmsg_t *, bool);
typedef bool (*n00b_chan_spos_fn)(void *, int, bool);
typedef int (*n00b_chan_gpos_fn)(void *);
typedef void (*n00b_chan_close_fn)(void *);
typedef n00b_string_t *(*n00b_chan_repr_fn)(void *);
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
} n00b_chan_impl;

enum {
    N00B_FILTER_READ  = 1,
    N00B_FILTER_WRITE = 2,
    N00B_FILTER_MAX   = 3,
};

typedef struct {
    int                 cookie_size;
    n00b_chan_filter_fn read_fn;
    n00b_chan_filter_fn write_fn;
    n00b_string_t      *name;
    n00b_type_t        *output_type;

    // If `polymorphic_w` is true, it indicates that filter WRITES to
    // a channel may accept multiple types. They must always go to the
    // underlying channel implementation as a buffer. And, any
    // individual filter must produce a single output type, even if
    // it's not intended to go to the channel without first going
    // through a conversion filter.
    //
    // The same thing happens for reads in reverse w
    // `polymorphic_r`... the first filter must accept the raw buffer
    // of the channel, and *should* produce a single type, but it is
    // not required (e.g., marshalling can produce any
    // type). Subsequent layers of filtering may also be flexible both
    // in and out, but in that case they will be type checked with
    // every message, instead of just when setting up the pipeline.
    unsigned int polymorphic_r : 1;
    unsigned int polymorphic_w : 1;

    /* Type checking to be done.
        union {
            n00b_type_t      *type;
            n00b_chan_type_fn validator;
        } input_type;
    */

} n00b_filter_impl;

typedef struct {
    n00b_filter_impl *filter_impl;
    int               policy;
} n00b_filter_spec_t;

struct n00b_filter_step_t {
    n00b_channel_t *next_write_step;
    n00b_channel_t *next_read_step;
    n00b_channel_t *core;
    unsigned int    w_skip : 1;
    unsigned int    r_skip : 1;
};

struct n00b_channel_t {
    // This is in both raw channels and filters in case we want to do
    // notifications per filter level.
    n00b_observable_t pub_info;
    n00b_string_t    *name;
    n00b_list_t      *read_cache;
    n00b_chan_kind    kind;

    union {
        n00b_chan_impl   *channel;
        n00b_filter_impl *filter;
    } impl;

    union {
        n00b_filter_step_t *filter;
        n00b_channel_t     *read_top;
    } stack;

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
    unsigned int is_core     : 1;
    unsigned int has_rsubs   : 1;
    unsigned int has_rawsubs : 1;

    // Channel-specific data.  Must start with *3* lock slots for base
    // channels.
    n00b_mutex_t *info[];
};
typedef struct {
    n00b_fd_stream_t *stream;
    n00b_fd_sub_t    *sub;
    bool              have_read_subscribers;
    n00b_buf_t       *read_cache;
} n00b_fd_cookie_t;

enum {
    N00B_CT_R,
    N00B_CT_Q,
    N00B_CT_W,
    N00B_CT_RAW,
    N00B_CT_CLOSE,
    N00B_CT_ERROR,
    N00B_CT_NUM_TOPICS,
};

#define N00B_LOCKRD_IX  0
#define N00B_LOCKWR_IX  1
#define N00B_LOCKSUB_IX 2
#define N00B_CTHUNK_IX  3

static inline n00b_channel_t *
n00b_channel_core(n00b_channel_t *c)
{
    if (c->is_core) {
        return c;
    }
    return c->stack.filter->core;
}

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
    assert(channel->is_core);
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

extern n00b_channel_t *n00b_channel_filter_add(n00b_channel_t *,
                                               n00b_filter_spec_t *);
extern n00b_channel_t *n00b_channel_create(n00b_chan_impl *,
                                           void *,
                                           n00b_list_t *);

extern void _n00b_channel_write(n00b_channel_t *, void *, char *, int);
extern void _n00b_channel_queue(n00b_channel_t *, void *, char *, int);

#define n00b_channel_write(chan, msg) \
    _n00b_channel_write(chan, msg, __FILE__, __LINE__)

#define n00b_channel_queue(chan, msg) \
    _n00b_channel_queue(chan, msg, __FILE__, __LINE__)

extern void *n00b_channel_read(n00b_channel_t *channel, int ms_timeout);
extern void  n00b_io_dispatcher_process_read_queue(n00b_channel_t *);

// This is probably moving to channel_fd.h
extern n00b_channel_t *_n00b_new_fd_channel(n00b_fd_stream_t *fd, ...);
extern n00b_channel_t *_n00b_channel_open_file(n00b_string_t *filename, ...);

extern int  n00b_channel_get_position(n00b_channel_t *);
extern bool n00b_channel_set_relative_position(n00b_channel_t *, int);
extern bool n00b_channel_set_absolute_position(n00b_channel_t *, int);
extern void n00b_channel_close(n00b_channel_t *);
