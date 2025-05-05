// TODO: Timeout field isn't implemented yet.

#define N00B_USE_INTERNAL_API
#include "io/channel.h"

static _Atomic int32_t global_msg_id = 0;
static void            route_channel_message(void *msg, void *dst);

static inline void *
get_channel_cookie(n00b_channel_t *core)
{
    return (void *)&core->info[N00B_CTHUNK_IX];
}

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
        (*c->impl.channel->read_subscribe_cb)(c, get_channel_cookie(c));
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
        (*c->impl.channel->read_unsubscribe_cb)(c, get_channel_cookie(c));
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

    int mode = (*impl->init_impl)(get_channel_cookie(core), init_args);

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
        cookie = get_channel_cookie(channel);
        // Notify BEFORE sending. If you want to be confident it sent,
        // do the blocking write yourself.
        n00b_cnotify_w(core, msg);
        (*channel->impl.channel->write_impl)(cookie, msg, blocking);
    }
    else {
        cookie = get_channel_cookie(core);
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

static void *
perform_read(n00b_channel_t *core, n00b_channel_t *top, bool block, int tout)
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
    v  = (*fn)(get_channel_cookie(core), block, &ok, tout);

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
    void *result = perform_read(core, channel, true, ms_timeout);
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

    perform_read(core, channel, false, 0);
}

int
n00b_channel_get_position(n00b_channel_t *c)
{
    n00b_channel_t *core = n00b_channel_core(c);
    if (!core->impl.channel->gpos_impl) {
        return -1;
    }
    return (*core->impl.channel->gpos_impl)(get_channel_cookie(c));
}

bool
n00b_channel_set_relative_position(n00b_channel_t *c, int pos)
{
    n00b_channel_t *core = n00b_channel_core(c);
    if (!core->impl.channel->spos_impl) {
        return false;
    }

    return (*core->impl.channel->spos_impl)(get_channel_cookie(c), pos, true);
}

extern bool
n00b_channel_set_absolute_position(n00b_channel_t *c,
                                   int             pos)
{
    n00b_channel_t *core = n00b_channel_core(c);

    if (!core->impl.channel->spos_impl) {
        return false;
    }

    return (*core->impl.channel->spos_impl)(get_channel_cookie(c), pos, false);
}

extern void
n00b_channel_close(n00b_channel_t *c)
{
    n00b_channel_t *core = n00b_channel_core(c);

    if (core->impl.channel->close_impl) {
        (*core->impl.channel->close_impl)(get_channel_cookie(c));
    }

    c->r = false;
    c->w = false;
    n00b_cnotify_close(c, (void *)~0ULL);
}

// Move the fd channel implementation to another file.

static int
fdchan_init(n00b_fd_cookie_t *p, n00b_fd_stream_t *s)
{
    p->stream = s;
    return s->fd_flags & O_ACCMODE;
}

static void
fdchan_on_read_event(n00b_fd_stream_t *s,
                     n00b_fd_sub_t    *sub,
                     n00b_buf_t       *buf,
                     void             *thunk)
{
    n00b_channel_t *channel = thunk;

    if (!channel->read_cache) {
        channel->read_cache = n00b_list(n00b_type_ref());
    }
    n00b_list_append(channel->read_cache, buf);
    n00b_io_dispatcher_process_read_queue(channel);
}

static void
fdchan_on_first_subscriber(n00b_channel_t *c, n00b_fd_cookie_t *p)
{
    p->sub = _n00b_fd_read_subscribe(p->stream,
                                     fdchan_on_read_event,
                                     2,
                                     (int)(false),
                                     c);
}

static void
fdchan_on_no_subscribers(n00b_channel_t *c, n00b_fd_cookie_t *p)
{
    if (p->sub) {
        n00b_fd_unsubscribe(p->stream, p->sub);
        p->sub = NULL;
    }
}

static bool
fdchan_close(n00b_fd_cookie_t *p)
{
    close(p->stream->fd);

    if (p->sub) {
        n00b_fd_unsubscribe(p->stream, p->sub);
    }

    return true;
}

static void *
fdchan_read(n00b_fd_cookie_t *p, bool block, bool *success, int ms_timeout)
{
    n00b_buf_t *data;

    // In the first case, we were called via the user.  In the second,
    // we will have been poll'd via the event loop, which is also the
    // current thread.
    //
    // We will have just stuck the value read into the cookie already,
    // and will not have contention.

    if (block) {
        data     = n00b_fd_read(p->stream, -1, ms_timeout, false);
        *success = (data != NULL);
    }
    else {
        data          = p->read_cache;
        p->read_cache = NULL;
    }
    return data;
}

