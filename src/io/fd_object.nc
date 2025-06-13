#define N00B_USE_INTERNAL_API
#include "n00b.h"

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
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &true_val, sizeof(int));
    setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &true_val, sizeof(int));
    setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &true_val, sizeof(int));
#if defined(__APPLE__) || defined(__FreeBSD__)
    const int linger = N00B_SOCKET_LINGER_SEC;

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
    n00b_fd_stream_t *result = n00b_fd_cache_lookup(fd, dispatcher);

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

    n00b_fd_stream_t *check = n00b_fd_cache_add(result);

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
