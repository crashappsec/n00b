// As I started to think about avoiding unnecessary type instantiation
// (the 'type hash' dominates performance at the moment), it occured
// to me that I don't even *need* to hash types unless they're
// concrete.
//
// And if they are concrete, all that hashing can easily be done at
// compile time anyway.
//
//
// So this scheme basically no longer uses a pointer for types. It will
// be one of the following:
//
// 1. A type ID, for concrete, built-in types. This has the top bit set,
//    but otherwise is the same as the base type index.
// 2. A type hash, implying the type is concrete. This has both the
//    first and second most significant bits set.
// 3. A 'temporary type key'. This indicates that we are doing some sort
//    of compilation, and should go look up a n00b_type_node_t. The type
//    may or may not be concrete. These are used during compilation, and
//    do not survive the compilation process. This clears the top two bits.
// 4. A 'long-term type key'. This is also an index to go look up a
//    n00b_type_node_t. However, they are not modifiable, and will
//    maintain stable values, even with incremental compilation.
//    This sets the first significant bit to 0, and the second to 1.
//
//
// Also, an invalid type has all bits set.
//
// To accomplish this, we will end up managing 1-2 heaps at any given
// time that are dedicated to type information. Nothing outside of the
// type system will hold a direct reference to any of it, just an
// indirect index.
//
// Ideally, to avoid collisions, we'd have the type be 100+
// bits. However, we would prefer to stick w/ 64 bit values for the
// type, and we will steal two bits from it, per above. If we want to
// understand the type structurally (e.g., is it a list?) we will keep
// information
//
//
// To create the hash, we're going to first normalize the type by
// doing a 'repr' of it, then hash the repr using XXH. However, we
// will truncate the hash to represent information we want to know
// without having to look up more information about the type.

#include "n00b.h"

typedef union convert_t {
    struct {
        n00b_tid_t left;
        void      *right;
    } words;
    XXH128_hash_t xhv;
} convert_t;

typedef struct repr_ctx {
    n00b_dict_t *memos;
    int64_t      n;
    bool         concrete;
} repr_ctx;

static n00b_string_t *
gen_type_string(n00b_type_info_t *t, repr_ctx *ctx);

static inline n00b_string_t *
primitive_type_name(n00b_type_info_t *t)
{
    return n00b_cstring(n00b_base_type_info[t->base_index].name);
}

static const char tv_letters[] = "jtvwxyzabcdefghi";

static inline n00b_string_t *
next_typevar_name(repr_ctx *ctx)
{
    char buf[19] = {
        0,
    };
    int i   = 0;
    int num = ++ctx->n;

    while (num) {
        buf[i++] = tv_letters[num & 0x0f];
        num >>= 4;
    }

    return n00b_cstring(buf);
}

static inline n00b_string_t *
gen_ts_typevar(n00b_type_info_t *t, repr_ctx *ctx)
{
    n00b_string_t *s;

    if (t) {
        s = hatrack_dict_get(ctx->memos, t, NULL);
        if (s) {
            return s;
        }

        if (t->obj_info && t->obj_info->name) {
            s = n00b_cstring(t->obj_info->name);
        }
        else {
            s = next_typevar_name(ctx);
            hatrack_dict_put(ctx->memos, t, s);
        }
        return s;
    }

    return next_typevar_name(ctx);
}

static n00b_string_t *
gen_ts_container(n00b_type_info_t *t, repr_ctx *ctx)
{
    int            n = t->items ? n00b_list_len(t->items) : 0;
    int            i = 0;
    n00b_list_t   *l = n00b_list(n00b_type_string());
    n00b_string_t *sub;

    n00b_private_list_append(l, primitive_type_name(t));
    n00b_private_list_append(l, n00b_cached_lbracket());

    goto first_loop_start;

    for (; i < n; i++) {
        n00b_private_list_append(l, n00b_cached_comma());

first_loop_start:
        if (i >= n) {
            sub = gen_ts_typevar(NULL, ctx);
        }
        else {
            sub = gen_type_string(n00b_list_get(t->items, i, NULL), ctx);
        }

        n00b_private_list_append(l, sub);
    }

    n00b_private_list_append(l, n00b_cached_rbracket());

    return n00b_string_join(l, n00b_cached_empty_string());
}

static inline n00b_string_t *
gen_ts_box(n00b_type_info_t *t, repr_ctx *ctx)
{
    n00b_list_t *l = n00b_list(n00b_type_string());
    const char  *n = n00b_base_type_info[t->base_index].name;

    n00b_private_list_append(l, n00b_cstring("box["));
    n00b_private_list_append(l, n00b_cstring(n));
    n00b_private_list_append(l, n00b_cached_rbracket());

    return n00b_string_join(l, n00b_cached_empty_string());
}

