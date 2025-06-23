#pragma once
#include "n00b.h"

typedef struct {
    n00b_string_t *name;
    n00b_ntype_t   tid;
    // Nim casts this around as a pointer, but it's always used as an integer
    // index into an array.
    int64_t        impl;
    int32_t        mid;
    bool           ffi;
    bool           skip_boxes;
} n00b_zcallback_t;

// stack values have no indicator of what's actually stored, instead relying on
// instructions to assume correctly what's present.
typedef union n00b_value_t {
    void               *vptr;
    n00b_zcallback_t   *callback;
    void              **lvalue;
    char               *cptr;
    union n00b_value_t *fp;         // saved fp
    void               *rvalue;
    uint64_t            static_ptr; // offset into static_data
    // saved pc / module_id, along with unsigned int values where
    // we don't care about the type field for the operation.
    uint64_t            uint;
    int64_t             sint; // signed int values.
    n00b_box_t          box;
    double              dbl;
    bool                boolean;
} n00b_value_t;

// Might want to trim a bit out of it, but for right now, an going to not.
typedef struct n00b_ffi_decl_t n00b_zffi_info_t;

typedef struct {
    n00b_ntype_t tid;
    int64_t      offset;
} n00b_zsymbol_t;

typedef struct {
    n00b_string_t *funcname;
    n00b_dict_t   *syms; // int64_t, string

    // sym_types maps offset to type ids at compile time. Particularly
    // for debugging info, but will be useful if we ever want to, from
    // the object file, create optimized instances of functions based
    // on the calling type, etc. Parameters are held separately from
    // stack symbols, and like w/ syms, we only capture variables
    // scoped to the entire function. We'll probably address that
    // later.
    //
    // At run-time, the type will always need to be concrete.
    n00b_list_t   *sym_types; // tspec_ref: n00b_zsymbol_t
    n00b_ntype_t   tid;
    n00b_string_t *shortdoc;
    n00b_string_t *longdoc;
    int32_t        mid;    // module_id
    int32_t        offset; // offset to start of instructions in module
    int32_t        size;   // Stack frame size.
    int32_t        static_lock;
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
    void               *ccache[N00B_CCACHE_LEN];
    struct n00b_spec_t *attr_spec;
    n00b_static_memory *static_contents;
    n00b_list_t        *module_contents; // tspec_ref: n00b_zmodule_info_t
    n00b_list_t        *func_info;       // tspec_ref: n00b_zfn_info_t
    n00b_list_t        *ffi_info;        // tspec_ref: n00b_zffi_info_t
    int                 ffi_info_entries;
    // CCACHE == 'compilation cache', which means stuff we use to
    // re-start incremental compilation; we *could* rebuild this stuff
    // when needed, but these things shouldn't be too large.
    //
    // The items are in the enum above.
    //

    n00b_version_t n00b_version;
    int32_t        first_entry;    // Initial entry point.
    int32_t        default_entry;  // The module to whitch we reset.
    bool           using_attrs;    // Should move into object.
    bool           root_populated; // This too.
} n00b_zobject_file_t;

typedef struct {
    struct n00b_module_t *call_module;
    struct n00b_module_t *targetmodule;
    n00b_zfn_info_t      *targetfunc;
    int32_t               calllineno;
    int32_t               targetline;
    uint32_t              pc;
} n00b_vmframe_t;

typedef struct {
    n00b_zinstruction_t *lastset;
    void                *contents;
    n00b_ntype_t         type; // Value types will not generally be boxed.
    bool                 is_set;
    bool                 locked;
    bool                 lock_on_write;
    int32_t              module_lock;
    bool                 override;
} n00b_attr_contents_t;

typedef struct {
    // The stuff in this struct isn't saved out; it needs to be
    // reinitialized on each startup.
#ifdef N00B_DEV
    n00b_buf_t           *print_buf;
    struct n00b_stream_t *print_stream;
#endif
} n00b_zrun_state_t;

// N00B_T_VM
typedef struct {
    n00b_zobject_file_t *obj;
    n00b_dict_t         *attrs; // string, n00b_attr_contents_t (tspec_ref)
    n00b_zrun_state_t   *run_state;
    void              ***module_allocations;
    n00b_set_t          *all_sections; // string
    n00b_duration_t      creation_time;
    n00b_duration_t      first_saved_run_time;
    n00b_duration_t      last_saved_run_time;
    uint32_t             num_saved_runs;
    int32_t              entry_point;
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
    void *r0;
    void *r1;
    void *r2;
    void *r3;

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
