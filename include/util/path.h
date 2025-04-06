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

// n00b_tempfile actually returns a n00b_stream_t * but it's not
// defined here, so we'll just declare its return value void *
extern void *n00b_tempfile(n00b_string_t *, n00b_string_t *);

#define n00b_path_walk(x, ...) _n00b_path_walk(x, N00B_VA(__VA_ARGS__))

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

static inline n00b_list_t *
n00b_path_parts(n00b_string_t *p)
{
    if (!p || !p->u8_bytes) {
        return NULL;
    }

    n00b_list_t   *result   = n00b_list(n00b_type_string());
    n00b_string_t *resolved = n00b_resolve_path(p);
    n00b_string_t *filename;
    int            n;

    // A directory.
    if (p->data[p->u8_bytes - 1] == '/'
        || resolved->data[resolved->u8_bytes - 1] == '/') {
        n00b_list_append(result, resolved);
        n00b_list_append(result, n00b_cached_empty_string());
        n00b_list_append(result, n00b_cached_empty_string());

        return result;
    }

    n = n00b_string_rfind(resolved, n00b_cached_slash());

    n00b_list_append(result, n00b_string_slice(resolved, 0, n));
    filename = n00b_string_slice(resolved, n + 1, -1);

    n = n00b_string_rfind(filename, n00b_cached_period());

    if (n == -1) {
        n00b_list_append(result, filename);
        n00b_list_append(result, n00b_cached_empty_string());
        return result;
    }

    n00b_list_append(result, n00b_string_slice(filename, 0, n));
    n00b_list_append(result, n00b_string_slice(filename, n + 1, -1));

    return result;
}

#ifdef N00B_USE_INTERNAL_API
// Strips IN PLACE.
extern void n00b_path_strip_slashes_both_ends(n00b_string_t *);
#endif
