#pragma once
#include "n00b.h"

// message, param, which should generally be the subscriber..
extern n00b_list_t *n00b_observable_add_subscribers(n00b_observable_t *,
                                                    void *,
                                                    n00b_observer_cb,
                                                    void *,
                                                    bool);

static inline n00b_observer_t *
n00b_observable_subscribe(n00b_observable_t *target,
                          void              *topic,
                          n00b_observer_cb   cb,
                          void              *sub,
                          bool               oneshot)
{
    n00b_list_t *subs = n00b_observable_add_subscribers(target,
                                                        topic,
                                                        cb,
                                                        sub,
                                                        oneshot);
    return n00b_list_get(subs, 0, NULL);
}

extern void n00b_observable_init(n00b_observable_t *o,
                                 n00b_list_t *);
extern int  n00b_observable_post(n00b_observable_t *,
                                 void *, // Either string or int
                                 void *);

extern n00b_list_t *n00b_observable_all_subscribers(n00b_observable_t *,
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

static inline void
n00b_observable_remove_all_subscriptions(n00b_observable_t *o)
{
    o->observers = n00b_list(n00b_type_ref());
}
