/*
 * cfg_factory.c
 *
 *  Created on: Jun 5, 2013
 *      Author: Ernie
 */

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include "cfg_factory.h"
#include <string.h>

////////////////////////////////////////////////////////////////////////////////
// Defines
////////////////////////////////////////////////////////////////////////////////
#define  FLASH_4KB_SEC_SZ          ( ( unsigned long ) 0x1000 )
#define  NUM_FCONFIGS              32
#define  FCFG_SIZE                 ( FLASH_4KB_SEC_SZ / NUM_FCONFIGS )
#define  FLASH_FCFG_ADDR           ( ( unsigned long ) 0x0000C000 )     // just so we are sure we agree with the clients.
#define  FACTORY_CFG_MAGIC         0xFEEDFACE
#define  FVERSION_1                0x1
#define  FVERSION_2                0x2
#define  FVERSION_3                0x3
#define  CURRENT_VERSION           FVERSION_1
#define  FCFG_HAS( x )             ( ( unsigned long )( 0x00000001 << (x-CFG_FACTORY_FIRST_FCFG) ) )
#define  FNULL                     ( ( unsigned long ) 0x00000000 )
#define  PROTO3_PART_NUMBER        "103-00122-00"
#define  PROTO4_PART_NUMBER        "103-00122-01"
#define  MLB_PART_NUMBER           "103-00103-00"
#define  MLB_REVB                  "B"

/*
 *   DEBUG FEATURES
 */
#define  FDEBUG                    0
#if      FDEBUG
    #define D(x)    x
    #include <stdio.h>
#else
    #define D(x)
#endif

////////////////////////////////////////////////////////////////////////////////
// Typedef's
////////////////////////////////////////////////////////////////////////////////

typedef unsigned long u32_t;
typedef unsigned char u8_t;

/*
 *   HEADER:  Changes are NOT allowed (except for "reserved" NAMES).
 */
typedef struct factory_cfg_header_type
{
    u32_t magic;
    u32_t version;
    u32_t has_field;
    
    u32_t reserved1;
    u32_t reserved2;
    u32_t reserved3;
    u32_t reserved4;
    
    fcfg_len_t  len[NUM_FCONFIGS];
    
}fcfg_header_t;

typedef struct factory_cfg_buffer
{
    u32_t      num_cfgs;    // number of configs that are in SPI Flash and that need buffering.
    fcfg_tag_t *pTags;      // array of tags that are buffered.
    u8_t       **pBuffers;  // array of buffers for each config. 
}fcfg_buf_t;

////////////////////////////////////////////////////////////////////////////////
// Variables
////////////////////////////////////////////////////////////////////////////////

/*
 *   Descriptors for each of the configs.
 */
static const char *g_fcfg_desc[ NUM_FCONFIGS ] = {  "hdr\0",          // 0
                                                    "ser\0",
                                                    "mpcb\0",
                                                    "mpcbrev\0",
                                                    "mpcbrw\0",
                                                    "lpcb\0",         // 5
                                                    "lpcbrev\0",
                                                    "lpcbrw\0",
                                                    "tla\0",
                                                    "bssid\0",
                                                    "flags\0",        // 10
                                                    "pddur\0",
                                                    "pdful\0",
                                                    "pdlow\0",
                                                    "zpcba\0",
                                                    "zfta\0",         // 15
                                                    "r16\0",
                                                    "r17\0",
                                                    "r18\0",
                                                    "r19\0",
                                                    "r20\0",          // 20
                                                    "r21\0",
                                                    "r22\0",
                                                    "r23\0",
                                                    "r24\0",
                                                    "r25\0",          // 25
                                                    "r26\0",
                                                    "r27\0",
                                                    "r28\0",
                                                    "r29\0",
                                                    "r30\0",          // 30
                                                    "r31\0"
};

