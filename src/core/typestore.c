#define N00B_USE_INTERNAL_API
#include "n00b.h"

// This is the core type store interface.

void
n00b_universe_init(n00b_type_universe_t *u)
{
    // Start w/ 128 items.
    u->dict = n00b_new_unmanaged_dict(HATRACK_DICT_KEY_TYPE_OBJ_INT,
                                      true,
                                      true);

    atomic_store(&u->next_typeid, 1LLU << 63);
}

// The most significant bits stay 0.
static inline void
init_hv(hatrack_hash_t *hv, n00b_type_hash_t id)
{
#ifdef NO___INT128_T
    hv->w1 = 0ULL;
    hv->w2 = id;
#else
    *hv = id;
#endif
}

n00b_type_t *
n00b_universe_get(n00b_type_universe_t *u, n00b_type_hash_t typeid)
{
    hatrack_hash_t hv;

    init_hv(&hv, typeid);
    assert(typeid);

    return crown_get_mmm(&u->dict->crown_instance,
                         mmm_thread_acquire(),
                         hv,
                         NULL);
}

bool
n00b_universe_add(n00b_type_universe_t *u, n00b_type_t *t)
{
    hatrack_hash_t hv;

    init_hv(&hv, t->typeid);
    assert(t->typeid);

    if (!n00b_mmm_thread_ref) {
        n00b_gc_register_root(&n00b_mmm_thread_ref, 1);
        n00b_mmm_thread_ref = mmm_thread_acquire();
    }

    return crown_add_mmm(&u->dict->crown_instance,
                         n00b_mmm_thread_ref,
                         hv,
                         t);
}

bool
n00b_universe_put(n00b_type_universe_t *u, n00b_type_t *t)
{
    hatrack_hash_t hv;

    init_hv(&hv, t->typeid);

    if (n00b_universe_add(u, t)) {
        return true;
    }

    crown_put_mmm(&u->dict->crown_instance,
                  n00b_mmm_thread_ref,
                  hv,
                  t,
                  NULL);

    return false;
}

n00b_type_t *
n00b_universe_attempt_to_add(n00b_type_universe_t *u, n00b_type_t *t)
{
    assert(t->typeid);

    if (n00b_universe_add(u, t)) {
        return t;
    }

    return n00b_universe_get(u, t->typeid);
}

void
n00b_universe_forward(n00b_type_universe_t *u, n00b_type_t *t1, n00b_type_t *t2)
{
    hatrack_hash_t hv;

    assert(t1->typeid);
    assert(t2->typeid);
    assert(!t2->fw);

    t1->fw = t2->typeid;
    init_hv(&hv, t1->typeid);
    if (!n00b_mmm_thread_ref) {
        n00b_mmm_thread_ref = mmm_thread_acquire();
    }
    crown_put_mmm(&u->dict->crown_instance,
                  n00b_mmm_thread_ref,
                  hv,
                  t2,
                  NULL);
}

n00b_grid_t *
n00b_format_global_type_environment(n00b_type_universe_t *u)
{
    uint64_t        len;
    hatrack_view_t *view;
    n00b_grid_t    *grid  = n00b_new(n00b_type_grid(),
                                 n00b_kw("start_cols",
                                         n00b_ka(3),
                                         "header_rows",
                                         n00b_ka(1),
                                         "stripe",
                                         n00b_ka(true)));
    n00b_list_t    *row   = n00b_new_table_row();
    n00b_dict_t    *memos = n00b_dict(n00b_type_ref(),
                                   n00b_type_utf8());
    int64_t         n     = 0;

    view = crown_view(&u->dict->crown_instance, &len, true);

    n00b_list_append(row, n00b_new_utf8("Id"));
    n00b_list_append(row, n00b_new_utf8("Value"));
    n00b_list_append(row, n00b_new_utf8("Base Type"));
    n00b_grid_add_row(grid, row);

    for (uint64_t i = 0; i < len; i++) {
        n00b_type_t *t = (n00b_type_t *)view[i].item;

        // Skip forwarded nodes.
        if (t->fw) {
            continue;
        }

        n00b_utf8_t *base_name;

        switch (n00b_type_get_kind(t)) {
        case N00B_DT_KIND_nil:
            base_name = n00b_new_utf8("nil");
            break;
        case N00B_DT_KIND_primitive:
            base_name = n00b_new_utf8("primitive");
            break;
        case N00B_DT_KIND_internal: // Internal primitives.
            base_name = n00b_new_utf8("internal");
            break;
        case N00B_DT_KIND_type_var:
            base_name = n00b_new_utf8("var");
            break;
        case N00B_DT_KIND_list:
            base_name = n00b_new_utf8("list");
            break;
        case N00B_DT_KIND_dict:
            base_name = n00b_new_utf8("dict");
            break;
        case N00B_DT_KIND_tuple:
            base_name = n00b_new_utf8("tuple");
            break;
        case N00B_DT_KIND_func:
            base_name = n00b_new_utf8("func");
            break;
        case N00B_DT_KIND_maybe:
            base_name = n00b_new_utf8("maybe");
            break;
        case N00B_DT_KIND_object:
            base_name = n00b_new_utf8("object");
            break;
        case N00B_DT_KIND_oneof:
            base_name = n00b_new_utf8("oneof");
            break;
        case N00B_DT_KIND_box:
            base_name = n00b_new_utf8("box");
            break;
        default:
            n00b_unreachable();
        }

        row = n00b_new_table_row();
        n00b_list_append(row, n00b_cstr_format("{:x}", n00b_box_i64(t->typeid)));
        n00b_list_append(row,
                         n00b_internal_type_repr(t, memos, &n));
        n00b_list_append(row, base_name);
        n00b_grid_add_row(grid, row);
    }
    n00b_set_column_style(grid, 0, n00b_new_utf8("snap"));
    n00b_set_column_style(grid, 1, n00b_new_utf8("snap"));
    n00b_set_column_style(grid, 2, n00b_new_utf8("snap"));
    return grid;
}
