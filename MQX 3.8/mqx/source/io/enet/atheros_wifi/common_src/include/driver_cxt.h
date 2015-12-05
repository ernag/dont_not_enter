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
#ifndef _DRIVER_CXT_H_
#define _DRIVER_CXT_H_

#include "a_types.h"
#include <athdefs.h>
#include <htc_api.h>
#include <wmi.h>
#include "ieee80211.h"

#define MIN_STRRCL_MSEC (100)


#define AR4100_DEFAULT_CREDIT_COUNT		  6
#define AR4100_BUFFER_SIZE                1664
#define AR4100_MAX_AMSDU_RX_BUFFERS       4
#define AR4100_AMSDU_REFILL_THRESHOLD     3

#define AR4100_AMSDU_BUFFER_SIZE          (WMI_MAX_AMSDU_RX_DATA_FRAME_LENGTH + 128)

#if WLAN_CONFIG_AMSDU_ON_HOST
#define AR4100_MAX_RX_MESSAGE_SIZE        (max(WMI_MAX_NORMAL_RX_DATA_FRAME_LENGTH,WMI_MAX_AMSDU_RX_DATA_FRAME_LENGTH))
/* The RX MESSAGE SIZE is ultimately dictated by the size of the firmware buffers used to
 * hold packets.  This is because any remaining space may be used by HTC for trailers. hence
 * a full size buffer must be anticipated by the host. */
#else
#define AR4100_MAX_RX_MESSAGE_SIZE 		  (AR4100_BUFFER_SIZE)
#endif

#define MAX_NUM_CHANLIST_CHANNELS (16)

#define MAX_WEP_KEY_SZ (16)

#define STRRCL_ST_DISABLED 0
#define	STRRCL_ST_INIT 1
#define	STRRCL_ST_START 2
#define	STRRCL_ST_ACTIVE 3
#define STRRCL_ST_ACTIVE_B 4
#define STRRCL_ST_AWAIT_FW 5

#define AES_KEY_SIZE_BYTES  16

#define ACCESS_REQUEST_IOCTL 0
#define ACCESS_REQUEST_TX    1
#define ACCESS_REQUEST_RX    2

#define DRIVER_SCOPE_RX 0x01
#define DRIVER_SCOPE_TX 0x02

#define DRIVER_STATE_IDLE 0x00 /* the driver task is idle/sleeping */
#define DRIVER_STATE_RX_PROCESSING 0x01 /* the driver task is processing a RX request */
#define DRIVER_STATE_TX_PROCESSING 0x02 /* the driver task is processing a TX request */
#define DRIVER_STATE_INTERRUPT_PROCESSING 0x03 /* the driver task is processing a chip Interrupt */
#define DRIVER_STATE_ASYNC_PROCESSING 0x04 /* the driver task is processing an asynch operation */
#define DRIVER_STATE_PENDING_CONDITION_A 0x05 /* the driver task is waiting for a condition to be met */
#define DRIVER_STATE_PENDING_CONDITION_B 0x06
#define DRIVER_STATE_PENDING_CONDITION_C 0x07
#define DRIVER_STATE_PENDING_CONDITION_D 0x08
#define DRIVER_STATE_PENDING_CONDITION_E 0x09
#define DRIVER_STATE_PENDING_CONDITION_F 0x0a
#define DRIVER_STATE_PENDING_CONDITION_G 0x0b

#define MAX_CREDIT_DEADLOCK_ATTEMPTS 2
#define DEADLOCK_BLOCK_MSEC 10

typedef struct ar_wep_key {
    A_UINT8 keyLen;
    A_UINT8 key[MAX_WEP_KEY_SZ];
}A_WEPKEY_T;



/* A_SCAN_SUMMARY provides useful information that
 * has been parsed from bss info headers and probe/beacon information elements.
 */
typedef struct scan_summary {
	A_UINT16 beacon_period;
	A_UINT16 caps;
	A_UINT8 bssid[6];
    A_UINT8 ssid[32];
	A_UINT8 channel;
    A_UINT8 ssid_len;
    A_UINT8 rssi;
    A_UINT8 bss_type;
    A_UINT8 rsn_cipher;
    A_UINT8 rsn_auth;
    A_UINT8 wpa_cipher;
    A_UINT8 wpa_auth;
    A_UINT8 wep_support;
    A_UINT8 reserved; //keeps struct on 4-byte boundary
}A_SCAN_SUMMARY;


