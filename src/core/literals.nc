#include "n00b.h"

static n00b_dict_t *mod_map[ST_MAX] = {
    NULL,
};

// TODO: Change this when adding objects.
static int       container_bitfield_words = (N00B_NUM_BUILTIN_DTS + 63) / 64;
static uint64_t *list_types               = NULL;
static uint64_t *dict_types;
static uint64_t *set_types;
static uint64_t *tuple_types;
static uint64_t *all_container_types;

static inline void
no_more_containers()
{
    all_container_types = n00b_gc_array_value_alloc(uint64_t,
                                                    container_bitfield_words);

    for (int i = 0; i < container_bitfield_words; i++) {
        all_container_types[i] = list_types[i] | dict_types[i];
        all_container_types[i] |= set_types[i] | tuple_types[i];
    }
}

static void
initialize_container_bitfields()
{
    if (list_types == NULL) {
        list_types  = n00b_gc_array_value_alloc(uint64_t,
                                               container_bitfield_words);
        dict_types  = n00b_gc_array_value_alloc(uint64_t,
                                               container_bitfield_words);
        set_types   = n00b_gc_array_value_alloc(uint64_t,
                                              container_bitfield_words);
        tuple_types = n00b_gc_array_value_alloc(uint64_t,
                                                container_bitfield_words);
        n00b_gc_register_root(&list_types, 1);
        n00b_gc_register_root(&dict_types, 1);
        n00b_gc_register_root(&set_types, 1);
        n00b_gc_register_root(&tuple_types, 1);
        n00b_gc_register_root(&all_container_types, 1);
        n00b_gc_register_root(mod_map, ST_MAX);
    }
}

void
n00b_register_container_type(n00b_builtin_t    bi,
                             n00b_lit_syntax_t st,
                             bool              alt_syntax)
{
    initialize_container_bitfields();
    int word = ((int)bi) / 64;
    int bit  = ((int)bi) % 64;

    switch (st) {
    case ST_List:
        list_types[word] |= 1UL << bit;
        return;
    case ST_Dict:
        if (alt_syntax) {
            set_types[word] |= 1UL << bit;
        }
        else {
            dict_types[word] |= 1UL << bit;
        }
        return;
    case ST_Tuple:
        tuple_types[word] |= 1UL << bit;
        return;
    default:
        n00b_unreachable();
    }
}

void
n00b_register_literal(n00b_lit_syntax_t st, char *mod, n00b_builtin_t bi)
{
    n00b_string_t *u8mod = n00b_cstring(mod);
    if (!n00b_dict_add(mod_map[st],
                       u8mod,
                       (void *)(int64_t)bi)) {
        N00B_CRAISE("Duplicate literal modifier for this syntax type.");
    }

    switch (st) {
    case ST_List:
    case ST_Tuple:
        n00b_register_container_type(bi, st, false);
        return;
    case ST_Dict:
        if (bi == N00B_T_SET) {
            n00b_register_container_type(bi, st, true);
        }
        else {
            n00b_register_container_type(bi, st, false);
        }
        return;
    default:
        return;
    }
}

n00b_builtin_t
n00b_base_type_from_litmod(n00b_lit_syntax_t st, n00b_string_t *mod)
{
    n00b_builtin_t bi;
    bool           found = false;

    if (mod == NULL) {
        mod = n00b_cached_empty_string();
    }

    bi = (n00b_builtin_t)n00b_dict_get(mod_map[st], mod, &found);

    if (found) {
        return bi;
    }
    bi = (n00b_builtin_t)n00b_dict_get(mod_map[st],
                                       n00b_cached_star(),
                                       &found);
    if (found) {
        return bi;
    }

    return N00B_T_ERROR;
}

