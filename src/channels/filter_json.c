#include "n00b.h"
#include "channels/channel.h"
#include "io/marshal.h"
#include "channels/filter.h"

n00b_list_t *
n00b_chan_filter_to_json(void *cookie, void *msg)
{
    n00b_string_t *s = n00b_to_json(msg);
    if (!s) {
        return NULL;
    }
    n00b_list_t *result = n00b_list(n00b_type_string());
    n00b_list_append(result, s);
    return result;
}

n00b_filter_impl *
n00b_new_chan_filter_to_json_impl()
{
    n00b_filter_impl *impl = n00b_gc_alloc(sizeof(n00b_filter_impl));
    impl->cookie_size = 0;
    // impl->name = n00b_cstring("filter_to_json");
    impl->name = n00b_string_empty();
    impl->output_type = n00b_type_list(n00b_type_string());
    impl->read_fn = n00b_chan_filter_to_json;
    impl->write_fn = n00b_chan_filter_to_json;
    return impl;
}

void
n00b_chan_filter_add_to_json_on_read(n00b_channel_t *c)
{
    n00b_filter_impl *impl = n00b_new_chan_filter_to_json_impl();
    n00b_filter_add(c, impl, N00B_FILTER_READ);
}

void
n00b_chan_filter_add_to_json_on_write(n00b_channel_t *c)
{
    n00b_filter_impl *impl = n00b_new_chan_filter_to_json_impl();
    n00b_filter_add(c, impl, N00B_FILTER_WRITE);
}

n00b_list_t *
n00b_chan_filter_from_json(void *cookie, void *msg)
{
    n00b_list_t *errors = NULL;
    void        *parsed = n00b_json_parse(msg, &errors);
    n00b_list_t *result;

    if (!errors || !n00b_list_len(errors)) {
        result = n00b_list(n00b_get_my_type(parsed));
        n00b_list_append(result, parsed);
        return result;
    }

    return NULL;
}

n00b_filter_impl *
n00b_new_chan_filter_from_json_impl()
{
    n00b_filter_impl *impl = n00b_gc_alloc(sizeof(n00b_filter_impl));
    impl->cookie_size = 0;
    // impl->name = n00b_cstring("filter_from_json");
    impl->name = n00b_string_empty();
    impl->output_type = n00b_type_list(n00b_type_ref());
    impl->read_fn = n00b_chan_filter_from_json;
    impl->write_fn = n00b_chan_filter_from_json;
    return impl;
}

void
n00b_chan_filter_add_from_json_on_read(n00b_channel_t *c)
{
    n00b_filter_impl *impl = n00b_new_chan_filter_from_json_impl();
    n00b_filter_add(c, impl, N00B_FILTER_READ);
}

void
n00b_chan_filter_add_from_json_on_write(n00b_channel_t *c)
{
    n00b_filter_impl *impl = n00b_new_chan_filter_from_json_impl();
    n00b_filter_add(c, impl, N00B_FILTER_WRITE);
}