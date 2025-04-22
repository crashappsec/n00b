#define N00B_USE_INTERNAL_API
#define N00B_MAX_LOCK_LEVEL 8

#include "n00b.h"
// Generally, we always go for the try-lock before actually locking,
// because some pthreads implementations generally don't handle nested
// locking at all.

extern void            n00b_gts_suspend(void);
extern void            n00b_gts_resume(void);
static n00b_rw_lock_t *all_rw_locks   = NULL;
static pthread_mutex_t rw_setup_mutex = PTHREAD_MUTEX_INITIALIZER;

static inline void
show_base(n00b_generic_lock_t *lock, char *kind_name, n00b_thread_t *t)
{
    char *name = lock->debug_name;
    char *file = lock->alloc_info.file;
    int   line = lock->alloc_info.line;

    if (!name) {
        name = "anon";
    }
    if (!file) {
        file = "unknown";
    }

    n00b_lock_record_t *rec = &lock->lock_info[0];
    n00b_alloc_hdr     *h   = n00b_find_allocation_record(lock);

    printf("  - %s %s @%p (%s:%d) was first locked at: %s:%d\n",
           kind_name,
           name,
           lock,
           file,
           line,
           rec->file,
           rec->line);

    if (lock->thread != t) {
        printf("    WARNING: Lock %s %s @%p is *actually* held by %p, not %p!!\n",
               kind_name,
               name,
               lock,
               lock->thread,
               t);
    }

    if (h) {
        printf("    Lock is inside allocation %p (%s:%d)\n",
               h->data,
               h->alloc_file ? h->alloc_file : "no file",
               h->alloc_line);
    }

    if (lock->level > 1) {
        printf("    Subsequent (nested) locks that are active:\n");
        for (int i = 1; i < lock->level; i++) {
            rec = &lock->lock_info[i];
            printf("      %s:%d\n", rec->file, rec->line);
        }
    }
}

static inline void
show_condition(n00b_condition_t *c, n00b_thread_t *t)
{
    show_base((void *)c, "condition", t);
    if (c->last_notify.thread) {
        printf("  Last notify was to %p (%s:%d)\n",
               c->last_notify.thread,
               c->last_notify.file,
               c->last_notify.line);
    }
}

// This is meant to be run in gdb/lldb for now, or from asserts.
// One for the n00b debugger should stop the world and build a list.
void
n00b_debug_locks(n00b_thread_t *t)
{
    bool                 found_readers = false;
    n00b_tsi_t          *foreign_tsi   = t->tsi;
    n00b_generic_lock_t *lock          = foreign_tsi->locks;
    n00b_rw_lock_t      *l;

    if (foreign_tsi->lock_wait_location.file) {
        printf("Thread %p: WAITING FOR LOCK %p at %s:%d\n",
               t,
               foreign_tsi->lock_wait_target,
               foreign_tsi->lock_wait_location.file,
               foreign_tsi->lock_wait_location.line);
    }

    if (!lock) {
        printf("Thread %p: no write locks held.\n", t);
        goto try_readers;
    }
    // Rewind to report from the top.
    while (lock) {
        if (lock->walked) {
            printf("ERROR: LOCK CYCLE DETECTED at %p.\n", lock);
            break;
        }
        lock->walked = true;
        // This shouldn't be necessary, but I'm getting the above
        // error with this statement not executing if it isn't
        // explicitly here.
        if (!lock->prev_held) {
            break;
        }
        lock = lock->prev_held;
    }

    printf("Thread %p: acquired (non-reader) locks:\n", t);

    while (lock) {
        lock->walked = false;
        switch (lock->kind) {
        case N00B_LK_MUTEX:
            show_base(lock, "mutex", t);
            break;
        case N00B_LK_RWLOCK:
            show_base(lock, "rw lock (for write)", t);
            break;
        case N00B_LK_CONDITION:
            show_condition((void *)lock, t);
            break;
        }
        lock = lock->next_held;
        if (!lock || !lock->walked) {
            break;
        }
    }

try_readers:
    l = all_rw_locks;

    while (l) {
        int n = atomic_read(&l->num_readers);
        if (n) {
            found_readers = true;
            if (!l->info.alloc_info.file) {
                l->info.alloc_info.file = "unknown";
            }
            printf("RW lock @%p (%s:%d) has %d active reader(s): ",
                   l,
                   l->info.alloc_info.file,
                   l->info.alloc_info.line,
                   n);
            for (int i = 0; i < N00B_LOCK_MAX_READERS; i++) {
                if (l->readers[i].thread) {
                    printf("  - Thread %p (%s:%d) %d time(s)",
                           l->readers[i].thread,
                           l->readers[i].file,
                           l->readers[i].line,
                           l->readers[i].level);
                }
            }
        }
        l = l->next_rw_lock;
    }
    if (!found_readers) {
        printf("No RW locks have readers.\n");
    }
    printf("\n");
}

