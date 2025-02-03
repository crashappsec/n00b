#define N00B_USE_INTERNAL_API
#include "n00b.h"

#ifdef N00B_DEV
#define gen_debug(debug_arg) \
    emit(ctx, N00B_ZDebug, n00b_kw("arg", n00b_ka(debug_arg)))
#define debug_label(cstr) \
    gen_label(ctx, n00b_new_utf8(cstr))
#else
#define gen_debug(debug_arg)
#define debug_label(cstr)
#endif

// This is used in nested if/else only.
typedef struct {
    n00b_jump_info_t *targets;
    int               next;
} target_info_t;

typedef enum {
    assign_to_mem_slot,
    assign_via_index_set_call,
    assign_via_slice_set_call,
    assign_via_len_then_slice_set_call,
    assign_to_attribute,
} assign_type_t;

typedef struct {
    n00b_compile_ctx *cctx;
    n00b_module_t    *fctx;
    n00b_list_t      *instructions;
    n00b_tree_node_t *cur_node;
    n00b_pnode_t     *cur_pnode;
    n00b_module_t    *cur_module;
    n00b_list_t      *call_backpatches;
    target_info_t    *target_info;
    n00b_symbol_t    *retsym;
    int               instruction_counter;
    int               current_stack_offset;
    int               max_stack_size;
    int               module_patch_loc;
    bool              lvalue;
    assign_type_t     assign_method;
    bool              attr_lock;
} gen_ctx;

static void gen_one_node(gen_ctx *);

void
n00b_zinstr_gc_bits(uint64_t *bitmap, n00b_zinstruction_t *instr)
{
    n00b_set_bit(bitmap, n00b_ptr_diff(instr, &instr->type_info));
}

n00b_zinstruction_t *
n00b_new_instruction()
{
    // TODO: fixme
    return n00b_gc_alloc_mapped(n00b_zinstruction_t, N00B_GC_SCAN_ALL);
}

void
n00b_zfn_gc_bits(uint64_t *bitmap, n00b_zfn_info_t *fn)
{
    n00b_mark_raw_to_addr(bitmap, fn, &fn->longdoc);
}

n00b_zfn_info_t *
n00b_new_zfn()
{
    return n00b_gc_alloc_mapped(n00b_zfn_info_t, n00b_zfn_gc_bits);
}

void
n00b_jump_info_gc_bits(uint64_t *bitmap, n00b_jump_info_t *ji)
{
    n00b_mark_raw_to_addr(bitmap, ji, &ji->to_patch);
}

n00b_jump_info_t *
n00b_new_jump_info()
{
    return n00b_gc_alloc_mapped(n00b_jump_info_t, n00b_jump_info_gc_bits);
}

void
n00b_backpatch_gc_bits(uint64_t *bitmap, n00b_call_backpatch_info_t *bp)
{
    n00b_mark_raw_to_addr(bitmap, bp, &bp->i);
}

static n00b_call_backpatch_info_t *
new_backpatch()
{
    return n00b_gc_alloc_mapped(n00b_call_backpatch_info_t, n00b_backpatch_gc_bits);
}

static void
_emit(gen_ctx *ctx, int32_t op32, ...)
{
    n00b_karg_only_init(op32);

    n00b_type_t          *type      = NULL;
    n00b_zinstruction_t **instrptr  = NULL;
    n00b_zop_t            op        = (n00b_zop_t)(op32);
    int64_t               arg       = 0;
    int64_t               immediate = 0;
    int64_t               module_id = ctx->fctx->module_id;

    n00b_kw_int64("immediate", immediate);
    n00b_kw_int64("arg", arg);
    n00b_kw_int64("module_id", module_id);
    n00b_kw_ptr("type", type);
    n00b_kw_ptr("instrptr", instrptr);

    n00b_zinstruction_t *instr = n00b_new_instruction();
    instr->op                  = op;
    instr->module_id           = (int16_t)module_id;
    instr->line_no             = n00b_node_get_line_number(ctx->cur_node);
    instr->arg                 = (int64_t)arg;
    instr->immediate           = immediate;
    instr->type_info           = type ? n00b_type_resolve(type) : NULL;

    if (instrptr != NULL) {
        *instrptr = instr;
    }

    n00b_list_append(ctx->instructions, instr);

    ctx->instruction_counter++;
}

#define emit(ctx, op, ...) _emit(ctx, (int32_t)op, N00B_VA(__VA_ARGS__))

// Returns true if the jump is fully emitted, false if a patch is required,
// in which case the caller is responsible for tracking the patch.
static inline bool
gen_jump_raw(gen_ctx          *ctx,
             n00b_jump_info_t *jmp_info,
             n00b_zop_t        opcode,
             bool              pop)
{
    bool    result = true;
    int32_t arg    = 0;

    if (!pop && opcode != N00B_ZJ) { // No builtin pop for N00B_ZJ.
        emit(ctx, N00B_ZDupTop);
    }

    // jmp_info->code_offset = ctx->instruction_counter;

    // Look to see if we can fill in the jump target now.  If 'top' is
    // set, we are definitely jumping backwards, and can patch.
    if (jmp_info->linked_control_structure != NULL) {
        if (jmp_info->top) {
            arg    = jmp_info->linked_control_structure->entry_ip;
            result = false;
        }
        else {
            if (jmp_info->linked_control_structure->exit_ip != 0) {
                arg    = jmp_info->linked_control_structure->exit_ip;
                result = false;
            }
        }
    }

    n00b_zinstruction_t *instr;

    emit(ctx, opcode, n00b_kw("arg", n00b_ka(arg), "instrptr", n00b_ka(&instr)));

    if (result) {
        jmp_info->to_patch      = instr;
        n00b_control_info_t *ci = jmp_info->linked_control_structure;

        if (ci != NULL) {
            n00b_list_append(ci->awaiting_patches, jmp_info);
        }
    }

    return result;
}

static inline bool
gen_jz(gen_ctx *ctx, n00b_jump_info_t *jmp_info, bool pop)
{
    return gen_jump_raw(ctx, jmp_info, N00B_ZJz, pop);
}

static inline bool
gen_jnz(gen_ctx *ctx, n00b_jump_info_t *jmp_info, bool pop)
{
    return gen_jump_raw(ctx, jmp_info, N00B_ZJnz, pop);
}

static inline bool
gen_j(gen_ctx *ctx, n00b_jump_info_t *jmp_info)
{
    return gen_jump_raw(ctx, jmp_info, N00B_ZJ, false);
}

static inline void
gen_finish_jump(gen_ctx *ctx, n00b_jump_info_t *jmp_info)
{
    int32_t              arg   = ctx->instruction_counter;
    n00b_zinstruction_t *instr = jmp_info->to_patch;

    if (instr) {
        instr->arg = arg;
    }
}

static inline void
gen_apply_waiting_patches(gen_ctx *ctx, n00b_control_info_t *ci)
{
    int n       = n00b_list_len(ci->awaiting_patches);
    ci->exit_ip = ctx->instruction_counter;

    for (int i = 0; i < n; i++) {
        n00b_jump_info_t *one = n00b_list_get(ci->awaiting_patches, i, NULL);
        n00b_assert(one->to_patch);
        one->to_patch->arg = ci->exit_ip;
    }
}

static inline void
gen_tcall(gen_ctx *ctx, n00b_builtin_type_fn fn, n00b_type_t *t)
{
    if (t != NULL) {
        t = n00b_type_resolve(t);
        if (n00b_type_is_box(t)) {
            t = n00b_type_unbox(t);
        }
    }

    emit(ctx, N00B_ZTCall, n00b_kw("arg", n00b_ka(fn), "type", n00b_ka(t)));
}

// Two operands have been pushed onto the stack. But if it's a primitive
// type, we want to generate a raw ZCmp; otherwise, we want to generate
// a polymorphic tcall.

static inline void
gen_equality_test(gen_ctx *ctx, n00b_type_t *operand_type)
{
    if (n00b_type_is_value_type(operand_type)) {
        emit(ctx, N00B_ZCmp);
    }
    else {
        gen_tcall(ctx, N00B_BI_EQ, operand_type);
    }
}

// Helpers for the common case where we have one exit only.
#define JMP_TEMPLATE(g, user_code, ...)                                  \
    do {                                                                 \
        n00b_jump_info_t *ji    = n00b_new_jump_info();                  \
        bool              patch = g(ctx, ji __VA_OPT__(, ) __VA_ARGS__); \
        user_code;                                                       \
        if (patch) {                                                     \
            gen_finish_jump(ctx, ji);                                    \
        }                                                                \
    } while (0)

#define GEN_J(user_code)         JMP_TEMPLATE(gen_j, user_code)
#define GEN_JZ(user_code)        JMP_TEMPLATE(gen_jz, user_code, true)
#define GEN_JNZ(user_code)       JMP_TEMPLATE(gen_jnz, user_code, true)
#define GEN_JZ_NOPOP(user_code)  JMP_TEMPLATE(gen_jz, user_code, false)
#define GEN_JNZ_NOPOP(user_code) JMP_TEMPLATE(gen_jnz, user_code, false)

static inline void
set_stack_offset(gen_ctx *ctx, n00b_symbol_t *sym)
{
    sym->static_offset = ctx->current_stack_offset++;
    if (ctx->current_stack_offset > ctx->max_stack_size) {
        ctx->max_stack_size = ctx->current_stack_offset;
    }
}

static void
gen_load_immediate(gen_ctx *ctx, int64_t value)
{
    emit(ctx, N00B_ZPushImm, n00b_kw("immediate", n00b_ka(value)));
}

// TODO: when we add refs, these should change the types written to
// indicate the item on the stack is a ref to a particular type.
//
// For now, they just indicate tspec_ref().

static inline bool
unbox_const_value(gen_ctx *ctx, n00b_obj_t obj, n00b_type_t *type)
{
    if (n00b_type_is_box(type) || n00b_type_is_value_type(type)) {
        gen_load_immediate(ctx, n00b_unbox(obj));
        return true;
    }

    return false;
}

static inline void
gen_load_const_obj(gen_ctx *ctx, n00b_obj_t obj)
{
    n00b_type_t *type = n00b_get_my_type(obj);

    if (unbox_const_value(ctx, obj, type)) {
        return;
    }

    uint32_t offset = n00b_layout_const_obj(ctx->cctx, obj);

    emit(ctx,
         N00B_ZPushConstObj,
         n00b_kw("arg", n00b_ka(offset), "type", n00b_ka(type)));
    return;
}

static void
gen_load_const_by_offset(gen_ctx *ctx, uint32_t offset, n00b_type_t *type)
{
    emit(ctx,
         N00B_ZPushConstObj,
         n00b_kw("arg", n00b_ka(offset), "type", n00b_ka(type)));
}

