#pragma once

// Base includes from the system and any dependencies should go here.

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <stdbool.h>
#include <stdalign.h>
#include <limits.h>
#include <signal.h>
#include <termios.h>
#include <assert.h>
#include <pthread.h>
#include <stdarg.h>
#include <math.h>
#include <setjmp.h>
#include <netdb.h>
#include <dlfcn.h>
#include <pwd.h>
#include <dirent.h>
#include <ctype.h>
#include <poll.h>

#include <sys/select.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>

#if defined(__linux__)
#include <sys/random.h>
#include <threads.h>
#include <endian.h>
#include <sys/time.h>
#include <sys/resource.h>
#endif

#if defined(__MACH__)
#include <machine/endian.h>
#include <libproc.h>
#include <util.h>
#endif

#ifdef HAVE_MUSL
#include <bits/limits.h>
#endif

#ifdef HAVE_PTY_H
#include <pty.h>
#endif

// Some type definitions so simple, they don't get their own file.
typedef uint64_t                       n00b_ntype_t;
typedef _Atomic uint32_t               n00b_futex_t;
typedef uint64_t                       n00b_size_t;
typedef struct timespec                n00b_duration_t;
typedef struct n00b_stream_t           n00b_stream_t;
typedef struct n00b_table_t            n00b_table_t;
typedef struct n00b_lock_base_t        n00b_lock_base_t;
typedef struct n00b_mutex_t            n00b_mutex_t;
typedef struct n00b_dict_t             n00b_dict_t;
typedef struct n00b_dict_t             n00b_set_t;
typedef struct n00b_string_t           n00b_string_t;
typedef struct n00b_tsi_t              n00b_tsi_t;
typedef struct n00b_lock_atomic_core_t n00b_lock_atomic_core_t;
typedef struct n00b_lock_log_t         n00b_lock_log_t;

#define n00b_min(a, b) ({ __typeof__ (a) _a = (a), _b = (b); \
                    _a < _b ? _a : _b; })
#define n00b_max(a, b) ({ __typeof__ (a) _a = (a), _b = (b); \
                    _a > _b ? _a : _b; })

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
