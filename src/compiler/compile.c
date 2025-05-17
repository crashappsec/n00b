#define N00B_USE_INTERNAL_API
#include "n00b.h"

extern void
n00b_setup_new_module_allocations(n00b_compile_ctx *cctx, n00b_vm_t *vm);

void
n00b_cctx_gc_bits(uint64_t *bitfield, n00b_compile_ctx *ctx)
{
    n00b_mark_raw_to_addr(bitfield, ctx, &ctx->processed);
}

void
n00b_smem_gc_bits(uint64_t *bitfield, n00b_static_memory *mem)
{
    n00b_mark_raw_to_addr(bitfield, mem, &mem->items);
}

n00b_compile_ctx *
n00b_new_compile_ctx()
{
    return n00b_gc_alloc_mapped(n00b_compile_ctx, N00B_GC_SCAN_ALL);
}

static hatrack_hash_t
module_ctx_hash(n00b_module_t *ctx)
{
    return ctx->modref;
}

n00b_compile_ctx *
n00b_new_compile_context(n00b_string_t *input)
{
    n00b_compile_ctx *result = n00b_new_compile_ctx();

    result->module_cache  = n00b_dict(n00b_type_u64(), n00b_type_ref());
    result->final_spec    = n00b_new_spec();
    result->final_attrs   = n00b_new_scope(NULL, N00B_SCOPE_GLOBAL);
    result->final_globals = n00b_new_scope(NULL, N00B_SCOPE_ATTRIBUTES);
    result->backlog       = n00b_new(n00b_type_set(n00b_type_ref()),
                               n00b_kw("hash", n00b_ka(module_ctx_hash)));
    result->processed     = n00b_new(n00b_type_set(n00b_type_ref()),
                                 n00b_kw("hash", n00b_ka(module_ctx_hash)));
    result->memory_layout = n00b_gc_alloc_mapped(n00b_static_memory,
                                                 N00B_GC_SCAN_ALL);
    result->str_consts    = n00b_dict(n00b_type_string(), n00b_type_u64());
    result->obj_consts    = n00b_dict(n00b_type_ref(), n00b_type_u64());
    result->value_consts  = n00b_dict(n00b_type_u64(), n00b_type_u64());

    if (input != NULL) {
        result->entry_point = n00b_init_module_from_loc(result, input);
        n00b_add_module_to_worklist(result, result->entry_point);
    }

    return result;
}

static n00b_string_t *str_to_type_tmp_path = NULL;

// This lives in this module because we invoke the parser; things that
// invoke the compiler live here.
n00b_type_t *
n00b_string_to_type(n00b_string_t *str)
{
    if (str_to_type_tmp_path == NULL) {
        str_to_type_tmp_path = n00b_cstring("<< string evaluation >>");
        n00b_gc_register_root(&str_to_type_tmp_path, 1);
    }

    n00b_type_t    *result = NULL;
    n00b_stream_t *stream = n00b_string_channel(str);
    n00b_module_t   ctx    = {
             .modref = 0xffffffff,
             .path   = str_to_type_tmp_path,
             .name   = str_to_type_tmp_path,
    };

    if (n00b_lex(&ctx, stream) != false) {
        n00b_parse_type(&ctx);
    }

    n00b_close(stream);

    if (ctx.ct->parse_tree != NULL) {
        n00b_dict_t *type_ctx = n00b_new(n00b_type_dict(n00b_type_string(),
                                                        n00b_type_ref()));

        result = n00b_node_to_type(&ctx, ctx.ct->parse_tree, type_ctx);
    }

    if (ctx.ct->parse_tree == NULL || n00b_fatal_error_in_module(&ctx)) {
        N00B_CRAISE("Invalid type.");
    }

    return result;
}

