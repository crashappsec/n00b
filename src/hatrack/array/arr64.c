/*
 * Copyright © 2024 John Viega
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
 *  Name:           arr64.c
 *  Description:    A flex array using only 64-bit CAS. Unlike the
 *                  128-bit version, this does not support
 *                  shrinking. There are two interfaces, one that
 *                  assumes we're setting pointers (or that the API
 *                  can otherwise steal the bottom three bits), and
 *                  one that always allocates a data cell for each
 *                  (64-bit) object. It sucks using dynamic memory
 *                  for that, but if you're not willing to emulate
 *                  the 128-bit CAS operation, it's doable.
 *
 *                  This ONLY allows indexing and resizing the array.
 *
 *  Author:         John Viega, john@zork.org
 */

#include "hatrack/arr64.h"
#include "hatrack/malloc.h"
#include "hatrack/mmm.h"
#include "hatrack/hatomic.h"
#include "hatrack/hatrack_common.h"

#define ARR64_MIN_STORE_SZ_LOG 4

enum64(arr64_enum_t,
       ARR64_USED   = 0x00000000000000001,
       ARR64_MOVED  = 0x00000000000000002,
       ARR64_MOVING = 0x00000000000000004);

enum {
    ARR64_OK,
    ARR64_OOB,
    ARR64_UNINITIALIZED
};

static arr64_store_t *arr64_new_store(uint64_t, uint64_t);
static void           arr64_migrate(arr64_store_t *, arr64_t *);

/* The size parameter is the one larger than the largest allowable index.
 * The underlying store may be bigger-- it will be sized up to the next
 * power of two.
 */
arr64_t *
arr64_new(uint64_t initial_size)
{
    arr64_t *arr;

    arr = (arr64_t *)hatrack_malloc(sizeof(arr64_t));

    arr64_init(arr, initial_size);

    return arr;
}

void
arr64_init(arr64_t *arr, uint64_t initial_size)
{
    uint64_t store_size;

    arr->ret_callback   = NULL;
    arr->eject_callback = NULL;
    store_size          = hatrack_round_up_to_power_of_2(initial_size);

    if (store_size < (1 << ARR64_MIN_STORE_SZ_LOG)) {
        store_size = 1 << ARR64_MIN_STORE_SZ_LOG;
    }

    atomic_store(&arr->store, arr64_new_store(initial_size, store_size));

    return;
}

void
arr64_set_ret_callback(arr64_t *self, arr64_callback_t callback)
{
    self->ret_callback = callback;

    return;
}

void
arr64_set_eject_callback(arr64_t *self, arr64_callback_t callback)
{
    self->eject_callback = callback;
}

void
arr64_cleanup(arr64_t *self)
{
    arr64_store_t *store;
    uint64_t       i;
    arr64_item_t   item;

    store = atomic_load(&self->store);

    if (self->eject_callback) {
        for (i = 0; i < store->array_size; i++) {
            item = atomic_read(&store->cells[i]);
            if (((uint64_t)item) &) {
                (*self->eject_callback)(item.item);
            }
        }
    }

    mmm_retire_unused(self->store);

    return;
}

void
arr64_delete(arr64_t *self)
{
    arr64_cleanup(self);
    hatrack_free(self, sizeof(arr64_t));

    return;
}

const uint64_t state_mask = ARR64_USED | ARR64_MOVED | ARR64_MOVING;
const uint64_t val_mask   = ~state_mask;

void *
arr64_get_ptr_mmm(arr64_t *self, mmm_thread_t *thread, uint64_t index, int *status)
{
    arr64_item_t   current;
    arr64_store_t *store;
    uint64_t       state;

    mmm_start_basic_op(thread);

    store = atomic_read(&self->store);

    if (index >= atomic_read(&store->array_size)) {
        if (status) {
            *status = ARR64_OOB;
        }
        mmm_end_op(thread);
        return NULL;
    }

    // A resize is in progress, item is not there yet.
    if (index >= store->store_size) {
        if (status) {
            *status = ARR64_UNINITIALIZED;
        }
        mmm_end_op(thread);
        return NULL;
    }

    current = atomic_read(&store->cells[index]);
    state   = ((uint64_t)current) & state_mask;

    if (!(state & ARR64_USED)) {
        if (status) {
            *status = ARR64_UNINITIALIZED;
        }
        mmm_end_op(thread);
        return NULL;
    }

    if (self->ret_callback && current.item) {
        (*self->ret_callback)(current.item);
    }

    mmm_end_op(thread);

    if (status) {
        *status = ARR64_OK;
    }

    return current.item;
}

