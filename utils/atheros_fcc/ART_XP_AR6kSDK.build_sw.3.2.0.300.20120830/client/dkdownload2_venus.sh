#!/bin/sh --verbose

BMILOADER=./dkbmiloader

# Program analog PLL register
$BMILOADER -s -a 0x1c284 -p 0xF9104001

# Run at 80/88 MHz by default
$BMILOADER -s -a 0x4020 -p 0x1

# Lower pad drive strengths; temporary WAR for SDIO CRC issue
$BMILOADER -s -a 0x14050 -p 0x20
$BMILOADER -s -a 0x14054 -p 0x20
$BMILOADER -s -a 0x14058 -p 0x20
$BMILOADER -s -a 0x1405c -p 0x20

# LPO_CAL.ENABLE = 1
$BMILOADER -s -a 0x40e0 -p 0x100000 

# Download EEPROM file
$BMILOADER -t -f fakeBoardData_AR6003_v2_0.bin
#$BMILOADER -w -a 0x543800 -c otp.bin.z77

# Download firmware
$BMILOADER -w -a 0x543180 -f device2_0.bin
#$BMILOADER -w -a 0x543800 -f athwlan.bin
#$BMILOADER -w -a 0x54300 -c device2_0_z77.bin
$BMILOADER -p -a 0x944e00
 
## WLAN Patch DataSets
$BMILOADER -s -a 0x540668 -p 0x1B00 
$BMILOADER -w -a 0x57e884 -f data.patch.hw2_0.bin
$BMILOADER -w -a 0x540618 -p 0x57e884

# Download ref clock
#$BMILOADER -w -a 0x540678 -f ref_clock_19p2.bin

#These 2 lines are to enbable UART debug on Venus 
$BMILOADER -w -a 0x540614 -p 0x1 
$BMILOADER -w -a 0x540680 -p 0x8

#These 3 lines are to set GPIO 22 to high on Venus 
#$BMILOADER -s -a 0x14080 -p 0x48 
#$BMILOADER -s -a 0x14010 -p 0x400000
#$BMILOADER -s -a 0x14004 -p 0x400000

# TBD: set hi_board_data_initialized to avoid loading eeprom.
#$BMILOADER -w -a 0x500458 -p 0x1

$BMILOADER -d

