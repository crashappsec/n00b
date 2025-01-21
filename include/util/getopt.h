#include "n00b.h"
#pragma once

typedef enum {
    N00B_GOPT_FLAG_OFF          = 0,
    // By default we are lax w/ the equals sign.  If you really want
    // the usuability nightmare of disallowing spaces around it, then
    // okay... this generates a warning if an equals sign is at the start of
    // a token, and then tokenizes the whole thing as a word.
    //
    // Handled when lexing.
    N00B_GOPT_NO_EQ_SPACING     = 0x0001,
    // --flag foo is normally accepted if the flag takes any sort
    // of argument.  But with this on, if the argument is
    // optional, the = is required.
    //
    // If the argument is not optional, we still allow no equals;
    // Perhaps could be talked into an option to change that.
    //
    // Handled when parsing.
    N00B_GOPT_ALWAYS_REQUIRE_EQ = 0x0002,
    // When on, -dbar is treated as "-d bar" if -d takes an argument,
    // or -d -b -a -r if they do not. The first flag name seen that
    // takes an argument triggers the rest of the word being treated
    // as an argument.
    //
    // When off, -dbar and --dbar are treated interchangably, and we only
    // generate the flag_prefix_1 token for flags.
    //
    // Handled in lexing.
    N00B_GOPT_RESPECT_DASH_LEN  = 0x0004,
    // By default a solo -- will cause the rest of the arguments to be
    // passed through as non-flag tokens (the words still get tag
    // types, and get comma separated).
    //
    // Turn this on to disable that behavior; -- will be a flag with
    // no name (generally an error).
    //
    // Handled in lexing.
    N00B_GOPT_NO_DOUBLE_DASH    = 0x0008,
    // By default, a solo - is passed through as a word, since it
    // usually implies stdin where a file name is expected. But if you
    // don't want that hassle, turn this on, then it'll be treated as
    // a badly formed flag (one with a null name).
    //
    // Handled in lexing.
    N00B_GOPT_SINGLE_DASH_ERR   = 0x0010,
    // Options on what we consider a boolean value.  If they're
    // all off, boolean flags will be errors.
    //
    // This is handled the same across the entire parse for
    // consistency, and is not a per-flag or per-command option.
    // Handled in the lexer.
    N00B_GOPT_BOOL_NO_TF        = 0x0020,
    N00B_GOPT_BOOL_NO_YN        = 0x0040,
    // If the same flag 'key' is provided multiple times, we usually allow it,
    // assuming that the last one we see overrides previous instances, as is
    // often the case in unix. If this gets turned on, if you do not
    // explicitly mark something as 'multi', then dupes will be an
    // error.
    // TODO-- eventually handle this in a lex post-processing step.
    N00B_GOPT_ERR_ON_DUPES      = 0x080,
    // By default, we are case INsensitive with flags; but if you need
    // case sensitivity, here you go. Note that this is partially
    // implemented by transforming strings when you register flags and
    // subcommands, so set it before you start setting up stuff.
    //
    // Note that I don't think it's good form when you do allow case
    // sensitivity for flags to allow flag names that only differ in
    // capitalization.
    //
    // Handled in lexer.
    N00B_CASE_SENSITIVE         = 0x0100,
    // If true, the lexer accepts /flag per Windows; it accepts both,
    // but can warn if you mix/match.
    //
    // Handled in lex.
    N00B_ALLOW_WINDOWS_SYNTAX   = 0x0200,
    // If true, the colon is also allowed where an equals sign may
    // appear. We only support this because Chalk is in Nim, and want
    // to follow their tropes; the equals sign cannot currently be
    // disabled, and I have no intention of doing so.
    // Handled in lex.
    N00B_ALLOW_COLON_SEPARATOR  = 0x0400,
    //
    // When this is true, the top-level 'command' is taken from the
    // command name, in which case if the command name is not a known
    // command, a special token OTHER_COMMAND_NAME is generated.
    // Handled in lex.
    N00B_TOPLEVEL_IS_ARGV0      = 0x0800,
    //
    // This option causes subcommand names to be treated as plain old
    // words inside any sub-command where the word doesn't constitute
    // a sub-command itself. Eventually, we might also add it all
    // places BEFORE the command's last nullable piece, when possible.
    //
    // For now though, this is not implemented at all yet, and
    // subcommands are subcommands, even if they're in the wrong
    // place.
    //
    // So this is just TODO.
    N00B_ACCEPT_SUBS_AS_WORDS   = 0x1000,
    //
    // Don't think I've done this one yet.
    N00B_BAD_GOPT_WARN          = 0x2000,
    //
    // By default, we automatically add help
    // documentation. Specifically, when finalizing the configuration
    // before first parse, if we do not already see such things, we
    // add a global --help flag, and will add 'help' commands to all
    // subcommands.
    //
    // If you don't want those things auto-added, turn this off.
    N00B_NO_ADD_AUTO_HELP       = 0x4000,
    //
    // Also by default, if we *see* a 'help' command run, or we see
    // the --help flag passed, we give default output (unless calling
    // the lower level interfaces). To suppress, turn this off.
    N00B_NO_HELP_AUTO_OUTPUT    = 0x8000,
} n00b_gopt_global_flags;

