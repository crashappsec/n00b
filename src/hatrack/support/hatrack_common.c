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
 *  Name:           hatrack_common.c
 *  Description:    Functionality shared across all our default hash tables.
 *                  Most of it consists of short inlined functions, which
 *                  live in hatrack_common.h
 *
 *  Author:         John Viega, john@zork.org
 */

#include "hatrack/hatrack_common.h"
#include "hatrack/malloc.h"

#include <stdlib.h>
#include <string.h>

void
hatrack_view_delete(hatrack_view_t *view, uint64_t num)
{
    hatrack_free(view, sizeof(hatrack_view_t) * num);
}

/* Used when using quicksort to sort the contents of a hash table
 * 'view' by insertion time (the sort_epoch field).
 */
int
hatrack_quicksort_cmp(const void *bucket1, const void *bucket2)
{
    hatrack_view_t *item1;
    hatrack_view_t *item2;

    item1 = (hatrack_view_t *)bucket1;
    item2 = (hatrack_view_t *)bucket2;

    return item1->sort_epoch - item2->sort_epoch;
}

[[noreturn]] static void
hatrack_default_panic(void *arg, const char *msg)
{
    extern ssize_t write(int, const void *, size_t) __attribute__((weak));
    if (write != NULL) {
        static const char prefix[] = "Hatrack PANIC: ";
        (void)write(2, prefix, sizeof(prefix) - 1);
        (void)write(2, msg, strlen(msg));
        (void)write(2, "\n", 1);
    }

    // no need to check for NULL here. either abort will be called or we'll
    // crash with a call to NULL pointer. Either way, we get what we want.
    [[noreturn]] extern void abort(void) __attribute__((weak));
    abort();
}

static hatrack_panic_func hatrack_panic_fn  = hatrack_default_panic;
static void              *hatrack_panic_arg = NULL;

void
hatrack_setpanicfn(hatrack_panic_func panicfn, void *arg)
{
    hatrack_panic_fn  = panicfn != NULL ? panicfn : hatrack_default_panic;
    hatrack_panic_arg = arg;
}

void
hatrack_panic(const char *msg)
{
    hatrack_panic_fn(hatrack_panic_arg, msg);
    // this should be unreachable, but just in case ...
    hatrack_default_panic(NULL, msg);
}
