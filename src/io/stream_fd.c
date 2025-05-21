#define N00B_USE_INTERNAL_API
#include "n00b.h"

#if defined(N00B_DEBUG_DESCRIBE_FLAGS_ON_OPEN)
// This was useful; will keep it around.
void
n00b_describe_open_flags(int f)
{
    switch (f & O_ACCMODE) {
    case O_WRONLY:
        printf("O_WRONLY ");
        break;
    case O_RDONLY:
        printf("O_RDONLY ");
        break;
    default:
        printf("O_RDWR ");
        break;
    }

    f = f & ~O_ACCMODE;

    if (f & O_EXEC) {
        printf("O_EXEC ");
        f = f & ~O_EXEC;
    }

    if (f & O_NONBLOCK) {
        printf("O_NONBLOCK ");
        f = f & ~O_NONBLOCK;
    }

    if (f & O_APPEND) {
        printf("O_APPEND ");
        f = f & ~O_APPEND;
    }

    if (f & O_CREAT) {
        printf("O_CREAT ");
        f = f & ~O_CREAT;
    }

    if (f & O_TRUNC) {
        printf("O_TRUNC ");
        f = f & ~O_TRUNC;
    }

    if (f & O_EXCL) {
        printf("O_EXCL ");
        f = f & ~O_EXCL;
    }

    if (f & O_SHLOCK) {
        printf("O_SHLOCK ");
        f = f & ~O_SHLOCK;
    }

    if (f & O_EXLOCK) {
        printf("O_EXLOCK ");
        f = f & ~O_EXLOCK;
    }

    if (f & O_DIRECTORY) {
        printf("O_DIRECTORY ");
        f = f & ~O_DIRECTORY;
    }

    if (f & O_NOFOLLOW) {
        printf("O_NOFOLLOW ");
        f = f & ~O_NOFOLLOW;
    }

    if (f & O_SYMLINK) {
        printf("O_SYMLINK ");
        f = f & ~O_SYMLINK;
    }

    if (f & O_EVTONLY) {
        printf("O_EVTONLY ");
        f = f & ~O_EVTONLY;
    }

    if (f & O_CLOEXEC) {
        printf("O_CLOEXEC ");
        f = f & ~O_CLOEXEC;
    }

    if (f & O_NOFOLLOW_ANY) {
        printf("O_NOFOLLOW_ANY ");
        f = f & ~O_NOFOLLOW_ANY;
    }

    if (f & O_NOCTTY) {
        printf("O_NOCTTY ");
        f = f & ~O_NOCTTY;
    }

    if (f) {
        printf(" +%x", f);
    }

    printf("\n");
}
#else
#define n00b_describe_open_flags(x)
#endif

void
on_fd_close(n00b_fd_stream_t *s, n00b_stream_t *c)
{
    c->r = false;
    c->w = false;
    n00b_close(c);
}

// The low-level event scheduler calls this when there's a read; we
// need to call the dispatcher to pass it through any filtering and
// hand it out to readers.
void
fd_stream_on_read_event(n00b_fd_stream_t *s,
                        n00b_fd_sub_t    *sub,
                        void             *msg,
                        void             *thunk)
{
    bool           err;
    n00b_stream_t *stream = thunk;

    n00b_cache_read(stream, msg);
    n00b_io_dispatcher_process_read_queue(stream, &err);
}

// Not static; reused by stream_listener
void
n00b_fd_stream_on_first_subscriber(n00b_stream_t *c, n00b_fd_cookie_t *p)
{
    n00b_dlog_io("Registering fd %d for reads", p->stream->fd);
    p->sub = _n00b_fd_read_subscribe(p->stream,
                                     (void *)fd_stream_on_read_event,
                                     2,
                                     (int)(false),
                                     c);
    _n00b_fd_close_subscribe(p->stream, (void *)on_fd_close, 2, (int)false, c);
}

// Not static; reused by stream_listener
void
n00b_fd_stream_on_no_subscribers(n00b_stream_t *c, n00b_fd_cookie_t *p)
{
    if (p->sub) {
        n00b_fd_unsubscribe(p->stream, p->sub);
        p->sub = NULL;
    }
}

