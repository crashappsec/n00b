#define N00B_USE_INTERNAL_API
#include "n00b.h"

static void
n00b_session_init(n00b_session_t *session, va_list args)
{
    n00b_duration_t *max_time           = NULL;
    n00b_string_t   *command            = NULL;
    n00b_list_t     *execv_args         = NULL;
    n00b_dict_t     *execv_env          = NULL;
    n00b_string_t   *pwd                = NULL;
    n00b_stream_t   *capture_stream     = NULL; // No more than one option
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
        capture_stream = n00b_new(n00b_type_file(),
                                  capture_filename,
                                  n00b_fm_rw);
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
    *ctrl_file_ptr         = n00b_tempfile(NULL, NULL);
    n00b_string_t *result  = n00b_stream_get_name(rc_file);
    n00b_buf_t    *buf     = n00b_new(n00b_type_buffer(),
                               n00b_kw("length",
                                       (int64_t)strlen(bash_setup_string),
                                       "ptr",
                                       bash_setup_string));

    n00b_write_blocking(rc_file, buf, NULL);
    n00b_close(rc_file);

    return result;
}

extern void n00b_restart_io(void);

static void
post_fork_hook(n00b_session_t *s)
{
    n00b_string_t *ctrl = n00b_stream_get_name(s->subproc_ctrl_stream);
    n00b_restart_io();
    n00b_set_env(n00b_cstring("N00B_BASH_INFO_LOG"), ctrl);

    if (!s->likely_bash) {
        n00b_set_env(n00b_cstring("ENV"), s->rc_filename);
    }

    if (s->pwd) {
        chdir(s->pwd->data);
    }

    n00b_unbuffer_stdin();
    n00b_unbuffer_stdout();
    n00b_unbuffer_stderr();
}

static void
n00b_capture_encode(n00b_stream_t *strm, void *ignore, void *msg);

static inline void
capture(n00b_session_t *s, n00b_capture_t kind, void *contents)
{
    if (!s->capture_stream) {
        return;
    }

    // Do not capture if the policy doesn't explicitly list
    // the event.
    if (!(s->capture_policy & kind)) {
        return;
    }

    n00b_cap_event_t *e = n00b_gc_alloc_mapped(n00b_cap_event_t,
                                               N00B_GC_SCAN_ALL);

    n00b_duration_t *d = n00b_duration_diff(n00b_now(), s->start_time);

    if (s->skew) {
        e->timestamp = n00b_duration_diff(d, s->skew);
    }
    else {
        e->timestamp = d;
    }

    e->id       = s->next_capture_id++;
    e->contents = contents;
    e->kind     = kind;

    // TODO: Something isn't working???
    // n00b_write(s->capture_stream, e);
    n00b_capture_encode(s->unproxied_capture, NULL, e);
}

static inline void
write_capture_header(n00b_stream_t *s)
{
    // TODO: anything w/ winsize at all.
    // We just write out 0's for window size. If we end properly,
    // we'll come back and fill in the max dimensions at the end.
    const int64_t    len = sizeof(int64_t) + sizeof(n00b_duration_t);
    n00b_buf_t      *b   = n00b_new(n00b_type_buffer(), n00b_kw("length", len));
    int64_t         *mp  = (int64_t *)b->data;
    char            *p   = b->data + sizeof(int64_t);
    n00b_duration_t *d   = n00b_now();

    *mp = (int64_t)N00B_SESSION_MAGIC;
    memcpy(p, d, sizeof(n00b_duration_t));
    n00b_stream_raw_fd_write(s, b);
}

static inline int64_t
read_capture_magic(n00b_stream_t *s)
{
    n00b_ev2_cookie_t *cookie = s->cookie;

    char buf[8];
    read(cookie->id, buf, sizeof(int64_t));
    int64_t *magic = (int64_t *)&buf[0];
    return *magic == N00B_SESSION_MAGIC;
}

static inline n00b_duration_t *
read_timestamp(n00b_stream_t *s, double scale)
{
    n00b_ev2_cookie_t *cookie = s->cookie;
    char               buf[sizeof(n00b_duration_t)];

    read(cookie->id, buf, sizeof(n00b_duration_t));

    return n00b_duration_multiply((n00b_duration_t *)&buf[0], scale);
}

static inline int32_t
read_capture_int32(n00b_stream_t *s)
{
    n00b_ev2_cookie_t *cookie = s->cookie;

    char buf[sizeof(int32_t)];
    read(cookie->id, buf, sizeof(int32_t));

    int32_t result = *(int32_t *)&buf[0];

    return result;
}

static inline uint8_t
read_capture_byte(n00b_stream_t *s)
{
    n00b_ev2_cookie_t *cookie = s->cookie;
    uint8_t            buf[1];

    read(cookie->id, buf, 1);

    return buf[0];
}

static inline n00b_string_t *
read_capture_payload_str(n00b_stream_t *s)
{
    n00b_buf_t *sbuf = n00b_new(n00b_type_buffer(), n00b_kw("length", 4ULL));
    int64_t     len;

    n00b_stream_raw_fd_read(s, sbuf);
    len = *(int32_t *)sbuf->data;

    sbuf = n00b_new(n00b_type_buffer(), n00b_kw("length", len));
    n00b_stream_raw_fd_read(s, sbuf);

    return n00b_buf_to_string(sbuf);
}

static inline n00b_list_t *
read_ansi_payload_str(n00b_stream_t *s)
{
    n00b_string_t *str = read_capture_payload_str(s);
    return n00b_string_to_ansi_node_list(str);
}

static inline struct winsize *
read_winch_payload(n00b_stream_t *s)
{
    const int64_t len  = sizeof(struct winsize *);
    n00b_buf_t   *sbuf = n00b_new(n00b_type_buffer(), n00b_kw("length", len));

    n00b_stream_raw_fd_read(s, sbuf);
    return (struct winsize *)sbuf->data;
}

