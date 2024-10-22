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
 *  Name:           arr64.h
 *  Description:    A fast, wait-free flex array using only a 64-bit CAS.
 *
 *                  This ONLY allows indexing and resizing the array.
 *
 *  Author:         John Viega, john@zork.org
 */

#pragma once

#include "base.h"
#include "mmm.h"

typedef void (*arr64_callback_t)(void *);

typedef uint64_t arr64_item_t;

typedef _Atomic arr64_item_t arr64_cell_t;

typedef struct arr64_store_t arr64_store_t;

typedef struct {
    uint64_t         next_ix;
    arr64_store_t   *contents;
    arr64_callback_t eject_callback;
} arr64_view_t;

struct arr64_store_t {
    uint64_t                 store_size;
    _Atomic uint64_t         array_size;
    _Atomic(arr64_store_t *) next;
    _Atomic bool             claimed;
    arr64_cell_t             cells[];
};

typedef struct {
    arr64_callback_t         ret_callback;
    arr64_callback_t         eject_callback;
    _Atomic(arr64_store_t *) store;
} arr64_t;

// clang-format off
HATRACK_EXTERN arr64_t      *arr64_new               (uint64_t);
HATRACK_EXTERN void          arr64_init              (arr64_t *, uint64_t);
HATRACK_EXTERN void          arr64_set_ret_callback  (arr64_t *, arr64_callback_t);
HATRACK_EXTERN void          arr64_set_eject_callback(arr64_t *, arr64_callback_t);
HATRACK_EXTERN void          arr64_cleanup           (arr64_t *);
HATRACK_EXTERN void          arr64_delete            (arr64_t *);
HATRACK_EXTERN void         *arr64_get               (arr64_t *, uint64_t, int *);
HATRACK_EXTERN bool          arr64_set               (arr64_t *, uint64_t, void *);
HATRACK_EXTERN void          arr64_grow_mmm          (arr64_t *, mmm_thread_t *, uint64_t);
HATRACK_EXTERN void          arr64_grow              (arr64_t *, uint64_t);
HATRACK_EXTERN void          arr64_shrink            (arr64_t *, uint64_t);
HATRACK_EXTERN uint32_t      arr64_len               (arr64_t *);
HATRACK_EXTERN arr64_view_t *arr64_view_mmm          (arr64_t *, mmm_thread_t *);
HATRACK_EXTERN arr64_view_t *arr64_view              (arr64_t *);
HATRACK_EXTERN void         *arr64_view_next         (arr64_view_t *, bool *);
HATRACK_EXTERN void          arr64_view_delete_mmm   (arr64_view_t *, mmm_thread_t *);
HATRACK_EXTERN void          arr64_view_delete       (arr64_view_t *);
