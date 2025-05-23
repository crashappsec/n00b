#define N00B_USE_INTERNAL_API
#include "n00b.h"

// TODO (nothing urgent):
// 1) Reopen.
// 2) procfd
// 5) Seek / tell aka get_location / set_location
// 6) fileno
// 7) Compress migration
// 8) add_to_backlog a polymorphic n00b_lock() and use it when automarshaling.
// 9) Either remove open_impl from the list, or come up w/ a good generic
//     interface for the needed parameters.
// 10) Buffer needs to use the write cursor instead of always
//     appending.
// 11) Add timeout back to proc.
// 12) Rename stuff back to marshal / stream.

// Iocore intrinsically knows about libevent, but still lets the
// different event types be managed as separate impls, which
// makes sense given semantics are different for timers, signals, etc.

static n00b_dict_t     *impls;
static n00b_thread_t   *n00b_io_thread;
static n00b_notifier_t *exit_notifier            = NULL;
bool                    n00b_io_has_exited       = false;
static bool             raise_on_uncaught_errors = true;
static struct event    *loop_timeout;
static n00b_lock_t      event_loop_lock;
n00b_stream_base_t     *n00b_system_event_base = NULL;
static pthread_once_t   io_roots               = PTHREAD_ONCE_INIT;

#define exit_requested() exit_notifier != NULL

bool
n00b_in_io_thread(void)
{
    return n00b_io_thread->pthread_id == pthread_self();
}

static void
n00b_io_impl_register(n00b_string_t *name, n00b_io_impl_info_t *info)
{
    info->name = name;

    if (info->use_libevent) {
        info->cookie = (void *)n00b_system_event_base;
    }

    n00b_string_t *s = n00b_string_copy(name);

    if (!hatrack_dict_add(impls, s, info)) {
        N00B_CRAISE("Implementation already exists.");
    }
}

void
n00b_post_close(n00b_stream_t *event)
{
    if (event->close_notified) {
        return;
    }

    if (event->impl->use_libevent) {
        n00b_ev2_cookie_t *c = event->cookie;

        event->close_notified = true;

        if (c->write_event) {
            n00b_event_del(c->write_event);
            c->write_event = NULL;
        }

        if (c->read_event) {
            n00b_event_del(c->read_event);
            c->read_event = NULL;
        }
    }

    if (n00b_stream_has_remaining_subscribers(event, n00b_io_sk_close)) {
        n00b_post_to_subscribers(event, (void *)event, n00b_io_sk_close);
    }

    event->read_subs        = NULL;
    event->write_subs       = NULL;
    event->write_start_subs = NULL;
}

void
n00b_initialize_event(n00b_stream_t       *event,
                      n00b_io_impl_info_t *impl,
                      n00b_io_permission_t perms,
                      n00b_io_event_type   et,
                      void                *cookie)
{
    event->impl   = impl;
    event->cookie = cookie;
    event->perms  = (unsigned int)perms;
    event->etype  = et;

    event->close_subs = n00b_dict(n00b_type_stream(), n00b_type_ref());
    event->error_subs = n00b_dict(n00b_type_stream(), n00b_type_ref());

    if (event->perms & n00b_io_perm_r) {
        event->read_filters = n00b_list(n00b_type_ref());
        event->read_subs    = n00b_dict(n00b_type_stream(),
                                     n00b_type_ref());
    }
    if (event->perms & n00b_io_perm_w) {
        event->write_filters    = n00b_list(n00b_type_ref());
        event->write_start_subs = n00b_dict(n00b_type_stream(),
                                            n00b_type_ref());
        event->write_subs       = n00b_dict(n00b_type_stream(),
                                      n00b_type_ref());
    }

    n00b_assert(event->perms & (n00b_io_perm_r | n00b_io_perm_w));

    n00b_named_lock_init(&event->lock, "event lock");
}

static void
n00b_stream_init(n00b_stream_t *event, va_list args)
{
    n00b_io_impl_info_t *impl   = va_arg(args, n00b_io_impl_info_t *);
    void                *cookie = va_arg(args, void *);
    n00b_io_permission_t perms  = va_arg(args, n00b_io_permission_t);
    n00b_io_event_type   et     = va_arg(args, n00b_io_event_type);

    n00b_initialize_event(event, impl, perms, et, cookie);
}

n00b_string_t *
n00b_stream_repr(n00b_stream_t *e)
{
    n00b_string_t *name;

    if (e->repr) {
        return e->repr;
    }

    if (e->impl && e->impl->repr_impl) {
        n00b_io_repr_fn fn = e->impl->repr_impl;
        name               = (*fn)(e);
    }
    else {
        int64_t id        = *(int64_t *)e->cookie;
        char    buf[1024] = {
            0,
        };

        snprintf(buf, 1023, "%llu (%s)", (long long int)id, e->impl->name->data);

        name = n00b_cstring(buf);
    }

    char read  = e->closed_for_read ? '-' : '+';
    char write = e->closed_for_write ? '-' : '+';
    return n00b_cformat("«#» «#»r «#»w\n",
                        name,
                        n00b_string_from_codepoint(read),
                        n00b_string_from_codepoint(write));
}

static inline n00b_string_t *
build_filter_list(n00b_list_t *from)
{
    if (!from || !n00b_list_len(from)) {
        return n00b_crich("«i»None.");
    }
    n00b_list_t *l = n00b_list(n00b_type_string());
    int          n = n00b_list_len(from);
    for (int i = 0; i < n; i++) {
        n00b_stream_filter_t *f = n00b_list_get(from, i, NULL);
        n00b_list_append(l, f->name);
    }

    return n00b_string_join(l, n00b_cached_comma_padded());
}

