#define N00B_USE_INTERNAL_API
#include "n00b.h"

static pthread_once_t debug_namespace_init = PTHREAD_ONCE_INIT;
static n00b_dict_t   *debug_namespace;

static void
setup_namespace(void)
{
    n00b_gc_register_root(&debug_namespace, 1);
    debug_namespace = n00b_dict(n00b_type_string(), n00b_type_ref());
}

static void *
start_debug_workflow(n00b_topic_info_t *tinfo, void *payload, void *unused)
{
    n00b_debug_msg_t *msg = n00b_gc_alloc_mapped(n00b_debug_msg_t,
                                                 N00B_GC_SCAN_ALL);
    msg->topic            = tinfo->name;
    msg->timestamp        = n00b_current_date_time();
    msg->workflow         = n00b_get_topic_workflow(msg->topic);
    msg->payload          = payload;

    n00b_apply_debug_workflow(msg);

    return NULL;
}

n00b_channel_t *
n00b_get_chan_debug_topic(n00b_string_t *name)
{
    pthread_once(&debug_namespace_init, setup_namespace);

    n00b_channel_t *result = hatrack_dict_get(debug_namespace, name, NULL);
    if (result) {
        return result;
    }

    return n00b_create_topic_channel(name,
                                     debug_namespace,
                                     start_debug_workflow,
                                     NULL);
}

void
_n00b_debug(n00b_string_t *topic, void *msg)
{
    n00b_channel_t *channel = n00b_get_chan_debug_topic(topic);
    n00b_channel_write(channel, msg);
}
