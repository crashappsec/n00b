#pragma once
#include "n00b/base.h"

// I've removed both of these for the moment. Currently, we either
// scan an entire allocation, or none of it.
//
// I've also removed memcheck via shadow allocs and ASAN support.
//
// I *have* kept allocation end sentinels though.

typedef void (*n00b_mem_scan_fn)(uint64_t *, void *);
typedef void (*n00b_system_finalizer_fn)(void *);

typedef struct n00b_alloc_hdr         n00b_alloc_hdr;
typedef struct n00b_finalizer_info_t *n00b_finalizer_info_t;
typedef struct n00b_gc_root_info_t    n00b_gc_root_info_t;
typedef struct n00b_alloc_hdr         n00b_alloc_hdr;
typedef struct n00b_heap_t            n00b_heap_t;
typedef struct n00b_arena_t           n00b_arena_t;

// Combine these ASAP.
typedef struct n00b_alloc_record_t {
    uint64_t     empty_guard;
    n00b_type_t *type;
#if defined(N00B_ADD_ALLOC_LOC_INFO)
    char *alloc_file;
#endif
    int32_t alloc_len;

#if defined(N00B_ADD_ALLOC_LOC_INFO)
    int16_t alloc_line;
#endif

    uint8_t n00b_marshal_end : 1;
    uint8_t n00b_ptr_scan    : 1;
    uint8_t n00b_obj         : 1;
    uint8_t n00b_finalize    : 1;
    uint8_t n00b_traced      : 1;
    uint8_t n00b_moving      : 1;
#if defined(N00B_GC_ALLOW_DEBUG_BIT)
    uint8_t n00b_debug : 1;
#endif

    uint64_t cached_hash[2];
    alignas(N00B_FORCED_ALIGNMENT) uint64_t data[0];
} n00b_alloc_record_t;

struct n00b_alloc_hdr {
    // A guard value added to every allocation so that cross-heap
    // memory accesses can scan backwards (if needed) to determine if
    // a write should be forwarded to a new location.
    //
    // It also tells us whether the cell has been allocated at all,
    // as it is not set until we allocate.
    //
    // The guard value is picked once per runtime by reading from
    // /dev/urandom, to make sure that we do not start adding
    // references  in memory to it.
    uint64_t guard;
    // This can eventually go down to just a 32-bit int.  For the
    // moment, this pointer gets overwritten when we set n00b_moved;
    // it certainly does get migrated first.

    struct n00b_type_t *type;

#if defined(N00B_ADD_ALLOC_LOC_INFO)
    char *alloc_file;
#endif

    int32_t alloc_len;
    // This is going to move to the VTABLE for objects, and perhaps
    // non-object types will get a few bits to index into a static
    // table.
    //
    // For now, the can fn it's moving down below into a single bit.
    // The 1st arg to the scan fn is a pointer to a sized
    // bitfield. The first word indicates the number of subsequent
    // words in the bitfield. The bits then represent the words of the
    // data structure, in order, and whether they contain a pointer to
    // track (such pointers MUST be 64-bit aligned).  If this function
    // exists, it's passed the # of words in the alloc and a pointer
    // to a bitfield that contains that many bits. The bits that
    // correspond to words with pointers should be set.
    // n00b_mem_scan_fn scan_fn;

#if defined(N00B_ADD_ALLOC_LOC_INFO)
    int16_t alloc_line;
#endif

    // True if the memory allocation is a direct n00b object, in
    // which case, we expect the type field to be valid.
    //
    // Used by Marshal to deliniate end-of-record.
    uint8_t n00b_marshal_end : 1;
    uint8_t n00b_ptr_scan    : 1;
    // Currently, we aren't using this for anything, because we just
    // rely on the presence of the type field. However, it's no cost
    // to keep it if we want the double checking.
    uint8_t n00b_obj         : 1;
    // Uses the finalizer associated w/ the type, so n00b_obj must be true.
    uint8_t n00b_finalize    : 1;
    // This gets set in out-of-heap allocs that have pointers, then cleared
    // when we finish GC.
    uint8_t n00b_traced      : 1;
    // This makes it easy for us to keep track of whether an alloc is
    // being copied.
    uint8_t n00b_moving      : 1;

#if defined(N00B_GC_ALLOW_DEBUG_BIT)
    // Can be set in debug mode to determine whether to print info on
    // the object.
    uint8_t n00b_debug : 1;
#endif
    // For the moment,this gets commandeered during collection.
    // It's turned into 2 64-byte objects, by casting it to a
    // n00b_alloc_record_t above.
    //
    // The first item is used to store the forwarding pointer.
    __uint128_t cached_hash;

    alignas(N00B_FORCED_ALIGNMENT) uint64_t data[];
};

#define N00B_HDR_GUARD_OFFSET 0
#define N00B_HDR_TYPE_OFFSET  offsetof(n00b_alloc_hdr, type)
#define N00B_HDR_LEN_OFFSET   offsetof(n00b_alloc_hdr, alloc_len)
#define N00B_HDR_HASH_OFFSET  offsetof(n00b_alloc_hdr, cached_hash)

