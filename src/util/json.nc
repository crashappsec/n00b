// This does NOT yet do streaming JSON. I wanted to get more experience w/
// the parser module which doesn't easily support streaming yet.
//
// But also, it's not a big modification to get it to support
// streaming, and this is a good excuse to do it (eventually).

#define N00B_USE_INTERNAL_API

#include "n00b.h"

// Naive translation of the official grammar; can optimize later.

n00b_grammar_t *n00b_json_grammar = NULL;

#define ntpi(s)           n00b_pitem_nonterm_raw(n00b_json_grammar, n00b_cstring(s))
#define ntobj(s)          n00b_pitem_get_ruleset(n00b_json_grammar, s)
#define add_jrule(x, ...) _add_jrule(x, __VA_ARGS__ __VA_OPT__(, ) 0)
#define cp(x)             n00b_pitem_terminal_cp(x)
#define empty()           n00b_new_pitem(N00B_P_NULL)
#define cclass(c)         n00b_pitem_builtin_raw(c)
#define cset(...)                            \
    n00b_pitem_choice_raw(n00b_json_grammar, \
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
_add_jrule(n00b_pitem_t *pi, ...)
{
    n00b_list_t  *rule = n00b_list(n00b_type_ref());
    va_list       args;
    n00b_pitem_t *item;

    va_start(args, pi);
    item = va_arg(args, n00b_pitem_t *);

    while (item) {
        n00b_list_append(rule, item);
        item = va_arg(args, n00b_pitem_t *);
    }

    n00b_ruleset_add_rule(n00b_json_grammar, ntobj(pi), rule, 0);
}
void *
json_walk_value(n00b_parse_node_t *n, n00b_list_t *l, void *ignore)
{
    // The grammar only accepts:
    // - sub-objects, which are one node,
    // - true
    // - false
    //
    // With sub-objects we just propogate them up.
    // So it's easy to differentiate by the number of kid nodes.

    switch (n00b_list_len(l)) {
    case 5:
        return n00b_box_bool(false);
    case 4:
        return n00b_box_bool(true);
    default:
        return n00b_list_get(l, 0, NULL);
    }
}

void *
json_walk_object(n00b_parse_node_t *node, n00b_list_t *l, void *ignore)
{
    n00b_list_t *pairs = n00b_list_get(l, 1, NULL);
    n00b_list_t *pair;
    n00b_dict_t *result;
    void        *value;
    int          n;

    if (!pairs) {
empty_object:
        return n00b_dict(n00b_type_string(), n00b_type_ref());
    }

    n = n00b_list_len(pairs);

    if (!n) {
        goto empty_object;
    }

    pair  = n00b_list_get(pairs, 0, NULL);
    value = n00b_list_get(pair, 1, NULL);

    n00b_ntype_t t = n00b_get_my_type(value);

    for (int i = 1; i < n; i++) {
        pair  = n00b_list_get(pairs, 0, NULL);
        value = n00b_list_get(pair, 1, NULL);

        if (!n00b_types_are_compat(t, n00b_get_my_type(value), NULL)) {
            result = n00b_dict(n00b_type_string(), n00b_type_ref());
            goto fill_dict;
        }
    }
    result = n00b_dict(n00b_type_string(), t);

fill_dict:
    // We populate these lists backwards due to the grammar.
    n00b_list_reverse(pairs);

    for (int i = 0; i < n; i++) {
        pair = n00b_list_get(pairs, i, NULL);
        n00b_dict_add(result,
                      n00b_list_get(pair, 0, NULL),
                      n00b_list_get(pair, 1, NULL));
    }

    return result;
}

void *
json_walk_container_items(n00b_parse_node_t *n, n00b_list_t *l, void *ignore)
{
    if (n00b_list_len(l) == 1) {
        return l;
    }

    n00b_list_t *result = n00b_list_get(l, 2, NULL);
    void        *item   = n00b_list_get(l, 0, NULL);

    n00b_list_append(result, item);

    return result;
}

void *
json_walk_member(n00b_parse_node_t *n, n00b_list_t *l, void *ignore)
{
    n00b_list_t *result = n00b_list(n00b_type_ref());
    n00b_list_append(result, n00b_list_get(l, 1, NULL));
    n00b_list_append(result, n00b_list_get(l, 4, NULL));

    return result;
}

void *
json_walk_array(n00b_parse_node_t *n, n00b_list_t *l, void *ignore)
{
    n00b_list_t *ret = n00b_list_get(l, 1, NULL);
    if (!ret) {
        ret = n00b_list(n00b_type_ref());
    }
    else {
        n00b_list_reverse(ret);
    }

    return ret;
}

void *
json_walk_element(n00b_parse_node_t *n, n00b_list_t *l, void *ignore)
{
    n00b_assert(n00b_list_len(l) == 3);
    return n00b_list_get(l, 1, NULL);
}

void *
json_walk_string(n00b_parse_node_t *n, n00b_list_t *l, void *ignore)
{
    return n00b_list_get(l, 1, NULL);
}

void *
json_walk_characters(n00b_parse_node_t *n, n00b_list_t *l, void *ignore)
{
    if (n00b_list_len(l) == 1) {
        return n00b_cached_empty_string();
    }
    return n00b_string_concat(n00b_list_get(l, 0, NULL),
                              n00b_list_get(l, 1, NULL));
}

void *
json_walk_character(n00b_parse_node_t *n, n00b_list_t *l, void *ignore)
{
    if (n00b_list_len(l) == 2) {
        return n00b_list_get(l, 1, NULL);
    }

    n00b_token_info_t *ti = n00b_list_get(l, 0, NULL);
    return ti->value;
}

void *
json_walk_escape(n00b_parse_node_t *n, n00b_list_t *l, void *ignore)
{
    if (n00b_list_len(l) == 5) {
        int64_t  cp;
        int64_t *fourbits = n00b_list_get(l, 1, NULL);
        cp                = (*fourbits) << 12;
        fourbits          = n00b_list_get(l, 2, NULL);
        cp |= ((*fourbits) << 8);
        fourbits = n00b_list_get(l, 3, NULL);
        cp |= ((*fourbits) << 4);
        fourbits = n00b_list_get(l, 4, NULL);
        cp |= *fourbits;
        return n00b_string_from_codepoint((n00b_codepoint_t)cp);
    }

    return n00b_list_get(l, 0, NULL);
}

void *
json_walk_hex(n00b_parse_node_t *n, n00b_list_t *l, void *ignore)
{
    n00b_token_info_t *ti = n00b_list_get(l, 0, NULL);
    n00b_string_t     *v  = ti->value;
    char               ch = v->data[0];
    int64_t            intval;

    if (ch >= '0' && ch <= '9') {
        intval = ch - '0';
    }
    else {
        if (ch >= 'a' && ch <= 'f') {
            intval = 0xa + (ch - 'a');
        }
        else {
            intval = 0xa + (ch - 'A');
        }
    }

    return n00b_box_i64(intval);
}

int64_t *
json_parse_int(n00b_string_t *n, n00b_list_t *errs)
{
    int64_t parsed;

    if (!n00b_parse_int64(n, &parsed)) {
        n00b_string_t *err = n00b_cformat(
            "Number «em»«#»«/» is too large to fit in a 64-bit integer.",
            n);
        n00b_list_append(errs, err);
        parsed = 0;
    }

    return n00b_box_i64(parsed);
}

void *
json_walk_number(n00b_parse_node_t *n, n00b_list_t *l, void *errs)
{
    n00b_string_t *nval     = n00b_list_get(l, 0, NULL);
    n00b_string_t *frac     = n00b_list_get(l, 1, NULL);
    n00b_string_t *expo     = n00b_list_get(l, 2, NULL);
    bool           got_frac = frac != NULL;
    bool           got_exp  = expo != NULL;
    double         dbl;

    if (!(got_frac || got_exp)) {
        return json_parse_int(nval, errs);
    }

    if (frac) {
        nval = n00b_string_concat(nval, frac);
    }

    if (expo) {
        nval = n00b_string_concat(nval, expo);
    }

    if (!n00b_parse_double(nval, &dbl)) {
        n00b_string_t *err = n00b_cformat(
            "Number «em»«#»«/» is out of bounds for a 64-bit double.",
            nval);
        n00b_list_append(errs, err);
        dbl = 0;
    }

    return n00b_box_double(dbl);
}

void *
json_walk_integer(n00b_parse_node_t *node, n00b_list_t *l, void *ignore)
{
    n00b_token_info_t *ti;

    switch (node->rule_index) {
    case 1:
        ti = n00b_list_get(l, 0, NULL);
        return n00b_string_concat(ti->value, n00b_list_get(l, 1, NULL));
    case 2:
        ti = n00b_list_get(l, 1, NULL);
        return n00b_cformat("-«#»", ti->value);
    case 3:
        ti = n00b_list_get(l, 1, NULL);
        return n00b_cformat("-«#»«#»", ti->value, n00b_list_get(l, 2, NULL));
    default:
        ti = n00b_list_get(l, 0, NULL);
        return ti->value;
    }
}

void *
json_walk_digits(n00b_parse_node_t *n, n00b_list_t *l, void *ignore)
{
    n00b_token_info_t *ti = n00b_list_get(l, 0, NULL);

    if (n00b_list_len(l) == 2) {
        return n00b_string_concat(ti->value, n00b_list_get(l, 1, NULL));
    }
    else {
        return ti->value;
    }
}

void *
json_walk_fraction(n00b_parse_node_t *n, n00b_list_t *l, void *ignore)
{
    if (n00b_list_len(l) != 2) {
        return NULL;
    }

    return n00b_cformat(".«#»", n00b_list_get(l, 1, NULL));
}

void *
json_walk_exponent(n00b_parse_node_t *n, n00b_list_t *l, void *ignore)
{
    if (n00b_list_len(l) != 3) {
        return NULL;
    }

    int64_t        sign   = (int64_t)n00b_list_get(l, 1, NULL);
    n00b_string_t *digits = n00b_list_get(l, 2, NULL);

    if (sign) {
        return n00b_cformat("-«#»", digits);
    }

    return digits;
}

void *
json_walk_sign(n00b_parse_node_t *n, n00b_list_t *l, void *ignore)
{
    n00b_token_info_t *ti = n00b_list_get(l, 0, NULL);
    if (ti && ti->tid == '-') {
        return ti->value;
    }
    return NULL;
}

void *
json_walk_ws(n00b_parse_node_t *n, n00b_list_t *l, void *ignore)
{
    return NULL;
}

static void once
n00b_json_init(void)
{
    n00b_gc_register_root(&n00b_json_grammar, 1);

    n00b_json_grammar = n00b_new(n00b_type_grammar(), detect_errors : false);

    n00b_pitem_t *value      = ntpi("value");
    n00b_pitem_t *object     = ntpi("object");
    n00b_pitem_t *members    = ntpi("members");
    n00b_pitem_t *member     = ntpi("member");
    n00b_pitem_t *array      = ntpi("array");
    n00b_pitem_t *elements   = ntpi("elements");
    n00b_pitem_t *element    = ntpi("element");
    n00b_pitem_t *string     = ntpi("string");
    n00b_pitem_t *characters = ntpi("characters");
    n00b_pitem_t *character  = ntpi("character");
    n00b_pitem_t *escape     = ntpi("escape");
    n00b_pitem_t *hex        = ntpi("hex");
    n00b_pitem_t *number     = ntpi("number");
    n00b_pitem_t *integer    = ntpi("integer");
    n00b_pitem_t *digits     = ntpi("digits");
    n00b_pitem_t *fraction   = ntpi("fraction");
    n00b_pitem_t *exponent   = ntpi("exponent");
    n00b_pitem_t *sign       = ntpi("sign");
    n00b_pitem_t *ws         = ntpi("ws");
    n00b_pitem_t *digit      = cclass(N00B_P_BIC_DIGIT);
    n00b_pitem_t *onenine    = cclass(N00B_P_BIC_NONZERO_ASCII_DIGIT);
    n00b_pitem_t *esc_chars  = cset(cp('"'),
                                   cp('\\'),
                                   cp('/'),
                                   cp('b'),
                                   cp('f'),
                                   cp('n'),
                                   cp('r'),
                                   cp('t'));

    n00b_grammar_set_default_start(n00b_json_grammar, ntobj(element));

    add_jrule(value, object);
    add_jrule(value, array);
    add_jrule(value, string);
    add_jrule(value, number);
    add_jrule(value, cp('t'), cp('r'), cp('u'), cp('e'));
    add_jrule(value, cp('f'), cp('a'), cp('l'), cp('s'), cp('e'));

    add_jrule(object, cp('{'), ws, cp('}'));
    add_jrule(object, cp('{'), members, cp('}'));

    add_jrule(members, member);
    add_jrule(members, member, cp(','), members);

    add_jrule(member, ws, string, ws, cp(':'), element);

    add_jrule(array, cp('['), ws, cp(']'));
    add_jrule(array, cp('['), elements, cp(']'));

    add_jrule(elements, element);
    add_jrule(elements, element, cp(','), elements);

    add_jrule(element, ws, value, ws);

    add_jrule(string, cp('"'), characters, cp('"'));

    add_jrule(characters, empty());
    add_jrule(characters, character, characters);

    add_jrule(character, cclass(N00B_P_BIC_JSON_string_CONTENTS));
    add_jrule(character, cp('\\'), escape);

    add_jrule(escape, esc_chars);
    add_jrule(escape, cp('u'), hex, hex, hex, hex);

    add_jrule(hex, cclass(N00B_P_BIC_HEX_DIGIT));

    add_jrule(number, integer, fraction, exponent);

    add_jrule(integer, digit);
    add_jrule(integer, onenine, digits);
    add_jrule(integer, cp('-'), digit);
    add_jrule(integer, cp('-'), onenine, digits);

    add_jrule(digits, digit);
    add_jrule(digits, digit, digits);

    add_jrule(fraction, empty());
    add_jrule(fraction, cp('.'), digits);

    add_jrule(exponent, empty());
    add_jrule(exponent, cp('E'), sign, digits);
    add_jrule(exponent, cp('e'), sign, digits);

    add_jrule(sign, empty());
    add_jrule(sign, cp('+'));
    add_jrule(sign, cp('-'));

    add_jrule(ws, empty());
    add_jrule(ws, cclass(N00B_P_BIC_SPACE));

    n00b_nonterm_set_walk_action(ntobj(value), json_walk_value);
    n00b_nonterm_set_walk_action(ntobj(object), json_walk_object);
    n00b_nonterm_set_walk_action(ntobj(members), json_walk_container_items);
    n00b_nonterm_set_walk_action(ntobj(member), json_walk_member);
    n00b_nonterm_set_walk_action(ntobj(array), json_walk_array);
    n00b_nonterm_set_walk_action(ntobj(elements), json_walk_container_items);
    n00b_nonterm_set_walk_action(ntobj(element), json_walk_element);
    n00b_nonterm_set_walk_action(ntobj(string), json_walk_string);
    n00b_nonterm_set_walk_action(ntobj(characters), json_walk_characters);
    n00b_nonterm_set_walk_action(ntobj(character), json_walk_character);
    n00b_nonterm_set_walk_action(ntobj(escape), json_walk_escape);
    n00b_nonterm_set_walk_action(ntobj(hex), json_walk_hex);
    n00b_nonterm_set_walk_action(ntobj(number), json_walk_number);
    n00b_nonterm_set_walk_action(ntobj(integer), json_walk_integer);
    n00b_nonterm_set_walk_action(ntobj(digits), json_walk_digits);
    n00b_nonterm_set_walk_action(ntobj(fraction), json_walk_fraction);
    n00b_nonterm_set_walk_action(ntobj(exponent), json_walk_exponent);
    n00b_nonterm_set_walk_action(ntobj(sign), json_walk_sign);
    n00b_nonterm_set_walk_action(ntobj(ws), json_walk_ws);
}

void *
n00b_json_parse(n00b_string_t *s, n00b_list_t **err_out)
{
    n00b_json_init();

    // This is showing off some bugs in repr :/
    // n00b_print(n00b_grammar_format(n00b_json_grammar));

    n00b_parser_t *parser = n00b_new(n00b_type_parser(), n00b_json_grammar);
    n00b_list_t   *errs   = n00b_list(n00b_type_string());

    n00b_parse_string(parser, s, NULL);

    n00b_list_t *trees = n00b_parse_get_parses(parser);
    int          l     = n00b_list_len(trees);

    if (!l) {
        if (err_out) {
            n00b_list_append(errs, n00b_cstring("Invalid JSON."));
            *err_out = errs;
        }
        return NULL;
    }

    if (l > 1) {
        // Ambiguous grammar.
        n00b_unreachable();
    }

    n00b_tree_node_t *t      = n00b_list_get(trees, 0, NULL);
    void             *result = n00b_parse_tree_walk(parser, t, errs);

    if (n00b_list_len(errs)) {
        if (err_out) {
            *err_out = errs;
            return result;
        }
        else {
            return NULL;
        }
    }

    return result;
}

static inline bool
validate_one_level(n00b_ntype_t t)
{
    if (n00b_type_is_string(t)
        || n00b_type_is_box(t)
        || n00b_type_is_list(t)
        || n00b_type_is_dict(t)) {
        return true;
    }

    return false;
}

static bool
validate_types_for_json(void *obj)
{
    n00b_ntype_t t;

    // Items inside dicts
    if (!n00b_in_heap(obj)) {
        return false; // Thou must pass an object.
    }

    t = n00b_get_my_type(obj);
    if (!validate_one_level(t)) {
        return false;
    }

    if (n00b_type_is_list(t)) {
        t = n00b_type_get_param(t, 0);
        if (n00b_type_is_int_type(t)
            || n00b_type_is_float_type(t)
            || n00b_type_is_string(t)) {
            return true;
        }

        if (n00b_type_is_ref(t)
            || n00b_type_is_list(t)
            || n00b_type_is_dict(t)) {
            n00b_list_t *l = obj;
            int          n = n00b_list_len(l);
            for (int i = 0; i < n; i++) {
                if (!validate_types_for_json(n00b_list_get(l, i, NULL))) {
                    return false;
                }
            }
            return true;
        }
        else {
            return false;
        }
    }

    if (n00b_type_is_dict(t)) {
        n00b_ntype_t sub = n00b_type_get_param(t, 0);

        if (!n00b_type_is_string(sub)) {
            return false;
        }

        sub = n00b_type_get_param(t, 1);
        if (n00b_type_is_int_type(sub)
            || n00b_type_is_float_type(t)
            || n00b_type_is_string(t)) {
            return true;
        }

        return validate_types_for_json(n00b_dict_values(obj));
    }

    return true;
}

n00b_string_t *
n00b_to_json(void *obj)
{
    if (!validate_types_for_json(obj)) {
        return NULL;
    }

    return n00b_cformat("«#»", obj);
}
