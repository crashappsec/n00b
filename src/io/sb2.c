// Provide a generic interface to libevent that abstracts
// away as much as possible.
//
// We try to treat file descriptors, strings and callbacks as uniformly
// as possible-- certainly behavior and implementation will depend on
// the type of object.
//
// The core abstraction is a subscribable object
// (n00b_sb_party_t). Objects that are data sources can be subscribed to
// for reading; sinks can be subscribed to for writing. Whenever it
// makes sense, subscribable objects can be BOTH sinks and sources.
//
//
// Specific types of subscribable objects:
//
// 1. A known socket. Here, we force it into non-blocking mode, as
//    this is just easier and better (the alternative is to read a
//    single byte every time
//
// 2. A known file descriptor (known to be a file because we opened
//    it, for instance if we opened a file, forked a process or
//    created a pipe). While these may be special devies, as long as
//    we know there is data to read, the read() call will return. For
//    writes, we are always guaranteed that a write of up to PIPE_BUF
//    bytes will succeed (thanks, Posix!!) If a file is receiving, it
//    can have some options set, see below.
//
// 3. A generic file descriptor we are handed. Here, we test to see if
//    it's a socket, and go w/ path #1. Otherwise, we go w/ path
//    #2. For receiving, sockets can have some options set (see
//    below).
//
// 4. A chunk of memory, using our buffer abstraction. This is not
//    currently implemented, though.
//
// 5. A callback (which can only be a sink, not a source).
//
// 6. Signals, although this has restrictions; only one switchboard in
//    an application can subscribe to signals, so there is a special
//    internal signal switchboard, and a different API. Also not
//    implemented yet.
//
// Also, you can 'publish' to any sink, sending it either a buffer, or
// a string. Buffers are always passed as-is. Sinks can set up multiple
// options for strings passed to them (not all mutually exclusive):
//
// - They can be sent as UTF-8.
// - They can be sent as UTF-32.
// - They can have formatting ALWAYS applied (as if you're writing to
//   a terminal).
// - Formatting can always be stripped.
// - They can have ansi formatting applied to FD sinks that are TTYs.
// - They can have ansi formatting applied to FD sinks that are sockets.
// - They can have ansi formatting applied to buffer outputs.
// - They can have ansi formatting applied to string outputs.
// - For sockets, buffers, and files that are not TTYs, you can
//   preserve metadata without an ANSI conversion. This is done by
//   marshaling the string, so it assumes receivers know the protocol.
//
// Eventually, you be able to write raw marshaled objects (and
// eventually, objects marshaled into JSON). These options also expect
// the receiver to understand the protocol. FD sinks cannot receive
// this kind of message if they're ttys. No sink can receive multiple
// kinds of messages (without creation of separate subscribable
// objects).

// When publishing, any appropriate subscribable objects will have the
// contents buffered, and will not begin sending until the next
// polling loop occurs.

// Note that we do NOT allow cycles in the subscription graph. Such a
// subscription will raise an error (currently with the internal
// exception handling).

// There are also possible failure conditions, such as EPIPE. You
// can subscribe to errors seprately.
//
// Subscriptions that have errored generally have internal
// subscription info maintained, but the switchboard instance will not
// attempt to write to sinks that errored, or read from source that
// have errored. If a source errors, and the source is also a sink, we
// assume the write-side is still live, until we get an error when we
// try to write.
//
// By default, there is no explicit exit condition for a
// switchboard. The caller needs to determine their own exit
// conditions.
//
// The only exception is for switchboards that still have active
// descriptors, but are no longer reachable (meaning the garbage
// collector determines it's no longer in use).  In this case, you may
// choose what should happen whenever the GC examines it:
//
// 1. You can always shut it down, but not close active file descriptors.
// 2. You can always shut it down, and close all active file descriptors.
// 3. You can always leave it up unless no further communication is possible.
// 4. You can call a clean-up routine that allows you to try to close
//    everything. In this option, you can decide to ressurect your
//    reference, as the GC won't release until the next collect, even if
//    the user indicated it can be freed.
//
// Note that we assume that all callers respect the lock around buffers,
// if used.

