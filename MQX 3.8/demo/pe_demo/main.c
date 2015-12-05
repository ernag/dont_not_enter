/**HEADER***********************************************************************
*
* Copyright (c) 2008 Freescale Semiconductor;
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
* $FileName: main.c$
* $Version : 3.8.7.1$
* $Date    : Nov-25-2011$
*
* Comments:
*
*   This example demonstrates how to use Processor Expert to configure MQX BSP.
*   The application demo
*   1. It generates sine signal with given period on DACO pin.
*      The signal amplitude sweeps between minimal to maximal value.
*      It can be observed by scope on DAC0_OUT - A32 pin
*      on TWR-ELEV FUNCTIONAL or TWR-PROTO board.
*   2. The PWM signal is generated using FlexTimer FTM0 Channel 0
*      It can be observed by scope on PWM0 - A40 pin
*      on TWR-ELEV FUNCTIONAL or TWR-PROTO board.
*   3. It also toggles LEDs on board using GPIO driver
*      to signalize that application is running.
*   4. The ewm_task task is periodically refreshing watchdog
*      within a time-out period at which watchdog can be refreshed.
*
*END***************************************************************************/

#include <mqx.h>
#include <bsp.h>

#if ! BSPCFG_ENABLE_IO_SUBSYSTEM
#error This application requires BSPCFG_ENABLE_IO_SUBSYSTEM defined non-zero in user_config.h.
/* Please recompile BSP with this option. */
#endif

#ifndef BSP_DEFAULT_IO_CHANNEL_DEFINED
#error This application requires BSP_DEFAULT_IO_CHANNEL to be not NULL.
/*
 * Please set corresponding BSPCFG_ENABLE_TTYx to non-zero in user_config.h
 * and recompile BSP with this option.
 */
#endif

/* Task enumerations and prototypes */
enum {
    DAC_TASK  = 1,
    PWM_TASK,
    LED_TASK,
    EWM_TASK
} etask_type;

void dac_task(uint_32);
void pwm_task(uint_32);
void led_task(uint_32);
void ewm_task(uint_32);

/* Task template list */
const TASK_TEMPLATE_STRUCT  MQX_template_list[] =
{
   /* Task Index,   Function,   Stack,  Priority,   Name,        Attributes,          Param,   Time Slice */
    { DAC_TASK,     dac_task,    400,       8,      "DAC Task",  MQX_AUTO_START_TASK,   0,         0 },
    { PWM_TASK,     pwm_task,    400,       9,      "PWM Task",  MQX_AUTO_START_TASK,   0,         0 },
    { EWM_TASK,     ewm_task,    300,       10,     "EWM Task",  MQX_AUTO_START_TASK,   0,         0 },
    { LED_TASK,     led_task,    200,       11,     "LED Task",  MQX_AUTO_START_TASK,   0,         0 },
    { 0 }
};

/* Function prototypes */
uint_16_ptr GEN_CreateTable(int_16_ptr table_ptr, uint_16 table_size, int_16 peak_peak, int_16 offset);
_mqx_uint   GEN_DestroyTable(uint_16_ptr table_ptr);



/*******************************************************************************
 * Sine 12bit table with 256 samples.
 * The positive value is not saturated to 4095.
 * Saturation needs to be handled in driver
 ******************************************************************************/

