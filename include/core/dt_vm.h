#pragma once
#include "n00b.h"

// This is mostly a straight port of the original Nim code. The overriding goal
// here is to keep things simple and easy. To that end, we always want to store
// primitives naturally (unboxed) alongside n00b objects whenever possible,
// because that's how the rest of the n00b API works. These points inform the
// design that is implemented here.

// Instructions are 32 bytes each and are defined as n00b_zinstruction_t. Types
// named with a z are marshalled. Each instruction requires a single byte opcode
// that defines what the instruction does. These opcodes are described below
// with information about what they do and what other components of the
// instruction structure they use. Most instructions do not use all possible
// instruction fields, but all fields are present for all instructions to keep
// things simple.

// Each element of the stack consists of a value and a type. The type is largely
// redundant in most cases, because it ought to match the type information that
// is stored in the value object itself, but if an unboxed primitive (e.g., an
// an integer or a float) is pushed onto the stack, the type information is
// needed to be stored along with it. It must be reiterated that the core n00b
// API expects primitives to be unboxed and so the VM does as well. A suggested
// alternative to the current implementation is to use two value stacks: one
// with type information and one without type information. Code generation would
// always know which stack to reference. But ultimately the savings are minimal,
// because the type information needs to be managed in other locations that need
// to store (return register, attributes, etc.). I think it's much simpler to
// just maintain a single value stack, even though it means storing redundant
// type information.

// Some instructions encode addressing information for various types of values.
// For these instructions, a 16-bit module id specifies which module the value
// belongs to. A module id >= 0 refers to that module's global variables, and
// the instruction's arg specifies the slot in the module's table of global
// variables. If the module id is -1, the value is a local variable stored on
// the stack, and the instruction's arg specifies the offset from the frame
// pointer. There is also a special case where a module id of -2 means that the
// instruction's arg refers to an offset in the current module's static data
// table, but this appears to be a dead end in the Nim code and has not been
// implemented in C.

// For all call instructions, arguments are pushed onto the stack in reverse
// order. It is up to the caller to pop the arguments from the stack after the
// call returns. This calling convention causes some difficulty in a strictly
// stack-based virtual machine, so there is a special register used for return
// values. The typical order of operations is:
//
//   * Push arguments onto the stack for the call in reverse order
//   * Call instruction (N00B_ZTCall, N00B_Z0Call, N00B_ZCallModule, etc.)
//   * Callee returns via N00B_ZRet or N00B_ZRetModule, as appropriate
//   * Caller pops arguments from the stack via N00B_ZPop or N00B_ZMoveSp
//   * Caller pushes the return value via pushing to R0.
//
// The N00B_ZTCall instruction used for calling builtin vtable functions differs
// from this. All arguments are still expected to be pushed onto the stack in
// reverse order; however, the call instruction itself pops the arguments and
// pushes the result onto the stack.
//
// The N00B_ZFFICall instruction used for making non-native FFI calls also
// differs similarly, though not identically. Arguments are popped from the
// stack, but the return value is stored in the return register, and so the
// caller must push the return register onto the stack if it's interested in
// its value.

