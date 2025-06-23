#pragma once

#include "n00b.h"

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
extern bool              n00b_condition_poll(n00b_condition_test_fn,
                                             void *,
                                             int64_t to);
extern n00b_fd_stream_t *n00b_fd_cache_lookup(int, n00b_event_loop_t *);
extern n00b_fd_stream_t *n00b_fd_cache_add(n00b_fd_stream_t *);
extern void              n00b_fd_post(n00b_fd_stream_t *,
                                      n00b_list_t *,
                                      void *);
extern void              n00b_fd_post_error(n00b_fd_stream_t *,
                                            n00b_wq_item_t *,
                                            int,
                                            bool,
                                            bool);
extern void              n00b_fd_post_close(n00b_fd_stream_t *);
extern bool              n00b_handle_one_read(n00b_fd_stream_t *);
extern bool              n00b_handle_one_write(n00b_fd_stream_t *);
extern void              n00b_end_system_io(void);

extern n00b_dict_t    *n00b_fd_cache;
extern /* once */ void n00b_fd_init_io(void);

// We don't expect a lot of contention, so we just use simple spin
// locks.

static inline void
n00b_fd_become_worker(n00b_fd_stream_t *s)
{
    n00b_thread_t *expected = NULL;
    n00b_thread_t *me       = n00b_thread_self();

    n00b_barrier();

    while (!n00b_cas(&s->worker, &expected, me)) {
        // Don't break the spin lock.
        if (expected == me) {
            break;
        }
        expected = NULL;
    }

    n00b_assert(atomic_read(&s->worker) == n00b_thread_self());
}

static inline void
n00b_fd_worker_yield(n00b_fd_stream_t *s)
{
    atomic_store(&s->worker, NULL);
}

static inline void
n00b_fd_stream_nonblocking(n00b_fd_stream_t *s)
{
    fcntl(s->fd, F_SETFL, fcntl(s->fd, F_GETFL) | O_NONBLOCK);
}

static inline void
n00b_fd_stream_blocking(n00b_fd_stream_t *s)
{
    fcntl(s->fd, F_SETFL, fcntl(s->fd, F_GETFL) & ~O_NONBLOCK);
}
static inline void
n00b_fd_discovered_read_close(n00b_fd_stream_t *s)
{
    s->read_closed = true;
    if (s->write_closed) {
        n00b_fd_post_close(s);
    }
}

static inline bool
n00b_fd_discovered_write_close(n00b_fd_stream_t *s)
{
    s->write_closed = true;
    if (s->read_closed) {
        n00b_fd_post_close(s);
        return true;
    }
    return false;
}

#endif