#include "n00b.h"
#include <event2/event.h>

static void
n00b_setup_libevent2(void)
{
    event_set_mem_functions(n00b_malloc_compat,
                            n00b_realloc_compat,
                            n00b_free_compat);
}

void
n00b_switchboard_init(n00b_switchboard_t *sb, va_list args)
{
    bool allow_files      = true;
    bool iocp             = false;
    bool risky_changelist = false;

    n00b_karg_va_init(args);
    n00b_kw_bool("allow_files", allow_files);
    n00b_kw_bool("iocp", iocp);
    n00b_kw_bool("epoll_changelist", risky_changelist);

    pthread_once(&system_sb_inited, n00b_setup_libevent2);

    event_config *sb_config = event_config_new();

    if (allow_files) {
        event_config_require_features(sb_config, EV_FEATURE_FDS);
        sb->files_ok = true;
    }

    if (iocp) {
        event_config_set_flag(sb_config, EVENT_BASE_FLAG_STARTUP_IOCP);
    }

    if (risky_changelist) {
        event_config_set_flag(sb_config, EVENT_BASE_FLAG_EPOLL_USE_CHANGELIST);
    }

    sb->event_ctx = event_base_new_with_config(sb_config);
}

static inline void
n00b_sb_fd_write_async(n00b_sb_party_t *party, void *value)
{
    if (!party->pending_writes) {
        party->pending_writes = n00b_list(n00b_get_my_type(value));
    }

    n00b_list_append(party->pending_writes, value);
    event_add(party->my_switchboard, party->sink_event);
}

static inline void
sb_cache_write(n00b_sb_party_t *party, void *msg)
{
    if (!(party->cache_policy & n00b_sb_cache_write)) {
        return;
    }

    if (!party->write_cache) {
        party->write_cache = n00b_list(n00b_type_ref());
    }

    n00b_list_append(party->write_cache, msg);
}

static inline void
sb_cache_read(n00b_sb_party_t *party, void *msg)
{
    if (!party->cache_policy & n00b_sb_cache_read) {
        return;
    }

    if (!party->read_cache) {
        party->read_cache = n00b_list(n00b_type_ref());
    }

    n00b_list_append(party->read_cache, msg);
}

static n00b_str_t *
ensure_proper_str_type(n00b_str_t *s, n00b_sb_munge_policy munge, bool orig_u8)
{
    bool out_u8    = false;
    int  specified = munge & (n00b_sb_str_pass_u8 | n00b_sb_str_pass_u32);

    if (specified) {
        if (specified & n00b_sb_str_pass_u8) {
            out_u8 = true;
        }
    }
    else {
        out_u8 = orig_u8;
    }

    if (out_u8) {
        return n00b_to_utf8(s);
    }

    return n00b_to_utf32(s);
}

// For now, we'll use the old interface; we need to rewrite.
static n00b_utf32_t *
ansify(n00b_str_t *s)
{
    n00b_buf_t    *buf    = n00b_buffer_empty();
    n00b_stream_t *stream = n00b_buffer_outstream(buf, true);
    size_t         w      = n00b_terminal_width();

    n00b_ansi_render_to_width(s, w, 0, stream);

    return n00b_new(n00b_type_u32(),
                    n00b_kw("length",
                            n00b_ka(buf->byte_len),
                            "cstring",
                            n00b_ka(buf->data)));
}