void
n00b_debug_all_locks(void)
{
    n00b_thread_t *prev = NULL;

    for (int i = 0; i < HATRACK_THREADS_MAX; i++) {
        n00b_thread_t *t = atomic_read(&n00b_global_thread_list[i]);
        if (!t) {
            continue;
        }
        printf("Locks for thread %p\n", t);
        n00b_assert(t != prev);
        n00b_debug_locks(t);
        prev = t;
    }
}

#define lock_assert(x, l)                               \
    if (!(x)) {                                         \
        n00b_generic_lock_t *g    = (void *)l;          \
        char                *name = g->debug_name;      \
        if (!name) {                                    \
            name = "anon";                              \
        }                                               \
        printf("When operating on %s (%s:%d @%p):\n",   \
               name,                                    \
               g->alloc_info.file,                      \
               g->alloc_info.line,                      \
               l);                                      \
        _n00b_show_assert_failure(#x,                   \
                                  (char *)__FUNCTION__, \
                                  (char *)__FILE__,     \
                                  __LINE__);            \
        n00b_debug_all_locks();                         \
        abort();                                        \
    }

static inline bool
no_locks(void)
{
    n00b_tsi_t *tsi = n00b_get_tsi_ptr();
    if (!tsi || !n00b_startup_complete || n00b_abort_signal) {
        return true;
    }

    return tsi->suspend_locking;
}

static inline void
n00b_generic_lock_init(n00b_generic_lock_t *lock,
                       char                *debug_name,
                       char                *alloc_file,
                       int                  alloc_line,
                       n00b_lock_kind       kind)
{
    if (lock->inited) {
        return;
    }

    memset(lock, 0, sizeof(n00b_generic_lock_t));
    lock->debug_name      = debug_name;
    lock->alloc_info.file = alloc_file;
    lock->alloc_info.line = alloc_line;
    lock->kind            = kind;
    lock->inited          = true;
}

static inline void
n00b_generic_on_lock_acquire(n00b_generic_lock_t *lock, char *file, int line)
{
    n00b_thread_t *self = n00b_thread_self();

    if (lock->thread && lock->thread != self) {
        fprintf(stderr,
                "Lock at %p improperly acquired @%s:%d.\n",
                lock,
                file,
                line);
        return;
        lock_assert((!lock->thread || lock->thread == self), lock);
    }

    int                 ix  = lock->level & (N00B_LOCK_DEBUG_RING - 1);
    n00b_lock_record_t *rec = &lock->lock_info[ix];

    if (lock->level++) {
        return;
    }

    lock_assert(!rec->file, lock);

    rec->file = file;
    rec->line = line;

    n00b_generic_lock_t *prev  = n00b_get_thread_locks();
    n00b_generic_lock_t *probe = prev;

    // For some reason, next_held isn't getting cleared in all
    // scenarios on linux (whereas it is on macOS). TODO.
    // lock_assert(!lock->next_held, lock);
    lock_assert(!lock->prev_held, lock);
    // lock_assert(!probe || !probe->next_held, lock);

    while (probe) {
        lock_assert(probe->thread == self, probe);
        lock_assert(probe != lock, lock);
        probe = probe->prev_held;
    }

    lock->prev_held = prev;
    lock->thread    = self;

    lock_assert(prev != lock, lock);
    if (prev) {
        prev->next_held = lock;
    }

    n00b_set_thread_locks(lock);
}

