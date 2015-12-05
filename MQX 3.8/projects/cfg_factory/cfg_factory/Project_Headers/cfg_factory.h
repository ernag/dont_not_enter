/*
 * cfg_factory.h
 *
 *  Created on: Jun 5, 2013
 *      Author: Ernie
 *      
 *  This is a library for setting arbitrary factory configs on the Corona platform.
 */

#ifndef CFG_FACTORY_H_
#define CFG_FACTORY_H_

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Defines
////////////////////////////////////////////////////////////////////////////////

/*
 *   Applications must include these sections in their linker files.
 */
#define FCFG_TXT_SEC        ".text.FCFG"
#define FCFG_DATA_SEC       ".data.g_fcfg_desc"

/*
 *   FACTORY_CFG's version of "Infinity", this is just a reasonable cieling to apply,
 *     to say that no revision number will every be larger than this.  This is to guard
 *     against the case where there is some bogus 32 bit value in Factory Config,
 *     like 0xFFFFFFFF, and we say it is good.
 */
#define FCFG_INFINITY   ( 10 * 1000 )

/*
 *   This length is reserved for integers only.  
 *   For strings of length 4, please use FCFG_NOT_INT_SZ.
 */
#define FCFG_INT_SZ         ( sizeof(unsigned long) )
#define FCFG_NOT_INT_SZ     ( sizeof(unsigned long) + 2 )

/*
 *   Console "man"ual pages.
 */
#define CC_MAN_SCFG "\
Set cfg\n\r\
\tscfg <cfg> <val>\n\n\r\
EX:\tscfg 2 77\t# set r2=77\n\r\
EX:\tscfg 15 $my_str\t# set r15='my_str'\n\r\
Note:\tUse pcfg to see cfgs\n\n\r"

#define CC_MAN_CCFG "\
Clear cfg\n\r\
\tccfg <cfg>\n\n\r\
EX:\tccfg *\t# clr ALL\n\r\
EX:\tccfg 5\t# clr r5\n\r\
Note:\tUse pcfg to see cfgs\n\n\r"

#define CC_MAN_PCFG "\
Print all\n\r\
pcfg\n\n\r"

////////////////////////////////////////////////////////////////////////////////
// Typedefs
////////////////////////////////////////////////////////////////////////////////

/*
 *   - TAG ID -
 *   
 *   https://whistle.atlassian.net/wiki/display/ENG/Device+Serial+and+Config+ID+tags
 *   https://whistle.atlassian.net/wiki/display/COR/Corona+assembly+revisions+with+software+impacts
 *   
 */
typedef enum fcfg_tag_type
{
    CFG_FACTORY_HEADER      = 0,
    CFG_FACTORY_FIRST_FCFG  = 1,
    CFG_FACTORY_SER         = 1,
    CFG_FACTORY_MPCBA       = 2,
    CFG_FACTORY_MPCBAREV    = 3,
    CFG_FACTORY_MPCBARW     = 4,
    CFG_FACTORY_LPCBA       = 5,
    CFG_FACTORY_LPCBAREV    = 6,
    CFG_FACTORY_LPCBARW     = 7,
    CFG_FACTORY_TLA         = 8,
    CFG_FACTORY_BURNIN_SSID = 9,
    CFG_FACTORY_FLAGS       = 10,
    CFG_FACTORY_PDDUR       = 11,
    CFG_FACTORY_PDFULL      = 12,
    CFG_FACTORY_PDLOW       = 13,
    CFG_FACTORY_ZAXIS_PCBA  = 14,
    CFG_FACTORY_ZAXIS_FTA   = 15,
    CFG_FACTORY_RESERVED_16 = 16,
    CFG_FACTORY_RESERVED_17 = 17,
    CFG_FACTORY_RESERVED_18 = 18,
    CFG_FACTORY_RESERVED_19 = 19,
    CFG_FACTORY_RESERVED_20 = 20,
    CFG_FACTORY_RESERVED_21 = 21,
    CFG_FACTORY_RESERVED_22 = 22,
    CFG_FACTORY_RESERVED_23 = 23,
    CFG_FACTORY_RESERVED_24 = 24,
    CFG_FACTORY_RESERVED_25 = 25,
    CFG_FACTORY_RESERVED_26 = 26,
    CFG_FACTORY_RESERVED_27 = 27,
    CFG_FACTORY_RESERVED_28 = 28,
    CFG_FACTORY_RESERVED_29 = 29,
    CFG_FACTORY_RESERVED_30 = 30,
    CFG_FACTORY_RESERVED_31 = 31,
    CFG_FACTORY_LAST_FCFG   = 31,
    
    // don't even think about it.
    CFG_FACTORY_GREATER_THAN_31_IS_BOGUS = 32
    
}fcfg_tag_t;

/*
 *    ERROR Codes for factory config library.
 */
typedef enum fcfg_error_type
{
    FCFG_OK           = 0,
    FCFG_BAD_TAG      = 1,
    FCFG_BAD_LEN      = 2,
    FCFG_VAL_UNSET    = 3,
    FCFG_WRONG_ADDR   = 4,
    FCFG_BAD_HANDLE   = 5,
    FCFG_DESC_UNKNOWN = 6,
    FCFG_FLASH_ERROR  = 7,
    FCFG_MALLOC_ERROR = 8,
    FCFG_HEADER_BIG   = 9
    
} fcfg_err_t;

/*
 *   Represents the board type present on the system.
 *     NOTE: There is not a factory config for this. 
 *           It is calculated based on revision / part number.
 */
