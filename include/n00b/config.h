#ifdef n00b_some_stuff_i_may_occasionally_enable_in_testing

#define N00B_USE_LOCK_DEBUGGING
#define N00B_ENABLE_ALLOC_DEBUG_OPT_IN
#define N00B_GC_SHOW_COLLECT_STACK_TRACES
#define N00B_DEBUG_GC_ROOTS
#define N00B_GC_ALLOW_DEBUG_BIT
#define N00B_MPROTECT_WRAPPED_ALLOCS
#define N00B_FLOG_DEBUG
#define N00B_FULL_MEMCHECK
#define N00B_DEBUG_LOCKS
#else
#endif

// #define N00B_GC_STATS
//  #define N00B_DEBUG_GC_ROOTS
// #define N00B_FIND_SCRIBBLES
// #define N00B_SCAN_ALLOC
// #define N00B_USE_LOCK_DEBUGGING

#pragma once
// Home of anything remotely configurable. Don't change this file;
// update the meson config.
//
// If it's not in the meson options, then I'd discourage you from even
// considering changing the value.
#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define __n00b_have_asan__
#endif // address_sanitizer
#elif defined(__SANITIZE_ADDRESS__)
#define __n00b_have_asan__
#endif // __has_feature

#define N00B_FORCED_ALIGNMENT 16

#ifdef N00B_DEBUG_PATTERNS
#undef N00B_DEBUG_PATTERNS
#endif

#ifdef N00B_PARSE_DEBUG
#undef N00B_PARSE_DEBUG
#endif

#ifdef N00B_ADD_ALLOC_LOC_INFO
#undef N00B_ADD_ALLOC_LOC_INFO
#endif

#ifdef N00B_DEBUG_KARGS
#undef N00B_DEBUG_KARGS
#endif

#ifdef N00B_TYPE_LOG
#undef N00B_TYPE_LOG
#endif

#ifdef N00B_SB_DEBUG
#undef N00B_SB_DEBUG
#endif

#ifdef N00B_SB_TEST
#undef N00B_SB_TEST
#endif

#if !defined(N00B_DEV)
#ifdef N00B_DEBUG
#error "N00B_DEBUG requires N00B_DEV"
#endif
#ifdef N00B_GC_STATS
#error "N00B_GC_STATS requires N00B_DEV"
#endif
#ifdef N00B_FULL_MEMCHECK
#error "N00B_FULL_MEMCHECK requires N00B_DEV"
#endif
#ifdef N00B_VM_DEBUG
#error "N00B_VM_DEBUG requires N00B_DEV"
#endif

#endif

#if !defined(HATRACK_PER_INSTANCE_AUX)
#error "HATRACK_PER_INSTANCE_AUX must be defined for n00b to compile."
#endif

#if defined(N00B_GC_SHOW_COLLECT_STACK_TRACES)
#define N00B_GC_STATS
#endif

#if defined(HATRACK_ALLOC_PASS_LOCATION)
#define N00B_ADD_ALLOC_LOC_INFO
#else
#if defined(N00B_GC_STATS)
#error "N00B_GC_STATS cannot be enabled without HATRACK_ALLOC_PASS_LOCATION"
#endif // N00B_GC_STATS

#endif // HATRACK_ALLOC_PASS_LOCATION

#if defined(N00B_VM_DEBUG) && !defined(N00B_VM_DEBUG_DEFAULT)
#define N00B_VM_DEBUG_DEFAULT false
#endif

#if defined(N00B_GC_FULL_TRACE) && !defined(N00B_GC_FULL_TRACE_DEFAULT)
#define N00B_GC_FULL_TRACE_DEFAULT 1
#endif

// This is a bit misnamed. It means 'minimum if nothing is explicitly
// supplied'.  Generally the user can supply a speicific width, and we
// query the terminal for rendering. But this is the fallback if we
// get nothing passed and nothing from the terminal...
#ifndef N00B_MIN_RENDER_WIDTH
#define N00B_MIN_RENDER_WIDTH 80
#endif

#ifndef N00B_DEBUG
#if defined(N00B_WATCH_SLOTS) || defined(N00B_WATCH_LOG_SZ)
#warning "Watchpoint compile parameters set, but watchpoints are disabled"
#endif // either set
#else
#ifndef N00B_WATCH_SLOTS
#define N00B_WATCH_SLOTS 30
#endif

#ifndef N00B_WATCH_LOG_SZ
#define N00B_WATCH_LOG_SZ (1 << 14)
#endif
#endif // N00B_DEBUG

