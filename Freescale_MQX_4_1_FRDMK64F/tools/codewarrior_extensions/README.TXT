Manual installation of CodeWarrior extensions:
----------------------------------------------

Supported compillers:
  - CodeWarrior for Microcontrollers v10.2
  - CodeWarrior for Microcontrollers v10.3 and newer
  
  - CodeWarrior for MobileGT V9.2
  



1) CodeWarrior for Microcontrollers v10.2

Navigate to <MQX install dir>/tools/CW MCU v10.2/ and execute install_cw10.2_plugin.bat
Ensure that CodeWarior application is closed before installation.

    Usage:   install_cw10.2_plugin.bat "<Path to CW10.2 installation>"
    Example  install_cw10.2_plugin.bat "c:\Freescale\CW MCU v10.2"

2) CodeWarrior for Microcontrollers v10.3 and newer

Extensions for CodeWarrior for Microcontrollers v10.3 and newer are available via "Update Site" directly in the IDE.

  Available in the Help->Install New Software... menu.  


3) CodeWarrior Classic      

Copy content of "<MQX install dir>/tools/codewarrior_extensions/<your CW version>" 
directory into installation directory. CodeWarrior is located in "Program Files/Freescale/<your CW version>"" by default. 

*TAD is not available only for "CodeWarrior for Microcontrollers V6.3"
