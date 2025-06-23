#pragma once
#include "n00b.h"

typedef enum {
    n00b_tt_error,
    n00b_tt_space,
    n00b_tt_semi,
    n00b_tt_newline,
    n00b_tt_line_comment,
    n00b_tt_long_comment,
    n00b_tt_lock_attr,
    n00b_tt_plus,
    n00b_tt_minus,
    n00b_tt_mul,
    n00b_tt_div,
    n00b_tt_mod,
    n00b_tt_lte,
    n00b_tt_lt,
    n00b_tt_gte,
    n00b_tt_gt,
    n00b_tt_neq,
    n00b_tt_not,
    n00b_tt_colon,
    n00b_tt_assign,
    n00b_tt_cmp,
    n00b_tt_comma,
    n00b_tt_period,
    n00b_tt_lbrace,
    n00b_tt_rbrace,
    n00b_tt_lbracket,
    n00b_tt_rbracket,
    n00b_tt_lparen,
    n00b_tt_rparen,
    n00b_tt_and,
    n00b_tt_or,
    n00b_tt_int_lit,
    n00b_tt_hex_lit,
    n00b_tt_float_lit,
    n00b_tt_string_lit,
    n00b_tt_char_lit,
    n00b_tt_unquoted_lit,
    n00b_tt_true,
    n00b_tt_false,
    n00b_tt_nil,
    n00b_tt_if,
    n00b_tt_elif,
    n00b_tt_else,
    n00b_tt_for,
    n00b_tt_from,
    n00b_tt_to,
    n00b_tt_break,
    n00b_tt_continue,
    n00b_tt_return,
    n00b_tt_enum,
    n00b_tt_identifier,
    n00b_tt_func,
    n00b_tt_var,
    n00b_tt_global,
    n00b_tt_const,
    n00b_tt_once,
    n00b_tt_let,
    n00b_tt_private,
    n00b_tt_backtick,
    n00b_tt_arrow,
    n00b_tt_object,
    n00b_tt_while,
    n00b_tt_in,
    n00b_tt_bit_and,
    n00b_tt_bit_or,
    n00b_tt_bit_xor,
    n00b_tt_shl,
    n00b_tt_shr,
    n00b_tt_typeof,
    n00b_tt_switch,
    n00b_tt_case,
    n00b_tt_plus_eq,
    n00b_tt_minus_eq,
    n00b_tt_mul_eq,
    n00b_tt_div_eq,
    n00b_tt_mod_eq,
    n00b_tt_bit_and_eq,
    n00b_tt_bit_or_eq,
    n00b_tt_bit_xor_eq,
    n00b_tt_shl_eq,
    n00b_tt_shr_eq,
    n00b_tt_lock,
#ifdef N00B_DEV
    n00b_tt_print,
#endif
    n00b_tt_eof
} n00b_token_kind_t;

typedef struct {
    struct n00b_module_t *module;
    n00b_codepoint_t     *start_ptr;
    n00b_codepoint_t     *end_ptr;
    n00b_string_t        *literal_modifier;
    void                 *literal_value; // Once parsed.
    n00b_string_t        *text;
    uint64_t              noscan;
    n00b_lit_syntax_t     syntax;
    n00b_token_kind_t     kind;
    int                   token_id;
    int                   line_no;
    int                   line_offset;
    // Original index relative to all added tokens under a parse node.
    // We do this because we don't keep the comments in the main tree,
    // we stash them with the node payload.
    uint32_t              child_ix;
    uint8_t               adjustment; // For keeping track of quoting.

} n00b_token_t;

extern bool           n00b_lex(n00b_module_t *, n00b_stream_t *);
extern n00b_string_t *n00b_format_one_token(n00b_token_t *, n00b_string_t *);
extern n00b_table_t  *n00b_format_tokens(n00b_module_t *);
extern n00b_string_t *n00b_token_type_to_string(n00b_token_kind_t);
extern n00b_string_t *n00b_token_raw_content(n00b_token_t *);
