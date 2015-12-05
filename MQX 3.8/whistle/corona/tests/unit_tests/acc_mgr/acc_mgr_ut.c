/*
 * acc_mgr_ut.c
 *
 *  Created on: Apr 2, 2013
 *      Author: Ben McCrea
 */

#include "unit_tests.h"
#ifdef ENABLE_WHISTLE_UNIT_TESTS

#include <mqx.h>
#include <bsp.h>
#include "app_errors.h"
#include "acc_mgr.h"
#include "accel_hal.h"
#include "accdrv_spi_dma.h"

void acc_mgr_ut( void )
{   
    int i;
    
    while ( !ACCMGR_is_ready() )
        _time_delay(100);
    
    corona_print("ACCMGR Unit Test\n\n");
    corona_print("Starting up..\n");
    fflush(stdout);
    ACCMGR_start_up();  
    
    while ( !ACCMGR_is_snoozing() )
    {
        _time_delay(100);
    }
    corona_print("Simulating motion detect..\n");
    accdrv_simulate_wakeup();
     
    _time_delay(8000);
    
    corona_print("Shutting down ACCMGR..\n");
    ACCMGR_shut_down( FALSE );  
    while (!ACCMGR_is_shutdown() )
    {
        _time_delay(150);
    }
    corona_print("ACCMGR is shut down.\n");
    for (i=0;i<2;i++)
    {
        corona_print("Starting it up again..\n");
        ACCMGR_start_up();  
        
        while ( !ACCMGR_is_snoozing() )
        {
            _time_delay(100);
        }
        corona_print("Simulating motion detect..\n"); 
        accdrv_simulate_wakeup();
        
        _time_delay(8000);
        corona_print("Shutting down ACCMGR..\n");
        ACCMGR_shut_down( FALSE );  
        while (!ACCMGR_is_shutdown() )
        {
            _time_delay(150);
        }
        corona_print("ACCMGR is shut down.\n");
    }
        
    corona_print("acc_mgr_ut: done!\n");
    
#if 0
// Simulated Wakeups
    // 5 small events
    corona_print("Five short motion events..\n");
    for (i=0;i<5;i++)
    {
        // Send a fake motion detect event
        accdrv_simulate_wakeup();
        do
        {
            _time_delay(3000);
        } while ( !ACCMGR_is_snoozing() );
    }

    
    // 30s event
    corona_print("One 30s motion event..\n");
    fflush(stdout);
    for (i=0;i<30;i++)
    {
        accdrv_simulate_wakeup(); 
        _time_delay(1000);
    }
    
    _time_delay(6000); // wait for logging to stop
    
    // One very long event
    corona_print("One 5m motion event..\n");
    fflush(stdout);
    for (i=0;i<300;i++)
    {
        accdrv_simulate_wakeup();
        _time_delay(1000);
    }
    
    _time_delay(6000); // wait 6 seconds
    corona_print("Shutting down..\n");
    fflush(stdout);
    
    // Shut down
    ACCMGR_shut_down( FALSE );
    
    _time_delay(3000); // Wait for last packet before displaying statistics
    corona_print("Done.\n");
    fflush(stdout);
#endif
}
#endif
