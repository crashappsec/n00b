#pragma once

#include "n00b.h"

extern n00b_buf_t  *n00b_buffer_add(n00b_buf_t *, n00b_buf_t *);
extern n00b_buf_t  *n00b_buffer_join(n00b_list_t *, n00b_buf_t *);
extern int64_t     n00b_buffer_len(n00b_buf_t *);
extern void        n00b_buffer_resize(n00b_buf_t *, uint64_t);
extern n00b_utf8_t *n00b_buf_to_utf8_string(n00b_buf_t *);

static inline n00b_buf_t *
n00b_buffer_empty()
{
    return n00b_new(n00b_type_buffer(), n00b_kw("length", n00b_ka(0)));
}
