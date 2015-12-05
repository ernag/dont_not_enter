/*
 * sherlock.c
 *
 *  Created on: Jul 12, 2013
 *      Author: Ernie
 */

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include <string.h>
#include "sherlock.h"

////////////////////////////////////////////////////////////////////////////////
// Defines
////////////////////////////////////////////////////////////////////////////////
#define SHLK_MAGIC_NUMBER               (0xBA550000 + 14)   // increment counter when doing any updates to Sherlock driver.
#define SHLK_DEFAULT_ENABLE             (sh_u8_t)0x01
#define SHLK_ENABLE_MASK                0x000000FF
#define ALL_PAGES_4KB_UNUSED            0x0000FFFE   // the header at least (first 256 bytes) is always in use.
#define EXT_FLASH_PAGE_SIZE             256
#define EXT_FLASH_4KB_SZ                0x1000  //4096   // First 32 sectors are 4KB large.
#define EXT_FLASH_64KB_SZ               0x10000 //65536  // Last 510 sectors are 64KB large.
#define EXT_FLASH_64KB_REGION           0x20000
#define EXT_FLASH_NUM_PGS_4KB           ( EXT_FLASH_4KB_SZ / EXT_FLASH_PAGE_SIZE )
#define EXT_FLASH_NUM_PGS_64KB          ( EXT_FLASH_64KB_SZ / EXT_FLASH_PAGE_SIZE )
#define UNUSED_ADDR                     (sh_u8_t)0xFF    // Erased sector has a byte value of 0xFF, assume bogus ASCii value.
#define FIRST_UNSUPPORTED_ASCII_CHAR    (sh_u8_t)0x7F
#define ASSDUMP_DONE                    "\n\rassdump: done\n\r\n\0"
#define ASSDUMP_DONE_LEN                strlen(ASSDUMP_DONE)
#define NUM_PAGE_BITFIELDS              ( sizeof(pg_usd_bf_t) / sizeof(sh_u32_t) )
#define ALL_PAGES_FULL                  ( NUM_PAGE_BITFIELDS * 32 )

//#define  SHDEBUG    1
#if      SHDEBUG
    #define D(x)    x
    #include <stdio.h>
#else
    #define D(x)
#endif


////////////////////////////////////////////////////////////////////////////////
// Typedefs
////////////////////////////////////////////////////////////////////////////////

/*
 *   Bitfield for which pages within a log sector are used.
 *   There are 256 pages in 1 sector, so we need 256 bits, or 8 * u32's.
 *   NOTE:  first page of every sector is always just for the header.
 */
typedef struct pages_used_bitfield_type
{
    sh_u32_t pgs_0;
    sh_u32_t pgs_1;
    sh_u32_t pgs_2;
    sh_u32_t pgs_3;
    sh_u32_t pgs_4;
    sh_u32_t pgs_5;
    sh_u32_t pgs_6;
    sh_u32_t pgs_7;
}pg_usd_bf_t;

/*
 *   Structure representing the header for an assertion dump
 */
typedef struct shlk_assdump_header_type
{
    sh_u32_t pages_used;  // which pages are used.
    sh_u32_t addr;        // address of this sector.
    sh_u32_t pp_count;    // ping pong count, which sector is newer.
    sh_u32_t magic;       // is the sector valid?

}shlk_ad_hdr_t;


/*
 *   Header for a log's sector
 */
typedef struct shlk_log_header_type
{
    pg_usd_bf_t used;       // which pages are used.  NOTE:  do NOT move this!
    sh_u32_t    addr;       // address of this sector.
    sh_u32_t    sec_count;  // sector count, so we know which sector is the most recent.
    sh_u32_t    magic;      // is the sector valid?

}shlk_lg_hdr_t;

/*
 *   Header for enable/disable sector.
 */
typedef struct shlk_enable_header_type
{
    sh_u32_t    is_enabled;
    sh_u32_t    reserved;
    sh_u32_t    magic;
}shlk_en_hdr_t;

/*
 *   Header for num pages pending sector.
 */
typedef struct shlk_serv_header_type
{
    sh_u32_t    num_pages;
    sh_u32_t    reserved;
    sh_u32_t    magic;
}shlk_serv_hdr_t;

////////////////////////////////////////////////////////////////////////////////
// Private Declarations
////////////////////////////////////////////////////////////////////////////////
static shlk_err_t _SHLK_get_asslog_addr(shlk_handle_t *pHandle, sh_u32_t *pAddr) __attribute__ ((section (SHLK_TXT_SEC)));
static shlk_err_t _SHLK_init_log_wr_addr(shlk_handle_t *pHandle) __attribute__ ((section (SHLK_TXT_SEC)));
static sh_u8_t _SHLK_get_next_asslog_loc(shlk_handle_t *pHandle, shlk_ad_hdr_t *pHdr, sh_u32_t *pAddr) __attribute__ ((section (SHLK_TXT_SEC)));
static sh_u32_t _SHLK_get_next_unused_page(sh_u32_t pages_used) __attribute__ ((section (SHLK_TXT_SEC)));
static inline void _SHLK_mark_page_used(sh_u32_t *pPagesUsed, sh_u32_t page_num) __attribute__ ((section (SHLK_TXT_SEC)));
static inline sh_u32_t _SHLK_get_log_addr_from_idx(sh_u32_t addr, sh_u32_t pg_idx) __attribute__ ((section (SHLK_TXT_SEC)));
static sh_u32_t _SHLK_get_used_pages(sh_u32_t pages_used, sh_u8_t max_pages) __attribute__ ((section (SHLK_TXT_SEC)));
static shlk_err_t _SHLK_assdump_sector(shlk_handle_t *pHandle, sh_u32_t addr) __attribute__ ((section (SHLK_TXT_SEC)));
static int _SHLK_ad_read_hdr(shlk_handle_t *pHandle, sh_u32_t addr, shlk_ad_hdr_t *pHdr) __attribute__ ((section (SHLK_TXT_SEC)));
static sh_u32_t _SHLK_null_term(sh_u8_t *buf, const sh_u32_t max_len) __attribute__ ((section (SHLK_TXT_SEC)));
static void _SHLK_print(shlk_handle_t *pHandle, char *pBuf, const sh_u32_t max_len) __attribute__ ((section (SHLK_TXT_SEC)));
static shlk_err_t _SHLK_init_log_sector(shlk_handle_t *pHandle, 
                                        sh_u32_t addr, 
                                        shlk_lg_hdr_t *pHdr,
                                        sh_u32_t sec_count,
                                        pg_usd_bf_t *pUsed) __attribute__ ((section (SHLK_TXT_SEC)));
