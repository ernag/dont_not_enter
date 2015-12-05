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
* $FileName: dcd_demo.c$
* $Version : 3.8.3.1$
* $Date    : Oct-19-2011$
*
* Comments:
*
*   This file contains the source for the hello example program.
*
*END************************************************************************/

#include <mqx.h>
#include <bsp.h> 
#include <fio.h>

#if ! BSPCFG_ENABLE_IO_SUBSYSTEM
#error This application requires BSPCFG_ENABLE_IO_SUBSYSTEM defined non-zero in user_config.h. Please recompile BSP with this option.
#endif


#ifndef BSP_DEFAULT_IO_CHANNEL_DEFINED
#error This application requires BSP_DEFAULT_IO_CHANNEL to be not NULL. Please set corresponding BSPCFG_ENABLE_TTYx to non-zero in user_config.h and recompile BSP with this option.
#endif


#if (BSP_KWIKSTIK_K40X256)
    #define  VBUS_DETECT   10000
    #define MY_ADC "adc0:" /* must be #1 as the inputs are wired to ADC 1 */
    #define MY_TRIGGER ADC_PDB_TRIGGER
    #if !BSPCFG_ENABLE_ADC0
        #error This application requires BSPCFG_ENABLE_ADC0 defined non-zero in user_config.h. Please recompile BSP with this option.
    #endif /* BSPCFG_ENABLE_ADCx */
/* Logical channel #1 init struct */
const ADC_INIT_CHANNEL_STRUCT adc_channel_param = 
{
#if defined(BSP_KWIKSTIK_K40X256) 
   /* physical ADC channel */
    ADC0_SOURCE_AD20,
#else
    BSP_ADC_CH_POT, /* physical ADC channel */
#endif
    ADC_CHANNEL_MEASURE_LOOP | ADC_CHANNEL_START_TRIGGERED, /* runs continuously after IOCTL trigger */
    10,             /* number of samples in one run sequence */
    0,              /* time offset from trigger point in us */
    300000,         /* period in us (= 0.3 sec) */
    0x10000,        /* scale range of result (not used now) */
    10,             /* circular buffer size (sample count) */
    MY_TRIGGER,     /* logical trigger ID that starts this ADC channel */

};

/* ADC device init struct */
const ADC_INIT_STRUCT adc_init = {
    ADC_RESOLUTION_DEFAULT,     /* resolution */
};
#endif /*#endif defined(BSP_KWIKSTIK_K40X256)*/

#ifdef BSP_TWR_K53N512
  #define CHECK_VBUS   0x10
  #define PORT_VBUS    GPIOE_PDIR
#endif
#ifdef BSP_TWR_MCF51JF
  #define CHECK_VBUS   0x20
  #define PORT_VBUS    PTD_D
#endif
/* Task IDs */
#define DCD_TASK 5

extern void dcd_task(uint_32);
extern void IO_IOCTL_TEST();
MQX_FILE_PTR f, f_ch1;

const TASK_TEMPLATE_STRUCT  MQX_template_list[] = 
{ 
   /* Task Index,   Function,   Stack,  Priority, Name,     Attributes,          Param, Time Slice */
    { DCD_TASK,   dcd_task, 1500,   5,        "usbdcd",  MQX_AUTO_START_TASK, 0,     0 },
    { 0 }
};

/*TASK*-----------------------------------------------------
* 
* Task Name    : hello_task
* Comments     :
*    This task prints " Hello World "
*
*END*-----------------------------------------------------*/
MQX_FILE_PTR                 fd;

