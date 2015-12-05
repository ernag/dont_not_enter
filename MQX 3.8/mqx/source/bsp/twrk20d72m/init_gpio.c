/**HEADER********************************************************************
*
* Copyright (c) 2008 Freescale Semiconductor;
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
* $FileName: init_gpio.c$
* $Version : 3.8.0.3$
* $Date    : Mar-10-2012$
*
* Comments:
*
*   This file contains board-specific pin initialization functions.
*
*END************************************************************************/

#include <mqx.h>
#include <bsp.h>

#define ALT2    0x2

/*FUNCTION*-------------------------------------------------------------------
*
* Function Name    : _bsp_serial_io_init
* Returned Value   : MQX_OK for success, -1 for failure
* Comments         :
*    This function performs BSP-specific initialization related to serial
*
*END*----------------------------------------------------------------------*/

_mqx_int _bsp_serial_io_init
(
    /* [IN] Serial device number */
    uint_8 dev_num,

    /* [IN] Required functionality */
    uint_8 flags
)
{
    SIM_MemMapPtr   sim = SIM_BASE_PTR;
    PORT_MemMapPtr  pctl;

    /* Setup GPIO for SCIx */
    switch (dev_num)
    {
        case 0:

           pctl = (PORT_MemMapPtr)PORTD_BASE_PTR;
            if (flags & IO_PERIPHERAL_PIN_MUX_ENABLE)
            {
                /* PTB6 as RX function (Alt.3) + drive strength */
                pctl->PCR[6] = 0 | PORT_PCR_MUX(3) | PORT_PCR_DSE_MASK;
                /* PTB7 as TX function (Alt.3) + drive strength */
                pctl->PCR[7] = 0 | PORT_PCR_MUX(3) | PORT_PCR_DSE_MASK;
            }
            if (flags & IO_PERIPHERAL_PIN_MUX_DISABLE)
            {
                /* PTB6 default */
                pctl->PCR[6] = 0;
                /* PTB7 default */
                pctl->PCR[7] = 0;
            }
            if (flags & IO_PERIPHERAL_CLOCK_ENABLE)
            {
                /* start SGI clock */
                sim->SCGC4 |= SIM_SCGC4_UART0_MASK;
            }
            if (flags & IO_PERIPHERAL_CLOCK_DISABLE)
            {
                /* stop SGI clock */
                sim->SCGC4 &= (~ SIM_SCGC4_UART0_MASK);
            }
            break;
        case 1:
			pctl = (PORT_MemMapPtr)PORTE_BASE_PTR;
            if (flags & IO_PERIPHERAL_PIN_MUX_ENABLE)
            {
                /* PTE1 as RX function (Alt.3) + drive strength */
                pctl->PCR[1] = 0 | PORT_PCR_MUX(3) | PORT_PCR_DSE_MASK;
                /* PTE0 as TX function (Alt.3) + drive strength */
                pctl->PCR[0] = 0 | PORT_PCR_MUX(3) | PORT_PCR_DSE_MASK;
            }
            if (flags & IO_PERIPHERAL_PIN_MUX_DISABLE)
            {
                /* PTE1 default */
                pctl->PCR[1] = 0;
                /* PTE0 default */
                pctl->PCR[0] = 0;
            }
            if (flags & IO_PERIPHERAL_CLOCK_ENABLE)
            {
                /* start SGI clock */
                sim->SCGC4 |= SIM_SCGC4_UART1_MASK;
            }
            if (flags & IO_PERIPHERAL_CLOCK_DISABLE)
            {
                /* start SGI clock */
                sim->SCGC4 &= (~ SIM_SCGC4_UART1_MASK);
            }
            break;
        case 2:
            pctl = (PORT_MemMapPtr)PORTD_BASE_PTR;
            if (flags & IO_PERIPHERAL_PIN_MUX_ENABLE)
            {
                /* PTD2 as RX function (Alt.3) + drive strength */
                pctl->PCR[2] = 0 | PORT_PCR_MUX(3) | PORT_PCR_DSE_MASK;
                /* PTD3 as TX function (Alt.3) + drive strength */
                pctl->PCR[3] = 0 | PORT_PCR_MUX(3) | PORT_PCR_DSE_MASK;
            }
            if (flags & IO_PERIPHERAL_PIN_MUX_DISABLE)
            {
                /* PTD2 default */
                pctl->PCR[2] = 0;
                /* PTD3 default */
                pctl->PCR[3] = 0;
            }
            if (flags & IO_PERIPHERAL_CLOCK_ENABLE)
            {
                /* start SGI clock */
                sim->SCGC4 |= SIM_SCGC4_UART2_MASK;
            }
            if (flags & IO_PERIPHERAL_CLOCK_DISABLE)
            {
                /* stop SGI clock */
                sim->SCGC4 &= (~ SIM_SCGC4_UART2_MASK);
            }
            break;

        case 3:
            pctl = (PORT_MemMapPtr)PORTC_BASE_PTR;
            if (flags & IO_PERIPHERAL_PIN_MUX_ENABLE)
            {
                /* PTC16 as RX function (Alt.3) + drive strength */
                pctl->PCR[16] = 0 | PORT_PCR_MUX(3) | PORT_PCR_DSE_MASK;
                /* PTC17 as TX function (Alt.3) + drive strength */
                pctl->PCR[17] = 0 | PORT_PCR_MUX(3) | PORT_PCR_DSE_MASK;
#if BSPCFG_ENABLE_TTYD_HW_SIGNALS
                /* PTC18 as RTS function (Alt.3) + drive strength */
                pctl->PCR[18] = 0 | PORT_PCR_MUX(3) | PORT_PCR_DSE_MASK;
                /* PTC19 as CTS function (Alt.3) + drive strength */
                pctl->PCR[19] = 0 | PORT_PCR_MUX(3) | PORT_PCR_DSE_MASK;
#endif
            }
            if (flags & IO_PERIPHERAL_PIN_MUX_DISABLE)
            {
                /* PTC16 default */
                pctl->PCR[16] = 0;
                /* PTC17 default */
                pctl->PCR[17] = 0;
#if BSPCFG_ENABLE_TTYD_HW_SIGNALS
                /* PTC18 default */
                pctl->PCR[18] = 0;
                /* PTC19 default */
                pctl->PCR[19] = 0;
#endif
            }
            if (flags & IO_PERIPHERAL_CLOCK_ENABLE)
            {
                /* start SGI clock */
                sim->SCGC4 |= SIM_SCGC4_UART3_MASK;
            }
            if (flags & IO_PERIPHERAL_CLOCK_DISABLE)
            {
                /* stop SGI clock */
                sim->SCGC4 &= (~ SIM_SCGC4_UART3_MASK);
            }
            break;

        case 4:
            pctl = (PORT_MemMapPtr)PORTC_BASE_PTR;
            if (flags & IO_PERIPHERAL_PIN_MUX_ENABLE)
            {
                /* PTC14 as RX function (Alt.3) + drive strength */
                pctl->PCR[14] = 0 | PORT_PCR_MUX(3) | PORT_PCR_DSE_MASK;
                /* PTC15 as TX function (Alt.3) + drive strength */
                pctl->PCR[15] = 0 | PORT_PCR_MUX(3) | PORT_PCR_DSE_MASK;
            }
            if (flags & IO_PERIPHERAL_PIN_MUX_DISABLE)
            {
                /* PTC14 default */
                pctl->PCR[14] = 0;
                /* PTC15 default */
                pctl->PCR[15] = 0;
            }
            if (flags & IO_PERIPHERAL_CLOCK_ENABLE)
            {
                /* starting SGI clock */
                sim->SCGC1 |= SIM_SCGC1_UART4_MASK;
            }
            if (flags & IO_PERIPHERAL_CLOCK_DISABLE)
            {
                /* starting SGI clock */
                sim->SCGC1 &= (~ SIM_SCGC1_UART4_MASK);
            }
            break;
         default:
            return -1;
  }

  return 0;
}

