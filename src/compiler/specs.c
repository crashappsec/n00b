#include "n00b.h"

static int
section_sort(n00b_spec_section_t **sp1, n00b_spec_section_t **sp2)
{
    return strcmp((*sp1)->name->data, (*sp2)->name->data);
}

static inline n00b_utf8_t *
format_field_opts(n00b_spec_field_t *field)
{
    n00b_list_t *opts = n00b_list(n00b_type_utf8());
    if (field->required) {
        n00b_list_append(opts, n00b_new_utf8("required"));
    }
    else {
        n00b_list_append(opts, n00b_new_utf8("optional"));
    }
    if (field->user_def_ok) {
        n00b_list_append(opts, n00b_new_utf8("user fields ok"));
    }
    else {
        n00b_list_append(opts, n00b_new_utf8("no user fields"));
    }
    if (field->lock_on_write) {
        n00b_list_append(opts, n00b_new_utf8("write lock"));
    }
    if (field->validate_range) {
        n00b_list_append(opts, n00b_new_utf8("range"));
    }
    else {
        if (field->validate_choice) {
            n00b_list_append(opts, n00b_new_utf8("choice"));
        }
    }
    if (field->validator != NULL) {
        n00b_list_append(opts, n00b_new_utf8("validator"));
    }
    if (field->hidden) {
        n00b_list_append(opts, n00b_new_utf8("hide field"));
    }

    return n00b_to_utf8(n00b_str_join(opts, n00b_new_utf8(", ")));
}

n00b_grid_t *
n00b_grid_repr_section(n00b_spec_section_t *section)
{
    n00b_list_t       *l;
    n00b_utf8_t       *s;
    n00b_renderable_t *r;
    n00b_grid_t       *one;
    n00b_grid_t       *result = n00b_new(n00b_type_grid(),
                                 n00b_kw("start_rows",
                                        n00b_ka(1),
                                        "start_cols",
                                        n00b_ka(1),
                                        "container_tag",
                                        n00b_ka(n00b_new_utf8("flow"))));

    if (section->name) {
        n00b_grid_add_row(result,
                         n00b_cstr_format("[h2]Section[/] [h3]{}[/]",

                                         section->name));
    }
    else {
        n00b_grid_add_row(result, n00b_cstr_format("[h2]Root Section"));
    }

    if (section->short_doc) {
        n00b_grid_add_row(result, section->short_doc);
    }
    else {
        l = n00b_list(n00b_type_utf8());
        n00b_list_append(l, n00b_cstr_format("[h1]No short doc provided"));
        n00b_grid_add_row(result, l);
    }
    if (section->long_doc) {
        l = n00b_list(n00b_type_utf8());
        n00b_list_append(l, section->long_doc);
        n00b_grid_add_row(result, l);
    }
    else {
        l = n00b_list(n00b_type_utf8());
        n00b_list_append(l, n00b_cstr_format("[em]Overview not provided[/]"));
        n00b_grid_add_row(result, l);
    }

    n00b_list_t *reqs   = n00b_set_to_xlist(section->required_sections);
    n00b_list_t *allows = n00b_set_to_xlist(section->allowed_sections);
    uint64_t    n      = n00b_list_len(reqs);

    if (!n) {
        l = n00b_list(n00b_type_utf8());
        n00b_list_append(l, n00b_cstr_format("[h1]No required subsections."));
        n00b_grid_add_row(result, l);
    }
    else {
        one = n00b_new(n00b_type_grid(),
                      n00b_kw("start_rows",
                             n00b_ka(2),
                             "start_cols",
                             n00b_ka(n),
                             "container_tag",
                             n00b_ka(n00b_new_utf8("table"))));

        s = n00b_new_utf8("Required Subsections");
        r = n00b_to_str_renderable(s, n00b_new_utf8("h3"));

        // tmp dummy row.
        n00b_grid_add_row(one, n00b_list(n00b_type_utf8()));
        n00b_grid_add_col_span(one, r, 0, 0, n);
        n00b_grid_add_row(one, reqs);
        n00b_grid_add_row(result, one);
    }

    n = n00b_list_len(allows);

    if (!n) {
        l = n00b_list(n00b_type_utf8());
        n00b_list_append(l, n00b_cstr_format("[i]No allowed subsections."));
        n00b_grid_add_row(result, l);
    }
    else {
        one = n00b_new(n00b_type_grid(),
                      n00b_kw("start_rows",
                             n00b_ka(2),
                             "start_cols",
                             n00b_ka(n),
                             "container_tag",
                             n00b_ka(n00b_new_utf8("table"))));

        s = n00b_new_utf8("Allowed Subsections");
        r = n00b_to_str_renderable(s, n00b_new_utf8("h3"));

        n00b_grid_add_row(one, n00b_list(n00b_type_utf8()));
        n00b_grid_add_col_span(one, r, 0, 0, n);
        n00b_grid_add_row(one, allows);
        n00b_grid_add_row(result, one);
    }

    n00b_spec_field_t **fields = (void *)hatrack_dict_values(section->fields,
                                                            &n);

    if (n == 0) {
        l = n00b_list(n00b_type_grid());
        n00b_list_append(l, n00b_callout(n00b_new_utf8("No field specs.")));

        n00b_grid_add_row(result, l);
        return result;
    }

    one = n00b_new(n00b_type_grid(),
                  n00b_kw("start_cols",
                         n00b_ka(7),
                         "start_rows",
                         n00b_ka(2),
                         "th_tag",
                         n00b_ka(n00b_new_utf8("h2")),
                         "stripe",
                         n00b_ka(true),
                         "container_tag",
                         n00b_ka(n00b_new_utf8("table2"))));
    s   = n00b_new_utf8("FIELD SPECIFICATIONS");
    r   = n00b_to_str_renderable(s, n00b_new_utf8("h3"));

    n00b_grid_add_row(one, n00b_list(n00b_type_utf8()));
    n00b_grid_add_col_span(one, r, 0, 0, 7);

    l = n00b_list(n00b_type_utf8());

    n00b_list_append(l, n00b_rich_lit("[h2]Name"));
    n00b_list_append(l, n00b_rich_lit("[h2]Short Doc"));
    n00b_list_append(l, n00b_rich_lit("[h2]Long Doc"));
    n00b_list_append(l, n00b_rich_lit("[h2]Type"));
    n00b_list_append(l, n00b_rich_lit("[h2]Default"));
    n00b_list_append(l, n00b_rich_lit("[h2]Options"));
    n00b_list_append(l, n00b_rich_lit("[h2]Exclusions"));
    n00b_grid_add_row(one, l);

    for (unsigned int i = 0; i < n; i++) {
        l                       = n00b_list(n00b_type_utf8());
        n00b_spec_field_t *field = fields[i];

        n00b_list_append(l, n00b_to_utf8(field->name));
        if (field->short_doc && field->short_doc->codepoints) {
            n00b_list_append(l, field->short_doc);
        }
        else {
            n00b_list_append(l, n00b_new_utf8("None"));
        }
        if (field->long_doc && field->long_doc->codepoints) {
            n00b_list_append(l, field->long_doc);
        }
        else {
            n00b_list_append(l, n00b_new_utf8("None"));
        }
        if (field->have_type_pointer) {
            n00b_list_append(l,
                            n00b_cstr_format("-> {}",
                                            field->tinfo.type_pointer));
        }
        else {
            n00b_list_append(l, n00b_cstr_format("{}", field->tinfo.type));
        }
        if (field->default_provided) {
            n00b_list_append(l, n00b_cstr_format("{}", field->default_value));
        }
        else {
            n00b_list_append(l, n00b_new_utf8("None"));
        }

        n00b_list_append(l, format_field_opts(field));

        n00b_list_t *exclude = n00b_set_to_xlist(field->exclusions);

        if (!n00b_list_len(exclude)) {
            n00b_list_append(l, n00b_new_utf8("None"));
        }
        else {
            n00b_list_append(l, n00b_str_join(exclude, n00b_new_utf8(", ")));
        }

        n00b_grid_add_row(one, l);
    }

    n00b_grid_add_row(result, one);

    return result;
}

