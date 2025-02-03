// The API is set up so that you should set things up as so:
//
// 1. Create the getopt context.
// 2. Add your commands, and any rules.
// 3. Add flags, optionally attaching them to particular commands
//    (they're always available below the command until there
//     is a command that passes through everything).
// 4. Finalize the parser (automatically happens if needed when using
//    it).
//
//
// Each flag generates a parse rule just for that flag. Each
// subcommand generate a parse rule for the set of flags specific to
// it, that it might accept.
//
// in-scope flag rules will get dropped in between every RHS item
// that makes sense.
//
//
// Sub-commands that are added should be named in rules, but if you
// don't, we automatically add a rule that takes no argument except
// the subcommand.
//
//
// Let's take `chalk` as an example. The `inspect` subcommand itself
// can take a list of items as arguments, or it can take subcommands
// that themselves may or may not take commands. For instance, you can
// do 'chalk inspect containers`, or `chalk inspect all`, and I think
// you can optionally add a list of containers.  The `chalk inspect`
// command, when passing the grammar, only specifies its arguments and
// any sub-commands.
//
// So essentially, you'd add rules like (in EBNF form):
//
// inspect ::= (ARG)*
// inspect ::= containers
// inspect ::= all
//
// You *can* add arguments before the sub-command.
//
// Note that when you add a subcommand like 'containers' to the
// grammar, it becomes its own token, and is not accepted within the
// current command that has it as a subcommand. As such, it will not
// matche 'ARG' for that sub-command (it will be accepted in other
// parts of the tree that do not have 'command' as a name).  In our
// rule scheme, if you wanted to accept the word 'containers', even
// though it might end up ambiguous, you are currently out of luck.
// I might provide a way to specify an exception in the future, but
// I am trying to keep it simple.
//
// Another artifical limitation is that the sub-command, if any, can
// only appear LAST right now. Sure, it's not unreasonable to want to
// have sub-command rules in the middle of the grammar the user
// provides.
//
// But for the moment, I'm going to air on the side of what I think is
// most inutitive, which is not to treat any of it like the user
// understands BNF.
//
// By the way, in-scope flags will end up grouped based on their
// command and dropped between any symbols in the grammar.
//
// For instance, if we have inspect-specific flags attached to the top
// 'inspect' command, then the 'inspect' rules above will
// translate to something like:
//
// toplevel_flags: ('--someflag', WORD | ... | BAD_FLAG) *
// iflags_1: (toplevel_flags | inspect_flags)*
// inspect_rule_1: iflags_1 'inspect' iflags1 (ARG iflags1)* iflags_1
// iflags_2: (iflags_1 | containers_flags)*
// inspect_rule_2: iflags_2 'inspect' iflags2 containers_rule iflags_2
//
// And so on.
//
// The grammar rules for the flags are added automatically as we add
// flags.
//
// No information on flags whatsoever is accepted on the spec the user
// gives.  Note that the 'finalization' stage is important, because we
// don't actually add grammar productions for COMMAND bodies until
// this stage (even though we do add rules for flags incrementally).

// We wait to add command rules, so that we can look at the whole
// graph and place command flag groupings everywhere
// appropriate. Additionally, any subcommand names can be added into
// all commands that don't have the name as a sub-command, if you set
// N00B_ACCEPT_SUBS_AS_WORDS

#define N00B_USE_INTERNAL_API
#include "n00b.h"

static inline n00b_pitem_t *
get_bi_pitem(n00b_gopt_ctx *ctx, n00b_gopt_bi_ttype ix)
{
    bool found;

    int64_t termid = (int64_t)n00b_list_get(ctx->terminal_info, ix, &found);

    return n00b_pitem_terminal_from_int(ctx->grammar, termid);
}

#define ADD_BI(ctx, name)                                        \
    n00b_list_append(ctx->terminal_info,                         \
                     (void *)n00b_grammar_add_term(ctx->grammar, \
                                                   n00b_new_utf8(name)))

static inline void
setup_unknown_option(n00b_gopt_ctx *ctx)
{
    n00b_list_t  *base   = n00b_list(n00b_type_ref());
    int64_t       termid = (int64_t)n00b_list_get(ctx->terminal_info,
                                            N00B_GOTT_UNKNOWN_OPT,
                                            NULL);
    n00b_pitem_t *pi     = n00b_pitem_terminal_from_int(ctx->grammar, termid);
    n00b_utf8_t  *name   = n00b_new_utf8("$$unknown-option");
    ctx->nt_badopt       = n00b_new(n00b_type_ruleset(), ctx->grammar, name);

    n00b_ruleset_set_user_data(ctx->nt_badopt, (void *)N00B_GTNT_BAD_OPTION);

    n00b_list_append(base, pi);
    n00b_ruleset_add_rule(ctx->grammar, ctx->nt_badopt, base, 0);

    n00b_list_t *rest = n00b_list(n00b_type_ref());
    n00b_list_append(rest, n00b_pitem_from_nt(ctx->nt_word));
    n00b_list_t *group = n00b_list(n00b_type_ref());
    n00b_list_append(group, get_bi_pitem(ctx, N00B_GOTT_COMMA));
    n00b_list_append(group, n00b_pitem_from_nt(ctx->nt_word));
    n00b_pitem_t *gpi = n00b_group_items(ctx->grammar, group, 0, 0);
    n00b_list_append(rest, gpi);
    n00b_list_t *rule = n00b_shallow(base);
    rule              = n00b_list_plus(rule, rest);

    n00b_ruleset_add_rule(ctx->grammar, ctx->nt_badopt, rule, 0);

    rule = n00b_shallow(base);
    n00b_list_append(rule, get_bi_pitem(ctx, N00B_GOTT_ASSIGN));
    rule = n00b_list_plus(rule, rest);
    n00b_ruleset_add_rule(ctx->grammar, ctx->nt_badopt, rule, 0);

    rule = n00b_list(n00b_type_ref());
    n00b_list_append(rule, n00b_pitem_from_nt(ctx->nt_badopt));
    n00b_ruleset_add_rule(ctx->grammar, ctx->nt_1opt, rule, 0);
}

static inline void
setup_bad_args_rule(n00b_gopt_ctx *ctx)
{
    n00b_pitem_t *pi_nt  = n00b_pitem_nonterm_raw(ctx->grammar,
                                                 n00b_new_utf8("$$bad_args"));
    n00b_pitem_t *pi_any = n00b_new_pitem(N00B_P_ANY);

    ctx->nt_badargs                = n00b_pitem_get_ruleset(ctx->grammar, pi_nt);
    ctx->nt_badargs->no_error_rule = true;

    n00b_list_t *l = n00b_list(n00b_type_ref());
    n00b_list_append(l, pi_nt);
    n00b_list_append(l, pi_any);

    n00b_ruleset_add_rule(ctx->grammar, ctx->nt_badargs, l, 10);

    l = n00b_list(n00b_type_ref());
    n00b_list_append(l, pi_any);

    n00b_parse_rule_t *rule = n00b_ruleset_add_rule(ctx->grammar,
                                                    ctx->nt_badargs,
                                                    l,
                                                    10);
    rule->penalty_rule      = true;
    n00b_ruleset_set_user_data(ctx->nt_badargs, (void *)N00B_GTNT_BAD_ARGS);
}

