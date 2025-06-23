#pragma once
#include "n00b.h"

static inline void
n00b_session_set_proxy_input(n00b_session_t *s, bool value)
{
    s->passthrough_input = value;
}

static inline void
n00b_session_set_proxy_output(n00b_session_t *s, bool value)
{
    s->passthrough_output = value;
}

static inline void
n00b_session_stop(n00b_session_t *s)
{
    s->early_exit = true;
}

static inline void
n00b_session_inject(n00b_session_t *s, void *data)
{
    if (!s->stdin_injection) {
        N00B_CRAISE("Session does not support input injection.");
    }

    if (n00b_type_is_string(n00b_get_my_type(data))) {
        data = (void *)n00b_string_to_buffer((void *)data);
    }

    assert(!n00b_type_is_list(n00b_get_my_type(data)));

    n00b_write(s->stdin_injection, data);
}

static inline void
n00b_session_write_to_user(n00b_session_t *s, void *data, bool stdout)
{
    n00b_stream_t *stream;

    if (stdout) {
        stream = n00b_stdout();
    }
    else {
        stream = n00b_stderr();
    }

    if (n00b_type_is_string(n00b_get_my_type(data))) {
        data = (void *)n00b_string_to_buffer((void *)data);
    }

    n00b_write(stream, data);
}

static inline void
n00b_set_replay_stream(n00b_session_t *session, n00b_stream_t *stream)
{
    if (!n00b_stream_can_read(stream)) {
        N00B_CRAISE("Capture stream must be open and readable.");
    }

    session->log_cursor.log    = stream;
    session->log_cursor.paused = true;
}

static inline void
n00b_session_pause_replay(n00b_session_t *s)
{
    if (!s->log_cursor.log) {
        N00B_CRAISE("No replay session");
    }

    s->log_cursor.paused = true;
}

static inline void
n00b_session_continue_replay(n00b_session_t *s)
{
    n00b_log_cursor_t *cursor = &s->log_cursor;

    if (!cursor->log) {
        N00B_CRAISE("No replay session");
    }

    n00b_lock_acquire(&cursor->unpause_notify);

    if (cursor->pause_before_input) {
        cursor->got_unpause = true;
    }
    cursor->paused = false;
    n00b_condition_notify_one(&cursor->unpause_notify);
    n00b_lock_release(&cursor->unpause_notify);
}

static inline void
n00b_set_session_replay_max_gap(n00b_session_t *s, n00b_duration_t *duration)
{
    n00b_log_cursor_t *cursor = &s->log_cursor;
    if (!cursor->log) {
        N00B_CRAISE("No replay session");
    }

    cursor->max_gap = duration;
}

static inline void
n00b_set_session_replay_speed(n00b_session_t *s, double speed)
{
    n00b_log_cursor_t *cursor = &s->log_cursor;
    if (!cursor->log) {
        N00B_CRAISE("No replay session");
    }
    if (speed <= 0) {
        N00B_CRAISE("Invalid speed");
    }

    cursor->time_scale = 1.0 / speed;
}

static inline void
n00b_set_session_replay_pause_before_input(n00b_session_t *s,
                                           bool            to_newline,
                                           bool            then_turn_off)
{
    n00b_log_cursor_t *cursor = &s->log_cursor;

    if (!cursor->log) {
        N00B_CRAISE("No replay session");
    }

    cursor->pause_before_input    = true;
    cursor->play_input_to_newline = to_newline;
    cursor->remove_autopause      = then_turn_off;
}

static inline void
n00b_session_disable_input_pause(n00b_session_t *s)
{
    n00b_log_cursor_t *cursor = &s->log_cursor;

    if (!cursor->log) {
        N00B_CRAISE("No replay session");
    }

    cursor->pause_before_input    = false;
    cursor->play_input_to_newline = false;
    cursor->remove_autopause      = false;
}

static inline n00b_string_t *
n00b_session_capture_filename(n00b_session_t *session)
{
    return session->cap_filename;
}

extern n00b_list_t *n00b_capture_stream_extractor(n00b_stream_t *, int);

static inline n00b_list_t *
n00b_session_capture_extractor(n00b_session_t *session, int events)
{
    n00b_stream_t *stream = session->unproxied_capture;
    n00b_string_t *fname  = n00b_session_capture_filename(session);
    n00b_close(stream);

    stream = n00b_stream_open_file(fname, n00b_header_kargs("read_only", 1ULL));

    session->unproxied_capture = stream;

    if (!stream) {
        N00B_CRAISE("No capture stream exists.");
    }

    return n00b_capture_stream_extractor(stream, events);
}

static inline n00b_list_t *
n00b_session_extract_all_input_raw(n00b_session_t *session)
{
    const int flags = N00B_CAPTURE_STDIN | N00B_CAPTURE_INJECTED;
    return n00b_session_capture_extractor(session, flags);
}

