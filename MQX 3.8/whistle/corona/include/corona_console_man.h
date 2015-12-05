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

#define CC_MAN_ABT      "\n\
Corona Console: Whistle (c)\n\
ernie@whistle.com\n\
Type ?\n"

#if 0
#define CC_MAN_INTS     "\
Print status of all GPIO interrupts.\n\
   ints\n"
#endif

#define CC_MAN_TIME "\
sys time\n"

#define CC_MAN_UID    "\
Sample UID pin and report back.\n\r\
uid\n\n\r"
#define CC_MAN_MAN      "\
MANual\n\
man <cmd>\n\n"

#if 0
#define CC_MAN_MEMR     "\
Read from memory, starting from 32-bit address given, to number of 32-bit WORDS given. Output is in hex\n\
   memr <address (hex)> <# of words (dec)>\n\n\
memr 1fff34a4 1\n\
Note: must be in hex without 0x or x in front.\n\
Note: # of Words must be in dec (must be at least 1)\n"

#define CC_MAN_MEMW     "\
Write to memory, starting from 32-bit address given, to number of 32-bit WORDS given.\n\
   memw  <address (hex)> <32-bit word(s) (hex) to write> <...>\n\n\
memw 1fff33c0 deadbeef 123456789 fdecba98\n\
Note: Address must be given in hex without 0x or x in front.\n\
Note: Words-to-Write must be given in hex without 0x or x in front.\n\n"
#endif

#define CC_MAN_PERSIST     "\
Persist var\n\
   persist <val> ...\n\
persist hello\n\n"

#if 0
#define CC_MAN_PWM      "\
Change PWM to given percentage duty cycle.\n\
   pwm <percentage>\n\n\
pwm 20 50   # set PWM to 20 millisecond, 50% duty cycle.\n\
\n\n"
#endif

#define CC_MAN_PWR      "\
Change power state\n\
pwr <state>\n\n\
pwr reboot\n\
pwr ship\n\
pwr runm\n\
pwr vlpr\n\
pwr use_lls 1\n\
pwr votes\n\
pwr rfoff\n\n"

#define CC_MAN_PRX      "\
Change prox time (seconds)\n\
prx 10\n\n"

#define CC_MAN_RFSWITCH      "\
Set RF Switch\n\
rfswitch bt\n\
rfswitch wifi\n\n"

#define CC_MAN_RTCDUMP      "\
Dump RTC Q, optionally <count> times, every <seconds> seconds\n\
rtcd [<seconds> <count>]\n\
rtcd 5 2\n\n"

#if 0
#define CC_MAN_SER      "\
Program serial number.\n\
   ser <serial number string>\n\n\
ser CCQ87dfk34r\n\
\n"
#endif

#define CC_MAN_SOL      "\
Sign of Life, flash LEDs\n\
   <seconds> optional, default is 10 sec\n\
   sol [<seconds>]\n\n\
sol 15\n\
\n"

#define CC_MAN_TESTB    "dunk it\n"

#if 0
#define CC_MAN_ACT      "\
Activity metric\n\
act # dump metrics\n\
act on\n\
act off\n\n"
#endif

#if 0
#define CC_MAN_ACC      "\
Control the Accelerometer.\n\
   acc  <command> <arg(s)...>\n\n\
acc dump       # Dump 1 X/Y/Z data sample to console.\n\
acc dump 10    # Dump X/Y/Z data to console for 10 seconds.\n\
acc dump 8 3   # Dump X/Y/Z data to console for 8 seconds, 3 ms between samples.\n\
acc set 20 7   # Set register 0x20 to value 0x07.\n\
acc get F      # Get value of register 0x0F.\n\
acc fs 3       # Set full scale to be FULLSCALE_16G.\n\
acc sec 10     # Set seconds_to_wait_before_sleeping to be 10.\n\
acc thresh 20  # Set any_motion_thresh to be 20 percent of FS.\n\
acc cont 1     # Enable continuous logging (0 to disable).\n\
Note: If no millisecond sample rate is specified, 300ms is used.\n\
\n\n"
#else
#define CC_MAN_ACC      "\
accel\n\
acc <cmd> <arg(s)>\n\n\
acc fs 3\t# 16G\n\
acc sec 10\t# 10 sec no act bf sleep\n\
acc thresh 20\t# anymotion_thresh 20per FS\n\
acc cont 1\t# cont ON\n\n"
#endif

#if 0
#define CC_MAN_ADC      "\
Sample the ADC of either VBatt or UID.\n\
   adc  <pin> <numSamples> <ms delay between samples>\n\n\
adc bat 10 2000  # Sample VBatt 10 times, 2sec per sample.\n\
adc uid 37 1     # Sample UID 37 times, 1ms per sample.\n\
\n\n"

#define CC_MAN_BFR      "\
Read a bitfield from a specific 32-bit address in memory.\n\
   bfr  <address (hex)> <msb> <lsb>\n\n\
bfr 1fff33c0 20 16\n\
Note: Address must be given in hex without 0x or x in front.\n\
Note: Most and least significant bits given in decimal, with MSB first.\n\n"

#define CC_MAN_BFW      "\
Write a bitfield to a specific field in a 32-bit address in memory.\n\
   bfw  <address (hex)> <msb> <lsb> <value (hex)>\n\n\
bfw 1fff33c0 20 16 e\n\
Note: Address must be given in hex without 0x or x in front.\n\
Note: Value must be given in hex without 0x or x in front.\n\
Note: Most and least significant bits given in decimal, with MSB first.\n\n"
#endif

