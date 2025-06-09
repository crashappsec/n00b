#define N00B_USE_INTERNAL_API
#include "n00b.h"

// There is one "global" heap for most allocations, but the same API
// can be used for private heaps (the raw arena interface).
//
// n00b_in_heap() checks only the bounds of the main heap;
// n00b_in_any_heap() checks all heaps we've allocated.

// This value is chosen randomly, and is used to identify the start of
// memory records. Each process gets a single guard; it is not
// persistant.
uint64_t            n00b_gc_guard      = 0;
// The global heap.
n00b_heap_t        *n00b_default_heap  = NULL;
// This is the current heap for a given thread. If it's null, use the
// default heap.
n00b_heap_t        *n00b_all_heaps     = NULL;
n00b_heap_t        *n00b_cur_heap_page = NULL;
static n00b_heap_t *long_term_pins     = NULL;
n00b_heap_t        *n00b_to_space      = NULL;
uint64_t            n00b_page_bytes;
uint64_t            n00b_page_modulus;
uint64_t            n00b_modulus_mask;
uint64_t            n00b_next_heap_index;
int                 n00b_heap_entries_pp;

n00b_heap_t *
n00b_addr_find_heap(void *p, bool any)
{
    if (!p) {
        return NULL;
    }

    n00b_heap_t *h = n00b_all_heaps;

    for (uint64_t i = 0; i < n00b_next_heap_index; i++) {
        if (i && !(i % n00b_heap_entries_pp)) {
            h = *(n00b_heap_t **)h;
            continue;
        }

        if (any && _n00b_addr_in_one_heap(h, p)) {
            return h;
        }
        if (n00b_addr_in_one_heap(h, p)) {
            return h;
        }

        h++;
    }

    return NULL;
}

bool
_n00b_addr_in_one_heap(n00b_heap_t *h, void *p)
{
    if (h->released) {
        return false;
    }

    n00b_arena_t *a = h->first_arena;

    while (a) {
        if (n00b_addr_in_arena(a, p)) {
            if (a == h->newest_arena) {
                n00b_crit_t crit = atomic_read(&h->ptr);
                if (p > (void *)crit.next_alloc) {
                    return false;
                }
            }
            return true;
        }
        a = a->successor;
    }

    return false;
}

n00b_string_t *
n00b_debug_repr_heap(n00b_heap_t *p)
{
    n00b_push_heap(n00b_default_heap);
    n00b_crit_t   crit = atomic_read(&p->ptr);
    char         *head = (char *)crit.next_alloc;
    int64_t       used = 0;
    uint64_t      free = 0;
    n00b_arena_t *a    = p->first_arena;

    while (a) {
        if (a->successor) {
            used += ((char *)a->addr_end) - (char *)a->addr_start;
        }
        else {
            used += head - (char *)a->addr_start;
            if (head < (char *)a->addr_end) {
                free = ((char *)a->addr_end) - head;
            }
        }
        a = a->successor;
    }

    char name[PIPE_BUF];

    if (p->name) {
        snprintf(name, PIPE_BUF - 1, "'%s' ", p->name);
    }
    else {
        name[0] = 0;
    }

    assert(used >= 0);
    n00b_string_t *propstr;
    n00b_string_t *r;

    if (p->released) {
        propstr = n00b_cstring(" (released)");
    }
    else {
        if (p->pinned) {
            propstr = n00b_cstring(" (pinned)");
        }
        else {
            propstr = n00b_cached_empty_string();
        }
        if (p->no_trace) {
            propstr = n00b_string_concat(propstr, n00b_cstring(" (no-trace)"));
        }
        if (p->local_collects) {
            propstr = n00b_string_concat(propstr, n00b_cstring(" (local gc)"));
        }
        if (p->private) {
            propstr = n00b_string_concat(propstr, n00b_cstring(" (private)"));
        }
    }

    r = n00b_cformat("Heap «#» (#«#»; «#»:«#»)«#»\n",
                     n00b_cstring(name),
                     (uint64_t)p->heap_id,
                     n00b_cstring(p->file),
                     (uint64_t)p->line,
                     propstr);

    if (p->released) {
        n00b_pop_heap();
        return r;
    }

    n00b_string_t *stats;
    stats = n00b_cformat(
        "\n«em4»Collects:«/»«#»\n"
        "«em4»Memory«/» Used: «#»b; Free: «#»b\n"
        "«em4»Allocs:«/» Since collect: «#»; "
        "Inherited: «#»; Lifetime: «#»\n",
        (uint64_t)p->num_collects,
        (uint64_t)used,
        (uint64_t)free,
        (uint64_t)p->alloc_count,
        (uint64_t)p->inherit_count,
        (uint64_t)p->total_alloc_count);

    r = n00b_string_concat(r, stats);
    a = p->first_arena;

    if (a) {
        r = n00b_cformat("«#»«em5»Next alloc:«/» «#:x»\n«em6»Arenas:«/»",
                         r,
                         (uint64_t)crit.next_alloc);

        while (a) {
            r = n00b_cformat("«#»  «#:p»-«#:p»\n",
                             r,
                             (uint64_t)a->addr_start,
                             (uint64_t)a->addr_end);
            a = a->successor;
        }
    }

    n00b_pop_heap();
    return r;
}

