#define N00B_USE_INTERNAL_API
#include "n00b.h"

static void
n00b_session_init(n00b_session_t *session, va_list args)
{
    n00b_duration_t *max_time           = NULL;
    n00b_duration_t *idle_timeout       = NULL;
    n00b_string_t   *command            = NULL;
    n00b_list_t     *execv_args         = NULL;
    n00b_dict_t     *execv_env          = NULL;
    n00b_string_t   *pwd                = NULL;
    bool             pass_input         = true;
    bool             pass_output        = true;
    bool             merge_output       = true;
    bool             capture_output     = true;
    bool             capture_input      = true;
    bool             capture_commands   = true;
    bool             capture_injections = false;
    bool             pty                = true;
    int64_t          backscroll         = 100;

    n00b_karg_va_init(args);
    n00b_kw_ptr("max_time", max_time);
    n00b_kw_ptr("idle_timeout", idle_timeout);
    n00b_kw_ptr("command", command);
    n00b_kw_ptr("execv_args", execv_args);
    n00b_kw_ptr("execv_env", execv_env);
    n00b_kw_ptr("pwd", pwd);
    n00b_kw_bool("pass_input", pass_input);
    n00b_kw_bool("pass_output", pass_output);
    n00b_kw_bool("merge_output", merge_output);
    n00b_kw_bool("capture_output", capture_output);
    n00b_kw_bool("capture_input", capture_input);
    n00b_kw_bool("capture_injections", capture_injections);
    n00b_kw_bool("capture_commands", capture_commands);
    n00b_kw_bool("pty", pty);
    n00b_kw_int64("backscroll", backscroll);

    session->end_time             = max_time;
    session->idle_timeout         = idle_timeout;
    session->launch_command       = command;
    session->launch_args          = execv_args;
    session->using_pty            = pty;
    session->max_match_backscroll = backscroll;
    session->merge_output         = merge_output;
    session->pwd                  = pwd;
    session->state                = N00B_SESSION_CLOSED;

    int policy = N00B_CAPTURE_SPAWN | N00B_CAPTURE_END;

    if (!capture_output && !capture_input) {
        policy = 0;
    }

    if (capture_output) {
        policy |= N00B_CAPTURE_STDOUT;
        if (!merge_output) {
            policy |= N00B_CAPTURE_STDERR;
        }
    }

    if (capture_input) {
        policy |= N00B_CAPTURE_STDIN;
    }

    if (capture_injections) {
        policy |= N00B_CAPTURE_INJECTED;
    }

    if (capture_commands) {
        policy |= N00B_CAPTURE_CMD_RUN | N00B_CAPTURE_CMD_EXIT;
    }

    session->capture_policy = policy;
}

static const char *bash_setup_string = N00B_SESSION_BASH_SETUP;

static inline n00b_string_t *
create_tmpfiles(n00b_stream_t **ctrl_file_ptr)
{
    n00b_stream_t *rc_file = n00b_tempfile(NULL, NULL);
    *ctrl_file_ptr         = n00b_tempfile(NULL, NULL);
    n00b_string_t *result  = n00b_stream_get_name(rc_file);
    n00b_buf_t    *buf     = n00b_new(n00b_type_buffer(),
                               n00b_kw("length",
                                       (int64_t)sizeof(bash_setup_string),
                                       "ptr",
                                       bash_setup_string));

    n00b_write_blocking(rc_file, buf, NULL);
    n00b_close(rc_file);

    return result;
}

static void
post_fork_hook(n00b_session_t *s)
{
    n00b_string_t *ctrl = n00b_stream_get_name(s->subproc_ctrl_stream);

    n00b_set_env(n00b_cstring("N00B_BASH_INFO_LOG"), ctrl);

    if (!s->likely_bash) {
        n00b_set_env(n00b_cstring("ENV"), s->rc_filename);
    }

    if (s->pwd) {
        chdir(s->pwd->data);
    }
}

static inline n00b_cap_event_t *
capture(n00b_session_t *s, n00b_capture_t kind, void *contents)
{
    // TODO: add a lock around this.
    n00b_cap_event_t *e = n00b_gc_alloc_mapped(n00b_cap_event_t,
                                               N00B_GC_SCAN_ALL);

    e->timestamp = n00b_duration_diff(n00b_now(),
                                      s->capture_log->base_timestamp);
    e->contents  = contents;
    e->kind      = kind;

    n00b_list_append(s->capture_log->events, e);
    return e;
}

