/*
 * crc_util.c
 * Use the Kinetis CRC hardware block to generate a CRC based on the passed in payload.
 *
 *  Created on: Apr 8, 2013
 *      Author: Ernie
 */

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include <mqx.h>
#include <bsp.h>
#include <mutex.h>
#include "crc_util.h"
#include "psptypes.h"
#include "coronak60n512.h"
#include "wassert.h"
#include "cormem.h"
#include "crc_sw_priv.h"
#include <MK60D10.h>
#include <stdint.h>
#include <string.h>

////////////////////////////////////////////////////////////////////////////////
// Defines
////////////////////////////////////////////////////////////////////////////////

/*
 *   Standard CRC Polynomials
 */
#define CRC32_POLY_HIGH_HALF_WORD     0x04C1
#define CRC32_POLY_LOW_HALF_WORD      0x1DB7
#define CRC8_POLY_HIGH_HALF_WORD      0x0000
#define CRC8_POLY_LOW_HALF_WORD       0x00D5

////////////////////////////////////////////////////////////////////////////////
// Externs
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// Private Declarations
////////////////////////////////////////////////////////////////////////////////

/*
 *   Initializes the CRC block registers on the K60.
 */
static void _UTIL_crc32_block_init(crc_handle_t *pHandle, uint_32 seed);

/*
 *   Reverses the bits in a byte
 */
static uint_8 reverse_byte(uint_8 in);

/*
 *   Convert a reversed & XOR'd result to be a continued CRC seed
 */
static uint_32 convert_res_to_seed(uint_32 res);

////////////////////////////////////////////////////////////////////////////////
// Variables
////////////////////////////////////////////////////////////////////////////////
static const uint_32 HIGH_TO_LOW_REVERESE_NIBBLE_LUT[] = {0b00000000, // 0: 0000
                            0b00001000, // 1: 0001
                            0b00000100, // 2: 0010
                            0b00001100, // 3: 0011
                            0b00000010, // 4: 0100
                            0b00001010, // 5: 0101
                            0b00000110, // 6: 0110
                            0b00001110, // 7: 0111
                            0b00000001, // 8: 1000
                            0b00001001, // 9: 1001
                            0b00000101, //10: 1010
                            0b00001101, //11: 1011
                            0b00000011, //12: 1100
                            0b00001011, //13: 1101
                            0b00000111, //14: 1110
                            0b00001111, //15: 1111
};

static const uint_32 LOW_TO_HIGH_REVERESE_NIBBLE_LUT[] = {0b00000000, // 0: 0000
                            0b10000000, // 1: 0001
                            0b01000000, // 2: 0010
                            0b11000000, // 3: 0011
                            0b00100000, // 4: 0100
                            0b10100000, // 5: 0101
                            0b01100000, // 6: 0110
                            0b11100000, // 7: 0111
                            0b00010000, // 8: 1000
                            0b10010000, // 9: 1001
                            0b01010000, //10: 1010
                            0b11010000, //11: 1011
                            0b00110000, //12: 1100
                            0b10110000, //13: 1101
                            0b01110000, //14: 1110
                            0b11110000, //15: 1111
};

static MUTEX_STRUCT g_CRC_mutex;

////////////////////////////////////////////////////////////////////////////////
// Code
////////////////////////////////////////////////////////////////////////////////

/*
 *   This function will wait for a mutex, then reset the CRC module for new usage.
 */
void UTIL_crc32_reset(crc_handle_t *pHandle, uint_32 seed)
{
    if( MQX_OK == _mutex_try_lock(&g_CRC_mutex) )
    {
    	/*
    	 *   The hardware CRC block is available.
    	 *   Use it since it is more optimal.
    	 */
    	pHandle->type = CRC_CALC_HARDWARE_BASED;
    }
    else
    {
    	/*
    	 *   Someone already got dibbs on the K60 CRC hardware peripheral.  So we don't block
    	 *      that poor soul, let's go ahead and calculate it software-wise on their behalf.
    	 */
    	pHandle->type = CRC_CALC_SOFTWARE_BASED;
    }
    
    _UTIL_crc32_block_init(pHandle, seed);
}

/*
 *   This function must be called when you are done with the CRC block, to free
 *     it up for others to use.
 */
void UTIL_crc_release(crc_handle_t *pHandle)
{
	if( CRC_CALC_HARDWARE_BASED == pHandle->type )
	{
		wint_disable(); // SIM_* can be accessed in other drivers not protected by our lock
	    SIM_SCGC6 &= ~(SIM_SCGC6_CRC_MASK);  // gate the clock to the CRC block, not needed while not in use.
	    wint_enable();
	    
	    _mutex_unlock(&g_CRC_mutex);
	    return;
	}
}

