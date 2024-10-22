#include "n00b.h"

// Currently, for strings, this does NOT inject ANSI codes.  It's a
// TODO to add an ansi streaming mode for strings.  I also want to add
// an ANSI parser to lift codes out of strings too.
static void
mem_n00b_stream_setup(n00b_cookie_t *c)
{
    if (c->object == NULL) {
        c->object = n00b_new(n00b_type_buffer());
        c->extra  = NULL;
        return;
    }
    else {
        switch (n00b_get_base_type_id(c->object)) {
        case N00B_T_UTF8:
        case N00B_T_UTF32: {
            n00b_str_t *s = (n00b_str_t *)c->object;
            c->extra     = s->data;
            c->eof       = s->byte_len;
            return;
        }
        case N00B_T_BUFFER: {
            n00b_buf_t *b = (n00b_buf_t *)c->object;
            c->extra     = b->data;
            c->eof       = b->byte_len;
            return;
        }
        default:
            assert(false);
        }
    }
}

static size_t
mem_n00b_stream_read(n00b_cookie_t *c, char *dst, int64_t request)
{
    // For buffers, which are mutable, this approach doesn't really
    // work when another thread can also be mutating the buffer.  So
    // for now, don't do that, even though we still take the time to
    // reset the buffer length each call (again, without any locking
    // this is basically meaningless).

    if (c->flags & N00B_F_STREAM_BUFFER_IN) {
        n00b_buf_t *b = (n00b_buf_t *)c->object;

        c->eof = b->byte_len;
    }

    if (request < 0) {
        request += c->eof;
    }

    int64_t read_len = n00b_min((int64_t)request, c->eof - c->position);

    if (read_len <= 0) {
        return 0;
    }

    memcpy(dst, c->extra, read_len);

    c->extra += read_len;
    c->position += read_len;

    return read_len;
}

static size_t
mem_n00b_stream_write(n00b_cookie_t *c, char *buf, int64_t request)
{
    // Same comment on buffers as above.
    n00b_buf_t *b = (n00b_buf_t *)c->object;
    c->eof       = b->byte_len;

    if (c->flags & N00B_F_STREAM_APPEND) {
        c->position = c->eof;
    }

    if (c->position + request > c->eof) {
        c->eof = c->position + request;
        n00b_buffer_resize(b, c->eof);
        c->extra = b->data + c->position;
    }

    memcpy(c->extra, buf, request);
    c->extra += request;
    c->position += request;

    return request;
}

static void
mem_n00b_stream_close(n00b_cookie_t *c)
{
    // Get rid of our heap pointers.
    c->object = NULL;
    c->extra  = NULL;
}

static bool
mem_n00b_stream_seek(n00b_cookie_t *c, int64_t pos)
{
    if (pos < 0) {
        return false;
    }

    if (pos > c->eof) {
        return false;
    }

    c->extra += (pos - c->position);
    c->position = pos;
    return true;
}

void
n00b_cookie_gc_bits(uint64_t *bitmap, n00b_cookie_t *cookie)
{
    n00b_mark_raw_to_addr(bitmap, cookie, &cookie->extra);
}

static inline n00b_cookie_t *
new_mem_cookie()
{
    n00b_cookie_t *result = n00b_gc_alloc_mapped(n00b_cookie_t,
                                               n00b_cookie_gc_bits);

    result->ptr_setup = mem_n00b_stream_setup;
    result->ptr_read  = mem_n00b_stream_read;
    result->ptr_write = mem_n00b_stream_write;
    result->ptr_close = mem_n00b_stream_close;
    result->ptr_seek  = mem_n00b_stream_seek;

    return result;
}