static n00b_list_t *
setup_shell_invocation(n00b_session_t *session)
{
    // Hopefully we have bash, because the 'control' interface we use
    // to pass command invocations assumes it.  If we don't have it,
    // we will have sh.
    //
    // First, we want to tell the shell to silently run a startup file
    // that sets up hooks that get called right before a command runs,
    // and right when a command returns to the bash prompt.
    //
    // Second, we want to agree on a file via which that hook will
    // write out info for us to read.
    //
    // Via a helper, we'll create both files, writing out the bash
    // script we want executed (which will make sure .bashrc runs if
    // it exits).
    //
    // Then we'll try to find commands named `bash`, and if we don't
    // find any, look for a command named `sh` (which should always be
    // there).
    //
    // When we don't find a command named `bash`, then there's a very
    // good chance that `sh` is actually bash. We don't bother
    // checking; we assume that it *is* bash. If it isn't, no big deal,
    // our hooks just won't run, and you won't get to match on those
    // events.
    //
    // Note that when bash is running as `bash`, it will accept the
    // config file as a command-line flag, so we set that up here.
    //
    // When it's running as `sh` it reads it from the `ENV`
    // environment variable.
    //
    // The communication file is also passed in an environment
    // variable (N00B_BASH_INFO_LOG).
    //
    // Environment variables do *not* get processed here, though.  We
    // don't want to add them to the environment of the parent process
    // (who might want to run multiple sessions at once). Instead,
    // once the fork happens, we add the environment variables before
    // we do the actual exec.

    n00b_stream_t *ctrl_stream;
    n00b_string_t *path;
    n00b_list_t   *result = n00b_list(n00b_type_string());

    session->rc_filename         = create_tmpfiles(&ctrl_stream);
    session->subproc_ctrl_stream = ctrl_stream;

    path = n00b_find_first_command_path(n00b_cstring("bash"), NULL, true);

    if (path) {
        session->likely_bash = true;
        n00b_list_append(result, path);
        n00b_list_append(result, n00b_cstring("-i"));
        path = n00b_cformat(N00B_SESSION_SHELL_BASH, session->rc_filename);
        n00b_list_append(result, path);
        return result;
    }
    session->likely_bash = false;

    path = n00b_find_first_command_path(n00b_cstring("sh"), NULL, true);
    n00b_assert(path);

    n00b_list_append(result, path);
    n00b_list_append(result, n00b_cstring("-i"));

    return result;
}

static inline bool
start_session_clock(n00b_session_t *session)
{
    if (!session->start_time) {
        session->start_time = n00b_now();

        if (session->end_time) {
            // Then it's the 'max' timeout, so translate to an abs time.
            session->end_time = n00b_duration_add(session->start_time,
                                                  session->end_time);
        }
        return true;
    }

    return false;
}

static void
control_cb(n00b_stream_t *s, void *msg, void *aux)
{
    // These come in line-buffered, but can come in multi-lined.
    n00b_buf_t     *data    = msg;
    n00b_session_t *session = aux;
}

static void
user_read_cb(n00b_stream_t *s, void *msg, void *aux)
{
    n00b_buf_t     *data    = msg;
    n00b_session_t *session = aux;
}

static void
subproc_written_to_cb(n00b_stream_t *s, void *msg, void *aux)
{
    n00b_list_t    *ansi_nodes = msg;
    n00b_session_t *session    = aux;
}

static void
subproc_read_stdout(n00b_stream_t *s, void *msg, void *aux)
{
    n00b_list_t    *ansi_nodes = msg;
    n00b_session_t *session    = aux;
}

static void
subproc_read_stderr(n00b_stream_t *s, void *msg, void *aux)
{
    n00b_list_t    *ansi_nodes = msg;
    n00b_session_t *session    = aux;
}

static inline void
setup_control_stream(n00b_session_t *s)
{
    n00b_stream_t *cb = n00b_callback_open(control_cb, s);
    n00b_line_buffer_writes(cb);
    n00b_io_subscribe_to_reads(s->subproc_ctrl_stream, cb, NULL);
}

static inline void
setup_initial_passthrough(n00b_session_t *s)
{
    // This will get set up when we call n00b_proc_run().
    if (s->passthrough_output) {
        s->subprocess->flags |= N00B_PROC_STDOUT_PROXY;
        s->subprocess->flags |= N00B_PROC_STDERR_PROXY;
    }

    if (s->passthrough_input) {
        s->subprocess->flags |= N00B_PROC_STDIN_PROXY;
    }
}

