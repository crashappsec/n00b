#pragma once
#include "n00b.h"

typedef n00b_list_t *n00b_va_t;

static inline n00b_va_t
n00b_sized_va_list(int32_t n, ...)
{
    va_list args;
    va_start(args, n);

    if (!n) {
        return NULL;
    }

    void        *item   = va_arg(args, void *);
    n00b_list_t *result = n00b_list(n00b_type_ref());
    n00b_alloc_hdr *h      = (n00b_alloc_hdr *)result;
    n00b_list_append(result, item);

    h[-1].type = n00b_type_va_list();
    
    for (int i = 1; i < n; i++) {
        item = va_arg(args, void *);
        n00b_list_append(result, item);
    }

    return (n00b_va_t)result;
}

static inline n00b_va_t
n00b_null_terminated_va_list(void *item, ...)
{
    if (!item) {
        return NULL;
    }
    n00b_list_t    *result = n00b_list(n00b_type_ref());
    n00b_alloc_hdr *h      = (n00b_alloc_hdr *)result;
    n00b_list_append(result, item);
    va_list args;

    h[-1].type = n00b_type_va_list();

    va_start(args, item);

    item = va_arg(args, void *);
    while (item) {
        n00b_list_append(result, item);
        item = va_arg(args, void *);
    }

    return (n00b_va_t)result;
}

static inline n00b_va_t
n00b_va_list_to_n00b_va(va_list args, bool sized)
{
    n00b_list_t    *result = n00b_list(n00b_type_ref());
    n00b_alloc_hdr *h      = (n00b_alloc_hdr *)result;    
    int32_t         num_items;

    h[-1].type = n00b_type_va_list();
    
    if (sized) {
        num_items = va_arg(args, int32_t);
        if (!num_items) {
            return NULL;
        }
        for (int i = 0; i < num_items; i++) {
            n00b_list_append(result, va_arg(args, void *));
        }
    }
    else {
        void *item = va_arg(args, void *);
        num_items  = 0;

        while (item) {
            num_items++;
            n00b_list_append(result, item);
            item = va_arg(args, void *);
        }
    }

    va_end(args);

    return result;
}

#define n00b_va_list(...) \
    n00b_size_va_list(N00B_PP_NARG(__VA_ARGS__) __VA_OPT__(,) __VA_ARGS__)
