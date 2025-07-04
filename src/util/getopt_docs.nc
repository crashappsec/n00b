#include "n00b.h"

static n00b_list_t *
get_all_specs(n00b_gopt_ctx *ctx, n00b_string_t *path)
{
    n00b_list_t *result = n00b_list(n00b_type_ref());

    if (!path) {
        n00b_list_append(result, hatrack_dict_get(ctx->top_specs, NULL, NULL));
        return result;
    }

    n00b_list_t     *parts = n00b_string_split(path, n00b_cached_period());
    n00b_string_t   *part  = n00b_list_get(parts, 0, NULL);
    n00b_gopt_cspec *cur   = hatrack_dict_get(ctx->top_specs, part, NULL);

    if (!cur) {
        return NULL;
    }

    int n = n00b_list_len(parts);

    for (int i = 1; i < n; i++) {
        part = n00b_list_get(parts, i, NULL);
        cur  = hatrack_dict_get(cur->sub_commands, part, NULL);
        if (!cur) {
            return NULL;
        }
        n00b_list_append(result, cur);
    }

    return result;
}

static inline n00b_gopt_cspec *
get_cmd_spec(n00b_gopt_ctx *ctx, n00b_string_t *path)
{
    n00b_list_t *all = get_all_specs(ctx, path);
    if (!all) {
        return NULL;
    }

    int n = n00b_list_len(all);

    if (!n) {
        return NULL;
    }

    return n00b_list_get(all, n - 1, NULL);
}

// Raw usage info. Returns a tuple of:
// 1. The proper command name.
// 2. The short description.
// 3. The long description.
// 4. A list of subcommands (the command names only)
// 5. A list of flag names explicitly tied to this command.
// 6. Any aliases.
// 7. The full grammar rules as a list of strings.
// 8. The chain of spec objects.
//
// These do not get processed for formatting, etc.
n00b_tuple_t *
n00b_getopt_raw_usage_info(n00b_gopt_ctx *ctx, n00b_string_t *path)
{
    n00b_list_t     *specs = get_all_specs(ctx, path);
    n00b_gopt_cspec *spec  = n00b_list_get(specs, n00b_list_len(specs) - 1, NULL);

    if (!spec) {
        return NULL;
    }

    n00b_tuple_t *result = n00b_new(
        n00b_type_tuple(7,
                        n00b_type_string(),
                        n00b_type_string(),
                        n00b_type_string(),
                        n00b_type_list(n00b_type_string()),
                        n00b_type_list(n00b_type_string()),
                        n00b_type_list(n00b_type_string()),
                        n00b_type_list(n00b_type_string()),
                        n00b_type_list(n00b_type_ref())));

    n00b_tuple_set(result, 0, spec->name);
    n00b_tuple_set(result,
                   1,
                   spec->short_doc
                       ? spec->short_doc
                       : n00b_cstring("No description"));
    n00b_tuple_set(result,
                   2,
                   spec->long_doc
                       ? spec->long_doc
                       : n00b_cstring("No further details available."));

    n00b_tuple_set(result, 3, n00b_dict_keys(spec->sub_commands));
    n00b_tuple_set(result, 4, n00b_dict_keys(spec->owned_opts));
    n00b_tuple_set(result, 5, n00b_shallow(spec->aliases));
    n00b_tuple_set(result, 6, specs);

    return result;
}

static n00b_table_t *
n00b_getopt_command_long(n00b_gopt_cspec *spec)
{
    if (!spec->long_doc) {
        return n00b_call_out(n00b_cstring("No documentation provided."));
    }

    if (!spec->long_doc->styling || spec->long_doc->styling->num_styles == 0) {
        return n00b_markdown_to_table(spec->long_doc, false);
    }

    return n00b_flow(n00b_to_list(n00b_type_ref(), spec->long_doc));
}

static inline bool
cmd_has_sub(n00b_list_t *l, n00b_string_t *s)
{
    if (!s) {
        return false;
    }
    s = n00b_string_strip(s);

    if (!n00b_len(s)) {
        return false;
    }

    int n = n00b_list_len(l);
    for (int i = 0; i < n; i++) {
        n00b_gopt_cspec *spec = n00b_list_get(l, i, NULL);

        if (spec->name && !strcmp(spec->name->data, s->data)) {
            return true;
        }
    }

    return false;
}

