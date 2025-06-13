#define N00B_USE_INTERNAL_API
#include "n00b.h"

#define MUTEX_UNLOCKED 0
#define MUTEX_LOCKED   1

// This mutex avoids most unnecessary calls into the kernel for
// unlocks, by keeping a counter of waiting threads. This is
// conservative, in that we still might perform spurious wakes, but we
// won't fail to call wake if there might be someone in the lock
// (i.e., no missed notifications).
//
void
N00B_DBG_DECL(n00b_mutex_init, n00b_mutex_t *lock)
{
    N00B_DBG_CALL_NESTED(n00b_lock_init_accounting,
                         (void *)lock,
                         N00B_NLT_MUTEX);
    n00b_futex_init(&lock->futex);
}

static void
mutex_constructor(n00b_mutex_t *mutex, va_list args)
{
    n00b_mutex_init(mutex);
    char *name = va_arg(args, char *);

    if (!name) {
        name = "object mutex";
    }

    mutex->debug_name = name;

#if defined(N00B_DEBUG)
    assert(mutex->allocation);
#endif
}

static n00b_string_t *
mutex_to_string(n00b_mutex_t *mutex)
{
    return n00b_cstring(mutex->debug_name);
}

static inline bool
N00B_DBG_DECL(spin_phase, n00b_mutex_t *mutex)
{
    n00b_tsi_t *tsi = n00b_get_tsi_ptr();

    if (!tsi) {
        tsi = n00b_init_self_tsi();
    }

    int32_t               tid  = (int32_t)tsi->thread_id;
    n00b_core_lock_info_t info = atomic_read(&mutex->data);

    // First, check to see if we're the owner.
    n00b_mac_barrier();
    if (info.owner == tid) {
        N00B_DBG_CALL_NESTED(n00b_lock_acquire_accounting,
                             (void *)mutex,
                             tsi);
        return true;
    }

    for (int i = 0; i < N00B_SPIN_LIMIT; i++) {
        if (!atomic_fetch_or(&mutex->futex, 1)) {
            N00B_DBG_CALL_NESTED(n00b_lock_acquire_accounting,
                                 (void *)mutex,
                                 tsi);
            return true;
        }
    }

    return false;
}

#if defined(N00B_DEBUG)
static inline void
ensure_ownership(n00b_mutex_t *mutex)
{
    n00b_mac_barrier();

    n00b_tsi_t           *tsi  = n00b_get_tsi_ptr();
    n00b_core_lock_info_t info = atomic_read(&mutex->data);

    assert(info.owner == tsi->thread_id);
}
#else
#define ensure_ownership(x)
#endif

int
N00B_DBG_DECL(n00b_mutex_lock, n00b_mutex_t *mutex)
{
    // First, busy-wait a bit.
    if (N00B_DBG_CALL_NESTED(spin_phase, mutex)) {
        ensure_ownership(mutex);
        return 0;
    }

    // Now, actually register w/ the OS to wait.
    n00b_tsi_t *tsi = n00b_get_tsi_ptr();

    atomic_fetch_add(&mutex->should_wake, 1);
    N00B_DBG_CALL_NESTED(n00b_thread_suspend);
    n00b_register_lock_wait(tsi, mutex);

    do {
        n00b_futex_wait_for_value(&mutex->futex, 0);
    } while (atomic_fetch_or(&mutex->futex, 1));

    atomic_fetch_add(&mutex->should_wake, -1);
    n00b_wait_done(tsi);
    n00b_mac_barrier();
    N00B_DBG_CALL_NESTED(n00b_lock_acquire_accounting, (void *)mutex, tsi);
    N00B_DBG_CALL_NESTED(n00b_thread_resume);

    ensure_ownership(mutex);

    return 0;
}

bool
N00B_DBG_DECL(n00b_mutex_try_lock, n00b_mutex_t *mutex, int usec)
{
    // First, busy-wait a bit.
    if (N00B_DBG_CALL_NESTED(spin_phase, mutex)) {
        ensure_ownership(mutex);
        return true;
    }

    n00b_tsi_t *tsi = n00b_get_tsi_ptr();
    atomic_fetch_add(&mutex->should_wake, 1);
    N00B_DBG_CALL_NESTED(n00b_thread_suspend);
    n00b_register_lock_wait(tsi, mutex);

    do {
        n00b_futex_timed_wait_for_value(&mutex->futex, 0, usec);
    } while (atomic_fetch_or(&mutex->futex, 1));

    atomic_fetch_add(&mutex->should_wake, -1);
    n00b_wait_done(tsi);
    N00B_DBG_CALL_NESTED(n00b_thread_resume);

    n00b_mac_barrier();

    N00B_DBG_CALL_NESTED(n00b_lock_acquire_accounting, (void *)mutex, tsi);
    ensure_ownership(mutex);

    return true;
}

// Return true if we unlocked, or false if nested.
bool
N00B_DBG_DECL(n00b_mutex_unlock, n00b_mutex_t *mutex)
{
    if (!N00B_DBG_CALL_NESTED(n00b_lock_release_accounting,
                              (void *)mutex)) {
        return false;
    }

    atomic_store(&mutex->futex, 0);
    assert(!atomic_read(&mutex->futex));
    n00b_dlog_lock2("Mutex lock @%p released (current: %d).",
                    mutex,
                    atomic_load(&mutex->futex));

    if (atomic_read(&mutex->should_wake)) {
        // Let the OS decide who gets the next crack at it to avoid
        // contention; only release one waiter.
        n00b_futex_wake(&mutex->futex, false);
    }

    return true;
}

const n00b_vtable_t n00b_mutex_vtable = {
    .methods = {
        [N00B_BI_CONSTRUCTOR] = (n00b_vtable_entry)mutex_constructor,
        [N00B_BI_TO_STRING]   = (n00b_vtable_entry)mutex_to_string,
    },
};
