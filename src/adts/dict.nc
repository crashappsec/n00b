/*
 * This is a new hash table specifically for n00b. It's *NOT*
 * lock-free; it instead keeps a per-bucket mutex (implemented as a
 * spin lock). I do this because we store both key and value in the
 * payload; this allows us to store both comfortably without having to
 * worry about keeping the atomic payload state below 128 bits.
 *
 * That not only helps us avoid unnecessary heap allocations, it makes
 * it much easier to manage storage when we have startup items, or
 * items that shouldn't be collected.
 *
 * Currently, it foregoes the extra machinations of the neighborhood
 * map that it used to have (and the hatrack version still has). We
 * also forego the table migration strategy. Instead, when a migration
 * occurs, one thread acquires the migration futex.
 *
 * That thread marks individual buckets as locked for migration,
 * before proceeding. If, when copying, it happens upon a bucket whose
 * write is in progress, it busy-waits.
 *
 * If threads notice a migration going on, they wait on the migration
 * futex, which gets notified when the migration is complete.
 *
 *  Author:         John Viega, john@crashoverride.com
 *
 */
#define N00B_USE_INTERNAL_API
#include "n00b.h"

__int128_t
n00b_cstring_hash(char *s)
{
    union {
        __int128_t    i;
        XXH128_hash_t x;
    } hv;

    hv.x = XXH3_128bits(s, strlen(s));

    return hv.i;
}

__int128_t
n00b_word_hash(uint64_t w)
{
    union {
        __int128_t    i;
        XXH128_hash_t x;
    } hv;

    hv.x = XXH3_128bits(&w, sizeof(uint64_t));

    return hv.i;
}

__int128_t
n00b_string_hash(n00b_string_t *s)
{
    if (!s || !s->u8_bytes) {
        return n00b_word_hash(0ULL);
    }

    union {
        __int128_t    i;
        XXH128_hash_t x;
    } hv;

    hv.x = XXH3_128bits(s->data, s->u8_bytes);

    return hv.i;
}

static inline __int128_t
hash_object(n00b_dict_t *d, void *key)
{
    n00b_alloc_hdr *hdr = NULL;

    if (d->non_object_keys) {
        return (*d->fn)(key);
    }

    if (n00b_in_heap(key)) {
        hdr = (void *)key;
        hdr--;

        if (hdr->guard == n00b_gc_guard) {
            if (hdr->cached_hash) {
                return hdr->cached_hash;
            }
            if (hdr->type) {
                n00b_vtable_t *vt = n00b_vtable(key);
                n00b_hash_fn   fn = (void *)vt->methods[N00B_BI_HASH];
                if (fn) {
                    hdr->cached_hash = (*fn)(key);
                }
                else {
                    hdr->cached_hash = n00b_word_hash((int64_t)key);
                }

                return hdr->cached_hash;
            }
        }
    }

    if (d->fn) {
        return (*d->fn)(key);
    }
    else {
        return n00b_word_hash((int64_t)key);
    }
}

// 75%
static inline uint32_t
resize_threshold(uint32_t size)
{
    return size - (size >> 2) - 1;
}

static inline int
bucket_reserved(n00b_dict_bucket_t *b)
{
    return (int)b->hv != 0;
}

static inline int
bucket_deleted(n00b_dict_bucket_t *b)
{
    return (atomic_read(&b->flags) & N00B_HT_FLAG_DELETED) != 0;
}

static inline uint32_t
new_dict_size(uint32_t last_bucket, uint32_t size)
{
    uint32_t table_size = last_bucket + 1;

    if (size >= table_size >> 1) {
        return table_size << 1;
    }
    // We will never bother to size back down to the smallest few
    // table sizes.
    if (size <= (N00B_DICT_MIN_SIZE << 2)) {
        return N00B_DICT_MIN_SIZE << 3;
    }
    if (size <= (table_size >> 2)) {
        return table_size >> 1;
    }

    return table_size;
}

