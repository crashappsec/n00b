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
extern char *n00b_m_log_string(char *, char *, int64_t N00B_ALLOC_XTRA);

// clang-format on

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
