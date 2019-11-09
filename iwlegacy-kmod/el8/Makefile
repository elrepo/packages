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

obj-m			+= iwlegacy.o
iwlegacy-objs 		:= common.o debug.o

# 4965
obj-m			+= iwl4965.o
iwl4965-objs		:= 4965.o 4965-mac.o 4965-rs.o 4965-calib.o 4965-debug.o

# 3945
obj-m			+= iwl3945.o
iwl3945-objs		:= 3945-mac.o 3945.o 3945-rs.o 3945-debug.o

endif
