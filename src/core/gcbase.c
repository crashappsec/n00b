#include "n00b.h"

uint64_t     n00b_gc_guard     = 0;
n00b_arena_t *n00b_current_heap = NULL;
uint64_t     n00b_page_bytes;
uint64_t     n00b_page_modulus;
uint64_t     n00b_modulus_mask;

_Atomic(n00b_segment_range_t *) n00b_static_segments = NULL;

#ifdef N00B_GC_STATS
thread_local uint64_t n00b_total_requested = 0;
thread_local uint64_t n00b_total_alloced   = 0;
thread_local uint32_t n00b_total_allocs    = 0;
#endif

uint64_t n00b_end_guard;

#ifdef N00B_USE_RING
static thread_local n00b_shadow_alloc_t *memcheck_ring[N00B_MEMCHECK_RING_SZ] = {
    0,
};
static thread_local unsigned int ring_head = 0;
static thread_local unsigned int ring_tail = 0;
#endif

static n00b_set_t    *external_holds = NULL;
static pthread_key_t n00b_thread_key;

struct n00b_pthread {
    size_t size;
    char   data[];
};

bool
n00b_in_heap(void *p)
{
    return p >= ((void *)n00b_current_heap->data) && p < (void *)n00b_current_heap->heap_end;
}

void
n00b_get_heap_bounds(uint64_t *start, uint64_t *next, uint64_t *end)
{
    *start = (uint64_t)n00b_current_heap;
    *next  = (uint64_t)n00b_current_heap->next_alloc;
    *end   = (uint64_t)n00b_current_heap->heap_end;
}

void
n00b_gc_heap_stats(uint64_t *used, uint64_t *available, uint64_t *total)
{
    uint64_t start = (uint64_t)n00b_current_heap;
    uint64_t end   = (uint64_t)n00b_current_heap->heap_end;
    uint64_t cur   = (uint64_t)n00b_current_heap->next_alloc;

    if (used != NULL) {
        *used = cur - start;
    }

    if (available != NULL) {
        *available = end - cur;
    }

    if (total != NULL) {
        *total = end - start;
    }
}

#ifdef N00B_ADD_ALLOC_LOC_INFO
void *
n00b_gc_malloc_wrapper(size_t size, void *arg, char *file, int line)
{
    return _n00b_gc_raw_alloc(size, arg, file, line);
}

static void
n00b_gc_free_wrapper(void *oldptr, size_t size, void *arg, char *file, int line)
{
    // do nothing; memory is garbage collected
}

static void *
n00b_gc_realloc_wrapper(void  *oldptr,
                       size_t oldsize,
                       size_t newsize,
                       void  *arg,
                       char  *file,
                       int    line)
{
    return n00b_gc_resize(oldptr, newsize);
}

#else
static void *
n00b_gc_malloc_wrapper(size_t size, void *arg)
{
    // Hatrack wants a 16-byte aligned pointer. The n00b gc allocator will
    // always produce a 16-byte aligned pointer. The raw allocation header is
    // 48 bytes and its base pointer is always 16-byte aligned.
    return n00b_gc_raw_alloc(size * 2, arg);
}

static void
n00b_gc_free_wrapper(void *oldptr, size_t size, void *arg)
{
    // do nothing; memory is garbage collected
}

static void *
n00b_gc_realloc_wrapper(void *oldptr, size_t oldsize, size_t newsize, void *arg)
{
    return n00b_gc_resize(oldptr, newsize);
}

#endif

static void
n00b_thread_release_pthread(void *arg)
{
    pthread_setspecific(n00b_thread_key, NULL);

    struct n00b_pthread *pt = arg;
    mmm_thread_release((mmm_thread_t *)pt->data);
}

static void
n00b_thread_acquire_init_pthread(void)
{
    pthread_key_create(&n00b_thread_key, n00b_thread_release_pthread);
}

