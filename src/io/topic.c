#define N00B_USE_INTERNAL_API
#include "n00b.h"

n00b_stream_t *
n00b_get_topic(n00b_string_t *topic, n00b_string_t *namespace)
{
    n00b_push_heap(n00b_default_heap);
    if (namespace == NULL) {
        namespace = n00b_cached_empty_string();
    }

    n00b_dict_t *ns = hatrack_dict_get(n00b_system_event_base->io_topic_ns_cache,
                                       namespace,
                                       NULL);
    if (!ns) {
        ns = n00b_dict(n00b_type_string(), n00b_type_ref());
        if (!hatrack_dict_add(n00b_system_event_base->io_topic_ns_cache,
                              namespace,
                              ns)) {
            ns = hatrack_dict_get(n00b_system_event_base->io_topic_ns_cache,
                                  namespace,
                                  NULL);
        }
    }

    n00b_stream_t *res = hatrack_dict_get(ns, topic, NULL);

    if (!res) {
        n00b_topic_cookie_t *c = n00b_gc_alloc_mapped(n00b_topic_cookie_t,
                                                      N00B_GC_SCAN_ALL);

        res = n00b_alloc_party(&n00b_topic_impl,
                               c,
                               n00b_io_perm_rw,
                               n00b_io_ev_topic);

        c->name                = topic;
        // TODO: fill these in.
        c->socket_write_filter = NULL;
        c->socket_read_filter  = NULL;
        c->file_write_filter   = NULL;
        c->file_read_filter    = NULL;

        if (!hatrack_dict_add(ns, topic, res)) {
            res = hatrack_dict_get(ns, topic, NULL);
        }
    }

    n00b_pop_heap();
    return res;
}

static n00b_stream_sub_t *
add_topic_proxy(n00b_stream_sub_t *info, n00b_stream_filter_t *filter)
{
    n00b_stream_sub_t *result;

    if (!filter) {
        return NULL;
    }

    n00b_raw_unsubscribe(info);
    n00b_stream_t *proxy = n00b_new_subscription_proxy();
    result               = n00b_raw_subscribe(info->source,
                                proxy,
                                n00b_timeval_to_duration(info->timeout),
                                info->kind,
                                false);
    n00b_raw_subscribe(proxy, info->sink, NULL, n00b_io_sk_post_write, false);
    n00b_add_filter(proxy, filter, true);

    n00b_topic_cookie_t *cookie = info->cookie;

    n00b_io_set_repr(proxy,
                     n00b_cformat("«#»«#» proxy«#»",
                                  n00b_cached_lbracket(),
                                  cookie->name,
                                  n00b_cached_rbracket()));

    return result;
}

// We already created the subscription object, but we might decide to
// add in a silent proxy, in which case it's easiest to write over the
// subscription object we're returning with the 'correct' subscription
// returned by add_topic_proxy.
//
// This could use some refactoring to avoid that need.
static bool
n00b_io_internal_topic_subscribe(n00b_stream_sub_t        *info,
                                 n00b_io_subscription_kind kind)
{
    n00b_topic_cookie_t *cookie = info->source->cookie;
    n00b_stream_sub_t   *replacement;

    switch (info->sink->etype) {
    case n00b_io_ev_file:
        if (info->kind == n00b_io_sk_read) {
            replacement = add_topic_proxy(info, cookie->file_read_filter);
        }
        else {
            replacement = add_topic_proxy(info, cookie->file_write_filter);
        }
        break;
    case n00b_io_ev_socket:
        if (info->kind == n00b_io_sk_read) {
            replacement = add_topic_proxy(info, cookie->socket_read_filter);
        }
        else {
            replacement = add_topic_proxy(info, cookie->socket_write_filter);
        }
        break;
    default:
        return true;
    }
    if (!replacement) {
        return true;
    }

    memcpy(info, replacement, sizeof(n00b_stream_sub_t));
    return true;
}

static void
n00b_check_topic(n00b_stream_t *t)
{
    if (!t || t->etype != n00b_io_ev_topic) {
        N00B_CRAISE("Not a topic object.");
    }
}

static n00b_string_t *
n00b_io_topic_repr(n00b_stream_t *e)
{
    n00b_topic_cookie_t *cookie = e->cookie;

    n00b_assert(cookie->name);
    return n00b_cformat("«#»topic «em»«#»«#»}",
                        n00b_cached_lbracket(),
                        cookie->name,
                        n00b_cached_rbracket());
}

extern bool
n00b_topic_unsubscribe(n00b_stream_t *topic_obj, n00b_stream_t *subscriber)
{
    defer_on();

    bool found = false;

    n00b_check_topic(topic_obj);

    n00b_list_t *l = n00b_dict_keys(topic_obj->write_subs);
    int          n = n00b_list_len(l);

    for (int i = 0; i < n; i++) {
        n00b_stream_sub_t *sub = n00b_list_get(l, i, NULL);
        if (!sub) {
            break;
        }

        n00b_acquire_party(sub->source);
        n00b_acquire_party(sub->sink);
        if (sub->source == topic_obj && sub->sink == subscriber) {
            n00b_raw_unsubscribe(sub);
            found = true;
        }
        n00b_release_party(sub->sink);
        n00b_release_party(sub->source);
    }

    Return found;

    defer_func_end();
}

void
n00b_topic_post(void *topic, void *msg)
{
    n00b_type_t *t = n00b_get_my_type(topic);

    if (n00b_type_is_string(t)) {
        topic = n00b_get_topic(topic, NULL);
    }
    else {
        n00b_assert(n00b_type_is_channel(t));
    }

    n00b_list_t *filtered = n00b_handle_read_operation(topic, msg);

    if (!filtered) {
        return;
    }

    int n = n00b_list_len(filtered);

    for (int i = 0; i < n; i++) {
        msg = n00b_list_get(filtered, i, NULL);
        n00b_write(topic, msg);
    }
}

void
n00b_message_init(n00b_message_t *obj, va_list args)
{
    n00b_stream_t *topic = va_arg(args, n00b_stream_t *);
    void          *msg   = va_arg(args, void *);

    n00b_check_topic(topic);

    n00b_topic_cookie_t *cookie = topic->cookie;
    obj->topic                  = cookie->name;
    obj->payload                = msg;
}

n00b_string_t *
n00b_message_repr(n00b_message_t *obj)
{
    return n00b_cformat("@«#»: «#»", obj->topic, obj->payload);
}

static void *
n00b_io_topic_write(n00b_stream_t *party, void *msg)
{
    return n00b_new(n00b_type_message(), party, msg);
}

static bool
n00b_io_topic_read(n00b_stream_t *party, uint64_t ignore_always_1)
{
    // Dummy; this already is going to wait until a message is posted
    // to the topic.
    return true;
}

n00b_io_impl_info_t n00b_topic_impl = {
    .open_impl      = (void *)n00b_get_topic,
    .subscribe_impl = n00b_io_internal_topic_subscribe,
    .read_impl      = n00b_io_topic_read,
    .write_impl     = n00b_io_topic_write,
    .repr_impl      = n00b_io_topic_repr,
};

const n00b_vtable_t n00b_message_vtable = {
    .methods = {
        [N00B_BI_CONSTRUCTOR] = (n00b_vtable_entry)n00b_message_init,
        [N00B_BI_TO_STRING]   = (n00b_vtable_entry)n00b_message_repr,
        [N00B_BI_GC_MAP]      = (n00b_vtable_entry)N00B_GC_SCAN_ALL,
    },
};
