/*
 * con_mgr.h
 *
 *  Created on: Mar 26, 2013
 *      Author: Chris
 */

#ifndef CON_MGR_H_
#define CON_MGR_H_

#include "app_errors.h"
#include "sys_event.h"

#define CON_MAGIC_VALUE 0xfeedbeef

typedef enum CONMGR_connection_type_e
{
    CONMGR_CONNECTION_TYPE_FIRST = 0,
    CONMGR_CONNECTION_TYPE_WIFI = CONMGR_CONNECTION_TYPE_FIRST, // WiFi to Cloud
    CONMGR_CONNECTION_TYPE_SMARTPHONE_BTC,  // Bluetooth Classic to Smart Phone
    CONMGR_CONNECTION_TYPE_SMARTPHONE_BTLE, // Bluetooth LE to Smart Phone
    CONMGR_CONNECTION_TYPE_ANY,   // Any (least power)		
    CONMGR_CONNECTION_TYPE_LAST = CONMGR_CONNECTION_TYPE_ANY,
    CONMGR_CONNECTION_TYPE_UNKNOWN
} CONMGR_connection_type_t;

typedef void * CONMGR_handle_t;

typedef enum CONMGR_client_id_e
{
	// Order is important. Clients called in this order.
    CONMGR_CLIENT_ID_FIRST   = 0,
    CONMGR_CLIENT_ID_FUDMGR  = CONMGR_CLIENT_ID_FIRST,
    CONMGR_CLIENT_ID_EVTMGR,
    CONMGR_CLIENT_ID_CONSOLE, // for testing
    CONMGR_CLIENT_ID_BTNMGR,
    CONMGR_CLIENT_ID_PERMGR,
    CONMGR_CLIENT_ID_LAST = CONMGR_CLIENT_ID_PERMGR, // NOTE! - adding clients means updating g_connection static init.
    CONMGR_CLIENT_ID_NONE
    
} CONMGR_client_id_t;

typedef void (*CONMGR_connected_cbk_t)(CONMGR_handle_t handle, boolean success);
typedef void (*CONMGR_disconnected_cbk_t)(CONMGR_handle_t handle);

err_t CONMGR_register_cbk(CONMGR_connected_cbk_t opened_cb,
        CONMGR_disconnected_cbk_t closed_cb, uint_32 max_wait_time, uint_32 min_wait_time,
        CONMGR_connection_type_t connection_type, CONMGR_client_id_t client_id, CONMGR_handle_t *handle);
err_t CONMGR_open(CONMGR_handle_t handle, char *server, uint_32 port);
err_t CONMGR_release(CONMGR_handle_t handle);
err_t CONMGR_close(CONMGR_handle_t handle);
void CONMGR_shutdown(void);
void CON_tsk(uint_32 dummy);
void CONMGR_init(void);
void CONMGR_disconnected(sys_event_t event);
err_t CONMGR_send(CONMGR_handle_t handle, uint_8 *data, uint_32 length, int_32 *pbytes_sent);
err_t CONMGR_receive(CONMGR_handle_t handle, void *buf, uint_32 buf_len, uint_32 ms_timeout, int_32 *pbytes_received, boolean continuous_aggregate);
void CONMGR_zero_copy_free(CONMGR_handle_t handle, void *buf);
err_t CONMGR_get_seg_size(CONMGR_handle_t handle);
CONMGR_connection_type_t CONMGR_get_current_actual_connection_type(void);
int CONGMR_get_max_seg_size();

void CONMGR_Checkin_begin(void);
void CONMGR_Checkin_complete(void);

void CONMGR_stop_timers(void);


#endif /* CON_MGR_H_ */
