/*
 * Copyright © 2021-2022 John Viega
 *
 *
 *  Name:           memring.h
 *
 *  Description:    Provides support that I've found useful for dealing
 *                  with multi-threaded operations.  Right now, this
 *                  consists of some custom assertion macros, and an
 *                  in-memory ring buffer.
 *
 *                  None of this code is used unless N00B_DEBUG is on.
 *
 *                  This code is less concise and elegent than it
 *                  could be: for instance, if we were to use sprintf()
 *                  instead of manually formatting strings.
 *
 *                  However, given the frequency at which this code
 *                  can get called, using sprintf() has a HUGE
 *                  negative impact on performance.
 *
 *                  So yes, I've forgone clarity for performance, all
 *                  to try to make it easier to debug busy
 *                  multi-threaded apps, without having too
 *                  detrimental an impact to performance.
 *
 *  Author:         John Viega, john@zork.org
 */

#pragma once
#define N00B_USE_INTERNAL_API
#include "n00b.h" // IWYU pragma: keep

#ifdef N00B_DEBUG

/* n00b_m_debug_record_t
 *
 * The type for records in our ring buffer.
 *
 * Note that the field named 'null' is intended to always be zero.
 * Records are strncpy()'d into the msg array, but just in case they
 * go right up to the end of the array, make sure we get a zero,
 * whatever the semantics of the strncpy() implementation.
 */
typedef struct {
    char     msg[N00B_DEBUG_MSG_SIZE];
    char     null;
    uint64_t sequence;
    int64_t  thread;
} n00b_m_debug_record_t;

/*
 * __n00b_m_debug_sequence is a monotoincally increasing counter for
 * debug messages. Threads that use our debugging API will get a
 * unique number per-debug message, by using atomic_fetch_add() on the
 * variable __n00b_m_debug_sequence.
 *
 * The value returned from that atomic_fetch_add() will map to an
 * entry in the ring buffer to which they may write, by modding
 * against the number of entries in the table.
 *
 * As long as our ring buffer is big enough, this will ensure that
 * threads don't stomp on each other's insertions while a write is in
 * progress.
 */

// clang-format off
extern n00b_m_debug_record_t  __n00b_m_debug[];
extern _Atomic uint64_t       __n00b_m_debug_sequence;
extern const char             __n00b_m_hex_conversion_table[];

/* The below functions are all defined in memring.c. Again, they're
 * only compiled in if N00B_DEBUG is on. They are meant for providing
 * access to the ring buffer, generally from within a debugger.
 *
 * For instance, I have aliases to call each of these functions in my
 * debugger init file.
 *
 * n00b_mdebug_dump() will also get called when an assertion (via our
 * macros) fails.
 */
extern void n00b_mdebug_dump         (uint64_t);
extern void n00b_mdebug_file(char *fname);
extern void n00b_mdebug_thread       (void);
extern void n00b_mdebug_other_thread (int64_t);
extern void n00b_mdebug_grep         (char *);
extern void n00b_mdebug_pgrep        (uintptr_t);
extern n00b_m_debug_record_t *n00b_mdebug_get(int64_t n);
extern char *n00b_m_log_string(char *, char *, int64_t, char *, int);

// clang-format on

/* n00b_m_debug()
 *
 * Writes a message into the ring buffer.
 */
static inline void
n00b_m_debug(char *msg)
{
    uint64_t               mysequence;
    uint64_t               index;
    n00b_m_debug_record_t *record_ptr;
    n00b_tsi_t            *tsi = n00b_get_tsi_ptr();

    mysequence           = atomic_fetch_add(&__n00b_m_debug_sequence, 1);
    index                = mysequence & N00B_DEBUG_RING_LAST_SLOT;
    record_ptr           = &__n00b_m_debug[index];
    record_ptr->sequence = mysequence;
    record_ptr->thread   = tsi->thread_id;

    strncpy(record_ptr->msg, msg, N00B_DEBUG_MSG_SIZE);

    return;
}

/* This is one of those things not we don't intend people to
 * configure; it's just there to avoid arcane numbers in the source
 * code.
 *
 * But the name really isn't much more descriptive, I admit!
 *
 * This represents the number of characters we need to allocate
 * statically on the stack for 'fixed' characters when constructing
 * the message to put in the record, in the function
 * n00b_m_debug_ptr.  The four characters are the "0x" before the
 * pointer, and the ": " after.
 */
#undef N00B_PTR_FMT_CHRS
#define N00B_PTR_FMT_CHRS 4

/* n00b_m_debug_ptr()
 *
 * Writes a message to the ring buffer, that basically starts with the
 * hex representation of a pointer, followed by the message passed in.
 *
 * Though, to be honest, I currently use this for ALL numbers I need
 * to write into the ring buffer, which is why you don't see a
 * n00b_m_debug_int() yet.
 */
