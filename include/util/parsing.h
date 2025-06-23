#pragma once
#include "n00b.h"

// This implements Earley's algorithm with the Aycock / Horspool
// trick (adding empty string predictions, which requires checking
// nullability of non-terminals).
//
// I have not yet implemented the Joop Leo optimization for right
// recursive grammars.
//
// I also add a couple of very minor enhancements to the core algorithm:
//
// 1. I support anonymous non-terminals, which maps to parenthesized
//    grouping of items in an EBNF grammar.
//
// 2. I support groups tracking min / max consecutive instances in a
//     rule.
//
// These two things combined make it easier to directly map common
// regexp operators like * (any), ? (optional), + (one or more), or
// 'n to m' instances.

typedef enum {
    N00B_P_NULL,     // A terminal value-- the empty string.
    N00B_P_NT,       // A non-terminal ruleset definition.
    N00B_P_TERMINAL, // A single terminal.
    N00B_P_ANY,      // Match for 'any terminal'.
    N00B_P_BI_CLASS, // Builtin character classes; these are effectively
                     // a 'choice' of one character terminals.
    N00B_P_SET,      // A specific set of terminals can
                     // match. Generally for use when the built-in
                     // classes don't meet your needs.
    N00B_P_GROUP,    // A group of items, which can have a min or max.
} n00b_pitem_kind;

typedef enum {
    N00B_P_BIC_ID_START,
    N00B_P_BIC_ID_CONTINUE,
    N00B_P_BIC_N00B_ID_START,
    N00B_P_BIC_N00B_ID_CONTINUE,
    N00B_P_BIC_DIGIT,
    N00B_P_BIC_ANY_DIGIT,
    N00B_P_BIC_UPPER,
    N00B_P_BIC_UPPER_ASCII,
    N00B_P_BIC_LOWER,
    N00B_P_BIC_LOWER_ASCII,
    N00B_P_BIC_SPACE,
    N00B_P_BIC_JSON_string_CONTENTS,
    N00B_P_BIC_HEX_DIGIT,
    N00B_P_BIC_NONZERO_ASCII_DIGIT,
} n00b_bi_class_t;

typedef enum {
    N00B_EO_PREDICT_NT,
    N00B_EO_PREDICT_G,
    N00B_EO_FIRST_GROUP_ITEM,
    N00B_EO_SCAN_TOKEN,
    N00B_EO_SCAN_ANY,
    N00B_EO_SCAN_NULL,
    N00B_EO_SCAN_CLASS,
    N00B_EO_SCAN_SET,
    N00B_EO_COMPLETE_N,
    N00B_EO_ITEM_END,
} n00b_earley_op;

// For my own sanity, this will map individual earley items to the
// kind of node info we have for it.
//
// I've explicitly numbered them to what they'd be assigned anyway, just
// because I check the relationships rn and want to make it explicit.
typedef enum {
    N00B_SI_NONE             = 0,
    N00B_SI_NT_RULE_START    = 1,
    N00B_SI_NT_RULE_END      = 2,
    N00B_SI_GROUP_START      = 3,
    N00B_SI_GROUP_END        = 4,
    N00B_SI_GROUP_ITEM_START = 5,
    N00B_SI_GROUP_ITEM_END   = 6,
} n00b_subtree_info_t;

typedef enum {
    N00B_EARLEY_CMP_EQ = 0,
    N00B_EARLEY_CMP_LT = -1,
    N00B_EARLEY_CMP_GT = 1,
} n00b_earley_cmp_t;

typedef struct n00b_terminal_t     n00b_terminal_t;
typedef struct n00b_nonterm_t      n00b_nonterm_t;
typedef struct n00b_parse_rule_t   n00b_parse_rule_t;
typedef struct n00b_rule_group_t   n00b_rule_group_t;
typedef struct n00b_pitem_t        n00b_pitem_t;
typedef struct n00b_token_info_t   n00b_token_info_t;
typedef struct n00b_parser_t       n00b_parser_t;
typedef struct n00b_earley_item_t  n00b_earley_item_t;
typedef struct n00b_grammar_t      n00b_grammar_t;
typedef struct n00b_earley_state_t n00b_earley_state_t;
typedef struct n00b_parse_node_t   n00b_parse_node_t;

