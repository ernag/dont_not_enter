/*****< btpskrnl.c >***********************************************************/
/*      Copyright 2012 - 2013 Stonestreet One.                                */
/*      All Rights Reserved.                                                  */
/*                                                                            */
/*  BTPSKRNL - Stonestreet One Bluetooth Stack Kernel Implementation for      */
/*             FreeRTOS.                                                      */
/*                                                                            */
/*  Author:  Marcus Funk                                                      */
/*                                                                            */
/*** MODIFICATION HISTORY *****************************************************/
/*                                                                            */
/*   mm/dd/yy  F. Lastname    Description of Modification                     */
/*   --------  -----------    ------------------------------------------------*/
/*   11/08/12  M. Funk        Initial creation.                               */
/******************************************************************************/
#include <mqx.h>
#include <mutex.h>
#include <lwevent.h>
#include <bsp.h>
#include <string.h>
#include "wassert.h"
#include "app_errors.h"
#include "main.h"

#include "BTPSKRNL.h"       /* BTPS Kernel Prototypes/Constants.              */
#include "BTTypes.h"        /* BTPS internal data types.                      */


   /* The following constant represents the maximum number of supported */
   /* Tasks that can be supported concurrently by this module.          */
#define MAX_NUMBER_SUPPORTED_TASKS                          5

   /* The following constant represents the number of bytes that are    */
   /* displayed on an individual line when using the DumpData()         */
   /* function.                                                         */
#define MAXIMUM_BYTES_PER_ROW        16

   /* The following structure is a container structure that exists to   */
   /* map the OS Thread Function to the BTPS Kernel Thread Function (it */
   /* doesn't totally map in a one to one relationship/mapping).        */
typedef struct _tagThreadWrapperInfo_t
{
   TASK_TEMPLATE_STRUCT TaskDescriptor;
   _task_id             TaskId;
   char                 ThreadName[5];
   Thread_t             ThreadFunction;
   void                 *ThreadParameter;
} ThreadWrapperInfo_t;

    /* The following type declartion represents the entire state        */
    /* information for a Mutex type.  This stucture is used with all    */
    /* of the Muxtex functions contained in this module.                */
typedef struct _tagMutexHeader_t
{
   MUTEX_STRUCT   Mutex;
   ThreadHandle_t ThreadHandle;
   unsigned int   Count;
} MutexHeader_t;

   /* The following type declaration represents the entire state        */
   /* information for a Mailbox.  This structure is used with all of    */
   /* the Mailbox functions contained in this module.                   */
typedef struct _tagMailboxHeader_t
{
   Event_t       Event;
   Mutex_t       Mutex;
   unsigned int  HeadSlot;
   unsigned int  TailSlot;
   unsigned int  OccupiedSlots;
   unsigned int  NumberSlots;
   unsigned int  SlotSize;
   void         *Slots;
} MailboxHeader_t;

   /* Internal Variables to this Module (Remember that all variables    */
   /* declared static are initialized to 0 automatically by the         */
   /* compiler as part of standard C/C++).                              */
static unsigned long                 DebugZoneMask;

   /* The following is used to guard access to MsgBuf and the function  */
   /* that sends Debug Messages to the Debug UART.                      */
static Mutex_t                      IOMutex;

   /* This variable provides a list of the task properties for MQX.     */
   /* * NOTE * We require a NULL entry after the entry we are adding so */
   /*          the Task List array is one larger in size.               */
static ThreadWrapperInfo_t          MQX_Task_List[MAX_NUMBER_SUPPORTED_TASKS];

   /* The following buffer is used when writing Debug Messages to Debug */
   /* UART.                                                             */
static char                         MsgBuf[256];

static BTPS_MessageOutputCallback_t MessageOutputCallback;

   /* Internal Function Prototypes.                                     */
static void ThreadWrapper(uint_32 UserData);

   /* The following function is a function that represents the native   */
   /* thread type for the operating system.  This function does nothing */
   /* more than to call the BTPSKRNL Thread function of the specified   */
   /* format (parameters/calling convention/etc.).  The UserData        */
   /* parameter that is passed to this function is a pointer to a       */
   /* ThreadWrapperInfo_t structure which will contain the actual       */
   /* BTPSKRNL Thread Information.  This function will free this pointer*/
   /* using the BTPS_FreeMemory() function (which means that the Thread */
   /* Information structure needs to be allocated with the              */
   /* BTPS_AllocateMemory() function.                                   */
static void ThreadWrapper(uint_32 UserData)
{
   ThreadWrapperInfo_t *wrapper = (ThreadWrapperInfo_t*)(UserData);
   
   /* Must have user data */
   wassert(wrapper);

   /* Call the thread function with the specified Thread Parameter.  */
   wrapper->ThreadFunction(wrapper->ThreadParameter);
   
   /* Mark as not running */
   wrapper->TaskId = 0;
}

   /* The following function is responsible for delaying the current    */
   /* task for the specified duration (specified in Milliseconds).      */
   /* * NOTE * Very small timeouts might be smaller in granularity than */
   /*          the system can support !!!!                              */
void BTPSAPI BTPS_Delay(unsigned long MilliSeconds)
{
   Boolean_t Done;

   /* Wrap the OS supplied delay function.                              */
   if(MilliSeconds != BTPS_INFINITE_WAIT)
      _time_delay(MilliSeconds);
   else
   {
      Done = FALSE;
      while(!Done)
         _time_delay(MilliSeconds);
   }
}

   /* The following function is responsible for creating an actual      */
   /* Mutex (Binary Semaphore).  The Mutex is unique in that if a       */
   /* Thread already owns the Mutex, and it requests the Mutex again    */
   /* it will be granted the Mutex.  This is in Stark contrast to a     */
   /* Semaphore that will block waiting for the second acquisition of   */
   /* the Semaphore.  This function accepts as input whether or not     */
   /* the Mutex is initially Signalled or not.  If this input parameter */
   /* is TRUE then the caller owns the Mutex and any other threads      */
   /* waiting on the Mutex will block.  This function returns a NON-NULL*/
   /* Mutex Handle if the Mutex was successfully created, or a NULL     */
   /* Mutex Handle if the Mutex was NOT created.  If a Mutex is         */
   /* successfully created, it can only be destroyed by calling the     */
   /* BTPS_CloseMutex() function (and passing the returned Mutex        */
   /* Handle).                                                          */