n00b_string_t *
n00b_stream_filter_repr(n00b_stream_t *e)
{
    n00b_string_t *result;

    result = n00b_crich("«em3»Read filters: ");
    result = n00b_string_concat(result, build_filter_list(e->read_filters));
    result = n00b_string_concat(result, n00b_crich("\n«em3»Write filters: "));
    result = n00b_string_concat(result, build_filter_list(e->write_filters));

    return result;
}

n00b_string_t *
one_subscription_repr(n00b_stream_t *s, n00b_io_subscription_kind k)
{
    n00b_dict_t *d = n00b_stream_get_subscriptions(s, k);

    if (!d) {
        return NULL;
    }
    n00b_list_t *subs = n00b_dict_keys(d);
    n00b_list_t *l    = n00b_list(n00b_type_string());
    int          n    = n00b_list_len(subs);

    if (!n) {
        return NULL;
    }

    for (int i = 0; i < n; i++) {
        n00b_stream_t *sink = n00b_list_get(subs, i, NULL);
        n00b_list_append(l,
                         n00b_string_concat(n00b_cached_newline(),
                                            n00b_stream_repr(sink)));
    }

    char *cname;

    switch (k) {
    case n00b_io_sk_read:
        cname = "Read";
        break;
    case n00b_io_sk_pre_write:
        cname = "Pre-write";
        break;
    case n00b_io_sk_post_write:
        cname = "Post-write";
        break;
    case n00b_io_sk_error:
        cname = "Err";
        break;
    default:
        cname = "Close";
        break;
    }

    return n00b_cformat("«em6»«#» subscriptions:«#»\n«#»\n",
                        n00b_cstring(cname),
                        n00b_string_join(l, n00b_cstring("\n  ")));
}

n00b_string_t *
n00b_stream_subs_repr(n00b_stream_t *e)
{
    n00b_string_t *r = NULL;
    n00b_string_t *t;

    for (int i = n00b_io_sk_read; i <= n00b_io_sk_close; i++) {
        t = one_subscription_repr(e, i);
        if (!r) {
            r = t;
        }
        else {
            if (t) {
                r = n00b_string_concat(r, t);
            }
        }
    }

    if (!r) {
        return n00b_crich("«em6»No subscriptions.\n");
    }

    return r;
}

n00b_string_t *
n00b_stream_full_repr(n00b_stream_t *s)
{
    n00b_string_t *result = n00b_stream_repr(s);
    result                = n00b_string_concat(result, n00b_cached_newline());
    result                = n00b_string_concat(result,
                                n00b_stream_subs_repr(s));
    result                = n00b_string_concat(result,
                                n00b_stream_filter_repr(s));
    result                = n00b_string_concat(result,
                                n00b_crich("\n«em5»--------------------«/»\n"));

    return result;
}

bool
n00b_at_eof(n00b_stream_t *e)
{
    if (e->closed_for_read) {
        return true;
    }

    if (!e->impl || !e->impl->eof_impl) {
        return false;
    }

    return (*e->impl->eof_impl)(e);
}

// This is used for caching party objects, so that all file
// descriptors or all signals use the same n00b_stream_t * object.  The
// caller basically creates a candidate, and we try to installed it,
// returning whatever actually got installed.
//
// Generally, the caller already failed to add something new, and
// saw something closed, but there might be a race to replace that.

n00b_stream_t *
n00b_add_or_replace(n00b_stream_t *new, n00b_dict_t *dict, void *key)
{
    defer_on();

    if (hatrack_dict_add(dict, key, new)) {
        Return new;
    }

    n00b_stream_t *found = hatrack_dict_get(dict, key, NULL);
    n00b_acquire_party(found);

    while (found->closed_for_read || found->closed_for_write) {
        n00b_stream_t *assure = hatrack_dict_get(dict, key, NULL);
        if (assure != found) {
            n00b_release_party(found);
            found = assure;
            n00b_acquire_party(found);
            continue;
        }
        else {
            hatrack_dict_put(dict, key, new);
            Return new;
        }
    }

    Return found;

    defer_func_end();
}

static inline struct timeval *
get_event_start_time(n00b_stream_sub_t *sub)
{
    if (!sub->timeout) {
        return NULL;
    }

    struct timeval *start = sub->subscription_time;

    if (start) {
        if (!sub->source->read_start_time) {
            sub->source->read_start_time = start;
            sub->subscription_time       = NULL;
        }
    }
    else {
        start = sub->source->read_start_time;
    }

    return start;
}

// When subscription timeouts fire, they come here. They first check
// to make sure there wasn't a race condition. But if the timeout
// still holds, their only job is to remove themselves.
//
// This only happens on read events. It's measured from the first
// subscription, OR from the previous read when there's been a read.
static void
timeout_purge(evutil_socket_t fd, short event_type, void *info)
{
    defer_on();

    n00b_stream_sub_t *sub = info;

    n00b_acquire_party(sub->source);
    n00b_acquire_party(sub->sink);

    struct timeval *start = get_event_start_time(sub);
    struct timeval *now   = n00b_duration_to_timeval(n00b_now());
    struct timeval *duration;

    duration = (struct timeval *)n00b_duration_diff((n00b_duration_t *)now,
                                                    (n00b_duration_t *)start);

    if (n00b_duration_gt((n00b_duration_t *)duration,
                         (n00b_duration_t *)sub->timeout)) {
        n00b_io_unsubscribe(info);
    }

    Return;
    defer_func_end();
}

static inline n00b_stream_base_t *
n00b_new_base(void)
{
    n00b_stream_base_t *result = n00b_gc_alloc_mapped(n00b_stream_base_t,
                                                      N00B_GC_SCAN_ALL);
    n00b_static_lock_init(result->lock);

    return result;
}

// This cleans up events at the time they've considered 'fired' (when
// they complete). We keep a separate TimeOut event in to_event, and
// keep individual timers for them; re-add the timers here.

