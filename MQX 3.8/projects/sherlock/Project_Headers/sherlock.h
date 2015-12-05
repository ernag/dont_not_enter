/*
 * sherlock.h
 * 
 * Sherlock is the lovable debug logger.  It helps you log text into SPI Flash,
 *   find it later, and dump it out at your nearest convenience.
 *
 *  Created on: Jul 12, 2013
 *      Author: Ernie
 */

#ifndef SHERLOCK_H_
#define SHERLOCK_H_

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Defines
////////////////////////////////////////////////////////////////////////////////

/*
 *   Applications must include these sections in their linker files.
 */
#define SHLK_TXT_SEC        ".text.SHLK"


#define CC_MAN_SHERLOCK "\
Sherlock - Log Detective\n\n\
sher on\n\
sher off\n\
sher serv <pages>\n\
sher reset\n\
sher assdump\n\
sher dump\n\n"


////////////////////////////////////////////////////////////////////////////////
// Typedefs
////////////////////////////////////////////////////////////////////////////////

typedef unsigned long  sh_u32_t;
typedef unsigned short sh_u16_t;
typedef unsigned char  sh_u8_t;

/*
 *    ERROR Codes for the Sherlock lib.
 */
typedef enum shlk_error_type
{
    SHLK_ERR_OK           = 0,
    SHLK_ERR_FLASH_READ   = 1,
    SHLK_ERR_FLASH_WRITE  = 2,
    SHLK_ERR_ASSLOG       = 3

} shlk_err_t;

/*
 *   READ CALLBACK - callback to use for reading from SPI Flash.
 */
typedef int (*shlk_rd_cbk_t)(sh_u32_t      address,
                             sh_u8_t       *pData,
                             sh_u32_t      numBytes);

/*
 *   WRITE CALLBACK - callback to use for writing to SPI Flash.
 */
typedef int (*shlk_wr_cbk_t)(sh_u32_t     address,
                             sh_u8_t      *pData,
                             sh_u32_t     numBytes,
                             sh_u32_t     eraseSector);

/*
 *   PRINT CALLBACK - callback for sending ascii text to the client.
 */
typedef void (*shlk_pr_cbk_t)(const char *pStr, sh_u32_t len);

/*
 *   TIME CALLBACK - callback for getting time (or arbitrary) string from clients.
 */
typedef sh_u8_t (*shlk_time_cbk_t)(char *pStr, sh_u32_t max_len);

/*
 *   HANDLE
 */
typedef struct shlk_handle_type
{
    shlk_rd_cbk_t   rd_cbk;
    shlk_wr_cbk_t   wr_cbk;
    shlk_pr_cbk_t   pr_cbk;
    sh_u32_t        curr_log_addr;
    sh_u32_t        asslog_start_addr;  // assumed a 4KB sector address, and it will use 2 consecutive (8KB total)
    sh_u32_t        log_start_addr;
    sh_u32_t        newest_sect_count;
    sh_u8_t         log_num_sect;
    sh_u8_t         is_valid;
    shlk_time_cbk_t time_cbk;
    sh_u8_t         num_pgs_between_time_write;
}shlk_handle_t;

////////////////////////////////////////////////////////////////////////////////
// Public Declarations
////////////////////////////////////////////////////////////////////////////////

shlk_err_t SHLK_init(shlk_handle_t *pHandle,
                     shlk_rd_cbk_t rd_cbk,
                     shlk_wr_cbk_t wr_cbk,
                     shlk_pr_cbk_t pr_cbk,
                     sh_u32_t      asslog_start_addr,
                     sh_u32_t      log_start_addr,
                     sh_u8_t       log_num_sect) __attribute__ ((section (SHLK_TXT_SEC)));
shlk_err_t SHLK_reset(shlk_handle_t *pHandle) __attribute__ ((section (SHLK_TXT_SEC)));
shlk_err_t SHLK_log(shlk_handle_t *pHandle, const char *pStr, sh_u32_t len) __attribute__ ((section (SHLK_TXT_SEC)));
shlk_err_t SHLK_dump(shlk_handle_t *pHandle, sh_u32_t num_pages) __attribute__ ((section (SHLK_TXT_SEC)));
shlk_err_t SHLK_assdump(shlk_handle_t *pHandle) __attribute__ ((section (SHLK_TXT_SEC)));
shlk_err_t SHLK_asslog(shlk_handle_t *pHandle, const char *pStr) __attribute__ ((section (SHLK_TXT_SEC)));
shlk_err_t SHLK_enable(shlk_handle_t *pHandle, sh_u32_t en_addr, sh_u8_t enable_sherlock) __attribute__ ((section (SHLK_TXT_SEC)));
sh_u8_t    SHLK_is_enabled(shlk_handle_t *pHandle, sh_u32_t en_addr) __attribute__ ((section (SHLK_TXT_SEC)));
sh_u32_t   SHLK_serv_pages(shlk_handle_t *pHandle, sh_u32_t en_addr, sh_u32_t num_pages, sh_u8_t read_only) __attribute__ ((section (SHLK_TXT_SEC)));
void       SHLK_set_time_cbk(shlk_handle_t *pHandle, shlk_time_cbk_t pCbk, sh_u8_t num_pgs_between_time_write) __attribute__ ((section (SHLK_TXT_SEC)));

#endif /* SHERLOCK_H_ */
