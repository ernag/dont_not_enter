/*
 * bt_mgr_priv.h
 *
 *  Created on: May 15, 2013
 *      Author: Chris
 */

#ifndef BT_MGR_PRIV_H_
#define BT_MGR_PRIV_H_

void BD_ADDRToStr(BD_ADDR_t Board_Address, char *BoardStr);
int GetLastPairedIndex(void);
int OpenStack(void);
void CloseStack(void);

int SetDisc(void);
int SetConnect(void);
int SetPairable(void);
void DeleteLinkKey(BD_ADDR_t BD_ADDR);

int SetDiscoverabilityMode(int mode);
int SetConnectabilityMode(int mode);
int SetPairabilityMode(int mode);
int GetLocalAddress(BD_ADDR_t *BD_ADD);
int SetLocalName(char *localName);
int GetLocalName(char *localName);

int OpenServer(unsigned int port);
void CloseServer(void);
int OpenRemoteServer(BD_ADDR_t BD_ADDR, Port_Operating_Mode_t pom);
int CloseRemoteServer(void);
int Read(unsigned char *data, unsigned int length);
int Write(unsigned char *data, unsigned int length);
int SendSessionData(unsigned char *data, unsigned int length);

int BTMGR_PRIV_receive(void *buf, uint_32 length, uint_32 ms, int_32 *pbytes_received, boolean continuous_aggregate);
int BTMGR_PRIV_send(uint_8 *data, uint_32 length);
void BTMGR_PRIV_zero_copy_free(void);
void BTMGR_PRIV_pairing_started(void);
void BTMGR_PRIV_pairing_complete(boolean succeeded);
void BTMGR_PRIV_connecting_complete(boolean succeeded);
void BTMGR_PRIV_ios_pairing_complete(void);
void BTMGR_PRIV_disconnected(void);
int BTMGR_PRIV_set_connect_timeout(uint_32 ms);
Port_Operating_Mode_t BTMGR_PRIV_get_current_pom(void);

#endif /* BT_MGR_PRIV_H_ */
