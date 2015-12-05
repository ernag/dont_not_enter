/**HEADER********************************************************************
* 
* Copyright (c) 2008 Freescale Semiconductor;
* All Rights Reserved
*
* Copyright (c) 2004-2008 Embedded Access Inc.;
* All Rights Reserved
*
*************************************************************************** 
*
* THIS SOFTWARE IS PROVIDED BY FREESCALE "AS IS" AND ANY EXPRESSED OR 
* IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES 
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  
* IN NO EVENT SHALL FREESCALE OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
* INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, 
* STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING 
* IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF 
* THE POSSIBILITY OF SUCH DAMAGE.
*
**************************************************************************
*
* $FileName: RTCS.c$
* $Version : 3.5.17.0$
* $Date    : Jan-6-2010$
*
* Comments:
*
*   Example of HVAC using RTCS.
*
*END************************************************************************/

#include "main.h"

#if DEMOCFG_USE_WIFI
#include "string.h"
#endif

_enet_handle    handle;
_enet_address   whistle_enet_address;

#if DEMOCFG_ENABLE_RTCS

#include <ipcfg.h>

extern const SHELL_COMMAND_STRUCT Telnet_commands[];

static void Telnetd_shell_fn (pointer dummy) 
{  
   Shell(Telnet_commands,NULL);
}

const RTCS_TASK Telnetd_shell_template = {"Telnet_shell", 8, 2000, Telnetd_shell_fn, NULL};

void HVAC_initialize_networking(void) 
{
   int_32            error;
    IPCFG_IP_ADDRESS_DATA   ip_data;

#if DEMOCFG_USE_POOLS && defined(DEMOCFG_RTCS_POOL_ADDR) && defined(DEMOCFG_RTCS_POOL_SIZE)
    /* use external RAM for RTCS buffers */
    _RTCS_mem_pool = _mem_create_pool((pointer)DEMOCFG_RTCS_POOL_ADDR, DEMOCFG_RTCS_POOL_SIZE);
#endif
   /* runtime RTCS configuration */
   _RTCSPCB_init = 4;
   _RTCSPCB_grow = 2;
   _RTCSPCB_max = 20;
   _RTCS_msgpool_init = 4;
   _RTCS_msgpool_grow = 2;
   _RTCS_msgpool_max  = 20;
   _RTCS_socket_part_init = 4;
   _RTCS_socket_part_grow = 2;
   _RTCS_socket_part_max  = 20;
   FTPd_buffer_size = 536;

    error = RTCS_create();

    IPCFG_default_enet_device = DEMOCFG_DEFAULT_DEVICE;
    IPCFG_default_ip_address = ENET_IPADDR;
    IPCFG_default_ip_mask = ENET_IPMASK;
    IPCFG_default_ip_gateway = ENET_IPGATEWAY;
    LWDNS_server_ipaddr = ENET_IPGATEWAY;

    ip_data.ip = IPCFG_default_ip_address;
    ip_data.mask = IPCFG_default_ip_mask;
    ip_data.gateway = IPCFG_default_ip_gateway;
    
    ENET_get_mac_address (IPCFG_default_enet_device, IPCFG_default_ip_address, IPCFG_default_enet_address);
    error = ipcfg_init_device (IPCFG_default_enet_device, IPCFG_default_enet_address);
    
     if((error = ENET_initialize (IPCFG_default_enet_device, IPCFG_default_enet_address, 0, &handle)) != ENETERR_INITIALIZED_DEVICE)
    {
    	while(1)
    	{
    		
    	}
    }
#if DEMOCFG_USE_WIFI
    /*iwcfg_set_essid (IPCFG_default_enet_device,DEMOCFG_SSID);
    if ((strcmp(DEMOCFG_SECURITY,"wpa") == 0)||strcmp(DEMOCFG_SECURITY,"wpa2") == 0)
    {
        iwcfg_set_passphrase (IPCFG_default_enet_device,DEMOCFG_PASSPHRASE);

    }
#if 0    
    if (strcmp(DEMOCFG_SECURITY,"wep") == 0)
    {
      iwcfg_set_wep_key (IPCFG_default_enet_device,DEMOCFG_WEP_KEY,strlen(DEMOCFG_WEP_KEY),DEMOCFG_WEP_KEY_INDEX);
    }
#else
	if (strcmp(DEMOCFG_SECURITY,"wep") == 0)
    {
      iwcfg_set_wep_keys (IPCFG_default_enet_device,
      	DEMOCFG_WEP_KEY,
      	DEMOCFG_WEP_KEY,
      	DEMOCFG_WEP_KEY,
      	DEMOCFG_WEP_KEY,
      	strlen(DEMOCFG_WEP_KEY),
      	DEMOCFG_WEP_KEY_INDEX);
    }
#endif    
    iwcfg_set_sec_type (IPCFG_default_enet_device,DEMOCFG_SECURITY);
    iwcfg_set_mode (IPCFG_default_enet_device,DEMOCFG_NW_MODE);
    
    iwcfg_commit(IPCFG_default_enet_device);*/
#endif  

   // error = ipcfg_bind_staticip (IPCFG_default_enet_device, &ip_data);

#if DEMOCFG_ENABLE_FTP_SERVER
   FTPd_init("FTP_server", 7, 3000 );
#endif

#if DEMOCFG_ENABLE_TELNET_SERVER
   TELNETSRV_init("Telnet_server", 7, 1800, (RTCS_TASK_PTR) &Telnetd_shell_template );
#endif
}

#else /* DEMOCFG_ENABLE_RTCS */
void HVAC_initialize_networking(void)
{
        int_32            error;

        /* runtime RTCS configuration */

        error = RTCS_create();

        if (error) {
                printf("RTCS_create failed %x \n", error);
        }

        ENET_get_mac_address (DEMOCFG_DEFAULT_DEVICE, 0, whistle_enet_address);

        error = ipcfg_init_device (DEMOCFG_DEFAULT_DEVICE, whistle_enet_address);

        if (error) {
                printf("ipcfg_init_device %x\n", error);
        }

        /* ENET_initialize is already called from within ipcfg_init_device.  We call it again
         * so that we can get a handle to the driver allowing this code to make direct driver
         * calls as needed. */
        if((error = ENET_initialize (DEMOCFG_DEFAULT_DEVICE, whistle_enet_address, 0, &handle)) != ENETERR_INITIALIZED_DEVICE)
        {
                while(1)
                {

                }
        }
}

#if (DEMOCFG_ENABLE_FTP_SERVER+DEMOCFG_ENABLE_TELNET_SERVER) > 0
#warning RTCS network stack is disabled, the selected service will not be available.
#endif

#endif /* DEMOCFG_ENABLE_RTCS */

/* EOF */
