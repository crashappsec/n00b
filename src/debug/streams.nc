#define N00B_USE_INTERNAL_API
#include "n00b.h"

static bool         stream_debugging_on = true;
static bool         stream_cleanup      = true;
static n00b_list_t *stream_registry;

static once void
stream_debug_setup(void)
{
    n00b_gc_register_root(&stream_registry, 1);
    stream_registry = n00b_list(n00b_type_stream());
}

void
n00b_stream_debug_register(n00b_stream_t *stream)
{
    if (!stream_debugging_on) {
        return;
    }

    stream_debug_setup();

    n00b_list_append(stream_registry, stream);
}

void
n00b_stream_debug_deregister(n00b_stream_t *stream)
{
    if (!stream_debugging_on || !stream_cleanup) {
        return;
    }

    n00b_list_remove_item(stream_registry, stream);
}

static inline n00b_string_t *
repr_perms(n00b_stream_t *stream)
{
    int            perms = ((int)stream->w) << 1 | (int)stream->r;
    n00b_string_t *s;
    n00b_string_t *pstr = NULL;

    switch (perms) {
    case 0:
        return n00b_cstring("closed");
    case 1:
        s = n00b_cstring("r");
        break;
    case 2:
        s = n00b_cstring("w");
        break;
    default:
        s = n00b_cstring("rw");
        break;
    }

    if (!stream->fd_backed) {
        return s;
    }

    n00b_fd_cookie_t   *c      = n00b_get_stream_cookie(stream);
    int                 fd     = c->stream->fd;
    n00b_event_loop_t  *evloop = c->stream->evloop;
    n00b_pevent_loop_t *ploop  = &evloop->algo.poll;

    for (int i = 0; i < ploop->pollset_last; i++) {
        struct pollfd *p = &ploop->pollset[i];
        if (p->fd == fd) {
            int x = 0;
            if (p->events & POLLIN) {
                x = 1;
            }
            if (p->events & POLLOUT) {
                x += 2;
            }

            switch (x) {
            case 0:
                pstr = n00b_cstring("poll: -");
                break;
            case 1:
                pstr = n00b_cstring("poll: r");
                break;
            case 2:
                pstr = n00b_cstring("poll: w");
                break;
            default:
                pstr = n00b_cstring("poll: rw");
                break;
            }

            x = 0;
            if (p->revents & POLLIN) {
                x = 1;
            }
            if (p->revents & POLLOUT) {
                x += 2;
            }

            switch (x) {
            case 0:
                break;
            case 1:
                pstr = n00b_cformat("«#»!r", pstr);
                break;
            case 2:
                pstr = n00b_cformat("«#»!w", pstr);
                break;
            default:
                pstr = n00b_cformat("«#»!rw", pstr);
                break;
            }

            return n00b_cformat("«#» («#»)",
                                s,
                                pstr);
        }
    }

    return s;
}

static inline n00b_string_t *
prep_read_subs(n00b_stream_t *stream)
{
    n00b_list_t *r   = n00b_observable_all_subscribers(&stream->pub_info,
                                                     (void *)(int64_t)N00B_CT_R);
    n00b_list_t *raw = n00b_observable_all_subscribers(&stream->pub_info,
                                                       (void *)(int64_t)N00B_CT_RAW);
    int          ct  = 0;

    if (r) {
        ct = n00b_list_len(r);
    }
    if (raw) {
        ct += n00b_list_len(raw);
    }

    if (!ct) {
        return n00b_cstring(" ");
    }

    n00b_list_t *items = n00b_list(n00b_type_string());

    if (r) {
        for (int i = 0; i < n00b_list_len(r); i++) {
            // This will break if we move where observables live in
            // the data structure.
            n00b_observer_t *item   = n00b_list_get(r, i, NULL);
            n00b_stream_t   *target = (void *)item->subscriber;
            n00b_list_append(items, n00b_cformat("«#:x»", target->stream_id));
        }
    }

    if (raw) {
        for (int i = 0; i < n00b_list_len(raw); i++) {
            n00b_observer_t *item   = n00b_list_get(raw, i, NULL);
            n00b_stream_t   *target = (void *)item->subscriber;
            n00b_list_append(items, n00b_cformat("«#:x» (raw)", target->stream_id));
        }
    }

    return n00b_string_join(items, n00b_cached_comma_padded());
}

