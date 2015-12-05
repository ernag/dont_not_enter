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
#ifndef _A_OSAPI_H_
#define _A_OSAPI_H_

/* PORT_NOTE: Include any System header files here to resolve any types used
 *  in A_CUSTOM_DRIVER_CONTEXT */ 
#include <string.h>
#include <mqx.h>
#include <pcb.h>
//#include <fio.h>
#include <mutex.h>
#include <lwevent.h>
#include "a_config.h"
#include "a_types.h"
#include <athdefs.h>
#include <cust_netbuf.h>


#define MEM_TYPE_ATHEROS_PERSIST_RX_PCB 0x2001

/* there exist within the common code a few places that make use of the 
 * inline option.  This macro can be used to properly declare inline for the compiler 
 * or it can be used to disable inlining be leaving the macro blank.
 */
#ifndef INLINE
#define INLINE                  inline
#endif
/* PREPACK and/or POSTPACK are used with compilers that require each data structure
 * that requires single byte alignment be declared individually. these macros typically
 * would be used in place of athstartpack.h and athendpack.h which would be used with 
 * compilers that allow this feature to be invoked through a one-time pre-processor command.
 */
#define PREPACK
#define POSTPACK                

#ifndef min
#define min(a,b) ((a) < (b))? (a) : (b)
#endif

#ifndef max
#define max(a,b) ((a) > (b))? (a) : (b)
#endif


/* unaligned little endian access */
#define A_LE_READ_2(p)                                              \
        ((A_UINT16)(                                                \
        (((A_UINT8 *)(p))[0]) | (((A_UINT8 *)(p))[1] <<  8)))

#define A_LE_READ_4(p)                                              \
        ((A_UINT32)(                                                \
        (((A_UINT8 *)(p))[0]      ) | (((A_UINT8 *)(p))[1] <<  8) | \
        (((A_UINT8 *)(p))[2] << 16) | (((A_UINT8 *)(p))[3] << 24)))
/*
 * Endianes macros - Used to achieve proper translation to/from wifi chipset endianess.
 * these macros should be directed to OS macros or functions that serve this purpose. the
 * naming convention is the direction of translation (BE2CPU == BigEndian to native CPU) 
 * appended with the bit-size (BE2CPU16 == 16 bit operation BE2CPU32 == 32 bit operation
 */   


#if DRIVER_CONFIG_ENDIANNESS == A_LITTLE_ENDIAN
#define A_BE2CPU16(x)   ((A_UINT16)(((x)<<8)&0xff00) | (A_UINT16)(((x)>>8)&0x00ff))
#define A_BE2CPU32(x)   ((((x)<<24)&0xff000000) | (((x)&0x0000ff00)<<8) | (((x)&0x00ff0000)>>8) | (((x)>>24)&0x000000ff))
#define A_LE2CPU16(x)   (x)
#define A_LE2CPU32(x)   (x)
#define A_CPU2BE16(x)   ((A_UINT16)(((x)<<8)&0xff00) | (A_UINT16)(((x)>>8)&0x00ff))
#define A_CPU2BE32(x)   ((((x)<<24)&0xff000000) | (((x)&0x0000ff00)<<8) | (((x)&0x00ff0000)>>8) | (((x)>>24)&0x000000ff))
#define A_CPU2LE32(x)   (x)
#define A_CPU2LE16(x)   (x)
#define A_ENDIAN A_LITTLE_ENDIAN
#elif DRIVER_CONFIG_ENDIANNESS == A_BIG_ENDIAN
#define A_BE2CPU16(x)   (x)
#define A_BE2CPU32(x)   (x)
#define A_LE2CPU16(x)   ((A_UINT16)(((x)<<8)&0xff00) | (A_UINT16)(((x)>>8)&0x00ff))
#define A_LE2CPU32(x)   ((((x)<<24)&0xff000000) | (((x)&0x0000ff00)<<8) | (((x)&0x00ff0000)>>8) | (((x)>>24)&0x000000ff))
#define A_CPU2BE16(x)   (x)
#define A_CPU2BE32(x)   (x)
#define A_CPU2LE32(x)   ((((x)<<24)&0xff000000) | (((x)&0x0000ff00)<<8) | (((x)&0x00ff0000)>>8) | (((x)>>24)&0x000000ff))
#define A_CPU2LE16(x)   ((A_UINT16)(((x)<<8)&0xff00) | (A_UINT16)(((x)>>8)&0x00ff))
#define A_ENDIAN A_BIG_ENDIAN
#else
#error DRIVER_CONFIG_ENDIANNESS is not properly defined!!!
#endif

