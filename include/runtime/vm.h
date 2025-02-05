#pragma once

#include "n00b.h"

// reset a vm's volatile state. this should normally only be done before the
// first run of a vm, but it may be done successively as well, as long as there
// are no vm thread states active for the vm.
extern void
n00b_vm_reset(n00b_vm_t *vm);

// Loads any constants and other state expected to be present
// before running for the first time.
extern void
n00b_vm_setup_first_runtime(n00b_vm_t *vm);

// create a new thread state attached to the specified vm. Multiple threads may
// run code from the same VM simultaneously, but each one needs its own thread
// state.
extern n00b_vmthread_t *
n00b_vmthread_new(n00b_vm_t *vm);

// set the specified thread state running. evaluation of instructions at the
// location previously set into the tstate will proceed.
extern int
n00b_vmthread_run(n00b_vmthread_t *tstate);

// reset the specified thread state, leaving the state as if it was newly
// return from n00b_vmthread_new.
extern void
n00b_vmthread_reset(n00b_vmthread_t *tstate);

// retrieve the attribute specified by key. if the `found param`is not
// provided, throw an exception when the attribute is not found.
extern void *
n00b_vm_attr_get(n00b_vmthread_t *tstate,
                 n00b_string_t      *key,
                 bool            *found);

extern void
n00b_vm_attr_set(n00b_vmthread_t *tstate,
                 n00b_string_t      *key,
                 void            *value,
                 n00b_type_t     *type,
                 bool             lock,
                 bool             override,
                 bool             internal);

// lock an attribute immediately of on_write is false; otherwise, lock it when
// it is set.
extern void
n00b_vm_attr_lock(n00b_vmthread_t *tstate, n00b_string_t *key, bool on_write);

extern void
n00b_vm_marshal(n00b_vm_t    *vm,
                n00b_stream_t *out,
                n00b_dict_t  *memos,
                int64_t      *mid);

extern void
n00b_vm_unmarshal(n00b_vm_t *vm, n00b_stream_t *in, n00b_dict_t *memos);

extern n00b_list_t *
n00b_validate_runtime(n00b_vm_t *vm);

extern n00b_buf_t *n00b_automarshal(void *);
extern void       *n00b_autounmarshal(n00b_buf_t *);
extern n00b_buf_t *n00b_vm_save(n00b_vm_t *vm);

static inline n00b_vm_t *
n00b_vm_restore(n00b_buf_t *b)
{
    return n00b_autounmarshal(b);
}
