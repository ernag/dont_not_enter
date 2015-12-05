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
#include <wlan_config.h>
#include <a_types.h>
#include <a_osapi.h>
#include <driver_cxt.h>
#include "mqx.h"
#include "bsp.h"
#include "bsp_prv.h"
#include "enet.h"
#include "enetprv.h"
#include "atheros_phy.h"
#if BSPCFG_ENABLE_ATHEROS_WIFI

static boolean phy_atheros_wifi_discover_addr(ENET_CONTEXT_STRUCT_PTR enet_ptr);
static boolean phy_atheros_wifi_init(ENET_CONTEXT_STRUCT_PTR enet_ptr);
static uint_32 phy_atheros_wifi_get_speed(ENET_CONTEXT_STRUCT_PTR enet_ptr);
static boolean phy_atheros_wifi_get_link_status(ENET_CONTEXT_STRUCT_PTR enet_ptr);

const ENET_PHY_IF_STRUCT ATHEROS_PHY_IF = {
    phy_atheros_wifi_discover_addr,
    phy_atheros_wifi_init,
    phy_atheros_wifi_get_speed,
    phy_atheros_wifi_get_link_status
};
  
  

/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : phy_atheros_wifi_discover_addr
*  Returned Value : none
*  Comments       :
*    Scan possible PHY addresses looking for a valid device
*
*END*-----------------------------------------------------------------*/

static boolean phy_atheros_wifi_discover_addr
   (
       ENET_CONTEXT_STRUCT_PTR	   enet_ptr
   )
{ 
	A_UNUSED_ARGUMENT(enet_ptr);
    return TRUE;
} 


/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : phy_atheros_wifi_init
*  Returned Value : boolean
*  Comments       :
*    Wait for PHY to automatically negotiate its configuration
*
*END*-----------------------------------------------------------------*/

static boolean phy_atheros_wifi_init
   (
       ENET_CONTEXT_STRUCT_PTR	   enet_ptr
   )
{ 
	A_UNUSED_ARGUMENT(enet_ptr);
   return TRUE;
}  


/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : phy_atheros_wifi_get_speed
*  Returned Value : uint_32 - connection speed
*  Comments       :
*    Determine connection speed.
*
*END*-----------------------------------------------------------------*/

static uint_32 phy_atheros_wifi_get_speed
   (
       ENET_CONTEXT_STRUCT_PTR	   enet_ptr
   )
{ 
	A_UNUSED_ARGUMENT(enet_ptr);
   return 54;
} 

  
/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : phy_atheros_wifi_get_link_status
*  Returned Value : TRUE if link active, FALSE otherwise
*  Comments       : 
*    Get actual link status.
*
*END*-----------------------------------------------------------------*/

static boolean phy_atheros_wifi_get_link_status
   (
       ENET_CONTEXT_STRUCT_PTR	   enet_ptr
   )
{ 
    boolean res = FALSE;
    A_VOID *pCxt;
    
    if((A_VOID*)enet_ptr->MAC_CONTEXT_PTR != NULL){
    	pCxt = (A_VOID*)enet_ptr->MAC_CONTEXT_PTR;
    	if(A_TRUE == GET_DRIVER_COMMON(pCxt)->isConnected){       
            res = TRUE;
        }        
    }
  
    return res;
} 
#endif /*BSPCFG_ENABLE_ATHEROS_WIFI*/

/*EOF*/    