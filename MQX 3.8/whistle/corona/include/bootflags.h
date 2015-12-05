/*
 * bootflags.h
 *
 *  Created on: June 26, 2013
 *      Author: Ben McCrea
 */

#ifndef _BOOTFLAGS_H_
#define _BOOTFLAGS_H_

// Boot Flags
// The VBAT Register File is a set of 8 32-bit registers which are 
// used for passing info between the app and bootloaders:
//  REG[0] Magic #
//  REG[1] App-to-Boot flags
//  REG[2] Boot-to-App flags
#define BOOTFLAGS_MAGIC             0xF1D0F1D0
#define BF_WAKE_FROM_SHIP_MODE      0x00000001
#define BF_REBOOT_ON_ERROR          0x00000002

#endif
