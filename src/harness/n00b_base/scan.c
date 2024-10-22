// Scan for test files, and populate the test matrix, including
// expected results.

#define N00B_USE_INTERNAL_API
#include "n00b/test_harness.h"

#define kat_is_bad(x)                  \
    n00b_give_malformed_warning = true; \
    x->is_malformed            = true; \
    return

static n00b_utf8_t *
process_hex(n00b_utf32_t *s)
{
    n00b_utf8_t *res = n00b_to_utf8(s);

    int n = 0;

    for (int i = 0; i < res->byte_len; i++) {
        char c = res->data[i];

        switch (c) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        case 'a':
        case 'b':
        case 'c':
        case 'd':
        case 'e':
        case 'f':
            res->data[n++] = c;
            continue;
        case 'A':
            res->data[n++] = 'a';
            continue;
        case 'B':
            res->data[n++] = 'b';
            continue;
        case 'C':
            res->data[n++] = 'c';
            continue;
        case 'D':
            res->data[n++] = 'd';
            continue;
        case 'E':
            res->data[n++] = 'e';
            continue;
        case 'F':
            res->data[n++] = 'f';
            continue;
        default:
            continue;
        }
    }
    res->data[n]    = 0;
    res->byte_len   = n;
    res->codepoints = n;

    return res;
}

static inline bool
add_or_warn(n00b_list_t *flist, n00b_utf8_t *s, n00b_utf8_t *ext)
{
    s = n00b_to_utf8(s);
    if (n00b_str_ends_with(s, ext)) {
        n00b_list_append(flist, s);
        return true;
    }
    else {
        n00b_list_t *parts = n00b_str_split(s, n00b_new_utf8("/"));
        int         n     = n00b_list_len(parts);
        n00b_utf8_t *file  = n00b_to_utf8(n00b_list_get(parts, n - 1, NULL));
        if (!n00b_str_starts_with(file, n00b_new_utf8("."))) {
            n00b_printf(
                "[yellow]warning:[/] Skipping file w/o [em].n00b[/]"
                " file extension: {}",
                s);
        }
        return false;
    }
}

static void
extract_output(n00b_test_kat *kat,
               int64_t       start,
               int64_t       end)
{
    n00b_str_t *s         = n00b_str_slice(kat->raw_docstring, start, end);
    s                    = n00b_str_strip(s);
    s                    = n00b_to_utf8(s);
    kat->expected_output = kat->is_hex ? process_hex(s) : s;
}

static void
extract_errors(n00b_test_kat *kat, int64_t start, int64_t end)
{
    n00b_utf32_t *s       = n00b_str_slice(kat->raw_docstring, start, end);
    kat->expected_errors = n00b_list(n00b_type_utf8());
    n00b_list_t *split    = n00b_str_split(s, n00b_new_utf8("\n"));
    int         l        = n00b_list_len(split);

    for (int i = 0; i < l; i++) {
        s = n00b_str_strip(n00b_list_get(split, i, NULL));

        if (!n00b_str_codepoint_len(s)) {
            continue;
        }

        n00b_list_append(kat->expected_errors, n00b_to_utf8(s));
    }
}

static void
parse_vm_instructions(n00b_test_kat *kat, n00b_utf32_t *line)
{
    kat->save = true;
    line      = n00b_str_strip(line);
    int n     = n00b_str_codepoint_len(line);

    if (n == 1) {
        return;
    }

    line   = n00b_str_slice(line, 1, n--);
    int ix = n00b_str_find(line, n00b_utf32_repeat(':', 1));

    if (ix != -1) {
        kat->second_entry = n00b_str_strip(n00b_str_slice(line, 0, ix));
        kat->second_entry = n00b_to_utf8(kat->second_entry);
        line              = n00b_str_strip(n00b_str_slice(line, ix + 1, n));
        line              = n00b_to_utf8(line);

        bool                neg;
        n00b_compile_error_t err;

        __uint128_t num_exes = n00b_raw_int_parse(line, &err, &neg);

        if (neg || err != n00b_err_no_error || num_exes > 1 << 16) {
            N00B_CRAISE("Invalid numerical value for # of executions.");
        }
        else {
            kat->second_entry_executions = (int)num_exes;
        }
    }
    else {
        kat->second_entry = n00b_to_utf8(line);
    }
}