void
n00b_debug_log_heap(n00b_heap_t *p)
{
#if N00B_DLOG_GC_LEVEL >= 0
    n00b_crit_t   crit = atomic_read(&p->ptr);
    char         *head = (char *)crit.next_alloc;
    int64_t       used = 0;
    uint64_t      free = 0;
    n00b_arena_t *a    = p->first_arena;

    while (a) {
        if (a->successor) {
            used += ((char *)a->addr_end) - (char *)a->addr_start;
        }
        else {
            used += head - (char *)a->addr_start;
            if (head < (char *)a->addr_end) {
                free = ((char *)a->addr_end) - head;
            }
        }
        a = a->successor;
    }

    n00b_dlog_gc("Heap '%s' @%p (#%lld; %s:%d)%s%s%s%s; #gcs: %d; used: %lld b; free: %lld b",
                 p->name ? p->name : "(anon)",
                 p,
                 (long long int)p->heap_id,
                 p->file,
                 p->line,
                 p->pinned ? " (pinned)" : "",
                 p->no_trace ? " (no-trace)" : "",
                 p->local_collects ? " (local gc)" : "",
                 p->private ? " (private)" : "",
                 p->num_collects,
                 (long long int)used,
                 (long long int)free);
    n00b_dlog_gc1("Allocs: since last collect: %d; Lifetime: %d\n",
                  p->alloc_count,
                  p->total_alloc_count);

#if defined(N00B_DLOG_GC_ON) && N00B_DLOG_GC_LEVEL >= 2
    a = p->first_arena;

    if (a) {
        n00b_dlog_gc2("Next alloc: %p", crit.next_alloc);
        int i = 0;
        while (a) {
            n00b_dlog_gc2("Arena %d (heap #%lld): %p-%p",
                          ++i,
                          (long long int)p->heap_id,
                          a->addr_start,
                          a->addr_end);

            a = a->successor;
        }
    }
#endif
#endif

    return;
}

n00b_string_t *
n00b_debug_repr_all_heaps(void)
{
    n00b_push_heap(n00b_default_heap);
    n00b_string_t *r = n00b_debug_repr_heap(n00b_all_heaps);

    for (unsigned int i = 1; i < n00b_next_heap_index; i++) {
        r = n00b_string_concat(r, n00b_debug_repr_heap(n00b_all_heaps + i));
    }

    n00b_pop_heap();
    return r;
}

void
n00b_debug_all_heaps(void)
{
    for (unsigned int i = 0; i < n00b_next_heap_index; i++) {
        n00b_debug_log_heap(n00b_all_heaps + i);
    }
}

static inline void
n00b_create_alloc_guards(void)
{
    n00b_gc_guard = n00b_rand64();
}

static inline void
n00b_discover_page_info(void)
{
    n00b_page_bytes   = getpagesize();
    // Page size is always a power of 2.
    n00b_page_modulus = n00b_page_bytes - 1;
    n00b_modulus_mask = ~n00b_page_modulus;
}

static inline void
n00b_setup_heap_info(void)
{
    n00b_heap_entries_pp = (n00b_page_bytes / sizeof(n00b_heap_t)) - 1;
}

static inline void
n00b_create_first_heaps(void)
{
    n00b_default_heap = n00b_new_heap(N00B_DEFAULT_HEAP_SIZE);
    n00b_heap_set_name(n00b_default_heap, "user");

    // Create the main allocation heap and the 'to_space' heap that
    // marshalling uses.

    // No arenas get created within this heap at first.
    n00b_to_space = n00b_new_heap(0);
    n00b_heap_pin(n00b_to_space);
    n00b_heap_set_no_trace(n00b_to_space);

    long_term_pins = n00b_new_heap(0);
    n00b_heap_pin(long_term_pins);

    n00b_default_heap->name = "user";
    n00b_to_space->name     = "gc 'to-space'";
    long_term_pins->name    = "long-term pins";

    long_term_pins->no_trace = true;
}

void
n00b_long_term_pin(n00b_heap_t *h)
{
    // Long-term pins a heap, which marks it as no_trace.
    n00b_heap_collect(h, 0);
    N00B_DBG_CALL(n00b_stop_the_world);
    n00b_crit_t crit = atomic_read(&h->ptr);

    if (!long_term_pins->first_arena) {
        long_term_pins->first_arena = h->first_arena;
        n00b_unlock_arena_header(h->newest_arena);
        h->newest_arena->last_issued = crit.next_alloc;
        n00b_lock_arena_header(h->newest_arena);
        long_term_pins->newest_arena      = h->newest_arena;
        long_term_pins->total_alloc_count = atomic_read(&h->alloc_count);
        long_term_pins->ptr               = crit;
    }
    else {
        n00b_arena_t *a = long_term_pins->newest_arena;
        a->successor    = h->first_arena;
        n00b_unlock_arena_header(h->newest_arena);
        h->newest_arena->last_issued = crit.next_alloc;
        n00b_lock_arena_header(h->newest_arena);
        long_term_pins->newest_arena = h->newest_arena;
        long_term_pins->total_alloc_count += atomic_read(&h->alloc_count);
        n00b_lock_arena_header(a);
        long_term_pins->ptr = crit;
    }

    h->first_arena  = NULL;
    h->newest_arena = NULL;
    n00b_add_arena(h, long_term_pins->newest_arena->user_length);

    N00B_DBG_CALL(n00b_restart_the_world);
}

