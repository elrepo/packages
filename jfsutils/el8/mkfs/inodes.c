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
#include <stddef.h>
#include <time.h>
#include <errno.h>
#include "jfs_types.h"
#include "jfs_endian.h"
#include "jfs_filsys.h"
#include "jfs_dinode.h"
#include "initmap.h"
#include "inode.h"
#include "devices.h"
#include "inodes.h"
#include "debug.h"
#include "message.h"

/* endian routines */
extern unsigned type_jfs;

/*
 * NAME: init_aggr_inode_table
 *
 * FUNCTION: Initialize aggregate inodes to disk
 *
 * PARAMETERS:
 *      aggr_block_size - block size for aggregate
 *      dev_ptr         - open port for device to write to
 *      aggr_inodes     - Array of initial aggregate inodes.  They have been
 *                        initialized elsewhere.
 *      num_aggr_inodes - Number of aggregate inodes initialized.
 *      table_loc       - Byte offset of table location
 *      inode_map_loc   - Block offset of inode map
 *      inode_map_sz    - Byte count of inode map
 *      inostamp        - Inode stamp to be used for aggregate inodes
 *
 * RETURNS:
 *      success: 0
 *      failure: any other value
 */
int init_aggr_inode_table(int aggr_block_size,
			  FILE *dev_ptr,
			  struct dinode *aggr_inodes,
			  int num_aggr_inodes,
			  int64_t table_loc,
			  int64_t inode_map_loc,
			  int inode_map_sz, unsigned inostamp)
{
	void *buffer;
	int64_t first_block, last_block, index;
	int i, rc;
	struct dinode *buf_ai;

	/*
	 * Allocate space for first inode extent, and clear
	 */
	buffer = calloc(INODE_EXTENT_SIZE, sizeof (char));
	if (buffer == NULL) {
		message_user(MSG_OSO_INSUFF_MEMORY, NULL, 0, OSO_MSG);
		return (ENOMEM);
	}

	/*
	 * Initialize inodes: 0, 1, 3, and 4
	 * Inode 2 has already been initialized;
	 */
	DBG_TRACE(("size of dinode = %d\n", sizeof (struct dinode)))
	    aggr_inodes[AGGR_RESERVED_I].di_nlink = 1;

	/*
	 * Initialize inode 1: "self" inode
	 */
	init_inode(&(aggr_inodes[AGGREGATE_I]), AGGREGATE_I, AGGREGATE_I,
		   inode_map_sz / aggr_block_size, inode_map_sz, inode_map_loc,
		   IFJOURNAL | IFREG, max_extent_data,
		   table_loc / aggr_block_size, aggr_block_size, inostamp);
	aggr_inodes[AGGREGATE_I].di_gengen = 1;

	/*
	 * Initialize inode 3: inline log inode
	 */
	init_inode(&(aggr_inodes[LOG_I]), AGGREGATE_I, LOG_I, 0, 0, 0,
		   IFJOURNAL | IFREG, no_data, table_loc / aggr_block_size,
		   aggr_block_size, inostamp);

	/*
	 * Initialize inode 4: bad block inode
	 */
	init_inode(&(aggr_inodes[BADBLOCK_I]), AGGREGATE_I, BADBLOCK_I, 0, 0, 0,
		   IFJOURNAL | IFREG | ISPARSE, no_data,
		   table_loc / aggr_block_size, aggr_block_size, inostamp);

	/*
	 * Copy initialized inodes to buffer
	 */
	memcpy(buffer, aggr_inodes, num_aggr_inodes * sizeof (struct dinode));

	/*
	 * Write Inode extent to disk
	 */

	/* swap if on big endian machine */
	/* swap gengen from aggr_inodes[AGGREGATE_I] in buffer */
	buf_ai = ((struct dinode *) buffer) + AGGREGATE_I;
	buf_ai->di_gengen = __le32_to_cpu(buf_ai->di_gengen);
	for (i = 0; i < num_aggr_inodes; i++) {
		ujfs_swap_dinode((struct dinode *) buffer, PUT, type_jfs);
		buffer = (char *) buffer + sizeof (struct dinode);
	}
	buffer = (char *) buffer - (num_aggr_inodes * sizeof (struct dinode));

	rc = ujfs_rw_diskblocks(dev_ptr, table_loc, INODE_EXTENT_SIZE, buffer,
				PUT);
	free(buffer);
	if (rc != 0)
		return rc;

	/*
	 * Mark blocks allocated in block allocation map
	 */
	first_block = table_loc / aggr_block_size;
	last_block = first_block + (INODE_EXTENT_SIZE / aggr_block_size);
	for (index = first_block; ((index < last_block) && (rc == 0)); index++) {
		rc = markit(index, ALLOC);
	}

	return (rc);
}

