#pragma once
#include "n00b.h"

typedef struct n00b_topic_info_t n00b_topic_info_t;
typedef void *(*n00b_topic_cb)(n00b_topic_info_t *, void *, void *);

struct n00b_topic_info_t {
    n00b_string_t *name;
    n00b_dict_t *namespace;
    n00b_topic_cb cb;
    void         *param;
};

extern n00b_stream_t *
_n00b_create_topic_stream(n00b_string_t *,
                           n00b_dict_t *,
                           n00b_topic_cb,
                           void *,
                           ...);

extern n00b_stream_t *_n00b_get_topic_stream(n00b_string_t *, ...);

#define n00b_create_topic_stream(name, ns, cb, etc, ...) \
    _n00b_create_topic_stream(                           \
        name,                                             \
        ns,                                               \
        cb,                                               \
        etc,                                              \
        __VA_ARGS__ __VA_OPT__(, ) 0ULL)

#define n00b_get_topic_stream(x, ...) \
    _n00b_get_topic_stream(x, __VA_ARGS__ __VA_OPT__(, ) 0ULL)
