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
/*
* $FileName: Task_Manager.c$
* $Version : $
* $Date    : May-20-2011$
*
* Comments: Handles all task management functions including mutithreaded support. 
*
*   
*
*END************************************************************************/

#include <mqx.h>
#include <mutex.h>
#include <lwevent.h>
#include "main.h"
#include <string.h>
#include <stdlib.h>

#include "throughput.h"

/*************************** GLOBALS **************************/
MUTEX_STRUCT task_mutex, mutex_tskcnt;
LWEVENT_STRUCT task_event, task_event1;
int_32 task_argc;
char_ptr task_argv[MAX_ARGC];
char arg[MAX_ARGC][MAX_STRLEN];
uint_8 hvac_init = 0, task_counter = 0;
uint_8 wps_in_progress = 0;
uint_8 number_streams = 0; 			//Counter to control number of streams

_enet_address   whistle_enet_address;
_enet_handle    whistle_handle;

#if USE_ATH_CHANGES
uint_8 wifi_connected_flag = 0;     //Flag to indicate WIFI connectivity status
#else
uint_8 wifi_connected_flag = 1;     //Flag to indicate WIFI connectivity status
#endif

/*************************** Function Declarations **************************/

int_32 worker_cmd_handler(int_32 argc, char_ptr argv[] );
int_32 worker_cmd_quit(int_32 argc, char_ptr argv[] );
static void WakeWorkerTask(void);


/************************************************************************
* NAME: incrementTaskCount
*
* DESCRIPTION: Increment task counter when a new command is handled
************************************************************************/
static void incrementTaskCount()
{
	_mutex_lock(&mutex_tskcnt);
	task_counter ++;
	_mutex_unlock(&mutex_tskcnt);
}


/************************************************************************
* NAME: decrementTaskCount
*
* DESCRIPTION: Decrement task counter when command handling is over
************************************************************************/
static void decrementTaskCount()
{
	_mutex_lock(&mutex_tskcnt);
	if(task_counter != 0)
		task_counter--;
	else
		printf("Error: decrementing task counter: counter already 0\n");
	
	_mutex_unlock(&mutex_tskcnt);
}



/************************************************************************
* NAME: DoPeriodic
*
* DESCRIPTION: Periodic WPS query. 
************************************************************************/
static void DoPeriodic(void)
{
#if USE_ATH_CHANGES
	wps_query(0);
#endif				
}


/************************************************************************
* NAME: worker_cmd_quit
*
* DESCRIPTION: Called when user enters "benchquit" 
************************************************************************/
int_32 worker_cmd_quit(int_32 argc, char_ptr argv[] )
{
	bench_quit = 1;
    return 0;
}


/************************************************************************
* NAME: worker_cmd_handler
*
* DESCRIPTION: Handles incoming commands from Shell task 
************************************************************************/
int_32 worker_cmd_handler(int_32 argc, char_ptr argv[] )
{
	int_32 str_len;
	int_32 ret=0;
	/* for now we don't use a queue because worker commands are coming from the user 
	 * shell and if commands start to back up then we just discard that most recent
	 */
	if(hvac_init){
		_mutex_lock(&task_mutex);
		
		if(task_counter < MAX_TASKS_ALLOWED){
			do{
				if(argc > MAX_ARGC){
					printf("Command discarded; too many arguments %d < %d\n", MAX_ARGC, argc);
					ret = 1;
					break;
				}
				
				task_argc = argc;
				while(argc){
					str_len = strlen(argv[task_argc-argc])+1;
					
					if(str_len > MAX_STRLEN){
						printf("Command discarded; argument too long %d < %d\n", MAX_STRLEN, str_len);
						break;
					}
					
					memcpy(task_argv[task_argc-argc], argv[task_argc - argc], str_len);
					
					argc--;
				}
				
				if(argc){
					ret = 1;
					break;
				}
			
				WakeWorkerTask();
				
			}while(0);	
		}
		else
		{
			printf("Only 2 simultaneous commands allowed\n");
			ret = 1;
		}
		_mutex_unlock(&task_mutex);
	}else{
		printf("worker thread not yet initialized\n");
	}
	
	return ret;
}



