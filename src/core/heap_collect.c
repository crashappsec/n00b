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

int          __n00b_collector_running = 0;
n00b_heap_t *__n00b_current_from_space;

#ifdef N00B_GC_ALLOW_DEBUG_BIT
bool n00b_using_debug_bit = false;
#endif

typedef struct {
    int      entries_per_page;
    int      read_index;
    int      write_index;
    int      total_items;
    int64_t *read_page;
    int64_t *write_page;
} n00b_work_list_t;

typedef struct {
#if defined(N00B_SHOW_GC_ROOTS)
    int32_t allocs_traced;
    int32_t new_allocs_traced;
#endif
    n00b_heap_t     *from_space;
    char            *next_alloc;
    n00b_work_list_t scan_work;
    n00b_work_list_t cleanup_work;
    int32_t          allocs_copied;
} n00b_collection_ctx;

static void _scan_root_set(n00b_collection_ctx *, int64_t **, int);
#define scan_root_set(ctx, start, n) _scan_root_set(ctx, (int64_t **)(start), n)

static inline void
setup_work_list(n00b_work_list_t *wl)
{
    wl->entries_per_page = (n00b_page_bytes / sizeof(int64_t)) - 1;
    wl->read_index       = 0;
    wl->write_index      = 0;
    wl->read_page        = n00b_unprotected_mempage();
    wl->write_page       = wl->read_page;
}

static inline void
enqueue_work(n00b_work_list_t *wl, n00b_alloc_hdr *hdr)
{
    int n = wl->write_index++;
    if (n == wl->entries_per_page) {
        int64_t *new_page = n00b_unprotected_mempage();
        wl->write_page[n] = (int64_t)new_page;
        wl->write_page    = new_page;
        wl->write_index   = 1;
        n                 = 0;
    }

    wl->write_page[n] = (int64_t)hdr;
    wl->total_items++;
}

static inline n00b_alloc_hdr *
dequeue_work(n00b_work_list_t *wl)
{
    if (wl->read_index == wl->write_index && wl->read_page == wl->write_page) {
        return NULL;
    }

    int n = wl->read_index++;

    if (n == wl->entries_per_page) {
        int64_t *next_page = (int64_t *)wl->read_page[n];
        n00b_delete_mempage(wl->read_page);

        wl->read_page  = next_page;
        wl->read_index = 1;
        n              = 0;
    }

    return (n00b_alloc_hdr *)(void *)wl->read_page[n];
}

static inline void
delete_work_list(n00b_work_list_t *wl)
{
    assert(wl->read_page == wl->write_page);
    assert(wl->read_page[wl->entries_per_page] == 0ull);
    n00b_delete_mempage(wl->read_page);
}

#if defined(N00B_SHOW_GC_ROOTS)
static inline void
alloc_reached_again(n00b_collection_ctx *ctx)
{
    ctx->allocs_traced++;
}

static inline void
record_new_alloc(n00b_collection_ctx *ctx, bool in_heap)
{
    if (in_heap) {
        ctx->allocs_traced++;
        ctx->new_allocs_traced++;
    }
}

#else
#define alloc_reached_again(x)
#define record_new_alloc(x, y)
#endif

#if defined(N00B_GC_ALLOW_DEBUG_BIT)
static inline char *
get_alloc_type(n00b_alloc_hdr *hdr)
{
    n00b_type_t *t = hdr->type;

    if (!t || t->base_index < 0 || t->base_index > N00B_NUM_BUILTIN_DTS) {
        return "untyped allocation";
    }

    return (char *)n00b_base_type_info[t->base_index].name;
}
#endif

static inline void
new_alloc_reached(n00b_collection_ctx *ctx, n00b_alloc_hdr *hdr, bool in_heap)
{
    record_new_alloc(ctx, in_heap);
    hdr->n00b_traced = true;
    enqueue_work(&ctx->scan_work, hdr);

    // List of out-of-heap pointers traced; we need to remove the
    // trace bit at the end of collection.
    if (!in_heap) {
        enqueue_work(&ctx->cleanup_work, hdr);
    }

#if defined(N00B_GC_ALLOW_DEBUG_BIT)
    if (hdr->n00b_debug || !n00b_using_debug_bit) {
        fprintf(stderr,
                "Reached %s at %p (%s:%d) (queuing scan)\n",
                get_alloc_type(hdr),
                hdr,
                hdr->alloc_file,
                hdr->alloc_line);
    }
#endif
}

