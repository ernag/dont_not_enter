/*HEADER**********************************************************************
*
* Copyright 2008 Freescale Semiconductor, Inc.
* Copyright 2004-2008 Embedded Access Inc.
*
* This software is owned or controlled by Freescale Semiconductor.
* Use of this software is governed by the Freescale MQX RTOS License
* distributed with this Material.
* See the MQX_RTOS_LICENSE file distributed for more details.
*
* Brief License Summary:
* This software is provided in source form for you to use free of charge,
* but it is not open source software. You are allowed to use this software
* but you cannot redistribute it or derivative works of it in source form.
* The software may be used only in connection with a product containing
* a Freescale microprocessor, microcontroller, or digital signal processor.
* See license agreement file for full license terms including other
* restrictions.
*****************************************************************************
*
* Comments:
*
*   Example using RTCS Library.
*
*
*END************************************************************************/

#include <mqx.h>
#include <bsp.h>
#include <rtcs.h>
#include <enet.h>
#include <ipcfg.h>
#include "config.h"
#include "PWM1.h"

#if ! BSPCFG_ENABLE_IO_SUBSYSTEM
#error This application requires BSPCFG_ENABLE_IO_SUBSYSTEM defined non-zero in user_config.h. Please recompile BSP with this option.
#endif

#ifndef BSP_DEFAULT_IO_CHANNEL_DEFINED
#error This application requires BSP_DEFAULT_IO_CHANNEL to be not NULL. Please set corresponding BSPCFG_ENABLE_ITTYx to non-zero in user_config.h and recompile BSP with this option.
#endif

#define MAIN_TASK_INDEX 		1
#define RX_TASK_INDEX 			2
#define TX_TASK_INDEX 			3
#define SERVO_TASK_INDEX		4
#define CAP_TOUCH_TASK_INDEX 	5
#define BUTTON_TASK_INDEX		6

#define SOCKET_WELCOME_MESSAGE "\n*******************\nCAT DOOR OPENER INTERFACE\n*******************\n"
#define SERIAL_WELCOME_MESSAGE "Serial <-> Ethernet bridge\n\r"

#define RX_BUFFER_SIZE 128
#define TX_BUFFER_SIZE 128

#if 0
/*
 *     Protocol for communicating actions with others.
 */
Event/Action       | Sender    | Message      | Recipient(s)  | Recipient Action
=============================================================================================
Doorbell Rings     | Door      | st_ringing   | All phones    | Display notification, present door control
---------------------------------------------------------------------------------------------
User opens door    | Phone     | cmd_open     | Door          | Open
---------------------------------------------------------------------------------------------
Door opened        | Door      | st_opened    | All phones    | Display notification, update app UI
---------------------------------------------------------------------------------------------
User closes door   | Phone     | cmd_close    | Door          | Close
---------------------------------------------------------------------------------------------
Door closed        | Door      | st_closed    | All phones    | Display notification, update app UI
---------------------------------------------------------------------------------------------
User queries state | Phone     | cmd_state    | Door          | Send current state (st_ringing|st_opened|st_closed)
#endif

// States
#define STATE_RINGING	"st_ringing\n"
#define STATE_OPENED	"st_opened\n"
#define STATE_CLOSED	"st_closed\n"

static const char * g_current_state   = STATE_CLOSED;

// Commands
#define CMD_OPEN		"cmd_open"
#define CMD_CLOSE		"cmd_close"
#define CMD_STATE		"cmd_state"

LWEVENT_STRUCT g_event;
uint8_t g_fluffy_at_door = 0;

#define CHANGE_SERVO_EVENT		0x1

/* Structure with sockets used for listening */
typedef struct socket_s
{
    uint32_t sock6; // socket for IPv6
    uint32_t sock4; // socket for IPv4
}SOCKETS_STRUCT, * SOCKETS_STRUCT_PTR;

/* Paramter for tasks */
typedef struct t_param
{
    uint32_t          sock;
    LWSEM_STRUCT     lwsem;
}TASK_PARAMS, * TASK_PARAMS_PTR;

/*
** MQX initialization information
*/
void Main_task (uint32_t);
void rx_task (uint32_t param);
//void tx_task (uint32_t param);
void catdoor_task (uint32_t param);
void captouch_task(uint32_t param);
void button_task(uint32_t param);

