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

static n00b_karg_info_t *
n00b_kargs_acquire()
{
    n00b_tsi_t *tsi = n00b_get_tsi_ptr();

    if (!tsi->init_kargs) {
        for (int i = 0; i < N00B_MAX_KARGS_NESTING_DEPTH; i++) {
            n00b_static_karg_t *ska = &tsi->kcache[i];
            ska->h.type             = n00b_type_kargs();
            ska->h.n00b_obj         = true;
            ska->ka.args            = ska->args;
        }
        tsi->init_kargs = true;
    }

    n00b_karg_info_t *result = &tsi->kcache[tsi->kargs_next_entry++].ka;
    tsi->kargs_next_entry &= kargs_cache_mod;

    return result;
}

n00b_karg_info_t *
n00b_pass_kargs(int nargs, ...)
{
    va_list           args;
    n00b_karg_info_t *kargs;

    va_start(args, nargs);

#ifdef N00B_DEBUG_KARGS
    char *fn   = va_arg(args, char *);
    int   line = va_arg(args, int);

    printf("kargs: %d args called from n00b_kw() on %s:%d\n", nargs, fn, line);
#endif

    if (nargs & 1) {
        if (nargs > 1) {
            N00B_CRAISE(
                "Got an odd number of parameters to kw() keyword decl macro.");
        }
        kargs               = n00b_kargs_acquire();
        kargs->num_provided = 0;
        return kargs;
    }

    nargs >>= 1;
    kargs = n00b_kargs_acquire();

    kargs->num_provided = nargs;

    n00b_assert(nargs < N00B_MAX_KEYWORD_SIZE);
    n00b_assert(kargs->num_provided == nargs);

    for (int i = 0; i < nargs; i++) {
        kargs->args[i].kw    = va_arg(args, char *);
        kargs->args[i].value = va_arg(args, void *);
    }

    va_end(args);

    return kargs;
}

n00b_karg_info_t *
n00b_get_kargs(va_list args)
{
    n00b_obj_t cur;
    va_list    arg_copy;

    va_copy(arg_copy, args);

    cur = va_arg(arg_copy, n00b_obj_t);

    while (cur != NULL) {
        if (n00b_get_my_type(cur) == n00b_type_kargs()) {
            va_end(arg_copy);
            return cur;
        }

        cur = va_arg(arg_copy, n00b_obj_t);
    }

    va_end(arg_copy);
    return NULL;
}

// This is for varargs functions, so it def needs to copy the va_list.
n00b_karg_info_t *
n00b_get_kargs_and_count(va_list args, int *nargs)
{
    va_list    arg_copy;
    n00b_obj_t cur;
    int        count = 0;

    va_copy(arg_copy, args);

    cur = va_arg(arg_copy, n00b_obj_t);

    while (cur != NULL) {
        if (n00b_get_my_type(cur) == n00b_type_kargs()) {
            *nargs = count;
            va_end(arg_copy);
            return cur;
        }

        count++;
        cur = va_arg(arg_copy, n00b_obj_t);
    }

    *nargs = count;
    va_end(arg_copy);
    return NULL;
}
