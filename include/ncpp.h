// This is not part of the n00b library itself. It's a small,
// lightweight preprocessor for C (with some constraints most people
// wouldn't notice).
//
// I'm avoiding using the n00b library in this preprocessor so that we
// don't end up bending over backwards to deal w/ bootstrapping
// problems.
//
// The approach is to be janky (on purpose). We tokenize
// conservatively enough. Transformations work on the token stream
// directly, and are free to do their own localized parsing. But we do
// not try to fully parse C.
//
// That's not really so much due to the overall complexity of C (esp
// if one tries to handle all the common compiler extensions). It's
// more because C-style conditional compilation makes keeping trees
// pretty abhorant.
//
// To that end, while this does pass through C preprocessor
// directives, any transformations I add will not take into account
// the preprocessor (whereas, I do take into account comments and pass
// them through reasonably).
//
// That means, inside the constructs I add, while you may add
// comments, you must not use the C preprocessor!

#pragma once
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/mman.h>
#include "n00b/config.h"

#define MMAP_PROTS (PROT_READ | PROT_WRITE)
#define MMAP_FLAGS (MAP_PRIVATE | MAP_ANON)


// Define this to remove #line directives, for debugging.
// #define SKIP_LINE_DIRECTIVES

typedef struct {
    int64_t len;
    char    data[];
} buf_t;

typedef int8_t ttype_t;

#define TT_ERR     -1
#define TT_WS      0
#define TT_ID      1
#define TT_NUM     2
#define TT_COMMENT 3
#define TT_STR     4
#define TT_CHR     5
#define TT_PUNCT   6
#define TT_PREPROC 7
#define TT_UNKNOWN 8

typedef struct {
    buf_t  *replacement;
    int32_t offset;
    int16_t len;
    int16_t line_no;
    ttype_t type;
    // Tokens we choose to rewrite get "skipped", but the "replacement"
    // text will all live in the first token of the rewrite.
    uint8_t skip_emit : 1;
} tok_t;

typedef struct {
    buf_t  *input;
    tok_t  *toks;
    char   *cur;
    char   *end;
    char   *in_file;
    char   *out_file;
    int32_t num_toks;
    int32_t offset;
    int32_t line_no;
    bool    line_start;
} lex_t;

typedef struct kw_use_ctx_t {
    struct kw_use_ctx_t *next;
    bool                 id;
    bool                 started_kobj;
    int                  kw_count;
} kw_use_ctx_t;

typedef struct {
    buf_t        *input;
    tok_t        *toks;
    char         *in_file;
    int           ix;
    int           max;
    int           rewrite_start_ix;
    int           id_nest;
    kw_use_ctx_t *kw_stack;
} xform_t;

typedef struct {
    int   nitems;
    void *items[];
} list_t;

static inline list_t *
list_alloc(int nitems)
{
    list_t *result = calloc(1, sizeof(list_t) + sizeof(void *) * nitems);
    result->nitems = nitems;

    return result;
}

// In utils.c
extern buf_t  *concat(buf_t *, char *, int);
extern buf_t  *concat_static(buf_t *, char *);
extern buf_t  *read_file(char *);
extern tok_t  *alloc_tokens(buf_t *);
extern int     count_newlines(lex_t *, tok_t *);
extern list_t *get_wrapper_actuals(xform_t *, int, int);
extern char   *join(list_t *, char *);

// In output.c
// First one is for debugging the token stream.
extern void print_tokens(lex_t *);
// This generates the output file.
extern bool output_new_file(lex_t *, char *);
extern bool write_to_file(FILE *f, char *, int);

// In tokenize.c
extern void   lex(lex_t *);
extern tok_t *advance(xform_t *, bool);
extern tok_t *backup(xform_t *, bool);
extern bool   id_check(char *, buf_t *, int, int);
extern char  *extract(buf_t *, tok_t *);
extern int    scan_back(xform_t *, int, ttype_t, char *);
extern int    scan_forward(xform_t *, int, ttype_t, char *);
extern tok_t *cur_tok(xform_t *);
extern tok_t *lookahead(xform_t *, int);
extern tok_t *lookbehind(xform_t *, int);

// In xforms.c
extern bool   apply_transforms(lex_t *);
extern void   finish_rewrite(xform_t *, bool);
extern char  *extract_line(xform_t *, tok_t *);
extern char  *extract_range(xform_t *, int, int);
extern tok_t *skip_forward_to_punct(xform_t *, char);

// In x_keyword.c
extern bool keyword_xform(xform_t *, tok_t *);
extern void kw_tracking(xform_t *, tok_t *);

// x_once.c
extern void once_xform(xform_t *, tok_t *);
