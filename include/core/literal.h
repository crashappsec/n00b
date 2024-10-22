#pragma once
#include "n00b.h"

extern __uint128_t         n00b_raw_int_parse(n00b_utf8_t *,
                                             n00b_compile_error_t *,
                                             bool *);
extern __uint128_t         n00b_raw_hex_parse(char *,
                                             n00b_compile_error_t *);
extern void                n00b_init_literal_handling();
extern void                n00b_register_literal(n00b_lit_syntax_t,
                                                char *,
                                                n00b_builtin_t);
extern n00b_compile_error_t n00b_parse_simple_lit(n00b_token_t *,
                                                n00b_lit_syntax_t *,
                                                n00b_utf8_t **);
extern n00b_builtin_t       n00b_base_type_from_litmod(n00b_lit_syntax_t,
                                                     n00b_utf8_t *);
extern bool                n00b_fix_litmod(n00b_token_t *, n00b_pnode_t *);
