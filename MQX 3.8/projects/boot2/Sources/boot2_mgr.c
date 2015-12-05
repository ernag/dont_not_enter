////////////////////////////////////////////////////////////////////////////////
//! \addtogroup boot2
//! \ingroup corona
//! @{
//
//! \file boot2_mgr.c
//! \brief boot2 data manager on Corona.
//! \version version 0.1
//! \date Feb, 2013
//! \author jim@whistle.com
//!
//! \detail Passes data between USB and console in command mode
//! \detail Handles firmware updates from USB to flash and SPI flash
//
// Copyright (c) 2013 Whistle Labs
//
// Whistle Labs
// Proprietary and Confidential
//
// This source code and the algorithms implemented therein constitute
// confidential information and may comprise trade secrets of Whistle Labs
// or its associates.
//
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include "boot2_mgr.h"
#include "corona_console.h"
#include "corona_gpio.h"
#include "virtual_com.h"
#include "PE_LDD.h"
#include "FLASH1.h"
#include "CRC1.h"
#include "wson_flash.h"
#include "parse_fwu_pkg.h"
#include "corona_ext_flash_layout.h"
#include "persist_var.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>


////////////////////////////////////////////////////////////////////////////////
// Enums
////////////////////////////////////////////////////////////////////////////////


enum
{
    FWU_BOOT2A_OFFSET = 0,
    FWU_BOOT2B_OFFSET = 1,
    FWU_APP_OFFSET    = 2,
    FWU_BOOT1_OFFSET  = 3,
    FWU_MAX_OFFSETS
};


////////////////////////////////////////////////////////////////////////////////
// Structs
////////////////////////////////////////////////////////////////////////////////

typedef struct
{
    uint8_t boot_flags[FWU_MAX_OFFSETS];
} FWU_PERSISTENT_SPI_T;

static FWU_PERSISTENT_SPI_T fwu_spi_data;

////////////////////////////////////////////////////////////////////////////////
// Definitions
////////////////////////////////////////////////////////////////////////////////
#define NUM_RX_BUF         2
#define RX_BUF_SIZE        2048
#define SECTOR_SIZE        2048
#define CONSOLE_BUF_SZ     128
#define USB_OK             0x00

#define MAX_APP_IMAGE_SIZE      (0x80000 - 0x22000)  // Total Internal FLASH - start location of app.
													 // For 0x80000->0x22000 = 0x5e000 = 385,024 bytes
#define MAX_BOOT2_IMAGE_SIZE    0x10000

// Magic number to check if patch is in SPI flash
#define PATCH_SPI_MAGIC_NUM            0xDEADBEEF

extern void delay(int dcount);
extern uint8_t b2h[];
extern int error_abort;


////////////////////////////////////////////////////////////////////////////////
// Variables
////////////////////////////////////////////////////////////////////////////////

static uint32_t             other_boot;
static uint8_t*             other_b2h;                                          // pointer to other boot2 header
static uint8_t*             appHdr              = (uint8_t*)APP_HDR;            // pointer to app header
static uint32_t             spi_0_start_addrs   = EXT_FLASH_APP_IMG_ADDR;
static uint32_t             spi_1_start_addrs   = EXT_FLASH_APP_IMG_BKP_ADDR;
static uint8_t              spi_num_sectors     = EXT_FLASH_APP_IMG_NUM_SECTORS;  // Same for both locations

static uint8_t              rx_buffer[NUM_RX_BUF][RX_BUF_SIZE];
static uint8_t*             prx_buf;
static int                  full_buffer;
static int                  fbuf;
static int                  buf_idx;
static int                  update_reset = 0;

volatile uint8_t            rx_rcvd;
volatile uint32_t           rx_idx;
volatile uint32_t           rx_count;

volatile int                OpFinishedFlg = FALSE;
LDD_TError                  f_result;
LDD_FLASH_TOperationStatus  OpStatus;
LDD_TDeviceData*            MyFLASH_Ptr = NULL;

LDD_TDeviceData*            MyCRC1_Ptr = NULL;

// Globals for receiving USB data
static uint32_t             fw_address;
static uint32_t             data_address;
static uint32_t             fw_size;
static char rxc[64];        // Number of bytes received from USB
static fwu_pkg_t            gpackage;
static fwu_pkg_t*           gppkg          = &gpackage;
static uint32_t             gsize;
static uint32_t             gcrc;


// corona console stuff
cc_handle_t                 g_cc_handle;

static cc_mode_t            cc_mode;
static uint8_t              g_console_buf[CONSOLE_BUF_SZ];
static uint8_t              g_curr_send_buf[USB_BUF_SIZE];
static uint8_t              g_recv_size;
static uint8_t              g_send_size;
static char                 *prompt;

////////////////////////////////////////////////////////////////////////////////
// Declarations
////////////////////////////////////////////////////////////////////////////////

static void         cc_rcv_cmd();
static void         init_cmd_mode();
static void         init_data_mode();
static corona_err_t b2_erase_spi_flash(uint32_t start_addrs, uint32_t esize);
static corona_err_t b2_write_spi(uint8_t *pdata, uint32_t t_address, uint32_t d_size);
static corona_err_t b2_read_spi(uint8_t *rbuf, uint32_t addrs, uint32_t num_bytes);
static void         b2_load_image_from_spi_flash(uint32_t spi_start_addrs, uint32_t app_image_offset, uint32_t target_address, uint32_t image_size);
static void         b2_transfer_patch (uint32_t spi_start_addrs, uint32_t patch_offset, uint32_t target_address, uint32_t patch_size);
static uint8_t      cksum(uint8_t *hdr);
static uint32_t     calc_crc_int(uint32_t address, uint32_t size);
static int          find_image(fwu_pkg_t* ppkg, contents_mask_t image_mask);
static void         set_pkg_update_flag(fwu_pkg_t* gppkg);
static uint32_t     calc_crc_spi_image(uint32_t spi_start_addrs, uint32_t binary_image_offset, uint32_t image_size);

void     set_flag(uint8_t *flagp);
uint32_t calc_crc_pkg (int spi_loc, int pkg_size);

extern void usb_reset(void);

////////////////////////////////////////////////////////////////////////////////
// Code
////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//! \brief
//!
//!
//!
//! \detail
//!
//! \return
//!
///////////////////////////////////////////////////////////////////////////////


