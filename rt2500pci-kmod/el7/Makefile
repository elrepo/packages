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

rt2x00lib-y += rt2x00dev.o
rt2x00lib-y += rt2x00mac.o
rt2x00lib-y += rt2x00config.o
rt2x00lib-y += rt2x00queue.o
rt2x00lib-y += rt2x00link.o
rt2x00lib-y += rt2x00debug.o
rt2x00lib-y += rt2x00crypto.o
rt2x00lib-y += rt2x00firmware.o
rt2x00lib-y += rt2x00leds.o

obj-m += rt2500pci.o
obj-m += rt2x00lib.o
obj-m += rt2x00mmio.o
obj-m += rt2x00pci.o

endif