typedef struct _a_hcd_context
{
    #define SDHD_BOOL_SHUTDOWN 					0x00000001
    #define SDHD_BOOL_EXTERNALIO_PENDING 		0x00000002
    #define SDHD_BOOL_DMA_WRITE_WAIT_FOR_BUFFER 0x00000004
    #define SDHD_BOOL_DMA_IN_PROG 				0x00000008
    #define SDHD_BOOL_TRANSFER_DIR_RX			0x00000010
    #define SDHD_BOOL_HOST_DMA_COPY_MODE 		0x00000020
    #define SDHD_BOOL_HOST_ACCESS_COPY_MODE 	0x00000040
    #define SDHD_BOOL_FATAL_ERROR				0x00000080
    #define SDHD_BOOL_DMA_COMPLETE              0x00000100

    A_UINT16		booleans;				  /* all booleans for structure */
    A_UINT16        SpiIntEnableShadow;     /* shadow copy of interrupt enables */
    A_UINT16        SpiConfigShadow;        /* shadow copy of configuration register */
    A_UINT16        irqMask;
    A_UINT8         HostAccessDataWidth;    /* data width to use for host access */
    A_UINT8         DMADataWidth;           /* data width to use for DMA access */
    A_UINT32        ReadBufferSpace;        /* cached copy of read buffer available register */
    A_UINT32        WriteBufferSpace;         /* cached copy of space remaining in the SPI write buffer */
    A_UINT32        MaxWriteBufferSpace;      /* max write buffer space that the SPI interface supports */
    A_UINT32        PktsInSPIWriteBuffer;     /* number of packets in SPI write buffer so far */
    A_VOID          *pCurrentRequest;       /* pointer to a request in-progress used only for deferred SPI ops. */
#define BYTE_SWAP    TRUE
#define NO_BYTE_SWAP FALSE

    /*******************************************
     *
     * the following fields must be filled in by the hardware specific layer
     *
     ********************************************/
    A_UINT32        PowerUpDelay;           /* delay before the common layer should initialize over spi */
    A_UINT32        OperationalClock;       /* spi module operational clock */
    A_UINT8         SpiHWCapabilitiesFlags; /* SPI hardware capabilities flags */
    #define       HW_SPI_FRAME_WIDTH_8    0x01
    #define       HW_SPI_FRAME_WIDTH_16   0x02
    #define       HW_SPI_FRAME_WIDTH_24   0x04
    #define       HW_SPI_FRAME_WIDTH_32   0x08
    #define       HW_SPI_INT_EDGE_DETECT  0x80
    #define       HW_SPI_NO_DMA           0x40
    A_UINT8         MiscFlags;
    #define       MISC_FLAG_SPI_SLEEP_WAR          0x04
    #define       MISC_FLAG_RESET_SPI_IF_SHUTDOWN  0x02
    #define       MISC_FLAG_DUMP_STATE_ON_SHUTDOWN 0x01
    // from SDHCD START HERE
    A_UINT8         PendingIrqAcks; /* mask of pending interrupts that have not been ACK'd */
    A_UINT8         IrqsEnabled;
    A_UINT8         IrqDetected;
    // from SDHCD END HERE
    A_UINT8			CpuInterrupt;
}A_HCD_CXT;

typedef struct {
	A_UINT16 maxMsgLength;
	A_UINT16 serviceID;
	A_UINT32 intCredits;
    A_UINT8 intCreditCounter;
    A_UINT8 rptCreditCounter;
    A_UINT8 epIdx;
    A_UINT8 credits;
    A_UINT8 maxCredits;
    A_UINT8 seqNo;
}A_ENDPOINT_T;

typedef A_STATUS (*TEMP_FUNC_T)(A_VOID *pCxt);

