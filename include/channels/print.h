#pragma once
#include "n00b.h"

extern void _n00b_print(void *, ...);

#define n00b_print(s, ...) _n00b_print(s __VA_OPT__(, __VA_ARGS__))
#define n00b_eprint(...)   _n00b_print(n00b_chan_stderr() __VA_OPT__(, __VA_ARGS__))

#define n00b_printf(fmt, ...)                                               \
    {                                                                       \
        n00b_string_t *__str = n00b_cformat(fmt __VA_OPT__(, __VA_ARGS__)); \
        _n00b_print(__str, NULL);                                           \
    }

#define n00b_eprintf(fmt, ...)                                              \
    {                                                                       \
        n00b_string_t *__str = n00b_cformat(fmt __VA_OPT__(, __VA_ARGS__)); \
        _n00b_print(n00b_chan_stderr(), __str, NULL);                       \
    }

#ifdef N00B_DEBUG
void _n00b_cprintf(char *, int64_t, ...);

#define cprintf(fmt, ...) \
    _n00b_cprintf(fmt, N00B_PP_NARG(__VA_ARGS__) __VA_OPT__(, __VA_ARGS__))
#endif