void
n00b_purge_subscription_list_on_boundary(n00b_dict_t *s)
{
    n00b_list_t *l = n00b_dict_values(s);
    int          n = n00b_list_len(l);

    for (int i = 0; i < n; i++) {
        n00b_stream_sub_t *sub = n00b_list_get(l, n, NULL);
        if (!sub) {
            continue;
        }

        if (sub->one_shot) {
            n00b_io_unsubscribe(sub);
            continue;
        }
        if (!sub->timeout) {
            continue;
        }
        n00b_event_add(sub->to_event, sub->timeout);
    }
}

bool
n00b_post_to_subscribers(n00b_stream_t            *party,
                         void                     *msg,
                         n00b_io_subscription_kind k)
{
    n00b_dict_t *d = n00b_stream_get_subscriptions(party, k);

    if (!d) {
        return false;
    }

    n00b_list_t *l = n00b_dict_keys(d);
    int          n = n00b_list_len(l);

    if (!n && party->etype == n00b_io_ev_socket && k == n00b_io_sk_read) {
        abort();
    }

    for (int i = 0; i < n; i++) {
        n00b_stream_t *sink = n00b_list_get(l, i, NULL);

        n00b_write(sink, msg);
    }

    n00b_purge_subscription_list_on_boundary(d);

    return true;
}

// End utility functions.
// Begin file impl.
static inline bool
still_can_read(int fd)
{
    struct pollfd poll_set = {
        .fd      = fd,
        .events  = POLLIN,
        .revents = 0,
    };

    return poll(&poll_set, 1, 0) == 1;
}

void
n00b_ev2_r(evutil_socket_t fd, short event_type, void *info)
{
    defer_on();
    // Because we force sockets into non-blocking mode, read() cannot
    // block (with non-sockets, read() always returns).
    //
    // Since we are not edge triggering, we keep things reasonably
    // simple by always attempting to read PIPE_BUF bytes. If we get a
    // short read, we just ignore it.

    n00b_stream_t *party = info;
    size_t         n;

    char tmp_buf[PIPE_BUF] = {
        0,
    };

    n00b_acquire_party(party);

    if (party->next_read_limit) {
        if (party->next_read_limit < PIPE_BUF) {
            n = party->next_read_limit;
        }
        else {
            n = PIPE_BUF;
        }
        party->next_read_limit = 0;
    }
    else {
        n = PIPE_BUF;
    }

read_again:;
    n00b_buf_t *buf;
    ssize_t     len = read(fd, tmp_buf, n);

    switch (len) {
    case -1:
        if (errno != EINTR && errno != EAGAIN) {
            party->error           = true;
            party->closed_for_read = true;
            n00b_post_close(party);
            n00b_post_errno(party);
        }
        Return;
    case 0:
        party->closed_for_read = true;
        n00b_post_close(party);
        Return;
    default:
#ifdef N00B_EV_STATS
    {
        c->bytes_read += len;
    }
#endif
        buf = n00b_new(n00b_type_buffer(),
                       n00b_kw("raw", &tmp_buf[0], "length", n00b_ka(len)));

#ifdef N00B_INTERNAL_DEBUG
        n00b_ev2_cookie_t *cookie = party->cookie;

        if (party->etype == n00b_io_ev_socket) {
            cprintf("\nev2(%d->r):\n%s\n",
                    (int)cookie->id,
                    n00b_hex_dump(buf->data, n00b_len(buf)));
        }
#endif

        n00b_list_t *to_post = n00b_handle_read_operation(party, buf);

        if (to_post) {
            // High-level write operations from the API post to their
            // subscribers in real-time, as calls get made.
            //
            // For reads, we wait until the impl produces
            // something to post.
            int n = n00b_list_len(to_post);

            for (int i = 0; i < n; i++) {
                void *msg = n00b_list_get(to_post, i, NULL);
                n00b_post_to_subscribers(party, msg, n00b_io_sk_read);
                n00b_purge_subscription_list_on_boundary(party->read_subs);
                n00b_ev2_post_read_cleanup(party);
            }
        }

        if (len == PIPE_BUF && still_can_read(fd)) {
            goto read_again;
        }

        Return;
    }

    defer_func_end();
}

static inline bool
signal_possible_waiter(void *obj)
{
    if (n00b_type_is_condition(n00b_get_my_type(obj))) {
        n00b_condition_lock_acquire(obj);
        n00b_condition_notify_one((n00b_condition_t *)obj);
        n00b_condition_lock_release(obj);
        return true;
    }
    return false;
}

void
n00b_io_ev_unsubscribe(n00b_stream_sub_t *info)
{
    n00b_stream_t     *source     = info->source;
    n00b_ev2_cookie_t *src_cookie = source->cookie;

    if (src_cookie->read_event && !info->source->read_active) {
        n00b_event_del(src_cookie->read_event);
    }

    if (src_cookie->write_event && !info->source->write_active) {
        n00b_event_del(src_cookie->write_event);
    }
}

bool
n00b_io_enqueue_fd_read(n00b_stream_t *party, uint64_t len)
{
    n00b_ev2_cookie_t *cookie = party->cookie;
    // This should be unnecessary; should already be queued.
    // But just in case.
    n00b_event_add(cookie->read_event, NULL);
    return true;
}

static inline void
_acquire_event_loop(char *f, int l)
{
    if (!_n00b_lock_acquire_if_unlocked(&event_loop_lock, f, l)) {
        abort();
    }
}

static inline void
release_event_loop(void)
{
    n00b_lock_release(&event_loop_lock);
}

#define acquire_event_loop()                 \
    _acquire_event_loop(__FILE__, __LINE__); \
    defer(release_event_loop())

