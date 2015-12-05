/*
 * cfg_mgr_priv.h
 *
 *  Created on: May 6, 2013
 *      Author: Ernie
 */

#ifndef CFG_MGR_PRIV_H_
#define CFG_MGR_PRIV_H_

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include <stdint.h>
#include "app_errors.h"

////////////////////////////////////////////////////////////////////////////////
// Defines
////////////////////////////////////////////////////////////////////////////////
#define SEC_MODE_OPEN               (0)
#define SEC_MODE_WEP                (1)
#define SEC_MODE_WPA                (2)

/*
 *   Static Configs Magic Number
 *   Change the Version to reset to new configs.
 */
#define CFG_STATIC_VERS     (22)
#define CFG_STATIC_MAGIC    (0xFEEDF2CE + CFG_STATIC_VERS)

////////////////////////////////////////////////////////////////////////////////
// Structures
////////////////////////////////////////////////////////////////////////////////


/*
 *   Various states for CFGMGR's ping pong storage.
 */
typedef enum config_pingpong_state_type
{
    
    CFG_PP_BOTH_SECTORS_INVALID = 0,
    CFG_PP_ONLY_SECTOR1_INVALID = 1,
    CFG_PP_ONLY_SECTOR2_INVALID = 2,
    CFG_PP_SECTOR1_NEWER        = 3,
    CFG_PP_SECTOR2_NEWER        = 4
    
}cfg_pp_state_t;

/*
 *   CFGMGR header - IMPORTANT:  NEVER change this structure.
 */
typedef struct config_header_type
{
    uint32_t signature;       // can NEVER change, unless you want customers to lose everything.
    uint32_t length;          // number of serialized bytes written.
    uint32_t ping_pong_count; // keep counting, bigger count wins.
    uint32_t this_address;    // address of the sector we wrote this to.
    
    uint32_t reserved_0;
    uint32_t reserved_1;
    uint32_t reserved_2;
    uint32_t reserved_3;
    
}cfg_header_t;

////////////////////////////////////////////////////////////////////////////////
// Private Declarations
////////////////////////////////////////////////////////////////////////////////
err_t _CFG_enc(void);
err_t _CFG_dec(void);
err_t _CFGMGR_get_encoded_data(cfg_header_t *pHeader, uint8_t *pSerializedData);
err_t _CFGMGR_prep_header_for_encoding(cfg_header_t *pHeader, uint32_t data_length);
err_t _CFGMGR_get_pingpong_state( cfg_pp_state_t *pState, 
                                  uint32_t *pNewPingPongCount,
                                  cfg_header_t *pNewestSectorHeader );

void _CFGMGR_static_defaults(void);
void _CFGMGR_dynamic_defaults(void);

#endif /* CFG_MGR_PRIV_H_ */
