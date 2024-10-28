#define N00B_USE_INTERNAL_API
#include "n00b.h"

static void gopt_extract_spec(n00b_gopt_extraction_ctx *);

static inline bool
is_group_node(n00b_parse_node_t *cur)
{
    return cur->group_top == true || cur->group_item == true;
}

static n00b_utf8_t *
get_node_text(n00b_tree_node_t *n)
{
    while (n->num_kids) {
        n = n->children[0];
    }

    n00b_parse_node_t *pn = n00b_tree_get_contents(n);
    return pn->info.token->value;
}

static void
handle_subcommand(n00b_gopt_extraction_ctx *ctx)
{
    // In case we decide to care about specifically tying flags to
    // where they showed up, we do ahead and save / restore
    // command state.

    n00b_tree_node_t *n = ctx->intree;

    while (n->num_kids) {
        n = n->children[0];
    }

    int                len        = n->num_kids;
    n00b_parse_node_t *cmd_node   = n00b_tree_get_contents(n);
    n00b_utf8_t       *saved_path = ctx->path;
    n00b_gopt_cspec   *cmd        = ctx->cur_cmd;
    int64_t            cmd_key;

    cmd_key      = cmd_node->info.token->tid;
    ctx->cur_cmd = hatrack_dict_get(cmd->sub_commands,
                                    (void *)cmd_key,
                                    NULL);

    if (!ctx->cur_cmd) {
        return;
    }

    n00b_list_append(ctx->cmd_stack, ctx->cur_cmd);

    n00b_utf8_t *cmd_name = ctx->cur_cmd->name;

    if (n00b_str_codepoint_len(saved_path)) {
        ctx->path = n00b_cstr_format("{}.{}", saved_path, cmd_name);
    }
    else {
        ctx->path = cmd_name;
    }

    n00b_list_t *args = n00b_list(n00b_type_ref());
    hatrack_dict_put(ctx->args, ctx->path, args);

    ctx->deepest_path = ctx->path;

    for (int i = 1; i < len; i++) {
        ctx->intree = n->children[i];
        gopt_extract_spec(ctx);
    }

    ctx->intree = n;
}

static inline void
boxed_bool_negation(bool *b)
{
    *b = !*b;
}

static bool
needs_negation(n00b_goption_t *o1, n00b_goption_t *o2)
{
    bool pos_1;
    bool pos_2;

    switch (o1->type) {
    case N00B_GOAT_BOOL_T_DEFAULT:
    case N00B_GOAT_BOOL_T_ALWAYS:
        pos_1 = true;
        break;
    default:
        pos_1 = false;
        break;
    }

    switch (o2->type) {
    case N00B_GOAT_BOOL_T_DEFAULT:
    case N00B_GOAT_BOOL_T_ALWAYS:
        pos_2 = true;
        break;
    default:
        pos_2 = false;
        break;
    }

    return pos_1 ^ pos_2;
}

static inline void
negate_bool_array(n00b_list_t *l)
{
    int n = n00b_list_len(l);

    for (int i = 0; i < n; i++) {
        bool *b = n00b_list_get(l, i, NULL);
        boxed_bool_negation(b);
    }
}

static n00b_utf8_t *
bad_num(n00b_gopt_extraction_ctx *ctx, n00b_parse_node_t *cur)
{
    n00b_utf8_t *msg = n00b_cstr_format("Number out of range: [em]{}[/] ",
                                        cur->info.token->value);
    n00b_list_append(ctx->errors, msg);

    return cur->info.token->value;
}

