#define N00B_USE_INTERNAL_API
#include "n00b/test_harness.h"

static inline void
no_input()
{
    n00b_printf("[red b]NO VALID INPUT FILES.[/] Exiting.");
    exit(-1);
}

void
n00b_show_err_diffs(n00b_utf8_t *fname, n00b_list_t *expected, n00b_list_t *actual)
{
    n00b_compile_error_t err;
    n00b_utf8_t         *errstr;

    n00b_printf("[red]FAIL[/]: test [i]{}[/]: error mismatch.", fname);

    if (!expected || n00b_list_len(expected) == 0) {
        n00b_printf("[h1]Expected no errors.");
    }
    else {
        n00b_printf("[h1]Expected errors:");

        int n = n00b_list_len(expected);

        for (int i = 0; i < n; i++) {
            errstr = n00b_list_get(expected, i, NULL);
            n00b_printf("[b]{}:[/] [em]{}", n00b_box_u64(i + 1), errstr);
        }
    }

    if (!actual || n00b_list_len(actual) == 0) {
        n00b_printf("[h2]Got no errors.");
    }
    else {
        n00b_printf("[h2]Actual errors:");

        int n = n00b_list_len(actual);

        for (int i = 0; i < n; i++) {
            uint64_t u64     = (uint64_t)n00b_list_get(actual, i, NULL);
            err              = (n00b_compile_error_t)u64;
            n00b_utf8_t *code = n00b_err_code_to_str(err);
            n00b_printf("[b]{}:[/] [em]{}", n00b_box_u64(i + 1), code);
        }
    }
}

void
n00b_show_dev_compile_info(n00b_compile_ctx *ctx)
{
    if (!ctx->entry_point->path) {
        return;
    }

    n00b_printf("[h2]Module Source Code for {}", ctx->entry_point->path);
    n00b_print(ctx->entry_point->source);
    n00b_printf("[h2]Module Tokens for {}", ctx->entry_point->path);
    n00b_print(n00b_format_tokens(ctx->entry_point));
    if (ctx->entry_point->ct->parse_tree) {
        n00b_print(n00b_format_ptree(ctx->entry_point));
    }
    if (ctx->entry_point->ct->cfg) {
        n00b_printf("[h1]Toplevel CFG for {}", ctx->entry_point->path);
        n00b_print(n00b_cfg_repr(ctx->entry_point->ct->cfg));
    }

    for (int j = 0; j < n00b_list_len(ctx->entry_point->fn_def_syms); j++) {
        n00b_symbol_t  *sym  = n00b_list_get(ctx->entry_point->fn_def_syms,
                                         j,
                                         NULL);
        n00b_fn_decl_t *decl = sym->value;
        n00b_printf("[h1]CFG for Function {}{}", sym->name, sym->type);
        n00b_print(n00b_cfg_repr(decl->cfg));
        n00b_printf("[h2]Function Scope for {}{}", sym->name, sym->type);
        n00b_print(n00b_format_scope(decl->signature_info->fn_scope));
    }

    n00b_printf("[h2]Module Scope");
    n00b_print(n00b_format_scope(ctx->entry_point->module_scope));
    n00b_printf("[h2]Global Scope");
    n00b_print(n00b_format_scope(ctx->final_globals));
    n00b_printf("[h2]Attribute Scope");
    n00b_print(n00b_format_scope(ctx->final_attrs));

    n00b_printf("[h2]Loaded Modules");
    n00b_print(n00b_get_module_summary_info(ctx));
    n00b_print(n00b_repr_spec(ctx->final_spec));
}

void
n00b_show_dev_disasm(n00b_vm_t *vm, n00b_module_t *m)
{
    n00b_grid_t *g = n00b_disasm(vm, m);
    n00b_print(g);
    n00b_printf("Module [em]{}[/] disassembly done.", m->path);
}