bool
n00b_io_run_base(n00b_stream_base_t *eb, int flags)
{
    n00b_process_queue();

    // This is technically a double-lock; the libevent API locks. But
    // it also force-prints error messages when there's a conflict,
    // and we are going to let multiple threads process the queue if
    // needed.
    sigset_t saved_set, cur_set;

    sigemptyset(&cur_set);
    sigaddset(&cur_set, SIGPIPE);
    pthread_sigmask(SIG_BLOCK, &cur_set, &saved_set);

    bool result;

    struct event_base *libevent2_event_base = eb->event_ctx;
    if (!libevent2_event_base) {
        return false;
    }

    event_base_loopcontinue(libevent2_event_base);
    result = event_base_loop(libevent2_event_base, flags);

    pthread_sigmask(SIG_SETMASK, &saved_set, 0);
    n00b_gts_checkin();

    return result;
}

static void
n00b_dislikes_libevent(int severity, const char *msg)
{
    if (severity >= 1) {
        fprintf(stderr, "%d: %s\n", severity, msg);
        n00b_static_c_backtrace();
        exit(-1);
    }
}

static inline void
setup_libevent(void)
{
    event_set_mem_functions(n00b_malloc_compat,
                            n00b_realloc_compat,
                            n00b_free_compat);

    if (!n00b_system_event_base) {
        n00b_system_event_base = n00b_new_base();
    }

    event_set_log_callback(n00b_dislikes_libevent);
    event_enable_debug_mode();
    event_enable_debug_logging(EVENT_DBG_ALL);

#ifdef WIN32
    evthread_use_pthreads();
#endif
}

n00b_stream_t *
n00b_open_event(n00b_string_t *type, void *info)
{
    n00b_io_impl_info_t *impl = hatrack_dict_get(impls,
                                                 type,
                                                 NULL);

    if (!impl) {
        N00B_CRAISE("Invalid event type.");
    }

    return (*impl->open_impl)(info, impl);
}

static void
n00b_io_register_subscription(n00b_stream_sub_t        *sub,
                              n00b_io_subscription_kind kind)
{
#ifdef N00B_INTERNAL_DEBUG
    printf("sub: %s (sink) to %s (source)\n",
           n00b_to_string(sub->sink)->data,
           n00b_to_string(sub->source)->data);
#endif

    n00b_stream_t *src  = sub->source;
    n00b_stream_t *sink = sub->sink;
    // TODO: some type checking.

    if (sink && !n00b_stream_can_write(sink)) {
        n00b_post_cerror(sink, "Destination for subscription is not a sink.");
    }

    if (kind == n00b_io_sk_read) {
        if (!n00b_stream_can_read(src)) {
            n00b_post_cerror(sink, "Cannot subscribe to reads on this object.");
        }
        else {
            src->read_active = true;
        }
    }

    if (kind == n00b_io_sk_pre_write || kind == n00b_io_sk_post_write) {
        if (!n00b_stream_can_write(src)) {
            n00b_post_cerror(sink,
                             "Cannot subscribe to writes on this object.");
        }
        else {
            src->write_active = true;
        }
    }

    n00b_dict_t *subs = n00b_stream_get_subscriptions(src, kind);

    n00b_assert(sub);
    hatrack_dict_put(subs, sink, sub);
    return;
}

void
n00b_io_close(n00b_stream_t *party)
{
    n00b_ev2_cookie_t *c = party->cookie;

    if (party->closed_for_write && party->closed_for_read) {
        close(c->id);
    }
}

static void
n00b_stream_cycle_check(n00b_stream_t *source, n00b_stream_t *sink)
{
    defer_on();

    n00b_list_t *l;
    int          n;

    if ((sink->write_start_subs
         && hatrack_dict_get(sink->write_start_subs, source, NULL) != NULL)
        || (sink->write_subs
            && hatrack_dict_get(sink->write_subs, source, NULL) != NULL)) {
        N00B_CRAISE("Subscription causes an IO cycle.");
    }

    if (sink->write_start_subs) {
        l = n00b_dict_keys(sink->write_start_subs);
        if (!l) {
            n = 0;
        }
        else {
            n = n00b_list_len(l);
        }

        for (int i = 0; i < n; i++) {
            n00b_stream_t *next_sink = n00b_list_get(l, i, NULL);
            n00b_acquire_party(next_sink);
            n00b_stream_cycle_check(source, next_sink);
            n00b_release_party(next_sink);
        }
    }
    if (!sink->write_subs) {
        Return;
    }

    l = n00b_dict_keys(sink->write_subs);
    if (!l) {
        n = 0;
    }
    else {
        n = n00b_list_len(l);
    }

    for (int i = 0; i < n; i++) {
        n00b_stream_t *next_sink = n00b_list_get(l, i, NULL);
        n00b_acquire_party(next_sink);
        n00b_stream_cycle_check(source, next_sink);
        n00b_release_party(next_sink);
    }

    Return;
    defer_func_end();
}

static n00b_stream_sub_t *
already_subscribed(n00b_stream_t            *source,
                   n00b_stream_t            *sink,
                   n00b_io_subscription_kind kind)
{
    n00b_dict_t *s = n00b_stream_get_subscriptions(source, kind);
    if (!s) {
        return NULL;
    }

    return hatrack_dict_get(s, sink, NULL);
}

n00b_stream_sub_t *
n00b_raw_subscribe(n00b_stream_t            *source,
                   n00b_stream_t            *sink,
                   n00b_duration_t          *timeout,
                   n00b_io_subscription_kind kind,
                   bool                      src_locked)
{
    n00b_stream_sub_t *existing = already_subscribed(source, sink, kind);

    if (existing) {
        if (src_locked) {
            if (sink) {
                n00b_release_party(sink);
            }
            n00b_release_party(source);
        }
        return existing;
    }

    n00b_stream_cycle_check(source, sink);

    n00b_stream_sub_t *sub = n00b_alloc_subscription(source);
    sub->source            = source;
    sub->sink              = sink;

    if (timeout) {
        sub->timeout = n00b_duration_to_timeval(timeout);
    }

    n00b_io_register_subscription(sub, kind);

    if (src_locked) {
        if (sink) {
            n00b_release_party(sink);
        }
        n00b_release_party(source);
    }

    n00b_io_subscribe_fn fn     = source->impl->subscribe_impl;
    bool                 sub_ok = true;

    if (fn) {
        (*fn)(sub, kind);
    }

    if (!sub_ok) {
        n00b_io_unsubscribe(sub);
        n00b_post_cerror(sub->sink, "Could not subscribe.");
    }

    if (timeout) {
        n00b_static_c_backtrace();
        sub->to_event = evtimer_new(n00b_system_event_base->event_ctx,
                                    timeout_purge,
                                    sub);
    }

    return sub;
}

