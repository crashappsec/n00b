#pragma once
#include "n00b.h"

typedef struct {
    n00b_fd_stream_t *stream;
    n00b_fd_sub_t    *sub;
    n00b_net_addr_t  *addr;
} n00b_fd_cookie_t;

extern n00b_stream_t *n00b_stream_fd_open(int);
extern n00b_stream_t *_n00b_new_fd_stream(n00b_fd_stream_t *fd, ...);
extern n00b_stream_t *_n00b_stream_open_file(n00b_string_t *filename, ...);
extern void          *_n00b_read_file(n00b_string_t *, ...);
extern n00b_stream_t *_n00b_stream_connect(n00b_net_addr_t *, ...);
extern n00b_stream_t *_n00b_create_listener(n00b_net_addr_t *, ...);

#define n00b_new_fd_stream(fd, ...) \
    _n00b_new_fd_stream(fd, __VA_ARGS__ __VA_OPT__(, ) NULL)
#define n00b_stream_open_file(fname, ...) \
    _n00b_stream_open_file(fname, __VA_ARGS__ __VA_OPT__(, ) NULL)
#define n00b_read_file(x, ...) \
    _n00b_read_file(x, __VA_ARGS__ __VA_OPT__(, ) NULL)
#define n00b_read_file_to_buffer(filename, ...) \
    n00b_read_file(filename, n00b_header_kargs("buffer", 1ULL))
#define n00b_stream_connect(addr, ...) \
    _n00b_stream_connect(addr, __VA_ARGS__ __VA_OPT__(, ) NULL)
#define n00b_create_listener(addr, ...) \
    _n00b_create_listener(addr, __VA_ARGS__ __VA_OPT__(, ) NULL)

#ifdef N00B_USE_INTERNAL_API
extern bool n00b_fd_stream_close(n00b_stream_t *);
extern void n00b_fd_stream_on_first_subscriber(n00b_stream_t *,
                                               n00b_fd_cookie_t *);
extern void n00b_fd_stream_on_no_subscribers(n00b_stream_t *,
                                             n00b_fd_cookie_t *);

#endif