// Instead of manually walking a tree, you can set a function for each
// NT type. It will walk in post order (children left to right, then
// root last<).
//
// For terminals, the walker returns the terminal. For non-terminals,
// your callback can return anything you like.
//
// For arguments to the callback, The first element is the node the
// callback is associated with the non-terminal that fired. From here
// you can get things like the index into the non-terminal's rule, or
// file and line info. The second is the results from each child node
// if any, or NULL if there are no children.
//
// If a node doesn't have a callback, then any values from tokens or
// callbacks below are propogated upward.
//
// The third argument is a 'thunk'; completely user defined; pass it
// in at the beginning of the walk.

typedef void *(*n00b_parse_action_t)(n00b_parse_node_t *,
                                     n00b_list_t *,
                                     void *);

struct n00b_terminal_t {
    n00b_string_t *value;
    void          *user_data;
    int64_t        id;
};

struct n00b_nonterm_t {
    n00b_string_t      *name;
    n00b_list_t        *rules; // A list of n00b_parse_rule_t objects;
    n00b_parse_action_t action;
    void               *user_data;
    int64_t             id;
    bool                group_nt;
    bool                empty_is_error;
    bool                error_nulled;
    bool                start_nt;
    bool                finalized;
    bool                nullable;
    bool                no_error_rule;
};

struct n00b_parse_rule_t {
    n00b_nonterm_t    *nt;
    n00b_list_t       *contents;
    int32_t            cost;
    // For penalty rules. We track the original rule so that we can
    // more easily reconstruct the intended tree structure.
    n00b_parse_rule_t *link;
    n00b_string_t     *doc;
    n00b_string_t     *short_doc;
    // Used when creating error rules; it denotes that the nonterminal
    // would be nullable if we allow a token omission, and that we
    // generated rules for single token omission that respect that
    // fact (done to avoid cycles).
    bool               penalty_rule;
    // User-defined info. Getopt uses it to hide intermediate productions.
    void              *thunk;
};

struct n00b_rule_group_t {
    n00b_nonterm_t *contents;
    int32_t         min;
    int32_t         max;
    int32_t         gid;
};

struct n00b_pitem_t {
    n00b_parser_t *parser;
    n00b_string_t *s; // cached repr. Should go away I think.

    union {
        n00b_list_t       *items; // n00b_pitem_t's
        n00b_rule_group_t *group;
        n00b_bi_class_t class;
        int64_t nonterm;
        int64_t terminal; // An index into a table of terminals.
    } contents;
    n00b_pitem_kind kind;
};

struct n00b_token_info_t {
    void          *user_info;
    n00b_string_t *value;
    int32_t        tid;
    int32_t        index;
    // These capture the start location.
    uint32_t       line;   // 1-indexed.
    uint32_t       column; // 0-indexed.
    uint32_t       endcol;
};

struct n00b_parse_node_t {
    union {
        n00b_token_info_t *token;
        n00b_string_t     *name;
    } info;
    int64_t  noscan;
    int64_t  id;
    int64_t  hv;
    int32_t  start;
    int32_t  end;
    int32_t  rule_index;
    uint32_t penalty;
    uint32_t cost;
    uint16_t penalty_location;
    bool     token;
    bool     group_item;
    bool     group_top;
    bool     missing;
    bool     bad_prefix;
};

