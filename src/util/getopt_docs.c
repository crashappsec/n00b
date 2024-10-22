#include "n00b.h"

static n00b_list_t *
get_all_specs(n00b_gopt_ctx *ctx, n00b_utf8_t *path)
{
    n00b_list_t *result = n00b_list(n00b_type_ref());

    if (!path) {
        n00b_list_append(result, hatrack_dict_get(ctx->top_specs, NULL, NULL));
        return result;
    }

    n00b_list_t     *parts = n00b_str_split(path, n00b_new_utf8("."));
    n00b_utf8_t     *part  = n00b_list_get(parts, 0, NULL);
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
get_cmd_spec(n00b_gopt_ctx *ctx, n00b_utf8_t *path)
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
n00b_getopt_raw_usage_info(n00b_gopt_ctx *ctx, n00b_utf8_t *path)
{
    n00b_list_t     *specs = get_all_specs(ctx, path);
    n00b_gopt_cspec *spec  = n00b_list_get(specs, n00b_list_len(specs) - 1, NULL);

    if (!spec) {
        return NULL;
    }

    n00b_tuple_t *result = n00b_new(
        n00b_type_tuple(8,
                       n00b_type_utf8(),
                       n00b_type_utf8(),
                       n00b_type_utf8(),
                       n00b_type_list(n00b_type_utf8()),
                       n00b_type_list(n00b_type_utf8()),
                       n00b_type_list(n00b_type_utf8()),
                       n00b_type_list(n00b_type_utf8()),
                       n00b_type_list(n00b_type_ref())));

    n00b_tuple_set(result, 0, spec->name);
    n00b_tuple_set(result,
                  1,
                  spec->short_doc
                      ? spec->short_doc
                      : n00b_new_utf8("No description"));
    n00b_tuple_set(result,
                  2,
                  spec->long_doc
                      ? spec->long_doc
                      : n00b_new_utf8("No further details available."));

    n00b_tuple_set(result, 3, n00b_dict_keys(spec->sub_commands));
    n00b_tuple_set(result, 4, n00b_dict_keys(spec->owned_opts));
    n00b_tuple_set(result, 5, n00b_shallow(spec->aliases));
    n00b_tuple_set(result, 6, n00b_shallow(spec->summary_docs));
    n00b_tuple_set(result, 7, specs);

    return result;
}

// Returns 'usage' info that shows the basic help overview text for a
// command. If there are multiple subcommands, don't give arg signatures.
// Otherwise, give arg signatures.
//
// This is meant for default, automatic help for individual commands.
n00b_grid_t *
n00b_getopt_default_usage(n00b_gopt_ctx *ctx, n00b_utf8_t *sub)
{
    n00b_utf8_t     *title;
    n00b_gopt_cspec *spec;
    n00b_list_t     *parts  = get_all_specs(ctx, sub);
    n00b_list_t     *cmds   = n00b_list(n00b_type_utf8());
    n00b_utf8_t     *prefix = n00b_new_utf8("");
    int             n      = n00b_list_len(parts);

    switch (n) {
    case 0:
        N00B_CRAISE("Invalid command info.");
    case 1:
        title = n00b_cstr_format("[h1]Usage:[/] ");
        break;
    default:
        title = n00b_cstr_format("[h1]Subcommand Usage:[/] ");
        break;
    }

    for (int i = 0; i < n; i++) {
        spec = n00b_list_get(parts, i, NULL);
        if (hatrack_dict_len(spec->owned_opts)) {
            prefix = n00b_cstr_format("{}{} ",
                                     prefix,
                                     n00b_new_utf8("[options]"));
        }
        if (spec->name && n00b_str_codepoint_len(spec->name)) {
            prefix = n00b_cstr_format("{}{} ", prefix, spec->name);
        }
    }

    n = n00b_list_len(spec->summary_docs);
    if (!n) {
        n00b_list_append(cmds, n00b_to_str_renderable(prefix, NULL));
    }
    for (int i = 0; i < n; i++) {
        n00b_utf8_t *s = n00b_list_get(spec->summary_docs, i, NULL);
        s             = n00b_cstr_format("{}{}", prefix, s);
        n00b_list_append(cmds, n00b_to_str_renderable(s, NULL));
    }

    n00b_grid_t *res = n00b_new(n00b_type_grid(),
                              n00b_kw("start_rows",
                                     1ULL,
                                     "start_cols",
                                     2ULL,
                                     "container_tag",
                                     n00b_new_utf8("flow")));
    n00b_grid_set_cell_contents(res, 0, 0, n00b_to_str_renderable(title, NULL));
    n00b_grid_set_cell_contents(res, 0, 1, n00b_grid_flow_from_list(cmds));

    return res;
}

// Returns the formatted 'long' description for terminal printing.  If
// the stored long description is not styled in any way, then it's
// treated as markdown.
n00b_grid_t *
n00b_getopt_command_get_long_for_printing(n00b_gopt_ctx *ctx, n00b_utf8_t *cmd)
{
    n00b_gopt_cspec *spec = get_cmd_spec(ctx, cmd);

    if (!spec->long_doc) {
        return n00b_grid_flow(1, n00b_new_utf8("[em]No documentation.[/]"));
    }

    if (!spec->long_doc->styling || spec->long_doc->styling->num_entries == 0) {
        return n00b_grid_flow(1, spec->long_doc);
    }

    return n00b_markdown_to_grid(spec->long_doc, false);
}

static inline n00b_utf8_t *
to_flag_name(n00b_goption_t *opt)
{
    if (n00b_str_codepoint_len(opt->name) == 1) {
        return n00b_cstr_format("-{}", opt->name);
    }
    return n00b_cstr_format("--{}", opt->name);
}

static inline n00b_utf8_t *
add_arg_info(n00b_utf8_t *s, n00b_goption_t *opt)
{
    switch (opt->type) {
    case N00B_GOAT_BOOL_T_DEFAULT:
    case N00B_GOAT_BOOL_F_DEFAULT:
        if (opt->max_args) {
            return n00b_cstr_format("{}=(t|f)", s);
        }
        return n00b_cstr_format("{}=(t|f),(t|f),...", s);
    case N00B_GOAT_WORD:
    case N00B_GOAT_WORD_ALIAS:
        if (opt->max_args) {
            return n00b_cstr_format("{}=str", s);
        }
        return n00b_cstr_format("{}=str,...", s);
    case N00B_GOAT_INT:
    case N00B_GOAT_INT_ALIAS:
        if (opt->max_args) {
            return n00b_cstr_format("{}=int", s);
        }
        return n00b_cstr_format("{}=int,...", s);
    case N00B_GOAT_FLOAT:
    case N00B_GOAT_FLOAT_ALIAS:
        if (opt->max_args) {
            return n00b_cstr_format("{}=num", s);
        }
        return n00b_cstr_format("{}=num,...", s);
    case N00B_GOAT_CHOICE:
        if (opt->max_args) {
            return n00b_cstr_format("{}=((})",
                                   s,
                                   n00b_str_join(opt->choices,
                                                n00b_new_utf8("|")));
        }

        return n00b_cstr_format("{}=((}),...",
                               s,
                               n00b_str_join(opt->choices,
                                            n00b_new_utf8("|")));

    default:
        return s;
    }
}

#define add_tf_aliases(X)                                                \
    n00b_list_t *same = n00b_list(n00b_type_utf8());                        \
    n00b_list_t *diff = n00b_list(n00b_type_utf8());                        \
                                                                         \
    n00b_list_append(same, s);                                            \
                                                                         \
    for (int i = 0; i < n00b_list_len(opt->links_inbound); i++) {         \
        n00b_goption_t *link = n00b_list_get(opt->links_inbound, i, NULL); \
        switch (link->type) {                                            \
        case N00B_GOAT_BOOL_##X##_DEFAULT:                                \
        case N00B_GOAT_BOOL_##X##_ALWAYS:                                 \
            n00b_list_append(diff, to_flag_name(link));                   \
            break;                                                       \
        default:                                                         \
            n00b_list_append(same, to_flag_name(link));                   \
        }                                                                \
    }                                                                    \
                                                                         \
    if (n00b_list_len(same) > 1) {                                        \
        s = n00b_str_join(same, n00b_new_utf8(", "));                      \
    }                                                                    \
    if (n00b_list_len(diff)) {                                            \
        s = n00b_cstr_format("{}; negate with: {}",                       \
                            s,                                           \
                            n00b_str_join(diff, n00b_new_utf8(", ")));     \
    }                                                                    \
                                                                         \
    return s

static inline n00b_utf8_t *
add_t_aliases(n00b_goption_t *opt, n00b_utf8_t *s)
{
    add_tf_aliases(F);
}

static inline n00b_utf8_t *
add_f_aliases(n00b_goption_t *opt, n00b_utf8_t *s)
{
    add_tf_aliases(T);
}

static inline n00b_utf8_t *
add_choices(n00b_goption_t *opt, n00b_utf8_t *s)
{
    if (n00b_list_len(opt->links_inbound)) {
        return n00b_cstr_format("{}; or use choice as a flag, e.g. --{}",
                               s,
                               n00b_list_get(opt->choices, 0, NULL));
    }
    return s;
}

static inline n00b_utf8_t *
add_full_aliases(n00b_goption_t *opt, n00b_utf8_t *s)
{
    n00b_list_t *l = n00b_list(n00b_type_utf8());

    n00b_list_append(l, s);

    for (int i = 0; i < n00b_list_len(opt->links_inbound); i++) {
        n00b_goption_t *link = n00b_list_get(opt->links_inbound, i, NULL);
        n00b_list_append(l, to_flag_name(link));
    }

    return n00b_str_join(l, n00b_new_utf8(", "));
}

static n00b_list_t *
one_flag_output(n00b_goption_t *opt)
{
    n00b_list_t *res = n00b_list(n00b_type_utf8());
    n00b_utf8_t *s   = to_flag_name(opt);

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
            n00b_list_append(res, n00b_new_utf8("No description available."));
        }
    }

    return res;
}

