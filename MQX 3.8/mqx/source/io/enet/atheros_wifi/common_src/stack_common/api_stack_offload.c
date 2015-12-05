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
#include "a_config.h"
#include <a_types.h>
#include <a_osapi.h>
#include <driver_cxt.h>
#include <common_api.h>
#include <custom_api.h>
#include "wmi_api.h"
#include "a_drv_api.h"
#include "cust_netbuf.h"

#include "atheros_stack_offload.h"
#include "common_stack_offload.h"
#include "custom_stack_offload.h"

#include "atheros_wifi_api.h"


ATH_SOCKET_CONTEXT* ath_sock_context[MAX_SOCKETS_SUPPORTED + 1];

// custom ping command
static A_INT32 custom_ping( void ) 
{
    ATH_IOCTL_PARAM_STRUCT ath_param;
    ath_param.cmd_id = ATH_ASSERT_DUMP;
    ath_param.data   = NULL;
    ath_param.length = 0;
    return (A_INT32) handle_ioctl(&ath_param);  
}

// ping the 4100P and dump debug info before giving up the ghost
static void dump_error_info( A_DRIVER_CONTEXT *pDCxt, A_UINT32 handle, A_UINT32 ip, char *func  )
{
    printf("ERR %s()\n4100P: 0x%x\n", func, custom_ping());
    printf("drv_up=%d, chipDown=%d\n", pDCxt->driver_up, pDCxt->chipDown);
    if (handle) 
        printf("hdl=%x\n", handle);
    if (ip)
        printf("IP=%x\n", ip);
    fflush(stdout);
}


/*****************************************************************************/
/*  find_socket_context - Finds socket context based on socket handle. "retrieve"
 *                flag is used to identify if a new context is requested or an
 *                exiting context is retrieved.
 *      A_UINT32 handle - socket handle.
 * 		A_UINT8 retrieve - see description
 * Returns- index in context array
 *****************************************************************************/
A_INT32 find_socket_context(A_UINT32 handle, A_UINT8 retrieve)
{
    A_INT32 index = 0;
    if(retrieve)
    {
        if(handle == 0)
        return SOCKET_NOT_FOUND;
    }

    /*If handle is 0, we want to find first empty context and return the index.
    If handle is non zero, return the index that matches */	
    for (index = 0; index < MAX_SOCKETS_SUPPORTED; index++)
    {
        if(ath_sock_context[index]->handle == handle)
        return index;
    }
    /*Index not found*/
    return SOCKET_NOT_FOUND;
}



/*****************************************************************************/
/*  socket_context_init - Initializes common socket context, called during 
 *                 driver init.
 *      
 * Returns- A_OK in case of successful init, A_ERROR otherwise
 *****************************************************************************/
A_STATUS socket_context_init()
{
	A_UINT32 index = 0;
	
	for (index = 0; index < MAX_SOCKETS_SUPPORTED + 1; index++)	
	{
		/*Allocate socket contexts*/
		if((ath_sock_context[index] = A_MALLOC(sizeof(ATH_SOCKET_CONTEXT),MALLOC_ID_CONTEXT)) == NULL)
			return A_NO_MEMORY;

		ath_sock_context[index]->data = NULL;
		ath_sock_context[index]->sock_st_mask = 0;
		ath_sock_context[index]->handle = 0;
		ath_sock_context[index]->domain = 0;
		ath_sock_context[index]->type = 0;
		ath_sock_context[index]->result = 0;
	}
	
	/*initialize custom socket context*/
	if(CUSTOM_SOCKET_CONTEXT_INIT() != A_OK)
		return A_ERROR;

		
	return A_OK;
}




/*****************************************************************************/
/*  socket_context_deinit - De-initializes common socket context, called during 
 *                 driver deinit.
 *****************************************************************************/
A_VOID socket_context_deinit()
{
	A_UINT32 index = 0;
	for (index = 0; index < MAX_SOCKETS_SUPPORTED + 1; index++)	
	{
		A_FREE(ath_sock_context[index],MALLOC_ID_CONTEXT);
	}
        /*Free the custom context as well*/
        CUSTOM_SOCKET_CONTEXT_DEINIT();
}



/*****************************************************************************/
/*  Api_SockResponseEventRx - Handler for WMI socket receive events.
 *  A_VOID *pCxt  - driver context
 *  A_UINT8 *datap - pointer to incoming event data
 *  A_UINT32 len   - length of event data 
 *****************************************************************************/
