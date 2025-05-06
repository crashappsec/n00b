#pragma once
#include "n00b.h"

typedef struct {
    n00b_buf_t *buffer;
    int         position;
} n00b_buffer_channel_t;

extern n00b_channel_t *n00b_channel_from_buffer(n00b_buf_t *, int64_t, va_list);
extern n00b_channel_t *_n00b_in_buf_channel(n00b_buf_t *b, ...);
extern n00b_channel_t *_n00b_out_buf_channel(n00b_buf_t *b, ...);
extern n00b_channel_t *_n00b_io_buf_channel(n00b_buf_t *b, ...);

#define n00b_in_buf_channel(buffer, ...) \
    _n00b_in_buf_channel(buffer,         \
                         N00B_PP_NARG(__VA_ARGS__) __VA_OPT__(, ) __VA_ARGS__)

#define n00b_out_buf_channel(buffer, ...) \
    _n00b_out_buf_channel(buffer,         \
                          N00B_PP_NARG(__VA_ARGS__) __VA_OPT__(, ) __VA_ARGS__)
#define n00b_io_buf_channel(buffer, ...) \
    _n00b_in_buf_channel(buffer,         \
                         N00B_PP_NARG(__VA_ARGS__) __VA_OPT__(, ) __VA_ARGS__)
