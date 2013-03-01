/*
 *  nvidia-detect - A utility to detect NVIDIA graphics cards
 *
 *  Copyright (C) 2013 Philip J Perry <phil@elrepo.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pci/pci.h>

#include "nvidia-detect.h"

#define	PROGRAM_NAME		"nvidia-detect"
#define	NVIDIA_VERSION		310.32

#ifndef PCI_VENDOR_ID_NVIDIA
#define	PCI_VENDOR_ID_NVIDIA	0x10de
#endif

/* define the return codes */
#define	NVIDIA_CURRENT		0x01
#define	NVIDIA_LEGACY_96XX	0x02
#define	NVIDIA_LEGACY_173XX	0x03
#define	NVIDIA_LEGACY_304XX	0x04

#define STREQ(a, b) (strcmp ((a), (b)) == 0)

void PrintUsage(void)
{
	printf("Usage: %s [-hV]\n", PROGRAM_NAME);
	printf("  -h --help         give this help\n");
	printf("  -V --version      display version number\n\n");
	printf("Detect NVIDIA graphics cards and determine the correct NVIDIA driver.\n\n");
	printf("%s will return the following codes:\n", PROGRAM_NAME);
	printf("0: No supported devices found\n");
	printf("1: Device supported by the current %3.2f NVIDIA driver\n", NVIDIA_VERSION);
	printf("2: Device supported by the NVIDIA legacy 96.xx driver\n");
	printf("3: Device supported by the NVIDIA legacy 173.xx driver\n");
	printf("4: Device supported by the NVIDIA legacy 304.xx driver\n\n");
	printf("Please report bugs at http://elrepo.org/bugs\n");
}

int main(int argc, char *argv[])
{
	int i, n, ret;
	char namebuf[128], *name;
	struct pci_access *pacc;
	struct pci_dev *dev;

	if (argc == 2) {
		if (STREQ (argv[1], "-V") || STREQ (argv[1], "--version")) {
			printf("Version: %3.2f\n", NVIDIA_VERSION);
			printf("Copyright (C) 2013 Philip J Perry <phil@elrepo.org>\n");
			exit(0);
		} else {
			PrintUsage();
			exit(0);
		}
	}

	/* some simple error handling */
	if (argc != 1) {
		PrintUsage();
		exit(0);
	}

	ret = 0;			/* Return 0 if no devices found */
	pacc = pci_alloc();		/* Get the pci_access structure */
	pci_init(pacc);			/* Initialize the PCI library */
	pci_scan_bus(pacc);		/* Scan the bus for devices */

	printf("Probing for supported NVIDIA devices...\n");

	/* Iterate over all devices */
	for (dev=pacc->devices; dev; dev=dev->next) {

		/* Find NVIDIA devices */
		if (dev->vendor_id == PCI_VENDOR_ID_NVIDIA) {

		 	/** Find devices supported by the 96xx legacy driver **/
			n = sizeof(nv_96xx_pci_ids)/sizeof(nv_96xx_pci_ids[0]);
			for (i = 0; i < n; i++)
				if (nv_96xx_pci_ids[i] == dev->device_id) {
					/* Get the name of the device */
					name = pci_lookup_name(pacc, namebuf, sizeof(namebuf),
						PCI_LOOKUP_VENDOR | PCI_LOOKUP_DEVICE,
						dev->vendor_id, dev->device_id);
					printf("Found: [%04x:%04x] %s\n", dev->vendor_id, dev->device_id, name);
					printf("This device requires the NVIDIA legacy 96.xx driver (kmod-nvidia-96xx).\n");
					ret = NVIDIA_LEGACY_96XX;
				}

		 	/** Find devices supported by the 173xx legacy driver **/
			n = sizeof(nv_173xx_pci_ids)/sizeof(nv_173xx_pci_ids[0]);
			for (i = 0; i < n; i++)
				if (nv_173xx_pci_ids[i] == dev->device_id) {
					/* Get the name of the device */
					name = pci_lookup_name(pacc, namebuf, sizeof(namebuf),
						PCI_LOOKUP_VENDOR | PCI_LOOKUP_DEVICE,
						dev->vendor_id, dev->device_id);
					printf("Found: [%04x:%04x] %s\n", dev->vendor_id, dev->device_id, name);
					printf("This device requires the NVIDIA legacy 173.xx driver (kmod-nvidia-173xx).\n");
					ret = NVIDIA_LEGACY_173XX;
				}

		 	/** Find devices supported by the 304xx legacy driver **/
			n = sizeof(nv_304xx_pci_ids)/sizeof(nv_304xx_pci_ids[0]);
			for (i = 0; i < n; i++)
				if (nv_304xx_pci_ids[i] == dev->device_id) {
					/* Get the name of the device */
					name = pci_lookup_name(pacc, namebuf, sizeof(namebuf),
						PCI_LOOKUP_VENDOR | PCI_LOOKUP_DEVICE,
						dev->vendor_id, dev->device_id);
					printf("Found: [%04x:%04x] %s\n", dev->vendor_id, dev->device_id, name);
					printf("This device requires the NVIDIA legacy 304.xx driver (kmod-nvidia-304xx).\n");
					ret = NVIDIA_LEGACY_304XX;
				}

		 	/** Find devices supported by the current driver **/
			n = sizeof(nv_current_pci_ids)/sizeof(nv_current_pci_ids[0]);
			for (i = 0; i < n; i++)
				if (nv_current_pci_ids[i] == dev->device_id) {
					/* Get the name of the device */
					name = pci_lookup_name(pacc, namebuf, sizeof(namebuf),
						PCI_LOOKUP_VENDOR | PCI_LOOKUP_DEVICE,
						dev->vendor_id, dev->device_id);
					printf("Found: [%04x:%04x] %s\n", dev->vendor_id, dev->device_id, name);
					printf("This device requires the current %3.2f NVIDIA driver (kmod-nvidia).\n", NVIDIA_VERSION);
					ret = NVIDIA_CURRENT;
				}

		}		/* End find NVIDIA devices */
	}			/* End iteration of devices */

	pci_cleanup(pacc);	/* Close everything */

	if (ret == 0) {
		printf("No supported devices were found.\n");
		printf("Please report bugs at http://elrepo.org/bugs quoting the output from '/sbin/lspci -nn'\n");
	}

	return ret;
}
