//------------------------------------------------------------------------------
// Copyright (c) 2011 Qualcomm Atheros, Inc.
// All Rights Reserved.
// Qualcomm Atheros Confidential and Proprietary.
// Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is
// hereby granted, provided that the above copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
// INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
// USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
//------------------------------------------------------------------------------
//==============================================================================
// Author(s): ="Atheros"
//==============================================================================
#include <a_config.h>
#include <a_types.h>
#include <a_osapi.h>
#include <common_api.h>
#include <spi_hcd_if.h>
#include <targaddrs.h>
#include <AR6002/hw2.0/hw/mbox_host_reg.h>

/* prototypes */
static A_STATUS Driver_SetAddressWindowRegister(A_VOID *pCxt, A_UINT32 RegisterAddr, A_UINT32 Address);

/* Compile the 4BYTE version of the window register setup routine,
 * This mitigates host interconnect issues with non-4byte aligned bus requests, some
 * interconnects use bus adapters that impose strict limitations.
 * Since diag window access is not intended for performance critical operations, the 4byte mode should
 * be satisfactory even though it generates 4X the bus activity. */

#ifdef USE_4BYTE_REGISTER_ACCESS

    /* set the window address register (using 4-byte register access ). */
/*****************************************************************************/
/*  Driver_SetAddressWindowRegister - Utility function to set the window 
 *	 register. This is used for diagnostic reads and writes.
 *      A_VOID *pCxt - the driver context. 
 *		A_UINT32 RegisterAddr - The window register address.
 *		A_UINT32 Address - the target address. 
 *****************************************************************************/
static A_STATUS 
Driver_SetAddressWindowRegister(A_VOID *pCxt, A_UINT32 RegisterAddr, A_UINT32 Address)
{
    A_STATUS status;
    A_UINT8 addrValue[4];
    A_INT32 i;
	A_NETBUF_DECLARE req;
	A_VOID *pReq = (A_VOID*)&req;
	
        /* write bytes 1,2,3 of the register to set the upper address bytes, the LSB is written
         * last to initiate the access cycle */
	Address = A_CPU2LE32(Address);
	
    for (i = 1; i <= 3; i++) {
            /* fill the buffer with the address byte value we want to hit 4 times*/
        addrValue[0] = ((A_UINT8 *)&Address)[i];
        addrValue[1] = addrValue[0];
        addrValue[2] = addrValue[0];
        addrValue[3] = addrValue[0];

		A_NETBUF_CONFIGURE(pReq, (A_VOID*)addrValue, 0, 4, 4);	
		ATH_SET_PIO_EXTERNAL_WRITE_OPERATION(pReq, RegisterAddr+i, A_FALSE, 4);
		
		if(A_OK != (status = Hcd_DoPioExternalAccess(pCxt, pReq))){
			break;
		}
    }

    if (status != A_OK) {        
        return status;
    }

	A_NETBUF_CONFIGURE(pReq, (A_VOID*)&Address, 0, 4, 4);	
	ATH_SET_PIO_EXTERNAL_WRITE_OPERATION(pReq, RegisterAddr, A_TRUE, 4);
	status = Hcd_DoPioExternalAccess(pCxt, pReq);
	
	return status;
}


#else
    
/*****************************************************************************/
/*  Driver_SetAddressWindowRegister - Utility function to set the window 
 *	 register. This is used for diagnostic reads and writes.
 *      A_VOID *pCxt - the driver context. 
 *		A_UINT32 RegisterAddr - The window register address.
 *		A_UINT32 Address - the target address. 
 *****************************************************************************/
