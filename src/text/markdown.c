#define _MD4C_INLINE_IN_THIS_MODULE
#include "n00b.h"

static int
enter_md_block(MD_BLOCKTYPE type, void *detail, void *extra)
{
    n00b_md_build_ctx *ctx  = (n00b_md_build_ctx *)extra;
    n00b_md_node_t    *node = n00b_gc_alloc_mapped(n00b_md_build_ctx,
                                                N00B_GC_SCAN_ALL);

    node->node_type = convert_block_kind(type);

    switch (type) {
    case MD_BLOCK_UL:;
        MD_BLOCK_UL_DETAIL *ul = (MD_BLOCK_UL_DETAIL *)detail;
        node->detail.ul        = *ul;
        break;
    case MD_BLOCK_OL:;
        MD_BLOCK_OL_DETAIL *ol = (MD_BLOCK_OL_DETAIL *)detail;
        node->detail.ol        = *ol;
        break;
    case MD_BLOCK_LI:;
        MD_BLOCK_LI_DETAIL *li = (MD_BLOCK_LI_DETAIL *)detail;
        node->detail.li        = *li;
        break;
    case MD_BLOCK_H:;
        MD_BLOCK_H_DETAIL *h = (MD_BLOCK_H_DETAIL *)detail;
        node->detail.h       = *h;
        break;
    case MD_BLOCK_CODE:;
        MD_BLOCK_CODE_DETAIL *code = (MD_BLOCK_CODE_DETAIL *)detail;
        node->detail.code          = *code;
        break;
    case MD_BLOCK_TABLE:;
        MD_BLOCK_TABLE_DETAIL *table = (MD_BLOCK_TABLE_DETAIL *)detail;
        node->detail.table           = *table;
        break;
    case MD_BLOCK_TH:
    case MD_BLOCK_TD:;
        MD_BLOCK_TD_DETAIL *td = (MD_BLOCK_TD_DETAIL *)detail;
        node->detail.td        = *td;
        break;
    default:
        break;
    }

    ctx->cur = n00b_tree_add_node(ctx->cur, node);

    return 0;
}

static int
exit_md_block(MD_BLOCKTYPE type, void *detail, void *extra)
{
    n00b_md_build_ctx *ctx = (n00b_md_build_ctx *)extra;

    ctx->cur = ctx->cur->parent;
    return 0;
}

static int
enter_md_span(MD_SPANTYPE type, void *detail, void *extra)
{
    n00b_md_build_ctx *ctx  = (n00b_md_build_ctx *)extra;
    n00b_md_node_t    *node = n00b_gc_alloc_mapped(n00b_md_build_ctx,
                                                N00B_GC_SCAN_ALL);

    node->node_type = convert_span_kind(type);

    switch (type) {
    case MD_SPAN_A:
    case MD_SPAN_A_CODELINK:
    case MD_SPAN_A_SELF:;
        MD_SPAN_A_DETAIL *a = (MD_SPAN_A_DETAIL *)detail;
        node->detail.a      = *a;
        break;
    case MD_SPAN_IMG:;
        MD_SPAN_IMG_DETAIL *img = (MD_SPAN_IMG_DETAIL *)detail;
        node->detail.img        = *img;
        break;
    case MD_SPAN_WIKILINK:;
        MD_SPAN_WIKILINK_DETAIL *wiki = (MD_SPAN_WIKILINK_DETAIL *)detail;
        node->detail.wiki             = *wiki;
        break;
    default:
        break;
    }

    ctx->cur = n00b_tree_add_node(ctx->cur, node);

    return 0;
}

static int
exit_md_span(MD_SPANTYPE type, void *detail, void *extra)
{
    n00b_md_build_ctx *ctx = (n00b_md_build_ctx *)extra;

    ctx->cur = ctx->cur->parent;
    return 0;
}

static int
md_text(MD_TEXTTYPE type, const MD_CHAR *text, MD_SIZE size, void *extra)
{
    n00b_md_build_ctx *ctx  = (n00b_md_build_ctx *)extra;
    n00b_md_node_t    *node = n00b_gc_alloc_mapped(n00b_md_build_ctx,
                                                N00B_GC_SCAN_ALL);

    node->node_type   = convert_text_kind(type);
    node->detail.text = n00b_utf8(text, size);

    n00b_tree_add_node(ctx->cur, node);

    return 0;
}

