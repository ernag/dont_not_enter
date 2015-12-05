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
* $FileName: rtcsmsec.c$
* $Version : 3.6.5.0$
* $Date    : Dec-1-2010$
*
* Comments:
*
*   This file contains the interface to the RTOS
*   time services.
*
*END************************************************************************/

#include <rtcsrtos.h>


/*FUNCTION*-------------------------------------------------------------
*
* Function Name   : RTCS_time_get
* Returned Value  : milliseconds elapsed since boot
* Comments        : Called to get number of milliseconds since
*                   bootup.
*
*END*-----------------------------------------------------------------*/

uint_32 RTCS_time_get
   (
      void
   )
{ /* Body */

#if (RTCSCFG_PRECISE_TIME == 0) && defined (PSP_GET_ELAPSED_MILLISECONDS) && !defined (__ICCCF__)
   return PSP_GET_ELAPSED_MILLISECONDS();

#else
   TIME_STRUCT    time;

   _time_get_elapsed(&time);
   return (time.SECONDS * 1000 + time.MILLISECONDS);

#endif
} /* Endbody */


/* EOF */
