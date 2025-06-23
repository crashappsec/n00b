#include "n00b.h"

const n00b_version_t n00b_current_version = {
    .dotted.preview = N00B_VERS_PREVIEW,
    .dotted.patch   = N00B_VERS_PATCH,
    .dotted.minor   = N00B_VERS_MINOR,
    .dotted.major   = N00B_VERS_MAJOR,
};

void
n00b_objfile_gc_bits(uint64_t *bitmap, n00b_zobject_file_t *obj)
{
    n00b_mark_raw_to_addr(bitmap, obj, &obj->ffi_info);
    // The above set 'zero magic' which is not a pointer, but must be first.
    *bitmap &= ~1ULL;
}

void
n00b_zrun_state_gc_bits(uint64_t *bitmap, n00b_zrun_state_t *state)
{
    // Mark all.
    *bitmap = ~0;
}

void
n00b_add_module(n00b_zobject_file_t *obj, n00b_module_t *module)
{
    uint32_t existing_len = (uint32_t)n00b_list_len(obj->module_contents);
    module->module_id     = existing_len;

    n00b_list_append(obj->module_contents, module);
}

static inline void
n00b_vm_setup_ffi(n00b_vm_t *vm)
{
    vm->obj->ffi_info_entries = n00b_list_len(vm->obj->ffi_info);

    if (vm->obj->ffi_info_entries == 0) {
        return;
    }

    for (int i = 0; i < vm->obj->ffi_info_entries; i++) {
        n00b_ffi_decl_t *ffi_info = n00b_list_get(vm->obj->ffi_info, i, NULL);
        n00b_zffi_cif   *cif      = &ffi_info->cif;

        cif->fptr = n00b_ffi_find_symbol(ffi_info->external_name,
                                         ffi_info->dll_list);

        if (!cif->fptr) {
            // TODO: warn. For now, just error if it gets called.
            continue;
        }

        int             n       = ffi_info->num_ext_params;
        n00b_ffi_type **arglist = n00b_gc_array_alloc(n00b_ffi_type *, n);

        if (n < 0) {
            n = 0;
        }
        for (int j = 0; j < n; j++) {
            uint8_t param = ffi_info->external_params[j];
            arglist[j]    = n00b_ffi_arg_type_map(param);

            if (param == N00B_CSTR_CTYPE_CONST && j < 63) {
                cif->str_convert |= (1UL << j);
            }
        }

        if (ffi_info->external_return_type == N00B_CSTR_CTYPE_CONST) {
            cif->str_convert |= (1UL << 63);
        }

        ffi_prep_cif(&cif->cif,
                     N00B_FFI_DEFAULT_ABI,
                     n,
                     n00b_ffi_arg_type_map(ffi_info->external_return_type),
                     arglist);
    }
}

void
n00b_setup_new_module_allocations(n00b_compile_ctx *cctx, n00b_vm_t *vm)
{
    // This only needs to be called to add space for new modules.

    int     old_modules = 0;
    int     new_modules = n00b_list_len(vm->obj->module_contents);
    void ***new_allocs;

    if (vm->module_allocations) {
        while (vm->module_allocations[old_modules++])
            ;
    }

    if (old_modules == new_modules) {
        return;
    }

    // New modules generally come with new static data.
    new_allocs = n00b_gc_array_alloc(void *, new_modules + 1);

    for (int i = 0; i < old_modules; i++) {
        new_allocs[i] = vm->module_allocations[i];
    }

    for (int i = old_modules; i < new_modules; i++) {
        n00b_module_t *m = n00b_list_get(vm->obj->module_contents,
                                         i,
                                         NULL);

        new_allocs[i] = n00b_gc_array_alloc(void *,
                                            (m->static_size + 1) * 8);
    }

    vm->module_allocations = new_allocs;
}

