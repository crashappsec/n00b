#pragma once

#include "n00b.h"

typedef struct {
    n00b_rwlock_t lock;
    int64_t     **data;
    uint64_t      noscan;
    int32_t       append_ix          : 30;
    // The actual length if treated properly. We should be
    // careful about it.
    int32_t       length             : 30; // The allocated length.
    uint32_t      enforce_uniqueness : 1;
} n00b_list_t;

typedef struct hatstack_t n00b_stack_t;

typedef int (*n00b_sort_fn)(const void *, const void *);

extern void         n00b_private_list_resize(n00b_list_t *, size_t);
extern void         n00b_list_resize(n00b_list_t *, size_t);
extern bool         n00b_private_list_set(n00b_list_t *, int64_t, void *);
extern bool         n00b_list_set(n00b_list_t *, int64_t, void *);
extern void         n00b_private_list_append(n00b_list_t *list, void *item);
extern void         n00b_list_append(n00b_list_t *list, void *item);
extern void        *n00b_private_list_get(n00b_list_t *, int64_t, bool *);
extern void        *n00b_list_get(n00b_list_t *, int64_t, bool *);
extern void        *n00b_private_list_pop(n00b_list_t *list);
extern void        *n00b_list_pop(n00b_list_t *list);
extern void         n00b_private_list_plus_eq(n00b_list_t *, n00b_list_t *);
extern void         n00b_list_plus_eq(n00b_list_t *, n00b_list_t *);
extern n00b_list_t *n00b_private_list_plus(n00b_list_t *, n00b_list_t *);
extern n00b_list_t *n00b_list_plus(n00b_list_t *, n00b_list_t *);
extern int64_t      n00b_list_len(const n00b_list_t *);
extern n00b_list_t *n00b_private_list_copy(n00b_list_t *);
extern n00b_list_t *n00b_list_copy(n00b_list_t *);
extern n00b_list_t *n00b_private_list_shallow_copy(n00b_list_t *);
extern n00b_list_t *n00b_list_shallow_copy(n00b_list_t *);
extern n00b_list_t *n00b_private_list_get_slice(n00b_list_t *,
                                                int64_t,
                                                int64_t);
extern void         n00b_private_list_set_slice(n00b_list_t *,
                                                int64_t,
                                                int64_t,
                                                n00b_list_t *,
                                                bool);
extern void         n00b_list_set_slice(n00b_list_t *,
                                        int64_t,
                                        int64_t,
                                        n00b_list_t *);

extern n00b_list_t *n00b_list_get_slice(n00b_list_t *, int64_t, int64_t);
extern bool         n00b_private_list_remove(n00b_list_t *list, int64_t);
extern bool         n00b_list_remove(n00b_list_t *list, int64_t);
extern bool         n00b_private_list_remove_item(n00b_list_t *, void *);
extern bool         n00b_list_remove_item(n00b_list_t *, void *);
extern void        *n00b_private_list_dequeue(n00b_list_t *);
extern void        *n00b_list_dequeue(n00b_list_t *);
extern int64_t      n00b_list_find(n00b_list_t *list, void *item);
extern int64_t      n00b_private_list_find(n00b_list_t *list, void *item);
extern void        *n00b_list_view(n00b_list_t *, uint64_t *);
extern void         n00b_private_list_reverse(n00b_list_t *);
extern void         n00b_list_reverse(n00b_list_t *);
extern n00b_list_t *_n00b_to_list(n00b_ntype_t, int, ...);
extern int          n00b_lexical_sort(const n00b_string_t **,
                                      const n00b_string_t **);
extern n00b_list_t *n00b_internal_list(void);
