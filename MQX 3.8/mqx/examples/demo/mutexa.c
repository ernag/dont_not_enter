/**HEADER********************************************************************
* 
* Copyright (c) 2008 Freescale Semiconductor;
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
* $FileName: mutexa.c$
* $Version : 3.8.4.0$
* $Date    : Sep-19-2011$
*
* Comments:
*
*   
*
*END************************************************************************/

#include <mqx.h>
#include <bsp.h>
#include <message.h>
#include <errno.h>
#include <mutex.h>
#include <sem.h>
#include <event.h>
#include <log.h>
#include "demo.h"

/*   Task Code -  MutexA     */


/*TASK---------------------------------------------------------------
*   
* Task Name   :  MutexA
* Comments    : 
* 
*END*--------------------------------------------------------------*/

void MutexA
   (
      uint_32   parameter
   )
{
   MUTEX_ATTR_STRUCT      mutex_init;

   /* initialize a mutex Mutex1 */
   if (_mutatr_init(&mutex_init) == MQX_EOK) {
      _mutatr_set_wait_protocol(&mutex_init,MUTEX_QUEUEING);
      _mutatr_set_sched_protocol(&mutex_init,MUTEX_NO_PRIO_INHERIT);
      _mutex_init(&Mutex1,&mutex_init);
   }

   /* 
   ** LOOP - 
   */
   while ( TRUE ) {
      if (_mutex_lock(&Mutex1) != MQX_EOK) { 
         /* an error occurred */
      }

      /* access shared resource */

      _time_delay_ticks(1);
      _mutex_unlock(&Mutex1);
   } /* endwhile */ 
} /*end of task*/

/* End of File */
