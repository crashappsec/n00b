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
 *  Name:           set.h
 *  Description:    Higher level set interface based on woolhat.
 *
 *  Author:         John Viega, john@zork.org
 */

#pragma once

#include "base.h"
#include "hatrack_common.h"
#include "dict.h"
#include "woolhat.h"
#include "mmm.h"

typedef struct hatrack_set_st hatrack_set_t;

struct hatrack_set_st {
    woolhat_t           woolhat_instance;
    uint32_t            item_type;
    hatrack_hash_info_t hash_info;
    hatrack_mem_hook_t  pre_return_hook;
    hatrack_mem_hook_t  free_handler;
};

// clang-format off
HATRACK_EXTERN hatrack_set_t  *hatrack_set_new             (uint32_t);
HATRACK_EXTERN void            hatrack_set_init            (hatrack_set_t *, uint32_t);
HATRACK_EXTERN void            hatrack_set_cleanup         (hatrack_set_t *);
HATRACK_EXTERN void            hatrack_set_delete          (hatrack_set_t *);
HATRACK_EXTERN void            hatrack_set_set_hash_offset (hatrack_set_t *, int32_t);
HATRACK_EXTERN void            hatrack_set_set_cache_offset(hatrack_set_t *, int32_t);
HATRACK_EXTERN void            hatrack_set_set_custom_hash (hatrack_set_t *, hatrack_hash_func_t);
HATRACK_EXTERN void            hatrack_set_set_free_handler(hatrack_set_t *, hatrack_mem_hook_t);
HATRACK_EXTERN void            hatrack_set_set_return_hook (hatrack_set_t *, hatrack_mem_hook_t);

HATRACK_EXTERN bool            hatrack_set_contains_mmm    (hatrack_set_t *, mmm_thread_t *, void *);
HATRACK_EXTERN bool            hatrack_set_put_mmm         (hatrack_set_t *, mmm_thread_t *, void *);
HATRACK_EXTERN bool            hatrack_set_add_mmm         (hatrack_set_t *, mmm_thread_t *, void *);
HATRACK_EXTERN bool            hatrack_set_remove_mmm      (hatrack_set_t *, mmm_thread_t *, void *);
HATRACK_EXTERN void           *hatrack_set_items_mmm       (hatrack_set_t *, mmm_thread_t *, uint64_t *);
HATRACK_EXTERN void           *hatrack_set_items_sort_mmm  (hatrack_set_t *, mmm_thread_t *, uint64_t *);
HATRACK_EXTERN void           *hatrack_set_any_item_mmm    (hatrack_set_t *, mmm_thread_t *, bool *);
HATRACK_EXTERN bool            hatrack_set_is_eq_mmm       (hatrack_set_t *, mmm_thread_t *, hatrack_set_t *);
HATRACK_EXTERN bool            hatrack_set_is_superset_mmm (hatrack_set_t *, mmm_thread_t *, hatrack_set_t *, bool);
HATRACK_EXTERN bool            hatrack_set_is_subset_mmm   (hatrack_set_t *, mmm_thread_t *, hatrack_set_t *, bool);
HATRACK_EXTERN bool            hatrack_set_is_disjoint_mmm (hatrack_set_t *, mmm_thread_t *, hatrack_set_t *);
HATRACK_EXTERN void            hatrack_set_difference_mmm  (hatrack_set_t *, mmm_thread_t *, hatrack_set_t *, hatrack_set_t *);
HATRACK_EXTERN void            hatrack_set_union_mmm       (hatrack_set_t *, mmm_thread_t *, hatrack_set_t *, hatrack_set_t *);
HATRACK_EXTERN void            hatrack_set_intersection_mmm(hatrack_set_t *, mmm_thread_t *, hatrack_set_t *, hatrack_set_t *);
HATRACK_EXTERN void            hatrack_set_disjunction_mmm (hatrack_set_t *, mmm_thread_t *, hatrack_set_t *, hatrack_set_t *);
// Unreliable if there are parallel mutations.
HATRACK_EXTERN uint64_t        hatrack_set_len(hatrack_set_t *);

HATRACK_EXTERN bool            hatrack_set_contains    (hatrack_set_t *, void *);
HATRACK_EXTERN bool            hatrack_set_put         (hatrack_set_t *, void *);
HATRACK_EXTERN bool            hatrack_set_add         (hatrack_set_t *, void *);
HATRACK_EXTERN bool            hatrack_set_remove      (hatrack_set_t *, void *);
HATRACK_EXTERN void           *hatrack_set_items       (hatrack_set_t *, uint64_t *);
HATRACK_EXTERN void           *hatrack_set_items_sort  (hatrack_set_t *, uint64_t *);
HATRACK_EXTERN void           *hatrack_set_any_item    (hatrack_set_t *, bool *);
HATRACK_EXTERN bool            hatrack_set_is_eq       (hatrack_set_t *, hatrack_set_t *);
HATRACK_EXTERN bool            hatrack_set_is_superset (hatrack_set_t *, hatrack_set_t *, bool);
HATRACK_EXTERN bool            hatrack_set_is_subset   (hatrack_set_t *, hatrack_set_t *, bool);
HATRACK_EXTERN bool            hatrack_set_is_disjoint (hatrack_set_t *, hatrack_set_t *);
HATRACK_EXTERN void            hatrack_set_difference  (hatrack_set_t *, hatrack_set_t *, hatrack_set_t *);
HATRACK_EXTERN void            hatrack_set_union       (hatrack_set_t *, hatrack_set_t *, hatrack_set_t *);
HATRACK_EXTERN void            hatrack_set_intersection(hatrack_set_t *, hatrack_set_t *, hatrack_set_t *);
HATRACK_EXTERN void            hatrack_set_disjunction (hatrack_set_t *, hatrack_set_t *, hatrack_set_t *);
