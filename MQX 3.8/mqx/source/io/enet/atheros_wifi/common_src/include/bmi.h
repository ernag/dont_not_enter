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
#ifndef _BMI_H_
#define _BMI_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Header files */
#include "a_config.h"
#include "athdefs.h"
#include "a_types.h"
//#include "hif.h"
#include "a_osapi.h"
#include "bmi_msg.h"

A_STATUS
BMIInit(A_VOID *pCxt);

void
BMICleanup(A_VOID);

A_STATUS
BMIDone(A_VOID *pCxt);

A_STATUS
BMIGetTargetInfo(A_VOID *pCxt, struct bmi_target_info *targ_info);

A_STATUS
BMIReadMemory(A_VOID *pCxt,
              A_UINT32 address,
              A_UCHAR *buffer,
              A_UINT32 length);

A_STATUS
BMIWriteMemory(A_VOID *pCxt,
               A_UINT32 address,
               A_UCHAR *buffer,
               A_UINT32 length);

A_STATUS
BMIExecute(A_VOID *pCxt,
           A_UINT32 address,
           A_UINT32 *param);

A_STATUS
BMIReadSOCRegister(A_VOID *pCxt,
                   A_UINT32 address,
                   A_UINT32 *param);

A_STATUS
BMIWriteSOCRegister(A_VOID *pCxt,
                    A_UINT32 address,
                    A_UINT32 param);

A_STATUS
BMIRawWrite(A_VOID *pCxt,
            A_UCHAR *buffer,
            A_UINT32 length);

A_STATUS
BMIRawRead(A_VOID *pCxt, 
           A_UCHAR *buffer, 
           A_UINT32 length, 
           A_BOOL want_timeout);             

#ifdef __cplusplus
}
#endif

#endif /* _BMI_H_ */
