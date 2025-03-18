#define N00B_USE_INTERNAL_API
#include "n00b.h"

static _Atomic(uint64_t) heap_creation_lock        = 0;
bool                     n00b_collection_suspended = false;

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
    result->last_issued = result->addr_end;

    n00b_assert(result->addr_start < result->addr_end);

    if (h->newest_arena) {
        n00b_crit_t crit = atomic_read(&h->ptr);

        n00b_unlock_arena_header(h->newest_arena);
        h->newest_arena->successor   = result;
        h->newest_arena->last_issued = crit.next_alloc;
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

    if (h->expand) {
        byte_len <<= 1;
    }

    n00b_arena_t *a = n00b_new_arena_pre_aligned(h, byte_len);

    if (!h->first_arena) {
        h->first_arena = a;
    }
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
    void *result
        = _n00b_heap_alloc(NULL,
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

void *
_n00b_heap_alloc(n00b_heap_t *h,
                 size_t       request_len,
                 bool         add_guard,
                 n00b_mem_scan_fn scan
                     N00B_ALLOC_XTRA)
{
    h = n00b_current_heap(h);

    n00b_crit_t   prev_entry;
    n00b_crit_t   new_entry;
    int64_t       alloc_len = n00b_calculate_alloc_len(request_len);
    int64_t       guarded_alloc_len;
    n00b_arena_t *newest_arena;

#if defined(N00B_ADD_ALLOC_LOC_INFO)
    assert(file);
#endif

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
            if (n00b_collection_suspended || __n00b_collector_running) {
                goto emergency_arena;
            }

            n00b_gts_checkin();
            continue;
        }
        // Try to reserve things.

#if defined(N00B_MPROTECT_GUARD_ALLOCS)
        if (add_guard) {
            uint64_t end = ((uint64_t)prev_entry.next_alloc) + alloc_len;
            // Make 'end' end on a page boundry.
            end          = n00b_round_up_to_given_power_of_2(n00b_page_bytes,
                                                    end);

            guarded_alloc_len = end - (uint64_t)prev_entry.next_alloc;
            // 2 int64_ts to maintain proper alignment.
            guarded_alloc_len += n00b_page_bytes + 2 * sizeof(int64_t);
        }
        else {
            guarded_alloc_len = alloc_len;
        }
#else
        guarded_alloc_len = alloc_len;
#endif
        new_entry.next_alloc = (void *)((char *)prev_entry.next_alloc)
                             + guarded_alloc_len;
        new_entry.thread = n00b_thread_self();

        if (!h->pinned && !__n00b_collector_running
            && !n00b_collection_suspended
            && ((void *)new_entry.next_alloc) > newest_arena->addr_end) {
            h->expand = true;
            n00b_heap_collect(h, alloc_len);
            continue;
        }

        if (CAS(&h->ptr, &prev_entry, new_entry)) {
            int64_t esize;
            if (((void *)new_entry.next_alloc) >= newest_arena->addr_end) {
                // We won, but we lost because there's not enough
                // room left in the current arena.
                if (n00b_heap_is_pinned(h) || __n00b_collector_running
                    || n00b_collection_suspended) {
emergency_arena:
                    esize = newest_arena->addr_end - newest_arena->addr_start;
                    esize *= 8;
                    esize = n00b_max(guarded_alloc_len, esize);
                    n00b_add_arena(h,
                                   n00b_max(guarded_alloc_len,
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
    hdr->alloc_len = guarded_alloc_len;

    if (scan != N00B_GC_SCAN_NONE) {
        hdr->n00b_ptr_scan = true;
    }

#ifdef N00B_MPROTECT_GUARD_ALLOCS
    if (add_guard) {
        char *end             = ((char *)hdr) + hdr->alloc_len;
        char *guard_loc       = end - n00b_page_bytes - 2 * sizeof(int64_t);
        *(int64_t *)guard_loc = N00B_NOSCAN;

        if (mprotect(guard_loc, n00b_page_bytes, PROT_READ)) {
            fprintf(stderr, "mprotect failed??\n");
            abort();
        }
        uint64_t *sentinel_loc = ((uint64_t *)end) - 1;
        *sentinel_loc          = N00B_GC_PAGEJUMP;
    }

#endif

#if defined(N00B_ADD_ALLOC_LOC_INFO)
    hdr->alloc_file = file;
    hdr->alloc_line = line;

#if defined(N00B_ENABLE_ALLOC_DEBUG)
    if (n00b_get_tsi_ptr()->show_alloc_locations && !__n00b_collector_running) {
        fprintf(stderr,
                "heap #%lld '%s' @%p @%p  (%s:%d)alloc of length %u\n",
                h->heap_id,
                h->name,
                h,
                hdr,
                file,
                line,
                hdr->alloc_len);
    }
#endif
#endif

#if defined(N00B_FLOG_DEBUG)
    fprintf(n00b_get_tsi_ptr()->flog_file,
            "%p (hdr) %p (user): %d bytes (%s:%d)\n",
            hdr,
            hdr->data,
            hdr->alloc_len,
            hdr->alloc_file,
            hdr->alloc_line);
#endif

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
