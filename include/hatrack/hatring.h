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
 *  Name:           hatring.h
 *  Description:    A wait-free ring buffer.
 *
 *  Note that, after finishing this, I just searched for other
 *  multi-consumer, multi-producer implementations, and found
 *  something I had not previously seen (I had searched, but nothing
 *  reasonable came up on the top page I guess).  There's a 2015 paper
 *  by Steven Feldman et al. that's worth mentioning.
 *
 *  I might implement their algorithm at some point to compare, and it
 *  takes a similar approach to wait freedom (exponential backoff,
 *  which is a pretty obvious and easy approach, especially when you
 *  can't exponentially grow the storage as with other algorithms).
 *
 *  However, from their paper, the thing that's most surprising to me
 *  is that, for what is allegedly a "ring buffer", enqueues can fail
 *  if a buffer is full.  While I could definitely speed my ring up by
 *  allowing that, it seems like the antithesis of what ring buffers
 *  are for... the newest data should be guaranteed an enqueue slot,
 *  at the expense of dropping older, unread data.
 *
 *  Without that, they haven't produced what I consider a true ring
 *  buffer-- it's just a fixed-sized FIFO.
 *
 *  Author:         John Viega, john@zork.org
 */

#pragma once

#include "base.h"

typedef struct {
    void    *item;
    uint64_t state;
} hatring_item_t;

typedef _Atomic hatring_item_t hatring_cell_t;

typedef void (*hatring_drop_handler)(void *);

typedef struct {
    uint64_t next_ix;
    uint64_t num_items;
    void    *cells[];
} hatring_view_t;

typedef struct {
    _Atomic uint64_t     epochs;
    hatring_drop_handler drop_handler;
    uint64_t             last_slot;
    uint64_t             size;
    hatring_cell_t       cells[];
} hatring_t;

// clang-format off
HATRACK_EXTERN hatring_t      *hatring_new             (uint64_t);
HATRACK_EXTERN void            hatring_init            (hatring_t *, uint64_t);
HATRACK_EXTERN void            hatring_cleanup         (hatring_t *);
HATRACK_EXTERN void            hatring_delete          (hatring_t *);
HATRACK_EXTERN uint32_t        hatring_enqueue         (hatring_t *, void *);
HATRACK_EXTERN void           *hatring_dequeue         (hatring_t *, bool *);
HATRACK_EXTERN void           *hatring_dequeue_w_epoch (hatring_t *, bool *, uint32_t *);
HATRACK_EXTERN hatring_view_t *hatring_view            (hatring_t *);
HATRACK_EXTERN void           *hatring_view_next       (hatring_view_t *, bool *);
HATRACK_EXTERN void            hatring_view_delete     (hatring_view_t *);
HATRACK_EXTERN void            hatring_set_drop_handler(hatring_t *, hatring_drop_handler);
