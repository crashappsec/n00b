#pragma once

typedef struct {
    n00b_fd_stream_t *stream;
    n00b_fd_sub_t    *sub;
    bool              have_read_subscribers;
    n00b_buf_t       *read_cache;
    n00b_net_addr_t  *addr;
} n00b_fd_cookie_t;

extern n00b_channel_t *_n00b_new_fd_channel(n00b_fd_stream_t *fd, ...);
extern n00b_channel_t *_n00b_channel_open_file(n00b_string_t *filename, ...);
// In chan_listener.c
extern n00b_channel_t *_n00b_create_listener(n00b_net_addr_t *, ...);
extern n00b_channel_t *_n00b_channel_connect(n00b_net_addr_t *, ...);

#define n00b_new_fd_channel(fdstrm, ...)           \
    _n00b_new_fd_channel(fdstrm,                   \
                         N00B_PP_NARG(__VA_ARGS__) \
                             __VA_OPT__(, ) __VA_ARGS__)

#define n00b_channel_open_file(fname, ...) \
    _n00b_channel_open_file(fname, __VA_OPT__(n00b_ka(__VA_ARGS__), ) 0ULL)
#define n00b_create_listener(addr, ...) \
    _n00b_create_listener(addr, __VA_ARGS__ __VA_OPT__(, ) 0)
#define n00b_channel_connect(addr, ...) \
    _n00b_channel_connect(addr __VA_OPT__(, ) __VA_ARGS__)
