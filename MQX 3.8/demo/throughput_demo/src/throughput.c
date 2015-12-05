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

#include "main.h"
#include <string.h>
#include <stdlib.h>

#include "throughput.h"
#include <atheros_stack_offload.h>
#include "atheros_wifi.h"
#include "atheros_wifi_api.h"



#if ENABLE_STACK_OFFLOAD

/**************************Globals *************************************/

static A_UINT32 num_traffic_streams = 0;        //Counter to monitor simultaneous traffic streams
static A_UINT32 port_in_use = 0;		//Used to prevent two streams from using same port

/************************************************************************
*     Definitions.
*************************************************************************/


/*TCP receive timeout*/
const int rx_timeout = ATH_RECV_TIMEOUT;          

#define CFG_COMPLETED_STR    "IOT Throughput Test Completed.\n"
#define CFG_REPORT_STR "."


unsigned char v6EnableFlag =  0;


extern int Inet6Pton(char * src, void * dst);
/*************************** Function Declarations **************************/
static void ath_tcp_tx (THROUGHPUT_CXT *p_tCxt);
static void ath_udp_tx (THROUGHPUT_CXT *p_tCxt);
uint_32 check_test_time(THROUGHPUT_CXT *p_tCxt);
void sendAck(THROUGHPUT_CXT *p_tCxt, A_VOID * address, uint_16 port);
static void ath_udp_rx (THROUGHPUT_CXT *p_tCxt);
static void ath_tcp_rx (THROUGHPUT_CXT *p_tCxt);
int_32 wait_for_response(THROUGHPUT_CXT *p_tCxt, SOCKADDR_T foreign_addr, SOCKADDR_6_T foreign_addr6);
int_32 handle_mcast_param(THROUGHPUT_CXT *p_tCxt);


A_UINT8 bench_quit= 0;



/************************************************************************
* NAME: test_for_quit
*
* DESCRIPTION: 
* Parameters: none
************************************************************************/
int_32 test_for_quit(void)
{
	return bench_quit;
}

A_INT32 setBenchMode(A_INT32 argc, char_ptr argv[] )
{
	if(argc < 2)
	{
		printf("Missing Parameters, specify v4 or v6\n");
		return A_ERROR;
	}
		
	
	if(strcmp(argv[1],"v4")==0)
	{
		v6EnableFlag = 0;
	}
	else if(strcmp(argv[1],"v6")==0)
	{
		v6EnableFlag = 1;
	}
	else
	{
		printf("Invalid parameter, specify v4 or v6\n");
		return A_ERROR;
	}
	return A_OK;
}





/************************************************************************
* NAME: print_test_results
*
* DESCRIPTION: Print throughput results 
************************************************************************/
static void print_test_results(THROUGHPUT_CXT *p_tCxt)
{
    /* Print throughput results.*/
    A_UINT32 throughput = 0;
    A_UINT32 total_bytes = 0;
    A_UINT32 total_kbytes = 0;
    A_UINT32 sec_interval = (p_tCxt->last_time.SECONDS - p_tCxt->first_time.SECONDS);                 
    A_UINT32 ms_interval = (p_tCxt->last_time.MILLISECONDS - p_tCxt->first_time.MILLISECONDS); 
    A_UINT32 total_interval = sec_interval*1000 + ms_interval;
    
    p_tCxt->last_interval = total_interval;

    if(total_interval > 0)
    {
    	/*No test was run, or test is terminated, print results for previous test*/
    	if(p_tCxt->bytes == 0 && p_tCxt->kbytes == 0)
    	{
    	    throughput = (p_tCxt->last_kbytes*1024*8)/(total_interval) + (p_tCxt->last_bytes*8)/(total_interval);
    	    total_bytes = p_tCxt->last_bytes;
            total_kbytes =  p_tCxt->last_kbytes;
            
            /*Reset previous test data*/
            p_tCxt->last_bytes = 0;
            p_tCxt->last_kbytes = 0;
    	}
    	
    	else
    	{    		    	
    	    throughput = (p_tCxt->kbytes*1024*8)/(total_interval) + (p_tCxt->bytes*8)/(total_interval);
    	    total_bytes = p_tCxt->bytes;
            total_kbytes = p_tCxt->kbytes;
    	    p_tCxt->last_bytes = p_tCxt->bytes;
            p_tCxt->last_kbytes = p_tCxt->kbytes;
    	}
    }    	
    else{ 	
 	throughput = 0;   
    }
    
    switch(p_tCxt->test_type)
    {
    	case UDP_TX:
    	printf("\nResults for %s test:\n\n", "UDP Transmit"); 
    	break;
    	
    	case UDP_RX:
    	printf("\nResults for %s test:\n\n", "UDP Receive");
    	break;
    	
    	case TCP_TX:
    	printf("\nResults for %s test:\n\n", "TCP Transmit");
    	break;
    	
    	case TCP_RX:
    	printf("\nResults for %s test:\n\n", "TCP Receive");
    	break;
    }
          
    printf("\t%d KBytes %d bytes in %d seconds %d ms  \n\n", total_kbytes, total_bytes,total_interval/1000, total_interval%1000); 
   	printf("\t throughput %d kb/sec\n", throughput);
}






/************************************************************************
* NAME: tx_command_parser
*
* DESCRIPTION: 
************************************************************************/
A_INT32 tx_command_parser(A_INT32 argc, char_ptr argv[] )
{ /* Body */
    boolean           print_usage=0;
    A_INT32            return_code = A_OK;
    THROUGHPUT_CXT     tCxt;
    A_INT32            retval = -1; 
    A_INT32 max_packet_size = 0;

     memset(&tCxt,0,sizeof(THROUGHPUT_CXT));    
    if(argc != 8) 
    {
        /*Incorrect number of params, exit*/
        return_code = A_ERROR;
        print_usage = TRUE;
    }       
    else
    {    
        if(v6EnableFlag)
        {
            /*Get IPv6 address of Peer*/
            retval =  Inet6Pton(argv[1], tCxt.tx_params.v6addr); /* server IP*/
            if(retval == 0)
              retval = -1;
        }
        else
        {
            /*Get IPv4 address of Peer*/
            retval = ath_inet_aton(argv[1], (A_UINT32*) &tCxt.tx_params.ip_address);
        }
        if(retval == -1) 
        {                      
                           
            /* Port.*/
            tCxt.tx_params.port = atoi(argv[2]);
            
            /*Check port validity*/
            if(port_in_use == tCxt.tx_params.port)
            {
            printf("This port is in use, use another Port\n");
            return_code = A_ERROR;
            goto ERROR;
            }                            
            
            port_in_use = tCxt.tx_params.port;
                        
            /* Packet size.*/
            tCxt.tx_params.packet_size = atoi(argv[4]);
            if(!v6EnableFlag)
            max_packet_size = CFG_PACKET_SIZE_MAX_TX;
            else
            max_packet_size = CFG_MAX_IPV6_PACKET_SIZE;
            
            if ((tCxt.tx_params.packet_size == 0) || (tCxt.tx_params.packet_size > max_packet_size))
            {
                if(!v6EnableFlag)
                printf("Invalid packet length for v4, allowed %d\n",max_packet_size); /* Print error mesage. */
                else
                printf("Invalid packet length for v6, allowed %d\n",max_packet_size); /* Print error mesage. */
                    
                return_code = A_ERROR;
                print_usage = TRUE;
                goto ERROR;
            }
            
            /* Test mode.*/                
            tCxt.tx_params.test_mode = atoi(argv[5]);
            
            if(tCxt.tx_params.test_mode != 0 && tCxt.tx_params.test_mode != 1)
            {
                printf("Invalid test mode, please enter 0-time or 1-number of packets\n"); /* Print error mesage. */
                return_code = A_ERROR;
                print_usage = TRUE;
                goto ERROR;	
            }
                                      
            /* Number of packets OR time period.*/                                              
            if(tCxt.tx_params.test_mode == 0)
            {
                tCxt.tx_params.tx_time = atoi(argv[6]);
            }
            else if (tCxt.tx_params.test_mode == 1)
            {
                tCxt.tx_params.packet_number = atoi(argv[6]);
            }
                                   
            /* Inter packet interval for Bandwidth control*/            
            tCxt.tx_params.interval = atoi(argv[7]);           
                
            /* TCP */
            if(strcasecmp("tcp", argv[3]) == 0) 
            {
              ath_tcp_tx (&tCxt);
            }
            /* UDP */
            else if(strcasecmp("udp", argv[3]) == 0) 
            {
               ath_udp_tx (&tCxt);
            }
            else
            {
                printf("Invalid protocol %s\n", argv[3]);
            }                    
        }
        else
        {
            printf("Incorrect Server IP address %s\n", argv[1]);   /* Wrong throughput Server IP address. */
            return_code = A_ERROR;
            goto ERROR;            
        }                   
    }
ERROR:   
    if (print_usage)  
    {
       printf("%s <Rx IP> <port> <tcp|udp> <msg size> <test mode> <number of packets | time (sec)> <delay in msec> \n", argv[0]);
       printf("               where-   test mode = 0  - enter time period in sec\n");
       printf("                        test mode = 1  - enter number of packets\n\n");

    }
    port_in_use = 0;
    return return_code;
} /* Endbody */




