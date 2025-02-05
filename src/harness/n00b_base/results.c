#define N00B_USE_INTERNAL_API
#include "n00b/test_harness.h"

static inline void
no_input()
{
    n00b_printf("«red»«b»NO VALID INPUT FILES.«/» Exiting.");
    exit(-1);
}

void
n00b_show_err_diffs(n00b_string_t *fname, n00b_list_t *expected, n00b_list_t *actual)
{
    n00b_compile_error_t err;
    n00b_string_t       *errstr;

    n00b_printf("«red»FAIL«/»: test «em»«#»«/»: error mismatch.", fname);

    if (!expected || n00b_list_len(expected) == 0) {
        n00b_printf("«em1»Expected no errors.");
    }
    else {
        n00b_printf("«em1»Expected errors:");

        int n = n00b_list_len(expected);

        for (int i = 0; i < n; i++) {
            errstr = n00b_list_get(expected, i, NULL);
            n00b_printf("«b»«#»:«/» «em2»«#»", (int64_t)(i + 1), errstr);
        }
    }

    if (!actual || n00b_list_len(actual) == 0) {
        n00b_printf("«em2»Got no errors.");
    }
    else {
        n00b_printf("«em2»Actual errors:");

        int n = n00b_list_len(actual);

        for (int i = 0; i < n; i++) {
            uint64_t u64        = (uint64_t)n00b_list_get(actual, i, NULL);
            err                 = (n00b_compile_error_t)u64;
            n00b_string_t *code = n00b_err_code_to_str(err);
            n00b_printf("«b»«#:i»:«/» «em2»«#»", (int64_t)(i + 1), code);
        }
    }
}

void
n00b_show_dev_compile_info(n00b_compile_ctx *ctx)
{
    if (!ctx->entry_point->path) {
        return;
    }

    n00b_printf("«em2»Module Source Code for «#»", ctx->entry_point->path);
    n00b_print(ctx->entry_point->source);
    n00b_printf("«em2»Module Tokens for «#»", ctx->entry_point->path);
    n00b_print(n00b_format_tokens(ctx->entry_point));
    if (ctx->entry_point->ct->parse_tree) {
        n00b_print_parse_node(ctx->entry_point->ct->parse_tree);
    }
    if (ctx->entry_point->ct->cfg) {
        n00b_printf("«em1»Toplevel CFG for «#»", ctx->entry_point->path);
        n00b_print(n00b_cfg_repr(ctx->entry_point->ct->cfg));
    }

    for (int j = 0; j < n00b_list_len(ctx->entry_point->fn_def_syms); j++) {
        n00b_symbol_t  *sym  = n00b_list_get(ctx->entry_point->fn_def_syms,
                                           j,
                                           NULL);
        n00b_fn_decl_t *decl = sym->value;
        n00b_printf("«em1»CFG for Function «#»«#»", sym->name, sym->type);
        n00b_print(n00b_cfg_repr(decl->cfg));
        n00b_printf("«em2»Function Scope for «#»«#»", sym->name, sym->type);
        n00b_print(n00b_format_scope(decl->signature_info->fn_scope));
    }

    n00b_printf("«em2»Module Scope");
    n00b_print(n00b_format_scope(ctx->entry_point->module_scope));
    n00b_printf("«em2»Global Scope");
    n00b_print(n00b_format_scope(ctx->final_globals));
    n00b_printf("«em2»Attribute Scope");
    n00b_print(n00b_format_scope(ctx->final_attrs));

    n00b_printf("«em2»Loaded Modules");
    n00b_print(n00b_get_module_summary_info(ctx));
    n00b_print(n00b_repr_spec(ctx->final_spec));
}

void
n00b_show_dev_disasm(n00b_vm_t *vm, n00b_module_t *m)
{
    n00b_table_t *g = n00b_disasm(vm, m);
    n00b_eprint(g);
    n00b_eprintf("Module «em2»«#»«/» disassembly done.", m->path);
}