static inline void
gen_sym_load_const(gen_ctx *ctx, n00b_symbol_t *sym, bool addressof)
{
    n00b_type_t *type = sym->type;

    if (unbox_const_value(ctx, sym->value, type)) {
        return;
    }
    emit(ctx,
         addressof ? N00B_ZPushConstRef : N00B_ZPushConstObj,
         n00b_kw("arg", n00b_ka(sym->static_offset), "type", n00b_ka(type)));
}

static inline void
gen_sym_load_attr(gen_ctx *ctx, n00b_symbol_t *sym, bool addressof)
{
    int64_t offset = n00b_add_static_string(sym->name, ctx->cctx);

    if (!addressof) {
        emit(ctx,
             N00B_ZPushConstObj,
             n00b_kw("arg", n00b_ka(offset), "type", n00b_ka(sym->type)));
        emit(ctx,
             N00B_ZLoadFromAttr,
             n00b_kw("arg", n00b_ka(addressof), "type", n00b_ka(sym->type)));
    }
    else {
        emit(ctx,
             N00B_ZPushConstObj,
             n00b_kw("arg", n00b_ka(offset), "type", n00b_ka(sym->type)));
        ctx->assign_method = assign_to_attribute;
    }
}

// Right now we only ever generate the version that returns the rhs
// not the attr storage address.
static inline void
gen_sym_load_attr_and_found(gen_ctx *ctx, n00b_symbol_t *sym, bool skipload)
{
    int64_t flag   = skipload ? N00B_F_ATTR_SKIP_LOAD : N00B_F_ATTR_PUSH_FOUND;
    int64_t offset = n00b_add_static_string(sym->name, ctx->cctx);

    emit(ctx,
         N00B_ZPushConstObj,
         n00b_kw("arg", n00b_ka(offset), "type", n00b_ka(sym->type)));
    emit(ctx, N00B_ZLoadFromAttr, n00b_kw("immediate", n00b_ka(flag)));
}

static inline void
gen_sym_load_stack(gen_ctx *ctx, n00b_symbol_t *sym, bool addressof)
{
    // This is measured in stack value slots.
    if (addressof) {
        emit(ctx, N00B_ZPushLocalRef, n00b_kw("arg", n00b_ka(sym->static_offset)));
    }
    else {
        emit(ctx, N00B_ZPushLocalObj, n00b_kw("arg", n00b_ka(sym->static_offset)));
    }
}

static inline void
gen_sym_load_static(gen_ctx *ctx, n00b_symbol_t *sym, bool addressof)
{
    // This is measured in 64-bit value slots.
    if (addressof) {
        emit(ctx,
             N00B_ZPushStaticRef,
             n00b_kw("arg",
                     n00b_ka(sym->static_offset),
                     "module_id",
                     n00b_ka(sym->local_module_id)));
    }
    else {
        emit(ctx,
             N00B_ZPushStaticObj,
             n00b_kw("arg",
                     n00b_ka(sym->static_offset),
                     "module_id",
                     n00b_ka(sym->local_module_id)));
    }
}

// Load from the storage location referred to by the symbol,
// pushing onto onto the stack.
//
// Or, optionally, push the address of the symbol.
static void
gen_sym_load(gen_ctx *ctx, n00b_symbol_t *sym, bool addressof)
{
    switch (sym->kind) {
    case N00B_SK_ENUM_VAL:
        gen_sym_load_const(ctx, sym, addressof);
        return;
    case N00B_SK_ATTR:
        gen_sym_load_attr(ctx, sym, addressof);
        return;
    case N00B_SK_VARIABLE:
        // clang-format off
        switch (sym->flags & (N00B_F_DECLARED_CONST |
			      N00B_F_USER_IMMUTIBLE |
			      N00B_F_REGISTER_STORAGE)) {
        // clang-format on
        case N00B_F_DECLARED_CONST:
            gen_sym_load_const(ctx, sym, addressof);
            return;

            // If it's got the user-immutible flag set it's a loop-related
            // automatic variable like $i or $last.
        case N00B_F_USER_IMMUTIBLE:
            gen_sym_load_stack(ctx, sym, addressof);
            return;

        case N00B_F_REGISTER_STORAGE:
            n00b_assert(!addressof);
            emit(ctx, N00B_ZPushFromR0);
            return;

        default:
            // Regular variables in a function are stack allocated.
            // Regular variables in a module are statically allocated.
            if (sym->flags & N00B_F_FUNCTION_SCOPE) {
                gen_sym_load_stack(ctx, sym, addressof);
            }
            else {
                gen_sym_load_static(ctx, sym, addressof);
            }
            return;
        }

    case N00B_SK_FORMAL:
        gen_sym_load_stack(ctx, sym, addressof);
        return;
    default:
        n00b_unreachable();
    }
}

// Couldn't resist the variable name. For variables, if true, the flag
// pops the value stored off the stack (that always happens for attributes),
// and for attributes, the boolean locks the attribute.

static void
gen_sym_store(gen_ctx *ctx, n00b_symbol_t *sym, bool pop_and_lock)
{
    int64_t arg;
    switch (sym->kind) {
    case N00B_SK_ATTR:
        // Byte offset into the const object arena where the attribute
        // name can be found.
        arg = n00b_add_static_string(sym->name, ctx->cctx);
        gen_load_const_by_offset(ctx, arg, n00b_type_utf8());

        emit(ctx, N00B_ZAssignAttr, n00b_kw("arg", n00b_ka(pop_and_lock)));
        return;
    case N00B_SK_VARIABLE:
    case N00B_SK_FORMAL:
        if (!pop_and_lock) {
            emit(ctx, N00B_ZDupTop);
        }

        if (sym->flags & N00B_F_REGISTER_STORAGE) {
            emit(ctx, N00B_ZPopToR0);
            break;
        }

        gen_sym_load(ctx, sym, true);
        emit(ctx, N00B_ZAssignToLoc);
        return;
    default:
        n00b_unreachable();
    }
}

static inline void
gen_load_string(gen_ctx *ctx, n00b_utf8_t *s)
{
    int64_t offset = n00b_add_static_string(s, ctx->cctx);
    emit(ctx, N00B_ZPushConstObj, n00b_kw("arg", n00b_ka(offset)));
}

static void
gen_bail(gen_ctx *ctx, n00b_utf8_t *s)
{
    gen_load_string(ctx, s);
    emit(ctx, N00B_ZBail);
}

static bool
gen_label(gen_ctx *ctx, n00b_utf8_t *s)
{
    if (s == NULL) {
        return false;
    }

    int64_t offset = n00b_add_static_string(s, ctx->cctx);
    emit(ctx, N00B_ZNop, n00b_kw("arg", n00b_ka(1), "immediate", n00b_ka(offset)));

    return true;
}

static inline void
gen_kids(gen_ctx *ctx)
{
    n00b_tree_node_t *saved = ctx->cur_node;

    for (int i = 0; i < saved->num_kids; i++) {
        ctx->cur_node = saved->children[i];
        gen_one_node(ctx);
    }

    ctx->cur_node = saved;
}

static inline void
gen_one_kid(gen_ctx *ctx, int n)
{
    n00b_tree_node_t *saved = ctx->cur_node;

    n00b_assert(saved);

    // TODO: Remove this when codegen is done.
    if (saved->num_kids <= n) {
        return;
    }

    ctx->cur_node = saved->children[n];

    gen_one_node(ctx);

    ctx->cur_node = saved;
}

static inline void
possible_restore_from_r3(gen_ctx *ctx, bool restore)
{
    if (restore) {
        emit(ctx, N00B_ZPushFromR3);
    }
}

// Helpers above, implementations below.
static inline void
gen_test_param_flag(gen_ctx                  *ctx,
                    n00b_module_param_info_t *param,
                    n00b_symbol_t            *sym)
{
    uint64_t index = param->param_index / 64;
    uint64_t flag  = 1 << (param->param_index % 64);

    emit(ctx, N00B_ZPushStaticObj, n00b_kw("arg", n00b_ka(index)));
    gen_load_immediate(ctx, flag);
    emit(ctx, N00B_ZBAnd); // Will be non-zero if the param is set.
}

static inline void
gen_param_via_default_value_type(gen_ctx                  *ctx,
                                 n00b_module_param_info_t *param,
                                 n00b_symbol_t            *sym)
{
    GEN_JNZ(gen_load_const_obj(ctx, param->default_value);
            gen_sym_store(ctx, sym, true));
}

static inline void
gen_param_via_default_ref_type(gen_ctx                  *ctx,
                               n00b_module_param_info_t *param,
                               n00b_symbol_t            *sym)
{
    uint32_t     offset = n00b_layout_const_obj(ctx->cctx, param->default_value);
    n00b_type_t *t      = n00b_get_my_type(param->default_value);

    GEN_JNZ(gen_load_const_by_offset(ctx, offset, t);
            gen_tcall(ctx, N00B_BI_COPY, t);
            gen_sym_store(ctx, sym, true));
}

static inline void
gen_run_internal_callback(gen_ctx *ctx, n00b_callback_info_t *cb)
{
    // This might go away soon.
    uint32_t     offset   = n00b_layout_const_obj(ctx->cctx, cb);
    n00b_type_t *t        = cb->target_type;
    int          nargs    = n00b_type_get_num_params(t) - 1;
    n00b_type_t *ret_type = n00b_type_get_param(t, nargs);
    bool         useret   = !(n00b_types_are_compat(ret_type,
                                          n00b_type_void(),
                                          NULL));

    gen_load_const_by_offset(ctx, offset, n00b_get_my_type(cb));
    emit(ctx,
         N00B_ZRunCallback,
         n00b_kw("arg", n00b_ka(nargs), "immediate", n00b_ka(useret)));

    if (nargs) {
        emit(ctx, N00B_ZMoveSp, n00b_kw("arg", n00b_ka(-1 * nargs)));
    }

    if (useret) {
        emit(ctx, N00B_ZPopToR0);
    }
}

static inline void
gen_box_if_needed(gen_ctx          *ctx,
                  n00b_tree_node_t *n,
                  n00b_symbol_t    *sym,
                  int               ix)
{
    n00b_pnode_t *pn          = n00b_get_pnode(n);
    n00b_type_t  *actual_type = n00b_type_resolve(pn->type);
    n00b_type_t  *param_type  = n00b_type_get_param(sym->type, ix);

    if (n00b_type_is_box(actual_type)) {
        actual_type = n00b_type_unbox(actual_type);
    }
    else {
        if (!n00b_type_is_value_type(actual_type)) {
            return;
        }
    }

    // We've already type checked, so we can play fast and loose
    // with the test here.
    if (!n00b_type_is_value_type(param_type)) {
        emit(ctx, N00B_ZBox, n00b_kw("type", n00b_ka(actual_type)));
    }
}
static inline void
gen_unbox_if_needed(gen_ctx          *ctx,
                    n00b_tree_node_t *n,
                    n00b_symbol_t    *sym)
{
    n00b_pnode_t *pn          = n00b_get_pnode(n);
    n00b_type_t  *actual_type = n00b_type_resolve(pn->type);
    int           ix          = n00b_type_get_num_params(sym->type) - 1;
    n00b_type_t  *param_type  = n00b_type_get_param(sym->type, ix);

    if (n00b_type_is_box(actual_type)) {
        actual_type = n00b_type_unbox(actual_type);
    }
    else {
        if (!n00b_type_is_value_type(actual_type)) {
            return;
        }
    }

    // We've already type checked, so we can play fast and loose
    // with the test here.
    if (!n00b_type_is_value_type(param_type)) {
        emit(ctx, N00B_ZUnbox, n00b_kw("type", n00b_ka(actual_type)));
    }
}