static inline n00b_string_t *
gen_ts_object(n00b_type_info_t *t, repr_ctx *ctx)
{
    n00b_string_t *s;

    if (!t->items || !n00b_list_len(t->items)) {
        return t->obj_info->name;
    }

    n00b_list_t *l = n00b_list(n00b_type_string());
    n00b_list_append(l, t->obj_info->name);
    n00b_list_append(l, n00b_cached_lbracket());

    int n = n00b_list_len(t->items);
    int i = 0;

    goto after_comma;

    for (; i < n; i++) {
        n00b_list_append(l, n00b_cached_comma_padded());
after_comma:
        s = gen_type_string(n00b_list_get(t->items, i, NULL), ctx);
        n00b_list_append(l, s);
    }

    n00b_list_append(l, n00b_cached_rbracket());

    return n00b_string_join(l, n00b_cached_empty_string());
}

static inline n00b_string_t *
gen_ts_function(n00b_type_info_t *t, repr_ctx *ctx)
{
    int               num_types = n00b_list_len(t->items);
    n00b_list_t      *to_join   = n00b_list(n00b_type_string());
    int               i         = 0;
    n00b_type_info_t *subnode;
    n00b_string_t    *substr;

    n00b_list_append(to_join, n00b_cached_lparen());

    // num_types - 1 will be 0 if there are no args, but there is a
    // return value. So the below loop won't run in all cases.  But
    // only need to do the test for comma once, not every time through
    // the loop.

    if (num_types > 1) {
        goto after_comma;

        for (; i < num_types - 1; i++) {
            n00b_list_append(to_join, n00b_cached_comma());

after_comma:
            if ((i == num_types - 2) && t->flags & N00B_FN_TY_VARARGS) {
                n00b_list_append(to_join, n00b_cached_star());
            }

            subnode = n00b_list_get(t->items, i, NULL);
            substr  = gen_type_string(subnode, ctx);
            n00b_list_append(to_join, substr);
        }
    }

    n00b_list_append(to_join, n00b_cached_rparen());
    n00b_list_append(to_join, n00b_cached_arrow());

    subnode = n00b_list_get(t->items, num_types - 1, NULL);

    if (subnode) {
        substr = gen_type_string(subnode, ctx);
        n00b_list_append(to_join, substr);
    }

    return n00b_string_join(to_join, n00b_cached_empty_string());
}

static inline n00b_string_t *
gen_ts_oneof(n00b_type_info_t *t, repr_ctx *ctx)
{
    n00b_string_t *s;
    n00b_list_t   *l = n00b_list(n00b_type_string());
    n00b_list_append(l, n00b_cstring("oneof["));
    int n = n00b_list_len(t->items);
    int i = 0;

    goto after_comma;

    for (; i < n; i++) {
        n00b_list_append(l, n00b_cached_comma_padded());
after_comma:
        s = gen_type_string(n00b_list_get(t->items, i, NULL), ctx);
        n00b_list_append(l, s);
    }

    n00b_list_append(l, n00b_cached_rbracket());

    return n00b_string_join(l, n00b_cached_empty_string());
}

static n00b_string_t *
gen_type_string(n00b_type_info_t *t, repr_ctx *ctx)
{
    switch (n00b_base_type_info[t->base_index].dt_kind) {
    case N00B_DT_KIND_nil:
    case N00B_DT_KIND_primitive:
    case N00B_DT_KIND_internal:
        return primitive_type_name(t);
    case N00B_DT_KIND_type_var:
        ctx->concrete = false;
        return gen_ts_typevar(t, ctx);
    case N00B_DT_KIND_list:
    case N00B_DT_KIND_dict:
    case N00B_DT_KIND_tuple:
    case N00B_DT_KIND_maybe:
        return gen_ts_container(t, ctx);
    case N00B_DT_KIND_box:
        return gen_ts_box(t, ctx);
    case N00B_DT_KIND_object:
        return gen_ts_object(t, ctx);
    case N00B_DT_KIND_func:
        return gen_ts_function(t, ctx);
    case N00B_DT_KIND_oneof:
        ctx->concrete = false;
        return gen_ts_oneof(t, ctx);
    default:
        n00b_unreachable();
    }
}
static inline n00b_string_t *
n00b_type_node_to_string(n00b_type_info_t *t, bool *concrete)
{
    struct repr_ctx ctx = {
        .memos    = n00b_dict(n00b_type_ref(),
                           n00b_type_string()),
        .n        = 0,
        .concrete = true, // Will be set to false when invalidated.
    };

    n00b_string_t *result = gen_type_string(t, &ctx);

    if (concrete) {
        *concrete = ctx.concrete;
    }

    return result;
}

n00b_tid_t
n00b_type_node_hash(n00b_type_info_t *t)
{
    switch (n00b_base_type_info[t->base_index].dt_kind) {
    case N00B_DT_KIND_nil:
        return ~0ULL;
    case N00B_DT_KIND_primitive:
    case N00B_DT_KIND_internal:
        return (1ULL << 63) | t->base_index;
    default:
        break;
    }

    bool           concrete;
    n00b_string_t *rep = n00b_type_node_to_string(t, &concrete);

    if (!concrete) {
        return ~0ULL;
    }

    convert_t hash = {.xhv = XXH3_128bits(rep->data, rep->u8_bytes)};

    return hash.words.left | (0x03ULL << 62);
}
