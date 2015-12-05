//------------------------------------------------------------------------------
// Copyright (c) 2011 Qualcomm Atheros, Inc.
// All Rights Reserved.
// Qualcomm Atheros Confidential and Proprietary.
// Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is
// hereby granted, provided that the above copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
// INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
// USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
//------------------------------------------------------------------------------
//==============================================================================
// Author(s): ="Atheros"
//==============================================================================


#include <string.h>
#include <mqx.h>
#include <bsp.h>
#include <rtcs.h>
#include <ipcfg.h>
#include <atheros_wifi_api.h>
#include "main.h"
#include "enetprv.h"
#include <enet_wifi.h>
#include <assert.h>

#define D(x)        

#ifdef USB_ENABLED
    extern void USBCDC_Task(uint_32 param);
    extern void usb_allow( void );
    
    #undef fgetc
    #undef fgets
    #undef fputs
    #undef fputc
#else
    #error "ERROR: USB_ENABLED is required for WIFI FCC Testing!"
#endif

#if !WHISTLE_UTF_AGENT
    #error "ERROR: This needs to be defined in user_config.h!"
#endif

uint_8 wbuffer[LINE_ARRAY_SIZE];
uint_32 wlength;
uint_8 fPacket;

static LWGPIO_STRUCT  pgood_b_gpio;

/*
 * Read  --- ART read data from AR600x via serial port
 * Write --- ART write data to AR600x via serial port
 * [0] = 0xaa: read start, 0xa1: large read start, 0xbb: write start, 0xb1: large write start, 0xcc: open
 * [1] = len
 * [2+] = data
*/

uint_8 line[512];
uint_8 lineLarge[LINE_ARRAY_SIZE];
static uint_8 ack[1];
uint_8 art_firmware_programmed = 0;
int sendUartLen=0;
A_UINT8 *writeBuf = NULL;
boolean ReadResp = FALSE;
volatile A_BOOL lWrite;
extern void* handle;         //Used to identify the driver context, must be passed in all calls to the driver
static MQX_FILE_PTR serial_fd = 0;
static LWSEM_STRUCT g_usb_mutex;
LWEVENT_STRUCT task_event1;
_enet_handle ehandle = 0;
static uint_8 g_usb_present = 0;

TASK_TEMPLATE_STRUCT MQX_template_list[] =
{
    /*  Task number, Entry point, Stack, Pri, String, Auto? */
   {MAIN_TASK,     Main_task,    4000,  8,   "main",   MQX_AUTO_START_TASK},
   {WRITE_TASK,    write_task,   4000,  7,   "write",  MQX_AUTO_START_TASK},
   {USB_CDC_TASK,  USBCDC_Task,  1024,  9,   "USBCDC", MQX_AUTO_START_TASK}, //NOTE: CDC must be lower priority.
   {0,             0,            0,     0,   0,       0}
};


extern A_UINT32 ar4XXX_boot_param;

/*
 *   Configure the RF Switch at PTB2 and PTB3 or Corona to be either for
 *     Bluetooth or WIFI.
 */
#define RF_SW_MASK 0x0Cu
#define RF_SW_V1   0x04u
#define RF_SW_V2   0x08u

/*
 *   A somewhat stupid wrapper function for usb_read(), which
 *     enables some debug "features".
 */
uint_32 utf_usb_read(uint_8 *pBuf, uint_32 sz)
{
    D(uint_32 count = 0;)
            
    usb_read(pBuf, sz);
    
    D
    (
        D(printf("\n\nusb_read() %i bytes\n", sz));
        while(count < sz)
        {
            D(printf("%x ", pBuf[count]));
            count++;
        }
        D(printf("\n\n");)
    )
    
    return sz;
}

