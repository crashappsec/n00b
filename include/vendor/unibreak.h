#pragma once

#include "n00b/base.h"

extern void set_linebreaks_utf32(const int32_t *s,
                                 size_t         len,
                                 const char    *lang,
                                 char          *brks);

extern size_t set_linebreaks_utf8_per_code_point(const int8_t *s,
                                                 size_t        len,
                                                 const char   *lang,
                                                 char         *brks);
