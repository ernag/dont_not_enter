/*
 * lm_mgr.h
 *
 *  Created on: Mar 30, 2013
 *      Author: Ernie
 */

#ifndef WIFI_CFG_MGR_H_
#define WIFI_CFG_MGR_H_

#include <atheros_wifi_api.h>
#include <wmi.h>
#include "ar4100p_main.h"
#include "wifi_mgr.h"
#include "whistlemessage.pb.h"

int LMMGR_process_main(void);
int LMMGR_test_network(char *ssid, security_parameters_t *security_parameters);
int LMMGR_disconnect(void);
int LMMGR_printscan(uint8_t *pMaxRSSI, uint8_t consider_only_configured);
int LMMGR_set_security(char *ssid, boolean doScan, security_parameters_t *psecurity_parameters);
void LMMGR_init(void);
void LMMGR_BT_connected(void);
void LMMGR_main_task(uint_32 dummy);
#endif /* WIFI_CFG_MGR_H_ */
