/*
 * evt_mgr_upload.c
 * This file contains the EVTMGR stuff that requires a bit of coupling with the server, WMP, HTTP, etc...
 *
 *  Created on: Apr 10, 2013
 *      Author: Ernie
 */

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include <mqx.h>
#include <bsp.h>
#include <watchdog.h>
#include <string.h>
#include "cormem.h"
#include "wassert.h"
#include "evt_mgr_priv.h"
#include "evt_mgr.h"
#include "psptypes.h"
#include "con_mgr.h"
#include "cfg_mgr.h"
#include "crc_util.h"
#include "pb_encode.h"
#include "wifi_mgr.h"
#include "pwr_mgr.h"
#include "wmp.h"
#include "whttp.h"
#include "wmp_datadump_encode.h"
#include "bark.h"
#include "persist_var.h"
#include "bt_mgr.h"

////////////////////////////////////////////////////////////////////////////////
// Defines
////////////////////////////////////////////////////////////////////////////////
#define WMP_HEADER_LEN                   		60
#define MAX_SEQ_GENERAL_FAILURES		        4
#define MAX_EVTMGR_RX_BUFFER_SIZE        		CFG_PACKET_SIZE_MAX_RX
#define EVTMGR_UPLOAD_TCP_SEND_TIMEOUT_WD		(60*1000) // Should be atleast TRANSMIT_BLOCK_TIMEOUT (20000, common_stack_offload.h)
#define EVTMGR_UPLOAD_TCP_RECEIVE_TIMEOUT		(20*1000)
#define EVTMGR_UPLOAD_TCP_RECEIVE_TIMEOUT_WD	(60*1000 + WIFI_RECEIVE_TSELECT_TOTAL_RETRY_TIME + 500)
#define EVTMGR_UPLOAD_DEF_WD					(60*1000)

#define EVTMGR_UPLOAD_TX_DELAY					50

#define REBOOT_REASON_BT_OR_WIFI (CONMGR_CONNECTION_TYPE_WIFI == CONMGR_get_current_actual_connection_type() ? PV_REBOOT_REASON_WIFI_FAIL : PV_REBOOT_REASON_BT_FAIL)

////////////////////////////////////////////////////////////////////////////////
// Structures
////////////////////////////////////////////////////////////////////////////////

/*
 *   Structure used to track the server dump buffer.
 */
typedef struct server_dump_info_type
{
    uint8_t  *pBuffer;
    uint32_t buf_index;

}serv_dump_info_t;

////////////////////////////////////////////////////////////////////////////////
// Variables
////////////////////////////////////////////////////////////////////////////////
static CONMGR_handle_t g_conmgr_handle;
extern LWEVENT_STRUCT  g_evtmgr_lwevent;
extern uint8_t         g_evtmgr_shutting_down;
static size_t upload_total = 0;

////////////////////////////////////////////////////////////////////////////////
// Declarations
////////////////////////////////////////////////////////////////////////////////
static void _EVTMGR_con_mgr_connected(CONMGR_handle_t handle, boolean success);
static void _EVTMGR_con_mgr_disconnected(CONMGR_handle_t handle);
void _EVTMGR_con_mgr_register_any(uint_32 min_wait_time, uint_32 max_wait_time);
static void _EVTMGR_process_opened_event(boolean success);
static void _EVTMGR_process_closed_event(void);
static void _EVTMGR_upload_init(void);
static err_t _EVTMGR_dump_srv(void);
static void _EVTMGR_reset_server_dump_info( serv_dump_info_t *pInfo );


////////////////////////////////////////////////////////////////////////////////
// Code
////////////////////////////////////////////////////////////////////////////////

/*
 *   Change the data dump seconds in config manager.
 *   Re-register with connection manager with the new time.
 *   Flush the new configs to SPI Flash.
 */
void EVTMGR_upload_dutycycle(uint_32 seconds_between_uploads)
{
    g_st_cfg.evt_st.data_dump_seconds = seconds_between_uploads;
    _EVTMGR_con_mgr_register_any(0, g_st_cfg.evt_st.data_dump_seconds);
    CFGMGR_flush( CFG_FLUSH_STATIC );
}

static void _EVTMGR_upload_init(void)
{
	boolean second_sync_pending = pmem_flag_get(PV_FLAG_2ND_SYNC_PENDING);
	
#if !WIFI_RSSI_TEST_APP     // RSSI App doesn't want others to step on its WIFI toes.
    _EVTMGR_con_mgr_register_any(0,  second_sync_pending ? g_st_cfg.evt_st.first_lm_data_dump_seconds : g_st_cfg.evt_st.data_dump_seconds);
    
    if (second_sync_pending)
    {
    	PMEM_FLAG_SET(PV_FLAG_2ND_SYNC_PENDING, FALSE);
    }
#endif
}

