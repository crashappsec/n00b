#pragma once

#if defined(__GLIBC__)
static inline va_list *
_n00b_build_va_list(void *arg1, ...)
{
    va_list local;
    va_list *result = n00b_gc_alloc_mapped(va_list, N00B_GC_SCAN_ALL);

    va_start(local, arg1);
    va_copy(*result, local);

    return result;
}

#define n00b_build_va_list(...) \
  (*_n00b_build_va_list(NULL, N00B_VA(__VA_ARGS__)))

#else
static inline va_list
_n00b_build_va_list(void *arg1, ...)
{
    va_list result;

    va_start(result, arg1);

    return result;
}

#define n00b_build_va_list(...) \
    _n00b_build_va_list(NULL, N00B_VA(__VA_ARGS__))

#endif