#define NUM_DEBOUNCE_SAMPLES      (8)
#define NUM_DEBOUNCE_THRESHOLD    (2)
static uint_8 pwrmgr_debounce_pin( LWGPIO_STRUCT *pin )
{
    uint_8 i, d;
    uint_32 debounced = 0;
    LWGPIO_VALUE pinval;
    // Integrate the pin input until it's settled
    do 
    {
        // Integrate the pin input by sampling N times
        for(i=0;i<NUM_DEBOUNCE_SAMPLES;i++)
        {
            // read the pin
            pinval = lwgpio_get_value( pin );
            // Debounce using moving average method 
            debounced += (uint_8) pinval;
            // Small delay between pin samples
            for(d=0;d<30;d++)
                asm("NOP");
        }
    } while ( (debounced - NUM_DEBOUNCE_SAMPLES >> 1) < NUM_DEBOUNCE_THRESHOLD );
    // Return the settled level (1=High, 0=Low)
    return (debounced > (NUM_DEBOUNCE_SAMPLES>>1));
}  

/*****************************************************************************
*
*   @name        PWRMGR_PGOOD_B_int
*
*   @brief       Interrupt Handler for the PGOOD_B pin interrupt
*
*   @param       void *pin
*
*   @return      None
**
*****************************************************************************/
static void PWRMGR_PGOOD_B_int(void *pin)
{
    uint_8 usb_pin_level;
    lwgpio_int_clear_flag((LWGPIO_STRUCT_PTR)pin);
    
    // Poll the pin level and post the correct event     
    if ( (usb_pin_level = pwrmgr_debounce_pin( pin )) == 1 )
    {
        g_usb_present = 0;
    }
    else
    {
        // Falling Edge - USB was plugged in
        
        g_usb_present = 1;
    }
}


/*****************************************************************************
*
*   @name       PWRMGR_PGOOD_B_Init
*
*   @brief       This function initializes the PGOOD_B pin
*
*   @param       None
*
*   @return      None
**
*****************************************************************************/
static void PWRMGR_PGOOD_B_Init(void)
{
    _mqx_uint result;
    
    _int_disable(); // disable interrupts

    // initialise PGOOD_B for input
    if (!lwgpio_init(&pgood_b_gpio, BSP_GPIO_PGOOD_B_PIN, LWGPIO_DIR_INPUT, LWGPIO_VALUE_NOCHANGE))
    {
        goto PGOOD_INIT_DONE;
    }
    
    /* disable in case it's already enabled */
    lwgpio_int_enable(&pgood_b_gpio, FALSE);
    
    lwgpio_set_functionality(&pgood_b_gpio, BSP_GPIO_PGOOD_B_MUX_GPIO);
    lwgpio_set_attribute(&pgood_b_gpio, LWGPIO_ATTR_PULL_UP, LWGPIO_AVAL_ENABLE);

    // enable gpio functionality for given pin, react on rising or falling edge 
    if (!lwgpio_int_init(&pgood_b_gpio, LWGPIO_INT_MODE_FALLING | LWGPIO_INT_MODE_RISING))
    {
        goto PGOOD_INIT_DONE;
    }
    
    // install the PGOOD_B handler using GPIO port isr mux 
    whistle_lwgpio_isr_install(&pgood_b_gpio, BSP_PORTE_INT_LEVEL, PWRMGR_PGOOD_B_int);   
   
PGOOD_INIT_DONE:
    _int_enable(); // enable interrupts
}

/*
 *   Assumes we are using AS-193
 */
void whistle_atheros_set_rf_switch(boolean toWifi)
{
    uint32_t portb = GPIO_PDOR_REG( PTB_BASE_PTR );
    
    portb &= ~RF_SW_MASK;
    
    if(toWifi)
    {
        /*
         *   Configure RF switch for WIFI.
         */
        portb |= RF_SW_V2;
    }
    else
    {
        /*
         *   Configure RF switch for BLUETOOTH.
         */
        portb |= RF_SW_V1;
    }
    
    GPIO_PDOR_REG( PTB_BASE_PTR ) = portb;
    
#if 0
    /*
     *   NOT using this method, b/c both signals must be set simultaneously.
     */
    lwgpio_set_value(&sw_v1, toWifi ? LWGPIO_VALUE_LOW : LWGPIO_VALUE_HIGH);
    lwgpio_set_value(&sw_v2, toWifi ? LWGPIO_VALUE_HIGH : LWGPIO_VALUE_LOW);
#endif

    _time_delay(1);  // give a bit of time to let the RF switch state "settle".
}

