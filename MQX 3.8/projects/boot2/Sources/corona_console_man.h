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

#if 0 // don't need this stuff compiled into boot2
#define CC_MAN_ABT "\n\
Corona Console v1.1.0: Whistle Labs (c) 2013.\n\
Contact ernie@whistle.com for support.\n\
Type ? for a list of commands.\n\n"

#define CC_MAN_INTR "\
Read the interrupt count for a given IRQ.\n\
   intr <irq name>\n\n\
IRQ Names:\n\
   btn     # BUTTON   (PTA29)\n\
   a1      # ACC_INT1 (PTC12)\n\
   a2      # ACC_INT2 (PTC13)\n\
   chg     # CHG_B    (PTC14)\n\
   pgood   # PGOOD_B  (PTC15)\n\
   wifi    # WIFI_INT (PTE5)\n\n"

#define CC_MAN_INTC "\
Clear the interrupt count for a given IRQ.\n\
   intc <irq name>\n\n\
IRQ Names:\n\
   btn     # BUTTON   (PTA29)\n\
   a1      # ACC_INT1 (PTC12)\n\
   a2      # ACC_INT2 (PTC13)\n\
   chg     # CHG_B    (PTC14)\n\
   pgood   # PGOOD_B  (PTC15)\n\
   wifi    # WIFI_INT (PTE5)\n\n"

#define CC_MAN_IOR "\
gpIO Read from GPIOx_PDIR register.\n\
   ior <field> <optional number of seconds to poll and print>\n\n\
EXAMPLES:\n\
	ior button     # Reads Button value (PTA29)\n\
	ior bt_pwdn    # Reads Bluetooth powerdown pin (PTB0).\n\
	ior mfi_rst    # Reads MFI Reset pin (PTB1).\n\
	ior rf_sw_vers # Reads RF_SW_Vx pins (PTB2:PTB3).\n\
	ior data_wp    # Reads DATA_SPI_WP_B (PTB18).\n\
	ior data_hold  # Reads DATA_SPI_HOLD_B (PTB19).\n\
	ior acc_int1   # Reads ACC_INT1 (PTC12).\n\
	ior acc_int2   # Reads ACC_INT2 (PTC13).\n\
	ior chg        # Reads CHG_B (PTC14).\n\
	ior pgood      # Reads PGOOD_B (PTC15).\n\
	ior 26mhz      # Reads 26MHZ_MCU_EN (PTC19).\n\
	ior led        # Reads hex representation of 4 debug LEDs' status (PTD12:PTD15).\n\
	ior wifi_pd    # Reads WIFI powerdown signal (PTE4).\n\
	ior wifi_int   # Reads WIFI interrupt signal (PTE5).\n\
	ior 32khz      # Reads 32KHz Bluetooth enable signal (PTE6).\n\
NOTE: You can specify a 3rd argument, which is number of seconds to poll the signal.\n\
 EXAMPLE: ior button 10  # Print status of button every 100ms, for 10 seconds.\n\
\n"

#define CC_MAN_IOW "\
gpIO Write to GPIOx_PDIR register.\n\
   iow <field> <value (hex)>\n\n\
NOTE: Only GPIO _outputs_ (all are given below), can be used with iow.\n\n\
EXAMPLES:\n\
	iow bt_pwdn 0    # Writes a 0 to Bluetooth powerdown pin (PTB0).\n\
	iow mfi_rst 0    # Writes a 0 to MFI Reset pin (PTB1).\n\
	iow rf_sw_vers 2 # Writes RF_SW_Vx pins to binary 10 (PTB2:PTB3).\n\
	iow data_wp 0    # Writes a 0 to DATA_SPI_WP_B (PTB18).\n\
	iow data_hold 1  # Writes a 1 to DATA_SPI_HOLD_B (PTB19).\n\
	iow 26mhz 1      # Enables 26MHZ_MCU_EN (PTC19).\n\
	iow led a        # Writes 4 debug LED's with <1010> pattern (PTD12:PTD15).\n\
	iow wifi_pd 0    # Writes a 0 to WIFI powerdown signal (PTE4).\n\
	iow 32khz 1      # Writes a 1 to 32KHz Bluetooth enable signal (PTE6).\n\
\n"

#define CC_MAN_TEST "\
Test console by printing out argv.\n\
   test\n\n"
#define CC_MAN_MEMR "\
Read from memory, starting from 32-bit address given, to number of 32-bit WORDS given.\n\
   memr <address (hex)> <number of words (dec)>\n\n\
Example: memr 1fff34a4 1\n\
Note: Address must be given in hex without 0x or x in front.\n\
Note: Number of Words must be given in decimal.\n\
Note: You can read as many words as you want, but must read at least 1.\n\
Note: Words printed out to the console are formatted in hex.\n\n"
#define CC_MAN_MEMW "\
Write to memory, starting from 32-bit address given, to number of 32-bit WORDS given.\n\
   memw  <address (hex)> <32-bit word(s) (hex) to write> <...>\n\n\
Example: memw 1fff33c0 deadbeef 123456789 fdecba98\n\
Note: Address must be given in hex without 0x or x in front.\n\
Note: Words-to-Write must be given in hex without 0x or x in front.\n\n"

#define CC_MAN_PWM "\
Change PWM to given percentage duty cycle.\n\
   pwm <percentage>\n\n\
