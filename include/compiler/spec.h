#pragma once
#include "n00b.h"

extern n00b_spec_t         *n00b_new_spec();
extern n00b_attr_info_t    *n00b_get_attr_info(n00b_spec_t *, n00b_list_t *);
extern n00b_spec_field_t   *n00b_new_spec_field();
extern n00b_spec_section_t *n00b_new_spec_section();
extern n00b_grid_t         *n00b_repr_spec(n00b_spec_t *);
extern n00b_grid_t         *n00b_grid_repr_section(n00b_spec_section_t *);
