/*****< sppledemo.c >**********************************************************/
/*      Copyright 2012 - 2013 Stonestreet One.                                */
/*      All Rights Reserved.                                                  */
/*                                                                            */
/*  SPPLEDEMO - Embedded Bluetooth SPP Emulation using GATT (LE) application. */
/*                                                                            */
/*  Author:  Tim Cook                                                         */
/*                                                                            */
/*** MODIFICATION HISTORY *****************************************************/
/*                                                                            */
/*   mm/dd/yy  F. Lastname    Description of Modification                     */
/*   --------  -----------    ------------------------------------------------*/
/*   04/16/12  Tim Cook       Initial creation.                               */
/******************************************************************************/
#include "SPPLEDemo.h"           /* Application Header.                       */
#include "HAL.h"                 /* Function for Hardware Abstraction.        */
#include "SS1BTPS.h"             /* Main SS1 Bluetooth Stack Header.          */
#include "BTPSKRNL.h"            /* BTPS Kernel Header.                       */
#include "HCITRANS.h"            /* HCI Transport Header.                     */
#include "BTPSVEND.h"            /* BTPS Vendor Header.                       */

#include "SS1BTGAT.h"            /* Main SS1 GATT Header.                     */
#include "SS1BTGAP.h"            /* Main SS1 GAP Service Header.              */

#include <mqx.h>

#define MAX_SUPPORTED_COMMANDS                     (36)  /* Denotes the       */
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

#define DEFAULT_IO_CAPABILITY       (licNoInputNoOutput) /* Denotes the       */
                                                         /* default I/O       */
                                                         /* Capability that is*/
                                                         /* used with Pairing.*/

#define DEFAULT_MITM_PROTECTION                 (TRUE)   /* Denotes the       */
                                                         /* default value used*/
                                                         /* for Man in the    */
                                                         /* Middle (MITM)     */
                                                         /* protection used   */
                                                         /* with Secure Simple*/
                                                         /* Pairing.          */

#define SPPLE_DATA_BUFFER_LENGTH  (BTPS_CONFIGURATION_GATT_MAXIMUM_SUPPORTED_MTU_SIZE)
                                                         /* Defines the length*/
                                                         /* of a SPPLE Data   */
                                                         /* Buffer.           */

#define SPPLE_DATA_CREDITS        (SPPLE_DATA_BUFFER_LENGTH*3) /* Defines the */
                                                         /* number of credits */
                                                         /* in an SPPLE Buffer*/

#define LED_TOGGLE_RATE_SUCCESS                   (500)  /* The LED Toggle    */
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

   /* Determine the Name we will use for this compilation.              */
#define LE_DEMO_DEVICE_NAME                        "SPPLEDemo"

   /* The following represent the possible values of UI_Mode variable.  */
#define UI_MODE_IS_CLIENT      (1)
#define UI_MODE_IS_SERVER      (0)
#define UI_MODE_IS_INVALID     (-1)

   /* Following converts a Sniff Parameter in Milliseconds to frames.   */
#define MILLISECONDS_TO_BASEBAND_SLOTS(_x)   ((_x) / (0.625))

   /* The following is used as a printf replacement.                    */
#define Display(_x)            do { BTPS_OutputMessage _x; } while(0)

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

   /* Structure used to hold all of the GAP LE Parameters.              */
typedef struct _tagGAPLE_Parameters_t
{
   GAP_LE_Connectability_Mode_t ConnectableMode;
   GAP_Discoverability_Mode_t   DiscoverabilityMode;
   GAP_LE_IO_Capability_t       IOCapability;
   Boolean_t                    MITMProtection;
   Boolean_t                    OOBDataPresent;
} GAPLE_Parameters_t;

#define GAPLE_PARAMETERS_DATA_SIZE                       (sizeof(GAPLE_Parameters_t))

   /* The following structure holds status information about a send     */
   /* process.                                                          */
typedef struct _tagSend_Info_t
{
   DWord_t BytesToSend;
   DWord_t BytesSent;
} Send_Info_t;

   /* The following defines the format of a SPPLE Data Buffer.          */
typedef struct __tagSPPLE_Data_Buffer_t
{
   unsigned int  InIndex;
   unsigned int  OutIndex;
   unsigned int  BytesFree;
   unsigned int  BufferSize;
   Byte_t        Buffer[SPPLE_DATA_BUFFER_LENGTH*3];
} SPPLE_Data_Buffer_t;

   /* The following structure represents the information we will store  */
   /* on a Discovered GAP Service.                                      */
typedef struct _tagGAPS_Client_Info_t
{
   Word_t DeviceNameHandle;
   Word_t DeviceAppearanceHandle;
} GAPS_Client_Info_t;

   /* The following structure holds information on known Device         */
   /* Appearance Values.                                                */
typedef struct _tagGAPS_Device_Appearance_Mapping_t
{
   Word_t  Appearance;
   char   *String;
} GAPS_Device_Appearance_Mapping_t;

   /* The following structure for a Master is used to hold a list of    */
   /* information on all paired devices. For slave we will not use this */
   /* structure.                                                        */
typedef struct _tagDeviceInfoInfo_t
{
   Byte_t                       Flags;
   Byte_t                       EncryptionKeySize;
   GAP_LE_Address_Type_t        ConnectionAddressType;
   BD_ADDR_t                    ConnectionBD_ADDR;
   Long_Term_Key_t              LTK;
   Random_Number_t              Rand;
   Word_t                       EDIV;
   unsigned int                 TransmitCredits;
   SPPLE_Data_Buffer_t          ReceiveBuffer;
   SPPLE_Data_Buffer_t          TransmitBuffer;
   GAPS_Client_Info_t           GAPSClientInfo;
   SPPLE_Client_Info_t          ClientInfo;
   SPPLE_Server_Info_t          ServerInfo;
   struct _tagDeviceInfoInfo_t *NextDeviceInfoInfoPtr;
} DeviceInfo_t;

#define DEVICE_INFO_DATA_SIZE                            (sizeof(DeviceInfo_t))

   /* Defines the bit mask flags that may be set in the DeviceInfo_t    */
   /* structure.                                                        */
#define DEVICE_INFO_FLAGS_LTK_VALID                         0x01
#define DEVICE_INFO_FLAGS_SPPLE_SERVER                      0x02
#define DEVICE_INFO_FLAGS_SERVICE_DISCOVERY_OUTSTANDING     0x04

   /* User to represent a structure to hold a BD_ADDR return from       */
   /* BD_ADDRToStr.                                                     */
typedef char BoardStr_t[16];

                        /* The Encryption Root Key should be generated  */
                        /* in such a way as to guarantee 128 bits of    */
                        /* entropy.                                     */
static BTPSCONST Encryption_Key_t ER = {0x28, 0xBA, 0xE1, 0x37, 0x13, 0xB2, 0x20, 0x45, 0x16, 0xB2, 0x19, 0xD0, 0x80, 0xEE, 0x4A, 0x51};

                        /* The Identity Root Key should be generated    */
                        /* in such a way as to guarantee 128 bits of    */
                        /* entropy.                                     */
static BTPSCONST Encryption_Key_t IR = {0x41, 0x09, 0xA0, 0x88, 0x09, 0x6B, 0x70, 0xC0, 0x95, 0x23, 0x3C, 0x8C, 0x48, 0xFC, 0xC9, 0xFE};

                        /* The following keys can be regenerated on the */
                        /* fly using the constant IR and ER keys and    */
                        /* are used globally, for all devices.          */
static Encryption_Key_t DHK;
static Encryption_Key_t IRK;

   /* Internal Variables to this Module (Remember that all variables    */
   /* declared static are initialized to 0 automatically by the         */
   /* compiler as part of standard C/C++).                              */
static Byte_t              SPPLEBuffer[SPPLE_DATA_BUFFER_LENGTH+1];  /* Buffer that is */
                                                    /* used for Sending/Receiving      */
                                                    /* SPPLE Service Data.             */

static unsigned int        SPPLEServiceID;          /* The following holds the SPP LE  */
                                                    /* Service ID that is returned from*/
                                                    /* GATT_Register_Service().        */

static unsigned int        GAPSInstanceID;          /* Holds the Instance ID for the   */
                                                    /* GAP Service.                    */

static GAPLE_Parameters_t  LE_Parameters;           /* Holds GAP Parameters like       */
                                                    /* Discoverability, Connectability */
                                                    /* Modes.                          */

static DeviceInfo_t       *DeviceInfoList;          /* Holds the list head for the     */
                                                    /* device info list.               */

static unsigned int        BluetoothStackID;        /* Variable which holds the Handle */
                                                    /* of the opened Bluetooth Protocol*/
                                                    /* Stack.                          */

static BD_ADDR_t           ConnectionBD_ADDR;       /* Holds the BD_ADDR of the        */
                                                    /* currently connected device.     */

static unsigned int        ConnectionID;            /* Holds the Connection ID of the  */
                                                    /* currently connected device.     */

static Boolean_t           LocalDeviceIsMaster;     /* Boolean that tells if the local */
                                                    /* device is the master of the     */
                                                    /* current connection.             */

static BD_ADDR_t           CurrentRemoteBD_ADDR;    /* Variable which holds the        */
                                                    /* current BD_ADDR of the device   */
                                                    /* which is currently pairing or   */
                                                    /* authenticating.                 */

static unsigned int        NumberCommands;          /* Variable which is used to hold  */
                                                    /* the number of Commands that are */
                                                    /* supported by this application.  */
                                                    /* Commands are added individually.*/

static CommandTable_t      CommandTable[MAX_SUPPORTED_COMMANDS]; /* Variable which is  */
                                                    /* used to hold the actual Commands*/
                                                    /* that are supported by this      */
                                                    /* application.                    */

static char                Input[MAX_COMMAND_LENGTH];/* Buffer which holds the user    */
                                                    /* input to be parsed.             */

static Send_Info_t         SendInfo;                /* Variable that contains          */
                                                    /* information about a data        */
                                                    /* transfer process.               */

static Boolean_t           LoopbackActive;          /* Variable which flags whether or */
                                                    /* not the application is currently*/
                                                    /* operating in Loopback Mode      */
                                                    /* (TRUE) or not (FALSE).          */

static Boolean_t           DisplayRawData;          /* Variable which flags whether or */
                                                    /* not the application is to       */
                                                    /* simply display the Raw Data     */
                                                    /* when it is received (when not   */
                                                    /* operating in Loopback Mode).    */

static Boolean_t           AutomaticReadActive;     /* Variable which flags whether or */
                                                    /* not the application is to       */
                                                    /* automatically read all data     */
                                                    /* as it is received.              */

   /* The following is used to map from ATT Error Codes to a printable  */
   /* string.                                                           */
static char *ErrorCodeStr[] =
{
   "ATT_PROTOCOL_ERROR_CODE_NO_ERROR",
   "ATT_PROTOCOL_ERROR_CODE_INVALID_HANDLE",
   "ATT_PROTOCOL_ERROR_CODE_READ_NOT_PERMITTED",
   "ATT_PROTOCOL_ERROR_CODE_WRITE_NOT_PERMITTED",
   "ATT_PROTOCOL_ERROR_CODE_INVALID_PDU",
   "ATT_PROTOCOL_ERROR_CODE_INSUFFICIENT_AUTHENTICATION",
   "ATT_PROTOCOL_ERROR_CODE_REQUEST_NOT_SUPPORTED",
   "ATT_PROTOCOL_ERROR_CODE_INVALID_OFFSET",
   "ATT_PROTOCOL_ERROR_CODE_INSUFFICIENT_AUTHORIZATION",
   "ATT_PROTOCOL_ERROR_CODE_PREPARE_QUEUE_FULL",
   "ATT_PROTOCOL_ERROR_CODE_ATTRIBUTE_NOT_FOUND",
   "ATT_PROTOCOL_ERROR_CODE_ATTRIBUTE_NOT_LONG",
   "ATT_PROTOCOL_ERROR_CODE_INSUFFICIENT_ENCRYPTION_KEY_SIZE",
   "ATT_PROTOCOL_ERROR_CODE_INVALID_ATTRIBUTE_VALUE_LENGTH",
   "ATT_PROTOCOL_ERROR_CODE_UNLIKELY_ERROR",
   "ATT_PROTOCOL_ERROR_CODE_INSUFFICIENT_ENCRYPTION",
   "ATT_PROTOCOL_ERROR_CODE_UNSUPPORTED_GROUP_TYPE",
   "ATT_PROTOCOL_ERROR_CODE_INSUFFICIENT_RESOURCES"
} ;

   /* The following array is used to map Device Appearance Values to    */
   /* strings.                                                          */
static GAPS_Device_Appearance_Mapping_t AppearanceMappings[] =
{
   { GAP_DEVICE_APPEARENCE_VALUE_UNKNOWN,                        "Unknown"                   },
   { GAP_DEVICE_APPEARENCE_VALUE_GENERIC_PHONE,                  "Generic Phone"             },
   { GAP_DEVICE_APPEARENCE_VALUE_GENERIC_COMPUTER,               "Generic Computer"          },
   { GAP_DEVICE_APPEARENCE_VALUE_GENERIC_WATCH,                  "Generic Watch"             },
   { GAP_DEVICE_APPEARENCE_VALUE_SPORTS_WATCH,                   "Sports Watch"              },
   { GAP_DEVICE_APPEARENCE_VALUE_GENERIC_CLOCK,                  "Generic Clock"             },
   { GAP_DEVICE_APPEARENCE_VALUE_GENERIC_DISPLAY,                "Generic Display"           },
   { GAP_DEVICE_APPEARENCE_VALUE_GENERIC_GENERIC_REMOTE_CONTROL, "Generic Remote Control"    },
   { GAP_DEVICE_APPEARENCE_VALUE_GENERIC_EYE_GLASSES,            "Eye Glasses"               },
   { GAP_DEVICE_APPEARENCE_VALUE_GENERIC_TAG,                    "Generic Tag"               },
   { GAP_DEVICE_APPEARENCE_VALUE_GENERIC_KEYRING,                "Generic Keyring"           },
   { GAP_DEVICE_APPEARENCE_VALUE_GENERIC_MEDIA_PLAYER,           "Generic Media Player"      },
   { GAP_DEVICE_APPEARENCE_VALUE_GENERIC_BARCODE_SCANNER,        "Generic Barcode Scanner"   },
   { GAP_DEVICE_APPEARENCE_VALUE_GENERIC_THERMOMETER,            "Generic Thermometer"       },
   { GAP_DEVICE_APPEARENCE_VALUE_THERMOMETER_EAR,                "Ear Thermometer"           },
   { GAP_DEVICE_APPEARENCE_VALUE_GENERIC_HEART_RATE_SENSOR,      "Generic Heart Rate Sensor" },
   { GAP_DEVICE_APPEARENCE_VALUE_BELT_HEART_RATE_SENSOR,         "Belt Heart Rate Sensor"    },
   { GAP_DEVICE_APPEARENCE_VALUE_GENERIC_BLOOD_PRESSURE,         "Generic Blood Pressure"    },
   { GAP_DEVICE_APPEARENCE_VALUE_BLOOD_PRESSURE_ARM,             "Blood Pressure: ARM"       },
   { GAP_DEVICE_APPEARENCE_VALUE_BLOOD_PRESSURE_WRIST,           "Blood Pressure: Wrist"     },
   { GAP_DEVICE_APPEARENCE_VALUE_HUMAN_INTERFACE_DEVICE,         "Human Interface Device"    },
   { GAP_DEVICE_APPEARENCE_VALUE_HID_KEYBOARD,                   "HID Keyboard"              },
   { GAP_DEVICE_APPEARENCE_VALUE_HID_MOUSE,                      "HID Mouse"                 },
   { GAP_DEVICE_APPEARENCE_VALUE_HID_JOYSTICK,                   "HID Joystick"              },
   { GAP_DEVICE_APPEARENCE_VALUE_HID_GAMEPAD,                    "HID Gamepad"               },
   { GAP_DEVICE_APPEARENCE_VALUE_HID_DIGITIZER_TABLET,           "HID Digitizer Tablet"      },
   { GAP_DEVICE_APPEARENCE_VALUE_HID_CARD_READER,                "HID Card Reader"           },
   { GAP_DEVICE_APPEARENCE_VALUE_HID_DIGITAL_PEN,                "HID Digitizer Pen"         },
   { GAP_DEVICE_APPEARENCE_VALUE_HID_BARCODE_SCANNER,            "HID Bardcode Scanner"      },
   { GAP_DEVICE_APPEARENCE_VALUE_GENERIC_GLUCOSE_METER,          "Generic Glucose Meter"     }
} ;

#define NUMBER_OF_APPEARANCE_MAPPINGS     (sizeof(AppearanceMappings)/sizeof(GAPS_Device_Appearance_Mapping_t))

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
   "No Input/Output",
   "Keyboard/Display"
} ;

   /* The following defines a data sequence that will be used to        */
   /* generate message data.                                            */
static char  DataStr[]  = "~!@#$%^&*()_+`1234567890-=:;\"'<>?,./@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]`abcdefghijklmnopqrstuvwxyz{|}<>\r\n";
static int   DataStrLen = (sizeof(DataStr)-1);

   /*********************************************************************/
   /**                     SPPLE Service Table                         **/
   /*********************************************************************/

   /* The SPPLE Service Declaration UUID.                               */
static BTPSCONST GATT_Primary_Service_128_Entry_t SPPLE_Service_UUID =
{
   SPPLE_SERVICE_UUID_CONSTANT
} ;

   /* The Tx Characteristic Declaration.                                */
static BTPSCONST GATT_Characteristic_Declaration_128_Entry_t SPPLE_Tx_Declaration =
{
   GATT_CHARACTERISTIC_PROPERTIES_NOTIFY,
   SPPLE_TX_CHARACTERISTIC_UUID_CONSTANT
} ;

   /* The Tx Characteristic Value.                                      */
static BTPSCONST GATT_Characteristic_Value_128_Entry_t  SPPLE_Tx_Value =
{
   SPPLE_TX_CHARACTERISTIC_UUID_CONSTANT,
   0,
   NULL
} ;

   /* The Tx Credits Characteristic Declaration.                        */
static BTPSCONST GATT_Characteristic_Declaration_128_Entry_t SPPLE_Tx_Credits_Declaration =
{
   (GATT_CHARACTERISTIC_PROPERTIES_READ|GATT_CHARACTERISTIC_PROPERTIES_WRITE_WITHOUT_RESPONSE|GATT_CHARACTERISTIC_PROPERTIES_WRITE),
   SPPLE_TX_CREDITS_CHARACTERISTIC_UUID_CONSTANT
} ;

   /* The Tx Credits Characteristic Value.                              */
static BTPSCONST GATT_Characteristic_Value_128_Entry_t SPPLE_Tx_Credits_Value =
{
   SPPLE_TX_CREDITS_CHARACTERISTIC_UUID_CONSTANT,
   0,
   NULL
} ;

   /* The SPPLE RX Characteristic Declaration.                          */
static BTPSCONST GATT_Characteristic_Declaration_128_Entry_t SPPLE_Rx_Declaration =
{
   (GATT_CHARACTERISTIC_PROPERTIES_WRITE_WITHOUT_RESPONSE),
   SPPLE_RX_CHARACTERISTIC_UUID_CONSTANT
} ;

   /* The SPPLE RX Characteristic Value.                                */
static BTPSCONST GATT_Characteristic_Value_128_Entry_t  SPPLE_Rx_Value =
{
   SPPLE_RX_CHARACTERISTIC_UUID_CONSTANT,
   0,
   NULL
} ;

   /* The SPPLE Rx Credits Characteristic Declaration.                  */
static BTPSCONST GATT_Characteristic_Declaration_128_Entry_t SPPLE_Rx_Credits_Declaration =
{
   (GATT_CHARACTERISTIC_PROPERTIES_READ|GATT_CHARACTERISTIC_PROPERTIES_NOTIFY),
   SPPLE_RX_CREDITS_CHARACTERISTIC_UUID_CONSTANT
};

   /* The SPPLE Rx Credits Characteristic Value.                        */
static BTPSCONST GATT_Characteristic_Value_128_Entry_t SPPLE_Rx_Credits_Value =
{
   SPPLE_RX_CREDITS_CHARACTERISTIC_UUID_CONSTANT,
   0,
   NULL
};

   /* Client Characteristic Configuration Descriptor.                   */
static GATT_Characteristic_Descriptor_16_Entry_t Client_Characteristic_Configuration =
{
   GATT_CLIENT_CHARACTERISTIC_CONFIGURATION_BLUETOOTH_UUID_CONSTANT,
   GATT_CLIENT_CHARACTERISTIC_CONFIGURATION_LENGTH,
   NULL
};

   /* The following defines the SPPLE service that is registered with   */
   /* the GATT_Register_Service function call.                          */
   /* * NOTE * This array will be registered with GATT in the call to   */
   /*          GATT_Register_Service.                                   */
BTPSCONST GATT_Service_Attribute_Entry_t SPPLE_Service[] =
{
   { GATT_ATTRIBUTE_FLAGS_READABLE,          aetPrimaryService128,            (Byte_t *)&SPPLE_Service_UUID                  },
   { GATT_ATTRIBUTE_FLAGS_READABLE,          aetCharacteristicDeclaration128, (Byte_t *)&SPPLE_Tx_Declaration                },
   { 0,                                      aetCharacteristicValue128,       (Byte_t *)&SPPLE_Tx_Value                      },
   { GATT_ATTRIBUTE_FLAGS_READABLE_WRITABLE, aetCharacteristicDescriptor16,   (Byte_t *)&Client_Characteristic_Configuration },
   { GATT_ATTRIBUTE_FLAGS_READABLE,          aetCharacteristicDeclaration128, (Byte_t *)&SPPLE_Tx_Credits_Declaration        },
   { GATT_ATTRIBUTE_FLAGS_READABLE_WRITABLE, aetCharacteristicValue128,       (Byte_t *)&SPPLE_Tx_Credits_Value              },
   { GATT_ATTRIBUTE_FLAGS_READABLE,          aetCharacteristicDeclaration128, (Byte_t *)&SPPLE_Rx_Declaration                },
   { GATT_ATTRIBUTE_FLAGS_WRITABLE,          aetCharacteristicValue128,       (Byte_t *)&SPPLE_Rx_Value                      },
   { GATT_ATTRIBUTE_FLAGS_READABLE,          aetCharacteristicDeclaration128, (Byte_t *)&SPPLE_Rx_Credits_Declaration        },
   { GATT_ATTRIBUTE_FLAGS_READABLE,          aetCharacteristicValue128,       (Byte_t *)&SPPLE_Rx_Credits_Value              },
   { GATT_ATTRIBUTE_FLAGS_READABLE_WRITABLE, aetCharacteristicDescriptor16,   (Byte_t *)&Client_Characteristic_Configuration }
} ;

#define SPPLE_SERVICE_ATTRIBUTE_COUNT               (sizeof(SPPLE_Service)/sizeof(GATT_Service_Attribute_Entry_t))

#define SPPLE_TX_CHARACTERISTIC_ATTRIBUTE_OFFSET               2
#define SPPLE_TX_CHARACTERISTIC_CCD_ATTRIBUTE_OFFSET           3
#define SPPLE_TX_CREDITS_CHARACTERISTIC_ATTRIBUTE_OFFSET       5
#define SPPLE_RX_CHARACTERISTIC_ATTRIBUTE_OFFSET               7
#define SPPLE_RX_CREDITS_CHARACTERISTIC_ATTRIBUTE_OFFSET       9
#define SPPLE_RX_CREDITS_CHARACTERISTIC_CCD_ATTRIBUTE_OFFSET   10

   /*********************************************************************/
   /**                    END OF SERVICE TABLE                         **/
   /*********************************************************************/

   /* Internal function prototypes.                                     */
static Boolean_t CreateNewDeviceInfoEntry(DeviceInfo_t **ListHead, GAP_LE_Address_Type_t ConnectionAddressType, BD_ADDR_t ConnectionBD_ADDR);
static DeviceInfo_t *SearchDeviceInfoEntryByBD_ADDR(DeviceInfo_t **ListHead, BD_ADDR_t BD_ADDR);
static DeviceInfo_t *DeleteDeviceInfoEntry(DeviceInfo_t **ListHead, BD_ADDR_t BD_ADDR);
static void FreeDeviceInfoEntryMemory(DeviceInfo_t *EntryToFree);
static void FreeDeviceInfoList(DeviceInfo_t **ListHead);

static void UserInterface(void);
static void GetInput(unsigned int InputBufferLength, char *InputBuffer);
static int CommandLineInterpreter(void);
static unsigned int StringToUnsignedInteger(char *StringInteger);
static char *StringParser(char *String);
static int CommandParser(UserCommand_t *TempCommand, char *Input);
static int CommandInterpreter(UserCommand_t *TempCommand);
static int AddCommand(char *CommandName, CommandFunction_t CommandFunction);
static CommandFunction_t FindCommand(char *Command);
static void ClearCommands(void);

static void BD_ADDRToStr(BD_ADDR_t Board_Address, BoardStr_t BoardStr);
static void StrToBD_ADDR(char *BoardStr, BD_ADDR_t *Board_Address);

static void DisplayIOCapabilities(void);
static void DisplayAdvertisingData(GAP_LE_Advertising_Data_t *Advertising_Data);
static void DisplayPairingInformation(GAP_LE_Pairing_Capabilities_t Pairing_Capabilities);
static void DisplayUUID(GATT_UUID_t *UUID);
static void DisplayPrompt(void);
static void DisplayUsage(char *UsageString);
static void DisplayFunctionError(char *Function,int Status);
static void DisplayFunctionSuccess(char *Function);

static int QueryMemory(ParameterList_t *TempParam);

static int OpenStack(HCI_DriverInformation_t *HCI_DriverInformation);
static int CloseStack(void);

static int SetDisc(void);
static int SetConnect(void);
static int SetPairable(void);

static void DumpAppearanceMappings(void);
static Boolean_t AppearanceToString(Word_t Appearance, char **String);
static Boolean_t AppearanceIndexToAppearance(unsigned int Index, Word_t *Appearance);

static void GAPSPopulateHandles(GAPS_Client_Info_t *ClientInfo, GATT_Service_Discovery_Indication_Data_t *ServiceInfo);
static void SPPLEPopulateHandles(SPPLE_Client_Info_t *ClientInfo, GATT_Service_Discovery_Indication_Data_t *ServiceInfo);

static unsigned int AddDataToBuffer(SPPLE_Data_Buffer_t *DataBuffer, unsigned int DataLength, Byte_t *Data);
static unsigned int RemoveDataFromBuffer(SPPLE_Data_Buffer_t *DataBuffer, unsigned int BufferLength, Byte_t *Buffer);
static void InitializeBuffer(SPPLE_Data_Buffer_t *DataBuffer);

static int EnableDisableNotificationsIndications(Word_t ClientConfigurationHandle, Word_t ClientConfigurationValue, GATT_Client_Event_Callback_t ClientEventCallback);