/************************************************************************
* NAME: rx_command_parser
*
* DESCRIPTION: parses parameters of receive command (both TCP & UDP)
************************************************************************/
A_INT32 rx_command_parser(A_INT32 argc, char_ptr argv[] )
{ /* Body */
    boolean           print_usage = 0, shorthelp = FALSE;
    A_INT32            return_code = A_OK;
    THROUGHPUT_CXT     tCxt;
    
    memset(&tCxt, 0, sizeof(THROUGHPUT_CXT));    

    if (!print_usage)
    {
        if (argc < 3) 
        {
            printf("Incomplete parameters\n");
            return_code = A_ERROR;
            print_usage = TRUE;
        } 
        else
        { 
        	
            tCxt.rx_params.port = atoi(argv[2]);
            
            /*Check port validity*/
            if(port_in_use ==  tCxt.rx_params.port)
            {
               printf("This port is in use, use another Port\n");
               return A_ERROR;
            }
             port_in_use = tCxt.rx_params.port;
            if(argc > 3)
            {
                if(argc == 5)
                {
                      //Multicast address is provided
                      if(!v6EnableFlag)
                      {
                          ath_inet_aton(argv[3], (A_UINT32 *) & tCxt.rx_params.group.imr_multiaddr);
                          ath_inet_aton(argv[4], (A_UINT32 *) & tCxt.rx_params.group.imr_interface);
                      }
                      else
                      {
                          Inet6Pton(argv[3], tCxt.rx_params.group6.ipv6mr_multiaddr.addr);
                          Inet6Pton(argv[4], tCxt.rx_params.group6.ipv6mr_interface.addr);
                          tCxt.rx_params.v6mcastEnabled = 1; 	    
                      }
                }
                else
                {
                      printf("ERROR: Incomplete Multicast Parameters provided\n");
                      return_code = A_ERROR;
                      goto ERROR;
                }
            }
			
            if((argc == 1)||(strcasecmp("tcp", argv[1]) == 0))
            {
                ath_tcp_rx (&tCxt);
            }
            else if((argc == 1)||(strcasecmp("udp", argv[1]) == 0))
            {
                ath_udp_rx (&tCxt);
            }
            else
            {
                printf("Invalid parameter\n"); 
            }
        }
    }
ERROR:  
    if (print_usage)  
    {
        if (shorthelp)  
        {
            printf("%s [tcp|udp] <port>\n", argv[0]);
        } 
        else 
        {
            printf("Usage: %s [tcp|udp] <port> (multicast address*> <local address*>\n", argv[0]);
            printf("   tcp  = TCP receiver\n");
            printf("   udp  = UDP receiver\n");
            printf("   port = receiver port\n");
            printf("   multicast address  = IP address of multicast group to join (optional)\n");
            printf("   local address      = IP address of interface (optional)\n");
        }
    }
    
    port_in_use = 0;
    
    return return_code;
} /* Endbody */














////////////////////////////////////////////// TX //////////////////////////////////////

