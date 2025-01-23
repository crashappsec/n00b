#define N00B_USE_INTERNAL_API
#include "n00b.h"

n00b_stream_t *
n00b_condition_open(n00b_condition_t *cv, void *aux)
{
    n00b_cv_cookie_t *cookie = n00b_gc_alloc_mapped(n00b_cv_cookie_t,
                                                    N00B_GC_SCAN_ALL);
    cookie->condition        = cv;
    cookie->aux              = aux;
    n00b_stream_t *res       = n00b_alloc_party(&n00b_condition_impl,
                                          cookie,
                                          n00b_io_perm_w,
                                          n00b_io_ev_condition);

    n00b_debug("cv", n00b_rich_lit("[h6]Condition opened"));

    return res;
}

static void *
n00b_io_condition_notify(n00b_stream_t *ev, void *msg)
{
    n00b_cv_cookie_t *cookie = ev->cookie;

    if (cookie->aux) {
        n00b_condition_notify_one(cookie->condition, cookie->aux);
    }
    else {
        n00b_condition_notify_one(cookie->condition, msg);
    }

    return NULL;
}

static n00b_utf8_t *
n00b_io_condition_repr(n00b_stream_t *ev)
{
    n00b_cv_cookie_t *cookie = ev->cookie;

    return n00b_cstr_format("condition @{:x}", cookie->condition);
}

n00b_io_impl_info_t n00b_condition_impl = {
    .open_impl  = (void *)n00b_condition_open,
    .write_impl = n00b_io_condition_notify,
    .repr_impl  = n00b_io_condition_repr,
};
