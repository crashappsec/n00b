#pragma once
#include "n00b.h"

typedef enum {
    n00b_fm_read_only  = O_RDONLY,
    n00b_fm_write_only = O_WRONLY,
    n00b_fm_rw         = O_RDWR,
} n00b_file_mode_t;

typedef enum {
    n00b_fk_regular,
    n00b_fk_directory,
    n00b_fk_link,
    n00b_fk_other,
} n00b_file_kind_t;

extern n00b_buf_t    *n00b_read_binary_file(n00b_string_t *, bool);
extern n00b_string_t *n00b_read_utf8_file(n00b_string_t *, bool);

#define n00b_read_text_file(x, y) n00b_read_utf8_file(x, y)

typedef struct {
    n00b_string_t   *name;
    uint64_t         noscan;
    struct stat      fd_info;
    n00b_file_kind_t kind;
} n00b_file_data_t;

static inline bool
n00b_is_directory(n00b_stream_t *stream)
{
    n00b_ev2_cookie_t *cookie = stream->cookie;

    if (n00b_type_is_file(n00b_get_my_type(stream))) {
        n00b_file_data_t *fd = cookie->aux;
        return fd->kind == n00b_fk_directory;
    }

    struct stat fd_info;

    fstat(cookie->id, &fd_info);

    return S_ISDIR(fd_info.st_mode);
}

static inline bool
n00b_is_regular_file(n00b_stream_t *stream)
{
    n00b_ev2_cookie_t *cookie = stream->cookie;

    if (n00b_type_is_file(n00b_get_my_type(stream))) {
        n00b_file_data_t *fd = cookie->aux;
        return fd->kind == n00b_fk_regular;
    }

    struct stat fd_info;

    fstat(cookie->id, &fd_info);

    return S_ISREG(fd_info.st_mode);
}

static inline bool
n00b_is_link(n00b_stream_t *stream)
{
    n00b_ev2_cookie_t *cookie = stream->cookie;

    if (n00b_type_is_file(n00b_get_my_type(stream))) {
        n00b_file_data_t *fd = cookie->aux;
        return fd->kind == n00b_fk_link;
    }

    struct stat fd_info;

    fstat(cookie->id, &fd_info);

    return S_ISLNK(fd_info.st_mode);
}

static inline bool
n00b_is_special_file(n00b_stream_t *stream)
{
    n00b_ev2_cookie_t *cookie = stream->cookie;

    if (n00b_type_is_file(n00b_get_my_type(stream))) {
        n00b_file_data_t *fd = cookie->aux;
        return fd->kind == n00b_fk_link;
    }

    struct stat fd_info;

    fstat(cookie->id, &fd_info);
    int mode = fd_info.st_mode;

    return !S_ISREG(mode) && !S_ISDIR(mode) && !S_ISLNK(mode);
}

static inline bool
n00b_is_socket(n00b_stream_t *stream)
{
    n00b_ev2_cookie_t *cookie = stream->cookie;
    struct stat        fd_info;

    fstat(cookie->id, &fd_info);

    return S_ISSOCK(fd_info.st_mode);
}
