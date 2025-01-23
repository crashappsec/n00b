#pragma once

#include "n00b.h"
extern const char n00b_hex_map_lower[16];
extern const char n00b_hex_map_upper[16];

extern n00b_str_t         *n00b_str_copy(const n00b_str_t *s);
extern n00b_utf32_t       *n00b_str_concat(const n00b_str_t *, const n00b_str_t *);
extern n00b_utf8_t        *n00b_to_utf8(const n00b_utf32_t *);
extern n00b_utf32_t       *n00b_to_utf32(const n00b_utf8_t *);
extern n00b_utf8_t        *n00b_from_file(const n00b_str_t *, int *);
extern int64_t             n00b_utf8_validate(const n00b_utf8_t *);
extern n00b_utf32_t       *n00b_str_slice(const n00b_str_t *, int64_t, int64_t);
extern n00b_utf8_t        *n00b_utf8_repeat(n00b_codepoint_t, int64_t);
extern n00b_utf32_t       *n00b_utf32_repeat(n00b_codepoint_t, int64_t);
extern n00b_utf32_t       *_n00b_str_strip(const n00b_str_t *s, ...);
extern n00b_str_t         *_n00b_str_truncate(const n00b_str_t *s, int64_t, ...);
extern n00b_str_t         *_n00b_str_join(n00b_list_t *,
                                          const n00b_str_t *,
                                          ...);
extern n00b_utf8_t        *n00b_str_from_int(int64_t n);
extern int64_t             _n00b_str_find(n00b_str_t *, n00b_str_t *, ...);
extern int64_t             _n00b_str_rfind(n00b_str_t *, n00b_str_t *, ...);
extern n00b_utf8_t        *n00b_cstring(char *s, int64_t len);
extern n00b_utf8_t        *n00b_rich(n00b_utf8_t *, n00b_utf8_t *style);
extern n00b_codepoint_t    n00b_index(const n00b_str_t *, int64_t);
extern bool                n00b_str_can_coerce_to(n00b_type_t *, n00b_type_t *);
extern n00b_obj_t          n00b_str_coerce_to(const n00b_str_t *, n00b_type_t *);
extern n00b_list_t        *n00b_str_split(n00b_str_t *, n00b_str_t *);
extern struct flexarray_t *n00b_str_fsplit(n00b_str_t *, n00b_str_t *);
extern bool                n00b_str_starts_with(const n00b_str_t *,
                                                const n00b_str_t *);
extern bool                n00b_str_ends_with(const n00b_str_t *,
                                              const n00b_str_t *);
extern n00b_list_t        *n00b_str_wrap(const n00b_str_t *, int64_t, int64_t);
extern n00b_utf32_t       *n00b_str_upper(n00b_str_t *);
extern n00b_utf32_t       *n00b_str_lower(n00b_str_t *);
extern n00b_utf32_t       *n00b_title_case(n00b_str_t *);
extern n00b_str_t         *n00b_str_pad(n00b_str_t *, int64_t);
extern n00b_utf8_t        *n00b_str_to_hex(n00b_str_t *, bool);
extern n00b_list_t        *_n00b_c_map(char *, ...);

// This is in richlit.c
extern n00b_utf8_t *n00b_rich_lit(char *);

#define n00b_str_strip(s, ...)       _n00b_str_strip(s, N00B_VA(__VA_ARGS__))
#define n00b_str_truncate(s, n, ...) _n00b_str_truncate(s, n, N00B_VA(__VA_ARGS__))
#define n00b_str_join(l, s, ...)     _n00b_str_join(l, s, N00B_VA(__VA_ARGS__))
#define n00b_str_find(str, sub, ...) _n00b_str_find(str, sub, N00B_VA(__VA_ARGS__))
#define n00b_str_rfind(a, b, ...)    _n00b_str_rfind(a, b, N00B_VA(__VA_ARGS__))
#define n00b_c_map(s, ...)           _n00b_c_map(s, N00B_VA(__VA_ARGS__))

extern const n00b_utf8_t *n00b_empty_string_const;
extern const n00b_utf8_t *n00b_newline_const;
extern const n00b_utf8_t *n00b_crlf_const;

static inline bool
n00b_str_is_u32(const n00b_str_t *s)
{
    return s ? (bool)(s->utf32) : false;
}

static inline bool
n00b_str_is_u8(const n00b_str_t *s)
{
    return s ? !s->utf32 : false;
}

static inline bool
n00b_str_is_styled(const n00b_str_t *s)
{
    return s ? s->styling && s->styling->num_entries : false;
}

static inline int64_t
n00b_str_codepoint_len(const n00b_str_t *s)
{
    return s ? s->codepoints : 0;
}

static inline int64_t
n00b_str_byte_len(const n00b_str_t *s)
{
    return s ? s->byte_len : 0;
}

extern int64_t n00b_str_render_len(const n00b_str_t *);

static inline n00b_utf8_t *
n00b_empty_string()
{
    n00b_utf8_t *s = n00b_str_copy(n00b_empty_string_const);

    s->codepoints = 0;

    return s;
}

static inline n00b_utf8_t *
n00b_str_newline()
{
    return n00b_str_copy(n00b_newline_const);
}

static inline n00b_utf8_t *
n00b_str_crlf()
{
    return n00b_str_copy(n00b_crlf_const);
}

static inline n00b_str_t *
n00b_str_replace(n00b_str_t *base, n00b_str_t *sub_old, n00b_str_t *sub_new)
{
    return n00b_str_join(n00b_str_split(base, sub_old), sub_new);
}

#define n00b_new_utf8(to_copy) \
    n00b_new(n00b_type_utf8(), n00b_kw("cstring", n00b_ka(to_copy)))

static inline char *
n00b_to_cstring(n00b_str_t *s)
{
    return s->data;
}

static inline int
n00b_str_index_width(const n00b_str_t *s)
{
    if (!s || !n00b_str_codepoint_len(s)) {
        return 0;
    }
    if (n00b_str_is_u32(s)) {
        return 4;
    }

    int n = n00b_str_byte_len(s);
    for (int i = 0; i < n; i++) {
        if (s->data[i] & 0x80) {
            return 4;
        }
    }
    return 1;
}

extern n00b_list_t *n00b_u8_map(n00b_list_t *);
extern bool         n00b_str_eq(n00b_str_t *, n00b_str_t *);
extern char        *n00b_to_cstr(void *);
extern char       **n00b_make_cstr_array(n00b_list_t *, int *);

extern const uint64_t n00b_pmap_str[2];

#ifdef N00B_USE_INTERNAL_API
extern void n00b_internal_utf8_set_codepoint_count(n00b_utf8_t *);
#endif

typedef enum {
    LB_MUSTBREAK  = 0,
    LB_ALLOWBREAK = 1,
    LB_NOBREAK    = 2
} lbreak_kind_t;
