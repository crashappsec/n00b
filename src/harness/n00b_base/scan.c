// Scan for test files, and populate the test matrix, including
// expected results.

#define N00B_USE_INTERNAL_API
#include "n00b/test_harness.h"

#define kat_is_bad(x)                   \
    n00b_give_malformed_warning = true; \
    x->is_malformed             = true; \
    return

static n00b_string_t *
process_hex(n00b_string_t *s)
{
    n00b_string_require_u32(s);

    char *r = n00b_gc_array_value_alloc(char, s->codepoints * 2 + 1);
    int   n = 0;

    for (int i = 0; i < s->codepoints; i++) {
        char c = s->u32_data[i];

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
            r[n++] = c;
            continue;
        case 'A':
            r[n++] = 'a';
            continue;
        case 'B':
            r[n++] = 'b';
            continue;
        case 'C':
            r[n++] = 'c';
            continue;
        case 'D':
            r[n++] = 'd';
            continue;
        case 'E':
            r[n++] = 'e';
            continue;
        case 'F':
            r[n++] = 'f';
            continue;
        default:
            continue;
        }
    }

    return n00b_utf8(r, n);
}

static inline bool
add_or_warn(n00b_list_t *flist, n00b_string_t *s, n00b_string_t *ext)
{
    if (n00b_string_ends_with(s, ext)) {
        n00b_list_append(flist, s);
        return true;
    }
    else {
        n00b_list_t   *parts = n00b_string_split(s, n00b_cached_slash());
        int            n     = n00b_list_len(parts);
        n00b_string_t *file  = n00b_list_get(parts, n - 1, NULL);
        if (!n00b_string_starts_with(file, n00b_cached_period())) {
            n00b_printf(
                "«#»warning:«/» Skipping file w/o «em2».n00b«/»"
                " file extension: «#»",
                s);
        }
        return false;
    }
}

static void
extract_output(n00b_test_kat *kat,
               int64_t        start,
               int64_t        end)
{
    n00b_string_t *s     = n00b_string_slice(kat->raw_docstring, start, end);
    s                    = n00b_string_strip(s);
    kat->expected_output = kat->is_hex ? process_hex(s) : s;
}

static void
extract_errors(n00b_test_kat *kat, int64_t start, int64_t end)
{
    n00b_string_t *s     = n00b_string_slice(kat->raw_docstring, start, end);
    kat->expected_errors = n00b_list(n00b_type_string());
    n00b_list_t *split   = n00b_string_split(s, n00b_cstring("\n"));
    int          l       = n00b_list_len(split);

    for (int i = 0; i < l; i++) {
        s = n00b_string_strip(n00b_list_get(split, i, NULL));

        if (!n00b_string_codepoint_len(s)) {
            continue;
        }

        n00b_list_append(kat->expected_errors, s);
    }
}

static void
parse_vm_instructions(n00b_test_kat *kat, n00b_string_t *line)
{
    kat->save = true;
    line      = n00b_string_strip(line);
    int n     = n00b_string_codepoint_len(line);

    if (n == 1) {
        return;
    }

    line   = n00b_string_slice(line, 1, n--);
    int ix = n00b_string_find(line, n00b_cached_colon());

    if (ix != -1) {
        kat->second_entry = n00b_string_strip(n00b_string_slice(line, 0, ix));
        line              = n00b_string_strip(n00b_string_slice(line, ix + 1, n));

        bool                 neg;
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
        kat->second_entry = line;
    }
}

