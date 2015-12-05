/*
 * ar4100p_main.c
 *
 *  Created on: Mar 6, 2013
 *      Author: Chris
 */
#include "ar4100p_main.h"
#include "wassert.h"
#include "app_errors.h"

_enet_address _whistle_enet_address;
_enet_handle _whistle_handle = NULL;

_enet_handle whistle_atheros_get_handle(void)
{
    return _whistle_handle;
}

void whistle_atheros_get_enet_address(_enet_address addr)
{
    memcpy(addr, _whistle_enet_address, sizeof(addr));
}

/* Initialize the driver. Return handle. */
_mqx_uint whistle_atheros_init(_enet_handle *handle)
{
    A_INT32  return_code;
    _mqx_uint mqx_rc;
    
    /* Ben says that threads created by RTCS_create depend on ENET_initialize having completed
     * so let's prevent them from running until then, mk?
     */ 
    _task_stop_preemption();
    
    /* runtime RTCS configuration */
    mqx_rc = RTCS_create();
    if( MQX_OK != mqx_rc )
    {
        PROCESS_NINFO( ERR_WIFI_RTCS_CREATE, "%x", mqx_rc );
        goto FAIL;
    }
    
    ENET_get_mac_address(DEMOCFG_DEFAULT_DEVICE, 0, _whistle_enet_address);
    
    mqx_rc = ipcfg_init_device(DEMOCFG_DEFAULT_DEVICE, _whistle_enet_address);
    if( MQX_OK != mqx_rc )
    {
        PROCESS_NINFO( ERR_WIFI_IPCFG_INIT, "%x", mqx_rc );
        goto FAIL;
    }
    
    /* ENET_initialize is already called from within ipcfg_init_device.  We call it again
     * so that we can get a handle to the driver allowing this code to make direct driver
     * calls as needed. */
    mqx_rc = ENET_initialize(DEMOCFG_DEFAULT_DEVICE, _whistle_enet_address, 0, &_whistle_handle);
    if( (ENETERR_INITIALIZED_DEVICE != mqx_rc) || (NULL == _whistle_handle) )
    {
        PROCESS_NINFO( ERR_WIFI_ENET_INIT, "%x", mqx_rc);
        goto FAIL;
    }
    
    _task_start_preemption();

#if DEMOCFG_USE_WIFI
#if USE_ATH_CHANGES
    clear_wep_keys();
#endif
#endif
    
    /*
     *   Ernie:  Use wmiconfig --pwrmode 1 (power savings mode), which saves about 45mA (!) on idle current.
     */
    return_code = SET_POWER_MODE("1\0");
    if(A_OK != return_code)
    {
        PROCESS_NINFO(ERR_WIFI_SET_POWER, "%i", return_code);
        return MQX_OK+1;
    }
    
    *handle = _whistle_handle;
    return MQX_OK;
    
FAIL:
    _task_start_preemption();
    return mqx_rc;
}
