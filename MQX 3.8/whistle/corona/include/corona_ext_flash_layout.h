/*
 * corona_ext_flash_layout.h
 * Defines the memory map layout of the External Flash on Corona.
 *
 *  Created on: Mar 29, 2013
 *      Author: Ernie
 */

#ifndef CORONA_EXT_FLASH_LAYOUT_H_
#define CORONA_EXT_FLASH_LAYOUT_H_

/*
 *     The WSON/Spansion 32MB Flash has 510 x 64KB, and 32 x 4KB sectors.
 *     By default, Configuration Register 1 TBPARM is 0, which means the 4KB sectors
 *      start at the bottom (low address).
 */
#define EXT_FLASH_PAGE_SIZE         256

#define EXT_FLASH_4KB_SZ            0x1000  //4096   // First 32 sectors are 4KB large.
#define EXT_FLASH_NUM_4KB_SECTORS   32
#define EXT_FLASH_4KB_REGION        0x0

#define EXT_FLASH_64KB_SZ           0x10000 //65536  // Last 510 sectors are 64KB large.
#define EXT_FLASH_NUM_64KB_SECTORS  510
#define EXT_FLASH_64KB_REGION       0x20000

#define EXT_FLASH_REGION_END        0x1FFFFFF       // last byte address in SPI Flash.
#define EXT_FLASH_BOGUS_ADDR        0xFEEDBEEF      // an address guaranteed to be bogus as it related to Spansion SPI Flash.

/*
 *     *** Program and Erase Rates for the Spansion Chip ***
 *     
 *     We can use these values, to optimize when we start to poll for erase/write completion.
 *     Otherwise, we're just polluting the DMA bus with requests.
 *     
 *     NOTE:  The theoretical ERASE limits ended up being > than the experimental ones.
 *            We'd rather go a bit low, than too much, so putting delays right on the edge
 *                is the way to go.  Theoretical is commented out.  Experimented being used.                                          n
 *     
 *     256  byte page write:         1000 bytes/milliseconds
 *     4KB  physical sector erase:   30   bytes/milliseconds
 *     64KB physical sector erase:   500  bytes/milliseconds
 */
#define WAIT_AFTER_PAGE_PROGRAM_MS  0   // easily done with a PP (with margin) after 1 ms, so don't delay at all.  pointless delay.

#define BYTES_PER_MS_4KB_ERASE      30
#define WAIT_AFTER_4KB_ERASE_MS     150 //136 // (EXT_FLASH_4KB_SZ / BYTES_PER_MS_4KB_ERASE)

#define BYTES_PER_MS_64KB_ERASE     500
#define WAIT_AFTER_64KB_ERASE_MS    150 //131 // (EXT_FLASH_64KB_SZ / BYTES_PER_MS_64KB_ERASE)


/*
 *      4 KB Area at the TOP of SPI Flash (lower address).
 */

// System Information (Boot Flags, Misc Info) is 1 x 4KB sector.
#define EXT_FLASH_SYSTEM_INFO_ADDR           0x0
#define EXT_FLASH_SYSTEM_INFO_NUM_SECTORS    1

#define EXT_FLASH_PERSISTENT_INFO_ADDR       0x1000
#define EXT_FLASH_SYSTEM_INFO_NUM_SECTORS    1

// Depcrecated CFGMGR (old hack way).
#define EXT_FLASH_CFG_STATIC_ADDR            0x2000
#define EXT_FLASH_CFG_STATIC_NUM_SECTORS     1

// CFGMGR Serialized Data Ping Pong #1
#define EXT_FLASH_CFG_SERIALIZED1_ADDR        0x3000
#define EXT_FLASH_CFG_SERIALIZED1_NUM_SECTORS 1

// CFGMGR Serialized Data Ping Pong #2
#define EXT_FLASH_CFG_SERIALIZED2_ADDR        0x4000
#define EXT_FLASH_CFG_SERIALIZED2_NUM_SECTORS 1

// CFGMGR Reserved Sector
#define EXT_FLASH_CFG_RESERVED_ADDR           0x5000
#define EXT_FLASH_CFG_RESERVED_NUM_SECTORS    1

// Scratch Pad (Misc persistent system malloc)
#define EXT_FLASH_SCRATCHPAD_ADDR            0x6000
#define EXT_FLASH_SCRATCHPAD_NUM_SECTORS     4

// Event Manager Information
#define EXT_FLASH_EVTMGR_INFO_ADDR_DEP          0xA000  // depcrecated
#define EXT_FLASH_EVTMGR_INFO_NUM_SECTORS_DEP   1       // depcrecated

// FW Update Manager Information
#define EXT_FLASH_FWUPDATE_INFO_ADDR         0xB000
#define EXT_FLASH_FWUPDATE_INFO_NUM_SECTORS  1

// Factory Config (strictly reserved sector --  CANNOT change!)
#define EXT_FLASH_FACTORY_CFG_ADDR           0xC000
#define EXT_FLASH_FACTORY_CFG_NUM_SECTORS    1

// Functional Test App Config Section
#define EXT_FLASH_FTA_CFG_ADDR               0xD000
#define EXT_FLASH_FTA_CFG_NUM_SECTORS        1

