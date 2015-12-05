@echo off

set bat_file_dir=%~dp0

rem 1st parameter is target path (in the <mqx>/lib/<board>/<dir>)
rem or it may be also a name of the library file created by the linker

rem 2nd parameter is compiler name (cw, iar, ewarm, ewcf, keil, uv4), default is cw
set compiler=%~2
if /I "%compiler%" == "" set compiler=cw 

rem 3rd parameter is target name (debug, release) - so fas for Keil compiler only
set target=%~3

rem 4th parameter is board name - so far for Keil compiler only


rem Make sure we are in target directory  (create if needed)
call "%bat_file_dir%\verify_dir.bat" "%~f1"
if %ERRORLEVEL% NEQ 0 exit 1

attrib -R * >NUL 2>&1

rem  Configuration header files

for %%F in (..\..\..\config\common\*.h)      do copy /Y /B ..\..\..\mqx\source\include\dontchg.h + %%F ..\%%~nF%%~xF
for %%F in (..\..\..\config\coronak60n512\*.h)  do copy /Y /B ..\..\..\mqx\source\include\dontchg.h + %%F ..\%%~nF%%~xF

rem  Board-specific BSP files

copy /Y ..\..\..\mqx\source\bsp\coronak60n512\bsp.h .
copy /Y ..\..\..\mqx\source\bsp\coronak60n512\bsp_rev.h .
copy /Y ..\..\..\mqx\source\bsp\coronak60n512\coronak60n512.h .
copy /Y ..\..\..\mqx\source\bsp\coronak60n512\init_lpm.h .
copy /Y ..\..\..\mqx\source\bsp\coronak60n512\bsp_cm.h .

rem  Board-specific IO driver files

copy /Y ..\..\..\mqx\source\io\adc\adc.h .
copy /Y ..\..\..\mqx\source\io\adc\kadc\adc_kadc.h .
copy /Y ..\..\..\mqx\source\io\adc\kadc\adc_mk60.h .
copy /Y ..\..\..\mqx\source\io\io_dun\io_dun.h .
copy /Y ..\..\..\mqx\source\io\io_mem\io_mem.h .
copy /Y ..\..\..\mqx\source\io\io_mem\iomemprv.h .
copy /Y ..\..\..\mqx\source\io\pcb\io_pcb.h .
copy /Y ..\..\..\mqx\source\io\pcb\mqxa\pcb_mqxa.h .
copy /Y ..\..\..\mqx\source\io\io_null\io_null.h .
copy /Y ..\..\..\mqx\source\io\serial\serial.h  .
copy /Y ..\..\..\mqx\source\io\serial\serl_kuart.h .
copy /Y ..\..\..\mqx\source\io\lwgpio\lwgpio.h .
copy /Y ..\..\..\mqx\source\io\lwgpio\whistle_lwgpio_isr_mux.h .
copy /Y ..\..\..\mqx\source\io\lwgpio\lwgpio_kgpio.h .
copy /Y ..\..\..\mqx\source\io\gpio\io_gpio.h .
copy /Y ..\..\..\mqx\source\io\gpio\kgpio\io_gpio_kgpio.h .
copy /Y ..\..\..\mqx\source\io\spi\spi.h .
copy /Y ..\..\..\mqx\source\io\spi\spi_dspi.h .
copy /Y ..\..\..\mqx\source\io\can\flexcan\kflexcan.h .
copy /Y ..\..\..\mqx\source\io\usb\usb_bsp.h .
copy /Y ..\..\..\mqx\source\io\pccard\apccard.h .
copy /Y ..\..\..\mqx\source\io\pccard\pccardflexbus.h .
copy /Y ..\..\..\mqx\source\io\pcflash\pcflash.h .
copy /Y ..\..\..\mqx\source\io\pcflash\ata.h .
copy /Y ..\..\..\mqx\source\io\rtc\krtc.h .
copy /Y ..\..\..\mqx\source\io\rtc\rtc.h .
copy /Y ..\..\..\mqx\source\io\i2c\i2c.h  .
copy /Y ..\..\..\mqx\source\io\i2c\i2c_ki2c.h .
copy /Y ..\..\..\mqx\source\io\i2s\i2s.h .
copy /Y ..\..\..\mqx\source\io\i2s\i2s_ki2s.h .
copy /Y ..\..\..\mqx\source\io\i2s\i2s_audio.h .
copy /Y ..\..\..\mqx\source\io\tfs\tfs.h .
copy /Y ..\..\..\mqx\source\io\flashx\flashx.h .
copy /Y ..\..\..\mqx\source\io\flashx\freescale\flash_ftfl.h .
copy /Y ..\..\..\mqx\source\io\flashx\freescale\flash_mk60.h .
copy /Y ..\..\..\mqx\source\io\flashx\freescale\flash_kinetis.h .
copy /Y ..\..\..\mqx\source\io\esdhc\esdhc.h .
copy /Y ..\..\..\mqx\source\io\sdcard\sdcard.h .
copy /Y ..\..\..\mqx\source\io\sdcard\sdcard_spi\sdcard_spi.h .
copy /Y ..\..\..\mqx\source\io\sdcard\sdcard_esdhc\sdcard_esdhc.h .
copy /Y ..\..\..\mqx\source\io\crc\crc_kn.h .
copy /Y ..\..\..\mqx\source\io\enet\ethernet.h .
copy /Y ..\..\..\mqx\source\io\enet\enet.h .
copy /Y ..\..\..\mqx\source\io\enet\enet_wifi.h .

