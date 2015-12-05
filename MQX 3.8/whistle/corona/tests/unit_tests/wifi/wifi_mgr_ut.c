/*
 * wifi_mgr_ut.c
 *
 *  Created on: Mar 14, 2013
 *      Author: Chris
 */

#include "unit_tests.h"
#ifdef ENABLE_WHISTLE_UNIT_TESTS

#include "ar4100p_main.h"
#include "wifi_ut.h"
#include "wifi_mgr.h"
#include "wifi_mgr_ut.h"
#include "app_errors.h"
#include "../shared_ut_config.h"

static char *hello = "\
POST /devices/Cheos1/data_pusher HTTP/1.1\r\n\
User-Agent: curl/7.24.0 (x86_64-apple-darwin12.0) libcurl/7.24.0 OpenSSL/0.9.8r zlib/1.2.5\r\n\
Host: dev.whistle.com\r\n\
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
1.124,437,251,-1060\r\n\
POST /devices/Cheos1/data_pusher HTTP/1.1\r\n\
User-Agent: curl/7.24.0 (x86_64-apple-darwin12.0) libcurl/7.24.0 OpenSSL/0.9.8r zlib/1.2.5\r\n\
Host: dev.whistle.com\r\n\
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
//1.145,436,198,-1086\r\n\
// //1.164,363,231,-1000\r\n\
// //1.185,410,278,-982\r\n\
// //1.204,505,239,-1025\r\n\
// //1.224,489,238,-1052\r\n\
// //1.244,411,285,-1029";

