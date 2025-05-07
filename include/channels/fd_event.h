#pragma once
#include "n00b.h"

#define N00B_FD_CLOSED             -1
#define N00B_SOCKET_LINGER_SEC     5
#define N00B_DEFAULT_POLLSET_SLOTS 32

typedef enum {
    N00B_FD_SUB_READ,
    N00B_FD_SUB_QUEUED_WRITE,
    N00B_FD_SUB_SENT_WRITE,
    N00B_FD_SUB_CLOSE,
    N00B_FD_SUB_ERROR,
} n00b_fd_sub_kind;

typedef enum {
    N00B_EV_POLL,
} n00b_event_impl_kind;

typedef struct n00b_fd_sub_t     n00b_fd_sub_t;
typedef struct n00b_event_loop_t n00b_event_loop_t;
typedef struct n00b_fd_stream_t  n00b_fd_stream_t;
typedef struct n00b_fd_err_t     n00b_fd_err_t;
typedef struct n00b_timer_t      n00b_timer_t;
;

typedef void (*n00b_ev_ready_cb)(n00b_fd_stream_t *,
                                 bool);
typedef void (*n00b_fd_cb)(n00b_fd_stream_t *,
                           n00b_fd_sub_t *,
                           n00b_buf_t *,
                           void *);
typedef void (*n00b_err_cb)(n00b_fd_err_t *, void *);
typedef void (*n00b_close_cb)(n00b_fd_stream_t *, void *);
typedef void (*n00b_timer_cb)(n00b_timer_t *, n00b_duration_t *, void *);
typedef void (*n00b_evloop_impl)(n00b_event_loop_t *l);

struct n00b_fd_stream_t {
    int                      fd;
    _Atomic(n00b_thread_t *) worker;
    n00b_event_loop_t       *evloop;
    n00b_list_t             *read_subs;
    n00b_list_t             *queued_subs;
    n00b_list_t             *sent_subs;
    n00b_list_t             *close_subs;
    n00b_list_t             *err_subs;
    n00b_list_t             *write_queue;
    unsigned int             socket           : 1;
    unsigned int             read_closed      : 1;
    unsigned int             write_closed     : 1;
    unsigned int             newly_added      : 1;
    unsigned int             listener         : 1;
    unsigned int             needs_r          : 1;
    unsigned int             r_added          : 1;
    unsigned int             w_added          : 1;
    // This is set externally if the dispatcher should not do the r/w
    // itself for a stream.  In that case, you probably set the notify
    // callback.
    //
    // This flag causes read_ready and/or write_ready to get set, and
    // then no further action happens until the flags are cleared.
    unsigned int             no_dispatcher_rw : 1;
    unsigned int             read_ready       : 1;
    unsigned int             write_ready      : 1;
    unsigned int             plain_file       : 1;
    unsigned int             tty              : 1;

    int              fd_mode;
    int              fd_flags;
    int              internal_ix;
    int64_t          total_read;
    int64_t          total_written;
    n00b_ev_ready_cb notify;
    n00b_string_t   *name;
};

struct n00b_fd_sub_t {
    void            *thunk;
    bool             oneshot;
    n00b_fd_sub_kind kind;
    union {
        n00b_err_cb   err;
        n00b_close_cb close;
        n00b_fd_cb    msg;
    } action;
};

struct n00b_fd_err_t {
    n00b_fd_stream_t *stream;
    bool              fatal;
    bool              write;
    int               err_code;
    n00b_buf_t       *msg;
};

typedef struct {
    char *start;
    char *cur;
    int   remaining;
} n00b_wq_item_t;

typedef struct n00b_rdbuf_t n00b_rdbuf_t;

struct n00b_rdbuf_t {
    n00b_rdbuf_t *next;
    int           len;
    char          segment[PIPE_BUF];
};

typedef struct {
    struct pollfd     *pollset;
    n00b_fd_stream_t **monitored_fds;
    int                pollset_alloc;
    int                pollset_last;
    int                poll_interval;
    n00b_list_t       *empty_slots;
    // The EOF list is a list of files we're monitoring for read where
    // we need to poll manually to see when they're really ready to read.
    n00b_list_t       *eof_list;
    int                ops_in_pollset;
} n00b_pevent_loop_t;