static inline n00b_dict_store_t *
new_dict_store(n00b_dict_t *d, uint32_t alloc_items)
{
    n00b_dict_store_t *result;

    if (d->use_mmap) {
        int sz = alloc_items * sizeof(n00b_dict_bucket_t)
               + sizeof(n00b_dict_store_t);

        result          = mmap(NULL,
                      sz,
                      PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANON,
                      -1,
                      0);
        result->mmap_sz = sz;
        if (!d->no_heap_data) {
            n00b_heap_register_dynamic_root(NULL, result, sz / sizeof(void *));
        }
    }

    else {
        result = n00b_gc_flex_alloc(n00b_dict_store_t,
                                    n00b_dict_bucket_t,
                                    alloc_items,
                                    N00B_GC_SCAN_ALL);
    }
    result->last_slot = alloc_items - 1;
    result->threshold = resize_threshold(alloc_items);

    return result;
}

bool
n00b_dict_lock(n00b_dict_t *d, bool try, uint32_t *count)
{
    uint32_t flags    = N00B_HT_FLAG_COPYING;
    uint32_t new_used = 0;

    if (try) {
        flags |= N00B_HT_FLAG_MOVING;
    }
    else {
        atomic_fetch_add(&d->wait_ct, 1);
    }

    uint32_t v = atomic_fetch_or(&d->futex, 1UL << 31);

    while (v & (1UL << 31)) {
        if (try) {
            return false;
        }
        n00b_futex_wait_timespec(&d->futex, v, NULL);
        v = atomic_fetch_or(&d->futex, 1UL << 31);
    }

    atomic_fetch_add(&d->wait_ct, -1);

    n00b_dict_store_t  *s = atomic_read(&d->store);
    n00b_dict_bucket_t *b;

    int first_active = -1;
    int last_active  = -1;

    for (uint32_t i = 0; i <= s->last_slot; i++) {
        b = &s->buckets[i];

        uint32_t f = atomic_fetch_or(&b->flags, flags);

        new_used += bucket_reserved(b) & !bucket_deleted(b);

        if (f & N00B_HT_FLAG_MUTEX) {
            last_active = i;
            if (first_active == -1) {
                first_active = i;
            }
        }
    }

    // If we noticed writes in progress, go through the range of the
    // store that contained threads, and busy-wait if needed.
    if (last_active != -1) {
        for (int i = first_active; i <= last_active; i++) {
            while (atomic_read(&s->buckets[i].flags) & N00B_HT_FLAG_MUTEX) {
            }
        }
    }

    *count = new_used;

    return true;
}

static void
dict_unlock_post_migrate(n00b_dict_t *d, n00b_dict_store_t *s)
{
    if (d->use_mmap) {
        n00b_dict_store_t *old = atomic_read(&d->store);
        atomic_store(&d->store, s);
        atomic_store(&d->futex, 0);
        n00b_heap_remove_root(NULL, old);
        munmap(old, old->mmap_sz);
        n00b_restart_the_world();
    }
    else {
        atomic_store(&d->store, s);
        atomic_store(&d->futex, 0);

        if (atomic_read(&d->wait_ct)) {
            n00b_futex_wake(&d->futex, true);
        }
    }
}

void
n00b_dict_unlock_post_copy(n00b_dict_t *d)
{
    n00b_dict_store_t *s = atomic_read(&d->store);

    for (uint32_t i = 0; i <= s->last_slot; i++) {
        atomic_fetch_and(&s->buckets[i].flags, ~N00B_HT_FLAG_COPYING);
    }

    atomic_store(&d->futex, 0);

    if (atomic_read(&d->wait_ct)) {
        n00b_futex_wake(&d->futex, true);
    }
}

