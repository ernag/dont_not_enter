/*
 * bt_mgr_wmps.c
 *
 *  Created on: Nov 6, 2013
 *      Author: Chris
 */

#include <mqx.h>
#include <bsp.h>
#include "cormem.h"
#include "wassert.h"
#include "bt_mgr.h"
#include "bt_mgr_priv.h"
#include "bt_mgr_wmps.h"

static wmps_seqnum_t g_btmgr_wmps_local_seqnum = 0; // Our local seq num. Mobile replies with this.

void BTMGR_wmps_increment_local_seqnum(void)
{
	g_btmgr_wmps_local_seqnum++ % 127;
}

wmps_seqnum_t BTMGR_wmps_get_local_seqnum(void)
{
	return g_btmgr_wmps_local_seqnum;
}

err_t BTMGR_wmps_send(int handle, uint8_t *bytes, size_t bytes_length, int_32 *pbytes_sent, wmps_seqnum_t *pseqnum, wmps_next_header_payload_type_t payload_type)
{
	uint8_t *dest_msg;
	size_t dest_msg_size;
	err_t err;
	wmps_msg_struct *msg;
	
	*pbytes_sent = 0;

	/* set up the msg that will be packed. */
	dest_msg = walloc(WMPS_MAX_PB_SERIALIZED_MSG_SIZE);

	/* here is where we make the message. */
	msg = WMP_make_msg_struct_with_payload(bytes, bytes_length);
	wassert(msg);
	msg->next_header->payload_type = payload_type;
	
	if (NULL == pseqnum)
	{
		// seq num not passed in means to use our outgoing seq num
		msg->seqnum = g_btmgr_wmps_local_seqnum;
	}
	else
	{
		msg->seqnum = *pseqnum;
	}
	
	/* set up and pack message into dest buffer */
	wassert(WMPS1ErrorOK == WMP_pack_message(dest_msg, WMPS_MAX_PB_SERIALIZED_MSG_SIZE, msg, &dest_msg_size));
	
	err = BTMGR_send(handle, dest_msg, dest_msg_size, pbytes_sent);

	if ((*pbytes_sent == dest_msg_size) && (*pbytes_sent > bytes_length))
	{
		*pbytes_sent = bytes_length;
	}
	else
	{
		*pbytes_sent = 0;
	}
		
    WMP_free_msg_struct(msg);
    wfree(dest_msg);
        
    return err;
}

err_t BTMGR_wmps_receive(int handle, void *buf, uint_32 buf_len, uint_32 ms, int_32 *pbytes_received, wmps_seqnum_t *pseq_num)
{
    TIME_STRUCT now;
    uint_64 ms_now_time_stamp;
    uint_64 ms_previous_time_stamp;
    uint_32 ms_remaining;
    int_32 received;
	err_t rc;
	uint_32 wmps_bytes_remaining;
    wmps_msg_struct *decoded_msg = NULL;
    wmps_error_t wmps_err;
    uint_8 *encoded_msg = NULL;
    
    *pbytes_received = 0;
    
    wassert(WMPS_HEADER_SZ <= buf_len);
    	
	// Since the complete read may span multiple BTMGR_receive calls, we need to keep
	// track of the elapsed ms.
	RTC_get_time_ms(&now);
	ms_previous_time_stamp = now.SECONDS*1000 + now.MILLISECONDS;
	ms_remaining = ms;
	
	encoded_msg = walloc(buf_len);
	
    rc = BTMGR_receive(handle, encoded_msg, WMPS_HEADER_SZ, ms_remaining, &received, TRUE);
	
	if (WMPS_HEADER_SZ > received)
	{
	    rc = PROCESS_NINFO(ERR_BT_ERROR, "rx1 h %x %d", rc, received);
	    goto DONE;
	}
	
	wassert(WMPS_HEADER_SZ == received);
	
	wmps_bytes_remaining = (uint_32)(*((wmps_messagesize_t *)(encoded_msg + WMPS_MESSAGESIZE_OFFSET))) - WMPS_HEADER_SZ;
	wassert(wmps_bytes_remaining <= buf_len);
		
	decoded_msg = WMP_make_msg_struct_with_payload(buf, buf_len);
	wassert(decoded_msg);

	*pseq_num = decoded_msg->seqnum;

	RTC_get_time_ms(&now);
	ms_now_time_stamp = now.SECONDS*1000 + now.MILLISECONDS;
	ms_remaining -= ms_now_time_stamp - ms_previous_time_stamp;
	ms_previous_time_stamp = ms_now_time_stamp;
	
	if (0 >= ms_remaining)
	{
		// Assume if we get here we didn't time out already. This is just in case we were right on the edge.
		ms_remaining = 1;
	}
		
	rc = BTMGR_receive(handle, encoded_msg + WMPS_HEADER_SZ, wmps_bytes_remaining, ms_remaining, &received, TRUE);

	if (received < wmps_bytes_remaining)
	{
		rc = PROCESS_NINFO(ERR_BT_ERROR, "rx2 %x %d %d", rc, *pbytes_received, wmps_bytes_remaining);
		goto DONE;
	}
	
    wmps_err = WMP_get_next_message(decoded_msg, encoded_msg, WMPS_MAX_PB_SERIALIZED_MSG_SIZE, (size_t *)&received);
        
    if (0 != wmps_err)
    {
		rc = PROCESS_NINFO(ERR_BT_ERROR, "wmp get %d", wmps_err);
		goto DONE;
    }

    *pbytes_received = decoded_msg->payload_size;
    
    //corona_print("BT WMPS msgSz: %d\n", *pbytes_received);
    
    rc = ERR_OK;
    
DONE:
	if (decoded_msg)
	{
		WMP_free_msg_struct(decoded_msg);
	}
	
	if (encoded_msg)
	{
		wfree(encoded_msg);
	}
	
	return rc;
}

