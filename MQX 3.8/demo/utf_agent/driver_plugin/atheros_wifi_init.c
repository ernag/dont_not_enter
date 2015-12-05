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
#include "mqx.h"
#include "bsp.h"
#include "enet.h"
#include "enetprv.h"
#include "atheros_wifi_api.h"
#include "atheros_wifi_internal.h"
#include "main.h"
#include "hw/apb_map.h"
#include "hw/mbox_reg.h"
#include "hw4.0/hw/rtc_reg.h"
#include <htc.h>
#include "spi_hcd_if.h"
#include "hif_internal.h"
#include "athdefs.h"


/***********************************************/
/* IMPLEMENTATION 							   */
/***********************************************/
#define TARGET_TYPE_AR6003 3
uint_8 bypasswmi = FALSE;
uint_8 resetok = FALSE;
uint_8 arRawHtcReady = FALSE;
A_UINT32 sleep;

#define A_SUCCESS(x)        (x == A_OK)
#define A_FAILED(x)         (!A_SUCCESS(x))
#define AR_DEBUG_ASSERT(condition)
//extern volatile A_BOOL arReady = FALSE;
//extern A_EVENT_T arEvent;

#define bmifn(fn) do { \
    if ((fn) < A_OK) { \
        return A_ERROR; \
    } \
} while(0)

#define A_SUCCESS(x)        (x == A_OK)
#define A_FAILED(x)         (!A_SUCCESS(x))

/* EOF */
