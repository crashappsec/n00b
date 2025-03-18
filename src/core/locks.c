#define N00B_USE_INTERNAL_API
#include "n00b.h"

#ifdef N00B_DEBUG_LOCKS
_Atomic bool __n00b_lock_debug = false;
#endif

void
n00b_raw_lock_init(n00b_lock_t *l)
{
    pthread_mutex_init(&l->lock, NULL);
    l->level = 0;
}
void
n00b_raw_condition_init(n00b_condition_t *c)
{
    pthread_mutex_init(&(c->lock.lock), NULL);
    pthread_cond_init(&(c->cv), NULL);
}

bool
_n00b_lock_acquire_if_unlocked(n00b_lock_t *l, char *file, int line)
{
    if (!n00b_startup_complete) {
        return true;
    }

    n00b_assert(n00b_thread_self());

    switch (pthread_mutex_trylock(&l->lock)) {
    case EBUSY:
        if (l->thread == n00b_thread_self()) {
            add_lock_record(l, file, line);
            l->level++;
#ifdef N00B_DEBUG_LOCKS
            if (__n00b_lock_debug) {
                printf("lock %p: acquired by %p @%s:%d\n",
                       l,
                       l->thread,
                       file,
                       line);
            }
#endif
            return true;
        }
        return false;
    case 0:
        l->thread = n00b_thread_self();
        add_lock_record(l, file, line);
        l->level = 0;

        n00b_lock_register(l);
#ifdef N00B_DEBUG_LOCKS
        if (__n00b_lock_debug) {
            printf("lock %p: acquired by %p @%s:%d\n",
                   l,
                   l->thread,
                   file,
                   line);
        }
#endif
        return true;
    case EINVAL:
        abort();
    default:
        n00b_raise_errno();
        return false;
    }
}

void
_n00b_lock_acquire_raw(n00b_lock_t *l, char *file, int line)
{
    if (!_n00b_lock_acquire_if_unlocked(l, file, line)) {
#ifdef N00B_DEBUG_LOCKS
        if (__n00b_lock_debug) {
            printf("lock %p: %p blocking (held by %p) @%s:%d\n",
                   l,
                   n00b_thread_self(),
                   l->thread,
                   file,
                   line);
        }

#endif

        pthread_mutex_lock(&l->lock);
        n00b_lock_register(l);
        add_lock_record(l, file, line);
        n00b_assert(!l->level);
#ifdef N00B_DEBUG_LOCKS
        if (__n00b_lock_debug) {
            printf("lock %p: %p unblocked (acquiring.) (thread %p) @%s:%d\n",
                   l,
                   n00b_thread_self(),
                   l->thread,
                   file,
                   line);
        }
#endif
        l->thread = n00b_thread_self();
    }
}

void
_n00b_lock_acquire(n00b_lock_t *l, char *file, int line)
{
    if (!_n00b_lock_acquire_if_unlocked(l, file, line)) {
#ifdef N00B_DEBUG_LOCKS
        if (__n00b_lock_debug) {
            printf("lock %p: %p blocking (held by %p) @%s:%d\n",
                   l,
                   n00b_thread_self(),
                   l->thread,
                   file,
                   line);
        }
#endif

        n00b_gts_suspend();
        pthread_mutex_lock(&l->lock);
        n00b_gts_resume();

        n00b_lock_register(l);
        add_lock_record(l, file, line);
        n00b_assert(!l->level);
#ifdef N00B_DEBUG_LOCKS
        if (__n00b_lock_debug) {
            printf("lock %p: %p unblocked @%s:%d\n",
                   l,
                   n00b_thread_self(),
                   file,
                   line);
        }
#endif
        l->thread = n00b_thread_self();
    }
}

void
n00b_lock_release(n00b_lock_t *l)
{
    if (!n00b_startup_complete) {
        return;
    }

    if (l->level) {
        l->level--;
        return;
    }

#ifdef N00B_DEBUG_LOCKS
    if (__n00b_lock_debug) {
        printf("%p: released by l->thread: %p.\n",
               l,
               n00b_thread_self());
    }
#endif

    l->thread = NULL;
    clear_lock_records(l);
    n00b_lock_unregister(l);
    pthread_mutex_unlock(&l->lock);
}

void
n00b_lock_release_all(n00b_lock_t *l)
{
    if (!n00b_startup_complete) {
        return;
    }

#ifdef N00B_DEBUG_LOCKS
    if (__n00b_lock_debug) {
        printf("%p: released by l->thread: %p.\n",
               l,
               n00b_thread_self());
    }
#endif

    l->level  = 0;
    l->thread = NULL;
    clear_lock_records(l);
    n00b_assert(!l->locks);
    n00b_lock_unregister(l);
    pthread_mutex_unlock(&l->lock);
}

static void
n00b_lock_constructor(n00b_lock_t *lock, va_list args)
{
    n00b_raw_lock_init(lock);
}

static void
n00b_condition_constructor(n00b_condition_t *c, va_list args)
{
    n00b_raw_condition_init(c);
    c->cb = va_arg(args, void *);
}

bool
n00b_rw_lock_acquire_for_read(n00b_rw_lock_t *l, bool wait)
{
    if (!n00b_startup_complete) {
        return true;
    }

    if (!wait) {
        int res;

        n00b_gts_suspend();
        res = pthread_rwlock_rdlock(&l->lock);
        n00b_gts_resume();

        if (res) {
            n00b_raise_errno();
        }

        return true;
    }

    switch (pthread_rwlock_tryrdlock(&l->lock)) {
    case EDEADLK:
    case EBUSY:
        if (l->thread == n00b_thread_self()) {
            l->level++;
            return true;
        }
        return false;
    case 0:
        return true;
    default:
        return false;
    }
}

n00b_notifier_t *
n00b_new_notifier(void)
{
    n00b_notifier_t *result = n00b_gc_alloc_mapped(n00b_notifier_t,
                                                   N00B_GC_SCAN_NONE);

    pipe(result->pipe);
    int flags = fcntl(result->pipe[1], F_GETFL);

#if !defined(__linux__)
    fcntl(result->pipe[1], F_SETNOSIGPIPE, 1);
#endif    
    fcntl(result->pipe[1], F_SETFL, flags | O_NONBLOCK);

    return result;
}

const n00b_vtable_t n00b_lock_vtable = {
    .methods = {
        [N00B_BI_CONSTRUCTOR] = (n00b_vtable_entry)n00b_lock_constructor,
        [N00B_BI_GC_MAP]      = (n00b_vtable_entry)N00B_GC_SCAN_NONE,
    },
};

const n00b_vtable_t n00b_condition_vtable = {
    .methods = {
        [N00B_BI_CONSTRUCTOR] = (n00b_vtable_entry)n00b_condition_constructor,
        [N00B_BI_GC_MAP]      = (n00b_vtable_entry)N00B_GC_SCAN_NONE,
    },
};