/*FUNCTION*-------------------------------------------------------------------
*
* Function Name    : _bsp_adc_io_init
* Returned Value   : 0 for success, -1 for failure
* Comments         :
*    This function performs BSP-specific initialization related to ADC
*
*END*----------------------------------------------------------------------*/

_mqx_int _bsp_adc_io_init
(
     /* [IN] number of ADC device on which to perform hardware initialization */
    _mqx_uint adc_num
)
{
    SIM_MemMapPtr sim_ptr = SIM_BASE_PTR;

    /* Enable ADC clocks */
    if (adc_num == 0)
        sim_ptr->SCGC6 |= SIM_SCGC6_ADC0_MASK;
    else if (adc_num == 1)
        sim_ptr->SCGC3 |= SIM_SCGC3_ADC1_MASK;
    else
        return IO_ERROR;

    return IO_OK;
}

/*FUNCTION*-------------------------------------------------------------------
*
* Function Name    : _bsp_adc_channel_io_init
* Returned Value   : 0 for success, -1 for failure
* Comments         :
*    This function performs BSP-specific initialization related to ADC channel
*
*END*----------------------------------------------------------------------*/

_mqx_int _bsp_adc_channel_io_init
(
     /* [IN] number of channel on which to perform hardware initialization */
    uint_16   source
)
{
    uint_8 ch = ADC_GET_CHANNEL(source);
    uint_8 mux = ADC_GET_MUXSEL(source);
    uint_8 gpio_port;
    PORT_MemMapPtr pctl;

    #define ADC_SIG_PORTA   (0x01 << 5)
    #define ADC_SIG_PORTB   (0x02 << 5)
    #define ADC_SIG_PORTC   (0x03 << 5)
    #define ADC_SIG_PORTD   (0x04 << 5)
    #define ADC_SIG_PORTE   (0x05 << 5)
    #define ADC_SIG_NA      (0x00) /* signal not available */
    #define ADC_SIG_NC      (0x01) /* signal not configurable */

    /* Conversion table for ADC0x inputs, where x is 0 to 23, mux is defaultly "B" */
    const static uint_8 adc0_conv_table[] = {
        ADC_SIG_NC, //0 leave as default
        ADC_SIG_NC, //1 leave as default
        ADC_SIG_NC, //2 leave as default
        ADC_SIG_NC, //3 leave as default
        ADC_SIG_PORTC | 2, //4b
        ADC_SIG_PORTD | 1, //5b
        ADC_SIG_PORTD | 5, //6b
        ADC_SIG_PORTD | 6, //7b
        ADC_SIG_PORTB | 0, //8
        ADC_SIG_PORTB | 1, //9
        ADC_SIG_PORTA | 7, //10
        ADC_SIG_PORTA | 8, //11
        ADC_SIG_PORTB | 2, //12
        ADC_SIG_PORTB | 3, //13
        ADC_SIG_PORTC | 0, //14
        ADC_SIG_PORTC | 1, //15
        ADC_SIG_NC, //16
        ADC_SIG_PORTE | 24, //17 conflict in K20 Sub-Family Reference Manual, Rev. 5, table 10.3.1 and 3.7.1.3.1
        ADC_SIG_PORTE | 25, //18 conflict in K20 Sub-Family Reference Manual, Rev. 5, table 10.3.1 and 3.7.1.3.1
        ADC_SIG_NC, //19 ADC0_DM0, leave as default
        ADC_SIG_NC, //20 ADC0_DM1, leave as default
        ADC_SIG_NA, //21 18 conflict in K20 Sub-Family Reference Manual, Rev. 5, ADC0_SE16 table (table 10.3.1) and ADC0_SE18 (table 10.3.1 and 3.7.1.3.1)
        ADC_SIG_NA, //22 not implemented
        ADC_SIG_NC, //23 DAC0, leave as default
        ADC_SIG_NC, //24 not implemented
        ADC_SIG_NC, //25 not implemented
        //below: use ADC_SIG_NC (leave as default)
    };

    /* Conversion table for ADC1x, where x is 0 to 23, mux is defaultly "B" (or nothing) */
    const static uint_8 adc1_conv_tableB[] = {
        ADC_SIG_NC, //0 leave as default
        ADC_SIG_NC, //1 leave as default
        ADC_SIG_NC, //2 leave as default
        ADC_SIG_NC, //3 leave as default
        ADC_SIG_PORTC | 8, //4b
        ADC_SIG_PORTC | 9, //5b
        ADC_SIG_PORTC | 10, //6b
        ADC_SIG_PORTC | 11, //7b
        ADC_SIG_PORTB | 0, //8
        ADC_SIG_PORTB | 1, //9
        ADC_SIG_PORTB | 4, //10
        ADC_SIG_PORTB | 5, //11
        ADC_SIG_PORTB | 6, //12
        ADC_SIG_PORTB | 7, //13
        ADC_SIG_PORTB | 10, //14
        ADC_SIG_PORTB | 11, //15
        ADC_SIG_NC, 
        ADC_SIG_PORTA | 17, //17
        ADC_SIG_NC, //18 VREF, leave as default
        ADC_SIG_NC, //19 ADC1_DM0, leave as default
        ADC_SIG_NC, //20 ADC1_DM1, leave as default
        ADC_SIG_NA, //21 not implemented
        ADC_SIG_NA, //22 not implemented
        ADC_SIG_NC, //23 not implemented
        ADC_SIG_NC, //24 not implemented
        ADC_SIG_NC, //25 not implemented
        //below: use ADC_SIG_NC (leave as default)
    };

    /* Conversion table for ADC1x, where x is 4 to 7, mux is "A" */
    const static uint_8 adc1_conv_tableA[] = {
        ADC_SIG_PORTE | 0, //4a
        ADC_SIG_PORTE | 1, //5a
        ADC_SIG_PORTE | 2, //6a
        ADC_SIG_PORTE | 3, //7a
    };

    if (ADC_GET_DIFF(source) && ch > 3)
        return IO_ERROR; /* signal not available */

    if (ADC_GET_DIFF(source) == 0 && ch == 2)
        return IO_ERROR; /* channel 2 (PGA) can be used only as a diff pair */

    if (ch < 26) {
        if (ADC_GET_MODULE(source) == ADC_SOURCE_MODULE(1)) {
            /* Get result for module 0 */
           gpio_port = adc0_conv_table[ch];
        }
        else {
            if ((ADC_GET_MUXSEL(source) == ADC_SOURCE_MUXSEL_B) || (ADC_GET_MUXSEL(source) == ADC_SOURCE_MUXSEL_X))
            /* Get result for module 1, if user wants "B" channel or any channel */
                gpio_port = adc1_conv_tableB[ch];
            else {
                /* Get result for module 1, if user wants "A" channel or any other */
                if (ch < 4 || ch > 7)
                    gpio_port = ADC_SIG_NA;
                else
                    gpio_port = adc1_conv_tableA[ch - 4];
            }
        }
    }
    else
        gpio_port = ADC_SIG_NC;

    if (gpio_port == ADC_SIG_NA)
        return IO_ERROR; /* signal not available */
    if (gpio_port == ADC_SIG_NC)
        return IO_OK; /* no need to configure signal */
    switch (gpio_port >> 5) {
        case 1: /* PORTA */
            pctl = (PORT_MemMapPtr) PORTA_BASE_PTR;
            break;
        case 2: /* PORTB */
            pctl = (PORT_MemMapPtr) PORTB_BASE_PTR;
            break;
        case 3: /* PORTC */
            pctl = (PORT_MemMapPtr) PORTC_BASE_PTR;
            break;
        case 4: /* PORTD */
            pctl = (PORT_MemMapPtr) PORTD_BASE_PTR;
            break;
        case 5: /* PORTE */
            pctl = (PORT_MemMapPtr) PORTE_BASE_PTR;
            break;
        /* There is no possibility to get other port from table */
    }
    pctl->PCR[gpio_port & 0x1F] &= ~PORT_PCR_MUX_MASK; /* set pin's multiplexer to analog */

    return IO_OK;
}

