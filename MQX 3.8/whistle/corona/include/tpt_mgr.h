/*
 * tpt_mgr.h
 *
 *  Created on: Mar 19, 2013
 *      Author: Chris
 */

#ifndef TPT_MGR_H_
#define TPT_MGR_H_

#include "atheros_wifi_api.h"
#include "app_errors.h"

typedef enum TPTMGR_transport_type_e
{
    TPTMGR_TRANSPORT_TYPE_WIFI, // WiFi
    TPTMGR_TRANSPORT_TYPE_BTC,  // Bluetooth Classic
    TPTMGR_TRANSPORT_TYPE_BTLE, // Bluetooth LE
    TPTMGR_TRANSPORT_TYPE_ANY,  // Any (least power)
    TPTMGR_TRANSPORT_TYPE_NONE  // None available
} TPTMGR_transport_type_t;

err_t TPTMGR_connect(TPTMGR_transport_type_t requested_transport_type, TPTMGR_transport_type_t *actual_transport_type);
err_t TPTMGR_disconnect(void);
void TPTMGR_init(void);
#endif /* TPT_MGR_H_ */
