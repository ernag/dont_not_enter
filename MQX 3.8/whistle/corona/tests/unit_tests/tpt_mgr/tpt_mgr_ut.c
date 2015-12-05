/*
 * tpt_mgr.c
 *
 *  Created on: Mar 20, 2013
 *      Author: Chris
 */

#include "unit_tests.h"
#ifdef ENABLE_WHISTLE_UNIT_TESTS

#include "wifi_mgr.h"
#include "tpt_mgr.h"
#include "tpt_mgr_ut.h"
#include "sys_event.h"
#include "app_errors.h"

#if 1
#undef corona_print
#define corona_print
#endif

//#define NATE
#ifdef NATE
static char *hello =
        "\
POST /devices/Cheos1/data_pusher HTTP/1.1\r\n\
User-Agent: curl/7.24.0 (x86_64-apple-darwin12.0) libcurl/7.24.0 OpenSSL/0.9.8r zlib/1.2.5\r\n\
Host: 192.168.1.100\r\n\
Accept: */*\r\n\
Content-Type:whistle\r\n\
Content-Length: 1211\r\n\
Expect: 100-continue\r\n\
\r\n\
1\r\n\
;Title, http://www.gcdataconcepts.com, X16-1c\r\n\
;Version, 1409, Build num, 0x21E, Build date, 20120404 13:16:08,  SN:CCDC42011002237\r\n\
;Start_time, 2012-10-24, 03:29:25.409\r\n\
;Temperature, 35.00, deg C,  Vbat, 1681, mv\r\n\
;Gain, low\r\n\
;SampleRate, 50,Hz\r\n\
;Deadband, 0, counts\r\n\
;DeadbandTimeout, 1,sec\r\n\
;Headers, time,Ax,Ay,Az\r\n\
0.266,624,362,-849\r\n\
0.286,654,369,-959\r\n\
0.306,708,457,-967\r\n\
0.326,603,505,-773\r\n\
0.346,659,416,-779\r\n\
0.366,797,295,-857\r\n\
0.386,849,246,-799\r\n\
0.406,770,298,-832\r\n\
0.426,647,375,-925\r\n\
0.446,504,458,-974\r\n\
0.466,425,486,-918\r\n\
0.486,447,437,-913\r\n\
0.506,519,359,-919\r\n\
0.526,604,320,-829\r\n\
0.545,682,312,-824\r\n\
0.566,713,352,-920\r\n\
0.585,670,399,-933\r\n\
0.606,566,393,-906\r\n\
0.626,500,426,-852\r\n\
0.646,479,441,-934\r\n\
0.666,490,386,-973\r\n\
0.685,510,355,-944\r\n\
0.706,570,356,-977\r\n\
0.725,569,389,-994\r\n\
0.746,516,404,-948\r\n\
0.765,491,388,-943\r\n\
0.786,473,354,-936\r\n\
0.805,476,346,-926\r\n\
0.826,500,360,-912\r\n\
0.845,499,342,-901\r\n\
0.865,494,309,-871\r\n\
0.885,539,283,-873\r\n\
0.905,585,281,-951\r\n\
0.925,540,275,-977\r\n\
0.945,444,349,-1034\r\n\
0.965,453,326,-1173\r\n\
0.985,504,291,-1070\r\n\
1.005,468,323,-966\r\n\
1.025,503,304,-986\r\n\
1.044,544,252,-1018\r\n\
1.065,480,240,-1039\r\n\
1.084,395,255,-967\r\n\
1.105,385,307,-1000\r\n\
1.124,437,251,-1060\r\n";
#else
static char *hello = "\
POST /devices/Cheos1/data_pusher HTTP/1.1\r\n\
User-Agent: curl/7.24.0 (x86_64-apple-darwin12.0) libcurl/7.24.0 OpenSSL/0.9.8r zlib/1.2.5\r\n\
Host: staging.whistle.com\r\n\
Accept: */*\r\n\
Content-Type:whistle\r\n\
Content-Length: 1211\r\n\
Expect: 100-continue\r\n\
\r\n\
1\r\n\
;Title, http://www.gcdataconcepts.com, X16-1c\r\n\
;Version, 1409, Build num, 0x21E, Build date, 20120404 13:16:08,  SN:CCDC42011002237\r\n\
;Start_time, 2012-10-24, 03:29:25.409\r\n\
;Temperature, 35.00, deg C,  Vbat, 1681, mv\r\n\
;Gain, low\r\n\
;SampleRate, 50,Hz\r\n\
;Deadband, 0, counts\r\n\
;DeadbandTimeout, 1,sec\r\n\
;Headers, time,Ax,Ay,Az\r\n\
0.266,624,362,-849\r\n\
0.286,654,369,-959\r\n\
0.306,708,457,-967\r\n\
0.326,603,505,-773\r\n\
0.346,659,416,-779\r\n\
0.366,797,295,-857\r\n\
0.386,849,246,-799\r\n\
0.406,770,298,-832\r\n\
0.426,647,375,-925\r\n\
0.446,504,458,-974\r\n\
0.466,425,486,-918\r\n\
0.486,447,437,-913\r\n\
0.506,519,359,-919\r\n\
0.526,604,320,-829\r\n\
0.545,682,312,-824\r\n\
0.566,713,352,-920\r\n\
0.585,670,399,-933\r\n\
0.606,566,393,-906\r\n\
0.626,500,426,-852\r\n\
0.646,479,441,-934\r\n\
0.666,490,386,-973\r\n\
0.685,510,355,-944\r\n\
0.706,570,356,-977\r\n\
0.725,569,389,-994\r\n\
0.746,516,404,-948\r\n\
0.765,491,388,-943\r\n\
0.786,473,354,-936\r\n\
0.805,476,346,-926\r\n\
0.826,500,360,-912\r\n\
0.845,499,342,-901\r\n\
0.865,494,309,-871\r\n\
0.885,539,283,-873\r\n\
0.905,585,281,-951\r\n\
0.925,540,275,-977\r\n\
0.945,444,349,-1034\r\n\
0.965,453,326,-1173\r\n\
0.985,504,291,-1070\r\n\
1.005,468,323,-966\r\n\
1.025,503,304,-986\r\n\
1.044,544,252,-1018\r\n\
1.065,480,240,-1039\r\n\
1.084,395,255,-967\r\n\
1.105,385,307,-1000\r\n\
1.124,437,251,-1060\r\n";
#endif

