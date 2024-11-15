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
    n00b_gopt_cspec *build = n00b_new(
        n00b_type_gopt_command(),
        n00b_kw("context",
                n00b_ka(gopt),
                "name",
                n00b_ka(n00b_new_utf8("build")),
                "parent",
                n00b_ka(top)));
    n00b_gopt_cspec *run = n00b_new(
        n00b_type_gopt_command(),
        n00b_kw("context",
                n00b_ka(gopt),
                "name",
                n00b_ka(n00b_new_utf8("run")),
                "parent",
                n00b_ka(top)));

    n00b_gopt_add_subcommand(gopt, compile, n00b_new_utf8("(str)*"));
    n00b_gopt_add_subcommand(gopt, build, n00b_new_utf8("(str)*"));
    n00b_gopt_add_subcommand(gopt, run, n00b_new_utf8("str"));

    return gopt;
}

int
main(int argc, char **argv, char **envp)
{
    n00b_init(argc, argv, envp);
    n00b_install_default_styles();

#if 0
    n00b_gopt_ctx      *opt_ctx = setup_cmd_line();
    n00b_gopt_result_t *opt_res = n00b_run_getopt_raw(opt_ctx, NULL, NULL);

    n00b_print(n00b_grammar_format(opt_ctx->grammar));

    if (!opt_res) {
        exit(1);
    }
    n00b_utf8_t        *cmd     = n00b_gopt_get_command(opt_res);

    if (n00b_gopt_has_errors(opt_res)) {
        n00b_list_t *errs = n00b_gopt_get_errors(opt_res);
        n00b_printf("[red]error: [/] {}", n00b_list_get(errs, 0, NULL));
        n00b_getopt_show_usage(opt_ctx, cmd);
        exit(1);
    }

    n00b_list_t *args = n00b_gopt_get_args(opt_res, cmd);

    n00b_printf("[h2]Argument: [/][em]{}", n00b_list_get(args, 0, NULL));
#endif
    n00b_utf8_t *cmd = n00b_new_utf8("/usr/local/bin/docker");
    n00b_list_t *args = n00b_list(n00b_type_utf8());
    n00b_list_append(args, n00b_new_utf8("--version"));
    n00b_cmd_out_t *out = n00b_subproc(cmd, args);

    n00b_printf("Done running {}.", );
    n00b_printf("[h1]stdout = [/]{}", out->stdout);
    n00b_printf("[h2]stderr = [/]{}", out->stderr);
    exit(0);
}

#if 0
int
test1()
{
    char                 *cmd    = "/bin/cat";
    char                 *args[] = {"/bin/cat", "../aes.nim", 0};
    n00b_subproc_t         ctx;
    n00b_capture_result_t *result;
    struct timeval        timeout = {.tv_sec = 0, .tv_usec = 1000};

    n00b_subproc_init(&ctx, cmd, args, true);
    n00b_subproc_use_pty(&ctx);
    n00b_subproc_set_passthrough(&ctx, N00B_SP_IO_ALL, false);
    n00b_subproc_set_capture(&ctx, N00B_SP_IO_ALL, false);
    n00b_subproc_set_timeout(&ctx, &timeout);
    n00b_subproc_set_io_callback(&ctx, N00B_SP_IO_STDOUT, capture_tty_data);

    result = n00b_subproc_run(&ctx);

    while (result) {
        if (result->tag) {
            print_hex(result->contents, result->content_len, result->tag);
        }
        else {
            printf("PID: %d\n", result->pid);
            printf("Exit status: %d\n", result->exit_status);
        }
        result = result->next;
    }
    return 0;
}

int
test2()
{
    char *cmd    = "/bin/cat";
    char *args[] = {"/bin/cat", "-", 0};

    n00b_subproc_t         ctx;
    n00b_capture_result_t *result;
    struct timeval        timeout = {.tv_sec = 0, .tv_usec = 1000};

    n00b_subproc_init(&ctx, cmd, args, true);
    n00b_subproc_set_passthrough(&ctx, N00B_SP_IO_ALL, false);
    n00b_subproc_set_capture(&ctx, N00B_SP_IO_ALL, false);
    n00b_subproc_pass_to_stdin(&ctx, test_txt, strlen(test_txt), true);
    n00b_subproc_set_timeout(&ctx, &timeout);
    n00b_subproc_set_io_callback(&ctx, N00B_SP_IO_STDOUT, capture_tty_data);

    result = n00b_subproc_run(&ctx);

    while (result) {
        if (result->tag) {
            print_hex(result->contents, result->content_len, result->tag);
        }
        else {
            printf("PID: %d\n", result->pid);
            printf("Exit status: %d\n", result->exit_status);
        }
        result = result->next;
    }
    return 0;
}

int
test3()
{
    char                 *cmd    = "/usr/bin/less";
    char                 *args[] = {"/usr/bin/less", "../aes.nim", 0};
    n00b_subproc_t         ctx;
    n00b_capture_result_t *result;
    struct timeval        timeout = {.tv_sec = 0, .tv_usec = 1000};

    n00b_subproc_init(&ctx, cmd, args, true);
    n00b_subproc_use_pty(&ctx);
    n00b_subproc_set_passthrough(&ctx, N00B_SP_IO_ALL, false);
    n00b_subproc_set_capture(&ctx, N00B_SP_IO_ALL, false);
    n00b_subproc_set_timeout(&ctx, &timeout);
    n00b_subproc_set_io_callback(&ctx, N00B_SP_IO_STDOUT, capture_tty_data);

    result = n00b_subproc_run(&ctx);

    while (result) {
        if (result->tag) {
            print_hex(result->contents, result->content_len, result->tag);
        }
        else {
            printf("PID: %d\n", result->pid);
            printf("Exit status: %d\n", result->exit_status);
        }
        result = result->next;
    }
    return 0;
}

int
test4()
{
    char *cmd    = "/bin/cat";
    char *args[] = {"/bin/cat", "-", 0};

    n00b_subproc_t         ctx;
    n00b_capture_result_t *result;
    struct timeval        timeout = {.tv_sec = 0, .tv_usec = 1000};

    n00b_subproc_init(&ctx, cmd, args, true);
    n00b_subproc_use_pty(&ctx);
    n00b_subproc_set_passthrough(&ctx, N00B_SP_IO_ALL, false);
    n00b_subproc_set_capture(&ctx, N00B_SP_IO_ALL, false);
    n00b_subproc_set_timeout(&ctx, &timeout);
    n00b_subproc_set_io_callback(&ctx, N00B_SP_IO_STDOUT, capture_tty_data);

    result = n00b_subproc_run(&ctx);

    while (result) {
        if (result->tag) {
            print_hex(result->contents, result->content_len, result->tag);
        }
        else {
            printf("PID: %d\n", result->pid);
            printf("Exit status: %d\n", result->exit_status);
        }
        result = result->next;
    }
    return 0;
}

#endif
