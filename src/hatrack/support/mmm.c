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
 *  Name:           mmm.c
 *  Description:    Miniature memory manager: a malloc wrapper to support
 *                  linearization and safe reclaimation for my hash tables.
 *
 *  Author:         John Viega, john@zork.org
 *
 */

#include "hatrack/mmm.h"
#include "hatrack/counters.h"
#include "hatrack/malloc.h"
#include "hatrack/hatomic.h"
#include "hatrack/hatrack_common.h"

#ifndef HATRACK_NO_PTHREAD
#include <pthread.h>
#endif

#ifdef HATRACK_MMM_DEBUG
#include "hatrack/debug.h"
#include <string.h>
#endif

static _Atomic uint64_t mmm_nexttid;

_Atomic uint64_t mmm_epoch = HATRACK_EPOCH_FIRST;

static uint64_t mmm_reservations[HATRACK_THREADS_MAX];

#ifndef HATRACK_DONT_DEALLOC
static void mmm_empty(mmm_thread_t *thread);
#endif

#ifdef HATRACK_MMM_DEBUG
__attribute__((unused)) static void
hatrack_debug_mmm(void *addr, char *msg);

#define DEBUG_MMM(x, y) hatrack_debug_mmm((void *)(x), y)
#ifdef HATRACK_MMM_DEBUG
#define DEBUG_MMM_INTERNAL(x, y) DEBUG_MMM(x, y)
#else
#define DEBUG_MMM_INTERNAL(x, y)
#endif
#else
#define DEBUG_MMM(x, y)
#define DEBUG_MMM_INTERNAL(x, y)
#endif

#ifdef HATRACK_MMMALLOC_CTRS
#define HATRACK_MALLOC_CTR()        HATRACK_CTR(HATRACK_CTR_MALLOCS)
#define HATRACK_FREE_CTR()          HATRACK_CTR(HATRACK_CTR_FREES)
#define HATRACK_RETIRE_UNUSED_CTR() HATRACK_CTR(HATRACK_CTR_RETIRE_UNUSED)
#else
#define HATRACK_MALLOC_CTR()
#define HATRACK_FREE_CTR()
#define HATRACK_RETIRE_UNUSED_CTR() HATRACK_CTR(HATRACK_CTR_RETIRE_UNUSED)
#endif

static mmm_thread_t *mmm_thread_acquire_default(void *aux, size_t size);

static void                   *mmm_thread_aux;
static mmm_thread_acquire_func mmm_thread_acquire_fn = mmm_thread_acquire_default;

/*
 * We want to avoid overrunning the reservations array that our memory
 * management system uses.
 *
 * In all cases, the first HATRACK_THREADS_MAX threads to register
 * with us get a steadily increasing id. Also, threads will yield
 * their TID altogether if and when they call the mmm thread cleanup
 * routine-- mmm_clean_up_before_exit().
 *
 * The unused ID then goes on a linked list (mmm_free_tids), and
 * doesn't get touched until we run out of TIDs to give out.
 *
 */

_Atomic(mmm_free_tids_t *) mmm_free_tids;

// Call when a thread exits to add to the free TID stack.
static void
mmm_tid_giveback(mmm_thread_t *thread)
{
    mmm_free_tids_t *new_head;
    mmm_free_tids_t *old_head;

    new_head      = mmm_alloc(sizeof(mmm_free_tids_t));
    new_head->tid = thread->tid;
    old_head      = atomic_load(&mmm_free_tids);

    do {
        new_head->next = old_head;
    } while (!CAS(&mmm_free_tids, &old_head, new_head));
    thread->initialized = false;
}

// This is here for convenience of testing; generally this
// is not the way to handle tid recyling!
void
mmm_reset_tids(void)
{
    atomic_store(&mmm_nexttid, 0);
}

#ifndef HATRACK_NO_PTHREAD
static pthread_key_t mmm_thread_pkey;

struct mmm_pthread {
    size_t size;
    char   data[];
};

static void
mmm_thread_release_pthread(void *arg)
{
    pthread_setspecific(mmm_thread_pkey, NULL);

    struct mmm_pthread *pt = arg;
    mmm_thread_release((mmm_thread_t *)pt->data);
    hatrack_free(arg, pt->size);
}

static void
mmm_thread_acquire_init_pthread(void)
{
    pthread_key_create(&mmm_thread_pkey, mmm_thread_release_pthread);
}
#endif

static mmm_thread_t *
mmm_thread_acquire_default(void *aux, size_t size)
{
#ifdef HATRACK_NO_PTHREAD
    hatrack_panic("no implementation for mmm_thread_acquire defined");
#else
    static pthread_once_t init = PTHREAD_ONCE_INIT;
    pthread_once(&init, mmm_thread_acquire_init_pthread);

    struct mmm_pthread *pt = pthread_getspecific(mmm_thread_pkey);
    if (NULL == pt) {
        pt       = hatrack_zalloc(sizeof(struct mmm_pthread) + size);
        pt->size = size;
        pthread_setspecific(mmm_thread_pkey, pt);
    }
    return (mmm_thread_t *)pt->data;
#endif
}

