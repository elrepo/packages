/*
 *   Copyright (c) International Business Machines Corp., 2000-2002
 *
 *   This program is free software;  you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY;  without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program;  if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#ifndef _H_JFS_XTREE
#define _H_JFS_XTREE

/*
 *      jfs_xtree.h: extent allocation descriptor B+-tree manager
 */

#include "jfs_btree.h"

/*
 *      extent allocation descriptor (xad)
 */
typedef struct xad {
	unsigned flag:8;	/* 1: flag */
	unsigned rsvrd:16;	/* 2: reserved */
	unsigned off1:8;	/* 1: offset in unit of fsblksize */
	uint32_t off2;		/* 4: offset in unit of fsblksize */
	unsigned len:24;	/* 3: length in unit of fsblksize */
	unsigned addr1:8;	/* 1: address in unit of fsblksize */
	uint32_t addr2;		/* 4: address in unit of fsblksize */
} xad_t;			/* (16) */

#define MAXXLEN         ((1 << 24) - 1)

#define XTSLOTSIZE      16
#define L2XTSLOTSIZE    4

/* xad_t field construction */
#define XADoffset(xad, offset64)\
{\
        (xad)->off1 = ((uint64_t)offset64) >> 32;\
        (xad)->off2 = __cpu_to_le32((offset64) & 0xffffffff);\
}
#define XADaddress(xad, address64)\
{\
        (xad)->addr1 = ((uint64_t)address64) >> 32;\
        (xad)->addr2 = __cpu_to_le32((address64) & 0xffffffff);\
}
#define XADlength(xad, length32)        (xad)->len = __cpu_to_le24(length32)

/* xad_t field extraction */
#define offsetXAD(xad)\
        ( ((int64_t)((xad)->off1)) << 32 | __le32_to_cpu((xad)->off2))
#define addressXAD(xad)\
        ( ((int64_t)((xad)->addr1)) << 32 | __le32_to_cpu((xad)->addr2))
#define lengthXAD(xad)  __le24_to_cpu((xad)->len)

/* xad_t flags */
#define XAD_NEW         0x01	/* new */
#define XAD_EXTENDED    0x02	/* extended */
#define XAD_COMPRESSED  0x04	/* compressed with recorded length */
#define XAD_NOTRECORDED 0x08	/* allocated but not recorded */
#define XAD_COW         0x10	/* copy-on-write */


/* possible values for maxentry */
#define XTROOTINITSLOT_DIR  6
#define XTROOTINITSLOT     10
#define XTROOTMAXSLOT      18
#define XTPAGEMAXSLOT     256
#define XTENTRYSTART        2

/*
 *      xtree page:
 */
typedef union {
	struct xtheader {
		int64_t next;	    /* 8: */
		int64_t prev;	    /* 8: */

		uint8_t flag;	    /* 1: */
		uint8_t rsrvd1;	    /* 1: */
		int16_t nextindex;	/* 2: next index = number of entries */
		int16_t maxentry;	/* 2: max number of entries */
		int16_t rsrvd2;	    /* 2: */

		pxd_t   self;	    /* 8: self */
	} header;		        /* (32) */

	xad_t xad[XTPAGEMAXSLOT];	/* 16 * maxentry: xad array */
} xtpage_t;

#endif				/* !_H_JFS_XTREE */
