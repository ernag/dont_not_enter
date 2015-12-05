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
* $FileName: flash_mcf51ag.c$
* $Version : 3.8.3.1$
* $Date    : Dec-7-2011$
*
* Comments:
*
*   The file contains functions for internal flash read, write, erase 
*
*END************************************************************************/
#include "mqx.h"
#include "bsp.h"
#include "flashx.h"
#include "flashxprv.h"
#include "flash_mcf51xx.h"
#include "flash_mcf51xx_prv.h"
#include "flash_mcf51ag.h"

/*FUNCTION*-------------------------------------------------------------------
*
* Function Name    : _bsp_get_cfm_address
* Returned Value   : Address upon success
* Comments         :
*    This function returns the base register address of the CFM
*
*END*----------------------------------------------------------------------*/
pointer _bsp_get_cfm_address()
{
   VMCF51AG_STRUCT_PTR reg_ptr = _PSP_GET_MBAR(); 
   VMCF51XX_FTSR_STRUCT_PTR ftsr_ptr = (VMCF51XX_FTSR_STRUCT_PTR)&reg_ptr->FTSR;
   return (pointer)ftsr_ptr;
}

/*FUNCTION*-------------------------------------------------------------------
*
* Function Name    : _bsp_get_pmc_address
* Returned Value   : Address upon success
* Comments         :
*    This function returns the base register address of the PMC
*
*END*----------------------------------------------------------------------*/
pointer _bsp_get_pmc_address()
{
   VMCF51AG_STRUCT_PTR reg_ptr = _PSP_GET_MBAR(); 
   VMCF51AG_PMC_STRUCT_PTR pmc_ptr = (VMCF51AG_PMC_STRUCT_PTR)&reg_ptr->PMC;
   return (pointer)pmc_ptr;   
}

/* EOF */
