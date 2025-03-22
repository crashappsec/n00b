#pragma once
#include "n00b.h"

#define N00B_MD_GITHUB MD_DIALECT_GITHUB

typedef enum {
    N00B_MD_BLOCK_BODY,
    N00B_MD_BLOCK_QUOTE,
    N00B_MD_BLOCK_UL,
    N00B_MD_BLOCK_OL,
    N00B_MD_BLOCK_LI,
    N00B_MD_BLOCK_HR,
    N00B_MD_BLOCK_H,
    N00B_MD_BLOCK_CODE,
    N00B_MD_BLOCK_HTML,
    N00B_MD_BLOCK_P,
    N00B_MD_BLOCK_TABLE,
    N00B_MD_BLOCK_THEAD,
    N00B_MD_BLOCK_TBODY,
    N00B_MD_BLOCK_TR,
    N00B_MD_BLOCK_TH,
    N00B_MD_BLOCK_TD,
    N00B_MD_SPAN_EM,
    N00B_MD_SPAN_STRONG,
    N00B_MD_SPAN_A,
    N00B_MD_SPAN_A_SELF,
    N00B_MD_SPAN_A_CODELINK,
    N00B_MD_SPAN_IMG,
    N00B_MD_SPAN_CODE,
    N00B_MD_SPAN_STRIKETHRU,
    N00B_MD_SPAN_LATEX,
    N00B_MD_SPAN_LATEX_DISPLAY,
    N00B_MD_SPAN_WIKI_LINK,
    N00B_MD_SPAN_U,
    N00B_MD_TEXT_NORMAL,
    N00B_MD_TEXT_NULL,
    N00B_MD_TEXT_BR,
    N00B_MD_TEXT_SOFTBR,
    N00B_MD_TEXT_ENTITY,
    N00B_MD_TEXT_CODE,
    N00B_MD_TEXT_HTML,
    N00B_MD_TEXT_LATEX,
    N00B_MD_DOCUMENT,
} n00b_md_node_kind_t;

#define convert_block_kind(x) (n00b_md_node_kind_t)(x)
#define convert_span_kind(x)  (n00b_md_node_kind_t)(x + (int)N00B_MD_SPAN_EM)
#define convert_text_kind(x)  (n00b_md_node_kind_t)(x + (int)N00B_MD_TEXT_NORMAL)

typedef enum {
    N00B_MD_ALIGN_DEFAULT,
    N00B_MD_ALIGN_LEFT,
    N00B_MD_ALIGN_CENTER,
    N00B_MD_ALIGN_RIGHT,
} n00b_md_align_type;

typedef union {
#ifdef _MD4C_INLINE_IN_THIS_MODULE

    MD_BLOCK_UL_DETAIL      ul;
    MD_BLOCK_OL_DETAIL      ol;
    MD_BLOCK_LI_DETAIL      li;
    MD_BLOCK_H_DETAIL       h;
    MD_BLOCK_CODE_DETAIL    code;
    MD_BLOCK_TABLE_DETAIL   table;
    MD_BLOCK_TD_DETAIL      td;
    MD_SPAN_A_DETAIL        a;
    MD_SPAN_IMG_DETAIL      img;
    MD_SPAN_WIKILINK_DETAIL wiki;
#endif
    void        *v;
    n00b_string_t *text;
} n00b_md_detail_t;

typedef struct {
    n00b_md_node_kind_t node_type;
    n00b_md_detail_t    detail;
} n00b_md_node_t;

typedef struct {
    n00b_tree_node_t *cur;
} n00b_md_build_ctx;

extern n00b_tree_node_t *n00b_parse_markdown(n00b_string_t *);
extern n00b_table_t     *n00b_repr_md_parse(n00b_tree_node_t *);
extern n00b_table_t     *n00b_markdown_to_table(n00b_string_t *, bool);
