#define N00B_USE_INTERNAL_API
#include "n00b.h"

static void
ev_timer_cb(evutil_socket_t ignore, short ignore2, void *info)
{
    n00b_stream_t *event = info;

    n00b_debug("timer", "[h6]tick!");
    n00b_post_to_subscribers(event, n00b_now(), n00b_io_sk_read);
}

n00b_stream_t *
n00b_io_timer_open(n00b_duration_t *duration, n00b_io_impl_info_t *impl)
{
    n00b_stream_base_t *base   = n00b_get_ev_base(impl);
    n00b_ev2_cookie_t *cookie = n00b_new_ev2_cookie();
    n00b_stream_t *new         = n00b_alloc_party(impl,
                                         cookie,
                                         n00b_io_perm_rw,
                                         n00b_io_ev_timer);

    cookie->read_event = event_new(base->event_ctx,
                                   -1,
                                   0,
                                   ev_timer_cb,
                                   new);
    cookie->aux        = n00b_duration_to_timeval(duration);

    return new;
}

static void *
n00b_io_timer_start(n00b_stream_t *timer, void *msg)
{
    struct timeval    *tv;
    n00b_ev2_cookie_t *cookie = timer->cookie;

    if (msg) {
        tv = n00b_duration_to_timeval(msg);
    }
    else {
        tv = cookie->aux;
    }

    n00b_stream_add(cookie->read_event, tv);

    return NULL;
}

static bool
n00b_io_timer_subscribe(n00b_stream_sub_t *info, n00b_io_subscription_kind kind)
{
    struct timeval    *d          = info->timeout;
    n00b_ev2_cookie_t *src_cookie = info->source->cookie;

    if (!d) {
        d = n00b_duration_to_timeval(src_cookie->aux);
    }

    n00b_stream_add(src_cookie->read_event, d);

    return true;
}

n00b_io_impl_info_t n00b_timer_impl = {
    .open_impl        = (void *)n00b_io_timer_open,
    .subscribe_impl   = n00b_io_timer_subscribe,
    .unsubscribe_impl = n00b_io_ev_unsubscribe,
    .write_impl       = n00b_io_timer_start,
    .repr_impl        = n00b_io_fd_repr,
    .use_libevent     = true,
};
