/*
 * con_mgr_ut.c
 *
 *  Created on: Mar 29, 2013
 *      Author: Chris
 */

#include "unit_tests.h"
#ifdef ENABLE_WHISTLE_UNIT_TESTS

#include "wifi_mgr.h"
#include "tpt_mgr.h"
#include "con_mgr.h"
#include "cfg_mgr.h"
#include "sys_event.h"
#include "app_errors.h"
#include "wassert.h"
#include "cormem.h"

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
static int open_client_id = 0;
static int close_client_id = 0;
#define MAX_CLIENTS 3
static CONMGR_handle_t handle[MAX_CLIENTS] = {0};


void connected(CONMGR_handle_t handle, boolean success)
{
    A_INT32 send_result;
    int_32 received;
    char *response;
    int client;
    err_t err;
    
    if (!success)
    {
        corona_print("CON_MGR_UT: Connect failed.\n");
        fflush(stdout);
        return;
    }

    corona_print("CON_MGR_UT: %p Connected!\n", handle);
    printf("CON_MGR_UT: Send some data...\n");
    fflush(stdout);
    
    /*
     *   Open a 'socket'
     */
    err = CONMGR_open(handle, HOST_TO_UPLOAD_DATA_TO, g_st_cfg.con_st.port);
    if(err != ERR_OK)
    {
        corona_print("CONMGR_open failed with code 0x%x\n", err);
        fflush(stdout);
        return;
    }
    
    // WARNING!!! For real clients, the following send/receive MUST go on a client thread. Right now, opened
    // is called on the MQX timer thread, which is BADDDDDD!!!!!
    err = CONMGR_send(handle, (uint_8 *) hello, strlen(hello), &send_result);
    if (send_result == strlen(hello))
    {
        corona_print("CON_MGR_UT: Sent!\n");
        fflush(stdout);
    }
    else
    {
        corona_print("CON_MGR_UT: CONMGR_send failed with code 0x%x\n", err);
        fflush(stdout);
        return;
    }
    
    corona_print("CON_MGR_UT: Receiving...\n");
    fflush(stdout);
    
    // Allocate buffer memory for the response
    response = walloc( MAX_WIFI_RX_BUFFER_SIZE );   
    
    err = CONMGR_receive(handle, (void *) response, MAX_WIFI_RX_BUFFER_SIZE, 0, &received, TRUE);
    if (ERR_OK == err)
    {
        corona_print("CON_MGR_UT: Recieved %d bytes:\n", received);
        response[received] = 0;
        corona_print("%s\n", response);
        fflush(stdout);
        CONMGR_zero_copy_free(handle, response);
    }
    else
    {
        corona_print("CON_MGR_UT: CONMGR_received failed with code 0x%x\n", err);
        fflush(stdout);
        return;
    }
    
    corona_print("CON_MGR_UT: Successfully processed Open\n");
    fflush(stdout);
    
    wfree(response);    
}

void disconnected(CONMGR_handle_t handle)
{
    corona_print("CON_MGR_UT: %p Disconnected!\n", handle);
    fflush(stdout);    
}

void con_mgr_disc_cb(sys_event_t sys_event)
{
        corona_print("CON_MGR_UT: Close client %p...\n", handle[close_client_id]);
        fflush(stdout);    
        
        if (0 == handle[close_client_id])
        {
            corona_print("CON_MGR_UT: Not opened.\n");
            fflush(stdout);       
            goto DONE;
        }

        CONMGR_release(handle[close_client_id]);
        handle[close_client_id] = 0;

        DONE:
        close_client_id++;
        close_client_id %= MAX_CLIENTS;
        return;
}
void con_mgr_connect_cb(sys_event_t sys_event)
{
    security_parameters_t security_parameters1;
    int w_rc;
    A_INT32 a_rc;
    int i;
    err_t e_rc;
    int timeout = 1;
    
    corona_print("CON_MGR_UT: Registering new client...\n");
    fflush(stdout);
    
    if (connectingState != 2)
    {
        corona_print("CON_MGR_UT: Not connected (0) or busy with previous registration(1)\n", connectingState);
        fflush(stdout);
        return;
    }
    
    
    if (0 != handle[open_client_id])
    {
        corona_print("CON_MGR_UT: Client busy.\n");
        fflush(stdout);       
        return;
    }

    connectingState = 1;
        
    e_rc = CONMGR_register_cbk(connected, disconnected, timeout, 0, CONMGR_CONNECTION_TYPE_ANY, open_client_id, &handle[open_client_id]);
    wassert(ERR_OK == e_rc);

    corona_print("CON_MGR_UT: Registered client %p successfully.\n", handle[open_client_id]);
    corona_print("CON_MGR_UT: Should see a connection in %d seconds...\n", timeout);
    fflush(stdout);

    open_client_id++;
    open_client_id %= MAX_CLIENTS;
    
    connectingState = 2;
}

void con_mgr_ut1(void)
{
    security_parameters_t security_parameters1;
    int w_rc;
    char *s;
    char addr[6];
    char *ssid1;
    
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
    
    w_rc = WIFIMGR_set_security(ssid1, &security_parameters1, WIFIMGR_SECURITY_FLUSH_NEVER);
    if (0 != w_rc)
    {
        corona_print("CON_MGR_UT: con_mgr_ut1: WIFIMGR_set_authentication failed %d\n", w_rc);
        goto ERROR_1;
    }
        
    corona_print("CON_MGR_UT: Push button with single-press to register a new callback.\n");
    corona_print("CON_MGR_UT: Push button with double-press to cancel/close callback.\n");
    fflush(stdout);
    
    sys_event_register_cbk(con_mgr_connect_cb, SYS_EVENT_BUTTON_SINGLE_PUSH);
    sys_event_register_cbk(con_mgr_disc_cb, SYS_EVENT_BUTTON_DOUBLE_PUSH);
    
    connectingState = 2;
    
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

void con_mgr_ut(void)
{
    con_mgr_ut1();
}
#endif
