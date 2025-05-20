#pragma once

#include "n00b.h"

extern n00b_type_t *n00b_type_resolve(n00b_type_t *);
extern bool         n00b_type_is_concrete(n00b_type_t *);
extern n00b_type_t *n00b_type_copy(n00b_type_t *);
extern n00b_type_t *n00b_get_builtin_type(n00b_builtin_t);
extern n00b_type_t *n00b_unify(n00b_type_t *, n00b_type_t *);

#if defined(N00B_GC_STATS) || defined(N00B_DEBUG)
extern n00b_type_t *_n00b_type_flist(n00b_type_t *, char *, int);
extern n00b_type_t *_n00b_type_list(n00b_type_t *, char *, int);
extern n00b_type_t *_n00b_type_tree(n00b_type_t *, char *, int);
extern n00b_type_t *_n00b_type_queue(n00b_type_t *, char *, int);
extern n00b_type_t *_n00b_type_ring(n00b_type_t *, char *, int);
extern n00b_type_t *_n00b_type_stack(n00b_type_t *, char *, int);
extern n00b_type_t *_n00b_type_set(n00b_type_t *, char *, int);
#define n00b_type_flist(x) _n00b_type_flist(x, __FILE__, __LINE__)
#define n00b_type_list(x)  _n00b_type_list(x, __FILE__, __LINE__)
#define n00b_type_tree(x)  _n00b_type_tree(x, __FILE__, __LINE__)
#define n00b_type_queue(x) _n00b_type_queue(x, __FILE__, __LINE__)
#define n00b_type_ring(x)  _n00b_type_ring(x, __FILE__, __LINE__)
#define n00b_type_stack(x) _n00b_type_stack(x, __FILE__, __LINE__)
#define n00b_type_set(x)   _n00b_type_set(x, __FILE__, __LINE__)
#else
extern n00b_type_t *n00b_type_flist(n00b_type_t *);
extern n00b_type_t *n00b_type_list(n00b_type_t *);
extern n00b_type_t *n00b_type_tree(n00b_type_t *);
extern n00b_type_t *n00b_type_queue(n00b_type_t *);
extern n00b_type_t *n00b_type_ring(n00b_type_t *);
extern n00b_type_t *n00b_type_stack(n00b_type_t *);
extern n00b_type_t *n00b_type_set(n00b_type_t *);
#endif
extern n00b_type_t *n00b_type_box(n00b_type_t *);
extern n00b_type_t *n00b_type_dict(n00b_type_t *, n00b_type_t *);

extern n00b_type_t     *n00b_type_tuple(int64_t, ...);
extern n00b_type_t     *n00b_type_tuple_from_list(n00b_list_t *);
extern n00b_type_t     *n00b_type_fn(n00b_type_t *, n00b_list_t *, bool);
extern n00b_type_t     *n00b_type_fn_va(n00b_type_t *, int64_t, ...);
extern n00b_type_t     *n00b_type_varargs_fn(n00b_type_t *, int64_t, ...);
extern n00b_type_t     *n00b_get_promotion_type(n00b_type_t *,
                                                n00b_type_t *,
                                                int *);
extern n00b_type_t     *n00b_new_typevar(void);
extern void             n00b_initialize_global_types(void);
extern n00b_type_hash_t n00b_calculate_type_hash(n00b_type_t *node);
extern uint64_t        *n00b_get_list_bitfield(void);
extern uint64_t        *n00b_get_dict_bitfield(void);
extern uint64_t        *n00b_get_set_bitfield(void);
extern uint64_t        *n00b_get_tuple_bitfield(void);
extern uint64_t        *n00b_get_all_containers_bitfield(void);
extern uint64_t        *n00b_get_no_containers_bitfield(void);
extern int              n00b_get_num_bitfield_words(void);
extern bool             n00b_partial_inference(n00b_type_t *);
extern bool             n00b_list_syntax_possible(n00b_type_t *);
extern bool             n00b_dict_syntax_possible(n00b_type_t *);
extern bool             n00b_set_syntax_possible(n00b_type_t *);
extern bool             n00b_tuple_syntax_possible(n00b_type_t *);
extern void             n00b_remove_list_options(n00b_type_t *);
extern void             n00b_remove_dict_options(n00b_type_t *);
extern void             n00b_remove_set_options(n00b_type_t *);
extern void             n00b_remove_tuple_options(n00b_type_t *);
extern bool             n00b_type_has_list_syntax(n00b_type_t *);
extern bool             n00b_type_has_dict_syntax(n00b_type_t *);
extern bool             n00b_type_has_set_syntax(n00b_type_t *);
extern bool             n00b_type_has_tuple_syntax(n00b_type_t *);

