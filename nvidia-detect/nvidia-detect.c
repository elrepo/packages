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
#include <linux/version.h>
#include <pci/pci.h>

#include "nvidia-detect.h"

#define PROGRAM_NAME		"nvidia-detect"
#define NVIDIA_VERSION		325.15

#ifndef PCI_VENDOR_ID_INTEL
#define PCI_VENDOR_ID_INTEL	0x8086
#endif
#ifndef PCI_VENDOR_ID_NVIDIA
#define PCI_VENDOR_ID_NVIDIA	0x10de
#endif

/* Only recommend elrepo drivers on RHEL*/
#if (RHEL_MAJOR == 5 || RHEL_MAJOR == 6)
#define KMOD_NVIDIA		"kmod-nvidia"
#define KMOD_NVIDIA_304XX	"kmod-nvidia-304xx"
#define KMOD_NVIDIA_173XX	"kmod-nvidia-173xx"
#define KMOD_NVIDIA_96XX	"kmod-nvidia-96xx"
#else	/* make no specific driver recommendation */
#define KMOD_NVIDIA		""
#define KMOD_NVIDIA_304XX	""
#define KMOD_NVIDIA_173XX	""
#define KMOD_NVIDIA_96XX	""
#endif

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define STREQ(a, b) (strcmp ((a), (b)) == 0)

/* driver return codes */
enum {
	NVIDIA_NONE,
	NVIDIA_CURRENT,
	NVIDIA_LEGACY_96XX,
	NVIDIA_LEGACY_173XX,
	NVIDIA_LEGACY_304XX,
};

static char namebuf[128], *name;
static struct pci_access *pacc;
static struct pci_dev *dev;

static void usage(void)
{
	printf("Usage: %s [-hlV]\n", PROGRAM_NAME);
	printf("  -h --help         give this help\n");
	printf("  -l --list         list all supported NVIDIA devices\n");
	printf("  -V --version      display version number\n\n");
	printf("Detect NVIDIA graphics cards and determine the correct NVIDIA "
		"driver.\n\n");
	printf("%s will return the following codes:\n\n", PROGRAM_NAME);
	printf("0: No supported devices found\n");
	printf("1: Device supported by the current %3.2f NVIDIA driver\n",
		NVIDIA_VERSION);
	printf("2: Device supported by the legacy   96.xx NVIDIA driver\n");
	printf("3: Device supported by the legacy  173.xx NVIDIA driver\n");
	printf("4: Device supported by the legacy  304.xx NVIDIA driver\n\n");
	printf("Please report bugs at http://elrepo.org/bugs\n");
}

static void has_optimus(void)
{
	printf("Optimus hardware detected: An Intel display controller was "
		"detected\n");
	printf("Either disable the Intel display controller in the BIOS\n");
	printf("or use the bumblebee driver to support Optimus hardware\n");
}

static void list_all_nvidia_devices(void)
{
	size_t i;

	printf("\n*** These devices are supported by the current %3.2f NVIDIA "
		"driver ***\n\n", NVIDIA_VERSION);
	for (i = 0; i < ARRAY_SIZE(nv_current_pci_ids); i++) {
		name = pci_lookup_name(pacc, namebuf, sizeof(namebuf),
			PCI_LOOKUP_VENDOR | PCI_LOOKUP_DEVICE,
			PCI_VENDOR_ID_NVIDIA, nv_current_pci_ids[i]);

		printf("[10de:%04x] %s\n", nv_current_pci_ids[i], name);
	}

	printf("\n*** These devices are supported by the legacy 304.xx NVIDIA "
		"driver ***\n\n");
	for (i = 0; i < ARRAY_SIZE(nv_304xx_pci_ids); i++) {
		name = pci_lookup_name(pacc, namebuf, sizeof(namebuf),
			PCI_LOOKUP_VENDOR | PCI_LOOKUP_DEVICE,
			PCI_VENDOR_ID_NVIDIA, nv_304xx_pci_ids[i]);

		printf("[10de:%04x] %s\n", nv_304xx_pci_ids[i], name);
	}

	printf("\n*** These devices are supported by the legacy 173.xx NVIDIA "
		"driver ***\n\n");
	for (i = 0; i < ARRAY_SIZE(nv_173xx_pci_ids); i++) {
		name = pci_lookup_name(pacc, namebuf, sizeof(namebuf),
			PCI_LOOKUP_VENDOR | PCI_LOOKUP_DEVICE,
			PCI_VENDOR_ID_NVIDIA, nv_173xx_pci_ids[i]);

		printf("[10de:%04x] %s\n", nv_173xx_pci_ids[i], name);
	}

	printf("\n*** These devices are supported by the legacy 96.xx NVIDIA "
		"driver ***\n\n");
	for (i = 0; i < ARRAY_SIZE(nv_96xx_pci_ids); i++) {
		name = pci_lookup_name(pacc, namebuf, sizeof(namebuf),
			PCI_LOOKUP_VENDOR | PCI_LOOKUP_DEVICE,
			PCI_VENDOR_ID_NVIDIA, nv_96xx_pci_ids[i]);

		printf("[10de:%04x] %s\n", nv_96xx_pci_ids[i], name);
	}
}