Mutex_t BTPSAPI BTPS_CreateMutex(Boolean_t CreateOwned)
{
   Mutex_t            ret_val;
   MutexHeader_t     *MutexHeader;
   MUTEX_ATTR_STRUCT  MutexAttr;

   /* Before proceeding allocate memory for the mutex header and verify */
   /* that the allocate was successful.                                 */
   if((MutexHeader = (MutexHeader_t *)BTPS_AllocateMemory(sizeof(MutexHeader_t))) != NULL)
   {
      /* Memory allocated, attempt to initialize the Mutex.             */
      _mutatr_init(&MutexAttr);

      /* Set the mutex queuing attributes.                              */
      _mutatr_set_wait_protocol(&MutexAttr, MUTEX_QUEUEING);
      _mutatr_set_sched_protocol(&MutexAttr, MUTEX_NO_PRIO_INHERIT);

      if(_mutex_init(&(MutexHeader->Mutex), &MutexAttr) == MQX_EOK)
      {
         /* The Mutex was successfully created, now check to see if the */
         /* mutex is being created as owned.                            */
         if(CreateOwned)
         {
            if(_mutex_lock(&(MutexHeader->Mutex)) == MQX_EOK)
            {
               /* The mutex is being created as owned so initialize the */
               /* count and Thread Handle values within the mutex header*/
               /* structure appropriately.                              */
               MutexHeader->ThreadHandle = BTPS_CurrentThreadHandle();
               MutexHeader->Count        = 1;

               /* The mutex has now been successfully created, set the  */
               /* return value to the mutex header.                     */
               ret_val                   = (Mutex_t)MutexHeader;
            }
            else
            {
               /* Unable to lock the Mutex, go ahead and clean          */
               /* everything up.                                        */
               _mutex_destroy(&(MutexHeader->Mutex));

               BTPS_FreeMemory(MutexHeader);

               ret_val = NULL;
            }
         }
         else
         {
            /* The mutex is not being created as owned so initialize the*/
            /* count and Thread Handle value within the mutex header    */
            /* structure appropriately.                                 */
            MutexHeader->Count        = 0;
            MutexHeader->ThreadHandle = (ThreadHandle_t)0xFFFFFFFF;

            /* The mutex has now been successfully created, set the     */
            /* return value to the mutex header.                        */
            ret_val                   = (Mutex_t)MutexHeader;
         }
      }
      else
      {
         /* We were unable to allocate the memory required for the      */
         /* creation of the semaphore, set the return value to NULL and */
         /* free all resources that have been allocated.                */
         BTPS_FreeMemory(MutexHeader);

         ret_val = (Mutex_t)NULL;
      }
   }
   else
   {
      /* An error occured while trying to allocate memory for the       */
      /* MutexHeader, set the return value to NULL.                     */
      ret_val = (Mutex_t)NULL;
   }
   
   wassert(ret_val);

   return(ret_val);
}

   /* The following function is responsible for waiting for the         */
   /* specified Mutex to become free.  This function accepts as input   */
   /* the Mutex Handle to wait for, and the Timeout (specified in       */
   /* Milliseconds) to wait for the Mutex to become available.  This    */
   /* function returns TRUE if the Mutex was successfully acquired and  */
   /* FALSE if either there was an error OR the Mutex was not           */
   /* acquired in the specified Timeout.  It should be noted that       */
   /* Mutexes have the special property that if the calling Thread      */
   /* already owns the Mutex and it requests access to the Mutex again  */
   /* (by calling this function and specifying the same Mutex Handle)   */
   /* then it will automatically be granted the Mutex.  Once a Mutex    */
   /* has been granted successfully (this function returns TRUE), then  */
   /* the caller MUST call the BTPS_ReleaseMutex() function.            */
   /* * NOTE * There must exist a corresponding BTPS_ReleaseMutex()     */
   /*          function call for EVERY successful BTPS_WaitMutex()      */
   /*          function call or a deadlock will occur in the system !!! */
Boolean_t BTPSAPI BTPS_WaitMutex(Mutex_t Mutex, unsigned long Timeout)
{
   Boolean_t ret_val;

   /* Simply wrap the OS supplied Wait Mutex function.                  */
   
   /* Since this function can return FALSE when non-fatal (i.e., event  */
   /* timeout, we need to assert on all fatal errors explicitly.        */
   wassert(Mutex);

   /* The parameters apprear to be semi-valid check if the thread    */
   /* asking for the Mutex is the current thread that owns it.       */
   if((BTPS_CurrentThreadHandle() == ((MutexHeader_t *)Mutex)->ThreadHandle) && (((MutexHeader_t *)Mutex)->Count))
   {
	   /* This thread owns the mutex so it is not forced to block,    */
	   /* increament the count signifying another call to wait has    */
	   /* been made by the calling thread.                            */
	   ((MutexHeader_t *)Mutex)->Count++;

	   /* Mutex acquired successfully.                                */
	   ret_val = TRUE;
   }
   else
   {
	   if(Timeout == BTPS_INFINITE_WAIT)
	   {
		   /* We are supposed to wait forever.  MQX defines a primitive*/
		   /* for this.                                                */
		   wassert(_mutex_lock(&(((MutexHeader_t *)Mutex)->Mutex)) == MQX_EOK);
		   ret_val = TRUE;
	   }
	   else
	   {
		   /* The event is set to wait for a finite amount of time, in */
		   /* order to do this we must poll the event for that amount  */
		   /* of time.                                                 */
		   ret_val = FALSE;
		   while((((long)(Timeout -= 10)) > 0) && (!ret_val))
		   {
			   /* Attempt to get the mutex for timeout milliseconds.    */
			   if(_mutex_try_lock(&(((MutexHeader_t *)Mutex)->Mutex)) == MQX_EOK)
			   {
				   /* The Mutex was successfully aquired, set the return */
				   /* value to true.                                     */
				   ret_val = TRUE;
			   }
			   else
			   {
				   /* We were unable to get the Mutex, sleep for ten     */
				   /* milliseconds and try again.                        */
				   BTPS_Delay(10);
			   }
		   }
	   }

	   /* If we were able to acquire the Mutex, then we need to flag  */
	   /* that the current Thread owns it.                            */
	   if(ret_val)
	   {
		   /* The Mutex has been aquired, save the current Thread      */
		   /* Handle to the mutex header showing that the calling      */
		   /* thread now owns the mutex.                               */
		   ((MutexHeader_t *)Mutex)->ThreadHandle = BTPS_CurrentThreadHandle();

		   /* Set the Mutex count to 1 since the thread has acquired it*/
		   /* for the first time.                                      */
		   ((MutexHeader_t *)Mutex)->Count        = 1;
	   }
   }

   return(ret_val);
}

   /* The following function is responsible for releasing a Mutex that  */
   /* was successfully acquired with the BTPS_WaitMutex() function.     */
   /* This function accepts as input the Mutex that is currently        */
   /* owned.                                                            */
   /* * NOTE * There must exist a corresponding BTPS_ReleaseMutex()     */
   /*          function call for EVERY successful BTPS_WaitMutex()      */
   /*          function call or a deadlock will occur in the system !!! */
void BTPSAPI BTPS_ReleaseMutex(Mutex_t Mutex)
{
   /* Before proceeding any further we need to make sure that the       */
   /* parameter that was passed to us appears semi-valid.               */
   if(Mutex)
   {
      /* The parameters appear to be semi-valid check if the thread     */
      /* asking to release the Mutex is the current thread that owns it.*/
      if((BTPS_CurrentThreadHandle() == ((MutexHeader_t *)Mutex)->ThreadHandle) && (((MutexHeader_t *)Mutex)->Count))
      {
         /* The Mutex is currently owned now check to see if the mutex  */
         /* is currently aquired else where by the same task.  If it is */
         /* no longer being used by the current task signal the Mutex.  */
         if(!(--((MutexHeader_t *)Mutex)->Count))
            _mutex_unlock(&(((MutexHeader_t *)Mutex)->Mutex));
      }
   }
}

   /* The following function is responsible for destroying a Mutex that */
   /* was created successfully via a successful call to the             */
   /* BTPS_CreateMutex() function.  This function accepts as input the  */
   /* Mutex Handle of the Mutex to destroy.  Once this function is      */
   /* completed the Mutex Handle is NO longer valid and CANNOT be       */
   /* used.  Calling this function will cause all outstanding           */
   /* BTPS_WaitMutex() functions to fail with an error.                 */
