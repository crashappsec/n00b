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

static thread_local n00b_karg_info_t *kcache[N00B_MAX_KARGS_NESTING_DEPTH];
static thread_local int              kargs_next_entry = 0;

const int kargs_cache_mod = N00B_MAX_KARGS_NESTING_DEPTH - 1;

static thread_local bool init_kargs = false;

static n00b_karg_info_t *
n00b_kargs_acquire()
{
    if (!init_kargs) {
        n00b_gc_register_root(kcache, N00B_MAX_KARGS_NESTING_DEPTH);

        for (int i = 0; i < N00B_MAX_KARGS_NESTING_DEPTH; i++) {
            n00b_karg_info_t *karg = n00b_gc_alloc_mapped(n00b_karg_info_t,
                                                        N00B_GC_SCAN_ALL);
            n00b_alloc_hdr   *h    = n00b_object_header(karg);
            h->type               = n00b_type_kargs();
            h->n00b_obj          = true;

            karg->args = n00b_gc_array_alloc(n00b_one_karg_t,
                                            N00B_MAX_KEYWORD_SIZE);
            kcache[i]  = karg;
        }
        init_kargs = true;
    }

    kargs_next_entry &= kargs_cache_mod;

    n00b_karg_info_t *result = kcache[kargs_next_entry++];

    return result;
}

n00b_karg_info_t *
n00b_pass_kargs(int nargs, ...)
{
    va_list args;

    va_start(args, nargs);

#ifdef N00B_DEBUG_KARGS
    char *fn   = va_arg(args, char *);
    int   line = va_arg(args, int);

    printf("kargs: %d args called from n00b_kw() on %s:%d\n", nargs, fn, line);
#endif

    if (nargs & 1) {
        N00B_CRAISE(
            "Got an odd number of parameters to kw() keyword decl macro.");
    }

    nargs >>= 1;
    n00b_karg_info_t *kargs = n00b_kargs_acquire();

    kargs->num_provided = nargs;

    assert(nargs < N00B_MAX_KEYWORD_SIZE);
    assert(kargs->num_provided == nargs);

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
    va_list   arg_copy;

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
    va_list   arg_copy;
    n00b_obj_t cur;
    int       count = 0;

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
