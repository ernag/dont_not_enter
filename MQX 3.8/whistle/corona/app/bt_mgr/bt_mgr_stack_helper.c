/*
 * bt_mgr_stack_helper.c
 *
 *  Created on: Apr 28, 2013
 *      Author: Chris
 */
#include <mqx.h>
#include <bsp.h>
#include "wassert.h"
#include "lwmsgq.h"

#include "HAL.h"                 /* Function for Hardware Abstraction.        */
#include "SS1BTPS.h"             /* Main SS1 BT Stack Header.                 */
#include "BTPSKRNL.h"            /* BTPS Kernel Header.                       */
#include "BTPSVEND.h"            /* Vendor specific command header.           */
#include "SS1BTISP.h"            /* Main SS1 iSPP Header.                     */
#include "IACPTRAN.h"            /* IACP Transport Header.                    */
#include "cfg_mgr.h"
#include "bt_mgr_priv.h"
#include "app_errors.h"
#include "cormem.h"
#include "pmem.h"

#define BT_AUTO_LAUNCH_APP

// Max. BT baud rate is BSP_CLOCK/16
#ifdef WHISTLE_CLOCK_96MHz
#define BT_COMM_BAUDRATE               (4000000)
#else
// BT baud rate for 24MHz core clock = 24/16 = 1.5MBit.
// Be cautious about changing this, as the UART dividers can't achieve all targets,
//   and this is the number we report to the BT chip (but not necessarily what we use).
//
//#define BT_COMM_BAUDRATE               (1500000)
// 1500000 didn't work out too well: Seeing a lot of these now:
// ISPP Raw Data Port: 1
//Lingo   :00Cmd  :02Len  :4BT:        00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F  BT: 0123456789ABCDEF
//
//BT:  ------------------------------------------------------
//
//BT:  00000 00 10 00 42                                      ...B
//
// Let's try a standard baud rate
#define BT_COMM_BAUDRATE               (921600)
#endif


#define DEFAULT_IO_CAPABILITY          (icNoInputNoOutput)  /* Denotes the    */
                                                         /* default I/O       */
                                                         /* Capability that is*/
                                                         /* used with Secure  */
                                                         /* Simple Pairing.   */

#define DEFAULT_MITM_PROTECTION                  (FALSE)  /* Denotes the       */
                                                         /* default value used*/
                                                         /* for Man in the    */
                                                         /* Middle (MITM)     */
                                                         /* protection used   */
                                                         /* with Secure Simple*/
                                                         /* Pairing.          */


#define VENDOR_ID_VALUE                            0x005E/* Bluetooth Company */
                                                         /* Identifier: TODO  */

   /* Set the number of milliseconds to delay before attempting to start*/
   /* the iAP process.                                                  */
#define IAP_START_DELAY_VALUE                      (2000)

   /* Set the number of milliseconds to delay before assuming the remote*/
   /* is not an IOS device.                              */
#define IAP_IS_NOT_IOS_DELAY_VALUE                 (1000)

   /* Following converts a Sniff Parameter in Milliseconds to frames.   */
#define MILLISECONDS_TO_BASEBAND_SLOTS(_x)   ((_x) / (0.625))

   /* session data receive events */
#define NUM_MESSAGES_RCV_Q 2 /* 1 for normal operation. 1 extra for NotifyDisconnect */
#define MSG_SIZE_RCV_Q 3 /* SessionID, DataLength, Buffer */

   /* session data send events */
#define NUM_MESSAGES_SND_Q 2 /* 1 for normal operation. 1 extra for NotifyDisconnect */
#define MSG_SIZE_SND_Q 3 /* SessionID, PacketID, Status */

	/* SDP Request complete events */
#define NUM_MESSAGES_SDPREQ_Q 4 /* 3 for normal operation. 1 extra for NotifyDisconnect */
#define MSG_SIZE_SDPREQ_Q 3 /* Port */

	/* Connection Test Responses */
#define NUM_MESSAGES_CT_Q 2 /* 1 for normal operation. 1 extra for NotifyDisconnect */
#define MSG_SIZE_CT_Q 2 /* LCID, Success/Fail value: 1=Success, 0=Failed */

#define TRANSMIT_BUFFER_EMPTY_EVENT 0x0001          /* ready to send more non-iOS data*/
#define DISCONNECT_COMPLETE_EVENT   0x0002          /* disconnect completed */

#define TEST_CONNECTION 0xfeedbeef /* callback parameter marker to indicate this is a connection test */

#define BT_VERBOSE(x)

typedef enum _test_connection_t
{
	test_connection_failed,
	test_connection_is_connected,
	test_connection_was_connected
} test_connection_t;

   /* Used to hold a BD_ADDR return from BD_ADDRToStr.                  */
typedef char BoardStr_t[15];

   /* Internal Variables to this Module (Remember that all variables    */
   /* declared static are initialized to 0 automatically by the         */
   /* compiler as part of standard C/C++).                              */

static unsigned int          BluetoothStackID;      /* Variable which holds the Handle */
                                                    /* of the opened Bluetooth Protocol*/
                                                    /* Stack.                          */

static int                   SerialPortID;          /* Variable which contains the     */
                                                    /* Handle of the most recent       */
                                                    /* SPP Port that was opened.       */

static unsigned int          iAPSessionID;          /* Variable which holds the        */
                                                    /* current iAP Session ID.         */

static unsigned int          iAPTimerID;            /* Variable used to timeout the    */
                                                    /* initiation of an iAP            */
                                                    /* authentication process.         */

static unsigned int          iAPTimerID2;           /* Variable used to see if the     */
                                                    /* remote is iOS or non.           */

static Word_t                Connection_Handle;     /* Holds the Connection Handle of  */
                                                    /* the most recent SPP Connection. */

static Boolean_t             Connected;             /* Variable which flags whether or */
                                                    /* not there is currently an active*/
                                                    /* connection.                     */

static Boolean_t             Connecting;            /* Variable which flags whether or */
                                                    /* not we are currently attempting */
                                                    /* to determine if a device can    */
                                                    /* can connect to us or not.       */

static Port_Operating_Mode_t CurrentOperatingMode;  /* Variable which holds the        */
                                                    /* current operating mode of the   */
                                                    /* current connection.             */

static int                   HCIEventCallbackHandle;/* Holds the handle of the         */
                                                    /* registered HCI Event Callback.  */

static DWord_t               SPPServerSDPHandle;    /* Variable used to hold the Serial*/
                                                    /* Port Service Record of the      */
                                                    /* Serial Port Server SDP Service  */
                                                    /* Record.                         */

static unsigned int          MaxPacketData;         /* Variable which holds the        */
                                                    /* largest available size (in      */
                                                    /* bytes of an iAP packet that     */
                                                    /* can be sent).                   */

static BD_ADDR_t             CurrentRemoteBD_ADDR;  /* Variable which holds the        */
                                                    /* current BD_ADDR of the device   */
                                                    /* which is currently pairing or   */
                                                    /* authenticating.                 */

static GAP_IO_Capability_t   IOCapability;          /* Variable which holds the        */
                                                    /* current I/O Capabilities that   */
                                                    /* are to be used for Secure Simple*/
                                                    /* Pairing.                        */

static Boolean_t             OOBSupport;            /* Variable which flags whether    */
                                                    /* or not Out of Band Secure Simple*/
                                                    /* Pairing exchange is supported.  */

static Boolean_t             MITMProtection;        /* Variable which flags whether or */
                                                    /* not Man in the Middle (MITM)    */
                                                    /* protection is to be requested   */
                                                    /* during a Secure Simple Pairing  */
                                                    /* procedure.                      */

static BD_ADDR_t             SelectedBD_ADDR;       /* Holds address of selected Device*/

static BoardStr_t            Callback_BoardStr;     /* Holds a BD_ADDR string in the   */
                                                    /* Callbacks.                      */

//static unsigned char         Buffer[ISPP_FRAME_SIZE_DEFAULT]; // Need this for Android. Maybe malloc instead?

static Boolean_t             pairMode = FALSE;   /* Variable telling the Request to */
                                                 /* Launch App to occur based on    */
                                                 /* whether the device is pairing.  */

static Boolean_t             maxPacketSuccess = FALSE; /* Used to confirm iOS pairing */
                                                       /* after launch app attempt    */

static Link_Key_t            CurrentLinkKey = {0};

static unsigned char         LaunchPacket[] =    /* Data sent for Application Launch           */
{
   0x00,0x01,0x00,           /* Prefix, 1 --> 2 for notify launch       */
   'c','o','m','.','w','h','i','s','t','l','e','.','W','h','i','s','t','l','e','A','p','p',0  /* Application ID */
};

/* Accessory Identification Information.                             */

   /* The following defines the static information that identifies the  */
   /* accessory and the features that it supports.                      */
   /* * NOTE * The data in this structure is custom for each accessory. */
   /*          The data that is contained in it will need to be updates */
   /*          for each project.                                        */
static BTPSCONST unsigned char FIDInfo[] =
{
   0x0C,                     /* Number of Tokens included.              */
   0x0C,                     /* Identity Token Length                   */
   0x00,0x00,                /* Identity Token IDs                      */
   0x01,                     /* Number of Supported Lingo's             */
   0x00,                     /* Supported Lingos.                       */
   0x00,0x00,0x00,0x02,      /* Device Options                          */
   0x00,0x00,0x02,0x00,      /* Device ID from Authentication Processor */
   0x0A,                     /* AccCap Token Length.                    */
   0x00,0x01,                /* AccCap Token IDs                        */
   0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x00,       /* AccCap Value       */
   0x14,                     /* Token Length                            */
   0x00,0x02,                /* Acc Info Token IDs                      */
   0x01,                     /* Acc Name                                */
   'A','c','t','i','v','i','t','y',' ','M','o','n','i','t','o','r',0,
   0x06,                     /* Token Length                            */
   0x00,0x02,                /* Acc Info Token IDs                      */
   0x04,0x01,0x00,0x00,      /* Acc Firmware Version                    */
   0x06,                     /* Token Length                            */
   0x00,0x02,                /* Acc Info Token IDs                      */
   0x05,0x01,0x00,0x00,      /* Acc Hardware Version                    */
   0x0B,                     /* Token Length                            */
   0x00,0x02,                /* Acc Info Token IDs                      */
   0x06,                     /* Manufacturer                            */
   'W','h','i','s','t','l','e',0,
   0x08,                     /* Token Length                            */
   0x00,0x02,                /* Acc Info Token IDs                      */
   0x07,                     /* Model Name                              */
   'W','0','1','A',0,
   0x0E,                     /* Token Length                            */
   0x00,0x02,                /* Acc Info Token IDs                      */
   0x08,                     /* Serial Number                           */
   /*************************** WARNING *********************************/
   /* The following is modified run-time by ModifiedFIDInfoAlloc().     */
   /* If you change any of the above lengths, causing this offset to    */
   /* change, you must update ModifiedFIDInfoAlloc(), curently 85       */
   0,0,0,0,0,0,0,0,0,0,0,    /* Force null terminated                   */
   /************************ END WARNING ********************************/
   0x05,                     /* Token Length                            */
   0x00,0x02,                /* Acc Info Token IDs                      */
   0x09,                     /* Max Incomming MTU size                  */
   0x00,0x80,                /* Set MTU to 128                          */
   0x07,                     /* Token Length                            */
   0x00,0x02,                /* Acc Info Token IDs                      */
   0x0C,0x00,0x00,0x00,0x00, /* Supported Products.                     */
   0x13,                     /* Token Length                            */
   0x00,0x04,                /* SDK Protocol Token                      */
   0x01,                     /* Supported Products.                     */
   'c','o','m','.','w','h','i','s','t','l','e','.','a','p','p',0,
   0x0D,                     /* Token Length                            */
   0x00,0x05,                /* Bundle Seed ID Pref Token               */
   'W','3','E','Y','C','2','M','S','2','B',0
} ;

static char *ModifiedFIDInfo = NULL;

   /* GAP Extended Inqiury Response Data.                               */
   /*    - Local Name                                                   */
   /*    - Apple Accessory UUID                                         */
   /*    - Service Class List (SDP, SPP, PNP)                           */
   /*    - Tx Power Level                                               */
static GAP_Extended_Inquiry_Response_Data_Entry_t GAPExtendedInquiryResponseDataEntries[] =
{
   { HCI_EXTENDED_INQUIRY_RESPONSE_DATA_TYPE_LOCAL_NAME_COMPLETE,                 248,          NULL                                        },
   { HCI_EXTENDED_INQUIRY_RESPONSE_DATA_TYPE_128_BIT_SERVICE_CLASS_UUID_COMPLETE, 16,           (Byte_t*)"\xFF\xCA\xCA\xDE\xAF\xDE\xCA\xDE\xDE\xFA\xCA\xDE\x00\x00\x00\x00" },
   { HCI_EXTENDED_INQUIRY_RESPONSE_DATA_TYPE_16_BIT_SERVICE_CLASS_UUID_COMPLETE,  6,            (Byte_t*)"\x00\x10\x01\x11\x00\x12"                                         },
   { HCI_EXTENDED_INQUIRY_RESPONSE_DATA_TYPE_TX_POWER_LEVEL,                      1,            0                                                                  }
} ;

   /* Q used to make SDPRequest() blocking */
static uint_32 SDPRequestQueue[sizeof(LWMSGQ_STRUCT)/sizeof(uint_32) + NUM_MESSAGES_SDPREQ_Q * MSG_SIZE_SDPREQ_Q];

/* Q used to make receive session data blocking */
static uint_32 ReceiveSessionEventQueue[sizeof(LWMSGQ_STRUCT)/sizeof(uint_32) + NUM_MESSAGES_RCV_Q * MSG_SIZE_RCV_Q];

/* Q used to make send session data blocking */
static uint_32 SendSessionEventQueue[sizeof(LWMSGQ_STRUCT)/sizeof(uint_32) + NUM_MESSAGES_SND_Q * MSG_SIZE_SND_Q];

/* Q used to wait for Connection Test Response */
static uint_32 CTResponseQueue[sizeof(LWMSGQ_STRUCT)/sizeof(uint_32) + NUM_MESSAGES_CT_Q * MSG_SIZE_CT_Q];

static unsigned int SDP_ServiceRequestID = 0;

static LWEVENT_STRUCT g_btmgrsh_lwevent;

static uint_8 *g_btmgrsh_iap_recv_buf_rd_ptr = NULL;
static uint_8 *g_btmgrsh_iap_recv_buf_end_ptr;
static uint_32 g_btmgr_spp_recv_remaining = 0;

/* Index of last paired device */
static int LastPairedIndex;

static boolean SSPDebugEnabled = FALSE;

   /* Internal function prototypes.                                     */
static void AddDeviceIDInformation(void);
static int PINCodeResponse(char *pin);
static int PassKeyResponse(char *key);
static int SniffMode(Word_t maxInterval, Word_t minInterval, Word_t attempt, Word_t timeout);
static int ExitSniffMode(void);
static int SDPRequest(BD_ADDR_t BD_ADDR, int *port, Port_Operating_Mode_t pom);

   /* BTPS Callback function prototypes.                                */
static void BTPSAPI BSC_Timer_Callback(unsigned int BluetoothStackID, unsigned int TimerID, unsigned long CallbackParameter);
static void BTPSAPI GAP_Event_Callback(unsigned int BluetoothStackID, GAP_Event_Data_t *GAP_Event_Data, unsigned long CallbackParameter);
static void BTPSAPI SDP_Event_Callback(unsigned int BluetoothStackID, unsigned int SDPRequestID, SDP_Response_Data_t *SDP_Response_Data, unsigned long CallbackParameter);
static void BTPSAPI ISPP_Event_Callback(unsigned int BluetoothStackID, ISPP_Event_Data_t *ISPP_Event_Data, unsigned long CallbackParameter);
static void BTPSAPI HCI_Event_Callback(unsigned int BluetoothStackID, HCI_Event_Data_t *HCI_Event_Data, unsigned long CallbackParameter);
static void BTPSAPI L2CAP_Event_Callback(unsigned int BluetoothStackID, L2CA_Event_Data_t *L2CAP_Event_Data, unsigned long CallbackParameter);
static void _chk_bt_id(void);


/* The following function is responsible for converting data of type */
/* BD_ADDR to a string.  The first parameter of this function is the */
/* BD_ADDR to be converted to a string.  The second parameter of this*/
/* function is a pointer to the string in which the converted BD_ADDR*/
/* is to be stored.                                                  */
void BD_ADDRToStr(BD_ADDR_t Board_Address, BoardStr_t BoardStr)
{
	BTPS_SprintF((char *)BoardStr, "0x%02X%02X%02X%02X%02X%02X", Board_Address.BD_ADDR5, Board_Address.BD_ADDR4, Board_Address.BD_ADDR3, Board_Address.BD_ADDR2, Board_Address.BD_ADDR1, Board_Address.BD_ADDR0);
}

/*
 *   Helper function to make sure BT Stack ID is OK.
 *     That way, we don't need to wassert(0) in a million places, and save some .text.
 */
static void _chk_bt_id(void)
{
    wassert(BluetoothStackID);
}

void *ModifiedFIDInfoAlloc(unsigned int *size_FIDInfo)
{
	if (!ModifiedFIDInfo)
	{
		ModifiedFIDInfo = walloc(sizeof(FIDInfo));

		memcpy(ModifiedFIDInfo, FIDInfo, sizeof(FIDInfo));
		
		// If you change the 10, change the place holder in FIDInfo too
		// If you change the offset of the serial number within FIDInfo,
		// you must change the 85.
		strncpy(ModifiedFIDInfo+85, g_st_cfg.fac_st.serial_number, 10);
	}
	
	*size_FIDInfo = sizeof(FIDInfo);

	return ModifiedFIDInfo;
}

void ModifiedFIDInfoFree(void)
{
	if (ModifiedFIDInfo)
	{
	    wfree(ModifiedFIDInfo);
		ModifiedFIDInfo = NULL;
	}
}

int GetLastPairedIndex(void)
{
	wassert(MAX_BT_REMOTES > LastPairedIndex && 0 <= LastPairedIndex);
    return LastPairedIndex;
}

int whistle_get_BluetoothStackID(void)
{
    return BluetoothStackID;
}