static void
n00b_dict_migrate(n00b_dict_t *d)
{
    uint32_t            nitems = 0;
    n00b_dict_store_t  *olds;
    n00b_dict_bucket_t *bold;

    if (d->use_mmap) {
        bool lost = false;

        while (true) {
            uint32_t v = atomic_fetch_or(&d->futex, 1UL << 31);
            if (v & (1UL << 31)) {
                lost = true;
                n00b_thread_checkin();
                continue;
            }
            break;
        }

        if (lost) {
            return;
        }

        n00b_stop_the_world();
        olds = atomic_read(&d->store);
        for (uint32_t i = 0; i <= olds->last_slot; i++) {
            bold = &olds->buckets[i];
            if (bucket_reserved(bold) && !bucket_deleted(bold)) {
                nitems++;
            }
        }
    }
    else {
        if (!n00b_dict_lock(d, true, &nitems)) {
            atomic_fetch_add(&d->wait_ct, 1);
            n00b_futex_wait_for_value(&d->futex, 0);
            return;
        }
        olds = atomic_read(&d->store);
    }

    uint32_t           alloc_items = new_dict_size(olds->last_slot, nitems);
    n00b_dict_store_t *news        = new_dict_store(d, alloc_items);
    uint32_t           last        = news->last_slot;
    uint32_t           bix;

    atomic_store(&news->used_count, nitems);

    for (uint32_t i = 0; i <= olds->last_slot; i++) {
        bold = &olds->buckets[i];
        if (!bucket_reserved(bold) || bucket_deleted(bold)) {
            continue;
        }

        bix = bold->hv & last;

        for (uint32_t j = 0; j <= last; j++) {
            n00b_dict_bucket_t *bnew = &news->buckets[bix];

            if (!bucket_reserved(bnew)) {
                bnew->hv           = bold->hv;
                bnew->key          = bold->key;
                bnew->value        = bold->value;
                bnew->insert_order = bold->insert_order;
                break;
            }

            bix = (bix + 1) & news->last_slot;
        }
    }

    dict_unlock_post_migrate(d, news);
}

n00b_dict_raw_item_t *
n00b_dict_raw_view(n00b_dict_t *d, uint32_t *n)
{
    // Views always use regular allocator.
    n00b_dict_raw_item_t *result;
    n00b_dict_store_t    *s;
    int32_t               items;
    int32_t               ix = 0;

    n00b_dict_lock(d, false, n);
    s     = d->store;
    items = *n;

    if (!items) {
        n00b_dict_unlock_post_copy(d);
        return NULL;
    }

    result = n00b_gc_array_alloc(n00b_dict_raw_item_t, items);

    for (uint32_t i = 0; i <= s->last_slot; i++) {
        n00b_dict_bucket_t *b = &s->buckets[i];

        if (!bucket_reserved(b) || bucket_deleted(b)) {
            continue;
        }
        result[ix++] = (n00b_dict_raw_item_t){
            .key   = b->key,
            .value = b->value,
            .order = b->insert_order,
        };
    }

    n00b_dict_unlock_post_copy(d);

    return result;
}

int64_t *
n00b_dict_items_array(n00b_dict_t *d, uint32_t *n)
{
    n00b_dict_raw_item_t *view   = n00b_dict_raw_view(d, n);
    int64_t              *result = (void *)view;
    int64_t              *p      = &result[2];
    int32_t               len    = *n;

    for (int i = 1; i < len; i++) {
        n00b_dict_raw_item_t item = view[i];
        *p++                      = (int64_t)item.key;
        *p++                      = (int64_t)item.value;
    }

    return result;
}

static int
dict_bucket_cmp(const void *bucket1, const void *bucket2)
{
    n00b_dict_raw_item_t *item1 = (void *)bucket1;
    n00b_dict_raw_item_t *item2 = (void *)bucket2;

    return item1->order - item2->order;
}