static inline void
n00b_remove_all_container_options(n00b_type_t *t)
{
    n00b_remove_list_options(t);
    n00b_remove_dict_options(t);
    n00b_remove_set_options(t);
    n00b_remove_tuple_options(t);
}

extern n00b_type_exact_result_t
n00b_type_cmp_exact(n00b_type_t *, n00b_type_t *);

static inline n00b_dt_info_t *
n00b_internal_type_base(n00b_type_t *n)
{
    return (n00b_dt_info_t *)&n00b_base_type_info[n->base_index];
}

static inline char *
n00b_internal_type_name(n00b_type_t *n)
{
    return (char *)n00b_base_type_info[n->base_index].name;
}

static inline n00b_dt_kind_t
n00b_type_get_kind(n00b_type_t *n)
{
    return n00b_internal_type_base(n00b_type_resolve(n))->dt_kind;
}

static inline n00b_list_t *
n00b_type_get_params(n00b_type_t *n)
{
    return n00b_type_resolve(n)->items;
}

static inline int
n00b_type_get_num_params(n00b_type_t *n)
{
    return n00b_list_len(n00b_type_resolve(n)->items);
}

static bool
n00b_ensure_type(n00b_type_t *t)
{
    if (!t) {
        return false;
    }
    if (((int64_t)t->base_index) < 0) {
        return false;
    }
    if (t->base_index >= N00B_NUM_BUILTIN_DTS) {
        return false;
    }
    n00b_type_t *r = n00b_type_resolve(t);

    if (r == t) {
        return true;
    }

    return n00b_ensure_type(r);
}

static inline bool
n00b_type_is_bool(n00b_type_t *t)
{
    if (!n00b_ensure_type(t)) {
        return false;
    }
    return n00b_type_resolve(t)->typeid == N00B_T_BOOL;
}

static inline bool
n00b_type_is_ref(n00b_type_t *t)
{
    if (!n00b_ensure_type(t)) {
        return false;
    }

    return n00b_type_resolve(t)->typeid == N00B_T_REF;
}

static inline bool
n00b_type_is_locked(n00b_type_t *t)
{
    if (!n00b_ensure_type(t)) {
        return false;
    }

    return n00b_type_resolve(t)->flags & N00B_FN_TY_LOCK;
}

static inline bool
n00b_type_is_tuple(n00b_type_t *t)
{
    if (!n00b_ensure_type(t)) {
        return false;
    }

    return n00b_type_get_kind(t) == N00B_DT_KIND_tuple;
}

static inline bool
n00b_type_is_list(n00b_type_t *t)
{
    if (!n00b_ensure_type(t)) {
        return false;
    }

    return n00b_type_resolve(t)->base_index == N00B_T_LIST;
}

static inline bool
n00b_type_is_dict(n00b_type_t *t)
{
    if (!n00b_ensure_type(t)) {
        return false;
    }

    return n00b_type_resolve(t)->base_index == N00B_T_DICT;
}

static inline bool
n00b_type_is_lock(n00b_type_t *t)
{
    if (!n00b_ensure_type(t)) {
        return false;
    }

    return n00b_type_resolve(t)->base_index == N00B_T_LOCK;
}

static inline bool
n00b_type_is_condition(n00b_type_t *t)
{
    if (!n00b_ensure_type(t)) {
        return false;
    }

    return n00b_type_resolve(t)->base_index == N00B_T_CONDITION;
}