// Perform boot2 tasks
void b2_task()
{
    int      status;
    int      result;
    uint32_t data_size;
    uint8_t  *pdata;

    if (rx_rcvd)
    {
        rx_rcvd = 0;
        if (cc_mode == CC_COMMAND)
        {
            cc_rcv_cmd();
            rx_idx      = 0;
            g_recv_size = 0;
        }
        else // cc_mode is CC_LOAD_INT or CC_LOAD_SPI or CC_LOAD_PKG
        {
            if (full_buffer)
            {
                full_buffer = 0;
                data_size   = RX_BUF_SIZE;
                pdata       = &rx_buffer[fbuf][0];

                if (cc_mode == CC_LOAD_INT)
                {
                    result = b2_write_fw(pdata, data_address, data_size);
                }
                else if (cc_mode == CC_LOAD_SPI)
                {
                    result = wson_write(data_address, pdata, data_size, FALSE);
                }
                else // cc_mode == CC_LOAD_PKG NOTE: Assumption is that a pkg is greater than 2kb
                {
                    if (gsize == 0)     // if this is the initial buffer
                    {
                        // parse package to get size and CRC
                        parse_pkg(pdata, gppkg);
                        gsize = gppkg->pkg_info.pkg_size;
                        gcrc  = gppkg->pkg_info.pkg_crc;
                        // adjust fw_size & write the first buffer to spi flash
                        fw_size = gsize;
                    }
                    result = wson_write(data_address, pdata, data_size, FALSE);
                }

                data_address += data_size;
                if (result != SUCCESS)
                {
                    cc_print(&g_cc_handle, "ERR WR\n");
                }
            }
            else if (rx_count >= fw_size)
            {
                data_size = rx_idx;
                pdata     = &rx_buffer[buf_idx][0];
                if (cc_mode == CC_LOAD_INT)
                {
                    result = b2_write_fw(pdata, data_address, data_size);
                }
                else // cc_mode == CC_LOAD_SPI or CC_LOAD_PKG
                {
                    result = wson_write(data_address, pdata, data_size, FALSE);
                }
                if (result != SUCCESS)
                {
                    // Not much we can do here...
                    cc_print(&g_cc_handle, "ERR WR\n");
                }
            }
            if (rx_count >= fw_size)    // If all the data received
            {
                cc_print(&g_cc_handle, "\r%d\nWrote %d bytes to 0x%x\n", rx_count, rx_count, fw_address);
                if (cc_mode == CC_LOAD_PKG) // Do a CRC of the package in spi flash
                {
                    if (gcrc == calc_crc_pkg (fw_address, gsize))
                    {
                        cc_print(&g_cc_handle, "CRC OK\n");
                        set_pkg_update_flag(gppkg);     // and update it now
                    }
                    else
                    {
                        cc_print(&g_cc_handle, "** ERR CRC\n");
                    }
                }
                delay(500000);
                init_cmd_mode(); // Switch to cc command mode
                return;
            }
            // b2_send_data (echo_buf, 1);
            sprintf(rxc, "\r%d", rx_count);
            b2_send_data((uint8_t *)rxc, strlen(rxc));
        } // CC_DATA
    }
}   // b2_task()


// Set our success flag if needed
void set_success(void)
{
    uint8_t* b2hsuccess = &b2h[HDR_SUCCESS];

    if (*b2hsuccess == FLAG_CLR)
    {
        set_flag(b2hsuccess);
    }
}

static int app_attempts()
{
    int attempts = 0;
    int i;

    for (i = HDR_ATTMPT1; i < HDR_ATTMPT3; i++)
    {
        if (appHdr[i] == FLAG_SET)
        {
            attempts++;
            continue;
        }
        break;
    }
    return attempts;
}

// Set app attempt flag
void set_app_attempt_flag()
{
    int i;

    for (i = HDR_ATTMPT1; i < HDR_ATTMPT3; i++)
    {
        if (appHdr[i] == FLAG_SET)
        {
            continue;
        }
        else
        {
            set_flag(&appHdr[i]);
        }
        break;
    }
}

// Read the FWU SPI data flags
void init_fwu_spi_data()
{
    corona_err_t result;

    result = b2_read_spi((uint8_t *)&fwu_spi_data, EXT_FLASH_FWUPDATE_INFO_ADDR, sizeof(fwu_spi_data));
    if (result != SUCCESS)
    {
        memset(fwu_spi_data, 0xff, sizeof(fwu_spi_data)); // Clear all flags since read failed
    }
}

// Check if the app needs an update
// Update if the update flag is set or if the header is invalid
void ck_app_update()
{
    fwu_pkg_t    package;
    fwu_pkg_t*   ppkg               = &package;
    uint32_t     app_image_offset;
    uint32_t     image_size;
    int          spi_pkg;
    uint32_t     spi_start_addrs;
    uint32_t     target_address     = APP_LOAD_ADDRS;
    uint32_t     app_crc;

    // If the app header is valid and the update flag is not set, no update needed
    if ((cksum(appHdr) == 0)             &&
        (appHdr[HDR_UPDATE] != FLAG_SET) &&
        (appHdr[HDR_BAD]    != FLAG_SET) &&
        (fwu_spi_data.boot_flags[FWU_APP_OFFSET])
       )
    {
        return; // No need to update
    }

    // Check if there is a valid app image in spi flash
    if ((spi_pkg = find_image(ppkg, PKG_CONTAINS_APP)) < 0)
    {
        return; // no app image available
    }
    // verify image size will fit
    if (package.app_img.image_size > MAX_APP_IMAGE_SIZE)
    {
        cc_print(&g_cc_handle, "ERR app sz 0x%x (%u)\n", package.app_img.image_size, package.app_img.image_size);
        error_abort++;
        return;
    }
    spi_start_addrs     = spi_0_start_addrs;
    if (spi_pkg == 1)
    {
        spi_start_addrs = spi_1_start_addrs;
    }
    // Figure out where the app image starts
    // package->pkg_info.pkg_raw_payload is the offset from the start of the package to the first image
    // package->app_img->image_offset is the offset of the app image from the pkg_raw_payload
    app_image_offset = package.pkg_info.pkg_raw_payload + package.app_img.image_offset;

    // get the size of the image
    image_size = package.app_img.image_size;

    b2_load_image_from_spi_flash(spi_start_addrs, app_image_offset, target_address, image_size);
    // Verify the image
    if ((app_crc = calc_crc_int(target_address, package.app_img.image_size)) != package.app_img.image_crc)
    {
        cc_print(&g_cc_handle, "CRC ERR 0x%x\n", app_crc);
        error_abort++;
        // someday: If this was spi_pkg 0, now try spi_pkg 1
    }
    else
    {
        // clear spi data flag = non-zero
        fwu_spi_data.boot_flags[FWU_APP_OFFSET] = TRUE;
        //  update the spi update flags
        wson_write(EXT_FLASH_FWUPDATE_INFO_ADDR, (uint8_t *)&fwu_spi_data, sizeof(fwu_spi_data), TRUE);
    }
}   // ck_app_update()


