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
#include <a_osapi.h>
#include <a_types.h>
#include <driver_cxt.h>
#include <common_api.h>
#include <custom_api.h>
#include <bmi_msg.h>
#include <targaddrs.h>
#include "bmi.h"
#include "atheros_wifi_api.h"
#include "atheros_wifi_internal.h"     
#include "hw/apb_map.h"
#include "hw/mbox_reg.h"
#include "hw4.0/hw/rtc_reg.h"
       
        
/***********************************************/
/* IMPLEMENTATION 							   */
/***********************************************/



#define bmifn(fn) do { \
    if ((fn) < A_OK) { \
        return A_ERROR; \
    } \
} while(0)



static A_STATUS
Driver_BMIConfig(A_VOID *pCxt)
{        
    /* The config is contained within the driver itself */
    A_UINT32 param, options, sleep, address;
	A_DRIVER_CONTEXT *pDCxt = GET_DRIVER_COMMON(pCxt);
	
	if (pDCxt->targetVersion != ATH_FIRMWARE_TARGET) {
		/* compatible wifi chip version is decided at compile time */
		A_ASSERT(0);
		//return A_ERROR;
	}
    /* Temporarily disable system sleep */
    address = MBOX_BASE_ADDRESS + LOCAL_SCRATCH_ADDRESS;
    bmifn(BMIReadSOCRegister(pCxt, address, &param));
    options = A_LE2CPU32(param);
    param |= A_CPU2LE32(AR6K_OPTION_SLEEP_DISABLE);
    bmifn(BMIWriteSOCRegister(pCxt, address, param));

    address = RTC_BASE_ADDRESS + SYSTEM_SLEEP_ADDRESS;
    bmifn(BMIReadSOCRegister(pCxt, address, &param));
    sleep = A_LE2CPU32(param);
    param |= A_CPU2LE32(WLAN_SYSTEM_SLEEP_DISABLE_SET(1));
    bmifn(BMIWriteSOCRegister(pCxt, address, param));        
    
    /* Run at 40/44MHz by default */
    param = CPU_CLOCK_STANDARD_SET(0);
    
    address = RTC_BASE_ADDRESS + CPU_CLOCK_ADDRESS;
    bmifn(BMIWriteSOCRegister(pCxt, address, A_CPU2LE32(param)));
	
	{
        address = RTC_BASE_ADDRESS + LPO_CAL_ADDRESS;
        param = LPO_CAL_ENABLE_SET(1);
        bmifn(BMIWriteSOCRegister(pCxt, address, A_CPU2LE32(param)));
    }
   
      	 
    return A_OK;
}
 
extern A_UINT32 ar4XXX_boot_param; 
void atheros_driver_setup_init(void)
{
	ath_custom_init.Driver_TargetConfig = NULL;	
	ath_custom_init.Driver_BootComm = NULL;
	ath_custom_init.Driver_BMIConfig = Driver_BMIConfig;
	ath_custom_init.skipWmi = A_TRUE;
	ath_custom_init.exitAtBmi = A_TRUE;
	
	ar4XXX_boot_param = AR4XXX_PARAM_MODE_BMI;
}     
 
void reset_atheros_driver_setup_init(void)
{
	ath_custom_init.Driver_TargetConfig = NULL;	
	ath_custom_init.Driver_BootComm = NULL;
	ath_custom_init.Driver_BMIConfig = NULL;
	ath_custom_init.skipWmi = A_FALSE;
	ath_custom_init.exitAtBmi = A_FALSE;
	
	ar4XXX_boot_param = AR4XXX_PARAM_MODE_NORMAL;
}     

/* EOF */
