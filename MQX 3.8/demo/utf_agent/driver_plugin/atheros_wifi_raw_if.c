//------------------------------------------------------------------------------
// Copyright (c) 2004-2010 Atheros Communications Inc.
// All rights reserved.
//
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
//
// Author(s): ="Atheros"
//------------------------------------------------------------------------------
#include <a_config.h>
#include <a_osapi.h>
#include <a_types.h>
#include <driver_cxt.h>
#include <common_api.h>
#include <custom_api.h>
#include <bmi_msg.h>
#include <targaddrs.h>
#include "mqx.h"
#include "bsp.h"
#include "enet.h"
#include "enetprv.h"
#include "atheros_wifi_api.h"
#include "atheros_wifi_internal.h"
#include "main.h"       
#include "athdefs.h"


int GetResp = FALSE;
volatile A_BOOL writeFlag = A_FALSE;
extern A_BOOL lWrite;

A_STATUS ar600_CheckWriteComplete(A_VOID *pCxt,unsigned char *buffer)
{
    A_STATUS status = ENET_OK;
    do {    
        CUSTOM_DRIVER_WAIT_FOR_CONDITION(pCxt, &writeFlag, A_TRUE,40);
    }while(lWrite && writeFlag == A_FALSE);

    writeFlag = A_FALSE;
    return status;
}

A_STATUS ar600_ReadTcmdResp(A_VOID *pCxt,unsigned char *buffer)
{
    A_STATUS status = ENET_OK;

    A_MEMCPY(buffer,GET_DRIVER_CXT(pCxt)->pScanOut,GET_DRIVER_CXT(pCxt)->testCmdRespBufLen);
    writeFlag = A_TRUE;
    CUSTOM_DRIVER_WAKE_USER(pCxt);
    return status;
}

A_STATUS ar600_SetTcmdResp(A_VOID *pCxt,unsigned char *buffer)	
{
   boolean Response = *(unsigned char *)buffer;
   GetResp = Response;
   return ENET_OK;
}

A_STATUS ar6000_writeTcmd(A_VOID *pCxt,unsigned char *buffer, uint_32 length,boolean rResp)
{
	A_STATUS status = ENET_OK;
	A_UINT32 waitTime = 0;

	A_DRIVER_CONTEXT *pDCxt = GET_DRIVER_COMMON(pCxt);
	wmi_cmd_start(pDCxt->pWmiCxt,buffer,WMI_TEST_CMDID,length);

	pDCxt->testResp = A_FALSE;

	if(GetResp) {
		do {
			CUSTOM_DRIVER_WAIT_FOR_CONDITION(pCxt, &(pDCxt->testResp), A_TRUE, 1000);
		}while(GetResp && (pDCxt->testResp == A_FALSE));

		tcmd_update_com_len(GET_DRIVER_CXT(pCxt)->testCmdRespBufLen);
	}
	pDCxt->testResp = A_FALSE;

	return status;
}