static int connectingState = 0;

void tpt_mgr_button_cb(sys_event_t sys_event)
{
    A_INT32 send_result;
    int_32 received;
    int sock_peer; /* Foreign socket.*/
    char *buffer_ptr;
    security_parameters_t security_parameters1;
    int w_rc;
    A_INT32 a_rc;
    int i;
    err_t err;
    
#ifdef NATE
    char *server = "192.168.1.100"; // Nate
    int port = 3000;
#else
    char *server = "107.20.162.205"; // whistle
    int port = 80;
#endif
       
    if (connectingState != 2)
        return;

    buffer_ptr = _lwmem_alloc( MAX_WIFI_RX_BUFFER_SIZE );
    wassert( buffer_ptr != NULL );
       
    connectingState = 1;
    
    bu26507_display_test(1);
    _time_delay(100);
    bu26507_display_test(0);

    
    corona_print("WIFIMGR_open(%s:%d)...\n", server, port);
    fflush(stdout);
    
    w_rc = WIFIMGR_open(server, port, &sock_peer, SOCK_STREAM_TYPE); //Open TCP socket
    if (ERR_OK != w_rc)
    {
        corona_print("WIFIMGR_open failed %d\n", w_rc);
        goto ERROR_1;
    }
    
    corona_print("Done\n");
    fflush(stdout);
    
    /* Sending.*/corona_print("Sending.\n");
    fflush(stdout);
    
    err = WIFIMGR_send(sock_peer, (uint_8 *) hello, strlen(hello), &send_result);
    if (send_result == strlen(hello))
    {
        corona_print("Sent!\n");
    }
    else
    {
        corona_print("WIFIMGR_send failed with code 0x%x\n", err);
        goto ERROR_1;
    }
    
    corona_print("Receiving.\n");
    fflush(stdout);
    
    err = WIFIMGR_receive(sock_peer, (void *) buffer_ptr, MAX_WIFI_RX_BUFFER_SIZE, 0, &received, TRUE);
    if (received > 0)
    {
        corona_print("Recieved %d bytes:\n", received);
        buffer_ptr[received] = 0;
        corona_print("%s\n", buffer_ptr);
        fflush(stdout);
        WIFIMGR_zero_copy_free(buffer_ptr);
    }
    else
    {
        corona_print("WIFIMGR_received failed with code 0x%x\n", err);
        goto ERROR_1;
    }
    
    bu26507_display_test(1);
    _time_delay(100);
    bu26507_display_test(0);
    _time_delay(100);
    bu26507_display_test(1);
    _time_delay(100);
    bu26507_display_test(0);
    
    goto DONE;

    ERROR_1:
    bu26507_display_test(1);
    _time_delay(100);
    bu26507_display_test(0);
    _time_delay(100);
    bu26507_display_test(1);
    _time_delay(100);
    bu26507_display_test(0);
    _time_delay(100);
    bu26507_display_test(1);
    _time_delay(100);
    bu26507_display_test(0);
    
    DONE:
    wassert( MQX_OK == _lwmem_free(buffer_ptr) );    
    fflush(stdout);
    connectingState = 2;
}

