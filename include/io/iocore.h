#pragma once
#include "n00b.h"

// With low-level event object types, you can currently subscribe to
// one of 5 channels:
//
// 1. Reads. When the event is a 'source', this gives you the
//    post-filtered read.
//
// 2. Raw writes. The value that the 'sink' side of an event type is
//    asked to write before any filtering occurs.
//
// 3. Filtered writes. You're subscribing to the final written value.
//
// 4. Close events. The object is passed when close is called or
//    detected.
//
// 5. Error conditions. This isn't fully implemented; some filters
//    always raise fatal errors instead.
//
// Where error channels are actually used under the hood, you can
// choose to ignore that error (which is done by default when you use
// a listener).
//
// Generally, you can route anything file-descriptor based wherever
// you want via subscription, as long as the perms of both sides
// support it.
//
// Subscribing to the 'read' side captures things coming in from the
// file descriptor, and subscripting to the 'write' side captures
// anything the USER sends via a 'write' call.
//
// The semantics of what's a 'read' and what's a 'write' is less clear
// for other event types. If a timer or a signal fires, that's
// producing data that clearly, if the user wants it, should go out
// the 'write' side.
//
// However, when we are routing between event types, we are
// essentially "skipping the read". If we route two sockets together,
// reads we accept on A, we WRITE to B, so subscriptions cause us to
// WRITE to the object that was subscribed.
//
// Even if we subscribe to what we're writing to a socket, and want to
// route that to a different socket, we're not *reading* from the
// target, we're writing to it.
//
// So messages that flow through subscriptions ALWAYS go to the
// 'write' side, and are considered messages that should (pending
// filtering) be written.
//
// The 'read' side is reserved for things where we want to collect
// data that we might need outside of any subscription, e.g., by the
// user calling 'n00b_read()' to kick off collecting the data.
//
// If the user wants data asynchronously, subscriptions will probably
// happen in the background, though.
//
// To understand, let's go through some of the other event types that
// are not file-descriptor based:
//
//  1) signal-- This is akin to a socket for us. We can READ signals
//              when they show up, but it wouldn't make much sense to
//              send data back. Since it makes no sense to write to a
//              'signald, these can't except writes, and thus cannot
//              receive messages routed from other events.
//
//  2) exitinfo-- This is a very similar story right now. Currently,
//                this is essentially wait4() info, but expect on
//                linux to use procfd, where info MIGHT end up
//                bi-directional. But for the moment, this is
//                available for read only.
//
//  3) timer-- Think of this as a file descriptor connected to the
//             system clock. Again, you can 'read' it when it fires.
//             'Writing' to the timer starts the timer (close it to
//             stop it).
//
//             These timers are different from the timeouts you can
//             add to subscriptions-- those timeouts, when specified,
//             will remove subscriptions if an event doesn't fire in
//             sufficient time (you can set a callback on the
//             subscription object if you want). For event types
//             backed directly by libevent, we use the built-in
//             capabilties.
//
//  4) condition -- This is our extension of a 'condition variable'.
//                  We've set this up so that you can more easily
//                  protect data without having to worry too much
//                  about the locking.
//
//                  First there are ops to grab the lock and deal with
//                  fixed data fields.
//
//                  AND, whenever a write to a condition happens, we
//                  acquire the lock before signaling, and call a
//                  callback (if provided) to deal with the write in a
//                  user-defined way, sticking the result in one of
//                  the pre-defined fields. Then, we take the entire
//                  condition variable and pass it through any
//                  filters. If it pops out the end, then we SIGNAL
//                  one (and only one) waiter, who can then safely
//                  access the munged state.
//
//                  If you subscribe to the read side here, you'll see
//                  the raw data that triggers the callback.
//
//                  If you subscribe to pre-filter writes, you'll see
//                  the value of the 'aux' field after the callback.
//
//                  If you subscribe to post-filter writes, you will
//                  get the value of 'aux' as well, but
//                  post-filtering, and ONLY if the condition was
//                  signaled.
//
//                  We use this to implement synchronous reads without
//                  regard to blocking. We use a condition variable to
//                  let us sleep until a read is done. We essentially
//                  just subscribe that condition variable to the
//                  appropriate place (note, we use conditions for
//                  writes, but in a different way; see below).
//
//                  In a bit of a twist, the debugging subsystem uses
//                  a condition to make sure that all debug messages
//                  get written before the process exits. No call to
//                  n00b_debug blocks. However, they all bump up a
//                  message count that goes DOWN as the messages are
//                  properly delivered.
//
//                  That state lives in the condition variable and its
//                  locks.
//
//                  On exit, we first stop accepting new debug message
//                  delivery. Then, we keep sitting on that condition
//                  variable until there are no more pending debug
//                  messages; once the value gets to 0, we allow the
//                  process to exit.
//
//  5) callbacks -- While it would make sense to subscribe to the
//                  inputs of arbitrary functions, we are not
//                  supporting that; we would like to do it for
//                  N00b-specific functions, but it will be its own
//                  type. Here, this is just a sink to receive
//                  messages via subscriptions, so you *can* subscribe
//                  to the write-side to audit what gets sent here,
//                  and you can add filters too. But there is only a
//                  write-side available.
//
//  6) topics -- General purpose named topics that can be read from or
//               written to. Topics have 'namespaces', and
//               subscriptions to topics should be done by the topic
//               interface, which will automatically add a transform
//               based on the kind via a subscription proxy. By
//               default, with file descriptors, the system will write
//               out a JSON blob for regular files, and soon will
//               marshal for other files.
//
//
// Note that most things have the expectation of run-time type
// checking (we may address that at some point).  Data read off a
// socket or fd raw will be in n00b_buf_t objects, but filtering can
// produce objects of any type.
//
// Data written to file descriptors can come in as any type of object,
// as long as installed filters deal with it, and present a buf_t at
// the end of the process.
//
// One special case is condition variables for blocking writes. With
// reads, waiting for a read always means waiting for the 'next' read
// to get yielded, so that's easy to wait on. For writes, you
// generally are waiting for 'your' write to complete.
//
// But filters can munge written inputs into multiple messages, or
// even block stuff. So it's not exactly clear what the 'right'
// semantics for a blocking read are, without help from the installed
// filters.
//
// To this end, all write filters must be prepared to take a condition
// variable, and decide when to 'publish' it, at which point we treat
// it much like we do with reads. The difference here is we're not
// subscribing to the condition variable... we're passing it through
// the filter pipeline as a message.
//
// Todo on that (and for most of this API is allowing for flow of info
// around error conditions, like cancelations / dropping of messages).
//
// In summary though, the current event types are available for the
// following ops:
//
// file-- file descriptors that are not sockets. rw depending on file.
// socket -- rw
// signal -- r
// exitinfo -- r
// timer -- rw
// condition -- rw (but mostly meant for w)
// callback -- w
// topic -- rw
//
// Note that libevent allows individual events to have timeouts, but
// we do not use that on most things. That's because doing
// per-subscription timeouts would be a right pain. It's easier to set
// multiple timers and deal with the possible race condition than to
// use one with guaranteed atomicity.
//
// Additionally, you can create a 'listener' party, which will set you
// up with a TCP listener on a specified port. Its 'read' events are
// simply accept()s on connecting sockets. When you create a listener,
// you just have to pass in what you want auto-subscribed to the read
// side of any socket (which can either be a n00b_streamt_t or a
// n00b_list_t containing only n00b_stream_t objects).
//
// If you want to get notified whenever a connection occurs, you can
// subscribe to the listener's 'read' event; the value published will
// be the created n00b_stream_t object of the resulting socket.
// In fact, there's a special hidden 'autosubscribe' callback that
// is automatically subscribed to this event.
//
// The open callback takes a primary key, and then an arbitrary custom
// pointer.

