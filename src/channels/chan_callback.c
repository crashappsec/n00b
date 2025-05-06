#define N00B_USE_INTERNAL_API
#include "n00b.h"

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
    if (!block) {
#if N00B_MAYBE_ALLOW_NONBLOCK_READS
        n00b_lock_list(c->cref->read_cache);
        if (!n00b_list_len(c->cref->read_cache)) {
            *success = false;
            n00b_unlock_list(c->cref->read_cache);
            return NULL;
        }
        void *res = n00b_private_list_dequeue(c->ref->read_cache);
        n00b_unlock_list(c->cref->read_cache);
        *success = true;
        return res;
#else
        *success = false;
        return NULL;
#endif
    }

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

    while (cond) {
        n00b_condition_lock_acquire(cond);
        n00b_condition_notify_aux(cond, for_readers);
        n00b_condition_lock_release(cond);
        cond = n00b_private_list_pop(c->waiting_readers);
    }

    n00b_unlock_list(c->waiting_readers);

    // Generally, this is how the pub/sub bus gets messages to publish
    // to readers.  Without this, we will hang in certain situations where
    // we've subscribed an fd to read a callback.
    if (!c->cref->read_cache) {
        c->cref->read_cache = n00b_list(n00b_type_ref());
        n00b_list_append(c->cref->read_cache, for_readers);
    }

    n00b_channel_t *core = (void *)n00b_channel_core(c->cref);

    n00b_perform_channel_read(core, c->cref, false, 0);
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
    n00b_callback_channel_t *cookie = n00b_get_channel_cookie(result);
    cookie->cref                    = result;

    return result;
}