static void
gen_native_call(gen_ctx *ctx, n00b_symbol_t *fsym)
{
    int n = ctx->cur_node->num_kids;

    for (int i = 1; i < n; i++) {
        gen_one_kid(ctx, i);
        gen_box_if_needed(ctx, ctx->cur_node->children[i], fsym, i - 1);
    }

    // Needed to calculate the loc of module variables.
    // Currently that's done at runtime, tho could be done in
    // a proper link pass in the future.
    int             target_module;
    // Index into the object file's cache.
    int             target_fn_id;
    int             loc  = ctx->instruction_counter;
    // When the call is generated, we might not have generated the
    // function we're calling. In that case, we will just generate
    // a stub and add the actual call instruction to a backpatch
    // list that gets processed at the end of compilation.
    //
    // To test this reliably, we can check the 'offset' field of
    // the function info object, as a function never starts at
    // offset 0.
    n00b_fn_decl_t *decl = fsym->value;
    target_module        = decl->module_id;
    target_fn_id         = decl->local_id;

    emit(ctx,
         N00B_Z0Call,
         n00b_kw("arg", n00b_ka(target_fn_id), "module_id", target_module));

    if (target_fn_id == 0) {
        n00b_call_backpatch_info_t *bp;

        bp       = new_backpatch();
        bp->decl = decl;
        bp->i    = n00b_list_get(ctx->instructions, loc, NULL);

        n00b_list_append(ctx->call_backpatches, bp);
    }

    n = decl->signature_info->num_params;

    if (n != 0) {
        emit(ctx, N00B_ZMoveSp, n00b_kw("arg", n00b_ka(-n)));
    }

    if (!decl->signature_info->void_return) {
        emit(ctx, N00B_ZPushFromR0);
        gen_unbox_if_needed(ctx, ctx->cur_node, fsym);
    }
}

static void
gen_extern_call(gen_ctx *ctx, n00b_symbol_t *fsym)
{
    n00b_ffi_decl_t *decl = (n00b_ffi_decl_t *)fsym->value;
    int              n    = ctx->cur_node->num_kids;

    for (int i = 1; i < n; i++) {
        gen_one_kid(ctx, i);
        if (!decl->skip_boxes) {
            gen_box_if_needed(ctx, ctx->cur_node->children[i], fsym, i - 1);
        }
    }

    emit(ctx, N00B_ZFFICall, n00b_kw("arg", n00b_ka(decl->global_ffi_call_ix)));

    if (decl->num_ext_params != 0) {
        emit(ctx, N00B_ZMoveSp, n00b_kw("arg", n00b_ka(-decl->num_ext_params)));
    }

    if (!decl->local_params->void_return) {
        emit(ctx, N00B_ZPushFromR0);
        if (!decl->skip_boxes) {
            gen_unbox_if_needed(ctx, ctx->cur_node, fsym);
        }
    }
}

static void
gen_run_callback(gen_ctx *ctx, n00b_call_resolution_info_t *info)
{
    // We're going to use r3 to keep the callback object where we need
    // it as we push on arguments.  But since it might be in use, we
    // need to spill, and then restore when the call returns.
    //
    // Note that ZRunCallback will have to dynamically handle boxing
    // and unboxing if needed.

    emit(ctx, N00B_ZPushFromR3);
    gen_one_kid(ctx, 0);
    emit(ctx, N00B_ZPopToR3);

    n00b_tree_node_t *call_node = ctx->cur_node;
    n00b_pnode_t     *pnode     = n00b_get_pnode(call_node);
    n00b_type_t      *rettype   = pnode->type;
    int               n         = call_node->num_kids;
    bool              use_ret   = (!n00b_types_are_compat(rettype,
                                           n00b_type_void(),
                                           NULL));
    n00b_list_t      *params    = n00b_list(n00b_type_typespec());

    for (int i = 1; i < n; i++) {
        pnode = n00b_get_pnode(call_node->children[i]);
        n00b_list_append(params, pnode->type);
        gen_one_kid(ctx, i);
    }

    n00b_type_t *sig = n00b_type_fn(rettype, params, false);

    emit(ctx, N00B_ZPushFromR3);
    emit(ctx,
         N00B_ZRunCallback,
         n00b_kw("arg",
                 n00b_ka(n - 1),
                 "immediate",
                 n00b_ka(use_ret),
                 "type",
                 n00b_ka(sig)));

    if (n > 1) {
        emit(ctx, N00B_ZMoveSp, n00b_kw("arg", n00b_ka(-1 * (n - 1))));
    }

    // Restore the saved register;
    emit(ctx, N00B_ZPopToR3);
    if (use_ret) {
        emit(ctx, N00B_ZPushFromR0);
    }
}

static void
gen_call(gen_ctx *ctx)
{
    n00b_call_resolution_info_t *info = ctx->cur_pnode->extra_info;

    if (info->callback) {
        gen_run_callback(ctx, info);
        return;
    }

    n00b_symbol_t *fsym = info->resolution;

    // Pushes arguments onto the stack.
    if (fsym->kind != N00B_SK_FUNC) {
        gen_extern_call(ctx, fsym);
    }
    else {
        gen_native_call(ctx, fsym);
    }
}

static void
gen_ret(gen_ctx *ctx)
{
    if (ctx->cur_node->num_kids != 0) {
        gen_kids(ctx);
        emit(ctx, N00B_ZPopToR0);
    }
    else {
        if (ctx->retsym) {
            gen_sym_load(ctx, ctx->retsym, false);
            emit(ctx, N00B_ZPopToR0);
        }
    }

    emit(ctx, N00B_ZRet);
}

static inline void
gen_param_via_callback(gen_ctx                  *ctx,
                       n00b_module_param_info_t *param,
                       n00b_symbol_t            *sym)
{
    // TODO: this needs to be redone.
    gen_run_internal_callback(ctx, param->callback);
    // The third parameter gives 'false' for attrs (where the
    // parameter would cause the attribute to lock) and 'true' for
    // variables, which leads to the value being popped after stored
    // (which happens automatically w/ the attr).
    gen_sym_store(ctx, sym, sym->kind != N00B_SK_ATTR);
    // TODO: call the validator too.
}

static inline void
gen_param_bail_if_missing(gen_ctx *ctx, n00b_symbol_t *sym)
{
    n00b_utf8_t *err = n00b_cstr_format(
        "Required parameter [em]{}[/] didn't have a value when "
        "entering module [em]{}[/].",
        sym->name,
        ctx->fctx->path);
    GEN_JNZ(gen_bail(ctx, err));
}

static inline uint64_t
gen_parameter_checks(gen_ctx *ctx)
{
    uint64_t n;
    void   **view = hatrack_dict_values_sort(ctx->fctx->parameters, &n);

    for (unsigned int i = 0; i < n; i++) {
        n00b_module_param_info_t *param = view[i];
        n00b_symbol_t            *sym   = param->linked_symbol;

        if (sym->kind == N00B_SK_ATTR) {
            gen_sym_load_attr_and_found(ctx, sym, true);
        }
        else {
            gen_test_param_flag(ctx, param, sym);
        }

        // Now, there's always an item on the top of the stack that tells
        // us if this parameter is loaded. We have to test, and generate
        // the right code if it's not loaded.
        //
        // If there's a default value, value types directly load from a
        // marshal'd location in const-land; ref types load by copying the
        // const.
        if (param->have_default) {
            if (n00b_type_is_value_type(sym->type)) {
                gen_param_via_default_value_type(ctx, param, sym);
            }
            else {
                gen_param_via_default_ref_type(ctx, param, sym);
            }
        }
        else {
            // If there's no default, the callback allows us to generate
            // dynamically if needed.
            if (param->callback) {
                gen_param_via_callback(ctx, param, sym);
            }

            // If neither default nor callback is provided, if a param
            // isn't set, we simply bail.
            gen_param_bail_if_missing(ctx, sym);
        }
    }

    return n;
}

static inline void
gen_module(gen_ctx *ctx)
{
    gen_label(ctx, n00b_cstr_format("Module '{}': ", ctx->fctx->path));

    int num_params = gen_parameter_checks(ctx);

    emit(ctx, N00B_ZModuleEnter, n00b_kw("arg", n00b_ka(num_params)));
    ctx->module_patch_loc = ctx->instruction_counter;
    emit(ctx, N00B_ZMoveSp, n00b_kw("arg", n00b_ka(0)));
    gen_kids(ctx);
}

static inline void
gen_elif(gen_ctx *ctx)
{
    gen_one_kid(ctx, 0);
    GEN_JZ(gen_one_kid(ctx, 1);
           gen_j(ctx, &ctx->target_info->targets[ctx->target_info->next++]));
}

static inline void
gen_if(gen_ctx *ctx)
{
    target_info_t end_info = {
        .targets = n00b_gc_array_alloc(n00b_jump_info_t, ctx->cur_node->num_kids),
        .next    = 0,
    };

    target_info_t *saved_info = ctx->target_info;
    ctx->target_info          = &end_info;

    gen_one_kid(ctx, 0);
    GEN_JZ(gen_one_kid(ctx, 1);
           gen_j(ctx, &ctx->target_info->targets[ctx->target_info->next++]));
    for (int i = 2; i < ctx->cur_node->num_kids; i++) {
        gen_one_kid(ctx, i);
    }

    for (int i = 0; i < ctx->target_info->next; i++) {
        n00b_jump_info_t *one_patch = &end_info.targets[i];

        gen_finish_jump(ctx, one_patch);
    }

    ctx->target_info = saved_info;
}