typedef struct n00b_io_impl_info_t n00b_io_impl_info_t;
typedef struct n00b_stream_sub_t   n00b_stream_sub_t;

typedef enum n00b_io_permission_t {
    n00b_io_perm_none = 0,
    n00b_io_perm_r    = 1,
    n00b_io_perm_w    = 2,
    n00b_io_perm_rw   = 3,
} n00b_io_permission_t;

typedef enum n00b_io_subscription_kind {
    n00b_io_sk_read       = 0,
    n00b_io_sk_pre_write  = 1,
    n00b_io_sk_post_write = 2,
    n00b_io_sk_error      = 3,
    n00b_io_sk_close      = 4,
} n00b_io_subscription_kind;

typedef struct n00b_stream_base_t {
    struct event_base *event_ctx;
    n00b_dict_t       *io_fd_cache;
    n00b_dict_t       *io_signal_cache;
    n00b_dict_t       *io_topic_ns_cache;
    n00b_lock_t        lock;
    bool               system;
} n00b_stream_base_t;

typedef enum {
    n00b_io_ev_file        = 1,
    n00b_io_ev_socket      = 2,
    n00b_io_ev_listener    = 3,
    n00b_io_ev_signal      = 4,
    n00b_io_ev_timer       = 5,
    n00b_io_ev_exitinfo    = 6,
    n00b_io_ev_condition   = 8,
    n00b_io_ev_callback    = 8,
    n00b_io_ev_topic       = 9,
    n00b_io_ev_passthrough = 10,
    n00b_io_ev_buffer      = 11,
    n00b_io_ev_string      = 12,
    n00b_io_ev_guard,
} n00b_io_event_type;