static void
merge_function_decls(n00b_compile_ctx *cctx, n00b_module_t *fctx)
{
    n00b_scope_t         *scope = fctx->module_scope;
    hatrack_dict_value_t *items;
    uint64_t              n;

    items = hatrack_dict_values(scope->symbols, &n);

    for (uint64_t i = 0; i < n; i++) {
        n00b_symbol_t *new = items[i];

        if (new->kind != N00B_SK_FUNC &&new->kind != N00B_SK_EXTERN_FUNC) {
            continue;
        }
        n00b_fn_decl_t *decl = new->value;
        if (decl->private) {
            continue;
        }

        if (!hatrack_dict_add(cctx->final_globals->symbols, new->name, new)) {
            n00b_symbol_t *old = hatrack_dict_get(cctx->final_globals->symbols,
                                                  new->name,
                                                  NULL);

            if (old != new) {
                n00b_add_warning(fctx,
                                 n00b_warn_cant_export,
                                 new->ct->declaration_node);
            }
        }
    }
}

// This loads all modules up through symbol declaration.
static void
n00b_perform_module_loads(n00b_compile_ctx *ctx)
{
    n00b_module_t *cur;

    while (true) {
        cur = n00b_set_any_item(ctx->backlog, NULL);
        if (cur == NULL) {
            return;
        }

        if (cur->ct == NULL) {
            // Previously processed from another run.
            continue;
        }

        if (cur->ct->status < n00b_compile_status_code_loaded) {
            n00b_parse(cur);
            n00b_module_decl_pass(ctx, cur);
            if (n00b_fatal_error_in_module(cur)) {
                ctx->fatality = true;
                return;
            }
            if (n00b_fatal_error_in_module(cur)) {
                ctx->fatality = true;
                return;
            }
        }

        if (cur->ct->status < n00b_compile_status_scopes_merged) {
            merge_function_decls(ctx, cur);
            n00b_module_set_status(cur, n00b_compile_status_scopes_merged);
        }

        n00b_set_put(ctx->processed, cur);
        n00b_set_remove(ctx->backlog, cur);
    }
}

typedef struct topologic_search_ctx {
    n00b_module_t    *cur;
    n00b_list_t      *visiting;
    n00b_compile_ctx *cctx;
} tsearch_ctx;

static void
topological_order_process(tsearch_ctx *ctx)
{
    n00b_module_t *cur = ctx->cur;

    if (n00b_list_contains(ctx->visiting, cur)) {
        // Cycle. I intend to add an info message here, otherwise
        // could avoid popping and just get this down to one test.
        return;
    }

    // If it already got added to the partial ordering, we don't need to
    // process it when it gets re-imported somewhere else.
    if (n00b_list_contains(ctx->cctx->module_ordering, cur)) {
        return;
    }

    if (cur->ct->imports == NULL || cur->ct->imports->symbols == NULL) {
        n00b_list_append(ctx->cctx->module_ordering, cur);
        return;
    }

    n00b_list_append(ctx->visiting, cur);

    uint64_t              num_imports;
    hatrack_dict_value_t *imports;

    imports = hatrack_dict_values(cur->ct->imports->symbols, &num_imports);

    for (uint64_t i = 0; i < num_imports; i++) {
        n00b_symbol_t    *sym  = imports[i];
        n00b_tree_node_t *n    = sym->ct->declaration_node;
        n00b_pnode_t     *pn   = n00b_tree_get_contents(n);
        n00b_module_t    *next = (n00b_module_t *)pn->value;

        if (next != cur) {
            ctx->cur = next;
            topological_order_process(ctx);
        }
        else {
            ctx->cctx->fatality = true;
            n00b_add_error(cur, n00b_err_self_recursive_use, n);
        }
    }

    cur = n00b_list_pop(ctx->visiting);
    n00b_list_append(ctx->cctx->module_ordering, cur);
}

static void
build_topological_ordering(n00b_compile_ctx *cctx)
{
    // While we don't strictly need this partial ordering, once we get
    // through phase 1 where we've pulled out symbols per-module, we
    // will process merging those symbols using a partial ordering, so
    // that, whenever possiblle, conflicts are raised when processing
    // the dependent code.
    //
    // That may not always happen with cycles, of course. We break
    // those cycles via keeping a "visiting" stack in a depth-first
    // search.

    tsearch_ctx search_state = {
        .cur      = cctx->sys_package,
        .visiting = n00b_new(n00b_type_list(n00b_type_ref())),
        .cctx     = cctx,
    };

    if (!cctx->module_ordering) {
        cctx->module_ordering = n00b_list(n00b_type_ref());
        topological_order_process(&search_state);
    }

    if (cctx->entry_point) {
        search_state.cur = cctx->entry_point;
        topological_order_process(&search_state);
    }
}

