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
topic_chan_init(n00b_stream_t *stream, n00b_list_t *args)
{
    n00b_topic_info_t *topic = n00b_get_channel_cookie(stream);

    topic->param     = n00b_list_pop(args);
    topic->cb        = n00b_list_pop(args);
    topic->namespace = n00b_list_pop(args);
    topic->name      = n00b_list_pop(args);

    return O_RDWR;
}

static void *
topic_read(n00b_stream_t *stream, bool *err)
{
    // The only way to generate a read from a topic is to write
    // something to it.
    *err = true;
    return NULL;
}

static void
topic_write(n00b_stream_t *stream, void *msg, bool block)
{
    n00b_topic_info_t *topic = n00b_get_channel_cookie(stream);
    void              *val   = msg;
    bool               err;

    if (topic->cb) {
        val = (*topic->cb)(topic, msg, topic->param);
    }

    // Generally, this is how the pub/sub bus gets messages to publish
    // to readers.  Without this, we will hang in certain situations where
    // we've subscribed an fd to read a callback.
    n00b_list_append(stream->read_cache, val);

    // Don't care about the result of err, just need to pass it.
    n00b_io_dispatcher_process_read_queue(stream, &err);
}

static n00b_chan_impl topic_impl = {
    .cookie_size = sizeof(n00b_topic_info_t),
    .init_impl   = (void *)topic_chan_init,
    .read_impl   = (void *)topic_read,
    .write_impl  = (void *)topic_write,
};

n00b_stream_t *
_n00b_create_topic_channel(n00b_string_t *name,
                           n00b_dict_t   *ns,
                           n00b_topic_cb  cb,
                           void          *param,
                           ...)
{
    n00b_list_t    *args = n00b_list(n00b_type_ref());
    n00b_list_t    *filters;
    n00b_stream_t *result;

    topics_init();

    n00b_build_filter_list(filters, param);

    if (!ns) {
        ns = n00b_default_namespace;
    }

    n00b_list_append(args, name);
    n00b_list_append(args, ns);
    n00b_list_append(args, cb);
    n00b_list_append(args, param);

    result = n00b_new(n00b_type_channel(), &topic_impl, args, filters);

    result->name = name;

    if (hatrack_dict_add(ns, name, result)) {
        return result;
    }

    return hatrack_dict_get(ns, name, NULL);
}

n00b_stream_t *
_n00b_get_topic_channel(n00b_string_t *name, ...)
{
    topics_init();

    va_list args;
    va_start(args, name);
    n00b_dict_t *ns = va_arg(args, n00b_dict_t *);

    if (!ns) {
        ns = n00b_default_namespace;
    }

    n00b_stream_t *res = hatrack_dict_get(ns, name, NULL);

    if (!res) {
        return _n00b_create_topic_channel(name, ns, NULL, NULL, 0ULL);
    }

    return res;
}
