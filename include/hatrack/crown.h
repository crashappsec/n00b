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
 *  Name:           crown.h
 *  Description:    Crown Really Overcomplicates Witchhat Now
 *
 *                  Crown is a slight modification of witchhat-- it
 *                  changes the probing strategy for buckets.
 *
 *                  Refer to crown.c for implementation notes.
 *
 *  Author: John Viega, john@zork.org
 */

#pragma once

#include "base.h"
#include "hatrack_common.h"
#include "mmm.h"

typedef struct {
    void    *item;
    uint64_t info;
} crown_record_t;

typedef struct {
    _Atomic hatrack_hash_t hv;
    _Atomic crown_record_t record;

#ifdef HATRACK_32_BIT_HOP_TABLE
    _Atomic uint32_t neighbor_map;
#else
    _Atomic uint64_t neighbor_map;
#endif
} crown_bucket_t;

typedef struct crown_store_st crown_store_t;

struct crown_store_st {
    _Atomic(crown_store_t *) store_next;
    uint64_t                 last_slot;
    uint64_t                 threshold;
    _Atomic uint64_t         used_count;
    _Atomic bool             claimed;
    crown_bucket_t           buckets[];
};

typedef struct {
    _Atomic(crown_store_t *) store_current;
    _Atomic uint64_t         item_count;
    _Atomic uint64_t         help_needed;
    uint64_t                 next_epoch;
#ifdef HATRACK_PER_INSTANCE_AUX
    void *aux_info_for_store;
#endif
} crown_t;

// clang-format off
#ifdef HATRACK_PER_INSTANCE_AUX
HATRACK_EXTERN crown_t        *crown_new          (void *);
HATRACK_EXTERN crown_t        *crown_new_size     (char, void *);
HATRACK_EXTERN void            crown_init         (crown_t *, void *);
HATRACK_EXTERN void            crown_init_size    (crown_t *, char, void *);
#else
HATRACK_EXTERN crown_t        *crown_new          (void);
HATRACK_EXTERN crown_t        *crown_new_size     (char);
HATRACK_EXTERN void            crown_init         (crown_t *);
HATRACK_EXTERN void            crown_init_size    (crown_t *, char);
#endif
HATRACK_EXTERN void            crown_cleanup      (crown_t *);
HATRACK_EXTERN void            crown_delete       (crown_t *);
HATRACK_EXTERN void           *crown_get_mmm      (crown_t *, mmm_thread_t *, hatrack_hash_t, bool *);
HATRACK_EXTERN void           *crown_get          (crown_t *, hatrack_hash_t, bool *);
HATRACK_EXTERN void           *crown_put_mmm      (crown_t *, mmm_thread_t *, hatrack_hash_t, void *, bool *);
HATRACK_EXTERN void           *crown_put          (crown_t *, hatrack_hash_t, void *, bool *);
HATRACK_EXTERN void           *crown_replace_mmm  (crown_t *, mmm_thread_t *, hatrack_hash_t, void *, bool *);
HATRACK_EXTERN void           *crown_replace      (crown_t *, hatrack_hash_t, void *, bool *);
HATRACK_EXTERN bool            crown_add_mmm      (crown_t *, mmm_thread_t *, hatrack_hash_t, void *);
HATRACK_EXTERN bool            crown_add          (crown_t *, hatrack_hash_t, void *);
HATRACK_EXTERN void           *crown_remove_mmm   (crown_t *, mmm_thread_t *, hatrack_hash_t, bool *);
HATRACK_EXTERN void           *crown_remove       (crown_t *, hatrack_hash_t, bool *);
HATRACK_EXTERN uint64_t        crown_len_mmm      (crown_t *, mmm_thread_t *);
HATRACK_EXTERN uint64_t        crown_len          (crown_t *);
HATRACK_EXTERN hatrack_view_t *crown_view_mmm     (crown_t *, mmm_thread_t *, uint64_t *, bool);
HATRACK_EXTERN hatrack_view_t *crown_view         (crown_t *, uint64_t *, bool);
HATRACK_EXTERN hatrack_view_t *crown_view_fast_mmm(crown_t *, mmm_thread_t *, uint64_t *, bool);
HATRACK_EXTERN hatrack_view_t *crown_view_fast    (crown_t *, uint64_t *, bool);
HATRACK_EXTERN hatrack_view_t *crown_view_slow_mmm(crown_t *, mmm_thread_t *, uint64_t *, bool);
HATRACK_EXTERN hatrack_view_t *crown_view_slow    (crown_t *, uint64_t *, bool);