// Check if sibling boot2 needs an update
void ck_boot2_update()
{
    fwu_pkg_t       package;
    fwu_pkg_t*      ppkg               = &package;
    contents_mask_t boot2_image_mask;
    fwu_bin_t*      boot2x_img;
    corona_err_t    result;
    uint32_t        boot2_image_offset;
    uint32_t        image_size;
    int             spi_pkg;
    uint32_t        spi_start_addrs;
    uint32_t        target_address;
    uint32_t        boot2_crc;
    uint8_t         fwu_update_flag;

    // Determine who the other boot2 is...
    if (b2h == (uint8_t *)B2A_HDR)
    {
        other_b2h        = (uint8_t *)B2B_HDR;   // Other is Boot2B
        boot2_image_mask = PKG_CONTAINS_BOOT2B;
        boot2x_img       = &package.boot2b_img;
        target_address   = B2B;

        fwu_update_flag = fwu_spi_data.boot_flags[FWU_BOOT2B_OFFSET];
        // fwu_spi_data.boot_flags[FWU_BOOT2B_OFFSET] = FALSE;
    }
    else
    {
        other_b2h        = (uint8_t *)B2A_HDR;     // Other is Boot2A
        boot2_image_mask = PKG_CONTAINS_BOOT2A;
        boot2x_img       = &package.boot2a_img;
        target_address   = B2A;

        fwu_update_flag = fwu_spi_data.boot_flags[FWU_BOOT2A_OFFSET];
        // fwu_spi_data.boot_flags[FWU_BOOT2A_OFFSET] = FALSE;
    }

    // If the boot2 header is valid and the update flag is not set, no update needed
    if ((cksum(other_b2h) == 0) &&
        (other_b2h[HDR_UPDATE] != FLAG_SET) &&
        (fwu_update_flag)
       )
    {
        return; // No need to update
    }

    // Check if there is a valid boot2 image in spi flash
    if ((spi_pkg = find_image(ppkg, boot2_image_mask)) < 0)
    {
        return; // no app image available
    }
    spi_start_addrs     = spi_0_start_addrs;
    if (spi_pkg == 1)
    {
        spi_start_addrs = spi_1_start_addrs;
    }
    // Figure out where the boot2 image starts
    // package->pkg_info.pkg_raw_payload is the offset from the start of the package to the first image
    // package->app_img->image_offset is the offset of the app image from the pkg_raw_payload
    boot2_image_offset = package.pkg_info.pkg_raw_payload + boot2x_img->image_offset;

    // get the size of the image
    image_size = boot2x_img->image_size;
    if (image_size > MAX_BOOT2_IMAGE_SIZE)
    {
        cc_print(&g_cc_handle, "ERR boot2 sz %x\n", image_size);
        error_abort++;
        return;
    }

    b2_load_image_from_spi_flash(spi_start_addrs, boot2_image_offset, target_address, image_size);
    // Verify the image
    if ((boot2_crc = calc_crc_int(target_address, boot2x_img->image_size)) != boot2x_img->image_crc)
    {
        cc_print(&g_cc_handle, "CRC ERR 0x%x\n", boot2_crc);
        error_abort++;
        // If this was spi_pkg 0, now try spi_pkg 1
    }
    else
    {
        // Clear the update flags = non-zero
        // Note: Update only one boot2 image per pkg load
        fwu_spi_data.boot_flags[FWU_BOOT2A_OFFSET] = TRUE;
        fwu_spi_data.boot_flags[FWU_BOOT2B_OFFSET] = TRUE;
        wson_write( EXT_FLASH_FWUPDATE_INFO_ADDR, (uint8_t *)&fwu_spi_data, sizeof(fwu_spi_data), TRUE);
        update_reset++;
        pmem_flag_set(PV_FLAG_FWU, TRUE);   // boot2 was updated, need to go thru new boot2 before going to app.
    }
}   // ck_boot2_update()


// Check if boot1 needs an update
void ck_boot1_update()
{
    fwu_pkg_t    package;
    fwu_pkg_t*   ppkg               = &package;
    corona_err_t result;
    uint32_t     binary_image_offset;
    uint32_t     image_size;
    int          spi_pkg;
    uint32_t     spi_start_addrs;
    uint32_t     target_address     = BT1;
    uint32_t     image_crc;
    uint8_t*     boot1Hdr           = (uint8_t*)BT1_HDR;

    // If the app header is valid and the update flag is not set, no update needed
    if ((boot1Hdr[HDR_UPDATE] != FLAG_SET) &&
        (fwu_spi_data.boot_flags[FWU_BOOT1_OFFSET]))
    {
        return; // No need to update
    }
    // Check if there is a valid app image in spi flash
    if ((spi_pkg = find_image(ppkg, PKG_CONTAINS_BOOT1)) < 0)
    {
        return; // no app image available
    }
    spi_start_addrs     = spi_0_start_addrs;
    if (spi_pkg == 1)
    {
        spi_start_addrs = spi_1_start_addrs;
    }

    // Figure out where the app image starts
    // package->pkg_info.pkg_raw_payload is the offset from the start of the package to the first image
    // package->app_img->image_offset is the offset of the app image from the pkg_raw_payload
    binary_image_offset = package.pkg_info.pkg_raw_payload + package.boot1_img.image_offset;

    // get the size of the image
    image_size = package.boot1_img.image_size;

    // Check the CRC of the binary image in spi flash
    if ((image_crc = calc_crc_spi_image(spi_start_addrs, binary_image_offset, image_size)) != package.boot1_img.image_crc)
    {
        // cc_print(&g_cc_handle, "spi image crc error: 0x%08X, expected: 0x%08X\n", image_crc, package.boot1_img.image_crc);
        return;
    }
    // cc_print(&g_cc_handle, "spi image CRC checked OK\n");
    while (1)   // This is boot1 being updated. We're dead if it doesn't work, so try forever...
    {
        b2_load_image_from_spi_flash(spi_start_addrs, binary_image_offset, target_address, image_size);

        // Verify the image
        if ((image_crc = calc_crc_int(target_address, package.boot1_img.image_size)) != package.boot1_img.image_crc)
        {
            // todo: If this was spi_pkg 0, now try spi_pkg 1
            // possible alternative would be to copy the current boot1 image to spi flash before update attempt
            // and use it if this fails
            // but for now, retry forever
            // cc_print(&g_cc_handle, "boot1 image crc error: 0x%08X, expected: 0x%08X\n", image_crc, package.boot1_img.image_crc);
            delay(500000);
            continue;   // This is boot1 being updated. We're dead if it doesn't work...
        }
        break; // success
    }
    // clear spi data flag = non-zero
    fwu_spi_data.boot_flags[FWU_BOOT1_OFFSET] = TRUE;
    //  update the spi update flags
    wson_write(EXT_FLASH_FWUPDATE_INFO_ADDR, (uint8_t *)&fwu_spi_data, sizeof(fwu_spi_data), TRUE);

    // cc_print(&g_cc_handle, "boot1 image update successful!!\n");
    delay(500000);
}   // ck_boot1_update()