A_STATUS Api_SockResponseEventRx(A_VOID *pCxt, A_UINT8 *datap, A_UINT32 len, A_VOID *pReq)
{
    A_INT32 index = 0;
    A_UINT8 unblock_flag=0;
    WMI_SOCK_RESPONSE_EVENT *response = (WMI_SOCK_RESPONSE_EVENT*)datap;
    A_UINT32 resp_type = A_CPU2LE32(response->resp_type);
    A_STATUS status = A_OK;
    A_UINT8 freeBuf = 1;
    
    switch(resp_type)
    {		
    case SOCK_OPEN:                
        index = find_socket_context(SOCKET_HANDLE_PLACEHOLDER, TRUE);

        if(index < 0 || index > MAX_SOCKETS_SUPPORTED)
        {
            last_driver_error = A_SOCKCXT_NOT_FOUND;
            status = A_ERROR;
            break;
        }   
        /*Check if a socket is waiting on the response*/
        if(SOCK_EV_MASK_TEST(ath_sock_context[index], resp_type))
        {
            /*Store the newly created socket handle*/
            ath_sock_context[index]->handle = A_CPU2LE32(response->sock_handle);        
            /* unlock the application thread*/
            unblock_flag = 1;                      
        }     
        break;            
        
    case SOCK_ACCEPT:
        index = find_socket_context(A_CPU2LE32(response->sock_handle), TRUE);
        
        if(index < 0 || index > MAX_SOCKETS_SUPPORTED)
        {
            last_driver_error = A_SOCKCXT_NOT_FOUND;
            status = A_ERROR;
            break;
        }	
        
        /*Check if a socket is waiting on the response*/
        if(SOCK_EV_MASK_TEST(ath_sock_context[index], resp_type))
        {
            /*Copy incoming socket related information*/
            ath_sock_context[index]->data = response->data; 
            ath_sock_context[index]->pReq = pReq;            
            
            /*Store the response in socket context result field*/
            ath_sock_context[index]->result = A_CPU2LE32(response->error);        
            /*unlock the thread*/
            unblock_flag=1;
            freeBuf = 0;
        }
        break;		
    case SOCK_CLOSE:
        /* A socket close event may be received under two scenarios-
          1. In response to explicit socket close request from host.
          2. An unsolicited socket close to close a socket created earlier for
             an incoming TCP connection
          In the first scenario, the application thread is waiting for the response, 
          so unblock the thread. 
          In the second case, just cleanup the socket context*/
        
        index = find_socket_context(A_CPU2LE32(response->sock_handle), TRUE);
        
        if(index < 0 || index > MAX_SOCKETS_SUPPORTED)
        {
            last_driver_error = A_SOCKCXT_NOT_FOUND;         
            status = A_ERROR;
            break;
        }		
        
        if(SOCK_EV_MASK_TEST(ath_sock_context[index], resp_type)){
            /* the blocked flag will clear the context. clearing 
             * the context here will erase the mask which would 
             * break up the task synchronization. */
            /*unlock the thread*/
            unblock_flag=1;     
            /*Store the response in socket context result field*/
            ath_sock_context[index]->result = A_CPU2LE32(response->error);
        }else{
            /*Store the response in socket context result field*/
            unblock_flag=1;     
            ath_sock_context[index]->result = HALF_CLOSE;
        }   
        break;	
    case SOCK_CONNECT:
    case SOCK_BIND:
    case SOCK_LISTEN:
      /*Listen Event may be received in two cases:
      Case 1: In response to a listen request from host, that puts TCP socket in
              Listen state.
      Case 2: An unsolicited listen with a result value TCP_CONNECTION_AVAILABLE,
              that informs an application thread that a new TCP connection is 
              available. This allows the application to call accept.*/
    case SOCK_ERRNO:
    case SOCK_SETSOCKOPT:
        index = find_socket_context(A_CPU2LE32(response->sock_handle), TRUE);
        
        if(index < 0 || index > MAX_SOCKETS_SUPPORTED)
        {
            last_driver_error = A_SOCKCXT_NOT_FOUND;
            status = A_ERROR;
            break;
        }	
        /*Check if a socket is waiting on the response*/
        if(SOCK_EV_MASK_TEST(ath_sock_context[index], resp_type))
        {	
            /*Store the response in socket context result field*/
            ath_sock_context[index]->result = A_CPU2LE32(response->error);        
            /*unlock the thread*/
            unblock_flag=1;    
        }else{
            if(A_CPU2LE32(response->error) == TCP_CONNECTION_AVAILABLE)
            {
                /*Special case: async listen event has been received indicating a new
                TCP connection is available. The User thread may be blocked with a select
                call, set the unblock_flag so the user thread can be unlocked*/
                ath_sock_context[index]->result = A_CPU2LE32(response->error);
                unblock_flag=1;
            }  
        }
        break;	        
    case SOCK_GETSOCKOPT:
        index = find_socket_context(A_CPU2LE32(response->sock_handle), TRUE);
        
        if(index < 0 || index > MAX_SOCKETS_SUPPORTED)
        {
            last_driver_error = A_SOCKCXT_NOT_FOUND;
            status = A_ERROR;
            break;
        }

        /*Check if a socket is waiting on the response*/
        if(SOCK_EV_MASK_TEST(ath_sock_context[index], resp_type))
        {
            ath_sock_context[index]->result = A_CPU2LE32(response->error);        
            /*Copy select related information*/
            ath_sock_context[index]->data = response->data; 
            ath_sock_context[index]->pReq = pReq;            
            /* unlock the application thread*/
            unblock_flag=1;        
            freeBuf = 0;
        }
        break;       
    case SOCK_IPCONFIG:
        index = GLOBAL_SOCK_INDEX;
        /*Check if a socket is waiting on the response*/
        if(SOCK_EV_MASK_TEST(ath_sock_context[index], resp_type))
        {
            /*Copy ipconfig related information*/
            ath_sock_context[index]->data = response->data; 
            ath_sock_context[index]->pReq = pReq;                        
            ath_sock_context[index]->result = A_CPU2LE32(response->error);        
            /*unlock the thread*/
            unblock_flag=1;  
            freeBuf = 0;
        }
        break;        
    case SOCK_PING:
    case SOCK_PING6:
        index = GLOBAL_SOCK_INDEX;
        /*Check if a socket is waiting on the response*/
        if(SOCK_EV_MASK_TEST(ath_sock_context[index], resp_type))
        {
            /*Copy ipconfig related information*/		
            ath_sock_context[index]->result = A_CPU2LE32(response->error);		
            /*unlock the thread*/
            unblock_flag=1;		
        }
       
        break;
    case SOCK_IP6CONFIG:
        index = GLOBAL_SOCK_INDEX;
        /*Check if a socket is waiting on the response*/
        if(SOCK_EV_MASK_TEST(ath_sock_context[index], resp_type))
        {
            /*Copy ipconfig related information*/
            ath_sock_context[index]->data = response->data;             
            ath_sock_context[index]->result = A_CPU2LE32(response->error);	
            ath_sock_context[index]->pReq = pReq;
            /*unlock the thread*/
            unblock_flag=1;
            freeBuf = 0;
        }
        break;		
    default:
        last_driver_error = A_UNKNOWN_CMD;
        status = A_ERROR;
        break;	
    }
    /* if a user task is blocked on this event then unblock it. */
    if(unblock_flag /* && SOCK_EV_MASK_TEST(ath_sock_context[index], resp_type)*/){
        SOCK_EV_MASK_CLEAR(ath_sock_context[index], resp_type); 
        CUSTOM_UNBLOCK(ath_sock_context[index], RX_DIRECTION);    
    }
    
    if(freeBuf){
        /*Free the netbuf*/
        A_NETBUF_FREE(pReq);   
    }    
    return status;	
}



/*****************************************************************************/
/*  getIPLength - Calculates total IP header length.
 *  A_UINT8 version - IP version    
 * Returns- IP header length
 *****************************************************************************/
A_UINT32 getIPLength(A_UINT8 version)
{
	A_UINT32 length = 0;
	switch(version)
	{
		case ATH_AF_INET:
			length = IPV4_HEADER_LENGTH;
		break;
		
		case ATH_AF_INET6:
			length = IPV6_HEADER_LENGTH;
		break;
		default:
		break;		
	}
	
	return length;
	
}


/*****************************************************************************/
/*  getTransportLength - Calculates Transport layer header length
 *  A_UINT8 proto - UDP or TCP    
 * Returns- Transport layer header length
 *****************************************************************************/
A_UINT32 getTransportLength(A_UINT8 proto)
{
	A_UINT32 length = 0;
	switch(proto)
	{
		case SOCK_STREAM_TYPE:
			length = TCP_HEADER_LENGTH;
			break;
		
		case SOCK_DGRAM_TYPE:
			length = UDP_HEADER_LENGTH;
			break;
		default:
			break;		
	}
	
	return length;
	
}



/*****************************************************************************/
/*  Api_socket - API to create new socket
 *  A_VOID *pCxt- driver context
 *  A_UINT32 domain- IPv4, IPv6 
 *  A_UINT32 type- TCP, UDP
 *  A_UINT32 protocol-    
 * Returns- socket handle in case of success, A_ERROR otherwise
 *****************************************************************************/
A_INT32 Api_socket(A_VOID *pCxt, A_UINT32 domain, A_UINT32 type, A_UINT32 protocol)
{
    A_DRIVER_CONTEXT *pDCxt;
    SOCK_OPEN_T sock_open;
    A_INT32 index = 0;
    A_INT32 result = A_OK;
    
    pDCxt = GET_DRIVER_COMMON(pCxt);
    
    do{
        /*Create new context*/
        if((index = find_socket_context(EMPTY_SOCKET_CONTEXT,FALSE)) != SOCKET_NOT_FOUND)
        {
            ath_sock_context[index]->handle = SOCKET_HANDLE_PLACEHOLDER;
            ath_sock_context[index]->domain = domain;
            ath_sock_context[index]->type = type;
        }
        else
        {
            last_driver_error = A_SOCK_UNAVAILABLE;
            result = A_ERROR;
            break;
        }
    
        /*Create socket open wmi message*/
        sock_open.domain = A_CPU2LE32(domain);
        sock_open.type = A_CPU2LE32(type);
        sock_open.protocol = A_CPU2LE32(protocol);

        SOCK_EV_MASK_SET(ath_sock_context[index], SOCK_OPEN);    

        if(wmi_socket_cmd(pDCxt->pWmiCxt, SOCK_OPEN, (void *)(&sock_open), sizeof(SOCK_OPEN_T)) != A_OK){
            SOCK_EV_MASK_CLEAR(ath_sock_context[index], SOCK_OPEN);
            /*Command failed, clear socket context and return error*/
            clear_socket_context(index);
            result = A_ERROR;
            break;
        }
    
        /*Wait for response from target*/
        do{
            if(CUSTOM_BLOCK(pCxt, ath_sock_context[index], COMMAND_BLOCK_TIMEOUT, RX_DIRECTION) != A_OK){               
                dump_error_info( pDCxt, 0, 0, (char*)__func__ );
                return A_WHISTLE_REBOOT;
            }       
        }while(SOCK_EV_MASK_TEST(ath_sock_context[index], SOCK_OPEN));    
    
        if(ath_sock_context[index]->handle == A_ERROR)
        {
            /*Socket not created, return error*/
            clear_socket_context(index);            
            result = A_ERROR;
            break;
        }
        else{
            result = ath_sock_context[index]->handle;
        }
    }while(0);  
    
    return result;
}


/*****************************************************************************/
/*  Api_shutdown - Close a previously opened socket
 *  A_VOID *pCxt- driver context 
 *  A_UINT32 handle- socket handle    
 * Returns- 0 in case of successful shutdown, A_ERROR otherwise
 *****************************************************************************/
