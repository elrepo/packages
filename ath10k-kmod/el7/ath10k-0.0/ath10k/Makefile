obj-m += ath10k_core.o
ath10k_core-y += mac.o \
		 debug.o \
		 core.o \
		 htc.o \
		 htt.o \
		 htt_rx.o \
		 htt_tx.o \
		 txrx.o \
		 wmi.o \
		 wmi-tlv.o \
		 bmi.o \
		 hw.o


ath10k_core-y += thermal.o
ath10k_core-y += debugfs_sta.o

obj-m += ath10k_pci.o
ath10k_pci-y += pci.o \
		ce.o

# for tracing framework to find trace.h
CFLAGS_trace.o := -I$(src)