////////////////////////////////////////////////////////////////////////////////
// Private Declarations
////////////////////////////////////////////////////////////////////////////////
static u8_t _FCFG_tag_isvalid(fcfg_tag_t tag) __attribute__ ((section (FCFG_TXT_SEC)));
static u8_t _FCFG_hdl_isvalid(fcfg_handle_t *pHandle) __attribute__ ((section (FCFG_TXT_SEC)));
static u8_t _FCFG_len_isvalid(fcfg_len_t len) __attribute__ ((section (FCFG_TXT_SEC)));
static u32_t _FCFG_addr(fcfg_handle_t *pHandle, fcfg_tag_t tag) __attribute__ ((section (FCFG_TXT_SEC)));
static u32_t _FCFG_num_cfgs(fcfg_header_t *pHdr, fcfg_tag_t *pTags) __attribute__ ((section (FCFG_TXT_SEC)));
static fcfg_err_t _FCFG_gethdr(fcfg_handle_t *pHandle, fcfg_header_t *pHdr) __attribute__ ((section (FCFG_TXT_SEC)));
static fcfg_err_t _FCFG_sethdr(fcfg_handle_t *pHandle, fcfg_header_t *pHdr, u32_t erase) __attribute__ ((section (FCFG_TXT_SEC)));

////////////////////////////////////////////////////////////////////////////////
// Code
////////////////////////////////////////////////////////////////////////////////

/*
 *   Retrieve header from SPI Flash (or build it from scratch.)
 */
static fcfg_err_t _FCFG_gethdr(fcfg_handle_t *pHandle, fcfg_header_t *pHdr)
{
    /*
     *   Read the header from SPI Flash into the caller's buffer.
     */
    int rd_err = 0;
    
    rd_err = pHandle->rd_cbk( pHandle->addr, (u8_t *)pHdr, (u32_t)sizeof(fcfg_header_t));
    if( rd_err )
    {
        D(printf("FCFG_gethdr() rd_cbk -> rd_err: %i\n", rd_err); fflush(stdout);)
        return FCFG_FLASH_ERROR;
    }

    if( FACTORY_CFG_MAGIC != pHdr->magic )
    {
        /*
         *   New header, so initialize it.
         */
        memset(pHdr, 0, sizeof(fcfg_header_t));
        pHdr->magic = FACTORY_CFG_MAGIC;
        pHdr->version = CURRENT_VERSION;
    }
    
    return FCFG_OK;
}

/*
 *   Set the header to the SPI Flash.
 */
static fcfg_err_t _FCFG_sethdr(fcfg_handle_t *pHandle, fcfg_header_t *pHdr, u32_t erase)
{
    int wr_err = 0;
    
    pHdr->version = CURRENT_VERSION;
    pHdr->magic = FACTORY_CFG_MAGIC;
    
    wr_err = pHandle->wr_cbk( pHandle->addr, (u8_t *)pHdr, sizeof(fcfg_header_t), erase);
    if( wr_err )
    {
        D(printf("FCFG_sethdrf() wr_cbk -> wr_err: %i\n", wr_err); fflush(stdout);)
        return FCFG_FLASH_ERROR;
    }

    return FCFG_OK;
}

/*
 *   Returns address in SPI Flash to write this config to.
 */
static u32_t _FCFG_addr(fcfg_handle_t *pHandle, fcfg_tag_t tag)
{
    return pHandle->addr + ( tag * FCFG_SIZE );
}

/*
 *   Is the tag valid?
 */
static u8_t _FCFG_tag_isvalid(fcfg_tag_t tag)
{
    return ( (tag >= CFG_FACTORY_FIRST_FCFG) && (tag < NUM_FCONFIGS ) ) ?  1:0;
}

/*
 *   Is the length valid?
 */
static u8_t _FCFG_len_isvalid(fcfg_len_t len)
{
    return ( (len > 0) && (len <= FCFG_SIZE ) ) ?  1:0;
}

/*
 *   Is the handle OK?
 */
