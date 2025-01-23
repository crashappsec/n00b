#define N00B_USE_INTERNAL_API
#include "n00b/test_harness.h"

static n00b_test_exit_code
execute_test(n00b_test_kat *kat)
{
    n00b_compile_ctx *ctx;
    n00b_gc_show_heap_stats_on();
    n00b_printf("[h1]Processing module {}", kat->path);

    ctx = n00b_compile_from_entry_point(kat->path);

    if (n00b_dev_mode) {
        n00b_show_dev_compile_info(ctx);
    }

    n00b_grid_t *err_output = n00b_format_errors(ctx);

    if (err_output != NULL) {
        n00b_grid_t *err_grid = n00b_new(n00b_type_grid(),
                                         n00b_kw("start_rows",
                                                 n00b_ka(2),
                                                 "start_cols",
                                                 n00b_ka(1),
                                                 "container_tag",
                                                 n00b_ka(n00b_new_utf8("flow"))));
        n00b_utf8_t *s        = n00b_new_utf8("Error Output");
        n00b_grid_add_row(err_grid,
                          n00b_to_str_renderable(s,
                                                 n00b_new_utf8("h2")));
        n00b_grid_add_row(err_grid, err_output);
        n00b_print(err_grid);
    }

    n00b_printf("[atomic lime]info:[/] Done processing: {}", kat->path);

    if (n00b_got_fatal_compiler_error(ctx)) {
        if (kat->is_test) {
            return n00b_compare_results(kat, ctx, NULL);
        }
        return n00b_tec_no_compile;
    }

    n00b_vm_t *vm = n00b_vm_new(ctx);
    n00b_generate_code(ctx, vm);

    if (n00b_dev_mode) {
        int            n = vm->entry_point;
        n00b_module_t *m = n00b_list_get(vm->obj->module_contents, n, NULL);

        n00b_show_dev_disasm(vm, m);
    }

    n00b_printf("[h6]****STARTING PROGRAM EXECUTION*****[/]");
    n00b_vmthread_t *thread = n00b_vmthread_new(vm);
    n00b_vmthread_run(thread);
    n00b_printf("[h6]****PROGRAM EXECUTION FINISHED*****[/]\n");

    if (kat->save) {
        n00b_printf("\n[h3]Saving VM state.");
        n00b_vm_save(vm);
        n00b_printf("[h4]First run saved.");

        if (kat->second_entry != NULL) {
            n00b_utf8_t *test_dir;
            n00b_utf8_t *s;

            test_dir = n00b_get_env(n00b_new_utf8("N00B_TEST_DIR"));

            if (test_dir == NULL) {
                test_dir = n00b_cstr_format("{}/tests/", n00b_n00b_root());
            }
            else {
                test_dir = n00b_resolve_path(test_dir);
            }

            s = n00b_path_simple_join(test_dir, kat->second_entry);
            n00b_compile_ctx *rc_ctx;
            if (!n00b_incremental_module(vm, s, true, &rc_ctx)) {
                n00b_printf("[h3]**Incremental module add failed.**");
            }
        }

        n00b_printf("[h3]Re-running.");
        n00b_vmthread_t *thread = n00b_vmthread_new(vm);
        n00b_vmthread_run(thread);
        n00b_printf("[h3]Finished re-running.");
    }

    // TODO: We need to mark unlocked types with sub-variables at some point,
    // so they don't get clobbered.
    //
    // E.g.,  (dict[`x, list[int]]) -> int

    // n00b_clean_environment();
    // n00b_print(n00b_format_global_type_environment());
    if (kat->is_test) {
        return n00b_compare_results(kat, ctx, vm->run_state->print_buf);
    }

    return n00b_tec_success;
}
static n00b_test_exit_code
run_one_item(n00b_test_kat *kat)
{
    n00b_test_exit_code ec = execute_test(kat);
    return ec;
}

