//------------------------------------------------------------------------------
// <copyright file="atheros_stack_offload.h" company="Atheros">
//    Copyright (c) 2004-2007 Atheros Corporation.  All rights reserved.
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
#ifndef __ATHEROS_STACK_OFFLOAD_H__
#define __ATHEROS_STACK_OFFLOAD_H__
#include "a_config.h"
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

/**********************Macros and config parameters**********/
#define MAX_SOCKETS_SUPPORTED      (8) // TODO - Atheros says this affects memory size
#define SOCKET_NOT_FOUND           (-1)
#define EMPTY_SOCKET_CONTEXT       (0)
#define SOCKET_HANDLE_PLACEHOLDER  (0xFFFFFFFF)
#define ATH_AF_INET                (2)
#define ATH_PF_INET                (ATH_AF_INET)
#define ATH_AF_INET6               (3)
#define ATH_PF_INET6               (ATH_AF_INET6)
#define IPV4_HEADER_LENGTH		   (20)
#define IPV6_HEADER_LENGTH		   (40)
#define TCP_HEADER_LENGTH          (20)
#define UDP_HEADER_LENGTH          (8)
#define IPV6_FRAGMENTATION_THRESHOLD   (1280)
#define IPV4_FRAGMENTATION_THRESHOLD   (AR4100_BUFFER_SIZE -TCP6_HEADROOM)    
#define MAX_ETHERNET_FRAME_SIZE    (1500)

#define GLOBAL_SOCK_INDEX          (MAX_SOCKETS_SUPPORTED)        //Index used for global commands e.g. ipconfig, ping


/*The following macro enables the zero copy feature. In memory-constrained systems,
  the application will provide a pointer instead of an allocated buffer. The driver 
  will return the pointer to received packet instead of copying the packet over.
  The application must call zero_copy_free() API after it is done with the buffer
  and pass the pointer to buffer.*/
#define ZERO_COPY                     0

/* NON_BLOCKING_TX- Macro used to control SEND behavior. 
   SECTION 1- Macro is disabled.
    If this macro is disabled, application thread will block until a packet is 
    sent over SPI. This is not desirable in a single buffer scenario as it may 
    lead to deadlocks. 
   SECTION 2- Macro is enabled.
   If this macro is enabled, the application thread will return after submitting 
   packet to the driver thread. Under this setting, the application MUST NOT 
   TRY TO REUSE OR FREE THIS BUFFER. This buffer is now owned by the driver. 
   The application should call custom_alloc again to get a new buffer. */
#define NON_BLOCKING_TX            0 


/* There may be scenarios where application does not wish to block on a receive operation.
   This macro will enable non blocking receive behavior. Note that this change is only 
   limited to the host and does not affect target behavior.*/ 
#define NON_BLOCKING_RX            0


/* If the host has only 1 RX buffer, receive operation can never be blocking, as
   this may lead to deadlocks.*/
#if WLAN_CONFIG_NUM_PRE_ALLOC_RX_BUFFERS < 2

#if NON_BLOCKING_RX == 0
#error "Blocking receive not permitted with single RX buffer, please enable NON_BLOCKING_RX"
#endif

#if NON_BLOCKING_TX == 0
#error "Blocking tranmit not permitted with single RX buffer, please enable NON_BLOCKING_TX"
#endif

#endif

/* BSD Sockets errors */
#define ENOBUFS         1
#define ETIMEDOUT       2
#define EISCONN         3
#define EOPNOTSUPP      4
#define ECONNABORTED    5
#define EWOULDBLOCK     6
#define ECONNREFUSED    7
#define ECONNRESET      8
#define ENOTCONN        9
#define EALREADY        10
#define EINVAL          11
#define EMSGSIZE        12
#define EPIPE           13
#define EDESTADDRREQ    14
#define ESHUTDOWN       15
#define ENOPROTOOPT     16
#define EHAVEOOB        17
#define ENOMEM          18
#define EADDRNOTAVAIL   19
#define EADDRINUSE      20
#define EAFNOSUPPORT    21


/* Generic/TCP socket options          */

#define SO_KEEPALIVE       0x0008    /* keep connections alive    */
#define SO_MAXMSG          0x1010    /* get TCP_MSS (max segment size) */
#define SO_LINGER          0x0080    /* linger on close if data present    */

/* TCP User-settable options (used with setsockopt).   */
#define TCP_MAXSEG         0x2003    /* set maximum segment size    */

/*Multicast options*/
#define  IP_MULTICAST_IF   9  /* u_char; set/get IP multicast i/f  */
#define  IP_MULTICAST_TTL  10 /* u_char; set/get IP multicast ttl */
#define  IP_MULTICAST_LOOP 11 /* u_char; set/get IP multicast loopback */
#define  IP_ADD_MEMBERSHIP 12 /* ip_mreq; add an IP group membership */
#define  IP_DROP_MEMBERSHIP 13 /* ip_mreq; drop an IP group membership */

