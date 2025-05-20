#pragma once
#include "n00b.h"

typedef struct {
    n00b_list_t *l;
    int          ix;
} n00b_string_cookie_t;

extern n00b_stream_t *
_n00b_new_string_stream(void *, ...);

#define n00b_string_stream(input, ...) \
    _n00b_new_string_stream(input, __VA_ARGS__ __VA_OPT__(, ) 0ULL)
