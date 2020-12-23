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

r8188eu-objs :=	core/rtw_ap.o		\
		core/rtw_cmd.o		\
		core/rtw_debug.o	\
		core/rtw_efuse.o	\
		core/rtw_ieee80211.o	\
		core/rtw_ioctl_set.o	\
		core/rtw_iol.o		\
		core/rtw_led.o		\
		core/rtw_mlme.o		\
		core/rtw_mlme_ext.o	\
		core/rtw_pwrctrl.o	\
		core/rtw_recv.o		\
		core/rtw_rf.o		\
		core/rtw_security.o	\
		core/rtw_sreset.o	\
		core/rtw_sta_mgt.o	\
		core/rtw_wlan_util.o	\
		core/rtw_xmit.o		\
		hal/fw.o	\
		hal/mac_cfg.o \
		hal/bb_cfg.o \
		hal/rf_cfg.o \
		hal/pwrseqcmd.o \
		hal/pwrseq.o \
		hal/hal8188e_rate_adaptive.o \
		hal/hal_intf.o		\
		hal/hal_com.o		\
		hal/odm.o		\
		hal/odm_hwconfig.o	\
		hal/odm_rtl8188e.o	\
		hal/rtl8188e_cmd.o	\
		hal/rtl8188e_dm.o	\
		hal/rtl8188e_hal_init.o	\
		hal/phy.o \
		hal/rf.o \
		hal/rtl8188e_rxdesc.o	\
		hal/rtl8188e_xmit.o	\
		hal/rtl8188eu_led.o	\
		hal/rtl8188eu_recv.o	\
		hal/rtl8188eu_xmit.o	\
		hal/usb_halinit.o	\
		os_dep/ioctl_linux.o	\
		os_dep/mlme_linux.o	\
		os_dep/mon.o		\
		os_dep/os_intfs.o	\
		os_dep/osdep_service.o	\
		os_dep/recv_linux.o	\
		os_dep/rtw_android.o	\
		os_dep/usb_intf.o	\
		os_dep/usb_ops_linux.o	\
		os_dep/xmit_linux.o

obj-m := r8188eu.o

ccflags-y += -DCONFIG_WIRELESS_EXT -DCONFIG_WEXT_PRIV -DCONFIG_88EU_AP_MODE
ccflags-y += -DCONFIG_LIB80211 -DCONFIG_LIB80211_CRYPT_WEP -DCONFIG_LIB80211_CRYPT_CCMP

ccflags-y += -I$(src)/include

endif