#define CC_MAN_BTCON  "\
(Dis)Connect paired dev\n\
	btcon <connect (0|1)>\n\n\
.\n\n"

#define CC_MAN_BTDEBUG  "\
btdebug <HCI debug en (0|1)> [<SSP debug en (0|1)]\n\
btdebug 0 1\n\n"

#define CC_MAN_BTKEY  "\
btkey <delete <index> | <all>>|<list>\n\
btkey delete all\n\n"

#define CC_MAN_CFGCLEAR "\
Reset cfg items to factory def\n\
  cfgclear\n\n"


#define CC_MAN_CHECKIN "\
Send chkn cmd2srv\n\n"

#define CC_MAN_CONSOLE "\
Enable/Disable Console\n\
console 1\n"

#define CC_MAN_PRINT_PACKAGE "\
Show pkg hdr\n\
   printpackage\n\n"
        
#define CC_MAN_VER "\
version\n\
  ver\n\n"

#define CC_MAN_WMON       "\
Sig strength mon\n\
 Connect USB\n\
wmon <SSID>\n\n"

#define CC_MAN_WSCAN      "\
Scan nets\n\
wscan\n\n"

#define CC_MAN_WCFG      "\
Cfg net\n\
wcfg clear resets all\n\
No argument lists current config\n\
 wcfg [<clear or SSID>$ID <pwd>$PW] [<type> <mcipher> <ucipher>]\n\
     type: open|wep|wpa|wpa2\n\
     mcipher: none|wep|tkip|aes\n\
     ucipher: none|wep|tkip|ccmp\n\
wcfg Sasha$ID walle~12$PW\n\
wcfg Sasha$ID walle~12$PW wpa2 aes ccmp\n\
wcfg\n\
wcfg clear\n\n"

#define CC_MAN_WCON      "\
Connect to net\n\
wcon <SSID>\n\n\
wcon Sasha\n\n"
//Note: The SSID should match something configured with wcfg.\n\n"

#define CC_MAN_WDIS      "\
Disconn all nets\n\
   wdis\n\n"
        
#define CC_MAN_WOFF "\ Turn off Wi-Fi driver and radio\n"

#define CC_MAN_WLIST      "\
List WIFI nets\n\
\n"

#if __ATHEROS_WMICONFIG_SUPPORT__
#define CC_MAN_WMICONFIG     "\
Ath wmiconfig\n\
\n\
wmiconfig --help\n"
//Note: See Atheros Release Thruput Demo doc for details.\n\n"   
#endif

#define CC_MAN_WPING     "\
Ping IP addr via WiFi\n\
wping 192.168.0.1\n\n"
        
#if 0

/*
 *     READING IS NOT SUPPORTED WITH THE DISPLAY DEVICE !
 */
#define CC_MAN_DISR    "\
Read a register from the BU26507 display chip.\n\
   disr <register (hex)>\n\n\
disr 2d\n\
\n"
#endif
        
#define CC_MAN_DATA     "\
\n\
data flush\n\
data reset\n\
data duty 600\t#upld cyc=10min\n\
data spam\t#str ~1MB\n\
data ichk 1\t# chkn msg no care\n\
data holdread\t#cont tx\n\n"

#define CC_MAN_DIS     "\
do pattern\n\
dis <patt enum>\n\n\
dis 7\n\
\n"

#define CC_MAN_ILD      "\
dump interrupt log\n\n"

#define CC_MAN_LOG      "\
log wkup 0\t#No log wake ups\n\
log wkup 1\t#Log wake ups\n\n"

#define CC_MAN_ROHM     "\
Write reg\n\
rohm <reg (hex)> <val (hex)>\n\n\
rohm 11 3f\n\n"
//Note: The ROHM chip does not support reading.\n\

#if 0
#define CC_MAN_FLE     "\
Erase a sector on the SPI Flash chip.\n\
   fle <address (hex)>\n\n\
fle 20000\n\
Note:  ERASE must take place on a sector boundary.\n\
       First 510 sectors are 64KB large.\n\
       Next 32 sectors are 4KB large.\n\
\n"

#define CC_MAN_FLR     "\
Read from SPI Flash memory.\n\
   flr <address (hex)> <number of bytes (dec)>\n\n\
flr 1A000 64\n\
Note:  You can read from any byte address in memory, but\n\
       write/erase must take place on sector boundary.\n\
\n"

#define CC_MAN_FLW     "\
Write to SPI Flash memory.\n\
   flw <address (hex)> <byte 0> ... <byte n>\n\n\
flw 40000 de ad be ef\n\
Note:  WRITE must take place on a sector boundary.\n\
       First 510 sectors are 64KB.\n\
       Next 32 sectors are 4KB.\n\
\n"
#endif

#define CC_MAN_RGB  "\
LED levels\n\
\trgb <R> <G> <B>\n\
\t<X> = [0:15]\n\
\n"

#define CC_MAN_HELP    "\
help\n\
\th|?|help\n\n"

#if 0
#define CC_MAN_KEY     "\
Change security.\n\
   key <p@ssw0rd>\n\n\
Reset security to 0 (low).\n\
   key reset\n\n"

#define CC_MAN_LEDTEST     "\
Cycle through LED colors\n\
   ledtest\n\n"
#endif

#endif /* CORONA_CONSOLE_MAN_H_ */
////////////////////////////////////////////////////////////////////////////////
// End of file
////////////////////////////////////////////////////////////////////////////////
//! @}
