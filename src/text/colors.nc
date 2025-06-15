#include "n00b.h"
#include "text/color_gen.h"

static n00b_dict_t *color_table = NULL;

extern const n00b_color_info_t n00b_color_data[];

static inline n00b_dict_t *
get_color_table()
{
    if (color_table == NULL) {
        n00b_gc_register_root(&color_table, 1);
        color_table = n00b_dict(n00b_type_string(), n00b_type_int());

        n00b_color_info_t *p = (n00b_color_info_t *)n00b_color_data;

        while (p->name != NULL) {
            hatrack_dict_put(color_table,
                             n00b_cstring(p->name),
                             (void *)(int64_t)p->rgb);
            p++;
        }
    }
    return color_table;
}

n00b_color_t
n00b_lookup_color(n00b_string_t *name)
{
    bool         found  = false;
    n00b_color_t result = (n00b_color_t)(int64_t)hatrack_dict_get(
        get_color_table(),
        name,
        &found);

    if (found == false) {
        return -1;
    }

    return result;
}

n00b_color_t
n00b_to_vga(n00b_color_t truecolor)
{
    n00b_color_t r = (truecolor >> 16) & 0xff;
    n00b_color_t g = (truecolor >> 8) & 0xff;
    n00b_color_t b = (truecolor >> 0) & 0xff;

    // clang-format off
    return ((n00b_color_t)(r * 7 / 255) << 5) |
	((n00b_color_t)(g * 7 / 255) << 2) |
	((n00b_color_t)(b * 3 / 255));
    // clang-format on
}
