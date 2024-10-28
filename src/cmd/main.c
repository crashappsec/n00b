#include "n00b.h"

static inline n00b_gopt_ctx *
setup_cmd_line(void)
{
    n00b_gopt_ctx   *gopt    = n00b_new(n00b_type_gopt_parser(),
                                   N00B_TOPLEVEL_IS_ARGV0);
    n00b_gopt_cspec *top     = n00b_new(n00b_type_gopt_command(),
                                    n00b_kw("context", n00b_ka(gopt)));
    n00b_gopt_cspec *compile = n00b_new(
        n00b_type_gopt_command(),
        n00b_kw("context",
                n00b_ka(gopt),
                "name",
                n00b_ka(n00b_new_utf8("compile")),
                "parent",
                n00b_ka(top)));

    n00b_gopt_add_subcommand(gopt, compile, n00b_new_utf8("str"));

    return gopt;
}

int
main(int argc, char **argv, char **envp)
{
    n00b_init(argc, argv, envp);
    n00b_gopt_ctx      *opt_ctx = setup_cmd_line();
    n00b_gopt_result_t *opt_res = n00b_run_getopt_raw(opt_ctx, NULL, NULL);
    n00b_utf8_t        *cmd     = n00b_gopt_get_command(opt_res);

    if (n00b_gopt_has_errors(opt_res)) {
        n00b_list_t *errs = n00b_gopt_get_errors(opt_res);
        n00b_printf("[red]error: [/] {}", n00b_list_get(errs, 0, NULL));
        n00b_getopt_show_usage(opt_ctx, cmd);
        exit(1);
    }

    n00b_list_t *args = n00b_gopt_get_args(opt_res, cmd);

    n00b_printf("[h2]Argument: [/][em]{}", n00b_list_get(args, 0, NULL));
    exit(0);
}