static sh_u32_t _SHLK_get_first_unused_page_idx(shlk_handle_t *pHandle, sh_u32_t *pBitfield) __attribute__ ((section (SHLK_TXT_SEC)));
static sh_u32_t _SHLK_crosses_sect_boundary(shlk_handle_t *pHandle, sh_u32_t len) __attribute__ ((section (SHLK_TXT_SEC)));
static sh_u32_t _SHLK_num_bytes_to_next_page(sh_u32_t addr) __attribute__ ((section (SHLK_TXT_SEC)));
static sh_u32_t _get_sector_addr(sh_u32_t address, sh_u32_t sectorSz, sh_u32_t regionStart) __attribute__ ((section (SHLK_TXT_SEC)));
static sh_u32_t _SHLK_get_next_sect(shlk_handle_t *pHandle) __attribute__ ((section (SHLK_TXT_SEC)));
static sh_u32_t _SHLK_log_time(shlk_handle_t *pHandle) __attribute__ ((section (SHLK_TXT_SEC)));
static shlk_err_t _SHLK_write_log_data(shlk_handle_t *pHandle, sh_u8_t *pData, sh_u32_t len) __attribute__ ((section (SHLK_TXT_SEC)));
static int _SHLK_update_bf(shlk_handle_t *pHandle, sh_u32_t bytes_to_write) __attribute__ ((section (SHLK_TXT_SEC)));
static void _SHLK_update_pgs_used(sh_u32_t *pUsed, sh_u32_t num_pgs_used) __attribute__ ((section (SHLK_TXT_SEC)));
static shlk_err_t _SHLK_dump_sector(shlk_handle_t *pHandle, sh_u32_t addr) __attribute__ ((section (SHLK_TXT_SEC)));
static shlk_err_t _SHLK_dump_recent_pages(shlk_handle_t *pHandle, sh_u32_t num_pages) __attribute__ ((section (SHLK_TXT_SEC)));
static shlk_err_t _SHLK_final_write_check(shlk_handle_t *pHandle, sh_u32_t len) __attribute__ ((section (SHLK_TXT_SEC)));

////////////////////////////////////////////////////////////////////////////////
// Code
////////////////////////////////////////////////////////////////////////////////

/*
 *   Returns the next un-used page in the pages-used bitfield.
 */
static sh_u32_t _SHLK_get_next_unused_page(sh_u32_t pages_used)
{
    sh_u32_t unused_page_idx = 0;
    
    while( pages_used != (pages_used | (sh_u32_t)(0x00000001 << unused_page_idx)) )
    {
        if(unused_page_idx < 32)
        {
            unused_page_idx++;
        }
        else
        {
            break;
        }
    }
    
    D( printf("unused_page_idx = %i\n\r", unused_page_idx); fflush(stdout); )
    return unused_page_idx;
}

/*
 *   Updates the pages-used field with the page number passed in.
 */
static inline void _SHLK_mark_page_used(sh_u32_t *pPagesUsed, sh_u32_t page_num)
{
    *pPagesUsed &= ~( (sh_u32_t)(0x00000001 << page_num) );
}

/*
 *   Returns the number of used pages for this assdump sector.
 */
static sh_u32_t _SHLK_get_used_pages(sh_u32_t pages_used, sh_u8_t max_pages)
{
    sh_u32_t num_used = 0;
    sh_u8_t  count = 0;
    
    while(count < max_pages)
    {
        num_used += (pages_used & (0x00000001 << count)) ? 0:1;
        count++;
    }
    
    return num_used;
}

/*
 *   Return pAddr, next asslog specific address to use.
 *   Erase/init 4KB sector if necessary.
 *   Returns 0 for success, non-zero otherwise.
 */
static sh_u8_t _SHLK_get_next_asslog_loc(shlk_handle_t *pHandle, shlk_ad_hdr_t *pHdr, sh_u32_t *pAddr)
{
    int wr_err;
    
    /*
     *   If this is the first time we've ever written to this sector, we need to initialize it.
     */
    if(ALL_PAGES_4KB_UNUSED == pHdr->pages_used)
    {
        _SHLK_mark_page_used(&(pHdr->pages_used), 1);
        
        wr_err = pHandle->wr_cbk(pHdr->addr, (sh_u8_t *)pHdr, sizeof(shlk_ad_hdr_t), 1);
        if( wr_err )
        {
            D(printf("shlk wr hdr err %i @ 0x%x\n\r", wr_err, pHdr->addr); fflush(stdout);)
            return 1;
        }
        
        *pAddr = pHdr->addr + EXT_FLASH_PAGE_SIZE;
    }
    else
    {
        sh_u32_t page_num;
        
        /*
         *   Check to make sure overflow would not occur by writing this sector.
         */
        if(0 == pHdr->pages_used)
        {
            return 2;  // all the pages are used up now.
        }
        
        /*
         *   Update to the next page.
         */
        page_num = _SHLK_get_next_unused_page(pHdr->pages_used);
        if( (0 == page_num) || (page_num > EXT_FLASH_NUM_PGS_4KB) )
        {
            return 3;
        }
        
        _SHLK_mark_page_used(&(pHdr->pages_used), page_num);
        
        wr_err = pHandle->wr_cbk(pHdr->addr, (sh_u8_t *)pHdr, sizeof(pHdr->pages_used), 0);
        if( wr_err )
        {
            D(printf("shlk wr pg usd err %i @ 0x%x\n\r", wr_err, pHdr->addr); fflush(stdout);)
            return 4;
        }
        
        *pAddr = pHdr->addr + (page_num * EXT_FLASH_PAGE_SIZE);
    }
    
    return 0;
}