void
n00b_gopt_init(n00b_gopt_ctx *ctx, va_list args)
{
    ctx->options = va_arg(args, uint32_t);
#if 1
    ctx->grammar = n00b_new(n00b_type_grammar(),
                            n00b_kw("detect_errors",
                                    n00b_ka(true),
                                    "max_penalty",
                                    n00b_ka(1)));
#else
    ctx->grammar = n00b_new(n00b_type_grammar());
#endif
    ctx->all_options     = n00b_dict(n00b_type_utf8(), n00b_type_ref());
    ctx->primary_options = n00b_dict(n00b_type_i64(), n00b_type_ref());
    ctx->top_specs       = n00b_dict(n00b_type_i64(), n00b_type_ref());
    ctx->sub_names       = n00b_dict(n00b_type_utf8(), n00b_type_i64());
    ctx->terminal_info   = n00b_list(n00b_type_ref());

    // Add in builtin token types.
    ADD_BI(ctx, "«Assign»");
    ADD_BI(ctx, "«Comma»");
    ADD_BI(ctx, "«Int»");
    ADD_BI(ctx, "«Float»");
    ADD_BI(ctx, "«True»");
    ADD_BI(ctx, "«False»");
    ADD_BI(ctx, "«Word»");
    ADD_BI(ctx, "«Unknown Cmd»");
    ADD_BI(ctx, "«Unknown Opt»");
    ADD_BI(ctx, "«Error»");

    // Add non-terminals and rules we're likely to need.
    // These will be numbered in the order we add them, since they're
    // the first non-terminals added, and the first will be the default
    // start rule.

    n00b_pitem_t *tmp;
    n00b_list_t  *rule;
    n00b_list_t  *items;

    // Start rule.
    tmp           = n00b_pitem_nonterm_raw(ctx->grammar,
                                 n00b_new_utf8("Start"));
    ctx->nt_start = n00b_pitem_get_ruleset(ctx->grammar, tmp);
    // The rule that matches one opt (but does not check their
    // positioning; that is done at the end of parsing).
    tmp           = n00b_pitem_nonterm_raw(ctx->grammar,
                                 n00b_new_utf8("Opt"));
    ctx->nt_1opt  = n00b_pitem_get_ruleset(ctx->grammar, tmp);
    tmp           = n00b_pitem_nonterm_raw(ctx->grammar,
                                 n00b_new_utf8("Opts"));
    ctx->nt_opts  = n00b_pitem_get_ruleset(ctx->grammar, tmp);
    tmp           = n00b_pitem_nonterm_raw(ctx->grammar,
                                 n00b_new_utf8("FlNT"));
    ctx->nt_float = n00b_pitem_get_ruleset(ctx->grammar, tmp);
    tmp           = n00b_pitem_nonterm_raw(ctx->grammar,
                                 n00b_new_utf8("IntNT"));
    ctx->nt_int   = n00b_pitem_get_ruleset(ctx->grammar, tmp);
    tmp           = n00b_pitem_nonterm_raw(ctx->grammar,
                                 n00b_new_utf8("BoolNT"));
    ctx->nt_bool  = n00b_pitem_get_ruleset(ctx->grammar, tmp);
    tmp           = n00b_pitem_nonterm_raw(ctx->grammar,
                                 n00b_new_utf8("WordNT"));
    ctx->nt_word  = n00b_pitem_get_ruleset(ctx->grammar, tmp);
    tmp           = n00b_pitem_nonterm_raw(ctx->grammar,
                                 n00b_new_utf8("EQ"));
    ctx->nt_eq    = n00b_pitem_get_ruleset(ctx->grammar, tmp);

    n00b_ruleset_set_user_data(ctx->nt_1opt, (void *)N00B_GTNT_OPT_JUNK_SOLO);
    n00b_ruleset_set_user_data(ctx->nt_opts, (void *)N00B_GTNT_OPT_JUNK_MULTI);
    n00b_ruleset_set_user_data(ctx->nt_float, (void *)N00B_GTNT_FLOAT_NT);
    n00b_ruleset_set_user_data(ctx->nt_int, (void *)N00B_GTNT_INT_NT);
    n00b_ruleset_set_user_data(ctx->nt_bool, (void *)N00B_GTNT_BOOL_NT);
    n00b_ruleset_set_user_data(ctx->nt_word, (void *)N00B_GTNT_WORD_NT);

    // Opts matches any number of consecutive options. Done w/ a group
    // for simplicity.
    rule  = n00b_list(n00b_type_ref());
    items = n00b_list(n00b_type_ref());
    n00b_list_append(items, n00b_pitem_from_nt(ctx->nt_1opt));
    n00b_list_append(rule, n00b_group_items(ctx->grammar, items, 0, 0));
    n00b_ruleset_add_rule(ctx->grammar, ctx->nt_opts, rule, 0);

    // Args that take floats may accept ints.
    items = n00b_list(n00b_type_ref());
    rule  = n00b_list(n00b_type_ref());
    n00b_list_append(items, get_bi_pitem(ctx, N00B_GOTT_FLOAT));
    n00b_list_append(items, get_bi_pitem(ctx, N00B_GOTT_INT));
    n00b_list_append(rule, n00b_pitem_choice_raw(ctx->grammar, items));
    n00b_ruleset_add_rule(ctx->grammar, ctx->nt_float, rule, 0);

    rule = n00b_list(n00b_type_ref());
    n00b_list_append(rule, get_bi_pitem(ctx, N00B_GOTT_INT));
    n00b_ruleset_add_rule(ctx->grammar, ctx->nt_int, rule, 0);

    items = n00b_list(n00b_type_ref());
    rule  = n00b_list(n00b_type_ref());
    n00b_list_append(items, get_bi_pitem(ctx, N00B_GOTT_BOOL_T));
    n00b_list_append(items, get_bi_pitem(ctx, N00B_GOTT_BOOL_F));
    n00b_list_append(rule, n00b_pitem_choice_raw(ctx->grammar, items));
    n00b_ruleset_add_rule(ctx->grammar, ctx->nt_bool, rule, 0);
    rule = n00b_list(n00b_type_ref());
    n00b_list_append(rule, n00b_pitem_choice_raw(ctx->grammar, items));
    n00b_ruleset_add_rule(ctx->grammar, ctx->nt_bool, rule, 0);

    // Args that take words can accept int, float or bool,
    // for better or worse.
    //
    // Note that if a parse is ambiguous, we probably should prefer
    // the more specific type. This is not done yet.
    items = n00b_list(n00b_type_ref());
    rule  = n00b_list(n00b_type_ref());
    n00b_list_append(items, get_bi_pitem(ctx, N00B_GOTT_FLOAT));
    n00b_list_append(items, get_bi_pitem(ctx, N00B_GOTT_INT));
    n00b_list_append(items, get_bi_pitem(ctx, N00B_GOTT_BOOL_T));
    n00b_list_append(items, get_bi_pitem(ctx, N00B_GOTT_BOOL_F));
    n00b_list_append(items, get_bi_pitem(ctx, N00B_GOTT_WORD));
    n00b_list_append(rule, n00b_pitem_choice_raw(ctx->grammar, items));
    n00b_ruleset_add_rule(ctx->grammar, ctx->nt_word, rule, 0);

    /*
    // We won't set up comma-separated lists of items unless we find a
    // flag than needs them.
    //
    // But we *will* set up the assign non-terminal, where the
    // assign can be omitted, unless the flag's argument is optional and
    // N00B_GOPT_ALWAYS_REQUIRE_EQ is set.
    //
    // The 'o' version is the 'other' version, which is used if the
    // above flag is set, and also in some error productions.
    //
    // Basically we're setting up:
    // EQ  ::= ('=')?
    rule  = n00b_list(n00b_type_ref());
    items = n00b_list(n00b_type_ref());
    n00b_list_append(items, get_bi_pitem(ctx, N00B_GOTT_ASSIGN));
    n00b_list_append(rule, n00b_group_items(ctx->grammar, items, 0, 1));
    n00b_ruleset_add_rule(ctx->grammar, ctx->nt_eq, rule, 0);
    */

    setup_unknown_option(ctx);
    setup_bad_args_rule(ctx);
}

