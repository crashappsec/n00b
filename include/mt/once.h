#include "n00b.h"

#define N00B_ONCE_PROTO(symbol) \
    void symbol(void)

#define N00B_ONCE_VALUE_PROTO(symbol, typename, ...) \
    typename symbol(__VA_OPT__(void *param))

// No args, no return, just initialization.
#define N00B_ONCE(symbol)                                                  \
    static n00b_futex_t __n00b_##symbol##_lock = 0;                        \
    static void         __n00b_once_func_##symbol(void);                   \
                                                                           \
    N00B_ONCE_PROTO(symbol)                                                \
    {                                                                      \
        uint32_t val = atomic_fetch_or(&__n00b_##symbol##_lock, 1);        \
        switch (val) {                                                     \
        case 0:                                                            \
            __n00b_once_func_##symbol();                                   \
            atomic_store(&__n00b_##symbol##_lock, ~0);                     \
            n00b_futex_wake(&__n00b_##symbol##_lock, true);                \
            return;                                                        \
        case 1:                                                            \
            while (atomic_read(&__n00b_##symbol##_lock) != (uint32_t)~0) { \
                n00b_futex_wait(&__n00b_##symbol##_lock, 1, ~0);           \
            }                                                              \
            return;                                                        \
        default:                                                           \
            return;                                                        \
        }                                                                  \
    }                                                                      \
    static void __n00b_once_func_##symbol(void)

#define N00B_ONCE_VALUE_PTR(symbol) \
    (&__n00b_##symbol##result)

// Register the cache location with the garbage collector.
#define N00B_ONCE_HEAP_REGISTER(symbol) \
    n00b_gc_register_root(N00B_ONCE_VALUE_PTR(symbol), 1)

// Return a value (and take an optional argument).
#define N00B_ONCE_VALUE(symbol, typename, ...)                             \
    static n00b_once_lock_t __n00b_##symbol##_lock  = {.non = 0};          \
    static void            *__n00b_##symbol##result = NULL;                \
    static typename __n00b_once_func_##symbol(__VA_ARGS__);                \
                                                                           \
    N00B_ONCE_PROTO(symbol, typename __VA_OPT__(, __VA_ARGS__))            \
    {                                                                      \
        int val = atomic_fetch_or(&__n00b_##symbol##_lock, 1);             \
        switch (val) {                                                     \
        case 0:                                                            \
            __n00b_##symbol##result = (void *)                             \
                __n00b_once_func_##symbol(__VA_OPT__(param));              \
            atomic_store(&__n00b_##symbol##_lock, ~0);                     \
            n00b_barrier();                                                \
            n00b_futex_wake(&__n00b_##symbol##_lock, true);                \
            return (typename)__n00b_##symbol##result;                      \
        case 1:                                                            \
            while (atomic_read(&__n00b_##symbol##_lock) != (uint32_t)~0) { \
                n00b_futex_wait(&__n00b_##symbol##_lock, 1, ~0);           \
            }                                                              \
            return (typename)__n00b_##symbol##result;                      \
        default:                                                           \
            return (typename)__n00b_##symbol##result;                      \
        }                                                                  \
    }                                                                      \
                                                                           \
    static typename __n00b_once_func_##symbol(__VA_ARGS__)

// SAMPLE USAGE:
/*
N00B_ONCE_VALUE(test_once, int64_t, void *x)
{
    return (int64_t)x;
}

int main(int argc, char **argv, char **envp)
{
    test_once((void *)7ull);
    test_once((void *)1ull);
    printf("%lld\n", test_once((void *)2ull));
}
//*/

// In your header, you can declare w/ N00B_ONCE_PROTO().
