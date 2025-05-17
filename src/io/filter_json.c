#include "n00b.h"

static n00b_list_t *
n00b_filter_to_json(void *ignore, void *msg)
{
    n00b_list_t *result = n00b_list(n00b_type_string());

    n00b_string_t *s = n00b_to_json(msg);
    if (!s) {
        N00B_CRAISE("Add error handling to filters.");
    }

    n00b_list_append(result, s);

    return result;
}

static n00b_list_t *
n00b_filter_from_json(void *ignore, void *msg)
{
    n00b_list_t *errors = NULL;
    void        *parsed = n00b_json_parse(msg, &errors);
    n00b_list_t *result;

    if (!errors || !n00b_list_len(errors)) {
        result = n00b_list(n00b_get_my_type(parsed));
        n00b_list_append(result, parsed);

        return result;
    }

    N00B_CRAISE("Add error handling to filters.");
    /*
    int n = n00b_list_len(errors);
    for (int i = 0; i < n; i++) {
        n00b_post_error(e, n00b_list_get(errors, i, NULL));
    }

    return NULL;
    */
}

static n00b_filter_impl json_filter_write_encodes = {
    .cookie_size = 0,
    .read_fn     = (void *)n00b_filter_from_json,
    .write_fn    = (void *)n00b_filter_to_json,
    .name        = NULL,
};

static n00b_filter_impl json_filter_write_decodes = {
    .cookie_size = 0,
    .read_fn     = (void *)n00b_filter_to_json,
    .write_fn    = (void *)n00b_filter_from_json,
    .name        = NULL,
};

n00b_filter_spec_t *
n00b_filter_json(bool encode_writes, bool filter_w, bool filter_r)
{
    if (!json_filter_write_encodes.name) {
        json_filter_write_encodes.name = n00b_cstring("json (w encode)");
        json_filter_write_decodes.name = n00b_cstring("json (w decode)");
    }

    if (!(filter_w || filter_r)) {
        N00B_CRAISE("Must choose to apply filter either writes or reads.");
    }

    n00b_filter_spec_t *result = n00b_gc_alloc(n00b_filter_spec_t);

    if (encode_writes) {
        result->impl = &json_filter_write_encodes;
    }
    else {
        result->impl = &json_filter_write_decodes;
    }

    if (filter_w) {
        result->policy = N00B_FILTER_WRITE;
    }
    if (filter_r) {
        result->policy |= N00B_FILTER_READ;
    }

    return result;
}