// Check if the patch needs an update
// Update if the update flag is set or if the header is invalid
void ck_patch_update(boolean isBase)
{
    fwu_pkg_t    package;
    fwu_pkg_t*   ppkg               = &package;
    uint32_t     patch_offset;
    uint32_t     patch_size;
    int          spi_pkg;
    uint32_t     spi_start_addrs;
    uint32_t     target_address;
    uint32_t     patch_crc;
    fwu_patch_t  *patch;

    // isBase == 1 means BasePatch
    // isBase == 0 means LowEnergy
    if (isBase)
    {
        target_address = EXT_FLASH_BASEPATCH_ADDR;
        patch = &package.basepatch_patch;
        // Check if there is a valid patch image in spi flash
        if ((spi_pkg = find_image(ppkg, PKG_CONTAINS_BASEPATCH)) < 0)
        {
            return; // no patch image available
        }
    }
    else
    {
        target_address = EXT_FLASH_LOWENERGY_ADDR;
        patch = &package.lowenergy_patch;
        if ((spi_pkg = find_image(ppkg, PKG_CONTAINS_LOWENERGY)) < 0)
        {
            return; // no patch image available
        }
    }

    spi_start_addrs     = spi_0_start_addrs;
    if (spi_pkg == 1)
    {
        spi_start_addrs = spi_1_start_addrs;
    }

    // Check if patch already in patch location matches package patch
    if ((patch_crc = calc_crc_spi_image(target_address, 0, patch->patch_size)) == patch->patch_crc)
    {
        cc_print(&g_cc_handle, "Patch OK\n");
        return;
    }

    // Figure out where the patch image starts
    // package->pkg_info.pkg_raw_payload is the offset from the start of the package to the first image
    // package->patch_img->image_offset is the offset of the patch image from the pkg_raw_payload
    patch_offset = package.pkg_info.pkg_raw_payload + patch->patch_offset;

    // get the size of the patch
    patch_size = patch->patch_size;

    b2_transfer_patch(spi_start_addrs, patch_offset, target_address, patch_size);

    // Verify the patch
    if ((patch_crc = calc_crc_spi_image(target_address, 0, patch_size)) != patch->patch_crc)
    {
        // Damn, now what? Repeat 3 times and give up?
        // If this was spi_pkg 0, now try spi_pkg 1
        cc_print(&g_cc_handle, "ERR Patch\n");

        // Erase the target space
        assert( SUCCESS == b2_erase_spi_flash(target_address, patch_size) );
    }
    else
    {
        cc_print(&g_cc_handle, "Patch Update OK\n");
    }
}   // ck_patch_update()

// Calculate the CRC for an image in internal flash
static uint32_t calc_crc_int(uint32_t address, uint32_t size)
{
    uint32_t target_address = address;
    uint8_t  *t_ptr         = (uint8_t *)target_address;
    uint32_t image_size     = size;
    uint32_t the_crc        = 0;
    int      i;

    if (!MyCRC1_Ptr)
    {
        MyCRC1_Ptr = CRC1_Init(NULL);  /* Initialization of CRC1 component */
    }
    CRC1_ResetCRC(MyCRC1_Ptr);

    for (i = 0; i < image_size; i++)
    {
        the_crc = CRC1_GetCRC8(MyCRC1_Ptr, *t_ptr++);
    }
    // cc_print(&g_cc_handle, "the_crc = 0x%08X\n", the_crc);
    return the_crc;
}


// Calculate the CRC for a package in spi flash
uint32_t calc_crc_pkg (int spi_address, int pkg_size)
{
    corona_err_t result;
    uint32_t     spi_addrs = spi_address;
    uint32_t     isize  = pkg_size;
    uint32_t     the_crc   = 0;
    uint8_t      *prx_buf = &rx_buffer[1][0];
    uint8_t      *t_ptr   = prx_buf;
    int          buf_size = RX_BUF_SIZE;
    int          d_size;
    int          i = 0;

    if (!MyCRC1_Ptr)
    {
        MyCRC1_Ptr = CRC1_Init(NULL);  /* Initialization of CRC1 component */
    }
    CRC1_ResetCRC(MyCRC1_Ptr);

    // Do the initial read
    result = b2_read_spi(prx_buf, spi_addrs, buf_size);
    if (result != SUCCESS)
    {
        return;                     // not much we can do about this
    }
    // Replace the package CRC and Signature with 0x00's for the CRC calculation
    memset(&t_ptr[PKG_CRC_OFFSET], 0, 8);

    // Do the CRC for the first buffer read
    for (i = 0; i < buf_size; i++)
    {
        the_crc = CRC1_GetCRC8(MyCRC1_Ptr, *t_ptr++);
    }
    // Adjust sizes and pointers
    isize     -= buf_size;
    spi_addrs += buf_size;
    t_ptr      = prx_buf;

    // Do CRC for the rest of the package
    while (isize > 0)
    {
        // cc_print(&g_cc_handle, "spi_addrs = 0x%X\n", spi_addrs);
        // cc_print(&g_cc_handle, "isize = %d\n", isize);
        // Read the next block of data
        result = b2_read_spi(prx_buf, spi_addrs, buf_size);
        if (result != SUCCESS)
        {
            return;                     // not much we can do about this
        }
        // Adjust the data size to read if near the end
        if (isize >= buf_size)
        {
            d_size = buf_size;
        }
        else
        {
            d_size = isize;
        }
        // Compute the CRC for this block of data
        for (i = 0; i < d_size; i++)
        {
            the_crc = CRC1_GetCRC8(MyCRC1_Ptr, *t_ptr++);
        }
        // Adjust the sizes and pointers for next block
        isize     -= d_size;
        spi_addrs += buf_size;
        t_ptr      = prx_buf;
    }
    // cc_print(&g_cc_handle, "the_crc = 0x%08X\n", the_crc);
    return the_crc;
}   // calc_crc_pkg()


