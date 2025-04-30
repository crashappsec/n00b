#include "n00b.h"

static inline void
n00b_do_write(n00b_stream_t *stream, void *data, bool noblock)
{
    if (noblock) {
        n00b_write(stream, data);
    }
    else {
        n00b_write_blocking(stream, data, NULL);
    }
}

void
_n00b_print(n00b_obj_t first, ...)
{
    va_list           args;
    n00b_obj_t        cur        = first;
    n00b_karg_info_t *_n00b_karg = NULL;
    n00b_stream_t    *old_stream = NULL;
    n00b_channel_t   *stream     = NULL;
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

    if (n00b_type_is_stream(t) || n00b_type_is_file(t)) {
        stream = first;
        first  = va_arg(args, n00b_obj_t);
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
        }
        cur = va_arg(args, n00b_obj_t);
    }
    else {
        _n00b_karg = n00b_get_kargs_and_count(args, &numargs);
        numargs++;
    }

    if (_n00b_karg != NULL) {
        if (!stream) {
            n00b_kw_ptr("stream", old_stream);
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
        cur = va_arg(args, n00b_obj_t);
    }

    if (end) {
        s = n00b_string_from_codepoint(end);
        n00b_do_write(stream, s, noblock);
    }

#if 0
// CURRENTLY needs implementing
    if (flush) {
        n00b_flush(stream);
    }
#endif

    va_end(args);
}

#ifdef N00B_DEBUG
void
_n00b_cprintf(char *fmt, int64_t num_params, ...)
{
    va_list args, copy;

    va_start(args, num_params);
    va_copy(copy, args);

    char  *p   = fmt;
    char  *end = p + strlen(fmt);
    void  *obj;
    void **start     = (void *)args;
    int    arg_index = 0;

    while (p < end) {
        if (*p != '%' || (*++p == '%')) {
            p++;
            continue;
        }

        int l_ct = 0;
        while (p < end) {
            switch (*p) {
            case 'l':
                l_ct++;
                p++;
                break;
            case 'd':
            case 'i':
            case 'u':
            case 'o':
            case 'x':
            case 'X':
                if (!l_ct) {
                    va_arg(args, int32_t);
                    arg_index++;
                }
                else {
                    va_arg(args, int64_t);
                    arg_index++;
                }
                p++;
                break;
            case 'f':
            case 'F':
            case 'e':
            case 'E':
            case 'g':
            case 'G':
            case 'a':
            case 'A':
            case 'p':
                va_arg(args, int64_t);
                arg_index++;
                p++;
                break;
            case 'c':
                // Might need to IFDEF this based on platform.
                va_arg(args, int32_t);
                arg_index++;
                p++;
                break;
            case '%':
                break;
            case 'n':
                p++;
                break;
            case 's':
                obj = va_arg(args, void *);
                n00b_string_t *s;

                if (n00b_in_heap(obj)) {
                    if (n00b_type_is_string(n00b_get_my_type(obj))) {
                        s = obj;
                    }
                    else {
                        s = n00b_cformat("«#»", obj);
                    }
                }
                else {
                    s = n00b_cformat("«#»", obj);
                }

                start[arg_index] = n00b_rich_to_ansi(s, NULL);

                p++;
                arg_index++;

                break;
            default:
                p++;
                continue;
            }
            // break from inner while loop.
            p++;
            break;
        }
    }

    int result;

    result = vfprintf(stderr, fmt, copy);
    // Can happen when stderr has O_NONBLOCK set.
    n00b_assert(result >= 0);
}
#endif
