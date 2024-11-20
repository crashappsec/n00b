#pragma once

#include "n00b.h"

typedef struct n00b_sb_party_t n00b_sb_party_t;
// Function type for the subscription callback.
// First argument is the subscriber that is receving the message.
// The second is the message.
// The third is the party that sent the message.
typedef void (*n00b_recv_cb)(n00b_sb_party_t *,
                             void *,
                             n00b_sb_party_t *);

typedef enum {
    n00b_sb_source           = 1,
    n00b_sb_sink             = 2,
    n00b_sb_rw               = n00b_sb_source | n00b_sb_sink,
    n00b_sb_subable_fd       = 1 << 2,
    n00b_sb_subable_socket   = 2 << 2,
    n00b_sb_subable_buffer   = 3 << 2,
    n00b_sb_subable_callback = 4 << 2,
    n00b_sb_signal           = 5 << 2,
} n00b_sb_subable_kind;

enum {
    n00b_sb_str_pass_u8      = 1,
    n00b_sb_str_pass_u32     = 2,
    n00b_sb_n00b_object      = 4,
    // The below are ignored if objects are being passed.
    n00b_sb_str_ansi_tty     = 8,
    n00b_sb_str_ansi_socket  = 16,
    n00b_sb_str_ansi_default = 32,
} n00b_sb_munge_policy;

enum {
    n00b_sb_cache_read  = 1,
    n00b_sb_cache_write = 2,
    n00b_sb_cache_all   = 3,
} n00b_sb_cache_policy;

typedef struct {
    n00b_buf_t *buf;
    int32_t     read_offset;
    int32_t     write_offset;
} n00b_sb_buffer_t;

// A subscribable object.
struct n00b_sb_party_t {
    struct event *source_event; // object is ready for read.
    struct event *sink_event;   // writes.
    n00b_list_t  *source_subs;  // Subscribes on read() calls
    n00b_list_t  *sink_subs;    // Subscribes on write() calls
    void         *thunk;        // Extra data for callbacks.

    n00b_sb_buffer_t   *read_cache;
    n00b_sb_buffer_t   *write_cache;
    n00b_list_t        *pending_writes;
    n00b_switchboard_t *my_switchboard;

    union {
        int             fd;
        evutil_socket_t socket;
        n00b_recv_cb    cb;
        n00b_buf_t     *buf;
        n00b_str_t     *str;
    } target;

    n00b_sb_subable_kind kind;
    n00b_sb_munge_policy munge_policy;
    n00b_sb_cache_policy cache_policy;
    bool                 closed;
    bool                 is_tty;
} n00b_sb_party_t;

typedef struct {
    struct event_base *event_ctx;
    struct event      *timeout_event;
    bool               signal_handler;
    bool               files_okay;
    bool               got_timeout;
    bool               timeout_inited;
} n00b_switchboard_t;

// Configures the libevent memory manager to be us.
static pthread_once_t      system_sb_inited   = PTHREAD_ONCE_INIT;
static n00b_switchboard_t *signal_switchboard = NULL;

static inline bool
n00b_sb_party_is_fd(n00b_sb_party_t *party)
{
    n00b_sb_subable_kind k = party->kind & (n00b_sb_rw);

    switch (k) {
    case n00b_sb_subable_fd:
    case n00b_sb_subable_socket:
        return true;
    default:
        return false;
    }
}

static inline bool
n00b_sb_party_is_socket(n00b_sb_party_t *party)
{
    n00b_sb_subable_kind k = party->kind & (n00b_sb_rw);

    return k == n00b_sb_subable_socket;
}

static inline bool
n00b_sb_party_is_tty(n00b_sb_party_t *party)
{
    return party->is_tty;
}

static inline void
n00b_sb_set_munge_policy(n00b_sb_party_t *party, n00b_sb_munge_policy policy)
{
    // The munge policy designates what we expect to "transmit". That is,
    // when something uses the party as a source, what should come out?
    //
    // Any time in the API when we write to a sink, we transform using
    // the munge policy.
    //
    // If you use N00b rich strings over a socket, then the other side
    // must know how to unmarshal them.

    party->munge_policy = policy;
}

static inline void
n00b_sb_set_cache_policy(n00b_sb_party_t *party, n00b_sb_cache_policy policy)
{
    party->cache_policy = policy;
}

static inline bool
n00b_sb_run_once(n00b_soundboard_t *sb, n00b_duration_t *timeout)
{
    sb_setup_timeout(sb, timeout);

    event_base_loop(sb->event_ctx, EVLOOP_ONCE);

    return !(sb->got_timeout);
}

static inline void
n00b_sb_loop_start(n00b_soundboard_t *sb)
{
    event_base_dispatch(sb->event_ctx);
}

static inline void
n00b_sb_loop_break(n00b_soundboard_t *sb)
{
    event_base_loopbreak(sb->event_ctx);
}

static inline void
n00b_sb_loop_exit(n00b_soundboard_t *sb)
{
    event_base_loopexit(sb->event_ctx, NULL);
}