static void _EVTMGR_con_mgr_connected(CONMGR_handle_t handle, boolean success)
{
    // Signal my thread to get this off the CONMGR context
    wassert(handle == g_conmgr_handle);

    PWRMGR_VoteForON(PWRMGR_EVTMGR_UPLOAD_VOTE);  // here guarantees task runs
    _lwevent_set(&g_evtmgr_lwevent, success ? EVTMGR_CONMGR_CONNECT_SUCCEEDED_EVENT : EVTMGR_CONMGR_CONNECT_FAILED_EVENT);
}

static void _EVTMGR_con_mgr_disconnected(CONMGR_handle_t handle)
{
    // Signal my thread to get this off the CONMGR context
    wassert(handle == g_conmgr_handle);
    
    PWRMGR_VoteForON(PWRMGR_EVTMGR_UPLOAD_VOTE);  // here guarantees task runs
    _lwevent_set(&g_evtmgr_lwevent, EVTMGR_CONMGR_DISCONNECTED_EVENT);
}

void _EVTMGR_con_mgr_register_any(uint_32 min_wait_time, uint_32 max_wait_time)
{
	err_t err;
	
	err = CONMGR_register_cbk(_EVTMGR_con_mgr_connected, _EVTMGR_con_mgr_disconnected, max_wait_time, min_wait_time,
            CONMGR_CONNECTION_TYPE_ANY, CONMGR_CLIENT_ID_EVTMGR, &g_conmgr_handle);
	
	if (ERR_OK != err)
	{
	    PWRMGR_VoteForOFF(PWRMGR_EVTMGR_UPLOAD_VOTE); // let go before ending our meaningless life so we don't drag others to our grave
		wassert(0);
	}
	
    corona_print("EM: register_cbk min/max: %d/%d\n", min_wait_time, max_wait_time);
}

/*
 *   Resets the server-dump-buffer structure to its initial state.
 */
static void _EVTMGR_reset_server_dump_info( serv_dump_info_t *pInfo )
{
    pInfo->buf_index = 0;
    
    if( NULL != pInfo->pBuffer )
    {
        //corona_print("TEST: freeing memory\n");
        wfree( pInfo->pBuffer );
        pInfo->pBuffer = NULL;
    }
}

/*
 *   *** Callback used for dumping data to a server ***
 *   
 *     We want to limit the amount sent to the server each time to [ EVTMGR_*_UPLOAD_CHUNK_SZ ] (optimal for Atheros).
 *     To do this, we malloc a buffer of that size, and make sure we send only in chunks of this.
 *     We continue fragmenting the payloads as they come in to meet this requirement, until we get a "last block",
 *     in which case we send the remaining length's worth of the payload (the end is probably not going to be aligned exactly).
 *     
 */
