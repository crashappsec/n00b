#pragma once
#include "n00b.h"

static inline void
n00b_proc_set_command(n00b_proc_t *proc, n00b_string_t *cmd)
{
    n00b_proc_check_and_set(proc, proc->cmd = cmd);
}

static inline void
n00b_proc_set_args(n00b_proc_t *proc, n00b_list_t *args)
{
    n00b_proc_check_and_set(proc, proc->args = args);
}

static inline void
n00b_proc_set_env(n00b_proc_t *proc, n00b_list_t *env)
{
    n00b_proc_check_and_set(proc, proc->env = env);
}

static inline void
n00b_proc_capture_stdin(n00b_proc_t *proc)
{
    n00b_proc_check_and_set(proc, proc->flags |= N00B_PROC_STDIN_CAP);
}

static inline void
n00b_proc_capture_stdout(n00b_proc_t *proc)
{
    n00b_proc_check_and_set(proc, proc->flags |= N00B_PROC_STDOUT_CAP);
}

static inline void
n00b_proc_capture_stderr(n00b_proc_t *proc)
{
    n00b_proc_check_and_set(proc, proc->flags |= N00B_PROC_STDERR_CAP);
}

static inline void
n00b_proc_proxy_stdin(n00b_proc_t *proc)
{
    n00b_proc_check_and_set(proc, proc->flags |= N00B_PROC_STDIN_PROXY);
}

static inline void
n00b_proc_proxy_stdout(n00b_proc_t *proc)
{
    n00b_proc_check_and_set(proc, proc->flags |= N00B_PROC_STDOUT_PROXY);
}

static inline void
n00b_proc_proxy_stderr(n00b_proc_t *proc)
{
    n00b_proc_check_and_set(proc, proc->flags |= N00B_PROC_STDERR_PROXY);
}

static inline void
n00b_proc_stdin_close_proxy(n00b_proc_t *proc)
{
    n00b_proc_check_and_set(proc, proc->flags |= N00B_PROC_STDIN_CLOSE_PROXY);
}

static inline void
n00b_proc_use_pty(n00b_proc_t *proc)
{
    n00b_proc_check_and_set(proc, proc->flags |= N00B_PROC_USE_PTY);
}

static inline void
n00b_proc_set_raw_argv(n00b_proc_t *proc)
{
    // If raw, we don't add the command name to the front of argv.
    n00b_proc_check_and_set(proc, proc->flags |= N00B_PROC_RAW_ARGV);
}

static inline void
n00b_proc_set_merge_output(n00b_proc_t *proc)
{
    n00b_proc_check_and_set(proc, proc->flags |= N00B_PROC_MERGE_OUTPUT);
}

static inline void
n00b_proc_set_kill_on_timeout(n00b_proc_t *proc)
{
    n00b_proc_check_and_set(proc, proc->flags |= N00B_PROC_KILL_ON_TIMEOUT);
}

static inline n00b_buf_t *
n00b_proc_get_stdout_capture(n00b_proc_t *proc)
{
    n00b_proc_post_check(proc);

    if (!proc->cap_out) {
        N00B_CRAISE("stdout was not monitored for this process.");
    }
    return n00b_stream_extract_buffer(proc->cap_out);
}

static inline n00b_buf_t *
n00b_proc_get_stderr_capture(n00b_proc_t *proc)
{
    n00b_proc_post_check(proc);

    if (!proc->cap_err) {
        N00B_CRAISE("stderr was not monitored for this process.");
    }
    return n00b_stream_extract_buffer(proc->cap_err);
}

static inline n00b_buf_t *
n00b_proc_get_stdin_capture(n00b_proc_t *proc)
{
    n00b_proc_post_check(proc);

    if (!proc->cap_in) {
        N00B_CRAISE("stdin was not monitored for this process.");
    }

    return n00b_stream_extract_buffer(proc->cap_in);
}

static inline bool
n00b_proc_get_errored(n00b_proc_t *proc)
{
    n00b_proc_post_check(proc);
    return proc->errored;
}

static inline int
n00b_proc_get_term_signal(n00b_proc_t *proc)
{
    if (!n00b_proc_get_errored(proc)) {
        N00B_CRAISE(
            "Cannot query termination signal for a process that exits "
            "normally.");
    }

    return proc->term_signal;
}

static inline int
n00b_proc_get_errno(n00b_proc_t *proc)
{
    if (!n00b_proc_get_errored(proc)) {
        N00B_CRAISE("Cannot query errno for a process that exits normally.");
    }

    return proc->saved_errno;
}

static inline int
n00b_proc_get_using_tty(n00b_proc_t *proc)
{
    return proc->flags & N00B_PROC_USE_PTY;
}

static inline bool
n00b_proc_get_exited(n00b_proc_t *proc)
{
    return proc->exited;
}

static inline int
n00b_proc_get_exit_code(n00b_proc_t *proc)
{
    n00b_proc_post_check(proc);
    if (proc->errored) {
        N00B_CRAISE("Cannot get exit status; process execution errored.");
    }
    return proc->exit_status;
}