void
n00b_gcommand_init(n00b_gopt_cspec *cmd_spec, va_list args)
{
    n00b_utf8_t     *name             = NULL;
    n00b_list_t     *aliases          = NULL;
    n00b_gopt_ctx   *context          = NULL;
    n00b_utf8_t     *short_doc        = NULL;
    n00b_utf8_t     *long_doc         = NULL;
    n00b_gopt_cspec *parent           = NULL;
    bool             bad_opt_passthru = false;
    bool             top_level        = false;

    n00b_karg_va_init(args);
    n00b_kw_ptr("name", name);
    n00b_kw_ptr("context", context);
    n00b_kw_ptr("aliases", aliases);
    n00b_kw_ptr("short_doc", short_doc);
    n00b_kw_ptr("long_doc", long_doc);
    n00b_kw_ptr("parent", parent);
    n00b_kw_bool("bad_opt_passthru", bad_opt_passthru);

    if (!context) {
        N00B_CRAISE(
            "Must provide a getopt context in the 'context' field");
    }

    if (name) {
        cmd_spec->token_id = n00b_grammar_add_term(context->grammar,
                                                   name);
    }
    else {
        if (parent) {
            N00B_CRAISE(
                "getopt commands that are not the top-level command "
                "must have the 'name' field set.");
        }
        if (context->default_command != 0) {
            N00B_CRAISE(
                "There is already a default command set; new "
                "top-level commands require a name.");
        }
        if (aliases != NULL) {
            N00B_CRAISE("Cannot alias an unnamed (default) command.");
        }
        cmd_spec->token_id = 0;
        top_level          = true;
    }

    if (!parent) {
        if (!hatrack_dict_add(context->top_specs,
                              (void *)cmd_spec->token_id,
                              cmd_spec)) {
dupe_error:
            N00B_CRAISE(
                "Attempt to add two commands at the same "
                "level with the same name.");
        }
    }
    else {
        if (!hatrack_dict_add(parent->sub_commands,
                              (void *)cmd_spec->token_id,
                              cmd_spec)) {
            goto dupe_error;
        }
    }

    cmd_spec->context          = context;
    cmd_spec->name             = name;
    cmd_spec->short_doc        = short_doc;
    cmd_spec->long_doc         = long_doc;
    cmd_spec->sub_commands     = n00b_dict(n00b_type_int(),
                                       n00b_type_ref());
    cmd_spec->aliases          = aliases;
    cmd_spec->bad_opt_passthru = bad_opt_passthru;
    cmd_spec->parent           = parent;
    cmd_spec->owned_opts       = n00b_dict(n00b_type_utf8(), n00b_type_ref());

    // Add a non-terminal rule to the grammar for any rules associated
    // with our command, and one for any flags.
    if (top_level) {
        name = n00b_new_utf8("$gopt");
    }

    // To support the same sub-command appearing multiple places in
    // the grammar we generate, add a rule counter to
    // the non-terminal name.
    n00b_utf8_t *nt_name = n00b_cstr_format("{}_{}",
                                            name,
                                            context->counter++);

    // Add a map from the name to the token ID if needed.
    hatrack_dict_add(context->sub_names,
                     name,
                     (void *)cmd_spec->token_id);

    // 'name_nt' is used to handle any aliases for this command.  Rules
    // get added to 'rule_nt'; 'name_nt' will be added as the first
    // item to any such rule.
    cmd_spec->rule_nt = n00b_new(n00b_type_ruleset(), context->grammar, nt_name);

    // For the main name and any aliases, We create a rule that matches
    // all our options (name_nt). The other rule is to match all of
    // the versions of the subcommand given.

    if (!top_level) {
        nt_name           = n00b_cstr_format("{}_name", nt_name);
        cmd_spec->name_nt = n00b_new(n00b_type_ruleset(),
                                     context->grammar,
                                     nt_name);
        n00b_ruleset_set_user_data(cmd_spec->name_nt, (void *)N00B_GTNT_CMD_NAME);
    }

    n00b_ruleset_set_user_data(cmd_spec->rule_nt, (void *)N00B_GTNT_CMD_RULE);

    n00b_list_t *rule = n00b_list(n00b_type_ref());

    if (top_level) {
        n00b_list_append(rule, n00b_pitem_from_nt(context->nt_badargs));
        n00b_ruleset_add_rule(context->grammar, cmd_spec->rule_nt, rule, 10);
        return;
    }

    n00b_list_t *items = n00b_list(n00b_type_ref());

    if (cmd_spec->token_id >= N00B_START_TOK_ID) {
        n00b_list_append(items,
                         n00b_pitem_terminal_from_int(cmd_spec->context->grammar,
                                                      cmd_spec->token_id));
    }
    else {
        n00b_list_append(rule, n00b_pitem_from_nt(cmd_spec->rule_nt));
        n00b_ruleset_add_rule(context->grammar, cmd_spec->name_nt, rule, 0);
        return;
    }

    if (aliases != NULL) {
        int     n = n00b_list_len(aliases);
        int64_t id;

        for (int i = 0; i < n; i++) {
            n00b_str_t *one = n00b_to_utf8(n00b_list_get(aliases, i, NULL));
            id              = n00b_grammar_add_term(context->grammar, one);
            n00b_list_append(items,
                             n00b_pitem_terminal_from_int(context->grammar, id));
        }
    }
    n00b_list_append(rule, n00b_pitem_choice_raw(context->grammar, items));
    n00b_ruleset_add_rule(context->grammar, cmd_spec->name_nt, rule, 0);

    rule  = n00b_list(n00b_type_ref());
    items = n00b_list(n00b_type_ref());

    n00b_list_append(rule, n00b_pitem_from_nt(cmd_spec->name_nt));
    n00b_list_append(rule, n00b_pitem_from_nt(context->nt_badargs));

    n00b_ruleset_add_rule(context->grammar, cmd_spec->rule_nt, rule, 10);
}

typedef struct gopt_rgen_ctx {
    n00b_list_t     *inrule;
    n00b_list_t     *outrule;
    n00b_gopt_cspec *cmd;
    int              inrule_index;
    int              inrule_len;
    int              nesting;
    n00b_gopt_ctx   *gctx;
} gopt_rgen_ctx;

static inline void
opts_pitem(gopt_rgen_ctx *ctx, n00b_list_t *out)
{
    if (true) {
        n00b_pitem_t *pi = n00b_pitem_from_nt(ctx->gctx->nt_opts);
        n00b_list_append(out, pi);
    }
}