// Calculate the CRC of a binary image in spi flash
static uint32_t calc_crc_spi_image(uint32_t spi_start_addrs, uint32_t binary_image_offset, uint32_t image_size)
{
    corona_err_t result;
    uint32_t     spi_addrs  = spi_start_addrs;
    uint32_t     isize      = image_size;
    uint32_t     the_crc    = 0;
    uint32_t     xoffset;
    uint8_t      *prx_buf   = &rx_buffer[1][0];
    uint8_t      *pdata     = prx_buf;
    int          buf_size   = RX_BUF_SIZE;
    int          d_size;
    int          i = 0;

    if (!MyCRC1_Ptr)
    {
        MyCRC1_Ptr = CRC1_Init(NULL);  /* Initialization of CRC1 component */
    }
    CRC1_ResetCRC(MyCRC1_Ptr);

    // Do the initial read
    result = b2_read_spi(prx_buf, spi_addrs, buf_size);
    if (result != SUCCESS)
    {
        return result;                     // not much we can do about this
    }

    if (binary_image_offset > buf_size)
    {
        xoffset = (binary_image_offset/buf_size) * buf_size;    // offset to the 2k block containing the start of the image
        spi_addrs += xoffset;
        binary_image_offset = binary_image_offset % buf_size;
    }
    // Do the initial read, have to adjust to the image offset
    result = b2_read_spi(prx_buf, spi_addrs, buf_size);
    if (result != SUCCESS)
    {
        return result;                     // not much we can do about this
    }
    // Start the calculation, adjusting for the beginning of the binary image
    pdata = prx_buf + binary_image_offset;
    d_size = buf_size - binary_image_offset;  // what if d_size is less than remaining buffer??? not likely, but...

    // Do the CRC for the first buffer read
    for (i = 0; i < d_size; i++)
    {
        the_crc = CRC1_GetCRC8(MyCRC1_Ptr, *pdata++);
    }
    // Adjust sizes and pointers
    isize     -= d_size;
    spi_addrs += buf_size;
    pdata      = prx_buf;

    // Do CRC for the rest of the package
    while (isize > 0)
    {
        // cc_print(&g_cc_handle, "spi_addrs = 0x%X\n", spi_addrs);
        // cc_print(&g_cc_handle, "isize = %d\n", isize);
        // Read the next block of data
        result = b2_read_spi(prx_buf, spi_addrs, buf_size);
        if (result != SUCCESS)
        {
            return;                     // not much we can do about this
        }
        // Adjust the data size to read if near the end
        if (isize >= buf_size)
        {
            d_size = buf_size;
        }
        else
        {
            d_size = isize;
        }
        // Compute the CRC for this block of data
        for (i = 0; i < d_size; i++)
        {
            the_crc = CRC1_GetCRC8(MyCRC1_Ptr, *pdata++);
        }
        // Adjust the sizes and pointers for next block
        isize     -= d_size;
        spi_addrs += buf_size;
        pdata      = prx_buf;
    }
    // cc_print(&g_cc_handle, "the_crc = 0x%08X\n", the_crc);
    return the_crc;
}   // calc_crc_spi_image

// A package has been loaded to spi flash
// Set the update flag for the image in the package
// Then do the update
//
static void set_pkg_update_flag(fwu_pkg_t* gppkg)
{
    int x;
    int image_found = 0;
    (uint8_t*) other_b2h;   // There is a global by the same name that has been initialized, but...

    if (gppkg->pkg_info.pkg_contents & PKG_CONTAINS_APP)
    {
        image_found++;
        cc_print(&g_cc_handle, "Updating APP\n");
        set_flag((uint8_t*) APP_HDR + HDR_UPDATE);
        ck_app_update();
        if (error_abort) {return;}
    }
    if ((gppkg->pkg_info.pkg_contents & PKG_CONTAINS_BOOT2A) &&
        (gppkg->pkg_info.pkg_contents & PKG_CONTAINS_BOOT2B))
    {
        image_found++;
        if (b2h == (uint8_t *)B2A_HDR)        // If we're boot2A
        {
            other_b2h = (uint8_t *)B2B_HDR;   // Other is Boot2B
            x = 'B';
        }
        else
        {
            other_b2h = (uint8_t *)B2A_HDR;   // Other is Boot2A
            x = 'A';
        }
        cc_print(&g_cc_handle, "Updating Boot2%c\n", x);
        set_flag (&other_b2h[HDR_UPDATE]);
        delay(500000);

        ck_boot2_update();
        if (error_abort) {return;}
    }
    if (gppkg->pkg_info.pkg_contents & PKG_CONTAINS_BOOT1)
    {
        image_found++;
        cc_print(&g_cc_handle, "Updating Boot1\n");
        set_flag((uint8_t*) BT1_HDR + HDR_UPDATE);
        ck_boot1_update();
    }
    if (gppkg->pkg_info.pkg_contents & PKG_CONTAINS_BASEPATCH)
    {
        image_found++;
        cc_print(&g_cc_handle, "Updating BasePatch\n");
        ck_patch_update(TRUE);
    }
    if (gppkg->pkg_info.pkg_contents & PKG_CONTAINS_LOWENERGY)
    {
        image_found++;
        cc_print(&g_cc_handle, "Updating LE Patch\n");
        ck_patch_update(FALSE);
    }
    if (!image_found)
    {
        cc_print(&g_cc_handle, "No img\n");
        return;
    }
    cc_print(&g_cc_handle, "Update OK. Press Enter\n");
}   // set_pkg_update_flag

// Read the package tlv's in spi flash loc0
// if desired image is not found, try the package in loc1
static int find_image(fwu_pkg_t* ppkg, contents_mask_t image_mask)
{
    int             spi_pkg = -1;
    int             i;
    corona_err_t    result;

    for (i = 0; i < 2; i++)
    {
        memset(ppkg, 0, sizeof(fwu_pkg_t));
        result = b2_read_pkg(ppkg, i);
        if (result != SUCCESS)
        {
            // cc_print(&g_cc_handle, "ck_app_update ERROR1\n");
            continue;
        }
        // check if the package contains an app image
        if (!(ppkg->pkg_info.pkg_contents & image_mask))
        {
            // cc_print(&g_cc_handle, "No App image in package\n");
            continue;
        }
        spi_pkg = i;
        break;
    }
    return spi_pkg;
}