typedef enum : uint8_t {
    // Push from const storage onto the stack.
    // Const values that are not reference objects can be pushed as immediates
    // and not put into const storage, so this only ever is expected to
    // contain read-only pointers to objects in the read-only heap.
    //
    // The type field should be redundant, and is not pushed.
    N00B_ZPushConstObj  = 0x01,
    // Push the *address* of the constant object. I wouldn't expect
    // this to get used until we add references.
    N00B_ZPushConstRef  = 0x02,
    // Push a copy of a local variable from its frame pointer offset to
    // the top of the stack.
    N00B_ZPushLocalObj  = 0x03,
    // Do we need this??
    N00B_ZPushLocalRef  = 0x04,
    // Push an rvalue from static space.
    N00B_ZPushStaticObj = 0x05,
    // Push an lvalue from static space.
    N00B_ZPushStaticRef = 0x06,
    // Push an immediate value onto the stack. The value to push is encoded in
    // the instruction's immediate field and may be an integer or floating point
    // value. The type of the immediate value is encoded in the instruction's
    // type_info.
    N00B_ZPushImm       = 0x07,
    // For an object on top of the stack, will retrieve and push the object's
    // type. Note that the top of the stack must not be a raw value type;
    // if it's not a by-reference type, box it first.
    N00B_ZPushObjType   = 0x09,
    // Duplicate the value at the top of the stack by pushing it again.
    N00B_ZDupTop        = 0x0A,
    // Replaces the top item on the stack with its dereference.
    N00B_ZDeref         = 0x0B,
    // Retrieves an attribute value and pushes it onto the stack. The
    // attribute is the top stack value pushed by N00B_ZPushConstObj
    // and is replaced by the attribute value. If the instruction's
    // arg is non-zero, an lvalue is pushed instead of an rvalue. Note
    // that in the case where an lvalue is pushed and subsequently
    // stored to via N00B_ZAssignToLoc, no lock checking for the
    // attribute is done, including lock on write.  If the immediate
    // field has the `1` bit set, then there is also a test for
    // whether the attribute is found... if it is, a non-zero value is
    // pushed after the result. If not, then only a zero is pushed.
    // If that non-zero field also has the `2` bit set, then the
    // actual value will not be pushed.
    N00B_ZLoadFromAttr  = 0x0C,
    N00B_ZLoadFromView  = 0x0D,
    // Create a callback and push it onto the stack. The instruction's
    // arg, immediate, and type_info fields are encoded into the
    // callback as the implementation (ffi function index), whether to
    // skip needed boxing,, and type info, respectively. The func name
    // is expected to be on the top of the stack, and get will
    // replaced with a callback object when this instruction runs. The
    // ZRunCallback instruction is used to run the callback, which is
    // run as an FFI function.
    //
    N00B_ZPushFfiPtr    = 0x0E,
    // Create a callback and push it onto the stack. The instruction's
    // arg, and type_info fields are encoded into the callback as the
    // implementation (function index), and type info,
    // respectively. The func name is expected to be on the top of the
    // stack, and get will replaced with a callback object when this
    // instruction runs. The ZRunCallback instruction is used to run
    // the callback, which is run as a native function via N00B_Z0Call,
    // but using a separate VM state.
    N00B_ZPushVmPtr     = 0x0F,
    // Stores a value to the attribute named by the top value on the stack. The
    // value to store is the stack value just below it. Both values are popped
    // from the stack. If the instruction's arg is non-zero, the attribute
    // will be locked when it's set. This instruction expects that the attribute
    // is stored on the stack via N00B_ZPushStaticPtr.
    N00B_ZAssignAttr    = 0x1D,
    // Pops the top value from the stack. This is the same as N00B_ZMoveSp with
    // an adjustment of -1.
    N00B_ZPop           = 0x20,
    // Stores the encoded immediate value into the value address encoded in the
    // instruction. The storage address is determined as described in the above
    // comment about address encodings.
    N00B_ZStoreImm      = 0x22,
    // Unpack the elements of a tuple, storing each one into the lvalue on the
    // stack, popping each lvalue as its assigned. The number of assignments to
    // perform is encoded in the instruction's arg field.
    N00B_ZUnpack        = 0x23,
    // Swap the two top values on the stack.
    N00B_ZSwap          = 0x24,
    // Perform an assignment into the lvalue at the top of the stack of the
    // value just below it and pops both items from the stack. This should be
    // paired with N00B_ZPushAddr or N00B_ZLoadFromAttr with a non-zero arg.
    N00B_ZAssignToLoc   = 0x25,
    // Jump if the top value on the stack is zero. The pc is adjusted by the
    // number of bytes encoded in the instruction's arg field, which is always
    // a multiple of the size of an instruction. A negative value jumps
    // backward. If the comparison triggers a jump, the stack is left as-is,
    // but the top value is popped if no jump occurs.
    N00B_ZJz            = 0x30,
    // Jump if the top value on the stack is not zero. The pc is adjusted by the
    // number of bytes encoded in the instruction's arg field, which is always
    // a multiple of the size of an instruction. A negative value jumps
    // backward. If the comparison triggers a jump, the stack is left as-is,
    // but the top value is popped if no jump occurs.
    N00B_ZJnz           = 0x31,
    // Unconditional jump. Adjust the pc by the number of bytes encoded in the
    // instruction's arg field, which is always a multiple of the size of an
    // instruction. A negative value jumps backward.
    N00B_ZJ             = 0x32,
    // Call one of n00b's builtin functions via vtable for the object on the
    // top of the stack. The index of the builtin function to call is encoded
    // in the instruction's arg field and should be treated as one of the values
    // in the n00b_builtin_type_fn enum type. The number of arguments expected to
    // be on the stack varies for each function. In all cases, contrary to how
    // other calls are handled, the arguments are popped from the stack and the
    // result is pushed onto the stack.
    N00B_ZTCall         = 0x33,
    // Call a "native" function, one which is defined in bytecode from the same
    // object file. The index of the function to call is encoded in the
    // instruction's arg parameter, adjusted up by 1 (0 is not a valid index).
    // The index is used to lookup the function from the object file's
    // func_info table.
    N00B_Z0Call         = 0x34,
    // Call an external "non-native" function via FFI. The index of the function
    // to call is encoded in the instruction's arg parameter. This index is not
    // adjusted as it is for other, similar instructions. The index is used to
    // lookup the function from the object file's ffi_info table. The arguments
    // are popped from the stack. The return value is stored in the return value
    // register.
    N00B_ZFFICall       = 0x35,
    // Call a module's initialization code. This corresponds with a "use"
    // statement. The module index of the module to call is encoded in the
    // instruction's arg parameter, adjusted up by 1 (0 is not a valid index).
    // The index is used to lookup the module from the object file's
    // module_contents table.
    N00B_ZCallModule    = 0x36,
    // Pops a callback from the stack (pushed via either N00B_ZPushFfiPtr or
    // N00B_ZPushVmPtr) and runs it. If the callback is an FFI callback, the
    // action is basically the same as N00B_ZFFICalll, except it uses the index
    // from the callback. Otherwise, the callback is the same as a native call
    // via N00B_Z0Call, except it uses the index from the callback.
    N00B_ZRunCallback   = 0x37,
    // Unused; will redo when adding objects.
    N00B_ZSObjNew       = 0x38,
    // Box a literal, which requires supplying the type for the object.
    N00B_ZBox           = 0x3e,
    // Unbox a value literal into its actual value.
    N00B_ZUnbox         = 0x3f,
    // Compare (and pop) two types to see if they're comptable.
    N00B_ZTypeCmp       = 0x40,
    // Compare (and pop) two values to see if they're equal. Note that
    // this is not the same as checking for object equality; this assumes
    // primitive type or reference.
    N00B_ZCmp           = 0x41,
    N00B_ZLt            = 0x42,
    N00B_ZULt           = 0x43,
    N00B_ZLte           = 0x44,
    N00B_ZULte          = 0x45,
    N00B_ZGt            = 0x46,
    N00B_ZUGt           = 0x47,
    N00B_ZGte           = 0x48,
    N00B_ZUGte          = 0x49,
    N00B_ZNeq           = 0x4A,
    // Do a GTE comparison without popping.
    N00B_ZGteNoPop      = 0x4D,
    // Do an equality comparison without popping.
    N00B_ZCmpNoPop      = 0x4E,
    // Mask out 3 bits from the top stack value; push them onto the stack.
    // Remove the bits from the pointer.
    //
    // This is meant to remove the low bits of pointers (pointer
    // stealing), so we can communicate info through them at
    // runtime. The pointer gets those bits cleared, but they are
    // pushed onto the stack. Currently, we use this to indicate how
    // much space is required per-item if we are iterating over a
    // type.
    //
    // Often we could know at compile time and generate code specific
    // to the size, but as a first pass this is easier.
    N00B_ZUnsteal       = 0x4F,
    // Begin register ops.
    N00B_ZPopToR0       = 0x50,
    // Pushes the value in R1 onto the stack.
    N00B_ZPushFromR0    = 0x51,
    // Zero out R0
    N00B_Z0R0c00l       = 0x52,
    N00B_ZPopToR1       = 0x53,
    // Pushes the value in R1 onto the stack.
    N00B_ZPushFromR1    = 0x54,
    N00B_ZPopToR2       = 0x56,
    N00B_ZPushFromR2    = 0x57,
    N00B_ZPopToR3       = 0x59,
    N00B_ZPushFromR3    = 0x5A,
    // Exits the current call frame, returning the current state back to the
    // originating location, which is the instruction immediately following the
    // N00B_Z0Call instruction that created this frame.
    N00B_ZRet           = 0x60,
    // Exits the current stack frame, returning the current state back to the
    // originating location, which is the instruction immediately following the
    // N00B_ZCallModule instruction that created this frame.
    N00B_ZModuleRet     = 0x61,
    // Halt the current program immediately.
    N00B_ZHalt          = 0x62,
    // Initialze module parameters. The number of parameters is encoded in the
    // instruction's arg field. This is only used during module initialization.
    N00B_ZModuleEnter   = 0x63,
    // Adjust the stack pointer down by the amount encoded in the instruction's
    // arg field. This means specifically that the arg field is subtracted from
    // sp, so a single pop would encode -1 as the adjustment.
    N00B_ZMoveSp        = 0x65,
    // Test the top stack value. If it is non-zero, pop it and continue running
    // the program. Otherwise, print an assertion failure and stop running the
    // program.
    // Math operations have signed and unsigned variants. We can go from
    // signed to unsigned where it makes sense by adding 0x10.
    // Currently, we do not do this for bit ops, they are just all unsigned.
    N00B_ZAdd           = 0x70,
    N00B_ZSub           = 0x71,
    N00B_ZMul           = 0x72,
    N00B_ZDiv           = 0x73,
    N00B_ZMod           = 0x74,
    N00B_ZBXOr          = 0x75,
    N00B_ZShl           = 0x76,
    N00B_ZShr           = 0x77,
    N00B_ZBOr           = 0x78,
    N00B_ZBAnd          = 0x79,
    N00B_ZBNot          = 0x7A,
    N00B_ZUAdd          = 0x80, // Same as ZAdd
    N00B_ZUSub          = 0x81, // Same as ZSub
    N00B_ZUMul          = 0x82,
    N00B_ZUDiv          = 0x83,
    N00B_ZUMod          = 0x84,
    N00B_ZFAdd          = 0x90,
    N00B_ZFSub          = 0x91,
    N00B_ZFMul          = 0x92,
    N00B_ZFDiv          = 0x93,
    N00B_ZAssert        = 0xA0,
    // Set the specified attribute to be "lock on write". Triggers an error if
    // the attribute is already set to lock on write. This instruction expects
    // the top stack value to be loaded via ZPushStaticPtr and does not pop it.
    // The attribute to lock is named according to the top stack value.
    N00B_ZLockOnWrite   = 0xB0,
    // Given a static offset as an argument, locks the mutex passed in
    // the argument.
    N00B_ZLockMutex     = 0xB1,
    N00B_ZUnlockMutex   = 0xB2,
    // Arithmetic and bitwise operators on 64-bit values; the two-arg
    // ones conceptually pop the right operand, then the left operand,
    // perform the operation, then push. But generally after the RHS
    // pop, the left operand will get replaced without additional
    // movement.
    //
    // Perform a logical not operation on the top stack value. If the value is
    // zero, it will be replaced with a one value of the same type. If the value
    // is non-zero, it will be replaced with a zero value of the same type.
    N00B_ZNot           = 0xE0,
    N00B_ZAbs           = 0xE1,
    // The rest of these mathy operators are less general purpose, and are
    // just used to make our lives easier when generating code, so
    // there's currently a lack of symmetry here.
    //
    // This version explicitly takes an immediate on the RHS so we
    // don't have to test for it.
    N00B_ZShlI          = 0xF0,
    N00B_ZSubNoPop      = 0xF1,
    // This one mostly computes abs(x) / x.
    // The only difference is that it's well defined for 0, it returns
    // 1 (0 is not a negative number).
    // This is mostly used to generate the step for ranged loops; you
    // can do for x in 10 to 0 to get 10 down to 1 (inclusive).
    N00B_ZGetSign       = 0xF2,
    // Print the error message that is the top value on the stack and stop
    // running the program.
    N00B_ZBail          = 0xFE,
    // Nop does nothing.
    N00B_ZNop           = 0xFF,
#ifdef N00B_DEV
    N00B_ZPrint = 0xFD,
    N00B_ZDebug = 0xFC,
#endif
} n00b_zop_t;

