//
// FILE            corCollate.c
//
// AUTHOR          Ken Zangelin
//
// Copyright 2026 Seamware
//
// SPDX-License-Identifier: Apache-2.0
//
#include <stdbool.h>                           // bool
#include <stdint.h>                            // uint32_t, uint8_t
#include <string.h>                            // strcmp, memcmp, strlen
#include <strings.h>                           // strncasecmp

#include "corBase/corCollate.h"                // Own interface



// -----------------------------------------------------------------------------
//
// Primary weights - one number per base letter, in the root order:
//
//   0x0100 + rank    a space, punctuation mark or symbol of ASCII or Latin-1, in symbolRankV's order
//   0x1000 + d       the digit d
//   L(c)             the letter c, 8 apart - room for a letter of its own after it (ı after i, ŋ after n, ...);
//                    after z: þ, then ʼ (of ŉ), then Swedish's å, ä, ö
//   0x10000 + cp     any other character: after the Latin letters, by code point
//
#define DIGIT_PRIMARY   0x1000
#define L(c)            (0x2000 + ((c) - 'a') * 8)
#define OTHER_PRIMARY   0x10000



// -----------------------------------------------------------------------------
//
// LatinWeight - the weights of one Latin-1 / Latin Extended-A letter
//
// secondary - the accent, in ICU 78's root order (DUCET), two apart:
//   2 acute, 4 grave, 6 breve, 8 circumflex, 10 caron, 12 ring, 14 diaeresis, 16 double acute, 18 tilde,
//   20 dot above, 21 the stroke of ø, 22 cedilla, 24 ogonek, 26 macron, 28 the stroke of đ ħ ł, 30 middle dot,
//   32 other (ð, ſ, ŉ - and æ, œ, ß against ae, oe, ss)
//
typedef struct LatinWeight
{
  uint32_t  primary;      // 0: not a letter (× ÷) - a symbol
  uint32_t  expansion;    // a second primary - ß is ss, œ is oe, ĳ is ij; 0: none
  uint8_t   secondary;
  uint8_t   upper;        // the tertiary level: 1 for an uppercase letter
} LatinWeight;



