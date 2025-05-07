#pragma once
#include "n00b.h"

typedef struct n00b_topic_info_t n00b_topic_info_t;
typedef void *(*n00b_topic_cb)(n00b_topic_info_t *, void *, void *);

struct n00b_topic_info_t {
    n00b_string_t *name;
    n00b_dict_t *namespace;
    n00b_topic_cb   cb;
    void           *thunk;
    n00b_channel_t *cref;
};
