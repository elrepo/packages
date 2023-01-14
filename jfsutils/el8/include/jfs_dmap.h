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
#ifndef	_H_JFS_DMAP
#define _H_JFS_DMAP

#include "jfs_types.h"

#define BMAPVERSION	1	/* version number */
#define	TREESIZE	(256+64+16+4+1)	/* size of a dmap tree */
#define	LEAFIND		(64+16+4+1)	/* index of 1st leaf of a dmap tree */
#define LPERDMAP	256	/* num leaves per dmap tree */
#define L2LPERDMAP	8	/* l2 number of leaves per dmap tree */
#define	DBWORD		32	/* # of blks covered by a map word */
#define	L2DBWORD	5	/* l2 # of blks covered by a mword */
#define BUDMIN  	L2DBWORD	/* max free string in a map word */
#define BPERDMAP	(LPERDMAP * DBWORD)	/* num of blks per dmap */
#define L2BPERDMAP	13	/* l2 num of blks per dmap */
#define CTLTREESIZE	(1024+256+64+16+4+1)	/* size of a dmapctl tree */
#define CTLLEAFIND	(256+64+16+4+1)	/* idx of 1st leaf of a dmapctl tree */
#define LPERCTL		1024	/* num of leaves per dmapctl tree */
#define L2LPERCTL	10	/* l2 num of leaves per dmapctl tree */
#define	ROOT		0	/* index of the root of a tree */
#define	NOFREE		((int8_t) -1)	/* no blocks free */
#define	MAXAG		128	/* max number of allocation groups */
#define L2MAXAG		7	/* l2 max num of AG */
#define L2MINAGSZ	25	/* l2 of minimum AG size in bytes */
#define	BMAPBLKNO	0	/* lblkno of bmap within the map */

/*
 * maximum l2 number of disk blocks at the various dmapctl levels.
 */
#define	L2MAXL0SIZE	(L2BPERDMAP + 1 * L2LPERCTL)
#define	L2MAXL1SIZE	(L2BPERDMAP + 2 * L2LPERCTL)
#define	L2MAXL2SIZE	(L2BPERDMAP + 3 * L2LPERCTL)

/*
 * maximum number of disk blocks at the various dmapctl levels.
 */
#define	MAXL0SIZE	((int64_t)1 << L2MAXL0SIZE)
#define	MAXL1SIZE	((int64_t)1 << L2MAXL1SIZE)
#define	MAXL2SIZE	((int64_t)1 << L2MAXL2SIZE)

#define	MAXMAPSIZE	MAXL2SIZE	/* maximum aggregate map size */

/*
 * determine the maximum free string for four (lower level) nodes
 * of the tree.
 */
#define	TREEMAX(cp)					\
	((signed char)(MAX(MAX(*(cp),*((cp)+1)),	\
	                   MAX(*((cp)+2),*((cp)+3)))))

/*
 * convert disk block number to the logical block number of the dmap
 * describing the disk block.  s is the log2(number of logical blocks per page)
 *
 * The calculation figures out how many logical pages are in front of the dmap.
 *	- the number of dmaps preceding it
 *	- the number of L0 pages preceding its L0 page
 *	- the number of L1 pages preceding its L1 page
 *	- 3 is added to account for the L2, L1, and L0 page for this dmap
 *	- 1 is added to account for the control page of the map.
 */
#define BLKTODMAP(b,s)    \
        ((((b) >> 13) + ((b) >> 23) + ((b) >> 33) + 3 + 1) << (s))

/*
 * convert disk block number to the logical block number of the LEVEL 0
 * dmapctl describing the disk block.  s is the log2(number of logical blocks
 * per page)
 *
 * The calculation figures out how many logical pages are in front of the L0.
 *	- the number of dmap pages preceding it
 *	- the number of L0 pages preceding it
 *	- the number of L1 pages preceding its L1 page
 *	- 2 is added to account for the L2, and L1 page for this L0
 *	- 1 is added to account for the control page of the map.
 */
#define BLKTOL0(b,s)      \
        (((((b) >> 23) << 10) + ((b) >> 23) + ((b) >> 33) + 2 + 1) << (s))

/*
 * convert disk block number to the logical block number of the LEVEL 1
 * dmapctl describing the disk block.  s is the log2(number of logical blocks
 * per page)
 *
 * The calculation figures out how many logical pages are in front of the L1.
 *	- the number of dmap pages preceding it
 *	- the number of L0 pages preceding it
 *	- the number of L1 pages preceding it
 *	- 1 is added to account for the L2 page
 *	- 1 is added to account for the control page of the map.
 */
#define BLKTOL1(b,s)      \
     (((((b) >> 33) << 20) + (((b) >> 33) << 10) + ((b) >> 33) + 1 + 1) << (s))

/*
 * convert disk block number to the logical block number of the dmapctl
 * at the specified level which describes the disk block.
 */
