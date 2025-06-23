#include "n00b.h"

static inline bool
n00b_in_heap(void *addr)
{
    return n00b_addr_find_heap(addr, false) != NULL;
}

static inline bool
n00b_in_any_heap(void *addr)
{
    return n00b_addr_find_heap(addr, true) != NULL;
}

static inline void
n00b_heap_set_roots(n00b_heap_t *h, n00b_zarray_t *roots)
{
    h->roots = roots;
}

static inline void
n00b_heap_pin(n00b_heap_t *h)
{
    h->pinned = true;
}

static inline void
n00b_heap_unpin(n00b_heap_t *h)
{
    h->pinned = false;
}

static inline void
n00b_heap_set_no_trace(n00b_heap_t *h)
{
    h->no_trace = true;
}

static inline void
n00b_heap_set_local_collects(n00b_heap_t *h)
{
    h->local_collects = true;
}

static inline void
n00b_heap_set_private(n00b_heap_t *h)
{
    h->private = true;
}

static inline ptrdiff_t
n00b_ptr_diff(void *base, void *field)
{
    return (int64_t *)field - (int64_t *)base;
}

static inline bool
n00b_heap_is_pinned(n00b_heap_t *h)
{
    return h->pinned || __n00b_collector_running;
}

static inline void
n00b_mark_raw_to_addr(uint64_t *bitfield, void *base, void *end)
{
    uint64_t diff = n00b_ptr_diff(base, end);

    while (diff >= 64) {
        *bitfield++ = ~0ULL;
        diff -= 64;
    }
    *bitfield = (1ULL << (diff + 1)) - 1;
}

static inline n00b_alloc_hdr *
n00b_object_header(void *o)
{
    n00b_alloc_hdr *hdr = (void *)o;
    return hdr - 1;
}

static inline void *
n00b_unprotected_mempage(void)
{
    return mmap(NULL,
                n00b_get_page_size(),
                PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANON,
                0,
                0);
}

static inline void
n00b_delete_mempage(void *page)
{
    int sz = n00b_get_page_size();

#if defined(N00B_MADV_ZERO)
    madvise(page, sz, MADV_ZERO);
    mprotect(page, sz, PROT_NONE);
#endif
    munmap(page, sz);
}

#ifdef N00B_ADD_ALLOC_LOC_INFO
#define n00b_gc_raw_alloc(sz, mem)            \
    _n00b_heap_alloc(n00b_current_heap(NULL), \
                     sz,                      \
                     false,                   \
                     mem,                     \
                     __FILE__,                \
                     __LINE__)
#define n00b_gc_guarded_alloc(sz, mem)        \
    _n00b_heap_alloc(n00b_current_heap(NULL), \
                     sz,                      \
                     true,                    \
                     mem,                     \
                     __FILE__,                \
                     __LINE__)
#define n00b_halloc(heap, sz, mem)            \
    _n00b_heap_alloc(n00b_current_heap(heap), \
                     sz,                      \
                     false,                   \
                     mem,                     \
                     __FILE__,                \
                     __LINE__)
#define n00b_guarded_halloc(heap, sz, mem)    \
    _n00b_heap_alloc(n00b_current_heap(heap), \
                     sz,                      \
                     true,                    \
                     mem,                     \
                     __FILE__,                \
                     __LINE__)

#else
#define n00b_gc_raw_alloc(sz, mem) \
    _n00b_heap_alloc(n00b_current_heap(NULL), sz, false, mem)
#define n00b_gc_guarded_alloc(sz, mem) \
    _n00b_heap_alloc(n00b_current_heap(NULL), sz, true, mem)
#define n00b_halloc(heap, sz, mem) \
    _n00b_heap_alloc(n00b_current_heap(heap), sz, false, mem)
#define n00b_guarded_halloc(heap, sz, mem) \
    _n00b_heap_alloc(n00b_current_heap(heap), sz, true, mem)
#endif

static inline uint64_t
n00b_round_up_to_given_power_of_2(uint64_t power, uint64_t n)
{
    uint64_t modulus   = (power - 1);
    uint64_t remainder = n & modulus;

    if (!remainder) {
        return n;
    }
    else {
        return (n & ~modulus) + power;
    }
}

