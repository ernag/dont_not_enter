/*
 * i2c1.c
 * 
 *  Helper utility for arbitrating access to i2c1 bus.
 *   For Whistle Corona device, MFI and ROHM both share the i2c1 bus.
 *
 *  Created on: May 16, 2013
 *      Author: Ernie
 */

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include <mqx.h>
#include <bsp.h>
#include <i2c.h>
#include "wmutex.h"
#include "wassert.h"
#include "i2c1.h"

////////////////////////////////////////////////////////////////////////////////
// Defines
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Variables
////////////////////////////////////////////////////////////////////////////////
static MUTEX_STRUCT g_i2c1_mutex;
static MQX_FILE_PTR g_local_i2c_hdl = NULL;
static uint8_t g_i2c1_takeover = 0;

////////////////////////////////////////////////////////////////////////////////
// Code
////////////////////////////////////////////////////////////////////////////////

/*
 *   Init the I2C1 Whistle Arbitrator (TM).
 */
void I2C1_init( void )
{
    wmutex_init(&g_i2c1_mutex);
}

/*
 *   Call this when you want to ignore typical I2C mutual exclusion courtesy.
 *     This may be desirable in a "we're F'd" type of state.
 */
void I2C1_take_over(void)
{
    _mutex_try_lock(&g_i2c1_mutex);
    g_i2c1_takeover = 1;
}

/*
 *   Obtain 'dibbs' on the I2C bus.
 *   http://www.urbandictionary.com/define.php?term=dibbs
 * 
 *   Arbitrate I2C1 bus access, and allow only 1 master at a time.
 *      This will prevent mutex issues to the I2C1 bus, since it is shared
 *      by the ROHM (display) chip, and the MFI (Apple's crappy BT chip).
 * 
 *   WARNING:  This will block until handle is received !
 */
err_t I2C1_dibbs( MQX_FILE_PTR *pI2C1Handle )
{
    /*
     *   Wait to make sure we don't crap all over the I2C1 bus while
     *      the other guy is using it.
     */
    if(!g_i2c1_takeover) { wmutex_lock( &g_i2c1_mutex); }
        
#if BSPCFG_ENABLE_I2C1
    /*
     *   When BT_ENABLED, we use the polled version.
     */
    *pI2C1Handle = fopen( "i2c1:", NULL );
#elif BSPCFG_ENABLE_II2C1
    *pI2C1Handle = fopen( "ii2c1:", NULL );
#else
    #error "I2C1 is not enabled in the BSP!"
#endif
    
    if( NULL == *pI2C1Handle)
    {
        wassert( g_local_i2c_hdl );
        *pI2C1Handle = g_local_i2c_hdl;
    }
    else
    {
        g_local_i2c_hdl = *pI2C1Handle;
    }
    
    return ERR_OK;
}

/*
 *    The opposite of 'dibbs', you are giving up your control of the I2C1 bus.
 */
err_t I2C1_liberate( MQX_FILE_PTR *pI2C1Handle )
{
    wassert( MQX_OK == fclose( *pI2C1Handle ) );
    *pI2C1Handle = NULL;
    g_local_i2c_hdl = NULL;
    if(!g_i2c1_takeover) { wmutex_unlock( &g_i2c1_mutex ); }
    return ERR_OK;
}