// Returns 'usage' info that shows the basic help overview text for a
// command. If there are multiple subcommands, don't give arg signatures.
// Otherwise, give arg signatures.
//
// This is meant for default, automatic help for individual commands.
n00b_table_t *
n00b_getopt_default_usage(n00b_gopt_ctx      *ctx,
                          n00b_string_t      *sub,
                          n00b_gopt_result_t *res)
{
    n00b_string_t   *title;
    n00b_gopt_cspec *spec;
    n00b_list_t     *all_specs = get_all_specs(ctx, sub);
    n00b_gopt_cspec *top_spec  = n00b_list_get(all_specs, 0, NULL);
    n00b_list_t     *parts     = all_specs;
    n00b_string_t   *prefix    = ctx->command_name;
    int              n         = n00b_list_len(parts);

    switch (n) {
    case 0:
        return NULL;
        N00B_CRAISE("Invalid command info.");
    case 1:
        if (!top_spec->name) {
            parts = n00b_dict_values(top_spec->sub_commands);
            n     = n00b_list_len(parts);
        }
        title = n00b_cformat("«i»«b»Usage for«/» «em1»«#»:«/» ", prefix);
        break;
    default:
        title = n00b_cformat("«em1»Subcommand Usage:«/» ");
        break;
    }

    if (prefix) {
        prefix = n00b_string_strip(prefix);
    }

    n00b_table_t *t = n00b_new(n00b_type_table(),
                               columns : 4,
                               style : N00B_TABLE_SIMPLE);

    n00b_table_next_column_fit(t);
    n00b_table_next_column_fit(t);
    n00b_table_next_column_fit(t);
    n00b_table_next_column_flex(t, 1);
    n00b_table_add_cell(t, title, N00B_COL_SPAN_ALL);
    n00b_table_end_row(t);

    for (int i = 0; i < n; i++) {
        n00b_string_t *cmd_name;

        spec = n00b_list_get(parts, i, NULL);
        if (hatrack_dict_len(spec->owned_opts)) {
            cmd_name = n00b_cformat("«em»«#»«/»",
                                    n00b_cstring("«options»"));
        }
        if (spec->name && n00b_string_codepoint_len(spec->name)) {
            cmd_name = n00b_cformat("«b»«#»«/»",
                                    spec->name);
        }

        int m = n00b_list_len(spec->rule_nt->rules);

        for (int j = 0; j < m; j++) {
            n00b_parse_rule_t *pr = n00b_list_get(spec->rule_nt->rules,
                                                  j,
                                                  NULL);
            if (pr->penalty_rule || pr->cost || pr->link) {
                continue;
            }

            n00b_table_add_cell(t, prefix);
            n00b_table_add_cell(t, cmd_name);

            n00b_string_t *user_spec = pr->short_doc;

            if (user_spec && n00b_string_codepoint_len(user_spec)) {
                user_spec = n00b_cformat("«em»«#»«/»", user_spec);
            }

            n00b_table_add_cell(t, user_spec);

            if (pr->doc) {
                n00b_table_add_cell(t, pr->doc);
            }
            else {
                n00b_table_add_cell(t, spec->short_doc);
            }
            n00b_table_end_row(t);
        }
    }

    n = n00b_list_len(top_spec->rule_nt->rules);

    if (n) {
        n00b_list_t *items = n00b_dict_values(top_spec->sub_commands);

        for (int i = 0; i < n; i++) {
            n00b_parse_rule_t *pr = n00b_list_get(top_spec->rule_nt->rules,
                                                  i,
                                                  NULL);

            if (pr->penalty_rule || pr->cost || pr->link || pr->thunk) {
                continue;
            }

            n00b_string_t *s = pr->short_doc;

            if (cmd_has_sub(items, s)) {
                continue;
            }

            if (s && n00b_string_codepoint_len(s)) {
                s = n00b_cformat("«b»«#»«/»", s);
            }
            n00b_table_add_cell(t, prefix);
            n00b_table_add_cell(t, s);
            n00b_table_add_cell(t, n00b_cached_empty_string());

            if (pr->doc) {
                n00b_table_add_cell(t, pr->doc);
            }
            else {
                n00b_table_add_cell(t, top_spec->short_doc);
            }
            n00b_table_end_row(t);
        }
    }

    // An easy way to get an empty line w/o custom padding.
    n00b_table_add_cell(t, n00b_cached_space(), N00B_COL_SPAN_ALL);
    n00b_table_add_cell(t,
                        n00b_getopt_command_long(top_spec),
                        N00B_COL_SPAN_ALL);

    return t;
}

// Returns the formatted 'long' description for terminal printing.  If
// the stored long description is not styled in any way, then it's
// treated as markdown.
n00b_table_t *
n00b_getopt_command_get_long_for_printing(n00b_gopt_ctx *ctx,
                                          n00b_string_t *cmd)
{
    return n00b_getopt_command_long(get_cmd_spec(ctx, cmd));
}