static inline bool
n00b_generic_before_lock_release(n00b_generic_lock_t *lock)
{
    lock_assert(lock->level >= 1, lock);
    lock_assert(lock->thread == n00b_thread_self(), lock);

    lock->level = lock->level - 1;
    if (lock->level > 0) {
        lock->lock_info[lock->level + 1].file = NULL;
        return false;
    }

    lock_assert(!lock->level, lock);
    n00b_generic_lock_t *ll = n00b_get_thread_locks();
    if (ll == lock) {
        n00b_generic_lock_t *prev = lock->prev_held;
        lock_assert(!lock->next_held, lock);

        if (prev) {
            prev->next_held = NULL;
        }

        n00b_set_thread_locks(prev);

        lock_assert(n00b_get_thread_locks() != lock, lock);
    }

    else {
        while (ll->prev_held) {
            if (ll->prev_held == lock) {
                lock_assert(lock->next_held == ll, lock);
                ll->prev_held = lock->prev_held;
                goto finish_up;
            }
            ll = ll->prev_held;
        }
        n00b_unreachable();
    }

finish_up:
    lock->prev_held         = NULL;
    lock->thread            = NULL;
    lock->lock_info[0].file = NULL;
    return true;
}

void
_n00b_lock_init(n00b_lock_t *l, char *debug_name, char *file, int line)
{
    n00b_generic_lock_init(&l->info, debug_name, file, line, N00B_LK_MUTEX);
    pthread_mutex_init(&l->lock, NULL);
}

bool
_n00b_lock_acquire_if_unlocked(n00b_lock_t *l, char *file, int line)
{
    if (no_locks()) {
        return true;
    }

    switch (pthread_mutex_trylock(&l->lock)) {
    case EBUSY:
        // We already own the lock.
        if (l->info.thread == n00b_thread_self()) {
            n00b_generic_on_lock_acquire(&l->info, file, line);
            return true;
        }
        return false;
    case 0:
        n00b_generic_on_lock_acquire(&l->info, file, line);
        return true;
    case EINVAL:
        n00b_abort();
    default:
        n00b_raise_errno();
        return false;
    }
}

// Will not try to suspend for stop-the-worlds.
void
_n00b_lock_acquire_raw(n00b_lock_t *l, char *file, int line)
{
    if (_n00b_lock_acquire_if_unlocked(l, file, line)) {
        return;
    }

    pthread_mutex_lock(&l->lock);
    n00b_generic_on_lock_acquire(&l->info, file, line);
}

void
_n00b_lock_acquire(n00b_lock_t *l, char *file, int line)
{
    n00b_tsi_t *tsi = n00b_get_tsi_ptr();
    if (_n00b_lock_acquire_if_unlocked(l, file, line)) {
        return;
    }

    n00b_gts_suspend();
    tsi->lock_wait_target        = l;
    tsi->lock_wait_location.file = file;
    tsi->lock_wait_location.line = line;
    pthread_mutex_lock(&l->lock);
    tsi->lock_wait_location.file = NULL;
    n00b_gts_resume();
    n00b_generic_on_lock_acquire(&l->info, file, line);
}

void
n00b_mutex_release(n00b_lock_t *l)
{
    if (no_locks()) {
        return;
    }

    if (n00b_generic_before_lock_release(&l->info)) {
        pthread_mutex_unlock(&l->lock);
    }
}

