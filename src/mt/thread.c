#define N00B_USE_INTERNAL_API
#include "n00b.h"

n00b_thread_t *n00b_thread_to_cancel = NULL;
n00b_futex_t   n00b_cancel_completed = 0;

#if defined(__linux__)
void
n00b_thread_stack_region(n00b_thread_t *t)
{
    pthread_t self = pthread_self();

    pthread_attr_t attrs;

    pthread_getattr_np(self, &attrs);

    size_t size;

    pthread_attr_getstack(&attrs, (void **)&t->cur, &size);
#if 0
    t->base = __builtin_frame_address(0);
#else
    t->base = t->cur + size;
#endif
}

#elif defined(__APPLE__) || defined(BSD)

// Apple at least has no way to get the thread's attr struct that
// I can find. But it does provide an API to get at the same data.
void
n00b_thread_stack_region(n00b_thread_t *t)
{
    pthread_t self = pthread_self();

    t->base = pthread_get_stackaddr_np(self);

#if N00B_USE_FRAME_INTRINSIC
    t->cur = __builtin_frame_address(0);
#else
    void *ptr;

    t->base = (void *)n00b_round_up_to_given_power_of_2((uint64_t)t->base, 8);
// For M1s only.
#ifndef N00B_N0_STACK_ASM
    __asm volatile("mov  %0, sp" : "=r"(ptr) :);
    t->cur = ptr + N00B_STACK_SLOP;
#else
    t->cur = &ptr + N00B_STACK_SLOP;
#endif

    t->cur = (void *)n00b_round_up_to_given_power_of_2((uint64_t)t->cur, 8);
    // #endif
    //  I don't know why this needs to get set; the value added when launching
    //  gets zeroed out at some point after tsi is supposedly initialized.
    t->tsi = n00b_get_tsi_ptr();
#endif
}
#else
#error "Unsupported platform."
#endif

static inline void
n00b_release_locks_on_thread_exit(n00b_tsi_t *tsi)
{
    while (tsi->exclusive_locks) {
        n00b_dlog_lock("On exit, thread held write lock @%p; releasing.",
                       tsi->exclusive_locks);
        n00b_lock_release(tsi->exclusive_locks);
        n00b_barrier();
    }

    n00b_lock_log_t *log = tsi->read_locks;

    while (log) {
        n00b_lock_release(log->obj);
        log = log->next_entry;
    }
    n00b_dlog_thread1("Thread has no remaining locks held.");
}

void *
n00b_post_thread_cleanup(n00b_tsi_t *tsi)
{
    n00b_dlog_thread1("begin thread deallocation.");
    int  page_sz = n00b_get_page_size();
    int  size    = n00b_round_up_to_given_power_of_2(page_sz,
                                                 sizeof(n00b_tsi_t));
    bool cancel  = n00b_thread_to_cancel == &tsi->self_data;

    if (tsi->system_thread) {
        n00b_end_system_io();
    }

    // If we're canceling, the unlocks will progress through their
    // atomic op, but will hit the GIL when
    n00b_release_locks_on_thread_exit(tsi);

    if (cancel) {
        atomic_store(&n00b_cancel_completed, 1);
        n00b_futex_wake(&n00b_cancel_completed, false);
    }

    // log this before we dealloc our ID actually.
    n00b_dlog_thread("Thread exited.");
    atomic_store(&n00b_global_thread_list[tsi->thread_id], NULL);

    // Don't give back TID 0, it's special.
    if (tsi->thread_id) {
        atomic_store(&n00b_next_thread_slot, tsi->thread_id);
    }
#if defined(N00B_MADV_ZERO)
    madvise(tsi, size, MADV_ZERO);
    mprotect(tsi, size, PROT_NONE);
#endif
    munmap(tsi, size);

    return NULL;
}

