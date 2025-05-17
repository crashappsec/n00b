#include "n00b.h"
#include "channels/channel.h"
#include "channels/filter.h"

static n00b_list_t *
n00b_chan_filter_to_json(void *cookie, void *msg)
{
    n00b_type_t *msg_type = n00b_get_my_type(msg);
    n00b_list_t *result   = n00b_list(n00b_type_string());

    if (n00b_type_is_buffer(msg_type)) {
        n00b_list_append(result, msg);
        return result;
    }

    if (!n00b_type_is_string(msg_type)) {
        msg = n00b_to_string(msg);
    }

    n00b_string_t *s = n00b_to_json(msg);
    if (!s) {
        // TODO: there was a problem converting to json
        return msg;
    }

    n00b_list_append(result, s);
    return result;
}

static n00b_filter_impl to_json_filter = {
    .cookie_size = 0,
    .read_fn     = (void *)n00b_chan_filter_to_json,
    .write_fn    = (void *)n00b_chan_filter_to_json,
    .name        = NULL,
};

n00b_filter_spec_t *
n00b_filter_to_json(int filter_policy)
{
    if (!to_json_filter.name) {
        to_json_filter.name = n00b_cstring("to_json");
    }

    n00b_filter_spec_t *result = n00b_gc_alloc(n00b_filter_spec_t);
    result->impl               = &to_json_filter;
    result->policy             = filter_policy;

    return result;
}

void
n00b_chan_filter_add_to_json_on_read(n00b_channel_t *c)
{
    n00b_filter_spec_t *spec = n00b_filter_to_json(N00B_FILTER_READ);
    n00b_filter_add(c, spec);
}

void
n00b_chan_filter_add_to_json_on_write(n00b_channel_t *c)
{
    n00b_filter_spec_t *spec = n00b_filter_to_json(N00B_FILTER_WRITE);
    ;
    n00b_filter_add(c, spec);
}

static n00b_list_t *
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

static n00b_filter_impl from_json_filter = {
    .cookie_size = 0,
    .read_fn     = (void *)n00b_chan_filter_from_json,
    .write_fn    = (void *)n00b_chan_filter_from_json,
    .name        = NULL,
};

n00b_filter_spec_t *
n00b_filter_from_json(int filter_policy)
{
    if (!to_json_filter.name) {
        to_json_filter.name = n00b_cstring("from_json");
    }

    n00b_filter_spec_t *result = n00b_gc_alloc(n00b_filter_spec_t);
    result->impl               = &from_json_filter;
    result->policy             = filter_policy;

    return result;
}

void
n00b_chan_filter_add_from_json_on_read(n00b_channel_t *c)
{
    n00b_filter_spec_t *spec = n00b_filter_from_json(N00B_FILTER_READ);
    n00b_filter_add(c, spec);
}

void
n00b_chan_filter_add_from_json_on_write(n00b_channel_t *c)
{
    n00b_filter_spec_t *spec = n00b_filter_from_json(N00B_FILTER_WRITE);
    n00b_filter_add(c, spec);
}
