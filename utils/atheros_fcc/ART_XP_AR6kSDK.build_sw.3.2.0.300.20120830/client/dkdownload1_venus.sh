#!/bin/sh --verbose

BMILOADER=./dkbmiloader

$BMILOADER -s -a 0x4020 -p 0x1
$BMILOADER -s -a 0x40e0 -p 0x100000 

# Download EEPROM file
#$BMILOADER -w -a 0x540f10 -f venusEeprom_v1_0.bin
#$BMILOADER -w -a 0x540f10 -f fakeBoardData_AR6003_v1_0.bin
$BMILOADER -t -f fakeBoardData_AR6003_v1_0.bin
$BMILOADER -w -a 0x542800 -f otp.bin
$BMILOADER -e -a 0x944c00 -p 0

# Download firmware
$BMILOADER -w -a 0x542800 -f device1_0.bin
#$BMILOADER -w -a 0x54300 -c device2_0_z77.bin
$BMILOADER -p -a 0x944c00
 
# WLAN Patch DataSets
$BMILOADER -s -a 0x540668 -p 0x1600 
$BMILOADER -w -a 0x57ea6c -f data.patch.hw1_0.bin
$BMILOADER -w -a 0x540618 -p 0x57ea6c

#These 2 lines are to enbable UART debug on Venus 
$BMILOADER -w -a 0x540614 -p 0x1 
$BMILOADER -w -a 0x540680 -p 0x8

#These 3 lines are to set GPIO 22 to high on Venus 
#$BMILOADER -s -a 0x14080 -p 0x48 
#$BMILOADER -s -a 0x14010 -p 0x400000
#$BMILOADER -s -a 0x14004 -p 0x400000

$BMILOADER -d