static inline void *
extract_value_from_node(n00b_gopt_extraction_ctx *ctx,
                        n00b_tree_node_t         *n,
                        bool                     *ok)
{
    n00b_parse_node_t  *cur = n00b_tree_get_contents(n);
    n00b_grammar_t     *g   = ctx->parser->grammar;
    n00b_gopt_node_type gopt_type;
    n00b_list_t        *items;
    int64_t             intval;
    double              dval;
    n00b_obj_t          val;

    if (ok) {
        *ok = true;
    }

    gopt_type = (n00b_gopt_node_type)n00b_parse_get_user_data(g, cur);

    while (!cur->token) {
        n   = n->children[0];
        cur = n00b_tree_get_contents(n);
    }

    switch (gopt_type) {
    case N00B_GTNT_FLOAT_NT:
        if ((cur->info.token->tid - N00B_START_TOK_ID) == N00B_GOTT_FLOAT) {
            if (!n00b_parse_double(cur->info.token->value, &dval)) {
                return bad_num(ctx, cur);
            }
            return n00b_box_double(dval);
        }
        if (!n00b_parse_int64(cur->info.token->value, &intval)) {
            return bad_num(ctx, cur);
        }
        return n00b_box_double(intval + 0.0);

    case N00B_GTNT_INT_NT:

        if (!n00b_parse_int64(cur->info.token->value, &intval)) {
            return bad_num(ctx, cur);
        }

        return n00b_box_i64(intval);

    case N00B_GTNT_BOOL_NT:
        if ((cur->info.token->tid - N00B_START_TOK_ID) == N00B_GOTT_BOOL_T) {
            return n00b_box_bool(true);
        }
        return n00b_box_bool(false);
    case N00B_GTNT_WORD_NT:
        val = n00b_to_utf8(cur->info.token->value);
        if (!val && ok) {
            *ok = false;
        }

        return val;
    case N00B_GTNT_OTHER:
        if (!cur) {
            return NULL;
        }

        if (is_group_node(cur)) {
            items = n00b_list(n00b_type_ref());

            bool v;

            for (int i = 0; i < n->num_kids; i++) {
                val = extract_value_from_node(ctx, n->children[i], &v);

                if (v) {
                    n00b_list_append(items, val);
                }
            }
            switch (n00b_list_len(items)) {
            case 0:
                if (ok) {
                    *ok = NULL;
                }
                return NULL;
            case 1:
                return n00b_list_get(items, 0, NULL);
            default:
                return n00b_clean_internal_list(items);
            }
        }

        if (ok) {
            *ok = false;
        }
        return NULL;

    default:
        if (ok) {
            *ok = false;
        }
        return NULL;
    }
}

static int
get_option_arg_offset(n00b_tree_node_t *n)
{
    n00b_tree_node_t *sub = n->children[1];

    if (sub->num_kids) {
        n00b_parse_node_t *pnode = n00b_tree_get_contents(sub->children[0]);
        if ((pnode->info.token->tid - N00B_START_TOK_ID) == N00B_GOTT_ASSIGN) {
            return 2;
        }
    }

    return 1;
}

static inline void
extract_other_args(n00b_gopt_extraction_ctx *ctx,
                   n00b_list_t              *alist,
                   n00b_tree_node_t         *n)
{
    n00b_parse_node_t *pn = n00b_tree_get_contents(n);

    switch ((int64_t)n00b_parse_get_user_data(ctx->parser->grammar, pn)) {
    case N00B_GTNT_FLOAT_NT:
    case N00B_GTNT_INT_NT:
    case N00B_GTNT_BOOL_NT:
    case N00B_GTNT_WORD_NT:
        n00b_list_append(alist, extract_value_from_node(ctx, n, NULL));
        return;
    default:
        for (int i = 0; i < n->num_kids; i++) {
            extract_other_args(ctx, alist, n->children[i]);
        }
    }
}

