/*
 * evt_mgr_spam_ut.c
 * 
 * Unit test for spamming the SPI Flash with EVTMGR data.
 *
 *  Created on: Aug 28, 2013
 *      Author: Ernie
 */
#include "unit_tests.h"
#ifdef ENABLE_WHISTLE_UNIT_TESTS

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include "evt_mgr.h"
#include "evt_mgr_ut.h"
#include "app_errors.h"
#include "corona_ext_flash_layout.h"
#include "wassert.h"
#include "pwr_mgr.h"
#include "ext_flash.h"

////////////////////////////////////////////////////////////////////////////////
// Defines
////////////////////////////////////////////////////////////////////////////////
#define NUM_ENTRIES_BEFORE_GIVING_EVTMGR_TIME_TO_FLUSH   25
#define BIG_EVENT_SZ		(2048)
#define TOTAL_DATA_TO_SEND	(600000)
#define MAX_SPAM_ENTRIES	( TOTAL_DATA_TO_SEND / ( BIG_EVENT_SZ + 83) )   // 83 is the size of the spam string entry.

////////////////////////////////////////////////////////////////////////////////
// Externs
////////////////////////////////////////////////////////////////////////////////
extern void _EVTMGR_con_mgr_register_any(uint_32 min_wait_time, uint_32 max_wait_time);
extern void _EVTMGR_lock(void);

////////////////////////////////////////////////////////////////////////////////
// Code
////////////////////////////////////////////////////////////////////////////////

/*
 *   Unit Test where we just continuously "SPAM" the SPI Flash with data.
 *     The goal here is to load up on data and see if things are working OK with no hanging / data loss.
 *     
 *   
 */
void EVTMGR_spam_ut(void)
{
    unsigned long num_spam_entries = 0, i;
    char *pBuf;
    char theChar = '0';
    
    pBuf = _lwmem_alloc( BIG_EVENT_SZ );
    wassert( pBuf );
    
    for(i=0; i < BIG_EVENT_SZ; i++)
    {
    	pBuf[i] = theChar++;
    	
    	if(theChar >= 'z')
    	{
    		theChar = '0'; //circle around.
    	}
    }
    
    // NULL terminate the "string".
    pBuf[BIG_EVENT_SZ-1] = 0;
    
#if 0
    // Pointless unit test for testing COR-522.
    while(1)
    {
        uint8_t data[20];
        int ret;
        
        ret = wson_read(0xFFFFFFFF, data, 20);
        corona_print("ret: %i\tdata: 0x%x%x%x%x\n", ret, data[0], data[1], data[2], data[3]);
        _time_delay(200);
    }
#endif
    
    PWRMGR_VoteForRUNM( PWRMGR_BURNIN_VOTE );  // crank it up !
    
    corona_print("EVTMGR_spam_ut\n");
    corona_print("\tptr addr: 0x%x\n\tptr last addr: 0x%x\n\tptr sects: %i\n", 
                 EXT_FLASH_EVTMGR_POINTERS_ADDR, 
                 EXT_FLASH_EVTMGR_POINTERS_LAST_ADDR, 
                 EXT_FLASH_EVTMGR_POINTERS_NUM_SECTORS);
    corona_print("\tblk addr: 0x%x\n\tblk last addr: 0x%x\n\tblk sects: %i\n", 
                 EXT_FLASH_EVENT_MGR_ADDR, 
                 EXT_FLASH_EVENT_MGR_LAST_ADDR,
                 EXT_FLASH_EVENT_MGR_NUM_SECTORS);

    while( num_spam_entries < MAX_SPAM_ENTRIES )
    {
        PROCESS_UINFO( ":!: [%u] - SPAM - YAY - SPAM - GOOD - SPAM - FUN - [%u] :!:", num_spam_entries, MAX_SPAM_ENTRIES );
        //_time_delay(10);
        //_time_delay(1000);
        num_spam_entries++;
        
        WMP_post_fw_info( UNNUMBERED_INFO, pBuf, "fake file", "fake function", 666);
        _time_delay(BIG_EVENT_SZ/8); // throttle delay based on size, so we don't run out of memory.
        
        if( 0 == (num_spam_entries % NUM_ENTRIES_BEFORE_GIVING_EVTMGR_TIME_TO_FLUSH) )
        {
        	EVTMGR_request_flush();
            _time_delay(BIG_EVENT_SZ/3);
            //_time_delay(5000);
        }
        
	#if 0
        /*
         *   Start a WIFI upload right in the middle of the upload for additional stress fun.
         */
        if( num_spam_entries == (MAX_SPAM_ENTRIES/2) )
        {
        	_EVTMGR_con_mgr_register_any(0, 0);
        }
	#endif
    }
    
    _lwmem_free(pBuf);
    
#if 1
    _EVTMGR_con_mgr_register_any(0, 0);
#endif
    
    //EVTMGR_request_flush();
    //PWRMGR_Reboot(1, PV_REBOOT_REASON_USER_REQUESTED, TRUE);
}
#endif
