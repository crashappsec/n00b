#pragma once

// #define _XOPEN_SOURCE 700
// #define _POSIX_C_SOURCE 200809L
#include "n00b/config.h"
// Useful options (mainly for dev) are commented out here.
// The logic below (and into the relevent header files) sets up defaults.
//
// Memory management
// #include "core/gc.h"

// Everything includes this; the ordering here is somewhat important
// due to interdependencies, though they can always be solved via
// prototyping.
#include "util/defer.h"
#include "util/assert.h"
#include "util/close.h"
#include "core/exit.h"
#include "n00b/base.h"

// Stuff used widely enough that it's worth defining early.
typedef struct hatrack_set_st          n00b_set_t;
typedef uint64_t                       n00b_size_t;
typedef struct timespec                n00b_duration_t;
typedef struct n00b_stream_t           n00b_stream_t;
typedef struct n00b_table_t            n00b_table_t;
typedef struct n00b_type_t             n00b_type_t;
typedef struct n00b_lock_base_t        n00b_lock_base_t;
typedef struct n00b_mutex_t            n00b_mutex_t;
typedef struct hatrack_dict_t          n00b_dict_t;
typedef struct hatrack_set_st          n00b_set_t;
typedef struct n00b_string_t           n00b_string_t;
typedef struct n00b_tsi_t              n00b_tsi_t;
typedef struct n00b_lock_atomic_core_t n00b_lock_atomic_core_t;
typedef struct n00b_lock_log_t         n00b_lock_log_t;

#define n00b_min(a, b) ({ __typeof__ (a) _a = (a), _b = (b); \
                    _a < _b ? _a : _b; })
#define n00b_max(a, b) ({ __typeof__ (a) _a = (a), _b = (b); \
                    _a > _b ? _a : _b; })

#include "vendor.h"
#include "hatrack.h"
#include "hatrack/htime.h"

#define n00b_barrier() atomic_thread_fence(memory_order_seq_cst)

#ifdef N00B_DEBUG
#define n00b_cdebug(x, ...) \
    _n00b_debug(x, __VA_ARGS__ __VA_OPT__(, ) NULL)
#else
#define n00b_cdebug(x, ...) \
    _n00b_debug(x, __VA_ARGS__ __VA_OPT__(, ) NULL)
#endif

#ifdef N00B_DEBUG
#define N00B_DBG_DECL(funcname, ...) \
    _##funcname(__VA_ARGS__ __VA_OPT__(, ) char *__file, int __line)
#else
#define N00B_DBG_DECL(funcname, ...) \
    _##funcname(__VA_ARGS__)
#endif

#ifdef N00B_DEBUG
#define N00B_DBG_CALL(funcname, ...) \
    _##funcname(__VA_ARGS__ __VA_OPT__(, ) __FILE__, __LINE__)
#else
#define N00B_DBG_CALL(funcname, ...) \
    _##funcname(__VA_ARGS__)
#endif

#ifdef N00B_DEBUG
#define N00B_DBG_CALL_NESTED(funcname, ...) \
    _##funcname(__VA_ARGS__ __VA_OPT__(, ) __file, __line)
#else
#define N00B_DBG_CALL_NESTED(funcname, ...) \
    _##funcname(__VA_ARGS__)
#endif

#include "mt/futex.h"
#include "mt/gil.h"
#include "adts/dt_box.h"
#include "core/dt_kargs.h"
#include "core/dt_objects.h"
#include "core/dt_literals.h"
#include "text/dt_colors.h"
#include "text/dt_codepoints.h"
#include "adts/dt_flags.h"
#include "core/heap.h"
#include "core/kargs.h"
#include "core/dt_exceptions.h"
#include "mt/thread.h"
#include "mt/lock_common.h"
#include "mt/mutex.h"
#include "mt/condition.h"
#include "mt/rwlock.h"
#include "mt/tsi.h" // Thread-specific info.
#include "mt/lock_api.h"

// While the hatrack data structures are done in a way that's
// independent of n00b, the memory management is core to everything
// EXCEPT for the locking code, which memory management does use.
#include "n00b/datatypes.h"

#if BYTE_ORDER == LITTLE_ENDIAN
#define little_64(x)
#define little_32(x)
#define little_16(x)
#elif BYTE_ORDER == BIG_ENDIAN
#if defined(linux)
#define little_64(x) x = htole64(x)
#define little_32(x) x = htole32(x)
#define little_16(x) x = htole16(x)
#else
#define little_64(x) x = htonll(x)
#define little_32(x) x = htonl(x)
#define little_16(x) x = htons(x)
#endif
#else
#error unknown endian
#endif

#define n00b_likely(x)   __builtin_expect(!!(x), 1)
#define n00b_unlikely(x) __builtin_expect(!!(x), 0)

extern bool n00b_startup_complete;
extern bool n00b_gc_inited;

#include "core/signal.h"
#include "core/init.h"
#include "core/env.h"    // env variable API
#include "util/macros.h" // Helper macros
#include "core/kargs.h"  // Keyword arguments.
#include "util/random.h"

// Core object.
#include "core/object.h"

#include "text/color.h"
#include "adts/list.h"

// Type system API.
#include "core/typestore.h"
#include "core/type.h"
#include "text/string.h"
#include "adts/box.h"

// Extra data structure stuff.
#include "adts/tree.h"
#include "util/tree_pattern.h"
#include "adts/buffer.h"
#include "adts/tuple.h"

// Basic string handling.
#include "text/codepoint.h"
#include "text/breaks.h"
#include "text/theme.h"
#include "text/style.h"
#include "text/rich.h"
#include "text/word_breaks.h"

// Basic exception handling support.
#include "core/exception.h"

// Other data types.
#include "adts/dict.h"
#include "adts/set.h"
#include "adts/net_addr.h"
#include "adts/datetime.h"
#include "adts/duration.h"
#include "adts/bytering.h"
#include "text/table.h"
#include "util/sleep.h"

// IO primitives.
#include "io/print.h"
#include "util/hex.h"
#include "text/layout.h"
#include "text/ansi.h"
#include "text/regex.h"

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

#include "io/proc.h"
#include "io/session.h"
#include "io/http.h"

// Mixed data type API.
#include "adts/mixed.h"

// Boxes for ordinal types
#include "adts/box.h"

// A few prototypes for literal handling.
#include "core/literal.h"

// Path handling utilities.
#include "util/path.h"

// Yes we use cryptographic hashes internally for type IDing.
#include "crypto/sha.h"

#include "compiler/module.h"

// The core runtime
#include "runtime/vm.h"

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
