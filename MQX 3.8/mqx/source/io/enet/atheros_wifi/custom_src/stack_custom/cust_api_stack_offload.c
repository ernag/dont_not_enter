#include <a_config.h>
#include <a_types.h>
#include <a_osapi.h>
#include <driver_cxt.h>
#include <common_api.h>
#include <custom_api.h>
#include <wmi_api.h>
#include <htc.h>
#include "mqx.h"
#include "bsp.h"
#include "enet.h"
#include "enetprv.h"
#include "atheros_wifi.h"
#include "enet_wifi.h"
#include "atheros_wifi_api.h"
#include "atheros_wifi_internal.h"
#include "cust_netbuf.h"

#include "atheros_stack_offload.h"
#include "common_stack_offload.h"
#include "custom_stack_offload.h"




#if ZERO_COPY
A_NETBUF_QUEUE_T zero_copy_free_queue;
#endif



/*****************************************************************************/
/*  custom_socket_context_init - Initializes the custom section of socket context.
 * RETURNS: A_OK on success or error otherwise. 
 *****************************************************************************/
A_STATUS custom_socket_context_init()
{
    SOCKET_CUSTOM_CONTEXT_PTR pcustctxt = NULL;
    A_UINT32 index;
    
    for (index = 0; index < MAX_SOCKETS_SUPPORTED + 1; index++)	
    {	
      if((pcustctxt = A_MALLOC(sizeof(SOCKET_CUSTOM_CONTEXT),MALLOC_ID_CONTEXT)) == NULL){
                return A_NO_MEMORY;
      }
        _lwevent_create(&pcustctxt->sockWakeEvent, 0/* no auto clear */); 		
        A_NETBUF_QUEUE_INIT(&(pcustctxt->rxqueue));
        pcustctxt->blockFlag = 0;
        pcustctxt->respAvailable = A_FALSE;
        ath_sock_context[index]->sock_custom_context = pcustctxt;
        
 #if NON_BLOCKING_TX
         A_NETBUF_QUEUE_INIT(&(pcustctxt->non_block_queue));
         A_MUTEX_INIT(&(pcustctxt->nb_tx_mutex));
#endif	
    }
                
    #if ZERO_COPY	
    /*Initilize common queue used to store Rx packets for zero copy option*/
    A_NETBUF_QUEUE_INIT(&zero_copy_free_queue);
    #endif
    
    
    return A_OK;
}

/*****************************************************************************/
/*  custom_socket_context_deinit - De-initializes the custom section of socket context.
 * RETURNS: A_OK on success or error otherwise. 
 *****************************************************************************/
A_STATUS custom_socket_context_deinit()
{
	SOCKET_CUSTOM_CONTEXT_PTR pcustctxt = NULL;
	A_UINT32 index;
	
	for (index = 0; index < MAX_SOCKETS_SUPPORTED + 1; index++)	
	{	
           pcustctxt = GET_CUSTOM_SOCKET_CONTEXT(ath_sock_context[index]); 
#if NON_BLOCKING_TX            
            A_MUTEX_DELETE(&(pcustctxt->nb_tx_mutex));
#endif	          		
	    _lwevent_destroy(&pcustctxt->sockWakeEvent); 		
             A_FREE(pcustctxt,MALLOC_ID_CONTEXT);
	}			

	return A_OK;
}



/*****************************************************************************/
/*  blockForResponse - blocks a thread, either indefinitely or for a specified
 *                     time period in milliseconds
 * RETURNS: A_OK on success or error if timeout occurs. 
 *****************************************************************************/
A_STATUS blockForResponse(A_VOID* pCxt, A_VOID* ctxt, A_UINT32 msec, A_UINT8 direction)
{
	A_UINT32 ret = (msec)*BSP_ALARM_FREQUENCY/1000; 
	A_STATUS result = A_OK;
	SOCKET_CUSTOM_CONTEXT_PTR pcustctxt = GET_CUSTOM_SOCKET_CONTEXT(ctxt);
	
	pcustctxt->blockFlag = 1;	
       
    if(direction == RX_DIRECTION)
    {
        
        if(pcustctxt->respAvailable == A_FALSE)
        {
            pcustctxt->rxBlock = A_TRUE;    
           	if(MQX_OK != _lwevent_wait_ticks(&pcustctxt->sockWakeEvent, 0x01, FALSE, ret)){  
    			result = A_ERROR;	                                               
    		}else{                                                      
        		_lwevent_clear(&pcustctxt->sockWakeEvent, 0x01);                            
    		}     
            pcustctxt->rxBlock = A_FALSE;       
        }
        
        if(pcustctxt->respAvailable == A_TRUE)
        {
                /*Response is available, reset the flag for future use*/
                result = A_OK;
                pcustctxt->respAvailable = A_FALSE;
        }
        
    }
    else if(direction == TX_DIRECTION)
    {
        if(pcustctxt->txUnblocked == A_FALSE)
        {
            pcustctxt->txBlock = A_TRUE;    
           	if(MQX_OK != _lwevent_wait_ticks(&pcustctxt->sockWakeEvent, 0x01, FALSE, ret)){  
    			result = A_ERROR;	                                               
    		}else{                                                      
        		_lwevent_clear(&pcustctxt->sockWakeEvent, 0x01);                            
    		}     
            pcustctxt->txBlock = A_FALSE;       
        }
        
        if(pcustctxt->txUnblocked == A_TRUE)
        {
                /*Response is available, reset the flag for future use*/
                result = A_OK;
                pcustctxt->txUnblocked = A_FALSE;
        }
    }
    else
    {
      last_driver_error = A_EINVAL;
    }
        
	pcustctxt->blockFlag = 0;
	return result;
}


/*****************************************************************************/
/*  isSocketBlocked - Checks if a thread is blocked on a given socket                
 * RETURNS: value of block flag 
 *****************************************************************************/
A_UINT32 isSocketBlocked(A_VOID* ctxt)
{
	SOCKET_CUSTOM_CONTEXT_PTR pcustctxt = GET_CUSTOM_SOCKET_CONTEXT(ctxt);	
	return pcustctxt->blockFlag;
}