typedef struct n00b_exitinfo_t {
    pid_t                    pid;
    int                      stats;
    struct rusage            rusage;
    _Atomic int              streams_to_drain;
    _Atomic(n00b_thread_t *) wait_thread;
    bool                     exited;
} n00b_exitinfo_t;

struct n00b_stream_t {
    void                *cookie;
    n00b_list_t         *read_filters;
    n00b_list_t         *write_filters;
    n00b_dict_t         *read_subs;
    n00b_dict_t         *write_start_subs;
    n00b_dict_t         *write_subs;
    n00b_dict_t         *error_subs;
    n00b_dict_t         *close_subs;
    n00b_string_t       *repr;
    n00b_lock_t          lock;
    struct timeval      *read_start_time;
    n00b_io_event_type   etype;
    unsigned int         closed_for_read      : 1;
    unsigned int         closed_for_write     : 1;
    unsigned int         is_tty               : 1;
    // Indicate whether there are subscribers for the particular event.
    unsigned int         read_active          : 1;
    unsigned int         write_active         : 1;
    unsigned int         error                : 1;
    unsigned int         perms                : 2;
    unsigned int         ignore_unseen_errors : 1;
    unsigned int         close_notified       : 1;
    n00b_io_impl_info_t *impl;
    uint64_t             next_read_limit;
};

struct n00b_stream_sub_t {
    n00b_stream_t            *source;
    n00b_stream_t            *sink;
    void                     *cookie;
    struct timeval           *subscription_time;
    struct timeval           *timeout;
    struct event             *to_event;
    n00b_io_subscription_kind kind;
    unsigned int              one_shot : 1;
};

typedef struct n00b_cv_cookie_t {
    n00b_condition_t *condition;
    void             *aux;
} n00b_cv_cookie_t;

#define N00B_SEEK_END -1

typedef struct n00b_bio_cookie_t {
    n00b_buf_t *b;
    int32_t     read_position;
    int32_t     write_position;
} n00b_bio_cookie_t;

typedef struct {
    n00b_string_t *s;
    int            position;
} n00b_string_cookie_t;

typedef void (*n00b_accept_cb)(n00b_stream_t *);

typedef struct n00b_listener_aux {
    n00b_net_addr_t       *addr;
    n00b_stream_t         *autosub;
    n00b_list_t           *read_filters;
    n00b_list_t           *write_filters;
    n00b_accept_cb         accept_cb;
    struct evconnlistener *server;
} n00b_listener_aux;

typedef struct n00b_ev2_cookie_t {
    int64_t       id; // Expected to be first.
    struct event *read_event;
    struct event *write_event;
    n00b_buf_t   *pending_write;
    int           pending_ix;
    n00b_list_t  *backlog;
    void         *aux; // File name, remote connection, etc.
    int           socket : 1;
    int           signal : 1;
    int64_t       bytes_read;
#ifdef N00B_EV_STATS
    int64_t bytes_written;
#endif
} n00b_ev2_cookie_t;

typedef void (*n00b_signal_fn)(int);
typedef n00b_stream_t *(*n00b_io_register_fn)(void *, n00b_io_impl_info_t *);
typedef bool (*n00b_io_subscribe_fn)(n00b_stream_sub_t *,
                                     n00b_io_subscription_kind);
typedef void (*n00b_io_unsubscribe_fn)(n00b_stream_sub_t *);
typedef bool (*n00b_io_read_fn)(n00b_stream_t *, uint64_t);
typedef void *(*n00b_io_write_start_fn)(n00b_stream_t *, void *);
typedef void (*n00b_io_close_fn)(n00b_stream_t *);
typedef bool (*n00b_io_eof_fn)(n00b_stream_t *);

// Returns a list of messages.
typedef n00b_list_t *(*n00b_filter_fn)(n00b_stream_t *, void *, void *);
typedef void (*n00b_io_callback_fn)(n00b_stream_t *, void *, void *);
typedef n00b_string_t *(*n00b_io_repr_fn)(n00b_stream_t *);
typedef bool (*n00b_callback_filter)(void *, n00b_stream_t *);