static A_STATUS 
Driver_SetAddressWindowRegister(A_VOID *pCxt, A_UINT32 RegisterAddr, A_UINT32 Address)
{
    A_STATUS status;
	A_NETBUF_DECLARE req;
	A_VOID *pReq = (A_VOID*)&req;
	
	Address = A_CPU2LE32(Address);
	
	do{
		A_NETBUF_CONFIGURE(pReq, (((A_UCHAR *)(&Address))+1), (sizeof(A_UINT32)-1));	
		ATH_SET_PIO_EXTERNAL_WRITE_OPERATION(pReq, RegisterAddr+1, A_TRUE, (sizeof(A_UINT32)-1));
		
		if(A_OK != (status = Hcd_DoPioExternalAccess(pCxt, pReq))){
			break;
		}
		
		A_NETBUF_CONFIGURE(pReq, ((A_UCHAR *)(&Address)), sizeof(A_UINT8));	
		ATH_SET_PIO_EXTERNAL_WRITE_OPERATION(pReq, RegisterAddr, A_TRUE, sizeof(A_UINT8));
		
		if(A_OK != (status = Hcd_DoPioExternalAccess(pCxt, pReq))){
			break;
		}
	}while(0);

	return status;
}

#endif

/*****************************************************************************/
/*  Driver_ReadRegDiag - Reads four bytes of data from the specified chip
 *	 device address.
 *      A_VOID *pCxt - the driver context. 
 *		A_UINT32 address - the device chip address to start the read.
 *		A_UCHAR *data - the start of data destination buffer.
 *****************************************************************************/
A_STATUS
Driver_ReadRegDiag(A_VOID *pCxt, A_UINT32 *address, A_UINT32 *data)
{
    A_STATUS status;
	A_NETBUF_DECLARE req;
	A_VOID *pReq = (A_VOID*)&req;
	
	A_NETBUF_CONFIGURE(pReq, data, 0, sizeof(A_UINT32), sizeof(A_UINT32));
	
	do{
        /* set window register to start read cycle */
    	if(A_OK != (status = Driver_SetAddressWindowRegister(pCxt,
                                             WINDOW_READ_ADDR_ADDRESS,
                                             *address))){
        	break;
        }

		ATH_SET_PIO_EXTERNAL_READ_OPERATION(pReq, WINDOW_DATA_ADDRESS, A_TRUE, sizeof(A_UINT32));
		
		if(A_OK != (status = Hcd_DoPioExternalAccess(pCxt, pReq))){
			break;
		}
	}while(0);    

    return status;
}

/*****************************************************************************/
/*  Driver_WriteRegDiag - Writes four bytes of data to the specified chip
 *	 device address.
 *      A_VOID *pCxt - the driver context. 
 *		A_UINT32 address - the device chip address to start the write.
 *		A_UCHAR *data - the start of data source buffer.
 *****************************************************************************/
A_STATUS
Driver_WriteRegDiag(A_VOID *pCxt, A_UINT32 *address, A_UINT32 *data)
{
    A_STATUS status;

	A_NETBUF_DECLARE req;
	A_VOID *pReq = (A_VOID*)&req;
	
	A_NETBUF_CONFIGURE(pReq, data, 0, sizeof(A_UINT32), sizeof(A_UINT32));
	
	
	ATH_SET_PIO_EXTERNAL_WRITE_OPERATION(pReq, WINDOW_DATA_ADDRESS, A_TRUE, sizeof(A_UINT32));
	
	do{
		if(A_OK != (status = Hcd_DoPioExternalAccess(pCxt, pReq))){
			break;
		}
        /* set window register, which starts the write cycle */
	    if(A_OK != (status = Driver_SetAddressWindowRegister(pCxt,
	                                           WINDOW_WRITE_ADDR_ADDRESS,
	                                           *address))){
	    	break;                                       
	    }
	}while(0);
	
	return status;                                           
}

/*****************************************************************************/
/*  Driver_ReadDataDiag - Reads a buffer of data starting from address. Length
 * 	 of data is specified by length. Data is read from a contiguous chip
 *	 address memory/register space.
 *      A_VOID *pCxt - the driver context. 
 *		A_UINT32 address - the device chip address to start the read.
 *		A_UCHAR *data - the start of data destination buffer.
 *		A_UINT32 length - the length of data in bytes.
 *****************************************************************************/
