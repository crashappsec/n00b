#define N00B_USE_INTERNAL_API
#include "n00b.h"

static n00b_dict_t   *n00b_default_namespace = NULL;
static pthread_once_t topic_init             = PTHREAD_ONCE_INIT;

static void
setup_default_namespace(void)
{
    n00b_gc_register_root(&n00b_default_namespace, 1);
    n00b_default_namespace = n00b_dict(n00b_type_string(), n00b_type_ref());
}

static inline void
topics_init(void)
{
    pthread_once(&topic_init, setup_default_namespace);
}

static int
topic_chan_init(n00b_topic_info_t *topic, n00b_list_t *args)
{
    topic->thunk           = n00b_list_pop(args);
    topic->cb              = n00b_list_pop(args);
    topic->namespace       = n00b_list_pop(args);
    topic->name            = n00b_list_pop(args);
    topic->blocked_readers = n00b_list(n00b_type_ref());

    return O_RDWR;
}

static void *
topic_read(n00b_topic_info_t *topic, bool block, bool *success, int ms_timeout)
{
    if (!block) {
        *success = false;
        return NULL;
    }

    n00b_condition_t *cond = n00b_new(n00b_type_condition());
    n00b_list_append(topic->blocked_readers, cond);

    if (!ms_timeout) {
        n00b_condition_lock_acquire(cond);
        n00b_condition_wait(cond);
        *success = true;
        return cond->aux;
    }

    struct timespec ts = {
        .tv_sec  = ms_timeout / 1000,
        .tv_nsec = (ms_timeout % 1000) * 1000000,
    };

    if (n00b_condition_timed_wait(cond, &ts)) {
        *success = true;
        return cond->aux;
    }
    *success = false;
    return NULL;
}

static void
topic_write(n00b_topic_info_t *topic, n00b_cmsg_t *msg, bool block)
{
    if (topic->cb) {
        (*topic->cb)(topic, msg->payload, topic->thunk);
    }

    n00b_lock_list(topic->blocked_readers);
    n00b_condition_t *cond = n00b_private_list_pop(topic->blocked_readers);

    while (cond) {
        n00b_condition_lock_acquire(cond);
        n00b_condition_notify_aux(cond, msg->payload);
        n00b_condition_lock_release(cond);
        cond = n00b_private_list_pop(topic->blocked_readers);
    }

    n00b_unlock_list(topic->blocked_readers);
}

static n00b_chan_impl topic_impl = {
    .cookie_size = sizeof(n00b_topic_info_t),
    .init_impl   = (void *)topic_chan_init,
    .read_impl   = (void *)topic_read,
    .write_impl  = (void *)topic_write,
};

n00b_channel_t *
_n00b_create_topic_channel(n00b_string_t *name,
                           n00b_dict_t   *ns,
                           n00b_topic_cb  cb,
                           void          *thunk,
                           ...)
{
    topics_init();

    va_list alist;
    va_start(alist, cb);

    n00b_list_t *args    = n00b_list(n00b_type_ref());
    n00b_list_t *filters = n00b_list(n00b_type_ref());
    int          nargs   = va_arg(alist, int);

    if (!ns) {
        ns = n00b_default_namespace;
    }

    n00b_list_append(args, name);
    n00b_list_append(args, ns);
    n00b_list_append(args, cb);
    n00b_list_append(args, thunk);

    while (nargs--) {
        n00b_list_append(filters, va_arg(alist, void *));
    }

    va_end(alist);

    n00b_channel_t    *result = n00b_channel_create(&topic_impl, args, filters);
    n00b_topic_info_t *ti     = n00b_get_channel_cookie(n00b_channel_core(result));

    ti->cref = result;

    if (hatrack_dict_add(ns, name, result)) {
        return result;
    }

    return hatrack_dict_get(ns, name, NULL);
}

n00b_channel_t *
_n00b_get_topic_channel(n00b_string_t *name, ...)
{
    topics_init();

    va_list args;
    va_start(args, name);
    n00b_dict_t *ns = va_arg(args, n00b_dict_t *);

    if (!ns) {
        ns = n00b_default_namespace;
    }

    n00b_channel_t *res = hatrack_dict_get(ns, name, NULL);

    if (!res) {
        return _n00b_create_topic_channel(name, ns, NULL, NULL, 0);
    }

    return res;
}
