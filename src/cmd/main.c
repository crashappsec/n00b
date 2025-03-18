#define N00B_USE_INTERNAL_API
#include "n00b.h"
#include "n00b/cmd.h"

static n00b_list_t *
resolve_paths(n00b_list_t *args)
{
    n00b_list_t *result = n00b_list(n00b_type_string());
    int          n      = n00b_list_len(args);

    for (int i = 0; i < n; i++) {
        n00b_string_t *s = n00b_list_get(args, i, NULL);
        s                = n00b_resolve_path(s);
        n00b_list_append(result, s);
    }

    return result;
}

static int
get_cmd_id(n00b_string_t *cmd)
{
    if (n00b_string_eq(cmd, n00b_cstring("compile"))) {
        return N00B_CMD_COMPILE;
    }
    if (n00b_string_eq(cmd, n00b_cstring("build"))) {
        return N00B_CMD_COMPILE;
    }
    if (n00b_string_eq(cmd, n00b_cstring("run"))) {
        return N00B_CMD_RUN;
    }

    return N00B_CMD_REPL;
}

static inline void
n00b_show_error_output(n00b_cmdline_ctx *ctx)
{
    n00b_table_t *err_output = n00b_format_errors(ctx->cctx);

    if (err_output) {
        n00b_eprint(n00b_crich("«em2»Errors:"));
        n00b_eprint(err_output);
    }
}

extern void n00b_rich_lex(n00b_string_t *s);

const char *MD_EX =
    "# This is a title.\n"
    "\n"
    "## This is a subtitle.\n"
    "Here's a block of *normal* text with some _highlights_.\n"
    "It runs a couple of lines, but not **too** long.\n"
    "\n"
    "Here's the next block of text, which should show up as "
    "its own separate paragraph."
    "\n"
    "1. Hello\n"
    "2. Sailor\n"
    "\n"
    "- Hi\n"
    "- Mom\n"
    "\n\n"
    /*    "|       |       |\n"
        "|-------|-------|\n"
        "|   1   |   2   |\n"
        "|   2   |   4   |\n"
        "\n"*/
    "### Bye!\n";

void
layout_test(void)
{
    n00b_tree_node_t *layout = n00b_new_layout();
    n00b_new_layout_cell(layout,
                         n00b_kw("min",
                                 n00b_ka(10),
                                 "preference",
                                 n00b_ka(20)));
    n00b_new_layout_cell(layout,
                         n00b_kw("flex",
                                 n00b_ka(1),
                                 "preference",
                                 n00b_ka(20)));
    n00b_new_layout_cell(layout,
                         n00b_kw("flex",
                                 n00b_ka(1),
                                 "preference",
                                 n00b_ka(30)));
    n00b_new_layout_cell(layout,
                         n00b_kw("min",
                                 n00b_ka(5),
                                 "max",
                                 n00b_ka(10)));

    n00b_tree_node_t *t = n00b_layout_calculate(layout, 120);
    n00b_string_t    *s = n00b_to_string(t);
    n00b_eprintf("layout has «#:i» kids.\n", (int64_t)layout->num_kids);
    n00b_eprintf("res 1 has «#:i» kids.\n", (int64_t)t->num_kids);
    n00b_eprintf("«em2»Layout 1");
    n00b_eprint(s);
    t = n00b_layout_calculate(layout, 20);
    s = n00b_to_string(t);
    n00b_eprintf("«em2»Layout 2");
    n00b_eprint(s);
    t = n00b_layout_calculate(layout, 19);
    s = n00b_to_string(t);
    n00b_eprintf("«em2»Layout 3");
    n00b_eprint(s);
    t = n00b_layout_calculate(layout, 10);
    s = n00b_to_string(t);
    n00b_eprintf("«em2»Layout 4«/»\n«#»", s);
}

