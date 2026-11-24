#!/usr/bin/make

KVER=
ifeq ($(KVER),)
    KVER=$(shell uname -r)
endif

PREFIX=

default: build


l2install:
	make -C bnxt_en KVER=$(KVER) PREFIX=$(PREFIX) install

l2clean:
	make -C bnxt_en clean

roceinstall:
	make -C bnxt_re KVER=$(KVER) PREFIX=$(PREFIX) install

roceclean:
	make -C bnxt_re clean

build:
	make -C bnxt_en KVER=$(KVER) PREFIX=$(PREFIX)
	make -C bnxt_re KVER=$(KVER) PREFIX=$(PREFIX)

install: build l2install roceinstall

clean: l2clean roceclean

.PHONEY: all clean install
                                         