struct n00b_timer_t {
    void              *thunk;
    n00b_duration_t   *stop_time;
    n00b_timer_cb      action;
    n00b_event_loop_t *loop;
    void (*pre_poll_callback)(n00b_event_loop_t *);
};

struct n00b_event_loop_t {
    n00b_list_t             *timers;
    n00b_event_impl_kind     kind;
    n00b_list_t             *pending;
    n00b_duration_t         *stop_time;
    int64_t                  heap_key;
    bool                     exit_loop;
    _Atomic(n00b_thread_t *) owner;
    _Atomic(n00b_list_t *)   conditions;

    union {
        n00b_pevent_loop_t poll;
    } algo;
};

typedef struct n00b_fd_cache_entry_t {
    _Atomic(n00b_fd_stream_t *) fd_info;
} n00b_fd_cache_entry_t;

extern n00b_event_loop_t *n00b_system_dispatcher;
extern bool               n00b_fd_set_absolute_position(n00b_fd_stream_t *,
                                                        int);
extern bool               n00b_fd_set_relative_position(n00b_fd_stream_t *,
                                                        int);
extern int                n00b_fd_get_position(n00b_fd_stream_t *);
extern int                n00b_fd_get_size(n00b_fd_stream_t *);
extern n00b_string_t     *n00b_fd_name(n00b_fd_stream_t *);
extern n00b_fd_stream_t  *n00b_fd_stream_from_fd(int                fd,
                                                 n00b_event_loop_t *dispatcher,
                                                 n00b_ev_ready_cb   notifier);
extern n00b_fd_sub_t     *_n00b_fd_read_subscribe(n00b_fd_stream_t *,
                                                  n00b_fd_cb,
                                                  ...);
extern n00b_fd_sub_t     *_n00b_fd_enqueue_subscribe(n00b_fd_stream_t *,
                                                     n00b_fd_cb,
                                                     ...);
extern n00b_fd_sub_t     *_n00b_fd_write_subscribe(n00b_fd_stream_t *,
                                                   n00b_fd_cb,
                                                   ...);
extern n00b_fd_sub_t     *_n00b_fd_close_subscribe(n00b_fd_stream_t *,
                                                   n00b_close_cb,
                                                   ...);
extern n00b_fd_sub_t     *_n00b_fd_error_subscribe(n00b_fd_stream_t *,
                                                   n00b_err_cb,
                                                   ...);
extern bool               n00b_fd_unsubscribe(n00b_fd_stream_t *,
                                              n00b_fd_sub_t *);
extern bool               n00b_fd_send(n00b_fd_stream_t *, char *, int);
extern bool               n00b_fd_write(n00b_fd_stream_t *, char *, int);
extern n00b_buf_t        *n00b_fd_read(n00b_fd_stream_t *, int, int, bool);
extern bool               n00b_fd_run_evloop_once(n00b_event_loop_t *);
extern bool               n00b_fd_run_evloop(n00b_event_loop_t *,
                                             n00b_duration_t *);
extern n00b_event_loop_t *n00b_new_event_context(n00b_event_impl_kind);
extern n00b_timer_t      *_n00b_add_timer(n00b_duration_t *,
                                          n00b_timer_cb,
                                          ...);
extern void               n00b_remove_timer(n00b_timer_t *);

static inline bool
n00b_fd_is_regular_file(n00b_fd_stream_t *s)
{
    return S_ISREG(s->fd_mode);
}

static inline bool
n00b_fd_is_directory(n00b_fd_stream_t *s)
{
    return S_ISDIR(s->fd_mode);
}

static inline bool
n00b_fd_is_link(n00b_fd_stream_t *s)
{
    return S_ISLNK(s->fd_mode);
}

static inline bool
n00b_fd_is_other(n00b_fd_stream_t *s)
{
    return (!(n00b_fd_is_regular_file(s) || n00b_fd_is_directory(s)
              || n00b_fd_is_link(s)));
}

#ifdef N00B_USE_INTERNAL_API
typedef bool (*n00b_condition_test_fn)(void *);
extern bool n00b_condition_poll(n00b_condition_test_fn, void *, int to);
#endif

#define n00b_add_timer(time, action, ...) \
    _n00b_add_timer(time,                 \
                    action,               \
                    N00B_PP_NARG(__VA_ARGS__) __VA_OPT__(, ) __VA_ARGS__)
