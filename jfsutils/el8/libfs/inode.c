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
#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "jfs_types.h"
#include "jfs_endian.h"
#include "jfs_filsys.h"
#include "jfs_dinode.h"
#include "devices.h"
#include "jfs_imap.h"
#include "inode.h"
#include "utilsubs.h"
#include "message.h"

/*
 * NAME: ujfs_rwinode
 *
 * FUNCTION: Read or write a specific aggregate or fileset inode.
 *
 * PRE CONDITIONS:
 *
 * POST CONDITIONS:
 *
 * PARAMETERS:
 *	fp	- open port for device to read/write
 *	di	- For read, filled in with inode read.  For write, contains
 *		  inode to write.
 *	inum	- number of inode to read/write
 *	mode	- are we reading or writing
 *	fs_block_size	- Block size for the aggregate
 *	which_table	- Aggregate Inode number describing Inode Allocation Map
 *			  which describes the specified inode.
 *
 * NOTES:
 *	Eventually when we have multiple filesets per aggregate we will need to
 *	determine the correct inode extent where the inode exists, and read the
 *	self inode to determine where that inode extent is on disk.  However,
 *	our first release we only support one fileset per aggregate, so we will
 *	never have more than NUM_INODE_PER_EXTENT aggregate inodes.  This first
 *	release of this function simply reads the inode from the necessary
 *	offset into the Aggregate Inode Table.
 *
 * RECOVERY OPERATION:
 *
 * DATA STRUCTURES:
 *
 * RETURNS: 0 for success
 *	Failure, any other value
 */
int ujfs_rwinode(FILE *fp,
		 struct dinode *di,
		 uint32_t inum,
		 int32_t mode,
		 int32_t fs_block_size,
		 uint32_t which_table,
		 uint32_t sb_flag)
{
	struct dinode map_inode;
	int rc;
	int64_t inode_extent_address, inode_address;
	int64_t map_inode_address;
	uint32_t iag_key;
	int32_t iag_inode_num, inode_ext_index;
	struct iag iag_for_inode;
	int64_t iag_address;
	int32_t l2nbperpage = log2shift(PSIZE / fs_block_size);

	/*
	 * Determine disk address for the inode to be read or written.
	 *
	 * If the inode we want is from the Aggregate Inode Table we can just
	 * determine the address for the inode directly since we know where this
	 * table lives.  If the inode we want is from the Fileset Inode Table we
	 * will need to read the Fileset Inode first and then follow its B+-tree to
	 * determine where the inode we want is.
	 */
	if (which_table == AGGREGATE_I) {
		/*
		 * Since the Aggregate Inode Table is just one inode extent for the
		 * first release we won't attempt to read an inode which is outside of
		 * this extent
		 */
		if (inum >= NUM_INODE_PER_EXTENT) {
			fprintf(stderr,
				"Internal error: %s(%d): Aggregate inode out of range (%d)\n",
				__FILE__, __LINE__, inum);
			return ERROR_INVALID_ACCESS;
		}

		inode_address = (inum * sizeof (struct dinode)) + AGGR_INODE_TABLE_START;
	} else if (which_table == FILESYSTEM_I) {
		/*
		 * Find the IAG which describes this inode.
		 */
		iag_key = INOTOIAG(inum);

		/*
		 * Read Fileset inode describing the Fileset Inode Allocation Map so we
		 * have the B+-tree information
		 */
		map_inode_address = AGGR_INODE_TABLE_START + (which_table * sizeof (struct dinode));
		rc = ujfs_rw_diskblocks(fp, map_inode_address, sizeof (struct dinode), &map_inode, GET);

		/* swap if on big endian machine */
		ujfs_swap_dinode(&map_inode, GET, sb_flag);

		if (rc != 0)
			return (rc);

		/*
		 * Get address for IAG describing this inode
		 */
		rc = ujfs_rwdaddr(fp, &iag_address, &map_inode,
				  IAGTOLBLK(iag_key, l2nbperpage), GET, fs_block_size);
		if (rc != 0)
			return (rc);

		/*
		 * Read iag which describes the specified inode.
		 */
		rc = ujfs_rw_diskblocks(fp, iag_address, sizeof (struct iag), &iag_for_inode, GET);

		/* swap if on big endian machine */
		ujfs_swap_iag(&iag_for_inode);

		if (rc != 0)
			return (rc);

		/*
		 * Determine which inode within the found IAG is being referenced
		 */
		iag_inode_num = inum % NUM_INODE_PER_IAG;

		/*
		 * Find the inode extent descriptor within the found IAG which describes
		 * the inode extent containing the specified inode.
		 */
		inode_ext_index = iag_inode_num / NUM_INODE_PER_EXTENT;

		/*
		 * From the correct inode extent descriptor in the IAG we can determine
		 * the disk address for the specified inode.
		 */
		inode_extent_address = addressPXD(&(iag_for_inode.inoext[inode_ext_index]));
		inode_extent_address *= fs_block_size;
		inode_address = (inum % NUM_INODE_PER_EXTENT * sizeof (struct dinode)) + inode_extent_address;
	} else {
		fprintf(stderr, "Internal error: %s(%d): Bad map inode number (%d)\n",
			__FILE__, __LINE__, which_table);
		return ERROR_INVALID_HANDLE;
	}

	/*
	 * Now read/write the actual inode
	 */

	/* swap if on big endian machine */
	if (mode == PUT)
		ujfs_swap_dinode(di, PUT, sb_flag);

	rc = ujfs_rw_diskblocks(fp, inode_address, sizeof (struct dinode), di, mode);

	/* swap if on big endian machine */
	ujfs_swap_dinode(di, GET, sb_flag);

	return rc;
}

