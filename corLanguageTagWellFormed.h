//
// FILE            corLanguageTagWellFormed.h
//
// AUTHOR          Ken Zangelin
//
// Copyright 2026 Seamware
//
// SPDX-License-Identifier: Apache-2.0
//
#ifndef CORBASE_CORLANGUAGETAGWELLFORMED_H_
#define CORBASE_CORLANGUAGETAGWELLFORMED_H_

#include <stdbool.h>                           // bool



// -----------------------------------------------------------------------------
//
// corLanguageTagWellFormed - is 'tag' a well-formed IETF RFC 5646 (BCP 47) language tag?
//
// SHAPE, not registration: "zz" and "qqq-ZZ" are well-formed even though no such language is registered.
// Validating against the IANA registry would need the registry itself, and would reject tags that become
// valid when it is next updated.
//
//   primary subtag  2-8 ALPHA          en, deu, klingon
//   further subtags 1-8 ALPHANUM each  en-US, zh-Hans-CN, de-CH-1901, und-u-ks-identic (RFC 6067)
//   singletons      x-... / i-...      private-use and grandfathered forms
//
extern bool corLanguageTagWellFormed(const char* tag);

#endif  // CORBASE_CORLANGUAGETAGWELLFORMED_H_
