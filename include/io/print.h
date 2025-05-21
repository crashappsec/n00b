#pragma once
#include "n00b.h"

extern void _n00b_print(void *, ...);

#define n00b_print(s, ...) _n00b_print(s __VA_OPT__(, __VA_ARGS__))
#define n00b_eprint(...)   _n00b_print(n00b_stderr() __VA_OPT__(, __VA_ARGS__))

#define n00b_printf(fmt, ...)                                               \
    {                                                                       \
        n00b_string_t *__str = n00b_cformat(fmt __VA_OPT__(, __VA_ARGS__)); \
        _n00b_print(__str, NULL);                                           \
    }

#define n00b_eprintf(fmt, ...)                                              \
    {                                                                       \
        n00b_string_t *__str = n00b_cformat(fmt __VA_OPT__(, __VA_ARGS__)); \
        _n00b_print(n00b_stderr(), __str, NULL);                            \
    }

extern void *__n00b_c_value(void *item);
#define N00B_CVALUE(x) __n00b_c_value((void *)x)

#define n00b_va_map(fn, ...) N00B_MAP_LIST(fn, __VA_ARGS__)

#define cprintf(fmt, ...)                                            \
    fprintf(stderr,                                                  \
            fmt __VA_OPT__(, n00b_va_map(N00B_CVALUE, __VA_ARGS__)), \
            NULL)
