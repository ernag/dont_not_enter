/**HEADER***********************************************************************
*
* Copyright (c) 2011 Freescale Semiconductor;
* All Rights Reserved
*
********************************************************************************
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
********************************************************************************
*
* $FileName: init_hw.c$
* $Version : 3.8.23.4$
* $Date    : Nov-30-2011$
*
* Comments:
*
*   This file contains flash boot code to initialize chip selects,
*   disable the watchdog timer and initialize the PLL.
*
*END***************************************************************************/

#include "mqx.h"
#include "bsp.h"
#include "bsp_prv.h"


/*FUNCTION*---------------------------------------------------------------------
*
* Function Name    : _bsp_watchdog_disable
* Returned Value   : void
* Comments         :
*   Disable watchdog timer
*
*END*-------------------------------------------------------------------------*/

void _bsp_watchdog_disable(void)
{
    WDOG_MemMapPtr reg = WDOG_BASE_PTR;

    /* unlock watchdog */
    reg->UNLOCK = 0xc520;
    reg->UNLOCK = 0xd928;

    /* disable watchdog, but allow updating. use LPO */
    reg->STCTRLH = ((reg->STCTRLH & ~(WDOG_STCTRLH_WDOGEN_MASK | WDOG_STCTRLH_CLKSRC_MASK)) | WDOG_STCTRLH_ALLOWUPDATE_MASK) ;
}

/*FUNCTION*---------------------------------------------------------------------
*
* Function Name    : init_hardware
* Returned Value   : void
* Comments         :
*   Initialize Kinetis device.
*
*END*-------------------------------------------------------------------------*/

void init_hardware(void)
{
/*
 * e2647: FMC: Cache aliasing is not supported on 512 KB and 384 KB
 * program flash only devices.
 * Disable the use of the cache for 512 KB and 384 KB program flash only devices.
 */
if (((SIM_SDID & SIM_SDID_REVID_MASK) >> SIM_SDID_REVID_SHIFT) == 0)
{
    FMC_PFB0CR &= ~(FMC_PFB0CR_B0DCE_MASK | FMC_PFB0CR_B0ICE_MASK | FMC_PFB0CR_B0SEBE_MASK);
    FMC_PFB1CR &= ~(FMC_PFB1CR_B1DCE_MASK | FMC_PFB1CR_B1ICE_MASK | FMC_PFB1CR_B1SEBE_MASK);
}
#if PE_LDD_VERSION
    /*  Watch Dog disabled by CPU bean (need to setup in CPU Inspector) */
    __pe_initialize_hardware();
#else
    _bsp_initialize_hardware();
#endif

    /* Enable pin clocks */
    _bsp_gpio_io_init ();
}
