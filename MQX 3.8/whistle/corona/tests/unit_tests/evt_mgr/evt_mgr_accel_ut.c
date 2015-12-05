/*
 * evt_mgr_accel_ut.c
 *
 *  Created on: Apr 2, 2013
 *      Author: Ernie
 */

#include "unit_tests.h"
#ifdef ENABLE_WHISTLE_UNIT_TESTS

#include "evt_mgr.h"
#include "corona_ext_flash_layout.h"
#include "wassert.h"

/*
 *   We want to test that even when we don't write exactly a number divisible by a page,
 *   the EVTMGR still does what it is supposed to do.
 *   We also want the total num_bytes_to_write a number divisible by 3, for the 3 axes.
 */
#define NUM_BYTES_WRITE     EXT_FLASH_PAGE_SIZE * 3 + (EXT_FLASH_PAGE_SIZE / 5) * 3
#define ACCEL_BUF_SZ        EXT_FLASH_PAGE_SIZE + 20    // We want buffer to be divisible by 3, so align on 3 axes (276 bytes)
#define AXIS_X  -14   // Center fake X data around -14
#define AXIS_Y  14    // Center fake Y data around 14
#define AXIS_Z  60    // Center fake Z data around 60

static uint8_t g_post_finished = 0;

/*
 *   Just writes some fake data for accel.  "len" must be divisible by 3.
 */
static void write_fake_data(int_8 * pBuf, uint_16 len)
{
    uint16_t i = 0;
    int_8 random_dummy = -2;
    
    if((len % 3) || (len == 0))
    {
        corona_print("ERROR: fake accel data length not divisible by 3!\n");
        fflush(stdout);
        return;
    }
    
    while(len)
    {
        random_dummy *= i * 3;
        random_dummy ^= i;
        random_dummy *= (i % 11);
        random_dummy &= ~0xE0; // keep it small
        random_dummy *= -1;    // toggle the sign.
        
        pBuf[i++] = AXIS_X + random_dummy;
        pBuf[i++] = AXIS_Y - random_dummy * 2;
        pBuf[i++] = AXIS_Z - random_dummy;
        len -= 3;
    }
}

void post_callback(err_t err, evt_obj_ptr_t pAccelObj)
{
    if(err == ERR_OK)
    {
        corona_print("Post finished with no errors.\n");
    }
    else
    {
        corona_print("ERROR:  EVTMGR_post() callback returned 0x%x\n", err);
    }
    fflush(stdout);
    g_post_finished = 1;
}

/*
 *   Populate fake accelerometer data, and send it to event manager.
 */
void evt_mgr_accel_ut(void)
{
    int_8 buf[ACCEL_BUF_SZ];
    evt_obj_t accelObj;
    uint_32 num_bytes_left = NUM_BYTES_WRITE;
    
    corona_print("evt_mgr_accel_ut = Write fake accel data to event manager.\n");
    corona_print("   Use 'dump' command in console to view evt mgr data.\n");
    fflush(stdout);
    
    accelObj.pPayload = (uint_8 *)buf;
    // TODO - The timestamp will part of the serialized payload itself, so not necessary here.
    //accelObj.timestamp_ticks = 0xFEEDBEEF; // don't support timestamp yet.
    
    while(num_bytes_left)
    {
        uint_16 len;
        if(num_bytes_left > ACCEL_BUF_SZ)
        {
            len = ACCEL_BUF_SZ;
        }
        else
        {
            len = num_bytes_left;
        }
        num_bytes_left -= len;
        write_fake_data(buf, len);
        accelObj.payload_len = len;
        
        wassert( ERR_OK == EVTMGR_post(&accelObj, post_callback) );
        while( ! g_post_finished )
        {
            _time_delay(10);
        }
        g_post_finished = 0;
    }
    
    /*
     *   Offloading this to the USB console "dump" command, since it is just going to be easier to test that way.
     */
    //wassert( ERR_OK == EVTMGR_dump(EVENT_DUMP_CONSOLE) );
    return;
}
#endif
