#define N00B_USE_INTERNAL_API
#include "n00b.h"

#ifdef N00B_FULL_MEMCHECK

typedef struct mc_state_t {
    n00b_heap_t    *heap;
    n00b_arena_t   *cur_arena;
    n00b_alloc_hdr *cur_alloc;
    n00b_alloc_hdr *prev_alloc;
    int             arena_count;
    int             total_errors;
    int             alloc_ix;
    bool            err_in_arena;
} mc_state_t;

void
n00b_show_arena_alloc_locs(n00b_arena_t *arena)
{
    uint64_t *p   = arena->addr_start;
    uint64_t *end = (uint64_t *)arena->last_issued;

    while (p < end) {
        if (*p == n00b_gc_guard) {
            n00b_alloc_hdr *h = (n00b_alloc_hdr *)p;
            if (h->alloc_file) {
                printf("Alloc hdr @%p alloced from %s:%d\n",
                       p,
                       h->alloc_file,
                       h->alloc_line);
            }
            else {
                printf("Alloc hdr @%p has no location info\n", p);
            }
            p += (h->alloc_len / 8);
        }
        else {
            while (p < end) {
                if (*p == N00B_NOSCAN) {
                    p += getpagesize() / 8;
                    break;
                }
                if (*p == n00b_gc_guard) {
                    break;
                }
                p++;
            }
        }
    }
}

static void
register_error(mc_state_t *state)
{
    if (!state->total_errors++) {
        fprintf(stderr,
                "======================================================\n");
        fprintf(stderr,
                "Memcheck found a problem on heap %lld (%s)"
                "(%s; %s:%d; %d allocs this cycle)\n",
                (long long int)state->heap->heap_id,
                state->heap->name,
                state->heap->name,
                state->heap->file,
                state->heap->line,
                atomic_read(&state->heap->alloc_count));
    }

    if (!state->err_in_arena) {
        fprintf(stderr,
                "!! Error(s) when scanning Arena #%d: %p-%p\n",
                state->arena_count,
                state->cur_arena->addr_start,
                state->cur_arena->last_issued);
        state->err_in_arena = true;
    }
}

static void
n00b_print_possible_loc(n00b_alloc_hdr *h, n00b_alloc_hdr *prev)
{
    if (!h) {
        return;
    }

    if (!h->alloc_file) {
        cprintf(" (no alloc location info) ");
        if (prev) {
            cprintf(" previous header @%p", prev);
            n00b_print_possible_loc(prev, NULL);
        }
    }
    else {
        fprintf(stderr, " %s:%d", h->alloc_file, h->alloc_line);
    }
}

#define mc_warn(state, msg, ...)                                         \
    {                                                                    \
        if (state->cur_alloc) {                                          \
            fprintf(stderr,                                              \
                    "record %d@%p: ",                                    \
                    state->alloc_ix,                                     \
                    state->cur_alloc);                                   \
        }                                                                \
        cprintf("n00b_memcheck: warn: " msg __VA_OPT__(, ) __VA_ARGS__); \
        n00b_print_possible_loc(state->cur_alloc, state->prev_alloc);    \
        cprintf("\n");                                                   \
    }

#define mc_err(state, msg, ...)                                           \
    {                                                                     \
        register_error(state);                                            \
        if (state->cur_alloc) {                                           \
            fprintf(stderr,                                               \
                    "record %d@%p: ",                                     \
                    state->alloc_ix,                                      \
                    state->cur_alloc);                                    \
        }                                                                 \
        cprintf("n00b_memcheck: error: " msg __VA_OPT__(, ) __VA_ARGS__); \
        n00b_print_possible_loc(state->cur_alloc, state->prev_alloc);     \
        cprintf("\n");                                                    \
    }

static inline bool
alloc_is_blank(n00b_alloc_hdr *h)
{
    if (h->guard || h->type || h->n00b_marshal_end || h->n00b_ptr_scan
        || h->n00b_obj || h->n00b_finalize || h->n00b_traced
        || h->cached_hash) {
        return false;
    }

#if defined(N00B_ADD_ALLOC_LOC_INFO)
    if (h->alloc_len || h->alloc_len) {
        return false;
    }
#endif

    return true;
}

