/**HEADER********************************************************************
* 
* Copyright (c) 2008 Freescale Semiconductor;
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
* $FileName: httpd_cnfg.h$
* $Version : 3.8.24.0$
* $Date    : Sep-7-2011$
*
* Comments:
*
*   HTTPD default configuration.
*
*END************************************************************************/

#ifndef HTTPD_CNFG_H_
#define HTTPD_CNFG_H_

#include "user_config.h"
#include "rtcscfg.h"

#ifndef HTTPDCFG_POLL_MODE
	#if RTCS_MINIMUM_FOOTPRINT
		#define HTTPDCFG_POLL_MODE              1
	#else
		#define HTTPDCFG_POLL_MODE              0
	#endif
#endif

#ifndef HTTPDCFG_STATIC_TASKS
#define HTTPDCFG_STATIC_TASKS           0
#endif

#ifndef HTTPDCFG_DYNAMIC_TASKS
	#if RTCS_MINIMUM_FOOTPRINT
		#define HTTPDCFG_DYNAMIC_TASKS          0
	#else
		#define HTTPDCFG_DYNAMIC_TASKS          1
	#endif
#endif

#if (HTTPDCFG_POLL_MODE + HTTPDCFG_STATIC_TASKS + HTTPDCFG_DYNAMIC_TASKS) != 1
#error Define one of HTTPDCFG_POLL_MODE or HTTPDCFG_STATIC_TASK or HTTPDCFG_DYNAMIC_TASK !
#endif


#ifndef HTTPDCFG_DEF_SERVER_PRIO
#define HTTPDCFG_DEF_SERVER_PRIO    (8)
#endif

#ifndef HTTPDCFG_DEF_SESSION_PRIO
#define HTTPDCFG_DEF_SESSION_PRIO   (8)
#endif

#ifndef HTTPDCFG_DEF_ADDR
#define HTTPDCFG_DEF_ADDR               INADDR_ANY     // default listen address
#endif

#ifndef HTTPDCFG_DEF_PORT
#define HTTPDCFG_DEF_PORT               80      // default listen port
#endif

#ifndef HTTPDCFG_DEF_INDEX_PAGE
#define HTTPDCFG_DEF_INDEX_PAGE         "index.htm"
#endif

#ifndef HTTPDCFG_DEF_SES_CNT
#define HTTPDCFG_DEF_SES_CNT            2       // default sessions count
#endif

#ifndef HTTPDCFG_DEF_URL_LEN
#define HTTPDCFG_DEF_URL_LEN            128     // maximal URL length
#endif

#ifndef HTTPDCFG_DEF_AUTH_LEN
#define HTTPDCFG_DEF_AUTH_LEN           16      // maximal length for auth data
#endif

#ifndef HTTPDCFG_MAX_BYTES_TO_SEND
#define HTTPDCFG_MAX_BYTES_TO_SEND      (512)  // maximal send data size in one step
#endif

#ifndef HTTPDCFG_MAX_SCRIPT_LN
#define HTTPDCFG_MAX_SCRIPT_LN          16      // maximal length for script line
#endif

#ifndef HTTPDCFG_RECV_BUF_LEN
#define HTTPDCFG_RECV_BUF_LEN           32
#endif

/* Default buffer configuration */
#ifndef HTTPD_MAX_LEN
#define HTTPD_MAX_LEN                   128
#endif

#ifndef HTTPDCFG_MAX_HEADER_LEN
#define HTTPDCFG_MAX_HEADER_LEN         256  // maximal length for http header
#endif

#ifndef HTTPDCFG_SES_TO
#define HTTPDCFG_SES_TO                 (20000) // session timeout in ms
#endif


#ifndef HTTPD_TIMEOUT_REQ_MS
#define HTTPD_TIMEOUT_REQ_MS            (4000) /*ms. Request Timeout */
#endif

#ifndef HTTPD_TIMEOUT_SEND_MS
#define HTTPD_TIMEOUT_SEND_MS           (8000) /*ms. Send Timeout */
#endif

/* socket settings */

#ifndef HTTPCFG_TX_WINDOW_SIZE
   #if RTCS_MINIMUM_FOOTPRINT
      #define HTTPCFG_TX_WINDOW_SIZE    (1460) 
   #else
      #define HTTPCFG_TX_WINDOW_SIZE    (3*1460) 
   #endif
#endif

#ifndef HTTPCFG_RX_WINDOW_SIZE
   #if RTCS_MINIMUM_FOOTPRINT
      #define HTTPCFG_RX_WINDOW_SIZE    (1460) 
   #else
      #define HTTPCFG_RX_WINDOW_SIZE    (3*1460) 
   #endif
#endif

#ifndef HTTPCFG_TIMEWAIT_TIMEOUT
   #define HTTPCFG_TIMEWAIT_TIMEOUT     1000 
#endif


#endif // HTTPD_CNFG_H_
