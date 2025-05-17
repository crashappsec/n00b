#define N00B_USE_INTERNAL_API
#include "n00b.h"

static void
n00b_session_init(n00b_session_t *session, va_list args)
{
    n00b_duration_t *max_time           = NULL;
    n00b_duration_t *event_timeout      = NULL;
    n00b_string_t   *command            = NULL;
    n00b_list_t     *execv_args         = NULL;
    n00b_dict_t     *execv_env          = NULL;
    n00b_string_t   *pwd                = NULL;
    n00b_stream_t  *capture_stream     = NULL; // No more than one option
    n00b_string_t   *capture_filename   = NULL; // for capture is allowed.
    bool             capture_tmpfile    = false;
    bool             pass_input         = true;
    bool             pass_output        = true;
    bool             merge_output       = true;
    bool             capture_output     = true;
    bool             capture_input      = true;
    bool             capture_commands   = true;
    bool             capture_injections = false;
    bool             pty                = true;
    int              capture_policy     = 0;

    n00b_karg_va_init(args);
    n00b_kw_ptr("max_time", max_time);
    n00b_kw_ptr("timeout", event_timeout);
    n00b_kw_ptr("command", command);
    n00b_kw_ptr("execv_args", execv_args);
    n00b_kw_ptr("execv_env", execv_env);
    n00b_kw_ptr("pwd", pwd);
    n00b_kw_bool("capture_tmpfile", capture_tmpfile);
    n00b_kw_bool("pass_input", pass_input);
    n00b_kw_bool("pass_output", pass_output);
    n00b_kw_bool("merge_output", merge_output);
    n00b_kw_bool("capture_output", capture_output);
    n00b_kw_bool("capture_input", capture_input);
    n00b_kw_bool("capture_injections", capture_injections);
    n00b_kw_bool("capture_commands", capture_commands);
    n00b_kw_ptr("capture_stream", capture_stream);
    n00b_kw_ptr("capture_filename", capture_filename);
    n00b_kw_int32("capture_policy", capture_policy);
    n00b_kw_bool("pty", pty);

    session->end_time           = max_time;
    session->launch_command     = command;
    session->launch_args        = execv_args;
    session->using_pty          = pty;
    session->merge_output       = merge_output;
    session->pwd                = pwd;
    session->state              = N00B_SESSION_CLOSED;
    session->passthrough_input  = pass_input;
    session->passthrough_output = pass_output;
    session->max_event_gap      = event_timeout;

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

    if (capture_tmpfile) {
        n00b_assert(!capture_stream);
        capture_stream = n00b_tempfile(NULL, n00b_cstring(".cap10"));
    }
    if (capture_filename) {
        n00b_assert(!capture_stream);
        capture_stream = n00b_channel_open_file(capture_filename);
    }

    if (capture_stream) {
        n00b_setup_capture(session, capture_stream, capture_policy);
    }
}

static const char *bash_setup_string = N00B_SESSION_BASH_SETUP;

static inline n00b_string_t *
create_tmpfiles(n00b_stream_t **ctrl_file_ptr)
{
    n00b_stream_t *rc_file = n00b_tempfile(NULL, NULL);
    *ctrl_file_ptr          = n00b_tempfile(NULL, NULL);
    n00b_string_t *result   = n00b_channel_get_name(rc_file);
    n00b_buf_t    *buf      = n00b_new(n00b_type_buffer(),
                               n00b_kw("length",
                                       (int64_t)strlen(bash_setup_string),
                                       "ptr",
                                       bash_setup_string));

    n00b_channel_write(rc_file, buf);
    n00b_close(rc_file);

    return result;
}

#define capture(x, y, z) n00b_session_capture(x, y, z)

static void
post_fork_hook(n00b_session_t *s)
{
    n00b_string_t *ctrl = n00b_channel_get_name(s->subproc_ctrl_stream);
    n00b_set_env(n00b_cstring("N00B_BASH_INFO_LOG"), ctrl);

    if (!s->likely_bash) {
        n00b_set_env(n00b_cstring("ENV"), s->rc_filename);
    }

    if (s->pwd) {
        chdir(s->pwd->data);
    }
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
    n00b_string_t  *path;
    n00b_list_t    *result = n00b_list(n00b_type_string());

    session->rc_filename         = create_tmpfiles(&ctrl_stream);
    session->subproc_ctrl_stream = ctrl_stream;

    path = n00b_find_first_command_path(n00b_cstring("bash"), NULL, true);

    if (path) {
        session->likely_bash = true;
        n00b_list_append(result, path);
        n00b_list_append(result, n00b_cstring(N00B_SESSION_SHELL_BASH));
        n00b_list_append(result, session->rc_filename);
        n00b_list_append(result, n00b_cstring("-i"));
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
        session->skew       = NULL;

        if (session->end_time) {
            // Then it's the 'max' timeout, so translate to an abs time.
            session->end_time = n00b_duration_add(session->start_time,
                                                  session->end_time);
        }
        return true;
    }

    return false;
}