/*
 *   Read an assdump header at the provided 4KB sector address.
 */
static int _SHLK_ad_read_hdr(shlk_handle_t *pHandle, sh_u32_t addr, shlk_ad_hdr_t *pHdr)
{
    int rd_err;
    
    rd_err = pHandle->rd_cbk( addr, (sh_u8_t *)pHdr, (sh_u32_t)sizeof(shlk_ad_hdr_t));
    
    D
    (
        if( rd_err )
        {
            D(printf("shlk read err %i ad hdr @0x%x\n", rd_err, addr); fflush(stdout));
        }
    )
    
    return rd_err;
}


/*
 *   Returns the next address we should write to in the assertion stream.
 *   
 *     If needed, we will erase/initialize the next 4KB sector in the ping pong
 *       with a shiny new header.
 */
static shlk_err_t _SHLK_get_asslog_addr(shlk_handle_t *pHandle, sh_u32_t *pAddr)
{
    shlk_ad_hdr_t hdr1, hdr2;
    shlk_ad_hdr_t *pHdr, *pHdrStratch;
    int rd_err;
    
    /*
     *   Read the headers for the two 4KB sectors.
     */
    if( _SHLK_ad_read_hdr(pHandle, pHandle->asslog_start_addr, &hdr1) )
    {
        return SHLK_ERR_FLASH_READ;
    }
    
    if( _SHLK_ad_read_hdr(pHandle, pHandle->asslog_start_addr + EXT_FLASH_4KB_SZ, &hdr2) )
    {
        return SHLK_ERR_FLASH_READ;
    }
    
    /*
     *   Figure out which sector is the one we should use.
     *   
     *   CASE 1:  Both 4KB sectors have good header.
     */
    if( (SHLK_MAGIC_NUMBER == hdr1.magic) && (SHLK_MAGIC_NUMBER == hdr2.magic) )
    {
        if(hdr1.pp_count > hdr2.pp_count)
        {
            pHdr = &hdr1;
            pHdrStratch = &hdr2;
        }
        else
        {
            pHdr = &hdr2;
            pHdrStratch = &hdr1;
        }
    }
    /*
     *   CASE 2:  hdr 1 good, hdr 2 invalid.
     */
    else if( (SHLK_MAGIC_NUMBER == hdr1.magic) && (SHLK_MAGIC_NUMBER != hdr2.magic) )
    {
        pHdr = &hdr1;
        pHdrStratch = &hdr2;
    }
    /*
     *   CASE 3:  hdr 1 invalid, hdr 2 good.
     */
    else if( (SHLK_MAGIC_NUMBER != hdr1.magic) && (SHLK_MAGIC_NUMBER == hdr2.magic) )
    {
        pHdr = &hdr2;
        pHdrStratch = &hdr1;
    }
    /*
     *   CASE 4:  Both headers are invalid, this should only happen once.
     *            Just pick the first header.
     */
    else
    {
        hdr1.addr  = pHandle->asslog_start_addr;
        hdr1.magic = SHLK_MAGIC_NUMBER;
        hdr1.pages_used = ALL_PAGES_4KB_UNUSED;
        hdr1.pp_count = 0;
        
        pHdr = &hdr1;
        pHdrStratch = &hdr2;
    }
    
    /*
     *   We now know the 4KB sector to use (assuming it doesn't overflow).
     */
    if( _SHLK_get_next_asslog_loc(pHandle, pHdr, pAddr) )
    {
        /*
         *   Apparently, this will cause data overflow of the sector, so use the other header instead.
         */
        pHdrStratch->addr = (pHdr->addr == pHandle->asslog_start_addr) ? (pHandle->asslog_start_addr + EXT_FLASH_4KB_SZ) : pHandle->asslog_start_addr;
        pHdrStratch->magic = SHLK_MAGIC_NUMBER;
        pHdrStratch->pages_used = ALL_PAGES_4KB_UNUSED;
        pHdrStratch->pp_count = pHdr->pp_count + 1;
        
        /*
         *   Try again to get the assdump location, but with this new header.
         */
        if( _SHLK_get_next_asslog_loc(pHandle, pHdrStratch, pAddr) )
        {
            return SHLK_ERR_ASSLOG;
        }
    }
    
    return SHLK_ERR_OK;
}

/*
 *   Init a sector for usage by the Sherlock logging system, for normal logs.
 */
static shlk_err_t _SHLK_init_log_sector(shlk_handle_t *pHandle, 
                                        sh_u32_t addr, 
                                        shlk_lg_hdr_t *pHdr,
                                        sh_u32_t sec_count,
                                        pg_usd_bf_t *pUsed)
{
    pHdr->addr = addr;
    pHdr->magic = SHLK_MAGIC_NUMBER;
    pHdr->sec_count = sec_count;
    if(!pUsed)
    {
        memset(&(pHdr->used), 0xFFFFFFFF, sizeof(pg_usd_bf_t));
        pHdr->used.pgs_0 = 0xFFFFFFFE; // header is used.
    }
    else
    {
        memcpy(&(pHdr->used), pUsed, sizeof(pg_usd_bf_t));
    }
    
    if( pHandle->wr_cbk(addr, (sh_u8_t *)pHdr, sizeof(shlk_lg_hdr_t), 1) )
    {
        return SHLK_ERR_FLASH_WRITE;
    }
    
    pHandle->newest_sect_count = sec_count;
    pHandle->curr_log_addr = addr + EXT_FLASH_PAGE_SIZE; // skip the header.
    
    return SHLK_ERR_OK;
}

