// TODO: Timeout field isn't implemented yet.

#define N00B_USE_INTERNAL_API
#include "n00b.h"

static _Atomic int32_t global_msg_id = 0;
static void            route_channel_message(void *msg, void *dst);

// This callback gets the first subscriber for *any* channel.
// So we need to know if any read subscriber is listening.
static void
dispatch_first_subscriber(n00b_observable_t *obs,
                          n00b_observer_t   *subscription)
{
    // This relies on the pub_info field being first in a channel. If
    // we ever move it, we'll have to go through the alloc's data
    // pointer.
    n00b_channel_t *c      = (void *)obs;
    bool            notify = (!c->has_rsubs) | (!c->has_rawsubs);

    switch (subscription->topic_ix) {
    case N00B_CT_R:
        c->has_rsubs = true;
        break;
    case N00B_CT_RAW:
        c->has_rawsubs = true;
        break;
    default:
        return;
    }

    if (!c->impl->read_subscribe_cb) {
        return;
    }

    if (notify) {
        (*c->impl->read_subscribe_cb)(c, n00b_get_channel_cookie(c));
    }
}

static void
dispatch_no_subscriber(n00b_observable_t *obs,
                       n00b_observer_t   *subscription)
{
    // This relies on the pub_info field being first in a channel. If
    // we ever move it, we'll have to go through the alloc's data
    // pointer.

    n00b_channel_t *c = (void *)obs;

    switch (subscription->topic_ix) {
    case N00B_CT_R:
        c->has_rsubs = false;
        break;
    case N00B_CT_RAW:
        c->has_rawsubs = false;
        break;
    default:
        return;
    }

    if (!c->impl->read_unsubscribe_cb) {
        return;
    }

    if (!c->has_rsubs && !c->has_rawsubs) {
        (*c->impl->read_unsubscribe_cb)(c, n00b_get_channel_cookie(c));
    }
}

// We expect the natural way for people to give us a list of filters is
// based on write transformations, first transformation first in the list.
// So we actually want to build the chain from the back of the array...
n00b_channel_t *
n00b_channel_create(n00b_chan_impl *impl,
                    void           *init_args,
                    n00b_list_t    *filter_specs)
{
    // Alloc the channl, the cookie (thunk), and slots for locks,
    // which are variable since we don't alloc them for filter layers.
    // The start of the thunk is found via indexing into the lock array,
    // so the number of lock pointers is equal to that index (currently 3).
    int             sz  = sizeof(n00b_channel_t) + impl->cookie_size;
    n00b_channel_t *cur = n00b_gc_alloc(sz);

    // Async writes get enqueued and the event-thread processes.
    // Sync writes grab the lock.
    n00b_observable_init(&cur->pub_info, NULL);
    n00b_observable_set_subscribe_callback(&cur->pub_info,
                                           dispatch_first_subscriber,
                                           false);
    n00b_observable_set_unsubscribe_callback(&cur->pub_info,
                                             dispatch_no_subscriber,
                                             false);
    cur->pub_info.max_topics = N00B_CT_NUM_TOPICS;

    cur->impl = impl;

    if (filter_specs) {
        int n = n00b_list_len(filter_specs);

        while (n--) {
            n00b_filter_spec_t *spec = n00b_list_get(filter_specs, n, NULL);
            if (!n00b_filter_add(cur, spec->impl, spec->policy)) {
                return NULL;
            }
        }
    }

    int mode = (*impl->init_impl)(n00b_get_channel_cookie(cur), init_args);

    switch (mode) {
    case O_RDONLY:
        cur->r = true;
        break;
    case O_WRONLY:
        cur->w = true;
        break;
    case O_RDWR:
        cur->r = true;
        cur->w = true;
        break;
    default:
        return NULL;
    }

    if (cur->r) {
        cur->locks[N00B_LOCKRD_IX] = n00b_new(n00b_type_lock());
    }
    if (cur->w) {
        cur->locks[N00B_LOCKWR_IX] = n00b_new(n00b_type_lock());
    }
    cur->locks[N00B_LOCKSUB_IX] = n00b_new(n00b_type_lock());

    return cur;
}