static err_t _EVTMGR_server_dump_cbk(err_t err, uint8_t *pData, uint32_t len, uint8_t last_block)
{
    static serv_dump_info_t info = { NULL, 0 };
    int32_t  send_result;
    uint32_t num_bytes_left_in_buf;
    uint32_t num_bytes_to_copy;
    uint32_t num_bytes_copied;
    int tansport_maximum_segment_size;
    
    tansport_maximum_segment_size = CONGMR_get_max_seg_size();
    
    /*
     *   First of all, let's check for errors the caller may be passing in to us...
     */
    if( ERR_EVTMGR_NO_DATA_TO_DUMP == err )
    {
        corona_print("no data\n");
        _lwevent_set(&g_evtmgr_lwevent, EVTMGR_NO_DATA_TO_DUMP);
        err = ERR_OK;
        goto last_block_server_dump;
    }
    else if( ERR_OK != err )
    {
        PROCESS_NINFO(err, NULL);
        _lwevent_set(&g_evtmgr_lwevent, EVTMGR_DUMPED_PACKET_FAIL);
        goto last_block_server_dump;
    }
//    corona_print("to send: %d\n", len);
    
    /*
     *   If this is our first entry on this open socket, we need to allocate space for our 
     *      optimal-for-Atheros BUFFER.
     */
    if( NULL == info.pBuffer )
    {
        /*
         *   First time dump() called our callback.  Alloc memory needed for our 'optimal-for-Atheros' buffer.
         */
        info.pBuffer = walloc( tansport_maximum_segment_size );
    }
    
    /*
     *   Zero length:  Could be the last call to our callback, or just a problem.
     */
    if( 0 == len )
    {
        //corona_print("TEST: ZERO len !\n"); fflush(stdout);
        if( last_block )
        {
            if( info.buf_index > 0 )
            {
                /*
                 *   Flush the remaining data we have in our local buffer out to CONMGR...
                 */
                //corona_print("TEST: sending remaining %d bytes\n", info.buf_index); fflush(stdout);

                _time_delay(EVTMGR_UPLOAD_TX_DELAY);
                _watchdog_start(EVTMGR_UPLOAD_TCP_SEND_TIMEOUT_WD);
                err = bark_send(g_conmgr_handle, info.pBuffer, info.buf_index, &send_result);
                upload_total += info.buf_index;
                _watchdog_start(EVTMGR_UPLOAD_DEF_WD);

                if (ERR_CONMGR_NOT_CONNECTED == err)
                {
                	err = PROCESS_NINFO(ERR_EVTMGR_NOT_CONNECTED, NULL);
                    _lwevent_set(&g_evtmgr_lwevent, EVTMGR_DUMPED_PACKET_FAIL);
                	goto last_block_server_dump;
                }
                if ( send_result != info.buf_index )
                {
                    PROCESS_NINFO(err, "EVT1 bad len Exp %d tx'd %d (ttl %d)\n", info.buf_index, send_result, upload_total);
                    PWRMGR_Reboot( 500, REBOOT_REASON_BT_OR_WIFI, FALSE );
                    err = ERR_EVTMGR_DUMP_TO_SERV_CONMGR_SEND;
                    _lwevent_set(&g_evtmgr_lwevent, EVTMGR_DUMPED_PACKET_FAIL);
                    goto last_block_server_dump;
                }
                else if ( ERR_OK != err)
                {
                    PROCESS_NINFO(err, "evtupl1");
                    err = ERR_EVTMGR_DUMP_TO_SERV_CONMGR_SEND;
                    _lwevent_set(&g_evtmgr_lwevent, EVTMGR_DUMPED_PACKET_FAIL);
                    goto last_block_server_dump;
                }
            }
            
            _lwevent_set(&g_evtmgr_lwevent, EVTMGR_DUMPED_PACKET_SUCCESS);
            err = ERR_OK;
            goto last_block_server_dump;
        }
        else
        {
            /*
             *   If we get here, someone called us back with zero length, but didn't set "last_block" to true.
             *     COR-523 - Unfortunately, this probably means Event Manager has somehow become corrupted, so restart things,
             *     and try to document we did that so we know for next time.
             */
            EVTMGR_reset();
            err = PROCESS_NINFO( ERR_EVTMGR_ZEROLEN_LAST_BLOCK, "0 len!" );
            wassert(0);
            
            _lwevent_set(&g_evtmgr_lwevent, EVTMGR_DUMPED_PACKET_FAIL);
            goto last_block_server_dump;
        }
    }
    
    
    /*
     *   Start making use of the 'optimal-for-Atheros' buffer, which may be only a partial amount of this payload.
     */
    num_bytes_left_in_buf = tansport_maximum_segment_size - info.buf_index;
    num_bytes_copied = 0;
    
    /*
     *   First traverse this payload and send 
     */
    while( len >= num_bytes_left_in_buf )
    {
        /*
         *   Keep sending data in optimal chunks until we have only a partial amount (or no data) left to send.
         */
        
        //corona_print("TEST: loop, len: %d, num_bytes_left_in_buf: %d\n", len, num_bytes_left_in_buf); fflush(stdout);
        memcpy( info.pBuffer + info.buf_index, pData + num_bytes_copied, num_bytes_left_in_buf );
        len -= num_bytes_left_in_buf;
        num_bytes_copied += num_bytes_left_in_buf;
        
        num_bytes_left_in_buf = tansport_maximum_segment_size;
        info.buf_index = 0;

        _time_delay(EVTMGR_UPLOAD_TX_DELAY);
        _watchdog_start(EVTMGR_UPLOAD_TCP_SEND_TIMEOUT_WD);
        upload_total += tansport_maximum_segment_size;
        err = bark_send( g_conmgr_handle, info.pBuffer, tansport_maximum_segment_size, &send_result );
        _watchdog_start(EVTMGR_UPLOAD_DEF_WD);

        if (ERR_CONMGR_NOT_CONNECTED == err)
        {
        	err = PROCESS_NINFO(ERR_EVTMGR_NOT_CONNECTED, NULL);
            _lwevent_set(&g_evtmgr_lwevent, EVTMGR_DUMPED_PACKET_FAIL);
        	goto last_block_server_dump;
        }
        if ( send_result != tansport_maximum_segment_size )
        {
            PROCESS_NINFO(err, "EVT2 bad len Exp %d tx'd %d (ttl %d)\n", tansport_maximum_segment_size, send_result, upload_total);
            PWRMGR_Reboot( 500, REBOOT_REASON_BT_OR_WIFI, FALSE );
            err = ERR_EVTMGR_DUMP_TO_SERV_CONMGR_SEND;
            _lwevent_set(&g_evtmgr_lwevent, EVTMGR_DUMPED_PACKET_FAIL);
            goto last_block_server_dump;
        }
        else if ( ERR_OK != err)
        {
            PROCESS_NINFO(err, "evtupl2");
            err = ERR_EVTMGR_DUMP_TO_SERV_CONMGR_SEND;
            _lwevent_set(&g_evtmgr_lwevent, EVTMGR_DUMPED_PACKET_FAIL);
            goto last_block_server_dump;
        }
    }
    
    if( len > 0 )
    {
        /*
         *   There is still more data in the payload we haven't sent.  Place it in the buffer,
         *     so we can send it later when we fill it (or now if this is the last block).
         *
         */
    
        if( last_block )
        {
            /*
             *   Since this is the last block, go ahead and send whatever partial amount.
             *     And no need to memcpy() into our optimal-sized buffer, just send directly from the 
             *     original buffer.
             */
            _time_delay(EVTMGR_UPLOAD_TX_DELAY);
            _watchdog_start(EVTMGR_UPLOAD_TCP_SEND_TIMEOUT_WD);
            upload_total += len;
            err = bark_send(g_conmgr_handle, pData + num_bytes_copied, len, &send_result);
            _watchdog_start(EVTMGR_UPLOAD_DEF_WD);

            if (ERR_CONMGR_NOT_CONNECTED == err)
            {
            	err = PROCESS_NINFO(ERR_EVTMGR_NOT_CONNECTED, NULL);
                _lwevent_set(&g_evtmgr_lwevent, EVTMGR_DUMPED_PACKET_FAIL);
            	goto last_block_server_dump;
            }
            if ( send_result != len )
            {
                PROCESS_NINFO(err, "EVT3 bad len Exp %d tx'd %d (ttl %d)\n", len, send_result, upload_total);
                PWRMGR_Reboot( 500, REBOOT_REASON_BT_OR_WIFI, FALSE );
                err = ERR_EVTMGR_DUMP_TO_SERV_CONMGR_SEND;
                _lwevent_set(&g_evtmgr_lwevent, EVTMGR_DUMPED_PACKET_FAIL);
                goto last_block_server_dump;
            }
            else if ( ERR_OK != err)
            {
                PROCESS_NINFO(err, "evtupl3");
                err = ERR_EVTMGR_DUMP_TO_SERV_CONMGR_SEND;
                _lwevent_set(&g_evtmgr_lwevent, EVTMGR_DUMPED_PACKET_FAIL);
                goto last_block_server_dump;
            }
        }
        else
        {
            /*
             *    Copy the remaining partial data into the buffer, we'll send it later.
             */
            memcpy( info.pBuffer + info.buf_index, pData + num_bytes_copied, len );
            info.buf_index += len;
            wassert( info.buf_index < tansport_maximum_segment_size );  // sanity check to make sure we don't over-run our buffer.
        }
    }
    
    /*
     *   See if this is our last block of data, and return.
     */
    if( last_block )
    {
        _lwevent_set(&g_evtmgr_lwevent, EVTMGR_DUMPED_PACKET_SUCCESS);
        goto last_block_server_dump;
    }
    
    return err;
    
last_block_server_dump:

    /*
     *   Since this is the last block we are going to get called back to send,
     *     let's perform some cleanup before our final return.
     */
    _EVTMGR_reset_server_dump_info( &info );
    return err;
}