// -----------------------------------------------------------------------------
//
// latinV - U+00C0 to U+017F, generated from Unicode's canonical decompositions (a letter and one combining
// accent); the letters with no decomposition (æ, ð, ø, þ, ß, đ, ħ, ı, ĳ, ĸ, ŀ, ł, ŉ, ŋ, œ, ŧ, ſ) by hand,
// where ICU 78's root collation puts them.
//
static const LatinWeight latinV[] =
{
  { L('a'),        0,        4, 1 },   // U+00C0 À Latin Capital Letter A With Grave
  { L('a'),        0,        2, 1 },   // U+00C1 Á Latin Capital Letter A With Acute
  { L('a'),        0,        8, 1 },   // U+00C2 Â Latin Capital Letter A With Circumflex
  { L('a'),        0,       18, 1 },   // U+00C3 Ã Latin Capital Letter A With Tilde
  { L('a'),        0,       14, 1 },   // U+00C4 Ä Latin Capital Letter A With Diaeresis
  { L('a'),        0,       12, 1 },   // U+00C5 Å Latin Capital Letter A With Ring Above
  { L('a'),        L('e'),  32, 1 },   // U+00C6 Æ Latin Capital Letter Ae
  { L('c'),        0,       22, 1 },   // U+00C7 Ç Latin Capital Letter C With Cedilla
  { L('e'),        0,        4, 1 },   // U+00C8 È Latin Capital Letter E With Grave
  { L('e'),        0,        2, 1 },   // U+00C9 É Latin Capital Letter E With Acute
  { L('e'),        0,        8, 1 },   // U+00CA Ê Latin Capital Letter E With Circumflex
  { L('e'),        0,       14, 1 },   // U+00CB Ë Latin Capital Letter E With Diaeresis
  { L('i'),        0,        4, 1 },   // U+00CC Ì Latin Capital Letter I With Grave
  { L('i'),        0,        2, 1 },   // U+00CD Í Latin Capital Letter I With Acute
  { L('i'),        0,        8, 1 },   // U+00CE Î Latin Capital Letter I With Circumflex
  { L('i'),        0,       14, 1 },   // U+00CF Ï Latin Capital Letter I With Diaeresis
  { L('d'),        0,       32, 1 },   // U+00D0 Ð Latin Capital Letter Eth
  { L('n'),        0,       18, 1 },   // U+00D1 Ñ Latin Capital Letter N With Tilde
  { L('o'),        0,        4, 1 },   // U+00D2 Ò Latin Capital Letter O With Grave
  { L('o'),        0,        2, 1 },   // U+00D3 Ó Latin Capital Letter O With Acute
  { L('o'),        0,        8, 1 },   // U+00D4 Ô Latin Capital Letter O With Circumflex
  { L('o'),        0,       18, 1 },   // U+00D5 Õ Latin Capital Letter O With Tilde
  { L('o'),        0,       14, 1 },   // U+00D6 Ö Latin Capital Letter O With Diaeresis
  { 0,             0,        0, 0 },   // U+00D7   Multiplication Sign
  { L('o'),        0,       21, 1 },   // U+00D8 Ø Latin Capital Letter O With Stroke
  { L('u'),        0,        4, 1 },   // U+00D9 Ù Latin Capital Letter U With Grave
  { L('u'),        0,        2, 1 },   // U+00DA Ú Latin Capital Letter U With Acute
  { L('u'),        0,        8, 1 },   // U+00DB Û Latin Capital Letter U With Circumflex
  { L('u'),        0,       14, 1 },   // U+00DC Ü Latin Capital Letter U With Diaeresis
  { L('y'),        0,        2, 1 },   // U+00DD Ý Latin Capital Letter Y With Acute
  { L('z') + 1,    0,        0, 1 },   // U+00DE Þ Latin Capital Letter Thorn
  { L('s'),        L('s'),  32, 0 },   // U+00DF ß Latin Small Letter Sharp S
  { L('a'),        0,        4, 0 },   // U+00E0 à Latin Small Letter A With Grave
  { L('a'),        0,        2, 0 },   // U+00E1 á Latin Small Letter A With Acute
  { L('a'),        0,        8, 0 },   // U+00E2 â Latin Small Letter A With Circumflex
  { L('a'),        0,       18, 0 },   // U+00E3 ã Latin Small Letter A With Tilde
  { L('a'),        0,       14, 0 },   // U+00E4 ä Latin Small Letter A With Diaeresis
  { L('a'),        0,       12, 0 },   // U+00E5 å Latin Small Letter A With Ring Above
  { L('a'),        L('e'),  32, 0 },   // U+00E6 æ Latin Small Letter Ae
  { L('c'),        0,       22, 0 },   // U+00E7 ç Latin Small Letter C With Cedilla
  { L('e'),        0,        4, 0 },   // U+00E8 è Latin Small Letter E With Grave
  { L('e'),        0,        2, 0 },   // U+00E9 é Latin Small Letter E With Acute
  { L('e'),        0,        8, 0 },   // U+00EA ê Latin Small Letter E With Circumflex
  { L('e'),        0,       14, 0 },   // U+00EB ë Latin Small Letter E With Diaeresis
  { L('i'),        0,        4, 0 },   // U+00EC ì Latin Small Letter I With Grave
  { L('i'),        0,        2, 0 },   // U+00ED í Latin Small Letter I With Acute
  { L('i'),        0,        8, 0 },   // U+00EE î Latin Small Letter I With Circumflex
  { L('i'),        0,       14, 0 },   // U+00EF ï Latin Small Letter I With Diaeresis
  { L('d'),        0,       32, 0 },   // U+00F0 ð Latin Small Letter Eth
  { L('n'),        0,       18, 0 },   // U+00F1 ñ Latin Small Letter N With Tilde
  { L('o'),        0,        4, 0 },   // U+00F2 ò Latin Small Letter O With Grave
  { L('o'),        0,        2, 0 },   // U+00F3 ó Latin Small Letter O With Acute
  { L('o'),        0,        8, 0 },   // U+00F4 ô Latin Small Letter O With Circumflex
  { L('o'),        0,       18, 0 },   // U+00F5 õ Latin Small Letter O With Tilde
  { L('o'),        0,       14, 0 },   // U+00F6 ö Latin Small Letter O With Diaeresis
  { 0,             0,        0, 0 },   // U+00F7   Division Sign
  { L('o'),        0,       21, 0 },   // U+00F8 ø Latin Small Letter O With Stroke
  { L('u'),        0,        4, 0 },   // U+00F9 ù Latin Small Letter U With Grave
  { L('u'),        0,        2, 0 },   // U+00FA ú Latin Small Letter U With Acute
  { L('u'),        0,        8, 0 },   // U+00FB û Latin Small Letter U With Circumflex
  { L('u'),        0,       14, 0 },   // U+00FC ü Latin Small Letter U With Diaeresis
  { L('y'),        0,        2, 0 },   // U+00FD ý Latin Small Letter Y With Acute
  { L('z') + 1,    0,        0, 0 },   // U+00FE þ Latin Small Letter Thorn
  { L('y'),        0,       14, 0 },   // U+00FF ÿ Latin Small Letter Y With Diaeresis
  { L('a'),        0,       26, 1 },   // U+0100 Ā Latin Capital Letter A With Macron
  { L('a'),        0,       26, 0 },   // U+0101 ā Latin Small Letter A With Macron
  { L('a'),        0,        6, 1 },   // U+0102 Ă Latin Capital Letter A With Breve
  { L('a'),        0,        6, 0 },   // U+0103 ă Latin Small Letter A With Breve
  { L('a'),        0,       24, 1 },   // U+0104 Ą Latin Capital Letter A With Ogonek
  { L('a'),        0,       24, 0 },   // U+0105 ą Latin Small Letter A With Ogonek
  { L('c'),        0,        2, 1 },   // U+0106 Ć Latin Capital Letter C With Acute
  { L('c'),        0,        2, 0 },   // U+0107 ć Latin Small Letter C With Acute
  { L('c'),        0,        8, 1 },   // U+0108 Ĉ Latin Capital Letter C With Circumflex
  { L('c'),        0,        8, 0 },   // U+0109 ĉ Latin Small Letter C With Circumflex
  { L('c'),        0,       20, 1 },   // U+010A Ċ Latin Capital Letter C With Dot Above
  { L('c'),        0,       20, 0 },   // U+010B ċ Latin Small Letter C With Dot Above
  { L('c'),        0,       10, 1 },   // U+010C Č Latin Capital Letter C With Caron
  { L('c'),        0,       10, 0 },   // U+010D č Latin Small Letter C With Caron
  { L('d'),        0,       10, 1 },   // U+010E Ď Latin Capital Letter D With Caron
  { L('d'),        0,       10, 0 },   // U+010F ď Latin Small Letter D With Caron
  { L('d'),        0,       28, 1 },   // U+0110 Đ Latin Capital Letter D With Stroke
  { L('d'),        0,       28, 0 },   // U+0111 đ Latin Small Letter D With Stroke
  { L('e'),        0,       26, 1 },   // U+0112 Ē Latin Capital Letter E With Macron
  { L('e'),        0,       26, 0 },   // U+0113 ē Latin Small Letter E With Macron
  { L('e'),        0,        6, 1 },   // U+0114 Ĕ Latin Capital Letter E With Breve
  { L('e'),        0,        6, 0 },   // U+0115 ĕ Latin Small Letter E With Breve
  { L('e'),        0,       20, 1 },   // U+0116 Ė Latin Capital Letter E With Dot Above
  { L('e'),        0,       20, 0 },   // U+0117 ė Latin Small Letter E With Dot Above
  { L('e'),        0,       24, 1 },   // U+0118 Ę Latin Capital Letter E With Ogonek
  { L('e'),        0,       24, 0 },   // U+0119 ę Latin Small Letter E With Ogonek
  { L('e'),        0,       10, 1 },   // U+011A Ě Latin Capital Letter E With Caron
  { L('e'),        0,       10, 0 },   // U+011B ě Latin Small Letter E With Caron
  { L('g'),        0,        8, 1 },   // U+011C Ĝ Latin Capital Letter G With Circumflex
  { L('g'),        0,        8, 0 },   // U+011D ĝ Latin Small Letter G With Circumflex
  { L('g'),        0,        6, 1 },   // U+011E Ğ Latin Capital Letter G With Breve
  { L('g'),        0,        6, 0 },   // U+011F ğ Latin Small Letter G With Breve
  { L('g'),        0,       20, 1 },   // U+0120 Ġ Latin Capital Letter G With Dot Above
  { L('g'),        0,       20, 0 },   // U+0121 ġ Latin Small Letter G With Dot Above
  { L('g'),        0,       22, 1 },   // U+0122 Ģ Latin Capital Letter G With Cedilla
  { L('g'),        0,       22, 0 },   // U+0123 ģ Latin Small Letter G With Cedilla
  { L('h'),        0,        8, 1 },   // U+0124 Ĥ Latin Capital Letter H With Circumflex
  { L('h'),        0,        8, 0 },   // U+0125 ĥ Latin Small Letter H With Circumflex
  { L('h'),        0,       28, 1 },   // U+0126 Ħ Latin Capital Letter H With Stroke
  { L('h'),        0,       28, 0 },   // U+0127 ħ Latin Small Letter H With Stroke
  { L('i'),        0,       18, 1 },   // U+0128 Ĩ Latin Capital Letter I With Tilde
  { L('i'),        0,       18, 0 },   // U+0129 ĩ Latin Small Letter I With Tilde
  { L('i'),        0,       26, 1 },   // U+012A Ī Latin Capital Letter I With Macron
  { L('i'),        0,       26, 0 },   // U+012B ī Latin Small Letter I With Macron
  { L('i'),        0,        6, 1 },   // U+012C Ĭ Latin Capital Letter I With Breve
  { L('i'),        0,        6, 0 },   // U+012D ĭ Latin Small Letter I With Breve
  { L('i'),        0,       24, 1 },   // U+012E Į Latin Capital Letter I With Ogonek
  { L('i'),        0,       24, 0 },   // U+012F į Latin Small Letter I With Ogonek
  { L('i'),        0,       20, 1 },   // U+0130 İ Latin Capital Letter I With Dot Above
  { L('i') + 1,    0,        0, 0 },   // U+0131 ı Latin Small Letter Dotless I
  { L('i'),        L('j'),   0, 1 },   // U+0132 Ĳ Latin Capital Ligature Ij
  { L('i'),        L('j'),   0, 0 },   // U+0133 ĳ Latin Small Ligature Ij
  { L('j'),        0,        8, 1 },   // U+0134 Ĵ Latin Capital Letter J With Circumflex
  { L('j'),        0,        8, 0 },   // U+0135 ĵ Latin Small Letter J With Circumflex
  { L('k'),        0,       22, 1 },   // U+0136 Ķ Latin Capital Letter K With Cedilla
  { L('k'),        0,       22, 0 },   // U+0137 ķ Latin Small Letter K With Cedilla
  { L('q') + 1,    0,        0, 0 },   // U+0138 ĸ Latin Small Letter Kra
  { L('l'),        0,        2, 1 },   // U+0139 Ĺ Latin Capital Letter L With Acute
  { L('l'),        0,        2, 0 },   // U+013A ĺ Latin Small Letter L With Acute
  { L('l'),        0,       22, 1 },   // U+013B Ļ Latin Capital Letter L With Cedilla
  { L('l'),        0,       22, 0 },   // U+013C ļ Latin Small Letter L With Cedilla
  { L('l'),        0,       10, 1 },   // U+013D Ľ Latin Capital Letter L With Caron
  { L('l'),        0,       10, 0 },   // U+013E ľ Latin Small Letter L With Caron
  { L('l'),        0,       30, 1 },   // U+013F Ŀ Latin Capital Letter L With Middle Dot
  { L('l'),        0,       30, 0 },   // U+0140 ŀ Latin Small Letter L With Middle Dot
  { L('l'),        0,       28, 1 },   // U+0141 Ł Latin Capital Letter L With Stroke
  { L('l'),        0,       28, 0 },   // U+0142 ł Latin Small Letter L With Stroke
  { L('n'),        0,        2, 1 },   // U+0143 Ń Latin Capital Letter N With Acute
  { L('n'),        0,        2, 0 },   // U+0144 ń Latin Small Letter N With Acute
  { L('n'),        0,       22, 1 },   // U+0145 Ņ Latin Capital Letter N With Cedilla
  { L('n'),        0,       22, 0 },   // U+0146 ņ Latin Small Letter N With Cedilla
  { L('n'),        0,       10, 1 },   // U+0147 Ň Latin Capital Letter N With Caron
  { L('n'),        0,       10, 0 },   // U+0148 ň Latin Small Letter N With Caron
  { L('z') + 2,    L('n'),   0, 0 },   // U+0149 ŉ Latin Small Letter N Preceded By Apostrophe
  { L('n') + 1,    0,        0, 1 },   // U+014A Ŋ Latin Capital Letter Eng
  { L('n') + 1,    0,        0, 0 },   // U+014B ŋ Latin Small Letter Eng
  { L('o'),        0,       26, 1 },   // U+014C Ō Latin Capital Letter O With Macron
  { L('o'),        0,       26, 0 },   // U+014D ō Latin Small Letter O With Macron
  { L('o'),        0,        6, 1 },   // U+014E Ŏ Latin Capital Letter O With Breve
  { L('o'),        0,        6, 0 },   // U+014F ŏ Latin Small Letter O With Breve
  { L('o'),        0,       16, 1 },   // U+0150 Ő Latin Capital Letter O With Double Acute
  { L('o'),        0,       16, 0 },   // U+0151 ő Latin Small Letter O With Double Acute
  { L('o'),        L('e'),  32, 1 },   // U+0152 Œ Latin Capital Ligature Oe
  { L('o'),        L('e'),  32, 0 },   // U+0153 œ Latin Small Ligature Oe
  { L('r'),        0,        2, 1 },   // U+0154 Ŕ Latin Capital Letter R With Acute
  { L('r'),        0,        2, 0 },   // U+0155 ŕ Latin Small Letter R With Acute
  { L('r'),        0,       22, 1 },   // U+0156 Ŗ Latin Capital Letter R With Cedilla
  { L('r'),        0,       22, 0 },   // U+0157 ŗ Latin Small Letter R With Cedilla
  { L('r'),        0,       10, 1 },   // U+0158 Ř Latin Capital Letter R With Caron
  { L('r'),        0,       10, 0 },   // U+0159 ř Latin Small Letter R With Caron
  { L('s'),        0,        2, 1 },   // U+015A Ś Latin Capital Letter S With Acute
  { L('s'),        0,        2, 0 },   // U+015B ś Latin Small Letter S With Acute
  { L('s'),        0,        8, 1 },   // U+015C Ŝ Latin Capital Letter S With Circumflex
  { L('s'),        0,        8, 0 },   // U+015D ŝ Latin Small Letter S With Circumflex
  { L('s'),        0,       22, 1 },   // U+015E Ş Latin Capital Letter S With Cedilla
  { L('s'),        0,       22, 0 },   // U+015F ş Latin Small Letter S With Cedilla
  { L('s'),        0,       10, 1 },   // U+0160 Š Latin Capital Letter S With Caron
  { L('s'),        0,       10, 0 },   // U+0161 š Latin Small Letter S With Caron
  { L('t'),        0,       22, 1 },   // U+0162 Ţ Latin Capital Letter T With Cedilla
  { L('t'),        0,       22, 0 },   // U+0163 ţ Latin Small Letter T With Cedilla
  { L('t'),        0,       10, 1 },   // U+0164 Ť Latin Capital Letter T With Caron
  { L('t'),        0,       10, 0 },   // U+0165 ť Latin Small Letter T With Caron
  { L('t') + 1,    0,        0, 1 },   // U+0166 Ŧ Latin Capital Letter T With Stroke
  { L('t') + 1,    0,        0, 0 },   // U+0167 ŧ Latin Small Letter T With Stroke
  { L('u'),        0,       18, 1 },   // U+0168 Ũ Latin Capital Letter U With Tilde
  { L('u'),        0,       18, 0 },   // U+0169 ũ Latin Small Letter U With Tilde
  { L('u'),        0,       26, 1 },   // U+016A Ū Latin Capital Letter U With Macron
  { L('u'),        0,       26, 0 },   // U+016B ū Latin Small Letter U With Macron
  { L('u'),        0,        6, 1 },   // U+016C Ŭ Latin Capital Letter U With Breve
  { L('u'),        0,        6, 0 },   // U+016D ŭ Latin Small Letter U With Breve
  { L('u'),        0,       12, 1 },   // U+016E Ů Latin Capital Letter U With Ring Above
  { L('u'),        0,       12, 0 },   // U+016F ů Latin Small Letter U With Ring Above
  { L('u'),        0,       16, 1 },   // U+0170 Ű Latin Capital Letter U With Double Acute
  { L('u'),        0,       16, 0 },   // U+0171 ű Latin Small Letter U With Double Acute
  { L('u'),        0,       24, 1 },   // U+0172 Ų Latin Capital Letter U With Ogonek
  { L('u'),        0,       24, 0 },   // U+0173 ų Latin Small Letter U With Ogonek
  { L('w'),        0,        8, 1 },   // U+0174 Ŵ Latin Capital Letter W With Circumflex
  { L('w'),        0,        8, 0 },   // U+0175 ŵ Latin Small Letter W With Circumflex
  { L('y'),        0,        8, 1 },   // U+0176 Ŷ Latin Capital Letter Y With Circumflex
  { L('y'),        0,        8, 0 },   // U+0177 ŷ Latin Small Letter Y With Circumflex
  { L('y'),        0,       14, 1 },   // U+0178 Ÿ Latin Capital Letter Y With Diaeresis
  { L('z'),        0,        2, 1 },   // U+0179 Ź Latin Capital Letter Z With Acute
  { L('z'),        0,        2, 0 },   // U+017A ź Latin Small Letter Z With Acute
  { L('z'),        0,       20, 1 },   // U+017B Ż Latin Capital Letter Z With Dot Above
  { L('z'),        0,       20, 0 },   // U+017C ż Latin Small Letter Z With Dot Above
  { L('z'),        0,       10, 1 },   // U+017D Ž Latin Capital Letter Z With Caron
  { L('z'),        0,       10, 0 },   // U+017E ž Latin Small Letter Z With Caron
  { L('s'),        0,       32, 0 },   // U+017F ſ Latin Small Letter Long S
};