static mmm_thread_t *
n00b_thread_acquire(void *aux, size_t size)
{
    static pthread_once_t init = PTHREAD_ONCE_INIT;
    pthread_once(&init, n00b_thread_acquire_init_pthread);

    struct n00b_pthread *pt = pthread_getspecific(n00b_thread_key);
    if (NULL == pt) {
        int len  = sizeof(struct n00b_pthread) + size;
        pt       = calloc(1, len);
        pt->size = size;
        pthread_setspecific(n00b_thread_key, pt);
        mmm_thread_t *r = (mmm_thread_t *)pt->data;
        n00b_gc_register_root(&(r->retire_list), 1);
        return r;

#ifdef N00B_USE_RING
        for (int i = 0; i < N00B_MEMCHECK_RING_SZ; i++) {
            memcheck_ring[i] = NULL;
        }
#endif
    }

    return (mmm_thread_t *)pt->data;
}

void
n00b_initialize_gc()
{
    static bool once = false;

    if (!once) {
        hatrack_zarray_t     *initial_roots;
        hatrack_mem_manager_t hatrack_manager = {
            .mallocfn  = n00b_gc_malloc_wrapper,
            .zallocfn  = n00b_gc_malloc_wrapper,
            .reallocfn = n00b_gc_realloc_wrapper,
            .freefn    = n00b_gc_free_wrapper,
            .arg       = NULL,
        };

#ifdef N00B_FULL_MEMCHECK
        n00b_end_guard = n00b_rand64();
#endif
        n00b_gc_guard     = n00b_rand64();
        initial_roots    = hatrack_zarray_new(N00B_MAX_GC_ROOTS,
                                           sizeof(n00b_gc_root_info_t));
        external_holds   = n00b_rc_alloc(sizeof(n00b_set_t));
        once             = true;
        n00b_page_bytes   = getpagesize();
        n00b_page_modulus = n00b_page_bytes - 1; // Page size is always a power of 2.
        n00b_modulus_mask = ~n00b_page_modulus;

        int initial_len  = N00B_DEFAULT_ARENA_SIZE;
        n00b_current_heap = n00b_new_arena(initial_len, initial_roots);
        n00b_arena_register_root(n00b_current_heap, &external_holds, 1);

        mmm_setthreadfns(n00b_thread_acquire, NULL);
        hatrack_setmallocfns(&hatrack_manager);
        n00b_gc_trace(N00B_GCT_INIT, "init:set_guard:%llx", n00b_gc_guard);

        hatrack_set_init(external_holds, HATRACK_DICT_KEY_TYPE_PTR);
    }
}

void
n00b_add_static_segment(void *start, void *end)
{
    // Keep this out of the world of the GC, since it's static.
    n00b_segment_range_t *range    = malloc(sizeof(n00b_segment_range_t));
    n00b_segment_range_t *expected = atomic_load(&n00b_static_segments);

    range->start = start;
    range->end   = end;

    do {
        if (expected) {
            range->segment_id = expected->segment_id + 1;
        }
        else {
            range->segment_id = 0;
        }

        range->next = expected;
    } while (!CAS(&n00b_static_segments, &expected, range));
}

void
n00b_gc_add_hold(n00b_obj_t obj)
{
    hatrack_set_add(external_holds, obj);
}

void
n00b_gc_remove_hold(n00b_obj_t obj)
{
    hatrack_set_remove(external_holds, obj);
}

// The idea here is once the object unmarshals the object file and
// const objects, it can make the heap up till that point read-only.
// We definitely won't want to allocate anything that will need
// to be writable at runtime...
static void
lock_existing_heap()
{
    uint64_t to_lock       = (uint64_t)n00b_current_heap;
    uint64_t words_to_lock = ((uint64_t)(n00b_current_heap->next_alloc)) - to_lock;
    int      b_to_lock     = words_to_lock * 8;
    b_to_lock              = n00b_round_up_to_given_power_of_2(getpagesize(),
                                                 b_to_lock);

// This doesn't seem to be working on mac; disallows reads.
#ifdef __linux__
//    mprotect((void *)to_lock, b_to_lock, PROT_READ);
#endif
}

