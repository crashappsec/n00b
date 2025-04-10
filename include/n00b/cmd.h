#pragma once
#include "n00b.h"

extern const char *n00b_fl_show_cmdline_parse;
extern const char *n00b_fl_show_source;
extern const char *n00b_fl_show_tokens;
extern const char *n00b_fl_show_parse;
extern const char *n00b_fl_show_cfg;
extern const char *n00b_fl_show_scopes;
extern const char *n00b_fl_show_deps;
extern const char *n00b_fl_show_spec;
extern const char *n00b_fl_show_disassembly;
extern const char *n00b_fl_show_function_info;
extern const char *n00b_fl_show_modules;
extern const char *n00b_fl_show_all;
extern const char *n00b_fl_quiet;
extern const char *n00b_fl_verbose;
extern const char *n00b_fl_ansi;
extern const char *n00b_fl_merge;

#define N00B_CMD_RUN       1
#define N00B_CMD_COMPILE   2
#define N00B_CMD_REPL      3
#define N00B_CMD_RECORD    4
#define N00B_CMD_TEST      5
#define N00B_CMD_TEST_SHOW 6

#define N00B_COMPILE_FAILED -1
#define N00B_RUN_FAILED     -2
#define N00B_NOT_DONE       -99

typedef struct {
    n00b_string_t    *cmd;
    n00b_list_t      *args;
    n00b_dict_t      *opts;
    n00b_compile_ctx *cctx;
    n00b_tree_node_t *cmd_parse;
    n00b_vm_t        *vm;
    int               exit_code;
} n00b_cmdline_ctx;

extern n00b_gopt_result_t *n00b_basic_setup(int, char **, char **);
extern void                n00b_show_compiler_debug_info(n00b_cmdline_ctx *);
extern void                n00b_show_cmdline_debug_info(n00b_cmdline_ctx *);
extern void                n00b_compile_and_run(n00b_cmdline_ctx *);
extern void                n00b_compile(n00b_cmdline_ctx *, bool);
extern void                n00b_record_testcase(n00b_cmdline_ctx *);
extern void                n00b_run_tests(n00b_cmdline_ctx *);
extern void                n00b_show_tests(n00b_cmdline_ctx *);

static inline bool
n00b_cmd_merge(n00b_cmdline_ctx *ctx)
{
    if (!ctx->opts) {
        return true;
    }

    bool found  = false;
    bool result = hatrack_dict_get(ctx->opts,
                                   n00b_cstring(n00b_fl_merge),
                                   &result);

    if (!found) {
        return true;
    }

    return result;
}

static inline bool
n00b_cmd_quiet(n00b_cmdline_ctx *ctx)
{
    if (!ctx->opts) {
        return false;
    }

    return hatrack_dict_get(ctx->opts, n00b_cstring(n00b_fl_quiet), NULL);
}

static inline bool
n00b_cmd_verbose(n00b_cmdline_ctx *ctx)
{
    if (!ctx->opts) {
        return false;
    }

    return hatrack_dict_get(ctx->opts, n00b_cstring(n00b_fl_verbose), NULL);
}

static inline bool
n00b_cmd_ansi(n00b_cmdline_ctx *ctx)
{
    if (!ctx->opts) {
        return false;
    }
    return hatrack_dict_get(ctx->opts, n00b_cstring(n00b_fl_ansi), NULL);
}

static inline bool
n00b_cmd_show_cmdline_parse(n00b_cmdline_ctx *ctx)
{
    return hatrack_dict_get(ctx->opts,
                            n00b_cstring(n00b_fl_show_cmdline_parse),
                            NULL);
}

static inline bool
n00b_cmd_show_all(n00b_cmdline_ctx *ctx)
{
    return hatrack_dict_get(ctx->opts, n00b_cstring(n00b_fl_show_all), NULL);
}
