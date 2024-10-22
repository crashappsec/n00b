#pragma once

#include "n00b.h"

typedef int (*n00b_sort_fn)(const void *, const void *);

extern void       *n00b_list_get(n00b_list_t *, int64_t, bool *);
extern void        n00b_list_append(n00b_list_t *list, void *item);
extern void        n00b_list_add_if_unique(n00b_list_t *list,
                                          void       *item,
                                          bool (*fn)(void *, void *));
extern bool        n00b_list_remove(n00b_list_t *list, int64_t);
extern void       *n00b_list_pop(n00b_list_t *list);
extern void        n00b_list_plus_eq(n00b_list_t *, n00b_list_t *);
extern n00b_list_t *n00b_list_plus(n00b_list_t *, n00b_list_t *);
extern bool        n00b_list_set(n00b_list_t *, int64_t, void *);
extern n00b_list_t *n00b_list(n00b_type_t *);
extern int64_t     n00b_list_len(const n00b_list_t *);
extern n00b_list_t *n00b_list_get_slice(n00b_list_t *, int64_t, int64_t);
extern void        n00b_list_set_slice(n00b_list_t *,
                                      int64_t,
                                      int64_t,
                                      n00b_list_t *);
extern bool        n00b_list_contains(n00b_list_t *, n00b_obj_t);
extern n00b_list_t *n00b_list_copy(n00b_list_t *);
extern n00b_list_t *n00b_list_shallow_copy(n00b_list_t *);
extern void        n00b_list_sort(n00b_list_t *, n00b_sort_fn);
extern void        n00b_list_resize(n00b_list_t *, size_t);
extern void       *n00b_list_view(n00b_list_t *, uint64_t *);
extern void        n00b_list_reverse(n00b_list_t *);