static inline void
initialize_dst_alloc(n00b_collection_ctx *ctx, n00b_alloc_record_t *from_p)
{
    n00b_alloc_record_t *to_p = (n00b_alloc_record_t *)ctx->next_alloc;

    ctx->next_alloc = ctx->next_alloc + from_p->alloc_len;

    to_p->empty_guard      = n00b_gc_guard;
    to_p->alloc_len        = from_p->alloc_len;
    to_p->n00b_obj         = from_p->n00b_obj;
    to_p->n00b_finalize    = from_p->n00b_finalize;
    to_p->n00b_ptr_scan    = from_p->n00b_ptr_scan;
    to_p->cached_hash[0]   = from_p->cached_hash[0];
    to_p->cached_hash[1]   = from_p->cached_hash[1];
    from_p->cached_hash[0] = (uint64_t)to_p; // Set forwarding address.

#if defined(N00B_ADD_ALLOC_LOC_INFO)
    to_p->alloc_file = from_p->alloc_file;
    to_p->alloc_line = from_p->alloc_line;

#if defined(N00B_FULL_MEMCHECK)
    if (!from_p->alloc_file) {
        fprintf(stderr, "Warning: no file name found for alloc %p\n", from_p);
    }
#endif
#endif

#if defined(N00B_GC_ALLOW_DEBUG_BIT)
    to_p->n00b_debug = from_p->n00b_debug;
#endif
}

// Returns the source address of the header if the passed pointer
// should be rewritten.  If the address needs to be scanned, this
// queues it.
//
// This does NOT rewrite the pointer; it returns the allocation header
// to use as an offset.
static inline n00b_alloc_hdr *
check_one_word(n00b_collection_ctx *ctx, void *addr)
{
    n00b_heap_t *h = n00b_addr_find_heap(addr, false);

    if (!h) {
        // Not in any heap; it's data.
        return NULL;
    }

    bool            in_gc_heap = h == ctx->from_space;
    n00b_alloc_hdr *record     = n00b_find_allocation_record(addr);

    if (!in_gc_heap)
        if (h->private || h->no_trace || ctx->from_space->local_collects) {
            return NULL;
        }

    if (record->n00b_traced) {
        if (in_gc_heap) {
            alloc_reached_again(ctx);
            return record;
        }
        return NULL;
    }

    new_alloc_reached(ctx, record, in_gc_heap);

    // We *might* have queued a record to scan, but the caller doesn't
    // need to do any rewriting.
    if (!in_gc_heap) {
        return NULL;
    }

    initialize_dst_alloc(ctx, (n00b_alloc_record_t *)record);

    return record;
}

static inline int64_t *
rewrite_pointer(n00b_alloc_hdr *hdr, int64_t old_p)
{
    int64_t diff = old_p - (int64_t)hdr;
    char   *p    = (char *)(((n00b_alloc_record_t *)hdr)->cached_hash[0]);

    return (int64_t *)(p + diff);
}

static inline void *
load_forwarding_alloc(void *p)
{
    n00b_alloc_record_t *from_hdr = p;
    n00b_alloc_hdr      *fw       = (void *)from_hdr->cached_hash[0];

    return fw;
}

