#include "n00b.h"

#ifdef N00B_SHOW_PREPROC_CONFIG

#define STR_EXPAND(x) #x
#define STR(x)        STR_EXPAND(x)

#ifdef N00B_DEV
#pragma message "N00B_DEV is ON (development mode)"
#else
#pragma message "N00B_DEV is OFF (no development mode; required for all tests)"
#endif

#ifdef N00B_DEBUG
#pragma message "N00B_DEBUG is ON (C stack traces on thrown exceptions, watchpoints, etc)"
#else
#pragma message "N00B_DEBUG is OFF (no C stack traces on thrown exceptions)"
#ifndef N00B_DEV
#pragma message "enabling requires N00B_DEV"
#endif
#endif

// Adds n00b_end_guard field.
#ifdef N00B_FULL_MEMCHECK
#pragma message "N00B_FULL_MEMCHECK is ON (memory guards around each allocation checked at GC collect)"

#ifdef N00B_STRICT_MEMCHECK
#pragma message "N00B_STRICT_MEMCHECK is ON (abort on any memcheck error)"
#else
#pragma message "N00B_STRICT_MEMCHECK is OFF (don't abort on any memcheck error)"
#endif

#ifdef N00B_SHOW_NEXT_ALLOCS
#pragma message "N00B_SHOW_NEXT_ALLOCS is: " STR(N00B_SHOW_NEXT_ALLOCS)
#else
#pragma message "N00B_SHOW_NEXT_ALLOCS is not set (show next allocs on memcheck error)"
#endif
#else
#pragma message "N00B_FULL_MEMCHECK is OFF (no memory guards around each allocation)"
#ifndef N00B_DEV
#pragma message "enabling requires N00B_DEV"
#endif
#endif

#ifdef N00B_TRACE_GC
#pragma message "N00B_TRACE_GC is ON (garbage collection tracing is enabled)"
#else
#pragma message "N00B_TRACE_GC is OFF (garbage collection tracing is disabled)"
#ifndef N00B_DEV
#pragma message "enabling requires N00B_DEV"
#endif
#endif

#ifdef N00B_USE_FRAME_INTRINSIC
#pragma message "N00B_USE_FRAME_INSTRINSIC is ON"
#else
#pragma message "N00B_USE_FRAME_INSTRINSIC is OFF"
#endif

#ifdef N00B_VM_DEBUG
#pragma message "N00B_VM_DEBUG is ON (virtual machine debugging is enabled)"

#if defined(N00B_VM_DEBUG_DEFAULT)
#if N00B_VM_DEBUG_DEFAULT == true
#define __vm_debug_default
#endif
#endif

#ifdef __vm_debug_default
#pragma message "N00B_VM_DEBUG_DEFAULT is true (VM debugging on by default)"
#else
#pragma message "VM debugging is off unless a debug instruction turns it on."
#endif

#else
#pragma message "N00B_VM_DEBUG is ON (virtual machine debugging is disabled)"
#ifndef N00B_DEV
#pragma message "enabling requires N00B_DEV"
#endif
#endif

#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define __n00b_have_asan__
#endif
#else
#ifdef HAS_ADDRESS_SANITIZER
#define __n00b_have_asan__
#endif
#endif

#ifdef __n00b_have_asan__
#pragma message "ADDRESS_SANTITIZER is ON (llvm memory checking enabled)"
#else
#pragma message "ADDRESS_SANTITIZER is OFF (llvm memory checking disabled)"
#ifndef N00B_DEV
#pragma message "enabling requires N00B_DEV"
#endif
#endif

#ifdef N00B_OMIT_UNDERFLOW_CHECKS
#pragma message "N00B_OMIT_UNDERFLOW_CHECKS is ON (Avoid some UBSAN falses)"
#else
#pragma message "N00B_OMIT_UNDERFLOW_CHECKS is OFF (Will see some UBSAN falses)"
#endif

#ifdef N00B_WARN_ON_ZERO_ALLOCS
#pragma message "N00B_WARN_ON_ZERO_ALLOCS is ON (Identify zero-length allocations)"
#else
#pragma message "N00B_WARN_ON_ZERO_ALLOCS is OFF (No spam about 0-length allocs)"
#ifndef N00B_DEV
#pragma message "enabling requires N00B_DEV"
#endif
#endif

#ifdef N00B_GC_SHOW_COLLECT_STACK_TRACES
#pragma message "N00B_GC_SHOW_COLLECT_STACK_TRACES is ON (Show C stack traces at every garbage collection invocation."
#else
#pragma message "N00B_GC_SHOW_COLLECT_STACK_TRACES is OFF (no C stack traces for GC collections)"
#ifndef N00B_DEV
#pragma message "enabling requires N00B_DEV"
#endif
#endif

#ifdef N00B_PARANOID_STACK_SCAN
#pragma message "N00B_PARANOID_STACK_SCAN is ON (Find slow, unaligned pointers)"
#else
#pragma message "N00B_PARANOID_STACK_SCAN is OFF (Does not look for unaligned pointers; this is slow and meant for debugging)"
#endif

#ifdef N00B_MIN_RENDER_WIDTH
#pragma message "N00B_MIN_RENDER_WIDTH is: " STR(N00B_MIN_RENDER_WIDTH)
#else
#pragma message "N00B_MIN_RENDER_WIDTH is not set."
#endif

#ifdef N00B_TEST_WITHOUT_FORK
#pragma message "N00B_TEST_WITHOUT_FORK is: ON (No forking during testing.)"
#else
#pragma message "N00B_TEST_WITHOUT_FORK is: OFF (Tests fork.)"
#endif

#ifdef _GNU_SOURCE
#pragma message "_GNU_SOURCE is ON (gnu stdlib)"
#else
#pragma message "_GNU_SOURCE is OFF (not a gnu stdlib)."
#endif

#ifdef HAVE_PTY_H
#pragma message "HAVE_PTY_H is ON (forkpty is in pty.h)"
#else
#pragma message "HAVE_PTY_H is OFF (forkpty is hopefully in util.h)"
#endif

#endif