static inline void
setup_user_command(n00b_session_t *s)
{
    int64_t err_pty;

    err_pty = (int64_t)((bool)s->using_pty & (bool)!s->merge_output);

    s->subprocess = n00b_run_process(s->launch_command,
                                     s->launch_args,
                                     false,
                                     false,
                                     n00b_kw("pty",
                                             (int64_t)s->using_pty,
                                             "stdout_subs",
                                             s->stdout_cb,
                                             "stderr_subs",
                                             s->stderr_cb,
                                             "run",
                                             false,
                                             "merge",
                                             (int64_t)s->merge_output,
                                             "err_pty",
                                             err_pty));

    n00b_capture_launch(s, s->launch_command, s->launch_args);
}

static inline void
setup_shell(n00b_session_t *s)
{
    n00b_list_t   *argv    = setup_shell_invocation(s);
    n00b_string_t *cmd     = n00b_list_dequeue(argv);
    int64_t        err_pty = (int64_t)((bool)s->using_pty & (bool)!s->merge_output);

    n00b_capture_launch(s, cmd, argv);
    // We don't use the run_process capture facility because it doesn't
    // record timestamps or worry about ansi, etc.
    s->subprocess = n00b_run_process(cmd,
                                     argv,
                                     false,
                                     false,
                                     n00b_kw("pre_exec_hook",
                                             post_fork_hook,
                                             "hook_param",
                                             s,
                                             "stdout_subs",
                                             s->stdout_cb,
                                             "stderr_subs",
                                             s->stderr_cb,
                                             "run",
                                             false,
                                             "pty",
                                             (int64_t)s->using_pty,
                                             "merge",
                                             (int64_t)s->merge_output,
                                             "err_pty",
                                             err_pty));
}

static inline bool
session_cleanup(n00b_session_t *session)
{
    n00b_string_t *s;
    bool           result = !session->early_exit;

    // Reset session data to remove session-specific stuff that
    // we shouldn't query afer the ression ends anyway.
    //
    // One needs to call session_reset() to properly reset.
    // We don't clean up here:
    // start / end, timeout and early_exit

    if (session->end_time) {
        session->end_time = n00b_duration_diff(session->start_time,
                                               session->end_time);
    }
    session->last_event = NULL;
    n00b_truncate_all_match_data(session, NULL, 0);
    n00b_close(session->stdin_injection);
    session->cur_user_state = NULL;
    if (session->rc_filename) {
        unlink(session->rc_filename->data);
        session->rc_filename = NULL;
    }
    if (session->subproc_ctrl_stream) {
        s = n00b_channel_get_name(session->subproc_ctrl_stream);
        n00b_close(session->subproc_ctrl_stream);
        unlink(s->data);
        session->subproc_ctrl_stream = NULL;
    }

    session->capture_stream = NULL;
    session->saved_capture  = NULL;

    if (session->subprocess) {
        n00b_proc_close(session->subprocess);
        session->subprocess = NULL;
    }
    n00b_condition_init(&session->control_notify);
    session->log_cursor = (n00b_log_cursor_t){
        0,
    };
    session->next_capture_id = 0;
    session->paused_clock    = false;

    return result;
}

bool
n00b_session_run(n00b_session_t *session)
{
    if (n00b_session_is_active(session)) {
        return false;
    }
    if (session->log_cursor.log && session->log_cursor.cinematic) {
        return n00b_session_run_cinematic(session);
    }

    start_session_clock(session);
    n00b_session_setup_state_handling(session);

    if (session->launch_command) {
        setup_user_command(session);
    }
    else {
        setup_shell(session);
        n00b_session_setup_control_stream(session);
    }

    if (session->log_cursor.log) {
        n00b_session_start_replay(session);
    }
    n00b_proc_spawn(session->subprocess);
    n00b_session_run_control_loop(session);
    capture(session, N00B_CAPTURE_END, NULL);
    n00b_session_finish_capture(session);
    return session_cleanup(session);
}

void
n00b_session_start_replay(n00b_session_t *session)
{
    if (!session->log_cursor.log) {
        N00B_CRAISE("Replay not set up.");
    }
    start_session_clock(session);
    n00b_thread_spawn((void *)n00b_session_run_replay_loop, session);
}

bool
n00b_session_run_cinematic(n00b_session_t *session)
{
    if (!session->log_cursor.log) {
        N00B_CRAISE("No replay event stream added");
    }
    session->log_cursor.paused = false;
    n00b_session_setup_user_read_cb(session);
    n00b_session_start_replay(session);
    n00b_session_run_control_loop(session);
    return session_cleanup(session);
}

const n00b_vtable_t n00b_session_vtable = {
    .methods = {
        [N00B_BI_CONSTRUCTOR] = (n00b_vtable_entry)n00b_session_init,
    },
};

// TODO:
//
// Finish finish capture / all the WINCH stuff
// When proxying, allow filters to choose what to deliver or not? If so, do
// it w/ a custom callback filter, which you already wrote.
// Custom filtering for recording.
// Generate ascii-cinema JSONL
// Generate Animated Gif
// STATE Actions for injecting, closing, interacting, removing interaction,
//                   breaking the pipes, changing speed, ...