static void
scan_one_alloc(n00b_collection_ctx *ctx, n00b_alloc_hdr *scanning)
{
    int64_t        *from_p  = (int64_t *)scanning->data;
    int64_t       **to_p    = NULL;
    int             alen    = scanning->alloc_len - sizeof(n00b_alloc_hdr);
    int             n_words = alen / sizeof(void *);
    // Applies to this alloc. We only copy if we're in the same heap.
    bool            copying = n00b_addr_in_one_heap(ctx->from_space, scanning);
    // Applies to individual memory cells that might be pointers.
    n00b_alloc_hdr *record;
    int             i = 0;

#if defined(N00B_GC_ALLOW_DEBUG_BIT)
    if (scanning->n00b_debug || !n00b_using_debug_bit) {
        char *typename = get_alloc_type(scanning);

        fprintf(stderr,
                "%p (%s:%d): Scanning %s alloc of %d words ",
                from_p,
                scanning->alloc_file,
                scanning->alloc_line,
                typename,
                n_words);

        if (copying) {
            fprintf(stderr,
                    " moving to %p",
                    ((n00b_alloc_record_t *)scanning)->cached_hash[0]);
        }
        fprintf(stderr, "\n");
    }
#endif

    if (copying) {
        n00b_alloc_record_t *fw = load_forwarding_alloc(scanning);
        // We special cased rewriting the type in place before
        // scanning, but we do need to copy it.
        fw->type                = scanning->type;
        to_p                    = (int64_t **)fw->data;
        ctx->allocs_copied      = ctx->allocs_copied + 1;

        // This condition will only be true here if we're copying;
        // when this bit is off, out-of-heap allocs don't get queued.
        if (!scanning->n00b_ptr_scan) {
copy_only:
            for (; i < n_words; i++) {
                *to_p++ = (int64_t *)*from_p++;
            }
            return;
        }
    }

    for (; i < n_words; i++) {
        record = check_one_word(ctx, (void *)*from_p);

        // First, cover the case where we didn't find an in-heap pointer.
        //
        // If the record we're scanning is in the heap we're
        // collecting, then 'copying' will be set, and we need to be
        // copying the entire allocation, not just looking for
        // pointers.
        //
        // Therefore, finding noscan is the end of the task for an
        // out-of-heap scan, but if we're copying we need to continue,
        // without rewriting pointers.

        if (!record) {
            if (((uint64_t)*from_p) == N00B_NOSCAN) {
                if (copying) {
                    goto copy_only;
                }
                return;
            }
            if (copying) {
                *to_p++ = (int64_t *)*from_p;
            }
            from_p++;
            continue;
        }

        // Now, we did find an in-heap pointer.
        //
        // We rewrite the pointer in place, even if the record we're
        // scanning is being moved (doing so makes it easier to see
        // what's garbage in a hex dump).
        //
        // After that rewrite (if appropriate), if the current
        // allocation we're scanning is in the heap we're collecting,
        // we copy over the current word into the new space.

        *from_p = (int64_t)rewrite_pointer(record, *from_p);

        if (copying) {
            *to_p++ = (int64_t *)*from_p;
        }
        from_p++;
    }
}

static inline void
run_all_scans(n00b_collection_ctx *ctx)
{
    n00b_alloc_hdr *item = dequeue_work(&ctx->scan_work);

    while (item) {
        n00b_type_t *t = item->type;

        if (t) {
            n00b_alloc_hdr *titem = check_one_word(ctx, t);

            if (titem) {
                item->type = (void *)rewrite_pointer(titem, (int64_t)t);
            }
            else {
                titem = ((n00b_alloc_hdr *)item->type) - 1;
            }
            scan_one_alloc(ctx, titem);
        }

        scan_one_alloc(ctx, item);
        item = dequeue_work(&ctx->scan_work);
    }
}

static void
_scan_root_set(n00b_collection_ctx *ctx, int64_t **start, int num_words)
{
    int64_t       **p = start;
    int64_t        *val;
    n00b_alloc_hdr *rec;

    for (int i = 0; i < num_words; i++) {
        val = *p;
        rec = check_one_word(ctx, val);

        if (rec) {
            *p = rewrite_pointer(rec, (int64_t)val);
        }
        p++;
    }
    run_all_scans(ctx);
}

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
        result += a->last_issued - a->addr_start;
        a = a->successor;
        break;
    }

    if (h->expand) {
        result <<= 1;
        h->expand = false;
    }

    // Convert from words to bytes; pointer math was on int64_t *'s
    result *= 8;

    return (result + alloc_request);
}

static inline void
reset_next_alloc_ptr(n00b_heap_t *h, void *next_alloc)
{
    n00b_crit_t new_crit = {.next_alloc = next_alloc, .thread = NULL};
    atomic_store(&h->ptr, new_crit);
}

static inline void
setup_collection(n00b_collection_ctx *ctx, n00b_heap_t *h, int64_t r)
{
    __n00b_collector_running++;
    __n00b_current_from_space = h;
    ctx->from_space           = h;
    ctx->allocs_copied        = 0;

    n00b_arena_t *a    = h->newest_arena;
    n00b_crit_t   crit = atomic_read(&h->ptr);

    n00b_unlock_arena_header(a);
    a->last_issued = crit.next_alloc;
    n00b_lock_arena_header(a);

    // Must happen after setting 'last_issued'
    n00b_run_memcheck(h);

    // Here, we're going to make sure there's ample free space in the heap.
    // If the arena is less than 25% utilized at the end, we'll give back
    // pages, down to our previous page size.
    int64_t arena_len = get_next_heap_request_len(h, r);

    n00b_add_arena(n00b_to_space, arena_len);

    // Here, we're going to:
    //
    // 1. Reset the per-collection counter, adding it to the total
    //    alloc counter (since we're going to use it in the
    //    calculation needed for the previous item).
    //
    // 2. setup the worklist.

    h->total_alloc_count += atomic_read(&h->alloc_count) + h->inherit_count;

    ctx->next_alloc = n00b_to_space->newest_arena->addr_start;
    setup_work_list(&ctx->scan_work);
    setup_work_list(&ctx->cleanup_work);
}

