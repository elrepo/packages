# SPDX-License-Identifier: GPL-2.0
#
## Makefile for the linux hfsplus filesystem routines.
#

obj-m += hfsplus.o

hfsplus-objs := super.o options.o inode.o ioctl.o extents.o catalog.o dir.o btree.o \
		bnode.o brec.o bfind.o tables.o unicode.o wrapper.o bitmap.o part_tbl.o \
		attributes.o xattr.o xattr_user.o xattr_security.o xattr_trusted.o

# hfsplus-$(CONFIG_HFSPLUS_FS_POSIX_ACL)	+= posix_acl.o
