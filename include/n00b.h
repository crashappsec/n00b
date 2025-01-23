#pragma once

// #define _XOPEN_SOURCE 700
// #define _POSIX_C_SOURCE 200809L
#include "n00b/config.h"
// Useful options (mainly for dev) are commented out here.
// The logic below (and into the relevent header files) sets up defaults.
//
// Memory management
// #include "core/gc.h"
//
// Everything includes this; the ordering here is somewhat important
// due to interdependencies, though they can always be solved via
// prototyping.
#include "n00b/base.h"
#include "core/init.h"
#include "core/exit.h"
#include "util/macros.h" // Helper macros
#include "core/kargs.h"  // Keyword arguments.
#include "util/random.h"

// Core object.
#include "core/object.h"

#include "util/color.h"

// Basic "exclusive" (i.e., single threaded) list.
#include "adts/list.h"
#include "core/thread.h"

// Type system API.
#include "core/typestore.h"
#include "core/type.h"

#include "adts/box.h"

// Extra data structure stuff.
#include "adts/tree.h"
#include "util/tree_pattern.h"
#include "adts/buffer.h"
#include "adts/tuple.h"

// Basic string handling.
#include "adts/codepoint.h"
#include "adts/string.h"
#include "util/breaks.h"
#include "util/style.h"
#include "util/styledb.h"
#include "util/richlit.h"

// Basic exception handling support.
#include "core/exception.h"

// Other data types.
#include "adts/grid.h"
#include "adts/dict.h"
#include "adts/set.h"
#include "adts/net_addr.h"
#include "adts/datetime.h"
#include "adts/duration.h"
#include "adts/bytering.h"

// IO primitives.
#include "io/term.h"
#include "io/iocore.h"
#include "io/ioqueue.h"
#include "io/proc.h" // To replace subproc.h

// Mixed data type API.
#include "adts/mixed.h"

// Basic internal API to cache and access common string constants.
#include "util/conststr.h"

// Boxes for ordinal types
#include "adts/box.h"

// A few prototypes for literal handling.
#include "core/literal.h"

// Format string API
#include "util/format.h"
#include "util/fp.h"

// Path handling utilities.
#include "util/path.h"

// Yes we use cryptographic hashes internally for type IDing.
#include "crypto/sha.h"

// The core runtime
#include "core/module.h"
#include "core/vm.h"

// Bitfields.
#include "adts/flags.h"

// Really int log2 only right now.
#include "util/math.h"

// For functions we need to wrap to use through the FFI.
#include "util/wrappers.h"

#include "util/cbacktrace.h"
#include "util/json.h"

// The compiler.
#include "compiler/ast_utils.h"
#include "compiler/rtmodule.h"
#include "compiler/compile.h"
#include "compiler/errors.h"
#include "compiler/lex.h"
#include "compiler/parse.h"
#include "compiler/scope.h"
#include "compiler/spec.h"
#include "compiler/cfgs.h"
#include "compiler/codegen.h"

#include "core/ffi.h"
#include "util/watch.h"

#include "io/http.h"
#include "io/file.h"
#include "io/filters.h"
#include "io/ansi.h"
#include "io/marshal.h"

#include "util/parsing.h"  // generic parser via Earley parsing.
#include "util/getopt.h"   // Getopt parsing.
#include "util/markdown.h" // Wrap of vendored md4c.

// Helper functions for object marshal implementations to
// marshal primitive values.
#include "util/highlights.h"
#include "util/hex.h"
#include "io/debug.h"
