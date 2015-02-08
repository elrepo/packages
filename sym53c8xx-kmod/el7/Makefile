# Makefile for the NCR/SYMBIOS/LSI 53C8XX PCI SCSI controllers driver.

sym53c8xx-y := sym_fw.o sym_glue.o sym_hipd.o sym_malloc.o sym_nvram.o
obj-m := sym53c8xx.o

ccflags-y += -DCONFIG_SCSI_SYM53C8XX_DMA_ADDRESSING_MODE=1 \
             -DCONFIG_SCSI_SYM53C8XX_DEFAULT_TAGS=16 \
             -DCONFIG_SCSI_SYM53C8XX_MAX_TAGS=64 \
             -DCONFIG_SCSI_SYM53C8XX_MMIO=y