static inline void
cleanup_work_lists(n00b_collection_ctx *ctx)
{
#if defined(N00B_GC_STATS)
    cprintf("Traced %d out-of-heap allocs.\n",
            ctx->scan_work.total_items,
            ctx->cleanup_work.total_items);
#endif

    delete_work_list(&ctx->scan_work);

    n00b_alloc_hdr *h = dequeue_work(&ctx->cleanup_work);

    while (h) {
        h->n00b_traced = false;
        h              = dequeue_work(&ctx->cleanup_work);
    }

    delete_work_list(&ctx->cleanup_work);
}

static inline void
make_to_space_our_space(n00b_collection_ctx *ctx, n00b_heap_t *h)
{
    // Once we've copied everything into the dedicated 'to-space'
    // heap, we move the arena object into the original heap, and
    // remove the reference to the arena from the to-space heap.

    h->cur_arena_end = n00b_to_space->cur_arena_end;
    h->alloc_count   = 0;
    h->first_arena   = n00b_to_space->first_arena;
    h->newest_arena  = h->first_arena;

    n00b_to_space->first_arena  = NULL;
    n00b_to_space->newest_arena = NULL;

    reset_next_alloc_ptr(h, ctx->next_alloc);
}

static inline void
trace_key_startup_items(n00b_collection_ctx *ctx)
{
    // These key bits of the type system need to be done before
    // anything else if we're collecting the heap they come out of.
    scan_root_set(ctx, &n00b_bi_types[2], 1);
    run_all_scans(ctx);
    scan_root_set(ctx, &n00b_bi_types[0], N00B_NUM_BUILTIN_DTS);
    run_all_scans(ctx);
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

#if defined(N00B_SHOW_GC_ROOTS)
#if defined(N00B_ADD_ALLOC_LOC_INFO)
        fprintf(stderr,
                "Scanning %d items for root %p, added at %s:%d: ",
                ri->num_items,
                ri->ptr,
                ri->file,
                ri->line);

#else
        fprintf(stderr,
                "Scanning %d items for root %p: ",
                ri->num_items,
                ri->ptr);
#endif
        ctx->new_allocs_traced = 0;
        ctx->allocs_traced     = 0;
#endif

        scan_root_set(ctx, ri->ptr, ri->num_items);
        run_all_scans(ctx);

#if defined(N00B_SHOW_GC_ROOTS)
        fprintf(stderr,
                "found %d new records to ptr scan (%d total records traced)\n",
                ctx->new_allocs_traced,
                ctx->allocs_traced);
#endif
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
    int n = atomic_read(&n00b_next_thread_slot);
    n     = n00b_min(n, HATRACK_THREADS_MAX);

    for (int i = 0; i < n; i++) {
        n00b_thread_t *ti = atomic_read(&n00b_global_thread_list[i]);
        if (!ti) {
            continue;
        }
        if (ti->cur) {
            int num_words = (ti->base - ti->cur) / 8;
#if defined(N00B_SHOW_GC_ROOTS)
            fprintf(stderr,
                    "Scanning stack for thread mmm #%lld : %p-%p (%d words)\n",
                    ti->mmm_info.tid,
                    ti->cur,
                    ti->cur + num_words,
                    num_words);
#endif
            scan_root_set(ctx, ti->cur, num_words);
        }
        run_all_scans(ctx);
    }
}

#ifdef N00B_SHOW_GARBAGE_REPORTS
typedef struct {
    char   *file;
    int64_t line;
    int64_t bytes;
    int64_t num_allocs;
} n00b_garbage_entry_t;

