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

aic7xxx-y := aic7xxx_core.o aic7xxx_93cx6.o aic7xxx_pci.o \
			aic7xxx_osm.o aic7xxx_proc.o aic7xxx_osm_pci.o

obj-m := aic7xxx.o

ccflags-y += -Idrivers/scsi \
			-DCONFIG_AIC7XXX_CMDS_PER_DEVICE=4 \
			-DCONFIG_AIC7XXX_RESET_DELAY_MS=15000 \
			-DCONFIG_AIC7XXX_DEBUG_MASK=0

endif
