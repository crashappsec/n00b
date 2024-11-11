/*
 * Currently, we're using select() here, not epoll(), etc.
 */

#include "n00b.h"

/*
 * Initializes a `subprocess` context, setting the process to spawn.
 * By default, it will *not* be run on a pty; call `n00b_subproc_use_pty()`
 * before calling `n00b_subproc_run()` in order to turn that on.
 *
 * By default, the process will run QUIETLY, without any capture or
 * passthrough of IO.  See `n00b_subproc_set_passthrough()` for routing IO
 * between the subprocess and the parent, and `n00b_subproc_set_capture()`
 * for capturing output from the subprocess (or from your terminal).
 *
 * This does not take ownership of the strings passed in, and doesn't
 * use them until you call n00b_subproc_run(). In general, don't free
 * anything passed into this API until the process is done.
 */
void
n00b_subproc_init(n00b_subproc_t *ctx,
                  char           *cmd,
                  char           *argv[],
                  bool            proxy_stdin_close)
{
    memset(ctx, 0, sizeof(n00b_subproc_t));
    n00b_sb_init(&ctx->sb, N00B_IO_HEAP_SZ);
    ctx->cmd               = cmd;
    ctx->argv              = argv;
    ctx->capture           = 0;
    ctx->passthrough       = 0;
    ctx->proxy_stdin_close = proxy_stdin_close;

    n00b_sb_init_party_fd(&ctx->sb,
                          &ctx->parent_stdin,
                          0,
                          O_RDONLY,
                          false,
                          false,
                          false);
    n00b_sb_init_party_fd(&ctx->sb,
                          &ctx->parent_stdout,
                          1,
                          O_WRONLY,
                          false,
                          false,
                          false);
    n00b_sb_init_party_fd(&ctx->sb,
                          &ctx->parent_stderr,
                          2,
                          O_WRONLY,
                          false,
                          false,
                          false);
}

/*
 * By default, we pass through the environment. Use this to set your own
 * environment.
 */
bool
n00b_subproc_set_envp(n00b_subproc_t *ctx, char *envp[])
{
    if (ctx->run) {
        return false;
    }

    ctx->envp = envp;

    return true;
}

/*
 * This function passes the given string to the subprocess via
 * stdin. You can set this once before calling `n00b_subproc_run()`; but
 * after you've called `n00b_subproc_run()`, you can call this as many
 * times as you like, as long as the subprocess is open and its stdin
 * file descriptor hasn't been closed.
 */
bool
n00b_subproc_pass_to_stdin(n00b_subproc_t *ctx,
                           char           *str,
                           size_t          len,
                           bool            close_fd)
{
    if (ctx->str_waiting || ctx->sb.done) {
        return false;
    }

    if (ctx->run && close_fd) {
        return false;
    }

    n00b_sb_init_party_input_buf(&ctx->sb,
                                 &ctx->str_stdin,
                                 str,
                                 len,
                                 true,
                                 close_fd);

    if (ctx->run) {
        return n00b_sb_route(&ctx->sb, &ctx->str_stdin, &ctx->subproc_stdin);
    }
    else {
        ctx->str_waiting = true;

        if (close_fd) {
            ctx->pty_stdin_pipe = true;
        }

        return true;
    }
}

/*
 * This controls whether I/O gets proxied between the parent process
 * and the subprocess.
 *
 * The `which` parameter should be some combination of the following
 * flags:
 *
 * N00B_SP_IO_STDIN   (what you type goes to subproc stdin)
 * N00B_SP_IO_STDOUT  (subproc's stdout gets written to your stdout)
 * N00B_SP_IO_STDERR
 *
 * N00B_SP_IO_ALL proxies everything.
 * It's fine to use this even if no pty is used.
 *
 * If `combine` is true, then all subproc output for any proxied streams
 * will go to STDOUT.
 */
