#pragma once
#include "n00b.h"

typedef struct n00b_observer_t   n00b_observer_t;
typedef struct n00b_observable_t n00b_observable_t;

// message, thunk
typedef void (*n00b_observer_cb)(void *, void *);
// Observable, subscription.
typedef void (*n00b_subscribe_cb)(n00b_observable_t *, n00b_observer_t *);

struct n00b_observer_t {
    n00b_observer_cb   callback;
    void              *thunk;
    n00b_string_t     *topic;
    int64_t            topic_ix;
    n00b_observable_t *target;
    bool               oneshot;
};

struct n00b_observable_t {
    n00b_list_t      *observers;
    n00b_list_t      *topics;
    n00b_subscribe_cb on_subscribe;
    n00b_subscribe_cb on_unsubscribe;
    bool              lt_sub;
    bool              lt_unsub;
    uint32_t          max_topics;
    n00b_mutex_t      lock;
#if defined(DEBUG_OBSERVERS)
    char *debug_name;
#endif
};

extern void             n00b_observable_init(n00b_observable_t *o,
                                             n00b_list_t *);
extern int              n00b_observable_post(n00b_observable_t *,
                                             void *, // Either string or int
                                             void *);
extern n00b_observer_t *n00b_observable_subscribe(n00b_observable_t *,
                                                  void *,
                                                  n00b_observer_cb,
                                                  bool,
                                                  void *);
extern void             n00b_observable_unsubscribe(n00b_observer_t *sub);
extern n00b_list_t     *n00b_observable_all_subscribers(n00b_observable_t *,
                                                        void *);

static inline void
n00b_observable_set_subscribe_callback(n00b_observable_t *o,
                                       n00b_subscribe_cb  cb,
                                       bool               level_triggered)
{
    o->on_subscribe = cb;
    o->lt_sub       = level_triggered;
}
static inline void
n00b_observable_set_unsubscribe_callback(n00b_observable_t *o,
                                         n00b_subscribe_cb  cb,
                                         bool               level_triggered)
{
    o->on_unsubscribe = cb;
    o->lt_unsub       = level_triggered;
}
