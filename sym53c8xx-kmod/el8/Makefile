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

# SPDX-License-Identifier: GPL-2.0-only
# Makefile for the NCR/SYMBIOS/LSI 53C8XX PCI SCSI controllers driver.


#  config SCSI_SYM53C8XX_DMA_ADDRESSING_MODE
#  defualt "1"

#  This option only applies to PCI-SCSI chips that are PCI DAC
#  capable (875A, 895A, 896, 1010-33, 1010-66, 1000).

#  When set to 0, the driver will program the chip to only perform
#  32-bit DMA.  When set to 1, the chip will be able to perform DMA
#  to addresses up to 1TB.  When set to 2, the driver supports the
#  full 64-bit DMA address range, but can only address 16 segments
#  of 4 GB each.  This limits the total addressable range to 64 GB.

#  Most machines with less than 4GB of memory should use a setting
#  of 0 for best performance.  If your machine has 4GB of memory
#  or more, you should set this option to 1 (the default).

ccflags-y += -DCONFIG_SCSI_SYM53C8XX_DMA_ADDRESSING_MODE=1

sym53c8xx-objs := sym_fw.o sym_glue.o sym_hipd.o sym_malloc.o sym_nvram.o
obj-m := sym53c8xx.o

endif