enum {
    FD_FILE,
    FD_CONNECT,
    FD_OTHER,
};

static int
fd_stream_init(n00b_stream_t *stream, n00b_list_t *l)
{
    stream->fd_backed   = true;
    n00b_fd_cookie_t *c = n00b_get_stream_cookie(stream);

    c->stream = n00b_list_pop(l);
    fcntl(c->stream->fd, F_GETFL, &c->stream->fd_flags);

    switch ((int64_t)n00b_list_pop(l)) {
    case FD_FILE:
        stream->name    = n00b_list_pop(l);
        c->stream->name = n00b_cformat("fd [|#|] (file)",
                                       (int64_t)c->stream->fd);
        break;
    case FD_CONNECT:
        c->addr         = n00b_list_pop(l);
        stream->name    = n00b_to_string(c->addr);
        c->stream->name = n00b_cformat("fd [|#|] (connect)",
                                       (int64_t)c->stream->fd);
        break;
    default:
        stream->name = n00b_fd_name(c->stream);

        if (c->stream->socket) {
            c->stream->name = n00b_cformat("fd [|#|] (socket)",
                                           (int64_t)c->stream->fd);
        }
        else {
            c->stream->name = n00b_cformat("fd [|#|] (pipe)",
                                           (int64_t)c->stream->fd);
            break;
        }
    }
    return c->stream->fd_flags & O_ACCMODE;
}

// Can't be static; reused by listener.
bool
n00b_fd_stream_close(n00b_stream_t *stream)
{
    n00b_fd_cookie_t *c = n00b_get_stream_cookie(stream);

    n00b_raw_fd_close(c->stream->fd);

    if (c->sub) {
        n00b_fd_unsubscribe(c->stream, c->sub);
    }

    return true;
}

bool
n00b_fd_stream_eof(n00b_stream_t *stream)
{
    n00b_fd_cookie_t *c = n00b_get_stream_cookie(stream);
    if (c->stream->read_closed && c->stream->write_closed) {
        return true;
    }
    if (!c->stream->socket) {
        int n = lseek(c->stream->fd, 0, SEEK_CUR);
        if (n == lseek(c->stream->fd, 0, SEEK_END)) {
            return true;
        }
        lseek(c->stream->fd, n, SEEK_SET);
    }
    else {
        struct pollfd fds[1] = {
            {
                .fd      = c->stream->fd,
                .events  = POLLIN | POLLOUT,
                .revents = 0,
            },
        };

        poll(fds, 1, 0);

        if (fds[0].events & POLLHUP) {
            return true;
        }
    }

    return false;
}

static void *
fd_stream_read(n00b_stream_t *stream, bool *err)
{
    n00b_fd_cookie_t *c = n00b_get_stream_cookie(stream);
    // Since all reads, including blocking reads, have the IO started
    // via the file descriptor polling loop, we know that, if this is
    // getting called, we're being asked to return more data, probably
    // due to some sort of filter, like line buffering, that is trying
    // to yield a full message.
    //
    // Here, we want to return anything we can without blocking. If
    // there is some data, we can just read it all in one shot.  But
    // we need to make sure the read can't block.
    //
    // Since this would have been dispatched from within the polling
    // loop, we are expecting exclusive access to this file
    // descriptor, so we can solve this easily with a poll().

    struct pollfd fds[1] = {
        {
            .fd      = c->stream->fd,
            .events  = POLLIN,
            .revents = 0,
        },
    };

    if (poll(fds, 1, 0) != 1) {
        *err = true;
        return NULL;
    }

    // Last parameter keeps it from trying to drain the fd and
    // potentially blocking.
    n00b_buf_t *data = n00b_fd_read(c->stream, -1, 0, false, err);

    return data;
}

static void
fd_stream_write(n00b_stream_t *stream, void *msg, bool block)
{
    n00b_fd_cookie_t *c = n00b_get_stream_cookie(stream);
    n00b_buf_t       *b;

    if (n00b_type_is_string(n00b_get_my_type(msg))) {
        b = n00b_string_to_buffer(msg);
    }
    else {
        b = msg;
    }

    if (!b) {
        return;
    }

    if (block) {
        n00b_fd_write(c->stream, b->data, b->byte_len);
    }
    else {
        n00b_fd_send(c->stream, b->data, b->byte_len);
    }
}