#ifndef N00B_MAX_KARGS_NESTING_DEPTH
// Must be a power of two, and probably shouldn't be lower.
#define N00B_MAX_KARGS_NESTING_DEPTH 32
#endif

// More accurately, max # of supported keywords.
// We use a lot internally, and there's a max based on some macros in
// an include file, so probably best not to mess w/ this one.
#ifndef N00B_MAX_KEYWORD_SIZE
#define N00B_MAX_KEYWORD_SIZE 50
#endif

#ifndef N00B_EMPTY_BUFFER_ALLOC
#define N00B_EMPTY_BUFFER_ALLOC 128
#endif

#ifndef N00B_DEFAULT_HEAP_SIZE
// This is the size any test case that prints a thing grows to awfully fast.
#define N00B_DEFAULT_HEAP_SIZE (1 << 26) // 30 == 1 g
#endif

#ifndef N00B_MARSHAL_CHUNK_SIZE
#define N00B_MARSHAL_CHUNK_SIZE 512
#endif

#ifndef N00B_STACK_SIZE
#define N00B_STACK_SIZE (1 << 17)
#endif

#ifndef N00B_MAX_CALL_DEPTH
#define N00B_MAX_CALL_DEPTH 100
#endif

#if defined(N00B_GC_STATS) && !defined(N00B_SHOW_GC_DEFAULT)
#define N00B_SHOW_GC_DEFAULT 0
#endif

#ifdef N00B_GC_ALL_ON
#define N00B_GC_DEFAULT_ON  1
#define N00B_GC_DEFAULT_OFF 1
#elif defined(N00B_GC_ALL_OFF)
#define N00B_GC_DEFAULT_ON  0
#define N00B_GC_DEFAULT_OFF 0
#else
#define N00B_GC_DEFAULT_ON  1
#define N00B_GC_DEFAULT_OFF 0
#endif

#ifndef N00B_GCT_INIT
#define N00B_GCT_INIT N00B_GC_DEFAULT_ON
#endif
#ifndef N00B_GCT_MMAP
#define N00B_GCT_MMAP N00B_GC_DEFAULT_ON
#endif
#ifndef N00B_GCT_MUNMAP
#define N00B_GCT_MUNMAP N00B_GC_DEFAULT_ON
#endif
#ifndef N00B_GCT_SCAN
#define N00B_GCT_SCAN N00B_GC_DEFAULT_ON
#endif
#ifndef N00B_GCT_OBJ
#define N00B_GCT_OBJ N00B_GC_DEFAULT_OFF
#endif
#ifndef N00B_GCT_SCAN_PTR
#define N00B_GCT_SCAN_PTR N00B_GC_DEFAULT_OFF
#endif
#ifndef N00B_GCT_PTR_TEST
#define N00B_GCT_PTR_TEST N00B_GC_DEFAULT_OFF
#endif
#ifndef N00B_GCT_PTR_TO_MOVE
#define N00B_GCT_PTR_TO_MOVE N00B_GC_DEFAULT_OFF
#endif
#ifndef N00B_GCT_MOVE
#define N00B_GCT_MOVE N00B_GC_DEFAULT_OFF
#endif
#ifndef N00B_GCT_ALLOC_FOUND
#define N00B_GCT_ALLOC_FOUND N00B_GC_DEFAULT_OFF
#endif
#ifndef N00B_GCT_PTR_THREAD
#define N00B_GCT_PTR_THREAD N00B_GC_DEFAULT_OFF
#endif
#ifndef N00B_GCT_WORKLIST
#define N00B_GCT_WORKLIST N00B_GC_DEFAULT_ON
#endif
#ifndef N00B_GCT_COLLECT
#define N00B_GCT_COLLECT N00B_GC_DEFAULT_ON
#endif
#ifndef N00B_GCT_REGISTER
#define N00B_GCT_REGISTER N00B_GC_DEFAULT_ON
#endif
#ifndef N00B_GCT_ALLOC
#define N00B_GCT_ALLOC N00B_GC_DEFAULT_OFF
#endif
#ifndef N00B_MAX_GC_ROOTS
#define N00B_MAX_GC_ROOTS (1 << 15)
#endif

#ifndef N00B_TEST_SUITE_TIMEOUT_SEC
#define N00B_TEST_SUITE_TIMEOUT_SEC 1
#endif
#ifndef N00B_TEST_SUITE_TIMEOUT_USEC
#define N00B_TEST_SUITE_TIMEOUT_USEC 0
#endif

#ifdef N00B_PACKAGE_INIT_MODULE
#undef N00B_PACKAGE_INIT_MODULE
#endif
#define N00B_PACKAGE_INIT_MODULE "__init"