/*****************************************************************************/
/*  unblock - Unblocks a thread if it is blocked.                
 * RETURNS: A_OK if unblock was successful, A_ERROR if thread was not blocked 
 *****************************************************************************/
A_STATUS unblock(A_VOID* ctxt, A_UINT8 direction)
{
	A_STATUS result = A_OK;
	SOCKET_CUSTOM_CONTEXT_PTR pcustctxt = GET_CUSTOM_SOCKET_CONTEXT(ctxt);

	/*Unblock task if it is blocked*/
	
	 if(direction == RX_DIRECTION)
        {
            pcustctxt->respAvailable = A_TRUE;
            /*Unblock task if it is blocked*/
            if(pcustctxt->rxBlock == A_TRUE) 
            {                    
               _lwevent_set(&pcustctxt->sockWakeEvent, 0x01);
            }
        }
        else if(direction == TX_DIRECTION)
        {
            pcustctxt->txUnblocked = A_TRUE;
            if(pcustctxt->txBlock == A_TRUE) 
            {                    
               _lwevent_set(&pcustctxt->sockWakeEvent, 0x01);
            }
        }
	return result;	
}



/*****************************************************************************/
/*  t_socket - Custom version of socket API- please see Api_socket for details                
 * RETURNS: socket handle or A_ERROR 
 *****************************************************************************/
A_INT32 t_socket(ENET_CONTEXT_STRUCT_PTR enet_ptr, A_UINT32 domain, A_UINT32 type, A_UINT32 protocol)
{
	A_VOID *pCxt;
	
	if((pCxt = enet_ptr->MAC_CONTEXT_PTR) == NULL)
	{
	   	return A_ERROR;
	}  
	/*Wait for chip to be up*/
    if(A_OK != Api_DriverAccessCheck(pCxt, 1, ACCESS_REQUEST_IOCTL)){
        return A_ERROR;
    }
	 
	return (Api_socket(pCxt, domain, type, protocol));
}


/*****************************************************************************/
/*  t_shutdown - Custom version of socket shutdown API- please see Api_shutdown for details                
 * RETURNS: A_OK or A_ERROR 
 *****************************************************************************/
A_INT32 t_shutdown(ENET_CONTEXT_STRUCT_PTR enet_ptr, A_UINT32 handle)
{
	A_VOID *pCxt;
	
	if((pCxt = enet_ptr->MAC_CONTEXT_PTR) == NULL)
	{
	   	return A_ERROR;
	}
	/*Wait for chip to be up*/
    if(A_OK != Api_DriverAccessCheck(pCxt, 1, ACCESS_REQUEST_IOCTL)){
        return A_ERROR;
    }
	   
	return (Api_shutdown(pCxt, handle));
}


/*****************************************************************************/
/*  t_connect - Custom version of socket connect API- please see Api_connect for details                
 * RETURNS: A_OK or A_ERROR 
 *****************************************************************************/
A_INT32 t_connect(ENET_CONTEXT_STRUCT_PTR enet_ptr, A_UINT32 handle, A_VOID* name, A_UINT16 length)
{
	A_VOID *pCxt;
	A_INT32 index;
	SOCKET_CUSTOM_CONTEXT_PTR pcustctxt;
	
	if((pCxt = enet_ptr->MAC_CONTEXT_PTR) == NULL)
	{
	   	return A_ERROR;
	} 
	/*Wait for chip to be up*/
    if(A_OK != Api_DriverAccessCheck(pCxt, 1, ACCESS_REQUEST_IOCTL)){
        return A_ERROR;
    }
    
    index = find_socket_context(handle,TRUE);
    
    if (SOCKET_NOT_FOUND == index)
	{
		last_driver_error = A_SOCKCXT_NOT_FOUND;
		return A_SOCK_INVALID;
	}

    pcustctxt = GET_CUSTOM_SOCKET_CONTEXT(ath_sock_context[index]);
    memcpy(&(pcustctxt->addr.addr4), name, sizeof(SOCKADDR_T));
	  
	return (Api_connect(pCxt, handle, name, length));
}


/*****************************************************************************/
/*  t_bind - Custom version of socket bind API- please see Api_bind for details                
 * RETURNS: A_OK or A_ERROR 
 *****************************************************************************/
A_INT32 t_bind(ENET_CONTEXT_STRUCT_PTR enet_ptr, A_UINT32 handle, A_VOID* name, A_UINT16 length)
{
	A_VOID *pCxt;
	
	if((pCxt = enet_ptr->MAC_CONTEXT_PTR) == NULL)
	{
	   	return A_ERROR;
	}  
	/*Wait for chip to be up*/
    if(A_OK != Api_DriverAccessCheck(pCxt, 1, ACCESS_REQUEST_IOCTL)){
        return A_ERROR;
    }
	 
	return (Api_bind(pCxt, handle, name, length));
}

/*****************************************************************************/
/*  t_listen - Custom version of socket listen API- please see Api_listen for details                
 * RETURNS: A_OK or A_ERROR 
 *****************************************************************************/
A_INT32 t_listen(ENET_CONTEXT_STRUCT_PTR enet_ptr, A_UINT32 handle, A_UINT32 backlog)
{
	A_VOID *pCxt;
	
	if((pCxt = enet_ptr->MAC_CONTEXT_PTR) == NULL)
	{
	   	return A_ERROR;
	}  
	/*Wait for chip to be up*/
    if(A_OK != Api_DriverAccessCheck(pCxt, 1, ACCESS_REQUEST_IOCTL)){
        return A_ERROR;
    }
	 
	return (Api_listen(pCxt, handle, backlog));
}


/*****************************************************************************/
/*  t_accept - Custom version of socket accept API- please see Api_accept for details                
 * RETURNS: A_OK or A_ERROR 
 *****************************************************************************/
A_INT32 t_accept(ENET_CONTEXT_STRUCT_PTR enet_ptr, A_UINT32 handle, A_VOID* name, A_UINT16 length)
{
	A_VOID *pCxt;
	
	if((pCxt = enet_ptr->MAC_CONTEXT_PTR) == NULL)
	{
	   	return A_ERROR;
	}   
	/*Wait for chip to be up*/
    if(A_OK != Api_DriverAccessCheck(pCxt, 1, ACCESS_REQUEST_IOCTL)){
        return A_ERROR;
    }
	
	return (Api_accept(pCxt, handle, name, length));
}


