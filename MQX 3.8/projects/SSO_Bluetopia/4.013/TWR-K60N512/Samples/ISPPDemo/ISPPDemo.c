/*****< isppdemo.c >***********************************************************/
/*      Copyright 2011 - 2012 Stonestreet One.                                */
/*      All Rights Reserved.                                                  */
/*                                                                            */
/*  ISPPDEMO - Simple demo application using iSPP Profile.                    */
/*                                                                            */
/*  Author:  Tim Cook                                                         */
/*                                                                            */
/*** MODIFICATION HISTORY *****************************************************/
/*                                                                            */
/*   mm/dd/yy  F. Lastname    Description of Modification                     */
/*   --------  -----------    ------------------------------------------------*/
/*   01/24/11  T. Cook        Initial creation.                               */
/******************************************************************************/

#include <mqx.h>

#include "ISPPDemo.h"            /* Application Header.                       */
#include "HAL.h"                 /* Function for Hardware Abstraction.        */
#include "SS1BTPS.h"             /* Main SS1 BT Stack Header.                 */
#include "BTPSKRNL.h"            /* BTPS Kernel Header.                       */
#include "BTPSVEND.h"            /* Vendor specific command header.           */

#include "SS1BTISP.h"            /* Main SS1 iSPP Header.                     */
#include "IACPTRAN.h"            /* IACP Transport Header.                    */

#define MAX_SUPPORTED_COMMANDS                     (40)  /* Denotes the       */
                                                         /* maximum number of */
                                                         /* User Commands that*/
                                                         /* are supported by  */
                                                         /* this application. */

#define MAX_COMMAND_LENGTH                         (64)  /* Denotes the max   */
                                                         /* buffer size used  */
                                                         /* for user commands */
                                                         /* input via the     */
                                                         /* User Interface.   */

#define MAX_NUM_OF_PARAMETERS                       (5)  /* Denotes the max   */
                                                         /* number of         */
                                                         /* parameters a      */
                                                         /* command can have. */

#define MAX_INQUIRY_RESULTS                        (20)  /* Denotes the max   */
                                                         /* number of inquiry */
                                                         /* results.          */

#define MAX_SUPPORTED_LINK_KEYS                    (1)   /* Max supported Link*/
                                                         /* keys.             */

#define DEFAULT_IO_CAPABILITY          (icDisplayYesNo)  /* Denotes the       */
                                                         /* default I/O       */
                                                         /* Capability that is*/
                                                         /* used with Secure  */
                                                         /* Simple Pairing.   */

#define DEFAULT_MITM_PROTECTION                  (TRUE)  /* Denotes the       */
                                                         /* default value used*/
                                                         /* for Man in the    */
                                                         /* Middle (MITM)     */
                                                         /* protection used   */
                                                         /* with Secure Simple*/
                                                         /* Pairing.          */

#define TEST_DATA              "This is a test string."  /* Denotes the data  */
                                                         /* that is sent via  */
                                                         /* SPP when calling  */
                                                         /* ISPP_Data_Write().*/

#define DEFAULT_LOCAL_DEVICE_NAME   "Wireless iAP (iSPP)"/* Denotes the       */
                                                         /* default device    */
                                                         /* name for the      */
                                                         /* local device.     */

#define LED_TOGGLE_RATE_SUCCESS                    (500) /* The LED Toggle    */
                                                         /* rate when the demo*/
                                                         /* successfully      */
                                                         /* starts up.        */

#define NO_COMMAND_ERROR                           (-1)  /* Denotes that no   */
                                                         /* command was       */
                                                         /* specified to the  */
                                                         /* parser.           */

#define INVALID_COMMAND_ERROR                      (-2)  /* Denotes that the  */
                                                         /* Command does not  */
                                                         /* exist for         */
                                                         /* processing.       */

#define EXIT_CODE                                  (-3)  /* Denotes that the  */
                                                         /* Command specified */
                                                         /* was the Exit      */
                                                         /* Command.          */

#define FUNCTION_ERROR                             (-4)  /* Denotes that an   */
                                                         /* error occurred in */
                                                         /* execution of the  */
                                                         /* Command Function. */

#define TO_MANY_PARAMS                             (-5)  /* Denotes that there*/
                                                         /* are more          */
                                                         /* parameters then   */
                                                         /* will fit in the   */
                                                         /* UserCommand.      */

#define INVALID_PARAMETERS_ERROR                   (-6)  /* Denotes that an   */
                                                         /* error occurred due*/
                                                         /* to the fact that  */
                                                         /* one or more of the*/
                                                         /* required          */
                                                         /* parameters were   */
                                                         /* invalid.          */

#define UNABLE_TO_INITIALIZE_STACK                 (-7)  /* Denotes that an   */
                                                         /* error occurred    */
                                                         /* while Initializing*/
                                                         /* the Bluetooth     */
                                                         /* Protocol Stack.   */

#define INVALID_STACK_ID_ERROR                     (-8)  /* Denotes that an   */
                                                         /* occurred due to   */
                                                         /* attempted         */
                                                         /* execution of a    */
                                                         /* Command when a    */
                                                         /* Bluetooth Protocol*/
                                                         /* Stack has not been*/
                                                         /* opened.           */

#define UNABLE_TO_REGISTER_SERVER                  (-9)  /* Denotes that an   */
                                                         /* error occurred    */
                                                         /* when trying to    */
                                                         /* create a Serial   */
                                                         /* Port Server.      */

#define EXIT_MODE                                  (-10) /* Flags exit from   */
                                                         /* any Mode.         */

#define VENDOR_ID_VALUE                            0x005E/* Bluetooth Company */
                                                         /* Identifier        */

   /* Set the number of milliseconds to delay before attempting to start*/
   /* the iAP process.                                                  */
#define IAP_START_DELAY_VALUE                      (2000)

   /* The following represent the possible values of UI_Mode variable.  */
#define UI_MODE_IS_CLIENT      (1)
#define UI_MODE_IS_SERVER      (0)
#define UI_MODE_IS_INVALID     (-1)

   /* Following converts a Sniff Parameter in Milliseconds to frames.   */
#define MILLISECONDS_TO_BASEBAND_SLOTS(_x)   ((_x) / (0.625))

   /* The following is used as a printf replacement.                    */
#define Display(_x)                   do { BTPS_OutputMessage _x; } while(0)

   /* The following type definition represents the container type which */
   /* holds the mapping between Bluetooth devices (based on the BD_ADDR)*/
   /* and the Link Key (BD_ADDR <-> Link Key Mapping).                  */
typedef struct _tagLinkKeyInfo_t
{
   BD_ADDR_t  BD_ADDR;
   Link_Key_t LinkKey;
} LinkKeyInfo_t;

   /* The following type definition represents the structure which holds*/
   /* all information about the parameter, in particular the parameter  */
   /* as a string and the parameter as an unsigned int.                 */
typedef struct _tagParameter_t
{
   char     *strParam;
   SDWord_t  intParam;
} Parameter_t;

   /* The following type definition represents the structure which holds*/
   /* a list of parameters that are to be associated with a command The */
   /* NumberofParameters variable holds the value of the number of      */
   /* parameters in the list.                                           */
typedef struct _tagParameterList_t
{
   int         NumberofParameters;
   Parameter_t Params[MAX_NUM_OF_PARAMETERS];
} ParameterList_t;

   /* The following type definition represents the structure which holds*/
   /* the command and parameters to be executed.                        */
typedef struct _tagUserCommand_t
{
   char            *Command;
   ParameterList_t  Parameters;
} UserCommand_t;

   /* The following type definition represents the generic function     */
   /* pointer to be used by all commands that can be executed by the    */
   /* test program.                                                     */
typedef int (*CommandFunction_t)(ParameterList_t *TempParam);

   /* The following type definition represents the structure which holds*/
   /* information used in the interpretation and execution of Commands. */
typedef struct _tagCommandTable_t
{
   char              *CommandName;
   CommandFunction_t  CommandFunction;
} CommandTable_t;

   /* Used to hold a BD_ADDR return from BD_ADDRToStr.                  */
typedef char BoardStr_t[15];

   /* The following structure holds status information about a send     */
   /* process.                                                          */
typedef struct _tagSend_Info_t
{
   unsigned int PacketID;
   DWord_t      PendingCount;
   DWord_t      BytesToSend;
   DWord_t      BytesSent;
} Send_Info_t;

   /* Internal Variables to this Module (Remember that all variables    */
   /* declared static are initialized to 0 automatically by the         */
   /* compiler as part of standard C/C++).                              */

static int                   UI_Mode;               /* Holds the UI Mode.              */

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

static Send_Info_t           SendInfo;              /* Variable which holds the        */
                                                    /* current state information for   */
                                                    /* sending session data.           */

static unsigned int          TotalSessionData;      /* Variable which holds the        */
                                                    /* the total number of Session     */
                                                    /* Data bytes that have been       */
                                                    /* received.                       */

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

static BD_ADDR_t             InquiryResultList[MAX_INQUIRY_RESULTS]; /* Variable which */
                                                    /* contains the inquiry result     */
                                                    /* received from the most recently */
                                                    /* preformed inquiry.              */

static unsigned int          NumberofValidResponses;/* Variable which holds the number */
                                                    /* of valid inquiry results within */
                                                    /* the inquiry results array.      */

static LinkKeyInfo_t         LinkKeyInfo[MAX_SUPPORTED_LINK_KEYS]; /* Variable holds   */
                                                    /* BD_ADDR <-> Link Keys for       */
                                                    /* pairing.                        */

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

static Boolean_t             LoopbackActive;        /* Variable which flags whether or */
                                                    /* not the application is currently*/
                                                    /* operating in Loopback Mode      */
                                                    /* (TRUE) or not (FALSE).          */

static Boolean_t             DisplayRawData;        /* Variable which flags whether or */
                                                    /* not the application is to       */
                                                    /* simply display the Raw Data     */
                                                    /* when it is received (when not   */
                                                    /* operating in Loopback Mode).    */

static Boolean_t             AutomaticReadActive;   /* Variable which flags whether or */
                                                    /* not the application is to       */
                                                    /* automatically read all data     */
                                                    /* as it is received.              */

static unsigned int          NumberCommands;        /* Variable which is used to hold  */
                                                    /* the number of Commands that are */
                                                    /* supported by this application.  */
                                                    /* Commands are added individually.*/

static BoardStr_t            Callback_BoardStr;     /* Holds a BD_ADDR string in the   */
                                                    /* Callbacks.                      */

static BoardStr_t            Function_BoardStr;     /* Holds a BD_ADDR string in the   */
                                                    /* various functions.              */

static CommandTable_t        CommandTable[MAX_SUPPORTED_COMMANDS]; /* Variable which is*/
                                                    /* used to hold the actual Commands*/
                                                    /* that are supported by this      */
                                                    /* application.                    */

static char                  Input[MAX_COMMAND_LENGTH];/* Buffer which holds the user  */
                                                    /* input to be parsed.             */

   /* Variables which contain information used by the loopback          */
   /* functionality of this test application.                           */
static unsigned int          BufferLength;

static unsigned char         Buffer[ISPP_FRAME_SIZE_DEFAULT];

static Boolean_t             BufferFull;

   /* The following string table is used to map HCI Version information */
   /* to an easily displayable version string.                          */
static BTPSCONST char *HCIVersionStrings[] =
{
   "1.0b",
   "1.1",
   "1.2",
   "2.0",
   "2.1",
   "3.0",
   "4.0",
   "Unknown (greater 4.0)"
} ;

#define NUM_SUPPORTED_HCI_VERSIONS              (sizeof(HCIVersionStrings)/sizeof(char *) - 1)

   /* The following string table is used to map the API I/O Capabilities*/
   /* values to an easily displayable string.                           */
static BTPSCONST char *IOCapabilitiesStrings[] =
{
   "Display Only",
   "Display Yes/No",
   "Keyboard Only",
   "No Input/Output"
} ;

   /* The following are Test Strings for Process Status values.         */
static BTPSCONST char *ProcessStatus[] =
{
   "Success",
   "Timeout Retrying",
   "Timeout Halting",
   "General Failure",
   "Process Failure",
   "Process Failure Retrying"
} ;

   /* Sample Accessory Identification Information.                      */

   /* The following defines the static information that identifies the  */
   /* accessory and the features that it supports.                      */
   /* * NOTE * The data in this structure is custom for each accessory. */
   /*          The data that is contained in it will need to be updates */
   /*          for each project.                                        */
static BTPSCONST unsigned char FIDInfo[] =
{
   0x0A,                     /* Number of Tokens included.              */
   0x0C,                     /* Identity Token Length                   */
   0x00,0x00,                /* Identity Token IDs                      */
   0x01,                     /* Number of Supported Lingo's             */
   0x00,                     /* Supported Lingos.                       */
   0x00,0x00,0x00,0x02,      /* Device Options                          */
   0x00,0x00,0x02,0x00,      /* Device ID from Authentication Processor */
   0x0A,                     /* AccCap Token Length.                    */
   0x00,0x01,                /* AccCap Token IDs                        */
   0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x00,       /* AccCap Value       */
   0x13,
   0x00,0x02,                /* Acc Info Token IDs                      */
   0x01,                     /* Acc Name                                */
   'A','p','p','l','e',' ','A','c','c','e','s','s','o','r','y',0,
   0x06,
   0x00,0x02,                /* Acc Info Token IDs                      */
   0x04,0x01,0x00,0x00,      /* Acc Firmware Version                    */
   0x06,
   0x00,0x02,                /* Acc Info Token IDs                      */
   0x05,0x01,0x00,0x00,      /* Acc Hardware Version                    */
   0x13,
   0x00,0x02,                /* Acc Info Token IDs                      */
   0x06,                     /* Manufacturer                            */
   'S','t','o','n','e','s','t','r','e','e','t',' ','O','n','e',0,
   0x0D,
   0x00,0x02,                /* Acc Info Token IDs                      */
   0x07,                     /* Model Name                              */
   'B','l','u','e','t','o','p','i','a',0,
   0x05,
   0x00,0x02,                /* Acc Info Token IDs                      */
   0x09,                     /* Max Incomming MTU size                  */
   0x00,0x80,                /* Set MTU to 128                          */
   0x07,
   0x00,0x02,                /* Acc Info Token IDs                      */
   0x0C,0x00,0x00,0x00,0x07, /* Supported Products.                     */
   0x1B,
   0x00,0x04,                /* SDK Protocol Token                      */
   0x01,                     /* Supported Products.                     */
   'c','o','m','.','s','t','o','n','e','s','t','r','e','e','t','o','n','e','.','d','e','m','o',0,
   0x0D,
   0x00,0x05,                /* Bundle Seed ID Pref Token               */
   '1','2','3','4','5','A','B','C','D','E',0
} ;

   /* The following defines a data sequence that will be used to        */
   /* generate session message data.                                    */
static BTPSCONST char SessionData[]  = "~!@#$%^&*()_+`1234567890-=:;\"'<>?,./@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]`abcdefghijklmnopqrstuvwxyz{|}<>\r\n";

   /* GAP Extended Inqiury Response Data.                               */
   /*    - Local Name                                                   */
   /*    - Apple Accessory UUID                                         */
   /*    - Service Class List (SDP, SPP, PNP)                           */
   /*    - Tx Power Level                                               */
static GAP_Extended_Inquiry_Response_Data_Entry_t GAPExtendedInquiryResponseDataEntries[] =
{
   { HCI_EXTENDED_INQUIRY_RESPONSE_DATA_TYPE_LOCAL_NAME_COMPLETE,                 sizeof(DEFAULT_LOCAL_DEVICE_NAME), (Byte_t*)DEFAULT_LOCAL_DEVICE_NAME                                          },
   { HCI_EXTENDED_INQUIRY_RESPONSE_DATA_TYPE_128_BIT_SERVICE_CLASS_UUID_COMPLETE, 16,                                (Byte_t*)"\xFF\xCA\xCA\xDE\xAF\xDE\xCA\xDE\xDE\xFA\xCA\xDE\x00\x00\x00\x00" },
   { HCI_EXTENDED_INQUIRY_RESPONSE_DATA_TYPE_16_BIT_SERVICE_CLASS_UUID_COMPLETE,  6,                                 (Byte_t*)"\x00\x10\x01\x11\x00\x12"                                         },
   { HCI_EXTENDED_INQUIRY_RESPONSE_DATA_TYPE_TX_POWER_LEVEL,                      1,                                 0                                                                  }
} ;

   /* Internal function prototypes.                                     */
static void UserInterface_Client(void);
static void UserInterface_Server(void);
static void UserInterface_Selection(void);
static void GetInput(unsigned int InputBufferLength, char *InputBuffer);
static int  CommandLineInterpreter(void);
static unsigned int StringToUnsignedInteger(char *StringInteger);
static char *StringParser(char *String);
static int CommandParser(UserCommand_t *TempCommand, char *Input);
static int CommandInterpreter(UserCommand_t *TempCommand);
static int AddCommand(char *CommandName, CommandFunction_t CommandFunction);
static CommandFunction_t FindCommand(char *Command);
static void ClearCommands(void);

static void BD_ADDRToStr(BD_ADDR_t Board_Address, char *BoardStr);
static void DisplayIOCapabilities(void);
static void DisplayClassOfDevice(Class_of_Device_t Class_of_Device);
static void DisplayPrompt(void);
static void DisplayUsage(char *UsageString);
static void DisplayFunctionError(char *Function,int Status);
static void DisplayFunctionSuccess(char *Function);

static void *ToggleLED(void *UserParameter);

static int AddDeviceIDInformation(void);

static int OpenStack(HCI_DriverInformation_t *HCI_DriverInformation);
static int CloseStack(void);

static int SetDisc(void);
static int SetConnect(void);
static int SetPairable(void);
static int DeleteLinkKey(BD_ADDR_t BD_ADDR);

static int DisplayHelp(ParameterList_t *TempParam);
static int QueryMemory(ParameterList_t *TempParam);
static int Inquiry(ParameterList_t *TempParam);
static int DisplayInquiryList(ParameterList_t *TempParam);
static int SetDiscoverabilityMode(ParameterList_t *TempParam);
static int SetConnectabilityMode(ParameterList_t *TempParam);
static int SetPairabilityMode(ParameterList_t *TempParam);
static int ChangeSimplePairingParameters(ParameterList_t *TempParam);
static int Pair(ParameterList_t *TempParam);
static int EndPairing(ParameterList_t *TempParam);
static int PINCodeResponse(ParameterList_t *TempParam);
static int PassKeyResponse(ParameterList_t *TempParam);
static int UserConfirmationResponse(ParameterList_t *TempParam);
static int GetLocalAddress(ParameterList_t *TempParam);
static int SetLocalName(ParameterList_t *TempParam);
static int GetLocalName(ParameterList_t *TempParam);
static int SetClassOfDevice(ParameterList_t *TempParam);
static int GetClassOfDevice(ParameterList_t *TempParam);
static int GetRemoteName(ParameterList_t *TempParam);
static int SniffMode(ParameterList_t *TempParam);
static int ExitSniffMode(ParameterList_t *TempParam);

static int OpenServer(ParameterList_t *TempParam);
static int CloseServer(ParameterList_t *TempParam);
static int OpenRemoteServer(ParameterList_t *TempParam);
static int CloseRemoteServer(ParameterList_t *TempParam);
static int Read(ParameterList_t *TempParam);
static int Write(ParameterList_t *TempParam);
static int SendSessionData(ParameterList_t *TempParam);
static int SDPRequest(ParameterList_t *TempParam);
static int GetConfigParams(ParameterList_t *TempParam);
static int SetConfigParams(ParameterList_t *TempParam);
static int GetQueueParams(ParameterList_t *TempParam);
static int SetQueueParams(ParameterList_t *TempParam);
static int Loopback(ParameterList_t *TempParam);
static int DisplayRawModeData(ParameterList_t *TempParam);
static int AutomaticReadMode(ParameterList_t *TempParam);
static int SetBaudRate(ParameterList_t *TempParam);

static int StackChecker(ParameterList_t *TempParam);

static int ServerMode(ParameterList_t *TempParam);
static int ClientMode(ParameterList_t *TempParam);

   /* BTPS Callback function prototypes.                                */
static void BTPSAPI BSC_Timer_Callback(unsigned int BluetoothStackID, unsigned int TimerID, unsigned long CallbackParameter);
static void BTPSAPI GAP_Event_Callback(unsigned int BluetoothStackID, GAP_Event_Data_t *GAP_Event_Data, unsigned long CallbackParameter);
static void BTPSAPI SDP_Event_Callback(unsigned int BluetoothStackID, unsigned int SDPRequestID, SDP_Response_Data_t *SDP_Response_Data, unsigned long CallbackParameter);
static void BTPSAPI ISPP_Event_Callback(unsigned int BluetoothStackID, ISPP_Event_Data_t *ISPP_Event_Data, unsigned long CallbackParameter);
static void BTPSAPI HCI_Event_Callback(unsigned int BluetoothStackID, HCI_Event_Data_t *HCI_Event_Data, unsigned long CallbackParameter);

   /* This function is responsible for taking the input from the user   */
   /* and dispatching the appropriate Command Function.  First, this    */
   /* function retrieves a String of user input, parses the user input  */
   /* into Command and Parameters, and finally executes the Command or  */
   /* Displays an Error Message if the input is not a valid Command.    */
static void UserInterface_Client(void)
{
   int Result = !EXIT_CODE;

   while(Result != EXIT_CODE)
   {
      /* Next display the available commands.                           */
      DisplayHelp(NULL);

      /* Clear the installed command.                                   */
      ClearCommands();

      AddCommand("INQUIRY", Inquiry);
      AddCommand("DISPLAYINQUIRYLIST", DisplayInquiryList);
      AddCommand("PAIR", Pair);
      AddCommand("ENDPAIRING", EndPairing);
      AddCommand("PINCODERESPONSE", PINCodeResponse);
      AddCommand("PASSKEYRESPONSE", PassKeyResponse);
      AddCommand("USERCONFIRMATIONRESPONSE", UserConfirmationResponse);
      AddCommand("SETDISCOVERABILITYMODE", SetDiscoverabilityMode);
      AddCommand("SETCONNECTABILITYMODE", SetConnectabilityMode);
      AddCommand("SETPAIRABILITYMODE", SetPairabilityMode);
      AddCommand("CHANGESIMPLEPAIRINGPARAMETERS", ChangeSimplePairingParameters);
      AddCommand("GETLOCALADDRESS", GetLocalAddress);
      AddCommand("SETLOCALNAME", SetLocalName);
      AddCommand("GETLOCALNAME", GetLocalName);
      AddCommand("SETCLASSOFDEVICE", SetClassOfDevice);
      AddCommand("GETCLASSOFDEVICE", GetClassOfDevice);
      AddCommand("GETREMOTENAME", GetRemoteName);
      AddCommand("SNIFFMODE",SniffMode);
      AddCommand("EXITSNIFFMODE",ExitSniffMode);
      AddCommand("OPEN", OpenRemoteServer);
      AddCommand("CLOSE", CloseRemoteServer);
      AddCommand("READ", Read);
      AddCommand("WRITE", Write);
      AddCommand("SEND", SendSessionData);
      AddCommand("SDPREQUEST", SDPRequest);
      AddCommand("GETCONFIGPARAMS", GetConfigParams);
      AddCommand("SETCONFIGPARAMS", SetConfigParams);
      AddCommand("GETQUEUEPARAMS", GetQueueParams);
      AddCommand("SETQUEUEPARAMS", SetQueueParams);
      AddCommand("LOOPBACK", Loopback);
      AddCommand("DISPLAYRAWMODEDATA", DisplayRawModeData);
      AddCommand("AUTOMATICREADMODE", AutomaticReadMode);
      AddCommand("SETBAUDRATE", SetBaudRate);
      AddCommand("HELP", DisplayHelp);
      AddCommand("QUERYMEMORY", QueryMemory);
      AddCommand("CHECKSTACKS", StackChecker);

      /* Call command line interpreter to parser user input and call the*/
      /* appropriate command function.                                  */
      Result = CommandLineInterpreter();
   }

   if(SerialPortID)
   {
      ISPP_Close_Port(BluetoothStackID,SerialPortID);

      if(iAPTimerID)
         BSC_StopTimer(BluetoothStackID, iAPTimerID);

      SerialPortID = 0;
      iAPTimerID   = 0;
      iAPSessionID = 0;
   }
}

   /* This function is responsible for taking the input from the user   */
   /* and dispatching the appropriate Command Function.  First, this    */
   /* function retrieves a String of user input, parses the user input  */
   /* into Command and Parameters, and finally executes the Command or  */
   /* Displays an Error Message if the input is not a valid Command.    */
