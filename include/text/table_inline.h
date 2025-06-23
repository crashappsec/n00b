#pragma once
#include "n00b.h"

static inline n00b_table_t *
n00b_call_out(n00b_string_t *s)
{
    n00b_table_t *t = n00b_new(n00b_type_table(),
                               n00b_header_kargs("style",
                                                 (int64_t)N00B_TABLE_CALLOUT));
    n00b_table_add_cell(t, s);
    n00b_table_end_row(t);
    n00b_table_end(t);

    return t;
}

static inline n00b_table_t *
n00b_flow(n00b_list_t *list)
{
    n00b_table_t *t     = n00b_new(n00b_type_table(),
                               n00b_header_kargs("style",
                                                 (int64_t)N00B_TABLE_FLOW));
    int           nargs = n00b_list_len(list);

    for (int i = 0; i < nargs; i++) {
        n00b_table_add_cell(t, n00b_list_get(list, i, NULL));
        n00b_table_end_row(t);
    }

    n00b_table_end(t);

    return t;
}

static void
n00b_ensure_theme(n00b_table_t *table)
{
    if (!table->theme) {
        table->theme = n00b_get_current_theme();
    }
}

static inline n00b_box_props_t *
n00b_outer_box(n00b_table_t *table)
{
    n00b_ensure_theme(table);

    n00b_box_props_t *result;

    switch (table->decoration_style) {
    case N00B_TABLE_ORNATE:
        result = table->theme->box_info[N00B_BOX_THEME_ORNATE_BASE];
        break;
    case N00B_TABLE_SIMPLE:
        result = table->theme->box_info[N00B_BOX_THEME_SIMPLE_BASE];
        break;
    case N00B_TABLE_VHEADER:
        result = table->theme->box_info[N00B_BOX_THEME_VHEADER_BASE];
        break;
    case N00B_TABLE_FLOW:
        result = table->theme->box_info[N00B_BOX_THEME_FLOW];
        break;
    case N00B_TABLE_CALLOUT:
        result = table->theme->box_info[N00B_BOX_THEME_CALLOUT];
        break;
    case N00B_TABLE_OL:
        result = table->theme->box_info[N00B_BOX_THEME_OL_BASE];
        if (!result) {
            result = table->theme->box_info[N00B_BOX_THEME_SIMPLE_BASE];
        }
        break;
    case N00B_TABLE_UL:
        result = table->theme->box_info[N00B_BOX_THEME_UL_BASE];
        if (!result) {
            result = table->theme->box_info[N00B_BOX_THEME_SIMPLE_BASE];
        }
        break;
    default:
        result = table->theme->box_info[N00B_BOX_THEME_TABLE_BASE];
        break;
    }

    if (!result) {
        result = table->theme->box_info[N00B_BOX_THEME_TABLE_BASE];
    }

    assert(result);

    return result;
}

static inline n00b_box_props_t *
n00b_cell_box(n00b_table_t *table)
{
    n00b_ensure_theme(table);

    n00b_box_props_t *result;

    switch (table->decoration_style) {
    case N00B_TABLE_ORNATE:
        result = table->theme->box_info[N00B_BOX_THEME_ORNATE_CELL_BASE];
        break;
    case N00B_TABLE_SIMPLE:
        result = table->theme->box_info[N00B_BOX_THEME_SIMPLE_CELL_BASE];
        break;
    case N00B_TABLE_VHEADER:
        result = table->theme->box_info[N00B_BOX_THEME_VHEADER_CELL_BASE];
        break;
    case N00B_TABLE_FLOW:
        result = table->theme->box_info[N00B_BOX_THEME_FLOW];
        break;
    case N00B_TABLE_CALLOUT:
        result = table->theme->box_info[N00B_BOX_THEME_CALLOUT];
        break;
    case N00B_TABLE_UL:
        result = table->theme->box_info[N00B_BOX_UL_CELL_BASE];
        if (!result) {
            result = table->theme->box_info[N00B_BOX_THEME_SIMPLE_CELL_BASE];
        }
        break;
    case N00B_TABLE_OL:
        result = table->theme->box_info[N00B_BOX_OL_CELL_BASE];
        if (!result) {
            result = table->theme->box_info[N00B_BOX_THEME_SIMPLE_CELL_BASE];
        }
        break;
    default:
        result = table->theme->box_info[N00B_BOX_THEME_TABLE_CELL_BASE];
        break;
    }

    if (!result) {
        return n00b_outer_box(table);
    }

    return result;
}

