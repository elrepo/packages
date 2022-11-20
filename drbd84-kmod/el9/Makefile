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


# SPDX-License-Identifier: GPL-2.0

# Add lru_cache [https://elrepo.org/bugs/view.php?id=1075#c7455]
drbd-y := lru_cache.o
drbd-y += drbd_bitmap.o drbd_proc.o
drbd-y += drbd_worker.o drbd_receiver.o drbd_req.o drbd_actlog.o
drbd-y += drbd_main.o drbd_strings.o drbd_nl.o
drbd-y += drbd_interval.o drbd_state.o
drbd-y += drbd_nla.o
drbd-$(CONFIG_DEBUG_FS) += drbd_debugfs.o

obj-$(CONFIG_BLK_DEV_DRBD)     += drbd.o

endif
