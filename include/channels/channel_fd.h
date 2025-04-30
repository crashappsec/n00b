#pragma once
#include "n00b.h"

typedef struct {
    n00b_fd_stream_t *stream;
    n00b_fd_sub_t    *sub;
    n00b_net_addr_t  *addr;
} n00b_fd_cookie_t;

extern n00b_channel_t *_n00b_new_fd_channel(n00b_fd_stream_t *fd, ...);
extern n00b_channel_t *_n00b_channel_open_file(n00b_string_t *filename, ...);
// In chan_listener.c
extern n00b_channel_t *_n00b_create_listener(n00b_net_addr_t *, ...);
extern n00b_channel_t *_n00b_channel_connect(n00b_net_addr_t *, ...);

#define n00b_new_fd_channel(fdstrm, ...) \
    _n00b_new_fd_channel(fdstrm, __VA_ARGS__ __VA_OPT__(, ) 0ULL)

#define n00b_channel_open_file(fname, ...) \
    _n00b_channel_open_file(fname, n00b_kw(__VA_ARGS__))
#define n00b_create_listener(addr, ...) \
    _n00b_create_listener(addr, __VA_ARGS__ __VA_OPT__(, ) 0ULL)
#define n00b_channel_connect(addr, ...) \
    _n00b_channel_connect(addr, __VA_ARGS__ __VA_OPT__(, ) 0ULL)

#ifdef N00B_USE_INTERNAL_API
extern bool n00b_fdchan_close(n00b_channel_t *);
extern void n00b_fdchan_on_first_subscriber(n00b_channel_t *,
                                            n00b_fd_cookie_t *);
extern void n00b_fdchan_on_no_subscribers(n00b_channel_t *,
                                          n00b_fd_cookie_t *);
#endif
