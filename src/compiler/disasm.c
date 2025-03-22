#define N00B_USE_INTERNAL_API
#include "n00b.h"

typedef enum : int8_t {
    fmt_unused,
    fmt_const_obj,
    fmt_const_ptr,
    fmt_offset,
    fmt_bool,
    fmt_int,
    fmt_hex,
    fmt_sym_local,
    fmt_sym_static,
    fmt_load_from_attr,
    fmt_label,
    fmt_tcall,
} inst_arg_fmt_t;

typedef struct {
    char          *name;
    inst_arg_fmt_t arg_fmt;
    inst_arg_fmt_t imm_fmt;
    unsigned int   unused      : 1;
    unsigned int   show_type   : 1;
    unsigned int   show_module : 1;
} inst_info_t;

n00b_string_t *n00b_instr_utf8_names[256] = {
    NULL,
};

static n00b_string_t *bi_fn_names[N00B_BI_NUM_FUNCS];

static const n00b_string_t *
get_bool_label(n00b_zop_t op)
{
    switch (op) {
    case N00B_ZLoadFromAttr:
        return n00b_cstring("addressof");
    case N00B_ZAssignAttr:
        return n00b_cstring("lock");
    case N00B_ZLoadFromView:
        return n00b_cstring("load kv pair");
    case N00B_ZPushFfiPtr:
        return n00b_cstring("skip boxing");
    case N00B_ZRunCallback:
        return n00b_cstring("use return");
#ifdef N00B_VM_DEBUG
    case N00B_ZDebug:
        return n00b_cstring("set debug");
#endif
    default:
        n00b_unreachable();
    }
}

