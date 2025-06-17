#define N00B_USE_INTERNAL_API
#include "n00b.h"

void
n00b_proc_proxy_winch(n00b_proc_t *ctx)
{
    int fd;

    ioctl(1, TIOCGWINSZ, &ctx->dimensions);
    if (ctx->subproc_stdout) {
        fd = n00b_stream_fileno(ctx->subproc_stdout);
        ioctl(fd, TIOCSWINSZ, &ctx->dimensions);
    }

    if (ctx->subproc_stderr) {
        fd = n00b_stream_fileno(ctx->subproc_stderr);
        ioctl(fd, TIOCSWINSZ, &ctx->dimensions);
    }
}

static void
n00b_handle_winch(n00b_stream_t *sig, int64_t signal, n00b_proc_t *ctx)
{
    n00b_proc_proxy_winch(ctx);
}

static void
proc_exited(n00b_exit_info_t *result, n00b_proc_t *ctx)
{
    ctx->subproc_results = result;
    ctx->exited          = true;

    if (ctx->wait_for_exit) {
        n00b_condition_notify_all(&ctx->cv);
        n00b_lock_release(&ctx->cv);
    }
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

#if defined(N00B_DEBUG) && N00B_DLOG_IO_LEVEL >= 1

    n00b_dlog_io1("Spawn: cmd = %s", proc->cmd->data);
    for (int i = 0; i < n00b_list_len(proc->args); i++) {
        n00b_string_t *s = n00b_list_get(proc->args, i, NULL);
        n00b_dlog_io1("arg %d: %s", i, s->data);
    }
#endif

    *argp = (void *)n00b_make_cstr_array(args, NULL);
}

static n00b_stream_t *
setup_exit_obj(n00b_proc_t *ctx)
{
    n00b_stream_t *result   = n00b_new_exit_stream(ctx->pid);
    n00b_stream_t *callback = n00b_new_callback_stream((void *)proc_exited,
                                                       ctx);
    n00b_stream_set_name(callback, n00b_cstring("wait4 exit"));
    n00b_stream_subscribe_read(result, callback, true);

    ctx->exit_cb = callback;

    return result;
}

static inline n00b_list_t *
bulk_subscribe(n00b_stream_t *target, n00b_list_t *subs)
{
    n00b_list_t *result;

    if (!n00b_list_len(subs)) {
        return NULL;
    }
    // Let's make sure internal subscriptions run first.
    n00b_list_reverse(subs);

    // The list of streams to subscribe becomes a list of
    // subscription objects we can use to unsubscribe,
    // once this call completes.
    result = n00b_observable_add_subscribers(&target->pub_info,
                                             (void *)(int64_t)N00B_CT_R,
                                             n00b_route_stream_message,
                                             subs,
                                             false);

    for (int i = 0; i < n00b_list_len(subs); i++) {
        n00b_stream_t   *dst = n00b_list_get(subs, i, NULL);
        n00b_observer_t *sub = n00b_list_get(result, i, NULL);

        n00b_list_append(dst->my_subscriptions, sub);
    }

    return result;
}