static char peer0_0[] = {
0x50, 0x4f, 0x53, 0x54, 0x20, 0x2f, 0x64, 0x65, 
0x76, 0x69, 0x63, 0x65, 0x73, 0x2f, 0x43, 0x4f, 
0x52, 0x2d, 0x30, 0x30, 0x30, 0x30, 0x31, 0x37, 
0x2f, 0x6d, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65, 
0x73, 0x20, 0x48, 0x54, 0x54, 0x50, 0x2f, 0x31, 
0x2e, 0x31, 0x0d, 0x0a, 0x55, 0x73, 0x65, 0x72, 
0x2d, 0x41, 0x67, 0x65, 0x6e, 0x74, 0x3a, 0x20, 
0x77, 0x68, 0x69, 0x73, 0x74, 0x6c, 0x65, 0x20, 
0x0d, 0x0a, 0x48, 0x6f, 0x73, 0x74, 0x3a, 0x20, 
0x73, 0x74, 0x61, 0x67, 0x69, 0x6e, 0x67, 0x2e, 
0x77, 0x68, 0x69, 0x73, 0x74, 0x6c, 0x65, 0x2e, 
0x63, 0x6f, 0x6d, 0x3a, 0x38, 0x30, 0x0d, 0x0a, 
0x43, 0x6f, 0x6e, 0x74, 0x65, 0x6e, 0x74, 0x2d, 
0x54, 0x79, 0x70, 0x65, 0x3a, 0x20, 0x77, 0x68, 
0x69, 0x73, 0x74, 0x6c, 0x65, 0x0d, 0x0a, 0x41, 
0x63, 0x63, 0x65, 0x70, 0x74, 0x3a, 0x20, 0x2a, 
0x2f, 0x2a, 0x0d, 0x0a, 0x43, 0x6f, 0x6e, 0x6e, 
0x65, 0x63, 0x74, 0x69, 0x6f, 0x6e, 0x3a, 0x20, 
0x63, 0x6c, 0x6f, 0x73, 0x65, 0x0d, 0x0a, 0x43, 
0x6f, 0x6e, 0x74, 0x65, 0x6e, 0x74, 0x2d, 0x4c, 
0x65, 0x6e, 0x67, 0x74, 0x68, 0x3a, 0x20, 0x31, 
0x34, 0x36, 0x33, 0x34, 0x0d, 0x0a, 0x0d, 0x0a };
static char peer0_1[] = {
0x08, 0x01, 0x10, 0x01, 0x1d, 0x9a, 0xe2, 0x01, 
0x00, 0x22, 0x0a, 0x43, 0x4f, 0x52, 0x2d, 0x30, 
0x30, 0x30, 0x30, 0x31, 0x37, 0x2a, 0x92, 0x72, 
0x08, 0xf2, 0xc8, 0x99, 0x02 };
static char peer0_2[] = {
0xa2, 0x01, 0xb1, 0x0b, 0x10, 0xb6, 0xf3, 0xcc, 
0x01, 0x18, 0x32, 0x20, 0x01, 0xa2, 0x01, 0xa0, 
0x0b, 0xff, 0x00, 0x10, 0xff, 0x00, 0x10, 0xfe, 
0xff, 0x0f, 0xfe, 0x00, 0x10, 0xfe, 0x00, 0x10, 
0xfe, 0xff, 0x0f, 0xfe, 0xff, 0x10, 0xfe, 0xff, 
0x10, 0xff, 0xff, 0x10, 0xfe, 0xff, 0x10, 0xfe, 
0xff, 0x10, 0xfe, 0xff, 0x0f, 0xfe, 0x00, 0x0f, 
0xfe, 0x00, 0x10, 0xfe, 0xff, 0x0f, 0xfe, 0x00, 
0x0f, 0xff, 0xff, 0x0f, 0xfe, 0x00, 0x10, 0xfe, 
0xff, 0x10, 0xfe, 0xff, 0x0f, 0xff, 0xff, 0x0f, 
0xff, 0xff, 0x0f, 0xfe, 0xff, 0x10, 0xfe, 0x00, 
0x0f, 0xfe, 0xff, 0x0f, 0xfe, 0x00, 0x10, 0xfe, 
0xff, 0x10, 0xfe, 0x00, 0x10, 0xfe, 0xff, 0x10, 
0xfe, 0xff, 0x0f, 0xff, 0xff, 0x0f, 0xfe, 0x00, 
0x10, 0xfe, 0x00, 0x10, 0xfe, 0xff, 0x0f, 0xfe, 
0xff, 0x0f, 0xfe, 0x00, 0x10, 0xff, 0x00, 0x0f, 
0xfe, 0xff, 0x10, 0xff, 0xff, 0x10, 0xfe, 0xff, 
0x10, 0xfe, 0xff, 0x0f, 0xfe, 0xff, 0x0f, 0xfe, 
0xff, 0x10, 0xfe, 0xff, 0x0f, 0xfe, 0xff, 0x10, 
0xfe, 0x00, 0x10, 0xfe, 0xff, 0x0f, 0xfe, 0xff, 
0x10, 0xfe, 0xff, 0x10, 0xff, 0xff, 0x0f, 0xfe, 
0xff, 0x0f, 0xfe, 0x00, 0x0f, 0xfe, 0xff, 0x0f, 
0xff, 0xff, 0x10, 0xfe, 0xff, 0x0f, 0xff, 0x00, 
0x10, 0xfe, 0xff, 0x10, 0xfe, 0x00, 0x0f, 0xfe, 
0xff, 0x10, 0xff, 0xff, 0x10, 0xff, 0xff, 0x10, 
0xff, 0xff, 0x10, 0xff, 0xff, 0x10, 0xfe, 0x00, 
0x0f, 0xff, 0xff, 0x10, 0xfe, 0xff, 0x0f, 0xfe, 
0xff, 0x10, 0xfe, 0x00, 0x10, 0xfe, 0xff, 0x10, 
0xfe, 0xff, 0x10, 0xfe, 0xff, 0x10, 0xfe, 0xff, 
0x10, 0xfe, 0xff, 0x10, 0xfe, 0xff, 0x10, 0xfe, 
0x00, 0x0f, 0xfe, 0x00, 0x10, 0xfe, 0x00, 0x10, 
0xff, 0xff, 0x0f, 0xfe, 0xff, 0x0f, 0xfe, 0xff, 
0x10, 0xfe, 0xff, 0x10, 0xfe, 0xff, 0x10, 0xfe, 
0xff, 0x10, 0xfe, 0xff, 0x10, 0xfe, 0xff, 0x10, 
0xff, 0xff, 0x0f, 0xfe, 0x00, 0x0f, 0xfe, 0x00, 
0x0f, 0xfe, 0xff, 0x10, 0xfe, 0xff, 0x0f, 0xfe, 
0xff, 0x10, 0xfd, 0xff, 0x0f, 0xfe, 0xff, 0x0f, 
0xff, 0xff, 0x0f, 0xfe, 0x00, 0x0f, 0xfe, 0x00, 
0x0f, 0xfe, 0xff, 0x0f, 0xff, 0xff, 0x10, 0xfe, 
0xff, 0x0f, 0xfe, 0xff, 0x0f, 0xfe, 0xff, 0x10, 
0xfe, 0xff, 0x10, 0xfe, 0xff, 0x10, 0xff, 0xff, 
0x10, 0xff, 0xff, 0x10, 0xfe, 0xff, 0x0f, 0xfe, 
0xff, 0x0f, 0xfe, 0x00, 0x10, 0xfe, 0xff, 0x10, 
0xfe, 0xff, 0x10, 0xfe, 0xff, 0x10, 0xfe, 0xff, 
0x10, 0xfe, 0xff, 0x0f, 0xfe, 0xff, 0x10, 0xfe, 
0xff, 0x0f, 0xfe, 0xff, 0x10, 0xfe, 0xff, 0x0f, 
0xfe, 0xff, 0x0f, 0xff, 0xff, 0x0f, 0xfe, 0xff, 
0x10, 0xfe, 0xff, 0x10, 0xfe, 0xff, 0x10, 0xfe, 
0xff, 0x10, 0xfe, 0xff, 0x10, 0xfe, 0xff, 0x0f, 
0xfe, 0xff, 0x0f, 0xfe, 0xff, 0x10, 0xfe, 0xff, 
0x10, 0xff, 0xff, 0x0f, 0xfe, 0xff, 0x10, 0xfe, 
0xff, 0x0f, 0xfe, 0x00, 0x0f, 0xfe, 0xff, 0x10, 
0xff, 0xff, 0x0f, 0xfe, 0xff, 0x0f, 0xfe, 0xff, 
0x10, 0xfe, 0xff, 0x0f, 0xfe, 0x00, 0x0f, 0xfe, 
0xff, 0x0f, 0xfe, 0xff, 0x10, 0xfd, 0x00, 0x0f, 
0xfd, 0x00, 0x0f, 0xfe, 0xff, 0x10, 0xff, 0xff, 
0x0f, 0xfe, 0x00, 0x10, 0xfe, 0x00, 0x0f, 0xfe, 
0x00, 0x10, 0xff, 0xff, 0x0f, 0xff, 0xff, 0x10, 
0xff, 0xff, 0x0f, 0xfe, 0xff, 0x0f, 0xfe, 0xff, 
0x10, 0xfe, 0xff, 0x10, 0xfe, 0x00, 0x0f, 0xfe, 
0x00, 0x0f, 0xff, 0xff, 0x10, 0xfe, 0xff, 0x0f, 
0xfe, 0x00, 0x10, 0xfe, 0x00, 0x0f, 0xfe, 0xff, 
0x10, 0xfe, 0xff, 0x10 };

