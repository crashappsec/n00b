// Validation of the n00b runtime state. By default, this will run
// any time there is a loaded confspec, and execution is in the
// process of halting, whether due to VM termination, or due to a
// callback running.
//
// This assumes that the attribute state will not change during
// the validation.

#include "n00b.h"

typedef enum : uint8_t {
    sk_singleton = 0,
    sk_blueprint = 1,
    sk_instance  = 2,
} section_kind;

typedef struct {
    n00b_dict_t         *contained_sections;
    n00b_dict_t         *contained_fields;
    n00b_spec_section_t *section_spec;
    section_kind         kind;
    bool                 instantiation;
} section_vinfo;

typedef struct {
    n00b_attr_contents_t *record;
    n00b_spec_field_t    *field_spec;
} field_vinfo;

typedef struct {
    union {
        section_vinfo section;
        field_vinfo   field;
    } info;
    n00b_string_t *path;
    bool           field;
    bool           checked;
} spec_node_t;

typedef struct {
    n00b_dict_t *attrs;
    n00b_dict_t *section_cache;
    n00b_spec_t *spec;
    n00b_list_t *errors;
    spec_node_t *section_tree;
    n00b_vm_t   *vm;
    void        *cur;
    bool         at_object_name;
} validation_ctx;

static n00b_string_t *
current_path(validation_ctx *ctx)
{
    spec_node_t *s = ctx->cur;

    if (!s->path || !strlen(s->path->data)) {
        return n00b_cstring("the configuration root");
    }

    return n00b_cformat("section «#»", s->path);
}

static n00b_string_t *
loc_from_decl(spec_node_t *n)
{
    if (n->field) {
        return n->info.field.field_spec->location_string;
    }
    else {
        return n->info.section.section_spec->location_string;
    }
}

static n00b_string_t *
loc_from_attr(validation_ctx *ctx, n00b_attr_contents_t *attr)
{
    n00b_zinstruction_t *ins = attr->lastset;
    n00b_module_t       *mi;

    if (!ins) {
        n00b_assert(attr->is_set);
        return n00b_cstring("Pre-execution");
    }

    mi = n00b_list_get(ctx->vm->obj->module_contents, ins->module_id, NULL);

    if (mi->full_uri) {
        return n00b_cformat("«b»«#»:«#:i»:", mi->full_uri, ins->line_no);
    }

    return n00b_cformat("«b»«#».«#»:«#:i»:",
                        mi->package,
                        mi->name,
                        ins->line_no);
}

static n00b_string_t *
best_field_loc(validation_ctx *ctx, spec_node_t *n)
{
    if (n->info.field.record->is_set) {
        return loc_from_attr(ctx, n->info.field.record);
    }
    return loc_from_decl(n);
}

static n00b_string_t *
spec_info_if_used(validation_ctx *ctx, spec_node_t *n)
{
    if (n->info.field.record->is_set) {
        return n00b_cformat("(field «#» defined at: «#»",
                            n->info.field.field_spec->name,
                            loc_from_decl(n));
    }

    return n00b_cached_empty_string();
}

static void
_n00b_validation_error(validation_ctx      *ctx,
                       n00b_compile_error_t code,
                       n00b_string_t       *loc,
                       ...)
{
    va_list args;

    va_start(args, loc);

    n00b_base_runtime_error(ctx->errors,
                            code,
                            loc,
                            args);

    va_end(args);
}

#define n00b_validation_error(ctx, code, ...) \
    _n00b_validation_error(ctx, code, N00B_VA(__VA_ARGS__))

static spec_node_t *
spec_node_alloc(validation_ctx *ctx, n00b_string_t *path)
{
    spec_node_t   *res  = n00b_gc_alloc_mapped(spec_node_t, N00B_GC_SCAN_ALL);
    section_vinfo *info = &res->info.section;

    info->contained_sections = n00b_dict(n00b_type_string(), n00b_type_ref());
    info->contained_fields   = n00b_dict(n00b_type_string(), n00b_type_ref());
    res->path                = path;

    n00b_dict_put(ctx->section_cache, path, res);

    return res;
}