struct n00b_io_impl_info_t {
    n00b_string_t         *name;
    void                  *cookie;
    n00b_io_register_fn    open_impl;
    n00b_io_subscribe_fn   subscribe_impl;
    n00b_io_unsubscribe_fn unsubscribe_impl;
    n00b_io_read_fn        read_impl;
    n00b_io_write_start_fn write_impl;
    n00b_io_repr_fn        repr_impl;
    n00b_io_close_fn       close_impl;
    n00b_io_eof_fn         eof_impl;
    bool                   use_libevent;
    bool                   byte_oriented;
};

typedef struct n00b_stream_filter_t {
    void          *state;
    int            cookie_size;
    n00b_filter_fn xform_fn;
    n00b_filter_fn flush_fn;
    n00b_string_t *name; // Mainly for debugging.
} n00b_stream_filter_t;

typedef n00b_stream_filter_t *(*n00b_filter_create_fn)(n00b_stream_t *);

typedef struct n00b_callback_cookie_t {
    n00b_io_callback_fn fn;
    void               *aux;
    n00b_notifier_t    *notifier;
    bool                notify;
} n00b_callback_cookie_t;

typedef struct {
    n00b_callback_cookie_t *cookie;
    n00b_stream_t          *stream;
    void                   *msg;
} n00b_iocb_info_t;

typedef struct n00b_topic_cookie_t {
    n00b_string_t        *name;
    n00b_stream_filter_t *socket_write_filter;
    n00b_stream_filter_t *socket_read_filter;
    n00b_stream_filter_t *file_write_filter;
    n00b_stream_filter_t *file_read_filter;
} n00b_topic_cookie_t;

typedef struct n00b_message_t {
    n00b_string_t *topic;
    void          *payload;
} n00b_message_t;

extern bool               n00b_io_run_base(n00b_stream_base_t *, int);
extern n00b_stream_t     *n00b_open_event(n00b_string_t *, void *);
extern n00b_stream_sub_t *n00b_io_subscribe(n00b_stream_t *,
                                            n00b_stream_t *,
                                            n00b_duration_t *,
                                            n00b_io_subscription_kind);
extern void               n00b_io_unsubscribe(n00b_stream_sub_t *);
extern bool               n00b_close(n00b_stream_t *);
extern void               n00b_write(n00b_stream_t *, void *);
extern bool               n00b_at_eof(n00b_stream_t *);
extern void               n00b_write_blocking(n00b_stream_t *,
                                              void *,
                                              n00b_duration_t *);
extern void              *n00b_read(n00b_stream_t *,
                                    uint64_t,
                                    n00b_duration_t *);
// Init w/o launching a thread in an IO loop.
extern void               n00b_io_init(void);
extern void               n00b_launch_io_loop(void);
extern void               n00b_io_loop_once(void);
extern n00b_stream_t     *n00b_fd_open(int);
extern void               n00b_io_close(n00b_stream_t *);
extern n00b_string_t     *n00b_io_fd_repr(n00b_stream_t *);
extern n00b_stream_t     *n00b_pid_monitor(int64_t, void *);
extern n00b_stream_t     *n00b_callback_open(n00b_io_callback_fn, void *);
extern n00b_stream_t     *n00b_condition_open(n00b_condition_t *, void *);
extern n00b_stream_t     *n00b_io_get_signal_event(int);
extern n00b_stream_sub_t *_n00b_io_register_signal_handler(int,
                                                           n00b_io_callback_fn,
                                                           ...);
extern n00b_stream_t     *n00b_io_listener(n00b_net_addr_t *,
                                           n00b_stream_t *,
                                           void *,
                                           void *,
                                           n00b_accept_cb);
extern bool               n00b_wait_for_io_shutdown(void);
extern void               n00b_io_begin_shutdown(void);
extern n00b_stream_t     *n00b_connect(n00b_net_addr_t *);
extern n00b_stream_t     *n00b_get_topic(n00b_string_t *, n00b_string_t *);
extern bool               n00b_topic_unsubscribe(n00b_stream_t *,
                                                 n00b_stream_t *);
extern void               n00b_topic_post(void *, void *);
extern n00b_stream_t     *n00b_new_subscription_proxy(void);
extern n00b_string_t     *n00b_get_signal_name(int64_t);
extern n00b_stream_t     *n00b_buffer_stream_open(n00b_buf_t *,
                                                  n00b_io_permission_t);
extern n00b_stream_t     *n00b_stdin(void);
extern n00b_stream_t     *n00b_stdout(void);
extern n00b_stream_t     *n00b_stderr(void);