static void
handle_option_rule(n00b_gopt_extraction_ctx *ctx, n00b_tree_node_t *n)
{
    int                len              = n->num_kids;
    n00b_utf8_t       *name             = get_node_text(n);
    n00b_tree_node_t  *name_tnode       = n->children[0]->children[0];
    n00b_parse_node_t *name_pnode       = n00b_tree_get_contents(name_tnode);
    int                flag_id          = name_pnode->info.token->tid;
    n00b_list_t       *extracted_values = n00b_list(n00b_type_ref());
    int64_t            key;

    if (flag_id == (N00B_START_TOK_ID + N00B_GOTT_UNKNOWN_OPT)) {
        n00b_utf8_t *msg = n00b_cstr_format("Unknown option: [em]{}[/] ",
                                            name_pnode->info.token->value);
        n00b_list_append(ctx->errors, msg);
        return;
    }

    n00b_goption_t *cur_option = hatrack_dict_get(ctx->gctx->all_options,
                                                  name,
                                                  NULL);
    if (!cur_option) {
        return;
    }

    // when len == 1, no arg was provided.
    if (len != 1) {
        int offset = get_option_arg_offset(n);

        n00b_list_append(extracted_values,
                         extract_value_from_node(ctx,
                                                 n->children[offset],
                                                 NULL));

        if (len > offset + 1) {
            extract_other_args(ctx, extracted_values, n->children[offset + 1]);
        }
    }
    else {
        n00b_list_append(extracted_values, n00b_box_bool(true));
    }
    n00b_goption_t *target = cur_option->linked_option;

    if (!target) {
        target = cur_option;
    }

    key = target->result_key;

    n00b_rt_option_t *existing = hatrack_dict_get(ctx->flags, (void *)key, NULL);

    if (!existing) {
        existing = n00b_gc_alloc_mapped(n00b_rt_option_t, N00B_GC_SCAN_ALL);
        hatrack_dict_put(ctx->flags, (void *)key, existing);
        existing->spec = target;
    }

    if (target->max_args == 1) {
        n00b_obj_t val;

        switch (cur_option->type) {
        case N00B_GOAT_BOOL_T_DEFAULT:
        case N00B_GOAT_BOOL_F_DEFAULT:
        case N00B_GOAT_BOOL_T_ALWAYS:
        case N00B_GOAT_BOOL_F_ALWAYS:

            val = n00b_list_get(extracted_values, 0, NULL);

            if (needs_negation(cur_option, target)) {
                boxed_bool_negation((bool *)val);
            }

            existing->value = val;
            existing->n     = 1;
            return;

        case N00B_GOAT_WORD:
        case N00B_GOAT_INT:
        case N00B_GOAT_FLOAT:
            val = n00b_list_get(extracted_values, 0, NULL);

            existing->value = val;
            existing->n     = 1;
            return;

            // Choices should all have more than one value.
        case N00B_GOAT_CHOICE:
        case N00B_GOAT_CHOICE_T_ALIAS:
        case N00B_GOAT_CHOICE_F_ALIAS:
        default:
            n00b_unreachable();
        }
    }

    switch (cur_option->type) {
    case N00B_GOAT_CHOICE_T_ALIAS:
        if (existing->n) {
            n00b_list_t *cur = (n00b_list_t *)existing->value;
            len              = n00b_list_len(cur);
            n00b_obj_t   us  = n00b_list_get(extracted_values, 0, NULL);
            n00b_type_t *t   = n00b_get_my_type(us);

            for (int i = 0; i < len; i++) {
                n00b_obj_t one = n00b_list_get(cur, i, NULL);
                if (n00b_eq(t, us, one)) {
                    // Nothing to do.
                    return;
                }
            }
        }
        break;
    case N00B_GOAT_CHOICE_F_ALIAS:
        if (!existing->n) {
            return;
        }
        n00b_list_t *cur = (n00b_list_t *)existing->value;
        len              = n00b_list_len(cur);
        n00b_obj_t   us  = n00b_list_get(extracted_values, 0, NULL);
        n00b_type_t *t   = n00b_get_my_type(us);
        n00b_list_t *new = n00b_list(t);

        for (int i = 0; i < len; i++) {
            n00b_obj_t one = n00b_list_get(cur, i, NULL);
            if (!n00b_eq(t, us, one)) {
                n00b_list_append(new, one);
            }
        }
        if (n00b_list_len(new) == len) {
            return;
        }

        existing->n      = 0;
        extracted_values = new;
        break;

    case N00B_GOAT_BOOL_T_DEFAULT:
    case N00B_GOAT_BOOL_F_DEFAULT:
    case N00B_GOAT_BOOL_T_ALWAYS:
    case N00B_GOAT_BOOL_F_ALWAYS:

        if (n00b_list_len(extracted_values) == 0) {
            n00b_list_append(extracted_values, n00b_box_bool(true));
        }

        if (needs_negation(cur_option, target)) {
            negate_bool_array(extracted_values);
        }
        break;
    default:
        break;
    }

    n00b_list_t *new_items;

    if (existing->n) {
        new_items = (n00b_list_t *)existing->value;
        new_items = n00b_list_plus(new_items, extracted_values);
    }
    else {
        new_items = extracted_values;
    }

    len = n00b_list_len(new_items);
    if (len > target->max_args && target->max_args) {
        len       = target->max_args;
        new_items = n00b_list_get_slice(new_items, len - target->max_args, len);
    }

    existing->value = new_items;
    existing->n     = len;

    hatrack_dict_put(ctx->flags, (void *)key, existing);
}

