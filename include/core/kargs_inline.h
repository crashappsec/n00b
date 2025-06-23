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

// Everything in this file is part of the *old* keyword argument
// system.  It's still here because there are two places where the
// preprocessor doesn't yet have a syntax to support the
// semantics. The big one is printf(), which has optional positional
// arguments, but anything with even required positional arguments
// that should get passed via a va_list (as can happen with
// constructor calls via n00b_new()) requires this old interface at
// the moment.

extern n00b_karg_info_t *n00b_get_kargs_and_count(va_list, int *);

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

#define n00b_kw_int64(a, b)     _n00b_kw_int64(_n00b_karg, a, (int64_t *)&b)
#define n00b_kw_ptr(a, b)       _n00b_kw_ptr(_n00b_karg, a, (void *)&b)
#define n00b_kw_codepoint(a, b) _n00b_kw_int32(_n00b_karg, a, &b)
#define n00b_kw_bool(a, b)      _n00b_kw_bool(_n00b_karg, a, (bool *)&b)

#define n00b_header_kargs(x, y, ...) n00b_kargs_obj(x, y, N00B_VA(__VA_ARGS__))

#define n00b_karg_only_init(last)                         \
    va_list _args;                                        \
    va_start(_args, last);                                \
    n00b_karg_info_t *_n00b_karg = va_arg(_args, void *); \
    va_end(_args)

#define n00b_karg_va_init(list) \
    n00b_karg_info_t *_n00b_karg = va_arg(list, void *)