// Update target with image from spi flash
//
static void b2_load_image_from_spi_flash (uint32_t spi_start_addrs, uint32_t app_image_offset,
                                          uint32_t target_address,  uint32_t image_size)
{
    corona_err_t result;
    uint32_t     spi_addrs = spi_start_addrs; // location to start reading spi data
    uint32_t     t_address = target_address;  // internal flash address for image
    uint32_t     isize     = image_size;
    uint32_t     d_size;                      // amount of data read from spi flash
    uint8_t      *pdata;                      // pointer to spi data in rx_buffer
    uint8_t      *prx_buf = &rx_buffer[1][0];
    int          buf_size = RX_BUF_SIZE;
    uint32_t      xoffset;

    // cc_print(&g_cc_handle, "Starting b2_load_image_from_spi_flash...\n");

    // Erase the target space
    result = b2_erase_flash(target_address, image_size);
    if (result != SUCCESS)
    {
        return;
    }
    if (app_image_offset > buf_size)
    {
        xoffset = (app_image_offset/buf_size) * buf_size;    // offset to the 2k block containing the start of the image
        spi_addrs += xoffset;
        app_image_offset = app_image_offset % buf_size;
    }
    // Do the initial read, have to adjust to the image offset before writing to int flash
    result = b2_read_spi(prx_buf, spi_addrs, buf_size);
    if (result != SUCCESS)
    {
        return;                     // not much we can do about this
    }
    // Do the initial write, adjusting for the beginning of the binary image
    pdata = prx_buf + app_image_offset;
    d_size = buf_size - app_image_offset;  // what if d_size is less than remaining buffer??? not likely, but...

    // cc_print(&g_cc_handle, "prx_buf = 0x%X\n", prx_buf);
    // cc_print(&g_cc_handle, "initial pdata = 0x%X\n", pdata);
    // cc_print(&g_cc_handle, "d_size = %d\n", d_size);
    // cc_print(&g_cc_handle, "t_address = 0x%X\n", t_address);
    result = b2_write_fw(pdata, t_address, d_size);
    if (result != SUCCESS)
    {
        return;
    }

    t_address += d_size;
    isize     -= d_size;
    spi_addrs += buf_size;
    pdata      = prx_buf;

    // Now read and write the rest of the image
    while (isize > 0)
    {
        // cc_print(&g_cc_handle, "spi_addrs = 0x%X\n", spi_addrs);
        result = b2_read_spi(prx_buf, spi_addrs, buf_size);
        if (result != SUCCESS)
        {
            return;                     // not much we can do about this
        }
        if (isize >= buf_size)
        {
            d_size = buf_size;
        }
        else
        {
            d_size = isize;
        }
        // cc_print(&g_cc_handle, "isize = %d\n", isize);
        // cc_print(&g_cc_handle, "d_size = %d\n", d_size);
        // cc_print(&g_cc_handle, "t_address = 0x%X\n", t_address);

        result = b2_write_fw(pdata, t_address, d_size);
        if (result != SUCCESS)
        {
            return;
        }
        // Adjust addresses and sizes
        t_address += d_size;
        spi_addrs += d_size;
        isize     -= d_size;
    }
    // cc_print(&g_cc_handle, "Update Finished: Wrote %d bytes to address 0x%X\n", image_size, target_address);
}

// Transfer patch data from package to SPI flash location
//
static void b2_transfer_patch (uint32_t spi_start_addrs, uint32_t patch_offset, uint32_t target_address, uint32_t patch_size)
{
    corona_err_t result;
    uint32_t     spi_addrs = spi_start_addrs + patch_offset; // location to start reading spi data
    uint32_t     t_address = target_address;  // spi flash address for patch
    uint32_t     psize     = patch_size;
    uint8_t      *prx_buf  = &rx_buffer[1][0];
    uint8_t      *pdata    = prx_buf;         // pointer to spi data in rx_buffer
    uint32_t     buf_size  = RX_BUF_SIZE;
    uint32_t     d_size    = buf_size;        // amount of data read from spi flash

    // Erase the target space
    assert( SUCCESS == b2_erase_spi_flash(target_address, patch_size) );

    // Loop through reading/writing patch data from/to SPI flash
    // in buf_size increments until the entire patch is done
    while (psize > 0)
    {
        assert( SUCCESS == b2_read_spi(prx_buf, spi_addrs, buf_size) );

        if (psize >= buf_size)
        {
            d_size = buf_size;
        }
        else
        {
            d_size = psize;
        }

        assert( SUCCESS == b2_write_spi(pdata, t_address, d_size) );

        // Adjust addresses and sizes
        t_address += d_size;
        spi_addrs += d_size;
        psize     -= d_size;
    }
}


// Set header flag
void set_flag(uint8_t *flagp)
{
    uint32_t flag_word;
    uint32_t flag_set;
    uint32_t flag_clr;
    uint32_t u32Addrs;
    uint32_t flag_shift;
    int      result;

    u32Addrs = (uint32_t)((uint32_t)flagp & (uint32_t)0xFFFFFFFC); // Get 4 byte address containing this flag

    flag_shift = ((uint32_t)flagp & (uint32_t)0x00000003);         // Byte position of flag
    flag_set   = FLAG_SET << (flag_shift * 8);
    flag_clr   = ~(FLAG_CLR << (flag_shift * 8));

    flag_word = *(uint32_t *)u32Addrs;

    flag_word &= flag_clr; // Clear the flag bits
    flag_word |= flag_set; // Set the flag

    // write to boot2 header
    result = b2_write_fw((uint8_t *)&flag_word, u32Addrs, 4);
}


void goto_app()
{
    int (*gtoApp)(void);
    
    // float the UID_EN pin for best power results and to avoid corosion.
    GPIOA_PDDR &= ~GPIO_PDDR_PDD(UID_EN_MASK);  // make it an input.
    GPIOA_PDOR &= ~UID_EN_MASK;
    PORTA_PCR12 &= ~PORT_PCR_MUX_MASK;

    if (update_reset)
    {
        // request software reset
        SCB_AIRCR = SCB_AIRCR_VECTKEY(0x5FA)| SCB_AIRCR_SYSRESETREQ_MASK;
        // wait for reset to occur
        while(1);
    }
    
    // Check success flag
    if (appHdr[HDR_SUCCESS] != FLAG_SET)
    {
        if (app_attempts() == 3)
        {
            set_flag(&appHdr[HDR_BAD]);     // 3 strikes, you're out!!
            ck_app_update();                // Try update from backup
        }
        else
        {
            set_app_attempt_flag();
        }
    }
    pmem_flag_set(PV_FLAG_FWU, FALSE);  // Was set to force using updated boot2 after boot2 fw update reboot

    usb_reset();
    
    gtoApp = (int (*)()) * (int *)(APP_ENTRY); // get the entry address (*0x22004)
    gtoApp();                                  // Jump to the app
}


void b2_recv_init()
{
    if (b2h == (uint8_t *)B2A_HDR)
    {
        prompt = "\n\rA] ";
    }
    else
    {
        prompt = "\n\rB] ";
    }
    init_cmd_mode();
}


static void init_cmd_mode()
{
    rx_idx   = 0;
    rx_rcvd  = 0; // Data received flag
    rx_count = 0;
    prx_buf  = &rx_buffer[0][0];
    cc_mode  = CC_COMMAND;
    memset(g_console_buf, 0, CONSOLE_BUF_SZ);
    memset(&rx_buffer[0][0], 0, CONSOLE_BUF_SZ);
    b2_send_data((uint8_t *)"\r", 2);
}


static void init_spi_pkg_mode()
{
    cc_mode = CC_LOAD_PKG; // Change to spi load pkg mode to receive the image
    gsize = 0;
    gcrc  = 0;
    init_data_mode();
}


static void init_spi_data_mode()
{
    cc_mode = CC_LOAD_SPI; // Change to spi flash data mode to receive the image
    init_data_mode();
}