static void
monitor_test(n00b_test_kat *kat, int readfd, pid_t pid)
{
    struct timeval timeout = (struct timeval){
        .tv_sec  = N00B_TEST_SUITE_TIMEOUT_SEC,
        .tv_usec = N00B_TEST_SUITE_TIMEOUT_USEC,
    };

    fd_set select_ctx;
    int    status = 0;

    FD_ZERO(&select_ctx);
    FD_SET(readfd, &select_ctx);

    switch (select(readfd + 1, &select_ctx, NULL, NULL, &timeout)) {
    case 0:
        kat->timeout = true;
        n00b_print(n00b_callout(n00b_cstr_format("{} TIMED OUT.",
                                                 kat->path)));
        wait4(pid, &status, WNOHANG | WUNTRACED, &kat->usage);
        break;
    case -1:
        n00b_print(n00b_callout(n00b_cstr_format("{} CRASHED.", kat->path)));
        kat->err_value = errno;
        break;
    default:
        kat->run_ok = true;
        close(readfd);
        wait4(pid, &status, 0, &kat->usage);
        break;
    }

    if (WIFEXITED(status)) {
        kat->exit_code = WEXITSTATUS(status);

        n00b_printf("[h4]{}[/h4] exited with return code: [em]{}[/].",
                    kat->path,
                    n00b_box_u64(kat->exit_code));

        announce_test_end(kat);
        return;
    }

    if (WIFSIGNALED(status)) {
        kat->signal = WTERMSIG(status);
        announce_test_end(kat);
        return;
    }

    if (WIFSTOPPED(status)) {
        kat->stopped = true;
        kat->signal  = WSTOPSIG(status);
    }

    // If we got here, the test needs to be cleaned up.
    kill(pid, SIGKILL);
    announce_test_end(kat);
}

void
n00b_run_expected_value_tests(void)
{
    // For now, since the GC isn't working w/ cross-thread accesses yet,
    // we are just going to spawn fork and communicate via exist status.

    for (int i = 0; i < n00b_test_total_items; i++) {
        n00b_test_kat *item = &n00b_test_info[i];

        if (!item->is_test) {
            continue;
        }

        announce_test_start(item);

        // We never write to this file descriptor; if the child dies
        // the select will fire, and if it doesn't, it still allows us
        // to time out, where waitpid() and friends do not.
        int pipefds[2];
        if (pipe(pipefds) == -1) {
            abort();
        }

#ifndef N00B_TEST_WITHOUT_FORK
        pid_t pid = fork();

        if (!pid) {
            close(pipefds[0]);
            n00b_exit(run_one_item(item));
        }

        close(pipefds[1]);
        monitor_test(item, pipefds[0], pid);
#else
        item->exit_code = run_one_item(item);
        item->run_ok    = true;
        announce_test_end(item);
#endif
    }
}

void
n00b_run_other_test_files(void)
{
    if (n00b_test_total_items == n00b_test_total_tests) {
        return;
    }

    n00b_print(n00b_callout(n00b_new_utf8("RUNNING TESTS LACKING TEST SPECS.")));
    for (int i = 0; i < n00b_test_total_items; i++) {
        n00b_test_kat *item = &n00b_test_info[i];

        if (item->is_test) {
            continue;
        }

        n00b_printf("[h4]Running non-test case:[i] {}", item->path);

        int pipefds[2];

        if (pipe(pipefds) == -1) {
            abort();
        }

        pid_t pid = fork();
        if (!pid) {
            close(pipefds[0]);
            exit(run_one_item(item));
        }

        close(pipefds[1]);

        struct timeval timeout = (struct timeval){
            .tv_sec  = N00B_TEST_SUITE_TIMEOUT_SEC,
            .tv_usec = N00B_TEST_SUITE_TIMEOUT_USEC,
        };

        fd_set select_ctx;
        int    status;

        FD_ZERO(&select_ctx);
        FD_SET(pipefds[0], &select_ctx);

        switch (select(pipefds[0] + 1, &select_ctx, NULL, NULL, &timeout)) {
        case 0:
            n00b_print(n00b_callout(n00b_cstr_format("{} TIMED OUT.",
                                                     item->path)));
            kill(pid, SIGKILL);
            continue;
        case -1:
            n00b_print(n00b_callout(n00b_cstr_format("{} CRASHED.", item->path)));
            continue;
        default:
            waitpid(pid, &status, WNOHANG);
            n00b_printf("[h4]{}[/h4] exited with return code: [em]{}[/].",
                        item->path,
                        n00b_box_u64(WEXITSTATUS(status)));
            continue;
        }
    }
}
