#define N00B_USE_INTERNAL_API

#include "n00b.h"

static pthread_once_t n00b_gc_init             = PTHREAD_ONCE_INIT;
static n00b_heap_t   *to_space                 = NULL;
static n00b_heap_t   *n00b_scratch_heap        = NULL;
n00b_heap_t          *n00b_marshal_heap        = NULL;
n00b_heap_t          *n00b_debug_heap          = NULL;
n00b_heap_t          *n00b_string_heap         = NULL;
bool                  __n00b_collector_running = false;
static n00b_heap_t   *__n00b_current_from_space;

bool
n00b_addr_in_one_heap(n00b_heap_t *h, void *p)
{
    if (h->released) {
        return false;
    }

    if (h->private && __n00b_current_from_space != h) {
        return false;
    }

    n00b_arena_t *a = h->first_arena;

    while (a) {
        if (n00b_addr_in_arena(a, p)) {
            return true;
        }
        a = a->successor;
    }

    return false;
}

#ifdef N00B_GC_STATS
static void
debug_print_heap(n00b_heap_t *p)
{
    n00b_crit_t crit = atomic_read(&p->ptr);
    char       *head = (char *)crit.next_alloc;
    uint64_t    used = 0;
    uint64_t    free = 0;

    n00b_arena_t *a = p->first_arena;

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

    printf(
        "Heap ID %lld: %s(%s:%d)%s%s%s%s%s\n"
        "%lld used, %llu free)\n"
        "  Allocs: new this heap: %d / inherited: %d / lifetime: %d\n",
        p->heap_id,
        name,
        p->file,
        p->line,
        p->pinned ? " (pinned)" : "",
        p->released ? " (released)" : "",
        p->no_trace ? " (no-trace)" : "",
        p->local_collects ? " (local gc)" : "",
        p->private ? " (private)" : "",
        used,
        free,
        p->alloc_count,
        p->inherit_count,
        p->total_alloc_count);

    a = p->first_arena;
    if (a) {
        printf("  Next alloc: %p & arena(s):\n", crit.next_alloc);

        while (a) {
            printf("  %p-%p\n", a->addr_start, a->addr_end);
            a = a->successor;
        }
    }
}

void
n00b_debug_all_heaps(void)
{
    for (unsigned int i = 0; i < n00b_next_heap_index; i++) {
        debug_print_heap(n00b_all_heaps + i);
    }
}
#endif

static inline void
n00b_create_alloc_guards(void)
{
    n00b_gc_guard = n00b_rand64();
#ifdef N00B_FULL_MEMCHECK
    n00b_end_guard = n00b_rand64();
#endif
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

    n00b_debug_heap = n00b_new_heap(N00B_DEFAULT_HEAP_SIZE);
    n00b_heap_set_name(n00b_debug_heap, "debug");

    // Create the main allocation heap and the 'to_space' heap that
    // marshalling uses.

    // No arenas get created within this heap at first.
    to_space = n00b_new_heap(0);
    n00b_heap_set_name(to_space, "gc to-space'");
    n00b_heap_pin(to_space);
    // n00b_heap_set_no_trace(to_space);
    n00b_heap_set_private(to_space);

    // When we do alloc in that space, it will not trigger collections.
    n00b_scratch_heap = n00b_new_heap(0);
    n00b_heap_set_name(n00b_scratch_heap, "gc scratch");
    n00b_heap_pin(n00b_scratch_heap);
    n00b_heap_set_private(n00b_scratch_heap);
    // n00b_heap_set_no_trace(n00b_scratch_heap);

    n00b_thread_master_heap = n00b_new_heap(N00B_START_TYPE_HEAP_SIZE);
    n00b_heap_set_name(n00b_thread_master_heap, "internal");
    n00b_heap_pin(n00b_thread_master_heap);
    // n00b_heap_set_local_collects(n00b_thread_master_heap);

    n00b_string_heap = n00b_new_heap(N00B_START_TYPE_HEAP_SIZE);
    n00b_heap_set_name(n00b_string_heap, "strings");

    //    n00b_type_heap = n00b_new_heap(N00B_START_TYPE_HEAP_SIZE);
    //    n00b_heap_set_no_trace(n00b_type_heap);
    // n00b_heap_set_local_collects(n00b_type_heap);

    //    n00b_marshal_heap = n00b_new_heap(N00B_START_MARSHAL_HEAP_SIZE);
    //    n00b_heap_set_no_trace(n00b_marshal_heap);
    // n00b_heap_set_local_collects(n00b_marshal_heap);

    n00b_default_heap->name       = "user";
    n00b_thread_master_heap->name = "internal";
    //    n00b_marshal_heap->name       = "marshal";
    //    n00b_type_heap->name          = "type";
    to_space->name                = "gc 'to-space'";
    n00b_scratch_heap->name       = "gc scratch";
}