A_INT32 Api_shutdown(A_VOID *pCxt, A_UINT32 handle)
{
    SOCK_CLOSE_T sock_close;
    A_INT32 index = 0;
    A_DRIVER_CONTEXT *pDCxt;
    A_INT32 result = A_OK;
    
    pDCxt = GET_DRIVER_COMMON(pCxt);  
    
    // Check for a live device
    if ((result=custom_ping())!= A_OK)
    {
        dump_error_info( pDCxt, handle, 0, (char*)__func__);
        // NOTE: we might try returning A_ERROR here to see if things can be recovered at a higher level
        return A_WHISTLE_REBOOT;
    }
    
    do{    
        /*Find context*/
        if((index = find_socket_context(handle,TRUE)) == SOCKET_NOT_FOUND){
            result = A_ERROR;
            break;
        }
    
        /*Check if some other thread is wating on this socket*/
        if(IS_SOCKET_BLOCKED(ath_sock_context[index])){
            result = A_ERROR;
            break;
        }
    
        /*Delete all pending packets in the receive queue*/
        CUSTOM_PURGE_QUEUE(index);
    
        /*Create a socket close wmi message*/
        sock_close.handle = A_CPU2LE32(handle);
        /* set the sock_st_flags before calling wmi_ to avoid possible race conditions */
        SOCK_EV_MASK_SET(ath_sock_context[index], SOCK_CLOSE);
        
        if(wmi_socket_cmd(pDCxt->pWmiCxt, SOCK_CLOSE, (void *)(&sock_close), sizeof(SOCK_CLOSE_T)) != A_OK){
            /* clear the flag that would have been cleared by receiving the event */
            SOCK_EV_MASK_CLEAR(ath_sock_context[index], SOCK_CLOSE);
            result = A_ERROR;
            break;	
        }
    
        /*Wait for response from target*/		
        do{
            if(CUSTOM_BLOCK(pCxt, ath_sock_context[index], COMMAND_BLOCK_TIMEOUT, RX_DIRECTION) != A_OK){
                dump_error_info( pDCxt, handle, 0, (char*)__func__ );
                return A_WHISTLE_REBOOT;
            }  
        }while(SOCK_EV_MASK_TEST(ath_sock_context[index], SOCK_CLOSE));
        
        result = ath_sock_context[index]->result;
        /* clear the socket to make it available for re-use */
        clear_socket_context(index);
        
    }while(0);
    
    return result;	
}



/*****************************************************************************/
/*  Api_connect - API to connect to a peer
 *  A_VOID *pCxt- driver context 
 *  A_UINT32 handle- socket handle    
 *  A_VOID* name- sock addr structure 
 *  A_UINT16 length- sock add length
 * Returns- 0 in case of successful connect, A_ERROR otherwise
 *****************************************************************************/
A_INT32 Api_connect(A_VOID *pCxt, A_UINT32 handle, A_VOID* name, A_UINT16 length)
{
    A_DRIVER_CONTEXT *pDCxt;
    SOCK_CONNECT_CMD_T sock_connect;
    A_INT32 index = 0;
    A_INT32 result = A_OK;
    
    pDCxt = GET_DRIVER_COMMON(pCxt);  
    
    do{
        /*Retrieve context*/
        if((index = find_socket_context(handle,TRUE)) == SOCKET_NOT_FOUND)
        {
            result = A_ERROR;
            break;
        }
    
        if(IS_SOCKET_BLOCKED(ath_sock_context[index])){
            result = A_ERROR;
            break;
        }
    
        sock_connect.handle = A_CPU2LE32(handle);
        sock_connect.length = A_CPU2LE16(length);
    
        if(ath_sock_context[index]->domain == ATH_AF_INET)
        {
            sock_connect.addr.name.sin_port = A_CPU2LE16(((SOCKADDR_T*)name)->sin_port);
            sock_connect.addr.name.sin_family = A_CPU2LE16(((SOCKADDR_T*)name)->sin_family);
            sock_connect.addr.name.sin_addr = A_CPU2LE32(((SOCKADDR_T*)name)->sin_addr);
        }
        else
        {
            sock_connect.addr.name6.sin6_port = A_CPU2LE16(((SOCKADDR_6_T*)name)->sin6_port);
            sock_connect.addr.name6.sin6_family = A_CPU2LE16(((SOCKADDR_6_T*)name)->sin6_family);
            sock_connect.addr.name6.sin6_flowinfo = A_CPU2LE32(((SOCKADDR_6_T*)name)->sin6_flowinfo);	
            A_MEMCPY((A_UINT8*)&(sock_connect.addr.name6.sin6_addr),(A_UINT8*)&((SOCKADDR_6_T*)name)->sin6_addr,sizeof(IP6_ADDR_T));
        }

        SOCK_EV_MASK_SET(ath_sock_context[index], SOCK_CONNECT);

        if(wmi_socket_cmd(pDCxt->pWmiCxt, SOCK_CONNECT, (void *)(&sock_connect), sizeof(SOCK_CONNECT_CMD_T)) != A_OK){
            SOCK_EV_MASK_CLEAR(ath_sock_context[index], SOCK_CONNECT);
            result = A_ERROR;
            break;
        }
    
        do{
            if( (result = CUSTOM_BLOCK(pCxt, ath_sock_context[index], COMMAND_BLOCK_TIMEOUT, RX_DIRECTION)) != A_OK){
                dump_error_info( pDCxt, handle, sock_connect.addr.name.sin_addr, (char*)__func__ );
                return A_WHISTLE_REBOOT;
            }      
        }while(SOCK_EV_MASK_TEST(ath_sock_context[index], SOCK_CONNECT));
        result = ath_sock_context[index]->result;
    }while(0);
    
    return result;
	
}



/*****************************************************************************/
/*  Api_bind - API to bind to an interface. Works for both IPv4 and v6, name 
 *            field must be populated accordingly. 
 *  A_VOID *pCxt- driver context 
 *  A_UINT32 handle- socket handle    
 *  A_VOID* name- sock addr structure 
 *  A_UINT16 length- sock add length
 * Returns- 0 in case of successful connect, A_ERROR otherwise
 *****************************************************************************/
A_INT32 Api_bind(A_VOID *pCxt, A_UINT32 handle, A_VOID* name, A_UINT16 length)
{
    A_DRIVER_CONTEXT *pDCxt;
    SOCK_BIND_CMD_T sock_bind;
    A_INT32 index = 0;
    A_INT32 result = A_OK;
    
    pDCxt = GET_DRIVER_COMMON(pCxt);   
    
    do{
        /*Find context*/
        if((index = find_socket_context(handle,TRUE)) == SOCKET_NOT_FOUND)
        {
            result = A_ERROR;
            break;
        }
    
        if(IS_SOCKET_BLOCKED(ath_sock_context[index])){
            result = A_ERROR;
            break;
        }
        sock_bind.handle = A_CPU2LE32(handle);
        sock_bind.length = A_CPU2LE16(length);
    
        if(ath_sock_context[index]->domain == ATH_AF_INET)
        {		
            sock_bind.addr.name.sin_port = A_CPU2LE16(((SOCKADDR_T*)name)->sin_port);
            sock_bind.addr.name.sin_family = A_CPU2LE16(((SOCKADDR_T*)name)->sin_family);
            sock_bind.addr.name.sin_addr = A_CPU2LE32(((SOCKADDR_T*)name)->sin_addr);			
        }
        else
        {
            sock_bind.addr.name6.sin6_port = A_CPU2LE16(((SOCKADDR_6_T*)name)->sin6_port);
            sock_bind.addr.name6.sin6_family = A_CPU2LE16(((SOCKADDR_6_T*)name)->sin6_family);
            sock_bind.addr.name6.sin6_flowinfo = A_CPU2LE32(((SOCKADDR_6_T*)name)->sin6_flowinfo);	
            A_MEMCPY((A_UINT8*)&(sock_bind.addr.name6.sin6_addr),(A_UINT8*)&((SOCKADDR_6_T*)name)->sin6_addr,sizeof(IP6_ADDR_T));
        }
    
        SOCK_EV_MASK_SET(ath_sock_context[index], SOCK_BIND);

        if(wmi_socket_cmd(pDCxt->pWmiCxt, SOCK_BIND, (void *)(&sock_bind), sizeof(SOCK_BIND_CMD_T)) != A_OK){
            SOCK_EV_MASK_CLEAR(ath_sock_context[index], SOCK_BIND);
            result = A_ERROR;
            break;
        }
    
        do{
            if(CUSTOM_BLOCK(pCxt, ath_sock_context[index], COMMAND_BLOCK_TIMEOUT, RX_DIRECTION) != A_OK){
                dump_error_info( pDCxt, handle, sock_bind.addr.name.sin_addr, (char*)__func__ );
                A_ASSERT(0);
            }   
        }while(SOCK_EV_MASK_TEST(ath_sock_context[index], SOCK_BIND));
        result = ath_sock_context[index]->result;
    }while(0);
    
    return result;	
}