static void
show_gridview(void)
{
    n00b_grid_t *success_grid = n00b_new(
        n00b_type_grid(),
        n00b_kw("start_cols",
               n00b_ka(2),
               "header_rows",
               n00b_ka(1),
               "container_tag",
               n00b_ka(n00b_new_utf8("error_grid"))));
    n00b_grid_t *fail_grid = n00b_new(
        n00b_type_grid(),
        n00b_kw("start_cols",
               n00b_ka(2),
               "header_rows",
               n00b_ka(1),
               "container_tag",
               n00b_ka(n00b_new_utf8("error_grid"))));

    n00b_list_t *row = n00b_list(n00b_type_utf8());

    n00b_list_append(row, n00b_new_utf8("Test #"));
    n00b_list_append(row, n00b_new_utf8("Test File"));

    n00b_grid_add_row(success_grid, row);
    n00b_grid_add_row(fail_grid, row);

    n00b_set_column_style(success_grid, 0, n00b_new_utf8("full_snap"));
    n00b_set_column_style(success_grid, 1, n00b_new_utf8("snap"));
    n00b_set_column_style(fail_grid, 0, n00b_new_utf8("full_snap"));
    n00b_set_column_style(fail_grid, 1, n00b_new_utf8("snap"));

    for (int i = 0; i < n00b_test_total_items; i++) {
        n00b_test_kat *item = &n00b_test_info[i];

        if (!item->is_test) {
            continue;
        }

        row = n00b_list(n00b_type_utf8());

        n00b_utf8_t *num = n00b_cstr_format("[em]{}",
                                          n00b_box_u64(item->case_number));
        n00b_list_append(row, num);
        n00b_list_append(row, item->path);

        if (item->run_ok && !item->exit_code) {
            n00b_grid_add_row(success_grid, row);
        }
        else {
            n00b_grid_add_row(fail_grid, row);
        }
    }

    n00b_printf("[h5]Passed Tests:[/]");
    n00b_print(success_grid);

    n00b_printf("[h4]Failed Tests:[/]");
    n00b_print(fail_grid);
}

static int
possibly_warn()
{
    if (!n00b_give_malformed_warning) {
        return 0;
    }

    int result = 0;

    n00b_grid_t *oops_grid = n00b_new(
        n00b_type_grid(),
        n00b_kw("start_cols",
               n00b_ka(1),
               "header_rows",
               n00b_ka(0),
               "container_tag",
               n00b_ka(n00b_new_utf8("error_grid"))));

    for (int i = 0; i < n00b_test_total_items; i++) {
        n00b_test_kat *item = &n00b_test_info[i];
        if (!item->is_malformed) {
            continue;
        }

        result++;

        n00b_list_t *row = n00b_list(n00b_type_utf8());

        n00b_list_append(row, item->path);
        n00b_grid_add_row(oops_grid, row);
    }

    n00b_printf("\[yellow]warning:[/] Bad test case format for file(s):");
    n00b_print(oops_grid);
    n00b_printf(
        "The second doc string may have 0 or 1 [em]$output[/] "
        "sections and 0 or 1 [em]$errors[/] sections ONLY.");
    n00b_printf(
        "If neither are provided, then the harness expects no "
        "errors and ignores output. There may be nothing else "
        "in the doc string except whitespace.");
    n00b_printf(
        "Also, instead of [em]$output[/] you may add a [em]$hex[/] "
        "section, where the contents must be raw hex bytes.");
    n00b_printf(
        "\n[i inv]Note: If you want to explicitly test for no output, then "
        "provide `$output:` with nothing following.");

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
        n00b_printf("[red b]Failed ALL TESTS.");
        exit(n00b_test_total_tests + 1);
    }

    n00b_printf("Passed [em]{}[/] out of [em]{}[/] run tests.",
               n00b_box_u64(n00b_test_number_passed),
               n00b_box_u64(n00b_test_total_tests));

    if (n00b_test_number_failed != 0) {
        show_gridview();
        n00b_printf("[h5] N00b testing [b red]FAILED![/]");
        exit(n00b_test_number_failed);
    }

    n00b_printf("[h5] N00b testing [b navy blue]PASSED.[/]");

    exit(0);
}