static void
n00b_stream_init(n00b_stream_t *stream, va_list args)
{
    n00b_str_t    *filename      = NULL;
    n00b_str_t    *instring      = NULL;
    n00b_buf_t    *buffer        = NULL;
    n00b_cookie_t *cookie        = NULL;
    FILE         *fstream       = NULL;
    int           fd            = -1;
    bool          read          = true;
    bool          write         = false;
    bool          append        = false;
    bool          no_create     = false;
    bool          close_on_exec = true;
    n00b_builtin_t out_type      = N00B_T_UTF8;

    n00b_karg_va_init(args);
    n00b_kw_ptr("filename", filename);
    n00b_kw_ptr("instring", instring);
    n00b_kw_ptr("buffer", buffer);
    n00b_kw_ptr("cookie", cookie);
    n00b_kw_ptr("cstream", fstream);
    n00b_kw_int32("fd", fd);
    n00b_kw_bool("read", read);
    n00b_kw_bool("write", write);
    n00b_kw_bool("append", append);
    n00b_kw_bool("no_create", no_create);
    n00b_kw_bool("close_on_exec", close_on_exec);
    n00b_kw_int64("out_type", out_type);

    int64_t src_count = 0;
    char    buf[10]   = {
        0,
    };
    int     i     = 0;
    int64_t flags = 0;

    src_count += (int64_t) !!filename;
    src_count += (int64_t) !!instring;
    src_count += (int64_t) !!buffer;
    src_count += (int64_t) !!cookie;
    src_count += (int64_t) !!fstream;
    src_count += (fd >= 0);

    switch (out_type) {
    case N00B_T_UTF8:
        flags = N00B_F_STREAM_UTF8_OUT;
        break;
    case N00B_T_UTF32:
        flags = N00B_F_STREAM_UTF32_OUT;
        break;
    case N00B_T_BUFFER:
        break;
    default:
        N00B_CRAISE("Invalid output type for streams.");
    }

    switch (src_count) {
    case 0:
        N00B_CRAISE("No stream source provided.");
    case 1:
        break;
    default:
        N00B_CRAISE("Cannot provide multiple stream sources.");
    }

    if (append) {
        buf[i++] = 'a';
        buf[i++] = 'b';
        if (read) {
            buf[i++] = '+';
        }

        if (no_create) {
            buf[i++] = 'x';
        }
    }
    else {
        if (write) {
            buf[i++] = 'w';
            buf[i++] = 'b';

            if (read) {
                buf[i++] = '+';
            }
            if (no_create) {
                buf[i++] = 'x';
            }
        }
        else {
            buf[i++] = 'r';
            buf[i++] = 'b';
        }
    }

    if (close_on_exec) {
        buf[i++] = 'e';
    }

    if (read) {
        flags |= N00B_F_STREAM_READ;
    }
    if (write) {
        flags |= N00B_F_STREAM_WRITE;
    }
    if (append) {
        flags |= N00B_F_STREAM_APPEND;
    }

    if (filename != NULL) {
        filename           = n00b_to_utf8(filename);
        stream->contents.f = fopen(filename->data, buf);
        stream->flags      = flags;

err_check:
        if (stream->contents.f == NULL) {
            n00b_raise_errno();
        }

        return;
    }

    if (fstream != NULL) {
        stream->contents.f = fstream;
        return;
    }

    if (fd != -1) {
        stream->contents.f = fdopen(fd, buf);
        goto err_check;
    }

    flags |= N00B_F_STREAM_USING_COOKIE;

    if (cookie == NULL) {
        if (instring && write) {
            N00B_CRAISE(
                "Cannot open string for writing "
                "(they are non-mutable).");
        }

        cookie = new_mem_cookie();
    }
    else {
        if (read && !cookie->ptr_read) {
            N00B_CRAISE(
                "Custom stream implementation does not "
                "support reading.");
        }
        if ((write || append) && !cookie->ptr_write) {
            N00B_CRAISE(
                "Custom stream implementation does not support "
                "writing.");
        }
    }

    if (instring) {
        cookie->object = instring;
        flags |= N00B_F_STREAM_STR_IN;
    }
    if (buffer) {
        cookie->object = buffer;
        flags |= N00B_F_STREAM_BUFFER_IN;
        if (append) {
            cookie->position = n00b_buffer_len(buffer);
        }
    }

    cookie->flags           = flags;
    stream->contents.cookie = cookie;
    stream->flags           = flags;

    if (cookie->ptr_setup != NULL) {
        (*cookie->ptr_setup)(cookie);
    }
}

static n00b_obj_t
n00b_stream_bytes_to_output(int64_t flags, char *buf, int64_t len)
{
    if (flags & N00B_F_STREAM_UTF8_OUT) {
        return n00b_new(n00b_type_utf8(),
                       n00b_kw("cstring", n00b_ka(buf), "length", n00b_ka(len)));
    }

    if (flags & N00B_F_STREAM_UTF32_OUT) {
        return n00b_new(n00b_type_utf32(),
                       n00b_kw("cstring",
                              n00b_ka(buf),
                              "codepoints",
                              n00b_ka(len / 4)));
    }

    else {
        // Else, it's going to a buffer.
        return n00b_new(n00b_type_buffer(),
                       n00b_kw("raw", n00b_ka(buf), "length", n00b_ka(len)));
    }
}

// We generally assume n00b is passing around 64 bit sizes, but when
// dealing w/ the C API, things are often 32 bits (e.g., size_t).
// Therefore, for the internal API, we accept a 64-bit value in, but
// expect the write length to be a size_t because that's what fread()
// will give us.