const int_16 sine_table[] =
{
        0,   101,   201,   301,   401,   501,   601,   700,
      799,   897,   995,  1092,  1189,  1285,  1380,  1474,
     1567,  1660,  1751,  1842,  1931,  2019,  2106,  2191,
     2276,  2359,  2440,  2520,  2598,  2675,  2751,  2824,
     2896,  2967,  3035,  3102,  3166,  3229,  3290,  3349,
     3406,  3461,  3513,  3564,  3612,  3659,  3703,  3745,
     3784,  3822,  3857,  3889,  3920,  3948,  3973,  3996,
     4017,  4036,  4052,  4065,  4076,  4085,  4091,  4095,
     4096,  4095,  4091,  4085,  4076,  4065,  4052,  4036,
     4017,  3996,  3973,  3948,  3920,  3889,  3857,  3822,
     3784,  3745,  3703,  3659,  3612,  3564,  3513,  3461,
     3406,  3349,  3290,  3229,  3166,  3102,  3035,  2967,
     2896,  2824,  2751,  2675,  2598,  2520,  2440,  2359,
     2276,  2191,  2106,  2019,  1931,  1842,  1751,  1660,
     1567,  1474,  1380,  1285,  1189,  1092,   995,   897,
      799,   700,   601,   501,   401,   301,   201,   101,
        0,  -101,  -201,  -301,  -401,  -501,  -601,  -700,
     -799,  -897,  -995, -1092, -1189, -1285, -1380, -1474,
    -1567, -1660, -1751, -1842, -1931, -2019, -2106, -2191,
    -2276, -2359, -2440, -2520, -2598, -2675, -2751, -2824,
    -2896, -2967, -3035, -3102, -3166, -3229, -3290, -3349,
    -3406, -3461, -3513, -3564, -3612, -3659, -3703, -3745,
    -3784, -3822, -3857, -3889, -3920, -3948, -3973, -3996,
    -4017, -4036, -4052, -4065, -4076, -4085, -4091, -4095,
    -4096, -4095, -4091, -4085, -4076, -4065, -4052, -4036,
    -4017, -3996, -3973, -3948, -3920, -3889, -3857, -3822,
    -3784, -3745, -3703, -3659, -3612, -3564, -3513, -3461,
    -3406, -3349, -3290, -3229, -3166, -3102, -3035, -2967,
    -2896, -2824, -2751, -2675, -2598, -2520, -2440, -2359,
    -2276, -2191, -2106, -2019, -1931, -1842, -1751, -1660,
    -1567, -1474, -1380, -1285, -1189, -1092,  -995,  -897,
     -799,  -700,  -601,  -501,  -401,  -301,  -201,  -101,
};


/* DAC */
#define                      DA1_INTERNAL_BUFFER_SIZE    (16)
LDD_TDeviceData             *DA1_Device;
LDD_TUserData               *DA1_UserDataPtr;
LDD_TError                   DA1_Error;
LDD_DAC_TBufferWatermark     DA1_WatermarkValue = LDD_DAC_BUFFER_WATERMARK_L4;

/* DAC Trigger */
LDD_TDeviceData            *TRG_Device;
LDD_TError                  TRG_Error;
#define                     TRG_FREQUENCY                 (uint_32)(TRG_CNT_INP_FREQ_U_0/TRG_PERIOD_TICKS)

/* Signal generator parameters and variables */
#define                     GEN_NUMBER_OF_SAMPLES       (sizeof(sine_table)/sizeof(sine_table[0]))
#define                     GEN_PEAK2PEAK               (4096)
#define                     GEN_PEAK2PEAK_STEP          (64)
#define                     GEN_OFFSET                  (2048)
#define                     GEN_FREQUENCY               (uint_32)(TRG_FREQUENCY/GEN_NUMBER_OF_SAMPLES)


uint_16_ptr                 GEN_BufferNewPtr;
uint_16_ptr                 GEN_BufferCurrentPtr;
uint_16_ptr                 GEN_BufferPtr;
int_16                      GEN_SignalOffsetNew     = GEN_OFFSET;
int_16                      GEN_Peak2PeakNew        = GEN_PEAK2PEAK;
int_16                      GEN_Peak2PeakStep       = GEN_PEAK2PEAK_STEP;
int_16                      GEN_SignalOffsetCurrent;
int_16                      GEN_Peak2PeakCurrent;


#define TERMINAL_CURSOR_POSITION_MAX    (80)


