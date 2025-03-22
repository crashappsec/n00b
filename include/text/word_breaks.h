#pragma once
#include "n00b.h"

typedef enum {
    N00B_WB_Other,
    N00B_WB_Double_Quote,
    N00B_WB_Single_Quote,
    N00B_WB_Hebrew_Letter,
    N00B_WB_CR,
    N00B_WB_LF,
    N00B_WB_Newline,
    N00B_WB_Extend,
    N00B_WB_Format,
    N00B_WB_Katakana,
    N00B_WB_ALetter,
    N00B_WB_MidLetter,
    N00B_WB_MidNum,
    N00B_WB_MidNumLet,
    N00B_WB_Numeric,
    N00B_WB_ExtendNumLet,
    N00B_WB_ZWJ,
    N00B_WB_WSegSpace,
    N00B_WB_Regional_Indicator,
    N00B_WB_Regional_Indicator_Next_Breaks,
} n00b_wb_kind;

extern n00b_wb_kind n00b_codepoint_word_break_prop(n00b_codepoint_t);