static void _UTIL_crc32_hw_init(uint_32 seed)
{
	wint_disable(); // SIM_* can be accessed in other drivers not protected by our lock
    SIM_SCGC6 |= SIM_SCGC6_CRC_MASK;
    wint_enable();
    
    /*
     *   WARNING! Changing this means you need to make changes to the software-based CRC.
     * 
     *   Set up the CRC control register for:
     *      # 32-bit
     *      # Setting up the seed
     *      # XOR enable (invert the read value)
     *      # No Transpose
     */
    CRC_CTRL = \
            (uint32_t)(( \
             (uint32_t)(( \
              (uint32_t)(( \
                      CRC_CTRL) | (( \
               CRC_CTRL_TCRC_MASK) | (( \
               CRC_CTRL_WAS_MASK) | ( \
               CRC_CTRL_FXOR_MASK))))) & (( \
              (uint32_t)(~(uint32_t)CRC_CTRL_TOT_MASK)) & ( 
              (uint32_t)(~(uint32_t)CRC_CTRL_TOTR_MASK))))) | (( \
             (uint32_t)((uint32_t)0x1U << CRC_CTRL_TOT_SHIFT)) | ( \
             (uint32_t)((uint32_t)0x2U << CRC_CTRL_TOTR_SHIFT))));
    
    /* CRC_CRC: HU=0xFF,HL=0xFF,LU=0xFF,LL=0xFF */
    CRC_CRC = seed;          /* Setup seed */
    
    /* CRC_GPOLY: HIGH=0x04C1,LOW=0x1DB7 */
    CRC_GPOLY = (CRC_GPOLY_HIGH( CRC32_POLY_HIGH_HALF_WORD ) | CRC_GPOLY_LOW( CRC32_POLY_LOW_HALF_WORD )); /* Setup polynomial */
    CRC_CTRL &= (uint32_t)~(uint32_t)(CRC_CTRL_WAS_MASK); /* Clear seed bit so writes become data. */
}

static void _UTIL_crc32_sw_init(void)
{
	F_CRC_InicializaTabla();
}

static void _UTIL_crc32_block_init(crc_handle_t *pHandle, uint_32 seed)
{
	if(CRC_CALC_HARDWARE_BASED == pHandle->type)
	{
		_UTIL_crc32_hw_init(seed);
	}
	else
	{
		_UTIL_crc32_sw_init();
		pHandle->last_crc32 = CRC_DEFAULT_SEED;
		pHandle->last_table_val = CRC_DEFAULT_SEED;
	}
}

void UTIL_crc_init(void)
{
	wmutex_init( &g_CRC_mutex );
}

/*
 *   Calculates the current CRC value based on 8-bit data.
 */
uint_32  UTIL_crc_get8(crc_handle_t *pHandle, uint_8 data)
{    
	if( CRC_CALC_HARDWARE_BASED == pHandle->type )
	{
	    CRC_CRCLL = CRC_CRC_LL(data);  /* Write only 8-bits. */
	    return CRC_CRC;
	}
	
	if( CRC_CALC_SOFTWARE_BASED == pHandle->type )
	{
		pHandle->last_table_val = F_CRC_CalculaCheckSum((uint_8 const *)&data, 1,  pHandle->last_table_val);
		pHandle->last_crc32 = F_CRC_CalcFinalVal(pHandle->last_table_val, CRC_DEFAULT_SEED);
		return pHandle->last_crc32;
	}
	
	return 0xFEEDBEEF;
}

/*
 *   Calculates the current CRC value based on 32-bit data.
 */
uint_32  UTIL_crc_get32(crc_handle_t *pHandle, uint_32 data)
{   
	if( CRC_CALC_HARDWARE_BASED == pHandle->type )
	{
	    CRC_CRC = data;
	    return CRC_CRC;
	}
	
	if( CRC_CALC_SOFTWARE_BASED == pHandle->type )
	{
		uint_8 little[4];
		uint_8 const *pTemp = (uint_8 const *)&data;
		
		// Need to convert 32-bit number to little Endian.
		little[0] = pTemp[3];
		little[1] = pTemp[2];
		little[2] = pTemp[1];
		little[3] = pTemp[0];
		
		pHandle->last_table_val = F_CRC_CalculaCheckSum((uint_8 const *)little, 4,  pHandle->last_table_val);
		pHandle->last_crc32 = F_CRC_CalcFinalVal(pHandle->last_table_val, CRC_DEFAULT_SEED);
		return pHandle->last_crc32;
	}
	
	return 0xFEEDFACE;
}

/*
 *   Calculates the whole 32-bit CRC result for you, if you already have an 8-bit payload to feed in.
 */
uint_32 UTIL_crc32_calc(uint_8 *pPayload, uint_32 len)
{
    return UTIL_crc32_progressive_calc(pPayload, len, PROGRESSIVE_CRC_INIT);
}

/*
 *  Used for chained CRC calculations.
 */
uint_32 UTIL_crc32_progressive_calc(uint_8 *pPayload, uint_32 len, uint_32 last_crc)
{
    uint_32 i;
    uint_32 result;
    crc_handle_t crc_handle;
    
    wassert(len > 0);
    
    UTIL_crc32_reset( &crc_handle, convert_res_to_seed( last_crc ) );
    
    for(i = 0; i < len; i++)
    {
        result = UTIL_crc_get8( &crc_handle, pPayload[i] );
    }
    
    UTIL_crc_release(&crc_handle);
    
    return result;
}

static uint_8 reverse_byte(uint_8 in)
{
    return LOW_TO_HIGH_REVERESE_NIBBLE_LUT[in & 0xF] | (HIGH_TO_LOW_REVERESE_NIBBLE_LUT[(in >> 4) & 0xF]);
}

static uint_32 convert_res_to_seed(uint_32 res)
{
    return (~(reverse_byte(((uint_8*) &res)[3]) | 
            reverse_byte(((uint_8*) &res)[2]) << 8 | 
            reverse_byte(((uint_8*) &res)[1]) << 16 |
            reverse_byte(((uint_8*) &res)[0]) << 24));
}
