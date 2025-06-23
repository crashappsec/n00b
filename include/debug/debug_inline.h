#pragma once
#include "n00b.h"
#include "n00b/debug_config.h"

#ifdef N00B_DEBUG
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
#endif