static void initialize_networking(void) 
{
    LWGPIO_STRUCT sw_v1, sw_v2;
    int_32 error;
    
    assert(lwgpio_init(&sw_v1, BSP_GPIO_RF_SW_V1_PIN, LWGPIO_DIR_OUTPUT,
            LWGPIO_VALUE_NOCHANGE));
    
    
    /* swich pin functionality (MUX) to GPIO mode */
    lwgpio_set_functionality(&sw_v1, BSP_GPIO_RF_SW_V1_MUX_GPIO);
    
    assert(lwgpio_init(&sw_v2, BSP_GPIO_RF_SW_V2_PIN, LWGPIO_DIR_OUTPUT,
            LWGPIO_VALUE_NOCHANGE));
    
    /* swich pin functionality (MUX) to GPIO mode */
    lwgpio_set_functionality(&sw_v2, BSP_GPIO_RF_SW_V2_MUX_GPIO);
    
    whistle_atheros_set_rf_switch(TRUE);  // WIFI
    
    // Disable WiFi
    GPIO_PDOR_REG( PTE_BASE_PTR ) |= (1 << 4); // make sure WIFI_PWDN is de-asserted.
    
    _time_delay( 1000 );

#ifdef EFM_UTF
     Custom_Api_Start(&handle);
#else

    IPCFG_default_enet_device = ENET_DEVICE;
    IPCFG_default_ip_address = ENET_IPADDR;
    
    //D(printf("Retrieving MAC Address...\n"); fflush(stdout);)
    if( 0 == ENET_get_mac_address (IPCFG_default_enet_device, IPCFG_default_ip_address, IPCFG_default_enet_address) )
    {
        //D(printf("ERROR: retrieving MAC address!\n"); fflush(stdout);
        assert(0);
    }
    
    //D(printf("MAC Address: %02x%02x%02x%02x%02x%02x\n",
    //        IPCFG_default_enet_address[0], IPCFG_default_enet_address[1], IPCFG_default_enet_address[2],
    //        IPCFG_default_enet_address[3], IPCFG_default_enet_address[4], IPCFG_default_enet_address[5]); 
    //fflush(stdout);)
    
    error = ENET_initialize(IPCFG_default_enet_device, IPCFG_default_enet_address, 0, &ehandle);
    if (ENET_OK != error) 
    {
        //D(printf("ERROR (0x%x): ENET_initialize!!!\n", error); fflush(stdout);)
        assert(0);
    }  
#endif
    //D(printf("ENET_initialize -- OK\n"); fflush(stdout);)
}

static uint_32 open_serial_port(void)
{
#ifndef USB_ENABLED
    uint_32 flags = 0;
    uint_32 baudrate = 115200; 

    serial_fd = fopen(BSP_DEFAULT_IO_CHANNEL, 0);
    assert(serial_fd != NULL);
    assert(MQX_OK == ioctl(serial_fd, IO_IOCTL_SERIAL_SET_FLAGS, &flags));
    
   /* Set the baud rate on the serial device */
   assert(MQX_OK == ioctl(serial_fd, IO_IOCTL_SERIAL_SET_BAUD, (pointer)&baudrate));  

   /* Turn off xon/xoff, translation and echo on the serial device */
   assert(MQX_OK == ioctl(serial_fd, IO_IOCTL_SERIAL_GET_FLAGS, (pointer)&flags));
   flags &= ~(IO_SERIAL_XON_XOFF | IO_SERIAL_TRANSLATION | IO_SERIAL_ECHO);

   assert(MQX_OK == ioctl(serial_fd, IO_IOCTL_SERIAL_SET_FLAGS, &flags));
#endif
    return MQX_OK;
} // end open_serial_port

static void Set_Resp_required(_enet_handle handle,boolean resp)
{
    ATH_IOCTL_PARAM_STRUCT  inout_param;
     uint_32 error;
     
    inout_param.data = (void *) &resp;
    inout_param.cmd_id = ATH_SET_RESP;

#ifdef EFM_UTF
    error = ath_ioctl_handler (handle/*,ENET_MEDIACTL_VENDOR_SPECIFIC*/,&inout_param);
    if (A_OK != error)
#else
    error = ENET_mediactl(handle,ENET_MEDIACTL_VENDOR_SPECIFIC,&inout_param);
    if (ENET_OK != error)
#endif
    if (A_OK != error)
    {
        return;
    }
}

