#define N00B_USE_INTERNAL_API
#include "n00b.h"

static char *n00b_cmd_doc =
    "### The N00b compiler.\n\n"
    "This is the long doc, but its contents are yet to be written.\n";

static inline n00b_gopt_ctx *
setup_cmd_line(void)
{
    n00b_gopt_ctx   *gopt    = n00b_new(n00b_type_gopt_parser(),
                                   N00B_TOPLEVEL_IS_ARGV0);
    n00b_gopt_cspec *top     = n00b_new(n00b_type_gopt_command(),
                                    n00b_kw("context",
                                            n00b_ka(gopt),
                                            "short_doc",
                                            n00b_new_utf8("The n00b compiler"),
                                            "long_doc",
                                            n00b_new_utf8(n00b_cmd_doc)));
    n00b_gopt_cspec *compile = n00b_new(
        n00b_type_gopt_command(),
        n00b_kw("context",
                n00b_ka(gopt),
                "name",
                n00b_ka(n00b_new_utf8("compile")),
                "short_doc",
                n00b_new_utf8("Compiles to an object file without running it."),
                "parent",
                n00b_ka(top)));
    n00b_gopt_cspec *build = n00b_new(
        n00b_type_gopt_command(),
        n00b_kw("context",
                n00b_ka(gopt),
                "name",
                n00b_ka(n00b_new_utf8("build")),
                "short_doc",
                n00b_new_utf8("Alias for compile."),
                "parent",
                n00b_ka(top)));
    n00b_gopt_cspec *run = n00b_new(
        n00b_type_gopt_command(),
        n00b_kw("context",
                n00b_ka(gopt),
                "name",
                n00b_ka(n00b_new_utf8("run")),
                "short_doc",
                n00b_new_utf8("Runs a file (compiling it first if needed)."),
                "parent",
                n00b_ka(top)));

    n00b_new(n00b_type_gopt_option(),
             n00b_kw("name",
                     n00b_new_utf8("debug"),
                     "linked_command",
                     top,
                     "opt_type",
                     n00b_ka(N00B_GOAT_BOOL_T_DEFAULT)));
    n00b_new(n00b_type_gopt_option(),
             n00b_kw("name",
                     n00b_new_utf8("show-command-parse"),
                     "linked_command",
                     top,
                     "opt_type",
                     n00b_ka(N00B_GOAT_BOOL_T_DEFAULT)));

    n00b_gopt_add_subcommand(gopt, compile, n00b_new_utf8("(str)+"));
    n00b_gopt_add_subcommand(gopt, build, n00b_new_utf8("(str)+"));
    n00b_gopt_add_subcommand(gopt, run, n00b_new_utf8("str"));
    n00b_gopt_add_subcommand(gopt,
                             top,
                             n00b_new_utf8("str"));
    _n00b_gopt_add_subcommand(gopt,
                              top,
                              n00b_new_utf8(""),
                              n00b_new_utf8("interactive mode (tbd)"));

    return gopt;
}

static void
exit_gracefully(n00b_stream_t *e, int64_t signal, void *aux)
{
    n00b_printf("[em]Shutting down[/] due to signal: [em]{}",
                n00b_get_signal_name(signal));
    n00b_exit(-1);
}

static n00b_list_t *
resolve_paths(n00b_list_t *args)
{
    n00b_list_t *result = n00b_list(n00b_type_utf8());
    int          n      = n00b_list_len(args);

    for (int i = 0; i < n; i++) {
        n00b_utf8_t *s = n00b_list_get(args, i, NULL);
        s              = n00b_resolve_path(s);
        n00b_list_append(result, s);
    }

    return result;
}

void
n00b_log_dev_compile_info(n00b_compile_ctx *ctx)
{
    if (!ctx->entry_point->path) {
        return;
    }

    n00b_debug("entry module", ctx->entry_point->path);
    n00b_debug("entry source", ctx->entry_point->source);
    n00b_debug("entry tokens", n00b_format_tokens(ctx->entry_point));
    if (ctx->entry_point->ct->parse_tree) {
        n00b_debug("parse tree", n00b_format_ptree(ctx->entry_point));
    }
    if (ctx->entry_point->ct->cfg) {
        n00b_debug("module cfg", n00b_cfg_repr(ctx->entry_point->ct->cfg));
    }

    for (int j = 0; j < n00b_list_len(ctx->entry_point->fn_def_syms); j++) {
        n00b_symbol_t  *sym  = n00b_list_get(ctx->entry_point->fn_def_syms,
                                           j,
                                           NULL);
        n00b_fn_decl_t *decl = sym->value;
        n00b_debug("function", sym->name);
        n00b_debug("function type", sym->type);
        n00b_debug("function cfg", n00b_cfg_repr(decl->cfg));
        n00b_debug("function scope",
                   n00b_format_scope(decl->signature_info->fn_scope));
    }

    n00b_debug("module scope",
               n00b_format_scope(ctx->entry_point->module_scope));
    n00b_debug("global scope", n00b_format_scope(ctx->final_globals));

    n00b_debug("attribute scope", n00b_format_scope(ctx->final_attrs));

    n00b_debug("loaded modules", n00b_get_module_summary_info(ctx));
    n00b_debug("attribute spec", n00b_repr_spec(ctx->final_spec));
}

