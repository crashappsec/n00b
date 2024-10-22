#include "hatrack/hatrack_common.h"
#include "hatrack/counters.h"
#include "hatrack/mmm.h"

/* These inline functions are used across all the hatrack
 * implementations.
 */

static inline uint64_t
hatrack_compute_table_threshold(uint64_t size)
{
    /* size - (size >> 2) calculates 75% of size (100% - 25%).
     *
     * When code checks to see if the current store has hit its
     * threshold,, implementations generally are adding to the used
     * count, and do atomic_fetch_add(), which returns the original
     * value.
     *
     * So, when checking if used_count >= 75%, the -1 gets us to 75%,
     * otherwise we're resizing at one item more full than 75%.
     *
     * That in itself is not a big deal, of course. And if there's
     * anywhere that we check to resize where we're not also reserving
     * a bucket, we would resize an item too early, but again, so what
     * and who cares.
     *
     * Frankly, if I didn't want to be able to just say "75%" without
     * an asterisk, I'd just drop the -1.
     */
    return size - (size >> 2) - 1;
}

/*
 * We always perform a migration when the number of buckets used is
 * 75% of the total number of buckets in the current store. But, we
 * reserve buckets in a store, even if those items are deleted, so the
 * number of actual items in a table could be small when the store is
 * getting full (never changing the hash in a store's bucket makes our
 * lives MUCH easier given parallel work being one in the table).
 *
 * This function figures out, when it's time to migrate, what the new
 * size of the table should be. Our metric here is:
 *
 *  1) If the CURRENT table was at least 50% full, we double the new
 *     table size.
 *
 *  2) If the CURRENT table was up to 25% full, we HALF the new table
 *     size.
 *
 *  3) Otherwise, we leave the table size the same.
 *
 *  Also, we never let the table shrink TOO far... which we base on
 *  the preprocessor variable HATRACK_MIN_SIZE.
 */
static inline uint64_t
hatrack_new_size(uint64_t last_bucket, uint64_t size)
{
    uint64_t table_size = last_bucket + 1;

    if (size >= table_size >> 1) {
        return table_size << 1;
    }
    // We will never bother to size back down to the smallest few
    // table sizes.
    if (size <= (HATRACK_MIN_SIZE << 2)) {
        HATRACK_CTR(HATRACK_CTR_STORE_SHRINK);
        return HATRACK_MIN_SIZE << 3;
    }
    if (size <= (table_size >> 2)) {
        HATRACK_CTR(HATRACK_CTR_STORE_SHRINK);
        return table_size >> 1;
    }

    return table_size;
}

/* Since we use 128-bit hash values, we can safely use the null hash
 * value to mean "unreserved" (and even "empty" in our locking
 * tables).
 */

#ifndef NO___INT128_T

static inline bool
hatrack_bucket_unreserved(hatrack_hash_t hv)
{
    return !hv;
}

#else
static inline bool
hatrack_bucket_unreserved(hatrack_hash_t hv)
{
    return !hv.w1 && !hv.w2;
}

#endif

/*
 * Calculates the starting bucket that a hash value maps to, given the
 * table size.  This is exactly the hash value modulo the table size.
 *
 * Since our table sizes are a power of two, "x % y" gives the same
 * result as "x & (y-1)", but the later is appreciably faster on most
 * architectures (probably on all architectures, since the processor
 * basically gets to skip an expensive division). And it gets run
 * enough that it could matter a bit.
 *
 * Also, since our table sizes will never get to 2^64 (and since we
 * can use the bitwise AND due to the power of two table size), when
 * we don't have a native 128-bit type, we only need to look at one of
 * the two 64-bit chunks in the hash (conceptually the one we look at
 * we consider the most significant chunk).
 */

#ifndef NO___INT128_T

static inline uint64_t
hatrack_bucket_index(hatrack_hash_t hv, uint64_t last_slot)
{
    return hv & last_slot;
}

#else

static inline uint64_t
hatrack_bucket_index(hatrack_hash_t hv, uint64_t last_slot)
{
    return hv.w1 & last_slot;
}

#endif

#ifndef NO___INT128_T

static inline void
hatrack_bucket_initialize(hatrack_hash_t *hv)
{
    *hv = 0;
}

#else

static inline void
hatrack_bucket_initialize(hatrack_hash_t *hv)
{
    hv->w1 = 0;
    hv->w2 = 0;
}
#endif

/* These are just basic bitwise operations, but performing them on
 * pointers requires some messy casting.  These inline functions just
 * hide the casting for us, improving code readability.
 */
static inline int64_t
hatrack_pflag_test(void *ptr, uint64_t flags)
{
    return ((int64_t)ptr) & flags;
}

static inline void *
hatrack_pflag_set(void *ptr, uint64_t flags)
{
    return (void *)(((uint64_t)ptr) | flags);
}

static inline void *
hatrack_pflag_clear(void *ptr, uint64_t flags)
{
    return (void *)(((uint64_t)ptr) & ~flags);
}

static inline void *
hatrack_found(bool *found, void *ret)
{
    if (found) {
        *found = true;
    }

    return ret;
}

static inline void *
hatrack_not_found(bool *found)
{
    if (found) {
        *found = false;
    }

    return NULL;
}

static inline void *
hatrack_found_w_mmm(mmm_thread_t *thread, bool *found, void *ret)
{
    mmm_end_op(thread);
    return hatrack_found(found, ret);
}

static inline void *
hatrack_not_found_w_mmm(mmm_thread_t *thread, bool *found)
{
    mmm_end_op(thread);
    return hatrack_not_found(found);
}

typedef struct
{
    uint64_t h;
    uint64_t l;
} generic_2x64_t;

typedef union {
    generic_2x64_t      st;
    _Atomic __uint128_t atomic_num;
    __uint128_t         num;
} generic_2x64_u;

static inline generic_2x64_u
hatrack_or2x64(generic_2x64_u *s1, generic_2x64_u s2)
{
    return (generic_2x64_u)atomic_fetch_or(&s1->atomic_num, s2.num);
}

static inline generic_2x64_u
hatrack_or2x64l(generic_2x64_u *s1, uint64_t l)
{
    generic_2x64_u n = {.st = {.h = 0, .l = l}};

    return (generic_2x64_u)atomic_fetch_or(&s1->atomic_num, n.num);
}

static inline generic_2x64_u
hatrack_or2x64h(generic_2x64_u *s1, uint64_t h)
{
    generic_2x64_u n = {.st = {.h = h, .l = 0}};

    return (generic_2x64_u)atomic_fetch_or(&s1->atomic_num, n.num);
}

#define OR2X64(s1, s2)  hatrack_or2x64((generic_2x64_u *)(s1), s2)
#define OR2X64L(s1, s2) hatrack_or2x64l((generic_2x64_u *)(s1), s2)
#define OR2X64H(s1, s2) hatrack_or2x64h((generic_2x64_u *)(s1), s2)
#define ORPTR(s1, s2)   atomic_fetch_or((_Atomic uint64_t *)(s1), s2)

#define hatrack_cell_alloc(container_type, cell_type, n) \
    (container_type *)hatrack_zalloc(sizeof(container_type) + sizeof(cell_type) * n)

int hatrack_quicksort_cmp(const void *, const void *);
