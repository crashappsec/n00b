#pragma once
#include "n00b.h"

typedef enum {
    N00B_FK_NOT_FOUND       = 0,
    N00B_FK_IS_REG_FILE     = S_IFREG,
    N00B_FK_IS_DIR          = S_IFDIR,
    N00B_FK_IS_FLINK        = S_IFLNK,
    N00B_FK_IS_DLINK        = S_IFLNK | S_IFDIR,
    N00B_FK_IS_SOCK         = S_IFSOCK,
    N00B_FK_IS_CHR_DEVICE   = S_IFCHR,
    N00B_FK_IS_BLOCK_DEVICE = S_IFBLK,
    N00B_FK_IS_FIFO         = S_IFIFO,
    N00B_FK_OTHER           = ~0,
} n00b_file_kind;

extern n00b_string_t *n00b_resolve_path(n00b_string_t *);
extern n00b_string_t *n00b_path_tilde_expand(n00b_string_t *);
extern n00b_string_t *n00b_get_user_dir(n00b_string_t *);
extern n00b_string_t *n00b_get_current_directory(void);
extern bool           n00b_set_current_directory(n00b_string_t *);
extern n00b_string_t *n00b_path_join(n00b_list_t *);
extern n00b_file_kind n00b_get_file_kind(n00b_string_t *);
extern n00b_list_t   *_n00b_path_walk(n00b_string_t *, ...);
extern n00b_string_t *n00b_app_path(void);
extern n00b_string_t *n00b_path_trim_trailing_slashes(n00b_string_t *);
extern n00b_string_t *n00b_new_temp_dir(n00b_string_t *, n00b_string_t *);
extern n00b_string_t *n00b_get_temp_root(void);
extern n00b_string_t *n00b_filename_from_path(n00b_string_t *);
extern n00b_list_t   *n00b_find_file_in_program_path(n00b_string_t *,
                                                     n00b_list_t *);
extern n00b_list_t   *n00b_find_command_paths(n00b_string_t *,
                                              n00b_list_t *,
                                              bool);
extern n00b_string_t *n00b_rename(n00b_string_t *, n00b_string_t *);
extern n00b_list_t   *n00b_path_parts(n00b_string_t *);
extern n00b_list_t   *_n00b_list_directory(n00b_string_t *, ...);
extern n00b_string_t *n00b_path_get_extension(n00b_string_t *);
extern n00b_string_t *n00b_path_remove_extension(n00b_string_t *);

// n00b_tempfile actually returns a n00b_stream_t * but it's not
// defined here, so we'll just declare its return value void *
extern void *n00b_tempfile(n00b_string_t *, n00b_string_t *);

static inline bool
n00b_path_exists(n00b_string_t *s)
{
    return n00b_get_file_kind(s) != N00B_FK_NOT_FOUND;
}

static inline bool
n00b_path_is_file(n00b_string_t *s)
{
    switch (n00b_get_file_kind(s)) {
    case N00B_FK_IS_REG_FILE:
    case N00B_FK_IS_FLINK:
        return true;
    default:
        return false;
    }
}

static inline bool
n00b_path_is_directory(n00b_string_t *s)
{
    switch (n00b_get_file_kind(s)) {
    case N00B_FK_IS_DIR:
    case N00B_FK_IS_DLINK:
        return true;
    default:
        return false;
    }
}

static inline bool
n00b_path_is_link(n00b_string_t *s)
{
    switch (n00b_get_file_kind(s)) {
    case N00B_FK_IS_FLINK:
    case N00B_FK_IS_DLINK:
        return true;
    default:
        return false;
    }
}

#define n00b_path_walk(x, ...) \
    _n00b_path_walk(x, N00B_VA(__VA_ARGS__))
#define n00b_list_directory(x, ...) \
    _n00b_list_directory(x, N00B_VA(__VA_ARGS__))

static inline n00b_string_t *
n00b_find_first_command_path(n00b_string_t *s, n00b_list_t *l, bool self)
{
    n00b_list_t *resolved = n00b_find_command_paths(s, l, self);

    if (!n00b_list_len(resolved)) {
        return NULL;
    }

    return n00b_private_list_get(resolved, 0, NULL);
}

static inline n00b_string_t *
n00b_get_home_directory(void)
{
    return n00b_path_tilde_expand(NULL);
}

// Inherently racy.
static inline bool
n00b_file_exists(n00b_string_t *filename)
{
    struct stat info;
    return stat(filename->data, &info) == 0;
}

// This maybe should move into a user / uid module, but I did it in
// the context of path testing (getting the proper user name for the
// tests).
static inline n00b_string_t *
n00b_get_user_name(void)
{
    struct passwd *pw = getpwuid(getuid());

    return n00b_cstring(pw->pw_name);
}

static inline n00b_list_t *
n00b_get_program_search_path(void)
{
    return n00b_string_split(n00b_get_env(n00b_cstring("PATH")),
                             n00b_cached_colon());
}

static inline n00b_string_t *
n00b_path_simple_join(n00b_string_t *p1, n00b_string_t *p2)
{
    if (n00b_string_starts_with(p2, n00b_cached_slash())) {
        return p2;
    }

    if (!p1 || !p1->codepoints) {
        p1 = n00b_cached_slash();
    }

    n00b_list_t *x = n00b_list(n00b_type_string());
    n00b_list_append(x, p1);
    n00b_list_append(x, p2);

    return n00b_path_join(x);
}

static inline n00b_string_t *
n00b_path_get_file(n00b_string_t *s)
{
    s                  = n00b_path_trim_trailing_slashes(n00b_resolve_path(s));
    n00b_list_t *parts = n00b_string_split(s, n00b_cached_slash());

    return n00b_list_pop(parts);
}

#ifdef N00B_USE_INTERNAL_API
extern int            n00b_lexical_sort_fn(const n00b_string_t **,
                                           const n00b_string_t **);
// These two strip IN PLACE.
extern void           n00b_path_strip_slashes_both_ends(n00b_string_t *);
extern n00b_string_t *n00b_path_chop_extension(n00b_string_t *);
#endif
