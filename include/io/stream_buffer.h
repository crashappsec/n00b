#pragma once
#include "n00b.h"

typedef struct {
    n00b_buf_t *buffer;
    int         rposition;
    int         wposition;
} n00b_buffer_cookie_t;

extern n00b_stream_t *n00b_stream_from_buffer(n00b_buf_t *,
                                              int64_t,
                                              n00b_list_t *,
                                              bool);
extern n00b_stream_t *_n00b_in_buf_stream(n00b_buf_t *, ...);
extern n00b_stream_t *_n00b_out_buf_stream(n00b_buf_t *, bool, ...);
extern n00b_stream_t *_n00b_io_buf_stream(n00b_buf_t *, bool, ...);

#define n00b_instream_buffer(buffer, ...) \
    _n00b_in_buf_stream(buffer, __VA_ARGS__ __VA_OPT__(, ) 0ULL)
#define n00b_outstream_buffer(buffer, ...) \
    _n00b_out_buf_stream(buffer, __VA_ARGS__ __VA_OPT__(, ) 0ULL, 0ULL)
#define n00b_iostream_buffer(buffer, ...) \
    _n00b_io_buf_stream(buffer, __VA_ARGS__ __VA_OPT__(, ) 0ULL, 0ULL)