void
n00b_mutex_release_all(n00b_lock_t *l)
{
    if (no_locks()) {
        return;
    }

    lock_assert(l->info.thread == n00b_thread_self(), l);
    for (int i = 1; i < l->info.level; i++) {
        l->info.lock_info[i].file = NULL;
    }
    l->info.level = 1;
    pthread_mutex_unlock(&l->lock);
}

void
_n00b_rw_lock_init(n00b_rw_lock_t *l, char *debug_name, char *file, int line)
{
    n00b_generic_lock_init(&l->info, debug_name, file, line, N00B_LK_RWLOCK);
    pthread_mutex_lock(&rw_setup_mutex);
    if (!all_rw_locks) {
        n00b_gc_register_root(&all_rw_locks, 1);
    }
    l->next_rw_lock = all_rw_locks;
    all_rw_locks    = l;
    pthread_rwlock_init(&l->lock, NULL);
    pthread_mutex_unlock(&rw_setup_mutex);
}

static bool
on_first_rw_lock_r(n00b_rw_lock_t *l, n00b_thread_t *t, char *file, int line)
{
    // Try up to MAX_READERS times to get a slot.
    // If we don't, we give up.
    for (int i = 0; i < N00B_LOCK_MAX_READERS; i++) {
        int ix = atomic_fetch_add(&l->reader_id, 1)
               & (N00B_LOCK_MAX_READERS - 1);
        if (!l->readers[ix].thread) {
            l->readers[ix] = (n00b_rw_reader_record_t){
                .file   = file,
                .line   = line,
                .level  = 1,
                .thread = t,
            };

            atomic_fetch_add(&l->num_readers, 1);
            return true;
        }
    }
    return false;
}

bool
_n00b_rw_lock_try_read(n00b_rw_lock_t *l, char *file, int line)
{
    if (no_locks()) {
        return true;
    }

    n00b_thread_t *t = n00b_thread_self();
    if (l->info.thread == t) {
        l->info.level++;
        return true;
    }

    switch (pthread_rwlock_tryrdlock(&l->lock)) {
    case EDEADLK:
    case EBUSY:
        if (l->info.thread == t) {
            l->info.level++;
            return true;
        }
        if (l->info.thread) {
            return false;
        }
        for (int i = 0; i < N00B_LOCK_MAX_READERS; i++) {
            if (l->readers[i].thread == t) {
                l->readers[i].level++;
                return true;
            }
        }
        return false;
    case 0:
        if (on_first_rw_lock_r(l, t, file, line)) {
            return true;
        }
        // Ran out of read slots.
        pthread_rwlock_unlock(&l->lock);
        return false;

    default:
        return false;
    }
}

void
_n00b_rw_lock_acquire_for_read(n00b_rw_lock_t *l, char *file, int line)
{
    if (_n00b_rw_lock_try_read(l, file, line)) {
        return;
    }

    n00b_gts_suspend();
    int err = pthread_rwlock_rdlock(&l->lock);
    n00b_gts_resume();

    if (err) {
        n00b_raise_errno();
    }

    n00b_thread_t *t = n00b_thread_self();

    for (int i = 0; i < N00B_LOCK_MAX_READERS; i++) {
        if (l->readers[i].thread == t) {
            l->readers[i].level++;
            return;
        }
    }
    if (!on_first_rw_lock_r(l, t, file, line)) {
        // Ran out of read slots.
        pthread_rwlock_unlock(&l->lock);
        N00B_CRAISE("Too many concurrent readers to track.");
    }
}

bool
_n00b_rw_lock_try_write(n00b_rw_lock_t *l, char *file, int line)
{
    if (no_locks()) {
        return true;
    }

    switch (pthread_rwlock_trywrlock(&l->lock)) {
    case EDEADLK:
    case EBUSY:
        if (l->info.thread == (void *)(int64_t)pthread_self()) {
            n00b_generic_on_lock_acquire(&l->info, file, line);
            return true;
        }
        return false;
    case 0:
        n00b_generic_on_lock_acquire(&l->info, file, line);
        return true;
    default:
        return false;
    }
}