n00b_observer_t *
n00b_channel_subscribe_read(n00b_channel_t *channel,
                            n00b_channel_t *dst,
                            bool            oneshot)
{
    return n00b_observable_subscribe(&channel->pub_info,
                                     (void *)(int64_t)N00B_CT_R,
                                     route_channel_message,
                                     oneshot,
                                     dst);
}

n00b_observer_t *
n00b_channel_subscribe_raw(n00b_channel_t *channel,
                           n00b_channel_t *dst,
                           bool            oneshot)
{
    return n00b_observable_subscribe(&channel->pub_info,
                                     (void *)(int64_t)N00B_CT_RAW,
                                     route_channel_message,
                                     oneshot,
                                     dst);
}

n00b_observer_t *
n00b_channel_subscribe_queue(n00b_channel_t *channel,
                             n00b_channel_t *dst,
                             bool            oneshot)
{
    return n00b_observable_subscribe(&channel->pub_info,
                                     (void *)(int64_t)N00B_CT_Q,
                                     route_channel_message,
                                     oneshot,
                                     dst);
}

n00b_observer_t *
n00b_channel_subscribe_write(n00b_channel_t *channel,
                             n00b_channel_t *dst,
                             bool            oneshot)
{
    return n00b_observable_subscribe(&channel->pub_info,
                                     (void *)(int64_t)N00B_CT_W,
                                     route_channel_message,
                                     oneshot,
                                     dst);
}

n00b_observer_t *
n00b_channel_subscribe_close(n00b_channel_t *channel,
                             n00b_channel_t *dst)
{
    return n00b_observable_subscribe(&channel->pub_info,
                                     (void *)(int64_t)N00B_CT_CLOSE,
                                     route_channel_message,
                                     true,
                                     dst);
}

n00b_observer_t *
n00b_channel_subscribe_error(n00b_channel_t *channel,
                             n00b_channel_t *dst,
                             bool            oneshot)
{
    return n00b_observable_subscribe(&channel->pub_info,
                                     (void *)(int64_t)N00B_CT_ERROR,
                                     route_channel_message,
                                     oneshot,
                                     dst);
}

static inline n00b_cmsg_t *
package_message(void *m, int nitems, char *f, int l)
{
    n00b_cmsg_t *result = n00b_gc_alloc_mapped(n00b_cmsg_t,
                                               N00B_GC_SCAN_ALL);
    result->payload     = m;
    result->nitems      = nitems;
    result->id          = atomic_fetch_add(&global_msg_id, 1);
    result->file        = f;
    result->line        = l;

    return result;
}

static void
internal_write(n00b_channel_t *channel,
               void           *msg,
               int             nitems,
               char           *file,
               int             line,
               bool            blocking)
{
    if (!channel->w) {
        n00b_cnotify_error(channel,
                           n00b_cstring("Channel is not open for writing."));
        return;
    }

    n00b_cmsg_t *pkg = package_message(msg, nitems, file, line);
    n00b_lock_acquire(channel->locks[N00B_LOCKWR_IX]);
    n00b_cnotify_q(channel, pkg);
    n00b_lock_release(channel->locks[N00B_LOCKWR_IX]);
    n00b_list_t *to_deliver = n00b_filter_writes(channel, pkg);

    if (!to_deliver) {
        return;
    }

    void *cookie = n00b_get_channel_cookie(channel);
    void *out_msg;

    int n = n00b_list_len(to_deliver);

    for (int i = 0; i < n; i++) {
        out_msg = n00b_list_get(to_deliver, i, NULL);
        n00b_cnotify_w(channel, out_msg);
        (*channel->impl->write_impl)(cookie, out_msg, blocking);
    }
}

// BLOCKING
void
_n00b_channel_write(n00b_channel_t *channel,
                    void           *msg,
                    char           *file,
                    int             line)
{
    internal_write(channel, msg, 1, file, line, true);
}