void Vbus_config()
{
#if defined BSP_TWR_K53N512  
   // Configure PortB as GPIO
   SIM_SCGC5 |= SIM_SCGC5_PORTB_MASK;
   PORTB_PCR0=(0|PORT_PCR_MUX(1));
   PORTB_PCR1=(0|PORT_PCR_MUX(1));
   PORTB_PCR2=(0|PORT_PCR_MUX(1));
   PORTB_PCR3=(0|PORT_PCR_MUX(1));
   PORTB_PCR4=(0|PORT_PCR_MUX(1));
   PORTB_PCR5=(0|PORT_PCR_MUX(1));
   PORTB_PCR6=(0|PORT_PCR_MUX(1));
   PORTB_PCR7=(0|PORT_PCR_MUX(1));
   PORTB_PCR8=(0|PORT_PCR_MUX(1));
   PORTB_PCR9=(0|PORT_PCR_MUX(1));
   PORTB_PCR10=(0|PORT_PCR_MUX(1));
   PORTB_PCR11=(0|PORT_PCR_MUX(1));
   PORTB_PCR12=(0|PORT_PCR_MUX(1));
   PORTB_PCR13=(0|PORT_PCR_MUX(1));
   PORTB_PCR14=(0|PORT_PCR_MUX(1));
   PORTB_PCR15=(0|PORT_PCR_MUX(1));
   GPIOB_PDDR = 0x00; 
   
#elif defined BSP_TWR_MCF51JF
    MXC_MemMapPtr   MXC = MXC_BASE_PTR;
    SIM_MemMapPtr   sim = SIM_BASE_PTR;
   /* Configure PortD as GPIO */
    sim->SCGC6 |= SIM_SCGC6_PORTD_MASK;
    MXC->PTDPF2 &= ~(MXC_PTDPF2_D5_MASK);
    MXC->PTDPF2 |= MXC_PTDPF2_D5(1);
    PTD_DD &= ~0x20;
    PCTLD_PUE |= 0x20;
    PCTLD_PUS |= 0x20;
#endif
   
}
#ifdef BSP_KWIKSTIK_K40X256
void Adc_Vbus()
{   
    printf("Opening ADC device ...");
    f = fopen(MY_ADC, (const char*)&adc_init);
    if(f != NULL)
    {    
        printf("done\n");
    }
    else
    {    
        printf("failed\n");
        _task_block();
    }


    printf("Opening ADC channel ...");
    f_ch1 = fopen(MY_ADC "first", (const char*)&adc_channel_param);
    if(f_ch1 != NULL)
    {    
        printf("done, prepared to start by trigger\n");
    }
     else
     {    
        printf("failed\n");
        _task_block();
     }
     ioctl(f, ADC_IOCTL_FIRE_TRIGGER, (pointer) MY_TRIGGER);
}
#endif