/*FUNCTION*-------------------------------------------------------------------
*
* Function Name    : _bsp_rtc_io_init
* Returned Value   : MQX_OK or -1
* Comments         :
*    This function performs BSP-specific initialization related to RTC
*
*END*----------------------------------------------------------------------*/

_mqx_int _bsp_rtc_io_init
(
    void
)
{

#if PE_LDD_VERSION
    /* Check if peripheral is not used by Processor Expert RTC_LDD component */
    if (PE_PeripheralUsed((uint_32)RTC_BASE_PTR) == TRUE)    {
        /* IO Device used by PE Component*/
        return IO_ERROR;
    }
#endif

    /* Enable the clock gate to the RTC module. */
    SIM_SCGC6 |= SIM_SCGC6_RTC_MASK;

    return MQX_OK;
}

/*FUNCTION*-------------------------------------------------------------------
*
* Function Name    : _bsp_dspi_io_init
* Returned Value   : MQX_OK or -1
* Comments         :
*    This function performs BSP-specific initialization related to DSPI
*
*END*----------------------------------------------------------------------*/

_mqx_int _bsp_dspi_io_init
(
    uint_32 dev_num
)
{
    SIM_MemMapPtr   sim = SIM_BASE_PTR;
    PORT_MemMapPtr  pctl;

    switch (dev_num)
    {
        case 0:
            /* Configure GPIOD for SPI0 peripheral function */
            pctl = (PORT_MemMapPtr)PORTD_BASE_PTR;

            pctl->PCR[0] = PORT_PCR_MUX(2) | PORT_PCR_DSE_MASK;     /* DSPI0.PCS0   */
            pctl->PCR[1] = PORT_PCR_MUX(2) | PORT_PCR_DSE_MASK;     /* DSPI0.SCK    */
            pctl->PCR[2] = PORT_PCR_MUX(2) | PORT_PCR_DSE_MASK;     /* DSPI0.SOUT   */
            pctl->PCR[3] = PORT_PCR_MUX(2) | PORT_PCR_DSE_MASK;     /* DSPI0.SIN    */

            /* Enable clock gate to SPI0 module */
            sim->SCGC6 |= SIM_SCGC6_SPI0_MASK;
            break;

        case 1:
            /* Configure GPIOE for DSPI1 peripheral function     */
        	
        	/*
        	// Ernie commented this out, since we want to use pin-mux for our SPI accelerometer!
            pctl = (PORT_MemMapPtr)PORTE_BASE_PTR;

            pctl->PCR[1] = PORT_PCR_MUX(ALT2) | PORT_PCR_DSE_MASK;     // DSPI1.SOUT
            pctl->PCR[2] = PORT_PCR_MUX(ALT2) | PORT_PCR_DSE_MASK;     // DSPI1.SCK
            pctl->PCR[3] = PORT_PCR_MUX(ALT2) | PORT_PCR_DSE_MASK;     // DSPI1.SIN
            pctl->PCR[4] = PORT_PCR_MUX(ALT2) | PORT_PCR_DSE_MASK;     // DSPI1.PCSO
            
        	*/
            
            pctl = (PORT_MemMapPtr)PORTB_BASE_PTR;

            pctl->PCR[16] = PORT_PCR_MUX(ALT2) | PORT_PCR_DSE_MASK;     // DSPI1.SOUT
            pctl->PCR[11] = PORT_PCR_MUX(ALT2) | PORT_PCR_DSE_MASK;     // DSPI1.SCK
            pctl->PCR[17] = PORT_PCR_MUX(ALT2) | PORT_PCR_DSE_MASK;     // DSPI1.SIN
            pctl->PCR[10] = PORT_PCR_MUX(ALT2) | PORT_PCR_DSE_MASK;     // DSPI1.PCSO			
            
            // TODO - what about the interrupts ?

            /* Enable clock gate to DSPI1 module */
            sim->SCGC6 |= SIM_SCGC6_SPI1_MASK; // Ernie commented out.  We don't want to gate the clock!
            break;

        default:
            /* do nothing if bad dev_num was selected */
            return -1;
    }

    return MQX_OK;

}