/* A_MEM -- macros that should be mapped to OS/STDLIB equivalent functions */
#define A_MEMCPY(dst, src, len)         memcpy((A_UINT8 *)(dst), (src), (len))
#define A_MEMZERO(addr, len)            memset((addr), 0, (len))
#define A_MEMCMP(addr1, addr2, len)     memcmp((addr1), (addr2), (len))

extern A_UINT32 g_totAlloc;
extern A_UINT32 g_poolid;

#define DRIVER_STATE_INVALID  0
#define DRIVER_STATE_INIT     1
#define DRIVER_STATE_RUN      2
#define DRIVER_STATE_SHUTDOWN 3
extern A_UINT8 g_driverState;

inline A_UINT32 _get_size(A_VOID* addr)
{
	A_UINT32 *ptr = addr;
	ptr -= 3;
	
	if(g_poolid == 0xffffffff){
		g_poolid = ptr[1];
	}	
	
	if(ptr[1] == g_poolid){
		return ptr[0];
	}else{
		printf("STUCK\n");
		while(1){};
	}
	
}

A_VOID* a_malloc(A_INT32 size, A_UINT8 id);
A_VOID a_free(A_VOID* addr, A_UINT8 id);
/* Definitions used for ID param in calls to A_MALLOC and A_FREE */
//		NAME					VALUE 	DESCRIPTION
#define MALLOC_ID_CONTEXT 		1	// persistent allocation for the life of Driver
#define MALLOC_ID_NETBUFF 		2	// buffer used to perform TX/RX SPI transaction
#define MALLOC_ID_NETBUFF_OBJ 	3	// NETBUF object
#define MALLOC_ID_TEMPORARY     4   // other temporary allocation
/* Macros used for heap memory allocation and free */
#define A_MALLOCINIT() 
#define A_MALLOC(size, id)           	a_malloc((size), (id))
#define A_FREE(addr, id)                a_free((addr), (id))

#define A_PRINTF(args...)               printf(args)

/* Mutual Exclusion */
typedef MUTEX_STRUCT                    A_MUTEX_T;
/* use default mutex attributes */
#define A_MUTEX_INIT(mutex)             (((MQX_EOK != _mutex_init((mutex), NULL)))? A_ERROR:A_OK)                      
    
#define A_MUTEX_LOCK(mutex)             (((MQX_EOK != _mutex_lock((mutex))))? A_ERROR:A_OK)
#define A_MUTEX_UNLOCK(mutex)           (((MQX_EOK != _mutex_unlock((mutex))))? A_ERROR:A_OK)
#define A_IS_MUTEX_VALID(mutex)         TRUE  /* okay to return true, since A_MUTEX_DELETE does nothing */
#define A_MUTEX_DELETE(mutex)           (((MQX_EOK != _mutex_destroy((mutex))))? A_ERROR:A_OK)

/*
 * A_MDELAY - used to delay specified number of milliseconds.
 */
#define A_MDELAY(msecs)                 _time_delay((msecs))


#if DRIVER_CONFIG_DISABLE_ASSERT
#define A_ASSERT(expr)
#else /* DRIVER_CONFIG_DISABLE_ASSERT */
extern A_VOID *p_Global_Cxt;    
extern void assert_func(A_UINT32, char const *pFileName);
#define A_ASSERT(expr) if(!(expr)) do{ ((A_CUSTOM_DRIVER_CONTEXT*)p_Global_Cxt)->line = __LINE__; assert_func(((A_CUSTOM_DRIVER_CONTEXT*)p_Global_Cxt)->line, __FILE__);}while(0)
#endif /* DRIVER_CONFIG_DISABLE_ASSERT */

/* A_UNUSED_ARGUMENT - used to solve compiler warnings where arguments to functions are not used
 * within the function. 
 */
#define A_UNUSED_ARGUMENT(arg) ((void)arg)

#include <cust_context.h>



#endif /* _OSAPI_MQX_H_ */
