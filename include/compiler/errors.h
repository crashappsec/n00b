#pragma once
#include "n00b.h"

extern n00b_string_t   *n00b_format_error_message(n00b_compile_error *, bool);
extern n00b_table_t *n00b_format_errors(n00b_compile_ctx *);
extern n00b_table_t *n00b_format_runtime_errors(n00b_list_t *);
extern n00b_list_t  *n00b_compile_extract_all_error_codes(n00b_compile_ctx *);
extern n00b_string_t  *n00b_err_code_to_str(n00b_compile_error_t);

extern n00b_compile_error *n00b_base_add_error(n00b_list_t *,
                                               n00b_compile_error_t,
                                               n00b_string_t *,
                                               n00b_err_severity_t,
                                               va_list);
extern n00b_compile_error *n00b_base_runtime_error(n00b_list_t *,
                                                   n00b_compile_error_t,
                                                   n00b_string_t *,
                                                   va_list);
extern n00b_compile_error *_n00b_add_error(n00b_module_t *,
                                           n00b_compile_error_t,
                                           n00b_tree_node_t *,
                                           ...);
extern n00b_compile_error *_n00b_add_warning(n00b_module_t *,
                                             n00b_compile_error_t,
                                             n00b_tree_node_t *,
                                             ...);
extern n00b_compile_error *_n00b_add_info(n00b_module_t *,
                                          n00b_compile_error_t,
                                          n00b_tree_node_t *,
                                          ...);
extern n00b_compile_error *_n00b_add_spec_error(n00b_spec_t *,
                                                n00b_compile_error_t,
                                                n00b_tree_node_t *,
                                                ...);
extern n00b_compile_error *_n00b_add_spec_warning(n00b_spec_t *,
                                                  n00b_compile_error_t,
                                                  n00b_tree_node_t *,
                                                  ...);
extern n00b_compile_error *_n00b_add_spec_info(n00b_spec_t *,
                                               n00b_compile_error_t,
                                               n00b_tree_node_t *,
                                               ...);
extern n00b_compile_error *_n00b_error_from_token(n00b_module_t *,
                                                  n00b_compile_error_t,
                                                  n00b_token_t *,
                                                  ...);

extern void _n00b_module_load_error(n00b_module_t *,
                                    n00b_compile_error_t,
                                    ...);

extern void _n00b_module_load_warn(n00b_module_t *,
                                   n00b_compile_error_t,
                                   ...);

extern n00b_compile_error *n00b_new_error(int);

#define n00b_add_error(x, y, z, ...) \
    _n00b_add_error(x, y, z, N00B_VA(__VA_ARGS__))

#define n00b_add_warning(x, y, z, ...) \
    _n00b_add_warning(x, y, z, N00B_VA(__VA_ARGS__))

#define n00b_add_info(x, y, z, ...) \
    _n00b_add_info(x, y, z, N00B_VA(__VA_ARGS__))

#define n00b_add_spec_error(x, y, z, ...) \
    _n00b_add_spec_error(x, y, z, N00B_VA(__VA_ARGS__))

#define n00b_add_spec_warning(x, y, z, ...) \
    _n00b_add_spec_warning(x, y, z, N00B_VA(__VA_ARGS__))

#define n00b_add_spec_info(x, y, z, ...) \
    _n00b_add_spec_info(x, y, z, N00B_VA(__VA_ARGS__))

#define n00b_error_from_token(x, y, z, ...) \
    _n00b_error_from_token(x, y, z, N00B_VA(__VA_ARGS__))

#define n00b_module_load_error(x, y, ...) \
    _n00b_module_load_error(x, y, N00B_VA(__VA_ARGS__))

#define n00b_module_load_warn(x, y, ...) \
    _n00b_module_load_warn(x, y, N00B_VA(__VA_ARGS__))

static inline bool
n00b_fatal_error_in_module(n00b_module_t *ctx)
{
    return ctx->ct->fatal_errors != 0;
}
