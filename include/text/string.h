#pragma once
#include "n00b.h"

#define N00B_string_HASH_KEY_POINTER_OFFSET 0
#define N00B_HASH_CACHE_OBJ_OFFSET          (-2 * (int32_t)sizeof(uint64_t))
#define N00B_HASH_CACHE_RAW_OFFSET          (-2 * (int32_t)sizeof(uint64_t))

extern const char n00b_hex_map_lower[16];
extern const char n00b_hex_map_upper[16];

typedef struct {
    void   *info; // a pointer to a n00b_text_element_t object.
    int32_t start;
    int32_t end;
} n00b_style_record_t;

// Right now, we're not really using the second.
enum {
    N00B_LB_MUSTBREAK  = 0,
    N00B_LB_ALLOWBREAK = 1,
    N00B_LB_NOBREAK    = 2
};

typedef struct n00b_string_style_info_t {
    int64_t             num_styles;
    // Used when rendering for unfilled bits, and as the inherited
    // defaults for anything else.
    void               *base_style;
    n00b_style_record_t styles[];
} n00b_string_style_info_t;

struct n00b_string_t {
    char                     *data;
    n00b_codepoint_t         *u32_data;
    n00b_string_style_info_t *styling;
    int32_t                   codepoints;
    int                       u8_bytes;
};

typedef n00b_string_t *(*n00b_string_convertor_t)(n00b_obj_t);

extern bool              n00b_valid_codepoint(n00b_codepoint_t);
extern n00b_codepoint_t *n00b_string_to_codepoint_array(n00b_string_t *);
extern n00b_string_t    *n00b_string_slice(n00b_string_t *, int64_t, int64_t);
extern n00b_codepoint_t  n00b_string_index(n00b_string_t *, int64_t);
extern n00b_string_t    *n00b_string_to_hex(n00b_string_t *, bool);
extern n00b_string_t    *_n00b_string_strip(n00b_string_t *s, ...);
extern n00b_string_t    *n00b_string_concat(n00b_string_t *, n00b_string_t *);
extern n00b_string_t    *_n00b_string_join(n00b_list_t *, n00b_string_t *, ...);
extern n00b_string_t    *n00b_string_from_int(int64_t);
extern n00b_string_t    *n00b_string_repeat(n00b_codepoint_t, int64_t);
extern int64_t           n00b_string_render_len(n00b_string_t *);
extern n00b_string_t    *n00b_string_truncate(n00b_string_t *, int64_t);
extern bool              n00b_string_starts_with(n00b_string_t *,
                                                 n00b_string_t *);
extern bool              n00b_string_ends_with(n00b_string_t *,
                                               n00b_string_t *);
extern bool              n00b_string_ends_with(n00b_string_t *,
                                               n00b_string_t *);
extern bool              n00b_string_from_file(n00b_string_t *, int *);
extern int64_t           _n00b_string_find(n00b_string_t *,
                                           n00b_string_t *,
                                           ...);
extern int64_t           _n00b_string_rfind(n00b_string_t *,
                                            n00b_string_t *,
                                            ...);
extern n00b_list_t      *n00b_string_split(n00b_string_t *, n00b_string_t *);
extern n00b_list_t      *_n00b_string_split_words(n00b_string_t *, ...);
extern n00b_list_t      *n00b_string_split_and_crop(n00b_string_t *,
                                                    n00b_string_t *,
                                                    int64_t);
extern bool              n00b_string_eq(n00b_string_t *, n00b_string_t *);
extern n00b_list_t      *n00b_string_wrap(n00b_string_t *, int64_t, int64_t);
extern n00b_string_t    *n00b_string_upper(n00b_string_t *);
extern n00b_string_t    *n00b_string_lower(n00b_string_t *);
extern n00b_string_t    *n00b_string_all_caps(n00b_string_t *);
extern n00b_string_t    *n00b_string_pad(n00b_string_t *, int64_t);
extern n00b_string_t    *n00b_string_copy(n00b_string_t *s);
extern n00b_string_t    *n00b_string_reuse_text(n00b_string_t *s);
extern char             *n00b_string_to_cstr(n00b_string_t *);
extern char            **n00b_make_cstr_array(n00b_list_t *, int *);
extern n00b_string_t    *n00b_string_align_left(n00b_string_t *, int);
extern n00b_string_t    *n00b_string_align_right(n00b_string_t *, int);
extern n00b_string_t    *n00b_string_align_center(n00b_string_t *, int);
extern n00b_string_t    *n00b_to_string(void *item);
extern n00b_string_t    *n00b_to_literal(void *item);
extern n00b_codepoint_t  n00b_codepoint_title_case(n00b_codepoint_t, bool *);
extern n00b_string_t    *n00b_string_to_hex(n00b_string_t *, bool);
extern n00b_string_t    *n00b_cstring_copy(char *);