static inline n00b_list_t *
n00b_session_extract_typed_input_raw(n00b_session_t *session)
{
    return n00b_session_capture_extractor(session, N00B_CAPTURE_STDIN);
}

static inline n00b_list_t *
n00b_session_extract_injected_input_raw(n00b_session_t *session)
{
    return n00b_session_capture_extractor(session, N00B_CAPTURE_INJECTED);
}

static inline n00b_list_t *
n00b_session_extract_commands_raw(n00b_session_t *session)
{
    return n00b_session_capture_extractor(session, N00B_CAPTURE_CMD_RUN);
}

static inline n00b_list_t *
n00b_session_extract_output_raw(n00b_session_t *session)
{
    return n00b_session_capture_extractor(session, N00B_CAPTURE_STDOUT);
}

static inline n00b_list_t *
n00b_session_extract_error_raw(n00b_session_t *session)
{
    return n00b_session_capture_extractor(session, N00B_CAPTURE_STDERR);
}

static inline n00b_string_t *
__n00b_flatten_text_events(n00b_list_t *events, bool ensure_nl)
{
    int n = n00b_list_len(events);

    for (int i = 0; i < n; i++) {
        n00b_cap_event_t *event = n00b_list_get(events, i, NULL);
        n00b_string_t    *s     = event->contents;

        if (ensure_nl && !n00b_string_ends_with(s, n00b_cached_newline())) {
            s = n00b_string_concat(s, n00b_cached_newline());
        }

        n00b_list_set(events, i, s);
    }

    return n00b_string_join(events, n00b_cached_empty_string());
}

static inline n00b_string_t *
__n00b_flatten_ansi_events(n00b_list_t *events, bool strip)
{
    int  n = n00b_list_len(events);
    bool f = !strip;

    for (int i = 0; i < n; i++) {
        n00b_cap_event_t *event = n00b_list_get(events, i, NULL);
        n00b_string_t    *s     = n00b_ansi_nodes_to_string(event->contents, f);

        n00b_list_set(events, i, s);
    }

    return n00b_string_join(events, n00b_cached_empty_string());
}

static inline n00b_string_t *
n00b_session_extract_all_input(n00b_session_t *session)
{
    n00b_list_t *events = n00b_session_extract_all_input_raw(session);
    return __n00b_flatten_text_events(events, false);
}

static inline n00b_string_t *
n00b_session_extract_typed_input(n00b_session_t *session)
{
    n00b_list_t *events = n00b_session_extract_typed_input_raw(session);
    return __n00b_flatten_text_events(events, false);
}

static inline n00b_string_t *
n00b_session_extract_injected_input(n00b_session_t *session)
{
    n00b_list_t *events = n00b_session_extract_injected_input_raw(session);
    return __n00b_flatten_text_events(events, false);
}

static inline n00b_string_t *
n00b_session_extract_commands(n00b_session_t *session)
{
    n00b_list_t *events = n00b_session_extract_commands_raw(session);
    return __n00b_flatten_text_events(events, true);
}

static inline n00b_string_t *
n00b_session_extract_output(n00b_session_t *session, bool strip)
{
    n00b_list_t *events = n00b_session_extract_output_raw(session);
    return __n00b_flatten_ansi_events(events, strip);
}

static inline n00b_string_t *
n00b_session_extract_error(n00b_session_t *session, bool strip)
{
    n00b_list_t *events = n00b_session_extract_error_raw(session);
    return __n00b_flatten_ansi_events(events, strip);
}

#define n00b_stream_create_state(session, name) \
    n00b_new(n00b_type_session_state(), session, name, NULL)

static inline n00b_session_state_t *
n00b_get_session_state_by_name(n00b_session_t *s, n00b_string_t *name)
{
    if (!name) {
        if (!s->global_actions) {
            s->global_actions = (void *)n00b_list(n00b_type_trigger());
        }
        return s->global_actions;
    }

    n00b_session_state_t *result = n00b_dict_get(s->user_states, name, NULL);

    if (!result) {
        result = n00b_stream_create_state(s, name);
    }

    return result;
}

extern n00b_trigger_t *__n00b_trigger(n00b_session_t *,
                                      char *,
                                      void *,
                                      void *,
                                      void *,
                                      bool);

static inline n00b_trigger_t *
n00b_subproc_timeout_trigger(n00b_session_t *ss, void *state, void *action)
{
    return __n00b_trigger(ss, "on_timeout", state, NULL, action, false);
}

static inline n00b_trigger_t *
n00b_subproc_exit_trigger(n00b_session_t *ss, void *st, void *action)
{
    return __n00b_trigger(ss, "on_subproc_exit", st, NULL, action, false);
}

static inline n00b_trigger_t *
n00b_input_trigger(n00b_session_t *ss, void *st, void *m, void *a, bool quiet)
{
    return __n00b_trigger(ss, "on_typing", st, m, a, quiet);
}

