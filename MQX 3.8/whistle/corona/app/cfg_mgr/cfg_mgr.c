/*
 * cfg_mgr.c
 *
 *  Created on: Mar 15, 2013
 *      Author: Chris
 */
////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include "cfg_mgr.h"
#include "cfg_mgr_priv.h"
#include "app_errors.h"
#include "corona_ext_flash_layout.h"
#include "ext_flash.h"
#include "wassert.h"
#include "pwr_mgr.h"
#include "crc_util.h"
#include "persist_var.h"

#if 0
#include "wifi_mgr.h"
#include "whistlemessage.pb.h"
#endif

////////////////////////////////////////////////////////////////////////////////
// Defines
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// Variables
////////////////////////////////////////////////////////////////////////////////

// DYNAMIC Configuration structure in RAM.  
//   For simplicity and memory/speed optimization, everyone in the system has
//   R/W access to this structure.
cfg_dynamic_t g_dy_cfg;

// STATIC Configuration structure in RAM.  
//   For simplicity and memory/speed optimization, everyone in the system has
//   R/W access to this structure.
cfg_static_t g_st_cfg;

static uint8_t flush_dy_at_shutdown = 0;

////////////////////////////////////////////////////////////////////////////////
// Code
////////////////////////////////////////////////////////////////////////////////

static void _CFGMGR_dynamic_init(void)
{
    err_t err;
    
    /*
     *   Apply the defaults always, they only get overwritten if there is something valid to decode.
     */
    _CFGMGR_dynamic_defaults();
    
    err = _CFG_dec();
    if(ERR_CFGMGR_NOTHING_TO_DECODE == err)
    {
        corona_print("CFG: clean slate\n");
        
        /*
         *   We have already loaded dynamic factory defaults, now just flush them and return.
         */
        CFGMGR_flush( CFG_FLUSH_DYNAMIC );
        return;
    }
    else if(ERR_CFGMGR_DECODE_FAILED == err)
    {
        /*
         *   Try again, since the decode that failed could have been a bad sector, so
         *    we will try the other sector.
         */
        err = _CFG_dec();
        if( (ERR_CFGMGR_NOTHING_TO_DECODE == err) || (ERR_OK == err) )
        {
            return;
        }
        else
        {
            /*
             *   We've lost all hope, so we need to clear the configs!
             */
            persist_set_untimely_crumb( BreadCrumbEvent_DYNAMIC_CFG_RESET );
            _CFGMGR_dynamic_defaults();
            CFGMGR_flush( CFG_FLUSH_DYNAMIC );
            return;
        }
    }
    else if(ERR_OK == err)
    {
        return;
    }
    
    wassert(0);
}

static uint8_t _CFGMGR_crc_valid( cfg_static_t *pStaticCfg )
{
    return ( UTIL_crc32_calc( (uint_8 *)(pStaticCfg), (uint32_t)((uint32_t)&(pStaticCfg->crc) - (uint32_t)pStaticCfg)) == pStaticCfg->crc )? 1:0;
}

static void _CFGMGR_static_init(void)
{
    /*
     *   TODO: For a buttoned up unit's release build, we can replace this function with a call to
     *         static_defaults(), which will save us from having to read this sector and verify CRC
     *         everytime.
     */
    //wassert( sizeof(g_st_cfg) <= EXT_FLASH_4KB_SZ ); // we use only 1 4KB sector for config, this will alert us if we go over.
    
    wson_read( EXT_FLASH_CFG_STATIC_ADDR, (uint_8 *)&g_st_cfg, sizeof(g_st_cfg) );
    if( CFG_STATIC_MAGIC != g_st_cfg.magic )
    {
        /*
         *   We do not recognize the magic value b/c it is either old or corrupted.
         *   Therefore apply defaults.  There is no need to FLUSH, until we actually
         *   make some change against the defaults, so don't even bother.
         */
        _CFGMGR_static_defaults();
        
        //wassert(ERR_OK == CFGMGR_flush( CFG_FLUSH_STATIC ) );
    }
    else
    {
        corona_print("Us'g ST Cfgs in FLASH\nSER: %s\n", g_st_cfg.fac_st.serial_number);
    }
    
    /*
     *   Factory Stuff can be separate and not glued to the magic number. Just load it new at every boot.
     */
	CFGMGR_load_fcfgs(FALSE);
	
#if 0
    if( ! _CFGMGR_crc_valid( &g_st_cfg ) )
    {
        _CFGMGR_static_defaults();
        CFGMGR_flush( CFG_FLUSH_STATIC );
    }
#endif
}

