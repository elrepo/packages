#=============================================================================
#
#             --- CAEN SpA - Computing Systems Division ---
#
#  Makefile
#
#
#  January  2011 :   Created.
#  February 2013 :   Last Version
#
#=============================================================================

###############################
# Kernel >= 2.6
###############################
# Comment/uncomment the following line to disable/enable debugging
#DEBUG = y

# Add your debugging flag (or not) to EXTRA_CFLAGS 



ifeq ($(DEBUG),y)
  DEBFLAGS = -O -g # "-O" is needed to expand inlines
else
  DEBFLAGS = -O2 -Wall
endif
EXTRA_CFLAGS  += $(DEBFLAGS) -I$(LDDINCDIR)

KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD       := $(shell pwd)

ifneq ($(KERNELRELEASE),)
# call from kernel build system


ifneq ("$(wildcard $(KERNELDIR)/include/linux/pci-aspm.h)","")
	CAEN_ASPM_EXISTS = -DCAEN_ASPM_EXISTS
		
endif

ccflags-y := $(CAEN_ASPM_EXISTS)	


obj-m := src/a3818.o

endif



default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) LDDINCDIR=$(PWD)/include modules
	
install:
	@echo "KERNEL==\"a3818_[0-9]\", MODE=\"0666\"" > /etc/udev/rules.d/10-CAEN-A3818.rules
	@echo "KERNEL==\"a3818_[0-9][0-9]\", MODE=\"0666\"" >> /etc/udev/rules.d/10-CAEN-A3818.rules
	@udevadm control --reload-rules
	@cp src/*.ko /lib/modules/$(shell uname -r)/kernel/drivers/misc/
	@echo a3818 >> /etc/modules
	@if test -d /etc/sysconfig/modules; then \
	echo #!/bin/sh > /etc/sysconfig/modules/a3818.modules; \
	echo 'if [ ! -c /dev/a3818_0 ] ; then' >> /etc/sysconfig/modules/a3818.modules; \
	echo 'exec /sbin/modprobe  a3818 >/dev/null 2>&1' >> /etc/sysconfig/modules/a3818.modules; \
	echo fi >> /etc/sysconfig/modules/a3818.modules; \
	chmod +x /etc/sysconfig/modules/a3818.modules; \
	fi
	@echo "installing a3818 driver.. please wait"
	@depmod -a
	@modprobe a3818
	@echo "installation done"

uninstall:
	@modprobe -r a3818
	@rm /etc/udev/rules.d/10-CAEN-A3818.rules
	@udevadm control --reload-rules
	@rm /lib/modules/$(shell uname -r)/kernel/drivers/misc/a3818.ko
	@sed -i '/a3818/d' /etc/modules
	@if test -d /etc/sysconfig/modules; then \
	rm /etc/sysconfig/modules/a3818.modules; \
	fi
	@depmod -a
	@echo "driver uninstalled"
	
clean:
	rm -rf src/*.o src/*~ src/core src/.depend src/.*.cmd src/*.ko src/*.ko.* src/*.mod.c src/.tmp_versions src/*.mod ./modules.order ./Module.markers ./Module.symvers ./report.log
	
all:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) LDDINCDIR=$(PWD)/include modules
	
depend .depend dep:
	$(CC) $(EXTRA_CFLAGS) -M *.c > .depend


ifeq (.depend,$(wildcard .depend))
include .depend
endif

