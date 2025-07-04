/*
 * Copyright © 2022 John Viega
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
 *  Name:           dict.h
 *  Description:    Higher level dictionary interface based on crown.
 *
 *  Author:         John Viega, john@zork.org
 */

#pragma once

#include "base.h"
#include "hatrack_common.h"
#include "crown.h"

enum {
    HATRACK_DICT_KEY_TYPE_INT,
    HATRACK_DICT_KEY_TYPE_REAL,
    HATRACK_DICT_KEY_TYPE_CSTR,
    HATRACK_DICT_KEY_TYPE_PTR,
    HATRACK_DICT_KEY_TYPE_OBJ_INT,
    HATRACK_DICT_KEY_TYPE_OBJ_REAL,
    HATRACK_DICT_KEY_TYPE_OBJ_CSTR,
    HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    HATRACK_DICT_KEY_TYPE_OBJ_CUSTOM
};

enum {
    HATRACK_DICT_NO_CACHE = 0xffffffff
};

typedef struct {
    int32_t hash_offset;
    int32_t cache_offset;
} hatrack_offset_info_t;

typedef struct {
    void *key;
    void *value;
} hatrack_dict_item_t;

typedef struct hatrack_dict_t hatrack_dict_t;

typedef void *hatrack_dict_key_t;
typedef void *hatrack_dict_value_t;

typedef union {
    hatrack_offset_info_t offsets;
    hatrack_hash_func_t   custom_hash;
} hatrack_hash_info_t;

struct hatrack_dict_t {
    crown_t             crown_instance;
    uint32_t            key_type;
    bool                slow_views;
    bool                sorted_views;
    hatrack_hash_info_t hash_info;
    hatrack_mem_hook_t  free_handler;
    hatrack_mem_hook_t  key_return_hook;
    hatrack_mem_hook_t  val_return_hook;
#ifdef HATRACK_PER_INSTANCE_AUX
    void *bucket_aux;
#endif
};

// clang-format off
#ifdef HATRACK_PER_INSTANCE_AUX
HATRACK_EXTERN hatrack_dict_t *hatrack_dict_new    (uint32_t, void *);
HATRACK_EXTERN void            hatrack_dict_init   (hatrack_dict_t *, uint32_t, void *);
#else
HATRACK_EXTERN hatrack_dict_t *hatrack_dict_new    (uint32_t);
HATRACK_EXTERN void            hatrack_dict_init   (hatrack_dict_t *, uint32_t);

#endif

HATRACK_EXTERN void            hatrack_dict_cleanup(hatrack_dict_t *);
HATRACK_EXTERN void            hatrack_dict_delete (hatrack_dict_t *);

HATRACK_EXTERN void hatrack_dict_set_hash_offset     (hatrack_dict_t *, int32_t);
HATRACK_EXTERN void hatrack_dict_set_cache_offset    (hatrack_dict_t *, int32_t);
HATRACK_EXTERN void hatrack_dict_set_custom_hash     (hatrack_dict_t *, hatrack_hash_func_t);
HATRACK_EXTERN void hatrack_dict_set_free_handler    (hatrack_dict_t *, hatrack_mem_hook_t);
HATRACK_EXTERN void hatrack_dict_set_key_return_hook (hatrack_dict_t *, hatrack_mem_hook_t);
HATRACK_EXTERN void hatrack_dict_set_val_return_hook (hatrack_dict_t *, hatrack_mem_hook_t);
HATRACK_EXTERN void hatrack_dict_set_consistent_views(hatrack_dict_t *, bool);
HATRACK_EXTERN void hatrack_dict_set_sorted_views    (hatrack_dict_t *, bool);
HATRACK_EXTERN bool hatrack_dict_get_consistent_views(hatrack_dict_t *);
HATRACK_EXTERN bool hatrack_dict_get_sorted_views    (hatrack_dict_t *);

HATRACK_EXTERN void *hatrack_dict_get_mmm    (hatrack_dict_t *, mmm_thread_t *thread, void *, bool *);
HATRACK_EXTERN void  hatrack_dict_put_mmm    (hatrack_dict_t *, mmm_thread_t *thread, void *, void *);
HATRACK_EXTERN bool  hatrack_dict_replace_mmm(hatrack_dict_t *, mmm_thread_t *thread, void *, void *);
HATRACK_EXTERN bool  hatrack_dict_cas_mmm(hatrack_dict_t *, mmm_thread_t *thread, void *, void *, void *, bool);
HATRACK_EXTERN bool  hatrack_dict_add_mmm    (hatrack_dict_t *, mmm_thread_t *thread, void *, void *);
HATRACK_EXTERN bool  hatrack_dict_remove_mmm (hatrack_dict_t *, mmm_thread_t *thread, void *);