#define CLOSE_DOOR		3900 //4000       // 4300 seems good for a close.
#define OPEN_DOOR		1400       // 1400 seems good for an open.

uint32_t   g_socket;

uint16_t g_servo_time = CLOSE_DOOR;

void broadcast_state(void);
void close_door(void);
void open_door(void);
void door_is_ringing(void);

void close_door(void)
{
	g_servo_time = CLOSE_DOOR;
	_lwevent_set(&g_event, CHANGE_SERVO_EVENT);
	
	g_current_state = STATE_CLOSED;
	broadcast_state();
}

void open_door(void)
{
	g_servo_time = OPEN_DOOR;
	_lwevent_set(&g_event, CHANGE_SERVO_EVENT);
	
	g_current_state = STATE_OPENED;
	broadcast_state();
}

void door_is_ringing(void)
{
	g_current_state = STATE_RINGING;
	broadcast_state();
}

void broadcast_state(void)
{
	send(g_socket, (char *)g_current_state ,strlen(g_current_state), 0);
	printf("BroadCasting State: %s\n", g_current_state);
}

const TASK_TEMPLATE_STRUCT MQX_template_list[] =
{
    /* Task Index,  Function,  Stack,  Priority,    Name,       Attributes,             Param,  Time Slice */
    {		MAIN_TASK_INDEX,      Main_task,           2500,     9,    "Bridge",   MQX_AUTO_START_TASK,    0,       0           },
    {		RX_TASK_INDEX,        rx_task,             2500,     10,   "rxtask",   0,                      0,       0           },
    //{		TX_TASK_INDEX,        tx_task,             2500,     10,   "txtask",   0,                      0,       0           },
    {		SERVO_TASK_INDEX,     catdoor_task,        3000,     10,    "servo",    MQX_AUTO_START_TASK,    0,       0           },
    {		CAP_TOUCH_TASK_INDEX, captouch_task,       3000,     8,    "captouch", MQX_AUTO_START_TASK,    0,   	0			},
    {		BUTTON_TASK_INDEX,    button_task,         1000,     10,   "button",   MQX_AUTO_START_TASK,    0,   	0			},
    
    
    {		0		}
};