static inline n00b_string_t *
to_flag_name(n00b_goption_t *opt)
{
    if (n00b_string_codepoint_len(opt->name) == 1) {
        return n00b_cformat("-«#»", opt->name);
    }
    return n00b_cformat("--«#»", opt->name);
}

static inline n00b_string_t *
add_arg_info(n00b_string_t *s, n00b_goption_t *opt)
{
    switch (opt->type) {
    case N00B_GOAT_BOOL_T_DEFAULT:
    case N00B_GOAT_BOOL_F_DEFAULT:
        if (opt->max_args) {
            return n00b_cformat("«#»=(t|f)", s);
        }
        return n00b_cformat("«#»=(t|f),(t|f),...", s);
    case N00B_GOAT_WORD:
    case N00B_GOAT_WORD_ALIAS:
        if (opt->max_args) {
            return n00b_cformat("«#»=str", s);
        }
        return n00b_cformat("«#»=str,...", s);
    case N00B_GOAT_INT:
    case N00B_GOAT_INT_ALIAS:
        if (opt->max_args) {
            return n00b_cformat("«#»=int", s);
        }
        return n00b_cformat("«#»=int,...", s);
    case N00B_GOAT_FLOAT:
    case N00B_GOAT_FLOAT_ALIAS:
        if (opt->max_args) {
            return n00b_cformat("«#»=num", s);
        }
        return n00b_cformat("«#»=num,...", s);
    case N00B_GOAT_CHOICE:
        if (opt->max_args) {
            return n00b_cformat("«#»=((})",
                                s,
                                n00b_string_join(opt->choices,
                                                 n00b_cstring("|")));
        }

        return n00b_cformat("«#»=((}),...",
                            s,
                            n00b_string_join(opt->choices,
                                             n00b_cstring("|")));

    default:
        return s;
    }
}

#define add_tf_aliases(X)                                                     \
    n00b_list_t *same = n00b_list(n00b_type_string());                        \
    n00b_list_t *diff = n00b_list(n00b_type_string());                        \
                                                                              \
    n00b_list_append(same, s);                                                \
                                                                              \
    for (int i = 0; i < n00b_list_len(opt->links_inbound); i++) {             \
        n00b_goption_t *link = n00b_list_get(opt->links_inbound, i, NULL);    \
        switch (link->type) {                                                 \
        case N00B_GOAT_BOOL_##X##_DEFAULT:                                    \
        case N00B_GOAT_BOOL_##X##_ALWAYS:                                     \
            n00b_list_append(diff, to_flag_name(link));                       \
            break;                                                            \
        default:                                                              \
            n00b_list_append(same, to_flag_name(link));                       \
        }                                                                     \
    }                                                                         \
                                                                              \
    if (n00b_list_len(same) > 1) {                                            \
        s = n00b_string_join(same, n00b_cached_comma_padded());               \
    }                                                                         \
    if (n00b_list_len(diff)) {                                                \
        s = n00b_cformat("«#»; negate with: «#»",                             \
                         s,                                                   \
                         n00b_string_join(diff, n00b_cached_comma_padded())); \
    }                                                                         \
                                                                              \
    return s

static inline n00b_string_t *
add_t_aliases(n00b_goption_t *opt, n00b_string_t *s)
{
    add_tf_aliases(F);
}

static inline n00b_string_t *
add_f_aliases(n00b_goption_t *opt, n00b_string_t *s)
{
    add_tf_aliases(T);
}

static inline n00b_string_t *
add_choices(n00b_goption_t *opt, n00b_string_t *s)
{
    if (n00b_list_len(opt->links_inbound)) {
        return n00b_cformat("«#»; or use choice as a flag, e.g. --«#»",
                            s,
                            n00b_list_get(opt->choices, 0, NULL));
    }
    return s;
}

static inline n00b_string_t *
add_full_aliases(n00b_goption_t *opt, n00b_string_t *s)
{
    n00b_list_t *l = n00b_list(n00b_type_string());

    n00b_list_append(l, s);

    for (int i = 0; i < n00b_list_len(opt->links_inbound); i++) {
        n00b_goption_t *link = n00b_list_get(opt->links_inbound, i, NULL);
        n00b_list_append(l, to_flag_name(link));
    }

    return n00b_string_join(l, n00b_cached_comma_padded());
}