// These token types are always available; flag names and command names
// get their own token IDs as well, which are handed out in the order
// they're registered.
//
// These tokens are really meant to be internal to the generated
// parser.  Most of them are elided when we produce output to the
// user.
typedef enum {
    N00B_GOTT_ASSIGN             = 0,
    N00B_GOTT_COMMA              = 1, // separate items for multi-words.
    N00B_GOTT_INT                = 2,
    N00B_GOTT_FLOAT              = 3,
    N00B_GOTT_BOOL_T             = 4,
    N00B_GOTT_BOOL_F             = 5,
    N00B_GOTT_WORD               = 6,
    // You can differentiate behavior based on argv[0];
    N00B_GOTT_OTHER_COMMAND_NAME = 7,
    N00B_GOTT_UNKNOWN_OPT        = 8,
    N00B_GOTT_LAST_BI            = 9,
} n00b_gopt_bi_ttype;

// The above values show up in *tokens* we generate.  However, when
// specifying a 'rule' for a command or subcommand, before we convert
// to an earley grammar, the api has the user pass a list of either
// the below IDs, or the id associated with commands.
//
// Note that any sub-command always has to be the last thing in any
// given specification, if it exists. You then specify the rules for
// that subcommand seprately.
//
// Similarly, nothing can appear after RAW; it acts like a '--' at the
// command line, and passes absolutely everything through.
//
// Parenthesis have to be nested, and OPT, STAR and PLUS modify
// whatever they follow (use the parens where needed).

typedef enum : int64_t {
    N00B_GOG_NONE,
    N00B_GOG_WORD = 1,
    N00B_GOG_INT,
    N00B_GOG_FLOAT,
    N00B_GOG_BOOL,
    N00B_GOG_RAW,
    N00B_GOG_LPAREN,
    N00B_GOG_RPAREN,
    N00B_GOG_OPT,
    N00B_GOG_STAR,
    N00B_GOG_PLUS,
    N00B_GOG_NUM_GRAMMAR_CONSTS,
} n00b_gopt_grammar_consts;

// For any flag type, you can make arguments optional, or accept
// multiple arguments.
//
// Internally, we deal with 'any number of args' by looking for the
// max value being negative.
typedef enum {
    N00B_GOAT_NONE, // No option is allowed.
    // For boolean flags, T_DEFAULT means the flag name is assumed
    // to be true if no argument is given, but can be overridden with
    // a boolean argument.
    N00B_GOAT_BOOL_T_DEFAULT,
    N00B_GOAT_BOOL_F_DEFAULT,
    N00B_GOAT_BOOL_T_ALWAYS,
    N00B_GOAT_BOOL_F_ALWAYS,
    N00B_GOAT_WORD, // Any value as a string; could be an INT, FLOAT or bool.
    N00B_GOAT_INT,
    N00B_GOAT_FLOAT,
    N00B_GOAT_CHOICE,         // User-supplied list of options
    N00B_GOAT_CHOICE_T_ALIAS, // Boolean flag that invokes a particular choice.
    N00B_GOAT_CHOICE_F_ALIAS, // Boolean flag that clears a particular choice.
    N00B_GOAT_WORD_ALIAS,
    N00B_GOAT_FLOAT_ALIAS,
    N00B_GOAT_INT_ALIAS,
    N00B_GOAT_LAST,
} n00b_flag_arg_type;