/*FUNCTION*-------------------------------------------------------------------
*
* Function Name    : _bsp_flexcan_io_init
* Returned Value   : 0 or 1 for success, -1 for failure
* Comments         :
*    This function performs BSP-specific initialization related to FLEXCAN
*
*END*----------------------------------------------------------------------*/
_mqx_int _bsp_flexcan_io_init
(
    uint_8 dev_num
)
{
    volatile CAN_MemMapPtr  can_reg_ptr;
    SIM_MemMapPtr   sim = SIM_BASE_PTR;
    PORT_MemMapPtr  pctl;

    OSC_CR |= OSC_CR_ERCLKEN_MASK;


    switch (dev_num)
    {
        case 0:
            /* Configure GPIO for FlexCAN 0 peripheral function */
            pctl = (PORT_MemMapPtr)PORTA_BASE_PTR;
            pctl->PCR[12] = (PORT_PCR_MUX(2)|PORT_PCR_DSE_MASK);    /* CAN0_TX.A12  */
            pctl->PCR[13] = (PORT_PCR_MUX(2)|PORT_PCR_DSE_MASK);    /* CAN0_RX.A13  */

            /* Alternate GPIO pins for FlexCAN 0 peripheral function */
            //pctl = (PORT_MemMapPtr)PORTB_BASE_PTR;
            //pctl->PCR[18] = (PORT_PCR_MUX(2)|PORT_PCR_DSE_MASK);    /* CAN0_TX.B18  */
            //pctl->PCR[19] = (PORT_PCR_MUX(2)|PORT_PCR_DSE_MASK);    /* CAN0_RX.B19  */

            /* Enable clock gate to FlexCAN 0 module */
            sim->SCGC6 |= SIM_SCGC6_FLEXCAN0_MASK;

            /* Select the bus clock as CAN engine clock source */
            can_reg_ptr = _bsp_get_flexcan_base_address (0);
            can_reg_ptr->CTRL1 |= CAN_CTRL1_CLKSRC_MASK;

            break;
        default:
            return -1;
    }
    return 0;
}