static spec_node_t *
init_section_node(validation_ctx *ctx,
                  n00b_string_t  *path,
                  n00b_string_t  *section,
                  n00b_list_t    *subitems)
{
    // This sets up data structures from the section cache, but does not
    // populate any field information whatsoever.

    spec_node_t   *sec_info;
    n00b_string_t *full_path;
    bool           alloced = false;

    if (path == NULL) {
        full_path = section;
    }
    else {
        full_path = n00b_path_simple_join(path, section);
    }

    sec_info = n00b_dict_get(ctx->section_cache, full_path, NULL);

    if (sec_info == NULL) {
        alloced                             = true;
        sec_info                            = spec_node_alloc(ctx, full_path);
        sec_info->info.section.section_spec = (n00b_spec_section_t *)ctx->cur;

        if (ctx->at_object_name) {
            sec_info->info.section.instantiation = true;
            sec_info->info.section.kind          = sk_instance;
        }
        else {
            if (sec_info->info.section.section_spec->singleton) {
                sec_info->info.section.kind = sk_singleton;
            }
            else {
                sec_info->info.section.kind = sk_blueprint;
            }
        }
    }

    if (!subitems) {
        return alloced ? sec_info : NULL;
    }

    int            n            = n00b_list_len(subitems);
    n00b_string_t *next_section = n00b_list_get(subitems, 0, NULL);
    n00b_list_t   *next_items   = NULL;

    if (n > 1) {
        next_items = n00b_list_get_slice(subitems, 1, n);
    }

    // Set up the right section for the child.
    if (ctx->cur != NULL) {
        n00b_spec_section_t *cur_sec = (n00b_spec_section_t *)ctx->cur;

        if (cur_sec->singleton || ctx->at_object_name) {
            ctx->at_object_name = false;
            ctx->cur            = n00b_dict_get(ctx->spec->section_specs,
                                     next_section,
                                     NULL);
        }
        else {
            ctx->at_object_name = true;
        }
    }

    spec_node_t *sub = init_section_node(ctx,
                                         full_path,
                                         next_section,
                                         next_items);

    if (sub != NULL) {
        n00b_dict_add(sec_info->info.section.contained_sections,
                      next_section,
                      sub);
    }

    return alloced ? sec_info : NULL;
}

static inline void
init_one_section(validation_ctx *ctx, n00b_string_t *path)
{
    if (!path) {
        ctx->section_tree = init_section_node(ctx,
                                              NULL,
                                              n00b_cached_empty_string(),
                                              NULL);
        return;
    }

    n00b_list_t   *parts = n00b_string_split(path, n00b_cached_period());
    n00b_string_t *next  = n00b_list_get(parts, 0, NULL);

    parts = n00b_list_get_slice(parts, 1, n00b_list_len(parts));

    if (!n00b_list_len(parts)) {
        parts = NULL;
    }

    ctx->cur = ctx->spec->root_section;

    init_section_node(ctx, NULL, next, parts);
}

// False if there's any sort of error. If the 'error' is no spec is installed,
// then the error list will be uninitialized.
static bool
spec_init_validation(validation_ctx *ctx, n00b_vm_t *runtime)
{
    if (!runtime->obj->using_attrs || !runtime->obj->attr_spec) {
        return false;
    }

    ctx->attrs         = runtime->attrs;
    ctx->spec          = runtime->obj->attr_spec;
    ctx->section_cache = n00b_dict(n00b_type_string(), n00b_type_ref());
    ctx->cur           = ctx->spec->root_section;
    ctx->errors        = n00b_list(n00b_type_ref());

    init_one_section(ctx, NULL);

    n00b_list_t *sections = n00b_set_items(runtime->all_sections);
    int          n        = n00b_list_len(sections);

    for (int i = 0; i < n; i++) {
        init_one_section(ctx, n00b_list_get(sections, i, NULL));
    }

    // At this point, the sections that have been used in the program
    // are loaded, but the fields used in the program are not, and
    // we haven't run any of our checks.

    return true;
}

static inline void
mark_required_field(n00b_flags_t *flags, n00b_list_t *req, n00b_string_t *name)
{
    for (int i = 0; i < n00b_list_len(req); i++) {
        n00b_spec_field_t *fspec = n00b_list_get(req, i, NULL);
        if (!strcmp(fspec->name->data, name->data)) {
            n00b_flags_set_index(flags, i, true);
            return;
        }
    }
}

