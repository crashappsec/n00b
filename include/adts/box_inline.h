#pragma once

#include "n00b.h"

static inline void *
n00b_box_obj(int64_t value, n00b_ntype_t type)
{
    n00b_ntype_t box_type = n00b_type_box(type);
    if (!box_type) {
        return NULL;
    }

    int64_t *result = n00b_gc_alloc_mapped(int64_t *, N00B_GC_SCAN_NONE);

    *result = value;

    n00b_alloc_hdr *hdr = (void *)result;
    hdr[-1].type        = box_type;

    return result;
}

// Safely dereference a boxed item, thus removing the box.
// Since we're internally reserving 64 bits for values, we
// return it as a 64 bit item.
//
// However, the allocated item allocated the actual item's size, so we
// have to make sure to get it right on both ends; we can't just
// dereference a uint64_t, for instance.

static inline int64_t
n00b_unbox(n00b_box_t box)
{
    n00b_ntype_t t = n00b_get_my_type(box);

    if (!n00b_type_is_box(t)) {
        return (int64_t)box;
    }

    return *(int64_t *)box;
}

static inline int64_t
n00b_resolve_and_unbox(void *obj)
{
    n00b_ntype_t t = n00b_get_my_type(obj);

    if (!n00b_type_is_box(t)) {
        return (int64_t)obj;
    }

    return n00b_unbox(obj);
}

static inline bool *
n00b_box_bool(bool val)
{
    return n00b_box_obj(val, n00b_type_bool());
}

static inline int8_t *
n00b_box_i8(int8_t val)
{
    return n00b_box_obj(val, n00b_type_i8());
}

static inline uint8_t *
n00b_box_u8(uint8_t val)
{
    return n00b_box_obj(val, n00b_type_u8());
}

static inline int32_t *
n00b_box_i32(int32_t val)
{
    return n00b_box_obj(val, n00b_type_i32());
}

static inline uint32_t *
n00b_box_u32(uint32_t val)
{
    return n00b_box_obj(val, n00b_type_u32());
}

static inline int64_t *
n00b_box_i64(int64_t val)
{
    return n00b_box_obj(val, n00b_type_i64());
}

static inline uint64_t *
n00b_box_u64(uint64_t val)
{
    return n00b_box_obj(val, n00b_type_u64());
}

static inline double *
n00b_box_double(double val)
{
    union {
        void  *v;
        double d;
    } u = {.d = val};

    return n00b_box_obj(u.d, n00b_type_f64());
}