static sh_u32_t _SHLK_get_first_unused_page_idx(shlk_handle_t *pHandle, sh_u32_t *pBitfield)
{
    sh_u32_t unused_idx = 1;
    sh_u32_t pg_bf_count = 0;
    
    while(pg_bf_count < NUM_PAGE_BITFIELDS)
    {
        sh_u32_t bit = 0;
        
        if(0 == pg_bf_count)
        {
            bit = 1; // skip the header.
        }
        
        while(bit < 32)
        {
            if( *pBitfield & (0x00000001 << bit) )  // unused = 1, used = 0
            {
                return unused_idx;
            }
            bit++;
            unused_idx++;
        }
        
        pBitfield++;
        pg_bf_count++;
    }
    
    return ALL_PAGES_FULL;
}

/*
 *   Return address corresponding to a page index.
 */
static inline sh_u32_t _SHLK_get_log_addr_from_idx(sh_u32_t addr, sh_u32_t pg_idx)
{
    return (addr + (pg_idx * EXT_FLASH_PAGE_SIZE));
}

/*
 *   Init the cached address that we want to write to when we log data.
 */
static shlk_err_t _SHLK_init_log_wr_addr(shlk_handle_t *pHandle)
{
    shlk_lg_hdr_t hdr;
    sh_u32_t newest_sec_count = 0;
    sh_u32_t oldest_sec_count = 0xFFFFFFFF;
    sh_u32_t oldest_sector = pHandle->log_start_addr;
    
    pHandle->curr_log_addr = pHandle->log_start_addr;
    
    /*
     *   First get the sector we should start writing to.
     */
    while(pHandle->curr_log_addr < (pHandle->log_start_addr + pHandle->log_num_sect*EXT_FLASH_64KB_SZ))
    {
        if( pHandle->rd_cbk( pHandle->curr_log_addr, (sh_u8_t *)&hdr, (sh_u32_t)sizeof(shlk_lg_hdr_t)) )
        {
            return SHLK_ERR_FLASH_READ;
        }
        
        if(hdr.magic != SHLK_MAGIC_NUMBER)
        {
            return _SHLK_init_log_sector(pHandle, pHandle->curr_log_addr, &hdr, newest_sec_count+1, NULL);  // this is the sector we should start writing to, write at the top.
        }
        else
        {
            sh_u32_t idx;
            
            /*
             *   Check to see if there are any spare pages available in this sector.
             */
            idx = _SHLK_get_first_unused_page_idx(pHandle, (sh_u32_t *)&(hdr.used));
            if( ALL_PAGES_FULL != idx )
            {
                pHandle->curr_log_addr = _SHLK_get_log_addr_from_idx(pHandle->curr_log_addr, idx);
                return SHLK_ERR_OK;
            }
            
            /*
             *   Update sector counts, so we can use them later if needed.
             */
            if(hdr.sec_count < oldest_sec_count)
            {
                oldest_sec_count = hdr.sec_count;
                oldest_sector = hdr.addr;           
            }
            
            if(hdr.sec_count > newest_sec_count)
            {
                newest_sec_count = hdr.sec_count;
            }
        }
        
        pHandle->curr_log_addr += EXT_FLASH_64KB_SZ;
    }
    
    /*
     *     If we got here, it means that all sectors are full, and we should erase/use the oldest one.
     */
    pHandle->curr_log_addr = oldest_sector;
    return _SHLK_init_log_sector(pHandle, pHandle->curr_log_addr, &hdr, newest_sec_count+1, NULL);
}

static sh_u32_t _get_sector_addr(sh_u32_t address, sh_u32_t sectorSz, sh_u32_t regionStart)
{
    sh_u32_t remainder = (address - regionStart) % sectorSz;

    return address - remainder;
}

/*
 *   Return 0 if we would not cross a sector boundary.
 *     Otherwise, return number of bytes until the next sector boundary.
 */
static sh_u32_t _SHLK_crosses_sect_boundary(shlk_handle_t *pHandle, sh_u32_t len)
{
    sh_u32_t bytes_to_next_sector;
    sh_u32_t this_sector;

    this_sector = _get_sector_addr(pHandle->curr_log_addr, EXT_FLASH_64KB_SZ, EXT_FLASH_64KB_REGION);
    bytes_to_next_sector = EXT_FLASH_64KB_SZ - (pHandle->curr_log_addr - this_sector);

    if(len > bytes_to_next_sector)
    {
        return bytes_to_next_sector;
    }
    else
    {
        return 0;
    }
}

/*
 *   Get the next sector to log data to.
 */
static sh_u32_t _SHLK_get_next_sect(shlk_handle_t *pHandle)
{
    sh_u32_t next_sect;
    
    next_sect = _get_sector_addr(pHandle->curr_log_addr, EXT_FLASH_64KB_SZ, EXT_FLASH_64KB_REGION) + EXT_FLASH_64KB_SZ;
    
    if(next_sect >= (pHandle->log_start_addr + (pHandle->log_num_sect * EXT_FLASH_64KB_SZ)))
    {
        return pHandle->log_start_addr;
    }
    
    return next_sect;
}

/*
 *   Update the pages-used structure.
 */
static void _SHLK_update_pgs_used(sh_u32_t *pUsed, sh_u32_t num_pgs_used)
{
    
    sh_u32_t count = 0;
    
    memset(pUsed, 0xFFFFFFFF, sizeof(pg_usd_bf_t));
    pUsed[0] = 0xFFFFFFFE; // header is used always.
    
    /*
     *   First clear whole pages if number of pages to clear > 32.
     */
    while( count < (num_pgs_used / 32) )
    {
        pUsed[count] = 0;
        count++;
    }
    
    /*
     *   Now we should be left with the modulus, which we can use on the final page.
     */
    pUsed += count;
    count = 0;
    
    while( count < (num_pgs_used % 32) )
    {
        *pUsed &= ~(0x00000001 << count);
        count++;
    }
}

/*
 *   Write out the bitfield.
 */