static void
n00b_memcheck_arena(mc_state_t *state)
{
    n00b_arena_t *a = state->cur_arena;

    if (!a) {
        return;
    }

    char *p   = a->addr_start;
    char *end = n00b_min(a->addr_end, a->last_issued);

    if (p + a->user_length != a->addr_end) {
        mc_warn(state,
                "Cached arena len doesn't match bounds (%d vs %d)",
                a->user_length,
                ((char *)a->addr_end) - p);
    }

    n00b_alloc_hdr *h           = (void *)p;
    uint64_t       *aligned     = (void *)p;
    uint64_t       *aligned_end = (uint64_t *)end;
    uint64_t       *expected_next;
    int             extra_magic = 0;

    while (p < end && expected_next < aligned_end) {
        state->prev_alloc = state->cur_alloc;
        state->cur_alloc  = h;

        if ((p + sizeof(n00b_alloc_hdr)) > end) {
            break;
        }
        if (h->guard != n00b_gc_guard) {
            if (alloc_is_blank(h)) {
                break;
            }
            mc_err(state, "Expected alloc record, but missing magic.");
scan_for_next:
            assert(aligned_end <= (uint64_t *)a->last_issued);
            while (aligned < aligned_end) {
                if (*aligned == n00b_gc_guard) {
                    p = (void *)aligned;
                    h = (void *)aligned;
                    break;
                }

                ++aligned;
            }

            if (aligned >= aligned_end) {
                goto cleanup;
            }
            state->alloc_ix++;
            p = (void *)aligned;
            h = (void *)p;
            continue;
        }

	
        if (h->alloc_len < 0 ||	h->alloc_len % 8) {
            mc_err(state,
                   "Invalid alloc length for allocation at %p (%ld)",
                   h,
                   h->alloc_len);
            goto scan_for_next;
        }
        p += h->alloc_len;
        expected_next = (uint64_t *)p;

        if (expected_next > aligned_end) {
            mc_err(state,
                   "Allocation record at %p has end beyond the arena bounds",
                   h);
            expected_next = aligned_end;
        }

        ++aligned;

        while (aligned < expected_next) {
            if (*aligned == n00b_gc_guard) {
                extra_magic++;
            }
            ++aligned;
        }

        if (extra_magic) {
            mc_warn(state,
                    "Found %d invalid magic values in record",
                    extra_magic);
            extra_magic = 0;
        }

        h = (n00b_alloc_hdr *)p;
        state->alloc_ix++;
    }

cleanup:
    // Remove stack references.
    h             = NULL;
    p             = NULL;
    end           = NULL;
    aligned       = NULL;
    aligned_end   = NULL;
    expected_next = NULL;
}

void
n00b_run_memcheck(n00b_heap_t *h)
{
    mc_state_t state = {
        .heap         = h,
        .cur_arena    = h->first_arena,
        .err_in_arena = false,
        .arena_count  = 0,
        .total_errors = 0,
        .alloc_ix     = 0,
        .cur_alloc    = NULL,
        .prev_alloc   = NULL,
    };

    while (true) {
        n00b_memcheck_arena(&state);
        n00b_arena_t *new = state.cur_arena->successor;

        if (!new) {
            break;
        }

        state.err_in_arena = false;
        state.cur_arena    = new;
        state.alloc_ix     = 0;
        state.cur_alloc    = NULL;
        state.prev_alloc   = NULL;
        state.arena_count++;
    }

    if (state.total_errors) {
        fprintf(stderr,
                "Completed memcheck on heap %lld (%s; %s:%d) "
                "-- %d total errors\n",
                (long long int)h->heap_id,
                h->name,
                h->file,
                h->line,
                state.total_errors);

        fprintf(stderr,
                "======================================================\n");
    }
}
#endif