static inline void
validate_field_contents(validation_ctx      *ctx,
                        spec_node_t         *node,
                        n00b_spec_section_t *secspec)
{
    uint64_t     num_req;
    n00b_dict_t *fdict      = node->info.section.contained_fields;
    n00b_list_t *fields     = n00b_dict_items(fdict);
    uint64_t     num_fields = n00b_list_len(fields);
    n00b_list_t *fspecs     = (void *)n00b_dict_values(secspec->fields);
    uint64_t     num_specs  = n00b_list_len(fspecs);

    n00b_flags_t *reqflags = NULL;
    n00b_list_t  *required = n00b_list(n00b_type_ref());

    // Scan the field specs to see how many are required.
    for (unsigned int i = 0; i < num_specs; i++) {
        n00b_spec_field_t *fs = n00b_list_get(fspecs, i, NULL);
        if (fs->required) {
            n00b_list_append(required, fs);
        }
    }

    num_req = n00b_list_len(required);

    if (num_req != 0) {
        reqflags = n00b_new(n00b_type_flags(), length : num_req);
    }

    for (unsigned int i = 0; i < num_fields; i++) {
        n00b_tuple_t         *tup    = n00b_list_get(fields, i, NULL);
        n00b_string_t        *name   = n00b_tuple_get(tup, 0);
        spec_node_t          *fnode  = n00b_tuple_get(tup, 1);
        n00b_spec_field_t    *fspec  = fnode->info.field.field_spec;
        n00b_attr_contents_t *record = fnode->info.field.record;
        n00b_ntype_t          t;

        if (!record->is_set) {
            continue;
        }

        if (!fspec) {
            if (secspec->user_def_ok) {
                continue;
            }
            n00b_validation_error(ctx,
                                  n00b_spec_disallowed_field,
                                  loc_from_attr(ctx, record),
                                  current_path(ctx),
                                  name);
            continue;
        }

        if (fspec->required) {
            mark_required_field(reqflags, required, fspec->name);
        }

        if (fnode->checked) {
            continue;
        }

        n00b_list_t *exclusions = n00b_set_items(fspec->exclusions);
        int32_t      num_ex     = n00b_list_len(exclusions);

        for (int i = 0; i < num_ex; i++) {
            n00b_string_t *one_ex = n00b_list_get(exclusions, i, 0);
            spec_node_t   *n      = n00b_dict_get(fdict, one_ex, NULL);

            if (n != NULL) {
                n00b_validation_error(ctx,
                                      n00b_spec_mutex_field,
                                      loc_from_attr(ctx, n->info.field.record),
                                      current_path(ctx),
                                      one_ex,
                                      node->info.field.field_spec->name,
                                      loc_from_attr(ctx,
                                                    node->info.field.record));
            }

            n00b_spec_field_t *x = n00b_dict_get(secspec->fields, one_ex, NULL);
            if (x && x->required) {
                mark_required_field(reqflags, required, x->name);
            }
        }

        if (fspec->deferred_type_field) {
            spec_node_t *bud = n00b_dict_get(fdict,
                                             fspec->deferred_type_field,
                                             NULL);
            if (!bud || !bud->info.field.record->is_set) {
                n00b_validation_error(ctx,
                                      n00b_spec_missing_ptr,
                                      best_field_loc(ctx, node),
                                      current_path(ctx),
                                      node->info.field.field_spec->name,
                                      fspec->deferred_type_field,
                                      spec_info_if_used(ctx, node));
            }
            else {
                n00b_attr_contents_t *bud_rec = bud->info.field.record;

                t = n00b_unify(bud_rec->type, n00b_type_typespec(), NULL);
                if (n00b_type_is_error(t)) {
                    n00b_validation_error(ctx,
                                          n00b_spec_invalid_type_ptr,
                                          best_field_loc(ctx, node),
                                          current_path(ctx),
                                          node->info.field.field_spec->name,
                                          fspec->deferred_type_field,
                                          bud_rec->type,
                                          loc_from_attr(ctx, bud_rec));
                }
                else {
                    t = n00b_type_copy((n00b_ntype_t)bud_rec->contents);
                    t = n00b_unify(t, record->type, NULL);
                    if (n00b_type_is_error(t)) {
                        n00b_validation_error(ctx,
                                              n00b_spec_ptr_typecheck,
                                              best_field_loc(ctx, node),
                                              current_path(ctx),
                                              node->info.field.field_spec->name,
                                              fspec->deferred_type_field,
                                              record->type,
                                              bud_rec->contents,
                                              loc_from_attr(ctx, bud_rec));
                    }
                }
            }
        }
        else {
            t = n00b_unify(n00b_type_copy(fspec->tinfo.type),
                           record->type,
                           NULL);
            if (n00b_type_is_error(t)) {
                n00b_validation_error(ctx,
                                      n00b_spec_field_typecheck,
                                      best_field_loc(ctx, node),
                                      current_path(ctx),
                                      node->info.field.field_spec->name,
                                      fspec->tinfo.type,
                                      record->type,
                                      spec_info_if_used(ctx, node));
            }
        }

        if (fspec->validate_range) {
            // TODO: extract the range... it's currently overwriting the
            // validator field and maybe isn't even checked.
            //
            // Remember this could be int, unsigned or float.
            // Get the boxing right.
            // n00b_spec_out_of_range
        }

        if (fspec->validate_choice) {
            // TODO: same stuff here.
            // Get the boxing and equality testing right.
            // n00b_spec_bad_choice
        }

        if (fspec->validator) {
            // TODO: call this callback, dawg.
            // But it's got to be there; don't be square.
            // n00b_spec_invalid_value,
        }
    }

    // Make sure we saw all required sections.
    for (unsigned int i = 0; i < n00b_list_len(required); i++) {
        if (!n00b_flags_index(reqflags, i)) {
            n00b_spec_field_t *missing = n00b_list_get(required, i, NULL);

            n00b_validation_error(ctx,
                                  n00b_spec_missing_field,
                                  loc_from_decl(ctx->cur),
                                  current_path(ctx),
                                  missing->name);
        }
    }
}

