/*
 * Copyright © 2021 John Viega
 *
 * See LICENSE.txt for licensing info.
 *
 * Name:          kargs.h
 * Description:   Support for limited keyword-style arguments in C functions.
 * Author:        John Viega, john@zork.org
 */

#include "n00b.h"

const int kargs_cache_mod = N00B_MAX_KARGS_NESTING_DEPTH - 1;

static inline n00b_karg_info_t *
n00b_kargs_acquire(void)
{
    n00b_tsi_t *tsi = n00b_get_tsi_ptr();

    if (!tsi->init_kargs) {
        n00b_static_karg_t *ska = &tsi->kcache;
        ska->h.type             = n00b_type_kargs();
        ska->h.n00b_obj         = true;
        ska->ka.args            = ska->args;
        tsi->init_kargs         = true;
    }

    n00b_karg_info_t *tsi_ka = &tsi->kcache.ka;

    if (tsi_ka->used) {
        n00b_karg_info_t *ki = n00b_gc_flex_alloc(n00b_karg_info_t,
                                                  n00b_one_karg_t,
                                                  N00B_MAX_KEYWORD_SIZE,
                                                  N00B_GC_SCAN_ALL);

        ki->args = (void *)(((char *)ki) + sizeof(n00b_one_karg_t));

        return ki;
    }

    return tsi_ka;
}

// This is for varargs functions, so it def needs to copy the va_list.
n00b_karg_info_t *
n00b_get_kargs_and_count(va_list args, int *nargs)
{
    va_list arg_copy;
    void   *cur;
    int     count = 0;

    va_copy(arg_copy, args);

    cur = va_arg(arg_copy, void *);

    while (cur != NULL) {
        if (n00b_get_my_type(cur) == n00b_type_kargs()) {
            *nargs = count;
            va_end(arg_copy);
            return cur;
        }

        count++;
        cur = va_arg(arg_copy, void *);
    }

    *nargs = count;
    va_end(arg_copy);
    return NULL;
}

// We automatically cast (in ncpp) to int64_t, so that both integer and pointer
// types will cast to it, even though we then store as 'void *', since it is
// more appropriate in C for a 'mixed' type.
n00b_karg_info_t *
_n00b_kargs_obj(char *kw, int64_t val, ...)
{
    int     i = 0;
    va_list args;
    va_start(args, val);

    n00b_karg_info_t *result = n00b_kargs_acquire();
    result->used             = true;

    while (true) {
        result->args[i++] = (n00b_one_karg_t){.kw = kw, .value = (void *)val};
        kw                = va_arg(args, char *);
        if (!kw) {
            result->num_provided = i;
            return result;
        }
        val = va_arg(args, int64_t);
    }
}