static void UserInterface_Server(void)
{
   int Result = !EXIT_CODE;

   while(Result != EXIT_CODE)
   {
      /* Next display the available commands.                           */
      DisplayHelp(NULL);

      /* Clear the installed command.                                   */
      ClearCommands();

      /* Install the commands revelant for this UI.                     */
      AddCommand("INQUIRY", Inquiry);
      AddCommand("DISPLAYINQUIRYLIST", DisplayInquiryList);
      AddCommand("PAIR", Pair);
      AddCommand("ENDPAIRING", EndPairing);
      AddCommand("PINCODERESPONSE", PINCodeResponse);
      AddCommand("PASSKEYRESPONSE", PassKeyResponse);
      AddCommand("USERCONFIRMATIONRESPONSE", UserConfirmationResponse);
      AddCommand("SETDISCOVERABILITYMODE", SetDiscoverabilityMode);
      AddCommand("SETCONNECTABILITYMODE", SetConnectabilityMode);
      AddCommand("SETPAIRABILITYMODE", SetPairabilityMode);
      AddCommand("CHANGESIMPLEPAIRINGPARAMETERS", ChangeSimplePairingParameters);
      AddCommand("GETLOCALADDRESS", GetLocalAddress);
      AddCommand("SETLOCALNAME", SetLocalName);
      AddCommand("GETLOCALNAME", GetLocalName);
      AddCommand("SETCLASSOFDEVICE", SetClassOfDevice);
      AddCommand("GETCLASSOFDEVICE", GetClassOfDevice);
      AddCommand("GETREMOTENAME", GetRemoteName);
      AddCommand("SNIFFMODE",SniffMode);
      AddCommand("EXITSNIFFMODE",ExitSniffMode);
      AddCommand("OPEN", OpenServer);
      AddCommand("CLOSE", CloseServer);
      AddCommand("READ", Read);
      AddCommand("WRITE", Write);
      AddCommand("SEND", SendSessionData);
      AddCommand("SDPREQUEST", SDPRequest);
      AddCommand("GETCONFIGPARAMS", GetConfigParams);
      AddCommand("SETCONFIGPARAMS", SetConfigParams);
      AddCommand("GETQUEUEPARAMS", GetQueueParams);
      AddCommand("SETQUEUEPARAMS", SetQueueParams);
      AddCommand("LOOPBACK", Loopback);
      AddCommand("DISPLAYRAWMODEDATA", DisplayRawModeData);
      AddCommand("AUTOMATICREADMODE", AutomaticReadMode);
      AddCommand("SETBAUDRATE", SetBaudRate);
      AddCommand("QUERYMEMORY", QueryMemory);
      AddCommand("CHECKSTACKS", StackChecker);
      AddCommand("HELP", DisplayHelp);

      /* Call command line interpreter to parser user input and call the*/
      /* appropriate command function.                                  */
      Result = CommandLineInterpreter();
   }

   if(SerialPortID)
   {
      CloseServer(NULL);

      if(iAPTimerID)
         BSC_StopTimer(BluetoothStackID, iAPTimerID);

      SerialPortID = 0;
      iAPTimerID   = 0;
      iAPSessionID = 0;
   }
}

   /* The following function is responsible for choosing the user       */
   /* interface to present to the user.                                 */
static void UserInterface_Selection(void)
{
   /* Next display the available commands.                              */
   DisplayHelp(NULL);

   ClearCommands();

   AddCommand("SERVER", ServerMode);
   AddCommand("CLIENT", ClientMode);
   AddCommand("HELP", DisplayHelp);

   /* Call command line interpreter to parser user input and call the   */
   /* appropriate command function.                                     */
   CommandLineInterpreter();
}

   /* The following function is responsible for retrieving the Commands */
   /* from the Serial Input routines and copying this Command into the  */
   /* specified Buffer.  This function blocks until a Command (defined  */
   /* to be a NULL terminated ASCII string).  The Serial Data Callback  */
   /* is responsible for building the Command and dispatching the Signal*/
   /* that this function waits for.                                     */
static void GetInput(unsigned int InputBufferLength, char *InputBuffer)
{
   char          Char;
   Boolean_t     Done;
   unsigned int  InputIndex;

   /* First let's verify that the input parameters appear to be         */
   /* semi-valid.                                                       */
   if((InputBufferLength) && (InputBuffer))
   {
      /* Initialize the Input Buffer to an Empty State and Done to      */
      /* FALSE.                                                         */
      InputBuffer[0] = '\0';
      InputIndex     = 0;
      Done           = FALSE;

      /* Loop until a complete Command has been received or an error has*/
      /* occurred.                                                      */
      while(Done == FALSE)
      {
         /* Attempt to read data from the Console.                      */
         if(HAL_ConsoleRead(1, &Char))
         {
            switch(Char)
            {
               case '\r':
               case '\n':
                  /* This character is a new-line or a line-feed        */
                  /* character NULL terminate the Input Buffer.         */
                  InputBuffer[InputIndex] = '\0';

                  /* Set Done to true to indicate that a complete       */
                  /* command has been received.                         */
                  /* ** NOTE ** In the current implementation any data  */
                  /*            after a new-line or line-feed character */
                  /*            will be lost.  This is fine for how this*/
                  /*            function is used is no more data will be*/
                  /*            entered after a new-line or line-feed.  */
                  Done = TRUE;
                  break;
               case 0x08:
                  /* Backspace has been pressed, so now decrement the   */
                  /* number of bytes in the buffer (if there are bytes  */
                  /* in the buffer).                                    */
                  if(InputIndex)
                  {
                     InputIndex--;
//                     HAL_ConsoleWrite(3, "\b \b");
                  }
                  break;
               default:
                  /* Accept any other printable characters.             */
                  if((Char >= ' ') && (Char <= '~'))
                  {
                     /* Add the Data Byte to the Input Buffer, and make */
                     /* sure that we do not overwrite the Input Buffer. */
                     InputBuffer[InputIndex++] = Char;
//                     HAL_ConsoleWrite(1, &Char);

                     /* Check to see if we have reached the end of the  */
                     /* buffer.                                         */
                     if(InputIndex == InputBufferLength)
                        Done = TRUE;
                  }
                  break;
            }
         }
         else
            BTPS_Delay(50);
      }
   }
}

   /* The following function is responsible for parsing user input      */
   /* and call appropriate command function.                            */
static int CommandLineInterpreter(void)
{
   int           Result = !EXIT_CODE;
   UserCommand_t TempCommand;

   /* This is the main loop of the program.  It gets user input from the*/
   /* command window, make a call to the command parser, and command    */
   /* interpreter.  After the function has been ran it then check the   */
   /* return value and displays an error message when appropriate.  If  */
   /* the result returned is ever the EXIT_CODE the loop will exit      */
   /* leading the exit of the program.                                  */
   while((Result != EXIT_CODE) && (Result != EXIT_MODE))
   {
      /* Output an Input Shell-type prompt.                             */
      DisplayPrompt();

      /* Retrieve the command entered by the user and store it in the   */
      /* User Input Buffer.  Note that this command will fail if the    */
      /* application receives a signal which cause the standard file    */
      /* streams to be closed.  If this happens the loop will be broken */
      /* out of so the application can exit.                            */
      GetInput(sizeof(Input) - 1, Input);

      /* Next, check to see if a command was input by the user.         */
      if(BTPS_StringLength(Input))
      {
         /* The string input by the user contains a value, now run the  */
         /* string through the Command Parser.                          */
         if(CommandParser(&TempCommand, Input) >= 0)
         {
            Display(("\r\n"));

            /* The Command was successfully parsed run the Command      */
            Result = CommandInterpreter(&TempCommand);

            switch(Result)
            {
               case INVALID_COMMAND_ERROR:
                  Display(("Invalid Command: %s.\r\n",TempCommand.Command));
                  break;
               case FUNCTION_ERROR:
                  Display(("Function Error.\r\n"));
                  break;
               case EXIT_CODE:
                  break;
            }
         }
         else
            Display(("Invalid Command.\r\n"));
      }
   }

   return(Result);
}

   /* The following function is responsible for converting number       */
   /* strings to there unsigned integer equivalent.  This function can  */
   /* handle leading and tailing white space, however it does not handle*/
   /* signed or comma delimited values.  This function takes as its     */
   /* input the string which is to be converted.  The function returns  */
   /* zero if an error occurs otherwise it returns the value parsed from*/
   /* the string passed as the input parameter.                         */
static unsigned int StringToUnsignedInteger(char *StringInteger)
{
   int          IsHex;
   unsigned int Index;
   unsigned int ret_val = 0;

   /* Before proceeding make sure that the parameter that was passed as */
   /* an input appears to be at least semi-valid.                       */
   if((StringInteger) && (BTPS_StringLength(StringInteger)))
   {
      /* Initialize the variable.                                       */
      Index = 0;

      /* Next check to see if this is a hexadecimal number.             */
      if(BTPS_StringLength(StringInteger) > 2)
      {
         if((StringInteger[0] == '0') && ((StringInteger[1] == 'x') || (StringInteger[1] == 'X')))
         {
            IsHex = 1;

            /* Increment the String passed the Hexadecimal prefix.      */
            StringInteger += 2;
         }
         else
            IsHex = 0;
      }
      else
         IsHex = 0;

      /* Process the value differently depending on whether or not a    */
      /* Hexadecimal Number has been specified.                         */
      if(!IsHex)
      {
         /* Decimal Number has been specified.                          */
         while(1)
         {
            /* First check to make sure that this is a valid decimal    */
            /* digit.                                                   */
            if((StringInteger[Index] >= '0') && (StringInteger[Index] <= '9'))
            {
               /* This is a valid digit, add it to the value being      */
               /* built.                                                */
               ret_val += (StringInteger[Index] & 0xF);

               /* Determine if the next digit is valid.                 */
               if(((Index + 1) < BTPS_StringLength(StringInteger)) && (StringInteger[Index+1] >= '0') && (StringInteger[Index+1] <= '9'))
               {
                  /* The next digit is valid so multiply the current    */
                  /* return value by 10.                                */
                  ret_val *= 10;
               }
               else
               {
                  /* The next value is invalid so break out of the loop.*/
                  break;
               }
            }

            Index++;
         }
      }
      else
      {
         /* Hexadecimal Number has been specified.                      */
         while(1)
         {
            /* First check to make sure that this is a valid Hexadecimal*/
            /* digit.                                                   */
            if(((StringInteger[Index] >= '0') && (StringInteger[Index] <= '9')) || ((StringInteger[Index] >= 'a') && (StringInteger[Index] <= 'f')) || ((StringInteger[Index] >= 'A') && (StringInteger[Index] <= 'F')))
            {
               /* This is a valid digit, add it to the value being      */
               /* built.                                                */
               if((StringInteger[Index] >= '0') && (StringInteger[Index] <= '9'))
                  ret_val += (StringInteger[Index] & 0xF);
               else
               {
                  if((StringInteger[Index] >= 'a') && (StringInteger[Index] <= 'f'))
                     ret_val += (StringInteger[Index] - 'a' + 10);
                  else
                     ret_val += (StringInteger[Index] - 'A' + 10);
               }

               /* Determine if the next digit is valid.                 */
               if(((Index + 1) < BTPS_StringLength(StringInteger)) && (((StringInteger[Index+1] >= '0') && (StringInteger[Index+1] <= '9')) || ((StringInteger[Index+1] >= 'a') && (StringInteger[Index+1] <= 'f')) || ((StringInteger[Index+1] >= 'A') && (StringInteger[Index+1] <= 'F'))))
               {
                  /* The next digit is valid so multiply the current    */
                  /* return value by 16.                                */
                  ret_val *= 16;
               }
               else
               {
                  /* The next value is invalid so break out of the loop.*/
                  break;
               }
            }

            Index++;
         }
      }
   }

   return(ret_val);
}

   /* The following function is responsible for parsing strings into    */
   /* components.  The first parameter of this function is a pointer to */
   /* the String to be parsed.  This function will return the start of  */
   /* the string upon success and a NULL pointer on all errors.         */
static char *StringParser(char *String)
{
   int   Index;
   char *ret_val = NULL;

   /* Before proceeding make sure that the string passed in appears to  */
   /* be at least semi-valid.                                           */
   if((String) && (BTPS_StringLength(String)))
   {
      /* The string appears to be at least semi-valid.  Search for the  */
      /* first space character and replace it with a NULL terminating   */
      /* character.                                                     */
      for(Index=0, ret_val=String;Index < BTPS_StringLength(String);Index++)
      {
         /* Is this the space character.                                */
         if((String[Index] == ' ') || (String[Index] == '\r') || (String[Index] == '\n'))
         {
            /* This is the space character, replace it with a NULL      */
            /* terminating character and set the return value to the    */
            /* beginning character of the string.                       */
            String[Index] = '\0';
            break;
         }
      }
   }

   return(ret_val);
}

   /* This function is responsible for taking command strings and       */
   /* parsing them into a command, param1, and param2.  After parsing   */
   /* this string the data is stored into a UserCommand_t structure to  */
   /* be used by the interpreter.  The first parameter of this function */
   /* is the structure used to pass the parsed command string out of the*/
   /* function.  The second parameter of this function is the string    */
   /* that is parsed into the UserCommand structure.  Successful        */
   /* execution of this function is denoted by a return value of zero.  */
   /* Negative return values denote an error in the parsing of the      */
   /* string parameter.                                                 */
static int CommandParser(UserCommand_t *TempCommand, char *Input)
{
   int            ret_val;
   int            StringLength;
   char          *LastParameter;
   unsigned int   Count         = 0;

   /* Before proceeding make sure that the passed parameters appear to  */
   /* be at least semi-valid.                                           */
   if((TempCommand) && (Input) && (BTPS_StringLength(Input)))
   {
      /* First get the initial string length.                           */
      StringLength = BTPS_StringLength(Input);

      /* Retrieve the first token in the string, this should be the     */
      /* command.                                                       */
      TempCommand->Command = StringParser(Input);

      /* Flag that there are NO Parameters for this Command Parse.      */
      TempCommand->Parameters.NumberofParameters = 0;

       /* Check to see if there is a Command                            */
      if(TempCommand->Command)
      {
         /* Initialize the return value to zero to indicate success on  */
         /* commands with no parameters.                                */
         ret_val    = 0;

         /* Adjust the UserInput pointer and StringLength to remove the */
         /* Command from the data passed in before parsing the          */
         /* parameters.                                                 */
         Input        += BTPS_StringLength(TempCommand->Command) + 1;
         StringLength  = BTPS_StringLength(Input);

         /* There was an available command, now parse out the parameters*/
         while((StringLength > 0) && ((LastParameter = StringParser(Input)) != NULL))
         {
            /* There is an available parameter, now check to see if     */
            /* there is room in the UserCommand to store the parameter  */
            if(Count < (sizeof(TempCommand->Parameters.Params)/sizeof(Parameter_t)))
            {
               /* Save the parameter as a string.                       */
               TempCommand->Parameters.Params[Count].strParam = LastParameter;

               /* Save the parameter as an unsigned int intParam will   */
               /* have a value of zero if an error has occurred.        */
               TempCommand->Parameters.Params[Count].intParam = StringToUnsignedInteger(LastParameter);

               Count++;
               Input        += BTPS_StringLength(LastParameter) + 1;
               StringLength -= BTPS_StringLength(LastParameter) + 1;

               ret_val = 0;
            }
            else
            {
               /* Be sure we exit out of the Loop.                      */
               StringLength = 0;

               ret_val      = TO_MANY_PARAMS;
            }
         }

         /* Set the number of parameters in the User Command to the     */
         /* number of found parameters                                  */
         TempCommand->Parameters.NumberofParameters = Count;
      }
      else
      {
         /* No command was specified                                    */
         ret_val = NO_COMMAND_ERROR;
      }
   }
   else
   {
      /* One or more of the passed parameters appear to be invalid.     */
      ret_val = INVALID_PARAMETERS_ERROR;
   }

   return(ret_val);
}

   /* This function is responsible for determining the command in which */
   /* the user entered and running the appropriate function associated  */
   /* with that command.  The first parameter of this function is a     */
   /* structure containing information about the command to be issued.  */
   /* This information includes the command name and multiple parameters*/
   /* which maybe be passed to the function to be executed.  Successful */
   /* execution of this function is denoted by a return value of zero.  */
   /* A negative return value implies that command was not found and is */
   /* invalid.                                                          */
static int CommandInterpreter(UserCommand_t *TempCommand)
{
   int               i;
   int               ret_val;
   CommandFunction_t CommandFunction;

   /* If the command is not found in the table return with an invalid   */
   /* command error                                                     */
   ret_val = INVALID_COMMAND_ERROR;

   /* Let's make sure that the data passed to us appears semi-valid.    */
   if((TempCommand) && (TempCommand->Command))
   {
      /* Now, let's make the Command string all upper case so that we   */
      /* compare against it.                                            */
      for(i=0;i<BTPS_StringLength(TempCommand->Command);i++)
      {
         if((TempCommand->Command[i] >= 'a') && (TempCommand->Command[i] <= 'z'))
            TempCommand->Command[i] -= ('a' - 'A');
      }

      /* Check to see if the command which was entered was exit.        */
      if(BTPS_MemCompare(TempCommand->Command, "QUIT", BTPS_StringLength("QUIT")) != 0)
      {
         /* The command entered is not exit so search for command in    */
         /* table.                                                      */
         if((CommandFunction = FindCommand(TempCommand->Command)) != NULL)
         {
            /* The command was found in the table so call the command.  */
            ret_val = (*CommandFunction)(&TempCommand->Parameters);
            if(!ret_val)
            {
               /* Return success to the caller.                         */
               ret_val = 0;
            }
            else
            {
               if((ret_val != EXIT_CODE) && (ret_val != EXIT_MODE))
                  ret_val = FUNCTION_ERROR;
            }
         }
      }
      else
      {
         /* The command entered is exit, set return value to EXIT_CODE  */
         /* and return.                                                 */
         ret_val = EXIT_CODE;
      }
   }
   else
      ret_val = INVALID_PARAMETERS_ERROR;

   return(ret_val);
}

   /* The following function is provided to allow a means to            */
   /* programatically add Commands the Global (to this module) Command  */
   /* Table.  The Command Table is simply a mapping of Command Name     */
   /* (NULL terminated ASCII string) to a command function.  This       */
   /* function returns zero if successful, or a non-zero value if the   */
   /* command could not be added to the list.                           */
static int AddCommand(char *CommandName, CommandFunction_t CommandFunction)
{
   int ret_val = 0;

   /* First, make sure that the parameters passed to us appear to be    */
   /* semi-valid.                                                       */
   if((CommandName) && (CommandFunction))
   {
      /* Next, make sure that we still have room in the Command Table   */
      /* to add commands.                                               */
      if(NumberCommands < MAX_SUPPORTED_COMMANDS)
      {
         /* Simply add the command data to the command table and        */
         /* increment the number of supported commands.                 */
         CommandTable[NumberCommands].CommandName       = CommandName;
         CommandTable[NumberCommands++].CommandFunction = CommandFunction;

         /* Return success to the caller.                               */
         ret_val                                        = 0;
      }
      else
         ret_val = 1;
   }
   else
      ret_val = 1;

   return(ret_val);
}

   /* The following function searches the Command Table for the         */
   /* specified Command.  If the Command is found, this function returns*/
   /* a NON-NULL Command Function Pointer.  If the command is not found */
   /* this function returns NULL.                                       */
static CommandFunction_t FindCommand(char *Command)
{
   unsigned int      Index;
   CommandFunction_t ret_val;

   /* First, make sure that the command specified is semi-valid.        */
   if(Command)
   {
      /* Now loop through each element in the table to see if there is  */
      /* a match.                                                       */
      for(Index = 0, ret_val = NULL; ((Index<NumberCommands) && (!ret_val)); Index ++)
      {
         if(BTPS_MemCompare(Command, CommandTable[Index].CommandName, BTPS_StringLength(CommandTable[Index].CommandName)) == 0)
            ret_val = CommandTable[Index].CommandFunction;
      }
   }
   else
      ret_val = NULL;

   return(ret_val);
}

   /* The following function is provided to allow a means to clear out  */
   /* all available commands from the command table.                    */
static void ClearCommands(void)
{
   /* Simply flag that there are no commands present in the table.      */
   NumberCommands = 0;
}

   /* The following function is responsible for converting data of type */
   /* BD_ADDR to a string.  The first parameter of this function is the */
   /* BD_ADDR to be converted to a string.  The second parameter of this*/
   /* function is a pointer to the string in which the converted BD_ADDR*/
   /* is to be stored.                                                  */
static void BD_ADDRToStr(BD_ADDR_t Board_Address, BoardStr_t BoardStr)
{
   BTPS_SprintF((char *)BoardStr, "0x%02X%02X%02X%02X%02X%02X", Board_Address.BD_ADDR5, Board_Address.BD_ADDR4, Board_Address.BD_ADDR3, Board_Address.BD_ADDR2, Board_Address.BD_ADDR1, Board_Address.BD_ADDR0);
}

   /* Displays the current I/O Capabilities.                            */
static void DisplayIOCapabilities(void)
{
   Display(("I/O Capabilities: %s, MITM: %s.\r\n", IOCapabilitiesStrings[(unsigned int)IOCapability], MITMProtection?"TRUE":"FALSE"));
}

   /* Utility function to display a Class of Device Structure.          */
static void DisplayClassOfDevice(Class_of_Device_t Class_of_Device)
{
   Display(("Class of Device: 0x%02X%02X%02X.\r\n", Class_of_Device.Class_of_Device0, Class_of_Device.Class_of_Device1, Class_of_Device.Class_of_Device2));
}

   /* Displays the correct prompt depending on the Server/Client Mode.  */
static void DisplayPrompt(void)
{
   if(UI_Mode == UI_MODE_IS_CLIENT)
      Display(("\r\nClient> \b"));
   else
   {
      if(UI_Mode == UI_MODE_IS_SERVER)
         Display(("\r\nServer> \b"));
      else
         Display(("\r\nChoose Mode> \b"));
   }
}

   /* Displays a usage string..                                         */
static void DisplayUsage(char *UsageString)
{
   Display(("\r\nUsage: %s.\r\n", UsageString));
}

   /* Displays a function error message.                                */
static void DisplayFunctionError(char *Function, int Status)
{
   Display(("\r\n%s Failed: %d.\r\n", Function, Status));
}

   /* Displays a function success message.                              */
static void DisplayFunctionSuccess(char *Function)
{
   Display(("\r\n%s success.\r\n", Function));
}

   /* The following Toggles an LED at a passed in blink rate.           */
void *ToggleLED(void *UserParameter)
{
   int BlinkRate = (int)UserParameter;
   Boolean_t Done;

   Done = FALSE;
   while(!Done)
   {
      HAL_LedToggle(0);

      BTPS_Delay(BlinkRate);
   }

   return(NULL);
}

   /* The following function is responsible for querying the Local      */
   /* Bluetooth Device Address.  This function will return zero on      */
   /* successful execution and a negative value on errors.              */
static int QueryMemory(ParameterList_t *TempParam)
{
   unsigned long OS_HighWater;

   /* Print out the current usage as well as the max and min numbers.   */
   OS_HighWater = (unsigned long)_mem_get_highwater();
   Display(("\r\nMQX High Water: 0x%08X.\r\n", OS_HighWater));

   return(0);
}

   /* The following thread is responsible for checking the Stack high   */
   /* water mark for each thread in the system.                         */
static int StackChecker(ParameterList_t *TempParam)
{
   _klog_show_stack_usage();
   return(0);
}

   /* The following function is a utility function that exists to add   */
   /* the Device Identification (DID) Service Record to the SDP         */
   /* Database.                                                         */