/************************************************************************
* NAME: wifi_mgr_ut
*
* DESCRIPTION: Start TCP Transmit test.
************************************************************************/
void wifi_mgr_ut(void)
{
    A_INT32               send_result;
    int_32                received;
    int                   sock_peer; /* Foreign socket.*/
    char                  *buffer_ptr;
    security_parameters_t security_parameters1, security_parameters2;
    int                   w_rc;
    A_INT32               a_rc;
    A_UINT8               temp_ssid[33];
    int                   i;
    ENET_SCAN_LIST        scan_list;
    ATH_GET_SCAN          ext_scan_list;
    char                  query_ssid[MAX_SSID_LENGTH];
    err_t                 err;

    char *ssid1 = "Sasha";
    //char *ssid1 = "CJ_Network-guest";
    char *ssid2 = "TP-LINK_2D071A";
    //char *server = "54.243.90.245"; // whistle
    char *server = "192.168.168.133"; // Kevin's server
    //char *server = "74.125.224.101"; // google
    //int port = 80;
    int port = 3000;

    corona_print("WIFIMGR_scan...\n");
    fflush(stdout);

    w_rc = WIFIMGR_scan(&scan_list);
    if (0 != w_rc)
    {
        corona_print("WIFIMGR_scan failed %d\n", w_rc);
        goto ERROR_1;
    }

    for (i = 0; i < scan_list.num_scan_entries; i++)
    {
        A_UINT8 j = MAX_RSSI_TABLE_SZ - 1;

        memcpy(temp_ssid, scan_list.scan_info_list[i].ssid, scan_list.scan_info_list[i].ssid_len);

        temp_ssid[scan_list.scan_info_list[i].ssid_len] = '\0';

        if (scan_list.scan_info_list[i].ssid_len == 0)
        {
            corona_print("ssid = SSID Not available\n");
        }
        else
        {
            corona_print("ssid = %s\n", temp_ssid);
        }

        corona_print("bssid = %.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n", scan_list.scan_info_list[i].bssid[0], scan_list.scan_info_list[i].bssid[1], scan_list.scan_info_list[i].bssid[2], scan_list.scan_info_list[i].bssid[3], scan_list.scan_info_list[i].bssid[4], scan_list.scan_info_list[i].bssid[5]);
        corona_print("channel = %d\n", scan_list.scan_info_list[i].channel);

        corona_print("indicator = %d\n", scan_list.scan_info_list[i].rssi);
        corona_print("\n\r");
    }


    corona_print("Done.\n");
    fflush(stdout);

    corona_print("WIFIMGR_ext_scan...\n");
    fflush(stdout);

    err = WIFIMGR_ext_scan(&ext_scan_list, FALSE);
    if (0 != err)
    {
        corona_print("WIFIMGR_ext_scan failed %d\n", w_rc);
        goto ERROR_1;
    }


    for (i = 0; i < ext_scan_list.num_entries; i++)
    {
        A_UINT8 j = MAX_RSSI_TABLE_SZ - 1;

        memcpy(temp_ssid, ext_scan_list.scan_list[i].ssid, ext_scan_list.scan_list[i].ssid_len);

        temp_ssid[ext_scan_list.scan_list[i].ssid_len] = '\0';

        if (ext_scan_list.scan_list[i].ssid_len == 0)
        {
            corona_print("ssid = SSID Not available\n");
        }
        else
        {
            corona_print("ssid = %s\n", temp_ssid);
        }

        corona_print("bssid = %.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n", ext_scan_list.scan_list[i].bssid[0], ext_scan_list.scan_list[i].bssid[1], ext_scan_list.scan_list[i].bssid[2], ext_scan_list.scan_list[i].bssid[3], ext_scan_list.scan_list[i].bssid[4], ext_scan_list.scan_list[i].bssid[5]);
        corona_print("channel = %d\n", ext_scan_list.scan_list[i].channel);

        corona_print("indicator = %d\n", ext_scan_list.scan_list[i].rssi);
        corona_print("security = ");

        if (ext_scan_list.scan_list[i].security_enabled)
        {
            if (ext_scan_list.scan_list[i].rsn_auth || ext_scan_list.scan_list[i].rsn_cipher)
            {
                corona_print("\n\r");
                corona_print("RSN/WPA2= ");
            }

            if (ext_scan_list.scan_list[i].rsn_auth)
            {
                corona_print(" {");
                if (ext_scan_list.scan_list[i].rsn_auth & SECURITY_AUTH_1X)
                {
                    corona_print("802.1X ");
                }

                if (ext_scan_list.scan_list[i].rsn_auth & SECURITY_AUTH_PSK)
                {
                    corona_print("PSK ");
                }
                corona_print("}");
            }

            if (ext_scan_list.scan_list[i].rsn_cipher)
            {
                corona_print(" {");

                /* AP security can support multiple options hence
                 * we check each one separately. Note rsn == wpa2 */
                if (ext_scan_list.scan_list[i].rsn_cipher & ATH_CIPHER_TYPE_WEP)
                {
                    corona_print("WEP ");
                }

                if (ext_scan_list.scan_list[i].rsn_cipher & ATH_CIPHER_TYPE_TKIP)
                {
                    corona_print("TKIP ");
                }

                if (ext_scan_list.scan_list[i].rsn_cipher & ATH_CIPHER_TYPE_CCMP)
                {
                    corona_print("AES ");
                }
                corona_print("}");
            }

            if (ext_scan_list.scan_list[i].wpa_auth || ext_scan_list.scan_list[i].wpa_cipher)
            {
                corona_print("\n\r");
                corona_print("WPA= ");
            }

            if (ext_scan_list.scan_list[i].wpa_auth)
            {
                corona_print(" {");
                if (ext_scan_list.scan_list[i].wpa_auth & SECURITY_AUTH_1X)
                {
                    corona_print("802.1X ");
                }

                if (ext_scan_list.scan_list[i].wpa_auth & SECURITY_AUTH_PSK)
                {
                    corona_print("PSK ");
                }
                corona_print("}");
            }

            if (ext_scan_list.scan_list[i].wpa_cipher)
            {
                corona_print(" {");
                if (ext_scan_list.scan_list[i].wpa_cipher & ATH_CIPHER_TYPE_WEP)
                {
                    corona_print("WEP ");
                }

                if (ext_scan_list.scan_list[i].wpa_cipher & ATH_CIPHER_TYPE_TKIP)
                {
                    corona_print("TKIP ");
                }

                if (ext_scan_list.scan_list[i].wpa_cipher & ATH_CIPHER_TYPE_CCMP)
                {
                    corona_print("AES ");
                }
                corona_print("}");
            }

            /* it may be old-fashioned WEP this is identified by
             * absent wpa and rsn ciphers */
            if ((ext_scan_list.scan_list[i].rsn_cipher == 0) &&
                (ext_scan_list.scan_list[i].wpa_cipher == 0))
            {
                corona_print("WEP ");
            }
        }
        else
        {
            corona_print("NONE! ");
        }

        corona_print("\n");
    }

    corona_print("Done.\n");
    fflush(stdout);

// ssid1 security parameters
#if 1
    security_parameters1.type = SEC_MODE_WPA;
    security_parameters1.parameters.wpa_parameters.version    = "wpa2";
    security_parameters1.parameters.wpa_parameters.passphrase = "walle~12";
    security_parameters1.parameters.wpa_parameters.ucipher    = ATH_CIPHER_TYPE_CCMP;
    security_parameters1.parameters.wpa_parameters.mcipher    = AES_CRYPT;
#else
    security_parameters1.type = SEC_MODE_OPEN;
    security_parameters1.parameters.wpa_parameters.version    = "wpa2";
    security_parameters1.parameters.wpa_parameters.passphrase = "bark1ey75";
    security_parameters1.parameters.wpa_parameters.ucipher    = ATH_CIPHER_TYPE_CCMP;
    security_parameters1.parameters.wpa_parameters.mcipher    = AES_CRYPT;
#endif

    // ssid2 security parameters
    security_parameters2.type = SEC_MODE_OPEN;
    security_parameters2.parameters.wep_parameters.key[0]            = "1234567890";
    security_parameters2.parameters.wep_parameters.key[1]            = "";
    security_parameters2.parameters.wep_parameters.key[2]            = "";
    security_parameters2.parameters.wep_parameters.key[3]            = "";
    security_parameters2.parameters.wep_parameters.default_key_index = 1; // 1 is key[0]

    corona_print("WIFIMGR_set_authentication %s...\n", ssid2);

    w_rc = WIFIMGR_set_security(ssid2, &security_parameters2, WIFIMGR_SECURITY_FLUSH_NEVER);
    if (0 != w_rc)
    {
        corona_print("WIFIMGR_set_authentication failed %d\n", w_rc);
        goto ERROR_1;
    }

    corona_print("WIFIMGR_connect %s...\n", ssid2);
    fflush(stdout);

    w_rc = WIFIMGR_connect(ssid2, NULL, FALSE);
    if (0 != w_rc)
    {
        corona_print("WIFIMGR_connect failed %d\n", w_rc);
    }

    corona_print("Done.\n");

    corona_print("WIFIMGR_disconnect...\n");
    fflush(stdout);

    w_rc = WIFIMGR_disconnect();
    if (0 != w_rc)
    {
        corona_print("WIFIMGR_disconnect failed %d\n", w_rc);
    }

    corona_print("WIFIMGR_connect %s...\n", ssid2);
    fflush(stdout);

    w_rc = WIFIMGR_connect(ssid2, NULL, FALSE);
    if (0 != w_rc)
    {
        corona_print("WIFIMGR_connect failed %d\n", w_rc);
    }
    corona_print("Done\n");

    corona_print("WIFIMGR_disconnect...\n");

    w_rc = WIFIMGR_disconnect();
    if (0 != w_rc)
    {
        corona_print("WIFIMGR_disconnect failed %d\n", w_rc);
    }

    corona_print("WIFIMGR_set_authentication %s...\n", ssid1);
    fflush(stdout);

    w_rc = WIFIMGR_set_security(ssid1, &security_parameters1, 0);
    if (0 != w_rc)
    {
        corona_print("WIFIMGR_set_authentication failed %d\n", w_rc);
        goto ERROR_1;
    }

    err = WIFIMGR_query_next_ssid(0, query_ssid);
    corona_print("WIFIMGR_query_next_ssid(0) returned %s (should be NULL)\n", query_ssid[0] ? query_ssid : "NULL");
    err = WIFIMGR_query_next_ssid(1, query_ssid);
    corona_print("WIFIMGR_query_next_ssid(1) returned %s (should be first)\n", query_ssid[0] ? query_ssid : "NULL");
    err = WIFIMGR_query_next_ssid(0, query_ssid);
    corona_print("WIFIMGR_query_next_ssid(0) returned %s (should be next or NULL if end)\n", query_ssid[0] ? query_ssid : "NULL");
    err = WIFIMGR_query_next_ssid(0, query_ssid);
    corona_print("WIFIMGR_query_next_ssid(0) returned %s (should be next or NULL if end)\n", query_ssid[0] ? query_ssid : "NULL");
    err = WIFIMGR_query_next_ssid(0, query_ssid);
    corona_print("WIFIMGR_query_next_ssid(0) returned %s (should be next or NULL if end)\n", query_ssid[0] ? query_ssid : "NULL");
    err = WIFIMGR_query_next_ssid(1, query_ssid);
    corona_print("WIFIMGR_query_next_ssid(1) returned %s (should be first)\n", query_ssid[0] ? query_ssid : "NULL");
    err = WIFIMGR_query_next_ssid(0, query_ssid);
    corona_print("WIFIMGR_query_next_ssid(0) returned %s (should be next or NULL if end)\n", query_ssid[0] ? query_ssid : "NULL");
    err = WIFIMGR_query_next_ssid(0, query_ssid);
    corona_print("WIFIMGR_query_next_ssid(0) returned %s (should be next or NULL if end)\n", query_ssid[0] ? query_ssid : "NULL");
    err = WIFIMGR_query_next_ssid(0, query_ssid);
    corona_print("WIFIMGR_query_next_ssid(0) returned %s (should be next or NULL if end)\n", query_ssid[0] ? query_ssid : "NULL");

    corona_print("WIFIMGR_connect %s...\n", ssid1);
    fflush(stdout);

    w_rc = WIFIMGR_connect(ssid1, NULL, FALSE);
    if (0 != w_rc)
    {
        corona_print("WIFIMGR_connect failed %d\n", w_rc);
    }
    corona_print("Done\n");
    fflush(stdout);

    // Test implicit disconnect within connect.
    corona_print("WIFIMGR_connect %s (implicit disconnect %s)...\n", ssid2, ssid1);
    fflush(stdout);

    w_rc = WIFIMGR_connect(ssid2, NULL, FALSE);
    if (0 != w_rc)
    {
        corona_print("WIFIMGR_connect failed %d\n", w_rc);
    }

    corona_print("Done\n");
    fflush(stdout);

    corona_print("WIFIMGR_connect %s (implicit disconnect %s)...\n", ssid1, ssid2); // implicit disconnect
    fflush(stdout);

    w_rc = WIFIMGR_connect(ssid1, NULL, FALSE);
    if (0 != w_rc)
    {
        corona_print("WIFIMGR_connect failed %d\n", w_rc);
        goto ERROR_1;
    }

    corona_print("Done\n");
    fflush(stdout);

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

    /* Sending.*/
    corona_print("Sending.\n");
    fflush(stdout);

    err = WIFIMGR_send(sock_peer, (uint_8 *)hello, strlen(hello), &send_result);
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
    
    buffer_ptr = _lwmem_alloc( MAX_WIFI_RX_BUFFER_SIZE );
    wassert( buffer_ptr != NULL );

    err = WIFIMGR_receive(sock_peer, (void *)buffer_ptr, MAX_WIFI_RX_BUFFER_SIZE, 0, &received, TRUE);
    if (received > 0)
    {
        corona_print("Recieved %d bytes:\n", received);
        buffer_ptr[received] = 0;
        corona_print("%s\n", buffer_ptr);
        fflush(stdout);
        
//        WIFIMGR_zero_copy_free(buffer_ptr));
        
    }
    else
    {
        corona_print("WIFIMGR_received failed with code 0x%x\n", err);
        goto ERROR_1;
    }

    corona_print("Receiving (should fail).\n");
    fflush(stdout);

    err = WIFIMGR_receive(sock_peer, (void *)buffer_ptr, MAX_WIFI_RX_BUFFER_SIZE, 0, &received, TRUE);
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

ERROR_1:
    WIFIMGR_close(sock_peer);
    wassert( MQX_OK == _lwmem_free(buffer_ptr) );    
    corona_print("Exiting.\n");
    fflush(stdout);
}

