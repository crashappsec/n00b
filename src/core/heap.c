// I keep rewriting this, but here the goal has been to make it
// simpler, ensuring it's MT-safe now that every runtime is implicitly
// using threads for I/O, and removing fields we don't need, since we
// are now marshaling into the exact same memory structure, and would
// like it to be more compact.
//
// Additionally, we've added an option for heap pinning (even while
// doing unbound allocations), and might (at some point) add the
// ability to make non-pinned heaps mark-and-sweep (they are copying,
// which has plenty of advantages, but occasionally mark-and-sweep is
// more appropriate).
//
// To that end, I've temporarily removed some things:
//
// 1. Finalization (object clean-up routines for objects that
//    *weren't* collected. This will come back soon.
//
// 2. The ability to provide pointer maps for allocations.  First,
//    building the and maintaining the pointer maps is a huge pain
//    (leading me to just set everything to 'scan all'), and second,
//    they take up too much space per allocation right now. I can
//    definitely solve problem #2, but would like to make real
//    progress on problem #1.
//
//    However, I've still left in the ability to say, "don't scan this
//    allocation for pointers". And, I will NEED to add back in
//    something more nuanced, because without it, false positives will
//    be possible, where data gets munged on a collection (which can
//    even be a security concern).
//
//    Realistically, once I add user-definable objects, this problem
//    is easy to solve for managed objects, given the type
//    system. However, the more practical issue is all the internal
//    allocations that aren't mapped to an external type, since
//    they're meant to be internal state.
//
//    For the time being, the 64-bit field to hold a callback to get a
//    pointer map remains. It could end up compressing down to a
//    single bit, but I don't think it will, which is why I'm not
//    taking the field out, even though I want to add pointer mapping
//    back in after adding a few more language-level features.
//
// 3. Most GC metric calculation and reporting (just wanted to
//    declutter and haven't been needing it, but will probably add it
//    back in).
//
// 4. The most draconian memory checks, including ASAN integration. It
//    takes up space, and hasn't been tremendously useful in a long
//    time (it was almost doubling the size of the collector's code
//    with no practical benefit after the first few weeks). There's
//    still the option to leave room for an end-guard, but I'm not
//    even checking it right now. I will add the check back in, but to
//    be seen if I feel the need to add back in the more intricate
//    capabilities.

//
// Note that it would be reasonably straightforward here to do
// cooperative collections, where threads divide up the roots to
// trace, and the migrations to do.  The get_relocation_addr() call
// would just need to migrate to a compare-and-swap, and the ordering
// of the hash table insertion would need to change to before that's
// done. And we'd switch to crown_add() to handle contention for the
// same allocation record.

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
uint64_t              n00b_gc_guard      = 0;
// This value is also chosen randomly, and is used to identify the end
// of memory records. Generally this should be back-to-back with the
// normal guard.
uint64_t              n00b_end_guard     = 0;
// The global heap.
n00b_heap_t          *n00b_default_heap  = NULL;
// This is the current heap for a given thread. If it's null, use the
// default heap.
n00b_heap_t          *n00b_internal_heap = NULL;
__thread n00b_heap_t *n00b_thread_heap   = NULL;
// The heap we're migrating into. We only allocate this once; when
// we're done, we move around the destination arena and leave the
// heap reference around to avoid needless heap entries.
// The scratch heap is used during GC.
n00b_heap_t          *n00b_all_heaps     = NULL;
n00b_heap_t          *n00b_cur_heap_page;
uint64_t              n00b_next_heap_index = 0;
uint64_t              n00b_page_bytes;
uint64_t              n00b_page_modulus;
uint64_t              n00b_modulus_mask;
// Heap entries per page, capculated once.
int                   n00b_heap_entries_pp;

static _Atomic(uint64_t) heap_creation_lock = 0;

uint64_t
n00b_get_page_size(void)
{
    return n00b_page_bytes;
}

void
n00b_heap_creation_lock_acquire(void)
{
    uint64_t expected;
    uint64_t desired = n00b_rand64();

    do {
        expected = 0;
    } while (!CAS(&heap_creation_lock, &expected, desired));
}

static inline void
n00b_heap_creation_lock_release(void)
{
    atomic_store(&heap_creation_lock, 0);
}

void
n00b_heap_clear(n00b_heap_t *h)
{
    n00b_heap_creation_lock_acquire();
    // Delete all arenas, but leave the heap intact.
    n00b_arena_t *a = h->first_arena;
    n00b_arena_t *next;

    h->first_arena  = NULL;
    h->newest_arena = NULL;
    h->to_finalize  = NULL;
    h->released     = false;

    n00b_heap_creation_lock_release();

    while (a) {
        next = a->successor;
        n00b_delete_arena(a);
        a = next;
    }
}