n00b_list_t *
_n00b_dict_keys(n00b_dict_t *d, ...)
{
    keywords
    {
        bool sort = true;
    }

    uint32_t              n;
    n00b_dict_raw_item_t *arr       = n00b_dict_raw_view(d, &n);
    n00b_ntype_t          dict_type = n00b_get_my_type(d);
    n00b_ntype_t          key_type  = n00b_type_get_param(dict_type, 0);
    n00b_list_t          *result    = n00b_new(n00b_type_list(key_type),
                                   length : n);

    if (!arr) {
        return result;
    }

    if (sort) {
        qsort(arr, n, sizeof(n00b_dict_raw_item_t), dict_bucket_cmp);
    }

    for (uint32_t i = 0; i < n; i++) {
        result->data[i] = arr[i].key;
    }

    return result;
}

n00b_list_t *
_n00b_dict_values(n00b_dict_t *d, ...)
{
    keywords
    {
        bool sort = true;
    }

    uint32_t              n;
    n00b_dict_raw_item_t *arr       = n00b_dict_raw_view(d, &n);
    n00b_ntype_t          dict_type = n00b_get_my_type(d);
    n00b_ntype_t          val_type  = n00b_type_get_param(dict_type, 1);
    n00b_list_t          *result    = n00b_new(n00b_type_list(val_type),
                                   length : n);

    if (!arr) {
        return result;
    }

    if (sort) {
        qsort(arr, n, sizeof(n00b_dict_raw_item_t), dict_bucket_cmp);
    }

    for (uint32_t i = 0; i < n; i++) {
        result->data[i] = arr[i].value;
    }

    return result;
}

n00b_list_t *
_n00b_dict_items(n00b_dict_t *d, ...)
{
    keywords
    {
        bool sort = true;
    }

    uint32_t              n;
    n00b_dict_raw_item_t *arr       = n00b_dict_raw_view(d, &n);
    n00b_ntype_t          dict_type = n00b_get_my_type(d);
    n00b_list_t          *params    = n00b_type_all_params(dict_type);
    n00b_ntype_t          tt        = n00b_type_tuple(params);
    n00b_list_t          *result    = n00b_new(n00b_type_list(tt), length : n);

    if (!arr) {
        return result;
    }

    if (sort) {
        qsort(arr, n, sizeof(n00b_dict_raw_item_t), dict_bucket_cmp);
    }

    for (uint32_t i = 0; i < n; i++) {
        n00b_tuple_t *t = n00b_new(tt);
        t->items[0]     = arr[i].key;
        t->items[1]     = arr[i].value;
        result->data[i] = (void *)t;
    }

    return result;
}

// Gives us the correct bucket if and only if the key is found in the
// table already. It can return a bucket where the item has been deleted,
// so the value needs to be checked.

static inline n00b_dict_bucket_t *
n00b_acquire_if_present(n00b_dict_t       *d,
                        n00b_dict_store_t *store,
                        __int128_t         hv)
{
    uint32_t            last_slot;
    uint32_t            bix;
    uint32_t            flags;
    n00b_dict_bucket_t *cur;
    bool                miss = false;

    do {
        last_slot = store->last_slot;
        bix       = hv & last_slot;

        for (uint32_t i = 0; i <= last_slot; i++) {
            cur = &store->buckets[bix];

            do {
                flags = atomic_fetch_or(&cur->flags, N00B_HT_FLAG_MUTEX);
                if (flags & N00B_HT_FLAG_MOVING) {
                    goto try_again;
                }
            } while (flags & N00B_HT_FLAG_MUTEX);

            if (cur->hv == hv) {
                // Keep it locked.
                // Note: We don't check for deletion here.
                return cur;
            }
            if (!bucket_reserved(cur)) {
                miss = true;
            }

            bix   = (bix + 1) & last_slot;
            flags = atomic_fetch_and(&cur->flags, ~N00B_HT_FLAG_MUTEX);

            if (miss) {
                return NULL;
            }
        }

        return NULL;

try_again:
        n00b_futex_wait_for_value(&d->futex, ~0);
        store = atomic_read(&d->store);
    } while (true);
}