/*FUNCTION*-------------------------------------------------------------------
*
* Function Name    : _bsp_i2c_io_init
* Returned Value   : MQX_OK or -1
* Comments         :
*    This function performs BSP-specific initialization related to I2C
*
*END*----------------------------------------------------------------------*/

_mqx_int _bsp_i2c_io_init
(
    uint_32 dev_num
)
{

    PORT_MemMapPtr  pctl;
    SIM_MemMapPtr   sim = SIM_BASE_PTR;

    switch (dev_num)
    {
        case 0:
            /* configure GPIO for I2C0 peripheral function */
            pctl = (PORT_MemMapPtr)PORTB_BASE_PTR;

            pctl->PCR[2] = PORT_PCR_MUX(ALT2) | PORT_PCR_ODE_MASK;
            pctl->PCR[3] = PORT_PCR_MUX(ALT2) | PORT_PCR_ODE_MASK;

             /* Enable clock gate to I2C0 module */
            sim->SCGC4 |= SIM_SCGC4_I2C0_MASK;
            break;

        case 1:
            /* Configure GPIO for I2C1 peripheral function */
            pctl = (PORT_MemMapPtr)PORTE_BASE_PTR;

            pctl->PCR[1] = PORT_PCR_MUX(ALT2) | PORT_PCR_ODE_MASK;
            pctl->PCR[0] = PORT_PCR_MUX(ALT2) | PORT_PCR_ODE_MASK;

             /* Enable clock gate to I2C1 module */
            sim->SCGC4 |= SIM_SCGC4_I2C1_MASK;
            break;

        default:
            /* Do nothing if bad dev_num was selected */
            return -1;
    }

    return MQX_OK;
}

