#define N00B_USE_INTERNAL_API
#include "n00b.h"

static inline bool
read_capture_magic(n00b_channel_t *s)
{
    char              buf[8];
    n00b_fd_cookie_t *cookie   = n00b_get_channel_cookie(s);
    const int64_t     expected = N00B_SESSION_MAGIC;
    int               r        = 0;
    int64_t          *magic    = (int64_t *)&buf[0];

    while (r < 8) {
        int x = read(cookie->stream->fd, buf + r, sizeof(int64_t));
        if (x == -1) {
            n00b_raise_errno();
        }
        r += x;
    }

    return *magic == expected;
}

static inline n00b_duration_t *
read_timestamp(n00b_channel_t *s, double scale)
{
    n00b_fd_cookie_t *cookie = n00b_get_channel_cookie(s);
    char              buf[sizeof(n00b_duration_t)];

    read(cookie->stream->fd, buf, sizeof(n00b_duration_t));

    return n00b_duration_multiply((n00b_duration_t *)&buf[0], scale);
}

static inline int32_t
read_capture_int32(n00b_channel_t *s)
{
    n00b_fd_cookie_t *cookie = n00b_get_channel_cookie(s);
    char              buf[sizeof(int32_t)];
    read(cookie->stream->fd, buf, sizeof(int32_t));

    int32_t result = *(int32_t *)&buf[0];

    return result;
}

static inline int
read_event_type(n00b_channel_t *s)
{
    n00b_fd_cookie_t *cookie = n00b_get_channel_cookie(s);
    uint16_t          result;

    read(cookie->stream->fd, &result, 2);

    return result;
}

static inline n00b_string_t *
read_capture_payload_str(n00b_channel_t *s)
{
    n00b_buf_t *sbuf;
    int64_t     len;

    sbuf = n00b_channel_unfiltered_read(s, 4);
    len  = *(int32_t *)sbuf->data;
    sbuf = n00b_channel_unfiltered_read(s, len);

    return n00b_buf_to_string(sbuf);
}

static inline n00b_list_t *
read_ansi_payload_str(n00b_channel_t *s)
{
    n00b_string_t *str = read_capture_payload_str(s);
    return n00b_string_to_ansi_node_list(str);
}

static inline struct winsize *
read_winch_payload(n00b_channel_t *s)
{
    const int64_t len  = sizeof(struct winsize *);
    n00b_buf_t   *sbuf = n00b_channel_unfiltered_read(s, len);

    return (struct winsize *)sbuf->data;
}

static inline n00b_cap_spawn_info_t *
read_spawn_payload(n00b_channel_t *s)
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
write_event(n00b_cap_event_t *event, n00b_channel_t *loc)
{
    n00b_string_t *s = NULL;
    if (n00b_type_is_string(n00b_get_my_type(event->contents))) {
        s = event->contents;
    }
    else {
        s = n00b_ansi_nodes_to_string(event->contents, true);
    }

    n00b_channel_write(loc, s);
}

static inline void
replay_one_event(n00b_session_t *session, n00b_cap_event_t *event)
{
    if (!(event->kind & session->log_cursor.channels)) {
        return;
    }

    switch (event->kind) {
    case N00B_CAPTURE_STDIN:
    case N00B_CAPTURE_INJECTED:
        write_event(event, session->log_cursor.stdin_dst);
        return;
    case N00B_CAPTURE_STDOUT:
        write_event(event, session->log_cursor.stdout_dst);
        return;
    case N00B_CAPTURE_STDERR:
        write_event(event, session->log_cursor.stderr_dst);
        return;
    case N00B_CAPTURE_SPAWN:
    case N00B_CAPTURE_END:
    case N00B_CAPTURE_CMD_RUN:
    case N00B_CAPTURE_CMD_EXIT:
    case N00B_CAPTURE_WINCH:
    case N00B_CAPTURE_TIMEOUT:
    case N00B_CAPTURE_MAX_TIME:
    default:
        return;
    }
}

static inline n00b_cap_event_t *
get_next_event(n00b_channel_t *s, double scale)
{
    if (n00b_channel_eof(s)) {
        return NULL;
    }
    n00b_cap_event_t *result = n00b_gc_alloc_mapped(n00b_cap_event_t,
                                                    N00B_GC_SCAN_ALL);
    result->id               = read_capture_int32(s);
    result->timestamp        = read_timestamp(s, scale);
    result->kind             = read_event_type(s);

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
    n00b_log_cursor_t *cap = &session->log_cursor;
    n00b_cap_event_t  *cur = cap->cache;

    if (!cur) {
        cur = get_next_event(cap->log, cap->time_scale);
        if (!cur) {
            cap->finished = true;
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
        if (!ignore_durations) {
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

        cur = get_next_event(cap->log, cap->time_scale);

        if (!cur) {
            cap->cache = NULL;
            return false;
        }
    }
}

void *
n00b_session_run_replay_loop(n00b_session_t *session)
{
    n00b_log_cursor_t *cap = &session->log_cursor;
    n00b_channel_t    *log = cap->log;

    n00b_channel_set_absolute_position(log, 0);

    if (!read_capture_magic(log)) {
        n00b_string_t *err;

        err = n00b_cformat("Stream [|em|][|#|][|/|] is not a capture file.",
                           n00b_channel_get_name(log));
        N00B_RAISE(err);
    }

    n00b_duration_t *soff = read_timestamp(log, 1.0);
    n00b_duration_t *now  = n00b_now();
    n00b_duration_t *dur;
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
            cap->paused         = false;
        }

        n00b_condition_lock_release(&cap->unpause_notify);

        now         = n00b_now();
        dur         = n00b_duration_diff(cap->absolute_start, now);
        cap->cursor = n00b_duration_add(cap->absolute_start, dur);

        if (!process_partial_replay(session)) {
            cap->finished = true;
            return NULL;
        }

        wait = *n00b_duration_diff(cap->cache->timestamp, dur);

        while (!n00b_nanosleep_raw(&wait, &leftover)) {
            wait = leftover;
        }
    }
}