static n00b_pitem_t *
opts_raw(gopt_rgen_ctx *ctx)
{
    n00b_list_t *items = n00b_list(n00b_type_ref());
    n00b_list_append(items, n00b_pitem_any_terminal_raw(ctx->gctx->grammar));
    return n00b_group_items(ctx->gctx->grammar, items, 0, 0);
}

static void
translate_gopt_rule(gopt_rgen_ctx *ctx)
{
    n00b_list_t *out = ctx->outrule;

    while (ctx->inrule_index < ctx->inrule_len) {
        int64_t item = (int64_t)n00b_list_get(ctx->inrule,
                                              ctx->inrule_index++,
                                              NULL);
        switch (item) {
        case N00B_GOG_WORD:
            n00b_list_append(out, n00b_pitem_from_nt(ctx->gctx->nt_word));
            // opts_pitem(ctx, out);
            continue;
        case N00B_GOG_INT:
            n00b_list_append(out, n00b_pitem_from_nt(ctx->gctx->nt_int));
            //            opts_pitem(ctx, out);
            continue;
        case N00B_GOG_FLOAT:
            n00b_list_append(out, n00b_pitem_from_nt(ctx->gctx->nt_float));
            //            opts_pitem(ctx, out);
            continue;
        case N00B_GOG_BOOL:
            n00b_list_append(out, n00b_pitem_from_nt(ctx->gctx->nt_bool));
            //            opts_pitem(ctx, out);
            continue;
        case N00B_GOG_RAW:
            n00b_list_append(out, opts_raw(ctx));
            if (ctx->inrule_index != ctx->inrule_len) {
                N00B_CRAISE("RAW must be the last item specified.");
            }
            return;
        case N00B_GOG_LPAREN:;
            n00b_list_t *out_rule = ctx->outrule;
            ctx->outrule          = n00b_list(n00b_type_ref());
            int old_nest          = ctx->nesting++;

            //            opts_pitem(ctx, out);

            translate_gopt_rule(ctx);

            if (ctx->nesting != old_nest) {
                N00B_CRAISE("Left paren is unmatched.");
            }
            n00b_list_t  *group = ctx->outrule;
            n00b_pitem_t *g;
            ctx->outrule = out_rule;
            // Check the postfix; these are defaults if none.
            int imin     = 1;
            int imax     = 1;
            if (ctx->inrule_index != ctx->inrule_len) {
                item = (int64_t)n00b_list_get(ctx->inrule,
                                              ctx->inrule_index,
                                              NULL);
                switch (item) {
                case N00B_GOG_OPT:
                    imin = 0;
                    ctx->inrule_index++;
                    break;
                case N00B_GOG_STAR:
                    imin = 0;
                    imax = 0;
                    ctx->inrule_index++;
                    break;
                case N00B_GOG_PLUS:
                    imax = 0;
                    ctx->inrule_index++;
                    break;
                default:
                    break;
                }
            }

            g = n00b_group_items(ctx->gctx->grammar, group, imin, imax);

            n00b_list_append(ctx->outrule, g);
            break;
        case N00B_GOG_RPAREN:
            if (--ctx->nesting < 0) {
                N00B_CRAISE("Right paren without matching left paren.");
            }
            return;
        case N00B_GOG_OPT:
        case N00B_GOG_STAR:
        case N00B_GOG_PLUS:
            N00B_CRAISE("Operator may only appear after a right paren.");
        default:;

            n00b_gopt_cspec *sub;

            sub = hatrack_dict_get(ctx->cmd->sub_commands,
                                   (void *)item,
                                   NULL);

            if (!sub) {
                N00B_CRAISE(
                    "Token is not a primitive, and is not a "
                    "sub-command name that's been registed for this command.");
            }
            if (ctx->inrule_index != ctx->inrule_len) {
                N00B_CRAISE("Subcommand must be the last item in a rule.");
            }

            sub->explicit_parent_rule = true;
            n00b_list_append(out, n00b_pitem_from_nt(sub->rule_nt));
            return;
        }
    }
    opts_pitem(ctx, out);
}

n00b_list_t *
_n00b_gopt_rule(int64_t start, ...)
{
    int64_t      next;
    n00b_list_t *result = n00b_list(n00b_type_int());
    va_list      vargs;

    n00b_list_append(result, (void *)start);
    va_start(vargs, start);

    while (true) {
        next = va_arg(vargs, int64_t);
        if (!next) {
            break;
        }
        n00b_list_append(result, (void *)next);
    }
    va_end(vargs);
    return result;
}

static inline n00b_utf8_t *
create_summary_doc(n00b_gopt_ctx *ctx, n00b_gopt_cspec *cmd, n00b_list_t *items)
{
    int n = n00b_list_len(items);

    if (!n) {
        return n00b_new_utf8("");
    }

    n00b_utf8_t *s               = n00b_new_utf8("");
    bool         words_add_space = false;

    for (int i = 0; i < n; i++) {
        int64_t x = (int64_t)n00b_list_get(items, i, NULL);

        switch (x) {
        case (int64_t)N00B_GOG_WORD:
            if (words_add_space) {
                s = n00b_cstr_format("{} str", s);
            }
            else {
                s = n00b_cstr_format("{}str", s);
            }
            words_add_space = true;
            break;
        case (int64_t)N00B_GOG_INT:
            if (words_add_space) {
                s = n00b_cstr_format("{} int", s);
            }
            else {
                s = n00b_cstr_format("{}int", s);
            }
            words_add_space = true;
            break;
        case (int64_t)N00B_GOG_FLOAT:
            if (words_add_space) {
                s = n00b_cstr_format("{} float", s);
            }
            else {
                s = n00b_cstr_format("{}float", s);
            }
            words_add_space = true;
            break;
        case (int64_t)N00B_GOG_BOOL:
            switch (ctx->options
                    & (N00B_GOPT_BOOL_NO_TF | N00B_GOPT_BOOL_NO_YN)) {
            case (N00B_GOPT_BOOL_NO_TF | N00B_GOPT_BOOL_NO_YN):
                N00B_CRAISE("Invalid bool argument (bools disabled)");
            case N00B_GOPT_BOOL_NO_TF:
                if (words_add_space) {
                    s = n00b_cstr_format("{} [y|n]", s);
                }
                else {
                    s = n00b_cstr_format("{}[y|n]", s);
                }
                break;
            default:
                if (words_add_space) {
                    s = n00b_cstr_format("{} [true|false]", s);
                }
                else {
                    s = n00b_cstr_format("{}[true|false]", s);
                }
            }

            words_add_space = true;
            break;
        case (int64_t)N00B_GOG_RAW:
            if (words_add_space) {
                return n00b_cstr_format("{} str*", s);
            }
        case (int64_t)N00B_GOG_LPAREN:
            if (words_add_space) {
                s = n00b_cstr_format("{} (", s);
            }
            else {
                s = n00b_cstr_format("{}(", s);
            }

            words_add_space = false;
            break;
        case (int64_t)N00B_GOG_RPAREN:
            s               = n00b_cstr_format("{})", s);
            words_add_space = true;
            break;
        case (int64_t)N00B_GOG_OPT:
            s               = n00b_cstr_format("{}?", s);
            words_add_space = true;
            break;
        case (int64_t)N00B_GOG_STAR:
            s               = n00b_cstr_format("{}*", s);
            words_add_space = true;
            break;
        case (int64_t)N00B_GOG_PLUS:
            s               = n00b_cstr_format("{}+", s);
            words_add_space = true;
            break;
        default:;
            n00b_gopt_cspec *sub;

            sub = hatrack_dict_get(cmd->sub_commands, (void *)x, NULL);

            if (!sub) {
                N00B_CRAISE(
                    "Token is not a primitive, and is not a "
                    "sub-command name that's been registed for this command.");
            }
            return n00b_cstr_format("{} [em]{}[/]", sub->name, s);
        }
    }

    return s;
}