/*FUNCTION*-------------------------------------------------------------------
*
* Function Name    : _bsp_usb_io_init
* Returned Value   : MQX_OK or -1
* Comments         :
*    This function performs BSP-specific I/O initialization related to USB
*
*END*----------------------------------------------------------------------*/

_mqx_int _bsp_usb_io_init
(
    void
)
{
#if PE_LDD_VERSION
    /* USB clock is configured using CPU component */

    /* Check if peripheral is not used by Processor Expert USB_LDD component */
     if (PE_PeripheralUsed((uint_32)USB0_BASE_PTR) == TRUE) {
         /* IO Device used by PE Component*/
         return IO_ERROR;
     }
#endif
    /* Configure USB to be clocked from PLL */
    SIM_SOPT2_REG(SIM_BASE_PTR) |= SIM_SOPT2_USBSRC_MASK | SIM_SOPT2_PLLFLLSEL_MASK;

    /* Enable USB-OTG IP clocking */
    SIM_SCGC4_REG(SIM_BASE_PTR) |= SIM_SCGC4_USBOTG_MASK;

    /* USB D+ and USB D- are standalone not multiplexed one-purpose pins */
    /* VREFIN for device is standalone not multiplexed one-purpose pin */

#if BSP_USB_TWR_SER2
    /* TWR-SER2 board has 2 connectors: on channel A, there is Micro-USB connector,
    ** which is not routed to TWRK20 board. On channel B, there is standard
    ** A-type host connector routed to the USB0 peripheral on TWRK20. To enable
    ** power to this connector, GPIO PTA14 must be set as GPIO output
    */
    PORT_PCR_REG(PORTA_BASE_PTR, 14) = PORT_PCR_MUX(0x01) | PORT_PCR_PE_MASK;
    GPIO_PDDR_REG(PTB_BASE_PTR) |= 0x00000100; // PB8 as output
    GPIO_PDOR_REG(PTB_BASE_PTR) |= 0x00000100; // PB8 in high level
#endif

    return MQX_OK;
}