// Assdump Section
#define EXT_FLASH_SHLK_ASSDUMP_ADDR          0xE000
#define EXT_FLASH_SHLK_ASSDUMP_NUM_SECTORS   2        // don't change this.  Sherlock assumes a ping pong of 2 sectors.

// Sherlock enable/disable section
#define EXT_FLASH_SHLK_EN_ADDR               0x10000
#define EXT_FLASH_SHLK_EN_NUM_SECTORS        1

// Burn-in sector for knowing pass/fail.
#define EXT_FLASH_BURNIN_ADDR                0x11000
#define EXT_FLASH_BURNIN_NUM_SECTORS         1

// Sherlock enable/disable section
#define EXT_FLASH_SHLK_SERV_ADDR             0x12000
#define EXT_FLASH_SHLK_SERV_NUM_SECTORS      1

// ACC calibration data
#define EXT_FLASH_ACC_CAL_ADDR               0x13000
#define EXT_FLASH_ACC_CAL_NUM_SECTORS        1

// 4KB Reserved Section
#define EXT_FLASH_RSVD_4KB_ADDR              0x14000
#define EXT_FLASH_RSVD_4KB_NUM_SECTORS       12


/*
 *      64 KB Area at the BOTTOM of SPI Flash (higher address).
 */

// Application Image is 16 x 64KB sectors (1MB)
#define EXT_FLASH_APP_IMG_ADDR              0x20000
#define EXT_FLASH_APP_IMG_NUM_SECTORS       16

// Event Block Pointers (indexes into event manager data).
#define EXT_FLASH_EVTMGR_POINTERS_ADDR          0x120000
#define EXT_FLASH_EVTMGR_POINTERS_NUM_SECTORS   5
#define EXT_FLASH_EVTMGR_POINTERS_LAST_ADDR     (EXT_FLASH_EVTMGR_POINTERS_ADDR + (EXT_FLASH_EVTMGR_POINTERS_NUM_SECTORS * EXT_FLASH_64KB_SZ) - 1)

// Event Manager DATA
#define EXT_FLASH_EVENT_MGR_ADDR            0x170000
#define EXT_FLASH_EVENT_MGR_NUM_SECTORS     460
#define EXT_FLASH_EVENT_MGR_LAST_ADDR       (EXT_FLASH_EVENT_MGR_ADDR + (EXT_FLASH_EVENT_MGR_NUM_SECTORS * EXT_FLASH_64KB_SZ) - 1) // 0x1E2FFFF

#if 0   // deprecated Sherlock sectors

	// Sherlock log sectors
	#define EXT_FLASH_SHLK_LOG_ADDR             0x1E30000
	#define EXT_FLASH_SHLK_LOG_NUM_SECTORS      3
	
	// Buffer between Sherlock and BT patches as a backup
	// to prevent Sherlock from overwriting BT patches
	#define EXT_FLASH_BUF_SHLKPATCH_ADDR        0x1E60000
	#define EXT_FLASH_BUF_SHLKPATCH_NUM_SECTORS 1

#else

	// 64KB Reserved Section
	#define EXT_FLASH_RSVD_64KB_ADDR            0x1E30000
	#define EXT_FLASH_RSVD_64KB_NUM_SECTORS     3

    #define EXT_FLASH_EVTMGR_INFO_ADDR          0x1E60000
    #define EXT_FLASH_EVTMGR_INFO_NUM_SECTORS   1

#endif

// BasePatch data
#define EXT_FLASH_BASEPATCH_ADDR            0x1E70000
#define EXT_FLASH_BASEPATCH_NUM_SECTORS     1

// LowEnergy data
#define EXT_FLASH_LOWENERGY_ADDR            0x1E80000
#define EXT_FLASH_LOWENERGY_NUM_SECTORS     1

// Application BACKUP Image is 16 x 64KB sectors (1MB)
#define EXT_FLASH_APP_IMG_BKP_ADDR          0x1E90000
#define EXT_FLASH_APP_IMG_BKP_NUM_SECTORS   16

#if 0  // Trading places with Sherlock, to give Sherlock more room to run.

	// 64KB Reserved Section
	#define EXT_FLASH_RSVD_64KB_ADDR            0x1F90000
	#define EXT_FLASH_RSVD_64KB_NUM_SECTORS     7

#else

	// Sherlock log sectors
	#define EXT_FLASH_SHLK_LOG_ADDR             0x1F90000
	#define EXT_FLASH_SHLK_LOG_NUM_SECTORS      6
	
	// Buffer between Sherlock and BT patches as a backup
	// to prevent Sherlock from overwriting BT patches
	#define EXT_FLASH_BUF_SHLKPATCH_ADDR        (EXT_FLASH_SHLK_LOG_ADDR + (EXT_FLASH_SHLK_LOG_NUM_SECTORS * EXT_FLASH_64KB_SZ))
	#define EXT_FLASH_BUF_SHLKPATCH_NUM_SECTORS 1

#endif


// LAST ADDRESS = 0x1FFFFFF = EXT_FLASH_REGION_END

#endif /* CORONA_EXT_FLASH_LAYOUT_H_ */
