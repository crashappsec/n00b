#define N00B_USE_INTERNAL_API
#include "n00b.h"

n00b_stream_t *
n00b_io_socket_open(int64_t fd, n00b_io_impl_info_t *impl)
{
    defer_on();

    evutil_socket_t     handle = fd;
    n00b_stream_base_t *base   = n00b_get_ev_base(impl);
    n00b_ev2_cookie_t  *cookie = n00b_new_ev2_cookie();
    n00b_stream_t *new         = n00b_alloc_party(impl,
                                          cookie,
                                          n00b_io_perm_rw,
                                          n00b_io_ev_socket);

    cookie->socket = true;
    cookie->id     = fd;

    evutil_make_socket_nonblocking(handle);
    n00b_acquire_party(new);
    n00b_stream_t *result = n00b_add_or_replace(new,
                                                base->io_fd_cache,
                                                (void *)fd);

    if (result != new) {
        n00b_release_party(new);
        Return result;
    }

    cookie->read_event  = event_new(base->event_ctx,
                                   handle,
                                   EV_READ | EV_PERSIST,
                                   n00b_ev2_r,
                                   result);
    cookie->write_event = event_new(base->event_ctx,
                                    handle,
                                    EV_WRITE | EV_ET,
                                    n00b_ev2_w,
                                    result);
    n00b_release_party(result);
    Return result;
    defer_func_end();
}

static n00b_string_t *
n00b_io_socket_repr(n00b_stream_t *e)
{
    n00b_ev2_cookie_t *cookie = e->cookie;

    return n00b_cformat("socket on fd «#»«#»",
                        cookie->id,
                        n00b_get_fd_extras(e));
}

n00b_stream_t *
n00b_connect(n00b_net_addr_t *addr)
{
    int fd = socket(addr->family, SOCK_STREAM, 0);
    if (fd < 0) {
        n00b_raise_errno();
    }

    if (connect(fd, (void *)&addr->addr, n00b_get_sockaddr_len(addr))) {
        return NULL;
    }

    return n00b_fd_open(fd);
}

n00b_stream_t *
n00b_raw_connect(n00b_net_addr_t *addr)
{
    int fd = socket(addr->family, SOCK_RAW, 0);
    if (fd < 0) {
        n00b_raise_errno();
    }

    if (connect(fd, (void *)&addr->addr, n00b_get_sockaddr_len(addr))) {
        return NULL;
    }

    return n00b_fd_open(fd);
}

n00b_io_impl_info_t n00b_socket_impl = {
    .open_impl           = (void *)n00b_io_socket_open,
    .subscribe_impl      = n00b_io_fd_subscribe,
    .unsubscribe_impl    = n00b_io_ev_unsubscribe,
    .read_impl           = n00b_io_enqueue_fd_read,
    .write_impl          = n00b_io_enqueue_fd_write,
    .blocking_write_impl = n00b_io_fd_sync_write,
    .repr_impl           = n00b_io_socket_repr,
    .close_impl          = n00b_io_close,
    .use_libevent        = true,
    .byte_oriented       = true,
};
