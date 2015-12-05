#ifdef USB_ENABLED

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
 * $FileName: virtual_com.c$
 * $Version : 3.8.4.1$
 * $Date    : Dec-7-2011$
 *
 * Comments:
 *
 * @brief  The file emulates a USB PORT as RS232 PORT.
 *****************************************************************************/

/******************************************************************************
 * Includes
 *****************************************************************************/
#include "usb_descriptor.h"
#include "virtual_com.h"
#include "wassert.h"
#include <mqx.h>
#include <mutex.h>

#define D(x)    
#ifdef WHISTLE_UTF_AGENT
    #include <assert.h>
#else
    #include "wassert.h"
#endif

static MUTEX_STRUCT g_vcom_mutex;
extern void USBCDC_Task(uint_32 param);

/*****************************************************************************
* Constant and Macro's - None
*****************************************************************************/

/*****************************************************************************
* Global Functions Prototypes
*****************************************************************************/
void CDCApp_Init(void);

/****************************************************************************
* Global Variables
****************************************************************************/
extern USB_ENDPOINTS usb_desc_ep;
extern DESC_CALLBACK_FUNCTIONS_STRUCT desc_callback;

CDC_HANDLE g_app_handle;

/*****************************************************************************
* Local Types - None
*****************************************************************************/

/*****************************************************************************
* Local Functions Prototypes
*****************************************************************************/
void USB_App_Callback(uint_8 event_type, void *val, pointer arg);
void USB_Notif_Callback(uint_8 event_type, void *val, pointer arg);
void Virtual_Com_App(void);

/*****************************************************************************
* Local Variables
*****************************************************************************/
static boolean start_app          = FALSE;
static boolean start_transactions = FALSE;
static boolean system_allow_usb   = FALSE;
static uint_8  *g_buffered_usb_rx;
static uint_8  *g_curr_recv_buf;
static uint_8  *g_curr_send_buf;
static uint_8  g_recv_size;
static uint_32 g_desired_recv_size;
static uint_8  *g_recv_buf = NULL;

static LWEVENT_STRUCT g_recv_lwevent;
#define RECV_COMPLETE    0x1
#define RECV_NOTIFY      0x2

/*****************************************************************************
* Local Functions
*****************************************************************************/

/*****************************************************************************
*
*   @name        TestApp_Init
*
*   @brief       This function is the entry for mouse (or other usuage)
*
*   @param       None
*
*   @return      None
**
*****************************************************************************/
void CDCApp_Init(void)
{
    //uint_8   error;
    CDC_CONFIG_STRUCT      cdc_config;
    USB_CLASS_CDC_ENDPOINT *endPoint_ptr;

    g_curr_recv_buf = _mem_alloc_uncached(DATA_BUFF_SIZE);
    g_curr_send_buf = _mem_alloc_uncached(DATA_BUFF_SIZE);
    g_buffered_usb_rx = _mem_alloc_uncached(DATA_BUFF_SIZE);
    
    wmutex_init(&g_vcom_mutex);

    endPoint_ptr = USB_mem_alloc_zero(sizeof(USB_CLASS_CDC_ENDPOINT) * CDC_DESC_ENDPOINT_COUNT);
    cdc_config.comm_feature_data_size       = COMM_FEATURE_DATA_SIZE;
    cdc_config.usb_ep_data                  = &usb_desc_ep;
    cdc_config.desc_endpoint_cnt            = CDC_DESC_ENDPOINT_COUNT;
    cdc_config.cdc_class_cb.callback        = USB_App_Callback;
    cdc_config.cdc_class_cb.arg             = &g_app_handle;
    cdc_config.vendor_req_callback.callback = NULL;
    cdc_config.vendor_req_callback.arg      = NULL;
    cdc_config.param_callback.callback      = USB_Notif_Callback;
    cdc_config.param_callback.arg           = &g_app_handle;
    cdc_config.desc_callback_ptr            = &desc_callback;
    cdc_config.ep = endPoint_ptr;
    cdc_config.cic_send_endpoint = CIC_NOTIF_ENDPOINT;
    /* Always happend in control endpoint hence hard coded in Class layer*/
    //cdc_config.cic_recv_endpoint =
    cdc_config.dic_send_endpoint = DIC_BULK_IN_ENDPOINT;
    cdc_config.dic_recv_endpoint = DIC_BULK_OUT_ENDPOINT;

    wint_disable();

    /* Initialize the USB interface */
    g_app_handle = USB_Class_CDC_Init(&cdc_config);
    g_recv_size  = 0;

    wint_enable();

    /* Create the lightweight semaphore */
    _lwevent_create(&g_recv_lwevent, LWEVENT_AUTO_CLEAR);

    while (TRUE)
    {
        /* call the periodic task function */
#if DELAYED_PROCESSING
#error "CA - This was rewritten to not use delayed processing. It wasn't in the first place, but now explicitly not. Didn't work anyway (Look at refs to DELAYED_PROCESSING)"
        USB_CDC_Periodic_Task();
#endif

        /*check whether enumeration is complete or not */
        if ((start_app == TRUE) && (start_transactions == TRUE))
        {
            Virtual_Com_App();
        }
        else
        {
            _time_delay(500);
        }
    } /* Endwhile */
}