n00b_string_t *
n00b_cinema_toggle_pause(n00b_session_t *s, n00b_trigger_t *t, void *thunk)
{
    s->log_cursor.paused = !s->log_cursor.paused;
    if (!s->log_cursor.paused) {
        n00b_condition_lock_acquire(&s->log_cursor.unpause_notify);
        n00b_condition_notify_one(&s->log_cursor.unpause_notify);
        n00b_condition_lock_release(&s->log_cursor.unpause_notify);
    }
    return s->start_state;
}

n00b_string_t *
n00b_cinema_exit(n00b_session_t *s, n00b_trigger_t *t, void *thunk)
{
    s->early_exit = true;
    return NULL;
}

n00b_string_t *
n00b_cinema_go_faster(n00b_session_t *s, n00b_trigger_t *t, void *thunk)
{
    s->log_cursor.time_scale *= 1.2;
    return s->start_state;
}

n00b_string_t *
n00b_cinema_go_slower(n00b_session_t *s, n00b_trigger_t *t, void *thunk)
{
    s->log_cursor.time_scale *= .8;
    return s->start_state;
}

void
n00b_session_replay_setup_default_controls(n00b_session_t *session)
{
    n00b_string_t *name = n00b_cstring("cinema");

    n00b_new(n00b_type_session_state(), session, name);
    session->start_state = name;

    n00b_input_codepoint_trigger(session,
                                 name,
                                 n00b_cinema_toggle_pause,
                                 ' ',
                                 true);
    n00b_input_codepoint_trigger(session,
                                 name,
                                 n00b_cinema_exit,
                                 '\e',
                                 true);
    n00b_input_codepoint_trigger(session,
                                 name,
                                 n00b_cinema_go_faster,
                                 '+',
                                 true);
    n00b_input_codepoint_trigger(session,
                                 name,
                                 n00b_cinema_go_slower,
                                 '-',
                                 true);

    n00b_session_setup_state_handling(session);
}

// A cinematic replay does not spawn a new session; It writes to the
// user's terminal only. Nothing gets spawned. State-machine stuff
// still works, but for USER INPUT ONLY. And you can turn on recording
// if you want, but one cannot inject anything to the terminal, and
// you won't be able to re-capture things, only capture what the user
// types.

n00b_session_t *
n00b_cinematic_replay_setup(n00b_channel_t *stream)
{
    n00b_session_t    *result = n00b_gc_alloc_mapped(n00b_session_t,
                                                  N00B_GC_SCAN_ALL);
    n00b_log_cursor_t *c      = &result->log_cursor;

    n00b_set_type(result, n00b_type_session());

    c->cinematic  = true;
    c->time_scale = 1.0;
    c->channels   = N00B_CAPTURE_STDIN | N00B_CAPTURE_STDOUT
                | N00B_CAPTURE_STDERR | N00B_CAPTURE_WINCH;
    c->stdin_dst  = n00b_chan_stdout();
    c->stdout_dst = n00b_chan_stdout();
    c->stderr_dst = n00b_chan_stderr();
    c->log        = stream;

    n00b_session_replay_setup_default_controls(result);
    n00b_named_condition_init(&c->unpause_notify, "pause");

    return result;
}

void
n00b_enable_replay(n00b_session_t *s, bool start_paused)
{
    if (s->log_cursor.log) {
        return;
    }

    if (s->subprocess) {
        s->log_cursor.stdin_dst = s->subprocess->subproc_stdin;
    }

    s->log_cursor.paused     = start_paused;
    s->log_cursor.time_scale = 1.0;

    return;
}

void
n00b_enable_command_replay(n00b_session_t *s, bool start_paused)
{
    n00b_enable_replay(s, start_paused);
    s->log_cursor.channels = N00B_CAPTURE_STDIN | N00B_CAPTURE_INJECTED;
}

n00b_list_t *
n00b_capture_stream_extractor(n00b_channel_t *stream, int events)
{
    if (!n00b_channel_can_read(stream)) {
        N00B_CRAISE("Capture stream must be open and readable.");
    }

    n00b_fd_cookie_t *cookie = n00b_get_channel_cookie(stream);
    lseek(cookie->stream->fd, 0, SEEK_SET);

    if (!read_capture_magic(stream)) {
        N00B_CRAISE("Stream is not a capture file.");
    }

    /*n00b_duration_t *soff   = */ read_timestamp(stream, 1.0);

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

void
n00b_session_set_replay_stream(n00b_session_t *session, n00b_channel_t *s)
{
    n00b_enable_replay(session, false);
    session->log_cursor.log = s;
}
