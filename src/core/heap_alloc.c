#define N00B_USE_INTERNAL_API
#include "n00b.h"

static _Atomic(uint64_t) heap_creation_lock        = 0;
static _Atomic bool      n00b_collection_suspended = false;

void
n00b_allow_collections(void)
{
    n00b_collection_suspended = false;
}

void
n00b_suspend_collections(void)
{
    n00b_collection_suspended = true;
}

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
    if (h->newest_arena) {
        n00b_crit_t crit = atomic_read(&h->ptr);

        n00b_unlock_arena_header(h->newest_arena);
        h->newest_arena->last_issued = crit.next_alloc;
        n00b_lock_arena_header(h->newest_arena);
    }

    n00b_run_memcheck(h);

    n00b_heap_creation_lock_acquire();
    // Delete all arenas, but leave the heap intact.
    n00b_arena_t *a = h->first_arena;
    n00b_arena_t *next;

    while (a->successor) {
        next = a->successor;
        a    = next;
    }

    h->first_arena  = NULL;
    h->newest_arena = NULL;
    h->released     = false;

    n00b_heap_creation_lock_release();

    while (a->successor) {
        next = a->successor;
        n00b_delete_arena(a);
        a = next;
    }
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

    n00b_stop_the_world();

    uint64_t total_len  = byte_len + N00B_ARENA_OVERHEAD;
    char    *true_start = mmap(NULL,
                            total_len,
                            PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANON,
                            -1,
                            0);

    if (true_start == MAP_FAILED) {
        n00b_dlog_alloc("Out of memory.");
        fprintf(stderr, "Out of memory.");
        abort();
    }

    n00b_arena_t *result = n00b_raw_alloc_to_arena_addr(true_start);

    result->user_length = byte_len;
    result->addr_start  = n00b_arena_user_data_start(result);
    result->addr_end    = n00b_arena_rear_guard_start(result);
    result->last_issued = result->addr_end;

    n00b_dlog_alloc("New arena for heap %d (heap @%p): %p-%p @%p",
                    h->heap_id,
                    h,
                    result->addr_start,
                    result->addr_end,
                    result);

    n00b_assert(result->addr_start < result->addr_end);

    if (!h->first_arena) {
        h->first_arena = result;
    }
    if (h->newest_arena) {
        n00b_crit_t   crit = atomic_read(&h->ptr);
        n00b_arena_t *last = atomic_read(&h->newest_arena);

        n00b_unlock_arena_header(last);
        last->successor   = result;
        last->last_issued = crit.next_alloc;
        n00b_lock_arena_header(last);
        atomic_store(&h->newest_arena, result);
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
    n00b_restart_the_world();

    return result;
}

void
n00b_add_arena(n00b_heap_t *h, uint64_t byte_len)
{
    if (byte_len & n00b_page_modulus) {
        byte_len = (byte_len & n00b_modulus_mask) + n00b_page_bytes;
    }

    if (h->expand) {
        byte_len <<= 1;
    }

    n00b_new_arena_pre_aligned(h, byte_len);
}

// The wrappers always use the global arena.
//
// The first set of wrappers is used by hatrack, so conforms to its
// API, even though we ignore many of the parameters.
//
// The mprotect compile-time option here is to help isolate issues
// given we can't easily capture source info in the second set of
// wrappers (which relies on this first set).
#if !defined(HATRACK_ALLOC_PASS_LOCATION)
#error
#endif

#if defined(N00B_MPROTECT_WRAPPED_ALLOCS)
bool n00b_protect_wrapped_allocs    = false;
bool n00b_protect_wrapping_auto_off = false;

void *
n00b_malloc_wrap(size_t size, void *arg, char *file, int line)
{
#if defined(N00B_ADD_ALLOC_LOC_INFO)
    assert(file);
#endif

    void *result = _n00b_heap_alloc(NULL,
                                    size,
                                    n00b_protect_wrapped_allocs,
                                    arg
                                        N00B_ALLOC_XPARAM);

    if (n00b_protect_wrapping_auto_off) {
        n00b_protect_wrapped_allocs = false;
    }
    return result;
}
#else
void *
n00b_malloc_wrap(size_t size, void *arg, char *file, int line)
{
#if defined(N00B_ADD_ALLOC_LOC_INFO)
    assert(file);
#endif
    void *result = _n00b_heap_alloc(NULL,
                                    size,
                                    false,
                                    arg
                                        N00B_ALLOC_XPARAM);

    return result;
}
#endif

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
#if defined(N00B_ADD_ALLOC_LOC_INFO)
    assert(file);
