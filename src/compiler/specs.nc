#include "n00b.h"

static int
section_sort(n00b_spec_section_t **sp1, n00b_spec_section_t **sp2)
{
    return strcmp((*sp1)->name->data, (*sp2)->name->data);
}

static inline n00b_string_t *
format_field_opts(n00b_spec_field_t *field)
{
    n00b_list_t *opts = n00b_list(n00b_type_string());
    if (field->required) {
        n00b_list_append(opts, n00b_cstring("required"));
    }
    else {
        n00b_list_append(opts, n00b_cstring("optional"));
    }
    if (field->user_def_ok) {
        n00b_list_append(opts, n00b_cstring("user fields ok"));
    }
    else {
        n00b_list_append(opts, n00b_cstring("no user fields"));
    }
    if (field->lock_on_write) {
        n00b_list_append(opts, n00b_cstring("write lock"));
    }
    if (field->validate_range) {
        n00b_list_append(opts, n00b_cstring("range"));
    }
    else {
        if (field->validate_choice) {
            n00b_list_append(opts, n00b_cstring("choice"));
        }
    }
    if (field->validator != NULL) {
        n00b_list_append(opts, n00b_cstring("validator"));
    }
    if (field->hidden) {
        n00b_list_append(opts, n00b_cstring("hide field"));
    }

    return n00b_string_join(opts, n00b_cached_comma_padded());
}

n00b_table_t *
n00b_table_repr_section(n00b_spec_section_t *section)
{
    n00b_string_t *s;
    n00b_table_t  *result = n00b_table(columns : 1, style : N00B_TABLE_SIMPLE);

    if (section->name) {
        n00b_table_add_cell(result,
                            n00b_cformat("«em2»Section«/» «em3»«#»«/»",

                                         section->name));
    }
    else {
        n00b_table_add_cell(result, n00b_cformat("«em2»Root Section"));
    }

    if (section->short_doc) {
        n00b_table_add_cell(result, section->short_doc);
    }
    else {
        n00b_table_add_cell(result, n00b_cformat("«em1»No short doc provided"));
    }
    if (section->long_doc) {
        n00b_table_add_cell(result, section->long_doc);
    }
    else {
        n00b_table_add_cell(result, n00b_cformat("«em»Overview not provided"));
    }

    n00b_list_t *reqs   = n00b_set_items(section->required_sections);
    n00b_list_t *allows = n00b_set_items(section->allowed_sections);
    uint64_t     n      = n00b_list_len(reqs);

    if (!n) {
        n00b_table_add_cell(result,
                            n00b_cformat("«em1»No required subsections."));
    }
    else {
        n00b_table_add_cell(result,
                            n00b_cformat("«em3»Required Subsections"));
        n00b_table_add_cell(result,
                            n00b_string_join(reqs, n00b_cached_comma_padded()));
    }

    n = n00b_list_len(allows);

    if (!n) {
        n00b_table_add_cell(result, n00b_cformat("«i»No allowed subsections."));
    }
    else {
        n00b_table_add_cell(result,
                            n00b_cformat("«em3»Allowed Subsections"));
        n00b_table_add_cell(result,
                            n00b_string_join(allows,
                                             n00b_cached_comma_padded()));
    }

    n00b_spec_field_t **fields = (void *)n00b_dict_values(section->fields,
                                                          &n);

    if (n == 0) {
        n00b_table_add_cell(result,
                            n00b_call_out(n00b_cstring("No field specs.")));

        return result;
    }

    n00b_table_t *sub = n00b_table(columns : 7,
                                   title : n00b_cstring("FIELD SPECIFICATIONS"));

    n00b_table_add_cell(sub, n00b_cstring("Name"));
    n00b_table_add_cell(sub, n00b_cstring("Short Doc"));
    n00b_table_add_cell(sub, n00b_cstring("Long Doc"));
    n00b_table_add_cell(sub, n00b_cstring("Type"));
    n00b_table_add_cell(sub, n00b_cstring("Default"));
    n00b_table_add_cell(sub, n00b_cstring("Options"));
    n00b_table_add_cell(sub, n00b_cstring("Exclusions"));

    for (unsigned int i = 0; i < n; i++) {
        n00b_spec_field_t *field = fields[i];

        n00b_table_add_cell(sub, field->name);

        if (field->short_doc && field->short_doc->codepoints) {
            n00b_table_add_cell(sub, field->short_doc);
        }
        else {
            n00b_table_add_cell(sub, n00b_crich("«em»None"));
        }
        if (field->long_doc && field->long_doc->codepoints) {
            n00b_table_add_cell(sub, field->long_doc);
        }
        else {
            n00b_table_add_cell(sub, n00b_crich("«em»None"));
        }
        if (field->have_type_pointer) {
            s = n00b_cformat("-> «#»", field->tinfo.type_pointer);
            n00b_table_add_cell(sub, s);
        }
        else {
            n00b_table_add_cell(sub, n00b_cformat("«#»", field->tinfo.type));
        }
        if (field->default_provided) {
            n00b_table_add_cell(sub, n00b_cformat("«#»", field->default_value));
        }
        else {
            n00b_table_add_cell(sub, n00b_crich("«em»None"));
        }

        n00b_table_add_cell(sub, format_field_opts(field));

        n00b_list_t *exclude = n00b_set_items(field->exclusions);

        if (!n00b_list_len(exclude)) {
            n00b_table_add_cell(sub, n00b_crich("«em»None"));
        }
        else {
            s = n00b_string_join(exclude, n00b_cached_comma_padded());
            n00b_table_add_cell(sub, s);
        }
    }

    n00b_table_add_cell(result, sub);

    return result;
}