/*****************************************************************************/
/*  t_select - Custom version of socket select API- please see Api_select for details                
 * RETURNS: A_OK or A_ERROR 
 *****************************************************************************/
A_INT32 t_select(ENET_CONTEXT_STRUCT_PTR enet_ptr, A_UINT32 handle, A_UINT32 tv)
{
    A_VOID *pCxt;

    if((pCxt = enet_ptr->MAC_CONTEXT_PTR) == NULL)
    {
   	    return A_ERROR;
    }   
    /*Wait for chip to be up*/
    if(A_OK != Api_DriverAccessCheck(pCxt, 1, ACCESS_REQUEST_RX)){
        return A_ERROR;
    }

    return (Api_select(pCxt, handle, tv));
}


/*****************************************************************************/
/*  t_errno - Custom version of socket errno API- please see Api_errno for details                
 * RETURNS: A_OK or A_ERROR 
 *****************************************************************************/
A_INT32 t_errno(ENET_CONTEXT_STRUCT_PTR enet_ptr, A_UINT32 handle)
{
    A_VOID *pCxt;

    if((pCxt = enet_ptr->MAC_CONTEXT_PTR) == NULL)
    {
   	    return A_ERROR;
    }   
    /*Wait for chip to be up*/
    if(A_OK != Api_DriverAccessCheck(pCxt, 1, ACCESS_REQUEST_IOCTL)){
        return A_ERROR;
    }

    return (Api_errno(pCxt, handle));
}


/*****************************************************************************/
/*  t_setsockopt - Custom version of socket setsockopt API- please see Api_setsockopt for details                
 * RETURNS: A_OK or A_ERROR 
 *****************************************************************************/
A_INT32 t_setsockopt(ENET_CONTEXT_STRUCT_PTR enet_ptr, A_UINT32 handle, A_UINT32 level, A_UINT32 optname, A_UINT8* optval, A_UINT32 optlen)
{
    A_VOID *pCxt;

    if((pCxt = enet_ptr->MAC_CONTEXT_PTR) == NULL)
    {
   	    return A_ERROR;
    }  
    /*Wait for chip to be up*/
    if(A_OK != Api_DriverAccessCheck(pCxt, 1, ACCESS_REQUEST_IOCTL)){
        return A_ERROR;
    }
     
    return (Api_setsockopt(pCxt, handle, level, optname, optval, optlen));
}

/*****************************************************************************/
/*  t_getsockopt - Custom version of socket getsockopt API- please see                 
 *                 Api_getsockopt for details. 
 * RETURNS: A_OK or A_ERROR 
 *****************************************************************************/
A_INT32 t_getsockopt(ENET_CONTEXT_STRUCT_PTR enet_ptr, A_UINT32 handle, A_UINT32 level, A_UINT32 optname, A_UINT8* optval, A_UINT32 optlen)
{
    A_VOID *pCxt;

    if((pCxt = enet_ptr->MAC_CONTEXT_PTR) == NULL)
    {
   	    return A_ERROR;
    } 
    /*Wait for chip to be up*/
    if(A_OK != Api_DriverAccessCheck(pCxt, 1, ACCESS_REQUEST_IOCTL)){
        return A_ERROR;
    }
      
    return (Api_getsockopt(pCxt, handle, level, optname, optval, optlen));
}


/*****************************************************************************/
/*  t_ipconfig - Custom version of ipconfig API- please see                 
 *                 Api_ipconfig for details. 
 * RETURNS: A_OK or A_ERROR 
 *****************************************************************************/
A_INT32 t_ipconfig(ENET_CONTEXT_STRUCT_PTR enet_ptr, A_UINT32 mode,A_UINT32* ipv4_addr, A_UINT32* subnetMask, A_UINT32* gateway4)
{
    A_VOID *pCxt;

    if((pCxt = enet_ptr->MAC_CONTEXT_PTR) == NULL)
    {
   	    return A_ERROR;
    } 
    /*Wait for chip to be up*/
    if(A_OK != Api_DriverAccessCheck(pCxt, 1, ACCESS_REQUEST_IOCTL)){
        return A_ERROR;
    }
      
    return (Api_ipconfig(pCxt, mode, ipv4_addr, subnetMask, gateway4));
}

/*****************************************************************************/
/*  t_ip6config - Custom version of ipconfig API- please see                 
 *                 Api_ip6config for details. 
 * RETURNS: A_OK or A_ERROR 
 *****************************************************************************/
A_INT32 t_ip6config(ENET_CONTEXT_STRUCT_PTR enet_ptr, A_UINT32 mode,IP6_ADDR_T *v6Global,IP6_ADDR_T *v6Local,IP6_ADDR_T *v6DefGw,IP6_ADDR_T *v6GlobalExtd,A_INT32 *LinkPrefix,
		    A_INT32 *GlbPrefix, A_INT32 *DefGwPrefix, A_INT32 *GlbPrefixExtd)
{
    A_VOID *pCxt;

    if((pCxt = enet_ptr->MAC_CONTEXT_PTR) == NULL)
    {
   	    return A_ERROR;
    }
    /*Wait for chip to be up*/
    if(A_OK != Api_DriverAccessCheck(pCxt, 1, ACCESS_REQUEST_IOCTL)){
        return A_ERROR;
    }
       
    return (Api_ip6config(pCxt, mode,v6Global,v6Local,v6DefGw,v6GlobalExtd,LinkPrefix,GlbPrefix,DefGwPrefix,GlbPrefixExtd));
}

/*****************************************************************************/
/*  t_ipconfig_dhcp_pool -  Function to create DHCP Pool                
 * RETURNS: A_OK or A_ERROR 
 *****************************************************************************/
