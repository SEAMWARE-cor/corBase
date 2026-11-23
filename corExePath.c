//
// FILE            corExePath.c
//
// AUTHOR          Ken Zangelin
//
// Copyright 2026 Seamware
//
// SPDX-License-Identifier: Apache-2.0
//
#include <limits.h>                     // PATH_MAX
#include <stdbool.h>                    // bool
#include <stdio.h>                      // snprintf
#include <stdlib.h>                     // realpath
#include <unistd.h>                     // readlink

#if defined(__APPLE__)
#include <mach-o/dyld.h>                // _NSGetExecutablePath
#endif

#include "corBase/corExePath.h"         // Own interface



// -----------------------------------------------------------------------------
//
// corExePath -
//
bool corExePath(char* out, int outLen)
{
  char path[PATH_MAX];

#if defined(__APPLE__)
  char     raw[PATH_MAX];
  uint32_t size = sizeof(raw);

  if ((_NSGetExecutablePath(raw, &size) != 0) || (realpath(raw, path) == NULL))
    return false;
#else
  ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);

  if (len <= 0)
    return false;
  path[len] = 0;
#endif

  return snprintf(out, outLen, "%s", path) < outLen;
}