static inline n00b_cap_spawn_info_t *
read_spawn_payload(n00b_stream_t *s)
{
    n00b_cap_spawn_info_t *result = n00b_gc_alloc_mapped(n00b_cap_spawn_info_t,
                                                         N00B_GC_SCAN_ALL);

    result->command = read_capture_payload_str(s);
    int n           = read_capture_int32(s);
    result->args    = n00b_list(n00b_type_string());

    for (int i = 0; i < n; i++) {
        n00b_list_append(result->args, read_capture_payload_str(s));
    }

    return result;
}

static inline void
add_record_header(n00b_cap_event_t *event, n00b_stream_t *s)
{
    const int64_t len = sizeof(n00b_duration_t) + sizeof(uint32_t) + 1;

    n00b_buf_t *b = n00b_new(n00b_type_buffer(), n00b_kw("length", len));
    char       *p = b->data;
    memcpy(p, &event->id, sizeof(uint32_t));
    p += sizeof(uint32_t);
    memcpy(p, event->timestamp, sizeof(n00b_duration_t));
    p += sizeof(n00b_duration_t);
    *p = (char)event->kind;

    n00b_stream_raw_fd_write(s, b);
}

static inline void
add_capture_payload_str(n00b_string_t *s, n00b_stream_t *strm)
{
    if (!s) {
        s = n00b_cached_empty_string();
    }
    n00b_buf_t *b1 = n00b_new(n00b_type_buffer(), n00b_kw("length", 4LL));
    uint32_t   *n  = (uint32_t *)b1->data;
    *n             = s->u8_bytes;
    n00b_buf_t *b2 = n00b_new(n00b_type_buffer(),
                              n00b_kw("length",
                                      (int64_t)s->u8_bytes,
                                      "ptr",
                                      s->data));
    n00b_stream_raw_fd_write(strm, b1);
    n00b_stream_raw_fd_write(strm, b2);
}

static inline void
add_capture_payload_ansi(n00b_list_t *anodes, n00b_stream_t *strm)
{
    n00b_string_t *s = n00b_ansi_nodes_to_string(anodes, true);

    add_capture_payload_str(s, strm);
}

static inline void
add_capture_winch(struct winsize *dims, n00b_stream_t *strm)
{
    n00b_buf_t *b = n00b_new(n00b_type_buffer(),
                             n00b_kw("length",
                                     (int64_t)sizeof(struct winsize),
                                     "ptr",
                                     dims));
    n00b_stream_raw_fd_write(strm, b);
}

static inline void
add_capture_spawn(n00b_cap_spawn_info_t *si, n00b_stream_t *strm)
{
    add_capture_payload_str(si->command, strm);

    int32_t     n = si->args ? n00b_list_len(si->args) : 0;
    n00b_buf_t *b = n00b_new(n00b_type_buffer(),
                             n00b_kw("length", sizeof(uint32_t)));

    *(int32_t *)b->data = n;
    n00b_stream_raw_fd_write(strm, b);

    for (int i = 0; i < n; i++) {
        n00b_string_t *s = n00b_list_get(si->args, i, NULL);
        add_capture_payload_str(s, strm);
    }
}

static void
n00b_capture_encode(n00b_stream_t *strm, void *ignore, void *msg)
{
    n00b_cap_event_t *event = msg;

    if (!event) {
        return;
    }

    if (!event->id) {
        write_capture_header(strm);
    }
    // Since captures may already take up a lot of space, we are going to
    // avoid a full marshal and do a somewhat more compact encoding. At some
    // point we should hook up compression too.
    add_record_header(event, strm);

    switch (event->kind) {
    case N00B_CAPTURE_STDIN:
    case N00B_CAPTURE_CMD_RUN:
        add_capture_payload_str(event->contents, strm);
        break;
    case N00B_CAPTURE_INJECTED:
    case N00B_CAPTURE_STDOUT:
    case N00B_CAPTURE_STDERR:
        add_capture_payload_ansi(event->contents, strm);
        break;
    case N00B_CAPTURE_WINCH:
        add_capture_winch(event->contents, strm);
        break;
    case N00B_CAPTURE_SPAWN:
        add_capture_spawn(event->contents, strm);
        break;
    default:
        break;
    }
}

/*
static inline n00b_stream_filter_t *
n00b_capture_encoder(void)
{
    return n00b_new_filter(n00b_capture_encode,
                           NULL,
                           n00b_cstring("capture-encoder"),
                           0);
}
*/

