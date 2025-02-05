#define N00B_USE_INTERNAL_API
#include "n00b/test_harness.h"

static n00b_string_t *
line_strip(n00b_string_t *s)
{
    n00b_list_t *parts = n00b_string_split(s, n00b_cstring("\n"));

    for (int i = 0; i < n00b_list_len(parts); i++) {
        n00b_string_t *line = n00b_list_get(parts, i, NULL);
        n00b_string_t *item = n00b_string_strip(line);
        n00b_list_set(parts, i, item);
    }

    return n00b_string_join(parts, n00b_cstring("\n"));
}

static void
show_hex_dump_if_enabled(n00b_string_t *expected, n00b_string_t *actual)
{
#ifdef N00B_HEX_DUMP_ON_TEST_FAIL
    int elen = n00b_string_byte_len(expected);
    int alen = n00b_string_byte_len(actual);
    n00b_printf("«em1»Expected hex:");
    n00b_print(n00b_hex_dump(expected->data, elen));
    n00b_printf("«em1»Actual hex:");
    n00b_print(n00b_hex_dump(actual->data, alen));

    int n = n00b_min(elen, alen);

    for (int i = 0; i < n; i++) {
        if (expected->data[i] != actual->data[i]) {
            n00b_printf("First difference at byte: «:x»", (uint64_t)i);
            return;
        }
    }

    assert(elen != alen);
    n00b_printf("First difference at byte: «#:x»", (uint64_t)n);
    ;

#endif
}

n00b_test_exit_code
n00b_compare_results(n00b_test_kat    *kat,
                     n00b_compile_ctx *ctx,
                     n00b_buf_t       *outbuf)
{
    if (kat->expected_output) {
        if (!outbuf || n00b_buffer_len(outbuf) == 0) {
            if (!n00b_string_codepoint_len(kat->expected_output)) {
                goto next_comparison;
            }
empty_err:
            n00b_printf(
                "«red»FAIL«/»: test «i»«#»«/»: program expected output "
                "but did not compile. Expected output:\n «#»",
                kat->path,
                kat->expected_output);
            return n00b_tec_no_compile;
        }
        else {
            n00b_string_t *output = n00b_buf_to_utf8_string(outbuf);
            output                = n00b_string_strip(output);

            if (n00b_string_codepoint_len(output) == 0) {
                goto empty_err;
            }

            if (kat->is_hex) {
                output = n00b_string_to_hex(output, false);
            }

            n00b_string_t *expected = line_strip(kat->expected_output);
            n00b_string_t *actual   = line_strip(output);

            if (!n00b_string_eq(expected, actual)) {
                n00b_printf(
                    "«red»FAIL«/»: test «i»«#»«/»: output mismatch.",
                    kat->path);
                n00b_printf("«em1»Expected output«/»\n");
                n00b_print(expected);
                n00b_printf("«em1»Actual«/»\n");
                n00b_print(actual);
                show_hex_dump_if_enabled(expected, actual);
                return n00b_tec_output_mismatch;
            }
        }
    }

next_comparison:;
    n00b_list_t *actual_errs  = n00b_compile_extract_all_error_codes(ctx);
    int          num_expected = 0;
    int          num_actual   = n00b_list_len(actual_errs);

    if (kat->expected_errors != NULL) {
        num_expected = n00b_list_len(kat->expected_errors);
    }

    if (num_expected != num_actual) {
        n00b_show_err_diffs(kat->path, kat->expected_errors, actual_errs);
        return n00b_tec_err_mismatch;
    }
    else {
        for (int i = 0; i < num_expected; i++) {
            n00b_compile_error_t c1;
            n00b_string_t       *c2;

            c1 = (uint64_t)n00b_list_get(actual_errs, i, NULL);
            c2 = n00b_list_get(kat->expected_errors, i, NULL);
            c2 = n00b_string_strip(c2);

            if (!n00b_string_eq(n00b_err_code_to_str(c1), c2)) {
                n00b_show_err_diffs(kat->path,
                                    kat->expected_errors,
                                    actual_errs);
                return n00b_tec_err_mismatch;
            }
        }
    }

    return n00b_tec_success;
}
