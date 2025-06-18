#define N00B_USE_INTERNAL_API
#include "n00b.h"

// This is a LOW-LEVEL interface. The callbacks are expected to return
// quickly, so should not be user callbacks.

n00b_event_loop_t *n00b_system_dispatcher = NULL;
n00b_condition_t   n00b_io_exit_request;
bool               n00b_io_exited = false;

void once
n00b_fd_init_io(void)
{
    n00b_named_lock_init(&n00b_io_exit_request, N00B_NLT_CV, "exit");
    n00b_gc_register_root(&n00b_system_dispatcher, 1);
    n00b_gc_register_root(&n00b_fd_cache, 1);
    n00b_fd_cache          = n00b_dict(n00b_type_int(), n00b_type_ref());
    n00b_system_dispatcher = n00b_new_event_context(N00B_EV_POLL);
    n00b_setup_terminal_streams();
}

void
n00b_end_system_io(void)
{
    n00b_io_exited = true;
    n00b_barrier();
    n00b_condition_notify_all(&n00b_io_exit_request);
    n00b_lock_release(&n00b_io_exit_request);
}

typedef struct {
    n00b_condition_test_fn fn;
    void                  *thunk;
    n00b_condition_t      *condition;
} cond_info_t;

static void
process_conditions(n00b_event_loop_t *ctx)
{
    n00b_list_t *old = atomic_read(&ctx->conditions);
    if (!n00b_list_len(old)) {
        return;
    }

    // We *should* be the only thread changing out the list.
    n00b_list_t *new_list = n00b_list(n00b_type_ref());

    while (!(CAS(&ctx->conditions, &old, new_list))) {
        // Nothing.
    }

    cond_info_t *info = n00b_list_pop(old);

    while (info) {
        if ((*info->fn)(info->thunk)) {
            n00b_lock_acquire(info->condition);
            n00b_condition_notify_one(info->condition);
            n00b_lock_release(info->condition);
        }
        n00b_list_append(new_list, info);
    }
    info = n00b_list_pop(old);
}

bool
n00b_condition_poll(n00b_condition_test_fn fn, void *thunk, int64_t to_ns)
{
    cond_info_t *info = n00b_gc_alloc_mapped(cond_info_t, N00B_GC_SCAN_ALL);
    info->fn          = fn;
    info->thunk       = thunk;
    info->condition   = n00b_new(n00b_type_condition());

    n00b_lock_acquire(info->condition);

    n00b_list_t *dl = atomic_read(&n00b_system_dispatcher->conditions);
    n00b_list_append(dl, info);

    return !n00b_condition_wait(info->condition, timeout : to_ns);
}

static inline void
check_timers(n00b_event_loop_t *loop,
             n00b_duration_t   *now)
{
    if (!loop->timers) {
        return;
    }
    int n = n00b_list_len(loop->timers);

    while (n--) {
        n00b_timer_t *t = n00b_private_list_get(loop->timers, n, NULL);

        if (!t) {
            // Could have been removed in a race condition.
            continue;
        }

        if (n00b_duration_lt(t->stop_time, now)) {
            n00b_list_remove_item(loop->timers, t);
            (*t->action)(t, now, t->thunk);
        }
    }
}

static inline void
add_to_pollset(n00b_pevent_loop_t *ploop, n00b_fd_stream_t *s)
{
    // We actually keep two entries for each FD we are polling at
    // once, one we can send directly to poll(), and a pointer to the
    // n00b_fd_stream_t * object.
    //
    // When we run out of empty slots, we need to grow and copy both
    // lists.
    //
    // We will never shrink these allocations.
    int64_t slot;

    if (ploop->empty_slots && n00b_list_len(ploop->empty_slots)) {
        slot = (int64_t)n00b_private_list_pop(ploop->empty_slots);
    }
    else {
        if (ploop->pollset_alloc == ploop->pollset_last) {
            int os               = ploop->pollset_alloc;
            int ns               = os << 1;
            ploop->pollset_alloc = ns;
            struct pollfd *new   = n00b_gc_array_value_alloc(struct pollfd, ns);
            memcpy(new, ploop->pollset, sizeof(struct pollfd) * os);
            ploop->pollset = new;

            n00b_fd_stream_t **fdlist = n00b_gc_array_alloc(void *, ns);
            memcpy(fdlist, ploop->monitored_fds, sizeof(void *) * os);
            ploop->monitored_fds = fdlist;
        }
        slot = ploop->pollset_last++;
    }

    s->internal_ix                       = slot;
    ploop->pollset[s->internal_ix].fd    = s->fd;
    ploop->monitored_fds[s->internal_ix] = s;
}