int tcmd_tx(_enet_handle handle,void *buf, int len, boolean resp)
{
    ATH_IOCTL_PARAM_STRUCT  inout_param;
    uint_32 error;
   
    ReadResp = resp;

    inout_param.data =  buf;
    inout_param.length = len;
    inout_param.cmd_id = ATH_WRITE_TCMD;
#ifdef EFM_UTF
    error = ath_ioctl_handler (handle/*,ENET_MEDIACTL_VENDOR_SPECIFIC*/,&inout_param);
    if (A_OK != error)
#else
    error = ENET_mediactl(handle, ENET_MEDIACTL_VENDOR_SPECIFIC, &inout_param);
    if (ENET_OK != error)
#endif
    {
        assert(0);
    }

    return ENET_OK;
}

static int wmiSend(_enet_handle handle,unsigned char *cmdBuf, unsigned int len, unsigned int totalLen, unsigned char version, boolean resp)
{
    TC_CMDS tCmd;

    memset(&tCmd,0,sizeof(tCmd));

    tCmd.hdr.testCmdId = TC_CMDS_ID;
    tCmd.hdr.u.parm.length = totalLen;
    tCmd.hdr.u.parm.version = version;
    tCmd.hdr.u.parm.bufLen = len;   // less than 256
    memcpy((void*)tCmd.buf, (void*)cmdBuf, len);

    Set_Resp_required(handle,resp);

    assert( ENET_OK == tcmd_tx(handle, (char*)&tCmd, sizeof(tCmd), resp) );
    return ENET_OK;
}

void tcmd_update_com_len(int Sendlen)
{
    sendUartLen = Sendlen;
}

//------------------------------------------------------------------------------------------
// writes zero terminated string to the serial port
// return code:
//   >= 0 = number of characters written
//   -1 = write failed
static uint_32 write_serial_port(uint_32 length)
{
    uint_32 i;
    
    D(printf("Write Serial Port Len: %i\n", length);)
    
#ifdef USB_ENABLED

    assert( 0xC1 != usb_write(line, length ));

#else
    
    for(i = 0; i < length; i++) 
    {
    
    #ifdef EFM_UTF
            USART1_TxByte((char) line[i]);
    #else
            fputc((char) line[i], serial_fd);
    #endif  
    }

#endif

    return length;
} // end write_serial_port

//------------------------------------------------------------------------------------------
// send ACK to serial port
static uint_32 send_serial_ack()
{
#ifndef USB_ENABLED
    #ifdef EFM_UTF
        USART1_TxByte((char) ack[0]);
    #else
        fputc((char) ack[0], serial_fd);
    #endif
#else
        D(printf("send serial_ack()\n");)
        assert( 0xC1 != usb_write(&(ack[0]), 1 ));
#endif
    return 1;
}

//------------------------------------------------------------------------------------------
// read ACK from the serial port
static void read_serial_ack()
{
    
#ifdef USB_ENABLED
    uint_8 iIn;
#else
    int_32 iIn;
#endif
    ack[0]= 0x00;
    
#ifdef USB_ENABLED
    assert( 1 == utf_usb_read(&iIn, 1) );
    if( iIn != 0xFF)
#else
    iIn = fgetc(serial_fd);
    if( iIn != IO_ERROR)
#endif
    {
        ack[0] = (uint_8) iIn;   
    }

#ifdef EFM_UTF 
        while(1)
        {
            iIn = getchar();
            if( iIn >= 0 )
            {
               ack[0] = (uint_8) iIn;
               break;
            }
        }
#endif
} // end read_serial_ack
//------------------------------------------------------------------------------------------
// read one byte from the serial port
// return code:
//   1 = read successed
//   0 = read failed
static uint_32 read_serial_port(uint_32 lineIndex)
{
    uint_32 rtn = 0;
#ifndef USB_ENABLED
    int_32 iIn = 0;
    
    iIn = fgetc(serial_fd);
    if( iIn != IO_ERROR) 
#else
    uint_8 iIn = 0;
    
    assert( 1 == utf_usb_read(&iIn, 1) );
    if( iIn != 0xFF) 
#endif
    {
        line[lineIndex] = (uint_8) iIn;
        rtn = 1;    
    }

#ifdef EFM_UTF 
    while(1)
        {
            iIn = getchar();
            if( iIn >= 0 )
            {
                    line[lineIndex] = (uint_8) iIn;
                    rtn = 1;    
                    break;
            }
        }
#endif
    return rtn;
} // end read_serial_port