static unsigned int FillBufferWithString(SPPLE_Data_Buffer_t *DataBuffer, unsigned *CurrentBufferLength, unsigned int MaxLength, Byte_t *Buffer);

static void SendProcess(DeviceInfo_t *DeviceInfo);
static void SendCredits(DeviceInfo_t *DeviceInfo, unsigned int DataLength);
static void ReceiveCreditEvent(DeviceInfo_t *DeviceInfo, unsigned int Credits);
static Boolean_t SendData(DeviceInfo_t *DeviceInfo, unsigned int DataLength, Byte_t *Data);
static void DataIndicationEvent(DeviceInfo_t *DeviceInfo, unsigned int DataLength, Byte_t *Data);
static int ReadData(DeviceInfo_t *DeviceInfo, unsigned int BufferLength, Byte_t *Buffer);

static int StartScan(unsigned int BluetoothStackID);
static int StopScan(unsigned int BluetoothStackID);

static int ConnectLEDevice(unsigned int BluetoothStackID, BD_ADDR_t BD_ADDR, Boolean_t UseWhiteList);
static int DisconnectLEDevice(unsigned int BluetoothStackID, BD_ADDR_t BD_ADDR);

static void ConfigureCapabilities(GAP_LE_Pairing_Capabilities_t *Capabilities);
static int SendPairingRequest(BD_ADDR_t BD_ADDR, Boolean_t ConnectionMaster);
static int SlavePairingRequestResponse(BD_ADDR_t BD_ADDR);
static int EncryptionInformationRequestResponse(BD_ADDR_t BD_ADDR, Byte_t KeySize, GAP_LE_Authentication_Response_Information_t *GAP_LE_Authentication_Response_Information);

static int DisplayHelp(ParameterList_t *TempParam);
static int SetDiscoverabilityMode(ParameterList_t *TempParam);
static int SetConnectabilityMode(ParameterList_t *TempParam);
static int SetPairabilityMode(ParameterList_t *TempParam);
static int ChangePairingParameters(ParameterList_t *TempParam);
static int LEPassKeyResponse(ParameterList_t *TempParam);
static int LEQueryEncryption(ParameterList_t *TempParam);
static int LESetPasskey(ParameterList_t *TempParam);
static int GetLocalAddress(ParameterList_t *TempParam);

static int AdvertiseLE(ParameterList_t *TempParam);

static int StartScanning(ParameterList_t *TempParam);
static int StopScanning(ParameterList_t *TempParam);

static int ConnectLE(ParameterList_t *TempParam);
static int DisconnectLE(ParameterList_t *TempParam);

static int PairLE(ParameterList_t *TempParam);

static int DiscoverGAPS(ParameterList_t *TempParam);
static int ReadLocalName(ParameterList_t *TempParam);
static int SetLocalName(ParameterList_t *TempParam);
static int ReadRemoteName(ParameterList_t *TempParam);
static int ReadLocalAppearance(ParameterList_t *TempParam);
static int SetLocalAppearance(ParameterList_t *TempParam);
static int ReadRemoteAppearance(ParameterList_t *TempParam);

static int DiscoverSPPLE(ParameterList_t *TempParam);
static int RegisterSPPLE(ParameterList_t *TempParam);
static int ConfigureSPPLE(ParameterList_t *TempParam);
static int SendDataCommand(ParameterList_t *TempParam);
static int ReadDataCommand(ParameterList_t *TempParam);

static int Loopback(ParameterList_t *TempParam);
static int DisplayRawModeData(ParameterList_t *TempParam);
static int AutomaticReadMode(ParameterList_t *TempParam);
static int SetBaudRate(ParameterList_t *TempParam);

static int StackChecker(ParameterList_t *TempParam);
   /* BTPS Callback function prototypes.                                */
static void BTPSAPI GAP_LE_Event_Callback(unsigned int BluetoothStackID,GAP_LE_Event_Data_t *GAP_LE_Event_Data, unsigned long CallbackParameter);
static void BTPSAPI GATT_ServerEventCallback(unsigned int BluetoothStackID, GATT_Server_Event_Data_t *GATT_ServerEventData, unsigned long CallbackParameter);
static void BTPSAPI GATT_ClientEventCallback_SPPLE(unsigned int BluetoothStackID, GATT_Client_Event_Data_t *GATT_Client_Event_Data, unsigned long CallbackParameter);
static void BTPSAPI GATT_ClientEventCallback_GAPS(unsigned int BluetoothStackID, GATT_Client_Event_Data_t *GATT_Client_Event_Data, unsigned long CallbackParameter);
static void BTPSAPI GATT_Connection_Event_Callback(unsigned int BluetoothStackID, GATT_Connection_Event_Data_t *GATT_Connection_Event_Data, unsigned long CallbackParameter);
static void BTPSAPI GATT_Service_Discovery_Event_Callback(unsigned int BluetoothStackID, GATT_Service_Discovery_Event_Data_t *GATT_Service_Discovery_Event_Data, unsigned long CallbackParameter);

static void *ToggleLED(void *UserParameter);

   /* The following function adds the specified Entry to the specified  */
   /* List.  This function allocates and adds an entry to the list that */
   /* has the same attributes as parameters to this function.  This     */
   /* function will return FALSE if NO Entry was added.  This can occur */
   /* if the element passed in was deemed invalid or the actual List    */
   /* Head was invalid.                                                 */
   /* ** NOTE ** This function does not insert duplicate entries into   */
   /*            the list.  An element is considered a duplicate if the */
   /*            Connection BD_ADDR.  When this occurs, this function   */
   /*            returns NULL.                                          */
static Boolean_t CreateNewDeviceInfoEntry(DeviceInfo_t **ListHead, GAP_LE_Address_Type_t ConnectionAddressType, BD_ADDR_t ConnectionBD_ADDR)
{
   Boolean_t     ret_val = FALSE;
   DeviceInfo_t *DeviceInfoPtr;

   /* Verify that the passed in parameters seem semi-valid.             */
   if((ListHead) && (!COMPARE_NULL_BD_ADDR(ConnectionBD_ADDR)))
   {
      /* Allocate the memory for the entry.                             */
      if((DeviceInfoPtr = BTPS_AllocateMemory(sizeof(DeviceInfo_t))) != NULL)
      {
         /* Initialize the entry.                                       */
         BTPS_MemInitialize(DeviceInfoPtr, 0, sizeof(DeviceInfo_t));
         DeviceInfoPtr->ConnectionAddressType = ConnectionAddressType;
         BTPS_MemCopy(&(DeviceInfoPtr->ConnectionBD_ADDR), &ConnectionBD_ADDR, sizeof(ConnectionBD_ADDR));

         ret_val = BSC_AddGenericListEntry_Actual(ekBD_ADDR_t, BTPS_STRUCTURE_OFFSET(DeviceInfo_t, ConnectionBD_ADDR), BTPS_STRUCTURE_OFFSET(DeviceInfo_t, NextDeviceInfoInfoPtr), (void **)(ListHead), (void *)(DeviceInfoPtr));
         if(!ret_val)
         {
            /* Failed to add to list so we should free the memory that  */
            /* we allocated for the entry.                              */
            BTPS_FreeMemory(DeviceInfoPtr);
         }
      }
   }

   return(ret_val);
}

   /* The following function searches the specified List for the        */
   /* specified Connection BD_ADDR.  This function returns NULL if      */
   /* either the List Head is invalid, the BD_ADDR is invalid, or the   */
   /* Connection BD_ADDR was NOT found.                                 */
static DeviceInfo_t *SearchDeviceInfoEntryByBD_ADDR(DeviceInfo_t **ListHead, BD_ADDR_t BD_ADDR)
{
   return(BSC_SearchGenericListEntry(ekBD_ADDR_t, (void *)(&BD_ADDR), BTPS_STRUCTURE_OFFSET(DeviceInfo_t, ConnectionBD_ADDR), BTPS_STRUCTURE_OFFSET(DeviceInfo_t, NextDeviceInfoInfoPtr), (void **)(ListHead)));
}

   /* The following function searches the specified Key Info List for   */
   /* the specified BD_ADDR and removes it from the List.  This function*/
   /* returns NULL if either the List Head is invalid, the BD_ADDR is   */
   /* invalid, or the specified Entry was NOT present in the list.  The */
   /* entry returned will have the Next Entry field set to NULL, and    */
   /* the caller is responsible for deleting the memory associated with */
   /* this entry by calling the FreeKeyEntryMemory() function.          */
static DeviceInfo_t *DeleteDeviceInfoEntry(DeviceInfo_t **ListHead, BD_ADDR_t BD_ADDR)
{
   return(BSC_DeleteGenericListEntry(ekBD_ADDR_t, (void *)(&BD_ADDR), BTPS_STRUCTURE_OFFSET(DeviceInfo_t, ConnectionBD_ADDR), BTPS_STRUCTURE_OFFSET(DeviceInfo_t, NextDeviceInfoInfoPtr), (void **)(ListHead)));
}

   /* This function frees the specified Key Info Information member     */
   /* memory.                                                           */
static void FreeDeviceInfoEntryMemory(DeviceInfo_t *EntryToFree)
{
   BSC_FreeGenericListEntryMemory((void *)(EntryToFree));
}

   /* The following function deletes (and frees all memory) every       */
   /* element of the specified Key Info List.  Upon return of this      */
   /* function, the Head Pointer is set to NULL.                        */
static void FreeDeviceInfoList(DeviceInfo_t **ListHead)
{
   BSC_FreeGenericListEntryList((void **)(ListHead), BTPS_STRUCTURE_OFFSET(DeviceInfo_t, NextDeviceInfoInfoPtr));
}

   /* This function is responsible for taking the input from the user   */
   /* and dispatching the appropriate Command Function.  First, this    */
   /* function retrieves a String of user input, parses the user input  */
   /* into Command and Parameters, and finally executes the Command or  */
   /* Displays an Error Message if the input is not a valid Command.    */