// This enum is used to categorize forest nonterms to help us more
// easily walk the graph. The non-terminal's 'user data' field, when
// set, becomes available through the forest node, if we keep the
// parse ctx around.
typedef enum : int64_t {
    N00B_GTNT_OTHER,
    N00B_GTNT_OPT_JUNK_MULTI,
    N00B_GTNT_OPT_JUNK_SOLO,
    N00B_GTNT_CMD_NAME,
    N00B_GTNT_CMD_RULE,
    N00B_GTNT_OPTION_RULE,
    N00B_GTNT_FLOAT_NT,
    N00B_GTNT_INT_NT,
    N00B_GTNT_BOOL_NT,
    N00B_GTNT_WORD_NT,
    N00B_GTNT_BAD_OPTION,
    N00B_GTNT_BAD_ARGS,
} n00b_gopt_node_type;

typedef struct n00b_goption_t {
    n00b_utf8_t            *name;
    n00b_utf8_t            *normalized;
    n00b_utf8_t            *short_doc;
    n00b_utf8_t            *long_doc;
    n00b_list_t            *choices;
    n00b_nonterm_t         *my_nonterm;
    // Token id is the unique ID associated with the specific flag name.
    int64_t                token_id;
    // If linked to a command, the flag will only be allowed
    // in cases where the command is supplied (or inferred).
    struct n00b_gopt_cspec *linked_command;
    struct n00b_goption_t  *linked_option;
    n00b_list_t            *links_inbound;
    // Parse results come back from the raw API in a dictionary keyed by
    // result key.
    int64_t                result_key;
    int16_t                min_args;
    int16_t                max_args;
    // If multiple flags w/ the same result key, this is the main one for the
    // purposes of documentation.
    bool                   primary;
    n00b_flag_arg_type      type;
} n00b_goption_t;

typedef struct n00b_gopt_cspec {
    struct n00b_gopt_ctx   *context;
    struct n00b_gopt_cspec *parent;
    n00b_nonterm_t         *name_nt;
    n00b_nonterm_t         *rule_nt;
    n00b_list_t            *summary_docs;
    n00b_utf8_t            *name;
    int64_t                token_id;
    uint32_t               seq; // Used for tracking valid flags per rule.
    n00b_utf8_t            *short_doc;
    n00b_utf8_t            *long_doc;
    n00b_dict_t            *sub_commands;
    n00b_list_t            *aliases;
    n00b_dict_t            *owned_opts;
    // By default, command opts must match known names.  If this is
    // on, it will pass through bad flag names (inapropriate to the
    // context or totally unknown), treating them as arguments (of
    // type WORD).
    bool                   bad_opt_passthru;
    // This causes warnings to be printed for bad opts, as opposed to
    // them being fatal errors. If ignore_bad_opts is also on, then
    // bad opts both produce warnings and get converted to words.
    // Otherwise, (if bad_opt_passthrough is off, and this is on), bad
    // opts warn but are dropped from the input.
    bool                   bad_opt_warn;
    // Whether the subcommand exists in an explicit parent rule.
    bool                   explicit_parent_rule;
} n00b_gopt_cspec;

typedef struct n00b_gopt_ctx {
    n00b_grammar_t *grammar;
    n00b_dict_t    *all_options;
    n00b_dict_t    *sub_names;
    n00b_dict_t    *primary_options;
    n00b_dict_t    *top_specs;
    n00b_list_t    *tokens;
    n00b_list_t    *terminal_info;
    n00b_nonterm_t *nt_start;
    n00b_nonterm_t *nt_float;
    n00b_nonterm_t *nt_int;
    n00b_nonterm_t *nt_word;
    n00b_nonterm_t *nt_bool;
    n00b_nonterm_t *nt_eq;
    n00b_nonterm_t *nt_opts;
    n00b_nonterm_t *nt_1opt;
    n00b_nonterm_t *nt_badopt;
    n00b_nonterm_t *nt_badargs;
    int64_t        default_command;
    uint64_t       counter;
    // See the global flags array above.
    uint32_t       options;
    bool           finalized;
    bool           show_debug;
} n00b_gopt_ctx;

typedef struct {
    n00b_gopt_ctx *gctx;
    n00b_utf8_t   *command_name;
    n00b_utf8_t   *normalized_name;
    n00b_list_t   *words;
    n00b_utf8_t   *word;
    n00b_utf32_t  *raw_word;
    int           num_words;
    int           word_id;
    int           cur_wordlen;
    int           cur_word_position;
    bool          force_word;
    bool          all_words;
} n00b_gopt_lex_state;

