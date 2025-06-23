#pragma once

#include "n00b.h"

typedef struct {
    char *kw;
    void *value;
} n00b_one_karg_t;

typedef struct {
    int32_t          num_provided;
    int32_t          used;
    n00b_one_karg_t *args;
} n00b_karg_info_t;

typedef struct {
    n00b_alloc_record_t h; // not an n00b_alloc_hdr to avoid warnings.
    n00b_karg_info_t    ka;
    n00b_one_karg_t     args[N00B_MAX_KEYWORD_SIZE];
} n00b_static_karg_t;

extern n00b_karg_info_t *_n00b_kargs_obj(char *, int64_t, ...);

#define n00b_kargs_obj(kw, val, ...) \
    _n00b_kargs_obj(kw, val, __VA_ARGS__ __VA_OPT__(, ) 0ULL)