static inline void
gen_one_tcase(gen_ctx *ctx, n00b_control_info_t *switch_exit)
{
    int               num_conditions = ctx->cur_node->num_kids - 1;
    n00b_jump_info_t *local_jumps    = n00b_gc_array_alloc(n00b_jump_info_t,
                                                        num_conditions);
    n00b_jump_info_t *exit_jump      = n00b_new_jump_info();
    n00b_jump_info_t *case_end       = n00b_new_jump_info();

    exit_jump->linked_control_structure = switch_exit;

    for (int i = 0; i < num_conditions; i++) {
        emit(ctx, N00B_ZDupTop);

        // We stashed the type in the `nt_case` node's value field during
        // the check pass for easy access.
        n00b_pnode_t *pnode = n00b_get_pnode(ctx->cur_node->children[i]);
        gen_load_const_obj(ctx, pnode->value);
        emit(ctx, N00B_ZTypeCmp);

        gen_jnz(ctx, &local_jumps[i], true);
    }
    gen_j(ctx, case_end);
    for (int i = 0; i < num_conditions; i++) {
        gen_finish_jump(ctx, &local_jumps[i]);
    }

    gen_one_kid(ctx, num_conditions);
    gen_j(ctx, exit_jump);
    gen_finish_jump(ctx, case_end);
}

static inline void
gen_typeof(gen_ctx *ctx)
{
    n00b_tree_node_t    *id_node;
    n00b_pnode_t        *id_pn;
    n00b_symbol_t       *sym;
    n00b_tree_node_t    *n           = ctx->cur_node;
    n00b_pnode_t        *pnode       = n00b_get_pnode(n);
    n00b_loop_info_t    *ci          = pnode->extra_info;
    int                  expr_ix     = 0;
    n00b_control_info_t *switch_exit = &ci->branch_info;

    if (gen_label(ctx, ci->branch_info.label)) {
        expr_ix++;
    }

    id_node = n00b_get_match_on_node(n->children[expr_ix], n00b_id_node)->parent;
    id_pn   = n00b_get_pnode(id_node);
    sym     = id_pn->extra_info;

    gen_sym_load(ctx, sym, false);

    // pops the variable due to the arg, leaving just the type,
    // which we will dupe for each test.
    emit(ctx, N00B_ZPushObjType);

    // We could elide some dup/pop and jmp instructions here, but for
    // now, let's keep it simple.
    for (int i = expr_ix + 1; i < n->num_kids; i++) {
        n00b_tree_node_t *kid = n->children[i];
        pnode                 = n00b_get_pnode(kid);

        ctx->cur_node = kid;
        if (i + 1 != n->num_kids || pnode->kind != n00b_nt_else) {
            gen_one_tcase(ctx, switch_exit);
        }
        else {
            gen_one_node(ctx);
        }
    }
    gen_apply_waiting_patches(ctx, switch_exit);
}

static inline void
gen_range_test(gen_ctx *ctx, n00b_type_t *type)
{
    // The range is already on the stack. The top will be the high,
    // the low second, and the value to test first.
    //
    // The test value will have been duped once, but not twice. So we:
    //
    // 1. Pop the upper end of the range.
    // 2. Run GTE test.
    // 3. If successful, dupe, push the other item, and leave the test result
    //    as the definitive answer for the JNZ.

    emit(ctx, N00B_ZPopToR0);
    if (n00b_type_is_signed(type)) {
        emit(ctx, N00B_ZGte);
    }
    else {
        emit(ctx, N00B_ZUGte);
    }

    GEN_JZ(emit(ctx, N00B_ZDupTop);
           emit(ctx, N00B_ZPushFromR0);
           emit(ctx, N00B_ZLte););
}

static inline void
gen_one_case(gen_ctx *ctx, n00b_control_info_t *switch_exit, n00b_type_t *type)
{
    n00b_tree_node_t *n              = ctx->cur_node;
    int               num_conditions = n->num_kids - 1;
    n00b_jump_info_t *local_jumps    = n00b_gc_array_alloc(n00b_jump_info_t,
                                                        num_conditions);
    n00b_jump_info_t *case_end       = n00b_new_jump_info();
    n00b_jump_info_t *exit_jump      = n00b_new_jump_info();

    exit_jump->linked_control_structure = switch_exit;

    for (int i = 0; i < num_conditions; i++) {
        emit(ctx, N00B_ZDupTop);
        gen_one_kid(ctx, i);
        n00b_pnode_t *pn = n00b_get_pnode(ctx->cur_node->children[i]);
        if (pn->kind == n00b_nt_range) {
            gen_range_test(ctx, type);
        }
        else {
            gen_equality_test(ctx, type);
        }

        gen_jnz(ctx, &local_jumps[i], true);
    }

    // If we've checked all the conditions, and nothing matched, we jump down
    // to the next case.
    gen_j(ctx, case_end);
    // Now, we backpatch all the local jumps to our current location.
    for (int i = 0; i < num_conditions; i++) {
        gen_finish_jump(ctx, &local_jumps[i]);
    }

    gen_one_kid(ctx, num_conditions);
    // Once the kid runs, jump to the switch exit.

    gen_j(ctx, exit_jump);

    // Now here's the start of the next case, if it exists, so
    // backpatch the jump to the case end.
    gen_finish_jump(ctx, case_end);
}

static inline void
gen_switch(gen_ctx *ctx)
{
    int                  expr_ix     = 0;
    n00b_tree_node_t    *n           = ctx->cur_node;
    n00b_pnode_t        *pnode       = n00b_get_pnode(n);
    n00b_loop_info_t    *ci          = pnode->extra_info;
    n00b_control_info_t *switch_exit = &ci->branch_info;
    n00b_type_t         *expr_type;

    if (gen_label(ctx, ci->branch_info.label)) {
        expr_ix++;
    }

    // Get the value to test to the top of the stack.
    gen_one_kid(ctx, expr_ix++);
    pnode     = n00b_get_pnode(n->children[expr_ix]);
    expr_type = pnode->type;

    int               n_cases       = n->num_kids - expr_ix;
    n00b_tree_node_t *possible_else = n->children[n->num_kids - 1];
    n00b_pnode_t     *kidpn         = n00b_get_pnode(possible_else);
    bool              have_else     = kidpn->kind == n00b_nt_else;

    if (have_else) {
        n_cases -= 1;
    }

    int ix = expr_ix;

    for (int i = 0; i < n_cases; i++) {
        ctx->cur_node = n->children[ix++];
        gen_one_case(ctx, switch_exit, expr_type);
    }

    if (have_else) {
        // No branch matched. Pop the value we were testing against, then
        // run the else block, if any.
        emit(ctx, N00B_ZPop);

        if (have_else) {
            ctx->cur_node = n;
            gen_one_kid(ctx, n->num_kids - 1);
        }
    }
    gen_apply_waiting_patches(ctx, switch_exit);
}

static inline bool
gen_index_var_init(gen_ctx *ctx, n00b_loop_info_t *li)
{
    if (li->loop_ix && n00b_list_len(li->loop_ix->ct->sym_uses) > 0) {
        li->gen_ix = 1;
        set_stack_offset(ctx, li->loop_ix);
    }
    if (li->named_loop_ix && n00b_list_len(li->named_loop_ix->ct->sym_uses) > 0) {
        li->gen_named_ix = 1;
        if (!li->gen_ix) {
            set_stack_offset(ctx, li->named_loop_ix);
        }
    }

    if (!(li->gen_ix | li->gen_named_ix)) {
        return false;
    }

    if (li->gen_ix && li->gen_named_ix) {
        n00b_assert(li->loop_ix == li->named_loop_ix);
    }

    gen_load_immediate(ctx, 0);

    // The value gets stored to the symbol at the beginning of the loop.
    return true;
}

static inline void
gen_len_var_init(gen_ctx *ctx, n00b_loop_info_t *li)
{
    n00b_symbol_t *sym = NULL;

    if (li->loop_last && n00b_list_len(li->loop_last->ct->sym_uses) > 0) {
        sym = li->loop_last;
    }
    if (li->named_loop_last && n00b_list_len(li->named_loop_last->ct->sym_uses) > 0) {
        if (sym != NULL) {
            n00b_assert(sym == li->named_loop_last);
        }
        else {
            sym = li->named_loop_last;
        }
    }
    if (sym) {
        set_stack_offset(ctx, sym);
        gen_sym_store(ctx, sym, false);
    }
}

static inline bool
gen_index_var_increment(gen_ctx *ctx, n00b_loop_info_t *li)
{
    if (!(li->gen_ix | li->gen_named_ix)) {
        return false;
    }

    if (li->gen_ix) {
        gen_sym_store(ctx, li->loop_ix, false);
    }
    if (li->gen_named_ix) {
        gen_sym_store(ctx, li->named_loop_ix, false);
    }
    // Index var would be on the stack here; the add will clobber it,
    // making this a +=.
    gen_load_immediate(ctx, 1);
    emit(ctx, N00B_ZAdd);
    return true;
}

static inline void
gen_index_var_cleanup(gen_ctx *ctx, n00b_loop_info_t *li)
{
    if (!(li->gen_ix | li->gen_named_ix)) {
        return;
    }

    emit(ctx, N00B_ZPop);
}

static inline void
set_loop_entry_point(gen_ctx *ctx, n00b_loop_info_t *li)
{
    li->branch_info.entry_ip = ctx->instruction_counter;
}

static inline void
store_view_item(gen_ctx *ctx, n00b_loop_info_t *li)
{
    gen_sym_store(ctx, li->lvar_1, true);
    if (li->lvar_2 != NULL) {
        gen_sym_store(ctx, li->lvar_2, true);
    }
}

static inline void
deal_with_iteration_count(gen_ctx          *ctx,
                          n00b_loop_info_t *li,
                          bool              have_index_var)
{
    if (have_index_var) {
        gen_index_var_increment(ctx, li);
    }
    else {
        gen_load_immediate(ctx, 1);
        emit(ctx, N00B_ZAdd);
    }
}

