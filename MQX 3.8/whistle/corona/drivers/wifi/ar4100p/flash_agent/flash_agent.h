/*
 * flash_agent.h
 *
 *  Created on: Mar 12, 2013
 *      Author: Chris
 */

#ifndef FLASH_AGENT_H_
#define FLASH_AGENT_H_

#include "ar4100p_main.h"

#define VERSION    (4)

#ifndef ATH_TARGET
#include "athstartpack.h"
#endif
typedef struct _request
{
    unsigned short cmdId  FIELD_PACKED;
    unsigned char         buffer[1024] FIELD_PACKED;
    unsigned int addr     FIELD_PACKED;
    unsigned int option   FIELD_PACKED;
    unsigned short length FIELD_PACKED;
    unsigned short crc    FIELD_PACKED;
} request_t;


typedef struct _response
{
    unsigned short cmdId FIELD_PACKED;
    unsigned int result  FIELD_PACKED;
    version_t version    FIELD_PACKED;
} response_t;
#ifndef ATH_TARGET
#include "athendpack.h"
#endif

enum cmd_id
{
    PROGRAM = 0,
    PROGRAM_RESULT,
    EXECUTE,
    EXECUTE_RESULT,
    CRC_ERROR,
    VERSION_NUM,
    FW_VERSION,
    UNKNOWN_CMD
};

void Flash_Agent_Task(uint_32);
void WifiConnected(int val);
void reset_atheros_driver_setup_init(void);
void atheros_driver_setup_mediactl(void);
void atheros_driver_setup_init(void);
#endif /* FLASH_AGENT_H_ */
