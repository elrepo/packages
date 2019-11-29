# SPDX-License-Identifier: GPL-2.0
#
## Makefile for the linux hfsplus filesystem routines.
#
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

obj-m += hfsplus.o

hfsplus-objs := super.o options.o inode.o ioctl.o extents.o catalog.o dir.o btree.o \
		bnode.o brec.o bfind.o tables.o unicode.o wrapper.o bitmap.o part_tbl.o \
		attributes.o xattr.o xattr_user.o xattr_security.o xattr_trusted.o

# hfsplus-$(CONFIG_HFSPLUS_FS_POSIX_ACL)	+= posix_acl.o

endif