Example: pwm 20 50   # set PWM to 20 millisecond, 50% duty cycle.\n\
\n\n"

#define CC_MAN_PWR "\
Change the system power state.\n\
   pwr <state>\n\n\
Example: pwr standby   # standby mode\n\
Example: pwr ship      # ship mode\n\
Example: pwr normal    # normal mode\n\n\
Note: Console access may be terminated after using this command.\n\n"

#define CC_MAN_ACC "\
Control the Accelerometer.\n\
   acc  <command> <arg(s)...>\n\n\
Example: acc dump       # Dumps 1 X/Y/Z data sample to console.\n\
Example: acc dump 10    # Dumps X/Y/Z data to console for 10 seconds.\n\
Example: acc dump 8 3   # Dumps X/Y/Z data to console for 8 seconds, 3 ms between samples.\n\
Example: acc set 20 7   # Sets register 0x20 to value 0x07.\n\
Example: acc get F      # Gets value of register 0x0F.\n\
Note: If no millisecond sample rate is specified, 300ms is used.\n\
\n\n"

#define CC_MAN_BFR "\
Read a bitfield from a specific 32-bit address in memory.\n\
   bfr  <address (hex)> <msb> <lsb>\n\n\
Example: bfr 1fff33c0 20 16\n\
Note: Address must be given in hex without 0x or x in front.\n\
Note: Most and least significant bits given in decimal, with MSB first.\n\n"

#define CC_MAN_BFW "\
Write a bitfield to a specific field in a 32-bit address in memory.\n\
   bfw  <address (hex)> <msb> <lsb> <value (hex)>\n\n\
Example: bfw 1fff33c0 20 16 e\n\
Note: Address must be given in hex without 0x or x in front.\n\
Note: Value must be given in hex without 0x or x in front.\n\
Note: Most and least significant bits given in decimal, with MSB first.\n\n"

#define CC_MAN_DIS "\
Write a register on the BU26507 display chip.\n\
   dis <register (hex)> <value (hex)>\n\n\
Example: dis 11 3f\n\
Note: The ROHM chip does not support reading.\n\
\n"

#define CC_MAN_KEY "\
Change security.\n\
   key <p@ssw0rd>\n\n\
Reset security to 0 (low).\n\
   key reset\n\n"

#endif

#define CC_MAN_MAN "\
manual\n\
   man <command>\n\n"

#if 0
#define CC_MAN_ADC "\
Sample the ADC of either VBatt or UID.\n\
   adc  <pin> <numSamples> <ms delay between samples>\n\n\
Example: adc bat 10 2000  # Sample VBatt 10 times, 2sec per sample.\n\
Example: adc uid 37 1     # Sample UID 37 times, 1ms per sample.\n\
\n\n"
#endif

#define CC_MAN_RESET "\
Reset device\n\n"

#define CC_MAN_STATE "\
State\n\n"

#define CC_MAN_UID "\
UID pin sample\n\n"

#define CC_MAN_WHO "\
boot2\n\n"

#define CC_MAN_FACTRST "\
Back to factory defaults\n\n"

#define CC_MAN_FLE "\
Erase a Spansion sector\n\
 fle <addr (hex)>\n\n\
Example: fle 2000\n\n"

#define CC_MAN_FLR "yo mama\n"

#if 0

#define CC_MAN_FLR "\
Read from SPI Flash memory.\n\
   flr <address (hex)> <number of bytes (dec)>\n\n\
Example: flr 1A000 64\n\
Note:  You can read from any byte address in memory, but\n\
       writing/erasing must take place on a sector boundary.\n\
\n"

#define CC_MAN_FLW "\
Write to SPI Flash memory.\n\
   flw <address (hex)> <byte 0> ... <byte n>\n\n\
Example: flw 40000 de ad be ef\n\
Note:  WRITE must take place on a sector boundary.\n\
       First 510 sectors are 64KB large.\n\
       Next 32 sectors are 4KB large.\n\
\n"
#endif

#define CC_MAN_FWA "\
Load a fw app img to int FLASH.\n\
    fwa <address (hex)>, <size (dec)>\n\n\
Example: fwa 40000, 16310\n\
\n"

#define CC_MAN_FWS "\
Load a fwu pkg to SPI FLASH\n\
    fws <size (dec)>\n\n\
Example: fws 262144\n\
\n"

#define CC_MAN_RPKG "\
Read fwu pkg in SPI FLASH\n\
    rpkg <pkg # (0 or 1)>\n\n\
Example: rpkg 0\n\
\n"

#define CC_MAN_LPKG "\
Load pkg to SPI FLASH\n\
    rpkg <pkg # (0 or 1)>\n\n"

#define CC_MAN_RHDR "\
Read boot2 and app hdrs. \n\n"

#define CC_MAN_GTO "\
Goto address\n\
 gto <address (hex)>\n\
default = 0x22000\n\n"

#define CC_MAN_SETUPD "\
Set update flag <app name>\n\
    app, b2a, or b2b\n\
setupd app\n\
\n"

#define CC_MAN_HELP "\
List cmds\n\n"

#define CC_MAN_PWR "\
Set power mode that app should go to\n\r\
  pwr ship <on/off>\n\n\r"

#endif /* CORONA_CONSOLE_MAN_H_ */
////////////////////////////////////////////////////////////////////////////////
// End of file
////////////////////////////////////////////////////////////////////////////////
//! @}

