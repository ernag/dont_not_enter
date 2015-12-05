#ifndef __rtcscfg_h__
#define __rtcscfg_h__
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
* $FileName: rtcscfg.h$
* $Version : 3.8.44.1$
* $Date    : Nov-2-2011$
*
* Comments:
*
*   This file contains the definitions for configuring
*   optional features in RTCS.
*   RTCS is delivered as follows:
*   RTCSCFG_CHECK_ERRORS                   1
*   RTCSCFG_CHECK_MEMORY_ALLOCATION_ERRORS 1
*   RTCSCFG_CHECK_VALIDITY                 1
*   RTCSCFG_CHECK_ADDRSIZE                 1
*   RTCSCFG_IP_DISABLE_DIRECTED_BROADCAST  0
*   RTCSCFG_BOOTP_RETURN_YIADDR            0
*   RTCSCFG_UDP_ENABLE_LBOUND_MULTICAST    0  
*   RTCSCFG_LOG_SOCKET_API                 1
*   RTCSCFG_LOG_PCB                        1
*   RTCSCFG_LINKOPT_8023                   0
*   RTCSCFG_LINKOPT_8021Q_PRIO             0
*   RTCSCFG_DISCARD_SELF_BCASTS            1
*
*   RTCSCFG_LOGGING                        0
*
*END************************************************************************/

/***************************************
**
** Error checking options
*/



/*
** When RTCSCFG_CHECK_ERRORS is 1, RTCS API functions will perform
** error checking on its parameters.
*/
#ifndef RTCSCFG_CHECK_ERRORS
   #define RTCSCFG_CHECK_ERRORS 1
#endif

/*
** When RTCSCFG_CHECK_MEMORY_ALLOCATION_ERRORS is 1, RTCS API functions
** will perform error checking on memory allocation.
*/
#ifndef RTCSCFG_CHECK_MEMORY_ALLOCATION_ERRORS
   #define RTCSCFG_CHECK_MEMORY_ALLOCATION_ERRORS 1
#endif

/*
** When RTCSCFG_CHECK_VALIDITY is 1, RTCS will check the VALID field
** in internal structures before accessing them.
*/
#ifndef RTCSCFG_CHECK_VALIDITY
   #define RTCSCFG_CHECK_VALIDITY 1
#endif

/*
** When RTCSCFG_CHECK_ADDRSIZE is 1, RTCS will check whether addrlen
** is at least sizeof(sockaddr_in) in functions that take a struct
** sockaddr _PTR_ parameter.  If not, RTCS returns an error (for bind(),
** connect() and sendto()) or does a partial copy (for accept(),
** getsockname(), getpeername() and recvfrom()).
*/
#ifndef RTCSCFG_CHECK_ADDRSIZE
   #define RTCSCFG_CHECK_ADDRSIZE 1
#endif


/***************************************
**
** Protocol behaviour options
*/

/*
** RTCSCFG_IP_DISABLE_DIRECTED_BROADCAST disables the reception and
** forwarding of directed broadcast datagrams.
*/
#ifndef RTCSCFG_IP_DISABLE_DIRECTED_BROADCAST
   #define RTCSCFG_IP_DISABLE_DIRECTED_BROADCAST 1
#endif

/*
** When RTCSCFG_BOOTP_RETURN_YIADDR is 1, the BOOTP_DATA_STRUCT has
** an additional field, which will be filled in with the YIADDR field
** of the BOOTREPLY.
*/
#ifndef RTCSCFG_BOOTP_RETURN_YIADDR
   #define RTCSCFG_BOOTP_RETURN_YIADDR 0
#endif

/*
** When RTCSCFG_UDP_ENABLE_LBOUND_MULTICAST is 1, locally bound sockets
** that are members of multicast groups will be able to receive messages
** sent to both their unicast and multicast addresses.
*/
#ifndef RTCSCFG_UDP_ENABLE_LBOUND_MULTICAST
   #define RTCSCFG_UDP_ENABLE_LBOUND_MULTICAST 0
#endif