#define n00b_string_strip(s, ...) \
    _n00b_string_strip(s, N00B_VA(__VA_ARGS__))
#define n00b_string_join(l, s, ...) \
    _n00b_string_join(l, s, N00B_VA(__VA_ARGS__))
#define n00b_string_find(str, sub, ...) \
    _n00b_string_find(str, sub, N00B_VA(__VA_ARGS__))
#define n00b_string_rfind(a, b, ...) \
    _n00b_string_rfind(a, b, N00B_VA(__VA_ARGS__))
#define n00b_string_split_words(str, ...) \
    _n00b_string_split_words(str, N00B_VA(__VA_ARGS__))

static inline n00b_codepoint_t
n00b_codepoint_upper_case(n00b_codepoint_t cp)
{
    return utf8proc_toupper(cp);
}

static inline n00b_codepoint_t
n00b_codepoint_lower_case(n00b_codepoint_t cp)
{
    return utf8proc_tolower(cp);
}

static inline n00b_string_t *
n00b_string_empty(void)
{
    return n00b_new(n00b_type_string(), NULL, false, 0);
}

static inline n00b_string_t *
_n00b_utf8(char *s, int64_t len)
{
    return n00b_new(n00b_type_string(), s, true, len);
}

static inline n00b_string_t *
_n00b_cstring(char *s)
{
    return n00b_new(n00b_type_string(), s, true, 0);
}

#define n00b_cstring(x) _n00b_cstring((char *)x)
#define n00b_utf8(s, l) _n00b_utf8((char *)(s), (l))

static inline n00b_string_t *
n00b_string_from_codepoint(n00b_codepoint_t cp)
{
    return n00b_string_repeat(cp, 1);
}

static inline n00b_string_t *
n00b_utf32(n00b_codepoint_t *s, int64_t num_cp)
{
    return n00b_new(n00b_type_string(), s, false, num_cp);
}

static inline bool
n00b_string_is_styled(n00b_string_t *s)
{
    return (s && s->styling) ? s->styling->num_styles != 0 : false;
}

static inline int64_t
n00b_string_codepoint_len(n00b_string_t *s)
{
    return s ? s->codepoints : 0;
}

static inline int64_t
n00b_string_utf8_len(n00b_string_t *s)
{
    return s ? s->u8_bytes : 0;
}

static inline n00b_string_t *
n00b_string_replace(n00b_string_t *base,
                    n00b_string_t *sub_old,
                    n00b_string_t *sub_new)
{
    return n00b_string_join(n00b_string_split(base, sub_old), sub_new);
}

static inline void
n00b_string_require_u32(n00b_string_t *s)
{
    if (!s->u32_data) {
        s->u32_data = n00b_string_to_codepoint_array(s);
    }
}

enum {
    N00B_STRCACHE_EMPTY_STR,
    N00B_STRCACHE_NEWLINE,
    N00B_STRCACHE_CR,
    N00B_STRCACHE_COMMA,
    N00B_STRCACHE_LBRACKET,
    N00B_STRCACHE_RBRACKET,
    N00B_STRCACHE_LBRACE,
    N00B_STRCACHE_RBRACE,
    N00B_STRCACHE_LPAREN,
    N00B_STRCACHE_RPAREN,
    N00B_STRCACHE_BACKTICK,
    N00B_STRCACHE_1QUOTE,
    N00B_STRCACHE_2QUOTE,
    N00B_STRCACHE_STAR,
    N00B_STRCACHE_SPACE,
    N00B_STRCACHE_SLASH,
    N00B_STRCACHE_BACKSLASH,
    N00B_STRCACHE_PERIOD,
    N00B_STRCACHE_COLON,
    N00B_STRCACHE_SEMICOLON,
    N00B_STRCACHE_BANG,
    N00B_STRCACHE_AT,
    N00B_STRCACHE_HASH,
    N00B_STRCACHE_DOLLAR,
    N00B_STRCACHE_PERCENT,
    N00B_STRCACHE_CARAT,
    N00B_STRCACHE_AMPERSAND,
    N00B_STRCACHE_MINUS,
    N00B_STRCACHE_UNDERSCORE,
    N00B_STRCACHE_PLUS,
    N00B_STRCACHE_EQUALS,
    N00B_STRCACHE_PIPE,
    N00B_STRCACHE_GT,
    N00B_STRCACHE_LT,
    N00B_STRCACHE_QUESTION_MARK,
    N00B_STRCACHE_TILDE,
    N00B_STRCACHE_ARROW,
    N00B_STRCACHE_COMMA_PADDED,
    N00B_STRCACHE_COLON_PADDED,
    N00B_STRCACHE_CRLF,
    N00B_STRCACHE_ELLIPSIS,
    N00B_STRCACHE_BUILTIN_MAX,
};