static void
post_spawn_subscription_setup(n00b_proc_t *ctx)
{
    ctx->subproc_pid = setup_exit_obj(ctx);

    if (ctx->seeded_stdin && ctx->subproc_stdin) {
        n00b_queue(ctx->subproc_stdin,
                   n00b_string_to_buffer(ctx->seeded_stdin));
        ctx->seeded_stdin = NULL;
    }

    n00b_buf_t *buf;

    if (ctx->flags & N00B_PROC_STDIN_CAP) {
        buf         = n00b_buffer_empty();
        ctx->cap_in = n00b_outstream_buffer(buf, true);
        n00b_list_append(ctx->stdin_subs, ctx->cap_in);
        n00b_stream_set_name(ctx->cap_in,
                             n00b_cformat("«#»«#» stdin cap«#»",
                                          n00b_cached_lbracket(),
                                          ctx->cmd,
                                          n00b_cached_rbracket()));
    }

    if (ctx->flags & N00B_PROC_STDOUT_CAP) {
        buf          = n00b_buffer_empty();
        ctx->cap_out = n00b_outstream_buffer(buf, true);
        n00b_list_append(ctx->stdout_subs, ctx->cap_out);
        n00b_stream_set_name(ctx->cap_out,
                             n00b_cformat("«#»«#» stdout cap«#»",
                                          n00b_cached_lbracket(),
                                          ctx->cmd,
                                          n00b_cached_rbracket()));

        if (ctx->flags & N00B_PROC_MERGE_OUTPUT && ctx->subproc_stderr) {
            n00b_list_append(ctx->stderr_subs, ctx->cap_out);
        }
    }

    if (ctx->flags & N00B_PROC_STDERR_CAP
        && !(ctx->flags & N00B_PROC_MERGE_OUTPUT) && ctx->subproc_stderr) {
        buf          = n00b_buffer_empty();
        ctx->cap_err = n00b_outstream_buffer(buf, true);
        n00b_list_append(ctx->stderr_subs, ctx->cap_err);
        n00b_stream_set_name(ctx->cap_err,
                             n00b_cformat("«#»«#» stderr cap«#»",
                                          n00b_cached_lbracket(),
                                          ctx->cmd,
                                          n00b_cached_rbracket()));
    }

    if (ctx->flags & N00B_PROC_STDIN_PROXY) {
        // Proxy parent's stdin to the subproc.
        n00b_list_append(ctx->stdin_subs, ctx->subproc_stdin);
    }

    if (ctx->flags & (N00B_PROC_STDOUT_PROXY)) {
        n00b_list_append(ctx->stdout_subs, n00b_stdout());

        if (ctx->flags & N00B_PROC_MERGE_OUTPUT && ctx->subproc_stderr
            && ctx->subproc_stderr != ctx->subproc_stdout) {
            n00b_list_append(ctx->stderr_subs, n00b_stdout());
        }
    }

    if ((ctx->flags & N00B_PROC_STDERR_PROXY)
        && !(ctx->flags & N00B_PROC_MERGE_OUTPUT) && ctx->subproc_stderr) {
        n00b_list_append(ctx->stderr_subs, n00b_stderr());
    }

    ctx->stdin_subs  = bulk_subscribe(n00b_stdin(), ctx->stdin_subs);
    ctx->stdout_subs = bulk_subscribe(ctx->subproc_stdout, ctx->stdout_subs);
    ctx->stderr_subs = bulk_subscribe(ctx->subproc_stderr, ctx->stderr_subs);

    write(ctx->gate, "x", 1);
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
    char  buf[1];

    read(ctx->gate, buf, 1);

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
    int    gate[2];
    char **argp;
    char **envp;

    pre_launch_prep(ctx, &argp);

    pipe(gate);

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
        ctx->gate = gate[1];
        close(gate[0]);
        ctx->pid = pid;

        if (proxy_in) {
            ctx->subproc_stdin = n00b_stream_fd_open(stdin_pipe[1]);
        }
        if (proxy_out) {
            ctx->subproc_stdout = n00b_stream_fd_open(stdout_pipe[0]);
        }
        if (proxy_err) {
            ctx->subproc_stderr = n00b_stream_fd_open(stderr_pipe[0]);
        }
        post_spawn_subscription_setup(ctx);

        return;
    }

    ctx->gate = gate[0];
    close(gate[1]);
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
should_exit_via_sig(int signal, siginfo_t *info, n00b_proc_t *ctx)
{
    n00b_lock_acquire(&ctx->cv);
    n00b_proc_close(ctx);
    ctx->exited = true;
    if (ctx->wait_for_exit) {
        n00b_condition_notify_all(&ctx->cv);
    }
    n00b_lock_release(&ctx->cv);
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
    int             gate[2];
    n00b_signal_register(SIGCHLD, (void *)should_exit_via_sig, ctx);
    n00b_signal_register(SIGPIPE, (void *)should_exit_via_sig, ctx);

    pre_launch_prep(ctx, &argp);

    if (!n00b_stream_is_tty(n00b_stdin())) {
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

        ctx->subproc_stderr = n00b_stream_fd_open(aux_fd);
    }

    int pmain[2];

    if (pipe(pmain)) {
        n00b_raise_errno();
    }

    if (pipe(gate)) {
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

        close(gate[0]);
        ctx->gate = gate[1];

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
            ctx->subproc_stderr = n00b_stream_fd_open(pty_fd);
            ctx->subproc_stdin  = ctx->subproc_stderr;
        }
        else {
            ctx->subproc_stdout = n00b_stream_fd_open(pty_fd);
            ctx->subproc_stdin  = ctx->subproc_stdout;
        }
        n00b_stream_set_name(ctx->subproc_stdin,
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
            n00b_signal_register(SIGWINCH,
                                 (void *)n00b_handle_winch,
                                 ctx);
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

    close(gate[1]);
    ctx->gate = gate[0];

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
    //    n00b_signal_register(SIGCHLD, (void *)should_exit_via_sig, ctx);
    //    n00b_signal_register(SIGPIPE, (void *)should_exit_via_sig, ctx);

    if (!ctx->cmd) {
        N00B_CRAISE("Cannot spawn; no command set.");
    }

    if (ctx->flags & N00B_PROC_USE_PTY) {
        proc_spawn_with_tty(ctx);
    }
    else {
        proc_spawn_no_tty(ctx);
    }
}

// Returns false if there's a timeout.
bool
n00b_proc_run(n00b_proc_t *ctx, n00b_duration_t *timeout)
{
    int64_t ns_tout = 0;

    if (timeout) {
        ns_tout = n00b_ns_from_duration(timeout);
    }

    ctx->wait_for_exit = true;

    if (!ctx->cmd) {
        N00B_CRAISE("Cannot spawn; no command set.");
    }

    n00b_lock_acquire(&ctx->cv);
    n00b_proc_spawn(ctx);
    // clang-format off
    bool result = !(bool)n00b_condition_wait(&ctx->cv,
                                             timeout     : ns_tout,
                                             auto_unlock : true);
    // clang-format on
    n00b_proc_close(ctx);
    return result;
}

// TODO: Do a proc_init
// Close stdin after??

typedef struct {
    int64_t location;
    char   *name;
    bool    found;
} ka_info;

n00b_proc_t *
_n00b_run_process(n00b_string_t *cmd,
                  n00b_list_t   *argv,
                  bool           proxy,
                  bool           capture,
                  ...)
{
    keywords
    {
        n00b_string_t        *stdin_injection     = NULL;
        n00b_list_t          *stdout_subs         = NULL;
        n00b_list_t          *stderr_subs         = NULL;
        struct termios       *termcap             = NULL;
        // n00b_list_t          *env              = NULL;
        //  Timeout doesn't get used if 'run' is not true.
        n00b_duration_t      *timeout             = NULL;
        n00b_post_fork_hook_t hook(pre_exec_hook) = NULL;
        void                 *param(hook_param)   = NULL;
        bool                  pty                 = false;
        bool                  raw_argv            = false;
        bool                  run                 = true;
        bool                  spawn(async)        = false;
        bool                  merge_output(merge) = true;
        bool                  err_pty             = false;
        bool                  handle_win_size     = true;
    }

    if (run && spawn) {
        N00B_CRAISE("Cannot both run and spawn.");
    }

    n00b_proc_t *proc = n00b_gc_alloc_mapped(n00b_proc_t, N00B_GC_SCAN_ALL);

    proc->pid          = -1;
    proc->cmd          = cmd;
    proc->args         = argv;
    proc->hook         = hook;
    proc->param        = param;
    proc->stdout_subs  = stdout_subs;
    proc->stderr_subs  = stderr_subs;
    proc->seeded_stdin = stdin_injection;

    if (!proc->stdout_subs) {
        proc->stdout_subs = n00b_list(n00b_type_stream());
    }

    if (!proc->stderr_subs) {
        proc->stderr_subs = n00b_list(n00b_type_stream());
    }

    proc->stdin_subs = n00b_list(n00b_type_stream());

    tcgetattr(0, &proc->initial_termcap);
    proc->parent_termcap = &proc->initial_termcap;

    if (termcap) {
        proc->subproc_termcap     = *termcap;
        proc->subproc_termcap_ptr = &proc->subproc_termcap;
    }
    else {
        proc->subproc_termcap_ptr = NULL;
    }

    n00b_named_lock_init(&proc->cv, N00B_NLT_CV, "process_exit_monitor");

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
        if (handle_win_size) {
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

void
n00b_proc_close(n00b_proc_t *proc)
{
    if (proc->subproc_stdin) {
        n00b_close(proc->subproc_stdin);
    }
    if (proc->subproc_stdout) {
        n00b_close(proc->subproc_stdout);
    }
    if (proc->subproc_stderr) {
        n00b_close(proc->subproc_stderr);
    }
    if (proc->subproc_pid) {
        n00b_close(proc->subproc_pid);
    }
    if (proc->cap_in) {
        n00b_close(proc->cap_in);
    }
    if (proc->cap_out) {
        n00b_close(proc->cap_out);
    }
    if (proc->cap_err) {
        n00b_close(proc->cap_err);
    }

    if (proc->exit_cb) {
        n00b_close(proc->exit_cb);
    }

    n00b_signal_unregister(SIGCHLD, (void *)should_exit_via_sig, proc);
    n00b_signal_unregister(SIGPIPE, (void *)should_exit_via_sig, proc);

    if (proc->flags & N00B_PROC_USE_PTY) {
        n00b_signal_unregister(SIGWINCH,
                               (void *)n00b_handle_winch,
                               proc);
    }
}
