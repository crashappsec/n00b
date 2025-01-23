#define N00B_USE_INTERNAL_API
#include "n00b.h"

static int64_t
n00b_extract_fd(void *obj)
{
    if (!n00b_is_object_reference(obj)) {
        return 0;
    }

    n00b_type_t *t = n00b_get_my_type(obj);

    if (!n00b_type_is_file(t) && !n00b_type_is_stream(t)) {
        return 0;
    }

    n00b_stream_t *stream = obj;

    if (stream->etype != n00b_io_ev_file) {
        return 0;
    }

    n00b_ev2_cookie_t *cookie = stream->cookie;
    return cookie->id;
}

static inline n00b_io_permission_t
mode_to_perms(n00b_file_mode_t mode)
{
    switch (mode) {
    case n00b_fm_rw:
        return n00b_io_perm_rw;
    case n00b_fm_write_only:
        return n00b_io_perm_w;
    case n00b_fm_read_only:
        return n00b_io_perm_r;
    default:
        return n00b_io_perm_none;
    }
}

static void
add_file_info(n00b_stream_t *stream, n00b_io_permission_t perms)
{
    n00b_ev2_cookie_t  *cookie    = stream->cookie;
    n00b_file_data_t   *file_info = n00b_gc_alloc_mapped(n00b_file_data_t,
                                                       N00B_GC_SCAN_ALL);
    n00b_stream_base_t *base      = n00b_get_ev_base(&n00b_fileio_impl);
    cookie->aux                   = file_info;

    fstat(cookie->id, &file_info->fd_info);

    int sm = file_info->fd_info.st_mode;

    if (S_ISREG(sm)) {
        file_info->kind = n00b_fk_regular;
    }
    else {
        if (S_ISDIR(sm)) {
            file_info->kind = n00b_fk_directory;
        }
        else {
            if (S_ISLNK(sm)) {
                file_info->kind = n00b_fk_link;
            }
            else {
                file_info->kind = n00b_fk_other;
            }
        }
    }

    if (isatty(cookie->id)) {
        stream->is_tty = true;
    }

    // None of these get enqueued until a subscription is set up.
    if (perms & n00b_io_perm_r) {
        cookie->read_event = event_new(base->event_ctx,
                                       cookie->id,
                                       EV_READ | EV_PERSIST,
                                       n00b_ev2_r,
                                       stream);
    }

    if (perms & n00b_io_perm_w) {
        cookie->write_event = event_new(base->event_ctx,
                                        cookie->id,
                                        EV_WRITE | EV_ET,
                                        n00b_ev2_w,
                                        stream);
    }
}