// NON-BLOCKING; processes the queue,
void
_n00b_channel_queue(n00b_channel_t *channel,
                    void           *msg,
                    char           *file,
                    int             line)
{
    internal_write(channel, msg, 1, file, line, false);
}

void
route_channel_message(void *msg, void *dst)
{
    _n00b_channel_queue(dst, msg, __FILE__, __LINE__);
}

// Attempt to read from the source until it fails.
// TODO: Handle fatal errors.
static inline bool
nonblocking_channel_read(n00b_channel_t *channel)
{
    bool           ok = true;
    n00b_chan_r_fn fn;
    void          *v;

    while (true) {
        fn = channel->impl->read_impl;
        v  = (*fn)(n00b_get_channel_cookie(channel), &ok);

        if (!ok) {
            return false;
        }

        n00b_cnotify_raw(channel, v);
        n00b_cmsg_t *pkg    = package_message(v, 1, __FILE__, __LINE__);
        channel->read_cache = n00b_filter_reads(channel, pkg);
        if (channel->read_cache) {
            return true;
        }
    }
}

static inline void *
wait_for_read(n00b_channel_t  *channel,
              n00b_lock_t     *lock,
              n00b_duration_t *end,
              int             *err)
{
    void *result;

    pthread_cond_t *cond = n00b_gc_alloc_mapped(pthread_cond_t,
                                                N00B_GC_SCAN_NONE);
    pthread_cond_t *check;

    pthread_cond_init(cond, NULL);

    if (!channel->blocked_readers) {
        channel->blocked_readers = n00b_list(n00b_type_ref());
    }
    n00b_list_append(channel->blocked_readers, cond);

    // Here, we need to make sure that, if there are no read subscribers,
    // the polling loop associated w/ the fd is still going to be polling
    // for reads.

    if (channel->fd_backed) {
        n00b_fd_cookie_t *cookie = n00b_get_channel_cookie(channel);
        n00b_fd_stream_t *s      = cookie->stream;

        if (!s->r_added) {
            while (!s->needs_r) {
                s->needs_r = true;
            }
            n00b_list_append(s->evloop->pending, s);
        }
    }

    if (end) {
        int res = pthread_cond_timedwait(cond,
                                         &lock->lock,
                                         end);
        if (res) {
            *err = true;
            n00b_private_list_remove_item(channel->blocked_readers,
                                          cond);
            n00b_lock_release(lock);
            return NULL;
        }
    }
    else {
        int res = pthread_cond_wait(cond, &lock->lock);
        if (res) {
            *err = true;
            return NULL;
        }
    }

    *err = false;

    result = n00b_private_list_dequeue(channel->read_cache);
    check  = n00b_private_list_dequeue(channel->blocked_readers);
    assert(check == cond);

    // If there are leftover messages and others are waiting, signal
    // the next waiter in line.

    if (n00b_list_len(channel->blocked_readers)
        && n00b_list_len(channel->read_cache)) {
        cond = n00b_list_get(channel->blocked_readers, 0, NULL);
        pthread_cond_signal(cond);
    }

    n00b_lock_release(lock);

    return result;
}