static inline n00b_dict_bucket_t *
n00b_acquire_or_add(n00b_dict_t *d, n00b_dict_store_t *store, __int128_t hv)
{
    uint32_t            last_slot;
    uint32_t            bix;
    uint32_t            flags;
    n00b_dict_bucket_t *cur;

    do {
        last_slot = store->last_slot;
        bix       = hv & last_slot;

        for (uint32_t i = 0; i <= last_slot; i++) {
            cur = &store->buckets[bix];

            do {
                flags = atomic_fetch_or(&cur->flags, N00B_HT_FLAG_MUTEX);
                if (flags & (N00B_HT_FLAG_COPYING)) {
                    goto try_again;
                }
            } while (flags & N00B_HT_FLAG_MUTEX);

            if (cur->hv == hv || !bucket_reserved(cur)) {
                // Keep it locked.
                //
                // We don't add in the hash value if it's not present;
                // that way the caller will know to do whatever
                // accounting needs to be done.

                return cur;
            }

            bix   = (bix + 1) & last_slot;
            flags = atomic_fetch_and(&cur->flags, ~N00B_HT_FLAG_MUTEX);
        }

        return NULL;

try_again:
        n00b_futex_wait_for_value(&d->futex, ~0);
        store = atomic_read(&d->store);
    } while (true);
}

static inline void
unlock_bucket(n00b_dict_bucket_t *b)
{
    atomic_fetch_and(&b->flags, ~N00B_HT_FLAG_MUTEX);
}

// Returns the old value if found, NULL otherwise.
void *
_n00b_dict_put(n00b_dict_t *d, void *key, void *value)
{
    __int128_t         hv     = hash_object(d, key);
    n00b_dict_store_t *store  = atomic_read(&d->store);
    void              *result = NULL;
try_again:
    n00b_dict_bucket_t *bucket      = n00b_acquire_or_add(d, store, hv);
    bool                reset_epoch = false;

    if (!bucket->hv) {
        if (atomic_fetch_add(&store->used_count, 1) >= store->threshold) {
            unlock_bucket(bucket);
            n00b_dict_migrate(d);
            store = atomic_read(&d->store);
            goto try_again;
        }
        reset_epoch = true;
        bucket->hv  = hv;
    }
    else {
        if (bucket_deleted(bucket)) {
            reset_epoch = true;
            bucket->flags &= ~N00B_HT_FLAG_DELETED;
        }
        else {
            result = bucket->value;
        }
    }

    if (reset_epoch) {
        bucket->insert_order
            = (uint32_t)atomic_fetch_add(&store->used_count, 1);
        atomic_fetch_add(&d->length, 1);
    }

    bucket->key   = key;
    bucket->value = value;
    unlock_bucket(bucket);

    return result;
}

void *
_n00b_dict_get(n00b_dict_t *d, void *key, bool *found)
{
    __int128_t          hv    = hash_object(d, key);
    n00b_dict_store_t  *store = atomic_read(&d->store);
    n00b_dict_bucket_t *b     = n00b_acquire_if_present(d, store, hv);
    void               *result;

    if (!b) {
        if (found) {
            *found = false;
        }

        return NULL;
    }
    if (bucket_deleted(b)) {
        if (found) {
            *found = false;
        }
        unlock_bucket(b);
        return NULL;
    }

    if (found) {
        *found = true;
    }

    result = b->value;

    unlock_bucket(b);

    return result;
}

bool
_n00b_dict_replace(n00b_dict_t *d, void *key, void *value)
{
    __int128_t          hv    = hash_object(d, key);
    n00b_dict_store_t  *store = atomic_read(&d->store);
    n00b_dict_bucket_t *b     = n00b_acquire_if_present(d, store, hv);

    if (!b) {
        return false;
    }
    if (!bucket_reserved(b) || bucket_deleted(b)) {
        unlock_bucket(b);
        return false;
    }

    b->value = value;
    unlock_bucket(b);
    return true;
}