static void
merge_one_plain_scope(n00b_compile_ctx *cctx,
                      n00b_module_t    *fctx,
                      n00b_scope_t     *local,
                      n00b_scope_t     *global)

{
    uint64_t              num_symbols;
    hatrack_dict_value_t *items;
    n00b_symbol_t        *new_sym;
    n00b_symbol_t        *old_sym;

    items = hatrack_dict_values(local->symbols, &num_symbols);

    for (uint64_t i = 0; i < num_symbols; i++) {
        new_sym = items[i];

        if (hatrack_dict_add(global->symbols,
                             new_sym->name,
                             new_sym)) {
            new_sym->local_module_id = fctx->module_id;
            continue;
        }

        old_sym = hatrack_dict_get(global->symbols,
                                   new_sym->name,
                                   NULL);
        if (n00b_merge_symbols(fctx, new_sym, old_sym)) {
            hatrack_dict_put(global->symbols, new_sym->name, old_sym);
            new_sym->linked_symbol   = old_sym;
            new_sym->local_module_id = old_sym->local_module_id;
        }
        // Else, there's some error and it doesn't matter, things won't run.
    }
}

static void
merge_one_confspec(n00b_compile_ctx *cctx, n00b_module_t *fctx)
{
    if (fctx->ct->local_specs == NULL) {
        return;
    }

    cctx->final_spec->in_use = true;

    uint64_t              num_sections;
    n00b_dict_t          *fspecs = cctx->final_spec->section_specs;
    hatrack_dict_value_t *sections;

    sections = hatrack_dict_values(fctx->ct->local_specs->section_specs,
                                   &num_sections);

    if (num_sections && cctx->final_spec->locked) {
        n00b_add_error(fctx,
                       n00b_err_spec_locked,
                       fctx->ct->local_specs->declaration_node);
        cctx->fatality = true;
    }

    for (uint64_t i = 0; i < num_sections; i++) {
        n00b_spec_section_t *cur = sections[i];

        if (hatrack_dict_add(fspecs, cur->name, cur)) {
            continue;
        }

        n00b_spec_section_t *old = hatrack_dict_get(fspecs, cur->name, NULL);

        n00b_add_error(fctx,
                       n00b_err_spec_redef_section,
                       cur->declaration_node,
                       cur->name,
                       n00b_node_get_loc_str(old->declaration_node));
        cctx->fatality = true;
    }

    n00b_spec_section_t *root_adds = fctx->ct->local_specs->root_section;
    n00b_spec_section_t *true_root = cctx->final_spec->root_section;
    uint64_t             num_fields;

    if (root_adds == NULL) {
        return;
    }

    if (root_adds->short_doc) {
        if (!true_root->short_doc) {
            true_root->short_doc = root_adds->short_doc;
        }
        else {
            true_root->short_doc = n00b_cformat("«#»\n«#»",
                                                true_root->short_doc,
                                                root_adds->short_doc);
        }
    }
    if (root_adds->long_doc) {
        if (!true_root->long_doc) {
            true_root->long_doc = root_adds->long_doc;
        }
        else {
            true_root->long_doc = n00b_cformat("«#»\n«#»",
                                               true_root->long_doc,
                                               root_adds->long_doc);
        }
    }

    hatrack_dict_value_t *fields = hatrack_dict_values(root_adds->fields,
                                                       &num_fields);

    for (uint64_t i = 0; i < num_fields; i++) {
        n00b_spec_field_t *cur = fields[i];

        if (hatrack_dict_add(true_root->fields, cur->name, cur)) {
            continue;
        }

        n00b_spec_field_t *old = hatrack_dict_get(root_adds->fields,
                                                  cur->name,
                                                  NULL);

        n00b_add_error(fctx,
                       n00b_err_spec_redef_field,
                       cur->declaration_node,
                       cur->name,
                       n00b_node_get_loc_str(old->declaration_node));
        cctx->fatality = true;
    }

    if (root_adds->allowed_sections != NULL) {
        uint64_t num_allows;
        void   **allows = n00b_set_items(root_adds->allowed_sections,
                                       &num_allows);

        for (uint64_t i = 0; i < num_allows; i++) {
            if (!n00b_set_add(true_root->allowed_sections, allows[i])) {
                n00b_add_warning(fctx,
                                 n00b_warn_dupe_allow,
                                 root_adds->declaration_node);
            }
            n00b_assert(n00b_set_contains(true_root->allowed_sections,
                                          allows[i]));
        }
    }

    if (root_adds->required_sections != NULL) {
        uint64_t num_reqs;
        void   **reqs = n00b_set_items(root_adds->required_sections,
                                     &num_reqs);

        for (uint64_t i = 0; i < num_reqs; i++) {
            if (!n00b_set_add(true_root->required_sections, reqs[i])) {
                n00b_add_warning(fctx,
                                 n00b_warn_dupe_require,
                                 root_adds->declaration_node);
            }
        }
    }

    if (root_adds->validator == NULL) {
        return;
    }

    if (true_root->validator != NULL) {
        n00b_add_error(fctx,
                       n00b_err_dupe_validator,
                       root_adds->declaration_node);
        cctx->fatality = true;
    }
    else {
        true_root->validator = root_adds->validator;
    }
}

