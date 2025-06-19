#define N00B_USE_INTERNAL_API
#include "n00b.h"

// Deep-copies an allocation, always into the user heap. This 100%
// assumes that either the object is read-only, or you have stopped
// the world.
//
// Unlike the main collector, which modifies pointers to point to the
// new heap, and then copies everything at the end, we re-write only
// when copying into the new heap.
//
// And, we are careful to count how much space we need before taking
// from the user heap, to avoid needing to collect in the middle of a
// copy.
//
// The address does not need to be at the front of the object, but it
// absolutely needs to be in the heap.
//
// Note that this does not copy the internal type object, the way
// marshal would.
//
// Also note that we only copy pointers reachable through the heap
// we're copying out of; we do NOT follow pointers into other heaps,
// we just preserve them.

typedef struct {
    n00b_dict_t *memos;
    int64_t      alloc_needs;
    n00b_list_t *scan_list;
    n00b_heap_t *src_heap;
} n00b_deep_copy_ctx;

static inline n00b_type_t *
dc_type(n00b_type_t *t)
{
    n00b_push_heap(n00b_default_heap);
    n00b_type_t *r = n00b_type_copy(t);
    n00b_pop_heap();
    return r;
}

static inline void
dc_scan(n00b_deep_copy_ctx *ctx)
{
    int remaining = n00b_list_len(ctx->scan_list);

    while (remaining--) {
        n00b_alloc_hdr *h = n00b_list_dequeue(ctx->scan_list);
        bool            f;
        n00b_dict_get(ctx->memos, h, &f);

        if (f) {
            continue;
        }
        n00b_dict_put(ctx->memos, h, (void *)(int64_t)ctx->alloc_needs);
        ctx->alloc_needs += h->alloc_len;

        if (!h->n00b_ptr_scan) {
            continue;
        }

        int       m = (h->alloc_len - sizeof(n00b_alloc_hdr)) / sizeof(void *);
        uint64_t *p = h->data;

        while (m--) {
            uint64_t value = *p;
            p++;
            if (value == N00B_NOSCAN) {
                break;
            }
            n00b_heap_t *heap = n00b_addr_find_heap((uint64_t *)value, true);

            if (heap == ctx->src_heap) {
                n00b_alloc_hdr *new;
                new = n00b_find_allocation_record((void *)value);
                n00b_list_append(ctx->scan_list, new);
                remaining++;
            }
        }
    }
}

static inline int64_t
dc_translate(n00b_deep_copy_ctx *ctx, int64_t start, int64_t value)
{
    // 'start' is where offsets start.
    // 'offset' is the offset to the start of the new record, from 'start'.
    // 'diff' is how many bytes into an allocation record we want the ptr to be.

    n00b_alloc_hdr *h      = n00b_find_allocation_record((void *)value);
    int64_t         offset = (int64_t)n00b_dict_get(ctx->memos, h, NULL);
    int64_t         diff   = value - (int64_t)h;

    return start + offset + diff;
}

static inline void *
dc_copy(n00b_deep_copy_ctx *ctx)
{
    // Since heap_alloc adds enough space for the header, we subtract
    // from our alloc request.  Still, The size we need to alloc will
    // get padded by our call to heap_alloc, so we can expect to end
    // up with some extra room.
    char           *p;
    n00b_alloc_hdr *src;
    n00b_alloc_hdr *dst;
    int             l     = ctx->alloc_needs - sizeof(n00b_alloc_hdr);
    char           *start = n00b_heap_alloc(n00b_default_heap, l, NULL);
    n00b_list_t    *items = n00b_dict_items(ctx->memos);
    uint64_t        n     = n00b_list_len(items);
    n00b_tuple_t   *tup;

    start -= sizeof(n00b_alloc_hdr);

    for (uint32_t i = 0; i < n; i++) {
        tup                = n00b_list_get(items, i, NULL);
        src                = n00b_tuple_get(tup, 0);
        p                  = start + (int64_t)n00b_tuple_get(tup, 1);
        dst                = (n00b_alloc_hdr *)p;
        dst->guard         = n00b_gc_guard;
        dst->type          = dc_type(src->type);
        dst->alloc_len     = src->alloc_len;
        dst->n00b_ptr_scan = src->n00b_ptr_scan;
        dst->n00b_obj      = src->n00b_obj;
        dst->n00b_finalize = src->n00b_finalize; // TODO: migrate the finalizer.
#if defined(N00B_ADD_ALLOC_LOC_INFO)
        dst->alloc_file = src->alloc_file;
        dst->alloc_line = dst->alloc_line;
#endif
#if defined(n00b_GC_ALLOW_DEBUG_BIT)
        dst->n00b_debug           = src->n00b_debug;
        dst->n00b_debug_propogate = src->n00b_debug_propogate;
#endif

        uint64_t *from_p    = src->data;
        uint64_t *to_p      = dst->data;
        int       num_bytes = src->alloc_len - sizeof(n00b_alloc_hdr);
        int       num_words = num_bytes / sizeof(void *);
        bool      check     = src->n00b_ptr_scan;

        while (num_words--) {
            if (check) {
                uint64_t value = *from_p;
                from_p++;

                if (value == N00B_NOSCAN) {
                    check   = false;
                    *to_p++ = value;
                    continue;
                }

                n00b_heap_t *heap = n00b_addr_find_heap((uint64_t *)value,
                                                        true);

                if (heap == ctx->src_heap) {
                    *to_p = dc_translate(ctx, (int64_t)start, value);
                    to_p++;
                }
                else {
                    *to_p = value;
                    to_p++;
                }
                continue;
            }

            *to_p++ = *from_p++;
        }
    }

    return start;
}

void *
n00b_heap_deep_copy(void *addr)
{
    n00b_deep_copy_ctx ctx;

    n00b_suspend_collections();

    ctx.src_heap = n00b_addr_find_heap(addr, true);

    if (!ctx.src_heap) {
        return NULL;
    }

    ctx.alloc_needs = 0;
    ctx.memos       = n00b_dict(n00b_type_ref(), n00b_type_int());
    ctx.scan_list   = n00b_list(n00b_type_ref());

    n00b_alloc_hdr *h = n00b_find_allocation_record(addr);

    n00b_list_append(ctx.scan_list, h);

    dc_scan(&ctx);
    h = dc_copy(&ctx);

    n00b_allow_collections();

    return h->data;
}