A_INT32 t_ipconfig_dhcp_pool(ENET_CONTEXT_STRUCT_PTR enet_ptr,A_UINT32* start_ipv4_addr, A_UINT32* end_ipv4_addr,A_INT32 leasetime)
{
    A_VOID *pCxt;

    if((pCxt = enet_ptr->MAC_CONTEXT_PTR) == NULL)
    {
   	    return A_ERROR;
    } 
    /*Wait for chip to be up*/
    if(A_OK != Api_DriverAccessCheck(pCxt, 1, ACCESS_REQUEST_IOCTL)){
        return A_ERROR;
    }
      
    return (Api_ipconfig_dhcp_pool(pCxt,start_ipv4_addr, end_ipv4_addr,leasetime));
}


A_INT32 t_ip6config_router_prefix(ENET_CONTEXT_STRUCT_PTR enet_ptr,A_UINT8 *v6addr,A_INT32 prefixlen, A_INT32 prefix_lifetime,
		                     A_INT32 valid_lifetime)
{

    A_VOID *pCxt;

    if((pCxt = enet_ptr->MAC_CONTEXT_PTR) == NULL)
    {
   	    return A_ERROR;
    } 
    /*Wait for chip to be up*/
    if(A_OK != Api_DriverAccessCheck(pCxt, 1, ACCESS_REQUEST_IOCTL)){
        return A_ERROR;
    }
    return (Api_ip6config_router_prefix(pCxt,v6addr,prefixlen,prefix_lifetime,valid_lifetime));
}	


A_INT32 custom_ipconfig_set_tcp_exponential_backoff_retry(ENET_CONTEXT_STRUCT_PTR enet_ptr,A_INT32 retry)
{
    A_VOID *pCxt;

    if((pCxt = enet_ptr->MAC_CONTEXT_PTR) == NULL)
    {
   	    return A_ERROR;
    } 
    /*Wait for chip to be up*/
    if(A_OK != Api_DriverAccessCheck(pCxt, 1, ACCESS_REQUEST_IOCTL)){
        return A_ERROR;
    }
    return (Api_ipconfig_set_tcp_exponential_backoff_retry(pCxt,retry));
}

A_INT32 custom_ipconfig_set_ip6_status(ENET_CONTEXT_STRUCT_PTR enet_ptr,A_UINT16 status)
{
    A_VOID *pCxt;

    if((pCxt = enet_ptr->MAC_CONTEXT_PTR) == NULL)
    {
   	    return A_ERROR;
    } 
    /*Wait for chip to be up*/
    if(A_OK != Api_DriverAccessCheck(pCxt, 1, ACCESS_REQUEST_IOCTL)){
        return A_ERROR;
    }
    return (Api_ipconfig_set_ip6_status(pCxt,status));
}	

A_INT32 custom_ipconfig_dhcp_release(ENET_CONTEXT_STRUCT_PTR enet_ptr)
{

    A_VOID *pCxt;

    if((pCxt = enet_ptr->MAC_CONTEXT_PTR) == NULL)
    {
   	    return A_ERROR;
    } 
    /*Wait for chip to be up*/
    if(A_OK != Api_DriverAccessCheck(pCxt, 1, ACCESS_REQUEST_IOCTL)){
        return A_ERROR;
    }
    return (Api_ipconfig_dhcp_release(pCxt));

}

A_INT32 custom_ipconfig_set_tcp_rx_buffer(ENET_CONTEXT_STRUCT_PTR enet_ptr,A_INT32 rxbuf)
{
    A_VOID *pCxt;
	
    if((pCxt = enet_ptr->MAC_CONTEXT_PTR) == NULL)
    {
   	    return A_ERROR;
    } 
    /*Wait for chip to be up*/
    if(A_OK != Api_DriverAccessCheck(pCxt, 1, ACCESS_REQUEST_IOCTL)){
        return A_ERROR;
    }
    return (Api_ipconfig_set_tcp_rx_buffer(pCxt,rxbuf));
}	
/*****************************************************************************/
/*  t_ping - Custom version of ping API- please see                 
 *                 Api_ping for details. 
 * RETURNS: A_OK or A_ERROR 
 *****************************************************************************/
A_INT32 t_ping(ENET_CONTEXT_STRUCT_PTR enet_ptr, A_UINT32 ipv4_addr, A_UINT32 size)
{
    A_VOID *pCxt;

    if((pCxt = enet_ptr->MAC_CONTEXT_PTR) == NULL)
    {
   	    return A_ERROR;
    } 
    /*Wait for chip to be up*/
    if(A_OK != Api_DriverAccessCheck(pCxt, 1, ACCESS_REQUEST_IOCTL)){
        return A_ERROR;
    }
      
    return (Api_ping(pCxt, ipv4_addr, size));
}

/*****************************************************************************/
/*  t_ping6 - Custom version of ping API- please see                 
 *                 Api_ping6 for details. 
 * RETURNS: A_OK or A_ERROR 
 *****************************************************************************/
A_INT32 t_ping6(ENET_CONTEXT_STRUCT_PTR enet_ptr, A_UINT8 *ip6addr, A_UINT32 size)
{
	A_VOID *pCxt;
	
	if((pCxt = enet_ptr->MAC_CONTEXT_PTR) == NULL)
	{
	   	return A_ERROR;
	} 
	
	/*Wait for chip to be up*/
    if(A_OK != Api_DriverAccessCheck(pCxt, 1, ACCESS_REQUEST_IOCTL)){
        return A_ERROR;
    }
	
	return (Api_ping6(pCxt, ip6addr, size));
}

/*****************************************************************************/
/*  t_send - Custom version of socket send API. Used for stream based sockets.                 
 * 			 Sends provided buffer to target.
 * RETURNS: Number of bytes sent to target 
 *****************************************************************************/
A_INT32 t_send(ENET_CONTEXT_STRUCT_PTR enet_ptr, A_UINT32 handle, A_UINT8* buffer, A_UINT32 length, A_UINT32 flags)
{
    A_INT32 index = find_socket_context(handle,TRUE);
    if (SOCKET_NOT_FOUND == index)
	{
		last_driver_error = A_SOCKCXT_NOT_FOUND;
		return A_SOCK_INVALID;
	}
    if (ath_sock_context[index]->type == SOCK_DGRAM_TYPE)
    {
        SOCKET_CUSTOM_CONTEXT_PTR pcustctxt = GET_CUSTOM_SOCKET_CONTEXT(ath_sock_context[index]);
        return (t_sendto(enet_ptr, handle, buffer, length, flags, 
                &(pcustctxt->addr.addr4), 
                sizeof(pcustctxt->addr.addr4)));
    }
	return (t_sendto(enet_ptr, handle, buffer, length, flags, NULL, 0));
}