/*FUNCTION*-------------------------------------------------------------------
*
* Function Name    : _bsp_gpio_io_init
* Returned Value   : MQX_OK or -1
* Comments         :
*    This function performs BSP-specific initialization related to GPIO
*
*END*----------------------------------------------------------------------*/

_mqx_int _bsp_gpio_io_init
(
    void
)
{
    /* Enable clock gating to all ports */
    SIM_SCGC5 |=   SIM_SCGC5_PORTA_MASK \
                 | SIM_SCGC5_PORTB_MASK \
                 | SIM_SCGC5_PORTC_MASK \
                 | SIM_SCGC5_PORTD_MASK \
                 | SIM_SCGC5_PORTE_MASK;

    return MQX_OK;
}

/*FUNCTION*-------------------------------------------------------------------
*
* Function Name    : _bsp_serial_rts_init
* Returned Value   : MQX_OK or -1
* Comments         :
*    This function performs BSP-specific initialization related to GPIO
*
*END*----------------------------------------------------------------------*/

_mqx_int _bsp_serial_rts_init
(
    uint_32 device_index
)
{
   PORT_MemMapPtr           pctl;

   /* set pin to RTS functionality */
   switch( device_index )
   {
      case 3:
         pctl = (PORT_MemMapPtr)PORTC_BASE_PTR;
         pctl->PCR[18] = 0 | PORT_PCR_MUX(3);
         break;
      default:
         /* not used on this board */
         break;
   }
   return MQX_OK;
}


/*FUNCTION*-------------------------------------------------------------------
*
* Function Name    : _bsp_ftfx_io_init
* Returned Value   : 0 for success, -1 for failure
* Comments         :
*    This function performs BSP-specific initialization related to LCD
*
*END*----------------------------------------------------------------------*/
_mqx_int _bsp_ftfx_io_init
(
    _mqx_uint device_index
)
{
    SIM_MemMapPtr sim = SIM_BASE_PTR;

    if (device_index > 0)
        return IO_ERROR;

    /* Clock the controller */
    sim->SCGC6 |= SIM_SCGC6_FTFL_MASK;

    return MQX_OK;
}

/* EOF */