static void *
sb_munge(void            *raw,
         n00b_sb_party_t *src,
         n00b_sb_party_t *dst)
{
    n00b_sb_munge_policy munge = dst->munge_policy;
    n00b_type_t         *t     = n00b_get_my_type(raw);

    if (munge) {
        if (munge & n00b_sb_n00b_object) {
            N00B_CRAISE("Not implemented yet.");
        }

        if (n00b_type_is_buffer(t)) {
            if (munge & (n00b_sb_str_pass_u8 | n00b_sb_str_pass_u32)) {
                N00B_CRAISE(
                    "subscriber explicitly expects utf8, but "
                    "was passed a raw buffer.");
            }
        }
        else {
            n00b_str_t *s = (n00b_str_t *)raw;

            bool utf8_in = n00b_str_is_u8(s);

            if (n00b_sb_party_is_fd(dst)) {
                if (n00b_sb_party_is_socket(dst)) {
                    if (munge & n00b_sb_str_ansi_socket) {
                        s = sb_ansify(s);
                    }
                    goto finish_up;
                }

                if (n00b_sb_party_is_tty(dst)) {
                    if (munge & n00b_sb_str_ansi_tty) {
                        s = sb_ansify(s);
                    }
                    goto finish_up;
                }

                if (munge & n00b_str_ansi_default) {
                    s = sb_ansify(s);
                }
            }
        }
    }

finish_up:
    return ensure_proper_string_type(s, munge, utf8_in);
}

static void
notify_subscriber_list(n00b_sb_party_t *party, bool read_op)
{
    c4m_list_t *l = read_op ? party->source_subs : party->sink_subs;
    int         n = n00b_list_len(l);
    void       *raw;

    // The only time this loop should run more than once is if preserve_fmt
    // is on, and there are multiple string objects buffered.
    while (true) {
        raw = sb_extract_postable_data(party, read_op);

        if (!raw) {
            return;
        }
    }

    for (int i = 0; i < n; i++) {
        n00b_sb_party_t *listener = n00b_list_get(l, i, NULL);
        void            *to_post  = sb_munge(raw, party, listener);

        if (listener->kind & n00b_sb_subable_buffer) {
            // TODO when we do new locking buffers.
            // n00b_sb_buffer_write(listener, to_post);
            N00B_CRAISE("Not implemented yet.");
            continue;
        }

        if (listener->kind & n00b_sb_subable_callback) {
            n00b_recv_cb cb = listener->target.cb;
            cb(listener, to_post, party);
            continue;
        }

        n00b_sb_fd_write_async(listener, to_post);
    }
}

static inline void
notify_read_subscribers(n00b_sb_party_t *party)
{
    notify_subscriber_list(party, true);
}

static inline void
notify_write_subscribers(n00b_sb_party_t *party)
{
    notify_subscriber_list(party, false);
}

void
n00b_sb_fd_write_async(n00b_sb_party_t *party, void *msg)
{
    // This API will only accept strings and buffers.  If we were
    // internally posted, the message will already be of the 'right'
    // type for a write, but if the message came from the user, we may
    // need to convert.
    //
    // When we enable marshalling, the user will be expected to use a
    // different interface; internal posting will pass a raw buffer
    // containing the object.

    if (!party->kind & n00b_sb_sink) {
        N00B_CRAISE("Cannot write to read-only object.");
    }

    if (!n00b_in_heap(msg)) {
        goto invalid;
    }

    n00b_type_t *t = n00b_get_my_type(msg);

    if (!n00b_type_is_string(t) && !n00b_type_is_buffer(t)) {
invalid:
        N00B_CRAISE(
            "The value passed for writing to a fd must "
            "be a string or a buffer.");
    }

    // Don't bother with 0-length writes.
    if (n00b_type_is_string(t)) {
        n00b_str_t *s = (n00b_str_t *)value;
        if (!n00b_codepoint_len(s)) {
            return;
        }
    }
    else {
        n00b_buf_t *b = (n00b_buf_t *)value;
        if (!b->byte_len) {
            return;
        }
    }

    n00b_sb_fd_write_async(party, (n00b_str_t *)msg);
    notify_write_subscribers(party);
    sb_cache_write(party, msg);
    return;
}

static n00b_utf8_t *
sb_extract_u8(n00b_sb_party_t *party, bool read)
{
    n00b_sb_buffer_t *cache = read ? party->read_cache : party->write_cache;
    n00b_str_t       *result;
    char             *start, *end;

    start = cache->buf->data + cache->read_offset;
    end   = cache->buf->data + cache->buf->buf_len;

    while (true) {
        char *p = end - 1;
        char  c = *p;

        if (!(((uint8_t)c) & 0x80)) {
            result             = n00b_new(n00b_type_utf8(),
                              n00b_kw("length",
                                      n00b_ka(end - start),
                                      "cstring",
                                      start));
            cache->read_offset = end - cache->buf->data;

            return result;
        }

        if (p == start) {
            return NULL;
        }
        end = p;
    }
}