void BTPSAPI BTPS_CloseMutex(Mutex_t Mutex)
{
   /* Simply wrap the OS supplied Close Mutex function.                 */
   if(Mutex)
   {
      /* Destroy the Mutex.                                             */
      _mutex_destroy(&(((MutexHeader_t *)Mutex)->Mutex));

      /* Free the memory that was allocated to support the Mutex.       */
      BTPS_FreeMemory(Mutex);
   }
}

   /* The following function is responsible for creating an actual      */
   /* Event.  The Event is unique in that it only has two states.  These*/
   /* states are Signalled and Non-Signalled.  Functions are provided   */
   /* to allow the setting of the current state and to allow the        */
   /* option of waiting for an Event to become Signalled.  This function*/
   /* accepts as input whether or not the Event is initially Signalled  */
   /* or not.  If this input parameter is TRUE then the state of the    */
   /* Event is Signalled and any BTPS_WaitEvent() function calls will   */
   /* immediately return.  This function returns a NON-NULL Event       */
   /* Handle if the Event was successfully created, or a NULL Event     */
   /* Handle if the Event was NOT created.  If an Event is successfully */
   /* created, it can only be destroyed by calling the BTPS_CloseEvent()*/
   /* function (and passing the returned Event Handle).                 */
Event_t BTPSAPI BTPS_CreateEvent(Boolean_t CreateSignalled)
{
   Event_t ret_val;

   /* First, attempt to allocate space to hold the Mutex itself.        */
   if((ret_val = (Event_t)BTPS_AllocateMemory(sizeof(LWEVENT_STRUCT))) != NULL)
   {
      /* Memory allocated, attempt to initialize the Event.             */
      if(_lwevent_create((LWEVENT_STRUCT_PTR)ret_val, 0) == MQX_EOK)
      {
         /* If we need to create this as signalled, go ahead and request*/
         /* it.                                                         */
         if(CreateSignalled)
            _lwevent_set((LWEVENT_STRUCT_PTR)ret_val, 1);
         else
            _lwevent_clear((LWEVENT_STRUCT_PTR)ret_val, 1);
      }
      else
      {
         /* Error initializing Event, go ahead and free the memory.     */
         BTPS_FreeMemory(ret_val);

         ret_val = NULL;
      }
   }

   wassert(ret_val);

   return(ret_val);
}

   /* The following function is responsible for waiting for the         */
   /* specified Event to become Signalled.  This function accepts as    */
   /* input the Event Handle to wait for, and the Timeout (specified    */
   /* in Milliseconds) to wait for the Event to become Signalled.  This */
   /* function returns TRUE if the Event was set to the Signalled       */
   /* State (in the Timeout specified) or FALSE if either there was an  */
   /* error OR the Event was not set to the Signalled State in the      */
   /* specified Timeout.  It should be noted that Signals have a       */
   /* special property in that multiple Threads can be waiting for the  */
   /* Event to become Signalled and ALL calls to BTPS_WaitEvent() will  */
   /* return TRUE whenever the state of the Event becomes Signalled.    */
Boolean_t BTPSAPI BTPS_WaitEvent(Event_t Event, unsigned long Timeout)
{
   Boolean_t       ret_val;
   TIME_STRUCT     _Timeout;
   MQX_TICK_STRUCT TickTimeout;
   _mqx_uint mqx_rc;

   /* Simply wrap the OS supplied Wait Mutex function.                  */
   
   /* Since this function can return FALSE when non-fatal (i.e., event  */
   /* timeout, we need to assert on all fatal errors explicitly.        */
   wassert(Event);

   if(Timeout == BTPS_INFINITE_WAIT)
   {
	   /* We are supposed to wait forever.                            */
	   _lwevent_wait_for((LWEVENT_STRUCT_PTR)Event, 1, (boolean)FALSE, NULL);
	   ret_val = TRUE;
   }
   else
   {
	   /* The event is set to wait for a finite amount of time, in    */
	   /* order to do this we must calculate the correct MQX Ticks    */
	   /* based on the Millisecond ticks to wait for.                 */
	   _Timeout.SECONDS      = 0;
	   _Timeout.MILLISECONDS = Timeout;

	   _time_to_ticks(&_Timeout, &TickTimeout);

	   mqx_rc = _lwevent_wait_for((LWEVENT_STRUCT_PTR)Event, 1, (boolean)FALSE, &TickTimeout);

	   if (LWEVENT_WAIT_TIMEOUT == mqx_rc)
	   {
		   return FALSE;
	   }

	   wassert(mqx_rc == MQX_EOK);

	   ret_val = TRUE;
   }

   return(ret_val);
}

   /* The following function is responsible for changing the state of   */
   /* the specified Event to the Non-Signalled State.  Once the Event   */
   /* is in this State, ALL calls to the BTPS_WaitEvent() function will */
   /* block until the State of the Event is set to the Signalled State. */
   /* This function accepts as input the Event Handle of the Event to   */
   /* set to the Non-Signalled State.                                   */
void BTPSAPI BTPS_ResetEvent(Event_t Event)
{
   /* Simply wrap the OS supplied Reset Event function.                 */
   if(Event)
      _lwevent_clear((LWEVENT_STRUCT_PTR)Event, 1);
}

   /* The following function is responsible for changing the state of   */
   /* the specified Event to the Signalled State.  Once the Event is in */
   /* this State, ALL calls to the BTPS_WaitEvent() function will       */
   /* return.  This function accepts as input the Event Handle of the   */
   /* Event to set to the Signalled State.                              */
void BTPSAPI BTPS_SetEvent(Event_t Event)
{
   /* Simply wrap the OS supplied Set Event function.                   */
   if(Event)
      _lwevent_set((LWEVENT_STRUCT_PTR)Event, 1);
}

   /* The following function is responsible for destroying an Event that*/
   /* was created successfully via a successful call to the             */
   /* BTPS_CreateEvent() function.  This function accepts as input the  */
   /* Event Handle of the Event to destroy.  Once this function is      */
   /* completed the Event Handle is NO longer valid and CANNOT be       */
   /* used.  Calling this function will cause all outstanding           */
   /* BTPS_WaitEvent() functions to fail with an error.                 */
void BTPSAPI BTPS_CloseEvent(Event_t Event)
{
   /* Simply wrap the OS supplied Close Event function.                 */
   if(Event)
   {
      /* Destroy the Mutex.                                             */
      _lwevent_destroy((LWEVENT_STRUCT_PTR)Event);

      /* Free the memory that was allocated to support the Event.       */
      BTPS_FreeMemory(Event);
   }
}

   /* The following function is provided to allow a mechanism to        */
   /* actually allocate a Block of Memory (of at least the specified    */
   /* size).  This function accepts as input the size (in Bytes) of the */
   /* Block of Memory to be allocated.  This function returns a NON-NULL*/
   /* pointer to this Memory Buffer if the Memory was successfully      */
   /* allocated, or a NULL value if the memory could not be allocated.  */
