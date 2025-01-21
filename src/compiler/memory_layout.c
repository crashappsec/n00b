#define N00B_USE_INTERNAL_API
#include "n00b.h"

static uint64_t
store_static_item(n00b_compile_ctx *ctx, void *value)
{
    n00b_static_memory *mem  = ctx->memory_layout;
    n00b_mem_ptr        item = (n00b_mem_ptr){.v = value};
    uint64_t            result;

    if (mem->num_items == mem->alloc_len) {
        n00b_mem_ptr *items;

        items = n00b_gc_array_alloc_mapped(n00b_mem_ptr,
                                           mem->num_items + getpagesize() / 8,
                                           N00B_GC_SCAN_ALL);
        if (mem->num_items) {
            memcpy(items, mem->items, mem->num_items * 8);
        }

        mem->alloc_len += getpagesize();
        mem->items = items;
    }

    result             = mem->num_items++;
    mem->items[result] = item;

    return result;
}

// Note that we do per-module offsets for most static data so that
// modules are easily relocatable, and so that we can eventually cache
// the info without having to recompile modules that don't need it.
//
// We'll even potentially be able to hot-reload modules that don't
// change their APIs or the static layout of existing variables.
//
// Global variables get a storage slot in the heap of the module that
// defines them.  That's why, if we see one that is linked to another
// symbol, we skip it. The actual static pointers are supposed to be
// 100% constant as far as the runtime is concerned.

uint64_t
n00b_add_static_object(void *obj, n00b_compile_ctx *ctx)
{
    n00b_type_t *t = n00b_get_my_type(obj);

    if (n00b_type_is_string(t)) {
        return n00b_add_static_string(obj, ctx);
    }

    bool found = false;

    uint64_t res = (uint64_t)hatrack_dict_get(ctx->obj_consts, obj, &found);

    if (found) {
        return res;
    }

    res = store_static_item(ctx, obj);
    hatrack_dict_put(ctx->obj_consts, obj, (void *)res);

    return res;
}

uint64_t
n00b_add_static_string(n00b_str_t *s, n00b_compile_ctx *ctx)
{
    // Strings that have style info are currently NOT cached by hash,
    // because the string hashing algorithm ignores the style info.
    // So if there's style info, we store it by pointer.
    //
    // TODO to change the hashing algorithm; we should take this into
    // account for sure.

    bool     found = false;
    uint64_t res;

    s = n00b_to_utf8(s);

    if (s->styling && s->styling->num_entries != 0) {
        res = (uint64_t)hatrack_dict_get(ctx->obj_consts, s, &found);
        if (found) {
            return res;
        }
        res = store_static_item(ctx, s);
        hatrack_dict_put(ctx->obj_consts, s, (void *)res);

        return res;
    }

    res = (uint64_t)hatrack_dict_get(ctx->str_consts, s, &found);

    if (found) {
        return res;
    }

    res = store_static_item(ctx, s);
    hatrack_dict_put(ctx->obj_consts, s, (void *)res);

    return res;
}

// This is meant for constant values associated with symbols.
uint64_t
n00b_add_value_const(uint64_t val, n00b_compile_ctx *ctx)
{
    bool     found = false;
    uint64_t res   = (uint64_t)hatrack_dict_get(ctx->value_consts,
                                              (void *)val,
                                              &found);

    if (found) {
        return res;
    }

    res = store_static_item(ctx, (void *)val);
    hatrack_dict_put(ctx->value_consts, (void *)val, (void *)res);

    return res;
}

static inline uint64_t
n00b_layout_static_obj(n00b_module_t *ctx, int bytes, int alignment)
{
    int      l      = ctx->static_size + bytes;
    uint64_t result = n00b_round_up_to_given_power_of_2(alignment, l);

    ctx->static_size += result;

    return result;
}

uint32_t
_n00b_layout_const_obj(n00b_compile_ctx *cctx, n00b_obj_t obj, ...)
{
    va_list args;

    va_start(args, obj);

    n00b_module_t    *fctx = va_arg(args, n00b_module_t *);
    n00b_tree_node_t *loc  = NULL;
    n00b_utf8_t      *name = NULL;

    if (fctx != NULL) {
        loc  = va_arg(args, n00b_tree_node_t *);
        name = va_arg(args, n00b_utf8_t *);
    }

    va_end(args);

    // Sym is only needed if there's an error, which there shouldn't
    // be when used internally during codegen. In those cases sym wil
    // be NULL, which should be totally fine.
    if (!obj) {
        n00b_add_error(fctx, n00b_err_const_not_provided, loc, name);
        return 0;
    }

    n00b_type_t *objtype = n00b_get_my_type(obj);

    if (n00b_type_is_box(objtype)) {
        return n00b_add_value_const(n00b_unbox(obj), cctx);
    }

    return n00b_add_static_object(obj, cctx);
}

