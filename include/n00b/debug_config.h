#pragma once
#if defined(N00B_DEBUG)
/* N00B_DEBUG_MSG_SIZE
 *
 * When using the ring bugger, this variable controls how many bytes
 * are available for each ring buffer entry. Try to keep it small. And
 * for alignment purposes, you probably want to keep it a power of
 * two, but at least pointer-aligned.
 *
 * If you do not provide this, and debugging is turned on, you'll get
 * 128 bytes per record.  The minimum value is 32 bytes; any lower
 * value will get you 32 bytes.
 *
 * Note that this number does NOT include the extra stuff we keep
 * along with our records, like thread ID and a message counter. Those
 * things get shown when viewing the log, but you don't have to worry
 * about them with the value you set here.
 *
 * Also note that, if you log something that would take up more space
 * than this value allows, it will be truncated to fit.
 */
#if !defined(N00B_DEBUG_MSG_SIZE)
#define N00B_DEBUG_MSG_SIZE 256
#endif

#if N00B_DEBUG_MSG_SIZE & 0x7
#error "Must keep N00B_DEBUG_MSG_SIZE aligned to an 8 byte boundary"
#endif

#if N00B_DEBUG_MSG_SIZE < 32
#define N00B_DEBUG_MSG_SIZE 32
#endif

/* N00B_DEBUG_RING_LOG
 *
 * This controls how many entries are in the debugging system's ring
 * buffer. As with N00B_MIN_SIZE_LOG above, this is expressed as a
 * base 2 logarithm, so the minimum value of 13 is a mere 8192
 * entries.
 *
 * When building my test cases, they can do so much logging across so many
 * threads, that I've definitely found it valuable to have more than
 * 100K messages in memory, which is why the default value for this is
 * 17 (2^17 = 131,072 entries).
 *
 * In those cases, I've been using 100 threads, reading or writing as
 * fast as they can.  I might need to go back pretty far for data,
 * because threads get preempted...
 *
 * Of course, if you set this value TOO large, you could end up with
 * real memory issues on your system.  But we don't enforce a maximum.
 */
#if !defined(N00B_DEBUG_RING_LOG) || N00B_DEBUG_RING_LOG < 13
#undef N00B_DEBUG_RING_LOG
#define N00B_DEBUG_RING_LOG 18
#endif

#undef N00B_DEBUG_RING_SIZE
#define N00B_DEBUG_RING_SIZE (1 << N00B_DEBUG_RING_LOG)

#undef N00B_DEBUG_RING_LAST_SLOT
#define N00B_DEBUG_RING_LAST_SLOT (N00B_DEBUG_RING_SIZE - 1)

/* N00B_ASSERT_FAIL_RECORD_LEN
 *
 * When using the N00B_MASSERT() macro, when an assertion fails, this
 * variable controls how many previous records to dump from the ring
 * buffer to stdout automatically.
 *
 * Things to note:
 *
 * 1) You can control how much to dump on a case-by-case basis with
 *    the N00B_MXASSERT() macro, instead.  See debug.h.
 *
 * 2) Since the N00B_MASSERT() macro does not stop the entire process,
 *    other threads will continue writing to the ring buffer while the
 *    dump is happening.  If your ring buffer is big enough, I
 *    wouldn't expect that to be a problem, but it's possible that
 *    threads scribble over your debugging data.
 *
 *    If that's an issue, try to get the MASSERT to trigger in a
 *    debugger, setting a breakpoint that only triggers along with the
 *    MASSERT.  That will quickly suspend all the other threads, and
 *    allow you to inspect the state at your leisure.
 *
 * 3) This is NOT a power of two... set it to the actual number of
 *    records you think you want to see.  Default is just 64 records;
 *    you can always choose to dump more.
 */
#ifndef N00B_ASSERT_FAIL_RECORD_LEN
#define N00B_ASSERT_FAIL_RECORD_LEN (1 << 6)
#endif

#if N00B_ASSERT_FAIL_RECORD_LEN > N00B_DEBUG_RING_SIZE
#undef N00B_ASSERT_FAIL_RECORD_LEN
#define N00B_ASSERT_FAIL_RECORD_LEN N00B_DEBUG_RING_SIZE
#endif

/* N00B_PTR_CHRS
 *
 * This variable controls how much of a pointer you'll see when
 * viewing the ring buffer.
 *
 * The default is the full 16 byte pointer; you probably don't need to
 * ever consider changing this one.
 */
#ifndef N00B_PTR_CHRS
#define N00B_PTR_CHRS 16
#endif

#if !defined(N00B_DLOG_DEFAULT_DISABLE)
#define N00B_DLOG_DEFAULT 1
#if !defined(N00B_DLOG_LOCKS_OFF)
#define N00B_DLOG_LOCKS
#endif
#if !defined(N00B_DLOG_ALLOCS_OFF)
#define N00B_DLOG_ALLOCS
#endif
#if !defined(N00B_DLOG_GC_OFF)
#define N00B_DLOG_GC
#endif
#if !defined(N00B_DLOG_GIL_OFF)
#define N00B_DLOG_GIL
#endif
#if !defined(N00B_DLOG_THREADS_OFF)
#define N00B_DLOG_THREADS
#endif
#else
#define N00B_DLOG_DEFAULT 0
#endif

#if !defined(N00B_DLOG_PRIORITY_CUTOFF)
#define N00B_DLOG_PRIORITY_CUTOFF 0
#endif

#if defined(N00B_DLOG_LOCKS)
#undef N00B_DEBUG
#define N00B_DEBUG
#define N00B_DLOG_LOCK_ON
#endif

#if defined(N00B_DLOG_ALLOCS)
#undef N00B_DEBUG
#define N00B_DEBUG
#define N00B_DLOG_ALLOC_ON
#endif

#if defined(N00B_DLOG_GC)
#undef N00B_DEBUG
#define N00B_DEBUG
#define N00B_DLOG_GC_ON
#endif

#if defined(N00B_DLOG_GIL)
#undef N00B_DEBUG
#define N00B_DEBUG
#define N00B_DLOG_GIL_ON
#endif

#if defined(N00B_DLOG_THREADS)
#undef N00B_DEBUG
#define N00B_DEBUG
#define N00B_DLOG_THREADS_ON
#endif

#endif // #ifdef N00B_DEBUG
