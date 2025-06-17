#include "n00b.h"

static n00b_list_t *
n00b_apply_line_buffering(n00b_buf_t **state, void *msg)
{
    n00b_type_t *t      = n00b_get_my_type(msg);
    n00b_list_t *result = n00b_list(n00b_type_ref());
    n00b_buf_t  *input;
    n00b_buf_t  *tmp;

    if (!n00b_can_coerce(t, n00b_type_buffer())) {
        n00b_string_t *s = n00b_to_string(msg);

        if (!s) {
            /*
            n00b_post_cerror_ctx(e,
                                 "Cannot line buffer object that cannot "
                                 "be converted to a buffer or string.",
                                 msg);*/
            return result;
        }

        t   = n00b_type_string();
        msg = s;
    }

    input = n00b_coerce(msg, t, n00b_type_buffer());

    if (input->byte_len == 0) {
        return result;
    }

    n00b_buf_t *remainder = *state;
    char       *p         = input->data;
    char       *start     = p;
    char       *end       = p + n00b_buffer_len(input);
    int64_t     n;

    while (p < end) {
        if (*p++ == '\n') {
            if (start == input->data && p == end) {
                tmp = input;
            }
            else {
                n = p - start;

                tmp = n00b_new(n00b_type_buffer(), length : n, raw : start);
            }
            if (remainder) {
                tmp       = n00b_buffer_add(remainder, tmp);
                remainder = NULL;
            }
            n00b_list_append(result, tmp);
            start = p;
        }
    }

    if (start != end) {
        if (start == input->data) {
            *state = n00b_buffer_add(remainder, input);
        }
        else {
            n                    = end - start;
            n00b_buf_t *new_part = n00b_new(n00b_type_buffer(),
                                            length : n, raw : start);
            if (remainder) {
                *state = n00b_buffer_add(remainder, new_part);
            }
            else {
                *state = new_part;
            }
        }
    }

    return result;
}

static n00b_filter_impl line_buf_filter = {
    .cookie_size = sizeof(n00b_buf_t *),
    .read_fn     = (void *)n00b_apply_line_buffering,
    .write_fn    = (void *)n00b_apply_line_buffering,
    .name        = NULL,
};

n00b_filter_spec_t *
n00b_filter_apply_line_buffering(void)
{
    if (!line_buf_filter.name) {
        line_buf_filter.name = n00b_cstring("line_buffer");
    }

    n00b_filter_spec_t *result = n00b_gc_alloc(n00b_filter_spec_t);
    result->impl               = &line_buf_filter;
    result->policy             = N00B_FILTER_WRITE | N00B_FILTER_READ;

    return result;
}
