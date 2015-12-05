/*****< hcicommt.h >***********************************************************/
/*      Copyright 2000 - 2013 Stonestreet One.                                */
/*      All Rights Reserved.                                                  */
/*                                                                            */
/*  HCICOMMT - Serial HCI Driver Layer Types.                                 */
/*                                                                            */
/*  Author:  Tim Thomas                                                       */
/*                                                                            */
/*** MODIFICATION HISTORY *****************************************************/
/*                                                                            */
/*   mm/dd/yy  F. Lastname    Description of Modification                     */
/*   --------  -----------    ------------------------------------------------*/
/*   07/25/00  T. Thomas      Initial creation.                               */
/******************************************************************************/
#ifndef __HCICOMMTH__
#define __HCICOMMTH__

#include "BTAPITyp.h"
#include "BaseTypes.h"

   /* The following constants represent the Minimum, Maximum, and       */
   /* Values that are used with the Initialization Delay member of the  */
   /* HCI_COMMDriverInformation_t structure.  These Delays are specified*/
   /* in Milliseconds and represent the delay that is to be added       */
   /* between Port Initialization (Open) and the writing of any data to */
   /* the Port.  This functionality was added because it was found that */
   /* some PCMCIA/Compact Flash Cards required a delay between the      */
   /* time the Port was opened and the time when the Card was ready to  */
   /* accept data.  The default is NO Delay (0 Milliseconds).           */
#define HCI_COMM_INFORMATION_INITIALIZATION_DELAY_MINIMUM     0
#define HCI_COMM_INFORMATION_INITIALIZATION_DELAY_MAXIMUM  5000
#define HCI_COMM_INFORMATION_INITIALIZATION_DELAY_DEFAULT     0

   /* The following type declaration defines the HCI Serial Protocol    */
   /* that will be used as the physical HCI Transport Protocol on the   */
   /* actual COM Port that is opened.  This type declaration is used in */
   /* the HCI_COMMDriverInformation_t structure that is required when   */
   /* an HCI COMM Port is opened.                                       */
typedef enum
{
   cpUART,
   cpUART_RTS_CTS,
   cpBCSP,
   cpBCSP_Muzzled,
   cpH4DS,
   cpH4DS_RTS_CTS,
   cpHCILL,
   cpHCILL_RTS_CTS
} HCI_COMM_Protocol_t;

   /* The following prototype is for a callback function to handle      */
   /* changes in the sleep state of the COMM layer.  The first parameter*/
   /* indicates if sleep is allowed.  If this parameter is true, the    */
   /* application can safely power-down the associated hardware.  If    */
   /* this parameter is false, the application should ensure that the   */
   /* hardware is in a state that allows for normal communication.      */
   /* * NOTE * If the COM port is put to sleep, the application is      */
   /*          responsible for reactivating the port when data is       */
   /*          received or when needs to be sent.                       */
   /* * NOTE * The callback function should not make any direct calls   */
   /*          into the Bluetooth stack.                                */
typedef void (BTPSAPI *HCI_Sleep_Callback_t)(Boolean_t SleepAllowed, unsigned long CallbackParameter);

   /* The following type declaration represents the structure of all    */
   /* Data that is needed to open an HCI COMM Port.                     */