static inline void
process_pending_changes(n00b_event_loop_t *loop, n00b_pevent_loop_t *ploop)
{
    n00b_fd_stream_t *s = n00b_private_list_pop(loop->pending);

    while (s) {
        if (s->newly_added) {
            add_to_pollset(ploop, s);
            s->newly_added = false;
        }

        struct pollfd *entry = &ploop->pollset[s->internal_ix];

        if (s->needs_r) {
            s->r_added = true;

            if (s->fd != 1 && s->fd != 2 && !(entry->events & POLLIN)) {
                entry->events |= POLLIN;
                ploop->ops_in_pollset++;
            }
            s->needs_r = false;
        }

        if (!s->w_added && !s->write_ready && n00b_list_len(s->write_queue)) {
            s->w_added = true;
            entry->events |= POLLOUT;
            ploop->ops_in_pollset++;
        }

        if (s->fd == 0) {
            assert(entry->events & POLLIN);
        }

        s = n00b_private_list_pop(loop->pending);
    }
}

static inline void
process_pset(n00b_event_loop_t *loop, n00b_pevent_loop_t *ploop)
{
    int                n         = ploop->pollset_last;
    struct pollfd     *slot_list = ploop->pollset;
    n00b_thread_t     *self      = n00b_thread_self();
    n00b_thread_t     *worker;
    n00b_fd_stream_t **fd_ptrs = ploop->monitored_fds;
    n00b_fd_stream_t  *s;

    for (int i = 0; i < n; i++) {
        s                   = fd_ptrs[i];
        worker              = NULL;
        struct pollfd *slot = slot_list + i;

        if (!s || atomic_read(&s->evloop->owner) != self) {
            continue;
        }
        if (!CAS(&s->worker, &worker, self)) {
            // Someone's either adding to it or doing their own r/w
            // so leave it alone until the next polling cycle.
            continue;
        }

        if (slot->revents & POLLIN) {
            if (s->no_dispatcher_rw) {
                s->read_ready = true;
                if (s->notify) {
                    (*s->notify)(s, true);
                }
                slot->events &= ~POLLIN;
                s->r_added = false;
                ploop->ops_in_pollset--;
            }
            else {
                if (n00b_handle_one_read(s)) {
                    n00b_dlog_io("Removing fd %d from poll set", s->fd);
                    slot->events &= ~POLLIN;
                    s->r_added = false;
                    ploop->ops_in_pollset--;

                    if (!s->socket) {
                        n00b_list_append(ploop->eof_list, s);
                        n00b_dlog_io("Adding fd %d to EOF list", s->fd);
                    }
                }
            }
        }
        else {
            if (s->closing) {
                n00b_fd_discovered_read_close(s);
            }
        }

        if (slot->revents & POLLOUT) {
            if (s->no_dispatcher_rw) {
                s->write_ready = true;
                if (s->notify) {
                    (*s->notify)(s, false);
                }
                slot->events &= ~POLLOUT;
                s->w_added = false;
                ploop->ops_in_pollset--;
            }
            else {
                if (n00b_handle_one_write(s)) {
                    slot->events &= ~POLLOUT;
                    s->w_added = false;
                    ploop->ops_in_pollset--;
                }
            }
        }
        if (slot->revents & POLLHUP) {
            // Might still have reading to do; but consider the fd
            // closed for writes in any circumstances.
            //
            // And if nothing is subscribed for reads, go ahead and
            // close the read side as well.

            if (!n00b_fd_discovered_write_close(s)) {
                if (!(slot->events & POLLIN)) {
                    n00b_fd_discovered_read_close(s);
                }
                else {
                    s->closing = true;
                }
            }
            else {
                slot->fd     = -1;
                slot->events = 0;
                n00b_fd_discovered_read_close(s);
            }
        }

        // Take back closed slots.
        if (s->fd == N00B_FD_CLOSED && s->r_added) {
            slot = ploop->pollset + s->internal_ix;

            fd_ptrs[s->internal_ix] = NULL;
            n00b_list_append(ploop->empty_slots, (void *)(int64_t)i);
            s->read_closed  = true;
            s->write_closed = true;
        }

        // Keep our pollcount up to date.
        if (s->read_closed && s->r_added) {
            ploop->ops_in_pollset--;
            s->r_added = false;
        }
        if (s->write_closed && s->w_added) {
            ploop->ops_in_pollset--;
            s->w_added = false;
        }

        atomic_store(&s->worker, NULL);
    }
}

static inline void
check_eof_list(n00b_event_loop_t *loop, n00b_pevent_loop_t *ploop)
{
    int n = n00b_list_len(ploop->eof_list);

    while (n--) {
        n00b_fd_stream_t *f = n00b_list_get(ploop->eof_list, n, NULL);
        if (n00b_fd_get_position(f) != n00b_fd_get_size(f)) {
            f->needs_r = true;
            n00b_list_remove(ploop->eof_list, n);
            n00b_list_append(loop->pending, f);
        }
    }
}