typedef struct n00b_static_memory {
    n00b_mem_ptr *items;
    uint32_t     num_items;
    uint32_t     alloc_len;
} n00b_static_memory;

// We'll make the main VM container, n00b_vm_t an object type (N00B_T_VM) so that
// we can have an entry point into the core marshalling functionality, but we
// won't make other internal types object types. Only those structs that have a
// z prefix on their name get marshalled. For lists we'll use n00b_type_list
// with a n00b_type_ref base type. But this means that we have to marshal these
// lists manually. As of right now at least, all dicts used have object keys
// and values (ints, strings, etc).
typedef struct {
    n00b_zop_t   op;
    uint8_t     pad;
    int32_t     module_id;
    int32_t     line_no;
    int32_t     arg;
    int64_t     immediate;
    n00b_type_t *type_info;
} n00b_zinstruction_t;

typedef struct {
    n00b_utf8_t *name;
    n00b_type_t *tid;
    // Nim casts this around as a pointer, but it's always used as an integer
    // index into an array.
    int64_t     impl;
    int32_t     mid;
    bool        ffi;
    bool        skip_boxes;
} n00b_zcallback_t;

// stack values have no indicator of what's actually stored, instead relying on
// instructions to assume correctly what's present.
typedef union n00b_value_t {
    void              *vptr;
    n00b_zcallback_t   *callback;
    n00b_obj_t         *lvalue;
    char              *cptr;
    union n00b_value_t *fp;         // saved fp
    n00b_obj_t          rvalue;
    uint64_t           static_ptr; // offset into static_data
    // saved pc / module_id, along with unsigned int values where
    // we don't care about the type field for the operation.
    uint64_t           uint;
    int64_t            sint; // signed int values.
    n00b_box_t          box;
    double             dbl;
    bool               boolean;
} n00b_value_t;

