#pragma once
#include "n00b.h"

typedef struct {
    n00b_list_t *l;
    int          ix;
} n00b_string_channel_t;

extern n00b_channel_t *
_n00b_new_string_channel(n00b_string_t *, ...);

#define n00b_string_channel(string, ...) \
    _n00b_in_buf_channel(string, __VA_ARGS__ __VA_OPT__(, ) 0ULL)
