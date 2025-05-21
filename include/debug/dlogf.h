#pragma once

#if defined(N00B_DEBUG)
extern char   *_n00b_dstrf(char *fmt, int64_t num_params, ...);
extern void    n00b_dlog(char *, int, int, char *,  int, char *);
extern int64_t n00b_dlog_get_topic_policy(char *);
extern bool n00b_dlog_set_topic_policy(char *, int64_t);

#else

#undef N00B_DLOG_COMPILE_OUT_THRESHOLD
#define N00B_DLOG_COMPILE_OUT_THRESHOLD 4
#undef N00B_DLOG_DEFAULT_DISABLE_LEVEL
#define N00B_DLOG_DEFAULT_DISABLE_LEVEL 4

#define n00b_dlog_get_topic_policy(x) 0
#define n00b_dlog_set_topic_policy(a, b) false
#define _n00b_dstrf(...)
#define n00b_dlog(...)

#endif

#define n00b_dstrf(fmt, ...)     _n00b_dstrf(fmt, N00B_PP_NARG(__VA_ARGS__) __VA_OPT__(, __VA_ARGS__))

#if !defined(N00B_DLOG_COMPILE_OUT_THRESHOLD)
#define N00B_DLOG_COMPILE_OUT_THRESHOLD 2
#endif

#if !defined(N00B_DLOG_DEFAULT_DISABLE_LEVEL)
#define N00B_DLOG_DEFAULT_DISABLE_LEVEL 2
#endif

#if N00B_DLOG_DEFAULT_DISABLE_LEVEL > N00B_DLOG_COMPILE_OUT_THRESHOLD
#warning "N00B_DLOG_COMPILE_OUT_THRESHOLD <= N00B_DLOG_DEFAULT_DISABLE_LEVEL"
#endif

#define N00B_DLOG_LOCK_IX 0
#define N00B_DLOG_ALLOC_IX 1
#define N00B_DLOG_GC_IX 2
#define N00B_DLOG_GIL_IX 3
#define N00B_DLOG_THREAD_IX 4
#define N00B_DLOG_IO_IX 5


#if defined(N00B_DEBUG)
#if !defined(N00B_DLOG_LOCK_DEFAULT_LAST_LEVEL)
#define N00B_DLOG_LOCK_DEFAULT_LAST_LEVEL 2
#endif

#if N00B_DLOG_LOCK_DEFAULT_LAST_LEVEL > N00B_DLOG_COMPILE_OUT_THRESHOLD
#define N00B_DLOG_LOCK_COMPILE_OUT_THRESHOLD N00B_DLOG_LOCK_DEFAULT_LAST_LEVEL
#else
#define N00B_DLOG_LOCK_COMPILE_OUT_THRESHOLD N00B_DLOG_COMPILE_OUT_THRESHOLD
#endif
#else // N00B_DEBUG
#define N00B_DLOG_LOCK_COMPILE_OUT_THRESHOLD 4 // Don't compile any dlogs
#endif // N00B_DEBUG

#if N00B_DLOG_LOCK_COMPILE_OUT_THRESHOLD <= 3
#define n00b_dlog_lock3(...)
#else
#define n00b_dlog_lock3(...)\
    n00b_dlog("lock", N00B_DLOG_LOCK_IX, 3, __FILE__, __LINE__, \
              n00b_dstrf(__VA_ARGS__))

#endif


#if N00B_DLOG_LOCK_COMPILE_OUT_THRESHOLD <= 2
#define n00b_dlog_lock2(...)
#else
#define n00b_dlog_lock2(...)\
    n00b_dlog("lock", N00B_DLOG_LOCK_IX, 2, __FILE__, __LINE__, \
              n00b_dstrf(__VA_ARGS__))

#endif


#if N00B_DLOG_LOCK_COMPILE_OUT_THRESHOLD <= 1
#define n00b_dlog_lock1(...)
#else
#define n00b_dlog_lock1(...)\
    n00b_dlog("lock", N00B_DLOG_LOCK_IX, 1, __FILE__, __LINE__, \
              n00b_dstrf(__VA_ARGS__))

#endif


#if N00B_DLOG_LOCK_COMPILE_OUT_THRESHOLD <= 0
#define n00b_dlog_lock(...)
#else
#define n00b_dlog_lock(...)\
    n00b_dlog("lock", N00B_DLOG_LOCK_IX, 0, __FILE__, __LINE__, \
              n00b_dstrf(__VA_ARGS__))

#endif

#if N00B_DLOG_LOCK_COMPILE_OUT_THRESHOLD > 0
#define N00B_DLOG_LOCK_ON
#endif
#define N00B_DLOG_LOCK_LEVEL (N00B_DLOG_LOCK_COMPILE_OUT_THRESHOLD - 1)

