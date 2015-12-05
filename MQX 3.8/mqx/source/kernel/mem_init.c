/**HEADER********************************************************************
* 
* Copyright (c) 2008 Freescale Semiconductor;
* All Rights Reserved
*
* Copyright (c) 2004-2008 Embedded Access Inc.;
* All Rights Reserved
*
* Copyright (c) 1989-2008 ARC International;
* All Rights Reserved
*
*************************************************************************** 
*
* THIS SOFTWARE IS PROVIDED BY FREESCALE "AS IS" AND ANY EXPRESSED OR 
* IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES 
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  
* IN NO EVENT SHALL FREESCALE OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
* INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, 
* STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING 
* IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF 
* THE POSSIBILITY OF SUCH DAMAGE.
*
**************************************************************************
*
* $FileName: mem_init.c$
* $Version : 3.0.6.0$
* $Date    : Jul-14-2009$
*
* Comments:
*
*   This file contains the function that initializes the memory pool.
*
*END************************************************************************/

#define __MEMORY_MANAGER_COMPILE__
#include "mqx_inc.h"
#if MQX_USE_MEM
#include "mem_prv.h"

/*FUNCTION*-----------------------------------------------------
* 
* Function Name    : _mem_init_internal
* Returned Value   : _mqx_uint error_code
*       MQX_OK, INIT_KERNEL_MEMORY_TOO_SMALL
* Comments         :
*   This function initializes the memory storage pool.
* 
*END*---------------------------------------------------------*/

_mqx_uint _mem_init_internal
   (
      void
   )
{ /* Body */
   extern uchar __UNCACHED_DATA_START[], __UNCACHED_DATA_END[];

   KERNEL_DATA_STRUCT_PTR kernel_data;
   pointer start;
   _mqx_uint result;

   _GET_KERNEL_DATA(kernel_data);

   _QUEUE_INIT(&kernel_data->MEM_COMP.POOLS, 0);

   _lwsem_create((LWSEM_STRUCT_PTR)&kernel_data->MEM_COMP.SEM, 1);

   kernel_data->MEM_COMP.VALID = MEMPOOL_VALID;

   /*
   ** Move the MQX memory pool pointer past the end of kernel data.
   */
   start = (pointer)((uchar_ptr)kernel_data + sizeof(KERNEL_DATA_STRUCT));
   
   result = _mem_create_pool_internal(start, kernel_data->INIT.END_OF_KERNEL_MEMORY, (MEMPOOL_STRUCT_PTR)&kernel_data->KD_POOL);

#if MQX_USE_UNCACHED_MEM && PSP_HAS_DATA_CACHE
   if (result == MQX_OK)
      result = _mem_create_pool_internal(__UNCACHED_DATA_START, __UNCACHED_DATA_END, (MEMPOOL_STRUCT_PTR)&kernel_data->UNCACHED_POOL);
   
#endif // MQX_USE_UNCACHED_MEM && PSP_HAS_DATA_CACHE

   return result;

} /* Endbody */

#endif
/* EOF */
