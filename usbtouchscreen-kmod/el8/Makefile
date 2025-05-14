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

# Example --
obj-m := usbtouchscreen.o

ccflags-y := -DCONFIG_TOUCHSCREEN_USB_EGALAX \
 -DCONFIG_TOUCHSCREEN_USB_PANJIT \
 -DCONFIG_TOUCHSCREEN_USB_3M \
 -DCONFIG_TOUCHSCREEN_USB_ITM \
 -DCONFIG_TOUCHSCREEN_USB_ETURBO \
 -DCONFIG_TOUCHSCREEN_USB_GUNZE \
 -DCONFIG_TOUCHSCREEN_USB_DMC_TSC10 \
 -DCONFIG_TOUCHSCREEN_USB_IRTOUCH \
 -DCONFIG_TOUCHSCREEN_USB_IDEALTEK \
 -DCONFIG_TOUCHSCREEN_USB_GENERAL_TOUCH \
 -DCONFIG_TOUCHSCREEN_USB_GOTOP \
 -DCONFIG_TOUCHSCREEN_USB_JASTEC \
 -DCONFIG_TOUCHSCREEN_USB_ELO \
 -DCONFIG_TOUCHSCREEN_USB_E2I \
 -DCONFIG_TOUCHSCREEN_USB_ZYTRONIC \
 -DCONFIG_TOUCHSCREEN_USB_ETT_TC45USB \
 -DCONFIG_TOUCHSCREEN_USB_NEXIO \
 -DCONFIG_TOUCHSCREEN_USB_EASYTOUCH
endif
