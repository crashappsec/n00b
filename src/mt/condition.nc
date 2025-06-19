#define N00B_USE_INTERNAL_API
#include "n00b.h"

void
N00B_DBG_DECL(n00b_condition_init, n00b_condition_t *cv)
{
    N00B_DBG_CALL_NESTED(n00b_lock_init_accounting,
                         (void *)cv,
                         N00B_NLT_CV);

    n00b_futex_init(&cv->mutex);
    n00b_futex_init(&cv->wait_queue);
    n00b_futex_init(&cv->notify_done);
    cv->ovalue         = NULL;
    cv->predicate      = NULL;
    cv->cv_param       = NULL;
    cv->num_to_process = 0;
    cv->num_to_wake    = 0;

    if (!n00b_in_heap(cv)) {
        n00b_gc_register_root(&cv->cv_param, 2);
    }
}

void
n00b_condition_set_callback(n00b_condition_t           *cv,
                            n00b_condition_predicate_fn fn,
                            void                       *param)
{
    N00B_DBG_CALL(n00b_condition_lock, cv);
    cv->predicate = fn;
    cv->cv_param  = param;

    if (!n00b_in_heap(cv)) {
        n00b_gc_register_root(&cv->cv_param, 1);
    }
    N00B_DBG_CALL(n00b_condition_unlock, cv);
}

int
N00B_DBG_DECL(n00b_condition_lock, n00b_condition_t *cv)
{
    if (n00b_lock_already_owner((void *)cv)) {
        return 1;
    }

    N00B_DBG_CALL_NESTED(n00b_mutex_lock, (void *)cv);

    n00b_dlog_lock3("CV lock @%p acquired (val: %d)",
                    cv,
                    atomic_read(&cv->mutex));

    return 1;
}

extern bool
N00B_DBG_DECL(n00b_condition_unlock, n00b_condition_t *cv)
{
    if (!n00b_lock_already_owner((void *)cv)) {
        n00b_dlog_lock3("Failed CV release (not owned.)");
        return false;
    }

    N00B_DBG_CALL_NESTED(n00b_mutex_unlock, (void *)cv);

    n00b_dlog_lock3("CV lock @%p released (current: %d).",
                    cv,
                    atomic_load(&cv->mutex));

    return true;
}

static void *
N00B_DBG_DECL(base_wait,
              n00b_condition_t *cv,
              n00b_tsi_t       *tsi,
              int64_t           timeout,
              bool              wake_unlocked)