void *BTPSAPI BTPS_AllocateMemory(unsigned long MemorySize)
{
    char *ptr = NULL;
    uint32_t *start_marker = NULL;
    char *end_marker = NULL;
    uint32_t caller = 0;
    
    /* Next make sure that the caller is actually requesting memory to be*/
    /* allocated.                                                        */
    if (MemorySize)
    {
        __asm
        {
           mov caller, LR
        }
        
        if (MemorySize > 2000)
           corona_print("large bt mem req(%d)\n", MemorySize);
        
        /* Simply wrap the OS supplied memory allocation function. Reserve 8 bytes as a red zone */
        ptr = (void *)_mem_alloc_system(MemorySize + 8);
        if (!ptr)
        {
            corona_print("failed: %u\n", MemorySize);
            wassert(ptr);
        }
        
        if (((DWord_t)ptr) & 0x00000003)
            DBG_MSG(DBG_ZONE_CRITICAL_ERROR, ("\n\nxxxx UNALIGNED MEMORY ALLOCATION : %d xxxx\n\n", MemorySize));
        
        /* Place markers in the red zone this is really fragile, buffer allocation greater than 8192 will blow up the system */
        start_marker = (uint32_t *)(ptr);
        if (MemorySize + 8 < 8192)
        {
            *start_marker = (caller << 12) & 0x7ffff000;
            *start_marker |= (((MemorySize + 8) & 0x00000fff) | (((MemorySize + 8) & 0x00001000) << 19));  
            end_marker = ptr + MemorySize + 4;
            memcpy(end_marker, start_marker, sizeof(uint32_t));
    /*        corona_print("RZS: p:%p s:%u l:%x b:%x e:%x\n", ptr, MemorySize + 8, caller, *start_marker, *end_marker); */
        }
        else
            *start_marker = 0xdeadbeef;
    }
    else
        ptr = NULL;
    
    wassert(ptr);
    return(ptr + 4);
}

   /* The following function is responsible for de-allocating a Block   */
   /* of Memory that was successfully allocated with the                */
   /* BTPS_AllocateMemory() function.  This function accepts a NON-NULL */
   /* Memory Pointer which was returned from the BTPS_AllocateMemory()  */
   /* function.  After this function completes the caller CANNOT use    */
   /* ANY of the Memory pointed to by the Memory Pointer.               */
void BTPSAPI BTPS_FreeMemory(void *MemoryPointer)
{
    char *ptr = ((char *)(MemoryPointer)) - 4;
    uint32_t *start_marker = (uint32_t *)(ptr);
    size_t alloc_size = (*start_marker & 0x00000fff) | ((*start_marker >> 19) & 0x00001000);
    uint32_t caller = (*start_marker >> 12) & 0x0007ffff;
    char *end_marker = ptr + alloc_size - 4;
    
    /* Before proceeding any further we need to make sure that the       */
    /* parameters that were passed to us appear semi-valid.              */
    if(MemoryPointer)
    {
        /* check for large memory allocation */
        if (*start_marker != 0xdeadbeef)
        {
            /* Check the start marker pointer range */
            if ((start_marker < (uint32_t *)0x1fff0000) || (start_marker > (uint32_t *)0x2000ffff))
            {
                PROCESS_NINFO(ERR_MEM_FREE, "RZFM p:%p s:%u", ptr, alloc_size);
                wassert(0);
            }
            
            /* Check the end marker point range */
            if ((end_marker < (char *)0x1fff0000) || (end_marker > (char *)0x2000ffff))
            {
                PROCESS_NINFO(ERR_MEM_FREE, "RZFS p:%p s:%u", ptr, alloc_size);
                wassert(0);
            }
            
            /* Check for matching marker contents */
            if (memcmp(start_marker, end_marker, sizeof(uint32_t)) != 0)
            {
                PROCESS_NINFO(ERR_MEM_FREE, "RZF p:%p s:%u l:%x b:%x e:%x", ptr, alloc_size, caller, *start_marker, *end_marker);
                wassert(0);
            }
        }
/*        corona_print("RZC: p:%p s:%u l:%x b:%x e:%x\n", ptr, alloc_size, caller, *start_marker, *end_marker); */
        _mem_free((pointer)(ptr));
    }
    else
        DBG_MSG(DBG_ZONE_CRITICAL_ERROR,("Bad Ptr\r\n"));
}

   /* The following function is responsible for copying a block of      */
   /* memory of the specified size from the specified source pointer    */
   /* to the specified destination memory pointer.  This function       */
   /* accepts as input a pointer to the memory block that is to be      */
   /* Destination Buffer (first parameter), a pointer to memory block   */
   /* that points to the data to be copied into the destination buffer, */
   /* and the size (in bytes) of the Data to copy.  The Source and      */
   /* Destination Memory Buffers must contain AT LEAST as many bytes    */
   /* as specified by the Size parameter.                               */
   /* * NOTE * This function does not allow the overlapping of the      */
   /*          Source and Destination Buffers !!!!                      */
void BTPSAPI BTPS_MemCopy(void *Destination, BTPSCONST void *Source, unsigned long Size)
{
   /* Simply wrap the C Run-Time memcpy() function.                     */
   memcpy(Destination, Source, Size);
}

   /* The following function is responsible for moving a block of       */
   /* memory of the specified size from the specified source pointer    */
   /* to the specified destination memory pointer.  This function       */
   /* accepts as input a pointer to the memory block that is to be      */
   /* Destination Buffer (first parameter), a pointer to memory block   */
   /* that points to the data to be copied into the destination buffer, */
   /* and the size (in bytes) of the Data to copy.  The Source and      */
   /* Destination Memory Buffers must contain AT LEAST as many bytes    */
   /* as specified by the Size parameter.                               */
   /* * NOTE * This function DOES allow the overlapping of the          */
   /*          Source and Destination Buffers.                          */
void BTPSAPI BTPS_MemMove(void *Destination, BTPSCONST void *Source, unsigned long Size)
{
   /* Simply wrap the C Run-Time memmove() function.                    */
   memmove(Destination, Source, Size);
}

   /* The following function is provided to allow a mechanism to fill   */
   /* a block of memory with the specified value.  This function accepts*/
   /* as input a pointer to the Data Buffer (first parameter) that is   */
   /* to filled with the specified value (second parameter).  The       */
   /* final parameter to this function specifies the number of bytes    */
   /* that are to be filled in the Data Buffer.  The Destination        */
   /* Buffer must point to a Buffer that is AT LEAST the size of        */
   /* the Size parameter.                                               */
void BTPSAPI BTPS_MemInitialize(void *Destination, unsigned char Value, unsigned long Size)
{
   /* Simply wrap the C Run-Time memset() function.                     */
   memset(Destination, Value, Size);
}

   /* The following function is provided to allow a mechanism to        */
   /* Compare two blocks of memory to see if the two memory blocks      */
   /* (each of size Size (in bytes)) are equal (each and every byte up  */
   /* to Size bytes).  This function returns a negative number if       */
   /* Source1 is less than Source2, zero if Source1 equals Source2, and */
   /* a positive value if Source1 is greater than Source2.              */