/*
 *   Return the relevant packet information needed to construct the HTTP & WMP headers needed
 *     for uploading data to the server.
 *     
 *     pNumEvtBlocks = output, the number of event blocks we are going to send.
 *     pLen = output, the total number of data bytes we are going to send.
 *     pCrc = output, the 32-bit CRC corresponding to the total data payload.
 *     packet_sz_limit = input, maximum number of bytes user wants to send in a single packet.
 *     pLastData = output, 1 if this is the last data available in EVTMGR, 0 otherwise.
 */
static err_t _EVTMGR_get_packet_info( uint32_t *pNumEvtBlocks, uint32_t *pLen, uint32_t *pCrc, uint32_t packet_sz_limit, uint8_t *pLastData )
{
    err_t err;
    uint8_t  add_another_page_of_evt_blk_ptr_data = TRUE;
    uint16_t num_evt_blk_ptrs;
    uint32_t *pHandle = NULL;
    uint32_t evt_blk_rd_addr;
    crc_handle_t crc_handle;
    
    *pNumEvtBlocks = 0;
    *pLen = 0;
    *pCrc = 0;
    *pLastData = 0;
    
    _EVTMGR_lock();
    UTIL_crc32_reset( &crc_handle, CRC_DEFAULT_SEED );
    
    evt_blk_rd_addr = _EVTMGR_get_evt_blk_rd_addr();
    
    while( (g_Info.evt_blk_wr_addr != evt_blk_rd_addr) && add_another_page_of_evt_blk_ptr_data ) /* iterate until we get the amount of data we asked for, or we drain all data */
    {
        uint32_t data_len = 0;
        uint16_t i;
        evt_blk_ptr_ptr_t pEvtBlkPtr;
        
        /*
         *   Load another event block pointer cache.
         */
        err = _EVTMGR_load_evt_blk_ptrs( &pHandle, evt_blk_rd_addr, &num_evt_blk_ptrs );
        if(err != ERR_OK)
        {
            goto done_packet_info;
        }
        wassert( num_evt_blk_ptrs > 0 );
        
        /*
         *   Get the length of this page's worth of event block pointers, and determine
         *     if adding it to the packet, would exceed our sz_limit.
         */
        for(i = 0; i < num_evt_blk_ptrs; i++)
        {
            pEvtBlkPtr = _EVTMGR_get_blk_ptr(&pHandle, i);
            wassert( NULL != pEvtBlkPtr );
            
            data_len += pEvtBlkPtr->len;
        }
        
        /*
         *   Log an event if the FW-922 case ever occurs.
         */
        if(data_len > packet_sz_limit)
        {
            PROCESS_NINFO( ERR_EVTMGR_CHUNK_EXCEEDS_MAX_PACKET_LEN, NULL );
        }
        
        if( ((*pLen + data_len) > packet_sz_limit) &&  // make sure we don't exceed the max packet sz here.
             (*pLen > 0) )   // FW-922 - make sure we aren't just dunking a zero length payload b/c one chunk is bigger than max.
        {
            /*
             *   Can't add this page's data into the packet.
             */
            add_another_page_of_evt_blk_ptr_data = FALSE;
        }
        else
        {   
            /*
             *   Looks like we're tacking on another page's worth of event block pointer data
             *     to the packet.  woo-hoo !
             */
            *pLen += data_len;  
            *pNumEvtBlocks += num_evt_blk_ptrs;
            
            /*
             *   Run back through the event blocks, to calculate the CRC.
             */
            for(i = 0; i < num_evt_blk_ptrs; i++)
            {
                pEvtBlkPtr = _EVTMGR_get_blk_ptr(&pHandle, i);
                wassert( NULL != pEvtBlkPtr );
                *pCrc = UTIL_crc_get32(&crc_handle, pEvtBlkPtr->crc);
            }
        }
        
        _EVTMGR_unload_evt_blk_ptrs( &pHandle );
        
        /*
         *   Increment event block pointer read address LOCALLY, so we can keep
         *   iterating and calculating length, CRC, etc. of data we are going to send.
         */
        evt_blk_rd_addr = _EVTMGR_update_evt_blk_ptr_addr( evt_blk_rd_addr );
    }
    
done_packet_info:
    
    if( g_Info.evt_blk_wr_addr == evt_blk_rd_addr )
    {
        *pLastData = 1;
    }
    
    _EVTMGR_unlock();
    
    if( 0 == *pLen )
    {
        /*
         *  This should never happen!  If it does, EVTMGR is invalid and must be reset.
         */
        EVTMGR_reset();
        wassert( ERR_OK == PROCESS_NINFO( ERR_EVTMGR_PACKET_INVALID, NULL ) );
    }

	UTIL_crc_release(&crc_handle);
    return err;
}