bool
n00b_subproc_set_passthrough(n00b_subproc_t *ctx,
                             unsigned char   which,
                             bool            combine)
{
    if (ctx->run || which > N00B_SP_IO_ALL) {
        return false;
    }

    ctx->passthrough      = which;
    ctx->pt_all_to_stdout = combine;

    return true;
}
/*
 * This controls whether input from a file descriptor is captured into
 * a string that is available when the process ends.
 *
 * You can capture any stream, including what the user's typing on stdin.
 *
 * The `which` parameter should be some combination of the following
 * flags:
 *
 * N00B_SP_IO_STDIN   (what you type); reference for string is "stdin"
 * N00B_SP_IO_STDOUT  reference for string is "stdout"
 * N00B_SP_IO_STDERR  reference for string is "stderr"
 *
 * N00B_SP_IO_ALL captures everything.
 It's fine to use this even if no pty is used.
 *
 * If `combine` is true, then all subproc output for any streams will
 * be combined into "stdout".  Retrieve from the `n00b_capture_result_t` object
 * returned from `n00b_subproc_run()`, using the sp_result_...() api.
 */
bool
n00b_subproc_set_capture(n00b_subproc_t *ctx, unsigned char which, bool combine)
{
    if (ctx->run || which > N00B_SP_IO_ALL) {
        return false;
    }

    ctx->capture          = which;
    ctx->pt_all_to_stdout = combine;

    return true;
}

void
n00b_deferred_cb_gc_bits(uint64_t *bitmap, n00b_deferred_cb_t *cb)
{
    n00b_mark_raw_to_addr(bitmap, cb, &cb->to_free);
}

bool
n00b_subproc_set_io_callback(n00b_subproc_t *ctx,
                             unsigned char   which,
                             n00b_sb_cb_t    cb)
{
    if (ctx->run || which > N00B_SP_IO_ALL) {
        return false;
    }

    n00b_deferred_cb_t *cbinfo = n00b_gc_alloc_mapped(n00b_deferred_cb_t,
                                                      n00b_deferred_cb_gc_bits);

    cbinfo->next  = ctx->deferred_cbs;
    cbinfo->which = which;
    cbinfo->cb    = cb;

    ctx->deferred_cbs = cbinfo;

    return true;
}

/*
 * This sets how long to wait in `select()` for file-descriptors to be
 * ready with data to read. If you don't set this, there will be no
 * timeout, and it's possible for the process to die and for the file
 * descriptors associated with them to never return ready.
 *
 * If you have a timeout, a progress callback can be called.
 *
 * Also, when the process is not blocked on the select(), right before
 * the next select we check the status of the subprocess. If it's
 * returned and all its descriptors are marked as closed, and no
 * descriptors that are open are waiting to write, then the subprocess
 * switchboard will exit.
 */
void
n00b_subproc_set_timeout(n00b_subproc_t *ctx, struct timeval *timeout)
{
    n00b_sb_set_io_timeout(&ctx->sb, timeout);
}

/*
 * Removes any set timeout.
 */
void
n00b_subproc_clear_timeout(n00b_subproc_t *ctx)
{
    n00b_sb_clear_io_timeout(&ctx->sb);
}

/*
 * When called before n00b_subproc_run(), will spawn the child process on
 * a pseudo-terminal.
 */
bool
n00b_subproc_use_pty(n00b_subproc_t *ctx)
{
    if (ctx->run) {
        return false;
    }
    ctx->use_pty = true;
    return true;
}

bool
n00b_subproc_set_startup_callback(n00b_subproc_t *ctx, void (*cb)(void *))
{
    ctx->startup_callback = cb;
    return true;
}

int
n00b_subproc_get_pty_fd(n00b_subproc_t *ctx)
{
    return ctx->pty_fd;
}