static n00b_grid_t *
build_one_flag_table(n00b_gopt_ctx *ctx, n00b_gopt_cspec *spec)
{
    n00b_list_t *l     = n00b_dict_values(spec->owned_opts);
    int         n     = n00b_list_len(l);
    n00b_grid_t *grid  = n00b_new(n00b_type_grid(),
                               n00b_kw("start_cols",
                                      n00b_ka(2),
                                      "header_rows",
                                      n00b_ka(0),
                                      "container_tag",
                                      n00b_ka(n00b_new_utf8("flow"))));
    int         count = 0;

    for (int i = 0; i < n; i++) {
        n00b_goption_t *opt = n00b_list_get(l, i, NULL);

        if (opt->linked_option && !opt->primary) {
            continue;
        }

        count++;

        n00b_list_t *l = one_flag_output(opt);
        n00b_grid_add_row(grid, l);
    }

    if (!count) {
        n00b_list_t *items = n00b_list(n00b_type_utf8());
        n00b_list_append(items, n00b_rich_lit("[em]None available.[/]"));
        return n00b_grid_flow_from_list(items);
    }

    if (!(ctx->options & N00B_GOPT_NO_DOUBLE_DASH)) {
        n00b_list_t *row = n00b_list(n00b_type_utf8());
        n00b_list_append(row, n00b_new_utf8("--"));
        n00b_list_append(row, n00b_new_utf8("Ends flag processing"));
        n00b_grid_add_row(grid, row);
    }

    return grid;
}

