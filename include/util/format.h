#include "n00b.h"
#pragma once
extern n00b_utf8_t *n00b_base_format(const n00b_str_t *, int, va_list);
extern n00b_utf8_t *n00b_str_vformat(const n00b_str_t *, n00b_dict_t *);
extern n00b_utf8_t *_n00b_str_format(const n00b_str_t *, int, ...);
extern n00b_utf8_t *_n00b_cstr_format(char *, int, ...);
extern n00b_utf8_t *n00b_cstr_array_format(char *, int, n00b_utf8_t **);

#define n00b_cstr_format(fmt, ...) \
    _n00b_cstr_format(fmt, N00B_PP_NARG(__VA_ARGS__) __VA_OPT__(, ) __VA_ARGS__)
#define n00b_str_format(fmt, ...) \
    _n00b_str_format(fmt, N00B_PP_NARG(__VA_ARGS__) __VA_OPT__(, ) __VA_ARGS__)

static inline n00b_utf8_t *
n00b_format_address(void *addr)
{
    char buf[20] = {
        0,
    };
    snprintf(buf, 19, "->%016llx", (uint64_t)addr);

    return n00b_new_utf8(buf);
}
