#define N00B_USE_INTERNAL_API
#include "n00b.h"

static void
process_write_queue(n00b_fd_stream_t *s)
{
    // The FD is in non-blocking mode while we're doing this.
    n00b_wq_item_t *qitem = n00b_list_get(s->write_queue, 0, NULL);
    n00b_buf_t     *b;
    int             l;

    while (qitem) {
        int val = write(s->fd, qitem->cur, qitem->remaining);

        if (val > 0) {
            qitem->cur += val;
            qitem->remaining -= val;
            s->total_written += val;

            if (!qitem->remaining) {
                n00b_list_dequeue(s->write_queue);
                l = qitem->cur - qitem->start;
                b = n00b_buffer_from_bytes(qitem->start, l);
                n00b_fd_post(s, s->sent_subs, b);
                qitem = n00b_list_get(s->write_queue, 0, NULL);
            }
            continue;
        }

        switch (errno) {
        case EAGAIN:
            return;
        case EINTR:
            continue;
        case ECONNRESET:
            s->write_closed = true;
            if (s->read_closed) {
                n00b_fd_post_close(s);
            }
            return;
        case EFBIG:
        case EINVAL:
        case ENETDOWN:
        case ENETUNREACH:
            n00b_fd_post_error(s, qitem, errno, true, false);
            return;
        default:
            n00b_fd_post_error(s, qitem, errno, true, true);
            return;
        }
    }
}

static inline n00b_buf_t *
assemble_buffer(n00b_rdbuf_t *cache, int len)
{
    n00b_buf_t *result = n00b_buffer_empty();
    result->data       = n00b_gc_array_value_alloc(char, len);
    char *p            = result->data;
    int   total        = 0;

    while (cache) {
        assert(cache->len + total <= len);
        memcpy(p, cache->segment, cache->len);
        total += cache->len;
        p += cache->len;
        assert(!cache->next || n00b_in_heap(cache->next));
        cache = cache->next;
    }

    result->byte_len = len;

    return result;
}

// This is the internal synchronous call for reading from a
// non-blocking file descriptor.
//
// Returns true to 'take it off the poll list'
// Reads until drained.
// We keep a linked list of PIPE_BUF length statements
bool
n00b_handle_one_read(n00b_fd_stream_t *s)
{
    char          tmpbuf[PIPE_BUF];
    n00b_rdbuf_t *first = NULL;
    n00b_rdbuf_t *last  = NULL;
    n00b_buf_t   *msg   = NULL;
    int           total = 0;

    if (s->listener) {
        struct sockaddr addr;
        socklen_t       addr_len = sizeof(struct sockaddr);

        int sock = accept(s->fd, &addr, &addr_len);

        if (sock == -1) {
            int e = errno;
            n00b_fd_post_error(s, NULL, e, false, true);
            switch (errno) {
            case EBADF:
            case EINVAL:
                n00b_fd_discovered_read_close(s);
                return true;
            }
            return false;
        }

        n00b_net_addr_t  *saddr = n00b_new(n00b_type_net_addr(),
                                          n00b_kw("sockaddr", &addr));
        n00b_fd_stream_t *fd    = n00b_fd_stream_from_fd(sock, NULL, NULL);
        n00b_stream_t    *strm  = n00b_new_fd_stream(fd, saddr);

        n00b_fd_stream_nonblocking(fd);
        n00b_fd_post(s, s->read_subs, strm);
        return false;
    }

    while (true) {
        // If the fd's been set back to blocking, we'd like to undo that;
        // ideally we have exclusive access here.
        n00b_fd_stream_nonblocking(s);
        int val = read(s->fd, tmpbuf, PIPE_BUF);
        if (val == 0) {
            n00b_dlog_io("Read %d bytes from fd %d", total, s->fd);
            // If it's a socket, reading EOF tells us it's closed.
            // For a regular file, it does NOT.
            if (s->socket) {
                n00b_fd_discovered_read_close(s);
            }
            return true;
        }
        if (val < 0) {
            switch (errno) {
            case EINTR:
                continue;
            case EAGAIN:
                if (first) {
                    msg = assemble_buffer(first, total);
                    n00b_fd_post(s, s->read_subs, msg);
                    first = last = NULL;
                }
                if (s->r_added && !n00b_list_len(s->read_subs)) {
                    return true;
                }
                return false;
            default:
                n00b_dlog_io("Errno %d when reading from fd %d (after %d bytes)",
                             errno,
                             s->fd,
                             total);
                n00b_fd_post_error(s, NULL, errno, false, true);
                return true;
            }
        }

        s->total_read += val;
        total += val;

        if (!first) {
            first = n00b_gc_alloc_mapped(n00b_rdbuf_t,
                                         N00B_GC_SCAN_ALL);
            last  = first;
        }
        else {
            last->next = n00b_gc_alloc_mapped(n00b_rdbuf_t,
                                              N00B_GC_SCAN_ALL);
            last       = last->next;
        }

        last->len = val;
        memcpy(last->segment, tmpbuf, val);
    }
}