/*****************************************************************************/
/*  t_sendto - Custom version of socket send API. Used for datagram sockets.                 
 * 		Sends provided buffer to target. Creates space for headroom at
 *              the begining of the packet. The headroom is used by the stack to 
 *              fill in the TCP-IP header.   
 * RETURNS: Number of bytes sent to target 
 *****************************************************************************/
A_INT32 t_sendto(ENET_CONTEXT_STRUCT_PTR enet_ptr, A_UINT32 handle, A_UINT8* buffer, A_UINT32 length, A_UINT32 flags, A_VOID* name, A_UINT32 socklength)
{
	A_UINT8 custom_hdr[TCP6_HEADROOM];
	A_INT32 index = 0;
	A_UINT16 custom_hdr_length = 0;
	DRV_BUFFER_PTR             db_ptr;
	A_UINT16 hdr_length = sizeof(ATH_MAC_HDR);
	A_UINT8* hdr = NULL, *bufPtr=buffer;
	A_VOID *pCxt;
#if NON_BLOCKING_TX	
	SOCKET_CUSTOM_CONTEXT_PTR pcustctxt;
#endif	
	A_INT32 result=0;
	A_STATUS status = A_OK;
	
	if((pCxt = enet_ptr->MAC_CONTEXT_PTR) == NULL)
	{
	   	return A_ERROR;
	}   
	
	/*Find socket context*/
	if((index = find_socket_context(handle,TRUE)) == SOCKET_NOT_FOUND)
	{
		last_driver_error = A_SOCKCXT_NOT_FOUND;
		return A_SOCK_INVALID;
	}

	A_MEMZERO(custom_hdr, TCP6_HEADROOM);

	if(ath_sock_context[index]->domain == ATH_AF_INET)
	{
		if(ath_sock_context[index]->type == SOCK_STREAM_TYPE)
			hdr_length = TCP_HEADROOM;
		else
			hdr_length = UDP_HEADROOM;
	}
	else
	{

		if(ath_sock_context[index]->type == SOCK_STREAM_TYPE)
			hdr_length = TCP6_HEADROOM_WITH_NO_OPTION;
		else
			hdr_length = UDP6_HEADROOM;
	}

	
	/*Calculate fragmentation threshold. We cannot send packets greater that HTC credit size
	  over HTC, Bigger packets need to be fragmented. Thresholds are different for IP v4 vs v6*/
	if(ath_sock_context[index]->domain == ATH_AF_INET6)
	{

		custom_hdr_length = sizeof(SOCK_SEND6_T);
		if(name != NULL) {
			((SOCK_SEND6_T*)custom_hdr)->name6.sin6_port = A_CPU2LE16(((SOCKADDR_6_T*)name)->sin6_port);
			((SOCK_SEND6_T*)custom_hdr)->name6.sin6_family = A_CPU2LE16(((SOCKADDR_6_T*)name)->sin6_family);
			((SOCK_SEND6_T*)custom_hdr)->name6.sin6_flowinfo = A_CPU2LE32(((SOCKADDR_6_T*)name)->sin6_flowinfo);	
			A_MEMCPY((A_UINT8*)&(((SOCK_SEND6_T*)custom_hdr)->name6.sin6_addr),(A_UINT8*)&((SOCKADDR_6_T*)name)->sin6_addr,sizeof(IP6_ADDR_T));	
			((SOCK_SEND6_T*)custom_hdr)->socklength = A_CPU2LE32(socklength);
		}
		else
		{
			memset((A_UINT8*)(&((SOCK_SEND6_T*)custom_hdr)->name6),0,sizeof(name));
		}
	}
	else
	{
		custom_hdr_length = sizeof(SOCK_SEND_T);
		if(name != NULL) {
//  		    corona_print("IP: sending to 0x%.8X : %d\r\n", ((SOCKADDR_T*)name)->sin_addr, ((SOCKADDR_T*)name)->sin_port);
			((SOCK_SEND_T*)custom_hdr)->name.sin_port = A_CPU2LE16(((SOCKADDR_T*)name)->sin_port);
			((SOCK_SEND_T*)custom_hdr)->name.sin_family = A_CPU2LE16(((SOCKADDR_T*)name)->sin_family);
			((SOCK_SEND_T*)custom_hdr)->name.sin_addr = A_CPU2LE32(((SOCKADDR_T*)name)->sin_addr);	
			((SOCK_SEND_T*)custom_hdr)->socklength = A_CPU2LE32(socklength);
		}
		else
		{			
			memset((A_UINT8*)(&((SOCK_SEND_T*)custom_hdr)->name),0,sizeof(name));
		}		
	}
	
	/*Populate common fields of custom header, these are the same for IP v4/v6*/
	((SOCK_SEND_T*)custom_hdr)->handle = A_CPU2LE32(handle);
	((SOCK_SEND_T*)custom_hdr)->flags = A_CPU2LE32(flags);
	

        db_ptr = &((TX_PACKET_PTR)(bufPtr-TX_PKT_OVERHEAD))->db;
        hdr = ((TX_PACKET_PTR)(bufPtr - TX_PKT_OVERHEAD))->hdr;
  
        A_MEMZERO(hdr, hdr_length);		

#if NON_BLOCKING_TX
        pcustctxt = GET_CUSTOM_SOCKET_CONTEXT(ath_sock_context[index]); 
	/*In non blocking TX case, Enqueue the packet so that it can be freed later when 
          TX over SPI is successful. Application should not free this buffer*/

        A_MUTEX_LOCK(&(pcustctxt->nb_tx_mutex));
	A_NETBUF_ENQUEUE(&(pcustctxt->non_block_queue), bufPtr);   
        A_MUTEX_UNLOCK(&(pcustctxt->nb_tx_mutex));        
#endif	

	result = length;
		
	if((length == 0) || (length > IPV4_FRAGMENTATION_THRESHOLD)){
          /*Host fragmentation is not allowed, application must send payload
           below IPV4_FRAGMENTATION_THRESHOLD size*/
          A_ASSERT(0); 
        }

        ((SOCK_SEND_T*)custom_hdr)->length = A_CPU2LE32(length);
        
        A_MEMCPY(hdr,custom_hdr, custom_hdr_length);	

        db_ptr->context = (pointer)ath_sock_context[index];
        db_ptr->bufFragment[0].payload = bufPtr;
        db_ptr->bufFragment[0].length = length;
       
        status = custom_send_tcpip(pCxt, db_ptr,length,1,hdr,hdr_length);
        if(status != A_OK)
        {
            printf("WM: fail custom_send_tcpip\n");fflush(stdout);
            goto END_TX;
        }

#if !NON_BLOCKING_TX	
        /*Wait till packet is sent to target*/		
        if(CUSTOM_BLOCK(pCxt,ath_sock_context[index], TRANSMIT_BLOCK_TIMEOUT, TX_DIRECTION) != A_OK)
        {
            printf("WM: TIMEOUT ON CUSTOM_BLOCK\n");fflush(stdout);          
           // result = A_ERROR;
           // goto END_TX;
           return A_WHISTLE_REBOOT; // wifi is not responding - reboot at the app level           
        }                
#endif		

END_TX: 

    if(status != A_OK)
    {   
        result = A_ERROR;
    }
	
	return result;
}