n00b_obj_t *
n00b_stream_raw_read(n00b_stream_t *stream, int64_t len, char *buf)
{
    // If a buffer is provided, return the length and write into
    // the buffer.
    bool return_len = (buf != NULL);

    if (stream->flags & N00B_F_STREAM_CLOSED) {
        N00B_CRAISE("Stream is already closed.");
    }

    if (!len) {
        if (return_len) {
            return (n00b_obj_t)(0); // I.e., null
        }
        return n00b_stream_bytes_to_output(stream->flags, "", 0);
    }

    int64_t flags  = stream->flags;
    size_t  actual = 0;

    if (!return_len) {
        buf = alloca(len);
    }

    if (!(flags & N00B_F_STREAM_READ)) {
        N00B_CRAISE("Cannot read; stream was not opened with read enabled.");
    }

    if (flags & N00B_F_STREAM_UTF32_OUT) {
        len *= 4;
    }

    if (stream->flags & N00B_F_STREAM_USING_COOKIE) {
        n00b_cookie_t      *cookie = stream->contents.cookie;
        n00b_stream_read_fn f      = cookie->ptr_read;

        actual = (*f)(cookie, buf, len);
    }
    else {
        actual = fread(buf, 1, len, stream->contents.f);
    }

    if (return_len) {
        return (n00b_obj_t)(actual);
    }
    else {
        if (actual) {
            return n00b_stream_bytes_to_output(stream->flags, buf, actual);
        }
        return (n00b_obj_t *)n00b_empty_string();
    }
}

n00b_obj_t *
n00b_stream_read_all(n00b_stream_t *stream)
{
    n00b_list_t *l;
    int         outkind;

    outkind = stream->flags & (N00B_F_STREAM_UTF8_OUT | N00B_F_STREAM_UTF32_OUT);

    switch (outkind) {
    case N00B_F_STREAM_UTF8_OUT:
        l = n00b_new(n00b_type_list(n00b_type_utf8()));
        break;
    case N00B_F_STREAM_UTF32_OUT:
        l = n00b_new(n00b_type_list(n00b_type_utf32()));
        break;
    default:
        // Buffers.
        l = n00b_new(n00b_type_list(n00b_type_buffer()));
        break;
    }
    while (true) {
        n00b_obj_t *one = n00b_stream_raw_read(stream, PIPE_BUF, NULL);

        if (outkind) {
            if (n00b_str_codepoint_len((n00b_str_t *)one) == 0) {
                break;
            }
        }
        else {
            if (n00b_buffer_len((n00b_buf_t *)one) == 0) {
                break;
            }
        }
        n00b_list_append(l, one);
    }

    n00b_str_t *s;

    if (outkind) {
        if (n00b_list_len(l) == 0) {
            s = n00b_empty_string();
        }
        else {
            s = n00b_str_join(l, n00b_empty_string());
        }

        if (outkind == N00B_F_STREAM_UTF8_OUT) {
            return (n00b_obj_t *)n00b_to_utf8(s);
        }
        else {
            return (n00b_obj_t *)n00b_to_utf32(s);
        }
    }
    else {
        return (n00b_obj_t *)n00b_buffer_join(l, NULL);
    }
}
size_t
n00b_stream_raw_write(n00b_stream_t *stream, int64_t len, char *buf)
{
    size_t        actual = 0;
    n00b_cookie_t *cookie = NULL;

    if (stream->flags & N00B_F_STREAM_CLOSED) {
        N00B_CRAISE("Stream is already closed.");
    }

    if (len <= 0) {
        return 0;
    }

    if (stream->flags & N00B_F_STREAM_USING_COOKIE) {
        cookie = stream->contents.cookie;

        n00b_stream_write_fn f = cookie->ptr_write;

        actual = (*f)(cookie, buf, len);
    }
    else {
        actual = fwrite(buf, len, 1, stream->contents.f);
    }

    if (actual > 0) {
        return actual;
    }

    if (cookie != NULL) {
        if (cookie->eof == cookie->position) {
            N00B_CRAISE("Custom stream implementation could write past EOF.");
        }
        else {
            N00B_CRAISE("Custom stream implementation could not finish write.");
        }
    }

    else {
        if (!feof(stream->contents.f)) {
            n00b_raise_errcode(ferror(stream->contents.f));
        }
    }

    return 0;
}