static hatrack_mem_manager_t hatrack_manager = {
    .mallocfn  = n00b_malloc_wrap,
    .zallocfn  = n00b_malloc_wrap,
    .reallocfn = n00b_realloc_wrap,
    .freefn    = n00b_free_wrap,
    .arg       = NULL,
};

static void
n00b_initial_gc_setup(void)
{
    n00b_create_alloc_guards();
    n00b_discover_page_info();
    n00b_setup_heap_info();
    n00b_create_first_heaps();
    hatrack_setmallocfns(&hatrack_manager);
}

void
n00b_initialize_gc(void)
{
    pthread_once(&n00b_gc_init, n00b_initial_gc_setup);
}

typedef struct {
    n00b_alloc_hdr *src;
    n00b_alloc_hdr *dst;
} n00b_collect_wl_item_t;

typedef struct n00b_scan_range_t n00b_scan_range_t;

struct n00b_scan_range_t {
    void              *ptr;
    uint64_t           num_words;
    unsigned int       scanned : 1;
    n00b_scan_range_t *next;
};

typedef struct {
    n00b_heap_t            *from_space;
    crown_t                 memos;
    n00b_collect_wl_item_t *worklist;
    int32_t                 ix;
    int32_t                 root_touches;
    n00b_scan_range_t      *scan_list;
} n00b_collection_ctx;

static inline int64_t
get_next_heap_request_len(n00b_heap_t *h, size_t alloc_request)
{
    // Generally, we should want to consolidate a heap that's grown to
    // multiple arenas into a single arena.
    //
    // Here, we first calculate the sum of all sub-heaps, and then
    // make sure it's at least 4 times the size of the single request
    // that caused the collection (if any).
    //
    // Generally that check will only matter for tiny heaps or huge
    // allocations. In the worst case, where nothing (or little)
    // actually gets moved, once we're done collecting we'll
    // immediately double the heap space via a successor heap.

    n00b_arena_t *a = h->first_arena;

    int64_t result = 0;

    while (a) {
        result += a->user_length;
        a = a->successor;
        break;
    }

    return result + alloc_request;
}

static inline void
setup_collection_ctx(n00b_collection_ctx *ctx, n00b_heap_t *h)
{
    // Here, we're going to:
    //
    // 1. Make sure the scratch heap is big enough to accomodate the
    //    accounting for those, to avoid extra copying.
    //
    // 2. Reset the per-collection counter, adding it to the total
    //    alloc counter (since we're going to use it in the
    //    calculation needed for the previous item).
    //
    // 3. allocate the memos dict.
    //
    // 4. setup the worklist.
    //
    // Ideally, we'd want to reserve enough pages to allocate a
    // worklist item and a hash bucket for every single alloc in the
    // from-space, in case we have a heap where we are collecting
    // absolutely nothing.
    //
    // Generally, this will be a huge overestimate, but in the worst
    // case, it's an underestimate, since we are not accounting for
    // the extra hash buckets, since we resize if we reach the 75%
    // threshold.
    //
    // But pages we don't use and we free after collection aren't a
    // big deal, so we can pre-allocate the data structures to
    // hold as much as we'd ever need.
    //
    // The most complicated bit is the hash table, since we need to
    // make sure it never gets 75% full, but we also need to make sure
    // that it can size up to a power-of-two in terms of the number of
    // buckets. So we want to assume our # of allocs is 75% of the
    // hash table, and we'd need space for about 25% more entries than
    // that in the hash table. We'll need less in the list, but we
    // will have some overhead that we just won't bother calculating
    // (if it's an underestimate for small heaps, the hit won't
    // matter).
    //
    // In fact, instead of having to calculate 4/3 * n, we just do 3/2
    // * n, which is a larger number, and easier to calculate (n = n +
    // n >> 1)

    int32_t bpn     = (sizeof(n00b_collect_wl_item_t)
                   + sizeof(crown_bucket_t));
    int32_t n       = atomic_read(&h->alloc_count);
    int32_t heap_sz = (n + (n >> 1)) * bpn;

    h->total_alloc_count += n;
    n00b_add_arena(n00b_scratch_heap, heap_sz);

    // Set up a 'crown' instance.
    // First, how much space do we need if we had 100% utilization
    // (even though that's now allowed).
    int64_t to_populate = n * sizeof(crown_bucket_t);
    int64_t bkt_space   = hatrack_round_up_to_power_of_2(to_populate);

    // Now, until the bucket space, when fully populated stays under
    // the 75% threshold, double the bucket space.
    // The math here is just: x - .25x = .75x
    while (bkt_space - (bkt_space >> 2) <= to_populate) {
        bkt_space <<= 1;
    }

    // For presized stores, hatrack wants the log base 2 of the needed
    // size, not the actual size.
    char crown_size = (char)n00b_int_log2((uint64_t)bkt_space);

    // Since we've already set the thread-local heap to the scratch
    // heap, the allocations that result from the below will come from it.
    crown_init_size(&ctx->memos, crown_size, NULL);

    n00b_push_heap(n00b_thread_master_heap);
    // Now, allocate the maximum number of worklist items in one hunk.
    ctx->worklist = n00b_gc_raw_alloc(sizeof(n00b_collect_wl_item_t) * n,
                                      N00B_GC_SCAN_NONE);
    n00b_pop_heap();
    ctx->ix         = 0;
    ctx->from_space = h;
    ctx->scan_list  = NULL;
}