void
n00b_setup_capture(n00b_session_t *s, n00b_stream_t *target, int policy)
{
    if (!policy) {
        policy = ~0;
    }

    if (s->capture_stream) {
        N00B_CRAISE("Currently sessions support only one capture stream");
    }

    if (!n00b_stream_can_write(target)) {
        N00B_CRAISE("Capture stream must be open and writable.");
    }

    s->cap_filename      = n00b_stream_get_name(target);
    s->unproxied_capture = target;

    n00b_stream_t *p = n00b_new_subscription_proxy();

    //    n00b_add_write_filter(p, n00b_capture_encoder());
    n00b_io_subscribe_to_delivery(p, target, NULL);

    s->capture_stream = p;
    s->capture_policy = policy;
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
        n00b_list_append(result, n00b_cstring(N00B_SESSION_SHELL_BASH));
        n00b_list_append(result, session->rc_filename);
        n00b_list_append(result, n00b_cstring("-i"));
        // n00b_list_append(result, n00b_cstring("-l"));
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
process_control_buffer(n00b_session_t *session, n00b_string_t *s)
{
    n00b_list_t   *lines = n00b_string_split(s, n00b_cached_newline());
    int            n     = n00b_list_len(lines);
    n00b_string_t *cur   = NULL;

    for (int i = 0; i < n; i++) {
        n00b_string_t *line = n00b_list_get(lines, i, NULL);

        if (!n00b_string_codepoint_len(line)) {
            continue;
        }

        switch (line->data[0]) {
        case 'r':
            cur              = n00b_string_slice(line, 2, -1);
            session->got_run = true;
            break;
        case 'p':
            capture(session, N00B_CAPTURE_CMD_EXIT, NULL);
            session->got_prompt = true;
            break;
        case 'q':
            if (cur) {
                capture(session, N00B_CAPTURE_CMD_RUN, cur);
                cur = NULL;
            }
            break;
        default:
            cur = n00b_string_concat(cur, n00b_cached_newline());
            cur = n00b_string_concat(cur, line);
            break;
        }
    }
    if (cur) {
        capture(session, N00B_CAPTURE_CMD_RUN, cur);
        cur = NULL;
    }
}

static inline void
add_ansi_to_match_buffer(n00b_session_t *session,
                         n00b_capture_t  kind,
                         n00b_list_t    *l)
{
    n00b_string_t  *s = n00b_ansi_nodes_to_string(l, session->match_ansi);
    n00b_string_t **buf_loc;
    n00b_string_t  *buf;

    switch (kind) {
    case N00B_CAPTURE_STDOUT:
        buf_loc = &session->stdout_match_buffer;
        break;
    case N00B_CAPTURE_STDERR:
        buf_loc = &session->stderr_match_buffer;
        break;
    default:
        n00b_unreachable();
    }

    buf = *buf_loc;

    if (!buf) {
        *buf_loc = s;
    }
    else {
        *buf_loc = n00b_string_concat(buf, s);
    }
}

static inline void
add_buf_to_match_buffer(n00b_session_t *s, n00b_capture_t kind, n00b_buf_t *buf)
{
    if (kind == N00B_CAPTURE_STDIN) {
        if (!s->input_match_buffer) {
            s->input_match_buffer = n00b_cstring(buf->data);
        }
        else {
            s->input_match_buffer = n00b_string_concat(s->input_match_buffer,
                                                       n00b_cstring(buf->data));
        }
        return;
    }
    if (kind == N00B_CAPTURE_INJECTED) {
        if (!s->injection_match_buffer) {
            s->injection_match_buffer = n00b_cstring(buf->data);
        }
        else {
            s->injection_match_buffer = n00b_string_concat(
                s->injection_match_buffer,
                n00b_cstring(buf->data));
        }
        return;
    }
    n00b_unreachable();
}

static void
control_cb(n00b_stream_t *stream, void *msg, void *aux)
{
    n00b_buf_t     *buf     = msg;
    n00b_session_t *session = aux;
    n00b_string_t  *s       = n00b_cstring(buf->data);

    n00b_condition_lock_acquire(&session->control_notify);
    process_control_buffer(session, s);
    n00b_condition_notify_one(&session->control_notify);
    n00b_condition_lock_release(&session->control_notify);
}

static void
user_read_cb(n00b_stream_t *stream, void *msg, void *aux)
{
    n00b_buf_t     *data    = msg;
    n00b_session_t *session = aux;

    n00b_condition_lock_acquire(&session->control_notify);

    capture(session, N00B_CAPTURE_STDIN, data);
    session->got_user_input = true;
    add_buf_to_match_buffer(session, N00B_CAPTURE_STDIN, data);
    n00b_condition_notify_one(&session->control_notify);
    n00b_condition_lock_release(&session->control_notify);
}

static void
subproc_injection_cb(n00b_stream_t *stream, void *msg, void *aux)
{
    n00b_buf_t     *input   = msg;
    n00b_session_t *session = aux;

    n00b_condition_lock_acquire(&session->control_notify);
    capture(session, N00B_CAPTURE_INJECTED, input);
    session->got_injection = true;
    n00b_write(session->subprocess->subproc_stdin, input);
    add_buf_to_match_buffer(session, N00B_CAPTURE_INJECTED, input);
    n00b_condition_notify_one(&session->control_notify);
    n00b_condition_lock_release(&session->control_notify);
}

static void
subproc_read_stdout(n00b_stream_t *stream, void *msg, void *aux)
{
    n00b_list_t    *ansi_nodes = msg;
    n00b_session_t *session    = aux;

    n00b_condition_lock_acquire(&session->control_notify);
    capture(session, N00B_CAPTURE_STDOUT, ansi_nodes);
    session->got_stdout = true;
    add_ansi_to_match_buffer(session, N00B_CAPTURE_STDOUT, ansi_nodes);
    if (session->passthrough_output) {
        n00b_string_t *s = n00b_ansi_nodes_to_string(ansi_nodes, true);
        // Can't use n00b_write until we add a way to cut through filters,
        // or send parameters to them anyway.
        // n00b_write(n00b_stdout(), s);
        write(1, s->data, s->u8_bytes);
    }
    n00b_condition_notify_one(&session->control_notify);
    n00b_condition_lock_release(&session->control_notify);
}

static void
subproc_read_stderr(n00b_stream_t *stream, void *msg, void *aux)
{
    n00b_list_t    *ansi_nodes = msg;
    n00b_session_t *session    = aux;

    n00b_condition_lock_acquire(&session->control_notify);
    capture(session, N00B_CAPTURE_STDERR, ansi_nodes);
    session->got_stderr = true;
    add_ansi_to_match_buffer(session, N00B_CAPTURE_STDERR, ansi_nodes);
    if (session->passthrough_output) {
        n00b_string_t *s = n00b_ansi_nodes_to_string(ansi_nodes, true);

        //  Per above.
        // n00b_write(n00b_stderr(), s);
        write(2, s->data, s->u8_bytes);
    }
    n00b_condition_notify_one(&session->control_notify);
    n00b_condition_lock_release(&session->control_notify);
}

static inline void
setup_control_stream(n00b_session_t *s)
{
    n00b_stream_t *cb = n00b_callback_open(control_cb, s);
    n00b_line_buffer_writes(cb);
    n00b_io_subscribe_to_reads(s->subproc_ctrl_stream, cb, NULL);
}

static inline n00b_list_t *
find_state(n00b_session_t *s, n00b_string_t *name)
{
    if (!s->user_states) {
        return s->global_actions;
    }

    return hatrack_dict_get(s->user_states, name, NULL);
}

static inline void
setup_state_handling(n00b_session_t *s)
{
    // The callbacks used here add to the capture IF we're capturing,
    // and they handle matching / state transitions.

    n00b_unbuffer_stdin();
    n00b_unbuffer_stdout();
    n00b_unbuffer_stderr();

    n00b_stream_t *cb;

    cb = n00b_callback_open(user_read_cb, s);
    n00b_io_subscribe_to_reads(n00b_stdin(), cb, NULL);

    cb = n00b_callback_open(subproc_injection_cb, s);
    n00b_ansi_ctrl_parse_on_write(cb);
    s->stdin_injection = cb;

    cb = n00b_callback_open(subproc_read_stdout, s);
    n00b_ansi_ctrl_parse_on_write(cb);
    s->stdout_cb = n00b_list(n00b_type_stream());
    n00b_list_append(s->stdout_cb, cb);

    if (!s->merge_output) {
        cb = n00b_callback_open(subproc_read_stderr, s);
        n00b_ansi_ctrl_parse_on_write(cb);
        s->stderr_cb = n00b_list(n00b_type_stream());
        n00b_list_append(s->stderr_cb, cb);
    }

    if (s->start_state) {
        s->cur_user_state = find_state(s, s->start_state);
    }

    n00b_raw_condition_init(&s->control_notify);
}

static inline void
capture_launch(n00b_session_t *s, n00b_string_t *cmd, n00b_list_t *args)
{
    if (!atomic_read(&s->capture_stream)) {
        return;
    }

    n00b_cap_spawn_info_t *info = n00b_gc_alloc_mapped(n00b_cap_spawn_info_t,
                                                       N00B_GC_SCAN_ALL);

    info->command = cmd;
    info->args    = args;

    capture(s, N00B_CAPTURE_SPAWN, info);
}

static inline void
setup_child_tc(struct termios *tc)
{
    tcgetattr(0, tc);
    tc->c_oflag     = OPOST | ONLCR;
    tc->c_iflag     = IGNBRK | INLCR | IXANY;
    tc->c_cflag     = CLOCAL | CREAD | CS8 | HUPCL;
    tc->c_lflag     = ECHO | ECHOE | ECHOK | ECHONL;
    tc->c_cc[VMIN]  = 0;
    tc->c_cc[VTIME] = 1;
}

static inline void
setup_user_command(n00b_session_t *s)
{
    int64_t        err_pty = (int64_t)((bool)s->using_pty & (bool)!s->merge_output);
    struct termios tc;

    setup_child_tc(&tc);

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
                                             /*  "termcap",
                                                 &tc,*/
                                             "merge",
                                             (int64_t)s->merge_output,
                                             "err_pty",
                                             err_pty));

    capture_launch(s, s->launch_command, s->launch_args);
}

