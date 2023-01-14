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
#ifndef H_PUFS_CHKDSK
#define H_PUFS_CHKDSK

enum xchkdsk_options {
	UFS_CHKDSK_LEVEL0,
	UFS_CHKDSK_LEVEL1,
	UFS_CHKDSK_LEVEL2,
	UFS_CHKDSK_LEVEL3,
	UFS_CHKDSK_IFDIRTY,
	UFS_CHKDSK_VERBOSE,
	UFS_CHKDSK_DEBUG,
	UFS_CHKDSK_CLRBDBLKLST,
	UFS_CHKDSK_SKIPLOGREDO,
	UFS_CHKDSK_BLOCK_NUMBER,
	UFS_CHKDSK_INODE_NUMBER,
	UFS_CHKDSK_FILENAME,
	UFS_CHKDSK_OPTIONS
};

#endif