n00b_stream_sub_t *
n00b_io_subscribe(n00b_stream_t            *source,
                  n00b_stream_t            *sink,
                  n00b_duration_t          *timeout,
                  n00b_io_subscription_kind kind)
{
    defer_on();

    n00b_stream_sub_t *result;

    n00b_acquire_party(source);

    if (sink) {
        n00b_acquire_party(sink);
    }

    result = n00b_raw_subscribe(source, sink, timeout, kind, true);

    Return result;
    defer_func_end();
}

void
n00b_raw_unsubscribe(n00b_stream_sub_t *sub)
{
    defer_on();

    n00b_stream_t         *source       = sub->source;
    bool                   remove_read  = false;
    bool                   remove_write = false;
    n00b_io_unsubscribe_fn fn           = source->impl->unsubscribe_impl;

    n00b_dict_t *subs = n00b_stream_get_subscriptions(source, sub->kind);

    if (!subs) {
        Return;
    }

    n00b_acquire_party(source);

    hatrack_dict_remove(subs, sub->sink);

    if (sub->timeout) {
        n00b_event_del(sub->to_event);
    }

    if (!n00b_stream_has_remaining_subscribers(source, sub->kind)) {
        switch (sub->kind) {
        case n00b_io_sk_read:
            source->read_active = false;
            break;
        case n00b_io_sk_pre_write:
            if (!n00b_stream_has_remaining_subscribers(source,
                                                       n00b_io_sk_post_write)) {
                source->write_active = false;
            }
            break;
        case n00b_io_sk_post_write:
            if (!n00b_stream_has_remaining_subscribers(source,
                                                       n00b_io_sk_pre_write)) {
                source->write_active = false;
            }
            break;
        default:
            break;
        }

        if (fn && (remove_read || remove_write)) {
            (*fn)(sub);
        }
    }

    Return;
    defer_func_end();
}

void
n00b_io_unsubscribe(n00b_stream_sub_t *sub)
{
    defer_on();
    n00b_acquire_party(sub->source);
    n00b_raw_unsubscribe(sub);
    Return;
    defer_func_end();
}

static n00b_list_t *
one_write_filter_level(n00b_stream_filter_t *filter_info,
                       n00b_stream_t        *party,
                       n00b_list_t          *msgs)
{
    n00b_list_t   *result = NULL;
    n00b_list_t   *one_set;
    n00b_filter_fn fn = filter_info->xform_fn;

    int n = n00b_list_len(msgs);

#ifdef N00B_INTERNAL_DEBUG
    cprintf("calling w filter %s\n", filter_info->name);
#endif

    for (int i = 0; i < n; i++) {
        void *msg = n00b_list_get(msgs, i, NULL);
        one_set   = (*fn)(party, filter_info->state, msg);

        if (one_set) {
            n00b_assert(n00b_type_is_list(n00b_get_my_type(one_set)));
            result = n00b_list_plus(result, one_set);
        }
    }

    return result;
}

n00b_list_t *
n00b_handle_read_operation(n00b_stream_t *party, void *buf)
{
    int          n  = 0;
    n00b_list_t *wl = n00b_list(n00b_type_ref());

    n00b_list_append(wl, buf);

    if (!party->read_subs || party->closed_for_read) {
        n00b_release_party(party);
        return NULL;
    }

    n = n00b_list_len(party->read_filters);

    if (!n) {
        return wl;
    }

    for (int i = 0; i < n; i++) {
        n00b_stream_filter_t *filter = n00b_list_get(party->read_filters,
                                                     i,
                                                     NULL);
        n00b_filter_fn        fn     = filter->xform_fn;
        n00b_list_t          *next   = NULL;
        int                   wl_len = n00b_list_len(wl);

        for (int j = 0; j < wl_len; j++) {
            void        *item = n00b_list_get(wl, j, NULL);
            n00b_list_t *res;

            N00B_TRY
            {
#ifdef N00B_INTERNAL_DEBUG
                cprintf("calling r filter %s\n", filter->name);
#endif

                res = (*fn)(party, filter->state, item);
                if (res) {
                    n00b_assert(n00b_type_is_list(n00b_get_my_type(res)));
                }
                if (!next) {
                    next = res;
                }
                else {
                    next = n00b_list_plus(next, res);
                }
            }
            N00B_EXCEPT
            {
                n00b_exception_t *exc = N00B_X_CUR();
                n00b_default_uncaught_handler(exc);
                continue;
            }
            N00B_TRY_END;
        }

        if (!next || !n00b_list_len(next)) {
            return NULL;
        }

        wl = next;
    }

    return wl;
}

static bool
can_quick_write(n00b_stream_t *party, void *msg)
{
    if (!n00b_list_len(party->write_filters)) {
        return true;
    }

    return false;
}

bool
n00b_close(n00b_stream_t *party)
{
    if (!party) {
        return false;
    }

    defer_on();
    n00b_acquire_party(party);

    n00b_io_close_fn fn = party->impl->close_impl;

    party->read_subs        = NULL;
    party->write_subs       = NULL;
    party->write_start_subs = NULL;

    if (fn) {
        (*fn)(party);
    }

    n00b_post_close(party);

    party->close_subs = NULL;
    party->error_subs = NULL;

    Return true;
    defer_func_end();
}

