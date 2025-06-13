#include "n00b.h"

// Note: This is not yet multi-thread safe. My intent is to
// do this with a pthread_mutex object at some point once we
// actually start adding threading support.

static void
flags_init(n00b_flags_t *self, va_list args)
{
    keywords
    {
        int64_t length = -1;
    }

    self->num_flags = (uint32_t)length;

    // Always allocate at least a word, even if there's no length provided.
    if (length < 0) {
        length = 64;
    }

    self->bit_modulus   = length % 64;
    self->alloc_wordlen = (length + 63) / 64;

    self->contents = n00b_gc_raw_alloc(sizeof(uint64_t) * self->alloc_wordlen,
                                       NULL);
}

n00b_flags_t *
n00b_flags_copy(const n00b_flags_t *self)
{
    n00b_flags_t *result = n00b_new(n00b_type_flags(),
                                    n00b_kw("length", n00b_ka(self->num_flags)));

    for (int i = 0; i < result->alloc_wordlen; i++) {
        result->contents[i] = self->contents[i];
    }

    return result;
}

n00b_flags_t *
n00b_flags_invert(n00b_flags_t *self)
{
    n00b_flags_t *result = n00b_flags_copy(self);

    for (int i = 0; i < result->alloc_wordlen; i++) {
        result->contents[i] = ~self->contents[i];
    }

    if (!result->bit_modulus) {
        return result;
    }

    int      num_to_clear = 64 - result->bit_modulus;
    uint64_t mask         = (1ULL << num_to_clear) - 1;

    result->contents[result->alloc_wordlen - 1] &= mask;

    return result;
}

static n00b_string_t *
flags_repr(const n00b_flags_t *self)
{
    n00b_string_t *result;
    int            n = self->alloc_wordlen;

    if (self->bit_modulus) {
        result = n00b_cformat("0x«#:x»", (int64_t)self->contents[--n]);
    }
    else {
        result = n00b_cstring("0x");
    }

    while (n--) {
        result = n00b_cformat("«#»«#:x»", result, (int64_t)self->contents[n]);
    }

    return result;
}

static n00b_flags_t *
flags_lit(n00b_string_t        *s,
          n00b_lit_syntax_t     st,
          n00b_string_t        *m,
          n00b_compile_error_t *code)
{
    int64_t       len    = n00b_string_codepoint_len(s);
    n00b_flags_t *result = n00b_new(n00b_type_flags(),
                                    n00b_kw("length", n00b_ka(len * 4)));

    uint64_t cur_word = 0;
    int      ct       = 0;
    int      alloc_ix = 0;

    while (--len) {
        cur_word <<= 4;
        ct++;

        switch (s->data[--len]) {
        case '0':
            break;
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            cur_word |= (s->data[len] - '0');
            break;
        case 'a':
        case 'b':
        case 'c':
        case 'd':
        case 'e':
        case 'f':
            cur_word |= (s->data[len] - 'a' + 10);
            break;
        case 'A':
        case 'B':
        case 'C':
        case 'D':
        case 'E':
        case 'F':
            cur_word |= (s->data[len] - 'A' + 10);
            break;
        default:
            *code = n00b_err_parse_lit_bad_flags;
            return NULL;
        }
        if (!(ct % 16)) {
            result->contents[alloc_ix++] = cur_word;
            cur_word                     = 0;
        }
    }

    if (ct % 16) {
        result->contents[alloc_ix] = cur_word;
    }

    return result;
}

n00b_flags_t *
n00b_flags_add(n00b_flags_t *self, n00b_flags_t *with)
{
    uint32_t      res_flags   = n00b_max(self->num_flags, with->num_flags);
    n00b_flags_t *result      = n00b_new(n00b_type_flags(),
                                    n00b_kw("length", n00b_ka(res_flags)));
    int32_t       min_wordlen = n00b_min(self->alloc_wordlen, with->alloc_wordlen);

    for (int i = 0; i < min_wordlen; i++) {
        result->contents[i] = self->contents[i] | with->contents[i];
    }

    if (self->alloc_wordlen > with->alloc_wordlen) {
        for (int i = min_wordlen; i < result->alloc_wordlen; i++) {
            result->contents[i] = self->contents[i];
        }
    }
    else {
        // If they're equal, this loop won't run.
        for (int i = min_wordlen; i < result->alloc_wordlen; i++) {
            result->contents[i] = with->contents[i];
        }
    }

    return result;
}

