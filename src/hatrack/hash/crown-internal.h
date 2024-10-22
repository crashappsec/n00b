#include "hatrack/crown.h"

#ifdef HATRACK_32_BIT_HOP_TABLE

#define CROWN_HOME_BIT 0x80000000

typedef uint32_t hop_t;

#define CLZ(n) __builtin_clzl(n)

#else

#define CROWN_HOME_BIT 0x8000000000000000ULL

typedef uint64_t hop_t;

#define CLZ(n) __builtin_clzll(n)

#endif

enum64(crown_flag_t,
       CROWN_F_MOVING   = 0x8000000000000000ULL,
       CROWN_F_MOVED    = 0x4000000000000000ULL,
       CROWN_F_INITED   = 0x2000000000000000ULL,
       CROWN_EPOCH_MASK = 0x1fffffffffffffffULL);

/* These need to be non-static because tophat and hatrack_dict both
 * need them, so that they can call in without a second call to
 * MMM. But, they should be considered "friend" functions, and not
 * part of the public API.
 */

extern crown_store_t *
crown_store_new(uint64_t size);

extern void *
crown_store_get(crown_store_t *, hatrack_hash_t, bool *);

extern void *
crown_store_put(crown_store_t *, mmm_thread_t *, crown_t *, hatrack_hash_t, void *, bool *, uint64_t);

extern void *
crown_store_replace(crown_store_t *, mmm_thread_t *, crown_t *, hatrack_hash_t, void *, bool *, uint64_t);

extern bool
crown_store_add(crown_store_t *, mmm_thread_t *, crown_t *, hatrack_hash_t, void *, uint64_t);

extern void *
crown_store_remove(crown_store_t *, mmm_thread_t *, crown_t *, hatrack_hash_t, bool *, uint64_t);
