#define N00B_USE_INTERNAL_API
#include "n00b.h"

// Subscription proxies. Used in the topic system.

static n00b_string_t *
n00b_io_subscription_proxy_repr(n00b_stream_t *e)
{
    return n00b_cstring("subscription proxy");
}

n00b_stream_t *
n00b_new_subscription_proxy(void)
{
    return n00b_alloc_party(&n00b_subproxy_impl,
                            NULL,
                            n00b_io_perm_rw,
                            n00b_io_ev_passthrough);
}

n00b_io_impl_info_t n00b_subproxy_impl = {
    .repr_impl = n00b_io_subscription_proxy_repr,
};
