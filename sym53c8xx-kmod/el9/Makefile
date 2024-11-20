# SPDX-License-Identifier: GPL-2.0-only
# Makefile for the NCR/SYMBIOS/LSI 53C8XX PCI SCSI controllers driver.

ccflags-y += -DCONFIG_SCSI_SYM53C8XX_DMA_ADDRESSING_MODE=1

sym53c8xx-objs := sym_fw.o sym_glue.o sym_hipd.o sym_malloc.o sym_nvram.o
obj-m := sym53c8xx.o