void Main_task(uint32_t temp)
{
#if RTCSCFG_ENABLE_IP4
   IPCFG_IP_ADDRESS_DATA    ip_data;
   uint32_t                  ip4_addr = ENET_IPADDR;
#endif
   uint32_t                  error;
   sockaddr                 addr;
   _enet_address            enet_addr = ENET_MAC;
   uint32_t                  retval = 0;
   SOCKETS_STRUCT           sockets;
   uint32_t                  conn_sock;
   uint32_t                  client_sock;
   _task_id                 rx_tid;
   _task_id                 tx_tid;
   FILE_PTR                 ser_device;
   uint32_t                  option;
   uint32_t                  param[3];
   TASK_PARAMS              task_p;
#if RTCSCFG_ENABLE_IP6
    uint32_t                 n = 0;
    uint32_t                 i = 0;
    IPCFG6_GET_ADDR_DATA    data[RTCSCFG_IP6_IF_ADDRESSES_MAX];
    char prn_addr6[sizeof "ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255"];
#endif  
    
    _lwevent_create( &g_event, LWEVENT_AUTO_CLEAR);
    close_door();
    
#if 0
    _time_delay(30 * 1000);
    while(1)
    {
        open_door(0);
        _time_delay(30 * 1000);
        
        close_door(0);
        _time_delay(30 * 1000);
    }
#endif
    
   sockets.sock4 = 0;
   sockets.sock6 = 0;
   /* Initialize RTCS */
   error = RTCS_create();
   if (error != RTCS_OK)
   {
       fputs("Fatal Error: RTCS initialization failed.", stderr);
       _task_block();
   }
#if RTCSCFG_ENABLE_IP4   
   ip_data.ip = ENET_IPADDR;
   ip_data.mask = ENET_IPMASK;
   ip_data.gateway = ENET_IPGATEWAY;
#endif
   /* Initialize ethernet */  
   error = ipcfg_init_device(BSP_DEFAULT_ENET_DEVICE, enet_addr);
   if (error != IPCFG_OK)
   {
       fprintf(stderr, "Fatal Error 0x%X: Network device initialization failed.\n", error);
       _task_block();
   }
#if RTCSCFG_ENABLE_IP4
   /* Bind static IP address */
   error = ipcfg_bind_staticip(BSP_DEFAULT_ENET_DEVICE, &ip_data);
   if (error != IPCFG_OK)
   {
       fprintf(stderr, "Fatal Error 0x%X: IP address binding failed\n", error);
       _task_block();
   }
   /* Prepare IPv4 socket for incoming connections */
   sockets.sock4 = socket(PF_INET, SOCK_STREAM, 0);
   if (sockets.sock4 == RTCS_SOCKET_ERROR)
   {
       fputs("Error: Unable to create socket for IPv4 connections.", stderr);
   }
   else
   {
       ((sockaddr_in*) &addr)->sin_family = AF_INET;
       ((sockaddr_in*) &addr)->sin_port = DEMO_PORT;
       ((sockaddr_in*) &addr)->sin_addr.s_addr = INADDR_ANY;
       retval = bind(sockets.sock4, &addr, sizeof(addr));
       if (retval != RTCS_OK)
       {
           fprintf(stderr, "Error 0x%X: Unable to bind IPv4 socket.\n", retval);
       }
       else
       {
           retval = listen(sockets.sock4, 0);
           if (retval != RTCS_OK)
           {
               fprintf(stderr, "Error 0x%X: Unable to put IPv4 socket in listening state.\n", retval);
           }
       }
   }
#endif
   
#if RTCSCFG_ENABLE_IP6
   while(!ipcfg6_get_addr(BSP_DEFAULT_ENET_DEVICE, n, &data[n]))
   {
       n++;
   }
   /* Prepare IPv6 socket for incoming connections */
   sockets.sock6 = socket(PF_INET6, SOCK_STREAM, 0);
   if (sockets.sock6 == RTCS_SOCKET_ERROR)
   {
       fputs("Error: Unable to create socket for IPv6 connections.", stderr);
   }
   else
   {
       ((sockaddr_in6*) &addr)->sin6_family = AF_INET6;
       ((sockaddr_in6*) &addr)->sin6_port = DEMO_PORT;
       ((sockaddr_in6*) &addr)->sin6_addr = in6addr_any; /* Any address */
       ((sockaddr_in6*) &addr)->sin6_scope_id= 0; /* Any scope */
       retval = bind(sockets.sock6, &addr, sizeof(addr));
       if (retval != RTCS_OK)
       {
           fprintf(stderr, "Error 0x%X: Unable to bind IPv6 socket.\n", retval);
       }
       else
       {
           retval = listen(sockets.sock6, 0);
           if (retval != RTCS_OK)
           {
               fprintf(stderr, "Error 0x%X: Unable to put IPv6 socket in listening state.\n", retval);
           }
       }
   } 
#endif
   
   if (((sockets.sock6 == RTCS_SOCKET_ERROR) && (sockets.sock4 == RTCS_SOCKET_ERROR)) || (retval != RTCS_OK))
   {
       fputs("Fatal Error: No socket avaiable for incoming connections.", stderr);
       _task_block();
   }
   /* Print listening addresses */
   fputs("Application listening on following ip addresses: \n", stdout);
#if RTCSCFG_ENABLE_IP4
   fprintf(stdout, "  IPv4 Address: %d.%d.%d.%d, port:%d\n", IPBYTES(ip4_addr), DEMO_PORT);
#endif
#if RTCSCFG_ENABLE_IP6
   /* Print IP6 addresses to stdout */
   while(i < n)
   {
       if(inet_ntop(AF_INET6,&data[i].ip_addr, prn_addr6, sizeof(prn_addr6)))
       {
           fprintf(stdout, "  IPv6 Address %d: %s, port: %d\n", i++, prn_addr6, DEMO_PORT);
       }
   }
#endif
   /* Initialize semaphore for client disconnection */
   _lwsem_create(&task_p.lwsem, 0);   
   
   while (1)
   {
       /* Let's wait for activity on both IPv4 and IPv6 */
       fprintf(stdout, "\nWaiting for incoming connection\n");
       conn_sock = RTCS_selectset(&sockets, 2, 0);
       if (conn_sock == RTCS_SOCKET_ERROR)
       {
           fprintf(stdout, "FAILED\n\n");
           fputs("Fatal Error: Unable to determine active socket.", stderr);
           _task_block();
       }
       fprintf(stdout, "OK\nAccepting Connection\n");
       
       /* Accept incoming connection */
       client_sock = accept(conn_sock, NULL, NULL);
       if (client_sock == RTCS_SOCKET_ERROR)
       {
           uint32_t status;
           status = RTCS_geterror(client_sock);
           fputs("Fatal Error: Unable to accept incoming connection. ", stderr);
           if (status == RTCS_OK)
           {
               fputs("Connection reset by peer.", stderr);
           }
           else 
           {
               fprintf(stderr, "Accept() failed with error code 0x%X.\n", status);
           }
           _task_block();
       }
       
       /* Set socket options */
       option = TRUE;
       
       fprintf(stdout, "\nSet Sock: 0x%x\n", client_sock);
       retval = setsockopt(client_sock, SOL_TCP, OPT_RECEIVE_NOWAIT, &option, sizeof(option));
       if (retval != RTCS_OK)
       {
           fputs("Fatal Error: Unable to set socket options.", stderr);
           _task_block();
       }
       
#if 0
       /* Prepare serial device */
       ser_device = fopen(SERIAL_DEVICE, "w");
       if (ser_device == NULL)
       {
           fprintf(stderr, "Fatal Error: Unable to open device \"%s\".\n", SERIAL_DEVICE);
           _task_block();
       }
       ioctl(ser_device, IO_IOCTL_DEVICE_IDENTIFY, param);
       if (param[0] != IO_DEV_TYPE_PHYS_SERIAL_INTERRUPT)
       {
           fprintf(stderr, "Fatal Error: Device \"%s\" is not interrupt driven serial line.\n");
           fputs("This demo application requires enabled interrupt driven serial line (i.e \"ittyf:\").\n", stderr);
           fputs("Please define proper device using macro SERIAL_DEVICE and recompile this demo application.\n", stderr);
           fputs("Please add proper configuration to user_config.h\n", stderr);
           fputs("i.e #define BSPCFG_ENABLE_ITTYF 1\n #define BSP_DEFAULT_IO_CHANNEL_DEFINED\n #define BSP_DEFAULT_IO_CHANNEL \"ittyf:\"\n", stderr);
           _task_block();
       }
       
#endif
       /* Print welcome message to socket and serial */
       send(client_sock, SOCKET_WELCOME_MESSAGE, sizeof(SOCKET_WELCOME_MESSAGE), 0);
       
#if 0
       retval = fwrite(SERIAL_WELCOME_MESSAGE, 1, strlen(SERIAL_WELCOME_MESSAGE), ser_device);
       fclose(ser_device);
#endif
       
       /* Start receive and transmit task */
       task_p.sock = client_sock;
       fprintf(stdout, "\nStarting TX and RX tasks.\n");
       rx_tid = _task_create(0, RX_TASK_INDEX, (uint32_t) &task_p);
       if (rx_tid == MQX_NULL_TASK_ID)
       {
           fprintf(stderr, "Fatal Error 0x%X: Unable to create receive task.", _task_get_error());
           _task_block();
       }   
       
#if 0
       tx_tid = _task_create(0, TX_TASK_INDEX, (uint32_t) &task_p);
       if (tx_tid == MQX_NULL_TASK_ID)
       {
           fprintf(stderr, "Fatal Error 0x%X: Unable to create transmit task.", _task_get_error());
           _task_block();
       }
#endif
       
       /* Init is done, connection accepted. Let created tasks run and process data from/to socket and serial port */
       retval = _lwsem_wait(&task_p.lwsem);
       fputs("\nClient disconnected.", stdout);
       if (retval != MQX_OK)
       {
           fputs("Fatal Error: Wait for semaphore failed.", stderr);
           _task_block();
       }
       retval = _task_destroy(rx_tid);
       if (retval != MQX_OK)
       {
           fputs("Fatal Error: Unable to destroy rxtask.", stderr);
           _task_block();
       }
       retval = _task_destroy(tx_tid);
       if (retval != MQX_OK)
       {
           fputs("Fatal Error: Unable to destroy txtask.", stderr);
           _task_block();
       }
   }
}

