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



uint_8 wbuffer[LINE_ARRAY_SIZE];
uint_32 wlength;
uint_8 fPacket;

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
static MQX_FILE_PTR output_port = NULL;
static MQX_FILE_PTR serial_fd = 0;
LWEVENT_STRUCT task_event1;
_enet_handle ehandle = 0;

TASK_TEMPLATE_STRUCT MQX_template_list[] =
{
    /*  Task number, Entry point, Stack, Pri, String, Auto? */
   {MAIN_TASK,   Main_task,   2500,  9,   "main", MQX_AUTO_START_TASK},
   {WRITE_TASK,  write_task,   2500,  9,   "write", MQX_AUTO_START_TASK},
   {0,           0,           0,     0,   0,      0}
};


extern A_UINT32 ar4XXX_boot_param;

static void initialize_networking(void) 
{
#ifdef EFM_UTF
     Custom_Api_Start(&handle);
#else
    int_32 error;

    IPCFG_default_enet_device = ENET_DEVICE;
    IPCFG_default_ip_address = ENET_IPADDR;
    
    ENET_get_mac_address (IPCFG_default_enet_device, IPCFG_default_ip_address, IPCFG_default_enet_address);
    error = ENET_initialize(IPCFG_default_enet_device, IPCFG_default_enet_address, 0, &ehandle);

    if (ENET_OK != error) 
    {
        _task_block();
    }  
#endif
}

static uint_32 open_serial_port(void)
{
    uint_32 flags = 0;
    uint_32 baudrate = 115200; 

    serial_fd = fopen(BSP_DEFAULT_IO_CHANNEL, 0);
    ioctl(serial_fd, IO_IOCTL_SERIAL_SET_FLAGS, &flags);
    
   /* Set the baud rate on the serial device */
   if (ioctl(serial_fd, IO_IOCTL_SERIAL_SET_BAUD, 
      (pointer)&baudrate) != MQX_OK)  
   {
      fclose(serial_fd);
      serial_fd = NULL;
      return(IO_ERROR);
   } /* Endif */

   /* Turn off xon/xoff, translation and echo on the serial device */
   ioctl(serial_fd, IO_IOCTL_SERIAL_GET_FLAGS, (pointer)&flags);
   flags &= ~(IO_SERIAL_XON_XOFF | IO_SERIAL_TRANSLATION | IO_SERIAL_ECHO);

   ioctl(serial_fd, IO_IOCTL_SERIAL_SET_FLAGS, &flags);

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
    error = ENET_mediactl(handle,ENET_MEDIACTL_VENDOR_SPECIFIC,&inout_param);
    if (ENET_OK != error)
#endif
    {
        return 0;
    }

    return error;
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

    if ((tcmd_tx(handle,(char*)&tCmd, sizeof(tCmd), resp))) {
        printf("tcmd_tx had error \n");
        return 0;
    }

    return 1;
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
    for(i = 0; i < length; i++) {
#ifdef EFM_UTF
        USART1_TxByte((char) line[i]);
#else
        fputc((char) line[i], serial_fd);
#endif
    }

    return length;
} // end write_serial_port

//------------------------------------------------------------------------------------------
// send ACK to serial port
static uint_32 send_serial_ack()
{
#ifdef EFM_UTF
    USART1_TxByte((char) ack[0]);
#else
    fputc((char) ack[0], serial_fd);
#endif
    return 1;
} 

//------------------------------------------------------------------------------------------
// read ACK from the serial port
static void read_serial_ack()
{
    int_32 iIn;

    ack[0]= 0x00;
    iIn = fgetc(serial_fd);
    if( iIn !=IO_ERROR)
        ack[0] = (uint_8) iIn;

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
    int_32 iIn;
    uint_32 rtn = 0;
    iIn = fgetc(serial_fd);
    if( iIn !=IO_ERROR) 
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
    fclose(serial_fd);
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
    
    while (bytes) {
        if (bytes > 200)
            chunkLen = 200; 
        else
            chunkLen = bytes;

        bytes -= chunkLen;

        if(bytes <= 0){
            *((uint_8 *)fPacket) = TRUE; 
            resp = TRUE;
        }
        wmiSend(fd,bufpos,chunkLen,totalbytesRcd,1,resp);
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
        return;
    }
}

static void CheckReceivedResp(_enet_handle fd)
{
    ATH_IOCTL_PARAM_STRUCT  inout_param;
    
    uint_32 error;
   
    inout_param.cmd_id = ATH_CHECK_WRITE_DONE;

    error = ENET_mediactl(fd,ENET_MEDIACTL_VENDOR_SPECIFIC,&inout_param);

    if (ENET_OK != error)
    {
        return;
    }
}