static bool
n00b_fd_run_poll_dispatcher_once(n00b_event_loop_t *loop,
                                 n00b_duration_t   *now)
{
    // We only register FDs if we need to; when we read / write, we do
    // so until we get EAGAIN. And if some other thread registers to do the
    // IO, then we don't re-add until we're told things are drained.
    n00b_pevent_loop_t *ploop = &loop->algo.poll;
    int                 wait  = N00B_POLL_DEFAULT_MS;

    check_timers(loop, now);
    check_eof_list(loop, ploop);
    process_conditions(loop);

    process_pending_changes(loop, ploop);

    if (!ploop->ops_in_pollset) {
        // If there are no fds registered, wait the whole polling
        // interval, then poll w/o blocking.

        // 100000 ns in a ms
        n00b_nanosleep(0, N00B_POLL_DEFAULT_MS * 1000000);
        process_pending_changes(loop, ploop);
        wait = 0;
    }

    N00B_DBG_CALL(n00b_thread_suspend);
    int val = poll(ploop->pollset, ploop->pollset_last, wait);
    N00B_DBG_CALL(n00b_thread_resume);

    if (val < 0) {
        return false;
    }

    process_pset(loop, ploop);

    return true;
}

static inline void
loop_exit_check(n00b_event_loop_t *loop, n00b_duration_t *now)
{
    if (n00b_condition_has_waiters(&n00b_io_exit_request)) {
        loop->exit_loop = true;
        return;
    }

    if (!loop->stop_time) {
        return;
    }
    if (n00b_duration_lt(loop->stop_time, now)) {
        loop->exit_loop = true;
    }
}

bool
n00b_fd_run_evloop_once(n00b_event_loop_t *loop)
{
    n00b_duration_t now;

    n00b_write_now(&now);
    return n00b_fd_run_poll_dispatcher_once(loop, &now);
}

bool
n00b_fd_run_evloop(n00b_event_loop_t *loop, n00b_duration_t *howlong)
{
    n00b_duration_t now;
    n00b_thread_t  *t        = n00b_thread_self();
    n00b_thread_t  *expected = NULL;
    n00b_tsi_t     *tsi      = t->tsi;

    tsi->system_thread = true;

    if (howlong) {
        n00b_write_now(&now);
        loop->stop_time = n00b_duration_add(&now, howlong);
    }
    else {
        loop->stop_time = NULL;
    }

    if (!CAS(&loop->owner, &expected, t)) {
        tsi->system_thread = false;
        return false;
    }

    // Always run at least once.
    do {
        n00b_write_now(&now);
        n00b_fd_run_poll_dispatcher_once(loop, &now);
        loop_exit_check(loop, &now);
    } while (!loop->exit_loop);

    n00b_end_system_io();
    loop->exit_loop = false;
    atomic_store(&loop->owner, NULL);

    tsi->system_thread = false;
    return true;
}

static void
new_poll_event_context(n00b_event_loop_t *ctx)
{
    n00b_pevent_loop_t *ploop = &ctx->algo.poll;
    ploop->pollset            = n00b_gc_array_value_alloc(struct pollfd,
                                               N00B_DEFAULT_POLLSET_SLOTS);
    ploop->monitored_fds      = n00b_gc_array_alloc(void *,
                                               N00B_DEFAULT_POLLSET_SLOTS);
    ploop->pollset_alloc      = N00B_DEFAULT_POLLSET_SLOTS;
    ploop->poll_interval      = N00B_POLL_DEFAULT_MS;
    ploop->empty_slots        = n00b_list(n00b_type_int());
    ploop->eof_list           = n00b_list(n00b_type_ref());
}

static void (*const event_impls[])(n00b_event_loop_t *) = {
    new_poll_event_context,
};

n00b_event_loop_t *
n00b_new_event_context(n00b_event_impl_kind kind)
{
    int len = sizeof(n00b_event_loop_t);
    len     = n00b_round_up_to_given_power_of_2(n00b_page_bytes, len);

    n00b_event_loop_t *ctx = mmap(NULL,
                                  len,
                                  PROT_READ | PROT_WRITE,
                                  MAP_PRIVATE | MAP_ANON,
                                  -1,
                                  0);

    n00b_gc_register_root(ctx, len / 8);

    ctx->kind    = kind;
    ctx->timers  = n00b_list(n00b_type_ref());
    ctx->pending = n00b_list(n00b_type_ref());
    atomic_store(&ctx->conditions, n00b_list(n00b_type_ref()));

    (*event_impls[kind])(ctx);

    return ctx;
}
