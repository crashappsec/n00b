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
extern n00b_string_t *n00b_get_current_directory();
extern n00b_string_t *n00b_path_join(n00b_list_t *);
extern n00b_file_kind n00b_get_file_kind(n00b_string_t *);
extern n00b_list_t   *_n00b_path_walk(n00b_string_t *, ...);
extern n00b_string_t *n00b_app_path();
extern n00b_string_t *n00b_path_trim_trailing_slashes(n00b_string_t *);

#define n00b_path_walk(x, ...) _n00b_path_walk(x, N00B_VA(__VA_ARGS__))

static inline n00b_string_t *
n00b_get_home_directory()
{
    return n00b_path_tilde_expand(NULL);
}

// This maybe should move into a user / uid module, but I did it in
// the context of path testing (getting the proper user name for the
// tests).
static inline n00b_string_t *
n00b_get_user_name()
{
    struct passwd *pw = getpwuid(getuid());

    return n00b_cstring(pw->pw_name);
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

#ifdef N00B_USE_INTERNAL_API
// Strips IN PLACE.
extern void n00b_path_strip_slashes_both_ends(n00b_string_t *);
#endif
