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
 *  Name:           tiara.h
 *
 *  Description:    This Is A Rediculous Acronym.
 *
 *                  This is roughly in the hihat family, but uses
 *                  64-bit hash values, which we do not generally
 *                  recommend. However, it allows us to show off an
 *                  algorithm that requires only a single
 *                  compare-and-swap per core operation.
 *
 *                  There are a few other differences between this and
 *                  the rest of the hihat family:
 *
 *                  1) We do NOT keep an epoch value.  It would
 *                  require more space, which defeats the purpose.
 *
 *                  2) We still need two status bits, which normally
 *                     we'd steal from the epoch, or, in the lohat
 *                     family, from a pointer to a dynamically
 *                     allocated record.  We've already weakened the
 *                     collision resistance of the hash to an amount
 *                     that I'm not truly comfortable with, so instead
 *                     we steal it from the item field, meaning that
 *                     you CANNOT store integers in here, without
 *                     shifting them up at least two bits.
 *
 *                  We obviously could do a lot better if we could CAS
 *                  more at once, and it's not particularly clear to
 *                  me why modern architectures won't just let you do
 *                  atomic loads and CASs on entire cache lines.
 *
 *                  Unless there's a good reason, hopefully we'll see
 *                  that happen some day!
 *
 *  Author:         John Viega, john@zork.org
 */

#pragma once

#include "base.h"
#include "hatrack_common.h"
#include "mmm.h"

typedef struct {
    uint64_t hv;
    void    *item;
} tiara_record_t;

typedef _Atomic(tiara_record_t) tiara_bucket_t;

typedef struct tiara_store_st tiara_store_t;

struct tiara_store_st {
    uint64_t                 last_slot;
    uint64_t                 threshold;
    _Atomic uint64_t         used_count;
    _Atomic(tiara_store_t *) store_next;
    tiara_bucket_t           buckets[];
};

typedef struct {
    _Atomic(tiara_store_t *) store_current;
    _Atomic uint64_t         item_count;
} tiara_t;

// clang-format off
HATRACK_EXTERN tiara_t        *tiara_new        (void);
HATRACK_EXTERN tiara_t        *tiara_new_size   (char);
HATRACK_EXTERN void            tiara_init       (tiara_t *);
HATRACK_EXTERN void            tiara_init_size  (tiara_t *, char);
HATRACK_EXTERN void            tiara_cleanup    (tiara_t *);
HATRACK_EXTERN void            tiara_delete     (tiara_t *);
HATRACK_EXTERN void           *tiara_get_mmm    (tiara_t *, mmm_thread_t *, uint64_t);
HATRACK_EXTERN void           *tiara_get        (tiara_t *, uint64_t);
HATRACK_EXTERN void           *tiara_put_mmm    (tiara_t *, mmm_thread_t *, uint64_t, void *);
HATRACK_EXTERN void           *tiara_put        (tiara_t *, uint64_t, void *);
HATRACK_EXTERN void           *tiara_replace_mmm(tiara_t *, mmm_thread_t *, uint64_t, void *);
HATRACK_EXTERN void           *tiara_replace    (tiara_t *, uint64_t, void *);
HATRACK_EXTERN bool            tiara_add_mmm    (tiara_t *, mmm_thread_t *, uint64_t, void *);
HATRACK_EXTERN bool            tiara_add        (tiara_t *, uint64_t, void *);
HATRACK_EXTERN void           *tiara_remove_mmm (tiara_t *, mmm_thread_t *, uint64_t);
HATRACK_EXTERN void           *tiara_remove     (tiara_t *, uint64_t);
HATRACK_EXTERN uint64_t        tiara_len_mmm    (tiara_t *, mmm_thread_t *);
HATRACK_EXTERN uint64_t        tiara_len        (tiara_t *);
HATRACK_EXTERN hatrack_view_t *tiara_view_mmm   (tiara_t *, mmm_thread_t *, uint64_t *, bool);
HATRACK_EXTERN hatrack_view_t *tiara_view       (tiara_t *, uint64_t *, bool);
