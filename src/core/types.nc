#define N00B_USE_INTERNAL_API
#include "n00b.h"

// Hashing a tree-based data structure down to a single integer can
// save a lot of work if the user does want to do any dynamic type
// checking at all.
//
// Generally, at run-time, objects mostly have instantiated types, but
// we still have to check against generic function parameters.
//
// And, I'd like to have a repl where we're incrementally typing,
// which also requires generic typing.
//
// The biggest challenge is that at time unit T, you might have two
// types that are (`x, `y) -> `z; sometimes those will be the same
// type, and sometimes they will be the same representation of
// different types. We can't have a hash value that say's they're
// always equal.
//
// The first n00b hash has been working pretty well, and I'm going to
// continue to use it for the time being. But at some point, I am
// going to revisit it, as I know how to do a lot better. The
// fundamental problen is that the hashes are almost fully one-way, so
// you still have to keep around the full graph for anything with
// generics in it. Not that big a deal, but eventually I wouldn't mind
// not having to keep full trees around at run-time.
//
//
// Here is the high-level on the current state of the type system,
// btw, covering the current, planned and possible "kinds" of types in
// n00b.

// SOLID type kinds that will never go away (tho might evolve a bit).
// -----------------------------------------------------------------
// 1. Primitive Types. These are built-in, non-structured types with
//    no directly exposed fields, like int, string, etc. Currently, a
//    lot of our config-related data types all fall in this bucket. It
//    also can include things like the 'void' type, although I currently
//    treat 'type' 'error' and 'void' as their own thing.
//
// 2. List Types. These are container types that take a single type
//    parameter (generally their their contents). Items must all have
//    a unified type, but in exchange you can have an arbitrary number
//    of items.  N00b is close to supporting multiple built-in list
//    types, including queues, stacks, and rings, with single-threaded
//    and multi-threaded variants. All lists use traditional [] syntax
//    for literals.
//
// 3. Dictionary Types. These are container types that take two, or
//    sometimes one type parameter (e.g., Sets), and use { } for syntax.
//
// 4. Tuple Types. These are container types that take an arbitrary
//    but fixed number of type parameters. Importantly, the parameters
//    are positional, and currently anonymous (so they're structurally
//    typed).
//
// 5. Function types. I'm going to want to extend these to handle
//    keyword arguments soon, so the representation here is probably
//    going to change. Right now, conceptually there are the
//    parameters, which are effectively a tuple, and the return value,
//    which gets special treatment. When I add keyword parameters,
//    those parameters will essentially be a third 'part' of the
//    signature, and will work similar to how objects will end up
//    working (see below).
//
// 6. Type variables. These stand in for any information we haven't
//    resolved yet. These are key to how type checking works
//    statically, but we need them when running too.
//
//    This is particularly the case because I am taking a different
//    implementation approach to most languages, which generate
//    implementations for each instance type. I currently rely on
//    run-time boxing, and don't plan to change that unless it's
//    someday incredibly obviously important.
//
// Basically, type variables are the only 'unknowns', but can easily
// live as components of anything other than a primitive type.
//
//
// Stuff that's there that I'm thinking about ditching.
// -----------------------------------------------------------------
// 1. Currently, TypeSpecs are their own kind. Types can basically be
//    run-time values that people can assign, print, etc. The reason
//    they're not a primitive type is that they are actually
//    parameterized currently. Essentially, you want people to be able
//    to specify their own type to go along with some data field, you
//    can parameterize it to narrow what is allowed. For instance, you
//    can declare a field to be typespec[list[`x]], and values assigned
//    to it MUST be in that form.
//
// 2. I also have "OneOf", which is basically an algebraic sum
//    type. It's mainly meant for making typespec constraints more
//    useful. I find unrestructed types like this in a language have a
//    lot of practical issues that don't lend themselves well to the
//    type of language n00b is becoming.
//
// Basically, since we have explicit post-execution validation
// checking of fields, #1 isn't really necessary, which makes #2
// unnecessary. I haven't made a firm decision yet, though.
//
//
// Stuff that I'm planning that I haven't gotten to yet.
// -----------------------------------------------------------------
// 1. Object types. Once Chalk doesn't need anything specific from
// n00b, this will probably end up my top priority, and it isn't too
// much work. From a type system perspective, this will be kind of a
// dual of tuples... they will be structs of data fields, but order
// will be unimportant, and will have name equivolence. Functions will
// probably *not* be considered data objects, since I have no plans
// for inheritance.
//
// 2. Reference types. Every type will have an associated reference
//    type. Go, aliasing.
//
// 3. "Maybe" types. Basically, this is an highly constrained
// algabraic type that 'extends' a type by adding null (the `void`
// type) as an optional value. This one should be easy relative to
// more generic algabraic types.
//
// Stuff that I'm considering, but am in no hurry to decide.
// -----------------------------------------------------------------
// Most of the stuff here is possible to do well statically, depending
// on constraints, but I really want to wait and see if I can nail the
// usability.
//
// 1. Interfaces; especially if they basically give the feeling of
//    allowing statically checkable duck typing.  These are
//    essentially constraints on generic types. But, it does have some
//    implications because all types, including primitive types
//    suddenly, if we think of types as sets, primitive types can all
//    the sudden be in multiple sets. I think I need to get objects to
//    the point where I'm happy before I can play around and decide if
//    this will work.
//
// 2. 'Either' types. While I'd like to do this for return values instead of
//    exception handling, given we're doing garbage collection, exception
//    handling is pretty easy, whereas this is pretty hard to do well.
//    I think I'm going to start w/ exceptions, and then once Maybe is in,
//    I'll consider this in more detail.

// Note: because of the type hashing, concrete types in the type
// environment are compatable across type environemnts / universes.
//
// Instead of parameterizing all of the APIs to take a separate
// parameter to the type environment, I've changed the API so that you
// can parameterize when need be, but avoid the extra complexity. All
// we need is to be able to issue type variables that never collide.
//
// To this end, you can register a callback to provide the next type
// identifier. The default callback initializes per-thread, and
// chooses a random 64 bit value. Note that the high bit will always
// be forcefully set by the type system.
n00b_type_t *n00b_bi_types[N00B_NUM_BUILTIN_DTS] = {
    0,
};

// This is just hex w/ a different char set; max size would be 18 digits.
static const char tv_letters[] = "jtvwxyzabcdefghi";

n00b_type_universe_t n00b_type_universe;

#ifdef N00B_TYPE_LOG

static bool log_types = true;

#define type_log(x, y)                                     \
    if (log_types) {                                       \
        n00b_printf("«em2»«#»:«/» «em1»«#»«/» (line «#»)", \
                    n00b_cstring(x),                       \
                    n00b_type_resolve(y),                  \
                    (int64_t)__LINE__);                    \
    }

void
type_start_log(void)
{
    log_types = true;
}

void
type_end_log(void)
{
    log_types = false;
}

#else
#define type_log(x, y)
#endif

typedef struct {
    n00b_sha_t  *sha;
    n00b_dict_t *memos;
    int          tv_count;
} type_hash_ctx;

static uint64_t             n00b_default_next_typevar(void);
static n00b_next_typevar_fn next_typevar_fn = n00b_default_next_typevar;
static pthread_key_t        n00b_tvar_start_key;
static pthread_key_t        n00b_tvar_next_key;

void
n00b_type_set_gc_bits(uint64_t *bitfield, void *alloc)
{
    n00b_type_t *t = (n00b_type_t *)alloc;
    n00b_mark_raw_to_addr(bitfield, t, &t->items);
}

