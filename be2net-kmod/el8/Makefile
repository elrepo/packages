# SPDX-License-Identifier: GPL-2.0-only
#
# Makefile to build the network driver for ServerEngine's BladeEngine.
#

ccflags-y += -DCONFIG_BE2NET_BE2 -DCONFIG_BE2NET_BE3

obj-$(CONFIG_BE2NET) += be2net.o

be2net-y :=  be_main.o be_cmds.o be_ethtool.o be_roce.o