/*****************************************************************************/
/*  Api_listen - API to Listen for incoming connections. Only used on stream sockets
 *               Works for both IPv4 and v6.
 *  A_VOID *pCxt- driver context 
 *  A_UINT32 handle- socket handle    
 *  A_UINT32 backlog- backlog of pending connections. The backlog parameter defines 
 *                    the maximum length for the queue of pending connections
 * Returns- 0 in case of success, A_ERROR otherwise
 *****************************************************************************/
A_INT32 Api_listen(A_VOID *pCxt, A_UINT32 handle, A_UINT32 backlog)
{
    A_DRIVER_CONTEXT *pDCxt;
    SOCK_LISTEN_T sock_listen;
    A_INT32 index = 0;
    A_INT32 result = A_OK;
    
    pDCxt = GET_DRIVER_COMMON(pCxt);   
    
    do{
        /*Find context*/
        if((index = find_socket_context(handle,TRUE)) == SOCKET_NOT_FOUND)
        {
            result = A_ERROR;
            break;
        }
    
        if(IS_SOCKET_BLOCKED(ath_sock_context[index])){
            result = A_ERROR;
            break;
        }
    
        sock_listen.handle = A_CPU2LE32(handle);
        sock_listen.backlog = A_CPU2LE16(backlog);
    
        SOCK_EV_MASK_SET(ath_sock_context[index], SOCK_LISTEN);    

        if(wmi_socket_cmd(pDCxt->pWmiCxt, SOCK_LISTEN, (void *)(&sock_listen), sizeof(SOCK_LISTEN_T)) != A_OK){
            SOCK_EV_MASK_CLEAR(ath_sock_context[index], SOCK_LISTEN);
            result = A_ERROR;
            break;
        }
    
        /*Block until stack provides a response*/
        do{
            if(CUSTOM_BLOCK(pCxt, ath_sock_context[index], COMMAND_BLOCK_TIMEOUT, RX_DIRECTION) != A_OK){
                dump_error_info( pDCxt, handle, 0, (char*)__func__ );
                A_ASSERT(0);
            }
        }while(SOCK_EV_MASK_TEST(ath_sock_context[index], SOCK_LISTEN));
        
        result = ath_sock_context[index]->result;
    }while(0);
    
    return result;	
}




/*****************************************************************************/
/*  Api_accept - API to accept incoming inconnections. Works for both IPv4 and v6,  
 *            name field must be populated accordingly. Must call select to wait 
 *            for incoming connection before calling this API.
 *  A_VOID *pCxt- driver context 
 *  A_UINT32 handle- socket handle    
 *  A_VOID* name- sock addr structure 
 *  A_UINT16 length- sock add length
 * Returns- 0 in case of successful connect, A_ERROR otherwise
 *****************************************************************************/
A_INT32 Api_accept(A_VOID *pCxt, A_UINT32 handle, A_VOID* name, A_UINT16 length)
{
    A_DRIVER_CONTEXT *pDCxt;
    SOCK_ACCEPT_CMD_T sock_accept;
    A_INT32 index = 0;	
    A_INT32 accept_index = 0;
    A_INT32 result = A_OK;
    A_UINT8 free_buf = 1;
    
    pDCxt = GET_DRIVER_COMMON(pCxt);   
    
    do{
        /*Find context*/
        if((index = find_socket_context(handle,TRUE)) == SOCKET_NOT_FOUND)
        {
            result = A_ERROR;
            free_buf = 0;
            break;
        }
        if(IS_SOCKET_BLOCKED(ath_sock_context[index])){
            result = A_ERROR;
            free_buf = 0;
            break;
        }
        /* prepare wmi accept structure*/
        sock_accept.handle = A_CPU2LE32(handle);
        sock_accept.length = A_CPU2LE16(length);		
    
        SOCK_EV_MASK_SET(ath_sock_context[index], SOCK_ACCEPT);

        if(wmi_socket_cmd(pDCxt->pWmiCxt, SOCK_ACCEPT, (void *)(&sock_accept), sizeof(SOCK_ACCEPT_CMD_T))!= A_OK){
            SOCK_EV_MASK_CLEAR(ath_sock_context[index], SOCK_ACCEPT);
            result = A_ERROR;
            free_buf = 0;
            break;
        }
    
        /*Block until stack provides a response*/
        do{
            if(CUSTOM_BLOCK(pCxt, ath_sock_context[index], COMMAND_BLOCK_TIMEOUT, RX_DIRECTION) != A_OK){
                dump_error_info( pDCxt, handle, 0, (char*)__func__ );
                A_ASSERT(0);
            }
        }while(SOCK_EV_MASK_TEST(ath_sock_context[index], SOCK_ACCEPT));
    
        if(ath_sock_context[index]->result == TCP_CONNECTION_AVAILABLE)
        {
            /*Special case: Received TCP_CONN_AVAILABLE from target in response to accept, call
            Accept again to create new connection, free the previos netbuf*/
            if(free_buf){   
                /*Free the netbuf*/
                A_NETBUF_FREE(ath_sock_context[index]->pReq); 
            }
            /*Target stack indicates that a new TCP connection is available, accept it*/      
            SOCK_EV_MASK_SET(ath_sock_context[index], SOCK_ACCEPT);

            if(wmi_socket_cmd(pDCxt->pWmiCxt, SOCK_ACCEPT, (void *)(&sock_accept), sizeof(SOCK_ACCEPT_CMD_T))!=A_OK){
                SOCK_EV_MASK_CLEAR(ath_sock_context[index], SOCK_ACCEPT);
                result = A_ERROR;
                free_buf = 0;
                break;
            }
        
            /*Block until stack provides a response*/
            do{
                if(CUSTOM_BLOCK(pCxt, ath_sock_context[index], COMMAND_BLOCK_TIMEOUT, RX_DIRECTION) != A_OK){
                    dump_error_info( pDCxt, handle, 0, (char*)__func__ );
                    A_ASSERT(0);
                }
            }while(SOCK_EV_MASK_TEST(ath_sock_context[index], SOCK_ACCEPT));
        }

        if(ath_sock_context[index]->result != A_ERROR)
        {
            /*Create new context*/
            if((accept_index = find_socket_context(EMPTY_SOCKET_CONTEXT,FALSE)) != SOCKET_NOT_FOUND)
            {
                ath_sock_context[accept_index]->handle = ath_sock_context[index]->result;
                ath_sock_context[accept_index]->domain = ath_sock_context[index]->domain;
                ath_sock_context[accept_index]->type = ath_sock_context[index]->type;
            }
            else
            {
                last_driver_error = A_SOCK_UNAVAILABLE;                
                result = A_ERROR;
                break;
            }
        
            if(ath_sock_context[index]->data != NULL)
            {
                /*Based on IPv4 vs IPv6, fill in name fields*/
                if(ath_sock_context[index]->domain == ATH_AF_INET)
                {		
                        A_MEMCPY(name, &((SOCK_ACCEPT_RECV_T*)(ath_sock_context[index]->data))->addr.name, sizeof(SOCKADDR_T));
                        ((SOCKADDR_T*)name)->sin_port = A_CPU2LE16(((SOCKADDR_T*)name)->sin_port);
                        ((SOCKADDR_T*)name)->sin_family = A_CPU2LE16(((SOCKADDR_T*)name)->sin_family);
                        ((SOCKADDR_T*)name)->sin_addr = A_CPU2LE32(((SOCKADDR_T*)name)->sin_addr);			
                }
                else
                {
                        A_MEMCPY(name, &((SOCK_ACCEPT_RECV_T*)(ath_sock_context[index]->data))->addr.name6, sizeof(SOCKADDR_6_T));
                        ((SOCKADDR_6_T*)name)->sin6_port = A_CPU2LE16(((SOCKADDR_6_T*)name)->sin6_port);
                        ((SOCKADDR_6_T*)name)->sin6_family = A_CPU2LE16(((SOCKADDR_6_T*)name)->sin6_family);
                }			
                //A_FREE(ath_sock_context[index]->data, MALLOC_ID_CONTEXT);
               
                ath_sock_context[index]->data = NULL;
            }	       	
        }	
        result = ath_sock_context[index]->result;	 
    }while(0);
    
    if(free_buf){
        /*Free the netbuf*/
        A_NETBUF_FREE(ath_sock_context[index]->pReq);
    }
    return result;
}



