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
#ifndef	_H_JFS_IMAP
#define _H_JFS_IMAP

/*
 *	jfs_imap.h: disk inode manager
 */

#define	EXTSPERIAG	128	/* number of disk inode extent per iag  */
#define IMAPBLKNO	  0	/* lblkno of dinomap within inode map   */
#define SMAPSZ		  4	/* number of words per summary map      */
#define	EXTSPERSUM	 32	/* number of extents per summary map entry */
#define	L2EXTSPERSUM  5	/* l2 number of extents per summary map */
#define	PGSPERIEXT	  4	/* number of 4K pages per dinode extent */
#define	MAXIAGS		((1<<20)-1)	/* maximum number of iags       */
#define	MAXAG		128	/* maximum number of allocation groups  */

#define AMAPSIZE    512	/* bytes in the IAG allocation maps */
#define SMAPSIZE     16	/* bytes in the IAG summary maps */

/* convert inode number to iag number */
#define	INOTOIAG(ino)	((ino) >> L2INOSPERIAG)

/* convert iag number to logical block number of the iag page */
#define IAGTOLBLK(iagno,l2nbperpg)	(((iagno) + 1) << (l2nbperpg))

/* get the starting block number of the 4K page of an inode extent
 * that contains ino.
 */
#define INOPBLK(pxd,ino,l2nbperpg)    	(addressPXD((pxd)) +		\
	((((ino) & (INOSPEREXT-1)) >> L2INOSPERPAGE) << (l2nbperpg)))

/*
 *	inode allocation map:
 *
 * inode allocation map consists of
 * . the inode map control page and
 * . inode allocation group pages (per 4096 inodes)
 * which are addressed by standard JFS xtree.
 */
/*
 *	inode allocation group page (per 4096 inodes of an AG)
 */
struct iag {
	int64_t   agstart;		       /* 8: starting block of ag              */
	int32_t   iagnum;		       /* 4: inode allocation group number     */
	int32_t   inofreefwd;		   /* 4: ag inode free list forward        */
	int32_t   inofreeback;	       /* 4: ag inode free list back           */
	int32_t   extfreefwd;		   /* 4: ag inode extent free list forward */
	int32_t   extfreeback;	       /* 4: ag inode extent free list back    */
	int32_t   iagfree;		       /* 4: iag free list                     */

	/* summary map: 1 bit per inode extent */
	int32_t   inosmap[SMAPSZ];	   /* 16: sum map of mapwords w/ free inodes;
				                    *     note: this indicates free and backed
				                    *     inodes, if the extent is not backed the
				                    *     value will be 1.  if the extent is
				                    *     backed but all inodes are being used the
				                    *     value will be 1.  if the extent is
				                    *     backed but at least one of the inodes is
				                    *     free the value will be 0.
				                    */
	int32_t   extsmap[SMAPSZ];	   /* 16: sum map of mapwords w/ free extents */
	int32_t   nfreeinos;		   /* 4: number of free inodes             */
	int32_t   nfreeexts;		   /* 4: number of free extents            */
	/* (72) */
	uint8_t   pad[1976];		   /* 1976: pad to 2048 bytes */
	/* allocation bit map: 1 bit per inode (0 - free, 1 - allocated) */
	uint32_t  wmap[EXTSPERIAG];	   /* 512: working allocation map  */
	uint32_t  pmap[EXTSPERIAG];	   /* 512: persistent allocation map */
	pxd_t     inoext[EXTSPERIAG];  /* 1024: inode extent addresses */
};			/* (4096) */

/*
 *	per AG control information (in inode map control page)
 */
struct iagctl {
	int32_t  inofree;		/* 4: free inode list anchor            */
	int32_t  extfree;		/* 4: free extent list anchor           */
	int32_t  numinos;		/* 4: number of backed inodes           */
	int32_t  numfree;		/* 4: number of free inodes             */
};			/* (16) */

/*
 *	per fileset/aggregate inode map control page
 */
struct dinomap {
	int32_t   in_freeiag;		/* 4: free iag list anchor     */
	int32_t   in_nextiag;		/* 4: next free iag number     */
	int32_t   in_numinos;		/* 4: num of backed inodes */
	int32_t   in_numfree;		/* 4: num of free backed inodes */
	int32_t   in_nbperiext;	    /* 4: num of blocks per inode extent */
	int32_t   in_l2nbperiext;	/* 4: l2 of in_nbperiext */
	int32_t   in_diskblock;	    /* 4: for standalone test driver  */
	int32_t   in_maxag;		    /* 4: for standalone test driver  */
	uint8_t   pad[2016];		/* 2016: pad to 2048 */
	struct iagctl  in_agctl[MAXAG];	/* 2048: AG control information */
};			/* (4096) */

#endif				/* _H_JFS_IMAP */
