#include <sys/resource.h>
#include <unistd.h>
#include <stdint.h>

#define limit_fn_gen(name, cval)                   \
    int n00b_current_##name##_limit(void)          \
    {                                              \
        struct rlimit limit;                       \
                                                   \
        getrlimit(cval, &limit);                   \
        return limit.rlim_cur;                     \
    }                                              \
                                                   \
    int n00b_max_##name##_limit(void)              \
    {                                              \
        struct rlimit limit;                       \
                                                   \
        getrlimit(cval, &limit);                   \
        return limit.rlim_max;                     \
    }                                              \
                                                   \
    bool n00b_set_##name##_limit(int desired)      \
    {                                              \
        struct rlimit limit;                       \
                                                   \
        getrlimit(cval, &limit);                   \
        if (((int64_t)limit.rlim_max) < desired) { \
            if (geteuid()) {                       \
                return false;                      \
            }                                      \
            else {                                 \
                limit.rlim_max = desired;          \
            }                                      \
        }                                          \
        if (desired < 0) {                         \
            limit.rlim_cur = limit.rlim_max;       \
        }                                          \
        else {                                     \
            limit.rlim_cur = desired;              \
        }                                          \
                                                   \
        return !setrlimit(cval, &limit);           \
    }

limit_fn_gen(fd, RLIMIT_NOFILE);
limit_fn_gen(cpu, RLIMIT_CPU);
limit_fn_gen(core_size, RLIMIT_CORE);
limit_fn_gen(file_size, RLIMIT_FSIZE);
limit_fn_gen(num_procs, RLIMIT_NPROC);
limit_fn_gen(memlock, RLIMIT_MEMLOCK);

#if defined(__linux__)
limit_fn_gen(nice, RLIMIT_NICE);
#endif