/*****************************************************************************/
/*  Api_errno - API to fetch last error code from target  
 *            
 *  A_VOID *pCxt- driver context 
 *  A_UINT32 handle- socket handle    
 * Returns- error code in case of success, A_ERROR otherwise
 *****************************************************************************/
A_INT32 Api_errno(A_VOID *pCxt, A_UINT32 handle)
{
    A_DRIVER_CONTEXT *pDCxt;
    A_INT32 index = 0;
    A_INT32 result = A_OK;
    SOCK_ERRNO_T sock_errno;
    
    pDCxt = GET_DRIVER_COMMON(pCxt);   
    
    do{
        /*Find context*/
        if((index = find_socket_context(handle,TRUE)) == SOCKET_NOT_FOUND)
        {
            result = A_ERROR;
            break;
        }
        if(IS_SOCKET_BLOCKED(ath_sock_context[index])){
            result = A_ERROR;
            break;
        }
        sock_errno.errno = A_CPU2LE32(handle);
        SOCK_EV_MASK_SET(ath_sock_context[index], SOCK_ERRNO);

        if(wmi_socket_cmd(pDCxt->pWmiCxt, SOCK_ERRNO, (A_UINT8*)&sock_errno, sizeof(SOCK_ERRNO_T)) != A_OK){
            SOCK_EV_MASK_CLEAR(ath_sock_context[index], SOCK_ERRNO);
            result = A_ERROR;
            break;
        }
    
        do{
            if(CUSTOM_BLOCK(pCxt, ath_sock_context[index], COMMAND_BLOCK_TIMEOUT, RX_DIRECTION) != A_OK){
                dump_error_info( pDCxt, handle, 0, (char*)__func__ );
                A_ASSERT(0);
            }        
        }while(SOCK_EV_MASK_TEST(ath_sock_context[index], SOCK_ERRNO));
        result = ath_sock_context[index]->result;
    }while(0);
    
    return result;	
}


/*****************************************************************************/
/*  Api_select -   
 *            
 *  A_VOID *pCxt- driver context 
 *  A_UINT32 handle- socket handle    
 *  A_UINT32 tv - time to wait in milliseconds
 * Returns- 0 in case of successful connect, A_ERROR otherwise
 *  Select can be called to check for incoming connections or for incoming data.
 *  It waits for a specified time period before returning. This call does not 
 *  propagate to the target, it is consumed in the host.
 *****************************************************************************/
A_INT32 Api_select(A_VOID *pCxt, A_UINT32 handle, A_UINT32 tv)
{
    A_INT32 index = 0;
    A_INT32 result = A_OK;
    
    do {
        /*Find context*/
        if ((index = find_socket_context(handle,TRUE)) == SOCKET_NOT_FOUND) {
            result = A_SOCK_INVALID;
            break;
        }

        /* Is some already blocked on the socket */
        if (IS_SOCKET_BLOCKED(ath_sock_context[index])) {
            result = A_ERROR;
            break;
        }
        
        /* Data available ?*/
        if (!CUSTOM_QUEUE_EMPTY(index)) {
            result = A_OK;
            break;
        }
    
        /*Wait for specified time*/
        result = CUSTOM_BLOCK(pCxt, ath_sock_context[index], tv, RX_DIRECTION);
        if (result != A_OK) {
            /*Check if there was a local close socket while we were waiting*/
            if(ath_sock_context[index]->handle == 0) {
                result = A_SOCK_INVALID;
                break;
            }
            
            /*Timeout, no activity detected*/
            result = A_ERROR;
            break;
        }
        
        /*Something is available, it may be a new incoming connection or data*/ 
        if (ath_sock_context[index]->result == TCP_CONNECTION_AVAILABLE) {
            ath_sock_context[index]->result = 0;
            result = A_OK;
            break;
        }
        
        /* Check for remote close with no data available */ 
        if ((ath_sock_context[index]->result == HALF_CLOSE) && CUSTOM_QUEUE_EMPTY(index))
        {
            result = A_SOCK_INVALID;
            break;
        }
        
    } while (1);
    
    return result;
}



/*****************************************************************************/
/*  Api_setsockopt - API to set specified socket option   
 *            name field must be populated accordingly. 
 *  A_VOID *pCxt- driver context 
 *  A_UINT32 handle- socket handle    
 *  A_UINT32 level- option level 
 *  A_UINT32 optname- option name
 *  A_UINT32 optlen- option length
 * Returns- 0 in case of success, A_ERROR otherwise
 *****************************************************************************/
A_INT32 Api_setsockopt(A_VOID *pCxt, A_UINT32 handle, A_UINT32 level, A_UINT32 optname, A_UINT8* optval, A_UINT32 optlen)
{
    A_DRIVER_CONTEXT *pDCxt;
    SOCK_OPT_T* sockSetopt;
    //	A_UINT8* data = NULL;
    A_INT32 index = 0, total_length = 0;
    A_INT32 result = A_OK;
    
    pDCxt = GET_DRIVER_COMMON(pCxt);   
    
    do{
        /*Find context*/
        if((index = find_socket_context(handle,TRUE)) == SOCKET_NOT_FOUND)
        {
            result = A_ERROR;
            break;
        }
        if(IS_SOCKET_BLOCKED(ath_sock_context[index])){
            result = A_ERROR;
            break;
        }    
        total_length = sizeof(SOCK_OPT_T);
    
        if(optval != NULL && optlen != 0)
        {
            total_length += optlen - sizeof(A_UINT8);						
        }
    
        /*Allocate space for option*/
        if((sockSetopt = A_MALLOC(total_length,MALLOC_ID_CONTEXT)) == NULL){
            result = A_NO_MEMORY;            
            break;
        }
    
        if(optval != NULL && optlen != 0)
        {
            A_MEMCPY(sockSetopt->optval, optval, optlen);					
        }		
            
        sockSetopt->handle = A_CPU2LE32(handle);
        sockSetopt->level = A_CPU2LE32(level);
        sockSetopt->optname = A_CPU2LE32(optname);
        sockSetopt->optlen = A_CPU2LE32(optlen);	

        SOCK_EV_MASK_SET(ath_sock_context[index], SOCK_SETSOCKOPT);
                
        if(wmi_socket_cmd(pDCxt->pWmiCxt, SOCK_SETSOCKOPT, (A_UINT8*)sockSetopt, total_length)!=A_OK){
            SOCK_EV_MASK_CLEAR(ath_sock_context[index], SOCK_SETSOCKOPT);
            result = A_ERROR;
            break;
        }
            
        do{
            if(CUSTOM_BLOCK(pCxt, ath_sock_context[index], COMMAND_BLOCK_TIMEOUT, RX_DIRECTION) != A_OK){
                dump_error_info( pDCxt, handle, 0, (char*)__func__ );
                A_ASSERT(0);
            }
        }while(SOCK_EV_MASK_TEST(ath_sock_context[index], SOCK_SETSOCKOPT));
    
        A_FREE(sockSetopt, MALLOC_ID_CONTEXT);	
        result = ath_sock_context[index]->result;
    }while(0);	
    
    return result;	
}


/*****************************************************************************/
/*  Api_getsockopt - API to fetch specified socket option   
 *            name field must be populated accordingly. 
 *  A_VOID *pCxt- driver context 
 *  A_UINT32 handle- socket handle    
 *  A_UINT32 level- option level 
 *  A_UINT32 optname- option name
 *  A_UINT32 optlen- option length
 * Returns- 0 in case of success, A_ERROR otherwise
 *****************************************************************************/