/******************************************************************************
 *
 *    @name       usb_read
 *
 *    @brief      Read API for USB CDC data. Blocks until required data read
 *
 *    @param      pointer to recv buffer and length to read
 *
 *    @return     number of bytes read
 *
 *****************************************************************************/
int usb_read(uint_8 *buf, size_t size)
{
    g_desired_recv_size = size;
    g_recv_buf          = buf;

    _lwevent_wait_for(&g_recv_lwevent, RECV_COMPLETE, FALSE, 0);

    return g_desired_recv_size;
}


/******************************************************************************
 *
 *    @name       usb_write
 *
 *    @brief      Write API for USB CDC data. Blocks until required data written
 *
 *    @param      pointer to transmit buffer and length to write
 *
 *    @return     USB Error code.
 *
 *****************************************************************************/
int usb_write(uint_8 *buf, size_t size)
{
    int             status;
    uint32_t        usb_retries  = 10;
    static uint32_t usb_send_err = 0;  // just for diagnostics...

    do
    {
        status = USB_Class_CDC_Send_Data(g_app_handle, DIC_BULK_IN_ENDPOINT, buf, size);
        if (status == 0xC1 /*USBERR_DEVICE_BUSY*/)
        {
            USB_Class_CDC_Wipe_Data(g_app_handle, DIC_BULK_IN_ENDPOINT);
            usb_send_err++;
            _time_delay(250);
        }
    } while ((status == 0xC1 /*USBERR_DEVICE_BUSY*/) && (usb_retries-- != 0));

    return status;
}

void usb_allow( void )
{
    /*
     *   TODO (COR-196):
     *   Post message that USB connected.
     */
    system_allow_usb = TRUE;
}

boolean usb_is_enabled( void )
{
#if 0
    const uint32_t PGOOD_B_MASK = (uint32_t)0x00000004;
    
    /*
     *   TODO (COR-196):
     *   Remove this function.  Instead of people using this function, they should instead wait on a message.
     */
    if( 0 == (GPIOE_PDIR & PGOOD_B_MASK) )
    {
        system_allow_usb = TRUE;
    }
    else
    {
        system_allow_usb = FALSE;
    }
#endif
    return system_allow_usb;
}

/******************************************************************************
 *
 *    @name       Virtual_Com_App
 *
 *    @brief
 *
 *    @param      None
 *
 *    @return     None
 *
 *****************************************************************************/
void Virtual_Com_App(void)
{
    _mqx_int recv_index = 0;

    /* User Code */
    while (1)
    {
        if (g_recv_size)
        {
            if (g_recv_buf != NULL)
            {
                _mqx_int i;
                
                _mutex_lock(&g_vcom_mutex);

                /* copy to usb_read() buf */
                for (i = 0; i < g_desired_recv_size && recv_index < g_recv_size; i++, recv_index++)
                {
                    g_recv_buf[i] = g_buffered_usb_rx[recv_index];
                }

                if (recv_index >= g_recv_size)
                {
                    recv_index = 0;
                    g_recv_size = 0;
                }

                /* check if received all usb_read() asked for */
                if (i >= g_desired_recv_size)
                {
                    g_recv_buf = NULL;
                    _lwevent_set(&g_recv_lwevent, RECV_COMPLETE);
                }
                
                _mutex_unlock(&g_vcom_mutex);
            }
            else
            {
                _time_delay(100);
            }

            /* resume the notification pump */
            if (recv_index == 0)
            {
                /* Schedule buffer for next receive event */
                USB_Class_CDC_Recv_Data(g_app_handle, DIC_BULK_OUT_ENDPOINT,
                                        g_curr_recv_buf, DIC_BULK_OUT_ENDP_PACKET_SIZE);
            }
        }
        else
        {
            /* wait for the notification pump to provide more data */
            _lwevent_wait_for(&g_recv_lwevent, RECV_NOTIFY, FALSE, 0);
        }
    }
}