#if defined(N00B_ADD_ALLOC_LOC_INFO)
#define N00B_HDR_FILE_OFFSET offsetof(n00b_alloc_hdr, alloc_file)
#define N00B_HDR_LINE_OFFSET offsetof(n00b_alloc_hdr, alloc_line)
#endif

struct n00b_finalizer_info_t {
    n00b_alloc_hdr        *allocation;
    n00b_finalizer_info_t *next;
    n00b_finalizer_info_t *prev;
};

typedef struct n00b_gc_root_tinfo_t {
    uint64_t thread_id : 63;
    uint64_t inactive  : 1;
} n00b_gc_root_tinfo_t;

struct n00b_gc_root_info_t {
    void                        *ptr;
    uint32_t                     num_items;
    // Ignored unless doing a linear scan for empty slots, which we do
    // for heaps where we keep temporary roots (the marshal heap only
    // for right now).
    _Atomic n00b_gc_root_tinfo_t tinfo;

#ifdef N00B_ADD_ALLOC_LOC_INFO
    char *file;
    int   line;
#endif
};

typedef struct {
    uint64_t      last_tid;
    n00b_arena_t *arena;
} n00b_crit_check_info_t;

typedef struct {
    n00b_alloc_hdr *next_alloc;
    void           *thread;
} n00b_crit_t;

// In our lingo, "heaps" consist of one or more "arenas". We link-list
// arenas together.
//
// We allocate the heap structure out of a separate data structure
// that is a list of pointers to heaps we've allocated, so that we can
// easily find all the heaps when we need to test whether we've
// got an address or not.
//
// Arenas on the other hand, have their data structure on a page that
// is sandwiched between guard pages. A third guard page lives at the
// very end. For convenience, n00b_arena_t * pointers point at the
// metadata start. When doing allocs / deallocs we consider the guard
// pages.
//
// If we didn't have to dynamically query the page size, we'd just
// make it static.

struct n00b_heap_t {
    int64_t                heap_id;
    _Atomic n00b_crit_t    ptr;
    char                  *cur_arena_end;
    // This is per-collection cycle; total_alloc_count below is
    // all-time.
    _Atomic uint32_t       alloc_count;
    uint32_t               inherit_count;
    n00b_arena_t          *first_arena;
    n00b_arena_t          *newest_arena;
    hatrack_zarray_t      *roots;
    n00b_finalizer_info_t *to_finalize;
    char                  *name;
    char                  *file;
    int                    line;
    uint32_t               total_alloc_count;
    uint16_t               num_collects;
    // This prevents heaps from being collected at all. memory addreses
    // in pinned heaps cannot be moved.
    unsigned int           pinned         : 1;
    // This indicates that the heap record (which is static once
    // allocated) is for a fully freed heap. Eventually, this will get
    // reused.
    unsigned int           released       : 1;
    // This gets marked by the collector when collection is done, if
    // we didn't discard enough trash; in the next collection cycle,
    // the heap size will double.
    unsigned int           expand         : 1;
    // Don't trace (or rewrite) pointers from the heap being collected
    // that are found in this heap (unless this heap is the one being
    // collected).
    //
    // For instance, we do not want to munge pointers in the GC
    // destination heap curing collection if we have one on the stack.
    //
    // Heaps with this set do not have their roots traced when
    // collecting other heaps. This is used to indicate that the heap
    // has no pointers to other collectable heaps.
    //
    // Again, if we do collect a heap with this set, will always trace
    // our own heap's pointers (we do this when out of room, unless
    // it's pinned).
    unsigned int           no_trace       : 1;
    // For some heaps, we don't need to bother scanning 'global'
    // roots; ours are good enough. Other heaps will trace into this
    // heap though, unless no_trace is also set.
    //
    // Additionally, when collecting heaps that have this set, we do
    // NOT look through other heaps for pointers to rewrite.
    //
    // The point here is that, when we collect THIS heap, we expect
    // that every outside pointer into this heap is irrelevent and
    // that we will reach it with local roots, so don't scan
    // the whole world.
    unsigned int           local_collects : 1;
    // If private, calling 'n00b_addr_find_heap() or n00b_in_heap() on
    // an address will indicate that it is not a heap address, unless
    // the ask is coming from the 'current' heap. Lingering pointers
    // into this heap will not be rewritten, and should possibly
    // throw errors (but do not).
    //
    // This ALSO implies no-trace.
    unsigned int private                  : 1;
};

struct n00b_arena_t {
    void         *addr_start;
    void         *addr_end;
    void         *last_issued;
    n00b_arena_t *successor;
    size_t        user_length; // Just for convenience.
};