/*
 * NAME: init_fileset_inode_table
 *
 * FUNCTION: Initialize fileset inodes and write to disk
 *
 * PARAMETERS:
 *      aggr_block_size - block size for aggregate
 *      dev_ptr         - open port for device to write to
 *      inode_location  - Filled in with byte offset of first extent of fileset
 *                        inode table
 *      inode_size      - Filled in with byte count of first extent of fileset
 *                        inode table
 *      fileset_start   - First block for fileset, will use this to determine
 *                        where to put the inodes on disk
 *      fileset_inode_map_loc - First block of fileset inode map
 *      inostamp        - stamp for inode
 *
 * RETURNS:
 *      success: 0
 *      failure: any other value
 */
int init_fileset_inode_table(int aggr_block_size,
			     FILE *dev_ptr,
			     int64_t * inode_location,
			     int *inode_size,
			     int64_t fileset_start,
			     int64_t fileset_inode_map_loc, unsigned inostamp)
{
	void *buffer, *bp;
	struct dinode inode_buffer;
	int64_t first_block, last_block, index;
	int i, rc;
	int root_size;
	dtroot_t *root_header;

	/*
	 * Find space for the inode extent
	 *
	 * Release 1 will not support multiple filesets per aggregate, so the
	 * location of the inode extent can be fixed.  However in future releases
	 * this will have to be modified to find the space for this extent using
	 * the block allocation map.
	 */
#ifdef ONE_FILESET_PER_AGGR
	/*
	 * The first fileset inode extent is the first thing written for the
	 * fileset, so its location is the start of the fileset
	 */
	*inode_location = fileset_start * aggr_block_size;
#else
	*inode_location = get_space(*inode_size);
#endif				/* ONE_FILESET_PER_AGGR */

	/*
	 * Allocate space for first inode extent, and clear
	 */
	*inode_size = INODE_EXTENT_SIZE;
	bp = buffer = calloc(*inode_size, sizeof (char));
	if (bp == NULL) {
		message_user(MSG_OSO_INSUFF_MEMORY, NULL, 0, OSO_MSG);
		return (ENOMEM);
	}
	/*
	 * Inode 0 - Reserved Inode
	 */
	memset(&inode_buffer, 0, sizeof (inode_buffer));

	init_inode(&inode_buffer, FILESYSTEM_I, FILESET_RSVD_I, 0, 0, 0,
		   IFJOURNAL | IFREG, no_data, fileset_start, aggr_block_size,
		   inostamp);

	memcpy(bp, &inode_buffer, sizeof (inode_buffer));
	bp = (char *) bp + sizeof (inode_buffer);

	/*
	 * Inode 1 - 2nd half of fileset superblock information
	 *
	 * When we add support for VFS this will be a special inode with
	 * different information.  For now we will just make it look like an
	 * empty inode to reserve it.
	 */
	memset(&inode_buffer, 0, sizeof (inode_buffer));

	init_inode(&inode_buffer, FILESYSTEM_I, FILESET_EXT_I, 0, 0, 0,
		   IFJOURNAL | IFREG, no_data, fileset_start, aggr_block_size,
		   inostamp);

	memcpy(bp, &inode_buffer, sizeof (inode_buffer));
	bp = (char *) bp + sizeof (inode_buffer);

	/*
	 * Inode 2 - Root directory
	 */
	memset(&inode_buffer, 0, sizeof (inode_buffer));
	root_size =
	    sizeof (struct dinode) - offsetof(struct dinode, di_inlinedata);
	init_inode(&inode_buffer, FILESYSTEM_I, ROOT_I, 0, root_size, 0,
		   IFJOURNAL | IFDIR | 0755, no_data, fileset_start,
		   aggr_block_size, inostamp);

	/* Set number of links for root to account for '.' entry */
	inode_buffer.di_nlink = 2;

	/*
	 * Initialize the directory B+-tree header for the root inode
	 * Since the root directory has no entries the nextindex is 0; nextindex
	 * is for the stbl index not the slot index.
	 */
	root_header = (dtroot_t *) & (inode_buffer.di_DASD);
	root_header->header.flag = DXD_INDEX | BT_ROOT | BT_LEAF;
	setDASDLIMIT(&(root_header->header.DASD), 0);
	setDASDUSED(&(root_header->header.DASD), 0);
	root_header->header.nextindex = 0;
	/*
	 * Determine how many slots will fit
	 */
	root_header->header.freecnt = DTROOTMAXSLOT - 1;
	root_header->header.freelist = DTENTRYSTART;
	root_header->header.idotdot = 2;

	for (index = DTENTRYSTART; index < DTROOTMAXSLOT; index++) {
		root_header->slot[index].cnt = 1;
		root_header->slot[index].next = index + 1;
	}
	/*
	 * Last entry should end the free list.
	 */
	index--;
	root_header->slot[index].next = -1;

	memcpy(bp, &inode_buffer, sizeof (inode_buffer));
	bp = (char *) bp + sizeof (inode_buffer);

	/*
	 * Inode 3 - ACL File
	 */
	memset(&inode_buffer, 0, sizeof (inode_buffer));

	init_inode(&inode_buffer, FILESYSTEM_I, ACL_I, 0, 0, 0,
		   IFJOURNAL | IFREG, no_data, fileset_start, aggr_block_size,
		   inostamp);

	memcpy(bp, &inode_buffer, sizeof (inode_buffer));

	/*
	 * Write Inode extent to disk
	 */

	/* swap if on big endian machine */
	bp = buffer;
	for (i = 0; i < 4; i++) {
		ujfs_swap_dinode((struct dinode *) bp, PUT, type_jfs);
		bp = (char *) bp + sizeof (inode_buffer);
	}

	rc = ujfs_rw_diskblocks(dev_ptr, *inode_location, *inode_size, buffer,
				PUT);
	free(buffer);
	if (rc != 0)
		return rc;

	/*
	 * Mark blocks allocated in block allocation map
	 */
	first_block = *inode_location / aggr_block_size;
	last_block = (*inode_location + *inode_size) / aggr_block_size;
	for (index = first_block; ((index < last_block) && (rc == 0)); index++) {
		rc = markit(index, ALLOC);
	}

	return (rc);
}