// -----------------------------------------------------------------------------
//
// symbolRankV - the order of the spaces, punctuation marks and symbols of ASCII and Latin-1 (U+0000-U+00FF),
// as ICU 78's root collation orders them (DUCET): tab, LF, VT, FF, CR, space, _ - , ; : ! ¡ ? ¿ . · ' " « » ( ) [ ] { } § ¶ @ * /
// backslash & # % ` ´ ^ ¯ ¨ ¸ ° © ® + ± ÷ × < = > ¬ | ¦ ~ ⁄ ¤ ¢ $ £ ¥. A no-break space is a space; the fraction slash ⁄ (U+2044, in ¼ ½ ¾) has the rank
// after ~ and no entry. 0: not one of them.
//
static const uint8_t symbolRankV[256] =
{
   0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  2,  3,  4,  5,  0,  0,   // 0x00
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,   // 0x10
   6, 12, 19, 35, 60, 36, 34, 18, 22, 23, 31, 46,  9,  8, 16, 32,   // 0x20
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 11, 10, 50, 51, 52, 14,   // 0x30
  30,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,   // 0x40
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 24, 33, 25, 39,  7,   // 0x50
  37,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,   // 0x60
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 26, 54, 27, 56,  0,   // 0x70
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,   // 0x80
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,   // 0x90
   6, 13, 59, 61, 58, 62, 55, 28, 41, 44,  0, 20, 53,  0, 45, 40,   // 0xA0
  43, 47,  0,  0, 38,  0, 29, 17, 42,  0,  0, 21,  0,  0,  0, 15,   // 0xB0
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,   // 0xC0
   0,  0,  0,  0,  0,  0,  0, 49,  0,  0,  0,  0,  0,  0,  0,  0,   // 0xD0
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,   // 0xE0
   0,  0,  0,  0,  0,  0,  0, 48,  0,  0,  0,  0,  0,  0,  0,  0,   // 0xF0
};