/***************************************
**
** Link layer options
*/

/*
** RTCSCFG_LINKOPT_8023 enables support for sending and receiving
** 802.3 frames.
*/
#ifndef RTCSCFG_LINKOPT_8023
   #define RTCSCFG_LINKOPT_8023 0
#endif

/*
** RTCSCFG_LINKOPT_8021Q_PRIO enables support for sending and receiving
** 802.1Q priority tags.
*/
#ifndef RTCSCFG_LINKOPT_8021Q_PRIO
   #define RTCSCFG_LINKOPT_8021Q_PRIO 0
#endif

/* 
** RTCSCFG_DISCARD_SELF_BCASTS controls whether or not to discard all 
** broadcast pkts that we sent, as they are likely echoes from older 
** hubs
*/ 
#ifndef RTCSCFG_DISCARD_SELF_BCASTS
   #define RTCSCFG_DISCARD_SELF_BCASTS 1
#endif




/***************************************
**
** Code and Data configuration options
**
**
*/
#ifndef RTCSCFG_FEATURE_DEFAULT

#if RTCS_MINIMUM_FOOTPRINT
   #define RTCSCFG_FEATURE_DEFAULT  0
#else
   #define RTCSCFG_FEATURE_DEFAULT  1
#endif

#endif // RTCSCFG_FEATURE_DEFAULT

/* 
** Enabled Protocols
*/
#ifndef RTCSCFG_ENABLE_ICMP
   #define RTCSCFG_ENABLE_ICMP RTCSCFG_FEATURE_DEFAULT
#endif

#ifndef RTCSCFG_ENABLE_IGMP
   #define RTCSCFG_ENABLE_IGMP RTCSCFG_FEATURE_DEFAULT
#endif

#ifndef RTCSCFG_ENABLE_NAT
   #define RTCSCFG_ENABLE_NAT  0 
#endif

#ifndef RTCSCFG_ENABLE_DNS
   #define RTCSCFG_ENABLE_DNS  RTCSCFG_FEATURE_DEFAULT 
#endif

#ifndef RTCSCFG_ENABLE_LWDNS
   #define RTCSCFG_ENABLE_LWDNS  1 
#endif

#ifndef RTCSCFG_ENABLE_IPSEC
   #define RTCSCFG_ENABLE_IPSEC 0
#endif

#ifndef RTCSCFG_ENABLE_IPIP
   #define RTCSCFG_ENABLE_IPIP RTCSCFG_FEATURE_DEFAULT
#endif

#ifndef RTCSCFG_ENABLE_RIP
   #define RTCSCFG_ENABLE_RIP RTCSCFG_FEATURE_DEFAULT
#endif

#ifndef RTCSCFG_ENABLE_OSPF
   #define RTCSCFG_ENABLE_OSPF 0
#endif

#ifndef RTCSCFG_ENABLE_SNMP
   #define RTCSCFG_ENABLE_SNMP RTCSCFG_FEATURE_DEFAULT
#endif

#ifndef RTCSCFG_ENABLE_IP_REASSEMBLY
   #define RTCSCFG_ENABLE_IP_REASSEMBLY RTCSCFG_FEATURE_DEFAULT
#endif

#ifndef RTCSCFG_ENABLE_LOOPBACK
   #define RTCSCFG_ENABLE_LOOPBACK  RTCSCFG_FEATURE_DEFAULT
#endif

#ifndef RTCSCFG_ENABLE_UDP
   #define RTCSCFG_ENABLE_UDP  RTCSCFG_FEATURE_DEFAULT
#endif

#ifndef RTCSCFG_ENABLE_TCP
   #define RTCSCFG_ENABLE_TCP  RTCSCFG_FEATURE_DEFAULT
#endif

#ifndef RTCSCFG_ENABLE_STATS
   #define RTCSCFG_ENABLE_STATS  RTCSCFG_FEATURE_DEFAULT
#endif

#ifndef RTCSCFG_ENABLE_GATEWAYS 
   #define RTCSCFG_ENABLE_GATEWAYS RTCSCFG_FEATURE_DEFAULT