/************************************************************************
* NAME: ath_tcp_tx
*
* DESCRIPTION: Start TCP Transmit test. 
************************************************************************/
static void ath_tcp_tx (THROUGHPUT_CXT *p_tCxt)
{
    SOCKADDR_T local_addr;
    SOCKADDR_T foreign_addr;
    char ip_str[16];
    SOCKADDR_6_T foreign_addr6;
    char ip6_str[48];      
    TIME_STRUCT start_time,current_time,block_time;
    A_UINT32 packet_size = p_tCxt->tx_params.packet_size;
    A_UINT32 cur_packet_number;
    A_UINT32 next_prog_report, prog_report_inc;
    A_UINT32 buffer_offset;
    A_INT32 send_result;
    _ip_address temp_addr;
	
    //init quit flag
    bench_quit = 0;
    num_traffic_streams++;
    
    if(packet_size > CFG_PACKET_SIZE_MAX_TX) /* Check max size.*/
         packet_size = CFG_PACKET_SIZE_MAX_TX;
	    

    
    /* ------ Start test.----------- */
    printf("**********************************************************\n");
    printf("IOT TCP TX Test\n" );
    printf("**********************************************************\n");
 
    if(!v6EnableFlag)
    { 
        temp_addr = LONG_BE_TO_HOST(p_tCxt->tx_params.ip_address);   
        printf("Remote IP addr. %s\n", inet_ntoa(*(A_UINT32 *)( &temp_addr), ip_str));
    }
    else
    {
        printf("Remote IP addr. %s\n", inet6_ntoa((char *)p_tCxt->tx_params.v6addr, (void *)ip6_str));
    }
    printf("Remote port %d\n", p_tCxt->tx_params.port);
    printf("Message size%d\n", p_tCxt->tx_params.packet_size);
    printf("Number of messages %d\n", p_tCxt->tx_params.packet_number);

    printf("Type benchquit to cancel\n");
    printf("**********************************************************\n");    

    if(p_tCxt->tx_params.test_mode == TIME_TEST)
    {
        _time_get(&start_time);	    	
        prog_report_inc = p_tCxt->tx_params.tx_time/20;
        
        if(prog_report_inc==0)
            prog_report_inc = 1;
        
        next_prog_report = start_time.SECONDS + prog_report_inc;
    }else if(p_tCxt->tx_params.test_mode == PACKET_TEST){
        /* generate 20 progress characters across screen to provide progress feedback */
        prog_report_inc = p_tCxt->tx_params.packet_number/20;
        
        if(prog_report_inc==0)
                prog_report_inc = 1;
        
        next_prog_report = prog_report_inc;
    }
    /* Create socket */
    if(!v6EnableFlag)
    {
        if((p_tCxt->sock_peer = t_socket((void*)whistle_handle, ATH_AF_INET, SOCK_STREAM_TYPE, 0)) == A_ERROR)
        {
            printf("ERROR: Unable to create socket\n");
            goto ERROR_1;
        }
    }
    else
    {
        if((p_tCxt->sock_peer = t_socket((void*)whistle_handle, ATH_AF_INET6, SOCK_STREAM_TYPE, 0)) == A_ERROR)
        {
            printf("ERROR: Unable to create socket\n");
            goto ERROR_1;
        }
    }

    /*Allow small delay to allow other thread to run*/
    _time_delay(TX_DELAY);
              
    printf("Connecting.\n");
    
    if(!v6EnableFlag)
    {
      memset(&foreign_addr, 0, sizeof(local_addr));
      foreign_addr.sin_addr = p_tCxt->tx_params.ip_address;
      foreign_addr.sin_port = p_tCxt->tx_params.port;    
      foreign_addr.sin_family = ATH_AF_INET;
                   
      /* Connect to the server.*/
      if(t_connect((void*)whistle_handle, p_tCxt->sock_peer, (&foreign_addr), sizeof(foreign_addr)) == A_ERROR)
      {
         printf("Connection failed.\n");
         goto ERROR_2;
      } 
    }
    else
    {
      memset(&foreign_addr6, 0, sizeof(foreign_addr6));
      memcpy(&foreign_addr6.sin6_addr, p_tCxt->tx_params.v6addr,16);;
      foreign_addr6.sin6_port = p_tCxt->tx_params.port;    
      foreign_addr6.sin6_family = ATH_AF_INET6;
                   
      /* Connect to the server.*/
      if(t_connect((void*)whistle_handle, p_tCxt->sock_peer, (SOCKADDR_T *)(&foreign_addr6), sizeof(foreign_addr6)) == A_ERROR)
      {
         printf("Connection failed.\n");
         goto ERROR_2;
      } 
    }

        /* Sending.*/
        printf("Sending.\n"); 
        
        /*Reset all counters*/
        p_tCxt->bytes = 0;
        p_tCxt->kbytes = 0;
        p_tCxt->last_bytes = 0;
        p_tCxt->last_kbytes = 0;
        cur_packet_number = 0;
        buffer_offset = 0;
        
        _time_get_elapsed(&p_tCxt->first_time);
        _time_get_elapsed(&p_tCxt->last_time);     
        block_time = p_tCxt->first_time;

        _time_delay(20);

        while(1)
        {
            if(test_for_quit()){
                    break;
            }
            
            while((p_tCxt->buffer = CUSTOM_ALLOC(packet_size)) == NULL)  
            {
                //Wait till we get a buffer 
                if(test_for_quit()){
                    goto ERROR_2;
                }         
                /*Allow small delay to allow other thread to run*/
                _time_delay(TX_DELAY);
            }
        	
            
            if(!v6EnableFlag)
            {
               send_result = t_send((void*)whistle_handle, p_tCxt->sock_peer, (unsigned char*)(p_tCxt->buffer), packet_size, 0);
            }
            else
            {
               send_result = t_send((void*)whistle_handle, p_tCxt->sock_peer, (unsigned char*)(p_tCxt->buffer), packet_size, 0);            				
            }
#if !NON_BLOCKING_TX          
            /*Free the buffer only if NON_BLOCKING_TX is not enabled*/
            if(p_tCxt->buffer)
                CUSTOM_FREE(p_tCxt->buffer);
#endif 
            _time_get_elapsed(&p_tCxt->last_time);
           
            if(test_for_delay(&p_tCxt->last_time, &block_time)){
            	/* block to give other tasks an opportunity to run */
            	_time_delay(TX_DELAY);
            	_time_get_elapsed(&block_time);
            }
            
            /****Bandwidth control- add delay if user has specified it************/
            if(p_tCxt->tx_params.interval)
            	_time_delay(p_tCxt->tx_params.interval);
          
            if(send_result == A_ERROR)
            {              
                printf("send packet error = %d\n", send_result);
               // resetTarget();
                goto ERROR_2;
            }
            else if(send_result == A_SOCK_INVALID )
            {
               /*Socket has been closed by target due to some error, gracefully exit*/
               printf("Socket closed unexpectedly\n");
               break;
            }
            else if(send_result)
            {
                p_tCxt->bytes += send_result;                
                p_tCxt->sent_bytes += send_result;
                buffer_offset += send_result;
                
                if(buffer_offset == packet_size)
                {
                    cur_packet_number ++;
                    buffer_offset = 0;
                }
                
                if(p_tCxt->tx_params.test_mode == PACKET_TEST)
                {
                	/*Test based on number of packets, check if we have reached specified limit*/
                	if(cur_packet_number >= next_prog_report){
                		printf(".");
                		next_prog_report += prog_report_inc;
                	}
                
	                if((cur_packet_number >= p_tCxt->tx_params.packet_number))
	                { 
	                    /* Test completed, print throughput results.*/
	                    break;
	                }
                }                
                else if(p_tCxt->tx_params.test_mode == TIME_TEST)
                {
                    /*Test based on time interval, check if we have reached specified limit*/
                    _time_get(&current_time);
                    
                    if(current_time.SECONDS >= next_prog_report){
                            printf(".");
                            next_prog_report += prog_report_inc;
                    }                	                	
                    
                    if(check_test_time(p_tCxt))
                    {
                        /* Test completed, print test results.*/   
                        
                        break;
                    }	                
                }
            }
        }
		
    ERROR_2:
    	printf("\n"); // new line to separate from progress line 
        p_tCxt->kbytes = p_tCxt->bytes/1024;
        p_tCxt->bytes = p_tCxt->bytes % 1024;
    	p_tCxt->test_type = TCP_TX;
    	print_test_results(p_tCxt);
        if(send_result != A_SOCK_INVALID )
          t_shutdown((void*)whistle_handle, p_tCxt->sock_peer);

ERROR_1:
    printf(CFG_COMPLETED_STR);    
	
	num_traffic_streams--;
}






