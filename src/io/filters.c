#define N00B_USE_INTERNAL_API
#include "n00b.h"

n00b_stream_filter_t *
n00b_new_filter(n00b_filter_fn xform_fn,
                n00b_filter_fn flush_fn,
                n00b_utf8_t   *name,
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

typedef struct {
    // Because the most we might queue up is a three bytes of a
    // four-byte UTF-8 sequence, the queue would only consist of
    // condition variables, if anything at all.
    n00b_list_t *queue;
    unsigned int partial_len : 2;
    char         partial[3];
} fu8_state_t;

// This filter takes a buffer, utf8 string, utf32
// string or message, and converts them to a canonical utf8
// representation if possible. Whenever there are errors,
// an error posts, with the contents stored in the 'context' field.

static n00b_list_t *
n00b_u8_flush(n00b_stream_t *e, void *ctx, void *ignore)
{
    fu8_state_t *state = ctx;

    if (state->partial_len) {
        state->partial_len = 0;
        n00b_post_cerror(e, "Flush operation dropped partial utf-8 char.");
    }

    n00b_list_t *result = state->queue;
    state->queue        = NULL;

    return result;
}

static n00b_list_t *
n00b_to_u8_xform(n00b_stream_t *e, void *ctx, void *msg)
{
    fu8_state_t     *state    = ctx;
    n00b_type_t     *t        = n00b_get_my_type(msg);
    n00b_list_t     *result   = n00b_list(n00b_type_ref());
    bool             bad_utf8 = false;
    n00b_utf8_t     *output   = NULL;
    n00b_utf8_t     *tmp;
    n00b_buf_t      *tbuf = NULL;
    n00b_buf_t      *msg_as_buf;
    int              len;
    char            *start;
    char            *end;
    char            *p;
    n00b_codepoint_t cp;

    if (n00b_type_is_condition(t)) {
        if (!state->partial_len) {
            n00b_list_append(result, msg);
        }
        else {
            // We're not ready to return anything.
            n00b_list_append(state->queue, msg);
        }
        return result;
    }

    if (state->partial_len && !n00b_type_is_buffer(t)) {
        n00b_post_cerror_ctx(e,
                             "Buffered utf8 state was not followed "
                             "by a raw buffer.",
                             msg);
    }

    if (n00b_type_is_buffer(t)) {
        msg_as_buf = msg;
    }
    else {
        if (n00b_type_is_string(t)) {
            tmp = n00b_to_utf8(msg);
        }
        else {
            tmp = n00b_object_repr_opt(msg);
            if (!tmp) {
                n00b_post_cerror_ctx(e, "Object doesn't have a repr.", msg);
                return result;
            }
        }

        msg_as_buf = n00b_new(n00b_type_buffer(),
                              n00b_kw("raw",
                                      tmp->data,
                                      "length",
                                      n00b_str_byte_len(tmp)));
    }

    n00b_buffer_acquire_r(msg_as_buf);

    len = n00b_buffer_len(msg_as_buf);

    if (state->partial_len) {
        tbuf = n00b_new(n00b_type_buffer(),
                        n00b_kw("length",
                                n00b_ka(len + state->partial_len)));

        n00b_buffer_acquire_w(tbuf);
        memcpy(tbuf->data, state->partial, state->partial_len);
        memcpy(tbuf->data + state->partial_len, msg_as_buf->data, len);
        len += state->partial_len;
        state->partial_len = 0;
        n00b_buffer_release(msg_as_buf);
        msg_as_buf = tbuf;
    }

    start = msg_as_buf->data;
    p     = msg_as_buf->data;
    end   = p + len;

    while (p < end) {
        int n = utf8proc_iterate((void *)p, len, &cp);
        if (n < 1) {
            if (p + 3 > end) {
                state->partial_len = end - p;
                for (int i = 0; i < n; i++) {
                    state->partial[i] = *p++;
                }
                break;
            }

            bad_utf8 = true;

            if (p != start) {
                int64_t lo_len = p - start;
                tmp            = n00b_new(n00b_type_utf8(),
                               n00b_kw("start", start, "length", lo_len));
                if (!output) {
                    output = tmp;
                }
                else {
                    output = n00b_str_concat(output, tmp);
                }
            }
            p++;
            continue;
        }
        p += n;
    }

    if (output) {
        n00b_list_append(result, output);

        if (state->queue && n00b_list_len(state->queue) != 0) {
            result       = n00b_list_plus(result, state->queue);
            state->queue = n00b_list(n00b_type_ref());
        }
    }

    n00b_buffer_release(msg_as_buf);

    if (bad_utf8) {
        n00b_post_cerror_ctx(e,
                             "Stripped invalid UTF-8 byte sequence(s)",
                             (tbuf ? tbuf : msg));
    }

    return result;
}

n00b_stream_filter_t *
n00b_new_ensure_utf8_filter(n00b_stream_t *party)
{
    n00b_stream_filter_t *r;

    r = n00b_new_filter(n00b_to_u8_xform,
                        n00b_u8_flush,
                        n00b_new_utf8("ensure utf8"),
                        sizeof(fu8_state_t));

    fu8_state_t *s = r->state;
    s->queue       = n00b_list(n00b_type_ref());

    return r;
}

void
n00b_ensure_utf8_on_read(n00b_stream_t *party)
{
    n00b_add_filter(party, n00b_new_ensure_utf8_filter(party), true);
}

void
n00b_ensure_utf8_on_write(n00b_stream_t *party)
{
    n00b_add_filter(party, n00b_new_ensure_utf8_filter(party), false);
}

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
        n00b_utf8_t *s = n00b_object_repr_opt(msg);

        if (!s) {
            n00b_post_cerror_ctx(e,
                                 "Cannot line buffer object that cannot "
                                 "be converted to a buffer or string.",
                                 msg);
            return result;
        }

        t   = n00b_type_utf8();
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
                           n00b_new_utf8("line buffer"),
                           sizeof(lb_state_t));
}