static int nv_lookup_device_id(u_int16_t device_id)
{
	size_t i;

	/** Find devices supported by the current driver **/
	for (i = 0; i < ARRAY_SIZE(nv_current_pci_ids); i++) {
		if (device_id == nv_current_pci_ids[i]) {
			printf("This device requires the current %3.2f NVIDIA "
				"driver %s\n", NVIDIA_VERSION, KMOD_NVIDIA);
			return NVIDIA_CURRENT;
		}
	}

	/** Find devices supported by the 304xx legacy driver **/
	for (i = 0; i < ARRAY_SIZE(nv_304xx_pci_ids); i++) {
		if (device_id == nv_304xx_pci_ids[i]) {
			printf("This device requires the legacy 304.xx NVIDIA "
				"driver %s\n", KMOD_NVIDIA_304XX);
			return NVIDIA_LEGACY_304XX;
		}
	}

	/** Find devices supported by the 173xx legacy driver **/
	for (i = 0; i < ARRAY_SIZE(nv_173xx_pci_ids); i++) {
		if (device_id == nv_173xx_pci_ids[i]) {
			printf("This device requires the legacy 173.xx NVIDIA "
				"driver %s\n", KMOD_NVIDIA_173XX);
			return NVIDIA_LEGACY_173XX;
		}
	}

	/** Find devices supported by the 96xx legacy driver **/
	for (i = 0; i < ARRAY_SIZE(nv_96xx_pci_ids); i++) {
		if (device_id == nv_96xx_pci_ids[i]) {
			printf("This device requires the legacy 96.xx NVIDIA "
				"driver %s\n", KMOD_NVIDIA_96XX);
			return NVIDIA_LEGACY_96XX;
		}
	}

	/** Catch NVIDIA devices that aren't supported **/
	printf("This device does not appear to be supported at present\n");
	printf("Please report at http://elrepo.org/bugs quoting the output "
		"from '/sbin/lspci -nn'\n");

	return NVIDIA_NONE;
}

int main(int argc, char *argv[])
{
	int has_intel = 0, has_nvidia = 0, ret = 0;

	pacc = pci_alloc();		/* Get the pci_access structure */
	pci_init(pacc);			/* Initialize the PCI library */

	if (argc == 2) {
		if (STREQ(argv[1], "-V") || STREQ(argv[1], "--version")) {
			printf("Version: %3.2f\n", NVIDIA_VERSION);
		} else if (STREQ(argv[1], "-l") || STREQ(argv[1], "--list"))
			list_all_nvidia_devices();
		else
			usage();

		goto exit;
	}

	/* some simple error handling */
	if (argc != 1) {
		usage();
		goto exit;
	}

	pci_scan_bus(pacc);		/* Scan the bus for devices */

	printf("Probing for supported NVIDIA devices...\n");

	/* Iterate over all devices */
	for (dev=pacc->devices; dev; dev=dev->next) {

		if (!dev->device_class) {
			fprintf(stderr, "Error getting device_class\n");
			ret = -1;
			goto exit;
		}

		if (dev->device_class == 0x0300) {

			/* Get the name of the device */
			name = pci_lookup_name(pacc, namebuf, sizeof(namebuf),
				PCI_LOOKUP_VENDOR | PCI_LOOKUP_DEVICE,
				dev->vendor_id, dev->device_id);

			printf("[%04x:%04x] %s\n",
				dev->vendor_id, dev->device_id, name);

			/* Find NVIDIA device */
			if (dev->vendor_id == PCI_VENDOR_ID_NVIDIA) {
				has_nvidia++;
				ret = nv_lookup_device_id(dev->device_id);
			}

			/* 
			 * Find Intel device for simplistic detection
			 * of Optimus hardware configurations
			 */
			if (dev->vendor_id == PCI_VENDOR_ID_INTEL)
				has_intel++;

		}	/* End of device_class */

	}		/* End iteration of devices */

	/* Check for Optimus hardware */
	if (has_intel > 0 && has_nvidia > 0)
		has_optimus();

	/* Catch cases where no NVIDIA devices were detected */
	if (has_nvidia == 0)
		printf("No NVIDIA devices were found.\n");

exit:
	pci_cleanup(pacc);	/* Close everything */

	exit(ret);
}
