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
#ifndef	_H_JFS_FSCKCBBL
#define _H_JFS_FSCKCBBL

/*
 *  This structure resides in the first page (aka the block map control page)
 *  of the fsck in-aggregate workspace.  JFS Clear Bad Block List utility
 *  processing writes to this record, then fsck reads it and reports to the
 *  caller.
 */
struct fsckcbbl_record {
	char eyecatcher[8];
	char avail_1[4];	/*   4 */
	int32_t cbbl_retcode;	/* JFS Clear Bad Block List utility
				   * return code
				 */
	int32_t fs_blksize;	/* aggregate block size */
	int32_t lv_blksize;	/* device block size    */
	int32_t fs_lv_ratio;	/* fs_blksize/lv_blksize */
	int64_t fs_last_metablk;	/*
					 * last fs block we won't try to relocate
					 * because it holds fixed-location metadata
					 */
	int64_t fs_first_wspblk;	/*
					 * first fs block we won't try to relocate
					 * because it holds fsck workspace or the
					 * inline journal log
					 */
	int32_t total_bad_blocks;	/* count of bad blocks in LVM's list
					 * at beginning of Bad Block List utility
					 * processing
					 */
	int32_t resolved_blocks;	/* count of bad blocks:
					 * - for which the data has been relocated,
					 * - which are now allocated to the bad block
					 *   inode, and
					 * - which the LVM has been told to forget
					 */
	int32_t reloc_extents;	/* count of relocated extents */
	int64_t reloc_blocks;	/* count of blocks in relocated extents */
	int32_t LVM_lists;	/* count of bad block lists maintained by LVM
				   * according to the last query
				 */
	char bufptr_eyecatcher[8];
	void *clrbblks_agg_recptr;	/* addr of clrbblks aggregate record   */
	void *ImapInoPtr;	/* addr of imap inode buffer         */
	void *ImapCtlPtr;	/* addr of imap control page buffer  */
	void *ImapLeafPtr;	/* addr of imap leaf page buffer     */
	void *iagPtr;		/* addr of iag buffer                        */
	void *InoExtPtr;	/* addr of inode extent buffer               */
	char avail_2[28];	/*   28 */
};				/* total = 128 bytes */

#endif				/*  _H_JFS_FSCKCBBL */
