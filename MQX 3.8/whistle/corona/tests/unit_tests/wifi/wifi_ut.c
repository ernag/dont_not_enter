/*
 * wifi_ut.c
 *
 *  Created on: Mar 12, 2013
 *      Author: Chris
 */

#include "unit_tests.h"
#ifdef ENABLE_WHISTLE_UNIT_TESTS

#include "ar4100p_main.h"
#include "../shared_ut_config.h"
#include "wifi_ut.h"

static boolean        isConnected = FALSE;
static LWEVENT_STRUCT ar4100_lwevent;
#define CONNECTED_EVENT    1

static char *hello = "POST /devices/Cheos1/data_pusher HTTP/1.1\r\n\
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


static char *udp_test_string = "TESTING_UDP";

/*FUNCTION*-----------------------------------------------------------------
 *
 * Function Name  : wifiCallback
 * Returned Value : N/A
 * Comments       : Called from driver on a WiFI connection event
 *
 * END------------------------------------------------------------------*/
static void
wifiCallback(int val)
{
    if (val == A_TRUE)
    {
        corona_print("Connected\n");
        isConnected = TRUE;
    }
    else
    {
        corona_print("Disconnected\n");
        isConnected = FALSE;
    }
    
    fflush(stdout);
    
    _lwevent_set(&ar4100_lwevent, CONNECTED_EVENT);
}

int_32 wifi_ut_disconnect()
{
    WIFIMGR_disconnect();
    return 0;
}

int_32 wifi_ut_connect(void)
{
    A_INT32   return_code;
    char      data[6];
    _mqx_uint result;
    
    whistle_set_callback_with_driver(wifiCallback);
    
    /* Create the lightweight semaphore */
    result = _lwevent_create(&ar4100_lwevent, LWEVENT_AUTO_CLEAR);
    if (result != MQX_OK)
    {
        return -4;
    }
    
    return_code = whistle_set_passphrase((char *) WIFI_UT_PASS);
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
    
    return_code = WIFIMGR_connect(WIFI_UT_SSID, NULL, FALSE);
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
    
    fflush(stdout);
    
    return 0;
}


/************************************************************************
 * NAME: whistleDotCom_test
 *
 * DESCRIPTION: Start TCP Transmit test.
 ************************************************************************/