void
n00b_subproc_pause_passthrough(n00b_subproc_t *ctx, unsigned char which)
{
    /*
     * Since there's no real consequence to trying to pause a
     * subscription that doesn't exist, we'll just try to pause every
     * subscription implied by `which`. Note that if we see stderr, we
     * try to unsubscribe it from both the parent's stdout and the
     * parent's stderr; no strong need to care whether they were
     * combined or not here..
     */

    if (which & N00B_SP_IO_STDIN) {
        if (ctx->pty_fd) {
            n00b_sb_pause_route(&ctx->sb,
                                &ctx->parent_stdin,
                                &ctx->subproc_stdout);
        }
        else {
            n00b_sb_pause_route(&ctx->sb,
                                &ctx->parent_stdin,
                                &ctx->subproc_stdin);
        }
    }
    if (which & N00B_SP_IO_STDOUT) {
        n00b_sb_pause_route(&ctx->sb,
                            &ctx->subproc_stdout,
                            &ctx->parent_stdout);
    }
    if (!ctx->pty_fd && (which & N00B_SP_IO_STDERR)) {
        n00b_sb_pause_route(&ctx->sb,
                            &ctx->subproc_stderr,
                            &ctx->parent_stdout);
        n00b_sb_pause_route(&ctx->sb,
                            &ctx->subproc_stderr,
                            &ctx->parent_stderr);
    }
}

void
n00b_subproc_resume_passthrough(n00b_subproc_t *ctx, unsigned char which)
{
    /*
     * Since there's no real consequence to trying to pause a
     * subscription that doesn't exist, we'll just try to pause every
     * subscription implied by `which`. Note that if we see stderr, we
     * try to unsubscribe it from both the parent's stdout and the
     * parent's stderr; no strong need to care whether they were
     * combined or not here..
     */

    if (which & N00B_SP_IO_STDIN) {
        if (ctx->pty_fd) {
            n00b_sb_resume_route(&ctx->sb,
                                 &ctx->parent_stdin,
                                 &ctx->subproc_stdout);
        }
        else {
            n00b_sb_resume_route(&ctx->sb,
                                 &ctx->parent_stdin,
                                 &ctx->subproc_stdin);
        }
    }
    if (which & N00B_SP_IO_STDOUT) {
        n00b_sb_resume_route(&ctx->sb,
                             &ctx->subproc_stdout,
                             &ctx->parent_stdout);
    }
    if (!ctx->pty_fd && (which & N00B_SP_IO_STDERR)) {
        n00b_sb_resume_route(&ctx->sb,
                             &ctx->subproc_stderr,
                             &ctx->parent_stdout);
        n00b_sb_resume_route(&ctx->sb,
                             &ctx->subproc_stderr,
                             &ctx->parent_stderr);
    }
}

void
n00b_subproc_pause_capture(n00b_subproc_t *ctx, unsigned char which)
{
    if (which & N00B_SP_IO_STDIN) {
        n00b_sb_pause_route(&ctx->sb,
                            &ctx->parent_stdin,
                            &ctx->capture_stdin);
    }

    if (which & N00B_SP_IO_STDOUT) {
        n00b_sb_pause_route(&ctx->sb,
                            &ctx->subproc_stdout,
                            &ctx->capture_stdout);
    }

    if ((which & N00B_SP_IO_STDERR) && !ctx->pty_fd) {
        n00b_sb_pause_route(&ctx->sb,
                            &ctx->subproc_stderr,
                            &ctx->capture_stdout);
        n00b_sb_pause_route(&ctx->sb,
                            &ctx->subproc_stderr,
                            &ctx->capture_stderr);
    }
}

void
n00b_subproc_resume_capture(n00b_subproc_t *ctx, unsigned char which)
{
    if (which & N00B_SP_IO_STDIN) {
        n00b_sb_resume_route(&ctx->sb,
                             &ctx->parent_stdin,
                             &ctx->capture_stdin);
    }

    if (which & N00B_SP_IO_STDOUT) {
        n00b_sb_resume_route(&ctx->sb,
                             &ctx->subproc_stdout,
                             &ctx->capture_stdout);
    }

    if ((which & N00B_SP_IO_STDERR) && !ctx->pty_fd) {
        n00b_sb_resume_route(&ctx->sb,
                             &ctx->subproc_stderr,
                             &ctx->capture_stdout);
        n00b_sb_resume_route(&ctx->sb,
                             &ctx->subproc_stderr,
                             &ctx->capture_stderr);
    }
}