static n00b_grid_t *
long_desc_table(n00b_gopt_cspec *spec)
{
    n00b_list_t *items;
    n00b_list_t *l     = n00b_dict_values(spec->owned_opts);
    n00b_grid_t *grid  = n00b_new(n00b_type_grid(),
                               n00b_kw("start_cols",
                                      n00b_ka(2),
                                      "header_rows",
                                      n00b_ka(0),
                                      "container_tag",
                                      n00b_ka(n00b_new_utf8("flow"))));
    int         count = 0;
    int         n     = n00b_list_len(l);

    for (int i = 0; i < n; i++) {
        n00b_goption_t *opt = n00b_list_get(l, i, NULL);
        if (opt->linked_option && !opt->primary) {
            continue;
        }

        if (!opt->long_doc) {
            continue;
        }
        count++;
        items = n00b_list(n00b_type_utf8());
        n00b_list_append(items, opt->name);
        n00b_list_append(items, opt->long_doc);
        n00b_grid_add_row(grid, items);
    }

    if (count == 0) {
        return NULL;
    }

    return grid;
}

// Formats flag info for output for a given command. If 'all' is true,
// then this prints all options (otherwise, it prints options from the
// most specific command, followed by global options if any). If
// 'ldesc' is true, then below each table, a table where long
// descriptions for flags will be printed.

n00b_grid_t *
n00b_getopt_option_table(n00b_gopt_ctx *ctx,
                        n00b_utf8_t   *cmd,
                        bool          all,
                        bool          ldesc)
{
    n00b_list_t     *specs  = get_all_specs(ctx, cmd);
    n00b_list_t     *pieces = n00b_list(n00b_type_renderable());
    int             n      = n00b_list_len(specs);
    n00b_gopt_cspec *spec   = n00b_list_get(specs, n - 1, NULL);
    n00b_list_t     *longs;
    n00b_grid_t     *long_grid;

    if (ldesc) {
        longs = n00b_list(n00b_type_renderable());
    }

    n00b_list_append(pieces,
                    n00b_to_str_renderable(n00b_new_utf8("Command options:"),
                                          n00b_new_utf8("h2")));

    n00b_list_append(pieces, build_one_flag_table(ctx, spec));

    if (ldesc) {
        long_grid = long_desc_table(spec);
        if (long_grid) {
            n00b_list_append(longs, long_grid);
        }
    }

    if (n != 1) {
        spec = n00b_list_get(specs, 0, NULL);

        n00b_list_append(pieces,
                        n00b_to_str_renderable(n00b_new_utf8("Global options:"),
                                              n00b_new_utf8("h2")));
        n00b_list_append(pieces, build_one_flag_table(ctx, spec));
        if (ldesc) {
            long_grid = long_desc_table(spec);
            if (long_grid) {
                n00b_list_append(longs, long_grid);
            }
        }
    }

    if (all && n > 2) {
        for (int i = 1; i < n - 1; i++) {
            spec = n00b_list_get(specs, i, NULL);
            n00b_list_append(pieces,
                            n00b_to_str_renderable(
                                n00b_cstr_format("Options for [em]{}[/]",
                                                spec->name),
                                n00b_new_utf8("h2")));
            n00b_list_append(pieces, build_one_flag_table(ctx, spec));
            if (ldesc) {
                long_grid = long_desc_table(spec);
                if (long_grid) {
                    n00b_list_append(longs, long_grid);
                }
            }
        }
    }

    if (ldesc && n00b_list_len(longs)) {
        pieces = n00b_list_plus(pieces, longs);
    }

    return n00b_grid_flow_from_list(pieces);
}
