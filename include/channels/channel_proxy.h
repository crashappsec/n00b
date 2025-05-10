#pragma once
#include "n00b.h"

typedef struct {
    n00b_channel_t *target;
    n00b_channel_t *read_cb;
    n00b_channel_t *close_cb;
} n00b_proxy_info_t;

extern n00b_channel_t *_n00b_new_channel_proxy(n00b_channel_t *, ...);
#define n00b_new_channel_proxy(other, ...) \
    _n00b_new_channel_proxy(other, __VA_ARGS__ __VA_OPT__(, ) 0ULL)