// Might want to trim a bit out of it, but for right now, an going to not.
typedef struct n00b_ffi_decl_t n00b_zffi_info_t;

typedef struct {
    n00b_type_t *tid;
    int64_t     offset;
} n00b_zsymbol_t;

typedef struct {
    n00b_str_t  *funcname;
    n00b_dict_t *syms; // int64_t, string

    // sym_types maps offset to type ids at compile time. Particularly
    // for debugging info, but will be useful if we ever want to, from
    // the object file, create optimized instances of functions based
    // on the calling type, etc. Parameters are held separately from
    // stack symbols, and like w/ syms, we only capture variables
    // scoped to the entire function. We'll probably address that
    // later.
    //
    // At run-time, the type will always need to be concrete.
    n00b_list_t *sym_types; // tspec_ref: n00b_zsymbol_t
    n00b_type_t *tid;
    n00b_str_t  *shortdoc;
    n00b_str_t  *longdoc;
    int32_t     mid;    // module_id
    int32_t     offset; // offset to start of instructions in module
    int32_t     size;   // Stack frame size.
    int32_t     static_lock;
} n00b_zfn_info_t;

enum {
    // The 'ORIG' fields all map to the state we want if we do a 'reset'.
    // The rest are the current saved state due to incremental compilation.
    N00B_CCACHE_ORIG_SCONSTS,
    N00B_CCACHE_ORIG_OCONSTS,
    N00B_CCACHE_ORIG_VCONSTS,
    N00B_CCACHE_ORIG_STATIC,
    N00B_CCACHE_ORIG_ASCOPE,
    N00B_CCACHE_ORIG_GSCOPE,
    N00B_CCACHE_ORIG_MODULES,
    N00B_CCACHE_ORIG_SPEC,
    N00B_CCACHE_ORIG_SORT,
    N00B_CCACHE_ORIG_ATTR,
    N00B_CCACHE_ORIG_SECTIONS,
    N00B_CCACHE_ORIG_TYPES,
    N00B_CCACHE_CUR_SCONSTS,
    N00B_CCACHE_CUR_OCONSTS,
    N00B_CCACHE_CUR_VCONSTS,
    N00B_CCACHE_CUR_ASCOPE,
    N00B_CCACHE_CUR_GSCOPE,
    N00B_CCACHE_CUR_MODULES,
    N00B_CCACHE_CUR_TYPES,
    N00B_CCACHE_LEN,
};

