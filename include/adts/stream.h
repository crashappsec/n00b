#pragma once
#include "n00b.h"

extern n00b_obj_t *n00b_stream_raw_read(n00b_stream_t *, int64_t, char *);
extern size_t     n00b_stream_raw_write(n00b_stream_t *, int64_t, char *);
extern void       n00b_stream_write_object(n00b_stream_t *, n00b_obj_t, bool);
extern bool       n00b_stream_at_eof(n00b_stream_t *);
extern int64_t    n00b_stream_get_location(n00b_stream_t *);
extern void       n00b_stream_set_location(n00b_stream_t *, int64_t);
extern void       n00b_stream_close(n00b_stream_t *);
extern void       n00b_stream_flush(n00b_stream_t *);
extern void       _n00b_print(n00b_obj_t, ...);
extern n00b_obj_t *n00b_stream_read_all(n00b_stream_t *);

#define n00b_print(s, ...) _n00b_print(s, N00B_VA(__VA_ARGS__))

static inline bool
n00b_stream_put_binary(n00b_stream_t *s, void *p, uint64_t len)
{
    return n00b_stream_raw_write(s, len, (char *)p) == len;
}

static inline bool
n00b_stream_putc(n00b_stream_t *s, char c)
{
    return n00b_stream_raw_write(s, 1, &c) == 1;
}

static inline bool
n00b_stream_putcp(n00b_stream_t *s, n00b_codepoint_t cp)
{
    uint8_t utf8[5];

    size_t n = utf8proc_encode_char(cp, utf8);
    utf8[n]  = 0;

    return n00b_stream_raw_write(s, n, (char *)utf8) == n;
}

static inline int
n00b_stream_puts(n00b_stream_t *s, char *c)
{
    return n00b_stream_raw_write(s, strlen(c), c);
}

static inline n00b_obj_t
n00b_stream_read(n00b_stream_t *stream, int64_t len)
{
    return n00b_stream_raw_read(stream, len, NULL);
}

static inline void
n00b_stream_puti(n00b_stream_t *s, int64_t n)
{
    if (!n) {
        n00b_stream_putc(s, '0');
        return;
    }
    if (n < 0) {
        n00b_stream_putc(s, '-');
        n *= -1;
    }
    char buf[21] = {
        0,
    };
    char *p = buf + 20;

    while (n != 0) {
        *--p = (n % 10) + '0';
        n /= 10;
    }

    n00b_stream_puts(s, p);
}

// For nim integration.
static inline n00b_stream_t *
n00b_string_instream(n00b_str_t *instring)
{
    return n00b_new(n00b_type_stream(), n00b_kw("instring", n00b_ka(instring)));
}

static inline n00b_stream_t *
n00b_buffer_instream(n00b_buf_t *inbuf)
{
    return n00b_new(n00b_type_stream(),
                   n00b_kw("buffer", n00b_ka(inbuf), "read", n00b_ka(true)));
}

static inline n00b_stream_t *
n00b_buffer_outstream(n00b_buf_t *outbuf, bool append)
{
    return n00b_new(n00b_type_stream(),
                   n00b_kw("buffer",
                          n00b_ka(outbuf),
                          "read",
                          n00b_ka(false),
                          "write",
                          n00b_ka(true),
                          "append",
                          n00b_ka(append)));
}

static inline n00b_stream_t *
buffer_iostream(n00b_buf_t *buf)
{
    return n00b_new(n00b_type_stream(),
                   n00b_kw("buffer",
                          n00b_ka(buf),
                          "read",
                          n00b_ka(true),
                          "write",
                          n00b_ka(true)));
}

static inline n00b_stream_t *
n00b_file_instream(n00b_str_t *filename, n00b_builtin_t output_type)
{
    return n00b_new(n00b_type_stream(),
                   n00b_kw("filename",
                          n00b_ka(filename),
                          "out_type",
                          n00b_ka(output_type)));
}

static inline n00b_stream_t *
n00b_file_outstream(n00b_str_t *filename, bool no_create, bool append)
{
    return n00b_new(n00b_type_stream(),
                   n00b_kw("filename",
                          n00b_ka(filename),
                          "read",
                          n00b_ka(false),
                          "append",
                          n00b_ka(append),
                          "write",
                          n00b_ka(true),
                          "can_create",
                          n00b_ka(no_create)));
}

static inline n00b_stream_t *
n00b_file_iostream(n00b_str_t *filename, bool no_create)
{
    return n00b_new(n00b_type_stream(),
                   n00b_kw("filename",
                          n00b_ka(filename),
                          "read",
                          n00b_ka(false),
                          "write",
                          n00b_ka(true),
                          "no_create",
                          n00b_ka(no_create)));
}

n00b_stream_t *n00b_get_stdin();
n00b_stream_t *n00b_get_stdout();
n00b_stream_t *n00b_get_stderr();
void          n00b_init_std_streams();

static inline bool
n00b_stream_using_cookie(n00b_stream_t *s)
{
    return (bool)(s->flags & N00B_F_STREAM_USING_COOKIE);
}

static inline int
n00b_stream_fileno(n00b_stream_t *s)
{
    if (n00b_stream_using_cookie(s)) {
        return -1;
    }

    return fileno(s->contents.f);
}