static void
setup_subscriptions(n00b_subproc_t *ctx, bool pty)
{
    n00b_party_t *stderr_dst = &ctx->parent_stderr;

    if (ctx->pt_all_to_stdout) {
        stderr_dst = &ctx->parent_stdout;
    }

    if (ctx->passthrough) {
        if (ctx->passthrough & N00B_SP_IO_STDIN) {
            if (pty) {
                // in pty, ctx->subproc_stdout is the same FD used for stdin
                // as its the same r/w FD for both
                n00b_sb_route(&ctx->sb,
                              &ctx->parent_stdin,
                              &ctx->subproc_stdout);
            }
            else {
                n00b_sb_route(&ctx->sb,
                              &ctx->parent_stdin,
                              &ctx->subproc_stdin);
            }
        }
        if (ctx->passthrough & N00B_SP_IO_STDOUT) {
            n00b_sb_route(&ctx->sb,
                          &ctx->subproc_stdout,
                          &ctx->parent_stdout);
        }
        if (!pty && ctx->passthrough & N00B_SP_IO_STDERR) {
            n00b_sb_route(&ctx->sb,
                          &ctx->subproc_stderr,
                          stderr_dst);
        }
    }

    if (ctx->capture) {
        if (ctx->capture & N00B_SP_IO_STDIN) {
            n00b_sb_init_party_output_buf(&ctx->sb,
                                          &ctx->capture_stdin,
                                          "stdin",
                                          N00B_CAP_ALLOC);
        }
        if (ctx->capture & N00B_SP_IO_STDOUT) {
            n00b_sb_init_party_output_buf(&ctx->sb,
                                          &ctx->capture_stdout,
                                          "stdout",
                                          N00B_CAP_ALLOC);
        }

        if (ctx->combine_captures) {
            if (!(ctx->capture & N00B_SP_IO_STDOUT) && ctx->capture & N00B_SP_IO_STDERR) {
                if (ctx->capture & N00B_SP_IO_STDOUT) {
                    n00b_sb_init_party_output_buf(&ctx->sb,
                                                  &ctx->capture_stdout,
                                                  "stdout",
                                                  N00B_CAP_ALLOC);
                }
            }

            stderr_dst = &ctx->capture_stdout;
        }
        else {
            if (!pty && ctx->capture & N00B_SP_IO_STDERR) {
                n00b_sb_init_party_output_buf(&ctx->sb,
                                              &ctx->capture_stderr,
                                              "stderr",
                                              N00B_CAP_ALLOC);
            }

            stderr_dst = &ctx->capture_stderr;
        }

        if (ctx->capture & N00B_SP_IO_STDIN) {
            n00b_sb_route(&ctx->sb, &ctx->parent_stdin, &ctx->capture_stdin);
        }
        if (ctx->capture & N00B_SP_IO_STDOUT) {
            n00b_sb_route(&ctx->sb, &ctx->subproc_stdout, &ctx->capture_stdout);
        }
        if (!pty && ctx->capture & N00B_SP_IO_STDERR) {
            n00b_sb_route(&ctx->sb, &ctx->subproc_stderr, stderr_dst);
        }
    }

    if (ctx->str_waiting) {
        n00b_sb_route(&ctx->sb, &ctx->str_stdin, &ctx->subproc_stdin);
        ctx->str_waiting = false;
    }

    // Make sure calls to the API know we've started!
    ctx->run = true;
}