#if ZERO_COPY
A_INT32 t_recvfrom(ENET_CONTEXT_STRUCT_PTR enet_ptr, A_UINT32 handle, void** buffer, A_UINT32 length, A_UINT32 flags, A_VOID* name, A_UINT32* socklength)
#else
A_INT32 t_recvfrom(ENET_CONTEXT_STRUCT_PTR enet_ptr, A_UINT32 handle, void* buffer, A_UINT32 length, A_UINT32 flags, A_VOID* name, A_UINT32* socklength)
#endif
{
	A_VOID *pCxt;
	
	if((pCxt = enet_ptr->MAC_CONTEXT_PTR) == NULL)
	{
	   	return A_ERROR;
	}   
	/*Wait for chip to be up*/
    if(A_OK != Api_DriverAccessCheck(pCxt, 1, ACCESS_REQUEST_RX)){
        return A_ERROR;
    }

	return (Api_recvfrom(pCxt, handle, buffer, length, flags, name, socklength));
}



#if ZERO_COPY
A_INT32 t_recv(ENET_CONTEXT_STRUCT_PTR enet_ptr, A_UINT32 handle, void** buffer, A_UINT32 length, A_UINT32 flags)
#else
A_INT32 t_recv(ENET_CONTEXT_STRUCT_PTR enet_ptr, A_UINT32 handle, void* buffer, A_UINT32 length, A_UINT32 flags)
#endif
{
	return (t_recvfrom(enet_ptr, handle, buffer, length, flags, NULL, NULL));
}










#if ZERO_COPY

/*****************************************************************************/
/*  zero_copy_free - ZERO_COPY free API.It will find and remove rx buffer from
 *                  a common queue.   
 * RETURNS: Number of bytes received or A_ERROR in case of failure
 *****************************************************************************/
A_VOID zero_copy_free(A_VOID* buffer)
{
	A_NETBUF* a_netbuf_ptr = NULL;
	
	/*find buffer in zero copy queue*/
	a_netbuf_ptr = A_NETBUF_DEQUEUE_ADV(&zero_copy_free_queue, buffer);
	
	if(a_netbuf_ptr != NULL)
		A_NETBUF_FREE(a_netbuf_ptr);
	else
		printf("ERR netbuf\n");
}


/*****************************************************************************/
/*  Api_recvfrom - ZERO_COPY version of receive API.It will check socket's
 *				receive quese for pending packets. If a packet is available,
 *				it will be passed to the application without a memcopy. The 
 *				application must call a zero_copy_free API to free this buffer.
 * RETURNS: Number of bytes received or A_ERROR in case of failure
 *****************************************************************************/
A_INT32 Api_recvfrom(A_VOID *pCxt, A_UINT32 handle, void** buffer, A_UINT32 length, A_UINT32 flags, A_VOID* name, A_UINT32* socklength)
{
	A_INT32 index;
	A_UINT32 len = 0, hdrlen = 0;
	A_NETBUF* a_netbuf_ptr = NULL;
	A_UINT8* data = NULL;
	SOCKET_CUSTOM_CONTEXT_PTR pcustctxt;
	
	/*Find context*/
	if((index = find_socket_context(handle,TRUE)) == SOCKET_NOT_FOUND)
	{
		return A_SOCK_INVALID;
	}
	pcustctxt = GET_CUSTOM_SOCKET_CONTEXT(ath_sock_context[index]);
	/*Check if a packet is available*/
	while((a_netbuf_ptr = A_NETBUF_DEQUEUE(&(pcustctxt->rxqueue))) == NULL)
	{
#if NON_BLOCKING_RX
            /*No packet is available, return -1*/
            return A_ERROR;	
#else          
            /*No packet available, block*/
            if(CUSTOM_BLOCK(pCxt, ath_sock_context[index], 0, RX_DIRECTION) != A_OK)
            {
                /*Check if Peer closed socket while we were waiting*/
		 		if(ath_sock_context[index]->handle == 0)
		    		return A_SOCK_INVALID;
		 		
	               // return A_ERROR;
		 		   return A_WHISTLE_REBOOT; // wifi is not responding - reboot at the app level	 				
            }
#endif     
	}
	data = (A_UINT8*)A_NETBUF_DATA(a_netbuf_ptr);
	if(ath_sock_context[index]->domain == ATH_AF_INET)
	{
		hdrlen = sizeof(SOCK_RECV_T); 
		/*If we are receiving data for a UDP socket, extract sockaddr info*/
		if(ath_sock_context[index]->type == SOCK_DGRAM_TYPE)
		{
			A_MEMCPY(name, &((SOCK_RECV_T*)data)->name, sizeof(SOCKADDR_T));
			((SOCKADDR_T*)name)->sin_port = A_CPU2LE16(((SOCKADDR_T*)name)->sin_port);
			((SOCKADDR_T*)name)->sin_family = A_CPU2LE16(((SOCKADDR_T*)name)->sin_family);
			((SOCKADDR_T*)name)->sin_addr = A_CPU2LE32(((SOCKADDR_T*)name)->sin_addr);
			*socklength = sizeof(SOCKADDR_T);
		}
	}
	else if(ath_sock_context[index]->domain == ATH_AF_INET6)
	{
		hdrlen = sizeof(SOCK_RECV6_T);
		/*If we are receiving data for a UDP socket, extract sockaddr info*/
		if(ath_sock_context[index]->type == SOCK_DGRAM_TYPE)
		{
			A_MEMCPY(name, &((SOCK_RECV6_T*)data)->name6, sizeof(SOCKADDR_6_T));
			*socklength = sizeof(SOCKADDR_6_T);
		}
	}
	else
	{
		A_ASSERT(0);
	}
	
	/*Remove custom header from payload*/
	A_NETBUF_PULL(a_netbuf_ptr, hdrlen);

	len = A_NETBUF_LEN(a_netbuf_ptr);
		
	if(len > length)
	{
		len = length;  //Discard excess bytes
	}
		
	
	*buffer = (A_UINT8*)A_NETBUF_DATA(a_netbuf_ptr);
	A_NETBUF_ENQUEUE(&zero_copy_free_queue, a_netbuf_ptr);
	
	return len;
}
#else