#endif

/*
** Note: must be true for PPP or tunnelling
*/
#ifndef RTCSCFG_ENABLE_VIRTUAL_ROUTES 
   #define RTCSCFG_ENABLE_VIRTUAL_ROUTES RTCSCFG_FEATURE_DEFAULT
#endif

#ifndef RTCSCFG_USE_KISS_RNG 
   #define RTCSCFG_USE_KISS_RNG 0
#endif



/***************************
** 
** ARP Configuration options
**
*/

/*
** ARP cache hash table size
*/
#ifndef ARPCACHE_SIZE
   #if RTCS_MINIMUM_FOOTPRINT
      #define ARPCACHE_SIZE 2
   #else
      #define ARPCACHE_SIZE 16
   #endif
#endif

/*
** ARP cache entry longevity (in milliseconds)
*/
#ifndef RTCSCFG_ARPTIME_RESEND_MIN
    #define RTCSCFG_ARPTIME_RESEND_MIN             5000     /*  5 sec */
#endif
#ifndef RTCSCFG_ARPTIME_RESEND_MAX  
    #define RTCSCFG_ARPTIME_RESEND_MAX            30000     /* 30 sec */
#endif
#ifndef RTCSCFG_ARPTIME_EXPIRE_INCOMPLETE
    #define RTCSCFG_ARPTIME_EXPIRE_INCOMPLETE    180000L    /*  3 min */
#endif
#ifndef RTCSCFG_ARPTIME_EXPIRE_COMPLETE
    #define RTCSCFG_ARPTIME_EXPIRE_COMPLETE     1200000L    /* 20 min */
#endif

/*
** Allocation blocks for ARP cache entries
*/
#ifndef ARPALLOC_SIZE
   #if RTCS_MINIMUM_FOOTPRINT
      #define ARPALLOC_SIZE 2 
   #else
      #define ARPALLOC_SIZE 16
   #endif
#endif

/*
** Number of PCBs queued on an outstanding ARP
** Per RFC, Minimum recommended is 1.
** When sending large UDP packets, which will result 
** in IP fragmentations, set to at least the largest number of 
** fragments
*/
#ifndef ARP_ENTRY_MAX_QUEUED_PCBS
   #if RTCS_MINIMUM_FOOTPRINT
      #define ARP_ENTRY_MAX_QUEUED_PCBS 1 
   #else
      #define ARP_ENTRY_MAX_QUEUED_PCBS 4
   #endif
#endif

#ifndef RTCSCFG_ENABLE_ARP_STATS
   #define RTCSCFG_ENABLE_ARP_STATS RTCSCFG_ENABLE_STATS
#endif


/***************************
** 
** PCB Configuration options
**
*/

// Override in application by setting _RTCSPCB_max
#ifndef RTCSCFG_PCBS_INIT
   #if RTCS_MINIMUM_FOOTPRINT
      #define RTCSCFG_PCBS_INIT 4
   #else
      #define RTCSCFG_PCBS_INIT 32
   #endif
#endif

// Override in application by setting _RTCSPCB_max
#ifndef RTCSCFG_PCBS_GROW
   #define RTCSCFG_PCBS_GROW 0 
#endif

// Override in application by setting _RTCSPCB_max
#ifndef RTCSCFG_PCBS_MAX
   #define RTCSCFG_PCBS_MAX RTCSCFG_PCBS_INIT 
#endif

/***************************
** 
** RTCS Message Configuration options
**
*/
// Override in application by setting _RTCS_msgpool_init
#ifndef RTCSCFG_MSGPOOL_INIT
   #if RTCS_MINIMUM_FOOTPRINT
      #define RTCSCFG_MSGPOOL_INIT 8 
   #else
      #define RTCSCFG_MSGPOOL_INIT 32
   #endif
#endif