void
n00b_init_literal_handling()
{
    if (mod_map[0] == NULL) {
        for (int i = 0; i < ST_MAX; i++) {
            mod_map[i] = n00b_dict(n00b_type_string(), n00b_type_int());
        }

        n00b_gc_register_root(&mod_map[0], ST_MAX);

        n00b_register_literal(ST_Bool, "", N00B_T_BOOL);
        n00b_register_literal(ST_Base10, "", N00B_T_INT);
        n00b_register_literal(ST_Base10, "int", N00B_T_INT);
        n00b_register_literal(ST_Base10, "i64", N00B_T_INT);
        n00b_register_literal(ST_Base10, "u64", N00B_T_UINT);
        n00b_register_literal(ST_Base10, "uint", N00B_T_UINT);
        n00b_register_literal(ST_Base10, "u32", N00B_T_U32);
        n00b_register_literal(ST_Base10, "i32", N00B_T_I32);
        n00b_register_literal(ST_Base10, "i8", N00B_T_BYTE);
        n00b_register_literal(ST_Base10, "byte", N00B_T_BYTE);
        n00b_register_literal(ST_Base10, "char", N00B_T_CHAR);
        n00b_register_literal(ST_Base10, "f", N00B_T_F64);
        n00b_register_literal(ST_Base10, "f64", N00B_T_F64);
        n00b_register_literal(ST_Base10, "sz", N00B_T_SIZE);
        n00b_register_literal(ST_Base10, "size", N00B_T_SIZE);
        n00b_register_literal(ST_Hex, "", N00B_T_INT);
        n00b_register_literal(ST_Hex, "int", N00B_T_INT);
        n00b_register_literal(ST_Hex, "i64", N00B_T_INT);
        n00b_register_literal(ST_Hex, "u64", N00B_T_UINT);
        n00b_register_literal(ST_Hex, "uint", N00B_T_UINT);
        n00b_register_literal(ST_Hex, "u32", N00B_T_U32);
        n00b_register_literal(ST_Hex, "i32", N00B_T_I32);
        n00b_register_literal(ST_Hex, "i8", N00B_T_BYTE);
        n00b_register_literal(ST_Hex, "byte", N00B_T_BYTE);
        n00b_register_literal(ST_Hex, "char", N00B_T_CHAR);
        n00b_register_literal(ST_Float, "f", N00B_T_F64);
        n00b_register_literal(ST_Float, "f64", N00B_T_F64);
        n00b_register_literal(ST_2Quote, "", N00B_T_STRING);
        n00b_register_literal(ST_2Quote, "*", N00B_T_STRING);
        n00b_register_literal(ST_2Quote, "u8", N00B_T_STRING);
        n00b_register_literal(ST_2Quote, "utf8", N00B_T_STRING);
        n00b_register_literal(ST_2Quote, "r", N00B_T_STRING);
        n00b_register_literal(ST_2Quote, "rich", N00B_T_STRING);
        n00b_register_literal(ST_2Quote, "date", N00B_T_DATE);
        n00b_register_literal(ST_2Quote, "time", N00B_T_TIME);
        n00b_register_literal(ST_2Quote, "datetime", N00B_T_DATETIME);
        n00b_register_literal(ST_2Quote, "dur", N00B_T_DURATION);
        n00b_register_literal(ST_2Quote, "duration", N00B_T_DURATION);
        n00b_register_literal(ST_2Quote, "ip", N00B_T_IPV4);
        n00b_register_literal(ST_2Quote, "sz", N00B_T_SIZE);
        n00b_register_literal(ST_2Quote, "size", N00B_T_SIZE);
        n00b_register_literal(ST_2Quote, "url", N00B_T_STRING);
        n00b_register_literal(ST_1Quote, "", N00B_T_CHAR);
        n00b_register_literal(ST_1Quote, "c", N00B_T_CHAR);
        n00b_register_literal(ST_1Quote, "char", N00B_T_CHAR);
        n00b_register_literal(ST_1Quote, "b", N00B_T_BYTE);
        n00b_register_literal(ST_1Quote, "byte", N00B_T_BYTE);
        n00b_register_literal(ST_List, "", N00B_T_LIST);
        n00b_register_literal(ST_List, "l", N00B_T_LIST);
        n00b_register_literal(ST_List, "flow", N00B_T_TABLE);
        n00b_register_literal(ST_List, "table", N00B_T_TABLE);
        n00b_register_literal(ST_List, "ol", N00B_T_TABLE);
        n00b_register_literal(ST_List, "ul", N00B_T_TABLE);
        n00b_register_literal(ST_List, "list", N00B_T_LIST);
        // n00b_register_literal(ST_List, "f", N00B_T_FLIST);
        // n00b_register_literal(ST_List, "flist", N00B_T_FLIST);
        // n00b_register_literal(ST_List, "q", N00B_T_QUEUE);
        // n00b_register_literal(ST_List, "queue", N00B_T_QUEUE);
        n00b_register_literal(ST_List, "t", N00B_T_TREE);
        n00b_register_literal(ST_List, "tree", N00B_T_TREE);
        // n00b_register_literal(ST_List, "r", N00B_T_RING);
        // n00b_register_literal(ST_List, "ring", N00B_T_RING);
        // n00b_register_literal(ST_List, "log", N00B_T_LOGRING);
        // n00b_register_literal(ST_List, "logring", N00B_T_LOGRING);
        // n00b_register_literal(ST_List, "s", N00B_T_STACK);
        // n00b_register_literal(ST_List, "stack", N00B_T_STACK);
        n00b_register_literal(ST_Dict, "", N00B_T_DICT);
        n00b_register_literal(ST_Dict, "d", N00B_T_DICT);
        n00b_register_literal(ST_Dict, "dict", N00B_T_DICT);
        n00b_register_literal(ST_Dict, "s", N00B_T_SET);
        n00b_register_literal(ST_Dict, "set", N00B_T_SET);
        n00b_register_literal(ST_Tuple, "", N00B_T_TUPLE);
        n00b_register_literal(ST_Tuple, "t", N00B_T_TUPLE);
        n00b_register_literal(ST_Tuple, "tuple", N00B_T_TUPLE);
        n00b_register_literal(ST_Float, "", N00B_T_F64);
        no_more_containers();
    }
}