/******************************************************************************
 *
 *    @name        USB_App_Callback
 *
 *    @brief       This function handles the callback
 *
 *    @param       handle : handle to Identify the controller
 *    @param       event_type : value of the event
 *    @param       val : gives the configuration value
 *
 *    @return      None
 *
 *****************************************************************************/
void USB_App_Callback(uint_8 event_type, void *val, pointer arg)
{
    UNUSED_ARGUMENT(arg)
    UNUSED_ARGUMENT(val)
    if (event_type == USB_APP_BUS_RESET)
    {
        start_app = FALSE;
    }
    else if (event_type == USB_APP_ENUM_COMPLETE)
    {
        start_app = TRUE;
    }
    else if (event_type == USB_APP_ERROR)
    {
        /* add user code for error handling */
    }
}


/******************************************************************************
 *
 *    @name        USB_Notif_Callback
 *
 *    @brief       This function handles the callback
 *
 *    @param       handle:  handle to Identify the controller
 *    @param       event_type : value of the event
 *    @param       val : gives the configuration value
 *
 *    @return      None
 *
 *****************************************************************************/
void USB_Notif_Callback(uint_8 event_type, void *val, pointer arg)
{
    uint_8 index;

    /* Note, this is called in ISR context!!! */

    g_app_handle = *((uint_32 *)arg);
    if (start_app == TRUE)
    {
        if (event_type == USB_APP_CDC_DTE_ACTIVATED)
        {
            start_transactions = TRUE;
        }
        else if (event_type == USB_APP_CDC_DTE_DEACTIVATED)
        {
            start_transactions = FALSE;
        }
        else if ((event_type == USB_APP_DATA_RECEIVED) &&
                 (start_transactions == TRUE))
        {
            D(uint_32 stupid_count = 0;)
            APP_DATA_STRUCT *dp_rcv;
            dp_rcv = (APP_DATA_STRUCT *)val;
            
            _mutex_lock(&g_vcom_mutex);

#ifdef wassert
            wassert( (dp_rcv->data_size + g_recv_size) <= DATA_BUFF_SIZE);  // make sure we don't over-run the buffer!
#else
            assert( (dp_rcv->data_size + g_recv_size) <= DATA_BUFF_SIZE);  // make sure we don't over-run the buffer!
#endif
            memcpy((g_buffered_usb_rx + g_recv_size), dp_rcv->data_ptr, dp_rcv->data_size);
            
            D(printf("\n\nUSB RX (%i bytes)\n", dp_rcv->data_size);)
            
            D
            (
                while(stupid_count < dp_rcv->data_size)
                {
                    printf("%x ", dp_rcv->data_ptr[stupid_count]);
                    stupid_count++;
                }
                printf("\n\n");
            )

            g_recv_size += dp_rcv->data_size;
            
            _mutex_unlock(&g_vcom_mutex);

            /* notify waiting receivers. won't resume until USB_Class_CDC_Recv_Data called */
            _lwevent_set(&g_recv_lwevent, RECV_NOTIFY);
        }
        else if ((event_type == USB_APP_SEND_COMPLETE) &&
                 (start_transactions == TRUE))
        {
            /* User: add your own code for send complete event */
        }
    }
}


/*FUNCTION*----------------------------------------------------------------
*
* Function Name  : Main_Task
* Returned Value : None
* Comments       :
*     First function called.  Calls the Test_App
*     callback functions.
*
* END*--------------------------------------------------------------------*/
void USBCDC_Task
(
    uint_32 param
)
{
    while (1)
    {
        // Wait for system to allow USB to launch
        
        // TODO - Use message here instead of polling.
        if ( system_allow_usb )
        {
            UNUSED_ARGUMENT(param)
                CDCApp_Init();
        }
        _time_delay(100);
    }
}

#endif // USB_ENABLED
/* EOF */
