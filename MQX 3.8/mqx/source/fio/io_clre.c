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
* $FileName: io_clre.c$
* $Version : 3.6.4.0$
* $Date    : Jun-4-2010$
*
* Comments:
*
*   Contains the function io_clearerr.
*
*END************************************************************************/

#include "mqx_inc.h"
#include "fio.h"
#include "fio_prv.h"
#include "io.h"
#include "io_prv.h"

/*FUNCTION*-------------------------------------------------------------------
* 
* Function Name    : _io_clearerr
* Returned Value   : none
* Comments         :
*    Clears the end of file and error indicators.
*
*END*----------------------------------------------------------------------*/

void _io_clearerr
   ( 
      /* [IN] the stream to perform the operation on */
      MQX_FILE_PTR file_ptr
   )
{ /* Body */

#if MQX_CHECK_ERRORS
   if (file_ptr == NULL) {
      return;
   } /* Endif */
#endif

   file_ptr->ERROR = 0;
   file_ptr->FLAGS &= ~IO_FLAG_AT_EOF;

} /* Endbody */

/* EOF */
