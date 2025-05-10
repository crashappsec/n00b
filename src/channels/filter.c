#include "n00b.h"

// This assumes filters are added atomicly. May add a setup lock soon.
bool
n00b_filter_add(n00b_channel_t *c, n00b_filter_impl *impl, int policy)
{
    int            sz = impl->cookie_size;
    n00b_filter_t *f  = n00b_gc_alloc(sizeof(n00b_filter_t) + sz);

    if (policy <= 0 || policy > N00B_FILTER_MAX) {
        N00B_CRAISE("Invalid operation policy for filter.");
    }

    if ((policy & N00B_FILTER_READ) && !c->r) {
        // Cannot add; not enabled for reads.
        return false;
    }

    if ((policy & N00B_FILTER_WRITE) && !c->w) {
        return false;
    }

    if (!impl->write_fn || (c->w && !(policy & N00B_FILTER_WRITE))) {
        f->w_skip = true;
    }

    if (!impl->read_fn || (c->r && !(policy & N00B_FILTER_READ))) {
        f->r_skip = true;
    }

    if (c->r && !(policy & N00B_FILTER_READ)) {
        f->r_skip = true;
    }

    if (f->r_skip && f->w_skip) {
        return false;
    }

    if (!c->write_top) {
        c->write_top = f;
    }
    else {
        c->read_top->next_write_step = f;
    }

    f->impl           = impl;
    f->next_read_step = c->read_top;
    c->read_top       = f;

    return true;
}

// The pkg construct is currently more to be able to add debug logging if
// needed.
n00b_list_t *
n00b_filter_writes(n00b_channel_t *c, n00b_cmsg_t *pkg)
{
    n00b_filter_t *f      = c->write_top;
    int            n_msgs = pkg->nitems;
    void          *msg;

    n00b_list_t *cur_msgs;  // In the process of passing
    n00b_list_t *pass;      // One set of msgs to pass to next filter level.
    n00b_list_t *next_msgs; // All msgs currently bound for next level.

    if (n_msgs == 1) {
        cur_msgs = n00b_list(n00b_type_ref());
        n00b_list_append(cur_msgs, pkg->payload);
    }
    else {
        cur_msgs = pkg->payload;
    }

    next_msgs = cur_msgs;

    while (f) {
        if (!f->w_skip) {
            next_msgs = n00b_list(n00b_type_ref());
            for (int i = 0; i < n_msgs; i++) {
                msg  = n00b_private_list_get(cur_msgs, i, NULL);
                pass = (*f->impl->write_fn)((void *)f->cookie, msg);
                if (!pass || !n00b_list_len(pass)) {
                    continue;
                }
                next_msgs = n00b_private_list_plus(next_msgs, pass);
            }

            if (!n00b_list_len(next_msgs)) {
                return NULL;
            }
        }
        f = f->next_write_step;
    }

    return next_msgs;
}

n00b_list_t *
n00b_filter_reads(n00b_channel_t *c, n00b_cmsg_t *pkg)
{
    n00b_filter_t *f      = c->read_top;
    int            n_msgs = pkg->nitems;
    void          *msg;
    n00b_list_t   *cur_msgs;  // In the process of passing
    n00b_list_t   *pass;      // One set of msgs to pass to next filter level.
    n00b_list_t   *next_msgs; // All msgs currently bound for next level.

    if (n_msgs == 1) {
        cur_msgs = n00b_list(n00b_type_ref());
        n00b_list_append(cur_msgs, pkg->payload);
    }
    else {
        cur_msgs = pkg->payload;
    }

    next_msgs = cur_msgs;

    while (f) {
        if (!f->r_skip) {
            next_msgs = n00b_list(n00b_type_ref());
            for (int i = 0; i < n_msgs; i++) {
                msg  = n00b_private_list_get(cur_msgs, i, NULL);
                pass = (*f->impl->read_fn)((void *)f->cookie, msg);
                if (!pass || !n00b_list_len(pass)) {
                    continue;
                }
                next_msgs = n00b_private_list_plus(next_msgs, pass);
            }

            if (!n00b_list_len(next_msgs)) {
                return NULL;
            }
        }
        f = f->next_read_step;
    }

    return next_msgs;
}
