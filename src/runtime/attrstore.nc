#include "n00b.h"

static void
populate_one_section(n00b_vmthread_t     *tstate,
                     n00b_spec_section_t *section,
                     n00b_string_t       *path)
{
    uint64_t           n;
    n00b_string_t     *key;
    n00b_spec_field_t *f;
    n00b_list_t       *fields;

    n00b_vm_t *vm = tstate->vm;

    if (!n00b_set_add(vm->all_sections, path)) {
        return;
    }

    if (!section) {
        return;
    }

    fields = n00b_dict_values(section->fields);
    n      = n00b_list_len(fields);

    for (uint64_t i = 0; i < n; i++) {
        f = n00b_list_get(fields, i, NULL);

        if (!path || !n00b_string_byte_len(path)) {
            key = f->name;
        }
        else {
            key = n00b_cformat("«#».«#»", path, f->name);
        }

        if (f->default_provided) {
            n00b_vm_attr_set(tstate,
                             key,
                             f->default_value,
                             f->tinfo.type,
                             f->lock_on_write,
                             false,
                             true);
        }
    }
}

static void
populate_defaults(n00b_vmthread_t *tstate, n00b_string_t *key)
{
    n00b_vm_t *vm = tstate->vm;

    if (!vm->obj->attr_spec || !vm->obj->attr_spec->in_use) {
        return;
    }

    int   ix = -1;
    char *p  = key->data + n00b_string_byte_len(key);

    while (--p > key->data) {
        if (*p == '.') {
            ix = p - key->data;
            break;
        }
    }

    if (!vm->obj->root_populated) {
        populate_one_section(tstate,
                             vm->obj->attr_spec->root_section,
                             n00b_cached_empty_string());
        vm->obj->root_populated = true;
    }
    if (ix == -1) {
        return;
    }

    // The slice will also always ensure a private copy (right now).
    key = n00b_string_slice(key, 0, ix);

    if (n00b_set_contains(vm->all_sections, key)) {
        return;
    }

    p = key->data;

    int                  start_codepoints = 0;
    int                  n                = 0;
    char                *last_start       = p;
    char                *e                = p + n00b_string_byte_len(key);
    n00b_string_t       *dummy            = n00b_cached_space();
    n00b_codepoint_t     cp;
    n00b_spec_section_t *section;

    key->u8_bytes   = 0;
    key->codepoints = 0;

    while (p < e) {
        n = utf8proc_iterate((const uint8_t *)p, 4, &cp);
        if (n < 0) {
            n00b_unreachable();
        }
        if (cp != '.') {
            key->codepoints += 1;
            p += n;
            continue;
        }

        // For the moment, turn the dot into a null terminator.
        *p = 0;

        // Set up the string we'll use to look up the section:
        dummy->data       = last_start;
        dummy->u8_bytes   = p - last_start;
        dummy->codepoints = key->codepoints - start_codepoints;
        key->u8_bytes     = p - key->data;

        // Can be null if it's an object; no worries.
        section = n00b_dict_get(vm->obj->attr_spec->section_specs,
                                dummy,
                                NULL);

        populate_one_section(tstate, section, key);

        // Restore our fragile state.
        *p         = '.';
        last_start = ++p;
        key->codepoints += 1;
        start_codepoints = key->codepoints;
    }

    dummy->data       = last_start;
    dummy->u8_bytes   = p - last_start;
    dummy->codepoints = key->codepoints - start_codepoints;
    key->u8_bytes     = p - key->data;

    section = n00b_dict_get(vm->obj->attr_spec->section_specs,
                            dummy,
                            NULL);

    populate_one_section(tstate, section, key);
}

void *
n00b_vm_attr_get(n00b_vmthread_t *tstate,
                 n00b_string_t   *key,
                 bool            *found)
{
    populate_defaults(tstate, key);

    n00b_attr_contents_t *info = n00b_dict_get(tstate->vm->attrs, key, NULL);

    if (found != NULL) {
        if (info != NULL && info->is_set) {
            *found = true;
            return info->contents;
        }
        *found = false;
        return NULL;
    }

    if (info == NULL || !info->is_set) {
        // Nim version uses N00bError stuff that doesn't exist in
        // n00b (yet?)
        n00b_string_t *errstr = n00b_cstring("attribute does not exist: ");
        n00b_string_t *msg    = n00b_string_concat(errstr, key);
        N00B_RAISE(msg);
    }

    return info->contents;
}