void
n00b_set_next_typevar_vn(n00b_next_typevar_fn f)
{
    next_typevar_fn = f ? f : n00b_default_next_typevar;
}

static inline uint64_t
n00b_acquire_typevar_id(void)
{
    return (1ULL << 63) | next_typevar_fn();
}

static void
n00b_thread_release_typevar(void *arg)
{
    pthread_setspecific(n00b_tvar_start_key, NULL);
    pthread_setspecific(n00b_tvar_next_key, NULL);
}

static void
n00b_initialize_next_typevar_fn(void)
{
    pthread_key_create(&n00b_tvar_start_key, n00b_thread_release_typevar);
    pthread_key_create(&n00b_tvar_next_key, NULL);
    uint64_t start = n00b_rand64();
    pthread_setspecific(n00b_tvar_start_key, (void *)start);
    pthread_setspecific(n00b_tvar_next_key, (void *)start);
}

static uint64_t
n00b_default_next_typevar(void)
{
    static pthread_once_t init = PTHREAD_ONCE_INIT;
    pthread_once(&init, n00b_initialize_next_typevar_fn);

    uint64_t tid = (uint64_t)pthread_getspecific(n00b_tvar_next_key);
    pthread_setspecific(n00b_tvar_next_key, (void *)(tid + 1));

    return tid;
}

static n00b_type_t *
n00b_alloc_base_type_object(int index)
{
    n00b_type_t *type = n00b_gc_alloc_mapped(n00b_type_t,
                                             n00b_type_set_gc_bits);
    n00b_mem_ptr obj  = {.v = type};
    n00b_mem_ptr hdr  = (n00b_mem_ptr){.alloc = obj.alloc - 1};

    type->base_index    = index;
    type->typeid        = index;
    hdr.alloc->n00b_obj = true;

    n00b_bi_types[type->base_index] = type;
    hdr.alloc->type                 = n00b_bi_types[N00B_T_TYPESPEC];

    if (index) {
        n00b_universe_put(&n00b_type_universe, type);
    }

    return type;
}

static inline n00b_list_t *
alloc_type_list(void)
{
    n00b_list_t    *result;
    n00b_alloc_hdr *hdr;

    result         = n00b_gc_alloc_mapped(n00b_list_t, N00B_GC_SCAN_ALL);
    hdr            = n00b_object_header(result);
    result->noscan = N00B_NOSCAN;

    hdr->n00b_obj = true;
    hdr->type     = n00b_bi_types[N00B_T_INTERNAL_TLIST];

    // Create the empty list to hold up to two types.
    result->data      = n00b_gc_raw_alloc(sizeof(uint64_t *) * 2,
                                     N00B_GC_SCAN_ALL);
    result->append_ix = 0;
    result->length    = 2;

    n00b_rw_lock_init(&result->lock);

    return result;
}

// Since types are represented as a graph, as we 'solve' for type
// variables, we often will need to make sure we are looking at the
// resolved type, not variables.
//
// We take the lookup opportunity to add ourselves to the type store
// if we aren't already there.
//
// Additionally, we always return the object already in the store, when
// there are dupes, unless it's a box.
static inline n00b_type_t *
_n00b_type_resolve(n00b_type_t *node)
{
    n00b_type_t *next;

    if (!node) {
        return n00b_type_error();
    }

    if (!node->typeid) {
        return node;
    }

    if (node->typeid < N00B_NUM_BUILTIN_DTS) {
        return n00b_bi_types[node->typeid];
    }

    if (n00b_universe_add(&n00b_type_universe, node)) {
        return node;
    }

    while (true) {
        next = n00b_universe_get(&n00b_type_universe, node->typeid);

        if (!next) {
            return node;
        }

        if (next == node || node->typeid == next->typeid) {
            return next;
        }

        node = next;
    }
}

n00b_type_t *
n00b_type_resolve(n00b_type_t *node)
{
    n00b_type_t *result;

    result = _n00b_type_resolve(node);

    return result;
}

static void
internal_type_hash(n00b_type_t *node, type_hash_ctx *ctx)
{
    // This is the recursive bits of the hash... it won't finalize
    // the hash context ever.
    if (node == NULL) {
        return;
    }

    if (node->typeid != 0) {
        // If it's a rehash, then we expect we may have updated pieces.
        node = n00b_type_resolve(node);
    }

    uint64_t       num_tvars;
    n00b_dt_kind_t kind = n00b_type_get_kind(node);

    if (kind == N00B_DT_KIND_box) {
        node = n00b_type_unbox(node);
        kind = N00B_DT_KIND_primitive;
    }

    n00b_sha_int_update(ctx->sha, node->base_index);
    n00b_sha_int_update(ctx->sha, kind);

    switch (kind) {
        // Currently not hashing for future things.
    case N00B_DT_KIND_func:
        n00b_sha_int_update(ctx->sha, node->flags);
        break;
    case N00B_DT_KIND_type_var:
        num_tvars = (uint64_t)hatrack_dict_get(ctx->memos,
                                               (void *)node->typeid,
                                               NULL);

        if (num_tvars == 0) {
            num_tvars = ++ctx->tv_count;
            hatrack_dict_put(ctx->memos,
                             (void *)node->typeid,
                             (void *)num_tvars);
        }

        n00b_sha_int_update(ctx->sha, num_tvars);
        n00b_sha_int_update(ctx->sha, node->typeid);
        break;
    case N00B_DT_KIND_primitive:
    case N00B_DT_KIND_nil:
    case N00B_DT_KIND_internal:
    case N00B_DT_KIND_box:

        return;
    default:; // Do nothing.
    }

    if (!node->items) {
        return;
    }

    size_t n = n00b_list_len(node->items);

    n00b_sha_int_update(ctx->sha, n);

    for (size_t i = 0; i < n; i++) {
        // This list shouldn't change.
        n00b_type_t *t = n00b_private_list_get(node->items, i, NULL);
        n00b_assert(t != node);
        internal_type_hash(t, ctx);
    }
}

static n00b_type_hash_t
type_hash_and_dedupe(n00b_type_t **nodeptr)
{
    // This is only used internally when requesting type objects; if
    // someone requests a concrete type, we keep yielding shared
    // instances, since there is no more resolution that can happen.

    n00b_buf_t    *buf;
    uint64_t       result;
    n00b_type_t   *node = n00b_type_resolve(*nodeptr);
    type_hash_ctx  ctx;
    n00b_dt_kind_t kind = n00b_type_get_kind(node);

    // Note that
    switch (kind) {
    case N00B_DT_KIND_box:
        node = n00b_type_unbox(node);
        return type_hash_and_dedupe(&node);
    case N00B_DT_KIND_nil:
    case N00B_DT_KIND_primitive:
    case N00B_DT_KIND_internal:
        node->typeid = node->base_index;
        return n00b_universe_attempt_to_add(&n00b_type_universe, node)->typeid;

    case N00B_DT_KIND_type_var:
        n00b_universe_put(&n00b_type_universe, node);
        return node->typeid;
    default:
        // I was leaving ctx on the stack but the GC was losing it :(
        node->typeid = 0;
        ctx.sha      = n00b_new(n00b_type_hash());
        ctx.tv_count = 0;
        ctx.memos    = n00b_new_unmanaged_dict(HATRACK_DICT_KEY_TYPE_PTR,
                                            false,
                                            false);

        internal_type_hash(node, &ctx);

        buf    = n00b_sha_finish(ctx.sha);
        result = ((uint64_t *)buf->data)[0];

        little_64(result);

        if (ctx.tv_count == 0) {
            result &= ~(1LLU << 63);
        }
        else {
            result |= (1LLU << 63);
        }

        node->typeid = result;

        if (!n00b_universe_add(&n00b_type_universe, node)) {
            n00b_type_t *o = n00b_universe_get(&n00b_type_universe,
                                               node->typeid);
            if (!o) {
                return result;
            }

            if (o->typeid == 0) {
                n00b_universe_put(&n00b_type_universe, node);
            }
            else {
                *nodeptr = n00b_type_resolve(o);
            }
        }
    }
    return result;
}

