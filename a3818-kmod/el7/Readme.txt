
----------------------------------------------------------------------

              --- CAEN SpA - Computing Systems Division --- 

----------------------------------------------------------------------
        
        A3818 Driver Readme file
        
----------------------------------------------------------------------

        Package for Linux kernels 2.6/3.x/4.x/5.4/5.11

        September	 2021

----------------------------------------------------------------------

 The complete documentation can be found in the User's Manual on CAEN's web
 site at: http://www.caen.it.


 Content
 -------

 Readme.txt       	: This file.

 ReleaseNotes.txt 	: Release Notes of the last software release.

 src\a3818.c          	: The source file of the driver

 include\a3818.h        : The header file of the driver

 Makefile         	: The Makefile to compile the driver


 System Requirements
 -------------------

 - CAEN A3818 PCI CARD
 - Linux kernel Rel 2.6/3.x/4.x/5.4/5.11 with gnu C/C++ compiler 
 - Tested on Linux kernel 4.18, 5.4 and 5.11
 
 --------------------------------------------------------------------------------------------------------------
 					N.B.
 CAEN provides Linux drivers for its products as source code (open source). For Linux kernels requiring the
 digital signature for driver installation, the User must compile the driver by signing it with his own digital
 certificate or disable the demand for the digital signature in the kernel.
 If an unsigned driver is loaded on a kernel that requires a signature, the message "ERROR: unable to insert \
 'DriverName \': Operation not allowed" will appear.
---------------------------------------------------------------------------------------------------------------

 Installation notes
 ------------------

  To install the A3818 device driver:

  - Execute: make

  - Execute: make install 

 Uninstallation notes
 ------------------

  To uninstall the A3818 device driver:

  - Execute: make uninstall
