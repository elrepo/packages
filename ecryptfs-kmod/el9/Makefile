# SPDX-License-Identifier: GPL-2.0-only
#
# Makefile for the Linux eCryptfs
#

obj-m += ecryptfs.o

ecryptfs-y := dentry.o file.o inode.o main.o super.o mmap.o read_write.o \
	      crypto.o keystore.o kthread.o debug.o

ecryptfs-$(CONFIG_ECRYPT_FS_MESSAGING) += messaging.o miscdev.o
