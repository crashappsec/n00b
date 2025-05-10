#include "n00b.h"

extern n00b_channel_t *n00b_get_chan_debug_topic(n00b_string_t *);
extern void            n00b_chan_debug(n00b_string_t *, void *);

#define n00b_obj_debug(topic, msg) \
    n00b_chan_debug(n00b_cstring(topic), msg)
#define n00b_debugf(topic, fmt, ...)     \
    n00b_chan_debug(n00b_cstring(topic), \
                    n00b_cformat(fmt __VA_OPT__(, ) __VA_ARGS__))