HATRACK_EXTERN hatrack_dict_key_t   *hatrack_dict_keys_mmm         (hatrack_dict_t *, mmm_thread_t *, uint64_t *);
HATRACK_EXTERN hatrack_dict_value_t *hatrack_dict_values_mmm       (hatrack_dict_t *, mmm_thread_t *, uint64_t *);
HATRACK_EXTERN hatrack_dict_item_t  *hatrack_dict_items_mmm        (hatrack_dict_t *, mmm_thread_t *, uint64_t *);
HATRACK_EXTERN hatrack_dict_key_t   *hatrack_dict_keys_sort_mmm    (hatrack_dict_t *, mmm_thread_t *, uint64_t *);
HATRACK_EXTERN hatrack_dict_value_t *hatrack_dict_values_sort_mmm  (hatrack_dict_t *, mmm_thread_t *, uint64_t *);
HATRACK_EXTERN hatrack_dict_item_t  *hatrack_dict_items_sort_mmm   (hatrack_dict_t *, mmm_thread_t *, uint64_t *);
HATRACK_EXTERN hatrack_dict_key_t   *hatrack_dict_keys_nosort_mmm  (hatrack_dict_t *, mmm_thread_t *, uint64_t *);
HATRACK_EXTERN hatrack_dict_value_t *hatrack_dict_values_nosort_mmm(hatrack_dict_t *, mmm_thread_t *, uint64_t *);
HATRACK_EXTERN hatrack_dict_item_t  *hatrack_dict_items_nosort_mmm (hatrack_dict_t *, mmm_thread_t *, uint64_t *);

HATRACK_EXTERN void *hatrack_dict_get    (hatrack_dict_t *, void *, bool *);
HATRACK_EXTERN void  hatrack_dict_put    (hatrack_dict_t *, void *, void *);
HATRACK_EXTERN bool  hatrack_dict_replace(hatrack_dict_t *, void *, void *);
HATRACK_EXTERN bool  hatrack_dict_cas    (hatrack_dict_t *, void *, void *, void *, bool);
HATRACK_EXTERN bool  hatrack_dict_add    (hatrack_dict_t *, void *, void *);
HATRACK_EXTERN bool  hatrack_dict_remove (hatrack_dict_t *, void *);

HATRACK_EXTERN hatrack_dict_key_t   *hatrack_dict_keys         (hatrack_dict_t *, uint64_t *);
HATRACK_EXTERN hatrack_dict_value_t *hatrack_dict_values       (hatrack_dict_t *, uint64_t *);
HATRACK_EXTERN hatrack_dict_item_t  *hatrack_dict_items        (hatrack_dict_t *, uint64_t *);
HATRACK_EXTERN hatrack_dict_key_t   *hatrack_dict_keys_sort    (hatrack_dict_t *, uint64_t *);
HATRACK_EXTERN hatrack_dict_value_t *hatrack_dict_values_sort  (hatrack_dict_t *, uint64_t *);
HATRACK_EXTERN hatrack_dict_item_t  *hatrack_dict_items_sort   (hatrack_dict_t *, uint64_t *);
HATRACK_EXTERN hatrack_dict_key_t   *hatrack_dict_keys_nosort  (hatrack_dict_t *, uint64_t *);
HATRACK_EXTERN hatrack_dict_value_t *hatrack_dict_values_nosort(hatrack_dict_t *, uint64_t *);
HATRACK_EXTERN hatrack_dict_item_t  *hatrack_dict_items_nosort (hatrack_dict_t *, uint64_t *);

static inline uint64_t
hatrack_dict_len(hatrack_dict_t *dict)
{
    return crown_len(&dict->crown_instance);
}

#ifdef HATRACK_PER_INSTANCE_AUX
static inline void
hatrack_dict_set_aux(hatrack_dict_t *dict, void *aux)
{
    dict->bucket_aux = aux;
}
#endif