#if defined(N00B_DEBUG)
#if !defined(N00B_DLOG_ALLOC_DEFAULT_LAST_LEVEL)
#define N00B_DLOG_ALLOC_DEFAULT_LAST_LEVEL 2
#endif

#if N00B_DLOG_ALLOC_DEFAULT_LAST_LEVEL > N00B_DLOG_COMPILE_OUT_THRESHOLD
#define N00B_DLOG_ALLOC_COMPILE_OUT_THRESHOLD N00B_DLOG_ALLOC_DEFAULT_LAST_LEVEL
#else
#define N00B_DLOG_ALLOC_COMPILE_OUT_THRESHOLD N00B_DLOG_COMPILE_OUT_THRESHOLD
#endif
#else // N00B_DEBUG
#define N00B_DLOG_ALLOC_COMPILE_OUT_THRESHOLD 4 // Don't compile any dlogs
#endif // N00B_DEBUG

#if N00B_DLOG_ALLOC_COMPILE_OUT_THRESHOLD <= 3
#define n00b_dlog_alloc3(...)
#else
#define n00b_dlog_alloc3(...)\
    n00b_dlog("alloc", N00B_DLOG_ALLOC_IX, 3, __FILE__, __LINE__, \
              n00b_dstrf(__VA_ARGS__))

#endif


#if N00B_DLOG_ALLOC_COMPILE_OUT_THRESHOLD <= 2
#define n00b_dlog_alloc2(...)
#else
#define n00b_dlog_alloc2(...)\
    n00b_dlog("alloc", N00B_DLOG_ALLOC_IX, 2, __FILE__, __LINE__, \
              n00b_dstrf(__VA_ARGS__))

#endif


#if N00B_DLOG_ALLOC_COMPILE_OUT_THRESHOLD <= 1
#define n00b_dlog_alloc1(...)
#else
#define n00b_dlog_alloc1(...)\
    n00b_dlog("alloc", N00B_DLOG_ALLOC_IX, 1, __FILE__, __LINE__, \
              n00b_dstrf(__VA_ARGS__))

#endif


#if N00B_DLOG_ALLOC_COMPILE_OUT_THRESHOLD <= 0
#define n00b_dlog_alloc(...)
#else
#define n00b_dlog_alloc(...)\
    n00b_dlog("alloc", N00B_DLOG_ALLOC_IX, 0, __FILE__, __LINE__, \
              n00b_dstrf(__VA_ARGS__))

#endif

#if N00B_DLOG_ALLOC_COMPILE_OUT_THRESHOLD > 0
#define N00B_DLOG_ALLOC_ON
#endif
#define N00B_DLOG_ALLOC_LEVEL (N00B_DLOG_ALLOC_COMPILE_OUT_THRESHOLD - 1)


#if defined(N00B_DEBUG)
#if !defined(N00B_DLOG_GC_DEFAULT_LAST_LEVEL)
#define N00B_DLOG_GC_DEFAULT_LAST_LEVEL 2
#endif

#if N00B_DLOG_GC_DEFAULT_LAST_LEVEL > N00B_DLOG_COMPILE_OUT_THRESHOLD
#define N00B_DLOG_GC_COMPILE_OUT_THRESHOLD N00B_DLOG_GC_DEFAULT_LAST_LEVEL
#else
#define N00B_DLOG_GC_COMPILE_OUT_THRESHOLD N00B_DLOG_COMPILE_OUT_THRESHOLD
#endif
#else // N00B_DEBUG
#define N00B_DLOG_GC_COMPILE_OUT_THRESHOLD 4 // Don't compile any dlogs
#endif // N00B_DEBUG

#if N00B_DLOG_GC_COMPILE_OUT_THRESHOLD <= 3
#define n00b_dlog_gc3(...)
#else
#define n00b_dlog_gc3(...)\
    n00b_dlog("gc", N00B_DLOG_GC_IX, 3, __FILE__, __LINE__, \
              n00b_dstrf(__VA_ARGS__))

#endif


#if N00B_DLOG_GC_COMPILE_OUT_THRESHOLD <= 2
#define n00b_dlog_gc2(...)
#else
#define n00b_dlog_gc2(...)\
    n00b_dlog("gc", N00B_DLOG_GC_IX, 2, __FILE__, __LINE__, \
              n00b_dstrf(__VA_ARGS__))

#endif


#if N00B_DLOG_GC_COMPILE_OUT_THRESHOLD <= 1
#define n00b_dlog_gc1(...)
#else
#define n00b_dlog_gc1(...)\
    n00b_dlog("gc", N00B_DLOG_GC_IX, 1, __FILE__, __LINE__, \
              n00b_dstrf(__VA_ARGS__))

#endif


