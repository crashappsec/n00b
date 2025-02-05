#define N00B_USE_INTERNAL_API
#include "n00b.h"

static void
proc_exited(n00b_exitinfo_t *result, int64_t stats, n00b_proc_t *ctx)
{
    n00b_condition_lock_acquire(&ctx->cv);

    ctx->subproc_results = result;
    ctx->exited          = true;

    if (ctx->wait_for_exit) {
        ctx->exited = true;
        n00b_condition_notify_all(&ctx->cv);
    }
    n00b_condition_lock_release(&ctx->cv);
}

static void
pre_launch_prep(n00b_proc_t *proc, char ***argp, char ***envp)
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

    if (proc->env) {
        *envp = (void *)n00b_make_cstr_array(proc->env, NULL);
    }

    else {
        *envp = n00b_raw_envp();
    }
}

static n00b_stream_t *
setup_exit_obj(n00b_proc_t *ctx)
{
    n00b_list_t *l = NULL;

    if (ctx->subproc_stdout) {
        l = n00b_list(n00b_type_stream());
        n00b_list_append(l, ctx->subproc_stdout);
    }

    if (ctx->subproc_stderr) {
        if (!l) {
            l = n00b_list(n00b_type_stream());
        }
        n00b_list_append(l, ctx->subproc_stderr);
    }

    n00b_stream_t *result   = n00b_pid_monitor(ctx->pid, l);
    n00b_stream_t *callback = n00b_callback_open((void *)proc_exited, ctx);

    n00b_io_set_repr(callback, n00b_cstring("exit (wait4 return)"));
    n00b_io_subscribe(result, callback, NULL, n00b_io_sk_read);

    return result;
}

static void
post_spawn_subscription_setup(n00b_proc_t *ctx)
{
    // We need to lock anything we might be reading from during the
    // subscription process to prevent delivery till we have
    // everything set up.  Just go ahead and lock stdio as a matter of
    // course.

    n00b_acquire_party(ctx->subproc_stdin);
    n00b_acquire_party(ctx->subproc_stdout);
    n00b_acquire_party(ctx->subproc_stderr);

    ctx->subproc_pid = setup_exit_obj(ctx);

    if (ctx->flags & N00B_PROC_STDIN_PROXY) {
        // Proxy parent's stdin to the subproc.
        n00b_io_subscribe_to_reads(n00b_stdin(), ctx->subproc_stdin, NULL);
    }

    if (ctx->flags & (N00B_PROC_STDOUT_PROXY)) {
        n00b_io_subscribe_to_reads(ctx->subproc_stdout,
                                   n00b_stdout(),
                                   NULL);

        if (ctx->flags & N00B_PROC_MERGE_OUTPUT) {
            n00b_io_subscribe_to_reads(ctx->subproc_stderr,
                                       n00b_stdout(),
                                       NULL);
        }
    }

    if ((ctx->flags & N00B_PROC_STDERR_PROXY)
        && !(ctx->flags & N00B_PROC_MERGE_OUTPUT) && ctx->subproc_stderr) {
        n00b_io_subscribe_to_reads(ctx->subproc_stderr,
                                   n00b_stderr(),
                                   NULL);
    }

    n00b_stream_t *buf;

    if (ctx->flags & N00B_PROC_STDIN_CAP) {
        ctx->cap_in = n00b_buffer_empty();
        buf         = n00b_outstream_buffer(ctx->cap_in, true);
        n00b_io_subscribe_to_reads(n00b_stdin(), buf, NULL);
        n00b_io_set_repr(buf,
                         n00b_cformat("«#»«#» stdin cap«#»",
                                      n00b_cached_lbracket(),
                                      ctx->cmd,
                                      n00b_cached_rbracket()));
    }

    if (ctx->flags & N00B_PROC_STDOUT_CAP) {
        ctx->cap_out = n00b_buffer_empty();
        buf          = n00b_outstream_buffer(ctx->cap_out, true);
        n00b_io_subscribe_to_reads(ctx->subproc_stdout, buf, NULL);
        n00b_io_set_repr(buf,
                         n00b_cformat("«#»«#» stdout cap«#»",
                                      n00b_cached_lbracket(),
                                      ctx->cmd,
                                      n00b_cached_rbracket()));

        if (ctx->flags & N00B_PROC_MERGE_OUTPUT) {
            n00b_io_subscribe_to_reads(ctx->subproc_stderr, buf, NULL);
        }
    }

    if (ctx->flags & N00B_PROC_STDERR_CAP
        && !(ctx->flags & N00B_PROC_MERGE_OUTPUT) && ctx->subproc_stderr) {
        ctx->cap_err = n00b_buffer_empty();
        buf          = n00b_outstream_buffer(ctx->cap_err, true);
        n00b_io_subscribe_to_reads(ctx->subproc_stderr, buf, NULL);
        n00b_io_set_repr(buf,
                         n00b_cformat("«#»«#» stderr cap«#»",
                                      n00b_cached_lbracket(),
                                      ctx->cmd,
                                      n00b_cached_rbracket()));
    }

    n00b_release_party(ctx->subproc_stderr);
    n00b_release_party(ctx->subproc_stdout);
    n00b_release_party(ctx->subproc_stdin);
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

    close(filedes[0]);
    return true;
}

