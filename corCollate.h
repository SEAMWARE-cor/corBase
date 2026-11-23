//
// FILE            corCollate.h
//
// AUTHOR          Ken Zangelin
//
// Copyright 2026 Seamware
//
// SPDX-License-Identifier: Apache-2.0
//
#ifndef CORBASE_CORCOLLATE_H_
#define CORBASE_CORCOLLATE_H_

#include <stdbool.h>                           // bool



// -----------------------------------------------------------------------------
//
// CorCollationTailoring - the languages whose order differs from the root order
//
typedef enum CorCollationTailoring
{
  CorCollationRoot,          // CLDR root: a language-agnostic order (every tag without a tailoring below)
  CorCollationSwedish,       // sv: å, ä (and æ), ö (and ø) are letters of their own, after z
  CorCollationGermanPhonebook  // de-u-co-phonebk: ä, ö, ü sort as ae, oe, ue
} CorCollationTailoring;



// -----------------------------------------------------------------------------
//
// CorCollation - a collation, as corCollationParse reads it from a language tag
//
typedef struct CorCollation
{
  CorCollationTailoring  tailoring;
  bool                   numeric;     // -u-kn: a run of digits sorts by its numeric value (item2 < item10)
} CorCollation;



// -----------------------------------------------------------------------------
//
// corCollationParse - the collation a BCP 47 language tag asks for (RFC 6067's -u- extension included)
//
// NULL or "" is the root order. A language without a tailoring here is the root order too - not an error,
// as ICU falls back to root for a locale it has no data for.
//
//   sv, sv-SE, ...        CorCollationSwedish
//   de-u-co-phonebk       CorCollationGermanPhonebook (co-phonebk on any other language is ignored)
//   -u-kn, -u-kn-true     numeric; -u-kn-false is not
//
extern void corCollationParse(const char* tag, CorCollation* collP);



// -----------------------------------------------------------------------------
//
// corCollate - compare two UTF-8 strings in a collation: <0, 0 or >0, as strcmp
//
// Three levels, after the Unicode Collation Algorithm: the base letters first (case and accents ignored), then
// the accents, then the case (lowercase first). So apple < Apple < äpple < banana. Spaces, punctuation and
// symbols sort before digits, digits before letters. Weights are defined for ASCII, Latin-1 and Latin
// Extended-A; any other character sorts after the Latin letters, by code point. Strings equal at all three
// levels are ordered by their bytes, so the order is total.
//
extern int corCollate(const char* a, const char* b, const CorCollation* collP);

#endif  // CORBASE_CORCOLLATE_H_