#define n00b_io_register_signal_handler(x, y, ...) \
    _n00b_io_register_signal_handler((x), (y), __VA_ARGS__ __VA_OPT__(, ) 0)

static inline bool
n00b_stream_can_read(n00b_stream_t *party)
{
    return party->read_subs || !party->closed_for_read;
}

static inline bool
n00b_stream_can_write(n00b_stream_t *party)
{
    return party->write_subs || !party->closed_for_write;
}

static inline n00b_stream_sub_t *
n00b_io_subscribe_oneshot(n00b_stream_t            *p1,
                          n00b_stream_t            *p2,
                          n00b_io_subscription_kind kind)

{
    n00b_stream_sub_t *result = n00b_io_subscribe(p1, p2, NULL, kind);
    result->one_shot          = true;

    return result;
}

static inline bool
n00b_is_tty(n00b_stream_t *party)
{
    return party->is_tty;
}

static inline void
n00b_io_set_repr(n00b_stream_t *event, n00b_string_t *repr)
{
    event->repr = repr;
}

static inline n00b_stream_sub_t *
n00b_io_subscribe_to_close(n00b_stream_t *source, n00b_stream_t *sink)
{
    return n00b_io_subscribe_oneshot(source, sink, n00b_io_sk_close);
}

static inline n00b_stream_sub_t *
n00b_io_subscribe_to_writes(n00b_stream_t   *source,
                            n00b_stream_t   *sink,
                            n00b_duration_t *timeout)
{
    return n00b_io_subscribe(source, sink, timeout, n00b_io_sk_pre_write);
}

static inline void
n00b_subscribe_to_errors(n00b_stream_t *source, n00b_stream_t *sink)
{
    n00b_io_subscribe(source, sink, NULL, n00b_io_sk_error);
}

static inline n00b_stream_sub_t *
n00b_io_subscribe_to_delivery(n00b_stream_t   *source,
                              n00b_stream_t   *sink,
                              n00b_duration_t *timeout)
{
    return n00b_io_subscribe(source, sink, timeout, n00b_io_sk_post_write);
}

static inline n00b_stream_sub_t *
n00b_io_subscribe_to_reads(n00b_stream_t   *source,
                           n00b_stream_t   *sink,
                           n00b_duration_t *timeout)
{
    return n00b_io_subscribe(source, sink, timeout, n00b_io_sk_read);
}

static inline n00b_stream_sub_t *
n00b_io_add_read_callback(n00b_stream_t      *source,
                          n00b_io_callback_fn cb,
                          void               *thunk)
{
    n00b_stream_t *sink = n00b_callback_open(cb, thunk);
    return n00b_io_subscribe_to_reads(source, sink, NULL);
}

static inline n00b_stream_sub_t *
n00b_io_set_write_callback(n00b_stream_t      *source,
                           n00b_io_callback_fn cb,
                           void               *thunk)
{
    n00b_stream_t *sink = n00b_callback_open(cb, thunk);
    return n00b_io_subscribe_to_writes(source, sink, NULL);
}

static inline n00b_stream_sub_t *
n00b_io_set_delivery_callback(n00b_stream_t      *source,
                              n00b_io_callback_fn cb,
                              void               *thunk)
{
    n00b_stream_t *sink = n00b_callback_open(cb, thunk);
    return n00b_io_subscribe_to_delivery(source, sink, NULL);
}

static inline void
n00b_make_oneshot(n00b_stream_sub_t *sub)
{
    sub->one_shot = true;
}

static inline void
n00b_ignore_unseen_errors(n00b_stream_t *e)
{
    e->ignore_unseen_errors = true;
}

static inline n00b_stream_sub_t *
n00b_topic_subscribe(n00b_stream_t *topic_obj, n00b_stream_t *subscriber)
{
    if ((!topic_obj) || topic_obj->etype != n00b_io_ev_topic) {
        N00B_CRAISE("Cannot subscribe; no topic object.");
    }

    return n00b_io_subscribe_to_delivery(topic_obj, subscriber, NULL);
}

static inline void
n00b_putcp(n00b_stream_t *stream, n00b_codepoint_t cp)
{
    n00b_string_t *s = n00b_string_repeat(cp, 1);
    n00b_write(stream, s);
}

static inline void
n00b_putc(n00b_stream_t *stream, char c)
{
    n00b_putcp(stream, (n00b_codepoint_t)c);
}

static inline void
n00b_stream_write_memory(n00b_stream_t *stream, void *ptr, size_t len)
{
    n00b_buf_t *b = n00b_new(n00b_type_buffer(),
                             n00b_kw("length", n00b_ka(len), "raw", ptr));
    n00b_write(stream, b);
}

