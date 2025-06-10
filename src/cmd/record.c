#define N00B_USE_INTERNAL_API
#include "n00b.h"
#include "n00b/cmd.h"

void
n00b_record_testcase(n00b_cmdline_ctx *ctx)
{
    n00b_string_t *arg      = n00b_list_get(ctx->args, 0, NULL);
    n00b_string_t *root     = n00b_resolve_path(n00b_n00b_root());
    n00b_string_t *pwd      = n00b_get_current_directory();
    n00b_string_t *work_dir = NULL;
    n00b_string_t *group    = n00b_cached_empty_string();
    n00b_string_t *case_name;
    n00b_string_t *s;

    root = n00b_path_simple_join(root, n00b_cstring("tests"));

    int ix = n00b_string_rfind(arg, n00b_cached_slash());

    // If there's no slash, assume they want CWD for the output, not
    // a test directory.

    if (ix == -1) {
        case_name = arg;
        work_dir  = pwd;
    }
    else {
        case_name = n00b_string_slice(arg, ix + 1, -1);

        if (ix != 0) {
            s = n00b_string_slice(arg, 0, ix);

            // If they provide an abs path, either it's under the test
            // directory, or it's not. If it is, we use that to determine
            // the group name, otherwise we leave the group name empty.
            if (s->data[0] == '/') {
                work_dir = n00b_resolve_path(s);

                if (n00b_string_starts_with(work_dir, root)) {
                    group = n00b_string_slice(work_dir, root->codepoints, -1);
                    n00b_path_strip_slashes_both_ends(group);
                }
            }
            else {
                // Relative path was a group spec.
                group    = s;
                work_dir = n00b_path_simple_join(root, s);
            }
        }
    }

    if (!n00b_path_is_directory(work_dir)) {
        if (mkdir(work_dir->data, 00744)) {
            n00b_eprintf(
                "Could not create the directory [=em=][=#=][=/=] to save "
                "the test file.",
                work_dir);
            ctx->exit_code = -1;
            return;
        }
    }

    s = n00b_path_chop_extension(case_name);

    if (n00b_string_codepoint_len(s)) {
        if (!n00b_string_eq(s, n00b_cstring(".cap10"))
            && !n00b_string_eq(s, n00b_cstring(".test"))) {
            n00b_eprintf("Bad file extension: [=em=][=#=][=/=]", s);
            n00b_eprintf("Must be [=i=].test .cap10,[=/=] or omitted");
            ctx->exit_code = -1;
            return;
        }
    }

    n00b_string_t *passed_path;
    n00b_string_t *check_path;

    passed_path = n00b_path_simple_join(work_dir, case_name);
    check_path  = n00b_cformat("[=#=].cap10", passed_path);

    if (n00b_path_exists(check_path)) {
        n00b_eprintf(
            "Capture file for [=em=][=#=]/[=#=][=/=] already exists "
            " at [=#=]",
            group,
            case_name,
            check_path);
        ctx->exit_code = -1;
        return;
    }

    check_path = n00b_cformat("[=#=].test", passed_path);

    if (n00b_path_exists(check_path)) {
        n00b_eprintf(
            "Capture file for [=em=][=#=]/[=#=][=/=] already exists "
            " at [=#=]",
            group,
            case_name,
            check_path);
        ctx->exit_code = -1;
        return;
    }

    n00b_set_current_directory(work_dir);
    n00b_eprintf("[=em4=]Recording interactive shell.");
    if (n00b_cmd_verbose(ctx)) {
        n00b_eprintf("Path is:[=em2=][=#=]", passed_path);
    }

    n00b_testgen_record(passed_path,
                        true,
                        n00b_cmd_ansi(ctx),
                        n00b_cmd_merge(ctx));
    n00b_eprintf("[=em4=]Recording complete.");
    n00b_eprintf("[=em4=]Test script saved to: [=#=].", check_path);
    n00b_set_current_directory(pwd);
}