static int _SHLK_update_bf(shlk_handle_t *pHandle, sh_u32_t bytes_to_write)
{
    sh_u32_t sect_addr;
    sh_u32_t num_bits_to_clear;
    pg_usd_bf_t used;
    
    sect_addr = _get_sector_addr(pHandle->curr_log_addr, EXT_FLASH_64KB_SZ, EXT_FLASH_64KB_REGION);
    
    num_bits_to_clear = (pHandle->curr_log_addr - sect_addr + bytes_to_write ) % EXT_FLASH_64KB_SZ;
    num_bits_to_clear = (num_bits_to_clear / EXT_FLASH_PAGE_SIZE);
    if(num_bits_to_clear % EXT_FLASH_PAGE_SIZE)
    {
        num_bits_to_clear++;
    }
    
    _SHLK_update_pgs_used((sh_u32_t *)&used, num_bits_to_clear);
    
    /*
     *   Write out the used fields (the used ones will be marked with a ZERO for their bit).
     */
    if( pHandle->wr_cbk(sect_addr, (sh_u8_t *)&used, (sh_u32_t)sizeof(pg_usd_bf_t), 0) )
    {
        return 1;
    }
    
    return 0;
}

/*
 *   Get number of bytes left in this page.
 */
static sh_u32_t _SHLK_num_bytes_to_next_page(sh_u32_t addr)
{
	return ( EXT_FLASH_PAGE_SIZE - (addr % EXT_FLASH_PAGE_SIZE) );
}

/*
 *   Log the time to FLASH.
 */
static sh_u32_t _SHLK_log_time(shlk_handle_t *pHandle)
{
	char     str_buf[32];
	sh_u32_t num_bytes_written;
	
	num_bytes_written = (sh_u32_t)pHandle->time_cbk(str_buf, 32);
	
    if( pHandle->wr_cbk(pHandle->curr_log_addr, (unsigned char *)str_buf, num_bytes_written, 0) )
    {
        return 0;
    }
	
	return num_bytes_written;
}

/*
 *   Write data out to SPI Flash, update address, and update pages-used bitfield.
 */
static shlk_err_t _SHLK_write_log_data(shlk_handle_t *pHandle, sh_u8_t *pData, sh_u32_t len)
{   
    shlk_err_t err;
    sh_u32_t num_bytes_to_next_page;
    sh_u32_t num_bytes_to_write = len;
    
    err = _SHLK_final_write_check(pHandle, len);
    if(SHLK_ERR_OK != err)
    {
        return err;
    }
    
    if( _SHLK_update_bf(pHandle, len) )
    {
        return SHLK_ERR_FLASH_WRITE;
    }
    
    num_bytes_to_next_page = _SHLK_num_bytes_to_next_page(pHandle->curr_log_addr);
    
    /*
     *   We can't write through a page boundary, so fragment it if needed.
     */
    if(num_bytes_to_next_page < len)
    {
        if( pHandle->wr_cbk(pHandle->curr_log_addr, pData, num_bytes_to_next_page, 0) )
        {
            return SHLK_ERR_FLASH_WRITE;
        }
        
        pHandle->curr_log_addr += num_bytes_to_next_page;
        pData += num_bytes_to_next_page;
        num_bytes_to_write -= num_bytes_to_next_page;
    }
    
    /*
     *   curr_log_addr needs to be a page boundary by this point!
     *   Let's see if we want to put a time-string at the top of this page.
     */
	if( 0 == (pHandle->curr_log_addr % (pHandle->num_pgs_between_time_write * EXT_FLASH_PAGE_SIZE)) )
	{
		pHandle->curr_log_addr += _SHLK_log_time(pHandle);
	}
    
	/*
	 *   Write the rest (or all) of the data out now.
	 */
    if( pHandle->wr_cbk(pHandle->curr_log_addr, pData, num_bytes_to_write, 0) )
    {
        return SHLK_ERR_FLASH_WRITE;
    }
    
    pHandle->curr_log_addr += num_bytes_to_write;
    return SHLK_ERR_OK;
}

/* 
 *   Double check to make sure that the write won't go beyond the
 *      allocated space. Shift back to beginning of log space.
 */
static shlk_err_t _SHLK_final_write_check(shlk_handle_t *pHandle, sh_u32_t len)
{   
    sh_u32_t end_addr;
    shlk_err_t err;
    shlk_lg_hdr_t hdr;
    
    end_addr = pHandle->curr_log_addr + len;
    if( end_addr >= (pHandle->log_start_addr + (pHandle->log_num_sect * EXT_FLASH_64KB_SZ)) )
    {
        pHandle->curr_log_addr = pHandle->log_start_addr;

        err = _SHLK_init_log_sector(pHandle, pHandle->curr_log_addr, &hdr, pHandle->newest_sect_count+1, NULL);
        if(SHLK_ERR_OK != err)
        {
            return err;
        }
    }
    
    return SHLK_ERR_OK;
}

/*
 *   Log data to the "normal" log stream, which exist in a circular buffer made
 *      up of 'N' 64KB SPI Flash sectors.
 */