mmm_thread_t *
mmm_thread_acquire(void)
{
    mmm_thread_t *thread = mmm_thread_acquire_fn(mmm_thread_aux,
                                                 sizeof(mmm_thread_t));

    if (!thread->initialized) {
        thread->initialized = true;

        /* We have a fixed number of TIDS to give out though (controlled by
         * the preprocessor variable, HATRACK_THREADS_MAX).  We give them out
         * sequentially till they're done, and then we give out ones that have
         * been "given back", which are stored on a stack (mmm_free_tids).
         *
         * If we finally run out, we panic.
         */
        thread->tid = atomic_fetch_add(&mmm_nexttid, 1);
        if (thread->tid >= HATRACK_THREADS_MAX) {
            mmm_free_tids_t *head = atomic_load(&mmm_free_tids);
            do {
                if (!head) {
                    hatrack_panic("HATRACK_THREADS_MAX limit reached");
                }
            } while (!CAS(&mmm_free_tids, &head, head->next));

            thread->tid = head->tid;
            mmm_retire(thread, head);
        }

        mmm_reservations[thread->tid] = HATRACK_EPOCH_UNRESERVED;
    }

    return thread;
}

/* For now, our cleanup function spins until it is able to retire
 * everything on its list. Soon, when we finally get around to
 * worrying about thread kills, we will change this to add its
 * contents to an "ophan" list.
 */
void
mmm_thread_release(mmm_thread_t *thread)
{
    if (thread->initialized) {
        mmm_end_op(thread);

#ifndef HATRACK_DONT_DEALLOC
        while (thread->retire_list) {
            mmm_empty(thread);
        }
#endif

        mmm_tid_giveback(thread);
    }
}

void
mmm_setthreadfns(mmm_thread_acquire_func acquirefn, void *aux)
{
    mmm_thread_acquire_fn = acquirefn != NULL ? acquirefn : mmm_thread_acquire_default;
    mmm_thread_aux        = aux;
}

/* Sets the retirement epoch on the pointer, and adds it to the
 * thread-local retirement list.
 *
 * We also keep a thread-local counter of how many times we've called
 * this function, and every HATRACK_RETIRE_FREQ calls, we run
 * mmm_empty(), which walks our thread-local retirement list, looking
 * for items to free.
 */

#ifndef HATRACK_DONT_DEALLOC
void
mmm_retire(mmm_thread_t *thread, void *ptr)
{
    mmm_header_t *cell;

    cell = mmm_get_header(ptr);

#ifdef HATRACK_MMM_DEBUG
    // Don't need this check when not debugging algorithms.
    if (!cell->write_epoch) {
        DEBUG_MMM_INTERNAL(ptr, "No write epoch??");
        hatrack_panic("no write epoch");
    }
    // Algorithms that steal bits from pointers might steal up to
    // three bits, thus the mask of 0x07.
    if (hatrack_pflag_test(ptr, 0x07)) {
        DEBUG_MMM_INTERNAL(ptr, "Bad alignment on retired pointer.");
        hatrack_panic("bad alignment on retired pointer");
    }

    /* Detect multiple threads adding this to their retire list.
     * Generally, you should be able to avoid this, but with
     * HATRACK_MMM_DEBUG on we explicitly check for it.
     */
    if (cell->retire_epoch) {
        DEBUG_MMM_INTERNAL(ptr, "Double free");
        DEBUG_PTR((void *)atomic_load(&mmm_epoch), "epoch of double free");
        hatrack_panic("double free");
    }
#endif

    cell->retire_epoch  = atomic_load(&mmm_epoch);
    cell->next          = thread->retire_list;
    thread->retire_list = cell;

    DEBUG_MMM_INTERNAL(cell->data, "mmm_retire");

    if (++thread->retire_ctr & HATRACK_RETIRE_FREQ) {
        thread->retire_ctr = 0;
        mmm_empty(thread);
    }

    return;
}

/* The basic gist of this algorithm is that we're going to look at
 * every reservation we can find, identifying the oldest reservation
 * in the list.
 *
 * Then, we can then safely free anything in the list with an earlier
 * retirement epoch than the reservation time. Since the items in the
 * stack were pushed on in order of their retirement epoch, it
 * suffices to find the first item that is lower than the target,
 * and free everything else.
 */