static int AddDeviceIDInformation(void)
{
   long               ret_val;
   DWord_t            SDPRecordHandle;
   SDP_UUID_Entry_t   UUIDEntry;
   SDP_Data_Element_t SDPDataElement;

   /* Create a Service Record with the PNP UUID.                        */
   UUIDEntry.SDP_Data_Element_Type = deUUID_16;
   SDP_ASSIGN_PNP_INFORMATION_UUID_16(UUIDEntry.UUID_Value.UUID_16);
   ret_val = SDP_Create_Service_Record(BluetoothStackID, 1, &UUIDEntry);
   if(ret_val)
   {
      /* Save the Handle to this Service Record.                        */
      SDPRecordHandle = ret_val;

      /* Add the Profile Version Attribute.                             */
      SDPDataElement.SDP_Data_Element_Type                  = deUnsignedInteger2Bytes;
      SDPDataElement.SDP_Data_Element_Length                = WORD_SIZE;
      SDPDataElement.SDP_Data_Element.UnsignedInteger2Bytes = 0x0103;

      ret_val = SDP_Add_Attribute(BluetoothStackID, SDPRecordHandle, SDP_ATTRIBUTE_ID_DID_SPECIFICATION_ID, &SDPDataElement);
      if(!ret_val)
      {
         /* Add the Vendor ID Attribute.                                */
         SDPDataElement.SDP_Data_Element_Type                  = deUnsignedInteger2Bytes;
         SDPDataElement.SDP_Data_Element_Length                = WORD_SIZE;
         SDPDataElement.SDP_Data_Element.UnsignedInteger2Bytes = VENDOR_ID_VALUE;

         ret_val = SDP_Add_Attribute(BluetoothStackID, SDPRecordHandle, SDP_ATTRIBUTE_ID_DID_VENDOR_ID, &SDPDataElement);
         if(!ret_val)
         {
            /* Add the Product ID Attribute.                            */
            SDPDataElement.SDP_Data_Element_Type                  = deUnsignedInteger2Bytes;
            SDPDataElement.SDP_Data_Element_Length                = WORD_SIZE;
            SDPDataElement.SDP_Data_Element.UnsignedInteger2Bytes = 0x0001;

            ret_val = SDP_Add_Attribute(BluetoothStackID, SDPRecordHandle, SDP_ATTRIBUTE_ID_DID_PRODUCT_ID, &SDPDataElement);
            if(!ret_val)
            {
               /* Add the Product Version ID Attribute.                 */
               SDPDataElement.SDP_Data_Element_Type                  = deUnsignedInteger2Bytes;
               SDPDataElement.SDP_Data_Element_Length                = WORD_SIZE;
               SDPDataElement.SDP_Data_Element.UnsignedInteger2Bytes = 0x0112;

               ret_val = SDP_Add_Attribute(BluetoothStackID, SDPRecordHandle, SDP_ATTRIBUTE_ID_DID_VERSION, &SDPDataElement);
               if(!ret_val)
               {
                  /* Add the Primary Record ID Attribute.               */
                  SDPDataElement.SDP_Data_Element_Type    = deBoolean;
                  SDPDataElement.SDP_Data_Element_Length  = BYTE_SIZE;
                  SDPDataElement.SDP_Data_Element.Boolean = TRUE;

                  ret_val = SDP_Add_Attribute(BluetoothStackID, SDPRecordHandle, SDP_ATTRIBUTE_ID_DID_PRIMARY_RECORD, &SDPDataElement);
                  if(!ret_val)
                  {
                     /* Add the Primary Record ID Attribute.            */
                     SDPDataElement.SDP_Data_Element_Type                  = deUnsignedInteger2Bytes;
                     SDPDataElement.SDP_Data_Element_Length                = WORD_SIZE;
                     SDPDataElement.SDP_Data_Element.UnsignedInteger2Bytes = 0x0001;

                     SDP_Add_Attribute(BluetoothStackID, SDPRecordHandle, SDP_ATTRIBUTE_ID_DID_VENDOR_ID_SOURCE, &SDPDataElement);
                  }
               }
            }
         }
      }
   }
   else
      Display(("Failed to add Device Identification Recorded to the SDP Database.\r\n"));

   return(ret_val);
}

   /* The following function is responsible for opening the SS1         */
   /* Bluetooth Protocol Stack.  This function accepts a pre-populated  */
   /* HCI Driver Information structure that contains the HCI Driver     */
   /* Transport Information.  This function returns zero on successful  */
   /* execution and a negative value on all errors.                     */
static int OpenStack(HCI_DriverInformation_t *HCI_DriverInformation)
{
   int                                   Result;
   int                                   ret_val = 0;
   char                                  BluetoothAddress[16];
   Byte_t                                Status;
   BD_ADDR_t                             BD_ADDR;
   HCI_Version_t                         HCIVersion;
   Extended_Inquiry_Response_Data_t     *ExtendedInquiryResponseData;
   Class_of_Device_t                     ClassOfDevice;
   L2CA_Link_Connect_Params_t            L2CA_Link_Connect_Params;
   GAP_Extended_Inquiry_Response_Data_t  GAPExtendedInquiryResponseData;
   BTPS_Initialization_t                 BTPS_Initialization;

   /* First check to see if the Stack has already been opened.          */
   if(!BluetoothStackID)
   {
      /* Next, makes sure that the Driver Information passed appears to */
      /* be semi-valid.                                                 */
      if(HCI_DriverInformation)
      {
         /* Initialize BTPSKNRL.                                        */
         BTPS_Initialization.MessageOutputCallback = HAL_ConsoleWrite;
         BTPS_Init((void *)&BTPS_Initialization);

         Display(("\r\nOpenStack().\r\n"));

         /* Initialize the Stack                                        */
         Result = BSC_Initialize(HCI_DriverInformation, 0);

         /* Next, check the return value of the initialization to see if*/
         /* it was successful.                                          */
         if(Result > 0)
         {
            /* The Stack was initialized successfully, inform the user  */
            /* and set the return value of the initialization function  */
            /* to the Bluetooth Stack ID.                               */
            BluetoothStackID = Result;
            Display(("Bluetooth Stack ID: %d\r\n", BluetoothStackID));

            /* Initialize the default Secure Simple Pairing parameters. */
            IOCapability     = DEFAULT_IO_CAPABILITY;
            OOBSupport       = FALSE;
            MITMProtection   = DEFAULT_MITM_PROTECTION;

            if(!HCI_Version_Supported(BluetoothStackID, &HCIVersion))
               Display(("Device Chipset: %s\r\n", (HCIVersion <= NUM_SUPPORTED_HCI_VERSIONS)?HCIVersionStrings[HCIVersion]:HCIVersionStrings[NUM_SUPPORTED_HCI_VERSIONS]));

            /* Let's output the Bluetooth Device Address so that the    */
            /* user knows what the Device Address is.                   */
            if(!GAP_Query_Local_BD_ADDR(BluetoothStackID, &BD_ADDR))
            {
               BD_ADDRToStr(BD_ADDR, BluetoothAddress);

               Display(("BD_ADDR: %s\r\n", BluetoothAddress));
            }

            /* Set the class of device.                                 */
            /* * NOTE * Apple is very specific about the Class of Device*/
            /*          values that they will respond to/support.       */
            ASSIGN_CLASS_OF_DEVICE(ClassOfDevice, 0, 0, 0);

            SET_MAJOR_SERVICE_CLASS(ClassOfDevice, HCI_LMP_CLASS_OF_DEVICE_SERVICE_CLASS_AUDIO_BIT);
            SET_MINOR_DEVICE_CLASS(ClassOfDevice, HCI_LMP_CLASS_OF_DEVICE_MINOR_DEVICE_CLASS_AUDIO_VIDEO_MICROPHONE);

            SET_MAJOR_SERVICE_CLASS(ClassOfDevice, HCI_LMP_CLASS_OF_DEVICE_MAJOR_DEVICE_CLASS_AUDIO_VIDEO);

            GAP_Set_Class_Of_Device(BluetoothStackID, ClassOfDevice);

            /* Write the local name.                                    */
            GAP_Set_Local_Device_Name(BluetoothStackID, DEFAULT_LOCAL_DEVICE_NAME);

            /* Write the Inquiry Response Data.                         */
            if((ExtendedInquiryResponseData = (Extended_Inquiry_Response_Data_t *)BTPS_AllocateMemory(sizeof(Extended_Inquiry_Response_Data_t))) != NULL)
            {
               GAPExtendedInquiryResponseData.Number_Data_Entries = sizeof(GAPExtendedInquiryResponseDataEntries)/sizeof(GAP_Extended_Inquiry_Response_Data_Entry_t);
               GAPExtendedInquiryResponseData.Data_Entries        = GAPExtendedInquiryResponseDataEntries;

               /* Format the EIR data and write it.                        */
               if(GAP_Convert_Extended_Inquiry_Response_Data(&(GAPExtendedInquiryResponseData), ExtendedInquiryResponseData))
                  GAP_Write_Extended_Inquiry_Information(BluetoothStackID, HCI_EXTENDED_INQUIRY_RESPONSE_FEC_REQUIRED, ExtendedInquiryResponseData);

               BTPS_FreeMemory(ExtendedInquiryResponseData);
            }

            /* Go ahead and allow Master/Slave Role Switch.             */
            L2CA_Link_Connect_Params.L2CA_Link_Connect_Request_Config  = cqAllowRoleSwitch;
            L2CA_Link_Connect_Params.L2CA_Link_Connect_Response_Config = csMaintainCurrentRole;

            L2CA_Set_Link_Connection_Configuration(BluetoothStackID, &L2CA_Link_Connect_Params);

            if(HCI_Command_Supported(BluetoothStackID, HCI_SUPPORTED_COMMAND_WRITE_DEFAULT_LINK_POLICY_BIT_NUMBER) > 0)
               HCI_Write_Default_Link_Policy_Settings(BluetoothStackID, (HCI_LINK_POLICY_SETTINGS_ENABLE_MASTER_SLAVE_SWITCH|HCI_LINK_POLICY_SETTINGS_ENABLE_SNIFF_MODE), &Status);

            /* Delete all Stored Link Keys.                             */
            ASSIGN_BD_ADDR(BD_ADDR, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);

            DeleteLinkKey(BD_ADDR);

            /* Finally, let's attempt to initialize the iSPP module.    */
            if(!ISPP_Initialize(BluetoothStackID, NULL))
            {
               IACPTR_Read(0, 4, Buffer, &Buffer[4]);
               Display(("Device Version: %d\r\n", Buffer[0]));
               Display(("Firmware Version: %d\r\n", Buffer[1]));
               Display(("Authentication Version: %d.%d\r\n", Buffer[2], Buffer[3]));

               IACPTR_Read(4, 4, Buffer, &Buffer[4]);
               Display(("DeviceID: %02X%02X%02X%02X\r\n", Buffer[0], Buffer[1], Buffer[2], Buffer[3]));
               IACPTR_Read(5, 1, Buffer, &Buffer[1]);
               Display(("Error Code: %d\r\n", Buffer[0]));

               Display(("ISPP_Initialize() Success\r\n"));

               /* Add the Device ID Information to the SDP Database.    */
               AddDeviceIDInformation();
            }
            else
            {
               Display(("ISPP_Initialize() Failed\r\n"));

               /* Go ahead and clean up since ISPP did not initiailize  */
               /* and it is required for this sample.                   */
               BSC_Shutdown(BluetoothStackID);

               BluetoothStackID = 0;

               ret_val          = UNABLE_TO_INITIALIZE_STACK;
            }
         }
         else
         {
            /* The Stack was NOT initialized successfully, inform the   */
            /* user and set the return value of the initialization      */
            /* function to an error.                                    */
            DisplayFunctionError("BSC_Initialize", Result);

            BluetoothStackID = 0;

            ret_val          = UNABLE_TO_INITIALIZE_STACK;
         }
      }
      else
      {
         /* One or more of the necessary parameters are invalid.        */
         ret_val = INVALID_PARAMETERS_ERROR;
      }
   }

   return(ret_val);
}

   /* The following function is responsible for closing the SS1         */
   /* Bluetooth Protocol Stack.  This function requires that the        */
   /* Bluetooth Protocol stack previously have been initialized via the */
   /* OpenStack() function.  This function returns zero on successful   */
   /* execution and a negative value on all errors.                     */
static int CloseStack(void)
{
   int ret_val = 0;

   /* First check to see if the Stack has been opened.                  */
   if(BluetoothStackID)
   {
      /* Simply close the Stack                                         */
      BSC_Shutdown(BluetoothStackID);

      /* Free BTPSKRNL allocated memory.                                */
      BTPS_DeInit();

      Display(("Stack Shutdown.\r\n"));

      /* Flag that the Stack is no longer initialized.                  */
      BluetoothStackID = 0;

      /* Flag success to the caller.                                    */
      ret_val          = 0;
   }
   else
   {
      /* A valid Stack ID does not exist, inform to user.               */
      ret_val = UNABLE_TO_INITIALIZE_STACK;
   }

   return(ret_val);
}

   /* The following function is responsible for placing the Local       */
   /* Bluetooth Device into General Discoverablity Mode.  Once in this  */
   /* mode the Device will respond to Inquiry Scans from other Bluetooth*/
   /* Devices.  This function requires that a valid Bluetooth Stack ID  */
   /* exists before running.  This function returns zero on successful  */
   /* execution and a negative value if an error occurred.              */
static int SetDisc(void)
{
   int ret_val = 0;

   /* First, check that a valid Bluetooth Stack ID exists.              */
   if(BluetoothStackID)
   {
      /* A semi-valid Bluetooth Stack ID exists, now attempt to set the */
      /* attached Devices Discoverablity Mode to General.               */
      ret_val = GAP_Set_Discoverability_Mode(BluetoothStackID, dmGeneralDiscoverableMode, 0);

      /* Next, check the return value of the GAP Set Discoverability    */
      /* Mode command for successful execution.                         */
      if(ret_val)
      {
         /* An error occurred while trying to set the Discoverability   */
         /* Mode of the Device.                                         */
         DisplayFunctionError("Set Discoverable Mode", ret_val);
      }
   }
   else
   {
      /* No valid Bluetooth Stack ID exists.                            */
      ret_val = INVALID_STACK_ID_ERROR;
   }

   return(ret_val);
}

   /* The following function is responsible for placing the Local       */
   /* Bluetooth Device into Connectable Mode.  Once in this mode the    */
   /* Device will respond to Page Scans from other Bluetooth Devices.   */
   /* This function requires that a valid Bluetooth Stack ID exists     */
   /* before running.  This function returns zero on success and a      */
   /* negative value if an error occurred.                              */
static int SetConnect(void)
{
   int ret_val = 0;

   /* First, check that a valid Bluetooth Stack ID exists.              */
   if(BluetoothStackID)
   {
      /* Attempt to set the attached Device to be Connectable.          */
      ret_val = GAP_Set_Connectability_Mode(BluetoothStackID, cmConnectableMode);

      /* Next, check the return value of the                            */
      /* GAP_Set_Connectability_Mode() function for successful          */
      /* execution.                                                     */
      if(ret_val)
      {
         /* An error occurred while trying to make the Device           */
         /* Connectable.                                                */
         DisplayFunctionError("Set Connectability Mode", ret_val);
      }
   }
   else
   {
      /* No valid Bluetooth Stack ID exists.                            */
      ret_val = INVALID_STACK_ID_ERROR;
   }

   return(ret_val);
}

   /* The following function is responsible for placing the local       */
   /* Bluetooth device into Pairable mode.  Once in this mode the device*/
   /* will response to pairing requests from other Bluetooth devices.   */
   /* This function returns zero on successful execution and a negative */
   /* value on all errors.                                              */
static int SetPairable(void)
{
   int Result;
   int ret_val = 0;

   /* First, check that a valid Bluetooth Stack ID exists.              */
   if(BluetoothStackID)
   {
      /* Attempt to set the attached device to be pairable.             */
      Result = GAP_Set_Pairability_Mode(BluetoothStackID, pmPairableMode_EnableSecureSimplePairing);

      /* Next, check the return value of the GAP Set Pairability mode   */
      /* command for successful execution.                              */
      if(!Result)
      {
         /* The device has been set to pairable mode, now register an   */
         /* Authentication Callback to handle the Authentication events */
         /* if required.                                                */
         Result = GAP_Register_Remote_Authentication(BluetoothStackID, GAP_Event_Callback, (unsigned long)0);

         /* Next, check the return value of the GAP Register Remote     */
         /* Authentication command for successful execution.            */
         if(Result)
         {
            /* An error occurred while trying to execute this function. */
            DisplayFunctionError("Auth", Result);

            ret_val = Result;
         }
      }
      else
      {
         /* An error occurred while trying to make the device pairable. */
         DisplayFunctionError("Set Pairability Mode", Result);

         ret_val = Result;
      }
   }
   else
   {
      /* No valid Bluetooth Stack ID exists.                            */
      ret_val = INVALID_STACK_ID_ERROR;
   }

   return(ret_val);
}

   /* The following function is a utility function that exists to delete*/
   /* the specified Link Key from the Local Bluetooth Device.  If a NULL*/
   /* Bluetooth Device Address is specified, then all Link Keys will be */
   /* deleted.                                                          */
static int DeleteLinkKey(BD_ADDR_t BD_ADDR)
{
   int       Result;
   Byte_t    Status_Result;
   Word_t    Num_Keys_Deleted = 0;
   BD_ADDR_t NULL_BD_ADDR;

   Result = HCI_Delete_Stored_Link_Key(BluetoothStackID, BD_ADDR, TRUE, &Status_Result, &Num_Keys_Deleted);

   /* Any stored link keys for the specified address (or all) have been */
   /* deleted from the chip.  Now, let's make sure that our stored Link */
   /* Key Array is in sync with these changes.                          */

   /* First check to see all Link Keys were deleted.                    */
   ASSIGN_BD_ADDR(NULL_BD_ADDR, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);

   if(COMPARE_BD_ADDR(BD_ADDR, NULL_BD_ADDR))
      BTPS_MemInitialize(LinkKeyInfo, 0, sizeof(LinkKeyInfo));
   else
   {
      /* Individual Link Key.  Go ahead and see if know about the entry */
      /* in the list.                                                   */
      for(Result=0;(Result<sizeof(LinkKeyInfo)/sizeof(LinkKeyInfo_t));Result++)
      {
         if(COMPARE_BD_ADDR(BD_ADDR, LinkKeyInfo[Result].BD_ADDR))
         {
            LinkKeyInfo[Result].BD_ADDR = NULL_BD_ADDR;

            break;
         }
      }
   }

   return(Result);
}

   /* The following function is responsible for displaying the current  */
   /* Command Options for either Serial Port Client or Serial Port      */
   /* Server.  The input parameter to this function is completely       */
   /* ignored, and only needs to be passed in because all Commands that */
   /* can be entered at the Prompt pass in the parsed information.  This*/
   /* function displays the current Command Options that are available  */
   /* and always returns zero.                                          */
static int DisplayHelp(ParameterList_t *TempParam)
{

   if(UI_Mode == UI_MODE_IS_CLIENT)
   {
      Display(("\r\n"));
      Display(("******************************************************************\r\n"));
      Display(("* Command Options: Inquiry, DisplayInquiryList, Pair,            *\r\n"));
      Display(("*                  EndPairing, PINCodeResponse, PassKeyResponse, *\r\n"));
      Display(("*                  UserConfirmationResponse,                     *\r\n"));
      Display(("*                  SetDiscoverabilityMode, SetConnectabilityMode,*\r\n"));
      Display(("*                  SetPairabilityMode,                           *\r\n"));
      Display(("*                  ChangeSimplePairingParameters,                *\r\n"));
      Display(("*                  GetLocalAddress, GetLocalName, SetLocalName,  *\r\n"));
      Display(("*                  GetClassOfDevice, SetClassOfDevice,           *\r\n"));
      Display(("*                  GetRemoteName, SniffMode, ExitSniffMode,      *\r\n"));
      Display(("*                  Open, Close, Read, Write,                     *\r\n"));
      Display(("*                  Send, SDPRequest,                             *\r\n"));
      Display(("*                  GetConfigParams, SetConfigParams,             *\r\n"));
      Display(("*                  GetQueueParams, SetQueueParams,               *\r\n"));
      Display(("*                  Loopback, DisplayRawModeData,                 *\r\n"));
      Display(("*                  AutomaticReadMode, SetBaudRate,               *\r\n"));
      Display(("*                  QueryMemory, CheckStacks, Help, Quit          *\r\n"));
      Display(("******************************************************************\r\n"));
   }
   else
   {
      if(UI_Mode == UI_MODE_IS_SERVER)
      {
         Display(("\r\n"));
         Display(("******************************************************************\r\n"));
         Display(("* Command Options: Inquiry, DisplayInquiryList, Pair,            *\r\n"));
         Display(("*                  EndPairing, PINCodeResponse, PassKeyResponse, *\r\n"));
         Display(("*                  UserConfirmationResponse,                     *\r\n"));
         Display(("*                  SetDiscoverabilityMode, SetConnectabilityMode,*\r\n"));
         Display(("*                  SetPairabilityMode,                           *\r\n"));
         Display(("*                  ChangeSimplePairingParameters,                *\r\n"));
         Display(("*                  GetLocalAddress, GetLocalName, SetLocalName,  *\r\n"));
         Display(("*                  GetClassOfDevice, SetClassOfDevice,           *\r\n"));
         Display(("*                  GetRemoteName, SniffMode, ExitSniffMode,      *\r\n"));
         Display(("*                  Open, Close, Read, Write,                     *\r\n"));
         Display(("*                  Send, SDPRequest,                             *\r\n"));
         Display(("*                  GetConfigParams, SetConfigParams,             *\r\n"));
         Display(("*                  GetQueueParams, SetQueueParams,               *\r\n"));
         Display(("*                  Loopback, DisplayRawModeData,                 *\r\n"));
         Display(("*                  AutomaticReadMode, SetBaudRate,               *\r\n"));
         Display(("*                  QueryMemory, CheckStacks, Help, Quit          *\r\n"));
         Display(("******************************************************************\r\n"));
      }
      else
      {
         Display(("\r\n"));
         Display(("******************************************************************\r\n"));
         Display(("* Command Options: Server, Client, Help                          *\r\n"));
         Display(("******************************************************************\r\n"));
      }
   }

   return(0);
}

   /* The following function is responsible for performing a General    */
   /* Inquiry for discovering Bluetooth Devices.  This function requires*/
   /* that a valid Bluetooth Stack ID exists before running.  This      */
   /* function returns zero is successful or a negative value if there  */
   /* was an error.                                                     */
static int Inquiry(ParameterList_t *TempParam)
{
   int Result;
   int ret_val = 0;

   /* First, check that valid Bluetooth Stack ID exists.                */
   if(BluetoothStackID)
   {
      /* Use the GAP_Perform_Inquiry() function to perform an Inquiry.  */
      /* The Inquiry will last 10 seconds or until MAX_INQUIRY_RESULTS  */
      /* Bluetooth Devices are found.  When the Inquiry Results become  */
      /* available the GAP_Event_Callback is called.                    */
      Result = GAP_Perform_Inquiry(BluetoothStackID, itGeneralInquiry, 0, 0, 10, MAX_INQUIRY_RESULTS, GAP_Event_Callback, (unsigned long)NULL);

      /* Next, check to see if the GAP_Perform_Inquiry() function was   */
      /* successful.                                                    */
      if(!Result)
      {
         /* The Inquiry appears to have been sent successfully.         */
         /* Processing of the results returned from this command occurs */
         /* within the GAP_Event_Callback() function.                   */

         /* Flag that we have found NO Bluetooth Devices.               */
         NumberofValidResponses = 0;

         ret_val                = 0;
      }
      else
      {
         /* A error occurred while performing the Inquiry.              */
         DisplayFunctionError("Inquiry", Result);

         ret_val = Result;
      }
   }
   else
   {
      /* No valid Bluetooth Stack ID exists.                            */
      ret_val = INVALID_STACK_ID_ERROR;
   }

   return(ret_val);
}

   /* The following function is a utility function that exists to       */
   /* display the current Inquiry List (with Indexes).  This is useful  */
   /* in case the user has forgotten what Inquiry Index a particular    */
   /* Bluteooth Device was located in.  This function returns zero on   */
   /* successful execution and a negative value on all errors.          */