/************************************************************************
* NAME: ath_udp_tx
*
* DESCRIPTION: Start TX UDP throughput test. 
************************************************************************/
static void ath_udp_tx (THROUGHPUT_CXT *p_tCxt)
{
	char ip_str[16];
	TIME_STRUCT start_time,current_time,block_time;
	A_UINT32 next_prog_report, prog_report_inc;
	SOCKADDR_T foreign_addr;
	SOCKADDR_T local_addr;	
        SOCKADDR_6_T foreign_addr6;
        SOCKADDR_6_T local_addr6;	
        char ip6_str [48];
	A_UINT16 retry_counter = MAX_END_MARK_RETRY;
	A_INT32 send_result,result;
	A_UINT32 packet_size = p_tCxt->tx_params.packet_size;
	A_UINT32 cur_packet_number;
	_ip_address temp_addr;

	bench_quit = 0;
		
	if(packet_size > CFG_PACKET_SIZE_MAX_TX) /* Check max size.*/
	     packet_size = CFG_PACKET_SIZE_MAX_TX;
		 
 	temp_addr = LONG_BE_TO_HOST(p_tCxt->tx_params.ip_address);

		 	
    /* ------ Start test.----------- */
    printf("****************************************************************\n");
    printf(" UDP TX Test\n" );
    printf("****************************************************************\n");
    if(!v6EnableFlag)
    {
      printf( "Remote IP addr. %s\n", inet_ntoa(*(A_UINT32 *)( &temp_addr), ip_str));
    }
    else
    {
      printf( "Remote IP addr. %s\n", inet6_ntoa((char *)p_tCxt->tx_params.v6addr, ip6_str));
    }
    printf("Remote port %d\n", p_tCxt->tx_params.port);
    printf("Message size %d\n", p_tCxt->tx_params.packet_size);
    printf("Number of messages %d\n", p_tCxt->tx_params.packet_number);

    printf("Type benchquit to terminate test\n");
    printf("****************************************************************\n");

      if(p_tCxt->tx_params.test_mode == TIME_TEST)
      {
          _time_get(&start_time);
          
          prog_report_inc = p_tCxt->tx_params.tx_time/20;
          
          if(prog_report_inc==0)
                  prog_report_inc = 1;
          
          next_prog_report = start_time.SECONDS + prog_report_inc;
      }else if(p_tCxt->tx_params.test_mode == PACKET_TEST){
          /* generate 20 progress characters across screen to provide progress feedback */
          prog_report_inc = p_tCxt->tx_params.packet_number/20;
          
          if(prog_report_inc==0)
                  prog_report_inc = 1;
          
          next_prog_report = prog_report_inc;
      }
          
      /* Create UDP socket */
      if(!v6EnableFlag)
      {   
        /* Create IPv4 socket */
        if((p_tCxt->sock_peer = t_socket((void*)whistle_handle, ATH_AF_INET, SOCK_DGRAM_TYPE, 0)) == A_ERROR) {
            goto ERROR_1;
        }               
      }
      else
      {
         /* Create IPv6 socket */
        if((p_tCxt->sock_peer = t_socket((void*)whistle_handle, ATH_AF_INET6, SOCK_DGRAM_TYPE, 0)) == A_ERROR){
            goto ERROR_1;
        } 
      }           
    
        /* Bind to the server.*/
        printf("Connecting.\n");
        memset(&foreign_addr, 0, sizeof(local_addr));
        foreign_addr.sin_addr = p_tCxt->tx_params.ip_address;
        foreign_addr.sin_port = p_tCxt->tx_params.port;    
        foreign_addr.sin_family = ATH_AF_INET;
        
        if(test_for_quit()){
        	goto ERROR_2;
        }
        
        /*Allow small delay to allow other thread to run*/
    	_time_delay(TX_DELAY);
    	
        if(!v6EnableFlag)
        {
            if(t_connect((void*)whistle_handle, p_tCxt->sock_peer, (&foreign_addr), sizeof(foreign_addr)) == A_ERROR)
            {
               printf("Conection failed.\n");
               goto ERROR_2;
            } 
        }
        else
        {
             memset(&foreign_addr6, 0, sizeof(local_addr6));
             memcpy(&foreign_addr6.sin6_addr,p_tCxt->tx_params.v6addr,16);
             foreign_addr6.sin6_port = p_tCxt->tx_params.port;    
             foreign_addr6.sin6_family = ATH_AF_INET6;
             if(t_connect((void*)whistle_handle, p_tCxt->sock_peer, (SOCKADDR_T *)(&foreign_addr6), sizeof(foreign_addr6)) == A_ERROR)
             {
                     printf("Conection failed.\n");
                     goto ERROR_2;
             } 
        }    
        
        /* Sending.*/
        printf("Sending.\n"); 
        
        /*Reset all counters*/  
        p_tCxt->bytes = 0;
        p_tCxt->kbytes = 0;
        p_tCxt->last_bytes = 0;
        p_tCxt->last_kbytes = 0;
        p_tCxt->sent_bytes = 0;
        cur_packet_number = 0;

        _time_get_elapsed(&p_tCxt->first_time);   
        _time_get_elapsed(&p_tCxt->last_time);
        block_time = p_tCxt->first_time;
                            
        while(1)
        {
          if(test_for_quit()){
                  p_tCxt->test_type = UDP_TX;
                  print_test_results(p_tCxt);
                  break;
          }
           
          while((p_tCxt->buffer = CUSTOM_ALLOC(packet_size)) == NULL)  
          {
              //Wait till we get a buffer 
              if(test_for_quit()){
                  p_tCxt->test_type = UDP_TX;
                  print_test_results(p_tCxt);
                  goto ERROR_2;
              }         
              /*Allow small delay to allow other thread to run*/
       	      _time_delay(TX_DELAY);
          }
          
          if(!v6EnableFlag)
          {
              send_result = t_sendto((void*)whistle_handle, p_tCxt->sock_peer, (unsigned char*)(&p_tCxt->buffer[0]), packet_size, 0,(&foreign_addr), sizeof(foreign_addr)) ;	
          }
          else
          {
              send_result = t_sendto((void*)whistle_handle, p_tCxt->sock_peer, (unsigned char*)(&p_tCxt->buffer[0]), packet_size, 0,(&foreign_addr6), 
                            sizeof(foreign_addr6)) ;	
          }

#if !NON_BLOCKING_TX          
          /*Free the buffer only if NON_BLOCKING_TX is not enabled*/
          if(p_tCxt->buffer){
              CUSTOM_FREE(p_tCxt->buffer);
          }
#endif          
          _time_get_elapsed(&p_tCxt->last_time);
          
          if(test_for_delay(&p_tCxt->last_time, &block_time)){
              /* block to give other tasks an opportunity to run */
              _time_delay(TX_DELAY);
              _time_get_elapsed(&block_time);
          }
          
          /****Bandwidth control***********/
          if(p_tCxt->tx_params.interval)
              _time_delay(p_tCxt->tx_params.interval);
          
          if(send_result == A_ERROR)
          {                           
              
              //printf("socket_error = %d\n", sock_err);
              
              p_tCxt->test_type = UDP_TX;
              
              /* Print throughput results.*/
              print_test_results(p_tCxt);           		
              break;
          }
          else
          {
              p_tCxt->bytes += send_result;
              p_tCxt->sent_bytes += send_result;
              cur_packet_number ++;
              
              /*Test mode can be "number of packets" or "fixed time duration"*/
              if(p_tCxt->tx_params.test_mode == PACKET_TEST)
              {                	                
                  if(cur_packet_number >= next_prog_report){
                          printf(".");
                          next_prog_report += prog_report_inc;
                  }
          
                  if((cur_packet_number >= p_tCxt->tx_params.packet_number))
                  { 
                      /*Test completed, print throughput results.*/
                      break;
                  }
              }
              else if(p_tCxt->tx_params.test_mode == TIME_TEST)
              {
                  _time_get(&current_time);
                  if(current_time.SECONDS >= next_prog_report){
                       printf(".");
                       next_prog_report += prog_report_inc;
                  }                	                	
                  
                  if(check_test_time(p_tCxt))	
                  {
                      /* Test completed, print throughput results.*/
                      break;
                  }
              }
          }
        }
     
        result = wait_for_response(p_tCxt, foreign_addr, foreign_addr6);
           
        if(result != A_OK)
                printf("UDP Transmit test failed, did not receive Ack from Peer\n");
        else
        {
            p_tCxt->test_type = UDP_TX;
            print_test_results(p_tCxt);
        }

ERROR_2:
        t_shutdown((void*)whistle_handle,p_tCxt->sock_peer);

ERROR_1:
    printf("*************IOT Throughput Test Completed **************\n");
}









