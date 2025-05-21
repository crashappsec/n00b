#define N00B_USE_INTERNAL_API
#include "n00b.h"

static inline n00b_fd_sub_t *
sub_alloc(void)
{
    return n00b_gc_alloc_mapped(n00b_fd_sub_t, N00B_GC_SCAN_ALL);
}

static n00b_fd_sub_t *
raw_fd_subscribe(n00b_fd_stream_t *s,
                 void             *action,
                 n00b_fd_sub_kind  kind,
                 n00b_list_t      *subs,
                 va_list           args)
{
    if (!subs || s->fd == N00B_FD_CLOSED) {
        return NULL;
    }
    if (kind == N00B_FD_SUB_READ && s->read_closed) {
        return NULL;
    }
    if (s->write_closed) {
        if (kind == N00B_FD_SUB_QUEUED_WRITE
            || kind == N00B_FD_SUB_SENT_WRITE) {
            return NULL;
        }
    }

    int   n       = va_arg(args, int);
    int   oneshot = 0;
    void *thunk   = NULL;

    if (n) {
        oneshot = va_arg(args, int);
    }
    if (n > 1) {
        thunk = va_arg(args, void *);
    }

    va_end(args);

    n00b_fd_sub_t *result = sub_alloc();
    result->thunk         = thunk;
    result->oneshot       = (bool)oneshot;
    result->kind          = kind;
    result->action.msg    = action;

    n00b_private_list_append(subs, result);

    return result;
}

n00b_fd_sub_t *
_n00b_fd_read_subscribe(n00b_fd_stream_t *s, n00b_fd_cb action, ...)
{
    n00b_fd_sub_t *result;
    va_list        args;

    va_start(args, action);

    result = raw_fd_subscribe(s, action, N00B_FD_SUB_READ, s->read_subs, args);

    n00b_fd_become_worker(s);

    if (!s->r_added) {
        // TODO: much better to use the spin lock here.
        while (!s->needs_r) {
            s->needs_r = true;
        }
        n00b_list_append(s->evloop->pending, s);
    }

    n00b_fd_worker_yield(s);

    return result;
}

n00b_fd_sub_t *
_n00b_fd_enqueue_subscribe(n00b_fd_stream_t *s, n00b_fd_cb action, ...)
{
    va_list args;

    va_start(args, action);

    return raw_fd_subscribe(s,
                            action,
                            N00B_FD_SUB_QUEUED_WRITE,
                            s->queued_subs,
                            args);
}

n00b_fd_sub_t *
_n00b_fd_write_subscribe(n00b_fd_stream_t *s, n00b_fd_cb action, ...)
{
    va_list args;
    va_start(args, action);

    return raw_fd_subscribe(s,
                            action,
                            N00B_FD_SUB_SENT_WRITE,
                            s->sent_subs,
                            args);
}

n00b_fd_sub_t *
_n00b_fd_close_subscribe(n00b_fd_stream_t *s, n00b_close_cb action, ...)
{
    va_list args;
    va_start(args, action);

    return raw_fd_subscribe(s,
                            action,
                            N00B_FD_SUB_CLOSE,
                            s->close_subs,
                            args);
}

n00b_fd_sub_t *
_n00b_fd_error_subscribe(n00b_fd_stream_t *s, n00b_err_cb action, ...)
{
    va_list args;
    va_start(args, action);

    return raw_fd_subscribe(s,
                            action,
                            N00B_FD_SUB_ERROR,
                            s->err_subs,
                            args);
}

bool
n00b_fd_unsubscribe(n00b_fd_stream_t *s, n00b_fd_sub_t *sub)
{
    n00b_list_t *l = NULL;
    switch (sub->kind) {
    case N00B_FD_SUB_READ:
        l = s->read_subs;
        break;
    case N00B_FD_SUB_QUEUED_WRITE:
        l = s->queued_subs;
        break;
    case N00B_FD_SUB_SENT_WRITE:
        l = s->sent_subs;
        break;
    case N00B_FD_SUB_CLOSE:
        l = s->close_subs;
        break;
    case N00B_FD_SUB_ERROR:
        l = s->err_subs;
        break;
    }

    if (!l) {
        return false;
    }

    return n00b_private_list_remove_item(l, sub);
}

void
n00b_fd_post(n00b_fd_stream_t *s, n00b_list_t *subs, void *msg)
{
    if (!s) {
        return;
    }
    int n = n00b_list_len(subs);

    while (n--) {
        n00b_fd_sub_t *sub = n00b_private_list_get(subs, n, NULL);

        if (!sub) {
            continue;
        }
        (*sub->action.msg)(s, sub, msg, sub->thunk);
        if (sub->oneshot) {
            n00b_private_list_remove_item(subs, sub);
        }
    }
}

void
n00b_fd_post_close(n00b_fd_stream_t *s)
{
    n00b_raw_fd_close(s->fd);
    s->read_closed  = true;
    s->write_closed = true;

    while (n00b_list_len(s->close_subs)) {
        n00b_fd_sub_t *sub = n00b_list_pop(s->close_subs);
        if (!sub) {
            continue;
        }
        if (sub->action.close) {
            (*sub->action.close)(s, sub->thunk);
        }
    }

    s->read_subs   = NULL;
    s->queued_subs = NULL;
    s->sent_subs   = NULL;
    s->write_queue = NULL;
}

void
n00b_fd_post_error(n00b_fd_stream_t *s,
                   n00b_wq_item_t   *wq,
                   int               err_code,
                   bool              write,
                   bool              fatal)
{
    if (fatal) {
        n00b_fd_post_close(s);
    }

    int n = n00b_list_len(s->err_subs);

    if (!n) {
        return;
    }

    n00b_fd_err_t *err = n00b_gc_alloc_mapped(n00b_fd_err_t,
                                              N00B_GC_SCAN_ALL);
    err->stream        = s;
    err->write         = write;
    err->fatal         = fatal;
    err->err_code      = err_code;

    if (wq) {
        int l    = wq->cur - wq->start;
        err->msg = n00b_buffer_from_bytes(wq->start, l);
    }

    while (n--) {
        n00b_fd_sub_t *sub = n00b_list_get(s->err_subs, n, NULL);
        (*sub->action.err)(err, sub->thunk);
        if (sub->oneshot) {
            n00b_private_list_remove_item(s->err_subs, sub);
        }
    }
}