static inline bool
n00b_type_is_stream(n00b_type_t *t)
{
    if (!n00b_ensure_type(t)) {
        return false;
    }

    return n00b_type_resolve(t)->base_index == N00B_T_STREAM;
}

static inline bool
n00b_type_is_text_element(n00b_type_t *t)
{
    if (!n00b_ensure_type(t)) {
        return false;
    }

    return n00b_type_resolve(t)->base_index == N00B_T_TEXT_ELEMENT;
}

static inline bool
n00b_type_is_theme(n00b_type_t *t)
{
    if (!n00b_ensure_type(t)) {
        return false;
    }

    return n00b_type_resolve(t)->base_index == N00B_T_THEME;
}

static inline bool
n00b_type_is_box_props(n00b_type_t *t)
{
    if (!n00b_ensure_type(t)) {
        return false;
    }

    return n00b_type_resolve(t)->base_index == N00B_T_BOX_PROPS;
}

static inline bool
n00b_type_is_message(n00b_type_t *t)
{
    if (!n00b_ensure_type(t)) {
        return false;
    }

    return n00b_type_resolve(t)->base_index == N00B_T_MESSAGE;
}

static inline bool
n00b_type_is_bytering(n00b_type_t *t)
{
    if (!n00b_ensure_type(t)) {
        return false;
    }

    return n00b_type_resolve(t)->base_index == N00B_T_BYTERING;
}

static inline bool
n00b_type_is_keyword(n00b_type_t *t)
{
    if (!n00b_ensure_type(t)) {
        return false;
    }
    return n00b_type_resolve(t)->base_index == N00B_T_KEYWORD;
}

static inline bool
n00b_type_is_set(n00b_type_t *t)
{
    if (!n00b_ensure_type(t)) {
        return false;
    }

    return n00b_type_resolve(t)->base_index == N00B_T_SET;
}

static inline void
n00b_lock_type(n00b_type_t *t)
{
    n00b_type_resolve(t)->flags |= N00B_FN_TY_LOCK;
}

static inline void
n00b_type_unlock(n00b_type_t *t)
{
    t->flags &= ~N00B_FN_TY_LOCK;
}

static inline n00b_type_t *
n00b_type_get_param(n00b_type_t *t, int i)
{
    if (t && t->items) {
        return n00b_private_list_get(t->items, i, NULL);
    }
    else {
        return NULL;
    }
}

static inline n00b_type_t *
n00b_type_get_last_param(n00b_type_t *n)
{
    return n00b_type_get_param(n, n00b_type_get_num_params(n) - 1);
}

static inline n00b_type_t *
n00b_type_get_list_param(n00b_type_t *t)
{
    if (!t || !n00b_type_is_list(t)) {
        return NULL;
    }

    return n00b_type_get_param(t, 0);
}

static inline n00b_dt_info_t *
n00b_type_get_data_type_info(n00b_type_t *t)
{
    t = n00b_type_resolve(t);
    return n00b_internal_type_base(t);
}

static inline bool
n00b_type_is_mutable(n00b_type_t *t)
{
    if (!n00b_ensure_type(t)) {
        return false;
    }

    return n00b_type_get_data_type_info(t)->mutable;
}

static inline int64_t
n00b_get_base_type_id(const n00b_obj_t obj)
{
    return n00b_get_my_type(obj)->base_index;
}

extern n00b_type_t *n00b_bi_types[N00B_NUM_BUILTIN_DTS];

static inline bool
n00b_type_is_error(n00b_type_t *t)
{
    if (!n00b_ensure_type(t)) {
        return false;
    }

    return n00b_type_resolve(t)->typeid == N00B_T_ERROR;
}

static inline n00b_type_t *
n00b_type_error(void)
{
    return n00b_bi_types[N00B_T_ERROR];
}

static inline n00b_type_t *
n00b_type_void(void)
{
    return n00b_bi_types[N00B_T_VOID];
}