void dcd_task
   (
      uint_32 initial_data
   )
{
    uint_32                      param;
    uint_32                      Vbus_flag=0x00;
    char ch;
    ADC_RESULT_STRUCT data;
    printf("\n USB DCD testing..... %d \n ");
    fd = fopen ("usbdcd:", NULL);
    if (fd == NULL) 
    {
        printf ("ERROR opening the USB DCD driver!\n");
        _task_block();
    }
#ifdef BSP_KWIKSTIK_K40X256
    Adc_Vbus(); 
#endif   
#if defined BSP_TWR_K53N512 || defined BSP_TWR_MCF51JF    
    Vbus_config(); 
    if (!(PORT_VBUS & CHECK_VBUS))
    {
        printf("Vbus is disable ........\r\n");
        Vbus_flag = 0x00;
    }
#endif

    printf("\n ---------------------------------------------------");
    printf("\n Press 1 to select testing IOCTL for USB DCD driver");
    printf("\n Press 2 to start detecting USB charger");   
    printf("\n---------------------------------------------------");
    printf("\n Your choice: ");
    do{      
        ch = fgetc(stdin);
    }while ((ch != '1') && (ch != '2'));
    if (ch == '1')
    {
    IO_IOCTL_TEST();
   }

   printf("\n Start detecting USB charger");
   for (;;)
   { 
 
#ifdef BSP_KWIKSTIK_K40X256
    read(f_ch1, &data, sizeof(data));
    // printf("%d\n",data.result);
    if (( data.result  > VBUS_DETECT)&& (!Vbus_flag) )
    {
        /* Starting DCD - IO_IOCTL_USB_DCD_START */
        printf("\n Vbus is enable ........\n");
        if (USB_DCD_OK == ioctl (fd, IO_IOCTL_USB_DCD_START, &param))
        {
            printf ("Starting detecting....  \r\n");
        }
        else
        {
            printf ("ERROR\n");
        }
         Vbus_flag = 0x01;
      }
      
      if ((data.result  < VBUS_DETECT)&& (Vbus_flag))
      {
         printf("Vbus is disable ........\r\n");
         Vbus_flag = 0x00;
      }
#elif defined BSP_TWR_K53N512 || defined BSP_TWR_MCF51JF
      if ( (PORT_VBUS & CHECK_VBUS) && (!Vbus_flag) )
            {
               /* Starting DCD - IO_IOCTL_USB_DCD_START */
               printf("\n Vbus is enable ........\n");
               if (USB_DCD_OK == ioctl (fd, IO_IOCTL_USB_DCD_START, &param))
               {
                       printf ("Starting detecting....  \r\n");
               } else {
                       printf ("ERROR\n");
               }
               Vbus_flag = 0x01;
            }
            
            if ((!(PORT_VBUS & CHECK_VBUS)) && (Vbus_flag))
            {
               printf("Vbus is disable ........\r\n");
               Vbus_flag = 0x00;
            }
            
#endif  
      if (USB_DCD_OK == ioctl (fd, IO_IOCTL_USB_DCD_GET_STATE, &param))
      {
 //        printf("\n IO_IOCTL_USB_DCD_GET_STATE - OK with param = 0x%X", param);
         if (param == USB_DCD_SEQ_COMPLETE )
         {
            if (USB_DCD_OK == ioctl (fd, IO_IOCTL_USB_DCD_GET_CHARGER_TYPE, &param))
            {
               if ((param & 0x0F) == STANDARD_HOST )
               {
                  printf("Connected to a Standard Host   \r\n");
               }
               else if ((param & 0x0F) == CHARGING_HOST)
               {
                  printf("Connected to a Charging Host   \r\n");
               }
               else if ((param & 0x0F) == DEDICATED_CHARGER)
               {
                  printf("Connected to a dedicated Charger  \r\n");
               }
               else // if (param & 0xF0)
                  printf("------------- DCD Sequence occurs error (param = 0x%X) ....  \r\n",param);
            }
            else
            {
               printf("IO_IOCTL_USB_DCD_GET_CHARGER_TYPE - ERROR ... \r\n");
            }
            if (USB_DCD_OK == ioctl (fd, IO_IOCTL_USB_DCD_RESET, &param))
            {
               printf("USB-DCD module : Reset \r\n");
            }
            else
            {
               printf("Cannot reset USB-DCD module \r\n");
            }
         }
         /* ioctl command test*/      
          _time_delay(20);
      }
      else
      {
         printf("Can not get state .... \r\n");
         /* ioctl command test*/      
          _time_delay(20);
      }     

   }
   printf("Closing channels...");
   fclose(f_ch1);

   _task_block();

}

