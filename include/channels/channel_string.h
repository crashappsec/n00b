#pragma once
#include "n00b.h"

typedef struct {
    n00b_list_t *l;
    int          ix;
} n00b_string_channel_t;

extern n00b_channel_t *
_n00b_new_string_channel(void *, ...);

#define n00b_string_channel(input, ...) \
    _n00b_new_string_channel(input, __VA_ARGS__ __VA_OPT__(, ) 0ULL)