/************************************************************************
* NAME: ath_tcp_rx
*
* DESCRIPTION: Start throughput TCP server. 
************************************************************************/
static void ath_tcp_rx (THROUGHPUT_CXT *p_tCxt)
{	
    A_UINT8 ip_str[16];
    A_UINT16 addr_len=0;
    A_UINT32 port;
    A_INT32 received =0;
 
    A_INT32 conn_sock,isFirst = 1;
    _ip_address temp_addr;
    SOCKADDR_T local_addr;
    SOCKADDR_T foreign_addr;    
    SOCKADDR_6_T local_addr6;
    SOCKADDR_6_T foreign_addr6;
    A_UINT8 ip6_str[48];	
    TIME_STRUCT      block_time;
	
	
    port = p_tCxt->rx_params.port;
    bench_quit = 0;
    p_tCxt->sock_peer = A_ERROR;
    
    num_traffic_streams++;

#if !ZERO_COPY     
    /*Allocate buffer*/
    if((p_tCxt->buffer = A_MALLOC(CFG_PACKET_SIZE_MAX_RX, MALLOC_ID_CONTEXT)) == NULL) 	
    {
            printf("Out of memory error\n");
             goto ERROR_1;
    }		   
#endif
    
    if(!v6EnableFlag)
    {  
    	/* Create listen socket */
        if((p_tCxt->sock_local = t_socket((void*)whistle_handle, ATH_AF_INET, SOCK_STREAM_TYPE, 0)) == A_ERROR)
        {
            printf("ERROR: Socket creation error.\r\n");
            goto ERROR_1;
        }
        
        memset(&local_addr, 0, sizeof(local_addr));	    
        local_addr.sin_port = port;   
        local_addr.sin_family = ATH_AF_INET;
        
         /* Bind socket.*/
        if(t_bind((void*)whistle_handle, p_tCxt->sock_local, &local_addr, sizeof(local_addr)) == A_ERROR)
        {
            printf("ERROR: Socket bind error.\r\n");
            goto ERROR_2;
        }        
    }
    else
    {
        if((p_tCxt->sock_local = t_socket((void*)whistle_handle, ATH_AF_INET6, SOCK_STREAM_TYPE, 0)) == A_ERROR)
        {
            printf("ERROR: Socket creation error.\r\n");
            goto ERROR_1;
        }
        
        memset(&local_addr6, 0, sizeof(local_addr6));
        local_addr6.sin6_port = port;    
        local_addr6.sin6_family = ATH_AF_INET6;
        
        /* Bind socket.*/
        if(t_bind((void*)whistle_handle, p_tCxt->sock_local, (SOCKADDR_T *)&local_addr6, sizeof(local_addr6)) == A_ERROR)
        {
            printf("ERROR: Socket bind error.\r\n");
            goto ERROR_2;
        }       
    }

    /* ------ Start test.----------- */
    printf("****************************************************\n");
    printf(" TCP RX Test\n" );
    printf("****************************************************\n");

    printf( "Local port %d\n", port);
    printf("Type benchquit to terminate test\n");
    printf("****************************************************\n");
    
    _time_get_elapsed(&p_tCxt->first_time);
    _time_get_elapsed(&p_tCxt->last_time); 
    p_tCxt->last_bytes = 0;
    p_tCxt->last_kbytes = 0;
    
    while(1)
    {
        printf("Waiting.\n");
             
        /* Listen. */
        if(t_listen((void*)whistle_handle, p_tCxt->sock_local, 1) == A_ERROR)
        {
            printf("ERROR: Socket listen error.\r\n");
            goto ERROR_2;
        }
          
        p_tCxt->bytes = 0;
        p_tCxt->kbytes = 0;
        p_tCxt->sent_bytes = 0;
        
        do
        {
            if(test_for_quit()){
                goto tcp_rx_QUIT;
            }
            /* block for 500msec or until a packet is received */
            conn_sock = t_select((void*)whistle_handle, p_tCxt->sock_local,UDP_CONNECTION_WAIT_TIME);
            
            if(conn_sock == A_SOCK_INVALID)
              goto tcp_rx_QUIT;       //Peer closed connection, socket no longer valid

        }while(conn_sock == A_ERROR);	
        
        /*Accept incoming connection*/
        if(!v6EnableFlag)
        {
          addr_len = sizeof(foreign_addr);
          p_tCxt->sock_peer = t_accept((void*)whistle_handle, p_tCxt->sock_local, &foreign_addr, addr_len);
        }
        else
        {
          addr_len = sizeof(foreign_addr6);
          p_tCxt->sock_peer = t_accept((void*)whistle_handle, p_tCxt->sock_local, &foreign_addr6, addr_len);
        }
                      
        if(p_tCxt->sock_peer != A_ERROR)
        {      
            if(!v6EnableFlag)
            {      
                temp_addr = LONG_BE_TO_HOST(foreign_addr.sin_addr);      	                
                printf("Receiving from %s:%d\n", inet_ntoa(*(A_UINT32 *)(&temp_addr), (char *)ip_str), foreign_addr.sin_port);
            }
            else
            {
                printf("Receiving from %s:%d\n", inet6_ntoa((char *)&foreign_addr6.sin6_addr,(char *)ip6_str), foreign_addr6.sin6_port);
            }
            
            _time_get_elapsed(&p_tCxt->first_time);
            _time_get_elapsed(&p_tCxt->last_time); 
            block_time = p_tCxt->first_time;
            isFirst = 1;
            
            while(1) /* Receiving data.*/
            {
            	if(test_for_quit()){
            	    _time_get_elapsed(&p_tCxt->last_time);
        		goto tcp_rx_QUIT;
        	}

                conn_sock = t_select((void*)whistle_handle, p_tCxt->sock_peer,UDP_CONNECTION_WAIT_TIME);      	
                
                if(conn_sock == A_OK)
                {
                   /*Packet is available, receive it*/
#if ZERO_COPY                  
                    received = t_recv((void*)whistle_handle, p_tCxt->sock_peer, (void**)(&p_tCxt->buffer), CFG_PACKET_SIZE_MAX_RX, 0);
                    if(received > 0)
                      zero_copy_free(p_tCxt->buffer);
#else
                    received = t_recv((void*)whistle_handle, p_tCxt->sock_peer, (void*)(&p_tCxt->buffer[0]), CFG_PACKET_SIZE_MAX_RX, 0);
#endif                        
                    if(received == A_SOCK_INVALID)
                    {
                        /*Test ended, peer closed connection*/
                        p_tCxt->kbytes = p_tCxt->bytes/1024;
                        p_tCxt->bytes = p_tCxt->bytes % 1024;
                        p_tCxt->test_type = TCP_RX;
                        /* Print test results.*/
                        print_test_results (p_tCxt);
                        //goto ERROR_2;
                        break;                        
                    }
                }
                    	
                if(conn_sock == A_SOCK_INVALID)
                {
                    /* Test ended, peer closed connection*/
                    p_tCxt->kbytes = p_tCxt->bytes/1024;
                    p_tCxt->bytes = p_tCxt->bytes % 1024;
                    p_tCxt->test_type = TCP_RX;
                    /* Print test results.*/
                    print_test_results (p_tCxt);
                    //goto ERROR_2;
                    break;
                }
                else
                { 
                    /*Valid packaet received*/
                    if(isFirst)
                    {
                       /*This is the first packet, set initial time used to calculate throughput*/   
                       _time_get_elapsed(&p_tCxt->first_time);
                       isFirst = 0;
                    }
                    _time_get_elapsed(&p_tCxt->last_time);
                    p_tCxt->bytes += received;
                    p_tCxt->sent_bytes += received;
                    
                    if(test_for_delay(&p_tCxt->last_time, &block_time)){
	            	/* block to give other tasks an opportunity to run */
	            	_time_delay(TX_DELAY);
	            	_time_get_elapsed(&block_time);
            	    }
                }                            
            }
        }
        
        if(test_for_quit()){
        _time_get_elapsed(&p_tCxt->last_time);
                goto tcp_rx_QUIT;
        }
    }
    
tcp_rx_QUIT:    
    p_tCxt->kbytes = p_tCxt->bytes/1024;
    p_tCxt->bytes = p_tCxt->bytes % 1024;
	p_tCxt->test_type = TCP_RX;
    /* Print test results.*/
    print_test_results (p_tCxt);


    //t_shutdown((void*)handle, p_tCxt->sock_peer);
    
ERROR_2:
    t_shutdown((void*)whistle_handle, p_tCxt->sock_local);

ERROR_1:
 
    printf("*************IOT Throughput Test Completed **************\n");
    printf("Shell> ");
    num_traffic_streams--;
#if !ZERO_COPY     
    if(p_tCxt->buffer)
    	A_FREE(p_tCxt->buffer,MALLOC_ID_CONTEXT);
#endif
}



