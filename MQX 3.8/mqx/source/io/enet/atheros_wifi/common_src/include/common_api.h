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
#ifndef _COMMON_API_H_
#define _COMMON_API_H_


#ifdef __cplusplus
extern "C" {
#endif

#include <a_config.h>
#include <a_types.h>
#include <a_osapi.h>
#include <htc_api.h>
#include <driver_cxt.h>
A_STATUS 
Driver_Init(A_VOID *pCxt);
A_STATUS 
Driver_DeInit(A_VOID *pCxt);
A_STATUS 
Driver_ContextInit(A_VOID *pCxt);
A_STATUS
Driver_ContextDeInit(A_VOID *pCxt);
A_UINT32 
Htc_ReadCreditCounter(A_VOID *pCxt, A_UINT32 index);
A_VOID
Htc_GetCreditCounterUpdate(A_VOID *pCxt, A_UINT16 epid);
A_VOID 
Htc_PrepareRecvPacket(A_VOID *pCxt, A_VOID *pReq);
A_STATUS
Htc_SendPacket(A_VOID *pCxt, A_VOID *pReq);
A_VOID 
Htc_RxComplete(A_VOID *pCxt, A_VOID *pReq);
A_STATUS 
Htc_ProcessTxComplete(A_VOID *pCxt, A_VOID *pReq);
A_STATUS 
Htc_ProcessRecvHeader(A_VOID *pCxt, A_VOID *pReq,
                                     A_UINT32   *pNextLookAheads,        
                                     A_UINT32        *pNumLookAheads);
A_STATUS 
HTC_Start(A_VOID *pCxt);                                     
A_STATUS 
HTC_WaitTarget(A_VOID *pCxt);                                     
A_STATUS 
HTC_ConnectService(A_VOID *pCxt,
                   HTC_SERVICE_CONNECT_REQ  *pConnectReq,
                   HTC_SERVICE_CONNECT_RESP *pConnectResp);
A_VOID 
HTC_ProcessCpuInterrupt(A_VOID *pCxt);                   
                   
                                                        
A_STATUS 
Driver_Main(A_VOID *pCxt, A_UINT8 scope, A_BOOL *pCanBlock, A_UINT16 *pBlock_msec);
A_VOID 
Driver_ReportRxBuffStatus(A_VOID *pCxt, A_BOOL status);
A_STATUS 
Driver_CompleteRequest(A_VOID *pCxt, A_VOID *pReq);
A_BOOL 
Driver_TxReady(A_VOID *pCxt);
A_STATUS 
Driver_SubmitTxRequest(A_VOID *pCxt, A_VOID *pReq);
A_BOOL 
Driver_RxReady(A_VOID *pCxt);
A_VOID 
Driver_DropTxDataPackets(A_VOID *pCxt);
A_STATUS 
Driver_SendPacket(A_VOID *pCxt, A_VOID *pReq);
A_STATUS 
Driver_RecvPacket(A_VOID *pCxt, A_VOID *pReq);
A_STATUS
Driver_GetTargetInfo(A_VOID *pCxt);
A_STATUS 
Driver_BootComm(A_VOID *pCxt);
A_VOID 
Driver_RxComplete(A_VOID *pCxt, A_VOID *pReq);

A_STATUS
Driver_ReadRegDiag(A_VOID *pCxt, A_UINT32 *address, A_UINT32 *data);
A_STATUS
Driver_WriteRegDiag(A_VOID *pCxt, A_UINT32 *address, A_UINT32 *data);
A_STATUS
Driver_ReadDataDiag(A_VOID *pCxt, A_UINT32 address,
                    A_UCHAR *data, A_UINT32 length);
A_STATUS
Driver_WriteDataDiag(A_VOID *pCxt, A_UINT32 address,
                    A_UCHAR *data, A_UINT32 length);
A_STATUS
Driver_DumpAssertInfo(A_VOID *pCxt, A_UINT32 *pData, A_UINT16 *pLength);

A_VOID 
HW_EnableDisableSPIIRQ(A_VOID *pCxt, A_BOOL Enable, A_BOOL FromIrq);
A_VOID 
HW_PowerUpDown(A_VOID *pCxt, A_UINT8 powerUp);
A_STATUS 
Bus_InOutDescriptorSet(A_VOID *pCxt, A_VOID *pReq);
A_STATUS 
Bus_InOutToken(A_VOID *pCxt,
                           A_UINT32        OutToken,
                           A_UINT8         DataSize,
                           A_UINT32        *pInToken);
A_VOID 
Bus_TransferComplete(A_VOID *pCxt, A_STATUS status);
A_VOID 
HW_InterruptHandler(A_VOID *pCxt);
A_VOID 
Strrcl_Recall(A_VOID *pCxt, A_UINT32 msec_sleep);
A_STATUS 
Hcd_UnmaskSPIInterrupts(A_VOID *pCxt, A_UINT16 Mask);
A_STATUS 
Hcd_ProgramWriteBufferWaterMark(A_VOID *pCxt, A_UINT32 length);
A_VOID
Hcd_RefreshWriteBufferSpace(A_VOID *pCxt);
A_STATUS 
Hcd_Request(A_VOID *pCxt, A_VOID *pReq);
A_BOOL 
Hcd_SpiInterrupt(A_VOID *pCxt);
A_STATUS 
Hcd_ReinitTarget(A_VOID *pCxt);
A_VOID 
Hcd_Deinitialize(A_VOID *pCxt);
A_STATUS 
Hcd_Init(A_VOID *pCxt);
A_STATUS 
Hcd_GetLookAhead(A_VOID *pCxt);
A_STATUS 
Hcd_DoPioExternalAccess(A_VOID *pCxt, A_VOID *pReq);
A_STATUS
Hcd_DoPioInternalAccess(A_VOID *pCxt, A_UINT16 addr, A_UINT32 *pValue, A_BOOL isRead);
A_UINT32
Hcd_GetMboxAddress(A_VOID *pCxt, A_UINT16 mbox, A_UINT32 length);
A_VOID
Hcd_ClearCPUInterrupt(A_VOID *pCxt);
A_VOID 
Hcd_EnableCPUInterrupt(A_VOID *pCxt);

A_UINT16 
Api_TxGetStatus(A_VOID* pCxt);
A_VOID 
Api_TxComplete(A_VOID *pCxt, A_VOID *pReq);
A_VOID 
Api_RxComplete(A_VOID *pCxt, A_VOID *pReq);
A_STATUS
Api_WmiTxStart(A_VOID *pCxt, A_VOID *pReq, HTC_ENDPOINT_ID eid);
A_STATUS 
Api_DataTxStart(A_VOID *pCxt, A_VOID *pReq);
A_VOID
Api_BootProfile(A_VOID *pCxt, A_UINT8 val);
A_STATUS 
Api_InitStart(A_VOID *pCxt);
A_STATUS 
Api_InitFinish(A_VOID *pCxt);
A_VOID 
Api_WMIInitFinish(A_VOID *pCxt);
A_STATUS 
Api_DeInitStart(A_VOID *pCxt);
A_STATUS 
Api_DeInitFinish(A_VOID *pCxt);
A_STATUS
Api_DisconnectWiFi(A_VOID *pCxt);
A_STATUS
Api_ConnectWiFi(A_VOID *pCxt);
A_STATUS
Api_ParseInfoElem(A_VOID *pCxt, WMI_BSS_INFO_HDR *bih, A_INT32 len, A_SCAN_SUMMARY *pSummary);
A_STATUS
Api_DriverAccessCheck(A_VOID *pCxt, A_UINT8 block_allowed, A_UINT8 request_reason);
A_STATUS
Api_ProgramMacAddress(A_VOID *pCxt, A_UINT8* addr, A_UINT16 length, A_UINT8 *pResult);

A_ENDPOINT_T *
Util_GetEndpoint(A_VOID *pCxt, A_UINT16 id);
A_UINT16
Util_Ieee2freq(A_INT32 chan);
A_UINT32
Util_Freq2ieee(A_UINT16 freq);
HTC_ENDPOINT_ID
Util_AC2EndpointID(A_VOID *pCxt, A_UINT8 ac);
A_UINT8
Util_EndpointID2AC(A_VOID *pCxt, HTC_ENDPOINT_ID ep );

#define SOCKET_CONTEXT_INIT      socket_context_init()
#define STACK_INIT(pCxt)         send_stack_init((pCxt))
#define SOCKET_CONTEXT_DEINIT()  socket_context_deinit()
#define MIN_HDR_LEN              sizeof (WMI_DATA_HDR)
#define WMI_DOT3_2_DIX(pReq)     A_OK

#ifdef __cplusplus
}
#endif

#endif /* _COMMON_API_H_ */
