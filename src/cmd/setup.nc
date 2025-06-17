#define N00B_USE_INTERNAL_API
#include "n00b.h"

const char *n00b_fl_show_cmdline_parse = "show-command-parse";
const char *n00b_fl_show_source        = "show-source";
const char *n00b_fl_show_tokens        = "show-tokens";
const char *n00b_fl_show_parse         = "show-parse";
const char *n00b_fl_show_cfg           = "show-cfg";
const char *n00b_fl_show_scopes        = "show-scopes";
const char *n00b_fl_show_deps          = "show-deps";
const char *n00b_fl_show_spec          = "show-spec";
const char *n00b_fl_show_disassembly   = "show-disassembly";
const char *n00b_fl_show_function_info = "show-function-info";
const char *n00b_fl_show_modules       = "show-modules";
const char *n00b_fl_show_all           = "show-all";
const char *n00b_fl_quiet              = "quiet";
const char *n00b_fl_verbose            = "verbose";
const char *n00b_fl_merge              = "ansi";
const char *n00b_fl_ansi               = "merge-output";
const char *n00b_fl_bright             = "bright";

const char *n00b_cmd_doc =
    "### The n00b compiler.\n\n"
    "This is the long doc, but its contents are yet to be written.\n";

static void
exit_gracefully(int64_t signal, void *, void *aux)
{
    n00b_eprintf("[|em|]Shutting down[|/|] due to signal: [|em|][|#|]",
                 n00b_get_signal_name(signal));
    n00b_exit(-1);
}