static void
garbage_report(n00b_heap_t *h)
{
    fprintf(stderr, "Garbage Report:\n---------------\n");
    n00b_push_heap(n00b_internal_heap);

    crown_t       info;
    n00b_arena_t *a = h->first_arena;

    crown_init_size(&info, 18, NULL);

    while (a) {
        uint64_t *cur = a->addr_start;
        uint64_t *end = a->addr_end;
        a             = a->successor;

        while (cur < end) {
            if (*cur != n00b_gc_guard) {
                cur++;
                continue;
            }
            n00b_alloc_hdr *h = (void *)cur;

            if (!h->n00b_moved) {
                bool                 found     = false;
                n00b_garbage_entry_t candidate = {
                    .file       = h->alloc_file,
                    .line       = h->alloc_line,
                    .bytes      = 0,
                    .num_allocs = 0,
                };

                hash_internal_conversion_t u;
                u.xhv = XXH3_128bits(&candidate, sizeof(n00b_garbage_entry_t));

                n00b_garbage_entry_t *entry = crown_get(&info, u.lhv, &found);

                if (found) {
                    entry->bytes += h->alloc_len;
                    entry->num_allocs++;
                }
                else {
                    entry = n00b_gc_alloc_mapped(n00b_garbage_entry_t,
                                                 N00B_GC_SCAN_NONE);

                    entry->file       = h->alloc_file;
                    entry->line       = h->alloc_line;
                    entry->bytes      = h->alloc_len;
                    entry->num_allocs = 1;
                    crown_put(&info, u.lhv, entry, NULL);
                }
            }
            int n = h->alloc_len / 8;

            if (!n) {
                n++;
            }
            cur += n;
        }
    }

    crown_store_t *store = atomic_read(&info.store_current);

    for (unsigned int i = 0; i <= store->last_slot; i++) {
        crown_bucket_t       *bucket = &store->buckets[i];
        crown_record_t        rec    = atomic_read(&bucket->record);
        n00b_garbage_entry_t *e      = rec.item;
        if (e) {
            fprintf(stderr,
                    "%lld allocs, %lld total bytes from %s:%lld\n",
                    e->num_allocs,
                    e->bytes,
                    e->file,
                    e->line);
        }
    }

    n00b_pop_heap();
}
#else
#define garbage_report(x)
#endif

static inline void
finish_collection(n00b_collection_ctx *ctx, n00b_heap_t *h)
{
    garbage_report(h);
    cleanup_work_lists(ctx);

    make_to_space_our_space(ctx, h);
    h->inherit_count = ctx->allocs_copied;
    h->num_collects++;

    void         *p = ((void *)ctx->next_alloc);
    n00b_arena_t *a = h->first_arena;

    uint64_t words_used  = p - a->addr_start;
    uint64_t total_words = a->addr_end - a->addr_start;

    if ((words_used << 2) < total_words) {
        h->expand = true;
    }
}

void
n00b_heap_collect(n00b_heap_t *h, int64_t alloc_request)
{
    n00b_collection_ctx ctx;

    assert(!n00b_collection_suspended);
    n00b_gts_stop_the_world();

    // Calling this on a pinned heap will cause this to collect, not
    // to add an arena. That allows us to selectively compact, but
    // only at a time of our control.

#ifdef N00B_GC_STATS
    n00b_duration_t  start;
    n00b_duration_t  end;
    n00b_duration_t *diff;
    cprintf("+++Collection beginning.\n");
    n00b_debug_print_heap(h);
    int stored_alloc_count = h->alloc_count;

#ifdef N00B_GC_SHOW_COLLECT_STACK_TRACES
    n00b_static_c_backtrace();
#endif

    clock_gettime(CLOCK_MONOTONIC, &start);
#endif

    setup_collection(&ctx, h, alloc_request);
    trace_key_startup_items(&ctx);
    if (h->local_collects) {
        trace_one_rootset(&ctx, h);
    }
    else {
        trace_all_roots(&ctx);
    }
    trace_stack(&ctx);

    finish_collection(&ctx, h);

#if defined(N00B_GC_STATS)
    clock_gettime(CLOCK_MONOTONIC, &end);
    diff = n00b_duration_diff(&end, &start);

    cprintf("---Collection ended (%s).\n", n00b_to_string(diff));
    fprintf(stderr,
            "Preserved %d of %d records (%f%%)\n",
            ctx.allocs_copied,
            stored_alloc_count,
            (100.0 * ctx.allocs_copied) / stored_alloc_count);

    __n00b_current_from_space = NULL;
    --__n00b_collector_running;
    n00b_gts_restart_the_world();

#endif
}