shlk_err_t SHLK_log(shlk_handle_t *pHandle, const char *pStr, sh_u32_t len)
{
    shlk_err_t err;
    sh_u32_t bytes_to_next_sect_boundary;
    shlk_lg_hdr_t hdr;
    
#if 0
    // echo test to make sure we are being passed the correct strings.
    pHandle->pr_cbk(pStr, len);
    return SHLK_ERR_OK;
#endif
    
    /*
     *   If the handle hasn't been initialized with a log address yet, init it.
     */
    if(!pHandle->is_valid)
    {
        err = _SHLK_init_log_wr_addr(pHandle);
        if(SHLK_ERR_OK != err)
        {
            return err;
        }
        pHandle->is_valid = 1;
    }
    
    /*
     *   Log the data to SPI Flash.
     *     - make sure that if we cross a sector boundary, we erase the next sector first.
     *     // TODO:  think about duplication here, what could be better organized?
     */
    bytes_to_next_sect_boundary = _SHLK_crosses_sect_boundary(pHandle, len);
    if( bytes_to_next_sect_boundary )
    {
        // crossing a sector boundary here.
        // TODO:  make sure this happens properly, and is ok for the wrap around case!
        len -= bytes_to_next_sect_boundary;
        if( pHandle->wr_cbk(pHandle->curr_log_addr, (sh_u8_t *)pStr, bytes_to_next_sect_boundary, 0) )
        {
            return SHLK_ERR_FLASH_WRITE;
        }
        pStr += bytes_to_next_sect_boundary;
        _SHLK_update_bf(pHandle, bytes_to_next_sect_boundary);
        pHandle->curr_log_addr = _SHLK_get_next_sect(pHandle);
        
        err = _SHLK_init_log_sector(pHandle, pHandle->curr_log_addr, &hdr, pHandle->newest_sect_count+1, NULL);
        if(SHLK_ERR_OK != err)
        {
            return err;
        }
        
        err = _SHLK_write_log_data(pHandle, (sh_u8_t *)pStr, len);
        if(SHLK_ERR_OK != err)
        {
            return err;
        }
    }
    else
    {
        /*
         *   This write will not cross a sector boundary, but let's check to 
         *      make sure we aren't right on a sector boundary.
         */
        if(0 == (pHandle->curr_log_addr % EXT_FLASH_64KB_SZ))
        {
            /*
             *   Rare case that we are right on a sector boundary.
             */
            
            /*   Rarer case that we are right on the sector boundary of the end of
             *      all of the sectors allocated to Sherlock.
             */
            if((pHandle->log_start_addr + (pHandle->log_num_sect * EXT_FLASH_64KB_SZ)) == pHandle->curr_log_addr)
            {
                pHandle->curr_log_addr = pHandle->log_start_addr;
            }
            
            err = _SHLK_init_log_sector(pHandle, pHandle->curr_log_addr, &hdr, pHandle->newest_sect_count+1, NULL);
            if(SHLK_ERR_OK != err)
            {
                return err;
            }
        }
        
        err = _SHLK_write_log_data(pHandle, (sh_u8_t *)pStr, len);
        if(SHLK_ERR_OK != err)
        {
            return err;
        }
    }

    return SHLK_ERR_OK;
}

/*
 *   NULL terminate string, either based on UNUSED byte, or zero.
 */
static sh_u32_t _SHLK_null_term(sh_u8_t *buf, const sh_u32_t max_len)
{
    sh_u32_t i = 0;
    
    while(i < max_len)
    {
        if( /*(buf[i] == 0) ||*/ (buf[i] > UNUSED_ADDR) )
        {
            buf[i] = 0;
            break;
        }
        i++;
    }
    
    buf[max_len-1] = 0;
    return i;
}

/*
 *   Standard Sherlock function for null terminating and printing strings.
 */
static void _SHLK_print(shlk_handle_t *pHandle, char *pBuf, const sh_u32_t max_len)
{
    sh_u32_t len;
    sh_u32_t frag_len;
    char frag_buf[33];
    
    len = _SHLK_null_term((sh_u8_t *)pBuf, max_len);
    pBuf[len-1] = 0;
    
    // send data in a fragmented fashion to the callback.
    frag_len = 0;
    while(len > 0)
    {
        frag_len = (len > 32) ? 32:len;   // fragment the data 32 bytes at a time.
        memcpy(frag_buf, pBuf, frag_len);
        frag_buf[frag_len] = 0;   // have to NULL terminate fragmented string, can't assume caller will only print 'len' chars.
        pHandle->pr_cbk(frag_buf, frag_len);
        pBuf += frag_len;
        len -= frag_len;
    }
}

/*
 *   Assdump a single 4KB sector.
 */
static shlk_err_t _SHLK_assdump_sector(shlk_handle_t *pHandle, sh_u32_t addr)
{
    const sh_u8_t BUF_SZ = 32;   // use a smaller multiple of 256, so we don't use a bunch of stack reading the whole page.
    char buf[BUF_SZ+1];
    sh_u32_t num_pages;
    shlk_ad_hdr_t hdr;
    
    if( _SHLK_ad_read_hdr(pHandle, addr, &hdr) )
    {
        return SHLK_ERR_FLASH_READ;
    }
    
    num_pages = _SHLK_get_used_pages(hdr.pages_used, EXT_FLASH_NUM_PGS_4KB);
    
    if(num_pages > 0)
    {
        num_pages--;  // subtract the header, that doesn't count.
    }
    
    sprintf(buf, "\n\rAD: 0x%x -> %i pages\n\r\0", addr, num_pages);
    _SHLK_print(pHandle, buf, BUF_SZ+1);
    
    while(num_pages--)
    {
        sh_u32_t bytes_out = 0;
        
        addr += EXT_FLASH_PAGE_SIZE;
        sprintf(buf, "\n\rassdump: 0x%x\n\r\0", addr);
        _SHLK_print(pHandle, buf, BUF_SZ+1);
        
        while(bytes_out < EXT_FLASH_PAGE_SIZE)
        {
            int rd_err;
            
            rd_err = pHandle->rd_cbk( addr + bytes_out, (sh_u8_t *)buf, BUF_SZ);
            
            D
            (
                if( rd_err )
                {
                    D(printf("shlk read err %i ad @0x%x\n", rd_err, addr + bytes_out); fflush(stdout);)
                }
            )
            
            if( UNUSED_ADDR == buf[0] )
            {
                break;
            }
            
            _SHLK_print(pHandle, buf, BUF_SZ+1);
            
            if( UNUSED_ADDR == buf[BUF_SZ-1] )
            {
                break;
            }
            
            bytes_out += BUF_SZ;
        }
    }
    
    return SHLK_ERR_OK;
}

/*
 *   Reset all log data.
 *      It does this by just clearing all bits for the log header.
 *      There is no need for the overhead associated with a sector erase.
 */