typedef void (*cc_cmd_callback_t)(void);

typedef struct console_cmd
{
	char *cmd;
	char *desc;
	cc_cmd_callback_t cbk;
}console_cmd_t;

#define DOOR_GRANULARITY	(CLOSE_DOOR - OPEN_DOOR) / 10

void increase_door(void)
{
	g_servo_time = (g_servo_time + DOOR_GRANULARITY > CLOSE_DOOR)?CLOSE_DOOR:(g_servo_time+DOOR_GRANULARITY);
	_lwevent_set(&g_event, CHANGE_SERVO_EVENT);
}

void decrease_door(void)
{
	g_servo_time = (g_servo_time - DOOR_GRANULARITY < OPEN_DOOR)?OPEN_DOOR:(g_servo_time-DOOR_GRANULARITY);
	_lwevent_set(&g_event, CHANGE_SERVO_EVENT);
}

void print_menu(void);

console_cmd_t g_cmds[] = {  {"\n***********************************\n", "", increase_door},  // dummy entry.
							{"h", "\t\tprint help menu\n", print_menu},
							{"+", "\t\tincrease door\n", increase_door},
							{"-", "\t\tdecrease door\n", decrease_door},
							{CMD_OPEN, "\t\topen door\n", open_door},
							{CMD_CLOSE, "\t\tclose door\n", close_door},
							{CMD_STATE, "\t\topen door\n", broadcast_state},
							//{"lighton", "\t\tturn light on\n", lighton},
							//{"lightoff", "\tturn light off\n", lightoff},
							{"\n***********************************\n", "", increase_door},  // dummy entry
{0, 0, NULL},};