void *
arr64_get_ptr(arr64_t *self, uint64_t index, int *status)
{
    return arr64_get_ptr_mmm(self, mmm_thread_acquire(), index, status);
}

// Returns true if successful, false if write would be out-of-bounds.
bool
arr64_set_ptr_mmm(arr64_t *self, mmm_thread_t *thread, uint64_t index, void *item)
{
    arr64_store_t *store;
    arr64_item_t   current;
    arr64_item_t   candidate;
    arr64_cell_t  *cellptr;
    uint64_t       read_index;
    uint64_t       state;
    void          *value;

    mmm_start_basic_op(thread);

    store      = atomic_read(&self->store);
    read_index = atomic_read(&store->array_size);

    if (index >= read_index) {
        mmm_end_op(thread);
        return false;
    }

    if (index >= store->store_size) {
        arr64_migrate(store, self);
        mmm_end_op(thread);
        return arr64_set(self, index, item);
    }

    cellptr = &store->cells[index];
    current = atomic_read(cellptr);
    state   = ((uint64_t)current) & state_mask;

    if (state & ARR64_ARRAY_MOVING) {
        arr64_migrate(store, self);
        mmm_end_op(thread);
        return arr64_set(self, index, item);
    }

    candidate = (((arr64_item_t)item) & val_mask) | ARR64_ARRAY_USED;

    if (CAS(cellptr, &current, candidate)) {
        state = ((uint64_t)current) & state_mask;
        value = (void *)(((uint64_t)current) & value_mask);

        if (self->eject_callback && (state == ARR64_ARRAY_USED)) {
            (*self->eject_callback)(value);
        }
        mmm_end_op(thread);
        return true;
    }
    else {
        state = ((uint64_t)current) & state_mask;
    }

    if (state & ARR64_ARRAY_MOVING) {
        arr64_migrate(store, self);
        mmm_end_op(thread);
        return arr64_set(self, index, item);
    }

    /* Otherwise, someone beat us to the CAS, but we sequence ourselves
     * BEFORE the CAS operation (i.e., we got overwritten).
     */

    if (self->eject_callback) {
        (*self->eject_callback)(item);
    }

    mmm_end_op(thread);
    return true;
}

bool
arr64_set_ptr(arr64_t *self, uint64_t index, void *item)
{
    return arr64_set_ptr_mmm(self, mmm_thread_acquire(), index, item);
}

void
arr64_grow_mmm(arr64_t *self, mmm_thread_t *thread, uint64_t index)
{
    arr64_store_t *store;
    uint64_t       array_size;

    mmm_start_basic_op(thread);

    /* Just change store->array_size, kick off a migration if
     * necessary, and be done.
     */
    do {
        store      = atomic_read(&self->store);
        array_size = atomic_read(&store->array_size);

        if (index < array_size) {
            mmm_end_op(thread);
            return;
        }
    } while (!CAS(&store->array_size, &array_size, index));

    if (index > store->store_size) {
        arr64_migrate(store, self);
    }

    mmm_end_op(thread);
    return;
}

void
arr64_grow(arr64_t *self, uint64_t index)
{
    arr64_grow_mmm(self, mmm_thread_acquire(), index);
}

arr64_view_t *
arr64_view_mmm(arr64_t *self, mmm_thread_t *thread)
{
    arr64_view_t  *ret;
    arr64_store_t *store;
    bool           expected;
    uint64_t       i;
    arr64_item_t   item;

    mmm_start_basic_op(thread);

    while (true) {
        store    = atomic_read(&self->store);
        expected = false;

        if (CAS(&store->claimed, &expected, true)) {
            break;
        }
        arr64_migrate(store, self);
    }

    arr64_migrate(store, self);

    if (self->ret_callback) {
        for (i = 0; i < store->array_size; i++) {
            item = atomic_read(&store->cells[i]);
            if (item.state & ARR64_ARRAY_USED) {
                (*self->ret_callback)(item.item);
            }
        }
    }

    mmm_end_op(thread);

    ret           = (arr64_view_t *)hatrack_malloc(sizeof(arr64_view_t));
    ret->contents = store;
    ret->next_ix  = 0;

    return ret;
}