int BTPSAPI BTPS_MemCompare(BTPSCONST void *Source1, BTPSCONST void *Source2, unsigned long Size)
{
   /* Simply wrap the C Run-Time memcmp() function.                     */
   return(memcmp(Source1, Source2, Size));
}

   /* The following function is provided to allow a mechanism to Compare*/
   /* two blocks of memory to see if the two memory blocks (each of size*/
   /* Size (in bytes)) are equal (each and every byte up to Size bytes) */
   /* using a Case-Insensitive Compare.  This function returns a        */
   /* negative number if Source1 is less than Source2, zero if Source1  */
   /* equals Source2, and a positive value if Source1 is greater than   */
   /* Source2.                                                          */
int BTPSAPI BTPS_MemCompareI(BTPSCONST void *Source1, BTPSCONST void *Source2, unsigned long Size)
{
   int           ret_val = 0;
   unsigned char Byte1;
   unsigned char Byte2;
   unsigned int  Index;

   /* Simply loop through each byte pointed to by each pointer and check*/
   /* to see if they are equal.                                         */
   for(Index = 0; ((Index<Size) && (!ret_val)); Index++)
   {
      /* Note each Byte that we are going to compare.                   */
      Byte1 = ((unsigned char *)Source1)[Index];
      Byte2 = ((unsigned char *)Source2)[Index];

      /* If the Byte in the first array is lower case, go ahead and make*/
      /* it upper case (for comparisons below).                         */
      if((Byte1 >= 'a') && (Byte1 <= 'z'))
         Byte1 = Byte1 - ('a' - 'A');

      /* If the Byte in the second array is lower case, go ahead and    */
      /* make it upper case (for comparisons below).                    */
      if((Byte2 >= 'a') && (Byte2 <= 'z'))
         Byte2 = Byte2 - ('a' - 'A');

      /* If the two Bytes are equal then there is nothing to do.        */
      if(Byte1 != Byte2)
      {
         /* Bytes are not equal, so set the return value accordingly.   */
         if(Byte1 < Byte2)
            ret_val = -1;
         else
            ret_val = 1;
      }
   }

   /* Simply return the result of the above comparison(s).              */
   return(ret_val);
}

   /* The following function is provided to allow a mechanism to        */
   /* copy a source NULL Terminated ASCII (character) String to the     */
   /* specified Destination String Buffer.  This function accepts as    */
   /* input a pointer to a buffer (Destination) that is to receive the  */
   /* NULL Terminated ASCII String pointed to by the Source parameter.  */
   /* This function copies the string byte by byte from the Source      */
   /* to the Destination (including the NULL terminator).               */
void BTPSAPI BTPS_StringCopy(char *Destination, BTPSCONST char *Source)
{
   /* Simply wrap the C Run-Time strcpy() function.                     */
   strcpy(Destination, Source);
}

   /* The following function is provided to allow a mechanism to        */
   /* determine the Length (in characters) of the specified NULL        */
   /* Terminated ASCII (character) String.  This function accepts as    */
   /* input a pointer to a NULL Terminated ASCII String and returns     */
   /* the number of characters present in the string (NOT including     */
   /* the terminating NULL character).                                  */
unsigned int BTPSAPI BTPS_StringLength(BTPSCONST char *Source)
{
   unsigned int ret_val;

   /* Simply wrap the C Run-Time strlen() function.                     */
   ret_val = strlen(Source);

   return(ret_val);
}

   /* The following function is provided to allow a means for the       */
   /* programmer to create a separate thread of execution.  This        */
   /* function accepts as input the Function that represents the        */
   /* Thread that is to be installed into the system as its first       */
   /* parameter.  The second parameter is the size of the Threads       */
   /* Stack (in bytes) required by the Thread when it is executing.     */
   /* The final parameter to this function represents a parameter that  */
   /* is to be passed to the Thread when it is created.  This function  */
   /* returns a NON-NULL Thread Handle if the Thread was successfully   */
   /* created, or a NULL Thread Handle if the Thread was unable to be   */
   /* created.  Once the thread is created, the only way for the Thread */
   /* to be removed from the system is for the Thread function to run   */
   /* to completion.                                                    */
   /* * NOTE * There does NOT exist a function to Kill a Thread that    */
   /*          is present in the system.  Because of this, other means  */
   /*          needs to be devised in order to signal the Thread that   */
   /*          it is to terminate.                                      */
ThreadHandle_t BTPSAPI BTPS_CreateThread(Thread_t ThreadFunction, unsigned int StackSize, void *ThreadParameter)
{
    ThreadWrapperInfo_t *thread_info;
    int i;
    
    /* Check parameters */
    wassert(ThreadFunction && StackSize);
    
    /* Find and empty slot */
    thread_info = NULL;
    for (i = 0; i < MAX_NUMBER_SUPPORTED_TASKS; ++i)
    {
        if (MQX_Task_List[i].TaskId == 0)
        {
            thread_info = &MQX_Task_List[i];
            break;
        }
    }
   
    /* Did we find one */
    if (!thread_info)
    {
        PROCESS_NINFO(ERR_BT_CREATE_TASK, "no tsk");
        return NULL;
    }
   
     /* Memory allocated, populate the structure with the correct   */
     /* information.                                                */
     thread_info->ThreadFunction  = ThreadFunction;
     thread_info->ThreadParameter = ThreadParameter;
     BTPS_SprintF(thread_info->ThreadName, "BT_%d", i);

     /* Fill in the Task Entry.                                     */
     thread_info->TaskDescriptor.TASK_TEMPLATE_INDEX = 0;
     thread_info->TaskDescriptor.TASK_ADDRESS        = ThreadWrapper;
     thread_info->TaskDescriptor.TASK_STACKSIZE      = StackSize + 512;
     thread_info->TaskDescriptor.TASK_PRIORITY       = DEFAULT_BTPS_TASK_PRIORITY;
     thread_info->TaskDescriptor.TASK_NAME           = thread_info->ThreadName;
     thread_info->TaskDescriptor.CREATION_PARAMETER  = (uint_32)thread_info;
     thread_info->TaskDescriptor.DEFAULT_TIME_SLICE  = DEFAULT_BTPS_TIME_SLICE_VALUE;
#if MQX_HAS_TIME_SLICE
     thread_info->TaskDescriptor.TASK_ATTRIBUTES     = MQX_TIME_SLICE_TASK;
#else
     thread_info->TaskDescriptor.TASK_ATTRIBUTES     = 0;
#endif
     
     /* Create the task */
     thread_info->TaskId = _task_create(0, 0, (uint_32)(&thread_info->TaskDescriptor));
     if(thread_info->TaskId == MQX_NULL_TASK_ID)
     {
         PROCESS_NINFO(ERR_BT_CREATE_TASK, "bt task[%i].tsk_err %x", i, _task_get_error());
         thread_info->TaskId = 0;
         return NULL;
     }

     return (ThreadHandle_t)(thread_info->TaskId);
}

   /* The following function is provided to allow a mechanism to        */
   /* retrieve the handle of the thread which is currently executing.   */
   /* This function require no input parameters and will return a valid */
   /* ThreadHandle upon success.                                        */