static inline void
gen_ranged_for(gen_ctx *ctx, n00b_loop_info_t *li)
{
    n00b_jump_info_t ji_top = {
        .linked_control_structure = &li->branch_info,
        .top                      = true,
    };
    // In a ranged for, the left value got pushed on, then the right
    // value. So if it's 0 .. 10, and we call subtract to calculate the
    // length, we're going to be computing 0 - 10, not the opposite.
    // So we first swap the two numbers.

    emit(ctx, N00B_ZSwap);
    // Subtract the 2 numbers now, but DO NOT POP; we need these.
    emit(ctx, N00B_ZSubNoPop);

    bool calc_last       = false;
    bool calc_named_last = false;

    if (li->loop_last && n00b_list_len(li->loop_last->ct->sym_uses) > 0) {
        calc_last = true;
    }
    if (li->named_loop_last && n00b_list_len(li->named_loop_last->ct->sym_uses) > 0) {
        calc_named_last = true;
    }
    if (calc_last || calc_named_last) {
        emit(ctx, N00B_ZDupTop);
        emit(ctx, N00B_ZAbs);

        if (calc_last && calc_named_last) {
            emit(ctx, N00B_ZDupTop);
        }

        if (calc_last) {
            gen_sym_store(ctx, li->loop_last, true);
        }
        if (calc_named_last) {
            gen_sym_store(ctx, li->named_loop_last, true);
        }
    }
    // Now, we have the number of iterations on top, but we actually
    // want this to be either 1 or -1, depending on what we have to add
    // to the count for each loop. So we use a magic instruction to
    // convert the difference into a 1 or -1.
    emit(ctx, N00B_ZGetSign);
    //
    // Now our loop is set up, with the exception of any possible iteration
    // variable. When we're using one, we'll leave it on top of the stack.
    gen_index_var_init(ctx, li);

    // Now we're ready for the loop!
    set_loop_entry_point(ctx, li);
    bool using_index = gen_index_var_increment(ctx, li);
    if (using_index) {
        emit(ctx, N00B_ZPopToR3);
    }

    // pop the step for a second.
    emit(ctx, N00B_ZPopToR1);
    // If the two items are equal, we bail from the loop.
    emit(ctx, N00B_ZCmpNoPop);

    // Pop the step back to add it;
    // Store a copy of the result w/o popping.
    // Then push the step back on,
    // And, if it's being used, the $i from R3.

    GEN_JNZ(
        gen_sym_store(ctx, li->lvar_1, false);
        emit(ctx, N00B_ZPushFromR1);
        emit(ctx, N00B_ZAdd);
        emit(ctx, N00B_ZPushFromR1);
        possible_restore_from_r3(ctx, using_index);
        gen_one_kid(ctx, ctx->cur_node->num_kids - 1);
        gen_j(ctx, &ji_top));
    gen_apply_waiting_patches(ctx, &li->branch_info);
    emit(ctx, N00B_ZMoveSp, n00b_kw("arg", n00b_ka(-3)));
}

static inline void
gen_container_for(gen_ctx *ctx, n00b_loop_info_t *li)
{
    n00b_jump_info_t ji_top = {
        .linked_control_structure = &li->branch_info,
        .top                      = true,
    };
    // In between iterations we will push these items on...
    //   sp     -------> Size of container
    //   sp + 1 -------> Iteration count
    //   sp + 2 -------> Number of bytes in the item; we will
    //                   advance the view this many bytes each
    //                   iteration (except, see below).
    //   sp + 3 -------> Pointer to Container View
    //
    // We always create a view the container before calculating the
    // size and iterating on it. This will
    //
    // After calculating the size, we set $last / label$last if
    // appropriate.
    //
    // At the top of each loop, the stack should look like the above.
    // Before we test, we will, in this order:

    // 1. Store the sp in $i and/or label$i, if those variables are used.
    // 2. Compare the index variable using ZGteNoPop (lhs is the container size)
    //    This instruction is explicitly used in for loops, keeping
    //    our index variable and i
    // 3. JZ out of the loop if the comparison returns false.
    // 4. Increment the index variable (again at stack top) by 1 for
    //    the sake of the next iteration (we don't do this at the bottom
    //    so that we don't have to compilicate `continue`.
    // 5. load the item at the container view into the loop variable.
    // 6. If there are two loop variables (dict), do the same thing again.
    //
    // We use two bits of magic here:
    // 1. The GTE check auto-pops to registers.
    // 2. The ZLoadFromView instruction assumes the item size is at the
    //    top slot, and the view in the second slot. It fetches the
    //    next item and pushes it automatically.
    //    If the view object is a bitfield, the iteration count is//    required to be in register 1 (necessary for calculating
    //    which bit to push and when to shift the view pointer).
    //
    // Also, note that the VIEW builtin doesn't need to copy objects,
    // but it needs to give us a pointer to items, where we steal the
    // lowest 3 bits in the pointer to represent the the log base 2 of
    // the bytes in the item count.  Currently, the only allowable
    // view item sizes are 1 byte, 2 bytes, 4 bytes, 8 bytes. Anything
    // else currently represents a bitfield (per below).
    //
    // Currently, we don't need more than 8 bytes, since anything
    // larger than 8 bytes is passed around as pointers. However, I
    // expect to eventually add 128-bit ints, which would entail a
    // double-word load.
    //
    // We have a bit of a special case for bitfield types (right now,
    // just n00b_flags_t). There, the value of the "number of bytes"
    // field will be set to 128; When we see that, we will use
    // different code that only advances the pointer 8 bytes every 64
    // iterations.
    //
    // There are cases this test might need to be dynamic, so for ease
    // of first implementation, it will just always happen, even
    // though in most cases it should be no problem to bind to the
    // right approach statically.
    gen_tcall(ctx, N00B_BI_VIEW, NULL);

    // The length of the container is on top right now; the view is
    // underneath.  We want to store a copy to $len if appropriate,
    // and then pop to a register 2, to work on the pointer. We'll
    // push it back on top when we properly enter the loop.
    gen_len_var_init(ctx, li);

    // Move the container length out to a register for a bit.
    emit(ctx, N00B_ZPopToR2);

    emit(ctx, N00B_ZUnsteal);
    // The bit length is actually encoded by taking log base 2; here
    // were expand it back out before going into the loop.
    emit(ctx, N00B_ZShlI, n00b_kw("arg", n00b_ka(0x1)));

    // On top of this, put the iteration count, which at the start of
    // each turn through the loop, will be the second item.

    bool have_index_var = gen_index_var_init(ctx, li);
    bool have_kv_pair   = li->lvar_2 != NULL;

    if (!have_index_var) {
        gen_load_immediate(ctx, 0x0);
    }
    // Container length needs to be on the stack above the index count
    // at the start of the loop.
    emit(ctx, N00B_ZPushFromR2);

    // Top of loop actions:
    // 1. compare the top two items, without popping yet (we want to
    //    enter the loop with everything on the stack, so easiest not to
    //    pop until after the test.
    // 2. If the test is 1, it's time to exit the loop.
    // 3. Otherwise, we pop the container length into a register.
    // 4. Increment the index counter; If $i is used, store it.
    // 5. Push the index counter into a register.
    // 5. Call ZLoadFromView, which will load the top view item to the
    //    top of the stack, and advance the pointer as appropriate (leaving
    //    the previous two items in place).
    // 6. Push the top item out to its storage location.
    // 7. Restore the stack start state by putting R1 and R2 back on.
    // 8. Loop body!
    // 9. Back to top for testing.
    set_loop_entry_point(ctx, li);
    emit(ctx, N00B_ZGteNoPop);

    GEN_JNZ(emit(ctx, N00B_ZPopToR2);
            deal_with_iteration_count(ctx, li, have_index_var);
            emit(ctx, N00B_ZPopToR1);
            emit(ctx, N00B_ZLoadFromView, n00b_kw("arg", n00b_ka(have_kv_pair)));
            store_view_item(ctx, li);
            emit(ctx, N00B_ZPushFromR1); // Re-groom the stack; iter count
            emit(ctx, N00B_ZPushFromR2); // container len
            gen_one_kid(ctx, ctx->cur_node->num_kids - 1);
            gen_j(ctx, &ji_top));
    // After the loop:
    // 1. backpatch.
    // 2. Move the stack down four items, popping the count, len, item size,
    //    and container.
    gen_apply_waiting_patches(ctx, &li->branch_info);
    emit(ctx, N00B_ZMoveSp, n00b_kw("arg", n00b_ka(-5)));
}

static inline void
gen_for(gen_ctx *ctx)
{
    int               expr_ix     = 0;
    n00b_loop_info_t *li          = ctx->cur_pnode->extra_info;
    int               saved_stack = ctx->current_stack_offset;

    if (gen_label(ctx, li->branch_info.label)) {
        expr_ix++;
    }

    set_stack_offset(ctx, li->lvar_1);
    if (li->lvar_2) {
        set_stack_offset(ctx, li->lvar_2);
    }
    // Load either the range or the container we're iterating over.
    gen_one_kid(ctx, expr_ix + 1);

    if (li->ranged) {
        gen_ranged_for(ctx, li);
    }
    else {
        gen_container_for(ctx, li);
    }

    ctx->current_stack_offset = saved_stack;
}

static inline void
gen_while(gen_ctx *ctx)
{
    int               saved_stack = ctx->current_stack_offset;
    int               expr_ix     = 0;
    n00b_tree_node_t *n           = ctx->cur_node;
    n00b_pnode_t     *pnode       = n00b_get_pnode(n);
    n00b_loop_info_t *li          = pnode->extra_info;
    n00b_jump_info_t  ji_top      = {
              .linked_control_structure = &li->branch_info,
              .top                      = true,
    };

    gen_index_var_init(ctx, li);
    set_loop_entry_point(ctx, li);

    if (gen_label(ctx, li->branch_info.label)) {
        expr_ix++;
    }

    gen_one_kid(ctx, expr_ix);
    pnode = n00b_get_pnode(n->children[expr_ix]);

    // Condition for a loop is always a boolean. So top of the stack
    // after the condition is evaluated will be 0 when it's time to
    // exit.
    gen_load_immediate(ctx, 1);
    emit(ctx, N00B_ZCmp);
    GEN_JZ(gen_one_kid(ctx, expr_ix + 1);
           gen_index_var_increment(ctx, li);
           gen_j(ctx, &ji_top));
    gen_apply_waiting_patches(ctx, &li->branch_info);
    gen_index_var_cleanup(ctx, li);
    ctx->current_stack_offset = saved_stack;
}

static inline void
gen_break(gen_ctx *ctx)
{
    n00b_jump_info_t *ji = ctx->cur_pnode->extra_info;
    gen_j(ctx, ji);
}

static inline void
gen_continue(gen_ctx *ctx)
{
    n00b_jump_info_t *ji = ctx->cur_pnode->extra_info;
    gen_j(ctx, ji);
}

static inline void
gen_int_binary_op(gen_ctx *ctx, n00b_operator_t op, bool sign)
{
    n00b_zop_t zop;

    switch (op) {
    case n00b_op_plus:
        zop = sign ? N00B_ZAdd : N00B_ZUAdd;
        break;
    case n00b_op_minus:
        zop = sign ? N00B_ZSub : N00B_ZUSub;
        break;
    case n00b_op_mul:
        zop = sign ? N00B_ZMul : N00B_ZUMul;
        break;
    case n00b_op_div:
    case n00b_op_fdiv:
        zop = sign ? N00B_ZDiv : N00B_ZUDiv;
        break;
    case n00b_op_mod:
        zop = sign ? N00B_ZMod : N00B_ZUMod;
        break;
    case n00b_op_shl:
        zop = N00B_ZShl;
        break;
    case n00b_op_shr:
        zop = N00B_ZShr;
        break;
    case n00b_op_bitand:
        zop = N00B_ZBAnd;
        break;
    case n00b_op_bitor:
        zop = N00B_ZBOr;
        break;
    case n00b_op_bitxor:
        zop = N00B_ZBXOr;
        break;
    case n00b_op_lt:
        zop = sign ? N00B_ZLt : N00B_ZULt;
        break;
    case n00b_op_lte:
        zop = sign ? N00B_ZLte : N00B_ZULte;
        break;
    case n00b_op_gt:
        zop = sign ? N00B_ZGt : N00B_ZUGt;
        break;
    case n00b_op_gte:
        zop = sign ? N00B_ZGte : N00B_ZUGte;
        break;
    case n00b_op_eq:
        zop = N00B_ZCmp;
        break;
    case n00b_op_neq:
        zop = N00B_ZNeq;
        break;
    default:
        n00b_unreachable();
    }
    emit(ctx, zop);
}