/*
 * NAME: init_fileset_inodes
 *
 * FUNCTION: Initialize fileset inodes in aggregate inode table
 *
 * PARAMETERS:
 *      aggr_block_size - block size for aggregate
 *      dev_ptr         - open port for device to write to
 *      inode_map_loc   - byte offset for first extent of fileset's inode map
 *      inode_map_size  - byte count for first extent of fileset's inode map
 *      fileset_start   - First block of fileset. Will be used to determine
 *                        location of the AG table.
 *      inostamp        - time stamp for inodes in this fileset
 *
 * RETURNS:
 *      success: 0
 *      failure: any other value
 */
int init_fileset_inodes(int aggr_block_size,
			FILE *dev_ptr,
			int64_t inode_map_loc,
			int inode_map_size,
			int64_t fileset_start,
			unsigned inostamp)
{
	struct dinode inode_buffer;
	unsigned inode_num;
	int rc;

	/*
	 * Figure out which is the next free aggregate fileset inode
	 *
	 * Release 1 only supports one fileset per aggregate, so we know this
	 * will always be inode FILESYSTEM_I in the aggregate inode table.  In
	 * future releases we will need to modify this code to look in the
	 * aggregate inode table for the next available free inode.
	 */

#ifdef ONE_FILESET_PER_AGGR
	inode_num = FILESYSTEM_I;
#else
	inode_num = get_next_free();
#endif				/* ONE_FILESET_PER_AGGR */

	/*
	 * Initialize Fileset Inode: Fileset Inode Allocation Map
	 */
	memset(&inode_buffer, 0, sizeof (inode_buffer));

	init_inode(&inode_buffer, AGGREGATE_I, inode_num,
		   inode_map_size / aggr_block_size, inode_map_size,
		   inode_map_loc, IFJOURNAL | IFREG, max_extent_data,
		   AITBL_OFF / aggr_block_size, aggr_block_size, inostamp);

	/* swap here if necessary for big endian */
	inode_buffer.di_gengen = __le32_to_cpu(1);

	/*
	 * Write fileset inode to disk
	 */
	rc = ujfs_rwinode(dev_ptr, &inode_buffer, inode_num, PUT,
			  aggr_block_size, AGGREGATE_I, type_jfs);

	return (rc);
}

/*
 * NAME: init_inode
 *
 * FUNCTION: Initialize inode fields for an inode with a single extent or inline
 *      data or a directory inode
 *
 * PARAMETERS:
 *      new_inode       - Pointer to inode to be initialized
 *      fileset_num     - Fileset number for inode
 *      inode_num       - Inode number of inode
 *      num_blocks      - Number of aggregate blocks allocated to inode
 *      size            - Size in bytes allocated to inode
 *      first_block     - Offset of first block of inode's extent
 *      mode            - Mode for inode
 *      inode_type      - Indicates the type of inode to be initialized.
 *                        Currently supported types are inline data, extents,
 *                        and no data.  The other parameters to this function
 *                        will provide the necessary information.
 *      inoext_address  - Address of inode extent containing this inode
 *      aggr_block_size - Aggregate block size
 *      inostamp        - Stamp used to identify inode as belonging to fileset
 *
 * RETURNS: None
 */