/************************************************************************
* NAME: ath_udp_rx
*
* DESCRIPTION: Start throughput UDP server. 
************************************************************************/
static void ath_udp_rx (THROUGHPUT_CXT *p_tCxt)
{
    A_UINT8           ip_str[16];
    A_UINT32          addr_len;
    A_UINT16          port;
    A_INT32 	      received;
    A_INT32 	      conn_sock;
    A_INT32 	      is_first = 1;
    SOCKADDR_T        addr;
    SOCKADDR_T        local_addr;
    SOCKADDR_6_T      addr6;
    SOCKADDR_6_T      local_addr6;
    A_UINT8           ip6_str[48]; 
    A_UINT32 	      temp_addr;
    TIME_STRUCT       block_time;
            
    // init quit flag
    bench_quit = 0;
    
    //Parse remaining parameters
    port = p_tCxt->rx_params.port;
    
     num_traffic_streams++;

#if !ZERO_COPY     
    if((p_tCxt->buffer = A_MALLOC(CFG_PACKET_SIZE_MAX_RX,MALLOC_ID_CONTEXT)) == NULL)  	
    {
            printf("Out of memory error\n");
            return; 
    }
#endif    

    if(!v6EnableFlag)
    {
        if((p_tCxt->sock_local = t_socket((void*)whistle_handle,ATH_AF_INET,SOCK_DGRAM_TYPE,0))== A_ERROR)
        {
            printf("ERROR:: Socket creation error.\r\n");
            goto ERROR_1;
        }

        /* Bind.*/
        memset(&local_addr, 0, sizeof(local_addr));            
        local_addr.sin_port = port;    
        local_addr.sin_family = ATH_AF_INET;

        if(t_bind((void*)whistle_handle, p_tCxt->sock_local, (&local_addr), sizeof(local_addr)) != A_OK)
        {
            printf("ERROR:: Socket bind error.\r\n");
            goto ERROR_2;
        }
        
        /*************Multicast support ************************/
	if(handle_mcast_param(p_tCxt) !=A_OK)
	  goto ERROR_2;
     }
     else
     {
        if((p_tCxt->sock_local = t_socket((void*)whistle_handle,ATH_AF_INET6,SOCK_DGRAM_TYPE,0))== A_ERROR)
        {
            printf("ERROR:: Socket creation error.\r\n");
            goto ERROR_1;
        }
		
	memset(&local_addr6, 0, sizeof(local_addr6));            
        local_addr6.sin6_port = port;    
        local_addr6.sin6_family = ATH_AF_INET6;

        if(t_bind((void*)whistle_handle, p_tCxt->sock_local,(SOCKADDR_T *) (&local_addr6), sizeof(local_addr6)) != A_OK)
        {
            printf("ERROR:: Socket bind error.\r\n");
            goto ERROR_2;
        }
        
        /*************Multicast support ************************/
	if(handle_mcast_param(p_tCxt) !=A_OK)
	  goto ERROR_2;
     } 
    
    
     /* ------ Start test.----------- */
    printf("****************************************************\n");
    printf(" UDP RX Test\n" );
    printf("****************************************************\n");

    printf("Local port %d\n", port);
    printf("Type benchquit to termintate test\n");
    printf("****************************************************\n");
    
    /*Initilize start, stop times*/
    
    _time_get_elapsed(&p_tCxt->last_time);
    _time_get_elapsed(&p_tCxt->first_time);
    p_tCxt->last_bytes = 0;
    p_tCxt->last_kbytes = 0; 
    block_time = p_tCxt->first_time;
     
    while(test_for_quit()==0) /* Main loop */
    {
        printf("Waiting.\n");
        
        p_tCxt->bytes = 0;
        p_tCxt->kbytes = 0;

        p_tCxt->sent_bytes = 0;
        
        addr_len = sizeof(SOCKADDR_T);
        is_first = 1;
        
        while(test_for_quit()==0)
        {	        	    		    		
              do
              {
                      if(test_for_quit()){
                              goto ERROR_3;
                      }
                      /* block for 500msec or until a packet is received */
                      conn_sock = t_select((void*)whistle_handle, p_tCxt->sock_local,UDP_CONNECTION_WAIT_TIME);
                      
                      if(conn_sock == A_SOCK_INVALID)
                        goto ERROR_3;       // socket no longer valid

              }while(conn_sock == A_ERROR);	
              
              /* Receive data */  
#if ZERO_COPY
             if(!v6EnableFlag)
             { 
                received = t_recvfrom((void*)whistle_handle, p_tCxt->sock_local, 
            					(void**)(&p_tCxt->buffer), 
            					CFG_PACKET_SIZE_MAX_RX, 0,
                				&addr, (A_UINT32*)&addr_len);  
             }
             else
             {
                 addr_len = sizeof(SOCKADDR_6_T);
                 received = t_recvfrom((void*)whistle_handle, p_tCxt->sock_local, 
            					(void**)(&p_tCxt->buffer), 
            					CFG_PACKET_SIZE_MAX_RX, 0,
                				&addr6, (A_UINT32*)&addr_len);
                 
             }
              if(received > 0)
                zero_copy_free(p_tCxt->buffer);
#else                
            if(!v6EnableFlag)
            {
                 received = t_recvfrom((void*)whistle_handle, p_tCxt->sock_local, 
            					(void*)(&p_tCxt->buffer[0]), 
            					CFG_PACKET_SIZE_MAX_RX, 0,
                				&addr, (A_UINT32*)&addr_len);                				               	
            }
            else
            {
                 addr_len = sizeof(SOCKADDR_6_T);
                 received = t_recvfrom((void*)whistle_handle, p_tCxt->sock_local, 
            					(void*)(&p_tCxt->buffer[0]), 
            					CFG_PACKET_SIZE_MAX_RX, 0,
                				&addr6, (A_UINT32*)&addr_len);                				               	
            }                				               	
#endif            
           
             
            if(received >= sizeof(EOT_PACKET))
            {   
                /* Reset timeout. */
                if( received > sizeof(EOT_PACKET) ){
                      _time_get_elapsed(&p_tCxt->last_time);
                }
                if(is_first)
                {
                    if( received > sizeof(EOT_PACKET) )
                    {
                   
                    	temp_addr = LONG_BE_TO_HOST(addr.sin_addr);              
                    	
                        if(!v6EnableFlag)
                        {	
                            printf("Receiving from %s:%d\n", inet_ntoa(*(A_UINT32 *)(&temp_addr), (char *)ip_str), addr.sin_port);
                        }
                        else
                        {
                            printf("Receiving from %s:%d\n", inet6_ntoa((char *)&addr6.sin6_addr, (char *)ip6_str), addr6.sin6_port);
                        }
               
                        _time_get_elapsed(&p_tCxt->first_time);
                        is_first = 0;
                    }
                }
                else
                {
                    p_tCxt->bytes += received;
                    p_tCxt->sent_bytes += received;
                    
                    if(test_for_delay(&p_tCxt->last_time, &block_time)){
                            /* block to give other tasks an opportunity to run */
                            _time_delay(TX_DELAY);
                            _time_get_elapsed(&block_time);
                    }
                    if(received == sizeof(EOT_PACKET) ) /* End of transfer. */
                    {
    			/* Send throughput results to Peer so that it can display correct results*/
                         /* Send throughput results to Peer so that it can display correct results*/
                        if(!v6EnableFlag)
                        {                                                
                         sendAck( p_tCxt, (A_VOID*)&addr, port);
                        }
                        else
                        {
                         sendAck( p_tCxt, (A_VOID*)&addr6, port);
                        }   
                        
                        
                        break;    	
                    }
                }
            }                                         
    	}                                          
   
ERROR_3:   
        p_tCxt->kbytes = p_tCxt->bytes/1024;
        p_tCxt->bytes = p_tCxt->bytes % 1024;  
	p_tCxt->test_type = UDP_RX;
	print_test_results (p_tCxt);       
     }
ERROR_2:
    t_shutdown((void*)whistle_handle,p_tCxt->sock_local);
                           
ERROR_1:
	
    printf(CFG_COMPLETED_STR);
    printf("shell> ");

#if !ZERO_COPY    
    if(p_tCxt->buffer)
    	A_FREE(p_tCxt->buffer,MALLOC_ID_CONTEXT);
#endif
    
     num_traffic_streams--;
}







