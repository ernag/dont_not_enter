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
#include <shell.h>
#include <stdlib.h>
#include "rtcs.h"
#include "ipcfg.h"
#include "throughput.h"


#if USE_ATH_CHANGES
#include "atheros_wifi_api.h"
#include "atheros_wifi.h"
#include "atheros_stack_offload.h"
#endif

#if !ENABLE_STACK_OFFLOAD


/**************************Globals *************************************/

 
uint_8 access_category = 0;		    //Variable to store User provided AC-not supported
static uint_32 num_traffic_streams = 0;     //Counter to monitor simultaneous traffic streams
static uint_32 port_in_use = 0;		    //Used to prevent two streams from using same port

/************************************************************************
*     Definitions.
*************************************************************************/


/*TCP receive timeout*/
const int rx_timeout = ATH_RECV_TIMEOUT;          

#define CFG_COMPLETED_STR    "IOT Throughput Test Completed.\n"
#define CFG_REPORT_STR "."

A_UINT8 bench_quit= 0;

/*************************** Function Declarations **************************/
void sendAck(THROUGHPUT_CXT *p_tCxt, A_VOID * address, uint_16 port);
int_32 wait_for_response(THROUGHPUT_CXT *p_tCxt, uint_16 port, sockaddr_in foreign_addr);
int_32 handle_mcast_param(THROUGHPUT_CXT *p_tCxt);
uint_32 adjust_buf_size( THROUGHPUT_CXT *p_tCxt, uint_32 direction, uint_32 transport_type, uint_16 port);


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


////////////////////////////////////////////// TX //////////////////////////////////////