void print_menu(void)
{
	int i = 0;
	
	while(g_cmds[i].cbk != NULL)
	{
		printf("%s%s", g_cmds[i].cmd,  g_cmds[i].desc);
		//send(sock, g_cmds[i].cmd, strlen(g_cmds[i].cmd), 0);
		//send(sock, g_cmds[i].desc, strlen(g_cmds[i].desc), 0);
		i++;
	}
}

void rx_task (uint32_t param)
{
    FILE_PTR ser_device;
    char buff[RX_BUFFER_SIZE];
    uint32_t count = 0;
    TASK_PARAMS_PTR task_p = (TASK_PARAMS_PTR) param;
    int i, j;

#if 0
    /* Prepare serial port */
    ser_device = fopen(SERIAL_DEVICE, "w");    
    if (ser_device == NULL)
    {
        fprintf(stderr, "RXtask Fatal Error: Unable to open device %s.\n", SERIAL_DEVICE);
        _task_block();
    }
#endif
    
    g_socket = task_p->sock;
    
    print_menu();

    /* Endless loop for reading from socket and writing to serial port */
    while(1)
    {
    	
    WAIT_FOR_COMMAND:

    	i = 0;
    	j = 0;
    	memset(buff, 0, RX_BUFFER_SIZE);
    	
    	while(i < RX_BUFFER_SIZE)
    	{
    		AGAIN:
    		
    		count = recv(task_p->sock, &buff[i], RX_BUFFER_SIZE, 0);
    		
            if (count != RTCS_ERROR)
            {
            	if(buff[i] == 0)
            	{
            		_time_delay(10);
            		goto AGAIN;
            	}
            	
                if( (buff[i] == '\n') || (buff[i] == '\r') )
                {
                	printf("%s", buff);
                	buff[i] = 0;
                	goto CHECK_COMMAND;
                }
            }
            else
            {
                shutdown(task_p->sock, 0);
                _lwsem_post(&task_p->lwsem);
                printf("ERROR: SHUTTING DOWN...\n");
                _task_block();
            }
    		
            i++;
    	}
    	
    CHECK_COMMAND:
    	
    	while(g_cmds[j].cbk != NULL)
    	{
        	if (!strcmp(g_cmds[j].cmd, buff))
    		{
#if 0
        		send(task_p->sock, "CMD:\t", strlen("CMD:\t"), 0);
        		send(task_p->sock, buff, strlen(buff), 0);
        		send(task_p->sock, "\n", strlen("\n"), 0);
#endif
        		g_cmds[j].cbk();
        		goto WAIT_FOR_COMMAND;
    		}
        	
        	j++;
    	}
    	
#if 1
        count = send(task_p->sock, "invalid command\n\n", strlen("invalid command\n\n"), 0);
        if (count == RTCS_ERROR)
        {
            shutdown(task_p->sock, 0);
            _lwsem_post(&task_p->lwsem);
        }
#endif
        print_menu();

        /* Let the other task run */
        _time_delay(1);
    }
}

uint32_t g_captouch_rtc;
LWGPIO_STRUCT captouch_gpio, led_gpio;
LWGPIO_STRUCT button2_gpio, button3_gpio;    	// SW2 = PTC6     // SW3 = PTA4