/************************************************************************
* NAME: ath_udp_echo
*
* DESCRIPTION: A reference implementation of UDP Echo server. It will echo 
*              a packet received on specified port.  
************************************************************************/
void ath_udp_echo (int_32 argc, char_ptr argv[])
{
    A_UINT32          addr_len;
    A_UINT16          port;
    A_INT32 	      received,result;
    A_INT32 	      conn_sock;
    SOCKADDR_T        addr;
    SOCKADDR_T        local_addr;
    SOCKADDR_6_T      addr6;
    SOCKADDR_6_T      local_addr6;
    A_UINT8*          rxBuffer, *txBuffer;
      
#if !ZERO_COPY
    printf("This example is only supported with zero copy feature\n");
    return;
#endif    
    // init quit flag
    bench_quit = 0;
    
    if(argc < 3){
      printf("Missing UDP port\n");
      return;
    }
    /*Get listening port*/
    port = atoi(argv[2]); 

    if(!v6EnableFlag)
    {
        if((conn_sock = t_socket((void*)whistle_handle,ATH_AF_INET,SOCK_DGRAM_TYPE,0))== A_ERROR)
        {
            printf("ERROR:: Socket creation error.\r\n");
            goto ERROR_1;
        }

        /* Bind.*/
        memset(&local_addr, 0, sizeof(local_addr));            
        local_addr.sin_port = port;    
        local_addr.sin_family = ATH_AF_INET;

        if(t_bind((void*)whistle_handle, conn_sock, (&local_addr), sizeof(local_addr)) != A_OK)
        {
            printf("ERROR:: Socket bind error.\r\n");
            goto ERROR_2;
        }
     }
     else
     {
        if((conn_sock = t_socket((void*)whistle_handle,ATH_AF_INET6,SOCK_DGRAM_TYPE,0))== A_ERROR)
        {
            printf("ERROR:: Socket creation error.\r\n");
            goto ERROR_1;
        }
		
	memset(&local_addr6, 0, sizeof(local_addr6));            
        local_addr6.sin6_port = port;    
        local_addr6.sin6_family = ATH_AF_INET6;

        if(t_bind((void*)whistle_handle, conn_sock,(SOCKADDR_T *) (&local_addr6), sizeof(local_addr6)) != A_OK)
        {
            printf("ERROR:: Socket bind error.\r\n");
            goto ERROR_2;
        }        
     } 

     /* ------ Start test.----------- */
    printf("****************************************************\n");
    printf(" UDP Echo Server\n" );
    printf("****************************************************\n");
    printf("Local port %d\n", port);
    printf("Type benchquit to termintate test\n");
    printf("****************************************************\n");
    
    addr_len = sizeof(SOCKADDR_T);
       
    while(test_for_quit()==0)
    {	        	    		    		
          do
          {
                  if(test_for_quit()){
                          goto ERROR_2;
                  }
                  /* block for 500msec or until a packet is received */
                  result = t_select((void*)whistle_handle, conn_sock,UDP_CONNECTION_WAIT_TIME);
                  
                  if(result == A_SOCK_INVALID)
                    goto ERROR_2;       // socket no longer valid
  
          }while(result == A_ERROR);	
          
          /* Receive data */  
  
         if(!v6EnableFlag)
         { 
            received = t_recvfrom((void*)whistle_handle, conn_sock, 
                                            (void**)(&rxBuffer), 
                                            CFG_PACKET_SIZE_MAX_RX, 0,
                                            &addr, (A_UINT32*)&addr_len);  
         }
         else
         {
             addr_len = sizeof(SOCKADDR_6_T);
             received = t_recvfrom((void*)whistle_handle, conn_sock, 
                                            (void**)(&rxBuffer), 
                                            CFG_PACKET_SIZE_MAX_RX, 0,
                                            &addr6, (A_UINT32*)&addr_len);
             
         }
          if(received > 0)
          {
              printf("Received %d bytes\n",received);
              while((txBuffer = CUSTOM_ALLOC(received)) == NULL)  
              {
                  //Wait till we get a buffer 
                  if(test_for_quit()){
                      goto ERROR_2;
                  }         
                  /*Allow small delay to allow other thread to run*/
                  _time_delay(TX_DELAY);
              }
              /*Copy received contents in TX buffer*/
              A_MEMCPY(txBuffer,rxBuffer,received);
              /*Free the RX buffer*/
              zero_copy_free(rxBuffer);
              
              /*Send the received data back to sender*/
              if(!v6EnableFlag)
              {
                if(t_sendto((void*)whistle_handle, conn_sock, (unsigned char*)(txBuffer), received, 0,(&addr), sizeof(addr)) == A_ERROR){
                  printf("Send failed\n"); 
                }
              }
              else
              {
                if(t_sendto((void*)whistle_handle, conn_sock, (unsigned char*)(txBuffer), received, 0,(&addr6),sizeof(addr6)) == A_ERROR){
                  printf("Send failed\n"); 
                }
              }

#if !NON_BLOCKING_TX          
                  /*Free the buffer only if NON_BLOCKING_TX is not enabled*/
                  if(txBuffer){
                      CUSTOM_FREE(txBuffer);
                  }
#endif                                                                      
    	    }                                                
     }
ERROR_2:
    t_shutdown((void*)whistle_handle,conn_sock);
                           
ERROR_1:
	
    printf(CFG_COMPLETED_STR);
    printf("shell> ");

}








/************************************************************************
* NAME: wait_for_response
*
* DESCRIPTION: In UDP uplink test, the test is terminated by transmitting
* end-mark (single byte packet). We have implemented a feedback mechanism 
* where the Peer will reply with receive stats allowing us to display correct
* test results.
* Parameters: pointer to throughput context
************************************************************************/

int_32 wait_for_response(THROUGHPUT_CXT *p_tCxt, SOCKADDR_T foreign_addr, SOCKADDR_6_T foreign_addr6)
{
    uint_32 received=0;
    int_32 error = A_ERROR;
    SOCKADDR_T local_addr;
    SOCKADDR_T addr;
    uint_32 addr_len;
    stat_packet_t* stat_packet;
    SOCKADDR_6_T local_addr6;
    SOCKADDR_6_T addr6;
    int retry_counter = MAX_END_MARK_RETRY;
    unsigned char* endmark;
    
    
    /* Create listen socket & Bind.*/
    if(!v6EnableFlag)
    {
            if((p_tCxt->sock_local = t_socket((void*)whistle_handle,ATH_AF_INET,SOCK_DGRAM_TYPE, 0)) == A_ERROR)
            {
                printf("ERROR:: Socket creation error.\r\n");
                goto ERROR_1;
            }
            
            memset(&local_addr, 0, sizeof(local_addr));  
            local_addr.sin_port = p_tCxt->tx_params.port;  
            local_addr.sin_family = ATH_AF_INET;

            if(t_bind((void*)whistle_handle,p_tCxt->sock_local, (&local_addr), sizeof(local_addr)) != A_OK)
            {
                printf("ERROR:: Socket bind error.\r\n");
                goto ERROR_2;
            }
    }
    else
    {

            if((p_tCxt->sock_local = t_socket((void*)whistle_handle,ATH_AF_INET6,SOCK_DGRAM_TYPE, 0)) == A_ERROR)
            {
                printf("ERROR:: Socket creation error.\r\n");
                goto ERROR_1;
            }
       
            memset(&local_addr6, 0, sizeof(local_addr6));  
            local_addr6.sin6_port = p_tCxt->tx_params.port;  
            local_addr6.sin6_family = ATH_AF_INET6;

            if(t_bind((void*)whistle_handle,p_tCxt->sock_local, (&local_addr6), sizeof(local_addr6)) != A_OK)
            {
                printf("ERROR:: Socket bind error.\r\n");
                goto ERROR_2;
            }
    }    

#if !ZERO_COPY     
        if((stat_packet = A_MALLOC(sizeof(stat_packet_t),MALLOC_ID_CONTEXT)) == NULL)  	
        {
                printf("Out of memory error\n");
                return A_ERROR; 
        }
    #endif      
    
    while(retry_counter)
    {		
          while((endmark = CUSTOM_ALLOC(sizeof(EOT_PACKET))) == NULL)  
          {                   
            /*Allow small delay to allow other thread to run*/
            _time_delay(TX_DELAY);
          }
          /* Send End mark.*/

          ((EOT_PACKET*)endmark)->code = HOST_TO_LE_LONG(END_OF_TEST_CODE);
          //((EOT_PACKET*)endmark)->packet_count = HOST_TO_LE_LONG(cur_packet_number);
          if(!v6EnableFlag)
          {
                t_sendto((void*)whistle_handle, p_tCxt->sock_peer, (unsigned char*)(endmark), sizeof(EOT_PACKET), 0,(&foreign_addr), sizeof(foreign_addr)) ;	
          }
          else
          {
                t_sendto((void*)whistle_handle, p_tCxt->sock_peer, (unsigned char*)(endmark), sizeof(EOT_PACKET), 0,(SOCKADDR_T *)(&foreign_addr6), sizeof(foreign_addr6)) ;	
          }
                
            
#if !NON_BLOCKING_TX          
            /*Free the buffer only if NON_BLOCKING_TX is not enabled*/
            CUSTOM_FREE(endmark);
#endif                 
       
          /* block for xxx msec or until activity on socket */
          if(A_OK == t_select((void*)whistle_handle,p_tCxt->sock_local/*p_tCxt->sock_peer*/, 1000)){    	    	
                  /* Receive data */
#if ZERO_COPY  
                if(!v6EnableFlag)
                {	
                    addr_len = sizeof(SOCKADDR_T);
                    received = t_recvfrom((void*)whistle_handle, /*p_tCxt->sock_peer*/p_tCxt->sock_local, (void**)(&stat_packet), sizeof(stat_packet_t), 0,&addr, &addr_len );
                }
                else
                {
                    addr_len = sizeof(SOCKADDR_6_T);  
                    received = t_recvfrom((void*)whistle_handle, /*p_tCxt->sock_peer*/p_tCxt->sock_local, (void**)(&stat_packet), sizeof(stat_packet_t), 0,&addr6, &addr_len );
                }
#else              	    
                if(!v6EnableFlag)
                {	
                    addr_len = sizeof(SOCKADDR_T);
                    received = t_recvfrom((void*)whistle_handle, /*p_tCxt->sock_peer*/p_tCxt->sock_local, (void*)(stat_packet), sizeof(stat_packet_t), 0,&addr, &addr_len );
                }
                else
                {
                    addr_len = sizeof(SOCKADDR_6_T);                	     		
                    received = t_recvfrom((void*)whistle_handle, /*p_tCxt->sock_peer*/p_tCxt->sock_local, (void*)(stat_packet), sizeof(stat_packet_t), 0,&addr6, &addr_len );
                }
#endif        
        }
        if(received == sizeof(stat_packet_t))
        {   
            printf("received statistics\n");
            error = A_OK;  
            
            /*Response received from peer, extract test statistics*/
            stat_packet->msec = HOST_TO_LE_LONG(stat_packet->msec);
            stat_packet->kbytes = HOST_TO_LE_LONG(stat_packet->kbytes);
            stat_packet->bytes = HOST_TO_LE_LONG(stat_packet->bytes);
            stat_packet->numPackets = HOST_TO_LE_LONG(stat_packet->numPackets);
            
            p_tCxt->first_time.SECONDS = p_tCxt->last_time.SECONDS = 0;
            p_tCxt->first_time.MILLISECONDS = 0;
            p_tCxt->last_time.MILLISECONDS = stat_packet->msec;
            p_tCxt->bytes = stat_packet->bytes;        
            p_tCxt->kbytes = stat_packet->kbytes;
            break;
        }
        else
        {      
            //error = A_ERROR;
            retry_counter--;
        
            //printf("Did not receive response\n");
        }
    }

#if ZERO_COPY    
    if(received > 0)
        zero_copy_free(stat_packet);    
#endif
        
ERROR_2:
#if !ZERO_COPY
    if(stat_packet)
       A_FREE(stat_packet,MALLOC_ID_CONTEXT);
#endif
    
  t_shutdown((void*)whistle_handle,p_tCxt->sock_local);

ERROR_1: 
 return error;
    
}
        
        
/************************************************************************
* NAME: handle_mcast_param
*
* DESCRIPTION: Handler for multicast parameters in UDp Rx test
* Parameters: pointer to throughput context
************************************************************************/
int_32 handle_mcast_param(THROUGHPUT_CXT *p_tCxt)
{	
	if(!v6EnableFlag)
	{
		if(p_tCxt->rx_params.group.imr_multiaddr != 0)
		{	
			
		    p_tCxt->rx_params.group.imr_multiaddr = A_CPU2BE32(p_tCxt->rx_params.group.imr_multiaddr);
		    p_tCxt->rx_params.group.imr_interface = A_CPU2BE32(p_tCxt->rx_params.group.imr_interface);
	            if(t_setsockopt((void*)whistle_handle, p_tCxt->sock_local, ATH_IPPROTO_IP, IP_ADD_MEMBERSHIP, 
	                             (A_UINT8*)(&(p_tCxt->rx_params.group)),sizeof(IP_MREQ_T)) != A_OK)
	            {
	                    printf("SetsockOPT error : unable to add to multicast group\r\n");
	                    return A_ERROR;	
	            }						
		}
	}
	else
	{
		if(p_tCxt->rx_params.v6mcastEnabled)
		{
			if(t_setsockopt((void*)whistle_handle,p_tCxt->sock_local, ATH_IPPROTO_IP, IPV6_JOIN_GROUP, 
			     (A_UINT8*)(&(p_tCxt->rx_params.group6)),sizeof(struct ipv6_mreq)) != A_OK)
			{
				printf("SetsockOPT error : unable to add to multicast group\r\n");
	                    return A_ERROR;	
			}
		}
	}
	return A_OK;	
}        




