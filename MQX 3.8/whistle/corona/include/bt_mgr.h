/*
 * bt_mgr.h
 *
 *  Created on: Apr 28, 2013
 *      Author: Chris
 */

#ifndef BT_MGR_H_
#define BT_MGR_H_

#include "app_errors.h"
#include "BTBTypes.h"
#include "ISPPAPI.h"
#include "wmps1.h"
#include "whistlemessage.pb.h"

// Phone doesn't support keeping the BT session connected and switching from LM to DDOBT
#define NO_PHONE_SUPPORT_LM_TO_DDOBT_SESSION

/* The following type definition represents the container type which */
   /* holds the mapping between Bluetooth devices (based on the BD_ADDR)*/
   /* and the Link Key (BD_ADDR <-> Link Key Mapping).                  */
typedef struct _tagLinkKeyInfo_t
{
   BD_ADDR_t  BD_ADDR;
   Link_Key_t LinkKey;
   uint32_t usage_count;
   Port_Operating_Mode_t pom;
} LinkKeyInfo_t;

void BTMGR_init(void);
void BTMGR_main_task(uint_32 dummy);
err_t BTMGR_pair(void);
void BTMGR_set_debug(boolean hci_debug_enable, boolean ssp_debug_enable);
err_t BTMGR_open(int *pHandle);
err_t BTMGR_close(int handle);
err_t BTMGR_connect(BD_ADDR_t BD_ADDR, uint_32 page_timeout, LmMobileStatus *plmMobileStatus);
err_t BTMGR_check_proximity(uint_8 *pNumPairedDevices, uint_32 max_devices, uint_64 *pMacAddrs);
err_t BTMGR_query_next_key(boolean firstQuery, int *key);
err_t BTMGR_disconnect(void);
void BTMGR_disconnect_async(void);
err_t BTMGR_send(int handle, uint_8 *data, uint_32 length, int_32 *bytes_sent);
err_t BTMGR_receive(int handle, void *buf, uint_32 buf_len, uint_32 ms, int_32 *pbytes_received, boolean continuous_aggregate);
void BTMGR_increment_local_seqnum(void);
void BTMGR_zero_copy_free(void);
void BTDISC_tsk(uint_32 dummy);
void BTMGR_request(void);
void BTMGR_release(void);
void BT_verify_hw(void);
uint8_t BTMGR_uart_comm_ok(void);
uint8_t BTMGR_mfi_comm_ok(void);
boolean BTMGR_bt_is_on(void);
void BTMGR_delete_key(int key);
void BTMGR_force_disconnect();
Port_Operating_Mode_t BTMGR_get_current_pom(void);

#endif /* BT_MGR_H_ */