typedef union {
    struct {
        uint8_t preview;
        uint8_t patch;
        uint8_t minor;
        uint8_t major;
    } dotted;
    uint64_t number;
} n00b_version_t;

typedef struct {
    void              *ccache[N00B_CCACHE_LEN];
    struct n00b_spec_t *attr_spec;
    n00b_static_memory *static_contents;
    n00b_list_t        *module_contents; // tspec_ref: n00b_zmodule_info_t
    n00b_list_t        *func_info;       // tspec_ref: n00b_zfn_info_t
    n00b_list_t        *ffi_info;        // tspec_ref: n00b_zffi_info_t
    int                ffi_info_entries;
    // CCACHE == 'compilation cache', which means stuff we use to
    // re-start incremental compilation; we *could* rebuild this stuff
    // when needed, but these things shouldn't be too large.
    //
    // The items are in the enum above.
    //

    n00b_version_t n00b_version;
    int32_t       first_entry;    // Initial entry point.
    int32_t       default_entry;  // The module to whitch we reset.
    bool          using_attrs;    // Should move into object.
    bool          root_populated; // This too.
} n00b_zobject_file_t;

typedef struct {
    struct n00b_module_t *call_module;
    struct n00b_module_t *targetmodule;
    n00b_zfn_info_t      *targetfunc;
    int32_t              calllineno;
    int32_t              targetline;
    uint32_t             pc;
} n00b_vmframe_t;

