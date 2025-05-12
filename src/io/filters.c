#define N00B_WARN_TMP
#define N00B_USE_INTERNAL_API
#include "n00b.h"

n00b_stream_filter_t *
n00b_new_filter(n00b_filter_fn xform_fn,
                n00b_filter_fn flush_fn,
                n00b_string_t *name,
                size_t         cookie_sz)
{
    n00b_stream_filter_t *filter = n00b_gc_alloc_mapped(n00b_stream_filter_t,
                                                        N00B_GC_SCAN_ALL);
    filter->xform_fn             = xform_fn;
    filter->flush_fn             = flush_fn;

    if (!xform_fn) {
        N00B_CRAISE("No transform supplied for filter.");
    }

    if (cookie_sz) {
        if (cookie_sz) {
            filter->state = n00b_gc_raw_alloc(cookie_sz, N00B_GC_SCAN_ALL);
        }
    }

    filter->name = name;

    return filter;
}

void
n00b_remove_filter(n00b_stream_t *party, n00b_stream_filter_t *filter)
{
    n00b_list_remove_item(party->read_filters, filter);
    n00b_list_remove_item(party->write_filters, filter);
}

// TODO: replace the u8 filter, which takes a buffer or string string
// or message, and converts them to a canonical string representation
// if possible. Whenever there are errors, an error posts, with the
// contents stored in the 'context' field.

typedef struct lb_state_t {
    n00b_buf_t *b;
} lb_state_t;

static n00b_list_t *
n00b_line_buffer_flush(n00b_stream_t *e, void *ctx, void *msg)
{
    lb_state_t *state = ctx;
    n00b_buf_t *b     = state->b;

    if (!b || !n00b_buffer_len(b)) {
        return NULL;
    }

    n00b_list_t *result = n00b_list(n00b_type_ref());
    n00b_list_append(result, b);
    state->b = NULL;

    return result;
}

static n00b_list_t *
n00b_line_buffer_xform(n00b_stream_t *e, void *ctx, void *msg)
{
    n00b_type_t *t      = n00b_get_my_type(msg);
    n00b_list_t *result = n00b_list(n00b_type_ref());
    lb_state_t  *saved  = ctx;
    n00b_buf_t  *input;
    n00b_buf_t  *tmp;

    if (n00b_type_is_condition(t)) {
        n00b_list_append(result, msg);
        return result;
    }

    if (!n00b_can_coerce(t, n00b_type_buffer())) {
        n00b_string_t *s = n00b_to_string(msg);

        if (!s) {
            n00b_post_cerror_ctx(e,
                                 "Cannot line buffer object that cannot "
                                 "be converted to a buffer or string.",
                                 msg);
            return result;
        }

        t   = n00b_type_string();
        msg = s;
    }

    input = n00b_coerce(msg, t, n00b_type_buffer());

    if (input->byte_len == 0) {
        return result;
    }

    char *p     = input->data;
    char *start = p;
    char *end   = p + n00b_buffer_len(input);

    while (p < end) {
        if (*p++ == '\n') {
            if (start == input->data && p == end) {
                tmp = input;
            }
            else {
                int n = p - start;

                tmp = n00b_new(n00b_type_buffer(),
                               n00b_kw("length",
                                       n00b_ka(n),
                                       "raw",
                                       start));
            }
            if (saved->b) {
                tmp      = n00b_buffer_add(saved->b, tmp);
                saved->b = NULL;
            }
            n00b_list_append(result, tmp);
            start = p;
        }
    }

    if (start != end) {
        if (start == input->data) {
            saved->b = input;
        }
        else {
            int n    = end - start;
            saved->b = n00b_new(n00b_type_buffer(),
                                n00b_kw("length",
                                        n00b_ka(n),
                                        "raw",
                                        start));
        }
    }

    return result;
}

n00b_stream_filter_t *
n00b_new_line_buffering_xform(void)
{
    return n00b_new_filter(n00b_line_buffer_xform,
                           n00b_line_buffer_flush,
                           n00b_cstring("line buffer"),
                           sizeof(lb_state_t));
}

void
n00b_line_buffer_reads(n00b_channel_t *party)
{
}

void
n00b_line_buffer_writes(n00b_channel_t *party)
{
}

n00b_list_t *
n00b_hex_dump_xform(n00b_stream_t *stream, void *ctx, void *msg)
{
    n00b_string_t *repr;
    n00b_type_t   *t = n00b_get_my_type(msg);

    if (n00b_type_is_buffer(t)) {
        n00b_buf_t *b = msg;
        repr          = n00b_hex_dump(b->data, n00b_len(b));
    }
    else {
        if (n00b_type_is_string(t)) {
            n00b_string_t *str = msg;
            repr               = n00b_hex_dump(str->data, str->u8_bytes);
        }
        else {
            repr = n00b_hex_debug_repr(msg);
        }
    }
    int64_t *count_ptr = ctx;
    int64_t  count     = 1 + *count_ptr;
    *count_ptr         = count;
    uint64_t *ptr      = n00b_box_u64((uint64_t)stream);

    n00b_string_t *s = n00b_cformat("«#» @«#:p» (msg: «#», ty: «#»):\n«#»",
                                    stream,
                                    ptr,
                                    count,
                                    t,
                                    repr);

    fprintf(stderr, "%s\n", s->data);

    n00b_list_t *result = n00b_list(n00b_type_ref());
    n00b_list_append(result, msg);

    return result;
}