#define BLKTOCTL(b,s,l)   \
        (((l) == 2) ? 1 : ((l) == 1) ? BLKTOL1((b),(s)) : BLKTOL0((b),(s)))

/*
 * convert aggregate map size to the zero origin dmapctl level of the
 * top dmapctl.
 */
#define	BMAPSZTOLEV(size)	\
	(((size) <= MAXL0SIZE) ? 0 : ((size) <= MAXL1SIZE) ? 1 : 2)

/* convert disk block number to allocation group number.
 */
#define BLKTOAG(b,sb)	((b) >> ((sb)->s_jfs_bmap->db_agl2size))

/* convert allocation group number to starting disk block
 * number.
 */
#define AGTOBLK(a,ip)	\
	((int64_t)(a) << ((ip)->i_sb->s_jfs_bmap->db_agl2size))

/*
 *	dmap summary tree
 *
 * struct dmaptree must be consistent with struct dmapctl.
 */
struct dmaptree {
	int32_t  nleafs;		/* 4: number of tree leafs      */
	int32_t  l2nleafs;		/* 4: l2 number of tree leafs   */
	int32_t  leafidx;		/* 4: index of first tree leaf  */
	int32_t  height;		/* 4: height of the tree        */
	int8_t   budmin;		/* 1: min l2 tree leaf value to combine */
	int8_t   stree[TREESIZE];	/* TREESIZE: tree               */
	uint8_t  pad[2];		/* 2: pad to word boundary      */
};			    /* - 360 -                      */

/*
 *	dmap page per 8K blocks bitmap
 */
struct dmap {
	int32_t     nblocks;		/* 4: num blks covered by this dmap     */
	int32_t     nfree;		    /* 4: num of free blks in this dmap     */
	int64_t     start;		    /* 8: starting blkno for this dmap      */
	struct dmaptree tree;	        /* 360: dmap tree                       */
	uint8_t     pad[1672];		/* 1672: pad to 2048 bytes              */
	uint32_t    wmap[LPERDMAP];	/* 1024: bits of the working map        */
	uint32_t    pmap[LPERDMAP];	/* 1024: bits of the persistent map     */
};				            /* - 4096 -                             */

/*
 *	disk map control page per level.
 *
 * struct dmapctl must be consistent with struct dmaptree.
 */
struct dmapctl {
	int32_t  nleafs;		/* 4: number of tree leafs         */
	int32_t  l2nleafs;		/* 4: l2 number of tree leafs      */
	int32_t  leafidx;		/* 4: index of the first tree leaf */
	int32_t  height;		/* 4: height of tree               */
	int8_t   budmin;		/* 1: minimum l2 tree leaf value   */
	int8_t   stree[CTLTREESIZE];/* CTLTREESIZE: dmapctl tree   */
	uint8_t  pad[2714];		/* 2714: pad to 4096               */
};		     		/* - 4096 -                        */

/*
 *	common definition for dmaptree within dmap and dmapctl
 */
typedef union {
	struct dmaptree t1;
	struct dmapctl t2;
} dmtree_t;

/* macros for accessing fields within dmtree_t */
#define	dmt_nleafs	  t1.nleafs
#define	dmt_l2nleafs  t1.l2nleafs
#define	dmt_leafidx   t1.leafidx
#define	dmt_height 	  t1.height
#define	dmt_budmin 	  t1.budmin
#define	dmt_stree 	  t1.stree

/*
 *	on-disk aggregate disk allocation map descriptor.
 */
struct dbmap {
	int64_t  dn_mapsize;		/* 8: number of blocks in aggregate     */
	int64_t  dn_nfree;		    /* 8: num free blks in aggregate map    */
	int32_t  dn_l2nbperpage;	/* 4: number of blks per page           */
	int32_t  dn_numag;		    /* 4: total number of ags               */
	int32_t  dn_maxlevel;	    /* 4: number of active ags              */
	int32_t  dn_maxag;		    /* 4: max active alloc group number     */
	int32_t  dn_agpref;	    	/* 4: preferred alloc group (hint)      */
	int32_t  dn_aglevel;		/* 4: dmapctl level holding the AG      */
	int32_t  dn_agheigth;	    /* 4: height in dmapctl of the AG       */
	int32_t  dn_agwidth;		/* 4: width in dmapctl of the AG        */
	int32_t  dn_agstart;		/* 4: start tree index at AG height     */
	int32_t  dn_agl2size;	    /* 4: l2 num of blks per alloc group    */
	int64_t  dn_agfree[MAXAG];  /* 8*MAXAG: per AG free count           */
	int64_t  dn_agsize;		    /* 8: num of blks per alloc group       */
	int8_t   dn_maxfreebud;	    /* 1: max free buddy system             */
	uint8_t  pad[3007];		    /* 3007: pad to 4096                    */
};			            /* - 4096 -                             */

#endif				/* _H_JFS_DMAP */
