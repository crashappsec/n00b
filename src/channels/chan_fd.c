#define N00B_USE_INTERNAL_API
#include "n00b.h"

static int
fdchan_init(n00b_fd_cookie_t *p, n00b_fd_stream_t *s)
{
    p->stream = s;
    return s->fd_flags & O_ACCMODE;
}

static void
on_fd_close(n00b_fd_stream_t *s, n00b_channel_t *c)
{
    c->r = false;
    c->w = false;
    n00b_cnotify_close(c, NULL);
}

static void
fdchan_on_read_event(n00b_fd_stream_t *s,
                     n00b_fd_sub_t    *sub,
                     n00b_buf_t       *buf,
                     void             *thunk)
{
    n00b_channel_t *channel = thunk;

    if (!channel->read_cache) {
        channel->read_cache = n00b_list(n00b_type_ref());
    }
    n00b_list_append(channel->read_cache, buf);
    n00b_io_dispatcher_process_read_queue(channel);
}

void
fdchan_on_first_subscriber(n00b_channel_t *c, n00b_fd_cookie_t *p)
{
    p->sub = _n00b_fd_read_subscribe(p->stream,
                                     fdchan_on_read_event,
                                     2,
                                     (int)(false),
                                     c);
    _n00b_fd_close_subscribe(p->stream, (void *)on_fd_close, 2, (int)false, c);
}

void
fdchan_on_no_subscribers(n00b_channel_t *c, n00b_fd_cookie_t *p)
{
    if (p->sub) {
        n00b_fd_unsubscribe(p->stream, p->sub);
        p->sub = NULL;
    }
}

bool
fdchan_close(n00b_fd_cookie_t *p)
{
    close(p->stream->fd);

    if (p->sub) {
        n00b_fd_unsubscribe(p->stream, p->sub);
    }

    return true;
}

static void *
fdchan_read(n00b_fd_cookie_t *p, bool block, bool *success, int ms_timeout)
{
    n00b_buf_t *data;

    // In the first case, we were called via the user.  In the second,
    // we will have been poll'd via the event loop, which is also the
    // current thread.
    //
    // We will have just stuck the value read into the cookie already,
    // and will not have contention.

    if (block) {
        data     = n00b_fd_read(p->stream, -1, ms_timeout, false);
        *success = (data != NULL);
    }
    else {
        data          = p->read_cache;
        p->read_cache = NULL;
    }
    return data;
}

static void
fdchan_write(n00b_fd_cookie_t *p, n00b_cmsg_t *msg, bool block)
{
    n00b_buf_t *b = msg->payload;

    if (block) {
        n00b_fd_write(p->stream, b->data, b->byte_len);
    }
    else {
        n00b_fd_send(p->stream, b->data, b->byte_len);
    }
}

static bool
fdchan_set_position(n00b_fd_cookie_t *p, int pos, bool relative)
{
    if (relative) {
        return n00b_fd_set_relative_position(p->stream, pos);
    }

    return n00b_fd_set_absolute_position(p->stream, pos);
}

static n00b_chan_impl n00b_fdchan_impl = {
    .cookie_size         = sizeof(n00b_fd_stream_t **),
    .init_impl           = (void *)fdchan_init,
    .read_impl           = (void *)fdchan_read,
    .write_impl          = (void *)fdchan_write,
    .spos_impl           = (void *)fdchan_set_position,
    .gpos_impl           = (void *)n00b_fd_get_position,
    .close_impl          = (void *)fdchan_close,
    .read_subscribe_cb   = (void *)fdchan_on_first_subscriber,
    .read_unsubscribe_cb = (void *)fdchan_on_no_subscribers,
};

n00b_channel_t *
_n00b_new_fd_channel(n00b_fd_stream_t *fd, ...)
{
    n00b_list_t    *filters = NULL;
    n00b_channel_t *result;

    int nargs;

    va_list args;
    va_start(args, fd);
    nargs = va_arg(args, int) - 1;

    if (nargs == 1) {
        void *item = va_arg(args, void *);
        if (n00b_type_is_list(n00b_get_my_type(item))) {
            filters = item;
        }
        else {
            filters = n00b_list(n00b_type_ref());
            n00b_list_append(filters, item);
        }
    }
    else {
        filters = n00b_list(n00b_type_ref());
        while (nargs--) {
            n00b_list_append(filters, va_arg(args, n00b_filter_spec_t *));
        }
    }
    va_end(args);

    result = n00b_channel_create(&n00b_fdchan_impl, fd, filters);

    return result;
}