void
n00b_vm_attr_set(n00b_vmthread_t *tstate,
                 n00b_string_t   *key,
                 void            *value,
                 n00b_ntype_t     type,
                 bool             lock,
                 bool             override,
                 bool             internal)
{
    n00b_vm_t *vm        = tstate->vm;
    vm->obj->using_attrs = true;

    if (!internal) {
        populate_defaults(tstate, key);
    }

    // We will create a new entry on every write, just to avoid any race
    // conditions with multiple threads updating via reference.

    bool                  found;
    n00b_attr_contents_t *old_info = n00b_dict_get(vm->attrs, key, &found);
    if (found) {
        // Nim code does this after allocating new_info and never settings it's
        // override field here, so that's clearly wrong. We do it first to avoid
        // wasting the allocation and swap to use old_info->override instead,
        // which makes more sense.
        if (old_info->override && !override) {
            return; // Pretend it was successful
        }
    }

    n00b_attr_contents_t *new_info
        = n00b_gc_raw_alloc(sizeof(n00b_attr_contents_t), N00B_GC_SCAN_ALL);
    *new_info = (n00b_attr_contents_t){
        .contents = value,
        .is_set   = true,
    };

    if (found) {
        bool locked = (old_info->locked
                       || (old_info->module_lock != 0
                           && (uint32_t)old_info->module_lock
                                  != tstate->current_module->module_id));
        if (locked) {
            if (!override && old_info->is_set) {
                if (!n00b_eq(type, value, old_info->contents)) {
                    // Nim version uses N00bError stuff that doesn't exist in
                    // n00b (yet?)
                    n00b_string_t *errstr = n00b_cstring("attribute is locked: ");
                    n00b_string_t *msg    = n00b_string_concat(errstr, key);
                    N00B_RAISE(msg);
                }
                // Set to same value; ignore it basically
                return;
            }
        }

        new_info->module_lock = old_info->module_lock;
        new_info->locked      = old_info->lock_on_write;
    }

    new_info->override = override;

    if (tstate->running) {
        new_info->lastset = n00b_list_get(tstate->current_module->instructions,
                                          tstate->pc - 1,
                                          NULL);
        /*
        if (n00b_list_len(tstate->module_lock_stack) > 0) {
            new_info->module_lock
                = (int32_t)(int64_t)n00b_list_get(tstate->module_lock_stack,
                                                 -1,
                                                 NULL);
        }
        */
    }

    // Don't trigger write lock if we're setting a default (i.e.,
    // internal is set).
    if (lock && !internal) {
        new_info->locked = true;
    }

    n00b_dict_put(vm->attrs, key, new_info);
}

void
n00b_vm_attr_lock(n00b_vmthread_t *tstate, n00b_string_t *key, bool on_write)
{
    n00b_vm_t *vm = tstate->vm;

    // We will create a new entry on every write, just to avoid any race
    // conditions with multiple threads updating via reference.

    bool                  found;
    n00b_attr_contents_t *old_info = n00b_dict_get(vm->attrs, key, &found);
    if (found && old_info->locked) {
        // Nim version uses N00bError stuff that doesn't exist in
        // n00b (yet?)
        n00b_string_t *errstr = n00b_cstring("attribute already locked: ");
        n00b_string_t *msg    = n00b_string_concat(errstr, key);
        N00B_RAISE(msg);
    }

    n00b_attr_contents_t *new_info
        = n00b_gc_raw_alloc(sizeof(n00b_attr_contents_t), N00B_GC_SCAN_ALL);
    *new_info = (n00b_attr_contents_t){
        .lock_on_write = true,
    };

    if (found) {
        new_info->contents = old_info->contents;
        new_info->is_set   = old_info->is_set;
    }

    n00b_dict_put(vm->attrs, key, new_info);
}