n00b_compile_error_t
n00b_parse_simple_lit(n00b_token_t      *tok,
                      n00b_lit_syntax_t *kptr,
                      n00b_string_t    **lm)
{
    n00b_init_literal_handling();

    n00b_string_t       *txt = n00b_token_raw_content(tok);
    n00b_string_t       *mod = tok->literal_modifier;
    n00b_lit_syntax_t    kind;
    n00b_compile_error_t err = n00b_err_no_error;

    if (lm != NULL) {
        *lm = mod;
    }

    switch (tok->kind) {
    case n00b_tt_int_lit:
        kind = ST_Base10;
        break;
    case n00b_tt_hex_lit:
        kind = ST_Hex;
        break;
    case n00b_tt_float_lit:
        kind = ST_Float;
        break;
    case n00b_tt_true:
    case n00b_tt_false:
        kind = ST_Bool;
        break;
    case n00b_tt_string_lit:
        kind = ST_2Quote;
        break;
    case n00b_tt_char_lit:
        kind = ST_1Quote;
        break;
    case n00b_tt_nil:
        // TODO-- one shared null value.
        tok->literal_value = n00b_new(n00b_type_void());
        return err;
    default:
        N00B_CRAISE("Token is not a simple literal");
    }

    n00b_builtin_t base_type = n00b_base_type_from_litmod(kind, mod);

    if (base_type == N00B_T_ERROR) {
        return n00b_err_parse_no_lit_mod_match;
    }

    n00b_vtable_t  *vtbl = (n00b_vtable_t *)n00b_base_type_info[base_type].vtable;
    n00b_literal_fn fn   = (n00b_literal_fn)vtbl->methods[N00B_BI_FROM_LITERAL];

    if (!fn) {
        return n00b_err_parse_no_lit_mod_match;
    }

    tok->literal_value = (*fn)(txt, kind, mod, &err);

    if (kptr != NULL) {
        *kptr = kind;
    }

    return err;
}

bool
n00b_fix_litmod(n00b_token_t *tok, n00b_pnode_t *pnode)
{
    // Precondition: pnode's type is concrete, simple, and no litmod
    // was spec'd.
    //
    // Our goal is to pick the first litmod for the syntax that matches
    // the type.

    n00b_dict_t   *d         = mod_map[tok->syntax];
    n00b_type_t   *t         = n00b_type_resolve(pnode->type);
    n00b_builtin_t base_type = t->base_index;
    n00b_list_t   *items     = n00b_dict_items(d);
    int            n         = n00b_list_len(items);

    for (int i = 0; i < n; i++) {
        n00b_tuple_t *t     = n00b_list_get(items, i, NULL);
        void         *key   = n00b_tuple_get(t, 0);
        void         *value = n00b_tuple_get(t, 1);

        if (base_type == (n00b_builtin_t)value) {
            n00b_string_t       *lm = key;
            n00b_vtable_t       *vtbl;
            n00b_literal_fn      fn;
            n00b_compile_error_t err = n00b_err_no_error;

            tok->literal_modifier = lm;

            vtbl = (n00b_vtable_t *)n00b_base_type_info[base_type].vtable;
            fn   = (n00b_literal_fn)vtbl->methods[N00B_BI_FROM_LITERAL];

            tok->literal_value = (*fn)(tok->text,
                                       tok->syntax,
                                       tok->literal_modifier,
                                       &err);
            if (err != n00b_err_no_error) {
                return false;
            }

            pnode->value = tok->literal_value;

            return true;
        }
    }

    return false;
}

bool
n00b_type_has_list_syntax(n00b_type_t *t)
{
    uint64_t bi      = t->base_index;
    int      word    = ((int)bi) / 64;
    int      bit     = ((int)bi) % 64;
    uint64_t to_test = 1UL << bit;

    return list_types[word] & to_test;
}

bool
n00b_type_has_dict_syntax(n00b_type_t *t)
{
    uint64_t bi      = t->base_index;
    int      word    = ((int)bi) / 64;
    int      bit     = ((int)bi) % 64;
    uint64_t to_test = 1UL << bit;

    return dict_types[word] & to_test;
}

bool
n00b_type_has_set_syntax(n00b_type_t *t)
{
    uint64_t bi      = t->base_index;
    int      word    = ((int)bi) / 64;
    int      bit     = ((int)bi) % 64;
    uint64_t to_test = 1UL << bit;

    return set_types[word] & to_test;
}