static u8_t _FCFG_hdl_isvalid(fcfg_handle_t *pHandle)
{
    if( FNULL == pHandle ){ return 0; }
    return ( FLASH_FCFG_ADDR == pHandle->addr ) ? 1:0;
}

/*
 *   Retrieve a factory config from SPI Flash.
 */
fcfg_err_t FCFG_get(fcfg_handle_t *pHandle, fcfg_tag_t tag, fcfg_pval_t pValue)
{
    fcfg_len_t len;
    fcfg_err_t err;
    int ierr = 0;
    D(u32_t TEST = 0;)
    
    if( ! _FCFG_tag_isvalid(tag) ) { return FCFG_BAD_TAG; }
    if( ! _FCFG_hdl_isvalid(pHandle) ){ return FCFG_BAD_HANDLE; }
    
    err = FCFG_getlen( pHandle, tag, &len );
    if( FCFG_OK != err )
    {
        return err;
    }
    
    /*
     *   Read the data from SPI Flash into the caller's buffer.
     */
    ierr = pHandle->rd_cbk( _FCFG_addr(pHandle, tag), pValue, (u32_t)len);
    
    D(memcpy(&TEST, pValue, 4);)
    D(printf("ADDR: 0x%x  TEST VALUE: %i  LEN %i\n", _FCFG_addr(pHandle, tag), TEST, len); fflush(stdout);)
    
    if( ierr )
    {
        D(printf("ierr: %i\n", ierr); fflush(stdout);)
        return FCFG_FLASH_ERROR;
    }

    return FCFG_OK;
}

/*
 *   Return the number of configs we have in the factory config table.
 */
static u32_t _FCFG_num_cfgs(fcfg_header_t *pHdr, fcfg_tag_t *pTags)
{
    u32_t bit = 1;
    u32_t num_cfgs = 0;
    u32_t i;
    
    for(i = 0; i < NUM_FCONFIGS; i++)
    {
        if(pHdr->has_field & bit)
        {
            pTags[num_cfgs++] = (fcfg_tag_t)(i + 1);
        }
        bit = bit << 1;
    }
    
    D(printf("num cfgs: %u\n", num_cfgs); fflush(stdout);)
    return num_cfgs;
}

/*
 *   Returns the RF Switch present on the hardware.
 *     Will return "INVALID" if there's an error.
 *     
 *     https://whistle.atlassian.net/wiki/display/COR/Corona+assembly+revisions+with+software+impacts
 */
fcfg_rfswitch_t FCFG_rfswitch(fcfg_handle_t *pHandle)
{   
    if( FCFG_RFSWITCH_INVALID != pHandle->rfswitch )
    {
        // Use the "cache" if we can.
        return (fcfg_rfswitch_t)pHandle->rfswitch;
    }
    
    switch( FCFG_board(pHandle) )
    {
        /*
         *    Process the boards that are unconditionally AS-169.
         */
        case FCFG_BOARD_MLB_REV_3:
        case FCFG_BOARD_P4_DEV:
        case FCFG_BOARD_P4_STUFFED_P3_DEV:
            pHandle->rfswitch = FCFG_RFSWITCH_AS169;
            break;
            
        /*
         *    Process the boards that are unconditionally AS-193   
         */
        case FCFG_BOARD_MLB_REV_4:
        case FCFG_BOARD_MLB_REV_5:
            pHandle->rfswitch = FCFG_RFSWITCH_AS193;
            break;
        
        /*
         *    P3 is the only board that requires looking at other configs.
         *       Use AS-169 if "scfg 4 == 0", otherwise use AS-193
         */
        case FCFG_BOARD_P3_DEV:
            {
                unsigned long rework;
                
                if( FCFG_OK != FCFG_get(pHandle, CFG_FACTORY_MPCBARW, &rework) )
                {
                    pHandle->rfswitch = FCFG_RFSWITCH_INVALID;
                }
                else if( 0 == rework )
                {
                    pHandle->rfswitch = FCFG_RFSWITCH_AS169;
                }
                else
                {
                    pHandle->rfswitch = FCFG_RFSWITCH_AS193;
                }
            }
            break;

        /*
         *    An invalid board type is not acceptable.
         */
        case FCFG_BOARD_INVALID:
        default:
            pHandle->rfswitch = FCFG_RFSWITCH_INVALID;
            break;
    }

    return (fcfg_rfswitch_t)pHandle->rfswitch;
}

