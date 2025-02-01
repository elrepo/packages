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
#ifndef _H_JFS_DINODE
#define _H_JFS_DINODE

/*
 *      jfs_dinode.h: on-disk inode manager
 *
 */
#include "jfs_types.h"
#include "jfs_dtree.h"
#include "jfs_xtree.h"

#define INODESLOTSIZE           128
#define L2INODESLOTSIZE         7
#define log2INODESIZE           9	/* log2(bytes per dinode) */


/*
 *      on-disk inode (struct dinode): 512 bytes
 *
 * note: align 64-bit fields on 8-byte boundary.
 */
struct dinode {
	/*
	 *      I. base area (128 bytes)
	 *      ------------------------
	 *
	 * define generic/POSIX attributes
	 */
	uint32_t  di_inostamp;	/* 4: stamp to show inode belongs to fileset */
	int32_t   di_fileset;	/* 4: fileset number */
	uint32_t  di_number;	/* 4: inode number, aka file serial number */
	uint32_t  di_gen;		/* 4: inode generation number */

	pxd_t     di_ixpxd;		/* 8: inode extent descriptor */

	int64_t   di_size;		/* 8: size */
	int64_t   di_nblocks;	/* 8: number of blocks allocated */

	uint32_t  di_nlink;		/* 4: number of links to the object */

	uint32_t  di_uid;		/* 4: user id of owner */
	uint32_t  di_gid;		/* 4: group id of owner */

	uint32_t  di_mode;		/* 4: attribute, format and permission */

	struct timestruc_t di_atime;	/* 8: time last data accessed */
	struct timestruc_t di_ctime;	/* 8: time last status changed */
	struct timestruc_t di_mtime;	/* 8: time last data modified */
	struct timestruc_t di_otime;	/* 8: time created */

	dxd_t     di_acl;		/* 16: acl descriptor */

	dxd_t     di_ea;		/* 16: ea descriptor */

	int32_t   di_next_index;/* 4: Next available dir_table index */

	int32_t   di_acltype;	/* 4: Type of ACL */

	/*
	 * 	Extension Areas.
	 *
	 *	Historically, the inode was partitioned into 4 128-byte areas,
	 *	the last 3 being defined as unions which could have multiple
	 *	uses.  The first 96 bytes had been completely unused until
	 *	an index table was added to the directory.  It is now more
	 *	useful to describe the last 3/4 of the inode as a single
	 *	union.  We would probably be better off redesigning the
	 *	entire structure from scratch, but we don't want to break
	 *	commonality with OS/2's JFS at this time.
	 */
	union {
		struct {
			/*
			 * This table contains the information needed to
			 * find a directory entry from a 32-bit index.
			 * If the index is small enough, the table is inline,
			 * otherwise, an x-tree root overlays this table
			 */
			struct dir_table_slot _table[12];	/* 96: inline */

			dtroot_t         _dtroot;		/* 288: dtree root */
		} _dir;					            /* (384) */
#define di_dirtable	u._dir._table
#define di_dtroot	u._dir._dtroot
#define di_parent   di_dtroot.header.idotdot
#define di_DASD		di_dtroot.header.DASD

		struct {
			union {
				uint8_t  _data[96];		/* 96: unused */
				struct {
					void      *_imap;	/* 4: unused */
					uint32_t  _gengen;	/* 4: generator */
				} _imap;
			} _u1;				        /* 96: */
#define di_gengen	u._file._u1._imap._gengen

			union {
				uint8_t  _xtroot[288];
				struct {
					uint8_t  unused[16];	/* 16: */
					dxd_t    _dxd;	        /* 16: */
					union {
						uint32_t  _rdev;	/* 4: */
						uint8_t   _fastsymlink[128];
					} _u;
					uint8_t  _inlineea[128];
				} _special;
			} _u2;
		} _file;
#define di_xtroot	    u._file._u2._xtroot
#define di_dxd		    u._file._u2._special._dxd
#define di_btroot	    di_xtroot
#define di_inlinedata	u._file._u2._special._u
#define di_rdev		    u._file._u2._special._u._rdev
#define di_fastsymlink	u._file._u2._special._u._fastsymlink
#define di_inlineea     u._file._u2._special._inlineea
	} u;
};

/* di_mode */
/*
 * The utilities that are dealing directly with the disk
 * i-node define the modes as follows. The filesystem itself
 * should use the standard S_IFMT, etc. defines in stat.h
 */
#define IFMT	0xF000		/* S_IFMT - mask of file type */
#define IFDIR	0x4000		/* S_IFDIR - directory */
#define IFREG	0x8000		/* S_IFREG - regular file */
#define IFLNK	0xA000		/* S_IFLNK - symbolic link */
#define IFBLK	0x6000		/* S_IFBLK - block special file */
#define IFCHR	0x2000		/* S_IFCHR - character special file */
#define IFFIFO	0x1000		/* S_IFFIFO - FIFO */
#define IFSOCK	0xC000		/* S_IFSOCK - socket */

#define ISUID	0x0800		/* S_ISUID - set user id when exec'ing */
#define ISGID	0x0400		/* S_ISGID - set group id when exec'ing */

#define IREAD	0x0100		/* S_IRUSR - read permission */
#define IWRITE	0x0080		/* S_IWUSR - write permission */
#define IEXEC	0x0040		/* S_IXUSR - execute permission */

/* extended mode bits (on-disk inode di_mode) */
#define IFJOURNAL   0x00010000	/* journalled file */
#define ISPARSE     0x00020000	/* sparse file enabled */
#define INLINEEA    0x00040000	/* inline EA area free */
#define ISWAPFILE	0x00800000	/* file open for pager swap space */

/* more extended mode bits */
#define IDIRECTORY	0x20000000	/* directory (shadow of real bit) */

#endif /*_H_JFS_DINODE */
