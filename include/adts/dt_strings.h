#pragma once
#include "n00b.h"

/**
 ** For UTF-32, we actually store in the codepoints field the bitwise
 ** NOT to distinguish whether strings are UTF-8 (the high bit will
 ** always be 0 with UTF-8).
 **/

#define N00B_STR_HASH_KEY_POINTER_OFFSET 0
#define N00B_HASH_CACHE_OBJ_OFFSET       (-2 * (int32_t)sizeof(uint64_t))
#define N00B_HASH_CACHE_RAW_OFFSET       (-2 * (int32_t)sizeof(uint64_t))

typedef struct n00b_str_t {
    char             *data;
    n00b_style_info_t *styling;
    int64_t           byte_len;
    int64_t           codepoints : 32;
    unsigned int      utf32      : 1;
} n00b_str_t;

typedef n00b_str_t n00b_utf8_t;
typedef n00b_str_t n00b_utf32_t;

typedef struct break_info_st {
    int32_t num_slots;
    int32_t num_breaks;
    int32_t breaks[];
} n00b_break_info_t;

// Only used for the static macro.
struct n00b_internal_string_st {
    n00b_dt_info_t     *base_data_type;
    struct n00b_type_t *concrete_type;
    uint64_t           hash_cache_1;
    uint64_t           hash_cache_2;
    n00b_str_t          s;
};

// This struct is used to manage state when rending ansi.
typedef enum {
    N00B_U8_STATE_START_DEFAULT,
    N00B_U8_STATE_START_STYLE,
    N00B_U8_STATE_DEFAULT_STYLE, // Stop at a new start ix or at the end.
    N00B_U8_STATE_IN_STYLE       // Stop at a new end ix or at the end.
} n00b_u8_state_t;

// This only works with ASCII strings, not arbitrary utf8.
#define N00B_STATIC_ASCII_STR(id, val)                                       \
    const struct n00b_internal_string_st _static_##id = {                    \
        .base_data_type = (n00b_dt_info_t *)&n00b_base_type_info[N00B_T_UTF8], \
        .concrete_type  = (struct n00b_type_t *)&n00b_bi_types[N00B_T_UTF8],   \
        .s.byte_len     = sizeof(val) - 1,                                  \
        .s.codepoints   = sizeof(val) - 1,                                  \
        .s.styling      = NULL,                                             \
        .s.data         = val,                                              \
    };                                                                      \
    const n00b_str_t *id = (n00b_str_t *)&(_static_##id.s)
