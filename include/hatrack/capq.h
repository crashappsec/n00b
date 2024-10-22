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
 *  Name:           capq.c
 *  Description:    A queue whose primary dequeue operation only
 *                  dequeues if the top value is as expected.
 *
 *                  The naive pop() operation on top of cap() retries
 *                  until it succeeds, making that operation
 *                  lock-free.
 *
 *                  However, the whole purpose of this queue is to
 *                  support a wait-free help system, where threads
 *                  stick jobs into the queue, and then process items
 *                  until their item has been processed.
 *
 *                  The compare-and-pop operation makes sure that
 *                  threads can "help" the top() item, yet, if
 *                  multiple threads try to pop it, only one will
 *                  succeed. Threads in that situation do NOT retry
 *                  the cap, so as long as the enqueue and cap
 *                  operations are wait-free, we're in good shape.
 *
 *                  In this queue, the head acts much like hq, in that
 *                  it FAA's, in a ring buffer, and if it catches up
 *                  with the tail, then it resizes the queue.
 *
 *                  The tail only updates via CAS.  We use the 'epoch'
 *                  as the thing that we compare against, and the tail
 *                  epoch is bumped up by 1<<32 for each migration,
 *                  just to ensure there's never any reuse.
 *
 *  Author:         John Viega, john@zork.org
 */

#pragma once

#include "base.h"
#include "mmm.h"

typedef struct {
    void    *item;
    uint64_t state;
} capq_item_t;

typedef capq_item_t capq_top_t;

typedef _Atomic capq_item_t capq_cell_t;

typedef struct capq_store_t capq_store_t;

struct capq_store_t {
    _Atomic(capq_store_t *) next_store;
    uint64_t                size;
    _Atomic uint64_t        enqueue_index;
    _Atomic uint64_t        dequeue_index;
    capq_cell_t             cells[];
};

typedef struct {
    _Atomic(capq_store_t *) store;
    _Atomic int64_t         len;
} capq_t;

// clang-format off
HATRACK_EXTERN capq_t    *capq_new        (void);
HATRACK_EXTERN capq_t    *capq_new_size   (uint64_t);
HATRACK_EXTERN void       capq_init       (capq_t *);
HATRACK_EXTERN void       capq_init_size  (capq_t *, uint64_t);
HATRACK_EXTERN void       capq_cleanup    (capq_t *);
HATRACK_EXTERN void       capq_delete     (capq_t *);
HATRACK_EXTERN uint64_t   capq_enqueue_mmm(capq_t *, mmm_thread_t *, void *);
HATRACK_EXTERN uint64_t   capq_enqueue    (capq_t *, void *);
HATRACK_EXTERN capq_top_t capq_top_mmm    (capq_t *, mmm_thread_t *, bool *);
HATRACK_EXTERN capq_top_t capq_top        (capq_t *, bool *);
HATRACK_EXTERN bool       capq_cap_mmm    (capq_t *, mmm_thread_t *, uint64_t);
HATRACK_EXTERN bool       capq_cap        (capq_t *, uint64_t);
HATRACK_EXTERN void      *capq_dequeue_mmm(capq_t *, mmm_thread_t *, bool *);
HATRACK_EXTERN void      *capq_dequeue    (capq_t *, bool *);
HATRACK_EXTERN uint64_t   capq_len_mmm    (capq_t *, mmm_thread_t *);
HATRACK_EXTERN uint64_t   capq_len        (capq_t *);
