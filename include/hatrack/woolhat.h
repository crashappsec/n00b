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
 *  Name:           woolhat.h
 *  Description:    Wait-free Operations, Orderable, Linearizable HAsh Table
 *                  This version keeps unordered buckets, and sorts
 *                  by epoch when needed. Views are fully consistent.
 *
 *  Author:         John Viega, john@zork.org
 */

#pragma once

#include "base.h"
#include "hatrack_common.h"
#include "mmm.h"

typedef struct woolhat_record_st woolhat_record_t;

struct woolhat_record_st {
    woolhat_record_t *next;
    void             *item;
    bool              deleted;
};

typedef struct {
    woolhat_record_t *head;
    uint64_t          flags;
} woolhat_state_t;

typedef struct {
    _Atomic hatrack_hash_t  hv;
    _Atomic woolhat_state_t state;
} woolhat_history_t;

typedef struct woolhat_store_st woolhat_store_t;

struct woolhat_store_st {
    uint64_t                   last_slot;
    uint64_t                   threshold;
    _Atomic uint64_t           used_count;
    _Atomic(woolhat_store_t *) store_next;
    woolhat_history_t          hist_buckets[];
};

typedef struct woolhat_st {
    _Atomic(woolhat_store_t *) store_current;
    _Atomic uint64_t           item_count;
    _Atomic uint64_t           help_needed;
    mmm_cleanup_func           cleanup_func;
    void                      *cleanup_aux;
} woolhat_t;

/* This is a special type of view result that includes the hash
 * value, intended for set operations. Currently, it is only in use
 * by woolhat (and by hatrack_set, which is built on woolhat).
 */

typedef struct {
    hatrack_hash_t hv;
    void          *item;
    int64_t        sort_epoch;
} hatrack_set_view_t;

// clang-format off
HATRACK_EXTERN woolhat_t      *woolhat_new             (void);
HATRACK_EXTERN woolhat_t      *woolhat_new_size        (char);
HATRACK_EXTERN void            woolhat_init            (woolhat_t *);
HATRACK_EXTERN void            woolhat_init_size       (woolhat_t *, char);
HATRACK_EXTERN void            woolhat_cleanup         (woolhat_t *);
HATRACK_EXTERN void            woolhat_delete          (woolhat_t *);
HATRACK_EXTERN void            woolhat_set_cleanup_func(woolhat_t *, mmm_cleanup_func, void *);
HATRACK_EXTERN void           *woolhat_get_mmm         (woolhat_t *, mmm_thread_t *, hatrack_hash_t, bool *);
HATRACK_EXTERN void           *woolhat_get             (woolhat_t *, hatrack_hash_t, bool *);
HATRACK_EXTERN void           *woolhat_put_mmm         (woolhat_t *, mmm_thread_t *, hatrack_hash_t, void *, bool *);
HATRACK_EXTERN void           *woolhat_put             (woolhat_t *, hatrack_hash_t, void *, bool *);
HATRACK_EXTERN void           *woolhat_replace_mmm     (woolhat_t *, mmm_thread_t *, hatrack_hash_t, void *, bool *);
HATRACK_EXTERN void           *woolhat_replace         (woolhat_t *, hatrack_hash_t, void *, bool *);
HATRACK_EXTERN bool            woolhat_add_mmm         (woolhat_t *, mmm_thread_t *, hatrack_hash_t, void *);
HATRACK_EXTERN bool            woolhat_add             (woolhat_t *, hatrack_hash_t, void *);
HATRACK_EXTERN void           *woolhat_remove_mmm      (woolhat_t *, mmm_thread_t *, hatrack_hash_t, bool *);
HATRACK_EXTERN void           *woolhat_remove          (woolhat_t *, hatrack_hash_t, bool *);
HATRACK_EXTERN uint64_t        woolhat_len_mmm         (woolhat_t *, mmm_thread_t *);
HATRACK_EXTERN uint64_t        woolhat_len             (woolhat_t *);

HATRACK_EXTERN hatrack_view_t     *woolhat_view_mmm    (woolhat_t *, mmm_thread_t *, uint64_t *, bool);
HATRACK_EXTERN hatrack_view_t     *woolhat_view        (woolhat_t *, uint64_t *, bool);
HATRACK_EXTERN hatrack_set_view_t *woolhat_view_epoch  (woolhat_t *, uint64_t *, uint64_t);

HATRACK_EXTERN void hatrack_set_view_delete(hatrack_set_view_t *view, uint64_t num);