static inline void
setup_shell(n00b_session_t *s)
{
    n00b_list_t   *argv    = setup_shell_invocation(s);
    n00b_string_t *cmd     = n00b_list_dequeue(argv);
    int64_t        err_pty = (int64_t)((bool)s->using_pty & (bool)!s->merge_output);
    struct termios tc;

    setup_child_tc(&tc);

    capture_launch(s, cmd, argv);
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
                                             /*  "termcap",
                                                 &tc,*/
                                             "pty",
                                             (int64_t)s->using_pty,
                                             "merge",
                                             (int64_t)s->merge_output,
                                             "err_pty",
                                             err_pty));
}

static inline void
truncate_all_match_data(n00b_session_t *s,
                        n00b_string_t  *trunc,
                        n00b_capture_t  kind)
{
    // TODO: replace for kind.
    // ALSO, somewhere else, enforce a max buffer size.
    //
    if (kind == N00B_CAPTURE_STDIN) {
        s->input_match_buffer = trunc;
    }
    else {
        s->input_match_buffer = NULL;
    }

    if (kind == N00B_CAPTURE_INJECTED) {
        s->injection_match_buffer = trunc;
    }
    else {
        s->injection_match_buffer = NULL;
    }
    if (kind == N00B_CAPTURE_STDOUT) {
        s->stdout_match_buffer = trunc;
    }
    else {
        s->stdout_match_buffer = NULL;
    }
    if (kind == N00B_CAPTURE_STDERR) {
        s->stderr_match_buffer = trunc;
    }
    else {
        s->stderr_match_buffer = NULL;
    }
    s->launch_command = NULL;
    s->got_user_input = false;
    s->got_injection  = false;
    s->got_stdout     = false;
    s->got_stderr     = false;
    s->got_prompt     = false;
    s->got_run        = false;
    s->got_winch      = false;
}

static void
successful_match(n00b_session_t *session,
                 n00b_trigger_t *trigger,
                 n00b_string_t  *str,
                 int             last,
                 n00b_capture_t  kind,
                 void           *match)
{
    trigger->cur_match = match;

    if (trigger->fn) {
        (*trigger->fn)(session, trigger, match);
    }

    if (trigger->next_state) {
        n00b_list_t *l = find_state(session, trigger->next_state);
        if (l) {
            session->cur_user_state = l;
        }
        else {
            N00B_RAISE(n00b_cformat("No such state: [|#|]",
                                    trigger->next_state));
        }
    }

    if (last == n00b_string_codepoint_len(str)) {
        truncate_all_match_data(session, NULL, 0);
    }
    else {
        n00b_string_t *s = n00b_string_slice(str, last, -1);
        truncate_all_match_data(session, s, kind);
    }
}

static bool
try_pattern_match(n00b_session_t *session,
                  n00b_string_t  *str,
                  n00b_trigger_t *trigger,
                  n00b_capture_t  kind)
{
    n00b_regex_t *re    = trigger->match_info.regexp;
    n00b_match_t *match = n00b_match(re, str);

    if (!match) {
        return false;
    }
    successful_match(session, trigger, str, match->end, kind, match);

    return true;
}

static bool
try_substring_match(n00b_session_t *session,
                    n00b_string_t  *str,
                    n00b_trigger_t *trigger,
                    n00b_capture_t  kind)
{
    n00b_string_t *sub   = trigger->match_info.substring;
    int64_t        where = n00b_string_find(str, sub);

    if (where == -1) {
        return false;
    }

    int last = where + n00b_string_codepoint_len(sub);
    successful_match(session, trigger, str, last, kind, sub);

    return true;
}

