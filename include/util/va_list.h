#pragma once

static inline va_list
_n00b_build_va_list(void *arg1, ...)
{
    va_list result;

    va_start(result, arg1);

    return result;
}

#define n00b_build_va_list(...) \
    _n00b_build_va_list(NULL, N00B_VA(__VA_ARGS__))