/*****************************************************************************/
/*  Api_recvfrom - Non ZERO_COPY version of receive API.It will check socket's
 *				receive quese for pending packets. If a packet is available,
 *				it will be copied into user provided buffer.
 * RETURNS: Number of bytes received or A_ERROR in case of failure
 *****************************************************************************/
A_INT32 Api_recvfrom(A_VOID *pCxt, A_UINT32 handle, void* buffer, A_UINT32 length, A_UINT32 flags, A_VOID* name, A_UINT32* socklength)
{
	A_INT32 index;
	A_UINT32 len = 0, hdrlen=0;
	A_NETBUF* a_netbuf_ptr = NULL;
	A_UINT8* data = NULL;
	SOCKET_CUSTOM_CONTEXT_PTR pcustctxt;
	
	/*Find context*/
	if((index = find_socket_context(handle,TRUE)) == SOCKET_NOT_FOUND)
	{
		return A_SOCK_INVALID;
	}
	pcustctxt = GET_CUSTOM_SOCKET_CONTEXT(ath_sock_context[index]);
	
	while((a_netbuf_ptr = A_NETBUF_DEQUEUE(&(pcustctxt->rxqueue))) == NULL)
	{
#if NON_BLOCKING_RX
		/*No packet is available, return -1*/
		return A_ERROR;	
#else          
		/*No packet available, block*/
		if(CUSTOM_BLOCK(pCxt, ath_sock_context[index], 0, RX_DIRECTION) != A_OK)
		{
			/*Check if Peer closed socket while we were waiting*/
		 	if(ath_sock_context[index]->handle == 0)
		    	return A_SOCK_INVALID;

		 	// return A_ERROR;
		 	return A_WHISTLE_REBOOT; // wifi is not responding - reboot at the app level	 	
		}
#endif     
	}
	
	/*Extract custom header*/
	data = (A_UINT8*)A_NETBUF_DATA(a_netbuf_ptr);
	if(ath_sock_context[index]->domain == ATH_AF_INET)
	{
		hdrlen = sizeof(SOCK_RECV_T); 
		/*If we are receiving data for a UDP socket, extract sockaddr info*/
		if(ath_sock_context[index]->type == SOCK_DGRAM_TYPE)
		{
			A_MEMCPY(name, &((SOCK_RECV_T*)data)->name, sizeof(SOCKADDR_T));
			((SOCKADDR_T*)name)->sin_port = A_CPU2LE16(((SOCKADDR_T*)name)->sin_port);
			((SOCKADDR_T*)name)->sin_family = A_CPU2LE16(((SOCKADDR_T*)name)->sin_family);
			((SOCKADDR_T*)name)->sin_addr = A_CPU2LE32(((SOCKADDR_T*)name)->sin_addr);
			*socklength = sizeof(SOCKADDR_T);
		}
	}
	else if(ath_sock_context[index]->domain == ATH_AF_INET6)
	{
		hdrlen = sizeof(SOCK_RECV6_T);
		/*If we are receiving data for a UDP socket, extract sockaddr info*/
		if(ath_sock_context[index]->type == SOCK_DGRAM_TYPE)
		{
			A_MEMCPY(name, &((SOCK_RECV6_T*)data)->name6, sizeof(SOCKADDR_6_T));
			*socklength = sizeof(SOCKADDR_6_T);
		}
	}
	else
	{
		A_ASSERT(0);
	}
	
	/*Remove custom header from payload*/
	A_NETBUF_PULL(a_netbuf_ptr, hdrlen);
	

	len = A_NETBUF_LEN(a_netbuf_ptr);
		
	if(len > length)
		len = length;  //Discard excess bytes

	A_MEMCPY((A_UINT8*)buffer,(A_UINT8*)A_NETBUF_DATA(a_netbuf_ptr), len);
	A_NETBUF_FREE(a_netbuf_ptr);
	
	return len;
}
#endif






/*****************************************************************************/
/*  txpkt_free - function to free driver buffers that are allocated during send 
 *          operations. The function is called from a_netbuf_free, after tx
 *          has successfully completed.
 *          If NON_BLOCKING_TX is defined, the user buffer is freed here
 *          as well. 
 *    Note 1- called from driver thread.
 *    Note 2- should not be called from application
 * RETURNS: none
 *****************************************************************************/