static inline bool
n00b_stream_position_me(n00b_stream_t *party, int position, bool relative)
{
    defer_on();
    n00b_acquire_party(party);

    n00b_io_seek_fn fn = party->impl->seek_impl;

    if (!fn) {
        Return false;
    }

    bool   result = (*fn)(party, relative, position);
    Return result;

    defer_func_end();
}

bool
n00b_stream_set_position(n00b_stream_t *party, int position)
{
    return n00b_stream_position_me(party, position, false);
}
bool
n00b_stream_relative_position(n00b_stream_t *party, int position)
{
    return n00b_stream_position_me(party, position, true);
}

int
n00b_stream_get_position(n00b_stream_t *party)
{
    defer_on();
    n00b_acquire_party(party);

    n00b_io_tell_fn fn = party->impl->tell_impl;

    if (!fn) {
        Return ~0;
    }

    int    result = (*fn)(party);
    Return result;

    defer_func_end();
}

void
n00b_handle_one_delivery(n00b_stream_t *party, void *msg, bool async)
{
    defer_on();
    n00b_acquire_party(party);

    n00b_io_write_start_fn fn = NULL;
    // The rest of this concerns what we write to the actual sink.
    // The waiter is blocking for the message to be written to the
    // sink, but filters on the sink might lead to the message being

    if (!async) {
        fn = party->impl->blocking_write_impl;
    }

    if (!fn) {
        fn = party->impl->write_impl;
    }

    if (!party->write_subs || party->closed_for_write) {
        n00b_post_cerror(party, "Stream cannot be written to.");
        Return;
    }

    if (signal_possible_waiter(msg)) {
        n00b_release_party(party);
        Return;
    }

    if (can_quick_write(party, msg)) {
        n00b_post_to_subscribers(party, msg, n00b_io_sk_pre_write);
        if (fn) {
            void *res = (*fn)(party, msg);

            if (res) {
                msg = res;
            }
        }

        n00b_post_to_subscribers(party, msg, n00b_io_sk_post_write);

        n00b_release_party(party);
        Return;
    }

    /*    cprintf("Write op (%s) on stream: %s\n",
                n00b_get_my_type(msg),
                n00b_stream_full_repr(party));
    */
    n00b_list_t          *filters = party->write_filters;
    int                   n       = n00b_list_len(filters);
    n00b_list_t          *msgs    = n00b_list(n00b_type_ref());
    n00b_stream_filter_t *filter;

    n00b_list_append(msgs, msg);

    for (int i = 0; i < n; i++) {
        n00b_assert(n == n00b_list_len(filters));
        filter = n00b_list_get(filters, i, NULL);
        msgs   = one_write_filter_level(filter, party, msgs);

        if (!msgs) {
            Return;
        }
    }

    n = n00b_list_len(msgs);

    if (n == 0) {
        Return;
    }

    for (int i = 0; i < n; i++) {
        msg = n00b_list_get(msgs, i, NULL);
        if (signal_possible_waiter(msg)) {
            continue;
        }

        n00b_post_to_subscribers(party, msg, n00b_io_sk_pre_write);

        if (fn) {
            void *res = (*fn)(party, msg);

            if (res) {
                msg = res;
            }
        }

        n00b_post_to_subscribers(party, msg, n00b_io_sk_post_write);
    }

    Return;
    defer_func_end();
}

void *
n00b_read(n00b_stream_t *party, uint64_t n, n00b_duration_t *timeout)
{
    defer_on();

    // Note that if there are read subscriptions attached to an event,
    // they will process as they come in. In that case, this call will
    // return a copy of the NEXT available read, and can control how
    // big that read will be.

    n00b_io_read_fn    fn = party->impl->read_impl;
    n00b_stream_sub_t *sub;
    n00b_condition_t  *cv = n00b_gc_alloc_mapped(n00b_condition_t, NULL);
    n00b_stream_t     *cv_event;
    void              *result;
    void              *one_item;
    int                num_objs;
    bool               use_list = false;

    n00b_acquire_party(party);

    if (party->impl->byte_oriented) {
        // For byte-oriented, 0 means, "as much as is available."
        // Also note that n00b_read() can return SHORT even if you do
        // provide a length, because of course it can; the next read
        // will only try to be as big as the request.
        //
        // Note that, unlike stdio, we NEVER try to read more than
        // PIPE_BUF bytes per read from a fd or socket, so specifying
        // more than that can't return more than PIPE_BUF bytes.

        num_objs = 1;

        if (n) {
            if (!party->next_read_limit || party->next_read_limit > n) {
                party->next_read_limit = n;
            }
        }
    }
    else {
        if (!n) {
            n00b_release_party(party);
            Return NULL;
        }

        num_objs = n;
        n        = 1;

        if (num_objs > 1) {
            use_list = true;
            result   = n00b_list(n00b_type_ref());
        }
    }

    if (!fn) {
        n00b_post_cerror(party,
                         "Party cannot be read from synchronously.)");
    }

    n00b_condition_init(cv);

    cv_event = n00b_condition_open(cv, NULL);

    for (int i = 0; i < num_objs; i++) {
        n00b_condition_lock_acquire(cv);
        n00b_printf("«em6»Waiting on [|#:p|] ", cv);
        sub = n00b_io_subscribe_to_reads(party, cv_event, timeout);
        n00b_make_oneshot(sub);

        n00b_release_party(party);
        n00b_condition_wait_then_unlock(cv, (*fn)(party, n));
        one_item = cv->aux;
        if (use_list) {
            n00b_list_append(result, one_item);
        }
        else {
            result = one_item;
        }
    }

    Return result;
    defer_func_end();
}

typedef struct {
    n00b_condition_t cv;
    n00b_list_t     *msgs;
    n00b_stream_t   *from_stream;
} drain_ctx;

