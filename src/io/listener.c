#define N00B_USE_INTERNAL_API
#include "n00b.h"

static void
io_accept_callback(struct evconnlistener *unused,
                   evutil_socket_t        fd,
                   struct sockaddr       *addr,
                   int                    socklen,
                   void                  *obj)
{
    defer_on();

    n00b_stream_t     *listener   = obj;
    n00b_stream_t     *connection = n00b_io_socket_open(fd, &n00b_socket_impl);
    n00b_ev2_cookie_t *lcookie    = listener->cookie;
    n00b_ev2_cookie_t *ccookie    = connection->cookie;
    n00b_listener_aux *aux        = lcookie->aux;
    n00b_list_t       *l;
    int                n;

    n00b_acquire_party(connection);

    ccookie->aux = n00b_new(n00b_type_net_addr(),
                            n00b_kw("sockaddr", addr));

    n00b_ignore_unseen_errors(connection);
    n00b_post_to_subscribers(listener, connection, n00b_io_sk_read);
    n00b_purge_subscription_list_on_boundary(listener->read_subs);

    l = aux->read_filters;
    n = n00b_list_len(l);

    for (int i = 0; i < n; i++) {
        n00b_filter_create_fn f = n00b_list_get(l, i, NULL);
        n00b_add_read_filter(connection, f(connection));
    }

    l = aux->write_filters;
    n = n00b_list_len(l);

    for (int i = 0; i < n; i++) {
        n00b_filter_create_fn f = n00b_list_get(l, i, NULL);
        n00b_add_write_filter(connection, f(connection));
    }

    if (aux->autosub) {
        n00b_io_subscribe(connection, aux->autosub, NULL, n00b_io_sk_read);
    }

    if (aux->accept_cb) {
        (*aux->accept_cb)(connection);
    }

    n00b_release_party(connection);
    defer_func_end();
}

n00b_stream_t *
n00b_io_listener(n00b_net_addr_t *addr,
                 n00b_stream_t   *autosubscribe,
                 void            *read_filters,
                 void            *write_filters,
                 n00b_accept_cb   cb)
{
    // autosubscribe is either a n00b_stream_t that can be written to, or a
    // list_t of n00b_stream_t objects.
    n00b_ev2_cookie_t *cookie = n00b_new_ev2_cookie();
    n00b_stream_t     *result = n00b_alloc_party(&n00b_listener_impl,
                                             cookie,
                                             n00b_io_perm_r,
                                             n00b_io_ev_listener);
    int                len    = n00b_get_sockaddr_len(addr);
    uint32_t           flags  = LEV_OPT_CLOSE_ON_FREE;

    flags |= LEV_OPT_CLOSE_ON_EXEC | LEV_OPT_REUSEABLE | LEV_OPT_THREADSAFE;

    n00b_listener_aux *aux = n00b_gc_alloc_mapped(n00b_listener_aux,
                                                  N00B_GC_SCAN_ALL);

    aux->server = evconnlistener_new_bind(n00b_system_event_base->event_ctx,
                                          io_accept_callback,
                                          result,
                                          flags,
                                          -1,
                                          (void *)(&addr->addr),
                                          len);

    cookie->aux    = aux;
    aux->addr      = addr;
    aux->accept_cb = cb;

    n00b_type_t *t;
    n00b_list_t *l;

    aux->autosub = autosubscribe;

    if (read_filters) {
        t = n00b_get_my_type(read_filters);
        if (n00b_type_is_list(t)) {
            l = read_filters;
        }
        else {
            l = n00b_list(n00b_type_ref());
            n00b_list_append(l, read_filters);
        }
        aux->read_filters = l;
    }

    if (write_filters) {
        t = n00b_get_my_type(write_filters);
        if (n00b_type_is_list(t)) {
            l = write_filters;
        }
        else {
            l = n00b_list(n00b_type_ref());
            n00b_list_append(l, write_filters);
        }
        aux->write_filters = l;
    }

    return result;
}

static n00b_string_t *
n00b_io_listener_repr(n00b_stream_t *listener)
{
    n00b_ev2_cookie_t *cookie = listener->cookie;
    n00b_listener_aux *aux    = cookie->aux;

    return n00b_cformat("«#»", aux->addr);
}

n00b_io_impl_info_t n00b_listener_impl = {
    .open_impl    = (void *)n00b_io_listener,
    .repr_impl    = (void *)n00b_io_listener_repr,
    .close_impl   = (void *)n00b_io_close,
    .use_libevent = true,
};