static n00b_utf32_t *
sb_extract_u32(n00b_sb_party_t *party, bool read)
{
    n00b_sb_buffer_t *cache = read ? party->read_cache : party->write_cache;
    n00b_str_t       *result;
    char             *start, *end;
    int               len;
    int               leftover;

    start = cache->buf->data + cache->read_offset;
    end   = cache->buf->data + cache->buf->buf_len;

    len      = end - start;
    leftover = len % 4;
    len      = len - leftover;
    end      = end - leftover;

    if (!len) {
        return NULL;
    }

    result = n00b_new(n00b_type_utf32(),
                      n00b_kw("length",
                              n00b_ka(len),
                              "cstring",
                              start));

    cache->read_offset = end - cache->buf->data;

    return result;
}

static n00b_buf_t *
sb_extract_raw(n00b_sb_party_t *party, bool read)

{
    n00b_sb_buffer_t *cache  = read ? party->read_cache : party->write_cache;
    char             *start  = cache->buf->data + cache->read_offset;
    char             *end    = cache->buf->data + cache->buf->buf_len;
    int               len    = end - start;
    n00b_buf_t       *result = n00b_new(n00b_type_buf(),
                                  n00b_kw("length",
                                          n00b_ka(len),
                                          "raw",
                                          n00b_ka(start)));

    cache->read_offset = end;

    return result;
}

static void *
sb_extract_marshaled_object(n00b_sb_party *party, bool read)
{
    N00B_CRAISE("Not yet implemented.");
}

static inline void *
sb_extract_postable_data(n00b_sb_party *party, bool read)
{
    void *extraction;

    if (party->munge_policy & n00b_sb_n00b_object) {
        extraction = sb_extract_marshaled_object(party, read);
    }
    else {
        if (party->munge_policy & n00b_sb_str_pass_u8) {
            extraction = sb_extract_u8(party, read);
        }
        else {
            if (party->munge_policy & n00b_sb_str_pass_u32) {
                extraction = sb_extract_u32(party, read);
            }
            else {
                extraction = sb_extract_raw(party, read);
            }
        }
    }

    if (extraction) {
        sb_cache_read(party, extraction);
    }

    return extraction;
}

static void
n00b_sb_read_from_fd(evutil_socket_t fd, short event_type, void *info)
{
    // Because we force sockets into non-blocking mode, read() cannot
    // block (with non-sockets, read() always returns).
    //
    // Since we are not edge triggering, we keep things reasonably
    // simple by always attempting to read PIPE_BUF bytes. If we get a
    // short read, we just ignore it.

    n00b_sb_party_t *party = (n00b_sb_party_t *)info;
    char             tmp_buf[PIPE_BUF];
    n00b_buf_t      *buf;
    ssize_t          len = read(fd, tmp_buf, PIPE_BUF);

    if (len < 0) {
        switch (len) {
        case -1:
            if (errno != EINTR && errno != EAGAIN) {
                n00b_raise_errno();
            }
            return;
        case 0:
            // This generally shouldn't happen; we shouldn't get an event
            // if there is nothing to read.
            party->closed = true;
            return;
        default:
            buf = n00b_new(c4m_type_buf(),
                           c4m_kw("raw", buf, "length", c4m_ka(len)));
            if (!party->read_cache) {
                party->read_cache      = n00b_gc_alloc(n00b_sb_buffer_t,
                                                  N00B_GC_SCAN_ALL);
                party->read_cache->buf = buf;
            }
            else {
                party->read_cache->buf = n00b_buffer_add(party->buf, buf);
            }

            notify_read_subscribers(party);
        }
    }
}