static void
n00b_subproc_do_exec(n00b_subproc_t *ctx)
{
    char  **p = ctx->argv;
    int64_t i = 0;

    while (*p != NULL) {
        n00b_printf("[h2]Arg {} =[/] {}", i++, n00b_new_utf8(*p++));
    }

    if (ctx->envp) {
        execve(ctx->cmd, ctx->argv, ctx->envp);
    }
    else {
        execv(ctx->cmd, ctx->argv);
    }
    // If we get past the exec, kill the subproc with non-zero exit code,
    // which will tear down the switchboard and print to stderr the
    // errono description. For example for nonexisting command will be:
    // foo: No such file or directory
    fprintf(stderr, "%s: %s\n", ctx->cmd, strerror(errno));
    // TODO switch back to abort() once better event handling is implemented
    // in switchboard to correctly detect exit code as otherwise waitpid()
    // detects program has exited however not all signal handlers have executed
    // hence exit code is unknown yet
    exit(1);
}

n00b_party_t *
n00b_subproc_new_party_callback(n00b_switchboard_t *ctx, n00b_sb_cb_t cb)
{
    n00b_party_t *result = n00b_new_party();
    n00b_sb_init_party_callback(ctx, result, cb);

    return result;
}

static void
n00b_subproc_install_callbacks(n00b_subproc_t *ctx)
{
    n00b_deferred_cb_t *entry = ctx->deferred_cbs;

    while (entry) {
        entry->to_free = n00b_subproc_new_party_callback(&ctx->sb, entry->cb);
        if (entry->which & N00B_SP_IO_STDIN) {
            n00b_sb_route(&ctx->sb, &ctx->parent_stdin, entry->to_free);
        }
        if (entry->which & N00B_SP_IO_STDOUT) {
            n00b_sb_route(&ctx->sb, &ctx->subproc_stdout, entry->to_free);
        }
        if (entry->which & N00B_SP_IO_STDERR) {
            n00b_sb_route(&ctx->sb, &ctx->subproc_stderr, entry->to_free);
        }
        entry = entry->next;
    }
}

static void
run_startup_callback(n00b_subproc_t *ctx)
{
    if (ctx->startup_callback) {
        (*ctx->startup_callback)(ctx);
    }
}

static void
n00b_subproc_spawn_fork(n00b_subproc_t *ctx)
{
    pid_t pid;
    int   stdin_pipe[2];
    int   stdout_pipe[2];
    int   stderr_pipe[2];

    if (pipe(stdin_pipe) || pipe(stdout_pipe) || pipe(stderr_pipe)) {
        N00B_CRAISE("Could not open pipe.");
    }

    pid = fork();

    if (pid != 0) {
        close(stdin_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

        n00b_sb_init_party_fd(&ctx->sb,
                              &ctx->subproc_stdin,
                              stdin_pipe[1],
                              O_WRONLY,
                              false,
                              true,
                              ctx->proxy_stdin_close);
        n00b_sb_init_party_fd(&ctx->sb,
                              &ctx->subproc_stdout,
                              stdout_pipe[0],
                              O_RDONLY,
                              false,
                              true,
                              false);
        n00b_sb_init_party_fd(&ctx->sb,
                              &ctx->subproc_stderr,
                              stderr_pipe[0],
                              O_RDONLY,
                              false,
                              true,
                              false);

        n00b_sb_monitor_pid(&ctx->sb,
                            pid,
                            &ctx->subproc_stdin,
                            &ctx->subproc_stdout,
                            &ctx->subproc_stderr,
                            true);
        n00b_subproc_install_callbacks(ctx);
        setup_subscriptions(ctx, false);
        run_startup_callback(ctx);
    }
    else {
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);
        dup2(stdin_pipe[0], 0);
        dup2(stdout_pipe[1], 1);
        dup2(stderr_pipe[1], 2);

        n00b_subproc_do_exec(ctx);
    }
}

