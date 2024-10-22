/*
 * copyright © 2022 John Viega
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
 *  Name:           flexarray.c
 *  Description:    A fast, wait-free flex array.
 *
 *                  This ONLY allows indexing and resizing the array.
 *                  If you need append/pop operations in addition, see
 *                  the vector_t type.
 *
 *  Author:         John Viega, john@zork.org
 */

#include "hatrack/flexarray.h"
#include "hatrack/malloc.h"
#include "hatrack/mmm.h"
#include "hatrack/hatomic.h"
#include "hatrack/hatrack_common.h"
#include "../hatrack-internal.h"

#define FLEXARRAY_MIN_STORE_SZ_LOG 4

enum64(flex_enum_t,
       FLEX_ARRAY_SHRINK = 0x8000000000000000,
       FLEX_ARRAY_MOVING = 0x4000000000000000,
       FLEX_ARRAY_MOVED  = 0x2000000000000000,
       FLEX_ARRAY_USED   = 0x1000000000000000);

static flex_store_t *flexarray_new_store(uint64_t, uint64_t);
static void          flexarray_migrate(flex_store_t *, mmm_thread_t *, flexarray_t *);

/* The size parameter is the one larger than the largest allowable index.
 * The underlying store may be bigger-- it will be sized up to the next
 * power of two.
 */
flexarray_t *
flexarray_new(uint64_t initial_size)
{
    flexarray_t *arr;

    arr = (flexarray_t *)hatrack_malloc(sizeof(flexarray_t));

    flexarray_init(arr, initial_size);

    return arr;
}

void
flexarray_init(flexarray_t *arr, uint64_t initial_size)
{
    uint64_t store_size;

    arr->ret_callback   = NULL;
    arr->eject_callback = NULL;
    store_size          = hatrack_round_up_to_power_of_2(initial_size);

    if (store_size < (1 << FLEXARRAY_MIN_STORE_SZ_LOG)) {
        store_size = 1 << FLEXARRAY_MIN_STORE_SZ_LOG;
    }

    atomic_store(&arr->store, flexarray_new_store(initial_size, store_size));

    return;
}

void
flexarray_set_ret_callback(flexarray_t *self, flex_callback_t callback)
{
    self->ret_callback = callback;

    return;
}

void
flexarray_set_eject_callback(flexarray_t *self, flex_callback_t callback)
{
    self->eject_callback = callback;
}

void
flexarray_cleanup(flexarray_t *self)
{
    flex_store_t *store;
    uint64_t      i;
    flex_item_t   item;

    store = atomic_load(&self->store);

    if (self->eject_callback) {
        for (i = 0; i < store->array_size; i++) {
            item = atomic_read(&store->cells[i]);
            if (item.state) {
                (*self->eject_callback)(item.item);
            }
        }
    }

    mmm_retire_unused(self->store);

    return;
}

void
flexarray_delete(flexarray_t *self)
{
    flexarray_cleanup(self);
    hatrack_free(self, sizeof(flexarray_t));

    return;
}

uint64_t
flexarray_len(flexarray_t *self)
{
    flex_store_t *store = atomic_read(&self->store);

    return atomic_read(&store->array_size);
}

uint64_t
flexarray_len_mmm(flexarray_t *self, mmm_thread_t *thread)
{
    return flexarray_len(self);
}

void *
flexarray_get_mmm(flexarray_t *self, mmm_thread_t *thread, uint64_t index, int *status)
{
    flex_item_t   current;
    flex_store_t *store;

    mmm_start_basic_op(thread);

    store = atomic_read(&self->store);

    if (index >= atomic_read(&store->array_size)) {
        if (status) {
            *status = FLEX_OOB;
        }
        mmm_end_op(thread);
        return NULL;
    }

    // A resize is in progress, item is not there yet.
    if (index >= store->store_size) {
        if (status) {
            *status = FLEX_UNINITIALIZED;
        }
        mmm_end_op(thread);
        return NULL;
    }

    current = atomic_read(&store->cells[index]);

    if (!(current.state & FLEX_ARRAY_USED)) {
        if (status) {
            *status = FLEX_UNINITIALIZED;
        }
        mmm_end_op(thread);
        return NULL;
    }

    if (self->ret_callback && current.item) {
        (*self->ret_callback)(current.item);
    }

    mmm_end_op(thread);

    if (status) {
        *status = FLEX_OK;
    }

    return current.item;
}