static void
sb_progress_cached_write(evutil_socket_t fd, n00b_sb_party_t *party)
{
    n00b_sb_buffer_t *internal_buffer = party->write_cache;
    n00b_buf_t       *raw_buf         = internal_buffer->buf;
    int               blen            = n00b_buffer_len(raw_buf);
    int               offset          = internal_buffer->write_offset;
    int               to_write        = blen - offset;
    char             *p               = raw_buf->data + offset;

    if (to_write > PIPE_BUF) {
        to_write = PIPE_BUF;
    }

    ssize_t written = write(fd, p, to_write);

    if (written == -1) {
        if (errno != EINTR && errno != EAGAIN) {
            n00b_raise_errno();
        }
        return;
    }

    internal_buffer->offset += written;

    if (internal_buffer->offset >= blen) {
        internal_buffer->write_offset = 0;
        internal_buffer->raw_buf      = NULL;
    }
}

static inline void
sb_start_next_write(evutil_socket_t fd, n00b_sb_party_t *party)
{
    if (!party->write_cache) {
        party->write_cache = n00b_gc_alloc_mapped(n00b_sb_buffer_t,
                                                  N00B_GC_ALLOC_SCAN_ALL);
    }

    n00b_sb_buffer_t *b        = party->write_cache;
    void             *contents = n00b_list_get(party->pending_writes, 0, NULL);
    n00b_type_t      *t        = n00b_get_my_type(contents);

    n00b_list_remove(party->pending_writes, 0);

    if (n00b_type_is_buffer(t)) {
        b->buf = (n00b_buf_t *)contents;
    }
    else {
        n00b_str_t *s = (n00b_str_t *)contents;
        b->buf        = n00b_new(n00b_type_buf(),
                          n00b_kw("length",
                                  n00b_ka(n00b_str_byte_len(s)),
                                  "ptr",
                                  n00b_ka(s->data)));
    }

    sb_progress_cached_write(fd, party);
}

static void
n00b_sb_write_to_fd(evutil_socket_t fd, short event_type, void *info)
{
    n00b_sb_party_t *party = (n00b_sb_party_t *)info;
    // We could be in the middle of an existing write. If we are,
    // try to finish it.
    //
    // Otherwise, we must have at least one new value to write; try to
    // write that out.
    //
    // At the end, if there is anything else to write, we re-subscribe
    // to the file descriptor.

    if (party->write_cache && party->write_cache->buf) {
        sb_progress_cached_write(fd, party);
    }
    else {
        sb_start_next_write(fd, party);
    }

    bool add_event = false;

    if (party->write_cache) {
        add_event = true;
    }
    else {
        if (party->pending_writes) {
            if (n00b_list_len(party->pending_writes)) {
                add_event = true;
            }
        }
    }

    if (add_event) {
        event_add(party->my_switchboard, party->sink_event);
    }
}

static inline n00b_sb_party_t *
alloc_subscribable(n00b_sb_subable_kind type, n00b_sb_subable_kind perms)
{
    n00b_sb_party_t *result = n00b_gc_alloc_mapped(n00b_sb_party_t,
                                                   N00B_GC_SCAN_ALL);

    result->kind = type | perms;

    if (perms & n00b_sb_source) {
        result->source_subs = n00b_list(n00b_type_ref());
    }
    if (perms & n00b_sb_sink) {
        result->sink_subs = n00b_list(n00b_type_ref());
    }

    return result;
}

n00b_sb_party_t *
n00b_sb_add_socket(n00b_switchboard_t *sb, evutil_socket_t handle)
{
    n00b_sb_party_t *result = alloc_subscribable(n00b_sb_subable_socket,
                                                 n00b_sb_rw);

    evutil_make_socket_nonblocking(handle);

    result->target.socket  = handle;
    result->source_event   = event_new(sb->event_ctx,
                                     EV_READ | EV_PERSIST,
                                     n00b_sb_read_from_fd,
                                     result);
    result->sink_event     = event_new(sb->event_ctx,
                                   EV_WRITE,
                                   n00b_sb_write_to_fd,
                                   result);
    result->my_switchboard = sb;

    event_add(sb->event_ctx, result->source_event);

    return result;
}

