#define N00B_USE_INTERNAL_API
#include "n00b.h"

pthread_key_t            n00b_static_tsi_key;
_Atomic int32_t          n00b_next_thread_slot = 0;
_Atomic int              n00b_live_threads     = 0;
_Atomic(n00b_thread_t *) n00b_global_thread_list[HATRACK_THREADS_MAX];

// I don't feel good giving it a function pointer to a static inline fn
static void *
n00b_mmm_get_tsi(void)
{
    n00b_tsi_t *ptr = n00b_get_tsi_ptr();
    return &ptr->self_data.mmm_info;
}

// IMPORTANT: This needs to live in non-GC'd space, and we cannot do
// *anything* to impact global state until after we've installed our
// TSI pointer and done a check-in (which happens at the end of the
// launch prologue, right before the user's thread is called.
n00b_tsi_t *
_n00b_init_self_tsi(int32_t acquired_thread_slot)
{
    int         page_sz = getpagesize();
    int         size    = n00b_round_up_to_given_power_of_2(page_sz,
                                                 sizeof(n00b_tsi_t));
    n00b_tsi_t *tsi     = mmap(NULL,
                           size,
                           PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANON,
                           0,
                           0);

    pthread_setspecific(n00b_static_tsi_key, tsi);

    // Even though we fetch-and-add an index, we CAS here for a couple
    // of reasons:
    //
    // 1. to avoid race conditions with slow yielders. To keep the
    // actual array fixed size; we essentially search for an empty
    // slot.
    n00b_thread_t *desired = &tsi->self_data;
    n00b_thread_t *expected;

    tsi->self_data.pthread_id = pthread_self();
#if defined(__linux__)
    pthread_getattr_np(tsi->self_data.pthread_id, &tsi->attrs);
#endif

    // Didn't I stop using this?? TODO: Probably remove.
    tsi->thread_sleeping = true;

    n00b_thread_stack_region(&tsi->self_data);
    atomic_fetch_add(&n00b_live_threads, 1);

    if (acquired_thread_slot) {
        tsi->thread_id = acquired_thread_slot;
        atomic_store(&n00b_global_thread_list[tsi->thread_id], desired);
    }
    else {
        do {
            tsi->thread_id = atomic_fetch_add(&n00b_next_thread_slot, 1);
            tsi->thread_id %= HATRACK_THREADS_MAX;
            expected = NULL;
        } while (!CAS(&n00b_global_thread_list[tsi->thread_id],
                      &expected,
                      desired));
    }

#if defined(N00B_FLOG_DEBUG)
    char fname[1024];
    snprintf(fname, 1024, "/tmp/flog.%llu", (long long int)tsi->thread_id);
    tsi->flog_file = fopen(fname, "w");
#endif

    return tsi;
}

void
n00b_thread_cancel_other_threads(void)
{
    n00b_thread_t *self = n00b_thread_self();

    for (int i = 0; i < HATRACK_THREADS_MAX; i++) {
        n00b_thread_t *t = atomic_read(&n00b_global_thread_list[i]);
        if (!t || t == self) {
            continue;
        }

        n00b_thread_cancel(t);
        //        pthread_cancel(t->pthread_id);
    }
}

N00B_ONCE(n00b_threading_setup)
{
    pthread_key_create(&n00b_static_tsi_key, NULL);
    mmm_setthreadfns((void *)n00b_mmm_get_tsi, NULL);

    n00b_tsi_t    *tsi  = n00b_init_self_tsi();
    n00b_thread_t *self = n00b_thread_self();
    self->tsi           = tsi;
}