/*IPv6*/
#define IPV6_MULTICAST_IF   80 /* unisgned int; set IF for outgoing MC pkts */
#define IPV6_MULTICAST_HOPS 81 /* int; set MC hopcount */
#define IPV6_MULTICAST_LOOP 82 /* unisgned int; set to 1 to loop back */
#define IPV6_JOIN_GROUP     83 /* ipv6_mreq; join MC group */
#define IPV6_LEAVE_GROUP    84 /* ipv6_mreq; leave MC group */
#define IPV6_V6ONLY         85 /* int; IPv6 only on this socket */

/*Driver level socket errors*/
#define A_SOCK_INVALID           -2  /*Socket handle is invalid*/ 

#define CUSTOM_ALLOC(size)  \
	custom_alloc(size)
	
#define CUSTOM_FREE(buf)  \
	custom_free(buf)

#define     ATH_IPPROTO_IP     0  
#define     ATH_IPPROTO_ICMP   1
#define     ATH_IPPROTO_IGMP   2  /* added for IP multicasting changes */
#define     ATH_IPPROTO_TCP    6
#define     ATH_IPPROTO_UDP    17
#define     ATH_IPPROTO_IPV6      41 /* IPv6 header */
#define     ATH_IPPROTO_ROUTING   43 /* IPv6 Routing header */
#define     ATH_IPPROTO_FRAGMENT  44 /* IPv6 fragmentation header */



/******************************************************************************
 * *****************************     CAUTION     ******************************
 * THESE DATA STRUCTURES ARE USED BY FIRMWARE ALSO. MAKE SURE THAT BOTH ARE IN
 * SYNCH WHEN YOU MODIFY THESE.
 ******************************************************************************/

typedef PREPACK struct ip6_addr {
   A_UINT8   addr[16];   /* 128 bit IPv6 address */
}POSTPACK IP6_ADDR_T;

typedef PREPACK struct sockaddr {
	A_UINT16 sin_port FIELD_PACKED;		//Port number
	A_UINT16 sin_family FIELD_PACKED;	//ATH_AF_INET
	A_UINT32 sin_addr FIELD_PACKED;		//IPv4 Address
} POSTPACK SOCKADDR_T;


typedef PREPACK struct sockaddr_6
{
   A_UINT16        sin6_family FIELD_PACKED;   // ATH_AF_INET6 
   A_UINT16        sin6_port FIELD_PACKED;     // transport layer port #
   A_UINT32        sin6_flowinfo FIELD_PACKED; // IPv6 flow information 
   IP6_ADDR_T     sin6_addr FIELD_PACKED;      // IPv6 address 
   A_UINT32        sin6_scope_id FIELD_PACKED; // set of interfaces for a scope 
}POSTPACK SOCKADDR_6_T;

typedef PREPACK struct _ip_mreq
{
   A_UINT32 imr_multiaddr FIELD_PACKED;     //Multicast group address
   A_UINT32 imr_interface FIELD_PACKED;     //Interface address
}POSTPACK IP_MREQ_T;


typedef PREPACK struct ipv6_mreq {
   IP6_ADDR_T ipv6mr_multiaddr FIELD_PACKED; /* IPv6 multicast addr */
   IP6_ADDR_T ipv6mr_interface FIELD_PACKED; /* IPv6 interface address */
}POSTPACK IPV6_MREQ_T;


enum SOCKET_TYPE 
{
	SOCK_STREAM_TYPE=1,    /* TCP */
	SOCK_DGRAM_TYPE,       /* UDP */
	SOCK_RAW_TYPE          /* IP */
};

enum IPCONFIG_MODE
{
    IPCFG_QUERY=0,          /*Retrieve IP parameters*/
    IPCFG_STATIC,           /*Set static IP parameters*/
    IPCFG_DHCP              /*Run DHCP client*/
};



/*Following functions prototypes are for APIS that are used by an application*********************/



#if ZERO_COPY
A_VOID zero_copy_free(A_VOID* buffer);
#endif

