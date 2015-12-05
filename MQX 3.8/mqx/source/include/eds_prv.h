#ifndef __eds_prv_h__
#define __eds_prv_h__
/**HEADER********************************************************************
* 
* Copyright (c) 2008 Freescale Semiconductor;
* All Rights Reserved
*
* Copyright (c) 2004-2008 Embedded Access Inc.;
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
* $FileName: eds_prv.h$
* $Version : 3.0.3.0$
* $Date    : Nov-21-2008$
*
* Comments:
*
*   IPC Embedded Debug Server private header file
*
*END************************************************************************/

#include "message.h"
#include "msg_prv.h"
#include "ipc.h"
#include "ipc_prv.h"
#include "timer.h"

/*--------------------------------------------------------------------------*/
/*                        CONSTANT DEFINITIONS                              */

/* 
** How long to wait for all mult-proc configuration messages to
** return.
*/
#define EDS_MSG_WAIT_TIMEOUT  (5000) /* Msec */

/* server operations */
#define EDS_IDENTIFY                 0x00000001
#define EDS_LITTLE_ENDIAN_IDENTIFY   0x01000000
#define EDS_READ                     0x00000002
#define EDS_WRITE                    0x00000003
#define EDS_CONFIG_MULTIPROC         0x00000004
#define EDS_CONFIG_MULTIPROC_END     0x00000005
#define EDS_CONFIG_REQUEST_TIMEOUT   0x00000006
#define EDS_IDENTIFY_MULTIPROC       0x00000008
#define EDS_CONFIG_MULTIPROC_RESPOND 0x00000064


#define EDS_BIG_ENDIAN      0
#define EDS_LITTLE_ENDIAN   0xFFFFFFFF

/* error codes */
#define EDS_OK              0
#define EDS_INVALID_OP      1
#define EDS_INVALID_SIZE    2

/* Usefull sizes */
#define IPC_COMMAND_SIZE \
   (sizeof(MESSAGE_HEADER_STRUCT) + (2*sizeof(uint_32)) + sizeof(_task_id))
#define EDS_COMMAND_SIZE  \
   (IPC_COMMAND_SIZE + sizeof(EDS_OP_STRUCT))
#define EDS_DATA_SIZE \
   (IPC_MAX_PARAMETERS - (sizeof(EDS_OP_STRUCT)/sizeof(uint_32)))

/* Validates the EDS component */
#define EDS_VALID    ((_mqx_uint)0x65647376)   /* "edsv" */

/* EDS States */
#define EDS_IDLE                 0
#define EDS_CONFIG_IN_PROGRESS   1

/*
** MACROS
*/


/*--------------------------------------------------------------------------*/
/*                      DATA STRUCTURE DEFINITIONS                          */

/* This structure is always BIG Endian */
typedef struct eds_op_struct 
{
   uint_32  OPERATION;   /* server operation          */
   uint_32  ADDRESS;     /* read/write memory address */
   uint_32  ADDRESS2;    /* extra address field       */
   uint_32  SIZE;        /* size of buffer            */
   uint_32  PROCESSOR;   /* processor type            */
   uint_32  ENDIAN;      /* endian of processor       */
   uint_32  EDS_ERROR;   /* error code                */
} EDS_OP_STRUCT, _PTR_ EDS_OP_STRUCT_PTR;


/*
** The EDS Message struct overlays the Parameters field of 
** the IPC Message struct
*/
typedef struct eds_msg_struct
{
   /* The EDS command to process, always in BIG endian format */
   EDS_OP_STRUCT          OP_COMMAND;

   /* The maximum number of datums to send/receive */
   uint_32                DATA[EDS_DATA_SIZE];

} EDS_MSG_STRUCT, _PTR_ EDS_MSG_STRUCT_PTR;


typedef struct eds_component_struct
{
   LWSEM_STRUCT           SEM;

   IPC_MESSAGE_STRUCT_PTR SAVED_MSG_PTR;
   _mqx_uint              VALID;
   _mqx_uint              STATE;

   _timer_id              TIMER_ID;
   _queue_id              HOST_QID;
   _queue_id              MY_QID;
   _processor_number      RESPONSES_EXPECTED;
   _processor_number      HOST_PNUM;
   _queue_number          HOST_QNUM;

   MQX_TICK_STRUCT        TIMEOUT;

} EDS_COMPONENT_STRUCT, _PTR_ EDS_COMPONENT_STRUCT_PTR;


/*--------------------------------------------------------------------------*/
/*                      PROTOTYPES                                          */


#ifdef __cplusplus
extern "C" {
#endif

extern _mqx_uint _eds_ipc_handler(pointer);
extern void      _eds_ipc_return_message(IPC_MESSAGE_STRUCT_PTR);
extern void      _eds_timeout(_timer_id,pointer,MQX_TICK_STRUCT_PTR);
 
#ifdef __cplusplus
}
#endif

#endif

/* EOF */
