#pragma once
#include "n00b.h"

static inline void *
n00b_double_to_ptr(double d)
{
    union {
        double  d;
        int64_t i;
    } u;

    u.d = d;

    return (void *)(u.i);
}

static inline double
n00b_ptr_to_double(void *p)
{
    union {
        double  d;
        int64_t i;
    } u;

    u.i = (int64_t)p;

    return u.d;
}