typedef struct {
    n00b_callback_filter fn;
} cb_state_t;

static n00b_list_t *
n00b_custom_cb_xform(n00b_stream_t *e, void *ctx, void *msg)
{
    cb_state_t *state = ctx;

    if ((*state->fn)(msg, e)) {
        n00b_list_t *result = n00b_list(n00b_type_ref());
        n00b_list_append(result, msg);
        return result;
    }

    return NULL;
}

n00b_stream_filter_t *
n00b_new_custom_callback_filter(n00b_callback_filter fn)
{
    n00b_stream_filter_t *result = n00b_new_filter(
        n00b_custom_cb_xform,
        NULL,
        n00b_cstring("custom callback"),
        sizeof(cb_state_t));

    cb_state_t *state = result->state;
    state->fn         = fn;

    return result;
}

void
n00b_add_custom_read_filter(n00b_stream_t *party, n00b_callback_filter fn)
{
    n00b_add_filter(party, n00b_new_custom_callback_filter(fn), true);
}

void
n00b_add_custom_write_filter(n00b_stream_t *party, n00b_callback_filter fn)
{
    n00b_add_filter(party, n00b_new_custom_callback_filter(fn), false);
}

n00b_stream_filter_t *
n00b_ansi_ctrl_parse_on_write(n00b_channel_t *party)
{
    return NULL;
}

n00b_stream_filter_t *
n00b_ansi_ctrl_parse_on_read(n00b_channel_t *party)
{
    return NULL;
}

static n00b_list_t *
n00b_to_json_xform(n00b_stream_t *e, void *ignore, void *msg)
{
    n00b_string_t *s = n00b_to_json(msg);

    if (!s) {
        n00b_post_cerror_ctx(e, "Value can't be converted to JSon.", msg);
        return NULL;
    }

    n00b_list_t *result = n00b_list(n00b_type_string());
    n00b_list_append(result, s);

    return result;
}

static n00b_list_t *
n00b_from_json_xform(n00b_stream_t *e, void *ignore, void *msg)
{
    n00b_list_t *errors = NULL;
    void        *parsed = n00b_json_parse(msg, &errors);
    n00b_list_t *result;

    if (!errors || !n00b_list_len(errors)) {
        result = n00b_list(n00b_get_my_type(parsed));
        n00b_list_append(result, parsed);

        return result;
    }

    int n = n00b_list_len(errors);
    for (int i = 0; i < n; i++) {
        n00b_post_error(e, n00b_list_get(errors, i, NULL));
    }

    return NULL;
}

n00b_stream_filter_t *
n00b_new_to_json_xform(void)
{
    return n00b_new_filter(n00b_to_json_xform,
                           NULL,
                           n00b_cstring("to json"),
                           0);
}

void
n00b_add_to_json_xform_on_read(n00b_stream_t *party)
{
    n00b_add_read_filter(party, n00b_new_to_json_xform());
}

void
n00b_add_to_json_xform_on_write(n00b_stream_t *party)
{
    n00b_add_write_filter(party, n00b_new_to_json_xform());
}

n00b_stream_filter_t *
n00b_new_from_json_xform(void)
{
    return n00b_new_filter(n00b_from_json_xform,
                           NULL,
                           n00b_cstring("from json"),
                           0);
}

void
n00b_add_from_json_xform_on_read(n00b_stream_t *party)
{
    n00b_add_read_filter(party, n00b_new_from_json_xform());
}

void
n00b_add_from_json_xform_on_write(n00b_stream_t *party)
{
    n00b_add_write_filter(party, n00b_new_from_json_xform());
}

void
n00b_add_filter(n00b_stream_t *party, n00b_stream_filter_t *filter, bool read)
{
    defer_on();
    n00b_acquire_party(party);

    if (read) {
        if (!party->read_filters) {
            party->read_filters = n00b_list(n00b_type_ref());
        }
        if (!n00b_list_contains(party->read_filters, filter)) {
            n00b_list_append(party->read_filters, filter);
        }
    }
    else {
        if (!party->write_filters) {
            party->write_filters = n00b_list(n00b_type_ref());
        }

        if (!n00b_list_contains(party->write_filters, filter)) {
            n00b_list_append(party->write_filters, filter);
        }
    }
    Return;
    defer_func_end();
}
