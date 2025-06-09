// To improve portability in how massaging varargs arguments is
// handled, I switched to a macro system that can "eval" macros (for
// cprintf specifically, which is for internal printing w/o hitting
// any dynamic allocations).
//
// In a bit of acknowledged irony, the macros as implemented do work
// everywhere, but can warn depending on the circumstances. For now,
// this kills the warnings across Linux and Mac.
//
// Can just `include "utils/nowarn.h"` before a call to cprintf, or on
// any files where you're getting warnings from a cprintf.

// See nowarn_pop to turn it off.

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat"
#pragma GCC diagnostic ignored "-Wformat-extra-args"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wunknown-pragmas"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat"
#pragma clang diagnostic ignored "-Wint-to-void-pointer-cast"