bool
_n00b_dict_add(n00b_dict_t *d, void *key, void *value)
{
    __int128_t         hv    = hash_object(d, key);
    n00b_dict_store_t *store = atomic_read(&d->store);
try_again:
    n00b_dict_bucket_t *bucket = n00b_acquire_or_add(d, store, hv);

    if (!bucket->hv) {
        if (atomic_fetch_add(&store->used_count, 1) >= store->threshold) {
            unlock_bucket(bucket);
            n00b_dict_migrate(d);
            store = atomic_read(&d->store);
            goto try_again;
        }
        bucket->hv = hv;
    }
    else {
        if (bucket_deleted(bucket)) {
            bucket->flags &= ~N00B_HT_FLAG_DELETED;
        }
        else {
            unlock_bucket(bucket);
            return false;
        }
    }

    bucket->insert_order = (uint32_t)atomic_fetch_add(&store->used_count,
                                                      1);

    bucket->key   = key;
    bucket->value = value;

    atomic_fetch_add(&d->length, 1);
    unlock_bucket(bucket);

    return true;
}

bool
_n00b_dict_remove(n00b_dict_t *d, void *key)
{
    __int128_t          hv    = hash_object(d, key);
    n00b_dict_store_t  *store = atomic_read(&d->store);
    n00b_dict_bucket_t *b     = n00b_acquire_if_present(d, store, hv);

    if (!b) {
        return false;
    }
    if (!bucket_reserved(b) || bucket_deleted(b)) {
        unlock_bucket(b);
        return false;
    }

    b->value = NULL;
    b->flags |= N00B_HT_FLAG_DELETED;
    atomic_fetch_add(&d->length, -1);
    unlock_bucket(b);

    return true;
}

bool
_n00b_dict_cas(n00b_dict_t *d,
               void        *key,
               void       **old_item_ptr,
               void        *new_item,
               bool         null_old_means_absence,
               bool         null_new_means_delete)
{
    __int128_t          hv           = hash_object(d, key);
    n00b_dict_store_t  *store        = atomic_read(&d->store);
    void               *old_item     = old_item_ptr ? *old_item_ptr : NULL;
    bool                expect_empty = !old_item && null_old_means_absence;
    bool                delete_it    = !new_item && null_new_means_delete;
    n00b_dict_bucket_t *b;

    if (expect_empty) {
try_again:
        b = n00b_acquire_or_add(d, store, hv);
        if (bucket_reserved(b) && !bucket_deleted(b)) {
            if (old_item_ptr) {
                *old_item_ptr = b->value;
            }
            unlock_bucket(b);
            return false;
        }

        if (!bucket_deleted(b)) {
            if (atomic_fetch_add(&store->used_count, 1)
                >= store->threshold) {
                unlock_bucket(b);
                n00b_dict_migrate(d);
                store = atomic_read(&d->store);
                goto try_again;
            }
        }

        b->hv = hv;
        b->flags &= ~N00B_HT_FLAG_DELETED;
        b->value        = new_item;
        b->insert_order = (uint32_t)atomic_fetch_add(&store->used_count, 1);
        atomic_fetch_add(&d->length, 1);
        unlock_bucket(b);

        return true;
    }
    else {
        b = n00b_acquire_if_present(d, store, hv);

        if (!b) {
            return false;
        }
        if (b->value != old_item) {
            *old_item_ptr = b->value;
            unlock_bucket(b);
            return false;
        }

        if (delete_it) {
            b->value = NULL;
            b->flags |= N00B_HT_FLAG_DELETED;
            atomic_fetch_add(&d->length, -1);
        }
        else {
            b->value = new_item;
        }
        unlock_bucket(b);
        return true;
    }
}