A_INT32 Api_getsockopt(A_VOID *pCxt, A_UINT32 handle, A_UINT32 level, A_UINT32 optname, A_UINT8* optval, A_UINT32 optlen)
{
    A_DRIVER_CONTEXT *pDCxt;
    SOCK_OPT_T* sock_getopt;
    A_UINT32 index = 0, total_length = 0;
    A_INT32 result = A_OK;
    A_UINT8 free_buf = 1;
    
    pDCxt = GET_DRIVER_COMMON(pCxt);   

    do{    
        /*Find context*/
        if((index = find_socket_context(handle,TRUE)) == SOCKET_NOT_FOUND)
        {
            result = A_ERROR;
            free_buf = 0;
            break;
        }
    
        /*Check if socket is blocked for a previous command*/
        if(IS_SOCKET_BLOCKED(ath_sock_context[index])){
            result = A_ERROR;
            free_buf = 0;            
            break;
        }
        /*Cannot call getsockopt with NULL*/
        if(optval == NULL || optlen == 0){
            result = A_ERROR;
            free_buf = 0;            
            break;
        }
        /*Total length depends upon the type of option*/
        total_length = sizeof(SOCK_OPT_T) + optlen - sizeof(A_UINT8);	
    
        /*Allocate buffer for option*/
        if((sock_getopt = A_MALLOC(total_length,MALLOC_ID_CONTEXT)) == NULL){
            result = A_NO_MEMORY;
            free_buf = 0;            
            break;
        }
           
        sock_getopt->handle = A_CPU2LE32(handle);
        sock_getopt->level = A_CPU2LE32(level);
        sock_getopt->optname = A_CPU2LE32(optname);
        sock_getopt->optlen = A_CPU2LE32(optlen);	
        
        SOCK_EV_MASK_SET(ath_sock_context[index], SOCK_GETSOCKOPT);            
        /*Send the packet*/	
        if(wmi_socket_cmd(pDCxt->pWmiCxt, SOCK_GETSOCKOPT, (A_UINT8*)sock_getopt, total_length) != A_OK){
            SOCK_EV_MASK_CLEAR(ath_sock_context[index], SOCK_GETSOCKOPT);
            result = A_ERROR;
            free_buf = 0;            
            break;
        }
    
        /*Wait for response from stack*/
        do{
            if(CUSTOM_BLOCK(pCxt, ath_sock_context[index], COMMAND_BLOCK_TIMEOUT, RX_DIRECTION) != A_OK){
                dump_error_info( pDCxt, handle, 0, (char*)__func__ );
                A_ASSERT(0);
            }
        }while(SOCK_EV_MASK_TEST(ath_sock_context[index], SOCK_GETSOCKOPT));	
    
        if(ath_sock_context[index]->data != NULL)
        {
            A_MEMCPY(optval,((SOCK_OPT_T*)ath_sock_context[index]->data)->optval, optlen);
            //A_FREE(ath_sock_context[index]->data,MALLOC_ID_CONTEXT);
            ath_sock_context[index]->data = NULL;
        }
        result = ath_sock_context[index]->result;
    }while(0);
    
    A_FREE(sock_getopt,MALLOC_ID_CONTEXT);
    
    if(free_buf){
        /*Free the netbuf*/
        A_NETBUF_FREE(ath_sock_context[index]->pReq);
    }
    
    return result;	
}



/*****************************************************************************/
/*  Api_ipconfig - API to obtain IP address information from target 
 *  A_VOID *pCxt- driver context 
 *  A_UINT32 mode- query vs set    
 *  A_UINT32* ipv4_addrA_UINT32* subnetMask, A_UINT32* gateway4
 * Returns- 0 in case of successful connect, A_ERROR otherwise
 *****************************************************************************/
A_INT32 Api_ipconfig(A_VOID *pCxt, A_UINT32 mode,A_UINT32* ipv4_addr, A_UINT32* subnetMask, A_UINT32* gateway4)
{
    A_DRIVER_CONTEXT *pDCxt;
    IPCONFIG_CMD_T ipcfg;
    IPCONFIG_RECV_T *result;
    A_UINT32 index = GLOBAL_SOCK_INDEX; //reserved for global commands ToDo- cleanup later    
    A_INT32 res = A_OK;
    A_UINT8 free_buf = 1;
    
    pDCxt = GET_DRIVER_COMMON(pCxt);   
    
    do{
        /*Check if socket is blocked for a previous command*/
        if(IS_SOCKET_BLOCKED(ath_sock_context[GLOBAL_SOCK_INDEX])){
            res = A_ERROR;
            free_buf = 0;
            break;
        }
        if(mode == 1)
        {
            /*This is not a query or dhcp command*/
            ipcfg.mode = A_CPU2LE32(mode);
            ipcfg.ipv4 = A_CPU2LE32(*ipv4_addr);
            ipcfg.subnetMask = A_CPU2LE32(*subnetMask);
            ipcfg.gateway4 = A_CPU2LE32(*gateway4);	
        }
        ipcfg.mode = A_CPU2LE32(mode);	

        SOCK_EV_MASK_SET(ath_sock_context[index], SOCK_IPCONFIG);

        if(wmi_socket_cmd(pDCxt->pWmiCxt, SOCK_IPCONFIG, (void *)(&ipcfg), sizeof(IPCONFIG_CMD_T))!=A_OK){
            SOCK_EV_MASK_CLEAR(ath_sock_context[index], SOCK_IPCONFIG);
            res = A_ERROR;
            free_buf = 0;
            break;
        }
    
        do{
            // CA - This was blocking for 960000 ms! Changed it to something reasonable. Now that it can actually time out, don't die if it does.
#if 0
            if(CUSTOM_BLOCK(pCxt, ath_sock_context[index], COMMAND_BLOCK_TIMEOUT, RX_DIRECTION) != A_OK){
                A_ASSERT(0);
            }
#else
            if(CUSTOM_BLOCK(pCxt, ath_sock_context[index], COMMAND_BLOCK_TIMEOUT, RX_DIRECTION) != A_OK){
                //printf("RECOVERING: Api_ipconfig1\n"); fflush(stdout);
                //SOCK_EV_MASK_CLEAR(ath_sock_context[index], SOCK_IPCONFIG);
                //res = A_ERROR;
                //free_buf = 0;
                // ^^ the above does not result in a recovery. something is wrong with the error code propagation up the stack and
                // EVTMGR never gets the indication that a request failed and hangs forever waiting for something to happen. conmgr apparently
                // is waiting, too, and wifi is hung even though the rest of the system is functioning.
                // For now we will handle this by rebooting, just like in the RX and TX cases.
                // TODO: need to find who calls this and propogate the return call back up, then reboot properly.
                return A_WHISTLE_REBOOT;
                break;                
            }
#endif          
        }while(SOCK_EV_MASK_TEST(ath_sock_context[index], SOCK_IPCONFIG));     

        // catch error from inner loop
        if (res != A_OK)
          {
            break; // out of outer loop
          }
    
        /*Got a response*/
        if(ath_sock_context[index]->result != -1)
        {
            result = (IPCONFIG_RECV_T*)(ath_sock_context[index]->data);
            if(mode != 1)
            {
                *ipv4_addr = A_CPU2LE32(result->ipv4);
                *subnetMask = A_CPU2LE32(result->subnetMask);
                *gateway4 = A_CPU2LE32(result->gateway4);	
            }		
        }
        res = ath_sock_context[index]->result;
    }while(0);
    
    if(free_buf){
        /*Free the netbuf*/
        A_NETBUF_FREE(ath_sock_context[index]->pReq);
    }
    return res;	
}

/*****************************************************************************/
/*  Api_ip6config - API to obtain IPv6 address information from target 
 *  A_VOID *pCxt- driver context 
 *  A_UINT32 mode- query vs set    
 *  
 * Returns- 0 in case of successful connect, A_ERROR otherwise
 *****************************************************************************/
