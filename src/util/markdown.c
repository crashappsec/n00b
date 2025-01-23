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
    node->detail.text = n00b_new(n00b_type_utf8(),
                                 n00b_kw("length",
                                         n00b_ka(size),
                                         "cstring",
                                         n00b_ka(text)));

    n00b_tree_add_node(ctx->cur, node);

    return 0;
}

static inline n00b_utf8_t *
md_node_type_to_name(n00b_md_node_kind_t kind)
{
    char *s = NULL;

    switch (kind) {
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
        s = "h";
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

    return n00b_new_utf8(s);
}

static n00b_utf8_t *
n00b_repr_md_node(n00b_md_node_t *node)
{
    n00b_utf8_t *ret = n00b_cstr_format("[atomic lime]{}[/] ({})",
                                        md_node_type_to_name(node->node_type),
                                        node->node_type);
    n00b_utf8_t *extra;

    switch (node->node_type) {
    case N00B_MD_TEXT_NORMAL:
    case N00B_MD_TEXT_NULL:
    case N00B_MD_TEXT_BR:
    case N00B_MD_TEXT_SOFTBR:
    case N00B_MD_TEXT_ENTITY:
    case N00B_MD_TEXT_CODE:
    case N00B_MD_TEXT_HTML:
    case N00B_MD_TEXT_LATEX:
        extra = n00b_cstr_format(" - [em]{}[/]", node->detail.text);
        break;
    default:
        return ret;
    }

    return n00b_str_concat(ret, extra);
}

n00b_tree_node_t *
n00b_parse_markdown(n00b_str_t *s)
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

    n00b_md_node_t   *r = n00b_gc_alloc_mapped(n00b_md_node_t, N00B_GC_SCAN_ALL);
    n00b_tree_node_t *t = n00b_new_tree_node(n00b_type_ref(), r);
    r->node_type        = N00B_MD_DOCUMENT;

    n00b_md_build_ctx build_ctx = {
        .cur = t,
    };

    s = n00b_to_utf8(s);
    md_parse(s->data, n00b_str_codepoint_len(s), &parser, (void *)&build_ctx);

    return t;
}

n00b_grid_t *
n00b_repr_md_parse(n00b_tree_node_t *t)
{
    return n00b_grid_tree_new(t, n00b_kw("callback", n00b_repr_md_node));
}

typedef struct {
    n00b_tree_node_t *cur_node;
    n00b_list_t      *entities;
    n00b_utf8_t      *str;
    n00b_grid_t      *table;
    int               row_count;
    bool              keep_soft_newlines;
} md_grid_ctx;

static inline void
md_newline(md_grid_ctx *ctx)
{
    if (ctx->str) {
        n00b_utf8_t       *style = n00b_new_utf8("text");
        n00b_renderable_t *r     = n00b_to_str_renderable(ctx->str, style);

        n00b_enforce_container_style(r, style, false);
        n00b_list_append(ctx->entities, r);
        ctx->str = NULL;
    }
}

static inline void
md_add_string(md_grid_ctx *ctx, n00b_utf8_t *s)
{
    if (!ctx->str) {
        ctx->str = s;
    }
    else {
        ctx->str = n00b_str_concat(ctx->str, s);
    }
}

static void md_node_to_grid(md_grid_ctx *);

static inline void
process_kids(md_grid_ctx *ctx)
{
    n00b_tree_node_t *saved_node = ctx->cur_node;

    for (int i = 0; i < saved_node->num_kids; i++) {
        ctx->cur_node = saved_node->children[i];
        md_node_to_grid(ctx);
    }

    ctx->cur_node = saved_node;
}

