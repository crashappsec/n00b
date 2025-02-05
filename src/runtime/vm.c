#define N00B_USE_INTERNAL_API
#include "n00b.h"

#define FORMAT_NOT_IN_FN()        \
    s = n00b_format(n00b_fmt_mod, \
                    frameno++,    \
                    modname,      \
                    (int64_t)lineno)
#define FORMAT_IN_FN()                                             \
    fnname = tstate->frame_stack[num_frames].targetfunc->funcname; \
    s      = n00b_format(n00b_fmt_fn,                              \
                    frameno++,                                \
                    fnname,                                   \
                    modname,                                  \
                    (int64_t)lineno)
#define OUTPUT_FRAME() \
    n00b_ansi_render(s, f);

static inline n00b_list_t *
format_one_frame(n00b_vmthread_t *tstate, int n)
{
    n00b_list_t     *l = n00b_list(n00b_type_string());
    uint64_t         pc;
    uint64_t         line;
    n00b_zfn_info_t *f;
    n00b_string_t   *modname;
    n00b_string_t   *fname;

    if (n == tstate->num_frames) {
        n00b_module_t       *m = tstate->current_module;
        n00b_zinstruction_t *i = n00b_list_get(m->instructions, tstate->pc, NULL);

        pc      = tstate->pc;
        modname = m->name;
        line    = i->line_no;
    }
    else {
        n00b_vmframe_t *frame = &tstate->frame_stack[n];
        modname               = frame->call_module->name;
        pc                    = frame->pc;
        line                  = frame->calllineno;
    }

    if (n == 0) {
        f = NULL;
    }
    else {
        f = tstate->frame_stack[n - 1].targetfunc;
    }

    if (n == 1) {
        fname = n00b_cstring("(start of execution)");
    }
    else {
        fname = f ? f->funcname : n00b_cstring("(module toplevel)");
    }

    n00b_list_append(l, n00b_cformat("«#:p»", pc));
    n00b_list_append(l, n00b_cformat("«#»:«#»", modname, line));
    n00b_list_append(l, fname);

    return l;
}

n00b_table_t *
n00b_get_backtrace(n00b_vmthread_t *tstate)
{
    if (!tstate->running) {
        return n00b_call_out(n00b_cstring("N00b is not running!"));
    }

    n00b_table_t *result = n00b_table("columns", n00b_ka(3));

    int nframes = tstate->num_frames;

    n00b_table_add_cell(result, n00b_cstring("PC"));
    n00b_table_add_cell(result, n00b_cstring("Location"));
    n00b_table_add_cell(result, n00b_cstring("Function"));

    while (nframes > 0) {
        n00b_table_add_row(result, format_one_frame(tstate, nframes--));
    }

    return result;
}

static void
n00b_vm_exception(n00b_vmthread_t *tstate, n00b_exception_t *exc)
{
    n00b_print_exception(exc, n00b_crich("«em2»Fatal Exception:«/» "));
    // The caller will know what to do on exception.
}

// So much of this VM implementation is based on trust that it seems silly to
// even include a stack underflow check anywhere at all, and maybe it is.
// However, if nothing else, it serves as documentation wherever the stack is
// touched to indicate how many values are expected.
//
// Note from John: in the original implementation this was just to
// help me both document for myself and help root out any codegen bugs
// with a helpful starting point instead of a crash.

#ifdef N00B_OMIT_UNDERFLOW_CHECKS
#define STACK_REQUIRE_VALUES(_n)
#else
#define STACK_REQUIRE_VALUES(_n)                               \
    if (tstate->sp > &tstate->stack[N00B_STACK_SIZE - (_n)]) { \
        N00B_CRAISE("stack underflow");                        \
    }
#endif

// Overflow checks are reasonable, however. The code generator has no way of
// knowing whether the code is generating is going to overflow the stack. It
// can only determine the maximum stack depth for a given function. But, it
// could compute the deepest stack depth and multiply by the maximum call depth
// to dynamically size the stack. Since that's not done at all, we need to check
// for stack overflow for safety.
#define STACK_REQUIRE_SLOTS(_n)              \
    if (tstate->sp - (_n) < tstate->stack) { \
        N00B_CRAISE("stack overflow");       \
    }

#define SIMPLE_COMPARE(op)                          \
    do {                                            \
        int64_t v1 = tstate->sp->sint;              \
        ++tstate->sp;                               \
        int64_t v2       = tstate->sp->sint;        \
        tstate->sp->sint = !!((int64_t)(v2 op v1)); \
    } while (0)

#define SIMPLE_COMPARE_UNSIGNED(op)                 \
    do {                                            \
        int64_t v1 = tstate->sp->uint;              \
        ++tstate->sp;                               \
        int64_t v2       = tstate->sp->uint;        \
        tstate->sp->uint = !!((int64_t)(v2 op v1)); \
    } while (0)

static n00b_obj_t
n00b_vm_variable(n00b_vmthread_t *tstate, n00b_zinstruction_t *i)
{
    return &tstate->vm->module_allocations[i->module_id][i->arg];
}

static inline bool
n00b_value_iszero(n00b_obj_t value)
{
    return 0 == value;
}

#if 0 // Will return soon

static n00b_string_t *
get_param_name(n00b_zparam_info_t *p, n00b_module_t *)
{
    if (p->attr != NULL && n00b_len(p->attr) > 0) {
        return p->attr;
    }
    return n00b_index_get(m->datasyms, (n00b_obj_t)p->offset);
}

static n00b_value_t *
get_param_value(n00b_vmthread_t *tstate, n00b_zparam_info_t *p)
{
    // if (p->userparam.type_info != NULL) {
    // return &p->userparam;
    //}
    if (p->have_default) {
        return &p->default_value;
    }

    n00b_string_t *name = get_param_name(p, tstate->current_module);

    errstr = n00b_cstring("parameter not set: ");
    n00b_string_t *msg = n00b_string_concat(errstr, name);
    module_prefix = n00b_cstring("parameter not set: ");
    msg = n00b_string_concat(msg, module_prefix);
    msg = n00b_string_concat(msg, tstate->current_module->modname);
    msg = n00b_string_concat(msg, n00b_cached_rparen());
    N00B_RAISE(msg);
}
#endif