static void
mmm_empty(mmm_thread_t *thread)
{
    mmm_header_t *tmp;
    mmm_header_t *cell;
    uint64_t      lowest;
    uint64_t      reservation;
    uint64_t      lasttid;
    uint64_t      i;

    /* We don't have to search the whole array, just the items assigned
     * to active threads. Even if a new thread comes along, it will
     * not be able to reserve something that's already been retired
     * by the time we call this.
     */
    lasttid = atomic_load(&mmm_nexttid);

    if (lasttid > HATRACK_THREADS_MAX) {
        lasttid = HATRACK_THREADS_MAX;
    }

    /* We start out w/ the "lowest" reservation we've seen as
     * HATRACK_EPOCH_MAX.  If this value never changes, then it
     * means no epochs were reserved, and we can safely
     * free every record in our stack.
     */
    lowest = HATRACK_EPOCH_MAX;

    for (i = 0; i < lasttid; i++) {
        reservation = mmm_reservations[i];

        if (reservation < lowest) {
            lowest = reservation;
        }
    }

    /* The list here is ordered by retire epoch, with most recent on
     * top.  Go down the list until the NEXT cell is the first item we
     * should delete.
     *
     * Then, set the current cell's next pointer to NULL (since
     * it's the new end of the list), and then place the pointer at
     * the top of the list of cells to delete.
     *
     * Note that this function is only called if there's something
     * something on the retire list, so cell will never start out
     * empty.
     */
    cell = thread->retire_list;

    // Special-case this, in case we have to delete the head cell,
    // to make sure we reinitialize the linked list right.
    if (thread->retire_list->retire_epoch < lowest) {
        thread->retire_list = NULL;
    }
    else {
        while (true) {
            // We got to the end of the list, and didn't
            // find one we should bother deleting.
            if (!cell->next) {
                return;
            }

            if (cell->next->retire_epoch < lowest) {
                tmp       = cell;
                cell      = cell->next;
                tmp->next = NULL;
                break;
            }

            cell = cell->next;
        }
    }

    // Now cell and everything below it can be freed.
    while (cell) {
        tmp  = cell;
        cell = cell->next;
        HATRACK_FREE_CTR();
        DEBUG_MMM_INTERNAL(tmp->data, "mmm_empty::free");

        // Call the cleanup handler, if one exists.
        if (tmp->cleanup) {
            (*tmp->cleanup)(&tmp->data, tmp->cleanup_aux);
        }

        size_t actual_size = sizeof(mmm_header_t) + tmp->size;
        hatrack_free(tmp, actual_size);
    }

    return;
}

void
mmm_retire_unused(void *ptr)
{
    DEBUG_MMM_INTERNAL(ptr, "mmm_retire_unused");
    HATRACK_RETIRE_UNUSED_CTR();

    mmm_header_t *item = mmm_get_header(ptr);
    hatrack_free(item, sizeof(mmm_header_t) + item->size);

    return;
}

// Use this in migration functions to avoid unnecessary scanning of the
// retire list, when we know the epoch won't have changed.
void
mmm_retire_fast(mmm_thread_t *thread, void *ptr)
{
    mmm_header_t *cell;

    cell                = mmm_get_header(ptr);
    cell->retire_epoch  = atomic_load(&mmm_epoch);
    cell->next          = thread->retire_list;
    thread->retire_list = cell;

    return;
}

#endif

void
mmm_start_basic_op(mmm_thread_t *thread)
{
    mmm_reservations[thread->tid] = atomic_load(&mmm_epoch);

    return;
}

uint64_t
mmm_start_linearized_op(mmm_thread_t *thread)
{
    uint64_t read_epoch;

    mmm_reservations[thread->tid] = atomic_load(&mmm_epoch);
    read_epoch                    = atomic_load(&mmm_epoch);

    HATRACK_YN_CTR_NORET(read_epoch == mmm_reservations[thread->tid],
                         HATRACK_CTR_LINEAR_EPOCH_EQ);

    return read_epoch;
}

void
mmm_end_op(mmm_thread_t *thread)
{
    atomic_signal_fence(memory_order_seq_cst);
    mmm_reservations[thread->tid] = HATRACK_EPOCH_UNRESERVED;

    return;
}

void *
mmm_alloc(uint64_t size)
{
    uint64_t      actual_size = sizeof(mmm_header_t) + size;
    mmm_header_t *item        = hatrack_zalloc(actual_size);

    HATRACK_MALLOC_CTR();
    DEBUG_MMM_INTERNAL(item->data, "mmm_alloc");

    return (void *)item->data;
}

void *
mmm_alloc_committed(uint64_t size)
{
    uint64_t      actual_size = sizeof(mmm_header_t) + size;
    mmm_header_t *item        = hatrack_zalloc(actual_size);

    atomic_store(&item->write_epoch, atomic_fetch_add(&mmm_epoch, 1) + 1);

    HATRACK_MALLOC_CTR();
    DEBUG_MMM_INTERNAL(item->data, "mmm_alloc_committed");

    return (void *)item->data;
}