// Override in application by setting _RTCS_msgpool_grow
#ifndef RTCSCFG_MSGPOOL_GROW
   #if RTCS_MINIMUM_FOOTPRINT
      #define RTCSCFG_MSGPOOL_GROW 2 
   #else
      #define RTCSCFG_MSGPOOL_GROW 16
   #endif
#endif

// Override in application by setting _RTCS_msgpool_limit
#ifndef RTCSCFG_MSGPOOL_MAX
   #if RTCS_MINIMUM_FOOTPRINT
      #define RTCSCFG_MSGPOOL_MAX RTCSCFG_MSGPOOL_INIT 
   #else
      #define RTCSCFG_MSGPOOL_MAX 160
   #endif
#endif


/***************************
** 
** IP Configuration options
**
*/

#ifndef IPIFALLOC_SIZE
   #if RTCS_MINIMUM_FOOTPRINT
      #define IPIFALLOC_SIZE 2 
   #else
      #define IPIFALLOC_SIZE 4
   #endif
#endif

#ifndef IPROUTEALLOC_SIZE
   #if RTCS_MINIMUM_FOOTPRINT
      #define IPROUTEALLOC_SIZE 2 
   #else
      #define IPROUTEALLOC_SIZE 4
   #endif
#endif

#ifndef IPGATEALLOC_SIZE
   #if RTCS_MINIMUM_FOOTPRINT
      #define IPGATEALLOC_SIZE 1 
   #else
      #define IPGATEALLOC_SIZE 4
   #endif
#endif

#ifndef IPMCBALLOC_SIZE
   #if RTCS_MINIMUM_FOOTPRINT
      #define IPMCBALLOC_SIZE 1 
   #else
      #define IPMCBALLOC_SIZE 4
   #endif
#endif

#ifndef RADIXALLOC_SIZE
   #if RTCS_MINIMUM_FOOTPRINT
      #define RADIXALLOC_SIZE 2 
   #else
      #define RADIXALLOC_SIZE 4
   #endif
#endif

#ifndef RTCSCFG_ENABLE_IP_STATS
   #define RTCSCFG_ENABLE_IP_STATS RTCSCFG_ENABLE_STATS
#endif

#ifndef RTCSCFG_ENABLE_ICMP_STATS
   #define RTCSCFG_ENABLE_ICMP_STATS (RTCSCFG_ENABLE_STATS & RTCSCFG_ENABLE_ICMP)
#endif

#ifndef RTCSCFG_ENABLE_IPIF_STATS
   #define RTCSCFG_ENABLE_IPIF_STATS RTCSCFG_ENABLE_STATS
#endif

/***************************
** 
** IGMP Configuration options
**
*/
#ifndef RTCSCFG_ENABLE_IGMP_STATS
   #define RTCSCFG_ENABLE_IGMP_STATS (RTCSCFG_ENABLE_STATS & RTCSCFG_ENABLE_IGMP)
#endif

#if RTCSCFG_ENABLE_IGMP
   #ifdef BSP_ENET_DEVICE_COUNT
   #if  (BSP_ENET_DEVICE_COUNT > 0) 
      #if !(BSPCFG_ENABLE_ENET_MULTICAST)
         #error RTCS IGMP uses ENET Multicast.  Enable BSPCFG_ENABLE_ENET_MULTICAST or disable RTCSCFG_ENABLE_IGMP in user_config.h
      #endif
   #endif
   #endif
#endif


/***************************
** 
** Socket Configuration options
**
*/

#ifndef RTCSCFG_SOCKET_OWNERSHIP
   #define RTCSCFG_SOCKET_OWNERSHIP RTCSCFG_FEATURE_DEFAULT
#endif

// Number of blocks allocated for sockets
// Override in application by setting _RTCS_socket_part_init
#ifndef RTCSCFG_SOCKET_PART_INIT
   #if RTCS_MINIMUM_FOOTPRINT
      #define RTCSCFG_SOCKET_PART_INIT 3 
   #else
      #define RTCSCFG_SOCKET_PART_INIT 20
   #endif
#endif