static void
n00b_vm_module_enter(n00b_vmthread_t *tstate, n00b_zinstruction_t *i)
{
    // If there's already a lock, we always push ourselves to the module
    // lock stack. If there isn't, we start the stack if our module
    // has parameters.

#if 0 // Redoing parameters soon.
    int nparams = n00b_list_len(tstate->current_module->parameters);

    for (int32_t n = 0; n < nparams; ++n) {
        n00b_zparam_info_t *p = n00b_list_get(tstate->current_module->parameters,
                                            n,
                                            NULL);

        // Fill in all parameter values now. If there's a validator,
        // it will get called after this loop, along w/ a call to
        // ZParamCheck.

        if (p->attr && n00b_len(p->attr) > 0) {
            bool found;
            hatrack_dict_get(tstate->vm->attrs, p->attr, &found);
            if (!found) {
                n00b_obj_t value = get_param_value(tstate, p);
                n00b_vm_attr_set(tstate,
				p->attr,
				value,
				true,
				false,
				true);
            }
        }
    }
#endif
}

static void
n00b_vmframe_push(n00b_vmthread_t     *tstate,
                  n00b_zinstruction_t *i,
                  n00b_module_t       *call_module,
                  int32_t              call_pc,
                  n00b_module_t       *target_module,
                  n00b_zfn_info_t     *target_func,
                  int32_t              target_lineno)
{
    // TODO: Should probably recover call_pc off the stack when
    // needed, instead of adding the extra param here.

    if (N00B_MAX_CALL_DEPTH == tstate->num_frames) {
        N00B_CRAISE("maximum call depth reached");
    }

    n00b_vmframe_t *f = &tstate->frame_stack[tstate->num_frames];

    *f = (n00b_vmframe_t){
        .call_module  = call_module,
        .calllineno   = i->line_no,
        .targetline   = target_lineno,
        .targetmodule = target_module,
        .targetfunc   = target_func,
        .pc           = call_pc,
    };

    ++tstate->num_frames;
}

static void
n00b_vm_tcall(n00b_vmthread_t *tstate, n00b_zinstruction_t *i)
{
    bool         b;
    n00b_obj_t   obj;
    n00b_type_t *t;

    switch ((n00b_builtin_type_fn)i->arg) {
    case N00B_BI_TO_STR:
        STACK_REQUIRE_VALUES(1);

        obj = n00b_to_str(tstate->sp->rvalue,
                          n00b_get_my_type(tstate->sp->rvalue));

        tstate->sp->rvalue = obj;
        return;
    case N00B_BI_REPR:
        STACK_REQUIRE_VALUES(1);

        obj                = n00b_repr(tstate->sp->rvalue);
        tstate->sp->rvalue = obj;
        return;
    case N00B_BI_COERCE:
#if 0
        STACK_REQUIRE_VALUES(2);
        // srcType = i->type_info
        // dstType = tstate->sp->rvalue.type_info (should be same as .obj)
        // obj = tstate->sp[1]

        t   = tstate->sp->rvalue.type_info;
        obj = n00b_coerce(tstate->sp[1].rvalue.obj, i->type_info, t);

        ++tstate->sp;
        tstate->sp->rvalue = obj;
#endif
        return;
    case N00B_BI_FORMAT:
    case N00B_BI_ITEM_TYPE:
        N00B_CRAISE("Currently format cannot be called via tcall.");
    case N00B_BI_VIEW:
        // This is used to implement loops over objects; we pop the
        // container by pushing the view on, then also push
        // the length of the view.
        STACK_REQUIRE_VALUES(1);
        STACK_REQUIRE_SLOTS(1);
        do {
            uint64_t n;
            tstate->sp->rvalue = n00b_get_view(tstate->sp->rvalue,
                                               (int64_t *)&n);
            --tstate->sp;
            tstate->sp[0].uint = n;
        } while (0);

        return;
    case N00B_BI_COPY:
        STACK_REQUIRE_VALUES(1);

        obj = n00b_copy(tstate->sp->rvalue);

        tstate->sp->rvalue = obj;
        return;

        // clang-format off
#define BINOP_OBJ(_op, _t) \
    obj = (n00b_obj_t)(uintptr_t)((_t)(uintptr_t)tstate->sp[1].rvalue \
     _op (_t)(uintptr_t) tstate->sp[0].rvalue)
        // clang-format on

#define BINOP_INTEGERS(_op)       \
    case N00B_T_I8:               \
        BINOP_OBJ(_op, int8_t);   \
        break;                    \
    case N00B_T_I32:              \
        BINOP_OBJ(_op, int32_t);  \
        break;                    \
    case N00B_T_INT:              \
        BINOP_OBJ(_op, int64_t);  \
        break;                    \
    case N00B_T_BYTE:             \
        BINOP_OBJ(_op, uint8_t);  \
        break;                    \
    case N00B_T_CHAR:             \
        BINOP_OBJ(_op, uint32_t); \
        break;                    \
    case N00B_T_U32:              \
        BINOP_OBJ(_op, uint32_t); \
        break;                    \
    case N00B_T_UINT:             \
        BINOP_OBJ(_op, uint64_t); \
        break

#define BINOP_FLOATS(_op)       \
    case N00B_T_F32:            \
        BINOP_OBJ(_op, float);  \
        break;                  \
    case N00B_T_F64:            \
        BINOP_OBJ(_op, double); \
        break

    case N00B_BI_ADD:
        STACK_REQUIRE_VALUES(2);

        t = n00b_get_my_type(tstate->sp->rvalue);
        switch (t->typeid) {
            BINOP_INTEGERS(+);
            BINOP_FLOATS(+);
        default:
            obj = n00b_add(tstate->sp[1].rvalue, tstate->sp[0].rvalue);
        }
        ++tstate->sp;
        tstate->sp->rvalue = obj;
        return;
    case N00B_BI_SUB:
        STACK_REQUIRE_VALUES(2);
        t = n00b_get_my_type(tstate->sp->rvalue);
        switch (t->typeid) {
            BINOP_INTEGERS(-);
            BINOP_FLOATS(-);
        default:
            obj = n00b_sub(tstate->sp[1].rvalue, tstate->sp[0].rvalue);
        }
        ++tstate->sp;
        tstate->sp->rvalue = obj;
        return;
    case N00B_BI_MUL:
        STACK_REQUIRE_VALUES(2);
        t = n00b_get_my_type(tstate->sp->rvalue);
        switch (t->typeid) {
            BINOP_INTEGERS(*);
            BINOP_FLOATS(*);
        default:
            obj = n00b_mul(tstate->sp[1].rvalue, tstate->sp[0].rvalue);
        }
        ++tstate->sp;
        tstate->sp->rvalue = obj;
        return;
    case N00B_BI_DIV:
        STACK_REQUIRE_VALUES(2);
        t = n00b_get_my_type(tstate->sp->rvalue);
        switch (t->typeid) {
            BINOP_INTEGERS(/);
            BINOP_FLOATS(/);
        default:
            obj = n00b_div(tstate->sp[1].rvalue, tstate->sp[0].rvalue);
        }
        ++tstate->sp;
        tstate->sp->rvalue = obj;
        return;
    case N00B_BI_MOD:
        STACK_REQUIRE_VALUES(2);
        t = n00b_get_my_type(tstate->sp->rvalue);
        switch (t->typeid) {
            BINOP_INTEGERS(%);
        default:
            obj = n00b_mod(tstate->sp[1].rvalue, tstate->sp[0].rvalue);
        }
        ++tstate->sp;
        tstate->sp->rvalue = obj;
        return;
    case N00B_BI_EQ:
        STACK_REQUIRE_VALUES(2);

        b = n00b_eq(i->type_info,
                    tstate->sp[1].rvalue,
                    tstate->sp[0].rvalue);

        ++tstate->sp;
        tstate->sp->rvalue = (n00b_obj_t)b;
        return;
    case N00B_BI_LT:
        STACK_REQUIRE_VALUES(2);

        b = n00b_lt(i->type_info,
                    tstate->sp[1].rvalue,
                    tstate->sp[0].rvalue);

        ++tstate->sp;
        tstate->sp->rvalue = (n00b_obj_t)b;
        return;
    case N00B_BI_GT:
        STACK_REQUIRE_VALUES(2);

        b = n00b_gt(i->type_info,
                    tstate->sp[1].rvalue,
                    tstate->sp[0].rvalue);

        ++tstate->sp;
        tstate->sp->rvalue = (n00b_obj_t)b;
        return;
    case N00B_BI_LEN:
        STACK_REQUIRE_VALUES(1);
        tstate->sp->rvalue = (n00b_obj_t)n00b_len(tstate->sp->rvalue);
        return;
    case N00B_BI_INDEX_GET:
        STACK_REQUIRE_VALUES(2);
        // index = sp[0]
        // container = sp[1]
        obj = n00b_index_get(tstate->sp[1].rvalue, tstate->sp[0].rvalue);

        ++tstate->sp;
        tstate->sp->rvalue = obj;
        return;
    case N00B_BI_INDEX_SET:
        STACK_REQUIRE_VALUES(3);
        // index = sp[0]
        // container = sp[1]
        // value = sp[2]

        n00b_index_set(tstate->sp[2].rvalue,
                       tstate->sp[0].rvalue,
                       tstate->sp[1].rvalue);

        tstate->sp += 3;
        return;
    case N00B_BI_SLICE_GET:
        STACK_REQUIRE_VALUES(3);
        obj = n00b_slice_get(tstate->sp[2].rvalue,
                             (int64_t)tstate->sp[1].rvalue,
                             (int64_t)tstate->sp[0].rvalue);

        tstate->sp += 2;
        // type is already correct on the stack, since we're writing
        // over the container location and this is a slice.
        tstate->sp->rvalue = obj;
        return;
    case N00B_BI_SLICE_SET:
        STACK_REQUIRE_VALUES(4);
        // container = sp[3]
        // endIx = sp[2]
        // startIx = sp[1]
        // value = sp[0]

        n00b_slice_set(tstate->sp[3].rvalue,
                       (int64_t)tstate->sp[2].rvalue,
                       (int64_t)tstate->sp[1].rvalue,
                       tstate->sp[0].rvalue);

        tstate->sp += 4;
        return;

        // Note: we pass the full container type through the type field;
        // we assume the item type is a tuple of the items types, decaying
        // to the actual item type if only one item.
    case N00B_BI_CONTAINER_LIT:
        do {
            uint64_t     n  = tstate->sp[0].uint;
            n00b_type_t *ct = i->type_info;
            n00b_list_t *xl;

            ++tstate->sp;

            xl = n00b_new(n00b_type_list(n00b_type_ref()),
                          n00b_kw("length", n00b_ka(n)));

            while (n--) {
                n00b_list_set(xl, n, tstate->sp[0].vptr);
                ++tstate->sp;
            }

            --tstate->sp;

            tstate->sp[0].rvalue = n00b_container_literal(ct, xl, NULL);
        } while (0);
        return;

        // Nim version has others, but they're missing from n00b_builtin_type_fn
        // FIDiv, FFDiv, FShl, FShr, FBand, FBor, FBxor, FDictIndex, FAssignDIx,
        // FContainerLit
        //
        // Assumptions:
        //   * FIDiv and FFDiv combined -> FDiv
        //   * FDictIndex folded into FIndex (seems supported by dict code)
        //   * FAssignDIx folded into FAssignIx (seems supported by dict code)
        //
        // At least some of of these should be added to n00b as well,
        // but doing that is going to touch a lot of things, so it should be
        // done later, probably best done in a PR all on its own
    default:
        // Not implemented yet, or not called via N00B_ZTCall.
        break;
    }

    N00B_CRAISE("invalid tcall instruction");
}