/*
 *   Dump all Event Manager data to the server.
 */
static err_t _EVTMGR_dump_srv(void)
{
    int_32 send_result, received, received_flush;
    char *response_flush;
    char *pResponseBuf;
    char *pHttpHeader;
    err_t err = ERR_OK;
    uint32_t packet_len, wmp_header_len;
    uint32_t num_evt_blocks;
    uint32_t crc;
    uint32_t total_data_to_upload;
    uint32_t left_to_upload;
    uint8_t last_data = 0;
    uint8_t sequential_parse_failures = 0;
    uint8_t sequential_general_errors = 0;
    uint8_t *pWmpHeaderBuf;
    uint8_t first_msg_sent = 0;
    _mqx_uint mask;
    pb_ostream_t wmp_stream;
    TIME_STRUCT timestamp;
    uint_64 l_timestamp2;
    uint32_t progress = progressType_PROGRESS_FIRST_MSG;
    
    pWmpHeaderBuf = walloc( WMP_HEADER_LEN );
    pResponseBuf = walloc( MAX_EVTMGR_RX_BUFFER_SIZE );
    total_data_to_upload = _EVTMGR_total_data_avail();
	
	corona_print("\n\t* %s *\n", HOST_TO_UPLOAD_DATA_TO);
    
    while( _EVTMGR_has_data_to_send() )
    {
        _watchdog_start(100*1000);

        corona_print("\n\tDump\tRD:%x WR:%x\n", g_Info.evt_blk_rd_addr, g_Info.evt_blk_wr_addr);
        
        /*
         *   TODO: we were getting unhandled int exceptions right around this location for some reason.
         */
        
        /*
         *   Look for the next batch of event block pointers to send.  
         *   
         *   Each dump will need to contain:
         *     (a)  N pages of event block pointers' worth of data, not to exceed MAX_PACKET_LEN.
         *     (b)  total length of payload.
         *     (c)  32-bit CRC of payload.
         */
        
        err = _EVTMGR_get_packet_info( &num_evt_blocks, &packet_len, &crc, g_st_cfg.evt_st.max_upload_transfer_len, &last_data );
        
        if( last_data )
        {
            progress = progressType_PROGRESS_LAST_MSG;
        }
        
        if(err == ERR_EVTMGR_NO_EVT_BLK_PTRS)
        {
            corona_print("NO DATA\n");
            err = ERR_OK;
            goto done_dumping_to_server;
        }
        else if(err != ERR_OK)
        {
            WPRINT_ERR("pktinfo %x", err);
            goto done_dumping_to_server;
        }
        
        // Set upload_total counter to 0 Bytes for this transfer
        upload_total = 0;
        
        /*
         *   Build WMP Header for this particular packet.
         */

        RTC_get_time_ms( &timestamp );
        
        wmp_stream = pb_ostream_from_buffer( pWmpHeaderBuf, g_st_cfg.evt_st.max_upload_transfer_len + WMP_HEADER_LEN );
        UTIL_time_with_boot(((uint_64) timestamp.SECONDS*1000) + timestamp.MILLISECONDS, &l_timestamp2);
        
        if( (progressType_PROGRESS_FIRST_MSG == progress) && (first_msg_sent) )
        {
            progress = progressType_PROGRESS_FIRST_MSG + 1;  // only send "first message" one (good) time.
        }

        WMP_DATADUMP_encode_wmp_and_datadump(   &wmp_stream,
                                                g_st_cfg.fac_st.serial_number,            // serial number
                                                strlen(g_st_cfg.fac_st.serial_number)+1,  // serial number length
                                                123546,                                   // magic checksum
                                                l_timestamp2,                             // current_time in ms with current boot #
                                                progress,                                 // progress of data dump
                                                packet_len);                              // payload size
        
        wmp_header_len = (uint32_t) ( (uint32_t) wmp_stream.state - (uint32_t) pWmpHeaderBuf ); //wmp_stream.bytes_written - packet_len;
        if( WMP_HEADER_LEN < wmp_header_len )
        {
            //corona_print("EM WMP Hdr Oflow\n");
            wassert(0);
        }
        
        /*
         *   Open a 'socket'
         */
        err = bark_open(g_conmgr_handle, wmp_stream.bytes_written);
        if(err != BARK_OPEN_OK)
        {
        	if (BARK_OPEN_NOT_CONNECTED == err || BARK_OPEN_CRITICAL == err)
        	{
        		err = PROCESS_NINFO(ERR_EVTMGR_NOT_CONNECTED, NULL);
        		goto done_dumping_to_server;
        	}
        	else
        	{
				err = ERR_EVTMGR_DUMP_TO_SERV_GENERIC;
				PROCESS_NINFO(STAT_EVTUP_RETRY_OPEN, "%d", err);
				sequential_general_errors++;
				goto dump_data_to_server_attempt_fail;
        	}
        }

        /*
         *   Send just the WMP header next.
         */
        err = bark_send(g_conmgr_handle, pWmpHeaderBuf, wmp_header_len, &send_result);
        
        if (send_result != wmp_header_len)
        {
        	if (BARK_SEND_NOT_CONNECTED == err)
        	{
        		err = PROCESS_NINFO(ERR_EVTMGR_NOT_CONNECTED, NULL);
				goto done_dumping_to_server;
        	}
        	else
        	{
        		err = ERR_EVTMGR_DUMP_TO_SERV_CONMGR_SEND;
				PROCESS_NINFO(STAT_EVTUP_RETRY_SEND_HDR, "%x %d", err, send_result);
        		sequential_general_errors++;
				goto dump_data_to_server_attempt_fail;
        	}
        }
        
        /*
         *   Invoke an EVTMGR dump for the payload portion of the data.
         */
        corona_print("\tUpload %d blks (%d)\n", num_evt_blocks, packet_len);
        
        /* Clear potential stale events before evoking them */
        _lwevent_clear(&g_evtmgr_lwevent, EVTMGR_DUMPED_PACKET_SUCCESS | EVTMGR_DUMPED_PACKET_FAIL | EVTMGR_NO_DATA_TO_DUMP);
        err = EVTMGR_dump(num_evt_blocks, _EVTMGR_server_dump_cbk, TRUE);
        _watchdog_start(EVTMGR_UPLOAD_DEF_WD);
        if(err != ERR_OK)
        {
            WPRINT_ERR("dump %x", err);
            goto done_dumping_to_server;
        }
        
        /*
         *   Wait for the EVTMGR server data dump callback to complete on this packet.
         */
        _lwevent_wait_ticks(&g_evtmgr_lwevent, EVTMGR_DUMPED_PACKET_SUCCESS | 
                                               EVTMGR_DUMPED_PACKET_FAIL    | 
                                               EVTMGR_NO_DATA_TO_DUMP, 
                                               FALSE, 0);
        
        if (PWRMGR_is_rebooting())
        {
            corona_print("dumpdata REBOOT\n");
            err = ERR_EVTMGR_DUMP_TO_SERV_GENERIC;
            goto done_dumping_to_server;
        }
        
        mask = _lwevent_get_signalled();
        if(mask & EVTMGR_DUMPED_PACKET_FAIL)
        {
            err = ERR_EVTMGR_DUMP_TO_SERV_GENERIC;
            sequential_general_errors++;
			PROCESS_NINFO(STAT_EVTUP_RETRY_SEND_DATA, "pkt FAIL");
			goto dump_data_to_server_attempt_fail;
        }
        else if(mask & EVTMGR_NO_DATA_TO_DUMP)
        {
            err = ERR_OK;
			PROCESS_NINFO(STAT_EVTUP_NO_DATA, NULL);
            goto done_dumping_to_server;
        }
        wassert(mask & EVTMGR_DUMPED_PACKET_SUCCESS);
        
        /*
         *   Try and receive a response from the HTTP host server.
         */
        corona_print("rx..\n");
        _watchdog_start(EVTMGR_UPLOAD_TCP_RECEIVE_TIMEOUT_WD);
        err = bark_receive(g_conmgr_handle, (void *) pResponseBuf, MAX_EVTMGR_RX_BUFFER_SIZE, EVTMGR_UPLOAD_TCP_RECEIVE_TIMEOUT, &received );
        if ( BARK_RECEIVE_OK == err)
        {
        	sequential_parse_failures = 0;
        	sequential_general_errors = 0;
			if (!g_st_cfg.evt_st.debug_hold_evt_rd_ptr)
			{
			    _EVTMGR_lock();
				_EVTMGR_set_evt_blk_rd_addr( g_Info.wip_evt_blk_rd_addr );
				_EVTMGR_flush_info(&g_Info);
				_EVTMGR_unlock();
			}
			
	        if( progressType_PROGRESS_FIRST_MSG == progress )
	        {
	            first_msg_sent = 1;  // only want to set this to true if server successfully ack'd our "first message sent".
	        }
	        
	        left_to_upload = total_data_to_upload-_EVTMGR_total_data_avail();
	        WMP_DATADUMP_get_progress(left_to_upload, total_data_to_upload, &progress);
	        corona_print("\tPROGRESS %u%% (%u of %u mem span)\n", progress, left_to_upload, total_data_to_upload);
	        PROCESS_NINFO(STAT_EVTUP_SINGLE_TRANSFER_COMPLETE, "%d", packet_len);
        }
        else if (BARK_RECEIVE_RESP_PARSE_FAIL == err)
        {
        	sequential_parse_failures++;
			PROCESS_NINFO(STAT_EVTUP_RETRY_RECEIVE, "%d", err);
        }
        else if (BARK_RECEIVE_NOT_CONNECTED == err  || BARK_RECEIVE_CRITICAL == err)
        {
            err = ERR_EVTMGR_NOT_CONNECTED; //Already done inside wifi receive function
            goto done_dumping_to_server;
        }
        else
        {
        	sequential_general_errors++;
			PROCESS_NINFO(STAT_EVTUP_RETRY_RECEIVE, "%d", err);
        }
        
        if (sequential_parse_failures > g_st_cfg.con_st.max_seq_parse_failures)
		{
			err = PROCESS_NINFO(ERR_EVTMGR_DUMP_TO_SERV_MAX_PARSE_FAIL, NULL);
			goto done_dumping_to_server;
		}
        
dump_data_to_server_attempt_fail:
		if(sequential_general_errors > MAX_SEQ_GENERAL_FAILURES)
		{
			err = PROCESS_NINFO(ERR_EVTMGR_MAX_TCP_ERR, NULL);
			goto done_dumping_to_server;
		}
		
        if (PWRMGR_is_rebooting())
        {
            corona_print("dumpdata REBOOT\n");
            err = ERR_EVTMGR_DUMP_TO_SERV_GENERIC;
            goto done_dumping_to_server;
        }
    }
    
done_dumping_to_server:
    CONMGR_close(g_conmgr_handle);
    wfree(pWmpHeaderBuf);
    wfree(pResponseBuf);
    _watchdog_stop();
    return err;
}