/*
 * NAME: ujfs_rwdaddr
 *
 * FUNCTION: read/write offset from/to an inode
 *
 * PRE CONDITIONS:
 *
 * POST CONDITIONS:
 *
 * PARAMETERS:
 *	fp	- device file descriptor
 *	offset	- where we put the offset that corresponds to <lbno>
 *	di	- disk inode to get offset from
 *	lbno	- logical block number
 *	mode	- GET or PUT (read/write block from/to inode)
 *	fs_block_size	- block size for aggregate
 *
 * NOTES:
 *
 * RECOVERY OPERATION:
 *
 * DATA STRUCTURES:
 *
 * RETURNS: 0 for success
 *	Failure, any other value
 */
int ujfs_rwdaddr(FILE *fp,
		 int64_t * offset,
		 struct dinode * di,
		 int64_t lbno,
		 int32_t mode,
		 int32_t fs_block_size)
{
	xad_t *disk_extent;
	int64_t disk_extent_offset;
	xtpage_t *page;
	int32_t lim, base, index;
	int rc;
	int32_t cmp;
	char buffer[PSIZE];
	int64_t offset64;

	page = (xtpage_t *) & (di->di_btroot);

      descend:
	/* Binary search */
	for (base = XTENTRYSTART, lim = page->header.nextindex - XTENTRYSTART; lim; lim >>= 1) {
		index = base + (lim >> 1);
		offset64 = offsetXAD(&(page->xad[index]));
		cmp = (lbno >= offset64 + lengthXAD(&(page->xad[index]))) ? 1 : (lbno < offset64) ? -1 : 0;
		if (cmp == 0) {
			/* HIT! */
			if (page->header.flag & BT_LEAF) {
				*offset = (addressXAD(&(page->xad[index])) +
					   (lbno - offsetXAD(&(page->xad[index])))) * fs_block_size;
				return 0;
			} else {
				rc = ujfs_rw_diskblocks(fp, addressXAD(&(page->xad[index])) *
							fs_block_size, PSIZE, buffer, GET);

				/* swap if on big endian machine */
				ujfs_swap_xtpage_t((xtpage_t *) buffer);

				if (rc) {
					fprintf(stderr,
						"Internal error: %s(%d): Error reading btree node\n",
						__FILE__, __LINE__);
					return rc;
				}
				page = (xtpage_t *) buffer;
				goto descend;
			}
		} else if (cmp > 0) {
			base = index + 1;
			--lim;
		}
	}

	if (page->header.flag & BT_INTERNAL) {
		/* Traverse internal page, it might hit down there
		 * If base is non-zero, decrement base by one to get the parent
		 * entry of the child page to search.
		 */
		index = base ? base - 1 : base;

		rc = ujfs_rw_diskblocks(fp, addressXAD(&(page->xad[index])) * fs_block_size,
				        PSIZE, buffer, GET);

		/* swap if on big endian machine */
		ujfs_swap_xtpage_t((xtpage_t *) buffer);

		if (rc) {
			fprintf(stderr,
				"Internal error: %s(%d): Error reading btree node\n", __FILE__, __LINE__);
			return rc;
		}
		page = (xtpage_t *) buffer;
		goto descend;
	}

	/* Not found! */
	fprintf(stderr, "Internal error: %s(%d): Block %lld not found!\n", __FILE__,
		__LINE__, (long long) lbno);
	return EINVAL;

	/*
	 * This is really stupid right now, doesn't understand multiple extents
	 */
	switch (mode) {
	case GET:
		disk_extent = &(((xtpage_t *) & (di->di_DASD))->xad[XTENTRYSTART]);
		disk_extent_offset = addressXAD(disk_extent);
		*offset = (disk_extent_offset + lbno) * fs_block_size;
		break;
	case PUT:
		fprintf(stderr, "Internal error: %s(%d): does not handle PUT\n",
			__FILE__, __LINE__);
		return EPERM;
		break;
	default:
		return EINVAL;
	}
	return 0;
}
