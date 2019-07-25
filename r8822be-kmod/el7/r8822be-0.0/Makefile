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

ccflags-y +=	-DRTLWIFI_DEBUG_ST

obj-m 		+= r8822be.o

r8822be-objs	:=		\
		base.o		\
		cam.o		\
		core.o		\
		debug.o		\
		efuse.o		\
		ps.o		\
		rc.o		\
		regd.o		\
		stats.o		\
		pci.o		\
		rtl8822be/fw.o	\
		rtl8822be/hw.o	\
		rtl8822be/led.o	\
		rtl8822be/phy.o	\
		rtl8822be/sw.o	\
		rtl8822be/trx.o	\
		btcoexist/halbtc8822b2ant.o	\
		btcoexist/halbtc8822b1ant.o	\
		btcoexist/halbtc8822bwifionly.o	\
		btcoexist/halbtcoutsrc.o	\
		btcoexist/rtl_btc.o		\
		halmac/halmac_api.o	\
		halmac/halmac_88xx/halmac_api_88xx_usb.o	\
	        halmac/halmac_88xx/halmac_api_88xx_sdio.o	\
	        halmac/halmac_88xx/halmac_api_88xx.o	\
	        halmac/halmac_88xx/halmac_api_88xx_pcie.o	\
	        halmac/halmac_88xx/halmac_func_88xx.o	\
	        halmac/halmac_88xx/halmac_8822b/halmac_api_8822b_pcie.o	\
	        halmac/halmac_88xx/halmac_8822b/halmac_func_8822b.o	\
	        halmac/halmac_88xx/halmac_8822b/halmac_api_8822b_sdio.o	\
	        halmac/halmac_88xx/halmac_8822b/halmac_api_8822b.o	\
	        halmac/halmac_88xx/halmac_8822b/halmac_8822b_phy.o	\
	        halmac/halmac_88xx/halmac_8822b/halmac_8822b_pwr_seq.o	\
	        halmac/halmac_88xx/halmac_8822b/halmac_api_8822b_usb.o	\
	        halmac/rtl_halmac.o					\
		phydm/phydm_debug.o	\
		phydm/phydm_antdiv.o\
		phydm/phydm_interface.o\
		phydm/phydm_hwconfig.o\
		phydm/phydm.o\
		phydm/halphyrf_ce.o\
		phydm/phydm_edcaturbocheck.o\
		phydm/phydm_dig.o\
		phydm/phydm_rainfo.o\
		phydm/phydm_dynamicbbpowersaving.o\
		phydm/phydm_powertracking_ce.o\
		phydm/phydm_dynamictxpower.o\
		phydm/phydm_adaptivity.o\
		phydm/phydm_cfotracking.o\
		phydm/phydm_noisemonitor.o\
		phydm/phydm_acs.o\
		phydm/phydm_psd.o\
		phydm/phydm_adc_sampling.o\
		phydm/phydm_kfree.o\
		phydm/phydm_ccx.o		\
		phydm/rtl8822b/halhwimg8822b_bb.o\
		phydm/rtl8822b/halhwimg8822b_mac.o\
		phydm/rtl8822b/halhwimg8822b_rf.o\
		phydm/rtl8822b/halphyrf_8822b.o\
		phydm/rtl8822b/phydm_hal_api8822b.o\
		phydm/rtl8822b/phydm_iqk_8822b.o\
		phydm/rtl8822b/phydm_regconfig8822b.o\
		phydm/rtl8822b/phydm_rtl8822b.o	\
		phydm/rtl_phydm.o

obj-m		+= rtl8822be/

endif
