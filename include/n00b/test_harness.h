#ifdef N00B_USE_INTERNAL_API
#pragma once
#include "n00b.h"

typedef enum {
    n00b_tec_success = 0,
    n00b_tec_no_compile,
    n00b_tec_output_mismatch,
    n00b_tec_err_mismatch,
    n00b_tec_memcheck,
    n00b_tec_exception,
} n00b_test_exit_code;

typedef struct {
    n00b_utf8_t   *path;
    n00b_str_t    *raw_docstring;
    n00b_str_t    *second_entry;
    n00b_utf8_t   *expected_output;
    n00b_list_t   *expected_errors;
    struct rusage usage;
    int           case_number;
    int           second_entry_executions;
    bool          ignore_output;
    bool          is_hex;
    bool          is_test;
    bool          is_malformed;
    bool          run_ok;    // True if it ran successfully.
    bool          timeout;   // True if failed due to a timeout (not a crash)
    bool          save;
    bool          stopped;   // Process was suspended via signal.
    int           exit_code; // Exit code of process if successfully run.
    int           signal;
    int           err_value; // Non-zero on a failure.
} n00b_test_kat;

extern int           n00b_test_total_items;
extern int           n00b_test_total_tests;
extern int           n00b_non_tests;
extern _Atomic int   n00b_test_number_passed;
extern _Atomic int   n00b_test_number_failed;
extern _Atomic int   n00b_test_next_test;
extern n00b_test_kat *n00b_test_info;
extern int           n00b_current_test_case;
extern int           n00b_watch_case;
extern bool          n00b_dev_mode;
extern bool          n00b_give_malformed_warning;
extern size_t        n00b_term_width;

extern void               n00b_scan_and_prep_tests(void);
extern void               n00b_run_expected_value_tests(void);
extern void               n00b_run_other_test_files(void);
extern n00b_test_exit_code n00b_compare_results(n00b_test_kat *,
                                              n00b_compile_ctx *,
                                              n00b_buf_t *);
extern void               n00b_report_results_and_exit(void);
extern void               n00b_show_err_diffs(n00b_utf8_t *,
                                             n00b_list_t *,
                                             n00b_list_t *);
extern void               n00b_show_dev_compile_info(n00b_compile_ctx *);
extern void               n00b_show_dev_disasm(n00b_vm_t *, n00b_module_t *);

static inline void
announce_test_start(n00b_test_kat *item)
{
    n00b_printf("[h4]Running test [atomic lime]{}[/atomic lime]: [i]{}",
               n00b_box_u64(item->case_number),
               item->path);
}

static inline void
announce_test_end(n00b_test_kat *kat)
{
    if (kat->run_ok && !kat->exit_code) {
        n00b_test_number_passed++;
        n00b_printf(
            "[h4]Finished test [atomic lime]{}[/atomic lime]: ({}). "
            "[i atomic lime]PASSED.",
            n00b_box_u64(kat->case_number),
            kat->path);
    }
    else {
        n00b_test_number_failed++;
        n00b_printf(
            "[h4]Finished test [atomic lime]{}[/atomic lime]: ({}). "
            "[i b navy blue]FAILED.",
            n00b_box_u64(kat->case_number),
            kat->path);
    }
    // TODO: format rusage data here.
}
#endif
