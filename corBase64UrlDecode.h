#ifndef CORBASE_CORBASE64URLDECODE_H_
#define CORBASE_CORBASE64URLDECODE_H_

//
// FILE            corBase64UrlDecode.h
//
// AUTHOR          Ken Zangelin
//
// Copyright 2026 Seamware
// SPDX-License-Identifier: Apache-2.0
//



// -----------------------------------------------------------------------------
//
// corBase64UrlDecode - base64url (RFC 4648 § 5), padding optional, into out (room for len * 3 / 4 + 3
// bytes); the number of bytes decoded, -1: not base64url. Standard base64's + and / are taken too.
//
extern int corBase64UrlDecode(const char* in, int len, unsigned char* out);

#endif  // CORBASE_CORBASE64URLDECODE_H_