typedef struct {
    n00b_utf8_t *cmd;
    n00b_dict_t *flags;
    n00b_dict_t *args;
    n00b_list_t *errors;
} n00b_gopt_result_t;

typedef struct {
    n00b_list_t      *flag_nodes;
    n00b_gopt_cspec  *cur_cmd;
    n00b_dict_t      *flags;
    n00b_dict_t      *args;
    n00b_tree_node_t *intree;
    n00b_gopt_ctx    *gctx;
    n00b_utf8_t      *path;
    n00b_utf8_t      *deepest_path;
    n00b_list_t      *errors;
    n00b_parser_t    *parser;
    n00b_list_t      *cmd_stack;
} n00b_gopt_extraction_ctx;

typedef struct {
    n00b_utf8_t    *cmd;
    n00b_goption_t *spec;
    n00b_obj_t      value;
    int            n; // Number of items stored.
} n00b_rt_option_t;

extern n00b_list_t *n00b_gopt_parse(n00b_gopt_ctx *, n00b_str_t *, n00b_list_t *);
extern void        n00b_gopt_command_add_rule(n00b_gopt_ctx *,
                                             n00b_gopt_cspec *,
                                             n00b_list_t *);
extern n00b_list_t *_n00b_gopt_rule(int64_t, ...);
#define n00b_gopt_rule(s, ...) _n00b_gopt_rule(s, N00B_VA(__VA_ARGS__))

extern void n00b_gopt_add_subcommand(n00b_gopt_ctx *,
                                    n00b_gopt_cspec *,
                                    n00b_utf8_t *);

extern n00b_grid_t *n00b_getopt_option_table(n00b_gopt_ctx *,
                                           n00b_utf8_t *,
                                           bool,
                                           bool);

extern n00b_grid_t  *n00b_getopt_command_get_long_for_printing(n00b_gopt_ctx *,
                                                             n00b_utf8_t *);
extern n00b_grid_t  *n00b_getopt_default_usage(n00b_gopt_ctx *, n00b_utf8_t *);
extern n00b_tuple_t *n00b_getopt_raw_usage_info(n00b_gopt_ctx *, n00b_utf8_t *);
extern n00b_gopt_result_t *n00b_run_getopt_raw(n00b_gopt_ctx *,
                                               n00b_utf8_t *,
                                               n00b_list_t *);

static inline void
n00b_getopt_show_usage(n00b_gopt_ctx *ctx, n00b_utf8_t *cmd)
{
    n00b_print(n00b_getopt_default_usage(ctx, cmd));
}

static inline n00b_list_t *
n00b_gopt_rule_any_word(void)
{
    return n00b_gopt_rule(N00B_GOG_LPAREN,
                         N00B_GOG_WORD,
                         N00B_GOG_RPAREN,
                         N00B_GOG_STAR);
}

static inline n00b_list_t *
n00b_gopt_rule_optional_word(void)
{
    return n00b_gopt_rule(N00B_GOG_LPAREN,
                         N00B_GOG_WORD,
                         N00B_GOG_RPAREN,
                         N00B_GOG_OPT);
}

static inline bool
n00b_gopt_has_errors(n00b_gopt_result_t *res)
{
    return res->errors && n00b_list_len(res->errors) != 0;
}

static inline n00b_list_t *
n00b_gopt_get_errors(n00b_gopt_result_t *res)
{
    return res->errors;
}

static inline n00b_utf8_t *
n00b_gopt_get_command(n00b_gopt_result_t *res)
{
    return res->cmd;
}

static inline n00b_dict_t *
n00b_gopt_get_flags(n00b_gopt_result_t *res)
{
    return res->flags;
}

static inline n00b_list_t *
n00b_gopt_get_args(n00b_gopt_result_t *res, n00b_utf8_t *section)
{
    return hatrack_dict_get(res->args, section, NULL);
}

#ifdef N00B_USE_INTERNAL_API
void n00b_gopt_tokenize(n00b_gopt_ctx *, n00b_utf8_t *, n00b_list_t *);
void n00b_gopt_finalize(n00b_gopt_ctx *);

static inline bool
n00b_gctx_gflag_is_set(n00b_gopt_ctx *ctx, uint32_t flag)
{
    return (bool)ctx->options & flag;
}

static inline bool
n00b_gopt_gflag_is_set(n00b_gopt_lex_state *state, uint32_t flag)
{
    return n00b_gctx_gflag_is_set(state->gctx, flag);
}
#endif