void
wifi_ut(void)
{
    SOCKADDR_T     foreign_addr;
    char           ip_str[16];
    A_INT32        send_result;
    uint_32        received = 0;
    _ip_address    temp_addr;
    int            sock_local; /* Listening socket.*/
    int            sock_peer;  /* Foreign socket.*/
    char           *buffer;
    char           *response = NULL;
    _enet_handle   handle;
    _ip_address    remote_addr;
    A_UINT32       packet_size = strlen(hello);
    unsigned short remote_port = 80;
    uint_32        addr_len;
    
    if (wifi_ut_connect() != 0)
    {
        corona_print("could not connect to Sasha\n");
        goto ERROR_1;
    }
    
    handle = whistle_atheros_get_handle();
    
    if (handle == NULL)
    {
        corona_print("ERROR: NULL handle\n");
        goto ERROR_1;
    }
    
    ath_inet_aton("54.243.90.245", (A_UINT32 *)&remote_addr);  // whistle
    //ath_inet_aton("192.168.169.27", (A_UINT32*) &remote_addr);    // Kevin's server
    //ath_inet_aton("74.125.224.101", (A_UINT32*) &remote_addr); // google
    
    temp_addr = LONG_BE_TO_HOST(remote_addr);
    corona_print("Remote IP addr. %s\n", inet_ntoa(*(A_UINT32 *)(&temp_addr), ip_str));
    corona_print("Remote port %d\n", remote_port);
    fflush(stdout);
    
    if ((sock_peer = t_socket((void *)handle, ATH_AF_INET, SOCK_STREAM_TYPE, 0))
            == A_ERROR)
    {
        corona_print("ERROR: Unable to create socket\n");
        goto ERROR_1;
    }
    
    /*Allow small delay to allow other thread to run*/
    _time_delay(1);
    
    corona_print("Connecting.\n");
    
    memset(&foreign_addr, 0, sizeof(foreign_addr));
    foreign_addr.sin_addr   = remote_addr;
    foreign_addr.sin_port   = remote_port;
    foreign_addr.sin_family = ATH_AF_INET;
    
    /* Connect to the server.*/
    if (t_connect((void *)handle, sock_peer, (&foreign_addr),
            sizeof(foreign_addr)) == A_ERROR)
    {
        corona_print("Connection failed.\n");
        goto ERROR_2;
    }
    
    corona_print("Connected.\n");
    
    /* Sending.*/
    corona_print("Sending.\n");
    fflush(stdout);
    
    while ((buffer = CUSTOM_ALLOC(packet_size)) == NULL)
    {
        /*Allow small delay to allow other thread to run*/
        _time_delay(1);
    }
    
    strcpy(buffer, hello);
    
    send_result = t_send((void *)handle, sock_peer, (unsigned char *)(buffer), packet_size, 0);
#if !NON_BLOCKING_TX
    /*Free the buffer only if NON_BLOCKING_TX is not enabled*/
    if (buffer)
    {
        CUSTOM_FREE(buffer);
    }
#endif
    if (send_result == A_ERROR)
    {
        corona_print("send packet error = %d\n", send_result);
        goto ERROR_2;
    }
    else if (send_result == A_SOCK_INVALID)
    {
        /*Socket has been closed by target due to some error, gracefully exit*/
        corona_print("Socket closed unexpectedly\n");
        goto ERROR_1;
    }
    else if (send_result == packet_size)
    {
        corona_print("Sent!\n");
        
        /* block for 1000 msec or until activity on socket */
        if (A_OK == t_select((void *)handle, sock_peer, 10000))
        {
            /* Receive data */
#if ZERO_COPY
            addr_len = sizeof(SOCKADDR_T);
            received = t_recv((void *)handle, sock_peer, (void **)(&response), CFG_PACKET_SIZE_MAX_RX, 0);
#else
            addr_len = sizeof(SOCKADDR_T);
            received = t_recv((void *)handle, sock_peer, (void *)(response), CFG_PACKET_SIZE_MAX_RX, 0);
#endif
        }
        if ((received != A_SOCK_INVALID) && (received != 0))
        {
            response[received] = 0;
            corona_print("received response %d bytes:\n\n%s\n", received, response);
        }
        else
        {
            corona_print("Did not receive response\n");
            goto ERROR_2;
        }
    }
    else
    {
        corona_print("Only sent %d bytes. Should probably call this in a loop to send the rest.\n", send_result);
        goto ERROR_2;
    }
    
    ERROR_2:
    //corona_print("ERROR_2\n"); // new line to separate from progress line
    if (send_result != A_SOCK_INVALID)
    {
        t_shutdown((void *)handle, sock_peer);
    }
    
    if (response != NULL)
    {
        zero_copy_free(response);
    }
    
    ERROR_1:
    //corona_print("ERROR_1\n");
    fflush(stdout);
}

