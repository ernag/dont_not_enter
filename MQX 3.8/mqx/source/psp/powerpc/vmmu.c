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
* $FileName: vmmu.c$
* $Version : 3.8.6.0$
* $Date    : Aug-1-2011$
*
* Comments:
*
*   This file contains utiltity functions for use with the virtual
*   MMU functions for the PowerPC.
*
*END************************************************************************/

#include "mqx_inc.h"

#if MQX_CPU == PSP_CPU_AMCC405
   #include "vmmu405.c"
#elif MQX_CPU == PSP_CPU_MPC850
   #include "vmmu850.c"
#elif MQX_CPU == PSP_CPU_MPC855
   #include "vmmu860.c"
#elif MQX_CPU == PSP_CPU_MPC860
   #include "vmmu860.c"
#elif MQX_CPU == PSP_CPU_MPC866
   #include "mmu860.c"
#elif MQX_CPU == PSP_CPU_MPC875
   #include "mmu860.c"
#elif MQX_CPU == PSP_CPU_MPC7400
   #include "vmmu7400.c"
#elif MQX_CPU == PSP_CPU_MPC8240
   #include "vmmu8240.c"
#elif MQX_CPU == PSP_CPU_MPC5200
   #include "vmmu5200.c"
#elif defined(PSP_G2_CORE)
#include "vmmu8260.c"
#elif defined(PSP_E300_CORE)
#include "vmmue300.c"
#elif defined(PSP_E200_CORE)
/* do nothing */
#else

#error "INCORRECT MQX_CPU SETTING"

#endif

/* EOF */