static inline void
n00b_m_debug_ptr(void *addr, char *msg)
{
    char buf[N00B_PTR_CHRS + N00B_PTR_FMT_CHRS + 1] = {
        '0',
        'x',
    };
    char                  *p = buf + N00B_PTR_CHRS + N00B_PTR_FMT_CHRS;
    uint64_t               i;
    uintptr_t              n = (uintptr_t)addr;
    uint64_t               mysequence;
    n00b_m_debug_record_t *record_ptr;
    n00b_tsi_t            *tsi = n00b_get_tsi_ptr();

    mysequence = atomic_fetch_add(&__n00b_m_debug_sequence, 1);
    record_ptr = &__n00b_m_debug[mysequence & N00B_DEBUG_RING_LAST_SLOT];

    record_ptr->sequence = mysequence;
    record_ptr->thread   = tsi->thread_id;

    *--p = ' ';
    *--p = ':';

    for (i = 0; i < N00B_PTR_CHRS; i++) {
        *--p = __n00b_m_hex_conversion_table[n & 0xf];
        n >>= 4;
    }
    strcpy(record_ptr->msg, buf);
    strncpy(record_ptr->msg + N00B_PTR_CHRS + N00B_PTR_FMT_CHRS,
            msg,
            N00B_DEBUG_MSG_SIZE - N00B_PTR_CHRS - N00B_PTR_FMT_CHRS);

    return;
}

static inline void
n00b_m_debug2(void *addr, void *addr2, char *msg)
{
    char buf[N00B_PTR_CHRS + N00B_PTR_FMT_CHRS + 1] = {
        '0',
        'x',
    };

    char                  *p = buf + N00B_PTR_CHRS + N00B_PTR_FMT_CHRS;
    uint64_t               i;
    uintptr_t              n = (uintptr_t)addr;
    char                  *where;
    char                  *end;
    uint64_t               mysequence;
    n00b_m_debug_record_t *record_ptr;
    n00b_tsi_t            *tsi = n00b_get_tsi_ptr();

    mysequence = atomic_fetch_add(&__n00b_m_debug_sequence, 1);
    record_ptr = &__n00b_m_debug[mysequence & N00B_DEBUG_RING_LAST_SLOT];
    end        = record_ptr->msg + N00B_DEBUG_MSG_SIZE;

    record_ptr->sequence = mysequence;
    record_ptr->thread   = tsi->thread_id;

    *--p = ' ';
    *--p = ':';

    for (i = 0; i < N00B_PTR_CHRS; i++) {
        *--p = __n00b_m_hex_conversion_table[n & 0xf];
        n >>= 4;
    }
    strcpy(record_ptr->msg, buf);

    p = buf + N00B_PTR_CHRS + N00B_PTR_FMT_CHRS;
    n = (uintptr_t)addr2;

    *--p = ' ';
    *--p = ':';

    for (i = 0; i < N00B_PTR_CHRS; i++) {
        *--p = __n00b_m_hex_conversion_table[n & 0xf];
        n >>= 4;
    }

    where = record_ptr->msg + N00B_PTR_CHRS + N00B_PTR_FMT_CHRS;
    strncpy(where, buf, end - where);

    where = where + N00B_PTR_CHRS + N00B_PTR_FMT_CHRS;
    strncpy(where, msg, end - where);

    return;
}

static inline void
n00b_m_debug3(void *addr, void *addr2, void *addr3, char *msg)
{
    char buf[N00B_PTR_CHRS + N00B_PTR_FMT_CHRS + 1] = {
        '0',
        'x',
    };

    char                  *p = buf + N00B_PTR_CHRS + N00B_PTR_FMT_CHRS;
    uint64_t               i;
    uintptr_t              n = (uintptr_t)addr;
    char                  *where;
    char                  *end;
    uint64_t               mysequence;
    n00b_m_debug_record_t *record_ptr;
    n00b_tsi_t            *tsi = n00b_get_tsi_ptr();

    mysequence = atomic_fetch_add(&__n00b_m_debug_sequence, 1);
    record_ptr = &__n00b_m_debug[mysequence & N00B_DEBUG_RING_LAST_SLOT];
    end        = record_ptr->msg + N00B_DEBUG_MSG_SIZE;

    record_ptr->sequence = mysequence;
    record_ptr->thread   = tsi->thread_id;

    *--p = ' ';
    *--p = ':';

    for (i = 0; i < N00B_PTR_CHRS; i++) {
        *--p = __n00b_m_hex_conversion_table[n & 0xf];
        n >>= 4;
    }
    strcpy(record_ptr->msg, buf);

    p = buf + N00B_PTR_CHRS + N00B_PTR_FMT_CHRS;
    n = (uintptr_t)addr2;

    *--p = ' ';
    *--p = ':';

    for (i = 0; i < N00B_PTR_CHRS; i++) {
        *--p = __n00b_m_hex_conversion_table[n & 0xf];
        n >>= 4;
    }

    where = record_ptr->msg + N00B_PTR_CHRS + N00B_PTR_FMT_CHRS;
    strncpy(where, buf, end - where);

    p = buf + N00B_PTR_CHRS + N00B_PTR_FMT_CHRS;
    n = (uintptr_t)addr3;

    *--p = ' ';
    *--p = ':';

    for (i = 0; i < N00B_PTR_CHRS; i++) {
        *--p = __n00b_m_hex_conversion_table[n & 0xf];
        n >>= 4;
    }

    where = where + N00B_PTR_CHRS + N00B_PTR_FMT_CHRS;
    strncpy(where, buf, end - where);

    where = where + N00B_PTR_CHRS + N00B_PTR_FMT_CHRS;
    strncpy(where, msg, end - where);

    return;
}