copy /Y ..\..\..\mqx\source\io\enet\atheros_wifi\atheros_wifi_api.h .
copy /Y ..\..\..\mqx\source\io\enet\atheros_wifi\atheros_wifi.h .
copy /Y ..\..\..\mqx\source\io\enet\atheros_wifi\atheros_wifi_internal.h .
copy /Y ..\..\..\mqx\source\io\enet\atheros_wifi\Custom_src\Include\a_types.h .

copy /Y ..\..\..\mqx\source\io\enet\macnet\macnet_mk60.h .
copy /Y ..\..\..\mqx\source\io\enet\macnet\macnet_1588.h .
copy /Y ..\..\..\mqx\source\io\enet\phy\phy_dp83xxx.h .
copy /Y ..\..\..\mqx\source\io\tchres\tchres.h .
copy /Y ..\..\..\mqx\source\io\lpm\lpm.h .
copy /Y ..\..\..\mqx\source\io\lpm\lpm_smc.h .
copy /Y ..\..\..\mqx\source\io\cm\cm.h .
copy /Y ..\..\..\mqx\source\io\cm\cm_kinetis.h .
copy /Y ..\..\..\mqx\source\io\debug\iodebug.h .

if NOT EXIST Generated_Code mkdir Generated_Code
rem Deleting old *.h files in bsp\Generated_Code directory
attrib -R Generated_Code\* >NUL 2>&1
del /Q Generated_Code\*.h  >NUL 2>&1

rem Copy default PE_LDD.h to bsp\Generated_Code
copy /Y ..\..\..\mqx\source\bsp\coronak60n512\PE_LDD.h .\Generated_Code

rem  Compiler-specific BSP files
goto comp_%compiler%

rem  CodeWarrior Compiler
:comp_cw
mkdir dbg
copy /Y ..\..\..\mqx\source\bsp\coronak60n512\cw\intram.lcf .
copy /Y ..\..\..\mqx\source\bsp\coronak60n512\cw\intflash.lcf .
copy /Y ..\..\..\mqx\source\bsp\coronak60n512\cw\dbg\coronak60n512.mem .\dbg
copy /Y ..\..\..\mqx\source\bsp\coronak60n512\cw\dbg\init_kinetis.tcl .\dbg
copy /Y ..\..\..\mqx\source\bsp\coronak60n512\cw\dbg\mass_erase_kinetis.tcl .\dbg

rem Copy Processor Expert generated files into Generated_Code directory
if NOT EXIST ..\..\..\mqx\build\cw10\bsp_coronak60n512\Generated_Code\PE_LDD.h goto comp_done
copy /Y ..\..\..\mqx\build\cw10\bsp_coronak60n512\Generated_Code\*.h .\Generated_Code
copy /Y ..\..\..\mqx\build\cw10\bsp_coronak60n512\Sources\*.h .\Generated_Code
goto comp_done

rem  IAR Compiler
:comp_iar
:comp_ewarm
:comp_ewcf
copy /Y ..\..\..\mqx\source\bsp\coronak60n512\iar\ram.icf .
copy /Y ..\..\..\mqx\source\bsp\coronak60n512\iar\ram_usr.icf .
copy /Y ..\..\..\mqx\source\bsp\coronak60n512\iar\intflash.icf .
copy /Y ..\..\..\mqx\source\bsp\coronak60n512\iar\intflash_usr.icf .
goto comp_done

rem  Keil Compiler
:comp_keil
:comp_uv4
rem Keil is not able to build directly into lib folder. Need to copy
if /I "%target%" == "debug"   copy /Y ..\..\..\mqx\build\uv4\coronak60n512_IntFlashDebug\bsp_coronak60n512_d.lib .
if /I "%target%" == "release" copy /Y ..\..\..\mqx\build\uv4\coronak60n512_IntFlashRelease\bsp_coronak60n512.lib .
copy /Y ..\..\..\mqx\source\bsp\coronak60n512\uv4\intflash.scf .
goto comp_done

rem  End of Compiler-specific files

:comp_done

rem Generate library readme file
call "%bat_file_dir%\write_readme.bat" > ..\coronak60n512_lib.txt
echo BSP Post-build operations done