void
n00b_stream_write_object(n00b_stream_t *stream, n00b_obj_t obj, bool ansi)
{
    if (stream->flags & N00B_F_STREAM_CLOSED) {
        N00B_CRAISE("Stream is already closed.");
    }

    n00b_type_t *t = n00b_get_my_type(obj);
    n00b_str_t  *s = n00b_to_str(obj, t);

    if (ansi) {
        n00b_ansi_render(s, stream);
    }
    else {
        s = n00b_to_utf8(s);
        n00b_stream_raw_write(stream, s->byte_len, s->data);
    }
}

void
n00b_stream_write_to_width(n00b_stream_t *stream, n00b_obj_t obj)
{
    if (stream->flags & N00B_F_STREAM_CLOSED) {
        N00B_CRAISE("Stream is already closed.");
    }

    n00b_str_t *s = n00b_to_str(obj, n00b_get_my_type(obj));
    n00b_ansi_render_to_width(s, n00b_terminal_width(), 0, stream);
}

bool
n00b_stream_at_eof(n00b_stream_t *stream)
{
    if (stream->flags & N00B_F_STREAM_CLOSED) {
        return true;
    }

    if (stream->flags & N00B_F_STREAM_USING_COOKIE) {
        n00b_cookie_t *cookie = stream->contents.cookie;

        return cookie->position >= cookie->eof;
    }

    return feof(stream->contents.f);
}

int64_t
n00b_stream_get_location(n00b_stream_t *stream)
{
    if (stream->flags & N00B_F_STREAM_CLOSED) {
        N00B_CRAISE("Stream is already closed.");
    }

    if (stream->flags & N00B_F_STREAM_USING_COOKIE) {
        return stream->contents.cookie->position;
    }

    return ftell(stream->contents.f);
}

void
n00b_stream_set_location(n00b_stream_t *stream, int64_t offset)
{
    if (stream->flags & N00B_F_STREAM_CLOSED) {
        N00B_CRAISE("Stream is already closed.");
    }

    if (stream->flags & N00B_F_STREAM_USING_COOKIE) {
        n00b_cookie_t *cookie = stream->contents.cookie;

        if (offset < 0) {
            offset += cookie->eof;
        }

        n00b_stream_seek_fn fn = cookie->ptr_seek;

        if (fn == NULL) {
            N00B_CRAISE("Custom stream does not have the ability to seek.");
        }

        if ((*fn)(cookie, offset) == false) {
            N00B_CRAISE("Seek position out of bounds for stream.");
        }
    }

    else {
        int result;

        if (offset < 0) {
            offset *= -1;

            result = fseek(stream->contents.f, offset, SEEK_END);
        }
        else {
            result = fseek(stream->contents.f, offset, SEEK_SET);
        }

        if (result != 0) {
            stream->flags = N00B_F_STREAM_CLOSED;
            n00b_raise_errno();
        }
    }
}

void
n00b_stream_close(n00b_stream_t *stream)
{
    if (stream->flags & N00B_F_STREAM_CLOSED) {
        return;
    }

    if (stream->flags & N00B_F_STREAM_USING_COOKIE) {
        n00b_cookie_t *cookie = stream->contents.cookie;

        if (!cookie) {
            return;
        }
        n00b_stream_close_fn fn = cookie->ptr_close;

        cookie->flags = N00B_F_STREAM_CLOSED;

        if (fn) {
            (*fn)(cookie);
        }
    }

    else {
        while (fclose(stream->contents.f) != 0) {
            if (errno != EINTR) {
                break;
            }
        }
    }

    stream->contents.f = NULL;
    stream->flags      = N00B_F_STREAM_CLOSED;
}

void
n00b_stream_flush(n00b_stream_t *stream)
{
    if (stream->flags & N00B_F_STREAM_CLOSED) {
        N00B_CRAISE("Stream is already closed.");
    }

    if (stream->flags & N00B_F_STREAM_USING_COOKIE) {
        return; // Not actually buffering ATM.
    }

    if (fflush(stream->contents.f)) {
        stream->flags = N00B_F_STREAM_CLOSED;
        n00b_raise_errno();
    }
}