#define N00B_GC_SCAN_ALL  ((void *)0)
#define N00B_GC_SCAN_NONE ((void *)0xffffffffffffffff)

#define n00b_gc_alloc_mapped(typename, map) \
    n00b_gc_raw_alloc(sizeof(typename), (void *)map)

#define n00b_gc_flex_alloc(fixed, var, numv, map) \
    (n00b_gc_raw_alloc((size_t)(sizeof(fixed)) + (sizeof(var)) * (numv), (map)))

#define n00b_gc_flexguard_alloc(fixed, var, numv, map)                       \
    (n00b_gc_guarded_alloc((size_t)(sizeof(fixed)) + (sizeof(var)) * (numv), \
                           (map)))

#define n00b_gc_alloc(typename) \
    n00b_gc_raw_alloc(sizeof(typename), N00B_GC_SCAN_ALL)

#define n00b_gc_value_alloc(typename) \
    n00b_gc_raw_alloc(sizeof(typename), N00B_GC_SCAN_NONE)

#define n00b_gc_array_alloc(typename, n) \
    n00b_gc_raw_alloc((sizeof(typename) * n), N00B_GC_SCAN_ALL)

#define n00b_gc_array_value_alloc(typename, n) \
    n00b_gc_raw_alloc((sizeof(typename) * n), N00B_GC_SCAN_NONE)

#define n00b_gc_array_alloc_mapped(typename, n, map) \
    n00b_gc_raw_alloc((sizeof(typename) * n), (void *)map)

#ifdef N00B_ADD_ALLOC_LOC_INFO
#define n00b_heap_register_root(h, p, l) \
    _n00b_heap_register_root(n00b_current_heap(h), p, l, __FILE__, __LINE__)
#define n00b_heap_register_dynamic_root(h, p, l) \
    _n00b_heap_register_dynamic_root(h, p, l, __FILE__, __LINE__)
#else
#define n00b_heap_register_root(h, p, l) \
    _n00b_heap_register_root(n00b_current_heap(h), p, l)
#define n00b_heap_register_dynamic_root(h, p, l) \
    _n00b_heap_register_dynamic_root(h, p, l)
#endif

#define n00b_gc_register_root(p, l) n00b_heap_register_root(NULL, p, l)

#define n00b_gc_show_heap_stats_on()

#ifdef N00B_USE_INTERNAL_API

// Each arena has overhead beyond what's available to the
// user. Specifically, it's three guard pages, and some meta-data,
// which we put on its own page.
#define N00B_ARENA_OVERHEAD (4 * n00b_page_bytes)
#define N00B_ALLOC_OVERHEAD sizeof(n00b_header_t)

extern void           n00b_add_arena(n00b_heap_t *, uint64_t);
extern n00b_string_t *noob_debug_repr_heap(n00b_heap_t *);
extern void           n00b_debug_log_heap(n00b_heap_t *);
extern n00b_string_t *n00b_debug_repr_all_heaps(void);
extern void           n00b_debug_all_heaps(void);
extern n00b_heap_t   *__n00b_current_from_space;

extern n00b_heap_t *n00b_all_heaps;
extern n00b_heap_t *n00b_cur_heap_page;
extern n00b_heap_t *n00b_to_space;
extern n00b_heap_t *n00b_internal_heap;
extern n00b_heap_t *n00b_debug_heap;
extern uint64_t     n00b_next_heap_index;
extern uint64_t     n00b_page_bytes;
extern uint64_t     n00b_page_modulus;
extern uint64_t     n00b_modulus_mask;
// Heap entries per page, capculated once.
extern int          n00b_heap_entries_pp;
extern void         n00b_suspend_collections(void);
extern void         n00b_allow_collections(void);
extern bool         _n00b_addr_in_one_heap(n00b_heap_t *, void *);