/*TASK*-------------------------------------------------------------------------
*
* Task Name    : dac_task
* Comments     :
*
*
*END*-------------------------------------------------------------------------*/
void dac_task
   (
      uint_32 initial_data
   )
{
    int terminal_cursor_position = 1;
    /* Print example functional description on terminal */
    puts("\n");
    puts("\n********************************************************************************");
    puts("\n*");
    puts("\n*   This example demonstrates how to use Processor Expert to configure MQX BSP.");
    puts("\n*   The application demo :");
    puts("\n*   1. It generates sine signal with given period on DACO pin.");
    puts("\n*      The signal amplitude sweeps between minimal to maximal value.");
    puts("\n*      It can be observed by scope on DAC0_OUT - A32 pin");
    puts("\n*      on TWR-ELEV FUNCTIONAL or TWR-PROTO board.");
    puts("\n*   2. The PWM signal is generated using FlexTimer FTM0 Channel 0");
    puts("\n*      It can be observed by scope on PWM0 - A40 pin");
    puts("\n*      on TWR-ELEV FUNCTIONAL or TWR-PROTO board.");
    puts("\n*   3. It also toggles LEDs (D9-D11) on board using GPIO driver");
    puts("\n*      to signalize that application is running.");
    puts("\n*   4. The ewm_task task is periodically refreshing watchdog");
    puts("\n*      within a time-out period at which watchdog can be refreshed");
    puts("\n*");
    puts("\n********************************************************************************");
    puts("\n");

    /* Initialize DAC */
    puts("\nInitializing DAC device.....");
    DA1_UserDataPtr = NULL;
    DA1_Device     = DA1_Init(DA1_UserDataPtr);
    if (DA1_Device == NULL)  {
        puts("failed");
        _task_block();
    } else {
        puts("done");

        /* Create sinetable in RAM with specified amplitude and offset */
        GEN_Peak2PeakCurrent    = GEN_Peak2PeakNew;
        GEN_SignalOffsetCurrent = GEN_SignalOffsetNew;
        GEN_BufferCurrentPtr    = GEN_CreateTable(
                                        (const int_16_ptr)sine_table,
                                         sizeof(sine_table),
                                         GEN_Peak2PeakCurrent,
                                         GEN_SignalOffsetCurrent);

        /* Enable DA1 device */
        DA1_Error = DA1_Enable(DA1_Device);

        /* Set Buffer Watermark */
        DA1_Error = DA1_SetBufferWatermark(DA1_Device, DA1_WatermarkValue);

        /* Initialize DA1 internal buffer */
        DA1_Error = DA1_SetBuffer(DA1_Device, GEN_BufferCurrentPtr, DA1_INTERNAL_BUFFER_SIZE, 0);

        GEN_BufferPtr = GEN_BufferCurrentPtr + DA1_INTERNAL_BUFFER_SIZE;
    }


    /* Initialize DAC trigger */
    TRG_Device = TRG_Init(NULL);

    printf("\n - DAC trigger frequency      = %d Hz", TRG_FREQUENCY);
    printf("\n - Generated signal frequency = %d Hz", GEN_FREQUENCY);
    puts("\n");

    TRG_Error  = TRG_Enable(TRG_Device);

    for(;;)
    {
        /* Suspend task for 250 milliseconds */
        _time_delay(250);

        /* Sweep signal amplitude between minimal and maximal value */
        GEN_Peak2PeakNew +=  GEN_Peak2PeakStep;

        if (GEN_Peak2PeakNew < 0)   {
            GEN_Peak2PeakNew  = 0;
            GEN_Peak2PeakStep = -GEN_Peak2PeakStep;
            puts("\nReached minimal amplitude -> increasing amplitude\n");
            /* reset cursor position */
            terminal_cursor_position = 1;
        }
        else if (GEN_Peak2PeakNew > (GEN_PEAK2PEAK)) {
            GEN_Peak2PeakNew  = GEN_PEAK2PEAK;
            GEN_Peak2PeakStep = -GEN_Peak2PeakStep;
            puts("\nReached maximal amplitude -> decreasing amplitude\n");
            /* reset cursor position */
            terminal_cursor_position = 1;
        }

        /* Change signal amplitude if it differs from current one */
        if (GEN_Peak2PeakNew != GEN_Peak2PeakCurrent)
        {
            GEN_BufferNewPtr = GEN_CreateTable((const int_16_ptr)sine_table,
                                                sizeof(sine_table),
                                                GEN_Peak2PeakNew,
                                                GEN_SignalOffsetNew);

            GEN_DestroyTable(GEN_BufferCurrentPtr);
            GEN_BufferCurrentPtr    = GEN_BufferNewPtr;
            GEN_Peak2PeakCurrent    = GEN_Peak2PeakNew;
            GEN_SignalOffsetCurrent = GEN_SignalOffsetNew;
        }

        /* Print dot on console to see that application is running */
        if (terminal_cursor_position++ > TERMINAL_CURSOR_POSITION_MAX) {
            terminal_cursor_position = 1;
            puts("\n");
        }
        else {
            puts(".");
        }
    }
}