static void
n00b_subproc_spawn_forkpty(n00b_subproc_t *ctx)
{
    struct winsize  wininfo;
    struct termios *term_ptr = ctx->child_termcap;
    struct winsize *win_ptr  = &wininfo;
    pid_t           pid;
    int             pty_fd;
    int             stdin_pipe[2];

    tcgetattr(0, &ctx->saved_termcap);

    if (ctx->pty_stdin_pipe) {
        if (pipe(stdin_pipe)) {
            N00B_CRAISE("pipe() failed.");
        }
    }

    // We're going to use a pipe for stderr to get a separate
    // stream. The tty FD will be stdin and stdout for the child
    // process.
    //
    // Also, if we want to close the subproc's stdin after an initial
    // write, we will dup2.
    //
    // Note that this means the child process will see isatty() return
    // true for stdin and stdout, but not stderr.
    if (!isatty(0)) {
        win_ptr = NULL;
    }
    else {
        ioctl(0, TIOCGWINSZ, win_ptr);
    }

    pid = forkpty(&pty_fd, NULL, term_ptr, win_ptr);

    if (pid != 0) {
        if (ctx->pty_stdin_pipe) {
            close(stdin_pipe[0]);
            n00b_sb_init_party_fd(&ctx->sb,
                                  &ctx->subproc_stdin,
                                  stdin_pipe[1],
                                  O_WRONLY,
                                  false,
                                  true,
                                  ctx->proxy_stdin_close);
        }

        ctx->pty_fd = pty_fd;

        n00b_sb_init_party_fd(&ctx->sb,
                              &ctx->subproc_stdout,
                              pty_fd,
                              O_RDWR,
                              true,
                              true,
                              false);

        n00b_sb_monitor_pid(&ctx->sb,
                            pid,
                            &ctx->subproc_stdout,
                            &ctx->subproc_stdout,
                            NULL,
                            true);
        n00b_subproc_install_callbacks(ctx);
        setup_subscriptions(ctx, true);

        if (!ctx->parent_termcap) {
            n00b_termcap_set_raw_mode(&ctx->saved_termcap);
        }
        else {
            tcsetattr(1, TCSAFLUSH, ctx->parent_termcap);
        }
        int flags = fcntl(pty_fd, F_GETFL, 0) | O_NONBLOCK;
        fcntl(pty_fd, F_SETFL, flags);
        run_startup_callback(ctx);
    }
    else {
        setvbuf(stdout, NULL, _IONBF, (size_t)0);
        setvbuf(stdin, NULL, _IONBF, (size_t)0);

        if (ctx->pty_stdin_pipe) {
            close(stdin_pipe[1]);
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
        n00b_subproc_do_exec(ctx);
    }
}

/*
 * Start a subprocess if you want to be responsible for making
 * sufficient calls to poll for IP, instead of having it run to
 * completion.
 *
 * If you use this, call n00b_subproc_poll() until it returns false
 */
void
n00b_subproc_start(n00b_subproc_t *ctx)
{
    if (ctx->use_pty) {
        n00b_subproc_spawn_forkpty(ctx);
    }
    else {
        n00b_subproc_spawn_fork(ctx);
    }
}

/*
 * Handle IO on the subprocess a single time. This is meant to be
 * called only when manually runnng the subprocess; if you call
 * n00b_subproc_run, don't use this interface!
 */
bool
n00b_subproc_poll(n00b_subproc_t *ctx)
{
    return n00b_sb_operate_switchboard(&ctx->sb, false);
}

/*
 * Spawns a process, and runs it until the process has ended. The
 * process must first be set up with `n00b_subproc_init()` and you may
 * configure it with other `n00b_subproc_*()` calls before running.
 *
 * The results can be queried via the `n00b_subproc_get_*()` API.
 */
void
n00b_subproc_run(n00b_subproc_t *ctx)
{
    n00b_subproc_start(ctx);
    n00b_sb_operate_switchboard(&ctx->sb, true);
}

void
n00b_subproc_reset_terminal(n00b_subproc_t *ctx)
{
    // Post-run cleanup.
    if (ctx->use_pty) {
        tcsetattr(0, TCSANOW, &ctx->saved_termcap);
    }
}
/*
 * This destroys any allocated memory inside a `subproc` object.  You
 * should *not* call this until you're done with the `n00b_capture_result_t`
 * object, as any dynamic memory (like string captures) that you
 * haven't taken ownership of will get freed when you call this.
 *
 * This call *will* destroy to n00b_capture_result_t object.
 *
 * However, this does *not* free the `n00b_subproc_t` object itself.
 */
void
n00b_subproc_close(n00b_subproc_t *ctx)
{
    n00b_subproc_reset_terminal(ctx);
    n00b_sb_destroy(&ctx->sb, false);
}

/*
 * Return the PID of the current subprocess.  Returns -1 if the
 * subprocess hasn't been launched.
 */
pid_t
n00b_subproc_get_pid(n00b_subproc_t *ctx)
{
    n00b_monitor_t *subproc = ctx->sb.pid_watch_list;

    if (!subproc) {
        return -1;
    }
    return subproc->pid;
}

/*
 * If you've got captures under the given tag name, then this will
 * return whatever was captured. If nothing was captured, it will
 * return a NULL pointer.
 *
 * But if a capture is returned, it will have been allocated via
 * `malloc()` and you will be responsible for calling `free()`.
 */
char *
n00b_sp_result_capture(n00b_capture_result_t *ctx, char *tag, size_t *outlen)
{
    for (int i = 0; i < ctx->num_captures; i++) {
        if (!strcmp(tag, ctx->captures[i].tag)) {
            *outlen = ctx->captures[i].len;
            return ctx->captures[i].contents;
        }
    }

    *outlen = 0;
    return NULL;
}

char *
n00b_subproc_get_capture(n00b_subproc_t *ctx, char *tag, size_t *outlen)
{
    n00b_sb_get_results(&ctx->sb, &ctx->result);
    return n00b_sp_result_capture(&ctx->result, tag, outlen);
}

int
n00b_subproc_get_exit(n00b_subproc_t *ctx, bool wait_for_exit)
{
    n00b_monitor_t *subproc = ctx->sb.pid_watch_list;

    if (!subproc) {
        return -1;
    }

    n00b_subproc_status_check(subproc, wait_for_exit);
    return subproc->exit_status;
}

int
n00b_subproc_get_errno(n00b_subproc_t *ctx, bool wait_for_exit)
{
    n00b_monitor_t *subproc = ctx->sb.pid_watch_list;

    if (!subproc) {
        return -1;
    }

    n00b_subproc_status_check(subproc, wait_for_exit);
    return subproc->found_errno;
}

int
n00b_subproc_get_signal(n00b_subproc_t *ctx, bool wait_for_exit)
{
    n00b_monitor_t *subproc = ctx->sb.pid_watch_list;

    if (!subproc) {
        return -1;
    }

    n00b_subproc_status_check(subproc, wait_for_exit);
    return subproc->term_signal;
}

void
n00b_subproc_set_parent_termcap(n00b_subproc_t *ctx, struct termios *tc)
{
    ctx->parent_termcap = tc;
}

void
n00b_subproc_set_child_termcap(n00b_subproc_t *ctx, struct termios *tc)
{
    ctx->child_termcap = tc;
}

void
n00b_subproc_set_extra(n00b_subproc_t *ctx, void *extra)
{
    n00b_sb_set_extra(&ctx->sb, extra);
}

void *
n00b_subproc_get_extra(n00b_subproc_t *ctx)
{
    return n00b_sb_get_extra(&ctx->sb);
}

char **
n00b_to_cstr_array(n00b_list_t *l)
{
    if (!l) {
        return NULL;
    }

    int64_t n      = n00b_list_len(l);
    char  **result = n00b_gc_array_alloc(char *, (n + 1));

    n00b_printf("[h3]In to_cstr_array, n =[/] {}", n);
    for (int i = 0; i < n; i++) {
        n00b_str_t *s = n00b_list_get(l, i, NULL);
        result[i]     = n00b_to_utf8(s)->data;
        n00b_printf("[h1]Debug:[/] {}", s);
    }

    result[n] = NULL;

    return result;
}

n00b_cmd_out_t *
_n00b_subproc(n00b_str_t *cmd, n00b_list_t *args, ...)
{
    static struct timeval to_default = {
        .tv_sec  = 1,
        .tv_usec = 0,
    };

    n00b_str_t     *new_stdin         = NULL;
    bool            close_stdin       = false;
    bool            proxy_stdin_close = true;
    bool            use_pty           = false;
    int32_t         passthrough       = N00B_SP_IO_ALL;
    bool            pass_err_to_out   = false;
    int             capture           = N00B_SP_IO_STDOUT | N00B_SP_IO_STDERR;
    bool            combine_capture   = false;
    struct timeval *timeout           = &to_default;
    bool            wait_for_exit     = false;
    n00b_list_t    *env               = NULL;
    n00b_cmd_out_t *result            = n00b_gc_alloc_mapped(n00b_cmd_out_t, N00B_GC_SCAN_ALL);

    n00b_karg_only_init(args);
    n00b_kw_ptr("new_stdin", new_stdin);
    n00b_kw_bool("close_stdin", close_stdin);
    n00b_kw_bool("proxy_stdin_close", proxy_stdin_close);
    n00b_kw_bool("use_pty", use_pty);
    n00b_kw_int32("passthrough", passthrough);
    n00b_kw_bool("pass_err_to_out", pass_err_to_out);
    n00b_kw_int32("capture", capture);
    n00b_kw_bool("combine_capture", combine_capture);
    n00b_kw_ptr("timeout", timeout);
    n00b_kw_bool("wait_for_exit", wait_for_exit);
    n00b_kw_ptr("env", env);

    n00b_list_t *all_args = n00b_list(n00b_type_utf8());

    n00b_list_append(all_args, cmd);
    all_args = n00b_list_plus(all_args, args);

    char         **argv = n00b_to_cstr_array(all_args);
    char         **envp = n00b_to_cstr_array(env);
    char          *ccmd = n00b_to_utf8(cmd)->data;
    n00b_subproc_t ctx;

    n00b_subproc_init(&ctx, ccmd, argv, proxy_stdin_close);
    n00b_subproc_set_timeout(&ctx, timeout);
    n00b_subproc_set_passthrough(&ctx, (unsigned char)passthrough, pass_err_to_out);
    n00b_subproc_set_capture(&ctx, (unsigned char)capture, combine_capture);
    if (use_pty) {
        n00b_subproc_use_pty(&ctx);
    }
    if (env) {
        n00b_subproc_set_envp(&ctx, envp);
    }
    if (new_stdin) {
        n00b_utf8_t *s   = n00b_to_utf8(new_stdin);
        int          len = n00b_str_byte_len(s);

        n00b_subproc_pass_to_stdin(&ctx, s->data, (size_t)len, close_stdin);
    }

    n00b_subproc_run(&ctx);
    n00b_sb_get_results(&ctx.sb, &ctx.result);

    size_t inlen;
    size_t outlen;
    size_t errlen;
    char  *sin  = n00b_sp_result_capture(&ctx.result, "stdin", &inlen);
    char  *sout = n00b_sp_result_capture(&ctx.result, "stdout", &outlen);
    char  *serr = n00b_sp_result_capture(&ctx.result, "stderr", &errlen);

    result->pid       = n00b_subproc_get_pid(&ctx);
    result->exit_code = n00b_subproc_get_exit(&ctx, wait_for_exit);
    result->stdin     = n00b_cstring(sin, inlen);
    result->stdout    = n00b_cstring(sout, outlen);
    result->stderr    = n00b_cstring(serr, errlen);

    return result;
}