static thread_local n00b_arena_t *stashed_heap;

n00b_arena_t *
n00b_internal_stash_heap()
{
    // This assumes the stashed heap isn't going to be used for allocations
    // until it's returneed.
    stashed_heap     = n00b_current_heap;
    n00b_current_heap = n00b_new_arena(
        N00B_DEFAULT_ARENA_SIZE,
        hatrack_zarray_unsafe_copy(n00b_current_heap->roots));

    uint64_t *s = (uint64_t *)stashed_heap;
    uint64_t *e = (uint64_t *)stashed_heap->next_alloc;

    n00b_arena_register_root(n00b_current_heap, stashed_heap, e - s);

    return stashed_heap;
}

void
n00b_internal_lock_then_unstash_heap()
{
    lock_existing_heap();
    n00b_current_heap = stashed_heap;
}

void
n00b_internal_unstash_heap()
{
    n00b_arena_t *popping = n00b_current_heap;

    n00b_arena_remove_root(popping, stashed_heap);
    n00b_current_heap = stashed_heap;

    uint64_t *s = (uint64_t *)popping;
    uint64_t *e = (uint64_t *)popping->next_alloc;

    n00b_arena_register_root(n00b_current_heap, popping, e - s);
}

void
n00b_internal_set_heap(n00b_arena_t *heap)
{
    n00b_current_heap = heap;
}

void *
n00b_raw_arena_alloc(uint64_t len, void **end, void **accounting)
{
    // Add two guard pages to sandwich the alloc, and a
    // page at the front for accounting.
    size_t total_len = (size_t)(n00b_page_bytes * 3 + len);

    char *full_alloc = mmap(NULL,
                            total_len,
                            PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANON,
                            0,
                            0);

    char *guard1 = full_alloc + n00b_page_bytes;
    char *ret    = guard1 + n00b_page_bytes;
    char *guard2 = full_alloc + total_len - n00b_page_bytes;

    mprotect(guard1, n00b_page_bytes, PROT_NONE);
    mprotect(guard2, n00b_page_bytes, PROT_NONE);

    *end        = guard2;
    *accounting = full_alloc;

    n00b_gc_trace(N00B_GCT_MMAP,
                 "arena:mmap:@%p-@%p:%llu",
                 full_alloc,
                 full_alloc + total_len,
                 len);

    return ret;
}

n00b_arena_t *
n00b_new_arena(size_t num_words, hatrack_zarray_t *roots)
{
    // Convert words to bytes.
    uint64_t allocation = ((uint64_t)num_words) * 8;

    // We're okay to over-allocate here. We round up to the nearest
    // power of 2 that is a multiple of the page size.

    if (allocation & n00b_page_modulus) {
        allocation = (allocation & n00b_modulus_mask) + n00b_page_bytes;
        num_words  = allocation >> 3;
    }

    void        *arena_end;
    void        *aux;
    n00b_arena_t *new_arena = n00b_raw_arena_alloc(allocation, &arena_end, &aux);

    new_arena->next_alloc   = (n00b_alloc_hdr *)new_arena->data;
    new_arena->heap_end     = arena_end;
    new_arena->roots        = roots;
    new_arena->history      = aux;
    new_arena->history->cur = 0;

    pthread_mutex_init(&new_arena->lock, NULL);

    return new_arena;
}

#if defined(N00B_ADD_ALLOC_LOC_INFO)
#define TRACE_DEBUG_ARGS , debug_file, debug_ln

void *
_n00b_gc_raw_alloc(size_t          len,
                  n00b_mem_scan_fn scan_fn,
                  char           *debug_file,
                  int             debug_ln)

#else
#define TRACE_DEBUG_ARGS

void *
_n00b_gc_raw_alloc(size_t len, n00b_mem_scan_fn scan_fn)

#endif
{
    return n00b_alloc_from_arena(&n00b_current_heap,
                                len,
                                scan_fn,
                                false TRACE_DEBUG_ARGS);
}