typedef struct _tagHCI_COMMDriverInformation_t
{
   unsigned int           DriverInformationSize;/* Physical Size of this      */
                                                /* structure.                 */
   unsigned int           COMPortNumber;        /* Physical COM Port Number   */
                                                /* of the Physical COM Port to*/
                                                /* Open.                      */
   unsigned long          BaudRate;             /* Baud Rate to Open COM Port.*/
   HCI_COMM_Protocol_t    Protocol;             /* HCI Protocol that will be  */
                                                /* used for communication over*/
                                                /* Opened COM Port.           */
   unsigned int           InitializationDelay;  /* Time (In Milliseconds) to  */
                                                /* Delay after the Port is    */
                                                /* opened before any data is  */
                                                /* sent over the Port.  This  */
                                                /* member is present because  */
                                                /* some PCMCIA/Compact Flash  */
                                                /* Cards have been seen that  */
                                                /* require a delay because the*/
                                                /* card does not function for */
                                                /* some specified period of   */
                                                /* time.                      */
  char                   *COMDeviceName;        /* Physical Device Name to use*/
                                                /* to override the device to  */
                                                /* open.  If COMPortNumber is */
                                                /* specified to be the        */
                                                /* equivalent of negative 1   */
                                                /* (-1), then this value is   */
                                                /* taken as an absolute name  */
                                                /* and the COM Port Number is */
                                                /* NOT appended to this value.*/
                                                /* If this value is NULL then */
                                                /* the default (compiled) COM */
                                                /* Device Name is used (and   */
                                                /* the COM Port Number is     */
                                                /* appended to the default).  */
} HCI_COMMDriverInformation_t;

   /* The following structure defines the application configurable      */
   /* settings for an H4DS instance.                                    */
   /* * NOTE * For all parameters except the SleepCallbackFunction and  */
   /*          SleepCallbackParameter, a value of 0 will keep the       */
   /*          current setting.                                         */
typedef struct _tagHCI_H4DSConfiguration_t
{
   unsigned int         SyncTimeMS;             /* The time between successive*/
                                                /* sync messages being sent.  */
   unsigned int         ConfTimeMS;             /* The time between successive*/
                                                /* conf messages being sent.  */
   unsigned int         WakeUpTimeMS;           /* The time between successive*/
                                                /* wake-up messages being     */
                                                /* sent.                      */
   unsigned int         IdleTimeMS;             /* The time of inactivity     */
                                                /* before the link is         */
                                                /* considered idle.           */
   unsigned int         MaxTxFlushTimeMS;       /* The time to wait for the   */
                                                /* transmit buffer to be      */
                                                /* flushed before the         */
                                                /* connection can be put to   */
                                                /* sleep.                     */
   unsigned int         WakeUpCount;            /* The number of wake-up      */
                                                /* messages that will be sent */
                                                /* before the peer is assumed */
                                                /* idle.                      */
   unsigned int         TicksPerWakueUp;        /* The number of ticks that   */
                                                /* will be sent before every  */
                                                /* wake-up message.           */
   HCI_Sleep_Callback_t SleepCallbackFunction;  /* Provides the function to be*/
                                                /* used for sleep callback    */
                                                /* indications. A NULL value  */
                                                /* will disable callbacks.    */
   unsigned long        SleepCallbackParameter; /* The Parameter to be passed */
                                                /* with the callback function.*/
                                                /* This value is ignored if   */
                                                /* the SleepCallbackFunction  */
                                                /* specified is NULL.         */
} HCI_H4DSConfiguration_t;


   /* The following structure defines the application configurable      */
   /* settings for an HCILL instance.                                   */
typedef struct _tagHCI_HCILLConfiguration_t
{
   HCI_Sleep_Callback_t SleepCallbackFunction;  /* Provides the function to be*/
                                                /* used for sleep callback    */
                                                /* indications. A NULL value  */
                                                /* will disable callbacks.    */
   unsigned long        SleepCallbackParameter; /* The Parameter to be passed */
                                                /* with the callback function.*/
                                                /* This value is ignored if   */
                                                /* the SleepCallbackFunction  */
                                                /* specified is NULL.         */
} HCI_HCILLConfiguration_t;

   /* The following constant is used with the                           */
   /* HCI_Driver_Reconfigure_Data_t structure (ReconfigureCommand       */
   /* member) to specify that the Communication parameters are required */
   /* to change.  When specified, the ReconfigureData member will point */
   /* to a valid HCI_COMMDriverInformation_t structure which holds the  */
   /* new parameters.                                                   */
   /* * NOTE * The underlying driver may not support changing all of    */
   /*          specified parameters.  For example, a BCSP enabled port  */
   /*          may not accept being changed to a UART port.             */
