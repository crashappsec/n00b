#define N00B_USE_INTERNAL_API
#include "n00b.h"

void
n00b_proc_proxy_winch(n00b_proc_t *ctx)
{
    int fd;

    ioctl(1, TIOCGWINSZ, &ctx->dimensions);
    if (ctx->subproc_stdout) {
        fd = n00b_channel_fileno(ctx->subproc_stdout);
        ioctl(fd, TIOCSWINSZ, &ctx->dimensions);
    }

    if (ctx->subproc_stderr) {
        fd = n00b_channel_fileno(ctx->subproc_stderr);
        ioctl(fd, TIOCSWINSZ, &ctx->dimensions);
    }
}

static void
n00b_handle_winch(n00b_channel_t *sig, int64_t signal, n00b_proc_t *ctx)
{
    n00b_proc_proxy_winch(ctx);
}

static void
proc_exited(n00b_exit_info_t *result, n00b_proc_t *ctx)
{
    n00b_condition_lock_acquire(&ctx->cv);
    ctx->subproc_results = result;
    ctx->exited          = true;

    if (ctx->wait_for_exit) {
        n00b_condition_notify_all(&ctx->cv);
    }
    n00b_condition_lock_release(&ctx->cv);
}

static void
pre_launch_prep(n00b_proc_t *proc, char ***argp)
{
    // At this point, the child process file descriptors will not yet
    // exist.
    if (!proc->cmd) {
        N00B_CRAISE("No command set.");
    }

    n00b_list_t *args;

    if (!(proc->flags & N00B_PROC_RAW_ARGV)) {
        args = n00b_list(n00b_type_ref());
        n00b_list_append(args, proc->cmd);
        if (proc->args != NULL) {
            args = n00b_list_plus(args, proc->args);
        }
    }
    else {
        if (proc->args) {
            args = proc->args;
        }
        else {
            args = n00b_list(n00b_type_ref());
        }
    }

    *argp = (void *)n00b_make_cstr_array(args, NULL);
}

static n00b_channel_t *
setup_exit_obj(n00b_proc_t *ctx)
{
    n00b_channel_t *result   = n00b_new_exit_channel(ctx->pid);
    n00b_channel_t *callback = n00b_new_callback_channel((void *)proc_exited,
                                                         ctx);
    n00b_channel_set_name(callback, n00b_cstring("wait4 exit"));
    n00b_channel_subscribe_read(result, callback, true);

    return result;
}