n00b_type_hash_t
n00b_calculate_type_hash(n00b_type_t *node)
{
    return type_hash_and_dedupe(&node);
}

static inline n00b_type_hash_t
type_rehash(n00b_type_t *node)
{
    node->typeid = 0;
    return n00b_calculate_type_hash(node);
}

static void
n00b_type_init(n00b_type_t *n, va_list args)
{
    n00b_builtin_t base_id = va_arg(args, n00b_builtin_t);
    n00b_mem_ptr   p       = (n00b_mem_ptr){.v = n};

    --p.alloc; // Now back it up to the beginning of the allocation.

    if (base_id == 0) {
        // This short circuit should be used when unmarshaling or
        // when creating an internal container inference type.
        if (va_arg(args, int64_t)) {
            n->items = alloc_type_list();
        }
        return;
    }

    if (base_id >= N00B_NUM_BUILTIN_DTS || base_id < N00B_T_ERROR) {
        N00B_CRAISE("Invalid type ID.");
    }

    n->base_index = base_id;

    if (base_id >= N00B_T_GENERIC) {
        n->typeid = n00b_acquire_typevar_id();
    }

    switch (n00b_type_get_kind(n)) {
    case N00B_DT_KIND_nil:
    case N00B_DT_KIND_primitive:
    case N00B_DT_KIND_internal:
    case N00B_DT_KIND_box:
        memcpy(n, n00b_bi_types[base_id], sizeof(n00b_type_t));
        break;
    case N00B_DT_KIND_type_var:
    case N00B_DT_KIND_list:
    case N00B_DT_KIND_tuple:
    case N00B_DT_KIND_dict:
    case N00B_DT_KIND_func:
    case N00B_DT_KIND_object:
        n->items = alloc_type_list();
        break;
    default:
        n00b_unreachable();
    }
}

static n00b_type_t *
tspec_copy_internal(n00b_type_t *node, n00b_dict_t *dupes)
{
    // Copies, anything that might be mutated, returning an unlocked
    // instance where mutation is okay.
    //
    // TODO: this is wrong for general purpose; uses global environment.
    // Need to do a base version that works for any type env.

    node              = n00b_type_resolve(node);
    n00b_type_t *dupe = hatrack_dict_get(dupes, node, NULL);

    if (dupe != NULL) {
        return dupe;
    }

    if (n00b_type_is_concrete(node) || n00b_type_is_box(node)) {
        return node;
    }

    int             n    = n00b_type_get_num_params(node);
    n00b_dt_info_t *base = n00b_internal_type_base(node);
    n00b_type_t    *result;

    if (base->dt_kind == N00B_DT_KIND_type_var) {
        result             = n00b_new_typevar();
        uint64_t *old_opts = node->options.container_options;
        uint64_t *new_opts = result->options.container_options;

        for (int i = 0; i < n00b_get_num_bitfield_words(); i++) {
            new_opts[i] = old_opts[i];
        }

        hatrack_dict_put(dupes, node, result);
        return result;
    }
    else {
        result = n00b_new(n00b_type_typespec(), node->base_index);
    }

    n00b_list_t *to_copy = node->items;
    n00b_list_t *ts_dst  = result->items;

    for (int i = 0; i < n; i++) {
        n00b_type_t *original = n00b_list_get(to_copy, i, NULL);
        n00b_type_t *copy     = tspec_copy_internal(original, dupes);

        n00b_list_append(ts_dst, copy);
    }

    n00b_calculate_type_hash(result);

    hatrack_dict_put(dupes, node, result);
    return result;
}

n00b_type_t *
n00b_type_copy(n00b_type_t *node)
{
    n00b_type_t *result;
    n00b_dict_t *dupes = n00b_dict(n00b_type_ref(), n00b_type_ref());

    result = tspec_copy_internal(node, dupes);

    return result;
}

static n00b_type_t *
copy_if_needed(n00b_type_t *t)
{
    if (n00b_type_is_concrete(t)) {
        return t;
    }

    if (!n00b_type_is_locked(t)) {
        return t;
    }

    return n00b_type_copy(t);
}

bool
n00b_type_is_concrete(n00b_type_t *node)
{
    int          n;
    n00b_list_t *param;

    node = n00b_type_resolve(node);

    switch (n00b_type_get_kind(node)) {
    case N00B_DT_KIND_nil:
    case N00B_DT_KIND_primitive:
    case N00B_DT_KIND_internal:
    case N00B_DT_KIND_box:
        return true;
    case N00B_DT_KIND_type_var:
        return false;
    case N00B_DT_KIND_list:
    case N00B_DT_KIND_dict:
    case N00B_DT_KIND_tuple:
    case N00B_DT_KIND_func:
        n     = n00b_type_get_num_params(node);
        param = n00b_type_get_params(node);

        for (int i = 0; i < n; i++) {
            if (!n00b_type_is_concrete(n00b_list_get(param, i, NULL))) {
                return false;
            }
        }
        return true;
    default:
        n00b_assert(false);
        return false;
    }
}

