//
// FILE            corFileMtimeNs.h
//
// AUTHOR          Ken Zangelin
//
// Copyright 2026 Seamware
//
// SPDX-License-Identifier: Apache-2.0
//
#ifndef CORBASE_CORFILEMTIMENS_H_
#define CORBASE_CORFILEMTIMENS_H_

#include <sys/stat.h>                   // struct stat



// -----------------------------------------------------------------------------
//
// corFileMtimeNs - a file's modification time in nanoseconds since the epoch, from its stat (Linux st_mtim,
// macOS st_mtimespec)
//
extern long long corFileMtimeNs(const struct stat* stP);

#endif  // CORBASE_CORFILEMTIMENS_H_