static void
fdchan_write(n00b_fd_cookie_t *p, n00b_cmsg_t *msg, bool block)
{
    n00b_buf_t *b = msg->payload;

    if (block) {
        n00b_fd_write(p->stream, b->data, b->byte_len);
    }
    else {
        n00b_fd_send(p->stream, b->data, b->byte_len);
    }
}

static bool
fdchan_set_position(n00b_fd_cookie_t *p, int pos, bool relative)
{
    if (relative) {
        return n00b_fd_set_relative_position(p->stream, pos);
    }

    return n00b_fd_set_absolute_position(p->stream, pos);
}

#include "io/channel_cb.h"

static n00b_chan_impl n00b_fdchan_impl = {
    .cookie_size         = sizeof(n00b_fd_stream_t **),
    .init_impl           = (void *)fdchan_init,
    .read_impl           = (void *)fdchan_read,
    .write_impl          = (void *)fdchan_write,
    .spos_impl           = (void *)fdchan_set_position,
    .gpos_impl           = (void *)n00b_fd_get_position,
    .close_impl          = (void *)fdchan_close,
    .read_subscribe_cb   = (void *)fdchan_on_first_subscriber,
    .read_unsubscribe_cb = (void *)fdchan_on_no_subscribers,
};

static int
cbchan_init(n00b_callback_channel_t *c, n00b_list_t *args)
{
    c->thunk           = n00b_private_list_pop(args);
    c->cb              = n00b_private_list_pop(args);
    c->waiting_readers = n00b_list(n00b_type_condition());

    return O_RDWR;
}

static void *
cbchan_read(n00b_callback_channel_t *c,
            bool                     block,
            bool                    *success,
            int                      ms_timeout)
{
    n00b_condition_t *cond = n00b_new(n00b_type_condition());
    n00b_list_append(c->waiting_readers, cond);

    if (!ms_timeout) {
        n00b_condition_lock_acquire(cond);
        n00b_condition_wait(cond);
        *success = true;
        return cond->aux;
    }
    struct timespec ts = {
        .tv_sec  = ms_timeout / 1000,
        .tv_nsec = (ms_timeout % 1000) * 1000000,
    };

    if (n00b_condition_timed_wait(cond, &ts)) {
        *success = true;
        return cond->aux;
    }
    *success = false;
    return NULL;
}

typedef struct {
    n00b_callback_channel_t *c;
    n00b_cmsg_t             *msg;
} cbchan_info_t;

static void cbchan_write(n00b_callback_channel_t *,
                         n00b_cmsg_t *,
                         bool);
static void *
async_callback_runner(cbchan_info_t *info)
{
    cbchan_write(info->c, info->msg, true);
    return NULL;
}

static void
cbchan_write(n00b_callback_channel_t *c, n00b_cmsg_t *msg, bool block)
{
    if (!block) {
        cbchan_info_t *info = n00b_gc_alloc_mapped(cbchan_info_t,
                                                   N00B_GC_SCAN_ALL);
        info->c             = c;
        info->msg           = msg;

        n00b_thread_spawn((void *)async_callback_runner, info);
        return;
    }

    n00b_lock_list(c->waiting_readers);
    void             *for_readers = (*c->cb)(msg->payload, c->thunk);
    n00b_condition_t *cond        = n00b_private_list_pop(c->waiting_readers);

    n00b_channel_t *core = (void *)n00b_channel_core(c->cref);

    while (cond) {
        n00b_condition_lock_acquire(cond);
        n00b_condition_notify_aux(cond, for_readers);
        n00b_condition_lock_release(cond);
        cond = n00b_private_list_pop(c->waiting_readers);
    }

    n00b_unlock_list(c->waiting_readers);

    if (!c->cref->read_cache) {
        c->cref->read_cache = n00b_list(n00b_type_ref());
        n00b_list_append(c->cref->read_cache, for_readers);
    }

    perform_read(core, c->cref, false, 0);
}

static n00b_chan_impl n00b_cbchan_impl = {
    .cookie_size = sizeof(n00b_callback_channel_t),
    .init_impl   = (void *)cbchan_init,
    .read_impl   = (void *)cbchan_read,
    .write_impl  = (void *)cbchan_write,
};