static void
n00b_ansi_ctrl_parse_before_writing(n00b_stream_t *cb)
{
}

static inline void
setup_state_handlers(n00b_session_t *s)
{
    // The callbacks used here add to the capture IF we're capturing,
    // and they handle matching / state transitions.

    n00b_stream_t *cb;

    if (s->passthrough_input) {
        cb = n00b_callback_open(user_read_cb, s);
        n00b_io_subscribe_to_reads(n00b_stdin(), cb, NULL);
    }

    cb = n00b_callback_open(subproc_written_to_cb, s);
    n00b_ansi_ctrl_parse_before_writing(cb);
    n00b_io_subscribe_to_delivery(s->subprocess->subproc_stdin, cb, NULL);

    cb = n00b_callback_open(subproc_read_stdout, s);
    n00b_ansi_ctrl_parse_before_writing(cb);
    n00b_io_subscribe_to_reads(s->subprocess->subproc_stdout, cb, NULL);

    if (!s->merge_output) {
        cb = n00b_callback_open(subproc_read_stderr, s);
        n00b_ansi_ctrl_parse_before_writing(cb);
        n00b_io_subscribe_to_reads(s->subprocess->subproc_stderr, cb, NULL);
    }

    s->cur_user_state = s->start_state;
}

static inline void
setup_capture(n00b_session_t *s, n00b_string_t *cmd, n00b_list_t *args)
{
    if (!s->capture_log) {
        s->capture_log = n00b_gc_alloc_mapped(n00b_capture_log_t,
                                              N00B_GC_SCAN_ALL);

        s->capture_log->base_timestamp = s->start_time;
        s->capture_log->events         = n00b_list(n00b_type_ref());
    }

    n00b_cap_spawn_info_t *info = n00b_gc_alloc_mapped(n00b_cap_spawn_info_t,
                                                       N00B_GC_SCAN_ALL);

    info->command = cmd;
    info->args    = args;

    capture(s, N00B_CAPTURE_SPAWN, info);
}

static inline void
setup_replay(n00b_session_t *s)
{
    // This is TODO. The basic idea is that we will use the timestamps to
    // (re-)inject input events onto the subprocess.
    //
    // We will ignore all other events.
}

static inline void
launch_user_command(n00b_session_t *s)
{
    int64_t err_pty = s->using_pty & !s->merge_output;

    s->subprocess = n00b_run_process(s->launch_command,
                                     s->launch_args,
                                     false,
                                     false,
                                     n00b_kw("pty",
                                             (int64_t)s->using_pty,
                                             "merge",
                                             (int64_t)s->merge_output,
                                             "err_pty",
                                             err_pty));

    setup_capture(s, s->launch_command, s->launch_args);
}

static inline void
launch_shell(n00b_session_t *s)
{
    n00b_list_t   *argv    = setup_shell_invocation(s);
    n00b_string_t *cmd     = n00b_list_dequeue(argv);
    int64_t        err_pty = s->using_pty & !s->merge_output;

    // We don't use the run_process capture facility because it doesn't
    // record timestamps or worry about A
    s->subprocess = n00b_run_process(cmd,
                                     argv,
                                     false,
                                     false,
                                     n00b_kw("pre_exec_hook",
                                             post_fork_hook,
                                             "hook_param",
                                             s,
                                             "pty",
                                             (int64_t)s->using_pty,
                                             "merge",
                                             (int64_t)s->merge_output,
                                             "err_pty",
                                             err_pty));

    setup_capture(s, cmd, argv);
}

static inline void
n00b_session_run_control_loop(n00b_session_t *s)
{
    n00b_session_start_control(s);
    // TODO:
    // wait on a condition variable for events.
    // scan the match buffers for matches.

    n00b_session_end_control(s);
}

void
n00b_session_start(n00b_session_t *session)
{
    if (session->launch_command) {
        launch_user_command(session);
    }
    else {
        launch_shell(session);
        setup_control_stream(session);
    }

    setup_initial_passthrough(session);
    setup_state_handlers(session);
    start_session_clock(session);
    setup_replay(session);
    n00b_proc_spawn(session->subprocess);
    n00b_session_run_control_loop(session);
}

// TODO:
//
// Filter to ansify stuff.
// control loop.
// pause.
// reset.
// replay.
// When proxying, allow filters to choose what to deliver or not? If so, do
// it w/ a custom callback filter, which you already wrote.