static bool
fd_stream_set_position(n00b_stream_t *stream, int pos, bool relative)
{
    n00b_fd_cookie_t *c = n00b_get_stream_cookie(stream);

    if (relative) {
        return n00b_fd_set_relative_position(c->stream, pos);
    }

    return n00b_fd_set_absolute_position(c->stream, pos);
}

static n00b_stream_impl fd_stream_impl = {
    .cookie_size         = sizeof(n00b_fd_cookie_t),
    .init_impl           = (void *)fd_stream_init,
    .read_impl           = (void *)fd_stream_read,
    .write_impl          = (void *)fd_stream_write,
    .spos_impl           = (void *)fd_stream_set_position,
    .gpos_impl           = (void *)n00b_fd_get_position,
    .close_impl          = (void *)n00b_fd_stream_close,
    .eof_impl            = (void *)n00b_fd_stream_eof,
    .read_subscribe_cb   = (void *)n00b_fd_stream_on_first_subscriber,
    .read_unsubscribe_cb = (void *)n00b_fd_stream_on_no_subscribers,
};

n00b_stream_t *
_n00b_new_fd_stream(n00b_fd_stream_t *fd, ...)
{
    n00b_list_t *args = n00b_list(n00b_type_ref());
    n00b_list_t *filters;

    n00b_build_filter_list(filters, fd);

    if (n00b_list_len(filters)) {
        void *item = n00b_list_get(filters, 0, NULL);
        if (n00b_type_is_net_addr(n00b_get_my_type(item))) {
            n00b_list_append(args, item); // Network address.
            n00b_list_append(args, (void *)(int64_t)FD_CONNECT);
            n00b_list_dequeue(filters);
        }
        else {
            abort();
        }
    }

    if (!n00b_list_len(args)) {
        n00b_list_append(args, (void *)(int64_t)FD_OTHER);
    }

    n00b_list_append(args, fd);

    return n00b_new(n00b_type_stream(), &fd_stream_impl, args, filters);
}

n00b_stream_t *
n00b_stream_fd_open(int fd)
{
    return n00b_new_fd_stream(n00b_fd_stream_from_fd(fd, NULL, NULL));
}

#define FERR(x)                \
    {                          \
        err = n00b_cstring(x); \
        if (error_ptr) {       \
            *error_ptr = err;  \
            return NULL;       \
        }                      \
        else {                 \
            N00B_RAISE(err);   \
        }                      \
    }

