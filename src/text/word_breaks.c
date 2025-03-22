
#include "n00b.h"

// This function was auto-generated, and currently very stupid.

n00b_wb_kind
n00b_codepoint_word_break_prop(n00b_codepoint_t cp)
{
    if (cp == 0x0022) {
        return N00B_WB_Double_Quote;
    }

    if (cp == 0x0027) {
        return N00B_WB_Single_Quote;
    }

    if (cp >= 0x05D0 && cp <= 0x05EA) {
        return N00B_WB_Hebrew_Letter;
    }

    if (cp >= 0x05EF && cp <= 0x05F2) {
        return N00B_WB_Hebrew_Letter;
    }

    if (cp == 0xFB1D) {
        return N00B_WB_Hebrew_Letter;
    }

    if (cp >= 0xFB1F && cp <= 0xFB28) {
        return N00B_WB_Hebrew_Letter;
    }

    if (cp >= 0xFB2A && cp <= 0xFB36) {
        return N00B_WB_Hebrew_Letter;
    }

    if (cp >= 0xFB38 && cp <= 0xFB3C) {
        return N00B_WB_Hebrew_Letter;
    }

    if (cp == 0xFB3E) {
        return N00B_WB_Hebrew_Letter;
    }

    if (cp >= 0xFB40 && cp <= 0xFB41) {
        return N00B_WB_Hebrew_Letter;
    }

    if (cp >= 0xFB43 && cp <= 0xFB44) {
        return N00B_WB_Hebrew_Letter;
    }

    if (cp >= 0xFB46 && cp <= 0xFB4F) {
        return N00B_WB_Hebrew_Letter;
    }

    if (cp == 0x000D) {
        return N00B_WB_CR;
    }

    if (cp == 0x000A) {
        return N00B_WB_LF;
    }

    if (cp >= 0x000B && cp <= 0x000C) {
        return N00B_WB_Newline;
    }

    if (cp == 0x0085) {
        return N00B_WB_Newline;
    }

    if (cp == 0x2028) {
        return N00B_WB_Newline;
    }

    if (cp == 0x2029) {
        return N00B_WB_Newline;
    }

    if (cp >= 0x0300 && cp <= 0x036F) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0483 && cp <= 0x0487) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0488 && cp <= 0x0489) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0591 && cp <= 0x05BD) {
        return N00B_WB_Extend;
    }

    if (cp == 0x05BF) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x05C1 && cp <= 0x05C2) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x05C4 && cp <= 0x05C5) {
        return N00B_WB_Extend;
    }

    if (cp == 0x05C7) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0610 && cp <= 0x061A) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x064B && cp <= 0x065F) {
        return N00B_WB_Extend;
    }

    if (cp == 0x0670) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x06D6 && cp <= 0x06DC) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x06DF && cp <= 0x06E4) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x06E7 && cp <= 0x06E8) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x06EA && cp <= 0x06ED) {
        return N00B_WB_Extend;
    }

    if (cp == 0x0711) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0730 && cp <= 0x074A) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x07A6 && cp <= 0x07B0) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x07EB && cp <= 0x07F3) {
        return N00B_WB_Extend;
    }

    if (cp == 0x07FD) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0816 && cp <= 0x0819) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x081B && cp <= 0x0823) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0825 && cp <= 0x0827) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0829 && cp <= 0x082D) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0859 && cp <= 0x085B) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0897 && cp <= 0x089F) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x08CA && cp <= 0x08E1) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x08E3 && cp <= 0x0902) {
        return N00B_WB_Extend;
    }

    if (cp == 0x0903) {
        return N00B_WB_Extend;
    }

    if (cp == 0x093A) {
        return N00B_WB_Extend;
    }

    if (cp == 0x093B) {
        return N00B_WB_Extend;
    }

    if (cp == 0x093C) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x093E && cp <= 0x0940) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0941 && cp <= 0x0948) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0949 && cp <= 0x094C) {
        return N00B_WB_Extend;
    }

    if (cp == 0x094D) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x094E && cp <= 0x094F) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0951 && cp <= 0x0957) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0962 && cp <= 0x0963) {
        return N00B_WB_Extend;
    }

    if (cp == 0x0981) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0982 && cp <= 0x0983) {
        return N00B_WB_Extend;
    }

    if (cp == 0x09BC) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x09BE && cp <= 0x09C0) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x09C1 && cp <= 0x09C4) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x09C7 && cp <= 0x09C8) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x09CB && cp <= 0x09CC) {
        return N00B_WB_Extend;
    }

    if (cp == 0x09CD) {
        return N00B_WB_Extend;
    }

    if (cp == 0x09D7) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x09E2 && cp <= 0x09E3) {
        return N00B_WB_Extend;
    }

    if (cp == 0x09FE) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0A01 && cp <= 0x0A02) {
        return N00B_WB_Extend;
    }

    if (cp == 0x0A03) {
        return N00B_WB_Extend;
    }

    if (cp == 0x0A3C) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0A3E && cp <= 0x0A40) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0A41 && cp <= 0x0A42) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0A47 && cp <= 0x0A48) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0A4B && cp <= 0x0A4D) {
        return N00B_WB_Extend;
    }

    if (cp == 0x0A51) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0A70 && cp <= 0x0A71) {
        return N00B_WB_Extend;
    }

    if (cp == 0x0A75) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0A81 && cp <= 0x0A82) {
        return N00B_WB_Extend;
    }

    if (cp == 0x0A83) {
        return N00B_WB_Extend;
    }

    if (cp == 0x0ABC) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0ABE && cp <= 0x0AC0) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0AC1 && cp <= 0x0AC5) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0AC7 && cp <= 0x0AC8) {
        return N00B_WB_Extend;
    }

    if (cp == 0x0AC9) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0ACB && cp <= 0x0ACC) {
        return N00B_WB_Extend;
    }

    if (cp == 0x0ACD) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0AE2 && cp <= 0x0AE3) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0AFA && cp <= 0x0AFF) {
        return N00B_WB_Extend;
    }

    if (cp == 0x0B01) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0B02 && cp <= 0x0B03) {
        return N00B_WB_Extend;
    }

    if (cp == 0x0B3C) {
        return N00B_WB_Extend;
    }

    if (cp == 0x0B3E) {
        return N00B_WB_Extend;
    }

    if (cp == 0x0B3F) {
        return N00B_WB_Extend;
    }

    if (cp == 0x0B40) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0B41 && cp <= 0x0B44) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0B47 && cp <= 0x0B48) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0B4B && cp <= 0x0B4C) {
        return N00B_WB_Extend;
    }

    if (cp == 0x0B4D) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0B55 && cp <= 0x0B56) {
        return N00B_WB_Extend;
    }

    if (cp == 0x0B57) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0B62 && cp <= 0x0B63) {
        return N00B_WB_Extend;
    }

    if (cp == 0x0B82) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0BBE && cp <= 0x0BBF) {
        return N00B_WB_Extend;
    }

    if (cp == 0x0BC0) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0BC1 && cp <= 0x0BC2) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0BC6 && cp <= 0x0BC8) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0BCA && cp <= 0x0BCC) {
        return N00B_WB_Extend;
    }

    if (cp == 0x0BCD) {
        return N00B_WB_Extend;
    }

    if (cp == 0x0BD7) {
        return N00B_WB_Extend;
    }

    if (cp == 0x0C00) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0C01 && cp <= 0x0C03) {
        return N00B_WB_Extend;
    }

    if (cp == 0x0C04) {
        return N00B_WB_Extend;
    }

    if (cp == 0x0C3C) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0C3E && cp <= 0x0C40) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0C41 && cp <= 0x0C44) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0C46 && cp <= 0x0C48) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0C4A && cp <= 0x0C4D) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0C55 && cp <= 0x0C56) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0C62 && cp <= 0x0C63) {
        return N00B_WB_Extend;
    }

    if (cp == 0x0C81) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0C82 && cp <= 0x0C83) {
        return N00B_WB_Extend;
    }

    if (cp == 0x0CBC) {
        return N00B_WB_Extend;
    }

    if (cp == 0x0CBE) {
        return N00B_WB_Extend;
    }

    if (cp == 0x0CBF) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0CC0 && cp <= 0x0CC4) {
        return N00B_WB_Extend;
    }

    if (cp == 0x0CC6) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0CC7 && cp <= 0x0CC8) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0CCA && cp <= 0x0CCB) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0CCC && cp <= 0x0CCD) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0CD5 && cp <= 0x0CD6) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0CE2 && cp <= 0x0CE3) {
        return N00B_WB_Extend;
    }

    if (cp == 0x0CF3) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0D00 && cp <= 0x0D01) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0D02 && cp <= 0x0D03) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0D3B && cp <= 0x0D3C) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0D3E && cp <= 0x0D40) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0D41 && cp <= 0x0D44) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0D46 && cp <= 0x0D48) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0D4A && cp <= 0x0D4C) {
        return N00B_WB_Extend;
    }

    if (cp == 0x0D4D) {
        return N00B_WB_Extend;
    }

    if (cp == 0x0D57) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0D62 && cp <= 0x0D63) {
        return N00B_WB_Extend;
    }

    if (cp == 0x0D81) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0D82 && cp <= 0x0D83) {
        return N00B_WB_Extend;
    }

    if (cp == 0x0DCA) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0DCF && cp <= 0x0DD1) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0DD2 && cp <= 0x0DD4) {
        return N00B_WB_Extend;
    }

    if (cp == 0x0DD6) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0DD8 && cp <= 0x0DDF) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0DF2 && cp <= 0x0DF3) {
        return N00B_WB_Extend;
    }

    if (cp == 0x0E31) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0E34 && cp <= 0x0E3A) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0E47 && cp <= 0x0E4E) {
        return N00B_WB_Extend;
    }

    if (cp == 0x0EB1) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0EB4 && cp <= 0x0EBC) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0EC8 && cp <= 0x0ECE) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0F18 && cp <= 0x0F19) {
        return N00B_WB_Extend;
    }

    if (cp == 0x0F35) {
        return N00B_WB_Extend;
    }

    if (cp == 0x0F37) {
        return N00B_WB_Extend;
    }

    if (cp == 0x0F39) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0F3E && cp <= 0x0F3F) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0F71 && cp <= 0x0F7E) {
        return N00B_WB_Extend;
    }

    if (cp == 0x0F7F) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0F80 && cp <= 0x0F84) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0F86 && cp <= 0x0F87) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0F8D && cp <= 0x0F97) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x0F99 && cp <= 0x0FBC) {
        return N00B_WB_Extend;
    }

    if (cp == 0x0FC6) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x102B && cp <= 0x102C) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x102D && cp <= 0x1030) {
        return N00B_WB_Extend;
    }

    if (cp == 0x1031) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1032 && cp <= 0x1037) {
        return N00B_WB_Extend;
    }

    if (cp == 0x1038) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1039 && cp <= 0x103A) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x103B && cp <= 0x103C) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x103D && cp <= 0x103E) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1056 && cp <= 0x1057) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1058 && cp <= 0x1059) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x105E && cp <= 0x1060) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1062 && cp <= 0x1064) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1067 && cp <= 0x106D) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1071 && cp <= 0x1074) {
        return N00B_WB_Extend;
    }

    if (cp == 0x1082) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1083 && cp <= 0x1084) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1085 && cp <= 0x1086) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1087 && cp <= 0x108C) {
        return N00B_WB_Extend;
    }

    if (cp == 0x108D) {
        return N00B_WB_Extend;
    }

    if (cp == 0x108F) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x109A && cp <= 0x109C) {
        return N00B_WB_Extend;
    }

    if (cp == 0x109D) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x135D && cp <= 0x135F) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1712 && cp <= 0x1714) {
        return N00B_WB_Extend;
    }

    if (cp == 0x1715) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1732 && cp <= 0x1733) {
        return N00B_WB_Extend;
    }

    if (cp == 0x1734) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1752 && cp <= 0x1753) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1772 && cp <= 0x1773) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x17B4 && cp <= 0x17B5) {
        return N00B_WB_Extend;
    }

    if (cp == 0x17B6) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x17B7 && cp <= 0x17BD) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x17BE && cp <= 0x17C5) {
        return N00B_WB_Extend;
    }

    if (cp == 0x17C6) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x17C7 && cp <= 0x17C8) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x17C9 && cp <= 0x17D3) {
        return N00B_WB_Extend;
    }

    if (cp == 0x17DD) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x180B && cp <= 0x180D) {
        return N00B_WB_Extend;
    }

    if (cp == 0x180F) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1885 && cp <= 0x1886) {
        return N00B_WB_Extend;
    }

    if (cp == 0x18A9) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1920 && cp <= 0x1922) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1923 && cp <= 0x1926) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1927 && cp <= 0x1928) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1929 && cp <= 0x192B) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1930 && cp <= 0x1931) {
        return N00B_WB_Extend;
    }

    if (cp == 0x1932) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1933 && cp <= 0x1938) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1939 && cp <= 0x193B) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1A17 && cp <= 0x1A18) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1A19 && cp <= 0x1A1A) {
        return N00B_WB_Extend;
    }

    if (cp == 0x1A1B) {
        return N00B_WB_Extend;
    }

    if (cp == 0x1A55) {
        return N00B_WB_Extend;
    }

    if (cp == 0x1A56) {
        return N00B_WB_Extend;
    }

    if (cp == 0x1A57) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1A58 && cp <= 0x1A5E) {
        return N00B_WB_Extend;
    }

    if (cp == 0x1A60) {
        return N00B_WB_Extend;
    }

    if (cp == 0x1A61) {
        return N00B_WB_Extend;
    }

    if (cp == 0x1A62) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1A63 && cp <= 0x1A64) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1A65 && cp <= 0x1A6C) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1A6D && cp <= 0x1A72) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1A73 && cp <= 0x1A7C) {
        return N00B_WB_Extend;
    }

    if (cp == 0x1A7F) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1AB0 && cp <= 0x1ABD) {
        return N00B_WB_Extend;
    }

    if (cp == 0x1ABE) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1ABF && cp <= 0x1ACE) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1B00 && cp <= 0x1B03) {
        return N00B_WB_Extend;
    }

    if (cp == 0x1B04) {
        return N00B_WB_Extend;
    }

    if (cp == 0x1B34) {
        return N00B_WB_Extend;
    }

    if (cp == 0x1B35) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1B36 && cp <= 0x1B3A) {
        return N00B_WB_Extend;
    }

    if (cp == 0x1B3B) {
        return N00B_WB_Extend;
    }

    if (cp == 0x1B3C) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1B3D && cp <= 0x1B41) {
        return N00B_WB_Extend;
    }

    if (cp == 0x1B42) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1B43 && cp <= 0x1B44) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1B6B && cp <= 0x1B73) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1B80 && cp <= 0x1B81) {
        return N00B_WB_Extend;
    }

    if (cp == 0x1B82) {
        return N00B_WB_Extend;
    }

    if (cp == 0x1BA1) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1BA2 && cp <= 0x1BA5) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1BA6 && cp <= 0x1BA7) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1BA8 && cp <= 0x1BA9) {
        return N00B_WB_Extend;
    }

    if (cp == 0x1BAA) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1BAB && cp <= 0x1BAD) {
        return N00B_WB_Extend;
    }

    if (cp == 0x1BE6) {
        return N00B_WB_Extend;
    }

    if (cp == 0x1BE7) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1BE8 && cp <= 0x1BE9) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1BEA && cp <= 0x1BEC) {
        return N00B_WB_Extend;
    }

    if (cp == 0x1BED) {
        return N00B_WB_Extend;
    }

    if (cp == 0x1BEE) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1BEF && cp <= 0x1BF1) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1BF2 && cp <= 0x1BF3) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1C24 && cp <= 0x1C2B) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1C2C && cp <= 0x1C33) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1C34 && cp <= 0x1C35) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1C36 && cp <= 0x1C37) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1CD0 && cp <= 0x1CD2) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1CD4 && cp <= 0x1CE0) {
        return N00B_WB_Extend;
    }

    if (cp == 0x1CE1) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1CE2 && cp <= 0x1CE8) {
        return N00B_WB_Extend;
    }

    if (cp == 0x1CED) {
        return N00B_WB_Extend;
    }

    if (cp == 0x1CF4) {
        return N00B_WB_Extend;
    }

    if (cp == 0x1CF7) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1CF8 && cp <= 0x1CF9) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1DC0 && cp <= 0x1DFF) {
        return N00B_WB_Extend;
    }

    if (cp == 0x200C) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x20D0 && cp <= 0x20DC) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x20DD && cp <= 0x20E0) {
        return N00B_WB_Extend;
    }

    if (cp == 0x20E1) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x20E2 && cp <= 0x20E4) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x20E5 && cp <= 0x20F0) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x2CEF && cp <= 0x2CF1) {
        return N00B_WB_Extend;
    }

    if (cp == 0x2D7F) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x2DE0 && cp <= 0x2DFF) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x302A && cp <= 0x302D) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x302E && cp <= 0x302F) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x3099 && cp <= 0x309A) {
        return N00B_WB_Extend;
    }

    if (cp == 0xA66F) {
        return N00B_WB_Extend;
    }

    if (cp >= 0xA670 && cp <= 0xA672) {
        return N00B_WB_Extend;
    }

    if (cp >= 0xA674 && cp <= 0xA67D) {
        return N00B_WB_Extend;
    }

    if (cp >= 0xA69E && cp <= 0xA69F) {
        return N00B_WB_Extend;
    }

    if (cp >= 0xA6F0 && cp <= 0xA6F1) {
        return N00B_WB_Extend;
    }

    if (cp == 0xA802) {
        return N00B_WB_Extend;
    }

    if (cp == 0xA806) {
        return N00B_WB_Extend;
    }

    if (cp == 0xA80B) {
        return N00B_WB_Extend;
    }

    if (cp >= 0xA823 && cp <= 0xA824) {
        return N00B_WB_Extend;
    }

    if (cp >= 0xA825 && cp <= 0xA826) {
        return N00B_WB_Extend;
    }

    if (cp == 0xA827) {
        return N00B_WB_Extend;
    }

    if (cp == 0xA82C) {
        return N00B_WB_Extend;
    }

    if (cp >= 0xA880 && cp <= 0xA881) {
        return N00B_WB_Extend;
    }

    if (cp >= 0xA8B4 && cp <= 0xA8C3) {
        return N00B_WB_Extend;
    }

    if (cp >= 0xA8C4 && cp <= 0xA8C5) {
        return N00B_WB_Extend;
    }

    if (cp >= 0xA8E0 && cp <= 0xA8F1) {
        return N00B_WB_Extend;
    }

    if (cp == 0xA8FF) {
        return N00B_WB_Extend;
    }

    if (cp >= 0xA926 && cp <= 0xA92D) {
        return N00B_WB_Extend;
    }

    if (cp >= 0xA947 && cp <= 0xA951) {
        return N00B_WB_Extend;
    }

    if (cp >= 0xA952 && cp <= 0xA953) {
        return N00B_WB_Extend;
    }

    if (cp >= 0xA980 && cp <= 0xA982) {
        return N00B_WB_Extend;
    }

    if (cp == 0xA983) {
        return N00B_WB_Extend;
    }

    if (cp == 0xA9B3) {
        return N00B_WB_Extend;
    }

    if (cp >= 0xA9B4 && cp <= 0xA9B5) {
        return N00B_WB_Extend;
    }

    if (cp >= 0xA9B6 && cp <= 0xA9B9) {
        return N00B_WB_Extend;
    }

    if (cp >= 0xA9BA && cp <= 0xA9BB) {
        return N00B_WB_Extend;
    }

    if (cp >= 0xA9BC && cp <= 0xA9BD) {
        return N00B_WB_Extend;
    }

    if (cp >= 0xA9BE && cp <= 0xA9C0) {
        return N00B_WB_Extend;
    }

    if (cp == 0xA9E5) {
        return N00B_WB_Extend;
    }

    if (cp >= 0xAA29 && cp <= 0xAA2E) {
        return N00B_WB_Extend;
    }

    if (cp >= 0xAA2F && cp <= 0xAA30) {
        return N00B_WB_Extend;
    }

    if (cp >= 0xAA31 && cp <= 0xAA32) {
        return N00B_WB_Extend;
    }

    if (cp >= 0xAA33 && cp <= 0xAA34) {
        return N00B_WB_Extend;
    }

    if (cp >= 0xAA35 && cp <= 0xAA36) {
        return N00B_WB_Extend;
    }

    if (cp == 0xAA43) {
        return N00B_WB_Extend;
    }

    if (cp == 0xAA4C) {
        return N00B_WB_Extend;
    }

    if (cp == 0xAA4D) {
        return N00B_WB_Extend;
    }

    if (cp == 0xAA7B) {
        return N00B_WB_Extend;
    }

    if (cp == 0xAA7C) {
        return N00B_WB_Extend;
    }

    if (cp == 0xAA7D) {
        return N00B_WB_Extend;
    }

    if (cp == 0xAAB0) {
        return N00B_WB_Extend;
    }

    if (cp >= 0xAAB2 && cp <= 0xAAB4) {
        return N00B_WB_Extend;
    }

    if (cp >= 0xAAB7 && cp <= 0xAAB8) {
        return N00B_WB_Extend;
    }

    if (cp >= 0xAABE && cp <= 0xAABF) {
        return N00B_WB_Extend;
    }

    if (cp == 0xAAC1) {
        return N00B_WB_Extend;
    }

    if (cp == 0xAAEB) {
        return N00B_WB_Extend;
    }

    if (cp >= 0xAAEC && cp <= 0xAAED) {
        return N00B_WB_Extend;
    }

    if (cp >= 0xAAEE && cp <= 0xAAEF) {
        return N00B_WB_Extend;
    }

    if (cp == 0xAAF5) {
        return N00B_WB_Extend;
    }

    if (cp == 0xAAF6) {
        return N00B_WB_Extend;
    }

    if (cp >= 0xABE3 && cp <= 0xABE4) {
        return N00B_WB_Extend;
    }

    if (cp == 0xABE5) {
        return N00B_WB_Extend;
    }

    if (cp >= 0xABE6 && cp <= 0xABE7) {
        return N00B_WB_Extend;
    }

    if (cp == 0xABE8) {
        return N00B_WB_Extend;
    }

    if (cp >= 0xABE9 && cp <= 0xABEA) {
        return N00B_WB_Extend;
    }

    if (cp == 0xABEC) {
        return N00B_WB_Extend;
    }

    if (cp == 0xABED) {
        return N00B_WB_Extend;
    }

    if (cp == 0xFB1E) {
        return N00B_WB_Extend;
    }

    if (cp >= 0xFE00 && cp <= 0xFE0F) {
        return N00B_WB_Extend;
    }

    if (cp >= 0xFE20 && cp <= 0xFE2F) {
        return N00B_WB_Extend;
    }

    if (cp >= 0xFF9E && cp <= 0xFF9F) {
        return N00B_WB_Extend;
    }

    if (cp == 0x101FD) {
        return N00B_WB_Extend;
    }

    if (cp == 0x102E0) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x10376 && cp <= 0x1037A) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x10A01 && cp <= 0x10A03) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x10A05 && cp <= 0x10A06) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x10A0C && cp <= 0x10A0F) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x10A38 && cp <= 0x10A3A) {
        return N00B_WB_Extend;
    }

    if (cp == 0x10A3F) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x10AE5 && cp <= 0x10AE6) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x10D24 && cp <= 0x10D27) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x10D69 && cp <= 0x10D6D) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x10EAB && cp <= 0x10EAC) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x10EFC && cp <= 0x10EFF) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x10F46 && cp <= 0x10F50) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x10F82 && cp <= 0x10F85) {
        return N00B_WB_Extend;
    }

    if (cp == 0x11000) {
        return N00B_WB_Extend;
    }

    if (cp == 0x11001) {
        return N00B_WB_Extend;
    }

    if (cp == 0x11002) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x11038 && cp <= 0x11046) {
        return N00B_WB_Extend;
    }

    if (cp == 0x11070) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x11073 && cp <= 0x11074) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1107F && cp <= 0x11081) {
        return N00B_WB_Extend;
    }

    if (cp == 0x11082) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x110B0 && cp <= 0x110B2) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x110B3 && cp <= 0x110B6) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x110B7 && cp <= 0x110B8) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x110B9 && cp <= 0x110BA) {
        return N00B_WB_Extend;
    }

    if (cp == 0x110C2) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x11100 && cp <= 0x11102) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x11127 && cp <= 0x1112B) {
        return N00B_WB_Extend;
    }

    if (cp == 0x1112C) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1112D && cp <= 0x11134) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x11145 && cp <= 0x11146) {
        return N00B_WB_Extend;
    }

    if (cp == 0x11173) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x11180 && cp <= 0x11181) {
        return N00B_WB_Extend;
    }

    if (cp == 0x11182) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x111B3 && cp <= 0x111B5) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x111B6 && cp <= 0x111BE) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x111BF && cp <= 0x111C0) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x111C9 && cp <= 0x111CC) {
        return N00B_WB_Extend;
    }

    if (cp == 0x111CE) {
        return N00B_WB_Extend;
    }

    if (cp == 0x111CF) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1122C && cp <= 0x1122E) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1122F && cp <= 0x11231) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x11232 && cp <= 0x11233) {
        return N00B_WB_Extend;
    }

    if (cp == 0x11234) {
        return N00B_WB_Extend;
    }

    if (cp == 0x11235) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x11236 && cp <= 0x11237) {
        return N00B_WB_Extend;
    }

    if (cp == 0x1123E) {
        return N00B_WB_Extend;
    }

    if (cp == 0x11241) {
        return N00B_WB_Extend;
    }

    if (cp == 0x112DF) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x112E0 && cp <= 0x112E2) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x112E3 && cp <= 0x112EA) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x11300 && cp <= 0x11301) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x11302 && cp <= 0x11303) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1133B && cp <= 0x1133C) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1133E && cp <= 0x1133F) {
        return N00B_WB_Extend;
    }

    if (cp == 0x11340) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x11341 && cp <= 0x11344) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x11347 && cp <= 0x11348) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1134B && cp <= 0x1134D) {
        return N00B_WB_Extend;
    }

    if (cp == 0x11357) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x11362 && cp <= 0x11363) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x11366 && cp <= 0x1136C) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x11370 && cp <= 0x11374) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x113B8 && cp <= 0x113BA) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x113BB && cp <= 0x113C0) {
        return N00B_WB_Extend;
    }

    if (cp == 0x113C2) {
        return N00B_WB_Extend;
    }

    if (cp == 0x113C5) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x113C7 && cp <= 0x113CA) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x113CC && cp <= 0x113CD) {
        return N00B_WB_Extend;
    }

    if (cp == 0x113CE) {
        return N00B_WB_Extend;
    }

    if (cp == 0x113CF) {
        return N00B_WB_Extend;
    }

    if (cp == 0x113D0) {
        return N00B_WB_Extend;
    }

    if (cp == 0x113D2) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x113E1 && cp <= 0x113E2) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x11435 && cp <= 0x11437) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x11438 && cp <= 0x1143F) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x11440 && cp <= 0x11441) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x11442 && cp <= 0x11444) {
        return N00B_WB_Extend;
    }

    if (cp == 0x11445) {
        return N00B_WB_Extend;
    }

    if (cp == 0x11446) {
        return N00B_WB_Extend;
    }

    if (cp == 0x1145E) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x114B0 && cp <= 0x114B2) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x114B3 && cp <= 0x114B8) {
        return N00B_WB_Extend;
    }

    if (cp == 0x114B9) {
        return N00B_WB_Extend;
    }

    if (cp == 0x114BA) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x114BB && cp <= 0x114BE) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x114BF && cp <= 0x114C0) {
        return N00B_WB_Extend;
    }

    if (cp == 0x114C1) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x114C2 && cp <= 0x114C3) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x115AF && cp <= 0x115B1) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x115B2 && cp <= 0x115B5) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x115B8 && cp <= 0x115BB) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x115BC && cp <= 0x115BD) {
        return N00B_WB_Extend;
    }

    if (cp == 0x115BE) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x115BF && cp <= 0x115C0) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x115DC && cp <= 0x115DD) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x11630 && cp <= 0x11632) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x11633 && cp <= 0x1163A) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1163B && cp <= 0x1163C) {
        return N00B_WB_Extend;
    }

    if (cp == 0x1163D) {
        return N00B_WB_Extend;
    }

    if (cp == 0x1163E) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1163F && cp <= 0x11640) {
        return N00B_WB_Extend;
    }

    if (cp == 0x116AB) {
        return N00B_WB_Extend;
    }

    if (cp == 0x116AC) {
        return N00B_WB_Extend;
    }

    if (cp == 0x116AD) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x116AE && cp <= 0x116AF) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x116B0 && cp <= 0x116B5) {
        return N00B_WB_Extend;
    }

    if (cp == 0x116B6) {
        return N00B_WB_Extend;
    }

    if (cp == 0x116B7) {
        return N00B_WB_Extend;
    }

    if (cp == 0x1171D) {
        return N00B_WB_Extend;
    }

    if (cp == 0x1171E) {
        return N00B_WB_Extend;
    }

    if (cp == 0x1171F) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x11720 && cp <= 0x11721) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x11722 && cp <= 0x11725) {
        return N00B_WB_Extend;
    }

    if (cp == 0x11726) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x11727 && cp <= 0x1172B) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1182C && cp <= 0x1182E) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1182F && cp <= 0x11837) {
        return N00B_WB_Extend;
    }

    if (cp == 0x11838) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x11839 && cp <= 0x1183A) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x11930 && cp <= 0x11935) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x11937 && cp <= 0x11938) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1193B && cp <= 0x1193C) {
        return N00B_WB_Extend;
    }

    if (cp == 0x1193D) {
        return N00B_WB_Extend;
    }

    if (cp == 0x1193E) {
        return N00B_WB_Extend;
    }

    if (cp == 0x11940) {
        return N00B_WB_Extend;
    }

    if (cp == 0x11942) {
        return N00B_WB_Extend;
    }

    if (cp == 0x11943) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x119D1 && cp <= 0x119D3) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x119D4 && cp <= 0x119D7) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x119DA && cp <= 0x119DB) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x119DC && cp <= 0x119DF) {
        return N00B_WB_Extend;
    }

    if (cp == 0x119E0) {
        return N00B_WB_Extend;
    }

    if (cp == 0x119E4) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x11A01 && cp <= 0x11A0A) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x11A33 && cp <= 0x11A38) {
        return N00B_WB_Extend;
    }

    if (cp == 0x11A39) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x11A3B && cp <= 0x11A3E) {
        return N00B_WB_Extend;
    }

    if (cp == 0x11A47) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x11A51 && cp <= 0x11A56) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x11A57 && cp <= 0x11A58) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x11A59 && cp <= 0x11A5B) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x11A8A && cp <= 0x11A96) {
        return N00B_WB_Extend;
    }

    if (cp == 0x11A97) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x11A98 && cp <= 0x11A99) {
        return N00B_WB_Extend;
    }

    if (cp == 0x11C2F) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x11C30 && cp <= 0x11C36) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x11C38 && cp <= 0x11C3D) {
        return N00B_WB_Extend;
    }

    if (cp == 0x11C3E) {
        return N00B_WB_Extend;
    }

    if (cp == 0x11C3F) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x11C92 && cp <= 0x11CA7) {
        return N00B_WB_Extend;
    }

    if (cp == 0x11CA9) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x11CAA && cp <= 0x11CB0) {
        return N00B_WB_Extend;
    }

    if (cp == 0x11CB1) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x11CB2 && cp <= 0x11CB3) {
        return N00B_WB_Extend;
    }

    if (cp == 0x11CB4) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x11CB5 && cp <= 0x11CB6) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x11D31 && cp <= 0x11D36) {
        return N00B_WB_Extend;
    }

    if (cp == 0x11D3A) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x11D3C && cp <= 0x11D3D) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x11D3F && cp <= 0x11D45) {
        return N00B_WB_Extend;
    }

    if (cp == 0x11D47) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x11D8A && cp <= 0x11D8E) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x11D90 && cp <= 0x11D91) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x11D93 && cp <= 0x11D94) {
        return N00B_WB_Extend;
    }

    if (cp == 0x11D95) {
        return N00B_WB_Extend;
    }

    if (cp == 0x11D96) {
        return N00B_WB_Extend;
    }

    if (cp == 0x11D97) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x11EF3 && cp <= 0x11EF4) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x11EF5 && cp <= 0x11EF6) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x11F00 && cp <= 0x11F01) {
        return N00B_WB_Extend;
    }

    if (cp == 0x11F03) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x11F34 && cp <= 0x11F35) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x11F36 && cp <= 0x11F3A) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x11F3E && cp <= 0x11F3F) {
        return N00B_WB_Extend;
    }

    if (cp == 0x11F40) {
        return N00B_WB_Extend;
    }

    if (cp == 0x11F41) {
        return N00B_WB_Extend;
    }

    if (cp == 0x11F42) {
        return N00B_WB_Extend;
    }

    if (cp == 0x11F5A) {
        return N00B_WB_Extend;
    }

    if (cp == 0x13440) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x13447 && cp <= 0x13455) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1611E && cp <= 0x16129) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1612A && cp <= 0x1612C) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1612D && cp <= 0x1612F) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x16AF0 && cp <= 0x16AF4) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x16B30 && cp <= 0x16B36) {
        return N00B_WB_Extend;
    }

    if (cp == 0x16F4F) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x16F51 && cp <= 0x16F87) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x16F8F && cp <= 0x16F92) {
        return N00B_WB_Extend;
    }

    if (cp == 0x16FE4) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x16FF0 && cp <= 0x16FF1) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1BC9D && cp <= 0x1BC9E) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1CF00 && cp <= 0x1CF2D) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1CF30 && cp <= 0x1CF46) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1D165 && cp <= 0x1D166) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1D167 && cp <= 0x1D169) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1D16D && cp <= 0x1D172) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1D17B && cp <= 0x1D182) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1D185 && cp <= 0x1D18B) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1D1AA && cp <= 0x1D1AD) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1D242 && cp <= 0x1D244) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1DA00 && cp <= 0x1DA36) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1DA3B && cp <= 0x1DA6C) {
        return N00B_WB_Extend;
    }

    if (cp == 0x1DA75) {
        return N00B_WB_Extend;
    }

    if (cp == 0x1DA84) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1DA9B && cp <= 0x1DA9F) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1DAA1 && cp <= 0x1DAAF) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1E000 && cp <= 0x1E006) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1E008 && cp <= 0x1E018) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1E01B && cp <= 0x1E021) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1E023 && cp <= 0x1E024) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1E026 && cp <= 0x1E02A) {
        return N00B_WB_Extend;
    }

    if (cp == 0x1E08F) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1E130 && cp <= 0x1E136) {
        return N00B_WB_Extend;
    }

    if (cp == 0x1E2AE) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1E2EC && cp <= 0x1E2EF) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1E4EC && cp <= 0x1E4EF) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1E5EE && cp <= 0x1E5EF) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1E8D0 && cp <= 0x1E8D6) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1E944 && cp <= 0x1E94A) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1F3FB && cp <= 0x1F3FF) {
        return N00B_WB_Extend;
    }

    if (cp >= 0xE0020 && cp <= 0xE007F) {
        return N00B_WB_Extend;
    }

    if (cp >= 0xE0100 && cp <= 0xE01EF) {
        return N00B_WB_Extend;
    }

    if (cp >= 0x1F1E6 && cp <= 0x1F1FF) {
        return N00B_WB_Regional_Indicator;
    }

    if (cp == 0x00AD) {
        return N00B_WB_Format;
    }

    if (cp == 0x061C) {
        return N00B_WB_Format;
    }

    if (cp == 0x180E) {
        return N00B_WB_Format;
    }

    if (cp >= 0x200E && cp <= 0x200F) {
        return N00B_WB_Format;
    }

    if (cp >= 0x202A && cp <= 0x202E) {
        return N00B_WB_Format;
    }

    if (cp >= 0x2060 && cp <= 0x2064) {
        return N00B_WB_Format;
    }

    if (cp >= 0x2066 && cp <= 0x206F) {
        return N00B_WB_Format;
    }

    if (cp == 0xFEFF) {
        return N00B_WB_Format;
    }

    if (cp >= 0xFFF9 && cp <= 0xFFFB) {
        return N00B_WB_Format;
    }

    if (cp >= 0x13430 && cp <= 0x1343F) {
        return N00B_WB_Format;
    }

    if (cp >= 0x1BCA0 && cp <= 0x1BCA3) {
        return N00B_WB_Format;
    }

    if (cp >= 0x1D173 && cp <= 0x1D17A) {
        return N00B_WB_Format;
    }

    if (cp == 0xE0001) {
        return N00B_WB_Format;
    }

    if (cp >= 0x3031 && cp <= 0x3035) {
        return N00B_WB_Katakana;
    }

    if (cp >= 0x309B && cp <= 0x309C) {
        return N00B_WB_Katakana;
    }

    if (cp == 0x30A0) {
        return N00B_WB_Katakana;
    }

    if (cp >= 0x30A1 && cp <= 0x30FA) {
        return N00B_WB_Katakana;
    }

    if (cp >= 0x30FC && cp <= 0x30FE) {
        return N00B_WB_Katakana;
    }

    if (cp == 0x30FF) {
        return N00B_WB_Katakana;
    }

    if (cp >= 0x31F0 && cp <= 0x31FF) {
        return N00B_WB_Katakana;
    }

    if (cp >= 0x32D0 && cp <= 0x32FE) {
        return N00B_WB_Katakana;
    }

    if (cp >= 0x3300 && cp <= 0x3357) {
        return N00B_WB_Katakana;
    }

    if (cp >= 0xFF66 && cp <= 0xFF6F) {
        return N00B_WB_Katakana;
    }

    if (cp == 0xFF70) {
        return N00B_WB_Katakana;
    }

    if (cp >= 0xFF71 && cp <= 0xFF9D) {
        return N00B_WB_Katakana;
    }

    if (cp >= 0x1AFF0 && cp <= 0x1AFF3) {
        return N00B_WB_Katakana;
    }

    if (cp >= 0x1AFF5 && cp <= 0x1AFFB) {
        return N00B_WB_Katakana;
    }

    if (cp >= 0x1AFFD && cp <= 0x1AFFE) {
        return N00B_WB_Katakana;
    }

    if (cp == 0x1B000) {
        return N00B_WB_Katakana;
    }

    if (cp >= 0x1B120 && cp <= 0x1B122) {
        return N00B_WB_Katakana;
    }

    if (cp == 0x1B155) {
        return N00B_WB_Katakana;
    }

    if (cp >= 0x1B164 && cp <= 0x1B167) {
        return N00B_WB_Katakana;
    }

    if (cp >= 0x0041 && cp <= 0x005A) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0061 && cp <= 0x007A) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x00AA) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x00B5) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x00BA) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x00C0 && cp <= 0x00D6) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x00D8 && cp <= 0x00F6) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x00F8 && cp <= 0x01BA) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x01BB) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x01BC && cp <= 0x01BF) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x01C0 && cp <= 0x01C3) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x01C4 && cp <= 0x0293) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x0294) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0295 && cp <= 0x02AF) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x02B0 && cp <= 0x02C1) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x02C2 && cp <= 0x02C5) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x02C6 && cp <= 0x02D1) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x02D2 && cp <= 0x02D7) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x02DE && cp <= 0x02DF) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x02E0 && cp <= 0x02E4) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x02E5 && cp <= 0x02EB) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x02EC) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x02ED) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x02EE) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x02EF && cp <= 0x02FF) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0370 && cp <= 0x0373) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x0374) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0376 && cp <= 0x0377) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x037A) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x037B && cp <= 0x037D) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x037F) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x0386) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0388 && cp <= 0x038A) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x038C) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x038E && cp <= 0x03A1) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x03A3 && cp <= 0x03F5) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x03F7 && cp <= 0x0481) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x048A && cp <= 0x052F) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0531 && cp <= 0x0556) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x0559) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x055A && cp <= 0x055C) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x055E) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0560 && cp <= 0x0588) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x058A) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x05F3) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0620 && cp <= 0x063F) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x0640) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0641 && cp <= 0x064A) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x066E && cp <= 0x066F) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0671 && cp <= 0x06D3) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x06D5) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x06E5 && cp <= 0x06E6) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x06EE && cp <= 0x06EF) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x06FA && cp <= 0x06FC) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x06FF) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x070F) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x0710) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0712 && cp <= 0x072F) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x074D && cp <= 0x07A5) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x07B1) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x07CA && cp <= 0x07EA) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x07F4 && cp <= 0x07F5) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x07FA) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0800 && cp <= 0x0815) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x081A) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x0824) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x0828) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0840 && cp <= 0x0858) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0860 && cp <= 0x086A) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0870 && cp <= 0x0887) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0889 && cp <= 0x088E) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x08A0 && cp <= 0x08C8) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x08C9) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0904 && cp <= 0x0939) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x093D) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x0950) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0958 && cp <= 0x0961) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x0971) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0972 && cp <= 0x0980) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0985 && cp <= 0x098C) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x098F && cp <= 0x0990) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0993 && cp <= 0x09A8) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x09AA && cp <= 0x09B0) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x09B2) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x09B6 && cp <= 0x09B9) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x09BD) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x09CE) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x09DC && cp <= 0x09DD) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x09DF && cp <= 0x09E1) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x09F0 && cp <= 0x09F1) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x09FC) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0A05 && cp <= 0x0A0A) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0A0F && cp <= 0x0A10) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0A13 && cp <= 0x0A28) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0A2A && cp <= 0x0A30) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0A32 && cp <= 0x0A33) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0A35 && cp <= 0x0A36) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0A38 && cp <= 0x0A39) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0A59 && cp <= 0x0A5C) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x0A5E) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0A72 && cp <= 0x0A74) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0A85 && cp <= 0x0A8D) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0A8F && cp <= 0x0A91) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0A93 && cp <= 0x0AA8) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0AAA && cp <= 0x0AB0) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0AB2 && cp <= 0x0AB3) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0AB5 && cp <= 0x0AB9) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x0ABD) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x0AD0) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0AE0 && cp <= 0x0AE1) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x0AF9) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0B05 && cp <= 0x0B0C) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0B0F && cp <= 0x0B10) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0B13 && cp <= 0x0B28) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0B2A && cp <= 0x0B30) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0B32 && cp <= 0x0B33) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0B35 && cp <= 0x0B39) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x0B3D) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0B5C && cp <= 0x0B5D) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0B5F && cp <= 0x0B61) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x0B71) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x0B83) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0B85 && cp <= 0x0B8A) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0B8E && cp <= 0x0B90) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0B92 && cp <= 0x0B95) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0B99 && cp <= 0x0B9A) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x0B9C) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0B9E && cp <= 0x0B9F) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0BA3 && cp <= 0x0BA4) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0BA8 && cp <= 0x0BAA) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0BAE && cp <= 0x0BB9) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x0BD0) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0C05 && cp <= 0x0C0C) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0C0E && cp <= 0x0C10) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0C12 && cp <= 0x0C28) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0C2A && cp <= 0x0C39) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x0C3D) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0C58 && cp <= 0x0C5A) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x0C5D) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0C60 && cp <= 0x0C61) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x0C80) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0C85 && cp <= 0x0C8C) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0C8E && cp <= 0x0C90) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0C92 && cp <= 0x0CA8) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0CAA && cp <= 0x0CB3) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0CB5 && cp <= 0x0CB9) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x0CBD) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0CDD && cp <= 0x0CDE) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0CE0 && cp <= 0x0CE1) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0CF1 && cp <= 0x0CF2) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0D04 && cp <= 0x0D0C) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0D0E && cp <= 0x0D10) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0D12 && cp <= 0x0D3A) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x0D3D) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x0D4E) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0D54 && cp <= 0x0D56) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0D5F && cp <= 0x0D61) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0D7A && cp <= 0x0D7F) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0D85 && cp <= 0x0D96) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0D9A && cp <= 0x0DB1) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0DB3 && cp <= 0x0DBB) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x0DBD) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0DC0 && cp <= 0x0DC6) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x0F00) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0F40 && cp <= 0x0F47) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0F49 && cp <= 0x0F6C) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x0F88 && cp <= 0x0F8C) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x10A0 && cp <= 0x10C5) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x10C7) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x10CD) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x10D0 && cp <= 0x10FA) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x10FC) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x10FD && cp <= 0x10FF) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1100 && cp <= 0x1248) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x124A && cp <= 0x124D) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1250 && cp <= 0x1256) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x1258) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x125A && cp <= 0x125D) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1260 && cp <= 0x1288) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x128A && cp <= 0x128D) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1290 && cp <= 0x12B0) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x12B2 && cp <= 0x12B5) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x12B8 && cp <= 0x12BE) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x12C0) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x12C2 && cp <= 0x12C5) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x12C8 && cp <= 0x12D6) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x12D8 && cp <= 0x1310) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1312 && cp <= 0x1315) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1318 && cp <= 0x135A) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1380 && cp <= 0x138F) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x13A0 && cp <= 0x13F5) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x13F8 && cp <= 0x13FD) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1401 && cp <= 0x166C) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x166F && cp <= 0x167F) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1681 && cp <= 0x169A) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x16A0 && cp <= 0x16EA) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x16EE && cp <= 0x16F0) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x16F1 && cp <= 0x16F8) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1700 && cp <= 0x1711) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x171F && cp <= 0x1731) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1740 && cp <= 0x1751) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1760 && cp <= 0x176C) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x176E && cp <= 0x1770) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1820 && cp <= 0x1842) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x1843) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1844 && cp <= 0x1878) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1880 && cp <= 0x1884) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1887 && cp <= 0x18A8) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x18AA) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x18B0 && cp <= 0x18F5) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1900 && cp <= 0x191E) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1A00 && cp <= 0x1A16) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1B05 && cp <= 0x1B33) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1B45 && cp <= 0x1B4C) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1B83 && cp <= 0x1BA0) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1BAE && cp <= 0x1BAF) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1BBA && cp <= 0x1BE5) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1C00 && cp <= 0x1C23) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1C4D && cp <= 0x1C4F) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1C5A && cp <= 0x1C77) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1C78 && cp <= 0x1C7D) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1C80 && cp <= 0x1C8A) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1C90 && cp <= 0x1CBA) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1CBD && cp <= 0x1CBF) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1CE9 && cp <= 0x1CEC) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1CEE && cp <= 0x1CF3) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1CF5 && cp <= 0x1CF6) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x1CFA) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1D00 && cp <= 0x1D2B) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1D2C && cp <= 0x1D6A) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1D6B && cp <= 0x1D77) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x1D78) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1D79 && cp <= 0x1D9A) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1D9B && cp <= 0x1DBF) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1E00 && cp <= 0x1F15) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1F18 && cp <= 0x1F1D) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1F20 && cp <= 0x1F45) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1F48 && cp <= 0x1F4D) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1F50 && cp <= 0x1F57) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x1F59) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x1F5B) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x1F5D) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1F5F && cp <= 0x1F7D) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1F80 && cp <= 0x1FB4) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1FB6 && cp <= 0x1FBC) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x1FBE) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1FC2 && cp <= 0x1FC4) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1FC6 && cp <= 0x1FCC) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1FD0 && cp <= 0x1FD3) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1FD6 && cp <= 0x1FDB) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1FE0 && cp <= 0x1FEC) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1FF2 && cp <= 0x1FF4) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1FF6 && cp <= 0x1FFC) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x2071) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x207F) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x2090 && cp <= 0x209C) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x2102) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x2107) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x210A && cp <= 0x2113) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x2115) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x2119 && cp <= 0x211D) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x2124) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x2126) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x2128) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x212A && cp <= 0x212D) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x212F && cp <= 0x2134) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x2135 && cp <= 0x2138) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x2139) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x213C && cp <= 0x213F) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x2145 && cp <= 0x2149) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x214E) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x2160 && cp <= 0x2182) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x2183 && cp <= 0x2184) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x2185 && cp <= 0x2188) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x24B6 && cp <= 0x24E9) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x2C00 && cp <= 0x2C7B) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x2C7C && cp <= 0x2C7D) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x2C7E && cp <= 0x2CE4) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x2CEB && cp <= 0x2CEE) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x2CF2 && cp <= 0x2CF3) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x2D00 && cp <= 0x2D25) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x2D27) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x2D2D) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x2D30 && cp <= 0x2D67) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x2D6F) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x2D80 && cp <= 0x2D96) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x2DA0 && cp <= 0x2DA6) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x2DA8 && cp <= 0x2DAE) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x2DB0 && cp <= 0x2DB6) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x2DB8 && cp <= 0x2DBE) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x2DC0 && cp <= 0x2DC6) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x2DC8 && cp <= 0x2DCE) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x2DD0 && cp <= 0x2DD6) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x2DD8 && cp <= 0x2DDE) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x2E2F) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x3005) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x303B) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x303C) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x3105 && cp <= 0x312F) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x3131 && cp <= 0x318E) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x31A0 && cp <= 0x31BF) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xA000 && cp <= 0xA014) {
        return N00B_WB_ALetter;
    }

    if (cp == 0xA015) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xA016 && cp <= 0xA48C) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xA4D0 && cp <= 0xA4F7) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xA4F8 && cp <= 0xA4FD) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xA500 && cp <= 0xA60B) {
        return N00B_WB_ALetter;
    }

    if (cp == 0xA60C) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xA610 && cp <= 0xA61F) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xA62A && cp <= 0xA62B) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xA640 && cp <= 0xA66D) {
        return N00B_WB_ALetter;
    }

    if (cp == 0xA66E) {
        return N00B_WB_ALetter;
    }

    if (cp == 0xA67F) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xA680 && cp <= 0xA69B) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xA69C && cp <= 0xA69D) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xA6A0 && cp <= 0xA6E5) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xA6E6 && cp <= 0xA6EF) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xA708 && cp <= 0xA716) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xA717 && cp <= 0xA71F) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xA720 && cp <= 0xA721) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xA722 && cp <= 0xA76F) {
        return N00B_WB_ALetter;
    }

    if (cp == 0xA770) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xA771 && cp <= 0xA787) {
        return N00B_WB_ALetter;
    }

    if (cp == 0xA788) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xA789 && cp <= 0xA78A) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xA78B && cp <= 0xA78E) {
        return N00B_WB_ALetter;
    }

    if (cp == 0xA78F) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xA790 && cp <= 0xA7CD) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xA7D0 && cp <= 0xA7D1) {
        return N00B_WB_ALetter;
    }

    if (cp == 0xA7D3) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xA7D5 && cp <= 0xA7DC) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xA7F2 && cp <= 0xA7F4) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xA7F5 && cp <= 0xA7F6) {
        return N00B_WB_ALetter;
    }

    if (cp == 0xA7F7) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xA7F8 && cp <= 0xA7F9) {
        return N00B_WB_ALetter;
    }

    if (cp == 0xA7FA) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xA7FB && cp <= 0xA801) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xA803 && cp <= 0xA805) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xA807 && cp <= 0xA80A) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xA80C && cp <= 0xA822) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xA840 && cp <= 0xA873) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xA882 && cp <= 0xA8B3) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xA8F2 && cp <= 0xA8F7) {
        return N00B_WB_ALetter;
    }

    if (cp == 0xA8FB) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xA8FD && cp <= 0xA8FE) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xA90A && cp <= 0xA925) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xA930 && cp <= 0xA946) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xA960 && cp <= 0xA97C) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xA984 && cp <= 0xA9B2) {
        return N00B_WB_ALetter;
    }

    if (cp == 0xA9CF) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xAA00 && cp <= 0xAA28) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xAA40 && cp <= 0xAA42) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xAA44 && cp <= 0xAA4B) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xAAE0 && cp <= 0xAAEA) {
        return N00B_WB_ALetter;
    }

    if (cp == 0xAAF2) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xAAF3 && cp <= 0xAAF4) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xAB01 && cp <= 0xAB06) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xAB09 && cp <= 0xAB0E) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xAB11 && cp <= 0xAB16) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xAB20 && cp <= 0xAB26) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xAB28 && cp <= 0xAB2E) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xAB30 && cp <= 0xAB5A) {
        return N00B_WB_ALetter;
    }

    if (cp == 0xAB5B) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xAB5C && cp <= 0xAB5F) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xAB60 && cp <= 0xAB68) {
        return N00B_WB_ALetter;
    }

    if (cp == 0xAB69) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xAB70 && cp <= 0xABBF) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xABC0 && cp <= 0xABE2) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xAC00 && cp <= 0xD7A3) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xD7B0 && cp <= 0xD7C6) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xD7CB && cp <= 0xD7FB) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xFB00 && cp <= 0xFB06) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xFB13 && cp <= 0xFB17) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xFB50 && cp <= 0xFBB1) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xFBD3 && cp <= 0xFD3D) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xFD50 && cp <= 0xFD8F) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xFD92 && cp <= 0xFDC7) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xFDF0 && cp <= 0xFDFB) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xFE70 && cp <= 0xFE74) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xFE76 && cp <= 0xFEFC) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xFF21 && cp <= 0xFF3A) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xFF41 && cp <= 0xFF5A) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xFFA0 && cp <= 0xFFBE) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xFFC2 && cp <= 0xFFC7) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xFFCA && cp <= 0xFFCF) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xFFD2 && cp <= 0xFFD7) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0xFFDA && cp <= 0xFFDC) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x10000 && cp <= 0x1000B) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1000D && cp <= 0x10026) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x10028 && cp <= 0x1003A) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1003C && cp <= 0x1003D) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1003F && cp <= 0x1004D) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x10050 && cp <= 0x1005D) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x10080 && cp <= 0x100FA) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x10140 && cp <= 0x10174) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x10280 && cp <= 0x1029C) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x102A0 && cp <= 0x102D0) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x10300 && cp <= 0x1031F) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1032D && cp <= 0x10340) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x10341) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x10342 && cp <= 0x10349) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x1034A) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x10350 && cp <= 0x10375) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x10380 && cp <= 0x1039D) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x103A0 && cp <= 0x103C3) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x103C8 && cp <= 0x103CF) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x103D1 && cp <= 0x103D5) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x10400 && cp <= 0x1044F) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x10450 && cp <= 0x1049D) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x104B0 && cp <= 0x104D3) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x104D8 && cp <= 0x104FB) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x10500 && cp <= 0x10527) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x10530 && cp <= 0x10563) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x10570 && cp <= 0x1057A) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1057C && cp <= 0x1058A) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1058C && cp <= 0x10592) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x10594 && cp <= 0x10595) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x10597 && cp <= 0x105A1) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x105A3 && cp <= 0x105B1) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x105B3 && cp <= 0x105B9) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x105BB && cp <= 0x105BC) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x105C0 && cp <= 0x105F3) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x10600 && cp <= 0x10736) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x10740 && cp <= 0x10755) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x10760 && cp <= 0x10767) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x10780 && cp <= 0x10785) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x10787 && cp <= 0x107B0) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x107B2 && cp <= 0x107BA) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x10800 && cp <= 0x10805) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x10808) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1080A && cp <= 0x10835) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x10837 && cp <= 0x10838) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x1083C) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1083F && cp <= 0x10855) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x10860 && cp <= 0x10876) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x10880 && cp <= 0x1089E) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x108E0 && cp <= 0x108F2) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x108F4 && cp <= 0x108F5) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x10900 && cp <= 0x10915) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x10920 && cp <= 0x10939) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x10980 && cp <= 0x109B7) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x109BE && cp <= 0x109BF) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x10A00) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x10A10 && cp <= 0x10A13) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x10A15 && cp <= 0x10A17) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x10A19 && cp <= 0x10A35) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x10A60 && cp <= 0x10A7C) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x10A80 && cp <= 0x10A9C) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x10AC0 && cp <= 0x10AC7) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x10AC9 && cp <= 0x10AE4) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x10B00 && cp <= 0x10B35) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x10B40 && cp <= 0x10B55) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x10B60 && cp <= 0x10B72) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x10B80 && cp <= 0x10B91) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x10C00 && cp <= 0x10C48) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x10C80 && cp <= 0x10CB2) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x10CC0 && cp <= 0x10CF2) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x10D00 && cp <= 0x10D23) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x10D4A && cp <= 0x10D4D) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x10D4E) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x10D4F) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x10D50 && cp <= 0x10D65) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x10D6F) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x10D70 && cp <= 0x10D85) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x10E80 && cp <= 0x10EA9) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x10EB0 && cp <= 0x10EB1) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x10EC2 && cp <= 0x10EC4) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x10F00 && cp <= 0x10F1C) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x10F27) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x10F30 && cp <= 0x10F45) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x10F70 && cp <= 0x10F81) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x10FB0 && cp <= 0x10FC4) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x10FE0 && cp <= 0x10FF6) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x11003 && cp <= 0x11037) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x11071 && cp <= 0x11072) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x11075) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x11083 && cp <= 0x110AF) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x110D0 && cp <= 0x110E8) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x11103 && cp <= 0x11126) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x11144) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x11147) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x11150 && cp <= 0x11172) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x11176) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x11183 && cp <= 0x111B2) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x111C1 && cp <= 0x111C4) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x111DA) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x111DC) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x11200 && cp <= 0x11211) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x11213 && cp <= 0x1122B) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1123F && cp <= 0x11240) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x11280 && cp <= 0x11286) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x11288) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1128A && cp <= 0x1128D) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1128F && cp <= 0x1129D) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1129F && cp <= 0x112A8) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x112B0 && cp <= 0x112DE) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x11305 && cp <= 0x1130C) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1130F && cp <= 0x11310) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x11313 && cp <= 0x11328) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1132A && cp <= 0x11330) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x11332 && cp <= 0x11333) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x11335 && cp <= 0x11339) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x1133D) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x11350) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1135D && cp <= 0x11361) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x11380 && cp <= 0x11389) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x1138B) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x1138E) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x11390 && cp <= 0x113B5) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x113B7) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x113D1) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x113D3) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x11400 && cp <= 0x11434) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x11447 && cp <= 0x1144A) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1145F && cp <= 0x11461) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x11480 && cp <= 0x114AF) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x114C4 && cp <= 0x114C5) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x114C7) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x11580 && cp <= 0x115AE) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x115D8 && cp <= 0x115DB) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x11600 && cp <= 0x1162F) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x11644) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x11680 && cp <= 0x116AA) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x116B8) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x11800 && cp <= 0x1182B) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x118A0 && cp <= 0x118DF) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x118FF && cp <= 0x11906) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x11909) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1190C && cp <= 0x11913) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x11915 && cp <= 0x11916) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x11918 && cp <= 0x1192F) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x1193F) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x11941) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x119A0 && cp <= 0x119A7) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x119AA && cp <= 0x119D0) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x119E1) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x119E3) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x11A00) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x11A0B && cp <= 0x11A32) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x11A3A) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x11A50) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x11A5C && cp <= 0x11A89) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x11A9D) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x11AB0 && cp <= 0x11AF8) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x11BC0 && cp <= 0x11BE0) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x11C00 && cp <= 0x11C08) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x11C0A && cp <= 0x11C2E) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x11C40) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x11C72 && cp <= 0x11C8F) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x11D00 && cp <= 0x11D06) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x11D08 && cp <= 0x11D09) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x11D0B && cp <= 0x11D30) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x11D46) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x11D60 && cp <= 0x11D65) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x11D67 && cp <= 0x11D68) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x11D6A && cp <= 0x11D89) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x11D98) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x11EE0 && cp <= 0x11EF2) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x11F02) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x11F04 && cp <= 0x11F10) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x11F12 && cp <= 0x11F33) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x11FB0) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x12000 && cp <= 0x12399) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x12400 && cp <= 0x1246E) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x12480 && cp <= 0x12543) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x12F90 && cp <= 0x12FF0) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x13000 && cp <= 0x1342F) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x13441 && cp <= 0x13446) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x13460 && cp <= 0x143FA) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x14400 && cp <= 0x14646) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x16100 && cp <= 0x1611D) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x16800 && cp <= 0x16A38) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x16A40 && cp <= 0x16A5E) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x16A70 && cp <= 0x16ABE) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x16AD0 && cp <= 0x16AED) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x16B00 && cp <= 0x16B2F) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x16B40 && cp <= 0x16B43) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x16B63 && cp <= 0x16B77) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x16B7D && cp <= 0x16B8F) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x16D40 && cp <= 0x16D42) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x16D43 && cp <= 0x16D6A) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x16D6B && cp <= 0x16D6C) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x16E40 && cp <= 0x16E7F) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x16F00 && cp <= 0x16F4A) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x16F50) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x16F93 && cp <= 0x16F9F) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x16FE0 && cp <= 0x16FE1) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x16FE3) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1BC00 && cp <= 0x1BC6A) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1BC70 && cp <= 0x1BC7C) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1BC80 && cp <= 0x1BC88) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1BC90 && cp <= 0x1BC99) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1D400 && cp <= 0x1D454) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1D456 && cp <= 0x1D49C) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1D49E && cp <= 0x1D49F) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x1D4A2) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1D4A5 && cp <= 0x1D4A6) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1D4A9 && cp <= 0x1D4AC) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1D4AE && cp <= 0x1D4B9) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x1D4BB) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1D4BD && cp <= 0x1D4C3) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1D4C5 && cp <= 0x1D505) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1D507 && cp <= 0x1D50A) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1D50D && cp <= 0x1D514) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1D516 && cp <= 0x1D51C) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1D51E && cp <= 0x1D539) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1D53B && cp <= 0x1D53E) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1D540 && cp <= 0x1D544) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x1D546) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1D54A && cp <= 0x1D550) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1D552 && cp <= 0x1D6A5) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1D6A8 && cp <= 0x1D6C0) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1D6C2 && cp <= 0x1D6DA) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1D6DC && cp <= 0x1D6FA) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1D6FC && cp <= 0x1D714) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1D716 && cp <= 0x1D734) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1D736 && cp <= 0x1D74E) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1D750 && cp <= 0x1D76E) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1D770 && cp <= 0x1D788) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1D78A && cp <= 0x1D7A8) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1D7AA && cp <= 0x1D7C2) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1D7C4 && cp <= 0x1D7CB) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1DF00 && cp <= 0x1DF09) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x1DF0A) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1DF0B && cp <= 0x1DF1E) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1DF25 && cp <= 0x1DF2A) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1E030 && cp <= 0x1E06D) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1E100 && cp <= 0x1E12C) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1E137 && cp <= 0x1E13D) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x1E14E) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1E290 && cp <= 0x1E2AD) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1E2C0 && cp <= 0x1E2EB) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1E4D0 && cp <= 0x1E4EA) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x1E4EB) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1E5D0 && cp <= 0x1E5ED) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x1E5F0) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1E7E0 && cp <= 0x1E7E6) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1E7E8 && cp <= 0x1E7EB) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1E7ED && cp <= 0x1E7EE) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1E7F0 && cp <= 0x1E7FE) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1E800 && cp <= 0x1E8C4) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1E900 && cp <= 0x1E943) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x1E94B) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1EE00 && cp <= 0x1EE03) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1EE05 && cp <= 0x1EE1F) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1EE21 && cp <= 0x1EE22) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x1EE24) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x1EE27) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1EE29 && cp <= 0x1EE32) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1EE34 && cp <= 0x1EE37) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x1EE39) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x1EE3B) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x1EE42) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x1EE47) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x1EE49) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x1EE4B) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1EE4D && cp <= 0x1EE4F) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1EE51 && cp <= 0x1EE52) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x1EE54) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x1EE57) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x1EE59) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x1EE5B) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x1EE5D) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x1EE5F) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1EE61 && cp <= 0x1EE62) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x1EE64) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1EE67 && cp <= 0x1EE6A) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1EE6C && cp <= 0x1EE72) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1EE74 && cp <= 0x1EE77) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1EE79 && cp <= 0x1EE7C) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x1EE7E) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1EE80 && cp <= 0x1EE89) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1EE8B && cp <= 0x1EE9B) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1EEA1 && cp <= 0x1EEA3) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1EEA5 && cp <= 0x1EEA9) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1EEAB && cp <= 0x1EEBB) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1F130 && cp <= 0x1F149) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1F150 && cp <= 0x1F169) {
        return N00B_WB_ALetter;
    }

    if (cp >= 0x1F170 && cp <= 0x1F189) {
        return N00B_WB_ALetter;
    }

    if (cp == 0x003A) {
        return N00B_WB_MidLetter;
    }

    if (cp == 0x00B7) {
        return N00B_WB_MidLetter;
    }

    if (cp == 0x0387) {
        return N00B_WB_MidLetter;
    }

    if (cp == 0x055F) {
        return N00B_WB_MidLetter;
    }

    if (cp == 0x05F4) {
        return N00B_WB_MidLetter;
    }

    if (cp == 0x2027) {
        return N00B_WB_MidLetter;
    }

    if (cp == 0xFE13) {
        return N00B_WB_MidLetter;
    }

    if (cp == 0xFE55) {
        return N00B_WB_MidLetter;
    }

    if (cp == 0xFF1A) {
        return N00B_WB_MidLetter;
    }

    if (cp == 0x002C) {
        return N00B_WB_MidNum;
    }

    if (cp == 0x003B) {
        return N00B_WB_MidNum;
    }

    if (cp == 0x037E) {
        return N00B_WB_MidNum;
    }

    if (cp == 0x0589) {
        return N00B_WB_MidNum;
    }

    if (cp >= 0x060C && cp <= 0x060D) {
        return N00B_WB_MidNum;
    }

    if (cp == 0x066C) {
        return N00B_WB_MidNum;
    }

    if (cp == 0x07F8) {
        return N00B_WB_MidNum;
    }

    if (cp == 0x2044) {
        return N00B_WB_MidNum;
    }

    if (cp == 0xFE50) {
        return N00B_WB_MidNum;
    }

    if (cp == 0xFE54) {
        return N00B_WB_MidNum;
    }

    if (cp == 0xFF0C) {
        return N00B_WB_MidNum;
    }

    if (cp == 0xFF1B) {
        return N00B_WB_MidNum;
    }

    if (cp == 0x002E) {
        return N00B_WB_MidNumLet;
    }

    if (cp == 0x2018) {
        return N00B_WB_MidNumLet;
    }

    if (cp == 0x2019) {
        return N00B_WB_MidNumLet;
    }

    if (cp == 0x2024) {
        return N00B_WB_MidNumLet;
    }

    if (cp == 0xFE52) {
        return N00B_WB_MidNumLet;
    }

    if (cp == 0xFF07) {
        return N00B_WB_MidNumLet;
    }

    if (cp == 0xFF0E) {
        return N00B_WB_MidNumLet;
    }

    if (cp >= 0x0030 && cp <= 0x0039) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x0600 && cp <= 0x0605) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x0660 && cp <= 0x0669) {
        return N00B_WB_Numeric;
    }

    if (cp == 0x066B) {
        return N00B_WB_Numeric;
    }

    if (cp == 0x06DD) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x06F0 && cp <= 0x06F9) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x07C0 && cp <= 0x07C9) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x0890 && cp <= 0x0891) {
        return N00B_WB_Numeric;
    }

    if (cp == 0x08E2) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x0966 && cp <= 0x096F) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x09E6 && cp <= 0x09EF) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x0A66 && cp <= 0x0A6F) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x0AE6 && cp <= 0x0AEF) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x0B66 && cp <= 0x0B6F) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x0BE6 && cp <= 0x0BEF) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x0C66 && cp <= 0x0C6F) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x0CE6 && cp <= 0x0CEF) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x0D66 && cp <= 0x0D6F) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x0DE6 && cp <= 0x0DEF) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x0E50 && cp <= 0x0E59) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x0ED0 && cp <= 0x0ED9) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x0F20 && cp <= 0x0F29) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x1040 && cp <= 0x1049) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x1090 && cp <= 0x1099) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x17E0 && cp <= 0x17E9) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x1810 && cp <= 0x1819) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x1946 && cp <= 0x194F) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x19D0 && cp <= 0x19D9) {
        return N00B_WB_Numeric;
    }

    if (cp == 0x19DA) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x1A80 && cp <= 0x1A89) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x1A90 && cp <= 0x1A99) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x1B50 && cp <= 0x1B59) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x1BB0 && cp <= 0x1BB9) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x1C40 && cp <= 0x1C49) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x1C50 && cp <= 0x1C59) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0xA620 && cp <= 0xA629) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0xA8D0 && cp <= 0xA8D9) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0xA900 && cp <= 0xA909) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0xA9D0 && cp <= 0xA9D9) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0xA9F0 && cp <= 0xA9F9) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0xAA50 && cp <= 0xAA59) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0xABF0 && cp <= 0xABF9) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0xFF10 && cp <= 0xFF19) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x104A0 && cp <= 0x104A9) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x10D30 && cp <= 0x10D39) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x10D40 && cp <= 0x10D49) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x11066 && cp <= 0x1106F) {
        return N00B_WB_Numeric;
    }

    if (cp == 0x110BD) {
        return N00B_WB_Numeric;
    }

    if (cp == 0x110CD) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x110F0 && cp <= 0x110F9) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x11136 && cp <= 0x1113F) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x111D0 && cp <= 0x111D9) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x112F0 && cp <= 0x112F9) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x11450 && cp <= 0x11459) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x114D0 && cp <= 0x114D9) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x11650 && cp <= 0x11659) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x116C0 && cp <= 0x116C9) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x116D0 && cp <= 0x116E3) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x11730 && cp <= 0x11739) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x118E0 && cp <= 0x118E9) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x11950 && cp <= 0x11959) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x11BF0 && cp <= 0x11BF9) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x11C50 && cp <= 0x11C59) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x11D50 && cp <= 0x11D59) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x11DA0 && cp <= 0x11DA9) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x11F50 && cp <= 0x11F59) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x16130 && cp <= 0x16139) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x16A60 && cp <= 0x16A69) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x16AC0 && cp <= 0x16AC9) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x16B50 && cp <= 0x16B59) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x16D70 && cp <= 0x16D79) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x1CCF0 && cp <= 0x1CCF9) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x1D7CE && cp <= 0x1D7FF) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x1E140 && cp <= 0x1E149) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x1E2F0 && cp <= 0x1E2F9) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x1E4F0 && cp <= 0x1E4F9) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x1E5F1 && cp <= 0x1E5FA) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x1E950 && cp <= 0x1E959) {
        return N00B_WB_Numeric;
    }

    if (cp >= 0x1FBF0 && cp <= 0x1FBF9) {
        return N00B_WB_Numeric;
    }

    if (cp == 0x005F) {
        return N00B_WB_ExtendNumLet;
    }

    if (cp == 0x202F) {
        return N00B_WB_ExtendNumLet;
    }

    if (cp >= 0x203F && cp <= 0x2040) {
        return N00B_WB_ExtendNumLet;
    }

    if (cp == 0x2054) {
        return N00B_WB_ExtendNumLet;
    }

    if (cp >= 0xFE33 && cp <= 0xFE34) {
        return N00B_WB_ExtendNumLet;
    }

    if (cp >= 0xFE4D && cp <= 0xFE4F) {
        return N00B_WB_ExtendNumLet;
    }

    if (cp == 0xFF3F) {
        return N00B_WB_ExtendNumLet;
    }

    if (cp == 0x200D) {
        return N00B_WB_ZWJ;
    }

    if (cp == 0x0020) {
        return N00B_WB_WSegSpace;
    }

    if (cp == 0x1680) {
        return N00B_WB_WSegSpace;
    }

    if (cp >= 0x2000 && cp <= 0x2006) {
        return N00B_WB_WSegSpace;
    }

    if (cp >= 0x2008 && cp <= 0x200A) {
        return N00B_WB_WSegSpace;
    }

    if (cp == 0x205F) {
        return N00B_WB_WSegSpace;
    }

    if (cp == 0x3000) {
        return N00B_WB_WSegSpace;
    }

    return N00B_WB_Other;
}