static inline void
n00b_puts(n00b_stream_t *stream, char *s)
{
    n00b_stream_write_memory(stream, s, strlen(s));
}

static inline void
n00b_puti(n00b_stream_t *stream, int64_t n)
{
    little_64(n);
    n00b_stream_write_memory(stream, &n, sizeof(int64_t));
}

static inline n00b_stream_t *
n00b_instream_buffer(n00b_buf_t *b)
{
    return n00b_buffer_stream_open(b, n00b_io_perm_r);
}

static inline n00b_stream_t *
n00b_outstream_buffer(n00b_buf_t *b, bool end)
{
    n00b_stream_t *result = n00b_buffer_stream_open(b, n00b_io_perm_w);

    if (end) {
        n00b_bio_cookie_t *c = result->cookie;
        c->write_position    = N00B_SEEK_END;
    }

    return result;
}

static inline n00b_stream_t *
n00b_iostream_buffer(n00b_buf_t *b)
{
    return n00b_buffer_stream_open(b, n00b_io_perm_rw);
}

extern n00b_stream_t *n00b_stream_string(n00b_string_t *s);
extern n00b_stream_t *n00b_instream_file(n00b_string_t *);
extern n00b_stream_t *n00b_outstream_file(n00b_string_t *, bool, bool);
extern n00b_stream_t *n00b_iostream_file(n00b_string_t *, bool);
extern void           n00b_ignore_uncaught_io_errors(void);
extern void          *n00b_stream_read_all(n00b_stream_t *);
extern void           _n00b_print(n00b_obj_t, ...);

#define n00b_print(s, ...) _n00b_print(s, N00B_VA(__VA_ARGS__))
#define n00b_eprint(...)   _n00b_print(n00b_stderr(), N00B_VA(__VA_ARGS__))

#define n00b_printf(fmt, ...)                                  \
    {                                                          \
        n00b_string_t *__str = n00b_cformat(fmt, __VA_ARGS__); \
        _n00b_print(__str, NULL);                              \
    }

#define n00b_eprintf(fmt, ...)                                 \
    {                                                          \
        n00b_string_t *__str = n00b_cformat(fmt, __VA_ARGS__); \
        _n00b_print(n00b_stderr(), __str, NULL);               \
    }

#ifdef N00B_DEBUG
void _n00b_cprintf(char *, int64_t, ...);

#define cprintf(fmt, ...) \
    _n00b_cprintf(fmt, N00B_PP_NARG(__VA_ARGS__) __VA_OPT__(, ) __VA_ARGS__)
#endif

#ifdef N00B_USE_INTERNAL_API
#define n00b_event_add(x, y) \
    if (x) {                 \
        event_add(x, y);     \
    }

#define n00b_event_del(x)                                       \
    {                                                           \
        if (event_pending(x,                                    \
                          EV_TIMEOUT                            \
                              | EV_READ | EV_WRITE | EV_SIGNAL, \
                          NULL)) {                              \
            event_del(x);                                       \
        }                                                       \
    }

extern bool                n00b_io_has_exited;
extern n00b_stream_base_t *n00b_system_event_base;
extern n00b_io_impl_info_t n00b_fileio_impl;
extern n00b_io_impl_info_t n00b_socket_impl;
extern n00b_io_impl_info_t n00b_signal_impl;
extern n00b_io_impl_info_t n00b_listener_impl;
extern n00b_io_impl_info_t n00b_timer_impl;
extern n00b_io_impl_info_t n00b_exitinfo_impl;
extern n00b_io_impl_info_t n00b_condition_impl;
extern n00b_io_impl_info_t n00b_callback_impl;
extern n00b_io_impl_info_t n00b_topic_impl;
extern n00b_io_impl_info_t n00b_subproxy_impl;
extern n00b_io_impl_info_t n00b_bufferio_impl;
extern n00b_io_impl_info_t n00b_strstream_impl;

extern n00b_stream_base_t *get_system_event_base(void);
extern void                n00b_io_ev_unsubscribe(n00b_stream_sub_t *);
extern n00b_stream_sub_t  *n00b_raw_subscribe(n00b_stream_t *,
                                              n00b_stream_t *,
                                              n00b_duration_t *,
                                              n00b_io_subscription_kind,
                                              bool);