typedef struct // _a_driver_context // this tag is unnecessary and messes with the linker
{
    A_UINT16		 driver_state; /* maintains current state of driver one of DRIVER_STATE_... */
    A_UINT8			 txQueryResult;
    A_BOOL			 txQueryInProgress;
    A_BOOL           driver_up;   /* maintains driver status for external access */
    A_BOOL           chipDown;
    A_BOOL           rxBufferStatus; /* maintains available status of rx buffers */
    A_BOOL 			 creditDeadlock;
    A_UINT32		 creditDeadlockCounter;
    A_NETBUF_QUEUE_T txQueue;
    A_VOID           *pRxReq;
    A_ENDPOINT_T 	 ep[ENDPOINT_MANAGED_MAX];
    HTC_ENDPOINT_ID  ac2EpMapping[WMM_NUM_AC];
    A_UINT8          ep2AcMapping[ENDPOINT_MAX];
    A_UINT16		 creditCount; /* number of tx credits available at init */
    A_UINT16         creditSize; //size in bytes of a credit
    /* the 5 version values maintained by the context */
    A_UINT32		targetType; /* general type value */
    A_UINT32        targetVersion; /* identifies the target chip */
    A_UINT32        wlanVersion; /* identifies firmware running on target chip */
    A_UINT32        abiVersion; /* identifies the interface compatible with wlan firmware */
    A_UINT32        hostVersion; /* identifies version of host driver */

    A_UINT8         phyCapability; /* identifies chip phy capability */
    A_BOOL          wmiReady;
    A_BOOL          wmiEnabled;
    A_VOID          *pWmiCxt; /* context for wmi driver component */
    A_VOID          *pAggrCxt; /* context for rx aggregate driver component */
    A_UINT16		txAggrTidMask;
    A_UINT16		rxAggrTidMask;

    A_UINT16        channelHint;
    A_INT8          numChannels;
    A_UINT16        channelList[MAX_NUM_CHANLIST_CHANNELS];

    A_UINT32        regCode;
    A_INT8          rssi;
    A_INT32         bitRate;
    A_BOOL          wmmEnabled;
    A_BOOL          tx_complete_pend; /* tracks new tx completes for task sync */
    TEMP_FUNC_T		asynchRequest;
    /* STORE&RECALL SPECIFICE ELEMENTS Starts here */
    A_VOID		    *strrclCxt;
    A_INT32			strrclCxtLen;
    A_BOOL          strrclBlock;
    A_UINT8	        strrclState;
    /* STORE&RECALL SPECIFICE ELEMENTS Ends here */
    /* WPS SPECIFICE ELEMENTS Starts here */
    A_VOID					*wpsBuf;
    A_VOID					*wpsEvent;
    A_BOOL					wpsState;
    /* WPS SPECIFIC ELEMENTS Ends here */
    A_UINT8			userPwrMode;
    A_UINT8			tkipCountermeasures;
    /* CONNECT SPECIFIC ELEMENTS Start Here */
    A_INT32         ssidLen;
    A_UINT8         ssid[32];
    A_UINT8         networkType; /* one of NETWORK_TYPE enum */
    A_UINT32        connectCtrlFlags;
    A_UINT8         dot11AuthMode;
    A_UINT8         wpaAuthMode;
    A_UINT8         wpaPairwiseCrypto;
    A_UINT8         wpaPairwiseCryptoLen;
    A_UINT8         wpaGroupCrypto;
    A_UINT8         wpaGroupCryptoLen;
    A_UINT8         wepDefTxKeyIndex;
    A_WEPKEY_T   	wepKeyList[WMI_MAX_KEY_INDEX + 1];
    A_UINT8		    wpaPmk[WMI_PMK_LEN];
    A_BOOL          wpaPmkValid;
    A_UINT8         bssid[ATH_MAC_LEN];
    A_UINT8         reqBssid[ATH_MAC_LEN];
    A_UINT16        bssChannel;
    A_BOOL          isConnected;
    A_BOOL          isConnectPending;
    A_UINT8			phyMode;
    /* CONNECT SPECIFIC ELEMENTS End Here */
    /* from hif_device START HERE */
    A_UINT16     enabledSpiInts;
    A_UINT32     lookAhead;
    A_UINT32     mboxAddress; /* the address for mailbox reads/writes. */
    A_UINT32     blockSize; /* the mailbox block size */
    A_UINT32     blockMask; /* the mask derived from BlockSize */
    A_UINT32     *padBuffer;
    /* from hif_device ENDS HERE */
    /* SPI SPECIFICE ELEMENTS Starts here */
    A_HCD_CXT       hcd;
    /* SPI SPECIFIC ELEMENTS Ends here */
    A_BOOL			scanDone;/* scan done indicator, set by WMI_SCAN_COMPLETE_EVENTID */
    A_BOOL			driverSleep;
    A_BOOL          htcStart;
    A_UINT8 		htc_creditInit;
    A_UINT8*        tempStorage;           /*temporary storage shared by independent modules*/
    A_UINT16        tempStorageLength;     /*size of temp storage allocated at init time, decided by feature set*/
    A_UINT8 		promiscuous_mode; 	/* if 1 promiscuous mode is enabled */
	A_BOOL			rssiFlag;

    A_UINT8         apmodeWPS;
#if ENABLE_P2P_MODE
    A_BOOL                  p2p_avail;
    A_UINT8                 wpsRole;
#endif
	A_BOOL          macProgramming;
#if MANUFACTURING_SUPPORT
    A_BOOL          testResp;
#endif
}A_DRIVER_CONTEXT;


extern A_UINT32 last_driver_error;

#define DEV_CALC_RECV_PADDED_LEN(pDCxt, length) (((length) + (pDCxt)->blockMask) & (~((pDCxt)->blockMask)))
#define DEV_CALC_SEND_PADDED_LEN(pDCxt, length) DEV_CALC_RECV_PADDED_LEN(pDCxt,length)
#define DEV_IS_LEN_BLOCK_ALIGNED(pDCxt, length) (((length) % (pDCxt)->blockSize) == 0)


#endif /* _DRIVER_CXT_H_ */
