/*
 * cfg_mgr.c
 *
 *  Created on: Jun 12, 2013
 *      Author: Ernie
 */

#include "cfg_mgr.h"

static unsigned char g_mem_pool[8192];
static unsigned long g_mem_pool_idx = 0;

/*
 *   Populate a factory string to the provided buffer.
 *   If factory string doesn't exist, memset to zero.
 */
void CFGMGR_faccfg_get( fcfg_handle_t *pHandle, 
                         fcfg_tag_t tag, 
                         unsigned long max_len, 
                         void *pVal,
                         const char *pDesc )
{
    fcfg_err_t ferr;
    fcfg_len_t len;
    
    ferr = FCFG_getlen(pHandle, tag, &len);
    if( FCFG_OK != ferr )
    {
        
        /*
         *   For the serial number case, we can't zero it out, b/c we may want to preserve 
         *     what has been previously set via the MAC Address (unless the fcfg tag is present).
         */
        if( tag != CFG_FACTORY_SER )
        {
            memset(pVal, 0, max_len);
        }
        return;
    }
    
    memset(pVal, 0, max_len);
    
    ferr = FCFG_get(pHandle, tag, pVal);
    if( FCFG_OK != ferr )
    {
        //corona_print("FCFG: %s:\tERR: %i\tFCFG_get.\n", pDesc, ferr);
        return;
    }
    
    if( FCFG_INT_SZ == len )
    {
        unsigned long val;
        
        memcpy(&val, pVal, FCFG_INT_SZ);
        //corona_print("FCFG: %s\tTAG: %i\tVAL: %u\n", pDesc, tag, val);
    }
    else
    {
        //corona_print("FCFG: %s\tTAG: %i\tVAL: %s\n", pDesc, tag, (char *)pVal);
    }
}

#if 0
/*
 *   Factory-Set Items
 */
void CFGMGR_load_fcfgs(void)
{
    fcfg_handle_t hdl;
    
    /*
     *   Init the handle, functions to use when talking to the factory config table.
     */
    if( FCFG_OK != FCFG_init(&hdl,
                                  EXT_FLASH_FACTORY_CFG_ADDR,
                                  &wson_read,
                                  &wson_write,
                                  &fcfg_malloc,
                                  &fcfg_free) )
    {
        /*
         *  argggggghhhhhh !
         */
        return;
    }
    
    /*
     *   Fill up all the factory config values, if they are present.
     */
    _CFGMGR_faccfg_get( &hdl, CFG_FACTORY_SER,      MAX_SERIAL_STRING_SIZE,  g_st_cfg.fac_st.serial_number, "Serial Number" );
    _CFGMGR_faccfg_get( &hdl, CFG_FACTORY_MPCBA,    MAX_PARTNUM_STRING_SIZE, g_st_cfg.fac_st.mpcba,         "PCBA Part #"   );
    _CFGMGR_faccfg_get( &hdl, CFG_FACTORY_MPCBAREV, FCFG_INT_SZ,             &(g_st_cfg.fac_st.mpcbarev),   "PCBA Revision" );
    _CFGMGR_faccfg_get( &hdl, CFG_FACTORY_MPCBARW,  FCFG_INT_SZ,             &(g_st_cfg.fac_st.mpcbarw),    "PCBA Rework"   );
    _CFGMGR_faccfg_get( &hdl, CFG_FACTORY_LPCBA,    MAX_PARTNUM_STRING_SIZE, g_st_cfg.fac_st.lpcba,         "LED Part #"    );
    _CFGMGR_faccfg_get( &hdl, CFG_FACTORY_LPCBAREV, FCFG_INT_SZ,             &(g_st_cfg.fac_st.lpcbarev),   "LED Revision"  );
    _CFGMGR_faccfg_get( &hdl, CFG_FACTORY_LPCBARW,  FCFG_INT_SZ,             &(g_st_cfg.fac_st.lpcbarw),    "LED Rework"    );
    _CFGMGR_faccfg_get( &hdl, CFG_FACTORY_TLA,      MAX_PARTNUM_STRING_SIZE, g_st_cfg.fac_st.tla,           "TLA Part #"    );
}
#endif

// Wrapper function for fcfg's free format.
unsigned long fcfg_free(void *pAddr)
{
    return 0;
}

// Wrapper function for malloc.
void *fcfg_malloc(unsigned long size)
{
    void *pReturn = (unsigned long)0x00000000;
    
    if( (size + g_mem_pool_idx + 1) > 8192 )
    {
        /*
         *   Wraps around, so start from the beginning.
         */
        g_mem_pool_idx = size;
        pReturn = (void *) &( g_mem_pool[0] );
    }
    else
    {
        g_mem_pool_idx += size;
        pReturn = (void *) &( g_mem_pool[g_mem_pool_idx - size] );
    }
    
    return pReturn;
}
