#pragma once

#include "n00b.h"

extern n00b_utf32_t *n00b_wrapper_join(n00b_list_t *, const n00b_str_t *);
extern n00b_str_t   *n00b_wrapper_hostname(void);
extern n00b_str_t   *n00b_wrapper_os(void);
extern n00b_str_t   *n00b_wrapper_arch(void);
extern n00b_str_t   *n00b_wrapper_repr(n00b_obj_t);
extern n00b_str_t   *n00b_wrapper_to_str(n00b_obj_t);
extern void         n00b_snap_column(n00b_grid_t *, int64_t);