/*TASK*-------------------------------------------------------------------------
*
* Task Name    : pwm_task
* Comments     :
*
*END*-------------------------------------------------------------------------*/

static int                  pwm_task_count;
static LDD_TDeviceData     *PWM_DeviceData;
static LDD_TError           PWM_Error;
volatile PWM_TValueType     PWM_Value;
volatile PWM_TValueType     PWM_MaxValue;
volatile PWM_TValueType     PWM_Step;

void pwm_task
   (
      uint_32 initial_data
   )
{
    /* Initialize PWM device on FTM0 device */
    puts("\nInitializing PWM device.....");
    PWM_DeviceData = PWM_Init(NULL);
    if (PWM_DeviceData == NULL)  {
    puts("failed");
        _task_block();
    }
    else  {
        puts("done");
    }

    PWM_Value    = 0;
    PWM_Step     = PWM_PERIOD_TICKS / 32;
    PWM_MaxValue = PWM_PERIOD_TICKS;

  printf("\n - PWM frequency              = %d Hz", (PWM_CNT_INP_FREQ_U_0/PWM_PERIOD_TICKS));
    puts("\nThe PWM signal is generated on FTM0 Channel 0");
    puts("\n");
    /* Enable PWM device */
    PWM_Error = PWM_Enable(PWM_DeviceData);

    while(1)
    {
        /* Suspend task for 200ms */
        _time_delay(200);

        pwm_task_count++;
    }
}

/*
** ===================================================================
**     Event       :  PWM_OnCounterRestart (overloads function in module Events)
**
**     Component   :  PWM [TimerUnit_LDD]
**     Description :
**         Called if counter overflow/underflow or counter is
**         reinitialized by modulo or compare register matching.
**         OnCounterRestart event and Timer unit must be enabled. See
**         <SetEventMask> and <GetEventMask> methods. This event is
**         available only if a <Interrupt> is enabled.
**     Parameters  :
**         NAME            - DESCRIPTION
**       * UserDataPtr     - Pointer to the user or
**                           RTOS specific data. The pointer passed as
**                           the parameter of Init method.
**     Returns     : Nothing
** ===================================================================
*/

void PWM_OnCounterRestart(LDD_TUserData *UserDataPtr)
{
    /* Increment PWM duty-cycle from 0-100% */

    PWM_Value += PWM_Step;


    if (PWM_Value > PWM_MaxValue) PWM_Value = 0;

    /* Set new PWM channel value */
    PWM_Error = PWM_SetOffsetTicks(PWM_DeviceData, 0, PWM_Value);
}


/*TASK*-------------------------------------------------------------------------
 *
* Task Name    : ewm_task
* Comments     :
*
*END*-------------------------------------------------------------------------*/

static LDD_TDeviceData *EWM_DeviceData;
static vuint_32         ewm_task_count;
static LDD_TError       EWM_Error;