// The goal here is to make it easy to change the amount of space
// associated with common data objects when we're treating them like
// arrays.
typedef union n00b_mem_ptr {
    void                 **lvalue;
    void                  *v;
    char                  *c;
    uint8_t               *u8;
    int32_t               *codepoint;
    uint32_t              *u32;
    int64_t               *i64;
    uint64_t              *u64;
    struct n00b_alloc_hdr *alloc;
    union n00b_mem_ptr    *memptr;
    uint64_t               nonpointer;
} n00b_mem_ptr;

extern uint64_t n00b_gc_guard;
extern int      __n00b_collector_running;

#if defined(N00B_FULL_MEMCHECK)
extern void n00b_run_memcheck(n00b_heap_t *);
#else
#define n00b_run_memcheck(h)
#endif

#ifdef N00B_ADD_ALLOC_LOC_INFO
#define N00B_ALLOC_XTRA      , char *file, int line
#define N00B_ALLOC_XPARAM    , file, line
#define N00B_ALLOC_CALLPARAM , __FILE__, __LINE__
#else
#define N00B_ALLOC_XTRA
#define N00B_ALLOC_XPARAM
#define N00B_ALLOC_CALLPARAM
#endif

// The global heap.
extern n00b_heap_t *n00b_default_heap;

// The last heap explicitly passed by a thread. IF it is not set
// (i.e., is NULL) when you call n00b_new(), that will set this when
// you call it, to ensure that sub-allocations use the same
// allocator. When that happens, it will reset to NULL.

extern n00b_heap_t    *n00b_addr_find_heap(void *);
extern bool            n00b_addr_in_heap(n00b_heap_t *, void *);
extern n00b_heap_t    *_n00b_new_heap(uint64_t, char *, int);
extern void            n00b_delete_heap(n00b_heap_t *);
extern void            _n00b_heap_register_root(n00b_heap_t *,
                                                void *,
                                                uint64_t
                                                    N00B_ALLOC_XTRA);
extern void            _n00b_heap_register_dynamic_root(n00b_heap_t *,
                                                        void *,
                                                        uint64_t
                                                            N00B_ALLOC_XTRA);
extern void            n00b_heap_remove_root(n00b_heap_t *, void *);
extern n00b_alloc_hdr *n00b_find_allocation_record(void *);
extern void           *_n00b_heap_alloc(n00b_heap_t *,
                                        size_t,
                                        bool,
                                        n00b_mem_scan_fn
                                            N00B_ALLOC_XTRA);
extern void           *n00b_malloc_wrap(size_t, void *, char *, int);
extern void            n00b_free_wrap(void *, size_t, void *, char *, int);
extern void           *n00b_realloc_wrap(void *,
                                         size_t,
                                         size_t,
                                         void *,
                                         char *,
                                         int);
extern void           *n00b_malloc_compat(size_t);
extern void           *n00b_realloc_compat(void *, size_t);
extern void            n00b_free_compat(void *);
extern void            n00b_initialize_gc(void);
extern void            n00b_gc_set_system_finalizer(n00b_system_finalizer_fn);
extern void            n00b_heap_collect(n00b_heap_t *, int64_t);
extern uint64_t        n00b_get_page_size(void);
extern void            n00b_long_term_pin(n00b_heap_t *);
// in utils/deep_copy.c
extern void           *n00b_heap_deep_copy(void *);

#define n00b_new_heap(x) _n00b_new_heap(x, __FILE__, __LINE__)
#define n00b_global_heap_collect() \
    n00b_heap_collect(n00b_default_heap, 0)
#define n00b_heap_alloc(h, sz, f) \
    _n00b_heap_alloc(h, sz, false, f N00B_ALLOC_CALLPARAM)
#define n00b_heap_alloc_guarded(h, sz, f) \
    _n00b_heap_alloc(h, sz, true, f N00B_ALLOC_CALLPARAM)

static inline bool
n00b_in_heap(void *addr)
{
    return n00b_addr_find_heap(addr) != NULL;
}

static inline void
n00b_heap_set_roots(n00b_heap_t *h, hatrack_zarray_t *roots)
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
    n00b_mem_ptr p = {.v = o};
    p.alloc -= 1;

    return p.alloc;
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

extern void         n00b_add_arena(n00b_heap_t *, uint64_t);
extern bool         n00b_addr_in_one_heap(n00b_heap_t *, void *);
extern n00b_string_t *noob_debug_repr_heap(n00b_heap_t *);
extern void         n00b_debug_print_heap(n00b_heap_t *);
extern n00b_string_t *n00b_debug_repr_all_heaps(void);
extern void         n00b_debug_all_heaps(void);

extern int          __n00b_collector_running;
extern n00b_heap_t *__n00b_current_from_space;

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
extern bool         n00b_collection_suspended;
extern void         n00b_suspend_collections(void);
extern void         n00b_allow_collections(void);

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

#if defined(N00B_MPROTECT_WRAPPED_ALLOCS)
extern bool n00b_protect_wrapped_allocs;
extern bool n00b_protect_wrapping_auto_off;
#endif