static inline n00b_string_t *
md_node_type_to_name(n00b_md_node_t *n)
{
    char *s = NULL;

    switch (n->node_type) {
    case N00B_MD_BLOCK_BODY:
        s = "body";
        break;
    case N00B_MD_BLOCK_QUOTE:
        s = "quote";
        break;
    case N00B_MD_BLOCK_UL:
        s = "ul";
        break;
    case N00B_MD_BLOCK_OL:
        s = "ol";
        break;
    case N00B_MD_BLOCK_LI:
        s = "li";
        break;
    case N00B_MD_BLOCK_HR:
        s = "hr";
        break;
    case N00B_MD_BLOCK_H:
        switch (n->detail.h.level) {
        case 1:
            s = "h1";
            break;
        case 2:
            s = "h2";
            break;
        case 3:
            s = "h3";
            break;
        case 4:
            s = "h4";
            break;
        case 5:
            s = "h5";
            break;
        case 6:
            s = "h6";
            break;
        default:
            s = "h?";
            break;
        }
        break;
    case N00B_MD_BLOCK_CODE:
        s = "code_block";
        break;
    case N00B_MD_BLOCK_HTML:
        s = "html";
        break;
    case N00B_MD_BLOCK_P:
        s = "p";
        break;
    case N00B_MD_BLOCK_TABLE:
        s = "table";
        break;
    case N00B_MD_BLOCK_THEAD:
        s = "thead";
        break;
    case N00B_MD_BLOCK_TBODY:
        s = "tbody";
        break;
    case N00B_MD_BLOCK_TR:
        s = "tr";
        break;
    case N00B_MD_BLOCK_TH:
        s = "th";
        break;
    case N00B_MD_BLOCK_TD:
        s = "td";
        break;
    case N00B_MD_SPAN_EM:
        s = "em";
        break;
    case N00B_MD_SPAN_STRONG:
        s = "strong";
        break;
    case N00B_MD_SPAN_A:
        s = "a";
        break;
    case N00B_MD_SPAN_A_SELF:
        s = "a_self";
        break;
    case N00B_MD_SPAN_A_CODELINK:
        s = "a_codelink";
        break;
    case N00B_MD_SPAN_IMG:
        s = "image";
        break;
    case N00B_MD_SPAN_CODE:
        s = "code_span";
        break;
    case N00B_MD_SPAN_STRIKETHRU:
        s = "strikethru";
        break;
    case N00B_MD_SPAN_LATEX:
        s = "latex";
        break;
    case N00B_MD_SPAN_LATEX_DISPLAY:
        s = "latex_display";
        break;
    case N00B_MD_SPAN_WIKI_LINK:
        s = "wiki_link";
        break;
    case N00B_MD_SPAN_U:
        s = "u";
        break;
    case N00B_MD_TEXT_NORMAL:
        s = "normal";
        break;
    case N00B_MD_TEXT_NULL:
        s = "null";
        break;
    case N00B_MD_TEXT_BR:
        s = "br";
        break;
    case N00B_MD_TEXT_SOFTBR:
        s = "br_soft";
        break;
    case N00B_MD_TEXT_ENTITY:
        s = "entity";
        break;
    case N00B_MD_TEXT_CODE:
        s = "code_text";
        break;
    case N00B_MD_TEXT_HTML:
        s = "html_text";
        break;
    case N00B_MD_TEXT_LATEX:
        s = "latex_text";
        break;
    case N00B_MD_DOCUMENT:
        s = "document";
        break;
    }

    return n00b_cstring(s);
}

static n00b_string_t *
n00b_repr_md_node(n00b_md_node_t *node)
{
    n00b_string_t *ret = n00b_cformat("«green»«#»«/»",
                                      md_node_type_to_name(node));
    n00b_string_t *extra;

    switch (node->node_type) {
    case N00B_MD_TEXT_NORMAL:
    case N00B_MD_TEXT_NULL:
    case N00B_MD_TEXT_BR:
    case N00B_MD_TEXT_SOFTBR:
    case N00B_MD_TEXT_ENTITY:
    case N00B_MD_TEXT_CODE:
    case N00B_MD_TEXT_HTML:
    case N00B_MD_TEXT_LATEX:
        extra = n00b_cformat(" - «em»«#»«/»", node->detail.text);
        break;
    default:
        return ret;
    }

    return n00b_string_concat(ret, extra);
}