static n00b_trigger_t *
match_base(n00b_session_t *s,
           n00b_string_t  *str,
           n00b_capture_t  kind,
           n00b_list_t    *l)
{
    int n = n00b_list_len(l);

    for (int i = 0; i < n; i++) {
        n00b_trigger_t *trigger = n00b_list_get(l, i, NULL);

        if (trigger->target != kind) {
            continue;
        }
        if (kind == N00B_CAPTURE_CMD_EXIT) {
            successful_match(s, trigger, NULL, 0, kind, NULL);
            return trigger;
        }
        if (kind == N00B_CAPTURE_WINCH) {
            successful_match(s, trigger, NULL, 0, kind, NULL);
            return trigger;
        }

        if (trigger->kind == N00B_TRIGGER_SUBSTRING) {
            if (try_substring_match(s, str, trigger, kind)) {
                return trigger;
            }
        }
        else {
            assert(trigger->kind == N00B_TRIGGER_PATTERN);
            if (try_pattern_match(s, str, trigger, kind)) {
                return trigger;
            }
        }
    }

    return NULL;
}

static n00b_trigger_t *
try_match(n00b_session_t *s, n00b_string_t *str, n00b_capture_t kind)
{
    n00b_trigger_t *result = NULL;
    if (s->cur_user_state) {
        result = match_base(s, str, kind, s->cur_user_state);
    }
    if (!result && s->global_actions) {
        result = match_base(s, str, kind, s->global_actions);
    }

    return result;
}

static inline bool
handle_input_matching(n00b_session_t *s)
{
    // This is more complicated than most matching, because we wait
    // until after the state machine processes it to decide what
    // to proxy down to the subprocess (in case some user input is
    // meant to be a control command only).
    n00b_string_t  *to_write = s->input_match_buffer;
    n00b_trigger_t *trigger  = try_match(s,
                                        s->input_match_buffer,
                                        N00B_CAPTURE_STDIN);
    if (!s->passthrough_input) {
        return trigger != NULL;
    }
    if (!trigger || !trigger->quiet) {
        n00b_write(s->subprocess->subproc_stdin, to_write);
        s->got_user_input = false;
        return true;
    }

    int64_t mstart, mend;
    if (trigger->kind == N00B_TRIGGER_SUBSTRING) {
        mstart = n00b_string_find(to_write, trigger->match_info.substring);
        mend   = mstart + trigger->match_info.substring->codepoints;
    }
    else {
        n00b_match_t *m = trigger->cur_match;
        mstart          = m->start;
        mend            = m->end;
    }

    trigger->cur_match = NULL;

    n00b_string_t *slice;

    if (mstart != 0) {
        assert(mstart != -1);
        slice = n00b_string_slice(to_write, 0, mstart);
        n00b_write(s->subprocess->subproc_stdin, slice);
    }
    if (mend != to_write->codepoints) {
        slice = n00b_string_slice(to_write, mend, -1);
        n00b_write(s->subprocess->subproc_stdin, slice);
    }

    return true;
}

static inline void
scan_for_matches(n00b_session_t *s)
{
    if (s->got_winch) {
        try_match(s, NULL, N00B_CAPTURE_WINCH);
        s->got_winch = false;
    }

    // The first shell prompt we get is from us starting up, it's not
    // an exit event. We track it by removing the unneeded reference
    // to the control stream (since we're already subscribed).
    if (s->got_prompt && s->subproc_ctrl_stream) {
        s->subproc_ctrl_stream = NULL;
        return;
    }

    // When we get here, we're looking at any data that came in async
    // since the last scan.
    //
    // For each item, we look at the current state to see if there
    // might be something to check. If not, we truncate the associated
    // buffer.  If there is, we check all appropriate rules. Currently
    // we are not combining regexes.
    if (s->got_user_input && handle_input_matching(s)) {
        return;
    }
    else {
        s->got_user_input = false;
    }

    // No state, no scanning. This does need to happen after we
    // process user input though, because we don't proxy user input
    // until after the state machine gets a chance to pass on the
    // input.
    if (!s->cur_user_state) {
        truncate_all_match_data(s, NULL, 0);
        return;
    }

    if (s->got_injection) {
        if (try_match(s, s->injection_match_buffer, N00B_CAPTURE_INJECTED)) {
            return;
        }
        else {
            s->got_injection = false;
        }
    }

    if (s->got_stdout) {
        if (try_match(s, s->stdout_match_buffer, N00B_CAPTURE_STDOUT)) {
            return;
        }
        else {
            s->got_stdout = false;
        }
    }

    if (s->got_stderr) {
        if (try_match(s, s->stderr_match_buffer, N00B_CAPTURE_STDERR)) {
            return;
        }
        else {
            s->got_stderr = false;
        }
    }

    if (s->got_prompt) {
        if (try_match(s, NULL, N00B_CAPTURE_CMD_EXIT)) {
            return;
        }
        else {
            s->got_prompt = false;
        }
    }

    if (s->got_run) {
        if (try_match(s, s->launch_command, N00B_CAPTURE_CMD_RUN)) {
            return;
        }

        s->got_run = false;
    }

    if (s->got_timeout) {
        if (try_match(s, NULL, N00B_CAPTURE_SYSTEM)) {
            return;
        }
        s->early_exit = true;
    }
}

static inline void
check_for_total_timeout(n00b_session_t *s)
{
    n00b_duration_t *now = n00b_now();

    s->last_event = now;

    if (!s->end_time) {
        return;
    }

    if (n00b_duration_gt(now, s->end_time)) {
        s->early_exit = true;
    }
}

static inline void
run_control_loop(n00b_session_t *s)
{
    while (!s->subprocess->exited && !s->early_exit) {
        n00b_condition_lock_acquire(&s->control_notify);
        n00b_condition_wait(&s->control_notify);
        n00b_condition_lock_release(&s->control_notify);
        scan_for_matches(s);
        check_for_total_timeout(s);
    }
}

static inline void
finish_capture(n00b_session_t *session)
{
    n00b_stream_t *s;

    // If it's paused, we still want to write out the proper header.
    s = session->saved_capture;
    if (!s) {
        s = atomic_read(&session->capture_stream);
        if (!s) {
            return;
        }
    }
    // TODO: seek and write the max terminal dimensions.
}