void
_n00b_print(n00b_obj_t first, ...)
{
    va_list          args;
    n00b_obj_t        cur       = first;
    n00b_karg_info_t *_n00b_karg = NULL;
    n00b_stream_t    *stream    = NULL;
    n00b_codepoint_t  sep       = ' ';
    n00b_codepoint_t  end       = '\n';
    bool             flush     = false;
    bool             force     = false;
    bool             nocolor   = false;
    int              numargs;
    bool             ansi;
    // These only happen if ANSI is on.
    // And the second only happens when one arg is passed.
    bool             truncate            = true;
    bool             wrap_simple_strings = true;

    va_start(args, first);

    if (first == NULL) {
        n00b_stream_putc(n00b_get_stdout(), '\n');
        return;
    }

    if (n00b_types_are_compat(n00b_get_my_type(first), n00b_type_kargs(), NULL)) {
        _n00b_karg = first;
        va_arg(args, int); // will be a 0
        numargs = va_arg(args, int);

        if (numargs < 0) {
            numargs = 0;
        }
        cur = va_arg(args, n00b_obj_t);
    }
    else {
        _n00b_karg = n00b_get_kargs_and_count(args, &numargs);
        numargs++;
    }

    if (_n00b_karg != NULL) {
        if (!n00b_kw_ptr("stream", stream)) {
            stream = n00b_get_stdout();
        }
        n00b_kw_codepoint("sep", sep);
        n00b_kw_codepoint("end", end);
        n00b_kw_bool("flush", flush);
        n00b_kw_bool("truncate", truncate);

        if (!n00b_kw_bool("n00b_to_color", force)) {
            n00b_kw_bool("no_color", nocolor);
        }
        else {
            if (n00b_kw_bool("no_color", nocolor)) {
                N00B_CRAISE(
                    "Cannot specify `n00b_to_color` and `no_color` "
                    "together.");
            }
        }
    }

    if (stream == NULL) {
        stream = n00b_get_stdout();
    }
    if (force) {
        ansi = true;
    }
    else {
        if (nocolor) {
            ansi = false;
        }
        else {
            int fno = n00b_stream_fileno(stream);

            if (fno == -1 || !isatty(fno)) {
                ansi = false;
            }
            else {
                ansi = true;
            }
        }
    }

    for (int i = 0; i < numargs; i++) {
        if (i && sep) {
            n00b_stream_putcp(stream, sep);
        }

        if (numargs == 1 && wrap_simple_strings) {
            if (n00b_types_are_compat(n00b_get_my_type(cur), n00b_type_utf8(), NULL)) {
                n00b_list_t *lines = n00b_str_wrap(cur, n00b_terminal_width(), 0);
                cur               = n00b_str_join(lines, NULL);
                n00b_ansi_render_to_width(cur, n00b_terminal_width(), 0, stream);
                break;
            }
        }

        if (ansi && truncate) {
            cur = n00b_to_str(cur, n00b_get_my_type(cur));
            n00b_ansi_render_to_width(cur, n00b_terminal_width(), 0, stream);
        }
        else {
            n00b_stream_write_object(stream, cur, ansi);
        }

        cur = va_arg(args, n00b_obj_t);
    }

    if (end) {
        n00b_stream_putcp(stream, end);
    }

    if (flush) {
        n00b_stream_flush(stream);
    }

    va_end(args);
}

static n00b_stream_t *n00b_stream_stdin  = NULL;
static n00b_stream_t *n00b_stream_stdout = NULL;
static n00b_stream_t *n00b_stream_stderr = NULL;

void
n00b_init_std_streams()
{
    if (n00b_stream_stdin == NULL) {
        n00b_stream_stdin  = n00b_new(n00b_type_stream(),
                                   n00b_kw("cstream", n00b_ka(stdin)));
        n00b_stream_stdout = n00b_new(n00b_type_stream(),
                                    n00b_kw("cstream", n00b_ka(stdout)));
        n00b_stream_stderr = n00b_new(n00b_type_stream(),
                                    n00b_kw("cstream", n00b_ka(stderr)));
        n00b_gc_register_root(&n00b_stream_stdin, 1);
        n00b_gc_register_root(&n00b_stream_stdout, 1);
        n00b_gc_register_root(&n00b_stream_stderr, 1);
    }
}

n00b_stream_t *
n00b_get_stdin()
{
    return n00b_stream_stdin;
}

n00b_stream_t *
n00b_get_stdout()
{
    return n00b_stream_stdout;
}

n00b_stream_t *
n00b_get_stderr()
{
    return n00b_stream_stderr;
}

void
n00b_stream_set_gc_bits(uint64_t *bitfield, void *alloc)
{
    n00b_stream_t *s = alloc;

    n00b_mark_raw_to_addr(bitfield, s, &s->contents.cookie);
}

const n00b_vtable_t n00b_stream_vtable = {
    .num_entries = N00B_BI_NUM_FUNCS,
    .methods     = {
        [N00B_BI_CONSTRUCTOR] = (n00b_vtable_entry)n00b_stream_init,
        [N00B_BI_GC_MAP]      = (n00b_vtable_entry)n00b_stream_set_gc_bits,
        [N00B_BI_FINALIZER]   = (n00b_vtable_entry)n00b_stream_close,
    },
};