static void *
n00b_perform_channel_read(n00b_channel_t *channel,
                          bool            blocking,
                          int             tout,
                          int            *err)
{
    n00b_mutex_t    *lock   = channel->locks[N00B_LOCKRD_IX];
    void            *result = NULL;
    pthread_cond_t  *cond;
    n00b_duration_t *end;

    if (tout) {
        end = n00b_new_ms_timeout(tout);
    }
    else {
        end = NULL;
    }

    n00b_lock_acquire(lock);

    // 1. If there aren't enough available queued messages for us
    //    (behind any pending waiters) then call the raw non-blocking
    //    channel read until it fails.
    //
    // 2. If we DO at any point have enough messages, take our message
    //    from its place in the queue (letting earlier readers have
    //    earlier messages), signal the first waiter (if any), and
    //    then exit.
    //
    // 3. If instead, we fail before getting enough messages,
    //    if we're blocking, we call `wait_for_read`. Otherwise,
    //    we keep going.

    // We don't need a message at all if this is called non-blocking,
    // and there are no blocked readers.
    //
    // However, we will need to dequeue the message.
    int n = 1;

    if (channel->blocked_readers) {
        n += n00b_list_len(channel->blocked_readers);
    }

    while (!channel->read_cache || n00b_list_len(channel->read_cache) < n) {
        if (!nonblocking_channel_read(channel)) {
            if (!blocking || channel->impl->disallow_blocking_reads) {
                n00b_lock_release(lock);
                *err = true;
                return NULL;
            }

            // Some channel types may not give us asynchronous notification.
            // In that case, we just poll.
            //
            // Specifically, buffer objects are mutable, and can be
            // added to whenever. But since they're usually not
            // monitored in an event loop, there's no built-in
            // notification mechanism.
            //
            // So first we see if we're passed any timeout.  And if
            // we're not, we wait for the default poll interval.  Hope
            // you know what you're doing if you're making a
            // non-blocking read on a buffer...
            if (channel->impl->poll_for_blocking_reads) {
                if (end && n00b_duration_lt(end, n00b_now())) {
                    *err = true;
                    return NULL;
                }

                n00b_nanosleep(0, N00B_POLL_DEFAULT_MS * 1000000);
            }
            else {
                // For most things, we can just hang to see if a poll
                // comes in.
                return wait_for_read(channel, lock, end, err);
            }
        }
    }

    result = n00b_list_get(channel->read_cache, n - 1, NULL);
    n00b_private_list_remove(channel->read_cache, n - 1);
    n00b_cnotify_r(channel, result);

    if (channel->blocked_readers && n00b_list_len(channel->blocked_readers)
        && n00b_list_len(channel->read_cache)) {
        cond = n00b_list_get(channel->blocked_readers, 0, NULL);
        pthread_cond_signal(cond);
    }

    *err = false;

    n00b_lock_release(lock);
    return result;
}

// This always blocks. Subscribe to reads if you want async.  Also,
// this does not allow you to specify a quantity of items to read; the
// filtering infrastructure should generally be making decisions about
// what to read.
//
// In this instance, if reads() should return a string or a buffer,
// this API should wait until data becomes available, and then return
// as much data is available without blocking.

void *
n00b_channel_read(n00b_channel_t *channel, int ms_timeout)
{
    if (!channel->r) {
        n00b_cnotify_error(channel,
                           n00b_cstring("Channel is not open for reading."));
        return NULL;
    }

    int err;

    return n00b_perform_channel_read(channel, true, ms_timeout, &err);
}

void
n00b_io_dispatcher_process_read_queue(n00b_channel_t *channel)
{
    if (!channel->r) {
        return;
    }

    int err;
    n00b_perform_channel_read(channel, false, 0, &err);
}

int
n00b_channel_get_position(n00b_channel_t *c)
{
    if (!c->impl->gpos_impl) {
        return -1;
    }
    return (*c->impl->gpos_impl)(n00b_get_channel_cookie(c));
}

bool
n00b_channel_set_relative_position(n00b_channel_t *c, int pos)
{
    if (!c->impl->spos_impl) {
        return false;
    }

    return (*c->impl->spos_impl)(n00b_get_channel_cookie(c), pos, true);
}

extern bool
n00b_channel_set_absolute_position(n00b_channel_t *c, int pos)
{
    if (!c->impl->spos_impl) {
        return false;
    }

    return (*c->impl->spos_impl)(n00b_get_channel_cookie(c), pos, false);
}

extern void
n00b_channel_close(n00b_channel_t *c)
{
    if (c->impl->close_impl) {
        (*c->impl->close_impl)(n00b_get_channel_cookie(c));
    }

    c->r = false;
    c->w = false;
    n00b_cnotify_close(c, (void *)~0ULL);
}
