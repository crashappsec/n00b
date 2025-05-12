#define N00B_USE_INTERNAL_API
#include "n00b.h"

static int
stringchan_init(n00b_channel_t *stream, void *args)
{
    n00b_string_channel_t *c = n00b_get_channel_cookie(stream);
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

    stream->name = n00b_cformat("String Channel @[|#:p|]", c);

    return O_RDWR;
}

static n00b_string_t *
stringchan_read(n00b_channel_t *stream, bool *err)
{
    n00b_string_channel_t *c      = n00b_get_channel_cookie(stream);
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
stringchan_write(n00b_channel_t *stream, void *msg, bool blocking)
{
    if (n00b_type_is_buffer(n00b_get_my_type(msg))) {
        msg = n00b_buf_to_string(msg);
    }

    n00b_string_channel_t *c = n00b_get_channel_cookie(stream);
    n00b_list_append(c->l, msg);
}

static bool
stringchan_eof(n00b_channel_t *stream)
{
    n00b_string_channel_t *c = n00b_get_channel_cookie(stream);
    bool                   result;

    n00b_lock_list(c->l);
    result = n00b_list_len(c->l) == c->ix;
    n00b_unlock_list(c->l);

    return result;
}

static n00b_chan_impl n00b_bufchan_impl = {
    .cookie_size             = sizeof(n00b_string_channel_t),
    .init_impl               = (void *)stringchan_init,
    .read_impl               = (void *)stringchan_read,
    .write_impl              = (void *)stringchan_write,
    .eof_impl                = (void *)stringchan_eof,
    .poll_for_blocking_reads = true,
};
