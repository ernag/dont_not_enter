/******************************************************************************
 *
 * Freescale Semiconductor Inc.
 * (c) Copyright 2004-2010 Freescale Semiconductor, Inc.
 * ALL RIGHTS RESERVED.
 *
 ******************************************************************************
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
 **************************************************************************//*!
 *
 * @file main_kinetis.c
 *
 * @author
 *
 * @version
 *
 * @date
 *
 * @brief   This software is the USB driver stack for S08 family
 *****************************************************************************/
#include "types.h"
#include "derivative.h" /* include peripheral declarations */
#include "user_config.h"
#include "RealTimerCounter.h"
#include "Wdt_kinetis.h"
#include "hidef.h"
#include "usb_dci_kinetis.h"
#include "usb_dciapi.h"
#include "virtual_com.h"



/*****************************************************************************
 * Local Functions Prototypes
 *****************************************************************************/
static void USB_Init(uint_8 controller_ID);

/*****************************************************************************
 * Global Functions
 *****************************************************************************/
/******************************************************************************
 * @name        main
 *
 * @brief       This routine is the starting point of the application
 *
 * @param       None
 *
 * @return      None
 *
 *****************************************************************************
 * This function initializes the system, enables the interrupts and calls the
 * application
 *****************************************************************************/
void main_k60_usb(void)
{
	/* Initialize the system */
    // SYS_Init();

    // USB_Init(MAX3353);

    /* Initialize the USB Application */
    usb_app_init();

    /* Enable USB Interrupts */
	NVICICPR2 = (1 << 9);	/* Clear any pending interrupts on USB */
	NVICISER2 = (1 << 9);	/* Enable interrupts from USB module */
return;
}


/* EOF */