void led_toggle(void)
{
	lwgpio_set_value( (LWGPIO_STRUCT_PTR) &led_gpio, 
			(lwgpio_get_value(&led_gpio) == LWGPIO_VALUE_LOW)?  LWGPIO_VALUE_HIGH:LWGPIO_VALUE_LOW );
}

static void captouch_isr(void *pointer)
{	
    // Clear the GPIO interrupt flag
    lwgpio_int_clear_flag(&captouch_gpio);
}

/*
 *      BUTTON TASK
 */
void button_task(uint32_t param)
{
	lwgpio_init(&button3_gpio, (GPIO_PORT_A | GPIO_PIN4), LWGPIO_DIR_INPUT, LWGPIO_VALUE_NOCHANGE);
	lwgpio_set_functionality(&button3_gpio, (LWGPIO_MUX_A4_GPIO));
	lwgpio_set_attribute(&button3_gpio, LWGPIO_ATTR_PULL_UP, LWGPIO_AVAL_DISABLE);

	lwgpio_init(&button2_gpio, (GPIO_PORT_C | GPIO_PIN6), LWGPIO_DIR_INPUT, LWGPIO_VALUE_NOCHANGE);
	lwgpio_set_functionality(&button2_gpio, (LWGPIO_MUX_C6_GPIO));
	lwgpio_set_attribute(&button2_gpio, LWGPIO_ATTR_PULL_UP, LWGPIO_AVAL_DISABLE);

	_time_delay(1000);

	while(1)
	{
		PORTA_PCR4 |= 0x2;
		PORTC_PCR6 |= 0x2;
		
		if( lwgpio_get_value(&button3_gpio) == LWGPIO_VALUE_LOW )
		{
			close_door();
		}
		else if(  lwgpio_get_value(&button2_gpio) == LWGPIO_VALUE_LOW )
		{
			open_door();
		}
		
		PORTA_PCR4 |= 0x2;
		PORTC_PCR6 |= 0x2;
		
		_time_delay(10000);
	}
}

void captouch_task(uint32_t param)
{	
	
#define PRESSED_CAPTOUCH_THRESH		(300)
	
	lwgpio_init(&captouch_gpio, (GPIO_PORT_C | GPIO_PIN14), LWGPIO_DIR_OUTPUT, LWGPIO_VALUE_LOW);
	lwgpio_set_functionality(&captouch_gpio, (LWGPIO_MUX_C14_GPIO));
	//lwgpio_set_attribute(&captouch_gpio, LWGPIO_ATTR_PULL_DOWN, LWGPIO_AVAL_DISABLE);
	
	lwgpio_init(&led_gpio, (GPIO_PORT_C | GPIO_PIN4), LWGPIO_DIR_OUTPUT, LWGPIO_VALUE_LOW);
	lwgpio_set_functionality(&led_gpio, (LWGPIO_MUX_C4_GPIO));
	
	//lwgpio_int_init(&captouch_gpio, LWGPIO_INT_MODE_RISING);
	//_int_install_isr(lwgpio_int_get_vector(&captouch_gpio), captouch_isr, (void *) &captouch_gpio);
	
	//lwgpio_int_enable(&captouch_gpio, TRUE);
	
	/*
	 *   We're using PTB10 as an input, and pulling up by default.
	 *   The K60 board will force it down when it detects prox.
	 */
	PORTB_PCR10 = PORT_PCR_MUX(0x01) | PORT_PCR_PS_MASK | PORT_PCR_PE_MASK;  // pull it up.
	GPIOB_PDDR &= ~(1 << 10);
	
	while(1)
	{
		uint32_t counts = 0;
		
		/*
		 *  PTC 14 = CAP TOUCH GPIO.
		   * (set output and set low)
		   * set as input with rising edge interrupt.
		   * read RTC.
		   * when you get the interrupt, take the difference. */
		//lwgpio_set_direction(&captouch_gpio, LWGPIO_DIR_OUTPUT);
		//lwgpio_set_value( (LWGPIO_STRUCT_PTR) &captouch_gpio, LWGPIO_VALUE_LOW );
		
#if 1
		GPIOC_PDDR |= 0x4000;  // make it an output.
		GPIOC_PCOR |= 0x4000;  // clear the signal.
		
		_time_delay(2 * 1000);
#endif
		GPIOC_PDDR &= ~0x4000;  // make it an input.
		while( (GPIOC_PDIR & 0x4000) == 0 ) 
		{
			counts++;
		}
#if 1
		if(counts > PRESSED_CAPTOUCH_THRESH)
		{
			uint32_t count = 0;
			
			door_is_ringing();
			lwgpio_set_value( (LWGPIO_STRUCT_PTR) &led_gpio, LWGPIO_VALUE_HIGH);
			printf("cap touch detected\n");
			
#if 0
			_task_stop_preemption();
			count = send(g_socket, pStr ,strlen(pStr), 0);
			if(count)
			{
				count++;
			}
			_task_start_preemption();
#else
			
#endif
		}
		else
		{
			lwgpio_set_value( (LWGPIO_STRUCT_PTR) &led_gpio, LWGPIO_VALUE_LOW);
		}
#endif	
		
		/*
		 *   Now check the GPIO from the Whistle Proto board to see if there is nearby prox detection.
		 *   J4.6 (white)   PTB10 from FRDM board is the input to check.
		 */
		if( 0 == (GPIOB_PDIR & (1 << 10)) )
		{
			door_is_ringing();
			lwgpio_set_value( (LWGPIO_STRUCT_PTR) &led_gpio, LWGPIO_VALUE_HIGH);
			printf("prox detected\n");
		}
		
		_time_delay(10 * 1000);
	}
}

