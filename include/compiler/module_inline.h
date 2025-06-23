#pragma once

#include "n00b.h"

static inline void
n00b_module_set_status(n00b_module_t *ctx, n00b_module_compile_status status)
{
    if (ctx->ct->status < status) {
        ctx->ct->status = status;
    }
}

static inline n00b_string_t *
n00b_module_fully_qualified(n00b_module_t *f)
{
    if (f->package) {
        return n00b_cformat("«#».«#»", f->package, f->name);
    }

    return f->name;
}

static inline bool
n00b_path_is_url(n00b_string_t *path)
{
    if (n00b_string_starts_with(path, n00b_cstring("https:"))) {
        return true;
    }

    if (n00b_string_starts_with(path, n00b_cstring("http:"))) {
        return true;
    }

    return false;
}

static inline n00b_table_t *
n00b_repr_module_parse_tree(n00b_module_t *m)
{
    return n00b_tree_format(m->ct->parse_tree,
                            n00b_repr_one_n00b_node,
                            NULL,
                            false);
}