/*
 *   Returns the Silego chip present on the hardware.
 *     Will return "INVALID" if there's an error.
 *     
 *     https://whistle.atlassian.net/wiki/display/COR/Corona+assembly+revisions+with+software+impacts
 */
fcfg_silego_t FCFG_silego(fcfg_handle_t *pHandle)
{
    if( FCFG_SILEGO_INVALID != pHandle->silego )
    {
        // Use the "cache" if we can.
        return (fcfg_silego_t)pHandle->silego;
    }
    
    switch( FCFG_board(pHandle) )
    {
        /*
         *   Majority of hardware is "old" Silego right now, unless specified otherwise.
         */
        case FCFG_BOARD_MLB_REV_3:
        case FCFG_BOARD_MLB_REV_4:
        case FCFG_BOARD_MLB_REV_5:
        case FCFG_BOARD_P3_DEV:
        case FCFG_BOARD_P4_STUFFED_P3_DEV:
            pHandle->silego = FCFG_SILEGO_OLD_BZE;
            break;
        
        case FCFG_BOARD_P4_DEV:
            pHandle->silego = FCFG_SILEGO_NEW_BZI;
            break;

        /*
         *    An invalid board type is not acceptable.
         */
        case FCFG_BOARD_INVALID:
        default:
            pHandle->silego = FCFG_SILEGO_INVALID;
            break;
    }
    
    return (fcfg_silego_t)pHandle->silego;
}

/*
 *   Returns the board type.
 *     Will return "INVALID" if there's an error.
 *     
 *     https://whistle.atlassian.net/wiki/display/COR/Corona+assembly+revisions+with+software+impacts
 */
fcfg_board_t FCFG_board(fcfg_handle_t *pHandle)
{
    unsigned long rev;
    char *pPartNumber = FNULL; //, *pProdRev = FNULL;
    fcfg_len_t len;
    
    if( FCFG_BOARD_INVALID != pHandle->board )
    {
        // Use the "cache" if we can.
        return (fcfg_board_t)pHandle->board;
    }
    
    if( FCFG_OK != FCFG_getlen(pHandle, CFG_FACTORY_MPCBA, &len) )
    {
        goto DONE;
    }
    
    pPartNumber = pHandle->malloc( len + 1 );
    if( FNULL == pPartNumber )
    {
        goto DONE;
    }
    
    if( FCFG_OK != FCFG_get(pHandle, CFG_FACTORY_MPCBA, pPartNumber) )
    {
        goto DONE;
    }
    
    if (!strcmp(pPartNumber, PROTO3_PART_NUMBER))
    {   
        /*
         *   2 Options:
         *   
         *   Option 1:  scfg 3 == 3 means Proto 3
         *   Option 2:  scfg 3 == 4 means Proto 4 stuffed as Proto 3
         */
        if( FCFG_OK != FCFG_get(pHandle, CFG_FACTORY_MPCBAREV, &rev) )
        {
        }
        else if( 3 == rev )
        {
            pHandle->board = FCFG_BOARD_P3_DEV;
        }
        else if( 4 == rev )
        {
            pHandle->board = FCFG_BOARD_P4_STUFFED_P3_DEV;
        }
    }
    else if (!strcmp(pPartNumber, PROTO4_PART_NUMBER))
    {
        pHandle->board = FCFG_BOARD_P4_DEV;
    }
    else if (!strcmp(pPartNumber, MLB_PART_NUMBER))
    {
        /*
         *   3 Options:
         *   
         *   Option 1:  scfg 3 == 5 means Rev 5
         *   Option 2:  scfg 3 == 3 means Rev 3
         *   Option 3:  scfg 3 == 4 means Rev 4
         */
        
#if 0   // Commenting this out since we aren't using prod rev anymore.
        
        /*
         *   Check for production revisions of the MLB.
         */
        if( FCFG_OK != FCFG_getlen(pHandle, CFG_FACTORY_PRODREV, &len) )
        {
            goto CHECK_NON_PROD_VER;
        }
        pProdRev = pHandle->malloc( len + 1 );
        if( FNULL == pProdRev )
        {
            goto CHECK_NON_PROD_VER;
        }
        
        if( FCFG_OK != FCFG_get(pHandle, CFG_FACTORY_PRODREV, pProdRev) )
        {
            goto CHECK_NON_PROD_VER;
        }
        
        if (!strcmp(pProdRev, MLB_REVB))
        {
            pHandle->board = FCFG_BOARD_MLB_REV_B;
            goto DONE;
        }
                
CHECK_NON_PROD_VER:

#endif

        /*
         *   Check for non-production revisions of the MLB.
         */
        if( FCFG_OK != FCFG_get(pHandle, CFG_FACTORY_MPCBAREV, &rev) )
        {
        }
        else if( 3 == rev )
        {
            pHandle->board = FCFG_BOARD_MLB_REV_3;
            
        }
        else if( 4 == rev )
        {
            pHandle->board = FCFG_BOARD_MLB_REV_4;
        }
        else if( 5 == rev )
        {
            pHandle->board = FCFG_BOARD_MLB_REV_5;
        }
    }
    
DONE:
    if( FNULL != pPartNumber )
    {
        pHandle->free(pPartNumber);
    }
#if 0
    if( FNULL != pProdRev )
    {
        pHandle->free(pProdRev);
    }
#endif
    return (fcfg_board_t)pHandle->board;
}

