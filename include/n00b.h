#pragma once

#define _DEFAULT_SOURCE
#ifndef TIMEVAL_TO_TIMESPEC
# define TIMEVAL_TO_TIMESPEC(tv, ts)              \
   do { (ts)->tv_sec  = (tv)->tv_sec;             \
        (ts)->tv_nsec = (tv)->tv_usec * 1000;     \
   } while (0)
#endif
#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L

#include "n00b/config.h"
#include "vendor.h"
#include "n00b/base.h" // Third-party headers widely used.#

#include "adts/zarray.h"
#include "core/heap.h"
#include "core/exceptions.h"
#include "core/kargs.h"

#include "mt/atomic.h"
#include "mt/gil.h"
#include "mt/thread.h"
#include "mt/lock_common.h"
#include "mt/mutex.h"
#include "mt/condition.h"
#include "mt/rwlock.h"
#include "mt/tsi.h" // Thread-specific info.

#include "adts/lists.h"
#include "adts/dict.h"

#include "core/vtable.h"
#include "core/ctype_info.h"
#include "core/builtin_ctypes.h"

#include "core/type.h"
#include "util/defer.h"
#include "util/assert.h"
#include "util/close.h"
#include "core/exit.h"
#include "adts/box.h"
#include "text/colors.h"
#include "text/codepoints.h"
#include "adts/flags.h"
#include "adts/tree.h"
#include "adts/buffer.h"
#include "util/crypto.h"
#include "adts/mixed.h"
#include "adts/tuple.h"
#include "core/literals.h"
#include "compiler/scope.h"
#include "compiler/module.h"
#include "compiler/cfg.h"
#include "compiler/lex.h"
#include "compiler/spec.h"
#include "compiler/parse.h"
#include "core/ffi.h"
#include "core/ufi.h"
#include "adts/callback.h"
#include "compiler/instruction.h"
#include "compiler/nodeinfo.h"
#include "compiler/errors.h"
#include "compiler/memmodel.h"
#include "core/vm.h"
#include "compiler/compile.h"
#include "compiler/rtmodule.h"
#include "compiler/codegen.h"
#include "core/cmethod.h"
#include "adts/number.h"

#include "core/signal.h"
#include "core/init.h"
#include "core/env.h"    // env variable API
#include "util/macros.h" // Helper macros
#include "util/random.h"

// Type system API.
#include "text/string.h"
#include "util/tree_pattern.h"

// Basic string handling.
#include "text/breaks.h"
#include "text/theme.h"
#include "text/style.h"
#include "text/rich.h"
#include "text/word_breaks.h"

// Other data types.
#include "adts/set.h"
#include "adts/net_addr.h"
#include "adts/datetime.h"
#include "adts/duration.h"
#include "adts/bytering.h"
#include "text/table.h"
#include "util/sleep.h"

// IO primitives.

#include "io/fd_event.h"
#include "io/observable.h"
#include "io/filter.h"
#include "io/stream.h"
#include "io/stream_fd.h"
#include "io/stream_cb.h"
#include "io/stream_buffer.h"
#include "io/stream_topic.h"
#include "io/stream_exit.h"
#include "io/stream_proxy.h"
#include "io/stream_string.h"
#include "io/terminal_io.h"
#include "io/marshal.h"

#include "io/print.h"

#include "text/layout.h"
#include "text/ansi.h"
#include "text/regex.h"

#include "io/proc.h"
#include "io/session.h"
#include "io/http.h"

// Path handling utilities.
#include "util/path.h"

// The core runtime
#include "runtime/vm.h"

// Really int log2 only right now.
#include "util/math.h"

#include "util/cbacktrace.h"
#include "util/json.h"

#include "compiler/lex.h"
#include "compiler/parse.h"

#include "runtime/ffi.h"
#include "util/watch.h"

#include "util/parsing.h"  // generic parser via Earley parsing.
#include "util/getopt.h"   // Getopt parsing.
#include "text/markdown.h" // Wrap of vendored md4c.
#include "util/limits.h"

// Helper functions for object marshal implementations to
// marshal primitive values.

#include "util/testgen.h"

#include "debug/workflow.h"
#include "debug/streams.h"
#include "debug/topic_api.h"
#include "debug/memring.h"
#include "debug/dlogf.h"
#include "debug/dump.h"

#include "core/builtin_tinfo_gen.h"
#include "core/cobject_api.h"
#include "mt/lock_api.h"
#include "core/literal_api.h"
#include "compiler/error_api.h"

// All inlined stuff comes after data types are declared.
#include "core/kargs_inline.h"
#include "core/heap_inline.h"
#include "adts/lists_inline.h"
#include "core/type_inline.h"
#include "core/obj_inline.h"
#include "text/string_inline.h"
#include "adts/buffer_inline.h"
#include "adts/tree_inline.h"
#include "adts/mixed_inline.h"
#include "mt/futex_inline.h"
#include "core/exception_inline.h"
#include "adts/box_inline.h"
#include "text/codepoint_inline.h"
#include "mt/tsi_inline.h"
#include "compiler/lex_inline.h"
#include "compiler/parse_inline.h"
#include "compiler/scope_inline.h"
#include "compiler/module_inline.h"
#include "util/va_list.h" // Minor helper
#include "text/breaks_inline.h"
#include "text/theme_inline.h"
#include "text/style_inline.h"
#include "text/regex_inline.h"
#include "text/table_inline.h"
#include "adts/datetime_inline.h"
#include "io/observable_inline.h"
#include "io/stream_inline.h"
#include "io/proc_inline.h"
#include "io/session_inline.h"
#include "io/http_inline.h"
#include "util/path_inline.h"
#include "util/hex_api.h"
#include "util/parsing_inline.h"
#include "util/getopt_inline.h"
#include "io/fd_inline.h"
#include "adts/duration_inline.h"
// For functions we need to wrap to use through the FFI.
#include "util/wrappers.h"
#include "debug/debug_inline.h"
#include "compiler/ast_utils.h"