static void *
n00b_thread_launcher(void *arg)
{
    // The spawner reserves our tid to it can return it.
    n00b_tbundle_t *info = arg;
    n00b_tsi_t     *tsi  = _n00b_init_self_tsi(info->tid);
    void           *result;

    n00b_assert(tsi);
    tsi->self_data.tsi = tsi;
    n00b_assert(tsi->self_data.tsi);

    int32_t init = atomic_load(&tsi->self_lock);

    while (!(init | N00B_RUNNING)) {
        n00b_futex_wait(&tsi->self_lock, init, 0);
        init = atomic_load(&tsi->self_lock);
    }

    atomic_store(&tsi->self_lock, init & ~N00B_RUNNING);

    pthread_cleanup_push((void *)n00b_post_thread_cleanup, tsi);
    n00b_thread_checkin();
    result = (*info->true_cb)(info->true_arg);
    n00b_dlog_thread("Thread exiting implicitly (no exit call)");
    pthread_cleanup_pop(1);

    return result;
}

n00b_thread_t *
n00b_thread_spawn(void *(*r)(void *), void *arg)
{
#ifdef N00B_DEBUG_SHOW_SPAWN
    n00b_static_c_backtrace();
#endif
    // Certainly don't launch another thread.
    // Instead, exit the current thread.
    if (n00b_current_process_is_exiting()) {
        n00b_thread_exit(NULL);
    }

    // Reserve a TID before we start.
    int32_t tid = 0;

    do {
        tid = atomic_fetch_add(&n00b_next_thread_slot, 1) % HATRACK_THREADS_MAX;
    } while (atomic_read(&n00b_global_thread_list[tid]));

    n00b_tbundle_t *info = n00b_gc_alloc_mapped(n00b_tbundle_t,
                                                N00B_GC_SCAN_ALL);
    info->true_cb        = r;
    info->true_arg       = arg;
    info->tid            = tid;

    pthread_t pt;
    int       ret = pthread_create(&pt, NULL, n00b_thread_launcher, info);

    if (ret) {
        n00b_abort();
    }

    n00b_thread_start();
    while (!atomic_read(&n00b_global_thread_list[tid]))
        ;
    n00b_dlog_thread("Spawned new thread %d.", tid);

    n00b_thread_t *result = n00b_global_thread_list[tid];
    n00b_tsi_t    *tsi    = result->tsi;

    atomic_fetch_or(&tsi->self_lock, N00B_STARTING);
    n00b_futex_wake(&tsi->self_lock, true);

    return result;
}

n00b_thread_t *
n00b_thread_by_id(int32_t tid)
{
    return atomic_read(&n00b_global_thread_list[tid]);
}

void
n00b_thread_cancel(n00b_thread_t *t)
{
    n00b_tsi_t       *tsi         = t->tsi;
    n00b_lock_base_t *wait_target = tsi->lock_wait_target;
    n00b_futex_t     *futex       = NULL;

    // If we got canceled and we're marked as the 'system' thread, then
    // We ignore the cancelation.
    if (tsi->system_thread) {
        n00b_system_dispatcher->exit_loop = true;
        return;
    }

    N00B_DBG_CALL(n00b_stop_the_world);

    n00b_dlog_thread("Requested cancelation for thread: %d", tsi->thread_id);

    // Always set the self-lock unless
    atomic_fetch_or(&tsi->self_lock, N00B_THREAD_CANCEL);

    if (wait_target) {
        switch (__n00b_get_lock_type(wait_target)) {
        case N00B_NLT_MUTEX:
            futex = &((n00b_mutex_t *)wait_target)->futex;
            break;
        case N00B_NLT_RW:
            futex = &((n00b_rwlock_t *)wait_target)->futex;
            break;
        case N00B_NLT_CV:
            futex = &((n00b_condition_t *)wait_target)->mutex;
            break;
        default:
            n00b_unreachable();
        }
    }

    n00b_thread_to_cancel = t;

    if (futex) {
        // Other threads might be waiting here, so we let
        // them check against this value.
        atomic_fetch_or(futex, N00B_THREAD_CANCEL);
        n00b_futex_wake(futex, true);
    }

    // Now we wait until the cancelation is completed.
    while (!atomic_read(&n00b_cancel_completed)) {
        n00b_futex_wait(&n00b_cancel_completed, 0, 0);
    }

    if (futex) {
        atomic_fetch_and(futex, ~N00B_THREAD_CANCEL);
    }

    N00B_DBG_CALL(n00b_restart_the_world);
}
