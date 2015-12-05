/*
 * bt_mgr_wmps.h
 *
 *  Created on: Nov 6, 2013
 *      Author: Chris
 */

#ifndef BT_MGR_WMPS_H_
#define BT_MGR_WMPS_H_

#include "wmps1.h"
#include "whistlemessage.pb.h"

err_t BTMGR_wmps_receive(int handle, void *buf, uint_32 buf_len, uint_32 ms, int_32 *pbytes_received, wmps_seqnum_t *pseq_num);
err_t BTMGR_wmps_send(int handle, uint8_t *bytes, size_t bytes_length, int_32 *pbytes_sent, wmps_seqnum_t *pseqnum, wmps_next_header_payload_type_t payload_type);
void BTMGR_wmps_increment_local_seqnum(void);
wmps_seqnum_t BTMGR_wmps_get_local_seqnum(void);

#endif /* BT_MGR_WMPS_H_ */