arr64_view_t *
arr64_view(arr64_t *self)
{
    return arr64_view_mmm(self, mmm_thread_acquire());
}

void *
arr64_view_next(arr64_view_t *view, bool *found)
{
    arr64_item_t item;

    while (true) {
        if (view->next_ix >= view->contents->array_size) {
            if (found) {
                *found = false;
            }
            return NULL;
        }

        item = atomic_read(&view->contents->cells[view->next_ix++]);

        if (item.state & ARR64_ARRAY_USED) {
            if (found) {
                *found = true;
            }
            return item.item;
        }
    }
}

void
arr64_view_delete_mmm(arr64_view_t *view, mmm_thread_t *thread)
{
    void *item;
    bool  found;

    if (view->eject_callback) {
        while (true) {
            item = arr64_view_next(view, &found);
            if (!found) {
                break;
            }

            (*view->eject_callback)(item);
        }
    }

    mmm_retire(thread, view->contents);

    hatrack_free(view, sizeof(arr64_view_t));

    return;
}

void
arr64_view_delete(arr64_view_t *view)
{
    arr64_view_delete_mmm(view, mmm_thread_acquire());
}

static arr64_store_t *
arr64_new_store(uint64_t array_size, uint64_t store_size)
{
    arr64_store_t *ret;
    uint32_t       alloc_len;

    alloc_len = sizeof(arr64_store_t) + sizeof(arr64_cell_t) * store_size;
    ret       = (arr64_store_t *)mmm_alloc_committed(alloc_len);

    ret->store_size = store_size;

    atomic_store(&ret->array_size, array_size);

    return ret;
}

static void
arr64_migrate(arr64_store_t *store, arr64_t *top)
{
    arr64_store_t *next_store;
    arr64_store_t *expected_next;
    arr64_item_t   expected_item;
    arr64_item_t   candidate_item;
    uint64_t       i;
    uint64_t       new_array_len;
    uint64_t       new_store_len;

    if (atomic_read(&top->store) != store) {
        return;
    }

    next_store = atomic_read(&store->next);

    if (next_store) {
        goto help_move;
    }

    // Set those migration bits!
    for (i = 0; i < store->store_size; i++) {
        expected_item = atomic_read(&store->cells[i]);

        if (!(expected_item.state & ARR64_ARRAY_MOVING)) {
            if (expected_item.state & ARR64_ARRAY_USED) {
                OR2X64L(&store->cells[i], ARR64_ARRAY_MOVING);
            }
            else {
                OR2X64L(&store->cells[i], ARR64_ARRAY_MOVING | ARR64_ARRAY_MOVED);
            }
        }
    }

    // Now, fight to install the store.
    expected_next = 0;
    new_array_len = store->array_size;
    new_store_len = hatrack_round_up_to_power_of_2(new_array_len) << 1;
    next_store    = arr64_new_store(new_store_len, new_store_len);

    if (!CAS(&store->next, &expected_next, next_store)) {
        mmm_retire_unused(next_store);
        next_store = expected_next;
    }

    // Now, help move items that are moving.
help_move:
    for (i = 0; i < store->store_size; i++) {
        candidate_item = atomic_read(&store->cells[i]);
        if (candidate_item.state & ARR64_ARRAY_MOVED) {
            continue;
        }

        if (i < new_array_len) {
            expected_item.item   = NULL;
            expected_item.state  = 0;
            candidate_item.state = ARR64_ARRAY_USED;

            CAS(&next_store->cells[i], &expected_item, candidate_item);
            OR2X64L(&store->cells[i], ARR64_ARRAY_MOVED);
            continue;
        }

        /* If there are any items left in the current array, we eject
         * them, which requires us CASing in ARR64_ARRAY_MOVED, so that
         * we can decide upon a thread to call the ejection handler.
         */
        expected_item = candidate_item;
        candidate_item.state |= ARR64_ARRAY_MOVED;

        if (CAS(&store->cells[i], &expected_item, candidate_item)) {
            if (top->eject_callback) {
                (*top->eject_callback)(candidate_item.item);
            }
        }
    }

    // Okay, now swing the store pointer; winner retires the old store!
    if (CAS(&top->store, &store, next_store)) {
        if (!store->claimed) {
            mmm_retire(store);
        }
    }

    return;
}
