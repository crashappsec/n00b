#pragma once
#include "n00b.h"

typedef struct n00b_observer_t   n00b_observer_t;
typedef struct n00b_observable_t n00b_observable_t;

// Observable, subscription.
typedef void (*n00b_subscribe_cb)(n00b_observable_t *, n00b_observer_t *);
typedef void (*n00b_observer_cb)(void *, void *);

struct n00b_observable_t {
    n00b_list_t      *observers;
    n00b_list_t      *topics;
    n00b_subscribe_cb on_subscribe;
    n00b_subscribe_cb on_unsubscribe;
    bool              lt_sub;
    bool              lt_unsub;
    uint32_t          max_topics;
    n00b_mutex_t      lock;
};

struct n00b_observer_t {
    n00b_observable_t *target;
    void              *subscriber;
    n00b_string_t     *topic;
    n00b_observer_cb   callback;
    int64_t            topic_ix;
    bool               oneshot;
};

extern void n00b_observable_unsubscribe(n00b_observer_t *sub);
