#define N00B_USE_INTERNAL_API
#include "n00b.h"
#include "n00b/cmd.h"

void
n00b_record_testcase(n00b_cmdline_ctx *ctx)
{
    n00b_string_t *root = n00b_n00b_root();
    n00b_string_t *pwd  = n00b_get_current_directory();
    n00b_string_t *tdir = n00b_path_simple_join(root, n00b_cstring("tests"));
    n00b_string_t *gdir;

    if (!n00b_path_is_directory(tdir)) {
        if (mkdir(tdir->data, 00744)) {
            n00b_eprintf(
                "Could not find a [|em|]tests[|/|] directory under "
                "$N00B_ROOT ([|em|][|#|][|/|]",
                root);
            n00b_exit(-1);
        }
    }

    n00b_string_t *group;
    n00b_string_t *case_name;
    n00b_string_t *passed_path;
    n00b_string_t *check_path;

    if (n00b_list_len(ctx->args) == 1) {
        group = n00b_cstring("default");
    }
    else {
        group = n00b_list_get(ctx->args, 1, NULL);
    }
    case_name = n00b_list_get(ctx->args, 0, NULL);

    gdir = n00b_path_simple_join(tdir, group);

    if (!n00b_path_is_directory(gdir)) {
        n00b_eprintf("[|i|]Creating directory: [|em|][|#|]", gdir);
        if (mkdir(gdir->data, 00744)) {
            n00b_eprintf(
                "Could not set up the group directory [|em|][|#|][|/|] under "
                "[|em|]$N00B_ROOT/tests[|/|].",
                group);
            ctx->exit_code = -1;
            return;
        }
    }

    passed_path = n00b_path_simple_join(gdir, case_name);
    check_path  = n00b_cformat("[|#|].cap10", passed_path);

    if (n00b_path_exists(check_path)) {
        n00b_eprintf(
            "Capture file for [|em|][|#|]/[|#|][|/|] already exists "
            " at [|#|]",
            group,
            case_name,
            check_path);
        ctx->exit_code = -1;
        return;
    }

    check_path = n00b_cformat("[|#|].test", passed_path);

    if (n00b_path_exists(check_path)) {
        n00b_eprintf(
            "Capture file for [|em|][|#|]/[|#|][|/|] already exists "
            " at [|#|]",
            group,
            case_name,
            check_path);
        ctx->exit_code = -1;
        return;
    }

    n00b_set_current_directory(gdir);
    n00b_eprintf("[|em4|]Recording interactive shell.");
    n00b_testgen_record(passed_path,
                        true,
                        n00b_cmd_ansi(ctx),
                        n00b_cmd_merge(ctx));
    n00b_eprintf("[|em4|]Recording complete.");
    n00b_eprintf("[|em4|]Test script saved to: [|#|].", check_path);
    n00b_set_current_directory(pwd);
}