void wifi_mgr_chunked_ut (void)
{
    A_INT32               send_result;
    int_32                received;
    uint_32               payload_size;
    uint_16               http_header_len;
    int                   sock_peer; /* Foreign socket.*/
    char                  *buffer_ptr;
    security_parameters_t security_parameters1;
    char                  pHttpHeader[300];
    int                   w_rc;
    A_INT32               a_rc;
    int                   i;
    err_t                 err;
    
//    char *ssid1 = "Sasha";
    char *ssid1 = "krcl";
    char *host = "staging.whistle.com";
    char *server = "54.243.90.245"; // whistle
    int port = 80;
//    char *server = "192.168.9.114"; // Kevin's server
//    int port =3000;
    
    buffer_ptr = _lwmem_alloc( MAX_WIFI_RX_BUFFER_SIZE );
    wassert( buffer_ptr != NULL );
    
    memset(pHttpHeader,0,300);
    
    // ssid1 security parameters
    security_parameters1.type = SEC_MODE_WPA;
    security_parameters1.parameters.wpa_parameters.version    = "wpa2";
//    security_parameters1.parameters.wpa_parameters.passphrase = "walle~12";
    security_parameters1.parameters.wpa_parameters.passphrase = "password9";
    security_parameters1.parameters.wpa_parameters.ucipher    = ATH_CIPHER_TYPE_CCMP;
    security_parameters1.parameters.wpa_parameters.mcipher    = AES_CRYPT;
    
    payload_size = sizeof(peer0_1) + sizeof(peer0_2);
    
    /*
     *   Build an HTTP header for this test.
     */
    corona_print("Building HTTP header for payload size %d...\n", payload_size); fflush(stdout);
    fflush(stdout);
    WHTTP_build_http_header(pHttpHeader,
            "COR-UNITTEST",
            host,
            port,
            &http_header_len,
            payload_size);

    corona_print("Built HTTP header, size %d...\n", http_header_len); fflush(stdout);
    
    
    corona_print("WIFIMGR_set_authentication %s...\n", ssid1);
    
    w_rc = WIFIMGR_set_security(ssid1, &security_parameters1, WIFIMGR_SECURITY_FLUSH_NEVER);
    if (0 != w_rc)
    {
        corona_print("WIFIMGR_set_authentication failed %d\n", w_rc);
        goto ERROR_1;
    }
    
    corona_print("WIFIMGR_connect %s...\n", ssid1);
    fflush(stdout);
    
    w_rc = WIFIMGR_connect(ssid1, NULL, FALSE);
    
    if (0 != w_rc)
    {
        corona_print("WIFIMGR_connect failed %d\n", w_rc);
        goto ERROR_1;
    }
    
    corona_print("Done\n");
    fflush(stdout);
    
    corona_print("WIFIMGR_open(%s:%d)...\n", server, port);
    fflush(stdout);
    
    w_rc = WIFIMGR_open(server, port, &sock_peer, SOCK_STREAM_TYPE); // Open TCP Socket
    if (ERR_OK != w_rc)
    {
        corona_print("WIFIMGR_open failed %d\n", w_rc);
        goto ERROR_1;
    }
    
    corona_print("Done\n");
    fflush(stdout);
    
    /* Sending.*/
    corona_print("Sending HTTP header (%d bytes).\n", http_header_len);
    fflush(stdout);
    
    err = WIFIMGR_send(sock_peer, (uint_8 *)pHttpHeader, http_header_len, &send_result);
    if (send_result == http_header_len)
    {
        corona_print("Sent!\n");
    }
    else
    {
        corona_print("WIFIMGR_send failed with code 0x%x\n", err);
        goto ERROR_1;
    }

    corona_print("Sending Payload 1 (%d bytes).\n", sizeof(peer0_1));
    err = WIFIMGR_send(sock_peer, (uint_8 *)peer0_1, sizeof(peer0_1), &send_result);
    if (send_result == sizeof(peer0_1))
    {
        corona_print("Sent!\n");
    }
    else
    {
        corona_print("WIFIMGR_send failed with code 0x%x\n", err);
        goto ERROR_1;
    }
    

    corona_print("Sending Payload 1 (%d bytes).\n", sizeof(peer0_2));
    err = WIFIMGR_send(sock_peer, (uint_8 *)peer0_2, sizeof(peer0_2), &send_result);
    if (send_result == sizeof(peer0_2))
    {
        corona_print("Sent!\n");
    }
    else
    {
        corona_print("WIFIMGR_send failed with code 0x%x\n", err);
        goto ERROR_1;
    }
    
    corona_print("Receiving attempt 1.\n");
    fflush(stdout);
    
    err = WIFIMGR_receive(sock_peer, (void *)buffer_ptr, MAX_WIFI_RX_BUFFER_SIZE, 0, &received, TRUE);
    if (received > 0)
    {
        corona_print("Received %d bytes:\n", received);
        buffer_ptr[received] = 0;
        corona_print("%s\n", buffer_ptr);
        fflush(stdout);
        
//        WIFIMGR_zero_copy_free(response);
        
    }
    else
    {
        corona_print("WIFIMGR_received failed with code 0x%x\n", err);
        goto ERROR_1;
    }
    
    corona_print("Receiving attempt 2.\n");
    fflush(stdout);
    
    err = WIFIMGR_receive(sock_peer, (void *)buffer_ptr, MAX_WIFI_RX_BUFFER_SIZE, 0, &received, TRUE);
    if (received > 0)
    {
        corona_print("Received %d bytes:\n", received);
        buffer_ptr[received] = 0;
        corona_print("%s\n", buffer_ptr);
        fflush(stdout);
        
//        WIFIMGR_zero_copy_free(response);
        
    }
    else
    {
        corona_print("WIFIMGR_received failed with code 0x%x\n", err);
        goto ERROR_1;
    }
    
    ERROR_1:
    WIFIMGR_close(sock_peer);
    wassert( MQX_OK == _lwmem_free(buffer_ptr) );
    corona_print("Exiting.\n");
    fflush(stdout);
}

int_32 wifi_mgr_ut_connect()
{
    A_INT32   return_code;
    char      data[6];
    _mqx_uint result;
    security_parameters_t security_parameters1;

    security_parameters1.type = SEC_MODE_WPA;
    security_parameters1.parameters.wpa_parameters.version    = "wpa2";
    security_parameters1.parameters.wpa_parameters.passphrase = WIFI_UT_PASS;
    security_parameters1.parameters.wpa_parameters.ucipher    = ATH_CIPHER_TYPE_CCMP;
    security_parameters1.parameters.wpa_parameters.mcipher    = AES_CRYPT;

    return_code = WIFIMGR_set_security(WIFI_UT_SSID, &security_parameters1, WIFIMGR_SECURITY_FLUSH_NEVER);
    if (0 != return_code)
    {
        corona_print("WIFIMGR_set_authentication failed %d\n", return_code);
        return return_code;
    }
    
    return_code = WIFIMGR_connect(WIFI_UT_SSID, NULL, FALSE);
    if (A_OK != return_code)
    {
        corona_print("connect_handler failed %x", return_code);
        return -3;
    }
    return return_code;
}
#endif