static void
collect_items(n00b_stream_t *stream, void *msg, void *aux)
{
    drain_ctx *ctx = aux;
    n00b_list_append(ctx->msgs, msg);

    if (n00b_at_eof(ctx->from_stream)) {
        n00b_condition_lock_acquire(&ctx->cv);
        n00b_condition_notify_one(&ctx->cv);
        n00b_condition_lock_release(&ctx->cv);
    }
}

void *
n00b_stream_read_all(n00b_stream_t *stream)
{
    defer_on();

    // Once we've drained the stream, if every value is the same type,
    // and if that type is either string or buffer, then we add them
    // all up.
    //
    // Otherwise, we return the list of objects.
    int n;

    n00b_acquire_party(stream);

    if (n00b_at_eof(stream)) {
        Return NULL;
    }

    drain_ctx *ctx   = n00b_gc_alloc_mapped(drain_ctx, N00B_GC_SCAN_ALL);
    ctx->msgs        = n00b_list(n00b_type_ref());
    ctx->from_stream = stream;

    n00b_condition_init(&ctx->cv);

    n00b_stream_sub_t *sub = n00b_io_add_read_callback(stream,
                                                       (void *)collect_items,
                                                       ctx);
    n00b_release_party(stream);
    stream = NULL;
    n00b_condition_lock_acquire(&ctx->cv);
    n00b_condition_wait_then_unlock(&ctx->cv);
    n00b_io_unsubscribe(sub);

    n = n00b_list_len(ctx->msgs);

    if (!n) {
        Return NULL;
    }

    void *item = n00b_list_get(ctx->msgs, 0, NULL);
    if (!n00b_is_object_reference(item)) {
        Return ctx->msgs;
    }

    n00b_type_t *t = n00b_get_my_type(item);

    if (!n00b_type_is_string(t) && !n00b_type_is_buffer(t)) {
        Return ctx->msgs;
    }

    if (n == 1) {
        Return item;
    }

    if (n00b_type_is_buffer(t)) {
        n00b_buf_t *b = item;

        for (int i = 1; i < n; i++) {
            item = n00b_list_get(ctx->msgs, i, NULL);
            if (!n00b_is_object_reference(item)) {
                Return ctx->msgs;
            }

            t = n00b_get_my_type(item);

            if (!n00b_type_is_buffer(t)) {
                Return ctx->msgs;
            }

            b = n00b_buffer_add(b, item);
        }

        Return b;
    }

    n00b_string_t *s = item;

    for (int i = 1; i < n; i++) {
        item = n00b_list_get(ctx->msgs, i, NULL);
        if (!n00b_is_object_reference(item)) {
            Return ctx->msgs;
        }

        t = n00b_get_my_type(item);

        if (!n00b_type_is_string(t)) {
            Return ctx->msgs;
        }

        s = n00b_string_concat(s, item);
    }

    Return s;

    defer_func_end();
}

// Plain ol' write queues, so lives in ioqueue.c
// For now, we only allow synchronous writes for file descriptors.
void
_n00b_write_blocking(n00b_stream_t *party,
                     void          *msg,
#ifdef N00B_INTERNAL_DEBUG
                     char *file,
                     int   line,
#endif
                     n00b_duration_t *timeout)
{
    if (timeout) {
        N00B_CRAISE("Timeout for n00b_write_blocking is not yet implemented.");
    }

#ifdef N00B_INTERNAL_DEBUG
    if (n00b_show_write_log) {
        fprintf(stderr, "Write %p @%s:%d\n", msg, file, line);
    }
#endif
    n00b_handle_one_delivery(party, msg, false);

    return;
}

void
n00b_io_loop_once(void)
{
    n00b_io_run_base(n00b_system_event_base, EVLOOP_ONCE | EVLOOP_NONBLOCK);
}

void
n00b_io_loop_drain(void)
{
    defer_on();
    // Allow a bit of recursion but not an eternal amount.
    // Otherwise, we'd just keep going until the callback queue is empty.
    n00b_duration_t *to_add = n00b_new(n00b_type_duration(),
                                       n00b_kw("nanosec",
                                               n00b_ka(N00B_IO_SHUTDOWN_NSEC)));

    n00b_debug_start_shutdown();

    n00b_acquire_event_base(n00b_system_event_base);
    acquire_event_loop();

    while (!n00b_is_debug_shutdown_queue_processed()) {
        n00b_io_loop_once();
        n00b_process_queue();
    }

    n00b_duration_t *now = n00b_now();
    n00b_duration_t *end = n00b_duration_add(now, to_add);

    while (n00b_duration_gt(end, n00b_now())) {
        n00b_io_loop_once();
        n00b_process_queue();
    }

    Return;
    defer_func_end();
}

#if defined(N00B_DEBUG)
extern int16_t *n00b_calculate_col_widths(n00b_table_t *, int16_t, int16_t *);
#endif

static void *
run_io_loop(void *ignore)
{
    bool do_thread_exit = false;

    defer_on();

    N00B_TRY
    {
#ifndef N00B_ASYNC_IO_CALLBACKS
        n00b_ioqueue_launch_callback_thread();
        n00b_acquire_event_base(n00b_system_event_base);
        acquire_event_loop();
        while (!(exit_requested())) {
            n00b_io_loop_once();
        }
#else
        while (!(exit_requested())) {
            n00b_ioqueue_launch_callback_thread();
            n00b_io_loop_once();
        }
#endif
    }
    N00B_EXCEPT
    {
#if defined(N00B_DEBUG) && defined(N00B_BACKTRACE_SUPPORTED)

        n00b_default_uncaught_handler(N00B_X_CUR());

        while (event_base_loopbreak(n00b_system_event_base->event_ctx))
            ;

#endif
        N00B_JUMP_TO_FINALLY();
    }
    N00B_FINALLY
    {
        event_base_loopcontinue(n00b_system_event_base->event_ctx);
        n00b_io_loop_drain();
        n00b_io_has_exited = true;
        n00b_notify(exit_notifier, 1ULL);
        do_thread_exit = true;
    }
    N00B_TRY_END;

    if (do_thread_exit) {
        n00b_release_event_base(n00b_system_event_base);
        n00b_thread_exit(0);
    }
    Return NULL;
    defer_func_end();
}

