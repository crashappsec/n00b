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
 *  Name:           q64.h
 *  Description:    A variant of our wait-free queue for x86 systems
 *                  lacking a 128-bit compare and swap.
 *
 *  Author:         John Viega, john@zork.org
 *
 * In the 128-bit version of the algorithm, fields get a full 64 bits
 * for data, and then a state field where most of the state is unused.
 *
 * In this version, we steal the two low bits for the state we
 * need. That means contents must either be pointers, or must fit in
 * 62 bits.
 *
 * In the case of pointers, they will be memory aligned, so stealing
 * the two bits is not going to impact anything. For non-pointer
 * values though, they should be shifted left a minimum of two bits.
 */

#pragma once

#include "base.h"
#include "mmm.h"

typedef uint64_t           q64_item_t;
typedef _Atomic q64_item_t q64_cell_t;

typedef struct q64_segment_st q64_segment_t;

/* If help_needed is non-zero, new segments get the default segement
 * size. Otherwise, we double the size of the queue, whatever it is.
 *
 * Combine this with writers (eventually) exponentially increasing the
 * number of cells they jump the counter when their enqueue attempts
 * fail, and this is guaranteed to be wait-free.
 */

struct q64_segment_st {
    _Atomic(q64_segment_t *) next;
    uint64_t                 size;
    _Atomic uint64_t         enqueue_index;
    _Atomic uint64_t         dequeue_index;
    q64_cell_t               cells[];
};

typedef struct {
    alignas(16) q64_segment_t *enqueue_segment;
    q64_segment_t *dequeue_segment;
} q64_seg_ptrs_t;

typedef struct {
    _Atomic q64_seg_ptrs_t segments;
    uint64_t               default_segment_size;
    _Atomic uint64_t       help_needed;
    _Atomic uint64_t       len;
} q64_t;

// clang-format off
HATRACK_EXTERN q64_t   *q64_new        (void);
HATRACK_EXTERN q64_t   *q64_new_size   (char);
HATRACK_EXTERN void     q64_init       (q64_t *);
HATRACK_EXTERN void     q64_init_size  (q64_t *, char);
HATRACK_EXTERN void     q64_cleanup    (q64_t *);
HATRACK_EXTERN void     q64_delete     (q64_t *);
HATRACK_EXTERN uint64_t q64_len_mmm    (q64_t *, mmm_thread_t *);
HATRACK_EXTERN uint64_t q64_len        (q64_t *);
HATRACK_EXTERN void     q64_enqueue_mmm(q64_t *, mmm_thread_t *, void *);
HATRACK_EXTERN void     q64_enqueue    (q64_t *, void *);
HATRACK_EXTERN void    *q64_dequeue_mmm(q64_t *, mmm_thread_t *, bool *);
HATRACK_EXTERN void    *q64_dequeue    (q64_t *, bool *);