shlk_err_t SHLK_reset(shlk_handle_t *pHandle)
{
    shlk_lg_hdr_t hdr;
    sh_u32_t addr = pHandle->log_start_addr;
    shlk_err_t err;
    
    pHandle->is_valid = 0;
    
    memset(&hdr, 0, sizeof(hdr));

    while(addr < (pHandle->log_start_addr + pHandle->log_num_sect*EXT_FLASH_64KB_SZ))
    {
        if( pHandle->wr_cbk(addr, (sh_u8_t *)&hdr, sizeof(hdr), 0) )
        {
            return SHLK_ERR_FLASH_WRITE;
        }
        
        addr += EXT_FLASH_64KB_SZ;
    }
    
    return SHLK_ERR_OK;
}

/*
 *   Dump assertion information to the print stream.
 */
shlk_err_t SHLK_assdump(shlk_handle_t *pHandle)
{
    shlk_err_t err;
    
    err = _SHLK_assdump_sector(pHandle, pHandle->asslog_start_addr);
    
    D
    (
        if( SHLK_ERR_OK != err )
        {
            D(printf("ERR %i in assdump_sector\n", err); fflush(stdout);)
        }
    )
    
    err |= _SHLK_assdump_sector(pHandle, pHandle->asslog_start_addr + EXT_FLASH_4KB_SZ);
    
    D
    (
        if( SHLK_ERR_OK != err )
        {
            D(printf("ERR %i in assdump_sector 0x%x\n", err, pHandle->asslog_start_addr + EXT_FLASH_4KB_SZ); fflush(stdout);)
        }
    )
    
    pHandle->pr_cbk(ASSDUMP_DONE, ASSDUMP_DONE_LEN);
    return err;
}

/*
 *   Dump out a whole sector to the console.
 */
static shlk_err_t _SHLK_dump_sector(shlk_handle_t *pHandle, sh_u32_t addr)
{
    shlk_lg_hdr_t hdr;
    shlk_err_t err;
    sh_u32_t page_idx;
    sh_u32_t last_used_page;
    char buf[EXT_FLASH_PAGE_SIZE+1];
    
    if( pHandle->rd_cbk( addr, (sh_u8_t *)&hdr, (sh_u32_t)sizeof(shlk_lg_hdr_t)) )
    {
        return SHLK_ERR_FLASH_READ;
    }
    
    if(hdr.magic != SHLK_MAGIC_NUMBER)
    {
        return SHLK_ERR_OK;  // nothing to dump.
    }
    
    last_used_page = _SHLK_get_first_unused_page_idx(pHandle, (sh_u32_t *)&(hdr.used));
    
    for(page_idx = 1; page_idx < last_used_page; page_idx++)
    {
        /*
         *   If this page has data, go ahead and dump it.
         */
        int len;
        int rd_err;
        
        addr += EXT_FLASH_PAGE_SIZE;
        
        /*
         *   Just print the address at the top of each sector.
         */
        if( 0 == ((addr - EXT_FLASH_PAGE_SIZE) % EXT_FLASH_64KB_SZ) )
        {
            len = sprintf(buf, "\n\r\t* SHLK DUMP: 0x%x *\n\r\0", (addr - EXT_FLASH_PAGE_SIZE));
            _SHLK_print(pHandle, buf, len );
        }

        rd_err = pHandle->rd_cbk( addr, (sh_u8_t *)buf, EXT_FLASH_PAGE_SIZE);
        if( rd_err )
        {
            D(printf("shlk read err %i ad @0x%x\n", rd_err, addr + bytes_out); fflush(stdout);)
            return SHLK_ERR_FLASH_READ;
        }
        
        if( UNUSED_ADDR == buf[0] )
        {
            break;
        }
        
        _SHLK_print(pHandle, buf, EXT_FLASH_PAGE_SIZE+1);
        
        if( UNUSED_ADDR == buf[EXT_FLASH_PAGE_SIZE-1] )
        {
            break;
        }
    }
    
    return SHLK_ERR_OK;
}

/*
 *   Dump on N most recent pages of data.
 */
static shlk_err_t _SHLK_dump_recent_pages(shlk_handle_t *pHandle, sh_u32_t num_pages)
{
    return SHLK_ERR_OK;
}

/*
 *   Dump log data to the print stream.  
 *   num_pages=0 means dump all
 *   num_pages=non-zero means dump that many of those most recent pages.
 */
shlk_err_t SHLK_dump(shlk_handle_t *pHandle, sh_u32_t num_pages)
{
    sh_u32_t addr = pHandle->log_start_addr;
    shlk_err_t err;
    
    if(0 == num_pages)
    {
        // dump it all !
        while(addr < (pHandle->log_start_addr + pHandle->log_num_sect*EXT_FLASH_64KB_SZ))
        {
            err = _SHLK_dump_sector(pHandle, addr);
            if(SHLK_ERR_OK != err)
            {
                return err;
            }
            
            addr += EXT_FLASH_64KB_SZ;
        }
    }
    else
    {
        return _SHLK_dump_recent_pages(pHandle, num_pages);
    }
    
    return SHLK_ERR_OK;
}

/*
 *   Log an assertion to the log stream, which is a ping pong storage in SPI Flash,
 *      consisting of TWO x 4KB SPI Flash sectors.
 */
shlk_err_t SHLK_asslog(shlk_handle_t *pHandle, const char *pStr)
{
    sh_u32_t        addr;
    shlk_err_t      err;
    sh_u32_t        bytes2write;
    int             wr_err;
    
    /*
     *    Get the address in SPI Flash, of where we should write out this data.
     */
    err = _SHLK_get_asslog_addr(pHandle, &addr);
    if( SHLK_ERR_OK != err )
    {
        return err;
    }
    
    /*
     *   Make sure the data is < 1 page.
     */
    bytes2write = strlen(pStr);
    bytes2write = (bytes2write > EXT_FLASH_PAGE_SIZE) ? EXT_FLASH_PAGE_SIZE : bytes2write;
    
    /*
     *    Write out the data.
     */
    wr_err = pHandle->wr_cbk(addr, (sh_u8_t *)pStr, bytes2write, 0);
    if( wr_err )
    {
        D(printf("shlk wr asslog ERR %i @ 0x%x\n\r", wr_err, addr); fflush(stdout);)
        return SHLK_ERR_FLASH_WRITE;
    }
    
    return SHLK_ERR_OK;
}