ThreadHandle_t BTPSAPI BTPS_CurrentThreadHandle(void)
{
   ThreadHandle_t ret_val;

   /* Simply return the Current Task ID of the Task that is executing.  */
   ret_val = (ThreadHandle_t)_task_get_id();

  return(ret_val);
}

   /* The following function is provided to allow a mechanism to create */
   /* a Mailbox.  A Mailbox is a Data Store that contains slots (all    */
   /* of the same size) that can have data placed into (and retrieved   */
   /* from).  Once Data is placed into a Mailbox (via the               */
   /* BTPS_AddMailbox() function, it can be retrieved by using the      */
   /* BTPS_WaitMailbox() function.  Data placed into the Mailbox is     */
   /* retrieved in a FIFO method.  This function accepts as input the   */
   /* Maximum Number of Slots that will be present in the Mailbox and   */
   /* the Size of each of the Slots.  This function returns a NON-NULL  */
   /* Mailbox Handle if the Mailbox is successfully created, or a       */
   /* NULL Mailbox Handle if the Mailbox was unable to be created.      */
Mailbox_t BTPSAPI BTPS_CreateMailbox(unsigned int NumberSlots, unsigned int SlotSize)
{
   Mailbox_t        ret_val;
   MailboxHeader_t *MailboxHeader;

   /* Before proceeding any further we need to make sure that the       */
   /* parameters that were passed to us appear semi-valid.              */
   if((NumberSlots) && (SlotSize))
   {
      /* Parameters appear semi-valid, so now let's allocate enough     */
      /* Memory to hold the Mailbox Header AND enough space to hold     */
      /* all requested Mailbox Slots.                                   */
      if((MailboxHeader = (MailboxHeader_t *)BTPS_AllocateMemory(sizeof(MailboxHeader_t)+(NumberSlots*SlotSize))) != NULL)
      {
         /* Memory successfully allocated so now let's create an        */
         /* Event that will be used to signal when there is Data        */
         /* present in the Mailbox.                                     */
         if((MailboxHeader->Event = BTPS_CreateEvent(FALSE)) != NULL)
         {
            /* Event created successfully, now let's create a Mutex     */
            /* that will guard access to the Mailbox Slots.             */
            if((MailboxHeader->Mutex = BTPS_CreateMutex(FALSE)) != NULL)
            {
               /* Everything successfully created, now let's initialize */
               /* the state of the Mailbox such that it contains NO     */
               /* Data.                                                 */
               MailboxHeader->NumberSlots   = NumberSlots;
               MailboxHeader->SlotSize      = SlotSize;
               MailboxHeader->HeadSlot      = 0;
               MailboxHeader->TailSlot      = 0;
               MailboxHeader->OccupiedSlots = 0;
               MailboxHeader->Slots         = ((unsigned char *)MailboxHeader) + sizeof(MailboxHeader_t);

               /* All finished, return success to the caller (the       */
               /* Mailbox Header).                                      */
               ret_val                      = (Mailbox_t)MailboxHeader;
            }
            else
            {
               /* Error creating the Mutex, so let's free the Event     */
               /* we created, Free the Memory for the Mailbox itself    */
               /* and return an error to the caller.                    */
               BTPS_CloseEvent(MailboxHeader->Event);

               BTPS_FreeMemory(MailboxHeader);

               ret_val = NULL;
            }
         }
         else
         {
            /* Error creating the Data Available Event, so let's free   */
            /* the Memory for the Mailbox itself and return an error    */
            /* to the caller.                                           */
            BTPS_FreeMemory(MailboxHeader);

            ret_val = NULL;
         }
      }
      else
         ret_val = NULL;
   }
   else
      ret_val = NULL;
   
   wassert(ret_val);

   /* Return the result to the caller.                                  */
   return(ret_val);
}

   /* The following function is provided to allow a means to Add data   */
   /* to the Mailbox (where it can be retrieved via the                 */
   /* BTPS_WaitMailbox() function.  This function accepts as input the  */
   /* Mailbox Handle of the Mailbox to place the data into and a        */
   /* pointer to a buffer that contains the data to be added.  This     */
   /* pointer *MUST* point to a data buffer that is AT LEAST the Size   */
   /* of the Slots in the Mailbox (specified when the Mailbox was       */
   /* created) and this pointer CANNOT be NULL.  The data that the      */
   /* MailboxData pointer points to is placed into the Mailbox where it */
   /* can be retrieved via the BTPS_WaitMailbox() function.             */
   /* * NOTE * This function copies from the MailboxData Pointer the    */
   /*          first SlotSize Bytes.  The SlotSize was specified when   */
   /*          the Mailbox was created via a successful call to the     */
   /*          BTPS_CreateMailbox() function.                           */
Boolean_t BTPSAPI BTPS_AddMailbox(Mailbox_t Mailbox, void *MailboxData)
{
   Boolean_t ret_val;

   /* Before proceeding any further make sure that the Mailbox Handle   */
   /* and the MailboxData pointer that was specified appears semi-valid.*/
   if((Mailbox) && (MailboxData))
   {
      /* Since we are going to change the Mailbox we need to acquire    */
      /* the Mutex that is guarding access to it.                       */
      if(BTPS_WaitMutex(((MailboxHeader_t *)Mailbox)->Mutex, BTPS_INFINITE_WAIT))
      {
         /* Before adding the data to the Mailbox, make sure that the   */
         /* Mailbox is not already full.                                */
         if(((MailboxHeader_t *)Mailbox)->OccupiedSlots < ((MailboxHeader_t *)Mailbox)->NumberSlots)
         {
            /* Mailbox is NOT full, so add the specified User Data to   */
            /* the next available free Mailbox Slot.                    */
            BTPS_MemCopy(&(((unsigned char *)(((MailboxHeader_t *)Mailbox)->Slots))[((MailboxHeader_t *)Mailbox)->HeadSlot*((MailboxHeader_t *)Mailbox)->SlotSize]), MailboxData, ((MailboxHeader_t *)Mailbox)->SlotSize);

            /* Update the Next available Free Mailbox Slot (taking      */
            /* into account wrapping the pointer).                      */
            if(++(((MailboxHeader_t *)Mailbox)->HeadSlot) == ((MailboxHeader_t *)Mailbox)->NumberSlots)
               ((MailboxHeader_t *)Mailbox)->HeadSlot = 0;

            /* Update the Number of occupied slots to signify that there*/
            /* was additional Mailbox Data added to the Mailbox.        */
            ((MailboxHeader_t *)Mailbox)->OccupiedSlots++;

            /* Signal the Event that specifies that there is indeed     */
            /* Data present in the Mailbox.                             */
            BTPS_SetEvent(((MailboxHeader_t *)Mailbox)->Event);

            /* Finally, return success to the caller.                   */
            ret_val = TRUE;
         }
         else
            ret_val = FALSE;

         /* All finished with the Mailbox, so release the Mailbox       */
         /* Mutex that we currently hold.                               */
         BTPS_ReleaseMutex(((MailboxHeader_t *)Mailbox)->Mutex);
      }
      else
         ret_val = FALSE;
   }
   else
      ret_val = FALSE;

   wassert(ret_val);

   /* Return the result to the caller.                                  */
   return(ret_val);
}

   /* The following function is provided to allow a means to retrieve   */
   /* data from the specified Mailbox.  This function will block until  */
   /* either Data is placed in the Mailbox or an error with the Mailbox */
   /* was detected.  This function accepts as its first parameter a     */
   /* Mailbox Handle that represents the Mailbox to wait for the data   */
   /* with.  This function accepts as its second parameter, a pointer   */
   /* to a data buffer that is AT LEAST the size of a single Slot of    */
   /* the Mailbox (specified when the BTPS_CreateMailbox() function     */
   /* was called).  The MailboxData parameter CANNOT be NULL.  This     */
   /* function will return TRUE if data was successfully retrieved      */
   /* from the Mailbox or FALSE if there was an error retrieving data   */
   /* from the Mailbox.  If this function returns TRUE then the first   */
   /* SlotSize bytes of the MailboxData pointer will contain the data   */
   /* that was retrieved from the Mailbox.                              */
   /* * NOTE * This function copies to the MailboxData Pointer the      */
   /*          data that is present in the Mailbox Slot (of size        */
   /*          SlotSize).  The SlotSize was specified when the Mailbox  */
   /*          was created via a successful call to the                 */
   /*          BTPS_CreateMailbox() function.                           */