A_INT32 Api_ip6config(A_VOID *pCxt, A_UINT32 mode,IP6_ADDR_T *v6Global, IP6_ADDR_T *v6Link, IP6_ADDR_T *v6DefGw,IP6_ADDR_T *v6GlobalExtd, A_INT32 *LinkPrefix,
		      A_INT32 *GlbPrefix, A_INT32 *DefgwPrefix, A_INT32 *GlbPrefixExtd)
{
    A_DRIVER_CONTEXT *pDCxt;
    IPCONFIG_CMD_T ipcfg;
    IPCONFIG_RECV_T *result;
    A_UINT32 index = GLOBAL_SOCK_INDEX; //reserved for global commands ToDo- cleanup later
    A_INT32 res = A_OK;
    A_UINT8 free_buf = 1;

    pDCxt = GET_DRIVER_COMMON(pCxt);   

    do{    
        /*Check if socket is blocked for a previous command*/
        if(IS_SOCKET_BLOCKED(ath_sock_context[GLOBAL_SOCK_INDEX])){
            res = A_ERROR;
            free_buf = 0;
            break;
        }
            
        ipcfg.mode = A_CPU2LE32(mode);	

        SOCK_EV_MASK_SET(ath_sock_context[index], SOCK_IP6CONFIG);

        if(wmi_socket_cmd(pDCxt->pWmiCxt, SOCK_IP6CONFIG, (void *)(&ipcfg), sizeof(IPCONFIG_CMD_T))!= A_OK){
            SOCK_EV_MASK_CLEAR(ath_sock_context[index], SOCK_IP6CONFIG);
            res = A_ERROR;
            free_buf = 0;
            break;
        }
    
        do{
            if(CUSTOM_BLOCK(pCxt, ath_sock_context[index], COMMAND_BLOCK_TIMEOUT, RX_DIRECTION) != A_OK){
                dump_error_info( pDCxt, 0, 0, (char*)__func__ );
                A_ASSERT(0);
            }
   
        }while(SOCK_EV_MASK_TEST(ath_sock_context[index], SOCK_IP6CONFIG));
    
        /*Got a response*/
        if(ath_sock_context[index]->result != -1)
        {
            
            result = (IPCONFIG_RECV_T*)(ath_sock_context[index]->data);
    
             A_MEMCPY(v6Global,&result->ipv6GlobalAddr,sizeof(IP6_ADDR_T));
             A_MEMCPY(v6Link,&result->ipv6LinkAddr,sizeof(IP6_ADDR_T));
             A_MEMCPY(v6DefGw,&result->ipv6DefGw,sizeof(IP6_ADDR_T));
             A_MEMCPY(v6GlobalExtd,&result->ipv6LinkAddrExtd,sizeof(IP6_ADDR_T));
             *LinkPrefix = A_CPU2LE32(result->LinkPrefix);
             *GlbPrefix = A_CPU2LE32(result->GlbPrefix);
             *DefgwPrefix = A_CPU2LE32 (result->DefGwPrefix);
             *GlbPrefixExtd = A_CPU2LE32 (result->GlbPrefixExtd);
                                             
        }
        res = ath_sock_context[index]->result;
    }while(0);

    if(free_buf){ 
        /*Free the netbuf*/
        A_NETBUF_FREE(ath_sock_context[index]->pReq);
    }
    
    return res;	
}


/*****************************************************************************/
/*  Api_ipconfig_dhcp_pool - API to configure dhcp pool 
 *  A_VOID *pCxt- driver context 
 *  A_UINT32 *start_ipv4_addr -   Start ip address    
 *  A_UINT32* end_ipv4_addr A_INT32 leasetime
 * Returns- 0 in case of successful connect, A_ERROR otherwise
 *****************************************************************************/
A_INT32 Api_ipconfig_dhcp_pool(A_VOID *pCxt,A_UINT32* start_ipv4_addr, A_UINT32* end_ipv4_addr, A_INT32 leasetime)
{
    A_DRIVER_CONTEXT *pDCxt;
    IPCONFIG_DHCP_POOL_T ipcfg, *result;
    A_UINT32 index = GLOBAL_SOCK_INDEX; //reserved for global commands ToDo- cleanup later
    pDCxt = GET_DRIVER_COMMON(pCxt);   
    
    /*Check if socket is blocked for a previous command*/
    if(IS_SOCKET_BLOCKED(ath_sock_context[GLOBAL_SOCK_INDEX])){
            return A_ERROR;
    }
            ipcfg.startaddr = A_CPU2LE32(*start_ipv4_addr);
            ipcfg.endaddr = A_CPU2LE32(*end_ipv4_addr);
            ipcfg.leasetime = A_CPU2LE32(leasetime);	

    if(wmi_socket_cmd(pDCxt->pWmiCxt, SOCK_IPCONFIG_DHCP_POOL, (void *)(&ipcfg), sizeof(IPCONFIG_DHCP_POOL_T))!=A_OK){
        return A_ERROR;
    }
    
    return (A_OK);	
}

/*****************************************************************************/
/*  Api_ip6config_router_prefix - API to configure router prefix 
 *  A_VOID *pCxt- driver context 
 *  A_UINT8 *v6addr -   v6prefix    
 *  A_INT32  prefixlen A_INT32 prefix_lifetime ,A_INT32 valid_lifetime 
 * Returns- 0 in case of successful connect, A_ERROR otherwise
 *****************************************************************************/
A_INT32 Api_ip6config_router_prefix(A_VOID *pCxt,A_UINT8 *v6addr,A_INT32 prefixlen,A_INT32 prefix_lifetime,A_INT32 valid_lifetime)
{
    A_DRIVER_CONTEXT *pDCxt;
    IP6CONFIG_ROUTER_PREFIX_T ip6cfg, *result;
    A_UINT32 index = GLOBAL_SOCK_INDEX; //reserved for global commands ToDo- cleanup later
    pDCxt = GET_DRIVER_COMMON(pCxt);   

    /*Check if socket is blocked for a previous command*/
    if(IS_SOCKET_BLOCKED(ath_sock_context[GLOBAL_SOCK_INDEX])){
            return A_ERROR;
    }
    A_MEMCPY(ip6cfg.v6addr,v6addr,16);
    ip6cfg.prefixlen = A_CPU2LE32(prefixlen);
    ip6cfg.prefix_lifetime = A_CPU2LE32(prefix_lifetime);
    ip6cfg.valid_lifetime = A_CPU2LE32(valid_lifetime);

    if(wmi_socket_cmd(pDCxt->pWmiCxt, SOCK_IP6CONFIG_ROUTER_PREFIX, (void *)(&ip6cfg), sizeof(IP6CONFIG_ROUTER_PREFIX_T))!=A_OK){
        return A_ERROR;
    }
    return (A_OK);
}
	
/*****************************************************************************/
/*  Api_ipconfig_set_tcp_exponential_backoff_retry - API to tcp exponential backoff retry 
 *  A_VOID *pCxt- driver context 
 *  A_INT32 retry -   No of MAX Retries
 * Returns- 0 in case of successful connect, A_ERROR otherwise
 *****************************************************************************/
A_INT32 Api_ipconfig_set_tcp_exponential_backoff_retry(A_VOID *pCxt,A_INT32 retry)
{
    A_DRIVER_CONTEXT *pDCxt = NULL;
    SOCK_IP_BACKOFF_T sock_backoff;

    pDCxt = GET_DRIVER_COMMON(pCxt);   
    /*Check if socket is blocked for a previous command*/
    if(IS_SOCKET_BLOCKED(ath_sock_context[GLOBAL_SOCK_INDEX])){
            return A_ERROR;
    }
    sock_backoff.max_retry = A_CPU2LE32(retry);
    if(wmi_socket_cmd(pDCxt->pWmiCxt, SOCK_IP_SET_TCP_EXP_BACKOFF_RETRY,(void *)(&sock_backoff), sizeof(SOCK_IP_BACKOFF_T))!=A_OK){
        return A_ERROR;
    }
    return (A_OK);
}	

/*****************************************************************************/
/*  Api_ipconfig_set_ip6_status - API to set ip6 status 
 *  A_VOID *pCxt- driver context 
 *  A_UINT16 status -   enable or diable
 * Returns- 0 in case of successful connect, A_ERROR otherwise
 *****************************************************************************/