static inline n00b_type_t *
n00b_type_nil(void)
{
    return n00b_bi_types[N00B_T_NIL];
}

static inline n00b_type_t *
n00b_type_bool(void)
{
    return n00b_bi_types[N00B_T_BOOL];
}

static inline n00b_type_t *
n00b_type_i8(void)
{
    return n00b_bi_types[N00B_T_I8];
}

static inline n00b_type_t *
n00b_type_u8(void)
{
    return n00b_bi_types[N00B_T_BYTE];
}

static inline n00b_type_t *
n00b_type_byte(void)
{
    return n00b_bi_types[N00B_T_BYTE];
}

static inline n00b_type_t *
n00b_type_i32(void)
{
    return n00b_bi_types[N00B_T_I32];
}

static inline n00b_type_t *
n00b_type_u32(void)
{
    return n00b_bi_types[N00B_T_CHAR];
}

static inline n00b_type_t *
n00b_type_char(void)
{
    return n00b_bi_types[N00B_T_CHAR];
}

static inline n00b_type_t *
n00b_type_i64(void)
{
    return n00b_bi_types[N00B_T_INT];
}

static inline n00b_type_t *
n00b_type_int(void)
{
    return n00b_bi_types[N00B_T_INT];
}

static inline n00b_type_t *
n00b_type_u64(void)
{
    return n00b_bi_types[N00B_T_UINT];
}

static inline n00b_type_t *
n00b_type_uint(void)
{
    return n00b_bi_types[N00B_T_UINT];
}

static inline n00b_type_t *
n00b_type_f32(void)
{
    return n00b_bi_types[N00B_T_F32];
}

static inline n00b_type_t *
n00b_type_f64(void)
{
    return n00b_bi_types[N00B_T_F64];
}

static inline n00b_type_t *
n00b_type_float(void)
{
    return n00b_bi_types[N00B_T_F64];
}

static inline n00b_type_t *
n00b_type_buffer(void)
{
    return n00b_bi_types[N00B_T_BUFFER];
}

static inline n00b_type_t *
n00b_type_typespec(void)
{
    return n00b_bi_types[N00B_T_TYPESPEC];
}

static inline n00b_type_t *
n00b_type_ip(void)
{
    return n00b_bi_types[N00B_T_IPV4];
}

static inline n00b_type_t *
n00b_type_ipv4(void)
{
    return n00b_bi_types[N00B_T_IPV4];
}

static inline n00b_type_t *
n00b_type_net_addr(void)
{
    return n00b_bi_types[N00B_T_IPV4];
}

static inline n00b_type_t *
n00b_type_duration(void)
{
    return n00b_bi_types[N00B_T_DURATION];
}

static inline n00b_type_t *
n00b_type_size(void)
{
    return n00b_bi_types[N00B_T_SIZE];
}

static inline n00b_type_t *
n00b_type_datetime(void)
{
    return n00b_bi_types[N00B_T_DATETIME];
}

static inline n00b_type_t *
n00b_type_date(void)
{
    return n00b_bi_types[N00B_T_DATE];
}

static inline n00b_type_t *
n00b_type_time(void)
{
    return n00b_bi_types[N00B_T_TIME];
}

static inline n00b_type_t *
n00b_type_url(void)
{
    return n00b_bi_types[N00B_T_URL];
}

static inline n00b_type_t *
n00b_type_flags(void)
{
    return n00b_bi_types[N00B_T_FLAGS];
}

static inline n00b_type_t *
n00b_type_callback(void)
{
    return n00b_bi_types[N00B_T_CALLBACK];
}
static inline n00b_type_t *
n00b_type_hash(void)
{
    return n00b_bi_types[N00B_T_SHA];
}

static inline n00b_type_t *
n00b_type_exception(void)
{
    return n00b_bi_types[N00B_T_EXCEPTION];
}

static inline n00b_type_t *
n00b_type_logring(void)
{
    return n00b_bi_types[N00B_T_LOGRING];
}

