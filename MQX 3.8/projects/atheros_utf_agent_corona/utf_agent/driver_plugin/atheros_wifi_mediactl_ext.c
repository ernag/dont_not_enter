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
#include <mqx.h>
#include <bsp.h>
#include "enet.h"
#include "enetprv.h"
#include "atheros_wifi_api.h"
#include "atheros_wifi_internal.h"
#include "../main.h"       
#include "atheros_driver_plugin.h"

uint_32 ath_ioctl_handler_ext(void* enet_ptr, ATH_IOCTL_PARAM_STRUCT_PTR param_ptr);

extern A_UINT8 *line;
extern boolean ReadResp;
                               
/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : ath_ioctl_handler
*  PARAMS         : 
*                   enet_ptr-> ptr to enet context
*                   inout_param -> input/output data for command.
*  Returned Value : ENET_OK or error code
*  Comments       :
*        IOCTL implementation of Atheros Wifi device.
*
*END*-----------------------------------------------------------------*/

uint_32 ath_ioctl_handler_ext(void* enet_ptr, ATH_IOCTL_PARAM_STRUCT_PTR param_ptr)
{
    uint_32 error = ENET_OK;
    uint_32 command_id = param_ptr->cmd_id;
    A_VOID* pCxt;

    if((pCxt = ((ENET_CONTEXT_STRUCT_PTR)enet_ptr)->MAC_CONTEXT_PTR) == NULL)
	{
	   	return ENET_ERROR;
	} 
	
    switch (command_id)
    {       


        case ATH_WRITE_TCMD:
            {
                error = ar6000_writeTcmd(pCxt,(unsigned char *)param_ptr->data, param_ptr->length,ReadResp);
            }       
            break;
        case ATH_READ_TCMD_BUF:
            {
                error = ar600_ReadTcmdResp(pCxt,(unsigned char *)param_ptr->data);  
            }
            break;
        case ATH_SET_RESP:
            {
                error = ar600_SetTcmdResp(pCxt,(unsigned char *)param_ptr->data);   
            }
            break;
	case ATH_CHECK_WRITE_DONE:
	    {
                error = ar600_CheckWriteComplete(pCxt,(unsigned char *)param_ptr->data);   
	    }
	    break;
        default:
            error = ENET_ERROR;
            break;
    }
    return  error;
}

void atheros_driver_setup_mediactl(void)
{
    ath_custom_mediactl.ath_ioctl_handler_ext = ath_ioctl_handler_ext;
}
/* EOF */
