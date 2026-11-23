//
// FILE            corExePath.h
//
// AUTHOR          Ken Zangelin
//
// Copyright 2026 Seamware
//
// SPDX-License-Identifier: Apache-2.0
//
#ifndef CORBASE_COREXEPATH_H_
#define CORBASE_COREXEPATH_H_

#include <stdbool.h>                    // bool



// -----------------------------------------------------------------------------
//
// corExePath - the running program's own file, its full path (Linux /proc/self/exe, macOS
// _NSGetExecutablePath, links resolved) - false when it cannot be found
//
extern bool corExePath(char* out, int outLen);

#endif  // CORBASE_COREXEPATH_H_