static inline n00b_type_t *
n00b_type_mixed(void)
{
    return n00b_bi_types[N00B_T_GENERIC];
}

static inline n00b_type_t *
n00b_type_ref(void)
{
    return n00b_bi_types[N00B_T_REF];
}

static inline n00b_type_t *
n00b_type_true_ref(void)
{
    return n00b_bi_types[N00B_T_TRUE_REF];
}

static inline n00b_type_t *
n00b_type_internal(void)
{
    return n00b_bi_types[N00B_T_INTERNAL];
}

static inline n00b_type_t *
n00b_type_kargs(void)
{
    return n00b_bi_types[N00B_T_KEYWORD];
}

static inline n00b_type_t *
n00b_type_parse_node(void)
{
    return n00b_bi_types[N00B_T_PARSE_NODE];
}

static inline n00b_type_t *
n00b_type_bit(void)
{
    return n00b_bi_types[N00B_T_BIT];
}

static inline n00b_type_t *
n00b_type_http(void)
{
    return n00b_bi_types[N00B_T_HTTP];
}

static inline n00b_type_t *
n00b_type_parser(void)
{
    return n00b_bi_types[N00B_T_PARSER];
}

static inline n00b_type_t *
n00b_type_grammar(void)
{
    return n00b_bi_types[N00B_T_GRAMMAR];
}

static inline n00b_type_t *
n00b_type_terminal(void)
{
    return n00b_bi_types[N00B_T_TERMINAL];
}

static inline n00b_type_t *
n00b_type_ruleset(void)
{
    return n00b_bi_types[N00B_T_RULESET];
}

static inline n00b_type_t *
n00b_type_gopt_parser(void)
{
    return n00b_bi_types[N00B_T_GOPT_PARSER];
}

static inline n00b_type_t *
n00b_type_gopt_command(void)
{
    return n00b_bi_types[N00B_T_GOPT_COMMAND];
}

static inline n00b_type_t *
n00b_type_gopt_option(void)
{
    return n00b_bi_types[N00B_T_GOPT_OPTION];
}

static inline n00b_type_t *
n00b_type_lock(void)
{
    return n00b_bi_types[N00B_T_LOCK];
}

static inline n00b_type_t *
n00b_type_condition(void)
{
    return n00b_bi_types[N00B_T_CONDITION];
}

static inline n00b_type_t *
n00b_type_stream(void)
{
    return n00b_bi_types[N00B_T_STREAM];
}

static inline n00b_type_t *
n00b_type_message(void)
{
    return n00b_bi_types[N00B_T_MESSAGE];
}

static inline n00b_type_t *
n00b_type_bytering(void)
{
    return n00b_bi_types[N00B_T_BYTERING];
}

static inline n00b_type_t *
n00b_type_text_element(void)
{
    return n00b_bi_types[N00B_T_TEXT_ELEMENT];
}

static inline n00b_type_t *
n00b_type_box_props(void)
{
    return n00b_bi_types[N00B_T_BOX_PROPS];
}

static inline n00b_type_t *
n00b_type_theme(void)
{
    return n00b_bi_types[N00B_T_THEME];
}

static inline n00b_type_t *
n00b_type_string(void)
{
    return n00b_bi_types[N00B_T_STRING];
}

static inline n00b_type_t *
n00b_type_table(void)
{
    return n00b_bi_types[N00B_T_TABLE];
}

static inline n00b_type_t *
n00b_type_regex(void)
{
    return n00b_bi_types[N00B_T_REGEX];
}

static inline n00b_type_t *
n00b_type_session(void)
{
    return n00b_bi_types[N00B_T_SESSION];
}

static inline n00b_type_t *
n00b_type_session_state(void)
{
    return n00b_bi_types[N00B_T_SESSION_STATE];
}

static inline n00b_type_t *
n00b_type_session_trigger(void)
{
    return n00b_bi_types[N00B_T_SESSION_TRIGGER];
}