// closes the serial port
static void close_serial_port()
{
#ifndef USB_ENABLED
    fclose(serial_fd);
#endif
    serial_fd = NULL;
} // end close_serial_port


//------------------------------------------------------------------------------------------
static void Tcmd_Write_Cmd(_enet_handle fd, uint_8 *buf, uint_32 bytes,uint_8 *fPacket)
{
    int_32  chunkLen;
    uint_8* bufpos = NULL;  
    uint_8      resp = FALSE;
    uint_32 totalbytesRcd = bytes;
    bufpos = buf;
    Set_Resp_required(fd,0);
    
    while (bytes) 
    {
        if (bytes > 200)
            chunkLen = 200; 
        else
            chunkLen = bytes;

        bytes -= chunkLen;

        if(bytes <= 0)
        {
            *((uint_8 *)fPacket) = TRUE; 
            resp = TRUE;
        }
        wmiSend(fd, bufpos, chunkLen, totalbytesRcd, 1, resp);
        
        bufpos += chunkLen;
    }
}

static void Tcmd_Read_Resp(_enet_handle fd,A_UINT8 *localBuffer)
{
    ATH_IOCTL_PARAM_STRUCT  inout_param;
    
    uint_32 error;
   
    inout_param.data = (pointer) localBuffer;
    inout_param.cmd_id = ATH_READ_TCMD_BUF;

#ifdef EFM_UTF
    error = ath_ioctl_handler (handle/*,ENET_MEDIACTL_VENDOR_SPECIFIC*/,&inout_param);
    sendUartLen = *(unsigned int *)&lineLarge[12];
    writeBuf = lineLarge + 12;
    if (A_OK != error)
#else
    error = ENET_mediactl(fd,ENET_MEDIACTL_VENDOR_SPECIFIC,&inout_param);
    sendUartLen = *(unsigned int *)&lineLarge[12];
    writeBuf = lineLarge + 12;
    if (ENET_OK != error)
#endif
    {
        assert(0);
        return;
    }
}

static void CheckReceivedResp(_enet_handle fd)
{
    ATH_IOCTL_PARAM_STRUCT  inout_param;
    
    uint_32 error;
   
    inout_param.cmd_id = ATH_CHECK_WRITE_DONE;

    error = ENET_mediactl(fd, ENET_MEDIACTL_VENDOR_SPECIFIC, &inout_param);

    if (ENET_OK != error)
    {
        return;
    }
}

static void Tcmd_Write_Resp(A_UINT8 *locBuf, A_UINT32 orgLength)
{
    A_UINT32 sendLen = 0;
    sendLen = orgLength;

    while(sendLen)
    {
        memset(line,0,512);
        if(sendLen > PLATFORM_UART_BUF_SIZE)
        {
            A_MEMCPY(line + 2,locBuf,PLATFORM_UART_BUF_SIZE);
            line[0] = DEF_SERIAL_AGENT_LARGE_READ_START;
            line[1] = PLATFORM_UART_BUF_SIZE & 0xFF;
            //_time_delay(10);
            write_serial_port(PLATFORM_UART_BUF_SIZE+2);
            locBuf += PLATFORM_UART_BUF_SIZE;
            sendLen -= PLATFORM_UART_BUF_SIZE;
            wait_for_reply();
        }
        else
        {
            A_MEMCPY(line + 2,locBuf,sendLen);
            line[0] = DEF_SERIAL_AGENT_READ_START;
            line[1] = (uint_8)sendLen ;
            //_time_delay(10);
            write_serial_port(sendLen+2);
            locBuf += sendLen;
            sendLen -= sendLen;
            break;
        }
    }
}