bool
n00b_type_has_tuple_syntax(n00b_type_t *t)
{
    uint64_t bi      = t->base_index;
    int      word    = ((int)bi) / 64;
    int      bit     = ((int)bi) % 64;
    uint64_t to_test = 1UL << bit;

    return tuple_types[word] & to_test;
}
int
n00b_get_num_bitfield_words()
{
    return container_bitfield_words;
}

uint64_t *
n00b_get_list_bitfield()
{
    n00b_init_literal_handling();

    uint64_t *result = n00b_gc_array_value_alloc(uint64_t,
                                                 container_bitfield_words);
    for (int i = 0; i < container_bitfield_words; i++) {
        result[i] = list_types[i];
    }

    return result;
}

uint64_t *
n00b_get_dict_bitfield()
{
    n00b_init_literal_handling();

    uint64_t *result = n00b_gc_array_value_alloc(uint64_t,
                                                 container_bitfield_words);
    for (int i = 0; i < container_bitfield_words; i++) {
        result[i] = dict_types[i];
    }

    return result;
}

uint64_t *
n00b_get_set_bitfield()
{
    n00b_init_literal_handling();

    uint64_t *result = n00b_gc_array_value_alloc(uint64_t,
                                                 container_bitfield_words);
    for (int i = 0; i < container_bitfield_words; i++) {
        result[i] = set_types[i];
    }

    return result;
}

uint64_t *
n00b_get_tuple_bitfield()
{
    n00b_init_literal_handling();

    uint64_t *result = n00b_gc_array_value_alloc(uint64_t,
                                                 container_bitfield_words);
    for (int i = 0; i < container_bitfield_words; i++) {
        result[i] = tuple_types[i];
    }

    return result;
}

uint64_t *
n00b_get_all_containers_bitfield()
{
    n00b_init_literal_handling();

    uint64_t *result = n00b_gc_array_value_alloc(uint64_t,
                                                 container_bitfield_words);
    for (int i = 0; i < container_bitfield_words; i++) {
        result[i] = all_container_types[i];
    }

    return result;
}

bool
n00b_partial_inference(n00b_type_t *t)
{
    tv_options_t *tsi = &t->options;

    for (int i = 0; i < container_bitfield_words; i++) {
        if (tsi->container_options[i] ^ all_container_types[i]) {
            return true;
        }
    }

    return false;
}

uint64_t *
n00b_get_no_containers_bitfield()
{
    n00b_init_literal_handling();

    return n00b_gc_array_value_alloc(uint64_t, container_bitfield_words);
}

bool
n00b_list_syntax_possible(n00b_type_t *t)
{
    tv_options_t *tsi = &t->options;

    for (int i = 0; i < container_bitfield_words; i++) {
        if (tsi->container_options[i] & list_types[i]) {
            return true;
        }
    }

    return false;
}

bool
n00b_dict_syntax_possible(n00b_type_t *t)
{
    tv_options_t *tsi = &t->options;

    for (int i = 0; i < container_bitfield_words; i++) {
        if (tsi->container_options[i] & dict_types[i]) {
            return true;
        }
    }

    return false;
}

bool
n00b_set_syntax_possible(n00b_type_t *t)
{
    tv_options_t *tsi = &t->options;

    for (int i = 0; i < container_bitfield_words; i++) {
        if (tsi->container_options[i] & set_types[i]) {
            return true;
        }
    }

    return false;
}

bool
n00b_tuple_syntax_possible(n00b_type_t *t)
{
    tv_options_t *tsi = &t->options;

    for (int i = 0; i < container_bitfield_words; i++) {
        if (tsi->container_options[i] & tuple_types[i]) {
            return true;
        }
    }

    return false;
}

void
n00b_remove_list_options(n00b_type_t *t)
{
    tv_options_t *tsi = &t->options;
    for (int i = 0; i < container_bitfield_words; i++) {
        tsi->container_options[i] &= ~(list_types[i]);
    }
}

void
n00b_remove_set_options(n00b_type_t *t)
{
    tv_options_t *tsi = &t->options;
    for (int i = 0; i < container_bitfield_words; i++) {
        tsi->container_options[i] &= ~(set_types[i]);
    }
}

void
n00b_remove_dict_options(n00b_type_t *t)
{
    tv_options_t *tsi = &t->options;
    for (int i = 0; i < container_bitfield_words; i++) {
        tsi->container_options[i] &= ~(dict_types[i]);
    }
}

void
n00b_remove_tuple_options(n00b_type_t *t)
{
    tv_options_t *tsi = &t->options;
    for (int i = 0; i < container_bitfield_words; i++) {
        tsi->container_options[i] &= ~(tuple_types[i]);
    }
}
