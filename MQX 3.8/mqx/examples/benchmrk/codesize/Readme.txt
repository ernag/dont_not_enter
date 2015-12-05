Freescale Semiconductor MQX(TM) CodeSize script

The script analyzes the generated xMAP file produced by CodeWarrior or
IAR Embedded Workbench build tools and creates codesize report in an HTML 
format.

The script PERL source code is located in the current folder, the script 
executable is located in /tools directory. 

The reports created with this script for different build tools are available 
in the "results" folder. The MQX kernel configurations used for these results
are available in the "config" directory.   

==============================================================================
 
Supported tools:
    CodeWarrior for MCU/ColdFire 6.3 (use -c cwcf6)
    CodeWarrior for ColdFire 7.2 (use -c cwcf7)
    CodeWarrior for mobileGT 9.2 (use -c cwmpc9)
    CodeWarrior version 10 for ColdFire platforms (use -c cwcf10)
    CodeWarrior version 10 for Kinetis platforms  (use -c cwarm10)
    IAR Embedded Workbench for ARM v 6.10 (use -c iararm6)

Options:
    -M ... print detailed MAP file analysis report
    -t ... dump text output to console
    -o <FILE>  ... specify report output file name
    -n <NAME>  ... alternative MAP file name displayed in the report
    -c <FORMAT> .. MAP file format specifier (use before the MAP file name)
       supported formats: cwcf7, cwmcu6, cwmpc9, cwcf10, cwarm10, iararm6

Report Options:
    +PSP  or -PSP  .. force or supporess PSP details (default +)
    +BSP  or -BSP  .. force or supporess BSP details (default +)
    +MFS  or -MFS  .. force or supporess MFS File System details (default -)
    +RTCS or -RTCS .. force or supporess RTCS TCP/IP stack details (default -)
    +USBH or -USBH .. force or supporess USB Host stack details (default -)
    +USBL or -USBL .. force or supporess USB Dev stack details  (default -)
    +SUMM or -SUMM .. force or supporess summary report (default +)

Usage examples:
    codesize.exe -c cwcf7 <MAP_file>
    codesize.exe -M -c cwcf10 <MAP_file>
    codesize.exe +MFS -PSP -BSP -c cwcf10 <MAP_file>
    codesize.exe -c cwcf10 <MAP_file1> -c cwmpc9 <MAP_file2> ....

