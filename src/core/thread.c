#define N00B_USE_INTERNAL_API
#include "n00b.h"

#define SLOTS_PER_PAGE (n00b_page_bytes / sizeof(void *) - 1)

static n00b_lock_t           **n00b_lock_registry   = NULL;
static pthread_mutex_t         n00b_lock_reg_edit   = PTHREAD_MUTEX_INITIALIZER;
static int                     n00b_used_lock_slots = 0;
static pthread_once_t          tinit                = PTHREAD_ONCE_INIT;
static __thread n00b_thread_t *n00b_self            = NULL;
static bool                    threads              = false;
static bool                    exiting              = false;
static _Atomic int             next_thread_slot     = 0;
__thread mmm_thread_t         *n00b_mmm_thread_ref;
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
    t->cur = &ptr - N00B_STACK_SLOP;
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
migrate_registry(void)
{
    int n             = n00b_used_lock_slots / SLOTS_PER_PAGE;
    int l             = n * n00b_get_page_size();
    n00b_lock_t **new = mmap(NULL,
                             l + n00b_get_page_size(),
                             PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_ANON,
                             0,
                             0);
    memcpy(new, n00b_lock_registry, l);
    munmap(n00b_lock_registry, l);
    n00b_lock_registry = new;
}

void
n00b_lock_register(n00b_lock_t *l)
{
    pthread_mutex_lock(&n00b_lock_reg_edit);
    // The value is the lock type.
    if (n00b_lock_registry) {
        for (int i = 0; i < n00b_used_lock_slots; i++) {
            if (!n00b_lock_registry[i]) {
                n00b_lock_registry[i] = l;
                l->slot               = i;
                pthread_mutex_unlock(&n00b_lock_reg_edit);
                return;
            }
        }

        if (!(n00b_used_lock_slots % SLOTS_PER_PAGE)) {
            if (!n00b_used_lock_slots) {
                n00b_lock_registry = n00b_unprotected_mempage();
            }
            else {
                migrate_registry();
            }
        }

        n00b_lock_registry[n00b_used_lock_slots] = l;
        l->slot                                  = n00b_used_lock_slots;
        n00b_used_lock_slots++;
    }

    pthread_mutex_unlock(&n00b_lock_reg_edit);
}

void
n00b_lock_unregister(n00b_lock_t *l)
{
    pthread_mutex_lock(&n00b_lock_reg_edit);

    if (n00b_lock_registry) {
        n00b_lock_registry[l->slot] = NULL;
    }
    pthread_mutex_unlock(&n00b_lock_reg_edit);
}

void
n00b_thread_unlock_all(void)
{
    if (!n00b_lock_registry) {
        return;
    }

    n00b_thread_t *self = n00b_thread_self();

    pthread_mutex_lock(&n00b_lock_reg_edit);
    n00b_lock_t **p = n00b_lock_registry;
    n00b_lock_t  *l;

    for (int i = 0; i < n00b_used_lock_slots; i++) {
        l = *p++;
        if (l && l->thread == self) {
            pthread_mutex_unlock(&n00b_lock_reg_edit);
            n00b_lock_release_all(l);
            pthread_mutex_lock(&n00b_lock_reg_edit);
        }
    }
    pthread_mutex_unlock(&n00b_lock_reg_edit);
}

// in thread_coop.c
extern void n00b_init_thread_accounting(void);

static void
n00b_setup_threads(void)
{
    n00b_push_heap(n00b_thread_master_heap);
    n00b_lock_registry = n00b_gc_alloc_mapped(crown_t, N00B_GC_SCAN_ALL);

    n00b_heap_register_root(n00b_thread_master_heap, &n00b_lock_registry, 1);

    mmm_setthreadfns((void *)n00b_thread_register, NULL);
    n00b_init_thread_accounting();

    n00b_global_thread_list = n00b_heap_alloc(n00b_thread_master_heap,
                                              sizeof(void *)
                                                  * HATRACK_THREADS_MAX,
                                              NULL);

    n00b_heap_register_root(n00b_thread_master_heap,
                            &n00b_global_thread_list,
                            sizeof(n00b_global_thread_list));
    threads = true;

    n00b_pop_heap();
}

n00b_thread_t *
n00b_thread_register(void)
{
    pthread_once(&tinit, n00b_setup_threads);

    n00b_thread_t *ti = n00b_thread_self();
    n00b_thread_t *expected;

    if (ti) {
        return ti;
    }

    ti = n00b_heap_alloc(n00b_thread_master_heap, sizeof(n00b_thread_t), NULL);
    /*
    assert(!posix_memalign((void **)&ti,
                           N00B_FORCED_ALIGNMENT,
                           sizeof(n00b_thread_t)));

    bzero(ti, sizeof(n00b_thread_t));
    */

    n00b_self = ti;
    n00b_heap_register_dynamic_root(n00b_thread_master_heap, &n00b_self, 1);

    ti->pthread_id = pthread_self();

    do {
        n00b_global_thread_array_ix = atomic_fetch_add(&next_thread_slot, 1);
        n00b_global_thread_array_ix %= HATRACK_THREADS_MAX;
        expected = NULL;
    } while (!CAS(&n00b_global_thread_list[n00b_global_thread_array_ix],
                  &expected,
                  ti));

    n00b_thread_enter_run_state();

    return ti;
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

    n00b_thread_leave_run_state();

    atomic_store(&n00b_global_thread_list[n00b_global_thread_array_ix],
                 NULL);
    if (n00b_mmm_thread_ref) {
        mmm_thread_release(n00b_mmm_thread_ref);
    }
}

static void *
n00b_thread_launcher(void *arg)
{
    n00b_tbundle_t *info = arg;
    n00b_thread_register();
    void *result = (*info->true_cb)(info->true_arg);
    n00b_thread_unregister(NULL);
    pthread_exit(0);
    return result;
}

int
n00b_thread_spawn(void *(*r)(void *), void *arg)
{
    n00b_push_heap(n00b_thread_master_heap);

    n00b_tbundle_t *info = n00b_gc_alloc_mapped(n00b_tbundle_t,
                                                N00B_GC_SCAN_ALL);

    info->true_cb  = r;
    info->true_arg = arg;

    pthread_t pt;
    n00b_pop_heap();
    //    n00b_static_c_backtrace();
    return pthread_create(&pt, NULL, n00b_thread_launcher, info);
}

void
n00b_exit(int code)
{
    exiting = true;
    n00b_ioqueue_dont_block_callbacks();
    n00b_io_begin_shutdown();

    n00b_wait_for_io_shutdown();
    n00b_thread_unregister(NULL);
    exit(code);
}

void
n00b_thread_exit(void *result)
{
    n00b_ioqueue_dont_block_callbacks();
    n00b_thread_unregister(NULL);

    pthread_exit(result);
}

void
n00b_abort(void)
{
    exiting = true;
    n00b_ioqueue_dont_block_callbacks();
    n00b_io_begin_shutdown();
    n00b_wait_for_io_shutdown();
    n00b_thread_unregister(NULL);

    abort();
}

bool
n00b_current_process_is_exiting(void)
{
    return exiting;
}