void
n00b_new_file_init(n00b_stream_t *stream, va_list args)
{
    // Fixed arguments.n
    n00b_str_t      *filename                    = va_arg(args, n00b_str_t *);
    n00b_file_mode_t mode                        = va_arg(args,
                                   n00b_file_mode_t);
    // o0744
    int              permissions                 = S_IRWXU | S_IRGRP | S_IROTH;
    n00b_stream_t   *base_for_relative_paths     = NULL;
    bool             allow_relative_paths        = true;
    bool             allow_backtracking          = true;
    bool             normalize_paths             = true;
    bool             allow_file_creation         = false;
    bool             error_if_exists             = false;
    bool             truncate_on_open            = false;
    bool             writes_always_append        = false;
    bool             shared_lock                 = false;
    bool             exclusive_lock              = false;
    bool             keep_open_on_exec           = false;
    bool             allow_tty_assumption        = false;
    bool             enable_blocking_io          = false;
    bool             open_symlink_not_target     = false;
    bool             allow_symlinked_targets     = true;
    bool             follow_symlinks_in_path     = true;
    bool             target_must_be_regular_file = false;
    bool             target_must_be_directory    = false;
    bool             target_must_be_link         = false;
    bool             target_must_be_special_file = false;

    n00b_karg_va_init(args);
    n00b_kw_ptr("base_for_relative_paths", base_for_relative_paths);
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
    n00b_kw_bool("enable_blocking_io", enable_blocking_io);
    n00b_kw_bool("open_symlink_not_target", open_symlink_not_target);
    n00b_kw_bool("allow_symlink_targets", allow_symlinked_targets);
    n00b_kw_bool("follow_symlinks_in_path", follow_symlinks_in_path);
    n00b_kw_bool("target_must_be_regular_file", target_must_be_regular_file);
    n00b_kw_bool("target_must_be_directory", target_must_be_directory);
    n00b_kw_bool("target_must_be_link", target_must_be_link);
    n00b_kw_bool("target_must_be_special_file", target_must_be_special_file);

    int relative_fd = 0;

    if (!n00b_str_codepoint_len(filename)) {
        N00B_CRAISE("Must provide a filename.");
    }
    else {
        filename = n00b_to_utf8(filename);
    }

    switch (mode) {
    case n00b_fm_read_only:
    case n00b_fm_write_only:
    case n00b_fm_rw:
        break;
    default:
        N00B_CRAISE("Invalid mode for opening a file.");
    }

    if (permissions < 0 || permissions > 07777) {
        N00B_CRAISE("Invalid file permissions.");
    }

    int flags = (int)mode;

    if (base_for_relative_paths) {
        relative_fd = n00b_extract_fd(base_for_relative_paths);
        if (!relative_fd) {
            N00B_CRAISE("Must provide a file or directory object.");
        }
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

    if (mode == n00b_fm_read_only) {
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

    if (!enable_blocking_io) {
        flags |= O_NONBLOCK;
    }

    if (target_must_be_directory) {
        flags |= O_DIRECTORY;
    }

    int sl_flags = 0;

    if (shared_lock) {
        flags |= O_SHLOCK;
        sl_flags++;
    }

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

    if (open_symlink_not_target) {
        flags |= O_SYMLINK;
        sl_flags++;
    }
    if (!allow_symlinked_targets) {
        flags |= O_NOFOLLOW;
        sl_flags++;
    }

    if (!follow_symlinks_in_path) {
        flags |= O_NOFOLLOW_ANY;
        sl_flags++;
    }

    if (sl_flags > 1) {
        N00B_CRAISE("Conflicting symbolic linking policies provided.");
    }

    n00b_ev2_cookie_t *cookie = n00b_gc_alloc_mapped(n00b_ev2_cookie_t,
                                                     N00B_GC_SCAN_ALL);
    int                perms  = mode_to_perms(mode);
    bool               path_is_relative;

    n00b_initialize_event(stream,
                          &n00b_fileio_impl,
                          perms,
                          n00b_io_ev_file,
                          cookie);

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
        n00b_list_t *parts = n00b_str_split(filename, n00b_get_slash_const());
        int          n     = n00b_list_len(parts);

        for (int i = 0; i < n; i++) {
            n00b_utf8_t *s = n00b_list_get(parts, i, NULL);
            if (!strcmp(s->data, "..")) {
                N00B_CRAISE("Path backtracking is disabled.");
            }
        }
    }

    filename = n00b_to_utf8(filename);

    if (relative_fd) {
        if (flags & O_CREAT) {
            cookie->id = openat(relative_fd,
                                filename->data,
                                flags,
                                permissions);
        }
        else {
            cookie->id = openat(relative_fd, filename->data, flags);
        }
    }
    else {
        if (normalize_paths) {
            filename = n00b_resolve_path(filename);
        }
        if (flags & O_CREAT) {
            cookie->id = open(filename->data, flags, permissions);
        }
        else {
            cookie->id = open(filename->data, flags);
        }
    }

    if (cookie->id == -1) {
        n00b_raise_errno();
    }

    add_file_info(stream, perms);

    n00b_file_data_t *file_info = cookie->aux;
    file_info->name             = filename;

    if (target_must_be_regular_file && file_info->kind != n00b_fk_regular) {
        N00B_CRAISE("Not a regular file.");
    }

    if (target_must_be_link && file_info->kind != n00b_fk_link) {
        N00B_CRAISE("Not a symbolic link.");
    }

    if (target_must_be_special_file && file_info->kind != n00b_fk_other) {
        N00B_CRAISE("Not a special file.");
    }
}

n00b_stream_t *
n00b_io_fd_open(int64_t fd, n00b_io_impl_info_t *impl)
{
    n00b_stream_base_t  *base   = n00b_get_ev_base(impl);
    n00b_io_permission_t perms  = n00b_io_perm_none;
    int                  flags  = fcntl(fd, F_GETFL);
    n00b_ev2_cookie_t   *cookie = n00b_new_ev2_cookie();

    if (flags & O_RDWR) {
        perms = n00b_io_perm_rw;
    }
    else {
        if (flags & O_RDONLY) {
            perms = n00b_io_perm_r;
        }
        else {
            if (flags & O_WRONLY) {
                perms = n00b_io_perm_w;
            }
            else {
                perms = n00b_io_perm_rw;
            }
        }
    }

    n00b_stream_t *new = n00b_alloc_party(impl,
                                          cookie,
                                          perms,
                                          n00b_io_ev_file);
    cookie->id         = fd;

    // Lock us; we'll unlock it when we've finished initializing.
    n00b_lock_debugging_on();
    n00b_acquire_party(new);
    n00b_lock_debugging_off();
    n00b_stream_t *result = n00b_add_or_replace(new,
                                                base->io_fd_cache,
                                                (void *)fd);

    if (result != new) {
        n00b_release_party(new);
        return result;
    }

    result = new;

    add_file_info(result, perms);

    n00b_lock_debugging_on();
    n00b_release_party(result);
    n00b_lock_debugging_off();

    return result;
}

bool
n00b_io_fd_subscribe(n00b_stream_sub_t *info, n00b_io_subscription_kind kind)
{
    // This is handed something 'complete' from the IO core
    // interface's point of view, and we have to implement it on top
    // of libevent.
    //
    // If nobody's listening to the party, we need to add the
    // listener. And, if the 'next_timeout' field is set, we should
    // always add the event.

    n00b_ev2_cookie_t *src_cookie = info->source->cookie;

    if (kind == n00b_io_sk_read) {
        info->source->read_active = true;
        n00b_event_add(src_cookie->read_event, NULL);
    }
    else {
        // Here, we don't add a libevent2 event or timeout; the event only
        // gets added when actual write requests come in.
        info->source->write_active = true;
    }

    return true;
}

n00b_stream_t *
n00b_fd_open(int fd)
{
    struct stat info;
    if (fstat(fd, &info)) {
        N00B_CRAISE("Invalid file descriptor.");
    }

    if (S_ISSOCK(info.st_mode)) {
        return n00b_io_socket_open(fd, &n00b_socket_impl);
    }

    return n00b_io_fd_open(fd, &n00b_fileio_impl);
}

static n00b_stream_t *__n00b_stdin_cache  = NULL;
static n00b_stream_t *__n00b_stdout_cache = NULL;
static n00b_stream_t *__n00b_stderr_cache = NULL;
static pthread_once_t stdio_init          = PTHREAD_ONCE_INIT;
void
n00b_stdio_init(void)
{
    __n00b_stdin_cache  = n00b_fd_open(fileno(stdin));
    __n00b_stdout_cache = n00b_fd_open(fileno(stdout));
    __n00b_stderr_cache = n00b_fd_open(fileno(stderr));

    n00b_gc_register_root(&__n00b_stdin_cache, 1);
    n00b_gc_register_root(&__n00b_stdout_cache, 1);
    n00b_gc_register_root(&__n00b_stderr_cache, 1);
}

n00b_stream_t *
n00b_stdin(void)
{
    pthread_once(&stdio_init, n00b_stdio_init);
    return __n00b_stdin_cache;
}

n00b_stream_t *
n00b_stdout(void)
{
    pthread_once(&stdio_init, n00b_stdio_init);
    return __n00b_stdout_cache;
}

n00b_stream_t *
n00b_stderr(void)
{
    pthread_once(&stdio_init, n00b_stdio_init);
    return __n00b_stderr_cache;
}

n00b_utf8_t *
n00b_get_fd_extras(n00b_stream_t *e)
{
    n00b_ev2_cookie_t *cookie = e->cookie;
    n00b_utf8_t       *aux    = cookie->aux;
    n00b_utf8_t       *xtxt;
    n00b_list_t       *extras = NULL;

    if (e->is_tty) {
        extras = n00b_list(n00b_type_utf8());
        n00b_list_append(extras, n00b_new_utf8("tty"));
    }

    if (e->closed_for_read | e->closed_for_write) {
        if (!extras) {
            extras = n00b_list(n00b_type_utf8());
        }

        if (e->perms != n00b_io_perm_rw
            || (e->closed_for_read && e->closed_for_write)) {
            n00b_list_append(extras, n00b_new_utf8("closed"));
        }
        else {
            if (e->closed_for_read) {
                n00b_list_append(extras, n00b_new_utf8("closed for read"));
            }
            else {
                n00b_list_append(extras, n00b_new_utf8("closed for write"));
            }
        }
    }

    if (e->error) {
        if (!extras) {
            extras = n00b_list(n00b_type_utf8());
        }
        n00b_list_append(extras, n00b_new_utf8("error"));
    }

    if (!aux) {
        aux = n00b_new_utf8("");
    }
    else {
        aux = n00b_cstr_format(" {}", aux);
    }

    if (!extras) {
        xtxt = n00b_new_utf8(" (open)");
    }
    else {
        xtxt = n00b_cstr_format(" ({})",
                                n00b_str_join(extras, n00b_new_utf8(", ")));
    }
    return n00b_str_concat(aux, xtxt);
}

static inline n00b_utf8_t *
repr_perms(n00b_stream_t *e)
{
    switch (e->perms) {
    case n00b_io_perm_none:
        return n00b_new_utf8("");
    case n00b_io_perm_r:
        return n00b_new_utf8("(r)");
    case n00b_io_perm_w:
        return n00b_new_utf8("(w)");
    case n00b_io_perm_rw:
        return n00b_new_utf8("(rw)");
    }
    n00b_unreachable();
}

n00b_utf8_t *
n00b_io_fd_repr(n00b_stream_t *e)
{
    n00b_ev2_cookie_t *cookie = e->cookie;

    return n00b_cstr_format("fd {}{}",
                            cookie->id,
                            repr_perms(e),
                            n00b_get_fd_extras(e));
}

static n00b_utf8_t *
n00b_file_repr(n00b_stream_t *f)
{
    n00b_ev2_cookie_t *cookie = f->cookie;
    n00b_file_data_t  *d      = cookie->aux;
    char              *s;

    if (!d->name) {
        return n00b_io_fd_repr(f);
    }

    switch (d->kind) {
    case n00b_fk_regular:
        s = "file";
        break;
    case n00b_fk_directory:
        s = "dir";
        break;
    case n00b_fk_link:
        s = "link";
        break;
    default:
        s = "special file";
        break;
    }
    return n00b_cstr_format("{} {}", n00b_new_utf8(s), d->name);
}

bool
n00b_io_fd_eof(n00b_stream_t *f)
{
    n00b_ev2_cookie_t *cookie = f->cookie;
    int                fd     = cookie->id;
    fd_set             select_set;

    FD_ZERO(&select_set);
    FD_SET(fd, &select_set);

    struct timeval tv = {.tv_sec = 0, .tv_usec = 0};

    if (select(fd + 1, &select_set, NULL, NULL, &tv) == -1) {
        return true;
    }

    return false;
}

n00b_io_impl_info_t n00b_fileio_impl = {
    .open_impl        = (void *)n00b_io_fd_open,
    .subscribe_impl   = n00b_io_fd_subscribe,
    .unsubscribe_impl = n00b_io_ev_unsubscribe,
    .read_impl        = n00b_io_enqueue_fd_read,
    .eof_impl         = n00b_io_fd_eof,
    .write_impl       = n00b_io_enqueue_fd_write,
    .repr_impl        = n00b_io_fd_repr,
    .close_impl       = n00b_io_close,
    .use_libevent     = true,
    .byte_oriented    = true,
};

const n00b_vtable_t n00b_file_vtable = {
    .num_entries = N00B_BI_NUM_FUNCS,
    .methods     = {
        [N00B_BI_CONSTRUCTOR] = (n00b_vtable_entry)n00b_new_file_init,
        [N00B_BI_TO_STR]      = (n00b_vtable_entry)n00b_file_repr,
        [N00B_BI_GC_MAP]      = (n00b_vtable_entry)N00B_GC_SCAN_ALL,
    },
};

// With the event system, we can can asynchronously wait for the IO
// loop to hit the end. But it's more sensical just to directly drain
// the descripfor ourselves.
n00b_buf_t *
n00b_read_binary_file(n00b_str_t *path, bool lock)
{
    bool           err = false;
    n00b_stream_t *f;

    N00B_TRY
    {
        f = n00b_new(n00b_type_file(),
                     path,
                     n00b_fm_read_only,
                     n00b_kw("exclusive_lock",
                             n00b_ka(lock)));
    }
    N00B_EXCEPT
    {
        err = true;
    }
    N00B_TRY_END;

    if (err) {
        return NULL;
    }

    n00b_ev2_cookie_t *cookie = f->cookie;

    if (!n00b_is_regular_file(f)) {
        N00B_CRAISE("Can only one-shot read regular files.");
    }

    n00b_buf_t *result = NULL;
    n00b_buf_t *tmp;

    char    buf[PIPE_BUF];
    ssize_t l = read(cookie->id, buf, PIPE_BUF);

    while (l) {
        if (l < 0) {
            if (errno != EINTR && errno != EAGAIN) {
                n00b_raise_errno();
            }
        }
        else {
            tmp    = n00b_new(n00b_type_buffer(),
                           n00b_kw(
                               "length",
                               n00b_ka(l),
                               "raw",
                               buf));
            result = n00b_buffer_add(result, tmp);
            l      = read(cookie->id, buf, PIPE_BUF);
        }
    }

    close(cookie->id);

    return result;
}

n00b_utf8_t *
n00b_read_utf8_file(n00b_str_t *path, bool lock)
{
    n00b_buf_t *b = n00b_read_binary_file(path, lock);

    if (!b) {
        return NULL;
    }

    return n00b_buf_to_utf8_string(b);
}