#define N00B_DEFAULT_DEBUG_PORT 5238

#ifndef N00B_IDLE_IO_SLEEP
#define N00B_IDLE_IO_SLEEP 500
#endif

#ifndef N00B_CALLBACK_THREAD_POLL_INTERVAL
#define N00B_CALLBACK_THREAD_POLL_INTERVAL 50000000
#endif

#ifndef N00B_POLL_DEFAULT_MS
#define N00B_POLL_DEFAULT_MS 1
#endif
/*
 * Since all our I/O is asynchonous by default, when someone chooses
 * to exit a process with n00b_exit(), they probably don't want to
 * exit immediately... they want to wait for any queued I/O to finish
 * first.
 *
 * However, it's incredibly easy for programs to have I/O feedback
 * cycles that keep a program alive forever.
 *
 * Calling n00b_thread_exit() makes no attempt to shut down the
 * program; the I/O loop and callbacks can run forever. But when you
 * call n00b_exit(), we want to let a reasonable amount of queued I/O
 * process, and allow at least a few cycles (lots of real deliveries
 * will include multiple hops, and we always add a cycle there.
 *
 * The default should generally amount to up to a tenth of a second of
 * waiting on exit.
 *
 */
#ifndef N00B_IO_SHUTDOWN_NSEC
#define N00B_IO_SHUTDOWN_NSEC 40000000ULL
#endif

#define N00B_IO_DEBUG_SHUTDOWN_SEC 100

#ifndef N00B_ASYNC_IO_CALLBACKS
#undef N00B_ASYNC_IO_CALLBACKS
#endif

// Number of extra stack words to capture per-thread to account for
// any stack frame that might happen in the gc thread after the stack
// is captured, etc.
#ifndef N00B_STACK_SLOP
// -32
#define N00B_STACK_SLOP 0
#endif

// GC stops scanning a memory record for pointers when it sees this.
#define N00B_NOSCAN      0xefefefeffefefefeULL
// GC, when scanning backward to find an alloc header, jumps back a
// page and a word when it sees this (to avoid an mprotect region)
#define N00B_GC_PAGEJUMP 0xdfdfdfdffdfdfdfdULL

#if defined(N00B_USE_LOCK_DEBUGGING)
#define N00B_DEBUG_LOCKS
#endif

// Current N00b version info.
#define N00B_VERS_MAJOR   0x00
#define N00B_VERS_MINOR   0x02
#define N00B_VERS_PATCH   0x08
#define N00B_VERS_PREVIEW 0x00

#undef N00B_MPROTECT_GUARD_ALLOCS
#if !defined(N00B_NO_ALLOC_GUARDS)
#define N00B_MPROTECT_GUARD_ALLOCS
#endif

#undef N00B_ENABLE_ALLOC_DEBUG
#if defined(N00B_ALLOC_LOC_INFO) && defined(N00B_ENABLE_ALLOC_DEBUG_OPT_IN)
#define N00B_ENABLE_ALLOC_DEBUG
#endif

#undef N00B_SHOW_GC_ROOTS
#ifdef N00B_DEBUG_GC_ROOTS
#if !defined(N00B_ADD_ALLOC_LOC_INFO)
#define N00B_ADD_ALLOC_LOC_INFO
#endif
#define N00B_SHOW_GC_ROOTS
#endif

#undef N00B_GC_ALLOW_DEBUG_BIT
#if defined(N00B_DEBUG) && defined(N00B_USE_GC_DEBUG_BIT)
#define N00B_GC_ALLOW_DEBUG_BIT
#endif

#undef N00B_SOCK_LISTEN_BACKLOG_DEFAULT
#if defined(N00B_DEFAULT_LISTEN_BACKLOG)
#define N00B_SOCK_LISTEN_BACKLOG_DEFAULT N00B_DEFAULT_LISTEN_BACKLOG
#else
#define N00B_SOCK_LISTEN_BACKLOG_DEFAULT 32
#endif

#if !defined(N00B_MAX_SIGNAL)
#define N00B_MAX_SIGNAL 128
#endif

#if !defined(N00B_ENV_DBG_ADDR)
#define N00B_ENV_DBG_ADDR "N00B_DEBUG_ADDRESS"
#endif

#if !defined(N00B_ENV_DBG_PORT)
#define N00B_ENV_DBG_PORT "N00B_DEBUG_PORT"
#endif

#if !defined(N00B_ENV_DBG_LOG)
#define N00B_ENV_DBG_LOG "N00B_DEBUG_LOG"
#endif
