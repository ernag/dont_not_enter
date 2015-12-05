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
#ifndef __ATHEROS_WIFI_INTERNAL_H__
#define __ATHEROS_WIFI_INTERNAL_H__

#define ATH_PROG_FEEDBACK_BLANK_FLASH   (0x00000001)
#define ATH_PROG_FEEDBACK_LOADING       (0x00000002)
#define ATH_PROG_FEEDBACK_EXECUTING     (0x00000003)
#define ATH_PROG_FEEDBACK_DONE          (0x00000004)
#define ATH_PROG_FEEDBACK_LOAD_FAIL     (0x00000005)
#define ATH_PROG_FEEDBACK_COMM_ERROR    (0x00000006)

typedef struct{
A_STATUS 	(*Driver_TargetConfig)(A_VOID *);
A_STATUS 	(*Driver_BootComm)(A_VOID *);
A_STATUS 	(*Driver_BMIConfig)(A_VOID *);
A_VOID      (*Api_TxComplete)(A_VOID*, A_VOID*);
A_VOID      (*Api_RxComplete)(A_VOID*, A_VOID*);
A_VOID		(*Boot_Profile)(A_UINT32 val);
A_VOID      (*Custom_Delay)(A_UINT32 delay);
A_BOOL		skipWmi;
A_BOOL 		exitAtBmi;
}ATH_CUSTOM_INIT_T;

extern ATH_CUSTOM_INIT_T ath_custom_init;


typedef struct{
	A_UINT32 (*ath_ioctl_handler_ext)(A_VOID*, ATH_IOCTL_PARAM_STRUCT_PTR);
}ATH_CUSTOM_MEDIACTL_T;

extern ATH_CUSTOM_MEDIACTL_T ath_custom_mediactl;



typedef struct{
A_STATUS (*HTCConnectServiceExch)(A_VOID*, HTC_SERVICE_CONNECT_REQ*, HTC_SERVICE_CONNECT_RESP*,
								  HTC_ENDPOINT_ID*, A_UINT32*);
A_STATUS (*HTCSendSetupComplete)(A_VOID*);
A_STATUS (*HTCGetReady)(A_VOID*);
}ATH_CUSTOM_HTC_T;

extern ATH_CUSTOM_HTC_T ath_custom_htc;


#endif /* __ATHEROS_WIFI_INTERNAL_H__ */
