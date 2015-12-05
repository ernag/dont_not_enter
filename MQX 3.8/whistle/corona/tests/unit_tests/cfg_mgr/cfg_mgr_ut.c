/*
 * cfg_mgr_ut.c
 *
 *  Created on: May 6, 2013
 *      Author: Ernie
 */

#include "unit_tests.h"
#ifdef ENABLE_WHISTLE_UNIT_TESTS

#include "cfg_mgr.h"
#include "wassert.h"
#include <mqx.h>

/*
 *   Puts a bunch of fake data in BT struct, so we can test that BT config is working...
 */
void _HACK_put_fake_data_in_bt( void )
{
    uint8_t *pStupid;
    uint32_t numBytes;
    int i;
    
    pStupid = (uint8_t *)g_dy_cfg.bt_dy.LinkKeyInfo;
    numBytes = sizeof(LinkKeyInfo_t) * MAX_BT_REMOTES;
    
    for(i = 0; i < numBytes; i++)
    {
        pStupid[i] = i + 1;
    }
}

/*
 *   Serialize configs to SPI Flash, then compare RAM buffer pre-serialization to post-de-serialization,
 *     to make sure they exactly match, and no configs were missed or mis-represented by serialization.
 */
void cfg_mgr_ut(void)
{
    cfg_dynamic_t *pCfg;
    int cmp;
    
    corona_print("CFGMGR Unit Test.  ** WARNING ** - all configs will be toast after this!\n");
#if 0
    _HACK_put_fake_data_in_bt();
#endif
    pCfg = _lwmem_alloc_system( sizeof(g_dy_cfg) );
    wassert(pCfg != NULL);
    
    memcpy(pCfg, &g_dy_cfg, sizeof(g_dy_cfg)); // save state of g_config prior to decoding.
    
    wassert( ERR_OK == _CFG_enc() );
    
    memset(&g_dy_cfg, 0, sizeof(g_dy_cfg)); // set table back to zero to make sure our memcmp() check catches discrepancies.
    
    wassert( ERR_OK == _CFG_dec() );
    
    // Make sure both configs match each other.
    cmp = memcmp(pCfg, &g_dy_cfg, sizeof(g_dy_cfg));
    
    if(cmp == 0)
    {
        corona_print("SUCCESS: encode config matches decode config!\n");
    }
    else if(cmp > 0)
    {
        corona_print("ERROR: Pre-decode buffer is > than post-decode buffer.  memcmp returned: %i\n", cmp);
    }
    else
    {
        corona_print("ERROR: Pre-decode buffer is < than post-decode buffer.  memcmp returned: %i\n", cmp);
    }
    
    fflush(stdout);
    _lwmem_free(pCfg);
}
#endif
