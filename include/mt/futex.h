#include <stdint.h>
#include <errno.h>

extern void n00b_thread_checkin(void);

typedef _Atomic uint32_t n00b_futex_t;

#define N00B_THREAD_CANCEL 0x80000000U
#define N00B_US_PER_SEC    1000000
#define N00B_NS_PER_US     1000
#define N00B_NS_PER_SEC    1000000000ULL
#define N00B_CANCEL_EXIT   ((void *)-55LL)

#if defined(__linux__)
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

#define n00b_mac_barrier()
static inline int
n00b_futex_wait_timespec(n00b_futex_t    *futex,
                         uint32_t         v32,
                         struct timespec *tptr)
{
    int err = syscall(SYS_futex,
                      futex,
                      FUTEX_WAIT_PRIVATE,
                      v32,
                      tptr,
                      NULL,
                      0);
    if (err == -1) {
        return errno;
    }

    return 0;
}

static inline int
n00b_futex_wake(n00b_futex_t *futex, bool all)
{
    uint32_t n = all ? INT_MAX : 1;

    return syscall(SYS_futex,
                   futex,
                   FUTEX_WAKE_PRIVATE,
                   n,
                   NULL,
                   NULL,
                   0);
}

static inline bool
n00b_futex_should_continue(int err)
{
    return !err || err == EAGAIN;
}

#elif defined(__APPLE__)
extern int __ulock_wait2(uint32_t, void *, uint64_t, uint64_t, uint64_t);
extern int __ulock_wake(uint32_t, void *, uint64_t);

#define N00B_LOCK_COMPARE_AND_WAIT          1
#define N00B_LOCK_UNFAIR_LOCK               2
#define N00B_LOCK_COMPARE_AND_WAIT_SHARED   3
#define N00B_LOCK_UNFAIR_LOCK64_SHARED      4
#define N00B_LOCK_COMPARE_AND_WAIT64        5
#define N00B_LOCK_COMPARE_AND_WAIT64_SHARED 6
#define N00B_LOCK_WAKE_ALL                  0x00000100
#define N00B_LOCK_WAKE_THREAD               0x00000200
#define N00B_LOCK_WAKE_ALLOW_NON_OWNER      0x00000400

#define n00b_mac_barrier() n00b_barrier()

static inline int
n00b_futex_wait_timespec(n00b_futex_t    *futex,
                         uint32_t         v32,
                         struct timespec *tout)
{
    return __ulock_wait2(N00B_LOCK_COMPARE_AND_WAIT,
                         &futex,
                         (uint64_t)v32,
                         tout ? tout->tv_nsec : 0,
                         0);
}

static inline int
n00b_futex_wake(n00b_futex_t *futex, bool all)
{
    return __ulock_wake(all ? N00B_LOCK_WAKE_ALL : N00B_LOCK_WAKE_THREAD,
                        &futex,
                        0ULL);
}

static inline bool
n00b_futex_should_continue(int err)
{
    return !err || err == -EINTR || err == -EFAULT;
}

#else
#error "Unsupported platform."
#endif

static inline void
n00b_futex_init(n00b_futex_t *futex)
{
    atomic_store_explicit(futex, 0, memory_order_release);
}

static inline int
n00b_futex_wait(n00b_futex_t *futex,
                uint32_t      v32,
                uint64_t      nsec)
{
    struct timespec tout   = {.tv_sec = 0, .tv_nsec = nsec};
    int             result = n00b_futex_wait_timespec(futex, v32, &tout);

    n00b_thread_checkin();

    return result;
}

static inline int64_t n00b_ns_timestamp(void);
static inline bool    n00b_other_canceled(n00b_futex_t *f);

static inline void
n00b_futex_wait_for_value(n00b_futex_t *futex, uint32_t v32)
{
    uint32_t cur = atomic_read(futex);

    while (cur != v32) {
        cur = atomic_read(futex);
        // If some other thread is canceled, the check doesn't matter
        // anyway.
        n00b_other_canceled(futex);
    }
}

static inline bool
n00b_futex_timed_wait_for_value(n00b_futex_t *futex,
                                uint32_t      v32,
                                int64_t       timeout)
{
    uint32_t cur       = atomic_read(futex);
    int64_t  start     = n00b_ns_timestamp();
    int64_t  remaining = timeout;
    int64_t  now;

    while (true) {
        if (n00b_futex_wait(futex, cur, remaining) == ETIMEDOUT) {
            return false; // Got timeout
        }
        cur = atomic_read(futex);
        // If some other thread is canceled, the check doesn't matter
        // anyway.
        n00b_other_canceled(futex);
        if (cur == v32) {
            return true;
        }
        now = n00b_ns_timestamp();
        remaining -= (now - start); // Subtract time elapsed.

        if (remaining < 0) {
            return false;
        }
        start = now;
    }
}

static inline void
n00b_futex_wait_on_mask(n00b_futex_t *futex, uint32_t mask)
{
    uint32_t cur = atomic_read(futex);
    while (!(cur & mask)) {
        n00b_futex_wait(futex, cur, 0);
        cur = atomic_read(futex);
        if (n00b_other_canceled(futex)) {
            continue;
        }
        if (cur & N00B_THREAD_CANCEL) {
            n00b_thread_exit(N00B_CANCEL_EXIT);
        }
    }
}