n00b_tree_node_t *
n00b_parse_markdown(n00b_string_t *s)
{
    MD_PARSER parser = {
        0,
        N00B_MD_GITHUB,
        enter_md_block,
        exit_md_block,
        enter_md_span,
        exit_md_span,
        md_text,
        NULL,
        NULL,
    };

    n00b_md_node_t   *r = n00b_gc_alloc_mapped(n00b_md_node_t,
                                             N00B_GC_SCAN_ALL);
    n00b_tree_node_t *t = n00b_new_tree_node(n00b_type_ref(), r);
    r->node_type        = N00B_MD_DOCUMENT;

    n00b_md_build_ctx build_ctx = {
        .cur = t,
    };

    md_parse(s->data, n00b_string_codepoint_len(s), &parser, (void *)&build_ctx);

    return t;
}

n00b_table_t *
n00b_repr_md_parse(n00b_tree_node_t *t)
{
    return n00b_tree_format(t, n00b_repr_md_node, NULL, false);
}

typedef struct {
    n00b_tree_node_t *cur_node;
    n00b_list_t      *entities;
    n00b_string_t    *str;
    n00b_table_t     *table;
    int               row_count;
    bool              keep_soft_newlines;
} md_table_ctx;

static inline void
md_newline(md_table_ctx *ctx)
{
    if (ctx->str) {
        n00b_list_append(ctx->entities, n00b_cformat("«p»«#»«/»", ctx->str));
        ctx->str = NULL;
    }
}

static inline void
md_add_string(md_table_ctx *ctx, n00b_string_t *s)
{
    if (!ctx->str) {
        ctx->str = s;
    }
    else {
        ctx->str = n00b_string_concat(ctx->str, s);
    }
}

static void md_node_to_table(md_table_ctx *);

static inline void
process_kids(md_table_ctx *ctx)
{
    n00b_tree_node_t *saved_node = ctx->cur_node;

    for (int i = 0; i < saved_node->num_kids; i++) {
        ctx->cur_node = saved_node->children[i];
        md_node_to_table(ctx);
    }

    ctx->cur_node = saved_node;
}

static void
md_block_merge(md_table_ctx *ctx, bool block, n00b_list_t *saved)
{
    n00b_table_t  *t;
    n00b_string_t *s;
    int            l;

    md_newline(ctx);

    switch (n00b_list_len(ctx->entities)) {
    case 0:
        ctx->entities = saved;
        return;
    case 1:
        s = n00b_list_get(ctx->entities, 0, NULL);

        if (block) {
            n00b_list_append(saved, n00b_call_out(s));
        }
        else {
            n00b_list_append(saved, s);
        }
        ctx->entities = saved;
        return;
    default:

        l = n00b_list_len(ctx->entities);
        t = n00b_table("columns", 1, "style", N00B_TABLE_FLOW);

        for (int i = 0; i < l; i++) {
            n00b_table_add_cell(t, n00b_list_get(ctx->entities, i, NULL));
        }

        n00b_table_end(t);
        n00b_list_append(saved, t);

        ctx->entities = saved;
        return;
    }
}
static void
md_header_merge(md_table_ctx *ctx, char *tag, n00b_list_t *saved)
{
    n00b_list_t   *styled = n00b_list(n00b_type_ref());
    n00b_string_t *t      = n00b_cstring(tag);

    md_newline(ctx);

    int l = n00b_list_len(ctx->entities);

    for (int i = 0; i < l; i++) {
        void *item = n00b_list_get(ctx->entities, i, NULL);
        if (n00b_type_is_string(n00b_get_my_type(item))) {
            n00b_list_append(styled, n00b_string_style_by_tag(item, t));
        }
        else {
            n00b_list_append(styled, item);
        }
    }

    ctx->entities = styled;
    md_block_merge(ctx, false, saved);
}

