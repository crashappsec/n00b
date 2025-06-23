#include "n00b.h"

extern n00b_ntype_t n00b_get_my_type(void *);

static inline void
n00b_set_type(void *addr, n00b_ntype_t t)
{
    n00b_assert(n00b_in_heap(addr));

    n00b_alloc_hdr *h = (void *)addr;

    assert(h[-1].guard == n00b_gc_guard);

    h[-1].type = t;
}

static inline void
n00b_basic_memory_info(void *addr, bool *is_obj, bool *interior)
{
    n00b_assert(n00b_in_heap(addr));

    n00b_alloc_hdr *h = n00b_find_allocation_record(addr);

    if (interior) {
        *interior = (h->data != addr);
    }

    if (is_obj) {
        *is_obj = (h->type != n00b_type_error());
    }
}

static inline bool
n00b_is_object_reference(void *addr)
{
    bool is_obj   = false;
    bool interior = false;

    n00b_basic_memory_info(addr, &is_obj, &interior);

    return is_obj && !interior;
}

#ifdef N00B_DEBUG

// Macro to preserve __FILE__ and __LINE__ for assert.
#define n00b_object_sanity_check(addr)                        \
    {                                                         \
        n00b_assert(n00b_is_object_reference(addr));          \
        n00b_ntype_t t = n00b_get_my_type(addr);              \
                                                              \
        n00b_assert(t);                                       \
        n00b_assert(t->base_index > N00B_T_ERROR              \
                    && t->base_index < N00B_NUM_BUILTIN_DTS); \
    }

#else
#define n00b_object_sanity_check(x)
#endif

static inline n00b_ntype_t
n00b_base_type(void *user_object)
{
    uint64_t n = n00b_get_my_type(user_object);

    n = _n00b_get_tnode(n, NULL)->info.base_typeid;

    return n;
}

static inline n00b_dt_info_t *
n00b_get_data_type_info(n00b_ntype_t t)
{
    t = _n00b_get_tnode(t, NULL)->info.base_typeid;
    return (n00b_dt_info_t *)&n00b_base_type_info[t];
}

static inline n00b_dt_info_t *
n00b_object_type_info(void *o)
{
    return (n00b_dt_info_t *)&n00b_base_type_info[n00b_base_type(o)];
}

static inline n00b_vtable_t *
n00b_vtable(void *user_object)
{
    return (n00b_vtable_t *)n00b_object_type_info(user_object)->vtable;
}

static inline void
n00b_restore(void *user_object)
{
    n00b_vtable_t  *vt = n00b_vtable(user_object);
    n00b_restore_fn fn = (void *)vt->methods[N00B_BI_RESTORE];

    if (fn != NULL) {
        (*fn)(user_object);
    }
}

static inline bool
n00b_is_null(void *n)
{
    return n == NULL;
}

static inline bool
n00b_is_renderable(void *user_object)
{
    n00b_vtable_t *vt = n00b_vtable(user_object);

    return vt && vt->methods[N00B_BI_RENDER] != NULL;
}
