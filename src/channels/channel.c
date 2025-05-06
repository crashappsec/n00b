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

    if (!c->impl.channel->read_subscribe_cb) {
        return;
    }

    if (notify) {
        (*c->impl.channel->read_subscribe_cb)(c, n00b_get_channel_cookie(c));
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

    if (!c->impl.channel->read_unsubscribe_cb) {
        return;
    }

    if (!c->has_rsubs && !c->has_rawsubs) {
        (*c->impl.channel->read_unsubscribe_cb)(c, n00b_get_channel_cookie(c));
    }
}

n00b_channel_t *
n00b_channel_filter_add(n00b_channel_t     *c,
                        n00b_filter_spec_t *filter_spec)
{
    int                 sz        = filter_spec->filter_impl->cookie_size;
    n00b_channel_t     *result    = n00b_gc_alloc(sizeof(n00b_channel_t) + sz);
    n00b_filter_step_t *step_info = n00b_gc_alloc_mapped(n00b_filter_step_t,
                                                         N00B_GC_SCAN_ALL);

    n00b_channel_t *core = n00b_channel_core(c);

    result->kind               = N00B_CHAN_FILTER;
    result->impl.filter        = filter_spec->filter_impl;
    step_info->core            = core;
    step_info->next_read_step  = core->stack.read_top;
    core->stack.read_top       = result;
    step_info->next_write_step = c;

    result->stack.filter = step_info;

    n00b_observable_init(&result->pub_info, NULL);

    result->pub_info.max_topics = -1; // Not used right now.

    if (filter_spec->policy <= 0 || filter_spec->policy > N00B_FILTER_MAX) {
        N00B_CRAISE("Invalid operation policy for filter.");
    }

    if ((filter_spec->policy & N00B_FILTER_READ) && !core->r) {
        // Cannot add; not enabled for reads.
        return NULL;
    }
    if ((filter_spec->policy & N00B_FILTER_WRITE) && !core->w) {
        return NULL;
    }

    if (core->r && !(filter_spec->policy & N00B_FILTER_READ)) {
        step_info->r_skip = true;
    }
    if (core->w && !(filter_spec->policy & N00B_FILTER_WRITE)) {
        step_info->w_skip = true;
    }

    if (step_info->w_skip && step_info->r_skip) {
        return NULL;
    }

    return result;
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
    int sz = sizeof(n00b_channel_t) + impl->cookie_size
           + sizeof(void *) * N00B_CTHUNK_IX;

    n00b_channel_t *core = n00b_gc_alloc(sz);
    n00b_channel_t *cur  = core;

    // Async writes get enqueued and the event-thread processes.
    // Sync writes grab the lock.
    n00b_observable_init(&core->pub_info, NULL);
    n00b_observable_set_subscribe_callback(&core->pub_info,
                                           dispatch_first_subscriber,
                                           false);
    n00b_observable_set_unsubscribe_callback(&core->pub_info,
                                             dispatch_no_subscriber,
                                             false);
    core->pub_info.max_topics = N00B_CT_NUM_TOPICS;

    core->impl.channel = impl;
    core->is_core      = true;

    if (filter_specs) {
        int n = n00b_list_len(filter_specs);
        while (n--) {
            n00b_filter_spec_t *spec = n00b_list_get(filter_specs, n, NULL);
            cur                      = n00b_channel_filter_add(cur, spec);
            if (!cur) {
                return NULL; // Some error.
            }
        }
    }

    int mode = (*impl->init_impl)(n00b_get_channel_cookie(core), init_args);

    switch (mode) {
    case O_RDONLY:
        core->r = true;
        break;
    case O_WRONLY:
        core->w = true;
        break;
    case O_RDWR:
        core->r = true;
        core->w = true;
        break;
    default:
        return NULL;
    }

    if (core->r) {
        core->info[N00B_LOCKRD_IX] = n00b_new(n00b_type_lock());
    }
    if (core->w) {
        core->info[N00B_LOCKWR_IX] = n00b_new(n00b_type_lock());
    }
    core->info[N00B_LOCKSUB_IX] = n00b_new(n00b_type_lock());

    return cur;
}