// -----------------------------------------------------------------------------
//
// CollElem - one collation element: the weights of one character, or of one run of digits under -u-kn
//
typedef struct CollElem
{
  uint32_t     primary;
  uint8_t      secondary;
  uint8_t      tertiary;
  const char*  digits;      // -u-kn: the run of digits this element stands for; NULL otherwise
  int          digitsLen;
} CollElem;



// -----------------------------------------------------------------------------
//
// CollIter - the collation elements of a string, one at a time
//
typedef struct CollIter
{
  const unsigned char*  s;
  const CorCollation*   collP;
  int                   pendingN;  // an expansion's further elements, waiting in nextV
  CollElem              nextV[2];
} CollIter;



// -----------------------------------------------------------------------------
//
// utf8Next - the code point at *sP, and *sP moved past it; a byte that starts no valid sequence is itself
//
static uint32_t utf8Next(const unsigned char** sP)
{
  const unsigned char* s = *sP;
  uint32_t             c = s[0];
  int                  n = 0;

  if      ((c & 0xE0) == 0xC0) { c &= 0x1F; n = 1; }
  else if ((c & 0xF0) == 0xE0) { c &= 0x0F; n = 2; }
  else if ((c & 0xF8) == 0xF0) { c &= 0x07; n = 3; }

  for (int i = 1; i <= n; i++)
  {
    if ((s[i] & 0xC0) != 0x80)   // truncated or malformed - the lead byte alone
    {
      *sP = s + 1;
      return s[0];
    }
    c = (c << 6) | (s[i] & 0x3F);
  }

  *sP = s + 1 + n;
  return c;
}



