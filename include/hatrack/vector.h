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
 *  Name:           vector.h
 *  Description:    A wait-free vector, complete w/ push/pop/peek.
 *
 *  Author:         John Viega, john@zork.org
 */

#pragma once

#include "base.h"
#include "helpmanager.h"
#include "mmm.h"

typedef void (*vector_callback_t)(void *);

typedef struct {
    void   *item;
    int64_t state;
} vector_item_t;

typedef _Atomic vector_item_t vector_cell_t;

typedef struct vector_store_t vector_store_t;

typedef struct {
    int64_t           next_ix;
    int64_t           size;
    vector_store_t   *contents;
    vector_callback_t eject_callback;
} vector_view_t;

typedef struct {
    int64_t array_size;
    int64_t job_id;
} vec_size_info_t;

struct vector_store_t {
    int64_t                   store_size;
    _Atomic vec_size_info_t   array_size_info;
    _Atomic(vector_store_t *) next;
    _Atomic bool              claimed;
    vector_cell_t             cells[];
};

typedef struct {
    vector_callback_t         ret_callback;
    vector_callback_t         eject_callback;
    _Atomic(vector_store_t *) store;
    help_manager_t            help_manager;
} vector_t;

// clang-format off
HATRACK_EXTERN vector_t      *vector_new               (int64_t);
HATRACK_EXTERN void           vector_init              (vector_t *, int64_t, bool);
HATRACK_EXTERN void           vector_set_ret_callback  (vector_t *, vector_callback_t);
HATRACK_EXTERN void           vector_set_eject_callback(vector_t *, vector_callback_t);
HATRACK_EXTERN void           vector_cleanup           (vector_t *);
HATRACK_EXTERN void           vector_delete            (vector_t *);
HATRACK_EXTERN void          *vector_get_mmm           (vector_t *, mmm_thread_t *, int64_t, int *);
HATRACK_EXTERN void          *vector_get               (vector_t *, int64_t, int *);
HATRACK_EXTERN bool           vector_set_mmm           (vector_t *, mmm_thread_t *, int64_t, void *);
HATRACK_EXTERN bool           vector_set               (vector_t *, int64_t, void *);
HATRACK_EXTERN void           vector_grow_mmm          (vector_t *, mmm_thread_t *, int64_t);
HATRACK_EXTERN void           vector_grow              (vector_t *, int64_t);
HATRACK_EXTERN void           vector_shrink_mmm        (vector_t *, mmm_thread_t *, int64_t);
HATRACK_EXTERN void           vector_shrink            (vector_t *, int64_t);
HATRACK_EXTERN uint32_t       vector_len_mmm           (vector_t *, mmm_thread_t *);
HATRACK_EXTERN uint32_t       vector_len               (vector_t *);
HATRACK_EXTERN void           vector_push_mmm          (vector_t *, mmm_thread_t *, void *);
HATRACK_EXTERN void           vector_push              (vector_t *, void *);
HATRACK_EXTERN void          *vector_pop_mmm           (vector_t *, mmm_thread_t *, bool *);
HATRACK_EXTERN void          *vector_pop               (vector_t *, bool *);
HATRACK_EXTERN void          *vector_peek_mmm          (vector_t *, mmm_thread_t *, bool *);
HATRACK_EXTERN void          *vector_peek              (vector_t *, bool *);
HATRACK_EXTERN vector_view_t *vector_view_mmm          (vector_t *, mmm_thread_t *);
HATRACK_EXTERN vector_view_t *vector_view              (vector_t *);
HATRACK_EXTERN void          *vector_view_next         (vector_view_t *, bool *);
HATRACK_EXTERN void           vector_view_delete_mmm   (vector_view_t *, mmm_thread_t *);
HATRACK_EXTERN void           vector_view_delete       (vector_view_t *);
