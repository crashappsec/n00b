#define N00B_USE_INTERNAL_API
#include "n00b.h"

// This is a LOW-LEVEL interface. The callbacks are expected to return
// quickly, so should not be user callbacks.

n00b_event_loop_t          *n00b_system_dispatcher = NULL;
static n00b_dict_t         *fd_cache               = NULL;
static pthread_once_t       io_inited              = PTHREAD_ONCE_INIT;
_Atomic(n00b_condition_t *) n00b_io_exit_request   = NULL;
bool                        n00b_io_exited         = false;

extern void n00b_setup_term_channels(void);

static void
setup_io(void)
{
    n00b_gc_register_root(&n00b_system_dispatcher, 1);
    n00b_gc_register_root(&n00b_io_exit_request, 1);
    n00b_gc_register_root(&fd_cache, 1);
    fd_cache               = n00b_dict(n00b_type_int(), n00b_type_ref());
    n00b_system_dispatcher = n00b_new_event_context(N00B_EV_POLL);
    n00b_setup_term_channels();
}

typedef struct {
    n00b_condition_test_fn fn;
    void                  *thunk;
    n00b_condition_t      *condition;
} cond_info_t;

static void
process_conditions(n00b_event_loop_t *ctx)
{
    n00b_list_t *old = atomic_read(&ctx->conditions);
    if (!n00b_list_len(old)) {
        return;
    }

    // We *should* be the only thread changing out the list.
    n00b_list_t *new_list = n00b_list(n00b_type_ref());

    while (!(CAS(&ctx->conditions, &old, new_list))) {
        // Nothing.
    }

    cond_info_t *info = n00b_list_pop(old);

    while (info) {
        if ((*info->fn)(info->thunk)) {
            n00b_condition_lock_acquire(info->condition);
            n00b_condition_notify_one(info->condition);
            n00b_lock_release(info->condition);
        }
        n00b_list_append(new_list, info);
    }
    info = n00b_list_pop(old);
}

bool
n00b_condition_poll(n00b_condition_test_fn fn, void *thunk, int to_ms)
{
    cond_info_t     *info = n00b_gc_alloc_mapped(cond_info_t, N00B_GC_SCAN_ALL);
    n00b_duration_t *to   = NULL;
    info->fn              = fn;
    info->thunk           = thunk;
    info->condition       = n00b_new(n00b_type_condition());

    if (to_ms > 0) {
        to = n00b_now();
        to->tv_sec += to_ms / 1000;
        to->tv_nsec += (to_ms % 1000) * 1000000;
    }

    n00b_condition_lock_acquire(info->condition);

    n00b_list_t *dl = atomic_read(&n00b_system_dispatcher->conditions);
    n00b_list_append(dl, info);

    if (to) {
        return n00b_condition_timed_wait(info->condition, to);
    }

    n00b_condition_wait(info->condition);
    return true;
}

void
n00b_fd_init_io(void)
{
    pthread_once(&io_inited, setup_io);
}

// We don't expect a lot of contention, so we just use simple spin
// locks.

static inline void
become_worker(n00b_fd_stream_t *s)
{
    n00b_thread_t *expected = NULL;
    n00b_thread_t *me       = n00b_thread_self();

    while (!CAS(&s->worker, &expected, me)) {
        // Don't break the spin lock.
        expected = NULL;
    }

    n00b_assert(atomic_read(&s->worker) == n00b_thread_self());
}

static inline void
worker_yield(n00b_fd_stream_t *s)
{
    atomic_store(&s->worker, NULL);
}

// Slight wrappers to std stuff.
bool
n00b_fd_set_absolute_position(n00b_fd_stream_t *s, int position)
{
    if (position >= 0) {
        return lseek(s->fd, position, SEEK_SET) != -1;
    }

    position++;

    return lseek(s->fd, position, SEEK_END) != -1;
}