#ifdef HATRACK_PER_INSTANCE_AUX
void *
mmm_alloc_committed_aux(uint64_t size, void *aux)
{
    uint64_t      actual_size = sizeof(mmm_header_t) + size;
    mmm_header_t *item        = hatrack_zalloc_aux(actual_size, aux);

    atomic_store(&item->write_epoch, atomic_fetch_add(&mmm_epoch, 1) + 1);

    HATRACK_MALLOC_CTR();
    DEBUG_MMM_INTERNAL(item->data, "mmm_alloc_committed");

    return (void *)item->data;
}
#endif

void
mmm_add_cleanup_handler(void *ptr, mmm_cleanup_func handler, void *aux)
{
    mmm_header_t *header = mmm_get_header(ptr);

    header->cleanup     = handler;
    header->cleanup_aux = aux;

    return;
}

void
mmm_commit_write(void *ptr)
{
    uint64_t      cur_epoch;
    uint64_t      expected_value = 0;
    mmm_header_t *item           = mmm_get_header(ptr);

    cur_epoch = atomic_fetch_add(&mmm_epoch, 1) + 1;

    /* If this CAS operation fails, it can only be because:
     *
     *   1) We stalled in our write commit, some other thread noticed,
     *       and helped us out.
     *
     *   2) We were trying to help, but either the original thread, or
     *      some other helper got there first.
     *
     * Either way, we're safe to ignore the return value.
     */
    LCAS(&item->write_epoch, &expected_value, cur_epoch, HATRACK_CTR_COMMIT);
    DEBUG_MMM_INTERNAL(ptr, "committed");

    return;
}

void
mmm_help_commit(void *ptr)
{
    mmm_header_t *item;
    uint64_t      found_epoch;
    uint64_t      cur_epoch;

    item        = mmm_get_header(ptr);
    found_epoch = item->write_epoch;

    if (!found_epoch) {
        cur_epoch = atomic_fetch_add(&mmm_epoch, 1) + 1;
        LCAS(&item->write_epoch,
             &found_epoch,
             cur_epoch,
             HATRACK_CTR_COMMIT_HELPS);
    }

    return;
}

#ifdef HATRACK_MMM_DEBUG
/* Conceptually, this might belong in the debug subsystem. However,
 * this is a specific debug interface for outputting the epoch info
 * associated with an MMM allocation, and as such should live
 * independently from the more generic debug mechanism.
 *
 * As with the debug routines in the debug subsystem, we've sacrificed
 * readability for performance, since allocations can be VERY frequent
 * in test code.
 *
 * Specifically, I was originally just using sprintf() and it was
 * leading to a dramatic slowdown when debugging was on.
 */
static void
hatrack_debug_mmm(void *addr, char *msg)
{
    char buf[HATRACK_DEBUG_MSG_SIZE] = {
        '0',
        'x',
    };
    static const char rest[] = " :)  :r , :w , :c( :x0";
    const char       *r      = &rest[0];
    mmm_header_t     *hdr    = mmm_get_header(addr);
    char             *p;
    uint64_t          i;
    uintptr_t         n;

    p = buf + (HATRACK_PTR_CHRS + 3 * HATRACK_EPOCH_DEBUG_LEN + sizeof(rest));

    strncpy(p, msg, HATRACK_DEBUG_MSG_SIZE - (p - buf));

    *--p = *r++;
    *--p = *r++;
    *--p = *r++;
    n    = hdr->retire_epoch;

    for (i = 0; i < HATRACK_EPOCH_DEBUG_LEN; i++) {
        *--p = __hatrack_hex_conversion_table[n & 0xf];
        n >>= 4;
    }

    *--p = *r++;
    *--p = *r++;
    *--p = *r++;
    *--p = *r++;
    *--p = *r++;
    *--p = *r++;
    n    = hdr->write_epoch;

    for (i = 0; i < HATRACK_EPOCH_DEBUG_LEN; i++) {
        *--p = __hatrack_hex_conversion_table[n & 0xf];
        n >>= 4;
    }

    *--p = *r++;
    *--p = *r++;
    *--p = *r++;
    *--p = *r++;
    *--p = *r++;
    n    = hdr->create_epoch;

    for (i = 0; i < HATRACK_EPOCH_DEBUG_LEN; i++) {
        *--p = __hatrack_hex_conversion_table[n & 0xf];
        n >>= 4;
    }

    *--p = *r++;
    *--p = *r++;
    *--p = *r++;
    *--p = *r++;
    *--p = *r++;
    *--p = *r++;
    n    = (intptr_t)addr;

    for (i = 0; i < HATRACK_PTR_CHRS; i++) {
        *--p = __hatrack_hex_conversion_table[n & 0xf];
        n >>= 4;
    }

    *--p = *r++;
    *--p = *r++;

    hatrack_debug(p);

    return;
}
#endif