/************************************************************************
* NAME: sendAck
*
* DESCRIPTION: In UDP receive test, the test is terminated on receiving
* end-mark (single byte packet). We have implemented a feedback mechanism 
* where the Client will reply with receive stats allowing Peer to display correct
* test results. The Ack packet will contain time duration and number of bytes 
* received. Implementation details-
* 1. Peer sends endMark packet, then waits for 500 ms for a response.
* 2. Client, on receiving endMark, sends ACK (containing RX stats), and waits for 
*    1000 ms to check for more incoming packets.  
* 3. If the Peer receives this ACK, it will stop sending endMark packets.
* 4. If the client does not see the endMark packet for 1000 ms, it will assume SUCCESS
*    and exit gracefully. 
* 5. Each side makes 20 attempts. 
* Parameters: pointer to throughput context, specified address, specified port
************************************************************************/

void sendAck(THROUGHPUT_CXT *p_tCxt, A_VOID * address, uint_16 port)
{
    int send_result;	
    uint_32 received = 0;
    uint_32 addr_len=0;    
    uint_16 retry = MAX_ACK_RETRY;	    
    stat_packet_t* statPacket;    
    uint_32 sec_interval = (p_tCxt->last_time.SECONDS - p_tCxt->first_time.SECONDS);                 
    uint_32 ms_interval = (p_tCxt->last_time.MILLISECONDS - p_tCxt->first_time.MILLISECONDS); 
    uint_32 total_interval = sec_interval*1000 + ms_interval;
    SOCKADDR_T* addr = (SOCKADDR_T*)address;
    SOCKADDR_6_T *addr6 = (SOCKADDR_6_T *)address;
    
    if(!v6EnableFlag)
    {
      addr_len = sizeof(addr); 
    }
    else
    {
      addr_len = sizeof(addr6); 
    }
   
    while(retry)
    {	    
      
       while((statPacket = (stat_packet_t*)CUSTOM_ALLOC(sizeof(stat_packet_t))) == NULL)  
      {                   
        /*Allow small delay to allow other thread to run*/
        _time_delay(TX_DELAY);
         if(test_for_quit()){
           return;
         }
      }  
          
      statPacket->kbytes = HOST_TO_LE_LONG(p_tCxt->kbytes);
      statPacket->bytes = HOST_TO_LE_LONG(p_tCxt->bytes);
      statPacket->msec = HOST_TO_LE_LONG(total_interval);
    
	if(!v6EnableFlag)
        {     	    
            addr->sin_port = port;
	    send_result = t_sendto((void*)whistle_handle, p_tCxt->sock_local, (unsigned char*)(statPacket), sizeof(stat_packet_t), 0,addr,addr_len);
        }
        else
        {
            addr6->sin6_port = port;
	    send_result = t_sendto((void*)whistle_handle, p_tCxt->sock_local, (unsigned char*)(statPacket), sizeof(stat_packet_t), 0,(SOCKADDR_T *)addr6,addr_len);
        }
	    
#if !NON_BLOCKING_TX          
            /*Free the buffer only if NON_BLOCKING_TX is not enabled*/
            CUSTOM_FREE(statPacket);
#endif              
	    if ( send_result == A_ERROR )
	    {                     
	        printf("error while sending stat packet\n");
	        break;       
	    }
	    else
	    {	   			
	    	if(t_select((void*)whistle_handle, p_tCxt->sock_local,1000) == A_OK)
	    	{
#if ZERO_COPY
                    if(!v6EnableFlag)
                    {
                        received = t_recvfrom((void*)whistle_handle, p_tCxt->sock_local, 
            					(void**)(&p_tCxt->buffer), 
            					CFG_PACKET_SIZE_MAX_RX, 0,
                				addr, &addr_len);  
                    }
                    else
                    {
                        received = t_recvfrom((void*)whistle_handle, p_tCxt->sock_local, 
            					(void**)(&p_tCxt->buffer), 
            					CFG_PACKET_SIZE_MAX_RX, 0,
                				addr6, &addr_len);  
                    }
                    zero_copy_free(p_tCxt->buffer);
#else                  
	    	    if(!v6EnableFlag)
                    { 
	    		received = t_recvfrom((void*)whistle_handle, p_tCxt->sock_local, 
            					(void*)(&p_tCxt->buffer[0]), 
            					CFG_PACKET_SIZE_MAX_RX, 0,
                				addr, &addr_len);     
                    }
                    else
                    {
	    		received = t_recvfrom((void*)whistle_handle, p_tCxt->sock_local, 
            					(void*)(&p_tCxt->buffer[0]), 
            					CFG_PACKET_SIZE_MAX_RX, 0,
                				(SOCKADDR_T *)addr6, &addr_len);     
                    }
#endif                        
               	    printf("received %d\n",received);
	    	}
	    	else
	    	{
	    	    printf("ACK success\n");
            	    break;
	    	}
	    	                         
    	}	    	
	    retry--;
    }
}








#endif //ENABLE_STACK_OFFLOAD