//------------------------------------------------------------------------------------------
static void wait_for_reply()
{
    read_serial_ack();
    
    if ((ack[0] == DEF_SERIAL_AGENT_LARGE_WRITE_START) || (ack[0] == DEF_SERIAL_AGENT_LARGE_READ_START))
    {
        
    }
    else
    {
#ifdef EFM_UTF
        while(1);
#else
        assert(0);
#endif
    }

}

void ReadUart(A_UINT8* buf, int size)
{
  int i;
  uint_32 rtn=0;
  
#ifdef USB_ENABLED
  uint_8 iIn;
#else
  int_32 iIn;
#endif

#if 1 //#ifndef USB_ENABLED
      for(i=0;i<size;i++)
      {
    #ifdef EFM_UTF
          while(1)
          {
            iIn = getchar();
            if( iIn >= 0 )
            {
                    buf[i] = (uint_8) iIn;
                    break;
            }
          }    
    #else
          
    #ifdef USB_ENABLED
        //_time_delay(1);
        assert( 1 == utf_usb_read(&iIn, 1) );
        if( iIn != 0xFF) 
        
    #else
        iIn = fgetc(serial_fd);
        if( iIn != IO_ERROR) 
    #endif
        {
            buf[i] = (uint_8) iIn;
            rtn = 1;    
        }
    #endif
      }
#else
      
  assert( size == utf_usb_read(buf, size) );
  
#endif
}