Boolean_t BTPSAPI BTPS_WaitMailbox(Mailbox_t Mailbox, void *MailboxData)
{
   Boolean_t ret_val;

   /* Before proceeding any further make sure that the Mailbox Handle   */
   /* and the MailboxData pointer that was specified appears semi-valid.*/
   if((Mailbox) && (MailboxData))
   {
      /* Next, we need to block until there is at least one Mailbox     */
      /* Slot occupied by waiting on the Data Available Event.          */
      if(BTPS_WaitEvent(((MailboxHeader_t *)Mailbox)->Event, BTPS_INFINITE_WAIT))
      {
         /* Since we are going to update the Mailbox, we need to acquire*/
         /* the Mutex that guards access to the Mailox.                 */
         if(BTPS_WaitMutex(((MailboxHeader_t *)Mailbox)->Mutex, BTPS_INFINITE_WAIT))
         {
            /* Let's double check to make sure that there does indeed   */
            /* exist at least one slot with Mailbox Data present in it. */
            if(((MailboxHeader_t *)Mailbox)->OccupiedSlots)
            {
               /* Flag success to the caller.                           */
               ret_val = TRUE;

               /* Now copy the Data into the Memory Buffer specified    */
               /* by the caller.                                        */
               BTPS_MemCopy(MailboxData, &((((unsigned char *)((MailboxHeader_t *)Mailbox)->Slots))[((MailboxHeader_t *)Mailbox)->TailSlot*((MailboxHeader_t *)Mailbox)->SlotSize]), ((MailboxHeader_t *)Mailbox)->SlotSize);

               /* Now that we've copied the data into the Memory Buffer */
               /* specified by the caller we need to mark the Mailbox   */
               /* Slot as free.                                         */
               if(++(((MailboxHeader_t *)Mailbox)->TailSlot) == ((MailboxHeader_t *)Mailbox)->NumberSlots)
                  ((MailboxHeader_t *)Mailbox)->TailSlot = 0;

               ((MailboxHeader_t *)Mailbox)->OccupiedSlots--;

               /* If there is NO more data available in this Mailbox    */
               /* then we need to flag it as such by Resetting the      */
               /* Mailbox Data available Event.                         */
               if(!((MailboxHeader_t *)Mailbox)->OccupiedSlots)
                  BTPS_ResetEvent(((MailboxHeader_t *)Mailbox)->Event);
            }
            else
            {
               /* Internal error, flag that there is NO Data present    */
               /* in the Mailbox (by resetting the Data Available       */
               /* Event) and return failure to the caller.              */
               BTPS_ResetEvent(((MailboxHeader_t *)Mailbox)->Event);

               ret_val = FALSE;
            }

            /* All finished with the Mailbox, so release the Mailbox    */
            /* Mutex that we currently hold.                            */
            BTPS_ReleaseMutex(((MailboxHeader_t *)Mailbox)->Mutex);
         }
         else
            ret_val = FALSE;
      }
      else
         ret_val = FALSE;
   }
   else
      ret_val = FALSE;
   
   wassert(ret_val);

   /* Return the result to the caller.                                  */
   return(ret_val);
}

   /* The following function is responsible for destroying a Mailbox    */
   /* that was created successfully via a successful call to the        */
   /* BTPS_CreateMailbox() function.  This function accepts as input    */
   /* the Mailbox Handle of the Mailbox to destroy.  Once this function */
   /* is completed the Mailbox Handle is NO longer valid and CANNOT be  */
   /* used.  Calling this function will cause all outstanding           */
   /* BTPS_WaitMailbox() functions to fail with an error.  The final    */
   /* parameter specifies an (optional) callback function that is called*/
   /* for each queued Mailbox entry.  This allows a mechanism to free   */
   /* any resources that might be associated with each individual       */
   /* Mailbox item.                                                     */
void BTPSAPI BTPS_DeleteMailbox(Mailbox_t Mailbox, BTPS_MailboxDeleteCallback_t MailboxDeleteCallback)
{
   /* Before proceeding any further make sure that the Mailbox Handle   */
   /* that was specified appears semi-valid.                            */
   if(Mailbox)
   {
      /* Mailbox appears semi-valid, so let's free all Events and       */
      /* Mutexes, perform any mailbox deletion callback actions, and    */
      /* finally free the Memory associated with the Mailbox itself.    */
      if(((MailboxHeader_t *)Mailbox)->Event)
         BTPS_CloseEvent(((MailboxHeader_t *)Mailbox)->Event);

      if(((MailboxHeader_t *)Mailbox)->Mutex)
         BTPS_CloseMutex(((MailboxHeader_t *)Mailbox)->Mutex);

      /* Check to see if a Mailbox Delete Item Callback was specified.  */
      if(MailboxDeleteCallback)
      {
         /* Now loop though all of the occupied slots and call the      */
         /* callback with the slot data.                                */
         while(((MailboxHeader_t *)Mailbox)->OccupiedSlots)
         {
            __BTPSTRY
            {
               (*MailboxDeleteCallback)(&((((unsigned char *)((MailboxHeader_t *)Mailbox)->Slots))[((MailboxHeader_t *)Mailbox)->TailSlot*((MailboxHeader_t *)Mailbox)->SlotSize]));
            }
            __BTPSEXCEPT(1)
            {
               /* Do Nothing.                                           */
            }

            /* Now that we've called back with the data, we need to     */
            /* advance to the next slot.                                */
            if(++(((MailboxHeader_t *)Mailbox)->TailSlot) == ((MailboxHeader_t *)Mailbox)->NumberSlots)
               ((MailboxHeader_t *)Mailbox)->TailSlot = 0;

            /* Flag that there is one less occupied slot.               */
            ((MailboxHeader_t *)Mailbox)->OccupiedSlots--;
         }
      }

      /* All finished cleaning up the Mailbox, simply free all          */
      /* memory that was allocated for the Mailbox.                     */
      BTPS_FreeMemory(Mailbox);
   }
}

   /* The following function is used to initialize the Platform module. */
   /* The Platform module relies on some static variables that are used */
   /* to coordinate the abstraction.  When the module is initially      */
   /* started from a cold boot, all variables are set to the proper     */
   /* state.  If the Warm Boot is required, then these variables need to*/
   /* be reset to their default values.  This function sets all static  */
   /* parameters to their default values.                               */
   /* * NOTE * The implementation is free to pass whatever information  */
   /*          required in this parameter.                              */