static void
try_to_load_kat(n00b_test_kat *kat)
{
    int64_t len = n00b_str_codepoint_len(kat->raw_docstring);
    int64_t nl  = n00b_str_find(kat->raw_docstring, n00b_new_utf8("\n"));

    if (nl != 0) {
        n00b_str_t *line    = n00b_str_slice(kat->raw_docstring, 0, nl);
        kat->raw_docstring = n00b_str_slice(kat->raw_docstring, nl, len);

        if (line->data[0] == '#') {
            parse_vm_instructions(kat, line);
        }
    }

    kat->raw_docstring = n00b_to_utf32(n00b_str_strip(kat->raw_docstring));

    n00b_utf8_t *output = n00b_new_utf8("$output:");
    n00b_utf8_t *errors = n00b_new_utf8("$errors:");
    n00b_utf8_t *hex    = n00b_new_utf8("$hex:");
    int64_t     outix  = n00b_str_find(kat->raw_docstring, output);
    int64_t     errix  = n00b_str_find(kat->raw_docstring, errors);
    int64_t     hexix  = n00b_str_find(kat->raw_docstring, hex);

    if (hexix != -1) {
        kat->is_hex = true;
        outix       = hexix;
    }

    if (outix == -1 && errix == -1) {
        if (n00b_str_codepoint_len(kat->raw_docstring) != 0) {
            kat_is_bad(kat);
        }
        return;
    }

    if (outix == -1) {
        if (errix != 0) {
            kat_is_bad(kat);
        }

        extract_errors(kat, 9, n00b_str_codepoint_len(kat->raw_docstring));
        kat->ignore_output = true;
        return;
    }

    if (errix == -1) {
        if (outix != 0) {
            kat_is_bad(kat);
        }
        extract_output(kat, 9, n00b_str_codepoint_len(kat->raw_docstring));
        return;
    }

    if (outix != 0 && errix != 0) {
        kat_is_bad(kat);
    }

    if (errix != 0) {
        extract_output(kat, 9, errix);
        extract_errors(kat,
                       errix + 9,
                       n00b_str_codepoint_len(kat->raw_docstring));
    }
    else {
        extract_errors(kat, 9, outix);
        extract_output(kat,
                       outix + 9,
                       n00b_str_codepoint_len(kat->raw_docstring));
    }
}

static int
fname_sort(const n00b_utf8_t **s1, const n00b_utf8_t **s2)
{
    return strcmp((*s1)->data, (*s2)->data);
}

static bool
find_docstring(n00b_test_kat *kat)
{
    n00b_compile_ctx *ctx = n00b_new_compile_context(NULL);
    n00b_module_t    *m   = n00b_init_module_from_loc(ctx, kat->path);

    if (!m || !m->ct->tokens) {
        return false;
    }

    bool have_doc1 = false;
    int  l         = n00b_list_len(m->ct->tokens);

    for (int i = 0; i < l; i++) {
        n00b_token_t *t = n00b_list_get(m->ct->tokens, i, NULL);

        switch (t->kind) {
        case n00b_tt_space:
        case n00b_tt_newline:
        case n00b_tt_line_comment:
        case n00b_tt_long_comment:
            continue;
        case n00b_tt_string_lit:
            if (!have_doc1) {
                have_doc1 = true;
                continue;
            }
            kat->raw_docstring = n00b_token_raw_content(t);
            kat->is_test       = true;
            kat->case_number   = ++n00b_test_total_tests;
            return true;
        default:
            ++n00b_non_tests;
            return false;
        }
    }

    return false;
}

