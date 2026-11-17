# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the Linux "Tulip" family network device drivers.
#

ccflags-y := -DDEBUG

obj-m += tulip.o

tulip-objs := eeprom.o interrupt.o media.o \
		timer.o tulip_core.o \
		21142.o pnic.o pnic2.o