const inst_info_t inst_info[256] = {
    [N00B_ZPushConstObj] = {
        .name      = "ZPushConstObj",
        .arg_fmt   = fmt_const_obj,
        .show_type = true,
    },
    [N00B_ZPushConstRef] = {
        .name      = "ZPushConstRef",
        .arg_fmt   = fmt_const_ptr,
        .show_type = true,
    },
    [N00B_ZPushLocalObj] = {
        .name    = "ZPushLocalObj",
        .arg_fmt = fmt_sym_local,
    },
    [N00B_ZPushLocalRef] = {
        .name    = "ZPushLocalRef",
        .arg_fmt = fmt_sym_local,
        .unused  = true,
    },
    [N00B_ZPushStaticObj] = {
        .name        = "ZPushStaticObj",
        .show_module = true,
        .arg_fmt     = fmt_sym_static,
    },
    [N00B_ZPushStaticRef] = {
        .name        = "ZPushStaticRef",
        .arg_fmt     = fmt_sym_static,
        .show_module = true,
        .unused      = true,
    },
    [N00B_ZJ] = {
        .name    = "ZJ",
        .arg_fmt = fmt_offset,
    },
    [N00B_ZJz] = {
        .name    = "ZJz",
        .arg_fmt = fmt_offset,
    },
    [N00B_ZJnz] = {
        .name    = "ZJnz",
        .arg_fmt = fmt_offset,
    },
    [N00B_ZDupTop] = {
        .name = "ZDupTop",
    },
    [N00B_ZLoadFromAttr] = {
        .name      = "ZLoadFromAttr",
        .arg_fmt   = fmt_bool,
        .imm_fmt   = fmt_load_from_attr,
        .show_type = true,
    },
    [N00B_ZAssignAttr] = {
        .name      = "ZAssignAttr",
        .arg_fmt   = fmt_bool,
        .show_type = true,
    },
    [N00B_ZAssignToLoc] = {
        .name      = "ZAssignToLoc",
        .show_type = true,
    },
    [N00B_ZBail] = {
        .name = "ZBail",
    },
    [N00B_ZPushImm] = {
        .name    = "ZPushImm",
        .imm_fmt = fmt_hex,
    },
    [N00B_ZNop] = {
        .name    = "ZNop",
        .imm_fmt = fmt_label,
    },
    [N00B_ZTCall] = {
        .name      = "ZTCall",
        .arg_fmt   = fmt_tcall,
        .show_type = true,
    },
    [N00B_ZBAnd] = {
        .name = "ZBAnd",
    },
    [N00B_ZPushFfiPtr] = {
        .name      = "ZPushFfiPtr",
        .arg_fmt   = fmt_int,
        .imm_fmt   = fmt_bool,
        .show_type = true,
    },
    [N00B_ZPushVmPtr] = {
        .name        = "ZPushVmPtr",
        .arg_fmt     = fmt_int,
        .show_type   = true,
        .show_module = true,
    },
    [N00B_ZRunCallback] = {
        .name      = "ZRunCallback",
        .arg_fmt   = fmt_int,
        .show_type = true,
        .imm_fmt   = fmt_bool,
    },
    [N00B_ZMoveSp] = {
        .name    = "ZMoveSp",
        .arg_fmt = fmt_int,
    },
    [N00B_ZModuleEnter] = {
        .name    = "ZModuleEnter",
        .arg_fmt = fmt_int,
    },
    [N00B_ZRet] = {
        .name = "ZRet",
    },
    [N00B_ZModuleRet] = {
        .name = "ZModuleRet",
    },
    [N00B_ZPushObjType] = {
        .name = "ZPushObjType",
    },
    [N00B_ZPop] = {
        .name = "ZPop",
    },
    [N00B_ZTypeCmp] = {
        .name = "ZTypeCmp",
    },
    [N00B_ZAdd] = {
        .name = "ZAdd",
    },
    [N00B_ZUAdd] = {
        .name = "ZUAdd",
    },
    [N00B_ZFAdd] = {
        .name = "ZFAdd",
    },
    [N00B_ZSub] = {
        .name = "ZSub",
    },
    [N00B_ZUSub] = {
        .name = "ZUSub",
    },
    [N00B_ZFSub] = {
        .name = "ZFSub",
    },
    [N00B_ZMul] = {
        .name = "ZMul",
    },
    [N00B_ZUMul] = {
        .name = "ZUMul",
    },
    [N00B_ZFMul] = {
        .name = "ZFMul",
    },
    [N00B_ZDiv] = {
        .name = "ZDiv",
    },
    [N00B_ZUDiv] = {
        .name = "ZUDiv",
    },
    [N00B_ZFDiv] = {
        .name = "ZFDiv",
    },
    [N00B_ZMod] = {
        .name = "ZMod",
    },
    [N00B_ZUMod] = {
        .name = "ZUMod",
    },
    [N00B_ZShl] = {
        .name = "ZShl",
    },
    [N00B_ZShr] = {
        .name = "ZShr",
    },
    [N00B_ZBOr] = {
        .name = "ZBOr",
    },
    [N00B_ZBXOr] = {
        .name = "ZBXOr",
    },
    [N00B_ZLt] = {
        .name = "ZLt",
    },
    [N00B_ZLte] = {
        .name = "ZLte",
    },
    [N00B_ZGt] = {
        .name = "ZGt",
    },
    [N00B_ZGte] = {
        .name = "ZGte",
    },
    [N00B_ZULt] = {
        .name = "ZULt",
    },
    [N00B_ZULte] = {
        .name = "ZULte",
    },
    [N00B_ZUGt] = {
        .name = "ZUGt",
    },
    [N00B_ZUGte] = {
        .name = "ZGteU",
    },
    [N00B_ZNeq] = {
        .name = "ZNeq",
    },
    [N00B_ZCmp] = {
        .name = "ZCmp",
    },
    [N00B_ZShlI] = {
        .name    = "ZShlI",
        .arg_fmt = fmt_hex,
    },
    [N00B_ZUnsteal] = {
        .name = "ZUnsteal",
    },
    [N00B_ZSwap] = {
        .name = "ZSwap",
    },
    [N00B_ZPopToR0] = {
        .name = "ZPopToR0",
    },
    [N00B_ZPopToR1] = {
        .name = "ZPopToR1",
    },
    [N00B_ZPopToR2] = {
        .name = "ZPopToR2",
    },
    [N00B_ZPopToR3] = {
        .name = "ZPopToR3",
    },
    [N00B_Z0R0c00l] = {
        .name    = "Z0R0(c00l)",
        .arg_fmt = fmt_int,
    },
    [N00B_ZPushFromR0] = {
        .name = "ZPushFromR0",
    },
    [N00B_ZPushFromR1] = {
        .name = "ZPushFromR1",
    },
    [N00B_ZPushFromR2] = {
        .name = "ZPushFromR2",
    },
    [N00B_ZPushFromR3] = {
        .name = "ZPushFromR3",
    },
    [N00B_ZGteNoPop] = {
        .name = "ZGteNoPop",
    },
    [N00B_ZLoadFromView] = {
        .name    = "ZLoadFromView",
        .arg_fmt = fmt_bool,
    },
    [N00B_ZAssert] = {
        .name = "ZAssert",
    },
    [N00B_ZBox] = {
        .name      = "ZBox",
        .show_type = true,
    },
    [N00B_ZUnbox] = {
        .name      = "ZUnbox",
        .show_type = true,
    },
    [N00B_ZSubNoPop] = {
        .name = "ZSubNoPop",
    },
    [N00B_ZCmpNoPop] = {
        .name = "ZCmpNoPop",
    },
    [N00B_ZAbs] = {
        .name = "ZAbs",
    },
    [N00B_ZGetSign] = {
        .name = "ZGetSign",
    },
    [N00B_ZDeref] = {
        .name = "ZDeref",
    },
    [N00B_ZNot] = {
        .name = "ZNot",
    },
    [N00B_ZBNot] = {
        .name = "ZBNot",
    },
    [N00B_ZLockMutex] = {
        .name        = "ZLockMutex",
        .arg_fmt     = fmt_hex,
        .show_module = true,
    },
    [N00B_ZUnlockMutex] = {
        .name        = "ZUnLockMutex",
        .arg_fmt     = fmt_hex,
        .show_module = true,
    },
    [N00B_Z0Call] = {
        .name        = "Z0Call",
        .arg_fmt     = fmt_hex, // Should add a fmt here.
        .show_module = true,
    },
    [N00B_ZFFICall] = {
        .name        = "ZFFICall",
        .arg_fmt     = fmt_int,
        .show_module = true,
    },
    [N00B_ZLockOnWrite] = {
        .name = "ZLockOnWrite",
    },
    [N00B_ZCallModule] = {
        .name        = "ZCallModule",
        .show_module = true,
    },
    [N00B_ZUnpack] = {
        .name    = "ZUnpack",
        .arg_fmt = fmt_int,
    },
#ifdef N00B_DEV
    [N00B_ZPrint] = {
        .name = "ZPrint",
    },
    [N00B_ZDebug] = {
        .name    = "ZDebug",
        .arg_fmt = fmt_bool,
    },
#endif
};