bool
n00b_handle_one_write(n00b_fd_stream_t *s)
{
    process_write_queue(s);
    return n00b_list_len(s->write_queue) == 0;
}

// non-blocking.
bool
n00b_fd_send(n00b_fd_stream_t *s, char *bytes, int len)
{
    if (!s->write_queue || s->fd == N00B_FD_CLOSED) {
        return false;
    }

    if (!len) {
        return true;
    }

    n00b_thread_t  *expected = NULL;
    n00b_thread_t  *me       = n00b_thread_self();
    n00b_wq_item_t *qitem    = n00b_gc_alloc_mapped(n00b_wq_item_t,
                                                 N00B_GC_SCAN_ALL);
    qitem->start             = bytes;
    qitem->cur               = bytes;
    qitem->remaining         = len;

    n00b_list_append(s->write_queue, qitem);

    n00b_fd_post(s, s->queued_subs, n00b_buffer_from_bytes(bytes, len));

    if (CAS(&s->worker, &expected, me)) {
        // Double check.
        if (atomic_read(&s->worker) == me) {
            process_write_queue(s);
            n00b_fd_worker_yield(s);
        }
    }

    return true;
}

bool
n00b_fd_write(n00b_fd_stream_t *s, char *bytes, int len)
{
    if (!s->write_queue || s->fd == N00B_FD_CLOSED) {
        return false;
    }

    if (!len) {
        return true;
    }

    n00b_wq_item_t *qitem = n00b_gc_alloc_mapped(n00b_wq_item_t,
                                                 N00B_GC_SCAN_ALL);
    qitem->start          = bytes;
    qitem->cur            = bytes;
    qitem->remaining      = len;

    n00b_list_append(s->write_queue, qitem);

    n00b_fd_post(s, s->queued_subs, n00b_buffer_from_bytes(bytes, len));

    n00b_fd_become_worker(s);
    n00b_fd_stream_blocking(s);

    qitem = n00b_list_get(s->write_queue, 0, NULL);

    while (qitem) {
        N00B_DBG_CALL(n00b_thread_suspend);
        int val = write(s->fd, qitem->cur, qitem->remaining);
        N00B_DBG_CALL(n00b_thread_resume);

        if (val > 0) {
            qitem->cur += val;
            qitem->remaining -= val;
            s->total_written += val;

            if (!qitem->remaining) {
                n00b_list_dequeue(s->write_queue);
                int         l = qitem->cur - qitem->start;
                n00b_buf_t *b = n00b_buffer_from_bytes(qitem->start, l);

                n00b_fd_post(s, s->sent_subs, b);
                qitem = n00b_list_get(s->write_queue, 0, NULL);
            }
            continue;
        }

        switch (errno) {
        case EINTR:
            continue;
        case EFBIG:
        case EINVAL:
        case ENETDOWN:
        case ENETUNREACH:
            n00b_fd_stream_nonblocking(s);
            n00b_fd_post_error(s, qitem, errno, true, false);
            n00b_fd_worker_yield(s);
            return false;
        default:
            n00b_fd_post_error(s, qitem, errno, true, true);
            n00b_fd_worker_yield(s);
            return false;
        }
    }

    n00b_fd_stream_nonblocking(s);
    n00b_fd_worker_yield(s);

    return true;
}

static inline n00b_buf_t *
blocking_greedy_read(n00b_fd_stream_t *s,
                     int               ms_timeout,
                     bool             *err)
{
    char          tmpbuf[PIPE_BUF];
    n00b_rdbuf_t *first = NULL;
    n00b_rdbuf_t *last  = NULL;
    n00b_buf_t   *msg   = NULL;
    int           total = 0;

    struct pollfd fds[1] = {
        {
            .fd      = s->fd,
            .events  = POLLIN,
            .revents = 0,
        },
    };

    if (ms_timeout <= 0) {
        ms_timeout = -1;
    }

    int r = poll(fds, 1, 0);

    if (!r) {
        n00b_fd_worker_yield(s);
        return NULL;
    }

    if (!(fds[0].revents & POLLIN)) {
        n00b_fd_post_error(s, NULL, errno, false, true);
        *err = true;
        return NULL;
    }

    while (true) {
        n00b_fd_stream_nonblocking(s);

        int val = read(s->fd, tmpbuf, PIPE_BUF);
        if (val == 0) {
            // If it's a socket, reading EOF tells us it's closed.
            // For a regular file, it does NOT.
            if (!s->plain_file) {
                n00b_fd_discovered_read_close(s);
            }
            goto finish;
        }

        if (val < 0) {
            switch (errno) {
            case EINTR:
                continue;
            default:
                n00b_fd_post_error(s, NULL, errno, false, true);
                // fallthrough
            case EAGAIN:
finish:
                if (first) {
                    msg = assemble_buffer(first, total);
                    n00b_fd_post(s, s->read_subs, msg);
                    first = last = NULL;
                }

                n00b_fd_worker_yield(s);
                return msg;
            }
        }

        s->total_read += val;
        total += val;

        if (!first) {
            first = n00b_gc_alloc_mapped(n00b_rdbuf_t,
                                         N00B_GC_SCAN_ALL);
            last  = first;
        }
        else {
            last->next = n00b_gc_alloc_mapped(n00b_rdbuf_t,
                                              N00B_GC_SCAN_ALL);
            last       = last->next;
        }

        last->next = NULL;

        if (val > PIPE_BUF) {
            val = PIPE_BUF;
            n00b_unreachable();
        }
        last->len = val;
        memcpy(last->segment, tmpbuf, val);
        continue;
    }
}