/* Empty all the Q's so there are no stale messages */
void EmptyQueuesClearEvents(void)
{
	_mqx_uint mqx_rc;
    _mqx_max_type sdpmsg[MSG_SIZE_SDPREQ_Q];  
    _mqx_max_type rcvmsg[MSG_SIZE_RCV_Q];  
    _mqx_max_type sndmsg[MSG_SIZE_SND_Q];  
    _mqx_max_type QRNmsg[MSG_SIZE_CT_Q];  
	
	mqx_rc = MQX_OK;
	while (LWMSGQ_EMPTY != mqx_rc)
	{
		mqx_rc = _lwmsgq_receive((pointer)SDPRequestQueue, sdpmsg, 0, 0, 0);
		wassert(MQX_OK == mqx_rc || LWMSGQ_EMPTY == mqx_rc);
	}
	
	mqx_rc = MQX_OK;
	while (LWMSGQ_EMPTY != mqx_rc)
	{
		mqx_rc = _lwmsgq_receive((pointer)ReceiveSessionEventQueue, rcvmsg, 0, 0, 0);
		wassert(MQX_OK == mqx_rc || LWMSGQ_EMPTY == mqx_rc);
	}
	
	mqx_rc = MQX_OK;
	while (LWMSGQ_EMPTY != mqx_rc)
	{
	    mqx_rc = _lwmsgq_receive((pointer)SendSessionEventQueue, sndmsg, 0, 0, 0);
		wassert(MQX_OK == mqx_rc || LWMSGQ_EMPTY == mqx_rc);
	}

	mqx_rc = MQX_OK;
	while (LWMSGQ_EMPTY != mqx_rc)
	{
	    mqx_rc = _lwmsgq_receive((pointer)CTResponseQueue, QRNmsg, 0, 0, 0);
		wassert(MQX_OK == mqx_rc || LWMSGQ_EMPTY == mqx_rc);
	}
	
	_lwevent_clear(&g_btmgrsh_lwevent, 0xffffffff);
}

/* Notify any potential waiters on any of the Qs that the link has been disconnected so waiters can break out of waits */
void NotifyDisconnect(void)
{
    _mqx_max_type sdpmsg[MSG_SIZE_SDPREQ_Q];  
    _mqx_max_type rcvmsg[MSG_SIZE_RCV_Q];  
    _mqx_max_type sndmsg[MSG_SIZE_SND_Q];  
    _mqx_max_type QRNmsg[MSG_SIZE_CT_Q];  

    sdpmsg[0] = -1; /* force bad port number */
	_lwmsgq_send((pointer)SDPRequestQueue, sdpmsg, 0);
	
	rcvmsg[1] = -1; /* force negative number of bytes received */
	_lwmsgq_send((pointer)ReceiveSessionEventQueue, rcvmsg, 0);
	
	sndmsg[2] = PACKET_SEND_STATUS_FAILED; /* force fail Status */
	_lwmsgq_send((pointer)SendSessionEventQueue, sndmsg, 0);

	QRNmsg[0] = -1; /* force fail Status */
	_lwmsgq_send((pointer)CTResponseQueue, QRNmsg, 0);
}

void SaveConfig(void)
{
    memcpy(&g_dy_cfg.bt_dy.LinkKeyInfo[LastPairedIndex].BD_ADDR, &CurrentRemoteBD_ADDR, sizeof(BD_ADDR_t));
    
    ASSIGN_BD_ADDR(CurrentRemoteBD_ADDR, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);

    memcpy(&g_dy_cfg.bt_dy.LinkKeyInfo[LastPairedIndex].LinkKey, &CurrentLinkKey, sizeof(Link_Key_t));
    g_dy_cfg.bt_dy.LinkKeyInfo[LastPairedIndex].pom = CurrentOperatingMode;
    CFGMGR_flush( CFG_FLUSH_DYNAMIC );
}

/* The following function is a utility function that exists to add   */
/* the Device Identification (DID) Service Record to the SDP         */
/* Database.                                                         */
static void AddDeviceIDInformation(void)
{
    SDWord_t            SDPRecordHandle;
    SDP_UUID_Entry_t   UUIDEntry;
    SDP_Data_Element_t SDPDataElement;
    
    /* Create a Service Record with the PNP UUID.                        */
    UUIDEntry.SDP_Data_Element_Type = deUUID_16;
    SDP_ASSIGN_PNP_INFORMATION_UUID_16(UUIDEntry.UUID_Value.UUID_16);
    
    /* Save the Handle to this Service Record.                        */
    SDPRecordHandle = SDP_Create_Service_Record(BluetoothStackID, 1, &UUIDEntry);
    
    wassert(SDPRecordHandle > 0);
    
    /* Add the Profile Version Attribute.                             */
    SDPDataElement.SDP_Data_Element_Type                  = deUnsignedInteger2Bytes;
    SDPDataElement.SDP_Data_Element_Length                = WORD_SIZE;
    SDPDataElement.SDP_Data_Element.UnsignedInteger2Bytes = 0x0103;
    
    wassert(!SDP_Add_Attribute(BluetoothStackID, SDPRecordHandle, SDP_ATTRIBUTE_ID_DID_SPECIFICATION_ID, &SDPDataElement));
    
    /* Add the Vendor ID Attribute.                                */
    SDPDataElement.SDP_Data_Element_Type                  = deUnsignedInteger2Bytes;
    SDPDataElement.SDP_Data_Element_Length                = WORD_SIZE;
    SDPDataElement.SDP_Data_Element.UnsignedInteger2Bytes = VENDOR_ID_VALUE;
    
    wassert(!SDP_Add_Attribute(BluetoothStackID, SDPRecordHandle, SDP_ATTRIBUTE_ID_DID_VENDOR_ID, &SDPDataElement));
    
    /* Add the Product ID Attribute.                            */
    SDPDataElement.SDP_Data_Element_Type                  = deUnsignedInteger2Bytes;
    SDPDataElement.SDP_Data_Element_Length                = WORD_SIZE;
    SDPDataElement.SDP_Data_Element.UnsignedInteger2Bytes = 0x0001;
    
    wassert(!SDP_Add_Attribute(BluetoothStackID, SDPRecordHandle, SDP_ATTRIBUTE_ID_DID_PRODUCT_ID, &SDPDataElement));
    
    /* Add the Product Version ID Attribute.                 */
    SDPDataElement.SDP_Data_Element_Type                  = deUnsignedInteger2Bytes;
    SDPDataElement.SDP_Data_Element_Length                = WORD_SIZE;
    SDPDataElement.SDP_Data_Element.UnsignedInteger2Bytes = 0x0112;
    
    wassert(!SDP_Add_Attribute(BluetoothStackID, SDPRecordHandle, SDP_ATTRIBUTE_ID_DID_VERSION, &SDPDataElement));
    
    /* Add the Primary Record ID Attribute.               */
    SDPDataElement.SDP_Data_Element_Type    = deBoolean;
    SDPDataElement.SDP_Data_Element_Length  = BYTE_SIZE;
    SDPDataElement.SDP_Data_Element.Boolean = TRUE;
    
    wassert(!SDP_Add_Attribute(BluetoothStackID, SDPRecordHandle, SDP_ATTRIBUTE_ID_DID_PRIMARY_RECORD, &SDPDataElement));
    
    /* Add the Primary Record ID Attribute.            */
    SDPDataElement.SDP_Data_Element_Type                  = deUnsignedInteger2Bytes;
    SDPDataElement.SDP_Data_Element_Length                = WORD_SIZE;
    SDPDataElement.SDP_Data_Element.UnsignedInteger2Bytes = 0x0001;
    
    wassert(!SDP_Add_Attribute(BluetoothStackID, SDPRecordHandle, SDP_ATTRIBUTE_ID_DID_VENDOR_ID_SOURCE, &SDPDataElement));
}

/*
 *   Return non-zero if MFI comm is OK.
 */