/*
 *   Initialize the configuration system and RAM structure for usage.
 */
void CFGMGR_init(void)
{
    _CFGMGR_static_init();
    _CFGMGR_dynamic_init();

    corona_print("chk'd in? %u\n", g_dy_cfg.sysinfo_dy.deviceHasCheckedIn);
    
    // Doen't allow LLS mode unless it's enabled in the static configuration
    if ( !g_st_cfg.pwr_st.enable_lls_deep_sleep )
    {
        PWRMGR_VoteForAwake( PWRMGR_PWRMGR_VOTE );
    }
}

/*
 *   Request flush at shutdown
 */
void  CFGMGR_req_flush_at_shutdown( void )
{
	flush_dy_at_shutdown = 1;
}

/*
 *   If anyone has requested a postponed flush, do that now.
 */
void  CFGMGR_postponed_flush( void )
{
	if( flush_dy_at_shutdown )
	{
		CFGMGR_flush( CFG_FLUSH_DYNAMIC );
		flush_dy_at_shutdown = 0;
	}
}

/*
 *   FLUSH Dynamic and/or Static Configs to SPI Flash.
 */
void CFGMGR_flush( cfg_flush_t type )
{   
    /*
     *   Make sure the user passed in a valid type.
     */
    //wassert( (type & CFG_FLUSH_DYNAMIC) || (type & CFG_FLUSH_STATIC) );
    
    if( type & CFG_FLUSH_DYNAMIC )
    {
        corona_print("CFG:flush DY\n");
        PROCESS_NINFO( _CFG_enc(), NULL );
    }
    
    /*
     *   TODO:  We only need to flush Static Configs on non-production-release builds!
     */
    if( type & CFG_FLUSH_STATIC )
    {
        corona_print("CFG:flush ST %x\n", EXT_FLASH_CFG_STATIC_ADDR);
#if 0
        g_st_cfg.crc = UTIL_crc32_calc( (uint_8 *)(&g_st_cfg), (uint32_t)((uint32_t)&(g_st_cfg.crc) - (uint32_t)&g_st_cfg));
#endif
        wson_write( EXT_FLASH_CFG_STATIC_ADDR, (uint_8 *)&g_st_cfg, sizeof(g_st_cfg), 1 );
    }
}

/*
 *   Sets an arbitrary config based on whatever the [server] wants.
 *   TODO - Add all the other configs as well to this function.
 *   NOTE - this API does NOT by design call FLUSH.  It is up to clients
 *          to call CFG_flush(CFG_FLUSH_DYNAMIC) after everything has been set, to reduce
 *          the extreme overhead associated with flushing configs.
 */
err_t CFGMGR_set_config(server_tag_t tag, const void *pValue)
{
    switch(tag)
    {
        case CFG_DOG_NAME:
            strcpy(g_dy_cfg.usr_dy.dog_name, (const char *)pValue);
            break;
            
        case CFG_MIN_ACT_GOAL:
            memcpy(&g_dy_cfg.usr_dy.daily_minutes_active_goal, pValue, sizeof(g_dy_cfg.usr_dy.daily_minutes_active_goal));
            break;
            
        case CFG_TIMEZONE_OFFSET:
            memcpy(&g_dy_cfg.usr_dy.timezone_offset, pValue, sizeof(g_dy_cfg.usr_dy.timezone_offset));
            break;
            
        default:
            corona_print("WARNING: setcfg req for bad cfg\n");
            return ERR_CFGMGR_CONFIG_UNKNOWN;
    }
    
    /*
     *    NOTE:  The app needs to flush after its sets all the configs.
     *           In this way, we can avoid flushing everytime this API is called.
     *    
     *    wassert( ERR_OK == CFGMGR_flush( CFG_FLUSH_DYNAMIC ) );
     */
    return ERR_OK;
}