// Override in application by setting _RTCS_socket_part_grow
#ifndef RTCSCFG_SOCKET_PART_GROW
   #if RTCS_MINIMUM_FOOTPRINT
      #define RTCSCFG_SOCKET_PART_GROW 0 
   #else
      #define RTCSCFG_SOCKET_PART_GROW 20
   #endif
#endif

// Override in application by setting _RTCS_socket_part_limit
#ifndef RTCSCFG_SOCKET_PART_MAX
   #define RTCSCFG_SOCKET_PART_MAX RTCSCFG_SOCKET_PART_INIT 
#endif



/***************************
** 
** UDP Configuration options
**
*/
// Override in application by setting _UDP_max_queue_size
#ifndef RTCSCFG_UDP_MAX_QUEUE_SIZE
   #if RTCS_MINIMUM_FOOTPRINT
      #define RTCSCFG_UDP_MAX_QUEUE_SIZE 1 
   #else
      #define RTCSCFG_UDP_MAX_QUEUE_SIZE (RTCSCFG_PCBS_MAX/2)
   #endif
#endif

#ifndef RTCSCFG_ENABLE_UDP_STATS
   #define RTCSCFG_ENABLE_UDP_STATS (RTCSCFG_ENABLE_STATS & RTCSCFG_ENABLE_UDP)
#endif

/***************************
** 
** TCP Configuration options
**
*/

#ifndef RTCSCFG_ENABLE_TCP_STATS
   #define RTCSCFG_ENABLE_TCP_STATS (RTCSCFG_ENABLE_STATS & RTCSCFG_ENABLE_TCP)
#endif


// Maximum number of simultaneous connections allowed.  Define as 0 for no limit.
#ifndef RTCSCFG_TCP_MAX_CONNECTIONS
   #define RTCSCFG_TCP_MAX_CONNECTIONS      0  
#endif

// Maximum number of simultaneoushalf open connections allowed.
// Define as 0 to disable the SYN attack recovery feature. 
#ifndef RTCSCFG_TCP_MAX_HALF_OPEN
   #define RTCSCFG_TCP_MAX_HALF_OPEN        0  
#endif

#ifndef RTCSCFG_TCP_ACKDELAY
    #define TCP_ACKDELAY                    10 //10
#else
    #if RTCSCFG_TCP_ACKDELAY > 500   /* Max ack delay, as per RFC1122 */
        #define TCP_ACKDELAY                500
    #else
        #define TCP_ACKDELAY                RTCSCFG_TCP_ACKDELAY
    #endif
#endif




/***************************
** 
** RIP Configuration options
**
*/
#ifndef RTCSCFG_ENABLE_RIP_STATS
   #define RTCSCFG_ENABLE_RIP_STATS RTCSCFG_ENABLE_STATS
#endif

#if RTCSCFG_ENABLE_RIP
   #if !(RTCSCFG_ENABLE_IGMP)
      #error RIP uses IGMP.  Enable RTCSCFG_ENABLE_IGMP or disable RTCSCFG_ENABLE_RIP in user_config.h
   #endif
#endif

/***************************
** 
** RTCS Configuration options
**
*/

/* Using a lower queue base requires fewer MQX resources
*/
// Override in application by setting _RTCSQUEUE_base
#ifndef RTCSCFG_QUEUE_BASE
   #if RTCS_MINIMUM_FOOTPRINT
      #define RTCSCFG_QUEUE_BASE 2 
   #else
      #define RTCSCFG_QUEUE_BASE 0x10
   #endif
#endif

// Override in application by setting _RTCSTASK_stacksize
#ifndef RTCSCFG_STACK_SIZE
   #if RTCS_MINIMUM_FOOTPRINT
      #define RTCSCFG_STACK_SIZE (750*sizeof(uint_32)) 
   #else
      #define RTCSCFG_STACK_SIZE (1000*sizeof(uint_32))
   #endif
#endif

#ifndef RTCSCFG_PRECISE_TIME
    #if RTCS_MINIMUM_FOOTPRINT
        #define RTCSCFG_PRECISE_TIME    1
    #else
        #define RTCSCFG_PRECISE_TIME    0
    #endif