static inline n00b_type_t *
n00b_merge_types(n00b_type_t *t1, n00b_type_t *t2, int *warning)
{
    n00b_type_t *result = n00b_unify(t1, t2);

    if (!n00b_type_is_error(result)) {
        return result;
    }

    if (warning != NULL) {
        return n00b_get_promotion_type(t1, t2, warning);
    }

    return n00b_type_error();
}

static inline n00b_type_t *
n00b_type_any_list(n00b_type_t *item_type)
{
    n00b_type_t *result               = n00b_new(n00b_type_typespec(),
                                   N00B_T_GENERIC);
    result->options.container_options = n00b_get_list_bitfield();
    result->items                     = n00b_list(n00b_type_typespec());

    if (item_type == NULL) {
        item_type = n00b_new_typevar();
    }

    n00b_list_append(result->items, item_type);

    return result;
}

static inline n00b_type_t *
n00b_type_any_dict(n00b_type_t *key, n00b_type_t *value)
{
    n00b_type_t *result = n00b_new(n00b_type_typespec(),
                                   N00B_T_GENERIC);

    result->options.container_options = n00b_get_dict_bitfield();
    result->items                     = n00b_new(n00b_type_list(n00b_type_typespec()));

    if (key == NULL) {
        key = n00b_new_typevar();
    }

    if (value == NULL) {
        value = n00b_new_typevar();
    }

    n00b_list_append(result->items, key);
    n00b_list_append(result->items, value);

    return result;
}

static inline n00b_type_t *
n00b_type_any_set(n00b_type_t *item_type)
{
    n00b_type_t *result               = n00b_new(n00b_type_typespec(),
                                   N00B_T_GENERIC);
    result->options.container_options = n00b_get_set_bitfield();
    result->items                     = n00b_new(n00b_type_list(n00b_type_typespec()));

    if (item_type == NULL) {
        item_type = n00b_new_typevar();
    }

    n00b_list_append(result->items, item_type);

    return result;
}

static inline bool
n00b_types_are_compat(n00b_type_t *t1, n00b_type_t *t2, int *warning)
{
    t1 = n00b_type_copy(t1);
    t2 = n00b_type_copy(t2);

    if (!n00b_type_is_error(n00b_unify(t1, t2))) {
        return true;
    }

    if (!warning) {
        return false;
    }

    if (!n00b_type_is_error(n00b_get_promotion_type(t1, t2, warning))) {
        return true;
    }

    return false;
}

static inline bool
n00b_obj_type_check(const n00b_obj_t *obj, n00b_type_t *t2, int *warn)
{
    return n00b_types_are_compat(n00b_get_my_type((n00b_type_t *)obj), t2, warn);
}

static inline bool
n00b_type_is_box(n00b_type_t *t)
{
    if (!n00b_ensure_type(t)) {
        return false;
    }

    t = n00b_type_resolve(t);
    switch (t->base_index) {
    case N00B_T_BOX_BOOL:
    case N00B_T_BOX_I8:
    case N00B_T_BOX_BYTE:
    case N00B_T_BOX_I32:
    case N00B_T_BOX_CHAR:
    case N00B_T_BOX_U32:
    case N00B_T_BOX_INT:
    case N00B_T_BOX_UINT:
    case N00B_T_BOX_F32:
    case N00B_T_BOX_F64:
        return true;
    default:
        return false;
    }
}

static inline n00b_type_t *
n00b_type_unbox(n00b_type_t *t)
{
    switch (t->base_index) {
    case N00B_T_BOX_BOOL:
        return n00b_bi_types[N00B_T_BOOL];
    case N00B_T_BOX_I8:
        return n00b_bi_types[N00B_T_I8];
    case N00B_T_BOX_BYTE:
        return n00b_bi_types[N00B_T_BYTE];
    case N00B_T_BOX_I32:
        return n00b_bi_types[N00B_T_I32];
    case N00B_T_BOX_CHAR:
        return n00b_bi_types[N00B_T_CHAR];
    case N00B_T_BOX_U32:
        return n00b_bi_types[N00B_T_U32];
    case N00B_T_BOX_INT:
        return n00b_bi_types[N00B_T_INT];
    case N00B_T_BOX_UINT:
        return n00b_bi_types[N00B_T_UINT];
    case N00B_T_BOX_F32:
        return n00b_bi_types[N00B_T_F32];
    case N00B_T_BOX_F64:
        return n00b_bi_types[N00B_T_F64];
    default:
        return t;
    }
}

