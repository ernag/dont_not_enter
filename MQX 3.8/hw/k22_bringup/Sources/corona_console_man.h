////////////////////////////////////////////////////////////////////////////////
//! \addtogroup application
//! \ingroup corona
//! @{
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
//! \file corona_console_man.h
//! \brief Contains the "man" (manual) strings for all the commands.
//! \version version 0.1
//! \date Jan, 2013
//! \author ernie@whistle.com
////////////////////////////////////////////////////////////////////////////////

#ifndef CORONA_CONSOLE_MAN_H_
#define CORONA_CONSOLE_MAN_H_

////////////////////////////////////////////////////////////////////////////////
// Structures
////////////////////////////////////////////////////////////////////////////////
typedef const char * cc_manual_t;

////////////////////////////////////////////////////////////////////////////////
// Console Manuals
////////////////////////////////////////////////////////////////////////////////
#define CC_MAN_INTR "\
Read the interrupt count for a given IRQ.\n\r\
   intr <irq name>\n\n\r\
IRQ Names:\n\r\
   btn     # BUTTON   (PTA29)\n\r\
   a1      # ACC_INT1 (PTC12)\n\r\
   a2      # ACC_INT2 (PTC13)\n\r\
   chg     # CHG_B    (PTC14)\n\r\
   pgood   # PGOOD_B  (PTC15)\n\r\
   wifi    # WIFI_INT (PTE5)\n\n\r"

#define CC_MAN_INTC "\
Clear the interrupt count for a given IRQ.\n\r\
   intc <irq name>\n\n\r\
IRQ Names:\n\r\
   btn     # BUTTON   (PTA29)\n\r\
   a1      # ACC_INT1 (PTC12)\n\r\
   a2      # ACC_INT2 (PTC13)\n\r\
   chg     # CHG_B    (PTC14)\n\r\
   pgood   # PGOOD_B  (PTC15)\n\r\
   wifi    # WIFI_INT (PTE5)\n\n\r"

#define CC_MAN_IOR "\
gpIO Read from GPIOx_PDIR register.\n\r\
   ior <field>\n\n\r\
EXAMPLES:\n\r\
	ior button     # Reads Button value (PTA29)\n\r\
	ior bt_pwdn    # Reads Bluetooth powerdown pin (PTB0).\n\r\
	ior mfi_rst    # Reads MFI Reset pin (PTB1).\n\r\
	ior rf_sw_vers # Reads RF_SW_Vx pins (PTB2:PTB3).\n\r\
	ior data_wp    # Reads DATA_SPI_WP_B (PTB18).\n\r\
	ior data_hold  # Reads DATA_SPI_HOLD_B (PTB19).\n\r\
	ior acc_int1   # Reads ACC_INT1 (PTC12).\n\r\
	ior acc_int2   # Reads ACC_INT2 (PTC13).\n\r\
	ior chg        # Reads CHG_B (PTC14).\n\r\
	ior pgood      # Reads PGOOD_B (PTC15).\n\r\
	ior 26mhz      # Reads 26MHZ_MCU_EN (PTC19).\n\r\
	ior led        # Reads hex representation of 4 debug LEDs' status (PTD12:PTD15).\n\r\
	ior wifi_pd    # Reads WIFI powerdown signal (PTE4).\n\r\
	ior wifi_int   # Reads WIFI interrupt signal (PTE5).\n\r\
	ior 32khz      # Reads 32KHz Bluetooth enable signal (PTE6).\n\r\
\n\r"

#define CC_MAN_IOW "\
gpIO Write to GPIOx_PDIR register.\n\r\
   iow <field> <value (hex)>\n\n\r\
NOTE: Only GPIO _outputs_ (all are given below), can be used with iow.\n\n\r\
EXAMPLES:\n\r\
	iow bt_pwdn 0    # Writes a 0 to Bluetooth powerdown pin (PTB0).\n\r\
	iow mfi_rst 0    # Writes a 0 to MFI Reset pin (PTB1).\n\r\
	iow rf_sw_vers 2 # Writes RF_SW_Vx pins to binary 10 (PTB2:PTB3).\n\r\
	iow data_wp 0    # Writes a 0 to DATA_SPI_WP_B (PTB18).\n\r\
	iow data_hold 1  # Writes a 1 to DATA_SPI_HOLD_B (PTB19).\n\r\
	iow 26mhz 1      # Enables 26MHZ_MCU_EN (PTC19).\n\r\
	iow led a        # Writes 4 debug LED's with <1010> pattern (PTD12:PTD15).\n\r\
	iow wifi_pd 0    # Writes a 0 to WIFI powerdown signal (PTE4).\n\r\
	iow 32khz 1      # Writes a 1 to 32KHz Bluetooth enable signal (PTE6).\n\r\
