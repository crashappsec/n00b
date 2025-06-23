#define N00B_USE_INTERNAL_API
#include "n00b.h"

n00b_grammar_t       *n00b_bnf_grammar = NULL;

#define ntpi(s)          n00b_pitem_nonterm_raw(n00b_bnf_grammar, n00b_cstring(s))
#define ntobj(s)         n00b_pitem_get_ruleset(n00b_bnf_grammar, s)
#define add_rule(x, ...) _add_rule(x, __VA_ARGS__ __VA_OPT__(, ) 0)
#define cp(x)            n00b_pitem_terminal_cp(x)
#define empty()          n00b_new_pitem(N00B_P_NULL)
#define cclass(c)        n00b_pitem_builtin_raw(c)
#define cset(...)                            \
    n00b_pitem_choice_raw(n00b_bnf_grammar, \
                          _construct_char_set(__VA_ARGS__ __VA_OPT__(, ) 0))

static inline n00b_list_t *
_construct_char_set(n00b_pitem_t *pi, ...)
{
    n00b_list_t *result = n00b_list(n00b_type_int());
    va_list      args;

    va_start(args, pi);

    while (pi) {
        n00b_list_append(result, pi);
        pi = va_arg(args, n00b_pitem_t *);
    }

    return result;
}

static inline void
_add_rule(n00b_pitem_t *pi, ...)
{
    n00b_list_t  *rule = n00b_list(n00b_type_ref());
    va_list       ap;
    n00b_pitem_t *item;
    va_start(ap, pi);
    item = va_arg(ap, n00b_pitem_t *);
    while (item) {
        n00b_list_append(rule, item);
        item = va_arg(ap, n00b_pitem_t *);
    }
    n00b_ruleset_add_rule(n00b_bnf_grammar, ntobj(pi), rule, 0);
}

static void *
bnf_walk_rule(n00b_parse_node_t *node, n00b_list_t *l, void *ignore)
{
    n00b_string_t *name = n00b_list_get(l, 2, NULL);
    void *expr = n00b_list_get(l, 9, NULL);
    n00b_list_t *pair = n00b_list(n00b_type_ref());

    n00b_list_append(pair, name);
    n00b_list_append(pair, expr);
    return pair;
}

static void *
bnf_walk_syntax(n00b_parse_node_t *node, n00b_list_t *l, void *ignore)
{
    n00b_dict_t *result;
    n00b_list_t *rule_pair = n00b_list_get(l, 0, NULL);
    n00b_string_t *name = n00b_list_get(rule_pair, 0, NULL);
    void *expr = n00b_list_get(rule_pair, 1, NULL);

    if (n00b_list_len(l) == 1) {
        result = n00b_dict(n00b_type_string(), n00b_type_ref());
    } else {
        result = n00b_list_get(l, 1, NULL);
    }

    hatrack_dict_add(result, name, expr);
    return result;
}

static void *
bnf_walk_expression(n00b_parse_node_t *node, n00b_list_t *l, void *ignore)
{
    n00b_list_t *list;
    n00b_list_t *alts = n00b_list(n00b_type_ref());
    void *expr;

    list = n00b_list_get(l, 0, NULL);

    // list
    if (n00b_list_len(l) == 1) {
        n00b_list_append(alts, list);
        return alts;
    }

    // list | expression
    list = n00b_list_get(l, 0, NULL);
    expr = n00b_list_get(l, 4, NULL);
    n00b_list_append(alts, list);
    n00b_list_append(alts, expr);
    return alts;
}

static void *
bnf_walk_list(n00b_parse_node_t *node, n00b_list_t *l, void *ignore)
{
    n00b_list_t *term;
    n00b_list_t *list;

    // term
    if (n00b_list_len(l) == 1) {
        term = n00b_list(n00b_type_ref());
        n00b_list_append(term, n00b_list_get(l, 0, NULL));
        return term;
    }

    // term opt_ws list
    list = n00b_list_get(l, 2, NULL);
    n00b_list_append(list, n00b_list_get(l, 0, NULL));
    return list;
}

static void *
bnf_walk_term(n00b_parse_node_t *node, n00b_list_t *l, void *ignore)
{
    // literal
    if (n00b_list_len(l) == 1) {
        return n00b_list_get(l, 0, NULL);
    }

    // '<' rule_name '>'
    return n00b_list_get(l, 1, NULL);
}

static void *
bnf_walk_literal(n00b_parse_node_t *node, n00b_list_t *l, void *ignore)
{
    // '"' text '"'
    // '\'' text '\''
    return n00b_list_get(l, 1, NULL);
}