typedef struct {
    n00b_zinstruction_t *lastset;
    void               *contents;
    n00b_type_t         *type; // Value types will not generally be boxed.
    bool                is_set;
    bool                locked;
    bool                lock_on_write;
    int32_t             module_lock;
    bool                override;
} n00b_attr_contents_t;

typedef struct {
    // The stuff in this struct isn't saved out; it needs to be
    // reinitialized on each startup.
#ifdef N00B_DEV
    n00b_buf_t    *print_buf;
    n00b_stream_t *print_stream;
#endif
} n00b_zrun_state_t;

// N00B_T_VM
typedef struct {
    n00b_zobject_file_t *obj;
    n00b_dict_t         *attrs; // string, n00b_attr_contents_t (tspec_ref)
    n00b_zrun_state_t   *run_state;
    n00b_obj_t         **module_allocations;
    n00b_set_t          *all_sections; // string
    n00b_duration_t      creation_time;
    n00b_duration_t      first_saved_run_time;
    n00b_duration_t      last_saved_run_time;
    uint32_t            num_saved_runs;
    int32_t             entry_point;
} n00b_vm_t;

typedef struct {
    // vm is a pointer to the global vm state shared by various threads.
    n00b_vm_t *vm;

    // sp is the current stack pointer. The stack grows down, so sp starts out
    // as &stack[STACK_SIZE] (stack bottom)
    n00b_value_t *sp;

    // fp points to the start of the current frame on the stack.
    n00b_value_t *fp;

    // current_module is the module to which currently executing instructions
    // belong.
    struct n00b_module_t *current_module;

    // The arena this allocation is from.
    n00b_arena_t *thread_arena;

    // frame_stack is the base address of the call stack
    n00b_vmframe_t frame_stack[N00B_MAX_CALL_DEPTH];

    // stack is the base address of the stack for this thread of execution.
    // the stack grows down, so the stack bottom is &stack[STACK_SIZE]
    n00b_value_t stack[N00B_STACK_SIZE];

    // General purpose registers.
    // R0 should generally only be used for passing return values.
    n00b_obj_t r0;
    n00b_obj_t r1;
    n00b_obj_t r2;
    n00b_obj_t r3;

    // pc is the current program counter, which is an index into current_module
    // instructions array.
    uint32_t pc;

    // num_frames is the number of active frames in the call stack. the current
    // frame is always frame_stack[num_frames - 1]
    int32_t num_frames;

    // running is true if this thread state is currently running VM code.
    bool running;

    // error is true if this thread state raised an error during evaluation.
    bool error;

} n00b_vmthread_t;

#define N00B_F_ATTR_PUSH_FOUND 1
#define N00B_F_ATTR_SKIP_LOAD  2