static n00b_sb_party_t *
n00b_sb_add_file_fd(n00b_switchboard_t *sb, int fd)
{
    if (!sb->files_okay) {
        N00B_CRAISE(
            "Switchboard is not configured to accept fds that "
            "are not sockets.");
    }

    n00b_sb_subable_kind perms = 0;

    int flags = fcntl(fd, F_GETFL);

    if (flags & O_RDWR) {
        perms = n00b_sb_rw;
    }
    else {
        if (flags & O_RDONLY) {
            perms = n00b_sb_source;
        }
        else {
            perms = n00b_sb_sink;
        }
    }

    n00b_sb_party_t *result = alloc_subscribable(n00b_sb_subable_fd,
                                                 perms);
    result->my_switchboard  = sb;

    result->target.fd = fd;

    if (isatty(fd)) {
        result->is_tty = true;
    }

    if (perms & n00b_sb_source) {
        result->source_event = event_new(sb->event_ctx,
                                         EV_READ | EV_PERSIST,
                                         n00b_sb_read_from_fd,
                                         result);

        event_add(sb->event_ctx, result->source_event);
    }

    if (perms & n00b_sb_sink) {
        result->sink_event = event_new(sb->event_ctx,
                                       EV_WRITE | EV_ET,
                                       n00b_sb_write_to_fd);
    }

    return result;
}

n00b_sb_party_t *
n00b_sb_add_descriptor(n00b_switchboard_t *sb, int fd)
{
    struct stat info;
    if (fstat(fd, &info)) {
        N00B_CRAISE("File descriptor passed to switchboard is invalid");
    }

    if (S_ISSOCK(info.st_mode)) {
        return n00b_sb_add_socket(sb, (void *)fd);
    }
    else {
        return n00b_sb_add_file_fd(sb, fd);
    }
}

n00b_sb_party_t *
n00b_sb_add_callback(n00b_switchboard_t *sb, n00b_recv_cb *cb, void *thunk)
{
    n00b_sb_party_t *result = alloc_subscribable(n00b_sb_subable_callback,
                                                 n00b_sb_sink);

    result->target.cb      = cb;
    result->thunk          = thunk;
    result->my_switchboard = sb;

    return result;
}

void
n00b_sb_subscribe_to_writes(n00b_sb_party_t *obj, n00b_sb_party_t *subscriber)
{
    if (!(obj->kind & n00b_sb_sink)) {
        N00B_CRAISE(
            "Attempting to subscribe to writes on something that "
            "only is available for reads.");
    }

    if (!(subscriber->kind & n00b_sb_sink)) {
        N00B_CRAISE("Subscriber is only set up to produce data.");
    }
    // TODO: check for a cycle here.

    n00b_list_append(obj->sink_subs, subscriber);
}

void
n00b_sb_subscribe_to_reads(n00b_sb_party_t *obj, n00b_sb_party_t *subscriber)
{
    if (!obj->kind & n00b_sb_source) {
        N00B_CRAISE(
            "Attempting to subscribe to reads on something that "
            "is only available for writes.");
    }

    if (!subscriber->kind & n00b_sb_sink))
        {
            N00B_CRAISE("Subscriber is only set up to produce data.");
        }
    // TODO: check for a cycle here.

    n00b_list_append(obj->source_subs, subscriber);
}

static void
sb_timeout_fired(evutil_socket_t fd, short event, void *ctx)
{
    n00b_soundboard_t *sb = (n00b_soundboard_t *)ctx;
    sb->got_timeout       = true;
}

static inline void
sb_setup_timeout(n00b_soundboard_t *sb, n00b_duration_t *timeout)
{
    if (timeout == NULL) {
        return;
    }

    if (!sb->timeout_inited) {
        sb->timeout_event  = evtimer_new(sb->event_ctx, sb_timeout_fired, sb);
        sb->timeout_inited = true;
    }

    sb->got_timeout = false;
    evtimer_add(sb->timeout_event, n00b_duration_to_timeval(timeout));
}