static int DisplayInquiryList(ParameterList_t *TempParam)
{
   int          ret_val = 0;
   unsigned int Index;

   /* First, check that valid Bluetooth Stack ID exists.                */
   if(BluetoothStackID)
   {
      /* Simply display all of the items in the Inquiry List.           */
      Display(("Inquiry List: %d Devices%s\r\n\r\n", NumberofValidResponses, NumberofValidResponses?":":"."));

      for(Index=0;Index<NumberofValidResponses;Index++)
      {
         BD_ADDRToStr(InquiryResultList[Index], Function_BoardStr);

         Display((" Inquiry Result: %d, %s.\r\n", (Index+1), Function_BoardStr));
      }

      if(NumberofValidResponses)
         Display(("\r\n"));

      /* All finished, flag success to the caller.                      */
      ret_val = 0;
   }
   else
   {
      /* No valid Bluetooth Stack ID exists.                            */
      ret_val = INVALID_STACK_ID_ERROR;
   }

   return(ret_val);
}

   /* The following function is responsible for setting the             */
   /* Discoverability Mode of the local device.  This function returns  */
   /* zero on successful execution and a negative value on all errors.  */
static int SetDiscoverabilityMode(ParameterList_t *TempParam)
{
   int                        Result;
   int                        ret_val;
   GAP_Discoverability_Mode_t DiscoverabilityMode;

   /* First, check that valid Bluetooth Stack ID exists.                */
   if(BluetoothStackID)
   {
      /* Make sure that all of the parameters required for this function*/
      /* appear to be at least semi-valid.                              */
      if((TempParam) && (TempParam->NumberofParameters > 0) && (TempParam->Params[0].intParam >= 0) && (TempParam->Params[0].intParam <= 2))
      {
         /* Parameters appear to be valid, map the specified parameters */
         /* into the API specific parameters.                           */
         if(TempParam->Params[0].intParam == 1)
            DiscoverabilityMode = dmLimitedDiscoverableMode;
         else
         {
            if(TempParam->Params[0].intParam == 2)
               DiscoverabilityMode = dmGeneralDiscoverableMode;
            else
               DiscoverabilityMode = dmNonDiscoverableMode;
         }

         /* Parameters mapped, now set the Discoverability Mode.        */
         Result = GAP_Set_Discoverability_Mode(BluetoothStackID, DiscoverabilityMode, (DiscoverabilityMode == dmLimitedDiscoverableMode)?60:0);

         /* Next, check the return value to see if the command was      */
         /* issued successfully.                                        */
         if(Result >= 0)
         {
            /* The Mode was changed successfully.                       */
            Display(("Discoverability: %s.\r\n", (DiscoverabilityMode == dmNonDiscoverableMode)?"Non":((DiscoverabilityMode == dmGeneralDiscoverableMode)?"General":"Limited")));

            /* Flag success to the caller.                              */
            ret_val = 0;
         }
         else
         {
            /* There was an error setting the Mode.                     */
            DisplayFunctionError("GAP_Set_Discoverability_Mode", Result);

            /* Flag that an error occurred while submitting the command.*/
            ret_val = FUNCTION_ERROR;
         }
      }
      else
      {
         /* One or more of the necessary parameters is/are invalid.     */
         DisplayUsage("SetDiscoverabilityMode [Mode(0 = Non Discoverable, 1 = Limited Discoverable, 2 = General Discoverable)]");

         ret_val = INVALID_PARAMETERS_ERROR;
      }
   }
   else
   {
      /* No valid Bluetooth Stack ID exists.                            */
      ret_val = INVALID_STACK_ID_ERROR;
   }

   return(ret_val);
}

   /* The following function is responsible for setting the             */
   /* Connectability Mode of the local device.  This function returns   */
   /* zero on successful execution and a negative value on all errors.  */
static int SetConnectabilityMode(ParameterList_t *TempParam)
{
   int                       Result;
   int                       ret_val;
   GAP_Connectability_Mode_t ConnectableMode;

   /* First, check that valid Bluetooth Stack ID exists.                */
   if(BluetoothStackID)
   {
      /* Make sure that all of the parameters required for this function*/
      /* appear to be at least semi-valid.                              */
      if((TempParam) && (TempParam->NumberofParameters > 0) && (TempParam->Params[0].intParam >= 0) && (TempParam->Params[0].intParam <= 1))
      {
         /* Parameters appear to be valid, map the specified parameters */
         /* into the API specific parameters.                           */
         if(TempParam->Params[0].intParam == 0)
            ConnectableMode = cmNonConnectableMode;
         else
            ConnectableMode = cmConnectableMode;

         /* Parameters mapped, now set the Connectabilty Mode.          */
         Result = GAP_Set_Connectability_Mode(BluetoothStackID, ConnectableMode);

         /* Next, check the return value to see if the command was      */
         /* issued successfully.                                        */
         if(Result >= 0)
         {
            /* The Mode was changed successfully.                       */
            Display(("Connectability Mode: %s.\r\n", (ConnectableMode == cmNonConnectableMode)?"Non Connectable":"Connectable"));

            /* Flag success to the caller.                              */
            ret_val = 0;
         }
         else
         {
            /* There was an error setting the Mode.                     */
            DisplayFunctionError("GAP_Set_Connectability_Mode", Result);

            /* Flag that an error occurred while submitting the command.*/
            ret_val = FUNCTION_ERROR;
         }
      }
      else
      {
         /* One or more of the necessary parameters is/are invalid.     */
         DisplayUsage("SetConnectabilityMode [(0 = NonConectable, 1 = Connectable)]");

         ret_val = INVALID_PARAMETERS_ERROR;
      }
   }
   else
   {
      /* No valid Bluetooth Stack ID exists.                            */
      ret_val = INVALID_STACK_ID_ERROR;
   }

   return(ret_val);
}

   /* The following function is responsible for setting the Pairability */
   /* Mode of the local device.  This function returns zero on          */
   /* successful execution and a negative value on all errors.          */
static int SetPairabilityMode(ParameterList_t *TempParam)
{
   int                     Result;
   int                     ret_val;
   char                   *Mode;
   GAP_Pairability_Mode_t  PairabilityMode;

   /* First, check that valid Bluetooth Stack ID exists.                */
   if(BluetoothStackID)
   {
      /* Make sure that all of the parameters required for this function*/
      /* appear to be at least semi-valid.                              */
      if((TempParam) && (TempParam->NumberofParameters > 0) && (TempParam->Params[0].intParam >= 0) && (TempParam->Params[0].intParam <= 2))
      {
         /* Parameters appear to be valid, map the specified parameters */
         /* into the API specific parameters.                           */
         if(TempParam->Params[0].intParam == 0)
         {
            PairabilityMode = pmNonPairableMode;
            Mode            = "pmNonPairableMode";
         }
         else
         {
            if(TempParam->Params[0].intParam == 1)
            {
               PairabilityMode = pmPairableMode;
               Mode            = "pmPairableMode";
            }
            else
            {
               PairabilityMode = pmPairableMode_EnableSecureSimplePairing;
               Mode            = "pmPairableMode_EnableSecureSimplePairing";
            }
         }

         /* Parameters mapped, now set the Pairability Mode.            */
         Result = GAP_Set_Pairability_Mode(BluetoothStackID, PairabilityMode);

         /* Next, check the return value to see if the command was      */
         /* issued successfully.                                        */
         if(Result >= 0)
         {
            /* The Mode was changed successfully.                       */
            Display(("Pairability Mode Changed to %s.\r\n", Mode));

            /* If Secure Simple Pairing has been enabled, inform the    */
            /* user of the current Secure Simple Pairing parameters.    */
            if(PairabilityMode == pmPairableMode_EnableSecureSimplePairing)
               DisplayIOCapabilities();

            /* Flag success to the caller.                              */
            ret_val = 0;
         }
         else
         {
            /* There was an error setting the Mode.                     */
            DisplayFunctionError("GAP_Set_Pairability_Mode", Result);

            /* Flag that an error occurred while submitting the command.*/
            ret_val = FUNCTION_ERROR;
         }
      }
      else
      {
         /* One or more of the necessary parameters is/are invalid.     */
         DisplayUsage("SetPairabilityMode [Mode (0 = Non Pairable, 1 = Pairable, 2 = Pairable (Secure Simple Pairing)]");

         ret_val = INVALID_PARAMETERS_ERROR;
      }
   }
   else
   {
      /* No valid Bluetooth Stack ID exists.                            */
      ret_val = INVALID_STACK_ID_ERROR;
   }

   return(ret_val);
}



   /* The following function is responsible for changing the Secure     */
   /* Simple Pairing Parameters that are exchanged during the Pairing   */
   /* procedure when Secure Simple Pairing (Security Level 4) is used.  */
   /* This function returns zero on successful execution and a negative */
   /* value on all errors.                                              */
static int ChangeSimplePairingParameters(ParameterList_t *TempParam)
{
   int ret_val = 0;

   /* First, check that valid Bluetooth Stack ID exists.                */
   if(BluetoothStackID)
   {
      /* Make sure that all of the parameters required for this function*/
      /* appear to be at least semi-valid.                              */
      if((TempParam) && (TempParam->NumberofParameters >= 2) && (TempParam->Params[0].intParam >= 0) && (TempParam->Params[0].intParam <= 3))
      {
         /* Parameters appear to be valid, map the specified parameters */
         /* into the API specific parameters.                           */
         if(TempParam->Params[0].intParam == 0)
            IOCapability = icDisplayOnly;
         else
         {
            if(TempParam->Params[0].intParam == 1)
               IOCapability = icDisplayYesNo;
            else
            {
               if(TempParam->Params[0].intParam == 2)
                  IOCapability = icKeyboardOnly;
               else
                  IOCapability = icNoInputNoOutput;
            }
         }

         /* Finally map the Man in the Middle (MITM) Protection valid.  */
         MITMProtection = (Boolean_t)(TempParam->Params[1].intParam?TRUE:FALSE);

         /* Inform the user of the New I/O Capabilities.                */
         DisplayIOCapabilities();

         /* Flag success to the caller.                                 */
         ret_val = 0;
      }
      else
      {
         /* One or more of the necessary parameters is/are invalid.     */
         DisplayUsage("ChangeSimplePairingParameters [I/O Capability (0 = Display Only, 1 = Display Yes/No, 2 = Keyboard Only, 3 = No Input/Output)] [MITM Requirement (0 = No, 1 = Yes)]");

         ret_val = INVALID_PARAMETERS_ERROR;
      }
   }
   else
   {
      /* No valid Bluetooth Stack ID exists.                            */
      ret_val = INVALID_STACK_ID_ERROR;
   }

   return(ret_val);
}

   /* The following function is responsible for initiating bonding with */
   /* a remote device.  This function returns zero on successful        */
   /* execution and a negative value on all errors.                     */
static int Pair(ParameterList_t *TempParam)
{
   int                Result;
   int                ret_val;
   GAP_Bonding_Type_t BondingType;

   /* First, check that valid Bluetooth Stack ID exists.                */
   if(BluetoothStackID)
   {
      /* Next, make sure that we are not already connected.             */
      if(!Connected)
      {
         /* There are currently no active connections, make sure that   */
         /* all of the parameters required for this function appear to  */
         /* be at least semi-valid.                                     */
         if((TempParam) && (TempParam->NumberofParameters > 0) && (TempParam->Params[0].intParam) && (NumberofValidResponses) && (TempParam->Params[0].intParam <= NumberofValidResponses) && (!COMPARE_NULL_BD_ADDR(InquiryResultList[(TempParam->Params[0].intParam - 1)])))
         {
            /* Check to see if General Bonding was specified.           */
            if(TempParam->NumberofParameters > 1)
               BondingType = TempParam->Params[1].intParam?btGeneral:btDedicated;
            else
               BondingType = btDedicated;

            /* Before we submit the command to the stack, we need to    */
            /* make sure that we clear out any Link Key we have stored  */
            /* for the specified device.                                */
            DeleteLinkKey(InquiryResultList[(TempParam->Params[0].intParam - 1)]);

            /* Attempt to submit the command.                           */
            Result = GAP_Initiate_Bonding(BluetoothStackID, InquiryResultList[(TempParam->Params[0].intParam - 1)], BondingType, GAP_Event_Callback, (unsigned long)0);

            /* Check the return value of the submitted command for      */
            /* success.                                                 */
            if(!Result)
            {
               /* Display a message indicating that Bonding was         */
               /* initiated successfully.                               */
               Display(("GAP_Initiate_Bonding(%s): Success.\r\n", (BondingType == btDedicated)?"Dedicated":"General"));

               /* Flag success to the caller.                           */
               ret_val = 0;
            }
            else
            {
               /* Display a message indicating that an error occurred   */
               /* while initiating bonding.                             */
               DisplayFunctionError("GAP_Initiate_Bonding", Result);

               ret_val = FUNCTION_ERROR;
            }
         }
         else
         {
            /* One or more of the necessary parameters is/are invalid.  */
            DisplayUsage("Pair [Inquiry Index] [0 = Dedicated, 1 = General (optional)]");

            ret_val = INVALID_PARAMETERS_ERROR;
         }
      }
      else
      {
         /* Display an error to the user describing that Pairing can    */
         /* only occur when we are not connected.                       */
         Display(("Only valid when not connected.\r\n"));

         ret_val = FUNCTION_ERROR;
      }
   }
   else
   {
      /* No valid Bluetooth Stack ID exists.                            */
      ret_val = INVALID_STACK_ID_ERROR;
   }

   return(ret_val);
}

   /* The following function is responsible for ending a previously     */
   /* initiated bonding session with a remote device.  This function    */
   /* returns zero on successful execution and a negative value on all  */
   /* errors.                                                           */