static void
post_spawn_subscription_setup(n00b_proc_t *ctx)
{
    ctx->subproc_pid = setup_exit_obj(ctx);

    if (ctx->subproc_stdout) {
        //        n00b_channel_fd_pause_reads(ctx->subproc_stdout);
    }
    if (ctx->subproc_stderr) {
        //        n00b_channel_fd_pause_reads(ctx->subproc_stderr);
    }

    if (ctx->seeded_stdin && ctx->subproc_stdin) {
        n00b_channel_queue(ctx->subproc_stdin,
                           n00b_string_to_buffer(ctx->seeded_stdin));
        ctx->seeded_stdin = NULL;
    }

    if (ctx->flags & N00B_PROC_STDIN_PROXY) {
        // Proxy parent's stdin to the subproc.
        n00b_channel_subscribe_read(n00b_chan_stdin(),
                                    ctx->subproc_stdin,
                                    false);
    }

    if (ctx->flags & (N00B_PROC_STDOUT_PROXY)) {
        n00b_channel_subscribe_read(ctx->subproc_stdout,
                                    n00b_chan_stdout(),
                                    false);

        if (ctx->flags & N00B_PROC_MERGE_OUTPUT) {
            n00b_channel_subscribe_read(ctx->subproc_stderr,
                                        n00b_chan_stdout(),
                                        false);
        }
    }

    if ((ctx->flags & N00B_PROC_STDERR_PROXY)
        && !(ctx->flags & N00B_PROC_MERGE_OUTPUT) && ctx->subproc_stderr) {
        n00b_channel_subscribe_read(ctx->subproc_stderr,
                                    n00b_chan_stderr(),
                                    NULL);
    }

    n00b_channel_t *buf;

    if (ctx->flags & N00B_PROC_STDIN_CAP) {
        ctx->cap_in = n00b_buffer_empty();
        buf         = n00b_out_buf_channel(ctx->cap_in, true);
        n00b_channel_subscribe_read(n00b_chan_stdin(), buf, false);
        n00b_channel_set_name(buf,
                              n00b_cformat("«#»«#» stdin cap«#»",
                                           n00b_cached_lbracket(),
                                           ctx->cmd,
                                           n00b_cached_rbracket()));
    }

    if (ctx->flags & N00B_PROC_STDOUT_CAP) {
        ctx->cap_out = n00b_buffer_empty();
        buf          = n00b_out_buf_channel(ctx->cap_out, true);
        n00b_channel_subscribe_read(ctx->subproc_stdout, buf, false);
        n00b_channel_set_name(buf,
                              n00b_cformat("«#»«#» stdout cap«#»",
                                           n00b_cached_lbracket(),
                                           ctx->cmd,
                                           n00b_cached_rbracket()));

        if (ctx->flags & N00B_PROC_MERGE_OUTPUT) {
            n00b_channel_subscribe_read(ctx->subproc_stderr, buf, false);
        }
    }

    if (ctx->flags & N00B_PROC_STDERR_CAP
        && !(ctx->flags & N00B_PROC_MERGE_OUTPUT) && ctx->subproc_stderr) {
        ctx->cap_err = n00b_buffer_empty();
        buf          = n00b_out_buf_channel(ctx->cap_err, true);
        n00b_channel_subscribe_read(ctx->subproc_stderr, buf, false);
        n00b_channel_set_name(buf,
                              n00b_cformat("«#»«#» stderr cap«#»",
                                           n00b_cached_lbracket(),
                                           ctx->cmd,
                                           n00b_cached_rbracket()));
    }

    if (ctx->pending_stdout_subs != NULL) {
        int n = n00b_list_len(ctx->pending_stdout_subs);
        for (int i = 0; i < n; i++) {
            n00b_channel_t *s = n00b_list_get(ctx->pending_stdout_subs, i, NULL);
            n00b_channel_subscribe_read(ctx->subproc_stdout, s, false);
        }
    }

    if (ctx->pending_stderr_subs != NULL) {
        int n = n00b_list_len(ctx->pending_stderr_subs);
        for (int i = 0; i < n; i++) {
            n00b_channel_t *s = n00b_list_get(ctx->pending_stderr_subs, i, NULL);
            n00b_channel_subscribe_read(ctx->subproc_stderr, s, false);
        }
    }

    /*
    if (ctx->subproc_stdout) {
        n00b_channel_fd_unpause_reads(ctx->subproc_stdout);
    }
    if (ctx->subproc_stderr) {
        n00b_channel_fd_unpause_reads(ctx->subproc_stderr);
    }

    if (ctx->flags & N00B_PROC_STDIN_PROXY) {
        n00b_channel_fd_unpause_reads(n00b_chan_stdin());
        }*/
    n00b_show_channels();

    return;
}

static inline bool
open_pipe(bool proxy, int filedes[2])
{
    if (!proxy) {
        return false;
    }

    if (pipe(filedes)) {
        n00b_raise_errno();
    }

    return true;
}

static inline bool
close_read_side(bool proxy, int filedes[2])
{
    if (!proxy) {
        return false;
    }

    n00b_raw_fd_close(filedes[0]);
    return true;
}

static inline bool
close_write_side(bool proxy, int filedes[2])
{
    if (!proxy) {
        return false;
    }

    n00b_raw_fd_close(filedes[1]);
    return true;
}

static void
try_execve(n00b_proc_t *ctx, char *argv[], char *envp[])
{
    char *cmd = n00b_string_to_cstr(ctx->cmd);

    execve(cmd, argv, envp);
    n00b_raise_errno();
}

