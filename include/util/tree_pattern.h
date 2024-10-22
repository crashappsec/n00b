#pragma once

#include "n00b.h"

// Would consider doing a str literal for these a la:
// "->x(a(b, c), b!, .(f, b), v*, ->identifier!)"
// .(f, b) and (f, b) would be the same.

n00b_tpat_node_t *_n00b_tpat_find(void *, int64_t, ...);
n00b_tpat_node_t *_n00b_tpat_match(void *, int64_t, ...);
n00b_tpat_node_t *_n00b_tpat_opt_match(void *, int64_t, ...);
n00b_tpat_node_t *_n00b_tpat_n_m_match(void *, int64_t, int64_t, int64_t, ...);
n00b_tpat_node_t *n00b_tpat_content_find(void *, int64_t);
n00b_tpat_node_t *n00b_tpat_content_match(void *, int64_t);
n00b_tpat_node_t *_n00b_tpat_opt_content_match(void *, int64_t);
n00b_tpat_node_t *n00b_tpat_n_m_content_match(void *, int64_t, int64_t, int64_t);

bool n00b_tree_match(n00b_tree_node_t *,
                    n00b_tpat_node_t *,
                    n00b_cmp_fn,
                    n00b_list_t **matches);

#ifdef N00B_USE_INTERNAL_API
// We use the null value (error) in patterns to match any type node.
#define n00b_nt_any    (n00b_nt_error)
#define n00b_max_nodes 0x7fff

#define N00B_PAT_NO_KIDS 0ULL

// More consise aliases internally only.

#define n00b_tfind(x, y, ...)                           \
    _n00b_tpat_find(((void *)(int64_t)x),               \
                   ((int64_t)y),                       \
                   ((int64_t)N00B_PP_NARG(__VA_ARGS__)) \
                       __VA_OPT__(, ) __VA_ARGS__)

#define n00b_tfind_content(x, y) n00b_tpat_content_find((void *)(int64_t)x, y)

#define n00b_tmatch(x, y, ...)                           \
    _n00b_tpat_match(((void *)(int64_t)x),               \
                    ((int64_t)y),                       \
                    ((int64_t)N00B_PP_NARG(__VA_ARGS__)) \
                        __VA_OPT__(, ) __VA_ARGS__)

#define n00b_tcontent(x, y, ...)                                \
    n00b_tpat_content_match(((void *)(int64_t)x),               \
                           ((int64_t)N00B_PP_NARG(__VA_ARGS__)) \
                               __VA_OPT__(, ) __VA_ARGS__)

#define n00b_toptional(x, y, ...)                            \
    _n00b_tpat_opt_match(((void *)(int64_t)x),               \
                        ((int64_t)y),                       \
                        ((int64_t)N00B_PP_NARG(__VA_ARGS__)) \
                            __VA_OPT__(, ) __VA_ARGS__)

#define n00b_tcount(a, b, c, ...)                            \
    _n00b_tpat_n_m_match(((void *)(int64_t)a),               \
                        ((int64_t)b),                       \
                        ((int64_t)c),                       \
                        ((int64_t)N00B_PP_NARG(__VA_ARGS__)) \
                            __VA_OPT__(, ) __VA_ARGS__)

#define n00b_tcount_content(a, b, c, d)             \
    n00b_tpat_n_m_content_match((void *)(int64_t)a, \
                               ((int64_t)b),       \
                               ((int64_t)c),       \
                               ((int64_t)d))
#endif