static void
n00b_vm_0call(n00b_vmthread_t *tstate, n00b_zinstruction_t *i, int64_t ix)
{
    STACK_REQUIRE_SLOTS(2);

    // combine pc / module id together and push it onto the stack for recovery
    // on return
    --tstate->sp;
    tstate->sp->uint = ((uint64_t)tstate->pc << 32u) | tstate->current_module->module_id;
    --tstate->sp;

    tstate->sp->vptr = tstate->fp;
    tstate->fp       = (n00b_value_t *)&tstate->sp->vptr;

    n00b_module_t *old_module = tstate->current_module;

    n00b_zfn_info_t *fn = n00b_list_get(tstate->vm->obj->func_info,
                                        ix - 1,
                                        NULL);

    int32_t call_pc = tstate->pc;

    tstate->pc             = fn->offset;
    tstate->current_module = n00b_list_get(tstate->vm->obj->module_contents,
                                           fn->mid,
                                           NULL);

    n00b_zinstruction_t *nexti = n00b_list_get(tstate->current_module->instructions,
                                               tstate->pc,
                                               NULL);

    // push a frame onto the call stack
    n00b_vmframe_push(tstate,
                      i,
                      old_module,
                      call_pc,
                      tstate->current_module,
                      fn,
                      nexti->line_no);
}

static void
n00b_vm_call_module(n00b_vmthread_t *tstate, n00b_zinstruction_t *i)
{
    STACK_REQUIRE_SLOTS(2);

    // combine pc / module id together and push it onto the stack for recovery
    // on return
    --tstate->sp;
    tstate->sp->uint = ((uint64_t)tstate->pc << 32u) | tstate->current_module->module_id;
    --tstate->sp;

    tstate->sp->vptr = tstate->fp;
    tstate->fp       = (n00b_value_t *)&tstate->sp->vptr;

    n00b_module_t *old_module = tstate->current_module;

    int32_t call_pc        = tstate->pc;
    tstate->pc             = 0;
    tstate->current_module = n00b_list_get(tstate->vm->obj->module_contents,
                                           i->module_id,
                                           NULL);

    n00b_zinstruction_t *nexti = n00b_list_get(tstate->current_module->instructions,
                                               tstate->pc,
                                               NULL);

    // push a frame onto the call stack
    n00b_vmframe_push(tstate,
                      i,
                      old_module,
                      call_pc,
                      tstate->current_module,
                      NULL,
                      nexti->line_no);
}

