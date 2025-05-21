#define N00B_USE_INTERNAL_API
#include "n00b.h"

#if N00B_DLOG_LOCK_LEVEL >= 1
static void
check_mutex_value(n00b_rwlock_t *lock)
{
    int32_t v = atomic_read(&lock->futex) & ~N00B_RW_W_LOCK;
    assert(v <= n00b_thread_run_count());
}
#else
#define check_mutex_value(x)
#endif

static n00b_lock_log_t *
get_read_lock_record(n00b_rwlock_t *lock, n00b_tsi_t *tsi)
{
    n00b_lock_log_t *log = tsi->read_locks;

    while (log != NULL) {
        if (log->obj == lock) {
            return log;
        }
        log = log->next_entry;
    }

    return NULL;
}

static void
add_read_record(n00b_rwlock_t *lock,
                n00b_tsi_t    *tsi,
                int            value,
                char          *__file,
                int            __line)
{
    n00b_lock_log_t *log;
    n00b_lock_log_t *prev;

    if (tsi->log_alloc_cache) {
        log                  = tsi->log_alloc_cache;
        tsi->log_alloc_cache = log->next_entry;

        if (tsi->log_alloc_cache) {
            tsi->log_alloc_cache->prev_entry = NULL;
        }

        log->next_entry = NULL;
    }
    else {
        log = n00b_gc_alloc_mapped(n00b_lock_log_t, N00B_GC_SCAN_ALL);
    }
    log->obj        = lock;
    log->thread_id  = 1; // Reused field for nesting level.
    prev            = tsi->read_locks;
    log->next_entry = prev;
    tsi->read_locks = log;

    if (prev) {
        prev->prev_entry = log;
    }

    n00b_rlock_accounting(lock, log, tsi, value, __file, __line);
}

// The W_LOCK bit doesn't prevent existing readers from reading, it
// prevents new readers from adding themselves. Therefore, writers who
// set the bit will have to block until the reader count drops to 0.

void
N00B_DBG_DECL(n00b_rw_init, n00b_rwlock_t *lock)
{
    N00B_DBG_CALL_NESTED(n00b_lock_init_accounting,
                         (void *)lock,
                         N00B_NLT_RW);
    n00b_futex_init(&lock->futex);
    if (!n00b_in_heap(lock)) {
        n00b_gc_register_root(lock,
                              sizeof(n00b_rwlock_t) / sizeof(void *));
    }
    assert(atomic_read(&lock->futex) <= (uint32_t)n00b_thread_run_count());
}

int
N00B_DBG_DECL(n00b_rw_write_lock, n00b_rwlock_t *lock)
{
    n00b_tsi_t *tsi = n00b_get_tsi_ptr();
    int32_t     tid = (int32_t)tsi->thread_id;
    uint32_t    value;

    n00b_core_lock_info_t info = atomic_read(&lock->data);

    check_mutex_value(lock);
    N00B_DBG_CALL(n00b_thread_suspend);
    // Writers first vie to be the sole writer, then they wait for
    // readers to drain before proceeding.
    value = atomic_fetch_or(&lock->futex, N00B_RW_W_LOCK);

    while (value & N00B_RW_W_LOCK) {
        if (info.owner == tid) {
            N00B_DBG_CALL(n00b_thread_resume);
            check_mutex_value(lock);
            return N00B_DBG_CALL_NESTED(n00b_lock_acquire_accounting,
                                        (void *)lock,
                                        tsi);
        }
        n00b_register_lock_wait(tsi, lock);
        n00b_futex_wait_for_value(&lock->futex, N00B_RW_UNLOCKED);
        value = atomic_fetch_or(&lock->futex, N00B_RW_W_LOCK);
        n00b_other_canceled(&lock->futex);
        n00b_wait_done(tsi);
    }

    // Now we need to wait for readers to clear out.  However, we need
    // to know whether or not WE are a reader; if we are, we're
    // waiting for the count to get to 1, not 0.

    n00b_register_lock_wait(tsi, lock);
    assert(lock);

    if (get_read_lock_record(lock, tsi)) {
        n00b_futex_wait_for_value(&lock->futex, N00B_RW_W_LOCK | 1);
    }
    else {
        n00b_futex_wait_for_value(&lock->futex, N00B_RW_W_LOCK);
    }

    n00b_barrier();
    n00b_wait_done(tsi);

    N00B_DBG_CALL(n00b_thread_resume);

    int result = N00B_DBG_CALL_NESTED(n00b_lock_acquire_accounting,
                                      (void *)lock,
                                      tsi);
    check_mutex_value(lock);
    return result;
}

