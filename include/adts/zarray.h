/*
 * Copyright © 2024 Crash Override, Inc.
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
 *  Name:           zeroarray.h
 *  Description:    A fast array that can expand without relocating.
 *
 *                  The idea with zeroarray is to use zero-mapped
 *                  pages to reserve a size far bigger than we ever
 *                  think we might need.
 *
 *                  Modern OSes are happy to give out large
 *                  allocations where each page, by default, maps to
 *                  the zero page, so uses no memory until there's a
 *                  write, at which point copy-on-write semantics kick
 *                  in.
 *
 *                  There generally won't even be entries in the page
 *                  table until the memory is used, so we can safely
 *                  allocate fairly large chunks.
 *
 *                  The primary motivation for this exact data
 *                  structure though is for the n00b's garbage
 *                  collector-- we will keep information about the
 *                  garbage collector's roots in a zero-array. Here,
 *                  multiple threads might need to add roots, and
 *                  contention to write might be possible, but read
 *                  contention should not be an issue.
 *
 *                  Basically, growing the array is done by
 *                  fetch-and-add of an index; we specify the cell
 *                  size up front.
 *
 *                  The total allocation size for a zeroarray is
 *                  fixed; instead of doing a linked list or a
 *                  migration, we just keep it simple. In the case of
 *                  the N00b GC, we can eventually determine how much
 *                  space we need for storing roots statically, but we
 *                  never really will need a whole ton of space, so if
 *                  we alloc a MB we'd never use it all (it'd hold
 *                  info on over 60k roots).
 *
 *                  The interface actually is calloc-like in that you
 *                  provide the maximum number of data objects you
 *                  want to be able to store, and the size for the
 *                  data objects in bytes-- the size must already be
 *                  16-byte aligned.
 *
 *                  Any requested allocation amount will actually be
 *                  rounded up to a multiple of the page size.
 *
 *                  Additionally, we put guard pages around the
 *                  allocation.
 *
 *                  Cell slots are given out atomically via FAA.
 *
 *                  It's assumed the caller deals with synchronization
 *                  on the cell level if needed; n00b's GC doesn't
 *                  really need it; writes only happen once, and write
 *                  in an order where there's never any issue.
 *
 *  Author: John Viega, john@crashoverride.com
 */

#pragma once
#include <stdint.h>

typedef struct {
    _Atomic uint32_t length;
    uint32_t         last_item;
    uint32_t         cell_size;
    uint32_t         alloc_len;
    alignas(16) char data[];
} n00b_zarray_t;

extern n00b_zarray_t *n00b_zarray_new(uint32_t, uint32_t);
extern void          *n00b_zarray_cell_address(n00b_zarray_t *,
                                               uint32_t);
extern uint32_t       n00b_zarray_new_cell(n00b_zarray_t *,
                                           void **);
extern uint32_t       n00b_zarray_len(n00b_zarray_t *);
extern void           n00b_zarray_delete(n00b_zarray_t *);
extern n00b_zarray_t *n00b_zarray_unsafe_copy(n00b_zarray_t *);