A_INT32 Api_ipconfig_set_ip6_status(A_VOID *pCxt,A_UINT16 status)
{
    A_DRIVER_CONTEXT *pDCxt = NULL;
    SOCK_IPv6_STATUS_T sock_ip6status;

    pDCxt = GET_DRIVER_COMMON(pCxt);   
    /*Check if socket is blocked for a previous command*/
    if(IS_SOCKET_BLOCKED(ath_sock_context[GLOBAL_SOCK_INDEX])){
            return A_ERROR;
    }
    sock_ip6status.ipv6_status = A_CPU2LE16(status);
    if(wmi_socket_cmd(pDCxt->pWmiCxt, SOCK_IP_SET_IP6_STATUS,(void *)(&sock_ip6status),sizeof(sock_ip6status))!=A_OK){
        return A_ERROR;
    }
    return (A_OK);
}

/*****************************************************************************/
/*  Api_ipconfig_dhcp_release - API to release dhcp ip address 
 *  A_VOID *pCxt- driver context 
 * Returns- 0 in case of successful connect, A_ERROR otherwise
 *****************************************************************************/
A_INT32 Api_ipconfig_dhcp_release(A_VOID *pCxt)
{

    A_DRIVER_CONTEXT *pDCxt = NULL;
    SOCK_IP_DHCP_RELEASE_T release;
    A_UINT16 ifindx = 0;

    pDCxt = GET_DRIVER_COMMON(pCxt);   
    /*Check if socket is blocked for a previous command*/
    if(IS_SOCKET_BLOCKED(ath_sock_context[GLOBAL_SOCK_INDEX])){
            return A_ERROR;
    }
    release.ifIndex = A_CPU2LE16(ifindx);
    if(wmi_socket_cmd(pDCxt->pWmiCxt, SOCK_IP_DHCP_RELEASE,(void *)(&release),sizeof(SOCK_IP_DHCP_RELEASE_T))!=A_OK){
        return A_ERROR;
    }
    return (A_OK);
}

A_INT32 Api_ipconfig_set_tcp_rx_buffer(A_VOID *pCxt,A_INT32 rxbuf)
{

    A_DRIVER_CONTEXT *pDCxt = NULL;
    SOCK_IP_TCP_RX_BUF_T sock_tcp_rx_buf;

    pDCxt = GET_DRIVER_COMMON(pCxt);   
    /*Check if socket is blocked for a previous command*/
    if(IS_SOCKET_BLOCKED(ath_sock_context[GLOBAL_SOCK_INDEX])){
            return A_ERROR;
    }
    sock_tcp_rx_buf.rxbuf = A_CPU2LE32(rxbuf);
    if(wmi_socket_cmd(pDCxt->pWmiCxt, SOCK_IP_SET_TCP_RX_BUF,(void *)(&sock_tcp_rx_buf), sizeof(SOCK_IP_TCP_RX_BUF_T))!=A_OK){
        return A_ERROR;
    }
    return (A_OK);

}	
/*****************************************************************************/
/*  Api_ping - API to obtain IP address information from target 
 *  A_VOID *pCxt- driver context   
 *  A_UINT32* ipv4_addr
 * Returns- 0 in case of successful connect, A_ERROR otherwise
 *****************************************************************************/
A_INT32 Api_ping(A_VOID *pCxt, A_UINT32 ipv4_addr, A_UINT32 size)
{
    A_DRIVER_CONTEXT *pDCxt;
    PING_T ping;
    A_UINT32 index = GLOBAL_SOCK_INDEX; //reserved for global commands ToDo- cleanup later
    A_INT32 result = A_OK;
    
    pDCxt = GET_DRIVER_COMMON(pCxt);   
    
    do{
        /*Check if socket is blocked for a previous command*/
        if(IS_SOCKET_BLOCKED(ath_sock_context[index])){
            result = A_ERROR;
            break;
        }
        ping.ip_addr = A_CPU2LE32(ipv4_addr);
        ping.size = A_CPU2LE32(size);	

        SOCK_EV_MASK_SET(ath_sock_context[index], SOCK_PING);

        if(wmi_socket_cmd(pDCxt->pWmiCxt, SOCK_PING, (void *)(&ping), sizeof(PING_T))!=A_OK){
            SOCK_EV_MASK_CLEAR(ath_sock_context[index], SOCK_PING);
            result = A_ERROR;
            break;
        }
    
        do{
            if(CUSTOM_BLOCK(pCxt, ath_sock_context[index], COMMAND_BLOCK_TIMEOUT, RX_DIRECTION) != A_OK){
                dump_error_info( pDCxt, 0, ipv4_addr, (char*)__func__ );
                A_ASSERT(0);
            }
 
        }while(SOCK_EV_MASK_TEST(ath_sock_context[index], SOCK_PING));	
        result = ath_sock_context[index]->result;
    }while(0);
    
    return result;	
}

/*****************************************************************************/
/*  Api_ping6 - API to obtain IP address information from target 
 *  A_VOID *pCxt- driver context   
 *  A_UINT32* ipv4_addr
 * Returns- 0 in case of successful connect, A_ERROR otherwise
 *****************************************************************************/
A_INT32 Api_ping6(A_VOID *pCxt, A_UINT8 *ip6addr, A_UINT32 size)
{
    A_DRIVER_CONTEXT *pDCxt;
    PING_6_T ping6;
    A_UINT32 index = GLOBAL_SOCK_INDEX; //reserved for global commands ToDo- cleanup later
    A_INT32 result = A_OK;
    
    pDCxt = GET_DRIVER_COMMON(pCxt);   
    
    do{
        /*Check if socket is blocked for a previous command*/
        if(IS_SOCKET_BLOCKED(ath_sock_context[index])){
          result = A_ERROR;
          break;
        }
        A_MEMCPY(ping6.ip6addr,ip6addr,sizeof(ping6.ip6addr));
        ping6.size = A_CPU2LE32(size);

        SOCK_EV_MASK_SET(ath_sock_context[index], SOCK_PING6);

        if(wmi_socket_cmd(pDCxt->pWmiCxt, SOCK_PING6, (void *)(&ping6), sizeof(PING_6_T))!=A_OK){
            SOCK_EV_MASK_CLEAR(ath_sock_context[index], SOCK_PING6);
            result = A_ERROR;
            break;
        }
    
        do{
            if(CUSTOM_BLOCK(pCxt, ath_sock_context[index], COMMAND_BLOCK_TIMEOUT, RX_DIRECTION) != A_OK){
                dump_error_info( pDCxt, 0, 0, (char*)__func__ );
                A_ASSERT(0);
            }
        }while(SOCK_EV_MASK_TEST(ath_sock_context[index], SOCK_PING6));	
        result = ath_sock_context[index]->result;
    }while(0);
    
    return result;	
}

/*****************************************************************************/
/*  clear_socket_context - clears all elements of socket context 
 *  A_INT32 - index to socket context 
 * Returns- None
 *****************************************************************************/
A_VOID clear_socket_context(A_INT32 index)
{		
	ath_sock_context[index]->handle = 0;
	ath_sock_context[index]->domain = 0;
	ath_sock_context[index]->type = 0;
	ath_sock_context[index]->sock_st_mask = 0;
	
	/*If for some reason, data field is not freed, free it here
	  This may happen when a command response comes later than Timeout period*/
	if(ath_sock_context[index]->data)
	{
		A_FREE(ath_sock_context[index]->data,MALLOC_ID_CONTEXT);
		ath_sock_context[index]->data = NULL;	
	}
}


/*****************************************************************************/
/*  send_stack_init - Sends Stack initialization information.
 * RETURNS: A_OK on success A_ERROR otherwise
 *****************************************************************************/
A_STATUS send_stack_init(A_VOID* pCxt)
{
    A_DRIVER_CONTEXT *pDCxt;
    A_UINT8 buffer[sizeof(WMI_SOCKET_CMD)+4];
    WMI_SOCKET_CMD *sock_cmd = (WMI_SOCKET_CMD *)&buffer[0];
    pDCxt = GET_DRIVER_COMMON(pCxt);
    
    buffer[8] = 1;
    buffer[9] = 2;
    buffer[10] = MAX_SOCKETS_SUPPORTED;
    
    if(wmi_socket_cmd(pDCxt->pWmiCxt, SOCK_STACK_INIT, (void *)(sock_cmd), sizeof(WMI_SOCKET_CMD)+4)!=A_OK){
        return A_ERROR;
    }  

    return A_OK;       
}
