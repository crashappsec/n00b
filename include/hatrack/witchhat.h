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
 *  Name:           witchhat.h
 *  Description:    Waiting I Trully Cannot Handle
 *
 *                  This is a lock-free, and wait freehash table,
 *                  without consistency / full ordering.
 *
 *                  Note that witchhat is based on hihat1, with a
 *                  helping mechanism in place to ensure wait freedom.
 *                  There are only a few places in hihat1 where we
 *                  need such a mechanism, so we will only comment on
 *                  those places.
 *
 *                  Refer to hihat.h and hihat.c for more detail on
 *                  the core algorithm, as here, we only comment on
 *                  the things that are different about witchhat.
 *
 *  Author: John Viega, john@zork.org
 */

#pragma once

#include "base.h"
#include "hatrack_common.h"
#include "mmm.h"

typedef struct {
    void    *item;
    uint64_t info;
} witchhat_record_t;

typedef struct {
    _Atomic hatrack_hash_t    hv;
    _Atomic witchhat_record_t record;
} witchhat_bucket_t;

typedef struct witchhat_store_st witchhat_store_t;

struct witchhat_store_st {
    uint64_t                    last_slot;
    uint64_t                    threshold;
    _Atomic uint64_t            used_count;
    _Atomic(witchhat_store_t *) store_next;
    witchhat_bucket_t           buckets[];
};

typedef struct {
    _Atomic(witchhat_store_t *) store_current;
    _Atomic uint64_t            item_count;
    _Atomic uint64_t            help_needed;
    uint64_t                    next_epoch;
} witchhat_t;

// clang-format off
HATRACK_EXTERN witchhat_t     *witchhat_new        (void);
HATRACK_EXTERN witchhat_t     *witchhat_new_size   (char);
HATRACK_EXTERN void            witchhat_init       (witchhat_t *);
HATRACK_EXTERN void            witchhat_init_size  (witchhat_t *, char);
HATRACK_EXTERN void            witchhat_cleanup    (witchhat_t *);
HATRACK_EXTERN void            witchhat_delete     (witchhat_t *);
HATRACK_EXTERN void           *witchhat_get_mmm    (witchhat_t *, mmm_thread_t *, hatrack_hash_t, bool *);
HATRACK_EXTERN void           *witchhat_get        (witchhat_t *, hatrack_hash_t, bool *);
HATRACK_EXTERN void           *witchhat_put_mmm    (witchhat_t *, mmm_thread_t *, hatrack_hash_t, void *, bool *);
HATRACK_EXTERN void           *witchhat_put        (witchhat_t *, hatrack_hash_t, void *, bool *);
HATRACK_EXTERN void           *witchhat_replace_mmm(witchhat_t *, mmm_thread_t *, hatrack_hash_t, void *, bool *);
HATRACK_EXTERN void           *witchhat_replace    (witchhat_t *, hatrack_hash_t, void *, bool *);
HATRACK_EXTERN bool            witchhat_add_mmm    (witchhat_t *, mmm_thread_t *, hatrack_hash_t, void *);
HATRACK_EXTERN bool            witchhat_add        (witchhat_t *, hatrack_hash_t, void *);
HATRACK_EXTERN void           *witchhat_remove_mmm (witchhat_t *, mmm_thread_t *, hatrack_hash_t, bool *);
HATRACK_EXTERN void           *witchhat_remove     (witchhat_t *, hatrack_hash_t, bool *);
HATRACK_EXTERN uint64_t        witchhat_len_mmm    (witchhat_t *, mmm_thread_t *);
HATRACK_EXTERN uint64_t        witchhat_len        (witchhat_t *);
HATRACK_EXTERN hatrack_view_t *witchhat_view_mmm   (witchhat_t *, mmm_thread_t *, uint64_t *, bool);
HATRACK_EXTERN hatrack_view_t *witchhat_view       (witchhat_t *, uint64_t *, bool);
// clang-format on 