#if defined(N00B_ADD_ALLOC_LOC_INFO)
void *
_n00b_gc_raw_alloc_with_finalizer(size_t          len,
                                 n00b_mem_scan_fn scan_fn,
                                 char           *debug_file,
                                 int             debug_ln)
#else
void *
_n00b_gc_raw_alloc_with_finalizer(size_t len, n00b_mem_scan_fn scan_fn)
#endif
{
    return n00b_alloc_from_arena(&n00b_current_heap,
                                len,
                                scan_fn,
                                true TRACE_DEBUG_ARGS);
}

void *
n00b_gc_resize(void *ptr, size_t len)
{
    // We'd like external C code to be able to use our GC. Some things
    // (i.e., openssl) will call realloc(NULL, ...) to get memory
    // for whatever reason.
    if (ptr == NULL) {
        return n00b_gc_raw_alloc(len, N00B_GC_SCAN_ALL);
    }
    n00b_alloc_hdr *hdr = &((n00b_alloc_hdr *)ptr)[-1];

    assert(hdr->guard = n00b_gc_guard);

#if defined(N00B_ADD_ALLOC_LOC_INFO)
    char *debug_file = hdr->alloc_file;
    int   debug_ln   = hdr->alloc_line;
#endif

    void *result = n00b_alloc_from_arena(&n00b_current_heap,
                                        len,
                                        hdr->scan_fn,
                                        (bool)hdr->finalize TRACE_DEBUG_ARGS);
    if (len > 0) {
        size_t bytes = ((char *)hdr->next_addr) - (char *)hdr->data;
        memcpy(result, ptr, n00b_min(len, bytes));
    }

    if (hdr->finalize == 1) {
        n00b_alloc_hdr *newhdr = &((n00b_alloc_hdr *)result)[-1];
        newhdr->finalize      = 1;

        n00b_finalizer_info_t *p = n00b_current_heap->to_finalize;

        while (p != NULL) {
            if (p->allocation == hdr) {
                p->allocation = newhdr;
                return result;
            }
            p = p->next;
        }
        n00b_unreachable();
    }

    return result;
}

void
n00b_delete_arena(n00b_arena_t *arena)
{
    // TODO-- allocations need to have an arena pointer or thread id
    // for cross-thread to work.
    //
    // n00b_gc_trace("******** delete late mutations dict: %p\n",
    // arena->late_mutations);
    // free(arena->late_mutations);

    char *start = ((char *)arena) - n00b_page_bytes;
    char *end   = (char *)arena->heap_end + n00b_page_bytes;

    n00b_gc_trace(N00B_GCT_MUNMAP, "arena:delete:%p:%p", start, end);

#if defined(N00B_MADV_ZERO)
    madvise(start, end - start, MADV_ZERO);
    mprotect((void *)start, end - start, PROT_NONE);
#endif
    munmap(start, end - start);

    return;
}

#ifdef N00B_ADD_ALLOC_LOC_INFO
void
_n00b_arena_register_root(n00b_arena_t *arena,
                         void        *ptr,
                         uint64_t     len,
                         char        *file,
                         int          line)
#else
void
_n00b_arena_register_root(n00b_arena_t *arena, void *ptr, uint64_t len)
#endif
{
    // Len is measured in 64 bit words and must be at least 1.
    n00b_gc_root_info_t *ri;
    hatrack_zarray_new_cell(arena->roots, (void *)&ri);
    ri->num_items = len;
    ri->ptr       = ptr;
#ifdef N00B_GC_STATS
    ri->file = file;
    ri->line = line;
#endif
}

void
n00b_arena_remove_root(n00b_arena_t *arena, void *ptr)
{
    int32_t max = atomic_load(&arena->roots->length);

    for (int i = 0; i < max; i++) {
        n00b_gc_root_info_t *ri = hatrack_zarray_cell_address(arena->roots, i);
        if (ri->ptr == ptr) {
            ri->num_items = 0;
            ri->ptr       = NULL;
        }
    }
}
n00b_alloc_hdr *
n00b_find_alloc(void *ptr)
{
    void **p = (void **)(((uint64_t)ptr) & ~0x0000000000000007);

    while (p > (void **)n00b_current_heap) {
        if (*p == (void *)n00b_gc_guard) {
            return (n00b_alloc_hdr *)p;
        }
        p -= 1;
    }

    return NULL;
}