//------------------------------------------------------------------------------------------
// this is the mainline thingee
void Main_task(uint_32 initial_data)
{
    uint_8    led = 0;
    uint_32   length = 0;
    uint_32   index = 0;
    uint_32   isRead = 0;
    uint_32   largeWritePos = 0;
    uint_32   orgLength = 0;
    uint_8    largeRead = 0;
    uint_8    largeWrite = 0;
    uint_32   totalLength = 0;
    ART_READ *artRead = NULL;
    uint_8    firstpacket = FALSE;
    uint32_t  stupid_loop_counter = 0;
    
#ifdef EFM_UTF
    /* Initialize BSP functions                             */
    BSP_Init();

    /* Initialize the uC/OS-II ticker                       */
    OS_CPU_SysTickInit(CMU_ClockFreqGet(cmuClock_HFPER)/OS_CFG_TICK_RATE_HZ);

    Mem_Init();  

    /* Initialize USART1                                    */
    USART1_Init();
#else
    
#endif
    
    D(printf("WIFI FCC Test\n"); fflush(stdout);)
    
    assert(MQX_OK == _lwsem_create(&g_usb_mutex, 1));
    PWRMGR_PGOOD_B_Init();
    
    while( ! g_usb_present )
    {
        _time_delay( 500 );
    }
    
    _time_delay( 500 );
    usb_allow();
    _time_delay( 500 );  // give time for the CDC to do its thing.
    
    D(printf("USB Insertion detected.\n"); fflush(stdout);)
    
    #if UTF_TEST_USB
    
        while(1)
        {
            char buf[10];
            
            memset(buf, 'a', 10);
            assert( 0xC1 != usb_write(buf, 10 ));
            _time_delay(1000);
            
            memset(buf, 0, 10);
            strcpy(buf, "\n\rTEST..\n\r");
            assert( 0xC1 != usb_write(buf, 10 ));
            _time_delay(1000);
        }
    
    #endif

    atheros_driver_setup_mediactl();

    ar4XXX_boot_param = (AR4XXX_PARAM_MANUFAC_MODE | AR4XXX_PARAM_MODE_NORMAL);

    initialize_networking();
    _time_delay(1000);

#ifndef USB_ENABLED
    assert(MQX_OK == open_serial_port());
#endif

    while (1)
    {
        /*  monitor activity */
        if(led > 15) led = 0;

        isRead        = 0xff;
        largeRead     = 0;
        largeWrite    = 0;
        largeWritePos = 0;
        totalLength   = 0;
        ReadResp = FALSE;
        
        stupid_loop_counter++;

        //read one byte into buf from serial port to determine the command 
#ifndef USB_ENABLED
    #ifdef EFM_UTF
            ReadUart(line,1); //Read cmd,length
    #else
            read(stdin, &line[0], 1);
    #endif
#else
        assert( 1 == utf_usb_read(&line[0], 1) );
#endif
        
        D(printf("line[0] = 0x%x\n", line[0]); fflush(stdout);)
        
        if (DEF_SERIAL_AGENT_START == line[0])
        {
            D(printf("Agent Start.\n"); fflush(stdout);)
            continue;
        }
        else if (line[0] == DEF_SERIAL_AGENT_READ_START)
        {
            D(printf("Agent READ Start.\n"); fflush(stdout);)
#ifndef USB_ENABLED
    #ifdef EFM_UTF
                ReadUart(&line[1],1);
    #else
                read(stdin, &line[1], 1);
    #endif
#else
                assert( 1 == utf_usb_read(&line[1], 1) );
#endif
            isRead = 1;
            //get read packet len with one byte 
            length = 0;
            length = line[1];
            orgLength = length;
            D(printf("Read Len: 0x%x\n", length);)
        }
        else if (line[0] == DEF_SERIAL_AGENT_LARGE_READ_START)
        {          
            D(printf("Large READ Start.\n"); fflush(stdout);)
            isRead = 1;
            largeRead = 1;
            //get read large packet len with 4 bytes

#ifndef USB_ENABLED
    #ifdef EFM_UTF
                ReadUart(&line[1],4);
    #else
                read(stdin, &line[1], 4);
    #endif
#else
                assert( 4 == utf_usb_read(&line[1], 4) );
#endif
            artRead = (ART_READ *) &line[0];
            orgLength = artRead->len;
        }
        else if (line[0] == DEF_SERIAL_AGENT_WRITE_START) // Dhana
        {
            uint_32 i_usb;
            
            D(printf("Agent WRITE Start.\n"); fflush(stdout);)
            
#ifndef USB_ENABLED
    #ifdef EFM_UTF
                ReadUart(&line[1],1);
    #else
                read(stdin,&line[1],1);
    #endif
#else
                assert( 1 == utf_usb_read(&line[1], 1) );
#endif
            length = 0;
            length = line[1];   
            isRead = 0;
            D(printf("Write Len: 0x%x\n", length);)
            
            //          get write packet len with one byte
            //          read_serial_port(1);
            //          length = line[1];
#ifndef USB_ENABLED
    #ifdef EFM_UTF
            ReadUart(line,length);                 
    #else
            read(stdin, line, length);
    #endif
#else
            assert( length == utf_usb_read(line, length) );
#endif
            orgLength = length; 
        }
        else if (line[0] == DEF_SERIAL_AGENT_LARGE_WRITE_START)
        {
            D(printf("Large WRITE Start.\n"); fflush(stdout);)
            
            isRead = 0;
            largeWrite = 1;
            //get write large packet len with one byte
            read_serial_port(1);
            length = 0;
            length = line[1];
            orgLength = length;
        }
        else
        {
            D(printf("Corrupted line[0] = 0x%x\n", line[0]); fflush(stdout);)
            _time_delay(500);
            
#ifdef EFM_UTF
            while(1);
#else
            assert(0);
#endif
        }

        if ((length > PLATFORM_UART_BUF_SIZE)&& (!largeRead))
        {
            //invalid length;
#ifdef EFM_UTF
            while(1);
#else
            assert(0);
#endif
        }

        if(firstpacket == FALSE) {
            firstpacket = TRUE;
        }

        if (0 == isRead)
        {
            if (line[0] == DEF_SERIAL_AGENT_LARGE_WRITE_START)
            {
                //large write

                largeWritePos = 0;
                while(line[0] != DEF_SERIAL_AGENT_WRITE_START)
                {
                    length = 0;
                    length = line[1];
                    if (length > PLATFORM_UART_BUF_SIZE)
                    {
                        //  D(printf("invalid length\n");
#ifdef EFM_UTF
                        while(1);
#else
                        assert(0);
#endif
                    }
                    totalLength += length;

                    for (index = 0; index < length; index++)
                    {
                        read_serial_port(index);
                    }
                    memcpy(&lineLarge[largeWritePos], line,length);
                    largeWritePos += length;
                    ack[0] = DEF_SERIAL_AGENT_LARGE_WRITE_START;
                    send_serial_ack();
                    read_serial_port(0);
                    read_serial_port(1);                                        
                }
                if (line[0] == DEF_SERIAL_AGENT_WRITE_START)
                {
                    length = 0;
                    length = line[1];
                    totalLength += length;

                    for (index = 0; index < length; index++)
                    {
                        read_serial_port(index);
                    }
                    memcpy(&lineLarge[largeWritePos], line,length);

                }
                else
                {
                    //D(printf("Unexpected line[0]\n");
#ifdef EFM_UTF
                    while(1);
#else
                    assert(0);
#endif
                }
            }
            else
            {               
            }
        }

        if (isRead == 1)
        {

            if (line[0] == DEF_SERIAL_AGENT_LARGE_READ_START)
            {

                Tcmd_Write_Resp(writeBuf, orgLength);
            }
            else
            {
		CheckReceivedResp(ehandle);
                Tcmd_Write_Resp(writeBuf, orgLength);
                writeBuf += orgLength;
            }
        }
        else
        {                  
            if (!largeWrite)
            {
                memcpy(wbuffer,line,orgLength);
                wlength = orgLength;
                fPacket = firstpacket;
#ifdef EFM_UTF
                Tcmd_Write_Cmd(ehandle,line,orgLength,&firstpacket);                             
                Tcmd_Read_Resp(ehandle,lineLarge);
#endif
                /* Resp will be filled up @ cust wmi_rx event */
            }
            else
            {
                lWrite = TRUE;
                wlength = totalLength;
                fPacket = firstpacket;
#ifdef EFM_UTF
                Tcmd_Write_Cmd(handle,lineLarge,totalLength,&firstpacket);
                Tcmd_Read_Resp(ehandle,lineLarge);
#endif
            }
            _lwevent_set(&task_event1,0x01);
        }
    } // end while
    close_serial_port();
} // end main

