#define N00B_USE_INTERNAL_API
#include "n00b.h"
#include "n00b/cmd.h"

void
n00b_run_tests(n00b_cmdline_ctx *ctx)
{
    // clang-format off
    n00b_testing_ctx *tctx = n00b_testgen_setup(args    : ctx->args,
                                                verbose : n00b_cmd_verbose(ctx),
                                                quiet   : n00b_cmd_quiet(ctx));
    // clang-format on

#if defined(N00B_DEBUG)
    tctx->debug = true;
#endif
    ctx->exit_code = n00b_testgen_run_tests(tctx);

    if (ctx->exit_code) {
        n00b_eprintf("\n[|red|]FAILED[|/|] [|#|] tests.", (int64_t)ctx->exit_code);
    }
}

void
n00b_show_tests(n00b_cmdline_ctx *ctx)
{
    n00b_testing_ctx *tctx = n00b_testgen_setup(args : ctx->args);

    n00b_eprintf(
        "[|em|][|#|][|/|] groups in use, [|em|][|#|][|/|] tests.",
        tctx->groups_in_use,
        tctx->tests_in_test_dir);

    int n = n00b_list_len(tctx->groups);

    for (int i = 0; i < n; i++) {
        n00b_test_group_t *g   = n00b_list_get(tctx->groups, i, NULL);
        n00b_table_t      *tbl = n00b_table(columns : 4);

        n00b_table_add_cell(tbl, n00b_cstring("ID"));
        n00b_table_add_cell(tbl, n00b_cstring("Name"));
        n00b_table_add_cell(tbl, n00b_cstring("Timeout"));
        n00b_table_add_cell(tbl, n00b_cstring("# Commands"));
        int m = n00b_list_len(g->tests);
        for (int j = 0; j < m; j++) {
            n00b_test_t *tc = n00b_list_get(g->tests, j, NULL);
            n00b_table_add_cell(tbl, n00b_cformat("[|#|]", tc->id + 1));
            n00b_table_add_cell(tbl, tc->name);
            n00b_table_add_cell(tbl, n00b_cformat("[|#|]", tc->timeout_sec));
            n00b_table_add_cell(tbl,
                                n00b_cformat("[|#|]",
                                             n00b_list_len(tc->commands)));
        }
        n00b_eprintf("[|em3|]Group:[|/|] [|em6|][|#|][|/|] ", g->name);
        n00b_eprint(tbl);
    }
}
