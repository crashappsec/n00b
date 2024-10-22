#pragma once
#include "n00b.h"

typedef enum {
    ST_Base10 = 0,
    ST_Hex    = 1,
    ST_Float  = 2,
    ST_Bool   = 3,
    ST_2Quote = 4,
    ST_1Quote = 5,
    ST_List   = 6,
    ST_Dict   = 7,
    ST_Tuple  = 8,
    ST_MAX    = 9
} n00b_lit_syntax_t;

typedef struct {
    struct n00b_str_t  *litmod;
    struct n00b_type_t *cast_to;
    struct n00b_type_t *type;
    n00b_builtin_t      base_type; // only set for containers from here down.
    n00b_lit_syntax_t   st;
    int                num_items;
} n00b_lit_info_t;
