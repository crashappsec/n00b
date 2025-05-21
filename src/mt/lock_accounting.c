#define N00B_USE_INTERNAL_API
#include "n00b.h"

void
N00B_DBG_DECL(n00b_lock_init_accounting, n00b_lock_base_t *lock, int type)
{
    n00b_core_lock_info_t info = {
        .owner    = N00B_NO_OWNER,
        .type     = type,
        .nesting  = 0,
        .reserved = 0,
    };

    atomic_store(&lock->data, info);
    atomic_store(&lock->next_thread_lock, NULL);
    atomic_store(&lock->prev_thread_lock, NULL);
    // Static locks may hold pointers to dynamic locks
    // when we build our linked list.
    if (!n00b_in_heap(lock)) {
        n00b_gc_register_root(&lock->next_thread_lock, N00B_PTR_WORDS);
    }

#if defined(N00B_DEBUG)
    lock->inited        = true;
    lock->creation_file = __file;
    lock->creation_line = __line;
    lock->allocation    = n00b_find_allocation_record(lock);
#endif
}

#if defined(N00B_DEBUG) && N00B_DLOG_LOCK_LEVEL >= 3
static inline char *
lock_kind(n00b_core_lock_info_t *info)
{
    switch (info->type) {
    case N00B_NLT_MUTEX:
        return "mutex";
    case N00B_NLT_RW:
        return "rw";
    case N00B_NLT_CV:
        return "condition";
    default:
        return "(error)";
    }
}

static inline char *
get_name(n00b_lock_base_t *lock)
{
    if (lock->debug_name) {
        return lock->debug_name;
    }

    return "(no name)";
}
#endif

int
N00B_DBG_DECL(n00b_lock_acquire_accounting,
              n00b_lock_base_t *lock,
              n00b_tsi_t       *tsi)
{
    int32_t               tid  = (int32_t)tsi->thread_id;
    n00b_core_lock_info_t info = atomic_read(&lock->data);

#if defined(N00B_DEBUG)
    if (!lock->inited) {
        if (lock->allocation) {
            fprintf(stderr,
                    "%s:%d: Fatal: Lock allocated at %s:%d "
                    "was not initialized before use.\n",
                    __file,
                    __line,
                    lock->allocation->alloc_file,
                    lock->allocation->alloc_line);
        }
        else {
            fprintf(stderr,
                    "%s:%d: Fatal: Lock at address %p "
                    "was not initialized before use.\n",
                    __file,
                    __line,
                    lock);
        }
        abort();
    }
#endif

    if (info.owner == tid) {
#if defined(N00B_DEBUG)
        n00b_lock_log_t *log = lock->logs;
        for (uint16_t i = 0; i < info.nesting; i++) {
            assert(log);
            log = log->next_entry;
        }
        n00b_barrier();
#endif
        ++info.nesting;
    }
    else {
#if defined(N00B_DEBUG)
        assert(info.owner == N00B_NO_OWNER);
#endif
        info.owner                 = tid;
        info.nesting               = 1;
        n00b_lock_base_t *top_held = tsi->exclusive_locks;

        if (top_held) {
            atomic_store(&top_held->prev_thread_lock, lock);
        }

        atomic_store(&lock->next_thread_lock, tsi->exclusive_locks);
        tsi->exclusive_locks = lock;
    }

#if defined(N00B_DEBUG)
    n00b_lock_log_t *log = n00b_gc_alloc_mapped(n00b_lock_log_t,
                                                N00B_GC_SCAN_ALL);
    log->file            = __file;
    log->line            = __line;
    log->lock_op         = true;
    log->thread_id       = tid;
    log->next_entry      = lock->logs;
    lock->logs           = log;

    atomic_store(&lock->data, info);
    n00b_dlog_lock3("%s:%d:@%p LOCK %s %s\n(tid: %x; init: %s:%d; level: %d)",
                    __file,
                    __line,
                    lock,
                    lock_kind(&info),
                    get_name((n00b_lock_base_t *)lock),
                    info.owner,
                    lock->creation_file,
                    lock->creation_line,
                    info.nesting);

#endif

    return 0;
}

