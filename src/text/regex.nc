// For now, this is via pcre2. I don't really want to use pcre2,
// because it has features that I don't want to provide, and it's a
// bit of a fugly code base (they don't believe in indentation, apparently?!).
//
// I would prefer to use re2 (which I've used in the past), but it's
// a fair bit of work to properly wrap it.

#include "n00b.h"

static pcre2_general_context *n00b_pcre2_global = NULL;
pcre2_compile_context        *n00b_pcre2_compile;
pcre2_match_context          *n00b_pcre2_match;

static void *
pcre2_malloc_wrap(size_t n, void *arg)
{
    return n00b_malloc_wrap(n, arg, __FILE__, __LINE__);
}

static void
pcre2_free_wrap(void *o, void *arg)
{
}

void
n00b_regex_init(void)
{
    if (n00b_pcre2_global) {
        return;
    }
    n00b_gc_register_root(&n00b_pcre2_global, 1);
    n00b_gc_register_root(&n00b_pcre2_compile, 1);
    n00b_gc_register_root(&n00b_pcre2_match, 1);
    n00b_pcre2_global  = pcre2_general_context_create(pcre2_malloc_wrap,
                                                     pcre2_free_wrap,
                                                     NULL);
    n00b_pcre2_compile = pcre2_compile_context_create(n00b_pcre2_global);
    n00b_pcre2_match   = pcre2_match_context_create(n00b_pcre2_global);
}

static void
process_ovector(n00b_string_t *s, pcre2_match_data *md, n00b_list_t *l)
{
    n00b_match_t *m     = n00b_gc_alloc_mapped(n00b_match_t, N00B_GC_SCAN_ALL);
    uint32_t      n     = pcre2_get_ovector_count(md);
    PCRE2_SIZE   *oinfo = pcre2_get_ovector_pointer(md);
    uint32_t      pos   = 2;

    m->start = oinfo[0];
    m->end   = oinfo[1];

    n00b_list_append(l, m);

    if (n == 1) {
        return;
    }

    m->captures = n00b_list(n00b_type_string());

    for (uint32_t i = 1; i < n; i++) {
        int64_t        start   = oinfo[pos++];
        int64_t        len     = oinfo[pos++] - start;
        n00b_string_t *capture = n00b_utf8(s->data + start, len);

        n00b_list_append(m->captures, capture);
    }
}

n00b_list_t *
n00b_regex_raw_match(n00b_regex_t  *re,
                     n00b_string_t *s,
                     int            off,
                     bool           exact,
                     bool           no_eol,
                     bool           all)
{
    pcre2_match_data *md;
    int               err;
    uint32_t          opts    = 0;
    n00b_list_t      *matches = n00b_list(n00b_type_ref());
    uint32_t          opt2    = 0; // added by pcre2_next_match;

    if (exact) {
        opts |= PCRE2_ENDANCHORED;
    }
    if (no_eol) {
        opts |= PCRE2_NOTEOL;
    }

    // pcre2 checks to ensure the offset isn't off the end.
    md = pcre2_match_data_create_from_pattern(re->regex, n00b_pcre2_global);

    while (true) {
        err = pcre2_match(re->regex,
                          (PCRE2_SPTR8)s->data,
                          s->u8_bytes,
                          off,
                          opts | opt2,
                          md,
                          n00b_pcre2_match);

        if (err < 1) {
            if (err == PCRE2_ERROR_NOMATCH) {
                return matches;
            }

            PCRE2_UCHAR8 buf[N00B_PCRE2_ERR_LEN];
            pcre2_get_error_message(err, buf, N00B_PCRE2_ERR_LEN);
            N00B_RAISE(n00b_cstring(buf));
        }

        process_ovector(s, md, matches);

        if (!all) {
            if (!n00b_list_len(matches)) {
                return NULL;
            }
            return matches;
        }

        n00b_match_t *m = n00b_list_get(matches,
                                        n00b_list_len(matches) - 1,
                                        NULL);
        m->input_string = s;

        if (m->end == m->start) {
            // Handle empty string matches.
            off = m->end + 1;
        }
        else {
            if (m->end <= off) {
                return matches;
            }
        }

        off = m->end;
    }
}

void
regex_object_init(n00b_regex_t *regex, va_list args)
{
    // Can't use `keywords` block here because we don't
    // have an option to accomodate fixed parameters (yet).

    n00b_string_t *pattern     = va_arg(args, n00b_string_t *);
    bool           anchored    = true;
    bool           insensitive = false;
    bool           multiline   = false;

    n00b_karg_va_init(args);
    n00b_kw_bool("anchored", anchored);
    n00b_kw_bool("insensitive", insensitive);
    n00b_kw_bool("multiline", multiline);

    regex->repr = *pattern;

    int        err;
    PCRE2_SIZE eoffset;
    int        flags = PCRE2_ALLOW_EMPTY_CLASS | PCRE2_ALT_BSUX
              | PCRE2_NEVER_BACKSLASH_C | PCRE2_UCP | PCRE2_NO_UTF_CHECK;

    flags |= (PCRE2_ANCHORED * (int)anchored);
    flags |= (PCRE2_CASELESS * (int)insensitive);
    flags |= (PCRE2_MULTILINE * (int)multiline);

    regex->regex = pcre2_compile((PCRE2_SPTR8)pattern->data,
                                 pattern->u8_bytes,
                                 flags,
                                 &err,
                                 &eoffset,
                                 n00b_pcre2_compile);

    if (!regex->regex) {
        PCRE2_UCHAR8 buf[N00B_PCRE2_ERR_LEN];
        pcre2_get_error_message(err, buf, N00B_PCRE2_ERR_LEN);
        N00B_RAISE(n00b_cstring(buf));
    }
}

n00b_string_t *
n00b_regex_to_string(n00b_regex_t *regex)
{
    return n00b_utf8(regex->repr.data, regex->repr.u8_bytes);
}

const n00b_vtable_t n00b_regex_vtable = {
    .methods = {
        [N00B_BI_CONSTRUCTOR] = (n00b_vtable_entry)regex_object_init,
        [N00B_BI_TO_STRING]   = (n00b_vtable_entry)n00b_regex_to_string,
        [N00B_BI_HASH]        = (n00b_vtable_entry)n00b_string_hash,
    },
};