bool
n00b_fd_set_relative_position(n00b_fd_stream_t *s, int position)
{
    return lseek(s->fd, position, SEEK_CUR) != -1;
}

int
n00b_fd_get_position(n00b_fd_stream_t *s)
{
    return lseek(s->fd, 0, SEEK_CUR);
}

int
n00b_fd_get_size(n00b_fd_stream_t *s)
{
    struct stat info;
    if (fstat(s->fd, &info)) {
        return -1;
    }

    return info.st_size;
}

n00b_string_t *
n00b_fd_name(n00b_fd_stream_t *s)
{
    if (s->name) {
        return s->name;
    }
#if defined(__APPLE__)
    char path[PATH_MAX];
    if (fcntl(s->fd, F_GETPATH, path) != -1) {
        return n00b_cstring(path);
    }
#endif

    if (s->tty) {
        return n00b_cstring(ttyname(s->fd));
    }

    char *tag;

    struct stat info;
    fstat(s->fd, &info);

    switch (info.st_mode & S_IFMT) {
    case S_IFREG:
        tag = "file";
        break;
    case S_IFDIR:
        tag = "dir";
        break;
    case S_IFSOCK:
        tag = "socket";
        break;
    case S_IFCHR:
    case S_IFBLK:
        tag = "device";
        break;
    case S_IFIFO:
        tag = "pipe";
        break;
    case S_IFLNK:
        tag = "link";
        break;
    default:
        tag = "fd";
        break;
    }
    return n00b_cstring(tag);
    // TODO: Linux support. readlink + proc
}

// When we add an entry for a file descriptor, we want to avoid adding
// it multiple times to the same polling context, but allow different
// contexts to keep the same FD. We could add a cache per poll
// context, but this is easier in case I see a reason to disallow the
// same fd across poll sets.
//
// The only challenge this approach gives us, is we can't quite use
// the FD as the key. We instead, want the hash key to be a fd /
// dispatcher pair.
//
// To avoid a custom hash, we need to get the key to be unique with
// high likelyhood, in 64 bits. In the same heap, we wouldn't get
// collisions from the bottom half of the pointer, but if we allow
// multiple arenas it's a possibility.
//
// One easy think would be to xor the FD into the unused bits of the
// pointer, but that only gets us 3 bits, before we risk collisions.
//
// BUT, we don't expect to have significant collisions in the high
// bits of the heap. And the object might move, so we'd have to be
// able to keep track of that (which we actually do, but accessing it
// is a bit of a pain).
//
// To make a long story short, we sidestep the issue by
// creating a random heap key for the dispatcher, and then XORing
// the fd into it.

static inline int64_t
fd_hash_key(n00b_event_loop_t *loop, int fd)
{
    if (!loop->heap_key) {
        loop->heap_key = n00b_rand64();
    }

    return loop->heap_key ^ (int64_t)fd;
}

static inline n00b_fd_stream_t *
fd_cache_lookup(int fd, n00b_event_loop_t *loop)
{
    int64_t           key    = fd_hash_key(loop, fd);
    n00b_fd_stream_t *result = hatrack_dict_get(fd_cache, (void *)key, NULL);

    if (result && result->fd == N00B_FD_CLOSED) {
        return NULL;
    }

    return result;
}

static inline n00b_fd_stream_t *
fd_cache_add(n00b_fd_stream_t *s)
{
    int64_t           key = fd_hash_key(s->evloop, s->fd);
    n00b_fd_stream_t *found_entry;

    while (true) {
        found_entry = hatrack_dict_get(fd_cache, (void *)key, NULL);

        // There's something there that appears to be open.
        if (found_entry && found_entry->fd >= 0) {
            return found_entry;
        }

        // Either there's nothing there, or the found entry is closed, and
        // we want to replace it.

        if (hatrack_dict_cas(fd_cache, (void *)key, s, found_entry, true)) {
            return s;
        }

        // If we didn't win the CAS, the hatrack API doesn't give us
        // the new value, so we need to re-load, so we loop.
    }
}

