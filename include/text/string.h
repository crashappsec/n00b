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

typedef n00b_string_t *(*n00b_string_convertor_t)(void *);

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
extern n00b_string_t    *n00b_string_unescape(n00b_string_t *, int *);
extern n00b_string_t    *n00b_string_escape(n00b_string_t *);

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

#if defined(N00B_USE_INTERNAL_API)
extern void           n00b_init_common_string_cache(void);
extern void           n00b_string_sanity_check(n00b_string_t *);
extern int            n00b_string_set_codepoint_count(n00b_string_t *);
extern n00b_string_t *n00b_to_string_provided_type(void *, n00b_ntype_t);
extern n00b_string_t *n00b_to_literal_provided_type(void *, n00b_ntype_t);
#endif