static inline void
make_to_space_our_space(n00b_heap_t *h)
{
    // Once we've copied everything into the dedicated 'to-space'
    // heap, we move the arena object into the original heap, and
    // remove the reference to the arena from the to-space heap.

    atomic_store(&h->ptr, atomic_read(&to_space->ptr));

    h->cur_arena_end = to_space->cur_arena_end;
    h->alloc_count   = 0;
    h->first_arena   = to_space->first_arena;
    h->newest_arena  = h->first_arena;

    to_space->first_arena  = NULL;
    to_space->newest_arena = NULL;
}

static inline void
free_excess_pages(n00b_heap_t *h, int64_t start_len)
{
}

static inline n00b_alloc_hdr *
get_relocation_addr(n00b_collection_ctx *ctx, n00b_alloc_hdr *hdr)
{
    // This is only called when MOVING, so is just an alloc, except we
    // don't need to do the typical alloc setup, just reserve an
    // address. The contents, including the header, will be copied out
    // of the old heap at the end.
    n00b_crit_t     entry     = atomic_read(&to_space->ptr);
    n00b_alloc_hdr *result    = entry.next_alloc;
    int             total_len = sizeof(n00b_alloc_hdr) + hdr->alloc_len;
    char           *end       = ((char *)result) + total_len;

    entry.next_alloc = (n00b_alloc_hdr *)end;
    assert(((void *)result) < to_space->first_arena->addr_end);

    atomic_store(&to_space->ptr, entry);

    return result;
}

static inline uint64_t
calculate_new_address(void *ptr, n00b_alloc_hdr *src, n00b_alloc_hdr *dst)
{
    int diff = (char *)ptr - (char *)src;
    return (uint64_t)diff + (uint64_t)dst;
}

static inline bool
should_process(n00b_collection_ctx *ctx, uint64_t *p)
{
    n00b_heap_t *heap = n00b_addr_find_heap(p);

    // Not in any heap, so just data.
    if (!heap) {
        return false;
    }
    // Local collects don't look for pointers in other heaps.
    if (ctx->from_space->local_collects && heap != ctx->from_space) {
        return false;
    }
    // Some heaps promise not to hold your pointers (or at least not to
    // care if you move the memory).
    if (heap != ctx->from_space && (heap->private || heap->no_trace)) {
        return false;
    }

    return true;
}
static inline void
add_mem_range(n00b_collection_ctx *ctx,
              uint64_t            *p,
              int                  num_words,
              bool                 scanned)
{
    n00b_scan_range_t *sr = n00b_heap_alloc(n00b_scratch_heap,
                                            sizeof(n00b_scan_range_t),
                                            N00B_GC_SCAN_NONE);

    assert(p);
    assert(!ctx->scan_list || n00b_addr_find_heap(ctx->scan_list) == n00b_addr_find_heap(sr));
    sr->next       = ctx->scan_list;
    sr->ptr        = p;
    sr->num_words  = num_words;
    sr->scanned    = scanned;
    ctx->scan_list = sr;

    //    printf("%p: %p / %08d  ; ", sr, p, num_words);
}