n00b_buf_t *
n00b_vm_save(n00b_vm_t *vm)
{
    // Any future runs should reset to this point, if it's our first
    // run, and the original entry point != the current one.
    //
    // If it *is* current, the stash happens at the start of
    // execution.
    //
    // This VM could be saved without running, or saved after a run.
    // If it's saved without running, num_saved_runs is 0, and we
    // don't have to cache the original spec/attr state. If the number
    // is 1 however, we just finished our first run, and we DO want to
    // cache these things, plus we need to change our entry point.

    if (vm->num_saved_runs == 1) {
        vm->obj->ccache[N00B_CCACHE_ORIG_SECTIONS] = vm->all_sections;
        vm->obj->ccache[N00B_CCACHE_ORIG_ATTR]     = vm->attrs;
        vm->entry_point                            = vm->obj->default_entry;
    }

    vm->run_state = NULL;
    n00b_vm_remove_compile_time_data(vm);

    return n00b_automarshal(vm);
}

void
n00b_vm_global_run_state_init(n00b_vm_t *vm)
{
    // Global, meaning for all threads. But this is one time execution
    // state setup.

    vm->run_state = n00b_gc_alloc_mapped(n00b_zrun_state_t,
                                         (void *)n00b_zrun_state_gc_bits);
    n00b_vm_setup_ffi(vm);

    vm->last_saved_run_time = *n00b_now();

    // Go ahead and count it as saved at the beginning; we don't really
    // use this during the run, and if it doesn't get saved, then it'll
    // automatically reset.
    if (!vm->num_saved_runs++) {
        vm->first_saved_run_time = vm->last_saved_run_time;
    }

#ifdef N00B_DEV
    vm->run_state->print_buf    = n00b_buffer_empty();
    vm->run_state->print_stream = n00b_outstream_buffer(
        vm->run_state->print_buf,
        false);
#endif
}

void
n00b_vm_gc_bits(uint64_t *bitmap, n00b_vm_t *vm)
{
    n00b_mark_raw_to_addr(bitmap, vm, &vm->all_sections);
}

n00b_vm_t *
n00b_vm_new(n00b_compile_ctx *cctx)
{
    n00b_vm_t *vm = n00b_gc_alloc_mapped(n00b_vm_t, (void *)n00b_vm_gc_bits);

    vm->obj = n00b_gc_alloc_mapped(n00b_zobject_file_t,
                                   (void *)n00b_objfile_gc_bits);

    vm->obj->n00b_version    = n00b_current_version;
    vm->obj->module_contents = n00b_list(n00b_type_ref());
    vm->obj->func_info       = n00b_list(n00b_type_ref());
    vm->obj->ffi_info        = n00b_list(n00b_type_ref());

    if (cctx->final_spec && cctx->final_spec->in_use) {
        vm->obj->attr_spec = cctx->final_spec;
    }

    vm->attrs         = n00b_dict(n00b_type_string(), n00b_type_ref());
    vm->all_sections  = n00b_new(n00b_type_set(n00b_type_string()));
    vm->creation_time = *n00b_now();

    return vm;
}

