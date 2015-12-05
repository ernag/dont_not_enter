/*
 * evt_mgr_ut.c
 *
 *  Created on: Mar 31, 2013
 *      Author: Chris
 */

#include "unit_tests.h"
#ifdef ENABLE_WHISTLE_UNIT_TESTS

#include "wifi_mgr.h"
#include "evt_mgr.h"
#include "app_errors.h"
#include "wassert.h"

static unsigned char g_test_post_done = 0;

static void test_callback(err_t err, evt_obj_ptr_t pObj)
{
    corona_print("test cbk err 0x%x. len: %u\n", err, pObj->payload_len);
    g_test_post_done = 1;
}

/*
 *   Unit test for COR-523.
 *   
 *   NOTE, you will need to not allow EVTMGR_post() to assert on zero len to
 *         run this test, which should fail on upload if a zero len payload is found.
 */
static void test_zero_len_COR_523(void)
{
    evt_obj_t test_obj;
    char *dummy = "dummy_data\n\0";
    int i = 10;
    
    test_obj.pPayload = (uint8_t *)dummy;
    test_obj.payload_len = strlen(dummy);
    
    wassert( ERR_OK == EVTMGR_post(&test_obj, test_callback) );
    while( g_test_post_done == 0 )
    {
        _time_delay(50);
    }
    g_test_post_done = 0;
    
    test_obj.payload_len = 0;
    
    while(i--)
    {
        /*
         *   Post a bunch of 0 payload shiz-nit.
         */
        wassert( ERR_OK == EVTMGR_post(&test_obj, test_callback) );
        while( g_test_post_done == 0 )
        {
            _time_delay(50);
        }
        g_test_post_done = 0;
    }
}

void evt_mgr_ut(void)
{
    security_parameters_t security_parameters1;
    int w_rc;
    char *s;
    char *ssid1;
    
    test_zero_len_COR_523();
    return;
    
    // ssid1 security parameters

#ifdef NATE
    ssid1 = "TP-LINK_2D071A";
    security_parameters1.type = SEC_MODE_WEP;
    security_parameters1.parameters.wep_parameters.key[0] = "1234567890";
    security_parameters1.parameters.wep_parameters.key[1] = "";
    security_parameters1.parameters.wep_parameters.key[2] = "";
    security_parameters1.parameters.wep_parameters.key[3] = "";
    security_parameters1.parameters.wep_parameters.default_key_index = 1; // 1 is key[0]
#else
    ssid1 = "Sasha";
    security_parameters1.type = SEC_MODE_WPA;
    security_parameters1.parameters.wpa_parameters.version = "wpa2";
    security_parameters1.parameters.wpa_parameters.passphrase = "walle~12";
    security_parameters1.parameters.wpa_parameters.ucipher = ATH_CIPHER_TYPE_CCMP;
    security_parameters1.parameters.wpa_parameters.mcipher = AES_CRYPT;
#endif  
    
    bu26507_display_test(1);
    _time_delay(100);
    bu26507_display_test(0);
    
    whistle_display_mac_addr();
      
    corona_print("WIFIMGR_set_security %s...\n", ssid1);
    fflush(stdout);
    
    w_rc = WIFIMGR_set_security(ssid1, &security_parameters1, WIFIMGR_SECURITY_FLUSH_NEVER);
    if (0 != w_rc)
    {
        corona_print("WIFIMGR_set_authentication failed %d\n", w_rc);
    }
}
#endif
