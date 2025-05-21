#pragma once
#include "n00b.h"

typedef struct {
    n00b_mutex_t internal_lock;
    int32_t      alloc_len;
    char        *data;
    char        *write_ptr;
    char        *read_ptr;
} n00b_bytering_t;

extern n00b_bytering_t *n00b_bytering_copy(n00b_bytering_t *);
extern int64_t          n00b_bytering_len(n00b_bytering_t *);
extern n00b_buf_t      *n00b_bytering_to_buffer(n00b_bytering_t *);
extern n00b_string_t   *n00b_bytering_to_utf8(n00b_bytering_t *);
extern void             n00b_bytering_resize(n00b_bytering_t *, int64_t);
extern int8_t           n00b_bytering_get_index(n00b_bytering_t *, int64_t);
extern bool             n00b_bytering_set_index(n00b_bytering_t *,
                                                int64_t,
                                                int8_t);
extern n00b_bytering_t *n00b_bytering_get_slice(n00b_bytering_t *,
                                                int64_t,
                                                int64_t);
extern void             n00b_set_slice(n00b_bytering_t *,
                                       int64_t,
                                       int64_t,
                                       n00b_bytering_t *);
extern n00b_bytering_t *n00b_bytering_copy(n00b_bytering_t *);
extern n00b_bytering_t *n00b_bytering_plus(n00b_bytering_t *,
                                           n00b_bytering_t *);
extern void             n00b_bytering_plus_eq(n00b_bytering_t *,
                                              n00b_bytering_t *);
extern void            *n00b_bytering_view(n00b_bytering_t *, uint64_t *);
extern void             n00b_bytering_truncate_end(n00b_bytering_t *,
                                                   int64_t);
extern void             n00b_bytering_truncate_front(n00b_bytering_t *,
                                                     int64_t);
// Write a string, buffer or bytering into an existing object at the
// write head, potentially dropping content.
extern void             n00b_bytering_write(n00b_bytering_t *, void *);
extern void             n00b_bytering_write_buffer(n00b_bytering_t *,
                                                   n00b_buf_t *);
extern void             n00b_bytering_write_str(n00b_bytering_t *,
                                                n00b_buf_t *);
extern void             n00b_bytering_write_bytering(n00b_bytering_t *,
                                                     n00b_bytering_t *);
// This does not grab the lock, and assumes we are iterating internally
// when we know we have the lock.
extern void             n00b_bytering_advance_ptr(n00b_bytering_t *r, char **ptrp);

static inline n00b_bytering_t *
n00b_string_to_bytering(n00b_string_t *s)
{
    return n00b_new(n00b_type_bytering(), n00b_kw("string", s));
}

static inline n00b_bytering_t *
n00b_buffer_to_bytering(n00b_buf_t *b)
{
    return n00b_new(n00b_type_bytering(), n00b_kw("buffer", b));
}
