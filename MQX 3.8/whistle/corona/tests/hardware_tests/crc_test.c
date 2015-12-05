/*
 * crc_test.c
 *
 *  Created on: Apr 11, 2013
 *      Author: Ernie
 */

#include "unit_tests.h"
#ifdef ENABLE_WHISTLE_UNIT_TESTS

#include "crc_test.h"
#include "crc_util.h"
#include "psptypes.h"
#include "wassert.h"
#include <stdio.h>

#define NUM_FAKE_BYTES      10
#define NUM_CRC_TEST_BYTES	514  // tests wrap around twice.
#define NUM_CRC_TEST_32		65

    // +ktZ09~abc
    uint_8 fake[NUM_FAKE_BYTES] = {'+',  'k',  't',  'Z',  '0',  '9',  '~',  'a',  'b',  'c'};
//    uint_8 fake[NUM_FAKE_BYTES] = {0x2B, 0x6B, 0x74, 0x5A, 0x30, 0x39, 0x7E, 0x61, 0x62, 0x63};
    uint_32 fake_progressive_crc_truth[] = {0x7ebe16cd, 0xedc95746, 0x1a38775a, 0xd218d7fa, 0xa961dcf4, 0x377e305c, 0x0755d059, 0xfa0702c3, 0x73297bf1, 0xcc70349e};
    uint_8 test_reverse_byte_in[4] =  {0x0D, 0x3F, 0xC4, 0xD9};
    uint_8 test_reverse_byte_out[4];
    uint_8 test_reverse_byte_truth[4] = {0xB0, 0xFC, 0x23, 0x9B};

/*
 *    Test to make sure the hardware CRC and software CRC are agreeing with each other,
 *       and both can be run in parallel.
 */
void crc_mutex_test(void)
{
	crc_handle_t hw_crc_hdl, sw_crc_hdl;
	uint_16 i = 0;
	uint_32 hw_result, sw_result, prog_result;
	uint_8 test_buf8[NUM_CRC_TEST_BYTES];
	uint_32 test_buf32[NUM_CRC_TEST_32];
	
	while(i < NUM_CRC_TEST_BYTES)
	{
		test_buf8[i] = (uint_8)i++;   // meaningless test data.
	}
	
	hw_crc_hdl.type = sw_crc_hdl.type = CRC_CALC_UNINITIALIZED;  // this is not necessary for production code.
	
	/*
	 *   Block until we get a hardware based CRC.
	 */
	while(hw_crc_hdl.type != CRC_CALC_HARDWARE_BASED)
	{
		corona_print("Waiting for HW CRC...\n");
		UTIL_crc32_reset(&hw_crc_hdl, CRC_DEFAULT_SEED);
		_time_delay(500);  // wait until we have the hardware one.
	}
	
	corona_print("Grab SW CRC\n");
	UTIL_crc32_reset(&sw_crc_hdl, CRC_DEFAULT_SEED);
	wassert(sw_crc_hdl.type == CRC_CALC_SOFTWARE_BASED);
	
	/*
	 *   Make sure the results of each CRC calculation is matches for each of these.
	 */
	for(i=0; i < NUM_CRC_TEST_BYTES; i++)
    {	
        hw_result = UTIL_crc_get8(&hw_crc_hdl, test_buf8[i]);
        sw_result = UTIL_crc_get8(&sw_crc_hdl, test_buf8[i]);
        
        if( hw_result != sw_result )
        {
        	corona_print("ERR: CRC8_HW: %x\tCRC8_SW: %x\tindex: %i\n", hw_result, sw_result, i);
        	_time_delay(50);
        }
    }
	
	/*
	 *   Now make sure prog result (which will be software based), is good.
	 */
	prog_result = UTIL_crc32_calc(test_buf8, NUM_CRC_TEST_BYTES);
	if(prog_result != hw_result || prog_result != sw_result)
	{
		corona_print("ERR: CRC_HW: %x\tCRC_SW: %x\tCRC_PROG: %x\n", hw_result, sw_result, prog_result);
	}
	
	corona_print("\n\n\tCRC8_HW: %x\tCRC8_SW: %x\tCRC8_PROG: %x\n\n", hw_result, sw_result, prog_result);
	_time_delay(100);
	
	/*
	 *   Release
	 */
	UTIL_crc_release(&sw_crc_hdl);
	UTIL_crc_release(&hw_crc_hdl);
	
	/*
	 *   Check prog result one more time after releasing Hardware Mutex, so you can do prog with HW.
	 */
	prog_result = UTIL_crc32_calc(test_buf8, NUM_CRC_TEST_BYTES);
	if(prog_result != hw_result || prog_result != sw_result)
	{
		corona_print("ERR: CRC8_HW: %x\tCRC8_SW: %x\tCRC8_PROG: %x\n", hw_result, sw_result, prog_result);
	}
	
	/*
	 *   Now do it again for the 32 API's.
	 */
	corona_print("Testing get32 API's\n");
	_time_delay(100);
	
	i = 0;
	while(i < NUM_CRC_TEST_32)
	{
		test_buf32[i] = i*7777;   // meaningless test data.
		i ++;
	}
	
	hw_crc_hdl.type = sw_crc_hdl.type = CRC_CALC_UNINITIALIZED;  // this is not necessary for production code.
	
	/*
	 *   Block until we get a hardware based CRC.
	 */
	while(hw_crc_hdl.type != CRC_CALC_HARDWARE_BASED)
	{
		corona_print("Waiting for HW CRC...\n");
		UTIL_crc32_reset(&hw_crc_hdl, CRC_DEFAULT_SEED);
		_time_delay(500);  // wait until we have the hardware one.
	}
	
	corona_print("Grab SW CRC\n");
	UTIL_crc32_reset(&sw_crc_hdl, CRC_DEFAULT_SEED);
	wassert(sw_crc_hdl.type == CRC_CALC_SOFTWARE_BASED);
	
	/*
	 *   Make sure the results of each CRC calculation is matches for each of these.
	 */
	for(i=0; i < NUM_CRC_TEST_32; i++)
    {	
        hw_result = UTIL_crc_get32(&hw_crc_hdl, test_buf32[i]);
        sw_result = UTIL_crc_get32(&sw_crc_hdl, test_buf32[i]);
        
        if( hw_result != sw_result )
        {
        	corona_print("ERR (32): CRC_HW: %x\tCRC_SW: %x\tindex: %i\n", hw_result, sw_result, i);
        	_time_delay(50);
        }
    }
	
	corona_print("\n\n\tCRC32_HW: %x\tCRC32_SW: %x\n\n", hw_result, sw_result);
	
	UTIL_crc_release(&sw_crc_hdl);
	UTIL_crc_release(&hw_crc_hdl);
}

