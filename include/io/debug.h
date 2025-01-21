#pragma once
#include "n00b.h"

extern void _n00b_debug(void *, void *, ...);
#define n00b_debug(x, y, ...) _n00b_debug(x, y, __VA_ARGS__ __VA_OPT__(, ) NULL)

typedef bool (*n00b_debug_filter)(n00b_utf8_t *, void *);

extern void           n00b_disable_debugging(void);
extern void           n00b_enable_debugging(void);
extern void           n00b_disallow_unknown_debug_topics(void);
extern void           n00b_allow_unknown_debug_topics(void);
extern n00b_stream_t *n00b_add_debug_topic(void *);
extern void           n00b_disable_debug_autosubscribe(void);
extern void           n00b_enable_debug_autosubscribe(void);
extern void           n00b_disable_debug_server(void);

static inline n00b_utf8_t *
n00b_hex_debug_repr(void *addr)
{
    if (!n00b_in_heap(addr)) {
        return n00b_cstr_format("\n{}", n00b_hex_dump(&addr, sizeof(void *)));
    }

    n00b_alloc_hdr *h      = n00b_find_allocation_record(addr);
    n00b_utf8_t    *result = n00b_cstr_format("\n{}",
                                           n00b_hex_dump(h->data,
                                                         h->request_len));

    if (h->data != addr) {
        void    *box = n00b_box_u64((uint64_t)addr);
        uint64_t n   = (uint64_t)((char *)addr - (char *)h->data);

        n00b_utf8_t *addendum = n00b_cstr_format(
            "[em]Debug param pointed to @{:x}, {} ({:x}) bytes into the above.",
            box,
            n,
            n);
        result = n00b_str_concat(result, addendum);
    }

    return result;
}
