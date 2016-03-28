obj-m 		+= rtlwifi.o
rtlwifi-objs	:=		\
		base.o		\
		cam.o		\
		core.o		\
		debug.o		\
		efuse.o		\
		ps.o		\
		rc.o		\
		regd.o		\
		stats.o


obj-m	+= rtl_pci.o
rtl_pci-objs	:= pci.o


obj-m		+= rtl8723be/
obj-m		+= btcoexist/
obj-m		+= rtl8723com/

ccflags-y += -D__CHECK_ENDIAN__
