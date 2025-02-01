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
#ifndef	_H_JFS_BTREE
#define _H_JFS_BTREE
/*
 *	jfs_btree.h: B+-tree
 *
 * JFS B+-tree (dtree and xtree) common definitions
 */

/*
 *	basic btree page - btpage
 */
struct btpage {
	int64_t next;		 /* 8: right sibling bn */
	int64_t prev;		 /* 8: left sibling bn */

	uint8_t flag;		/* 1: */
	uint8_t rsrvd[7];	/* 7: type specific */
	int64_t self;		/* 8: self address */

	uint8_t entry[4064];	/* 4064: */
};				/* (4096) */

/* btpage flag */
#define BT_TYPE       0x07	/* B+-tree index */
#define	BT_ROOT       0x01	/* root page */
#define	BT_LEAF       0x02	/* leaf page */
#define	BT_INTERNAL   0x04	/* internal page */
#define	BT_RIGHTMOST  0x10	/* rightmost page */
#define	BT_LEFTMOST   0x20	/* leftmost page */
#define	BT_SWAPPED    0x80	/* endian swapped when read from disk */

#endif				/* _H_JFS_BTREE */
