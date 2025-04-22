#define N00B_USE_INTERNAL_API
#include "n00b.h"

#define capture(x, y, z) n00b_session_capture(x, y, z)

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

static inline void
write_capture_header(n00b_stream_t *s)
{
    const int64_t    len = sizeof(int64_t) + sizeof(n00b_duration_t);
    n00b_buf_t      *b   = n00b_new(n00b_type_buffer(), n00b_kw("length", len));
    int64_t         *mp  = (int64_t *)b->data;
    char            *p   = b->data + sizeof(int64_t);
    n00b_duration_t *d   = n00b_now();

    *mp = (int64_t)N00B_SESSION_MAGIC;
    memcpy(p, d, sizeof(n00b_duration_t));
    n00b_stream_raw_fd_write(s, b);
}

static inline void
add_record_header(n00b_cap_event_t *event, n00b_stream_t *s)
{
    const int64_t len = sizeof(n00b_duration_t) + sizeof(uint32_t) + 2;

    n00b_buf_t *b = n00b_new(n00b_type_buffer(), n00b_kw("length", len));
    char       *p = b->data;
    memcpy(p, &event->id, sizeof(uint32_t));
    p += sizeof(uint32_t);
    memcpy(p, event->timestamp, sizeof(n00b_duration_t));
    p += sizeof(n00b_duration_t);
    *(int16_t *)p = (int16_t)event->kind;

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

void
n00b_session_capture(n00b_session_t *s, n00b_capture_t kind, void *contents)
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

// proc does proxy WINCH, so we don't have to do that. However, we do need to
// capture the window size at the right point in the event stream. We *could*
// add a hook to proc, but it's just as easy to subscribe to the event
// ourselves when capture is on.

static void
record_winch(n00b_stream_t *sig, int64_t signal, n00b_session_t *session)
{
    struct winsize *dims = n00b_gc_alloc_mapped(struct winsize,
                                                N00B_GC_SCAN_ALL);
    ioctl(1, TIOCGWINSZ, dims);
    capture(session, N00B_CAPTURE_WINCH, dims);
}

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

    n00b_io_subscribe_to_delivery(p, target, NULL);

    s->capture_stream = p;
    s->capture_policy = policy;
    n00b_io_register_signal_handler(SIGWINCH, (void *)record_winch, s);
}

void
n00b_capture_launch(n00b_session_t *s, n00b_string_t *cmd, n00b_list_t *args)
{
    if (!atomic_read(&s->capture_stream)) {
        return;
    }

    n00b_cap_spawn_info_t *info = n00b_gc_alloc_mapped(n00b_cap_spawn_info_t,
                                                       N00B_GC_SCAN_ALL);

    info->command = cmd;
    info->args    = args;

    capture(s, N00B_CAPTURE_SPAWN, info);
    record_winch(NULL, SIGWINCH, s);
}

void
n00b_session_finish_capture(n00b_session_t *session)
{
    if (session->saved_capture) {
        n00b_close(session->saved_capture);
    }
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
