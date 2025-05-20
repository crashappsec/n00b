#define N00B_USE_INTERNAL_API
#include "n00b.h"

static int
string_stream_init(n00b_stream_t *stream, void *args)
{
    n00b_string_cookie_t *c = n00b_get_stream_cookie(stream);
    c->ix                    = 0;

    if (!args) {
        c->l = n00b_list(n00b_type_string());
    }
    else {
        if (n00b_type_is_list(n00b_get_my_type(args))) {
            c->l = args;
        }
        else {
            c->l = n00b_list(n00b_type_string());
            n00b_list_append(c->l, args);
        }
    }

    stream->name = n00b_cformat("String Stream @[|#:p|]", c);

    return O_RDWR;
}

static n00b_string_t *
string_stream_read(n00b_stream_t *stream, bool *err)
{
    n00b_string_cookie_t *c      = n00b_get_stream_cookie(stream);
    n00b_string_t         *result = n00b_list_get(c->l, c->ix, NULL);

    if (!result) {
        *err = true;
    }
    else {
        *err = false;
        c->ix++;
    }

    return result;
}

static void
string_stream_write(n00b_stream_t *stream, void *msg, bool blocking)
{
    if (n00b_type_is_buffer(n00b_get_my_type(msg))) {
        msg = n00b_buf_to_string(msg);
    }

    n00b_string_cookie_t *c = n00b_get_stream_cookie(stream);
    n00b_list_append(c->l, msg);
}

static bool
string_stream_eof(n00b_stream_t *stream)
{
    n00b_string_cookie_t *c = n00b_get_stream_cookie(stream);
    bool                   result;

    n00b_lock_list(c->l);
    result = n00b_list_len(c->l) == c->ix;
    n00b_unlock_list(c->l);

    return result;
}

static n00b_stream_impl n00b_string_stream_impl = {
    .cookie_size             = sizeof(n00b_string_cookie_t),
    .init_impl               = (void *)string_stream_init,
    .read_impl               = (void *)string_stream_read,
    .write_impl              = (void *)string_stream_write,
    .eof_impl                = (void *)string_stream_eof,
    .poll_for_blocking_reads = true,
};

n00b_stream_t *
_n00b_new_string_stream(void *seed, ...)
{
    n00b_list_t *fl;
    n00b_build_filter_list(fl, seed);

    return n00b_new(n00b_type_stream(), &n00b_string_stream_impl, seed, fl);
}
