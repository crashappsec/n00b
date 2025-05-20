#define N00B_USE_INTERNAL_API
#include "n00b.h"

pthread_key_t            n00b_static_tsi_key;
_Atomic int32_t          n00b_next_thread_slot = 0;
_Atomic int              n00b_live_threads     = 0;
_Atomic(n00b_thread_t *) n00b_global_thread_list[HATRACK_THREADS_MAX];
static bool              threads_inited = false;

static void
n00b_register_thread_tsi(n00b_tsi_t *tsi, int len)
{
    if (!n00b_default_heap) {
        return;
    }

    n00b_heap_register_dynamic_root(n00b_default_heap, tsi, len);
    assert(atomic_read(&n00b_global_thread_list[tsi->thread_id]));
}

n00b_thread_t *
n00b_thread_find_by_pthread_id(pthread_t id)
{
    int bound = atomic_read(&n00b_next_thread_slot) % HATRACK_THREADS_MAX;
    int n     = bound;

    while (n--) {
        n00b_thread_t *t = atomic_read(&n00b_global_thread_list[n]);

        if (t && t->pthread_id == id) {
            return t;
        }
    }

    n = HATRACK_THREADS_MAX;

    while (n-- != bound) {
        n00b_thread_t *t = atomic_read(&n00b_global_thread_list[n]);

        if (t && t->pthread_id == id) {
            return t;
        }
    }

    // Thread exited before we got an answer.
    return NULL;
}

// I don't feel good giving it a function pointer to a static inline fn
static void *
n00b_mmm_get_tsi(void)
{
    n00b_tsi_t *ptr = n00b_get_tsi_ptr();
    return &ptr->self_data.mmm_info;
}

n00b_tsi_t *
n00b_init_self_tsi(void)
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
    n00b_register_thread_tsi(tsi, size / sizeof(void *));

    // Even though we fetch-and-add an index, we CAS here for a couple
    // of reasons:
    //
    // 1. to avoid race conditions with slow yielders. To keep the
    // actual array fixed size; we essentially search for an empty
    // slot.
    n00b_thread_t *desired = &tsi->self_data;
    n00b_thread_t *expected;

    tsi->self_data.pthread_id = pthread_self();
    tsi->thread_sleeping      = true;

    n00b_thread_stack_region(&tsi->self_data);
    atomic_fetch_add(&n00b_live_threads, 1);

    do {
        tsi->thread_id = atomic_fetch_add(&n00b_next_thread_slot, 1);
        tsi->thread_id %= HATRACK_THREADS_MAX;
        expected = NULL;
    } while (!CAS(&n00b_global_thread_list[tsi->thread_id],
                  &expected,
                  desired));

#if defined(N00B_FLOG_DEBUG)
    char fname[1024];
    snprintf(fname, 1024, "/tmp/flog.%llu", (long long int)tsi->thread_id);
    tsi->flog_file = fopen(fname, "w");
#endif

    return tsi;
}

void
n00b_finish_main_thread_initialization(void)
{
    if (!threads_inited) {
        n00b_gts_start();
        n00b_register_thread_tsi(n00b_thread_self()->tsi,
                                 n00b_round_up_to_given_power_of_2(
                                     getpagesize(),
                                     sizeof(n00b_tsi_t))
                                     / sizeof(void *));
        threads_inited = true;
    }
}

static void
n00b_tsi_cleanup(void *arg)
{
    // This code has all moved to post_thread_cleanup in thread.c:
    //
    // The thread-specific information not passed to this call on
    // Linux, as if the key is deleted before the handler gets called?
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
        pthread_cancel(t->pthread_id);
    }
}

static pthread_once_t tsi_initialized = PTHREAD_ONCE_INIT;

static void
n00b_global_tsi_setup(void)
{
    pthread_key_create(&n00b_static_tsi_key, n00b_tsi_cleanup);
    mmm_setthreadfns((void *)n00b_mmm_get_tsi, NULL);

    n00b_tsi_t *tsi     = n00b_init_self_tsi();
    tsi->system_thread  = true;
    n00b_thread_t *self = n00b_thread_self();
    self->tsi           = tsi;
}

void
n00b_initialize_thread_specific_info(void)
{
    pthread_once(&tsi_initialized, n00b_global_tsi_setup);
}
