#include "hatrack/witchhat.h" // IWYU pragma: export

enum64(witchhat_flag_t,
       WITCHHAT_F_MOVING   = 0x8000000000000000,
       WITCHHAT_F_MOVED    = 040000000000000000,
       WITCHHAT_F_INITED   = 0x2000000000000000,
       WITCHHAT_EPOCH_MASK = 0x1fffffffffffffff);

/* These need to be non-static because tophat and hatrack_dict both
 * need them, so that they can call in without a second call to
 * MMM. But, they should be considered "friend" functions, and not
 * part of the public API.
 *
 * Actually, hatrack_dict no longer uses Witchhat, it uses Crown, but
 * I'm going to explicitly leave these here, instead of going back to
 * making them static.
 */

extern hatrack_view_t *
witchhat_view_no_mmm(witchhat_t *, uint64_t *, bool);

extern witchhat_store_t *
witchhat_store_new(uint64_t size);

extern void *
witchhat_store_get(witchhat_store_t *, hatrack_hash_t, bool *);

extern void *
witchhat_store_put(witchhat_store_t *, mmm_thread_t *, witchhat_t *, hatrack_hash_t, void *, bool *, uint64_t);

extern void *
witchhat_store_replace(witchhat_store_t *, mmm_thread_t *, witchhat_t *, hatrack_hash_t, void *, bool *, uint64_t);

extern bool
witchhat_store_add(witchhat_store_t *, mmm_thread_t *, witchhat_t *, hatrack_hash_t, void *, uint64_t);

extern void *
witchhat_store_remove(witchhat_store_t *, mmm_thread_t *, witchhat_t *, hatrack_hash_t, bool *, uint64_t);