void
_n00b_rw_lock_acquire_for_write(n00b_rw_lock_t *l, char *file, int line)
{
    if (_n00b_rw_lock_try_write(l, file, line)) {
        return;
    }

    n00b_gts_suspend();
    pthread_rwlock_wrlock(&l->lock);
    n00b_gts_resume();
    n00b_generic_on_lock_acquire(&l->info, file, line);
}

void
_n00b_rw_lock_release(n00b_rw_lock_t *l, bool full)
{
    if (no_locks()) {
        return;
    }

    n00b_thread_t *t = n00b_thread_self();

    // If it's got a writer, we MUST be that thread, or we shouldn't
    // be calling release.
    if (l->info.thread) {
        if (full) {
            l->info.level = 1;
        }
        if (n00b_generic_before_lock_release(&l->info)) {
            pthread_rwlock_unlock(&l->lock);
        }
        return;
    }

    for (int i = 0; i < N00B_LOCK_MAX_READERS; i++) {
        if (l->readers[i].thread == t) {
            lock_assert(l->readers[i].level >= 1, l);
            // If we're not doing a full release, only do the release
            // if the nesting level is 0.
            if (!full && --l->readers[i].level) {
                return;
            }
            l->readers[i].thread = NULL;
            atomic_fetch_add(&l->num_readers, -1);
            pthread_rwlock_unlock(&l->lock);
            return;
        }
    }
    n00b_static_c_backtrace();
    N00B_CRAISE("Called release() on a rw lock you didn't hold");
}

void
_n00b_condition_init(n00b_condition_t *c, char *name, char *file, int line)
{
    n00b_generic_lock_init(&c->mutex.info, name, file, line, N00B_LK_CONDITION);
    pthread_mutex_init(&c->mutex.lock, NULL);
    pthread_cond_init(&c->cv, NULL);
}

void
n00b_condition_pre_wait(n00b_condition_t *c)
{
    n00b_tsi_t *tsi = n00b_get_tsi_ptr();

    if (c->mutex.info.thread != &tsi->self_data) {
        N00B_CRAISE("Must hold lock before calling wait()");
    }
    if (c->mutex.info.next_held || c->mutex.info.prev_held) {
        N00B_CRAISE("Can't wait until all other locks are released.");
    }

    tsi->saved_lock_level = c->mutex.info.level;
    lock_assert(c->mutex.info.level < N00B_MAX_LOCK_LEVEL, c);
    for (int i = 0; i < tsi->saved_lock_level; i++) {
        tsi->saved_lock_records[i] = c->mutex.info.lock_info[i];
    }

    c->mutex.info.level = 1;
    n00b_generic_before_lock_release(&c->mutex.info);
}

void
n00b_condition_post_wait(n00b_condition_t *c, char *file, int line)
{
    lock_assert(!c->mutex.info.thread, c);
    n00b_generic_on_lock_acquire(&c->mutex.info, file, line);
    n00b_tsi_t *tsi     = n00b_get_tsi_ptr();
    c->mutex.info.level = tsi->saved_lock_level;
    for (int i = 0; i < tsi->saved_lock_level; i++) {
        c->mutex.info.lock_info[i] = tsi->saved_lock_records[i];
    }
}

void
_n00b_condition_notify_one(n00b_condition_t *c, void *aux, char *f, int l)
{
    n00b_thread_t *t = n00b_thread_self();
    if (c->mutex.info.thread != t) {
        N00B_CRAISE("Must hold lock before calling notify()");
    }

    c->aux = aux;
    if (c->cb) {
        (*c->cb)(c);
    }
    c->last_notify.file   = f;
    c->last_notify.line   = l;
    c->last_notify.thread = n00b_thread_self();
    pthread_cond_signal(&c->cv);
}