static inline bool
n00b_type_is_int_type(n00b_type_t *t)
{
    if (!n00b_ensure_type(t)) {
        return false;
    }

    t = n00b_type_unbox(n00b_type_resolve(t));

    switch (t->typeid) {
    case N00B_T_I8:
    case N00B_T_BYTE:
    case N00B_T_I32:
    case N00B_T_CHAR:
    case N00B_T_U32:
    case N00B_T_INT:
    case N00B_T_UINT:
        return true;
    default:
        return false;
    }
}

static inline bool
n00b_type_is_float_type(n00b_type_t *t)
{
    if (!n00b_ensure_type(t)) {
        return false;
    }

    t = n00b_type_unbox(n00b_type_resolve(t));

    switch (t->typeid) {
    case N00B_T_F64:
    case N00B_T_F32:
        return true;
    default:
        return false;
    }
}

static inline bool
n00b_type_is_signed(n00b_type_t *t)
{
    if (!n00b_ensure_type(t)) {
        return false;
    }

    t = n00b_type_unbox(n00b_type_resolve(t));

    switch (t->typeid) {
    case N00B_T_I8:
    case N00B_T_I32:
    case N00B_T_CHAR:
    case N00B_T_INT:
        return true;
    default:
        return false;
    }
}

static inline bool
n00b_type_is_tvar(n00b_type_t *t)
{
    if (!n00b_ensure_type(t)) {
        return false;
    }
    t = n00b_type_resolve(t);
    return (n00b_type_get_kind(t) == N00B_DT_KIND_type_var);
}

static inline bool
n00b_type_is_void(n00b_type_t *t)
{
    if (!n00b_ensure_type(t)) {
        return false;
    }
    t = n00b_type_resolve(t);
    return t->typeid == N00B_T_VOID;
}

static inline bool
n00b_type_is_nil(n00b_type_t *t)
{
    if (!n00b_ensure_type(t)) {
        return false;
    }
    t = n00b_type_resolve(t);
    return t->typeid == N00B_T_VOID;
}

static inline bool
n00b_type_is_string(n00b_type_t *t)
{
    if (!n00b_ensure_type(t)) {
        return false;
    }

    t = n00b_type_resolve(t);
    return t->typeid == N00B_T_STRING;
}

static inline bool
n00b_type_is_table(n00b_type_t *t)
{
    if (!n00b_ensure_type(t)) {
        return false;
    }
    t = n00b_type_resolve(t);
    return t->typeid == N00B_T_TABLE;
}

static inline bool
n00b_type_is_regex(n00b_type_t *t)
{
    if (!n00b_ensure_type(t)) {
        return false;
    }
    t = n00b_type_resolve(t);
    return t->typeid == N00B_T_REGEX;
}

static inline bool
n00b_type_is_session(n00b_type_t *t)
{
    if (!n00b_ensure_type(t)) {
        return false;
    }
    t = n00b_type_resolve(t);
    return t->typeid == N00B_T_SESSION;
}

static inline bool
n00b_type_is_session_state(n00b_type_t *t)
{
    if (!n00b_ensure_type(t)) {
        return false;
    }
    t = n00b_type_resolve(t);
    return t->typeid == N00B_T_SESSION_STATE;
}

static inline bool
n00b_type_is_session_trigger(n00b_type_t *t)
{
    if (!n00b_ensure_type(t)) {
        return false;
    }
    t = n00b_type_resolve(t);
    return t->typeid == N00B_T_SESSION_TRIGGER;
}