void
_n00b_gopt_command_add_rule(n00b_gopt_ctx   *gctx,
                            n00b_gopt_cspec *cmd,
                            n00b_list_t     *items,
                            n00b_utf8_t     *doc)
{
    // The only items allowed are the values in n00b_gopt_grammar_consts
    // and the specific token id for any command that is actually
    // one of our sub-commands (though, that can only be in the
    // very last slot).
    int          n;
    n00b_list_t *rule = n00b_list(n00b_type_ref());

    if (!items || (n = n00b_len(items)) == 0) {
        n00b_list_append(rule, n00b_pitem_from_nt(cmd->context->nt_opts));

        if (cmd->name) {
            n00b_list_append(rule, n00b_pitem_from_nt(cmd->name_nt));
            n00b_list_append(rule, n00b_pitem_from_nt(cmd->context->nt_opts));
        }

        _n00b_ruleset_add_rule(cmd->context->grammar,
                               cmd->rule_nt,
                               rule,
                               0,
                               doc);
        return;
    }

    gopt_rgen_ctx ctx = {
        .inrule       = items,
        .outrule      = rule,
        .cmd          = cmd,
        .inrule_index = 0,
        .inrule_len   = n,
        .nesting      = 0,
        .gctx         = cmd->context,

    };

    rule = n00b_list(n00b_type_ref());

    if (ctx.cmd->name) {
        n00b_list_append(rule, n00b_pitem_from_nt(ctx.cmd->name_nt));
        n00b_list_append(rule, n00b_pitem_from_nt(cmd->context->nt_opts));
    }

    else {
        n00b_list_append(rule, n00b_pitem_from_nt(cmd->context->nt_opts));
    }

    translate_gopt_rule(&ctx);
    rule                  = n00b_list_plus(rule, ctx.outrule);
    n00b_parse_rule_t *pr = _n00b_ruleset_add_rule(cmd->context->grammar,
                                                   cmd->rule_nt,
                                                   rule,
                                                   0,
                                                   doc);
    pr->short_doc         = create_summary_doc(gctx, cmd, items);
}

static inline bool
needs_a_rule(n00b_gopt_cspec *cmd)
{
    if (n00b_list_len(cmd->rule_nt->rules) > 1) {
        return false;
    }

    if (!cmd->sub_commands) {
        return true;
    }

    if (!hatrack_dict_len(cmd->sub_commands)) {
        return true;
    }

    return false;
}

static void
add_help_commands(n00b_gopt_ctx *gctx, n00b_gopt_cspec *spec)
{
    n00b_utf8_t *helpstr = n00b_new_utf8("help");

    if (n00b_str_eq(spec->name, helpstr)) {
        return;
    }

    n00b_list_t *subs = n00b_dict_values(spec->sub_commands);

    for (int i = 0; i < n00b_list_len(subs); i++) {
        add_help_commands(gctx, n00b_list_get(subs, i, NULL));
    }

    if (!n00b_dict_contains(spec->sub_commands, helpstr)) {
        n00b_new(n00b_type_gopt_command(),
                 n00b_kw("context",
                         gctx,
                         "name",
                         helpstr,
                         "parent",
                         spec));
        n00b_gopt_add_subcommand(gctx, spec, n00b_new_utf8("(STR)* help"));
    }
}

static void
add_gopt_auto_help(n00b_gopt_ctx *gctx)
{
    if (!n00b_dict_contains(gctx->all_options, n00b_new_utf8("help"))) {
        n00b_new(n00b_type_gopt_option(),
                 n00b_kw("name",
                         n00b_new_utf8("help"),
                         "opt_type",
                         N00B_GOAT_BOOL_T_ALWAYS,
                         "short_doc",
                         n00b_new_utf8("Output detailed help.")));
    }
    n00b_list_t *subs = n00b_dict_values(gctx->top_specs);

    for (int i = 0; i < n00b_list_len(subs); i++) {
        add_help_commands(gctx, n00b_list_get(subs, i, NULL));
    }
}

void
n00b_gopt_finalize(n00b_gopt_ctx *gctx)
{
    if (gctx->finalized) {
        return;
    }

    // This sets up the productions of the 'start' rule and makes sure
    // all sub-commands are properly linked and have some sub-rule.
    //
    // When people add sub-commands, we assume that, if they didn't add
    // a rule to the parent, then the parent should take no
    // arguments of its own when the `sub-command is used. Here, we
    // detect those cases and add rules at the 11th hour.
    //
    // This is breadth first.
    n00b_list_t      *stack = n00b_list(n00b_type_ref());
    uint64_t          n;
    n00b_gopt_cspec **tops = (void *)hatrack_dict_values(gctx->top_specs, &n);
    if (!n) {
        N00B_CRAISE("No commands added to the getopt environment.");
    }

    for (uint64_t i = 0; i < n; i++) {
        n00b_list_t *rule = n00b_list(n00b_type_ref());
        n00b_list_append(rule, n00b_pitem_from_nt(tops[i]->rule_nt));
        n00b_ruleset_add_rule(gctx->grammar, gctx->nt_start, rule, 0);
        n00b_list_append(stack, tops[i]);
    }

    while (n00b_len(stack) != 0) {
        n00b_gopt_cspec *one = n00b_list_pop(stack);
        n00b_assert(one->context == gctx);

        if (needs_a_rule(one)) {
            n00b_gopt_command_add_rule(gctx, one, NULL);
            continue;
        }

        tops = (void *)hatrack_dict_values(one->sub_commands,
                                           &n);
        if (!n) {
            continue;
        }
        for (uint64_t i = 0; i < n; i++) {
            if (!tops[i]->explicit_parent_rule) {
                n00b_list_t *items = n00b_list(n00b_type_int());
                n00b_list_append(items, (void *)tops[i]->token_id);
                n00b_gopt_command_add_rule(gctx, one, items);
            }

            n00b_list_append(stack, tops[i]);
        }
    }

    if (!(gctx->options & N00B_NO_ADD_AUTO_HELP)) {
        // add_gopt_auto_help(gctx);
    }

    gctx->finalized = true;
}

static inline n00b_pitem_t *
flag_pitem(n00b_gopt_ctx *ctx, n00b_goption_t *option)
{
    return n00b_pitem_terminal_from_int(ctx->grammar, option->token_id);
}

static inline n00b_pitem_t *
from_nt(n00b_nonterm_t *nt)
{
    return n00b_pitem_from_nt(nt);
}