static void
show_pass_fail(void)
{
    n00b_table_t  *success_list = n00b_table("columns",
                                            n00b_ka(2),
                                            "outstream",
                                            n00b_stderr(),
                                            "title",
                                            n00b_cstring("Passed Tests"));
    n00b_table_t  *fail_list    = n00b_table("columns",
                                         n00b_ka(2),
                                         "outstream",
                                         n00b_stderr(),
                                         "title",
                                         n00b_cstring("Passed Tests"));
    n00b_list_t   *row          = n00b_list(n00b_type_string());
    n00b_string_t *s;

    n00b_list_append(row, n00b_cstring("Test #"));
    n00b_list_append(row, n00b_cstring("Test File"));

    n00b_table_add_row(success_list, row);
    n00b_table_add_row(fail_list, row);

    for (int i = 0; i < n00b_test_total_items; i++) {
        n00b_test_kat *item = &n00b_test_info[i];
        row                 = n00b_list(n00b_type_string());

        if (!item->is_test) {
            continue;
        }

        s = n00b_cformat("«em2»«#»", (uint64_t)item->case_number);
        n00b_list_append(row, s);
        n00b_list_append(row, item->path);

        if (item->run_ok && !item->exit_code) {
            n00b_table_add_row(success_list, row);
        }
        else {
            n00b_table_add_row(fail_list, row);
        }
    }

    n00b_table_end(success_list);
    n00b_table_end(fail_list);
}

static int
possibly_warn(void)
{
    if (!n00b_give_malformed_warning) {
        return 0;
    }

    int            result = 0;
    n00b_string_t *s      = n00b_cstring(
        "«yellow»warning:«/» Bad test case format for file(s):");

    n00b_table_t *oopsies = n00b_table("columns",
                                       n00b_ka(1),
                                       "style",
                                       n00b_ka(N00B_TABLE_SIMPLE),
                                       "title",
                                       s,
                                       "outstream",
                                       n00b_stderr());

    for (int i = 0; i < n00b_test_total_items; i++) {
        n00b_test_kat *item = &n00b_test_info[i];
        if (!item->is_malformed) {
            continue;
        }

        result++;

        n00b_table_add_cell(oopsies, item->path);
    }

    n00b_table_add_cell(oopsies,
                        n00b_crich("The second doc string may have 0 or 1 "
                                   "«em2»$output«/» sections and 0 or 1 "
                                   "«em2»$errors«/» sections ONLY."));
    n00b_table_add_cell(oopsies,
                        n00b_crich("If neither are provided, then the harness "
                                   "expects no errors and ignores output. "
                                   "There may be nothing else "
                                   "in the doc string except whitespace."));
    n00b_table_add_cell(oopsies,
                        n00b_crich("Also, instead of «em2»$output«/» you may "
                                   "add a «em2»$hex«/» section, where the "
                                   "contents must be raw hex bytes."));
    n00b_table_add_cell(oopsies,
                        n00b_crich("«i»«rev»Note: If you want to explicitly "
                                   "test for no output, then provide "
                                   "`$output:` with nothing following."));

    return result;
}

void
n00b_report_results_and_exit(void)
{
    if (!n00b_test_total_items) {
        no_input();
    }

    int malformed_items = possibly_warn();

    if (n00b_test_total_tests == 0) {
        if (n00b_test_total_items == malformed_items) {
            no_input();
        }

        exit(0);
    }

    if (n00b_test_number_passed == 0) {
        n00b_printf("«red»«b»Failed ALL TESTS.");
        exit(n00b_test_total_tests + 1);
    }

    n00b_eprintf("Passed «em2»«#»«/» out of «em2»«#»«/» run tests.",
                 (int64_t)n00b_test_number_passed,
                 (int64_t)n00b_test_total_tests);

    if (n00b_test_number_failed != 0) {
        show_pass_fail();
        n00b_printf("«em5» N00b testing «b»«red»FAILED!«/»");
        exit(n00b_test_number_failed);
    }

    n00b_printf("«em5» N00b testing «b»«green»PASSED.«/»");

    exit(0);
}
