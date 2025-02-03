// Simple reference-counted memory object header.  Incref it BEFORE a
// thread hands away a reference, and decref it once you're 100% sure
// it's gone.
//
// So the thread that hands out a reference is responsible for the
// incref, and the thread holding the reference does the decref.
//
// Note that this is a primitive on which we're going to build out our
// garbage collector, and should not generally be used directly.
//
// Particularly, this does NOT register pointers are 'roots' for the
// GC algorithm.
//
// Instead, this is intended to be used to manage arenas,

#include "n00b.h"

typedef struct {
    _Atomic int64_t refcount;
    alignas(N00B_FORCED_ALIGNMENT) char data[];
} refcount_alloc_t;

static inline void *
n00b_rc_alloc(size_t len)
{
    refcount_alloc_t *raw;

    len += sizeof(refcount_alloc_t);

    n00b_assert(!posix_memalign((void **)&raw, N00B_FORCED_ALIGNMENT, len));
    bzero(raw, len);
    atomic_store(&(raw->refcount), 1);

    return (void *)raw->data;
}

static inline void *
n00b_rc_ref(void *ptr)
{
    refcount_alloc_t *raw = ptr - sizeof(refcount_alloc_t);
    atomic_fetch_add(&(raw->refcount), 1);
    return ptr;
}

static inline void
n00b_rc_free(void *ptr)
{
    refcount_alloc_t *raw = ptr - sizeof(refcount_alloc_t);
    if (atomic_fetch_add(&(raw->refcount), -1) == 0) {
        free(raw);
    }
}

typedef void (*cleanup_fn)(void *);

static inline void
n00b_rc_free_and_cleanup(void *ptr, cleanup_fn callback)
{
    refcount_alloc_t *raw = ptr - sizeof(refcount_alloc_t);
    if (atomic_fetch_add(&(raw->refcount), -1) == 0) {
        callback(raw);
        free(raw);
    }
}

static inline void *
n00b_zalloc(size_t len)
{
    return n00b_rc_alloc(len);
}

#define n00b_zalloc_flex(fixed, variable, num_variable) \
    (void *)n00b_rc_alloc((sizeof(fixed)) + (sizeof(variable)) * (num_variable))

static inline void
n00b_zfree(void *ptr)
{
    n00b_rc_free(ptr);
}
