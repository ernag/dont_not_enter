#ifndef __usbhost_cnfg_h__
#define __usbhost_cnfg_h__

#include "mqx.h"
#include "bsp.h"        // bsp.h include user_config.h, too

#ifndef USBCFG_DEFAULT_HOST_CONTROLLER
#define USBCFG_DEFAULT_HOST_CONTROLLER  (0)
#endif

#ifndef USBCFG_REGISTER_ENDIANNESS
#define USBCFG_REGISTER_ENDIANNESS MQX_BIG_ENDIAN
#endif

#ifndef USBCFG_MEMORY_ENDIANNESS
#define USBCFG_MEMORY_ENDIANNESS MQX_BIG_ENDIAN
#endif

#ifdef USBCFG_KHCI

    #define __USB_HOST_KHCI__

    /*
    ** device specific configuration
    */
    #ifndef USBCFG_MAX_PIPES
    #define USBCFG_MAX_PIPES                        (8)
    #endif

    #ifndef USBCFG_MAX_INTERFACE
    #define USBCFG_MAX_INTERFACE                    (1)
    #endif

    #ifndef USBCFG_HOST_NUM_ISO_PACKET_DESCRIPTORS
    #define USBCFG_HOST_NUM_ISO_PACKET_DESCRIPTORS  (0)
    #endif

    // KHCI task priority
    #ifndef USBCFG_KHCI_TASK_PRIORITY
    #define USBCFG_KHCI_TASK_PRIORITY               (8)
    #endif

    // wait time in tick for events - must be above 1 !!!
    #ifndef USBCFG_KHCI_WAIT_TICK
    #define USBCFG_KHCI_WAIT_TICK                   (_time_get_ticks_per_sec() / 10)    // 100ms
    #endif

    // max msg count for KHCI
    #ifndef USBCFG_KHCI_TR_QUE_MSG_CNT
    #define USBCFG_KHCI_TR_QUE_MSG_CNT              (10)
    #endif

    // max interrupt transaction count
    #ifndef USBCFG_KHCI_MAX_INT_TR
    #define USBCFG_KHCI_MAX_INT_TR                  (10)
    #endif

#endif // USBCFG_KHCI

#ifdef USBCFG_EHCI

    #define __USB_HOST_EHCI__

    #ifndef USBCFG_EHCI_USE_SW_TOGGLING
    #define USBCFG_EHCI_USE_SW_TOGGLING             (0)
    #endif

    #ifndef USBCFG_EHCI_PIPE_TIMEOUT
    #define USBCFG_EHCI_PIPE_TIMEOUT                (300)
    #endif

#endif // USBCFG_EHCI

/*
** default configuration
*/

// maximum pipes
#ifndef USBCFG_MAX_PIPES
#define  USBCFG_MAX_PIPES                       (24)
#endif

// maximum number of usb interfaces
#ifndef USBCFG_MAX_INTERFACE
#define USBCFG_MAX_INTERFACE                    (1)
#endif

#ifndef USBCFG_HOST_NUM_ISO_PACKET_DESCRIPTORS
#define USBCFG_HOST_NUM_ISO_PACKET_DESCRIPTORS  (0)
#endif

#ifndef USBCFG_MAX_DRIVERS
#define USBCFG_MAX_DRIVERS                      (1)
#endif

#ifndef USBCFG_DEFAULT_MAX_NAK_COUNT
#define USBCFG_DEFAULT_MAX_NAK_COUNT            (15)
#endif

#ifndef USBCFG_MFS_OPEN_READ_CAPACITY_RETRY_DELAY
#define USBCFG_MFS_OPEN_READ_CAPACITY_RETRY_DELAY   (_time_get_ticks_per_sec() * 1)
#endif

#ifndef USBCFG_MFS_LWSEM_TIMEOUT
#define USBCFG_MFS_LWSEM_TIMEOUT                    (_time_get_ticks_per_sec() * 20)
#endif

#ifndef USBCFG_MFS_MAX_RETRIES
#define USBCFG_MFS_MAX_RETRIES                  (2)
#endif

#ifndef USBCFG_CTRL_RETRY
#define USBCFG_CTRL_RETRY                       (4)
#endif

#endif
