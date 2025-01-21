#define N00B_USE_INTERNAL_API
#include "n00b.h"

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
n00b_lock_acquire_if_unlocked(n00b_lock_t *l)
{
    switch (pthread_mutex_trylock(&l->lock)) {
    case EBUSY:
        if (l->thread == n00b_thread_self()) {
            l->level++;
            //            printf("acquired by %p\n", l->thread);
            return true;
        }
        return false;
    case 0:
        l->thread = n00b_thread_self();
        n00b_lock_register(l);
        //        printf("acquired by %p\n", l->thread);
        return true;
    case EINVAL:
        abort();
    default:
        n00b_raise_errno();
        return false;
    }
}

void
n00b_lock_acquire_raw(n00b_lock_t *l)
{
    if (!n00b_lock_acquire_if_unlocked(l)) {
        /*        printf("lock %p: %p blocking (held by %p)\n",
                       l,
                       n00b_thread_self(),
                       l->thread);
        */
        pthread_mutex_lock(&l->lock);
        n00b_lock_register(l);
        /*        printf("lock %p: %p unblocked (acquiring.).\n",
                         l,
                         n00b_thread_self());
        */
        l->thread = n00b_thread_self();
    }
}

void
n00b_lock_acquire(n00b_lock_t *l)
{
    if (!n00b_lock_acquire_if_unlocked(l)) {
        /*
        printf("lock %p: %p blocking (held by %p)\n",
               l,
               n00b_thread_self(),
               l->thread);
        */

        n00b_thread_leave_run_state();
        pthread_mutex_lock(&l->lock);
        n00b_lock_register(l);
        /*
        printf("lock %p: %p unblocked (acquiring.).\n",
               l,
               n00b_thread_self());
        */
        n00b_thread_enter_run_state();
        l->thread = n00b_thread_self();
    }
}

void
n00b_lock_release(n00b_lock_t *l)
{
    if (l->level) {
        l->level--;
        return;
    }

    n00b_lock_unregister(l);
    pthread_mutex_unlock(&l->lock);

    //    printf("released by l->thread: %p.\n", l->thread);
}

void
n00b_lock_release_all(n00b_lock_t *l)
{
    l->level = 0;
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
    if (!wait) {
        int res;

        n00b_thread_leave_run_state();
        res = pthread_rwlock_rdlock(&l->lock);

        n00b_thread_enter_run_state();

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
    fcntl(result->pipe[1], F_SETNOSIGPIPE, 1);
    fcntl(result->pipe[1], F_SETFL, flags | O_NONBLOCK);

    return result;
}

const n00b_vtable_t n00b_lock_vtable = {
    .num_entries = N00B_BI_NUM_FUNCS,
    .methods     = {
        [N00B_BI_CONSTRUCTOR] = (n00b_vtable_entry)n00b_lock_constructor,
        [N00B_BI_GC_MAP]      = (n00b_vtable_entry)N00B_GC_SCAN_NONE,
    },
};

const n00b_vtable_t n00b_condition_vtable = {
    .num_entries = N00B_BI_NUM_FUNCS,
    .methods     = {
        [N00B_BI_CONSTRUCTOR] = (n00b_vtable_entry)n00b_condition_constructor,
        [N00B_BI_GC_MAP]      = (n00b_vtable_entry)N00B_GC_SCAN_NONE,
    },
};

int64_t
n00b_wait(n00b_notifier_t *n, int ms_timeout)
{
    uint64_t result;
    ssize_t  err;

    n00b_thread_leave_run_state();

    if (ms_timeout == 0) {
        ms_timeout = -1;
    }

    struct pollfd ctx = {
        .fd     = n->pipe[0],
        .events = POLLIN | POLLHUP | POLLERR,
    };

    if (poll(&ctx, 1, ms_timeout) != 1) {
        return -1LL;
    }

    if (ctx.revents & POLLHUP) {
        return 0LL;
    }

    do {
        err = read(n->pipe[0], &result, sizeof(uint64_t));
    } while ((err < 1) && (errno == EAGAIN || errno == EINTR));

    n00b_thread_enter_run_state();

    if (err < 1) {
        n00b_raise_errno();
    }

    return result;
}

// In locks.h
#ifdef N00B_LOCK_DEBUG
static pthread_once_t n00b_lock_debug_init = PTHREAD_ONCE_INIT;
#define n00b_lock_debug_init() \
    pthread_once(&n00b_lock_debug_init, n00b_lock_debug_setup)

extern void n00b_lock_debug_setup();
extern void n00b_debug_lock(void *x, char *kind);
extern void n00b_debug_unlock(void *x);

#else
#define n00b_lock_debug_init()
#define n00b_debug_lock(x, y)
#define n00b_debug_unlock(x)
#endif