#if N00B_DLOG_GC_COMPILE_OUT_THRESHOLD <= 0
#define n00b_dlog_gc(...)
#else
#define n00b_dlog_gc(...)\
    n00b_dlog("gc", N00B_DLOG_GC_IX, 0, __FILE__, __LINE__, \
              n00b_dstrf(__VA_ARGS__))

#endif

#if N00B_DLOG_GC_COMPILE_OUT_THRESHOLD > 0
#define N00B_DLOG_GC_ON
#endif
#define N00B_DLOG_GC_LEVEL (N00B_DLOG_GC_COMPILE_OUT_THRESHOLD - 1)


#if defined(N00B_DEBUG)
#if !defined(N00B_DLOG_GIL_DEFAULT_LAST_LEVEL)
#define N00B_DLOG_GIL_DEFAULT_LAST_LEVEL 2
#endif

#if N00B_DLOG_GIL_DEFAULT_LAST_LEVEL > N00B_DLOG_COMPILE_OUT_THRESHOLD
#define N00B_DLOG_GIL_COMPILE_OUT_THRESHOLD N00B_DLOG_GIL_DEFAULT_LAST_LEVEL
#else
#define N00B_DLOG_GIL_COMPILE_OUT_THRESHOLD N00B_DLOG_COMPILE_OUT_THRESHOLD
#endif
#else // N00B_DEBUG
#define N00B_DLOG_GIL_COMPILE_OUT_THRESHOLD 4 // Don't compile any dlogs
#endif // N00B_DEBUG

#if N00B_DLOG_GIL_COMPILE_OUT_THRESHOLD <= 3
#define n00b_dlog_gil3(...)
#else
#define n00b_dlog_gil3(...)\
    n00b_dlog("gil", N00B_DLOG_GIL_IX, 3, __FILE__, __LINE__, \
              n00b_dstrf(__VA_ARGS__))

#endif


#if N00B_DLOG_GIL_COMPILE_OUT_THRESHOLD <= 2
#define n00b_dlog_gil2(...)
#else
#define n00b_dlog_gil2(...)\
    n00b_dlog("gil", N00B_DLOG_GIL_IX, 2, __FILE__, __LINE__, \
              n00b_dstrf(__VA_ARGS__))

#endif


#if N00B_DLOG_GIL_COMPILE_OUT_THRESHOLD <= 1
#define n00b_dlog_gil1(...)
#else
#define n00b_dlog_gil1(...)\
    n00b_dlog("gil", N00B_DLOG_GIL_IX, 1, __FILE__, __LINE__, \
              n00b_dstrf(__VA_ARGS__))

#endif


#if N00B_DLOG_GIL_COMPILE_OUT_THRESHOLD <= 0
#define n00b_dlog_gil(...)
#else
#define n00b_dlog_gil(...)\
    n00b_dlog("gil", N00B_DLOG_GIL_IX, 0, __FILE__, __LINE__, \
              n00b_dstrf(__VA_ARGS__))

#endif

#if N00B_DLOG_GIL_COMPILE_OUT_THRESHOLD > 0
#define N00B_DLOG_GIL_ON
#endif
#define N00B_DLOG_GIL_LEVEL (N00B_DLOG_GIL_COMPILE_OUT_THRESHOLD - 1)


#if defined(N00B_DEBUG)
#if !defined(N00B_DLOG_THREAD_DEFAULT_LAST_LEVEL)
#define N00B_DLOG_THREAD_DEFAULT_LAST_LEVEL 2
#endif

#if N00B_DLOG_THREAD_DEFAULT_LAST_LEVEL > N00B_DLOG_COMPILE_OUT_THRESHOLD
#define N00B_DLOG_THREAD_COMPILE_OUT_THRESHOLD N00B_DLOG_THREAD_DEFAULT_LAST_LEVEL
#else
#define N00B_DLOG_THREAD_COMPILE_OUT_THRESHOLD N00B_DLOG_COMPILE_OUT_THRESHOLD
#endif
#else // N00B_DEBUG
#define N00B_DLOG_THREAD_COMPILE_OUT_THRESHOLD 4 // Don't compile any dlogs
#endif // N00B_DEBUG

#if N00B_DLOG_THREAD_COMPILE_OUT_THRESHOLD <= 3
#define n00b_dlog_thread3(...)
#else
#define n00b_dlog_thread3(...)\
    n00b_dlog("thread", N00B_DLOG_THREAD_IX, 3, __FILE__, __LINE__, \
              n00b_dstrf(__VA_ARGS__))

#endif


#if N00B_DLOG_THREAD_COMPILE_OUT_THRESHOLD <= 2
#define n00b_dlog_thread2(...)
#else
#define n00b_dlog_thread2(...)\
    n00b_dlog("thread", N00B_DLOG_THREAD_IX, 2, __FILE__, __LINE__, \
              n00b_dstrf(__VA_ARGS__))