uint8_t BTMGR_mfi_comm_ok(void)
{
    unsigned char byte = 0;
    unsigned char status;
    
    IACPTR_Read(0x2, 1, &byte, &status);   // 0x2 is command for MFI MAJOR version I believe.
    
    //corona_print("\n*MFI*\trc: %i  byte %x  stat %x\n\n", rc, byte, status);
    
    if(byte != 0x2)   // we expect 2 here for MAJOR VERSION
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

/*
 *   Return 0 if BT comm not OK, non-zero otherwise.
 *   
 *   Just set the pairing to whatever state we already are.
 *     This'll ensure we can at least still talk to the BT chip.
 */
uint8_t BTMGR_uart_comm_ok(void)
{
    int result;
    
    Byte_t statusResult;
    Byte_t hciVerResult;
    Byte_t lmpVerResult;
    
    Word_t hciRevResult;
    Word_t mfgNamResult;
    Word_t lmpSubResult;

    result = HCI_Read_Local_Version_Information(whistle_get_BluetoothStackID(), 
                                                &statusResult, 
                                                &hciVerResult, 
                                                &hciRevResult, 
                                                &lmpVerResult, 
                                                &mfgNamResult, 
                                                &lmpSubResult);
    
#if 0
    corona_print("\n*BT*\n\nRESULT\t%x\n",      result);
    corona_print("STAT\t%x\n",      statusResult);
    corona_print("VER\t%x\n",         hciVerResult);
    corona_print("REV:\t%x\n",         hciRevResult);
    corona_print("LMP VER:\t%x\n",     lmpVerResult);
    corona_print("MFG NAME:\t%x\n",    mfgNamResult);
    corona_print("LMP SUB:\t%x\n",     lmpSubResult);
#endif
    
    /*
     *   Check for errors.
     */
    if(result)
    {
        return 0;
    }
    else if(statusResult)
    {
        return 0;
    }
    else if(0x6 != hciVerResult)
    {
        return 0;
    }
    
    /*
     *   EA:  I saw these results on my P3, not checking for all of them due to overkill.

            RESULT:     0x0
            STATUS:     0x0
            VER:        0x6
            REV:        0x0
            LMP VER:    0x6
            MFG NAME:   0xd
            LMP SUB:    0x1b4d
     */
    
    
    // MFI is OK in my book.
    return 1;
}

/*
 *   Check that the BT hardware is good.  If it is not, log an error and go solid yellow.
 *   
 *   TODO:  Maybe in the future, if there is a failure, we retry in a loop or something.
 */
void BT_verify_hw(void)
{
    if( 0 == BTMGR_uart_comm_ok() )
    {
        PWRMGR_Fail( ERR_BT_HARDWARE_FAIL, "BT");
    }
    
    if( 0 == BTMGR_mfi_comm_ok() )
    {
        PWRMGR_Fail( ERR_MFI_HARDWARE_FAIL, "MFI");
    }
}

/* The following function is responsible for opening the SS1         */
/* Bluetooth Protocol Stack.  This function accepts a pre-populated  */
/* HCI Driver Information structure that contains the HCI Driver     */
/* Transport Information.  */
int OpenStack(void)
{
    char                                  BluetoothAddress[16];
    Byte_t                                Status;
    BD_ADDR_t                             BD_ADDR;
    Extended_Inquiry_Response_Data_t     *ExtendedInquiryResponseData;
    Class_of_Device_t                     ClassOfDevice;
    L2CA_Link_Connect_Params_t            L2CA_Link_Connect_Params;
    GAP_Extended_Inquiry_Response_Data_t  GAPExtendedInquiryResponseData;
    BTPS_Initialization_t                 BTPS_Initialization;
    HCI_DriverInformation_t               HCI_DriverInformation;
    ISPP_Configuration_Params_t           configParam;
    
    Connected               = FALSE;
    Connecting              = FALSE;
    iAPTimerID              = 0;
    iAPTimerID2             = 0;
    iAPSessionID            = 0;

    /* Configure the UART Parameters.                                    */
    HCI_DRIVER_SET_COMM_INFORMATION(&HCI_DriverInformation, 1, BT_COMM_BAUDRATE, cpUART);
    HCI_DriverInformation.DriverInformation.COMMDriverInformation.InitializationDelay = 400; //100;  // Increase to 400, give more time BT chip to boot up after HCI_RTS goes low.

    /* First check to see if the Stack has already been opened.          */
    if(BluetoothStackID)
    {
        goto DONE;
    }
        
    /* Initialize BTPSKNRL.                                        */
    BTPS_Initialization.MessageOutputCallback = HAL_ConsoleWrite;
    BTPS_Init((void *)&BTPS_Initialization);
   
    /* Initialize the Stack                                        */
    BluetoothStackID = BSC_Initialize(&HCI_DriverInformation, 0); 
    
    corona_print("BT ID %d\n", BluetoothStackID);
        
    /* The Stack was initialized successfully, inform the user  */
    /* and set the return value of the initialization function  */
    /* to the Bluetooth Stack ID.                               */
    if ((int)BluetoothStackID <= 0)
    {
    	goto DONE;
    }
    
    /* Initialize the default Secure Simple Pairing parameters. */
    IOCapability     = DEFAULT_IO_CAPABILITY;
    OOBSupport       = FALSE;
    MITMProtection   = DEFAULT_MITM_PROTECTION;
    
    /* Let's output the Bluetooth Device Address so that the    */
    /* user knows what the Device Address is.                   */
    if(!GAP_Query_Local_BD_ADDR(BluetoothStackID, &BD_ADDR))
    {
        BD_ADDRToStr(BD_ADDR, BluetoothAddress);
        
        corona_print("addr %s\n", BluetoothAddress);
    }
    
    /* Set the class of device.                                 */
    /* * NOTE * Apple is very specific about the Class of Device*/
    /*          values that they will respond to/support.       */
    ASSIGN_CLASS_OF_DEVICE(ClassOfDevice, 0, 0, 0);
    
    SET_MAJOR_SERVICE_CLASS(ClassOfDevice, HCI_LMP_CLASS_OF_DEVICE_SERVICE_CLASS_AUDIO_BIT);
    SET_MINOR_DEVICE_CLASS(ClassOfDevice, HCI_LMP_CLASS_OF_DEVICE_MINOR_DEVICE_CLASS_AUDIO_VIDEO_MICROPHONE);
    
    SET_MAJOR_DEVICE_CLASS(ClassOfDevice, HCI_LMP_CLASS_OF_DEVICE_MAJOR_DEVICE_CLASS_AUDIO_VIDEO);
    
    GAP_Set_Class_Of_Device(BluetoothStackID, ClassOfDevice);
    
    /* Write the local name.                                    */
    wassert(0 == SetLocalName(g_dy_cfg.bt_dy.deviceName));
    
    /* Write the Inquiry Response Data.                         */
    if((ExtendedInquiryResponseData = (Extended_Inquiry_Response_Data_t *)BTPS_AllocateMemory(sizeof(Extended_Inquiry_Response_Data_t))) != NULL)
    {
        GAPExtendedInquiryResponseData.Number_Data_Entries = sizeof(GAPExtendedInquiryResponseDataEntries)/sizeof(GAP_Extended_Inquiry_Response_Data_Entry_t);
        GAPExtendedInquiryResponseData.Data_Entries        = GAPExtendedInquiryResponseDataEntries;
        
        /* Format the EIR data and write it.                        */
        if(GAP_Convert_Extended_Inquiry_Response_Data(&(GAPExtendedInquiryResponseData), ExtendedInquiryResponseData))
            wassert(0 == GAP_Write_Extended_Inquiry_Information(BluetoothStackID, HCI_EXTENDED_INQUIRY_RESPONSE_FEC_REQUIRED, ExtendedInquiryResponseData));
        
        BTPS_FreeMemory(ExtendedInquiryResponseData);
    }
    
    /* Go ahead and allow Master/Slave Role Switch.             */
    L2CA_Link_Connect_Params.L2CA_Link_Connect_Request_Config  = cqAllowRoleSwitch;
    L2CA_Link_Connect_Params.L2CA_Link_Connect_Response_Config = csMaintainCurrentRole;
    
    L2CA_Set_Link_Connection_Configuration(BluetoothStackID, &L2CA_Link_Connect_Params);
    
    if(HCI_Command_Supported(BluetoothStackID, HCI_SUPPORTED_COMMAND_WRITE_DEFAULT_LINK_POLICY_BIT_NUMBER) > 0)
        HCI_Write_Default_Link_Policy_Settings(BluetoothStackID, (HCI_LINK_POLICY_SETTINGS_ENABLE_MASTER_SLAVE_SWITCH|HCI_LINK_POLICY_SETTINGS_ENABLE_SNIFF_MODE), &Status);
    
    if(SSPDebugEnabled)
    {
    	wassert(0 == HCI_Write_Simple_Pairing_Debug_Mode(BluetoothStackID, 1, &Status));
    	wassert(0 == Status);
    }
    
    /* Finally, let's attempt to initialize the iSPP module.    */
    wassert(!ISPP_Initialize(BluetoothStackID, NULL));
    
    // BM: adding this line for debug of garbage bytes on FTDI during BT init
    BT_VERBOSE(corona_print("ISPP_Initialize() done\n");)
    
    /* Add the Device ID Information to the SDP Database.    */
    AddDeviceIDInformation();
    
    /* The stack was opened successfully.  Now set some defaults.     */
    HCIEventCallbackHandle = HCI_Register_Event_Callback(BluetoothStackID, HCI_Event_Callback, (unsigned long)NULL);
    
    /* Config iSPP buffer sizes */ 
    ISPP_Get_Configuration_Parameters(BluetoothStackID, &configParam);
    BT_VERBOSE(corona_print("ISPP\nrx buf sz: %d\ninit cfg rx buf sz: %d\n", ISPP_BUFFER_SIZE_DEFAULT_RECEIVE, configParam.ReceiveBufferSize);)
    configParam.ReceiveBufferSize = ISPP_BUFFER_SIZE_DEFAULT_RECEIVE;
    configParam.PacketTimeout = 10000;
    ISPP_Set_Configuration_Parameters(BluetoothStackID, &configParam);
    BT_VERBOSE(corona_print("ISPP new cfg rx buf sz: %d\n", configParam.ReceiveBufferSize);)
    
    /* Attempt to register a HCI Event Callback.             */
    wassert(0 < HCIEventCallbackHandle);
    
    /* Attempt to make us pairable with SSP */
    wassert(!SetPairable());
    
    /* init Q used to make SDPRequest() blocking */
    _lwmsgq_init((pointer)SDPRequestQueue, NUM_MESSAGES_SDPREQ_Q, MSG_SIZE_SDPREQ_Q);

    /* init Q used to make receive session data blocking */
    _lwmsgq_init((pointer)ReceiveSessionEventQueue, NUM_MESSAGES_RCV_Q, MSG_SIZE_RCV_Q);

    /* init Q used to make send session data blocking */
    _lwmsgq_init((pointer)SendSessionEventQueue, NUM_MESSAGES_SND_Q, MSG_SIZE_SND_Q);
    
    /* init Q used to receive connection test responses */
    _lwmsgq_init((pointer)CTResponseQueue, NUM_MESSAGES_CT_Q, MSG_SIZE_CT_Q);
    
    /* init event to wait for disconnect complete */
	_lwevent_create(&g_btmgrsh_lwevent, LWEVENT_AUTO_CLEAR);
	
DONE:
	return (int)BluetoothStackID;
}

/* The following function is responsible for closing the SS1         */
/* Bluetooth Protocol Stack.  This function requires that the        */
/* Bluetooth Protocol stack previously have been initialized via the */
/* OpenStack() function.       */
void CloseStack(void)
{
    /* First check to see if the Stack has been opened.                  */
    if(0 == BluetoothStackID)
    {
        return;
    }
    
    /* Simply close the Stack                                         */
    BSC_Shutdown(BluetoothStackID);
    
    /* Free BTPSKRNL allocated memory.                                */
    BTPS_DeInit();
    
    /* Check if we attempted, but failed to open */
    if (0 > (int)BluetoothStackID)
    {
    	goto DONE;
    }
    
    EmptyQueuesClearEvents();
    
    ModifiedFIDInfoFree();
    
    _lwevent_destroy(&g_btmgrsh_lwevent);
        
DONE:
    BT_VERBOSE(corona_print("Stack Shutdown\n");)
    
    /* Flag that the Stack is no longer initialized.                  */
    BluetoothStackID = 0;
}

/* The following function is responsible for placing the Local       */
/* Bluetooth Device into General Discoverablity Mode.  Once in this  */
/* mode the Device will respond to Inquiry Scans from other Bluetooth*/
/* Devices.  This function requires that a valid Bluetooth Stack ID  */
/* exists before running.     */
int SetDisc(void)
{
    /* First, check that a valid Bluetooth Stack ID exists.              */
    _chk_bt_id();
    
    /* A semi-valid Bluetooth Stack ID exists, now attempt to set the */
    /* attached Devices Discoverablity Mode to General.               */
    return GAP_Set_Discoverability_Mode(BluetoothStackID, dmGeneralDiscoverableMode, 0);
}

/* The following function is responsible for placing the Local       */
/* Bluetooth Device into Connectable Mode.  Once in this mode the    */
/* Device will respond to Page Scans from other Bluetooth Devices.   */
/* This function requires that a valid Bluetooth Stack ID exists     */
/* before running. */
int SetConnect(void)
{
    /* First, check that a valid Bluetooth Stack ID exists.              */
    _chk_bt_id();
    
    /* Attempt to set the attached Device to be Connectable.          */
    return GAP_Set_Connectability_Mode(BluetoothStackID, cmConnectableMode);
}

/* The following function is responsible for placing the local       */
/* Bluetooth device into Pairable mode.  Once in this mode the device*/
/* will response to pairing requests from other Bluetooth devices.   */
int SetPairable(void)
{
    int Result;
    
    /* First, check that a valid Bluetooth Stack ID exists.              */
    _chk_bt_id();
    
    Result = GAP_Set_Pairability_Mode(BluetoothStackID, pmPairableMode_EnableSecureSimplePairing);
    
    /* Attempt to set the attached device to be pairable.             */
    if (Result)
    {
        return Result;
    }
    
    /* Next, check the return value of the GAP Set Pairability mode   */
    /* command for successful execution.                              */
    /* The device has been set to pairable mode, now register an   */
    /* Authentication Callback to handle the Authentication events */
    /* if required.                                                */
    GAP_Un_Register_Remote_Authentication(BluetoothStackID);
    return GAP_Register_Remote_Authentication(BluetoothStackID, GAP_Event_Callback, (unsigned long)0);
}

/* The following function is a utility function that exists to delete*/
/* the specified Link Key from the Local Bluetooth Device.  If a NULL*/
/* Bluetooth Device Address is specified, then all Link Keys will be */
/* deleted.                                                          */
void DeleteLinkKey(BD_ADDR_t BD_ADDR)
{
    int       Result;
    Byte_t    Status_Result;
    Word_t    Num_Keys_Deleted = 0;
    
    HCI_Delete_Stored_Link_Key(BluetoothStackID, BD_ADDR, TRUE, &Status_Result, &Num_Keys_Deleted);
    
    /* Any stored link keys for the specified address (or all) have been */
    /* deleted from the chip.  Now, let's make sure that our stored Link */
    /* Key Array is in sync with these changes.                          */
    
    if(COMPARE_NULL_BD_ADDR(BD_ADDR))
    {
        BTPS_MemInitialize(g_dy_cfg.bt_dy.LinkKeyInfo, 0, sizeof(g_dy_cfg.bt_dy.LinkKeyInfo));
        CFGMGR_flush( CFG_FLUSH_DYNAMIC );
    }
    else
    {
        /* Individual Link Key.  Go ahead and see if know about the entry */
        /* in the list.                                                   */
        for(Result=0;(Result<sizeof(g_dy_cfg.bt_dy.LinkKeyInfo)/sizeof(LinkKeyInfo_t));Result++)
        {
            if(COMPARE_BD_ADDR(BD_ADDR, g_dy_cfg.bt_dy.LinkKeyInfo[Result].BD_ADDR))
            {
                ASSIGN_BD_ADDR(g_dy_cfg.bt_dy.LinkKeyInfo[Result].BD_ADDR, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
                CFGMGR_flush( CFG_FLUSH_DYNAMIC );
                
                break;
            }
        }
    }
}

/* The following function is responsible for setting the             */
/* Discoverability Mode of the local device.  */
int SetDiscoverabilityMode(int mode)
{
    GAP_Discoverability_Mode_t DiscoverabilityMode;
    
    /* First, check that valid Bluetooth Stack ID exists.                */
    _chk_bt_id();
    
    /* Parameters appear to be valid, map the specified parameters */
    /* into the API specific parameters.                           */
    if(mode == 1)
        DiscoverabilityMode = dmLimitedDiscoverableMode;
    else
    {
        if(mode == 2)
            DiscoverabilityMode = dmGeneralDiscoverableMode;
        else
            DiscoverabilityMode = dmNonDiscoverableMode;
    }
    
    /* Parameters mapped, now set the Discoverability Mode.        */
    return GAP_Set_Discoverability_Mode(BluetoothStackID, DiscoverabilityMode, (DiscoverabilityMode == dmLimitedDiscoverableMode)?60:0);
}

/* The following function is responsible for setting the             */
/* Connectability Mode of the local device.  */
int SetConnectabilityMode(int mode)
{
    GAP_Connectability_Mode_t ConnectableMode;
    
    /* First, check that valid Bluetooth Stack ID exists.                */
    _chk_bt_id();
    
    /* Parameters appear to be valid, map the specified parameters */
    /* into the API specific parameters.                           */
    if(mode == 0)
        ConnectableMode = cmNonConnectableMode;
    else
        ConnectableMode = cmConnectableMode;
    
    /* Parameters mapped, now set the Connectabilty Mode.          */
    return GAP_Set_Connectability_Mode(BluetoothStackID, ConnectableMode);
}

/* The following function is responsible for setting the Pairability */
/* Mode of the local device.          */
int SetPairabilityMode(int mode)
{
    GAP_Pairability_Mode_t  PairabilityMode;
    
    /* First, check that valid Bluetooth Stack ID exists.                */
    _chk_bt_id();
    
    /* Parameters appear to be valid, map the specified parameters */
    /* into the API specific parameters.                           */
    if(mode == 0)
    {
        PairabilityMode = pmNonPairableMode;
    }
    else
    {
        if(mode == 1)
        {
            PairabilityMode = pmPairableMode;
        }
        else
        {
            PairabilityMode = pmPairableMode_EnableSecureSimplePairing;
        }
    }
    
    /* Parameters mapped, now set the Pairability Mode.            */
    return GAP_Set_Pairability_Mode(BluetoothStackID, PairabilityMode);
}

/* The following function is responsible for issuing a GAP           */
/* Authentication Response with a PIN Code value specified via the   */
/* input parameter.                     */
static int PINCodeResponse(char *pin)
{
    int                              Result;
    PIN_Code_t                       PINCode;
    GAP_Authentication_Information_t GAP_Authentication_Information;
    
    /* First, check that valid Bluetooth Stack ID exists.                */
    _chk_bt_id();
    
    /* First, check to see if there is an on-going Pairing operation  */
    /* active.                                                        */
    if(COMPARE_NULL_BD_ADDR(CurrentRemoteBD_ADDR))
    {
        return(-99);
    }
    
    /* convert the  */
    /* input parameter into a PIN Code.                         */
    
    /* Initialize the PIN code.                                 */
    ASSIGN_PIN_CODE(PINCode, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    
    BTPS_MemCopy(&PINCode, pin, BTPS_StringLength(pin));
    
    /* Populate the response structure.                         */
    GAP_Authentication_Information.GAP_Authentication_Type      = atPINCode;
    GAP_Authentication_Information.Authentication_Data_Length   = (Byte_t)(BTPS_StringLength(pin));
    GAP_Authentication_Information.Authentication_Data.PIN_Code = PINCode;
    
    /* Submit the Authentication Response.                      */
    Result = GAP_Authentication_Response(BluetoothStackID, CurrentRemoteBD_ADDR, &GAP_Authentication_Information);
        
    /* Flag that there is no longer a current Authentication    */
    /* procedure in progress.                                   */
    ASSIGN_BD_ADDR(CurrentRemoteBD_ADDR, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);

    return(Result);
}

/* The following function is responsible for issuing a GAP           */
/* Authentication Response with a Pass Key value specified via the   */
/* input parameter.                     */
static int PassKeyResponse(char *key)
{
    int                              Result;
    GAP_Authentication_Information_t GAP_Authentication_Information;
    
    /* First, check that valid Bluetooth Stack ID exists.                */
    _chk_bt_id();
    
    /* First, check to see if there is an on-going Pairing operation  */
    /* active.                                                        */
    if(COMPARE_NULL_BD_ADDR(CurrentRemoteBD_ADDR))
    {
        return(-99);
    }
    
    /* Make sure that all of the parameters required for this      */
    /* function appear to be at least semi-valid.                  */
    if (BTPS_StringLength(key) > GAP_PASSKEY_MAXIMUM_NUMBER_OF_DIGITS)
    {
        return (-98);
    }
    
    /* Parameters appear to be valid, go ahead and populate the */
    /* response structure.                                      */
    GAP_Authentication_Information.GAP_Authentication_Type     = atPassKey;
    GAP_Authentication_Information.Authentication_Data_Length  = (Byte_t)(sizeof(DWord_t));
    GAP_Authentication_Information.Authentication_Data.Passkey = (DWord_t)strtoul(key, NULL, 0);
    
    /* Submit the Authentication Response.                      */
    Result = GAP_Authentication_Response(BluetoothStackID, CurrentRemoteBD_ADDR, &GAP_Authentication_Information);
        
    /* Flag that there is no longer a current Authentication    */
    /* procedure in progress.                                   */
    ASSIGN_BD_ADDR(CurrentRemoteBD_ADDR, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
    
    return(Result);
}

/* The following function is responsible for querying the Bluetooth  */
/* Device Address of the local Bluetooth Device.                     */
int GetLocalAddress(BD_ADDR_t *BD_ADDR)
{
    /* First, check that valid Bluetooth Stack ID exists.                */
    _chk_bt_id();
    
    /* Attempt to submit the command.                                 */
    return GAP_Query_Local_BD_ADDR(BluetoothStackID, BD_ADDR);
}

/* The following function is responsible for setting the name of the */
/* local Bluetooth Device to a specified name.  */
int SetLocalName(char *localName)
{
    int                                   Result;
    Extended_Inquiry_Response_Data_t     *ExtendedInquiryResponseData;
    GAP_Extended_Inquiry_Response_Data_t  GAPExtendedInquiryResponseData;
    static char Name[MAX_NAME_LENGTH];
    
    /* First, check that valid Bluetooth Stack ID exists.                */
    _chk_bt_id();
   
    /* Attempt to submit the command.                              */
    Result = GAP_Set_Local_Device_Name(BluetoothStackID, localName);
    
    /* Check the return value of the submitted command for success.*/
    if(Result)
    {
        return(Result);
    }
    
    /* Display a message indicating that the Device Name was    */
    /* successfully submitted.                                  */
    corona_print("Local Dev Name: %s\n", localName);
    
    /* Since we changed the local name, go ahead and update the */
    /* EIR data as well.                                        */
    if((ExtendedInquiryResponseData = (Extended_Inquiry_Response_Data_t *)BTPS_AllocateMemory(sizeof(Extended_Inquiry_Response_Data_t))) != NULL)
    {
        /* Fix up the EIR data to reflect the new name.             */
        // GAPExtendedInquiryResponseDataEntries[0].Data_Length = BTPS_StringLength(localName) + 1; // this is a bug. length should be 248
    	strncpy(Name, localName, MAX_NAME_LENGTH);
        GAPExtendedInquiryResponseDataEntries[0].Data_Buffer = (Byte_t *)(Name);
        
        GAPExtendedInquiryResponseData.Number_Data_Entries = sizeof(GAPExtendedInquiryResponseDataEntries)/sizeof(GAP_Extended_Inquiry_Response_Data_Entry_t);
        GAPExtendedInquiryResponseData.Data_Entries        = GAPExtendedInquiryResponseDataEntries;
        
        /* Format the EIR data and write it.                        */
        if(GAP_Convert_Extended_Inquiry_Response_Data(&GAPExtendedInquiryResponseData, ExtendedInquiryResponseData))
            Result = GAP_Write_Extended_Inquiry_Information(BluetoothStackID, HCI_EXTENDED_INQUIRY_RESPONSE_FEC_REQUIRED, ExtendedInquiryResponseData);
        
        BTPS_FreeMemory(ExtendedInquiryResponseData);
    }
    else
    {
    	Result = -99;
    }
    
    /* Flag success to the caller.                              */
    return(Result);
}

/* The following function is responsible for querying the Bluetooth  */
/* Device Name of the local Bluetooth Device.  This function returns */
/* zero on successful execution and a negative value on all errors.  */
int GetLocalName(char *localName)
{
    /* First, check that valid Bluetooth Stack ID exists.                */
    _chk_bt_id();
    
    /* Attempt to submit the command.                              */
    return GAP_Query_Local_Device_Name(BluetoothStackID, 257, (char *)localName);
}

/* The following function is responsible for putting a specified     */
/* connection into HCI Sniff Mode with passed in parameters.         */
static int SniffMode(Word_t maxInterval, Word_t minInterval, Word_t attempt, Word_t timeout)
{
    Word_t Sniff_Max_Interval;
    Word_t Sniff_Min_Interval;
    Word_t Sniff_Attempt;
    Word_t Sniff_Timeout;
    Byte_t Status;
    
    /* First, check that valid Bluetooth Stack ID exists.                */
    _chk_bt_id();
    
    Sniff_Max_Interval   = (Word_t)MILLISECONDS_TO_BASEBAND_SLOTS(maxInterval);
    Sniff_Min_Interval   = (Word_t)MILLISECONDS_TO_BASEBAND_SLOTS(minInterval);
    Sniff_Attempt        = attempt;
    Sniff_Timeout        = timeout;
    
    /* Make sure the Sniff Mode parameters seem semi valid.        */
    wassert((Sniff_Attempt) && (Sniff_Max_Interval) && (Sniff_Min_Interval) && (Sniff_Min_Interval < Sniff_Max_Interval));
    
    /* Make sure the connection handle is valid.                */
    if(!Connection_Handle)
    {
        return(-99);
    }
    
    /* Now that we have the connection try and go to Sniff.  */
    return HCI_Sniff_Mode(BluetoothStackID, Connection_Handle, Sniff_Max_Interval, Sniff_Min_Interval, Sniff_Attempt, Sniff_Timeout, &Status);
}

/* The following function is responsible for attempting to Exit      */
/* Sniff Mode for a specified connection.                            */
static int ExitSniffMode(void)
{
    Byte_t Status;
    
    /* First, check that valid Bluetooth Stack ID exists.                */
    _chk_bt_id();
        
    /* Make sure the connection handle is valid.                */
    if(!Connection_Handle)
    {
        return (-99);
    }
    
    /* Attempt to Exit Sniff Mode for the Specified Device.        */
    return HCI_Exit_Sniff_Mode(BluetoothStackID, Connection_Handle, &Status);
}

/* The following function is responsible for opening a Serial Port   */
/* Server on the Local Device.  This function opens the Serial Port  */
/* Server on the specified RFCOMM Channel.                           */
int OpenServer(unsigned int port)
{
    int  ret_val;
    char *ServiceName;
    int tempPort;
    
    /* First check to see if a valid Bluetooth Stack ID exists.          */
    _chk_bt_id();

    /* Make sure that there is not already a Serial Port Server open. */
    if(SerialPortID)
    {
        return(-99);
    }

    /* Make sure there're no stale messages in the Qs */
    EmptyQueuesClearEvents();
    
    g_btmgrsh_iap_recv_buf_rd_ptr = NULL;
    
    ASSIGN_BD_ADDR(CurrentRemoteBD_ADDR, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
    
    /* Simply attempt to open an Serial Server, on requested RFCOMM Server port */
    tempPort = ISPP_Open_Server_Port(BluetoothStackID, port, ISPP_Event_Callback, (unsigned long)0);
    
    /* If the Open was successful, then note the Serial Port    */
    /* Server ID.                                               */
    if(tempPort <= 0)
    {
        return (tempPort);
    }
       
    /* Create a Buffer to hold the Service Name.             */
    wassert((ServiceName = BTPS_AllocateMemory(64)) != NULL);
    
    /* The Server was opened successfully, now register a */
    /* SDP Record indicating that an Serial Port Server   */
    /* exists. Do this by first creating a Service Name.  */
    BTPS_SprintF(ServiceName, "Serv Port %d (iSPP)", (int)port);
    
    /* Now that a Service Name has been created try to    */
    /* Register the SDP Record.                           */
    ret_val = ISPP_Register_SDP_Record(BluetoothStackID, tempPort, NULL, ServiceName, &SPPServerSDPHandle);
    
    /* Free the Service Name buffer.                      */
    BTPS_FreeMemory(ServiceName);

    /* If there was an error creating the Serial Port     */
    /* Server's SDP Service Record then go ahead an close */
    /* down the server an flag an error.                  */
    if(ret_val < 0)
    {
        PROCESS_NINFO((err_t)ret_val, "Unable to Register Serv SDP Record");
        ISPP_Close_Server_Port(BluetoothStackID, tempPort);
        
        /* Flag that we are no longer connected.           */
        Connected    = FALSE;
        Connecting   = FALSE;
        
        return (ret_val);
    }
    
    /* Simply flag to the user that everything         */
    /* initialized correctly.                          */
    BT_VERBOSE(corona_print("ServOpen %d\n", port);)
    
    SerialPortID = tempPort;
            
    return(ret_val);
}

/* The following function is responsible for closing a Serial Port   */
/* Server that was previously opened via a successful call to the    */
/* OpenServer() function.              */
void CloseServer(void)
{
    _mqx_uint mqx_rc;
    Byte_t StatusResult;
    int rc;

    BT_VERBOSE(corona_print("CloseServ\n");)
    
    /* First check to see if a valid Bluetooth Stack ID exists.          */
    _chk_bt_id();
    
    ModifiedFIDInfoFree();
    
    /* Next, let's make sure that a Local Serial Port Server is   */
    /* indeed Open.                                                */
    if(!SerialPortID)
    {
    	BT_VERBOSE(corona_print("Already closed\n");)
        return;
    }

    /* If there is an SDP Service Record associated with the Serial*/
    /* Port Server then we need to remove it from the SDP Database.*/
    if(SPPServerSDPHandle)
    {
        ISPP_Un_Register_SDP_Record(BluetoothStackID, SerialPortID, SPPServerSDPHandle);
        
        /* Flag that there is no longer an SDP Serial Port Server   */
        /* Record.                                                  */
        SPPServerSDPHandle = 0;
    }
    
    /* Finally close the Serial Port Server.                       */
    ISPP_Close_Server_Port(BluetoothStackID, SerialPortID);
    
    /* make sure there's nothing in the Q's except the Disc Notification we're about to send */
    EmptyQueuesClearEvents();
    
    /* Notify waiters */
    NotifyDisconnect();

    if (Connecting && SDP_ServiceRequestID)
    {
    	SDP_Cancel_Service_Request(BluetoothStackID, SDP_ServiceRequestID);
    }

    /* Simply close the Serial Port Client.                     */
    ISPP_Close_Port(BluetoothStackID, SerialPortID);
    
    if (0 != Connection_Handle)
    {
    	rc = HCI_Disconnect(BluetoothStackID, Connection_Handle, 0, &StatusResult);
    	
    	if (0 == rc)
    	{
    		mqx_rc =_lwevent_wait_ticks(&g_btmgrsh_lwevent,
    				DISCONNECT_COMPLETE_EVENT, FALSE,
    				1*BSP_ALARM_FREQUENCY); // 1 second

    		if (LWEVENT_WAIT_TIMEOUT == mqx_rc)
    		{
    			BT_VERBOSE(corona_print("CloseServ TO wait Disconn\n");)
    		}
    		else
    		{
    			wassert(MQX_OK == mqx_rc);
    		}
    	}
    }

    /* Flag that there is no Serial Port Server currently open.    */
    SerialPortID = 0;
    
    if(iAPTimerID)
        BSC_StopTimer(BluetoothStackID, iAPTimerID);
    
    iAPTimerID = 0;
    
    if(iAPTimerID2)
        BSC_StopTimer(BluetoothStackID, iAPTimerID2);
    
    iAPTimerID2 = 0;

    /* Flag that we are no longer connected.                       */
    Connected  = FALSE;
    
    ASSIGN_BD_ADDR(CurrentRemoteBD_ADDR, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
    
    BT_VERBOSE(corona_print("Serv Closed\n");)
}

/* The following function is responsible for initiating a connection */
/* with a Remote Serial Port Server.               */
int OpenRemoteServer(BD_ADDR_t BD_ADDR, Port_Operating_Mode_t pom)
{
    int ret_val;
    int Result;
    unsigned int port;
    int retries = 0;
    unsigned int size_FIDInfo = 0;
    Byte_t *pFIDInfo = NULL;
    unsigned int maxRxPacketSize = 0;
    
    /* First, check that valid Bluetooth Stack ID exists.                */
    _chk_bt_id();
    
    /* Next, let's make sure that there is not an Serial Port      */
    /* Client already open.                                        */
    if(SerialPortID)
    {
        return (-99);
    }
    
    /* Make sure there're no stale messages in the Qs */
    EmptyQueuesClearEvents();
    
    g_btmgrsh_iap_recv_buf_rd_ptr = NULL;

    CurrentOperatingMode = pom;

    /* Blocking call to get the remote port number */
    Result = SDPRequest(BD_ADDR, (int *)&port, pom);
    
    if (Result)
    {
        return (-98);
    }
    
    /* Now let's attempt to open the Remote Serial Port      */
    /* Server.                                               */
    
    if (pomiAP == pom)
    {
    	pFIDInfo = (Byte_t *)ModifiedFIDInfoAlloc(&size_FIDInfo);
    	maxRxPacketSize = MAX_SMALL_PACKET_PAYLOAD_SIZE;
    }
    
    do
    {
    	Result = ISPP_Open_Remote_Port(BluetoothStackID, BD_ADDR, port, size_FIDInfo, pFIDInfo, maxRxPacketSize, ISPP_Event_Callback, (unsigned long)0);
    	
    	if (BTPS_ERROR_RFCOMM_UNABLE_TO_CONNECT_TO_REMOTE_DEVICE == Result)
    	{
    		_time_delay(100);
    	}
    } while (BTPS_ERROR_RFCOMM_UNABLE_TO_CONNECT_TO_REMOTE_DEVICE == Result && 10 > retries++);
        
    /* Note the Serial Port Client ID.                    */
    SerialPortID = Result;
        
    /* Save the BD_ADDR so we can Query the Connection    */
    /* handle when receive Connection Confirmation Event. */
    SelectedBD_ADDR = BD_ADDR;
    
    return(Result);
}

int ForceDisconnect(void)
{
    Byte_t StatusResult;

    if (0 != Connection_Handle)
    {
    	return HCI_Disconnect(BluetoothStackID, Connection_Handle, 0, &StatusResult);
    }
    
    return 0;
}

/* The following function is responsible for terminating a connection*/
/* with a Remote Serial Port Server.               */
/* Blocks until closed */
int CloseRemoteServer(void)
{
    _mqx_uint mqx_rc;
    int rc = 0;
    
    BT_VERBOSE(corona_print("%s\n", __func__);)

    /* First, check that valid Bluetooth Stack ID exists.                */
    _chk_bt_id();
    
    ModifiedFIDInfoFree();
    
    /* make sure there's nothing in the Q's except the Disc Notification we're about to send */
    EmptyQueuesClearEvents();
    
    /* Notify waiters */
    NotifyDisconnect();
    
    /* Next, let's make sure that a Remote Serial Port Server is   */
    /* indeed Open.                                                */
    if(!SerialPortID)
    {
    	rc = -1;
    	goto DONE;
    }
        
    if (Connecting && SDP_ServiceRequestID)
    {
    	SDP_Cancel_Service_Request(BluetoothStackID, SDP_ServiceRequestID);
    }
    
    /* Simply close the Serial Port Client.                     */
    ISPP_Close_Port(BluetoothStackID, SerialPortID);
    
    if(iAPTimerID)
        BSC_StopTimer(BluetoothStackID, iAPTimerID);
    
    iAPTimerID = 0;
    
    if(iAPTimerID2)
        BSC_StopTimer(BluetoothStackID, iAPTimerID2);
    
    iAPTimerID2 = 0;
    
    rc = ForceDisconnect();
	
    /* Wait for the disconnect to complete */
	if (0 == rc)
	{
		mqx_rc =_lwevent_wait_ticks(&g_btmgrsh_lwevent,
				DISCONNECT_COMPLETE_EVENT, FALSE,
				1*BSP_ALARM_FREQUENCY); // 1 second

		if (LWEVENT_WAIT_TIMEOUT == mqx_rc)
		{
			BT_VERBOSE(corona_print("CloseServ T.O. wait Disconn\n");)
		}
	}

    
    /* Flag that there is no longer a Serial Port Client        */
    /* connected.                                               */
    SerialPortID = 0;
    
    /* Flag that we are no longer connected.                    */
    Connected    = FALSE;
    
DONE:
	BTMGR_PRIV_disconnected();
	
    BT_VERBOSE(corona_print("Client Port closed\n");)
    
    return rc;
}

/* The following function is responsible for reading data that was   */
/* received via an Open SPP port.  This */
/* function requires that a valid Bluetooth Stack ID and Serial Port */
/* ID exist before running.             */
int Read(unsigned char *data, unsigned int length)
{
    /* First, check that valid Bluetooth Stack ID exists.                */
    _chk_bt_id();
    
    /* Next, let's make sure that a Remote Serial Port Server is   */
    /* indeed Open.                                                */
    if(!SerialPortID)
    {
        return (-99);
    }

    /* First check to see if the parameters required for the execution of*/
    /* this function appear to be semi-valid.                            */
    if(CurrentOperatingMode != pomSPP)
    {
        return (-98);
    }
    
    /* The required parameters appear to be semi-valid, send the      */
    /* command to Read Data from SPP.                                 */
    return ISPP_Data_Read(BluetoothStackID, SerialPortID, (Word_t)length, (Byte_t*)data);
}

/* The following function is responsible for Writing Data to an Open */
/* SPP Port (non-iOS).  This function requires that */
/* a valid Bluetooth Stack ID and Serial Port ID exist before        */
/* running.                               */
int Write(unsigned char *data, unsigned int length)
{
    unsigned int bytes_to_write = length;
	_mqx_uint mqx_rc;
	int rc;
	unsigned char *wr_ptr = data;

    /* First, check that valid Bluetooth Stack ID exists.                */
    _chk_bt_id();
    
    /* Next, let's make sure that a Remote Serial Port Server is   */
    /* indeed Open.                                                */
    if(!SerialPortID)
    {
        return (-99);
    }

    /* First check to see if the parameters required for the execution of*/
    /* this function appear to be semi-valid.                            */
    if(CurrentOperatingMode != pomSPP)
    {
        return (-98);
    }
        
    while (TRUE)
    {
    	rc = ISPP_Data_Write(BluetoothStackID, SerialPortID, (Word_t)bytes_to_write, (Byte_t *)wr_ptr);
    	
    	if (0 > rc)
    		return rc; /* error */
    	
    	bytes_to_write -= rc;
    	wr_ptr += rc;

    	if (0 == bytes_to_write)
    		break; /* succeeded */

    	wassert(0 < bytes_to_write);

    	/* wait for room for more */
    	mqx_rc = _lwevent_wait_ticks(&g_btmgrsh_lwevent, TRANSMIT_BUFFER_EMPTY_EVENT, FALSE, 5*BSP_ALARM_FREQUENCY); // 5 seconds

    	if (LWEVENT_WAIT_TIMEOUT == mqx_rc)
    	{
    		BT_VERBOSE(corona_print("Write TO\n");)
    		return (-97);
    	}
    }

    return 0;
}

/* The following function is responsible for sending a number of     */
/* characters to a remote device to which a session exists.  The     */
/* function receives a parameter that indicates the number of byte to*/
/* be transferred.                           */
int SendSessionData(unsigned char *data, unsigned int length)
{
    Boolean_t Done;
    Byte_t    Status;
    Word_t    DataLength;
    DWord_t   BytesToSend;
    DWord_t   BytesSent = 0;
    int       PacketID;
    _mqx_max_type msg[MSG_SIZE_SND_Q];  
    
    /* Verify that there is an open session.                          */
    if(CurrentOperatingMode != pomiAP)
    {
        return (-99);
    }

    if((!iAPSessionID) || (!SerialPortID) || (!Connected))
    {
        return (-98);
    }
    
    BytesSent    = 0;
    
    Done = FALSE;

    do
    {
        /* Limit the amount of data to send.                  */
        if(length - BytesSent > MaxPacketData)
        {
        	BytesToSend = MaxPacketData;
        }
        else
        {
        	BytesToSend = length - BytesSent;
        }

        PacketID = ISPP_Send_Session_Data(BluetoothStackID, SerialPortID, iAPSessionID, BytesToSend, data);

        /* Make sure it didn't fail right off the bat */
        if(0 >= PacketID)
        {
           return -97;
        }

        /* Block waiting for ACK or transmit buffer empty */
        _lwmsgq_receive((pointer)SendSessionEventQueue, msg, LWMSGQ_RECEIVE_BLOCK_ON_EMPTY, 0, 0);

        /* Make sure the SessionID matches */
        if(msg[0] != iAPSessionID)
        {
        	return -96;
        }

        /* Make sure the  PacketID matches */
        if(msg[1] != PacketID)
        {
        	return -95;
        }

        /* Verify that the packet was successfully sent.        */
        switch(msg[2])
        {
            case PACKET_SEND_STATUS_ACKNOWLEDGED:
                /* Credit the number of byte that we have sent.       */
                BytesSent += BytesToSend;
                //corona_print("\nSent %d of %d\n", BytesSent, BytesToSend);
    
                break;
            case PACKET_SEND_STATUS_FAILED:
                /* Besides stack generated failures, msg[2] is forced to 'failed' if link is disconnected */
                return -92;
            default:
                return -91;
        }
    } while (BytesSent < length);
    
    return(0);
}

/* The following function is responsible for querying a Remote Device*/
/* for the Port Number of the iAP Server.  */
static int SDPRequest(BD_ADDR_t BD_ADDR, int *port, Port_Operating_Mode_t pom)
{
    int                           Result;
    SDP_UUID_Entry_t              UUID_Entry;
    SDP_Attribute_ID_List_Entry_t AttributeID;
    _mqx_max_type msg[MSG_SIZE_SDPREQ_Q];  
    
    /* First, check that valid Bluetooth Stack ID exists.                */
    _chk_bt_id();
    
	AttributeID.Attribute_Range      = FALSE;
	AttributeID.Start_Attribute_ID   = SDP_ATTRIBUTE_ID_PROTOCOL_DESCRIPTOR_LIST;

    if (pomiAP == pom)
    {
    	UUID_Entry.SDP_Data_Element_Type = deUUID_128;
    	SDP_ASSIGN_IAP_SPECIAL_SERIAL_PORT_PROFILE_UUID_128(UUID_Entry.UUID_Value.UUID_128);
    }
    else
    {
    	UUID_Entry.SDP_Data_Element_Type = deUUID_128;
    	//SDP_ASSIGN_SERIAL_PORT_PROFILE_UUID_128(UUID_Entry.UUID_Value.UUID_128);
    	ASSIGN_SDP_UUID_128(UUID_Entry.UUID_Value.UUID_128, 0x6C, 0xDC, 0x3B, 0x69, 0x00, 0x77, 0x4D, 0x0A, 0xB9, 0x1C, 0x71, 0x2B, 0x2B, 0x37, 0x77, 0x5F)
    }
    
    Result = SDP_Service_Search_Attribute_Request(BluetoothStackID, BD_ADDR, 1, &UUID_Entry, 1, &AttributeID, SDP_Event_Callback, 0);
    
    if (Result <= 0)
    {
        return Result;
    }
    
    SDP_ServiceRequestID = Result;
    
    _lwmsgq_receive((pointer)SDPRequestQueue, msg, LWMSGQ_RECEIVE_BLOCK_ON_EMPTY, 0, 0);
    
    SDP_ServiceRequestID = 0;
       
    /* Note, msg[0] will be forced to -1 if link is disconnected */
    *port = (int)msg[0];
    
    return *port < 0 ? *port : 0;
}

static int ReceiveBlock(_mqx_max_type *msg, uint_32 ms)
{
    MQX_TICK_STRUCT ticks;
    _mqx_uint m_rc;

	_time_init_ticks(&ticks, 0);
	_time_add_msec_to_ticks(&ticks, ms);

	m_rc = _lwmsgq_receive(ReceiveSessionEventQueue, msg, LWMSGQ_RECEIVE_BLOCK_ON_EMPTY | LWMSGQ_TIMEOUT_FOR, 0, &ticks);

	if (LWMSGQ_TIMEOUT == m_rc)
	{
		return -99;
	}

	if (-1 == msg[1])
	{
		/* Note, msg[1] will be forced to -1 if link is disconnected */
		return -98;
	}
		
	return 0;
}

static int IAPBlockingReceive(void **buf, uint_32 ms, int_32 *pbytes_received)
{
	_mqx_max_type msg[MSG_SIZE_RCV_Q];
	int rc;

	*pbytes_received = 0;

	rc = ReceiveBlock(msg, ms);
	
	if (0 != rc)
	{
		return rc;
	}
	
	if (iAPSessionID != (int)msg[0])
	{
		return -199;
	}
	
	*pbytes_received = (Word_t)msg[1];
    *buf = (Byte_t *)msg[2];

	return rc;
}

static int SPPBlockingReceive(void *buf, uint_32 buf_len, uint_32 ms, int_32 *pbytes_received)
{
	_mqx_max_type msg[MSG_SIZE_RCV_Q];
	int rc;

	*pbytes_received = 0;
	
	rc = ReceiveBlock(msg, ms);
	
	if (0 != rc)
	{
		return rc;
	}
	
	if (SerialPortID != (int)msg[0])
	{
		return -199;
	}
	
	rc = Read(buf, (uint_32)msg[1] > buf_len ? buf_len : (uint_32)msg[1]);
	
	if (0 > rc)
	{
		return rc;
	}
	
	*pbytes_received = (int_32)msg[1];

	return 0;
}

static void ReceiveWithCopyComplete(void)
{
	g_btmgrsh_iap_recv_buf_rd_ptr = NULL;
	
	// The stack can release it now that we're done
	BTMGR_PRIV_zero_copy_free();
}

static int IAPReceive(void *buf, uint_32 buf_len, uint_32 ms, int_32 *pbytes_received, boolean continuous_aggregate)
{
    TIME_STRUCT now;
    uint_64 ms_now_time_stamp;
    uint_64 ms_previous_time_stamp;
    uint_32 ms_remaining;
    int_32 received;
    uint_8 *wr_ptr;
    uint_32 wr_remaining;
    int rc;
    
    //corona_print("IAPReceive1 %d %x %x %d\n", buf_len, g_btmgrsh_iap_recv_buf_rd_ptr, g_btmgrsh_iap_recv_buf_end_ptr, g_btmgrsh_iap_recv_buf_end_ptr - g_btmgrsh_iap_recv_buf_rd_ptr);
    
    *pbytes_received = 0;
    	
	// See if we still have some from last read from the stack
	if (NULL != g_btmgrsh_iap_recv_buf_rd_ptr)
	{
		uint_32 stack_recv_buf_bytes_remaining = g_btmgrsh_iap_recv_buf_end_ptr - g_btmgrsh_iap_recv_buf_rd_ptr;
		
		// See if remaining amount can satisfy current request
		if (buf_len <= stack_recv_buf_bytes_remaining)
		{
			memcpy(buf, g_btmgrsh_iap_recv_buf_rd_ptr, buf_len);

			// See if we just consumed it all
			if (buf_len == stack_recv_buf_bytes_remaining)
			{
				ReceiveWithCopyComplete(); // tell the stack we're done with it
			    //corona_print("IAPReceive2 %d\n", buf_len);
			}
			else
			{
				g_btmgrsh_iap_recv_buf_rd_ptr += buf_len;
			    //corona_print("IAPReceive3 %x %d\n", g_btmgrsh_iap_recv_buf_rd_ptr, buf_len);
			}
			
			*pbytes_received = buf_len;			
			goto DONE;
		}
		else
		{
			// Well now, we still have some from last time, but he needs even more.
			// I suppose we can give him what we have so far, and then wait for more.
			// Sound good?
			memcpy(buf, g_btmgrsh_iap_recv_buf_rd_ptr, stack_recv_buf_bytes_remaining);
			*pbytes_received = stack_recv_buf_bytes_remaining; // so far
			
			ReceiveWithCopyComplete(); // tell the stack we're done with it
		    //corona_print("IAPReceive4 %d\n", stack_recv_buf_bytes_remaining);
		}
	}
	
	// At this point, we've consumed anything that was left over from the last call
	// to us (if there was anything), AND we know we still need more to satisfy the caller's request.
	// g_btmgr_stack_iap_recv_buf_rd_ptr is NULL, and we are starting on a new buffer from the stack.
    
	// Since the complete read may span multiple BTMGR_receive calls, we need to keep
	// track of the elapsed ms.
	RTC_get_time_ms(&now);
	ms_previous_time_stamp = now.SECONDS*1000 + now.MILLISECONDS;
	ms_remaining = ms;
	
    wr_ptr = (uint_8 *)buf + *pbytes_received;
    wr_remaining = buf_len - *pbytes_received;

    do
    {
    	rc = IAPBlockingReceive((void **)&g_btmgrsh_iap_recv_buf_rd_ptr, ms_remaining, &received);

    	if (received <= 0)
    	{
    		g_btmgrsh_iap_recv_buf_rd_ptr = NULL;
    		return PROCESS_NINFO(ERR_BT_ERROR, "rx %d %d", rc, received);
    	}

    	if (received <= wr_remaining)
    	{
    		// We got less than or equal to how much he's waiting for. Consume that.
    		memcpy(wr_ptr, g_btmgrsh_iap_recv_buf_rd_ptr, received);

    		ReceiveWithCopyComplete(); // tell the stack we're done with it

    		*pbytes_received += received;

    		if (received == wr_remaining)
    		{
    			// Hey, we got exactly how much he was waiting for. We're done.
    			wassert(*pbytes_received == buf_len);
			    //corona_print("IAPReceive5 %d %d\n", received, *pbytes_received);
    			goto DONE;
    		}

    		// Still more to go.
    		wr_ptr += received;  
    		wr_remaining -= received;
    		
		    //corona_print("IAPReceive6 %x %d %d\n", wr_ptr, received, wr_remaining);
    	}
    	else // received > wr_remaining
    	{
    		// We got more than he (caller) needs. Give him however much
    		// he asked for. Save the rest for the next call.
    		memcpy(wr_ptr, g_btmgrsh_iap_recv_buf_rd_ptr, wr_remaining);

    		g_btmgrsh_iap_recv_buf_end_ptr = g_btmgrsh_iap_recv_buf_rd_ptr + received;
    		g_btmgrsh_iap_recv_buf_rd_ptr += wr_remaining;

    		*pbytes_received += wr_remaining;
    		wassert(*pbytes_received == buf_len);
    		
		    //corona_print("IAPReceive7 %x %x %x %d %d %d\n", wr_ptr, g_btmgrsh_iap_recv_buf_rd_ptr, g_btmgrsh_iap_recv_buf_end_ptr, received, wr_remaining, *pbytes_received);

    		goto DONE;
    	}
    	
	    //corona_print("IAPReceive8 %x %x %x %d %d %d\n", wr_ptr, g_btmgrsh_iap_recv_buf_rd_ptr, g_btmgrsh_iap_recv_buf_end_ptr, received, wr_remaining, *pbytes_received);
    	
        RTC_get_time_ms(&now);
        ms_now_time_stamp = now.SECONDS*1000 + now.MILLISECONDS;
        ms_remaining -= ms_now_time_stamp - ms_previous_time_stamp;
        ms_previous_time_stamp = ms_now_time_stamp;
    } while (continuous_aggregate && ms_remaining > 0);

DONE:
	return 0;
}

static int SPPReceive(void *buf, uint_32 buf_len, uint_32 ms, int_32 *pbytes_received, boolean continuous_aggregate)
{
    TIME_STRUCT now;
    uint_64 ms_now_time_stamp;
    uint_64 ms_previous_time_stamp;
    uint_32 ms_remaining;
    int_32 received;
    uint_8 *wr_ptr;
    uint_32 wr_remaining;
    int rc;
    
    *pbytes_received = 0;
    	
	// See if we still have some from last read from the stack
	if (g_btmgr_spp_recv_remaining)
	{
		// See if remaining amount can satisfy current request
		if (buf_len <= g_btmgr_spp_recv_remaining)
		{
			received = Read(buf, buf_len); // We already know we have this much
			
			if (0 > received)
			{
				return PROCESS_NINFO(ERR_BT_ERROR, "rx %d", received);
			}

			// See if we just consumed it all
			if (buf_len == g_btmgr_spp_recv_remaining)
			{
				g_btmgr_spp_recv_remaining = 0;
			}
			else
			{
				g_btmgr_spp_recv_remaining -= buf_len;
			}
			
			*pbytes_received = buf_len;			
			goto DONE;
		}
		else
		{
			// Well now, we still have some from last time, but he needs even more.
			// I suppose we can give him what we have so far, and then wait for more.
			// Sound good?
			received = Read(buf, g_btmgr_spp_recv_remaining); // We already know we have this much
			
			if (0 > received)
			{
				return PROCESS_NINFO(ERR_BT_ERROR, "rx %d", received);
			}

			*pbytes_received = g_btmgr_spp_recv_remaining; // so far
			
			g_btmgr_spp_recv_remaining = 0;
		}
	}
	
	// At this point, we've consumed anything that was left over from the last call
	// to us (if there was anything), AND we know we still need more to satisfy the caller's request.
	// g_btmgr_stack_iap_recv_buf_rd_ptr is NULL, and we are starting on a new buffer from the stack.
    
	// Since the complete read may span multiple BTMGR_receive calls, we need to keep
	// track of the elapsed ms.
	RTC_get_time_ms(&now);
	ms_previous_time_stamp = now.SECONDS*1000 + now.MILLISECONDS;
	ms_remaining = ms;

	wr_ptr = (uint_8 *)buf + *pbytes_received;
	wr_remaining = buf_len - *pbytes_received;

	do
	{
		rc = SPPBlockingReceive(wr_ptr, wr_remaining, ms, &received);

		g_btmgr_spp_recv_remaining = 0;

		if (received <= 0)
		{
			return PROCESS_NINFO(ERR_BT_ERROR, "rx %d %d", rc, received);
		}

		if (received <= wr_remaining)
		{
			// We got less than or equal to how much he's waiting for. Consume that.
			*pbytes_received += received;

			if (received == wr_remaining)
			{
				// Hey, we got exactly how much he was waiting for. We're done.
				wassert(*pbytes_received == buf_len);
				goto DONE;
			}

			// Still more to go.
			wr_ptr += received;  
			wr_remaining -= received;
		}
		else // received > wr_remaining
		{
			// We got more than he (caller) needs. Give him however much
			// he asked for. Save the rest for the next call.
			g_btmgr_spp_recv_remaining = received - wr_remaining;

			*pbytes_received += wr_remaining;
			wassert(*pbytes_received == buf_len);

			goto DONE;
		}
    	
        RTC_get_time_ms(&now);
        ms_now_time_stamp = now.SECONDS*1000 + now.MILLISECONDS;
        ms_remaining -= ms_now_time_stamp - ms_previous_time_stamp;
        ms_previous_time_stamp = ms_now_time_stamp;
    } while (continuous_aggregate && ms_remaining > 0);

DONE:
	return 0;
}

int BTMGR_PRIV_receive(void *buf, uint_32 buf_len, uint_32 ms, int_32 *pbytes_received, boolean continuous_aggregate)
{
	if (pomiAP == CurrentOperatingMode)
	{
		return IAPReceive(buf, buf_len, ms, pbytes_received, continuous_aggregate);
	}
	else
	{
		return SPPReceive(buf, buf_len, ms, pbytes_received, continuous_aggregate);
	}
}

int BTMGR_PRIV_send(uint_8 *data, uint_32 length)
{
	if (CurrentOperatingMode == pomiAP)
		return SendSessionData(data, length);
	else
		return Write(data, length);
}

void BTMGR_PRIV_zero_copy_free(void)
{
    ISPP_Ack_Last_Session_Data_Packet(BluetoothStackID, SerialPortID, iAPSessionID);
}

void BTMGR_PRIV_set_pair(Boolean_t pairEnable)
{
    pairMode = pairEnable;
}

void BTMGR_PRIV_set_ssp_debug(boolean ssp_debug_enable)
{
	Byte_t StatusResult;
	int rc;
	
	if (SSPDebugEnabled == ssp_debug_enable)
	{
		return;
	}

	SSPDebugEnabled = ssp_debug_enable;
	
	if (0 == BluetoothStackID)
	{
		// We still want to save SSPDebugEnabled for when stack does come up
		return;
	}
	
	rc = HCI_Write_Simple_Pairing_Debug_Mode(BluetoothStackID, ssp_debug_enable, &StatusResult);
	BT_VERBOSE(corona_print("pair mode ret'd %d. Stat %d\n", rc, StatusResult);)
}

boolean BTMGR_PRIV_test_connection(BD_ADDR_t BD_ADDR)
{
    _mqx_max_type msg[MSG_SIZE_CT_Q];  
    _mqx_uint m_rc;
    Byte_t StatusResult;
    Class_of_Device_t CODResult;
    int LCID;
    MQX_TICK_STRUCT ticks;
    
    _time_init_ticks(&ticks, 0);
    _time_add_msec_to_ticks(&ticks, 30*1000);
    
    /* Make sure there're no stale messages in the Qs */
    EmptyQueuesClearEvents();
    
    /* Attempt to setup an L2CAP connection */
    LCID = L2CA_Connect_Request(BluetoothStackID, BD_ADDR, 1 /*SDP*/, L2CAP_Event_Callback, TEST_CONNECTION);

    if (0 >= LCID)
    {
    	return FALSE;
    }
        
    /* Wait for a response */
    m_rc = _lwmsgq_receive(CTResponseQueue, msg, LWMSGQ_RECEIVE_BLOCK_ON_EMPTY, 0, &ticks);
    
    /* L2CA_Connect_Request() should timeout and reply back. If not, something's really wrong */
    if (LWMSGQ_TIMEOUT == m_rc)
    {
    	PROCESS_NINFO( ERR_BT_STACK_FAILURE, NULL );
        PWRMGR_Reboot( 500, PV_REBOOT_REASON_BT_FAIL, FALSE );
    }
    
    if (LCID != msg[0])
    {
    	/* Note, msg[0] will be forced to -1 if link is disconnected */
    	return FALSE;
    }
    
    if (test_connection_failed != (test_connection_t)msg[1])
    {
        /* If it's still connected, disconnect it */
    	if (test_connection_is_connected == (test_connection_t)msg[1])
    	{
    		if (0 == L2CA_Disconnect_Request(BluetoothStackID, LCID))
    		{
    			/* Wait for a response */
    			_time_init_ticks(&ticks, 0);
    			_time_add_msec_to_ticks(&ticks, 5*1000);

    			_lwmsgq_receive(CTResponseQueue, msg, LWMSGQ_RECEIVE_BLOCK_ON_EMPTY, 0, &ticks);
    		}
    	}
    	
    	return TRUE;
    }
        	
	return FALSE;
}

static void BTMGR_PRIV_launch_application()
{
    ISPP_Send_Raw_Data(BluetoothStackID, SerialPortID, IAP_LINGO_ID_GENERAL, IAP_GENERAL_LINGO_REQUEST_APPLICATION_LAUNCH, 0, sizeof(LaunchPacket), (Byte_t *)LaunchPacket);
}

int BTMGR_PRIV_set_connect_timeout(uint_32 ms)
{
	Byte_t StatusResult;
	int rc;
	
	rc = HCI_Write_Page_Timeout(BluetoothStackID, (Word_t)(((float)ms)/0.625), &StatusResult);
	
	return rc == 0 && StatusResult == HCI_ERROR_CODE_NO_ERROR;
}

Port_Operating_Mode_t BTMGR_PRIV_get_current_pom(void)
{
	return CurrentOperatingMode;
}

/*********************************************************************/
/*                         Event Callbacks                           */
/*********************************************************************/

/* The following function is the Event Timeout callback function that*/
/* is registered to timeout waiting for the iAP Authentication       */
/* process to being.                                                 */
static void BTPSAPI BSC_Timer_Callback(unsigned int BluetoothStackID, unsigned int TimerID, unsigned long CallbackParameter)
{
    int                   ret_val;
    Port_Operating_Mode_t OperatingMode;
    
    if ((SerialPortID) && (Connected) && (iAPTimerID) && (0 == CallbackParameter))
    {
        //ISPP_Get_Port_Operating_Mode(BluetoothStackID, SerialPortID, &OperatingMode);
        if(CurrentOperatingMode == pomiAP)
        {
            BT_VERBOSE(corona_print("iAP T.O. Port %d\n", SerialPortID);)
                        
            ret_val = ISPP_Start_Authorization(BluetoothStackID, (unsigned int)SerialPortID);
            BT_VERBOSE(corona_print("\nISPP_Start_Auth(%d) RC %d\n", (int)SerialPortID, ret_val);)
        }
        else
        {
    		BTMGR_PRIV_connecting_complete(TRUE); // This is where we are "connected" in the non-ios world
        }
        
        iAPTimerID = 0;
    }
    else if ((iAPTimerID2) && (1 == CallbackParameter))
    {
    	SaveConfig();
    	BTMGR_PRIV_non_ios_pairing_complete();
    	iAPTimerID2 = 0;
    }
}

/* The following function is for the GAP Event Receive Data Callback.*/
/* This function will be called whenever a Callback has been         */
/* registered for the specified GAP Action that is associated with   */
/* the Bluetooth Stack.  This function passes to the caller the GAP  */
/* Event Data of the specified Event and the GAP Event Callback      */
/* Parameter that was specified when this Callback was installed.    */
/* The caller is free to use the contents of the GAP Event Data ONLY */
/* in the context of this callback.  If the caller requires the Data */
/* for a longer period of time, then the callback function MUST copy */
/* the data into another Data Buffer.  This function is guaranteed   */
/* NOT to be invoked more than once simultaneously for the specified */
/* installed callback (i.e.  this function DOES NOT have be          */
/* reentrant).  It Needs to be noted however, that if the same       */
/* Callback is installed more than once, then the callbacks will be  */
/* called serially.  Because of this, the processing in this function*/
/* should be as efficient as possible.  It should also be noted that */
/* this function is called in the Thread Context of a Thread that the*/
/* User does NOT own.  Therefore, processing in this function should */
/* be as efficient as possible (this argument holds anyway because   */
/* other GAP Events will not be processed while this function call is*/
/* outstanding).                                                     */
/* * NOTE * This function MUST NOT Block and wait for events that    */
/*          can only be satisfied by Receiving other GAP Events.  A  */
/*          Deadlock WILL occur because NO GAP Event Callbacks will  */
/*          be issued while this function is currently outstanding.  */
static void BTPSAPI GAP_Event_Callback(unsigned int BluetoothStackID, GAP_Event_Data_t *GAP_Event_Data, unsigned long CallbackParameter)
{
    int                               Result;
    int                               Index;
    Boolean_t                         OOB_Data;
    Boolean_t                         MITM;
    GAP_IO_Capability_t               RemoteIOCapability;
    GAP_Inquiry_Event_Data_t         *GAP_Inquiry_Event_Data;
    GAP_Remote_Name_Event_Data_t     *GAP_Remote_Name_Event_Data;
    GAP_Authentication_Information_t  GAP_Authentication_Information;
    
    BT_VERBOSE(corona_print("EventCbk %x EVT %x\n", GAP_Event_Data->Event_Data_Type, 
                                                    GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->GAP_Authentication_Event_Type);)

    /* First, check to see if the required parameters appear to be       */
    /* semi-valid.                                                       */
    if((BluetoothStackID) && (GAP_Event_Data))
    {
        /* The parameters appear to be semi-valid, now check to see what  */
        /* type the incoming event is.                                    */
        switch(GAP_Event_Data->Event_Data_Type)
        {
            case etAuthentication:
                /* An authentication event occurred, determine which type of*/
                /* authentication event occurred.                           */
                switch(GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->GAP_Authentication_Event_Type)
                {
                    case atLinkKeyRequest:
                        BD_ADDRToStr(GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Remote_Device, Callback_BoardStr);
                        BT_VERBOSE(corona_print("\nLinkKeyReq: %s\n", Callback_BoardStr);)
                        
                        /* Setup the authentication information response      */
                        /* structure.                                         */
                        GAP_Authentication_Information.GAP_Authentication_Type    = atLinkKey;
                        GAP_Authentication_Information.Authentication_Data_Length = 0;
                        
                        /* See if we have stored a Link Key for the specified */
                        /* device.                                            */
                        BT_VERBOSE(corona_print("find key\n");)
                        for(Index=0;Index<(sizeof(g_dy_cfg.bt_dy.LinkKeyInfo)/sizeof(LinkKeyInfo_t));Index++)
                        {
                        	if(COMPARE_BD_ADDR(g_dy_cfg.bt_dy.LinkKeyInfo[Index].BD_ADDR, GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Remote_Device))
                        	{
                        		/* Link Key information stored, go ahead and    */
                        		/* respond with the stored Link Key.            */
                        		GAP_Authentication_Information.Authentication_Data_Length   = sizeof(Link_Key_t);
                        		memcpy(&GAP_Authentication_Information.Authentication_Data.Link_Key, &g_dy_cfg.bt_dy.LinkKeyInfo[Index].LinkKey, sizeof(Link_Key_t));

                        		break;
                        	}
                        }
                        
                        /* Submit the authentication response.                */
                        Result = GAP_Authentication_Response(BluetoothStackID, GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Remote_Device, &GAP_Authentication_Information);
                        
                        /* Check the result of the submitted command.         */
                        if(!Result)
                        {                        	
                        	BT_VERBOSE(corona_print("Auth OK\n");)

                        	LastPairedIndex = Index;
                        	
                            memcpy(&CurrentLinkKey, &g_dy_cfg.bt_dy.LinkKeyInfo[Index].LinkKey, sizeof(Link_Key_t));
                            memcpy(&CurrentRemoteBD_ADDR, &g_dy_cfg.bt_dy.LinkKeyInfo[Index].BD_ADDR, sizeof(BD_ADDR_t));
                            CurrentOperatingMode = g_dy_cfg.bt_dy.LinkKeyInfo[Index].pom;

                        	corona_print("key 0x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
                        			g_dy_cfg.bt_dy.LinkKeyInfo[Index].LinkKey.Link_Key15, g_dy_cfg.bt_dy.LinkKeyInfo[Index].LinkKey.Link_Key14, g_dy_cfg.bt_dy.LinkKeyInfo[Index].LinkKey.Link_Key13, g_dy_cfg.bt_dy.LinkKeyInfo[Index].LinkKey.Link_Key12, 
                        			g_dy_cfg.bt_dy.LinkKeyInfo[Index].LinkKey.Link_Key11, g_dy_cfg.bt_dy.LinkKeyInfo[Index].LinkKey.Link_Key10, g_dy_cfg.bt_dy.LinkKeyInfo[Index].LinkKey.Link_Key9, g_dy_cfg.bt_dy.LinkKeyInfo[Index].LinkKey.Link_Key8, 
                        			g_dy_cfg.bt_dy.LinkKeyInfo[Index].LinkKey.Link_Key7, g_dy_cfg.bt_dy.LinkKeyInfo[Index].LinkKey.Link_Key6, g_dy_cfg.bt_dy.LinkKeyInfo[Index].LinkKey.Link_Key5, g_dy_cfg.bt_dy.LinkKeyInfo[Index].LinkKey.Link_Key4, 
                        			g_dy_cfg.bt_dy.LinkKeyInfo[Index].LinkKey.Link_Key3, g_dy_cfg.bt_dy.LinkKeyInfo[Index].LinkKey.Link_Key2, g_dy_cfg.bt_dy.LinkKeyInfo[Index].LinkKey.Link_Key1, g_dy_cfg.bt_dy.LinkKeyInfo[Index].LinkKey.Link_Key0);
                        }
                        else
                        {
                        	BT_VERBOSE(corona_print("Auth FAIL %d\n", Result);)
                        }
                        break;
                    case atPINCodeRequest:
                        /* A pin code request event occurred, first display   */
                        /* the BD_ADD of the remote device requesting the pin.*/
                        BD_ADDRToStr(GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Remote_Device, Callback_BoardStr);
                        BT_VERBOSE(corona_print("\nPINCodeReq: %s\n", Callback_BoardStr);)
                        
                        /* Note the current Remote BD_ADDR that is requesting */
                        /* the PIN Code.                                      */
                        CurrentRemoteBD_ADDR = GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Remote_Device;
                        
                        /* Inform the user that they will need to respond with*/
                        /* a PIN Code Response.                               */
                        BT_VERBOSE(corona_print("PIN Resp\n");)
                        break;
                    case atAuthenticationStatus:
                        /* An authentication status event occurred, display   */
                        /* all relevant information.                          */
                        BD_ADDRToStr(GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Remote_Device, Callback_BoardStr);
                        BT_VERBOSE(corona_print("\nAuth: %d for %s\n", GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Authentication_Event_Data.Authentication_Status, Callback_BoardStr);)
                        
                        /* Flag that there is no longer a current             */
                        /* Authentication procedure in progress.              */
                        //ASSIGN_BD_ADDR(CurrentRemoteBD_ADDR, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00); // moved to SaveConfig
                        
                        BT_VERBOSE(corona_print("CbkParam %d SerPortID %d\n", CallbackParameter, SerialPortID);)
                        
                        /* Check to see if this was performed due to a        */
                        /* connection request.                                */
                        if(CallbackParameter == SerialPortID)
                        {
                            /* Check to see if the authentication process was  */
                            /* successful.                                     */
                            if(!GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Authentication_Event_Data.Authentication_Status)
                            {
                                BT_VERBOSE(corona_print("Req Encryp\n");)
                                Result = GAP_Set_Encryption_Mode(BluetoothStackID, GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Remote_Device, emEnabled, GAP_Event_Callback, CallbackParameter);
                                if(Result)
                                {
                                    BT_VERBOSE(corona_print("Reject Conn %d\n", Result);)
                                    
                                    /* We failed the Encryption Request so do */
                                    /* not allow the connection.              */
                                    ISPP_Open_Port_Request_Response(BluetoothStackID, (unsigned int)CallbackParameter, FALSE, 0, NULL, 0);
                                    
                                    Connecting = FALSE;
                                    
                                    SerialPortID = 0;
                                    
                                    BTMGR_PRIV_pairing_complete(FALSE);
                                    BTMGR_PRIV_connecting_complete(FALSE);
                                }
                            }
                            else
                            {
                                BT_VERBOSE(corona_print("Reject Conn\n");)
                                
                                /* We failed authenticate so do not allow the   */
                                /* connection.                                  */
                                ISPP_Open_Port_Request_Response(BluetoothStackID, (unsigned int)CallbackParameter, FALSE, 0, NULL, 0);
                                
                                Connecting = FALSE;
                                
                                SerialPortID = 0;

                                BTMGR_PRIV_pairing_complete(FALSE);
                                BTMGR_PRIV_connecting_complete(FALSE);
                            }
                        }
                        break;
                    case atLinkKeyCreation:
                        /* A link key creation event occurred, first display  */
                        /* the remote device that caused this event.          */
                        BD_ADDRToStr(GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Remote_Device, Callback_BoardStr);
                        BT_VERBOSE(corona_print("\nLinkKeyCreation: %s\n", Callback_BoardStr);)
                        
                        /* Now store the link Key in either a free location OR*/
                        /* over the old key location.                         */
                        
                        for(Index=0,Result=-1;Index<(sizeof(g_dy_cfg.bt_dy.LinkKeyInfo)/sizeof(LinkKeyInfo_t));Index++)
                        {
                            if(COMPARE_BD_ADDR(g_dy_cfg.bt_dy.LinkKeyInfo[Index].BD_ADDR, GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Remote_Device))
                                break;
                            else
                            {
                                if((Result == (-1)) && (COMPARE_NULL_BD_ADDR(g_dy_cfg.bt_dy.LinkKeyInfo[Index].BD_ADDR)))
                                    Result = Index;
                            }
                        }
                        
                        /* If we didn't find a match, see if we found an empty*/
                        /* location.                                          */
                        if(Index == (sizeof(g_dy_cfg.bt_dy.LinkKeyInfo)/sizeof(LinkKeyInfo_t)))
                            Index = Result;
                        
                        /* Check to see if we found a location to store the   */
                        /* Link Key information into.                         */
                        if(Index != (-1))
                        {
                        	LastPairedIndex = Index;
                            memcpy(&CurrentLinkKey, &GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Authentication_Event_Data.Link_Key_Info.Link_Key, sizeof(Link_Key_t));
                                                        
                            /* Wait to see if we get any iOS authentication events before disconnect */
                            iAPTimerID2 = BSC_StartTimer(BluetoothStackID, IAP_IS_NOT_IOS_DELAY_VALUE, BSC_Timer_Callback, 1);
                            
                            BTMGR_PRIV_pairing_complete(TRUE);
                            
                            BT_VERBOSE(corona_print("LinkKey Stored\n");)
                            corona_print("key 0x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
                                    CurrentLinkKey.Link_Key15, CurrentLinkKey.Link_Key14, CurrentLinkKey.Link_Key13, CurrentLinkKey.Link_Key12, 
                                    CurrentLinkKey.Link_Key11, CurrentLinkKey.Link_Key10, CurrentLinkKey.Link_Key9, CurrentLinkKey.Link_Key8, 
                                    CurrentLinkKey.Link_Key7, CurrentLinkKey.Link_Key6, CurrentLinkKey.Link_Key5, CurrentLinkKey.Link_Key4, 
                                    CurrentLinkKey.Link_Key3, CurrentLinkKey.Link_Key2, CurrentLinkKey.Link_Key1, CurrentLinkKey.Link_Key0);
                        }
                        else
                        {
                            BT_VERBOSE(corona_print("LinkKey full\n");)
                            SerialPortID = 0;
                            BTMGR_PRIV_pairing_complete(FALSE);
                            BTMGR_PRIV_connecting_complete(FALSE);
                        }
                        break;
                    case atIOCapabilityRequest:
                        BD_ADDRToStr(GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Remote_Device, Callback_BoardStr);
                        BT_VERBOSE(corona_print("\nIOCapabilityReq: %s\n", Callback_BoardStr);)
                        
                        /* Setup the Authentication Information Response      */
                        /* structure.                                         */
                        GAP_Authentication_Information.GAP_Authentication_Type                                      = atIOCapabilities;
                        GAP_Authentication_Information.Authentication_Data_Length                                   = sizeof(GAP_IO_Capabilities_t);
                        GAP_Authentication_Information.Authentication_Data.IO_Capabilities.IO_Capability            = (GAP_IO_Capability_t)IOCapability;
                        GAP_Authentication_Information.Authentication_Data.IO_Capabilities.MITM_Protection_Required = MITMProtection;
                        GAP_Authentication_Information.Authentication_Data.IO_Capabilities.OOB_Data_Present         = OOBSupport;
                        
                        /* Submit the Authentication Response.                */
                        Result = GAP_Authentication_Response(BluetoothStackID, GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Remote_Device, &GAP_Authentication_Information);
                        
                        /* Check the result of the submitted command.         */
                        /* Check the result of the submitted command.         */
                        if(!Result)
                        {
                            BT_VERBOSE(corona_print("Auth OK\n");)
                        }
                        else
                        {
                            PROCESS_NINFO(ERR_BT_ERROR, "auth %i", Result);
                            SerialPortID = 0;
                            BTMGR_PRIV_pairing_complete(FALSE);
                            BTMGR_PRIV_connecting_complete(FALSE);
                        }
                        break;
                    case atIOCapabilityResponse:
                        BD_ADDRToStr(GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Remote_Device, Callback_BoardStr);
                        BT_VERBOSE(corona_print("\nIOCapabilityResp: %s\n", Callback_BoardStr);)
                        
                        RemoteIOCapability = GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Authentication_Event_Data.IO_Capabilities.IO_Capability;
                        MITM               = (Boolean_t)GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Authentication_Event_Data.IO_Capabilities.MITM_Protection_Required;
                        OOB_Data           = (Boolean_t)GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Authentication_Event_Data.IO_Capabilities.OOB_Data_Present;
                        
                        break;
                    case atUserConfirmationRequest:
                        BD_ADDRToStr(GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Remote_Device, Callback_BoardStr);
                        BT_VERBOSE(corona_print("\nUsrConfirmReq: %s\n", Callback_BoardStr);)
                        
                        CurrentRemoteBD_ADDR = GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Remote_Device;
                        
                        /* Invoke JUST Works Process...                    */
                        GAP_Authentication_Information.GAP_Authentication_Type          = atUserConfirmation;
                        GAP_Authentication_Information.Authentication_Data_Length       = (Byte_t)sizeof(Byte_t);
                        GAP_Authentication_Information.Authentication_Data.Confirmation = TRUE;
                        
                        /* Submit the Authentication Response.             */
                        BT_VERBOSE(corona_print("\nAutoAccept: %d\n", GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Authentication_Event_Data.Numeric_Value);)
                        
                        Result = GAP_Authentication_Response(BluetoothStackID, GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Remote_Device, &GAP_Authentication_Information);

                        if(!Result)
                        {
                        	BT_VERBOSE(corona_print("Auth OK\n");)
                        }
                        else
                        {
                        	BT_VERBOSE(corona_print("Auth FAIL %d\n", Result);)
                        	SerialPortID = 0;
                        	BTMGR_PRIV_pairing_complete(FALSE);
                        	BTMGR_PRIV_connecting_complete(FALSE);
                        }
                                                
                        /* Flag that there is no longer a current          */
                        /* Authentication procedure in progress.           */
                        //ASSIGN_BD_ADDR(CurrentRemoteBD_ADDR, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00); // Moved to SaveConfig
                        break;
                    case atPasskeyRequest:
                        BD_ADDRToStr(GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Remote_Device, Callback_BoardStr);
                        BT_VERBOSE(corona_print("\nPasskeyReq: %s\n", Callback_BoardStr);)
                        
                        /* Note the current Remote BD_ADDR that is requesting */
                        /* the Passkey.                                       */
                        CurrentRemoteBD_ADDR = GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Remote_Device;
                        
                        /* Inform the user that they will need to respond with*/
                        /* a Passkey Response.                                */
                        BT_VERBOSE(corona_print("PassKeyResp\n");)
                        break;
                    case atRemoteOutOfBandDataRequest:
                        BD_ADDRToStr(GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Remote_Device, Callback_BoardStr);
                        BT_VERBOSE(corona_print("\nRemoteOutOfBandDataReq: %s\n", Callback_BoardStr);)
                        
                        /* This application does not support OOB data so      */
                        /* respond with a data length of Zero to force a      */
                        /* negative reply.                                    */
                        GAP_Authentication_Information.GAP_Authentication_Type    = atOutOfBandData;
                        GAP_Authentication_Information.Authentication_Data_Length = 0;
                        
                        Result = GAP_Authentication_Response(BluetoothStackID, GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Remote_Device, &GAP_Authentication_Information);
                        
                        if(!Result)
                        {
                            BT_VERBOSE(corona_print("Auth OK\n");)
                        }
                        else
                        {
                            BT_VERBOSE(corona_print("Auth FAIL %d\n", Result);)
                            SerialPortID = 0;
                            BTMGR_PRIV_pairing_complete(FALSE);
                            BTMGR_PRIV_connecting_complete(FALSE);
                        }
                        break;
                    case atPasskeyNotification:
                        BD_ADDRToStr(GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Remote_Device, Callback_BoardStr);
                        BT_VERBOSE(corona_print("\nPasskey: %s\n", Callback_BoardStr);)
                        BT_VERBOSE(corona_print("Passkey: %d\n", GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Authentication_Event_Data.Numeric_Value);)
                        break;
                    case atKeypressNotification:
                        BD_ADDRToStr(GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Remote_Device, Callback_BoardStr);
                        BT_VERBOSE(corona_print("\nKeypress: %s\n", Callback_BoardStr);)
                        BT_VERBOSE(corona_print("Keypress: %d\n", (int)GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Authentication_Event_Data.Keypress_Type);)
                        break;
                    case atSecureSimplePairingComplete:
                        BTMGR_PRIV_pairing_started();
                        break;
                    default:
                        BT_VERBOSE(corona_print("Un-Handled Auth Event %x\n", GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->GAP_Authentication_Event_Type);)
                        break;
                }
                break;
            case etEncryption_Change_Result:
                BD_ADDRToStr(GAP_Event_Data->Event_Data.GAP_Encryption_Mode_Event_Data->Remote_Device,Callback_BoardStr);
                BT_VERBOSE(corona_print("\nEncryptChange %s 0x%02X Mode %s\n", Callback_BoardStr,
                        GAP_Event_Data->Event_Data.GAP_Encryption_Mode_Event_Data->Encryption_Change_Status,
                        ((GAP_Event_Data->Event_Data.GAP_Encryption_Mode_Event_Data->Encryption_Mode == emDisabled)?"Disabled": "Enabled"));)
                
                /* Check to see if the encryption was changed do to a       */
                /* connect request.                                         */
                if(CallbackParameter)
                {
                    if(CurrentOperatingMode == pomiAP)
                    {
                    	unsigned int size_FIDInfo;
                        Byte_t *pFIDInfo;

                        BT_VERBOSE(corona_print("Accept iOS\n");)
                        
                        pFIDInfo = (Byte_t *)ModifiedFIDInfoAlloc(&size_FIDInfo);
                        
                        ISPP_Open_Port_Request_Response(BluetoothStackID, (unsigned int)CallbackParameter, TRUE, size_FIDInfo, pFIDInfo, MAX_SMALL_PACKET_PAYLOAD_SIZE);
                    }
                    else
                    {
                        BT_VERBOSE(corona_print("Accept Non-iOS\n");)
                        
                        ISPP_Open_Port_Request_Response(BluetoothStackID, (unsigned int)CallbackParameter, TRUE, 0, NULL, 0);
                    }
                }
                break;
            default:
                /* An unknown/unexpected GAP event was received.            */
                BT_VERBOSE(corona_print("\nUnknown Event: %d\n", GAP_Event_Data->Event_Data_Type);)
                break;
        }
    }
    else
    {
        /* There was an error with one or more of the input parameters.   */
        BT_VERBOSE(corona_print("\nNULL Event\n");)
    }
}

/* The following function is for the SDP Event Receive Data Callback.*/
/* This function will be called whenever a Callback has been         */
/* registered for the specified SDP Action that is associated with   */
/* the Bluetooth Stack.  This function passes to the caller the SDP  */
/* Request ID of the SDP Request, the SDP Response Event Data of the */
/* specified Response Event and the SDP Response Event Callback      */
/* Parameter that was specified when this Callback was installed.    */
/* The caller is free to use the contents of the SDP Event Data ONLY */
/* in the context of this callback.  If the caller requires the Data */
/* for a longer period of time, then the callback function MUST copy */
/* the data into another Data Buffer.  This function is guaranteed   */
/* NOT to be invoked more than once simultaneously for the specified */
/* installed callback (i.e. this function DOES NOT have be           */
/* reentrant).  It Needs to be noted however, that if the same       */
/* Callback is installed more than once, then the callbacks will be  */
/* called serially.  Because of this, the processing in this function*/
/* should be as efficient as possible.  It should also be noted that */
/* this function is called in the Thread Context of a Thread that the*/
/* User does NOT own.  Therefore, processing in this function should */
/* be as efficient as possible (this argument holds anyway because   */
/* other SDP Events will not be processed while this function call is*/
/* outstanding).                                                     */
/* * NOTE * This function MUST NOT Block and wait for events that    */
/*          can only be satisfied by Receiving other SDP Events.  A  */
/*          Deadlock WILL occur because NO SDP Event Callbacks will  */
/*          be issued while this function is currently outstanding.  */
static void BTPSAPI SDP_Event_Callback(unsigned int BluetoothStackID, unsigned int SDPRequestID, SDP_Response_Data_t *SDP_Response_Data, unsigned long CallbackParameter)
{
    int                                           Port;
    unsigned int                                  _SerialPortID;
    SDP_Data_Element_t                           *SDE;
    GAP_Encryption_Mode_t                         EncryptionMode;
    SDP_Service_Attribute_Response_Data_t        *SAR;
    SDP_Service_Search_Attribute_Response_Data_t *SSAR;
    _mqx_max_type msg[MSG_SIZE_SDPREQ_Q];  
    
    /* First, check to see if the required parameters appear to be       */
    /* semi-valid.                                                       */
    wassert((SDP_Response_Data != NULL) && (BluetoothStackID));
    
    /* Obtain the Serial Port ID that this result is associated with. */
    _SerialPortID = (unsigned int)CallbackParameter;
    
    /* Set the port number to an invalid value.                       */
    Port          = -1;
    
    /* The parameters appear to be semi-valid, now check to see what  */
    /* type the incoming Event is.                                    */
    switch(SDP_Response_Data->SDP_Response_Data_Type)
    {
        case rdTimeout:
            /* A SDP Timeout was received, display a message indicating */
            /* this.                                                    */
            BT_VERBOSE(corona_print("\nSDP T.O. RX'd (Sz=0x%04X)\n", sizeof(SDP_Response_Data_t));)
            
            /* Error, if this was during a connection, go ahead and     */
            /* reject it.                                               */
            if(_SerialPortID == SerialPortID)
            {
                ISPP_Open_Port_Request_Response(BluetoothStackID, _SerialPortID, FALSE, 0, NULL, 0);
                
                Connecting = FALSE;
                
                SerialPortID = 0;

                BTMGR_PRIV_connecting_complete(FALSE);
            }
            break;
        case rdConnectionError:
            /* A SDP Connection Error was received, display a message   */
            /* indicating this.                                         */
            /* PROCESS_NINFO(ERR_BT_SDP_CONN, NULL); */
        	BT_VERBOSE(corona_print("SDP rdConnectionError %d %d\n", SerialPortID, _SerialPortID);)
            
            /* Error, if this was during a connection, go ahead and     */
            /* reject it.                                               */
            if(_SerialPortID == SerialPortID)
            {
                ISPP_Open_Port_Request_Response(BluetoothStackID, _SerialPortID, FALSE, 0, NULL, 0);
                
                Connecting = FALSE;
                
                SerialPortID = 0;

                BTMGR_PRIV_connecting_complete(FALSE);
            }
            break;
        case rdErrorResponse:
            /* A SDP error response was received, display all relevant  */
            /* information regarding this event.                        */
            PROCESS_NINFO(ERR_BT_SDP_RESP, "EC: %d", SDP_Response_Data->SDP_Response_Data.SDP_Error_Response_Data.Error_Code);
            
            /* Error, if this was during a connection, go ahead and     */
            /* reject it.                                               */
            if(_SerialPortID == SerialPortID)
            {
                ISPP_Open_Port_Request_Response(BluetoothStackID, _SerialPortID, FALSE, 0, NULL, 0);
                
                Connecting = FALSE;
                
                SerialPortID = 0;

                BTMGR_PRIV_connecting_complete(FALSE);
            }
            break;
        case rdServiceSearchAttributeResponse:
            /* A SDP Service Search Attribute Response was received,    */
            /* display all relevant information regarding this event    */

        	/* First, get a pointer to the Service Search Attribute     */
            /* Response Data.                                           */
            SSAR = &(SDP_Response_Data->SDP_Response_Data.SDP_Service_Search_Attribute_Response_Data);
            if(SSAR)
            {
                BT_VERBOSE(corona_print("Case: %d: %d Recs\n", rdServiceSearchAttributeResponse, SSAR->Number_Service_Records);)
                if(SSAR->Number_Service_Records)
                {
                    /* Make sure that we have   */
                    /* the information on Attribute (Protocol Descriptor  */
                    /* List) - 0x0004.                                    */
                    SAR = SSAR->SDP_Service_Attribute_Response_Data;
                    if(SAR->SDP_Service_Attribute_Value_Data->Attribute_ID == SDP_ATTRIBUTE_ID_PROTOCOL_DESCRIPTOR_LIST)
                    {
                        /* This is the information for Attribute 0x0004, so*/
                        /* now make sure that there are 2 Data Element     */
                        /* Sequences.                                      */
                        SDE = SAR->SDP_Service_Attribute_Value_Data->SDP_Data_Element;
                        if((SDE->SDP_Data_Element_Type == deSequence) && (SDE->SDP_Data_Element_Length >= 2))
                        {
                            /* The first Data Element Sequence is           */
                            /* information about L2CAP, so move to the      */
                            /* second Data Element Sequence for information */
                            /* about RFCOMM.                                */
                            SDE = &SDE->SDP_Data_Element.SDP_Data_Element_Sequence[1];
                            if((SDE->SDP_Data_Element_Type == deSequence) && (SDE->SDP_Data_Element_Length >= 2))
                            {
                                /* The first element indicates RFCOMM, so    */
                                /* look at the second element to find the    */
                                /* Port Number that is being used.           */
                                SDE = &SDE->SDP_Data_Element.SDP_Data_Element_Sequence[1];
                                if((SDE->SDP_Data_Element_Type == deUnsignedInteger1Byte) || (SDE->SDP_Data_Element_Type == deUnsignedInteger2Bytes) || (SDE->SDP_Data_Element_Type == deUnsignedInteger4Bytes))
                                {
                                    /* Save the Port number of the remote */
                                    /* Server.  This port number is Unique for*/
                                    /* this device.                       */
                                    if(SDE->SDP_Data_Element_Type == deUnsignedInteger1Byte)
                                        Port = (int)SDE->SDP_Data_Element.UnsignedInteger1Byte;
                                    else
                                    {
                                        if(SDE->SDP_Data_Element_Type == deUnsignedInteger2Bytes)
                                            Port = (int)SDE->SDP_Data_Element.UnsignedInteger2Bytes;
                                        else
                                            Port = (int)SDE->SDP_Data_Element.UnsignedInteger4Bytes;
                                    }
                                    
                                    /* When connecting to a remote server, we initiate the SDP request with CallbackParameter == 0. */
                                    /* In this case, we already know CurrentOperatingMode and don't want to affect it.
                                    /* When responding to a a remote client (we are server), CallbackParameter = SerialPortID != 0. */
                                    /* In this case, we don't yet know CurrentOperatingMode and determine it here. */
                                    if (0 != CallbackParameter)
                                    {
                                    	CurrentOperatingMode = pomiAP;
                                        if(iAPTimerID2)
                                            BSC_StopTimer(BluetoothStackID, iAPTimerID2);
                                        
                                        iAPTimerID2 = 0;

                                        g_dy_cfg.bt_dy.LinkKeyInfo[LastPairedIndex].pom = pomiAP;
                                        CFGMGR_flush( CFG_FLUSH_DYNAMIC );
                                    }
                                    
                                    BT_VERBOSE(corona_print("Port %d %d\n", Port, CurrentOperatingMode);)
                                }
                            }
                        }
                    }
                }
            }
            
            if(_SerialPortID == SerialPortID)
            {
                /* Before we accept the connection we need to check to   */
                /* see if the link is encrypted.  If not, then we will   */
                /* authenticate and encrypt before we accept the         */
                /* connection.                                           */
                if(!GAP_Query_Encryption_Mode(BluetoothStackID, CurrentRemoteBD_ADDR, &EncryptionMode))
                {
                    /* Check to see if we are encrypted.                  */
                    if(EncryptionMode == emEnabled)
                    {
                        /* Check to see if we located the iOS WiAP Server  */
                        /* Port.  If so, the acceopt the connection.  If   */
                        /* not, then refuse the connection.                */
                        if(CurrentOperatingMode == pomiAP)
                        {
                        	unsigned int size_FIDInfo;
                        	Byte_t *pFIDInfo;
                        	
                            BT_VERBOSE(corona_print("Accept iOS\n");)
                            
                            pFIDInfo = (Byte_t *)ModifiedFIDInfoAlloc(&size_FIDInfo);
                            
                            ISPP_Open_Port_Request_Response(BluetoothStackID, SerialPortID, TRUE, size_FIDInfo, pFIDInfo, MAX_SMALL_PACKET_PAYLOAD_SIZE);
                        }
                        else
                        {
                            BT_VERBOSE(corona_print("Accept Non-iOS\n");)
                            
                            ISPP_Open_Port_Request_Response(BluetoothStackID, SerialPortID, TRUE, 0, NULL, 0);
                        }
                    }
                    else
                    {
                        /* Request to be authenticated.                    */
                        if(GAP_Authenticate_Remote_Device(BluetoothStackID, CurrentRemoteBD_ADDR, GAP_Event_Callback, SerialPortID))
                        {
                            /* Failure.  Go ahead and disconnect.           */
                            ISPP_Open_Port_Request_Response(BluetoothStackID, SerialPortID, FALSE, 0, NULL, 0);
                            
                            Connecting = FALSE;
                            /* 
                             * CA - I added this, thinking I needed it in order not to miss a failure.
                             * However, it looks like this is not fatal as the connection proceeds from here
                             * OK. Note, we already have the Port Number by now, so that's all we care about anyway.

                            SerialPortID = 0;

                            BTMGR_connecting_complete(FALSE);
                            */
                        }
                    }
                }
                else
                {
                    /* We failed to request authentication so do not allow*/
                    /* the conneciton.                                    */
                    ISPP_Open_Port_Request_Response(BluetoothStackID, SerialPortID, FALSE, 0, NULL, 0);
                    
                    Connecting = FALSE;
                    /* 
                     * CA - I added this, thinking I needed it in order not to miss a failure.
                     * However, it looks like this is not fatal as the connection proceeds from here
                     * OK. Note, we already have the Port Number by now, so that's all we care about anyway.

                    SerialPortID = 0;

                    BTMGR_connecting_complete(FALSE);
                    */                
                }
            }
            break;
        default:
            /* An unknown/unexpected SDP event was received.            */
            PROCESS_NINFO( ERR_BT_SDP_RESP, "Unknown SDP: %d", SDP_Response_Data->SDP_Response_Data_Type );
            
            /* If this was during a connection, go ahead and reject it. */
            if(_SerialPortID == SerialPortID)
            {
                ISPP_Open_Port_Request_Response(BluetoothStackID, _SerialPortID, FALSE, 0, NULL, 0);
                
                Connecting = FALSE;
                
                SerialPortID = 0;

                BTMGR_PRIV_connecting_complete(FALSE);
            }
            break;
    }
    
    if (!Connecting || Port > 0)
    {
    	/* Done. Notify waiter */
    	/* Notify SDPRequest */
    	msg[0] = (_mqx_max_type)Port;

    	_lwmsgq_send((pointer)SDPRequestQueue, msg, 0);
    }
}

/* The following function is for an iSPP Event Callback.  This       */
/* function will be called whenever a iSPP Event occurs that is      */
/* associated with the Bluetooth Stack.  This function passes to the */
/* caller the iSPP Event Data that occurred and the iSPP Event       */
/* Callback Parameter that was specified when this Callback was      */
/* installed.  The caller is free to use the contents of the iSPP    */
/* iSPP Event Data ONLY in the context of this callback.  If the     */
/* caller requires the Data for a longer period of time, then the    */
/* callback function MUST copy the data into another Data Buffer.    */
/* This function is guaranteed NOT to be invoked more than once      */
/* simultaneously for the specified installed callback (i.e. this    */
/* function DOES NOT have be reentrant).  It Needs to be noted       */
/* however, that if the same Callback is installed more than once,   */
/* then the callbacks will be called serially.  Because of this, the */
/* processing in this function should be as efficient as possible.   */
/* It should also be noted that this function is called in the Thread*/
/* Context of a Thread that the User does NOT own.  Therefore,       */
/* processing in this function should be as efficient as possible    */
/* (this argument holds anyway because another iSPP Event will not be*/
/* processed while this function call is outstanding).               */
/* * NOTE * This function MUST NOT Block and wait for Events that    */
/*          can only be satisfied by Receiving iSPP Event Packets.  A*/
/*          Deadlock WILL occur because NO iSPP Event Callbacks will */
/*          be issued while this function is currently outstanding.  */
static void BTPSAPI ISPP_Event_Callback(unsigned int BluetoothStackID, ISPP_Event_Data_t *ISPP_Event_Data, unsigned long CallbackParameter)
{
    int                           ret_val = 0;
    int                           Index;
    int                           Index1;
    int                           TempLength;
    Byte_t                        Status;
    Word_t                        DataLength;
    Boolean_t                     Done;
    unsigned short                SessionID;
    SDP_UUID_Entry_t              UUID_Entry;
    SDP_Attribute_ID_List_Entry_t AttributeID;
    _mqx_max_type                 rmsg[MSG_SIZE_RCV_Q];  
    _mqx_max_type                 smsg[MSG_SIZE_SND_Q];  
    Byte_t                        CommandStatus;
    
    /* **** SEE ISPPAPI.H for a list of all possible event types.  This  */
    /* program only services its required events.                   **** */
    
    /* First, check to see if the required parameters appear to be       */
    /* semi-valid.                                                       */
    if((ISPP_Event_Data) && (BluetoothStackID))
    {
        /* The parameters appear to be semi-valid, now check to see what  */
        /* type the incoming event is.                                    */
        switch(ISPP_Event_Data->Event_Data_Type)
        {
            case etIPort_Open_Request_Indication:
                /* A remote port is requesting a connection.                */
                BD_ADDRToStr(ISPP_Event_Data->Event_Data.ISPP_Open_Port_Request_Indication_Data->BD_ADDR, Callback_BoardStr);
                BT_VERBOSE(corona_print("\nCase: %d: 0x%04X, Brd: %s\n", ISPP_Event_Data->Event_Data_Type, ISPP_Event_Data->Event_Data.ISPP_Open_Port_Request_Indication_Data->SerialPortID, Callback_BoardStr);)
                
                /* Verify that we can accept this connection.               */
                if((!Connected) && (!Connecting))
                {
                    Connecting                     = TRUE;
                    CurrentOperatingMode           = pomSPP;
                    CurrentRemoteBD_ADDR           = ISPP_Event_Data->Event_Data.ISPP_Open_Port_Request_Indication_Data->BD_ADDR;
                    SerialPortID                   = ISPP_Event_Data->Event_Data.ISPP_Open_Port_Request_Indication_Data->SerialPortID;
                    
                    /* Perform an SDP Search to locate the APPLE Device UUID */
                    /* that will identify the connecting device as an Apple  */
                    /* Device.                                               */
                    AttributeID.Attribute_Range    = FALSE;
                    AttributeID.Start_Attribute_ID = SDP_ATTRIBUTE_ID_PROTOCOL_DESCRIPTOR_LIST;
                    
                    UUID_Entry.SDP_Data_Element_Type = deUUID_128;
                    SDP_ASSIGN_APPLE_DEVICE_UUID(UUID_Entry.UUID_Value.UUID_128);
                    
                    ret_val = SDP_Service_Search_Attribute_Request(BluetoothStackID, CurrentRemoteBD_ADDR, 1, &UUID_Entry, 1, &AttributeID, SDP_Event_Callback, SerialPortID);
                    if(ret_val < 0)
                    {
                        PROCESS_NINFO(ERR_BT_SDP_RESP, NULL);
                        
                        ISPP_Open_Port_Request_Response(BluetoothStackID, SerialPortID, FALSE, 0, NULL, 0);
                        
                        /* Flag that we are no longer in the connecting state. */
                        Connecting = FALSE;
                        
                        SerialPortID = 0;

                        BTMGR_PRIV_pairing_complete(FALSE);
                    }
                    else
                    {
                        /* Service Discovery submitted successfully.          */
                        BT_VERBOSE(corona_print("SDP Attr Req OK\n\n");)
                        
                        ret_val = 0;
                    }
                }
                else
                    ISPP_Open_Port_Request_Response(BluetoothStackID, SerialPortID, FALSE, 0, NULL, 0);
                break;
            case etIPort_Open_Indication:
                /* A remote port is requesting a connection.                */
                BD_ADDRToStr(ISPP_Event_Data->Event_Data.ISPP_Open_Port_Indication_Data->BD_ADDR, Callback_BoardStr);
                BT_VERBOSE(corona_print("\nCase: %d: 0x%04X, Brd: %s\n", ISPP_Event_Data->Event_Data_Type, ISPP_Event_Data->Event_Data.ISPP_Open_Port_Indication_Data->SerialPortID, Callback_BoardStr);)
                
                /* Save the Serial Port ID for later use.                   */
                SerialPortID = ISPP_Event_Data->Event_Data.ISPP_Open_Port_Indication_Data->SerialPortID;
                
                /* Start a timer to wait for Authentication to start.       */
                iAPTimerID = BSC_StartTimer(BluetoothStackID, IAP_START_DELAY_VALUE, BSC_Timer_Callback, 0);
                
                /* Flag that we are now connected.                          */
                Connected    = TRUE;
                
                /* Flag that there are no sessions active.                  */
                iAPSessionID     = 0;
                
                /* Query the connection handle.                             */
                ret_val = GAP_Query_Connection_Handle(BluetoothStackID,ISPP_Event_Data->Event_Data.ISPP_Open_Port_Indication_Data->BD_ADDR,&Connection_Handle);
                if(ret_val)
                {
                    /* Failed to Query the Connection Handle.                */
                    PROCESS_NINFO(ERR_BT_ERROR, "gapQry RC %i", ret_val);
                    
                    ret_val           = 0;
                    Connection_Handle = 0;
                }
                break;
            case etIPort_Open_Confirmation:
                /* A Client Port was opened.  The Status indicates the      */
                /* Status of the Open.                                      */
                BT_VERBOSE(corona_print("\nCase: %d 0x%04X: 0x%04X\n", ISPP_Event_Data->Event_Data_Type, ISPP_Event_Data->Event_Data.ISPP_Open_Port_Confirmation_Data->SerialPortID,
                        ISPP_Event_Data->Event_Data.ISPP_Open_Port_Confirmation_Data->PortOpenStatus);)
                
                /* Check the Status to make sure that an error did not      */
                /* occur.                                                   */
                if(ISPP_Event_Data->Event_Data.ISPP_Open_Port_Confirmation_Data->PortOpenStatus)
                {
                    /* An error occurred while opening the Serial Port so    */
                    /* invalidate the Serial Port ID.                        */
                    SerialPortID      = 0;
                    Connection_Handle = 0;
                    iAPSessionID      = 0;
                    
                    /* Flag that we are no longer connected.                 */
                    Connected         = FALSE;
                    Connecting        = FALSE;
                    
                    BTMGR_PRIV_connecting_complete(FALSE);
                }
                else
                {
                    /* Flag that we are now connected.                       */
                    Connected    = TRUE;
                    Connecting   = FALSE;
                    
                    /* Start a timer to wait for Authentication to start.    */
                    iAPTimerID = BSC_StartTimer(BluetoothStackID, IAP_START_DELAY_VALUE, BSC_Timer_Callback, 0);
                    
                    /* Flag that there are no sessions active.               */
                    iAPSessionID     = 0;
                    
                    /* Query the connection Handle.                          */
                    ret_val = GAP_Query_Connection_Handle(BluetoothStackID,SelectedBD_ADDR,&Connection_Handle);
                    if(ret_val)
                    {
                        /* Failed to Query the Connection Handle.                */
                        BT_VERBOSE(corona_print("GAP_Query_Conn_Hdl FAIL %d\n", ret_val);)
                        
                        ret_val           = 0;
                        Connection_Handle = 0;
                    }
                }
                break;
            case etIPort_Close_Port_Indication:
                /* The Remote Port was Disconnected.                        */
                BT_VERBOSE(corona_print("\nCase: %d 0x%04X\n", ISPP_Event_Data->Event_Data_Type, ISPP_Event_Data->Event_Data.ISPP_Close_Port_Indication_Data->SerialPortID);)
                
                if(iAPTimerID)
                    BSC_StopTimer(BluetoothStackID, iAPTimerID);
                
                if(iAPTimerID2)
                    BSC_StopTimer(BluetoothStackID, iAPTimerID2);
                
                /* Invalidate the Serial Port ID.                           */
                SerialPortID      = 0;
                Connection_Handle = 0;
                iAPSessionID      = 0;
                iAPTimerID        = 0;
                iAPTimerID2       = 0;
                
                /* Flag that we are no longer connected.                    */
                Connected  = FALSE;
                Connecting = FALSE;
                
                /* In case we're still waiting for initial session to come up */
                BTMGR_PRIV_connecting_complete(FALSE);

                /* Unblock any waiters */
                NotifyDisconnect();
                break;
            case etIPort_Status_Indication:
                /* Display Information about the new Port Status.           */
                BT_VERBOSE(corona_print("\nCase: %d 0x%04X: 0x%04X, Break: 0x%04X len: 0x%04X\n", ISPP_Event_Data->Event_Data_Type, ISPP_Event_Data->Event_Data.ISPP_Port_Status_Indication_Data->SerialPortID,
                        ISPP_Event_Data->Event_Data.ISPP_Port_Status_Indication_Data->PortStatus,
                        ISPP_Event_Data->Event_Data.ISPP_Port_Status_Indication_Data->BreakStatus,
                        ISPP_Event_Data->Event_Data.ISPP_Port_Status_Indication_Data->BreakTimeout);)
                
                break;
            case etIPort_Data_Indication:
            	// TODO: Might need to buffer this data rather than notifying reader one by one
            	// but the advantage of this approach is that it's zero copy.
            	if (pomiAP != CurrentOperatingMode)
            	{
            		rmsg[0] = (_mqx_max_type)ISPP_Event_Data->Event_Data.ISPP_Data_Indication_Data->SerialPortID;
            		rmsg[1] = (_mqx_max_type)ISPP_Event_Data->Event_Data.ISPP_Data_Indication_Data->DataLength;

            		BT_VERBOSE(corona_print("ISPP Port Data: %d len: %d\n", rmsg[0], rmsg[1]);)

            		/* Notify receiver */
            		_lwmsgq_send((pointer)ReceiveSessionEventQueue, rmsg, 0);
            	}

                break;
            case etIPort_Transmit_Buffer_Empty_Indication:
            	_lwevent_set(&g_btmgrsh_lwevent, TRANSMIT_BUFFER_EMPTY_EVENT);
                break;
            case etIPort_Send_Port_Information_Indication:
                /* Simply Respond with the Information that was sent to us. */
                ret_val = ISPP_Respond_Port_Information(BluetoothStackID, ISPP_Event_Data->Event_Data.ISPP_Send_Port_Information_Indication_Data->SerialPortID, &ISPP_Event_Data->Event_Data.ISPP_Send_Port_Information_Indication_Data->SPPPortInformation);
                break;
                
                /* All the events below this are special iOS events ONLY.   */
                
            case etIPort_Process_Status:
                Status = ISPP_Event_Data->Event_Data.ISPP_Process_Status->Status;
                
                switch(ISPP_Event_Data->Event_Data.ISPP_Process_Status->ProcessState)
                {
                    case psStartIdentificationRequest:
                        BT_VERBOSE(corona_print("Start ID Req(%d) %d\n", SerialPortID, ISPP_Event_Data->Event_Data.ISPP_Process_Status->Status);)
                        
                        if(iAPTimerID)
                        {
                            BSC_StopTimer(BluetoothStackID, iAPTimerID);
                            
                            iAPTimerID = 0;
                        }
                        
                        ret_val = ISPP_Cancel_Authorization(BluetoothStackID, ISPP_Event_Data->Event_Data.ISPP_Process_Status->SerialPortID);
                        
                        if(ret_val < 0)
                            BT_VERBOSE(corona_print("ISPP_Cancel_Auth(%d) RC %d\n", SerialPortID, ret_val);)
                        
                        ret_val = ISPP_Start_Authorization(BluetoothStackID, SerialPortID);
                        
                        BT_VERBOSE(corona_print("ISPP_Start_Auth(%d) RC %d\n", SerialPortID, ret_val);)
                        break;
                    case psStartIdentificationProcess:
                        BT_VERBOSE(corona_print("Start ID Proc (%d) %d\n", SerialPortID, ISPP_Event_Data->Event_Data.ISPP_Process_Status->Status);)
                        
                        if(iAPTimerID)
                        {
                            BSC_StopTimer(BluetoothStackID, iAPTimerID);
                            
                            iAPTimerID = 0;
                        }
                        break;
                    case psIdentificationProcess:
                        BT_VERBOSE(corona_print("ID Proc(%d) %d\n", SerialPortID, ISPP_Event_Data->Event_Data.ISPP_Process_Status->Status);)
                        break;
                    case psIdentificationProcessComplete:
                        BT_VERBOSE(corona_print("ID Proc done (%d) %d\n", SerialPortID, ISPP_Event_Data->Event_Data.ISPP_Process_Status->Status);)
                        break;
                    case psStartAuthenticationProcess:
                        BT_VERBOSE(corona_print("Auth Proc (%d) %d\n", SerialPortID, ISPP_Event_Data->Event_Data.ISPP_Process_Status->Status);)
                        break;
                    case psAuthenticationProcess:
                        BT_VERBOSE(corona_print("Auth Proc (%d) %d", SerialPortID, ISPP_Event_Data->Event_Data.ISPP_Process_Status->Status);)
                        break;
                    case psAuthenticationProcessComplete:
                        BT_VERBOSE(corona_print("Auth Proc done (%d) %s\n", SerialPortID, ISPP_Event_Data->Event_Data.ISPP_Process_Status->Status);)
                        
                        if(!(ISPP_Event_Data->Event_Data.ISPP_Process_Status->Status))
                        {
                            /* Send a request to determine the Max Packet   */
                            /* Size that is supported by the remote device. */
                            /* * NOTE * We are actually authenticated at    */
                            /*          this point but we won't flag it     */
                            /*          complete or dispatch a result to the*/
                            /*          upper layer until we determine the  */
                            /*          Max Packet size that is supported on*/
                            /*          this channel.                       */
                            maxPacketSuccess = FALSE;
                            ISPP_Send_Raw_Data(BluetoothStackID, SerialPortID, IAP_LINGO_ID_GENERAL, IAP_GENERAL_LINGO_REQUEST_TRANSPORT_MAX_PACKET_SIZE, 0, 0, NULL);
                            
                            /* Request to launch application.               */
                            /* Launch application with ignore/allow prompt  */
                            /* if button has been pressed. Otherwise, launch*/
                            /* silently.                                    */
#ifdef BT_AUTO_LAUNCH_APP
                            if(pairMode)
                            {
                                BTMGR_PRIV_launch_application();
                                BTMGR_PRIV_set_pair(FALSE);
                            }
#endif
                        }
                        break;
                }
                
                /* Recover from an error.                                */
                if((Status == IAP_PROCESS_STATUS_TIMEOUT_HALTING) | (Status == IAP_PROCESS_STATUS_GENERAL_FAILURE) | (Status == IAP_PROCESS_STATUS_PROCESS_FAILURE))
                {
                    Connecting   = FALSE;
                    Connected    = FALSE;
                    SerialPortID = 0;
                    
                    ISPP_Close_Port(BluetoothStackID, ISPP_Event_Data->Event_Data.ISPP_Process_Status->SerialPortID);
                    BTMGR_PRIV_pairing_complete(FALSE);
                    BTMGR_PRIV_connecting_complete(FALSE);
                }
                break;
            case etIPort_Open_Session_Indication:
            	SessionID = ISPP_Event_Data->Event_Data.ISPP_Session_Open_Indication->SessionID;

            	BT_VERBOSE(corona_print("ISPP Port Open: %d\n", SessionID);)

            	ISPP_Open_Session_Request_Response(BluetoothStackID, SerialPortID, SessionID, TRUE);

            	iAPSessionID     = SessionID;
            	
            	/* Gotta make sure there's nothing stale from a previoius session */
            	EmptyQueuesClearEvents();
            	
        		BTMGR_PRIV_connecting_complete(TRUE); // This is where we have a "connection" we can use in the ios world
            	break;
            case etIPort_Close_Session_Indication:
            	BT_VERBOSE(corona_print("ISPP Port Close: %d\n", ISPP_Event_Data->Event_Data.ISPP_Session_Close_Indication->SessionID);)

            	iAPSessionID = 0;

                /* Unblock any waiters */
                NotifyDisconnect();

            	break;
            case etIPort_Session_Data_Indication:
            	// TODO: Might need to buffer this data rather than notifying reader one by one
            	// but the advantage of this approach is that it's zero copy.
                rmsg[0] = (_mqx_max_type)ISPP_Event_Data->Event_Data.ISPP_Session_Data_Indication->SessionID;
            	rmsg[1] = (_mqx_max_type)ISPP_Event_Data->Event_Data.ISPP_Session_Data_Indication->DataLength;
            	rmsg[2] = (_mqx_max_type)ISPP_Event_Data->Event_Data.ISPP_Session_Data_Indication->DataPtr;

            	/* Don't mark it consumed until the upper layer consumes it. Note this will delay the
            	 * ACK to iOS.
            	 */
            	ISPP_Event_Data->Event_Data.ISPP_Session_Data_Indication->PacketConsumed = FALSE;

            	//corona_print("ISPP Port Data: %d len: %d\n", rmsg[0], rmsg[1]);

            	/* Notify receiver */
            	_lwmsgq_send((pointer)ReceiveSessionEventQueue, rmsg, 0);

            	break;
            case etIPort_Send_Session_Data_Confirmation:
            	smsg[0] = ISPP_Event_Data->Event_Data.ISPP_Send_Session_Data_Confirmation->SessionID;
            	smsg[1] = ISPP_Event_Data->Event_Data.ISPP_Send_Session_Data_Confirmation->PacketID;
            	smsg[2] = ISPP_Event_Data->Event_Data.ISPP_Send_Session_Data_Confirmation->Status;

            	//corona_print("ISPP Port Data Session %d PktID %d Status %d\n", smsg[0], smsg[1], smsg[2]);
            	
            	/* Notify sender */
    	    	_lwmsgq_send((pointer)SendSessionEventQueue, smsg, 0);
            	break;
            case etIPort_Send_Raw_Data_Confirmation:
            	BT_VERBOSE(corona_print("ISPP Port Send Raw Data PktID %d Status %d\n",
            			ISPP_Event_Data->Event_Data.ISPP_Send_Raw_Data_Confirmation->PacketID,
            			ISPP_Event_Data->Event_Data.ISPP_Send_Raw_Data_Confirmation->Status);)
            	break;
            case etIPort_Raw_Data_Indication:
            	/* Check to see if this is a response to the Max Packet Size*/
            	/* request.                                                 */
            	if((ISPP_Event_Data->Event_Data.ISPP_Raw_Data_Indication->Lingo == IAP_LINGO_ID_GENERAL) && (ISPP_Event_Data->Event_Data.ISPP_Raw_Data_Indication->Command == IAP_GENERAL_LINGO_RETURN_TRANSPORT_MAX_PACKET_SIZE))
            	{
            		MaxPacketData = READ_UNALIGNED_WORD_BIG_ENDIAN(&ISPP_Event_Data->Event_Data.ISPP_Raw_Data_Indication->DataPtr[2]);

            		BT_VERBOSE(corona_print("Max Pkt Sz %d\n", MaxPacketData);)

            		/* Since the Transaction ID and SessionID are counted in */
            		/* the Payload, then the MaxPacketData is 4 bytes less.  */
            		MaxPacketData -= 4;
            		
#ifndef BT_AUTO_LAUNCH_APP
            		SaveConfig();
            		BTMGR_PRIV_ios_pairing_complete();
#else
                    maxPacketSuccess = TRUE;
#endif                    
            	}
            	else
            	{
            		if((ISPP_Event_Data->Event_Data.ISPP_Raw_Data_Indication->Lingo == IAP_LINGO_ID_GENERAL) && (ISPP_Event_Data->Event_Data.ISPP_Raw_Data_Indication->Command == IAP_GENERAL_LINGO_NOTIFY_IPOD_STATE_CHANGE))
            		{
            			Status = READ_UNALIGNED_BYTE_BIG_ENDIAN(&ISPP_Event_Data->Event_Data.ISPP_Raw_Data_Indication->DataPtr[2]);

            			BT_VERBOSE(corona_print("Pwr State Change: %d\n", Status);)
            		}
            		else if((ISPP_Event_Data->Event_Data.ISPP_Raw_Data_Indication->Lingo == IAP_LINGO_ID_GENERAL) && (ISPP_Event_Data->Event_Data.ISPP_Raw_Data_Indication->Command == IAP_GENERAL_LINGO_ACK) && (ISPP_Event_Data->Event_Data.ISPP_Raw_Data_Indication->DataPtr[3] == IAP_GENERAL_LINGO_REQUEST_APPLICATION_LAUNCH))
                    {            		    
            		    CommandStatus = ISPP_Event_Data->Event_Data.ISPP_Raw_Data_Indication->DataPtr[2];
                       
                        if(IAP_ACK_COMMAND_ERROR_SUCCESS == CommandStatus) 
                        {
                            BT_VERBOSE(corona_print("\nApp Launch OK\n");)
                        }
                        else
                        {
                            BT_VERBOSE(corona_print("\nApp Launch FAIL\n");)
                        }                        
                        if( maxPacketSuccess )
                        {
                        	SaveConfig();
                        	BTMGR_PRIV_ios_pairing_complete();
                        }
            		}
            		else
            		{
            			BT_VERBOSE(corona_print("ISPP Raw Data Port: %i\n", ISPP_Event_Data->Event_Data.ISPP_Raw_Data_Indication->SerialPortID);)

            			BT_VERBOSE(corona_print("Lingo\t:%02X", ISPP_Event_Data->Event_Data.ISPP_Raw_Data_Indication->Lingo);)
            			BT_VERBOSE(corona_print("Cmd\t:%02X", ISPP_Event_Data->Event_Data.ISPP_Raw_Data_Indication->Command);)
            			BT_VERBOSE(corona_print("Len\t:%d",   ISPP_Event_Data->Event_Data.ISPP_Raw_Data_Indication->DataLength);)

            			BTPS_DumpData(ISPP_Event_Data->Event_Data.ISPP_Raw_Data_Indication->DataLength, ISPP_Event_Data->Event_Data.ISPP_Raw_Data_Indication->DataPtr);
            			
            	    	PROCESS_NINFO( ERR_BT_DUPLICATE_MSG, NULL );
            	        PWRMGR_Reboot( 500, PV_REBOOT_REASON_BT_FAIL, TRUE );
            		}
            	}
            	break;
            default:
            	/* An unknown/unexpected SPP event was received.            */
            	PROCESS_NINFO(ERR_BT_ERROR, "unknown %d", ISPP_Event_Data->Event_Data_Type);
            	break;
        }

        /* Check the return value of any function that might have been    */
        /* executed in the callback.                                      */
        if(ret_val)
        {
        	/* An error occurred, so output an error message.              */
        	PROCESS_NINFO(ERR_BT_ERROR, "RC %i", ret_val);
        }
    }
    else
    {
    	/* There was an error with one or more of the input parameters.   */
    	BT_VERBOSE(corona_print("NULL Event\n");)
    }
}

/* The following function is responsible for processing HCI Mode     */
/* change events.                                                    */
static void BTPSAPI HCI_Event_Callback(unsigned int BluetoothStackID, HCI_Event_Data_t *HCI_Event_Data, unsigned long CallbackParameter)
{
    char *Mode;
    
    /* Make sure that the input parameters that were passed to us are    */
    /* semi-valid.                                                       */
    if((BluetoothStackID) && (HCI_Event_Data))
    {
        /* Process the Event Data.                                        */
        switch(HCI_Event_Data->Event_Data_Type)
        {
            case etMode_Change_Event:
                if(HCI_Event_Data->Event_Data.HCI_Mode_Change_Event_Data)
                {
                    switch(HCI_Event_Data->Event_Data.HCI_Mode_Change_Event_Data->Current_Mode)
                    {
                        case HCI_CURRENT_MODE_HOLD_MODE:
                            Mode = "Hold";
                            break;
                        case HCI_CURRENT_MODE_SNIFF_MODE:
                            Mode = "Sniff";
                            break;
                        case HCI_CURRENT_MODE_PARK_MODE:
                            Mode = "Park";
                            break;
                        case HCI_CURRENT_MODE_ACTIVE_MODE:
                        default:
                            Mode = "Active";
                            break;
                    }
                    
                    BT_VERBOSE(corona_print("\nHCI chg stat 0x%02X hdl: %d Mode %s, Int %d\n", HCI_Event_Data->Event_Data.HCI_Mode_Change_Event_Data->Status,
                            HCI_Event_Data->Event_Data.HCI_Mode_Change_Event_Data->Connection_Handle,
                            Mode,
                            HCI_Event_Data->Event_Data.HCI_Mode_Change_Event_Data->Interval);)
                }
                break;
            case etDisconnection_Complete_Event:            	
            	if(HCI_Event_Data->Event_Data.HCI_Mode_Change_Event_Data && Connection_Handle == HCI_Event_Data->Event_Data.HCI_Mode_Change_Event_Data->Connection_Handle)
            	{
                	BT_VERBOSE(corona_print("DISCONNECTED\n");)

            		if(iAPTimerID)
            			BSC_StopTimer(BluetoothStackID, iAPTimerID);

            		if(iAPTimerID2)
            			BSC_StopTimer(BluetoothStackID, iAPTimerID2);

            		/* Invalidate the Serial Port ID.                           */
            		SerialPortID      = 0;
            		Connection_Handle = 0;
            		iAPSessionID      = 0;
            		iAPTimerID        = 0;
            		iAPTimerID2       = 0;

            		/* Flag that we are no longer connected.                    */
            		Connected  = FALSE;
            		Connecting = FALSE;

            		_lwevent_set(&g_btmgrsh_lwevent, DISCONNECT_COMPLETE_EVENT);
            		BTMGR_PRIV_disconnected();
            	}
            	break;
            default:
                break;
        }
    }
}

/* The following function is responsible for processing L2CAP Mode     */
/* change events.                                                    */
static void BTPSAPI L2CAP_Event_Callback(unsigned int BluetoothStackID, L2CA_Event_Data_t *L2CAP_Event_Data, unsigned long CallbackParameter)
{   
    _mqx_max_type msg[MSG_SIZE_CT_Q];  

    /* Make sure that the input parameters that were passed to us are    */
    /* semi-valid.                                                       */
    if((BluetoothStackID) && (L2CAP_Event_Data))
    {
		//corona_print("L2C %d\n", L2CAP_Event_Data->L2CA_Event_Type);

        /* Process the Event Data.                                        */
        switch(L2CAP_Event_Data->L2CA_Event_Type)
        {
            case etConnect_Confirmation:
            	if (TEST_CONNECTION == CallbackParameter)
            	{
            		//corona_print("CC %d %x\n", L2CAP_Event_Data->Event_Data.L2CA_Connect_Confirmation->LCID, L2CAP_Event_Data->Event_Data.L2CA_Connect_Confirmation->Result);
            		
                	msg[0] = (_mqx_max_type)L2CAP_Event_Data->Event_Data.L2CA_Connect_Confirmation->LCID;
            		
            		switch (L2CAP_Event_Data->Event_Data.L2CA_Connect_Confirmation->Result)
            		{
            		case L2CAP_CONNECT_RESULT_CONNECTION_SUCCESSFUL:
            			msg[1] = (_mqx_max_type)test_connection_is_connected;
            			_lwmsgq_send((pointer)CTResponseQueue, msg, 0);
            			break;
            		case L2CAP_CONNECT_RESULT_CONNECTION_PENDING:
            			break;
            		default:
            			msg[1] = (_mqx_max_type)test_connection_failed;
            			_lwmsgq_send((pointer)CTResponseQueue, msg, 0);
            			break;
            		}
            	}
            	break;
            case etTimeout_Indication:
            	if (TEST_CONNECTION == CallbackParameter)
            	{
            		//corona_print("TI %d\n", L2CAP_Event_Data->Event_Data.L2CA_Timeout_Indication->LCID);
            		
                	msg[0] = (_mqx_max_type)L2CAP_Event_Data->Event_Data.L2CA_Timeout_Indication->LCID;
                	msg[1] = (_mqx_max_type)test_connection_failed;
                	                	
                	_lwmsgq_send((pointer)CTResponseQueue, msg, 0);
            	}
            	break;
            case etDisconnect_Indication:
            	if (TEST_CONNECTION == CallbackParameter)
            	{
            		//corona_print("DI %d\n", L2CAP_Event_Data->Event_Data.L2CA_Disconnect_Indication->LCID);

            		msg[0] = (_mqx_max_type)L2CAP_Event_Data->Event_Data.L2CA_Disconnect_Indication->LCID;
                	msg[1] = (_mqx_max_type)test_connection_was_connected; /* let's treat this as a success (connected then disconnected?) */
                	                	
                	_lwmsgq_send((pointer)CTResponseQueue, msg, 0);
            	}
            	break;
            case etDisconnect_Confirmation:
            	if (TEST_CONNECTION == CallbackParameter)
            	{
            		//corona_print("DC %d\n", L2CAP_Event_Data->Event_Data.L2CA_Disconnect_Confirmation->LCID);
            		
            		msg[0] = (_mqx_max_type)L2CAP_Event_Data->Event_Data.L2CA_Disconnect_Confirmation->LCID;

            		// Don't care what's in msg[1]. Just need the msg as an event
                	_lwmsgq_send((pointer)CTResponseQueue, msg, 0);
            	}
            	break;
            default:
            	break;
        }
    }
}