void
n00b_vm_reset(n00b_vm_t *vm)
{
    void **cache = (void **)&vm->obj->ccache;

    vm->module_allocations         = NULL;
    vm->num_saved_runs             = 1; // Restore point is always 1.
    cache[N00B_CCACHE_CUR_SCONSTS] = n00b_copy(cache[N00B_CCACHE_ORIG_SCONSTS]);
    cache[N00B_CCACHE_CUR_OCONSTS] = n00b_copy(cache[N00B_CCACHE_ORIG_OCONSTS]);
    cache[N00B_CCACHE_CUR_VCONSTS] = n00b_copy(cache[N00B_CCACHE_ORIG_VCONSTS]);
    cache[N00B_CCACHE_CUR_ASCOPE]  = n00b_copy(cache[N00B_CCACHE_ORIG_ASCOPE]);
    cache[N00B_CCACHE_CUR_GSCOPE]  = n00b_copy(cache[N00B_CCACHE_ORIG_ASCOPE]);
    cache[N00B_CCACHE_CUR_MODULES] = n00b_copy(cache[N00B_CCACHE_ORIG_MODULES]);
    cache[N00B_CCACHE_CUR_TYPES]   = n00b_copy(cache[N00B_CCACHE_ORIG_TYPES]);
    vm->obj->module_contents       = n00b_copy(cache[N00B_CCACHE_ORIG_SORT]);
    vm->obj->attr_spec             = n00b_copy(cache[N00B_CCACHE_ORIG_SPEC]);
    vm->attrs                      = n00b_copy(cache[N00B_CCACHE_ORIG_ATTR]);
    vm->all_sections               = n00b_copy(cache[N00B_CCACHE_ORIG_SECTIONS]);
    vm->obj->static_contents       = n00b_copy(cache[N00B_CCACHE_ORIG_STATIC]);
    vm->entry_point                = vm->obj->default_entry;
}

n00b_vmthread_t *
n00b_vmthread_new(n00b_vm_t *vm)
{
    if (vm->run_state == NULL) {
        // First thread only.
        n00b_vm_global_run_state_init(vm);
    }

    n00b_vmthread_t *tstate = n00b_gc_alloc_mapped(n00b_vmthread_t,
                                                   N00B_GC_SCAN_ALL);
    tstate->vm              = vm;

    n00b_vmthread_reset(tstate);

    // tstate->thread_arena = arena;

    // n00b_internal_unstash_heap();
    return tstate;
}

void
n00b_vmthread_reset(n00b_vmthread_t *tstate)
{
    tstate->sp         = &tstate->stack[N00B_STACK_SIZE];
    tstate->fp         = tstate->sp;
    tstate->pc         = 0;
    tstate->num_frames = 1;
    tstate->r0         = NULL;
    tstate->r1         = NULL;
    tstate->r2         = NULL;
    tstate->r3         = NULL;
    tstate->running    = false;
    tstate->error      = false;

    tstate->current_module = n00b_list_get(tstate->vm->obj->module_contents,
                                           tstate->vm->entry_point,
                                           NULL);
}

void
n00b_vm_remove_compile_time_data(n00b_vm_t *vm)
{
    int n = n00b_list_len(vm->obj->module_contents);

    for (int i = 0; i < n; i++) {
        n00b_module_t *m        = n00b_list_get(vm->obj->module_contents,
                                         i,
                                         NULL);
        m->ct                   = NULL;
        m->module_scope->parent = NULL;
        n00b_list_t *syms       = n00b_dict_values(m->module_scope->symbols);
        uint64_t     nsyms      = n00b_list_len(syms);

        for (uint64_t j = 0; j < nsyms; j++) {
            n00b_symbol_t *sym = n00b_list_get(syms, j, NULL);
            sym->ct            = NULL;

            if (sym->kind == N00B_SK_FUNC) {
                n00b_fn_decl_t *fn       = sym->value;
                n00b_scope_t   *fnscope  = fn->signature_info->fn_scope;
                n00b_list_t    *sub_syms = n00b_dict_values(fnscope->symbols);
                uint64_t        nsub     = n00b_list_len(sub_syms);

                for (uint64_t k = 0; k < nsub; k++) {
                    n00b_symbol_t *sub = n00b_list_get(sub_syms, k, NULL);
                    sub->ct            = NULL;
                }

                fnscope  = fn->signature_info->formals;
                sub_syms = n00b_dict_values(fnscope->symbols);
                nsub     = n00b_list_len(sub_syms);

                for (uint64_t k = 0; k < nsub; k++) {
                    n00b_symbol_t *sub = n00b_list_get(sub_syms, k, NULL);
                    sub->ct            = NULL;
                }

                fn->cfg = NULL; // TODO: move to a ct object
            }
        }
    }
}

const n00b_vtable_t n00b_vm_vtable = {
    .methods = {},
};
