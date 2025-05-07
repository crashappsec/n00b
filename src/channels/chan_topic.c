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
    topic->thunk     = n00b_list_pop(args);
    topic->cb        = n00b_list_pop(args);
    topic->namespace = n00b_list_pop(args);
    topic->name      = n00b_list_pop(args);

    return O_RDWR;
}

static void *
topic_read(n00b_topic_info_t *topic, bool *success)
{
    // The only way to generate a read from a topic is to write
    // something to it.
    *success = false;
    return NULL;
}

static void
topic_write(n00b_topic_info_t *topic, void *msg, bool block)
{
    void *val = msg;

    if (!topic->cb) {
        val = (*topic->cb)(topic, msg, topic->thunk);
    }

    if (!topic->cref->read_cache) {
        topic->cref->read_cache = n00b_list(n00b_type_ref());
    }

    // Generally, this is how the pub/sub bus gets messages to publish
    // to readers.  Without this, we will hang in certain situations where
    // we've subscribed an fd to read a callback.
    n00b_list_append(topic->cref->read_cache, val);
    n00b_io_dispatcher_process_read_queue(topic->cref);
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
    n00b_topic_info_t *ti     = n00b_get_channel_cookie(result);

    ti->cref     = result;
    result->name = name;

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
