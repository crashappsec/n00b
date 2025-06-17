// Not really much has to happen here; the core event implementation
// handles the accept for us, as long as we mark that it's a 'server'.
//
// We may also add a 'read' implementation that supports 'blocking'
// accepts. However, this does seem to require (at least on some
// platforms) flipping between blocking / non-blocking, as poll()
// doesn't work w/ server sockets unless they're in non-blocking
// mode. We can prob make it work, but given it'd only be for
// regularity, no big deal / hurry.

#define N00B_USE_INTERNAL_API
#include "n00b.h"

static inline void
enable_opt(int socket, int opt)
{
    const int64_t truth_value = 1;
    if (setsockopt(socket, SOL_SOCKET, opt, &truth_value, sizeof(int64_t))) {
        n00b_raise_errno();
    }
}

extern void
fd_stream_on_read_event(n00b_fd_stream_t *s,
                        n00b_fd_sub_t    *sub,
                        void             *msg,
                        void             *thunk);

extern void
on_fd_close(n00b_fd_stream_t *s, n00b_stream_t *c);

static int
listener_open(n00b_stream_t *stream, n00b_list_t *args)
{
    int               sock;
    n00b_net_addr_t  *addr    = n00b_list_pop(args);
    int               backlog = (int64_t)n00b_list_pop(args);
    n00b_fd_cookie_t *c       = n00b_get_stream_cookie(stream);
    int               len     = n00b_get_sockaddr_len(addr);

    sock = socket(n00b_get_net_addr_family(addr), SOCK_STREAM, 0);

    if (sock == -1) {
        n00b_raise_errno();
    }

    fcntl(sock, F_SETFL, O_NONBLOCK);

    enable_opt(sock, SO_REUSEADDR);
    enable_opt(sock, SO_KEEPALIVE);
#if defined(__APPLE__) || defined(__FreeBSD__)
    enable_opt(sock, SO_NOSIGPIPE);
#endif

    if (bind(sock, (void *)&addr->addr, len)) {
        n00b_raise_errno();
    }

    if (listen(sock, backlog)) {
        n00b_raise_errno();
    }

    c->addr             = addr;
    c->stream           = n00b_fd_stream_from_fd(sock, NULL, NULL);
    c->stream->listener = true;

    stream->fd_backed = true;
    stream->name      = n00b_cformat("listen: [|#|] (fd [|#|])",
                                addr,
                                (int64_t)sock);

    c->sub = _n00b_fd_read_subscribe(c->stream,
                                     (void *)fd_stream_on_read_event,
                                     2,
                                     (int)(false),
                                     stream);
    _n00b_fd_close_subscribe(c->stream,
                             (void *)on_fd_close,
                             2,
                             (int)false,
                             stream);

    return O_RDONLY;
}

static n00b_stream_impl listener_impl = {
    .cookie_size = sizeof(n00b_fd_cookie_t),
    .init_impl   = (void *)listener_open,
    .close_impl  = (void *)n00b_fd_stream_close,
};

n00b_stream_t *
_n00b_create_listener(n00b_net_addr_t *addr, ...)
{
    int          backlog;
    n00b_list_t *params = n00b_list(n00b_type_ref());
    va_list      args;

    va_start(args, addr);
    backlog = va_arg(args, int);

    if (backlog <= 0) {
        backlog = N00B_SOCK_LISTEN_BACKLOG_DEFAULT;
    }

    n00b_list_append(params, (void *)(int64_t)backlog);
    n00b_list_append(params, addr);

    return n00b_new(n00b_type_stream(), &listener_impl, params);
}