static inline bool
close_write_side(bool proxy, int filedes[2])
{
    if (!proxy) {
        return false;
    }

    close(filedes[1]);
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

    pre_launch_prep(ctx, &argp, &envp);

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

        close_read_side(proxy_in, stdin_pipe);
        close_write_side(proxy_out, stdout_pipe);
        close_write_side(proxy_err, stderr_pipe);

        if (proxy_in) {
            ctx->subproc_stdin = n00b_fd_open(stdin_pipe[1]);
            n00b_io_set_repr(ctx->subproc_stdin,
                             n00b_cformat("«#»«#» stdin«#»",
                                          n00b_cached_lbracket(),
                                          ctx->cmd,
                                          n00b_cached_rbracket()));
        }
        if (proxy_out) {
            ctx->subproc_stdout = n00b_fd_open(stdout_pipe[0]);
            n00b_io_set_repr(ctx->subproc_stdout,
                             n00b_cformat("«#»«#» stdout«#»",
                                          n00b_cached_lbracket(),
                                          ctx->cmd,
                                          n00b_cached_rbracket()));
        }
        if (proxy_err) {
            ctx->subproc_stderr = n00b_fd_open(stderr_pipe[0]);
            n00b_io_set_repr(ctx->subproc_stderr,
                             n00b_cformat("«#»«#» stderr«#»",
                                          n00b_cached_lbracket(),
                                          ctx->cmd,
                                          n00b_cached_rbracket()));
        }
        post_spawn_subscription_setup(ctx);

        return;
    }

    if (proxy_in) {
        dup2(stdin_pipe[0], 0);
    }
    if (proxy_out) {
        dup2(stdout_pipe[1], 1);
    }
    if (proxy_err) {
        dup2(stderr_pipe[1], 2);
    }

    try_execve(ctx, argp, envp);
}

static inline void
proc_spawn_with_tty(n00b_proc_t *ctx)
{
    struct winsize  wininfo;
    struct termios *term_ptr = ctx->subproc_termcap;
    struct winsize *win_ptr  = &wininfo;
    bool            proxy_in = ctx->flags & N00B_PROC_STDIN_MASK;
    /*
    bool            proxy_out = ctx->flags & N00B_PROC_STDOUT_MASK;
    bool            proxy_err = ctx->flags & N00B_PROC_STDERR_MASK;
    */
    int             stdin_pipe[2];
    //    int             stderr_pipe[2];
    int             pty_fd;
    pid_t           pid;
    char          **argp;
    char          **envp;

    pre_launch_prep(ctx, &argp, &envp);
    tcgetattr(0, &ctx->initial_termcap);

    open_pipe(true, stdin_pipe);

    if (n00b_is_tty(n00b_stdin())) {
        win_ptr = NULL;
    }
    else {
        ioctl(0, TIOCGWINSZ, win_ptr);
    }

    pid = forkpty(&pty_fd, NULL, term_ptr, win_ptr);

    int flags = fcntl(pty_fd, F_GETFL) | O_NONBLOCK;
    fcntl(pty_fd, F_SETFL, flags);

    if (pid != 0) {
        ctx->pid            = pid;
        ctx->subproc_stdout = n00b_fd_open(pty_fd);
        ctx->subproc_stdin  = ctx->subproc_stdout;

        n00b_io_set_repr(ctx->subproc_stdin,
                         n00b_cformat("«#»«#» pty«#»",
                                      n00b_cached_lbracket(),
                                      ctx->cmd,
                                      n00b_cached_rbracket()));

        close_read_side(proxy_in, stdin_pipe);
        post_spawn_subscription_setup(ctx);

        if (!ctx->parent_termcap) {
            n00b_termcap_apply_raw_mode(&ctx->initial_termcap);
            n00b_termcap_set(&ctx->initial_termcap);
        }
        else {
            n00b_termcap_set(ctx->parent_termcap);
        }
        return;
    }

    setvbuf(stdout, NULL, _IONBF, (size_t)0);
    setvbuf(stdin, NULL, _IONBF, (size_t)0);

    if (close_write_side(proxy_in, stdin_pipe)) {
        dup2(stdin_pipe[0], 0);
    }

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
#if 0
    n00b_duration_t *end;
    n00b_duration_t *now;

    // TODO: timeout.
    if (timeout) {
        now = n00b_now();
        end = n00b_duration_add(now, timeout);
    }
#endif
    ctx->wait_for_exit = true;

    if (!ctx->cmd) {
        N00B_CRAISE("Cannot spawn; no command set.");
    }

    n00b_condition_wait(&(ctx->cv), n00b_proc_spawn(ctx));
    n00b_condition_lock_release(&(ctx->cv));
}

// TODO: Do a proc_init

n00b_proc_t *
_n00b_run_process(n00b_string_t *cmd,
                  n00b_list_t   *argv,
                  bool           proxy,
                  bool           capture,
                  ...)
{
    n00b_list_t     *env          = NULL;
    n00b_duration_t *timeout      = NULL;
    bool             pty          = false;
    bool             raw_argv     = false;
    bool             run          = true;
    bool             spawn        = false;
    bool             merge_output = false;

    n00b_karg_only_init(capture);
    n00b_kw_ptr("env", env);
    n00b_kw_ptr("timeout", timeout);
    n00b_kw_bool("pty", pty);
    n00b_kw_bool("raw_argv", raw_argv);
    n00b_kw_bool("run", run);
    n00b_kw_bool("spawn", spawn);
    n00b_kw_bool("merge", merge_output);

    if (run && spawn) {
        N00B_CRAISE("Cannot both run and spawn.");
    }

    n00b_proc_t *proc = n00b_gc_alloc_mapped(n00b_proc_t, N00B_GC_SCAN_ALL);

    proc->pid  = -1;
    proc->cmd  = cmd;
    proc->args = argv;

    n00b_raw_condition_init(&proc->cv);

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
    }
    if (raw_argv) {
        proc->flags |= N00B_PROC_RAW_ARGV;
    }

    if (run) {
        n00b_proc_run(proc, timeout);
    }
    else {
        if (spawn) {
            n00b_proc_spawn(proc);
        }
    }

    return proc;
}
