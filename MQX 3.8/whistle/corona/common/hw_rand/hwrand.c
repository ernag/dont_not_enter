/*
 * hwrand.c
 *
 *  Created on: Jul 15, 2013
 *      Author: SpottedLabs
 */

#include <mqx.h>
#include "hwrand.h"
#include "rng1.h"
#include "app_errors.h"

/*
 *   Test the random number generator built into hardware on the K60.
 */
err_t hwrand_uint32(uint32_t *randNum)
{
    err_t res;
    LDD_TError Error;
    uint32_t number;
    uint32_t status;
    uint32_t i;
    
    RNG1_Init();  /* Initialization of RNG component */
    
    i = 0;
    do
    {
        status = RNG1_GetStatus();
        i++;
    } while ( (!((status & RNG_SR_OREG_LVL_MASK) >> RNG_SR_OREG_LVL_SHIFT)) && (i < 50));

    if (i >= 50)
    {
        WPRINT_ERR("RNG init");
        res = ERR_GENERIC;
        goto _hwrand_uint32_done;
    }

    Error = RNG1_GetRandomNumber(&number);          /* Read random data from the FIFO */
        
    /*
     *   TODO:  Check the status register for real errors in the final implementation to assure truly random values.
     */
    
    if (Error == ERR_FAULT) 
    {
        /* RNG failed, reset module and try re-initialization. */
        WPRINT_ERR("RNG %x", Error);
        res = ERR_FAULT;
    }
    else
    {
        *randNum = number;
        res = ERR_OK;
    }
    
_hwrand_uint32_done:
    RNG1_Close();
    return res;
}