static n00b_list_t *
one_flag_output(n00b_goption_t *opt)
{
    n00b_list_t   *res = n00b_list(n00b_type_string());
    n00b_string_t *s   = to_flag_name(opt);

    s = add_arg_info(s, opt);

    if (opt->links_inbound) {
        switch (opt->type) {
        case N00B_GOAT_BOOL_T_DEFAULT:
        case N00B_GOAT_BOOL_T_ALWAYS:
            s = add_t_aliases(opt, s);
            break;
        case N00B_GOAT_BOOL_F_DEFAULT:
        case N00B_GOAT_BOOL_F_ALWAYS:
            s = add_f_aliases(opt, s);
            break;
        case N00B_GOAT_CHOICE:
            s = add_choices(opt, s);
            break;
        default:
            s = add_full_aliases(opt, s);
            break;
        }

        n00b_list_append(res, s);
        if (opt->short_doc) {
            n00b_list_append(res, opt->short_doc);
        }
        else {
            n00b_list_append(res, n00b_cstring("No description available."));
        }
    }

    return res;
}

static n00b_table_t *
build_one_flag_table(n00b_gopt_ctx *ctx, n00b_gopt_cspec *spec)
{
    n00b_list_t  *l     = n00b_dict_values(spec->owned_opts);
    int           n     = n00b_list_len(l);
    n00b_table_t *t     = n00b_new(n00b_type_table(),
                               columns : 2,
                               style : N00B_TABLE_SIMPLE);
    int           count = 0;

    for (int i = 0; i < n; i++) {
        n00b_goption_t *opt = n00b_list_get(l, i, NULL);

        if (opt->linked_option && !opt->primary) {
            continue;
        }

        count++;

        n00b_list_t *l = one_flag_output(opt);
        n00b_table_add_row(t, l);
    }

    if (!count) {
        return n00b_call_out(n00b_cstring("None available."));
    }

    if (!(ctx->options & N00B_GOPT_NO_DOUBLE_DASH)) {
        n00b_table_add_cell(t, n00b_cstring("--"));
        n00b_table_add_cell(t, n00b_cstring("Ends flag processing"));
        n00b_table_end_row(t);
    }

    n00b_table_end(t);
    return t;
}

static n00b_table_t *
long_desc_table(n00b_gopt_cspec *spec)
{
    n00b_list_t  *l     = n00b_dict_values(spec->owned_opts);
    n00b_table_t *t     = n00b_new(n00b_type_table(),
                               columns : 2,
                               style : N00B_TABLE_SIMPLE);
    int           count = 0;
    int           n     = n00b_list_len(l);

    for (int i = 0; i < n; i++) {
        n00b_goption_t *opt = n00b_list_get(l, i, NULL);
        if (opt->linked_option && !opt->primary) {
            continue;
        }

        if (!opt->long_doc) {
            continue;
        }
        count++;
        n00b_table_add_cell(t, opt->name);
        n00b_table_add_cell(t, opt->long_doc);
        n00b_table_end_row(t);
    }

    if (count == 0) {
        return NULL;
    }

    n00b_table_end(t);

    return t;
}

// Formats flag info for output for a given command. If 'all' is true,
// then this prints all options (otherwise, it prints options from the
// most specific command, followed by global options if any). If
// 'ldesc' is true, then below each table, a table where long
// descriptions for flags will be printed.

n00b_table_t *
n00b_getopt_option_table(n00b_gopt_ctx *ctx,
                         n00b_string_t *cmd,
                         bool           all,
                         bool           ldesc)
{
    n00b_table_t    *long_table;
    n00b_list_t     *specs = get_all_specs(ctx, cmd);
    int              n     = n00b_list_len(specs);
    n00b_gopt_cspec *spec  = n00b_list_get(specs, n - 1, NULL);
    n00b_table_t    *t     = n00b_new(n00b_type_table(),
                               columns : 1,
                               style : N00B_TABLE_FLOW);

    n00b_table_add_cell(t, n00b_crich("«em2»Command options:"));
    n00b_table_add_cell(t, build_one_flag_table(ctx, spec));

    if (n != 1) {
        spec = n00b_list_get(specs, 0, NULL);

        n00b_table_add_cell(t, n00b_crich("«em2»Global options:"));
        n00b_table_add_cell(t, build_one_flag_table(ctx, spec));
        if (ldesc) {
            long_table = long_desc_table(spec);
            if (long_table) {
                n00b_table_add_cell(t, long_table);
            }
        }
    }

    if (all && n > 2) {
        for (int i = 1; i < n - 1; i++) {
            spec = n00b_list_get(specs, i, NULL);
            n00b_table_add_cell(t, n00b_cformat("«em2»Options for «em»«#»«/»", spec->name));
            n00b_table_add_cell(t, build_one_flag_table(ctx, spec));
            if (ldesc) {
                long_table = long_desc_table(spec);
                if (long_table) {
                    n00b_table_add_cell(t, long_table);
                }
            }
        }
    }

    if (ldesc) {
        long_table = long_desc_table(spec);
        if (long_table) {
            n00b_table_add_cell(t, long_table);
        }
    }

    n00b_table_end(t);

    return t;
}
