#pragma once
#include "n00b/config.h"
// Useful options (mainly for dev) are commented out here.
// The logic below (and into the relevent header files) sets up defaults.
//
// Everything includes this; the ordering here is somewhat important
// due to interdependencies, though they can always be solved via
// prototyping.
#include "n00b/base.h"
#include "core/init.h"
#include "util/macros.h" // Helper macros
#include "core/kargs.h"  // Keyword arguments.
#include "util/random.h"

// Memory management
#include "core/refcount.h"
#include "core/gc.h"

// Core object.
#include "core/object.h"

#include "util/color.h"

// Basic "exclusive" (i.e., single threaded) list.
#include "adts/list.h"

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
#include "io/ansi.h"
#include "util/hex.h"
#include "util/style.h"
#include "util/styledb.h"
#include "util/richlit.h"

// Our grid API.
#include "adts/grid.h"

// IO primitives.
#include "io/term.h"
#include "io/switchboard.h"
#include "io/subproc.h"

// Basic exception handling support.
#include "core/exception.h"

// Stream IO API.
#include "adts/stream.h"

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

#include "adts/dict.h"
#include "adts/set.h"
#include "adts/ipaddr.h"
#include "adts/datetime.h"
#include "adts/duration.h"

#include "core/ffi.h"
#include "util/watch.h"
#include "io/http.h"
#include "io/file.h"

// Helper functions for object marshal implementations to
// marshal primitive values.
#include "core/marshal.h"

#include "util/parsing.h"  // generic parser via Earley parsing.
#include "util/getopt.h"   // Getopt parsing.
#include "util/markdown.h" // Wrap of vendored md4c.
