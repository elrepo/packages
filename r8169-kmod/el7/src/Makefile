################################################################################
# 
# r8169 is the Linux device driver released for RealTek RTL8169S/8110S, 
# RTL8169SB/8110SB, and RTL8110SC Gigabit Ethernet controllers with 
# PCI interface.
# 
# Copyright(c) 2013 Realtek Semiconductor Corp. All rights reserved.
# 
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the Free
# Software Foundation; either version 2 of the License, or (at your option)
# any later version.
# 
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
# more details.
# 
# You should have received a copy of the GNU General Public License along with
# this program; if not, see <http://www.gnu.org/licenses/>.
# 
# Author:
# Realtek NIC software team <nicfae@realtek.com>
# No. 2, Innovation Road II, Hsinchu Science Park, Hsinchu 300, Taiwan
# 
################################################################################

################################################################################
# This product is covered by one or more of the following patents:
# US5,307,459, US5,434,872, US5,732,094, US6,570,884, US6,115,776, and US6,327,625.
################################################################################

ifneq ($(KERNELRELEASE),)
	obj-m := r8169.o
	r8169-objs := r8169_n.o
	EXTRA_CFLAGS += -DCONFIG_R8169_NAPI
	EXTRA_CFLAGS += -DCONFIG_R8169_VLAN
else
	BASEDIR := /lib/modules/$(shell uname -r)
	KERNELDIR ?= $(BASEDIR)/build
	PWD :=$(shell pwd)
	DRIVERDIR := $(shell find $(BASEDIR)/kernel/drivers/net -name realtek -type d)
	ifeq ($(DRIVERDIR),)
		DRIVERDIR := $(BASEDIR)/kernel/drivers/net
	endif
	RTKDIR := $(subst $(BASEDIR)/,,$(DRIVERDIR))

.PHONY: all
all: clean modules install

.PHONY:modules
modules:
	$(MAKE) -C $(KERNELDIR) SUBDIRS=$(PWD) modules

.PHONY:clean
clean:
	$(MAKE) -C $(KERNELDIR) SUBDIRS=$(PWD) clean

.PHONY:install
install:
	$(MAKE) -C $(KERNELDIR) SUBDIRS=$(PWD) INSTALL_MOD_DIR=$(RTKDIR) modules_install

endif

