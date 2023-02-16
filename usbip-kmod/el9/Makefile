# If KERNELRELEASE is defined, the make command using this Makefile has
# been invoked by the kernel build system and so can use its language.
# Otherwise, if KERNELRELEASE is null, a make command was issued from
# the command line. So invoke the kernel build system.

ifeq ($(KERNELRELEASE),)

    # KVERSION should be set in the environment if this
    # build is not for the currently running kernel.
    KVERSION ?= $(shell uname -r)

    # BUILD_DIR should be set in the environment if a
    # subdirectory of /lib/modules/ is not appropriate.
    BUILD_DIR ?= /lib/modules/${KVERSION}/build

    PWD := $(shell pwd)

modules:
	$(MAKE) -C $(BUILD_DIR) M=$(PWD) modules

modules_install:
	$(MAKE) -C $(BUILD_DIR) M=$(PWD) modules_install

clean:
	rm -rf *~ *.o .*.cmd *.mod.c *.ko *.ko.unsigned .depend \
	.tmp_versions modules.order Module.symvers Module.markers

.PHONY: modules modules_install clean

else

# Called from kernel build system -- just declare the module(s).
# SPDX-License-Identifier: GPL-2.0
ccflags-y := -DDEBUG

obj-m += usbip-core.o
usbip-core-y := usbip_common.o usbip_event.o

obj-m += vhci-hcd.o
vhci-hcd-y := vhci_sysfs.o vhci_tx.o vhci_rx.o vhci_hcd.o

obj-m += usbip-host.o
usbip-host-y := stub_dev.o stub_main.o stub_rx.o stub_tx.o

# obj-m += usbip-vudc.o
# usbip-vudc-y := vudc_dev.o vudc_sysfs.o vudc_tx.o vudc_rx.o vudc_transfer.o vudc_main.o

endif