void
tmp_testing(void)
{
    n00b_string_t *s;
    n00b_string_t *tc;

    tc = n00b_theme_palate_table(n00b_get_current_theme());
    n00b_eprint(tc);

    tc = n00b_preview_theme(n00b_get_current_theme());
    n00b_eprint(tc);

    s = n00b_cstring("default text.");

    n00b_eprint(s);

    n00b_eprint(n00b_cformat("«#:i»wow", 1000));

    layout_test();

    n00b_tree_node_t *t   = n00b_parse_markdown(n00b_cstring(MD_EX));
    n00b_table_t     *tbl = n00b_repr_md_parse(t);
    n00b_eprint(tbl);
    n00b_eprint(n00b_markdown_to_table(n00b_cstring(MD_EX), false));

    n00b_eprint(n00b_crich("example [|h1|]bar "));
    n00b_eprint(n00b_crich("[|em2|]example[|/|] [|em1|]bar «em2»[|bah"));
    n00b_eprint(n00b_crich("«i»«b»hello,[|/b|] world!"));
    n00b_eprint(n00b_crich("«i»hell«b»«strikethrough»o, wo[|/|]rld!"));
    n00b_eprint(n00b_crich("«strikethrough»hello, wo[|/|]rld!"));
    n00b_eprint(n00b_crich("«u»hello, wo[|/|]rld!"));
    n00b_eprint(n00b_crich("«uu»hello, wo[|/|]rld!"));
    n00b_eprint(n00b_crich("«allcaps»hello, wo[|/|]rld!"));
    n00b_eprint(n00b_crich("«upper»hello, wo[|/|]rld!"));

    s = n00b_crich("example [|em1|]bar ");
    n00b_eprint(n00b_cformat("«caps»wow, bob, wow!", s));
    n00b_eprint(n00b_cformat("«caps»«#0:»wow", s));
    n00b_eprint(n00b_cformat("«upper»«#0:»wow", s));
    n00b_eprint(n00b_cformat("«upper»«#0:!»wow", s));
    n00b_eprint(n00b_cformat("«upper»«#1!»wow", s));

    n00b_string_t *s1 = n00b_cstring("Hello, ");
    n00b_string_t *s2 = n00b_cstring("world.");

    n00b_list_t *l = n00b_list(n00b_type_string());
    n00b_list_append(l, s1);
    n00b_list_append(l, s2);
    n00b_eprint(n00b_unordered_list(l, NULL));
    n00b_eprint(n00b_ordered_list(l, NULL));

    s2                = n00b_string_style_by_tag(s2,
                                  n00b_cstring("h6"));
    n00b_string_t *s3 = n00b_string_concat(s1, s2);
    n00b_eprint(s3);

    n00b_exit(0);
}

int
main(int argc, char **argv, char **envp)
{
    n00b_gopt_result_t *opt_res = n00b_basic_setup(argc, argv, envp);
    n00b_cmdline_ctx   *ctx     = n00b_gc_alloc_mapped(n00b_cmdline_ctx,
                                                 N00B_GC_SCAN_ALL);

    ctx->cmd       = n00b_gopt_get_command(opt_res);
    ctx->args      = NULL;
    ctx->opts      = n00b_gopt_get_flags(opt_res);
    ctx->cctx      = NULL;
    ctx->vm        = NULL;
    ctx->cmd_parse = opt_res ? opt_res->tree : NULL;
    ctx->exit_code = 0;

    if (ctx->cmd) {
        ctx->args = resolve_paths(n00b_gopt_get_args(opt_res, ctx->cmd));
    }

    if (!n00b_cmd_quiet(ctx)) {
        n00b_eprintf(
            "«em2»N00b «#».«#».«#»«/» «i»(«#», «#»)«/»\n"
            "© 2024-2025 Crash Override, Inc.\n"
            "Licensed under the BSD-3-Clause license.\n",
            (int64_t)N00B_VERS_MAJOR,
            (int64_t)N00B_VERS_MINOR,
            (int64_t)N00B_VERS_PATCH,
            n00b_compile_date(),
            n00b_compile_time());

        n00b_eprintf("«h3»N00b Root:«/» «#»", n00b_n00b_root());
    }

    if (!opt_res) {
        n00b_exit(-1);
    }

    switch (get_cmd_id(ctx->cmd)) {
    case N00B_CMD_RUN:
        n00b_compile_and_run(ctx);
        break;
    case N00B_CMD_COMPILE:
        n00b_compile(ctx, true);
        break;
    case N00B_CMD_REPL:
        tmp_testing();
        // n00b_eprintf("«em2»Interactive mode is not implemented yet.");
        ctx->exit_code = N00B_NOT_DONE;
    }

    n00b_show_cmdline_debug_info(ctx);
    n00b_show_compiler_debug_info(ctx);
    n00b_show_error_output(ctx);

    n00b_exit(ctx->exit_code);
}