static inline void
gen_polymorphic_binary_op(gen_ctx *ctx, n00b_operator_t op, n00b_type_t *t)
{
    // Todo: type check and restrict down to the set of posssible
    // types statically.

    switch (op) {
    case n00b_op_plus:
        gen_tcall(ctx, N00B_BI_ADD, t);
        break;
    case n00b_op_minus:
        gen_tcall(ctx, N00B_BI_SUB, t);
        break;
    case n00b_op_mul:
        gen_tcall(ctx, N00B_BI_MUL, t);
        break;
    case n00b_op_div:
        gen_tcall(ctx, N00B_BI_DIV, t);
        break;
    case n00b_op_mod:
        gen_tcall(ctx, N00B_BI_MOD, t);
        break;
    case n00b_op_lt:
        gen_tcall(ctx, N00B_BI_LT, t);
        break;
    case n00b_op_gt:
        gen_tcall(ctx, N00B_BI_GT, t);
        break;
    case n00b_op_eq:
        gen_tcall(ctx, N00B_BI_EQ, t);
        break;
    case n00b_op_neq:
        gen_tcall(ctx, N00B_BI_EQ, t);
        emit(ctx, N00B_ZNot);
        break;
    case n00b_op_lte:
        gen_tcall(ctx, N00B_BI_GT, t);
        emit(ctx, N00B_ZNot);
        break;
    case n00b_op_gte:
        gen_tcall(ctx, N00B_BI_LT, t);
        emit(ctx, N00B_ZNot);
        break;
    case n00b_op_fdiv:
    case n00b_op_shl:
    case n00b_op_shr:
    case n00b_op_bitand:
    case n00b_op_bitor:
    case n00b_op_bitxor:
    default:
        n00b_unreachable();
    }
}

static inline void
gen_float_binary_op(gen_ctx *ctx, n00b_operator_t op)
{
    n00b_zop_t zop;

    switch (op) {
    case n00b_op_plus:
        zop = N00B_ZFAdd;
        break;
    case n00b_op_minus:
        zop = N00B_ZFSub;
        break;
    case n00b_op_mul:
        zop = N00B_ZFMul;
        break;
    case n00b_op_div:
    case n00b_op_fdiv:
        zop = N00B_ZFDiv;
        break;
    default:
        n00b_unreachable();
    }
    emit(ctx, zop);
}

static inline bool
skip_poly_call(n00b_type_t *t)
{
    if (n00b_type_is_box(t)) {
        return true;
    }

    if (n00b_type_is_int_type(t)) {
        return true;
    }

    if (n00b_type_is_float_type(t)) {
        return true;
    }

    return false;
}

static inline void
gen_binary_op(gen_ctx *ctx, n00b_type_t *t)
{
    n00b_operator_t op = (n00b_operator_t)ctx->cur_pnode->extra_info;

    gen_kids(ctx);

    if (!skip_poly_call(t)) {
        gen_polymorphic_binary_op(ctx, op, t);
        return;
    }

    if (n00b_type_is_box(t)) {
        t = n00b_type_unbox(t);
    }

    if (n00b_type_is_int_type(t)) {
        gen_int_binary_op(ctx, op, n00b_type_is_signed(t));
        return;
    }

    if (n00b_type_is_bool(t)) {
        gen_int_binary_op(ctx, op, false);
        return;
    }

    if (t->typeid == N00B_T_F64) {
        gen_float_binary_op(ctx, op);
        return;
    }
}

static inline void
gen_assert(gen_ctx *ctx)
{
    gen_kids(ctx);
    emit(ctx, N00B_ZAssert);
}

static inline void
gen_box_if_value_type(gen_ctx *ctx, int pos)
{
    n00b_pnode_t *pnode = n00b_get_pnode(ctx->cur_node->children[pos]);
    n00b_type_t  *t     = pnode->type;

    if (n00b_type_is_box(t)) {
        t = n00b_type_unbox(t);
    }
    else {
        if (!n00b_type_is_value_type(t)) {
            return;
        }
    }

    emit(ctx, N00B_ZBox, n00b_kw("type", n00b_ka(t)));
}

static inline void
gen_or(gen_ctx *ctx)
{
    gen_one_kid(ctx, 0);
    // If the first clause is false, we WILL need to pop it,
    // as we should only leave one value on the stack.
    GEN_JNZ_NOPOP(emit(ctx, N00B_ZPop);
                  gen_one_kid(ctx, 1););
}

static inline void
gen_and(gen_ctx *ctx)
{
    // Same as above except JZ not JNZ.
    gen_one_kid(ctx, 0);
    GEN_JZ_NOPOP(emit(ctx, N00B_ZPop);
                 gen_one_kid(ctx, 1););
}

#ifdef N00B_DEV
static inline void
gen_print(gen_ctx *ctx)
{
    gen_kids(ctx);
    gen_box_if_value_type(ctx, 0);
    emit(ctx, N00B_ZPrint);
}
#endif

static inline void
gen_callback_literal(gen_ctx *ctx)
{
    n00b_callback_info_t *scb = (n00b_callback_info_t *)ctx->cur_pnode->value;

    int64_t arg = n00b_add_static_string(scb->target_symbol_name, ctx->cctx);
    gen_load_const_by_offset(ctx, arg, n00b_type_utf8());

    if (scb->binding.ffi) {
        n00b_ffi_decl_t *f = scb->binding.implementation.ffi_interface;

        emit(ctx,
             N00B_ZPushFfiPtr,
             n00b_kw("arg",
                     n00b_ka(f->global_ffi_call_ix),
                     "type",
                     n00b_ka(scb->target_type),
                     "immediate",
                     n00b_ka(f->skip_boxes)));
    }
    else {
        n00b_fn_decl_t *f = scb->binding.implementation.local_interface;

        emit(ctx,
             N00B_ZPushVmPtr,
             n00b_kw("arg",
                     n00b_ka(f->local_id),
                     "module_id",
                     n00b_ka(f->module_id),
                     "type",
                     n00b_ka(scb->target_type)));
    }
}

static inline void
gen_literal(gen_ctx *ctx)
{
    n00b_obj_t        lit = ctx->cur_pnode->value;
    n00b_lit_info_t  *li  = (n00b_lit_info_t *)ctx->cur_pnode->extra_info;
    n00b_tree_node_t *n   = ctx->cur_node;

    if (lit != NULL) {
        n00b_type_t *t = n00b_type_resolve(n00b_get_my_type(lit));

        if (n00b_type_is_box(t)) {
            gen_load_immediate(ctx, n00b_unbox(lit));
        }
        else {
            gen_load_const_obj(ctx, lit);
            // This is only true for containers, which need to be
            // copied since they are mutable, but the const version
            // is... const.
            if (n00b_type_is_mutable(t)) {
                gen_tcall(ctx, N00B_BI_COPY, t);
            }
        }

        return;
    }

    if (li->num_items == 1) {
        gen_kids(ctx);
        gen_load_immediate(ctx, n->num_kids);
        gen_tcall(ctx, N00B_BI_CONTAINER_LIT, li->type);
        return;
    }

    // We need to convert each set of items into a tuple object.
    n00b_type_t *ttype = n00b_type_tuple_from_xlist(li->type->items);

    for (int i = 0; i < n->num_kids;) {
        for (int j = 0; j < li->num_items; j++) {
            ctx->cur_node = n->children[i++];
            gen_one_node(ctx);
        }

        if (!ctx->lvalue) {
            gen_load_immediate(ctx, li->num_items);
            gen_tcall(ctx, N00B_BI_CONTAINER_LIT, ttype);
        }
    }

    if (!n00b_type_is_tuple(li->type)) {
        gen_load_immediate(ctx, n->num_kids / li->num_items);
        gen_tcall(ctx, N00B_BI_CONTAINER_LIT, li->type);
    }

    ctx->cur_node = n;
}

static inline bool
is_tuple_assignment(gen_ctx *ctx)
{
    n00b_tree_node_t *n = n00b_get_match_on_node(ctx->cur_node, n00b_tuple_assign);

    if (n != NULL) {
        return true;
    }

    return false;
}

static inline void
gen_assign(gen_ctx *ctx)
{
    n00b_type_t *t        = n00b_type_resolve(ctx->cur_pnode->type);
    bool         lhs_attr = false;
    ctx->assign_method    = assign_to_mem_slot;
    ctx->lvalue           = true;

    gen_one_kid(ctx, 0);
    if (ctx->assign_method == assign_to_attribute) {
        lhs_attr = true;
    }
    ctx->lvalue = false;
    gen_one_kid(ctx, 1);

    switch (ctx->assign_method) {
    case assign_to_mem_slot:
        if (lhs_attr) {
            goto handle_attribute;
        }
        if (is_tuple_assignment(ctx)) {
            emit(ctx, N00B_ZPopToR1);
            emit(ctx,
                 N00B_ZUnpack,
                 n00b_kw("arg", n00b_ka(n00b_type_get_num_params(t))));
        }
        else {
            emit(ctx, N00B_ZSwap);
            emit(ctx, N00B_ZAssignToLoc);
        }
        break;
    case assign_via_slice_set_call:
        gen_tcall(ctx, N00B_BI_SLICE_SET, ctx->cur_pnode->type);
        break;
    case assign_to_attribute:
handle_attribute:
        emit(ctx,
             N00B_ZAssignAttr,
             n00b_kw("arg", n00b_ka(ctx->attr_lock), "type", n00b_ka(t)));
        break;
    case assign_via_len_then_slice_set_call:
        // Need to call len() on the object for the 2nd slice
        // param.  The 2nd slice parameter is supposed to get
        // pushed on first though.
        //
        // Stash the value in R0.
        emit(ctx, N00B_ZPopToR0);
        // stash start ix
        emit(ctx, N00B_ZPopToR1);
        // Dupe the container.
        emit(ctx, N00B_ZDupTop);
        // Call len on the non-popped version.
        gen_tcall(ctx, N00B_BI_LEN, ctx->cur_pnode->type);
        // Push the index back.
        emit(ctx, N00B_ZPushFromR1);
        // Swap the two indices to be in the proper order.
        emit(ctx, N00B_ZSwap);
        // Push the value back.
        emit(ctx, N00B_ZPushFromR0);
        // Slice!
        gen_tcall(ctx, N00B_BI_SLICE_SET, ctx->cur_pnode->type);
        break;
    case assign_via_index_set_call:
        emit(ctx, N00B_ZSwap);
        gen_tcall(ctx, N00B_BI_INDEX_SET, ctx->cur_pnode->type);
        break;
    }
}