static inline n00b_string_t *
prep_filters(n00b_stream_t *stream)
{
    if (!stream->write_top) {
        return n00b_cached_space();
    }

    n00b_list_t *l = n00b_list(n00b_type_ref());

    n00b_filter_t *f = stream->write_top;
    while (f) {
        n00b_string_t *mode;

        if (!f->w_skip && !f->r_skip) {
            mode = n00b_cstring("rw");
        }
        else {
            if (f->w_skip) {
                mode = n00b_cstring("r");
            }
            else {
                mode = n00b_cstring("w");
            }
        }

        n00b_list_append(l, n00b_cformat("«#» («#»)", f->impl->name, mode));
        f = f->next_write_step;
    }

    return n00b_string_join(l, n00b_cached_comma_padded());
}

static inline n00b_string_t *
prep_write_subs(n00b_stream_t *stream)
{
    n00b_list_t *q  = n00b_observable_all_subscribers(&stream->pub_info,
                                                     (void *)(int64_t)N00B_CT_Q);
    n00b_list_t *w  = n00b_observable_all_subscribers(&stream->pub_info,
                                                     (void *)(int64_t)N00B_CT_W);
    int          ct = 0;

    if (q) {
        ct = n00b_list_len(q);
    }
    if (w) {
        ct += n00b_list_len(w);
    }

    if (!ct) {
        return n00b_cstring(" ");
    }

    n00b_list_t *items = n00b_list(n00b_type_string());

    if (q) {
        for (int i = 0; i < n00b_list_len(q); i++) {
            n00b_observer_t *item   = n00b_list_get(q, i, NULL);
            n00b_stream_t   *target = (void *)item->subscriber;
            n00b_list_append(items,
                             n00b_cformat("«#:x» (q)",
                                          target->stream_id));
        }
    }

    if (w) {
        for (int i = 0; i < n00b_list_len(w); i++) {
            n00b_observer_t *item = n00b_list_get(q, i, NULL);
            if (!item) {
                continue;
            }
            n00b_stream_t *target = (void *)item->subscriber;
            n00b_list_append(items,
                             n00b_cformat("«#:x» (deliver)",
                                          target->stream_id));
        }
    }

    n00b_list_append(items, prep_filters(stream));

    return n00b_string_join(items, n00b_cached_comma_padded());
}

static inline n00b_list_t *
prep_one_stream(n00b_stream_t *stream)
{
    n00b_list_t *row = n00b_list(n00b_type_string());
    n00b_private_list_append(row,
                             n00b_cformat("«#:x»",
                                          (int64_t)stream->stream_id));

    n00b_string_t *name     = n00b_to_string(stream);
    n00b_string_t *fd_extra = n00b_get_fd_repr(stream);

    if (fd_extra) {
        name = n00b_cformat("«#»: «#»", name, fd_extra);
    }

    n00b_private_list_append(row, name);
    n00b_private_list_append(row, repr_perms(stream));
    n00b_private_list_append(row, prep_read_subs(stream));
    n00b_private_list_append(row, prep_write_subs(stream));
    n00b_private_list_append(row, prep_filters(stream));

    return row;
}

static inline n00b_list_t *
header_row(void)
{
    n00b_list_t *row = n00b_list(n00b_type_string());
    n00b_list_append(row, n00b_cstring("SID"));
    n00b_list_append(row, n00b_cstring("Name"));
    n00b_list_append(row, n00b_cstring("State"));
    n00b_list_append(row, n00b_cstring("Read Subs"));
    n00b_list_append(row, n00b_cstring("Write Subs"));
    n00b_list_append(row, n00b_cstring("Filters"));

    return row;
}

n00b_string_t *
n00b_get_streams_table(void)
{
    if (!stream_debugging_on) {
        return NULL;
    }

    n00b_table_t *t = n00b_table(columns : 6);
    n00b_list_t  *l = n00b_list_shallow_copy(stream_registry);
    int           n = n00b_list_len(l);

    n00b_table_add_row(t, header_row());

    for (int i = 0; i < n; i++) {
        n00b_table_add_row(t, prep_one_stream(n00b_list_get(l, i, NULL)));
    }

    l = n00b_render(t, n00b_terminal_width(), -1);
    return n00b_string_join(l, n00b_cached_empty_string());
}

void
n00b_show_streams(void)
{
    n00b_string_t *s = n00b_get_streams_table();
    if (s) {
        n00b_eprint(s);
    }
    else {
        n00b_eprint("Stream debugging is not enabled.");
    }
}

void
n00b_log_streams(FILE *f)
{
    n00b_string_t *s = n00b_get_streams_table();
    if (s) {
        fprintf(f, "%s", s->data);
    }
}

void
n00b_log_streams_to_file(char *fname)
{
    FILE *f = fopen(fname, "w");
    if (f) {
        n00b_log_streams(f);
    }
    fclose(f);
}

void
n00b_stream_debug_enable(bool cleanup)
{
    stream_debugging_on = true;
    stream_cleanup      = cleanup;
}

void
n00b_stream_debug_disable(void)
{
    stream_debugging_on = false;
}