{
    void   *result;
    bool    want_to_wake;
    bool    waking = false;
    bool    timed;
    int64_t last_ts;
    int64_t cur_ts;

    if (timeout <= 0) {
        timeout = 0;
        timed   = false;
    }
    else {
        timed   = true;
        last_ts = n00b_ns_timestamp();
    }

    N00B_DBG_CALL_NESTED(n00b_condition_lock, cv);

    // We might be asked to only wait if the lock is held, and to
    // return immediately if not.
    if (!tsi->cv_info.current_cv || tsi->cv_info.current_cv != cv) {
        memset(&tsi->cv_info, 0, sizeof(n00b_condition_thread_state_t));
        tsi->cv_info.current_cv     = cv;
        tsi->cv_info.wait_predicate = N00B_CV_ANY;
    }

#if defined(N00B_DEBUG)
    tsi->lock_wait_file = __file;
    tsi->lock_wait_line = __line;
#endif

    int32_t waiters = atomic_fetch_add(&cv->wait_queue, 1);
    n00b_dlog_lock3("Beginning wait() on CV %p (waiter # %d)", cv, waiters + 1);

    N00B_DBG_CALL(n00b_thread_suspend);
    n00b_register_lock_wait(tsi, cv);

    n00b_dlog_lock3("Registered on CV %p; begin waiting", cv);
    N00B_DBG_CALL_NESTED(n00b_condition_unlock, cv);

    n00b_futex_wait(&cv->wait_queue, waiters + 1, timeout);
    waiters = atomic_read(&cv->wait_queue);

    do {
        while (!(waiters & N00B_CV_NOTIFY_IN_PROGRESS)) {
            if (timed) {
                cur_ts = n00b_ns_timestamp();
                timeout -= (cur_ts - last_ts);
                last_ts = cur_ts;

                if (timeout < 0) {
                    atomic_fetch_add(&cv->wait_queue, -1);
                    return (void *)~0ULL;
                }
            }
            n00b_futex_wait(&cv->wait_queue, waiters, timeout);
            waiters = atomic_read(&cv->wait_queue);
        }

        if (cv->predicate) {
            want_to_wake = (*cv->predicate)(cv->pvalue,
                                            tsi->cv_info.wait_predicate,
                                            cv->ovalue,
                                            cv->cv_param,
                                            tsi->cv_info.thread_param);
        }
        else {
            want_to_wake = (cv->pvalue == tsi->cv_info.wait_predicate
                            || tsi->cv_info.wait_predicate == N00B_CV_ANY);
        }

        if (want_to_wake) {
            // assert(cv->num_to_wake-- > 0);
            waking = true;
            n00b_dlog_lock2("CV %p: will wake.", cv);
            result = cv->ovalue;
        }
        else {
            n00b_dlog_lock3("CV %p: chose not to wake.", cv);
        }

        int32_t remaining = (cv->num_to_wake--)
                          & ~(N00B_CV_NOTIFY_IN_PROGRESS | N00B_GIL);

        // assert(remaining >= 0);

        if (remaining) {
            n00b_futex_wake(&cv->wait_queue, false);
            n00b_dlog_lock3("CV %p: Signal next waiter", cv);
        }
        else {
            atomic_store(&cv->notify_done, 1);
            n00b_futex_wake(&cv->notify_done, false);
            n00b_dlog_lock3("CV %p: Signal notify done", cv);
        }

        n00b_barrier();
    } while (!waking);

    n00b_dlog_lock3("CV %p: Wait for barrier signal.", cv);
    n00b_futex_wait_for_value(&cv->notify_done, 0);

    N00B_DBG_CALL_NESTED(n00b_condition_lock, cv);
    n00b_wait_done(tsi);
    N00B_DBG_CALL(n00b_thread_resume);

    int nw = atomic_fetch_add(&cv->wait_queue, -1);
    nw     = nw & ~(N00B_CV_NOTIFY_IN_PROGRESS | N00B_GIL);
    n00b_dlog_lock3("CV %p: Resuming due to notify. Still %d waiters.",
                    cv,
                    nw - 1);

    if (wake_unlocked) {
        N00B_DBG_CALL_NESTED(n00b_condition_unlock, cv);
    }

    return result;
}

static int32_t
N00B_DBG_DECL(internal_cv_notify,
              n00b_condition_t *cv,
              int64_t           pvalue,
              void             *ovalue,
              int32_t           max,
              bool              unlock)
{
    n00b_tsi_t *tsi = n00b_get_tsi_ptr();
    int32_t     result;

    if (!tsi) {
        tsi = n00b_init_self_tsi();
    }

    // We need the lock to notify. If we already have it, nbd.
    N00B_DBG_CALL_NESTED(n00b_condition_lock, cv);
    cv->pvalue         = pvalue;
    cv->ovalue         = ovalue;
    cv->num_to_wake    = max;
    result             = atomic_read(&cv->wait_queue);
    cv->num_to_process = result & ~N00B_GIL;
    // assert(cv->num_to_process < N00B_THREADS_MAX);
    // assert(cv->num_to_process >= 0);

    if (!cv->num_to_process) {
        if (unlock) {
            N00B_DBG_CALL_NESTED(n00b_condition_unlock, cv);
        }
        return 0;
    }

    atomic_fetch_or(&cv->wait_queue, N00B_CV_NOTIFY_IN_PROGRESS);
    atomic_store(&cv->notify_done, 0);
    n00b_dlog_lock3("CV %p: Notify waiters", cv);
    n00b_futex_wake(&cv->wait_queue, false);
    N00B_DBG_CALL_NESTED(n00b_condition_unlock, cv);
    N00B_DBG_CALL_NESTED(n00b_thread_suspend);
    n00b_register_lock_wait(tsi, cv);
    n00b_dlog_lock3("CV %p: Notifier waiting for %d waiters",
                    cv,
                    cv->num_to_process);

    n00b_futex_wait_for_value(&cv->notify_done, 1);

    n00b_dlog_lock3("CV %p: Notification thread awake", cv);
    n00b_wait_done(tsi);
    // Mac is giving me problems w/ atomic AND ops actually being
    // sequentially consistent.

    n00b_barrier();
    atomic_fetch_and(&cv->wait_queue, ~N00B_CV_NOTIFY_IN_PROGRESS);
    atomic_store(&cv->notify_done, 0);
    // assert(!(atomic_read(&cv->wait_queue) & N00B_CV_NOTIFY_IN_PROGRESS));

    N00B_DBG_CALL_NESTED(n00b_thread_resume);
    // Waiters are done with their process, and all queing up.
    // We calculate the result and auto-unlock, if desired.

    if (unlock) {
        n00b_dlog_lock3("CV %p: Auto-unlock", cv);
        N00B_DBG_CALL_NESTED(n00b_condition_unlock, cv);
    }
    n00b_mac_barrier();
    n00b_dlog_lock3("CV %p: Notification finished. Mutex is %d",
                    cv,
                    atomic_read(&cv->mutex));
    return result;
}

