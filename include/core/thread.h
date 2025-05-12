#pragma once
#include "n00b.h"

typedef struct n00b_thread_t n00b_thread_t;

struct n00b_thread_t {
    // This reserves space for MMM's use; the n00b_thread_t * is
    // transparently cast to it, so this must be first.
    mmm_thread_t mmm_info;
    void        *tsi;
    void        *base; // base pointer of stack
    void        *cur;  // current stack 'top'.
    pthread_t    pthread_id;
};

// Global thread cooperation is going to happen w/o worrying about
// contention deadlock. Instead, we are going to use CAS and sleeps to
// ensure lock freedom.
//
// There will be one global thread-state array, but there will be
// three global states:
//
// 1. GO. All threads may proceed.
//
// 2. YIELD! The world is stopping; all threads must stop when they
//    check in. Threads must check in whenever they might pause (e.g.,
//    sleep), whenever they wake up, whenever they are allocating
//    dynamic memory, and every N VM instructions.
//
//    Once all threads are stopped, the requestor gets to run, and
//    then must RESTART the world, moving the global state back to the
//    'RUN' state.
//
// 3. STOP. If threads come on line during this state for any reason
// (i.e., waking up), they must immediately wait for restart.
//
// We don't actually need to distinguish between YIELD and STOP since
// the thread who starts the stop-the-world waits around for all other
// threads to yield, and at that point nobody would come along and get
// confused anyway. But for now, I made it 3 states to make it easier to
// understand internal state when debugging.
//
// To avoid ABA issues, we swap in a random 62 bit value (instead of a
// thread ID). The bottom two bits of that value consist of the global
// state.
//
// When we CAS, we only CAS a pointer to a thread-local data structure.
typedef enum : int8_t {
    n00b_gts_exited      = -1,
    n00b_gts_not_started = 0,
    n00b_gts_go          = 1,
    n00b_gts_yield       = 2,
    n00b_gts_stop        = 3,
} n00b_global_thread_state_t;

// This has an upper bound of 2^16 concurrent threads. But that allows us
// to fit all our state in 128 bits
typedef struct {
    n00b_thread_t             *leader;
    uint32_t                   epoch;
    int16_t                    running;
    n00b_global_thread_state_t state;
} n00b_global_thread_info_t;

// Use the first one on thread first start-up only.
extern void _n00b_gts_start(char *, int);
extern void _n00b_gts_suspend(char *, int);
extern void _n00b_gts_resume(char *, int);
extern void _n00b_gts_might_stop(char *, int);
extern void _n00b_gts_stop_the_world(char *, int);
extern void _n00b_gts_wont_stop(char *, int);
extern void _n00b_gts_restart_the_world(char *, int);
extern void n00b_gts_reacquire(void);
extern void n00b_gts_notify_abort(void);

#define n00b_gts_start()             _n00b_gts_start(__FILE__, __LINE__)
#define n00b_gts_suspend()           _n00b_gts_suspend(__FILE__, __LINE__)
#define n00b_gts_resume()            _n00b_gts_resume(__FILE__, __LINE__)
#define n00b_gts_might_stop()        _n00b_gts_might_stop(__FILE__, __LINE__)
#define n00b_gts_wont_stop()         _n00b_gts_stop_the_world(__FILE__, __LINE__)
#define n00b_gts_stop_the_world()    _n00b_gts_stop_the_world(__FILE__, __LINE__)
#define n00b_gts_restart_the_world() _n00b_gts_restart_the_world(__FILE__, __LINE__)

extern void           n00b_thread_stack_region(n00b_thread_t *);
extern n00b_thread_t *n00b_thread_register(void);
extern void           n00b_thread_unregister(void *);
extern n00b_thread_t *n00b_thread_spawn(void *(*)(void *), void *);
extern void           n00b_thread_cancel_other_threads(void);

static inline void
n00b_thread_prevent_cancelation(void)
{
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
}

static inline void
n00b_thread_async_cancelable(void)
{
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
}

typedef struct {
    void *(*true_cb)(void *);
    void *true_arg;
} n00b_tbundle_t;

// 1/1000th of a second
#define N00B_GTS_POLL_DURATION_NS 100000

#ifdef N00B_USE_INTERNAL_API
extern void           n00b_threading_setup(void);
extern n00b_thread_t *n00b_thread_list_acquire(void);
extern void           n00b_thread_list_release(void);
extern bool           n00b_current_process_is_exiting(void);
extern void           n00b_initialize_global_thread_info(void);
extern n00b_thread_t *n00b_thread_find_by_pthread_id(pthread_t);
#endif