#if defined(N00B_DEBUG)
void
n00b_rlock_accounting(n00b_rwlock_t   *lock,
                      n00b_lock_log_t *record,
                      n00b_tsi_t      *tsi,
                      int              value,
                      char            *__file,
                      int              __line)
{
#if N00B_DLOG_LOCK_LEVEL >= 3
    n00b_core_lock_info_t info = atomic_read(&lock->data);

    n00b_dlog_lock3(
        "%s:%d:@%p: R_LOCK %s \n(owner: %x; init: %s:%d; "
        "level: %d; value: %x)",
        __file,
        __line,
        get_name((n00b_lock_base_t *)lock),
        lock,
        info.owner,
        lock->creation_file,
        lock->creation_line,
        record->thread_id,
        value);

    n00b_rwdebug_t *log = n00b_gc_alloc_mapped(n00b_rwdebug_t,
                                               N00B_GC_SCAN_ALL);

    log->prev        = lock->last_entry;
    lock->last_entry = log;
    if (!lock->first_entry) {
        lock->first_entry = log;
    }
    if (log->prev) {
        log->prev->next = log;
    }
    log->file      = __file;
    log->line      = __line;
    log->lock_op   = true;
    log->thread_id = tsi->thread_id;
    log->nest      = record->thread_id;
#endif
}

void
n00b_runlock_accounting(n00b_rwlock_t   *lock,
                        n00b_lock_log_t *record,
                        n00b_tsi_t      *tsi,
                        int              value,
                        char            *__file,
                        int              __line)
{
#if N00B_DLOG_LOCK_LEVEL >= 3
    n00b_core_lock_info_t info = atomic_read(&lock->data);

    n00b_dlog_lock3(
        "%s:%d:t@%p: R_UNLOCK %s \n(owner: %x; init: %s:%d; "
        "level: %d; value: %x)",
        __file,
        __line,
        lock,
        get_name((n00b_lock_base_t *)lock),
        info.owner,
        lock->creation_file,
        lock->creation_line,
        record->thread_id,
        value);

    n00b_rwdebug_t *log = n00b_gc_alloc_mapped(n00b_rwdebug_t,
                                               N00B_GC_SCAN_ALL);
    log->prev           = lock->last_entry;
    lock->last_entry    = log;
    if (!lock->first_entry) {
        lock->first_entry = log;
    }
    if (log->prev) {
        log->prev->next = log;
    }
    log->file      = __file;
    log->line      = __line;
    log->lock_op   = true;
    log->thread_id = tsi->thread_id;
    log->nest      = record->thread_id;
    //    char *s        = n00b_backtrace_cstring();
    //    log->trace     = s;
#endif
}
#endif

// Return 'true' if we should unlock.
bool
N00B_DBG_DECL(n00b_lock_release_accounting, n00b_lock_base_t *lock)
{
    bool                  unlock = false;
    n00b_tsi_t           *tsi    = n00b_get_tsi_ptr();
    n00b_core_lock_info_t info   = atomic_read(&lock->data);
    int32_t               tid    = (int32_t)tsi->thread_id;
    n00b_lock_base_t     *prev   = NULL;
    n00b_lock_base_t     *next   = NULL;

    if (info.type != N00B_NLT_CV) {
        if (info.owner == N00B_NO_OWNER) {
            fprintf(stderr,
                    "Fatal: Attempt to unlock %p, which is unlocked.\n",
                    lock);
            n00b_static_c_backtrace();
            abort();
        }

        if (info.owner != tid) {
            switch (info.type) {
            case N00B_NLT_CV:
                return false;
            default:
                fprintf(stderr,
                        "Fatal: tid %d tried to unlock %p (owned by %d)\n",
                        tid,
                        lock,
                        info.owner);
                n00b_static_c_backtrace();
                abort();
            }
        }
#if defined(N00B_DEBUG)
        assert(info.nesting > 0);
        assert(lock->inited);
#endif
    }
    else {
        if (info.owner != tid) {
            return false;
        }
    }

    if (!--info.nesting) {
        unlock     = true;
        info.owner = N00B_NO_OWNER;

        prev = atomic_read(&lock->prev_thread_lock);
        next = atomic_read(&lock->next_thread_lock);

        if (prev) {
            /*
            assert(prev != lock);
            assert(prev != next);
            assert(next != lock);
            */
            if (prev != next) {
                atomic_store(&prev->next_thread_lock, next);
            }
        }

        if (next) {
#if defined(N00B_DEBUG)
            // n00b_lock_base_t *l = atomic_read(&next->prev_thread_lock);
//            assert(l == lock);
#endif
            if (prev != next) {
                atomic_store(&next->prev_thread_lock, prev);
            }
        }

        atomic_store(&lock->prev_thread_lock, NULL);
        atomic_store(&lock->next_thread_lock, NULL);

        if (tsi->exclusive_locks == lock) {
            // assert(!prev);
            tsi->exclusive_locks = next;
        }

        if (tsi->exclusive_locks == lock) {
            tsi->exclusive_locks = NULL;
        }

#if defined(N00B_DEBUG)
        lock->allocation = NULL;
        lock->logs       = NULL;
    }
    else {
        n00b_lock_log_t *log = n00b_gc_alloc_mapped(n00b_lock_log_t,
                                                    N00B_GC_SCAN_ALL);
        log->obj             = lock;
        log->file            = __file;
        log->line            = __line;
        log->lock_op         = false;
        log->thread_id       = tid;
        log->next_entry      = lock->logs;
        lock->logs           = log;
    }
#else
    }
