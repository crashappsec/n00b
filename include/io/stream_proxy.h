#pragma once
#include "n00b.h"

typedef struct {
    n00b_stream_t *target;
    n00b_stream_t *read_cb;
    n00b_stream_t *close_cb;
} n00b_proxy_info_t;

extern n00b_stream_t *_n00b_new_stream_proxy(n00b_stream_t *, ...);
#define n00b_new_stream_proxy(other, ...) \
    _n00b_new_stream_proxy(other, __VA_ARGS__ __VA_OPT__(, ) 0ULL)
