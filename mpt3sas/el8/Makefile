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
	rm -rf *~ *.o .*.cmd *.mod.c *.ko .depend .tmp_versions \
	modules.order Module.symvers Module.markers

.PHONY: modules modules_install clean

else

mpt3sas-objs += mpt3sas_base.o \
		mpt3sas_config.o \
		mpt3sas_scsih.o \
		mpt3sas_transport.o \
		mpt3sas_ctl.o \
		mpt3sas_trigger_diag.o \
		mpt3sas_warpdrive.o

obj-m += mpt3sas.o

endif