#define BINOP_ASSIGN_GEN(ctx, op, t)                            \
    if (n00b_type_get_kind(t) == N00B_DT_KIND_primitive) {      \
        if (n00b_type_is_int_type(t)) {                         \
            gen_int_binary_op(ctx, op, n00b_type_is_signed(t)); \
        }                                                       \
                                                                \
        else {                                                  \
            if (n00b_type_is_bool(t)) {                         \
                gen_int_binary_op(ctx, op, false);              \
            }                                                   \
            else {                                              \
                if (t->typeid == N00B_T_F64) {                  \
                    gen_float_binary_op(ctx, op);               \
                }                                               \
                else {                                          \
                    gen_polymorphic_binary_op(ctx, op, t);      \
                }                                               \
            }                                                   \
        }                                                       \
    }

static inline void
gen_binary_assign(gen_ctx *ctx)
{
    n00b_operator_t op = (n00b_operator_t)ctx->cur_pnode->extra_info;
    n00b_type_t    *t  = n00b_type_resolve(ctx->cur_pnode->type);

    ctx->assign_method = assign_to_mem_slot;
    ctx->lvalue        = true;
    gen_one_kid(ctx, 0);
    ctx->lvalue = false;

    switch (ctx->assign_method) {
    case assign_to_mem_slot:
        emit(ctx, N00B_ZDupTop);
        emit(ctx, N00B_ZDeref);
        gen_one_kid(ctx, 1);

        BINOP_ASSIGN_GEN(ctx, op, t);

        emit(ctx, N00B_ZSwap);
        emit(ctx, N00B_ZAssignToLoc);
        break;
    case assign_via_index_set_call:
        emit(ctx, N00B_ZPopToR1);
        emit(ctx, N00B_ZPopToR2);
        //
        emit(ctx, N00B_ZPushFromR2);
        emit(ctx, N00B_ZPushFromR1);
        //
        emit(ctx, N00B_ZPushFromR2);
        emit(ctx, N00B_ZPushFromR1);

        gen_tcall(ctx, N00B_BI_INDEX_GET, ctx->cur_pnode->type);
        gen_one_kid(ctx, 1);

        BINOP_ASSIGN_GEN(ctx, op, t);

        gen_tcall(ctx, N00B_BI_INDEX_SET, ctx->cur_pnode->type);
        break;
    default:
        // TODO: disallow slice assignments and tuple assignments here.
        n00b_unreachable();
    }
}

static inline void
gen_index_or_slice(gen_ctx *ctx)
{
    // We turn of LHS tracking internally, because we don't poke
    // directly into the object's memory and don't want to generate a
    // settable ref.
    bool          lvalue = ctx->lvalue;
    n00b_pnode_t *pnode  = n00b_get_pnode(ctx->cur_node->children[1]);
    bool          slice  = pnode->kind == n00b_nt_range;

    ctx->lvalue = false;

    gen_kids(ctx);

    if (lvalue) {
        if (slice) {
            if (pnode->extra_info == (void *)1) {
                ctx->assign_method = assign_via_len_then_slice_set_call;
            }
            else {
                ctx->assign_method = assign_via_slice_set_call;
            }
        }
        else {
            ctx->assign_method = assign_via_index_set_call;
        }
        ctx->lvalue = true;
        return;
    }
    if (slice) {
        if (pnode->extra_info == (void *)1) {
            // Need to call len() on the object for the 2nd slice
            // param.  The 2nd slice parameter is supposed to get
            // pushed on first though.
            //
            // Stash the other index.
            emit(ctx, N00B_ZPopToR0);
            // Dupe the copy.
            emit(ctx, N00B_ZDupTop);
            // Call len on the dupe.
            gen_tcall(ctx, N00B_BI_LEN, ctx->cur_pnode->type);
            // Push the index back.
            emit(ctx, N00B_ZPushFromR0);
            // Swap positions.
            emit(ctx, N00B_ZSwap);
        }
        gen_tcall(ctx, N00B_BI_SLICE_GET, ctx->cur_pnode->type);
    }
    else {
        gen_tcall(ctx, N00B_BI_INDEX_GET, ctx->cur_pnode->type);
    }
}

static inline void
gen_elision(gen_ctx *ctx)
{
    // Right now, this is only for indexes on slices.  If were' on the
    // LHS, life is easy; we just emit an actual 0.
    if (ctx->cur_node->parent->children[0] == ctx->cur_node) {
        gen_load_immediate(ctx, 0);
        return;
    }

    // Otherwise, we cheat a little bit here, and signal to
    // gen_index_or_slice through the range pnode.
    n00b_pnode_t *range_pnode = n00b_get_pnode(ctx->cur_node->parent);
    range_pnode->extra_info   = (void *)1;
}

static inline void
gen_sym_decl(gen_ctx *ctx)
{
    int               last = ctx->cur_node->num_kids - 1;
    n00b_pnode_t     *kid  = n00b_get_pnode(ctx->cur_node->children[last]);
    n00b_pnode_t     *psym;
    n00b_tree_node_t *cur = ctx->cur_node;
    n00b_symbol_t    *sym;

    if (kid->kind == n00b_nt_assign) {
        psym = n00b_get_pnode(ctx->cur_node->children[last - 1]);
        if (psym->kind == n00b_nt_lit_tspec) {
            psym = n00b_get_pnode(ctx->cur_node->children[last - 2]);
        }

        sym = (n00b_symbol_t *)psym->value;

        if (sym->flags & N00B_F_DECLARED_CONST) {
            return;
        }

        ctx->cur_node = cur->children[last]->children[0];
        gen_one_node(ctx);
        ctx->cur_node = cur;
        gen_sym_load(ctx, sym, true);
        emit(ctx, N00B_ZAssignToLoc);
    }
}

static inline void
gen_unary_op(gen_ctx *ctx)
{
    // Right now, the pnode extra_info will be NULL when it's a unary
    // minus and non-null when it's a not operation.
    n00b_pnode_t *n = n00b_get_pnode(ctx->cur_node);

    gen_kids(ctx);

    if (n->extra_info != NULL) {
        gen_kids(ctx);
        emit(ctx, N00B_ZNot, n00b_kw("type", n00b_ka(n00b_type_bool())));
    }
    else {
        gen_load_immediate(ctx, -1);
        emit(ctx, N00B_ZMul);
    }
}

static inline void
gen_lock(gen_ctx *ctx)
{
    n00b_tree_node_t *saved = ctx->cur_node;
    n00b_pnode_t     *p     = n00b_tree_get_contents(saved->children[0]);

    switch (p->kind) {
    case n00b_nt_assign:
    case n00b_nt_binary_assign_op:
        ctx->attr_lock = true;
        gen_kids(ctx);
        ctx->attr_lock = false;
        break;
    default:
        gen_one_kid(ctx, 0);
        ctx->cur_node = saved;
        emit(ctx, N00B_ZLockOnWrite);
        gen_kids(ctx);
        break;
    }
}

static inline void
gen_use(gen_ctx *ctx)
{
    n00b_module_t *tocall;

    tocall = (n00b_module_t *)ctx->cur_pnode->value;
    emit(ctx,
         N00B_ZCallModule,
         n00b_kw("module_id", n00b_ka(tocall->module_id)));
}

static inline void
gen_section(gen_ctx *ctx)
{
    gen_one_kid(ctx, ctx->cur_node->num_kids - 1);
}

static void
gen_one_node(gen_ctx *ctx)
{
    ctx->cur_pnode = n00b_get_pnode(ctx->cur_node);

    switch (ctx->cur_pnode->kind) {
    case n00b_nt_module:
        gen_module(ctx);
        break;
    case n00b_nt_error:
    case n00b_nt_cast: // No syntax for this yet.
        n00b_unreachable();
    case n00b_nt_body:
    case n00b_nt_else:
        gen_kids(ctx);
        break;
    case n00b_nt_if:
        gen_if(ctx);
        break;
    case n00b_nt_elif:
        gen_elif(ctx);
        break;
    case n00b_nt_typeof:
        gen_typeof(ctx);
        break;
    case n00b_nt_switch:
        gen_switch(ctx);
        break;
    case n00b_nt_for:
        gen_for(ctx);
        break;
    case n00b_nt_while:
        gen_while(ctx);
        break;
    case n00b_nt_break:
        gen_break(ctx);
        break;
    case n00b_nt_continue:
        gen_continue(ctx);
        break;
    case n00b_nt_range:
        gen_kids(ctx);
        break;
    case n00b_nt_or:
        gen_or(ctx);
        break;
    case n00b_nt_and:
        gen_and(ctx);
        break;
    case n00b_nt_cmp:;
        n00b_pnode_t *p = n00b_get_pnode(ctx->cur_node->children[0]);
        gen_binary_op(ctx, p->type);
        break;
    case n00b_nt_binary_op:
        gen_binary_op(ctx, ctx->cur_pnode->type);
        break;
    case n00b_nt_member:
    case n00b_nt_identifier:
        do {
            n00b_symbol_t *sym = ctx->cur_pnode->extra_info;
            if (sym != NULL) {
                gen_sym_load(ctx, sym, ctx->lvalue);
            }
        } while (0);
        break;
    case n00b_nt_assert:
        gen_assert(ctx);
        break;
#ifdef N00B_DEV
    case n00b_nt_print:
        gen_print(ctx);
        break;
#endif
    case n00b_nt_lit_callback:
        gen_callback_literal(ctx);
        break;
    case n00b_nt_simple_lit:
    case n00b_nt_lit_list:
    case n00b_nt_lit_dict:
    case n00b_nt_lit_set:
    case n00b_nt_lit_empty_dict_or_set:
    case n00b_nt_lit_tuple:
    case n00b_nt_lit_unquoted:
    case n00b_nt_lit_tspec:
        gen_literal(ctx);
        break;
    case n00b_nt_assign:
        gen_assign(ctx);
        break;
    case n00b_nt_binary_assign_op:
        gen_binary_assign(ctx);
        break;
    case n00b_nt_index:
        gen_index_or_slice(ctx);
        break;
    case n00b_nt_sym_decl:
        gen_sym_decl(ctx);
        break;
    case n00b_nt_unary_op:
        gen_unary_op(ctx);
        break;
    case n00b_nt_call:
        gen_call(ctx);
        break;
    case n00b_nt_return:
        gen_ret(ctx);
        break;
    case n00b_nt_attr_set_lock:
        gen_lock(ctx);
        break;
    case n00b_nt_use:
        gen_use(ctx);
        break;
    case n00b_nt_elided:
        gen_elision(ctx);
        break;
        // The following list is still TODO:
    case n00b_nt_varargs_param:
        // These should always be passthrough.
    case n00b_nt_expression:
    case n00b_nt_paren_expr:
    case n00b_nt_variable_decls:
        gen_kids(ctx);
        break;
    case n00b_nt_section:
        gen_section(ctx);
        break;
    // These nodes should NOT do any work and not descend if they're
    // hit; many of them are handled elsewhere and this should be
    // unreachable for many of them.
    //
    // Some things spec related, param related, etc will get generated
    // before the tree is walked from cached info.
    case n00b_nt_case:
    case n00b_nt_decl_qualifiers:
    case n00b_nt_label:
    case n00b_nt_config_spec:
    case n00b_nt_section_spec:
    case n00b_nt_section_prop:
    case n00b_nt_field_spec:
    case n00b_nt_field_prop:
    case n00b_nt_lit_tspec_tvar:
    case n00b_nt_lit_tspec_named_type:
    case n00b_nt_lit_tspec_parameterized_type:
    case n00b_nt_lit_tspec_func:
    case n00b_nt_lit_tspec_varargs:
    case n00b_nt_lit_tspec_return_type:
    case n00b_nt_param_block:
    case n00b_nt_param_prop:
    case n00b_nt_extern_block:
    case n00b_nt_extern_sig:
    case n00b_nt_extern_param:
    case n00b_nt_extern_local:
    case n00b_nt_extern_dll:
    case n00b_nt_extern_pure:
    case n00b_nt_extern_box:
    case n00b_nt_extern_holds:
    case n00b_nt_extern_allocs:
    case n00b_nt_extern_return:
    case n00b_nt_enum:
    case n00b_nt_enum_item:
    case n00b_nt_global_enum:
    case n00b_nt_func_def:
    // We start this from our reference to the functions.  So when
    // it comes via the top-level of the module, ignore it.
    case n00b_nt_func_mods:
    case n00b_nt_func_mod:
    case n00b_nt_formals:
        break;
    }
}