n00b_channel_t *
_n00b_channel_open_file(n00b_string_t *filename, ...)
{
    va_list args;
    va_start(args, filename);

    // o0744
    int permissions = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

    n00b_list_t *filters                     = NULL;
    bool         read_only                   = false;
    bool         write_only                  = false;
    bool         allow_relative_paths        = true;
    bool         allow_backtracking          = true;
    bool         normalize_paths             = true;
    bool         allow_file_creation         = false;
    bool         error_if_exists             = false;
    bool         truncate_on_open            = false;
    bool         writes_always_append        = false;
    bool         shared_lock                 = false;
    bool         exclusive_lock              = false;
    bool         keep_open_on_exec           = false;
    bool         allow_tty_assumption        = false;
    bool         open_symlink_not_target     = false;
    bool         allow_symlinked_targets     = true;
    bool         follow_symlinks_in_path     = true;
    bool         target_must_be_regular_file = false;
    bool         target_must_be_directory    = false;
    bool         target_must_be_link         = false;
    bool         target_must_be_special_file = false;

    n00b_karg_only_init(args);
    n00b_kw_ptr("filters", filters);
    n00b_kw_int32("permissions", permissions);
    n00b_kw_bool("allow_relative_paths", allow_relative_paths);
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
        N00B_CRAISE("Must provide a filename.");
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
        N00B_CRAISE("Invalid file permissions.");
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
            N00B_CRAISE("Cannot truncate when requiring a new file.");
        }
        flags |= O_TRUNC;
    }

    if (mode == O_RDONLY) {
        if (flags & O_CREAT) {
            N00B_CRAISE("Cannot specify creation for read-only files.");
        }
        if (flags & O_APPEND) {
            N00B_CRAISE("Cannot append without write permissions");
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
            N00B_CRAISE("Cannot select both locking styles");
        }
        sl_flags++;
    }

    if (sl_flags > 1) {
        N00B_CRAISE("Cannot select both locking styles");
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
        N00B_CRAISE("Conflicting symbolic linking policies provided.");
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
        N00B_CRAISE("Relative paths are disallowed.");
    }

    if (!allow_backtracking) {
        n00b_list_t *parts = n00b_string_split(filename, n00b_cached_slash());
        int          n     = n00b_list_len(parts);

        for (int i = 0; i < n; i++) {
            n00b_string_t *s = n00b_list_get(parts, i, NULL);
            if (!strcmp(s->data, "..")) {
                N00B_CRAISE("Path backtracking is disabled.");
            }
        }
    }

    int fd;

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
        N00B_CRAISE("Not a regular file.");
    }

    if (target_must_be_link && !n00b_fd_is_link(fds)) {
        N00B_CRAISE("Not a symbolic link.");
    }

    if (target_must_be_special_file && !n00b_fd_is_other(fds)) {
        N00B_CRAISE("Not a special file.");
    }

    return n00b_new_fd_channel(fds, filters);
}

n00b_channel_t *
_n00b_channel_connect(n00b_net_addr_t *addr, ...)
{
    n00b_list_t *filters;
    va_list      args;

    va_start(args, filename);
    filters = va_arg(args, n00b_list_t *);
    va_end(args);

    int sock = socket(addr->family, SOCK_STREAM, 0);

    if (sock < 0) {
        n00b_raise_errno();
    }

    if (connect(sock, (void *)&addr->addr, n00b_get_sockaddr_len(addr))) {
        return NULL;
    }

    n00b_fd_stream_t *fd     = n00b_fd_stream_from_fd(sock, NULL, NULL);
    n00b_channel_t   *result = n00b_channel_create(&n00b_fdchan_impl,
                                                 fd,
                                                 filters);
    n00b_fd_cookie_t *cookie = n00b_get_channel_cookie(result);
    cookie->addr             = addr;

    return result;
}