void
_n00b_heap_register_root(n00b_heap_t *h, void *p, uint64_t l N00B_ALLOC_XTRA)
{
    // Len is measured in 64 bit words and must be at least 1.
    n00b_gc_root_info_t *ri;

    h = n00b_current_heap(h);

    if (!h->roots) {
        h->roots = hatrack_zarray_new(N00B_MAX_GC_ROOTS,
                                      sizeof(n00b_gc_root_info_t));
    }

    hatrack_zarray_new_cell(h->roots, (void *)&ri);
    ri->num_items = l;
    ri->ptr       = p;
#ifdef N00B_ADD_ALLOC_LOC_INFO
    ri->file = file;
    ri->line = line;
#endif
}

void
_n00b_heap_register_dynamic_root(n00b_heap_t *h,
                                 void        *p,
                                 uint64_t l
                                     N00B_ALLOC_XTRA)
{
    int                  max = h->roots ? hatrack_zarray_len(h->roots) : 0;
    uint64_t             id  = (uint64_t)n00b_thread_self();
    n00b_gc_root_tinfo_t expect;
    n00b_gc_root_tinfo_t new = {.thread_id = id >> 1, .inactive = false};

    for (int i = 0; i < max; i++) {
        n00b_gc_root_info_t *ri = hatrack_zarray_cell_address(h->roots, i);
        expect.thread_id        = 0;
        expect.inactive         = true;

        if (CAS(&ri->tinfo, &expect, new)) {
            ri->num_items = l;
            ri->ptr       = p;
#ifdef N00B_ADD_ALLOC_LOC_INFO
            ri->file = file;
            ri->line = line;
#endif
            return;
        }
    }
    n00b_heap_register_root(h, p, l);
}

void
n00b_heap_remove_root(n00b_heap_t *h, void *ptr)
{
    int32_t max = atomic_load(&h->roots->length);

    for (int i = 0; i < max; i++) {
        n00b_gc_root_info_t *ri = hatrack_zarray_cell_address(h->roots, i);
        if (ri->ptr == ptr) {
            ri->num_items = 0;
            ri->ptr       = NULL;

            n00b_gc_root_tinfo_t ti = {.thread_id = 0, .inactive = true};
            atomic_store(&ri->tinfo, ti);
        }
    }
}

n00b_alloc_hdr *
n00b_find_allocation_record(void *addr)
{
    if (((uint64_t)addr) & 0x0000000000000007) {
        addr = (void *)(((uint64_t)addr) & ~0x0000000000000007ULL);
    }

    void       **p = (void **)addr;
    n00b_heap_t *h = n00b_addr_find_heap(p, true);

    if (!h) {
        return NULL;
    }

    n00b_arena_t *a = h->first_arena;
    while (true) {
        if (!a) {
            return NULL;
        }
        if (addr >= a->addr_start && addr < a->last_issued) {
            break;
        }
        a = a->successor;
    }

    while (p > (void **)a->addr_start) {
        // If we crash here do to an access protection issue, but the
        // address crashes w/ an access error, it's been mprotected to
        // PRTC_NONE, which should only be for a full read guard.
        //
        // Add after the protected region the N00B_GC_PAGEJUMP sentinel.
        if (*(uint64_t *)p == n00b_gc_guard) {
            break;
        }

        if (*(uint64_t *)p == N00B_GC_PAGEJUMP) {
            // We added a page plus a two word sentinel.
            p -= (n00b_page_bytes / sizeof(void **));
            p -= 2;
            continue;
        }

        --p;
    }

    return (n00b_alloc_hdr *)p;
}

static n00b_system_finalizer_fn system_finalizer = NULL;

void
n00b_gc_set_system_finalizer(n00b_system_finalizer_fn fn)
{
    system_finalizer = fn;
}

static hatrack_mem_manager_t hatrack_manager = {
    .mallocfn  = n00b_malloc_wrap,
    .zallocfn  = n00b_malloc_wrap,
    .reallocfn = n00b_realloc_wrap,
    .freefn    = n00b_free_wrap,
    .arg       = NULL,
};

void
n00b_initialize_gc(void)
{
    n00b_create_alloc_guards();
    n00b_discover_page_info();
    n00b_setup_heap_info();
    n00b_create_first_heaps();
    hatrack_setmallocfns(&hatrack_manager);
}
