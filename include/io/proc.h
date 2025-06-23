#pragma once
#include "n00b.h"

enum {
    N00B_PROC_STDIN_CAP         = 1,
    N00B_PROC_STDOUT_CAP        = 2,
    N00B_PROC_STDERR_CAP        = 4,
    N00B_PROC_CAP_ALL           = 7,
    N00B_PROC_STDIN_PROXY       = 8,
    N00B_PROC_STDIN_MASK        = 9,
    N00B_PROC_STDOUT_PROXY      = 16,
    N00B_PROC_STDOUT_MASK       = 18,
    N00B_PROC_STDERR_PROXY      = 32,
    N00B_PROC_STDERR_MASK       = 36,
    N00B_PROC_STDIN_CLOSE_PROXY = 64,
    N00B_PROC_PROXY_ALL         = 120,
    N00B_PROC_USE_PTY           = 128,
    N00B_PROC_RAW_ARGV          = 256,
    N00B_PROC_MERGE_OUTPUT      = 512,
    N00B_PROC_KILL_ON_TIMEOUT   = 1024,
    N00B_PROC_PTY_STDERR        = 2048,
    N00B_PROC_HANDLE_WIN_SIZE   = 4096,
};

typedef void (*n00b_post_fork_hook_t)(void *);

typedef struct {
    n00b_string_t        *cmd;
    n00b_string_t        *seeded_stdin;
    n00b_list_t          *args;
    n00b_list_t          *env;
    n00b_stream_t        *subproc_stdin;
    n00b_stream_t        *subproc_stdout;
    n00b_stream_t        *subproc_stderr;
    n00b_stream_t        *subproc_pid;
    n00b_exit_info_t     *subproc_results;
    n00b_list_t          *stdin_subs;
    n00b_list_t          *stdout_subs;
    n00b_list_t          *stderr_subs;
    n00b_stream_t        *cap_in;
    n00b_stream_t        *cap_out;
    n00b_stream_t        *cap_err;
    n00b_stream_t        *exit_cb;
    n00b_post_fork_hook_t hook;
    void                 *param;
    struct winsize        dimensions;
    struct termios       *parent_termcap;
    struct termios       *subproc_termcap_ptr;
    struct termios        subproc_termcap;
    struct termios        initial_termcap;
    int                   exit_status;
    int                   term_signal;
    int                   saved_errno;
    pid_t                 pid;
    int                   flags;
    bool                  errored;
    bool                  exited;
    bool                  timeout;
    bool                  wait_for_exit;
    int                   gate;
    n00b_condition_t      cv;
} n00b_proc_t;

#define n00b_proc_check_and_set(p, operation)              \
    bool err = false;                                      \
    if (p->pid >= 0) {                                     \
        err = true;                                        \
    }                                                      \
    else {                                                 \
        operation;                                         \
    }                                                      \
    if (err) {                                             \
        N00B_CRAISE("Cannot set value on launched proc."); \
    }

#define n00b_proc_post_check(p)

// TODO: Add a callback option.

extern n00b_proc_t *_n00b_run_process(n00b_string_t *cmd,
                                      n00b_list_t   *argv,
                                      bool           proxy,
                                      bool           capture,
                                      ...);

extern void n00b_proc_spawn(n00b_proc_t *);
extern bool n00b_proc_run(n00b_proc_t *, n00b_duration_t *);
extern void n00b_proc_close(n00b_proc_t *);

#define n00b_run_process(cmd, proxy, capture, ...) \
    _n00b_run_process(cmd, proxy, capture, N00B_VA(__VA_ARGS__))