n00b_flags_t *
n00b_flags_sub(n00b_flags_t *self, n00b_flags_t *with)
{
    // If one flag set is longer, we auto-expand the result, even if
    // it's the rhs, as it implies the lhs has unset flags, but
    // they're valid.

    uint32_t      res_flags   = n00b_max(self->num_flags, with->num_flags);
    n00b_flags_t *result      = n00b_new(n00b_type_flags(),
                                    n00b_kw("length", n00b_ka(res_flags)));
    int32_t       min_wordlen = n00b_min(self->alloc_wordlen, with->alloc_wordlen);
    int           i;

    for (i = 0; i < min_wordlen; i++) {
        result->contents[i] = self->contents[i] & ~with->contents[i];
    }

    if (self->alloc_wordlen > with->alloc_wordlen) {
        for (; i < result->alloc_wordlen; i++) {
            result->contents[i] = self->contents[i];
        }
    }

    return result;
}

n00b_flags_t *
n00b_flags_test(n00b_flags_t *self, n00b_flags_t *with)
{
    // If one flag set is longer, we auto-expand the result, even if
    // it's the rhs, as it implies the lhs has unset flags, but
    // they're valid.

    uint32_t      res_flags   = n00b_max(self->num_flags, with->num_flags);
    n00b_flags_t *result      = n00b_new(n00b_type_flags(),
                                    n00b_kw("length", n00b_ka(res_flags)));
    int32_t       min_wordlen = n00b_min(self->alloc_wordlen, with->alloc_wordlen);

    for (int i = 0; i < min_wordlen; i++) {
        result->contents[i] = self->contents[i] & with->contents[i];
    }

    return result;
}

n00b_flags_t *
n00b_flags_xor(n00b_flags_t *self, n00b_flags_t *with)
{
    uint32_t      res_flags   = n00b_max(self->num_flags, with->num_flags);
    n00b_flags_t *result      = n00b_new(n00b_type_flags(),
                                    n00b_kw("length", n00b_ka(res_flags)));
    int32_t       min_wordlen = n00b_min(self->alloc_wordlen, with->alloc_wordlen);

    for (int i = 0; i < min_wordlen; i++) {
        result->contents[i] = self->contents[i] ^ with->contents[i];
    }

    if (self->alloc_wordlen > with->alloc_wordlen) {
        for (int i = min_wordlen; i < result->alloc_wordlen; i++) {
            result->contents[i] = self->contents[i];
        }
    }
    else {
        for (int i = min_wordlen; i < result->alloc_wordlen; i++) {
            result->contents[i] = with->contents[i];
        }
    }

    return result;
}

bool
n00b_flags_eq(n00b_flags_t *self, n00b_flags_t *other)
{
    // We do this in as close to constant time as possible to avoid
    // any side channels.

    uint64_t      sum = 0;
    int           i;
    int32_t       nlow  = n00b_min(self->alloc_wordlen, other->alloc_wordlen);
    int32_t       nhigh = n00b_max(self->alloc_wordlen, other->alloc_wordlen);
    n00b_flags_t *high_ptr;

    if (self->alloc_wordlen == nhigh) {
        high_ptr = self;
    }
    else {
        high_ptr = other;
    }

    for (i = 0; i < nlow; i++) {
        sum += self->contents[i] ^ other->contents[i];
    }

    for (; i < nhigh; i++) {
        sum += high_ptr->contents[i];
    }

    return sum == 0;
}

uint64_t
n00b_flags_len(n00b_flags_t *self)
{
    return self->num_flags;
}

static void
resize_flags(n00b_flags_t *self, uint64_t new_num)
{
    int32_t new_words   = new_num / 64;
    int32_t new_modulus = new_num % 64;

    if (new_words < self->alloc_wordlen) {
        uint64_t *contents = n00b_gc_raw_alloc(sizeof(uint64_t) * new_words,
                                               NULL);

        for (int i = 0; i < self->alloc_wordlen; i++) {
            contents[i] = self->contents[i];
        }

        self->contents = contents;
    }

    self->bit_modulus   = new_modulus;
    self->alloc_wordlen = new_words;
    self->num_flags     = new_num;
}