extern n00b_string_t *n00b_common_string_cache[N00B_STRCACHE_BUILTIN_MAX];

static inline n00b_string_t *
n00b_cached_empty_string(void)
{
    return n00b_common_string_cache[N00B_STRCACHE_EMPTY_STR];
}

static inline n00b_string_t *
n00b_cached_newline(void)
{
    return n00b_common_string_cache[N00B_STRCACHE_NEWLINE];
}

static inline n00b_string_t *
n00b_cached_cr(void)
{
    return n00b_common_string_cache[N00B_STRCACHE_CR];
}

static inline n00b_string_t *
n00b_cached_comma(void)
{
    return n00b_common_string_cache[N00B_STRCACHE_COMMA];
}

static inline n00b_string_t *
n00b_cached_lbracket(void)
{
    return n00b_common_string_cache[N00B_STRCACHE_LBRACKET];
}

static inline n00b_string_t *
n00b_cached_rbracket(void)
{
    return n00b_common_string_cache[N00B_STRCACHE_RBRACKET];
}

static inline n00b_string_t *
n00b_cached_lbrace(void)
{
    return n00b_common_string_cache[N00B_STRCACHE_LBRACE];
}

static inline n00b_string_t *
n00b_cached_rbrace(void)
{
    return n00b_common_string_cache[N00B_STRCACHE_RBRACE];
}

static inline n00b_string_t *
n00b_cached_lparen(void)
{
    return n00b_common_string_cache[N00B_STRCACHE_LPAREN];
}

static inline n00b_string_t *
n00b_cached_rparen(void)
{
    return n00b_common_string_cache[N00B_STRCACHE_RPAREN];
}

static inline n00b_string_t *
n00b_cached_backtick(void)
{
    return n00b_common_string_cache[N00B_STRCACHE_BACKTICK];
}

static inline n00b_string_t *
n00b_cached_1quote(void)
{
    return n00b_common_string_cache[N00B_STRCACHE_1QUOTE];
}

static inline n00b_string_t *
n00b_cached_2quote(void)
{
    return n00b_common_string_cache[N00B_STRCACHE_2QUOTE];
}

static inline n00b_string_t *
n00b_cached_star(void)
{
    return n00b_common_string_cache[N00B_STRCACHE_STAR];
}

static inline n00b_string_t *
n00b_cached_space(void)
{
    return n00b_common_string_cache[N00B_STRCACHE_SPACE];
}

static inline n00b_string_t *
n00b_cached_slash(void)
{
    return n00b_common_string_cache[N00B_STRCACHE_SLASH];
}

static inline n00b_string_t *
n00b_cached_backslash(void)
{
    return n00b_common_string_cache[N00B_STRCACHE_BACKSLASH];
}

static inline n00b_string_t *
n00b_cached_period(void)
{
    return n00b_common_string_cache[N00B_STRCACHE_PERIOD];
}

static inline n00b_string_t *
n00b_cached_colon(void)
{
    return n00b_common_string_cache[N00B_STRCACHE_COLON];
}

static inline n00b_string_t *
n00b_cached_semicolon(void)
{
    return n00b_common_string_cache[N00B_STRCACHE_SEMICOLON];
}

static inline n00b_string_t *
n00b_cached_bang(void)
{
    return n00b_common_string_cache[N00B_STRCACHE_BANG];
}