static void Tcmd_Write_Resp(A_UINT8 *locBuf,A_UINT32 orgLength)
{
    A_UINT32 sendLen =0;
    sendLen = orgLength;

    while(sendLen)
    {
        memset(line,0,512);
        if(sendLen > PLATFORM_UART_BUF_SIZE){
            A_MEMCPY(line + 2,locBuf,PLATFORM_UART_BUF_SIZE);
            line[0] = DEF_SERIAL_AGENT_LARGE_READ_START;
            line[1] = PLATFORM_UART_BUF_SIZE & 0xFF;
            write_serial_port(PLATFORM_UART_BUF_SIZE+2);
            locBuf += PLATFORM_UART_BUF_SIZE;
            sendLen -= PLATFORM_UART_BUF_SIZE;
            wait_for_reply();
        }else{
            A_MEMCPY(line + 2,locBuf,sendLen);
            line[0] = DEF_SERIAL_AGENT_READ_START;
            line[1] = sendLen;
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
    if ((ack[0] == DEF_SERIAL_AGENT_LARGE_WRITE_START) ||(ack[0] == DEF_SERIAL_AGENT_LARGE_READ_START))
    {
    }
    else
    {
#ifdef EFM_UTF
        while(1);
#else
        _task_block();  
#endif
    }
}

void ReadUart(A_UINT8* buf, int size)
{
  int i;
  int_32 iIn;
    uint_32 rtn=0;
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
    iIn = fgetc(serial_fd);
    if( iIn !=IO_ERROR) 
    {
        buf[i] = (uint_8) iIn;
        rtn = 1;    
    }
#endif
  }
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
    atheros_driver_setup_mediactl();

    ar4XXX_boot_param = (AR4XXX_PARAM_MANUFAC_MODE | AR4XXX_PARAM_MODE_NORMAL);

    initialize_networking();

    _time_delay(1000);

    //open serial port 
    if( open_serial_port() != MQX_OK)
    {
        //ERROR: failed to open serial port
        _task_block();          
    }

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

        //read one byte into buf from serial port to determine the command 
#ifdef EFM_UTF
        ReadUart(line,1); //Read cmd,length
#else
        read(stdin,&line[0],1);
#endif

        if (DEF_SERIAL_AGENT_START == line[0])
        {
            //Agent is already started, ignore....
            continue;
        }
        else if (line[0] == DEF_SERIAL_AGENT_READ_START)
        {
#ifdef EFM_UTF
            ReadUart(&line[1],1);
#else
            read(stdin,&line[1],1);
#endif
            isRead = 1;
            //get read packet len with one byte 
            length = line[1];
            orgLength = length;

        }
        else if (line[0] == DEF_SERIAL_AGENT_LARGE_READ_START)
        {          
            isRead = 1;
            largeRead = 1;
            //get read large packet len with 4 bytes

#ifdef EFM_UTF
            ReadUart(&line[1],4);
#else
            read(stdin,&line[1],4);
#endif
            artRead = (ART_READ *) &line[0];
            orgLength = HOST_TO_BE_LONG(artRead->len);
        }
        else if (line[0] == DEF_SERIAL_AGENT_WRITE_START) // Dhana
        {
#ifdef EFM_UTF
            ReadUart(&line[1],1);
#else
            read(stdin,&line[1],1);
#endif
            length = line[1];   
            isRead = 0;
            //          get write packet len with one byte
            //          read_serial_port(1);
            //          length = line[1];
#ifdef EFM_UTF
            ReadUart(line,length);                 
#else
            read(stdin,line,length);
#endif
            orgLength = length; 
        }
        else if (line[0] == DEF_SERIAL_AGENT_LARGE_WRITE_START)
        {
            isRead = 0;
            largeWrite = 1;
            //get write large packet len with one byte
            read_serial_port(1);
            length = line[1];
            orgLength = length;
        }
        else
        {
            //corrupted line received ;
#ifdef EFM_UTF
            while(1);
#else
            _task_block();
#endif
        }

        if ((length > PLATFORM_UART_BUF_SIZE)&& (!largeRead))
        {
            //invalid length;
#ifdef EFM_UTF
            while(1);
#else
            _task_block();
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
                    length = line[1];
                    if (length > PLATFORM_UART_BUF_SIZE)
                    {
                        //  printf("invalid length\n");
#ifdef EFM_UTF
                        while(1);
#else
                        _task_block();
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
                    printf("Unexpected line[0]\n");
#ifdef EFM_UTF
                    while(1);
#else
                    _task_block();
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

                Tcmd_Write_Resp(writeBuf,orgLength);
            }
            else
            {
		CheckReceivedResp(ehandle);
                Tcmd_Write_Resp(writeBuf,orgLength);
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
    for(;;){
        switch(_lwevent_wait_ticks(&task_event1, 0x01, TRUE, 10000))
        {
        case MQX_OK:
            if(wlength != 0)
            {
                /* write TCMD */
		if (lWrite) {
                    Tcmd_Write_Cmd(ehandle,lineLarge,wlength,&fPacket);                             
		    lWrite = FALSE;
		}else{
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
        
        if(flags == 1){
            break;
        }
    }
    _lwevent_destroy(&task_event1);
}
  
/* EOF */

