#define N00B_USE_INTERNAL_API
#include "n00b.h"

// This is the core type store interface.

void
n00b_universe_init(n00b_type_universe_t *u)
{
    // Start w/ 128 items.
    u->dict = n00b_new_unmanaged_dict();
    atomic_store(&u->next_typeid, 1LLU << 63);
}

n00b_type_t *
n00b_universe_get(n00b_type_universe_t *u, n00b_type_hash_t typeid)
{
    return n00b_dict_get(u->dict, typeid, NULL);
}

bool
n00b_universe_add(n00b_type_universe_t *u, n00b_type_t *t)
{
    if (!t->typeid) {
        return false;
    }

    return n00b_dict_add(u->dict, t->typeid, t);
}

bool
n00b_universe_put(n00b_type_universe_t *u, n00b_type_t *t)
{
    return (n00b_dict_put(u->dict, t->typeid, t));
}

n00b_type_t *
n00b_universe_attempt_to_add(n00b_type_universe_t *u, n00b_type_t *t)
{
    n00b_assert(t->typeid);

    if (n00b_universe_add(u, t)) {
        return t;
    }

    return n00b_universe_get(u, t->typeid);
}

void
n00b_universe_forward(n00b_type_universe_t *u, n00b_type_t *t1, n00b_type_t *t2)
{
    n00b_assert(t1->typeid);
    n00b_assert(t2->typeid);
    n00b_assert(!t2->fw);

    t1->fw = t2->typeid;
    n00b_assert(n00b_thread_self());

    n00b_dict_put(u->dict, t1->typeid, t2);
}

// Not migrated.
n00b_table_t *
n00b_format_global_type_environment(n00b_type_universe_t *u)
{
    n00b_table_t *tbl   = n00b_table(columns : 3, style : N00B_TABLE_ORNATE);
    n00b_dict_t  *memos = n00b_dict(n00b_type_ref(), n00b_type_string());
    int64_t       n     = 0;
    n00b_list_t  *view  = n00b_dict_keys(u->dict);
    int           len   = n00b_list_len(view);

    n00b_table_add_cell(tbl, n00b_cstring("Id"));
    n00b_table_add_cell(tbl, n00b_cstring("Value"));
    n00b_table_add_cell(tbl, n00b_cstring("Base Type"));

    for (int i = 0; i < len; i++) {
        n00b_type_t *t = n00b_list_get(view, i, NULL);

        // Skip forwarded nodes.
        if (t->fw) {
            continue;
        }

        n00b_string_t *base_name;

        switch (n00b_type_get_kind(t)) {
        case N00B_DT_KIND_nil:
            base_name = n00b_cstring("nil");
            break;
        case N00B_DT_KIND_primitive:
            base_name = n00b_cstring("primitive");
            break;
        case N00B_DT_KIND_internal: // Internal primitives.
            base_name = n00b_cstring("internal");
            break;
        case N00B_DT_KIND_type_var:
            base_name = n00b_cstring("var");
            break;
        case N00B_DT_KIND_list:
            base_name = n00b_cstring("list");
            break;
        case N00B_DT_KIND_dict:
            base_name = n00b_cstring("dict");
            break;
        case N00B_DT_KIND_tuple:
            base_name = n00b_cstring("tuple");
            break;
        case N00B_DT_KIND_func:
            base_name = n00b_cstring("func");
            break;
        case N00B_DT_KIND_maybe:
            base_name = n00b_cstring("maybe");
            break;
        case N00B_DT_KIND_object:
            base_name = n00b_cstring("object");
            break;
        case N00B_DT_KIND_oneof:
            base_name = n00b_cstring("oneof");
            break;
        case N00B_DT_KIND_box:
            base_name = n00b_cstring("box");
            break;
        default:
            n00b_unreachable();
        }

        n00b_table_add_cell(tbl, n00b_cformat("«#:x»", t->typeid));
        n00b_table_add_cell(tbl, n00b_internal_type_repr(t, memos, &n));
        n00b_table_add_cell(tbl, base_name);
    }

    return tbl;
}