/* n00b_m_debug_assert()
 *
 * This is meant to be called either through the N00B_MASSERT() macro,
 * which fills in the values of function, file and line with their
 * appropriate values.
 *
 * When an assertion made to this function fails, it prints history
 * from the ring buffer to stderr, and then goes into a busy-loop so
 * that you may attach a debugger. I even occasionally jump past the
 * loop to let the program keep running...
 */
static inline void
n00b_m_debug_assert(bool        expression_result,
                    char       *assertion,
                    const char *function,
                    const char *file,
                    int         line)
{
    if (!expression_result) {
        fprintf(stderr,
                "%s:%d: Assertion \"%s\" failed (function: %s; pid: %d)\n",
                file,
                line,
                assertion,
                function,
                getpid());

        n00b_mdebug_dump(N00B_ASSERT_FAIL_RECORD_LEN);

        /* Loop instead of crashing, so that we can attach a
         * debugger.  The asm call forces clang not to optimize
         * this loop away, which it tends to do, even when the C
         * standard tells it not to do so.
         */
        while (true) {
            __asm("");
        }

        fprintf(stderr, "Use the debugger to jump here to keep going.\n");
    }

    return;
}

/* n00b_m_debug_assert_w_params()
 *
 * Like n00b_m_debug_assert() above, except lets you specify a
 * specific number of records to go back, when we dump them, and also
 * allows you to skip the busy wait loop, if you want to do something
 * else (like abort or keep going).
 *
 * This one is meant to be used via the XASSERT() macro.
 */
static inline void
n00b_m_debug_assert_w_params(bool        expression_result,
                             char       *assertion,
                             const char *function,
                             const char *file,
                             int         line,
                             uint32_t    num_records,
                             bool        busy_wait)
{
    if (!expression_result) {
        fprintf(stderr,
                "%s:%d: Assertion \"%s\" failed (in function %s)\n",
                file,
                line,
                assertion,
                function);

        n00b_mdebug_dump(num_records);

        if (busy_wait) {
            /* Loop instead of crashing, so that we can attach a
             * debugger.  The asm call forces clang not to optimize
             * this loop away, which it tends to do, even when the C
             * standard tells it not to do so.
             */
            while (true) {
                __asm("");
            }
        }

        fprintf(stderr, "Use the debugger to jump here to keep going.\n");
    }

    return;
}

static inline void
n00b_m_logf(char *fmt, ...)
{
    uint64_t               mysequence;
    uint64_t               index;
    n00b_m_debug_record_t *record_ptr;
    n00b_tsi_t            *tsi = n00b_get_tsi_ptr();
    va_list                args;

    mysequence           = atomic_fetch_add(&__n00b_m_debug_sequence, 1);
    index                = mysequence & N00B_DEBUG_RING_LAST_SLOT;
    record_ptr           = &__n00b_m_debug[index];
    record_ptr->sequence = mysequence;
    record_ptr->thread   = tsi->thread_id;
    va_start(args, fmt);

    vsnprintf(record_ptr->msg, N00B_DEBUG_MSG_SIZE, fmt, args);

    va_end(args);
}

#define N00B_MDEBUG(x) \
    n00b_m_debug(x)
#define N00B_MDEBUG_PTR(x, y) \
    n00b_m_debug_ptr((void *)(x), y)
#define N00B_MDEBUG2(x, y, m) \
    n00b_m_debug2((void *)(x), (void *)(y), m)
#define N00B_MDEBUG3(x, y, z, m) \
    n00b_m_debug3((void *)(x), (void *)(y), (void *)(z), m)
#define N00B_MASSERT(x) \
    n00b_m_debug_assert(x, #x, __FUNCTION__, __FILE__, __LINE__)
#define N00B_MXASSERT(x, n, b) \
    n00b_m_debug_assert_w_params(x, #x, __FUNCTION__, __FILE__, __LINE__, n, b)
#define N00B_MLOGF(...) n00b_m_logf(__VA_ARGS__)
#else
#define N00B_MDEBUG(x)
#define N00B_MDEBUG_PTR(x, y)
#define N00B_MDEBUG2(x, y, m)
#define N00B_MDEBUG3(x, y, z, m)
#define N00B_MASSERT(x)
#define N00B_MXASSERT(x, n, b)
#define N00B_MLOGF(...)
#endif

#define N00B_MDEBUGF N00B_MLOGF