/*
 *   Returns the led type present on the LED flex.
 *     Will return "INVALID" if there's an error.
 *     
 *     https://whistle.atlassian.net/wiki/display/COR/Corona+assembly+revisions+with+software+impacts
 */
fcfg_led_t FCFG_led(fcfg_handle_t *pHandle)
{
    unsigned long lpcba_rev;
    
    if( FCFG_LED_INVALID != pHandle->led )
    {
        // Use the "cache" if we can.
    }
    else if( FCFG_OK != FCFG_get(pHandle, CFG_FACTORY_LPCBAREV, &lpcba_rev) )
    {
        pHandle->led = FCFG_LED_INVALID;
    }
    else if( lpcba_rev <= (unsigned long)FCFG_LED_ROHM)
    {
        pHandle->led = FCFG_LED_ROHM;
    }
    else
    {
        pHandle->led = FCFG_LED_EVERBRIGHT;
    }
    
    return (fcfg_led_t)pHandle->led;
}

/*
 *   Set a single factory config.
 */
fcfg_err_t FCFG_set(fcfg_handle_t *pHandle, fcfg_tag_t tag, fcfg_len_t len, fcfg_pval_t pValue)
{
    fcfg_header_t *pHdr;
    fcfg_err_t err = FCFG_OK;
    fcfg_buf_t fbuf;
    u32_t i;
    int ierr = 0;
    
    if( ! _FCFG_tag_isvalid(tag) ) { return FCFG_BAD_TAG; }
    if( ! _FCFG_hdl_isvalid(pHandle) ){ return FCFG_BAD_HANDLE; }
    if( ! _FCFG_len_isvalid(len) ){ return FCFG_BAD_LEN; }
    
    pHdr = 0;
    fbuf.pBuffers = 0;
    fbuf.pTags = 0;
    fbuf.num_cfgs = 0;
    
    pHdr = pHandle->malloc(sizeof(fcfg_header_t));
    if( FNULL == pHdr )
    {
        D(printf("ERR: malloc: pHdr: 0x%x (%i bytes)\n", pHdr, sizeof(fcfg_header_t)); fflush(stdout);)
        return FCFG_MALLOC_ERROR;
    }
    
    /*
     *   Retrieve the header.
     */
    err = _FCFG_gethdr( pHandle, pHdr );
    if( FCFG_OK != err )
    {
        goto done_fcfg_set;
    }
    
    /*
     *   Malloc memory for existing configs and save them off in RAM, so we can re-write them
     *     after the necessary step of ERASING the SPI Memory.
     *     
     *   Notice we are not including the one we are about to set.  The reason for this is because
     *     the caller already buffered this tag and its data, so no need to double buffer it.
     */
    pHdr->has_field &= ~FCFG_HAS(tag);
    fbuf.pTags = pHandle->malloc( sizeof(fcfg_tag_t) * NUM_FCONFIGS );
    if( FNULL == fbuf.pTags )
    {
        D( printf("ERR: malloc: pTags: 0x%x (%i bytes)\n",
            fbuf.pTags, sizeof(fcfg_tag_t) * NUM_FCONFIGS); fflush(stdout); )
        err = FCFG_MALLOC_ERROR;
        goto done_fcfg_set;
    }
    fbuf.num_cfgs = _FCFG_num_cfgs( pHdr, fbuf.pTags );
    
    if( FNULL == fbuf.num_cfgs )
    {
        /*
         *   No need to buffer configs that don't exist!
         */
        goto wrapup_fcfg_set;
    }
    
    /*
     *   We have at least one existing config that needs to be buffered,
     *      so malloc memory for existing configs
     */
    fbuf.pBuffers = pHandle->malloc( sizeof(u8_t *) * fbuf.num_cfgs );
    if( FNULL == fbuf.pBuffers )
    {
        D( printf("ERR: malloc: pBuffers: 0x%x (%i bytes)\n", 
            fbuf.pBuffers, sizeof(u8_t *) * fbuf.num_cfgs); fflush(stdout); )
        err = FCFG_MALLOC_ERROR;
        goto done_fcfg_set;
    }
    memset(fbuf.pBuffers, 0, sizeof(u8_t *) * fbuf.num_cfgs );
    
    for(i = 0; i < fbuf.num_cfgs; i++)
    {
        fbuf.pBuffers[i] = pHandle->malloc(pHdr->len[fbuf.pTags[i]]);
        if( FNULL == fbuf.pBuffers[i] )
        {
            D( printf("ERR: malloc: pBuffers[%u]: 0x%x (%i bytes)\n", 
                        i, fbuf.pBuffers[i], pHdr->len[fbuf.pTags[i]]); fflush(stdout); )
            err = FCFG_MALLOC_ERROR;
            goto done_fcfg_set;
        }
        
        /*
         *   Buffer the config's data.
         */
        ierr = pHandle->rd_cbk( _FCFG_addr(pHandle, fbuf.pTags[i]), fbuf.pBuffers[i], (u32_t)pHdr->len[fbuf.pTags[i]]);
        if( ierr  )
        {
            D(printf("FCFG_set() rd_cbk -> ierr: %i\n", ierr); fflush(stdout);)
            err = FCFG_FLASH_ERROR;
            goto done_fcfg_set;
        }
    }
    
wrapup_fcfg_set:
    
    /*
     *    Add presence of this config to the header, then write out the latest header to SPI Flash.
     *    An ERASE is necessary here.
     */
    pHdr->has_field |= FCFG_HAS(tag);
    pHdr->len[tag] = len;
    
    err = _FCFG_sethdr( pHandle, pHdr, 1);
    if( FCFG_OK != err )
    {
        goto done_fcfg_set;
    }
    
    /*
     *   If we have pending buffered data to write out, write it...
     */
    for(i = 0; i < fbuf.num_cfgs; i++)
    {   
        /*
         *   Write the buffered data out to SPI Flash.
         */
        ierr = pHandle->wr_cbk( _FCFG_addr(pHandle, fbuf.pTags[i]), fbuf.pBuffers[i], pHdr->len[fbuf.pTags[i]], 0);
        if( ierr )
        {
            D(printf("FCFG_set() wr_cbk -> ierr: %i\n", ierr); fflush(stdout);)
            err = FCFG_FLASH_ERROR;
            goto done_fcfg_set;
        }
    }
    
    /*
     *   Write the user's NEW data to SPI Flash.
     */
    ierr = pHandle->wr_cbk( _FCFG_addr(pHandle, tag), pValue, len, 0);
    if( ierr )
    {
        D(printf("FCFG_set() wr_cbk -> ierr: %i\n", ierr); fflush(stdout);)
        err = FCFG_FLASH_ERROR;
        goto done_fcfg_set;
    }
    
done_fcfg_set:

    /*
     *   Handle all the memory freeing in one place.
     */

    if(FNULL != pHdr)
    {
        pHandle->free(pHdr);
    }
    
    if(FNULL != fbuf.pTags)
    {
        pHandle->free(fbuf.pTags);
    }
    
    if(FNULL != fbuf.pBuffers)
    {
        for(i = 0; i < fbuf.num_cfgs; i++)
        {
            if(FNULL != fbuf.pBuffers[i])
            {
                pHandle->free(fbuf.pBuffers[i]);
            }
        }
        pHandle->free(fbuf.pBuffers);
    }
    
    return err;
}

