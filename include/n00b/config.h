#pragma once
#define N00B_GC_SHOW_COLLECT_STACK_TRACES
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
#ifdef N00B_VM_WARN_ON_ZERO_ALLOCS
#error "N00B_VM_WARN_ON_ZERO_ALLOCS requires N00B_DEV"
#endif

#endif

#if !defined(HATRACK_PER_INSTANCE_AUX)
#error "HATRACK_PER_INSTANCE_AUX must be defined for n00b to compile."
#endif

#if defined(N00B_GC_FULL_TRACE) && !defined(N00B_GC_STATS)
#define N00B_GC_STATS
#endif

#if defined(N00B_FULL_MEMCHECK) && !defined(N00B_GC_STATS)
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
#ifndef N00B_MAX_KEYWORD_SIZE
#define N00B_MAX_KEYWORD_SIZE 32
#endif

#if defined(N00B_FULL_MEMCHECK)
// #define N00B_SHOW_NEXT_ALLOCS 1000
#ifndef N00B_MEMCHECK_RING_SZ
// Must be a power of 2.
#define N00B_MEMCHECK_RING_SZ 128
#endif // RING_SZ

#if N00B_MEMCHECK_RING_SZ != 0
#define N00B_USE_RING
#else
#undef N00B_USE_RING
#endif // N00B_MEMCHECK_RING_SZ
#endif // N00B_FULL_MEMCHECK

#ifndef N00B_EMPTY_BUFFER_ALLOC
#define N00B_EMPTY_BUFFER_ALLOC 128
#endif

#ifndef N00B_DEFAULT_ARENA_SIZE
// This is the size any test case that prints a thing grows to awfully fast.
#define N00B_DEFAULT_ARENA_SIZE (1 << 24)
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

// Current N00b version info.
#define N00B_VERS_MAJOR   0x00
#define N00B_VERS_MINOR   0x02
#define N00B_VERS_PATCH   0x08
#define N00B_VERS_PREVIEW 0x00
