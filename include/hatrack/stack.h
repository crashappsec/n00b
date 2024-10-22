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
 *  Name:           stack.h
 *  Description:    A faster stack implementation that avoids
 *                  using a linked list node for each item.
 *
 *                  We could devise something that is never going to
 *                  copy state when it needs to expand the underlying
 *                  store, breaking the stack up into linked
 *                  segments. For now, I'm not doing that, just to
 *                  keep things as simple as possible.
 *
 *
 *  Author:         John Viega, john@zork.org
 */

#pragma once

#include "base.h"
#include "mmm.h"

/* "Valid after" means that, in any epoch after the epoch stored in
 * this field, pushers that are assigned that slot are free to try
 * to write there.
 *
 * Slow pushers assigned this slot in or before the listed epoch
 * are not allowed to write here.
 *
 * Similarly, pushes add valid_after to tell (very) poppers whether
 * they're allowed to pop the item.  As a pusher, if the operation
 * happens in epoch n, we'll actually write epoch-1 into the field, so
 * that the name "valid after" holds true.
 */
typedef struct {
    void    *item;
    uint32_t state;
    uint32_t valid_after;
} stack_item_t;

typedef _Atomic stack_item_t stack_cell_t;
typedef struct stack_store_t stack_store_t;

typedef struct {
    uint64_t       next_ix;
    stack_store_t *store;
} stack_view_t;

struct stack_store_t {
    uint64_t                 num_cells;
    _Atomic uint64_t         head_state;
    _Atomic(stack_store_t *) next_store;
    _Atomic bool             claimed;
    stack_cell_t             cells[];
};

typedef struct {
    _Atomic(stack_store_t *) store;
    uint64_t                 compress_threshold;
#ifdef HATSTACK_WAIT_FREE
    _Atomic int64_t push_help_shift;
#endif
} hatstack_t;

// clang-format off
HATRACK_EXTERN hatstack_t   *hatstack_new            (uint64_t);
HATRACK_EXTERN void          hatstack_init           (hatstack_t *, uint64_t);
HATRACK_EXTERN void          hatstack_cleanup        (hatstack_t *);
HATRACK_EXTERN void          hatstack_delete         (hatstack_t *);
HATRACK_EXTERN void          hatstack_push_mmm       (hatstack_t *, mmm_thread_t *, void *);
HATRACK_EXTERN void          hatstack_push           (hatstack_t *, void *);
HATRACK_EXTERN void         *hatstack_pop_mmm        (hatstack_t *, mmm_thread_t *, bool *);
HATRACK_EXTERN void         *hatstack_pop            (hatstack_t *, bool *);
HATRACK_EXTERN void         *hatstack_peek_mmm       (hatstack_t *, mmm_thread_t *, bool *);
HATRACK_EXTERN void         *hatstack_peek           (hatstack_t *, bool *);
HATRACK_EXTERN stack_view_t *hatstack_view_mmm       (hatstack_t *, mmm_thread_t *);
HATRACK_EXTERN stack_view_t *hatstack_view           (hatstack_t *);
HATRACK_EXTERN void         *hatstack_view_next      (stack_view_t *, bool *);
HATRACK_EXTERN void          hatstack_view_delete_mmm(stack_view_t *, mmm_thread_t *);
HATRACK_EXTERN void          hatstack_view_delete    (stack_view_t *);