/*
 *   Clear all factory configs.
 */
fcfg_err_t FCFG_clearall(fcfg_handle_t *pHandle)
{
    /*
     *   This is easy, just write a magic number that disagrees, invalidating the header.
     *      Don't bother erasing the sector, that is not necessary since we are clearing the bits.
     */
    fcfg_header_t *pHdr;
    fcfg_err_t err = FCFG_OK;
    
    pHdr = pHandle->malloc(sizeof(fcfg_header_t));
    if( FNULL == pHdr )
    {
        D( printf("ERR: malloc: pHdr: 0x%x (%i bytes)\n", 
             pHdr, sizeof(fcfg_header_t)); fflush(stdout); )
        return FCFG_MALLOC_ERROR;
    }
    
    memset(pHdr, 0, sizeof(fcfg_header_t));
    
    err = _FCFG_sethdr( pHandle, pHdr, 0 );
    
    pHandle->free(pHdr);
    return err;
}

/*
 *    Clear a single factory config.
 */
fcfg_err_t FCFG_clear(fcfg_handle_t *pHandle, fcfg_tag_t tag)
{
    fcfg_header_t *pHdr;
    fcfg_err_t err = FCFG_OK;
    
    pHdr = pHandle->malloc(sizeof(fcfg_header_t));
    if( FNULL == pHdr )
    {
        D( printf("ERR: malloc: pHdr: 0x%x (%i bytes)\n", 
              pHdr, sizeof(fcfg_header_t)); fflush(stdout); )
        return FCFG_MALLOC_ERROR;
    }
    
    /*
     *   Retrieve the header.
     */
    err = _FCFG_gethdr( pHandle, pHdr );
    if( FCFG_OK != err )
    {
        goto done_fcfg_clear;
    }
    
    /*
     *   Clear only this particular bit in the 'has' field.
     */
    pHdr->has_field &= ~FCFG_HAS(tag);
    
    /*
     *   Since we can clear bits in SPI Flash, and we are changing nothing else,
     *      there is no need to ERASE.
     */
    err = _FCFG_sethdr( pHandle, pHdr, 0);
    
done_fcfg_clear:
    pHandle->free(pHdr);
    return err;
}