n00b_stream_t *
_n00b_stream_open_file(n00b_string_t *filename, ...)
{
    va_list args;
    va_start(args, filename);

    // o0744
    int permissions = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

    n00b_list_t    *filters                     = NULL;
    bool            read_only                   = false;
    bool            write_only                  = false;
    bool            allow_relative_paths        = true;
    bool            allow_backtracking          = true;
    bool            normalize_paths             = true;
    bool            allow_file_creation         = false;
    bool            error_if_exists             = false;
    bool            truncate_on_open            = false;
    bool            writes_always_append        = false;
    bool            shared_lock                 = false;
    bool            exclusive_lock              = false;
    bool            keep_open_on_exec           = false;
    bool            allow_tty_assumption        = false;
    bool            open_symlink_not_target     = false;
    bool            allow_symlinked_targets     = true;
    bool            follow_symlinks_in_path     = true;
    bool            target_must_be_regular_file = false;
    bool            target_must_be_directory    = false;
    bool            target_must_be_link         = false;
    bool            target_must_be_special_file = false;
    n00b_string_t **error_ptr                   = NULL;
    n00b_string_t  *err;

    n00b_karg_only_init(args);
    n00b_kw_ptr("filters", filters);
    n00b_kw_ptr("error_ptr", error_ptr);
    n00b_kw_int32("permissions", permissions);
    n00b_kw_bool("allow_relative_paths", allow_relative_paths);
    n00b_kw_bool("write_only", write_only);
    n00b_kw_bool("allow_backtracking", allow_backtracking);
    n00b_kw_bool("normalize_paths", normalize_paths);
    n00b_kw_bool("error_if_exists", error_if_exists);
    n00b_kw_bool("allow_file_creation", allow_file_creation);
    n00b_kw_bool("truncate_on_open", truncate_on_open);
    n00b_kw_bool("writes_always_append", writes_always_append);
    n00b_kw_bool("shared_lock", shared_lock);
    n00b_kw_bool("exclusive_lock", exclusive_lock);
    n00b_kw_bool("keep_open_on_exec", keep_open_on_exec);
    n00b_kw_bool("allow_tty_assumption", allow_tty_assumption);
    n00b_kw_bool("open_symlink_not_target", open_symlink_not_target);
    n00b_kw_bool("allow_symlink_targets", allow_symlinked_targets);
    n00b_kw_bool("follow_symlinks_in_path", follow_symlinks_in_path);
    n00b_kw_bool("target_must_be_regular_file", target_must_be_regular_file);
    n00b_kw_bool("target_must_be_directory", target_must_be_directory);
    n00b_kw_bool("target_must_be_link", target_must_be_link);
    n00b_kw_bool("target_must_be_special_file", target_must_be_special_file);

    int relative_fd = 0;
    int flags;
    int mode;

    if (!n00b_string_codepoint_len(filename)) {
        FERR("Must provide a filename.");
    }

    if (read_only ^ write_only) {
        if (read_only) {
            flags = O_RDONLY;
        }
        else {
            flags = O_WRONLY;
        }
    }
    else {
        flags = O_RDWR;
    }

    mode = flags;

    if (permissions < 0 || permissions > 07777) {
        FERR("Invalid file permissions.");
    }

    // Implies creation.
    if (error_if_exists) {
        flags |= O_EXCL;
        flags |= O_CREAT;
    }

    if (allow_file_creation) {
        flags |= O_CREAT;
    }

    if (writes_always_append) {
        flags |= O_APPEND;
    }

    if (truncate_on_open) {
        if (flags & O_EXCL) {
            FERR("Cannot truncate when requiring a new file.");
        }
        flags |= O_TRUNC;
    }

    if (mode == O_RDONLY) {
        if (flags & O_CREAT) {
            FERR("Cannot specify creation for read-only files.");
        }
        if (flags & O_APPEND) {
            FERR("Cannot append without write permissions");
        }
    }

    if (!keep_open_on_exec) {
        flags |= O_CLOEXEC;
    }

    if (!allow_tty_assumption) {
        flags |= O_NOCTTY;
    }

    // This ifdef is because on some linux distros, libevent is
    // complaining about O_NONBLOCK even on stderr where it shouldn't
    // be.
#if defined(__linux__)
    flags |= O_NONBLOCK;
#endif

    if (target_must_be_directory) {
        flags |= O_DIRECTORY;
    }

    int sl_flags = 0;

#if !defined(__linux__)
    if (shared_lock) {
        flags |= O_SHLOCK;
        sl_flags++;
    }
#endif
    if (exclusive_lock) {
        if (shared_lock) {
            FERR("Cannot select both locking styles");
        }
        sl_flags++;
    }

    if (sl_flags > 1) {
        FERR("Cannot select both locking styles");
    }

    sl_flags = 0;

#if !defined(__linux__)
    if (open_symlink_not_target) {
        flags |= O_SYMLINK;
        sl_flags++;
    }
#endif

    if (!allow_symlinked_targets) {
        flags |= O_NOFOLLOW;
        sl_flags++;
    }

#if !defined(__linux__)
    if (!follow_symlinks_in_path) {
        flags |= O_NOFOLLOW_ANY;
        sl_flags++;
    }
#endif

    if (sl_flags > 1) {
        FERR("Conflicting symbolic linking policies provided.");
    }

    bool             path_is_relative;
    n00b_codepoint_t cp = (n00b_codepoint_t)(int64_t)n00b_index_get(filename,
                                                                    0);

    switch (cp) {
    case '/':
        path_is_relative = false;
        break;
    case '~':
        if (normalize_paths) {
            path_is_relative = false;
        }
        else {
            path_is_relative = true;
        }
        break;
    default:
        path_is_relative = false;
    }

    if (!allow_relative_paths && path_is_relative) {
        FERR("Relative paths are disallowed.");
    }

    if (!allow_backtracking) {
        n00b_list_t *parts = n00b_string_split(filename, n00b_cached_slash());
        int          n     = n00b_list_len(parts);

        for (int i = 0; i < n; i++) {
            n00b_string_t *s = n00b_list_get(parts, i, NULL);
            if (!strcmp(s->data, "..")) {
                FERR("Path backtracking is disabled.");
            }
        }
    }

    n00b_describe_open_flags(flags);
    int fd = -1;

    if (relative_fd) {
        if (flags & O_CREAT) {
            fd = openat(relative_fd, filename->data, flags, permissions);
        }
        else {
            fd = openat(relative_fd, filename->data, flags);
        }
    }
    else {
        if (normalize_paths) {
            filename = n00b_resolve_path(filename);
        }

        if (flags & O_CREAT) {
            fd = open(filename->data, flags, permissions);
        }
        else {
            fd = open(filename->data, flags);
        }
    }

    if (fd == -1) {
        n00b_raise_errno();
    }

    n00b_fd_stream_t *fds = n00b_fd_stream_from_fd(fd, NULL, NULL);
    fds->name             = filename;

    if (target_must_be_regular_file && !n00b_fd_is_regular_file(fds)) {
        FERR("Not a regular file.");
    }

    if (target_must_be_link && !n00b_fd_is_link(fds)) {
        FERR("Not a symbolic link.");
    }

    if (target_must_be_special_file && !n00b_fd_is_other(fds)) {
        FERR("Not a special file.");
    }

    n00b_list_t *stream_args = n00b_list(n00b_type_ref());
    n00b_list_append(stream_args, filename);
    n00b_list_append(stream_args, (void *)(int64_t)FD_FILE);
    n00b_list_append(stream_args, fds);

    return n00b_new(n00b_type_stream(), &fd_stream_impl, stream_args, filters);
}

