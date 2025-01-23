#define N00B_USE_INTERNAL_API
#include "n00b.h"

static pthread_once_t          tinit     = PTHREAD_ONCE_INIT;
static __thread n00b_thread_t *n00b_self = NULL;
static __thread n00b_thread_t  self_data;
static bool                    threads          = false;
static _Atomic int             next_thread_slot = 0;
static _Atomic int             live_threads     = 0;
__thread int                   n00b_global_thread_array_ix;
_Atomic(n00b_thread_t *)      *n00b_global_thread_list = NULL;

#if defined(__linux__)
void
n00b_thread_stack_region(n00b_thread_t *t)
{
    pthread_t self = pthread_self();

    pthread_attr_t attrs;

    pthread_getattr_np(self, &attrs);

    size_t size;

    pthread_attr_getstack(&attrs, (void **)&t->base, &size);

#ifdef N00B_USE_FRAME_INTRINSIC
    t->cur = __builtin_frame_address(0);
#else
    t->cur = t->base + (size / 8);
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

#ifdef N00B_USE_FRAME_INTRINSIC
    t->cur = __builtin_frame_address(0);
#else
    void *ptr;

    // For M1s only.
    // __asm volatile("mov  %0, sp" : "=r"(ptr) :);
    t->cur = &ptr + N00B_STACK_SLOP;
#endif
}
#else
#error "Unsupported platform."
#endif

n00b_thread_t *
n00b_thread_self(void)
{
    return n00b_self;
}

static void
n00b_do_setup_requiring_heap(void)
{
    n00b_push_heap(n00b_internal_heap);
    n00b_setup_lock_registry(); // lock_registry.c
    mmm_setthreadfns((void *)n00b_thread_register, NULL);

    n00b_global_thread_list = n00b_heap_alloc(n00b_internal_heap,
                                              sizeof(void *)
                                                  * HATRACK_THREADS_MAX,
                                              NULL);

    n00b_heap_register_root(n00b_internal_heap,
                            &n00b_global_thread_list,
                            sizeof(n00b_global_thread_list));
    threads = true;

    n00b_pop_heap();
}

// Reserve our space in the array we scan to iterate over live threads.
// This should ONLY ever be called from one of two places:

// 1. n00b_init() for the main thread.
// 2. n00b_thread_register()

void
n00b_thread_get_gta_slot(void)
{
    pthread_once(&tinit, n00b_do_setup_requiring_heap);

    n00b_heap_register_dynamic_root(n00b_internal_heap,
                                    n00b_self,
                                    sizeof(self_data) / 8);

    n00b_thread_t *expected;

    do {
        n00b_global_thread_array_ix = atomic_fetch_add(&next_thread_slot, 1);
        n00b_global_thread_array_ix %= HATRACK_THREADS_MAX;
        expected = NULL;
    } while (!CAS(&n00b_global_thread_list[n00b_global_thread_array_ix],
                  &expected,
                  n00b_self));
}

void
n00b_thread_bootstrap_initialization(void)
{
    n00b_self = &self_data;

    self_data.pthread_id = pthread_self();
    n00b_thread_stack_region(n00b_self);
    n00b_gts_start();
    atomic_fetch_add(&live_threads, 1);
}

n00b_thread_t *
n00b_thread_register(void)
{
    if (n00b_self) {
        return n00b_self;
    }

    n00b_thread_bootstrap_initialization();
    n00b_thread_get_gta_slot();

    return n00b_self;
}

int
n00b_thread_run_count(void)
{
    return atomic_read(&live_threads);
}

void
n00b_thread_unregister(void *unused)
{
    // After sleep, we can only use memory outside
    // the heap.
    n00b_thread_t *ti = n00b_thread_self();

    if (!ti) {
        return;
    }

    n00b_thread_unlock_all();
    n00b_heap_remove_root(n00b_internal_heap, &n00b_self);
    n00b_self = NULL;

    atomic_store(&n00b_global_thread_list[n00b_global_thread_array_ix],
                 NULL);
    mmm_thread_release(&ti->mmm_info);

    n00b_gts_quit();
    atomic_fetch_add(&live_threads, -1);
    n00b_self = NULL;
}

static void *
n00b_thread_launcher(void *arg)
{
    n00b_tbundle_t *info = arg;
    n00b_thread_register();
    void *result = (*info->true_cb)(info->true_arg);
    n00b_thread_exit(0);
    return result;
}

int
n00b_thread_spawn(void *(*r)(void *), void *arg)
{
    n00b_push_heap(n00b_internal_heap);

    n00b_tbundle_t *info = n00b_gc_alloc_mapped(n00b_tbundle_t,
                                                N00B_GC_SCAN_ALL);

    info->true_cb  = r;
    info->true_arg = arg;

    pthread_t pt;
    n00b_pop_heap();
    int result;

#ifdef N00B_DEBUG_SHOW_SPAWN
    n00b_static_c_backtrace();
#endif

    result = pthread_create(&pt, NULL, n00b_thread_launcher, info);
    return result;
}

int64_t
n00b_wait(n00b_notifier_t *n, int ms_timeout)
{
    uint64_t result;
    ssize_t  err;

    if (ms_timeout == 0) {
        ms_timeout = -1;
    }

    struct pollfd ctx = {
        .fd     = n->pipe[0],
        .events = POLLIN | POLLHUP | POLLERR,
    };

    N00B_DEBUG_HELD_LOCKS();
    n00b_gts_suspend();

    if (poll(&ctx, 1, ms_timeout) != 1) {
        n00b_gts_resume();
        return -1LL;
    }

    n00b_gts_resume();

    if (ctx.revents & POLLHUP) {
        return 0LL;
    }

    do {
        err = read(n->pipe[0], &result, sizeof(uint64_t));
    } while ((err < 1) && (errno == EAGAIN || errno == EINTR));

    if (err < 1) {
        n00b_raise_errno();
    }

    return result;
}