void BTPSAPI BTPS_Init(void *UserParam)
{
    corona_print("BTPS_Init: %p\n", UserParam);
    
   /* Initialize the callback function and IO mutex if the callback     */
   /* function was specified.                                           */
   if((UserParam) && (((BTPS_Initialization_t *)UserParam)->MessageOutputCallback))
   {
      /* Initialize the message callback function and IO mutex.         */
      MessageOutputCallback = ((BTPS_Initialization_t *)UserParam)->MessageOutputCallback;
      IOMutex = BTPS_CreateMutex(FALSE);
   }
   else
   {
      MessageOutputCallback = NULL;
      IOMutex = NULL;
   }

   /* Initialize the debug zone mask.                                   */
   DebugZoneMask = DEBUG_ZONES;

   /* Initialize the task tracking information.                         */
   DebugZoneMask     = DEBUG_ZONES;
   BTPS_MemInitialize(MQX_Task_List, 0, sizeof(MQX_Task_List));
}

   /* The following function is used to cleanup the Platform module.    */
void BTPSAPI BTPS_DeInit(void)
{
    int i;
    
    corona_print("BTPS_DeInit: invoked\n");
    
    /* Wait threads to exit */
    _watchdog_start(30000);
    while (1)
    {
        for (i = 0; i < MAX_NUMBER_SUPPORTED_TASKS; ++i)
            if (MQX_Task_List[i].TaskId != 0)
                break;
        if (i == MAX_NUMBER_SUPPORTED_TASKS)
            break;
        corona_print("BTPS_DeInit: waiting for task\n");
        _sched_yield();
    }
    _watchdog_stop();
    
    /* Remove the IO mutex if it exists.                                 */
    if(IOMutex)
    {
        BTPS_CloseMutex(IOMutex);
        IOMutex = NULL;
    }
    
    /* Invalidate the message output callback.                           */
    MessageOutputCallback = NULL;
}

   /* Write out the specified NULL terminated Debugging String to the   */
   /* Debug output.                                                     */
void BTPSAPI BTPS_OutputMessage(BTPSCONST char *DebugString, ...)
{
   va_list args;
   int     Length;

      /* Grab the I/O Mutex and send the Message over USB               */
   if((MessageOutputCallback) && (IOMutex) && (BTPS_WaitMutex(IOMutex, BTPS_INFINITE_WAIT) == TRUE))
   {
      /* Write out the Data.                                            */
      va_start(args, DebugString);
      Length = vsnprintf(MsgBuf, (sizeof(MsgBuf) - 1), DebugString, args);
      va_end(args);

      MessageOutputCallback(((Length < sizeof(MsgBuf)) ? Length : sizeof(MsgBuf)), MsgBuf);
      BTPS_ReleaseMutex(IOMutex);
   }
}

   /* The following function is used to set the Debug Mask that controls*/
   /* which debug zone messages get displayed.  The function takes as   */
   /* its only parameter the Debug Mask value that is to be used.  Each */
   /* bit in the mask corresponds to a debug zone.  When a bit is set,  */
   /* the printing of that debug zone is enabled.                       */
void BTPSAPI BTPS_SetDebugMask(unsigned long DebugMask)
{
   DebugZoneMask = DebugMask;
}

   /* The following function is a utility function that can be used to  */
   /* determine if a specified Zone is currently enabled for debugging. */
int BTPSAPI BTPS_TestDebugZone(unsigned long Zone)
{
   return(DebugZoneMask & Zone);
}

   /* The following function is responsible for displaying binary debug */
   /* data.  The first parameter to this function is the length of data */
   /* pointed to by the next parameter.  The final parameter is a       */
   /* pointer to the binary data to be  displayed.                      */
int BTPSAPI BTPS_DumpData(unsigned int DataLength, BTPSCONST unsigned char *DataPtr)
{
   int                    ret_val;
   char                   Buffer[80];
   char                  *BufPtr;
   char                  *HexBufPtr;
   Byte_t                 DataByte;
   unsigned int           Index;
   static BTPSCONST char  HexTable[] = "0123456789ABCDEF\r\n";
   static BTPSCONST char  Header1[]  = "       00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F  ";
   static BTPSCONST char  Header2[]  = " ------------------------------------------------------\r\n";

   /* Before proceeding any further, lets make sure that the parameters */
   /* passed to us appear semi-valid.                                   */
   if((DataLength > 0) && (DataPtr != NULL))
   {
      /* The parameters which were passed in appear to be at least      */
      /* semi-valid, next write the header out to the file.             */
      BTPS_OutputMessage((char *)Header1);
      BTPS_OutputMessage((char *)HexTable);
      BTPS_OutputMessage((char *)Header2);

      /* Limit the number of bytes that will be displayed in the dump.  */
      if(DataLength > MAX_DBG_DUMP_BYTES)
      {
         DataLength = MAX_DBG_DUMP_BYTES;
      }

      /* Now that the Header is written out, let's output the Debug     */
      /* Data.                                                          */
      BTPS_MemInitialize(Buffer, ' ', sizeof(Buffer));
      BufPtr    = Buffer + BTPS_SprintF(Buffer, " %05X ", 0);
      HexBufPtr = Buffer + sizeof(Header1)-1;
      for(Index = 1; Index <= DataLength; Index ++)
      {
         DataByte     = *DataPtr++;
         *BufPtr++    = HexTable[(Byte_t)(DataByte >> 4)];
         *BufPtr      = HexTable[(Byte_t)(DataByte & 0x0F)];
         BufPtr      += 2;

         /* Check to see if this is a printable character and not a     */
         /* special reserved character.  Replace the non-printable      */
         /* characters with a period.                                   */
         *HexBufPtr++ = (char)(((DataByte >= ' ') && (DataByte <= '~') && (DataByte != '\\') && (DataByte != '%'))?DataByte:'.');
         if(((Index % MAXIMUM_BYTES_PER_ROW) == 0) || (Index == DataLength))
         {
            /* We are at the end of this line, so terminate the data    */
            /* compiled in the buffer and send it to the output device. */
            *HexBufPtr++ = '\r';
            *HexBufPtr++ = '\n';
            *HexBufPtr   = 0;
            BTPS_OutputMessage(Buffer);

            if(Index != DataLength)
            {
               /* We have more to process, so prepare for the next line.*/
               BTPS_MemInitialize(Buffer, ' ', sizeof(Buffer));
               BufPtr    = Buffer + BTPS_SprintF(Buffer, " %05X ", Index);
               HexBufPtr = Buffer + sizeof(Header1)-1;
            }
            else
            {
               /* Flag that there is no more data.                      */
               HexBufPtr = NULL;
            }
         }
      }

      /* Check to see if there is partial data in the buffer.           */
      if(HexBufPtr)
      {
         /* Terminate the buffer and output the line.                   */
         *HexBufPtr++ = '\r';
         *HexBufPtr++ = '\n';
         *HexBufPtr   = 0;
         BTPS_OutputMessage(Buffer);
      }

      BTPS_OutputMessage("\r\n");

      /* Finally, set the return value to indicate success.             */
      ret_val = 0;
   }
   else
   {
      /* An invalid parameter was enterer.                              */
      ret_val = -1;
   }

   return(ret_val);
}

