/*
 * Copyright © 2021-2022 John Viega
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *  Name:           hatrack_common.h
 *  Description:    Data structures and utility functions used across all
 *                  our default hash tables.
 *
 *  Author:         John Viega, john@zork.org
 */

#pragma once
#include "base.h"
#include "mmm.h"

/* hatrack_hash_t
 *
 * The below type represents a hash value.
 *
 * We use 128-bit hash values and a universal hash function to make
 * accidental collisions so improbable, we can use hash equality as a
 * standin for identity, so that we never have to worry about
 * comparing keys.
 */

#ifndef NO___INT128_T

typedef __int128_t hatrack_hash_t;

static inline bool
hatrack_hashes_eq(hatrack_hash_t hv1, hatrack_hash_t hv2)
{
    return hv1 == hv2;
}

static inline bool
hatrack_hash_gt(hatrack_hash_t hv1, hatrack_hash_t hv2)
{
    return hv1 > hv2;
}

#else

typedef struct {
    // __int128_t is naturally aligned to 16 bytes, but this struct is not.
    // Force 16-byte alignment to match __int128_t.
    alignas(16)
        uint64_t w1;
    uint64_t w2;
} hatrack_hash_t;

static inline bool
hatrack_hashes_eq(hatrack_hash_t hv1, hatrack_hash_t hv2)
{
    return (hv1.w1 == hv2.w1) && (hv1.w2 == hv2.w2);
}

static inline bool
hatrack_hash_gt(hatrack_hash_t hv1, hatrack_hash_t hv2)
{
    if (hv1.w1 > hv2.w1) {
        return true;
    }

    if ((hv1.w1 == hv2.w1) && (hv1.w2 > hv2.w2)) {
        return true;
    }

    return false;
}

#endif // NO___INT128_T

/* hatrack_hash_func_t
 *
 * A generic type for function pointers to functions that hash for
 * us. This is used in the higher-level dict and set implementations.
 *
 * Note that several implementations of hash functions (using XXH3)
 * are in hash.h.
 */
typedef hatrack_hash_t (*hatrack_hash_func_t)(void *);

/* hatrack_mem_hook_t
 *
 * Function pointer to a function of two arguments. This is used for
 * memory management hooks in our higher-level dict and set
 * implementations:
 *
 * 1) A hook that runs to notify when a user-level record has been
 *    ejected from the hash table (either via over-write or
 *    deletion). Here, it's guaranteed that the associated record is
 *    no longer being accessed by other threads.
 *
 *    However, it says nothing about threads using records read from
 *    the hash table. Therefore, if you're using dynamic records, this
 *    hook should probably be used to decrement a reference count.
 *
 * 2) A hook that runs to notify when a user-level record is about to
 *    be returned.  This is primarily intended for clients to
 *    increment a reference count on objects being returned BEFORE the
 *    table would ever run the previous hook.
 *
 * The first argument will be a pointer to the data structure, and the
 * second argument will be a pointer to the item in question. In
 * dictionaries, if both a key and a value are getting returned
 * (through a call to dict_items() for example), then the callback
 * will get used once for the key, and once for the item.
 *
 * This type happens to have the same signature as a different memory
 * management hook that MMM uses, which is indeed used in implementing
 * these higher level hooks.  However, the meaning of the arguments is
 * different, so we've kept them distinct types.
 */
typedef void (*hatrack_mem_hook_t)(void *, void *);

/* hatrack_view_t
 *
 * All our calls to return a view of a hash table (generally for the
 * purposes of some kind of iteration) return the hatrack_view_t.
 *
 * When requested, views can be sorted by insertion time (to the best
 * of the algorithm's ability... some are better than others). The
 * semantics here is identical to Python dictionaries:
 *
 * 1) If { K : V1 } is in the table, and we write { K : V2 } over it,
 *    the new item in the 'replace' operation keeps the logical
 *    insertion order of the original item.
 *
 * 2) If { K : V1 } is removed, then a new item inserted with the same
 *    key (even { K : V1 }), the time of re-insertion is the new
 *    insertion time for the purposes of sort ordering.
 *
 * Our view operations all go through the hash table, pick out live
 * entries, and stick them in an array of hatrack_view_t objects, and
 * then sort that array (either via qsort, or in some cases an
 * insertion sort).
 *
 * While the end user probably doesn't need the sort epoch, we give it
 * to them anyway, instead of coping just the items post-sort.  We
 * even give it to them if we are NOT sorting.
 */
