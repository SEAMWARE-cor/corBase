//
// FILE            corFileMtimeNs.c
//
// AUTHOR          Ken Zangelin
//
// Copyright 2026 Seamware
//
// SPDX-License-Identifier: Apache-2.0
//
#if defined(__linux__)
#define _DEFAULT_SOURCE                          // st_mtim under a strict -std=c17
#endif

#include <sys/stat.h>                   // struct stat

#include "corBase/corFileMtimeNs.h"     // Own interface



// -----------------------------------------------------------------------------
//
// corFileMtimeNs -
//
long long corFileMtimeNs(const struct stat* stP)
{
#if defined(__APPLE__)
  return (long long) stP->st_mtimespec.tv_sec * 1000000000LL + stP->st_mtimespec.tv_nsec;
#else
  return (long long) stP->st_mtim.tv_sec * 1000000000LL + stP->st_mtim.tv_nsec;
#endif
}
