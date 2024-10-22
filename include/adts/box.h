#pragma once

#include "n00b.h"

static inline n00b_obj_t
n00b_box_obj(n00b_box_t value, n00b_type_t *type)
{
    return n00b_new(n00b_type_box(type), value);
}

// Safely dereference a boxed item, thus removing the box.
// Since we're internally reserving 64 bits for values, we
// return it as a 64 bit item.
//
// However, the allocated item allocated the actual item's size, so we
// have to make sure to get it right on both ends; we can't just
// dereference a uint64_t, for instance.

static inline n00b_box_t
n00b_unbox_obj(n00b_box_t *box)
{
    n00b_box_t result = {
        .u64 = 0,
    };

    n00b_type_t *t = n00b_type_unbox(n00b_get_my_type(box));

    switch (n00b_get_alloc_len(t)) {
    case 1:
        // On my mac, when this gets compiled w/ ASAN, ASAN somehow
        // mangles the bool even when properly going through the union
        // here.
        //
        // So this shouldn't be necessary, yet here it is.
        if (t->base_index == N00B_T_BOOL) {
            result.u64 = !!box->u64;
        }
        else {
            result.u8 = box->u8;
        }
        break;
    case 2:
        result.u16 = box->u16;
        break;
    case 4:
        result.u32 = box->u32;
        break;
    default:
        result.u64 = box->u64;
        break;
    }

    return result;
}

// This just drops the union, which is not needed after the above.
static inline uint64_t
n00b_unbox(n00b_obj_t value)
{
    return n00b_unbox_obj(value).u64;
}

static inline bool *
n00b_box_bool(bool val)
{
    n00b_box_t v = {
        .b = val,
    };
    return n00b_box_obj(v, n00b_type_bool());
}

static inline int8_t *
n00b_box_i8(int8_t val)
{
    n00b_box_t v = {
        .i8 = val,
    };
    return n00b_box_obj(v, n00b_type_i8());
}

static inline uint8_t *
n00b_box_u8(uint8_t val)
{
    n00b_box_t v = {
        .u8 = val,
    };
    return n00b_box_obj(v, n00b_type_u8());
}

static inline int32_t *
n00b_box_i32(int32_t val)
{
    n00b_box_t v = {
        .i32 = val,
    };
    return n00b_box_obj(v, n00b_type_i32());
}

static inline uint32_t *
n00b_box_u32(uint32_t val)
{
    n00b_box_t v = {
        .u32 = val,
    };
    return n00b_box_obj(v, n00b_type_u32());
}

static inline int64_t *
n00b_box_i64(int64_t val)
{
    n00b_box_t v = {
        .i64 = val,
    };
    return n00b_box_obj(v, n00b_type_i64());
}

static inline uint64_t *
n00b_box_u64(uint64_t val)
{
    n00b_box_t v = {
        .u64 = val,
    };
    return n00b_box_obj(v, n00b_type_u64());
}

static inline double *
n00b_box_double(double val)
{
    n00b_box_t v = {
        .dbl = val,
    };

    n00b_box_t *res = n00b_box_obj(v, n00b_type_f64());

    return (double *)res;
}

// in numbers.c
extern bool n00b_parse_int64(n00b_utf8_t *, int64_t *);
extern bool n00b_parse_double(n00b_utf8_t *, double *);