static void _EVTMGR_process_opened_event(boolean success)
{
    err_t err;

    if (!success)
    {
        WPRINT_ERR("open");
        _EVTMGR_con_mgr_register_any(0, g_st_cfg.evt_st.data_dump_seconds);
        return;
    }
    
    /*
     *   Before dumping, let's make sure all the latest EVTMGR blocks have been flushed.
     */
    _EVTMGR_lock();
    _EVTMGR_flush_evt_blk_ptrs(0);
    _EVTMGR_unlock();

    err = _EVTMGR_dump_srv();
    if(err != ERR_OK)
    {
        WPRINT_ERR("dump %x", err);
    }
    
    wassert(ERR_OK == CONMGR_release(g_conmgr_handle));
    
    //corona_print("Done.Re-reg\n");
    _EVTMGR_con_mgr_register_any(0, g_st_cfg.evt_st.data_dump_seconds);
}

static void _EVTMGR_process_closed_event(void)
{
    //corona_print("con down.ReReg\n");
    _EVTMGR_con_mgr_register_any(0, g_st_cfg.evt_st.data_dump_seconds);
}

/*
 *   EVTMGR task responsible for listening for events related to uploading/dumping data.
 */
void EVTUPLD_tsk(uint_32 dummy)
{       
    _mqx_uint mask;

    // Comment out this line before running the evt_mgr upload unit test    
    _EVTMGR_upload_init();

    while (1)
    {
    	PWRMGR_VoteForOFF(PWRMGR_EVTMGR_UPLOAD_VOTE);

    	if ( g_evtmgr_shutting_down )
        {
            goto DONE;
        }

        _lwevent_wait_ticks(&g_evtmgr_lwevent, EVTMGR_CONMGR_CONNECT_SUCCEEDED_EVENT |
                                               EVTMGR_CONMGR_CONNECT_FAILED_EVENT    | 
                                               EVTMGR_CONMGR_DISCONNECTED_EVENT,
                                               FALSE, 0);
        
        if ( g_evtmgr_shutting_down )
        {
            goto DONE;
        }
        
        PWRMGR_VoteForON(PWRMGR_EVTMGR_UPLOAD_VOTE);  // This ON vote should be redundant, but do it jic

        mask = _lwevent_get_signalled();

        if (mask & EVTMGR_CONMGR_CONNECT_SUCCEEDED_EVENT)
        {
            _EVTMGR_process_opened_event(TRUE);
        }

        if (mask & EVTMGR_CONMGR_CONNECT_FAILED_EVENT)
        {
            _EVTMGR_process_opened_event(FALSE);
        }

        if (mask & EVTMGR_CONMGR_DISCONNECTED_EVENT)
        {
            _EVTMGR_process_closed_event();
        }
    }
    
DONE:
    PWRMGR_VoteExit(PWRMGR_EVTMGR_UPLOAD_VOTE);
    _task_block(); // Don't exit or our MQX resources may be deleted
}



