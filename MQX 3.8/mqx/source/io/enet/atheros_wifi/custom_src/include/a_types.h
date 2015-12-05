//------------------------------------------------------------------------------
// Copyright (c) 2011 Qualcomm Atheros, Inc.
// All Rights Reserved.
// Qualcomm Atheros Confidential and Proprietary.
// Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is
// hereby granted, provided that the above copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
// INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
// USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
//------------------------------------------------------------------------------
//==============================================================================
// Author(s): ="Atheros"
//==============================================================================

#ifndef _A_TYPES_H_
#define _A_TYPES_H_

#include <psptypes.h>


typedef int_8       A_INT8;
typedef int_16      A_INT16;
typedef int_32      A_INT32;

typedef uint_8      A_UINT8;
typedef uint_16     A_UINT16;
typedef uint_32     A_UINT32;

/* NOTE: A_BOOL is a type that is used in various WMI commands and events.
 * as such it is a type that is shared between the wifi chipset and the host.
 * It is required for that reason that A_BOOL be treated as a 32-bit/4-byte type.
 */
typedef int_32      A_BOOL;
typedef char        A_CHAR;
typedef uint_8      A_UCHAR;
#define A_VOID void 
#define A_CONST const

#define A_TRUE      ((A_BOOL)1)
#define A_FALSE     ((A_BOOL)0)


#endif /* _A_TYPES_H_ */
