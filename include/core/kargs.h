/*
 * Copyright © 2021 John Viega
 *
 * See LICENSE.txt for licensing info.
 *
 * Name:          kargs.h
 * Description:   Support for limited keyword-style arguments in C functions.
 * Author:        John Viega, john@zork.org
 *
 * The 'provided' versions allow you to get a bitfield as to which
 * arguments were provided. That's for when you can't pre-assign an
 * invalid value to check.
 *
 */

#pragma once

#include "n00b.h"

typedef struct {
    n00b_alloc_record_t h; // not an n00b_alloc_hdr to avoid warnings.
    n00b_karg_info_t    ka;
    n00b_one_karg_t     args[N00B_MAX_KEYWORD_SIZE];
} n00b_static_karg_t;

n00b_karg_info_t *n00b_get_kargs(va_list);
n00b_karg_info_t *n00b_pass_kargs(int, ...);
n00b_karg_info_t *n00b_get_kargs_and_count(va_list, int *);

static inline bool
_n00b_kw_int64(n00b_karg_info_t *provided, char *name, int64_t *ptr)
{
    if (!provided) {
        return false;
    }

    int64_t n = provided->num_provided;

    for (int64_t i = 0; i < n; i++) {
        if (!strcmp(name, provided->args[i].kw)) {
            *ptr = (int64_t)provided->args[i].value;
            return true;
        }
    }

    return false;
}

static inline bool
_n00b_kw_ptr(n00b_karg_info_t *provided, char *name, void *ptr)
{
    return _n00b_kw_int64(provided, name, (int64_t *)ptr);
}

static inline bool
_n00b_kw_int32(n00b_karg_info_t *provided, char *name, int32_t *ptr)
{
    if (!provided) {
        return false;
    }

    int64_t n = provided->num_provided;
    int64_t tmp;

    for (int64_t i = 0; i < n; i++) {
        if (!strcmp(name, provided->args[i].kw)) {
            tmp = (int64_t)provided->args[i].value;

            *ptr = (int32_t)tmp;
            return true;
        }
    }

    return false;
}

static inline bool
_n00b_kw_int16(n00b_karg_info_t *provided, char *name, int16_t *ptr)
{
    if (!provided) {
        return false;
    }

    int64_t n = provided->num_provided;
    int64_t tmp;

    for (int64_t i = 0; i < n; i++) {
        if (!strcmp(name, provided->args[i].kw)) {
            tmp = (int64_t)provided->args[i].value;

            *ptr = (int16_t)tmp;
            return true;
        }
    }

    return false;
}

static inline bool
_n00b_kw_int8(n00b_karg_info_t *provided, char *name, int8_t *ptr)
{
    if (!provided) {
        return false;
    }

    int64_t n = provided->num_provided;
    int64_t tmp;

    for (int64_t i = 0; i < n; i++) {
        if (!strcmp(name, provided->args[i].kw)) {
            tmp = (int64_t)provided->args[i].value;

            *ptr = (int8_t)tmp;
            return true;
        }
    }

    return false;
}

static inline bool
_n00b_kw_bool(n00b_karg_info_t *provided, char *name, bool *ptr)
{
    if (!provided) {
        return false;
    }

    int64_t n = provided->num_provided;
    int64_t tmp;

    for (int64_t i = 0; i < n; i++) {
        if (!strcmp(name, provided->args[i].kw)) {
            tmp  = (int64_t)provided->args[i].value;
            *ptr = (bool)tmp;
            return true;
        }
    }

    return false;
}

static inline bool
_n00b_kw_float(n00b_karg_info_t *provided, char *name, double *ptr)
{
    if (!provided) {
        return false;
    }

    int64_t n = provided->num_provided;
    int64_t tmp;

    for (int64_t i = 0; i < n; i++) {
        if (!strcmp(name, provided->args[i].kw)) {
            tmp = (int64_t)provided->args[i].value;

            *ptr = (double)tmp;
            return true;
        }
    }

    return false;
}

#define n00b_kw_int64(a, b)     _n00b_kw_int64(_n00b_karg, a, (int64_t *)&b)
#define n00b_kw_uint64(a, b)    _n00b_kw_int64(_n00b_karg, a, (int64_t *)&b)
#define n00b_kw_ptr(a, b)       _n00b_kw_ptr(_n00b_karg, a, (void *)&b)
#define n00b_kw_int32(a, b)     _n00b_kw_int32(_n00b_karg, a, (int32_t *)&b)
#define n00b_kw_uint32(a, b)    _n00b_kw_int32(_n00b_karg, a, (int32_t *)&b)
#define n00b_kw_codepoint(a, b) _n00b_kw_int32(_n00b_karg, a, &b)
#define n00b_kw_int16(a, b)     _n00b_kw_int16(_n00b_karg, a, &b)
#define n00b_kw_uint16(a, b)    _n00b_kw_int16(_n00b_karg, a, (int16_t *)&b)
#define n00b_kw_char(a, b)      _n00b_kw_int8(_n00b_karg, a, (char *)&b)
#define n00b_kw_int8(a, b)      _n00b_kw_int8(_n00b_karg, a, (int8_t *)&b)
#define n00b_kw_unt8(a, b)      _n00b_kw_int8(_n00b_karg, a, (int8_t *)&b)
#define n00b_kw_bool(a, b)      _n00b_kw_bool(_n00b_karg, a, (bool *)&b)
#define n00b_kw_float(a, b)     _n00b_kw_float(_n00b_karg, a, (double_t *)&b)

// print(foo, bar, boz, kw("file", stdin, "sep", ' ', "end", '\n',
//                         "flush", false ));

#define n00b_ka(x) ((int64_t)x)

#ifdef N00B_DEBUG_KARGS
#define n00b_kw(...) n00b_pass_kargs(N00B_PP_NARG(__VA_ARGS__), \
                                     (char *)__FILE__,          \
                                     (int)__LINE__,             \
                                     __VA_ARGS__),              \
                     NULL
#else
#define n00b_kw(...) n00b_pass_kargs(N00B_PP_NARG(__VA_ARGS__), __VA_ARGS__), NULL
#endif
#define n00b_karg_only_init(last)                             \
    va_list _args;                                            \
    va_start(_args, last);                                    \
    n00b_karg_info_t *_n00b_karg = va_arg(_args, n00b_obj_t); \
    va_end(_args);

#define n00b_karg_va_init(list) \
    n00b_karg_info_t *_n00b_karg = va_arg(list, n00b_obj_t)
