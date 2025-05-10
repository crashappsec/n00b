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

#define n00b_min(a, b) ({ __typeof__ (a) _a = (a), _b = (b); \
                    _a < _b ? _a : _b; })
#define n00b_max(a, b) ({ __typeof__ (a) _a = (a), _b = (b); \
                    _a > _b ? _a : _b; })

#include "vendor.h"
#include "hatrack.h"

typedef struct hatrack_dict_t n00b_dict_t;
typedef struct hatrack_set_st n00b_set_t;
typedef struct n00b_string_t  n00b_string_t;
typedef struct n00b_string_t  n00b_string_t;
typedef struct n00b_string_t  n00b_string_t;

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