void tpt_mgr_ut2(void)
{
    security_parameters_t security_parameters1;
    int w_rc;
    char *s;
    char *ssid1;
    err_t e_rc;
    TPTMGR_transport_type_t actual_transport_type;
    
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
    
#if 0
    s = malloc(strlen(hello) + 1);
    scorona_print(s, hello, addr);
    strcpy(hello, s);
    free(s);
#endif
    
    corona_print("WIFIMGR_set_security %s...\n", ssid1);
    fflush(stdout);
    
    w_rc = WIFIMGR_set_security(ssid1, &security_parameters1, WIFIMGR_SECURITY_FLUSH_NEVER);
    if (0 != w_rc)
    {
        corona_print("WIFIMGR_set_authentication failed %d\n", w_rc);
        goto ERROR_1;
    }
    
    do {
        corona_print("TPTMGR_connect(TPT_ANY)...\n");
        fflush(stdout);
        
        e_rc = TPTMGR_connect(TPTMGR_TRANSPORT_TYPE_ANY, &actual_transport_type);

        if (0 != w_rc)
        {
            corona_print("TPTMGR_connect(TPT_ANY) failed %d\n", w_rc);
            fflush(stdout);
        }
    } while (0 != w_rc);
    
    corona_print("Done.\n");
    fflush(stdout);
    
    sys_event_register_cbk(tpt_mgr_button_cb, SYS_EVENT_BUTTON_SINGLE_PUSH);
    
    connectingState = 2;
    
    bu26507_display_test(1);
    _time_delay(1000);
    bu26507_display_test(0);
    
//#define TEST_DISC
#ifdef TEST_DISC
    corona_print("TPTMGR_disconnect...\n");
    fflush(stdout);

    w_rc = TPTMGR_disconnect();
    if (0 != w_rc)
    {   
        corona_print("TPTMGR_disconnect failed %d\n", w_rc);
        goto ERROR_1;
    }

    corona_print("Done.\n");
    fflush(stdout);
#endif
    
    ERROR_1:
    fflush(stdout);
}
#endif