n00b_table_t *
n00b_repr_spec(n00b_spec_t *spec)
{
    if (!spec || !spec->in_use) {
        return n00b_call_out(n00b_cstring("No specification provided."));
    }

    n00b_table_t         *result = n00b_table(columns : 1,
                                      style : N00B_TABLE_SIMPLE);
    n00b_spec_section_t **secs;
    uint64_t              n;

    if (spec->short_doc) {
        n00b_table_add_cell(result, spec->short_doc);
    }
    else {
        n00b_table_add_cell(result, n00b_crich("«em2»Configuration Spec"));
    }
    if (spec->long_doc) {
        n00b_table_add_cell(result, spec->long_doc);
    }
    else {
        n00b_table_add_cell(result,
                            n00b_crich("«i»Overview not provided"));
    }

    secs = (void *)n00b_dict_values(spec->section_specs, &n);

    qsort(secs, (size_t)n, sizeof(n00b_spec_section_t *), (void *)section_sort);

    for (unsigned int i = 0; i < n; i++) {
        n00b_table_add_cell(result, n00b_table_repr_section(secs[i]));
    }

    if (spec->locked) {
        n00b_table_add_cell(result, n00b_crich("«em»This spec is locked."));
    }

    return result;
}

void
n00b_spec_gc_bits(uint64_t *bitmap, n00b_spec_t *spec)
{
    n00b_mark_raw_to_addr(bitmap, spec, &spec->section_specs);
}

void
n00b_attr_info_gc_bits(uint64_t *bitmap, n00b_attr_info_t *ai)
{
    n00b_mark_raw_to_addr(bitmap, ai, &ai->info.field_info);
}

void
n00b_section_gc_bits(uint64_t *bitmap, n00b_spec_section_t *sec)
{
    n00b_mark_raw_to_addr(bitmap, sec, &sec->validator);
}

void
n00b_spec_field_gc_bits(uint64_t *bitmap, n00b_spec_field_t *field)
{
    n00b_mark_raw_to_addr(bitmap, field, &field->exclusions);
}

n00b_spec_field_t *
n00b_new_spec_field(void)
{
    return n00b_gc_alloc_mapped(n00b_spec_field_t, n00b_spec_field_gc_bits);
}

n00b_spec_section_t *
n00b_new_spec_section(void)
{
    n00b_spec_section_t *section = n00b_gc_alloc_mapped(n00b_spec_section_t,
                                                        n00b_section_gc_bits);

    section->fields            = n00b_new(n00b_type_dict(n00b_type_string(),
                                              n00b_type_ref()));
    section->allowed_sections  = n00b_new(n00b_type_set(n00b_type_string()));
    section->required_sections = n00b_new(n00b_type_set(n00b_type_string()));

    return section;
}

n00b_spec_t *
n00b_new_spec(void)
{
    n00b_spec_t *result = n00b_gc_alloc_mapped(n00b_spec_t, n00b_spec_gc_bits);

    result->section_specs           = n00b_new(n00b_type_dict(n00b_type_string(),
                                                    n00b_type_ref()));
    result->root_section            = n00b_new_spec_section();
    result->root_section->singleton = true;

    return result;
}

n00b_attr_info_t *
n00b_get_attr_info(n00b_spec_t *spec, n00b_list_t *fqn)
{
    n00b_attr_info_t *result = n00b_gc_alloc_mapped(n00b_attr_info_t,
                                                    n00b_attr_info_gc_bits);

    if (!spec || !spec->root_section || !spec->in_use) {
        result->kind = n00b_attr_user_def_field;
        return result;
    }

    n00b_spec_section_t *cur_sec = spec->root_section;
    n00b_spec_section_t *next_sec;
    int                  i = 0;
    int                  n = n00b_list_len(fqn) - 1;

    while (true) {
        n00b_string_t     *cur_name = n00b_list_get(fqn, i, NULL);
        n00b_spec_field_t *field    = n00b_dict_get(cur_sec->fields,
                                                 cur_name,
                                                 NULL);
        if (field != NULL) {
            if (i != n) {
                result->err = n00b_attr_err_sec_under_field;
                return result;
            }
            else {
                result->info.field_info = field;
                result->kind            = n00b_attr_field;

                return result;
            }
        }

        next_sec = n00b_dict_get(spec->section_specs, cur_name, NULL);

        if (next_sec == NULL) {
            if (i == n) {
                if (cur_sec->user_def_ok) {
                    result->kind = n00b_attr_user_def_field;
                    return result;
                }
                else {
                    result->err     = n00b_attr_err_field_not_allowed;
                    result->err_arg = cur_name;
                    return result;
                }
            }
            else {
                result->err_arg = cur_name;
                result->err     = n00b_attr_err_no_such_sec;
                return result;
            }
        }

        if (!n00b_set_contains(cur_sec->allowed_sections, cur_name)) {
            if (!n00b_set_contains(cur_sec->required_sections, cur_name)) {
                result->err_arg = cur_name;
                result->err     = n00b_attr_err_sec_not_allowed;
                return result;
            }
        }

        if (i == n) {
            if (next_sec->singleton) {
                result->kind = n00b_attr_singleton;
            }
            else {
                result->kind = n00b_attr_object_type;
            }

            result->info.sec_info = next_sec;

            return result;
        }

        cur_sec = next_sec;

        if (cur_sec->singleton == false) {
            i += 1;
            if (i == n) {
                result->kind          = n00b_attr_instance;
                result->info.sec_info = next_sec;

                return result;
            }
        }
        i += 1;
    }
}
