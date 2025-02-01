/*
 *   Copyright (C) International Business Machines Corp., 2000-2004
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
#ifndef _H_JFS_DTREE
#define	_H_JFS_DTREE

/*
 *	jfs_dtree.h: directory B+-tree manager
 */
#include "jfs_btree.h"

typedef union {
	struct {
		int           tid;
		struct inode  *ip;
		uint32_t      ino;
	} leaf;
	pxd_t xd;
} ddata_t;


/*
 *      entry segment/slot
 *
 * an entry consists of type dependent head/only segment/slot and
 * additional segments/slots linked vi next field;
 * N.B. last/only segment of entry is terminated by next = -1;
 */
/*
 *	directory page slot
 */
struct dtslot {
	int8_t   next;		/*  1: */
	int8_t   cnt;		/*  1: */
	UniChar  name[15];	/* 30: */
};				/* (32) */


#define DATASLOTSIZE	16
#define L2DATASLOTSIZE	 4
#define	DTSLOTSIZE	32
#define	L2DTSLOTSIZE	 5
#define DTSLOTHDRSIZE	 2
#define DTSLOTDATASIZE	30
#define DTSLOTDATALEN	15

/*
 *	 internal node entry head/only segment
 */
struct idtentry {
	pxd_t    xd;		/* 8: child extent descriptor */

	int8_t   next;		/* 1: */
	uint8_t  namlen;	/* 1: */
	UniChar  name[11];	/* 22: 2-byte aligned */
};				/* (32) */

#define DTIHDRSIZE	10
#define DTIHDRDATALEN	11

/* compute number of slots for entry */
#define	NDTINTERNAL(klen) ( ((4 + (klen)) + (15 - 1)) / 15 )


/*
 *	leaf node entry head/only segment
 *
 * 	For legacy filesystems, name contains 13 unichars -- no index field
 */
struct ldtentry {
	uint32_t  inumber;	/* 4: 4-byte aligned */
	int8_t    next;		/* 1: */
	uint8_t   namlen;	/* 1: */
	UniChar   name[11];	/* 22: 2-byte aligned */
	uint32_t  index;	/* 4: index into dir_table */
};				/* (32) */

#define DTLHDRSIZE		6
#define DTLHDRDATALEN_LEGACY	13	/* Old (OS/2) format */
#define DTLHDRDATALEN		11

/*
 * dir_table used for directory traversal during readdir
 */

/*
 * Maximum entry in inline directory table
 */
#define MAX_INLINE_DIRTABLE_ENTRY 13

struct dir_table_slot {
	uint8_t   rsrvd;  /* 1: */
	uint8_t   flag;	  /* 1: 0 if free */
	uint8_t   slot;	  /* 1: slot within leaf page of entry */
	uint8_t   addr1;  /* 1: upper 8 bits of leaf page address */
	uint32_t  addr2;  /* 4: lower 32 bits of leaf page address -OR- index
			                of next entry when this entry was deleted */
};			  /* (8) */

/*
 * flag values
 */
#define DIR_INDEX_VALID 1
#define DIR_INDEX_FREE  0

#define DTSaddress(dir_table_slot, address64)\
{\
	(dir_table_slot)->addr1 = ((uint64_t)address64) >> 32;\
	(dir_table_slot)->addr2 = __cpu_to_le32((address64) & 0xffffffff);\
}

#define addressDTS(dts)\
	( ((uint64_t)((dts)->addr1)) << 32 | __le32_to_cpu((dts)->addr2) )

/* compute number of slots for entry */
#define	NDTLEAF_LEGACY(klen)	( ((2 + (klen)) + (15 - 1)) / 15 )
#define	NDTLEAF	NDTINTERNAL


/*
 *	directory root page (in-line in on-disk inode):
 *
 * cf. dtpage_t below.
 */
typedef union {
	struct {
		struct dasd DASD;	    /* 16: DASD limit/usage info  F226941 */

		uint8_t   flag;	    /* 1: */
		int8_t    nextindex;/* 1: next free entry in stbl */
		int8_t    freecnt;	/* 1: free count */
		int8_t    freelist;	/* 1: freelist header */

		uint32_t  idotdot;	/* 4: parent inode number */

		int8_t    stbl[8];	/* 8: sorted entry index table */
	} header;		        /* (32) */

	struct dtslot  slot[9];
} dtroot_t;

#define DTROOTMAXSLOT	9

/*
 *	directory regular page:
 *
 *	entry slot array of 32 byte slot
 *
 * sorted entry slot index table (stbl):
 * contiguous slots at slot specified by stblindex,
 * 1-byte per entry
 *   512 byte block:  16 entry tbl (1 slot)
 *  1024 byte block:  32 entry tbl (1 slot)
 *  2048 byte block:  64 entry tbl (2 slot)
 *  4096 byte block: 128 entry tbl (4 slot)
 *
 * data area:
 *   512 byte block:  16 - 2 =  14 slot
 *  1024 byte block:  32 - 2 =  30 slot
 *  2048 byte block:  64 - 3 =  61 slot
 *  4096 byte block: 128 - 5 = 123 slot
 *
 * N.B. index is 0-based; index fields refer to slot index
 * except nextindex which refers to entry index in stbl;
 * end of entry stot list or freelist is marked with -1.
 */
typedef union {
	struct {
		int64_t  next;	    /* 8: next sibling */
		int64_t  prev;	    /* 8: previous sibling */

		uint8_t  flag;	    /* 1: */
		int8_t   nextindex;	/* 1: next entry index in stbl */
		int8_t   freecnt;	/* 1: */
		int8_t   freelist;	/* 1: slot index of head of freelist */

		uint8_t  maxslot;	/* 1: number of slots in page slot[] */
		int8_t   stblindex;	/* 1: slot index of start of stbl */
		uint8_t  rsrvd[2];	/* 2: */

		pxd_t    self;	    /* 8: self pxd */
	} header;		        /* (32) */

	struct dtslot  slot[128];
} dtpage_t;

#define DTPAGEMAXSLOT        128

#define DT8THPGNODEBYTES     512
#define DT8THPGNODETSLOTS      1
#define DT8THPGNODESLOTS      16

#define DTQTRPGNODEBYTES    1024
#define DTQTRPGNODETSLOTS      1
#define DTQTRPGNODESLOTS      32

#define DTHALFPGNODEBYTES   2048
#define DTHALFPGNODETSLOTS     2
#define DTHALFPGNODESLOTS     64

#define DTFULLPGNODEBYTES   4096
#define DTFULLPGNODETSLOTS     4
#define DTFULLPGNODESLOTS    128

#define DTENTRYSTART	1

/* get sorted entry table of the page */
#define DT_GETSTBL(p) ( ((p)->header.flag & BT_ROOT) ?\
	((dtroot_t *)(p))->header.stbl : \
	(int8_t *)&(p)->slot[(p)->header.stblindex] )

/*
 * Flags for dtSearch
 */
#define JFS_CREATE 1
#define JFS_LOOKUP 2
#define JFS_REMOVE 3
#define JFS_RENAME 4

#endif				/* !_H_JFS_DTREE */