static void init_int_flash_data_mode()
{
    cc_mode = CC_LOAD_INT; // Change to internal flash data mode to receive the image
    init_data_mode();
}


static void init_data_mode()
{
    prx_buf      = &rx_buffer[0][0];
    data_address = fw_address;
    rx_idx       = 0;
    rx_count     = 0;
    buf_idx      = 0;
    fbuf         = 0;
    full_buffer  = 0;
}


// Receive data sent via USB
void b2_recv_data(uint8_t *rcv_buf, uint8_t rcv_len)
{
    int rlen;

    if (rcv_len == 0)
    {
        return;
    }

    rx_rcvd++;            // new data received
    rx_count += rcv_len;  // Count total bytes received

    if (rcv_len < RX_BUF_SIZE - rx_idx)
    {
        memcpy(&prx_buf[rx_idx], rcv_buf, rcv_len);
        rx_idx      += rcv_len;
        g_recv_size += rcv_len;
    }
    else  // received data will will fill or overrun buffer
    {
        rlen = RX_BUF_SIZE - rx_idx; // remaining length in buffer
        memcpy(&prx_buf[rx_idx], rcv_buf, rlen); // fill the buffer
        full_buffer++;
        fbuf = buf_idx;
        // rotate to the next buffer
        rx_idx = 0;
        buf_idx++;
        buf_idx %= NUM_RX_BUF;
        prx_buf  = &rx_buffer[buf_idx][0];

        // copy remaining data into new buffer
        rx_idx = rcv_len - rlen;
        if (rx_idx > 0)
        {
            memcpy(&prx_buf[0], &rcv_buf[rlen], rx_idx);
        }
    }
}   // b2_recv_data


// Load fw package to spi flash
// Parse the header to get the size and CRC
// spi_loc == 0 for spi addrs 0, 1 for spi_addrs 0x80000
// Tell them to send the file,
//  then parse the package info to get the size and CRC
//  load that buffer to spi flash,
//  then all the rest of the package,
//  and do a CRC to verify the load
//  cc cmd lpkg
corona_err_t b2_load_pkg (int spi_loc)
{
    int         result;
    uint32_t    spi_addrs;
    uint32_t    spi_size = EXT_FLASH_APP_IMG_NUM_SECTORS * EXT_FLASH_64KB_SZ;

    spi_addrs = spi_0_start_addrs;
    if (spi_loc == 1)
    {
        spi_addrs = spi_1_start_addrs;
    }
    // We don't know the size yet, so just erase the complete location
    cc_print(&g_cc_handle, "ERASE\n");
    if ((result = b2_erase_spi_flash(spi_addrs, spi_size)) != SUCCESS)
    {
        cc_print(&g_cc_handle, "ERR ers\n");
        return result;
    }
    // The flash area to be used is erased and ready to be loaded
    cc_print(&g_cc_handle, "ERASE OK\n");
    cc_print(&g_cc_handle, "Send pkg\n");
    fw_address = spi_addrs;
    fw_size  = RX_BUF_SIZE * 2; // placeholder until we parse the pkg header and get the pkg size
    init_spi_pkg_mode();
}


// Load fw package to SPI flash
//
corona_err_t b2_load_spi(uint32_t start_addrs, uint32_t image_size)
{
    int result;

    fw_address = start_addrs;
    fw_size    = image_size;

    if ((result = b2_erase_spi_flash(start_addrs, image_size)) != SUCCESS)
    {
        cc_print(&g_cc_handle, "ERR ERASE\n");
        return result;
    }
    // The flash area to be used is erased and ready to be loaded
    cc_print(&g_cc_handle, "Erase OK\n");
    cc_print(&g_cc_handle, "Send img\n");
    init_spi_data_mode();

    return SUCCESS;
}


// Read fwu package tlv's in spi flash
// pkg = 0 for package loaded at EXT_FLASH_APP_IMG_ADDR              0x20000
// pkg = 1 for package loaded at EXT_FLASH_APP_IMG_BKP_ADDR          0xA0000
corona_err_t b2_read_pkg(fwu_pkg_t* ppkg, int pkg)
{
    uint32_t     pkg_addrs;
    corona_err_t result;
    uint8_t      rbuf[512];

    pkg_addrs     = spi_0_start_addrs;
    if (pkg == 1)
    {
        pkg_addrs = spi_1_start_addrs;
    }

    if ((result = b2_read_spi(rbuf, pkg_addrs, sizeof(rbuf))) != SUCCESS)
    {
        // do something
        // cc_print(&g_cc_handle, "b2_read_spi ERROR\n");
        return -1;
    }
    parse_pkg(rbuf, ppkg);  // Parse the package and fill in the package structure
    return SUCCESS;
}


// Load fw image to internal flash via USB
corona_err_t b2_load_app_fw(address, numBytes)
{
    int result;

    fw_address = address;
    fw_size    = numBytes;

    if ((result = b2_erase_flash(address, numBytes)) != SUCCESS)
    {
        cc_print(&g_cc_handle, "ERR Erase\n");
        return result;
    }
    // The flash area to be used is erased and ready to be loaded
    cc_print(&g_cc_handle, "Erase OK\n");
    cc_print(&g_cc_handle, "Send img\n");
    init_int_flash_data_mode();

    return SUCCESS;
}


// erase the SPI flash sectors where this image goes
static corona_err_t b2_erase_spi_flash(uint32_t start_addrs, uint32_t esize)
{
    int          i;
    uint32_t     spi_address = start_addrs;
    uint8_t      num_sectors;
    corona_err_t status = SUCCESS;

    // Determine the number of sectors to erase
    num_sectors = esize / EXT_FLASH_64KB_SZ;

    if (esize % EXT_FLASH_64KB_SZ)
    {
        num_sectors++;
    }
    // cc_print(&g_cc_handle, "b2_erase_spi_flash sectors %d\n", num_sectors);

    for (i = 0; i < num_sectors; i++)
    {
        status = wson_erase(spi_address);
        if (status != SUCCESS)
        {
            return(CC_ERR_FLASH_ERASE);
        }
        spi_address += EXT_FLASH_64KB_SZ;
    }
    return status;
}


// Erase internal flash sectors
int b2_erase_flash(eaddress, esize)
{
    uint32_t erase_size;

    OpFinishedFlg = FALSE;

    // Set erase_size to full sectors
    erase_size = esize - (esize % SECTOR_SIZE); // round down to sector boundary
    if (esize % SECTOR_SIZE)
    {
        erase_size += SECTOR_SIZE;
    }

    f_result = FLASH1_Erase(MyFLASH_Ptr, eaddress, erase_size); // Erase sector(s)
    if (f_result != ERR_OK)
    {
        return(CC_ERR_FLASH_ERASE);
    }
    while (!OpFinishedFlg)                                          // Wait until the erase is finished
    {
        FLASH1_Main(MyFLASH_Ptr);                                   // Run the main method
    }
    if (FLASH1_GetOperationStatus(MyFLASH_Ptr) == LDD_FLASH_FAILED) // Check if the operation has successfully ended
    {
        return(CC_ERR_FLASH_ERASE);
    }
    return SUCCESS;
}