n00b_channel_t *
_n00b_new_callback_channel(n00b_channel_cb_t *cb, ...)
{
    n00b_list_t *args    = n00b_list(n00b_type_ref());
    n00b_list_t *filters = n00b_list(n00b_type_ref());
    void        *thunk   = NULL;

    n00b_list_append(args, cb);
    n00b_list_append(args, thunk);

    va_list alist;

    va_start(alist, cb);
    int nargs = va_arg(alist, int);

    if (nargs--) {
        thunk = va_arg(alist, void *);
    }
    while (nargs--) {
        n00b_list_append(filters, va_arg(alist, void *));
    }

    va_end(alist);

    n00b_channel_t          *result = n00b_channel_create(&n00b_cbchan_impl,
                                                 args,
                                                 filters);
    n00b_callback_channel_t *cookie = get_channel_cookie(result);
    cookie->cref                    = result;

    return result;
}

static void
on_fd_close(n00b_fd_stream_t *s, n00b_channel_t *c)
{
    c->r = false;
    c->w = false;
    n00b_cnotify_close(c, NULL);
}

n00b_channel_t *
_n00b_new_fd_channel(n00b_fd_stream_t *fd, ...)
{
    n00b_list_t        *filters = NULL;
    n00b_filter_spec_t *filter;
    n00b_channel_t     *result;

    va_list args;
    va_start(args, fd);

    filter = va_arg(args, n00b_filter_spec_t *);

    if (filter) {
        if (n00b_type_is_list(n00b_get_my_type(filter))) {
            filters = (void *)filter;
        }

        filters = n00b_list(n00b_type_ref());

        while (filter) {
            n00b_list_append(filters, filter);
            filter = va_arg(args, n00b_filter_spec_t *);
        }
    }

    va_end(args);

    result = n00b_channel_create(&n00b_fdchan_impl, fd, filters);
    _n00b_fd_close_subscribe(fd, (void *)on_fd_close, 2, (int)false, result);

    return result;
}

