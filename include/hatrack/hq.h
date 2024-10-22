/*
 * Copyright © 2022 John Viega
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License atn
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *  Name:           hq.h
 *  Description:    A fast, wait-free queue implementation.
 *
 *  Author:         John Viega, john@zork.org
 *
 * I've decided to build a queue that doesn't use segments; instead,
 * it uses a single buffer in a ring, and resizes up if the head
 * pointer catches up to the tail pointer.
 *
 * Once I realized it was possible, it seemed more likely to work well
 * than continually allocing and freeing segments. And that does
 * indeed seem to be the case. The difference is particularly stark
 * when there are few enqueuers, but lots of dequeuers; the "skipping"
 * of the wait mechanism slows down the segment queue, because it will
 * end up doing a lot more mallocing than this one, which only ever
 * doubles the entire queue in size when it needs to grow.
 */

#pragma once

#include "base.h"
#include "mmm.h"

typedef struct {
    void    *item;
    uint64_t state;
} hq_item_t;

typedef _Atomic hq_item_t hq_cell_t;

typedef struct hq_store_t hq_store_t;

typedef struct {
    uint64_t    start_epoch;
    uint64_t    last_epoch;
    uint64_t    next_ix;
    hq_store_t *store;
} hq_view_t;

struct hq_store_t {
    _Atomic(hq_store_t *) next_store;
    uint64_t              size;
    _Atomic uint64_t      enqueue_index;
    _Atomic uint64_t      dequeue_index;
    _Atomic bool          claimed;
    hq_cell_t             cells[];
};

typedef struct {
    _Atomic(hq_store_t *) store;
    _Atomic int64_t       len;
} hq_t;

// clang-format off
HATRACK_EXTERN hq_t      *hq_new            (void);
HATRACK_EXTERN hq_t      *hq_new_size       (uint64_t);
HATRACK_EXTERN void       hq_init           (hq_t *);
HATRACK_EXTERN void       hq_init_size      (hq_t *, uint64_t);
HATRACK_EXTERN void       hq_cleanup        (hq_t *);
HATRACK_EXTERN void       hq_delete         (hq_t *);
HATRACK_EXTERN void       hq_enqueue_mmm    (hq_t *, mmm_thread_t *, void *);
HATRACK_EXTERN void       hq_enqueue        (hq_t *, void *);
HATRACK_EXTERN void      *hq_dequeue_mmm    (hq_t *, mmm_thread_t *, bool *);
HATRACK_EXTERN void      *hq_dequeue        (hq_t *, bool *);
HATRACK_EXTERN int64_t    hq_len_mmm        (hq_t *, mmm_thread_t *);
HATRACK_EXTERN int64_t    hq_len            (hq_t *);
HATRACK_EXTERN hq_view_t *hq_view_mmm       (hq_t *, mmm_thread_t *);
HATRACK_EXTERN hq_view_t *hq_view           (hq_t *);
HATRACK_EXTERN void      *hq_view_next      (hq_view_t *, bool *);
HATRACK_EXTERN void       hq_view_delete_mmm(hq_view_t *, mmm_thread_t *);
HATRACK_EXTERN void       hq_view_delete    (hq_view_t *);