#endif
    n00b_dlog_lock3(
        "%s:%d:@%p: UNLOCK %s %s\n(tid: %x; init: %s:%d; level: %d)",
        __file,
        __line,
        lock,
        lock_kind(&info),
        get_name((n00b_lock_base_t *)lock),
        info.owner,
        lock->creation_file,
        lock->creation_line,
        info.nesting);

    atomic_store(&lock->data, info);

    return unlock;
}

#ifdef N00B_DEBUG

void
n00b_wait_log(n00b_tsi_t       *tsi,
              n00b_lock_base_t *lock,
              char             *f,
              int               line,
              bool              wake)
{
#if N00B_DLOG_LOCK_LEVEL >= 3
    n00b_core_lock_info_t info = atomic_read(&lock->data);

    n00b_dlog_lock3(
        "%s:%d:@%p: %s %s %s\n(tid: %x; init: %s:%d; level: %d)",
        f,
        line,
        lock,
        wake ? "WAKE" : "WAIT",
        lock_kind(&info),
        get_name(lock),
        info.owner,
        lock->creation_file,
        lock->creation_line,
        info.nesting);

    if (!wake) {
        char *s              = n00b_backtrace_cstring();
        tsi->lock_wait_trace = s;
        n00b_dlog_lock2("%x wait trace:\n%s\n",
                        tsi->thread_id,
                        s);
    }
#endif
}

static inline void
show_lock_logs(n00b_lock_log_t *log, FILE *f)
{
    while (log) {
        fprintf(f,
                "      %s: %s:%d (tid:%x)\n",
                log->lock_op ? "lock" : "unlock",
                log->file,
                log->line,
                (int)log->thread_id);
        log = log->next_entry;
    }
}

static inline void
show_lock(n00b_lock_base_t *l, FILE *f)
{
    n00b_core_lock_info_t info = atomic_read(&l->data);

    fprintf(f,
            "    %s (owner 0x%x, init @%s:%d, ",
            l->debug_name,
            info.owner,
            l->creation_file,
            l->creation_line);
    if (l->allocation) {
        fprintf(f,
                " alloc @%s:%d):",
                l->allocation->alloc_file,
                l->allocation->alloc_line);
    }
    else {
        fprintf(f, " static):");
    }
}

static inline void
n00b_show_write_locks(n00b_tsi_t *tsi, FILE *f)
{
    n00b_lock_base_t *l = tsi->exclusive_locks;
    if (!l) {
        fprintf(f,
                "  No write locks for thread %p.\n",
                (void *)tsi->thread_id);
        return;
    }

    fprintf(f, "  Write Locks for thread %p:\n", (void *)tsi->thread_id);

    while (l) {
        show_lock(l, f);
        fprintf(f, " (@%p)\n", l);
        show_lock_logs(l->logs, f);
        l = l->next_thread_lock;
    }
}