static void
md_node_to_table(md_table_ctx *ctx)
{
    n00b_md_node_t *n = n00b_tree_get_contents(ctx->cur_node);
    n00b_list_t    *saved_entities;
    n00b_string_t  *saved_str;
    n00b_table_t   *saved_table;
    int             l;

    switch (n->node_type) {
    case N00B_MD_SPAN_EM:
    case N00B_MD_SPAN_STRONG:
    case N00B_MD_SPAN_A:
    case N00B_MD_SPAN_A_SELF:
    case N00B_MD_SPAN_A_CODELINK:
    case N00B_MD_SPAN_IMG:
    case N00B_MD_SPAN_CODE:
    case N00B_MD_SPAN_STRIKETHRU:
    case N00B_MD_SPAN_LATEX:
    case N00B_MD_SPAN_LATEX_DISPLAY:
    case N00B_MD_SPAN_WIKI_LINK:
    case N00B_MD_SPAN_U:
        saved_str = ctx->str;
        ctx->str  = NULL;
        break;
    case N00B_MD_DOCUMENT:
        break;
    case N00B_MD_TEXT_SOFTBR:
        if (!ctx->keep_soft_newlines) {
            return;
        }
        if (!ctx->str) {
            ctx->str = n00b_cached_empty_string();
        }
        md_newline(ctx);
        return;
    case N00B_MD_TEXT_BR:
        if (!ctx->str) {
            ctx->str = n00b_cached_empty_string();
        }
        md_newline(ctx);
        return;
    case N00B_MD_TEXT_NULL:
        return;
    case N00B_MD_TEXT_NORMAL:
        md_add_string(ctx, n->detail.text);
        return;
    case N00B_MD_TEXT_ENTITY:
    case N00B_MD_TEXT_HTML:
    case N00B_MD_TEXT_LATEX:
        md_add_string(ctx, n00b_cformat("«i»«#»«/»", n->detail.text));
        return;
    case N00B_MD_TEXT_CODE:
        md_add_string(ctx, n00b_cformat("«code»«#»«/»", n->detail.text));
        return;
    case N00B_MD_BLOCK_TABLE:
        saved_table = ctx->table;
        ctx->table  = NULL;
        break;
    case N00B_MD_BLOCK_H:
        saved_entities = ctx->entities;
        ctx->entities  = n00b_list(n00b_type_ref());
        break;
    default:
        saved_entities = ctx->entities;
        ctx->entities  = n00b_list(n00b_type_ref());
        break;
    }

    process_kids(ctx);

    switch (n->node_type) {
    case N00B_MD_SPAN_EM:
        ctx->str = n00b_string_style_by_tag(ctx->str, n00b_cstring("em"));

        if (saved_str) {
            ctx->str  = n00b_cformat("[|#|][|#|]", saved_str, ctx->str);
            saved_str = NULL;
        }

        return;

    case N00B_MD_SPAN_STRONG:
        ctx->str = n00b_string_style_by_tag(ctx->str, n00b_cstring("b"));

        if (saved_str) {
            ctx->str  = n00b_cformat("[|#|][|#|]", saved_str, ctx->str);
            saved_str = NULL;
        }
        return;
    case N00B_MD_SPAN_U:
        ctx->str = n00b_string_style_by_tag(ctx->str, n00b_cstring("u"));
        if (saved_str) {
            ctx->str  = n00b_cformat("[|#|][|#|]", saved_str, ctx->str);
            saved_str = NULL;
        }
        return;
    case N00B_MD_SPAN_STRIKETHRU:
        ctx->str = n00b_string_style_by_tag(ctx->str,
                                            n00b_cstring("strikethrough"));
        if (saved_str) {
            ctx->str  = n00b_cformat("[|#|][|#|]", saved_str, ctx->str);
            saved_str = NULL;
        }
        return;
    case N00B_MD_SPAN_A_SELF:
    case N00B_MD_SPAN_A_CODELINK:
finish_span:
        if (saved_str) {
            ctx->str  = n00b_cformat("[|#|][|#|]", saved_str, ctx->str);
            saved_str = NULL;
        }
        return;
    case N00B_MD_SPAN_CODE:
    case N00B_MD_SPAN_LATEX:
    case N00B_MD_SPAN_LATEX_DISPLAY:
        n00b_string_style_by_tag(ctx->str, n00b_cstring("code"));
        goto finish_span;
    case N00B_MD_SPAN_A:
        if (n->detail.a.href.size) {
            ctx->str = n00b_cformat("«#» «i»(«#»)«/»",
                                    ctx->str,
                                    n00b_utf8((char *)n->detail.a.href.text,
                                              n->detail.a.href.size));
        }
        goto finish_span;
    case N00B_MD_SPAN_IMG:
        if (n->detail.img.title.size) {
            ctx->str = n00b_cformat(
                "[image «em2»\"«#»\"«/» @«em3»«#»«/»]",
                n00b_utf8((char *)n->detail.img.title.text,
                          n->detail.img.title.size),
                n00b_utf8((char *)n->detail.img.src.text,
                          n->detail.img.src.size));
        }
        else {
            ctx->str = n00b_cformat(
                "[image @«em3»«#»«/»]",
                n00b_utf8((char *)n->detail.img.src.text,
                          n->detail.img.src.size));
        }
        goto finish_span;
    case N00B_MD_SPAN_WIKI_LINK:
        ctx->str = n00b_cformat("«em3»«#»«/» «i»(«#»)«/»",
                                ctx->str,
                                n00b_utf8((char *)n->detail.img.src.text,
                                          n->detail.img.src.size));
        goto finish_span;
    case N00B_MD_BLOCK_HR:
        // Right now, don't have a hard rule primitive.
    case N00B_MD_BLOCK_BODY:
    case N00B_MD_BLOCK_HTML:
    case N00B_MD_BLOCK_P:
        md_newline(ctx);
        ctx->str = n00b_cached_empty_string();

        md_block_merge(ctx, false, saved_entities);
        break;
    case N00B_MD_BLOCK_TD:
        md_block_merge(ctx, false, saved_entities);
        break;
    case N00B_MD_BLOCK_CODE:
        md_block_merge(ctx, true, saved_entities);
        return;
    case N00B_MD_BLOCK_QUOTE:
        md_block_merge(ctx, true, saved_entities);
        return;
    case N00B_MD_BLOCK_UL:
        md_newline(ctx);
        n00b_list_append(saved_entities,
                         n00b_unordered_list(ctx->entities, NULL));
finish_block:
        ctx->entities = saved_entities;
        return;
    case N00B_MD_BLOCK_OL:
        md_newline(ctx);
        n00b_list_append(saved_entities,
                         n00b_ordered_list(ctx->entities, NULL));

        goto finish_block;
    case N00B_MD_BLOCK_LI:
        md_block_merge(ctx, false, saved_entities);
        return;
    case N00B_MD_BLOCK_H:
        if (ctx->str) {
            n00b_list_append(ctx->entities, ctx->str);
            ctx->str = NULL;
        }

        char *s;
        switch (n->detail.h.level) {
        case 1:
            s = "h1";
            break;
        case 2:
            s = "h2";
            break;
        case 3:
            s = "h3";
            break;
        case 4:
            s = "h4";
            break;
        case 5:
            s = "h5";
            break;
        case 6:
            s = "h6";
            break;
        default:
            s = "p";
            break;
        }

        md_header_merge(ctx, s, saved_entities);
        return;
    case N00B_MD_BLOCK_TR:
        md_newline(ctx);
        if (!ctx->table) {
            ctx->table = n00b_table("style", N00B_TABLE_SIMPLE);
        }

        l = n00b_list_len(ctx->entities);

        for (int i = 0; i < l; i++) {
            n00b_table_add_cell(ctx->table,
                                n00b_list_get(ctx->entities, i, NULL));
        }
        ctx->entities = saved_entities;
        return;

    case N00B_MD_BLOCK_TABLE:
        n00b_list_append(ctx->entities, ctx->table);
        ctx->table = saved_table;
        return;

    case N00B_MD_BLOCK_THEAD:
    case N00B_MD_BLOCK_TBODY:
        return;

    case N00B_MD_BLOCK_TH:
        if (!ctx->table) {
            ctx->table = n00b_table();
        }

        l = n00b_list_len(ctx->entities);

        for (int i = 0; i < l; i++) {
            n00b_table_add_cell(ctx->table,
                                n00b_list_get(ctx->entities, i, NULL));
        }
        ctx->entities = saved_entities;
        return;

    case N00B_MD_DOCUMENT:
    case N00B_MD_TEXT_NORMAL:
    case N00B_MD_TEXT_NULL:
    case N00B_MD_TEXT_BR:
    case N00B_MD_TEXT_SOFTBR:
    case N00B_MD_TEXT_ENTITY:
    case N00B_MD_TEXT_CODE:
    case N00B_MD_TEXT_HTML:
    case N00B_MD_TEXT_LATEX:
        return;
    default:
        md_newline(ctx);
        return;
    }
    return;
}

n00b_table_t *
n00b_markdown_to_table(n00b_string_t *s, bool keep_soft_newlines)
{
    md_table_ctx ctx = {
        .cur_node           = n00b_parse_markdown(s),
        .entities           = n00b_list(n00b_type_ref()),
        .keep_soft_newlines = keep_soft_newlines,
        .str                = NULL,
        .table              = NULL,
    };

    md_node_to_table(&ctx);

    return n00b_list_get(ctx.entities, 0, NULL);
}