// -----------------------------------------------------------------------------
//
// collNext - the next collation element of the string; false at its end
//
// The other control characters and the soft hyphen are ignorable: they have no element at all.
//
static bool collNext(CollIter* itP, CollElem* eP)
{
  if (itP->pendingN > 0)
  {
    *eP = itP->nextV[0];
    itP->nextV[0] = itP->nextV[1];
    itP->pendingN--;
    return true;
  }

  while (true)
  {
    if (*itP->s == 0)
      return false;

    eP->secondary = 0;
    eP->tertiary  = 0;
    eP->digits    = NULL;
    eP->digitsLen = 0;

    unsigned char c0 = *itP->s;

    if ((c0 >= '0') && (c0 <= '9'))
    {
      if (itP->collP->numeric == true)   // the whole run, compared by value in levelCompare
      {
        const char* start = (const char*) itP->s;
        while ((*itP->s >= '0') && (*itP->s <= '9'))
          itP->s++;
        eP->primary   = DIGIT_PRIMARY;
        eP->digits    = start;
        eP->digitsLen = (int) ((const char*) itP->s - start);
        return true;
      }

      eP->primary = DIGIT_PRIMARY + (c0 - '0');
      itP->s++;
      return true;
    }

    uint32_t cp = utf8Next(&itP->s);

    if ((cp >= 'a') && (cp <= 'z'))      { eP->primary = L(cp);                  return true; }
    if ((cp >= 'A') && (cp <= 'Z'))      { eP->primary = L(cp - 'A' + 'a');      eP->tertiary = 1; return true; }
    if (cp > 0x17F)                      { eP->primary = OTHER_PRIMARY + cp;     return true; }
    if (cp < 0xC0)
    {
      if (symbolRankV[cp] != 0)          { eP->primary = 0x100 + symbolRankV[cp]; eP->tertiary = (cp == 0xA0); return true; }
      if ((cp < 0x20) || ((cp >= 0x7F) && (cp < 0xA0)) || (cp == 0xAD))      continue;   // ignorable

      switch (cp)                        // the rest of U+00A0-U+00BF: digits and letters in disguise
      {
      case 0xB9: eP->primary = DIGIT_PRIMARY + 1;  eP->tertiary = 1; return true;   // ¹
      case 0xB2: eP->primary = DIGIT_PRIMARY + 2;  eP->tertiary = 1; return true;   // ²
      case 0xB3: eP->primary = DIGIT_PRIMARY + 3;  eP->tertiary = 1; return true;   // ³
      case 0xBC:   // ¼: 1 ⁄ 4
        eP->primary   = DIGIT_PRIMARY + 1;
        eP->tertiary  = 1;
        itP->nextV[0] = (CollElem) { 0x100 + symbolRankV['~'] + 1, 0, 1, NULL, 0 };
        itP->nextV[1] = (CollElem) { DIGIT_PRIMARY + 4, 0, 1, NULL, 0 };
        itP->pendingN = 2;
        return true;
      case 0xBD:   // ½: 1 ⁄ 2
        eP->primary   = DIGIT_PRIMARY + 1;
        eP->tertiary  = 1;
        itP->nextV[0] = (CollElem) { 0x100 + symbolRankV['~'] + 1, 0, 1, NULL, 0 };
        itP->nextV[1] = (CollElem) { DIGIT_PRIMARY + 2, 0, 1, NULL, 0 };
        itP->pendingN = 2;
        return true;
      case 0xBE:   // ¾: 3 ⁄ 4
        eP->primary   = DIGIT_PRIMARY + 3;
        eP->tertiary  = 1;
        itP->nextV[0] = (CollElem) { 0x100 + symbolRankV['~'] + 1, 0, 1, NULL, 0 };
        itP->nextV[1] = (CollElem) { DIGIT_PRIMARY + 4, 0, 1, NULL, 0 };
        itP->pendingN = 2;
        return true;
      case 0xAA: eP->primary = L('a');             eP->tertiary = 2; return true;   // ª
      case 0xBA: eP->primary = L('o');             eP->tertiary = 2; return true;   // º
      case 0xB5: eP->primary = OTHER_PRIMARY + 0x3BC;               return true;   // µ is the Greek μ
      default:   eP->primary = OTHER_PRIMARY + cp;                   return true;
      }
    }

    const LatinWeight* wP = &latinV[cp - 0xC0];

    if (wP->primary == 0)                // × ÷ - symbols
    {
      eP->primary = 0x100 + symbolRankV[cp];
      return true;
    }

    eP->primary   = wP->primary;
    eP->secondary = wP->secondary;
    eP->tertiary  = wP->upper;

    if (itP->collP->tailoring == CorCollationSwedish)
    {
      //
      // CLDR sv: å, ä, ö are three letters after z (and after þ and ʼ) - ä with æ and ę as its variants, ö with ø, ő, œ and ô (in that
      // order, at the second level); ü and ű are a y, and þ is th
      //
      switch (cp)
      {
      case 0xC5: case 0xE5:                              eP->primary = L('z') + 3; eP->secondary = 0; break;   // å
      case 0xC4: case 0xE4:                              eP->primary = L('z') + 4; eP->secondary = 0; break;   // ä
      case 0xC6: case 0xE6:                              eP->primary = L('z') + 4; eP->secondary = 1; break;   // æ
      case 0x118: case 0x119:                            eP->primary = L('z') + 4; eP->secondary = 2; break;   // ę
      case 0xD6: case 0xF6:                              eP->primary = L('z') + 5; eP->secondary = 0; break;   // ö
      case 0xD8: case 0xF8:                              eP->primary = L('z') + 5; eP->secondary = 1; break;   // ø
      case 0x150: case 0x151:                            eP->primary = L('z') + 5; eP->secondary = 2; break;   // ő
      case 0x152: case 0x153:                            eP->primary = L('z') + 5; eP->secondary = 3; break;   // œ
      case 0xD4: case 0xF4:                              eP->primary = L('z') + 5; eP->secondary = 4; break;   // ô
      case 0xDC: case 0xFC:                              eP->primary = L('y');     eP->secondary = 40; break;   // ü - after every accented y
      case 0x170: case 0x171:                            eP->primary = L('y');     eP->secondary = 41; break;   // ű
      case 0xDE: case 0xFE:                                                                                    // þ
        eP->primary   = L('t');                // CLDR: &t<<<þ/h - a t, apart only at the third level
        eP->secondary = 0;
        eP->tertiary  = 2 + wP->upper;
        itP->nextV[0] = (CollElem) { L('h'), 0, 0, NULL, 0 }; itP->pendingN = 1;
        return true;
      }
    }
    else if (itP->collP->tailoring == CorCollationGermanPhonebook)
    {
      //
      // CLDR de-u-co-phonebk: &AE<<ä - ä, ö, ü are ae, oe, ue, a second-level step after them: the step is on the e,
      // so "ae" < "ä" < "àe"
      //
      if ((cp == 0xC4) || (cp == 0xE4) || (cp == 0xD6) || (cp == 0xF6) || (cp == 0xDC) || (cp == 0xFC))
      {
        eP->secondary = 0;
        itP->nextV[0] = (CollElem) { L('e'), 1, 0, NULL, 0 };
        itP->pendingN = 1;
        return true;
      }
    }

    if ((wP->expansion != 0) && (eP->primary == wP->primary))   // a tailoring that made it a letter of its own: no expansion
    {
      itP->nextV[0] = (CollElem) { wP->expansion, eP->secondary, eP->tertiary, NULL, 0 }; itP->pendingN = 1;
    }

    return true;
  }
}