void
n00b_line_buffer_reads(n00b_stream_t *party)
{
    n00b_add_filter(party, n00b_new_line_buffering_xform(), true);
}

void
n00b_line_buffer_writes(n00b_stream_t *party)
{
    n00b_add_filter(party, n00b_new_line_buffering_xform(), false);
}

n00b_list_t *
n00b_hex_dump_xform(n00b_stream_t *stream, void *ctx, void *msg)
{
    n00b_utf8_t *repr;
    n00b_type_t *t = n00b_get_my_type(msg);

    if (n00b_type_is_buffer(t)) {
        n00b_buf_t *b = msg;
        repr          = n00b_hex_dump(b->data, n00b_len(b));
    }
    else {
        if (n00b_type_is_string(t)) {
            n00b_utf8_t *str = n00b_to_utf8(msg);
            repr             = n00b_hex_dump(str->data, str->byte_len);
        }
        else {
            repr = n00b_hex_debug_repr(msg);
        }
    }
    int64_t *count_ptr = ctx;
    int64_t  count     = 1 + *count_ptr;
    *count_ptr         = count;
    uint64_t *ptr      = n00b_box_u64((uint64_t)stream);

    n00b_utf8_t *s = n00b_cstr_format("{} @{:x} (msg: {}, ty: {}):\n{}",
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
        n00b_new_utf8("custom callback"),
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

static n00b_list_t *
n00b_to_json_xform(n00b_stream_t *e, void *ignore, void *msg)
{
    n00b_utf8_t *s = n00b_to_json(msg);

    if (!s) {
        n00b_post_cerror_ctx(e, "Value can't be converted to JSon.", msg);
        return NULL;
    }

    n00b_list_t *result = n00b_list(n00b_type_utf8());
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
                           n00b_new_utf8("to json"),
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
                           n00b_new_utf8("from json"),
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
    n00b_release_party(party);
}