void
_n00b_dict_init(n00b_dict_t *dict, ...)
{
    // This is also the set initializer now.
    keywords
    {
        uint32_t num_entries  = N00B_DICT_MIN_SIZE;
        bool     system_dict  = false;
        bool     cstring_keys = false;
        bool     hide_from_gc = false;
    }

    if (num_entries < N00B_DICT_MIN_SIZE) {
        num_entries = N00B_DICT_MIN_SIZE;
    }
    else {
        uint32_t bitdiff = (num_entries & (num_entries - 1));

        if (bitdiff) {
            num_entries = 1 << (n00b_int_log2(num_entries) + 1);
        }
    }

    dict->use_mmap     = system_dict;
    dict->no_heap_data = hide_from_gc;
    dict->store        = new_dict_store(dict, num_entries);

    if (cstring_keys) {
        dict->fn              = (void *)n00b_cstring_hash;
        dict->non_object_keys = true;
        return;
    }

    assert(!hide_from_gc || system_dict);

    n00b_ntype_t t = n00b_type_get_param(n00b_get_my_type(dict), 0);

    if (n00b_type_is_value_type(t)) {
        dict->non_object_keys = true;
        dict->fn              = (void *)n00b_word_hash;
    }
}

n00b_dict_t *
n00b_new_unmanaged_dict(void)
{
    n00b_dict_t *result = n00b_gc_alloc_mapped(n00b_dict_t, N00B_GC_SCAN_ALL);
    n00b_dict_init(result, num_entries : 16, system_dict : true);

    return result;
}

n00b_string_t *
n00b_dict_to_string(n00b_dict_t *d)
{
    n00b_list_t   *view  = n00b_dict_items(d);
    int            n     = n00b_list_len(view);
    n00b_list_t   *items = n00b_list(n00b_type_string());
    n00b_tuple_t  *tup;
    n00b_string_t *s;

    for (int i = 0; i < n; i++) {
        tup                = n00b_list_get(view, i, NULL);
        n00b_string_t *s1  = n00b_tuple_get(tup, 0);
        n00b_string_t *s2  = n00b_tuple_get(tup, 1);
        int            len = s1->u8_bytes + s2->u8_bytes + 3;
        char           buf[len + 1];
        char          *p = &buf[0];

        memcpy(p, s1->data, s1->u8_bytes);
        p += s1->u8_bytes;
        *p++ = ' ';
        *p++ = ':';
        *p++ = ' ';
        memcpy(p, s2->data, s2->u8_bytes);
        *p++ = 0;

        s = n00b_cstring(buf);
        n00b_list_append(items, s);
    }

    s = n00b_string_join(items, n00b_cached_comma_padded());
    char  buf[s->u8_bytes + 3];
    char *p = &buf[0];

    *p++ = '{';
    memcpy(p, s->data, s->u8_bytes);
    *p++ = '}';
    *p   = 0;

    return n00b_cstring(buf);
}

void
n00b_dict_init_valist(n00b_dict_t *dict, va_list args)
{
    n00b_va_t va_list = n00b_va_list_to_n00b_va(args, false);

    if (!va_list) {
        _n00b_dict_init(dict, NULL);
    }

    n00b_karg_info_t kinfo = {
        .num_provided = n00b_list_len(va_list),
        .args         = (void *)((n00b_list_t *)va_list)->data,
    };

    _n00b_dict_init(dict, &kinfo);
}

static bool
dict_can_coerce_to(n00b_ntype_t my_type, n00b_ntype_t dst_type)
{
    return n00b_types_are_compat(my_type, dst_type, NULL);
}

static n00b_dict_t *
dict_coerce_to(n00b_dict_t *d, n00b_ntype_t dst_type)
{
    n00b_list_t *view   = n00b_dict_items(d);
    int          n      = n00b_list_len(view);
    n00b_dict_t *result = n00b_new(dst_type);

    for (int i = 0; i < n; i++) {
        n00b_tuple_t *tup      = n00b_list_get(view, i, NULL);
        void         *key_copy = n00b_copy(n00b_tuple_get(tup, 0));
        void         *val_copy = n00b_copy(n00b_tuple_get(tup, 1));

        n00b_dict_put(result, key_copy, val_copy);
    }

    return result;
}

