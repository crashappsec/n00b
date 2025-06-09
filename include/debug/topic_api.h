#include "n00b.h"

extern n00b_stream_t *n00b_get_debug_topic(n00b_string_t *);
extern void           _n00b_debug(n00b_string_t *, void *);

#define n00b_obj_debug(topic, msg) \
    _n00b_debug(n00b_cstring(topic), msg)
#define n00b_debugf(topic, fmt, ...)                               \
    if (n00b_type_is_string(n00b_get_my_type(topic))) {            \
        _n00b_debug(n00b_cstring(topic),                           \
                    n00b_cformat(fmt __VA_OPT__(, ) __VA_ARGS__)); \
    }                                                              \
    else {                                                         \
        _n00b_debug(n00b_cstring(topic),                           \
                    n00b_cformat(fmt __VA_OPT__(, ) __VA_ARGS__)); \
    }
#define n00b_debug(topic, contents)                     \
    if (n00b_type_is_string(n00b_get_my_type(topic))) { \
        _n00b_debug((void *)topic, contents);           \
    }                                                   \
    else {                                              \
        _n00b_debug(n00b_cstring(topic), contents);     \
    }
