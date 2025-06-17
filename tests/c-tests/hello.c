#include "n00b.h"

int
main()
{
    n00b_terminal_app_setup();
    n00b_string_t *cmd = n00b_cstring("echo");
    cmd                = n00b_find_first_command_path(cmd, NULL, true);
    n00b_list_t *l     = n00b_list(n00b_type_string());
    n00b_list_append(l, n00b_cstring("hello,"));
    n00b_list_append(l, n00b_cstring(" world!"));

    struct timeval timeout = {.tv_sec = 600, .tv_usec = 0};
    n00b_proc_t   *pi      = n00b_run_process(cmd,
                                       l,
                                       false,
                                       true,
                                       n00b_header_kargs("merge_output",
                                                         1ULL,
                                                         "timeout",
                                                         &timeout));
    n00b_buf_t    *bout    = n00b_proc_get_stdout_capture(pi);

    printf("stdout capture: %s\n",
           (bout && bout->byte_len) ? bout->data : "(none)");

    n00b_printf("«em1»Subprocess completed with error code «#».",
                (int64_t)n00b_proc_get_exit_code(pi));
}