void
n00b_heap_prune(n00b_heap_t *h)
{
    n00b_heap_creation_lock_acquire();
    // Delete all arenas, but leave the heap intact.
    n00b_arena_t *a = h->first_arena;
    n00b_arena_t *next;

    while (a->successor) {
        next = a->successor;
        a    = next;
    }

    h->first_arena  = a;
    h->newest_arena = a;
    h->released     = false;

    n00b_heap_creation_lock_release();

    while (a->successor) {
        next = a->successor;
        n00b_delete_arena(a);
        a = next;
    }
}

n00b_heap_t *
n00b_addr_find_heap(void *p)
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

        if (n00b_addr_in_one_heap(h, p)) {
            return h;
        }

        h++;
    }

    return NULL;
}

static inline void
n00b_add_heap_entry_page(void)
{
    // You should have the lock to call this (or call from init).
    n00b_heap_t *new_page = n00b_unprotected_mempage();

    if (!n00b_all_heaps) {
        n00b_all_heaps = new_page;
    }
    else {
        void *lloc = (n00b_heap_t *)&n00b_cur_heap_page[n00b_heap_entries_pp];

        *(n00b_heap_t **)lloc = new_page;
    }

    n00b_cur_heap_page = new_page;
}

static inline n00b_arena_t *
n00b_new_arena_pre_aligned(n00b_heap_t *h, uint64_t byte_len)
{
    // This assumes that byte_len is already a multiple of the page
    // size. It returns a pointer TWO PAGES past where it
    // allocates. The first page can contain debug information, but
    // also contains the heap ID and other info used to manage the
    // heap (n00b_arena_t *).
    //
    // The second page is a guard page.
    // The final page of the heap is also a guard page.

    uint64_t      total_len  = byte_len + N00B_ARENA_OVERHEAD;
    char         *true_start = mmap(NULL,
                            total_len,
                            PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANON,
                            -1,
                            0);
    n00b_arena_t *result     = n00b_raw_alloc_to_arena_addr(true_start);

    if (result == MAP_FAILED) {
        n00b_raise_errno();
    }

    result->user_length = byte_len;
    result->addr_start  = n00b_arena_user_data_start(result);
    result->addr_end    = n00b_arena_rear_guard_start(result);

    n00b_assert(result->addr_start < result->addr_end);

    if (h->newest_arena) {
        n00b_unlock_arena_header(h->newest_arena);
        h->newest_arena->successor = result;
        n00b_lock_arena_header(h->newest_arena);
    }

    n00b_crit_t info = {
        .next_alloc = result->addr_start,
        .thread     = NULL,
    };

    h->newest_arena  = result;
    h->cur_arena_end = result->addr_end;
    atomic_store(&h->ptr, info);

    void *separator_guard = n00b_arena_front_guard_start(result);
    void *end_guard       = result->addr_end;

    mprotect(true_start, n00b_page_bytes, PROT_NONE);
    mprotect(separator_guard, n00b_page_bytes, PROT_NONE);
    mprotect(end_guard, n00b_page_bytes, PROT_NONE);

    n00b_lock_arena_header(result);

    return result;
}

void
n00b_add_arena(n00b_heap_t *h, uint64_t byte_len)
{
    if (byte_len & n00b_page_modulus) {
        byte_len = (byte_len & n00b_modulus_mask) + n00b_page_bytes;
    }

    n00b_arena_t *a = n00b_new_arena_pre_aligned(h, byte_len);

    if (!h->first_arena) {
        h->first_arena = a;
    }
}

n00b_heap_t *
_n00b_new_heap(uint64_t byte_len, char *file, int line)
{
    n00b_heap_creation_lock_acquire();

    n00b_heap_t *result;
    n00b_heap_t *h = n00b_all_heaps;

    for (uint64_t i = 0; i < n00b_next_heap_index; i++) {
        if (i && !(i % n00b_heap_entries_pp)) {
            h = *(n00b_heap_t **)h;
            continue;
        }

        if (h->released) {
            h->released = false;
            h->file     = file;
            h->line     = line;
            result      = h;

            goto finish_setup;
        }

        h++;
    }

    int64_t heap_id     = n00b_next_heap_index++;
    int     entry_index = heap_id % n00b_heap_entries_pp;

    if (!entry_index) {
        n00b_add_heap_entry_page();
    }

    result               = n00b_cur_heap_page + entry_index;
    result->heap_id      = heap_id;
    result->alloc_count  = 0;
    result->first_arena  = NULL;
    result->newest_arena = NULL;
    result->roots        = NULL;
    result->to_finalize  = NULL;
    result->pinned       = false;
    result->released     = false;
    result->file         = file;
    result->line         = line;

    if (!heap_id) {
        n00b_default_heap = result;
    }

finish_setup:
    if (byte_len) {
        n00b_add_arena(result, byte_len);
    }

    n00b_heap_creation_lock_release();

    return result;
}

void
n00b_delete_heap(n00b_heap_t *h)
{
    n00b_heap_clear(h);
    bzero(((char *)h) + sizeof(int64_t),
          sizeof(n00b_heap_t) - sizeof(int64_t));
    h->released = true;
}

