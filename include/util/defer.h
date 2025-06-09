// defer.h
// john@crashoverride.com
// © 2025 Crash Override, Inc.
// Licensed under the BSD 3-Clause license

#pragma once
#include <stdint.h>

typedef struct n00b_defer_ll_t n00b_defer_ll_t;

struct n00b_defer_ll_t {
    void   *next_target;
    int64_t guard;
};

#define N00B_DEFER_INIT ((int64_t)0xdefe11defe11defeLL)

#define n00b_enable_defer()               \
    n00b_defer_ll_t __n00b_defer_list = { \
        NULL,                             \
        N00B_DEFER_INIT,                  \
    };                                    \
    void *__n00b_defer_return_label = NULL

#define n00b_token_paste(x, y) x##y

#define n00b_defer_label(x) n00b_token_paste(__defer_block_, x)

#define n00b_defer_node(x) n00b_token_paste(__n00b_defer_node_, x)

// The unnecessary extra block after the label is to prevent
// clang-format from wrapping oddly.
//
// #pragma GCC diagnostic pop
#if defined(__GNUC__) && !defined(__APPLE__)
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#pragma GCC diagnostic ignored "-Wuninitialized"
#endif

#define n00b_defer(defer_block)                                                \
    n00b_defer_ll_t n00b_defer_node(__LINE__);                                 \
    if (n00b_defer_node(__LINE__).guard != N00B_DEFER_INIT) {                  \
        n00b_defer_node(__LINE__).guard       = N00B_DEFER_INIT;               \
        n00b_defer_node(__LINE__).next_target = __n00b_defer_list.next_target; \
        __n00b_defer_list.next_target         = &&n00b_defer_label(__LINE__);  \
    }                                                                          \
    if (false) {                                                               \
        n00b_defer_label(__LINE__) :                                           \
        {                                                                      \
            n00b_defer_node(__LINE__).guard = 0ULL;                            \
            defer_block;                                                       \
            if (!n00b_defer_node(__LINE__).next_target) {                      \
                if (!__n00b_defer_return_label) {                              \
                    goto n00b_defer_bottom_exit;                               \
                }                                                              \
                goto *(__n00b_defer_return_label);                             \
            }                                                                  \
        }                                                                      \
        goto *(n00b_defer_node(__LINE__).next_target);                         \
    }

#define n00b_defer_func_exit()                 \
    if (__n00b_defer_list.next_target) {       \
        goto *(__n00b_defer_list.next_target); \
    }

#define n00b_defer_return                                     \
    __n00b_defer_return_label = &&n00b_defer_label(__LINE__); \
    n00b_defer_func_exit();                                   \
    n00b_defer_label(__LINE__) : return

#define n00b_defer_longjmp(jump_env, jump_passed_state)       \
    __n00b_defer_return_label = &&n00b_defer_label(__LINE__); \
    n00b_defer_func_exit();                                   \
    n00b_defer_label(__LINE__) : longjmp(jump_env, jump_passed_state)

#define n00b_defer_func_end()                                              \
    n00b_defer_bottom_exit : assert("You forgot to return on some branch." \
                                    == NULL)

#if defined(N00B_USE_INTERNAL_API)
#define Return           n00b_defer_return
#define Longjmp(x, y)    n00b_defer_longjmp(x, y)
#define defer(x)         n00b_defer(x)
#define defer_on()       n00b_enable_defer()
#define defer_func_end() n00b_defer_func_end()
#endif