static n00b_type_t *
unify_type_variables(n00b_type_t *t1, n00b_type_t *t2)
{
    int           bf_words    = n00b_get_num_bitfield_words();
    int64_t       num_options = 0;
    n00b_type_t  *result      = n00b_new_typevar();
    tv_options_t *t1_opts     = &t1->options;
    tv_options_t *t2_opts     = &t2->options;
    tv_options_t *res_opts    = &result->options;
    int           t1_arg_ct   = n00b_type_get_num_params(t1);
    bool          t1_fixed_ct = !(t1->flags & N00B_FN_UNKNOWN_TV_LEN);
    int           t2_arg_ct   = n00b_type_get_num_params(t2);
    bool          t2_fixed_ct = !(t2->flags & N00B_FN_UNKNOWN_TV_LEN);

    for (int i = 0; i < bf_words; i++) {
        uint64_t one    = t1_opts->container_options[i];
        uint64_t other  = t2_opts->container_options[i];
        uint64_t merged = one & other;

        num_options += __builtin_popcountll(merged);
        res_opts->container_options[i] = merged;
    }

    if (num_options == 0) {
        type_log("unify(t1, t2)", n00b_type_error());
        return n00b_type_error();
    }

    // Okay, now we need to look at any info we have on type
    // parameters.

    int nparams = n00b_max(t1_arg_ct, t2_arg_ct);

    for (int i = 0; i < nparams; i++) {
        if (i >= t1_arg_ct) {
            n00b_list_append(result->items,
                             n00b_type_get_param(t2, i));
        }
        else {
            if (i >= t2_arg_ct) {
                n00b_list_append(result->items,
                                 n00b_type_get_param(t1, i));
            }
            else {
                n00b_type_t *sub1 = n00b_type_get_param(t1, i);
                n00b_type_t *sub2 = n00b_type_get_param(t2, i);
                n00b_type_t *sub3 = n00b_unify(sub1, sub2);
                if (n00b_type_is_error(sub3)) {
                    type_log("unify(t1, t2)", n00b_type_error());
                    return n00b_type_error();
                }

                n00b_list_append(result->items, sub3);
            }
        }
    }

    bool list_ok      = n00b_tuple_syntax_possible(result);
    bool set_ok       = n00b_set_syntax_possible(result);
    bool dict_ok      = n00b_dict_syntax_possible(result);
    bool tuple_ok     = n00b_tuple_syntax_possible(result);
    bool known_arglen = t1_fixed_ct | t2_fixed_ct;
    bool recal_pop    = false;

    if (known_arglen) {
        switch (nparams) {
        case 1:
            if (dict_ok) {
                n00b_remove_dict_options(result);
                recal_pop = true;
            }
            if (tuple_ok) {
                n00b_remove_tuple_options(result);
                recal_pop = true;
            }
            break;
        case 2:
            if (list_ok) {
                n00b_remove_tuple_options(result);
                recal_pop = true;
            }
            if (set_ok) {
                n00b_remove_set_options(result);
                recal_pop = true;
            }
            break;
        default:
            // Has to be tuple syntax.
            if (list_ok) {
                n00b_remove_tuple_options(result);
                recal_pop = true;
            }
            if (set_ok) {
                n00b_remove_set_options(result);
                recal_pop = true;
            }
            if (dict_ok) {
                n00b_remove_dict_options(result);
                recal_pop = true;
            }
            break;
        }
    }

    if (!dict_ok && !tuple_ok) {
        known_arglen = true;

        if (nparams > 1) {
            type_log("unify(t1, t2)", n00b_type_error());
            return n00b_type_error();
        }
        if (nparams == 0) {
            n00b_list_append(result->items, n00b_new_typevar());
        }
    }

    if (!tuple_ok && !list_ok && !set_ok) {
        known_arglen = true;

        if (nparams > 2) {
            type_log("unify(t1, t2)", n00b_type_error());
            return n00b_type_error();
        }
        while (nparams < 2) {
            n00b_list_append(result->items, n00b_new_typevar());
        }
    }

    if (known_arglen) {
        result->flags = 0;
    }
    else {
        result->flags = N00B_FN_UNKNOWN_TV_LEN;
    }

    if (recal_pop) {
        num_options = 0;

        for (int i = 0; i < bf_words; i++) {
            num_options += __builtin_popcountll(res_opts->container_options[i]);
        }
    }

    if (num_options == 0) {
        if (t1_arg_ct || t2_arg_ct) {
            type_log("unify(t1, t2)", n00b_type_error());
            return n00b_type_error();
        }
        // Otherwise, it's a type variable that MUST point to a concrete type.
    }
    if (num_options == 1) {
        uint64_t baseid = 0;

        for (int i = 0; i < bf_words; i++) {
            uint64_t val = res_opts->container_options[i];

            // It at least used to be the case that clzll on an empty
            // word gives arch dependent results (either 64 or 63).
            // So special case to be safe.
            if (!val) {
                baseid += 64;
                continue;
            }

            // If the 0th bit is set, we want to return 0.
            // If the 64th bit is set, we want to return 63.
            baseid += 63 - __builtin_clzll(val);
            break;
        }

        n00b_type_t *last = n00b_type_get_param(result, nparams - 1);

        if (t1_opts->value_type != 0) {
            // This is supposed to be caught by the index node not
            // being a constant, but that's not done yet.
            if (tuple_ok) {
                type_log("unify(t1, t2)", n00b_type_error());
                return n00b_type_error();
            }

            if (n00b_type_is_error(n00b_unify(t1_opts->value_type, last))) {
                type_log("unify(t1, t2)", n00b_type_error());
                return n00b_type_error();
            }
        }

        if (t2_opts->value_type != 0) {
            if (tuple_ok) {
                type_log("unify(t1, t2)", n00b_type_error());
                return n00b_type_error();
            }

            if (n00b_type_is_error(n00b_unify(t1_opts->value_type, last))) {
                type_log("unify(t1, t2)", n00b_type_error());
                return n00b_type_error();
            }
        }
        result->base_index = baseid;
        type_rehash(result);
    }
    else {
        if (!t1_opts->value_type && t2_opts->value_type) {
            t1_opts->value_type = n00b_new_typevar();
        }

        if (!t2_opts->value_type && t1_opts->value_type) {
            t2_opts->value_type = n00b_new_typevar();
        }

        if (t1_opts->value_type) {
            if (n00b_type_is_error(n00b_unify(t1_opts->value_type,
                                              t2_opts->value_type))) {
                type_log("unify(t1, t2)", n00b_type_error());
                return n00b_type_error();
            }
        }
    }

    n00b_universe_forward(&n00b_type_universe, t1, result);
    n00b_universe_forward(&n00b_type_universe, t2, result);

    if (nparams != 0 && res_opts->value_type != NULL) {
        if (n00b_type_is_error(n00b_unify(t1_opts->value_type,
                                          t2_opts->value_type))) {
            if (n00b_type_is_error(
                    n00b_unify(res_opts->value_type,
                               n00b_type_get_param(result,
                                                   nparams - 1)))) {
                type_log("unify(t1, t2)", n00b_type_error());
                return n00b_type_error();
            }
        }
    }

    type_log("unify(t1, t2)", result);
    return result;
}

static n00b_type_t *
unify_tv_with_concrete_type(n00b_type_t *t1,
                            n00b_type_t *t2)
{
    switch (n00b_type_get_kind(t2)) {
    case N00B_DT_KIND_primitive:
    case N00B_DT_KIND_internal:
    case N00B_DT_KIND_func:
    case N00B_DT_KIND_nil:
        n00b_universe_forward(&n00b_type_universe, t1, t2);
        type_log("unify(t1, t2)", t2);
        return t2;
    default:
        break;
    }

    uint32_t      baseid  = (uint32_t)t2->base_index;
    int           word    = ((int)baseid) / 64;
    int           bit     = ((int)baseid) % 64;
    tv_options_t *t1_opts = &t1->options;

    if (!(t1_opts->container_options[word] & (1UL << bit))) {
        type_log("unify(t1, t2)", n00b_type_error());
        return n00b_type_error();
    }

    for (int i = 0; i < n00b_type_get_num_params(t1); i++) {
        n00b_type_t *sub = n00b_unify(n00b_type_get_param(t1, i),
                                      n00b_type_get_param(t2, i));

        if (n00b_type_is_error(sub)) {
            type_log("unify(t1, t2)", n00b_type_error());
            return sub;
        }
    }

    if (t1_opts->value_type != 0) {
        if (n00b_type_has_tuple_syntax(t2)) {
            // This is supposed to be caught by the index node not
            // being a constant, but that's not done yet.
            return n00b_type_error();
        }
        int          n   = n00b_type_get_num_params(t2);
        n00b_type_t *sub = n00b_unify(n00b_type_get_param(t2, n - 1),
                                      t1_opts->value_type);

        if (n00b_type_is_error(sub)) {
            type_log("unify(t1, t2)", n00b_type_error());
            return sub;
        }
    }

    type_rehash(t2);
    n00b_universe_forward(&n00b_type_universe, t1, t2);
    type_log("unify(t1, t2)", t2);
    return t2;
}