struct n00b_earley_item_t {
    // The predicted item in our cur rule.
    n00b_earley_item_t *start_item;
    // This is where we really are keeping track of the previous
    // sibling in the same chain of rules. This will never need to
    // hold more than one item when pointing in this direction, part
    // of why it's easier to build the graph backwards.
    n00b_earley_item_t *previous_scan;
    // This basically represents all potential parent nodes in the tree
    // above the node we're currently processing.
    n00b_set_t         *parent_states;
    // Any item we predict, if we predicted. This basically tracks
    // subgraph starts, and the next one tracks subgraph ends.
    n00b_set_t         *predictions;
    // Earley items that, (like Jerry McGuire?), complete us. This is
    // used in conjunction with the prior one to identify all matching
    // derrivations for a non-terminal when we have an ambiguous
    // grammar.
    n00b_set_t         *completors;
    // A pointer to the actual rule contents we represent.
    n00b_parse_rule_t  *rule;
    // Static info about the current group.
    n00b_rule_group_t  *group;
    // The Earley state associated with a group.
    n00b_earley_item_t *group_top;
    // Used to cache info related to the state during tree-building.
    n00b_dict_t        *cache;
    int64_t             noscan;
    // The `rule_id` is the non-terminal we pulled the rule from.
    // The non-terminal can contain multiple rules though.
    int32_t             ruleset_id;
    // This is the field that distinguishes which rule in the ruleset
    // we pulled. This isn't all that useful given we have the pointer
    // to the rule, but I keep it around for debuggability.
    int32_t             rule_index;
    // Which item in the rule are we to process (the 'dot')?
    int32_t             cursor;
    // The next few items are to help us distinguish when we need to
    // create unique states, or when we can share them.
    int32_t             predictor_ruleset_id;
    int32_t             predictor_rule_index;
    int32_t             predictor_cursor;
    // If we're a group item, how many have we matched?
    int32_t             match_ct;
    // Thhe next two fields is the recognizer's state machine
    // location, mainly useful for I/O, but since we have it, we use
    // it over the state arrays in the parser.
    //
    // Basically, each 'state' in the Early algorithm processes one
    // token of input stream, and generates a bunch of so-called Early
    // Items in that state. You *could* think about them as sub-states
    // for sure. But I stick with the algorithm's lingo, so we track
    // here both the state this item was created in, as well as the
    // index into that state in which we live.
    int32_t             estate_id;
    int32_t             eitem_index;
    // These tracks penalties the grammar associates with this rule.
    // Current 'total' associated with an earley state.
    // This will just be a sub of the components below.
    uint32_t            penalty;
    // Anything added to this state, or prior sibling states, via a
    // completion. In the case of groups, the individual items inside
    // a group take sub-penalties from their elements; they are
    // propogated up to the group node, but
    uint32_t            sub_penalties;
    // Anything produced from SELECTING the current rule, including
    // any inherited penalty. This should be constant for every item
    // in a rule, and should be 0 for group items.
    uint32_t            my_penalty;
    // Anything associated with too few or too many matches. This
    // really is meant to disambiguate item starts only based on the
    // penalty we would assess; during group completion, the proper
    // group penalty gets calculated, instead of propogating it
    // throughout the group item.
    uint32_t            group_penalty;
    uint32_t            cost;
    // In the classic Earley algorithm, this operation to perform on a
    // state is not selected when we generate the state; it's
    // generated by looking at the item when we get to it for
    // processing.
    //
    // However, we absolutely *do* have all the information we need to
    // determine the operation when we create the state, which is what
    // I am doing. I do it this way because, with the addition of
    // groups, it is the best, most intuitive mechanism in my view for
    // create the extra helper state we need that represents the
    // difference between starting a group node in the tree, and
    // starting the first group item. For that reason alone, it's
    // easier for us to just set the operation when we generate the
    // state, so we do that for everything now.
    //
    // To that end, whereas the raw Earley algorithm only uses
    // 'predict', 'scan' and 'complete', we basically split out
    // prediction into 'predict non-terminal', 'predict group start',
    // and 'predict group item'. You can think of 'scan' as 'predict
    // terminal'; that's exactly what it is, as we do add the state
    // when we see a terminal, even before we've checked to see if it
    // matches the input (we don't move the cursor to the right of
    // terminals unless the scan operation can succeed).
    //
    // In some ways, that would be more regular, but I continue to
    // call it 'scan' because it's aligned with the classic algorithm.
    n00b_earley_op      op;
    // When we predict a group we seprately predict items. So for
    // groups it would be natural for to write Earley states like:
    //
    // >>anonymous_group<< := • (<<Digit>>)
    //
    // But to make tree building easy, we really need to keep groups
    // separate from their items.  So Right now, we bascially use this
    // to denote that a state is associated with the top node, not the
    // items.
    //
    // We'll represent it like this:
    // >>group<< := •• (<<Digit>> <<Any>>)+
    //
    // And the items as:
    // >>item<< := • (<<Digit>> <<Any>>)+
    //
    // And the completion versions:
    // >>item<< := (<<Digit>> <<Any>>)+ •
    // >>group<< := (<<Digit>> <<Any>>)+ ••
    //
    // The individual item nodes don't need to keep count of matches;
    // we do for convenience, but it can lead to more Earley states
    // than necessary for sure.
    //
    // Also note that any time a group is first predicted, it's
    // randomly assigned an ID right now. I'll probably change that and
    // assign them hidden non-terminal IDs statically.
    bool                double_dot;
    // When predicting a non-terminal, if this is true, we should
    // additionally perform a 'null' prediction.
    // Currently, this skirts the whole penalty / cost system, so is
    // explicitly not done with error productions, which must match
    // a null pitem.
    bool                null_prediction;
    bool                no_reprocessing;
    // This strictly isn't necessary, but it's been a nice helper for
    // me.  The basic idea is that subtrees span from a start item to
    // an end item. The start item will result from a prediction (in
    // practice, the cursor will be at 0), and the end item will be
    // where the cursor is at the end.
    //
    // In ambiguous grammars, this can resilt in an n:m mapping of
    // valid subtrees. So when tree-building, we cache subtrees in the
    // earley item associated with the start, using a dictionary where
    // the key is the END early item, and the payload is the node info
    // for that particular possible subtree.
    //
    // This helper allows us to look at the raw state machine output
    // and be able to tell whether the EI is the start or end of a
    // subtree, and if so, whether the subtree constitutes a group, an
    // item in a group, or a vanilla non-terminal.
    n00b_subtree_info_t subtree_info;
};