// Synchronous read.
// 'full' parameter will block until the first of the following:

// 1) `len` bytes are available
// 2) EOF is reached.
// 3) The timeout is reached.
// 4) The fd closes / errors on us.
//
// The 'full' parameter indicates we should ignore EOF, because not
// all the data may be available.
//
//
// We are still going to use non-blocking reads, until we get EAGAIN
// or EINTR.  At that point, we will poll until there's data.
//
// If, on the other hand, a negative number is passed for the length,
// we block (if necessary) until anything is available, and then we
// read as much data as we can before blocking!

n00b_buf_t *
n00b_fd_read(n00b_fd_stream_t *s, int len, int ms_timeout, bool full, bool *err)
{
    if (!s->read_subs || s->fd == N00B_FD_CLOSED) {
        *err = true;
        return NULL;
    }

    *err = false;

    n00b_fd_become_worker(s);

    if (len <= 0) {
        return blocking_greedy_read(s, ms_timeout, err);
    }

    char buf[len];

    n00b_buf_t      *b         = NULL;
    char            *p         = buf;
    int              remaining = len;
    n00b_duration_t *end       = NULL;
    n00b_duration_t  now;

    struct pollfd fds[1] = {
        {
            .fd      = s->fd,
            .events  = POLLIN,
            .revents = 0,
        },
    };

    if (ms_timeout > 0) {
        end = n00b_new_ms_timeout(ms_timeout);
    }
    else {
        ms_timeout = -1;
    }

    n00b_fd_stream_nonblocking(s);
    int r = poll(fds, 1, 0);

    while (true) {
        // The first time through, the 'r' is from the no-delay poll.
        switch (r) {
        case 0:
            goto wait;
        case -1:
            switch (errno) {
            case EAGAIN:
                goto wait;
            case EINTR:
                continue;
            default:
                n00b_fd_post_error(s, NULL, errno, false, true);
                *err = true;
                goto finish;
            }
        default:
            break;
        }

        if (!(fds[0].revents & POLLIN)) {
            n00b_fd_post_error(s, NULL, EBADF, false, true);
            *err = true;
            goto finish;
        }

        N00B_DBG_CALL(n00b_thread_suspend);
        r = read(s->fd, p, remaining);
        N00B_DBG_CALL(n00b_thread_resume);

        switch (r) {
        case -1:
            switch (errno) {
            case EAGAIN:
                goto wait;
            case EINTR:
                continue;
            default:
                n00b_fd_post_error(s, NULL, errno, false, true);
                *err = true;
                goto finish;
            }
        case 0:
            // For sockets, it is a close. Anything else, 'no data available'.
            if (s->socket) {
                n00b_fd_discovered_read_close(s);
                goto finish;
            }
            //  might be more data in the future. Whether we keep waiting is
            // up to the 'full' parameter.
            if (full) {
                goto wait;
            }
            else {
                goto finish;
            }
        default:
            remaining -= r;
            p += r;
            if (!remaining) {
                goto finish;
            }
            continue; // We don't poll until we get EAGAIN.
        }

wait:

        if (end) {
            n00b_write_now(&now);
            if (n00b_duration_gt(&now, end)) {
                goto finish;
            }
            n00b_duration_t *diff = n00b_duration_diff(&now, end);
            ms_timeout            = n00b_duration_to_ms(diff);
        }

        N00B_DBG_CALL(n00b_thread_suspend);
        r = poll(fds, 1, ms_timeout);
        N00B_DBG_CALL(n00b_thread_resume);
        n00b_fd_stream_nonblocking(s);

        continue;
    }

finish:
    if (p != (char *)&buf) {
        int l = p - buf;
        b     = n00b_buffer_from_bytes(buf, l);
        s->total_read += l;
        n00b_fd_post(s, s->read_subs, b);
        n00b_fd_worker_yield(s);
        return b;
    }
    *err = true;
    n00b_fd_worker_yield(s);
    return b;
}