n00b_observer_t *
n00b_channel_subscribe_read(n00b_channel_t *channel,
                            n00b_channel_t *dst,
                            bool            oneshot)
{
    n00b_observer_t *result;
    n00b_channel_t  *core = n00b_channel_core(channel);

    if (channel->is_core && core->stack.read_top) {
        // This doesn't impact notifying the implementation about subscribers;
        // raw subscriptions sit there until there is a blocking read() or
        // until a non-raw subscription is made.
        return n00b_observable_subscribe(&core->pub_info,
                                         (void *)(int64_t)N00B_CT_RAW,
                                         route_channel_message,
                                         oneshot,
                                         dst);
    }
    else {
        return n00b_observable_subscribe(&core->pub_info,
                                         (void *)(int64_t)N00B_CT_R,
                                         route_channel_message,
                                         oneshot,
                                         dst);
    }

    return result;
}

n00b_observer_t *
n00b_channel_subscribe_queue(n00b_channel_t *channel,
                             n00b_channel_t *dst,
                             bool            oneshot)
{
    n00b_channel_t *core = n00b_channel_core(channel);

    return n00b_observable_subscribe(&core->pub_info,
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
    n00b_channel_t *core = n00b_channel_core(channel);

    return n00b_observable_subscribe(&core->pub_info,
                                     (void *)(int64_t)N00B_CT_W,
                                     route_channel_message,
                                     oneshot,
                                     dst);
}

n00b_observer_t *
n00b_channel_subscribe_close(n00b_channel_t *channel,
                             n00b_channel_t *dst)
{
    n00b_channel_t *core = n00b_channel_core(channel);

    return n00b_observable_subscribe(&core->pub_info,
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
    n00b_channel_t *core = n00b_channel_core(channel);

    return n00b_observable_subscribe(&core->pub_info,
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
n00b_next_w(n00b_channel_t *channel, n00b_cmsg_t *msg, bool blocking)
{
    n00b_channel_t *core = n00b_channel_core(channel);
    n00b_list_t    *pass;
    void           *cookie;
    int             n;

    while (channel != core && channel->stack.filter->w_skip) {
        channel = channel->stack.filter->next_write_step;
    }

    if (channel == core) {
        cookie = n00b_get_channel_cookie(channel);
        // Notify BEFORE sending. If you want to be confident it sent,
        // do the blocking write yourself.
        n00b_cnotify_w(core, msg);
        (*channel->impl.channel->write_impl)(cookie, msg, blocking);
    }
    else {
        cookie = n00b_get_channel_cookie(core);
        pass   = (*channel->impl.filter->write_fn)(cookie, msg, blocking);
        n      = n00b_list_len(pass);
        for (int i = 0; i < n; i++) {
            n00b_next_w(channel->stack.filter->next_write_step,
                        n00b_list_get(pass, n, NULL),
                        blocking);
        }
    }
}

static void
internal_write(n00b_channel_t *channel,
               void           *msg,
               int             nitems,
               char           *file,
               int             line,
               bool            blocking)
{
    n00b_channel_t *core;

    if (!channel->is_core) {
        core = channel->stack.filter->core;
    }
    else {
        core = channel;
    }

    if (!core->w) {
        N00B_CRAISE("Channel is not open for writing.");
    }

    n00b_cmsg_t *pkg = package_message(msg, nitems, file, line);
    n00b_lock_acquire(core->info[N00B_LOCKWR_IX]);
    n00b_cnotify_q(core, pkg);
    n00b_lock_release(core->info[N00B_LOCKWR_IX]);
    n00b_next_w(channel, pkg, blocking);
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

static bool
apply_read_filter_layer(n00b_channel_t *cur, n00b_list_t *input)
{
    void *cookie    = (void *)cur->info;
    cur->read_cache = (*cur->impl.filter->read_fn)(cookie, input, true);

    if (!cur->read_cache || !n00b_list_len(cur->read_cache)) {
        return false;
    }

    return true;
}

// The 'top' parameter here is how we can allow for us to
// short-circuit some of the filter stack.
//
// For instance, we could have a read-filter stack on stdin that will
// read, line-buffer and then strip out ansi codes. But if we want to
// read and get the ansi codes, we can request the read from the
// line-buffer filter, or from the core channel if we don't even want
// buffering.

void *
n00b_perform_channel_read(n00b_channel_t *core,
                          n00b_channel_t *top,
                          bool            block,
                          int             tout)
{
    bool           ok = true;
    void          *result;
    n00b_list_t   *cache = top->read_cache;
    n00b_chan_r_fn fn;
    void          *v;

    if (cache && n00b_list_len(cache)) {
        result = n00b_list_dequeue(cache);
        if (core == top) {
            n00b_cnotify_raw(core, result);
        }
        n00b_cnotify_r(core, result);
        return result;
    }

raw_read_required:

    fn = core->impl.channel->read_impl;
    v  = (*fn)(n00b_get_channel_cookie(core), block, &ok, tout);

    if (!ok) {
        return NULL;
    }

    n00b_cnotify_raw(core, v);
    n00b_channel_t *cur  = core->stack.read_top;
    n00b_channel_t *prev = core;

    if (!cur) {
        n00b_cnotify_r(core, v);
        return v;
    }

    n00b_list_t *input = n00b_list(n00b_type_ref());
    n00b_list_append(input, v);

    while (true) {
        // If filter layers are one-way only, and we skip them, we go
        // down to the previous active r filter's output once we reach
        // the end. And if there turned out to be filters but no read
        // filters, v was still intact...
        if (cur == top || cur->stack.filter->r_skip) {
            if (cur == top) {
                if (prev == core) {
                    n00b_cnotify_r(core, v);
                    return v;
                }
                v = n00b_list_dequeue(input);
                n00b_cnotify_r(core, v);
                return v;
            }
            cur = cur->stack.filter->next_read_step;
            continue;
        }

        // This layer is not to be skipped; the return value is
        // true if it successfully yielded a message, and false
        // if it's buffering or dropped, in which case we need
        // to go to the underlying core and get more input.
        //
        // But either way, the new filter is expected to completely
        // clear the cache of the previous layer.
        if (prev != core) {
            input            = prev->read_cache;
            prev->read_cache = NULL;
        }
        if (!apply_read_filter_layer(cur, input)) {
            goto raw_read_required;
        }

        prev = cur;
        cur  = cur->stack.filter->next_read_step;
    }
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
    n00b_channel_t *core = n00b_channel_core(channel);

    if (!core->r) {
        N00B_CRAISE("Channel is not open for reading.");
    }

    n00b_lock_acquire(core->info[N00B_LOCKRD_IX]);
    void *result = n00b_perform_channel_read(core, channel, true, ms_timeout);
    n00b_lock_release(core->info[N00B_LOCKRD_IX]);
    return result;
}
void
n00b_io_dispatcher_process_read_queue(n00b_channel_t *channel)
{
    n00b_channel_t *core = n00b_channel_core(channel);

    if (!core->r) {
        return;
    }

    n00b_perform_channel_read(core, channel, false, 0);
}

int
n00b_channel_get_position(n00b_channel_t *c)
{
    n00b_channel_t *core = n00b_channel_core(c);
    if (!core->impl.channel->gpos_impl) {
        return -1;
    }
    return (*core->impl.channel->gpos_impl)(n00b_get_channel_cookie(c));
}

bool
n00b_channel_set_relative_position(n00b_channel_t *c, int pos)
{
    n00b_channel_t *core = n00b_channel_core(c);
    if (!core->impl.channel->spos_impl) {
        return false;
    }

    return (*core->impl.channel->spos_impl)(n00b_get_channel_cookie(c),
                                            pos,
                                            true);
}

extern bool
n00b_channel_set_absolute_position(n00b_channel_t *c,
                                   int             pos)
{
    n00b_channel_t *core = n00b_channel_core(c);

    if (!core->impl.channel->spos_impl) {
        return false;
    }

    return (*core->impl.channel->spos_impl)(n00b_get_channel_cookie(c),
                                            pos,
                                            false);
}

extern void
n00b_channel_close(n00b_channel_t *c)
{
    n00b_channel_t *core = n00b_channel_core(c);

    if (core->impl.channel->close_impl) {
        (*core->impl.channel->close_impl)(n00b_get_channel_cookie(c));
    }

    c->r = false;
    c->w = false;
    n00b_cnotify_close(c, (void *)~0ULL);
}