void IO_IOCTL_TEST
(
)
{
     uint_32                      param;
     /* Testing ioctl*/
     printf("\n IOCTL command for USB DCD testing ...... \r\n");
     param = USBDCD_TSEQ_INIT_RESET_VALUE;
     if (USB_DCD_OK == ioctl (fd, IO_IOCTL_USB_DCD_SET_TSEQ_INIT, &param))
     {
        printf ("IO_IOCTL_USB_DCD_SET_TSEQ_INIT with value = %x - OK \n", param );
     } else {
        printf ("ERROR\n");
     }
         
     if (USB_DCD_OK == ioctl (fd, IO_IOCTL_USB_DCD_GET_TSEQ_INIT, &param))
     {
        printf ("TSEQ_INIT value = %x\n", param );
     } else {
        printf ("ERROR\n");
     }
     
     param = USB_DCD_TDCD_DBNC_RESET_VALUE;
     
     if (USB_DCD_OK == ioctl (fd, IO_IOCTL_USB_DCD_SET_TDCD_DBNC, &param))
     {
         printf ("IO_IOCTL_USB_DCD_SET_TDCD_DBNC with TDCD_DBNC value = %x\n", param );
     } else {
         printf ("ERROR\n");
     }
     
     if (USB_DCD_OK == ioctl (fd, IO_IOCTL_USB_DCD_GET_TDCD_DBNC, &param))
     {
        printf ("IO_IOCTL_USB_DCD_GET_TDCD_DBNC - with TDCD_DBNC value = %x\n", param );
     } else {
        printf ("ERROR\n");
     }
     
     param = USB_DCD_TVDPSRC_ON_RESET_VALUE;
     
     if (USB_DCD_OK == ioctl (fd, IO_IOCTL_USB_DCD_SET_TVDPSRC_ON, &param))
     {
         printf ("IO_IOCTL_USB_DCD_SET_TVDPSRC_ON with TVDPSRC_ON value = %x\n", param );
     } else {
         printf ("ERROR\n");
     }
     
     if (USB_DCD_OK == ioctl (fd, IO_IOCTL_USB_DCD_GET_TVDPSRC_ON, &param))
     {
        printf ("IO_IOCTL_USB_DCD_GET_TVDPSRC_ON - with TVDPSRC_ON value = %x\n", param );
     } else {
        printf ("ERROR\n");
     }
     
     param = USB_DCD_TVDPSRC_CON_RESET_VALUE;
     if (USB_DCD_OK == ioctl (fd, IO_IOCTL_USB_DCD_SET_TVDPSRC_CON, &param))
      {
          printf ("IO_IOCTL_USB_DCD_SET_TVDPSRC_CON with TVDPSRC_CON value = %x\n", param );
      } else {
          printf ("ERROR\n");
      }
      
      if (USB_DCD_OK == ioctl (fd, IO_IOCTL_USB_DCD_GET_TVDPSRC_CON, &param))
      {
         printf ("IO_IOCTL_USB_DCD_GET_TVDPSRC_CON - with TVDPSRC_CON value = %x\n", param );
      } else {
         printf ("ERROR\n");
      }     
     param = USB_DCD_CHECK_DM_RESET_VALUE;
     if (USB_DCD_OK == ioctl (fd, IO_IOCTL_USB_DCD_SET_CHECK_DM, &param))
     {
         printf ("IO_IOCTL_USB_DCD_SET_CHECK_DM with CHECK_DM value = %x\n", param );
     } else {
         printf ("ERROR\n");
     }
      
     if (USB_DCD_OK == ioctl (fd, IO_IOCTL_USB_DCD_GET_CHECK_DM, &param))
     {
        printf ("IO_IOCTL_USB_DCD_GET_CHECK_DM - with CHECK_DM value = %x\n", param );
     } else {
        printf ("ERROR\n");
     }     
         
     param = 48000;
     if (USB_DCD_OK == ioctl (fd, IO_IOCTL_USB_DCD_SET_CLOCK_SPEED, &param))
     {
        printf ("IO_IOCTL_USB_DCD_SET_CLOCK_SPEED with clock = %d [Khz] - OK\n", param );
     } else {
        printf ("ERROR\n");
     }
     
     if (USB_DCD_OK == ioctl (fd, IO_IOCTL_USB_DCD_GET_CLOCK_SPEED, &param))
     {
        printf ("Clock speed = %d [Khz]\n", param );
     } else {
        printf ("ERROR\n");
     }
     
     if (USB_DCD_OK == ioctl (fd, IO_IOCTL_USB_DCD_GET_STATUS, &param))
     {
        printf ("IO_IOCTL_USB_DCD_GET_STATUS - with STATUS REG value = %x\n", param );
     } else {
        printf ("ERROR\n");
     }
}


/* EOF */