void init_inode(struct dinode *new_inode,
		int fileset_num,
		unsigned inode_num,
		int64_t num_blocks,
		int64_t size,
		int64_t first_block,
		mode_t mode,
		ino_data_type inode_type,
		int64_t inoext_address,
		int aggr_block_size,
		unsigned inostamp)
{
	/*
	 * Initialize inode with where this stuff lives
	 */
	new_inode->di_inostamp = inostamp;
	new_inode->di_fileset = fileset_num;
	new_inode->di_number = inode_num;
	new_inode->di_gen = 1;
	PXDaddress(&(new_inode->di_ixpxd), inoext_address);
	PXDlength(&(new_inode->di_ixpxd), INODE_EXTENT_SIZE / aggr_block_size);
	new_inode->di_mode = mode;
	new_inode->di_nblocks = num_blocks;
	new_inode->di_size = size;
	new_inode->di_nlink = 1;
	new_inode->di_next_index = 2;

	switch (inode_type) {
	case inline_data:
		new_inode->di_dxd.flag = DXD_INLINE;
		DXDlength(&(new_inode->di_dxd),
			  sizeof (struct dinode) - offsetof(struct dinode,
							    di_inlinedata));
		DXDaddress(&(new_inode->di_dxd), 0);
		break;
	case extent_data:
	case max_extent_data:
		((xtpage_t *) & (new_inode->di_DASD))->header.flag =
		    DXD_INDEX | BT_ROOT | BT_LEAF;
		/*
		 * Since this is the root, we don't actually use the next and
		 * prev entries.  Set to 0 in case we decide to use this space
		 * for something in the future.
		 */
		((xtpage_t *) & (new_inode->di_DASD))->header.next = 0;
		((xtpage_t *) & (new_inode->di_DASD))->header.prev = 0;
		((xtpage_t *) & (new_inode->di_DASD))->header.nextindex =
		    XTENTRYSTART + 1;
		((xtpage_t *) & (new_inode->di_DASD))->header.maxentry =
		    XTROOTMAXSLOT;
		((xtpage_t *) & (new_inode->di_DASD))->xad[XTENTRYSTART].flag =
		    0;
		((xtpage_t *) & (new_inode->di_DASD))->xad[XTENTRYSTART].rsvrd =
		    0;
		XADoffset(&((xtpage_t *) & (new_inode->di_DASD))->
			  xad[XTENTRYSTART], 0);
		XADlength(&((xtpage_t *) & (new_inode->di_DASD))->
			  xad[XTENTRYSTART], num_blocks);
		XADaddress(&((xtpage_t *) & (new_inode->di_DASD))->
			   xad[XTENTRYSTART], first_block);
		break;
	case no_data:
		/*
		 * No data to be filled in here, don't do anything
		 */
		((xtpage_t *) & (new_inode->di_DASD))->header.flag =
		    DXD_INDEX | BT_ROOT | BT_LEAF;
		/*
		 * Since this is the root, we don't actually use the next and
		 * prev entries.  Set to 0 in case we decide to use this space
		 * for something in the future.
		 */
		((xtpage_t *) & (new_inode->di_DASD))->header.next = 0;
		((xtpage_t *) & (new_inode->di_DASD))->header.prev = 0;
		((xtpage_t *) & (new_inode->di_DASD))->header.nextindex =
		    XTENTRYSTART;
		((xtpage_t *) & (new_inode->di_DASD))->header.maxentry =
		    XTROOTMAXSLOT;
		((xtpage_t *) & (new_inode->di_DASD))->xad[XTENTRYSTART].flag =
		    0;
		((xtpage_t *) & (new_inode->di_DASD))->xad[XTENTRYSTART].rsvrd =
		    0;
		break;
	default:
		DBG_ERROR(("Internal error: %s(%d): Unrecognized inode data type %d\n",
			  __FILE__, __LINE__, inode_type))
		    break;
	}

	new_inode->di_atime.tv_sec = new_inode->di_ctime.tv_sec =
	    new_inode->di_mtime.tv_sec = new_inode->di_otime.tv_sec =
	    (unsigned) time(NULL);
	return;
}