A_VOID txpkt_free(A_VOID* buffPtr)
{
    A_NETBUF_PTR a_netbuf_ptr = (A_NETBUF*)buffPtr;
    DRV_BUFFER_PTR db_ptr = a_netbuf_ptr->native_orig;
    SOCKET_CUSTOM_CONTEXT_PTR pcustctxt = GET_CUSTOM_SOCKET_CONTEXT((ATH_SOCKET_CONTEXT_PTR)db_ptr->context);		 

#if NON_BLOCKING_TX
    A_VOID* bufPtr = NULL;	
    /*In case of non blocking TX, driver should free the payload buffer*/	
    A_MUTEX_LOCK(&(pcustctxt->nb_tx_mutex));        
    bufPtr = A_NETBUF_DEQUEUE(&(pcustctxt->non_block_queue));
    A_MUTEX_UNLOCK(&(pcustctxt->nb_tx_mutex));                
    
    if(bufPtr != NULL) {    
      A_FREE(((A_UINT8*)bufPtr - TX_PKT_OVERHEAD),MALLOC_ID_CONTEXT);  
      bufPtr = NULL;
    }
    
    /*We are done with netbuf, free Netbuf structure here*/
    A_FREE(a_netbuf_ptr->head, MALLOC_ID_NETBUFF);
    A_FREE(a_netbuf_ptr, MALLOC_ID_NETBUFF_OBJ);    
#else
    /*Tail, data pointers within netbuf may have moved during previous TX, 
     reinit the netbuf so it can be used again*/
    a_netbuf_ptr = a_netbuf_reinit((A_VOID*)a_netbuf_ptr, TCP6_HEADROOM);
    
    /*Unblock the application thread*/
    CUSTOM_UNBLOCK(db_ptr->context, TX_DIRECTION);
#endif

}



/*****************************************************************************/
/*  custom_alloc - API for application to allocate a TX buffer. Here we ensure that 
              enough resources are available for a packet TX. All allocations 
             for a TX operation are made here. This includes Headroom, DB and netbuf.
             If any allocation fails, this API returns NULL, and the host must 
             wait for some time to allow memory to be freed. 
      Note 1-  Allocation may fail if too many packets are queued up for Transmission,
               e.g. in the the non-blocking TX case.
      Note 2- This API should ONLY be used for TX packets.

 * RETURNS: pointer to payload buffer for success and NULL for failure 
 *****************************************************************************/
A_VOID* custom_alloc(A_UINT32 size)
{
    A_UINT32 total_size = 0;
    A_UINT8* buffer, *payload;
    A_UINT32 hdr_length = 0;     
    
    /*Allocate TX buffer that will include- payload + headroom + Driver buffer + pointer to netbuf*/
    total_size = size + TX_PKT_OVERHEAD;
    
    /*Round off to 4 byte boundary*/
    if((total_size % 4) != 0){
        total_size += 4 - total_size%4;
    }
    if((buffer = A_MALLOC(total_size, MALLOC_ID_CONTEXT)) == NULL){
      return NULL;
    }
    
    /*Allocate netbuf with max possible headroom here so that there is no need to do it during TX*/
    if((((TX_PACKET_PTR)buffer)->a_netbuf_ptr = A_NETBUF_ALLOC(TCP6_HEADROOM)) == NULL){ 
      A_FREE(buffer, MALLOC_ID_CONTEXT);
      return NULL;
    }
    /*Obtain pointer to start of payload*/
    payload = buffer + TX_PKT_OVERHEAD;
    return payload;
}


/*****************************************************************************/
/*  custom_free - API for application to free TX buffer. It should ONLY be called
 *              by the app if Blocking TX mode is enabled. It will free all allocations
 *              made during the custom_alloc 
 * RETURNS: none 
 *****************************************************************************/
A_VOID custom_free(A_VOID* buf)
{
#if (!NON_BLOCKING_TX)	  
    A_UINT8* buffer;
    A_NETBUF_PTR a_netbuf_ptr;
    
    /*Move the the begining of TX buffer*/
    buffer = (A_UINT8*)buf - TX_PKT_OVERHEAD; 
    
    /*Get pointer to netbuf from TX buffer*/
    a_netbuf_ptr = ((TX_PACKET_PTR)buffer)->a_netbuf_ptr;
    
    if(a_netbuf_ptr)
    {
        /*We are done with netbuf, free Netbuf structure here*/
        A_FREE(a_netbuf_ptr->head, MALLOC_ID_NETBUFF);
	A_FREE(a_netbuf_ptr, MALLOC_ID_NETBUFF_OBJ);  
    }
    A_FREE(buffer, MALLOC_ID_CONTEXT);
#endif    
}



/*****************************************************************************/
/*  get_total_pkts_buffered - Returns number of packets buffered across all
 *          socket queues.           
 * RETURNS: number of packets 
 *****************************************************************************/
A_UINT32 get_total_pkts_buffered()
{
	A_UINT32 index;
	A_UINT32 totalPkts = 0;
	
	SOCKET_CUSTOM_CONTEXT_PTR pcustctxt;
	
	for(index = 0; index < MAX_SOCKETS_SUPPORTED; index++)
	{
		pcustctxt = GET_CUSTOM_SOCKET_CONTEXT(ath_sock_context[index]);
		totalPkts += A_NETBUF_QUEUE_SIZE(&(pcustctxt->rxqueue));
	}
	return totalPkts;
}



/*****************************************************************************/
/*  custom_queue_empty - Checkes whether a socket queue is empty
 * RETURNS: 1 - empty or 0 - not empty 
 *****************************************************************************/
A_UINT32 custom_queue_empty(A_UINT32 index)
{
	SOCKET_CUSTOM_CONTEXT_PTR pcustctxt;
	pcustctxt = GET_CUSTOM_SOCKET_CONTEXT(ath_sock_context[index]);
		
	return A_NETBUF_QUEUE_EMPTY(&(pcustctxt->rxqueue));
}

ATH_SOCKET_CONTEXT* get_ath_sock_context(A_UINT32 handle)
{
    A_INT32 index = find_socket_context(handle,TRUE);
    if (SOCKET_NOT_FOUND == index)
    {
        return NULL;
    }
    return ath_sock_context[index];
}
