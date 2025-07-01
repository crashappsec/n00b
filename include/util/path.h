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

#ifdef N00B_USE_INTERNAL_API
// These two strip IN PLACE.
extern void           n00b_path_strip_slashes_both_ends(n00b_string_t *);
extern n00b_string_t *n00b_path_chop_extension(n00b_string_t *);
#endif
