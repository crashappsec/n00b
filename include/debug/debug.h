#include "n00b.h"

extern n00b_stream_t *n00b_get_chan_debug_topic(n00b_string_t *);
extern void            _n00b_debug(n00b_string_t *, void *);

#define n00b_obj_debug(topic, msg) \
    _n00b_debug(n00b_cstring(topic), msg)
#define n00b_debug(topic, fmt, ...)  \
    _n00b_debug(n00b_cstring(topic), \
                n00b_cformat(fmt __VA_OPT__(, ) __VA_ARGS__))

#ifdef N00B_DEBUG
#define n00b_cdebug(x, ...) \
    _n00b_debug(x, __VA_ARGS__ __VA_OPT__(, ) NULL)
#else
#define n00b_cdebug(x, ...) \
    _n00b_debug(x, __VA_ARGS__ __VA_OPT__(, ) NULL)
#endif
