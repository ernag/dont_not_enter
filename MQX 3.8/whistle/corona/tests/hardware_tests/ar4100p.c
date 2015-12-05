/*
 * ar4100p.c
 *
 *  Created on: Mar 6, 2013
 *      Author: Chris
 */

#include "unit_tests.h"
#ifdef ENABLE_WHISTLE_UNIT_TESTS

#include <ar4100p_main.h>

static boolean isConnected = FALSE;

static LWEVENT_STRUCT ar4100_lwevent;
#define CONNECTED_EVENT    1

/*FUNCTION*-----------------------------------------------------------------
 *
 * Function Name  : wifiCallback
 * Returned Value : N/A
 * Comments       : Called from driver on a WiFI connection event
 *
 * END------------------------------------------------------------------*/
static void wifiCallback(int val)
{
    if (val == A_TRUE)
    {
        isConnected = TRUE;
    }
    else
    {
        isConnected = FALSE;
    }

    _lwevent_set(&ar4100_lwevent, CONNECTED_EVENT);
}


int ar4100p_test(void)
{
    A_INT32   return_code;
    _mqx_uint result;

    whistle_set_callback_with_driver(wifiCallback);

    /* Create the lightweight semaphore */
    result = _lwevent_create(&ar4100_lwevent, LWEVENT_AUTO_CLEAR);
    if (result != MQX_OK)
    {
        return -4;
    }

#if 0
    whistle_mac_store("00037f04e3df");
#endif

#if 1
    whistle_display_mac_addr();
#endif

    get_version();

    return_code = whistle_set_passphrase("walle~12");
    if (A_OK != return_code)
    {
        corona_print("set_passphrase failed %x", return_code);
        return -1;
    }

    return_code = whistle_set_wpa("wpa2", ATH_CIPHER_TYPE_CCMP, AES_CRYPT);
    if (A_OK != return_code)
    {
        corona_print("set_wpa failed %x", return_code);
        return -2;
    }

    return_code = whistle_connect_handler("TP-LINK_2D071A");
    if (A_OK != return_code)
    {
        corona_print("connect_handler failed %x", return_code);
        return -3;
    }

    fflush(stdout);

    if (MQX_OK != _lwevent_wait_for(&ar4100_lwevent, CONNECTED_EVENT, FALSE, 0))
    {
        corona_print("_lwevent_wait_for failed\n");
        return -5;
    }

    fflush(stdout);

    return_code = block_on_dhcp();
    if (A_OK != return_code)
    {
        corona_print("block_on_dhcp failed\n");
    }

    while (1)
    {
        whistle_wmi_ping("192.168.1.1", 1, 64);
        fflush(stdout);
        _time_delay(1000);
    }

    return 0;
}
#endif