struct n00b_grammar_t {
    n00b_list_t *named_terms; // I think this is unneeded
    n00b_list_t *rules;
    n00b_list_t *nt_list;
    n00b_dict_t *nt_map;
    n00b_dict_t *terminal_map; // Registered non-unicode token types.
    int32_t      default_start;
    uint32_t     max_penalty;
    bool         hide_penalty_rewrites;
    bool         hide_groups;
    int          suspend_penalty_hiding;
    bool         suspend_group_hiding;
    bool         error_rules;
    bool         finalized;
};

struct n00b_earley_state_t {
    // The token associated with the current state;
    n00b_token_info_t *token;
    // When tree-building, any ambiguous parse can share the leaf.
    n00b_tree_node_t  *cache;
    n00b_list_t       *items;
    int                id;
};

// Token iterators get passed the parse context, and can assign to the
// second parameter a memory address associated with the yielded
// token.  This gets put into the 'info' field of the token we
// produce.
//
// The callback should return the special N00B_TOK_EOF when there are
// no more tokens. Note, if the token is not in the parser's set,
// there will not be an explicit error directly-- the token might be a
// unicode codepoint that match a builtin class, or else will still
// match when the rule specifies the N00B_P_ANY terminal.
typedef int64_t (*n00b_tokenizer_fn)(n00b_parser_t *, void **);

struct n00b_parser_t {
    n00b_grammar_t      *grammar;
    n00b_list_t         *states;
    n00b_earley_state_t *current_state;
    n00b_earley_state_t *next_state;
    void                *user_context;
    n00b_dict_t         *debug_highlights;
    // The tokenization callback should set this if the token ID
    // returned isn't recognized, or if the value wasn't otherwise set
    // when initializing the corresponding non-terminal.
    void                *token_cache;
    n00b_tokenizer_fn    tokenizer;
    bool                 run;
    int32_t              start;
    int32_t              position;       // Absolute pos in tok stream
    int32_t              current_line;   // 1-indexed.
    int32_t              current_column; // 0-indexed.
    int32_t              fnode_count;    // Next unique node ID
    bool                 ignore_escaped_newlines;
    bool                 preloaded_tokens;
    bool                 show_debug;
    bool                 tree_annotations;
};

// Any registered terminals that we give IDs will be strings. If the
// string is only one character, we reuse the unicode code point, and
// don't bother registering. All terminals of more than one character
// start with this prefix.
//
// Ensuring that single-digit terminals match their code point makes
// it easier to provide a simple API for cases where we want to
// tokenize a string into single codepoints... we don't have to
// register or translate anything.

#define N00B_START_TOK_ID        0x40000000
#define N00B_TOK_OTHER           -3
#define N00B_TOK_IGNORED         -2 // Passthrough tokens, like whitespace.
#define N00B_TOK_EOF             -1
#define N00B_EMPTY_STRING        -126
#define N00B_GID_SHOW_GROUP_LHS  -255
#define N00B_ERROR_NODE_ID       -254
#define N00B_IX_START_OF_PROGRAM -1