static inline bool
session_cleanup(n00b_session_t *session)
{
    n00b_string_t *s;
    bool           result = !session->early_exit;

    // Reset session data to remove session-specific stuff.
    if (session->end_time) {
        session->end_time = n00b_duration_diff(session->start_time,
                                               session->end_time);
    }
    session->start_time = NULL;
    session->last_event = NULL;
    truncate_all_match_data(session, NULL, 0);
    n00b_close(session->stdin_injection);
    session->cur_user_state = NULL;
    if (session->rc_filename) {
        unlink(session->rc_filename->data);
        session->rc_filename = NULL;
    }
    if (session->subproc_ctrl_stream) {
        s = n00b_stream_get_name(session->subproc_ctrl_stream);
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
    n00b_raw_condition_init(&session->control_notify);
    session->log_cursor      = NULL;
    session->next_capture_id = 0;
    session->paused_clock    = false;
    session->early_exit      = false;

    return result;
}

static inline void
make_gap_adjustment(n00b_log_cursor_t *cap, n00b_cap_event_t *cur)
{
    if (!cap->max_gap) {
        return;
    }
    n00b_duration_t *diff = n00b_duration_diff(cur->timestamp, cap->cursor);
    if (n00b_duration_gt(diff, cap->max_gap)) {
        diff                = n00b_duration_diff(diff, cap->max_gap);
        cap->cursor         = n00b_duration_add(cap->cursor, diff);
        cap->absolute_start = n00b_duration_add(cap->absolute_start, diff);
    }
}

static void
write_event(n00b_cap_event_t *event, n00b_stream_t *loc)
{
    n00b_string_t *s = NULL;
    if (n00b_type_is_string(n00b_get_my_type(event->contents))) {
        s = event->contents;
    }
    else {
        s = n00b_ansi_nodes_to_string(event->contents, true);
    }

    n00b_stream_raw_fd_write(loc, n00b_string_to_buffer(s));
}

static inline void
replay_one_event(n00b_session_t *session, n00b_cap_event_t *event)
{
    if (!(event->kind & session->log_cursor->channels)) {
        return;
    }

    switch (event->kind) {
    case N00B_CAPTURE_STDIN:
    case N00B_CAPTURE_INJECTED:
        write_event(event, session->log_cursor->stdin_dst);
        return;
    case N00B_CAPTURE_STDOUT:
        write_event(event, session->log_cursor->stdout_dst);
        return;
    case N00B_CAPTURE_STDERR:
        write_event(event, session->log_cursor->stderr_dst);
        return;
    case N00B_CAPTURE_SPAWN:
    case N00B_CAPTURE_END:
    case N00B_CAPTURE_CMD_RUN:
    case N00B_CAPTURE_CMD_EXIT:
    case N00B_CAPTURE_WINCH:
    default:
        return;
    }
}

static inline n00b_cap_event_t *
get_next_event(n00b_stream_t *s, double scale)
{
    if (n00b_at_eof(s)) {
        return NULL;
    }
    n00b_cap_event_t *result = n00b_gc_alloc_mapped(n00b_cap_event_t,
                                                    N00B_GC_SCAN_ALL);
    result->id               = read_capture_int32(s);
    result->timestamp        = read_timestamp(s, scale);
    result->kind             = read_capture_byte(s);

    switch (result->kind) {
    case N00B_CAPTURE_STDIN:
    case N00B_CAPTURE_CMD_RUN:
        result->contents = read_capture_payload_str(s);
        break;
    case N00B_CAPTURE_INJECTED:
    case N00B_CAPTURE_STDOUT:
    case N00B_CAPTURE_STDERR:
        result->contents = read_ansi_payload_str(s);
        break;
    case N00B_CAPTURE_WINCH:
        result->contents = read_winch_payload(s);
        break;
    case N00B_CAPTURE_SPAWN:
        result->contents = read_spawn_payload(s);
        break;
    case N00B_CAPTURE_END:
        return NULL;
    default:
        break;
    }

    return result;
}

static inline bool
has_newline(n00b_cap_event_t *event)
{
    if (n00b_type_is_string(n00b_get_my_type(event->contents))) {
        return n00b_string_find(event->contents, n00b_cached_newline()) != -1;
    }

    n00b_list_t *l = event->contents;
    int          n = n00b_list_len(l);

    for (int i = 0; i < n; i++) {
        n00b_ansi_node_t *n = n00b_list_get(l, i, NULL);
        if (n->kind != N00B_ANSI_C0_CODE) {
            continue;
        }
        if (n->ctrl.ctrl_byte == '\n') {
            return true;
        }
    }

    return false;
}

// Returns true if there's more to process.
static inline bool
process_partial_replay(n00b_session_t *session)
{
    n00b_log_cursor_t *cap = session->log_cursor;
    n00b_cap_event_t  *cur = cap->cache;

    if (!cur) {
        cur = get_next_event(session->log_cursor->log, cap->time_scale);
        if (!cur) {
            return false;
        }
        make_gap_adjustment(cap, cur);
    }

    n00b_duration_t *cutoff = n00b_duration_diff(n00b_now(),
                                                 cap->absolute_start);

    bool ignore_durations = false;
    bool keep_going       = false;

    while (true) {
        if (cur->kind == N00B_CAPTURE_STDIN
            || cur->kind == N00B_CAPTURE_INJECTED) {
            if (cap->pause_before_input) {
                if (cap->got_unpause) {
                    cap->got_unpause = false;
                    ignore_durations = true;
                    keep_going       = cap->play_input_to_newline;
                }
                else {
                    if (!keep_going) {
                        cap->paused = true;
                        return true;
                    }
                }
            }
        }
        if (ignore_durations) {
            if (n00b_duration_gt(cur->timestamp, cutoff)) {
                cap->cache = cur;
                return true;
            }
        }

        replay_one_event(session, cur);

        if (ignore_durations && keep_going
            && (cur->kind == N00B_CAPTURE_STDIN
                || cur->kind == N00B_CAPTURE_INJECTED)) {
            keep_going = !has_newline(cur);
        }

        cur = get_next_event(session->log_cursor->log, cap->time_scale);

        if (!cur) {
            cap->cache = NULL;
            return false;
        }
    }
}

static void *
run_replay_loop(n00b_session_t *session)
{
    n00b_log_cursor_t *cap = session->log_cursor;
    n00b_stream_t     *log = cap->log;

    if (!read_capture_magic(log)) {
        N00B_CRAISE("Stream is not a capture file.");
    }
    n00b_duration_t *soff = read_timestamp(log, 1.0);
    n00b_duration_t *now  = n00b_now();
    n00b_duration_t *dur;
    // read_winch_payload(log);
    n00b_duration_t  wait;
    n00b_duration_t  leftover;

    if (!cap->cursor) {
        cap->cursor = soff;
    }

    cap->absolute_start = now;

    while (true) {
        n00b_condition_lock_acquire(&cap->unpause_notify);
        if (cap->paused) {
            n00b_condition_wait(&cap->unpause_notify);
            dur                 = n00b_duration_diff(now, n00b_now());
            cap->absolute_start = n00b_duration_add(cap->absolute_start, dur);
        }
        n00b_condition_lock_release(&cap->unpause_notify);

        now         = n00b_now();
        dur         = n00b_duration_diff(cap->absolute_start, now);
        cap->cursor = n00b_duration_add(cap->absolute_start, dur);

        if (!process_partial_replay(session)) {
            return NULL;
        }

        wait = *n00b_duration_diff(cap->cache->timestamp, cap->cursor);

        while (true) {
            int x = n00b_nanosleep_raw(&wait, &leftover);
            if (x) {
                wait = leftover;
            }
        }
    }
}

static inline void
setup_replay(n00b_session_t *session)
{
    if (!session->log_cursor) {
        return;
    }
    n00b_raw_condition_init(&session->log_cursor->unpause_notify);
    n00b_thread_spawn((void *)run_replay_loop, session);
}

bool
n00b_session_run_cinematic(n00b_session_t *session)
{
    if (!session->log_cursor) {
        N00B_CRAISE("No replay event stream added");
    }
    session->log_cursor->paused = false;
    n00b_stream_t *cb           = n00b_callback_open(user_read_cb, session);
    n00b_io_subscribe_to_reads(n00b_stdin(), cb, NULL);
    start_session_clock(session);
    return run_replay_loop(session);
    return session_cleanup(session);
}

bool
n00b_session_run(n00b_session_t *session)
{
    if (n00b_session_is_active(session)) {
        return false;
    }
    if (session->log_cursor && session->log_cursor->cinematic) {
        return n00b_session_run_cinematic(session);
    }

    start_session_clock(session);
    setup_state_handling(session);

    if (session->launch_command) {
        setup_user_command(session);
    }
    else {
        setup_shell(session);
        setup_control_stream(session);
    }

    setup_replay(session);
    n00b_proc_spawn(session->subprocess);
    run_control_loop(session);
    capture(session, N00B_CAPTURE_END, NULL);
    finish_capture(session);
    return session_cleanup(session);
}

void
n00b_session_pause_recording(n00b_session_t *s)
{
    if (s->saved_capture) {
        return;
    }
    s->last_event    = n00b_now();
    s->paused_clock  = true;
    s->saved_capture = atomic_read(&s->capture_stream);

    CAS(&s->capture_stream, &s->saved_capture, NULL);
}

void
n00b_session_continue_recording(n00b_session_t *s)
{
    n00b_stream_t *expected = NULL;

    if (!s->saved_capture) {
        return;
    }
    CAS(&s->capture_stream, &expected, s->saved_capture);
    s->paused_clock       = false;
    n00b_duration_t *now  = n00b_now();
    n00b_duration_t *diff = n00b_duration_diff(now, s->last_event);
    s->last_event         = now;

    if (s->skew) {
        s->skew = n00b_duration_add(s->skew, diff);
    }
    else {
        s->skew = diff;
    }
}

// A cinematic replay does not spawn a new session; It writes to the
// user's terminal only. Nothing gets spawned. State-machine stuff
// still works, but for USER INPUT ONLY. And you can turn on recording
// if you want, but one cannot inject anything to the terminal, and
// you won't be able to re-capture things, only capture what the user
// types.

n00b_session_t *
n00b_cinematic_replay(n00b_stream_t *stream)
{
    n00b_session_t *result = n00b_gc_alloc_mapped(n00b_session_t,
                                                  N00B_GC_SCAN_ALL);
    n00b_set_type(result, n00b_type_session());
    n00b_log_cursor_t *c = n00b_gc_alloc_mapped(n00b_log_cursor_t,
                                                N00B_GC_SCAN_ALL);
    result->log_cursor   = c;
    c->cinematic         = true;
    c->time_scale        = 1.0;
    c->channels          = N00B_CAPTURE_STDIN | N00B_CAPTURE_STDOUT
                | N00B_CAPTURE_STDERR | N00B_CAPTURE_WINCH;
    c->stdin_dst  = n00b_stdout();
    c->stdout_dst = n00b_stdout();
    c->stderr_dst = n00b_stderr();

    return result;
}

void
n00b_add_command_replay(n00b_session_t *s, bool start_paused)
{
    n00b_log_cursor_t *c = n00b_gc_alloc_mapped(n00b_log_cursor_t,
                                                N00B_GC_SCAN_ALL);
    c->stdin_dst         = s->subprocess->subproc_stdin;
    c->channels          = N00B_CAPTURE_STDIN;
    c->paused            = start_paused;
    c->time_scale        = 1.0;
    s->log_cursor        = c;

    return;
}

static void
n00b_session_state_init(n00b_session_state_t *state, va_list args)
{
    n00b_session_t *s = va_arg(args, n00b_session_t *);
    n00b_string_t  *n = va_arg(args, n00b_string_t *);

    n00b_list_init((n00b_list_t *)state, args);
    if (!s->user_states) {
        s->user_states = n00b_dict(n00b_type_string(),
                                   n00b_type_session_state());
    }
    if (!hatrack_dict_add(s->user_states, n, state)) {
        N00B_CRAISE("State name already exists.");
    }
}

static void
n00b_session_trigger_init(n00b_trigger_t *trigger, va_list args)
{
    n00b_session_state_type *state           = NULL;
    n00b_regex_t            *regexp          = NULL;
    n00b_string_t           *substring       = NULL;
    n00b_trigger_fn          fn              = NULL;
    n00b_string_t           *next_state      = NULL;
    bool                     on_typing       = false;
    bool                     on_stdout       = false;
    bool                     on_stderr       = false;
    bool                     on_injections   = false;
    bool                     on_timeout      = false;
    bool                     on_subproc_exit = false;
    bool                     quiet           = false;

    n00b_karg_va_init(args);
    n00b_kw_ptr("regexp", regexp);
    n00b_kw_ptr("substring", substring);
    n00b_kw_ptr("state", state);
    n00b_kw_ptr("callback", fn);
    n00b_kw_ptr("next_state", next_state);
    n00b_kw_bool("on_typing", on_typing);
    n00b_kw_bool("on_stdout", on_stdout);
    n00b_kw_bool("on_stderr", on_stderr);
    n00b_kw_bool("on_injections", on_injections);
    n00b_kw_bool("on_timeout", on_timeout);
    n00b_kw_bool("on_subproc_exit", on_subproc_exit);
    n00b_kw_bool("quiet", quiet);

    int n = (int)on_typing + (int)on_stdout + (int)on_stderr
          + (int)on_injections + (int)on_timeout + (int)on_subproc_exit;

    if (n != 1) {
        N00B_CRAISE("Trigger must have exactly one target");
    }

    if ((regexp || substring) && (on_timeout || on_subproc_exit)) {
        N00B_CRAISE("Cannot string match for that trigger type");
    }

    if (state) {
        n00b_list_append((n00b_list_t *)state, trigger);
    }

    trigger->next_state = next_state;
    trigger->fn         = fn;
    trigger->quiet      = quiet;

    if (on_timeout) {
        trigger->kind = N00B_TRIGGER_TIMEOUT;
        return;
    }
    if (on_subproc_exit) {
        trigger->kind = N00B_TRIGGER_ANY_EXIT;
        return;
    }
    if ((!regexp && !substring) || (regexp && substring)) {
        N00B_CRAISE("Must provide either a regex or a substring");
    }

    if (regexp) {
        trigger->kind              = N00B_TRIGGER_PATTERN;
        trigger->match_info.regexp = regexp;
    }
    else {
        trigger->kind                 = N00B_TRIGGER_SUBSTRING;
        trigger->match_info.substring = substring;
    }
    if (on_typing) {
        trigger->target = N00B_CAPTURE_STDIN;
        return;
    }
    if (on_stdout) {
        trigger->target = N00B_CAPTURE_STDOUT;
        return;
    }
    if (on_stderr) {
        trigger->target = N00B_CAPTURE_STDERR;
        return;
    }
    if (on_injections) {
        trigger->target = N00B_CAPTURE_INJECTED;
        return;
    }
    n00b_unreachable();
}

static inline n00b_builtin_t
n00b_get_action_type(void *action)
{
    if (!n00b_in_heap(action)) {
        return N00B_T_CALLBACK;
    }

    return n00b_get_my_type(action)->base_index;
}

// We provide a bunch of helpers for instantiating triggers.
n00b_trigger_t *
__n00b_trigger(n00b_session_t *session,
               char           *trigger_name,
               void           *cur_state,
               void           *match,
               void           *action,
               bool            quiet)

{
    n00b_list_t *cur;

    if (n00b_type_is_string(n00b_get_my_type(cur_state))) {
        cur = n00b_get_session_state_by_name(session, cur_state);
    }
    else {
        cur = cur_state;
    }

    n00b_builtin_t action_type = n00b_get_action_type(action);
    char          *atkw        = "next_state";
    char          *mkkw        = NULL;

    if (match && n00b_type_is_string(n00b_get_my_type(match))) {
        mkkw = "substring";
    }
    else {
        mkkw = "regexp";
    }

    if (action_type == N00B_T_STRING) {
        action = n00b_get_session_state_by_name(session, action);
    }
    else {
        if (action_type == N00B_T_CALLBACK) {
            atkw = "callback";
        }
    }
    return n00b_new(n00b_type_session_trigger(),
                    n00b_kw("state",
                            cur,
                            atkw,
                            action,
                            mkkw,
                            match,
                            trigger_name,
                            n00b_ka(true),
                            "quiet",
                            n00b_ka(quiet)));
}

n00b_list_t *
n00b_capture_stream_extractor(n00b_stream_t *stream, int events)
{
    if (!n00b_stream_can_read(stream)) {
        N00B_CRAISE("Capture stream must be open and readable.");
    }

    n00b_ev2_cookie_t *cookie = stream->cookie;
    lseek(cookie->id, 0, SEEK_SET);

    if (!read_capture_magic(stream)) {
        N00B_CRAISE("Stream is not a capture file.");
    }

    /*n00b_duration_t *soff   = */ read_timestamp(stream, 1.0);
    //    /*struct winsize  *wsz    = */ read_winch_payload(stream);

    n00b_list_t *result = n00b_list(n00b_type_ref());

    while (true) {
        n00b_cap_event_t *e = get_next_event(stream, 1.0);
        if (!e) {
            return result;
        }

        if (e->kind & events) {
            n00b_list_append(result, e);
        }
    }
}

const n00b_vtable_t n00b_session_vtable = {
    .methods = {
        [N00B_BI_CONSTRUCTOR] = (n00b_vtable_entry)n00b_session_init,
    },
};

const n00b_vtable_t n00b_session_state_vtable = {
    .methods = {
        [N00B_BI_CONSTRUCTOR] = (n00b_vtable_entry)n00b_session_state_init,
    },
};

const n00b_vtable_t n00b_session_trigger_vtable = {
    .methods = {
        [N00B_BI_CONSTRUCTOR] = (n00b_vtable_entry)n00b_session_trigger_init,
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
// Handle capturing window size and WINCH.
// STATE Actions for injecting, closing, interacting, removing interaction,
//                   breaking the pipes, changing speed, ...
// Ensure resulting terminal is big enough for the largest window size by
//    sticking max dimensions in the top, and monitoring winch on playbacks.
// Default actions in state machines (e.g., timeout)
// Add local timeout (gotta look through the states)
// Add an 'idle timeout'
// Extractors from the capture (autoexpect)
