/*
 * crc_util.h
 * 
 * See the crc unit test for example usage !
 *
 *  Created on: Apr 8, 2013
 *      Author: Ernie
 */

#ifndef CRC_UTIL_H_
#define CRC_UTIL_H_

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include "psptypes.h"

////////////////////////////////////////////////////////////////////////////////
// Defines
////////////////////////////////////////////////////////////////////////////////
#define PROGRESSIVE_CRC_INIT 0x00000000
#define CRC_DEFAULT_SEED     0xFFFFFFFF

////////////////////////////////////////////////////////////////////////////////
// Typedefs
////////////////////////////////////////////////////////////////////////////////
typedef uint_32 (*get8_cbk_t)(uint_8 data);
typedef uint_32 (*get32_cbk_t)(uint_32 data);

/*
 *   We need to tell the CRC block what type of CRC calculation we intend on using.
 */
typedef enum crc_calculation_type
{
	CRC_CALC_HARDWARE_BASED		= 0xDEADBEEF,
	CRC_CALC_SOFTWARE_BASED		= 0xFEEDFACE,
	CRC_CALC_UNINITIALIZED      = 0x12345678
}crc_calc_t;

typedef struct crc_handle_type
{
	crc_calc_t     type;
	uint_32        last_crc32;
	uint_32        last_table_val;
}crc_handle_t;

////////////////////////////////////////////////////////////////////////////////
// Public Interfaces
////////////////////////////////////////////////////////////////////////////////

/*
 *   Initialize the CRC for usage.
 *      This only needs to be called once, by the main task.
 */
void UTIL_crc_init(void);

/*
 *   Reset the CRC before calculating a fresh one.
 *   This is the first function clients call when they want to use this module.
 *   
 *   You must allocate memory and pass it in for crc_handle.
 *   After returning, you can use the callbacks passed back.
 */
void UTIL_crc32_reset(crc_handle_t *pHandle, uint_32 seed);

/*
 *   Calculate an 8-bit CRC based on the passed in payload.
 */
uint_32  UTIL_crc_get8(crc_handle_t *pHandle, uint_8 data);

/*
 *   Calculates the current 32-bit CRC value based on 32-bit data.
 */
uint_32  UTIL_crc_get32(crc_handle_t *pHandle, uint_32 data);

/*
 *   This function must be called when you are done with the CRC block, to free
 *     it up for others to use.
 */
void UTIL_crc_release(crc_handle_t *pHandle);

/*
 *   Calculates the whole CRC result for you, if you already have an 8-bit payload to feed in.
 */
uint_32 UTIL_crc32_calc(uint_8 *pPayload, uint_32 len);
uint_32 UTIL_crc32_progressive_calc(uint_8 *pPayload, uint_32 len, uint_32 last_crc);

#endif /* CRC_UTIL_H_ */