void wifi_ut_udp(void)
{
    SOCKADDR_T     foreign_addr, local_addr;
    char           ip_str[16];
    A_INT32        send_result;
    uint_32        received = 0;
    _ip_address    temp_addr;
    int            sock_peer;  /* Foreign socket.*/
    char           *buffer;
    char           *response = NULL;
    _enet_handle   handle;
    _ip_address    remote_addr;
    A_UINT32       packet_size = strlen(udp_test_string);
    unsigned short remote_port = 80;
    unsigned short local_port = 1020;
    uint_32        foreign_addr_len, local_addr_len;
    

    memset(&local_addr, 0, sizeof(local_addr));
    
    if (wifi_ut_connect() != 0)
    {
        corona_print("could not connect to Sasha\n");
        goto ERROR_1;
    }
    
    handle = whistle_atheros_get_handle();
    
    if (handle == NULL)
    {
        corona_print("ERROR: NULL handle\n");
        goto ERROR_1;
    }
    
    //    ath_inet_aton("54.243.90.245", (A_UINT32 *)&remote_addr);  // whistle
//    ath_inet_aton("192.168.168.140", (A_UINT32*) &remote_addr);    // Kevin's server
    ath_inet_aton("192.168.9.119", (A_UINT32*) &remote_addr);    // Kevin's server
    
    temp_addr = LONG_BE_TO_HOST(remote_addr);
    corona_print("Remote IP addr. %s\n", inet_ntoa(*(A_UINT32 *)(&temp_addr), ip_str));
    corona_print("Remote port %d\n", remote_port);
    fflush(stdout);
    
    //    if ((sock_peer = t_socket((void *)handle, ATH_AF_INET, SOCK_STREAM_TYPE, 0))
    if ((sock_peer = t_socket((void *)handle, ATH_AF_INET, SOCK_DGRAM_TYPE, 0))
            == A_ERROR)
    {
        corona_print("ERROR: Unable to create socket\n");
        goto ERROR_1;
    }
    /*Allow small delay to allow other thread to run*/
    _time_delay(1);
    
    corona_print("Connecting.\n");
    
    memset(&foreign_addr, 0, sizeof(foreign_addr));
    foreign_addr.sin_addr   = remote_addr;
    foreign_addr.sin_port   = remote_port;
    foreign_addr.sin_family = ATH_AF_INET;
    
    /* Connect to the server.*/
    if (t_connect((void *)handle, sock_peer, (&foreign_addr),
            sizeof(foreign_addr)) == A_ERROR)
    {
        corona_print("Connection failed.\n");
        goto ERROR_2;
    }
    
    corona_print("Connected.\n");
    
    /* Sending.*/
    corona_print("Sending.\n");
    fflush(stdout);
    
    while ((buffer = CUSTOM_ALLOC(packet_size)) == NULL)
    {
        /*Allow small delay to allow other thread to run*/
        _time_delay(1);
    }
    
    strcpy(buffer, udp_test_string);

//    send_result = t_sendto((void *)handle, sock_peer, (unsigned char *)(buffer), packet_size, 0, (&foreign_addr), sizeof(foreign_addr));
    send_result = t_send((void *)handle, sock_peer, (unsigned char *)(buffer), packet_size, 0);
#if !NON_BLOCKING_TX
    /*Free the buffer only if NON_BLOCKING_TX is not enabled*/
    if (buffer)
    {
        CUSTOM_FREE(buffer);
    }
#endif
    if (send_result == A_ERROR)
    {
        corona_print("send packet error = %d\n", send_result);
        goto ERROR_2;
    }
    else if (send_result == A_SOCK_INVALID)
    {
        /*Socket has been closed by target due to some error, gracefully exit*/
        corona_print("Socket closed unexpectedly\n");
        goto ERROR_1;
    }
    else if (send_result == packet_size)
    {
        corona_print("Sent!\n");
        
        /* block for 1000 msec or until activity on socket */
        if (A_OK == t_select((void *)handle, sock_peer, 10000))
        {
            /* Receive data */
#if ZERO_COPY
            foreign_addr_len = sizeof(SOCKADDR_T);
//            received = t_recv((void *)handle, sock_peer, (void **)(&response), CFG_PACKET_SIZE_MAX_RX, 0);
            received = t_recvfrom((void *)handle, sock_peer, 
                    (void **)(&response), CFG_PACKET_SIZE_MAX_RX, 0,
                    &local_addr, &local_addr_len);
#else
            foreign_addr_len = sizeof(SOCKADDR_T);
            received = t_recv((void *)handle, sock_peer, (void *)(response), CFG_PACKET_SIZE_MAX_RX, 0);
#endif
        }
        if ((received != A_SOCK_INVALID) && (received != 0))
        {
            response[received] = 0;
            corona_print("received response %d bytes:\n\n%s\n", received, response);
        }
        else
        {
            corona_print("Did not receive response\n");
            goto ERROR_2;
        }
    }
    else
    {
        corona_print("Only sent %d bytes. Should probably call this in a loop to send the rest.\n", send_result);
        goto ERROR_2;
    }
    
    ERROR_2:
    //corona_print("ERROR_2\n"); // new line to separate from progress line
    if (send_result != A_SOCK_INVALID)
    {
        t_shutdown((void *)handle, sock_peer);
    }
    
    if (response != NULL)
    {
        zero_copy_free(response);
    }
    
    ERROR_1:

    wifi_ut_disconnect();
    //corona_print("ERROR_1\n");
    fflush(stdout);
}
#endif