static inline uint64_t
ffi_possibly_box(n00b_vmthread_t     *tstate,
                 n00b_zinstruction_t *i,
                 n00b_type_t         *dynamic_type,
                 int                  local_param)
{
    // TODO: Handle varargs.
    if (dynamic_type == NULL) {
        return tstate->sp[local_param].uint;
    }

    n00b_type_t *param = n00b_type_get_param(dynamic_type, local_param);
    param              = n00b_resolve_and_unbox(param);

    if (n00b_type_is_concrete(param)) {
        return tstate->sp[local_param].uint;
    }

    n00b_type_t *actual = n00b_type_get_param(i->type_info, local_param);

    actual = n00b_resolve_and_unbox(actual);

    if (n00b_type_is_value_type(actual)) {
        n00b_box_t box = {
            .u64 = tstate->sp[local_param].uint,
        };

        return (uint64_t)n00b_box_obj(box, actual);
    }

    return tstate->sp[local_param].uint;
}

static inline void
ffi_possible_ret_munge(n00b_vmthread_t *tstate, n00b_type_t *at, n00b_type_t *ft)
{
    // at == actual type; ft == formal type.
    at = n00b_resolve_and_unbox(at);
    ft = n00b_resolve_and_unbox(ft);

    if (!n00b_type_is_concrete(at)) {
        if (n00b_type_is_concrete(ft) && n00b_type_is_value_type(ft)) {
            tstate->r0 = n00b_box_obj((n00b_box_t){.v = tstate->r0}, ft);
        }
    }
    else {
        if (n00b_type_is_value_type(at) && !n00b_type_is_concrete(ft)) {
            tstate->r0 = n00b_unbox_obj(tstate->r0).v;
        }
    }

    return;
}

static void
n00b_vm_ffi_call(n00b_vmthread_t     *tstate,
                 n00b_zinstruction_t *instr,
                 int64_t              ix,
                 n00b_type_t         *dynamic_type)
{
    n00b_ffi_decl_t *decl = n00b_list_get(tstate->vm->obj->ffi_info,
                                          instr->arg,
                                          NULL);

    if (decl == NULL) {
        N00B_CRAISE("Could not load external function.");
    }

    n00b_zffi_cif *ffiinfo     = &decl->cif;
    int            local_param = 0;
    void         **args;

    if (!ffiinfo->cif.nargs) {
        args = NULL;
    }
    else {
        args  = n00b_gc_array_value_alloc(void *, ffiinfo->cif.nargs);
        int n = ffiinfo->cif.nargs;

        for (unsigned int i = 0; i < ffiinfo->cif.nargs; i++) {
            // clang-format off
	    --n;

	    if (ffiinfo->str_convert &&
		n < 63 &&
		((1 << n) & ffiinfo->str_convert)) {

		n00b_string_t *s = (n00b_string_t *)tstate->sp[i].rvalue;
		args[n]       = &s->data;
            }
            // clang-format on
            else {
                uint64_t raw;
                raw = ffi_possibly_box(tstate,
                                       instr,
                                       dynamic_type,
                                       i);

                n00b_box_t value = {.u64 = raw};

                n00b_box_t *box = n00b_new(n00b_type_box(n00b_type_i64()),
                                           value);
                args[n]         = n00b_ref_via_ffi_type(box,
                                                ffiinfo->cif.arg_types[n]);
            }

            // From old heap code.
            // if (n < 63 && ((1 << n) & ffiinfo->hold_info)) {
            // n00b_gc_add_hold(tstate->sp[i].rvalue);
            // }
        }
    }

    ffi_call(&ffiinfo->cif, ffiinfo->fptr, &tstate->r0, args);

    if (ffiinfo->str_convert & (1UL << 63)) {
        char *s    = (char *)tstate->r0;
        tstate->r0 = n00b_cstring(s);
    }

    if (dynamic_type != NULL) {
        ffi_possible_ret_munge(tstate,
                               n00b_type_get_param(dynamic_type, local_param),
                               n00b_type_get_param(instr->type_info,
                                                   local_param));
    }
}

static void
n00b_vm_foreign_z_call(n00b_vmthread_t *tstate, n00b_zinstruction_t *i, int64_t ix)
{
    // TODO foreign_z_call
}

n00b_zcallback_t *
n00b_new_zcallback()
{
    return n00b_new(n00b_type_callback());
}

static void
n00b_vm_run_callback(n00b_vmthread_t *tstate, n00b_zinstruction_t *i)
{
    STACK_REQUIRE_VALUES(1);

    n00b_zcallback_t *cb = tstate->sp->callback;
    ++tstate->sp;

    if (cb->ffi) {
        n00b_vm_ffi_call(tstate, i, cb->impl, cb->tid);
    }
    else if (tstate->running) {
        // The generated code will, in this branch, push the result
        // if merited.
        n00b_vm_0call(tstate, i, cb->impl);
    }
    else {
        n00b_vm_foreign_z_call(tstate, i, cb->impl);
    }
}

static void
n00b_vm_return(n00b_vmthread_t *tstate, n00b_zinstruction_t *i)
{
    if (!tstate->num_frames) {
        N00B_CRAISE("call stack underflow");
    }

    tstate->sp = tstate->fp;
    tstate->fp = tstate->sp->vptr;

    uint64_t v             = tstate->sp[1].uint;
    tstate->pc             = (v >> 32u);
    tstate->current_module = n00b_list_get(tstate->vm->obj->module_contents,
                                           (v & 0xFFFFFFFF),
                                           NULL);

    tstate->sp += 2;

    --tstate->num_frames; // pop call frame
}