static void UserInterface(void)
{
   Boolean_t Done;

   Done = FALSE;
   while(!Done)
   {
      /* Display the available commands.                                */
      DisplayHelp(NULL);

      /* Clear the installed command.                                   */
      ClearCommands();

      /* Install the commands relevant for this UI.                     */
      AddCommand("SETDISCOVERABILITYMODE", SetDiscoverabilityMode);
      AddCommand("SETCONNECTABILITYMODE", SetConnectabilityMode);
      AddCommand("SETPAIRABILITYMODE", SetPairabilityMode);
      AddCommand("CHANGEPAIRINGPARAMETERS", ChangePairingParameters);
      AddCommand("GETLOCALADDRESS", GetLocalAddress);
      AddCommand("ADVERTISELE", AdvertiseLE);
      AddCommand("STARTSCANNING", StartScanning);
      AddCommand("STOPSCANNING", StopScanning);
      AddCommand("CONNECTLE", ConnectLE);
      AddCommand("DISCONNECTLE", DisconnectLE);
      AddCommand("PAIRLE", PairLE);
      AddCommand("LEPASSKEYRESPONSE", LEPassKeyResponse);
      AddCommand("QUERYENCRYPTIONMODE", LEQueryEncryption);
      AddCommand("SETPASSKEY", LESetPasskey);
      AddCommand("SEND", SendDataCommand);
      AddCommand("READ", ReadDataCommand);
      AddCommand("LOOPBACK", Loopback);
      AddCommand("DISCOVERSPPLE", DiscoverSPPLE);
      AddCommand("REGISTERSPPLE", RegisterSPPLE);
      AddCommand("CONFIGURESPPLE", ConfigureSPPLE);
      AddCommand("DISCOVERGAPS", DiscoverGAPS);
      AddCommand("GETLOCALNAME", ReadLocalName);
      AddCommand("SETLOCALNAME", SetLocalName);
      AddCommand("GETREMOTENAME", ReadRemoteName);
      AddCommand("GETLOCALAPPEARANCE", ReadLocalAppearance);
      AddCommand("SETLOCALAPPEARANCE", SetLocalAppearance);
      AddCommand("GETREMOTEAPPEARANCE", ReadRemoteAppearance);
      AddCommand("DISPLAYRAWMODEDATA", DisplayRawModeData);
      AddCommand("AUTOMATICREADMODE", AutomaticReadMode);
      AddCommand("SETBAUDRATE", SetBaudRate);
      AddCommand("QUERYMEMORY", QueryMemory);
      AddCommand("CHECKSTACKS", StackChecker);
      AddCommand("HELP", DisplayHelp);

      /* Call command line interpreter to parser user input and call the*/
      /* appropriate command function.                                  */
      CommandLineInterpreter();
   }
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

      /* The command entered is not exit so search for command in table.*/
      if((CommandFunction = FindCommand(TempCommand->Command)) != NULL)
      {
         /* The command was found in the table so call the command.     */
         ret_val = (*CommandFunction)(&TempCommand->Parameters);
         if(!ret_val)
         {
            /* Return success to the caller.                            */
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
      ret_val = INVALID_PARAMETERS_ERROR;

   return(ret_val);
}

   /* The following function is provided to allow a means to            */
   /* problematically add Commands the Global (to this module) Command  */
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
      for(Index=0, ret_val = NULL; ((Index<NumberCommands) && (!ret_val)); Index++)
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

   /* The following function is responsible for the specified string    */
   /* into data of type BD_ADDR.  The first parameter of this function  */
   /* is the BD_ADDR string to be converted to a BD_ADDR.  The second   */
   /* parameter of this function is a pointer to the BD_ADDR in which   */
   /* the converted BD_ADDR String is to be stored.                     */
static void StrToBD_ADDR(char *BoardStr, BD_ADDR_t *Board_Address)
{
   char Buffer[5];

   if((BoardStr) && (BTPS_StringLength(BoardStr) == sizeof(BD_ADDR_t)*2) && (Board_Address))
   {
      Buffer[0] = '0';
      Buffer[1] = 'x';
      Buffer[4] = '\0';

      Buffer[2] = BoardStr[0];
      Buffer[3] = BoardStr[1];
      Board_Address->BD_ADDR5 = (Byte_t)StringToUnsignedInteger(Buffer);

      Buffer[2] = BoardStr[2];
      Buffer[3] = BoardStr[3];
      Board_Address->BD_ADDR4 = (Byte_t)StringToUnsignedInteger(Buffer);

      Buffer[2] = BoardStr[4];
      Buffer[3] = BoardStr[5];
      Board_Address->BD_ADDR3 = (Byte_t)StringToUnsignedInteger(Buffer);

      Buffer[2] = BoardStr[6];
      Buffer[3] = BoardStr[7];
      Board_Address->BD_ADDR2 = (Byte_t)StringToUnsignedInteger(Buffer);

      Buffer[2] = BoardStr[8];
      Buffer[3] = BoardStr[9];
      Board_Address->BD_ADDR1 = (Byte_t)StringToUnsignedInteger(Buffer);

      Buffer[2] = BoardStr[10];
      Buffer[3] = BoardStr[11];
      Board_Address->BD_ADDR0 = (Byte_t)StringToUnsignedInteger(Buffer);
   }
   else
   {
      if(Board_Address)
         BTPS_MemInitialize(Board_Address, 0, sizeof(BD_ADDR_t));
   }
}

   /* Displays the current I/O Capabilities.                            */
static void DisplayIOCapabilities(void)
{
   Display(("I/O Capabilities: %s, MITM: %s.\r\n", IOCapabilitiesStrings[(unsigned int)(LE_Parameters.IOCapability - licDisplayOnly)], LE_Parameters.MITMProtection?"TRUE":"FALSE"));
}

   /* Utility function to display advertising data.                     */
static void DisplayAdvertisingData(GAP_LE_Advertising_Data_t *Advertising_Data)
{
   unsigned int Index;
   unsigned int Index2;

   /* Verify that the input parameters seem semi-valid.                 */
   if(Advertising_Data)
   {
      for(Index = 0; Index < Advertising_Data->Number_Data_Entries; Index++)
      {
         Display(("  AD Type: 0x%02X.\r\n", Advertising_Data->Data_Entries[Index].AD_Type));
         Display(("  AD Length: 0x%02X.\r\n", Advertising_Data->Data_Entries[Index].AD_Data_Length));
         if(Advertising_Data->Data_Entries[Index].AD_Data_Buffer)
         {
            Display(("  AD Data: "));
            for(Index2 = 0; Index2 < Advertising_Data->Data_Entries[Index].AD_Data_Length; Index2++)
            {
               Display(("0x%02X ", Advertising_Data->Data_Entries[Index].AD_Data_Buffer[Index2]));
            }
            Display(("\r\n"));
         }
      }
   }
}

   /* The following function displays the pairing capabilities that is  */
   /* passed into this function.                                        */
static void DisplayPairingInformation(GAP_LE_Pairing_Capabilities_t Pairing_Capabilities)
{
   /* Display the IO Capability.                                        */
   switch(Pairing_Capabilities.IO_Capability)
   {
      case licDisplayOnly:
         Display(("   IO Capability:       lcDisplayOnly.\r\n"));
         break;
      case licDisplayYesNo:
         Display(("   IO Capability:       lcDisplayYesNo.\r\n"));
         break;
      case licKeyboardOnly:
         Display(("   IO Capability:       lcKeyboardOnly.\r\n"));
         break;
      case licNoInputNoOutput:
         Display(("   IO Capability:       lcNoInputNoOutput.\r\n"));
         break;
      case licKeyboardDisplay:
         Display(("   IO Capability:       lcKeyboardDisplay.\r\n"));
         break;
   }

   Display(("   MITM:                %s.\r\n", (Pairing_Capabilities.MITM == TRUE)?"TRUE":"FALSE"));
   Display(("   Bonding Type:        %s.\r\n", (Pairing_Capabilities.Bonding_Type == lbtBonding)?"Bonding":"No Bonding"));
   Display(("   OOB:                 %s.\r\n", (Pairing_Capabilities.OOB_Present == TRUE)?"OOB":"OOB Not Present"));
   Display(("   Encryption Key Size: %d.\r\n", Pairing_Capabilities.Maximum_Encryption_Key_Size));
   Display(("   Sending Keys: \r\n"));
   Display(("      LTK:              %s.\r\n", ((Pairing_Capabilities.Sending_Keys.Encryption_Key == TRUE)?"YES":"NO")));
   Display(("      IRK:              %s.\r\n", ((Pairing_Capabilities.Sending_Keys.Identification_Key == TRUE)?"YES":"NO")));
   Display(("      CSRK:             %s.\r\n", ((Pairing_Capabilities.Sending_Keys.Signing_Key == TRUE)?"YES":"NO")));
   Display(("   Receiving Keys: \r\n"));
   Display(("      LTK:              %s.\r\n", ((Pairing_Capabilities.Receiving_Keys.Encryption_Key == TRUE)?"YES":"NO")));
   Display(("      IRK:              %s.\r\n", ((Pairing_Capabilities.Receiving_Keys.Identification_Key == TRUE)?"YES":"NO")));
   Display(("      CSRK:             %s.\r\n", ((Pairing_Capabilities.Receiving_Keys.Signing_Key == TRUE)?"YES":"NO")));
}

   /* The following function is provided to properly print a UUID.      */
static void DisplayUUID(GATT_UUID_t *UUID)
{
   if(UUID)
   {
      if(UUID->UUID_Type == guUUID_16)
         Display(("%02X%02X", UUID->UUID.UUID_16.UUID_Byte1, UUID->UUID.UUID_16.UUID_Byte0));
      else
      {
         if(UUID->UUID_Type == guUUID_128)
         {
            Display(("%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X", UUID->UUID.UUID_128.UUID_Byte15, UUID->UUID.UUID_128.UUID_Byte14, UUID->UUID.UUID_128.UUID_Byte13,
                                                                                         UUID->UUID.UUID_128.UUID_Byte12, UUID->UUID.UUID_128.UUID_Byte11, UUID->UUID.UUID_128.UUID_Byte10,
                                                                                         UUID->UUID.UUID_128.UUID_Byte9,  UUID->UUID.UUID_128.UUID_Byte8,  UUID->UUID.UUID_128.UUID_Byte7,
                                                                                         UUID->UUID.UUID_128.UUID_Byte6,  UUID->UUID.UUID_128.UUID_Byte5,  UUID->UUID.UUID_128.UUID_Byte4,
                                                                                         UUID->UUID.UUID_128.UUID_Byte3,  UUID->UUID.UUID_128.UUID_Byte2,  UUID->UUID.UUID_128.UUID_Byte1,
                                                                                         UUID->UUID.UUID_128.UUID_Byte0));
         }
      }
   }

   Display((".\r\n"));
}

   /* Displays the correct prompt depending on the Server/Client Mode.  */
static void DisplayPrompt(void)
{
   Display(("\r\nLE>"));
}

   /* The following thread is responsible for checking changing the     */
   /* current Baud Rate used to talk to the Radio.                      */
   /* * NOTE * This function ONLY configures the Baud Rate for a TI     */
   /*          Bluetooth chipset.                                       */
static int SetBaudRate(ParameterList_t *TempParam)
{
   int                              ret_val;

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

   /* Displays a usage string..                                         */
static void DisplayUsage(char *UsageString)
{
   Display(("Usage: %s.\r\n",UsageString));
}

   /* Displays a function error message.                                */
static void DisplayFunctionError(char *Function,int Status)
{
   Display(("%s Failed: %d.\r\n", Function, Status));
}

   /* Displays a function success message.                              */
static void DisplayFunctionSuccess(char *Function)
{
   Display(("%s success.\r\n",Function));
}

   /* The following Toggles an LED at a passed in blink rate.           */
static void *ToggleLED(void *UserParameter)
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

   /* The following function is responsible for opening the SS1         */
   /* Bluetooth Protocol Stack.  This function accepts a pre-populated  */
   /* HCI Driver Information structure that contains the HCI Driver     */
   /* Transport Information.  This function returns zero on successful  */
   /* execution and a negative value on all errors.                     */
static int OpenStack(HCI_DriverInformation_t *HCI_DriverInformation)
{
   int                        Result;
   int                        ret_val = 0;
   char                       BluetoothAddress[16];
   Byte_t                     Status;
   BD_ADDR_t                  BD_ADDR;
   unsigned int               ServiceID;
   HCI_Version_t              HCIVersion;
   L2CA_Link_Connect_Params_t L2CA_Link_Connect_Params;
   BTPS_Initialization_t      BTPS_Initialization;

   /* First check to see if the Stack has already been opened.          */
   if(!BluetoothStackID)
   {
      /* Next, makes sure that the Driver Information passed appears to */
      /* be semi-valid.                                                 */
      if(HCI_DriverInformation)
      {
         Display(("\r\n"));

         /* Initialize BTPSKNRL.                                        */
         BTPS_Initialization.MessageOutputCallback = HAL_ConsoleWrite;
         BTPS_Init((void *)&BTPS_Initialization);

         Display(("OpenStack().\r\n"));

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
            Display(("Bluetooth Stack ID: %d.\r\n", BluetoothStackID));

            /* Initialize the Default Pairing Parameters.               */
            LE_Parameters.IOCapability   = DEFAULT_IO_CAPABILITY;
            LE_Parameters.OOBDataPresent = FALSE;
            LE_Parameters.MITMProtection = DEFAULT_MITM_PROTECTION;

            if(!HCI_Version_Supported(BluetoothStackID, &HCIVersion))
               Display(("Device Chipset: %s.\r\n", (HCIVersion <= NUM_SUPPORTED_HCI_VERSIONS)?HCIVersionStrings[HCIVersion]:HCIVersionStrings[NUM_SUPPORTED_HCI_VERSIONS]));

            /* Let's output the Bluetooth Device Address so that the    */
            /* user knows what the Device Address is.                   */
            if(!GAP_Query_Local_BD_ADDR(BluetoothStackID, &BD_ADDR))
            {
               BD_ADDRToStr(BD_ADDR, BluetoothAddress);

               Display(("BD_ADDR: %s\r\n", BluetoothAddress));
            }

            /* Go ahead and allow Master/Slave Role Switch.             */
            L2CA_Link_Connect_Params.L2CA_Link_Connect_Request_Config  = cqAllowRoleSwitch;
            L2CA_Link_Connect_Params.L2CA_Link_Connect_Response_Config = csMaintainCurrentRole;

            L2CA_Set_Link_Connection_Configuration(BluetoothStackID, &L2CA_Link_Connect_Params);

            if(HCI_Command_Supported(BluetoothStackID, HCI_SUPPORTED_COMMAND_WRITE_DEFAULT_LINK_POLICY_BIT_NUMBER) > 0)
               HCI_Write_Default_Link_Policy_Settings(BluetoothStackID, (HCI_LINK_POLICY_SETTINGS_ENABLE_MASTER_SLAVE_SWITCH|HCI_LINK_POLICY_SETTINGS_ENABLE_SNIFF_MODE), &Status);

            /* Flag that no connection is currently active.             */
            ASSIGN_BD_ADDR(ConnectionBD_ADDR, 0, 0, 0, 0, 0, 0);
            ASSIGN_BD_ADDR(CurrentRemoteBD_ADDR, 0, 0, 0, 0, 0, 0);
            LocalDeviceIsMaster = FALSE;

            /* Regenerate IRK and DHK from the constant Identity Root   */
            /* Key.                                                     */
            GAP_LE_Diversify_Function(BluetoothStackID, (Encryption_Key_t *)(&IR), 1, 0, &IRK);
            GAP_LE_Diversify_Function(BluetoothStackID, (Encryption_Key_t *)(&IR), 3, 0, &DHK);

            /* Flag that we have no Key Information in the Key List.    */
            DeviceInfoList = NULL;

            /* Initialize the GATT Service.                             */
            if((Result = GATT_Initialize(BluetoothStackID, GATT_INITIALIZATION_FLAGS_SUPPORT_LE, GATT_Connection_Event_Callback, 0)) == 0)
            {
               /* Initialize the GAPS Service.                          */
               Result = GAPS_Initialize_Service(BluetoothStackID, &ServiceID);
               if(Result > 0)
               {
                  /* Save the Instance ID of the GAP Service.           */
                  GAPSInstanceID = (unsigned int)Result;

                  /* Set the GAP Device Name and Device Appearance.     */
                  GAPS_Set_Device_Name(BluetoothStackID, GAPSInstanceID, LE_DEMO_DEVICE_NAME);
                  GAPS_Set_Device_Appearance(BluetoothStackID, GAPSInstanceID, GAP_DEVICE_APPEARENCE_VALUE_GENERIC_COMPUTER);

                  /* Return success to the caller.                      */
                  ret_val        = 0;
               }
               else
               {
                  /* The Stack was NOT initialized successfully, inform */
                  /* the user and set the return value of the           */
                  /* initialization function to an error.               */
                  DisplayFunctionError("GAPS_Initialize_Service", Result);

                  /* Un-registered SPP LE Service.                      */
                  GATT_Un_Register_Service(BluetoothStackID, SPPLEServiceID);

                  /* Cleanup GATT Module.                               */
                  GATT_Cleanup(BluetoothStackID);

                  BluetoothStackID = 0;

                  ret_val          = UNABLE_TO_INITIALIZE_STACK;
               }
            }
            else
            {
               /* The Stack was NOT initialized successfully, inform the*/
               /* user and set the return value of the initialization   */
               /* function to an error.                                 */
               DisplayFunctionError("GATT_Initialize", Result);

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
      /* Cleanup GAP Service Module.                                    */
      if(GAPSInstanceID)
         GAPS_Cleanup_Service(BluetoothStackID, GAPSInstanceID);

      /* Un-registered SPP LE Service.                                  */
      if(SPPLEServiceID)
         GATT_Un_Register_Service(BluetoothStackID, SPPLEServiceID);

      /* Cleanup GATT Module.                                           */
      GATT_Cleanup(BluetoothStackID);

      /* Simply close the Stack                                         */
      BSC_Shutdown(BluetoothStackID);

      /* Free BTPSKRNL allocated memory.                                */
      BTPS_DeInit();

      Display(("Stack Shutdown.\r\n"));

      /* Free the Key List.                                             */
      FreeDeviceInfoList(&DeviceInfoList);

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
      /* * NOTE * Discoverability is only applicable when we are        */
      /*          advertising so save the default Discoverability Mode  */
      /*          for later.                                            */
      LE_Parameters.DiscoverabilityMode = dmGeneralDiscoverableMode;
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
      /* * NOTE * Connectability is only an applicable when advertising */
      /*          so we will just save the default connectability for   */
      /*          the next time we enable advertising.                  */
      LE_Parameters.ConnectableMode = lcmConnectable;
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
      Result = GAP_LE_Set_Pairability_Mode(BluetoothStackID, lpmPairableMode);

      /* Next, check the return value of the GAP Set Pairability mode   */
      /* command for successful execution.                              */
      if(!Result)
      {
         /* The device has been set to pairable mode, now register an   */
         /* Authentication Callback to handle the Authentication events */
         /* if required.                                                */
         Result = GAP_LE_Register_Remote_Authentication(BluetoothStackID, GAP_LE_Event_Callback, (unsigned long)0);

         /* Next, check the return value of the GAP Register Remote     */
         /* Authentication command for successful execution.            */
         if(Result)
         {
            /* An error occurred while trying to execute this function. */
            DisplayFunctionError("GAP_LE_Register_Remote_Authentication", Result);

            ret_val = Result;
         }
      }
      else
      {
         /* An error occurred while trying to make the device pairable. */
         DisplayFunctionError("GAP_LE_Set_Pairability_Mode", Result);

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

   /* The following function is a utility function that is used to dump */
   /* the Appearance to String Mapping Table.                           */
static void DumpAppearanceMappings(void)
{
   unsigned int Index;

   for(Index=0;Index<NUMBER_OF_APPEARANCE_MAPPINGS;++Index)
      Display(("   %u = %s.\r\n", Index, AppearanceMappings[Index].String));
}

   /* The following function is used to map a Appearance Value to it's  */
   /* string representation.  This function returns TRUE on success or  */
   /* FALSE otherwise.                                                  */
static Boolean_t AppearanceToString(Word_t Appearance, char **String)
{
   Boolean_t    ret_val;
   unsigned int Index;

   /* Verify that the input parameters are semi-valid.                  */
   if(String)
   {
      for(Index=0,ret_val=FALSE;Index<NUMBER_OF_APPEARANCE_MAPPINGS;++Index)
      {
         if(AppearanceMappings[Index].Appearance == Appearance)
         {
            *String = AppearanceMappings[Index].String;
            ret_val = TRUE;
            break;
         }
      }
   }
   else
      ret_val = FALSE;

   return(ret_val);
}

   /* The following function is used to map an Index into the Appearance*/
   /* Mapping table to it's Appearance Value.  This function returns    */
   /* TRUE on success or FALSE otherwise.                               */
static Boolean_t AppearanceIndexToAppearance(unsigned int Index, Word_t *Appearance)
{
   Boolean_t ret_val;

   if((Index < NUMBER_OF_APPEARANCE_MAPPINGS) && (Appearance))
   {
      *Appearance = AppearanceMappings[Index].Appearance;
      ret_val     = TRUE;
   }
   else
      ret_val = FALSE;

   return(ret_val);
}

   /* The following function is a utility function that provides a      */
   /* mechanism of populating discovered GAP Service Handles.           */
static void GAPSPopulateHandles(GAPS_Client_Info_t *ClientInfo, GATT_Service_Discovery_Indication_Data_t *ServiceInfo)
{
   unsigned int                       Index1;
   GATT_Characteristic_Information_t *CurrentCharacteristic;

   /* Verify that the input parameters are semi-valid.                  */
   if((ClientInfo) && (ServiceInfo) && (ServiceInfo->ServiceInformation.UUID.UUID_Type == guUUID_16) && (GAP_COMPARE_GAP_SERVICE_UUID_TO_UUID_16(ServiceInfo->ServiceInformation.UUID.UUID.UUID_16)))
   {
      /* Loop through all characteristics discovered in the service     */
      /* and populate the correct entry.                                */
      CurrentCharacteristic = ServiceInfo->CharacteristicInformationList;
      if(CurrentCharacteristic)
      {
         for(Index1=0;Index1<ServiceInfo->NumberOfCharacteristics;Index1++,CurrentCharacteristic++)
         {
            /* All GAP Service UUIDs are defined to be 16 bit UUIDs.    */
            if(CurrentCharacteristic->Characteristic_UUID.UUID_Type == guUUID_16)
            {
               /* Determine which characteristic this is.               */
               if(!GAP_COMPARE_GAP_DEVICE_NAME_UUID_TO_UUID_16(CurrentCharacteristic->Characteristic_UUID.UUID.UUID_16))
               {
                  if(!GAP_COMPARE_GAP_DEVICE_APPEARANCE_UUID_TO_UUID_16(CurrentCharacteristic->Characteristic_UUID.UUID.UUID_16))
                     continue;
                  else
                  {
                     ClientInfo->DeviceAppearanceHandle = CurrentCharacteristic->Characteristic_Handle;
                     continue;
                  }
               }
               else
               {
                  ClientInfo->DeviceNameHandle = CurrentCharacteristic->Characteristic_Handle;
                  continue;
               }
            }
         }
      }
   }
}

   /* The following function is a utility function that provides a      */
   /* mechanism of populating a BRSM Client Information structure with  */
   /* the information discovered from a GATT Discovery operation.       */
static void SPPLEPopulateHandles(SPPLE_Client_Info_t *ClientInfo, GATT_Service_Discovery_Indication_Data_t *ServiceInfo)
{
   Word_t                                       *ClientConfigurationHandle;
   unsigned int                                  Index1;
   unsigned int                                  Index2;
   GATT_Characteristic_Information_t            *CurrentCharacteristic;
   GATT_Characteristic_Descriptor_Information_t *CurrentDescriptor;

   /* Verify that the input parameters are semi-valid.                  */
   if((ClientInfo) && (ServiceInfo) && (ServiceInfo->ServiceInformation.UUID.UUID_Type == guUUID_128) && (SPPLE_COMPARE_SPPLE_SERVICE_UUID_TO_UUID_128(ServiceInfo->ServiceInformation.UUID.UUID.UUID_128)))
   {
      /* Loop through all characteristics discovered in the service     */
      /* and populate the correct entry.                                */
      CurrentCharacteristic = ServiceInfo->CharacteristicInformationList;
      if(CurrentCharacteristic)
      {
         for(Index1=0;Index1<ServiceInfo->NumberOfCharacteristics;Index1++,CurrentCharacteristic++)
         {
            /* All SPPLE UUIDs are defined to be 128 bit UUIDs.         */
            if(CurrentCharacteristic->Characteristic_UUID.UUID_Type == guUUID_128)
            {
               ClientConfigurationHandle = NULL;

               /* Determine which characteristic this is.               */
               if(!SPPLE_COMPARE_SPPLE_TX_UUID_TO_UUID_128(CurrentCharacteristic->Characteristic_UUID.UUID.UUID_128))
               {
                  if(!SPPLE_COMPARE_SPPLE_TX_CREDITS_UUID_TO_UUID_128(CurrentCharacteristic->Characteristic_UUID.UUID.UUID_128))
                  {
                     if(!SPPLE_COMPARE_SPPLE_RX_UUID_TO_UUID_128(CurrentCharacteristic->Characteristic_UUID.UUID.UUID_128))
                     {
                        if(!SPPLE_COMPARE_SPPLE_RX_CREDITS_UUID_TO_UUID_128(CurrentCharacteristic->Characteristic_UUID.UUID.UUID_128))
                           continue;
                        else
                        {
                           ClientInfo->Rx_Credit_Characteristic = CurrentCharacteristic->Characteristic_Handle;
                           ClientConfigurationHandle            = &(ClientInfo->Rx_Credit_Client_Configuration_Descriptor);
                        }
                     }
                     else
                     {
                        ClientInfo->Rx_Characteristic = CurrentCharacteristic->Characteristic_Handle;
                        continue;
                     }
                  }
                  else
                  {
                     ClientInfo->Tx_Credit_Characteristic = CurrentCharacteristic->Characteristic_Handle;
                     continue;
                  }
               }
               else
               {
                  ClientInfo->Tx_Characteristic = CurrentCharacteristic->Characteristic_Handle;
                  ClientConfigurationHandle     = &(ClientInfo->Tx_Client_Configuration_Descriptor);
               }

               /* Loop through the Descriptor List.                     */
               CurrentDescriptor = CurrentCharacteristic->DescriptorList;
               if((CurrentDescriptor) && (ClientConfigurationHandle))
               {
                  for(Index2=0;Index2<CurrentCharacteristic->NumberOfDescriptors;Index2++,CurrentDescriptor++)
                  {
                     if(CurrentDescriptor->Characteristic_Descriptor_UUID.UUID_Type == guUUID_16)
                     {
                        if(GATT_COMPARE_CLIENT_CHARACTERISTIC_CONFIGURATION_ATTRIBUTE_TYPE_TO_BLUETOOTH_UUID_16(CurrentDescriptor->Characteristic_Descriptor_UUID.UUID.UUID_16))
                        {
                           *ClientConfigurationHandle = CurrentDescriptor->Characteristic_Descriptor_Handle;
                           break;
                        }
                     }
                  }
               }
            }
         }
      }
   }
}

   /* The following function is a utility function that is used to add  */
   /* data (using InIndex as the buffer index) from the buffer specified*/
   /* by the DataBuffer parameter.  The second and third parameters     */
   /* specified the length of the data to add and the pointer to the    */
   /* data to add to the buffer.  This function returns the actual      */
   /* number of bytes that were added to the buffer (or 0 if none were  */
   /* added).                                                           */
static unsigned int AddDataToBuffer(SPPLE_Data_Buffer_t *DataBuffer, unsigned int DataLength, Byte_t *Data)
{
   unsigned int BytesAdded = 0;
   unsigned int Count;

   /* Verify that the input parameters are valid.                       */
   if((DataBuffer) && (DataLength) && (Data))
   {
      /* Loop while we have data AND space in the buffer.               */
      while(DataLength)
      {
         /* Get the number of bytes that can be placed in the buffer    */
         /* until it wraps.                                             */
         Count = DataBuffer->BufferSize - DataBuffer->InIndex;

         /* Determine if the number of bytes free is less than the      */
         /* number of bytes till we wrap and choose the smaller of the  */
         /* numbers.                                                    */
         Count = (DataBuffer->BytesFree < Count)?DataBuffer->BytesFree:Count;

         /* Cap the Count that we add to buffer to the length of the    */
         /* data provided by the caller.                                */
         Count = (Count > DataLength)?DataLength:Count;

         if(Count)
         {
            /* Copy the data into the buffer.                           */
            BTPS_MemCopy(&DataBuffer->Buffer[DataBuffer->InIndex], Data, Count);

            /* Update the counts.                                       */
            DataBuffer->InIndex   += Count;
            DataBuffer->BytesFree -= Count;
            DataLength            -= Count;
            BytesAdded            += Count;
            Data                  += Count;

            /* Wrap the InIndex if necessary.                           */
            if(DataBuffer->InIndex >= DataBuffer->BufferSize)
               DataBuffer->InIndex = 0;
         }
         else
            break;
      }
   }

   return(BytesAdded);
}

   /* The following function is a utility function that is used to      */
   /* removed data (using OutIndex as the buffer index) from the buffer */
   /* specified by the DataBuffer parameter The second parameter        */
   /* specifies the length of the Buffer that is pointed to by the third*/
   /* parameter.  This function returns the actual number of bytes that */
   /* were removed from the DataBuffer (or 0 if none were added).       */
   /* * NOTE * Buffer is optional and if not specified up to            */
   /*          BufferLength bytes will be deleted from the Buffer.      */
static unsigned int RemoveDataFromBuffer(SPPLE_Data_Buffer_t *DataBuffer, unsigned int BufferLength, Byte_t *Buffer)
{
   unsigned int Count;
   unsigned int BytesRemoved = 0;
   unsigned int MaxRemove;

   /* Verify that the input parameters are valid.                       */
   if((DataBuffer) && (BufferLength))
   {
      /* Loop while we have data to remove and space in the buffer to   */
      /* place it.                                                      */
      while(BufferLength)
      {
         /* Determine the number of bytes that are present in the       */
         /* buffer.                                                     */
         Count = DataBuffer->BufferSize - DataBuffer->BytesFree;
         if(Count)
         {
            /* Calculate the maximum number of bytes that I can remove  */
            /* from the buffer before it wraps.                         */
            MaxRemove = DataBuffer->BufferSize - DataBuffer->OutIndex;

            /* Cap max we can remove at the BufferLength of the caller's*/
            /* buffer.                                                  */
            MaxRemove = (MaxRemove > BufferLength)?BufferLength:MaxRemove;

            /* Cap the number of bytes I will remove in this iteration  */
            /* at the maximum I can remove or the number of bytes that  */
            /* are in the buffer.                                       */
            Count = (Count > MaxRemove)?MaxRemove:Count;

            /* Copy the data into the caller's buffer (If specified).   */
            if(Buffer)
            {
               BTPS_MemCopy(Buffer, &DataBuffer->Buffer[DataBuffer->OutIndex], Count);
               Buffer += Count;
            }

            /* Update the counts.                                       */
            DataBuffer->OutIndex  += Count;
            DataBuffer->BytesFree += Count;
            BytesRemoved          += Count;
            BufferLength          -= Count;

            /* Wrap the OutIndex if necessary.                          */
            if(DataBuffer->OutIndex >= DataBuffer->BufferSize)
               DataBuffer->OutIndex = 0;
         }
         else
            break;
      }
   }

   return(BytesRemoved);
}

   /* The following function is used to initialize the specified buffer */
   /* to the defaults.                                                  */
static void InitializeBuffer(SPPLE_Data_Buffer_t *DataBuffer)
{
   /* Verify that the input parameters are valid.                       */
   if(DataBuffer)
   {
      DataBuffer->BufferSize = SPPLE_DATA_CREDITS;
      DataBuffer->BytesFree  = SPPLE_DATA_CREDITS;
      DataBuffer->InIndex    = 0;
      DataBuffer->OutIndex   = 0;
   }
}

   /* The following function is used to enable/disable notifications on */
   /* a specified handle.  This function returns the positive non-zero  */
   /* Transaction ID of the Write Request or a negative error code.     */
static int EnableDisableNotificationsIndications(Word_t ClientConfigurationHandle, Word_t ClientConfigurationValue, GATT_Client_Event_Callback_t ClientEventCallback)
{
   int              ret_val;
   NonAlignedWord_t Buffer;

   /* Verify the input parameters.                                      */
   if((BluetoothStackID) && (ConnectionID) && (ClientConfigurationHandle))
   {
      ASSIGN_HOST_WORD_TO_LITTLE_ENDIAN_UNALIGNED_WORD(&Buffer, ClientConfigurationValue);

      ret_val = GATT_Write_Request(BluetoothStackID, ConnectionID, ClientConfigurationHandle, sizeof(Buffer), &Buffer, ClientEventCallback, 0);
   }
   else
      ret_val = BTPS_ERROR_INVALID_PARAMETER;

   return(ret_val);
}

   /* The following function is a utility function that exists to fill  */
   /* the specified buffer with the DataStr that is used to send data.  */
   /* This function will fill from the CurrentBufferLength up to Max    */
   /* Length in Buffer.  CurrentBufferLength is used to return the total*/
   /* length of the buffer.  The first parameter specifies the          */
   /* DeviceInfo which is used to fill any remainder of the string so   */
   /* that there are no breaks in the pattern.  This function returns   */
   /* the number of bytes added to the transmit buffer of the specified */
   /* device.                                                           */
static unsigned int FillBufferWithString(SPPLE_Data_Buffer_t *DataBuffer, unsigned *CurrentBufferLength, unsigned int MaxLength, Byte_t *Buffer)
{
   unsigned int DataCount;
   unsigned int Length;
   unsigned int Added2Buffer = 0;

   /* Verify that the input parameter is semi-valid.                    */
   if((DataBuffer) && (CurrentBufferLength) && (MaxLength) && (Buffer))
   {
      /* Copy as much of the DataStr into the Transmit buffer as is     */
      /* possible.                                                      */
      while(*CurrentBufferLength < MaxLength)
      {
         /* Cap the data to copy at the maximum of the string length and*/
         /* the remaining amount that can be placed in the buffer.      */
         DataCount = (DataStrLen > (MaxLength-*CurrentBufferLength))?(MaxLength-*CurrentBufferLength):DataStrLen;

         /* Build the data string into the SPPLEBuffer.                 */
         BTPS_MemCopy(&Buffer[*CurrentBufferLength], DataStr, DataCount);

         /* Increment the index.                                        */
         *CurrentBufferLength += DataCount;

         /* Add whatever bytes remaining in the DataStr into the        */
         /* transmit buffer to keep the pattern consistent.             */
         Length = DataStrLen-DataCount;
         if(Length)
         {
            /* Add the bytes remaining in the string.                   */
            Added2Buffer += AddDataToBuffer(DataBuffer, Length, (Byte_t *)&DataStr[DataCount]);
         }
      }
   }

   return(Added2Buffer);
}

   /* The following function is responsible for handling a Send Process.*/
static void SendProcess(DeviceInfo_t *DeviceInfo)
{
   int          Result;
   Boolean_t    Done = FALSE;
   unsigned int TransmitIndex;
   unsigned int DataCount;
   unsigned int MaxLength;
   unsigned int SPPLEBufferLength;
   unsigned int Added2Buffer;

   /* Verify that the input parameter is semi-valid.                    */
   if(DeviceInfo)
   {
      /* Loop while we have data to send and we have not used up all    */
      /* Transmit Credits.                                              */
      TransmitIndex     = 0;
      SPPLEBufferLength = 0;
      Added2Buffer      = 0;
      while((SendInfo.BytesToSend) && (DeviceInfo->TransmitCredits) && (!Done))
      {
         /* Get the maximum length of what we can send in this          */
         /* transaction.                                                */
         MaxLength = (SendInfo.BytesToSend > DeviceInfo->TransmitCredits)?DeviceInfo->TransmitCredits:SendInfo.BytesToSend;
         MaxLength = (MaxLength > SPPLE_DATA_BUFFER_LENGTH)?SPPLE_DATA_BUFFER_LENGTH:MaxLength;

         /* If we do not have any outstanding data get some more data.  */
         if(!SPPLEBufferLength)
         {
            /* Send any buffered data first.                            */
            if(DeviceInfo->TransmitBuffer.BytesFree != DeviceInfo->TransmitBuffer.BufferSize)
            {
               /* Remove the queued data from the Transmit Buffer.      */
               SPPLEBufferLength = RemoveDataFromBuffer(&(DeviceInfo->TransmitBuffer), MaxLength, SPPLEBuffer);

               /* If we added some data to the transmit buffer decrement*/
               /* what we just removed.                                 */
               if(Added2Buffer)
                  Added2Buffer -= SPPLEBufferLength;
            }

            /* Fill up the rest of the buffer with the data string.     */
            Added2Buffer     += FillBufferWithString(&(DeviceInfo->TransmitBuffer), &SPPLEBufferLength, MaxLength, SPPLEBuffer);

            /* Set the count of data that we can send.                  */
            DataCount         = SPPLEBufferLength;

            /* Reset the Transmit Index to 0.                           */
            TransmitIndex     = 0;
         }
         else
         {
            /* Move the data that to the beginning of the buffer.       */
            BTPS_MemMove(SPPLEBuffer, &SPPLEBuffer[TransmitIndex], SPPLEBufferLength);

            /* Send any buffered data first.                            */
            if(DeviceInfo->TransmitBuffer.BytesFree != DeviceInfo->TransmitBuffer.BufferSize)
            {
               /* Remove the queued data from the Transmit Buffer.      */
               TransmitIndex = RemoveDataFromBuffer(&(DeviceInfo->TransmitBuffer), MaxLength-SPPLEBufferLength, &SPPLEBuffer[SPPLEBufferLength]);

               /* If we added some data to the transmit buffer decrement*/
               /* what we just removed.                                 */
               if(Added2Buffer)
                  Added2Buffer -= TransmitIndex;

               /* Increment the buffer length.                          */
               SPPLEBufferLength += TransmitIndex;
            }

            /* Reset the Transmit Index to 0.                           */
            TransmitIndex     = 0;

            /* Fill up the rest of the buffer with the data string.     */
            Added2Buffer += FillBufferWithString(&(DeviceInfo->TransmitBuffer), &SPPLEBufferLength, MaxLength, SPPLEBuffer);

            /* We have data to send so cap it at the maximum that can be*/
            /* transmitted.                                             */
            DataCount     = (SPPLEBufferLength > MaxLength)?MaxLength:SPPLEBufferLength;
         }

         /* Use the correct API based on device role for SPPLE.         */
         if(DeviceInfo->Flags & DEVICE_INFO_FLAGS_SPPLE_SERVER)
         {
            /* We are acting as SPPLE Server, so notify the Tx          */
            /* Characteristic.                                          */
            if(DeviceInfo->ServerInfo.Tx_Client_Configuration_Descriptor == GATT_CLIENT_CONFIGURATION_CHARACTERISTIC_NOTIFY_ENABLE)
               Result = GATT_Handle_Value_Notification(BluetoothStackID, SPPLEServiceID, ConnectionID, SPPLE_TX_CHARACTERISTIC_ATTRIBUTE_OFFSET, (Word_t)DataCount, SPPLEBuffer);
            else
            {
               /* Not configured for notifications so exit the loop.    */
               Done = TRUE;
            }
         }
         else
         {
            /* We are acting as SPPLE Client, so write to the Rx        */
            /* Characteristic.                                          */
            if(DeviceInfo->ClientInfo.Tx_Characteristic)
               Result = GATT_Write_Without_Response_Request(BluetoothStackID, ConnectionID, DeviceInfo->ClientInfo.Rx_Characteristic, (Word_t)DataCount, SPPLEBuffer);
            else
            {
               /* We have not discovered the Tx Characteristic, so exit */
               /* the loop.                                             */
               Done = TRUE;
            }
         }

         /* Check to see if any data was written.                       */
         if(!Done)
         {
            /* Check to see if the data was written successfully.       */
            if(Result >= 0)
            {
               /* Adjust the counters.                                  */
               SendInfo.BytesToSend        -= (unsigned int)Result;
               SendInfo.BytesSent          += (unsigned int)Result;
               TransmitIndex               += (unsigned int)Result;
               SPPLEBufferLength           -= (unsigned int)Result;
               DeviceInfo->TransmitCredits -= (unsigned int)Result;

               /* If we have no more remaining Tx Credits AND we have   */
               /* data built up to send, we need to queue this in the Tx*/
               /* Buffer.                                               */
               if((!(DeviceInfo->TransmitCredits)) && (SPPLEBufferLength))
               {
                  /* Add the remaining data to the transmit buffer.     */
                  AddDataToBuffer(&(DeviceInfo->TransmitBuffer), SPPLEBufferLength, &SPPLEBuffer[TransmitIndex]);

                  SPPLEBufferLength = 0;
               }
            }
            else
            {
               Display(("SEND failed with error %d\r\n", Result));

               SendInfo.BytesToSend  = 0;
            }
         }
      }

      /* If we have added more bytes to the transmit buffer than we can */
      /* send in this process remove the extra.                         */
      if(Added2Buffer > SendInfo.BytesToSend)
         RemoveDataFromBuffer(&(DeviceInfo->TransmitBuffer), Added2Buffer-SendInfo.BytesToSend, NULL);

      /* Display a message if we have sent all required data.           */
      if((!SendInfo.BytesToSend) && (SendInfo.BytesSent))
      {
         Display(("\r\nSend Complete, Sent %u.\r\n", SendInfo.BytesSent));
         DisplayPrompt();

         SendInfo.BytesSent = 0;
      }
   }
}

   /* The following function is responsible for transmitting the        */
   /* specified number of credits to the remote device.                 */
static void SendCredits(DeviceInfo_t *DeviceInfo, unsigned int DataLength)
{
   NonAlignedWord_t Credits;

   /* Verify that the input parameters are semi-valid.                  */
   if((DeviceInfo) && (DataLength))
   {
      /* Format the credit packet.                                      */
      ASSIGN_HOST_WORD_TO_LITTLE_ENDIAN_UNALIGNED_WORD(&Credits, DataLength);

      /* Determine how to send credits based on the role.               */
      if(DeviceInfo->Flags & DEVICE_INFO_FLAGS_SPPLE_SERVER)
      {
         /* We are acting as a server so notify the Rx Credits          */
         /* characteristic.                                             */
         if(DeviceInfo->ServerInfo.Rx_Credit_Client_Configuration_Descriptor == GATT_CLIENT_CONFIGURATION_CHARACTERISTIC_NOTIFY_ENABLE)
            GATT_Handle_Value_Notification(BluetoothStackID, SPPLEServiceID, ConnectionID, SPPLE_RX_CREDITS_CHARACTERISTIC_ATTRIBUTE_OFFSET, WORD_SIZE, (Byte_t *)&Credits);
      }
      else
      {
         /* We are acting as a client so send a Write Without Response  */
         /* packet to the Tx Credit Characteristic.                     */
         if(DeviceInfo->ClientInfo.Tx_Credit_Characteristic)
            GATT_Write_Without_Response_Request(BluetoothStackID, ConnectionID, DeviceInfo->ClientInfo.Tx_Credit_Characteristic, WORD_SIZE, &Credits);
      }
   }
}

   /* The following function is responsible for handling a received     */
   /* credit, event.                                                    */
static void ReceiveCreditEvent(DeviceInfo_t *DeviceInfo, unsigned int Credits)
{
   /* Verify that the input parameters are semi-valid.                  */
   if(DeviceInfo)
   {
      /* If this is a real credit event store the number of credits.    */
      DeviceInfo->TransmitCredits += Credits;

      /* Handle any active send process.                                */
      SendProcess(DeviceInfo);

      /* Send all queued data.                                          */
      SendData(DeviceInfo, 0, NULL);

      /* It is possible that we have received data queued, so call the  */
      /* Data Indication Event to handle this.                          */
      DataIndicationEvent(DeviceInfo, 0, NULL);
   }
}

   /* The following function sends the specified data to the specified  */
   /* data.  This function will queue any of the data that does not go  */
   /* out.  This function returns TRUE if all the data was sent, or     */
   /* FALSE.                                                            */
   /* * NOTE * If DataLength is 0 and Data is NULL then all queued data */
   /*          will be sent.                                            */
static Boolean_t SendData(DeviceInfo_t *DeviceInfo, unsigned int DataLength, Byte_t *Data)
{
   int          Result;
   Boolean_t    DataSent = FALSE;
   Boolean_t    Done;
   unsigned int DataCount;
   unsigned int MaxLength;
   unsigned int TransmitIndex;
   unsigned int SPPLEBufferLength;

   /* Verify that the input parameters are semi-valid.                  */
   if(DeviceInfo)
   {
      /* Loop while we have data to send and we can send it.            */
      Done              = FALSE;
      TransmitIndex     = 0;
      SPPLEBufferLength = 0;
      while(!Done)
      {
         /* Check to see if we have credits to use to transmit the data.*/
         if(DeviceInfo->TransmitCredits)
         {
            /* Get the maximum length of what we can send in this       */
            /* transaction.                                             */
            MaxLength = (SPPLE_DATA_BUFFER_LENGTH > DeviceInfo->TransmitCredits)?DeviceInfo->TransmitCredits:SPPLE_DATA_BUFFER_LENGTH;

            /* If we do not have any outstanding data get some more     */
            /* data.                                                    */
            if(!SPPLEBufferLength)
            {
               /* Send any buffered data first.                         */
               if(DeviceInfo->TransmitBuffer.BytesFree != DeviceInfo->TransmitBuffer.BufferSize)
               {
                  /* Remove the queued data from the Transmit Buffer.   */
                  SPPLEBufferLength = RemoveDataFromBuffer(&(DeviceInfo->TransmitBuffer), MaxLength, SPPLEBuffer);
               }
               else
               {
                  /* Check to see if we have data to send.              */
                  if((DataLength) && (Data))
                  {
                     /* Copy the data to send into the SPPLEBuffer.     */
                     SPPLEBufferLength = (DataLength > MaxLength)?MaxLength:DataLength;
                     BTPS_MemCopy(SPPLEBuffer, Data, SPPLEBufferLength);

                     DataLength -= SPPLEBufferLength;
                     Data       += SPPLEBufferLength;
                  }
                  else
                  {
                     /* No data queued or data left to send so exit the */
                     /* loop.                                           */
                     Done = TRUE;
                  }
               }

               /* Set the count of data that we can send.               */
               DataCount         = SPPLEBufferLength;

               /* Reset the Transmit Index to 0.                        */
               TransmitIndex     = 0;
            }
            else
            {
               /* We have data to send so cap it at the maximum that can*/
               /* be transmitted.                                       */
               DataCount = (SPPLEBufferLength > MaxLength)?MaxLength:SPPLEBufferLength;
            }

            /* Try to write data if not exiting the loop.               */
            if(!Done)
            {
               /* Use the correct API based on device role for SPPLE.   */
               if(DeviceInfo->Flags & DEVICE_INFO_FLAGS_SPPLE_SERVER)
               {
                  /* We are acting as SPPLE Server, so notify the Tx    */
                  /* Characteristic.                                    */
                  if(DeviceInfo->ServerInfo.Tx_Client_Configuration_Descriptor == GATT_CLIENT_CONFIGURATION_CHARACTERISTIC_NOTIFY_ENABLE)
                     Result = GATT_Handle_Value_Notification(BluetoothStackID, SPPLEServiceID, ConnectionID, SPPLE_TX_CHARACTERISTIC_ATTRIBUTE_OFFSET, (Word_t)DataCount, &SPPLEBuffer[TransmitIndex]);
                  else
                  {
                     /* Not configured for notifications so exit the    */
                     /* loop.                                           */
                     Done = TRUE;
                  }
               }
               else
               {
                  /* We are acting as SPPLE Client, so write to the Rx  */
                  /* Characteristic.                                    */
                  if(DeviceInfo->ClientInfo.Tx_Characteristic)
                     Result = GATT_Write_Without_Response_Request(BluetoothStackID, ConnectionID, DeviceInfo->ClientInfo.Rx_Characteristic, (Word_t)DataCount, &SPPLEBuffer[TransmitIndex]);
                  else
                  {
                     /* We have not discovered the Tx Characteristic, so*/
                     /* exit the loop.                                  */
                     Done = TRUE;
                  }
               }

               /* Check to see if any data was written.                 */
               if(!Done)
               {
                  /* Check to see if the data was written successfully. */
                  if(Result >= 0)
                  {
                     /* Adjust the counters.                            */
                     TransmitIndex               += (unsigned int)Result;
                     SPPLEBufferLength           -= (unsigned int)Result;
                     DeviceInfo->TransmitCredits -= (unsigned int)Result;

                     /* Flag that data was sent.                        */
                     DataSent                     = TRUE;

                     /* If we have no more remaining Tx Credits AND we  */
                     /* have data built up to send, we need to queue    */
                     /* this in the Tx Buffer.                          */
                     if((!(DeviceInfo->TransmitCredits)) && (SPPLEBufferLength))
                     {
                        /* Add the remaining data to the transmit       */
                        /* buffer.                                      */
                        AddDataToBuffer(&(DeviceInfo->TransmitBuffer), SPPLEBufferLength, &SPPLEBuffer[TransmitIndex]);

                        SPPLEBufferLength = 0;
                     }
                  }
                  else
                  {
                     Display(("SEND failed with error %d\r\n", Result));

                     DataSent  = FALSE;
                  }
               }
            }
         }
         else
         {
            /* We have no transmit credits, so buffer the data.         */
            DataCount = AddDataToBuffer(&(DeviceInfo->TransmitBuffer), DataLength, Data);
            if(DataCount == DataLength)
               DataSent = TRUE;
            else
               DataSent = FALSE;

            /* Exit the loop.                                           */
            Done = TRUE;
         }
      }
   }

   return(DataSent);
}

   /* The following function is responsible for handling a data         */
   /* indication event.                                                 */
static void DataIndicationEvent(DeviceInfo_t *DeviceInfo, unsigned int DataLength, Byte_t *Data)
{
   Boolean_t    Done;
   unsigned int ReadLength;
   unsigned int Length;

   /* Verify that the input parameters are semi-valid.                  */
   if(DeviceInfo)
   {
      /* If we are automatically reading the data, go ahead and credit  */
      /* what we just received, as well as reading everything in the    */
      /* buffer.                                                        */
      if((AutomaticReadActive) || (LoopbackActive))
      {
         /* Loop until we read all of the data queued.                  */
         Done = FALSE;
         while(!Done)
         {
            /* If in loopback mode cap what we remove at the max of what*/
            /* we can send or queue.                                    */
            if(LoopbackActive)
               ReadLength = (SPPLE_DATA_BUFFER_LENGTH > (DeviceInfo->TransmitCredits + DeviceInfo->TransmitBuffer.BytesFree))?(DeviceInfo->TransmitCredits + DeviceInfo->TransmitBuffer.BytesFree):SPPLE_DATA_BUFFER_LENGTH;
            else
               ReadLength = SPPLE_DATA_BUFFER_LENGTH;

            /* Read all queued data.                                    */
            Length = ReadData(DeviceInfo, ReadLength, SPPLEBuffer);
            if(Length > 0)
            {
               /* If loopback is active, loopback the data.             */
               if(LoopbackActive)
                  SendData(DeviceInfo, Length, SPPLEBuffer);

               /* If we are displaying the data then do that here.      */
               if(DisplayRawData)
               {
                  SPPLEBuffer[Length] = '\0';
                  Display(("%s", SPPLEBuffer));
               }
            }
            else
               Done = TRUE;
         }

         /* Only send/display data just received if any is specified in */
         /* the call to this function.                                  */
         if((DataLength) && (Data))
         {
            /* If loopback is active, loopback the data just received.  */
            if((AutomaticReadActive) || (LoopbackActive))
            {
               /* If we are displaying the data then do that here.      */
               if(DisplayRawData)
               {
                  BTPS_MemCopy(SPPLEBuffer, Data, DataLength);
                  SPPLEBuffer[DataLength] = '\0';
                  Display(("%s", SPPLEBuffer));
               }

               /* Check to see if Loopback is active, if it is we will  */
               /* loopback the data we just received.                   */
               if(LoopbackActive)
               {
                  /* Only queue the data in the receive buffer that we  */
                  /* cannot send.                                       */
                  ReadLength = (DataLength > (DeviceInfo->TransmitCredits + DeviceInfo->TransmitBuffer.BytesFree))?(DeviceInfo->TransmitCredits + DeviceInfo->TransmitBuffer.BytesFree):DataLength;

                  /* Send the data.                                     */
                  if(SendData(DeviceInfo, ReadLength, Data))
                  {
                     /* Credit the data we just sent.                   */
                     SendCredits(DeviceInfo, ReadLength);

                     /* Increment what was just sent.                   */
                     DataLength -= ReadLength;
                     Data       += ReadLength;
                  }
               }
               else
               {
                  /* Loopback is not active so just credit back the data*/
                  /* we just received.                                  */
                  SendCredits(DeviceInfo, DataLength);

                  DataLength = 0;
               }

               /* If we have data left that cannot be sent, queue this  */
               /* in the receive buffer.                                */
               if((DataLength) && (Data))
               {
                  /* We are not in Loopback or Automatic Read Mode so   */
                  /* just buffer all the data.                          */
                  Length = AddDataToBuffer(&(DeviceInfo->ReceiveBuffer), DataLength, Data);
                  if(Length != DataLength)
                     Display(("Receive Buffer Overflow of %u bytes", DataLength - Length));
               }
            }

            /* If we are displaying the data then do that here.         */
            if(DisplayRawData)
            {
               BTPS_MemCopy(SPPLEBuffer, Data, DataLength);
               SPPLEBuffer[DataLength] = '\0';
               Display(("%s", SPPLEBuffer));
            }
         }
      }
      else
      {
         if((DataLength) && (Data))
         {
            /* Display a Data indication event.                         */
            Display(("\r\nData Indication Event, Connection ID %u, Received %u bytes.\r\n", ConnectionID, DataLength));

            /* We are not in Loopback or Automatic Read Mode so just    */
            /* buffer all the data.                                     */
            Length = AddDataToBuffer(&(DeviceInfo->ReceiveBuffer), DataLength, Data);
            if(Length != DataLength)
               Display(("Receive Buffer Overflow of %u bytes.\r\n", DataLength - Length));
         }
      }
   }
}

   /* The following function is used to read data from the specified    */
   /* device.  The final two parameters specify the BufferLength and the*/
   /* Buffer to read the data into.  On success this function returns   */
   /* the number of bytes read.  If an error occurs this will return a  */
   /* negative error code.                                              */
static int ReadData(DeviceInfo_t *DeviceInfo, unsigned int BufferLength, Byte_t *Buffer)
{
   int          ret_val;
   Boolean_t    Done;
   unsigned int Length;
   unsigned int TotalLength;

   /* Verify that the input parameters are semi-valid.                  */
   if((DeviceInfo) && (BufferLength) && (Buffer))
   {
      Done        = FALSE;
      TotalLength = 0;
      while(!Done)
      {
         Length = RemoveDataFromBuffer(&(DeviceInfo->ReceiveBuffer), BufferLength, Buffer);
         if(Length > 0)
         {
            BufferLength -= Length;
            Buffer       += Length;
            TotalLength   = Length;
         }
         else
            Done = TRUE;
      }

      /* Credit what we read.                                           */
      SendCredits(DeviceInfo, TotalLength);

      /* Return the total number of bytes read.                         */
      ret_val = (int)TotalLength;
   }
   else
      ret_val = BTPS_ERROR_INVALID_PARAMETER;

   return(ret_val);
}

   /* The following function is responsible for starting a scan.        */
static int StartScan(unsigned int BluetoothStackID)
{
   int Result;

   /* First, determine if the input parameters appear to be semi-valid. */
   if(BluetoothStackID)
   {
      /* Not currently scanning, go ahead and attempt to perform the    */
      /* scan.                                                          */
      Result = GAP_LE_Perform_Scan(BluetoothStackID, stActive, 10, 10, latPublic, fpNoFilter, TRUE, GAP_LE_Event_Callback, 0);

      if(!Result)
      {
         Display(("Scan started successfully.\r\n"));
      }
      else
      {
         /* Unable to start the scan.                                   */
         Display(("Unable to perform scan: %d\r\n", Result));
      }
   }
   else
      Result = -1;

   return(Result);
}

   /* The following function is responsible for stopping on on-going    */
   /* scan.                                                             */
static int StopScan(unsigned int BluetoothStackID)
{
   int Result;

   /* First, determine if the input parameters appear to be semi-valid. */
   if(BluetoothStackID)
   {
      Result = GAP_LE_Cancel_Scan(BluetoothStackID);
      if(!Result)
      {
         Display(("Scan stopped successfully.\r\n"));
      }
      else
      {
         /* Error stopping scan.                                        */
         Display(("Unable to stop scan: %d\r\n", Result));
      }
   }
   else
      Result = -1;

   return(Result);
}

   /* The following function is responsible for creating an LE          */
   /* connection to the specified Remote Device.                        */
static int ConnectLEDevice(unsigned int BluetoothStackID, BD_ADDR_t BD_ADDR, Boolean_t UseWhiteList)
{
   int                            Result;
   unsigned int                   WhiteListChanged;
   GAP_LE_White_List_Entry_t      WhiteListEntry;
   GAP_LE_Connection_Parameters_t ConnectionParameters;

   /* First, determine if the input parameters appear to be semi-valid. */
   if((BluetoothStackID) && (!COMPARE_NULL_BD_ADDR(BD_ADDR)))
   {
      if(COMPARE_NULL_BD_ADDR(ConnectionBD_ADDR))
      {
         /* Remove any previous entries for this device from the White  */
         /* List.                                                       */
         WhiteListEntry.Address_Type = latPublic;
         WhiteListEntry.Address      = BD_ADDR;

         GAP_LE_Remove_Device_From_White_List(BluetoothStackID, 1, &WhiteListEntry, &WhiteListChanged);

         if(UseWhiteList)
            Result = GAP_LE_Add_Device_To_White_List(BluetoothStackID, 1, &WhiteListEntry, &WhiteListChanged);
         else
            Result = 1;

         /* If everything has been successful, up until this point, then*/
         /* go ahead and attempt the connection.                        */
         if(Result >= 0)
         {
            /* Initialize the connection parameters.                    */
            ConnectionParameters.Connection_Interval_Min    = 50;
            ConnectionParameters.Connection_Interval_Max    = 200;
            ConnectionParameters.Minimum_Connection_Length  = 0;
            ConnectionParameters.Maximum_Connection_Length  = 10000;
            ConnectionParameters.Slave_Latency              = 0;
            ConnectionParameters.Supervision_Timeout        = 20000;

            /* Everything appears correct, go ahead and attempt to make */
            /* the connection.                                          */
            Result = GAP_LE_Create_Connection(BluetoothStackID, 100, 100, Result?fpNoFilter:fpWhiteList, latPublic, Result?&BD_ADDR:NULL, latPublic, &ConnectionParameters, GAP_LE_Event_Callback, 0);

            if(!Result)
            {
               Display(("Connection Request successful.\r\n"));

               /* Note the connection information.                      */
               ConnectionBD_ADDR = BD_ADDR;
            }
            else
            {
               /* Unable to create connection.                          */
               Display(("Unable to create connection: %d.\r\n", Result));
            }
         }
         else
         {
            /* Unable to add device to White List.                      */
            Display(("Unable to add device to White List.\r\n"));
         }
      }
      else
      {
         /* Device already connected.                                   */
         Display(("Device is already connected.\r\n"));

         Result = -2;
      }
   }
   else
      Result = -1;

   return(Result);
}

   /* The following function is provided to allow a mechanism to        */
   /* disconnect a currently connected device.                          */
static int DisconnectLEDevice(unsigned int BluetoothStackID, BD_ADDR_t BD_ADDR)
{
   int Result;

   /* First, determine if the input parameters appear to be semi-valid. */
   if((BluetoothStackID) && (!COMPARE_NULL_BD_ADDR(BD_ADDR)))
   {
      if(!COMPARE_NULL_BD_ADDR(ConnectionBD_ADDR))
      {
         Result = GAP_LE_Disconnect(BluetoothStackID, BD_ADDR);

         if(!Result)
         {
            Display(("Disconnect Request successful.\r\n"));
         }
         else
         {
            /* Unable to disconnect device.                             */
            Display(("Unable to disconnect device: %d.\r\n", Result));
         }
      }
      else
      {
         /* Device not connected.                                       */
         Display(("Device is not connected.\r\n"));

         Result = 0;
      }
   }
   else
      Result = -1;

   return(Result);
}

   /* The following function provides a mechanism to configure a        */
   /* Pairing Capabilities structure with the application's pairing     */
   /* parameters.                                                       */
static void ConfigureCapabilities(GAP_LE_Pairing_Capabilities_t *Capabilities)
{
   /* Make sure the Capabilities pointer is semi-valid.                 */
   if(Capabilities)
   {
      /* Configure the Pairing Capabilities structure.                  */
      Capabilities->Bonding_Type                    = lbtBonding;
      Capabilities->IO_Capability                   = LE_Parameters.IOCapability;
      Capabilities->MITM                            = LE_Parameters.MITMProtection;
      Capabilities->OOB_Present                     = LE_Parameters.OOBDataPresent;

      /* ** NOTE ** This application always requests that we use the    */
      /*            maximum encryption because this feature is not a    */
      /*            very good one, if we set less than the maximum we   */
      /*            will internally in GAP generate a key of the        */
      /*            maximum size (we have to do it this way) and then   */
      /*            we will zero out how ever many of the MSBs          */
      /*            necessary to get the maximum size.  Also as a slave */
      /*            we will have to use Non-Volatile Memory (per device */
      /*            we are paired to) to store the negotiated Key Size. */
      /*            By requesting the maximum (and by not storing the   */
      /*            negotiated key size if less than the maximum) we    */
      /*            allow the slave to power cycle and regenerate the   */
      /*            LTK for each device it is paired to WITHOUT storing */
      /*            any information on the individual devices we are    */
      /*            paired to.                                          */
      Capabilities->Maximum_Encryption_Key_Size        = GAP_LE_MAXIMUM_ENCRYPTION_KEY_SIZE;

      /* This application only demonstrates using Long Term Key's (LTK) */
      /* for encryption of a LE Link, however we could request and send */
      /* all possible keys here if we wanted to.                        */
      Capabilities->Receiving_Keys.Encryption_Key     = TRUE;
      Capabilities->Receiving_Keys.Identification_Key = FALSE;
      Capabilities->Receiving_Keys.Signing_Key        = FALSE;

      Capabilities->Sending_Keys.Encryption_Key       = TRUE;
      Capabilities->Sending_Keys.Identification_Key   = FALSE;
      Capabilities->Sending_Keys.Signing_Key          = FALSE;
   }
}

   /* The following function provides a mechanism for sending a pairing */
   /* request to a device that is connected on an LE Link.              */
static int SendPairingRequest(BD_ADDR_t BD_ADDR, Boolean_t ConnectionMaster)
{
   int                           ret_val;
   BoardStr_t                    BoardStr;
   GAP_LE_Pairing_Capabilities_t Capabilities;

   /* Make sure a Bluetooth Stack is open.                              */
   if(BluetoothStackID)
   {
      /* Make sure the BD_ADDR is valid.                                */
      if(!COMPARE_NULL_BD_ADDR(BD_ADDR))
      {
         /* Configure the application pairing parameters.               */
         ConfigureCapabilities(&Capabilities);

         /* Set the BD_ADDR of the device that we are attempting to pair*/
         /* with.                                                       */
         CurrentRemoteBD_ADDR = BD_ADDR;

         BD_ADDRToStr(BD_ADDR, BoardStr);
         Display(("Attempting to Pair to %s.\r\n", BoardStr));

         /* Attempt to pair to the remote device.                       */
         if(ConnectionMaster)
         {
            /* Start the pairing process.                               */
            ret_val = GAP_LE_Pair_Remote_Device(BluetoothStackID, BD_ADDR, &Capabilities, GAP_LE_Event_Callback, 0);

            Display(("     GAP_LE_Pair_Remote_Device returned %d.\r\n", ret_val));
         }
         else
         {
            /* As a slave we can only request that the Master start     */
            /* the pairing process.                                     */
            ret_val = GAP_LE_Request_Security(BluetoothStackID, BD_ADDR, Capabilities.Bonding_Type, Capabilities.MITM, GAP_LE_Event_Callback, 0);

            Display(("     GAP_LE_Request_Security returned %d.\r\n", ret_val));
         }
      }
      else
      {
         Display(("Invalid Parameters.\r\n"));

         ret_val = INVALID_PARAMETERS_ERROR;
      }
   }
   else
   {
      Display(("Stack ID Invalid.\r\n"));

      ret_val = INVALID_STACK_ID_ERROR;
   }

   return(ret_val);
}

   /* The following function provides a mechanism of sending a Slave    */
   /* Pairing Response to a Master's Pairing Request.                   */
static int SlavePairingRequestResponse(BD_ADDR_t BD_ADDR)
{
   int                                          ret_val;
   BoardStr_t                                   BoardStr;
   GAP_LE_Authentication_Response_Information_t AuthenticationResponseData;

   /* Make sure a Bluetooth Stack is open.                              */
   if(BluetoothStackID)
   {
      BD_ADDRToStr(BD_ADDR, BoardStr);
      Display(("Sending Pairing Response to %s.\r\n", BoardStr));

      /* We must be the slave if we have received a Pairing Request     */
      /* thus we will respond with our capabilities.                    */
      AuthenticationResponseData.GAP_LE_Authentication_Type = larPairingCapabilities;
      AuthenticationResponseData.Authentication_Data_Length = GAP_LE_PAIRING_CAPABILITIES_SIZE;

      /* Configure the Application Pairing Parameters.                  */
      ConfigureCapabilities(&(AuthenticationResponseData.Authentication_Data.Pairing_Capabilities));

      /* Attempt to pair to the remote device.                          */
      ret_val = GAP_LE_Authentication_Response(BluetoothStackID, BD_ADDR, &AuthenticationResponseData);

      Display(("GAP_LE_Authentication_Response returned %d.\r\n", ret_val));
   }
   else
   {
      Display(("Stack ID Invalid.\r\n"));

      ret_val = INVALID_STACK_ID_ERROR;
   }

   return(ret_val);
}

   /* The following function is provided to allow a mechanism of        */
   /* responding to a request for Encryption Information to send to a   */
   /* remote device.                                                    */
static int EncryptionInformationRequestResponse(BD_ADDR_t BD_ADDR, Byte_t KeySize, GAP_LE_Authentication_Response_Information_t *GAP_LE_Authentication_Response_Information)
{
   int    ret_val;
   Word_t LocalDiv;

   /* Make sure a Bluetooth Stack is open.                              */
   if(BluetoothStackID)
   {
      /* Make sure the input parameters are semi-valid.                 */
      if((!COMPARE_NULL_BD_ADDR(BD_ADDR)) && (GAP_LE_Authentication_Response_Information))
      {
         Display(("   Calling GAP_LE_Generate_Long_Term_Key.\r\n"));

         /* Generate a new LTK, EDIV and Rand tuple.                    */
         ret_val = GAP_LE_Generate_Long_Term_Key(BluetoothStackID, (Encryption_Key_t *)(&DHK), (Encryption_Key_t *)(&ER), &(GAP_LE_Authentication_Response_Information->Authentication_Data.Encryption_Information.LTK), &LocalDiv, &(GAP_LE_Authentication_Response_Information->Authentication_Data.Encryption_Information.EDIV), &(GAP_LE_Authentication_Response_Information->Authentication_Data.Encryption_Information.Rand));
         if(!ret_val)
         {
            Display(("   Encryption Information Request Response.\r\n"));

            /* Response to the request with the LTK, EDIV and Rand      */
            /* values.                                                  */
            GAP_LE_Authentication_Response_Information->GAP_LE_Authentication_Type                                     = larEncryptionInformation;
            GAP_LE_Authentication_Response_Information->Authentication_Data_Length                                     = GAP_LE_ENCRYPTION_INFORMATION_DATA_SIZE;
            GAP_LE_Authentication_Response_Information->Authentication_Data.Encryption_Information.Encryption_Key_Size = KeySize;

            ret_val = GAP_LE_Authentication_Response(BluetoothStackID, BD_ADDR, GAP_LE_Authentication_Response_Information);
            if(!ret_val)
            {
               Display(("   GAP_LE_Authentication_Response (larEncryptionInformation) success.\r\n", ret_val));
            }
            else
            {
               Display(("   Error - SM_Generate_Long_Term_Key returned %d.\r\n", ret_val));
            }
         }
         else
         {
            Display(("   Error - SM_Generate_Long_Term_Key returned %d.\r\n", ret_val));
         }
      }
      else
      {
         Display(("Invalid Parameters.\r\n"));

         ret_val = INVALID_PARAMETERS_ERROR;
      }
   }
   else
   {
      Display(("Stack ID Invalid.\r\n"));

      ret_val = INVALID_STACK_ID_ERROR;
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
         if(TempParam->Params->intParam)
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

   /* The following function is responsible for displaying the current  */
   /* Command Options for either Serial Port Client or Serial Port      */
   /* Server.  The input parameter to this function is completely       */
   /* ignored, and only needs to be passed in because all Commands that */
   /* can be entered at the Prompt pass in the parsed information.  This*/
   /* function displays the current Command Options that are available  */
   /* and always returns zero.                                          */
static int DisplayHelp(ParameterList_t *TempParam)
{
   Display(("\r\n"));
   Display(("******************************************************************\r\n"));
   Display(("* Command Options General: Help, GetLocalAddress, SetBaudRate    *\r\n"));
   Display(("*                          QueryMemory, CheckStacks              *\r\n"));
   Display(("* Command Options GAPLE:   SetDiscoverabilityMode,               *\r\n"));
   Display(("*                          SetConnectabilityMode,                *\r\n"));
   Display(("*                          SetPairabilityMode,                   *\r\n"));
   Display(("*                          ChangePairingParameters,              *\r\n"));
   Display(("*                          AdvertiseLE, StartScanning,           *\r\n"));
   Display(("*                          StopScanning, ConnectLE,              *\r\n"));
   Display(("*                          DisconnectLE, PairLE,                 *\r\n"));
   Display(("*                          LEPasskeyResponse,                    *\r\n"));
   Display(("*                          QueryEncryptionMode, SetPasskey,      *\r\n"));
   Display(("*                          DiscoverGAPS, GetLocalName,           *\r\n"));
   Display(("*                          SetLocalName, GetRemoteName,          *\r\n"));
   Display(("*                          SetLocalAppearance,                   *\r\n"));
   Display(("*                          GetLocalAppearance,                   *\r\n"));
   Display(("*                          GetRemoteAppearance,                  *\r\n"));
   Display(("* Command Options SPPLE:   DiscoverSPPLE, RegisterSPPLE, Send,   *\r\n"));
   Display(("*                          ConfigureSPPLE, Read, Loopback,       *\r\n"));
   Display(("*                          DisplayRawModeData, AutomaticReadMode *\r\n"));
   Display(("******************************************************************\r\n"));
   return(0);
}

   /* The following function is responsible for setting the             */
   /* Discoverability Mode of the local device.  This function returns  */
   /* zero on successful execution and a negative value on all errors.  */
static int SetDiscoverabilityMode(ParameterList_t *TempParam)
{
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

         /* Set the LE Discoveryability Mode.                           */
         LE_Parameters.DiscoverabilityMode = DiscoverabilityMode;

         /* The Mode was changed successfully.                          */
         Display(("Discoverability: %s.\r\n", (DiscoverabilityMode == dmNonDiscoverableMode)?"Non":((DiscoverabilityMode == dmGeneralDiscoverableMode)?"General":"Limited")));

         /* Flag success to the caller.                                 */
         ret_val = 0;
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
   int ret_val;

   /* First, check that valid Bluetooth Stack ID exists.                */
   if(BluetoothStackID)
   {
      /* Make sure that all of the parameters required for this function*/
      /* appear to be at least semi-valid.                              */
      if((TempParam) && (TempParam->NumberofParameters >= 1) && (TempParam->Params[0].intParam >= 0) && (TempParam->Params[0].intParam <= 1))
      {
         /* Parameters appear to be valid, map the specified parameters */
         /* into the API specific parameters.                           */
         /* * NOTE * The Connectability Mode in LE is only applicable   */
         /*          when advertising, if a device is not advertising   */
         /*          it is not connectable.                             */
         if(TempParam->Params[1].intParam == 0)
            LE_Parameters.ConnectableMode = lcmNonConnectable;
         else
            LE_Parameters.ConnectableMode = lcmConnectable;

         /* The Mode was changed successfully.                          */
         Display(("Connectability Mode: %s.\r\n", (LE_Parameters.ConnectableMode == lcmNonConnectable)?"Non Connectable":"Connectable"));

         /* Flag success to the caller.                                 */
         ret_val = 0;
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
   int                        Result;
   int                        ret_val;
   char                      *Mode;
   GAP_LE_Pairability_Mode_t  PairabilityMode;

   /* First, check that valid Bluetooth Stack ID exists.                */
   if(BluetoothStackID)
   {
      /* Make sure that all of the parameters required for this function*/
      /* appear to be at least semi-valid.                              */
      if((TempParam) && (TempParam->NumberofParameters > 0) && (TempParam->Params[0].intParam >= 0) && (TempParam->Params[0].intParam <= 1))
      {
         /* Parameters appear to be valid, map the specified parameters */
         /* into the API specific parameters.                           */
         PairabilityMode = lpmNonPairableMode;
         Mode            = "lpmNonPairableMode";

         if(TempParam->Params[0].intParam == 1)
         {
            PairabilityMode = lpmPairableMode;
            Mode            = "lpmPairableMode";
         }

         /* Parameters mapped, now set the Pairability Mode.            */
         Result = GAP_LE_Set_Pairability_Mode(BluetoothStackID, PairabilityMode);

         /* Next, check the return value to see if the command was      */
         /* issued successfully.                                        */
         if(Result >= 0)
         {
            /* The Mode was changed successfully.                       */
            Display(("Pairability Mode Changed to %s.\r\n", Mode));

            /* If Secure Simple Pairing has been enabled, inform the    */
            /* user of the current Secure Simple Pairing parameters.    */
            if(PairabilityMode == lpmPairableMode)
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
         DisplayUsage("SetPairabilityMode [Mode (0 = Non Pairable, 1 = Pairable]");

         ret_val = INVALID_PARAMETERS_ERROR;
      }
   }
   else
   {
      Display(("Invalid Stack ID.\r\n"));

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
static int ChangePairingParameters(ParameterList_t *TempParam)
{
   int ret_val;

   /* First, check that valid Bluetooth Stack ID exists.                */
   if(BluetoothStackID)
   {
      /* Make sure that all of the parameters required for this function*/
      /* appear to be at least semi-valid.                              */
      if((TempParam) && (TempParam->Params[0].intParam >= 0) && (TempParam->Params[0].intParam <= 4))
      {
         /* Parameters appear to be valid, map the specified parameters */
         /* into the API specific parameters.                           */
         if(TempParam->Params[0].intParam == 0)
            LE_Parameters.IOCapability = licDisplayOnly;
         else
         {
            if(TempParam->Params[0].intParam == 1)
               LE_Parameters.IOCapability = licDisplayYesNo;
            else
            {
               if(TempParam->Params[0].intParam == 2)
                  LE_Parameters.IOCapability = licKeyboardOnly;
               else
               {
                  if(TempParam->Params[0].intParam == 3)
                     LE_Parameters.IOCapability = licNoInputNoOutput;
                  else
                     LE_Parameters.IOCapability = licKeyboardDisplay;
               }
            }
         }

         /* Finally map the Man in the Middle (MITM) Protection valid.  */
         LE_Parameters.MITMProtection = (Boolean_t)(TempParam->Params[1].intParam?TRUE:FALSE);

         /* Inform the user of the New I/O Capabilities.                */
         DisplayIOCapabilities();

         /* Flag success to the caller.                                 */
         ret_val = 0;
      }
      else
      {
         /* One or more of the necessary parameters is/are invalid.     */
         Display(("Usage: ChangePairingParameters [I/O Capability (0 = Display Only, 1 = Display Yes/No, 2 = Keyboard Only, 3 = No Input/Output, 4 = Keyboard/Display)] [MITM Requirement (0 = No, 1 = Yes)].\r\n"));

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
   /* Authentication Response with a Pass Key value specified via the   */
   /* input parameter.  This function returns zero on successful        */
   /* execution and a negative value on all errors.                     */
static int LEPassKeyResponse(ParameterList_t *TempParam)
{
   int                              Result;
   int                              ret_val;
   GAP_LE_Authentication_Response_Information_t  GAP_LE_Authentication_Response_Information;

   /* First, check that valid Bluetooth Stack ID exists.                */
   if(BluetoothStackID)
   {
      /* First, check to see if there is an on-going Pairing operation  */
      /* active.                                                        */
      if(!COMPARE_NULL_BD_ADDR(CurrentRemoteBD_ADDR))
      {
         /* Make sure that all of the parameters required for this      */
         /* function appear to be at least semi-valid.                  */
         if((TempParam) && (TempParam->NumberofParameters > 0) && (BTPS_StringLength(TempParam->Params[0].strParam) <= GAP_LE_PASSKEY_MAXIMUM_NUMBER_OF_DIGITS))
         {
            /* Parameters appear to be valid, go ahead and populate the */
            /* response structure.                                      */
            GAP_LE_Authentication_Response_Information.GAP_LE_Authentication_Type  = larPasskey;
            GAP_LE_Authentication_Response_Information.Authentication_Data_Length  = (Byte_t)(sizeof(DWord_t));
            GAP_LE_Authentication_Response_Information.Authentication_Data.Passkey = (DWord_t)(TempParam->Params[0].intParam);

            /* Submit the Authentication Response.                      */
            Result = GAP_LE_Authentication_Response(BluetoothStackID, CurrentRemoteBD_ADDR, &GAP_LE_Authentication_Response_Information);

            /* Check the return value for the submitted command for     */
            /* success.                                                 */
            if(!Result)
            {
               /* Operation was successful, inform the user.            */
               DisplayFunctionSuccess("Passkey Response");

               /* Flag success to the caller.                           */
               ret_val = 0;
            }
            else
            {
               /* Inform the user that the Authentication Response was  */
               /* not successful.                                       */
               DisplayFunctionError("GAP_LE_Authentication_Response", Result);

               ret_val = FUNCTION_ERROR;
            }

            /* Flag that there is no longer a current Authentication    */
            /* procedure in progress.                                   */
            ASSIGN_BD_ADDR(CurrentRemoteBD_ADDR, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
         }
         else
         {
            /* One or more of the necessary parameters is/are invalid.  */
            Display(("PassKeyResponse [Numeric Passkey(0 - 999999)].\r\n"));

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
   /* The following function is responsible for querying the Encryption */
   /* Mode for an LE Connection.  This function returns zero on         */
   /* successful execution and a negative value on all errors.          */
static int LEQueryEncryption(ParameterList_t *TempParam)
{
   int                   ret_val;
   GAP_Encryption_Mode_t GAP_Encryption_Mode;

   /* First, check that valid Bluetooth Stack ID exists.                */
   if(BluetoothStackID)
   {
      /* First, check to see if there is an on-going Pairing operation  */
      /* active.                                                        */
      if(!COMPARE_NULL_BD_ADDR(ConnectionBD_ADDR))
      {
         /* Query the current Encryption Mode for this Connection.      */
         ret_val = GAP_LE_Query_Encryption_Mode(BluetoothStackID, ConnectionBD_ADDR, &GAP_Encryption_Mode);
         if(!ret_val)
            Display(("Current Encryption Mode: %s.\r\n", (GAP_Encryption_Mode == emEnabled)?"Enabled":"Disabled"));
         else
         {
            Display(("Error - GAP_LE_Query_Encryption_Mode returned %d.\r\n", ret_val));

            ret_val = FUNCTION_ERROR;
         }
      }
      else
      {
         /* There is not currently an on-going authentication operation,*/
         /* inform the user of this error condition.                    */
         Display(("Not Connected.\r\n"));

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

   /* The following function is responsible for querying the Encryption */
   /* Mode for an LE Connection.  This function returns zero on         */
   /* successful execution and a negative value on all errors.          */
static int LESetPasskey(ParameterList_t *TempParam)
{
   int     ret_val;
   DWord_t Passkey;

   /* First, check that valid Bluetooth Stack ID exists.                */
   if(BluetoothStackID)
   {
      /* Make sure that all of the parameters required for this         */
      /* function appear to be at least semi-valid.                     */
      if((TempParam) && (TempParam->NumberofParameters >= 1) && ((TempParam->Params[0].intParam == 0) || (TempParam->Params[0].intParam == 1)))
      {
         if(TempParam->Params[0].intParam == 1)
         {
            /* We are setting the passkey so make sure it is valid.     */
            if(BTPS_StringLength(TempParam->Params[1].strParam) <= GAP_LE_PASSKEY_MAXIMUM_NUMBER_OF_DIGITS)
            {
               Passkey = (DWord_t)(TempParam->Params[1].intParam);

               ret_val = GAP_LE_Set_Fixed_Passkey(BluetoothStackID, &Passkey);
               if(!ret_val)
                  Display(("Fixed Passkey set to %06u.\r\n", Passkey));
            }
            else
            {
               Display(("Error - Invalid Passkey.\r\n"));

               ret_val = INVALID_PARAMETERS_ERROR;
            }
         }
         else
         {
            /* Un-set the fixed passkey that we previously configured.  */
            ret_val = GAP_LE_Set_Fixed_Passkey(BluetoothStackID, NULL);
            if(!ret_val)
               Display(("Fixed Passkey no longer configured.\r\n"));
         }

         /* If GAP_LE_Set_Fixed_Passkey returned an error display this. */
         if((ret_val) && (ret_val != INVALID_PARAMETERS_ERROR))
         {
            Display(("Error - GAP_LE_Set_Fixed_Passkey returned %d.\r\n", ret_val));

            ret_val = FUNCTION_ERROR;
         }
      }
      else
      {
         /* One or more of the necessary parameters is/are invalid.     */
         Display(("SetPasskey [(0 = UnSet Passkey, 1 = Set Fixed Passkey)] [6 Digit Passkey (optional)].\r\n"));

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
   /* Device Address of the local Bluetooth Device.  This function      */
   /* returns zero on successful execution and a negative value on all  */
   /* errors.                                                           */
static int GetLocalAddress(ParameterList_t *TempParam)
{
   int        Result;
   int        ret_val;
   BD_ADDR_t  BD_ADDR;
   BoardStr_t BoardStr;

   /* First, check that valid Bluetooth Stack ID exists.                */
   if(BluetoothStackID)
   {
      /* Attempt to submit the command.                                 */
      Result = GAP_Query_Local_BD_ADDR(BluetoothStackID, &BD_ADDR);

      /* Check the return value of the submitted command for success.   */
      if(!Result)
      {
         BD_ADDRToStr(BD_ADDR, BoardStr);

         Display(("BD_ADDR of Local Device is: %s.\r\n", BoardStr));

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

   /* The following function is responsible for enabling LE             */
   /* Advertisements.  This function returns zero on successful         */
   /* execution and a negative value on all errors.                     */
static int AdvertiseLE(ParameterList_t *TempParam)
{
   int                                 ret_val;
   int                                 Length;
   GAP_LE_Advertising_Parameters_t     AdvertisingParameters;
   GAP_LE_Connectability_Parameters_t  ConnectabilityParameters;
   union
   {
      Advertising_Data_t               AdvertisingData;
      Scan_Response_Data_t             ScanResponseData;
   } Advertisement_Data_Buffer;

   /* First, check that valid Bluetooth Stack ID exists.                */
   if(BluetoothStackID)
   {
      /* Make sure that all of the parameters required for this function*/
      /* appear to be at least semi-valid.                              */
      if((TempParam) && (TempParam->NumberofParameters > 0) && (TempParam->Params[0].intParam >= 0) && (TempParam->Params[0].intParam <= 1))
      {
         /* Determine whether to enable or disable Advertising.         */
         if(TempParam->Params[0].intParam == 0)
         {
            /* Disable Advertising.                                     */
            ret_val = GAP_LE_Advertising_Disable(BluetoothStackID);
            if(!ret_val)
               Display(("   GAP_LE_Advertising_Disable success.\r\n"));
            else
            {
               Display(("   GAP_LE_Advertising_Disable returned %d.\r\n", ret_val));

               ret_val = FUNCTION_ERROR;
            }
         }
         else
         {
            /* Enable Advertising.                                      */
            /* Set the Advertising Data.                                */
            BTPS_MemInitialize(&(Advertisement_Data_Buffer.AdvertisingData), 0, sizeof(Advertising_Data_t));

            /* Set the Flags A/D Field (1 byte type and 1 byte Flags.   */
            Advertisement_Data_Buffer.AdvertisingData.Advertising_Data[0] = 2;
            Advertisement_Data_Buffer.AdvertisingData.Advertising_Data[1] = HCI_LE_ADVERTISING_REPORT_DATA_TYPE_FLAGS;
            Advertisement_Data_Buffer.AdvertisingData.Advertising_Data[2] = 0;

            /* Configure the flags field based on the Discoverability   */
            /* Mode.                                                    */
            if(LE_Parameters.DiscoverabilityMode == dmGeneralDiscoverableMode)
               Advertisement_Data_Buffer.AdvertisingData.Advertising_Data[2] = HCI_LE_ADVERTISING_FLAGS_GENERAL_DISCOVERABLE_MODE_FLAGS_BIT_MASK;
            else
            {
               if(LE_Parameters.DiscoverabilityMode == dmLimitedDiscoverableMode)
                  Advertisement_Data_Buffer.AdvertisingData.Advertising_Data[2] = HCI_LE_ADVERTISING_FLAGS_LIMITED_DISCOVERABLE_MODE_FLAGS_BIT_MASK;
            }

            /* Write thee advertising data to the chip.                 */
            ret_val = GAP_LE_Set_Advertising_Data(BluetoothStackID, (Advertisement_Data_Buffer.AdvertisingData.Advertising_Data[0] + 1), &(Advertisement_Data_Buffer.AdvertisingData));
            if(!ret_val)
            {
               BTPS_MemInitialize(&(Advertisement_Data_Buffer.ScanResponseData), 0, sizeof(Scan_Response_Data_t));

               /* Set the Scan Response Data.                           */
               Length = BTPS_StringLength(LE_DEMO_DEVICE_NAME);
               if(Length < (ADVERTISING_DATA_MAXIMUM_SIZE - 2))
               {
                  Advertisement_Data_Buffer.ScanResponseData.Scan_Response_Data[1] = HCI_LE_ADVERTISING_REPORT_DATA_TYPE_LOCAL_NAME_COMPLETE;
               }
               else
               {
                  Advertisement_Data_Buffer.ScanResponseData.Scan_Response_Data[1] = HCI_LE_ADVERTISING_REPORT_DATA_TYPE_LOCAL_NAME_SHORTENED;
                  Length = (ADVERTISING_DATA_MAXIMUM_SIZE - 2);
               }

               Advertisement_Data_Buffer.ScanResponseData.Scan_Response_Data[0] = (Byte_t)(1 + Length);
               BTPS_MemCopy(&(Advertisement_Data_Buffer.ScanResponseData.Scan_Response_Data[2]),LE_DEMO_DEVICE_NAME,Length);

               ret_val = GAP_LE_Set_Scan_Response_Data(BluetoothStackID, (Advertisement_Data_Buffer.ScanResponseData.Scan_Response_Data[0] + 1), &(Advertisement_Data_Buffer.ScanResponseData));
               if(!ret_val)
               {
                  /* Set up the advertising parameters.                 */
                  AdvertisingParameters.Advertising_Channel_Map   = HCI_LE_ADVERTISING_CHANNEL_MAP_DEFAULT;
                  AdvertisingParameters.Scan_Request_Filter       = fpNoFilter;
                  AdvertisingParameters.Connect_Request_Filter    = fpNoFilter;
                  AdvertisingParameters.Advertising_Interval_Min  = 100;
                  AdvertisingParameters.Advertising_Interval_Max  = 200;

                  /* Configure the Connectability Parameters.           */
                  /* * NOTE * Since we do not ever put ourselves to be  */
                  /*          direct connectable then we will set the   */
                  /*          DirectAddress to all 0s.                  */
                  ConnectabilityParameters.Connectability_Mode   = LE_Parameters.ConnectableMode;
                  ConnectabilityParameters.Own_Address_Type      = latPublic;
                  ConnectabilityParameters.Direct_Address_Type   = latPublic;
                  ASSIGN_BD_ADDR(ConnectabilityParameters.Direct_Address, 0, 0, 0, 0, 0, 0);

                  /* Now enable advertising.                            */
                  ret_val = GAP_LE_Advertising_Enable(BluetoothStackID, TRUE, &AdvertisingParameters, &ConnectabilityParameters, GAP_LE_Event_Callback, 0);
                  if(!ret_val)
                  {
                     Display(("   GAP_LE_Advertising_Enable success.\r\n"));
                  }
                  else
                  {
                     Display(("   GAP_LE_Advertising_Enable returned %d.\r\n", ret_val));

                     ret_val = FUNCTION_ERROR;
                  }
               }
               else
               {
                  Display(("   GAP_LE_Set_Advertising_Data(dtScanResponse) returned %d.\r\n", ret_val));

                  ret_val = FUNCTION_ERROR;
               }

            }
            else
            {
               Display(("   GAP_LE_Set_Advertising_Data(dtAdvertising) returned %d.\r\n", ret_val));

               ret_val = FUNCTION_ERROR;
            }
         }
      }
      else
      {
         DisplayUsage("Advertise [(0 = Disable, 1 = Enable)].");

         ret_val = INVALID_PARAMETERS_ERROR;
      }
   }
   else
   {
      /* No valid Bluetooth Stack ID exists.                            */
      ret_val = INVALID_STACK_ID_ERROR;
   }

   return(0);
}


   /* The following function is responsible for starting an LE scan     */
   /* procedure.  This function returns zero if successful and a        */
   /* negative value if an error occurred.                              */
static int StartScanning(ParameterList_t *TempParam)
{
   int ret_val;

   /* First, check that valid Bluetooth Stack ID exists.                */
   if(BluetoothStackID)
   {
      /* Simply start scanning.                                         */
      if(!StartScan(BluetoothStackID))
         ret_val = 0;
      else
         ret_val = FUNCTION_ERROR;
   }
   else
   {
      /* No valid Bluetooth Stack ID exists.                            */
      ret_val = INVALID_STACK_ID_ERROR;
   }

   return(ret_val);
}

   /* The following function is responsible for stopping an LE scan     */
   /* procedure.  This function returns zero if successful and a        */
   /* negative value if an error occurred.                              */
static int StopScanning(ParameterList_t *TempParam)
{
   int ret_val;

   /* First, check that valid Bluetooth Stack ID exists.                */
   if(BluetoothStackID)
   {
      /* Simply stop scanning.                                          */
      if(!StopScan(BluetoothStackID))
         ret_val = 0;
      else
         ret_val = FUNCTION_ERROR;
   }
   else
   {
      /* No valid Bluetooth Stack ID exists.                            */
      ret_val = INVALID_STACK_ID_ERROR;
   }

   return(ret_val);
}

   /* The following function is responsible for connecting to an LE     */
   /* device.  This function returns zero if successful and a negative  */
   /* value if an error occurred.                                       */
static int ConnectLE(ParameterList_t *TempParam)
{
   int       ret_val;
   BD_ADDR_t BD_ADDR;

   /* First, check that valid Bluetooth Stack ID exists.                */
   if(BluetoothStackID)
   {
      /* Next, make sure that a valid device address exists.            */
      if((TempParam) && (TempParam->NumberofParameters > 0) && (TempParam->Params[0].strParam) && (BTPS_StringLength(TempParam->Params[0].strParam) == (sizeof(BD_ADDR_t)*2)))
      {
         /* Convert the parameter to a Bluetooth Device Address.        */
         StrToBD_ADDR(TempParam->Params[0].strParam, &BD_ADDR);

         if(!ConnectLEDevice(BluetoothStackID, BD_ADDR, FALSE))
            ret_val = 0;
         else
            ret_val = FUNCTION_ERROR;
      }
      else
      {
         /* Invalid parameters specified so flag an error to the user.  */
         Display(("Usage: Connect [BD_ADDR].\r\n"));

         /* Flag that an error occurred while submitting the command.   */
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

   /* The following function is responsible for disconnecting to an LE  */
   /* device.  This function returns zero if successful and a negative  */
   /* value if an error occurred.                                       */
static int DisconnectLE(ParameterList_t *TempParam)
{
   int ret_val;

   /* First, check that valid Bluetooth Stack ID exists.                */
   if(BluetoothStackID)
   {
      /* Next, check to make sure we are currently connected.           */
      if(!COMPARE_NULL_BD_ADDR(ConnectionBD_ADDR))
      {
         if(!DisconnectLEDevice(BluetoothStackID, ConnectionBD_ADDR))
            ret_val = 0;
         else
            ret_val = FUNCTION_ERROR;
      }
      else
      {
         Display(("Device is not connected.\r\n"));

         /* Flag success to the caller.                                 */
         ret_val = 0;
      }
   }
   else
   {
      /* No valid Bluetooth Stack ID exists.                            */
      ret_val = INVALID_STACK_ID_ERROR;
   }

   return(ret_val);
}

   /* The following function is provided to allow a mechanism of        */
   /* Pairing (or requesting security if a slave) to the connected      */
   /* device.                                                           */
static int PairLE(ParameterList_t *TempParam)
{
   int ret_val;

   /* First, check that valid Bluetooth Stack ID exists.                */
   if(BluetoothStackID)
   {
      /* Next, check to make sure we are currently connected.           */
      if(!COMPARE_NULL_BD_ADDR(ConnectionBD_ADDR))
      {
         if(!SendPairingRequest(ConnectionBD_ADDR, LocalDeviceIsMaster))
            ret_val = 0;
         else
            ret_val = FUNCTION_ERROR;
      }
      else
      {
         Display(("Device is not connected.\r\n"));

         /* Flag success to the caller.                                 */
         ret_val = 0;
      }
   }
   else
   {
      /* No valid Bluetooth Stack ID exists.                            */
      ret_val = INVALID_STACK_ID_ERROR;
   }

   return(ret_val);
}

   /* The following function is responsible for performing a GAP Service*/
   /* Service Discovery Operation.  This function will return zero on   */
   /* successful execution and a negative value on errors.              */
static int DiscoverGAPS(ParameterList_t *TempParam)
{
   int           ret_val;
   GATT_UUID_t   UUID[1];
   DeviceInfo_t *DeviceInfo;

   /* Verify that there is a connection that is established.            */
   if(ConnectionID)
   {
      /* Get the device info for the connection device.                 */
      if((DeviceInfo = SearchDeviceInfoEntryByBD_ADDR(&DeviceInfoList, ConnectionBD_ADDR)) != NULL)
      {
         /* Verify that no service discovery is outstanding for this    */
         /* device.                                                     */
         if(!(DeviceInfo->Flags & DEVICE_INFO_FLAGS_SERVICE_DISCOVERY_OUTSTANDING))
         {
            /* Configure the filter so that only the SPP LE Service is  */
            /* discovered.                                              */
            UUID[0].UUID_Type = guUUID_16;
            GAP_ASSIGN_GAP_SERVICE_UUID_16(UUID[0].UUID.UUID_16);

            /* Start the service discovery process.                     */
            ret_val = GATT_Start_Service_Discovery(BluetoothStackID, ConnectionID, (sizeof(UUID)/sizeof(GATT_UUID_t)), UUID, GATT_Service_Discovery_Event_Callback, 0);
            if(!ret_val)
            {
               /* Display success message.                              */
               Display(("GATT_Service_Discovery_Start success.\r\n"));

               /* Flag that a Service Discovery Operation is            */
               /* outstanding.                                          */
               DeviceInfo->Flags |= DEVICE_INFO_FLAGS_SERVICE_DISCOVERY_OUTSTANDING;
            }
            else
            {
               /* An error occur so just clean-up.                      */
               Display(("Error - GATT_Service_Discovery_Start returned %d.\r\n", ret_val));

               ret_val = FUNCTION_ERROR;
            }
         }
         else
         {
            Display(("Service Discovery Operation Outstanding for Device.\r\n"));

            ret_val = FUNCTION_ERROR;
         }
      }
      else
      {
         Display(("No Device Info.\r\n"));

         ret_val = FUNCTION_ERROR;
      }
   }
   else
   {
      Display(("No Connection Established\r\n"));

      ret_val = FUNCTION_ERROR;
   }

   return(ret_val);
}

   /* The following function is responsible for reading the current     */
   /* Local Device Name.  This function will return zero on successful  */
   /* execution and a negative value on errors.                         */
static int ReadLocalName(ParameterList_t *TempParam)
{
   int  ret_val;
   char NameBuffer[BTPS_CONFIGURATION_GAPS_MAXIMUM_SUPPORTED_DEVICE_NAME+1];

   /* Verify that the GAP Service is registered.                        */
   if(GAPSInstanceID)
   {
      /* Initialize the Name Buffer to all zeros.                       */
      BTPS_MemInitialize(NameBuffer, 0, sizeof(NameBuffer));

      /* Query the Local Name.                                          */
      ret_val = GAPS_Query_Device_Name(BluetoothStackID, GAPSInstanceID, NameBuffer);
      if(!ret_val)
         Display(("Device Name: %s.\r\n", NameBuffer));
      else
      {
         Display(("Error - GAPS_Query_Device_Name returned %d.\r\n", ret_val));

         ret_val = FUNCTION_ERROR;
      }
   }
   else
   {
      Display(("GAP Service not registered.\r\n"));

      ret_val = FUNCTION_ERROR;
   }

   return(ret_val);
}

   /* The following function is responsible for setting the current     */
   /* Local Device Name.  This function will return zero on successful  */
   /* execution and a negative value on errors.                         */
static int SetLocalName(ParameterList_t *TempParam)
{
   int  ret_val;

   /* Verify that the input parameters are semi-valid.                  */
   if((TempParam) && (TempParam->NumberofParameters > 0) && (BTPS_StringLength(TempParam->Params[0].strParam) > 0) && (BTPS_StringLength(TempParam->Params[0].strParam) <= BTPS_CONFIGURATION_GAPS_MAXIMUM_SUPPORTED_DEVICE_NAME))
   {
      /* Verify that the GAP Service is registered.                     */
      if(GAPSInstanceID)
      {
         /* Query the Local Name.                                       */
         ret_val = GAPS_Set_Device_Name(BluetoothStackID, GAPSInstanceID, TempParam->Params[0].strParam);
         if(!ret_val)
            Display(("GAPS_Set_Device_Name success.\r\n"));
         else
         {
            Display(("Error - GAPS_Query_Device_Name returned %d.\r\n", ret_val));

            ret_val = FUNCTION_ERROR;
         }
      }
      else
      {
         Display(("GAP Service not registered.\r\n"));

         ret_val = FUNCTION_ERROR;
      }
   }
   else
   {
      Display(("Usage: SetLocalName [NameString].\r\n"));

      ret_val = FUNCTION_ERROR;
   }

   return(ret_val);
}

   /* The following function is responsible for reading the Device Name */
   /* for the currently connected remote device.  This function will    */
   /* return zero on successful execution and a negative value on       */
   /* errors.                                                           */
static int ReadRemoteName(ParameterList_t *TempParam)
{
   int           ret_val;
   DeviceInfo_t *DeviceInfo;

   /* Verify that there is a connection that is established.            */
   if(ConnectionID)
   {
      /* Get the device info for the connection device.                 */
      if((DeviceInfo = SearchDeviceInfoEntryByBD_ADDR(&DeviceInfoList, ConnectionBD_ADDR)) != NULL)
      {
         /* Verify that we discovered the Device Name Handle.           */
         if(DeviceInfo->GAPSClientInfo.DeviceNameHandle)
         {
            /* Attempt to read the remote device name.                  */
            ret_val = GATT_Read_Value_Request(BluetoothStackID, ConnectionID, DeviceInfo->GAPSClientInfo.DeviceNameHandle, GATT_ClientEventCallback_GAPS, (unsigned long)DeviceInfo->GAPSClientInfo.DeviceNameHandle);
            if(ret_val > 0)
            {
               Display(("Attempting to read Remote Device Name.\r\n"));

               ret_val = 0;
            }
            else
            {
               Display(("Error - GATT_Read_Value_Request returned %d.\r\n", ret_val));

               ret_val = FUNCTION_ERROR;
            }
         }
         else
         {
            Display(("GAP Service Device Name Handle not discovered.\r\n"));

            ret_val = FUNCTION_ERROR;
         }
      }
      else
      {
         Display(("No Device Info.\r\n"));

         ret_val = FUNCTION_ERROR;
      }
   }
   else
   {
      Display(("No Connection Established\r\n"));

      ret_val = FUNCTION_ERROR;
   }

   return(ret_val);
}

   /* The following function is responsible for reading the Local Device*/
   /* Appearance value.  This function will return zero on successful   */
   /* execution and a negative value on errors.                         */
static int ReadLocalAppearance(ParameterList_t *TempParam)
{
   int     ret_val;
   char   *AppearanceString;
   Word_t  Appearance;

   /* Verify that the GAP Service is registered.                        */
   if(GAPSInstanceID)
   {
      /* Query the Local Name.                                          */
      ret_val = GAPS_Query_Device_Appearance(BluetoothStackID, GAPSInstanceID, &Appearance);
      if(!ret_val)
      {
         /* Map the Appearance to a String.                             */
         if(AppearanceToString(Appearance, &AppearanceString))
            Display(("Device Appearance: %s(%u).\r\n", AppearanceString, Appearance));
         else
            Display(("Device Appearance: Unknown(%u).\r\n", Appearance));
      }
      else
      {
         Display(("Error - GAPS_Query_Device_Appearance returned %d.\r\n", ret_val));

         ret_val = FUNCTION_ERROR;
      }
   }
   else
   {
      Display(("GAP Service not registered.\r\n"));

      ret_val = FUNCTION_ERROR;
   }

   return(ret_val);
}

   /* The following function is responsible for setting the Local Device*/
   /* Appearance value.  This function will return zero on successful   */
   /* execution and a negative value on errors.                         */
static int SetLocalAppearance(ParameterList_t *TempParam)
{
   int    ret_val;
   Word_t Appearance;

   /* Verify that the input parameters are semi-valid.                  */
   if((TempParam) && (TempParam->NumberofParameters > 0) && (TempParam->Params[0].intParam >= 0) && (TempParam->Params[0].intParam < NUMBER_OF_APPEARANCE_MAPPINGS))
   {
      /* Verify that the GAP Service is registered.                     */
      if(GAPSInstanceID)
      {
         /* Map the Appearance Index to the GAP Appearance Value.       */
         if(AppearanceIndexToAppearance(TempParam->Params[0].intParam, &Appearance))
         {
            /* Set the Local Appearance.                                */
            ret_val = GAPS_Set_Device_Appearance(BluetoothStackID, GAPSInstanceID, Appearance);
            if(!ret_val)
               Display(("GAPS_Set_Device_Appearance success.\r\n"));
            else
            {
               Display(("Error - GAPS_Set_Device_Appearance returned %d.\r\n", ret_val));

               ret_val = FUNCTION_ERROR;
            }
         }
         else
         {
            Display(("Invalid Appearance Index.\r\n"));

            ret_val = FUNCTION_ERROR;
         }
      }
      else
      {
         Display(("GAP Service not registered.\r\n"));

         ret_val = FUNCTION_ERROR;
      }
   }
   else
   {
      Display(("Usage: SetLocalName [Index].\r\n"));
      Display(("Where Index = \r\n"));
      DumpAppearanceMappings();

      ret_val = FUNCTION_ERROR;
   }

   return(ret_val);
}

   /* The following function is responsible for reading the Device Name */
   /* for the currently connected remote device.  This function will    */
   /* return zero on successful execution and a negative value on       */
   /* errors.                                                           */
static int ReadRemoteAppearance(ParameterList_t *TempParam)
{
   int           ret_val;
   DeviceInfo_t *DeviceInfo;

   /* Verify that there is a connection that is established.            */
   if(ConnectionID)
   {
      /* Get the device info for the connection device.                 */
      if((DeviceInfo = SearchDeviceInfoEntryByBD_ADDR(&DeviceInfoList, ConnectionBD_ADDR)) != NULL)
      {
         /* Verify that we discovered the Device Name Handle.           */
         if(DeviceInfo->GAPSClientInfo.DeviceAppearanceHandle)
         {
            /* Attempt to read the remote device name.                  */
            ret_val = GATT_Read_Value_Request(BluetoothStackID, ConnectionID, DeviceInfo->GAPSClientInfo.DeviceAppearanceHandle, GATT_ClientEventCallback_GAPS, (unsigned long)DeviceInfo->GAPSClientInfo.DeviceAppearanceHandle);
            if(ret_val > 0)
            {
               Display(("Attempting to read Remote Device Appearance.\r\n"));

               ret_val = 0;
            }
            else
            {
               Display(("Error - GATT_Read_Value_Request returned %d.\r\n", ret_val));

               ret_val = FUNCTION_ERROR;
            }
         }
         else
         {
            Display(("GAP Service Device Appearance Handle not discovered.\r\n"));

            ret_val = FUNCTION_ERROR;
         }
      }
      else
      {
         Display(("No Device Info.\r\n"));

         ret_val = FUNCTION_ERROR;
      }
   }
   else
   {
      Display(("No Connection Established\r\n"));

      ret_val = FUNCTION_ERROR;
   }

   return(ret_val);
}

   /* The following function is responsible for performing a SPPLE      */
   /* Service Discovery Operation.  This function will return zero on   */
   /* successful execution and a negative value on errors.              */
static int DiscoverSPPLE(ParameterList_t *TempParam)
{
   int           ret_val;
   GATT_UUID_t   UUID[1];
   DeviceInfo_t *DeviceInfo;

   /* Verify that there is a connection that is established.            */
   if(ConnectionID)
   {
      /* Get the device info for the connection device.                 */
      if((DeviceInfo = SearchDeviceInfoEntryByBD_ADDR(&DeviceInfoList, ConnectionBD_ADDR)) != NULL)
      {
         /* Verify that no service discovery is outstanding for this    */
         /* device.                                                     */
         if(!(DeviceInfo->Flags & DEVICE_INFO_FLAGS_SERVICE_DISCOVERY_OUTSTANDING))
         {
            /* Configure the filter so that only the SPP LE Service is  */
            /* discovered.                                              */
            UUID[0].UUID_Type = guUUID_128;
            SPPLE_ASSIGN_SPPLE_SERVICE_UUID_128(&(UUID[0].UUID.UUID_128));

            /* Start the service discovery process.                     */
            ret_val = GATT_Start_Service_Discovery(BluetoothStackID, ConnectionID, (sizeof(UUID)/sizeof(GATT_UUID_t)), UUID, GATT_Service_Discovery_Event_Callback, 0);
            if(!ret_val)
            {
               /* Display success message.                              */
               Display(("GATT_Service_Discovery_Start success.\r\n"));

               /* Flag that a Service Discovery Operation is            */
               /* outstanding.                                          */
               DeviceInfo->Flags |= DEVICE_INFO_FLAGS_SERVICE_DISCOVERY_OUTSTANDING;
            }
            else
            {
               /* An error occur so just clean-up.                      */
               Display(("Error - GATT_Service_Discovery_Start returned %d.\r\n", ret_val));

               ret_val = FUNCTION_ERROR;
            }
         }
         else
         {
            Display(("Service Discovery Operation Outstanding for Device.\r\n"));

            ret_val = FUNCTION_ERROR;
         }
      }
      else
      {
         Display(("No Device Info.\r\n"));

         ret_val = FUNCTION_ERROR;
      }
   }
   else
   {
      Display(("No Connection Established\r\n"));

      ret_val = FUNCTION_ERROR;
   }

   return(ret_val);
}

   /* The following function is responsible for registering a SPPLE     */
   /* Service.  This function will return zero on successful execution  */
   /* and a negative value on errors.                                   */
static int RegisterSPPLE(ParameterList_t *TempParam)
{
   int                           ret_val;
   GATT_Attribute_Handle_Group_t ServiceHandleGroup;

   /* Verify that there is no active connection.                        */
   if(!ConnectionID)
   {
      /* Verify that the Service is not already registered.             */
      if(!SPPLEServiceID)
      {
         /* Register the SPPLE Service.                                 */
         ret_val = GATT_Register_Service(BluetoothStackID, SPPLE_SERVICE_FLAGS, SPPLE_SERVICE_ATTRIBUTE_COUNT, (GATT_Service_Attribute_Entry_t *)SPPLE_Service, &ServiceHandleGroup, GATT_ServerEventCallback, 0);
         if(ret_val > 0)
         {
            /* Display success message.                                 */
            Display(("Successfully registered SPPLE Service.\r\n"));

            /* Save the ServiceID of the registered service.            */
            SPPLEServiceID = (unsigned int)ret_val;

            /* Return success to the caller.                            */
            ret_val        = 0;
         }
      }
      else
      {
         Display(("SPPLE Service already registered.\r\n"));

         ret_val = FUNCTION_ERROR;
      }
   }
   else
   {
      Display(("Connection current active.\r\n"));

      ret_val = FUNCTION_ERROR;
   }

   return(ret_val);
}

   /* The following function is responsible for configure a SPPLE       */
   /* Service on a remote device.  This function will return zero on    */
   /* successful execution and a negative value on errors.              */
static int ConfigureSPPLE(ParameterList_t *TempParam)
{
   int           ret_val;
   DeviceInfo_t *DeviceInfo;

   /* Verify that there is a connection that is established.            */
   if(ConnectionID)
   {
      /* Get the device info for the connection device.                 */
      if((DeviceInfo = SearchDeviceInfoEntryByBD_ADDR(&DeviceInfoList, ConnectionBD_ADDR)) != NULL)
      {
         /* Determine if a service discovery operation has been         */
         /* previously done.                                            */
         if(SPPLE_CLIENT_INFORMATION_VALID(DeviceInfo->ClientInfo))
         {
            Display(("SPPLE Service found on remote device, attempting to read Transmit Credits, and configured CCCDs.\r\n"));

            /* Send the Initial Credits to the remote device.           */
            SendCredits(DeviceInfo, DeviceInfo->ReceiveBuffer.BytesFree);

            /* Enable Notifications on the proper characteristics.      */
            EnableDisableNotificationsIndications(DeviceInfo->ClientInfo.Rx_Credit_Client_Configuration_Descriptor, GATT_CLIENT_CONFIGURATION_CHARACTERISTIC_NOTIFY_ENABLE, GATT_ClientEventCallback_SPPLE);
            EnableDisableNotificationsIndications(DeviceInfo->ClientInfo.Tx_Client_Configuration_Descriptor, GATT_CLIENT_CONFIGURATION_CHARACTERISTIC_NOTIFY_ENABLE, GATT_ClientEventCallback_SPPLE);

            ret_val = 0;
         }
         else
         {
            Display(("No SPPLE Service discovered on device.\r\n"));

            ret_val = FUNCTION_ERROR;
         }
      }
      else
      {
         Display(("No Device Info.\r\n"));

         ret_val = FUNCTION_ERROR;
      }
   }
   else
   {
      Display(("No Connection Established\r\n"));

      ret_val = FUNCTION_ERROR;
   }

   return(ret_val);
}

   /* The following function is responsible for sending a number of     */
   /* characters to a remote device to which a connection exists.  The  */
   /* function receives a parameter that indicates the number of byte to*/
   /* be transferred.  This function will return zero on successful     */
   /* execution and a negative value on errors.                         */
static int SendDataCommand(ParameterList_t *TempParam)
{
   DeviceInfo_t *DeviceInfo;

   /* Make sure that all of the parameters required for this function   */
   /* appear to be at least semi-valid.                                 */
   if((TempParam) && (TempParam->NumberofParameters >= 1) && (TempParam->Params[0].intParam > 0))
   {
      /* Verify that there is a connection that is established.         */
      if(ConnectionID)
      {
         /* Check to see if we are sending to another port.             */
         if(!SendInfo.BytesToSend)
         {
            /* Get the device info for the connection device.           */
            if((DeviceInfo = SearchDeviceInfoEntryByBD_ADDR(&DeviceInfoList, ConnectionBD_ADDR)) != NULL)
            {
               /* Get the count of the number of bytes to send.         */
               SendInfo.BytesToSend  = (DWord_t)TempParam->Params[0].intParam;
               SendInfo.BytesSent    = 0;

               /* Kick start the send process.                          */
               SendProcess(DeviceInfo);
            }
            else
               Display(("No Device Info.\r\n"));
         }
         else
            Display(("Send Currently in progress.\r\n"));
      }
      else
         Display(("No Connection Established\r\n"));
   }
   else
      DisplayUsage("SEND [Number of Bytes to send]\r\n");

   return(0);
}

   /* The following function is responsible for reading data sent by a  */
   /* remote device to which a connection exists.  This function will   */
   /* return zero on successful execution and a negative value on       */
   /* errors.                                                           */
static int ReadDataCommand(ParameterList_t *TempParam)
{
   Boolean_t     Done;
   unsigned int  Temp;
   DeviceInfo_t *DeviceInfo;

   /* Verify that there is a connection that is established.            */
   if(ConnectionID)
   {
      /* Get the device info for the connection device.                 */
      if((DeviceInfo = SearchDeviceInfoEntryByBD_ADDR(&DeviceInfoList, ConnectionBD_ADDR)) != NULL)
      {
         /* Determine the number of bytes we are going to read.         */
         Temp = DeviceInfo->ReceiveBuffer.BufferSize - DeviceInfo->ReceiveBuffer.BytesFree;

         Display(("Read: %u.\r\n", Temp));

         /* Loop and read all of the data.                              */
         Done = FALSE;
         while(!Done)
         {
            /* Read the data.                                           */
            Temp = ReadData(DeviceInfo, SPPLE_DATA_BUFFER_LENGTH, SPPLEBuffer);
            if(Temp > 0)
            {
               /* Display the data.                                     */
               HAL_ConsoleWrite(Temp, (char *)SPPLEBuffer);
            }
            else
               Done = TRUE;
         }
      }
      else
         Display(("No Device Info.\r\n"));
   }
   else
      Display(("No Connection Established\r\n"));

   Display(("\r\n"));


   return(0);
}

   /* ***************************************************************** */
   /*                         Event Callbacks                           */
   /* ***************************************************************** */

   /* The following function is for the GAP LE Event Receive Data       */
   /* Callback.  This function will be called whenever a Callback has   */
   /* been registered for the specified GAP LE Action that is associated*/
   /* with the Bluetooth Stack.  This function passes to the caller the */
   /* GAP LE Event Data of the specified Event and the GAP LE Event     */
   /* Callback Parameter that was specified when this Callback was      */
   /* installed.  The caller is free to use the contents of the GAP LE  */
   /* Event Data ONLY in the context of this callback.  If the caller   */
   /* requires the Data for a longer period of time, then the callback  */
   /* function MUST copy the data into another Data Buffer.  This       */
   /* function is guaranteed NOT to be invoked more than once           */
   /* simultaneously for the specified installed callback (i.e.  this   */
   /* function DOES NOT have be reentrant).  It Needs to be noted       */
   /* however, that if the same Callback is installed more than once,   */
   /* then the callbacks will be called serially.  Because of this, the */
   /* processing in this function should be as efficient as possible.   */
   /* It should also be noted that this function is called in the Thread*/
   /* Context of a Thread that the User does NOT own.  Therefore,       */
   /* processing in this function should be as efficient as possible    */
   /* (this argument holds anyway because other GAP Events will not be  */
   /* processed while this function call is outstanding).               */
   /* * NOTE * This function MUST NOT Block and wait for Events that can*/
   /*          only be satisfied by Receiving a Bluetooth Event         */
   /*          Callback.  A Deadlock WILL occur because NO Bluetooth    */
   /*          Callbacks will be issued while this function is currently*/
   /*          outstanding.                                             */
static void BTPSAPI GAP_LE_Event_Callback(unsigned int BluetoothStackID, GAP_LE_Event_Data_t *GAP_LE_Event_Data, unsigned long CallbackParameter)
{
   int                                           Result;
   BoardStr_t                                    BoardStr;
   unsigned int                                  Index;
   DeviceInfo_t                                 *DeviceInfo;
   Long_Term_Key_t                               GeneratedLTK;
   GAP_LE_Security_Information_t                 GAP_LE_Security_Information;
   GAP_LE_Advertising_Report_Data_t             *DeviceEntryPtr;
   GAP_LE_Authentication_Event_Data_t           *Authentication_Event_Data;
   GAP_LE_Authentication_Response_Information_t  GAP_LE_Authentication_Response_Information;

   /* Verify that all parameters to this callback are Semi-Valid.       */
   if((BluetoothStackID) && (GAP_LE_Event_Data))
   {
      switch(GAP_LE_Event_Data->Event_Data_Type)
      {
         case etLE_Advertising_Report:
            Display(("etLE_Advertising_Report with size %d.\r\n",(int)GAP_LE_Event_Data->Event_Data_Size));
            Display(("  %d Responses.\r\n",GAP_LE_Event_Data->Event_Data.GAP_LE_Advertising_Report_Event_Data->Number_Device_Entries));

            for(Index = 0; Index < GAP_LE_Event_Data->Event_Data.GAP_LE_Advertising_Report_Event_Data->Number_Device_Entries; Index++)
            {
               DeviceEntryPtr = &(GAP_LE_Event_Data->Event_Data.GAP_LE_Advertising_Report_Event_Data->Advertising_Data[Index]);

               /* Display the packet type for the device                */
               switch(DeviceEntryPtr->Advertising_Report_Type)
               {
                  case rtConnectableUndirected:
                     Display(("  Advertising Type: %s.\r\n", "rtConnectableUndirected"));
                     break;
                  case rtConnectableDirected:
                     Display(("  Advertising Type: %s.\r\n", "rtConnectableDirected"));
                     break;
                  case rtScannableUndirected:
                     Display(("  Advertising Type: %s.\r\n", "rtScannableUndirected"));
                     break;
                  case rtNonConnectableUndirected:
                     Display(("  Advertising Type: %s.\r\n", "rtNonConnectableUndirected"));
                     break;
                  case rtScanResponse:
                     Display(("  Advertising Type: %s.\r\n", "rtScanResponse"));
                     break;
               }

               /* Display the Address Type.                             */
               if(DeviceEntryPtr->Address_Type == latPublic)
               {
                  Display(("  Address Type: %s.\r\n","atPublic"));
               }
               else
               {
                  Display(("  Address Type: %s.\r\n","atRandom"));
               }

               /* Display the Device Address.                           */
               Display(("  Address: 0x%02X%02X%02X%02X%02X%02X.\r\n", DeviceEntryPtr->BD_ADDR.BD_ADDR5, DeviceEntryPtr->BD_ADDR.BD_ADDR4, DeviceEntryPtr->BD_ADDR.BD_ADDR3, DeviceEntryPtr->BD_ADDR.BD_ADDR2, DeviceEntryPtr->BD_ADDR.BD_ADDR1, DeviceEntryPtr->BD_ADDR.BD_ADDR0));
               Display(("  RSSI: 0x%02X.\r\n", DeviceEntryPtr->RSSI));
               Display(("  Data Length: %d.\r\n", DeviceEntryPtr->Raw_Report_Length));

               DisplayAdvertisingData(&(DeviceEntryPtr->Advertising_Data));
            }
            break;
         case etLE_Connection_Complete:
            Display(("etLE_Connection_Complete with size %d.\r\n",(int)GAP_LE_Event_Data->Event_Data_Size));

            if(GAP_LE_Event_Data->Event_Data.GAP_LE_Connection_Complete_Event_Data)
            {
               BD_ADDRToStr(GAP_LE_Event_Data->Event_Data.GAP_LE_Connection_Complete_Event_Data->Peer_Address, BoardStr);

               Display(("   Status:       0x%02X.\r\n", GAP_LE_Event_Data->Event_Data.GAP_LE_Connection_Complete_Event_Data->Status));
               Display(("   Role:         %s.\r\n", (GAP_LE_Event_Data->Event_Data.GAP_LE_Connection_Complete_Event_Data->Master)?"Master":"Slave"));
               Display(("   Address Type: %s.\r\n", (GAP_LE_Event_Data->Event_Data.GAP_LE_Connection_Complete_Event_Data->Peer_Address_Type == latPublic)?"Public":"Random"));
               Display(("   BD_ADDR:      %s.\r\n", BoardStr));

               if(GAP_LE_Event_Data->Event_Data.GAP_LE_Connection_Complete_Event_Data->Status == HCI_ERROR_CODE_NO_ERROR)
               {
                  ConnectionBD_ADDR   = GAP_LE_Event_Data->Event_Data.GAP_LE_Connection_Complete_Event_Data->Peer_Address;
                  LocalDeviceIsMaster = GAP_LE_Event_Data->Event_Data.GAP_LE_Connection_Complete_Event_Data->Master;

                  /* Make sure that no entry already exists.            */
                  if((DeviceInfo = SearchDeviceInfoEntryByBD_ADDR(&DeviceInfoList, ConnectionBD_ADDR)) == NULL)
                  {
                     /* No entry exists so create one.                  */
                     if(!CreateNewDeviceInfoEntry(&DeviceInfoList, GAP_LE_Event_Data->Event_Data.GAP_LE_Connection_Complete_Event_Data->Peer_Address_Type, ConnectionBD_ADDR))
                        Display(("Failed to add device to Device Info List.\r\n"));
                  }
                  else
                  {
                     /* If we are the Master of the connection we will  */
                     /* attempt to Re-Establish Security if a LTK for   */
                     /* this device exists (i.e.  we previously paired).*/
                     if(LocalDeviceIsMaster)
                     {
                        /* Re-Establish Security if there is a LTK that */
                        /* is stored for this device.                   */
                        if(DeviceInfo->Flags & DEVICE_INFO_FLAGS_LTK_VALID)
                        {
                           /* Re-Establish Security with this LTK.      */
                           Display(("Attempting to Re-Establish Security.\r\n"));

                           /* Attempt to re-establish security to this  */
                           /* device.                                   */
                           GAP_LE_Security_Information.Local_Device_Is_Master                                      = TRUE;
                           BTPS_MemCopy(&(GAP_LE_Security_Information.Security_Information.Master_Information.LTK), &(DeviceInfo->LTK), sizeof(DeviceInfo->LTK));
                           GAP_LE_Security_Information.Security_Information.Master_Information.EDIV                = DeviceInfo->EDIV;
                           BTPS_MemCopy(&(GAP_LE_Security_Information.Security_Information.Master_Information.Rand), &(DeviceInfo->Rand), sizeof(DeviceInfo->Rand));
                           GAP_LE_Security_Information.Security_Information.Master_Information.Encryption_Key_Size = DeviceInfo->EncryptionKeySize;

                           Result = GAP_LE_Reestablish_Security(BluetoothStackID, ConnectionBD_ADDR, &GAP_LE_Security_Information, GAP_LE_Event_Callback, 0);
                           if(Result)
                           {
                              Display(("GAP_LE_Reestablish_Security returned %d.\r\n",Result));
                           }
                        }
                     }
                  }
               }
            }
            break;
         case etLE_Disconnection_Complete:
            Display(("etLE_Disconnection_Complete with size %d.\r\n", (int)GAP_LE_Event_Data->Event_Data_Size));

            if(GAP_LE_Event_Data->Event_Data.GAP_LE_Disconnection_Complete_Event_Data)
            {
               Display(("   Status: 0x%02X.\r\n", GAP_LE_Event_Data->Event_Data.GAP_LE_Disconnection_Complete_Event_Data->Status));
               Display(("   Reason: 0x%02X.\r\n", GAP_LE_Event_Data->Event_Data.GAP_LE_Disconnection_Complete_Event_Data->Reason));

               BD_ADDRToStr(GAP_LE_Event_Data->Event_Data.GAP_LE_Disconnection_Complete_Event_Data->Peer_Address, BoardStr);
               Display(("   BD_ADDR: %s.\r\n", BoardStr));

               /* Clear the Send Information.                           */
               SendInfo.BytesToSend = 0;
               SendInfo.BytesSent   = 0;

               /* Check to see if the device info is present in the     */
               /* list.                                                 */
               if((DeviceInfo = SearchDeviceInfoEntryByBD_ADDR(&DeviceInfoList, ConnectionBD_ADDR)) != NULL)
               {
                  /* Flag that no service discovery operation is        */
                  /* outstanding for this device.                       */
                  DeviceInfo->Flags &= ~DEVICE_INFO_FLAGS_SERVICE_DISCOVERY_OUTSTANDING;

                  /* Re-initialize the Transmit and Receive Buffers, as */
                  /* well as the transmit credits.                      */
                  InitializeBuffer(&(DeviceInfo->TransmitBuffer));
                  InitializeBuffer(&(DeviceInfo->ReceiveBuffer));

                  /* Clear the CCCDs stored for this device.            */
                  DeviceInfo->ServerInfo.Rx_Credit_Client_Configuration_Descriptor = 0;
                  DeviceInfo->ServerInfo.Tx_Client_Configuration_Descriptor        = 0;

                  /* Clear the Transmit Credits count.                  */
                  DeviceInfo->TransmitCredits = 0;
               }

               /* Clear the saved Connection BD_ADDR.                   */
               ASSIGN_BD_ADDR(ConnectionBD_ADDR, 0, 0, 0, 0, 0, 0);
               LocalDeviceIsMaster = FALSE;
            }
            break;
         case etLE_Encryption_Change:
            Display(("etLE_Encryption_Change with size %d.\r\n",(int)GAP_LE_Event_Data->Event_Data_Size));
            break;
         case etLE_Encryption_Refresh_Complete:
            Display(("etLE_Encryption_Refresh_Complete with size %d.\r\n", (int)GAP_LE_Event_Data->Event_Data_Size));
            break;
         case etLE_Authentication:
            Display(("etLE_Authentication with size %d.\r\n", (int)GAP_LE_Event_Data->Event_Data_Size));

            /* Make sure the authentication event data is valid before  */
            /* continuing.                                              */
            if((Authentication_Event_Data = GAP_LE_Event_Data->Event_Data.GAP_LE_Authentication_Event_Data) != NULL)
            {
               BD_ADDRToStr(Authentication_Event_Data->BD_ADDR, BoardStr);

               switch(Authentication_Event_Data->GAP_LE_Authentication_Event_Type)
               {
                  case latLongTermKeyRequest:
                     Display(("    latKeyRequest: \r\n"));
                     Display(("      BD_ADDR: %s.\r\n", BoardStr));

                     /* The other side of a connection is requesting    */
                     /* that we start encryption. Thus we should        */
                     /* regenerate LTK for this connection and send it  */
                     /* to the chip.                                    */
                     Result = GAP_LE_Regenerate_Long_Term_Key(BluetoothStackID, (Encryption_Key_t *)(&DHK), (Encryption_Key_t *)(&ER), Authentication_Event_Data->Authentication_Event_Data.Long_Term_Key_Request.EDIV, &(Authentication_Event_Data->Authentication_Event_Data.Long_Term_Key_Request.Rand), &GeneratedLTK);
                     if(!Result)
                     {
                        Display(("      GAP_LE_Regenerate_Long_Term_Key Success.\r\n"));

                        /* Respond with the Re-Generated Long Term Key. */
                        GAP_LE_Authentication_Response_Information.GAP_LE_Authentication_Type                                        = larLongTermKey;
                        GAP_LE_Authentication_Response_Information.Authentication_Data_Length                                        = GAP_LE_LONG_TERM_KEY_INFORMATION_DATA_SIZE;
                        GAP_LE_Authentication_Response_Information.Authentication_Data.Long_Term_Key_Information.Encryption_Key_Size = GAP_LE_MAXIMUM_ENCRYPTION_KEY_SIZE;
                        GAP_LE_Authentication_Response_Information.Authentication_Data.Long_Term_Key_Information.Long_Term_Key       = GeneratedLTK;
                     }
                     else
                     {
                        Display(("      GAP_LE_Regenerate_Long_Term_Key returned %d.\r\n",Result));

                        /* Since we failed to generate the requested key*/
                        /* we should respond with a negative response.  */
                        GAP_LE_Authentication_Response_Information.GAP_LE_Authentication_Type = larLongTermKey;
                        GAP_LE_Authentication_Response_Information.Authentication_Data_Length = 0;
                     }

                     /* Send the Authentication Response.               */
                     Result = GAP_LE_Authentication_Response(BluetoothStackID, Authentication_Event_Data->BD_ADDR, &GAP_LE_Authentication_Response_Information);
                     if(Result)
                     {
                        Display(("      GAP_LE_Authentication_Response returned %d.\r\n",Result));
                     }
                     break;
                  case latSecurityRequest:
                     /* Display the data for this event.                */
                     /* * NOTE * This is only sent from Slave to Master.*/
                     /*          Thus we must be the Master in this     */
                     /*          connection.                            */
                     Display(("    latSecurityRequest:.\r\n"));
                     Display(("      BD_ADDR: %s.\r\n", BoardStr));
                     Display(("      Bonding Type: %s.\r\n", ((Authentication_Event_Data->Authentication_Event_Data.Security_Request.Bonding_Type == lbtBonding)?"Bonding":"No Bonding")));
                     Display(("      MITM: %s.\r\n", ((Authentication_Event_Data->Authentication_Event_Data.Security_Request.MITM == TRUE)?"YES":"NO")));

                     /* Determine if we have previously paired with the */
                     /* device. If we have paired we will attempt to    */
                     /* re-establish security using a previously        */
                     /* exchanged LTK.                                  */
                     if((DeviceInfo = SearchDeviceInfoEntryByBD_ADDR(&DeviceInfoList, Authentication_Event_Data->BD_ADDR)) != NULL)
                     {
                        /* Determine if a Valid Long Term Key is stored */
                        /* for this device.                             */
                        if(DeviceInfo->Flags & DEVICE_INFO_FLAGS_LTK_VALID)
                        {
                           Display(("Attempting to Re-Establish Security.\r\n"));

                           /* Attempt to re-establish security to this  */
                           /* device.                                   */
                           GAP_LE_Security_Information.Local_Device_Is_Master                                      = TRUE;
                           BTPS_MemCopy(&(GAP_LE_Security_Information.Security_Information.Master_Information.LTK), &(DeviceInfo->LTK), sizeof(DeviceInfo->LTK));
                           GAP_LE_Security_Information.Security_Information.Master_Information.EDIV                = DeviceInfo->EDIV;
                           BTPS_MemCopy(&(GAP_LE_Security_Information.Security_Information.Master_Information.Rand), &(DeviceInfo->Rand), sizeof(DeviceInfo->Rand));
                           GAP_LE_Security_Information.Security_Information.Master_Information.Encryption_Key_Size = DeviceInfo->EncryptionKeySize;

                           Result = GAP_LE_Reestablish_Security(BluetoothStackID, Authentication_Event_Data->BD_ADDR, &GAP_LE_Security_Information, GAP_LE_Event_Callback, 0);
                           if(Result)
                           {
                              Display(("GAP_LE_Reestablish_Security returned %d.\r\n",Result));
                           }
                        }
                        else
                        {
                           CurrentRemoteBD_ADDR = Authentication_Event_Data->BD_ADDR;

                           /* We do not have a stored Link Key for this */
                           /* device so go ahead and pair to this       */
                           /* device.                                   */
                           SendPairingRequest(Authentication_Event_Data->BD_ADDR, TRUE);
                        }
                     }
                     else
                     {
                        CurrentRemoteBD_ADDR = Authentication_Event_Data->BD_ADDR;

                        /* There is no Key Info Entry for this device   */
                        /* so we will just treat this as a slave        */
                        /* request and initiate pairing.                */
                        SendPairingRequest(Authentication_Event_Data->BD_ADDR, TRUE);
                     }

                     break;
                  case latPairingRequest:
                     CurrentRemoteBD_ADDR = Authentication_Event_Data->BD_ADDR;

                     Display(("Pairing Request: %s.\r\n",BoardStr));
                     DisplayPairingInformation(Authentication_Event_Data->Authentication_Event_Data.Pairing_Request);

                     /* This is a pairing request. Respond with a       */
                     /* Pairing Response.                               */
                     /* * NOTE * This is only sent from Master to Slave.*/
                     /*          Thus we must be the Slave in this      */
                     /*          connection.                            */

                     /* Send the Pairing Response.                      */
                     SlavePairingRequestResponse(Authentication_Event_Data->BD_ADDR);
                     break;
                  case latConfirmationRequest:
                     Display(("latConfirmationRequest.\r\n"));

                     if(Authentication_Event_Data->Authentication_Event_Data.Confirmation_Request.Request_Type == crtNone)
                     {
                        Display(("Invoking Just Works.\r\n"));

                        /* Just Accept Just Works Pairing.              */
                        GAP_LE_Authentication_Response_Information.GAP_LE_Authentication_Type = larConfirmation;

                        /* By setting the Authentication_Data_Length to */
                        /* any NON-ZERO value we are informing the GAP  */
                        /* LE Layer that we are accepting Just Works    */
                        /* Pairing.                                     */
                        GAP_LE_Authentication_Response_Information.Authentication_Data_Length = DWORD_SIZE;

                        Result = GAP_LE_Authentication_Response(BluetoothStackID, Authentication_Event_Data->BD_ADDR, &GAP_LE_Authentication_Response_Information);
                        if(Result)
                        {
                           Display(("GAP_LE_Authentication_Response returned %d.\r\n",Result));
                        }
                     }
                     else
                     {
                        if(Authentication_Event_Data->Authentication_Event_Data.Confirmation_Request.Request_Type == crtPasskey)
                        {
                           Display(("Call LEPasskeyResponse [PASSCODE].\r\n"));
                        }
                        else
                        {
                           if(Authentication_Event_Data->Authentication_Event_Data.Confirmation_Request.Request_Type == crtDisplay)
                           {
                              Display(("Passkey: %06u.\r\n", Authentication_Event_Data->Authentication_Event_Data.Confirmation_Request.Display_Passkey));
                           }
                        }
                     }
                     break;
                  case latSecurityEstablishmentComplete:
                     Display(("Security Re-Establishment Complete: %s.\r\n", BoardStr));
                     Display(("                            Status: 0x%02X.\r\n", Authentication_Event_Data->Authentication_Event_Data.Security_Establishment_Complete.Status));
                     break;
                  case latPairingStatus:
                     ASSIGN_BD_ADDR(CurrentRemoteBD_ADDR, 0, 0, 0, 0, 0, 0);

                     Display(("Pairing Status: %s.\r\n", BoardStr));
                     Display(("        Status: 0x%02X.\r\n", Authentication_Event_Data->Authentication_Event_Data.Pairing_Status.Status));

                     if(Authentication_Event_Data->Authentication_Event_Data.Pairing_Status.Status == GAP_LE_PAIRING_STATUS_NO_ERROR)
                     {
                        Display(("        Key Size: %d.\r\n", Authentication_Event_Data->Authentication_Event_Data.Pairing_Status.Negotiated_Encryption_Key_Size));
                     }
                     else
                     {
                        /* Failed to pair so delete the key entry for   */
                        /* this device and disconnect the link.         */
                        if((DeviceInfo = DeleteDeviceInfoEntry(&DeviceInfoList, Authentication_Event_Data->BD_ADDR)) != NULL)
                           FreeDeviceInfoEntryMemory(DeviceInfo);

                        /* Disconnect the Link.                         */
                        GAP_LE_Disconnect(BluetoothStackID, Authentication_Event_Data->BD_ADDR);
                     }
                     break;
                  case latEncryptionInformationRequest:
                     Display(("Encryption Information Request %s.\r\n", BoardStr));

                     /* Generate new LTK, EDIV and Rand and respond with*/
                     /* them.                                           */
                     EncryptionInformationRequestResponse(Authentication_Event_Data->BD_ADDR, Authentication_Event_Data->Authentication_Event_Data.Encryption_Request_Information.Encryption_Key_Size, &GAP_LE_Authentication_Response_Information);
                     break;
                  case latEncryptionInformation:
                     /* Display the information from the event.         */
                     Display((" Encryption Information from RemoteDevice: %s.\r\n", BoardStr));
                     Display(("                             Key Size: %d.\r\n", Authentication_Event_Data->Authentication_Event_Data.Encryption_Information.Encryption_Key_Size));

                     /* ** NOTE ** If we are the Slave we will NOT      */
                     /*            store the LTK that is sent to us by  */
                     /*            the Master.  However if it was ever  */
                     /*            desired that the Master and Slave    */
                     /*            switch roles in a later connection   */
                     /*            we could store that information at   */
                     /*            this point.                          */
                     if(LocalDeviceIsMaster)
                     {
                        /* Search for the entry for this slave to store */
                        /* the information into.                        */
                        if((DeviceInfo = SearchDeviceInfoEntryByBD_ADDR(&DeviceInfoList, Authentication_Event_Data->BD_ADDR)) != NULL)
                        {
                           BTPS_MemCopy(&(DeviceInfo->LTK), &(Authentication_Event_Data->Authentication_Event_Data.Encryption_Information.LTK), sizeof(DeviceInfo->LTK));
                           DeviceInfo->EDIV              = Authentication_Event_Data->Authentication_Event_Data.Encryption_Information.EDIV;
                           BTPS_MemCopy(&(DeviceInfo->Rand), &(Authentication_Event_Data->Authentication_Event_Data.Encryption_Information.Rand), sizeof(DeviceInfo->Rand));
                           DeviceInfo->EncryptionKeySize = Authentication_Event_Data->Authentication_Event_Data.Encryption_Information.Encryption_Key_Size;
                           DeviceInfo->Flags            |= DEVICE_INFO_FLAGS_LTK_VALID;
                        }
                        else
                        {
                           Display(("No Key Info Entry for this Slave.\r\n"));
                        }
                     }
                     break;
                  default:
                     break;
               }
            }
            break;
         default:
            break;
      }

      /* Display the command prompt.                                    */
      DisplayPrompt();
   }
}

   /* The following function is for an GATT Server Event Callback.  This*/
   /* function will be called whenever a GATT Request is made to the    */
   /* server who registers this function that cannot be handled         */
   /* internally by GATT.  This function passes to the caller the GATT  */
   /* Server Event Data that occurred and the GATT Server Event Callback*/
   /* Parameter that was specified when this Callback was installed.    */
   /* The caller is free to use the contents of the GATT Server Event   */
   /* Data ONLY in the context of this callback.  If the caller requires*/
   /* the Data for a longer period of time, then the callback function  */
   /* MUST copy the data into another Data Buffer.  This function is    */
   /* guaranteed NOT to be invoked more than once simultaneously for the*/
   /* specified installed callback (i.e.  this function DOES NOT have be*/
   /* reentrant).  It Needs to be noted however, that if the same       */
   /* Callback is installed more than once, then the callbacks will be  */
   /* called serially.  Because of this, the processing in this function*/
   /* should be as efficient as possible.  It should also be noted that */
   /* this function is called in the Thread Context of a Thread that the*/
   /* User does NOT own.  Therefore, processing in this function should */
   /* be as efficient as possible (this argument holds anyway because   */
   /* another GATT Event (Server/Client or Connection) will not be      */
   /* processed while this function call is outstanding).               */
   /* * NOTE * This function MUST NOT Block and wait for Events that can*/
   /*          only be satisfied by Receiving a Bluetooth Event         */
   /*          Callback.  A Deadlock WILL occur because NO Bluetooth    */
   /*          Callbacks will be issued while this function is currently*/
   /*          outstanding.                                             */
static void BTPSAPI GATT_ServerEventCallback(unsigned int BluetoothStackID, GATT_Server_Event_Data_t *GATT_ServerEventData, unsigned long CallbackParameter)
{
   Byte_t        Temp[2];
   Word_t        Value;
   Word_t        PreviousValue;
   Word_t        AttributeOffset;
   DeviceInfo_t *DeviceInfo;

   /* Verify that all parameters to this callback are Semi-Valid.       */
   if((BluetoothStackID) && (GATT_ServerEventData))
   {
      /* Grab the device for the currently connected device.            */
      if((DeviceInfo = SearchDeviceInfoEntryByBD_ADDR(&DeviceInfoList, ConnectionBD_ADDR)) != NULL)
      {
         switch(GATT_ServerEventData->Event_Data_Type)
         {
            case etGATT_Server_Read_Request:
               /* Verify that the Event Data is valid.                  */
               if(GATT_ServerEventData->Event_Data.GATT_Read_Request_Data)
               {
                  if(GATT_ServerEventData->Event_Data.GATT_Read_Request_Data->AttributeValueOffset == 0)
                  {
                     /* Determine which request this read is coming for.*/
                     switch(GATT_ServerEventData->Event_Data.GATT_Read_Request_Data->AttributeOffset)
                     {
                        case SPPLE_TX_CHARACTERISTIC_CCD_ATTRIBUTE_OFFSET:
                           ASSIGN_HOST_WORD_TO_LITTLE_ENDIAN_UNALIGNED_WORD(Temp, DeviceInfo->ServerInfo.Tx_Client_Configuration_Descriptor);
                           break;
                        case SPPLE_TX_CREDITS_CHARACTERISTIC_ATTRIBUTE_OFFSET:
                           ASSIGN_HOST_WORD_TO_LITTLE_ENDIAN_UNALIGNED_WORD(Temp, DeviceInfo->TransmitCredits);
                           break;
                        case SPPLE_RX_CREDITS_CHARACTERISTIC_ATTRIBUTE_OFFSET:
                           ASSIGN_HOST_WORD_TO_LITTLE_ENDIAN_UNALIGNED_WORD(Temp, DeviceInfo->ReceiveBuffer.BytesFree);
                           break;
                        case SPPLE_RX_CREDITS_CHARACTERISTIC_CCD_ATTRIBUTE_OFFSET:
                           ASSIGN_HOST_WORD_TO_LITTLE_ENDIAN_UNALIGNED_WORD(Temp, DeviceInfo->ServerInfo.Rx_Credit_Client_Configuration_Descriptor);
                           break;
                     }

                     GATT_Read_Response(BluetoothStackID, GATT_ServerEventData->Event_Data.GATT_Read_Request_Data->TransactionID, WORD_SIZE, Temp);
                  }
                  else
                     GATT_Error_Response(BluetoothStackID, GATT_ServerEventData->Event_Data.GATT_Read_Request_Data->TransactionID, GATT_ServerEventData->Event_Data.GATT_Read_Request_Data->AttributeOffset, ATT_PROTOCOL_ERROR_CODE_ATTRIBUTE_NOT_LONG);
               }
               else
                  Display(("Invalid Read Request Event Data.\r\n"));
               break;
            case etGATT_Server_Write_Request:
               /* Verify that the Event Data is valid.                  */
               if(GATT_ServerEventData->Event_Data.GATT_Write_Request_Data)
               {
                  if(GATT_ServerEventData->Event_Data.GATT_Write_Request_Data->AttributeValueOffset == 0)
                  {
                     /* Cache the Attribute Offset.                     */
                     AttributeOffset = GATT_ServerEventData->Event_Data.GATT_Write_Request_Data->AttributeOffset;

                     /* Verify that the value is of the correct length. */
                     if((AttributeOffset == SPPLE_RX_CHARACTERISTIC_ATTRIBUTE_OFFSET) || ((GATT_ServerEventData->Event_Data.GATT_Write_Request_Data->AttributeValueLength) && (GATT_ServerEventData->Event_Data.GATT_Write_Request_Data->AttributeValueLength <= WORD_SIZE)))
                     {
                        /* Since the value appears valid go ahead and   */
                        /* accept the write request.                    */
                        GATT_Write_Response(BluetoothStackID, GATT_ServerEventData->Event_Data.GATT_Write_Request_Data->TransactionID);

                        /* If this is not a write to the Rx             */
                        /* Characteristic we will read the data here.   */
                        if(AttributeOffset != SPPLE_RX_CHARACTERISTIC_ATTRIBUTE_OFFSET)
                        {
                           if(GATT_ServerEventData->Event_Data.GATT_Write_Request_Data->AttributeValueLength == WORD_SIZE)
                              Value = READ_UNALIGNED_WORD_LITTLE_ENDIAN(GATT_ServerEventData->Event_Data.GATT_Write_Request_Data->AttributeValue);
                           else
                              Value = READ_UNALIGNED_BYTE_LITTLE_ENDIAN(GATT_ServerEventData->Event_Data.GATT_Write_Request_Data->AttributeValue);
                        }
                        else
                           Value = 0;

                        /* Determine which attribute this write request */
                        /* is for.                                      */
                        switch(AttributeOffset)
                        {
                           case SPPLE_TX_CHARACTERISTIC_CCD_ATTRIBUTE_OFFSET:
                              /* Client has updated the Tx CCCD.  Now we*/
                              /* need to check if we have any data to   */
                              /* send.                                  */
                              DeviceInfo->ServerInfo.Tx_Client_Configuration_Descriptor = Value;

                              /* If may be possible for transmit queued */
                              /* data now.  So fake a Receive Credit    */
                              /* event with 0 as the received credits.  */
                              ReceiveCreditEvent(DeviceInfo, 0);
                              break;
                           case SPPLE_TX_CREDITS_CHARACTERISTIC_ATTRIBUTE_OFFSET:
                              /* Client has sent updated credits.       */
                              ReceiveCreditEvent(DeviceInfo, Value);
                              break;
                           case SPPLE_RX_CHARACTERISTIC_ATTRIBUTE_OFFSET:
                              /* Client has sent data, so we should     */
                              /* handle this as a data indication event.*/
                              DataIndicationEvent(DeviceInfo, GATT_ServerEventData->Event_Data.GATT_Write_Request_Data->AttributeValueLength, GATT_ServerEventData->Event_Data.GATT_Write_Request_Data->AttributeValue);

                              if((!AutomaticReadActive) && (!LoopbackActive))
                                 DisplayPrompt();
                              break;
                           case SPPLE_RX_CREDITS_CHARACTERISTIC_CCD_ATTRIBUTE_OFFSET:
                              /* Cache the previous CCD Value.          */
                              PreviousValue = DeviceInfo->ServerInfo.Rx_Credit_Client_Configuration_Descriptor;

                              /* Note the updated Rx CCCD Value.        */
                              DeviceInfo->ServerInfo.Rx_Credit_Client_Configuration_Descriptor = Value;

                              /* If we were not previously configured   */
                              /* for notifications send the initial     */
                              /* credits to the device.                 */
                              if(PreviousValue != GATT_CLIENT_CONFIGURATION_CHARACTERISTIC_NOTIFY_ENABLE)
                              {
                                 /* Send the initial credits to the     */
                                 /* device.                             */
                                 SendCredits(DeviceInfo, DeviceInfo->ReceiveBuffer.BytesFree);
                              }
                              break;
                        }
                     }
                     else
                        GATT_Error_Response(BluetoothStackID, GATT_ServerEventData->Event_Data.GATT_Write_Request_Data->TransactionID, GATT_ServerEventData->Event_Data.GATT_Write_Request_Data->AttributeOffset, ATT_PROTOCOL_ERROR_CODE_INVALID_ATTRIBUTE_VALUE_LENGTH);
                  }
                  else
                     GATT_Error_Response(BluetoothStackID, GATT_ServerEventData->Event_Data.GATT_Write_Request_Data->TransactionID, GATT_ServerEventData->Event_Data.GATT_Write_Request_Data->AttributeOffset, ATT_PROTOCOL_ERROR_CODE_ATTRIBUTE_NOT_LONG);
               }
               else
                  Display(("Invalid Write Request Event Data.\r\n"));
               break;
            default:
               break;
         }
      }
   }
   else
   {
      /* There was an error with one or more of the input parameters.   */
      Display(("\r\n"));

      Display(("GATT Callback Data: Event_Data = NULL.\r\n"));

      DisplayPrompt();
   }
}

   /* The following function is for an GATT Client Event Callback.  This*/
   /* function will be called whenever a GATT Response is received for a*/
   /* request that was made when this function was registered.  This    */
   /* function passes to the caller the GATT Client Event Data that     */
   /* occurred and the GATT Client Event Callback Parameter that was    */
   /* specified when this Callback was installed.  The caller is free to*/
   /* use the contents of the GATT Client Event Data ONLY in the context*/
   /* of this callback.  If the caller requires the Data for a longer   */
   /* period of time, then the callback function MUST copy the data into*/
   /* another Data Buffer.  This function is guaranteed NOT to be       */
   /* invoked more than once simultaneously for the specified installed */
   /* callback (i.e.  this function DOES NOT have be reentrant).  It    */
   /* Needs to be noted however, that if the same Callback is installed */
   /* more than once, then the callbacks will be called serially.       */
   /* Because of this, the processing in this function should be as     */
   /* efficient as possible.  It should also be noted that this function*/
   /* is called in the Thread Context of a Thread that the User does NOT*/
   /* own.  Therefore, processing in this function should be as         */
   /* efficient as possible (this argument holds anyway because another */
   /* GATT Event (Server/Client or Connection) will not be processed    */
   /* while this function call is outstanding).                         */
   /* * NOTE * This function MUST NOT Block and wait for Events that can*/
   /*          only be satisfied by Receiving a Bluetooth Event         */
   /*          Callback.  A Deadlock WILL occur because NO Bluetooth    */
   /*          Callbacks will be issued while this function is currently*/
   /*          outstanding.                                             */
static void BTPSAPI GATT_ClientEventCallback_SPPLE(unsigned int BluetoothStackID, GATT_Client_Event_Data_t *GATT_Client_Event_Data, unsigned long CallbackParameter)
{
   Word_t        Credits;
   BoardStr_t    BoardStr;
   DeviceInfo_t *DeviceInfo;

   /* Verify that all parameters to this callback are Semi-Valid.       */
   if((BluetoothStackID) && (GATT_Client_Event_Data))
   {
      /* Determine the event that occurred.                             */
      switch(GATT_Client_Event_Data->Event_Data_Type)
      {
         case etGATT_Client_Error_Response:
            if(GATT_Client_Event_Data->Event_Data.GATT_Request_Error_Data)
            {
               Display(("\r\nError Response.\r\n"));
               BD_ADDRToStr(GATT_Client_Event_Data->Event_Data.GATT_Request_Error_Data->RemoteDevice, BoardStr);
               Display(("Connection ID:   %u.\r\n", GATT_Client_Event_Data->Event_Data.GATT_Request_Error_Data->ConnectionID));
               Display(("Transaction ID:  %u.\r\n", GATT_Client_Event_Data->Event_Data.GATT_Request_Error_Data->TransactionID));
               Display(("Connection Type: %s.\r\n", (GATT_Client_Event_Data->Event_Data.GATT_Request_Error_Data->ConnectionType == gctLE)?"LE":"BR/EDR"));
               Display(("BD_ADDR:         %s.\r\n", BoardStr));
               Display(("Error Type:      %s.\r\n", (GATT_Client_Event_Data->Event_Data.GATT_Request_Error_Data->ErrorType == retErrorResponse)?"Response Error":"Response Timeout"));

               /* Only print out the rest if it is valid.               */
               if(GATT_Client_Event_Data->Event_Data.GATT_Request_Error_Data->ErrorType == retErrorResponse)
               {
                  Display(("Request Opcode:  0x%02X.\r\n", GATT_Client_Event_Data->Event_Data.GATT_Request_Error_Data->RequestOpCode));
                  Display(("Request Handle:  0x%04X.\r\n", GATT_Client_Event_Data->Event_Data.GATT_Request_Error_Data->RequestHandle));
                  Display(("Error Code:      0x%02X.\r\n", GATT_Client_Event_Data->Event_Data.GATT_Request_Error_Data->ErrorCode));
                  Display(("Error Mesg:      %s.\r\n", ErrorCodeStr[GATT_Client_Event_Data->Event_Data.GATT_Request_Error_Data->ErrorCode]));
               }
            }
            else
               Display(("Error - Null Error Response Data.\r\n"));
            break;
         case etGATT_Client_Read_Response:
            if(GATT_Client_Event_Data->Event_Data.GATT_Read_Response_Data)
            {
               if((DeviceInfo = SearchDeviceInfoEntryByBD_ADDR(&DeviceInfoList, GATT_Client_Event_Data->Event_Data.GATT_Read_Response_Data->RemoteDevice)) != NULL)
               {
                  if((Word_t)CallbackParameter == DeviceInfo->ClientInfo.Rx_Credit_Characteristic)
                  {
                     if(GATT_Client_Event_Data->Event_Data.GATT_Read_Response_Data->AttributeValueLength == WORD_SIZE)
                     {
                        /* Display the credits we just received.        */
                        Credits = READ_UNALIGNED_WORD_LITTLE_ENDIAN(GATT_Client_Event_Data->Event_Data.GATT_Read_Response_Data->AttributeValue);
                        Display(("\r\nReceived %u Initial Credits.\r\n", Credits));

                        /* We have received the initial credits from the*/
                        /* device so go ahead and handle a Receive      */
                        /* Credit Event.                                */
                        ReceiveCreditEvent(DeviceInfo, Credits);
                     }
                  }
               }
            }
            else
               Display(("\r\nError - Null Read Response Data.\r\n"));
            break;
         case etGATT_Client_Exchange_MTU_Response:
            if(GATT_Client_Event_Data->Event_Data.GATT_Exchange_MTU_Response_Data)
            {
               Display(("\r\nExchange MTU Response.\r\n"));
               BD_ADDRToStr(GATT_Client_Event_Data->Event_Data.GATT_Exchange_MTU_Response_Data->RemoteDevice, BoardStr);
               Display(("Connection ID:   %u.\r\n", GATT_Client_Event_Data->Event_Data.GATT_Exchange_MTU_Response_Data->ConnectionID));
               Display(("Transaction ID:  %u.\r\n", GATT_Client_Event_Data->Event_Data.GATT_Exchange_MTU_Response_Data->TransactionID));
               Display(("Connection Type: %s.\r\n", (GATT_Client_Event_Data->Event_Data.GATT_Exchange_MTU_Response_Data->ConnectionType == gctLE)?"LE":"BR/EDR"));
               Display(("BD_ADDR:         %s.\r\n", BoardStr));
               Display(("MTU:             %u.\r\n", GATT_Client_Event_Data->Event_Data.GATT_Exchange_MTU_Response_Data->ServerMTU));
            }
            else
               Display(("\r\nError - Null Write Response Data.\r\n"));
            break;
         case etGATT_Client_Write_Response:
            if(GATT_Client_Event_Data->Event_Data.GATT_Write_Response_Data)
            {
               Display(("\r\nWrite Response.\r\n"));
               BD_ADDRToStr(GATT_Client_Event_Data->Event_Data.GATT_Write_Response_Data->RemoteDevice, BoardStr);
               Display(("Connection ID:   %u.\r\n", GATT_Client_Event_Data->Event_Data.GATT_Write_Response_Data->ConnectionID));
               Display(("Transaction ID:  %u.\r\n", GATT_Client_Event_Data->Event_Data.GATT_Write_Response_Data->TransactionID));
               Display(("Connection Type: %s.\r\n", (GATT_Client_Event_Data->Event_Data.GATT_Write_Response_Data->ConnectionType == gctLE)?"LE":"BR/EDR"));
               Display(("BD_ADDR:         %s.\r\n", BoardStr));
               Display(("Bytes Written:   %u.\r\n", GATT_Client_Event_Data->Event_Data.GATT_Write_Response_Data->BytesWritten));
            }
            else
               Display(("\r\nError - Null Write Response Data.\r\n"));
            break;
         default:
            break;
      }

      /* Print the command line prompt.                                 */
      DisplayPrompt();
   }
   else
   {
      /* There was an error with one or more of the input parameters.   */
      Display(("\r\n"));

      Display(("GATT Callback Data: Event_Data = NULL.\r\n"));

      DisplayPrompt();
   }
}


   /* The following function is for an GATT Client Event Callback.  This*/
   /* function will be called whenever a GATT Response is received for a*/
   /* request that was made when this function was registered.  This    */
   /* function passes to the caller the GATT Client Event Data that     */
   /* occurred and the GATT Client Event Callback Parameter that was    */
   /* specified when this Callback was installed.  The caller is free to*/
   /* use the contents of the GATT Client Event Data ONLY in the context*/
   /* of this callback.  If the caller requires the Data for a longer   */
   /* period of time, then the callback function MUST copy the data into*/
   /* another Data Buffer.  This function is guaranteed NOT to be       */
   /* invoked more than once simultaneously for the specified installed */
   /* callback (i.e.  this function DOES NOT have be reentrant).  It    */
   /* Needs to be noted however, that if the same Callback is installed */
   /* more than once, then the callbacks will be called serially.       */
   /* Because of this, the processing in this function should be as     */
   /* efficient as possible.  It should also be noted that this function*/
   /* is called in the Thread Context of a Thread that the User does NOT*/
   /* own.  Therefore, processing in this function should be as         */
   /* efficient as possible (this argument holds anyway because another */
   /* GATT Event (Server/Client or Connection) will not be processed    */
   /* while this function call is outstanding).                         */
   /* * NOTE * This function MUST NOT Block and wait for Events that can*/
   /*          only be satisfied by Receiving a Bluetooth Event         */
   /*          Callback.  A Deadlock WILL occur because NO Bluetooth    */
   /*          Callbacks will be issued while this function is currently*/
   /*          outstanding.                                             */
static void BTPSAPI GATT_ClientEventCallback_GAPS(unsigned int BluetoothStackID, GATT_Client_Event_Data_t *GATT_Client_Event_Data, unsigned long CallbackParameter)
{
   char         *NameBuffer;
   Word_t        Appearance;
   BoardStr_t    BoardStr;
   DeviceInfo_t *DeviceInfo;

   /* Verify that all parameters to this callback are Semi-Valid.       */
   if((BluetoothStackID) && (GATT_Client_Event_Data))
   {
      /* Determine the event that occurred.                             */
      switch(GATT_Client_Event_Data->Event_Data_Type)
      {
         case etGATT_Client_Error_Response:
            if(GATT_Client_Event_Data->Event_Data.GATT_Request_Error_Data)
            {
               Display(("\r\nError Response.\r\n"));
               BD_ADDRToStr(GATT_Client_Event_Data->Event_Data.GATT_Request_Error_Data->RemoteDevice, BoardStr);
               Display(("Connection ID:   %u.\r\n", GATT_Client_Event_Data->Event_Data.GATT_Request_Error_Data->ConnectionID));
               Display(("Transaction ID:  %u.\r\n", GATT_Client_Event_Data->Event_Data.GATT_Request_Error_Data->TransactionID));
               Display(("Connection Type: %s.\r\n", (GATT_Client_Event_Data->Event_Data.GATT_Request_Error_Data->ConnectionType == gctLE)?"LE":"BR/EDR"));
               Display(("BD_ADDR:         %s.\r\n", BoardStr));
               Display(("Error Type:      %s.\r\n", (GATT_Client_Event_Data->Event_Data.GATT_Request_Error_Data->ErrorType == retErrorResponse)?"Response Error":"Response Timeout"));

               /* Only print out the rest if it is valid.               */
               if(GATT_Client_Event_Data->Event_Data.GATT_Request_Error_Data->ErrorType == retErrorResponse)
               {
                  Display(("Request Opcode:  0x%02X.\r\n", GATT_Client_Event_Data->Event_Data.GATT_Request_Error_Data->RequestOpCode));
                  Display(("Request Handle:  0x%04X.\r\n", GATT_Client_Event_Data->Event_Data.GATT_Request_Error_Data->RequestHandle));
                  Display(("Error Code:      0x%02X.\r\n", GATT_Client_Event_Data->Event_Data.GATT_Request_Error_Data->ErrorCode));
                  Display(("Error Mesg:      %s.\r\n", ErrorCodeStr[GATT_Client_Event_Data->Event_Data.GATT_Request_Error_Data->ErrorCode]));
               }
            }
            else
               Display(("Error - Null Error Response Data.\r\n"));
            break;
         case etGATT_Client_Read_Response:
            if(GATT_Client_Event_Data->Event_Data.GATT_Read_Response_Data)
            {
               if((DeviceInfo = SearchDeviceInfoEntryByBD_ADDR(&DeviceInfoList, GATT_Client_Event_Data->Event_Data.GATT_Read_Response_Data->RemoteDevice)) != NULL)
               {
                  if((Word_t)CallbackParameter == DeviceInfo->GAPSClientInfo.DeviceNameHandle)
                  {
                     /* Display the remote device name.                 */
                     if((NameBuffer = (char *)BTPS_AllocateMemory(GATT_Client_Event_Data->Event_Data.GATT_Read_Response_Data->AttributeValueLength+1)) != NULL)
                     {
                        BTPS_MemInitialize(NameBuffer, 0, GATT_Client_Event_Data->Event_Data.GATT_Read_Response_Data->AttributeValueLength+1);
                        BTPS_MemCopy(NameBuffer, GATT_Client_Event_Data->Event_Data.GATT_Read_Response_Data->AttributeValue, GATT_Client_Event_Data->Event_Data.GATT_Read_Response_Data->AttributeValueLength);

                        Display(("\r\nRemote Device Name: %s.\r\n", NameBuffer));

                        BTPS_FreeMemory(NameBuffer);
                     }
                  }
                  else
                  {
                     if((Word_t)CallbackParameter == DeviceInfo->GAPSClientInfo.DeviceAppearanceHandle)
                     {
                        if(GATT_Client_Event_Data->Event_Data.GATT_Read_Response_Data->AttributeValueLength == GAP_DEVICE_APPEARENCE_VALUE_LENGTH)
                        {
                           Appearance = READ_UNALIGNED_WORD_LITTLE_ENDIAN(GATT_Client_Event_Data->Event_Data.GATT_Read_Response_Data->AttributeValue);
                           if(AppearanceToString(Appearance, &NameBuffer))
                              Display(("\r\nRemote Device Appearance: %s(%u).\r\n", NameBuffer, Appearance));
                           else
                              Display(("\r\nRemote Device Appearance: Unknown(%u).\r\n", Appearance));
                        }
                        else
                           Display(("Invalid Remote Appearance Value Length %u.\r\n", GATT_Client_Event_Data->Event_Data.GATT_Read_Response_Data->AttributeValueLength));
                     }
                  }
               }
            }
            else
               Display(("\r\nError - Null Read Response Data.\r\n"));
            break;
         default:
            break;
      }

      /* Print the command line prompt.                                 */
      DisplayPrompt();
   }
   else
   {
      /* There was an error with one or more of the input parameters.   */
      Display(("\r\n"));

      Display(("GATT Callback Data: Event_Data = NULL.\r\n"));

      DisplayPrompt();
   }
}

   /* The following function is for an GATT Connection Event Callback.  */
   /* This function is called for GATT Connection Events that occur on  */
   /* the specified Bluetooth Stack.  This function passes to the caller*/
   /* the GATT Connection Event Data that occurred and the GATT         */
   /* Connection Event Callback Parameter that was specified when this  */
   /* Callback was installed.  The caller is free to use the contents of*/
   /* the GATT Client Event Data ONLY in the context of this callback.  */
   /* If the caller requires the Data for a longer period of time, then */
   /* the callback function MUST copy the data into another Data Buffer.*/
   /* This function is guaranteed NOT to be invoked more than once      */
   /* simultaneously for the specified installed callback (i.e.  this   */
   /* function DOES NOT have be reentrant).  It Needs to be noted       */
   /* however, that if the same Callback is installed more than once,   */
   /* then the callbacks will be called serially.  Because of this, the */
   /* processing in this function should be as efficient as possible.   */
   /* It should also be noted that this function is called in the Thread*/
   /* Context of a Thread that the User does NOT own.  Therefore,       */
   /* processing in this function should be as efficient as possible    */
   /* (this argument holds anyway because another GATT Event            */
   /* (Server/Client or Connection) will not be processed while this    */
   /* function call is outstanding).                                    */
   /* * NOTE * This function MUST NOT Block and wait for Events that can*/
   /*          only be satisfied by Receiving a Bluetooth Event         */
   /*          Callback.  A Deadlock WILL occur because NO Bluetooth    */
   /*          Callbacks will be issued while this function is currently*/
   /*          outstanding.                                             */
static void BTPSAPI GATT_Connection_Event_Callback(unsigned int BluetoothStackID, GATT_Connection_Event_Data_t *GATT_Connection_Event_Data, unsigned long CallbackParameter)
{
   Word_t        Credits;
   Boolean_t     SuppressResponse = FALSE;
   BoardStr_t    BoardStr;
   DeviceInfo_t *DeviceInfo;

   /* Verify that all parameters to this callback are Semi-Valid.       */
   if((BluetoothStackID) && (GATT_Connection_Event_Data))
   {
      /* Determine the Connection Event that occurred.                  */
      switch(GATT_Connection_Event_Data->Event_Data_Type)
      {
         case etGATT_Connection_Device_Connection:
            if(GATT_Connection_Event_Data->Event_Data.GATT_Device_Connection_Data)
            {
               /* Save the Connection ID for later use.                 */
               ConnectionID = GATT_Connection_Event_Data->Event_Data.GATT_Device_Connection_Data->ConnectionID;

               Display(("\r\netGATT_Connection_Device_Connection with size %u: \r\n", GATT_Connection_Event_Data->Event_Data_Size));
               BD_ADDRToStr(GATT_Connection_Event_Data->Event_Data.GATT_Device_Connection_Data->RemoteDevice, BoardStr);
               Display(("   Connection ID:   %u.\r\n", GATT_Connection_Event_Data->Event_Data.GATT_Device_Connection_Data->ConnectionID));
               Display(("   Connection Type: %s.\r\n", ((GATT_Connection_Event_Data->Event_Data.GATT_Device_Connection_Data->ConnectionType == gctLE)?"LE":"BR/EDR")));
               Display(("   Remote Device:   %s.\r\n", BoardStr));
               Display(("   Connection MTU:  %u.\r\n", GATT_Connection_Event_Data->Event_Data.GATT_Device_Connection_Data->MTU));

               if((DeviceInfo = SearchDeviceInfoEntryByBD_ADDR(&DeviceInfoList, GATT_Connection_Event_Data->Event_Data.GATT_Device_Connection_Data->RemoteDevice)) != NULL)
               {
                  /* Clear the SPPLE Role Flag.                         */
                  DeviceInfo->Flags &= ~DEVICE_INFO_FLAGS_SPPLE_SERVER;

                  /* Initialize the Transmit and Receive Buffers.       */
                  InitializeBuffer(&(DeviceInfo->ReceiveBuffer));
                  InitializeBuffer(&(DeviceInfo->TransmitBuffer));

                  if(!LocalDeviceIsMaster)
                  {
                     /* Flag that we will act as the Server.            */
                     DeviceInfo->Flags |= DEVICE_INFO_FLAGS_SPPLE_SERVER;

                     /* Send the Initial Credits if the Rx Credit CCD is*/
                     /* already configured (for a bonded device this    */
                     /* could be the case).                             */
                     SendCredits(DeviceInfo, DeviceInfo->ReceiveBuffer.BytesFree);
                  }
                  else
                  {
                     /* Attempt to update the MTU to the maximum        */
                     /* supported.                                      */
                     GATT_Exchange_MTU_Request(BluetoothStackID, ConnectionID, BTPS_CONFIGURATION_GATT_MAXIMUM_SUPPORTED_MTU_SIZE, GATT_ClientEventCallback_SPPLE, 0);
                  }
               }
            }
            else
               Display(("Error - Null Connection Data.\r\n"));
            break;
         case etGATT_Connection_Device_Disconnection:
            if(GATT_Connection_Event_Data->Event_Data.GATT_Device_Disconnection_Data)
            {
               /* Clear the Connection ID.                              */
               ConnectionID = 0;

               Display(("\r\netGATT_Connection_Device_Disconnection with size %u: \r\n", GATT_Connection_Event_Data->Event_Data_Size));
               BD_ADDRToStr(GATT_Connection_Event_Data->Event_Data.GATT_Device_Disconnection_Data->RemoteDevice, BoardStr);
               Display(("   Connection ID:   %u.\r\n", GATT_Connection_Event_Data->Event_Data.GATT_Device_Disconnection_Data->ConnectionID));
               Display(("   Connection Type: %s.\r\n", ((GATT_Connection_Event_Data->Event_Data.GATT_Device_Disconnection_Data->ConnectionType == gctLE)?"LE":"BR/EDR")));
               Display(("   Remote Device:   %s.\r\n", BoardStr));
            }
            else
               Display(("Error - Null Disconnection Data.\r\n"));
            break;
         case etGATT_Connection_Server_Notification:
            if(GATT_Connection_Event_Data->Event_Data.GATT_Server_Notification_Data)
            {
               /* Find the Device Info for the device that has sent us  */
               /* the notification.                                     */
               if((DeviceInfo = SearchDeviceInfoEntryByBD_ADDR(&DeviceInfoList, GATT_Connection_Event_Data->Event_Data.GATT_Server_Notification_Data->RemoteDevice)) != NULL)
               {
                  /* Determine the characteristic that is being         */
                  /* notified.                                          */
                  if(GATT_Connection_Event_Data->Event_Data.GATT_Server_Notification_Data->AttributeHandle == DeviceInfo->ClientInfo.Rx_Credit_Characteristic)
                  {
                     /* Verify that the length of the Rx Credit         */
                     /* Notification is correct.                        */
                     if(GATT_Connection_Event_Data->Event_Data.GATT_Server_Notification_Data->AttributeValueLength == WORD_SIZE)
                     {
                        Credits = READ_UNALIGNED_WORD_LITTLE_ENDIAN(GATT_Connection_Event_Data->Event_Data.GATT_Server_Notification_Data->AttributeValue);

                        /* Handle the received credits event.           */
                        ReceiveCreditEvent(DeviceInfo, Credits);

                        /* Suppress the command prompt.                 */
                        SuppressResponse   = TRUE;
                     }
                  }
                  else
                  {
                     if(GATT_Connection_Event_Data->Event_Data.GATT_Server_Notification_Data->AttributeHandle == DeviceInfo->ClientInfo.Tx_Characteristic)
                     {
                        /* This is a Tx Characteristic Event.  So call  */
                        /* the function to handle the data indication   */
                        /* event.                                       */
                        DataIndicationEvent(DeviceInfo, GATT_Connection_Event_Data->Event_Data.GATT_Server_Notification_Data->AttributeValueLength, GATT_Connection_Event_Data->Event_Data.GATT_Server_Notification_Data->AttributeValue);

                        /* If we are not looping back or doing automatic*/
                        /* reads we will display the prompt.            */
                        if((!AutomaticReadActive) && (!LoopbackActive))
                           SuppressResponse = FALSE;
                        else
                           SuppressResponse = TRUE;
                     }
                  }
               }
            }
            else
               Display(("Error - Null Server Notification Data.\r\n"));
            break;
         default:
            break;
      }

      /* Print the command line prompt.                                 */
      if(!SuppressResponse)
         DisplayPrompt();
   }
   else
   {
      /* There was an error with one or more of the input parameters.   */
      Display(("\r\n"));

      Display(("GATT Connection Callback Data: Event_Data = NULL.\r\n"));

      DisplayPrompt();
   }
}

   /* The following function is for an GATT Discovery Event Callback.   */
   /* This function will be called whenever a GATT Service is discovered*/
   /* or a previously started service discovery process is completed.   */
   /* This function passes to the caller the GATT Discovery Event Data  */
   /* that occurred and the GATT Client Event Callback Parameter that   */
   /* was specified when this Callback was installed.  The caller is    */
   /* free to use the contents of the GATT Discovery Event Data ONLY in */
   /* the context of this callback.  If the caller requires the Data for*/
   /* a longer period of time, then the callback function MUST copy the */
   /* data into another Data Buffer.  This function is guaranteed NOT to*/
   /* be invoked more than once simultaneously for the specified        */
   /* installed callback (i.e.  this function DOES NOT have be          */
   /* reentrant).  It Needs to be noted however, that if the same       */
   /* Callback is installed more than once, then the callbacks will be  */
   /* called serially.  Because of this, the processing in this function*/
   /* should be as efficient as possible.  It should also be noted that */
   /* this function is called in the Thread Context of a Thread that the*/
   /* User does NOT own.  Therefore, processing in this function should */
   /* be as efficient as possible (this argument holds anyway because   */
   /* another GATT Discovery Event will not be processed while this     */
   /* function call is outstanding).                                    */
   /* * NOTE * This function MUST NOT Block and wait for Events that can*/
   /*          only be satisfied by Receiving a Bluetooth Event         */
   /*          Callback.  A Deadlock WILL occur because NO Bluetooth    */
   /*          Callbacks will be issued while this function is currently*/
   /*          outstanding.                                             */
static void BTPSAPI GATT_Service_Discovery_Event_Callback(unsigned int BluetoothStackID, GATT_Service_Discovery_Event_Data_t *GATT_Service_Discovery_Event_Data, unsigned long CallbackParameter)
{
   DeviceInfo_t *DeviceInfo;

   if((BluetoothStackID) && (GATT_Service_Discovery_Event_Data))
   {
      if((DeviceInfo = SearchDeviceInfoEntryByBD_ADDR(&DeviceInfoList, ConnectionBD_ADDR)) != NULL)
      {
         switch(GATT_Service_Discovery_Event_Data->Event_Data_Type)
         {
            case etGATT_Service_Discovery_Indication:
               /* Verify the event data.                                */
               if(GATT_Service_Discovery_Event_Data->Event_Data.GATT_Service_Discovery_Indication_Data)
               {
                  Display(("\r\n"));
                  Display(("Service 0x%04X - 0x%04X, UUID: ", GATT_Service_Discovery_Event_Data->Event_Data.GATT_Service_Discovery_Indication_Data->ServiceInformation.Service_Handle, GATT_Service_Discovery_Event_Data->Event_Data.GATT_Service_Discovery_Indication_Data->ServiceInformation.End_Group_Handle));
                  DisplayUUID(&(GATT_Service_Discovery_Event_Data->Event_Data.GATT_Service_Discovery_Indication_Data->ServiceInformation.UUID));
                  Display(("\r\n"));

                  /* Attempt to populate the handles for the GAP        */
                  /* Service.                                           */
                  GAPSPopulateHandles(&(DeviceInfo->GAPSClientInfo), GATT_Service_Discovery_Event_Data->Event_Data.GATT_Service_Discovery_Indication_Data);

                  /* Attempt to populate the handles for the SPPLE      */
                  /* Service.                                           */
                  SPPLEPopulateHandles(&(DeviceInfo->ClientInfo), GATT_Service_Discovery_Event_Data->Event_Data.GATT_Service_Discovery_Indication_Data);
               }
               break;
            case etGATT_Service_Discovery_Complete:
               /* Verify the event data.                                */
               if(GATT_Service_Discovery_Event_Data->Event_Data.GATT_Service_Discovery_Complete_Data)
               {
                  Display(("\r\n"));
                  Display(("Service Discovery Operation Complete, Status 0x%02X.\r\n", GATT_Service_Discovery_Event_Data->Event_Data.GATT_Service_Discovery_Complete_Data->Status));

                  /* Flag that no service discovery operation is        */
                  /* outstanding for this device.                       */
                  DeviceInfo->Flags &= ~DEVICE_INFO_FLAGS_SERVICE_DISCOVERY_OUTSTANDING;
               }
               break;
         }

         DisplayPrompt();
      }
   }
   else
   {
      /* There was an error with one or more of the input parameters.   */
      Display(("\r\n"));

      Display(("GATT Callback Data: Event_Data = NULL.\r\n"));

      DisplayPrompt();
   }
}

   /* The following function is used to initialize the application      */
   /* instance.  This function should open the stack and prepare to     */
   /* execute commands based on user input.  The first parameter passed */
   /* to this function is the HCI Driver Information that will be used  */
   /* when opening the stack and the second parameter is used to pass   */
   /* parameters to BTPS_Init.  This function returns the               */
   /* BluetoothStackID returned from BSC_Initialize on success or a     */
   /* negative error code (of the form APPLICATION_ERROR_XXX).          */
void SampleMain(void *UserParameter)
{
   int                           Result;
   HCI_DriverInformation_t       HCI_DriverInformation;

   /* Initialize some defaults.                                         */
   LoopbackActive      = FALSE;
   DisplayRawData      = FALSE;
   AutomaticReadActive = FALSE;

   /* Configure the UART Parameters.                                    */
   HCI_DRIVER_SET_COMM_INFORMATION(&HCI_DriverInformation, 1, 115200, cpHCILL);
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
               if(Result >= 0)
               {
                  /* Attempt to create LED Toggle Task.                 */
                  if(BTPS_CreateThread(ToggleLED, 384, (void *)LED_TOGGLE_RATE_SUCCESS))
                  {
                     while(1)
                     {
                        /* The program is currently running in Client   */
                        /* Mode.  Start the User Interface.             */
                        UserInterface();
                     }
                  }
                  else
                  {
                     Display(("Error creating LED Toggle Task.\r\n"));
                  }
               }
               else
               {
                  Display(("Failed to configure HCILL.\r\n"));
               }
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
      Display(("Unable to open the stack.\r\n"));

   /* If we get here an error occurred.                                 */
   while(TRUE)
   {
      ToggleLED((void *)100);
   }
}