static inline bool
n00b_addr_in_one_heap(n00b_heap_t *h, void *addr)
{
    // The base impl skips this check for privacy to avoid
    // too much code duplication for n00b_in_any_heap()
    if (h->private && __n00b_current_from_space != h) {
        return false;
    }

    return _n00b_addr_in_one_heap(h, addr);
}

static inline void
n00b_heap_set_name(n00b_heap_t *h, char *n)
{
    h->name = n;
}

static inline bool
n00b_addr_in_arena(n00b_arena_t *arena, void *addr)
{
    // We consider addresses in the arena if it's a user-usable
    // address, or if it's the start position of the end guard.

    assert(arena->addr_end > arena->addr_start);
    return addr >= arena->addr_start && addr < arena->addr_end;
}

static inline int64_t
n00b_calculate_alloc_len(int64_t ask)
{
    int64_t len = ask + sizeof(n00b_alloc_hdr);

    return n00b_round_up_to_given_power_of_2(N00B_FORCED_ALIGNMENT, len);
}

static inline void *
n00b_raw_alloc_to_arena_addr(char *alloc)
{
    return alloc + n00b_page_bytes;
}

static inline void *
n00b_arena_to_alloc_location(n00b_arena_t *a)
{
    return ((char *)a) - n00b_page_bytes;
}

static inline void *
n00b_arena_front_guard_start(n00b_arena_t *a)
{
    return ((char *)a) + n00b_page_bytes;
}

static inline void *
n00b_arena_user_data_start(n00b_arena_t *a)
{
    return ((char *)a) + n00b_page_bytes * 2;
}

static inline void *
n00b_arena_rear_guard_start(n00b_arena_t *a)
{
    return ((char *)n00b_arena_user_data_start(a)) + a->user_length;
}

static inline void
n00b_delete_arena(n00b_arena_t *a)
{
    void  *alloc_start = n00b_arena_to_alloc_location(a);
    size_t len         = a->user_length + N00B_ARENA_OVERHEAD;

#if defined(N00B_MADV_ZERO)
    madvise(alloc_start, len, MADV_ZERO);
    mprotect(alloc_start, len, PROT_NONE);
#endif
    munmap(alloc_start, len);
}

static inline void
n00b_lock_arena_header(n00b_arena_t *a)
{
    mprotect(a, n00b_page_bytes, PROT_READ);
}

static inline void
n00b_unlock_arena_header(n00b_arena_t *a)
{
    mprotect(a, n00b_page_bytes, PROT_READ | PROT_WRITE);
}

#if defined(N00B_GC_ALLOW_DEBUG_BIT)
extern bool n00b_using_debug_bit;

static inline void
n00b_start_using_debug_bit(void)
{
    n00b_using_debug_bit = true;
}

static inline void
n00b_stop_using_debug_bit(void)
{
    n00b_using_debug_bit = false;
}

static inline void *
n00b_debug_alloc(void *addr)
{
    n00b_alloc_hdr *h = n00b_find_allocation_record(addr);

    h->n00b_debug = true;

    return addr;
}

#endif

extern void n00b_heap_clear(n00b_heap_t *);
extern void n00b_heap_prune(n00b_heap_t *);

#endif

static inline void
n00b_heap_force_expansion(n00b_heap_t *h)
{
    h->expand = true;
}

static inline void
n00b_heap_no_force_expansion(n00b_heap_t *h)
{
    h->expand = false;
}

static inline bool
n00b_heap_has_multiple_arenas(n00b_heap_t *h)
{
    return h->first_arena && h->newest_arena
        && h->first_arena != h->newest_arena;
}

#ifdef N00B_ENABLE_ALLOC_DEBUG
#define n00b_enable_thread_alloc_log()                   \
    {                                                    \
        n00b_get_tsi_ptr()->show_alloc_locations = true; \
        fprintf(stderr,                                  \
                "Alloc logging on for thread %p\n",      \
                n00b_thread_self());                     \
    }
#define n00b_disable_thread_alloc_log()                   \
    {                                                     \
        n00b_get_tsi_ptr()->show_alloc_locations = false; \
        fprintf(stderr,                                   \
                "Alloc logging off for thread %p\n",      \
                n00b_thread_self());                      \
    }

#endif