static int EndPairing(ParameterList_t *TempParam)
{
   int Result;
   int ret_val;

   /* First, check that valid Bluetooth Stack ID exists.                */
   if(BluetoothStackID)
   {
      /* Make sure that all of the parameters required for this function*/
      /* appear to be at least semi-valid.                              */
      if((TempParam) && (TempParam->NumberofParameters > 0) && (TempParam->Params[0].intParam) && (NumberofValidResponses) && (TempParam->Params[0].intParam <= NumberofValidResponses) && (!COMPARE_NULL_BD_ADDR(InquiryResultList[(TempParam->Params[0].intParam - 1)])))
      {
         /* Attempt to submit the command.                              */
         Result = GAP_End_Bonding(BluetoothStackID, InquiryResultList[(TempParam->Params[0].intParam - 1)]);

         /* Check the return value of the submitted command for success.*/
         if(!Result)
         {
            /* Display a message indicating that the End bonding was    */
            /* successfully submitted.                                  */
            DisplayFunctionSuccess("GAP_End_Bonding");

            /* Flag success to the caller.                              */
            ret_val = 0;

            /* Flag that there is no longer a current Authentication    */
            /* procedure in progress.                                   */
            ASSIGN_BD_ADDR(CurrentRemoteBD_ADDR, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
         }
         else
         {
            /* Display a message indicating that an error occurred while*/
            /* ending bonding.                                          */
            DisplayFunctionError("GAP_End_Bonding", Result);

            ret_val = FUNCTION_ERROR;
         }
      }
      else
      {
         /* One or more of the necessary parameters is/are invalid.     */
         DisplayUsage("EndPairing [Inquiry Index]");

         ret_val = INVALID_PARAMETERS_ERROR;
      }
   }
   else
   {
      /* No valid Bluetooth Stack ID exists.                            */
      ret_val = INVALID_STACK_ID_ERROR;
   }

   return(ret_val);
}

   /* The following function is responsible for issuing a GAP           */
   /* Authentication Response with a PIN Code value specified via the   */
   /* input parameter.  This function returns zero on successful        */
   /* execution and a negative value on all errors.                     */
static int PINCodeResponse(ParameterList_t *TempParam)
{
   int                              Result;
   int                              ret_val;
   PIN_Code_t                       PINCode;
   GAP_Authentication_Information_t GAP_Authentication_Information;

   /* First, check that valid Bluetooth Stack ID exists.                */
   if(BluetoothStackID)
   {
      /* First, check to see if there is an on-going Pairing operation  */
      /* active.                                                        */
      if(!COMPARE_NULL_BD_ADDR(CurrentRemoteBD_ADDR))
      {
         /* Make sure that all of the parameters required for this      */
         /* function appear to be at least semi-valid.                  */
         if((TempParam) && (TempParam->NumberofParameters > 0) && (TempParam->Params[0].strParam) && (BTPS_StringLength(TempParam->Params[0].strParam) > 0) && (BTPS_StringLength(TempParam->Params[0].strParam) <= sizeof(PIN_Code_t)))
         {
            /* Parameters appear to be valid, go ahead and convert the  */
            /* input parameter into a PIN Code.                         */

            /* Initialize the PIN code.                                 */
            ASSIGN_PIN_CODE(PINCode, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

            BTPS_MemCopy(&PINCode, TempParam->Params[0].strParam, BTPS_StringLength(TempParam->Params[0].strParam));

            /* Populate the response structure.                         */
            GAP_Authentication_Information.GAP_Authentication_Type      = atPINCode;
            GAP_Authentication_Information.Authentication_Data_Length   = (Byte_t)(BTPS_StringLength(TempParam->Params[0].strParam));
            GAP_Authentication_Information.Authentication_Data.PIN_Code = PINCode;

            /* Submit the Authentication Response.                      */
            Result = GAP_Authentication_Response(BluetoothStackID, CurrentRemoteBD_ADDR, &GAP_Authentication_Information);

            /* Check the return value for the submitted command for     */
            /* success.                                                 */
            if(!Result)
            {
               /* Operation was successful, inform the user.            */
               Display(("GAP_Authentication_Response(), Pin Code Response Success.\r\n"));

               /* Flag success to the caller.                           */
               ret_val = 0;
            }
            else
            {
               /* Inform the user that the Authentication Response was  */
               /* not successful.                                       */
               Display(("GAP_Authentication_Response() Failure: %d.\r\n", Result));

               ret_val = FUNCTION_ERROR;
            }

            /* Flag that there is no longer a current Authentication    */
            /* procedure in progress.                                   */
            ASSIGN_BD_ADDR(CurrentRemoteBD_ADDR, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
         }
         else
         {
            /* One or more of the necessary parameters is/are invalid.  */
            DisplayUsage("PINCodeResponse [PIN Code]");

            ret_val = INVALID_PARAMETERS_ERROR;
         }
      }
      else
      {
         /* There is not currently an on-going authentication operation,*/
         /* inform the user of this error condition.                    */
         Display(("PIN Code Authentication Response: Authentication not in progress.\r\n"));

         ret_val = FUNCTION_ERROR;
      }
   }
   else
   {
      /* No valid Bluetooth Stack ID exists.                            */
      ret_val = INVALID_STACK_ID_ERROR;
   }

   return(ret_val);
}

   /* The following function is responsible for issuing a GAP           */
   /* Authentication Response with a Pass Key value specified via the   */
   /* input parameter.  This function returns zero on successful        */
   /* execution and a negative value on all errors.                     */
static int PassKeyResponse(ParameterList_t *TempParam)
{
   int                              Result;
   int                              ret_val;
   GAP_Authentication_Information_t GAP_Authentication_Information;

   /* First, check that valid Bluetooth Stack ID exists.                */
   if(BluetoothStackID)
   {
      /* First, check to see if there is an on-going Pairing operation  */
      /* active.                                                        */
      if(!COMPARE_NULL_BD_ADDR(CurrentRemoteBD_ADDR))
      {
         /* Make sure that all of the parameters required for this      */
         /* function appear to be at least semi-valid.                  */
         if((TempParam) && (TempParam->NumberofParameters > 0) && (BTPS_StringLength(TempParam->Params[0].strParam) <= GAP_PASSKEY_MAXIMUM_NUMBER_OF_DIGITS))
         {
            /* Parameters appear to be valid, go ahead and populate the */
            /* response structure.                                      */
            GAP_Authentication_Information.GAP_Authentication_Type     = atPassKey;
            GAP_Authentication_Information.Authentication_Data_Length  = (Byte_t)(sizeof(DWord_t));
            GAP_Authentication_Information.Authentication_Data.Passkey = (DWord_t)(TempParam->Params[0].intParam);

            /* Submit the Authentication Response.                      */
            Result = GAP_Authentication_Response(BluetoothStackID, CurrentRemoteBD_ADDR, &GAP_Authentication_Information);

            /* Check the return value for the submitted command for     */
            /* success.                                                 */
            if(!Result)
            {
               /* Operation was successful, inform the user.            */
               DisplayFunctionSuccess("Passkey Response Success");

               /* Flag success to the caller.                           */
               ret_val = 0;
            }
            else
            {
               /* Inform the user that the Authentication Response was  */
               /* not successful.                                       */
               DisplayFunctionError("GAP_Authentication_Response", Result);

               ret_val = FUNCTION_ERROR;
            }

            /* Flag that there is no longer a current Authentication    */
            /* procedure in progress.                                   */
            ASSIGN_BD_ADDR(CurrentRemoteBD_ADDR, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
         }
         else
         {
            /* One or more of the necessary parameters is/are invalid.  */
            Display(("PassKeyResponse[Numeric Passkey(0 - 999999)].\r\n"));

            ret_val = INVALID_PARAMETERS_ERROR;
         }
      }
      else
      {
         /* There is not currently an on-going authentication operation,*/
         /* inform the user of this error condition.                    */
         Display(("Pass Key Authentication Response: Authentication not in progress.\r\n"));

         ret_val = FUNCTION_ERROR;
      }
   }
   else
   {
      /* No valid Bluetooth Stack ID exists.                            */
      ret_val = INVALID_STACK_ID_ERROR;
   }

   return(ret_val);
}

   /* The following function is responsible for issuing a GAP           */
   /* Authentication Response with a User Confirmation value specified  */
   /* via the input parameter.  This function returns zero on successful*/
   /* execution and a negative value on all errors.                     */
static int UserConfirmationResponse(ParameterList_t *TempParam)
{
   int                              Result;
   int                              ret_val;
   GAP_Authentication_Information_t GAP_Authentication_Information;

   /* First, check that valid Bluetooth Stack ID exists.                */
   if(BluetoothStackID)
   {
      /* First, check to see if there is an on-going Pairing operation  */
      /* active.                                                        */
      if(!COMPARE_NULL_BD_ADDR(CurrentRemoteBD_ADDR))
      {
         /* Make sure that all of the parameters required for this      */
         /* function appear to be at least semi-valid.                  */
         if((TempParam) && (TempParam->NumberofParameters > 0))
         {
            /* Parameters appear to be valid, go ahead and populate the */
            /* response structure.                                      */
            GAP_Authentication_Information.GAP_Authentication_Type          = atUserConfirmation;
            GAP_Authentication_Information.Authentication_Data_Length       = (Byte_t)(sizeof(Byte_t));
            GAP_Authentication_Information.Authentication_Data.Confirmation = (Boolean_t)(TempParam->Params[0].intParam?TRUE:FALSE);

            /* Submit the Authentication Response.                      */
            Result = GAP_Authentication_Response(BluetoothStackID, CurrentRemoteBD_ADDR, &GAP_Authentication_Information);

            /* Check the return value for the submitted command for     */
            /* success.                                                 */
            if(!Result)
            {
               /* Operation was successful, inform the user.            */
               DisplayFunctionSuccess("User Confirmation Response");

               /* Flag success to the caller.                           */
               ret_val = 0;
            }
            else
            {
               /* Inform the user that the Authentication Response was  */
               /* not successful.                                       */
               DisplayFunctionError("GAP_Authentication_Response", Result);

               ret_val = FUNCTION_ERROR;
            }

            /* Flag that there is no longer a current Authentication    */
            /* procedure in progress.                                   */
            ASSIGN_BD_ADDR(CurrentRemoteBD_ADDR, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
         }
         else
         {
            /* One or more of the necessary parameters is/are invalid.  */
            DisplayUsage("UserConfirmationResponse [Confirmation(0 = No, 1 = Yes)]");
            ret_val = INVALID_PARAMETERS_ERROR;
         }
      }
      else
      {
         /* There is not currently an on-going authentication operation,*/
         /* inform the user of this error condition.                    */
         Display(("User Confirmation Authentication Response: Authentication is not currently in progress.\r\n"));

         ret_val = FUNCTION_ERROR;
      }
   }
   else
   {
      /* No valid Bluetooth Stack ID exists.                            */
      ret_val = INVALID_STACK_ID_ERROR;
   }

   return(ret_val);
}

   /* The following function is responsible for querying the Bluetooth  */
   /* Device Address of the local Bluetooth Device.  This function      */
   /* returns zero on successful execution and a negative value on all  */
   /* errors.                                                           */
static int GetLocalAddress(ParameterList_t *TempParam)
{
   int       Result;
   int       ret_val;
   BD_ADDR_t BD_ADDR;

   /* First, check that valid Bluetooth Stack ID exists.                */
   if(BluetoothStackID)
   {
      /* Attempt to submit the command.                                 */
      Result = GAP_Query_Local_BD_ADDR(BluetoothStackID, &BD_ADDR);

      /* Check the return value of the submitted command for success.   */
      if(!Result)
      {
         BD_ADDRToStr(BD_ADDR, Function_BoardStr);

         Display(("BD_ADDR of Local Device is: %s.\r\n", Function_BoardStr));

         /* Flag success to the caller.                                 */
         ret_val = 0;
      }
      else
      {
         /* Display a message indicating that an error occurred while   */
         /* attempting to query the Local Device Address.               */
         Display(("GAP_Query_Local_BD_ADDR() Failure: %d.\r\n", Result));

         ret_val = FUNCTION_ERROR;
      }
   }
   else
   {
      /* No valid Bluetooth Stack ID exists.                            */
      ret_val = INVALID_STACK_ID_ERROR;
   }

   return(ret_val);
}

   /* The following function is responsible for setting the name of the */
   /* local Bluetooth Device to a specified name.  This function returns*/
   /* zero on successful execution and a negative value on all errors.  */
static int SetLocalName(ParameterList_t *TempParam)
{
   int                                   Result;
   int                                   ret_val = 0;
   Extended_Inquiry_Response_Data_t     *ExtendedInquiryResponseData;
   GAP_Extended_Inquiry_Response_Data_t  GAPExtendedInquiryResponseData;

   /* First, check that valid Bluetooth Stack ID exists.                */
   if(BluetoothStackID)
   {
      /* Make sure that all of the parameters required for this function*/
      /* appear to be at least semi-valid.                              */
      if((TempParam) && (TempParam->NumberofParameters > 0) && (TempParam->Params[0].strParam))
      {
         /* Attempt to submit the command.                              */
         Result = GAP_Set_Local_Device_Name(BluetoothStackID, TempParam->Params[0].strParam);

         /* Check the return value of the submitted command for success.*/
         if(!Result)
         {
            /* Display a message indicating that the Device Name was    */
            /* successfully submitted.                                  */
            Display(("Local Device Name: %s.\r\n", TempParam->Params[0].strParam));

            /* Since we changed the local name, go ahead and update the */
            /* EIR data as well.                                        */
            if((ExtendedInquiryResponseData = (Extended_Inquiry_Response_Data_t *)BTPS_AllocateMemory(sizeof(Extended_Inquiry_Response_Data_t))) != NULL)
            {
               /* Fix up the EIR data to reflect the new name.             */
               GAPExtendedInquiryResponseDataEntries[0].Data_Length = BTPS_StringLength(TempParam->Params[0].strParam) + 1;
               GAPExtendedInquiryResponseDataEntries[0].Data_Buffer = (Byte_t *)(TempParam->Params[0].strParam);

               GAPExtendedInquiryResponseData.Number_Data_Entries = sizeof(GAPExtendedInquiryResponseDataEntries)/sizeof(GAP_Extended_Inquiry_Response_Data_Entry_t);
               GAPExtendedInquiryResponseData.Data_Entries        = GAPExtendedInquiryResponseDataEntries;

               /* Format the EIR data and write it.                        */
               if(GAP_Convert_Extended_Inquiry_Response_Data(&GAPExtendedInquiryResponseData, ExtendedInquiryResponseData))
                  GAP_Write_Extended_Inquiry_Information(BluetoothStackID, HCI_EXTENDED_INQUIRY_RESPONSE_FEC_REQUIRED, ExtendedInquiryResponseData);

               BTPS_FreeMemory(ExtendedInquiryResponseData);
            }

            /* Flag success to the caller.                              */
            ret_val = 0;
         }
         else
         {
            /* Display a message indicating that an error occurred while*/
            /* attempting to set the local Device Name.                 */
            DisplayFunctionError("GAP_Set_Local_Device_Name", Result);

            ret_val = FUNCTION_ERROR;
         }
      }
      else
      {
         /* One or more of the necessary parameters is/are invalid.     */
         DisplayUsage("SetLocalName [Local Name]");

         ret_val = INVALID_PARAMETERS_ERROR;
      }
   }
   else
   {
      /* No valid Bluetooth Stack ID exists.                            */
      ret_val = INVALID_STACK_ID_ERROR;
   }

   return(ret_val);
}

   /* The following function is responsible for querying the Bluetooth  */
   /* Device Name of the local Bluetooth Device.  This function returns */
   /* zero on successful execution and a negative value on all errors.  */
static int GetLocalName(ParameterList_t *TempParam)
{
   int   Result;
   int   ret_val;
   char *LocalName;

   /* First, check that valid Bluetooth Stack ID exists.                */
   if(BluetoothStackID)
   {
      /* Allocate a Buffer to hold the Local Name.                      */
      if((LocalName = BTPS_AllocateMemory(257)) != NULL)
      {
         /* Attempt to submit the command.                              */
         Result = GAP_Query_Local_Device_Name(BluetoothStackID, 257, (char *)LocalName);

         /* Check the return value of the submitted command for success.*/
         if(!Result)
         {
            Display(("Name of Local Device is: %s.\r\n", LocalName));

            /* Flag success to the caller.                              */
            ret_val = 0;
         }
         else
         {
            /* Display a message indicating that an error occurred while*/
            /* attempting to query the Local Device Name.               */
            Display(("GAP_Query_Local_Device_Name() Failure: %d.\r\n", Result));

            ret_val = FUNCTION_ERROR;
         }

         BTPS_FreeMemory(LocalName);
      }
      else
      {
         Display(("Failed to allocate buffer to hold Local Name.\r\n"));

         ret_val = FUNCTION_ERROR;
      }
   }
   else
   {
      /* No valid Bluetooth Stack ID exists.                            */
      ret_val = INVALID_STACK_ID_ERROR;
   }

   return(ret_val);
}

   /* The following function is responsible for setting the Class of    */
   /* Device of the local Bluetooth Device to a Class Of Device value.  */
   /* This function returns zero on successful execution and a negative */
   /* value on all errors.                                              */
static int SetClassOfDevice(ParameterList_t *TempParam)
{
   int               Result;
   int               ret_val;
   Class_of_Device_t Class_of_Device;

   /* First, check that valid Bluetooth Stack ID exists.                */
   if(BluetoothStackID)
   {
      /* Make sure that all of the parameters required for this function*/
      /* appear to be at least semi-valid.                              */
      if((TempParam) && (TempParam->NumberofParameters > 0))
      {
         /* Attempt to submit the command.                              */
         ASSIGN_CLASS_OF_DEVICE(Class_of_Device, (Byte_t)((TempParam->Params[0].intParam) & 0xFF), (Byte_t)(((TempParam->Params[0].intParam) >> 8) & 0xFF), (Byte_t)(((TempParam->Params[0].intParam) >> 16) & 0xFF));

         Result = GAP_Set_Class_Of_Device(BluetoothStackID, Class_of_Device);

         /* Check the return value of the submitted command for success.*/
         if(!Result)
         {
            /* Display a message indicating that the Class of Device was*/
            /* successfully submitted.                                  */
            DisplayClassOfDevice(Class_of_Device);

            /* Flag success to the caller.                              */
            ret_val = 0;
         }
         else
         {
            /* Display a message indicating that an error occurred while*/
            /* attempting to set the local Class of Device.             */
            DisplayFunctionError("GAP_Set_Class_Of_Device", Result);

            ret_val = FUNCTION_ERROR;
         }
      }
      else
      {
         /* One or more of the necessary parameters is/are invalid.     */
         DisplayUsage("SetClassOfDevice [Class of Device]");

         ret_val = INVALID_PARAMETERS_ERROR;
      }
   }
   else
   {
      /* No valid Bluetooth Stack ID exists.                            */
      ret_val = INVALID_STACK_ID_ERROR;
   }

   return(ret_val);
}

   /* The following function is responsible for querying the Bluetooth  */
   /* Class of Device of the local Bluetooth Device.  This function     */
   /* returns zero on successful execution and a negative value on all  */
   /* errors.                                                           */
static int GetClassOfDevice(ParameterList_t *TempParam)
{
   int               Result;
   int               ret_val;
   Class_of_Device_t Class_of_Device;

   /* First, check that valid Bluetooth Stack ID exists.                */
   if(BluetoothStackID)
   {
      /* Attempt to submit the command.                                 */
      Result = GAP_Query_Class_Of_Device(BluetoothStackID, &Class_of_Device);

      /* Check the return value of the submitted command for success.   */
      if(!Result)
      {
         Display(("Local Class of Device is: 0x%02X%02X%02X.\r\n", Class_of_Device.Class_of_Device0, Class_of_Device.Class_of_Device1, Class_of_Device.Class_of_Device2));

         /* Flag success to the caller.                                 */
         ret_val = 0;
      }
      else
      {
         /* Display a message indicating that an error occurred while   */
         /* attempting to query the Local Class of Device.              */
         Display(("GAP_Query_Class_Of_Device() Failure: %d.\r\n", Result));

         ret_val = FUNCTION_ERROR;
      }
   }
   else
   {
      /* No valid Bluetooth Stack ID exists.                            */
      ret_val = INVALID_STACK_ID_ERROR;
   }

   return(ret_val);
}

   /* The following function is responsible for querying the Bluetooth  */
   /* Device Name of the specified remote Bluetooth Device.  This       */
   /* function returns zero on successful execution and a negative value*/
   /* on all errors.                                                    */
static int GetRemoteName(ParameterList_t *TempParam)
{
   int Result;
   int ret_val;

   /* First, check that valid Bluetooth Stack ID exists.                */
   if(BluetoothStackID)
   {
      /* Make sure that all of the parameters required for this function*/
      /* appear to be at least semi-valid.                              */
      if((TempParam) && (TempParam->NumberofParameters > 0) && (TempParam->Params[0].intParam) && (NumberofValidResponses) && (TempParam->Params[0].intParam <= NumberofValidResponses) && (!COMPARE_NULL_BD_ADDR(InquiryResultList[(TempParam->Params[0].intParam - 1)])))
      {
         /* Attempt to submit the command.                              */
         Result = GAP_Query_Remote_Device_Name(BluetoothStackID, InquiryResultList[(TempParam->Params[0].intParam - 1)], GAP_Event_Callback, (unsigned long)0);

         /* Check the return value of the submitted command for success.*/
         if(!Result)
         {
            /* Display a message indicating that Remote Name request was*/
            /* initiated successfully.                                  */
            DisplayFunctionSuccess("GAP_Query_Remote_Device_Name");

            /* Flag success to the caller.                              */
            ret_val = 0;
         }
         else
         {
            /* Display a message indicating that an error occurred while*/
            /* initiating the Remote Name request.                      */
            DisplayFunctionError("GAP_Query_Remote_Device_Name", Result);

            ret_val = FUNCTION_ERROR;
         }
      }
      else
      {
         /* One or more of the necessary parameters is/are invalid.     */
         DisplayUsage("GetRemoteName [Inquiry Index]");

         ret_val = INVALID_PARAMETERS_ERROR;
      }
   }
   else
   {
      /* No valid Bluetooth Stack ID exists.                            */
      ret_val = INVALID_STACK_ID_ERROR;
   }

   return(ret_val);
}

   /* The following function is responsible for putting a specified     */
   /* connection into HCI Sniff Mode with passed in parameters.         */
static int SniffMode(ParameterList_t *TempParam)
{
   int    Result;
   int    ret_val;
   Word_t Sniff_Max_Interval;
   Word_t Sniff_Min_Interval;
   Word_t Sniff_Attempt;
   Word_t Sniff_Timeout;
   Byte_t Status;

   /* First, check that valid Bluetooth Stack ID exists.                */
   if(BluetoothStackID)
   {
      /* Make sure that all of the parameters required for this function*/
      /* appear to be at least semi-valid.                              */
      if((TempParam) && (TempParam->NumberofParameters >= 4))
      {
         Sniff_Max_Interval   = (Word_t)MILLISECONDS_TO_BASEBAND_SLOTS(TempParam->Params[0].intParam);
         Sniff_Min_Interval   = (Word_t)MILLISECONDS_TO_BASEBAND_SLOTS(TempParam->Params[1].intParam);
         Sniff_Attempt        = TempParam->Params[2].intParam;
         Sniff_Timeout        = TempParam->Params[3].intParam;

         /* Make sure the Sniff Mode parameters seem semi valid.        */
         if((Sniff_Attempt) && (Sniff_Max_Interval) && (Sniff_Min_Interval) && (Sniff_Min_Interval < Sniff_Max_Interval))
         {
            /* Make sure the connection handle is valid.                */
            if(Connection_Handle)
            {
               /* Now that we have the connection try and go to Sniff.  */
               Result = HCI_Sniff_Mode(BluetoothStackID, Connection_Handle, Sniff_Max_Interval, Sniff_Min_Interval, Sniff_Attempt, Sniff_Timeout, &Status);
               if(!Result)
               {
                  /* Check the Command Status.                          */
                  if(!Status)
                  {
                     DisplayFunctionSuccess("HCI_Sniff_Mode()");

                     /* Return success to the caller.                   */
                     ret_val = 0;
                  }
                  else
                  {
                     Display(("HCI_Sniff_Mode() Returned Non-Zero Status: 0x%02X\r\n", Status));

                     ret_val = FUNCTION_ERROR;
                  }
               }
               else
               {
                  DisplayFunctionError("HCI_Sniff_Mode()", Result);

                  ret_val = FUNCTION_ERROR;
               }
            }
            else
            {
               Display(("Invalid Connection Handle.\r\n"));

               ret_val = FUNCTION_ERROR;
            }
         }
         else
         {
            /* One or more of the necessary parameters is/are invalid.  */
            DisplayUsage("SniffMode [MaxInterval] [MinInterval] [Attempt] [Timeout]");

            ret_val = INVALID_PARAMETERS_ERROR;
         }
      }
      else
      {
         /* One or more of the necessary parameters is/are invalid.     */
         DisplayUsage("SniffMode [MaxInterval] [MinInterval] [Attempt] [Timeout]");

         ret_val = INVALID_PARAMETERS_ERROR;
      }
   }
   else
   {
      /* No valid Bluetooth Stack ID exists.                            */
      ret_val = INVALID_STACK_ID_ERROR;
   }

   return(ret_val);
}

   /* The following function is responsible for attempting to Exit      */
   /* Sniff Mode for a specified connection.                            */
static int ExitSniffMode(ParameterList_t *TempParam)
{
   int    Result;
   int    ret_val;
   Byte_t Status;

   /* First, check that valid Bluetooth Stack ID exists.                */
   if(BluetoothStackID)
   {
      /* Make sure that all of the parameters required for this function*/
      /* appear to be at least semi-valid.                              */
      if((Connection_Handle) && (TempParam))
      {
         /* Attempt to Exit Sniff Mode for the Specified Device.        */
         Result = HCI_Exit_Sniff_Mode(BluetoothStackID, Connection_Handle, &Status);
         if(!Result)
         {
            if(!Status)
            {
               /* Flag that HCI_Exit_Sniff_Mode was successful.         */
               DisplayFunctionSuccess("HCI_Exit_Sniff_Mode()");

               ret_val = 0;
            }
            else
            {
               /* We received a failure status in the Command Status    */
               Display(("HCI_Exit_Sniff_Mode() Returned Non-Zero Status: 0x%02X.\r\n", Status));
               ret_val = FUNCTION_ERROR;
            }
         }
         else
         {
            /* Failed to get exit sniff mode.                           */
            DisplayFunctionError("HCI_Exit_Sniff_Mode()", Result);

            /* Return function error to the caller.                     */
            ret_val = FUNCTION_ERROR;
         }
      }
      else
      {
         /* One or more of the necessary parameters is/are invalid.     */
         DisplayUsage("ExitSniffMode");

         ret_val = INVALID_PARAMETERS_ERROR;
      }
   }
   else
   {
      /* No valid Bluetooth Stack ID exists.                            */
      ret_val = INVALID_STACK_ID_ERROR;
   }

   return(ret_val);
}

   /* The following function is responsible for opening a Serial Port   */
   /* Server on the Local Device.  This function opens the Serial Port  */
   /* Server on the specified RFCOMM Channel.  This function returns    */
   /* zero if successful, or a negative return value if an error        */
   /* occurred.                                                         */
static int OpenServer(ParameterList_t *TempParam)
{
   int  ret_val;
   char *ServiceName;

   /* First check to see if a valid Bluetooth Stack ID exists.          */
   if(BluetoothStackID)
   {
      /* Make sure that there is not already a Serial Port Server open. */
      if(!SerialPortID)
      {
         /* Next, check to see if the parameters specified are valid.   */
         if((TempParam) && (TempParam->NumberofParameters >= 1) && (TempParam->Params[0].intParam))
         {
            /* Simply attempt to open an Serial Server, on RFCOMM Server*/
            /* Port 1.                                                  */
            ret_val = ISPP_Open_Server_Port(BluetoothStackID, TempParam->Params[0].intParam, ISPP_Event_Callback, (unsigned long)0);

            /* If the Open was successful, then note the Serial Port    */
            /* Server ID.                                               */
            if(ret_val > 0)
            {
               /* Note the Serial Port Server ID of the opened Serial   */
               /* Port Server.                                          */
               SerialPortID = ret_val;

               /* Create a Buffer to hold the Service Name.             */
               if((ServiceName = BTPS_AllocateMemory(64)) != NULL)
               {
                  /* The Server was opened successfully, now register a */
                  /* SDP Record indicating that an Serial Port Server   */
                  /* exists. Do this by first creating a Service Name.  */
                  BTPS_SprintF(ServiceName, "Serial Port Server Port %d (iSPP)", (int)(TempParam->Params[0].intParam));

                  /* Now that a Service Name has been created try to    */
                  /* Register the SDP Record.                           */
                  ret_val = ISPP_Register_SDP_Record(BluetoothStackID, SerialPortID, NULL, ServiceName, &SPPServerSDPHandle);

                  /* If there was an error creating the Serial Port     */
                  /* Server's SDP Service Record then go ahead an close */
                  /* down the server an flag an error.                  */
                  if(ret_val < 0)
                  {
                     Display(("Unable to Register Server SDP Record, Error = %d.\r\n", ret_val));

                     ISPP_Close_Server_Port(BluetoothStackID, SerialPortID);

                     /* Flag that there is no longer an Serial Port     */
                     /* Server Open.                                    */
                     SerialPortID = 0;

                     /* Flag that we are no longer connected.           */
                     Connected    = FALSE;
                     Connecting   = FALSE;

                     ret_val      = UNABLE_TO_REGISTER_SERVER;
                  }
                  else
                  {
                     /* Simply flag to the user that everything         */
                     /* initialized correctly.                          */
                     Display(("Server Opened: %d.\r\n", TempParam->Params[0].intParam));

                     /* Flag success to the caller.                     */
                     ret_val = 0;
                  }

                  /* Free the Service Name buffer.                      */
                  BTPS_FreeMemory(ServiceName);
               }
               else
               {
                  Display(("Failed to allocate buffer to hold Service Name in SDP Record.\r\n"));
               }
            }
            else
            {
               Display(("Unable to Open Server on: %d, Error = %d.\r\n", TempParam->Params[0].intParam, ret_val));

               ret_val = UNABLE_TO_REGISTER_SERVER;
            }
         }
         else
         {
            DisplayUsage("Open [Port Number]");

            ret_val = INVALID_PARAMETERS_ERROR;
         }
      }
      else
      {
         /* A Server is already open, this program only supports one    */
         /* Server or Client at a time.                                 */
         Display(("Server already open.\r\n"));

         ret_val = FUNCTION_ERROR;
      }
   }
   else
   {
      /* No valid Bluetooth Stack ID exists.                            */
      ret_val = INVALID_STACK_ID_ERROR;
   }

   return(ret_val);
}

   /* The following function is responsible for closing a Serial Port   */
   /* Server that was previously opened via a successful call to the    */
   /* OpenServer() function.  This function returns zero if successful  */
   /* or a negative return error code if there was an error.            */
static int CloseServer(ParameterList_t *TempParam)
{
   int ret_val = 0;

   /* First check to see if a valid Bluetooth Stack ID exists.          */
   if(BluetoothStackID)
   {
      /* If a Serial Port Server is already opened, then simply close   */
      /* it.                                                            */
      if(SerialPortID)
      {
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
         ret_val = ISPP_Close_Server_Port(BluetoothStackID, SerialPortID);

         if(ret_val < 0)
         {
            DisplayFunctionError("ISPP_Close_Server_Port", ret_val);

            ret_val = FUNCTION_ERROR;
         }
         else
            ret_val = 0;

         /* Flag that there is no Serial Port Server currently open.    */
         SerialPortID = 0;

         if(iAPTimerID)
            BSC_StopTimer(BluetoothStackID, iAPTimerID);

         iAPTimerID = 0;

         /* Flag that we are no longer connected.                       */
         Connected  = FALSE;

         Display(("Server Closed.\r\n"));
      }
      else
      {
         Display(("NO Server open.\r\n"));

         ret_val = INVALID_PARAMETERS_ERROR;
      }
   }
   else
   {
      /* No valid Bluetooth Stack ID exists.                            */
      ret_val = INVALID_STACK_ID_ERROR;
   }

   return(ret_val);
}

   /* The following function is responsible for initiating a connection */
   /* with a Remote Serial Port Server.  This function returns zero if  */
   /* successful and a negative value if an error occurred.             */
static int OpenRemoteServer(ParameterList_t *TempParam)
{
   int ret_val;
   int Result;

   /* First, check that valid Bluetooth Stack ID exists.                */
   if(BluetoothStackID)
   {
      /* Next, check that we are in client mode.                        */
      if(UI_Mode == UI_MODE_IS_CLIENT)
      {
         /* Next, let's make sure that there is not an Serial Port      */
         /* Client already open.                                        */
         if(!SerialPortID)
         {
            /* Next, let's make sure that the user has specified a      */
            /* Remote Bluetooth Device to open.                         */
            if((TempParam) && (TempParam->NumberofParameters > 1) && (TempParam->Params[0].intParam) && (NumberofValidResponses) && (TempParam->Params[0].intParam <= NumberofValidResponses) && (!COMPARE_NULL_BD_ADDR(InquiryResultList[(TempParam->Params[0].intParam - 1)])) && (TempParam->Params[1].intParam))
            {
               /* Now let's attempt to open the Remote Serial Port      */
               /* Server.                                               */
               Result = ISPP_Open_Remote_Port(BluetoothStackID, InquiryResultList[(TempParam->Params[0].intParam - 1)], TempParam->Params[1].intParam, sizeof(FIDInfo), (void *)FIDInfo, MAX_SMALL_PACKET_PAYLOAD_SIZE, ISPP_Event_Callback, (unsigned long)0);

               if(Result > 0)
               {
                  /* Inform the user that the call to open the Remote   */
                  /* Serial Port Server was successful.                 */
                  DisplayFunctionSuccess("ISPP_Open_Remote_Port");

                  /* Note the Serial Port Client ID.                    */
                  SerialPortID = Result;

                  /* Flag success to the caller.                        */
                  ret_val = 0;

                  /* Save the BD_ADDR so we can Query the Connection    */
                  /* handle when receive Connection Confirmation Event. */
                  SelectedBD_ADDR = InquiryResultList[(TempParam->Params[0].intParam - 1)];
               }
               else
               {
                  /* Inform the user that the call to Open the Remote   */
                  /* Serial Port Server failed.                         */
                  DisplayFunctionError("ISPP_Open_Remote_Port", Result);

                  /* One or more of the necessary parameters is/are     */
                  /* invalid.                                           */
                  ret_val = INVALID_PARAMETERS_ERROR;
               }
            }
            else
            {
               DisplayUsage("Open [Inquiry Index] [RFCOMM Server Port].\r\n");

               /* One or more of the necessary parameters is/are        */
               /* invalid.                                              */
               ret_val = INVALID_PARAMETERS_ERROR;
            }
         }
         else
         {
            Display(("Unable to open port.\r\n"));

            ret_val = INVALID_PARAMETERS_ERROR;
         }
      }
      else
      {
         /* One or more of the necessary parameters is/are invalid.     */
         ret_val = INVALID_PARAMETERS_ERROR;
      }
   }
   else
   {
      /* No valid Bluetooth Stack ID exists.                            */
      ret_val = INVALID_STACK_ID_ERROR;
   }

   return(ret_val);
}

   /* The following function is responsible for terminating a connection*/
   /* with a Remote Serial Port Server.  This function returns zero if  */
   /* successful and a negative value if an error occurred.             */
static int CloseRemoteServer(ParameterList_t *TempParam)
{
   int ret_val = 0;

   /* First, check that valid Bluetooth Stack ID exists.                */
   if(BluetoothStackID)
   {
      /* Next, check that we are in client mode.                        */
      if(UI_Mode == UI_MODE_IS_CLIENT)
      {
         /* Next, let's make sure that a Remote Serial Port Server is   */
         /* indeed Open.                                                */
         if(SerialPortID)
         {
            Display(("Client Port closed.\r\n"));

            /* Simply close the Serial Port Client.                     */
            ISPP_Close_Port(BluetoothStackID, SerialPortID);

            if(iAPTimerID)
               BSC_StopTimer(BluetoothStackID, iAPTimerID);

            iAPTimerID = 0;

            /* Flag that there is no longer a Serial Port Client        */
            /* connected.                                               */
            SerialPortID = 0;

            /* Flag that we are no longer connected.                    */
            Connected    = FALSE;

            /* Flag success to the caller.                              */
            ret_val      = 0;
         }
         else
         {
            /* Display an error to the user informing them that no      */
            /* Serial Port Client is open.                              */
            Display(("No Open Client Port.\r\n"));

            ret_val = INVALID_PARAMETERS_ERROR;
         }
      }
      else
      {
         /* One or more of the necessary parameters is/are invalid.     */
         ret_val = INVALID_PARAMETERS_ERROR;
      }
   }
   else
   {
      /* No valid Bluetooth Stack ID exists.                            */
      ret_val = INVALID_STACK_ID_ERROR;
   }

   return(ret_val);
}

   /* The following function is responsible for reading data that was   */
   /* received via an Open SPP port.  The function reads a fixed number */
   /* of bytes at a time from the SPP Port and displays it. If the call */
   /* to the ISPP_Data_Read() function is successful but no data is     */
   /* available to read the function displays "No data to read.".  This */
   /* function requires that a valid Bluetooth Stack ID and Serial Port */
   /* ID exist before running.  This function returns zero if successful*/
   /* and a negative value if an error occurred.                        */
static int Read(ParameterList_t *TempParam)
{
   int  ret_val;
   int  Result;
   char Buffer[32];

   /* First check to see if the parameters required for the execution of*/
   /* this function appear to be semi-valid.                            */
   if((BluetoothStackID) && (SerialPortID) && (CurrentOperatingMode == pomSPP))
   {
      /* The required parameters appear to be semi-valid, send the      */
      /* command to Read Data from SPP.                                 */
      do
      {
         Result = ISPP_Data_Read(BluetoothStackID, SerialPortID, (Word_t)(sizeof(Buffer)-1), (Byte_t*)&Buffer);

         /* Next, check the return value to see if the command was      */
         /* successfully.                                               */
         if(Result >= 0)
         {
            /* NULL terminate the read data.                            */
            Buffer[Result] = 0;

            /* Data was read successfully, the result indicates the     */
            /* of bytes that were successfully Read.                    */
            Display(("Read: %d.\r\n", Result));

            if(Result > 0)
               Display(("Message: %s\r\n",Buffer));

            ret_val = 0;
         }
         else
         {
            /* An error occurred while reading from SPP.                */
            DisplayFunctionError("ISPP_Data_Read Failure", Result);

            ret_val = Result;
         }
      } while(Result > 0);
   }
   else
   {
      /* One or more of the necessary parameters are invalid.           */
      ret_val = INVALID_PARAMETERS_ERROR;
   }

   return(ret_val);
}

   /* The following function is responsible for Writing Data to an Open */
   /* SPP Port.  The string that is written is defined by the constant  */
   /* TEST_DATA (at the top of this file).  This function requires that */
   /* a valid Bluetooth Stack ID and Serial Port ID exist before        */
   /* running.  This function returns zero is successful or a negative  */
   /* return value if there was an error.                               */
static int Write(ParameterList_t *TempParam)
{
   int  ret_val;
   int  Result;

   /* First check to see if the parameters required for the execution of*/
   /* this function appear to be semi-valid.                            */
   if((BluetoothStackID) && (SerialPortID) && (CurrentOperatingMode == pomSPP))
   {
      /* Simply write out the default string value.                     */
      Result = ISPP_Data_Write(BluetoothStackID, SerialPortID, (Word_t)BTPS_StringLength(TEST_DATA), (Byte_t *)TEST_DATA);

      /* Next, check the return value to see if the command was issued  */
      /* successfully.                                                  */
      if(Result >= 0)
      {
         /* The Data was written successfully, Result indicates the     */
         /* number of bytes successfully written.                       */
         Display(("Wrote: %d.\r\n", Result));

         /* Flag success to the caller.                                 */
         ret_val = 0;
      }
      else
      {
         /* There was an error writing the Data to the SPP Port.        */
         Display(("Failed: %d.\r\n", Result));

         /* Flag that an error occurred while submitting the command.   */
         ret_val = FUNCTION_ERROR;
      }
   }
   else
   {
      /* One or more of the necessary parameters are invalid.           */
      ret_val = INVALID_PARAMETERS_ERROR;
   }

   return(ret_val);
}

   /* The following function is responsible for sending a number of     */
   /* characters to a remote device to which a session exists.  The     */
   /* function receives a parameter that indicates the number of byte to*/
   /* be transferred.  This function will return zero on successful     */
   /* execution and a negative value on errors.                         */
static int SendSessionData(ParameterList_t *TempParam)
{
   Word_t    DataCount;
   Boolean_t Done;

   /* Make sure that all of the parameters required for this function   */
   /* appear to be at least semi-valid.                                 */
   if((TempParam) && (TempParam->NumberofParameters >= 1) && (TempParam->Params[0].intParam > 0))
   {
      /* Verify that there is an open session.                          */
      if((CurrentOperatingMode == pomiAP) && (iAPSessionID) && (SerialPortID) && (Connected))
      {
         /* Check to see if we are already sending.                     */
         if(!SendInfo.BytesToSend)
         {
            /* Get the count of the number of bytes to send.            */
            SendInfo.BytesToSend  = (Word_t)TempParam->Params[0].intParam;
            SendInfo.BytesSent    = 0;

            Done = FALSE;
            while((SendInfo.BytesToSend) && (!Done))
            {
               /* Set the Number of bytes to send in the first packet.  */
               DataCount = SendInfo.BytesToSend;
               if(DataCount > BTPS_StringLength(SessionData))
                  DataCount = BTPS_StringLength(SessionData);

               if(CurrentOperatingMode == pomiAP)
               {
                  /* Limit the amount of data to send.                  */
                  if(DataCount > MaxPacketData)
                     DataCount = MaxPacketData;

                  SendInfo.PacketID = ISPP_Send_Session_Data(BluetoothStackID, SerialPortID, iAPSessionID, DataCount, (unsigned char *)SessionData);

                  /* We must wait for an ACK before we can send more.   */
                  Done = TRUE;
               }

               if(SendInfo.PacketID > 0)
                  SendInfo.PendingCount = DataCount;
               else
               {
                  Display(("SEND failed with error %d\r\n", SendInfo.PacketID));
                  SendInfo.BytesToSend  = 0;
               }
            }
         }
         else
            Display(("Send Currently in progress.\r\n"));
      }
      else
         Display(("No Session Available\r\n"));
   }
   else
      DisplayUsage("SEND [Number of Bytes to send]\r\n");

   return(0);
}

   /* The following function is responsible for querying a Remote Device*/
   /* for the Port Number of the iAP Server.  This function returns     */
   /* zero on successful execution and a negative value on all errors.  */
static int SDPRequest(ParameterList_t *TempParam)
{
   int                           Result;
   int                           ret_val;
   SDP_UUID_Entry_t              UUID_Entry;
   SDP_Attribute_ID_List_Entry_t AttributeID;

   /* First, check that valid Bluetooth Stack ID exists.                */
   if(BluetoothStackID)
   {
      /* Make sure that all of the parameters required for this function*/
      /* appear to be at least semi-valid.                              */
      if((TempParam) && (TempParam->NumberofParameters > 0) && (TempParam->Params[0].intParam) && (NumberofValidResponses) && (TempParam->Params[0].intParam <= NumberofValidResponses) && (!COMPARE_NULL_BD_ADDR(InquiryResultList[(TempParam->Params[0].intParam - 1)])))
      {
         AttributeID.Attribute_Range      = FALSE;
         AttributeID.Start_Attribute_ID   = SDP_ATTRIBUTE_ID_PROTOCOL_DESCRIPTOR_LIST;

         UUID_Entry.SDP_Data_Element_Type = deUUID_16;
         SDP_ASSIGN_SERIAL_PORT_PROFILE_UUID_16(UUID_Entry.UUID_Value.UUID_16);

         Result = SDP_Service_Search_Attribute_Request(BluetoothStackID, InquiryResultList[(TempParam->Params[0].intParam - 1)], 1, &UUID_Entry, 1, &AttributeID, SDP_Event_Callback, 0);

         /* Check the return value of the submitted command for success.*/
         if(Result > 0)
         {
            /* Display a messsage indicating that Remote Name request   */
            /* was initiated successfully.                              */
            Display(("Request ID %d.\r\n", Result));

            /* Flag success to the caller.                              */
            ret_val = 0;
         }
         else
         {
            /* Display a message indicating that an error occured while */
            /* initiating the Remote Name request.                      */
            DisplayFunctionError("Failed to Request Services", Result);

            ret_val = FUNCTION_ERROR;
         }
      }
      else
      {
         /* One or more of the necessary parameters is/are invalid.     */
         DisplayUsage("SDPRequest [Inquiry Index]");

         ret_val = INVALID_PARAMETERS_ERROR;
      }
   }
   else
   {
      /* No valid Bluetooth Stack ID exists.                            */
      ret_val = INVALID_STACK_ID_ERROR;
   }

   return(ret_val);
}

   /* The following function is responsible for querying the current    */
   /* configuration parameters that are used by SPP.  This function will*/
   /* return zero on successful execution and a negative value on       */
   /* errors.                                                           */
static int GetConfigParams(ParameterList_t *TempParam)
{
   int                        ret_val;
   ISPP_Configuration_Params_t ISPPConfigurationParams;

   /* First check to see if the parameters required for the execution of*/
   /* this function appear to be semi-valid.                            */
   if(BluetoothStackID)
   {
      /* Simply query the configuration parameters.                     */
      ret_val = ISPP_Get_Configuration_Parameters(BluetoothStackID, &ISPPConfigurationParams);

      if(ret_val >= 0)
      {
         /* Parameters have been queried successfully, go ahead and     */
         /* notify the user.                                            */
         Display(("ISPP_Get_Configuration_Parameters(): Success\r\n", ret_val));
         Display(("   MaximumFrameSize   : %d (0x%X)\r\n", ISPPConfigurationParams.MaximumFrameSize, ISPPConfigurationParams.MaximumFrameSize));
         Display(("   TransmitBufferSize : %d (0x%X)\r\n", ISPPConfigurationParams.TransmitBufferSize, ISPPConfigurationParams.TransmitBufferSize));
         Display(("   ReceiveBufferSize  : %d (0x%X)\r\n", ISPPConfigurationParams.ReceiveBufferSize, ISPPConfigurationParams.ReceiveBufferSize));

         /* Flag success.                                               */
         ret_val = 0;
      }
      else
      {
         /* Error querying the current parameters.                      */
         Display(("ISPP_Get_Configuration_Parameters(): Error %d.\r\n", ret_val));

         ret_val = FUNCTION_ERROR;
      }
   }
   else
   {
      /* One or more of the necessary parameters are invalid.           */
      ret_val = INVALID_PARAMETERS_ERROR;
   }

   return(ret_val);
}

   /* The following function is responsible for setting the current     */
   /* configuration parameters that are used by SPP.  This function will*/
   /* return zero on successful execution and a negative value on       */
   /* errors.                                                           */
static int SetConfigParams(ParameterList_t *TempParam)
{
   int                        ret_val;
   ISPP_Configuration_Params_t ISPPConfigurationParams;

   /* First check to see if the parameters required for the execution of*/
   /* this function appear to be semi-valid.                            */
   if(BluetoothStackID)
   {
      /* Next check to see if the parameters required for the execution */
      /* of this function appear to be semi-valid.                      */
      if((TempParam) && (TempParam->NumberofParameters > 2))
      {
         /* Parameters have been specified, go ahead and write them to  */
         /* the stack.                                                  */
         ISPPConfigurationParams.MaximumFrameSize   = (unsigned int)(TempParam->Params[0].intParam);
         ISPPConfigurationParams.TransmitBufferSize = (unsigned int)(TempParam->Params[1].intParam);
         ISPPConfigurationParams.ReceiveBufferSize  = (unsigned int)(TempParam->Params[2].intParam);

         ret_val = ISPP_Set_Configuration_Parameters(BluetoothStackID, &ISPPConfigurationParams);

         if(ret_val >= 0)
         {
            Display(("ISPP_Set_Configuration_Parameters(): Success\r\n", ret_val));
            Display(("   MaximumFrameSize   : %d (0x%X)\r\n", ISPPConfigurationParams.MaximumFrameSize, ISPPConfigurationParams.MaximumFrameSize));
            Display(("   TransmitBufferSize : %d (0x%X)\r\n", ISPPConfigurationParams.TransmitBufferSize, ISPPConfigurationParams.TransmitBufferSize));
            Display(("   ReceiveBufferSize  : %d (0x%X)\r\n", ISPPConfigurationParams.ReceiveBufferSize, ISPPConfigurationParams.ReceiveBufferSize));

            /* Flag success.                                            */
            ret_val = 0;
         }
         else
         {
            /* Error setting the current parameters.                    */
            Display(("ISPP_Set_Configuration_Parameters(): Error %d.\r\n", ret_val));

            ret_val = FUNCTION_ERROR;
         }
      }
      else
      {
         DisplayUsage("SetConfigParams [MaximumFrameSize] [TransmitBufferSize (0: don't change)] [ReceiveBufferSize (0: don't change)]");

         ret_val = INVALID_PARAMETERS_ERROR;
      }
   }
   else
   {
      /* One or more of the necessary parameters are invalid.           */
      ret_val = INVALID_PARAMETERS_ERROR;
   }

   return(ret_val);
}

   /* The following function is responsible for querying the current    */
   /* queuing parameters that are used by SPP/RFCOMM (into L2CAP).  This*/
   /* function will return zero on successful execution and a negative  */
   /* value on errors.                                                  */
static int GetQueueParams(ParameterList_t *TempParam)
{
   int          ret_val;
   unsigned int MaximumNumberDataPackets;
   unsigned int QueuedDataPacketsThreshold;

   /* First check to see if the parameters required for the execution of*/
   /* this function appear to be semi-valid.                            */
   if(BluetoothStackID)
   {
      /* Simply query the queuing parameters.                           */
      ret_val = ISPP_Get_Queuing_Parameters(BluetoothStackID, &MaximumNumberDataPackets, &QueuedDataPacketsThreshold);

      if(ret_val >= 0)
      {
         /* Parameters have been queried successfully, go ahead and     */
         /* notify the user.                                            */
         Display(("ISPP_Get_Queuing_Parameters(): Success.\r\n"));
         Display(("   MaximumNumberDataPackets   : %d (0x%X)\r\n", MaximumNumberDataPackets, MaximumNumberDataPackets));
         Display(("   QueuedDataPacketsThreshold : %d (0x%X)\r\n", QueuedDataPacketsThreshold, QueuedDataPacketsThreshold));

         /* Flag success.                                               */
         ret_val = 0;
      }
      else
      {
         /* Error querying the current parameters.                      */
         Display(("ISPP_Get_Queuing_Parameters(): Error %d.\r\n", ret_val));

         ret_val = FUNCTION_ERROR;
      }
   }
   else
   {
      /* One or more of the necessary parameters are invalid.           */
      ret_val = INVALID_PARAMETERS_ERROR;
   }

   return(ret_val);
}

   /* The following function is responsible for setting the current     */
   /* queuing parameters that are used by SPP/RFCOMM (into L2CAP).  This*/
   /* function will return zero on successful execution and a negative  */
   /* value on errors.                                                  */
static int SetQueueParams(ParameterList_t *TempParam)
{
   int ret_val;

   /* First check to see if the parameters required for the execution of*/
   /* this function appear to be semi-valid.                            */
   if(BluetoothStackID)
   {
      /* Next check to see if the parameters required for the execution */
      /* of this function appear to be semi-valid.                      */
      if((TempParam) && (TempParam->NumberofParameters > 1))
      {
         /* Parameters have been specified, go ahead and write them to  */
         /* the stack.                                                  */
         ret_val = ISPP_Set_Queuing_Parameters(BluetoothStackID, (unsigned int)(TempParam->Params[0].intParam), (unsigned int)(TempParam->Params[1].intParam));

         if(ret_val >= 0)
         {
            Display(("ISPP_Set_Queuing_Parameters(): Success.\r\n"));
            Display(("   MaximumNumberDataPackets   : %d (0x%X)\r\n", (unsigned int)(TempParam->Params[0].intParam), (unsigned int)(TempParam->Params[0].intParam)));
            Display(("   QueuedDataPacketsThreshold : %d (0x%X)\r\n", (unsigned int)(TempParam->Params[1].intParam), (unsigned int)(TempParam->Params[1].intParam)));

            /* Flag success.                                            */
            ret_val = 0;
         }
         else
         {
            /* Error setting the current parameters.                    */
            Display(("ISPP_Set_Queuing_Parameters(): Error %d.\r\n", ret_val));

            ret_val = FUNCTION_ERROR;
         }
      }
      else
      {
         DisplayUsage("SetQueueParams [MaximumNumberDataPackets] [QueuedDataPacketsThreshold]");

         ret_val = INVALID_PARAMETERS_ERROR;
      }
   }
   else
   {
      /* One or more of the necessary parameters are invalid.           */
      ret_val = INVALID_PARAMETERS_ERROR;
   }

   return(ret_val);
}

   /* The following function is responsible for setting the application */
   /* state to support loopback mode.  This function will return zero on*/
   /* successful execution and a negative value on errors.              */
static int Loopback(ParameterList_t *TempParam)
{
   int ret_val;

   /* First check to see if the parameters required for the execution of*/
   /* this function appear to be semi-valid.                            */
   if(BluetoothStackID)
   {
      /* Next check to see if the parameters required for the execution */
      /* of this function appear to be semi-valid.                      */
      if((TempParam) && (TempParam->NumberofParameters > 0))
      {
         if(TempParam->Params[0].intParam)
            LoopbackActive = TRUE;
         else
            LoopbackActive = FALSE;
      }
      else
         LoopbackActive = (LoopbackActive?FALSE:TRUE);

      /* Finally output the current Loopback state.                     */
      Display(("Current Loopback Mode set to: %s.\r\n", LoopbackActive?"ACTIVE":"INACTIVE"));

      /* Flag success.                                                  */
      ret_val = 0;
   }
   else
   {
      /* One or more of the necessary parameters are invalid.           */
      ret_val = INVALID_PARAMETERS_ERROR;
   }

   return(ret_val);
}

   /* The following function is responsible for setting the application */
   /* state to support displaying Raw Data.  This function will return  */
   /* zero on successful execution and a negative value on errors.      */
static int DisplayRawModeData(ParameterList_t *TempParam)
{
   int ret_val;

   /* First check to see if the parameters required for the execution of*/
   /* this function appear to be semi-valid.                            */
   if(BluetoothStackID)
   {
      /* Check to see if Loopback is active.  If it is then we will not */
      /* process this command (and we will inform the user).            */
      if(!LoopbackActive)
      {
         /* Next check to see if the parameters required for the        */
         /* execution of this function appear to be semi-valid.         */
         if((TempParam) && (TempParam->NumberofParameters > 0))
         {
            if(TempParam->Params->intParam)
               DisplayRawData = TRUE;
            else
               DisplayRawData = FALSE;
         }
         else
            DisplayRawData = (DisplayRawData?FALSE:TRUE);

         /* Output the current Raw Data Display Mode state.             */
         Display(("Current Raw Data Display Mode set to: %s.\r\n", DisplayRawData?"ACTIVE":"INACTIVE"));

         /* Flag that the function was successful.                      */
         ret_val = 0;
      }
      else
      {
         Display(("Unable to process Raw Mode Display Request when operating in Loopback Mode.\r\n"));

         ret_val = INVALID_PARAMETERS_ERROR;
      }
   }
   else
   {
      /* One or more of the necessary parameters are invalid.           */
      ret_val = INVALID_PARAMETERS_ERROR;
   }

   return(ret_val);
}

   /* The following function is responsible for setting the application */
   /* state to support Automatically reading all data that is received  */
   /* through SPP.  This function will return zero on successful        */
   /* execution and a negative value on errors.                         */
static int AutomaticReadMode(ParameterList_t *TempParam)
{
   int ret_val;

   /* First check to see if the parameters required for the execution of*/
   /* this function appear to be semi-valid.                            */
   if(BluetoothStackID)
   {
      /* Check to see if Loopback is active.  If it is then we will not */
      /* process this command (and we will inform the user).            */
      if(!LoopbackActive)
      {
         /* Next check to see if the parameters required for the        */
         /* execution of this function appear to be semi-valid.         */
         if((TempParam) && (TempParam->NumberofParameters > 0))
         {
            if(TempParam->Params->intParam)
               AutomaticReadActive = TRUE;
            else
               AutomaticReadActive = FALSE;
         }
         else
            AutomaticReadActive = (AutomaticReadActive?FALSE:TRUE);

         /* Output the current Automatic Read Mode state.               */
         Display(("Current Automatic Read Mode set to: %s.\r\n", AutomaticReadActive?"ACTIVE":"INACTIVE"));

         /* Flag that the function was successful.                      */
         ret_val = 0;
      }
      else
      {
         Display(("Unable to process Automatic Read Mode Request when operating in Loopback Mode.\r\n"));

         ret_val = INVALID_PARAMETERS_ERROR;
      }
   }
   else
   {
      /* One or more of the necessary parameters are invalid.           */
      ret_val = INVALID_PARAMETERS_ERROR;
   }

   return(ret_val);
}

   /* The following thread is responsible for checking changing the     */
   /* current Baud Rate used to talk to the Radio.                      */
   /* * NOTE * This function ONLY configures the Baud Rate for a TI     */
   /*          Bluetooth chipset.                                       */
static int SetBaudRate(ParameterList_t *TempParam)
{
   int ret_val;

   /* First check to see if the parameters required for the execution of*/
   /* this function appear to be semi-valid.                            */
   if(BluetoothStackID)
   {
      /* Next check to see if the parameters required for the execution */
      /* of this function appear to be semi-valid.                      */
      if((TempParam) && (TempParam->NumberofParameters > 0))
      {
         /* Verify that this is a valid baud rate.                      */
         if(TempParam->Params[0].intParam)
         {
            if(HCI_VS_SetBaudRate(BluetoothStackID, (TempParam->Params[0].intParam)))
            {
               Display(("HCI_VS_SetBaudRate Success.\r\n"));
               ret_val = 0;
            }
            else
            {
               Display(("HCI_VS_SetBaudRate Failure.\r\n"));
               ret_val = FUNCTION_ERROR;
            }
         }
         else
         {
            DisplayUsage("SetBaudRate [BaudRate]");

            ret_val = INVALID_PARAMETERS_ERROR;
         }
      }
      else
      {
         DisplayUsage("SetBaudRate [BaudRate]");

         ret_val = INVALID_PARAMETERS_ERROR;
      }
   }
   else
   {
      /* One or more of the necessary parameters are invalid.           */
      ret_val = INVALID_PARAMETERS_ERROR;
   }

   return(ret_val);
}

   /* The following function is responsible for changing the User       */
   /* Interface Mode to Server.                                         */
static int ServerMode(ParameterList_t *TempParam)
{
   UI_Mode = UI_MODE_IS_SERVER;

   return(EXIT_MODE);
}

   /* The following function is responsible for changing the User       */
   /* Interface Mode to Client.                                         */
static int ClientMode(ParameterList_t *TempParam)
{
   UI_Mode = UI_MODE_IS_CLIENT;

   return(EXIT_MODE);
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

   if((SerialPortID) && (Connected) && (iAPTimerID))
   {
      ISPP_Get_Port_Operating_Mode(BluetoothStackID, SerialPortID, &OperatingMode);
      if(OperatingMode == pomiAP)
      {
         Display(("iAP Timeout for SerialPortID %d\r\n", SerialPortID));

         BD_ADDRToStr(CurrentRemoteBD_ADDR, (char *)Buffer);
         Display(("iSPP Port %d BD_ADDR %s\r\n", (int)SerialPortID, Buffer));

         ret_val = ISPP_Start_Authorization(BluetoothStackID, (unsigned int)SerialPortID);
         Display(("\r\nISPP_Start_Authorization(%d) returned %d\r\n", (int)SerialPortID, ret_val));

         DisplayPrompt();
      }

      iAPTimerID = 0;
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
   BD_ADDR_t                         NULL_BD_ADDR;
   Boolean_t                         OOB_Data;
   Boolean_t                         MITM;
   GAP_IO_Capability_t               RemoteIOCapability;
   GAP_Inquiry_Event_Data_t         *GAP_Inquiry_Event_Data;
   GAP_Remote_Name_Event_Data_t     *GAP_Remote_Name_Event_Data;
   GAP_Authentication_Information_t  GAP_Authentication_Information;

   /* First, check to see if the required parameters appear to be       */
   /* semi-valid.                                                       */
   if((BluetoothStackID) && (GAP_Event_Data))
   {
      /* The parameters appear to be semi-valid, now check to see what  */
      /* type the incoming event is.                                    */
      switch(GAP_Event_Data->Event_Data_Type)
      {
         case etInquiry_Result:
            /* The GAP event received was of type Inquiry_Result.       */
            GAP_Inquiry_Event_Data = GAP_Event_Data->Event_Data.GAP_Inquiry_Event_Data;

            /* Next, Check to see if the inquiry event data received    */
            /* appears to be semi-valid.                                */
            if(GAP_Inquiry_Event_Data)
            {
               /* Now, check to see if the gap inquiry event data's     */
               /* inquiry data appears to be semi-valid.                */
               if(GAP_Inquiry_Event_Data->GAP_Inquiry_Data)
               {
                  Display(("\r\n"));

                  /* Display a list of all the devices found from       */
                  /* performing the inquiry.                            */
                  for(Index=0;(Index<GAP_Inquiry_Event_Data->Number_Devices) && (Index<MAX_INQUIRY_RESULTS);Index++)
                  {
                     InquiryResultList[Index] = GAP_Inquiry_Event_Data->GAP_Inquiry_Data[Index].BD_ADDR;
                     BD_ADDRToStr(GAP_Inquiry_Event_Data->GAP_Inquiry_Data[Index].BD_ADDR, Callback_BoardStr);

                     Display(("Result: %d, %s.\r\n", (Index+1), Callback_BoardStr));
                  }

                  NumberofValidResponses = GAP_Inquiry_Event_Data->Number_Devices;
               }
            }
            break;
         case etInquiry_Entry_Result:
            /* Next convert the BD_ADDR to a string.                    */
            BD_ADDRToStr(GAP_Event_Data->Event_Data.GAP_Inquiry_Entry_Event_Data->BD_ADDR, Callback_BoardStr);

            /* Display this GAP Inquiry Entry Result.                   */
            Display(("\r\n"));
            Display(("Inquiry Entry: %s.\r\n", Callback_BoardStr));
            break;
         case etAuthentication:
            /* An authentication event occurred, determine which type of*/
            /* authentication event occurred.                           */
            switch(GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->GAP_Authentication_Event_Type)
            {
               case atLinkKeyRequest:
                  BD_ADDRToStr(GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Remote_Device, Callback_BoardStr);
                  Display(("\r\n"));
                  Display(("atLinkKeyRequest: %s\r\n", Callback_BoardStr));

                  /* Setup the authentication information response      */
                  /* structure.                                         */
                  GAP_Authentication_Information.GAP_Authentication_Type    = atLinkKey;
                  GAP_Authentication_Information.Authentication_Data_Length = 0;

                  /* See if we have stored a Link Key for the specified */
                  /* device.                                            */
                  for(Index=0;Index<(sizeof(LinkKeyInfo)/sizeof(LinkKeyInfo_t));Index++)
                  {
                     if(COMPARE_BD_ADDR(LinkKeyInfo[Index].BD_ADDR, GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Remote_Device))
                     {
                        /* Link Key information stored, go ahead and    */
                        /* respond with the stored Link Key.            */
                        GAP_Authentication_Information.Authentication_Data_Length   = sizeof(Link_Key_t);
                        GAP_Authentication_Information.Authentication_Data.Link_Key = LinkKeyInfo[Index].LinkKey;

                        break;
                     }
                  }

                  /* Submit the authentication response.                */
                  Result = GAP_Authentication_Response(BluetoothStackID, GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Remote_Device, &GAP_Authentication_Information);

                  /* Check the result of the submitted command.         */
                  if(!Result)
                     DisplayFunctionSuccess("GAP_Authentication_Response");
                  else
                     DisplayFunctionError("GAP_Authentication_Response", Result);
                  break;
               case atPINCodeRequest:
                  /* A pin code request event occurred, first display   */
                  /* the BD_ADD of the remote device requesting the pin.*/
                  BD_ADDRToStr(GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Remote_Device, Callback_BoardStr);
                  Display(("\r\n"));
                  Display(("atPINCodeRequest: %s\r\n", Callback_BoardStr));

                  /* Note the current Remote BD_ADDR that is requesting */
                  /* the PIN Code.                                      */
                  CurrentRemoteBD_ADDR = GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Remote_Device;

                  /* Inform the user that they will need to respond with*/
                  /* a PIN Code Response.                               */
                  Display(("Respond with: PINCodeResponse\r\n"));
                  break;
               case atAuthenticationStatus:
                  /* An authentication status event occurred, display   */
                  /* all relevant information.                          */
                  BD_ADDRToStr(GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Remote_Device, Callback_BoardStr);
                  Display(("\r\n"));
                  Display(("atAuthenticationStatus: %d for %s\r\n", GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Authentication_Event_Data.Authentication_Status, Callback_BoardStr));

                  /* Flag that there is no longer a current             */
                  /* Authentication procedure in progress.              */
                  ASSIGN_BD_ADDR(CurrentRemoteBD_ADDR, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);

                  /* Check to see if this was performed due to a        */
                  /* connection request.                                */
                  if(CallbackParameter == SerialPortID)
                  {
                     /* Check to see if the authentication process was  */
                     /* successful.                                     */
                     if(!GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Authentication_Event_Data.Authentication_Status)
                     {
                        Display(("Request Encryption"));
                        Result = GAP_Set_Encryption_Mode(BluetoothStackID, GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Remote_Device, emEnabled, GAP_Event_Callback, CallbackParameter);
                        if(Result)
                        {
                           Display(("Reject Connection %d", Result));

                           /* We failed the Encryption Request so do */
                           /* not allow the connection.              */
                           ISPP_Open_Port_Request_Response(BluetoothStackID, (unsigned int)CallbackParameter, FALSE, 0, NULL, 0);

                           Connecting = FALSE;
                        }
                     }
                     else
                     {
                        Display(("Reject Connection"));

                        /* We failed authenticate so do not allow the   */
                        /* connection.                                  */
                        ISPP_Open_Port_Request_Response(BluetoothStackID, (unsigned int)CallbackParameter, FALSE, 0, NULL, 0);

                        Connecting = FALSE;
                     }
                  }
                  break;
               case atLinkKeyCreation:
                  /* A link key creation event occurred, first display  */
                  /* the remote device that caused this event.          */
                  BD_ADDRToStr(GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Remote_Device, Callback_BoardStr);
                  Display(("\r\n"));
                  Display(("atLinkKeyCreation: %s\r\n", Callback_BoardStr));

                  /* Now store the link Key in either a free location OR*/
                  /* over the old key location.                         */
                  ASSIGN_BD_ADDR(NULL_BD_ADDR, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);

                  for(Index=0,Result=-1;Index<(sizeof(LinkKeyInfo)/sizeof(LinkKeyInfo_t));Index++)
                  {
                     if(COMPARE_BD_ADDR(LinkKeyInfo[Index].BD_ADDR, GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Remote_Device))
                        break;
                     else
                     {
                        if((Result == (-1)) && (COMPARE_BD_ADDR(LinkKeyInfo[Index].BD_ADDR, NULL_BD_ADDR)))
                           Result = Index;
                     }
                  }

                  /* If we didn't find a match, see if we found an empty*/
                  /* location.                                          */
                  if(Index == (sizeof(LinkKeyInfo)/sizeof(LinkKeyInfo_t)))
                     Index = Result;

                  /* Check to see if we found a location to store the   */
                  /* Link Key information into.                         */
                  if(Index != (-1))
                  {
                     LinkKeyInfo[Index].BD_ADDR = GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Remote_Device;
                     LinkKeyInfo[Index].LinkKey = GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Authentication_Event_Data.Link_Key_Info.Link_Key;

                     Display(("Link Key Stored.\r\n"));
                  }
                  else
                     Display(("Link Key array full.\r\n"));
                  break;
               case atIOCapabilityRequest:
                  BD_ADDRToStr(GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Remote_Device, Callback_BoardStr);
                  Display(("\r\n"));
                  Display(("atIOCapabilityRequest: %s\r\n", Callback_BoardStr));

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
                     DisplayFunctionSuccess("Auth");
                  else
                     DisplayFunctionError("Auth", Result);
                  break;
               case atIOCapabilityResponse:
                  BD_ADDRToStr(GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Remote_Device, Callback_BoardStr);
                  Display(("\r\n"));
                  Display(("atIOCapabilityResponse: %s\r\n", Callback_BoardStr));

                  RemoteIOCapability = GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Authentication_Event_Data.IO_Capabilities.IO_Capability;
                  MITM               = (Boolean_t)GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Authentication_Event_Data.IO_Capabilities.MITM_Protection_Required;
                  OOB_Data           = (Boolean_t)GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Authentication_Event_Data.IO_Capabilities.OOB_Data_Present;

                  Display(("Capabilities: %s%s%s\r\n", IOCapabilitiesStrings[RemoteIOCapability], ((MITM)?", MITM":""), ((OOB_Data)?", OOB Data":"")));
                  break;
               case atUserConfirmationRequest:
                  BD_ADDRToStr(GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Remote_Device, Callback_BoardStr);
                  Display(("\r\n"));
                  Display(("atUserConfirmationRequest: %s\r\n", Callback_BoardStr));

                  CurrentRemoteBD_ADDR = GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Remote_Device;

                  if(IOCapability != icDisplayYesNo)
                  {
                     /* Invoke JUST Works Process...                    */
                     GAP_Authentication_Information.GAP_Authentication_Type          = atUserConfirmation;
                     GAP_Authentication_Information.Authentication_Data_Length       = (Byte_t)sizeof(Byte_t);
                     GAP_Authentication_Information.Authentication_Data.Confirmation = TRUE;

                     /* Submit the Authentication Response.             */
                     Display(("\r\nAuto Accepting: %d\r\n", GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Authentication_Event_Data.Numeric_Value));

                     Result = GAP_Authentication_Response(BluetoothStackID, GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Remote_Device, &GAP_Authentication_Information);

                     if(!Result)
                        DisplayFunctionSuccess("GAP_Authentication_Response");
                     else
                        DisplayFunctionError("GAP_Authentication_Response", Result);

                     /* Flag that there is no longer a current          */
                     /* Authentication procedure in progress.           */
                     ASSIGN_BD_ADDR(CurrentRemoteBD_ADDR, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
                  }
                  else
                  {
                     Display(("User Confirmation: %d\r\n", (unsigned long)GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Authentication_Event_Data.Numeric_Value));

                     /* Inform the user that they will need to respond  */
                     /* with a PIN Code Response.                       */
                     Display(("Respond with: UserConfirmationResponse\r\n"));
                  }
                  break;
               case atPasskeyRequest:
                  BD_ADDRToStr(GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Remote_Device, Callback_BoardStr);
                  Display(("\r\n"));
                  Display(("atPasskeyRequest: %s\r\n", Callback_BoardStr));

                  /* Note the current Remote BD_ADDR that is requesting */
                  /* the Passkey.                                       */
                  CurrentRemoteBD_ADDR = GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Remote_Device;

                  /* Inform the user that they will need to respond with*/
                  /* a Passkey Response.                                */
                  Display(("Respond with: PassKeyResponse\r\n"));
                  break;
               case atRemoteOutOfBandDataRequest:
                  BD_ADDRToStr(GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Remote_Device, Callback_BoardStr);
                  Display(("\r\n"));
                  Display(("atRemoteOutOfBandDataRequest: %s\r\n", Callback_BoardStr));

                  /* This application does not support OOB data so      */
                  /* respond with a data length of Zero to force a      */
                  /* negative reply.                                    */
                  GAP_Authentication_Information.GAP_Authentication_Type    = atOutOfBandData;
                  GAP_Authentication_Information.Authentication_Data_Length = 0;

                  Result = GAP_Authentication_Response(BluetoothStackID, GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Remote_Device, &GAP_Authentication_Information);

                  if(!Result)
                     DisplayFunctionSuccess("GAP_Authentication_Response");
                  else
                     DisplayFunctionError("GAP_Authentication_Response", Result);
                  break;
               case atPasskeyNotification:
                  BD_ADDRToStr(GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Remote_Device, Callback_BoardStr);
                  Display(("\r\n"));
                  Display(("atPasskeyNotification: %s\r\n", Callback_BoardStr));

                  Display(("Passkey Value: %d\r\n", GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Authentication_Event_Data.Numeric_Value));
                  break;
               case atKeypressNotification:
                  BD_ADDRToStr(GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Remote_Device, Callback_BoardStr);
                  Display(("\r\n"));
                  Display(("atKeypressNotification: %s\r\n", Callback_BoardStr));

                  Display(("Keypress: %d\r\n", (int)GAP_Event_Data->Event_Data.GAP_Authentication_Event_Data->Authentication_Event_Data.Keypress_Type));
                  break;
               default:
                  Display(("Un-handled Auth. Event.\r\n"));
                  break;
            }
            break;
         case etRemote_Name_Result:
            /* Bluetooth Stack has responded to a previously issued     */
            /* Remote Name Request that was issued.                     */
            GAP_Remote_Name_Event_Data = GAP_Event_Data->Event_Data.GAP_Remote_Name_Event_Data;
            if(GAP_Remote_Name_Event_Data)
            {
               /* Inform the user of the Result.                        */
               BD_ADDRToStr(GAP_Remote_Name_Event_Data->Remote_Device, Callback_BoardStr);

               Display(("\r\n"));
               Display(("BD_ADDR: %s.\r\n", Callback_BoardStr));

               if(GAP_Remote_Name_Event_Data->Remote_Name)
                  Display(("Name: %s.\r\n", GAP_Remote_Name_Event_Data->Remote_Name));
               else
                  Display(("Name: NULL.\r\n"));
            }
            break;
         case etEncryption_Change_Result:
            BD_ADDRToStr(GAP_Event_Data->Event_Data.GAP_Encryption_Mode_Event_Data->Remote_Device,Callback_BoardStr);
            Display(("\r\netEncryption_Change_Result for %s, Status: 0x%02X, Mode: %s.\r\n", Callback_BoardStr,
                                                                                             GAP_Event_Data->Event_Data.GAP_Encryption_Mode_Event_Data->Encryption_Change_Status,
                                                                                             ((GAP_Event_Data->Event_Data.GAP_Encryption_Mode_Event_Data->Encryption_Mode == emDisabled)?"Disabled": "Enabled")));

            /* Check to see if the encryption was changed do to a       */
            /* connect request.                                         */
            if(CallbackParameter)
            {
               if(CurrentOperatingMode == pomiAP)
               {
                  Display(("Accept iOS.\r\n"));

                  ISPP_Open_Port_Request_Response(BluetoothStackID, (unsigned int)CallbackParameter, TRUE, sizeof(FIDInfo), (unsigned char *)FIDInfo, MAX_SMALL_PACKET_PAYLOAD_SIZE);
               }
               else
               {
                  Display(("Accept Non-iOS.\r\n"));

                  ISPP_Open_Port_Request_Response(BluetoothStackID, (unsigned int)CallbackParameter, TRUE, 0, NULL, 0);
               }
            }
            break;
         default:
            /* An unknown/unexpected GAP event was received.            */
            Display(("\r\nUnknown Event: %d.\r\n", GAP_Event_Data->Event_Data_Type));
            break;
      }
   }
   else
   {
      /* There was an error with one or more of the input parameters.   */
      Display(("\r\n"));

      Display(("NULL Event\r\n"));
   }

   DisplayPrompt();
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

   /* First, check to see if the required parameters appear to be       */
   /* semi-valid.                                                       */
   if((SDP_Response_Data != NULL) && (BluetoothStackID))
   {
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
            Display(("\r\n"));
            Display(("SDP Timeout Received (Size = 0x%04X).\r\n", sizeof(SDP_Response_Data_t)));

            /* Error, if this was during a connection, go ahead and     */
            /* reject it.                                               */
            if(_SerialPortID == SerialPortID)
            {
               ISPP_Open_Port_Request_Response(BluetoothStackID, _SerialPortID, FALSE, 0, NULL, 0);

               Connecting = FALSE;
            }
            break;
         case rdConnectionError:
            /* A SDP Connection Error was received, display a message   */
            /* indicating this.                                         */
            Display(("\r\n"));
            Display(("SDP Connection Error Received (Size = 0x%04X).\r\n", sizeof(SDP_Response_Data_t)));

            /* Error, if this was during a connection, go ahead and     */
            /* reject it.                                               */
            if(_SerialPortID == SerialPortID)
            {
               ISPP_Open_Port_Request_Response(BluetoothStackID, _SerialPortID, FALSE, 0, NULL, 0);

               Connecting = FALSE;
            }
            break;
         case rdErrorResponse:
            /* A SDP error response was received, display all relevant  */
            /* information regarding this event.                        */
            Display(("\r\n"));
            Display(("SDP Error Response Received (Size = 0x%04X) - Error Code: %d.\r\n", sizeof(SDP_Response_Data_t), SDP_Response_Data->SDP_Response_Data.SDP_Error_Response_Data.Error_Code));

            /* Error, if this was during a connection, go ahead and     */
            /* reject it.                                               */
            if(_SerialPortID == SerialPortID)
            {
               ISPP_Open_Port_Request_Response(BluetoothStackID, _SerialPortID, FALSE, 0, NULL, 0);

               Connecting = FALSE;
            }
            break;
         case rdServiceSearchAttributeResponse:
            /* A SDP Service Search Attribute Response was received,    */
            /* display all relevant information regarding this event    */
            Display(("\r\n"));

            /* First, get a pointer to the Service Search Attribute     */
            /* Response Data.                                           */
            SSAR = &(SDP_Response_Data->SDP_Response_Data.SDP_Service_Search_Attribute_Response_Data);
            if(SSAR)
            {
               /* We should locate 1 record if this is na iOS device.   */
               Display(("rdServiceSearchAttributeResponse (%d) Records.\r\n", SSAR->Number_Service_Records));
               if(SSAR->Number_Service_Records)
               {
                  /* This is an iOS device, so make sure that we have   */
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
                              /* Save the Port number of the remote WiAP*/
                              /* Server.  This port number is Unique for*/
                              /* this iOS device.                       */
                              if(SDE->SDP_Data_Element_Type == deUnsignedInteger1Byte)
                                 Port = (int)SDE->SDP_Data_Element.UnsignedInteger1Byte;
                              else
                              {
                                 if(SDE->SDP_Data_Element_Type == deUnsignedInteger2Bytes)
                                    Port = (int)SDE->SDP_Data_Element.UnsignedInteger2Bytes;
                                 else
                                    Port = (int)SDE->SDP_Data_Element.UnsignedInteger4Bytes;
                              }

                              CurrentOperatingMode = pomiAP;

                              Display(("Port Number %d.\r\n", Port));
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
                        Display(("Accept iOS.\r\n"));

                        ISPP_Open_Port_Request_Response(BluetoothStackID, SerialPortID, TRUE, sizeof(FIDInfo), (unsigned char *)FIDInfo, MAX_SMALL_PACKET_PAYLOAD_SIZE);
                     }
                     else
                     {
                        Display(("Accept Non-iOS.\r\n"));

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
                     }
                  }
               }
               else
               {
                  /* We failed to request authentication so do not allow*/
                  /* the conneciton.                                    */
                  ISPP_Open_Port_Request_Response(BluetoothStackID, SerialPortID, FALSE, 0, NULL, 0);

                  Connecting = FALSE;
               }
            }
            break;
         default:
            /* An unknown/unexpected SDP event was received.            */
            Display(("\r\n"));
            Display(("Unknown SDP Event.\r\n"));

            /* If this was during a connection, go ahead and reject it. */
            if(_SerialPortID == SerialPortID)
            {
               ISPP_Open_Port_Request_Response(BluetoothStackID, _SerialPortID, FALSE, 0, NULL, 0);

               Connecting = FALSE;
            }
            break;
      }
   }
   else
   {
      /* There was an error with one or more of the input parameters.   */
      Display(("\r\n"));
      Display(("SDP callback data: Response_Data = NULL.\r\n"));
   }

   /* Make sure the output is displayed to the user.                    */
   DisplayPrompt();
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
   int                           PacketID;
   Byte_t                        Status;
   Word_t                        DataLength;
   Boolean_t                     _DisplayPrompt = TRUE;
   Boolean_t                     Done;
   unsigned short                SessionID;
   SDP_UUID_Entry_t              UUID_Entry;
   SDP_Attribute_ID_List_Entry_t AttributeID;

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

            Display(("\r\n"));
            Display(("ISPP Open Request Indication, ID: 0x%04X, Board: %s.\r\n", ISPP_Event_Data->Event_Data.ISPP_Open_Port_Request_Indication_Data->SerialPortID, Callback_BoardStr));

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
                  Display(("Failed to Request Services, Fail Connect Request.\r\n"));

                  ISPP_Open_Port_Request_Response(BluetoothStackID, SerialPortID, FALSE, 0, NULL, 0);

                  /* Flag that we are no longer in the connecting state. */
                  Connecting = FALSE;
               }
               else
               {
                  /* Service Discovery submitted successfully.          */
                  Display(("SDP_Service_Search_Attribute_Request(): Success.\n\n"));

                  ret_val = 0;
               }
            }
            else
               ISPP_Open_Port_Request_Response(BluetoothStackID, SerialPortID, FALSE, 0, NULL, 0);
            break;
         case etIPort_Open_Indication:
            /* A remote port is requesting a connection.                */
            BD_ADDRToStr(ISPP_Event_Data->Event_Data.ISPP_Open_Port_Indication_Data->BD_ADDR, Callback_BoardStr);

            Display(("\r\n"));
            Display(("ISPP Open Indication, ID: 0x%04X, Board: %s.\r\n", ISPP_Event_Data->Event_Data.ISPP_Open_Port_Indication_Data->SerialPortID, Callback_BoardStr));

            /* Save the Serial Port ID for later use.                   */
            SerialPortID = ISPP_Event_Data->Event_Data.ISPP_Open_Port_Indication_Data->SerialPortID;

            /* Start a timer to wait for Authentication to start.       */
            iAPTimerID = BSC_StartTimer(BluetoothStackID, IAP_START_DELAY_VALUE, BSC_Timer_Callback, 0);

            /* Flag that we are now connected.                          */
            Connected    = TRUE;

            /* Flag that there are no sessions active.                  */
            iAPSessionID     = 0;
            TotalSessionData = 0;

            /* Flag that we are sending no data.                        */
            BTPS_MemInitialize(&SendInfo, 0, sizeof(SendInfo));

            /* Query the connection handle.                             */
            ret_val = GAP_Query_Connection_Handle(BluetoothStackID,ISPP_Event_Data->Event_Data.ISPP_Open_Port_Indication_Data->BD_ADDR,&Connection_Handle);
            if(ret_val)
            {
               /* Failed to Query the Connection Handle.                */
               DisplayFunctionError("GAP_Query_Connection_Handle()",ret_val);

               ret_val           = 0;
               Connection_Handle = 0;
            }
            break;
         case etIPort_Open_Confirmation:
            /* A Client Port was opened.  The Status indicates the      */
            /* Status of the Open.                                      */
            Display(("\r\n"));
            Display(("ISPP Open Confirmation, ID: 0x%04X, Status 0x%04X.\r\n", ISPP_Event_Data->Event_Data.ISPP_Open_Port_Confirmation_Data->SerialPortID,
                                                                               ISPP_Event_Data->Event_Data.ISPP_Open_Port_Confirmation_Data->PortOpenStatus));

            /* Check the Status to make sure that an error did not      */
            /* occur.                                                   */
            if(ISPP_Event_Data->Event_Data.ISPP_Open_Port_Confirmation_Data->PortOpenStatus)
            {
               /* An error occurred while opening the Serial Port so    */
               /* invalidate the Serial Port ID.                        */
               SerialPortID      = 0;
               Connection_Handle = 0;
               iAPSessionID      = 0;
               TotalSessionData  = 0;

               /* Flag that we are no longer connected.                 */
               Connected         = FALSE;
               Connecting        = FALSE;
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
               TotalSessionData = 0;

               /* Query the connection Handle.                          */
               ret_val = GAP_Query_Connection_Handle(BluetoothStackID,SelectedBD_ADDR,&Connection_Handle);
               if(ret_val)
               {
                  /* Failed to Query the Connection Handle.                */
                  DisplayFunctionError("GAP_Query_Connection_Handle()",ret_val);

                  ret_val           = 0;
                  Connection_Handle = 0;
               }

               /* Flag that we are sending no data.                     */
               BTPS_MemInitialize(&SendInfo, 0, sizeof(SendInfo));
            }
            break;
         case etIPort_Close_Port_Indication:
            /* The Remote Port was Disconnected.                        */
            Display(("\r\n"));
            Display(("ISPP Close Port, ID: 0x%04X\r\n", ISPP_Event_Data->Event_Data.ISPP_Close_Port_Indication_Data->SerialPortID));

            if(iAPTimerID)
               BSC_StopTimer(BluetoothStackID, iAPTimerID);

            /* Invalidate the Serial Port ID.                           */
            SerialPortID      = 0;
            Connection_Handle = 0;
            iAPSessionID      = 0;
            iAPTimerID        = 0;
            TotalSessionData  = 0;

            /* Flag that we are no longer connected.                    */
            Connected  = FALSE;
            Connecting = FALSE;
            break;
         case etIPort_Status_Indication:
            /* Display Information about the new Port Status.           */
            Display(("\r\n"));
            Display(("ISPP Port Status Indication: 0x%04X, Status: 0x%04X, Break Status: 0x%04X, Length: 0x%04X.\r\n", ISPP_Event_Data->Event_Data.ISPP_Port_Status_Indication_Data->SerialPortID,
                                                                                                                       ISPP_Event_Data->Event_Data.ISPP_Port_Status_Indication_Data->PortStatus,
                                                                                                                       ISPP_Event_Data->Event_Data.ISPP_Port_Status_Indication_Data->BreakStatus,
                                                                                                                       ISPP_Event_Data->Event_Data.ISPP_Port_Status_Indication_Data->BreakTimeout));

            break;
         case etIPort_Data_Indication:
            /* Data was received.  Process it differently based upon the*/
            /* current state of the Loopback Mode.                      */
            if(LoopbackActive)
            {
               /* Initialize Done to false.                             */
               Done = FALSE;

               /* Loop until the write buffer is full or there is not   */
               /* more data to read.                                    */
               while((Done == FALSE) && (BufferFull == FALSE))
               {
                  /* The application state is currently in the loop back*/
                  /* state.  Read as much data as we can read.          */
                  if((TempLength = ISPP_Data_Read(BluetoothStackID, SerialPortID, (Word_t)sizeof(Buffer), (Byte_t *)Buffer)) > 0)
                  {
                     /* Adjust the Current Buffer Length by the number  */
                     /* of bytes which were successfully read.          */
                     BufferLength = TempLength;

                     /* Next attempt to write all of the data which is  */
                     /* currently in the buffer.                        */
                     if((TempLength = ISPP_Data_Write(BluetoothStackID, SerialPortID, (Word_t)BufferLength, (Byte_t *)Buffer)) < (int)BufferLength)
                     {
                        /* Not all of the data was successfully written */
                        /* or an error occurred, first check to see if  */
                        /* an error occurred.                           */
                        if(TempLength >= 0)
                        {
                           /* An error did not occur therefore the      */
                           /* Transmit Buffer must be full.  Adjust the */
                           /* Buffer and Buffer Length by the amount    */
                           /* which as successfully written.            */
                           if(TempLength)
                           {
                              for(Index=0,Index1=TempLength;Index1<BufferLength;Index++,Index1++)
                                 Buffer[Index] = Buffer[Index1];

                              BufferLength -= TempLength;
                           }

                           /* Set the flag indicating that the SPP Write*/
                           /* Buffer is full.                           */
                           BufferFull = TRUE;
                        }
                        else
                           Done = TRUE;
                     }
                  }
                  else
                     Done = TRUE;
               }

               _DisplayPrompt = FALSE;
            }
            else
            {
               /* If we are operating in Raw Data Display Mode then     */
               /* simply display the data that was give to use.         */
               if((DisplayRawData) || (AutomaticReadActive))
               {
                  /* Initialize Done to false.                          */
                  Done = FALSE;

                  /* Loop through and read all data that is present in  */
                  /* the buffer.                                        */
                  while(!Done)
                  {
                     /* Read as much data as possible.                  */
                     if((TempLength = ISPP_Data_Read(BluetoothStackID, SerialPortID, (Word_t)sizeof(Buffer)-1, (Byte_t *)Buffer)) > 0)
                     {
                        /* Now simply display each character that we    */
                        /* have just read.                              */
                        if(DisplayRawData)
                        {
                           Buffer[TempLength] = '\0';

                           Display(((char *)Buffer));
                        }
                     }
                     else
                     {
                        /* Either an error occurred or there is no more */
                        /* data to be read.                             */
                        if(TempLength < 0)
                        {
                           /* Error occurred.                           */
                           Display(("ISPP_Data_Read(): Error %d.\r\n", TempLength));
                        }

                        /* Regardless if an error occurred, we are      */
                        /* finished with the current loop.              */
                        Done = TRUE;
                     }
                  }

                  _DisplayPrompt = FALSE;
               }
               else
               {
                  /* Simply inform the user that data has arrived.      */
                  Display(("\r\n"));
                  Display(("ISPP Data Indication, ID: 0x%04X, Length: 0x%04X.\r\n", ISPP_Event_Data->Event_Data.ISPP_Data_Indication_Data->SerialPortID,
                                                                                    ISPP_Event_Data->Event_Data.ISPP_Data_Indication_Data->DataLength));
               }
            }
            break;
         case etIPort_Transmit_Buffer_Empty_Indication:
            if((SendInfo.BytesToSend) && (CurrentOperatingMode == pomiAP))
            {
               /* Send the remainder of the last attempt.               */
               Index                 = (BTPS_StringLength(SessionData)-SendInfo.PendingCount);
               SendInfo.PacketID     = ISPP_Data_Write(BluetoothStackID, SerialPortID, Index, (unsigned char *)&(SessionData[SendInfo.PendingCount]));
               SendInfo.BytesToSend -= SendInfo.PacketID;
               while(SendInfo.BytesToSend)
               {
                  /* Set the Number of bytes to send in the first       */
                  /* packet.                                            */
                  Index = SendInfo.BytesToSend;
                  if(Index > BTPS_StringLength(SessionData))
                     Index = BTPS_StringLength(SessionData);

                  SendInfo.BytesSent = ISPP_Data_Write(BluetoothStackID, SerialPortID, Index, (unsigned char *)SessionData);
                  if(((int)SendInfo.BytesSent) >= 0)
                  {
                     SendInfo.BytesToSend -= SendInfo.BytesSent;
                     if(SendInfo.BytesSent < Index)
                     {
                        /* Record the amount that was last sent.        */
                        SendInfo.PendingCount = SendInfo.BytesSent;
                        break;
                     }
                  }
                  else
                     SendInfo.BytesToSend = 0;
               }
            }
            break;
         case etIPort_Send_Port_Information_Indication:
            /* Simply Respond with the Information that was sent to us. */
            ret_val = ISPP_Respond_Port_Information(BluetoothStackID, ISPP_Event_Data->Event_Data.ISPP_Send_Port_Information_Indication_Data->SerialPortID, &ISPP_Event_Data->Event_Data.ISPP_Send_Port_Information_Indication_Data->SPPPortInformation);
            break;

            /* All the events below this are special iOS events ONLY.   */

         case etIPort_Process_Status:
            Display(("\r\n"));

            Status = ISPP_Event_Data->Event_Data.ISPP_Process_Status->Status;

            switch(ISPP_Event_Data->Event_Data.ISPP_Process_Status->ProcessState)
            {
               case psStartIdentificationRequest:
                  Display(("Start Identification Request(%d) %s\r\n", SerialPortID, ProcessStatus[ISPP_Event_Data->Event_Data.ISPP_Process_Status->Status]));

                  if(iAPTimerID)
                  {
                     BSC_StopTimer(BluetoothStackID, iAPTimerID);

                     iAPTimerID = 0;
                  }

                  ret_val = ISPP_Cancel_Authorization(BluetoothStackID, ISPP_Event_Data->Event_Data.ISPP_Process_Status->SerialPortID);

                  if(ret_val < 0)
                     Display(("ISPP_Cancel_Authorization(%d) returned %d\r\n", SerialPortID, ret_val));

                  ret_val = ISPP_Start_Authorization(BluetoothStackID, SerialPortID);

                  Display(("ISPP_Start_Authorization(%d) returned %d\r\n", SerialPortID, ret_val));
                  break;
               case psStartIdentificationProcess:
                  Display(("Start Identification Process (%d) %s\r\n", SerialPortID, ProcessStatus[ISPP_Event_Data->Event_Data.ISPP_Process_Status->Status]));

                  if(iAPTimerID)
                  {
                     BSC_StopTimer(BluetoothStackID, iAPTimerID);

                     iAPTimerID = 0;
                  }
                  break;
               case psIdentificationProcess:
                  Display(("Identification Process(%d) %s\r\n", SerialPortID, ProcessStatus[ISPP_Event_Data->Event_Data.ISPP_Process_Status->Status]));
                  break;
               case psIdentificationProcessComplete:
                  Display(("Identification Process Complete (%d) %s\r\n", SerialPortID, ProcessStatus[ISPP_Event_Data->Event_Data.ISPP_Process_Status->Status]));
                  break;
               case psStartAuthenticationProcess:
                  Display(("Start Authentication Process (%d) %s\r\n", SerialPortID, ProcessStatus[ISPP_Event_Data->Event_Data.ISPP_Process_Status->Status]));
                  break;
               case psAuthenticationProcess:
                  Display(("Authentication Process (%d) %s", SerialPortID, ProcessStatus[ISPP_Event_Data->Event_Data.ISPP_Process_Status->Status]));
                  break;
               case psAuthenticationProcessComplete:
                  Display(("Authentication Process Complete (%d) %s\r\n", SerialPortID, ProcessStatus[ISPP_Event_Data->Event_Data.ISPP_Process_Status->Status]));

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
                     ISPP_Send_Raw_Data(BluetoothStackID, SerialPortID, IAP_LINGO_ID_GENERAL, IAP_GENERAL_LINGO_REQUEST_TRANSPORT_MAX_PACKET_SIZE, 0, 0, NULL);
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
            }
            break;
         case etIPort_Open_Session_Indication:
            Display(("\r\n"));

            SessionID = ISPP_Event_Data->Event_Data.ISPP_Session_Open_Indication->SessionID;

            Display(("ISPP Port Open Session Indication %d\r\n", SessionID));

            ISPP_Open_Session_Request_Response(BluetoothStackID, SerialPortID, SessionID, TRUE);

            iAPSessionID     = SessionID;
            TotalSessionData = 0;
            break;
         case etIPort_Close_Session_Indication:
            Display(("\r\n"));

            SessionID = ISPP_Event_Data->Event_Data.ISPP_Session_Close_Indication->SessionID;

            Display(("ISPP Port Close Session Indication SessionID %d\r\n", SessionID));

            iAPSessionID = 0;
            break;
         case etIPort_Session_Data_Indication:
            Display(("\r\n"));

            SessionID         = ISPP_Event_Data->Event_Data.ISPP_Session_Data_Indication->SessionID;
            DataLength        = ISPP_Event_Data->Event_Data.ISPP_Session_Data_Indication->DataLength;
            TotalSessionData += DataLength;

            Display(("Port Session Data Indication %d bytes of %d\r\n", DataLength, TotalSessionData));
            break;
         case etIPort_Send_Session_Data_Confirmation:
            PacketID     = ISPP_Event_Data->Event_Data.ISPP_Send_Session_Data_Confirmation->PacketID;
            SessionID    = ISPP_Event_Data->Event_Data.ISPP_Send_Session_Data_Confirmation->SessionID;
            Status       = ISPP_Event_Data->Event_Data.ISPP_Send_Session_Data_Confirmation->Status;

            Display(("ISPP Port Send Session Data Confirmation. PacketID %d SessionID %d Status %d\r\n", PacketID, SessionID, Status));
            if(SendInfo.PacketID == PacketID)
            {
               /* Verify that the packet was succcessfully sent.        */
               if(Status == PACKET_SEND_STATUS_ACKNOWLEDGED)
               {
                  /* Credit the number of byte that we have sent.       */
                  SendInfo.BytesSent += SendInfo.PendingCount;
                  Display(("\r\nSent %d of %d\r\n", SendInfo.BytesSent, SendInfo.BytesToSend));
                  if(SendInfo.BytesSent < SendInfo.BytesToSend)
                  {
                     /* Setup for another transfer.                     */
                     DataLength = (SendInfo.BytesToSend - SendInfo.BytesSent);

                     /* Set the Number of bytes to send in the first    */
                     /* packet.                                         */
                     if(DataLength > BTPS_StringLength(SessionData))
                        DataLength = BTPS_StringLength(SessionData);

                     if(DataLength > MaxPacketData)
                        DataLength = MaxPacketData;

                     /* Send the first packet of data.                  */
                     SendInfo.PacketID = ISPP_Send_Session_Data(BluetoothStackID, SerialPortID, SessionID, DataLength, (unsigned char *)SessionData);
                     if(SendInfo.PacketID > 0)
                        SendInfo.PendingCount = DataLength;
                     else
                     {
                        Display(("SEND failed with error %d\r\n", SendInfo.PacketID));

                        SendInfo.BytesToSend = 0;
                     }
                  }
                  else
                     SendInfo.BytesToSend = 0;
               }
               else
                  SendInfo.BytesToSend = 0;
            }
            break;
         case etIPort_Send_Raw_Data_Confirmation:
            Display(("\r\n"));

            PacketID = ISPP_Event_Data->Event_Data.ISPP_Send_Raw_Data_Confirmation->PacketID;
            Status   = ISPP_Event_Data->Event_Data.ISPP_Send_Raw_Data_Confirmation->Status;

            Display(("ISPP Port Send Raw Data Confirmation. PacketID %d Status %d\r\n", PacketID, Status));
            break;
         case etIPort_Raw_Data_Indication:
            /* Check to see if this is a response to the Max Packet Size*/
            /* request.                                                 */
            if((ISPP_Event_Data->Event_Data.ISPP_Raw_Data_Indication->Lingo == IAP_LINGO_ID_GENERAL) && (ISPP_Event_Data->Event_Data.ISPP_Raw_Data_Indication->Command == IAP_GENERAL_LINGO_RETURN_TRANSPORT_MAX_PACKET_SIZE))
            {
               MaxPacketData = READ_UNALIGNED_WORD_BIG_ENDIAN(&ISPP_Event_Data->Event_Data.ISPP_Raw_Data_Indication->DataPtr[2]);

               Display(("Max Packet Size %d\r\n", MaxPacketData));

               /* Since the Transaction ID and SessionID are counted in */
               /* the Payload, then the MaxPacketData is 4 bytes less.  */
               MaxPacketData -= 4;
            }
            else
            {
               if((ISPP_Event_Data->Event_Data.ISPP_Raw_Data_Indication->Lingo == IAP_LINGO_ID_GENERAL) && (ISPP_Event_Data->Event_Data.ISPP_Raw_Data_Indication->Command == IAP_GENERAL_LINGO_NOTIFY_IPOD_STATE_CHANGE))
               {
                  Status = READ_UNALIGNED_BYTE_BIG_ENDIAN(&ISPP_Event_Data->Event_Data.ISPP_Raw_Data_Indication->DataPtr[2]);

                  Display(("Power State Change: %d\r\n", Status));
               }
               else
               {
                  Display(("ISPP Port Raw Data Indication on Port\r\n", ISPP_Event_Data->Event_Data.ISPP_Raw_Data_Indication->SerialPortID));

                  Display(("Lingo   : %02X", ISPP_Event_Data->Event_Data.ISPP_Raw_Data_Indication->Lingo));
                  Display(("Command : %02X", ISPP_Event_Data->Event_Data.ISPP_Raw_Data_Indication->Command));
                  Display(("Length  : %d",   ISPP_Event_Data->Event_Data.ISPP_Raw_Data_Indication->DataLength));

                  BTPS_DumpData(ISPP_Event_Data->Event_Data.ISPP_Raw_Data_Indication->DataLength, ISPP_Event_Data->Event_Data.ISPP_Raw_Data_Indication->DataPtr);
               }
            }
            break;
         default:
            /* An unknown/unexpected SPP event was received.            */
            Display(("\r\n"));
            Display(("Unknown Event.\r\n"));
            break;
      }

      /* Check the return value of any function that might have been    */
      /* executed in the callback.                                      */
      if(ret_val)
      {
         /* An error occurred, so output an error message.              */
         Display(("\r\n"));
         Display(("Error %d.\r\n", ret_val));
      }
   }
   else
   {
      /* There was an error with one or more of the input parameters.   */
      Display(("Null Event\r\n"));
   }

   if(_DisplayPrompt)
      DisplayPrompt();
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

               Display(("\r\n"));
               Display(("HCI Mode Change Event, Status: 0x%02X, Connection Handle: %d, Mode: %s, Interval: %d\r\n", HCI_Event_Data->Event_Data.HCI_Mode_Change_Event_Data->Status,
                                                                                                                    HCI_Event_Data->Event_Data.HCI_Mode_Change_Event_Data->Connection_Handle,
                                                                                                                    Mode,
                                                                                                                    HCI_Event_Data->Event_Data.HCI_Mode_Change_Event_Data->Interval));
               DisplayPrompt();
            }
            break;
         default:
            break;
      }
   }
}

   /* The following function is used to initialize the application      */
   /* instance.  This function should open the stack and prepare to     */
   /* execute commands based on user input.                             */