void
_n00b_condition_notify_all(n00b_condition_t *c, char *f, int l)
{
    n00b_thread_t *t = n00b_thread_self();
    if (c->mutex.info.thread != t) {
        N00B_CRAISE("Must hold lock before calling notify_all()");
    }

    if (c->cb) {
        (*c->cb)(c);
    }
    c->last_notify.file   = f;
    c->last_notify.line   = l;
    c->last_notify.thread = n00b_thread_self();
    pthread_cond_broadcast(&c->cv);
}

bool
_n00b_condition_timed_wait(n00b_condition_t *c,
                           n00b_duration_t  *d,
                           char             *f,
                           int               l)
{
    if (no_locks()) {
        return true;
    }

    n00b_gts_suspend();
    n00b_condition_pre_wait(c);
    int result = pthread_cond_timedwait(&((c)->cv),
                                        (&(c)->mutex.lock),
                                        d);
    n00b_gts_resume();
    if (result && result != ETIMEDOUT) {
        errno = result;
        n00b_raise_errno();
    }
    //     lock_assert(!result || result == ETIMEDOUT, c);

    n00b_condition_post_wait(c, f, l);

    if (!result) {
        return false;
    }
    return true;
}

void
n00b_lock_release(void *l)
{
    switch (((n00b_generic_lock_t *)l)->kind) {
    case N00B_LK_MUTEX:
    case N00B_LK_CONDITION:
        n00b_mutex_release(l);
        return;
    case N00B_LK_RWLOCK:
        n00b_rw_lock_release(l);
        return;
    }
}

void
n00b_lock_release_all(void *l)
{
    switch (((n00b_generic_lock_t *)l)->kind) {
    case N00B_LK_MUTEX:
    case N00B_LK_CONDITION:
        n00b_mutex_release_all(l);
        return;
    case N00B_LK_RWLOCK:
        n00b_rw_lock_release_all(((n00b_rw_lock_t *)l));
        return;
    }
}

static void
n00b_lock_constructor(n00b_lock_t *lock, va_list args)
{
    n00b_generic_lock_init(&lock->info, NULL, NULL, 0, N00B_LK_MUTEX);
}

static void
n00b_condition_constructor(n00b_condition_t *c, va_list args)
{
    _n00b_condition_init(c, NULL, NULL, 0);
    c->cb = va_arg(args, void *);
}

n00b_string_t *
n00b_lock_to_string(n00b_generic_lock_t *g)
{
    // Avoid lock recursion.
    char kind[3] = {0, 0, 0};

    switch (g->kind) {
    case N00B_LK_MUTEX:
        kind[0] = 'm';
        break;
    case N00B_LK_RWLOCK:
        kind[0] = 'r';
        kind[1] = 'w';
        break;
    default:
        kind[0] = 'c';
        break;
    }

    if (!g->debug_name) {
        g->debug_name = "anon";
    }

    int kl = strlen(kind);
    int dl = strlen(g->debug_name);
    int n  = kl + dl + 40;

    char  buf[n];
    char *p = buf;
    memcpy(p, kind, kl);
    p += kl;
    *p++ = ' ';
    memcpy(p, g->debug_name, dl);
    p += dl;
    *p++      = ' ';
    *p++      = '@';
    *(p + 16) = ' ';
    *(p + 17) = 0;
    snprintf(p, 16, "%p", g);

    return n00b_cstring(buf);
}

const n00b_vtable_t n00b_lock_vtable = {
    .methods = {
        [N00B_BI_CONSTRUCTOR] = (n00b_vtable_entry)n00b_lock_constructor,
        [N00B_BI_TO_STRING]   = (n00b_vtable_entry)n00b_lock_to_string,
    },
};

const n00b_vtable_t n00b_condition_vtable = {
    .methods = {
        [N00B_BI_CONSTRUCTOR] = (n00b_vtable_entry)n00b_condition_constructor,
        [N00B_BI_TO_STRING]   = (n00b_vtable_entry)n00b_lock_to_string,
    },
};
