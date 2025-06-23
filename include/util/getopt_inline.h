#pragma once
#include "n00b.h"

static inline void
n00b_getopt_show_usage(n00b_gopt_ctx      *ctx,
                       n00b_string_t      *cmd,
                       n00b_gopt_result_t *res)
{
    n00b_eprint(n00b_getopt_default_usage(ctx, cmd, res));
}

static inline n00b_list_t *
n00b_gopt_rule_any_word(void)
{
    return n00b_gopt_rule(N00B_GOG_LPAREN,
                          N00B_GOG_WORD,
                          N00B_GOG_RPAREN,
                          N00B_GOG_STAR);
}

static inline n00b_list_t *
n00b_gopt_rule_optional_word(void)
{
    return n00b_gopt_rule(N00B_GOG_LPAREN,
                          N00B_GOG_WORD,
                          N00B_GOG_RPAREN,
                          N00B_GOG_OPT);
}

static inline bool
n00b_gopt_has_errors(n00b_gopt_result_t *res)
{
    return res->errors && n00b_list_len(res->errors) != 0;
}

static inline n00b_list_t *
n00b_gopt_get_errors(n00b_gopt_result_t *res)
{
    return res->errors;
}

static inline n00b_string_t *
n00b_gopt_get_command(n00b_gopt_result_t *res)
{
    if (!res) {
        return NULL;
    }

    return res->cmd;
}

static inline n00b_dict_t *
n00b_gopt_get_flags(n00b_gopt_result_t *res)
{
    if (!res) {
        return NULL;
    }

    n00b_list_t *l      = n00b_dict_values(res->flags);
    n00b_dict_t *result = n00b_dict(n00b_type_string(), n00b_type_ref());
    int          n      = n00b_list_len(l);

    for (int i = 0; i < n; i++) {
        n00b_rt_option_t *opt = n00b_list_get(l, i, NULL);
        n00b_dict_add(result, opt->spec->name, opt->value);
    }

    return result;
}

static inline n00b_list_t *
n00b_gopt_get_args(n00b_gopt_result_t *res, n00b_string_t *section)
{
    return n00b_dict_get(res->args, section, NULL);
}

#ifdef N00B_USE_INTERNAL_API
static inline bool
n00b_gctx_gflag_is_set(n00b_gopt_ctx *ctx, uint32_t flag)
{
    return (bool)ctx->options & flag;
}

static inline bool
n00b_gopt_gflag_is_set(n00b_gopt_lex_state *state, uint32_t flag)
{
    return n00b_gctx_gflag_is_set(state->gctx, flag);
}
#endif