/*
 *    Get the size/length of a particular config.
 */
fcfg_err_t FCFG_getlen(fcfg_handle_t *pHandle, fcfg_tag_t tag, fcfg_len_t *pLen)
{
    fcfg_header_t *pHdr;
    fcfg_err_t err = FCFG_OK;
    
    if( ! _FCFG_tag_isvalid(tag) ) { return FCFG_BAD_TAG; }
    if( ! _FCFG_hdl_isvalid(pHandle) ){ return FCFG_BAD_HANDLE; }
    
    pHdr = pHandle->malloc(sizeof(fcfg_header_t));
    if( FNULL == pHdr )
    {
        D(printf("ERR: malloc: pHdr: 0x%x (%i bytes)\n", 
                pHdr, sizeof(fcfg_header_t)); fflush(stdout);)
        return FCFG_MALLOC_ERROR;
    }
    
    /*
     *   Retrieve the header.
     */
    err = _FCFG_gethdr( pHandle, pHdr );
    if( FCFG_OK != err )
    {
        goto done_fcfg_getlen;
    }
    
    if( pHdr->has_field & FCFG_HAS(tag) )
    {
        *pLen = pHdr->len[tag];
    }
    else
    {
        *pLen = 0;
        err = FCFG_VAL_UNSET;  // config doesn't exist.
        D(printf("ERR: Tag %i is not yet set.\n", tag); fflush(stdout);)
        goto done_fcfg_getlen;
    }
    
done_fcfg_getlen:
    
    pHandle->free(pHdr);
    return err;
}