static void
extract_args(n00b_gopt_extraction_ctx *ctx)
{
    n00b_tree_node_t  *n   = ctx->intree;
    n00b_parse_node_t *cur = n00b_tree_get_contents(n);

    if (!n00b_parse_get_user_data(ctx->parser->grammar, cur)) {
        for (int i = 0; i < n->num_kids; i++) {
            ctx->intree = n->children[i];
            extract_args(ctx);
        }
        ctx->intree = n;
    }
    else {
        n00b_list_t *arg_info = hatrack_dict_get(ctx->args, ctx->path, NULL);
        n00b_obj_t   obj      = extract_value_from_node(ctx, n, NULL);
        n00b_list_append(arg_info, obj);
    }
}

static void
gopt_extract_spec(n00b_gopt_extraction_ctx *ctx)
{
    n00b_tree_node_t   *n   = ctx->intree;
    n00b_parse_node_t  *cur = n00b_tree_get_contents(n);
    n00b_grammar_t     *g   = ctx->parser->grammar;
    n00b_gopt_node_type t   = (n00b_gopt_node_type)n00b_parse_get_user_data(g,
                                                                          cur);
    int                 len = n->num_kids;

    if (is_group_node(cur)) {
        extract_args(ctx);
        ctx->intree = n;
        return;
    }

    switch (t) {
    case N00B_GTNT_FLOAT_NT:
    case N00B_GTNT_INT_NT:
    case N00B_GTNT_BOOL_NT:
    case N00B_GTNT_WORD_NT:
    case N00B_GTNT_OPT_JUNK_MULTI:
    case N00B_GTNT_OPT_JUNK_SOLO:
    case N00B_GTNT_OPTION_RULE:
        for (int i = 0; i < len; i++) {
            ctx->intree = n->children[i];
            gopt_extract_spec(ctx);
        }
        break;
        //        n00b_unreachable();

    case N00B_GTNT_CMD_NAME:
        handle_subcommand(ctx);
        // fallthrough
    default:
        for (int i = 0; i < len; i++) {
            ctx->intree = n->children[i];
            gopt_extract_spec(ctx);
        }
    }
    ctx->intree = n;
}

static void
gopt_extract_top_spec(n00b_gopt_extraction_ctx *ctx)
{
    n00b_tree_node_t  *n   = ctx->intree;
    n00b_parse_node_t *cur = n00b_tree_get_contents(n);
    int                len = n->num_kids;

    ctx->cur_cmd = hatrack_dict_get(ctx->gctx->top_specs,
                                    (void *)cur->id,
                                    NULL);

    n00b_list_t *args = n00b_list(n00b_type_ref());

    hatrack_dict_put(ctx->args, ctx->path, args);

    assert(ctx->cur_cmd);

    for (int i = 0; i < len; i++) {
        ctx->intree = n->children[i];
        gopt_extract_spec(ctx);
    }
}

static void
extract_individual_flags(n00b_gopt_extraction_ctx *ctx, n00b_tree_node_t *n)
{
    n00b_parse_node_t  *np = n00b_tree_get_contents(n);
    n00b_grammar_t     *g  = ctx->parser->grammar;
    n00b_gopt_node_type tinfo;

    tinfo = (n00b_gopt_node_type)n00b_parse_get_user_data(g, np);

    if (tinfo == N00B_GTNT_OPT_JUNK_SOLO) {
        n00b_list_append(ctx->flag_nodes, n->children[0]);
        handle_option_rule(ctx, n->children[0]);
        return;
    }
    for (int i = 0; i < n->num_kids; i++) {
        extract_individual_flags(ctx, n->children[i]);
    }
}

static void
gopt_extract_flags(n00b_gopt_extraction_ctx *ctx, n00b_tree_node_t *n)
{
    int                 l = n->num_kids;
    n00b_grammar_t     *g = ctx->parser->grammar;
    n00b_gopt_node_type tinfo;

    while (l--) {
        n00b_tree_node_t  *kidt = n->children[l];
        n00b_parse_node_t *kidp = n00b_tree_get_contents(kidt);

        tinfo = (n00b_gopt_node_type)n00b_parse_get_user_data(g, kidp);

        if (tinfo == N00B_GTNT_OPT_JUNK_MULTI) {
            extract_individual_flags(ctx, kidt);

            for (int i = l + 1; i < n->num_kids; i++) {
                n->children[i - 1] = n->children[i];
            }

            n->num_kids--;
        }
        else {
            gopt_extract_flags(ctx, kidt);
        }
    }
}