\n\r"

#define CC_MAN_TEST "\
Test console by printing out argv.\n\r\
   test\n\n\r"

#define CC_MAN_MAN "\
Present MANual for given command.\n\r\
   man <command>\n\n\r"

#define CC_MAN_MEMR "\
Read from memory, starting from 32-bit address given, to number of 32-bit WORDS given.\n\r\
   memr <address (hex)> <number of words (dec)>\n\n\r\
Example: memr 1fff34a4 1\n\r\
Note: Address must be given in hex without 0x or x in front.\n\r\
Note: Number of Words must be given in decimal.\n\r\
Note: You can read as many words as you want, but must read at least 1.\n\r\
Note: Words printed out to the console are formatted in hex.\n\n\r"

#define CC_MAN_MEMW "\
Write to memory, starting from 32-bit address given, to number of 32-bit WORDS given.\n\r\
   memw  <address (hex)> <32-bit word(s) (hex) to write> <...>\n\n\r\
Example: memw 1fff33c0 deadbeef 123456789 fdecba98\n\r\
Note: Address must be given in hex without 0x or x in front.\n\r\
Note: Words-to-Write must be given in hex without 0x or x in front.\n\n\r"

#define CC_MAN_PWR "\
Change the system power state.\n\r\
   pwr <state>\n\n\r\
Example: pwr standby   # standby mode\n\r\
Example: pwr ship      # ship mode\n\r\
Example: pwr normal    # normal mode\n\n\r\
Note: Console access may be terminated after using this command.\n\n\r"

#define CC_MAN_ADC "\
Sample the ADC of either VBatt or UID.\n\r\
   adc  <pin> <numSamples> <ms delay between samples>\n\n\r\
Example: adc bat 10 2000  # Sample VBatt 10 times, 2sec per sample.\n\r\
Example: adc uid 37 1     # Sample UID 37 times, 1ms per sample.\n\r\
\n\n\r"

#define CC_MAN_BFR "\
Read a bitfield from a specific 32-bit address in memory.\n\r\
   bfr  <address (hex)> <msb> <lsb>\n\n\r\
Example: bfr 1fff33c0 20 16\n\r\
Note: Address must be given in hex without 0x or x in front.\n\r\
Note: Most and least significant bits given in decimal, with MSB first.\n\n\r"

#define CC_MAN_BFW "\
Write a bitfield to a specific field in a 32-bit address in memory.\n\r\
   bfw  <address (hex)> <msb> <lsb> <value (hex)>\n\n\r\
Example: bfw 1fff33c0 20 16 e\n\r\
Note: Address must be given in hex without 0x or x in front.\n\r\
Note: Value must be given in hex without 0x or x in front.\n\r\
Note: Most and least significant bits given in decimal, with MSB first.\n\n\r"

#if 0
/*
 *     READING IS NOT SUPPORTED WITH THE DISPLAY DEVICE !
 */
#define CC_MAN_DISR "\
Read a register from the BU26507 display chip.\n\r\
   disr <register (hex)>\n\n\r\
Example: disr 2d\n\r\
\n\r"
#endif

#define CC_MAN_DIS "\
Write a register on the BU26507 display chip.\n\r\
   dis <register (hex)> <value (hex)>\n\n\r\
Example: dis 11 3f\n\r\
Note: The ROHM chip does not support reading.\n\r\
\n\r"

#define CC_MAN_FLE "\
Erase a sector on the SPI Flash chip.\n\r\
   fle <address (hex)>\n\n\r\
Example: fle 11B00\n\r\
\n\r"

#define CC_MAN_FLR "\
Read from SPI Flash memory.\n\r\
   flr <address (hex)> <number of bytes (dec)>\n\n\r\
Example: flr 1A000 64\n\r\
\n\r"

#define CC_MAN_FLW "\
Write to SPI Flash memory.\n\r\
   flw <address (hex)> <byte 0> ... <byte n>\n\n\r\
Example: flw 4C00 de ad be ef\n\r\
\n\r"

#define CC_MAN_HELP "\
List commands.\n\r\
   h,?,help\n\n\r"

#define CC_MAN_KEY "\
Change security.\n\r\
   key <p@ssw0rd>\n\n\r\
Reset security to 0 (low).\n\r\
   key reset\n\n\r"


#endif /* CORONA_CONSOLE_MAN_H_ */
////////////////////////////////////////////////////////////////////////////////
// End of file
////////////////////////////////////////////////////////////////////////////////
//! @}