static inline void
proc_spawn_no_tty(n00b_proc_t *ctx)
{
    bool   proxy_in  = ctx->flags & N00B_PROC_STDIN_MASK;
    bool   proxy_out = ctx->flags & N00B_PROC_STDOUT_MASK;
    bool   proxy_err = ctx->flags & N00B_PROC_STDERR_MASK;
    pid_t  pid;
    int    stdin_pipe[2];
    int    stdout_pipe[2];
    int    stderr_pipe[2];
    char **argp;
    char **envp;

    pre_launch_prep(ctx, &argp);

    open_pipe(proxy_in, stdin_pipe);
    open_pipe(proxy_out, stdout_pipe);
    open_pipe(proxy_err, stderr_pipe);

    if (stdin_pipe[0] > -1) {
        fcntl(stdin_pipe[0],
              F_SETFD,
              fcntl(stdin_pipe[0], F_GETFD) & ~FD_CLOEXEC);
    }

    if (stdout_pipe[1] > -1) {
        fcntl(stdout_pipe[1],
              F_SETFD,
              fcntl(stdout_pipe[1], F_GETFD) & ~FD_CLOEXEC);
    }

    if (stderr_pipe[1] > -1) {
        fcntl(stderr_pipe[1],
              F_SETFD,
              fcntl(stderr_pipe[1], F_GETFD) & ~FD_CLOEXEC);
    }

    pid = fork();

    if (pid != 0) {
        ctx->pid = pid;

        if (ctx->flags & N00B_PROC_STDIN_PROXY) {
            //            n00b_channel_fd_pause_reads(n00b_chan_stdin());
        }

        if (proxy_in) {
            ctx->subproc_stdin = n00b_channel_fd_open(stdin_pipe[1]);
        }
        if (proxy_out) {
            ctx->subproc_stdout = n00b_channel_fd_open(stdout_pipe[0]);
        }
        if (proxy_err) {
            ctx->subproc_stderr = n00b_channel_fd_open(stderr_pipe[0]);
        }
        post_spawn_subscription_setup(ctx);

        return;
    }

    close_write_side(proxy_in, stdin_pipe);
    close_read_side(proxy_out, stdout_pipe);
    close_read_side(proxy_err, stderr_pipe);

    if (proxy_in) {
        dup2(stdin_pipe[0], 0);
    }
    if (proxy_out) {
        dup2(stdout_pipe[1], 1);
    }
    if (proxy_err) {
        dup2(stderr_pipe[1], 2);
    }

    if (ctx->hook) {
        (*ctx->hook)(ctx->param);
    }

    if (ctx->env) {
        envp = (void *)n00b_make_cstr_array(ctx->env, NULL);
    }

    else {
        envp = n00b_raw_envp();
    }

    try_execve(ctx, argp, envp);
}

static void
should_exit_via_sig(n00b_channel_t *sig, int64_t signal, n00b_proc_t *ctx)
{
    n00b_condition_lock_acquire(&ctx->cv);
    n00b_channel_close(ctx->subproc_stdin);
    n00b_channel_close(ctx->subproc_stdout);
    n00b_channel_close(ctx->subproc_stderr);
    ctx->exited = true;
    if (ctx->wait_for_exit) {
        n00b_condition_notify_all(&ctx->cv);
    }
    n00b_condition_lock_release(&ctx->cv);
}