void ewm_task
   (
      uint_32 initial_data
   )
{

    /* Initialize EWM device */
    puts("\nInitializing EWM device.....");
    EWM_DeviceData = WDog1_Init(NULL);
    if (EWM_DeviceData == NULL)  {
    puts("failed");
        _task_block();
    }
    else  {
        puts("done");
    }

    puts("\nWatchdog runs in windowed mode of operation.");
  printf("\n - The Timeout period         = %d msec", (int)(WDog1_WATCHDOG_TIMEOUT * 1000));
  printf("\n - The Window period          = %d msec", (int)(WDog1_WATCHDOG_WINDOW * 1000));
    puts("\n");

    EWM_Error = WDog1_Enable(EWM_DeviceData);

    while(1)
    {
        ewm_task_count++;
        /*
         * Suspend ewm_task for 155 milliseconds
         * to clear watch dog within specified (150ms) window period.
         */
        _time_delay(155);
        EWM_Error = WDog1_Clear(EWM_DeviceData);
    }
}
/*TASK*-------------------------------------------------------------------------
*
* Task Name    : led_task
* Comments     :
*
*END*-------------------------------------------------------------------------*/

/* LED */
LDD_TDeviceData     *LED_DeviceData;
LDD_TError           LED_Error;

static int                 count = 1;
static int                 sign = 1;
static LDD_GPIO_TBitField  LED;

void led_task
   (
      uint_32 initial_data
   )
{
    /* Initialize GPIO  */
    puts("\nInitializing LED device.....");
    LED_DeviceData = GPIO1_Init(NULL);
    if (LED_DeviceData == NULL)    {
        puts("failed");
        _task_block();
    }
    else {
        puts("done");
    }

    puts("\n");

    while(1)
    {
        _time_delay(250);

        if (count == 4) {
            sign = -1;
            count = 3;
        } else {
            if (count == 0) {
                sign = 1;
                count = 1;
            }
        }

        switch (count) {
            case 1:
                LED = LED1;
                break;
            case 2:
                LED = LED2;
                break;
            case 3:
                LED = LED3;
                break;
            default:
                break;
        }

        count = count + sign;

        GPIO1_ToggleFieldBits(LED_DeviceData, LED, 1);
    }
}

/*
** ===================================================================
**     Event       :  TRG_OnCounterRestart (module Events)
**
**     Component   :  TRG [TimerUnit_LDD]
**     Description :
**         Called if counter overflow/underflow or counter is
**         reinitialized by modulo or compare register matching.
**         OnCounterRestart event and Timer unit must be enabled. See
**         <SetEventMask> and <GetEventMask> methods. This event is
**         available only if a <Interrupt> is enabled.
**     Parameters  :
**         NAME            - DESCRIPTION
**       * UserDataPtr     - Pointer to the user or
**                           RTOS specific data. The pointer passed as
**                           the parameter of Init method.
**     Returns     : Nothing
** ===================================================================
*/

static vuint_32 TRG_OnCounterRestartCount;

void TRG_OnCounterRestart
(
    LDD_TUserData *UserDataPtr
)
{
    TRG_OnCounterRestartCount++;

    /* Trigger data buffer read pointer */
    DA1_Error = DA1_ForceSwTrigger(DA1_Device);
}

/*
** ===================================================================
**     Event       :  DA1_OnBufferStart
**
**     Component   :  DA1 [DAC_LDD]
**     Description :
**         Buffer read pointer reached buffer bottom position. This
**         event can be enabled only if Interrupt service is enabled.
**         Overrides default function in module Events.
**     Parameters  :
**         NAME            - DESCRIPTION
**       * UserDataPtr     - Pointer to the user or
**                           RTOS specific data.
**     Returns     : Nothing
** ===================================================================
*/
static vuint_32 DA1_BufferStartCount;

void DA1_OnBufferStart(LDD_TUserData *UserDataPtr)
{
    DA1_BufferStartCount++;
}

/*
** ===================================================================
**     Event       :  DA1_OnBufferWatermark
**
**     Component   :  DA1 [DAC_LDD]
**     Description :
**         Buffer read pointer reached the watermark level. This event
**         can be enabled only if Interrupt service is enabled.
**         Overrides default function in module Events.
**     Parameters  :
**         NAME            - DESCRIPTION
**       * UserDataPtr     - Pointer to the user or
**                           RTOS specific data.
**     Returns     : Nothing
** ===================================================================
*/

static vuint_32 DA1_BufferWattermarkCount;

