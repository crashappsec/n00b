/*
 * copyright © 2024 Crash Override
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
 *  Name:           zeroarray.c
 *  Description:    Intended initially for n00b GC root management.
 *
 *  Author:         John Viega, john@crashoverride.com
 */

#include "hatrack/zeroarray.h"
#include "hatrack/hatomic.h"
#include "../hatrack-internal.h"
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>

hatrack_zarray_t *
hatrack_zarray_new(uint32_t max_items, uint32_t item_size)
{
    // Make sure alignment is good.
    if (item_size & 0x7) {
        return NULL;
    }

    int      page_size = getpagesize();
    uint32_t len       = item_size * max_items + sizeof(hatrack_zarray_t);

    // Add in space for two guard pages, and then (page_size - 1)
    // bytes extra in case the allocation puts us one byte over a
    // page. The page size is always a power of two, so we then can
    // truncate by subtracting 1 from page size, taking the
    // complement, and anding the two values together.

    len += (3 * page_size) - 1;

    len &= ~(page_size - 1);

    char *full_alloc = mmap(NULL,
                            len,
                            PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANON,
                            0,
                            0);

    char *ret_point = full_alloc + page_size;
    char *guard     = full_alloc + len - page_size;

    mprotect(full_alloc, page_size, PROT_NONE);
    mprotect(guard, page_size, PROT_NONE);

    hatrack_zarray_t *ret = (hatrack_zarray_t *)ret_point;
    ret->last_item        = max_items;
    ret->cell_size        = item_size;
    ret->alloc_len        = len;

    return ret;
}

void *
hatrack_zarray_cell_address(hatrack_zarray_t *arr, uint32_t n)
{
    if (n > arr->last_item) {
        return NULL;
    }

    return (void *)(&arr->data[arr->cell_size * n]);
}

uint32_t
hatrack_zarray_new_cell(hatrack_zarray_t *arr, void **loc)
{
    uint32_t ret = atomic_fetch_add(&arr->length, 1);

    if (loc) {
        if (ret >= arr->last_item) {
            *loc = NULL;
        }
        else {
            *loc = hatrack_zarray_cell_address(arr, ret);
        }
    }

    return ret;
}

hatrack_zarray_t *
hatrack_zarray_unsafe_copy(hatrack_zarray_t *old)
{
    // This works if the array is write-once. Otherwise, who knows
    // what you will get.
    uint32_t actual_len   = atomic_load(&old->length);
    hatrack_zarray_t *new = hatrack_zarray_new(old->alloc_len, old->cell_size);

    atomic_store(&new->length, actual_len);
    uint32_t total_copy_len = actual_len * new->cell_size;
    memcpy(new->data, old->data, total_copy_len);

    return new;
}

uint32_t
hatrack_zarray_len(hatrack_zarray_t *arr)
{
    return atomic_load(&arr->length);
}

void
hatrack_zarray_delete(hatrack_zarray_t *arr)
{
    char *p = (char *)arr;

    p -= getpagesize();
    munmap(p, arr->alloc_len);
}