static inline void
populate_penalty_errors(n00b_gopt_extraction_ctx *ctx)
{
    n00b_tree_node_t   *cur = ctx->intree;
    n00b_parse_node_t  *pn  = n00b_tree_get_contents(cur);
    n00b_grammar_t     *g   = ctx->parser->grammar;
    n00b_gopt_node_type t   = (n00b_gopt_node_type)n00b_parse_get_user_data(g,
                                                                          pn);
    n00b_utf8_t        *msg;
    n00b_tree_node_t   *n;

    if (pn->missing) {
        if (n00b_str_eq(pn->info.name, n00b_new_utf8("'«Unknown Opt»'"))) {
            msg = n00b_new_utf8("Invalid argument.");
        }
        else {
            msg = n00b_cstr_format("Missing an expected [em]{}[/].",
                                   pn->info.name);
        }

        n00b_list_append(ctx->errors, msg);
        return;
    }
    if (pn->bad_prefix) {
        n00b_tree_node_t *n = cur->children[0];

        assert(!n->num_kids);
        n = cur->children[1];
        // assert(!n->num_kids);

        n00b_parse_node_t *bad_kid = n00b_tree_get_contents(cur->children[0]);

        msg = n00b_cstr_format("Got [em]{}[/] when expecting a [em]{}[/].",
                               bad_kid->info.token->value,
                               pn->info.name);
        n00b_list_append(ctx->errors, msg);
        return;
    }
    int i = 0;
    n     = cur;

    switch (t) {
    case N00B_GTNT_BAD_ARGS:
        msg = n00b_cstr_format("Invalid arguments for command.");

        if (ctx->path) {
            msg = n00b_cstr_format("In command [em]{}[/]: {}", ctx->path, msg);
        }

        n00b_list_append(ctx->errors, msg);
        return;
    case N00B_GTNT_CMD_NAME:
        while (n->num_kids) {
            n = n->children[0];
        }
        pn                       = n00b_tree_get_contents(n);
        n00b_gopt_cspec *cmd     = ctx->cur_cmd;
        int64_t          cmd_key = pn->info.token->tid;
        ctx->cur_cmd             = hatrack_dict_get(cmd->sub_commands,
                                        (void *)cmd_key,
                                        NULL);
        if (!ctx->cur_cmd) {
            break;
        }

        n00b_utf8_t *cmd_name = ctx->cur_cmd->name;

        if (n00b_str_codepoint_len(ctx->path)) {
            ctx->path = n00b_cstr_format("{}.{}", ctx->path, cmd_name);
        }
        else {
            ctx->path = cmd_name;
        }
        i = 1;
        break;
    default:
        break;
    }

    for (; i < n->num_kids; i++) {
        ctx->intree = n->children[i];
        populate_penalty_errors(ctx);
    }

    ctx->intree = cur;
}

static inline bool
gopt_structural_error_check(n00b_gopt_extraction_ctx *ctx)
{
    n00b_parse_node_t *top = n00b_tree_get_contents(ctx->intree);

    ctx->cur_cmd = hatrack_dict_get(ctx->gctx->top_specs,
                                    (void *)top->id,
                                    NULL);

    if (top->penalty) {
        n00b_list_append(ctx->errors, n00b_new_utf8("Parse error."));
        populate_penalty_errors(ctx);
        return true;
    }
    return false;
}

static inline void
check_opt_against_commands(n00b_gopt_extraction_ctx *ctx, n00b_rt_option_t *opt)
{
    int n = n00b_list_len(ctx->cmd_stack);

    if (!opt->spec->linked_command->name) {
        return; // Root flag.
    }

    for (int i = 0; i < n; i++) {
        n00b_gopt_cspec *cmd = n00b_list_get(ctx->cmd_stack, i, NULL);
        if (cmd == opt->spec->linked_command) {
            return;
        }
    }

    n00b_utf8_t *err = n00b_cstr_format(
        "Option [em]{}[/] is not valid for this command.",
        opt->spec->name);
    n00b_list_append(ctx->errors, err);
}

static inline void
gopt_do_flag_validation(n00b_gopt_extraction_ctx *ctx)
{
    uint64_t           n;
    n00b_rt_option_t **option_info = (void *)hatrack_dict_values(ctx->flags, &n);

    for (uint64_t i = 0; i < n; i++) {
        check_opt_against_commands(ctx, option_info[i]);
    }
}