static void
run_scans(n00b_collection_ctx *ctx)
{
    // Here, we need to do two things:
    // 1. Replace pointers into the heap we're collecting with pointers
    //    in the space we're moving everything into.
    // 2. Follow *any* pointers to find heap allocations; we will
    //    want to scan any heap allocation that is reachable, period.
    //
    // If, in step 1, we haven't seen the pointer before, we will
    // eventually need to copy out the live record into the to-space.
    // But, since we have the world stopped, we can do that once all
    // pointers are pointing to the new heap.
    //
    // For #2, we want to prevent recursion. Therefore, when we find a
    // new live allocation record through a pointer, we add it to our
    // memos list (the memos map allocation records to relocation
    // addresses for those records), but with a NULL relocation
    // address, indicating it's not being relocated, but has been
    // scanned.

    n00b_scan_range_t *range = ctx->scan_list;

    while (range) {
        assert(!ctx->scan_list
               || n00b_addr_find_heap(ctx->scan_list) == n00b_addr_find_heap(range));
        uint64_t *p         = range->ptr;
        int       num_words = range->num_words;
        ctx->scan_list      = range->next;
        assert(num_words >= 0);
        assert(p);

        for (int i = 0; i < num_words; i++) {
            bool                    found;
            bool                    moving;
            n00b_alloc_hdr         *src_record;
            hatrack_hash_t          hv;
            n00b_alloc_hdr         *dst_record;
            n00b_collect_wl_item_t *wl;
            uint64_t                value = *p;

            if (!should_process(ctx, (void *)value)) {
                p++;
                continue;
            }

            src_record = n00b_find_allocation_record((void *)value);
            moving     = n00b_addr_in_one_heap(ctx->from_space, src_record);
            hv         = hash_pointer(src_record);
            dst_record = crown_get(&ctx->memos, hv, &found);

            if (!found) {
                if (moving) {
                    dst_record = get_relocation_addr(ctx, src_record);
                    wl         = ctx->worklist + ctx->ix++;
                    wl->src    = src_record;
                    wl->dst    = dst_record;
                }
                else {
                    dst_record = NULL;
                }
                assert((dst_record && moving) || !moving);
                ctx->root_touches++;
                crown_put(&ctx->memos, hv, dst_record, NULL);
                if (src_record->scan_fn != N00B_GC_SCAN_NONE) {
                    int len = sizeof(n00b_alloc_hdr) + src_record->alloc_len;
                    add_mem_range(ctx, (void *)src_record, len / 8, true);
                }
                if (moving) {
                    *p = calculate_new_address((void *)value,
                                               src_record,
                                               dst_record);
                }
            }
            else {
                if (moving) {
                    *p = calculate_new_address((void *)value,
                                               src_record,
                                               dst_record);
                }
            }

            p++;
        }
        range = ctx->scan_list;
    }
}

static inline void
trace_key_startup_items(n00b_collection_ctx *ctx)
{
    // These key bits of the type system need to be done before
    // anything else if we're collecting the heap they come out of.
    //
    // Though soon I expect to have the startup type info in its own
    // pinned heap and this will go away at that point.
    add_mem_range(ctx,
                  (uint64_t *)&n00b_bi_types[2],
                  1,
                  false);
    add_mem_range(ctx,
                  (uint64_t *)&n00b_bi_types[0],
                  N00B_NUM_BUILTIN_DTS,
                  false);
    run_scans(ctx);
}

static inline void
trace_one_rootset(n00b_collection_ctx *ctx, n00b_heap_t *h)
{
    if (!h->roots || h->released) {
        return;
    }

    n00b_gc_root_info_t *ri;

    uint32_t num_roots = hatrack_zarray_len(h->roots);
    for (uint32_t j = 0; j < num_roots; j++) {
        ri = hatrack_zarray_cell_address(h->roots,
                                         j);

        struct n00b_gc_root_tinfo_t tinfo = atomic_read(&ri->tinfo);

        if (tinfo.inactive) {
            continue;
        }

        // printf("trace root from %s:%d: ", ri->file, ri->line);
        add_mem_range(ctx, ri->ptr, ri->num_items, false);
        run_scans(ctx);
        // printf("had %d root touches.\n", ctx->root_touches);
        ctx->root_touches = 0;
    }
}

static inline void
trace_all_roots(n00b_collection_ctx *ctx)
{
    // Even though we allow tying roots to heaps, we assume bad
    // behavior, and will fully trace from all roots across all heaps,
    // looking for pointers into the current heap.
    n00b_heap_t *h = n00b_all_heaps;

    for (unsigned int i = 0; i < n00b_next_heap_index; i++) {
        if (i && !(i % n00b_heap_entries_pp)) {
            h = *(n00b_heap_t **)h;
            continue;
        }

        if (h->no_trace && h != ctx->from_space) {
            h++;
            continue;
        }

        trace_one_rootset(ctx, h);
        h++;
    }
}