static void
gen_set_once_memo(gen_ctx *ctx, n00b_fn_decl_t *decl)
{
    if (decl->signature_info->void_return) {
        return;
    }

    emit(ctx, N00B_ZPushFromR0);
    emit(ctx, N00B_ZPushStaticRef, n00b_kw("arg", n00b_ka(decl->sc_memo_offset)));
    emit(ctx, N00B_ZAssignToLoc);
}

static void
gen_return_once_memo(gen_ctx *ctx, n00b_fn_decl_t *decl)
{
    if (decl->signature_info->void_return) {
        return;
    }

    emit(ctx, N00B_ZPushStaticObj, n00b_kw("arg", n00b_ka(decl->sc_memo_offset)));
    emit(ctx, N00B_ZRet);
}

static void
gen_function(gen_ctx       *ctx,
             n00b_symbol_t *sym,
             n00b_module_t *module,
             n00b_vm_t     *vm)
{
    n00b_fn_decl_t *decl = sym->value;
    int             n    = sym->ct->declaration_node->num_kids;
    ctx->cur_node        = sym->ct->declaration_node->children[n - 1];

    n00b_zfn_info_t *fn_info_for_obj = n00b_new_zfn();

    ctx->retsym = hatrack_dict_get(decl->signature_info->fn_scope->symbols,
                                   n00b_new_utf8("$result"),
                                   NULL);

    fn_info_for_obj->mid      = module->module_id;
    decl->module_id           = module->module_id;
    decl->offset              = ctx->instruction_counter;
    fn_info_for_obj->offset   = ctx->instruction_counter;
    fn_info_for_obj->tid      = decl->signature_info->full_type;
    fn_info_for_obj->shortdoc = decl->short_doc;
    fn_info_for_obj->longdoc  = decl->long_doc;
    fn_info_for_obj->funcname = sym->name;
    ctx->current_stack_offset = decl->frame_size;

    // In anticipation of supporting multi-threaded access, I've put a
    // write-lock around this. And once the result is cached, the lock
    // does not matter. It's really only there to avoid multiple
    // threads competing to cache the memo.
    gen_label(ctx, sym->name);
    if (decl->once) {
        fn_info_for_obj->static_lock = decl->sc_lock_offset;

        emit(ctx,
             N00B_ZPushStaticObj,
             n00b_kw("arg", n00b_ka(n00b_ka(decl->sc_bool_offset))));
        GEN_JZ(gen_return_once_memo(ctx, decl));
        emit(ctx,
             N00B_ZLockMutex,
             n00b_kw("arg", n00b_ka(decl->sc_lock_offset)));
        emit(ctx,
             N00B_ZPushStaticObj,
             n00b_kw("arg", n00b_ka(decl->sc_bool_offset)));
        // If it's not zero, we grabbed the lock, but waited while
        // someone else computed the memo.
        GEN_JZ(emit(ctx,
                    N00B_ZUnlockMutex,
                    n00b_kw("arg",
                            n00b_ka(decl->sc_lock_offset)));
               emit(ctx,
                    N00B_ZPushStaticObj,
                    n00b_kw("arg", n00b_ka(decl->sc_memo_offset)));
               emit(ctx, N00B_ZRet););
        // Set the boolean to true.
        gen_load_immediate(ctx, 1);
        emit(ctx,
             N00B_ZPushStaticRef,
             n00b_kw("arg", n00b_ka(decl->sc_bool_offset)));
        emit(ctx, N00B_ZAssignToLoc);
    }

    // Until we have full PDG analysis, always zero out the return register
    // on a call entry.
    if (!decl->signature_info->void_return) {
        emit(ctx, N00B_Z0R0c00l);
    }
    // The frame size needs to be backpatched, since the needed stack
    // space from block scopes hasn't been computed by this point. It
    // gets computed as we generate code. So stash this:
    int sp_loc = ctx->instruction_counter;
    emit(ctx, N00B_ZMoveSp);

    gen_one_node(ctx);

    if (decl->once) {
        gen_set_once_memo(ctx, decl);
        emit(ctx,
             N00B_ZUnlockMutex,
             n00b_kw("arg", n00b_ka(decl->sc_lock_offset)));
    }

    if (ctx->retsym) {
        gen_sym_load(ctx, ctx->retsym, false);
        emit(ctx, N00B_ZPopToR0);
    }

    emit(ctx, N00B_ZRet);

    // The actual backpatching of the needed frame size.
    n00b_zinstruction_t *ins = n00b_list_get(module->instructions, sp_loc, NULL);
    ins->arg                 = ctx->current_stack_offset;
    fn_info_for_obj->size    = ctx->current_stack_offset;

    n00b_list_append(vm->obj->func_info, fn_info_for_obj);

    // TODO: Local id is set to 1 more than its natural index because
    // 0 was taken to mean the module entry point back when I was
    // going to cache fn info per module. But since we don't handle
    // the same way, should probably make this more sane, but
    // that's why this is below the append.
    decl->local_id = n00b_list_len(vm->obj->func_info);
    ctx->retsym    = NULL;
}

static void
gen_module_code(gen_ctx *ctx, n00b_vm_t *vm)
{
    n00b_module_t *module     = ctx->fctx;
    ctx->cur_node             = module->ct->parse_tree;
    ctx->instructions         = n00b_list(n00b_type_ref());
    ctx->cur_pnode            = n00b_get_pnode(ctx->cur_node);
    ctx->cur_module           = module;
    ctx->target_info          = NULL;
    ctx->retsym               = NULL;
    ctx->instruction_counter  = 0;
    ctx->current_stack_offset = 0;
    ctx->max_stack_size       = 0;
    ctx->lvalue               = false;
    ctx->assign_method        = assign_to_mem_slot;
    module->instructions      = ctx->instructions;

    // Still to fill in to the zmodule object (need to reshuffle to align):
    // authority/path/provided_path/package/module_id
    // Remove key / location / ext / url.
    //
    // Also need to do a bit of work aorund sym_types, codesyms, datasyms
    // and parameters.

    gen_one_node(ctx);

    n00b_zinstruction_t *sp_move = n00b_list_get(module->instructions,
                                                 ctx->module_patch_loc,
                                                 NULL);
    sp_move->arg                 = ctx->max_stack_size;

    emit(ctx, N00B_ZModuleRet);

    n00b_list_t *symlist = ctx->fctx->fn_def_syms;
    int          n       = n00b_list_len(symlist);

    if (n) {
        gen_label(ctx, n00b_new_utf8("Functions: "));
    }

    for (int i = 0; i < n; i++) {
        n00b_symbol_t *sym = n00b_list_get(symlist, i, NULL);
        gen_function(ctx, sym, module, vm);
    }

    int l = n00b_list_len(ctx->fctx->extern_decls);
    if (l != 0) {
        for (int j = 0; j < l; j++) {
            n00b_symbol_t   *d    = n00b_list_get(ctx->fctx->extern_decls,
                                             j,
                                             NULL);
            n00b_ffi_decl_t *decl = d->value;

            n00b_list_append(vm->obj->ffi_info, decl);
        }
    }
}

static inline void
backpatch_calls(gen_ctx *ctx)
{
    int                         n = n00b_list_len(ctx->call_backpatches);
    n00b_call_backpatch_info_t *info;

    for (int i = 0; i < n; i++) {
        info               = n00b_list_get(ctx->call_backpatches, i, NULL);
        info->i->arg       = info->decl->local_id;
        info->i->module_id = info->decl->module_id;
        n00b_assert(info->i->arg != 0);
    }
}

void
n00b_internal_codegen(n00b_compile_ctx *cctx, n00b_vm_t *vm)
{
    gen_ctx ctx = {
        .cctx             = cctx,
        .call_backpatches = n00b_new(n00b_type_list(n00b_type_ref())),
        0,
    };

    int n = n00b_list_len(vm->obj->module_contents);

    for (int i = 0; i < n; i++) {
        ctx.fctx = n00b_list_get(vm->obj->module_contents, i, NULL);

        if (ctx.fctx->ct == NULL) {
            continue; // Already fully compiled.
        }

        if (ctx.fctx->ct->status < n00b_compile_status_tree_typed) {
            N00B_CRAISE("Cannot call n00b_codegen with untyped modules.");
        }

        if (n00b_fatal_error_in_module(ctx.fctx)) {
            N00B_CRAISE("Cannot generate code for files with fatal errors.");
        }

        if (ctx.fctx->ct->status >= n00b_compile_status_generated_code) {
            continue;
        }

        gen_module_code(&ctx, vm);

        ctx.fctx->ct->status = n00b_compile_status_generated_code;
    }

    backpatch_calls(&ctx);
}
