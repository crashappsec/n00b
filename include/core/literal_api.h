#pragma once

#include "n00b.h"

extern __uint128_t          n00b_raw_int_parse(n00b_string_t *,
                                               n00b_compile_error_t *,
                                               bool *);
extern __uint128_t          n00b_raw_hex_parse(char *,
                                               n00b_compile_error_t *);
extern void                 n00b_init_literal_handling();
extern void                 n00b_register_literal(n00b_lit_syntax_t,
                                                  char *,
                                                  n00b_builtin_t);
extern n00b_compile_error_t n00b_parse_simple_lit(n00b_token_t *,
                                                  n00b_lit_syntax_t *,
                                                  n00b_string_t **);
extern n00b_builtin_t       n00b_base_type_from_litmod(n00b_lit_syntax_t,
                                                       n00b_string_t *);
extern bool                 n00b_fix_litmod(n00b_token_t *, n00b_pnode_t *);
extern void                 n00b_register_container_type(n00b_builtin_t,
                                                         n00b_lit_syntax_t,
                                                         bool);
extern void                 n00b_register_literal(n00b_lit_syntax_t,
                                                  char *,
                                                  n00b_builtin_t);
extern n00b_builtin_t       n00b_base_type_from_litmod(n00b_lit_syntax_t,
                                                       n00b_string_t *);
extern void                 n00b_init_literal_handling(void);
extern n00b_compile_error_t n00b_parse_simple_lit(n00b_token_t *,
                                                  n00b_lit_syntax_t *,
                                                  n00b_string_t **);
extern bool                 n00b_fix_litmod(n00b_token_t *, n00b_pnode_t *);
extern bool                 n00b_type_has_list_syntax(n00b_ntype_t);
extern bool                 n00b_type_has_dict_syntax(n00b_ntype_t);
extern bool                 n00b_type_has_set_syntax(n00b_ntype_t);
extern bool                 n00b_type_has_tuple_syntax(n00b_ntype_t);
extern int                  n00b_get_num_bitfield_words(void);
extern uint64_t            *n00b_get_list_bitfield(void);
extern uint64_t            *n00b_get_dict_bitfield(void);
extern uint64_t            *n00b_get_set_bitfield(void);
extern uint64_t            *n00b_get_tuple_bitfield(void);
extern uint64_t            *n00b_get_all_containers_bitfield(void);
extern bool                 n00b_partial_inference(n00b_ntype_t);
extern uint64_t            *n00b_get_no_containers_bitfield(void);
extern bool                 n00b_list_syntax_possible(n00b_ntype_t);
extern bool                 n00b_dict_syntax_possible(n00b_ntype_t);
extern bool                 n00b_set_syntax_possible(n00b_ntype_t);
extern bool                 n00b_tuple_syntax_possible(n00b_ntype_t);
extern void                 n00b_remove_list_options(n00b_ntype_t);
extern void                 n00b_remove_set_options(n00b_ntype_t);
extern void                 n00b_remove_dict_options(n00b_ntype_t);
extern void                 n00b_remove_tuple_options(n00b_ntype_t);
extern n00b_ntype_t         n00b_type_any_dict(n00b_ntype_t, n00b_ntype_t);