static inline n00b_type_t *
_n00b_unify(n00b_type_t *t1, n00b_type_t *t2)
{
    n00b_type_t *result;
    n00b_type_t *sub1;
    n00b_type_t *sub2;
    n00b_type_t *sub_result;
    n00b_list_t *p1;
    n00b_list_t *p2;
    n00b_list_t *new_subs;
    int          num_params = 0;

    if (!t1 || !t2) {
        return n00b_type_error();
    }

    t1 = n00b_type_resolve(t1);
    t2 = n00b_type_resolve(t2);

    t1 = copy_if_needed(t1);
    t2 = copy_if_needed(t2);

    // If comparing types w/ something boxed, ignored the box.
    if (n00b_type_is_box(t1)) {
        t1 = n00b_type_unbox(t1);
        if (n00b_type_is_box(t2)) {
            t2 = n00b_type_unbox(t2);
        }
        return n00b_unify(t1, t2);
    }

    if (n00b_type_is_box(t2)) {
        t2 = n00b_type_unbox(t2);
        return n00b_unify(t1, t2);
    }

    if (t1->typeid == 0 && t1->base_index == 0) {
        return n00b_type_error();
    }

    if (t2->typeid == 0 && t2->base_index == 0) {
        return n00b_type_error();
    }

    // This is going to re-check the structure, just to cover any
    // cases where we didn't or couldn't update it before.
    //
    // This needs to be here, for reasons I don't even understand;
    // I'm clearly missing a needed rehash in a previous call.
    if (n00b_type_is_concrete(t1)) {
        type_rehash(t1);
    }
    if (n00b_type_is_concrete(t2)) {
        type_rehash(t2);
    }

    type_log("t1", t1);
    type_log("t2", t2);

    if (t1->typeid == t2->typeid) {
        type_log("unify(t1, t2)", t1);
        return t1;
    }

    if (t1->typeid == N00B_T_ERROR || t2->typeid == N00B_T_ERROR) {
        type_log("unify(t1, t2)", n00b_type_error());
        return n00b_type_error();
    }

    if (n00b_type_is_concrete(t1) && n00b_type_is_concrete(t2)) {
        // Concrete, but not the same. Types are not equivolent.
        // While casting may be possible, that doesn't happen here;
        // unification is about type equivolence, not coercion!
        type_log("unify(t1, t2)", n00b_type_error());
        return n00b_type_error();
    }

    n00b_dt_kind_t b1 = n00b_type_get_kind(t1);
    n00b_dt_kind_t b2 = n00b_type_get_kind(t2);

    if (b1 != b2) {
        if (b1 != N00B_DT_KIND_type_var) {
            if (b2 != N00B_DT_KIND_type_var) {
                type_log("unify(t1, t2)", n00b_type_error());
                return n00b_type_error();
            }

            n00b_type_t   *tswap;
            n00b_dt_kind_t bswap;

            tswap = t1;
            t1    = t2;
            t2    = tswap;
            bswap = b1;
            b1    = b2;
            b2    = bswap;
        }
    }

    switch (b1) {
    case N00B_DT_KIND_type_var:

        if (b2 == N00B_DT_KIND_type_var) {
            return unify_type_variables(t1, t2);
        }
        else {
            return unify_tv_with_concrete_type(t1, t2);
        }

    case N00B_DT_KIND_list:
    case N00B_DT_KIND_dict:
    case N00B_DT_KIND_tuple:
        num_params = n00b_type_get_num_params(t1);

        if (num_params != n00b_type_get_num_params(t2)) {
            type_log("unify(t1, t2)", n00b_type_error());
            return n00b_type_error();
        }

unify_sub_nodes:
        p1       = n00b_type_get_params(t1);
        p2       = n00b_type_get_params(t2);
        new_subs = n00b_new(n00b_type_list(n00b_type_typespec()),
                            n00b_kw("length", n00b_ka(num_params)));

        for (int i = 0; i < num_params; i++) {
            sub1 = n00b_list_get(p1, i, NULL);
            sub2 = n00b_list_get(p2, i, NULL);

            if (sub1 == NULL) {
                sub1 = n00b_new_typevar();
                n00b_list_set(p1, i, sub1);
            }
            if (sub2 == NULL) {
                sub2 = n00b_new_typevar();
                n00b_list_set(p1, i, sub2);
            }

            sub_result = n00b_unify(sub1, sub2);

            if (n00b_type_is_error(sub_result)) {
                type_log("unify(t1, t2)", sub_result);
                return sub_result;
            }
            n00b_list_append(new_subs, sub_result);
        }

        result        = t1;
        result->items = new_subs;
        break;
    case N00B_DT_KIND_func: {
        int f1_params = n00b_type_get_num_params(t1);
        int f2_params = n00b_type_get_num_params(t2);

        if (f2_params == 0) {
            result = t1;
            break;
        }

        if (f1_params == 0) {
            result = t2;
            break;
        }
        // Actuals will never be varargs, so if we have two vararg
        // functions, it's only because we're trying to unify two formals.
        if (!((t1->flags & t2->flags) & N00B_FN_TY_VARARGS)) {
            if (f1_params != f2_params) {
                type_log("unify(t1, t2)", n00b_type_error());
                return n00b_type_error();
            }

            num_params = f1_params;
            goto unify_sub_nodes;
        }

        // Below here, we got varargs; put the vararg param on the left.
        if ((t2->flags & N00B_FN_TY_VARARGS) != 0) {
            n00b_type_t *swap;

            swap       = t1;
            t1         = t2;
            t2         = swap;
            num_params = f1_params;
            f1_params  = f2_params;
            f2_params  = num_params;
        }

        // -1 here because varargs params are optional.
        if (f2_params < f1_params - 1) {
            type_log("unify(t1, t2)", n00b_type_error());
            return n00b_type_error();
        }

        // The last item is always the return type, so we have to
        // unify the last items, plus any items before varargs.  Then,
        // if there are any items in type2, they each need to unify
        // with t1's va;l;rargs parameter.
        p1       = n00b_type_get_params(t1);
        p2       = n00b_type_get_params(t2);
        new_subs = n00b_new(n00b_type_list(n00b_type_typespec()),
                            n00b_kw("length", n00b_ka(num_params)));

        for (int i = 0; i < f1_params - 2; i++) {
            sub1       = n00b_list_get(p1, i, NULL);
            sub2       = n00b_list_get(p2, i, NULL);
            sub_result = n00b_unify(sub1, sub2);

            n00b_list_append(new_subs, sub_result);
        }

        // This checks the return type.
        sub1 = n00b_list_get(p1, f1_params - 1, NULL);
        sub2 = n00b_list_get(p2, f2_params - 1, NULL);

        if (!sub1) {
            sub1 = n00b_type_void();
        }
        if (!sub2) {
            sub2 = n00b_type_void();
        }

        sub_result = n00b_unify(sub1, sub2);

        if (n00b_type_is_error(sub_result)) {
            type_log("unify(t1, t2)", sub_result);
            return sub_result;
        }
        // Now, check any varargs.
        sub1 = n00b_list_get(p1, f1_params - 2, NULL);

        for (int i = n00b_max(f1_params - 2, 0); i < f2_params - 1; i++) {
            sub2       = n00b_list_get(p2, i, NULL);
            sub_result = n00b_unify(sub1, sub2);
            if (n00b_type_is_error(sub_result)) {
                type_log("unify(t1, t2)", sub_result);
                return sub_result;
            }
        }

        result = t1;

        break;
    }
    case N00B_DT_KIND_object:
    case N00B_DT_KIND_nil:
    case N00B_DT_KIND_primitive:
    case N00B_DT_KIND_internal:
    case N00B_DT_KIND_maybe:
    case N00B_DT_KIND_oneof:
    default:
        // Either not implemented yet or covered before the switch.
        // These are all implemented in the Nim checker but won't
        // be moved until N00b is using them.
        n00b_unreachable();
    }

    type_hash_and_dedupe(&result);

    type_log("unify(t1, t2)", result);
    return result;
}

n00b_type_t *
n00b_unify(n00b_type_t *t1, n00b_type_t *t2)
{
    n00b_type_t *result;
    result = _n00b_unify(t1, t2);

    return result;
}

