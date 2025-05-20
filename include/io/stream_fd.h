#pragma once
#include "n00b.h"

typedef struct {
    n00b_fd_stream_t *stream;
    n00b_fd_sub_t    *sub;
    n00b_net_addr_t  *addr;
} n00b_fd_cookie_t;

extern n00b_stream_t *_n00b_new_fd_stream(n00b_fd_stream_t *fd, ...);
extern n00b_stream_t *_n00b_stream_open_file(n00b_string_t *filename, ...);
extern n00b_stream_t *n00b_stream_fd_open(int);
extern void          *_n00b_read_file(n00b_string_t *, ...);

#define n00b_read_file(filename, ...) \
    _n00b_read_file(filename, N00B_VA(n00b_kw(__VA_ARGS__)))

#define n00b_read_file_to_buffer(filename, ...) \
    _n00b_read_file(filename, n00b_kw("buffer", n00b_ka(true)))

extern n00b_stream_t *_n00b_create_listener(n00b_net_addr_t *, ...);
extern n00b_stream_t *_n00b_stream_connect(n00b_net_addr_t *, ...);

#define n00b_new_fd_stream(fdstrm, ...) \
    _n00b_new_fd_stream(fdstrm, N00B_VA(__VA_ARGS__));

#define n00b_stream_open_file(fname, ...) \
    _n00b_stream_open_file(fname, N00B_VA(n00b_kw(__VA_ARGS__)))

#define n00b_create_listener(addr, ...) \
    _n00b_create_listener(addr, N00B_VA(__VA_ARGS__))
#define n00b_stream_connect(addr, ...) \
    _n00b_stream_connect(addr, N00B_VA(__VA_ARGS__))

static inline int
n00b_stream_fileno(n00b_stream_t *s)
{
    if (!s->fd_backed) {
        return -1;
    }

    n00b_fd_cookie_t *cookie = (void *)s->cookie;
    return cookie->stream->fd;
}

static inline n00b_net_addr_t *
n00b_stream_net_address(n00b_stream_t *s)
{
    if (!s->fd_backed) {
        return NULL;
    }

    n00b_fd_cookie_t *cookie = (void *)s->cookie;
    return cookie->addr;
}

#ifdef N00B_USE_INTERNAL_API
extern bool n00b_fd_stream_close(n00b_stream_t *);
extern void n00b_fd_stream_on_first_subscriber(n00b_stream_t *,
                                               n00b_fd_cookie_t *);
extern void n00b_fd_stream_on_no_subscribers(n00b_stream_t *,
                                             n00b_fd_cookie_t *);
#endif