n00b_grid_t *
n00b_repr_spec(n00b_spec_t *spec)
{
    if (!spec || !spec->in_use) {
        return n00b_callout(n00b_new_utf8("No specification provided."));
    }

    n00b_grid_t *result = n00b_new(n00b_type_grid(),
                                 n00b_kw("start_rows",
                                        n00b_ka(1),
                                        "start_cols",
                                        n00b_ka(1),
                                        "container_tag",
                                        n00b_ka(n00b_new_utf8("flow"))));

    if (spec->short_doc) {
        n00b_grid_add_row(result, spec->short_doc);
    }
    else {
        n00b_grid_add_row(result,
                         n00b_to_str_renderable(
                             n00b_new_utf8("Configuration Specification"),
                             n00b_new_utf8("h2")));
    }
    if (spec->long_doc) {
        n00b_grid_add_row(result, spec->long_doc);
    }
    else {
        n00b_grid_add_row(result,
                         n00b_cstr_format("[i]Overview not provided[/]"));
    }

    n00b_grid_add_row(result, n00b_grid_repr_section(spec->root_section));

    uint64_t             n;
    n00b_spec_section_t **secs = (void *)hatrack_dict_values(spec->section_specs,
                                                            &n);

    qsort(secs, (size_t)n, sizeof(n00b_spec_section_t *), (void *)section_sort);

    for (unsigned int i = 0; i < n; i++) {
        n00b_grid_add_row(result, n00b_grid_repr_section(secs[i]));
    }

    if (spec->locked) {
        n00b_grid_add_row(result, n00b_cstr_format("[em]This spec is locked."));
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

    section->fields            = n00b_new(n00b_type_dict(n00b_type_utf8(),
                                            n00b_type_ref()));
    section->allowed_sections  = n00b_new(n00b_type_set(n00b_type_utf8()));
    section->required_sections = n00b_new(n00b_type_set(n00b_type_utf8()));

    return section;
}

n00b_spec_t *
n00b_new_spec(void)
{
    n00b_spec_t *result = n00b_gc_alloc_mapped(n00b_spec_t, n00b_spec_gc_bits);

    result->section_specs           = n00b_new(n00b_type_dict(n00b_type_utf8(),
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
    int                 i = 0;
    int                 n = n00b_list_len(fqn) - 1;

    while (true) {
        n00b_utf8_t       *cur_name = n00b_to_utf8(n00b_list_get(fqn, i, NULL));
        n00b_spec_field_t *field    = hatrack_dict_get(cur_sec->fields,
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

        next_sec = hatrack_dict_get(spec->section_specs, cur_name, NULL);

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