/*
 *   Return a string descriptor for a particular config.
 */
fcfg_err_t FCFG_getdescriptor(fcfg_tag_t tag, char **ppDesc)
{
    if( ! _FCFG_tag_isvalid(tag) ) { return FCFG_BAD_TAG; }
    
    *ppDesc = (char *)g_fcfg_desc[tag];
    return FCFG_OK;
}

/*
 *   Return a tag corresponding to a descriptor.
 */
fcfg_err_t FCFG_gettagfromdescriptor(char *pDescriptor, fcfg_tag_t *pTag)
{
    fcfg_tag_t i;
    
    for(i = CFG_FACTORY_FIRST_FCFG; i < NUM_FCONFIGS; i++)
    {
        if( !strcmp(g_fcfg_desc[i], pDescriptor) )
        {
            *pTag = i;
            return FCFG_OK;
        }
    }
    
    D(printf("Descriptor (%s) is not in the table!\n", pDescriptor); fflush(stdout);)
    return FCFG_DESC_UNKNOWN;
}

/*
 *   Initialize an FCFG handle (required before using FCFG system).
 */
fcfg_err_t FCFG_init(fcfg_handle_t *pHandle,
                     fcfg_addr_t   addr,
                     fcfg_rd_cbk_t rd_cbk,
                     fcfg_wr_cbk_t wr_cbk,
                     fcfg_malloc_t malloc,
                     fcfg_free_t   free)
{
    /*
     *   Make sure header size doesn't exceed amount of space we reserve for it.
     */
    if( FCFG_SIZE < sizeof(fcfg_header_t) )
    {
        D(printf("ERR: header size %i is > 128\n", sizeof(fcfg_header_t)); fflush(stdout);)
        return FCFG_HEADER_BIG;
    }
    
    if( FLASH_FCFG_ADDR != addr )
    {
        /*
         *   Make sure we aren't using an inconsistent address accross FW images,
         *      for storing factory config info.
         */
        D(printf("ERR: address 0x%x is inconsistent with fac cfgs!\n", addr); fflush(stdout);)
        return FCFG_WRONG_ADDR;
    }
    
    pHandle->addr = addr;
    pHandle->rd_cbk = rd_cbk;
    pHandle->wr_cbk = wr_cbk;
    pHandle->malloc = malloc;
    pHandle->free = free;
    
    // Initialize factory configs to invalid, so we know to set them later on.
    pHandle->led = FCFG_LED_INVALID;
    pHandle->rfswitch = FCFG_RFSWITCH_INVALID;
    pHandle->board = FCFG_BOARD_INVALID;
    pHandle->silego = FCFG_SILEGO_INVALID;
    
    return FCFG_OK;
}
