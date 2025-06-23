#pragma once
#include "n00b.h"

static inline n00b_bytering_t *
n00b_string_to_bytering(n00b_string_t *s)
{
    return n00b_new(n00b_type_bytering(),
                    n00b_header_kargs("string", (int64_t)s));
}

static inline n00b_bytering_t *
n00b_buffer_to_bytering(n00b_buf_t *b)
{
    return n00b_new(n00b_type_bytering(),
                    n00b_header_kargs("buffer", (int64_t)b));
}