void
N00B_DBG_DECL(n00b_rw_read_lock, n00b_rwlock_t *lock)
{
    // The reader first wants to see if it's already in the lock.  We
    // can cheaply see if there is nobody in the lock at all, or if
    // we are in the lock as the writer.
    //
    // It's a little less cheap for readers; we need to go through our
    // in-thread linked list.

    n00b_tsi_t           *tsi   = n00b_get_tsi_ptr();
    n00b_core_lock_info_t info  = atomic_read(&lock->data);
    uint32_t              value = 0;
    volatile uint32_t     desired;

    n00b_barrier();
    if (CAS(&lock->futex, &value, 1)) {
        check_mutex_value(lock);
#ifdef N00B_DEBUG
        add_read_record(lock, tsi, 1, __file, __line);
#else
        add_read_record(lock, tsi, 1, NULL, 0);
#endif
        check_mutex_value(lock);
        return;
    }

    if (info.owner == (int32_t)tsi->thread_id) {
        // If we already have the write lock, we keep track
        // of nesting as a writer.
        N00B_DBG_CALL_NESTED(n00b_lock_acquire_accounting,
                             (void *)lock,
                             tsi);
        check_mutex_value(lock);
        return;
    }

    n00b_barrier();
    n00b_lock_log_t *record = get_read_lock_record(lock, tsi);

    if (record) {
        // We reuse the thread_id field to capture reader nesting
        // level.
        record->thread_id++;
        n00b_rlock_accounting(lock,
                              record,
                              tsi,
                              -1,
                              __file,
                              __line);
        check_mutex_value(lock);
        return;
    }

    N00B_DBG_CALL(n00b_thread_suspend);
    value = atomic_read(&lock->futex);
    assert((value & ~N00B_RW_W_LOCK) <= (uint32_t)n00b_thread_run_count());
    do {
        if (n00b_other_canceled(&lock->futex)) {
            n00b_register_lock_wait(tsi, lock);
            n00b_futex_wait(&lock->futex, value, 0);
            n00b_wait_done(tsi);

            value = atomic_read(&lock->futex);
            continue;
        }

        if (value & N00B_RW_W_LOCK) {
            n00b_register_lock_wait(tsi, lock);
            n00b_futex_wait(&lock->futex, value, 0);
            n00b_wait_done(tsi);

            value = atomic_read(&lock->futex);
            continue;
        }

        desired = value + 1;

    } while (!CAS((volatile _Atomic(uint32_t) *)&lock->futex, &value, desired));
    check_mutex_value(lock);
    N00B_DBG_CALL(n00b_thread_resume);
    check_mutex_value(lock);
#ifdef N00B_DEBUG
    add_read_record(lock, tsi, desired, __file, __line);
#else
    add_read_record(lock, tsi, desired, NULL, 0);
#endif
    check_mutex_value(lock);
    n00b_dlog_lock3("Post rw-rlock, Top read record = %p; cache = %p",
                    tsi->read_locks,
                    tsi->log_alloc_cache);
}

// Return true if we drop something, whether we drop 'w' or 'r'.
// Meaning, if we downgrade from 'w' to 'r', this still returns false.
bool
N00B_DBG_DECL(n00b_rw_unlock, n00b_rwlock_t *lock)
{
    // This is the slightly more complicated operation. We may have
    // the read lock, we may have the write lock, and we may have
    // both.
    //
    // And, of course, we may be nested.
    //
    // Above, when we already hold the write lock, we treat read-locks
    // as write locks for purposes of nesting. That allows us to do
    // the following here:
    //
    // 1. If we're a nested write locker, just drop the nesting level.
    // 2. If we're a non-nested write locker, drop the write lock and
    //    notify, regardless of whether we have the read lock. Then return.
    // 3. If we're a nested-read locker, lower the nesting level
    //    and return.
    // 4. Non-nested read locker, set write count to 0 and notify.
    n00b_tsi_t           *tsi  = n00b_get_tsi_ptr();
    n00b_core_lock_info_t info = atomic_read(&lock->data);

    n00b_dlog_lock3("Pre rw-rulock of %p, Top read record = %p; cache = %p",
                    lock,
                    tsi->read_locks,
                    tsi->log_alloc_cache);

    if (info.owner == (int32_t)tsi->thread_id) {
        if (!N00B_DBG_CALL_NESTED(n00b_lock_release_accounting, (void *)lock)) {
            check_mutex_value(lock);
            return false;
        }
        atomic_fetch_and(&lock->futex, ~N00B_RW_W_LOCK);
        // For now, we just wake everyone; it would be better to wake
        // one at a time, though readers would have to keep track of
        // when they're woken, and wake again when done, so this is
        // easier.
        n00b_futex_wake(&lock->futex, true);
        check_mutex_value(lock);
        return true;
    }

    n00b_lock_log_t *log = get_read_lock_record(lock, tsi);

    if (!log) {
        // Should warn or possibly error for this.
        abort();
        return true;
    }
    if (--log->thread_id) { // thread_id slot is used for nesting level.
        check_mutex_value(lock);
        return false;       // Still a reader.
    }

    if (tsi->read_locks == log) {
        tsi->read_locks = log->next_entry;
        assert(!log->prev_entry);
    }
    else {
        log->prev_entry->next_entry = log->next_entry;
    }

    if (log->next_entry) {
        log->next_entry->prev_entry = log->prev_entry;
    }

    if (tsi->log_alloc_cache) {
        tsi->log_alloc_cache->prev_entry = log;
    }

    log->prev_entry      = NULL;
    log->next_entry      = tsi->log_alloc_cache;
    tsi->log_alloc_cache = log;

    N00B_DBG_CALL(n00b_thread_suspend);
    uint32_t value, desired;

    do {
        value = atomic_read(&lock->futex);

        if (n00b_other_canceled(&lock->futex)) {
            n00b_futex_wait(&lock->futex, value, 0);
            value = atomic_read(&lock->futex);
            continue;
        }

        desired = value - 1;
    } while (!CAS(&lock->futex, &value, desired));

    n00b_runlock_accounting(lock, log, tsi, desired, __file, __line);

    N00B_DBG_CALL(n00b_thread_resume);

    check_mutex_value(lock);

    return true;
}

static void
rwlock_constructor(n00b_rwlock_t *lock, va_list args)
{
    n00b_rw_lock_init(lock);
    char *name = va_arg(args, char *);

    if (!name) {
        name = "object rw lock";
    }

    lock->debug_name = name;
}

static n00b_string_t *
rwlock_to_string(n00b_rwlock_t *lock)
{
    return n00b_cstring(lock->debug_name);
}

const n00b_vtable_t n00b_rwlock_vtable = {
    .methods = {
        [N00B_BI_CONSTRUCTOR] = (n00b_vtable_entry)rwlock_constructor,
        [N00B_BI_TO_STRING]   = (n00b_vtable_entry)rwlock_to_string,
    },
};