static inline void
n00b_fd_stream_nonblocking(n00b_fd_stream_t *s)
{
    fcntl(s->fd, F_SETFL, fcntl(s->fd, F_GETFL) | O_NONBLOCK);
}

static inline void
n00b_fd_stream_blocking(n00b_fd_stream_t *s)
{
    fcntl(s->fd, F_SETFL, fcntl(s->fd, F_GETFL) & ~O_NONBLOCK);
}

static inline void
apply_preferred_fdopts(int fd, int lflags)
{
    fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | FD_CLOEXEC);
    fcntl(fd, F_SETFL, lflags | O_NONBLOCK);
}

static inline void
apply_preferred_sockopts(int fd)
{
    const int true_val = 1;
    const int linger   = N00B_SOCKET_LINGER_SEC;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &true_val, sizeof(int));
    setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &true_val, sizeof(int));
    setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &true_val, sizeof(int));
#if defined(__APPLE__) || defined(__FreeBSD__)
    setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &true_val, sizeof(int));
    setsockopt(fd, SOL_SOCKET, SO_LINGER_SEC, &linger, sizeof(int));
#elif defined(__linux__)
    struct linger linger_opt = {
        .l_onoff  = 1,
        .l_linger = N00B_SOCKET_LINGER_SEC};
    setsockopt(fd, SOL_SOCKET, SO_LINGER, &linger_opt, sizeof(struct linger));
#endif
}

n00b_fd_stream_t *
n00b_fd_stream_from_fd(int                fd,
                       n00b_event_loop_t *dispatcher,
                       n00b_ev_ready_cb   notifier)
{
    while (!dispatcher) {
        dispatcher = n00b_system_dispatcher;
    }
    n00b_fd_stream_t *result = fd_cache_lookup(fd, dispatcher);

    if (result) {
        return result;
    }
    struct stat info;
    if (fstat(fd, &info)) {
        n00b_raise_errno();
        return NULL; // Invalid FD
    }

    result          = n00b_gc_alloc_mapped(n00b_fd_stream_t,
                                  N00B_GC_SCAN_ALL);
    result->fd      = fd;
    result->fd_mode = info.st_mode;

    if (dispatcher) {
        result->evloop = dispatcher;
    }
    else {
        result->evloop = n00b_system_dispatcher;
    }

    result->socket      = S_ISSOCK(info.st_mode);
    result->newly_added = true;

    result->fd_flags = fcntl(result->fd, F_GETFL);

    int mode = result->fd_flags & O_ACCMODE;

    switch (mode) {
    case O_WRONLY:
        result->read_closed = true;
        break;
    case O_RDONLY:
        result->write_closed = true;
        break;
    default:
        break;
    }

    if (isatty(fd)) {
        result->tty = true;
    }

    if (!result->read_closed) {
        result->read_subs = n00b_list(n00b_type_ref());
    }
    if (!result->write_closed) {
        result->queued_subs = n00b_list(n00b_type_ref());
        result->sent_subs   = n00b_list(n00b_type_ref());
        result->write_queue = n00b_list(n00b_type_ref());
    }

    result->err_subs   = n00b_list(n00b_type_ref());
    result->close_subs = n00b_list(n00b_type_ref());

    apply_preferred_fdopts(result->fd, result->fd_flags);

    if (result->socket) {
        apply_preferred_sockopts(result->fd);
    }

    if (notifier) {
        result->notify           = notifier;
        result->no_dispatcher_rw = true;
    }

    n00b_fd_stream_t *check = fd_cache_add(result);

    if (check == result) {
        n00b_list_append(dispatcher->pending, result);
    }
    else {
        if (notifier && check->notify != notifier) {
            N00B_CRAISE("Multiple incompatible stream creations for an fd");
        }
    }

    return check;
}

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

    become_worker(s);

    if (!s->r_added) {
        // TODO: much better to use the spin lock here.
        while (!s->needs_r) {
            s->needs_r = true;
        }
        n00b_list_append(s->evloop->pending, s);
    }

    worker_yield(s);

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