// -----------------------------------------------------------------------------
//
// digitsCompare - two runs of digits by their numeric value; leading zeros do not count
//
static int digitsCompare(const char* a, int aLen, const char* b, int bLen)
{
  while ((aLen > 1) && (*a == '0')) { a++; aLen--; }
  while ((bLen > 1) && (*b == '0')) { b++; bLen--; }

  if (aLen != bLen)
    return (aLen < bLen) ? -1 : 1;

  return memcmp(a, b, aLen);
}



// -----------------------------------------------------------------------------
//
// levelCompare - the two strings at one level: 1 primary, 2 secondary, 3 tertiary
//
static int levelCompare(const char* a, const char* b, const CorCollation* collP, int level)
{
  CollIter  ia = { (const unsigned char*) a, collP, 0, { { 0, 0, 0, NULL, 0 } } };
  CollIter  ib = { (const unsigned char*) b, collP, 0, { { 0, 0, 0, NULL, 0 } } };
  CollElem  ea;
  CollElem  eb;

  while (true)
  {
    bool moreA = collNext(&ia, &ea);
    bool moreB = collNext(&ib, &eb);

    if ((moreA == false) || (moreB == false))
      return (moreA == moreB) ? 0 : (moreA == false) ? -1 : 1;   // a prefix sorts first

    if (level == 1)
    {
      if (ea.primary != eb.primary)
        return (ea.primary < eb.primary) ? -1 : 1;

      if ((ea.digits != NULL) && (eb.digits != NULL))
      {
        int c = digitsCompare(ea.digits, ea.digitsLen, eb.digits, eb.digitsLen);
        if (c != 0)
          return c;
      }
    }
    else if (level == 2)
    {
      if (ea.secondary != eb.secondary)
        return (ea.secondary < eb.secondary) ? -1 : 1;
    }
    else if (ea.tertiary != eb.tertiary)
      return (ea.tertiary < eb.tertiary) ? -1 : 1;
  }
}