typedef struct {
    void   *item;
    int64_t sort_epoch;
} hatrack_view_t;

HATRACK_EXTERN void
hatrack_view_delete(hatrack_view_t *view, uint64_t num);

static inline uint64_t
hatrack_round_up_to_power_of_2(uint64_t n)
{
    // n & (n - 1) only returns 0 when n is a power of two.
    // If n's already a power of 2, we're done.
    if (!(n & (n - 1))) {
        return n;
    }

    // CLZ returns the number of zeros before the leading one.
    // The next power of two will have one fewer leading zero,
    // and that will be the only bit set.
    return 0x8000000000000000ull >> (__builtin_clzll(n) - 1);
}

static inline uint64_t
hatrack_round_up_to_given_power_of_2(uint64_t power, uint64_t n)
{
    uint64_t modulus   = (power - 1);
    uint64_t remainder = n & modulus;

    if (!remainder) {
        return n;
    }
    else {
        return (n & ~modulus) + power;
    }
}

static inline uint64_t
hatrack_int_log2(uint64_t n)
{
    return 63 - __builtin_clzll(n);
}

typedef void (*hatrack_panic_func)(void *arg, const char *msg);

[[noreturn]] HATRACK_EXTERN void
hatrack_panic(const char *msg);

HATRACK_EXTERN void
hatrack_setpanicfn(hatrack_panic_func panicfn, void *arg);

#ifndef HATRACK_NO_PTHREAD
#define hatrack_mutex_lock(_m)                          \
    ({                                                  \
        if (pthread_mutex_lock(_m)) {                   \
            hatrack_panic("pthread_mutex_lock failed"); \
        }                                               \
    })
#define hatrack_mutex_unlock(_m)                          \
    ({                                                    \
        if (pthread_mutex_unlock(_m)) {                   \
            hatrack_panic("pthread_mutex_unlock failed"); \
        }                                                 \
    })
#define hatrack_mutex_destroy(_m)                          \
    ({                                                     \
        if (pthread_mutex_destroy(_m)) {                   \
            hatrack_panic("pthread_mutex_destroy failed"); \
        }                                                  \
    })
#else
#define hatrack_mutex_lock(_m)    ({})
#define hatrack_mutex_unlock(_m)  ({})
#define hatrack_mutex_destroy(_m) ({})
#endif

/* For testing, and for our tophat implementation (which switches the
 * backend hash table out when it notices multiple writers), we keep
 * vtables of the operations to make it easier to switch between
 * different algorithms for testing. These types are aliases for the
 * methods that we expect to see.
 *
 * We use void * in the first parameter to all of these methods to
 * stand in for an arbitrary pointer to a hash table.
 */

// clang-format off
typedef void            (*hatrack_init_func)   (void *);
typedef void            (*hatrack_init_sz_func)(void *, char);
typedef void *          (*hatrack_get_func)    (void *, mmm_thread_t *, hatrack_hash_t, bool *);
typedef void *          (*hatrack_put_func)    (void *, mmm_thread_t *, hatrack_hash_t, void *, bool *);
typedef void *          (*hatrack_replace_func)(void *, mmm_thread_t *, hatrack_hash_t, void *, bool *);
typedef bool            (*hatrack_add_func)    (void *, mmm_thread_t *, hatrack_hash_t,	void *);
typedef void *          (*hatrack_remove_func) (void *, mmm_thread_t *, hatrack_hash_t,	bool *);
typedef void            (*hatrack_delete_func) (void *);
typedef uint64_t        (*hatrack_len_func)    (void *, mmm_thread_t *);
typedef hatrack_view_t *(*hatrack_view_func)   (void *, mmm_thread_t *, uint64_t *, bool);

typedef struct hatrack_vtable_st {
    hatrack_init_func    init;
    hatrack_init_sz_func init_sz;
    hatrack_get_func     get;
    hatrack_put_func     put;
    hatrack_replace_func replace;
    hatrack_add_func     add;
    hatrack_remove_func  remove;
    hatrack_delete_func  delete;
    hatrack_len_func     len;
    hatrack_view_func    view;
} hatrack_vtable_t;
