#define N00B_USE_INTERNAL_API
#include "n00b.h"
#include "n00b/cmd.h"

#define decl_parse_option_func(option_base_name)                       \
    static inline bool                                                 \
        n00b_cmd_##option_base_name(n00b_cmdline_ctx *ctx)             \
    {                                                                  \
        bool    found;                                                 \
        int64_t r;                                                     \
        r = hatrack_dict_get(ctx->opts,                                \
                             n00b_cstring(n00b_fl_##option_base_name), \
                             &found)                                   \
         || (n00b_cmd_show_all(ctx) && !found);                        \
        return (bool)r;                                                \
    }

decl_parse_option_func(show_source);
decl_parse_option_func(show_tokens);
decl_parse_option_func(show_parse);
decl_parse_option_func(show_cfg);
decl_parse_option_func(show_scopes);
decl_parse_option_func(show_deps);
decl_parse_option_func(show_spec);
decl_parse_option_func(show_disassembly);
decl_parse_option_func(show_function_info);
decl_parse_option_func(show_modules);

static inline bool
n00b_show_any_debug_info(n00b_cmdline_ctx *ctx)
{
    return n00b_cmd_show_source(ctx) || n00b_cmd_show_tokens(ctx)
        || n00b_cmd_show_parse(ctx) || n00b_cmd_show_cfg(ctx)
        || n00b_cmd_show_scopes(ctx) || n00b_cmd_show_deps(ctx)
        || n00b_cmd_show_spec(ctx) || n00b_cmd_show_disassembly(ctx)
        || n00b_cmd_show_function_info(ctx);
}

static void
n00b_show_module_debug_info(n00b_cmdline_ctx *ctx, n00b_module_t *m, bool entry)
{
  if (!n00b_show_any_debug_info(ctx)) {
        return;
    }

    if (entry) {
        n00b_debug("entry point path", m->path);
    }
    else {
        n00b_debug("module path", m->path);
    }

    if (n00b_cmd_show_source(ctx)) {
        n00b_debug("source code", m->source);
    }

    if (n00b_cmd_show_tokens(ctx)) {
        n00b_debug("tokens", n00b_format_tokens(m));
    }

    if (n00b_cmd_show_parse(ctx)) {
        if (ctx->cctx->entry_point->ct->parse_tree) {
            n00b_debug("parse tree", n00b_repr_module_parse_tree(m));
        }
        else {
            n00b_debug("parse tree",
                       n00b_crich("«em3»No parse tree produced."));
        }
    }

    if (n00b_cmd_show_cfg(ctx)) {
        if (ctx->cctx->entry_point->ct->cfg) {
            n00b_debug("control flow", n00b_cfg_repr(m->ct->cfg));
        }
        else {
            n00b_debug("control flow",
                       n00b_crich("«em3»No control flow graph produced."));
        }
    }

    if (n00b_cmd_show_function_info(ctx)) {
        int n = n00b_list_len(m->fn_def_syms);

        for (int i = 0; i < n; i++) {
            n00b_symbol_t  *sym  = n00b_list_get(m->fn_def_syms, i, NULL);
            n00b_fn_decl_t *decl = sym->value;

            n00b_debug("function", sym->name);
            n00b_debug("function type", sym->type);

            if (n00b_cmd_show_cfg(ctx)) {
                n00b_debug("function cfg", n00b_cfg_repr(decl->cfg));
            }
            if (n00b_cmd_show_scopes(ctx)) {
                n00b_debug("function scope",
                           n00b_format_scope(decl->signature_info->fn_scope));
            }
        }
    }

    if (n00b_cmd_show_scopes(ctx)) {
        n00b_debug("module scope",
                   n00b_format_scope(m->module_scope));
    }

    if (ctx->vm && n00b_cmd_show_disassembly(ctx)) {
        n00b_debug("disassembly", n00b_disasm(ctx->vm, m));
    }
}

void
n00b_show_compiler_debug_info(n00b_cmdline_ctx *ctx)
{
    if (ctx->exit_code) {
        return;
    }

    if (!ctx->cctx->entry_point->path) {
        return;
    }

    n00b_enable_debugging();

    if (n00b_cmd_show_modules(ctx)) {
        int n = n00b_list_len(ctx->cctx->module_ordering);

        for (int i = 0; i < n; i++) {
            n00b_module_t *m = n00b_list_get(ctx->cctx->module_ordering,
                                             i,
                                             NULL);
            if (m == ctx->cctx->entry_point) {
                continue;
            }
            n00b_show_module_debug_info(ctx, m, false);
        }
    }

    if (n00b_cmd_show_scopes(ctx)) {
        n00b_debug("global scope", n00b_format_scope(ctx->cctx->final_globals));

        n00b_debug("attribute scope",
                   n00b_format_scope(ctx->cctx->final_attrs));
    }

    if (n00b_cmd_show_deps(ctx)) {
        n00b_debug("loaded dependencies",
                   n00b_get_module_summary_info(ctx->cctx));
    }

    if (n00b_cmd_show_spec(ctx)) {
        n00b_debug("attribute spec",
                   n00b_repr_spec(ctx->cctx->final_spec));
    }

    n00b_show_module_debug_info(ctx, ctx->cctx->entry_point, true);
}

void
n00b_show_cmdline_debug_info(n00b_cmdline_ctx *ctx)
{
    if (n00b_cmd_show_cmdline_parse(ctx)) {
        n00b_debug("n00b_parsed_cmd", ctx->cmd);
        n00b_debug("n00b_parsed_args", ctx->args);
        n00b_debug("n00b_cmd_parse_tree",
                   n00b_parse_tree_format(ctx->cmd_parse));
        n00b_debug("n00b_cmd_options", ctx->opts);
    }
}