#endif

    if (p == NULL) {
        return n00b_malloc_wrap(len, NULL N00B_ALLOC_XPARAM);
    }

    n00b_alloc_hdr *hdr = n00b_object_header(p);
    n00b_assert(hdr->guard == n00b_gc_guard);

    if (((int32_t)len) <= hdr->alloc_len) {
        return p;
    }

    void *result = n00b_malloc_wrap(len, arg, file, line);
    memcpy(result, p, hdr->alloc_len);

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
    // TODO-- finish this
}

static bool
need_space(n00b_arena_t *arena, n00b_crit_t entry, int64_t len)
{
    return ((void *)entry.next_alloc) < arena->addr_start
        || ((char *)entry.next_alloc) + len >= (char *)arena->addr_end;
}

void *
_n00b_heap_alloc(n00b_heap_t *h,
                 size_t       request_len,
                 bool         add_guard,
                 n00b_mem_scan_fn scan
                     N00B_ALLOC_XTRA)
{
    h = n00b_current_heap(h);

    n00b_crit_t prev_entry;
    n00b_crit_t new_entry;
    int64_t     alloc_len = n00b_calculate_alloc_len(request_len);
    bool        expand    = false;

#ifdef N00B_FIND_SCRIBBLES
    alloc_len = n00b_round_up_to_given_power_of_2(n00b_page_bytes, alloc_len);
#endif
#if defined(N00B_ADD_ALLOC_LOC_INFO)
    assert(file);
#endif
    while (true) {
        n00b_thread_checkin();
        prev_entry = atomic_read(&h->ptr);

        // If there's not enough space, don't even try.
        if (need_space(h->newest_arena, prev_entry, alloc_len)) {
            n00b_dlog_alloc1(
                "Not enough memory in arena %p for "
                "%d byte alloc (user request: %d (not including hdr); available: %d)",
                h->newest_arena,
                alloc_len,
                request_len,
		((char *)h->newest_arena->addr_end) - (char *)prev_entry.next_alloc);
            if (!need_space(h->newest_arena, prev_entry, alloc_len)) {
                continue;
            }

            if (n00b_collection_suspended) {
                size_t esize = (h->newest_arena->addr_end - h->newest_arena->addr_start) << 3;
                esize        = n00b_max(alloc_len << 3, esize);
                n00b_add_arena(h, esize);
            }
            else {
                h->expand = expand;

                n00b_heap_collect(h, alloc_len);
                h      = n00b_current_heap(h);
                expand = true;
            }
        }

        // Here, we know there's enough space, IF we win the race.
        new_entry.next_alloc = (void *)((char *)prev_entry.next_alloc)
                             + alloc_len;
        new_entry.thread = n00b_thread_self();

        if (CAS(&h->ptr, &prev_entry, new_entry)) {
            break;
        }
        // Otherwise, we lost, and need to check again that there's
        // enough space to try again.
    }

    atomic_fetch_add(&h->alloc_count, 1);

    n00b_alloc_hdr *hdr = prev_entry.next_alloc;

#ifdef N00B_FIND_SCRIBBLES

    n00b_dlog_alloc("scribble detector: unlock %d bytes (%p:%p) for %s:%d",
                    alloc_len,
                    hdr,
                    hdr + alloc_len,
                    file,
                    line);

    assert(!mprotect(hdr, alloc_len, PROT_READ | PROT_WRITE));

#endif
    char *p    = (void *)hdr;
    char *pend = p + alloc_len;

    memset(p, 0, pend - p);

    hdr->guard     = n00b_gc_guard;
    hdr->alloc_len = alloc_len;

    if (scan != N00B_GC_SCAN_NONE) {
        hdr->n00b_ptr_scan = true;
    }

#if defined(N00B_ADD_ALLOC_LOC_INFO)
    hdr->alloc_file = file;
    hdr->alloc_line = line;
#endif

    n00b_dlog_alloc1("heap #%lld @%p (%s:%d)alloc of length %u",
                     h->heap_id,
                     h,
                     file,
                     line,
                     alloc_len);

    return hdr->data;
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

    n00b_dlog_alloc1("%s:%d: New heap @%p: id = %d; len = %d",
                     result->file,
                     result->line,
                     result,
                     result->heap_id,
                     byte_len);
    return result;
}

void
n00b_delete_heap(n00b_heap_t *h)
{
    n00b_dlog_alloc1("Deleting heap id = %d @%p",
                     h->heap_id,
                     h);

    n00b_heap_clear(h);
    bzero(((char *)h) + sizeof(int64_t),
          sizeof(n00b_heap_t) - sizeof(int64_t));
    h->released = true;
}