/************************************************************************
* NAME: ath_tcp_tx
*
* DESCRIPTION: Start TX TCP throughput. 
************************************************************************/
static void ath_tcp_tx (THROUGHPUT_CXT *p_tCxt)
{
    struct sockaddr_in local_addr;
    struct sockaddr_in foreign_addr;
    char ip_str[16];
    TIME_STRUCT start_time,current_time,block_time;
    unsigned long bufsize_option_tx = TCP_TXTEST_HT_TX_BUF_SIZE;
    unsigned long bufsize_option_rx = TCP_TXTEST_RX_BUF_SIZE;       
    const int_32 keepalive_option = 1;
    const int_32 nonagle_option = 1;
    uint_16 option_len;
    int_32 sock_err ;	    
    uint_32 packet_size =  p_tCxt->tx_params.packet_size;
    uint_32 cur_packet_number;
    uint_32 next_prog_report, prog_report_inc;
    uint_32 buffer_offset;
    uint_32 send_result;
    _ip_address temp_addr;
	
    //init quit flag
    bench_quit = 0;
    num_traffic_streams++;
    
    if(packet_size > CFG_PACKET_SIZE_MAX_TX) /* Check max size.*/
         packet_size = CFG_PACKET_SIZE_MAX_TX;


#if USE_ATH_CHANGES    
    if((p_tCxt->buffer = _mem_alloc_system_zero(CFG_PACKET_SIZE_MAX_TX)) == NULL) 
#else   
	if((p_tCxt->buffer = malloc(CFG_PACKET_SIZE_MAX_TX)) == NULL)  
#endif	
	{
		printf("Out of memory error\n");
		return; 
	}

    
    /* ------ Start test.----------- */
    printf("*************************************************\n");
    printf(" TCP TX Test\n" );
    printf("*************************************************\n");

#if USE_ATH_CHANGES    
    temp_addr = LONG_BE_TO_HOST( p_tCxt->tx_params.ip_address);
#else
    temp_addr =  p_tCxt->tx_params.ip_address;
#endif    
    
    printf( "Remote IP addr. %s\n", inet_ntoa(*(A_UINT32 *)( &temp_addr), ip_str));
    printf( "Remote port %d\n",  p_tCxt->tx_params.port);
    printf( "Message size %d\n",  p_tCxt->tx_params.packet_size);
    printf( "Number of messages %d\n",  p_tCxt->tx_params.packet_number);
    printf("Type benchquit to cancel\n");
    printf("*************************************************\n");    
    
    	if( p_tCxt->tx_params.test_mode == TIME_TEST)
        {
            _time_get(&start_time);
            
            prog_report_inc =  p_tCxt->tx_params.tx_time/20;
            
            if(prog_report_inc==0)
                    prog_report_inc = 1;
            
            next_prog_report = start_time.SECONDS + prog_report_inc;
        }else if( p_tCxt->tx_params.test_mode == PACKET_TEST){
            /* generate 20 progress characters across screen to provide progress feedback */
            prog_report_inc =  p_tCxt->tx_params.packet_number/20;
            
            if(prog_report_inc==0)
                    prog_report_inc = 1;
            
            next_prog_report = prog_report_inc;
        }
        /* Create socket */
        if((p_tCxt->sock_peer = socket(AF_INET, SOCK_STREAM, 0)) == RTCS_SOCKET_ERROR)
        {
            printf("ERROR: Unable to create socket\n");

            goto ERROR_1;
        }

	    /*Adjust initial buffer size based on number of traffic streams*/
	    if(num_traffic_streams < MAX_STREAMS_ALLOWED)
	    {        
	    	/*Set buffers for high throughput*/
	    	if((setsockopt(p_tCxt->sock_peer, SOL_TCP, OPT_RBSIZE, (char *) &bufsize_option_rx, sizeof(bufsize_option_rx))== RTCS_SOCKET_ERROR) ||
               (setsockopt(p_tCxt->sock_peer, SOL_TCP, OPT_TBSIZE, (char *) &bufsize_option_tx, sizeof(bufsize_option_tx))== RTCS_SOCKET_ERROR)) 
	    	{
	            printf("ERROR: Socket setsockopt error.\n");
	            goto ERROR_2;			    
	    	}	    
	    }
	    else
	    {
	    	/*Set buffers for low throughput*/
	    	bufsize_option_tx = TCP_TXTEST_TX_BUF_SIZE;
		bufsize_option_rx = TCP_TXTEST_RX_BUF_SIZE;	
			
		if((setsockopt(p_tCxt->sock_peer, SOL_TCP, OPT_RBSIZE, (char *) &bufsize_option_rx, sizeof(bufsize_option_rx))== RTCS_SOCKET_ERROR) ||
               (setsockopt(p_tCxt->sock_peer, SOL_TCP, OPT_TBSIZE, (char *) &bufsize_option_tx, sizeof(bufsize_option_tx))== RTCS_SOCKET_ERROR)) 
	    	{
	            printf("ERROR: Socket setsockopt error.\n");
	            goto ERROR_2;			    
	    	}
	    }
	    
        /* Enable keepalive_option option. */
        if((setsockopt (p_tCxt->sock_peer, SOL_TCP, OPT_KEEPALIVE, (char *)&keepalive_option, sizeof(keepalive_option)) == RTCS_SOCKET_ERROR)||            
           /* No Nagle Algorithm.*/
           (setsockopt (p_tCxt->sock_peer, SOL_TCP, OPT_NO_NAGLE_ALGORITHM, (char *)&nonagle_option, sizeof(nonagle_option)) == RTCS_SOCKET_ERROR))
    	{
        	printf("ERROR: Socket setsockopt error.\n");
            goto ERROR_2;		
    	}
    	
    	/*Allow small delay to allow other thread to run*/
    	_time_delay(TX_DELAY);
    	        
        printf("Connecting.\n");
        memset(&foreign_addr, 0, sizeof(local_addr));
        foreign_addr.sin_addr.s_addr =  p_tCxt->tx_params.ip_address;
        foreign_addr.sin_port =  p_tCxt->tx_params.port;    
        foreign_addr.sin_family = AF_INET;
               
        /* Connect to the server.*/
        if(connect(p_tCxt->sock_peer, (&foreign_addr), sizeof(foreign_addr)) == RTCS_SOCKET_ERROR)
        {
           printf("Conection failed.\n");
           goto ERROR_2;
        } 
        
        if(test_for_quit()){
        	goto ERROR_2;
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
        //_time_get_elapsed(&block_time);
        
        while(1)
        {
        	if(test_for_quit()){
	        	break;
        	}
        	
            send_result = send( p_tCxt->sock_peer, (char*)(&p_tCxt->buffer[buffer_offset]), packet_size-buffer_offset, 0);

            _time_get_elapsed(&p_tCxt->last_time);
           
            if(test_for_delay(&p_tCxt->last_time, &block_time)){
            	/* block to give other tasks an opportunity to run */
            	_time_delay(TX_DELAY);
            	_time_get_elapsed(&block_time);
            }
            
            /****Bandwidth control- add delay if user has specified it************/
            if( p_tCxt->tx_params.interval)
            	_time_delay( p_tCxt->tx_params.interval);
          
            if ( send_result == RTCS_SOCKET_ERROR )
            {              
                option_len = sizeof(sock_err); 
                getsockopt(p_tCxt->sock_peer, SOL_SOCKET, OPT_SOCKET_ERROR, (char*)&sock_err, (unsigned long*)&option_len);
                printf("socket_error = %d\n", sock_err);
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
                
                if( p_tCxt->tx_params.test_mode == PACKET_TEST)
                {
                	/*Test based on number of packets, check if we have reached specified limit*/
                	if(cur_packet_number >= next_prog_report){
                		printf(".");
                		next_prog_report += prog_report_inc;
                	}
                
	                if((cur_packet_number >=  p_tCxt->tx_params.packet_number))
	                { 
	                     /* Test completed, print throughput results.*/
	                    break;
	                }
                }                
                else if( p_tCxt->tx_params.test_mode == TIME_TEST)
                {
                	/*Test based on time interval, check if we have reached specified limit*/
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
		
    ERROR_2:
    	printf("\n"); // new line to separate from progress line 
        p_tCxt->kbytes = p_tCxt->bytes/1024;
        p_tCxt->bytes = p_tCxt->bytes % 1024;
    	p_tCxt->test_type = TCP_TX;
    	print_test_results(p_tCxt);
        shutdown(p_tCxt->sock_peer, FLAG_ABORT_CONNECTION);

ERROR_1:
    printf("*************IOT Throughput Test Completed **************\n");   
	
	if(p_tCxt->buffer)
    	_mem_free(p_tCxt->buffer);
	
	num_traffic_streams--;
}









/************************************************************************
* NAME: ath_udp_tx
*
* DESCRIPTION: Start TX UDP Throughput test. 
************************************************************************/
static void ath_udp_tx (THROUGHPUT_CXT *p_tCxt)
{
	char ip_str[16];
	TIME_STRUCT start_time,current_time,block_time;
	uint_32 next_prog_report, prog_report_inc;
	struct sockaddr_in foreign_addr;
	struct sockaddr_in local_addr;		
	uint_16 option_len;
	int_32 sock_err ;
	int_32 send_result,result;
	uint_32 packet_size = p_tCxt->tx_params.packet_size;
	uint_32 cur_packet_number;
	_ip_address temp_addr;
	
	//init quit flag
	bench_quit = 0;

		
	if(packet_size > CFG_PACKET_SIZE_MAX_TX) /* Check max size.*/
	     packet_size = CFG_PACKET_SIZE_MAX_TX;
	
#if USE_ATH_CHANGES    
    if((p_tCxt->buffer = _mem_alloc_system_zero(CFG_PACKET_SIZE_MAX_TX)) == NULL) 
#else   
	if((p_tCxt->buffer = malloc(CFG_PACKET_SIZE_MAX_TX)) == NULL)  
#endif	
	{
		printf("Out of memory error\n");
		return; 
	}


#if USE_ATH_CHANGES     
 	temp_addr = LONG_BE_TO_HOST(p_tCxt->tx_params.ip_address);
#else
	temp_addr = p_tCxt->tx_params.ip_address;
#endif
		 	
    /* ------ Start test.----------- */
    printf("*************************************************\n");
    printf(" UDP TX Test\n" );
    printf("*************************************************\n");
    printf("Remote IP addr. %s\n", inet_ntoa(*(A_UINT32 *)( &temp_addr), ip_str));
    printf("Remote port %d\n", p_tCxt->tx_params.port);
    printf("Message size %d\n", p_tCxt->tx_params.packet_size);
    printf("Number of messages %d\n", p_tCxt->tx_params.packet_number);

    printf("Type benchquit to cancel\n");
    printf("*************************************************\n");
        
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
        if((p_tCxt->sock_peer = socket(AF_INET, SOCK_DGRAM, 0)) == RTCS_SOCKET_ERROR)
        {
            goto ERROR_1;
        }               
      
        /* Bind to the server.*/
        printf("Connecting.\n");
        memset(&foreign_addr, 0, sizeof(local_addr));
        foreign_addr.sin_addr.s_addr = p_tCxt->tx_params.ip_address;
        foreign_addr.sin_port = p_tCxt->tx_params.port;    
        foreign_addr.sin_family = AF_INET;
        
        if(test_for_quit()){
        	goto ERROR_2;
        }
        
        /*Allow small delay to allow other thread to run*/
    	_time_delay(TX_DELAY);
    	
        if(connect(p_tCxt->sock_peer, (&foreign_addr), sizeof(foreign_addr)) == RTCS_SOCKET_ERROR)
        {
           printf("Conection failed.\n");
           goto ERROR_2;
        } 
    
    	if(test_for_quit()){
        	goto ERROR_2;
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

      	    send_result = sendto(p_tCxt->sock_peer, (char*)(&p_tCxt->buffer[0]), packet_size, 0,(&foreign_addr), sizeof(foreign_addr)) ;	
            _time_get_elapsed(&p_tCxt->last_time);
            
             if(test_for_delay(&p_tCxt->last_time, &block_time)){
            	/* block to give other tasks an opportunity to run */
            	_time_delay(TX_DELAY);
            	_time_get_elapsed(&block_time);
            }
            
            /****Bandwidth control***********/
            if(p_tCxt->tx_params.interval)
            	_time_delay(p_tCxt->tx_params.interval);
         
            if ( send_result == RTCS_SOCKET_ERROR )
            {              
               
                option_len = sizeof(sock_err); 
                getsockopt(p_tCxt->sock_peer, SOL_SOCKET, OPT_SOCKET_ERROR, (char*)&sock_err, (unsigned long*)&option_len);
                printf("socket_error = %d\n", sock_err);
              	
              	p_tCxt->test_type = UDP_TX;
              	
                /* Print throughput results.*/
                print_test_results (p_tCxt);           		
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
	                     /* Test completed, print throughput results.*/
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
        
        result = wait_for_response(p_tCxt, p_tCxt->tx_params.port,foreign_addr);
           
        if(result != RTCS_OK){
            printf("UDP Transmit test failed, did not receive Ack from Peer\n");
        }else
        {
            p_tCxt->test_type = UDP_TX;
            print_test_results(p_tCxt);
        }

ERROR_2:
        shutdown(p_tCxt->sock_peer, 0);


ERROR_1:
    printf("*************IOT Throughput Test Completed **************\n");
    
    if(p_tCxt->buffer)
    	_mem_free(p_tCxt->buffer);

}









/************************************************************************
* NAME: ath_tcp_rx
*
* DESCRIPTION: Start throughput TCP server. 
************************************************************************/
static void ath_tcp_rx (THROUGHPUT_CXT *p_tCxt)
{
    unsigned long bufsize_option_tx = TCP_RXTEST_TX_BUF_SIZE; 
    unsigned long bufsize_option_rx = TCP_RXTEST_HT_RX_BUF_SIZE;	
    uint_8 ip_str[16];
    uint_8 high_buff_used = 1;
    uint_16 addr_len=0;
    uint_32 opt_value = TRUE;
    uint_32 port;
    uint_32 received =0;
    uint_32 keepalive_option = 1;
    int_32 conn_sock;
    _ip_address temp_addr;
    struct sockaddr_in local_addr;
    struct sockaddr_in foreign_addr;    
    TIME_STRUCT block_time;
    IPCFG_IP_ADDRESS_DATA	ip_data;	
	
    port = p_tCxt->rx_params.port;
    bench_quit = 0;
    
    num_traffic_streams++;
    
    /*Allocate buffer*/
#if USE_ATH_CHANGES    
    if((p_tCxt->buffer = _mem_alloc_system_zero(TCP_RXTEST_RX_BUF_SIZE)) == NULL) 
#else   
	if((p_tCxt->buffer = malloc(TCP_RXTEST_RX_BUF_SIZE)) == NULL)  
#endif	
    {
            printf("Out of memory error\n");
             goto ERROR_1;
    }		   

	/* Create listen socket */
    if((p_tCxt->sock_local = socket(AF_INET, SOCK_STREAM, 0)) == RTCS_SOCKET_ERROR)
    {
        printf("ERROR: Socket creation error.\r\n");
        goto ERROR_1;
    }
           
    memset(&local_addr, 0, sizeof(local_addr));
    
    local_addr.sin_port = port;
    local_addr.sin_family = AF_INET;
    
     /* Bind socket.*/
    if(bind(p_tCxt->sock_local, &local_addr, sizeof(local_addr)) == RTCS_SOCKET_ERROR)
    {
        printf("ERROR: Socket bind error.\r\n");
        goto ERROR_2;
    }

    /*Adjust initial buffer size based on number of traffic streams*/
    if(num_traffic_streams < MAX_STREAMS_ALLOWED)
    {
      /* Set socket options. */    
      if( (setsockopt(p_tCxt->sock_local, SOL_TCP, OPT_TBSIZE, (char *) &bufsize_option_tx, sizeof(bufsize_option_tx))== RTCS_SOCKET_ERROR) ||
          (setsockopt(p_tCxt->sock_local, SOL_TCP, OPT_RBSIZE, (char *) &bufsize_option_rx, sizeof(bufsize_option_rx))== RTCS_SOCKET_ERROR))
      {
          printf("ERROR: Socket setsockopt error.\r\n");
          goto ERROR_2;
      }	
    }
    else
    {
         bufsize_option_tx = TCP_RXTEST_TX_BUF_SIZE;
         bufsize_option_rx = TCP_RXTEST_RX_BUF_SIZE;
         if((setsockopt(p_tCxt->sock_local, SOL_TCP, OPT_TBSIZE, (char *) &bufsize_option_tx, sizeof(bufsize_option_tx))== RTCS_SOCKET_ERROR) ||
          (setsockopt(p_tCxt->sock_local, SOL_TCP, OPT_RBSIZE, (char *) &bufsize_option_rx, sizeof(bufsize_option_rx))== RTCS_SOCKET_ERROR))
	      {
	          printf("ERROR: Socket setsockopt error.\r\n");
	          goto ERROR_2;
	      }	
          high_buff_used = 0; 
    }
          
    if((setsockopt(p_tCxt->sock_local, SOL_TCP, OPT_RECEIVE_NOWAIT, (char *) &opt_value, sizeof(opt_value))== RTCS_SOCKET_ERROR) ||
        /* Enable keepalive_option option. */
        (setsockopt (p_tCxt->sock_local, SOL_TCP, OPT_KEEPALIVE, (char *)&keepalive_option, sizeof(keepalive_option)) == RTCS_SOCKET_ERROR) ||
        (setsockopt (p_tCxt->sock_local, SOL_TCP, OPT_RECEIVE_TIMEOUT, (char *)&rx_timeout, sizeof(rx_timeout)) == RTCS_SOCKET_ERROR)        
        )
    {
        printf("ERROR: Socket setsockopt error.\r\n");
        goto ERROR_2;
    }	
    
    
   
    
    /* Listen. */
    if(listen(p_tCxt->sock_local, 1) == RTCS_SOCKET_ERROR)
    {
        printf("ERROR: Socket listen error.\r\n");
        goto ERROR_2;
    }

    /* ------ Start test.----------- */
    printf("*************************************************\n");
    printf(" TCP RX Test\n" );
    printf("*************************************************\n");
    ipcfg_get_ip (IPCFG_default_enet_device, &ip_data);
    printf(" %-16s : %d.%d.%d.%d\n", "Local IP addr.", IPBYTES(ip_data.ip) );
    printf("Listening port %d\n", port);
    printf("Type benchquit to cancel\n");
    printf("*************************************************\n");
    
    _time_get_elapsed(&p_tCxt->first_time);
    _time_get_elapsed(&p_tCxt->last_time); 
    p_tCxt->last_bytes = 0;
    p_tCxt->last_kbytes = 0;
    
    while(1)
    {
        printf("Waiting.\n");
        
        p_tCxt->bytes = 0;
        p_tCxt->kbytes = 0;
        p_tCxt->sent_bytes = 0;
        
        if(p_tCxt->sock_peer != RTCS_SOCKET_ERROR)
        {
            shutdown(p_tCxt->sock_peer, 0);
            p_tCxt->sock_peer = RTCS_SOCKET_ERROR;
        }
        
        /* wait for connection request for 50 ms and allow for user quit */
        do{
              if(test_for_quit()){
                  goto TCP_RX_QUIT;
              }
              
              if(num_traffic_streams == MAX_STREAMS_ALLOWED && high_buff_used)
              {
                  if(adjust_buf_size(p_tCxt, 1, 1,port) == RTCS_SOCKET_ERROR)
                  {
                      printf("Unable to adjust buffer size\n");
                      goto ERROR_2;
                  }
                  high_buff_used = 0;
              }
              _time_delay(TCP_CONNECTION_WAIT_TIME);                   
              conn_sock = RTCS_selectset(&p_tCxt->sock_local, 1, -1);  	        	        	        	
        }while(conn_sock == 0);        
        
        /*Accept incoming connection*/
        addr_len = sizeof(foreign_addr);
        p_tCxt->sock_peer = accept(p_tCxt->sock_local, &foreign_addr, &addr_len);
                
        if(p_tCxt->sock_peer != RTCS_SOCKET_ERROR)
        {
#if USE_ATH_CHANGES        
            temp_addr = LONG_BE_TO_HOST(foreign_addr.sin_addr.s_addr);
#else
	    temp_addr = foreign_addr.sin_addr.s_addr;
#endif        	
        	
            printf("Receiving from %s:%d\n", inet_ntoa(*(A_UINT32 *)(&temp_addr), (char *)ip_str), foreign_addr.sin_port);
            
            _time_get_elapsed(&p_tCxt->first_time);
            _time_get_elapsed(&p_tCxt->last_time); 
            block_time = p_tCxt->first_time;
            
            while(1) /* Receiving data.*/
            {
            	if(test_for_quit()){
                    _time_get_elapsed(&p_tCxt->last_time);
                    goto TCP_RX_QUIT;
                }
        		
                received = recv(p_tCxt->sock_peer, (char*)(&p_tCxt->buffer[0]), TCP_RXTEST_RX_BUF_SIZE, 0);
                if ((received == RTCS_SOCKET_ERROR))
                {              
                    _time_get_elapsed(&p_tCxt->last_time);
                    
                    if(RTCS_geterror(p_tCxt->sock_peer) != RTCSERR_TCP_CONN_CLOSING)
                    {
                    	printf("TCP socket error %d\n",RTCS_geterror(p_tCxt->sock_peer));
                    }
                    p_tCxt->kbytes = p_tCxt->bytes/1024;
                    p_tCxt->bytes = p_tCxt->bytes % 1024;
                    p_tCxt->test_type = TCP_RX;
		    
                    /* Print throughput results.*/
                    print_test_results (p_tCxt); 
                    break;					            
                }
                else
                {
                             
                    if(test_for_delay(&p_tCxt->last_time, &block_time)){
                        /* block to give other tasks an opportunity to run */
                        _time_delay(5);
                        _time_get_elapsed(&block_time);
                    }
                
                    p_tCxt->bytes += received;
                    p_tCxt->sent_bytes += received;
                    _time_get_elapsed(&p_tCxt->last_time);
                }                              
            }
        }
        
        if(test_for_quit()){
        _time_get_elapsed(&p_tCxt->last_time);
                goto TCP_RX_QUIT;
        }
    }
    
TCP_RX_QUIT:    
    p_tCxt->kbytes = p_tCxt->bytes/1024;
    p_tCxt->bytes = p_tCxt->bytes % 1024;
	p_tCxt->test_type = TCP_RX;
    /* Print throughput results.*/
    print_test_results (p_tCxt);


    shutdown(p_tCxt->sock_peer, FLAG_ABORT_CONNECTION);
    
ERROR_2:
    shutdown(p_tCxt->sock_local, FLAG_ABORT_CONNECTION);

ERROR_1:
 
    printf("*************IOT Throughput Test Completed **************\n");
    num_traffic_streams--;
    if(p_tCxt->buffer)
    	_mem_free(p_tCxt->buffer);

}





/************************************************************************
* NAME: ath_udp_rx
*
* DESCRIPTION: Start throughput UDP server. 
************************************************************************/
static void ath_udp_rx (THROUGHPUT_CXT *p_tCxt)
{
    uint_8              ip_str[16];
    uint_16             addr_len=0;
    uint_16             port;
    int_32 		received;
    int_32 		conn_sock;
    int_32 		is_first = 1;
    uint_32 		nonblock = TRUE;
#if USE_ATH_CHANGES     
    uint_32 		old_udp_queue_size;    
#endif	
    struct sockaddr_in   addr;
    struct sockaddr_in   local_addr;	
    IPCFG_IP_ADDRESS_DATA	ip_data;
    _ip_address 	 temp_addr;
    TIME_STRUCT 	 block_time;
            
    // init quit flag
    bench_quit = 0;
    
    //Parse remaining parameters
    port = p_tCxt->rx_params.port;
    
     num_traffic_streams++;

#if USE_ATH_CHANGES  	
    /* _UDP_max_queue_size controls how many rx packets will be queued 
     * in the TCP stack.  Setting this to the Atheros buffer count ensures
     * that the TCP stack won't drop any packets thus affecting performance.
     */
    old_udp_queue_size = _UDP_max_queue_size;
    
    if(_UDP_max_queue_size < BSP_CONFIG_ATHEROS_PCB){
            _UDP_max_queue_size = BSP_CONFIG_ATHEROS_PCB;
    }	
    
    
    if((p_tCxt->buffer = _mem_alloc_system_zero(CFG_PACKET_SIZE_MAX_RX)) == NULL) 
#else   
    if((p_tCxt->buffer = malloc(CFG_PACKET_SIZE_MAX_RX)) == NULL)  
#endif	
    {
        printf("Out of memory error\n");
        return; 
    }

    /* Create listen socket */
    if((p_tCxt->sock_local = socket(AF_INET, SOCK_DGRAM, 0)) == RTCS_SOCKET_ERROR)
    {
        printf("ERROR: Socket creation error.\r\n");
        goto ERROR_1;
    }

    /* Bind.*/
    memset(&local_addr, 0, sizeof(local_addr));            
    local_addr.sin_port = port;
    local_addr.sin_family = AF_INET;

    if(bind(p_tCxt->sock_local, (&local_addr), sizeof(local_addr)) != RTCS_OK)
    {
        printf("ERROR: Socket bind error.\r\n");
        goto ERROR_2;
    }

    if(setsockopt(p_tCxt->sock_local, SOL_UDP, OPT_RECEIVE_NOWAIT, &nonblock, sizeof(nonblock))!= RTCS_OK)
    {
            printf("ERROR: Socket setsockopt error.\r\n");
    goto ERROR_2;
    }	
            
    /*************Multicast support ************************/
    if(handle_mcast_param(p_tCxt) != RTCS_OK)
    goto ERROR_2;
	

    /* ------ Start test.----------- */
    printf("*************************************************\n");
    printf(" UDP RX Test\n" );
    printf("*************************************************\n");
    ipcfg_get_ip (IPCFG_default_enet_device, &ip_data);
    printf(" %-16s : %d.%d.%d.%d\n", "Local IP addr.", IPBYTES(ip_data.ip) );
    printf( "Local port %d\n", port);
    printf("Type benchquit to cancel\n");
    printf("*************************************************\n");
    
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
        
        addr_len = sizeof(struct sockaddr_in);
        is_first = 1;
        
        while(test_for_quit()==0)
        {	        	    		
          do
          {
              if(test_for_quit()){
                      goto ERROR_3;
              }
            
              /*Check if packet is available*/    
              conn_sock = RTCS_selectall(-1);
              if(conn_sock == 0)
              {
                  /*Give an opportunity to other threads*/
                  _time_delay(1);
              }
                                                                        
          
          }while(conn_sock==0);	
			
            /* Receive data */  
            received = recvfrom(p_tCxt->sock_local, 
            					(char*)(&p_tCxt->buffer[0]), 
            					CFG_PACKET_SIZE_MAX_RX, 0,
                				&addr, (unsigned short*)&addr_len);                				               	
            

            if(received >= sizeof(EOT_PACKET))
            {   
                /* Reset timeout. */
                _time_get_elapsed(&p_tCxt->last_time);
                if(is_first)
                {
                    if( received > sizeof(EOT_PACKET) )
                    {
#if USE_ATH_CHANGES                     
                    	temp_addr = LONG_BE_TO_HOST(addr.sin_addr.s_addr);
#else
                        temp_addr = addr.sin_addr.s_addr;
#endif                    	
                    	
                        printf("Receiving from %s:%d\n", inet_ntoa(*(A_UINT32 *)(&temp_addr), (char *)ip_str), addr.sin_port);

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
                        sendAck( p_tCxt, &addr, port);
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
    shutdown(p_tCxt->sock_local, 0);
   
ERROR_1:
#if USE_ATH_CHANGES 
 	_UDP_max_queue_size = old_udp_queue_size;
#endif 	
    printf("*************IOT Throughput Test Completed **************\n");
    
    if(p_tCxt->buffer)
    	_mem_free(p_tCxt->buffer);
    
     num_traffic_streams--;
}





/************************************************************************
* NAME: tx_command_parser
*
* DESCRIPTION: 
************************************************************************/
int_32 tx_command_parser(int_32 argc, char_ptr argv[] )
{ /* Body */
    boolean           print_usage, shorthelp = FALSE;
    int_32            return_code = SHELL_EXIT_SUCCESS;
    THROUGHPUT_CXT     tCxt;
    A_INT32            retval = -1; 
    A_INT32 max_packet_size = 0;

    if(argc != 8) 
    {
        /*Incorrect number of params, exit*/
        return_code = A_ERROR;
        print_usage = TRUE;
    }       
    else
    {    
        
        /*Get IPv4 address of Peer*/
        retval = ath_inet_aton(argv[1], (A_UINT32*) &tCxt.tx_params.ip_address);
 
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

            max_packet_size = CFG_PACKET_SIZE_MAX_TX;

            
            if ((tCxt.tx_params.packet_size == 0) || (tCxt.tx_params.packet_size > max_packet_size))
            {
                printf("Invalid packet length for v4, allowed %d\n",max_packet_size); /* Print error mesage. */

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
* DESCRIPTION: 
************************************************************************/
int_32 rx_command_parser(int_32 argc, char_ptr argv[] )
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
                      ath_inet_aton(argv[3], (A_UINT32 *) & tCxt.rx_params.group.imr_multiaddr);
                      ath_inet_aton(argv[4], (A_UINT32 *) & tCxt.rx_params.group.imr_interface);                     
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







/************************************************************************
* NAME: wait_for_response
*
* DESCRIPTION: In UDP uplink test, the test is terminated by transmitting
* end-mark (single byte packet). We have implemented a feedback mechanism 
* where the Peer will reply with receive stats allowing us to display correct
* test results.
* Parameters: pointer to THROUGHPUT_CXT structure, specified port
************************************************************************/

int_32 wait_for_response(THROUGHPUT_CXT *p_tCxt, uint_16 port, sockaddr_in foreign_addr)
{	
    uint_32 received=0;
    int_32 error = RTCS_ERROR;
    uint_32 nonblock = TRUE;
    struct sockaddr_in local_addr;
    struct sockaddr_in addr;
    uint_16 addr_len;
    stat_packet_t stat_packet;
    uint_16 retry_counter = MAX_END_MARK_RETRY;
		
	/* Create listen socket */
    if((p_tCxt->sock_local = socket(AF_INET, SOCK_DGRAM, 0)) == RTCS_SOCKET_ERROR)
    {
        printf("ERROR: Socket creation error.\r\n");
        goto ERROR_1;
    }

    /* Bind.*/
    memset(&local_addr, 0, sizeof(local_addr));  
    local_addr.sin_port = port;  
    local_addr.sin_family = AF_INET;

    if(bind(p_tCxt->sock_local, (&local_addr), sizeof(local_addr)) != RTCS_OK)
    {
        printf("ERROR: Socket bind error.\r\n");
        goto ERROR_2;
    }

	if(setsockopt(p_tCxt->sock_local, SOL_UDP, OPT_RECEIVE_NOWAIT, &nonblock, sizeof(nonblock))!= RTCS_OK)
	{
		printf("ERROR: Socket setsockopt error.\r\n");
        goto ERROR_2;
	}

    addr_len = sizeof(struct sockaddr_in);                
    
    while(retry_counter)
    {		
        /* Send End mark.*/
        sendto(p_tCxt->sock_peer, (char*)(&p_tCxt->buffer[0]), sizeof(EOT_PACKET), 0,(&foreign_addr), sizeof(foreign_addr)) ;	
              
	/* block for xxx msec or until activity on socket */
	if(p_tCxt->sock_local == RTCS_selectset(&p_tCxt->sock_local, 1, 1000)){    	    	
		/* Receive data */
    	received = recvfrom (p_tCxt->sock_local, (char*)(&stat_packet), sizeof(stat_packet_t), 0,&addr, &addr_len );
	}
        if(received == sizeof(stat_packet_t))
        {   
            printf("received response\n");
            error = RTCS_OK;  
#if USE_ATH_CHANGES              
            stat_packet.msec = HOST_TO_LE_LONG(stat_packet.msec);
            stat_packet.kbytes = HOST_TO_LE_LONG(stat_packet.kbytes);
            stat_packet.bytes = HOST_TO_LE_LONG(stat_packet.bytes);
#endif
        
            p_tCxt->first_time.SECONDS = p_tCxt->last_time.SECONDS = 0;
            p_tCxt->first_time.MILLISECONDS = 0;
            p_tCxt->last_time.MILLISECONDS = stat_packet.msec;
            p_tCxt->bytes = stat_packet.bytes;        
            p_tCxt->kbytes = stat_packet.kbytes;
            break;
        }
        else
        {      
            retry_counter--;
            //printf("Did not receive response\n");
        }
    }

ERROR_2:
    shutdown(p_tCxt->sock_local, 0);

ERROR_1:
 
 return error;
    
}







/************************************************************************
* NAME: adjust_buf_size
*
* DESCRIPTION: 
* Parameters: pointer to THROUGHPUT_CXT structure, specified port
************************************************************************/
uint_32 adjust_buf_size( THROUGHPUT_CXT *p_tCxt, uint_32 direction, uint_32 transport_type, uint_16 port)
{
   uint_32 bufsize_option_tx = TCP_RXTEST_TX_BUF_SIZE;
   uint_32 bufsize_option_rx = TCP_RXTEST_RX_BUF_SIZE;
   uint_32 opt_value = TRUE;
   struct sockaddr_in local_addr;
   uint_32 keepalive_option = 1;
   uint_32 rx_timeout = ATH_RECV_TIMEOUT;
    
   shutdown(p_tCxt->sock_local, 0);  
   
   /* Create listen socket */
    if((p_tCxt->sock_local = socket(AF_INET, SOCK_STREAM, 0)) == RTCS_SOCKET_ERROR)
    {
        printf("ERROR: Socket creation error.\r\n");
        return 1;
    }
    
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_port = port;   
    local_addr.sin_family = AF_INET;
    
    /* Set socket buffer size. */
       
      if((setsockopt(p_tCxt->sock_local, SOL_TCP, OPT_TBSIZE, (char *) &bufsize_option_tx, sizeof(bufsize_option_tx))== RTCS_SOCKET_ERROR) ||
        (setsockopt(p_tCxt->sock_local, SOL_TCP, OPT_RBSIZE, (char *) &bufsize_option_rx, sizeof(bufsize_option_rx))== RTCS_SOCKET_ERROR) ||
        (setsockopt(p_tCxt->sock_local, SOL_TCP, OPT_RECEIVE_NOWAIT, (char *) &opt_value, sizeof(opt_value))== RTCS_SOCKET_ERROR) ||
        /* Enable keepalive_option option. */
        (setsockopt (p_tCxt->sock_local, SOL_TCP, OPT_KEEPALIVE, (char *)&keepalive_option, sizeof(keepalive_option)) == RTCS_SOCKET_ERROR) ||
        (setsockopt (p_tCxt->sock_local, SOL_TCP, OPT_RECEIVE_TIMEOUT, (char *)&rx_timeout, sizeof(rx_timeout)) == RTCS_SOCKET_ERROR))
    {
        printf("ERROR: Socket setsockopt error.\r\n");
        return 1;
    }	
  
    /* Bind socket.*/
    if(bind(p_tCxt->sock_local, &local_addr, sizeof(local_addr)) == RTCS_SOCKET_ERROR)
    {
        printf("ERROR: Socket bind error.\r\n");
        return 1;
    }

    /* Listen. */
    if(listen(p_tCxt->sock_local, 1) == RTCS_SOCKET_ERROR)
    {
        printf("ERROR: Socket listen error.\r\n");
       return 1;
    }
    return 0;
}





/************************************************************************
* NAME: handle_mcast_param
*
* DESCRIPTION: Handler for multicast parameters in UDp Rx test
* Parameters: pointer to THROUGHPUT_CXT structure, pointer to rx_params
************************************************************************/
int_32 handle_mcast_param(THROUGHPUT_CXT *p_tCxt)
{
	
	if(p_tCxt->rx_params.group.imr_multiaddr.s_addr != 0)
	{
		
		if(setsockopt(p_tCxt->sock_local, SOL_IGMP,RTCS_SO_IGMP_ADD_MEMBERSHIP, &(p_tCxt->rx_params.group),sizeof(struct ip_mreq)) != RTCS_OK)
		{
			printf("SetsockOPT error : unable to add to multicast group\r\n");
			return RTCS_ERROR;	
		}						
	}
	return RTCS_OK;	
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
* Parameters: pointer to THROUGHPUT_CXT structure, specified address, specified port
************************************************************************/

void sendAck(THROUGHPUT_CXT *p_tCxt, A_VOID * address, uint_16 port)
{

	int send_result;	
	int sock_err ;
	int option_len,conn_sock=0;
    uint_32 received = 0;
    uint_16 addr_len=0;    
    uint_16 retry = MAX_ACK_RETRY;	    
	stat_packet_t statPacket;    
    uint_32 sec_interval = (p_tCxt->last_time.SECONDS - p_tCxt->first_time.SECONDS);                 
    uint_32 ms_interval = (p_tCxt->last_time.MILLISECONDS - p_tCxt->first_time.MILLISECONDS); 
    uint_32 total_interval = sec_interval*1000 + ms_interval;
	struct sockaddr_in * addr = (struct sockaddr_in *)address;
	uint_8 ack_retry = 5;
	
#if USE_ATH_CHANGES     
    statPacket.kbytes = HOST_TO_LE_LONG(p_tCxt->kbytes);
    statPacket.bytes = HOST_TO_LE_LONG(p_tCxt->bytes);
    statPacket.msec = HOST_TO_LE_LONG(total_interval);
#else
	statPacket.kbytes = p_tCxt->kbytes;
    statPacket.bytes = p_tCxt->bytes;
    statPacket.msec = total_interval;
#endif     

    addr_len = sizeof(addr); 
    addr->sin_port = port;
   
    while(retry)
    {
        addr->sin_port = port;
    	   
	    send_result = sendto( p_tCxt->sock_local, (char*)(&statPacket), sizeof(stat_packet_t), 0,addr,sizeof(struct sockaddr_in));
	    
	    if ( send_result == RTCS_SOCKET_ERROR )
	    {                     
	        option_len = sizeof(sock_err); 
	        getsockopt(p_tCxt->sock_local, SOL_SOCKET, OPT_SOCKET_ERROR, (char*)&sock_err, (unsigned long*)&option_len);
	        printf("socket_error = %d\n", sock_err);
	        break;       
	    }
	    else
	    {	
	        ack_retry = 5;    			
	        while(ack_retry)
	        {
	        	/* block for 200msec or until a packet is received */
		    	conn_sock = RTCS_selectset(&p_tCxt->sock_local, 1, 200);
		    		
		    	if(conn_sock == 0)
		    	{	    			    	
		    		printf("ACK success\n");    	    	
		    		break;
		    	}
		    	else
		    	{
		    		 received = recvfrom(p_tCxt->sock_local, 
	            					(char*)(&p_tCxt->buffer[0]), 
	            					CFG_PACKET_SIZE_MAX_RX, 0,
	                				addr, &addr_len);     
	                //if(received)
	                //	printf("Unexpected rx %d\n",received);                				       
		    	}
		    	ack_retry--;	    	
	        }
	        
	        if(conn_sock == 0)
	        	break;
	    	
	    }
	    retry--;
    }   
}

#endif //!ENABLE_STACK_OFFLOAD



