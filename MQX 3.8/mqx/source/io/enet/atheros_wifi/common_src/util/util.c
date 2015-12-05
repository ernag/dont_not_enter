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
#include <a_config.h>
#include <a_types.h>
#include <a_osapi.h>
#include <common_api.h>
#include <driver_cxt.h>

A_ENDPOINT_T *
Util_GetEndpoint(A_VOID *pCxt, A_UINT16 id)
{
    /* this translation allows multiple virtual endpoints to be mapped to 
     * a common endpoint object. */
    if(id >= ENDPOINT_MANAGED_MAX){
        id = ENDPOINT_MANAGED_MAX-1;
    }
    return &(GET_DRIVER_COMMON(pCxt)->ep[id]);
}


/*
 * converts ieee channel number to frequency
 */
A_UINT16
Util_Ieee2freq(A_INT32 chan)
{
    if (chan == 14) {
        return (A_UINT16)2484;
    }
    if (chan < 14) {    /* 0-13 */
        return (A_UINT16)(2407 + (chan*5));
    }
    if (chan < 27) {    /* 15-26 */
        return (A_UINT16)(2512 + ((chan-15)*20));
    }
    return (A_UINT16)(5000 + (chan*5));
}

/*
 * Converts MHz frequency to IEEE channel number.
 */
A_UINT32
Util_Freq2ieee(A_UINT16 freq)
{
    if (freq == 2484)
        return (A_UINT32)14;
    if (freq < 2484)
        return (A_UINT32)((freq - 2407) / 5);
    if (freq < 5000)
        return (A_UINT32)(15 + ((freq - 2512) / 20));
    return (A_UINT32)((freq - 5000) / 5);
}



HTC_ENDPOINT_ID
Util_AC2EndpointID(A_VOID *pCxt, A_UINT8 ac)
{
    A_DRIVER_CONTEXT *pDCxt = GET_DRIVER_COMMON(pCxt);
    return pDCxt->ac2EpMapping[ac];
}

A_UINT8
Util_EndpointID2AC(A_VOID *pCxt, HTC_ENDPOINT_ID ep )
{
    A_DRIVER_CONTEXT *pDCxt = GET_DRIVER_COMMON(pCxt);
    return pDCxt->ep2AcMapping[ep];
}