static void
fd_post(n00b_fd_stream_t *s, n00b_list_t *subs, void *msg)
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

static void
fd_post_close(n00b_fd_stream_t *s)
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

static inline void
fd_discovered_read_close(n00b_fd_stream_t *s)
{
    s->read_closed = true;
    if (s->write_closed) {
        fd_post_close(s);
    }
}

static inline bool
fd_discovered_write_close(n00b_fd_stream_t *s)
{
    s->write_closed = true;
    if (s->read_closed) {
        fd_post_close(s);
        return true;
    }
    return false;
}

static void
fd_post_error(n00b_fd_stream_t *s,
              n00b_wq_item_t   *wq,
              int               err_code,
              bool              write,
              bool              fatal)
{
    if (fatal) {
        fd_post_close(s);
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
                fd_post(s, s->sent_subs, b);
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
                fd_post_close(s);
            }
            return;
        case EFBIG:
        case EINVAL:
        case ENETDOWN:
        case ENETUNREACH:
            fd_post_error(s, qitem, errno, true, false);
            return;
        default:
            fd_post_error(s, qitem, errno, true, true);
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
// non-blocking
//
// Returns true to 'take it off the poll list'
// Reads until drained.
// We keep a linked list of PIPE_BUF length statements
static bool
handle_one_read(n00b_fd_stream_t *s)
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
            fd_post_error(s, NULL, e, false, true);
            switch (errno) {
            case EBADF:
            case EINVAL:
                fd_discovered_read_close(s);
                return true;
            }
            return false;
        }

        n00b_net_addr_t  *saddr = n00b_new(n00b_type_net_addr(),
                                          n00b_kw("sockaddr", &addr));
        n00b_fd_stream_t *fd    = n00b_fd_stream_from_fd(sock, NULL, NULL);
        n00b_channel_t   *chan  = n00b_new_fd_channel(fd, saddr);

        n00b_fd_stream_nonblocking(fd);
        fd_post(s, s->read_subs, chan);
        return false;
    }

    while (true) {
        // If the fd's been set back to blocking, we'd like to undo that;
        // ideally we have exclusive access here.
        n00b_fd_stream_nonblocking(s);
        int val = read(s->fd, tmpbuf, PIPE_BUF);
        if (val == 0) {
            // If it's a socket, reading EOF tells us it's closed.
            // For a regular file, it does NOT.
            if (s->socket) {
                fd_discovered_read_close(s);
                return true;
            }
            return false;
        }
        if (val < 0) {
            switch (errno) {
            case EINTR:
                continue;
            case EAGAIN:
                if (first) {
                    msg = assemble_buffer(first, total);
                    fd_post(s, s->read_subs, msg);
                    first = last = NULL;
                }
                if (s->r_added && !n00b_list_len(s->read_subs)) {
                    return true;
                }
                return false;
            default:
                fd_post_error(s, NULL, errno, false, true);
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

static inline bool
handle_one_write(n00b_fd_stream_t *s)
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

    fd_post(s, s->queued_subs, n00b_buffer_from_bytes(bytes, len));

    if (CAS(&s->worker, &expected, me)) {
        // Double check.
        if (atomic_read(&s->worker) == me) {
            process_write_queue(s);
            worker_yield(s);
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

    fd_post(s, s->queued_subs, n00b_buffer_from_bytes(bytes, len));

    become_worker(s);
    n00b_fd_stream_blocking(s);

    qitem = n00b_list_get(s->write_queue, 0, NULL);

    while (qitem) {
        n00b_gts_suspend();
        int val = write(s->fd, qitem->cur, qitem->remaining);
        n00b_gts_resume();

        if (val > 0) {
            qitem->cur += val;
            qitem->remaining -= val;
            s->total_written += val;

            if (!qitem->remaining) {
                n00b_list_dequeue(s->write_queue);
                int         l = qitem->cur - qitem->start;
                n00b_buf_t *b = n00b_buffer_from_bytes(qitem->start, l);

                fd_post(s, s->sent_subs, b);
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
            fd_post_error(s, qitem, errno, true, false);
            worker_yield(s);
            return false;
        default:
            fd_post_error(s, qitem, errno, true, true);
            worker_yield(s);
            return false;
        }
    }

    n00b_fd_stream_nonblocking(s);
    worker_yield(s);

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
        worker_yield(s);
        return NULL;
    }

    if (!(fds[0].revents & POLLIN)) {
        fd_post_error(s, NULL, errno, false, true);
        *err = true;
        return NULL;
    }

    while (true) {
        n00b_fd_stream_nonblocking(s);

        int val = read(s->fd, tmpbuf, PIPE_BUF);
        if (val == 0) {
            // If it's a socket, reading EOF tells us it's closed.
            // For a regular file, it does NOT.
            if (s->socket) {
                fd_discovered_read_close(s);
            }
            goto finish;
        }

        if (val < 0) {
            switch (errno) {
            case EINTR:
                continue;
            default:
                fd_post_error(s, NULL, errno, false, true);
                // fallthrough
            case EAGAIN:
finish:
                if (first) {
                    msg = assemble_buffer(first, total);
                    fd_post(s, s->read_subs, msg);
                    first = last = NULL;
                }

                worker_yield(s);
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

    become_worker(s);

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
                fd_post_error(s, NULL, errno, false, true);
                *err = true;
                goto finish;
            }
        default:
            break;
        }

        if (!(fds[0].revents & POLLIN)) {
            fd_post_error(s, NULL, EBADF, false, true);
            *err = true;
            goto finish;
        }

        n00b_gts_suspend();
        r = read(s->fd, p, remaining);
        n00b_gts_resume();

        switch (r) {
        case -1:
            switch (errno) {
            case EAGAIN:
                goto wait;
            case EINTR:
                continue;
            default:
                fd_post_error(s, NULL, errno, false, true);
                *err = true;
                goto finish;
            }
        case 0:
            // For sockets, it is a close. Anything else, 'no data available'.
            if (s->socket) {
                fd_discovered_read_close(s);
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

        n00b_gts_suspend();
        r = poll(fds, 1, ms_timeout);
        n00b_gts_resume();
        n00b_fd_stream_nonblocking(s);

        continue;
    }

finish:
    if (p != (char *)&buf) {
        int l = p - buf;
        b     = n00b_buffer_from_bytes(buf, l);
        s->total_read += l;
        fd_post(s, s->read_subs, b);
        worker_yield(s);
        return b;
    }
    *err = true;
    return b;
}

static inline void
check_timers(n00b_event_loop_t *loop,
             n00b_duration_t   *now)
{
    if (!loop->timers) {
        return;
    }
    int n = n00b_list_len(loop->timers);

    while (n--) {
        n00b_timer_t *t = n00b_private_list_get(loop->timers, n, NULL);

        if (!t) {
            // Could have been removed in a race condition.
            continue;
        }

        if (n00b_duration_lt(t->stop_time, now)) {
            n00b_list_remove_item(loop->timers, t);
            (*t->action)(t, now, t->thunk);
        }
    }
}

static inline void
add_to_pollset(n00b_pevent_loop_t *ploop, n00b_fd_stream_t *s)
{
    // We actually keep two entries for each FD we are polling at
    // once, one we can send directly to poll(), and a pointer to the
    // n00b_fd_stream_t * object.
    //
    // When we run out of empty slots, we need to grow and copy both
    // lists.
    //
    // We will never shrink these allocations.
    int64_t slot;

    if (ploop->empty_slots && n00b_list_len(ploop->empty_slots)) {
        slot = (int64_t)n00b_private_list_pop(ploop->empty_slots);
    }
    else {
        if (ploop->pollset_alloc == ploop->pollset_last) {
            int os               = ploop->pollset_alloc;
            int ns               = os << 1;
            ploop->pollset_alloc = ns;
            struct pollfd *new   = n00b_gc_array_value_alloc(struct pollfd, ns);
            memcpy(new, ploop->pollset, sizeof(struct pollfd) * os);
            ploop->pollset = new;

            n00b_fd_stream_t **fdlist = n00b_gc_array_alloc(void *, ns);
            memcpy(fdlist, ploop->monitored_fds, sizeof(void *) * os);
            ploop->monitored_fds = fdlist;
        }
        slot = ploop->pollset_last++;
    }

    s->internal_ix                       = slot;
    ploop->pollset[s->internal_ix].fd    = s->fd;
    ploop->monitored_fds[s->internal_ix] = s;
}

static inline void
process_pending_changes(n00b_event_loop_t *loop, n00b_pevent_loop_t *ploop)
{
    n00b_fd_stream_t *s = n00b_private_list_pop(loop->pending);

    while (s) {
        if (s->newly_added) {
            add_to_pollset(ploop, s);
            s->newly_added = false;
        }

        struct pollfd *entry = &ploop->pollset[s->internal_ix];

        if (s->needs_r) {
            s->r_added = true;

            if (s->fd != 1 && s->fd != 2 && !(entry->events & POLLIN)) {
                entry->events |= POLLIN;
                ploop->ops_in_pollset++;
            }
            s->needs_r = false;
        }

        if (!s->w_added && !s->write_ready && n00b_list_len(s->write_queue)) {
            s->w_added = true;
            entry->events |= POLLOUT;
            ploop->ops_in_pollset++;
        }
        s = n00b_private_list_pop(loop->pending);
    }
}

static inline void
process_pset(n00b_event_loop_t *loop, n00b_pevent_loop_t *ploop)
{
    int                n         = ploop->pollset_last;
    struct pollfd     *slot_list = ploop->pollset;
    n00b_thread_t     *self      = n00b_thread_self();
    n00b_thread_t     *worker;
    n00b_fd_stream_t **fd_ptrs = ploop->monitored_fds;
    n00b_fd_stream_t  *s;

    for (int i = 0; i < n; i++) {
        s                   = fd_ptrs[i];
        worker              = NULL;
        struct pollfd *slot = slot_list + i;

        if (!s || atomic_read(&s->evloop->owner) != self) {
            continue;
        }
        if (!CAS(&s->worker, &worker, self)) {
            // Someone's either adding to it or doing their own r/w
            // so leave it alone until the next polling cycle.
            continue;
        }

        if (slot->revents & POLLIN) {
            if (s->no_dispatcher_rw) {
                s->read_ready = true;
                if (s->notify) {
                    (*s->notify)(s, true);
                }
                slot->events &= ~POLLIN;
                s->r_added = false;
                ploop->ops_in_pollset--;
            }
            else {
                if (s->fd == -1) {
                    s->fd = slot->fd;
                }
                if (handle_one_read(s) || s->plain_file) {
                    slot->events &= ~POLLIN;
                    s->r_added = false;
                    ploop->ops_in_pollset--;

                    if (s->plain_file) {
                        n00b_list_append(ploop->eof_list, s);
                    }
                }
            }
        }
        if (slot->revents & POLLOUT) {
            if (s->no_dispatcher_rw) {
                s->write_ready = true;
                if (s->notify) {
                    (*s->notify)(s, false);
                }
                slot->events &= ~POLLOUT;
                s->w_added = false;
                ploop->ops_in_pollset--;
            }
            else {
                if (handle_one_write(s)) {
                    slot->events &= ~POLLOUT;
                    s->w_added = false;
                    ploop->ops_in_pollset--;
                }
            }
        }
        if (slot->revents & POLLHUP) {
            // Might still have reading to do; but consider the fd
            // closed for writes in any circumstances.
            //
            // And if nothing is subscribed for reads, go ahead and
            // close the read side as well.

            if (!fd_discovered_write_close(s)) {
                if (!(slot->events & POLLIN)) {
                    fd_discovered_read_close(s);
                }
            }
        }

        // Take back closed slots.
        if (s->fd == N00B_FD_CLOSED) {
            *fd_ptrs        = NULL;
            // n00b_list_append(ploop->empty_slots, (void *)(int64_t)i);
            s->read_closed  = true;
            s->write_closed = true;
        }

        // Keep our pollcount up to date.
        if (s->read_closed && s->r_added) {
            ploop->ops_in_pollset--;
            s->r_added = false;
        }
        if (s->write_closed && s->w_added) {
            ploop->ops_in_pollset--;
            s->w_added = false;
        }

        atomic_store(&s->worker, NULL);
    }
}

static inline void
check_eof_list(n00b_event_loop_t *loop, n00b_pevent_loop_t *ploop)
{
    int n = n00b_list_len(ploop->eof_list);

    while (n--) {
        n00b_fd_stream_t *f = n00b_list_get(ploop->eof_list, n, NULL);
        if (n00b_fd_get_position(f) != n00b_fd_get_size(f)) {
            f->needs_r = true;
            n00b_list_remove(ploop->eof_list, n);
            n00b_list_append(loop->pending, f);
        }
    }
}

static bool
n00b_fd_run_poll_dispatcher_once(n00b_event_loop_t *loop,
                                 n00b_duration_t   *now)
{
    // We only register FDs if we need to; when we read / write, we do
    // so until we get EAGAIN. And if some other thread registers to do the
    // IO, then we don't re-add until we're told things are drained.
    n00b_pevent_loop_t *ploop = &loop->algo.poll;
    int                 wait  = N00B_POLL_DEFAULT_MS;

    check_timers(loop, now);
    check_eof_list(loop, ploop);
    process_conditions(loop);

    process_pending_changes(loop, ploop);

    if (!ploop->ops_in_pollset) {
        // If there are no fds registered, wait the whole polling
        // interval, then poll w/o blocking.

        // 100000 ns in a ms
        n00b_nanosleep(0, N00B_POLL_DEFAULT_MS * 1000000);
        process_pending_changes(loop, ploop);
        wait = 0;
    }

    n00b_gts_suspend();
    int val = poll(ploop->pollset, ploop->pollset_last, wait);
    n00b_gts_resume();

    if (val < 0) {
        return false;
    }

    process_pset(loop, ploop);

    return true;
}

static inline void
loop_exit_check(n00b_event_loop_t *loop, n00b_duration_t *now)
{
    if (atomic_read(&n00b_io_exit_request)) {
        loop->exit_loop = true;
        return;
    }

    if (!loop->stop_time) {
        return;
    }
    if (n00b_duration_lt(loop->stop_time, now)) {
        loop->exit_loop = true;
    }
}

bool
n00b_fd_run_evloop_once(n00b_event_loop_t *loop)
{
    n00b_duration_t now;

    n00b_write_now(&now);
    return n00b_fd_run_poll_dispatcher_once(loop, &now);
}

bool
n00b_fd_run_evloop(n00b_event_loop_t *loop, n00b_duration_t *howlong)
{
    n00b_duration_t now;
    n00b_thread_t  *t        = n00b_thread_self();
    n00b_thread_t  *expected = NULL;

    if (howlong) {
        n00b_write_now(&now);
        loop->stop_time = n00b_duration_add(&now, howlong);
    }
    else {
        loop->stop_time = NULL;
    }

    if (!CAS(&loop->owner, &expected, t)) {
        return false;
    }

    // Always run at least once.
    do {
        n00b_write_now(&now);
        n00b_fd_run_poll_dispatcher_once(loop, &now);
        loop_exit_check(loop, &now);
    } while (!loop->exit_loop);

    n00b_condition_t *cond = atomic_read(&n00b_io_exit_request);
    if (cond) {
        n00b_condition_lock_acquire(cond);
        n00b_condition_notify_all(cond);
        n00b_condition_lock_release(cond);
    }

    loop->exit_loop = false;
    atomic_store(&loop->owner, NULL);
    return true;
}

static void
new_poll_event_context(n00b_event_loop_t *ctx)
{
    n00b_pevent_loop_t *ploop = &ctx->algo.poll;
    ploop->pollset            = n00b_gc_array_value_alloc(struct pollfd,
                                               N00B_DEFAULT_POLLSET_SLOTS);
    ploop->monitored_fds      = n00b_gc_array_alloc(void *,
                                               N00B_DEFAULT_POLLSET_SLOTS);
    ploop->pollset_alloc      = N00B_DEFAULT_POLLSET_SLOTS;
    ploop->poll_interval      = N00B_POLL_DEFAULT_MS;
    ploop->empty_slots        = n00b_list(n00b_type_int());
    ploop->eof_list           = n00b_list(n00b_type_ref());
}

static void (*const event_impls[])(n00b_event_loop_t *) = {
    new_poll_event_context,
};

n00b_event_loop_t *
n00b_new_event_context(n00b_event_impl_kind kind)
{
    n00b_event_loop_t *ctx = n00b_gc_alloc_mapped(n00b_event_loop_t,
                                                  N00B_GC_SCAN_ALL);
    ctx->kind              = kind;
    ctx->timers            = n00b_list(n00b_type_ref());
    ctx->pending           = n00b_list(n00b_type_ref());
    atomic_store(&ctx->conditions, n00b_list(n00b_type_ref()));

    (*event_impls[kind])(ctx);

    return ctx;
}

n00b_timer_t *
_n00b_add_timer(n00b_duration_t *time,
                n00b_timer_cb    action,
                ...)

{
    va_list            args;
    void              *thunk = NULL;
    n00b_event_loop_t *loop  = NULL;

    va_start(args, action);
    int nargs = va_arg(args, int);

    if (nargs) {
        thunk = va_arg(args, void *);

        if (nargs != 1) {
            loop = va_arg(args, void *);
        }
    }

    n00b_timer_t *result = n00b_gc_alloc_mapped(n00b_timer_t,
                                                N00B_GC_SCAN_ALL);

    if (!loop) {
        loop = n00b_system_dispatcher;
    }

    result->thunk     = thunk;
    result->stop_time = n00b_duration_add(n00b_now(), time);
    result->action    = action;
    result->loop      = loop;

    n00b_list_append(loop->timers, result);

    return result;
}

void
n00b_remove_timer(n00b_timer_t *timer)
{
    n00b_list_remove_item(timer->loop->timers, timer);
}

void
n00b_dump_read_subscriptions(n00b_event_loop_t *hll)
{
    if (!hll) {
        hll = n00b_system_dispatcher;
    }

    n00b_pevent_loop_t *l = &hll->algo.poll;

    for (int i = 0; i < l->pollset_last; i++) {
        n00b_fd_stream_t *s = l->monitored_fds[i];
        if (!s) {
            continue;
        }

        int nr = 0;

        if (s->read_subs) {
            nr = n00b_list_len(s->read_subs);
        }

        n00b_string_t *cstr;
        cstr = n00b_cformat("fd [|#|] has [|#|] read subs\n",
                            (int64_t)s->fd,
                            (int64_t)nr,
                            0);
        printf("%s", cstr->data);

        for (int j = 0; j < nr; j++) {
            n00b_fd_sub_t *sub = n00b_list_get(s->read_subs, j, NULL);
            cstr               = n00b_cformat("Subscriber param: [|#|]\n", sub->thunk);
            printf("%s", cstr->data);
        }
    }
}
