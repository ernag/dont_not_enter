/*
 * evt_mgr_upload_ut.c
 *
 *  Created on: Apr 11, 2013
 *      Author: Ernie
 */
#include "unit_tests.h"
#ifdef ENABLE_WHISTLE_UNIT_TESTS

#include "evt_mgr.h"
#include "app_errors.h"
#include "con_mgr.h"
#include "cfg_mgr.h"
#include "wassert.h"
#include <mqx.h>
#include <string.h>

#define FAKE_PAYLOAD_SZ     13
#define RESPONSE_SZ         16

static uint8_t g_waiting_for_connection = TRUE;
static CONMGR_handle_t g_conmgr_handle;

static void con_mgr_connected(CONMGR_handle_t handle, boolean success)
{
    if(!success)
    {
        corona_print("con_mgr_connected callback returned an error!\n\n\r"); fflush(stdout);
    }
    wassert(handle == g_conmgr_handle);
    g_waiting_for_connection = FALSE;
}

static void con_mgr_disconnected(CONMGR_handle_t handle)
{
    corona_print("CONMGR lost its connection!\n\r"); fflush(stdout);
    g_waiting_for_connection = TRUE;
}

err_t evt_mgr_upload_ut(void)
{
    char *pPayload = NULL;
    char *pServer = "192.168.168.65";//"staging.whistle.com"; //"192.168.168.140"; //"192.168.171.191"; //"192.168.168.140"; // Whistle Server
    uint_32 port = 33;//3000; // Whistle Server Port
    uint_16 header_len = 0;
    uint_8  fake_payload[FAKE_PAYLOAD_SZ] = {'1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', '\r', '\n', 0};
    int_32 send_result;
    int_32 received;
    err_t err;
    _mem_size payload_size = FAKE_PAYLOAD_SZ + 2;
    
    _time_delay(2000);  // give some time for con_mgr_ut() to actually connect, and 'stuff'...
    
    pPayload = _lwmem_alloc( payload_size);
    wassert(pPayload != NULL);
        
    
    _time_delay(5);
    fflush(stdout);  // apparently the above corona_print is stress testing JTAG's terrible speeds...
    _time_delay(2);
    
    strcat(pPayload, (const char *)fake_payload);
    
    /*
     *   Try to send the fake header/payload to the Whistle server and get some type of sane response.
     */
    
    g_st_cfg.con_st.port = port;
    
    wassert(ERR_OK == CONMGR_register_cbk(con_mgr_connected, con_mgr_disconnected, 0, 0,
                CONMGR_CONNECTION_TYPE_ANY, CONMGR_CLIENT_ID_CONSOLE, &g_conmgr_handle));
    
    corona_print("evt_mgr_upload_ut() - Waiting for connection...\n"); fflush(stdout);
    
    while(g_waiting_for_connection)
    {
        _time_delay(100);
    }
    
    /*
     *   Open a 'socket'
     */
    err = bark_open(g_conmgr_handle, FAKE_PAYLOAD_SZ);
    if(err != ERR_OK)
    {
        corona_print("CONMGR_open failed with code 0x%x\n", err);
        goto evt_mgr_upload_done;
    }

    corona_print("evt_mgr_upload_ut() - Sending data to conn mgr...\n"); fflush(stdout);
    err = bark_send(g_conmgr_handle, (uint_8 *)pPayload, FAKE_PAYLOAD_SZ, &send_result);
    if (send_result == (FAKE_PAYLOAD_SZ))
    {
        corona_print("Sent successful\n\r"); fflush(stdout);
    }
    else
    {
        corona_print("CONMGR_send failed with code 0x%x\n", err); fflush(stdout);
        goto evt_mgr_upload_done;
    }
    
    corona_print("evt_mgr_upload_ut(): Receiving...\n"); fflush(stdout);
    
    err = bark_receive(g_conmgr_handle, (void *) pPayload, payload_size, 0, &received, TRUE);
    if (received > 0)
    {
        corona_print("evt_mgr_upload_ut(): Recieved %d bytes:\n", received);
        pPayload[received] = 0;
        corona_print("%s\n", pPayload);
        fflush(stdout);
//        CONMGR_zero_copy_free(g_conmgr_handle, response);
    }
    else
    {
        corona_print("EM: CONMGR_received failed with code 0x%x\n", err); fflush(stdout);
        goto evt_mgr_upload_done;
    }
    

evt_mgr_upload_done:

    wassert(ERR_OK == bark_close(g_conmgr_handle));
    
    wassert( MQX_OK == _lwmem_free(pPayload) );
    return ERR_OK;
}
#endif
