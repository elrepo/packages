# SPDX-License-Identifier: GPL-2.0
# reiserfs makefile

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

obj-m += reiserfs.o

reiserfs-objs := bitmap.o do_balan.o namei.o inode.o file.o dir.o fix_node.o \
		super.o prints.o objectid.o lbalance.o ibalance.o stree.o \
		hashes.o tail_conversion.o journal.o resize.o \
		item_ops.o ioctl.o xattr.o lock.o

ccflags-y += -DCONFIG_REISERFS_PROC_INFO
ccflags-y += -DCONFIG_REISERFS_FS_XATTR
ccflags-y += -DCONFIG_REISERFS_FS_SECURITY
ccflags-y += -DCONFIG_REISERFS_FS_POSIX_ACL

reiserfs-y := procfs.o xattr_user.o xattr_trusted.o xattr_security.o xattr_acl.o

endif