bool
n00b_flags_index(n00b_flags_t *self, int64_t n)
{
    if (n < 0) {
        n += self->num_flags;

        if (n < 0) {
            N00B_CRAISE("Negative index is out of bounds.");
        }
    }

    if (n >= self->num_flags) {
        resize_flags(self, n);
        // It will never be set.
        return false;
    }

    uint32_t word_ix = n / 64;
    uint32_t offset  = n % 64;
    uint64_t flag    = 1UL << offset;

    return (self->contents[word_ix] & flag) != 0;
}

void
n00b_flags_set_index(n00b_flags_t *self, int64_t n, bool value)
{
    if (n < 0) {
        n += self->num_flags;

        if (n < 0) {
            N00B_CRAISE("Negative index is out of bounds.");
        }
    }

    if (n >= self->num_flags) {
        resize_flags(self, n);
    }

    uint32_t word_ix = n / 64;
    uint32_t offset  = n % 64;
    uint64_t flag    = 1UL << offset;

    if (value) {
        self->contents[word_ix] |= flag;
    }
    else {
        self->contents[word_ix] &= ~flag;
    }
}

#if 0
// DO THESE LATER; NOT IMPORTANT RIGHT NOW.
n00b_flags_t *
n00b_flags_slice(n00b_flags_t *self, int64_t start, int64_t end)
{
    if (start < 0) {
        start += len;
    }
    else {
        if (start >= len) {
            return n00b_new(n00b_type_flags());
        }
    }
    if (end < 0) {
        end += len + 1;
    }
    else {
        if (end > len) {
            end = len;
        }
    }
    if ((start | end) < 0 || start >= end) {
        return n00b_new(n00b_type_flags());
    }
}
#endif

static inline n00b_type_t *
flags_item_type(n00b_obj_t ignore)
{
    return n00b_type_bit();
}

// For iterating over other container types, we just take a pointer to
// memory and a length, which is what we get from hatrack's views.
//
// Because indexing into a bit field is a bit more complicated
// (advancing a pointer one bit doesn't make sense), we avoid
// duplicating the code in the code generator, and just accept that
// views based on data types with single-bit items are going to use
// the index interface.

static inline void *
flags_view(n00b_flags_t *self, uint64_t *n)
{
    n00b_flags_t *copy = n00b_flags_copy(self);
    *n                 = copy->num_flags;

    return copy;
}

const n00b_vtable_t n00b_flags_vtable = {
    .methods = {
        [N00B_BI_CONSTRUCTOR]  = (n00b_vtable_entry)flags_init,
        [N00B_BI_TO_STRING]    = (n00b_vtable_entry)flags_repr,
        [N00B_BI_FROM_LITERAL] = (n00b_vtable_entry)flags_lit,
        [N00B_BI_COPY]         = (n00b_vtable_entry)n00b_flags_copy,
        [N00B_BI_ADD]          = (n00b_vtable_entry)n00b_flags_add,
        [N00B_BI_SUB]          = (n00b_vtable_entry)n00b_flags_sub,
        [N00B_BI_EQ]           = (n00b_vtable_entry)n00b_flags_eq, // EQ
        [N00B_BI_LEN]          = (n00b_vtable_entry)n00b_flags_len,
        [N00B_BI_INDEX_GET]    = (n00b_vtable_entry)n00b_flags_index,
        [N00B_BI_INDEX_SET]    = (n00b_vtable_entry)n00b_flags_set_index,
        [N00B_BI_ITEM_TYPE]    = (n00b_vtable_entry)flags_item_type,
        [N00B_BI_VIEW]         = (n00b_vtable_entry)flags_view,
#if 0
        [N00B_BI_SLICE_GET]    = (n00b_vtable_entry)n00b_flags_slice,
        [N00B_BI_SLICE_SET]    = (n00b_vtable_entry)n00b_flags_set_slice,
#endif
        [N00B_BI_FINALIZER] = NULL,
    },
};