#endif


#if N00B_DLOG_THREAD_COMPILE_OUT_THRESHOLD <= 1
#define n00b_dlog_thread1(...)
#else
#define n00b_dlog_thread1(...)\
    n00b_dlog("thread", N00B_DLOG_THREAD_IX, 1, __FILE__, __LINE__, \
              n00b_dstrf(__VA_ARGS__))

#endif


#if N00B_DLOG_THREAD_COMPILE_OUT_THRESHOLD <= 0
#define n00b_dlog_thread(...)
#else
#define n00b_dlog_thread(...)\
    n00b_dlog("thread", N00B_DLOG_THREAD_IX, 0, __FILE__, __LINE__, \
              n00b_dstrf(__VA_ARGS__))

#endif

#if N00B_DLOG_THREAD_COMPILE_OUT_THRESHOLD > 0
#define N00B_DLOG_THREAD_ON
#endif
#define N00B_DLOG_THREAD_LEVEL (N00B_DLOG_THREAD_COMPILE_OUT_THRESHOLD - 1)


#if defined(N00B_DEBUG)
#if !defined(N00B_DLOG_IO_DEFAULT_LAST_LEVEL)
#define N00B_DLOG_IO_DEFAULT_LAST_LEVEL 2
#endif

#if N00B_DLOG_IO_DEFAULT_LAST_LEVEL > N00B_DLOG_COMPILE_OUT_THRESHOLD
#define N00B_DLOG_IO_COMPILE_OUT_THRESHOLD N00B_DLOG_IO_DEFAULT_LAST_LEVEL
#else
#define N00B_DLOG_IO_COMPILE_OUT_THRESHOLD N00B_DLOG_COMPILE_OUT_THRESHOLD
#endif
#else // N00B_DEBUG
#define N00B_DLOG_IO_COMPILE_OUT_THRESHOLD 4 // Don't compile any dlogs
#endif // N00B_DEBUG

#if N00B_DLOG_IO_COMPILE_OUT_THRESHOLD <= 3
#define n00b_dlog_io3(...)
#else
#define n00b_dlog_io3(...)\
    n00b_dlog("io", N00B_DLOG_IO_IX, 3, __FILE__, __LINE__, \
              n00b_dstrf(__VA_ARGS__))

#endif


#if N00B_DLOG_IO_COMPILE_OUT_THRESHOLD <= 2
#define n00b_dlog_io2(...)
#else
#define n00b_dlog_io2(...)\
    n00b_dlog("io", N00B_DLOG_IO_IX, 2, __FILE__, __LINE__, \
              n00b_dstrf(__VA_ARGS__))

#endif


#if N00B_DLOG_IO_COMPILE_OUT_THRESHOLD <= 1
#define n00b_dlog_io1(...)
#else
#define n00b_dlog_io1(...)\
    n00b_dlog("io", N00B_DLOG_IO_IX, 1, __FILE__, __LINE__, \
              n00b_dstrf(__VA_ARGS__))

#endif


#if N00B_DLOG_IO_COMPILE_OUT_THRESHOLD <= 0
#define n00b_dlog_io(...)
#else
#define n00b_dlog_io(...)\
    n00b_dlog("io", N00B_DLOG_IO_IX, 0, __FILE__, __LINE__, \
              n00b_dstrf(__VA_ARGS__))

#endif

#if N00B_DLOG_IO_COMPILE_OUT_THRESHOLD > 0
#define N00B_DLOG_IO_ON
#endif
#define N00B_DLOG_IO_LEVEL (N00B_DLOG_IO_COMPILE_OUT_THRESHOLD - 1)



#if defined(N00B_DEBUG)
#define N00B_DLOG_GENERATED_INITS\
    {\
    {.topic = "lock", .value =  N00B_DLOG_LOCK_DEFAULT_LAST_LEVEL - 1ULL},\
    {.topic = "alloc", .value =  N00B_DLOG_ALLOC_DEFAULT_LAST_LEVEL - 1ULL},\
    {.topic = "gc", .value =  N00B_DLOG_GC_DEFAULT_LAST_LEVEL - 1ULL},\
    {.topic = "gil", .value =  N00B_DLOG_GIL_DEFAULT_LAST_LEVEL - 1ULL},\
    {.topic = "thread", .value =  N00B_DLOG_THREAD_DEFAULT_LAST_LEVEL - 1ULL},\
    {.topic = "io", .value =  N00B_DLOG_IO_DEFAULT_LAST_LEVEL - 1ULL},\
};

#define N00B_DLOG_NUM_TOPICS 6
#endif