void SampleMain(void *UserParameter)
{
   int                           Result;
   HCI_DriverInformation_t       HCI_DriverInformation;

      LoopbackActive          = FALSE;
      DisplayRawData          = FALSE;
      AutomaticReadActive     = FALSE;
      Connected               = 0;
      Connecting              = FALSE;
      iAPTimerID              = 0;
      iAPSessionID            = 0;
   /* Configure the UART Parameters.                                    */
   HCI_DRIVER_SET_COMM_INFORMATION(&HCI_DriverInformation, 1, 4000000, cpUART);
   HCI_DriverInformation.DriverInformation.COMMDriverInformation.InitializationDelay = 100;

   /* Try to Open the stack and check if it was successful.             */
   if(!OpenStack(&HCI_DriverInformation))
   {
      /* The stack was opened successfully.  Now set some defaults.     */

      /* First, attempt to set the Device to be Connectable.            */
      Result = SetConnect();

      /* Next, check to see if the Device was successfully made         */
      /* Connectable.                                                   */
      if(!Result)
      {
         /* Now that the device is Connectable attempt to make it       */
         /* Discoverable.                                               */
         Result = SetDisc();

         /* Next, check to see if the Device was successfully made      */
         /* Discoverable.                                               */
         if(!Result)
         {
            /* Now that the device is discoverable attempt to make it   */
            /* pairable.                                                */
            Result = SetPairable();
            if(!Result)
            {
               /* Attempt to register a HCI Event Callback.             */
               Result = HCI_Register_Event_Callback(BluetoothStackID, HCI_Event_Callback, (unsigned long)NULL);
               if(Result > 0)
               {
                  /* Assign the Callback Handle.                        */
                  HCIEventCallbackHandle = Result;

                  /* Attempt to create LED Toggle Task.              */
                  Result = LED_TOGGLE_RATE_SUCCESS;
                  if(BTPS_CreateThread(ToggleLED, 384, (void *)Result) != NULL)
                  {
                     UI_Mode = UI_MODE_IS_INVALID;
                     while(1)
                     {
                        /* Next, check to see if the program is      */
                        /* running Client or Server Mode.            */

                        if(UI_Mode == UI_MODE_IS_CLIENT)
                        {
                           /* The program is currently running in    */
                           /* Client Mode.  Start the User Interface.*/
                           UserInterface_Client();

                           /* Restart the User Interface Selection.  */
                           UI_Mode = UI_MODE_IS_INVALID;
                        }
                        else
                        {
                           if(UI_Mode == UI_MODE_IS_SERVER)
                           {
                              /* The Server has been set up          */
                              /* successfully start the User         */
                              /* Interface.                          */
                              UserInterface_Server();

                              /* Restart the User Interface          */
                              /* Selection.                          */
                              UI_Mode = UI_MODE_IS_INVALID;
                           }
                           else
                           {
                              /* Call a function to select the user  */
                              /* User Interface.                     */
                              UserInterface_Selection();
                           }
                        }
                     }
                  }
                  else
                  {
                     Display(("Error creating LED Toggle Task.\r\n"));

                     /* Unregister the previously registered HCI     */
                     /* Event Callback.                              */
                     HCI_Un_Register_Callback(BluetoothStackID, HCIEventCallbackHandle);

                     HCIEventCallbackHandle = 0;
                  }
               }
               else
                  DisplayFunctionError("HCI_Register_Event_Callback()", Result);
            }
            else
               DisplayFunctionError("SetPairable", Result);
         }
         else
            DisplayFunctionError("SetDisc", Result);
      }
      else
         DisplayFunctionError("SetDisc", Result);

      /* Close the Bluetooth Stack.                                     */
      CloseStack();
   }
   else
   {
      /* There was an error while attempting to open the Stack.         */
      Display(("Unable to open the stack.\r\n"));
   }

   /* If we get here an error occurred.                                 */
   while(1)
      ToggleLED((void *)100);
}