static n00b_gopt_ctx *
n00b_setup_cmd_line(void)
{
    // clang-format off
    n00b_gopt_ctx *gopt  = n00b_new(n00b_type_gopt_parser(), N00B_TOPLEVEL_IS_ARGV0);
    n00b_gopt_cspec *top = n00b_new(n00b_type_gopt_command(),
                                    context:   gopt,
			            short_doc: n00b_cstring("The n00b compiler"),
                                    long_doc:  n00b_cstring(n00b_cmd_doc));
    n00b_gopt_cspec *compile = n00b_new(n00b_type_gopt_command(),
      context:   gopt,
      name:      n00b_cstring("compile"),
      short_doc: n00b_cstring("Compiles to an object file without running it."),
      parent:    top);
    n00b_gopt_cspec *build = n00b_new(n00b_type_gopt_command(),
      context:   gopt,
      name:      n00b_cstring("build"),
      short_doc: n00b_cstring("Alias for compile."),
      parent:    top);
    n00b_gopt_cspec *run = n00b_new(n00b_type_gopt_command(),
      context:   gopt,
      name:      n00b_cstring("run"),
      short_doc: n00b_cstring("Runs a file (compiling it first if needed)."),
      parent:    top);
    n00b_gopt_cspec *record = n00b_new(n00b_type_gopt_command(),
      context:   gopt,
      name:      n00b_cstring("record"),
      short_doc: n00b_cstring("Records a command session, and generates a "
				"starting test case file."),
      parent:    top);
    n00b_gopt_cspec *play = n00b_new(n00b_type_gopt_command(),
      context:   gopt,
      name:      n00b_cstring("play"),
      short_doc: n00b_cstring("Play back a terminal capture (.cap10) file"),
      parent:    top);
    n00b_gopt_cspec *test = n00b_new(n00b_type_gopt_command(),
      context:   gopt,
      name:      n00b_cstring("test"),
      short_doc: n00b_cstring("Run existing test cases."),
      parent:    top);

    /*    n00b_gopt_cspec *test_show =*/n00b_new(n00b_type_gopt_command(),
      context:   gopt,
      name:      n00b_cstring("show"),
      short_doc: n00b_cstring("Show info about existing test cases."),
      parent:    test);

    n00b_new(n00b_type_gopt_option(),
      name:           n00b_cstring((char *)n00b_fl_show_cmdline_parse),
      linked_command: N00B_GOAT_BOOL_T_DEFAULT);
    n00b_new(n00b_type_gopt_option(),
      name:           n00b_cstring((char *)n00b_fl_show_parse),
      linked_command: N00B_GOAT_BOOL_T_DEFAULT);
    n00b_new(n00b_type_gopt_option(),
      name:           n00b_cstring((char *)n00b_fl_show_tokens),
      linked_command: N00B_GOAT_BOOL_T_DEFAULT);
    n00b_new(n00b_type_gopt_option(),
      name:           n00b_cstring((char *)n00b_fl_show_cfg),
      linked_command: N00B_GOAT_BOOL_T_DEFAULT);
    n00b_new(n00b_type_gopt_option(),
      name:           n00b_cstring((char *)n00b_fl_show_scopes),
      linked_command: N00B_GOAT_BOOL_T_DEFAULT);
    n00b_new(n00b_type_gopt_option(),
      name:           n00b_cstring((char *)n00b_fl_show_deps),
      linked_command: N00B_GOAT_BOOL_T_DEFAULT);
    n00b_new(n00b_type_gopt_option(),
      name:           n00b_cstring((char *)n00b_fl_show_spec),
      linked_command: N00B_GOAT_BOOL_T_DEFAULT);
    n00b_new(n00b_type_gopt_option(),
      name:           n00b_cstring((char *)n00b_fl_show_disassembly),
      linked_command: N00B_GOAT_BOOL_T_DEFAULT);
    n00b_new(n00b_type_gopt_option(),
      name:           n00b_cstring((char *)n00b_fl_show_all),
      linked_command: N00B_GOAT_BOOL_T_DEFAULT);
    n00b_new(n00b_type_gopt_option(),
      name:           n00b_cstring((char *)n00b_fl_ansi),
      linked_command: N00B_GOAT_BOOL_T_DEFAULT);
    n00b_new(n00b_type_gopt_option(),
      name:           n00b_cstring((char *)n00b_fl_quiet),
      linked_command: N00B_GOAT_BOOL_T_DEFAULT);
    n00b_new(n00b_type_gopt_option(),
      name:           n00b_cstring((char *)n00b_fl_verbose),
      linked_command: N00B_GOAT_BOOL_T_DEFAULT);
    n00b_new(n00b_type_gopt_option(),
      name:           n00b_cstring((char *)n00b_fl_bright),
      linked_command: N00B_GOAT_BOOL_T_DEFAULT);

    n00b_gopt_add_subcommand(gopt, compile, n00b_cstring("(str)+"));
    n00b_gopt_add_subcommand(gopt, build, n00b_cstring("(str)+"));
    n00b_gopt_add_subcommand(gopt, run, n00b_cstring("str"));
    _n00b_gopt_add_subcommand(gopt,
                              record,
                              n00b_cstring("str"),
                              n00b_cstring("Record a test case, specifing the "
                                           "test name"));
    n00b_gopt_add_subcommand(gopt, play, n00b_cstring("str"));
    n00b_new(n00b_type_gopt_option(),
         name:           n00b_cstring((char *)n00b_fl_merge),
         linked_command: record,
         opt_type:       N00B_GOAT_BOOL_F_DEFAULT);

    _n00b_gopt_add_subcommand(gopt,
                              test,
                              n00b_cstring("(str)*"),
                              n00b_cstring("Run test cases (or test case "
                                           "groups)."));

    _n00b_gopt_add_subcommand(gopt,
                              top,
                              n00b_cached_empty_string(),
                              n00b_cstring("interactive mode (tbd)"));

    return gopt;
}

static inline n00b_gopt_result_t *
parse_cmd_line(void)
{
    n00b_gopt_ctx *opt_ctx = n00b_setup_cmd_line();
    return n00b_run_getopt_raw(opt_ctx, NULL, NULL);
}

n00b_gopt_result_t *
n00b_basic_setup(int argc, char **argv, char **envp)
{
    sigset_t saved_set, cur_set;

    n00b_terminal_app_setup();

    n00b_signal_register(SIGTERM, (void *)exit_gracefully, NULL);
    n00b_signal_register(SIGSEGV, (void *)exit_gracefully, NULL);
    n00b_signal_register(SIGBUS, (void *)exit_gracefully, NULL);

    sigemptyset(&cur_set);
    sigaddset(&cur_set, SIGPIPE);
    pthread_sigmask(SIG_BLOCK, &cur_set, &saved_set);

    return parse_cmd_line();
}