void *
flexarray_get(flexarray_t *self, uint64_t index, int *status)
{
    return flexarray_get_mmm(self, mmm_thread_acquire(), index, status);
}

// Returns true if successful, false if write would be out-of-bounds.
bool
flexarray_set_mmm(flexarray_t *self, mmm_thread_t *thread, uint64_t index, void *item)
{
    flex_store_t *store;
    flex_item_t   current;
    flex_item_t   candidate;
    flex_cell_t  *cellptr;
    uint64_t      read_index;

    mmm_start_basic_op(thread);

    store      = atomic_read(&self->store);
    read_index = atomic_read(&store->array_size) & ~FLEX_ARRAY_SHRINK;

    if (index >= read_index) {
        mmm_end_op(thread);
        return false;
    }

    if (index >= store->store_size) {
        flexarray_migrate(store, thread, self);
        mmm_end_op(thread);
        return flexarray_set_mmm(self, thread, index, item);
    }
    else {
        if (index >= store->array_size) {
            flexarray_grow_mmm(self, thread, index);
        }
    }

    cellptr = &store->cells[index];
    current = atomic_read(cellptr);

    if (current.state & FLEX_ARRAY_MOVING) {
        flexarray_migrate(store, thread, self);
        mmm_end_op(thread);
        return flexarray_set_mmm(self, thread, index, item);
    }

    candidate.item  = item;
    candidate.state = FLEX_ARRAY_USED;

    if (CAS(cellptr, &current, candidate)) {
        if (self->eject_callback && (current.state == FLEX_ARRAY_USED)) {
            (*self->eject_callback)(current.item);
        }
        mmm_end_op(thread);
        return true;
    }

    if (current.state & FLEX_ARRAY_MOVING) {
        flexarray_migrate(store, thread, self);
        mmm_end_op(thread);
        return flexarray_set_mmm(self, thread, index, item);
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
flexarray_set(flexarray_t *self, uint64_t index, void *item)
{
    return flexarray_set_mmm(self, mmm_thread_acquire(), index, item);
}

void
flexarray_grow_mmm(flexarray_t *self, mmm_thread_t *thread, uint64_t index)
{
    flex_store_t *store;
    uint64_t      array_size;

    mmm_start_basic_op(thread);

    /* Just change store->array_size, kick off a migration if
     * necessary, and be done.
     */
    do {
        store      = atomic_read(&self->store);
        array_size = atomic_read(&store->array_size);

        /* If we're shrinking, we don't want to re-expand until we
         * know that truncated cells are zeroed out.
         */
        if (array_size & FLEX_ARRAY_SHRINK) {
            flexarray_migrate(store, thread, self);
            continue;
        }

        if (index < array_size) {
            mmm_end_op(thread);
            return;
        }
    } while (!CAS(&store->array_size, &array_size, index));

    if (index > store->store_size) {
        flexarray_migrate(store, thread, self);
    }

    mmm_end_op(thread);
    return;
}

void
flexarray_grow(flexarray_t *self, uint64_t index)
{
    flexarray_grow_mmm(self, mmm_thread_acquire(), index);
}

void
flexarray_shrink_mmm(flexarray_t *self, mmm_thread_t *thread, uint64_t index)
{
    flex_store_t *store;
    uint64_t      array_size;

    index |= FLEX_ARRAY_SHRINK;

    mmm_start_basic_op(thread);

    do {
        store      = atomic_read(&self->store);
        array_size = atomic_read(&store->array_size);

        if (index > array_size) {
            mmm_end_op(thread);
            return;
        }
    } while (!CAS(&store->array_size, &array_size, index));

    flexarray_migrate(store, thread, self);

    mmm_end_op(thread);
    return;
}

void
flexarray_shrink(flexarray_t *self, uint64_t index)
{
    flexarray_shrink_mmm(self, mmm_thread_acquire(), index);
}

flex_view_t *
flexarray_view_mmm(flexarray_t *self, mmm_thread_t *thread)
{
    flex_view_t  *ret;
    flex_store_t *store;
    bool          expected;
    uint64_t      i;
    flex_item_t   item;

    mmm_start_basic_op(thread);

    while (true) {
        store    = atomic_read(&self->store);
        expected = false;

        if (CAS(&store->claimed, &expected, true)) {
            break;
        }
        flexarray_migrate(store, thread, self);
    }

    flexarray_migrate(store, thread, self);

    if (self->ret_callback) {
        for (i = 0; i < store->array_size; i++) {
            item = atomic_read(&store->cells[i]);
            if (item.state & FLEX_ARRAY_USED) {
                (*self->ret_callback)(item.item);
            }
        }
    }

    mmm_end_op(thread);

    ret           = (flex_view_t *)hatrack_malloc(sizeof(flex_view_t));
    ret->contents = store;
    ret->next_ix  = 0;

    return ret;
}

flex_view_t *
flexarray_view(flexarray_t *self)
{
    return flexarray_view_mmm(self, mmm_thread_acquire());
}

void *
flexarray_view_next(flex_view_t *view, int *found)
{
    flex_item_t item;

    while (true) {
        if (view->next_ix >= view->contents->array_size) {
            if (found) {
                *found = 0;
            }
            return NULL;
        }

        item = atomic_read(&view->contents->cells[view->next_ix++]);

        if (item.state & FLEX_ARRAY_USED) {
            if (found) {
                *found = 1;
            }
            return item.item;
        }
    }
}

void
flexarray_view_delete_mmm(flex_view_t *view, mmm_thread_t *thread)
{
    void *item;
    int   found;

    if (view->eject_callback) {
        while (true) {
            item = flexarray_view_next(view, &found);
            if (!found) {
                break;
            }

            (*view->eject_callback)(item);
        }
    }

    mmm_retire(thread, view->contents);

    hatrack_free(view, sizeof(flex_view_t));

    return;
}

void
flexarray_view_delete(flex_view_t *view)
{
    flexarray_view_delete_mmm(view, mmm_thread_acquire());
}

void *
flexarray_view_get(flex_view_t *view, uint64_t ix, int *err)
{
    flex_item_t item;

    if (ix >= view->contents->array_size) {
        if (err) {
            *err = FLEX_OOB;
        }
        return NULL;
    }

    item = atomic_read(&view->contents->cells[ix]);

    if (!(item.state & FLEX_ARRAY_USED)) {
        if (err) {
            *err = FLEX_UNINITIALIZED;
        }
        return NULL;
    }
    if (err) {
        *err = FLEX_OK;
    }
    return item.item;
}

/* This assumes the view is non-mutable. We still use atomic_read(), but
 * this generally no-ops out anyway.
 */
uint64_t
flexarray_view_len(flex_view_t *view)
{
    return view->contents->array_size;
}

flexarray_t *
flexarray_add_mmm(flexarray_t *arr1, mmm_thread_t *thread, flexarray_t *arr2)
{
    flexarray_t  *res   = (flexarray_t *)hatrack_malloc(sizeof(flexarray_t));
    flex_view_t  *v1    = flexarray_view_mmm(arr1, thread);
    flex_view_t  *v2    = flexarray_view_mmm(arr2, thread);
    flex_store_t *s1    = v1->contents;
    flex_store_t *s2    = v2->contents;
    uint64_t      v1_sz = atomic_read(&s1->array_size);
    uint64_t      v2_sz = atomic_read(&s2->array_size);

    res->ret_callback   = arr1->ret_callback;
    res->eject_callback = arr1->eject_callback;

    atomic_store(&res->store, s1);
    hatrack_free(v1, sizeof(flex_view_t));

    if (v1_sz + v2_sz > s1->array_size) {
        flexarray_grow_mmm(res, thread, v1_sz + v2_sz);
        s1 = atomic_load(&res->store);
    }

    for (uint64_t i = 0; i < v2_sz; i++) {
        atomic_store(&s1->cells[v1_sz++], s2->cells[i]);
    }

    atomic_store(&(s1->array_size), v1_sz);
    mmm_retire_unused(s2);
    hatrack_free(v2, sizeof(flex_view_t));

    return res;
}

flexarray_t *
flexarray_add(flexarray_t *arr1, flexarray_t *arr2)
{
    return flexarray_add_mmm(arr1, mmm_thread_acquire(), arr2);
}

static flex_store_t *
flexarray_new_store(uint64_t array_size, uint64_t store_size)
{
    flex_store_t *ret;
    uint32_t      alloc_len;

    alloc_len = sizeof(flex_store_t) + sizeof(flex_cell_t) * store_size;
    ret       = (flex_store_t *)mmm_alloc_committed(alloc_len);

    ret->store_size = store_size;

    atomic_store(&ret->array_size, array_size);

    return ret;
}

static void
flexarray_migrate(flex_store_t *store, mmm_thread_t *thread, flexarray_t *top)
{
    flex_store_t *next_store;
    flex_store_t *expected_next;
    flex_item_t   expected_item;
    flex_item_t   candidate_item;
    uint64_t      i;
    uint64_t      new_array_len;
    uint64_t      new_store_len;

    if (atomic_read(&top->store) != store) {
        return;
    }

    next_store = atomic_read(&store->next);

    if (next_store) {
        new_array_len = store->array_size;
        goto help_move;
    }

    // Set those migration bits!
    for (i = 0; i < store->store_size; i++) {
        expected_item = atomic_read(&store->cells[i]);

        if (!(expected_item.state & FLEX_ARRAY_MOVING)) {
            if (expected_item.state & FLEX_ARRAY_USED) {
                OR2X64L(&store->cells[i], FLEX_ARRAY_MOVING);
            }
            else {
                OR2X64L(&store->cells[i], FLEX_ARRAY_MOVING | FLEX_ARRAY_MOVED);
            }
        }
    }

    // Now, fight to install the store.
    expected_next = 0;
    new_array_len = store->array_size;
    new_store_len = hatrack_round_up_to_power_of_2(new_array_len) << 1;
    next_store    = flexarray_new_store(new_array_len, new_store_len);

    if (!CAS(&store->next, &expected_next, next_store)) {
        mmm_retire_unused(next_store);
        next_store = expected_next;
    }

    // Now, help move items that are moving.
help_move:
    for (i = 0; i < store->store_size; i++) {
        candidate_item = atomic_read(&store->cells[i]);
        if (candidate_item.state & FLEX_ARRAY_MOVED) {
            continue;
        }

        if (i < new_array_len) {
            expected_item.item   = NULL;
            expected_item.state  = 0;
            candidate_item.state = FLEX_ARRAY_USED;

            CAS(&next_store->cells[i], &expected_item, candidate_item);
            OR2X64L(&store->cells[i], FLEX_ARRAY_MOVED);
            continue;
        }

        /* If there are any items left in the current array, we eject
         * them, which requires us CASing in FLEX_ARRAY_MOVED, so that
         * we can decide upon a thread to call the ejection handler.
         */
        expected_item = candidate_item;
        candidate_item.state |= FLEX_ARRAY_MOVED;

        if (CAS(&store->cells[i], &expected_item, candidate_item)) {
            if (top->eject_callback) {
                (*top->eject_callback)(candidate_item.item);
            }
        }
    }

    // Okay, now swing the store pointer; winner retires the old store!
    if (CAS(&top->store, &store, next_store)) {
        if (!store->claimed) {
            mmm_retire(thread, store);
        }
    }

    return;
}