static void
add_noarg_rules(n00b_gopt_ctx *ctx, n00b_goption_t *option)
{
    // We will add a dummy rule that looks for a '=' WORD after; if
    // it matches, then we know there was an error.
    n00b_list_t  *good_rule = n00b_list(n00b_type_ref());
    n00b_pitem_t *pi        = flag_pitem(ctx, option);

    n00b_list_append(good_rule, pi);
    n00b_ruleset_add_rule(ctx->grammar, option->my_nonterm, good_rule, 0);

#if 0
    n00b_list_t *bad_rule = n00b_list(n00b_type_ref());

    n00b_list_append(bad_rule, pi);

    n00b_list_append(bad_rule, from_nt(ctx->nt_eq));
    n00b_list_append(bad_rule, from_nt(ctx->nt_word));
    n00b_ruleset_add_rule(ctx->grammar, option->my_nonterm, bad_rule, 0);
#endif
}

static void
base_add_rules(n00b_gopt_ctx *ctx, n00b_goption_t *option, n00b_nonterm_t *type)
{
    n00b_list_t  *base = n00b_list(n00b_type_ref());
    n00b_pitem_t *pi   = flag_pitem(ctx, option);

    n00b_list_append(base, pi);

    if (option->min_args == 0) {
        n00b_ruleset_add_rule(ctx->grammar, option->my_nonterm, base, 0);
        base = n00b_shallow(base);
    }

    n00b_list_t *with_eq = n00b_shallow(base);
    n00b_list_append(with_eq, get_bi_pitem(ctx, N00B_GOTT_ASSIGN));
    n00b_list_append(with_eq, from_nt(type));
    n00b_list_append(base, from_nt(type));

    if (option->max_args != 1) {
        n00b_list_t *group = n00b_list(n00b_type_ref());
        n00b_list_append(group, get_bi_pitem(ctx, N00B_GOTT_COMMA));
        n00b_list_append(group, from_nt(type));
        n00b_pitem_t *gpi = n00b_group_items(ctx->grammar,
                                             group,
                                             option->min_args,
                                             option->max_args);
        n00b_list_append(with_eq, gpi);
        n00b_list_append(base, gpi);
    }

    if (!(ctx->options & N00B_GOPT_ALWAYS_REQUIRE_EQ)) {
        n00b_ruleset_add_rule(ctx->grammar, option->my_nonterm, base, 0);
    }
    n00b_ruleset_add_rule(ctx->grammar, option->my_nonterm, with_eq, 0);
}

static void
add_bool_rules(n00b_gopt_ctx *ctx, n00b_goption_t *option)
{
    base_add_rules(ctx, option, ctx->nt_bool);
}

static void
add_int_rules(n00b_gopt_ctx *ctx, n00b_goption_t *option)
{
    base_add_rules(ctx, option, ctx->nt_int);
}

static void
add_float_rules(n00b_gopt_ctx *ctx, n00b_goption_t *option)
{
    base_add_rules(ctx, option, ctx->nt_float);
}

static void
add_word_rules(n00b_gopt_ctx *ctx, n00b_goption_t *option)
{
    base_add_rules(ctx, option, ctx->nt_word);
}