// 'exact' match is mainly used for comparing declarations to
// other types. It needs to ignore boxes though.
//
// TODO: Need to revisit this with the new generics.
//
static inline n00b_type_exact_result_t
_n00b_type_cmp_exact(n00b_type_t *t1, n00b_type_t *t2)
{
    t1 = n00b_type_resolve(t1);
    t2 = n00b_type_resolve(t2);

    if (n00b_type_is_box(t1)) {
        t1 = n00b_type_unbox(t1);
    }

    if (n00b_type_is_box(t2)) {
        t2 = n00b_type_unbox(t2);
    }

    if (t1->typeid == t2->typeid) {
        return n00b_type_match_exact;
    }

    if (t1->typeid == N00B_T_ERROR || t2->typeid == N00B_T_ERROR) {
        return n00b_type_cant_match;
    }

    n00b_dt_kind_t b1 = n00b_type_get_kind(t1);
    n00b_dt_kind_t b2 = n00b_type_get_kind(t2);

    if (b1 != b2) {
        if (b1 == N00B_DT_KIND_type_var) {
            return n00b_type_match_right_more_specific;
        }
        if (b2 == N00B_DT_KIND_type_var) {
            return n00b_type_match_left_more_specific;
        }

        return n00b_type_cant_match;
    }

    int  n1    = n00b_type_get_num_params(t1);
    int  n2    = n00b_type_get_num_params(t2);
    bool err   = false;
    bool left  = false;
    bool right = false;

    if (n1 != n2) {
        return n00b_type_cant_match;
    }

    if ((t1->flags ^ t2->flags) & N00B_FN_TY_VARARGS) {
        return n00b_type_cant_match;
    }

    for (int i = 0; i < n1; i++) {
        n00b_list_t *p1;
        n00b_list_t *p2;
        n00b_type_t *sub1;
        n00b_type_t *sub2;

        p1   = n00b_type_get_params(t1);
        p2   = n00b_type_get_params(t2);
        sub1 = n00b_list_get(p1, i, NULL);
        sub2 = n00b_list_get(p2, i, NULL);

        switch (n00b_type_cmp_exact(sub1, sub2)) {
        case n00b_type_match_exact:
            continue;
        case n00b_type_cant_match:
            return n00b_type_cant_match;
        case n00b_type_match_left_more_specific:
            err  = true;
            left = true;
            continue;
        case n00b_type_match_right_more_specific:
            err   = true;
            right = true;
            continue;
        case n00b_type_match_both_have_more_generic_bits:
            err   = true;
            left  = true;
            right = true;
            continue;
        }
    }

    if (!err) {
        return n00b_type_match_exact;
    }
    if (left && right) {
        return n00b_type_match_both_have_more_generic_bits;
    }
    if (left) {
        return n00b_type_match_left_more_specific;
    }
    else { // right only
        return n00b_type_match_right_more_specific;
    }
}

n00b_type_exact_result_t
n00b_type_cmp_exact(n00b_type_t *t1, n00b_type_t *t2)
{
    n00b_type_exact_result_t result;

    result = _n00b_type_cmp_exact(t1, t2);

    return result;
}

static inline n00b_string_t *
create_typevar_name(int64_t num)
{
    char buf[19] = {
        0,
    };
    int i = 0;

    while (num) {
        buf[i++] = tv_letters[num & 0x0f];
        num >>= 4;
    }

    return n00b_cstring(buf);
}

static inline n00b_string_t *
internal_repr_tv(n00b_type_t *t, n00b_dict_t *memos, int64_t *nexttv)
{
    n00b_string_t *s = hatrack_dict_get(memos, t, NULL);

    if (s != NULL) {
        return s;
    }

    if (n00b_partial_inference(t)) {
        bool           list_ok  = n00b_list_syntax_possible(t);
        bool           set_ok   = n00b_set_syntax_possible(t);
        bool           dict_ok  = n00b_dict_syntax_possible(t);
        bool           tuple_ok = n00b_tuple_syntax_possible(t);
        int            num_ok   = 0;
        n00b_list_t   *parts    = n00b_list(n00b_type_string());
        n00b_string_t *res;

        if (list_ok) {
            num_ok++;
            n00b_list_append(parts, n00b_cstring("some_list"));
        }
        // For now, hardcode knowing we don't expose other options.
        if (dict_ok) {
            num_ok++;
            n00b_list_append(parts, n00b_cstring("dict"));
        }
        if (set_ok) {
            num_ok++;
            n00b_list_append(parts, n00b_cstring("set"));
        }
        if (tuple_ok) {
            n00b_list_append(parts, n00b_cstring("tuple"));
            num_ok++;
        }

        switch (num_ok) {
        case 0:
            return n00b_cstring("$non_container");
        case 1:
            if (list_ok) {
                res = n00b_cstring("$some_list[");
            }
            else {
                res = n00b_cformat("«#»«#»",
                                   n00b_list_get(parts, 0, NULL),
                                   n00b_cached_lbracket());
            }
            break;
        default:
            res = n00b_cformat("$«#»«#»",
                               n00b_string_join(parts,
                                                n00b_cstring("_or_")),
                               n00b_cached_lbracket());
            break;
        }

        int num = n00b_type_get_num_params(t);

        if (num) {
            n00b_type_t   *sub = n00b_type_get_param(t, 0);
            n00b_string_t *one = n00b_internal_type_repr(sub, memos, nexttv);

            res = n00b_cformat("«#»«#»", res, one);
        }

        for (int i = 1; i < num; i++) {
            n00b_string_t *one = n00b_internal_type_repr(n00b_type_get_param(t, i),
                                                         memos,
                                                         nexttv);

            res = n00b_cformat("«#», «#»", res, one);
        }

        if (t->flags & N00B_FN_UNKNOWN_TV_LEN) {
            res = n00b_cformat("«#»...]", res);
        }
        else {
            res = n00b_cformat("«#»]", res);
        }
        return res;
    }

    if (t->name != NULL) {
        s = t->name;
    }
    else {
        int64_t v = *nexttv;
        s         = create_typevar_name(++v);
        *nexttv   = v;
    }

    s = n00b_string_concat(n00b_cached_backtick(), s);

    hatrack_dict_put(memos, t, s);

    return s;
}

static inline n00b_string_t *
internal_repr_container(n00b_type_t *type,
                        n00b_dict_t *memos,
                        int64_t     *nexttv)
{
    int            num_types = n00b_list_len(type->items);
    n00b_list_t   *to_join   = n00b_list(n00b_type_string());
    int            i         = 0;
    n00b_type_t   *subnode;
    n00b_string_t *substr;

    n00b_list_append(to_join, n00b_cstring(n00b_internal_type_name(type)));
    n00b_list_append(to_join, n00b_cached_lbracket());

    int n = type->items ? 0 : n00b_list_len(type->items);

    goto first_loop_start;

    for (; i < num_types; i++) {
        n00b_list_append(to_join, n00b_cached_comma());

first_loop_start:
        if (i >= n) {
            subnode = n00b_new_typevar();
        }
        else {
            subnode = n00b_list_get(type->items, i, NULL);
        }

        substr = n00b_internal_type_repr(subnode, memos, nexttv);

        n00b_list_append(to_join, substr);
    }

    n00b_list_append(to_join, n00b_cached_rbracket());

    return n00b_string_join(to_join, n00b_cached_empty_string());
}

// This will get more complicated when we add keyword parameter sypport.