static n00b_list_t *
identify_test_files(void)
{
    n00b_list_t *argv      = n00b_get_program_arguments();
    int         n         = n00b_list_len(argv);
    n00b_list_t *to_load   = n00b_list(n00b_type_utf8());
    n00b_utf8_t *test_dir  = n00b_get_env(n00b_new_utf8("N00B_TEST_DIR"));
    n00b_utf8_t *cur_dir   = n00b_get_current_directory();
    n00b_utf8_t *ext       = n00b_new_utf8(".n00b");
    n00b_list_t *all_files = n00b_list(n00b_type_utf8());

    if (test_dir == NULL) {
        test_dir = n00b_cstr_format("{}/tests/", n00b_n00b_root());
    }
    else {
        test_dir = n00b_resolve_path(test_dir);
    }

    n00b_list_append(n00b_path, test_dir);

    if (!n) {
        n    = 1;
        argv = n00b_list(n00b_type_utf8());
        n00b_list_append(argv, test_dir);
    }

    for (int i = 0; i < n; i++) {
        n00b_utf8_t *fname = n00b_to_utf8(n00b_list_get(argv, i, NULL));
one_retry:;
        n00b_utf8_t *s = n00b_path_simple_join(test_dir, fname);

        switch (n00b_get_file_kind(s)) {
        case N00B_FK_IS_REG_FILE:
        case N00B_FK_IS_FLINK:
            // Don't worry about the extension if they explicitly
            // named a file on the command line.
            n00b_list_append(all_files, n00b_to_utf8(s));
            continue;
        case N00B_FK_IS_DIR:
        case N00B_FK_IS_DLINK:
            n00b_list_append(to_load, n00b_to_utf8(s));
            continue;
        case N00B_FK_NOT_FOUND:
            if (!n00b_str_ends_with(s, ext)) {
                // We only attempt to add the file extension if
                // it's something on the command line.
                fname = n00b_to_utf8(n00b_str_concat(fname, ext));
                goto one_retry;
            }
            s = n00b_path_simple_join(cur_dir, fname);
            switch (n00b_get_file_kind(s)) {
            case N00B_FK_IS_REG_FILE:
            case N00B_FK_IS_FLINK:
                n00b_list_append(all_files, n00b_to_utf8(s));
                continue;
            case N00B_FK_IS_DIR:
            case N00B_FK_IS_DLINK:
                n00b_list_append(to_load, n00b_to_utf8(s));
                continue;
            default:
                break;
            }
            n00b_printf("[red]error:[/] No such file or directory: {}", s);
            continue;
        default:
            n00b_printf("[red]error:[/] Cannot process special file: {}", s);
            continue;
        }
    }

    n = n00b_list_len(to_load);

    for (int i = 0; i < n; i++) {
        n00b_utf8_t *path  = n00b_list_get(to_load, i, NULL);
        n00b_list_t *files = n00b_path_walk(path,
                                          n00b_kw("follow_links",
                                                 n00b_ka(true),
                                                 "recurse",
                                                 n00b_ka(false),
                                                 "yield_dirs",
                                                 n00b_ka(false)));

        int  walk_len                 = n00b_list_len(files);
        bool found_file_in_this_batch = false;

        for (int j = 0; j < walk_len; j++) {
            n00b_utf8_t *one = n00b_list_get(files, j, NULL);

            if (add_or_warn(all_files, one, ext)) {
                found_file_in_this_batch = true;
            }
        }

        if (!found_file_in_this_batch) {
            n00b_printf("[yellow]warning:[/] Nothing added from: {}", path);
        }
    }

    if (n00b_list_len(all_files) > 1) {
        n00b_list_sort(all_files, (n00b_sort_fn)fname_sort);
    }

    return all_files;
}

void
n00b_scan_and_prep_tests(void)
{
    n00b_gc_register_root(&n00b_test_info, 1);

    n00b_list_t *all_files = identify_test_files();
    n00b_test_total_items  = n00b_list_len(all_files);
    n00b_test_info         = n00b_gc_array_alloc(n00b_test_kat,
                                       n00b_test_total_items);

    for (int i = 0; i < n00b_test_total_items; i++) {
        n00b_test_info[i].path = n00b_list_get(all_files, i, NULL);
        if (find_docstring(&n00b_test_info[i])) {
            try_to_load_kat(&n00b_test_info[i]);
        }
    }
}