static void
init_disasm(void)
{
    static bool inited = false;

    if (!inited) {
        inited = true;
        n00b_gc_register_root(n00b_instr_utf8_names, 256);
        n00b_gc_register_root(bi_fn_names, N00B_BI_NUM_FUNCS);

        for (int i = 0; i < 256; i++) {
            if (inst_info[i].name != NULL) {
                n00b_instr_utf8_names[i] = n00b_cstring(inst_info[i].name);
            }
        }

        bi_fn_names[N00B_BI_TO_STR]        = n00b_cstring("$str");
        bi_fn_names[N00B_BI_FORMAT]        = n00b_cstring("$format");
        bi_fn_names[N00B_BI_FINALIZER]     = n00b_cstring("$final");
        bi_fn_names[N00B_BI_COERCIBLE]     = n00b_cstring("$can_cast");
        bi_fn_names[N00B_BI_COERCE]        = n00b_cstring("$cast");
        bi_fn_names[N00B_BI_FROM_LITERAL]  = n00b_cstring("$parse_literal");
        bi_fn_names[N00B_BI_COPY]          = n00b_cstring("$copy");
        bi_fn_names[N00B_BI_ADD]           = n00b_cstring("$add");
        bi_fn_names[N00B_BI_SUB]           = n00b_cstring("$sub");
        bi_fn_names[N00B_BI_MUL]           = n00b_cstring("$mul");
        bi_fn_names[N00B_BI_DIV]           = n00b_cstring("$div");
        bi_fn_names[N00B_BI_MOD]           = n00b_cstring("$mod");
        bi_fn_names[N00B_BI_EQ]            = n00b_cstring("$eq");
        bi_fn_names[N00B_BI_LT]            = n00b_cstring("$lt");
        bi_fn_names[N00B_BI_GT]            = n00b_cstring("$gt");
        bi_fn_names[N00B_BI_LEN]           = n00b_cstring("$len");
        bi_fn_names[N00B_BI_INDEX_GET]     = n00b_cstring("$get_item");
        bi_fn_names[N00B_BI_INDEX_SET]     = n00b_cstring("$set_item");
        bi_fn_names[N00B_BI_SLICE_GET]     = n00b_cstring("$get_slice");
        bi_fn_names[N00B_BI_SLICE_SET]     = n00b_cstring("$set_slice");
        bi_fn_names[N00B_BI_VIEW]          = n00b_cstring("$view");
        bi_fn_names[N00B_BI_ITEM_TYPE]     = n00b_cstring("$item_type");
        bi_fn_names[N00B_BI_CONTAINER_LIT] = n00b_cstring("$parse_literal");
    }
}

static n00b_string_t *
fmt_builtin_fn(int64_t value)
{
    n00b_string_t *s = bi_fn_names[value];

    if (s == NULL) {
        s = n00b_cstring("???");
    }

    return n00b_cformat("«em2»«#»«/»", s);
}