static void *
bnf_walk_text(n00b_parse_node_t *node, n00b_list_t *l, void *ignore)
{
    // ""
    if (n00b_list_len(l) == 1) {
        return n00b_cached_empty_string();
    }

    // character text
    return n00b_string_concat(n00b_list_get(l, 0, NULL),
                              n00b_list_get(l, 1, NULL));
}

static void *
bnf_walk_char(n00b_parse_node_t *node, n00b_list_t *l, void *ignore)
{
    n00b_token_info_t *ti = n00b_list_get(l, 0, NULL);
    n00b_string_t     *v  = ti->value;
    char               ch = v->data[0];

    if (ch == '\'' || ch == '"') {
        return n00b_list(n00b_type_ref());
    }

    return n00b_list_get(l, 0, NULL);
}

static void *
bnf_walk_rule_name(n00b_parse_node_t *node, n00b_list_t *l, void *ignore)
{
    // letter
    if (n00b_list_len(l) == 1) {
        return n00b_list_get(l, 0, NULL);
    }

    // rule_name rule_char
    return n00b_string_concat(n00b_list_get(l, 0, NULL),
                              n00b_list_get(l, 1, NULL));
}

static once void
n00b_bnf_init(void)
{
    n00b_gc_register_root(&n00b_bnf_grammar, 1);
    n00b_bnf_grammar = n00b_new(n00b_type_grammar(), detect_errors : false);

    n00b_pitem_t *syntax = ntpi("syntax");
    n00b_pitem_t *rule = ntpi("rule");
    n00b_pitem_t *opt_whitespace = ntpi("opt_whitespace");
    n00b_pitem_t *expression = ntpi("expression");
    n00b_pitem_t *line_end = ntpi("line_end");
    n00b_pitem_t *list = ntpi("list");
    n00b_pitem_t *term = ntpi("term");
    n00b_pitem_t *literal = ntpi("literal");
    n00b_pitem_t *text1 = ntpi("text1");
    n00b_pitem_t *text2 = ntpi("text2");
    n00b_pitem_t *character = ntpi("character");
    n00b_pitem_t *letter = ntpi("letter");
    n00b_pitem_t *digit = ntpi("digit");
    n00b_pitem_t *symbol = ntpi("symbol");
    n00b_pitem_t *character1 = ntpi("character1");
    n00b_pitem_t *character2 = ntpi("character2");
    n00b_pitem_t *rule_name = ntpi("rule_name");
    n00b_pitem_t *rule_char = ntpi("rule_char");

    n00b_grammar_set_default_start(n00b_bnf_grammar, ntobj(rule));

    // <syntax> ::= <rule> | <rule> <syntax>
    add_rule(syntax, rule);
    add_rule(syntax, rule, syntax);

    // <rule> ::= <opt-whitespace> "<" <rule-name> ">" <opt-whitespace> "::=" <opt-whitespace> <expression> <line-end>
    add_rule(rule, opt_whitespace, cp('<'), rule_name, cp('>'), opt_whitespace, cp(':'), cp(':'), cp('='), opt_whitespace, expression, line_end);

    // <opt_whitespace> ::= " " <opt_whitespace> | ""
    add_rule(opt_whitespace, cclass(N00B_P_BIC_SPACE), opt_whitespace);
    add_rule(opt_whitespace, empty());

    // <expression> ::= <list> | <list> <opt-whitespace> "|" <opt-whitespace> <expression>
    add_rule(expression, list);
    add_rule(expression, list, opt_whitespace, cp('|'), opt_whitespace, expression);

    // <line-end> ::= <opt-whitespace> <EOL> | <line-end> <line-end>
    add_rule(line_end, opt_whitespace, cp('\n'));
    add_rule(line_end, line_end, line_end);

    // <list> ::= <term> | <term> <opt-whitespace> <list>
    add_rule(list, term);
    add_rule(list, term, opt_whitespace, list);

    // <term> ::= <literal> | "<" <rule-name> ">"
    add_rule(term, literal);
    add_rule(term, cp('<'), rule_name, cp('>'));

    // <literal> ::= '"' <text1> '"' | "'" <text2> "'"
    add_rule(literal, cp('"'), text1, cp('"'));
    add_rule(literal, cp('\''), text2, cp('\''));

    // <text1> ::= "" | <character1> <text1>
    add_rule(text1, empty());
    add_rule(text1, character1, text1);

    // <text2> ::= "" | <character2> <text2>
    add_rule(text2, empty());
    add_rule(text2, character2, text2);

    // <character> ::= <letter> | <digit> | <symbol>
    add_rule(character, letter);
    add_rule(character, digit);
    add_rule(character, symbol);

    // <letter> ::= "A" | "B" | "C" | "D" | "E" | "F" | "G" | "H" | "I" | "J" | "K" | "L" | "M" | "N" | "O" | "P" | "Q" | "R" | "S" | "T" | "U" | "V" | "W" | "X" | "Y" | "Z" | "a" | "b" | "c" | "d" | "e" | "f" | "g" | "h" | "i" | "j" | "k" | "l" | "m" | "n" | "o" | "p" | "q" | "r" | "s" | "t" | "u" | "v" | "w" | "x" | "y" | "z"
    add_rule(letter, cclass(N00B_P_BIC_ALPHA_ASCII));

    // <digit> ::= "0" | "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9"
    add_rule(digit, cclass(N00B_P_BIC_DIGIT));

    // <symbol> ::= "|" | " " | "!" | "#" | "$" | "%" | "&" | "(" | ")" | "*" | "+" | "," | "-" | "." | "/" | ":" | ";" | ">" | "=" | "<" | "?" | "@" | "[" | "\" | "]" | "^" | "_" | "`" | "{" | "}" | "~"
    add_rule(symbol, cset(cp('|'),
                          cp(' '),
                          cp('!'),
                          cp('#'),
                          cp('$'),
                          cp('%'),
                          cp('&'),
                          cp('('),
                          cp(')'),
                          cp('*'),
                          cp('+'),
                          cp(','),
                          cp('-'),
                          cp('.'),
                          cp('/'),
                          cp(':'),
                          cp(';'),
                          cp('>'),
                          cp('='),
                          cp('<'),
                          cp('?'),
                          cp('['),
                          cp('\\'),
                          cp(']'),
                          cp('^'),
                          cp('_'),
                          cp('`'),
                          cp('{'),
                          cp('}'),
                          cp('~')));
    
    // <character1> ::= <character> | "'"
    add_rule(character1, character);
    add_rule(character1, cp('\''));
    
    // <character2> ::= <character> | '"'
    add_rule(character2, character);
    add_rule(character2, cp('"'));

    // <rule-name> ::= <letter> | <rule-name> <rule-char>
    add_rule(rule_name, letter);
    add_rule(rule_name, rule_name, rule_char);

    // <rule-char> ::= <letter> | <digit> | "-"
    add_rule(rule_char, letter);
    add_rule(rule_char, digit);
    add_rule(rule_char, cp('-'));

    // set up walk actions for AST construction
    n00b_nonterm_set_walk_action(ntobj(syntax),      bnf_walk_syntax);
    n00b_nonterm_set_walk_action(ntobj(rule),        bnf_walk_rule);
    n00b_nonterm_set_walk_action(ntobj(expression),  bnf_walk_expression);
    n00b_nonterm_set_walk_action(ntobj(list),        bnf_walk_list);
    n00b_nonterm_set_walk_action(ntobj(term),        bnf_walk_term);
    n00b_nonterm_set_walk_action(ntobj(literal),     bnf_walk_literal);
    n00b_nonterm_set_walk_action(ntobj(text1),       bnf_walk_text);
    n00b_nonterm_set_walk_action(ntobj(text2),       bnf_walk_text);
    n00b_nonterm_set_walk_action(ntobj(character),   bnf_walk_char);
    n00b_nonterm_set_walk_action(ntobj(letter),      bnf_walk_char);
    n00b_nonterm_set_walk_action(ntobj(digit),       bnf_walk_char);
    n00b_nonterm_set_walk_action(ntobj(symbol),      bnf_walk_char);
    n00b_nonterm_set_walk_action(ntobj(character1),  bnf_walk_char);
    n00b_nonterm_set_walk_action(ntobj(character2),  bnf_walk_char);
    n00b_nonterm_set_walk_action(ntobj(rule_name),   bnf_walk_rule_name);
    n00b_nonterm_set_walk_action(ntobj(rule_char),   bnf_walk_char);
}

void *
n00b_bnf_parse(n00b_string_t *text, n00b_list_t **errors)
{
    n00b_bnf_init();
    n00b_parser_t *parser = n00b_new(n00b_type_parser(), n00b_bnf_grammar);
    n00b_list_t *errs = n00b_list(n00b_type_string());
    n00b_parse_string(parser, text, NULL);
    n00b_list_t *trees = n00b_parse_get_parses(parser);
    int num_trees = n00b_list_len(trees);
    if (!num_trees) {
        if (errors) {
            n00b_list_append(errs, n00b_cstring("Invalid BNF grammar."));
            *errors = errs;
        }
        return NULL;
    }

    if (num_trees > 1) {
        // ambiguous grammar
        n00b_unreachable();
    }

    n00b_tree_node_t *t = n00b_list_get(trees, 0, NULL);
    void *result = n00b_parse_tree_walk(parser, t, errs);

    if (n00b_list_len(errs)) {
        if (errors) {
            *errors = errs;
            return result;
        }
        return NULL;
    }

    return result;
}