static int
n00b_vm_runloop(n00b_vmthread_t *tstate_arg)
{
    // Other portions of the n00b API use exceptions to communicate errors,
    // and so we need to be prepared for that. We'll also use exceptions to
    // raise errors as well since already having to handle exceptions
    // complicates doing anything else, mainly because you can't use return in
    // an exception block.

    // For many ABIs, tstate_arg will be passed in a register that is not
    // preserved. Odds are pretty good that the compiler will save it on the
    // stack for its own reasons, but it doesn't have to. By declaring tstate
    // as volatile and assigning tstate_arg to it, we ensure that it's available
    // even after setjmp returns from a longjmp via tstate, but not tstate_arg!
    // This is crucial, because we need it in the except block and we need it
    // after the try block ends.
    n00b_vmthread_t *volatile const tstate = tstate_arg;
    n00b_zcallback_t *cb;
    n00b_mem_ptr     *static_mem = tstate->vm->obj->static_contents->items;

    // This temporary is used to hold popped operands during binary
    // operations.
    union {
        uint64_t uint;
        int64_t  sint;
        double   dbl;
    } rhs;

    N00B_TRY
    {
        for (;;) {
            n00b_zinstruction_t *i;

            i = n00b_list_get(tstate->current_module->instructions,
                              tstate->pc,
                              NULL);

#ifdef N00B_VM_DEBUG
            static bool  debug_on = (bool)(N00B_VM_DEBUG_DEFAULT);
            static char *debug_fmt_str =
                "\n«i»> «#» (PC@«#:p»; SP@«#:p»; "
                "FP@«#:p»; a = «#»; i = «#»; m = «#»)";

            if (debug_on && i->op != N00B_ZNop) {
                int num_stack_items = &tstate->stack[N00B_STACK_SIZE] - tstate->sp;
                printf("stack has %d items on it: ", num_stack_items);
                for (int i = 0; i < num_stack_items; i++) {
                    if (&tstate->sp[i] == tstate->fp) {
                        printf("\e[34m[%p]\e[0m ", tstate->sp[i].vptr);
                    }
                    else {
                        // stored program counter and module id.
                        if (&tstate->sp[i - 1] == tstate->fp) {
                            printf("\e[32m[pc: 0x%llx module: %lld]\e[0m ",
                                   tstate->sp[i].uint >> 28,
                                   tstate->sp[i].uint & 0xffffffff);
                        }
                        else {
                            if (&tstate->sp[i] > tstate->fp) {
                                // Older frames.
                                printf("\e[31m[%p]\e[0m ", tstate->sp[i].vptr);
                            }
                            else {
                                // This frame.
                                printf("\e[33m[%p]\e[0m ", tstate->sp[i].vptr);
                            }
                        }
                    }
                }
                printf("\n");
                n00b_printf(
                    debug_fmt_str,
                    n00b_fmt_instr_name(i),
                    (int64_t)tstate->pc * 16,
                    (uint64_t)(void *)tstate->sp,
                    (uint64_t)(void *)tstate->fp,
                    (int64_t)i->arg,
                    (int64_t)i->immediate,
                    (uint64_t)tstate->current_module->module_id);
            }

#endif

            switch (i->op) {
            case N00B_ZNop:
                break;
            case N00B_ZMoveSp:
                if (i->arg > 0) {
                    STACK_REQUIRE_SLOTS(i->arg);
                }
                else {
                    STACK_REQUIRE_VALUES(i->arg);
                }
                tstate->sp -= i->arg;
                break;
            case N00B_ZPushConstObj:
                STACK_REQUIRE_SLOTS(1);
                --tstate->sp;
                *tstate->sp = (n00b_value_t){
                    .uint = static_mem[i->arg].nonpointer,
                };
                break;
            case N00B_ZPushConstRef:
                STACK_REQUIRE_SLOTS(1);
                --tstate->sp;
                *tstate->sp = (n00b_value_t){
                    .rvalue = (void *)static_mem + i->arg,
                };
                break;
            case N00B_ZDeref:
                STACK_REQUIRE_VALUES(1);
                tstate->sp->uint = *(uint64_t *)tstate->sp->uint;
                break;
            case N00B_ZPushImm:
                STACK_REQUIRE_SLOTS(1);
                --tstate->sp;
                *tstate->sp = (n00b_value_t){
                    .rvalue = (n00b_obj_t)i->immediate,
                };
                break;
            case N00B_ZPushLocalObj:
                STACK_REQUIRE_SLOTS(1);
                --tstate->sp;
                tstate->sp->rvalue = tstate->fp[-i->arg].rvalue;
                break;
            case N00B_ZPushLocalRef:
                STACK_REQUIRE_SLOTS(1);
                --tstate->sp;
                *tstate->sp = (n00b_value_t){
                    .lvalue = &tstate->fp[-i->arg].rvalue,
                };
                break;
            case N00B_ZPushStaticObj:
                STACK_REQUIRE_SLOTS(1);
                --tstate->sp;
                *tstate->sp = (n00b_value_t){
                    .rvalue = *(n00b_obj_t *)n00b_vm_variable(tstate, i),
                };
                break;
            case N00B_ZPushStaticRef:
                STACK_REQUIRE_SLOTS(1);
                --tstate->sp;
                *tstate->sp = (n00b_value_t){
                    .lvalue = n00b_vm_variable(tstate, i),
                };
                break;
            case N00B_ZDupTop:
                STACK_REQUIRE_VALUES(1);
                STACK_REQUIRE_SLOTS(1);
                --tstate->sp;
                tstate->sp[0] = tstate->sp[1];
                break;
            case N00B_ZPop:
                STACK_REQUIRE_VALUES(1);
                ++tstate->sp;
                break;
            case N00B_ZJz:
                STACK_REQUIRE_VALUES(1);
                if (n00b_value_iszero(tstate->sp->rvalue)) {
                    tstate->pc = i->arg;
                    continue;
                }
                ++tstate->sp;
                break;
            case N00B_ZJnz:
                STACK_REQUIRE_VALUES(1);
                if (!n00b_value_iszero(tstate->sp->rvalue)) {
                    tstate->pc = i->arg;
                    continue;
                }
                ++tstate->sp;
                break;
            case N00B_ZJ:
                tstate->pc = i->arg;
                continue;
            case N00B_ZAdd:
                STACK_REQUIRE_VALUES(2);
                rhs.sint = tstate->sp[0].sint;
                ++tstate->sp;
                tstate->sp[0].uint += rhs.sint;
                break;
            case N00B_ZSub:
                STACK_REQUIRE_VALUES(2);
                rhs.sint = tstate->sp[0].sint;
                ++tstate->sp;

                tstate->sp[0].sint -= rhs.sint;
                break;
            case N00B_ZSubNoPop:
                STACK_REQUIRE_VALUES(2);
                STACK_REQUIRE_SLOTS(1);
                --tstate->sp;
                tstate->sp[0].sint = tstate->sp[2].sint - tstate->sp[1].sint;
                break;
            case N00B_ZMul:
                STACK_REQUIRE_VALUES(2);
                rhs.sint = tstate->sp[0].sint;
                ++tstate->sp;
                tstate->sp[0].uint *= rhs.sint;
                break;
            case N00B_ZDiv:
                STACK_REQUIRE_VALUES(2);
                rhs.sint = tstate->sp[0].sint;
                ++tstate->sp;
                if (rhs.sint == 0) {
                    N00B_CRAISE("Division by zero error.");
                }
                tstate->sp[0].sint /= rhs.sint;
                break;
            case N00B_ZMod:
                STACK_REQUIRE_VALUES(2);
                rhs.sint = tstate->sp[0].sint;
                ++tstate->sp;
                tstate->sp[0].uint %= rhs.sint;
                break;
            case N00B_ZUAdd:
                STACK_REQUIRE_VALUES(2);
                rhs.uint = tstate->sp[0].uint;
                ++tstate->sp;
                tstate->sp[0].uint += rhs.uint;
                break;
            case N00B_ZUSub:
                STACK_REQUIRE_VALUES(2);
                rhs.uint = tstate->sp[0].uint;
                ++tstate->sp;
                tstate->sp[0].uint -= rhs.uint;
                break;
            case N00B_ZUMul:
                STACK_REQUIRE_VALUES(2);
                rhs.uint = tstate->sp[0].uint;
                ++tstate->sp;
                tstate->sp[0].uint *= rhs.uint;
                break;
            case N00B_ZUDiv:
                STACK_REQUIRE_VALUES(2);
                rhs.uint = tstate->sp[0].uint;
                ++tstate->sp;
                tstate->sp[0].uint /= rhs.uint;
                break;
            case N00B_ZUMod:
                STACK_REQUIRE_VALUES(2);
                rhs.uint = tstate->sp[0].uint;
                ++tstate->sp;
                tstate->sp[0].uint %= rhs.uint;
                break;
            case N00B_ZFAdd:
                STACK_REQUIRE_VALUES(2);
                rhs.dbl = tstate->sp[0].dbl;
                ++tstate->sp;
                tstate->sp[0].dbl += rhs.dbl;
                break;
            case N00B_ZFSub:
                STACK_REQUIRE_VALUES(2);
                rhs.dbl = tstate->sp[0].dbl;
                ++tstate->sp;
                tstate->sp[0].dbl -= rhs.dbl;
                break;
            case N00B_ZFMul:
                STACK_REQUIRE_VALUES(2);
                rhs.dbl = tstate->sp[0].dbl;
                ++tstate->sp;
                tstate->sp[0].dbl *= rhs.dbl;
                break;
            case N00B_ZFDiv:
                STACK_REQUIRE_VALUES(2);
                rhs.dbl = tstate->sp[0].dbl;
                ++tstate->sp;
                tstate->sp[0].dbl /= rhs.dbl;
                break;
            case N00B_ZBOr:
                STACK_REQUIRE_VALUES(2);
                rhs.uint = tstate->sp[0].uint;
                ++tstate->sp;
                tstate->sp[0].uint |= rhs.uint;
                break;
            case N00B_ZBAnd:
                STACK_REQUIRE_VALUES(2);
                rhs.uint = tstate->sp[0].uint;
                ++tstate->sp;
                tstate->sp[0].uint &= rhs.uint;
                break;
            case N00B_ZShl:
                STACK_REQUIRE_VALUES(2);
                rhs.uint = tstate->sp[0].uint;
                ++tstate->sp;
                tstate->sp[0].uint <<= rhs.uint;
                break;
            case N00B_ZShlI:
                STACK_REQUIRE_VALUES(1);
                rhs.uint           = tstate->sp[0].uint;
                tstate->sp[0].uint = i->arg << tstate->sp[0].uint;
                break;
            case N00B_ZShr:
                STACK_REQUIRE_VALUES(2);
                rhs.uint = tstate->sp[0].uint;
                ++tstate->sp;
                tstate->sp[0].uint >>= rhs.uint;
                break;
            case N00B_ZBXOr:
                STACK_REQUIRE_VALUES(2);
                rhs.uint = tstate->sp[0].uint;
                ++tstate->sp;
                tstate->sp[0].uint ^= rhs.uint;
                break;
            case N00B_ZBNot:
                STACK_REQUIRE_VALUES(1);
                tstate->sp[0].uint = ~tstate->sp[0].uint;
                break;
            case N00B_ZNot:
                STACK_REQUIRE_VALUES(1);
                tstate->sp->uint = !tstate->sp->uint;
                break;
            case N00B_ZAbs:
                STACK_REQUIRE_VALUES(1);
                do {
                    // Done w/o a branch; since value is signed,
                    // when we shift right, if it's negative, we sign
                    // extend to all ones. Meaning, we end up with
                    // either 64 ones or 64 zeros.
                    //
                    // Then, if we DO flip the sign, we need to add back 1;
                    // if we don't, we add back in 0.
                    int64_t  value = (int64_t)tstate->sp->uint;
                    uint64_t tmp   = value >> 63;
                    value ^= tmp;
                    value += tmp & 1;
                    tstate->sp->uint = (uint64_t)value;
                } while (0);
                break;
            case N00B_ZGetSign:
                STACK_REQUIRE_VALUES(1);
                do {
                    // Here, we get tmp to the point where it's either -1
                    // or 0, then OR in a 1, which will do nothing to -1,
                    // and will turn the 0 to 1.
                    tstate->sp->sint >>= 63;
                    tstate->sp->sint |= 1;
                } while (0);
                break;
            case N00B_ZHalt:
                N00B_JUMP_TO_TRY_END();
            case N00B_ZSwap:
                STACK_REQUIRE_VALUES(2);
                do {
                    n00b_value_t tmp = tstate->sp[0];
                    tstate->sp[0]    = tstate->sp[1];
                    tstate->sp[1]    = tmp;
                } while (0);
                break;
            case N00B_ZLoadFromAttr:
                STACK_REQUIRE_VALUES(1);
                do {
                    bool           found = true;
                    n00b_string_t *key   = tstate->sp->vptr;
                    n00b_obj_t     val;
                    uint64_t       flag = i->immediate;

                    if (flag) {
                        val = n00b_vm_attr_get(tstate, key, &found);
                    }
                    else {
                        val = n00b_vm_attr_get(tstate, key, NULL);
                    }

                    // If we didn't pass the reference to `found`,
                    // then an exception generally gets thrown if the
                    // attr doesn't exist, which is why `found` is
                    // true by default.
                    if (found && n00b_type_is_value_type(i->type_info)) {
                        n00b_box_t box     = n00b_unbox_obj((n00b_box_t *)val);
                        tstate->sp[0].vptr = box.v;
                    }
                    else {
                        *tstate->sp = (n00b_value_t){
                            .lvalue = val,
                        };
                    }

                    // Only push the status if it was explicitly requested.
                    if (flag) {
                        *--tstate->sp = (n00b_value_t){
                            .uint = found ? 1 : 0,
                        };
                    }
                } while (0);
                break;
            case N00B_ZAssignAttr:
                STACK_REQUIRE_VALUES(2);
                do {
                    void *val = tstate->sp[0].vptr;

                    if (n00b_type_is_value_type(i->type_info)) {
                        n00b_box_t item = {
                            .v = val,
                        };

                        val = n00b_box_obj(item, i->type_info);
                    }

                    n00b_vm_attr_set(tstate,
                                     tstate->sp[1].vptr,
                                     val,
                                     i->type_info,
                                     i->arg != 0,
                                     false,
                                     false);
                    tstate->sp += 2;
                } while (0);
                break;
            case N00B_ZLockOnWrite:
                STACK_REQUIRE_VALUES(1);
                do {
                    n00b_string_t *key = tstate->sp->vptr;
                    n00b_vm_attr_lock(tstate, key, true);
                } while (0);
                break;
            case N00B_ZLoadFromView:
                STACK_REQUIRE_VALUES(2);
                STACK_REQUIRE_SLOTS(2); // Usually 1, except w/ dict.
                do {
                    uint64_t    obj_len   = tstate->sp->uint;
                    void      **view_slot = &(tstate->sp + 1)->rvalue;
                    char       *p         = *(char **)view_slot;
                    n00b_box_t *box       = (n00b_box_t *)p;

                    --tstate->sp;

                    switch (obj_len) {
                    case 1:
                        tstate->sp->uint = box->u8;
                        *view_slot       = (void *)(p + 1);
                        break;
                    case 2:
                        tstate->sp->uint = box->u16;
                        *view_slot       = (void *)(p + 2);
                        break;
                    case 4:
                        tstate->sp->uint = box->u32;
                        *view_slot       = (void *)(p + 4);
                        break;
                    case 8:
                        tstate->sp->uint = box->u64;
                        *view_slot       = (void *)(p + 8);
                        // This is the only size that can be a dict.
                        // Push the value on first.
                        if (i->arg) {
                            --tstate->sp;
                            p += 8;
                            box              = (n00b_box_t *)p;
                            tstate->sp->uint = box->u64;
                            *view_slot       = (p + 8);
                        }
                        break;
                    default:
                        do {
                            uint64_t count  = (uint64_t)(tstate->r1);
                            uint64_t bit_ix = (count - 1) % 64;
                            uint64_t val    = **(uint64_t **)view_slot;

                            tstate->sp->uint = val & (1 << bit_ix);

                            if (bit_ix == 63) {
                                *view_slot += 1;
                            }
                        } while (0);
                    }
                } while (0);
                break;
            case N00B_ZStoreImm:
                *(n00b_obj_t *)n00b_vm_variable(tstate, i) = (n00b_obj_t)i->immediate;
                break;
            case N00B_ZPushObjType:
                // Name is a a bit of a mis-name because it also pops
                // the object. Should be ZReplaceObjWType
                STACK_REQUIRE_SLOTS(1);
                do {
                    n00b_type_t *type = n00b_get_my_type(tstate->sp->rvalue);

                    *tstate->sp = (n00b_value_t){
                        .rvalue = type,
                    };
                } while (0);
                break;
            case N00B_ZTypeCmp:
                STACK_REQUIRE_VALUES(2);
                do {
                    n00b_type_t *t1 = tstate->sp[0].rvalue;
                    n00b_type_t *t2 = tstate->sp[1].rvalue;

                    ++tstate->sp;

                    // Does NOT check for coercible.
                    tstate->sp->uint = (uint64_t)n00b_types_are_compat(t1,
                                                                       t2,
                                                                       NULL);
                } while (0);
                break;
            case N00B_ZCmp:
                STACK_REQUIRE_VALUES(2);
                SIMPLE_COMPARE(==);
                break;
            case N00B_ZLt:
                STACK_REQUIRE_VALUES(2);
                SIMPLE_COMPARE(<);
                break;
            case N00B_ZLte:
                STACK_REQUIRE_VALUES(2);
                SIMPLE_COMPARE(<=);
                break;
            case N00B_ZGt:
                STACK_REQUIRE_VALUES(2);
                SIMPLE_COMPARE(>);
                break;
            case N00B_ZGte:
                STACK_REQUIRE_VALUES(2);
                SIMPLE_COMPARE(>=);
                break;
            case N00B_ZULt:
                STACK_REQUIRE_VALUES(2);
                SIMPLE_COMPARE_UNSIGNED(<);
                break;
            case N00B_ZULte:
                STACK_REQUIRE_VALUES(2);
                SIMPLE_COMPARE_UNSIGNED(<=);
                break;
            case N00B_ZUGt:
                STACK_REQUIRE_VALUES(2);
                SIMPLE_COMPARE_UNSIGNED(>);
                break;
            case N00B_ZUGte:
                STACK_REQUIRE_VALUES(2);
                SIMPLE_COMPARE_UNSIGNED(>=);
                break;
            case N00B_ZNeq:
                STACK_REQUIRE_VALUES(2);
                SIMPLE_COMPARE(!=);
                break;
            case N00B_ZGteNoPop:
                STACK_REQUIRE_VALUES(2);
                STACK_REQUIRE_SLOTS(1);
                do {
                    uint64_t v1 = tstate->sp->uint;
                    uint64_t v2 = (tstate->sp + 1)->uint;
                    --tstate->sp;
                    tstate->sp->uint = (uint64_t)(v2 >= v1);
                } while (0);
                break;
            case N00B_ZCmpNoPop:
                STACK_REQUIRE_VALUES(2);
                STACK_REQUIRE_SLOTS(1);
                do {
                    uint64_t v1 = tstate->sp->uint;
                    uint64_t v2 = (tstate->sp + 1)->uint;
                    --tstate->sp;
                    tstate->sp->uint = (uint64_t)(v2 == v1);
                } while (0);
                break;
            case N00B_ZUnsteal:
                STACK_REQUIRE_VALUES(1);
                STACK_REQUIRE_SLOTS(1);
                *(tstate->sp - 1) = (n00b_value_t){
                    .static_ptr = tstate->sp->static_ptr & 0x07,
                };
                tstate->sp->static_ptr &= ~(0x07ULL);
                --tstate->sp;
                break;
            case N00B_ZTCall:
                n00b_vm_tcall(tstate, i);
                break;
            case N00B_Z0Call:
                n00b_vm_0call(tstate, i, i->arg);
                break;
            case N00B_ZCallModule:
                n00b_vm_call_module(tstate, i);
                break;
            case N00B_ZRunCallback:
                n00b_vm_run_callback(tstate, i);
                break;
            case N00B_ZPushFfiPtr:;
                STACK_REQUIRE_VALUES(1);
                cb = n00b_new_zcallback();

                *cb = (n00b_zcallback_t){
                    .name       = tstate->sp->vptr,
                    .tid        = i->type_info,
                    .impl       = i->arg,
                    .ffi        = true,
                    .skip_boxes = (bool)i->immediate,
                };

                tstate->sp->vptr = cb;
                break;
            case N00B_ZPushVmPtr:;
                STACK_REQUIRE_VALUES(1);
                cb = n00b_new_zcallback();

                *cb = (n00b_zcallback_t){
                    .name = tstate->sp->vptr,
                    .tid  = i->type_info,
                    .impl = i->arg,
                    .mid  = i->module_id,
                };

                tstate->sp->vptr = cb;
                break;
            case N00B_ZRet:
                n00b_vm_return(tstate, i);
                break;
            case N00B_ZModuleEnter:
                n00b_vm_module_enter(tstate, i);
                break;
            case N00B_ZModuleRet:
                if (tstate->num_frames <= 2) {
                    N00B_JUMP_TO_TRY_END();
                }
                n00b_vm_return(tstate, i);
                break;
            case N00B_ZFFICall:
                n00b_vm_ffi_call(tstate, i, i->arg, NULL);
                break;
            case N00B_ZSObjNew:
                STACK_REQUIRE_SLOTS(1);
                do {
                    n00b_obj_t obj = static_mem[i->immediate].v;

                    if (NULL == obj) {
                        N00B_CRAISE("could not unmarshal");
                    }
                    obj = n00b_copy(obj);

                    --tstate->sp;
                    tstate->sp->rvalue = obj;
                } while (0);
                break;
            case N00B_ZAssignToLoc:
                STACK_REQUIRE_VALUES(2);
                *tstate->sp[0].lvalue = tstate->sp[1].rvalue;
                tstate->sp += 2;
                break;
            case N00B_ZAssert:
                STACK_REQUIRE_VALUES(1);
                if (!n00b_value_iszero(tstate->sp->rvalue)) {
                    ++tstate->sp;
                }
                else {
                    N00B_CRAISE("assertion failed");
                }
                break;
#ifdef N00B_DEV
            case N00B_ZDebug:
#ifdef N00B_VM_DEBUG
                debug_on = (bool)i->arg;
#endif
                break;
                // This is not threadsafe. It's just for early days.
            case N00B_ZPrint:
                STACK_REQUIRE_VALUES(1);
                // n00b_print(tstate->sp->rvalue, NULL);
                // The debug stream for testing. This should go away soon;
                // tee off stuff instead.
                n00b_write_blocking(n00b_stdout(), tstate->sp->rvalue, NULL);
                n00b_write_blocking(n00b_stdout(), n00b_cached_newline(), NULL);

                n00b_write(tstate->vm->run_state->print_stream,
                           tstate->sp->rvalue);
                n00b_putc(tstate->vm->run_state->print_stream, '\n');
                ++tstate->sp;
                break;
#endif
            case N00B_ZPopToR0:
                STACK_REQUIRE_VALUES(1);
                tstate->r0 = tstate->sp->rvalue;
                ++tstate->sp;
                break;
            case N00B_ZPushFromR0:
                STACK_REQUIRE_SLOTS(1);
                --tstate->sp;
                tstate->sp->rvalue = tstate->r0;
                break;
            case N00B_Z0R0c00l:
                tstate->r0 = (void *)NULL;
                break;
            case N00B_ZPopToR1:
                STACK_REQUIRE_VALUES(1);
                tstate->r1 = tstate->sp->rvalue;
                ++tstate->sp;
                break;
            case N00B_ZPushFromR1:
                STACK_REQUIRE_SLOTS(1);
                --tstate->sp;
                tstate->sp->rvalue = tstate->r1;
                break;
            case N00B_ZPopToR2:
                STACK_REQUIRE_VALUES(1);
                tstate->r2 = tstate->sp->rvalue;
                ++tstate->sp;
                break;
            case N00B_ZPushFromR2:
                STACK_REQUIRE_SLOTS(1);
                --tstate->sp;
                tstate->sp->rvalue = tstate->r2;
                break;
            case N00B_ZPopToR3:
                STACK_REQUIRE_VALUES(1);
                tstate->r3 = tstate->sp->rvalue;
                ++tstate->sp;
                break;
            case N00B_ZPushFromR3:
                --tstate->sp;
                STACK_REQUIRE_SLOTS(1);
                tstate->sp->rvalue = tstate->r3;
                break;
            case N00B_ZBox:;
                STACK_REQUIRE_VALUES(1);
                n00b_box_t item = {
                    .u64 = tstate->sp->uint,
                };
                tstate->sp->rvalue = n00b_box_obj(item, i->type_info);
                break;
            case N00B_ZUnbox:
                STACK_REQUIRE_VALUES(1);
                tstate->sp->uint = n00b_unbox_obj(tstate->sp->rvalue).u64;
                break;
            case N00B_ZUnpack:
                for (int32_t x = 1; x <= i->arg; ++x) {
                    *tstate->sp[0].lvalue = n00b_tuple_get(tstate->r1,
                                                           i->arg - x);
                    ++tstate->sp;
                }
                break;
            case N00B_ZBail:
                STACK_REQUIRE_VALUES(1);
                N00B_RAISE(tstate->sp->rvalue);
                break;
            case N00B_ZLockMutex:
                STACK_REQUIRE_VALUES(1);
                n00b_lock_acquire((n00b_lock_t *)n00b_vm_variable(tstate,
                                                                  i));
                break;
            case N00B_ZUnlockMutex:
                STACK_REQUIRE_VALUES(1);
                n00b_lock_release((n00b_lock_t *)n00b_vm_variable(tstate,
                                                                  i));
                break;
            }

            ++tstate->pc;
            // Probably could do this less often.
            n00b_gts_checkin();
        }
    }
    N00B_EXCEPT
    {
        n00b_vm_exception(tstate, N00B_X_CUR());

        // not strictly needed, but it silences a compiler warning about an
        // unused label due to having N00B_FINALLY, and it otherwise does no
        // harm.
        N00B_JUMP_TO_FINALLY();
    }
    N00B_FINALLY
    {
        // we don't need the finally block, but exceptions.h says we need to
        // have it and I don't want to trace all of the logic to confirm. So
        // we have it :)
    }
    N00B_TRY_END;

    return tstate->error ? -1 : 0;
}

int
n00b_vmthread_run(n00b_vmthread_t *tstate)
{
    n00b_assert(!tstate->running);
    tstate->running = true;

    n00b_get_tsi_ptr()->thread_runtime = tstate;

    n00b_zinstruction_t *i = n00b_list_get(tstate->current_module->instructions,
                                           tstate->pc,
                                           NULL);

    n00b_vmframe_push(tstate,
                      i,
                      tstate->current_module,
                      0,
                      tstate->current_module,
                      NULL,
                      0);

    int result = n00b_vm_runloop(tstate);
    --tstate->num_frames;
    tstate->running = false;

    return result;
}
