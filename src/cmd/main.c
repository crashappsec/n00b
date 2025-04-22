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
    if (!cmd->codepoints) {
        return N00B_CMD_REPL;
    }
    if (n00b_string_eq(cmd, n00b_cstring("compile"))) {
        return N00B_CMD_COMPILE;
    }
    if (n00b_string_eq(cmd, n00b_cstring("build"))) {
        return N00B_CMD_COMPILE;
    }
    if (n00b_string_eq(cmd, n00b_cstring("run"))) {
        return N00B_CMD_RUN;
    }
    if (n00b_string_eq(cmd, n00b_cstring("record"))) {
        return N00B_CMD_RECORD;
    }
    if (n00b_string_eq(cmd, n00b_cstring("play"))) {
        return N00B_CMD_PLAY;
    }
    if (n00b_string_eq(cmd, n00b_cstring("test"))) {
        return N00B_CMD_TEST;
    }
    if (n00b_string_eq(cmd, n00b_cstring("test.show"))) {
        return N00B_CMD_TEST_SHOW;
    }
    n00b_unreachable();
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

static void
show_n00b_info(n00b_cmdline_ctx *ctx)
{
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

    if (n00b_cmd_bright(ctx)) {
        n00b_set_current_theme(n00b_lookup_theme(n00b_cstring("n00b-bright")));
    }

    if (opt_res) {
        ctx->args = n00b_gopt_get_args(opt_res, ctx->cmd);
    }

    if (opt_res && opt_res->tree) {
        //        n00b_eprint(n00b_grammar_format(opt_res->debug->grammar));
        //        n00b_eprint(n00b_parse_tree_format(opt_res->tree));
    }

    if (!opt_res) {
        n00b_exit(-1);
    }

    switch (get_cmd_id(ctx->cmd)) {
    case N00B_CMD_RUN:
        ctx->args = resolve_paths(ctx->args);
        show_n00b_info(ctx);
        n00b_compile_and_run(ctx);
        break;
    case N00B_CMD_COMPILE:
        ctx->args = resolve_paths(ctx->args);
        show_n00b_info(ctx);
        n00b_compile(ctx, true);
        break;
    case N00B_CMD_REPL:
        show_n00b_info(ctx);
        n00b_eprintf("«em2»Interactive mode is not implemented yet.");
        ctx->exit_code = N00B_NOT_DONE;
        break;
    case N00B_CMD_RECORD:
        show_n00b_info(ctx);
        n00b_record_testcase(ctx);
        n00b_exit(ctx->exit_code);
        break;
    case N00B_CMD_PLAY:
        n00b_play_capture(ctx);
        n00b_exit(ctx->exit_code);
        break;
    case N00B_CMD_TEST:
        n00b_run_tests(ctx);
        n00b_exit(ctx->exit_code);
    case N00B_CMD_TEST_SHOW:
        n00b_show_tests(ctx);
        n00b_exit(0);
    default:
        n00b_unreachable();
    }

    n00b_show_cmdline_debug_info(ctx);
    n00b_show_compiler_debug_info(ctx);
    n00b_show_error_output(ctx);

    n00b_exit(ctx->exit_code);
}