static inline n00b_trigger_t *
n00b_injection_trigger(n00b_session_t *ss, void *st, void *m, void *a, bool q)
{
    return __n00b_trigger(ss, "on_injections", st, m, a, q);
}

static inline n00b_trigger_t *
n00b_output_trigger(n00b_session_t *ss, void *state, void *match, void *action)
{
    return __n00b_trigger(ss, "on_stdout", state, match, action, false);
}

static inline n00b_trigger_t *
n00b_error_trigger(n00b_session_t *ss, void *state, void *match, void *action)
{
    return __n00b_trigger(ss, "on_stderr", state, match, action, false);
}

static inline n00b_trigger_t *
n00b_auto_trigger(n00b_session_t *ss, void *state, void *action)
{
    return __n00b_trigger(ss, "auto", state, NULL, action, false);
}

static inline n00b_trigger_t *
n00b_input_codepoint_trigger(n00b_session_t  *session,
                             void            *state,
                             void            *action,
                             n00b_codepoint_t cp,
                             bool             quiet)
{
    n00b_string_t *str = n00b_string_from_codepoint(cp);
    return n00b_input_trigger(session, state, str, action, quiet);
}

static inline n00b_trigger_t *
n00b_injection_codepoint_trigger(n00b_session_t  *session,
                                 void            *state,
                                 void            *action,
                                 n00b_codepoint_t cp,
                                 bool             quiet)
{
    n00b_string_t *str = n00b_string_from_codepoint(cp);
    return n00b_injection_trigger(session, state, str, action, quiet);
}

static inline n00b_trigger_t *
n00b_output_codepoint_trigger(n00b_session_t  *session,
                              void            *state,
                              void            *action,
                              n00b_codepoint_t cp)
{
    n00b_string_t *str = n00b_string_from_codepoint(cp);
    return n00b_output_trigger(session, state, str, action);
}

static inline n00b_trigger_t *
n00b_error_codepoint_trigger(n00b_session_t  *session,
                             void            *state,
                             void            *action,
                             n00b_codepoint_t cp)
{
    n00b_string_t *str = n00b_string_from_codepoint(cp);
    return n00b_error_trigger(session, state, str, action);
}

#define n00b_input_newline_trigger(a, b, c, d) \
    n00b_input_codepoint_trigger(a, b, c, n00b_cached_newline(), d)
#define n00b_injection_newline_trigger(a, b, c, d) \
    n00b_input_codepoint_trigger(a, b, c, n00b_cached_newline(), d)
#define n00b_output_newline_trigger(a, b, c) \
    n00b_output_codepoint_trigger(a, b, c, n00b_cached_newline())
#define n00b_error_newline_trigger(a, b, c) \
    n00b_error_codepoint_trigger(a, b, c, n00b_cached_newline())

static inline n00b_trigger_t *
n00b_input_any_trigger(n00b_session_t *session,
                       void           *state,
                       void           *action,
                       bool            quiet)
{
    n00b_regex_t *re = n00b_regex_any_non_empty();
    return n00b_input_trigger(session, state, re, action, quiet);
}

static inline n00b_trigger_t *
n00b_injection_any_trigger(n00b_session_t *session,
                           void           *state,
                           void           *action,
                           bool            quiet)
{
    n00b_regex_t *re = n00b_regex_any_non_empty();
    return n00b_injection_trigger(session, state, re, action, quiet);
}

static inline n00b_trigger_t *
n00b_output_any_trigger(n00b_session_t *session,
                        void           *state,
                        void           *action)
{
    n00b_regex_t *re = n00b_regex_any_non_empty();
    return n00b_output_trigger(session, state, re, action);
}

static inline n00b_trigger_t *
n00b_error_any_trigger(n00b_session_t *session,
                       void           *state,
                       void           *action)
{
    n00b_regex_t *re = n00b_regex_any_non_empty();
    return n00b_error_trigger(session, state, re, action);
}

static inline n00b_trigger_t *
n00b_trigger_add_user_data(n00b_trigger_t *t, void *thunk)
{
    t->thunk = thunk;

    return t;
}

static inline void *
n00b_trigger_get_user_data(n00b_trigger_t *t)
{
    return t->thunk;
}

static inline void
n00b_session_exit(n00b_session_t *s)
{
    s->early_exit = true;
    if (s->subprocess) {
        n00b_proc_close(s->subprocess);
    }
}

static inline void
n00b_session_set_start_state(n00b_session_t *session, n00b_string_t *name)
{
    session->start_state = name;
}

static inline void
n00b_session_enable_ansi_matching(n00b_session_t *session)
{
    session->match_ansi = true;
}

static inline void
n00b_session_disable_ansi_matching(n00b_session_t *session)
{
    session->match_ansi = false;
}