#define HCI_COMM_DRIVER_RECONFIGURE_DATA_COMMAND_CHANGE_PARAMETERS        (HCI_DRIVER_RECONFIGURE_DATA_RECONFIGURE_COMMAND_TRANSPORT_START)

   /* The following constant is used with the                           */
   /* HCI_Driver_Reconfigure_Data_t structure (ReconfigureCommand       */
   /* member) to specify that the H4DS parameters are required to       */
   /* change.  When specified, the ReconfigureData member will point to */
   /* a valid HCI_H4DSConfigureation_t structure which holds the new    */
   /* parameters.                                                       */
#define HCI_COMM_DRIVER_RECONFIGURE_DATA_COMMAND_CHANGE_H4DS_PARAMETERS   (HCI_DRIVER_RECONFIGURE_DATA_RECONFIGURE_COMMAND_TRANSPORT_START + 1)

   /* The following constant is used with the                           */
   /* HCI_Driver_Reconfigure_Data_t structure (ReconfigureCommand       */
   /* member) to specify that the HCILL parameters are required to      */
   /* change.  When specified, the ReconfigureData member will point to */
   /* a valid HCI_HCILLConfigureation_t structure which holds the new   */
   /* parameters.                                                       */
#define HCI_COMM_DRIVER_RECONFIGURE_DATA_COMMAND_CHANGE_HCILL_PARAMETERS (HCI_DRIVER_RECONFIGURE_DATA_RECONFIGURE_COMMAND_TRANSPORT_START + 2)

   /* The following constants represent the Minimum and Maximum time in */
   /* milliseconds before the H4DS layer considers the port to be idle. */
#define HCI_H4DS_MINIMUM_IDLE_TIME                           10
#define HCI_H4DS_MAXIMUM_IDLE_TIME                        30000

   /* The following constants represent the Minimum and Maximum time in */
   /* milliseconds between wake-up messages being sent by the H4DS      */
   /* layer.                                                            */
#define HCI_H4DS_MINIMUM_WAKE_UP_MESSAGE_TIME                10
#define HCI_H4DS_MAXIMUM_WAKE_UP_MESSAGE_TIME              1000

   /* The following constants represent the Minimum and Maximum number  */
   /* of ticks sent before each wake-up message by the H4DS layer.      */
#define HCI_H4DS_MINIMUM_TICKS_PER_WAKE_UP                    1
#define HCI_H4DS_MAXIMUM_TICKS_PER_WAKE_UP                   50

   /* The following constants represent the Minimum and Maximum number  */
   /* of Wake-up messages sent by the H4DS layer before the peer is     */
   /* considered idle.                                                  */
#define HCI_H4DS_MINIMUM_WAKE_UP_MESSAGE_COUNT                1
#define HCI_H4DS_MAXIMUM_WAKE_UP_MESSAGE_COUNT              100

   /* The following constants represent the Minimum and Maximum time in */
   /* milliseconds that the H4DS layer waits for the transmit buffers to*/
   /* be flushed before transitioning to a state where sleep is allowed.*/
#define HCI_H4DS_MINIMUM_TRANSMIT_FLUSH_TIME                 10
#define HCI_H4DS_MAXIMUM_TRANSMIT_FLUSH_TIME              30000

   /* The following constants represent the Minimum and Maximum time in */
   /* milliseconds between SYNC messages being sent by the H4DS layer.  */
#define HCI_H4DS_MINIMUM_SYNC_MESSAGE_TIME                   10
#define HCI_H4DS_MAXIMUM_SYNC_MESSAGE_TIME                 1000

   /* The following constants represent the Minimum and Maximum time in */
   /* milliseconds between CONF messages being sent by the H4DS layer.  */
#define HCI_H4DS_MINIMUM_CONF_MESSAGE_TIME                   10
#define HCI_H4DS_MAXIMUM_CONF_MESSAGE_TIME                 1000

#endif