static inline n00b_box_props_t *
n00b_alt_cell_box(n00b_table_t *table)
{
    n00b_ensure_theme(table);

    n00b_box_props_t *result;

    switch (table->decoration_style) {
    case N00B_TABLE_ORNATE:
        result = table->theme->box_info[N00B_BOX_THEME_ORNATE_CELL_ALT];
        break;
    case N00B_TABLE_SIMPLE:
        result = table->theme->box_info[N00B_BOX_THEME_SIMPLE_CELL_ALT];
        break;
    case N00B_TABLE_VHEADER:
        result = table->theme->box_info[N00B_BOX_THEME_VHEADER_CELL_ALT];
        break;
    case N00B_TABLE_FLOW:
        result = table->theme->box_info[N00B_BOX_THEME_FLOW];
        break;
    case N00B_TABLE_CALLOUT:
        result = table->theme->box_info[N00B_BOX_THEME_CALLOUT];
        break;
    default:
        result = table->theme->box_info[N00B_BOX_THEME_TABLE_CELL_ALT];
        break;
    }

    if (!result) {
        return n00b_cell_box(table);
    }

    return result;
}

static inline n00b_box_props_t *
n00b_header_box(n00b_table_t *table)
{
    n00b_ensure_theme(table);

    n00b_box_props_t *result;

    switch (table->decoration_style) {
    case N00B_TABLE_ORNATE:
        result = table->theme->box_info[N00B_BOX_THEME_ORNATE_HEADER];
        break;
    case N00B_TABLE_SIMPLE:
        result = table->theme->box_info[N00B_BOX_THEME_SIMPLE_HEADER];
        break;
    case N00B_TABLE_FLOW:
        result = table->theme->box_info[N00B_BOX_THEME_FLOW];
        break;
    case N00B_TABLE_CALLOUT:
        result = table->theme->box_info[N00B_BOX_THEME_CALLOUT];
        break;
    case N00B_TABLE_UL:
        result = table->theme->box_info[N00B_BOX_UL_BULLET];
        if (!result) {
            result = table->theme->box_info[N00B_BOX_THEME_TABLE_HEADER];
            break;
        }
        break;
    case N00B_TABLE_OL:
        result = table->theme->box_info[N00B_BOX_OL_BULLET];
        if (!result) {
            result = table->theme->box_info[N00B_BOX_THEME_TABLE_HEADER];
            break;
        }
        break;
    case N00B_TABLE_VHEADER:
        result = table->theme->box_info[N00B_BOX_THEME_VHEADER_HEADER];
        break;
    default:
        result = table->theme->box_info[N00B_BOX_THEME_TABLE_HEADER];
        break;
    }

    if (!result) {
        return n00b_cell_box(table);
    }

    return result;
}

static inline n00b_box_props_t *
n00b_title_box(n00b_table_t *table)
{
    n00b_ensure_theme(table);

    n00b_box_props_t *result;

    result = table->theme->box_info[N00B_BOX_THEME_TITLE];

    if (result) {
        return result;
    }

    switch (table->decoration_style) {
    case N00B_TABLE_ORNATE:
        result = table->theme->box_info[N00B_BOX_THEME_ORNATE_HEADER];
        break;
    case N00B_TABLE_SIMPLE:
        result = table->theme->box_info[N00B_BOX_THEME_SIMPLE_HEADER];
        break;
    case N00B_TABLE_FLOW:
        result = table->theme->box_info[N00B_BOX_THEME_FLOW];
        break;
    case N00B_TABLE_CALLOUT:
        result = table->theme->box_info[N00B_BOX_THEME_CALLOUT];
        break;
    default:
        result = table->theme->box_info[N00B_BOX_THEME_TABLE_HEADER];
        break;
    }

    if (!result) {
        return n00b_cell_box(table);
    }

    return result;
}

static inline n00b_box_props_t *
n00b_caption_box(n00b_table_t *table)
{
    n00b_ensure_theme(table);

    n00b_box_props_t *result;

    result = table->theme->box_info[N00B_BOX_THEME_CAPTION];

    if (result) {
        return result;
    }

    switch (table->decoration_style) {
    case N00B_TABLE_ORNATE:
        result = table->theme->box_info[N00B_BOX_THEME_ORNATE_HEADER];
        break;
    case N00B_TABLE_SIMPLE:
        result = table->theme->box_info[N00B_BOX_THEME_SIMPLE_HEADER];
        break;
    case N00B_TABLE_FLOW:
        result = table->theme->box_info[N00B_BOX_THEME_FLOW];
        break;
    case N00B_TABLE_CALLOUT:
        result = table->theme->box_info[N00B_BOX_THEME_CALLOUT];
        break;
    default:
        result = table->theme->box_info[N00B_BOX_THEME_TABLE_HEADER];
        break;
    }

    if (!result) {
        return n00b_cell_box(table);
    }

    return result;
}