static inline n00b_string_t *
internal_repr_func(n00b_type_t *t, n00b_dict_t *memos, int64_t *nexttv)
{
    int            num_types = n00b_list_len(t->items);
    n00b_list_t   *to_join   = n00b_list(n00b_type_string());
    int            i         = 0;
    n00b_type_t   *subnode;
    n00b_string_t *substr;

    n00b_list_append(to_join, n00b_cached_lparen());

    // num_types - 1 will be 0 if there are no args, but there is a
    // return value. So the below loop won't run in all cases.  But
    // only need to do the test for comma once, not every time through
    // the loop.

    if (num_types > 1) {
        goto first_loop_start;

        for (; i < num_types - 1; i++) {
            n00b_list_append(to_join, n00b_cached_comma());

first_loop_start:

            if ((i == num_types - 2) && t->flags & N00B_FN_TY_VARARGS) {
                n00b_list_append(to_join, n00b_cached_star());
            }

            subnode = n00b_list_get(t->items, i, NULL);
            substr  = n00b_internal_type_repr(subnode, memos, nexttv);
            n00b_list_append(to_join, substr);
        }
    }

    n00b_list_append(to_join, n00b_cached_rparen());
    n00b_list_append(to_join, n00b_cached_arrow());

    subnode = n00b_list_get(t->items, num_types - 1, NULL);

    if (subnode) {
        substr = n00b_internal_type_repr(subnode, memos, nexttv);
        n00b_list_append(to_join, substr);
    }

    return n00b_string_join(to_join, n00b_cached_empty_string());
}

static n00b_string_t *n00b_type_repr(n00b_type_t *);

n00b_string_t *
n00b_internal_type_repr(n00b_type_t *t, n00b_dict_t *memos, int64_t *nexttv)
{
    t = n00b_type_resolve(t);

    switch (n00b_type_get_kind(t)) {
    case N00B_DT_KIND_nil:
    case N00B_DT_KIND_primitive:
    case N00B_DT_KIND_internal:
        return n00b_cstring(n00b_internal_type_name(t));
    case N00B_DT_KIND_type_var:
        return internal_repr_tv(t, memos, nexttv);
    case N00B_DT_KIND_list:
    case N00B_DT_KIND_dict:
    case N00B_DT_KIND_tuple:
        return internal_repr_container(t, memos, nexttv);
    case N00B_DT_KIND_func:
        return internal_repr_func(t, memos, nexttv);
    case N00B_DT_KIND_box:
        return n00b_cformat("«#»(boxed)",
                            n00b_internal_type_repr(n00b_type_unbox(t),
                                                    memos,
                                                    nexttv));
    default:
        n00b_assert(false);
    }

    return NULL;
}

static n00b_string_t *
n00b_type_repr(n00b_type_t *t)
{
    n00b_dict_t *memos = n00b_dict(n00b_type_ref(), n00b_type_string());
    int64_t      n     = 0;

    return n00b_internal_type_repr(n00b_type_resolve(t), memos, &n);
}

const n00b_vtable_t n00b_type_spec_vtable = {
    .methods = {
        [N00B_BI_CONSTRUCTOR] = (n00b_vtable_entry)n00b_type_init,
        [N00B_BI_COPY]        = (n00b_vtable_entry)n00b_type_copy,
        [N00B_BI_GC_MAP]      = (n00b_vtable_entry)n00b_type_set_gc_bits,
        [N00B_BI_TO_STRING]   = (n00b_vtable_entry)n00b_type_repr,
        [N00B_BI_FINALIZER]   = NULL,
    },
};

static void
setup_primitive_types(void)
{
    n00b_heap_register_root(n00b_default_heap,
                            n00b_bi_types,
                            N00B_NUM_BUILTIN_DTS);

    n00b_heap_register_root(n00b_default_heap,
                            &n00b_type_universe,
                            sizeof(n00b_type_universe_t) / 8);

    n00b_alloc_base_type_object(N00B_T_TYPESPEC);

    for (int i = 0; i < N00B_NUM_BUILTIN_DTS; i++) {
        if (i == N00B_T_TYPESPEC) {
            continue;
        }

        n00b_dt_info_t *one_spec = (n00b_dt_info_t *)&n00b_base_type_info[i];

        switch (one_spec->dt_kind) {
        case N00B_DT_KIND_nil:
        case N00B_DT_KIND_primitive:
        case N00B_DT_KIND_internal:;
        case N00B_DT_KIND_box:;
            n00b_alloc_base_type_object(i);
            continue;
        default:
            continue;
        }
    }
}

static int inited = false;
void
n00b_initialize_global_types(void)
{
    if (!inited) {
        n00b_universe_init(&n00b_type_universe);
        setup_primitive_types();
        inited = true;
    }
}

#if defined(N00B_ADD_ALLOC_LOC_INFO)
#define DECLARE_ONE_PARAM_FN(tname, idnumber)                      \
    n00b_type_t *                                                  \
        _n00b_type_##tname(n00b_type_t *sub, char *file, int line) \
    {                                                              \
        n00b_type_t *ts                                            \
            = n00b_type_typespec();                                \
        n00b_type_t *result = _n00b_new(NULL,                      \
                                        file,                      \
                                        line,                      \
                                        ts,                        \
                                        idnumber,                  \
                                        0);                        \
        n00b_list_t *items  = result->items;                       \
        n00b_list_append(items, sub);                              \
                                                                   \
        type_hash_and_dedupe(&result);                             \
                                                                   \
        return result;                                             \
    }

#else
#define DECLARE_ONE_PARAM_FN(tname, idnumber)         \
    n00b_type_t *                                     \
        n00b_type_##tname(n00b_type_t *sub)           \
    {                                                 \
        n00b_type_t *ts     = n00b_type_typespec();   \
        n00b_type_t *result = n00b_new(ts, idnumber); \
        n00b_list_t *items  = result->items;          \
        n00b_list_append(items, sub);                 \
                                                      \
        type_hash_and_dedupe(&result);                \
                                                      \
        return result;                                \
    }
#endif

extern int n00b_current_test_case;
extern int n00b_watch_case;
extern int TMP_DEBUG;