static void
merge_var_scope(n00b_compile_ctx *cctx, n00b_module_t *fctx)
{
    merge_one_plain_scope(cctx,
                          fctx,
                          fctx->ct->global_scope,
                          cctx->final_globals);
}

static void
merge_attrs(n00b_compile_ctx *cctx, n00b_module_t *fctx)
{
    merge_one_plain_scope(cctx,
                          fctx,
                          fctx->ct->attribute_scope,
                          cctx->final_attrs);
}

static void
merge_global_info(n00b_compile_ctx *cctx)
{
    n00b_module_t *fctx = NULL;

    build_topological_ordering(cctx);

    uint64_t mod_len = n00b_list_len(cctx->module_ordering);

    for (uint64_t i = 0; i < mod_len; i++) {
        fctx = n00b_list_get(cctx->module_ordering, i, NULL);

        if (!fctx->ct) {
            continue;
        }

        fctx->module_id = i;
        merge_one_confspec(cctx, fctx);
        merge_var_scope(cctx, fctx);
        merge_attrs(cctx, fctx);
    }
}

n00b_list_t *
n00b_system_module_files()
{
    return n00b_path_walk(n00b_system_module_path());
}

n00b_compile_ctx *
n00b_compile_from_entry_point(n00b_string_t *entry)
{
    // This creates a new compilation; a second entry point
    // needs to be added seprately after code has been generated
    // and there's a new VM.
    n00b_compile_ctx *result = n00b_new_compile_context(NULL);

    result->sys_package = n00b_find_module(result,
                                           n00b_n00b_root(),
                                           n00b_cstring(N00B_PACKAGE_INIT_MODULE),
                                           n00b_cstring("sys"),
                                           NULL,
                                           NULL,
                                           NULL);

    if (result->sys_package != NULL) {
        n00b_add_module_to_worklist(result, result->sys_package);
    }

    if (entry != NULL) {
        result->entry_point = n00b_init_module_from_loc(result, entry);
        n00b_add_module_to_worklist(result, result->entry_point);
        if (result->fatality) {
            return result;
        }
    }

    n00b_perform_module_loads(result);

    if (result->fatality) {
        return result;
    }
    merge_global_info(result);

    if (result->fatality) {
        return result;
    }
    n00b_check_pass(result);

    if (result->fatality) {
        return result;
    }

    return result;
}

