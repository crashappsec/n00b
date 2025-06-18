#define N00B_USE_INTERNAL_API
#include "n00b.h"

static once n00b_dict_t *
get_default_namespace(void)
{
    return n00b_dict(n00b_type_string(), n00b_type_ref());
}

static int
topic_stream_init(n00b_stream_t *stream, n00b_list_t *args)
{
    n00b_topic_info_t *topic = n00b_get_stream_cookie(stream);

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
    n00b_topic_info_t *topic = n00b_get_stream_cookie(stream);
    void              *val   = msg;
    bool               err;

    if (topic->cb) {
        val = (*topic->cb)(topic, msg, topic->param);
    }

    if (!val) {
        return;
    }
    // Generally, this is how the pub/sub bus gets messages to publish
    // to readers.  Without this, we will hang in certain situations where
    // we've subscribed an fd to read a callback.
    n00b_cache_read(stream, val);

    // Don't care about the result of err, just need to pass it.
    n00b_io_dispatcher_process_read_queue(stream, &err);
}

static n00b_stream_impl topic_impl = {
    .cookie_size = sizeof(n00b_topic_info_t),
    .init_impl   = (void *)topic_stream_init,
    .read_impl   = (void *)topic_read,
    .write_impl  = (void *)topic_write,
};

n00b_stream_t *
_n00b_create_topic_stream(n00b_string_t *name,
                          n00b_dict_t   *ns,
                          n00b_topic_cb  cb,
                          void          *param,
                          ...)
{
    n00b_list_t   *args = n00b_list(n00b_type_ref());
    n00b_list_t   *filters;
    n00b_stream_t *result;

    n00b_build_filter_list(filters, param);

    if (!ns) {
        ns = get_default_namespace();
    }

    n00b_list_append(args, name);
    n00b_list_append(args, ns);
    n00b_list_append(args, cb);
    n00b_list_append(args, param);

    result = n00b_new(n00b_type_stream(), &topic_impl, args, filters);

    result->name = name;

    if (hatrack_dict_add(ns, name, result)) {
        return result;
    }

    return hatrack_dict_get(ns, name, NULL);
}

n00b_stream_t *
_n00b_get_topic_stream(n00b_string_t *name, ...)
{
    va_list args;
    va_start(args, name);
    n00b_dict_t *ns = va_arg(args, n00b_dict_t *);

    if (!ns) {
        ns = get_default_namespace();
    }

    n00b_stream_t *res = hatrack_dict_get(ns, name, NULL);

    if (!res) {
        return _n00b_create_topic_stream(name, ns, NULL, NULL, 0ULL);
    }

    return res;
}