// Read from spi flash
static corona_err_t b2_read_spi(uint8_t *rbuf, uint32_t addrs, uint32_t num_bytes)
{
    return(wson_read(addrs, rbuf, num_bytes));
}


// Write data to target address in SPI flash
static corona_err_t b2_write_spi(uint8_t *pdata, uint32_t t_address, uint32_t d_size)
{
    return(wson_write(t_address, pdata, d_size, FALSE));
}


// Write data to target address in internal flash
int b2_write_fw(uint8_t *pdata, uint32_t t_address, uint32_t d_size)
{
    corona_err_t result;

    OpFinishedFlg = FALSE;
    f_result      = FLASH1_Write(MyFLASH_Ptr, pdata, t_address, d_size); // Start writing to the target

    if (f_result != ERR_OK)
    {
        return(102);
    }
    while (!OpFinishedFlg)                                          /* Wait until the data are written */
    {
        FLASH1_Main(MyFLASH_Ptr);                                   /* Run the main method */
    }
    if (FLASH1_GetOperationStatus(MyFLASH_Ptr) == LDD_FLASH_FAILED) /* Check if the operation has successfully ended */
    {
        return(103);
    }
    return SUCCESS;
}


// Send data to USB virtual com port
int b2_send_data(uint8_t *send_buf, uint16_t send_len)
{
    int status;

    status = virtual_com_send(send_buf, send_len);
    return status;
}

// Receive data from USB for CC command
void cc_rcv_cmd()
{
    static uint8_t status = 0;
    static uint8_t count  = 0;
    uint8_t        *prx   = &rx_buffer[0][0];

    if (g_recv_size)
    {
        /* Copy Received Buffer to Send Buffer */
        for (status = 0; status < g_recv_size; status++)
        {
            g_curr_send_buf[status] = prx[status];

            if (count < CONSOLE_BUF_SZ)
            {
                g_console_buf[count] = prx[status];
            }
            else
            {
                // \todo Make sure no overflow occuring on the command line for this case.
                g_console_buf[CONSOLE_BUF_SZ - 1] = '\n';
                count = CONSOLE_BUF_SZ - 1;
            }

            if ((g_console_buf[count] == '\n') || (g_console_buf[count] == '\r')) // newline was received.  Process the input
            {
                g_console_buf[count] = 0;                                         // null terminate the str

                cc_process_cmd(&g_cc_handle, (char *)&(g_console_buf[0]));        // send off the command + args to be processed.

                count = 0;
                if (cc_mode == CC_COMMAND)
                {
                    b2_send_data((uint8_t *)prompt, strlen(prompt));
                }
                return;
            }
            count++;
        }
        g_send_size = g_recv_size;
        g_recv_size = 0;

        if (g_send_size)
        {
            /* Send Data to USB Host*/
            uint8_t size = g_send_size;

            if ((g_curr_send_buf[0] != '\n') && (g_curr_send_buf[0] != '\r')) // don't let the app print newlines, we want to manage that explicitly.
            {
                status = b2_send_data(g_curr_send_buf, size);
            }
            else
            {
                status = USB_OK;
            }

            if (status != USB_OK)
            {
                /* Send Data Error Handling Code goes here */
                // printf("USB Send Error! (0x%x)\n", status);
            }
        }
    }
}

// Read the boot2 and app headers
// todo: print printable chars, otherwise, hex
void read_headers ()
{
    char        buf[32];
    char        c;
    uint8_t*    phdr;
    int         i;
    int         x;
    unsigned int PatchAddr;
    unsigned int SPI_CK;

    for (i = 0; i < 4; i++)
    {
        switch (i)
        {
            case 0:
                phdr = (uint8_t*)APP_HDR;
                break;
            case 1:
                phdr = (uint8_t*)B2A_HDR;
                break;
            case 2:
                phdr = (uint8_t*)B2B_HDR;
                break;
            case 3:
                phdr = (uint8_t*)BT1_HDR;
                break;
       }
        cc_print(&g_cc_handle, "HDR:\t0x%08x\n", phdr);
        cc_print(&g_cc_handle, "ID:\t%c%c%c VER:%x %x %x CHKSM:%x\n",
                  phdr[0],phdr[1],phdr[2],phdr[3],phdr[4],phdr[5],phdr[6]);
        cc_print(&g_cc_handle, "Success:\t%x Update:%x Attmpts:%x %x %x Bad:%x Savd:%x Used:%x\n",
                  phdr[7],phdr[8],phdr[9],phdr[10],phdr[11],phdr[12],phdr[13],phdr[14]);

        memset (buf, 0, sizeof(buf));
        for (x = 16; x < 32; x++)
        {
            c = phdr[x];
            if ((c >= ' ') && (c <= '~'))
            {
                buf[x - 16] = c;
            }
            else
            {
                break;
            }
        }
        cc_print(&g_cc_handle, "VER:\t%s\n\n", buf);

//        cc_print(&g_cc_handle, "Version Tag:%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x \n\n",
//                  phdr[16],phdr[17],phdr[18],phdr[19],phdr[20],phdr[21],phdr[22],phdr[23],
//                  phdr[24],phdr[25],phdr[26],phdr[27],phdr[28],phdr[29],phdr[30],phdr[31]);
    }

    for (i = 0; i < 2; i++)
    {
        switch (i)
        {
            case 0:
                PatchAddr = EXT_FLASH_BASEPATCH_ADDR;
                break;
            case 1:
                PatchAddr = EXT_FLASH_LOWENERGY_ADDR;
                break;
        }

        PatchAddr += 6;
        assert( ERR_OK == wson_read(PatchAddr, (unsigned char *) &SPI_CK, 4) );

        cc_print(&g_cc_handle, "%s Patch: %s\n", (i ? "LE" : "BASE"), (( SPI_CK != PATCH_SPI_MAGIC_NUM ) ? "<MISSING>" : "OK"));
    }

}
// Perform a checksum over the first 6 bytes of a header
// 7th byte is the checksum
// Returns 0 for a successful checksum
uint8_t cksum(uint8_t *hdr)
{
    int     i;
    uint8_t hdrcksum = 0;

    for (i = 0; i < 7; i++)
    {
        hdrcksum += hdr[i];
    }
    // printf ("hdrcksum = 0x%x\n", hdrcksum);
    return(hdrcksum);
}




////////////////////////////////////////////////////////////////////////////////
// End of file
////////////////////////////////////////////////////////////////////////////////
//! @}