extern n00b_pitem_t      *n00b_group_items(n00b_grammar_t *p,
                                           n00b_list_t    *pitems,
                                           int             min,
                                           int             max);
extern n00b_parse_rule_t *_n00b_ruleset_add_rule(n00b_grammar_t *,
                                                 n00b_nonterm_t *,
                                                 n00b_list_t *,
                                                 int,
                                                 n00b_string_t *);
#define n00b_ruleset_add_rule(g, nt, l, i) \
    _n00b_ruleset_add_rule(g, nt, l, i, NULL)

extern void            n00b_parser_set_default_start(n00b_grammar_t *,
                                                     n00b_nonterm_t *);
extern void            n00b_internal_parse(n00b_parser_t *,
                                           n00b_nonterm_t *);
extern void            n00b_parse_token_list(n00b_parser_t *,
                                             n00b_list_t *,
                                             n00b_nonterm_t *);
extern void            n00b_parse_string(n00b_parser_t *,
                                         n00b_string_t *,
                                         n00b_nonterm_t *);
extern void            n00b_parse_string_list(n00b_parser_t *,
                                              n00b_list_t *,
                                              n00b_nonterm_t *);
extern n00b_nonterm_t *n00b_pitem_get_ruleset(n00b_grammar_t *,
                                              n00b_pitem_t *);
extern n00b_table_t   *n00b_parse_state_format(n00b_parser_t *, bool);
extern n00b_table_t   *n00b_forest_format(n00b_list_t *);
extern n00b_string_t  *n00b_repr_token_info(n00b_token_info_t *);
extern int64_t         n00b_token_stream_codepoints(n00b_parser_t *,
                                                    void **);
extern int64_t         n00b_token_stream_strings(n00b_parser_t *, void **);
extern n00b_table_t   *n00b_get_parse_state(n00b_parser_t *, bool);
extern n00b_table_t   *n00b_format_parse_state(n00b_parser_t *, bool);
extern n00b_table_t   *n00b_grammar_format(n00b_grammar_t *);
extern void            n00b_parser_reset(n00b_parser_t *);
extern n00b_string_t  *n00b_repr_parse_node(n00b_parse_node_t *);
extern n00b_list_t    *n00b_parse_get_parses(n00b_parser_t *);
extern void           *n00b_parse_tree_walk(n00b_parser_t *,
                                            n00b_tree_node_t *,
                                            void *);
extern n00b_string_t  *n00b_repr_rule(n00b_grammar_t *,
                                      n00b_list_t *,
                                      int);

#ifdef N00B_USE_INTERNAL_API
#define N00B_GROUP_ID                  1 << 28
#define N00B_DEFAULT_MAX_PARSE_PENALTY 128

extern n00b_nonterm_t *n00b_get_nonterm(n00b_grammar_t *, int64_t);
extern n00b_string_t  *n00b_repr_rule(n00b_grammar_t *, n00b_list_t *, int);
extern n00b_list_t    *n00b_repr_earley_item(n00b_parser_t *,
                                             n00b_earley_item_t *,
                                             int);
extern n00b_string_t  *n00b_repr_nonterm(n00b_grammar_t *, int64_t, bool);
extern n00b_string_t  *n00b_repr_group(n00b_grammar_t *, n00b_rule_group_t *);
extern n00b_string_t  *n00b_repr_term(n00b_grammar_t *, int64_t);
extern n00b_string_t  *n00b_repr_rule(n00b_grammar_t *, n00b_list_t *, int);
extern n00b_string_t  *n00b_repr_token_info(n00b_token_info_t *);
extern n00b_table_t   *n00b_repr_state_table(n00b_parser_t *, bool);
extern void            n00b_parser_load_token(n00b_parser_t *);
extern n00b_table_t   *n00b_get_parse_state(n00b_parser_t *, bool);
extern n00b_string_t  *n00b_parse_repr_item(n00b_grammar_t *, n00b_pitem_t *);
extern void            n00b_prep_first_parse(n00b_grammar_t *);
extern bool            n00b_is_nullable_pitem(n00b_grammar_t *,
                                              n00b_pitem_t *,
                                              n00b_list_t *);

#endif

extern void         n00b_add_debug_highlight(n00b_parser_t *, int32_t, int32_t);
extern n00b_list_t *n00b_clean_trees(n00b_parser_t *, n00b_list_t *);