static n00b_gopt_result_t *
convert_parse_tree(n00b_gopt_ctx *gctx, n00b_parser_t *p, n00b_tree_node_t *node)
{
    n00b_gopt_extraction_ctx ctx = {
        .errors       = n00b_list(n00b_type_utf8()),
        .cmd_stack    = n00b_list(n00b_type_ref()),
        .flag_nodes   = n00b_list(n00b_type_ref()),
        .flags        = n00b_dict(n00b_type_int(), n00b_type_ref()),
        .args         = n00b_dict(n00b_type_utf8(), n00b_type_ref()),
        .intree       = node,
        .gctx         = gctx,
        .path         = n00b_new_utf8(""),
        .deepest_path = n00b_new_utf8(""),
        .parser       = p,
    };
    n00b_gopt_result_t *result = n00b_gc_alloc_mapped(n00b_gopt_result_t,
                                                      N00B_GC_SCAN_ALL);

    if (gctx->show_debug) {
        n00b_print(n00b_parse_tree_format(ctx.intree));
    }

    if (gopt_structural_error_check(&ctx)) {
        result->errors = ctx.errors;
        return result;
    }

    gopt_extract_flags(&ctx, ctx.intree);
    ctx.intree = node;

    if (gctx->show_debug) {
        for (int i = 0; i < n00b_list_len(ctx.flag_nodes); i++) {
            n00b_grid_t *s;
            n00b_printf("[h4]Flag {}", i + 1);
            s = n00b_parse_tree_format(n00b_list_get(ctx.flag_nodes, i, NULL));
            n00b_print(s);
        }
    }

    hatrack_dict_put(ctx.args, ctx.path, n00b_list(n00b_type_ref()));

    gopt_extract_top_spec(&ctx);
    ctx.intree = node;
    gopt_do_flag_validation(&ctx);

    result->flags  = ctx.flags;
    result->args   = ctx.args;
    result->errors = ctx.errors;
    result->cmd    = ctx.deepest_path;

    return result;
}

static int64_t
fix_pruned_tree(n00b_tree_node_t *t)
{
    n00b_parse_node_t *pn = n00b_tree_get_contents(t);
    // Without resetting the hash on each node, the builtin dedupe
    // won't pick up the tree changes we made (removing Opts nodes)
    pn->hv                = 0;

    if (t->num_kids != 0) {
        for (int i = 0; i < t->num_kids; i++) {
            fix_pruned_tree(t->children[i]);
            n00b_parse_node_t *cur = n00b_tree_get_contents(t->children[i]);

            // We're not actually going to use the token position indicators,
            // so this is the easiest thing to do.
            cur->start = 0;
            cur->end   = 0;
        }
    }
    return pn->hv;
}

n00b_list_t *
n00b_gopt_parse(n00b_gopt_ctx *ctx, n00b_str_t *argv0, n00b_list_t *args)
{
    if (args == NULL && argv0 == NULL) {
        argv0 = n00b_get_argv0();
    }
    if (args == NULL) {
        args = n00b_get_program_arguments();
    }

    n00b_gopt_finalize(ctx);
    n00b_gopt_tokenize(ctx, argv0, args);

    n00b_parser_t *opt_parser = n00b_new(n00b_type_parser(), ctx->grammar);

    n00b_parse_token_list(opt_parser, ctx->tokens, ctx->nt_start);

    n00b_list_t *trees      = n00b_parse_get_parses(opt_parser);
    n00b_list_t *cleaned    = n00b_list(n00b_type_ref());
    n00b_list_t *results    = n00b_list(n00b_type_ref());
    bool         error_free = false;

    for (int i = 0; i < n00b_list_len(trees); i++) {
        n00b_tree_node_t   *t   = n00b_list_get(trees, i, NULL);
        n00b_gopt_result_t *res = convert_parse_tree(ctx, opt_parser, t);

        fix_pruned_tree(t);
        n00b_list_append(cleaned, t);

        int num_trees = n00b_list_len(cleaned);

        if (num_trees > 1) {
            cleaned = n00b_clean_trees(opt_parser, cleaned);
            if (n00b_list_len(cleaned) != num_trees) {
                continue;
            }
        }

        if (!error_free && !n00b_list_len(res->errors)) {
            if (n00b_list_len(results)) {
                results = n00b_list(n00b_type_ref());
            }

            error_free = true;
        }

        n00b_list_append(results, res);
    }

    if (!error_free && n00b_list_len(results) > 1) {
        results->append_ix = 1;
    }

    return results;
}