#if defined(N00B_ADD_ALLOC_LOC_INFO)
n00b_type_t *
_n00b_type_list(n00b_type_t *sub, char *file, int line)
{
    n00b_type_t *result = _n00b_new(NULL,
                                    file,
                                    line,
                                    n00b_type_typespec(),
                                    N00B_T_LIST);
#else
n00b_type_t *
_n00b_type_list(n00b_type_t *sub)
{
    n00b_type_t *result = _n00b_new(NULL,
                                    n00b_type_typespec(),
                                    N00B_T_LIST);
#endif
    n00b_assert(result->base_index == N00B_T_LIST);
    n00b_private_list_append(result->items, sub);
    type_hash_and_dedupe(&result);
    return result;
}

DECLARE_ONE_PARAM_FN(flist, N00B_T_FLIST);
DECLARE_ONE_PARAM_FN(tree, N00B_T_TREE);
DECLARE_ONE_PARAM_FN(queue, N00B_T_QUEUE);
DECLARE_ONE_PARAM_FN(ring, N00B_T_RING);
DECLARE_ONE_PARAM_FN(stack, N00B_T_STACK);
DECLARE_ONE_PARAM_FN(set, N00B_T_SET);

n00b_type_t *
n00b_type_box(n00b_type_t *sub)
{
    int64_t n;

    switch (sub->base_index) {
    case N00B_T_BOOL:
        n = N00B_T_BOX_BOOL;
        break;
    case N00B_T_I8:
        n = N00B_T_BOX_I8;
        break;

    case N00B_T_BYTE:
        n = N00B_T_BOX_BYTE;
        break;

    case N00B_T_I32:
        n = N00B_T_BOX_I32;
        break;

    case N00B_T_CHAR:
        n = N00B_T_BOX_CHAR;
        break;

    case N00B_T_U32:
        n = N00B_T_BOX_U32;
        break;

    case N00B_T_INT:
        n = N00B_T_BOX_INT;
        break;

    case N00B_T_UINT:
        n = N00B_T_BOX_UINT;
        break;

    case N00B_T_F32:
        n = N00B_T_BOX_F32;
        break;

    case N00B_T_F64:
        n = N00B_T_BOX_F64;
        break;

    default:
        return sub;
    }

    return n00b_bi_types[n];
}

n00b_type_t *
n00b_type_dict(n00b_type_t *sub1, n00b_type_t *sub2)
{
    n00b_type_t *result = n00b_new(n00b_type_typespec(),
                                   N00B_T_DICT);
    n00b_list_t *items  = result->items;

    n00b_private_list_append(items, sub1);
    n00b_private_list_append(items, sub2);

    type_hash_and_dedupe(&result);

    return result;
}

n00b_type_t *
n00b_type_tuple(int64_t nitems, ...)
{
    va_list      args;
    n00b_type_t *result = n00b_new(n00b_type_typespec(), N00B_T_TUPLE);
    n00b_list_t *items  = result->items;

    va_start(args, nitems);

    if (nitems <= 1) {
        N00B_CRAISE("Tuples must contain 2 or more items.");
    }

    for (int i = 0; i < nitems; i++) {
        n00b_type_t *sub = va_arg(args, n00b_type_t *);
        n00b_list_append(items, sub);
    }

    type_hash_and_dedupe(&result);

    return result;
}

n00b_type_t *
n00b_type_tuple_from_list(n00b_list_t *items)
{
    n00b_type_t *result = n00b_new(n00b_type_typespec(), N00B_T_TUPLE);

    result->items = items;

    type_hash_and_dedupe(&result);

    return result;
}

n00b_type_t *
n00b_type_fn_va(n00b_type_t *return_type, int64_t nparams, ...)
{
    va_list      args;
    n00b_type_t *result = n00b_new(n00b_type_typespec(),
                                   N00B_T_FUNCDEF);
    n00b_list_t *items  = result->items;

    va_start(args, nparams);

    for (int i = 0; i < nparams; i++) {
        n00b_type_t *sub = va_arg(args, n00b_type_t *);
        n00b_list_append(items, sub);
    }
    n00b_list_append(items, return_type);

    type_hash_and_dedupe(&result);

    return result;
}

// This one explicitly sets the varargs flag, as opposed to the one above that
// simply just takes variable # of args as an input.
n00b_type_t *
n00b_type_varargs_fn(n00b_type_t *return_type, int64_t nparams, ...)
{
    va_list      args;
    n00b_type_t *result = n00b_new(n00b_type_typespec(),
                                   N00B_T_FUNCDEF);
    n00b_list_t *items  = result->items;

    va_start(args, nparams);

    if (nparams < 1) {
        N00B_CRAISE("Varargs functions require at least one argument.");
    }

    for (int i = 0; i < nparams; i++) {
        n00b_type_t *sub = va_arg(args, n00b_type_t *);
        n00b_list_append(items, sub);
    }
    n00b_list_append(items, return_type);
    result->flags |= N00B_FN_TY_VARARGS;

    type_hash_and_dedupe(&result);

    return result;
}

n00b_type_t *
n00b_type_fn(n00b_type_t *ret, n00b_list_t *params, bool va)
{
    n00b_type_t *result = n00b_new(n00b_type_typespec(),
                                   N00B_T_FUNCDEF);
    n00b_list_t *items  = result->items;
    int          n      = n00b_list_len(params);

    // Copy the list to be safe.
    for (int i = 0; i < n; i++) {
        n00b_list_append(items, n00b_list_get(params, i, NULL));
    }
    n00b_list_append(items, ret);

    if (va) {
        result->flags |= N00B_FN_TY_VARARGS;
    }
    type_hash_and_dedupe(&result);

    return result;
}

n00b_type_t *
n00b_get_builtin_type(n00b_builtin_t base_id)
{
    return n00b_bi_types[base_id];
}

n00b_type_t *
n00b_get_promotion_type(n00b_type_t *t1, n00b_type_t *t2, int *warning)
{
    *warning = 0;

    t1 = n00b_type_resolve(t1);
    t2 = n00b_type_resolve(t2);

    if (n00b_type_is_box(t1)) {
        t1 = n00b_type_unbox(t1);
    }
    if (n00b_type_is_box(t2)) {
        t2 = n00b_type_unbox(t2);
    }

    n00b_type_hash_t id1 = t1->base_index;
    n00b_type_hash_t id2 = t2->base_index;

    // clang-format off
     if (id1 < N00B_T_I8 || id1 > N00B_T_UINT ||
         id2 < N00B_T_I8 || id2 > N00B_T_UINT) {
        // clang-format on
        *warning = -1;
        return n00b_type_error();
    }

    if (id2 > id1) {
        n00b_type_hash_t swap = id1;
        id1                   = id2;
        id2                   = swap;
    }

    switch (id1) {
    case N00B_T_UINT:
        switch (id2) {
        case N00B_T_INT:
            *warning = 1; // Might wrap.;
                          // fallthrough
        case N00B_T_I32:
        case N00B_T_I8:
            return n00b_type_i64();
        default:
            return n00b_type_u64();
        }
    case N00B_T_U32:
    case N00B_T_CHAR:
        switch (id2) {
        case N00B_T_I32:
            *warning = 1;
            // fallthrough
        case N00B_T_I8:
            return n00b_type_i32();
        default:
            return n00b_type_u32();
        }
    case N00B_T_BYTE:
        if (id2 != N00B_T_BYTE) {
            *warning = 1;
            return n00b_type_i8();
        }
        return n00b_type_byte();
    case N00B_T_INT:
        return n00b_type_i64();
    case N00B_T_I32:
        return n00b_type_i32();
    default:
        return n00b_type_i8();
    }
}

/* void */
/* n00b_clean_environment(void) */
/* { */
/*     n00b_type_universe_t *new_env = n00b_new_type_universe(); */
/*     hatrack_dict_item_t *items; */
/*     uint64_t             len; */

/*     items = hatrack_dict_items_sort(n00b_type_universe->store, &len); */

/*     for (uint64_t i = 0; i < len; i++) { */
/*         n00b_type_t *t = items[i].value; */

/*         if (t->typeid != (uint64_t)items[i].key) { */
/*             continue; */
/*         } */

/*         if (n00b_type_get_kind(t) != N00B_DT_KIND_type_var) { */
/*             hatrack_dict_put(new_env->store, (void *)t->typeid, t); */
/*         } */

/*         int nparams = n00b_list_len(t->items); */
/*         for (int i = 0; i < nparams; i++) { */
/*             n00b_type_t *it = n00b_type_get_param(t, i); */
/*             it             = n00b_type_resolve(it); */

/*             if (n00b_type_get_kind(it) == N00B_DT_KIND_type_var) { */
/*                 hatrack_dict_put(new_env->store, (void *)t->typeid, t); */
/*             } */
/*         } */
/*     } */

/*     n00b_type_universe = new_env; */
/* } */

n00b_type_t *
n00b_new_typevar()
{
    n00b_type_t *result = n00b_new(n00b_type_typespec(),
                                   N00B_T_GENERIC);

    result->options.container_options = n00b_get_all_containers_bitfield();
    result->items                     = n00b_list(n00b_type_typespec());
    result->flags                     = N00B_FN_UNKNOWN_TV_LEN;

    return result;
}