extern void                n00b_raw_unsubscribe(n00b_stream_sub_t *);
extern bool                n00b_io_enqueue_fd_read(n00b_stream_t *, uint64_t);
extern void               *n00b_io_enqueue_fd_write(n00b_stream_t *, void *);
extern bool                n00b_io_fd_subscribe(n00b_stream_sub_t *,
                                                n00b_io_subscription_kind);
extern n00b_stream_t      *n00b_io_file_open(int64_t, n00b_io_impl_info_t *);
extern n00b_stream_t      *n00b_io_socket_open(int64_t, n00b_io_impl_info_t *);
extern n00b_stream_t      *n00b_add_or_replace(n00b_stream_t *,
                                               n00b_dict_t *,
                                               void *);
extern n00b_stream_t      *n00b_io_timer_open(n00b_duration_t *,
                                              n00b_io_impl_info_t *);
extern bool                n00b_post_to_subscribers(n00b_stream_t *,
                                                    void *,
                                                    n00b_io_subscription_kind);
extern void                n00b_purge_subscription_list_on_boundary(n00b_dict_t *);
extern void                n00b_ev2_r(evutil_socket_t, short, void *);
extern void                n00b_ev2_w(evutil_socket_t, short, void *);
extern n00b_string_t      *n00b_get_fd_extras(n00b_stream_t *);
extern n00b_stream_base_t *get_n00b_system_event_base(void);
extern n00b_list_t        *n00b_handle_read_operation(n00b_stream_t *, void *);
extern void                n00b_initialize_event(n00b_stream_t *,
                                                 n00b_io_impl_info_t *,
                                                 n00b_io_permission_t,
                                                 n00b_io_event_type,
                                                 void *);
extern bool                n00b_in_io_thread(void);
extern void                n00b_handle_one_delivery(n00b_stream_t *, void *);
extern void                n00b_ioqueue_dont_block_callbacks(void);
extern void                n00b_ioqueue_enqueue_callback(n00b_iocb_info_t *);
extern void                n00b_ioqueue_dont_block_callbacks(void);
extern void                n00b_ioqueue_launch_callback_thread(void);
extern n00b_string_t      *n00b_stream_full_repr(n00b_stream_t *);
extern void                n00b_internal_io_setup(void);

static inline n00b_stream_base_t *
n00b_get_ev_base(n00b_io_impl_info_t *impl)
{
    if (impl->cookie) {
        return impl->cookie;
    }
    return n00b_system_event_base;
}

static inline n00b_ev2_cookie_t *
n00b_new_ev2_cookie(void)
{
    return n00b_gc_alloc_mapped(n00b_ev2_cookie_t, N00B_GC_SCAN_ALL);
}

static inline void
_n00b_acquire_event_base(n00b_stream_base_t *eb, char *f, int l)
{
    _n00b_lock_acquire(&eb->lock, f, l);
}

static inline void
n00b_release_event_base(n00b_stream_base_t *eb)
{
    n00b_lock_release(&eb->lock);
}

#define n00b_acquire_event_base(eb)                   \
    _n00b_acquire_event_base(eb, __FILE__, __LINE__); \
    defer(n00b_release_event_base(eb))

static inline n00b_stream_t *
n00b_alloc_party(n00b_io_impl_info_t *impl,
                 void                *cookie,
                 n00b_io_permission_t perms,
                 n00b_io_event_type   et)
{
    n00b_stream_t *s = n00b_new(n00b_type_stream(), impl, cookie, perms, et);
    return s;
}

#ifdef N00B_TMP_DEBUG
static inline void
_n00b_core_acquire_party(n00b_stream_t *party, char *file, int line)
{
    if (party != NULL) {
        cprintf("Acquire: %s (%s:%d)\n", party, file, line);
        n00b_lock_acquire(&party->lock);
    }
}

#else
static inline bool
_n00b_core_acquire_party(n00b_stream_t *party, char *file, int line)
{
    if (party != NULL) {
        _n00b_lock_acquire(&((party)->lock), file, line);
        return true;
    }
    return false;
}
#endif

#define n00b_core_acquire_party(x) _n00b_core_acquire_party(x, __FILE__, __LINE__)

#ifdef N00B_TMP_DEBUG
#define n00b_release_party(x) _n00b_release_party(x, __FILE__, __LINE__)
static inline void
_n00b_release_party(n00b_stream_t *party, char *file, int line)
{
    if (party != NULL) {
        cprintf("Release: %s (%s:%d)\n", party, file, line);
        n00b_lock_release(&party->lock);
    }
}
#else
static inline void
n00b_release_party(n00b_stream_t *party)
{
    if (party != NULL) {
        n00b_lock_release(&party->lock);
    }
}
#endif

