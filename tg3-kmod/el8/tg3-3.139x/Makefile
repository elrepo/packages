#!/usr/bin/make
#
# Makefile for building Linux Broadcom Gigabit ethernet driver as a module.
# Copyright (C) 2007-2019 Broadcom.  All Rights Reserved.
# The term "Broadcom" refers to Broadcom Limited and/or its subsidiaries.
#

# Uncomment the following line if you want EEE support completely removed.
#TG3_EXTRA_DEFS += TG3_NO_EEE

KVER=
ifeq ($(KVER),)
	KVER=$(shell uname -r)
endif

KVER_MAJ=$(shell echo $(KVER) | cut -d "." -f1)

BCMPROC=
ifeq ($(BCMPROC),)
	BCMPROC=$(shell uname -m)
endif

ifeq ($(BCMCFGDIR),)
	BCMCFGS := /lib/modules/$(KVER)/build \
	           /usr/src/kernels/$(KVER)-$(BCMPROC) \
	           /usr/src/linux-$(shell echo $(KVER) | sed "s|\(.*\)-\([a-z]*\)|\1-obj/$(shell uname -i)/\2|") \
	           /usr/src/linux

	# Delete any entries that do not exist.
	BCMTSTDIR = $(shell if ls $(DIR) > /dev/null 2>&1; then echo $(DIR); fi)
	BCMCFGS := $(foreach DIR, $(BCMCFGS), $(BCMTSTDIR))

	ifeq ($(BCMCFGS),)
		$(error Linux kernel config directory not found)
	else
		BCMCFGDIR := $(firstword $(BCMCFGS))
	endif
endif

ifeq ($(BCMSRCDIR),)
	BCMSRCS := /lib/modules/$(KVER)/source \
	           /lib/modules/$(KVER)/build  \
	           /usr/src/kernels/$(KVER)-$(BCMPROC) \
	           /usr/src/linux-$(shell echo $(KVER) | sed "s|\(.*\)-[a-z]*|\1|") \
	           /usr/src/linux

	# Delete any entries that do not exist.
	BCMTSTDIR = $(shell if ls $(DIR)/include/linux > /dev/null 2>&1; then \
	                        echo $(DIR); \
	                    fi)
	BCMSRCS := $(foreach DIR, $(BCMSRCS), $(BCMTSTDIR))

	ifeq ($(BCMSRCS),)
		$(error Linux kernel source directory not found)
	else
		BCMSRCDIR := $(firstword $(BCMSRCS))
	endif
endif

# PREFIX may be set by the RPM build to set the effective root.
PREFIX=

ifeq ($(BCMMODDIR),)
	ifeq ($(shell ls /lib/modules/$(KVER)/updates > /dev/null 2>&1 && echo 1),1)
		BCMMODDIR=/lib/modules/$(KVER)/updates
	else
		ifeq ($(shell grep -q "search.*[[:space:]]updates" /etc/depmod.conf > /dev/null 2>&1 && echo 1),1)
			BCMMODDIR=/lib/modules/$(KVER)/updates
		else
			ifeq ($(shell grep -q "search.*[[:space:]]updates" /etc/depmod.d/* > /dev/null 2>&1 && echo 1),1)
				BCMMODDIR=/lib/modules/$(KVER)/updates
			else
				ifeq ($(shell expr $(KVER_MAJ) \>= 3), 1)
					BCMMODDIR=/lib/modules/$(KVER)/updates
				else
					BCMMODDIR=/lib/modules/$(KVER)/kernel/drivers/net
				endif
			endif
		endif
	endif
endif

# check if 2.4 kernel or 2.5+ kernel
BCM_KVER := $(shell echo $(KVER) | cut -c1-3)

ifneq ($(BCM_KVER), 2.4)
# Makefile for 2.5+ kernel

BCM_DRV = tg3.ko

ifneq ($(KERNELRELEASE),)

obj-m += tg3.o

else

default: tg3_flags.h
	make -C $(BCMCFGDIR) M=$(shell pwd) modules

endif

else # ifneq ($(BCM_KVER),2.4)
# Makefile for 2.4 kernel

BCM_DRV = tg3.o

CC = gcc

CFLAGS=-DMODULE -D__KERNEL__ -Wall -Wstrict-prototypes -I$(BCMCFGDIR)/include -fno-strict-aliasing

ifeq ($(shell grep netif_poll_disable $(BCMSRCDIR)/include/linux/netdevice.h > /dev/null 2>&1 && echo newnetif),)
  CFLAGS+=-DOLD_NETIF
endif

ARCH:=$(shell uname -m)

ifeq ($(ARCH),x86_64)
  CFLAGS+=-mno-red-zone -mcmodel=kernel -pipe -finline-limit=2000
endif

ifeq ($(ARCH),ia64)
  CFLAGS+=-pipe -ffixed-r13 -mfixed-range=f10-f15,f32-f127 -falign-functions=32
endif

ifeq ($(ARCH),ppc64)
  ifneq ($(shell ls /opt/cross/bin/powerpc64-linux-gcc > /dev/null 2>&1 && echo ppcgcc),)
    CC=/opt/cross/bin/powerpc64-linux-gcc
  endif

  CFLAGS+=-fno-common -fomit-frame-pointer -msoft-float -pipe -mminimal-toc -fno-builtin
endif

CFLAGS += -O2

all: tg3_flags.h tg3.o

endif # ifneq ($(BCM_KVER),2.4)

tg3_flags.h:
	sh makeflags.sh $(BCMSRCDIR) $(TG3_EXTRA_DEFS) > tg3_flags.h

tg3.4.gz:
	gzip -c tg3.4 > tg3.4.gz


ifneq ($(BCM_KVER), 2.4)
install: default tg3.4.gz
else
install: $(BCM_DRV) tg3.4.gz
endif
	mkdir -p $(PREFIX)/$(BCMMODDIR);
	install -m 444 $(BCM_DRV) $(PREFIX)/$(BCMMODDIR);
	@if [ "$(PREFIX)" = "" ]; then /sbin/depmod -a ;\
	else echo " *** Run '/sbin/depmod -a' to update the module database.";\
	fi
	install -m 444 tg3.4.gz $(PREFIX)/usr/share/man/man4;\

.PHONEY: all clean install

clean:
	-rm -f tg3.o tg3.mod.c tg3.mod.o .tg3*
	-rm -f tg3.ko tg3.ko.unsigned
	-rm -f tg3.4.gz tg3_flags.h
	-rm -f Module.symvers Modules.symvers modules.order
	-rm -rf .tmp_versions Module.markers