static void
md_block_merge_and_style(md_grid_ctx *ctx, char *s, n00b_list_t *saved)
{
    n00b_utf8_t       *style = s ? n00b_new_utf8(s) : NULL;
    n00b_renderable_t *r;
    n00b_grid_t       *g;
    int                l;

    md_newline(ctx);

    switch (n00b_list_len(ctx->entities)) {
    case 0:
        ctx->entities = saved;
        return;
    case 1:
        r = n00b_list_get(ctx->entities, 0, NULL);

        if (n00b_type_is_string(n00b_get_my_type(r->raw_item))) {
            n00b_enforce_container_style(r, style, true);
        }

        n00b_list_append(saved, r);
        ctx->entities = saved;
        return;
    default:
        l = n00b_list_len(ctx->entities);
        g = n00b_new(n00b_type_grid(),
                     n00b_kw("start_rows",
                             n00b_ka(l),
                             "start_cols",
                             n00b_ka(1),
                             "container_tag",
                             n00b_new_utf8("flow")));

        for (int i = 0; i < l; i++) {
            n00b_renderable_t *item = n00b_list_get(ctx->entities, i, NULL);

            if (n00b_type_is_string(n00b_get_my_type(item->raw_item))) {
                n00b_enforce_container_style(item, style, true);
            }

            n00b_grid_set_cell_contents(g, i, 0, item);
        }

        n00b_enforce_container_style(g, style, true);

        ctx->entities = saved;
        n00b_list_append(ctx->entities, g);
        return;
    }
}