#endif

/***************************************
**
** Logging options
*/

// TODO clean and fix defines, add filters

/* Enable RTCS logging functionality */
#ifndef RTCSCFG_LOGGING
#define RTCSCFG_LOGGING     RTCSCFG_FEATURE_DEFAULT
#endif

/*
** When RTCSCFG_LOG_SOCKET_API is 1, RTCS will call RTCS_log() on
** every socket API entry and exit.
*/
#ifndef RTCSCFG_LOG_SOCKET_API
#define RTCSCFG_LOG_SOCKET_API RTCSCFG_FEATURE_DEFAULT
#endif

/*
** When RTCSCFG_LOG_PCB is 1, RTCS will call RTCS_log() every time
** a PCB is allocated, freed, or passed between two layers.
*/
#ifndef RTCSCFG_LOG_PCB
#define RTCSCFG_LOG_PCB RTCSCFG_FEATURE_DEFAULT
#endif



/***************************
** 
** FTP Client Configuration options
**
*/
#ifndef FTPCCFG_SMALL_FILE_PERFORMANCE_ENHANCEMENT
   #define FTPCCFG_SMALL_FILE_PERFORMANCE_ENHANCEMENT 1  // Better performance for files less than 4MB
#endif

#ifndef FTPCCFG_BUFFER_SIZE
   #if RTCS_MINIMUM_FOOTPRINT
      #define FTPCCFG_BUFFER_SIZE 256 
   #else
      #define FTPCCFG_BUFFER_SIZE (16*1460)  /* Should be a multiple of maximum packet size */
   #endif
#endif

#ifndef FTPCCFG_WINDOW_SIZE
   #if RTCS_MINIMUM_FOOTPRINT
      #define FTPCCFG_WINDOW_SIZE 536 
   #else
      #define FTPCCFG_WINDOW_SIZE (3*1460)  
   #endif
#endif

#ifndef FTPCCFG_TIMEWAIT_TIMEOUT
   #define FTPCCFG_TIMEWAIT_TIMEOUT 1000
#endif


/***************************
** 
** FTP Server Configuration options
**
*/
// must be one of FLAG_ABORT_CONNECTION or FLAG_CLOSE_TX
#ifndef FTPDCFG_SHUTDOWN_OPTION
   #define FTPDCFG_SHUTDOWN_OPTION FLAG_ABORT_CONNECTION  
#endif

#ifndef FTPDCFG_DATA_SHUTDOWN_OPTION
   #define FTPDCFG_DATA_SHUTDOWN_OPTION FLAG_CLOSE_TX  
#endif


#ifndef FTPDCFG_USES_MFS
   #if RTCS_MINIMUM_FOOTPRINT
      #define FTPDCFG_USES_MFS 0 
   #else
      #define FTPDCFG_USES_MFS 1
   #endif
#endif

#ifndef FTPDCFG_ENABLE_MULTIPLE_CLIENTS
   #define FTPDCFG_ENABLE_MULTIPLE_CLIENTS RTCSCFG_FEATURE_DEFAULT 
#endif

#ifndef FTPDCFG_ENABLE_USERNAME_AND_PASSWORD
   #define FTPDCFG_ENABLE_USERNAME_AND_PASSWORD RTCSCFG_FEATURE_DEFAULT 
#endif

#ifndef FTPDCFG_ENABLE_RENAME
   #define FTPDCFG_ENABLE_RENAME 1//RTCSCFG_FEATURE_DEFAULT 
#endif
/* Should be a multiple of maximum packet size */
// Override in application by setting FTPd_window_size
#ifndef FTPDCFG_WINDOW_SIZE
   #if RTCS_MINIMUM_FOOTPRINT
      #define FTPDCFG_WINDOW_SIZE (536) 
   #else
      #define FTPDCFG_WINDOW_SIZE (6*1460)  
   #endif
#endif