#if defined(N00B_ADD_ALLOC_LOC_INFO)
n00b_utf8_t *
n00b_gc_alloc_info(void *addr, int *line)
{
    if (!n00b_in_heap(addr)) {
        if (line != NULL) {
            *line = 0;
        }
        return NULL;
    }

    n00b_alloc_hdr *h = n00b_find_alloc(addr);

    if (line != NULL) {
        *line = h->alloc_line;
    }

    return n00b_new_utf8(h->alloc_file);
}

void
_n00b_gc_register_root(void *ptr, uint64_t num_words, char *f, int l)
{
    n00b_gc_trace(N00B_GCT_REGISTER,
                 "root_register:@%p-%p (%s:%d)",
                 ptr,
                 ptr + num_words,
                 f,
                 l);
    _n00b_arena_register_root(n00b_current_heap, ptr, num_words, f, l);
}
#else
void
_n00b_gc_register_root(void *ptr, uint64_t num_words)
{
    n00b_arena_register_root(n00b_current_heap, ptr, num_words);
}
#endif

void
n00b_gcm_remove_root(void *ptr)
{
    n00b_arena_remove_root(n00b_current_heap, ptr);
}

#ifdef N00B_FULL_MEMCHECK
static inline void
memcheck_process_ring()
{
    unsigned int cur = ring_tail;

    cur &= ~(N00B_MEMCHECK_RING_SZ - 1);

    while (cur != ring_head) {
        n00b_shadow_alloc_t *a = memcheck_ring[cur++];
        if (!a) {
            return;
        }

        if (a->start->guard != n00b_gc_guard) {
            n00b_alloc_display_front_guard_error(a->start,
                                                a->start->data,
                                                a->file,
                                                a->line,
                                                true);
        }

        if (*a->end != n00b_end_guard) {
            n00b_alloc_display_rear_guard_error(a->start,
                                               a->start->data,
                                               a->len,
                                               a->end,
                                               a->file,
                                               a->line,
                                               true);
        }
    }
}
#endif

#if defined(N00B_ADD_ALLOC_LOC_INFO)
void *
n00b_alloc_from_arena(n00b_arena_t   **arena_ptr,
                     size_t          len,
                     n00b_mem_scan_fn scan_fn,
                     bool            finalize,
                     char           *file,
                     int             line)
#else

// Note that len is measured in WORDS not bytes.
void *
n00b_alloc_from_arena(n00b_arena_t   **arena_ptr,
                     size_t          len,
                     n00b_mem_scan_fn scan_fn,
                     bool            finalize)
