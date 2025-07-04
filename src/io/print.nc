#include "n00b.h"

static inline void
n00b_do_write(n00b_stream_t *stream, void *data, bool noblock)
{
    if (noblock) {
        n00b_queue(stream, data);
    }
    else {
        n00b_write(stream, data);
    }
}

void
_n00b_print(void *first, ...)
{
    va_list           args;
    void             *cur        = first;
    n00b_karg_info_t *_n00b_karg = NULL;
    n00b_stream_t    *stream     = NULL;
    n00b_codepoint_t  sep        = ' ';
    n00b_codepoint_t  end        = '\n';
    bool              flush      = false;
    bool              nocolor    = false;
    bool              noblock    = false;
    int               numargs    = 1;

    va_start(args, first);

    if (first == NULL) {
        n00b_do_write(n00b_stdout(), n00b_cached_newline(), true);
        return;
    }

    n00b_type_t *t = n00b_get_my_type(first);

    if (n00b_type_is_stream(t)) {
        stream = first;
        first  = va_arg(args, void *);
        cur    = first;
        if (!first) {
            n00b_do_write(n00b_stdout(), n00b_cached_newline(), true);
            return;
        }
    }

    if (n00b_type_is_keyword(t)) {
        _n00b_karg = first;
        va_arg(args, int); // will be a 0
        numargs = va_arg(args, int);

        if (numargs < 0) {
            numargs = 0;
            cur     = va_arg(args, void *);
        }
        else {
            _n00b_karg = n00b_get_kargs_and_count(args, &numargs);
            numargs++;
        }
    }

    if (_n00b_karg != NULL) {
        if (!stream) {
            n00b_kw_ptr("stream", stream);
        }

        n00b_kw_codepoint("sep", sep);
        n00b_kw_codepoint("end", end);
        n00b_kw_bool("flush", flush);
        n00b_kw_bool("no_color", nocolor);
        n00b_kw_bool("no_block", noblock);
    }

    if (stream == NULL) {
        stream = n00b_stdout();
    }

    n00b_string_t *s;

    for (int i = 0; i < numargs; i++) {
        if (i && sep) {
            n00b_do_write(stream, n00b_string_from_codepoint(sep), noblock);
        }
        if (n00b_type_is_string(n00b_get_my_type(cur))) {
            s = cur;
        }
        else {
            s = n00b_to_string(cur);
        }
        if (nocolor && s->styling && s->styling->num_styles) {
            s = n00b_string_reuse_text(s);
        }

        n00b_do_write(stream, s, noblock);
        cur = va_arg(args, void *);
    }

    if (end) {
        s = n00b_string_from_codepoint(end);
        n00b_do_write(stream, s, noblock);
    }

    va_end(args);
}

void *
__n00b_c_value(void *item)
{
    if (!n00b_in_heap(item)) {
        return item;
    }
    n00b_type_t *t = n00b_get_my_type(item);

    if (!t) {
        return item;
    }

    if (n00b_type_is_string(t)) {
        n00b_string_t *s = item;
        return s->data;
    }

    return item;
}