/************************************************************************
* NAME: DoWork
*
* DESCRIPTION: Calls functions corresponding to shell command. 
************************************************************************/
static void DoWork(void)
{
	if(strcmp(task_argv[0], "benchtx") == 0){
		
		if(wifi_connected_flag)
		{
			if(number_streams < MAX_STREAMS_ALLOWED)
			{
				number_streams++;
				tx_command_parser(task_argc, task_argv);
				number_streams--;
			}
			else
			{
				printf("ERROR: Only 2 simultaneous traffic streams allowed\n");
			}
		}			
		else
			printf("ERROR: No WiFi connection available, please connect to an Access Point\n");
		
	}else if(strcmp(task_argv[0], "benchrx") == 0){
		if(wifi_connected_flag)	
		{
			if(number_streams < MAX_STREAMS_ALLOWED)
			{
				number_streams++;
				rx_command_parser(task_argc, task_argv);
				number_streams--;
			}	
			else
			{
				printf("ERROR: Only 2 simultaneous traffic streams allowed\n");
			}
		}
		else
			printf("No WiFi connection available, please connect to an Access Point\n");
		
	}
#if USE_ATH_CHANGES
	else if(strcmp(task_argv[0], "wmiconfig") == 0){
		wmiconfig_handler(task_argc, task_argv);
	}else if(strcmp(task_argv[0], "iwconfig") == 0){
		wmi_iwconfig(task_argc, task_argv);
	}
#endif	
#if ENABLE_STACK_OFFLOAD 
    else if (strcmp(task_argv[0], "benchmode") == 0)
    {
    	if(number_streams == 0)
    		setBenchMode(task_argc, task_argv);
    	else
    		printf("Cannot change mode while test is running\n");
    }
#endif    
	else{
		printf("unknown cmd: %s\n", task_argv[0]);
	}	
}



/************************************************************************
* NAME: wmiconfig_Task1
*
* DESCRIPTION: Handler for 1st task, waits for Shell task to provide command 
************************************************************************/
void wmiconfig_Task1(uint_32 temp)
{
   	uint_32 flags = 0; 
   	   
	_lwevent_create(&task_event1, 1/* autoclear */);
		
	/* block for events from other tasks */
	for(;;){
		switch(_lwevent_wait_ticks(&task_event1, 0x01, TRUE, MSEC_HEARTBEAT))
		{
		case MQX_OK:
			/* check for new work */
			DoWork();
				decrementTaskCount();
			break;
		case LWEVENT_WAIT_TIMEOUT:
			/* perform periodic tasks */
		//	DoPeriodic();
		
			break;
		default:
			printf("worker task error\n");
			flags = 1;
			break;
		}	
	    
	    if(flags == 1){
	    	break;
	    }
	}
	_lwevent_destroy(&task_event1);
}

void whistle_initialize_networking(void) 
{
	int_32            error;

    /* runtime RTCS configuration */

	error = RTCS_create();
	
	if (error) {
		printf("RTCS_create failed %x \n", error);
	}
	
	ENET_get_mac_address (DEMOCFG_DEFAULT_DEVICE, 0, whistle_enet_address);

    error = ipcfg_init_device (DEMOCFG_DEFAULT_DEVICE, whistle_enet_address);
    
    if (error) {
    	printf("ipcfg_init_device %x\n", error);
    }

	/* ENET_initialize is already called from within ipcfg_init_device.  We call it again
	 * so that we can get a handle to the driver allowing this code to make direct driver
	 * calls as needed. */
	if((error = ENET_initialize (DEMOCFG_DEFAULT_DEVICE, whistle_enet_address, 0, &whistle_handle)) != ENETERR_INITIALIZED_DEVICE)
	{
		while(1)
		{

		}
	}

#if DEMOCFG_USE_WIFI
#if USE_ATH_CHANGES
	clear_wep_keys();
#endif	
#endif
	
}

/************************************************************************
* NAME: wmiconfig_Task2
*
* DESCRIPTION: Handler for 2nd task, waits for Shell task to provide command 
************************************************************************/
void wmiconfig_Task2(uint_32 temp)
{
   	uint_32 flags = 0, i=0; 
   	
   	_lwevent_create(&task_event, 1/* autoclear */);
   	_mutex_init(&task_mutex, NULL);
   	_mutex_init(&mutex_tskcnt, NULL);
        atheros_driver_setup();
        
   	whistle_initialize_networking();
   	
   	for(i=0 ; i<MAX_ARGC ; i++){
   		task_argv[i] = &arg[i][0];
   	}
	
	hvac_init = 1;

	_lwevent_create(&task_event, 1/* autoclear */);
		
	/* block for events from other tasks */

	for(;;){
		switch(_lwevent_wait_ticks(&task_event, 0x01, TRUE, MSEC_HEARTBEAT))
		{
		case MQX_OK:
			/* check for new work */
			DoWork();
				decrementTaskCount();
			break;
		case LWEVENT_WAIT_TIMEOUT:
			/* perform periodic tasks */
			DoPeriodic();
		
			break;
		default:
			printf("worker task error\n");
			flags = 1;
			break;
		}	
	    
	    if(flags == 1){
	    	break;
	    }
	}

	_lwevent_destroy(&task_event);
}




/************************************************************************
* NAME: WakeWorkerTask
*
* DESCRIPTION: Wakes task on incoming command
************************************************************************/
static void WakeWorkerTask(void)
{
	if(hvac_init){
	
		if(task_counter < MAX_TASKS_ALLOWED)	
		{
			incrementTaskCount();	
			switch(task_counter)
			{
				
				case 1:
				_lwevent_set(&task_event1, 0x01);
				break;
				
				case 2:
				_lwevent_set(&task_event, 0x01);
				break;							
				
				default:
					printf("Invalid task number\n");
			}
			
		}
	}
}