static inline n00b_string_t *
n00b_cached_at(void)
{
    return n00b_common_string_cache[N00B_STRCACHE_AT];
}

static inline n00b_string_t *
n00b_cached_hash(void)
{
    return n00b_common_string_cache[N00B_STRCACHE_HASH];
}

static inline n00b_string_t *
n00b_cached_dollar(void)
{
    return n00b_common_string_cache[N00B_STRCACHE_DOLLAR];
}

static inline n00b_string_t *
n00b_cached_percent(void)
{
    return n00b_common_string_cache[N00B_STRCACHE_PERCENT];
}

static inline n00b_string_t *
n00b_cached_carat(void)
{
    return n00b_common_string_cache[N00B_STRCACHE_CARAT];
}

static inline n00b_string_t *
n00b_cached_ampersand(void)
{
    return n00b_common_string_cache[N00B_STRCACHE_AMPERSAND];
}

static inline n00b_string_t *
n00b_cached_minus(void)
{
    return n00b_common_string_cache[N00B_STRCACHE_MINUS];
}

static inline n00b_string_t *
n00b_cached_underscore(void)
{
    return n00b_common_string_cache[N00B_STRCACHE_UNDERSCORE];
}

static inline n00b_string_t *
n00b_cached_plus(void)
{
    return n00b_common_string_cache[N00B_STRCACHE_PLUS];
}

static inline n00b_string_t *
n00b_cached_equals(void)
{
    return n00b_common_string_cache[N00B_STRCACHE_EQUALS];
}

static inline n00b_string_t *
n00b_cached_pipe(void)
{
    return n00b_common_string_cache[N00B_STRCACHE_PIPE];
}

static inline n00b_string_t *
n00b_cached_gt(void)
{
    return n00b_common_string_cache[N00B_STRCACHE_GT];
}

static inline n00b_string_t *
n00b_cached_lt(void)
{
    return n00b_common_string_cache[N00B_STRCACHE_LT];
}

static inline n00b_string_t *
n00b_cached_question(void)
{
    return n00b_common_string_cache[N00B_STRCACHE_QUESTION_MARK];
}

static inline n00b_string_t *
n00b_cached_tilde(void)
{
    return n00b_common_string_cache[N00B_STRCACHE_TILDE];
}

static inline n00b_string_t *
n00b_cached_arrow(void)
{
    return n00b_common_string_cache[N00B_STRCACHE_ARROW];
}

static inline n00b_string_t *
n00b_cached_comma_padded(void)
{
    return n00b_common_string_cache[N00B_STRCACHE_COMMA_PADDED];
}

static inline n00b_string_t *
n00b_cached_colon_padded(void)
{
    return n00b_common_string_cache[N00B_STRCACHE_COLON_PADDED];
}

static inline n00b_string_t *
n00b_cached_crlf(void)
{
    return n00b_common_string_cache[N00B_STRCACHE_CRLF];
}

static inline n00b_string_t *
n00b_cached_ellipsis(void)
{
    return n00b_common_string_cache[N00B_STRCACHE_ELLIPSIS];
}

static inline int
n00b_string_byte_len(n00b_string_t *s)
{
    return s->u8_bytes;
}

#if defined(N00B_USE_INTERNAL_API)
extern hatrack_hash_t n00b_custom_string_hash(void *);
extern void           n00b_init_common_string_cache(void);
extern void           n00b_string_sanity_check(n00b_string_t *);
extern int            n00b_string_set_codepoint_count(n00b_string_t *);
extern n00b_string_t *n00b_to_string_provided_type(void *, n00b_type_t *);
extern n00b_string_t *n00b_to_literal_provided_type(void *, n00b_type_t *);

// This is meant for internal use.

static inline n00b_string_t *
n00b_string_copy_text(n00b_string_t *s)
{
    // Copies the string data for internal operations that are going to
    // mutate a string.
    //
    // This does NOT copy styles.
    return n00b_new(n00b_type_string(), s->data, true, s->u8_bytes);
}

static inline n00b_string_t *
n00b_alloc_utf8_to_copy(int64_t len)
{
    // Allocs the required number of raw bytes intended to hold utf8
    // characters.  The caller is responsible for calling
    // n00b_string_set_codepoint_count();
    return n00b_new(n00b_type_string(), NULL, true, len);
}
#endif