static inline bool
n00b_type_is_buffer(n00b_type_t *t)
{
    if (!n00b_ensure_type(t)) {
        return false;
    }
    t = n00b_type_resolve(t);
    return t->typeid == N00B_T_BUFFER;
}

static inline bool
n00b_type_is_net_addr(n00b_type_t *t)
{
    if (!n00b_ensure_type(t)) {
        return false;
    }
    t = n00b_type_resolve(t);
    return t->typeid == N00B_T_IPV4;
}

static inline bool
n00b_type_is_duration(n00b_type_t *t)
{
    if (!n00b_ensure_type(t)) {
        return false;
    }
    t = n00b_type_resolve(t);
    return t->typeid == N00B_T_DURATION;
}

static inline bool
n00b_type_is_datetime(n00b_type_t *t)
{
    if (!n00b_ensure_type(t)) {
        return false;
    }
    t = n00b_type_resolve(t);
    return t->typeid == N00B_T_DATETIME;
}

static inline bool
n00b_type_is_date(n00b_type_t *t)
{
    if (!n00b_ensure_type(t)) {
        return false;
    }
    t = n00b_type_resolve(t);
    return t->typeid == N00B_T_DATE;
}

static inline bool
n00b_type_is_time(n00b_type_t *t)
{
    if (!n00b_ensure_type(t)) {
        return false;
    }
    t = n00b_type_resolve(t);
    return t->typeid == N00B_T_TIME;
}

static inline bool
n00b_type_is_renderable(n00b_type_t *t)
{
    return false;
}

static inline bool
n00b_type_is_value_type(n00b_type_t *t)
{
    if (!n00b_ensure_type(t)) {
        return false;
    }
    // This should NOT unbox; check n00b_type_is_box() too if needed.
    t                  = n00b_type_resolve(t);
    n00b_dt_info_t *dt = n00b_type_get_data_type_info(t);

    return dt->by_value;
}

static inline bool
n00b_obj_item_type_is_value(n00b_obj_t obj)
{
    n00b_type_t *t = n00b_get_my_type(obj);

    if (!n00b_ensure_type(t)) {
        return false;
    }

    n00b_type_t *item_type = n00b_type_get_param(t, 0);

    return n00b_type_is_value_type(item_type);
}

// Once we add objects, this will be a dynamic number.
static inline int
n00b_number_concrete_types()
{
    return N00B_NUM_BUILTIN_DTS;
}

static inline int
n00b_get_alloc_len(n00b_type_t *t)
{
    return n00b_type_get_data_type_info(t)->alloc_len;
}

static inline n00b_type_t *
n00b_resolve_and_unbox(n00b_type_t *t)
{
    t = n00b_type_resolve(t);

    if (n00b_type_is_box(t)) {
        return n00b_type_unbox(t);
    }

    return t;
}

static inline bool
n00b_obj_is_int_type(const n00b_obj_t *obj)
{
    n00b_type_t *t = n00b_get_my_type((n00b_obj_t *)obj);

    if (!n00b_ensure_type(t)) {
        return false;
    }

    return n00b_type_is_int_type(n00b_resolve_and_unbox(t));
}

static inline bool
n00b_type_requires_gc_scan(n00b_type_t *t)
{
    t                  = n00b_type_resolve(t);
    n00b_dt_info_t *dt = n00b_type_get_data_type_info(t);

    if (dt->by_value) {
        return false;
    }

    return true;
}

static inline n00b_string_t *
n00b_base_type_name(n00b_obj_t user_object)
{
    return n00b_new(n00b_type_string(),
                    (char *)n00b_object_type_info(user_object)->name,
                    true,
                    0);
}

void n00b_set_next_typevar_fn(n00b_next_typevar_fn);

#ifdef N00B_USE_INTERNAL_API
extern n00b_table_t             *
n00b_format_global_type_environment(n00b_type_universe_t *);
extern void n00b_clean_environment(void);

#ifdef N00B_TYPE_LOG
extern void type_log_on();
extern void type_log_off();
#else
#define type_log_on()
#define type_log_off()
#endif
#endif