#endif
{
    size_t orig_len = len;

    len += sizeof(n00b_alloc_hdr);

#if defined(N00B_DEBUG) && defined(N00B_ADD_ALLOC_LOC_INFO)
    _n00b_watch_scan(file, line);
#endif

#ifdef N00B_FULL_MEMCHECK
    len += 16;
#endif
    n00b_arena_t *arena = *arena_ptr;

    pthread_mutex_lock(&arena->lock);

    // Round up to aligned length.
    len = n00b_round_up_to_given_power_of_2(N00B_FORCED_ALIGNMENT, len);

    n00b_alloc_hdr *raw = arena->next_alloc;
    char          *p   = (char *)raw;
    raw->next_addr     = p + len;

    if (raw->next_addr >= (char *)arena->heap_end) {
        pthread_mutex_unlock(&arena->lock);
        arena      = n00b_collect_arena(arena);
        *arena_ptr = arena;

        pthread_mutex_lock(&arena->lock);
        raw            = (n00b_alloc_hdr *)arena->next_alloc;
        p              = (char *)raw;
        raw->next_addr = p + len;
        if (raw->next_addr > (char *)arena->heap_end) {
            arena->grow_next = true;
            pthread_mutex_unlock(&arena->lock);
#if defined(N00B_ADD_ALLOC_LOC_INFO)
            return n00b_alloc_from_arena(arena_ptr,
                                        len,
                                        scan_fn,
                                        finalize,
                                        file,
                                        line);
#else
            return n00b_alloc_from_arena(arena_ptr, len, scan_fn, finalize);
#endif
        }
    }

    arena->alloc_count++;
    arena->next_alloc = (n00b_alloc_hdr *)raw->next_addr;
    raw->guard        = n00b_gc_guard;
    raw->alloc_len    = len;
    raw->request_len  = orig_len;
    raw->scan_fn      = scan_fn;

#ifdef N00B_FULL_MEMCHECK
    uint64_t *end_guard_addr = ((uint64_t *)raw->next_addr) - 1;

    n00b_shadow_alloc_t *record = n00b_rc_alloc(sizeof(n00b_shadow_alloc_t));
    record->start              = raw;
    record->end                = end_guard_addr;

    record->file = file;
    record->line = line;
    record->len  = orig_len;
    record->next = NULL;
    record->prev = arena->shadow_end;
    *record->end = n00b_end_guard;

#ifdef N00B_WARN_ON_ZERO_ALLOCS
    if (orig_len == 0) {
        fprintf(stderr,
                "Memcheck zero-byte alloc from %s:%d (record @%p)\n",
                file,
                line,
                raw);
    }
#endif
    // Duplicated in the header for spot-checking; this can get corrupted;
    // the out-of-heap list is better, but we don't want to bother searching
    // through the whole heap.
    raw->end_guard_loc = end_guard_addr;

    assert(*raw->end_guard_loc == n00b_end_guard);

    if (arena->shadow_end != NULL) {
        arena->shadow_end->next = record;
        arena->shadow_end       = record;
    }
    else {
        arena->shadow_start = record;
        arena->shadow_end   = record;
    }

#ifdef N00B_USE_RING
    memcheck_process_ring();

    memcheck_ring[ring_head++] = record;
    ring_head &= ~(N00B_MEMCHECK_RING_SZ - 1);
    if (ring_tail == ring_head) {
        ring_tail++;
        ring_tail &= ~(N00B_MEMCHECK_RING_SZ - 1);
    }
#endif // N00B_USE_RING
#endif // N00B_FULL_MEMCHECK

#ifdef N00B_GC_STATS
    n00b_total_requested += orig_len;
    n00b_total_alloced += len;

    raw->alloc_file = file;
    raw->alloc_line = line;

    n00b_gc_trace(N00B_GCT_ALLOC,
                 "new_record:%p-%p:data:%p:len:%zu:arena:%p-%p (%s:%d)",
                 raw,
                 raw->next_addr,
                 raw->data,
                 len,
                 arena,
                 arena->heap_end,
                 raw->alloc_file,
                 raw->alloc_line);

    n00b_total_allocs++;
#endif

    if (finalize) {
        n00b_finalizer_info_t *record = n00b_rc_alloc(sizeof(n00b_finalizer_info_t));
        record->allocation           = raw;
        record->next                 = arena->to_finalize;

        if (arena->to_finalize) {
            arena->to_finalize->prev = record;
        }

        arena->to_finalize = record;
    }

    arena->history->ring[arena->history->cur++] = raw->data;

    if (arena->history->cur == N00B_ALLOC_HISTORY_SIZE) {
        arena->history->cur = 0;
    }

    pthread_mutex_unlock(&arena->lock);

    assert(raw != NULL);
    return (void *)(raw->data);
}

void
n00b_header_gc_bits(uint64_t *bitfield, void *alloc)
{
    n00b_mem_ptr p = {.v = alloc};
    --p.alloc;

    n00b_mark_raw_to_addr(bitfield, alloc, &p.alloc->type);
}