// -----------------------------------------------------------------------------
//
// corCollate -
//
int corCollate(const char* a, const char* b, const CorCollation* collP)
{
  for (int level = 1; level <= 3; level++)
  {
    int c = levelCompare(a, b, collP, level);
    if (c != 0)
      return c;
  }

  return strcmp(a, b);
}



// -----------------------------------------------------------------------------
//
// subtagIs - is the subtag at 'tag' (len characters) 'name', ignoring case?
//
static bool subtagIs(const char* tag, int len, const char* name)
{
  return ((int) strlen(name) == len) && (strncasecmp(tag, name, len) == 0);
}



// -----------------------------------------------------------------------------
//
// corCollationParse -
//
void corCollationParse(const char* tag, CorCollation* collP)
{
  collP->tailoring = CorCollationRoot;
  collP->numeric   = false;

  if ((tag == NULL) || (*tag == 0))
    return;

  bool         german     = false;
  bool         phonebook  = false;
  bool         inU        = false;     // inside the -u- extension
  const char*  key        = NULL;      // the -u- key the types that follow belong to
  int          keyLen     = 0;
  int          ix         = 0;
  const char*  p          = tag;

  while (*p != 0)
  {
    int len = 0;
    while ((p[len] != 0) && (p[len] != '-'))
      len++;

    if (ix == 0)
    {
      if      (subtagIs(p, len, "sv")) collP->tailoring = CorCollationSwedish;
      else if (subtagIs(p, len, "de")) german = true;
    }
    else if (len == 1)                 // a singleton: -u- opens the Unicode extension, any other closes it
    {
      inU = ((*p == 'u') || (*p == 'U'));
      key = NULL;
    }
    else if (inU == true)
    {
      if (len == 2)                    // a key - kn alone means true
      {
        key    = p;
        keyLen = len;
        if (subtagIs(key, keyLen, "kn"))
          collP->numeric = true;
      }
      else if (key != NULL)            // a type of the key before it
      {
        if (subtagIs(key, keyLen, "kn"))
          collP->numeric = !subtagIs(p, len, "false");
        else if (subtagIs(key, keyLen, "co") && subtagIs(p, len, "phonebk"))
          phonebook = true;
      }
    }

    p += len;
    if (*p == '-')
      p++;
    ix++;
  }

  if ((german == true) && (phonebook == true))
    collP->tailoring = CorCollationGermanPhonebook;
}