/*Write task takes on the responsibility to write ART messages to the target.
  This allows the main task to focus on incoming messages*/
void write_task(uint_32 initial_data)
{
    uint_32 flags = 0; 
       
    _lwevent_create(&task_event1, 1/* autoclear */);
        
    /* block for events from other tasks */
    for(;;)
    {
        switch(_lwevent_wait_ticks(&task_event1, 0x01, TRUE, 10000))
        {
            case MQX_OK:
                if(wlength != 0)
                {
                    /* write TCMD */
                    if (lWrite) 
                    {
                        Tcmd_Write_Cmd(ehandle,lineLarge,wlength,&fPacket);                             
                        lWrite = FALSE;
                    }
                    else
                    {
                        Tcmd_Write_Cmd(ehandle,wbuffer,wlength,&fPacket);                             
                    }
                    
                    Tcmd_Read_Resp(ehandle,lineLarge);
                    wlength= 0;         
                    fPacket = FALSE;
                }
                break;
                
            case LWEVENT_WAIT_TIMEOUT:  
                break;
                
            default:
                break;
        }   
        
        if(flags == 1)
        {
            break;
        }
    }
    
    _lwevent_destroy(&task_event1);
}

// App Header
// Pragma to place the App header field on correct location defined in linker file.
#pragma define_section appheader ".appheader" far_abs R

__declspec(appheader) uint8_t apph[0x20] = {
    // Name [0,1,2]
    'A', 'P', 'P',      // 0x41, 0x50, 0x50
    // Version [3,4,5]
    0x00, 0x01, 0x00,
    // Checksum of Name and Version [6]
    0x1EU,
    // Success, Update, 3 Attempt flags, BAD flag [7,8,9,A,B,C]
    0x55U, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU,
    // SAVD [D], Reserved [E-F]
    0xFFU, 0xFFU, 0xFFU,
    // Version tag [10-1F]
    'A','L','P','H','A','_','1',0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU,0xFFU, 0xFFU, 0xFFU
};
  
/* EOF */

