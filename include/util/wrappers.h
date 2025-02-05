#pragma once

#include "n00b.h"

extern n00b_string_t *n00b_wrapper_join(n00b_list_t *, n00b_string_t *);
extern n00b_string_t *n00b_wrapper_hostname(void);
extern n00b_string_t *n00b_wrapper_os(void);
extern n00b_string_t *n00b_wrapper_arch(void);
extern n00b_string_t *n00b_wrapper_repr(n00b_obj_t);
extern n00b_string_t *n00b_wrapper_to_str(n00b_obj_t);

static inline n00b_string_t *
n00b_compile_date(void)
{
    return n00b_cstring(__DATE__);
}
static inline n00b_string_t *
n00b_compile_time(void)
{
    return n00b_cstring(__TIME__);
}