static void
layout_static(n00b_compile_ctx *cctx,
              n00b_module_t    *fctx,
              void            **view,
              uint64_t          n)
{
    for (unsigned int i = 0; i < n; i++) {
        n00b_symbol_t *my_sym_copy = view[i];
        n00b_symbol_t *sym         = my_sym_copy;

        if (sym->linked_symbol != NULL) {
            // Only allocate space for globals in the module where
            // we're most dependent.
            continue;
        }
        // We go ahead and add this to all symbols, but it's only
        // used for static allocations of non-const variables.
        sym->local_module_id = fctx->module_id;

        switch (sym->kind) {
        case N00B_SK_ENUM_VAL:
            if (n00b_types_are_compat(sym->type, n00b_type_utf8(), NULL)) {
                sym->static_offset = n00b_layout_const_obj(cctx,
                                                           sym->value,
                                                           fctx,
                                                           sym->ct->declaration_node,
                                                           sym->name);
            }
            break;
        case N00B_SK_VARIABLE:
            // We might someday allow references to const vars, so go
            // ahead and stick them in static data always.
            if (n00b_sym_is_declared_const(sym)) {
                sym->static_offset = n00b_layout_const_obj(cctx,
                                                           sym->value,
                                                           fctx,
                                                           sym->ct->declaration_node,
                                                           sym->name);
                break;
            }
            else {
                // For now, just lay everything in the world out as
                // 8 byte values (and thus 8 byte aligned).
                sym->static_offset = n00b_layout_static_obj(fctx, 8, 8);
                break;
            }
        default:
            continue;
        }
        my_sym_copy->static_offset = sym->static_offset;
    }
}

// This one measures in stack value slots, not in bytes.
static int64_t
layout_stack(void **view, uint64_t n)
{
    // Address 0 is always $result, if it exists.
    int32_t next_formal = -2;
    int32_t next_local  = 1;

    while (n--) {
        n00b_symbol_t *sym = view[n];

        // Will already be zero-allocated.
        if (!strcmp(sym->name->data, "$result")) {
            continue;
        }

        switch (sym->kind) {
        case N00B_SK_VARIABLE:
            sym->static_offset = next_local;
            next_local += 1;
            continue;
        case N00B_SK_FORMAL:
            sym->static_offset = next_formal;
            next_formal -= 1;
            continue;
        default:
            continue;
        }
    }

    return next_local;
}

static void
layout_func(n00b_module_t *ctx,
            n00b_symbol_t *sym,
            int            i)
{
    uint64_t        n;
    n00b_fn_decl_t *decl       = sym->value;
    n00b_scope_t   *scope      = decl->signature_info->fn_scope;
    void          **view       = hatrack_dict_values_sort(scope->symbols, &n);
    int             frame_size = layout_stack(view, n);

    decl->frame_size = frame_size;

    if (decl->once) {
        decl->sc_bool_offset = n00b_layout_static_obj(ctx,
                                                      sizeof(bool),
                                                      8);
        decl->sc_lock_offset = n00b_layout_static_obj(ctx,
                                                      sizeof(n00b_lock_t),
                                                      8);
        decl->sc_memo_offset = n00b_layout_static_obj(ctx,
                                                      sizeof(void *),
                                                      8);
    }
}

void
n00b_layout_module_symbols(n00b_compile_ctx *cctx, n00b_module_t *fctx)
{
    uint64_t n;

    // Very first item in every module will be information about whether
    // there are parameters, and if they are set.
    void **view = hatrack_dict_values_sort(fctx->parameters, &n);
    int    pix  = 0;

    for (unsigned int i = 0; i < n; i++) {
        n00b_module_param_info_t *param = view[i];
        n00b_symbol_t            *sym   = param->linked_symbol;

        // These don't need an index; we test for the default by
        // asking the attr store.
        if (sym->kind == N00B_SK_ATTR) {
            continue;
        }

        param->param_index = pix++;
    }

    // We keep one bit per parameter, and the length is measured
    // in bytes, so we divide by 8, after adding 7 to make sure we
    // round up. We don't need the result; it's always at offset
    // 0, but it controls where the next variable is stored.
    n00b_layout_static_obj(fctx, (pix + 7) / 8, 8);

    view = hatrack_dict_values_sort(fctx->ct->global_scope->symbols, &n);
    layout_static(cctx, fctx, view, n);

    view = hatrack_dict_values_sort(fctx->module_scope->symbols, &n);
    layout_static(cctx, fctx, view, n);

    n = n00b_list_len(fctx->fn_def_syms);

    for (unsigned int i = 0; i < n; i++) {
        n00b_symbol_t *sym = n00b_list_get(fctx->fn_def_syms, i, NULL);
        layout_func(fctx, sym, i);
    }
}