static void
md_node_to_grid(md_grid_ctx *ctx)
{
    n00b_md_node_t *n = n00b_tree_get_contents(ctx->cur_node);
    n00b_list_t    *saved_entities;
    n00b_utf8_t    *saved_str;
    n00b_grid_t    *saved_table;
    int             saved_row_count;

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
        saved_str = NULL;
        break;
    case N00B_MD_DOCUMENT:
        break;
    case N00B_MD_TEXT_SOFTBR:
        if (!ctx->keep_soft_newlines) {
            return;
        }
        // fallthrough
    case N00B_MD_TEXT_BR:
        if (!ctx->str) {
            ctx->str = n00b_new_utf8("");
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
        md_add_string(ctx, n00b_cstr_format("[i]{}[/]", n->detail.text));
        return;
    case N00B_MD_TEXT_CODE:
        md_add_string(ctx, n00b_cstr_format("[code]{}[/]", n->detail.text));
        return;
    case N00B_MD_BLOCK_TABLE:
        saved_table     = ctx->table;
        saved_row_count = ctx->row_count;
        ctx->table      = NULL;
        ctx->row_count  = 0;
        break;
    default:
        saved_entities = ctx->entities;
        ctx->entities  = n00b_list(n00b_type_ref());
        break;
    }

    process_kids(ctx);

    switch (n->node_type) {
    case N00B_MD_SPAN_EM:
        n00b_str_apply_style(ctx->str, N00B_STY_ITALIC, false);
finish_span:
        if (saved_str && ctx->str) {
            ctx->str = n00b_str_concat(saved_str, ctx->str);
        }
        else {
            if (!ctx->str) {
                ctx->str = saved_str;
            }
        }

        return;
    case N00B_MD_SPAN_STRONG:
        n00b_str_apply_style(ctx->str, N00B_STY_BOLD, false);
        goto finish_span;
    case N00B_MD_SPAN_U:
        n00b_str_apply_style(ctx->str, N00B_STY_UL, false);
        goto finish_span;
    case N00B_MD_SPAN_STRIKETHRU:
        n00b_str_apply_style(ctx->str, N00B_STY_ST, false);
        goto finish_span;
    case N00B_MD_SPAN_A_SELF:
    case N00B_MD_SPAN_A_CODELINK:
        goto finish_span;
    case N00B_MD_SPAN_CODE:
    case N00B_MD_SPAN_LATEX:
    case N00B_MD_SPAN_LATEX_DISPLAY:
        n00b_str_apply_style(ctx->str,
                             n00b_lookup_text_style(n00b_new_utf8("code")),
                             false);
        goto finish_span;
    case N00B_MD_SPAN_A:
        if (n->detail.a.href.size) {
            n00b_str_apply_style(ctx->str,
                                 n00b_lookup_text_style(n00b_new_utf8("link")),
                                 false);
            ctx->str = n00b_cstr_format(
                "{} [i]({})[/]",
                ctx->str,
                n00b_cstring((char *)n->detail.a.href.text,
                             n->detail.a.href.size));
        }
        goto finish_span;
    case N00B_MD_SPAN_IMG:
        if (n->detail.img.title.size) {
            ctx->str = n00b_cstr_format(
                "{}image [em]\"{}\"[/] @[link]{}[/]] ",
                n00b_new_utf8("["),
                n00b_cstring((char *)n->detail.img.title.text,
                             n->detail.img.title.size),
                n00b_cstring((char *)n->detail.img.src.text,
                             n->detail.img.src.size));
        }
        else {
            ctx->str = n00b_cstr_format(
                "{}image @[link]{}[/]]",
                n00b_new_utf8("["),
                n00b_cstring((char *)n->detail.img.src.text,
                             n->detail.img.src.size));
        }
        goto finish_span;
    case N00B_MD_SPAN_WIKI_LINK:
        ctx->str = n00b_cstr_format("[link]{}[/] [i]({})[/]",
                                    ctx->str,
                                    n00b_cstring((char *)n->detail.img.src.text,
                                                 n->detail.img.src.size));
        goto finish_span;
    case N00B_MD_BLOCK_HR:
        // Right now, don't have a hard rule primitive.
    case N00B_MD_BLOCK_BODY:
    case N00B_MD_BLOCK_HTML:
    case N00B_MD_BLOCK_P:
        md_block_merge_and_style(ctx, "text", saved_entities);
        break;
    case N00B_MD_BLOCK_TD:
        md_block_merge_and_style(ctx, "td", saved_entities);
        break;
    case N00B_MD_BLOCK_CODE:
        md_block_merge_and_style(ctx, "code", saved_entities);
        return;
    case N00B_MD_BLOCK_QUOTE:
        md_block_merge_and_style(ctx, "callout_cell", saved_entities);
        return;
    case N00B_MD_BLOCK_UL:
        md_newline(ctx);
        n00b_list_append(saved_entities,
                         n00b_new(n00b_type_renderable(),
                                  n00b_kw("obj",
                                          n00b_unordered_list(ctx->entities))));
        ;
finish_block:
        ctx->entities = saved_entities;
        return;
    case N00B_MD_BLOCK_OL:
        md_newline(ctx);
        n00b_list_append(saved_entities,
                         n00b_new(n00b_type_renderable(),
                                  n00b_kw("obj",
                                          n00b_ordered_list(ctx->entities))));
        goto finish_block;
    case N00B_MD_BLOCK_LI:
        md_block_merge_and_style(ctx, "li", saved_entities);
        return;
    case N00B_MD_BLOCK_H:
        md_newline(ctx);
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
            s = "flow";
            break;
        }

        md_block_merge_and_style(ctx, s, saved_entities);
        return;
    case N00B_MD_BLOCK_TR:
        md_newline(ctx);

        if (!ctx->table) {
            ctx->table = n00b_new(n00b_type_grid(),
                                  n00b_kw("start_rows",
                                          n00b_ka(1),
                                          "start_cols",
                                          n00b_list_len(ctx->entities),
                                          "stripe",
                                          n00b_ka(true)));
        }

        n00b_grid_add_row(ctx->table, ctx->entities);
        n00b_set_row_style(ctx->table, ctx->row_count++, n00b_new_utf8("tr"));
        ctx->entities = saved_entities;
        return;

    case N00B_MD_BLOCK_TABLE:
        n00b_list_append(ctx->entities, ctx->table);
        ctx->row_count = saved_row_count;
        ctx->table     = saved_table;
        return;

    case N00B_MD_BLOCK_THEAD:
    case N00B_MD_BLOCK_TBODY:
        return;

    case N00B_MD_BLOCK_TH:
        if (!ctx->table) {
            ctx->table = n00b_new(n00b_type_grid(),
                                  n00b_kw("start_rows",
                                          n00b_ka(1),
                                          "start_cols",
                                          n00b_list_len(ctx->entities),
                                          "stripe",
                                          n00b_ka(true)));
        }

        n00b_grid_add_row(ctx->table, ctx->entities);
        n00b_set_row_style(ctx->table, ctx->row_count++, n00b_new_utf8("th"));
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
}

n00b_grid_t *
n00b_markdown_to_grid(n00b_str_t *s, bool keep_soft_newlines)
{
    md_grid_ctx ctx = {
        .cur_node           = n00b_parse_markdown(s),
        .entities           = n00b_list(n00b_type_ref()),
        .keep_soft_newlines = keep_soft_newlines,
        .str                = NULL,
        .table              = NULL,
    };

    md_node_to_grid(&ctx);

    return n00b_list_get(ctx.entities, 0, NULL);
}
