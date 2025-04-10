#define N00B_USE_INTERNAL_API
#include "n00b.h"

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
    // I don't know why this needs to get set; the value added when launching
    // gets zeroed out at some point after tsi is supposedly initialized.
    t->tsi = n00b_get_tsi_ptr();
#endif
}
#else
#error "Unsupported platform."
#endif

static void
post_thread_cleanup(n00b_tsi_t *tsi)
{
    mmm_thread_release(&tsi->self_data.mmm_info);
}

static void *
n00b_thread_launcher(void *arg)
{
    n00b_tsi_t *tsi = n00b_init_self_tsi();
    void       *result;

    n00b_assert(tsi);
    n00b_gts_start();
    tsi->self_data.tsi = tsi;
    n00b_assert(tsi->self_data.tsi);

    pthread_cleanup_push((void *)post_thread_cleanup, n00b_get_tsi_ptr());
    n00b_tbundle_t *info = arg;
    result               = (*info->true_cb)(info->true_arg);
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

    n00b_push_heap(n00b_internal_heap);
    n00b_tbundle_t *info = n00b_gc_alloc_mapped(n00b_tbundle_t,
                                                N00B_GC_SCAN_ALL);
    n00b_pop_heap();

    info->true_cb  = r;
    info->true_arg = arg;

    pthread_t pt;
    int       ret = pthread_create(&pt, NULL, n00b_thread_launcher, info);

    if (ret) {
        n00b_abort();
    }

    return n00b_thread_find_by_pthread_id(pt);
}

// These things happen before we even bring up the memory manager, so
// be careful.
void
n00b_threading_setup(void)
{
    n00b_initialize_thread_specific_info(); // thread_tsi.c
    n00b_initialize_global_thread_info();   // thread_coop.c
}