A_STATUS
Driver_ReadDataDiag(A_VOID *pCxt, A_UINT32 address,
                    A_UCHAR *data, A_UINT32 length)
{
    A_UINT32 count;
    A_STATUS status = A_OK;

    for (count = 0; count < length; count += 4, address += 4) {
        if ((status = Driver_ReadRegDiag(pCxt, &address,
                                         (A_UINT32 *)&data[count])) != A_OK)
        {
            break;
        }
    }

    return status;
}

/*****************************************************************************/
/*  Driver_WriteDataDiag - Writes a buffer of data starting at address. Length
 * 	 of data is specified by length. Data is written to a contiguous chip
 *	 address memory/register space.
 *      A_VOID *pCxt - the driver context. 
 *		A_UINT32 address - the device chip address to start the write.
 *		A_UCHAR *data - the start of data source buffer.
 *		A_UINT32 length - the length of data in bytes.
 *****************************************************************************/
A_STATUS
Driver_WriteDataDiag(A_VOID *pCxt, A_UINT32 address,
                    A_UCHAR *data, A_UINT32 length)
{
    A_UINT32 count;
    A_STATUS status = A_OK;

    for (count = 0; count < length; count += 4, address += 4) {
        if ((status = Driver_WriteRegDiag(pCxt, &address,
                                         (A_UINT32 *)&data[count])) != A_OK)
        {
            break;
        }
    }

    return status;
}


#define REG_DUMP_COUNT_AR4100   60
#define REG_DUMP_COUNT_AR4002   60
#define REGISTER_DUMP_LEN_MAX   60

/*****************************************************************************/
/*  Driver_DumpAssertInfo - Collects assert information from chip and writes
 *	 it to stdout.
 *      A_VOID *pCxt - the driver context. 
 *		A_UINT32 *pData - buffer to store UINT32 results
 *		A_UINT16 *pLength - IN/OUT param to store length of buffer for IN and
 *		 length of contents for OUT.     
 *****************************************************************************/
A_STATUS
Driver_DumpAssertInfo(A_VOID *pCxt, A_UINT32 *pData, A_UINT16 *pLength)
{
	A_DRIVER_CONTEXT *pDCxt = GET_DRIVER_COMMON(pCxt);
    A_UINT32 address;
    A_UINT32 regDumpArea = 0;
    A_STATUS status = A_ERROR;   
    A_UINT32 regDumpCount = 0;
    
		
    do {
		if(*pLength < REGISTER_DUMP_LEN_MAX){
			break;
		}
		/* clear it */
		*pLength = 0;
            /* the reg dump pointer is copied to the host interest area */
        address = HOST_INTEREST_ITEM_ADDRESS(hi_failure_state);
        address = TARG_VTOP(address);

        if (pDCxt->targetType == TARGET_TYPE_AR4100 || 
        	pDCxt->targetType == TARGET_TYPE_AR400X) {
            regDumpCount = REG_DUMP_COUNT_AR4100;
        } else {
            A_ASSERT(0); /* should never happen */
#if DRIVER_CONFIG_DISABLE_ASSERT            
            break;      
#endif            
        }

            /* read RAM location through diagnostic window */
        if(A_OK != (status = Driver_ReadRegDiag(pCxt, &address, &regDumpArea))){
        	A_ASSERT(0); /* should never happen */
#if DRIVER_CONFIG_DISABLE_ASSERT            
            break;      
#endif          
        }

		regDumpArea = A_LE2CPU32(regDumpArea);

        if (regDumpArea == 0) {
                /* no reg dump */
            break;
        }

        regDumpArea = TARG_VTOP(regDumpArea);

            /* fetch register dump data */
        if(A_OK != (status = Driver_ReadDataDiag(pCxt,
                                     regDumpArea,
                                     (A_UCHAR *)&pData[0],
                                     regDumpCount * (sizeof(A_UINT32))))){
        	A_ASSERT(0); /* should never happen */
#if DRIVER_CONFIG_DISABLE_ASSERT            
            break;      
#endif                                      
        }      
        
        *pLength = regDumpCount;              
    } while (0);


	return status;
}