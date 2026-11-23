//
// FILE            corLanguageTagWellFormed.c
//
// AUTHOR          Ken Zangelin
//
// Copyright 2026 Seamware
//
// SPDX-License-Identifier: Apache-2.0
//
#include <stdbool.h>                           // bool
#include <ctype.h>                             // isalpha, isalnum

#include "corBase/corLanguageTagWellFormed.h"  // Own interface



// -----------------------------------------------------------------------------
//
// corLanguageTagWellFormed -
//
bool corLanguageTagWellFormed(const char* tag)
{
  int i = 0;

  // Private-use (x-) and grandfathered (i-) tags start with a one-letter singleton.
  if (((tag[0] == 'x') || (tag[0] == 'X') || (tag[0] == 'i') || (tag[0] == 'I')) && (tag[1] == '-'))
    i = 1;
  else
  {
    while (isalpha((unsigned char) tag[i]))
      i++;
    if ((i < 2) || (i > 8))                                 // primary subtag length
      return false;
    if (tag[i] == 0)
      return true;
  }

  // Remaining subtags: '-' followed by 1-8 alphanumerics, repeated.
  while (tag[i] == '-')
  {
    int len = 0;
    i++;
    while (isalnum((unsigned char) tag[i]))
    {
      i++;
      len++;
    }
    if ((len < 1) || (len > 8))
      return false;
  }

  return (tag[i] == 0);
}
