#pragma once
#include "n00b.h"

extern n00b_string_t *_n00b_format(n00b_string_t *s, int nargs, ...);
extern n00b_string_t *n00b_format_arg_list(n00b_string_t *, n00b_list_t *);
extern n00b_string_t *n00b_rich(n00b_string_t *s);

#define n00b_format(fmt, ...) \
    _n00b_format(fmt, N00B_PP_NARG(__VA_ARGS__) __VA_OPT__(, ) __VA_ARGS__)

#define n00b_cformat(fmt, ...)      \
    _n00b_format(n00b_cstring(fmt), \
                 N00B_PP_NARG(__VA_ARGS__) __VA_OPT__(, ) __VA_ARGS__)

#define n00b_crich(x) n00b_rich(n00b_cstring(x))

// These are in fmt_numbers.h

extern n00b_string_t *n00b_fmt_hex(uint64_t, bool);
extern n00b_string_t *n00b_fmt_int(__int128_t, bool);
extern n00b_string_t *n00b_fmt_uint(uint64_t, bool);
extern n00b_string_t *n00b_fmt_float(double, int, bool);
extern n00b_string_t *n00b_fmt_bool(bool, bool, bool, bool);
extern n00b_string_t *n00b_fmt_codepoint(n00b_codepoint_t);
extern n00b_string_t *n00b_fmt_pointer(void *, bool);

#if defined(N00B_USE_INTERNAL_API)
extern int n00b_internal_fptostr(double, char[24]);
#endif