void *
#if defined(N00B_DEBUG)
_n00b_condition_wait(n00b_condition_t *cv, char *__file, int __line, ...)
#else
_n00b_condition_wait(n00b_condition_t *cv, ...)
#endif
{
    keywords
    {
        int64_t predicate                  = N00B_CV_ANY;
        int64_t timeout                    = 0;
        bool    auto_unlock                = false;
        void   *wake_parameter(wake_param) = NULL;
    }

    void       *result;
    n00b_tsi_t *tsi = n00b_get_tsi_ptr();

    if (!tsi) {
        tsi = n00b_init_self_tsi();
    }

    // These go in the TSI so we can report on them in thread debugging.
    // Will probably move this to all to debug-mode only.
    tsi->cv_info.current_cv     = cv;
    tsi->cv_info.wait_predicate = predicate;
    tsi->cv_info.thread_param   = wake_parameter;
#if defined(N00B_DEBUG)
    tsi->cv_info.wait_file = __file;
    tsi->cv_info.wait_line = __line;

    result = _base_wait(cv, tsi, timeout, auto_unlock, __file, __line);
#else
    result = _base_wait(cv, tsi, timeout, auto_unlock);
#endif

    return result;
}

int32_t
#if defined(N00B_DEBUG)
_n00b_condition_notify(n00b_condition_t *cv,
                       char             *__file,
                       int               __line,
                       ...)
#else
_n00b_condition_notify(n00b_condition_t *cv, ...)
#endif
{
    keywords
    {
        int64_t predicate           = 0;
        int64_t max                 = 1;
        void   *value               = NULL;
        bool    all                 = false;
        bool    unlock(auto_unlock) = false;
    }

    if (all || max <= 0) {
        max = 0x7fffffff;
    }

    return _internal_cv_notify(cv,
                               predicate,
                               value,
                               (int32_t)max,
                               unlock
#if defined(N00B_DEBUG)
                               ,
                               __file,
                               __line);
#else
    );
#endif
}

static void
condition_constructor(n00b_condition_t *cv, va_list args)
{
    n00b_condition_init(cv);
    char *name = va_arg(args, char *);

    if (!name) {
        name = "condition variable";
    }

    cv->debug_name = name;
}

static n00b_string_t *
condition_to_string(n00b_condition_t *cv)
{
    return n00b_cstring(cv->debug_name);
}

const n00b_vtable_t n00b_condition_vtable = {
    .methods = {
        [N00B_BI_CONSTRUCTOR] = (n00b_vtable_entry)condition_constructor,
        [N00B_BI_TO_STRING]   = (n00b_vtable_entry)condition_to_string,
    },
};