static void spec_validate_section(validation_ctx *);

static void
validate_subsection_names(validation_ctx *ctx, spec_node_t *node)
{
    // This does NOT get called for blueprint attributes.
    // It only gets called on instances and singletons.

    n00b_dict_t         *secdict  = node->info.section.contained_sections;
    n00b_list_t         *view     = n00b_dict_items(secdict);
    int64_t              num_subs = n00b_list_len(view);
    n00b_spec_section_t *secspec  = node->info.section.section_spec;
    n00b_flags_t        *reqflags = NULL;
    n00b_list_t         *reqnames = n00b_set_items(secspec->required_sections);
    int32_t              num_req  = n00b_list_len(reqnames);

    if (num_req != 0) {
        reqflags = n00b_new(n00b_type_flags(), length : num_req);
    }

    // Make sure this spec is allowed at all; we need to see it in the
    // 'required' list or the 'allow' list.

    for (int i = 0; i < num_subs; i++) {
        n00b_tuple_t  *tup  = n00b_list_get(view, i, NULL);
        n00b_string_t *name = n00b_tuple_get(tup, 0);
        spec_node_t   *sub  = n00b_tuple_get(tup, 1);
        bool           ok   = false;

        // If this section name is required, mark that we saw it.
        if (num_req && n00b_set_contains(secspec->required_sections, name)) {
            for (int j = 0; j < num_req; j++) {
                n00b_string_t *s = n00b_list_get(reqnames, j, NULL);

                if (!strcmp(s->data, name->data)) {
                    n00b_flags_set_index(reqflags, j, true);
                    ok = true;
                    break;
                }
            }
        }
        if (!ok && !n00b_set_contains(secspec->allowed_sections, name)) {
            // Set up the right node to error
            ctx->cur     = sub;
            // Keeps us from recursing into broken subsections.
            sub->checked = true;

            if (n00b_dict_get(ctx->spec->section_specs, name, NULL)) {
                n00b_validation_error(ctx,
                                      n00b_spec_disallowed_section,
                                      loc_from_decl(ctx->cur),
                                      current_path(ctx),
                                      name);
            }
            else {
                n00b_validation_error(ctx,
                                      n00b_spec_unknown_section,
                                      loc_from_decl(ctx->cur),
                                      current_path(ctx),
                                      name);
            }
        }

        ctx->cur = node;
    }

    // Make sure we saw all required sections.
    for (int i = 0; i < num_req; i++) {
        if (!n00b_flags_index(reqflags, i)) {
            n00b_validation_error(ctx,
                                  n00b_spec_missing_require,
                                  loc_from_decl(ctx->cur),
                                  n00b_list_get(reqnames, i, NULL));
        }
    }

    validate_field_contents(ctx, node, secspec);

    // Descend to any non-broken sections to check them.
    for (int i = 0; i < num_subs; i++) {
        n00b_tuple_t *tup = n00b_list_get(view, i, NULL);
        spec_node_t  *sub = n00b_tuple_get(tup, 1);

        if (sub->checked) {
            continue;
        }
        ctx->cur = sub;
        spec_validate_section(ctx);
    }

    // TODO: Add back in a section validation callback here.

    ctx->cur = node;
}

static void
spec_validate_section(validation_ctx *ctx)
{
    spec_node_t   *node = (spec_node_t *)ctx->cur;
    section_vinfo *info = &node->info.section;

    if (info->kind == sk_blueprint) {
        uint64_t     num_fields;
        n00b_dict_t *fdict  = info->contained_fields;
        n00b_list_t *fields = n00b_dict_items(fdict);
        num_fields          = n00b_list_len(fields);

        if (num_fields) {
            n00b_tuple_t *tup   = n00b_list_get(fields, 0, NULL);
            spec_node_t  *fnode = n00b_tuple_get(tup, 1);

            n00b_validation_error(ctx,
                                  n00b_spec_blueprint_fields,
                                  node->path,
                                  info->section_spec->name,
                                  n00b_tuple_get(tup, 0),
                                  loc_from_attr(ctx, fnode->info.field.record));
        }
    }
    else {
        validate_subsection_names(ctx, node);
        validate_field_contents(ctx, node, info->section_spec);
    }
}

n00b_list_t *
n00b_validate_runtime(n00b_vm_t *runtime)
{
    validation_ctx ctx;

    spec_init_validation(&ctx, runtime);
    ctx.cur = ctx.section_tree;
    spec_validate_section(&ctx);

    return ctx.errors;
}