bool
n00b_incremental_module(n00b_vm_t         *vm,
                        n00b_string_t     *location,
                        bool               new_entry,
                        n00b_compile_ctx **compile_state)
{
    void            **cache = (void **)&vm->obj->ccache;
    n00b_compile_ctx *ctx   = n00b_new_compile_ctx();

    ctx->final_spec      = vm->obj->attr_spec;
    ctx->module_ordering = vm->obj->module_contents;
    ctx->memory_layout   = vm->obj->static_contents;

    ctx->module_cache  = cache[N00B_CCACHE_CUR_MODULES];
    ctx->final_attrs   = cache[N00B_CCACHE_CUR_ASCOPE];
    ctx->final_globals = cache[N00B_CCACHE_CUR_GSCOPE];
    ctx->str_consts    = cache[N00B_CCACHE_CUR_SCONSTS];
    ctx->obj_consts    = cache[N00B_CCACHE_CUR_OCONSTS];
    ctx->value_consts  = cache[N00B_CCACHE_CUR_VCONSTS];

    ctx->backlog   = n00b_new(n00b_type_set(n00b_type_ref()),
                            n00b_kw("hash", n00b_ka(module_ctx_hash)));
    ctx->processed = n00b_new(n00b_type_set(n00b_type_ref()),
                              n00b_kw("hash", n00b_ka(module_ctx_hash)));

    if (compile_state) {
        *compile_state = ctx;
    }

    n00b_module_t *module = n00b_init_module_from_loc(ctx, location);
    n00b_module_t *sys;

    // This is only the entry point for the incremental add, used to
    // compute a partial ordering. Should prob do some renaming, since
    // this is not the permanant entry point in the VM.
    ctx->entry_point = module;

    n00b_add_module_to_worklist(ctx, module);

    if (ctx->fatality) {
        return false;
    }

    sys = n00b_find_module(ctx,
                           n00b_n00b_root(),
                           n00b_cstring(N00B_PACKAGE_INIT_MODULE),
                           n00b_cstring("sys"),
                           NULL,
                           NULL,
                           NULL);

    ctx->sys_package = sys;

    n00b_perform_module_loads(ctx);

    if (ctx->fatality) {
        return false;
    }

    merge_global_info(ctx);

    if (ctx->fatality) {
        return false;
    }

    n00b_check_pass(ctx);

    if (ctx->fatality) {
        return false;
    }

    n00b_internal_codegen(ctx, vm);
    n00b_setup_new_module_allocations(ctx, vm);

    if (new_entry) {
        vm->entry_point = module->module_id;
    }

    return true;
    // We don't need to copy back into the CCACHE; we got refs for
    // anything we might save out; everything else is first-save only.
}

bool
n00b_generate_code(n00b_compile_ctx *ctx, n00b_vm_t *vm)
{
    vm->obj->module_contents = ctx->module_ordering;
    vm->obj->first_entry     = ctx->entry_point->module_id;
    vm->obj->default_entry   = ctx->entry_point->module_id;
    vm->entry_point          = ctx->entry_point->module_id;
    vm->obj->static_contents = ctx->memory_layout;

    n00b_internal_codegen(ctx, vm);

    if (ctx->fatality) {
        return false;
    }

    n00b_setup_new_module_allocations(ctx, vm);

    void **cache = vm->obj->ccache;

    cache[N00B_CCACHE_ORIG_ASCOPE]  = ctx->final_attrs;
    cache[N00B_CCACHE_CUR_ASCOPE]   = n00b_scope_copy(ctx->final_attrs);
    cache[N00B_CCACHE_ORIG_GSCOPE]  = ctx->final_globals;
    cache[N00B_CCACHE_CUR_GSCOPE]   = n00b_scope_copy(ctx->final_globals);
    cache[N00B_CCACHE_ORIG_SCONSTS] = ctx->str_consts;
    cache[N00B_CCACHE_CUR_SCONSTS]  = n00b_shallow(ctx->str_consts);
    cache[N00B_CCACHE_ORIG_OCONSTS] = ctx->obj_consts;
    cache[N00B_CCACHE_CUR_OCONSTS]  = n00b_shallow(ctx->obj_consts);
    cache[N00B_CCACHE_ORIG_VCONSTS] = ctx->value_consts;
    cache[N00B_CCACHE_CUR_VCONSTS]  = n00b_shallow(ctx->value_consts);
    cache[N00B_CCACHE_ORIG_SPEC]    = ctx->final_spec;
    cache[N00B_CCACHE_ORIG_SORT]    = n00b_shallow(ctx->module_ordering);
    cache[N00B_CCACHE_ORIG_MODULES] = ctx->module_cache;
    cache[N00B_CCACHE_CUR_MODULES]  = n00b_shallow(ctx->module_cache);
    cache[N00B_CCACHE_ORIG_STATIC]  = ctx->memory_layout;

    // Attrs and sects don't get saved to the cache until after we run.
    return true;
}
