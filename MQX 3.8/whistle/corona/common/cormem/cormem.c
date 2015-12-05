/*
 * cormem.c
 * 
 * CORona dynamic MEMory handling.
 *
 *  Created on: Oct 28, 2013
 *      Author: Ernie
 */

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include <mqx.h>
#include <bsp.h>
#include <stdarg.h>
#include "app_errors.h"
#include "cormem.h"
#include "wassert.h"

////////////////////////////////////////////////////////////////////////////////
// Code
////////////////////////////////////////////////////////////////////////////////

/*
 *   walloc()
 *   
 *   Standard Whistle Function for dynamic memory allocation in the Corona firmware system.
 *     - allocates system memory for size given.
 *     - initializes that memory to zero.
 *     - wassert(0)'s on NULL ptr exception.
 *     - logs error on NULL ptr exception.
 */
void *_walloc( _mem_size sz, char *file, int line )
{
    void *ptr = _lwmem_alloc_system_zero( sz );
    
#if CORMEM_DEBUG
    fprintf(stdout, "ALLOC(%d) %p %x %s:%d\n", sz, ptr, _task_get_id(), file, line);
#endif
    
    if( NULL == ptr )
    {
        uint_32 u32_LR = 0;  // holds the link register for more debugging help.
        
        __asm
        {
            mov u32_LR, LR
        }
        
        // TODO: untimely crumb here.
        wassert( ERR_OK == PROCESS_NINFO(ERR_MEM_ALLOC, "ALLOC tsk %x LR %x", _task_get_id(), u32_LR) );
    }
    
    return ptr;
}

/*
 *   wfree()
 *   
 *   Standard Whistle Function for freeing memory that was acquired by walloc().
 */
void  _wfree(void *ptr, char *file, int line)
{
    _mqx_uint ret;
    
#if CORMEM_DEBUG
    fprintf(stdout, "FREE %p %x %s:%d\n", ptr, _task_get_id(), file, line);
#endif
    
    wassert( NULL != ptr );
    
    ret = _lwmem_free(ptr);
    
    if( MQX_OK != ret )
    {
        uint_32 u32_LR = 0;  // holds the link register for more debugging help.
        
        __asm
        {
            mov u32_LR, LR
        }
        
        // TODO: untimely crumb here.
        wassert( ERR_OK == PROCESS_NINFO(ERR_MEM_FREE, "FREE ret %u tsk %x LR %x", ret, _task_get_id(), u32_LR) );
    }
}