static void
compile(n00b_list_t *args, n00b_dict_t *opts)
{
}

static void
compile_and_run(n00b_list_t *args, n00b_dict_t *opts)
{
    n00b_utf8_t *s = n00b_list_get(args, 0, NULL);
    n00b_printf("[h1]Parsing module {} and its dependencies", s);
    n00b_compile_ctx *ctx        = n00b_compile_from_entry_point(s);
    n00b_grid_t      *err_output = n00b_format_errors(ctx);

    if (err_output != NULL) {
        n00b_grid_t *err_grid = n00b_new(n00b_type_grid(),
                                         n00b_kw("start_rows",
                                                 n00b_ka(2),
                                                 "start_cols",
                                                 n00b_ka(1),
                                                 "container_tag",
                                                 n00b_ka(n00b_new_utf8("flow"))));
        n00b_utf8_t *s        = n00b_new_utf8("Error Output");
        n00b_grid_add_row(err_grid,
                          n00b_to_str_renderable(s,
                                                 n00b_new_utf8("h2")));
        n00b_grid_add_row(err_grid, err_output);
        n00b_print(err_grid);
        n00b_exit(0);
    }

    n00b_printf("[h1]Generating code.");
    n00b_vm_t *vm = n00b_vm_new(ctx);
    n00b_generate_code(ctx, vm);
    n00b_printf("[h4]Beginning execution.");
    n00b_vmthread_t *thread = n00b_vmthread_new(vm);
    n00b_vmthread_run(thread);
    n00b_printf("[h4]Execution finished.");

    bool debug_on = (bool)(void *)hatrack_dict_get(opts,
                                                   n00b_new_utf8("debug"),
                                                   NULL);
    if (debug_on) {
        n00b_log_dev_compile_info(ctx);
        n00b_debug("disassembly", n00b_disasm(vm, ctx->entry_point));
    }

    n00b_exit(0);
}

int
main(int argc, char **argv, char **envp)
{
    n00b_init(argc, argv, envp);
    n00b_terminal_app_setup();

    n00b_io_register_signal_handler(SIGTERM, (void *)exit_gracefully);
    // TODO: redo this on libevent.
    /*
    n00b_io_register_signal_handler(SIGSEGV, (void *)exit_crash_handler);
    n00b_io_register_signal_handler(SIGBUS, (void *)exit_crash_handler);
    */

    n00b_eprintf(
        "[em]N00b {}.{}.{}[/] [i]({}, {})[/]\n"
        "© 2024-2025 Crash Override, Inc.\n"
        "Licensed under the BSD-3-Clause license.\n",
        n00b_box_u64(N00B_VERS_MAJOR),
        n00b_box_u64(N00B_VERS_MINOR),
        n00b_box_u64(N00B_VERS_PATCH),
        n00b_compile_date(),
        n00b_compile_time());
    n00b_enable_debugging();
    n00b_ignore_uncaught_io_errors();

    sigset_t saved_set, cur_set;

    sigemptyset(&cur_set);
    sigaddset(&cur_set, SIGPIPE);
    pthread_sigmask(SIG_BLOCK, &cur_set, &saved_set);

    n00b_gopt_ctx      *opt_ctx = setup_cmd_line();
    n00b_gopt_result_t *opt_res = n00b_run_getopt_raw(opt_ctx, NULL, NULL);

    if (!opt_res) {
        n00b_exit(-1);
    }

    n00b_utf8_t *cmd  = n00b_gopt_get_command(opt_res);
    n00b_list_t *args = resolve_paths(n00b_gopt_get_args(opt_res, cmd));
    n00b_dict_t *opts = n00b_gopt_get_flags(opt_res);

    if (hatrack_dict_get(opts, n00b_new_utf8("show-command-parse"), NULL)) {
        n00b_debug("n00b_parsed_cmd", cmd);
        n00b_debug("n00b_parsed_args", args);
        n00b_debug("n00b_cmd_parse_tree",
                   n00b_parse_tree_format(opt_res->tree));
        n00b_debug("n00b_cmd_options", opts);
    }

    if (n00b_str_eq(cmd, n00b_new_utf8("compile"))) {
        compile(args, opts);
    }
    if (n00b_str_eq(cmd, n00b_new_utf8("build"))) {
        compile(args, opts);
    }
    if (n00b_str_eq(cmd, n00b_new_utf8("run"))) {
        compile_and_run(args, opts);
    }
    if (n00b_str_eq(cmd, n00b_new_utf8(""))) {
        compile_and_run(args, opts);
    }
    n00b_exit(0);
}