int crc_test(void)
{   
    uint_16 idx;
    uint_32 result;
    uint_32 result2;
    uint_32 expected_crc = 0xcc70349e;
    uint_32 i;
    int res = 0;
    crc_handle_t crc_handle;
    
    corona_print("\r\nTesting crc32_calc\n");
    
    for(i = 0; i < 10; i++)
    {
        
        corona_print("Iteration %d\n", i);
        
        UTIL_crc32_reset( &crc_handle, CRC_DEFAULT_SEED );
        
        for(idx = 0; idx < NUM_FAKE_BYTES; idx++)
        {
            result = UTIL_crc_get8(&crc_handle, fake[idx]);
            corona_print("Result (%d): 0x%x\n", idx, result);
        }
        
        UTIL_crc_release(&crc_handle);
        result2 = UTIL_crc32_calc(fake, NUM_FAKE_BYTES);
        
        if (result != result2)
        {
            corona_print("FAILED CRC_get8 did not match CRC32_calc!\r\n");
            fflush(stdout);
            res = -1;
        }   
        
        if (result2 != expected_crc)
        {
            corona_print("FAILED Expected value. Got: 0x%x, expected 0x%x.\n", result2, expected_crc);
            fflush(stdout);
            res = -1;
        }
    }
    
    corona_print("\r\nTesting byte/bit reversal\r\n");
    for (i = 0; i < 4; i++)
    {
        test_reverse_byte_out[i] = reverse_byte(test_reverse_byte_in[i]);
        corona_print(" Reverse 0x%.2x -> 0x%.2x \n", test_reverse_byte_in[i], test_reverse_byte_out[i]);
        if (test_reverse_byte_truth[i] != test_reverse_byte_out[i])
        {
            corona_print(" FAILED\n");
            res = -1;
        }
    }

    corona_print("\r\nTesting segmented CRC32\n");
    result = 0x00000000;

    for(i = 0; i < NUM_FAKE_BYTES; i++)
    {
        corona_print(" Item[%d] Seed: 0x%.8x  Input: 0x%.2x", i, result, fake[i]);
        result = UTIL_crc32_progressive_calc(&(fake[i]), 1, result);
        corona_print("  CRC: 0x%.8x\r\n", result);
        if (result != fake_progressive_crc_truth[i])
        {
            corona_print("  FAILED\n");
            res = -1;
        }
    }
    
    
    
    if (res == 0)
        corona_print("CRC Unit Test PASS!\n");
    else
        corona_print("CRC unit test FAIL!\n");
    
    return res;
}
#endif