n00b_stream_t *
_n00b_stream_connect(n00b_net_addr_t *addr, ...)
{
    n00b_list_t *filters;

    int sock = socket(addr->family, SOCK_STREAM, 0);

    if (sock < 0) {
        n00b_raise_errno();
    }

    if (connect(sock, (void *)&addr->addr, n00b_get_sockaddr_len(addr))) {
        return NULL;
    }

    n00b_build_filter_list(filters, addr);

    n00b_list_t *args = n00b_list(n00b_type_ref());

    n00b_list_append(args, addr);
    n00b_list_append(args, (void *)(int64_t)FD_CONNECT);
    n00b_list_append(args, n00b_fd_stream_from_fd(sock, NULL, NULL));

    return n00b_new(n00b_type_stream(), &fd_stream_impl, args, filters);
}

void *
_n00b_read_file(n00b_string_t *path, ...)
{
    bool            buffer    = false;
    bool            lock      = false;
    n00b_string_t **error_ptr = NULL;

    n00b_karg_only_init(path);
    n00b_kw_bool("buffer", buffer);
    n00b_kw_bool("lock", lock);
    n00b_kw_ptr("error_ptr", error_ptr);

    n00b_stream_t *f = n00b_stream_open_file(path,
                                             "exclusive_lock",
                                             n00b_ka(lock),
                                             "read_only",
                                             n00b_ka(true),
                                             "target_must_be_regular_file",
                                             n00b_ka(true),
                                             "error_ptr",
                                             error_ptr);

    if (!f) {
        return NULL;
    }

    bool        err = false;
    n00b_buf_t *b   = n00b_stream_read(f, 0, NULL);

    n00b_close(f);

    if (err) {
        n00b_string_t *msg = n00b_cstring("Error when reading file.");

        if (error_ptr) {
            *error_ptr = msg;
            return NULL;
        }
        else {
            N00B_RAISE(msg);
        }
    }

    if (buffer) {
        return b;
    }

    return n00b_buf_to_string(b);
}