/* Should be a multiple of sector size */
// Override in application by setting FTPd_buffer_size
#ifndef FTPDCFG_BUFFER_SIZE
   #if RTCS_MINIMUM_FOOTPRINT
      #define FTPDCFG_BUFFER_SIZE 512 
   #else
      #define FTPDCFG_BUFFER_SIZE (2048)  
   #endif
#endif

#ifndef FTPDCFG_CONNECT_TIMEOUT
   #define FTPDCFG_CONNECT_TIMEOUT 1000 
#endif

#ifndef FTPDCFG_SEND_TIMEOUT
   #define FTPDCFG_SEND_TIMEOUT 5000 
#endif

#ifndef FTPDCFG_TIMEWAIT_TIMEOUT
   #define FTPDCFG_TIMEWAIT_TIMEOUT 500 
#endif

/***************************
** 
** Telnet Configuration options
**
*/

#ifndef TELNETDCFG_BUFFER_SIZE
   #if RTCS_MINIMUM_FOOTPRINT
      #define TELNETDCFG_BUFFER_SIZE 256 
   #else
      #define TELNETDCFG_BUFFER_SIZE (3*1460)  
   #endif
#endif

#ifndef TELNETDCFG_NOWAIT
   #define TELNETDCFG_NOWAIT FALSE 
#endif

#ifndef TELNETDCFG_ENABLE_MULTIPLE_CLIENTS
   #define TELNETDCFG_ENABLE_MULTIPLE_CLIENTS RTCSCFG_FEATURE_DEFAULT 
#endif

#ifndef TELENETDCFG_CONNECT_TIMEOUT
   #define TELENETDCFG_CONNECT_TIMEOUT 1000 
#endif

#ifndef TELENETDCFG_SEND_TIMEOUT
   #define TELENETDCFG_SEND_TIMEOUT 5000 
#endif

#ifndef TELENETDCFG_TIMEWAIT_TIMEOUT
   #define TELENETDCFG_TIMEWAIT_TIMEOUT 1000 
#endif

/***************************
** 
** SNMP Configuration options
**
*/
#ifndef RTCSCFG_ENABLE_SNMP_STATS
   #define RTCSCFG_ENABLE_SNMP_STATS RTCSCFG_ENABLE_STATS
#endif


/***************************
** 
** IPCFG Configuration options
**
*/
#ifndef RTCSCFG_IPCFG_ENABLE_DNS
   #define RTCSCFG_IPCFG_ENABLE_DNS (RTCSCFG_ENABLE_DNS & RTCSCFG_ENABLE_UDP) | (RTCSCFG_ENABLE_LWDNS)
#endif

#ifndef RTCSCFG_IPCFG_ENABLE_DHCP
   #define RTCSCFG_IPCFG_ENABLE_DHCP RTCSCFG_ENABLE_UDP
#endif

#ifndef RTCSCFG_IPCFG_ENABLE_BOOT
   #define RTCSCFG_IPCFG_ENABLE_BOOT 0
#endif

#ifndef RTCSCFG_USE_MQX_PARTITIONS
   #define RTCSCFG_USE_MQX_PARTITIONS 1
#endif

#ifndef RTCSCFG_USE_INTERNAL_PARTITIONS
   #define RTCSCFG_USE_INTERNAL_PARTITIONS !RTCSCFG_USE_MQX_PARTITIONS
#endif

/***************************
** 
** PPPHDLC Message Configuration options
**
*/
#ifndef PPPHDLC_INIT
   #if RTCS_MINIMUM_FOOTPRINT
      #define PPPHDLC_INIT 1 
   #else
      #define PPPHDLC_INIT 8
   #endif
#endif

#ifndef PPPHDLC_GROW
   #if RTCS_MINIMUM_FOOTPRINT
      #define PPPHDLC_GROW 1 
   #else
      #define PPPHDLC_GROW 0
   #endif
#endif

#ifndef PPPHDLC_MAX
   #if RTCS_MINIMUM_FOOTPRINT
      #define PPPHDLC_MAX 2 
   #else
      #define PPPHDLC_MAX 8
   #endif
#endif


#endif
/* EOF */