n00b_channel_t *
_n00b_channel_open_file(n00b_string_t *filename, ...)
{
    va_list args;
    va_start(args, filename);

    // o0744
    int permissions = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

    n00b_list_t *filters                     = NULL;
    bool         read_only                   = false;
    bool         write_only                  = false;
    bool         allow_relative_paths        = true;
    bool         allow_backtracking          = true;
    bool         normalize_paths             = true;
    bool         allow_file_creation         = false;
    bool         error_if_exists             = false;
    bool         truncate_on_open            = false;
    bool         writes_always_append        = false;
    bool         shared_lock                 = false;
    bool         exclusive_lock              = false;
    bool         keep_open_on_exec           = false;
    bool         allow_tty_assumption        = false;
    bool         open_symlink_not_target     = false;
    bool         allow_symlinked_targets     = true;
    bool         follow_symlinks_in_path     = true;
    bool         target_must_be_regular_file = false;
    bool         target_must_be_directory    = false;
    bool         target_must_be_link         = false;
    bool         target_must_be_special_file = false;

    n00b_karg_only_init(args);
    n00b_kw_ptr("filters", filters);
    n00b_kw_int32("permissions", permissions);
    n00b_kw_bool("allow_relative_paths", allow_relative_paths);
    n00b_kw_bool("allow_backtracking", allow_backtracking);
    n00b_kw_bool("normalize_paths", normalize_paths);
    n00b_kw_bool("error_if_exists", error_if_exists);
    n00b_kw_bool("allow_file_creation", allow_file_creation);
    n00b_kw_bool("truncate_on_open", truncate_on_open);
    n00b_kw_bool("writes_always_append", writes_always_append);
    n00b_kw_bool("shared_lock", shared_lock);
    n00b_kw_bool("exclusive_lock", exclusive_lock);
    n00b_kw_bool("keep_open_on_exec", keep_open_on_exec);
    n00b_kw_bool("allow_tty_assumption", allow_tty_assumption);
    n00b_kw_bool("open_symlink_not_target", open_symlink_not_target);
    n00b_kw_bool("allow_symlink_targets", allow_symlinked_targets);
    n00b_kw_bool("follow_symlinks_in_path", follow_symlinks_in_path);
    n00b_kw_bool("target_must_be_regular_file", target_must_be_regular_file);
    n00b_kw_bool("target_must_be_directory", target_must_be_directory);
    n00b_kw_bool("target_must_be_link", target_must_be_link);
    n00b_kw_bool("target_must_be_special_file", target_must_be_special_file);

    int relative_fd = 0;
    int flags;
    int mode;

    if (!n00b_string_codepoint_len(filename)) {
        N00B_CRAISE("Must provide a filename.");
    }

    if (read_only ^ write_only) {
        if (read_only) {
            flags = O_RDONLY;
        }
        else {
            flags = O_WRONLY;
        }
    }
    else {
        flags = O_RDWR;
    }

    mode = flags;

    if (permissions < 0 || permissions > 07777) {
        N00B_CRAISE("Invalid file permissions.");
    }

    // Implies creation.
    if (error_if_exists) {
        flags |= O_EXCL;
        flags |= O_CREAT;
    }

    if (allow_file_creation) {
        flags |= O_CREAT;
    }

    if (writes_always_append) {
        flags |= O_APPEND;
    }

    if (truncate_on_open) {
        if (flags & O_EXCL) {
            N00B_CRAISE("Cannot truncate when requiring a new file.");
        }
        flags |= O_TRUNC;
    }

    if (mode == O_RDONLY) {
        if (flags & O_CREAT) {
            N00B_CRAISE("Cannot specify creation for read-only files.");
        }
        if (flags & O_APPEND) {
            N00B_CRAISE("Cannot append without write permissions");
        }
    }

    if (!keep_open_on_exec) {
        flags |= O_CLOEXEC;
    }

    if (!allow_tty_assumption) {
        flags |= O_NOCTTY;
    }

    // This ifdef is because on some linux distros, libevent is
    // complaining about O_NONBLOCK even on stderr where it shouldn't
    // be.
#if defined(__linux__)
    flags |= O_NONBLOCK;
#endif

    if (target_must_be_directory) {
        flags |= O_DIRECTORY;
    }

    int sl_flags = 0;

#if !defined(__linux__)
    if (shared_lock) {
        flags |= O_SHLOCK;
        sl_flags++;
    }
#endif
    if (exclusive_lock) {
        if (shared_lock) {
            N00B_CRAISE("Cannot select both locking styles");
        }
        sl_flags++;
    }

    if (sl_flags > 1) {
        N00B_CRAISE("Cannot select both locking styles");
    }

    sl_flags = 0;

#if !defined(__linux__)
    if (open_symlink_not_target) {
        flags |= O_SYMLINK;
        sl_flags++;
    }
#endif

    if (!allow_symlinked_targets) {
        flags |= O_NOFOLLOW;
        sl_flags++;
    }

#if !defined(__linux__)
    if (!follow_symlinks_in_path) {
        flags |= O_NOFOLLOW_ANY;
        sl_flags++;
    }
#endif

    if (sl_flags > 1) {
        N00B_CRAISE("Conflicting symbolic linking policies provided.");
    }

    bool             path_is_relative;
    n00b_codepoint_t cp = (n00b_codepoint_t)(int64_t)n00b_index_get(filename,
                                                                    0);

    switch (cp) {
    case '/':
        path_is_relative = false;
        break;
    case '~':
        if (normalize_paths) {
            path_is_relative = false;
        }
        else {
            path_is_relative = true;
        }
        break;
    default:
        path_is_relative = false;
    }

    if (!allow_relative_paths && path_is_relative) {
        N00B_CRAISE("Relative paths are disallowed.");
    }

    if (!allow_backtracking) {
        n00b_list_t *parts = n00b_string_split(filename, n00b_cached_slash());
        int          n     = n00b_list_len(parts);

        for (int i = 0; i < n; i++) {
            n00b_string_t *s = n00b_list_get(parts, i, NULL);
            if (!strcmp(s->data, "..")) {
                N00B_CRAISE("Path backtracking is disabled.");
            }
        }
    }

    int fd;

    if (relative_fd) {
        if (flags & O_CREAT) {
            fd = openat(relative_fd, filename->data, flags, permissions);
        }
        else {
            fd = openat(relative_fd, filename->data, flags);
        }
    }
    else {
        if (normalize_paths) {
            filename = n00b_resolve_path(filename);
        }
        if (flags & O_CREAT) {
            fd = open(filename->data, flags, permissions);
        }
        else {
            fd = open(filename->data, flags);
        }
    }

    if (fd == -1) {
        n00b_raise_errno();
    }

    n00b_fd_stream_t *fds = n00b_fd_stream_from_fd(fd, NULL, NULL);
    fds->name             = filename;

    if (target_must_be_regular_file && !n00b_fd_is_regular_file(fds)) {
        N00B_CRAISE("Not a regular file.");
    }

    if (target_must_be_link && !n00b_fd_is_link(fds)) {
        N00B_CRAISE("Not a symbolic link.");
    }

    if (target_must_be_special_file && !n00b_fd_is_other(fds)) {
        N00B_CRAISE("Not a special file.");
    }

    return _n00b_new_fd_channel(fds, filters);
}
