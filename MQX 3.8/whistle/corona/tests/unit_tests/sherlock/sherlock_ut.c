/*
 * sherlock_ut.c
 *
 *  Created on: Jul 14, 2013
 *      Author: Ernie
 */

#include "unit_tests.h"
#ifdef ENABLE_WHISTLE_UNIT_TESTS

#include "sherlock_ut.h"
#include "sherlock.h"
#include "corona_console.h"
#include "corona_ext_flash_layout.h"
#include "ext_flash.h"
#include <mqx.h>

extern uint8_t g_sherlock_in_progress;

void sherlock_ut(void)
{
    shlk_err_t err;
    shlk_handle_t hdl;
    char buf[256];
    uint32_t count = 0;
    
    _time_delay( 3 * 1000 );  // give some time for accel to boot up, so it doesn't complain about DMA/FIFO stuff due to the erasing required here.
    
    err = SHLK_init(&hdl,
                    wson_read,
                    wson_write,
                    shlk_printf,
                    EXT_FLASH_SHLK_ASSDUMP_ADDR,
                    EXT_FLASH_SHLK_LOG_ADDR,
                    (sh_u8_t)EXT_FLASH_SHLK_LOG_NUM_SECTORS);
    
    if( SHLK_ERR_OK != err )
    {
        corona_print("SHLK init ERR %i\n", err); fflush(stdout);
        return;
    }
    
#if 0
    /*
     *   Test out asslogging and assdumping.
     */
    while(count < 20)
    {
        if(count % 2)
        {
            sprintf(buf, "short assert %i\n\0", count);
        }
        else
        {
            sprintf(buf, "<<<%i>>>yay blah yay far out man.456789012345678901234567890\n\0", count);
        }

        err = SHLK_asslog(&hdl, buf);
        
        if( SHLK_ERR_OK != err )
        {
            corona_print("SHLK asslog ERR %i\n", err); fflush(stdout);
            return;
        }
        
        count++;
    }
    
    g_sherlock_in_progress = 1;
    err = SHLK_assdump(&hdl);
    g_sherlock_in_progress = 0;
    if( SHLK_ERR_OK != err )
    {
        corona_print("SHLK assdump ERR %i\n", err); fflush(stdout);
        return;
    }
#endif
    
    count = 0;
    
    while(count < 10)
    {
        if(count % 2)
        {
            corona_print("my fun test %i\n", count);
        }
        else
        {
            corona_print("WHAT OH YEAH !  WOO_HOO?  %i  %i  \n", count, count);
        }
        
        _time_delay(25);  // give some time for Sherlock low priority task to actually process crap.
        count++;
    }
    
    g_sherlock_in_progress = 1;
    err = SHLK_dump(&hdl, 0);
    g_sherlock_in_progress = 0;
    if( SHLK_ERR_OK != err )
    {
        corona_print("SHLK dump ERR %i\n", err);
    }
    
    return;
}
#endif