typedef enum fcfg_board_type
{
    FCFG_BOARD_P3_DEV            = (unsigned char) 1,
    FCFG_BOARD_P4_DEV            = (unsigned char) 2,
    FCFG_BOARD_P4_STUFFED_P3_DEV = (unsigned char) 3,
    FCFG_BOARD_MLB_REV_3         = (unsigned char) 4,
    FCFG_BOARD_MLB_REV_4         = (unsigned char) 5,
    FCFG_BOARD_MLB_REV_5         = (unsigned char) 6,
    
    FCFG_BOARD_INVALID           = (unsigned char) 0xFF
    
}fcfg_board_t;

/*
 *   Which type of Silego chip is on the board.
 *     NOTE: There is not a factory config for this. 
 *           It is calculated based on revision / part number.
 */
typedef enum fcfg_silego_type
{
    FCFG_SILEGO_OLD_BZE  = (unsigned char) 1,
    FCFG_SILEGO_NEW_BZI  = (unsigned char) 2,
    
    FCFG_SILEGO_INVALID  = (unsigned char) 0xFF
    
}fcfg_silego_t;

/*
 *   Which type of RF Switch is on the board.
 *     NOTE: There is not a factory config for this. 
 *           It is calculated based on revision / part number.
 */
typedef enum fcfg_rfswitch_type
{
    FCFG_RFSWITCH_AS169    = (unsigned char) 169,
    FCFG_RFSWITCH_AS193    = (unsigned char) 193,
    
    FCFG_RFSWITCH_INVALID  = (unsigned char) 0xFF
    
}fcfg_rfswitch_t;

/*
 *   Which LED's are on the LED flex?
 */
typedef enum fcfg_led_type
{
    FCFG_LED_ROHM        = (unsigned char) 1,
    FCFG_LED_EVERBRIGHT  = (unsigned char) 2,
    
    FCFG_LED_INVALID     = (unsigned char) 0xFF
    
}fcfg_led_t;

/*
 *   LENGTH - in bytes of payload.  LIMIT = 128 bytes
 */
typedef unsigned char fcfg_len_t;

/*
 *   VALUE - pointer to config's payload.
 */
typedef void         *fcfg_pval_t;

/*
 *   ADDRESS - where in SPI FLASH 4KB section do you want to store factory config data.
 */
typedef unsigned long fcfg_addr_t;

/*
 *   READ CALLBACK - callback to use for reading from SPI Flash.
 */
typedef int (*fcfg_rd_cbk_t)(unsigned long      address,
                             unsigned char      *pData,
                             unsigned long      numBytes);

/*
 *   WRITE CALLBACK - callback to use for writing to SPI Flash.
 */
typedef int (*fcfg_wr_cbk_t)(unsigned long      address,
                             unsigned char      *pData,
                             unsigned long      numBytes,
                             unsigned long      eraseSector);

/*
 *   Malloc CALLBACK - callback used to alloc dynamic memory by the lib.
 */
typedef void *(*fcfg_malloc_t)(unsigned long size);

/*
 *   Free CALLBACK - callback used to free memory by the lib.
 */
typedef unsigned long (*fcfg_free_t)(void *pAddr);

/*
 *   HANDLE
 */
typedef struct fcfg_handle_type
{
    fcfg_addr_t     addr;
    fcfg_rd_cbk_t   rd_cbk;
    fcfg_wr_cbk_t   wr_cbk;
    fcfg_malloc_t   malloc;
    fcfg_free_t     free;
    
    unsigned char   silego;
    unsigned char   led;
    unsigned char   rfswitch;
    unsigned char   board;
    
}fcfg_handle_t;


////////////////////////////////////////////////////////////////////////////////
// Public Declarations
////////////////////////////////////////////////////////////////////////////////
fcfg_err_t FCFG_get(fcfg_handle_t *pHandle, fcfg_tag_t tag, fcfg_pval_t pValue) __attribute__ ((section (FCFG_TXT_SEC)));
fcfg_err_t FCFG_set(fcfg_handle_t *pHandle, fcfg_tag_t tag, fcfg_len_t len, fcfg_pval_t pValue) __attribute__ ((section (FCFG_TXT_SEC)));
fcfg_err_t FCFG_getlen(fcfg_handle_t *pHandle, fcfg_tag_t tag, fcfg_len_t *pLen) __attribute__ ((section (FCFG_TXT_SEC)));
fcfg_err_t FCFG_clear(fcfg_handle_t *pHandle, fcfg_tag_t tag) __attribute__ ((section (FCFG_TXT_SEC)));
fcfg_err_t FCFG_clearall(fcfg_handle_t *pHandle) __attribute__ ((section (FCFG_TXT_SEC)));
fcfg_err_t FCFG_getdescriptor(fcfg_tag_t tag, char **ppDesc) __attribute__ ((section (FCFG_TXT_SEC)));
fcfg_err_t FCFG_gettagfromdescriptor(char *pDescriptor, fcfg_tag_t *pTag) __attribute__ ((section (FCFG_TXT_SEC)));
fcfg_err_t FCFG_init(fcfg_handle_t *pHandle,
                     fcfg_addr_t   addr,
                     fcfg_rd_cbk_t rd_cbk,
                     fcfg_wr_cbk_t wr_cbk,
                     fcfg_malloc_t malloc,
                     fcfg_free_t   free) __attribute__ ((section (FCFG_TXT_SEC)));
fcfg_rfswitch_t FCFG_rfswitch(fcfg_handle_t *pHandle) __attribute__ ((section (FCFG_TXT_SEC)));
fcfg_board_t    FCFG_board(fcfg_handle_t *pHandle) __attribute__ ((section (FCFG_TXT_SEC)));
fcfg_silego_t   FCFG_silego(fcfg_handle_t *pHandle) __attribute__ ((section (FCFG_TXT_SEC)));
fcfg_led_t      FCFG_led(fcfg_handle_t *pHandle) __attribute__ ((section (FCFG_TXT_SEC)));

#endif /* CFG_FACTORY_H_ */