/*
 *   Open or close the catdoor.
 */
void catdoor_task (uint32_t param)
{ 
	uint32_t ptr[100];
	uint32_t mask;
	
	while(1)
	{
		uint32_t i = 0;
		
		//_lwevent_clear(&g_event, CHANGE_SERVO_EVENT);
		
        _lwevent_wait_ticks(&g_event, CHANGE_SERVO_EVENT, FALSE, 0);
        mask = _lwevent_get_signalled();

        _task_stop_preemption();
        
        PWM1_Init((LDD_TDeviceData *)ptr);
		PWM1_SetDutyUS((LDD_TDeviceData *)ptr, g_servo_time);
		
		/*
		 *   Try to keep door from making so much racket.
		 */
		if( CLOSE_DOOR == g_servo_time )
		{
			_time_delay(1000);
			PWM1_SetDutyUS((LDD_TDeviceData *)ptr, g_servo_time-DOOR_GRANULARITY);
			
			_time_delay(1000);
			PWM1_SetDutyUS((LDD_TDeviceData *)ptr, g_servo_time-DOOR_GRANULARITY);
			
			_time_delay(1000);
			PWM1_SetDutyUS((LDD_TDeviceData *)ptr, g_servo_time+DOOR_GRANULARITY);
			
			_time_delay(1000);
			PWM1_SetDutyUS((LDD_TDeviceData *)ptr, g_servo_time+DOOR_GRANULARITY);
		}
		
		_task_start_preemption();
	}
}

void tx_task (uint32_t param)
{ 
    FILE_PTR ser_device;
    char buff[TX_BUFFER_SIZE];
    uint32_t ser_opts = IO_SERIAL_NON_BLOCKING;
    uint32_t retval = 0;
    TASK_PARAMS_PTR task_p = (TASK_PARAMS_PTR) param;
    
#if 0
    /* Prepare serial port */
    ser_device = fopen(SERIAL_DEVICE, "r");    
    if (ser_device == NULL)
    {
        fprintf(stderr, "TXtask Fatal Error: Unable to open device %s.\n", SERIAL_DEVICE);
        _task_block();
    }
    
    
    /* Set non-blocking mode */
    retval = ioctl(ser_device, IO_IOCTL_SERIAL_SET_FLAGS, &ser_opts);
    if (retval != MQX_OK)
    {
        fputs("TXtask Fatal Error: Unable to setup non-blocking mode.", stderr);
        _task_block();
    }
    
#endif
    
#if 0
    /* Endless loop for reading from serial and writing to socket */
    while(1)
    {
		uint32_t count = 0;

		if(g_fluffy_at_door)
		{
	        count = send(task_p->sock, "@", strlen("@"), 0);
	        if (count == RTCS_ERROR)
	        {
	            shutdown(task_p->sock, 0);
	            _lwsem_post(&task_p->lwsem);
	        }
			g_fluffy_at_door = 0;
		}

        /* Let the other task run */ 
        _time_delay(10 * 1000);
    }
#endif
}

