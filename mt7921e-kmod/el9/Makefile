# SPDX-License-Identifier: GPL-2.0-only
obj-m += mt76.o
obj-m += mt76-connac-lib.o

mt76-objs := mmio.o util.o trace.o dma.o mac80211.o debugfs.o eeprom.o \
	     tx.o agg-rx.o mcu.o pci.o

CFLAGS_trace.o := -I$(src)

mt76-connac-lib-objs := mt76_connac_mcu.o mt76_connac_mac.o

obj-m += mt7921/