static void
io_loop_break(int unused1, short unused2, void *unused3)
{
}

extern void n00b_ioqueue_setup(void);

void
n00b_internal_io_setup(void)
{
    n00b_named_lock_init(&event_loop_lock, "event loop");

    impls = n00b_dict(n00b_type_string(), n00b_type_ref());
    setup_libevent();
    n00b_system_event_base->system = true;

    n00b_io_has_exited = false;

    struct event_config *sb_config = event_config_new();

    event_config_require_features(sb_config, EV_FEATURE_FDS);

    n00b_gc_register_root(&n00b_system_event_base, 1);
    n00b_gc_register_root(&loop_timeout, 1);
    n00b_gc_register_root(&n00b_io_thread, 1);
    n00b_gc_register_root(&impls, 1);

    n00b_system_event_base->event_ctx = event_base_new_with_config(sb_config);

    n00b_system_event_base->io_fd_cache       = n00b_dict(n00b_type_int(),
                                                    n00b_type_ref());
    n00b_system_event_base->io_signal_cache   = n00b_dict(n00b_type_int(),
                                                        n00b_type_ref());
    n00b_system_event_base->io_topic_ns_cache = n00b_dict(n00b_type_int(),
                                                          n00b_type_ref());

    n00b_io_impl_register(n00b_cstring("file"), &n00b_fileio_impl);
    n00b_io_impl_register(n00b_cstring("socket"), &n00b_socket_impl);
    n00b_io_impl_register(n00b_cstring("listener"), &n00b_listener_impl);
    n00b_io_impl_register(n00b_cstring("signal"), &n00b_signal_impl);
    n00b_io_impl_register(n00b_cstring("timer"), &n00b_timer_impl);
    n00b_io_impl_register(n00b_cstring("exitinfo"), &n00b_exitinfo_impl);
    n00b_io_impl_register(n00b_cstring("condition"), &n00b_condition_impl);
    n00b_io_impl_register(n00b_cstring("callback"), &n00b_callback_impl);
    n00b_io_impl_register(n00b_cstring("topic"), &n00b_topic_impl);
    n00b_io_impl_register(n00b_cstring("subproxy"), &n00b_subproxy_impl);
    n00b_io_impl_register(n00b_cstring("buffer"), &n00b_bufferio_impl);
    n00b_io_impl_register(n00b_cstring("string"), &n00b_strstream_impl);

    struct timeval tv = {
        .tv_sec  = 0,
        .tv_usec = N00B_IDLE_IO_SLEEP,
    };

    loop_timeout = event_new(n00b_system_event_base->event_ctx,
                             -1,
                             EV_READ | EV_PERSIST,
                             io_loop_break,
                             NULL);

    n00b_event_add(loop_timeout, &tv);

    n00b_stream_t *sio = n00b_stdout();
    n00b_io_set_repr(sio, n00b_cstring("[stdout]"));
    sio = n00b_stderr();
    n00b_io_set_repr(sio, n00b_cstring("[stderr]"));
    sio = n00b_stdin();
    n00b_io_set_repr(sio, n00b_cstring("[stdin]"));

    n00b_io_thread = n00b_thread_self();
    n00b_ioqueue_setup();
}

bool
n00b_wait_for_io_shutdown(void)
{
    // This should really only be used by threads that have
    // asked for the program to exit.

    n00b_assert(exit_notifier);
    n00b_wait(exit_notifier, -1);

    return true;
}

void
n00b_io_begin_shutdown(void)
{
    exit_notifier = n00b_new_notifier();
}

void
n00b_io_init(void)
{
    n00b_gc_register_root(&impls, 1);
    n00b_gc_register_root(&n00b_system_event_base,
                          sizeof(n00b_system_event_base) / 8);
    n00b_gc_register_root(&loop_timeout, 1);
}

static bool is_running = false;

static void *
launch_once(void *ignore)
{
    n00b_set_system_thread();
    n00b_thread_prevent_cancelation();

    if (is_running) {
        return NULL;
    }
    is_running = true;
    run_io_loop(NULL);
    is_running = false;

    n00b_thread_exit(0);
    return NULL;
}

void
n00b_launch_io_loop(void)
{
    pthread_once(&io_roots, n00b_io_init);
    n00b_thread_spawn(launch_once, NULL);
}

void n00b_restart_debugging(void);

void
n00b_restart_io(void)
{
    is_running = false;
    n00b_thread_spawn(launch_once, NULL);
    n00b_restart_debugging();
}

void
n00b_ignore_uncaught_io_errors(void)
{
    raise_on_uncaught_errors = false;
}

void
n00b_post_error_internal(n00b_stream_t *e,
                         n00b_string_t *msg,
                         void          *context,
                         n00b_table_t  *backtrace,
                         char          *filename,
                         int            line)
{
    n00b_exception_t *err = n00b_alloc_string_exception(msg);

    err->file    = filename;
    err->line    = line;
    err->c_trace = backtrace;
    err->context = context;

    if (!n00b_post_to_subscribers(e, (void *)err, n00b_io_sk_error)) {
        if (raise_on_uncaught_errors && !e->ignore_unseen_errors) {
            abort();
            n00b_exception_raise(err, backtrace, filename, line);
        }
    }
}

const n00b_vtable_t n00b_stream_vtable = {
    .methods = {
        [N00B_BI_CONSTRUCTOR] = (n00b_vtable_entry)n00b_stream_init,
        [N00B_BI_TO_STRING]   = (n00b_vtable_entry)n00b_stream_repr,
        [N00B_BI_GC_MAP]      = (n00b_vtable_entry)N00B_GC_SCAN_ALL,
    },
};
