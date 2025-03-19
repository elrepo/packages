
----------------------------------------------------------------------

              --- CAEN SpA - Computing Systems Division --- 

----------------------------------------------------------------------
        
        A3818 Driver Readme file
        
----------------------------------------------------------------------

        Package for Linux kernels

----------------------------------------------------------------------

 The complete documentation can be found in the User's Manual on CAEN's web
 site at: http://www.caen.it.


 Content
 -------
 Readme.txt       	: This file.
 ReleaseNotes.txt 	: Release Notes of the last software release.
 src\a3818.c          	: The source file of the driver
 src\a3818.h            : The header file of the driver
 install.sh             : A script to install the driver using DKMS
 Makefile         	: The Makefile to compile the driver


 System Requirements
 -------------------
 - CAEN A3818 PCI CARD
 - Compilation tested on:
  - Debian: 10, 11, 12
  - Ubuntu (including HWE): 16.04, 18.04, 20.04, 22.04, 24.04
  - RHEL (and derivatives): 7.9, 8.10, 9.5
  - Fedora: 39, 40, 41
 
 
 --------------------------------------------------------------------------------------------------------------
 					N.B.
 CAEN provides Linux drivers for its products as source code (open source). For Linux kernels requiring the
 digital signature for driver installation, the User must compile the driver by signing it with his own digital
 certificate or disable the demand for the digital signature in the kernel.
 If an unsigned driver is loaded on a kernel that requires a signature, the message "ERROR: unable to insert \
 'DriverName \': Operation not allowed" will appear.
---------------------------------------------------------------------------------------------------------------

 Compilation notes
 ------------------

  To compile the a3818 device driver using DKMS:
  - $ ./install.sh

  Alternatively, to compile and install the a3818 device driver:
  - $ make
  - $ sudo make install

  To uninstall the a3818 device driver:
  - $ sudo make uninstall
