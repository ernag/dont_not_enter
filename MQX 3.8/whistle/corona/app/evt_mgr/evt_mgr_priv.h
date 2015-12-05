/*
 * evt_mgr_priv.h
 *
 *  Created on: Apr 5, 2013
 *      Author: Ernie
 *      https://whistle.atlassian.net/wiki/display/COR/Event+Manager+Memory+Management+Design
 */

#ifndef EVT_MGR_PRIV_H_
#define EVT_MGR_PRIV_H_

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include "psptypes.h"
#include "app_errors.h"
#include <mqx.h>

////////////////////////////////////////////////////////////////////////////////
// Defines
////////////////////////////////////////////////////////////////////////////////
#define EVTMGR_VERSION                     ( 3 )     // NOTE: changing this causes total data loss.
#define EVTMGR_MAGIC_SIGNATURE             ( 0xDEAFBEAD + EVTMGR_VERSION )

#define MAX_EVT_BLK_LEN                    (4096)  // max allowable size of a single evt blk.

/*
 *   EVTMGR Sys-Events used to provoke an upload.
 */
#define EVTMGR_CONMGR_CONNECT_SUCCEEDED_EVENT 0x0001
#define EVTMGR_CONMGR_CONNECT_FAILED_EVENT    0x0002
#define EVTMGR_CONMGR_DISCONNECTED_EVENT      0x0004
#define EVTMGR_CONMGR_BUTTON_EVENT            0x0008
#define EVTMGR_DUMPED_PACKET_SUCCESS          0x0010
#define EVTMGR_DUMPED_PACKET_FAIL             0x0020
#define EVTMGR_NO_DATA_TO_DUMP                0x0040
#define EVTMGR_POST_ENQUEUED_EVENT            0x0080
#define EVTMGR_DUMP_ENQUEUED_EVENT            0x0100
#define EVTMGR_FLUSH_REQUEST_EVENT            0x0200
#define EVTMGR_SHUTDOWN_EVENT                 0x0400

////////////////////////////////////////////////////////////////////////////////
// Structs
////////////////////////////////////////////////////////////////////////////////
//#pragma pack(push, 1)
typedef struct evtmgr_info_type
{     
    uint_32 reserved[4];         // in case we need to add something in the future without wrecking users' data.
    uint_32 write_addr;          // SPI Flash physical address where we want to write event blocks next.
    uint_32 evt_blk_rd_addr;     // Address of Event Block Pointer in SPI Flash, where we read (and destroy if sent to server) the next old event block pointer.
    uint_32 evt_blk_wr_addr;     // Address of Event Block Pointer in SPI Flash, where we create the next new event block pointer.
    uint_32 wip_evt_blk_rd_addr; // Address of event block pointer in SPI Flash, where the data dump flusher is currently at.
    uint_32 signature;           // a magic number to let us know that the EVTMGR is good in FLASH.
    uint_32 crc;                 // CRC of everything above.
}evtmgr_info_t;
//#pragma pack(pop)

typedef enum evtmgr_atomic_vote_type
{
	EVTMGR_ATOMIC_VOTE_ON,
	EVTMGR_ATOMIC_VOTE_OFF
}evtmgr_atomic_vote_t;

////////////////////////////////////////////////////////////////////////////////
// Externs
////////////////////////////////////////////////////////////////////////////////
extern evtmgr_info_t   g_Info; 

////////////////////////////////////////////////////////////////////////////////
// Structures
////////////////////////////////////////////////////////////////////////////////
//#pragma pack(push, 1)
typedef struct event_block_pointer_type
{
    uint_32 addr; // address where event block is stored.
    uint_32 crc;  // CRC of the event block.
    uint_16 len;  // length of the event block.
}evt_blk_ptr_t, *evt_blk_ptr_ptr_t;
//#pragma pack(pop)

////////////////////////////////////////////////////////////////////////////////
// EVTMGR's Private Declarations
////////////////////////////////////////////////////////////////////////////////
err_t _EVTMGR_create_blk_ptr(evt_blk_ptr_ptr_t ptr);
err_t _EVTMGR_wr_evt_blk(evt_blk_ptr_ptr_t pBlkPtr, uint_8 *pPayload);
err_t _EVTMGR_load_evt_blk_ptrs( uint32_t **pCacheHandle, uint_32 addr, uint_16 *pNumPtrs );
err_t _EVTMGR_unload_evt_blk_ptrs( uint32_t **pCacheHandle);
err_t _EVTMGR_update_rd_blk_ptr(uint_32 addr);
void _EVTMGR_add_blk_ptr(evt_blk_ptr_ptr_t pBlkPtr);
void _EVTMGR_flush_evt_blk_ptrs(uint_32 num_bytes);
void _EVTMGR_prep_mem(uint_32 addr);
void _EVTMGR_write_page(uint_32 addr, uint_8 *pPage, uint_16 len);
void  _EVTMGR_init_info(uint_8 force_reset);
void  _EVTMGR_init_blk_ptr_buf(void);
void  _EVTMGR_set_evt_blk_rd_addr(uint_32 addr);
void  _EVTMGR_inc_wr_addr(void);
void  _EVTMGR_lock(void);
void  _EVTMGR_unlock(void);
void  _EVTMGR_flush_info( evtmgr_info_t *pInfo );
void  _EVTMGR_read_info( evtmgr_info_t *pInfo );
void  _EVTMGR_atomic_vote( evtmgr_atomic_vote_t vote );
uint_8  _EVTMGR_check_crc(evtmgr_info_t *pInfo);
uint_8  _EVTMGR_is_locked(void);
uint_8  _EVTMGR_is_flushed( void );
uint_8  _EVTMGR_is_sector_boundary(uint_32 addr);
uint_8  _EVTMGR_addr_is_valid(uint_32 addr);
uint_8  _EVTMGR_blk_ptr_is_valid(uint_32 addr);
uint_8  _EVTMGR_has_data_to_send(void);
uint_8  _EVTMGR_get_blk_data(evt_blk_ptr_ptr_t pEvtBlkPtr, uint8_t *pData);
uint_32 _EVTMGR_total_data_avail(void);
uint_32 _EVTMGR_crc( evtmgr_info_t *pInfo );
uint_32 _EVTMGR_get_wr_addr(void);
uint_32 _EVTMGR_get_evt_blk_rd_addr(void);
uint_32 _EVTMGR_get_evt_blk_wr_addr(void);
uint_32 _EVTMGR_update_page_addr(uint_32 addr);
uint_32 _EVTMGR_update_evt_blk_ptr_addr(uint_32 addr);
uint_32 _EVTMGR_next_sector(uint32_t address);
uint32_t _EVTMGR_next_evt_blk_sector(uint32_t address);
evt_blk_ptr_ptr_t _EVTMGR_get_blk_ptr(uint32_t **pCacheHandle, uint_16 idx);
int _EVTMGR_VoteForPowerON(boolean requestON);
void __TEST_check_for_corruption(uint8_t *pData, uint32_t addr, uint16_t len);

#endif /* EVT_MGR_PRIV_H_ */
