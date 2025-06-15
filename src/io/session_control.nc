#define N00B_USE_INTERNAL_API
#include "n00b.h"

#define capture(x, y, z) n00b_session_capture(x, y, z)

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

void
n00b_process_control_buffer(n00b_session_t *session, n00b_string_t *s)
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

static void
control_cb(n00b_buf_t *buf, n00b_session_t *session)
{
    n00b_string_t *s = n00b_utf8(buf->data, buf->byte_len);

    n00b_dlog_io("Control callback beginning.");
    n00b_lock_acquire(&session->control_notify);
    n00b_process_control_buffer(session, s);
    n00b_condition_notify_one(&session->control_notify);
    n00b_lock_release(&session->control_notify);
    n00b_dlog_io("Control callback ending.");
}

void
n00b_session_setup_control_stream(n00b_session_t *s)
{
    n00b_filter_spec_t *lb = n00b_filter_apply_line_buffering();
    n00b_stream_t      *cb = n00b_new_callback_stream((void *)control_cb,
                                                 s,
                                                 lb);
    n00b_stream_subscribe_read(s->subproc_ctrl_stream, cb, false);
}
static void
user_read_cb(n00b_buf_t *data, n00b_session_t *session)
{
    n00b_dlog_io("User input callback beginning.");
    capture(session, N00B_CAPTURE_STDIN, data);

    n00b_lock_acquire(&session->control_notify);
    session->got_user_input = true;
    add_buf_to_match_buffer(session, N00B_CAPTURE_STDIN, data);
    n00b_condition_notify(&session->control_notify, auto_unlock : true);
    n00b_dlog_io("User input callback ending.");
}

static void
subproc_injection_cb(void *msg, n00b_session_t *session)
{
    n00b_dlog_io("Injection callback beginning.");

    if (n00b_type_is_list(n00b_get_my_type(msg))) {
        msg = n00b_ansi_nodes_to_string(msg, true);
    }

    n00b_buf_t *input = msg;

    n00b_lock_acquire(&session->control_notify);
    capture(session, N00B_CAPTURE_INJECTED, input);
    session->got_injection = true;
    n00b_queue(session->subprocess->subproc_stdin, input);
    add_buf_to_match_buffer(session, N00B_CAPTURE_INJECTED, input);
    n00b_condition_notify(&session->control_notify, auto_unlock : true);
    n00b_dlog_io("Injection callback ending.");
}

void
n00b_subproc_read_stdout(n00b_list_t *ansi_nodes, n00b_session_t *session)
{
    n00b_dlog_io("stdout callback beginning.");
    n00b_lock_acquire(&session->control_notify);
    capture(session, N00B_CAPTURE_STDOUT, ansi_nodes);
    session->got_stdout = true;
    add_ansi_to_match_buffer(session, N00B_CAPTURE_STDOUT, ansi_nodes);
    if (session->passthrough_output) {
        n00b_string_t *s = n00b_ansi_nodes_to_string(ansi_nodes, true);
        n00b_queue(n00b_stdout(), s);
    }

    n00b_condition_notify(&session->control_notify, auto_unlock : true);
    n00b_dlog_io("stdout callback ending.");
}

void
n00b_subproc_read_stderr(n00b_list_t    *ansi_nodes,
                         n00b_session_t *session)
{
    n00b_dlog_io("stderr callback beginning.");
    n00b_lock_acquire(&session->control_notify);
    capture(session, N00B_CAPTURE_STDERR, ansi_nodes);
    session->got_stderr = true;
    add_ansi_to_match_buffer(session, N00B_CAPTURE_STDERR, ansi_nodes);
    if (session->passthrough_output) {
        n00b_string_t *s = n00b_ansi_nodes_to_string(ansi_nodes, true);
        n00b_queue(n00b_stderr(), s);
    }
    n00b_condition_notify(&session->control_notify, auto_unlock : true);
    n00b_dlog_io("stderr callback ending.");
}

void
n00b_session_setup_user_read_cb(n00b_session_t *s)
{
    n00b_stream_t *cb = n00b_new_callback_stream(user_read_cb, s);
    n00b_stream_subscribe_read(n00b_stdin(), cb, false);
}

void
n00b_session_setup_state_handling(n00b_session_t *s)
{
    // The callbacks used here add to the capture IF we're capturing,
    // and they handle matching / state transitions.

    n00b_unbuffer_stdin();
    n00b_unbuffer_stdout();
    n00b_unbuffer_stderr();

    n00b_stream_t *cb;
    n00b_session_setup_user_read_cb(s);

    cb                 = n00b_new_callback_stream(subproc_injection_cb,
                                  s,
                                  n00b_filter_parse_ansi(false));
    s->stdin_injection = cb;

    cb           = n00b_new_callback_stream(n00b_subproc_read_stdout,
                                  s,
                                  n00b_filter_parse_ansi(false));
    s->stdout_cb = n00b_list(n00b_type_stream());

    n00b_list_append(s->stdout_cb, cb);

    if (!s->merge_output) {
        cb           = n00b_new_callback_stream(n00b_subproc_read_stderr,
                                      s,
                                      n00b_filter_parse_ansi(false));
        s->stderr_cb = n00b_list(n00b_type_stream());

        n00b_list_append(s->stderr_cb, cb);
    }

    if (s->start_state) {
        s->cur_user_state = n00b_session_find_state(s, s->start_state);
    }

    n00b_named_lock_init(&s->control_notify,
                         N00B_NLT_CV,
                         "session control");
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
        capture(s, N00B_CAPTURE_MAX_TIME, NULL);
        s->early_exit = true;
    }
}

static inline bool
should_stop(n00b_session_t *s)
{
    if (s->subprocess && s->subprocess->exited) {
        s->early_exit = true;
        return true;
    }

    if (s->log_cursor.finished && s->log_cursor.cinematic) {
        return true;
    }

    return s->early_exit;
}

void
n00b_session_run_control_loop(n00b_session_t *s)
{
    int64_t timeout = 0;
    int64_t iota    = 10000000;
    int64_t now;
    int64_t last;
    int64_t target;

    // Avoid notification while scanning for matches.
    n00b_lock_acquire(&s->control_notify);
    n00b_session_scan_for_matches(s);

    while (!should_stop(s)) {
        now = n00b_ns_timestamp();
        if (s->max_event_gap) {
            timeout = n00b_ns_from_duration(s->max_event_gap);
        }

        if (timeout < iota) {
            target = timeout;
        }
        else {
            target = iota;
        }

        // This loop polls in 1/1000th of a second increments to see
        // if it should exit, even if there's no timeout. If there is
        // a timeout, it checks at each poll spot to see if it should
        // bail.
        last = now;
        while (true) {
            if (n00b_condition_wait(&s->control_notify,
                                    timeout : target, auto_unlock : true)) {
                // If the branch is true, we got a timeout.
                // It returns NULL when we
                now = n00b_ns_timestamp();
                if (last + target < now) {
                    capture(s, N00B_CAPTURE_TIMEOUT, NULL);
                    //                    s->got_timeout = true;
                    break;
                }
                last = now;
                // We still scan for matches on our timeout, so
                // we don't automatically condinue;
            }
        }
        n00b_session_scan_for_matches(s);
        check_for_total_timeout(s);
        if (should_stop(s)) {
            return;
        }
        now = n00b_ns_timestamp();
    }
}