void DA1_OnBufferWatermark
(
    LDD_TUserData *UserDataPtr
)
{
    DA1_BufferWattermarkCount++;

    /* Copy data from buffer start to watermark */
    DA1_Error = DA1_SetBuffer(DA1_Device,
                              GEN_BufferPtr,
                              DA1_INTERNAL_BUFFER_SIZE - (DA1_WatermarkValue + 1),
                              0);

    /* Copy data from buffer start to watermark */
    GEN_BufferPtr += (DA1_INTERNAL_BUFFER_SIZE - (DA1_WatermarkValue + 1));

    /* Are we at the end of sinetable?  */
     if (GEN_BufferPtr >= GEN_BufferCurrentPtr + GEN_NUMBER_OF_SAMPLES)
     {
         GEN_BufferPtr = GEN_BufferCurrentPtr;
     }
}


/*
** ===================================================================
**     Event       :  DA1_OnBufferEnd
**
**     Component   :  DA1 [DAC_LDD]
**     Description :
**         Buffer read pointer reached buffer top position. This event
**         can be enabled only if Interrupt service is enabled.
**         Overrides default function in module Events.
**     Parameters  :
**         NAME            - DESCRIPTION
**       * UserDataPtr     - Pointer to the user or
**                           RTOS specific data.
**     Returns     : Nothing
** ===================================================================
*/

static vuint_32 DA1_BufferEndCount;

void DA1_OnBufferEnd
(
    LDD_TUserData *UserDataPtr
)
{
    DA1_BufferEndCount++;

    /* Copy data from watermark to end of buffer */
    DA1_Error = DA1_SetBuffer(DA1_Device,
                              GEN_BufferPtr,
                              (DA1_WatermarkValue + 1),
                              DA1_INTERNAL_BUFFER_SIZE - (DA1_WatermarkValue + 1));

    GEN_BufferPtr += (DA1_WatermarkValue + 1);

    /* Are we at the end of sinetable?  */
    if (GEN_BufferPtr >= GEN_BufferCurrentPtr + GEN_NUMBER_OF_SAMPLES)
    {
        GEN_BufferPtr = GEN_BufferCurrentPtr;
    }
}

/*FUNCTION**********************************************************************
*
* Function Name   : GEN_CreateTable()
*
* Input Params    : table_ptr   - pointer to source table to be rescaled.
*                   table_size  - the size of source table in bytes.
*                   peak_peak   - peak to peak value
*                   offset      - signal DC offset
*
* Returned Value  : output table pointer
*
* Comments        : Function creates table in RAM and return pointer
*   to table base address. The output data have rescaled by peak to peak value
*   and are shifted by DC offset.
*
*END***************************************************************************/

uint_16_ptr GEN_CreateTable
(
    int_16_ptr  table_ptr,
    uint_16     table_size,
    int_16      peak_peak,
    int_16      offset
)
{
    uint_32         i;
    /* Calculate number of samples in source table */
    uint_32         table_count = table_size / sizeof(uint_16);
    uint_16_ptr     out_table;
    int_32          temp;

    /* Allocate memory for table */
    if (NULL == (out_table = _mem_alloc(table_size)))    {
        return NULL;
    }

    for (i = 0; i < table_count; i++)    {

        temp   = table_ptr[i];
        temp  *= peak_peak / 2;
        temp >>= 12;
        temp  += offset;

        /* positive saturation */
        if (temp > 4095) temp = 4095;
        /* negative saturation */
        if (temp < 0)    temp = 0;

        out_table[i] = (uint_16)temp;
    }

    return out_table;
}

/*FUNCTION**********************************************************************
*
* Function Name   : GEN_DestroyTable()
*
* Input Params    : table_ptr   - pointer to table to be destroyed
*
* Returned Value  :
*
* Comments        :
*   Function destroys the table pointed by table_ptr .
*
*END***************************************************************************/

_mqx_uint GEN_DestroyTable
(
    uint_16_ptr table_ptr
)
{
    return _mem_free(table_ptr);
}
/* EOF */