void
n00b_goption_init(n00b_goption_t *option, va_list args)
{
    n00b_utf8_t     *name           = NULL;
    n00b_gopt_ctx   *context        = NULL;
    n00b_utf8_t     *short_doc      = NULL;
    n00b_utf8_t     *long_doc       = NULL;
    n00b_list_t     *choices        = NULL;
    n00b_gopt_cspec *linked_command = NULL;
    int64_t          key            = 0;
    int32_t          opt_type       = N00B_GOAT_NONE;
    int32_t          min_args       = 1;
    int32_t          max_args       = 1;

    n00b_karg_va_init(args);
    n00b_kw_ptr("name", name);
    n00b_kw_ptr("context", context);
    n00b_kw_ptr("short_doc", short_doc);
    n00b_kw_ptr("long_doc", short_doc);
    n00b_kw_ptr("choices", choices);
    n00b_kw_ptr("linked_command", linked_command);
    n00b_kw_int64("key", key);
    n00b_kw_int32("opt_type", opt_type);
    n00b_kw_int32("min_args", min_args);
    n00b_kw_int32("max_args", max_args);

    if (!context && linked_command) {
        context = linked_command->context;
    }

    if (!context) {
        N00B_CRAISE(
            "Must provide a getopt context in the 'context' field, "
            "or linked_command to a command that has an associated context.");
    }

    if (!name) {
        N00B_CRAISE("getopt options must have the 'name' field set.");
    }
    // clang-format off
    if (n00b_str_find(name, n00b_new_utf8("=")) != -1 ||
	n00b_str_find(name, n00b_new_utf8(",")) != -1) {
        N00B_CRAISE("Options may not contain '=' or ',' in the name.");
    }
    // clang-format on

    n00b_goption_t *linked_option = NULL;

    switch (opt_type) {
    case N00B_GOAT_BOOL_T_DEFAULT:
    case N00B_GOAT_BOOL_F_DEFAULT:
        min_args = 0;
        max_args = 1;
        break;
    case N00B_GOAT_BOOL_T_ALWAYS:
    case N00B_GOAT_BOOL_F_ALWAYS:
        min_args = 0;
        max_args = 0;
        break;
    case N00B_GOAT_WORD:
    case N00B_GOAT_INT:
    case N00B_GOAT_FLOAT:
    case N00B_GOAT_WORD_ALIAS:
    case N00B_GOAT_INT_ALIAS:
    case N00B_GOAT_FLOAT_ALIAS:
        break;
    case N00B_GOAT_CHOICE: // User-supplied list of options
        if (!choices || n00b_list_len(choices) <= 1) {
            N00B_CRAISE(
                "Choice alias options cannot be the primary value"
                "for a key; they must link to a pre-existing"
                "choice field.");
        }
        break;
    case N00B_GOAT_CHOICE_T_ALIAS:
    case N00B_GOAT_CHOICE_F_ALIAS:
        if (!key) {
            N00B_CRAISE("Must provide a linked option.");
        }
        linked_option = hatrack_dict_get(context->primary_options,
                                         (void *)key,
                                         NULL);

        goto check_link;
        break;
    default:
        N00B_CRAISE(
            "Must provide a valid option type in the "
            "'opt_type' field.");
    }

    if (key && !linked_option) {
        linked_option = hatrack_dict_get(context->primary_options,
                                         (void *)key,
                                         NULL);
check_link:
        if (!linked_option) {
            N00B_CRAISE("Linked key for choice alias must already exist.");
        }
    }

    if (!linked_option) {
        option->primary = true;
        if (!key) {
            key = n00b_rand64();
        }
    }
    else {
        if (!linked_option->links_inbound) {
            linked_option->links_inbound = n00b_list(n00b_type_ref());
        }
        n00b_list_append(linked_option->links_inbound, option);

        if (!linked_option->primary) {
            N00B_CRAISE(
                "Linked option is not the primary option for the "
                "key provided; you must link to the primary option.");
        }
    }

    if (!linked_command) {
        linked_command = hatrack_dict_get(context->primary_options,
                                          (void *)context->default_command,
                                          NULL);
    }

    if (linked_option) {
        bool compat = false;

        if (linked_option->linked_command != linked_command) {
            N00B_CRAISE("Linked options must be part of the same command.");
        }

        switch (linked_option->type) {
        case N00B_GOAT_BOOL_T_DEFAULT:
        case N00B_GOAT_BOOL_T_ALWAYS:
        case N00B_GOAT_BOOL_F_ALWAYS:
            switch (opt_type) {
            case N00B_GOAT_BOOL_T_DEFAULT:
            case N00B_GOAT_BOOL_T_ALWAYS:
            case N00B_GOAT_BOOL_F_ALWAYS:
                compat = true;
                break;
            }
            break;
        case N00B_GOAT_WORD:
            if (opt_type == N00B_GOAT_WORD || opt_type == N00B_GOAT_WORD_ALIAS) {
                compat = true;
                break;
            }
            // fallthrough
        case N00B_GOAT_INT:
            if (opt_type == N00B_GOAT_INT || opt_type == N00B_GOAT_INT_ALIAS) {
                compat = true;
                break;
            }
            // fallthrough
        case N00B_GOAT_FLOAT:
            if (opt_type == N00B_GOAT_FLOAT || opt_type == N00B_GOAT_FLOAT_ALIAS) {
                compat = true;
            }
            break;
        case N00B_GOAT_CHOICE:
            switch (opt_type) {
            case N00B_GOAT_CHOICE:
                if (choices != NULL) {
                    N00B_CRAISE(
                        "Choice fields that alias another choice "
                        "field currently are not allowed to provide their "
                        "own options.");
                }
                compat = true;
                break;
            case N00B_GOAT_CHOICE_T_ALIAS:
            case N00B_GOAT_CHOICE_F_ALIAS:
                compat = true;
                break;
            default:
                break;
            }
        default:
            n00b_unreachable();
        }
        if (!compat) {
            N00B_CRAISE(
                "Type of linked option is not compatible with "
                "the declared type of the current option.");
        }
    }
    option->name           = n00b_to_utf8(name);
    option->short_doc      = short_doc;
    option->long_doc       = long_doc;
    option->linked_option  = linked_option;
    option->result_key     = key;
    option->min_args       = min_args;
    option->max_args       = max_args;
    option->type           = opt_type;
    option->choices        = choices;
    option->linked_command = linked_command;

    hatrack_dict_put(linked_command->owned_opts, option->name, option);

    if (n00b_gctx_gflag_is_set(context, N00B_CASE_SENSITIVE)) {
        option->normalized = option->name;
    }
    else {
        option->normalized = n00b_to_utf8(n00b_str_lower(name));
    }

    option->token_id = n00b_grammar_add_term(context->grammar,
                                             option->normalized);

    if (!hatrack_dict_add(context->all_options,
                          (void *)option->normalized,
                          option)) {
        N00B_CRAISE("Cannot add option that has already been added.");
    }

    if (!link) {
        hatrack_dict_put(context->primary_options,
                         (void *)key,
                         option);
    }

    uint64_t     rand    = n00b_rand16();
    n00b_utf8_t *nt_name = n00b_cstr_format("opt_{}_{}",
                                            option->name,
                                            rand);
    option->my_nonterm   = n00b_new(n00b_type_ruleset(),
                                  context->grammar,
                                  nt_name);

    n00b_ruleset_set_user_data(option->my_nonterm, (void *)N00B_GTNT_OPTION_RULE);
    // When flags are required to have arguments, we will add a rule
    // for a null argument, knowing if the final parse matches it, it
    // will be invalid.
    //
    // Similarly, if a flag takes no argument, we will add a rule for
    // '=', and error if the final parse accepts it. At some point, we
    // should more generally automate such error detection /
    // correction in the Earley parser directly.

    switch (option->type) {
    case N00B_GOAT_BOOL_T_ALWAYS:
    case N00B_GOAT_BOOL_F_ALWAYS:
    case N00B_GOAT_CHOICE_T_ALIAS:
    case N00B_GOAT_CHOICE_F_ALIAS:
        add_noarg_rules(context, option);
        break;
    case N00B_GOAT_BOOL_T_DEFAULT:
    case N00B_GOAT_BOOL_F_DEFAULT:
        add_bool_rules(context, option);
        break;
    case N00B_GOAT_INT:
    case N00B_GOAT_INT_ALIAS:
        add_int_rules(context, option);
        break;
    case N00B_GOAT_FLOAT:
    case N00B_GOAT_FLOAT_ALIAS:
        add_float_rules(context, option);
        break;
    case N00B_GOAT_CHOICE:
    case N00B_GOAT_WORD:
    case N00B_GOAT_WORD_ALIAS:
        add_word_rules(context, option);
        break;
    default:
        n00b_unreachable();
    }

    // Create a rule that consists only of the non-terminal we created for
    // ourselves, and add it to the list of flags in the grammar (nt_1opt)
    n00b_list_t *rule = n00b_list(n00b_type_ref());
    n00b_list_append(rule, n00b_pitem_from_nt(option->my_nonterm));
    n00b_ruleset_add_rule(context->grammar, context->nt_1opt, rule, 0);
}