// The wrappers always use the global arena.
//
// The first set of wrappers is used by hatrack, so conforms to its
// API, even though we ignore many of the parameters.
#if !defined(HATRACK_ALLOC_PASS_LOCATION)
#error
#endif

void *
n00b_malloc_wrap(size_t size, void *arg, char *file, int line)
{
    void *result = _n00b_heap_alloc(NULL,
                                    size,
                                    arg
                                        N00B_ALLOC_XPARAM);

    return result;
}

void
n00b_free_wrap(void *p, size_t sz, void *arg, char *file, int line)
{
    bzero(p, sz);
}

void *
n00b_realloc_wrap(void  *p,
                  size_t ig,
                  size_t len,
                  void  *arg,
                  char  *file,
                  int    line)
{
    if (p == NULL) {
        return n00b_malloc_wrap(len, NULL N00B_ALLOC_XPARAM);
    }

    n00b_assert(n00b_in_heap(p));

    n00b_alloc_hdr *hdr = n00b_object_header(p);
    n00b_assert(hdr->guard == n00b_gc_guard);

    if (len + N00B_EXTRA_MEMCHECK_BYTES <= hdr->alloc_len) {
        return p;
    }

    void *result = n00b_malloc_wrap(len, arg, file, line);
    memcpy(result, p, hdr->alloc_len - N00B_EXTRA_MEMCHECK_BYTES);

    return result;
}

// Compat versions only ever have one argument, and don't track
// allocation location (it'll point here).  These are only necessary
// when we have N00B_ADD_ALLOC_LOC_INFO on; otherwise they get
// #defined to forward to the wrap versions.

void *
n00b_malloc_compat(size_t sz)
{
    return n00b_malloc_wrap(sz, NULL, __FILE__, __LINE__);
}

void *
n00b_realloc_compat(void *old, size_t len)
{
    return n00b_realloc_wrap(old, len, len, NULL, __FILE__, __LINE__);
}

void
n00b_free_compat(void *p)
{
}

static inline bool
n00b_heap_is_pinned(n00b_heap_t *h)
{
    return h->pinned || __n00b_collector_running;
}

void *
_n00b_heap_alloc(n00b_heap_t *h,
                 size_t       request_len,
                 n00b_mem_scan_fn scan
                     N00B_ALLOC_XTRA)
{
    h = n00b_get_heap(h);

    n00b_crit_t   prev_entry;
    n00b_crit_t   new_entry;
    int64_t       alloc_len = n00b_calculate_alloc_len(request_len);
    n00b_arena_t *newest_arena;

    // #if defined(N00B_DEBUG) && defined(N00B_ADD_ALLOC_LOC_INFO)
    //     _n00b_watch_scan(file, line);
    // #endif

    // Since other heaps than ours need to collect too, always
    // check in at least once.
    n00b_gts_checkin();

    while (true) {
        newest_arena = h->newest_arena;
        prev_entry   = atomic_read(&h->ptr);

        if (((void *)prev_entry.next_alloc) < newest_arena->addr_start
            || ((void *)prev_entry.next_alloc) >= newest_arena->addr_end) {
            // Someone should be installing a new arena (if h->pinned)
            // or collecting otherwise.
            if (__n00b_collector_running) {
                goto emergency_arena;
            }

            n00b_gts_checkin();
            continue;
        }
        // Try to reserve things.

        new_entry.next_alloc = (void *)((char *)prev_entry.next_alloc)
                             + alloc_len;
        new_entry.thread = n00b_thread_self();

        if (CAS(&h->ptr, &prev_entry, new_entry)) {
            if (((void *)new_entry.next_alloc) >= newest_arena->addr_end) {
                // We won, but we lost because there's not enough
                // room left in the current arena.
                if (n00b_heap_is_pinned(h)) {
emergency_arena:
                    n00b_add_arena(h,
                                   n00b_max(alloc_len,
                                            N00B_START_SCRATCH_HEAP_SIZE));
                    continue;
                }
                else {
                    n00b_heap_collect(h, alloc_len);
                }
                continue;
            }

            break;
        }

        n00b_gts_checkin();
    }

    atomic_fetch_add(&h->alloc_count, 1);

    n00b_alloc_hdr *hdr = prev_entry.next_alloc;

    hdr->guard     = n00b_gc_guard;
    hdr->alloc_len = alloc_len;

    if (scan != N00B_GC_SCAN_NONE) {
        hdr->n00b_ptr_scan = true;
    }

#if defined(N00B_ADD_ALLOC_LOC_INFO)
    hdr->alloc_file = file;
    hdr->alloc_line = line;
#endif

#if defined(N00B_FULL_MEMCHECK)
    uint64_t *guard_loc = ((uint64_t *)(((char *)hdr) + hdr->alloc_len)) - 1;
    *guard_loc          = n00b_end_guard;
#endif

#if defined(N00B_WARN_ON_ZERO_ALLOCS)
    if (orig_len == 0) {
        fprintf(stderr,
                "Memcheck zero-byte alloc from %s:%d (record @%p)\n",
                file,
                line,
                raw);
    }
#endif

    return hdr->data;
}
