#pragma once
#include "n00b.h"

extern bool        n00b_lex(n00b_module_t *, n00b_stream_t *);
extern n00b_utf8_t *n00b_format_one_token(n00b_token_t *, n00b_str_t *);
extern n00b_grid_t *n00b_format_tokens(n00b_module_t *);
extern n00b_utf8_t *n00b_token_type_to_string(n00b_token_kind_t);
extern n00b_utf8_t *n00b_token_raw_content(n00b_token_t *);

static inline n00b_utf8_t *
n00b_token_get_location_str(n00b_token_t *t)
{
    if (t->module) {
        return n00b_cstr_format("{}:{}:{}",
                               t->module->path,
                               n00b_box_i64(t->line_no),
                               n00b_box_i64(t->line_offset + 1));
    }

    return n00b_cstr_format("{}:{}",
                           n00b_box_i64(t->line_no),
                           n00b_box_i64(t->line_offset + 1));
}
