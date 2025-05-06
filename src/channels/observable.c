#include "n00b.h"

void
n00b_observable_init(n00b_observable_t *o, n00b_list_t *topics)
{
    if (topics) {
        o->max_topics = n00b_list_len(topics);
        o->topics     = topics;
    }
    else {
        o->topics = n00b_list(n00b_type_string());
    }

    o->observers = n00b_list(n00b_type_ref());
    n00b_named_lock_init(&o->lock, "observable");
}

static inline int64_t
topic_id_by_name(n00b_observable_t *o, n00b_string_t *t)
{
    // Topics are meant to be static.
    int64_t result = n00b_private_list_find(o->topics, t);
    if (o->max_topics && result >= o->max_topics) {
        return -1;
    }

    return result;
}

static int64_t
get_topic_ix(n00b_observable_t *o, void *topic_info)
{
    if (n00b_type_is_string(n00b_get_my_type(topic_info))) {
        return topic_id_by_name(o, topic_info);
    }
    else {
        return (int64_t)topic_info;
    }
}

int
n00b_observable_add_topic(n00b_observable_t *o, n00b_string_t *t)
{
    n00b_list_append(o->topics, t);
    int result = topic_id_by_name(o, t);

    if (o->max_topics && result >= (int)o->max_topics) {
        return -1;
    }

    return result;
}

void
n00b_observable_set_num_topics(n00b_observable_t *o, uint32_t n)
{
    o->max_topics = n;
}

static inline n00b_list_t *
get_topic_subs(n00b_observable_t *o, int64_t ix)
{
    return n00b_list_get(o->observers, ix, NULL);
}

n00b_list_t *
n00b_observable_all_subscribers(n00b_observable_t *o,
                                void              *topic_info)
{
    return get_topic_subs(o, get_topic_ix(o, topic_info));
}

// Topic is either a string or an int.
int
n00b_observable_post(n00b_observable_t *o, void *topic_info, void *msg)
{
    int64_t topic_ix = get_topic_ix(o, topic_info);

    n00b_lock_acquire(&o->lock);

    int          r = 0;
    n00b_list_t *l = n00b_list_get(o->observers, topic_ix, NULL);

    if (!l) {
        n00b_lock_release(&o->lock);
        return r;
    }

    l = n00b_list_shallow_copy(l);
    n00b_lock_release(&o->lock);

    int n = n00b_list_len(l);

    while (n--) {
        n00b_observer_t *target = n00b_private_list_get(l, n, NULL);

        if (!target) {
            n00b_private_list_remove(l, n);
            continue;
        }

        (*target->callback)(msg, target->thunk);
        r++;

        if (target->oneshot) {
            n00b_private_list_remove_item(l, target);
        }
    }

    return r;
}

n00b_observer_t *
n00b_observable_subscribe(n00b_observable_t *target,
                          void              *topic_info,
                          n00b_observer_cb   cb,
                          bool               oneshot,
                          void              *thunk)
{
    int64_t        topic_ix   = get_topic_ix(target, topic_info);
    n00b_list_t   *topic_subs = get_topic_subs(target, topic_ix);
    n00b_string_t *topic      = n00b_list_get(target->topics, topic_ix, NULL);

    if (!topic_subs) {
        topic_subs = n00b_list(n00b_type_ref());
        n00b_list_set(target->observers, topic_ix, topic_subs);
    }

    n00b_observer_t *result = n00b_gc_alloc_mapped(n00b_observer_t *,
                                                   N00B_GC_SCAN_ALL);
    result->callback        = cb;
    result->oneshot         = oneshot;
    result->thunk           = thunk;
    result->topic           = topic;
    result->topic_ix        = topic_ix;
    result->target          = target;

    n00b_list_append(topic_subs, result);

    if (target->on_subscribe) {
        if (n00b_list_len(topic_subs) == 1 || target->lt_sub) {
            (*target->on_subscribe)(target, result);
        }
    }
    return result;
}

void
n00b_observable_unsubscribe(n00b_observer_t *sub)
{
    n00b_observable_t *target     = sub->target;
    n00b_list_t       *topic_subs = get_topic_subs(target, sub->topic_ix);

    if (!topic_subs) {
        return;
    }

    n00b_lock_acquire(&target->lock);

    n00b_list_remove_item(topic_subs, sub);
    if (target->on_unsubscribe) {
        if (target->lt_unsub || !n00b_list_len(topic_subs)) {
            (*target->on_unsubscribe)(target, sub);
        }
    }

    n00b_lock_release(&target->lock);
}