static n00b_string_t *
fmt_arg_or_imm_no_syms(n00b_vm_t *vm, n00b_zinstruction_t *instr, int i, bool imm)
{
    inst_arg_fmt_t fmt;
    int64_t        value;

    if (imm) {
        fmt   = inst_info[instr->op].imm_fmt;
        value = (int64_t)instr->immediate;
    }
    else {
        fmt   = inst_info[instr->op].arg_fmt;
        value = (int64_t)(uint64_t)instr->arg;
    }

    switch (fmt) {
    case fmt_unused:
        return n00b_cached_space();
    case fmt_const_obj:
        return n00b_cformat("«#» (const obj #«#:i»)",
                            vm->obj->static_contents->items[value].v,
                            (int64_t)value);
    case fmt_const_ptr:
        return n00b_cformat("offset to ptr: «#:x»", (int64_t)value);
    case fmt_offset:
        return n00b_cformat("target «#:x»", (int64_t)value);
    case fmt_bool:
        return n00b_cformat("«#»: «#:i»",
                            get_bool_label(instr->op),
                            (int64_t)value);
    case fmt_int:
        return n00b_cformat("«#:i»", value);
    case fmt_hex:
        return n00b_cformat("0x«#:x»", value);
    case fmt_sym_local:
        return n00b_cformat("sym stack slot offset: «#»", value);
    case fmt_sym_static:
        return n00b_cformat("static offset: «#:x»", value);
    case fmt_load_from_attr:
        return n00b_cformat("attr «em2»«#»",
                            vm->obj->static_contents->items[value].v);
    case fmt_label:
        return n00b_cformat("«em2»«#»",
                            vm->obj->static_contents->items[value].v);
    case fmt_tcall:
        return n00b_cformat("builtin call of «em2»«#»",
                            fmt_builtin_fn(value));
    default:
        n00b_unreachable();
    }
}

static n00b_string_t *
fmt_type_no_syms(n00b_zinstruction_t *instr)
{
    if (!inst_info[instr->op].show_type || !instr->type_info) {
        return n00b_cached_space();
    }

    return n00b_cformat("«#»", instr->type_info);
}

static inline n00b_string_t *
fmt_module_no_syms(n00b_zinstruction_t *instr)
{
    if (!inst_info[instr->op].show_module) {
        return n00b_cached_space();
    }

    return n00b_cformat("«#»", (int64_t)instr->module_id);
}

static inline n00b_string_t *
fmt_addr(int64_t i)
{
    return n00b_cformat("«#:x»", i);
}

n00b_string_t *
n00b_fmt_instr_name(n00b_zinstruction_t *instr)
{
    n00b_string_t *result = n00b_instr_utf8_names[instr->op];

    if (!result) {
        // This shouldn't really happen.
        return n00b_cstring("???");
    }

    return result;
}

static inline n00b_string_t *
fmt_line_no_syms(n00b_zinstruction_t *instr)
{
    return n00b_cformat("«#»", (int64_t)instr->line_no);
}

n00b_table_t *
n00b_disasm(n00b_vm_t *vm, n00b_module_t *m)
{
    init_disasm();

    n00b_table_t *tbl = n00b_table("columns", n00b_ka(7));
    int64_t       len = n00b_list_len(m->instructions);

    n00b_table_add_cell(tbl, n00b_cstring("Address"));
    n00b_table_add_cell(tbl, n00b_cstring("Instruction"));
    n00b_table_add_cell(tbl, n00b_cstring("Arg"));
    n00b_table_add_cell(tbl, n00b_cstring("Immediate"));
    n00b_table_add_cell(tbl, n00b_cstring("Type"));
    n00b_table_add_cell(tbl, n00b_cstring("Module"));
    n00b_table_add_cell(tbl, n00b_cstring("Line"));

    for (int64_t i = 0; i < len; i++) {
        n00b_zinstruction_t *ins  = n00b_list_get(m->instructions, i, NULL);
        n00b_string_t       *addr = fmt_addr(i);
        n00b_string_t       *name = n00b_fmt_instr_name(ins);
        n00b_string_t       *arg  = fmt_arg_or_imm_no_syms(vm, ins, i, false);
        n00b_string_t       *imm  = fmt_arg_or_imm_no_syms(vm, ins, i, true);
        n00b_string_t       *type = fmt_type_no_syms(ins);
        n00b_string_t       *mod  = fmt_module_no_syms(ins);
        n00b_string_t       *line = fmt_line_no_syms(ins);

        if (ins->op == N00B_ZNop) {
            n00b_table_add_cell(tbl, imm, N00B_COL_SPAN_ALL);
            continue;
        }

        n00b_table_add_cell(tbl, addr);
        n00b_table_add_cell(tbl, name);
        n00b_table_add_cell(tbl, arg);
        n00b_table_add_cell(tbl, imm);
        n00b_table_add_cell(tbl, type);
        n00b_table_add_cell(tbl, mod);
        n00b_table_add_cell(tbl, line);
    }

    return tbl;
}