A_INT32 t_socket(ENET_CONTEXT_STRUCT_PTR enet_ptr, A_UINT32 domain, A_UINT32 type, A_UINT32 protocol);
A_INT32 t_shutdown(ENET_CONTEXT_STRUCT_PTR enet_ptr, A_UINT32 handle);
A_INT32 t_connect(ENET_CONTEXT_STRUCT_PTR enet_ptr, A_UINT32 handle, A_VOID* name, A_UINT16 length);
A_INT32 t_bind(ENET_CONTEXT_STRUCT_PTR enet_ptr, A_UINT32 handle, A_VOID* name, A_UINT16 length);
A_INT32 t_listen(ENET_CONTEXT_STRUCT_PTR enet_ptr, A_UINT32 handle, A_UINT32 backlog);
A_INT32 t_accept(ENET_CONTEXT_STRUCT_PTR enet_ptr, A_UINT32 handle, A_VOID* name, A_UINT16 length);
A_INT32 t_sendto(ENET_CONTEXT_STRUCT_PTR enet_ptr, A_UINT32 handle, uint_8* buffer, A_UINT32 length, A_UINT32 flags, A_VOID* name, A_UINT32 socklength);
A_INT32 t_send(ENET_CONTEXT_STRUCT_PTR enet_ptr, A_UINT32 handle, uint_8* buffer, A_UINT32 length, A_UINT32 flags);

A_INT32 t_setsockopt(ENET_CONTEXT_STRUCT_PTR enet_ptr, A_UINT32 handle, A_UINT32 level, A_UINT32 optname, uint_8* optval, A_UINT32 optlen);
A_INT32 t_getsockopt(ENET_CONTEXT_STRUCT_PTR enet_ptr, A_UINT32 handle, A_UINT32 level, A_UINT32 optname, uint_8* optval, A_UINT32 optlen);

#if ZERO_COPY
A_INT32 t_recvfrom(ENET_CONTEXT_STRUCT_PTR enet_ptr, A_UINT32 handle, void** buffer, A_UINT32 length, A_UINT32 flags, A_VOID* name, A_UINT32* socklength);
A_INT32 t_recv(ENET_CONTEXT_STRUCT_PTR enet_ptr, A_UINT32 handle, void** buffer, A_UINT32 length, A_UINT32 flags);
#else
A_INT32 t_recvfrom(ENET_CONTEXT_STRUCT_PTR enet_ptr, A_UINT32 handle, void* buffer, A_UINT32 length, A_UINT32 flags, A_VOID* name, A_UINT32* socklength);
A_INT32 t_recv(ENET_CONTEXT_STRUCT_PTR enet_ptr, A_UINT32 handle, void* buffer, A_UINT32 length, A_UINT32 flags);
#endif
A_INT32 t_select(ENET_CONTEXT_STRUCT_PTR enet_ptr, A_UINT32 handle, A_UINT32 tv);
A_INT32 t_errno(ENET_CONTEXT_STRUCT_PTR enet_ptr, A_UINT32 handle);
A_INT32 t_ipconfig(ENET_CONTEXT_STRUCT_PTR enet_ptr, A_UINT32 mode,A_UINT32* ipv4_addr, A_UINT32* subnetMask, A_UINT32* gateway4);
A_INT32 t_ping(ENET_CONTEXT_STRUCT_PTR enet_ptr, A_UINT32 ipv4_addr, A_UINT32 size);
A_INT32 t_ping6(ENET_CONTEXT_STRUCT_PTR enet_ptr, A_UINT8 *ip6addr, A_UINT32 size);
A_INT32 t_ip6config(ENET_CONTEXT_STRUCT_PTR enet_ptr, A_UINT32 mode,IP6_ADDR_T *v6Global,IP6_ADDR_T *v6Local,
		    IP6_ADDR_T *v6DefGw,IP6_ADDR_T *v6GlobalExtd,A_INT32 *LinkPrefix, A_INT32 *GlbPrefix, A_INT32 *DefGwPrefix,A_INT32 *GlbPrefixExtd);
A_INT32  t_ipconfig_dhcp_pool(ENET_CONTEXT_STRUCT_PTR enet_ptr,A_UINT32* start_ipv4_addr, A_UINT32* end_ipv4_addr,A_INT32 leasetime);
A_INT32  t_ip6config_router_prefix(ENET_CONTEXT_STRUCT_PTR enet_ptr,A_UINT8 *v6addr,A_INT32 prefixlen, A_INT32 prefix_lifetime,
		                     A_INT32 valid_lifetime);
A_INT32 custom_ipconfig_set_tcp_exponential_backoff_retry(ENET_CONTEXT_STRUCT_PTR enet_ptr,A_INT32 retry);
A_INT32 custom_ipconfig_set_ip6_status(ENET_CONTEXT_STRUCT_PTR enet_ptr,A_UINT16 status);	    
A_INT32 custom_ipconfig_dhcp_release(ENET_CONTEXT_STRUCT_PTR enet_ptr);
A_INT32 custom_ipconfig_set_tcp_rx_buffer(ENET_CONTEXT_STRUCT_PTR enet_ptr,A_INT32 rxbuf);
/***************************************************************************************************/

A_VOID* custom_alloc(A_UINT32 size);
A_VOID custom_free(A_VOID* buf);
#endif 
