#pragma once

#include "n00b.h"

typedef struct {
    char         *data;
    int32_t       flags;
    int32_t       byte_len;
    int32_t       alloc_len;
    n00b_rwlock_t lock;
} n00b_buf_t;

extern n00b_buf_t           *n00b_buffer_add(n00b_buf_t *, n00b_buf_t *);
extern n00b_buf_t           *n00b_buffer_join(n00b_list_t *, n00b_buf_t *);
extern int64_t               n00b_buffer_len(n00b_buf_t *);
extern void                  n00b_buffer_resize(n00b_buf_t *, uint64_t);
extern n00b_string_t        *n00b_buf_to_utf8_string(n00b_buf_t *);
extern n00b_string_t        *n00b_buffer_to_hex_str(n00b_buf_t *buf);
extern int64_t               _n00b_buffer_find(n00b_buf_t *, n00b_buf_t *, ...);
extern char                 *n00b_buffer_to_c(n00b_buf_t *, int64_t *);
extern n00b_buf_t           *n00b_buffer_from_codepoint(n00b_codepoint_t);
static inline n00b_buf_t    *n00b_buffer_empty(void);
static inline void           n00b_buffer_release(n00b_buf_t *b);
static inline n00b_string_t *n00b_buf_to_string(n00b_buf_t *b);
static inline n00b_buf_t    *n00b_string_to_buffer(n00b_string_t *s);
static inline n00b_buf_t    *n00b_buffer_from_bytes(char *bytes, int64_t len);

#define n00b_buffer_find(buf_main, sub, ...) \
    _n00b_buffer_find(buf_main, sub, N00B_VA(__VA_ARGS__))