/*
 *   Enable or disable Sherlock logging system.
 */
shlk_err_t SHLK_enable(shlk_handle_t *pHandle, sh_u32_t en_addr, sh_u8_t enable_sherlock)
{
    shlk_en_hdr_t hdr;
    int wr_err;
    
    hdr.is_enabled = SHLK_ENABLE_MASK & enable_sherlock;
    hdr.reserved = 0xFFFFFFFF;
    hdr.magic = SHLK_MAGIC_NUMBER;
    
    wr_err = pHandle->wr_cbk(en_addr, (sh_u8_t *)&hdr, (sh_u32_t)sizeof(shlk_en_hdr_t), 1);
    if( wr_err )
    {
        D(printf("shlk wr en ERR %i @ 0x%x\n\r", wr_err, en_addr); fflush(stdout);)
        return SHLK_ERR_FLASH_WRITE;
    }
    
    return SHLK_ERR_OK;
}

/*
 *   Return 0 if Sherlock is not enabled, non-zero otherwise.
 */
sh_u8_t SHLK_is_enabled(shlk_handle_t *pHandle, sh_u32_t en_addr)
{
    shlk_en_hdr_t hdr;
    int rd_err;
    
    rd_err = pHandle->rd_cbk( en_addr, (sh_u8_t *)&hdr, (sh_u32_t)sizeof(shlk_en_hdr_t));
    
    D
    (
        if( rd_err )
        {
            D(printf("shlk read err %i is en hdr @0x%x\n\r", rd_err, en_addr); fflush(stdout);)
        }
    )
    
    if( rd_err || (hdr.magic != SHLK_MAGIC_NUMBER) )
    {
        return SHLK_DEFAULT_ENABLE;
    }
    else
    {
        return (sh_u8_t)(SHLK_ENABLE_MASK & hdr.is_enabled);
    }
}

/*
 *   Set and/or Read number of pages pending FROM Sherlock TO the "Server".
 *   This function just a convenience function to help keep track of number of pages
 *     pending to be uploaded to the server.  It is provided in the Sherlock lib only b/c
 *     then it will be easier and more standardized to set it and read it from boot2 and the app.
 *     
 *     pHandle:     sherlock handle (input).
 *     addr:        address to use for storing/reading the serv pages.
 *     num_pages:   number of pages to set.  This will be a "don't care" if read_only == 1.
 *     read_only:   set to 0 if you want to set pages, set to 1 if you just want to know how many pages.
 *     
 *     Returns number of pages pending.  Return 0 if unset, error, or nothing pending.
 */
sh_u32_t SHLK_serv_pages(shlk_handle_t *pHandle, sh_u32_t addr, sh_u32_t num_pages, sh_u8_t read_only)
{
    shlk_serv_hdr_t hdr;
    sh_u32_t erase_sector;
    int err = 0;
    
    if( read_only )
    {
        goto rd_pages;
    }
    
    /*
     *   Number of pages to dump can at most be the number we can fit in the available Sherlock space.
     */
    hdr.num_pages = (num_pages > (EXT_FLASH_NUM_PGS_64KB*pHandle->log_num_sect))? (EXT_FLASH_NUM_PGS_64KB*pHandle->log_num_sect):num_pages;
    hdr.reserved = 0xFFFFFFFF;
    
    /*
     *   Optimization.  If we are saying we have 0 pages/clearing, no need to ERASE sector.  Just invalidate the signature is enough.
     *   NOTE:  We re-use the magic number, and add +1, just as a safety precaution to make sure clients don't mix up addresses
     *           between the enable sector and the "pending server dump" sector.
     */
    erase_sector = (hdr.num_pages > 0)? 1:0;
    hdr.magic = erase_sector? (SHLK_MAGIC_NUMBER + 1) : 0;
    
    err = pHandle->wr_cbk(addr, (sh_u8_t *)&hdr, (sh_u32_t)sizeof(shlk_serv_hdr_t), erase_sector);
    
rd_pages:
    err |= pHandle->rd_cbk( addr, (sh_u8_t *)&hdr, (sh_u32_t)sizeof(shlk_serv_hdr_t));
    if( err || (hdr.magic != (SHLK_MAGIC_NUMBER + 1)) )
    {
        return 0;
    }
    else
    {
        return hdr.num_pages;
    }
}

/*
 *   Sets the Sherlock time callback, so that we can set time if we want.
 */
void SHLK_set_time_cbk( shlk_handle_t *pHandle, shlk_time_cbk_t pCbk, sh_u8_t num_pgs_between_time_write )
{
	pHandle->time_cbk = pCbk;
	pHandle->num_pgs_between_time_write = num_pgs_between_time_write;
}

/*
 *   Initialize the Sherlock logging system.
 */
shlk_err_t SHLK_init(shlk_handle_t *pHandle,
                     shlk_rd_cbk_t rd_cbk,
                     shlk_wr_cbk_t wr_cbk,
                     shlk_pr_cbk_t pr_cbk,
                     sh_u32_t      asslog_start_addr,
                     sh_u32_t      log_start_addr,
                     sh_u8_t       log_num_sect)
{
    pHandle->log_num_sect    = log_num_sect;
    
    pHandle->asslog_start_addr = asslog_start_addr;
    pHandle->log_start_addr    = log_start_addr;
    
    pHandle->rd_cbk = rd_cbk;
    pHandle->wr_cbk = wr_cbk;
    pHandle->pr_cbk = pr_cbk;
    pHandle->time_cbk = 0;
    
    pHandle->is_valid = 0;
    
    return SHLK_ERR_OK;
}