n00b_dict_t *
n00b_dict_shallow_copy(n00b_dict_t *d)
{
    n00b_list_t *view   = n00b_dict_items(d);
    int          n      = n00b_list_len(view);
    n00b_dict_t *result = n00b_new(n00b_get_my_type(d));

    for (int i = 0; i < n; i++) {
        n00b_tuple_t *tup = n00b_list_get(view, i, NULL);
        void         *key = n00b_tuple_get(tup, 0);
        void         *val = n00b_tuple_get(tup, 1);

        n00b_dict_put(result, key, val);
    }

    return result;
}

int64_t
n00b_dict_len(n00b_dict_t *d)
{
    return atomic_read(&d->length);
}

n00b_dict_t *
n00b_dict_plus(n00b_dict_t *d1, n00b_dict_t *d2)
{
    n00b_list_t *v1     = n00b_dict_items(d1);
    n00b_list_t *v2     = n00b_dict_items(d2);
    int32_t      n1     = n00b_list_len(v1);
    int32_t      n2     = n00b_list_len(v2);
    n00b_dict_t *result = n00b_new(n00b_get_my_type(d1));

    for (int i = 0; i < n1; i++) {
        n00b_tuple_t *tup = n00b_list_get(v1, i, NULL);
        void         *key = n00b_tuple_get(tup, 0);
        void         *val = n00b_tuple_get(tup, 1);

        n00b_dict_put(result, key, val);
    }

    for (int i = 0; i < n2; i++) {
        n00b_tuple_t *tup = n00b_list_get(v2, i, NULL);
        void         *key = n00b_tuple_get(tup, 0);
        void         *val = n00b_tuple_get(tup, 1);

        n00b_dict_put(result, key, val);
    }

    return result;
}

n00b_dict_t *
n00b_dict_copy(n00b_dict_t *d)
{
    return dict_coerce_to(d, n00b_get_my_type(d));
}

static void *
dict_get(n00b_dict_t *d, void *k)
{
    return n00b_dict_get(d, k, NULL);
}

static n00b_dict_t *
to_dict_lit(n00b_ntype_t objtype, n00b_list_t *items, n00b_string_t *lm)
{
    uint64_t     n      = n00b_list_len(items);
    n00b_dict_t *result = n00b_new(objtype);

    for (unsigned int i = 0; i < n; i++) {
        n00b_tuple_t *tup = n00b_list_get(items, i, NULL);
        n00b_dict_put(result,
                      n00b_tuple_get(tup, 0),
                      n00b_tuple_get(tup, 1));
    }

    return result;
}

const n00b_vtable_t n00b_dict_vtable = {
    .methods = {
        [N00B_BI_CONSTRUCTOR]   = (n00b_vtable_entry)n00b_dict_init_valist,
        [N00B_BI_TO_STRING]     = (n00b_vtable_entry)n00b_dict_to_string,
        [N00B_BI_COERCIBLE]     = (n00b_vtable_entry)dict_can_coerce_to,
        [N00B_BI_COERCE]        = (n00b_vtable_entry)dict_coerce_to,
        [N00B_BI_SHALLOW_COPY]  = (n00b_vtable_entry)n00b_dict_shallow_copy,
        [N00B_BI_COPY]          = (n00b_vtable_entry)n00b_dict_copy,
        [N00B_BI_ADD]           = (n00b_vtable_entry)n00b_dict_plus,
        [N00B_BI_LEN]           = (n00b_vtable_entry)n00b_dict_len,
        [N00B_BI_INDEX_GET]     = (n00b_vtable_entry)dict_get,
        [N00B_BI_INDEX_SET]     = (n00b_vtable_entry)_n00b_dict_put,
        [N00B_BI_VIEW]          = (n00b_vtable_entry)n00b_dict_items_array,
        [N00B_BI_CONTAINER_LIT] = (n00b_vtable_entry)to_dict_lit,
        NULL,
    },
};
