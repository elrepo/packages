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

obj-m		+= mlx4_core.o

mlx4_core-objs :=	alloc.o catas.o cmd.o cq.o eq.o fw.o fw_qos.o icm.o intf.o \
			main.o mcg.o mr.o pd.o port.o profile.o qp.o reset.o sense.o \
			srq.o resource_tracker.o crdump.o

ccflags-y += -DCONFIG_MLX4_CORE_GEN2

obj-m		+= mlx4_en.o

mlx4_en-objs := 	en_main.o en_tx.o en_rx.o en_ethtool.o en_port.o en_cq.o \
			en_resources.o en_netdev.o en_selftest.o en_clock.o en_dcb_nl.o

endif