static void
try_to_load_kat(n00b_test_kat *kat)
{
    int64_t len = n00b_string_codepoint_len(kat->raw_docstring);
    int64_t nl  = n00b_string_find(kat->raw_docstring, n00b_cstring("\n"));

    if (nl != 0) {
        n00b_string_t *line = n00b_string_slice(kat->raw_docstring, 0, nl);
        kat->raw_docstring  = n00b_string_slice(kat->raw_docstring, nl, len);

        if (line->data[0] == '#') {
            parse_vm_instructions(kat, line);
        }
    }

    kat->raw_docstring = n00b_string_strip(kat->raw_docstring);

    n00b_string_t *output = n00b_cstring("$output:");
    n00b_string_t *errors = n00b_cstring("$errors:");
    n00b_string_t *hex    = n00b_cstring("$hex:");
    int64_t        outix  = n00b_string_find(kat->raw_docstring, output);
    int64_t        errix  = n00b_string_find(kat->raw_docstring, errors);
    int64_t        hexix  = n00b_string_find(kat->raw_docstring, hex);

    if (hexix != -1) {
        kat->is_hex = true;
        outix       = hexix;
    }

    if (outix == -1 && errix == -1) {
        if (n00b_string_codepoint_len(kat->raw_docstring) != 0) {
            kat_is_bad(kat);
        }
        return;
    }

    if (outix == -1) {
        if (errix != 0) {
            kat_is_bad(kat);
        }

        extract_errors(kat, 9, n00b_string_codepoint_len(kat->raw_docstring));
        kat->ignore_output = true;
        return;
    }

    if (errix == -1) {
        if (outix != 0) {
            kat_is_bad(kat);
        }
        extract_output(kat, 9, n00b_string_codepoint_len(kat->raw_docstring));
        return;
    }

    if (outix != 0 && errix != 0) {
        kat_is_bad(kat);
    }

    if (errix != 0) {
        extract_output(kat, 9, errix);
        extract_errors(kat,
                       errix + 9,
                       n00b_string_codepoint_len(kat->raw_docstring));
    }
    else {
        extract_errors(kat, 9, outix);
        extract_output(kat,
                       outix + 9,
                       n00b_string_codepoint_len(kat->raw_docstring));
    }
}

static int
fname_sort(const n00b_string_t **s1, const n00b_string_t **s2)
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
    n00b_list_t   *argv      = n00b_get_program_arguments();
    int            n         = n00b_list_len(argv);
    n00b_list_t   *to_load   = n00b_list(n00b_type_string());
    n00b_string_t *test_dir  = n00b_get_env(n00b_cstring("N00B_TEST_DIR"));
    n00b_string_t *cur_dir   = n00b_get_current_directory();
    n00b_string_t *ext       = n00b_cstring(".n");
    n00b_list_t   *all_files = n00b_list(n00b_type_string());

    if (test_dir == NULL) {
        test_dir = n00b_cformat("«#»/tests/", n00b_n00b_root());
    }
    else {
        test_dir = n00b_resolve_path(test_dir);
    }

    n00b_list_append(n00b_path, test_dir);

    if (!n) {
        n    = 1;
        argv = n00b_list(n00b_type_string());
        n00b_list_append(argv, test_dir);
    }

    for (int i = 0; i < n; i++) {
        n00b_string_t *fname = n00b_list_get(argv, i, NULL);
one_retry:;
        n00b_string_t *s = n00b_path_simple_join(test_dir, fname);

        switch (n00b_get_file_kind(s)) {
        case N00B_FK_IS_REG_FILE:
        case N00B_FK_IS_FLINK:
            // Don't worry about the extension if they explicitly
            // named a file on the command line.
            n00b_list_append(all_files, s);
            continue;
        case N00B_FK_IS_DIR:
        case N00B_FK_IS_DLINK:
            n00b_list_append(to_load, s);
            continue;
        case N00B_FK_NOT_FOUND:
            if (!n00b_string_ends_with(s, ext)) {
                // We only attempt to add the file extension if
                // it's something on the command line.
                fname = n00b_string_concat(fname, ext);
                goto one_retry;
            }
            s = n00b_path_simple_join(cur_dir, fname);
            switch (n00b_get_file_kind(s)) {
            case N00B_FK_IS_REG_FILE:
            case N00B_FK_IS_FLINK:
                n00b_list_append(all_files, s);
                continue;
            case N00B_FK_IS_DIR:
            case N00B_FK_IS_DLINK:
                n00b_list_append(to_load, s);
                continue;
            default:
                break;
            }
            n00b_printf("«red»error:«/» No such file or directory: «#»", s);
            continue;
        default:
            n00b_printf("«red»error:«/» Cannot process special file: «#»", s);
            continue;
        }
    }

    n = n00b_list_len(to_load);

    for (int i = 0; i < n; i++) {
        n00b_string_t *path  = n00b_list_get(to_load, i, NULL);
        n00b_list_t   *files = n00b_path_walk(path,
                                            n00b_kw("follow_links",
                                                    n00b_ka(true),
                                                    "recurse",
                                                    n00b_ka(false),
                                                    "yield_dirs",
                                                    n00b_ka(false)));

        int  walk_len                 = n00b_list_len(files);
        bool found_file_in_this_batch = false;

        for (int j = 0; j < walk_len; j++) {
            n00b_string_t *one = n00b_list_get(files, j, NULL);

            if (add_or_warn(all_files, one, ext)) {
                found_file_in_this_batch = true;
            }
        }

        if (!found_file_in_this_batch) {
            n00b_printf("«yellow»warning:«/» Nothing added from: «#»", path);
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
