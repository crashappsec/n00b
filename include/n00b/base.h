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