static inline void
proc_spawn_with_tty(n00b_proc_t *ctx)
{
    struct winsize  wininfo;
    struct termios *term_ptr = ctx->subproc_termcap_ptr;
    struct winsize *win_ptr  = &wininfo;
    bool            use_aux  = ctx->flags & N00B_PROC_PTY_STDERR;
    int             pty_fd;
    pid_t           pid;
    int             aux_fd;
    char          **argp;
    char          **envp;

    // n00b_register_signal_handler(SIGCHLD, (void *)should_exit_via_sig, ctx);
    // n00b_register_signal_handler(SIGPIPE, (void *)should_exit_via_sig, ctx);

    pre_launch_prep(ctx, &argp);

    if (!n00b_channel_is_tty(n00b_chan_stdin())) {
        win_ptr = NULL;
    }
    else {
        ioctl(0, TIOCGWINSZ, win_ptr);
    }

    int subproc_aux;
    int subproc_fd;
    int flags = fcntl(0, F_GETFL) | O_NONBLOCK;

    if (openpty(&pty_fd, &subproc_fd, NULL, term_ptr, win_ptr)) {
        n00b_raise_errno();
    }

    if (use_aux) {
        // If the user asks for a separate stderr, we need to make
        // sure stderr is actually attached to a TTY; bash doesn't
        // like it if it's not (when passing down a dup'd fd instead,
        // somewhat common stuff breaks).
        //
        // However, for whatever reason, `more` will hang waiting for
        // input unless I put stdin and stderr on the same TTY. I
        // believe it opens stderr's tty and waits for reads from it.
        //
        // Right now, the below seems to work pretty well. But if
        // something makes the same assumption about stdout, then we
        // can write a little input proxy.
        openpty(&aux_fd, &subproc_aux, NULL, term_ptr, win_ptr);

        ctx->subproc_stderr = n00b_channel_fd_open(aux_fd);
    }

    int pmain[2];

    if (pipe(pmain)) {
        n00b_raise_errno();
    }

    pid = fork();

    if (pid < 0) {
        n00b_raise_errno();
    }

    int err_code;
    if (pid != 0) {
        n00b_raw_fd_close(subproc_fd);
        n00b_raw_fd_close(pmain[1]);

        if (ctx->flags & N00B_PROC_STDIN_PROXY) {
            n00b_channel_fd_pause_reads(n00b_chan_stdin());
        }

        if (read(pmain[0], &err_code, sizeof(err_code)) > 0) {
            int status;
            waitpid(pid, &status, 0);
            ctx->pid = -1;
            errno    = err_code;
            n00b_raw_fd_close(pmain[0]);
            n00b_raise_errno();
        }
        n00b_raw_fd_close(pmain[0]);

        ctx->pid = pid;
        if (use_aux) {
            ctx->subproc_stdout = ctx->subproc_stderr;
            ctx->subproc_stderr = n00b_channel_fd_open(pty_fd);
            ctx->subproc_stdin  = ctx->subproc_stderr;
        }
        else {
            ctx->subproc_stdout = n00b_channel_fd_open(pty_fd);
            ctx->subproc_stdin  = ctx->subproc_stdout;
        }
        n00b_channel_set_name(ctx->subproc_stdin,
                              n00b_cformat("«#»«#» pty«#»",
                                           n00b_cached_lbracket(),
                                           ctx->cmd,
                                           n00b_cached_rbracket()));

        post_spawn_subscription_setup(ctx);

        if (ctx->subproc_termcap_ptr) {
            n00b_termcap_set(ctx->subproc_termcap_ptr);
        }

        if (ctx->flags & N00B_PROC_HANDLE_WIN_SIZE) {
            n00b_proc_proxy_winch(ctx);
            // n00b_register_signal_handler(SIGWINCH,
            //(void *)n00b_handle_winch,
            //                              ctx);
        }

        fcntl(pty_fd, F_SETFL, flags);

        if (use_aux) {
            n00b_raw_fd_close(subproc_aux);
            fcntl(aux_fd, F_SETFL, flags);
        }

        n00b_unbuffer_stdin();
        n00b_unbuffer_stdout();
        n00b_unbuffer_stderr();

        return;
    }

    n00b_raw_fd_close(pty_fd);

    if (use_aux) {
        n00b_raw_fd_close(aux_fd);
    }
    close(pmain[0]);

    setsid();

    if (ioctl(subproc_fd, TIOCSCTTY, 0)) {
        write(pmain[1], &errno, sizeof(errno));
        _exit(127);
    }

    close(pmain[1]);

    dup2(subproc_fd, 0);
    if (use_aux) {
        dup2(subproc_fd, 2);
        dup2(subproc_aux, 1);
        if (subproc_aux > 2) {
            close(subproc_aux);
        }
    }
    else {
        dup2(subproc_fd, 1);
        dup2(subproc_fd, 2);
    }
    if (subproc_fd > 2) {
        close(subproc_fd);
    }

    n00b_unbuffer_stdin();
    n00b_unbuffer_stdout();
    n00b_unbuffer_stderr();

    signal(SIGHUP, SIG_DFL);
    signal(SIGINT, SIG_DFL);
    signal(SIGILL, SIG_DFL);
    signal(SIGABRT, SIG_DFL);
    signal(SIGFPE, SIG_DFL);
    signal(SIGKILL, SIG_DFL);
    signal(SIGSEGV, SIG_DFL);
    signal(SIGPIPE, SIG_DFL);
    signal(SIGALRM, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
    signal(SIGCHLD, SIG_DFL);
    signal(SIGCONT, SIG_DFL);
    signal(SIGSTOP, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGTTIN, SIG_DFL);
    signal(SIGTTOU, SIG_DFL);
    signal(SIGWINCH, SIG_DFL);

    if (ctx->hook) {
        (*ctx->hook)(ctx->param);
    }
    // Do this after the hook so that it sticks.
    if (ctx->env) {
        envp = (void *)n00b_make_cstr_array(ctx->env, NULL);
    }

    else {
        envp = n00b_raw_envp();
    }

    try_execve(ctx, argp, envp);
}

void
n00b_proc_spawn(n00b_proc_t *ctx)
{
    if (!ctx->cmd) {
        N00B_CRAISE("Cannot spawn; no command set.");
    }

    n00b_condition_lock_acquire(&(ctx->cv));

    if (ctx->flags & N00B_PROC_USE_PTY) {
        proc_spawn_with_tty(ctx);
    }
    else {
        proc_spawn_no_tty(ctx);
    }

    n00b_condition_lock_release(&(ctx->cv));
}

void
n00b_proc_run(n00b_proc_t *ctx, n00b_duration_t *timeout)
{
    n00b_duration_t *end = NULL;
    n00b_duration_t *now;

    if (timeout) {
        now = n00b_now();
        end = n00b_duration_add(now, timeout);
    }

    ctx->wait_for_exit = true;

    if (!ctx->cmd) {
        N00B_CRAISE("Cannot spawn; no command set.");
    }

    if (end) {
        n00b_condition_lock_acquire(&(ctx->cv));
        n00b_proc_spawn(ctx);
        if (!ctx->exited
            && !_n00b_condition_timed_wait(&(ctx->cv),
                                           end,
                                           __FILE__,
                                           __LINE__)) {
            n00b_proc_close(ctx);
        }
    }
    else {
        n00b_condition_lock_acquire(&(ctx->cv));
        n00b_proc_spawn(ctx);
        n00b_condition_wait(&(ctx->cv));
        n00b_proc_close(ctx);
    }
    n00b_condition_lock_release(&(ctx->cv));
}

// TODO: Do a proc_init
// Close stdin after??

n00b_proc_t *
_n00b_run_process(n00b_string_t *cmd,
                  n00b_list_t   *argv,
                  bool           proxy,
                  bool           capture,
                  ...)
{
    n00b_string_t        *stdin_injection = NULL;
    n00b_list_t          *stdout_subs     = NULL;
    n00b_list_t          *stderr_subs     = NULL;
    n00b_list_t          *env             = NULL;
    // Timeout doesn't get used if 'run' is not true.
    n00b_duration_t      *timeout         = NULL;
    n00b_post_fork_hook_t hook            = NULL;
    struct termios       *termcap         = NULL;
    void                 *param           = NULL;
    bool                  pty             = false;
    bool                  raw_argv        = false;
    bool                  run             = true;
    bool                  spawn           = false;
    bool                  merge_output    = true;
    bool                  err_pty         = false;
    bool                  handle_winch    = true;

    n00b_karg_only_init(capture);
    n00b_kw_ptr("stdin_injection", stdin_injection);
    n00b_kw_ptr("env", env);
    n00b_kw_ptr("termcap", termcap);
    n00b_kw_ptr("timeout", timeout);
    n00b_kw_ptr("pre_exec_hook", hook);
    n00b_kw_ptr("hook_param", param);
    n00b_kw_ptr("stdout_subs", stdout_subs);
    n00b_kw_ptr("stderr_subs", stderr_subs);
    n00b_kw_bool("pty", pty);
    n00b_kw_bool("raw_argv", raw_argv);
    n00b_kw_bool("async", spawn);
    n00b_kw_bool("merge", merge_output);
    n00b_kw_bool("err_pty", err_pty);
    n00b_kw_bool("run", run);
    n00b_kw_bool("handle_win_size", handle_winch); // Only if pty is true.

    if (run && spawn) {
        N00B_CRAISE("Cannot both run and spawn.");
    }

    n00b_proc_t *proc = n00b_gc_alloc_mapped(n00b_proc_t, N00B_GC_SCAN_ALL);

    proc->pid                 = -1;
    proc->cmd                 = cmd;
    proc->args                = argv;
    proc->hook                = hook;
    proc->param               = param;
    proc->pending_stdout_subs = stdout_subs;
    proc->pending_stderr_subs = stderr_subs;
    proc->seeded_stdin        = stdin_injection;

    tcgetattr(0, &proc->initial_termcap);
    proc->parent_termcap = &proc->initial_termcap;

    if (termcap) {
        proc->subproc_termcap     = *termcap;
        proc->subproc_termcap_ptr = &proc->subproc_termcap;
    }
    else {
        proc->subproc_termcap_ptr = NULL;
    }

    n00b_named_condition_init(&proc->cv, "process_exit_monitor");

    if (capture) {
        proc->flags = N00B_PROC_CAP_ALL;
    }
    if (proxy) {
        proc->flags |= N00B_PROC_PROXY_ALL;
    }
    if (merge_output) {
        proc->flags |= N00B_PROC_MERGE_OUTPUT;
    }
    if (pty) {
        proc->flags |= N00B_PROC_USE_PTY;
        if (handle_winch) {
            proc->flags |= N00B_PROC_HANDLE_WIN_SIZE;
        }
    }
    if (raw_argv) {
        proc->flags |= N00B_PROC_RAW_ARGV;
    }
    if (err_pty) {
        proc->flags |= N00B_PROC_PTY_STDERR;
    }

    if (spawn) {
        n00b_proc_spawn(proc);
    }
    else {
        if (run) {
            n00b_proc_run(proc, timeout);
        }
    }

    return proc;
}