static inline void
show_read_trail(n00b_rwlock_t *lock, FILE *f)
{
    n00b_rwdebug_t *log = lock->first_entry;

    while (log) {
        fprintf(f,
                "      -- %c @%s:%d by %x (%d)\n",
                log->lock_op ? 'l' : 'u',
                log->file,
                log->line,
                log->thread_id,
                log->nest);
        if (log->trace) {
            fprintf(f, "*****Backtrace****:\n%s\n", log->trace);
        }
        log = log->next;
    }
    fprintf(f,
            "    ** Current mutex value: %x\n",
            atomic_read(&lock->futex));
}

static inline void
n00b_show_read_locks(n00b_tsi_t *tsi, FILE *f)
{
    n00b_lock_log_t *log = tsi->read_locks;

    if (!log) {
        fprintf(f,
                "  No read locks for thread %llx.\n",
                (long long int)tsi->thread_id);
        return;
    }

    fprintf(f, "  Read Locks for thread %llx:\n", (long long int)tsi->thread_id);

    while (log) {
        show_lock(log->obj, f);
        fprintf(f, " (@%p; ", log->obj);
        // for rw lock records in the TSI, the thread_id field is overloaded
        // to indicate the nesting level. It's a TODO to just have a
        // different type, which also should keep its own linked list
        // of all the locations it's read-locked from.
        fprintf(f, " %d times)\n", log->thread_id);
        show_read_trail((void *)log->obj, f);
        log = log->next_entry;
    }
}

static inline void
n00b_show_wait_status(n00b_tsi_t *tsi, FILE *f)
{
    n00b_core_lock_info_t info;

    if (tsi->lock_wait_target) {
        fprintf(f,
                "  BLOCKED waiting on %s:%d for lock (@%p)\n  ",
                tsi->lock_wait_file,
                tsi->lock_wait_line,
                tsi->lock_wait_target);
        show_lock(tsi->lock_wait_target, f);
        fprintf(f, "\n");
        fprintf(f, "%s\n", tsi->lock_wait_trace);
        info = atomic_read(&tsi->lock_wait_target->data);
        if (info.type == N00B_NLT_RW) {
            fprintf(f, "    Read trail:\n");
            show_read_trail((void *)tsi->lock_wait_target, f);
        }
    }
}

void
n00b_debug_thread_locks(n00b_thread_t *t, FILE *f)
{
    if (!t) {
        t = n00b_thread_self();
    }

    n00b_show_write_locks(t->tsi, f);
    n00b_show_read_locks(t->tsi, f);
    n00b_show_wait_status(t->tsi, f);
    fflush(f);
}

void
n00b_debug_locks_stream(FILE *stream)
{
    n00b_thread_t *prev = NULL;

    for (int i = 0; i < HATRACK_THREADS_MAX; i++) {
        n00b_thread_t *t = atomic_read(&n00b_global_thread_list[i]);
        n00b_tsi_t    *tsi;

        if (!t) {
            continue;
        }
        n00b_assert(t != prev);

        tsi = t->tsi;
        if (!tsi->exclusive_locks && !tsi->read_locks
            && !tsi->lock_wait_target) {
            fprintf(stream, "Thread %d: unlocked.\n\n", i);
        }
        else {
            fprintf(stream, "Thread %d: ", i);
            n00b_debug_thread_locks(t, stream);
            fprintf(stream, "\n");
        }
        prev = t;
    }
}

void
n00b_debug_all_locks(char *fname)
{
    FILE *stream = stderr;

    if (fname) {
        FILE *f = fopen(fname, "w");

        if (f) {
            stream = f;
        }
    }
    n00b_debug_locks_stream(stream);
    fclose(stream);
}

void
_n00b_register_lock_wait(n00b_tsi_t *tsi, void *lock, char *f, int line)
{
    assert(lock);
    tsi->lock_wait_target = lock;
    tsi->lock_wait_file   = f;
    tsi->lock_wait_line   = line;

    n00b_wait_log(tsi, lock, f, line, false);
}

void
_n00b_wait_done(n00b_tsi_t *tsi, char *__file, int __line)
{
    n00b_lock_base_t *lock = tsi->lock_wait_target;

    if (lock) {
        n00b_wait_log(tsi, lock, __file, __line, true);
    }
    tsi->lock_wait_target = NULL;
}
#endif