#define n00b_acquire_party(p)              \
    if (n00b_core_acquire_party(p)) {      \
        n00b_defer(n00b_release_party(p)); \
    }

static inline n00b_stream_sub_t *
n00b_alloc_subscription(n00b_stream_t *src)
{
    n00b_stream_sub_t *sub = n00b_gc_alloc_mapped(n00b_stream_t,
                                                  N00B_GC_SCAN_ALL);
    sub->source            = src;

    return sub;
}

static inline n00b_dict_t *
n00b_stream_get_subscriptions(n00b_stream_t            *party,
                              n00b_io_subscription_kind k)
{
    switch (k) {
    case n00b_io_sk_read:
        return party->read_subs;
    case n00b_io_sk_pre_write:
        return party->write_start_subs;
    case n00b_io_sk_error:
        return party->error_subs;
    case n00b_io_sk_close:
        return party->close_subs;
    default:
        return party->write_subs;
    }
}

static inline bool
n00b_stream_has_remaining_subscribers(n00b_stream_t            *source,
                                      n00b_io_subscription_kind k)
{
    n00b_dict_t *subs = n00b_stream_get_subscriptions(source, k);
    return subs && n00b_len(subs) != 0;
}

// The way we use libev, read subscriptions are always active as long
// as someone is a subscriber.  Write events are only ever done as
// edge-triggered one-shots; we re-add each time if appropriate.
static inline void
n00b_ev2_post_read_cleanup(n00b_stream_t *party)
{
    if (!n00b_stream_has_remaining_subscribers(party, n00b_io_sk_read)) {
        n00b_ev2_cookie_t *cookie = party->cookie;
        n00b_event_del(cookie->read_event);
    }
}

static inline n00b_notifier_t *
n00b_get_io_callback_exit_notifier(n00b_stream_t *s)
{
    if (s->etype != n00b_io_ev_callback) {
        return NULL;
    }

    n00b_callback_cookie_t *c = s->cookie;
    c->notify                 = true;
    c->notifier               = n00b_new_notifier();

    return c->notifier;
}

static inline void
n00b_fd_make_blocking(int fd)
{
    int flags = fcntl(fd, F_GETFL) | O_NONBLOCK;
    fcntl(fd, F_SETFL, flags);
}

static inline void
n00b_fd_make_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL) & ~O_NONBLOCK;
    fcntl(fd, F_SETFL, flags);
}

extern void
n00b_post_error_internal(n00b_stream_t *e,
                         n00b_string_t *msg,
                         void          *context,
                         n00b_table_t  *backtrace,
                         char          *filename,
                         int            line);

#define n00b_post_error(x, m) n00b_post_error_internal(x,        \
                                                       m,        \
                                                       NULL,     \
                                                       NULL,     \
                                                       __FILE__, \
                                                       __LINE__)

#define n00b_post_cerror(x, m) n00b_post_error(x, n00b_cstring(m))

#define n00b_post_error_ctx(x, c, m) n00b_post_error_internal(x,            \
                                                              m,            \
                                                              c,            \
                                                              n00b_trace(), \
                                                              __FILE__,     \
                                                              __LINE__)

#define n00b_post_cerror_ctx(x, c, m) \
    n00b_post_error_ctx(x, c, n00b_cstring(m))

#define n00b_post_errcode(event, code) \
    {                                  \
        char msg[2048] = {             \
            0,                         \
        };                             \
        strerror_r(code, msg, 2048);   \
        n00b_post_cerror(event, msg);  \
    }

#define n00b_post_errno(event) n00b_post_errcode(event, errno)

// Since the IO system shouldn't use itself recursively, use iodb_...
#define N00B_IODB
#ifdef N00B_IODB
#define n00b_iodb(x, y) \
    printf("%s: %s\n", x, n00b_to_string((n00b_repr(y))->data));

#define n00b_iodb_hex(x, y)                                                    \
    {                                                                          \
        n00b_type_t *t = n00b_get_my_type(y);                                  \
        if (n00b_type_is_buffer(t)) {                                          \
            n00b_buf_t *b = y;                                                 \
            printf("%s:\n%s\n", x, n00b_hex_dump(b->data, b->byte_len)->data); \
        }                                                                      \
        else {                                                                 \
            n00b_string_t *s = y;                                              \
            printf("%s:\n%s\n", x, n00b_hex_dump(s->data, s->byte_len)->data); \
        }                                                                      \
    }
#else
#define n00b_iodb(x, y)
#define n00b_iodb_hex(x, y)
#endif
#endif