void
_n00b_gopt_add_subcommand(n00b_gopt_ctx   *ctx,
                          n00b_gopt_cspec *command,
                          n00b_utf8_t     *s,
                          n00b_utf8_t     *doc)
{
    // Nesting check will get handled the next level down.
    n00b_list_t     *rule  = n00b_list(n00b_type_int());
    n00b_list_t     *raw   = n00b_str_split(n00b_to_utf8(s),
                                      n00b_new_utf8(" "));
    n00b_list_t     *words = n00b_list(n00b_type_utf8());
    int              n     = n00b_list_len(raw);
    n00b_codepoint_t cp;
    int64_t          l;
    int64_t          ix;
    int64_t          value;

    for (int i = 0; i < n; i++) {
        n00b_str_t *w = n00b_list_get(raw, i, NULL);
        w             = n00b_str_strip(w);
        if (!n00b_str_codepoint_len(w)) {
            continue;
        }

        w = n00b_to_utf8(n00b_str_lower(w));

        while (w->data[0] == '(') {
            n00b_list_append(words, n00b_new_utf8("("));
            l = n00b_str_codepoint_len(w);

            if (l == 1) {
                break;
            }

            w = n00b_to_utf8(n00b_str_slice(w, 1, l--));
        }
        if (!l) {
            continue;
        }

        // This is a little janky; currently we expect spacing after
        // the end of a group.
        //
        // Also, should probably handle commas too.
        //
        // Heck, this should use the earley parser.

        ix = n00b_str_find(w, n00b_new_utf8(")"));

        if (ix > 0) {
            n00b_list_append(words, n00b_to_utf8(n00b_str_slice(w, 0, ix)));
            w = n00b_to_utf8(n00b_str_slice(w, ix, n00b_str_codepoint_len(w)));
        }

        n00b_list_append(words, w);
    }

    n = n00b_list_len(words);

    for (int i = 0; i < n; i++) {
        n00b_str_t *w = n00b_list_get(words, i, NULL);

        if (n00b_str_eq(w, n00b_new_utf8("("))) {
            n00b_list_append(rule, (void *)N00B_GOG_LPAREN);
            continue;
        }

        if (n00b_str_eq(w, n00b_new_utf8("arg"))) {
            n00b_list_append(rule, (void *)N00B_GOG_WORD);
            continue;
        }

        if (n00b_str_eq(w, n00b_new_utf8("word"))) {
            n00b_list_append(rule, (void *)N00B_GOG_WORD);
            continue;
        }

        if (n00b_str_eq(w, n00b_new_utf8("str"))) {
            n00b_list_append(rule, (void *)N00B_GOG_WORD);
            continue;
        }

        if (n00b_str_eq(w, n00b_new_utf8("int"))) {
            n00b_list_append(rule, (void *)N00B_GOG_INT);
            continue;
        }

        if (n00b_str_eq(w, n00b_new_utf8("float"))) {
            n00b_list_append(rule, (void *)N00B_GOG_FLOAT);
            continue;
        }

        if (n00b_str_eq(w, n00b_new_utf8("bool"))) {
            n00b_list_append(rule, (void *)N00B_GOG_BOOL);
            continue;
        }

        if (n00b_str_eq(w, n00b_new_utf8("raw"))) {
            n00b_list_append(rule, (void *)N00B_GOG_RAW);
            if (i + 1 != n) {
                N00B_CRAISE("RAW must be the final argument.");
            }
            continue;
        }
        if (w->data[0] == ')') {
            n00b_list_append(rule, (void *)N00B_GOG_RPAREN);

            switch (n00b_str_codepoint_len(w)) {
            case 1:
                continue;
            case 2:
                break;
            default:
mod_error:
                N00B_CRAISE(
                    "Can currently only have one of '*', '+' or '?' "
                    "after a grouping.");
            }
            cp = w->data[1];

handle_group_modifier:
            switch (cp) {
            case '*':
                n00b_list_append(rule, (void *)N00B_GOG_STAR);
                continue;
            case '+':
                n00b_list_append(rule, (void *)N00B_GOG_PLUS);
                continue;
            case '?':
                n00b_list_append(rule, (void *)N00B_GOG_OPT);
                continue;
            default:
                goto mod_error;
            }
        }
        switch (w->data[0]) {
        case '*':
        case '+':
        case '?':
            l = n00b_list_len(rule);
            if (l != 0) {
                value = (int64_t)n00b_list_get(rule, l - 1, NULL);
                if (value == N00B_GOG_RPAREN) {
                    if (n00b_str_codepoint_len(w) != 1) {
                        goto mod_error;
                    }
                    cp = w->data[0];
                    goto handle_group_modifier;
                }
            }
            goto mod_error;
        default:
            break;
        }

        // At this point, anything we find should be the final
        // argument, and should be a sub-command name.
        if (i + 1 != n) {
            N00B_CRAISE("Subcommand names must be the final argument.");
        }

        hatrack_dict_item_t *values;

        values = hatrack_dict_items_sort(command->sub_commands, (uint64_t *)&l);

        for (int j = 0; j < l; j++) {
            hatrack_dict_item_t *one = &values[j];
            n00b_gopt_cspec     *sub = one->value;

            if (n00b_str_eq(w, sub->name)) {
                n00b_list_append(rule, one->key);
                goto add_it;
            }
        }

        n00b_utf8_t *msg = n00b_cstr_format("Unknown sub-command: [em]{}[/]", w);

        N00B_RAISE(msg);
    }

    // It's okay for the rule to be empty.

add_it:
    _n00b_gopt_command_add_rule(ctx, command, rule, doc);
}

static bool
run_auto_help(n00b_gopt_ctx *gopt, n00b_gopt_result_t *res)
{
    if (gopt->options & N00B_NO_HELP_AUTO_OUTPUT) {
        return false;
    }

    n00b_list_t *parts;

    if (res->cmd) {
        parts = n00b_str_split(res->cmd, n00b_new_utf8("."));
    }
    else {
        parts = n00b_list(n00b_type_utf8());
    }

    if (!n00b_list_contains(parts, n00b_new_utf8("help"))
        && !n00b_dict_contains(res->flags, n00b_new_utf8("help"))) {
        return false;
    }

    n00b_print(n00b_getopt_default_usage(gopt, res->cmd, res));
    n00b_print(n00b_getopt_command_get_long_for_printing(gopt, res->cmd));
    n00b_print(n00b_getopt_option_table(gopt, res->cmd, true, true));

    return true;
}

// Will run getopt, but does not do any n00b attribute setting.
n00b_gopt_result_t *
n00b_run_getopt_raw(n00b_gopt_ctx *gopt, n00b_utf8_t *cmd, n00b_list_t *args)
{
    if (!gopt->command_name) {
        if (cmd && n00b_len(cmd)) {
            gopt->command_name = cmd;
        }
        else {
            n00b_list_t *parts = n00b_str_split(n00b_app_path(),
                                                n00b_new_utf8("/"));

            gopt->command_name = n00b_list_pop(parts);
            if (!gopt->command_name) {
                gopt->command_name = n00b_get_argv0();
            }
        }
    }

    n00b_list_t *parses = n00b_gopt_parse(gopt, cmd, args);
    int          num    = n00b_list_len(parses);

    if (num > 1) {
        n00b_eprintf("[em]Warning:[/] ambiguous input ({} possible parses).",
                     num);
    }
    if (num == 0) {
        n00b_eprintf("[em]Error:[/] parsing failed.");
        n00b_getopt_show_usage(gopt, n00b_new_utf8(""), NULL);
        return NULL;
    }

    n00b_gopt_result_t *res = n00b_list_get(parses, 0, NULL);
    if (n00b_list_len(res->errors)) {
        n00b_utf8_t *err = n00b_rich_lit("[red]error: [/] ");
        err              = n00b_str_concat(err, n00b_list_get(res->errors, -1, NULL));
        n00b_eprint(err);
        n00b_getopt_show_usage(gopt, res->cmd, res);
        return NULL;
    }

    if (run_auto_help(gopt, res)) {
        return NULL;
    }

    return res;
}

const n00b_vtable_t n00b_gopt_parser_vtable = {
    .num_entries = N00B_BI_NUM_FUNCS,
    .methods     = {
        [N00B_BI_CONSTRUCTOR] = (n00b_vtable_entry)n00b_gopt_init,
        [N00B_BI_GC_MAP]      = (n00b_vtable_entry)N00B_GC_SCAN_ALL,
    },
};

const n00b_vtable_t n00b_gopt_command_vtable = {
    .num_entries = N00B_BI_NUM_FUNCS,
    .methods     = {
        [N00B_BI_CONSTRUCTOR] = (n00b_vtable_entry)n00b_gcommand_init,
        [N00B_BI_GC_MAP]      = (n00b_vtable_entry)N00B_GC_SCAN_ALL,
    },
};

const n00b_vtable_t n00b_gopt_option_vtable = {
    .num_entries = N00B_BI_NUM_FUNCS,
    .methods     = {
        [N00B_BI_CONSTRUCTOR] = (n00b_vtable_entry)n00b_goption_init,
        [N00B_BI_GC_MAP]      = (n00b_vtable_entry)N00B_GC_SCAN_ALL,
    },
};

// TODO:
// 4- Hook up to n00b; allow tying to attributes.
//
//     Figure out how to return any args. Probably the components when
//     we spec need to be namable so we can set things appropriately.
//     Also should be able to get at the raw string values for
//     non-flag items if you need it.
// 5- Help flag type, and automatic output for it.
// 7- Logic / options for auto-setting attributes (w/ type checking).
// 8- Debug flag type for exception handling?
// 9- Return raw results, argv style, and full line w/ and w/o flags.