static inline void
trace_stack(n00b_collection_ctx *ctx)
{
    // Here, we trace the stack for every thread.
    for (int i = 0; i < HATRACK_THREADS_MAX; i++) {
        n00b_thread_t *ti = atomic_read(&n00b_global_thread_list[i]);
        if (!ti) {
            continue;
        }
        if (ti->cur) {
            add_mem_range(ctx, ti->cur, (ti->base - ti->cur) / 8, false);
        }
        else {
            assert(ti->pthread_id != pthread_self());
        }
        run_scans(ctx);
        ctx->root_touches = 0;
    }
}

static inline void
perform_relocations(n00b_collection_ctx *ctx)
{
    n00b_collect_wl_item_t *item = ctx->worklist;
    int                     bytelen;

    for (int i = 0; i < ctx->ix; i++) {
        bytelen = sizeof(n00b_alloc_hdr) + item->src->alloc_len;
        memcpy(item->dst, item->src, bytelen);
        // We are about to get rid of the next_addr field, but for now,
        // keep it correct.
        item->dst->next_addr = ((char *)item->dst) + bytelen;
        assert(item->dst->guard == n00b_gc_guard);
        item++;
    }
}

void
n00b_heap_collect(n00b_heap_t *h, int64_t alloc_request)
{
    // Calling this on a pinned heap un-pins it.  Also note that
    // generally you will want to stop the world for this.

    int64_t      starting_arena_len = 0;
    n00b_heap_t *thread_local_stash = n00b_thread_master_heap;

#ifdef N00B_GC_STATS
    cprintf("+++Collection beginning:\n");
    debug_print_heap(h);
#ifdef N00B_GC_SHOW_COLLECT_STACK_TRACES
    n00b_static_c_backtrace();
#endif
#endif

    __n00b_collector_running  = true;
    __n00b_current_from_space = h;

    starting_arena_len = get_next_heap_request_len(h, alloc_request);

    // Here, we're going to make sure there's ample free space in the heap.
    // If the arena is less than 25% utilized at the end, we'll give back
    // pages, down to our previous page size.

    n00b_add_arena(to_space, starting_arena_len << 2);
    n00b_add_arena(n00b_scratch_heap, N00B_START_SCRATCH_HEAP_SIZE);

    // Set the thread heap to the scratch heap; this will allow us to
    // allocate dynamically for worklist management. We'll nuke those
    // allocations at the end. For anything we migrate, we will
    // explicitly specify the to-space heap when we allocate.
    n00b_thread_heap = n00b_scratch_heap;

    n00b_collection_ctx *ctx = n00b_gc_alloc_mapped(n00b_collection_ctx,
                                                    N00B_GC_SCAN_NONE);

    setup_collection_ctx(ctx, h);
    trace_key_startup_items(ctx);
    if (h->local_collects) {
        trace_one_rootset(ctx, h);
    }
    else {
        trace_all_roots(ctx);
    }
    trace_stack(ctx);

    printf("Done scanning; about to copy %d allocs.\n",
           ctx->ix);

    perform_relocations(ctx);

    make_to_space_our_space(h);
    h->inherit_count = ctx->ix;
    free_excess_pages(h, starting_arena_len);
    n00b_heap_clear(n00b_scratch_heap);

#ifdef N00B_GC_STATS
    cprintf("---Collection finished::\n");
    debug_print_heap(h);
#endif

    n00b_thread_heap          = thread_local_stash;
    __n00b_current_from_space = NULL;
    __n00b_collector_running  = false;
}

void
_n00b_heap_register_root(n00b_heap_t *h, void *p, uint64_t l N00B_ALLOC_XTRA)
{
    // Len is measured in 64 bit words and must be at least 1.
    n00b_gc_root_info_t *ri;

    h = n00b_get_heap(h);

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
    // Align the pointer for scanning back to the header.
    // It should be aligned, but people do the craziest things.
    void       **p = (void **)(((uint64_t)addr) & ~0x0000000000000007);
    n00b_heap_t *h = n00b_addr_find_heap(addr);

    if (!h) {
        return NULL;
    }

    while (p != (void *)h) {
        if (*(uint64_t *)p == n00b_gc_guard) {
            break;
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
