/*
 *   Copyright (C) International Business Machines Corp., 2000-2005
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
#define _GNU_SOURCE	/* FOR O_DIRECT */
#include <config.h>
#include "jfs_types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "jfs_endian.h"
#include "jfs_filsys.h"
#include "jfs_dinode.h"
#include "devices.h"
#include "inodes.h"
#include "jfs_dmap.h"
#include "diskmap.h"
#include "inode.h"
#include "initmap.h"
#include "message.h"
#include "utilsubs.h"

extern unsigned type_jfs;

#define UZWORD  (0x80000000u)
#define DBBYTE          8	/* number of bits per byte */
#define L2DBBYTE        3	/* log2 of number of bits per byte */
#define CHAR_ONES       (0xffu)

static struct dmap **block_map_array;
static unsigned sz_block_map_array;
static unsigned cur_dmap_index;
static struct dbmap *control_page;
static int64_t last_allocated;
static struct dmap *empty_page;

struct xtree_buf {
	struct xtree_buf *down;	/* next rightmost child */
	struct xtree_buf *up;	/* parent */
	xtpage_t *page;
};

static struct xtree_buf *badblock_pages;

static int xtAppend(FILE *, struct dinode *, int64_t, int64_t, int,
		    struct xtree_buf *, int);

/*--------------------------------------------------------------------
 * NAME:        initdmap()
 *
 * FUNCTION:    Initialize a dmap for the specified block range
 *              (blkno thru blkno+nblocks-1).
 *
 * PARAMETERS:
 *      dev_ptr - device to write map page to
 *      blkno   - Starting disk block number to be covered by this dmap.
 *      nblocks - Number of blocks covered by this dmap.
 *      treemax - Return value set as maximum free string found in this dmap
 *      start   - Logical block address of where this dmap should live on disk.
 *
 * NOTES: The wmap and pmap words along the leaves of the dmap tree are
 *      initialized, with the leaves initialized to the maximum free string of
 *      the wmap word they describe.  With this complete ujfs_adjtree() is
 *      called to combine all appropriate buddies and update the higher level of
 *      the tree to reflect the result of the buddy combination.  The maximum
 *      free string of the dmap (i.e. the root value of the tree) is returned
 *      in treemax.
 *
 * RETURNS: NONE
 */
static int initdmap(FILE *dev_ptr, int64_t blkno, int64_t nblocks,
		    int8_t * treemax, int64_t start)
{
	int rc = 0;

	/*
	 * Determine if the dmap already exists
	 */
	if (block_map_array[cur_dmap_index] == NULL) {
		if (nblocks == BPERDMAP) {
			/*
			 * alloc/init a template empty full page buffer
			 */
			if (empty_page == NULL) {
				empty_page = malloc(sizeof (struct dmap));
				if (empty_page == NULL) {
					message_user(MSG_OSO_INSUFF_MEMORY,
						     NULL, 0, OSO_MSG);
					return (ENOMEM);
				}
				memset(empty_page, 0, sizeof (struct dmap));
				ujfs_idmap_page(empty_page, nblocks);
				ujfs_complete_dmap(empty_page, blkno, treemax);
			} else {
				/*
				 * customize/reuse the template empty page
				 */
				empty_page->start = blkno;
				*treemax = empty_page->tree.stree[0];
			}
			block_map_array[cur_dmap_index] = empty_page;
		} else {
			/*
			 * alloc/init a special dmap page with the correct size
			 */
			block_map_array[cur_dmap_index] =
			    malloc(sizeof (struct dmap));
			if (block_map_array[cur_dmap_index] == NULL) {
				message_user(MSG_OSO_INSUFF_MEMORY, NULL, 0,
					     OSO_MSG);
				return (ENOMEM);
			}
			memset(block_map_array[cur_dmap_index], 0,
			       sizeof (struct dmap));
			ujfs_idmap_page(block_map_array[cur_dmap_index],
					nblocks);
			ujfs_complete_dmap(block_map_array[cur_dmap_index],
					   blkno, treemax);
		}
	} else {
		/*
		 * Fill in rest of fields of special existing dmap page
		 */
		ujfs_complete_dmap(block_map_array[cur_dmap_index], blkno,
				   treemax);
	}

	/*
	 * Write the dmap page and free if special buffer
	 */

	/* swap if on big endian machine */
	ujfs_swap_dmap(block_map_array[cur_dmap_index]);
	rc = ujfs_rw_diskblocks(dev_ptr, start, PSIZE,
				block_map_array[cur_dmap_index], PUT);
	ujfs_swap_dmap(block_map_array[cur_dmap_index]);

	if (rc != 0)
		return rc;

	if (block_map_array[cur_dmap_index] != empty_page) {
		free(block_map_array[cur_dmap_index]);
	}

	cur_dmap_index++;

	return (rc);
}

/*--------------------------------------------------------------------
 * NAME:        initctl()
 *
 * FUNCTION:    Initialize a dmapctl for the specified block range
 *              (blkno thru blkno+nblocks-1) and
 *              level and initialize all dmapctls and dmaps under this dmapctl.
 *
 * PARAMETERS:
 *      dev_ptr - device to write page to
 *      blkno   - Starting disk block number to be covered by this dmapctl.
 *      nblocks - Number of blocks covered by this dmapctl.
 *      level   - The level of this dmapctl.
 *      treemax - Return value set as the maximum free string found in this
 *                dmapctl.
 *      start   - Logical block address of where this page should live on disk.
 *
 * NOTES: This routine is called recursively. On first invocation it is called
 *      for the top level dmapctl of the tree.  For each leaf of the dmapctl,
 *      the lower level dmap (level == 0) or dmapctl (level > 0) is created for
 *      the block range covered by the leaf and the leaf is set to the maximum
 *      free string found in the lower level object.  If the lower level object
 *      is a dmap, initdmap() is called to handle it's initialization.
 *      Otherwise, initctl() is called recursively to initialize the lower level
 *      dmapctl with the level specified as the current level - 1; once all
 *      leaves have been initialized ujfs_adjtree() is called to combine all
 *      appropriate buddies and update the higher level of the tree to reflect
 *      the result of the buddy combination.  The maximum free string of the
 *      dmapctl (i.e. the root value of the tree) is returned in treemax.
 *
 * RETURNS: None.
 */
static int initctl(FILE *dev_ptr,
		   int64_t blkno,
		   int64_t nblocks,
		   int level,
		   int8_t * treemax,
		   int64_t * start)
{
	int index, rc = 0, l2cblks, nchild;
	int8_t *cp, max;
	struct dmapctl *dcp;
	int64_t nb, cblks;
	int64_t next_page;

	/*
	 * alloc/init current level dmapctl page buffer
	 */
	dcp = malloc(sizeof (struct dmapctl));
	if (dcp == NULL) {
		message_user(MSG_OSO_INSUFF_MEMORY, NULL, 0, OSO_MSG);
		return (ENOMEM);
	}
	memset(dcp, 0, sizeof (struct dmapctl));

	dcp->height = 5;
	dcp->leafidx = CTLLEAFIND;
	dcp->nleafs = LPERCTL;
	dcp->l2nleafs = L2LPERCTL;
	dcp->budmin = L2BPERDMAP + level * L2LPERCTL;

	/* Pick up the pointer to the first leaf of the dmapctl tree */
	cp = dcp->stree + dcp->leafidx;

	/*
	 * Determine how many lower level dmapctls or dmaps will be described by
	 * this dmapctl based upon the number of blocks covered by this dmapctl.
	 */
	l2cblks = L2BPERDMAP + level * L2LPERCTL;
	cblks = (1LL << l2cblks);
	nchild = nblocks >> l2cblks;
	nchild = (nblocks & (cblks - 1)) ? nchild + 1 : nchild;
	next_page = *start + PSIZE;
	for (index = 0; index < nchild; index++, nblocks -= nb, blkno += nb) {
		/*
		 * Determine how many blocks the lower level dmapctl or dmap will cover.
		 */
		nb = MIN(cblks, nblocks);

		/*
		 * If this is a level 0 dmapctl, initialize the dmap for the
		 * block range (i.e. blkno thru blkno+nb-1).  Otherwise,
		 * initialize the lower level dmapctl for this block range.
		 * In either case, the pointer to the leaf covering this block
		 * range is passed down and will be set to the length of the
		 * maximum free string of blocks found at the lower level.
		 */
		if (level == 0) {
			rc += initdmap(dev_ptr, blkno, nb, cp + index, next_page);
			next_page += PSIZE;
		} else {
			rc += initctl(dev_ptr, blkno, nb, level - 1, cp + index,
				    &next_page);
		}
	}

	/*
	 * Initialize the leaves for this dmapctl that were not covered by the
	 * specified input block range (i.e. the leaves have no low level
	 * dmapctl or dmap.
	 */
	for (; index < LPERCTL; index++) {
		*(cp + index) = NOFREE;
	}

	/*
	 * With the leaves initialized, adjust the tree for this dmapctl.
	 */
	max = ujfs_adjtree(dcp->stree, L2LPERCTL, l2cblks);

	/*
	 * Write and release the dmapctl page
	 */

	/* swap if on big endian machine */
	ujfs_swap_dmapctl(dcp);

	rc += ujfs_rw_diskblocks(dev_ptr, *start, PSIZE, dcp, PUT);

	free(dcp);

	/*
	 * Set the treemax return value with the maximum free described by
	 * this dmapctl.
	 */
	*treemax = max;
	*start = next_page;

	return (rc);
}

/*--------------------------------------------------------------------
 * NAME:        initbmap()
 *
 * FUNCTION:    Initialize the disk block allocation map for an aggregate.
 *
 * PARAMETERS:
 *      dev_ptr - device to write page to
 *      nblocks - Number of blocks within the aggregate.
 *
 * NOTES: The bmap control page is created.  Next, the number dmapctl level
 *      required to described the aggregate size (number of blocks within the
 *      aggregate) is determined. initctl() is then called to initialize the
 *      appropriate dmapctl levels and corresponding dmaps.
 *
 * RETURNS:
 */
static int initbmap(FILE *dev_ptr, int64_t nblocks)
{
	int level, rc = 0;
	int64_t next_page;

	/*
	 * get the level for the actual top dmapctl for the aggregate and
	 * its physical address (N.B. map file has been allocated
	 * to cover full control level hierarchy);
	 */
	level = BMAPSZTOLEV(nblocks);
	next_page = BMAP_OFF + PSIZE + PSIZE * (2 - level);

	/*
	 * initialize only the dmapctls and the dmaps they describe
	 * that covers the actual aggregate size.
	 */
	rc = initctl(dev_ptr, 0, nblocks, level, &control_page->dn_maxfreebud,
		     &next_page);
	if (rc != 0)
		return (rc);

	/*
	 * Write the control page to disk.
	 */

	/* swap if on big endian machine */
	ujfs_swap_dbmap(control_page);
	rc = ujfs_rw_diskblocks(dev_ptr, BMAP_OFF, PSIZE, control_page, PUT);
	ujfs_swap_dbmap(control_page);

	return (rc);
}

/*--------------------------------------------------------------------
 * NAME:        alloc_map()
 *
 * FUNCTION:    Allocate and initialize to zero the memory for dmap pages
 *              and the control page of block map.
 *
 * PARAMETERS:
 *      num_dmaps       - Indicates number of dmaps to allocate
 *
 * DATA STRUCTURES: Initializes file static variable block_map
 *
 * RETURNS:     0 for success
 */
static int alloc_map(int num_dmaps)
{
	if (num_dmaps <= 0)
		return EINVAL;

	/* alloc/init dmap page pointer array */
	block_map_array = malloc(num_dmaps * sizeof (struct dmap *));
	if (block_map_array == NULL) {
		message_user(MSG_OSO_INSUFF_MEMORY, NULL, 0, OSO_MSG);
		return ENOMEM;
	}
	sz_block_map_array = num_dmaps;

	memset(block_map_array, 0, num_dmaps * sizeof (struct dmap *));

	/* alloc/init control page */
	control_page = malloc(sizeof (struct dbmap));
	if (control_page == NULL) {
		message_user(MSG_OSO_INSUFF_MEMORY, NULL, 0, OSO_MSG);
		return ENOMEM;
	}

	memset(control_page, 0, sizeof (struct dbmap));

	return 0;
}

/*--------------------------------------------------------------------
 * NAME:        initmap()
 *
 * FUNCTION:    Initialize control page
 *
 * PARAMETERS:
 *      nblocks - Number of blocks covered by this map
 *      ag_size - Will be filled in with AG size in blocks
 *      aggr_block_size         - Aggregate block size
 *
 * RETURNS: NONE
 */
static void initmap(int64_t nblocks, int *ag_size, int aggr_block_size)
{
	int index, l2nl, n;
	int64_t nb;

	/*
	 * Initialize base information
	 */
	control_page->dn_l2nbperpage = log2shift(PSIZE / aggr_block_size);
	control_page->dn_mapsize = control_page->dn_nfree = nblocks;
	control_page->dn_maxlevel = BMAPSZTOLEV(nblocks);
	/* control_page->dn_maxfreebud is computed at finalization */

	/*
	 * Initialize allocation group information.
	 */
	control_page->dn_agl2size = ujfs_getagl2size(nblocks, aggr_block_size);
	*ag_size = control_page->dn_agsize =
	    (int64_t) 1 << control_page->dn_agl2size;
	control_page->dn_numag = nblocks / control_page->dn_agsize;
	control_page->dn_numag += (nblocks % control_page->dn_agsize) ? 1 : 0;

	for (index = 0, nb = nblocks; index < control_page->dn_numag;
	     index++, nb -= *ag_size) {
		control_page->dn_agfree[index] = MIN(nb, *ag_size);
	}

	control_page->dn_aglevel = BMAPSZTOLEV(control_page->dn_agsize);
	l2nl =
	    control_page->dn_agl2size - (L2BPERDMAP +
					 control_page->dn_aglevel * L2LPERCTL);
	control_page->dn_agheigth = l2nl >> 1;
	control_page->dn_agwidth =
	    1 << (l2nl - (control_page->dn_agheigth << 1));
	for (index = 5 - control_page->dn_agheigth, control_page->dn_agstart =
	     0, n = 1; index > 0; index--) {
		control_page->dn_agstart += n;
		n <<= 2;
	}

	/* control_page->dn_maxag is computed at finalization */

	control_page->dn_agpref = 0;
}

/*--------------------------------------------------------------------
 * NAME:        calc_map_size()
 *
 * FUNCTION:    Calculates the size of a block map and
 *              initializes memory for dmap pages of map.
 *              Later when we are ready to write the map to disk
 *              we will initialize the rest of the map pages.
 *
 *              N.B. map file is ALLOCATED as a single extent
 *              of physical pages covering full control level (L2)
 *              tree control pages for the dmap pages required:
 *              the tree will be INITIALIZED to cover only the
 *              the dmap pages required;
 *
 * PARAMETERS:
 *      number_of_blocks        - Number of blocks in aggregate
 *      aggr_inodes             - Array of aggregate inodes
 *      aggr_block_size         - Aggregate block size
 *      ag_size                 - Will be filled in with AG size in blocks
 *      inostamp                - Inode stamp value to be used.
 *
 * RETURNS:     0 for success
 */
int calc_map_size(int64_t number_of_blocks,
		  struct dinode *aggr_inodes,
		  int aggr_block_size, int *ag_size, unsigned inostamp)
{
	int rc = 0;
	int64_t npages, ndmaps, nl0pages;
	int64_t nb_diskmap;
	int64_t size_of_map;
	int64_t location;

	/*
	 * compute the number dmap pages required to cover number_of_blocks;
	 * add one extra dmap page for extendfs(): this is added before
	 * we figure out how many control pages are needed, so we get
	 * the correct number of control pages.
	 */
	npages = ndmaps = ((number_of_blocks + BPERDMAP - 1) >> L2BPERDMAP) + 1;

	/*
	 * Make sure the number of dmaps needed is within the supported range
	 */
	if ((((int64_t) ndmaps) << L2BPERDMAP) > MAXMAPSIZE)
		return (EINVAL);

	/*
	 * compute number of (logical) control pages at each level of the map
	 */
	/* account for L0 pages to cover dmap pages */
	nl0pages = (ndmaps + LPERCTL - 1) >> L2LPERCTL;
	npages += nl0pages;

	if (nl0pages > 1) {
		/* account for one L2 and actual L1 pages to cover L0 pages */
		npages += 1 + ((nl0pages + LPERCTL - 1) >> L2LPERCTL);
	} else {
		/* account for one logical L2 and one logical L1 pages */
		npages += 2;
	}

	/* account for global control page of map */
	npages++;

	/*
	 * init the block allocation map inode
	 */
	size_of_map = npages << L2PSIZE;
	nb_diskmap = size_of_map / aggr_block_size;
	location = BMAP_OFF / aggr_block_size;

	init_inode(&(aggr_inodes[BMAP_I]), AGGREGATE_I,	/* di_fileset */
		   BMAP_I,	/* di_number */
		   nb_diskmap,	/* di_nblocks */
		   size_of_map,	/* di_size */
		   location, IFJOURNAL | IFREG,	/* di_mode */
		   max_extent_data, AITBL_OFF / aggr_block_size,
		   aggr_block_size, inostamp);

	/*
	 *  Allocate dmap pages and initialize them for the aggregate blocks
	 */
	if ((rc = alloc_map(ndmaps)) != 0)
		return rc;
	initmap(number_of_blocks, ag_size, aggr_block_size);

	/*
	 * reset last_allocated to ignore the fsck working space
	 */
	last_allocated = 0;

	return 0;
}

/*--------------------------------------------------------------------
 * NAME:        markit()
 *
 * FUNCTION:    Mark specified block allocated/unallocated in block map
 *
 * PARAMETERS:
 *      block   - Map object to set or clear
 *      flag    - Indicates ALLOCATE or FREE of block.  Indicates if block is
 *                bad.
 *
 * RETURNS: NONE
 */
int markit(int64_t block, unsigned flag)
{
	int page, rem, word, bit;
	struct dmap *p1;
	int agno;
	int64_t num_blocks_left, nb;

	/*
	 * Keep track of the last allocated block to be filled into block map
	 * inode.  Don't update last allocated for bad blocks.
	 */
	if (block > last_allocated && !(flag & BADBLOCK)) {
		last_allocated = block;
	}

	/*
	 *  calculate page number in map, and word and bit number in word.
	 */
	page = block / BPERDMAP;
	rem = block - page * BPERDMAP;
	word = rem >> L2DBWORD;
	bit = rem - (word << L2DBWORD);

	if (page > sz_block_map_array) {
		fprintf(stderr,
			"Internal error: %s(%d): Trying to mark block which doesn't exist.\n",
			__FILE__, __LINE__);
		return (-1);
	}

	/*
	 * Determine if this dmap page has been allocated yet
	 */
	if (block_map_array[page] == NULL) {
		num_blocks_left = control_page->dn_mapsize - (page * BPERDMAP);
		nb = MIN(BPERDMAP, num_blocks_left);
		block_map_array[page] = malloc(sizeof (struct dmap));
		if (block_map_array[page] == NULL) {
			message_user(MSG_OSO_INSUFF_MEMORY, NULL, 0, OSO_MSG);
			return (ENOMEM);
		}
		memset(block_map_array[page], 0, sizeof (struct dmap));
		ujfs_idmap_page(block_map_array[page], nb);
	}

	p1 = block_map_array[page];

	agno = block >> control_page->dn_agl2size;

	/*
	 *  now we process the first word.
	 */
	if (flag & ALLOC) {
		p1->pmap[word] |= (UZWORD >> bit);
		p1->wmap[word] |= (UZWORD >> bit);

		/*
		 * Update the stats
		 */
		p1->nfree--;
		control_page->dn_nfree--;
		control_page->dn_agfree[agno]--;
	} else {
		p1->pmap[word] &= ~(UZWORD >> bit);
		p1->wmap[word] &= ~(UZWORD >> bit);

		/*
		 * Update the stats
		 */
		p1->nfree++;
		control_page->dn_nfree++;
		control_page->dn_agfree[agno]++;
	}
	return (0);
}

/*--------------------------------------------------------------------
 * NAME:        write_block_map()
 *
 * FUNCTION:    Update tree part of block map to match rest of map and
 *              then write the block map to disk.
 *              Also write the block map inode to disk.
 *
 * PARAMETERS:
 *      dev_ptr         - open port of device to write map to
 *      size_of_map     - size of map
 *      aggr_block_size - size of an aggregate block in bytes
 *
 * RETURNS: 0 for success
 */
int write_block_map(FILE *dev_ptr, int64_t number_of_blocks, int aggr_block_size)
{
	int rc = 0;

	/*
	 * At this point all of the dmaps have been initialized except for their
	 * trees.  Now we need to build the other levels of the map and adjust
	 * the tree for each of the dmaps.
	 */
	cur_dmap_index = 0;
	control_page->dn_maxag = last_allocated / control_page->dn_agsize;
	rc = initbmap(dev_ptr, number_of_blocks);

	return rc;
}

/*--------------------------------------------------------------------
 * NAME: dbAlloc
 *
 * FUNCTION: Allocate the specified number of blocks
 *
 * PARAMETERS:
 *      xlen    - Number of blocks to allocate
 *      xaddr   - On return, filled in with starting block number of allocated
 *                blocks
 *
 * NOTES:
 *	This function is only called when adding blocks to the Bad Block Inode
 *	required a page for an xtree node.  LVM Bad Block processing must
 *	be in effect during this allocation.  This will not be the case if format
 *	is processing /L.  So, at entry to this routine, we check to see whether
 *	the LVM Bad Block processing is enabled and, if not, we enable it.
 *	At exit from this routine the LVM Bad Block processing will be as it
 *	was (i.e., enabled or disabled) on entry.
 *
 * RETURNS: 0 for success; Other indicates failure
 */
static int dbAlloc(FILE *dev_ptr, int64_t xlen, int64_t * xaddr)
{
	int rc = 0;
	int page, word;
	struct dmap *p1;
	int64_t last_block, index;
	unsigned mask, cmap;
	int bitno;
	int l2nb;
	int8_t leafw;

	/*
	 * Start looking at last block allocated for a contiguous extent of xlen
	 * blocks.  Since we may have bad blocks intermixed we can't just
	 * take blocks starting at the last block allocated.  However,
	 * last_allocated won't be updated with bad blocks, so it will be the
	 * start of the real last place to start looking.  Once found, mark them
	 * allocated and return the starting address in xaddr
	 */
	l2nb = log2shift(xlen);

	for (page = last_allocated / BPERDMAP,
	     word = (last_allocated & (BPERDMAP - 1)) >> L2DBWORD;
	     page < sz_block_map_array; page++, word = 0) {
		/*
		 * Determine if this dmap page has been allocated yet; if not we
		 * can take the first blocks from it for our allocation since we
		 * know all the blocks in it are free.  (markit will handle
		 * allocating the page for us, so we don't have to do that here.)
		 */
		if (block_map_array[page] == NULL) {
			*xaddr = page << L2BPERDMAP;
			last_block = *xaddr + xlen;
			for (index = *xaddr;
			     ((index < last_block) && (rc == 0)); index++) {
				rc = markit(index, ALLOC);
			}
			if (rc != 0) {
				return rc;
			}
			return 0;
		}

		/*
		 * We have a dmap page which has had allocations before, we need
		 * to check for free blocks starting with <word> to the end of
		 * this dmap page.  If we don't find it in this page we will go
		 * on to the next page.
		 */
		p1 = block_map_array[page];

		for (; word < LPERDMAP; word++) {
			/*
			 * Determine if the leaf describes sufficient free space.
			 * Since we have not yet completed the block map
			 * initialization we will have to compute this on the-fly.
			 */
			leafw = ujfs_maxbuddy((char *) &p1->wmap[word]);
			if (leafw < l2nb)
				continue;

			/*
			 * We know this word has sufficient free space, find it
			 * and allocate it
			 */
			*xaddr = (page << L2BPERDMAP) + (word << L2DBWORD);

			if (leafw < BUDMIN) {
				mask = ONES << (DBWORD - xlen);
				cmap = ~(p1->wmap[word]);

				/* scan the word for xlen free bits */
				for (bitno = 0; mask != 0; bitno++, mask >>= 1) {
					if ((mask & cmap) == mask)
						break;
				}

				*xaddr += bitno;
			}

			/* Allocate the blocks */
			last_block = *xaddr + xlen;
			for (index = *xaddr;
			     ((index < last_block) && (rc == 0)); index++) {
				rc = markit(index, ALLOC);
			}
			if (rc != 0) {
				return (rc);
			}
			return 0;
		}
	}
	return 1;
}

/*--------------------------------------------------------------------
 * NAME: xtSplitRoot
 *
 * FUNCTION: Split full root of xtree
 *
 * PARAMETERS:
 *      dev_ptr - Device handle
 *      ip      - Inode of xtree
 *      xroot   - Root of xtree
 *      offset  - Offset of extent to add
 *      nblocks - number of blocks for extent to add
 *      blkno   - starting block of extent to add
 *
 * RETURNS: 0 for success; Other indicates failure
 */
static int xtSplitRoot(FILE *dev_ptr,
		       struct dinode *ip,
		       struct xtree_buf *xroot,
		       int64_t offset, int nblocks, int64_t blkno)
{
	xtpage_t *rootpage;
	xtpage_t *newpage;
	int64_t xaddr;
	int nextindex;
	xad_t *xad;
	int rc = 0;
	struct xtree_buf *newbuf;
	int xlen;

	/* Allocate and initialize buffer for new page to accomodate the split */
	newbuf = malloc(sizeof (struct xtree_buf));
	if (newbuf == NULL) {
		message_user(MSG_OSO_INSUFF_MEMORY, NULL, 0, OSO_MSG);
		return (ENOMEM);
	}
	newbuf->up = xroot;
	if (xroot->down == NULL) {
		badblock_pages = newbuf;
	} else {
		xroot->down->up = newbuf;
	}
	newbuf->down = xroot->down;
	xroot->down = newbuf;
	newpage = newbuf->page = malloc(PSIZE);
	if (newpage == NULL) {
		message_user(MSG_OSO_INSUFF_MEMORY, NULL, 0, OSO_MSG);
		return (ENOMEM);
	}

	/* Allocate disk blocks for new page */
	xlen = 1 << control_page->dn_l2nbperpage;
	if ((rc = dbAlloc(dev_ptr, xlen, &xaddr)))
		return rc;

	rootpage = xroot->page;

	/* Initialize new page */
	newpage->header.flag =
	    (rootpage->header.flag & BT_LEAF) ? BT_LEAF : BT_INTERNAL;
	PXDlength(&(newpage->header.self), xlen);
	PXDaddress(&(newpage->header.self), xaddr);
	newpage->header.nextindex = XTENTRYSTART;
	newpage->header.maxentry = PSIZE >> L2XTSLOTSIZE;

	/* initialize sibling pointers */
	newpage->header.next = 0;
	newpage->header.prev = 0;

	/* copy the in-line root page into new right page extent */
	nextindex = rootpage->header.maxentry;
	memcpy(&newpage->xad[XTENTRYSTART], &rootpage->xad[XTENTRYSTART],
	       (nextindex - XTENTRYSTART) << L2XTSLOTSIZE);

	/* insert the new entry into the new right/child page */
	xad = &newpage->xad[nextindex];
	XADoffset(xad, offset);
	XADlength(xad, nblocks);
	XADaddress(xad, blkno);

	/* update page header */
	newpage->header.nextindex = nextindex + 1;

	/* init root with the single entry for the new right page */
	xad = &rootpage->xad[XTENTRYSTART];
	XADoffset(xad, 0);
	XADlength(xad, xlen);
	XADaddress(xad, xaddr);

	/* update page header of root */
	rootpage->header.flag &= ~BT_LEAF;
	rootpage->header.flag |= BT_INTERNAL;

	rootpage->header.nextindex = XTENTRYSTART + 1;

	/* Update nblocks for inode to account for new page */
	ip->di_nblocks += xlen;

	return 0;
}

/*--------------------------------------------------------------------
 * NAME: xtSplitPage
 *
 * FUNCTION: Split non-root page of xtree
 *
 * PARAMETERS:
 *      ip      - Inode of xtree splitting
 *      xpage   - page to split
 *      offset  - offset of new extent to add
 *      nblocks - number of blocks of new extent to add
 *      blkno   - starting block number of new extent to add
 *      dev_ptr - Device handle
 *      aggr_block_size - aggregate block size
 *
 * RETURNS: 0 for success; Other indicates failure
 */
static int xtSplitPage(struct dinode *ip,
		       struct xtree_buf *xpage,
		       int64_t offset,
		       int nblocks,
		       int64_t blkno, FILE *dev_ptr, int aggr_block_size)
{
	int rc = 0;
	int64_t xaddr;		/* new right page block number */
	xad_t *xad;
	int xlen;
	xtpage_t *lastpage, *newpage;
	int64_t leftbn;

	/* Allocate disk space for the new xtree page */
	xlen = 1 << control_page->dn_l2nbperpage;
	if ((rc = dbAlloc(dev_ptr, xlen, &xaddr)))
		return rc;

	/*
	 * Modify xpage's next entry to point to the new disk space,
	 * write the xpage to disk since we won't be needing it anymore.
	 */
	lastpage = xpage->page;
	lastpage->header.next = xaddr;

	leftbn = addressPXD(&(lastpage->header.self));

	/* swap if on big endian machine */
	ujfs_swap_xtpage_t(lastpage);
	rc = ujfs_rw_diskblocks(dev_ptr, leftbn * aggr_block_size, PSIZE,
				lastpage, PUT);
	ujfs_swap_xtpage_t(lastpage);

	if (rc != 0)
		return rc;

	/*
	 * We are now done with the xpage as-is.  We can now re-use this buffer
	 * for our new buffer.
	 */
	newpage = xpage->page;

	PXDlength(&(newpage->header.self), xlen);
	PXDaddress(&(newpage->header.self), xaddr);
	newpage->header.flag = newpage->header.flag & BT_TYPE;

	/* initialize sibling pointers of newpage */
	newpage->header.next = 0;
	newpage->header.prev = leftbn;

	/* insert entry at the first entry of the new right page */
	xad = &newpage->xad[XTENTRYSTART];
	XADoffset(xad, offset);
	XADlength(xad, nblocks);
	XADaddress(xad, blkno);

	newpage->header.nextindex = XTENTRYSTART + 1;

	/* Now append new page to parent page */
	rc = xtAppend(dev_ptr, ip, offset, xaddr, xlen, xpage->up,
		      aggr_block_size);

	/* Update inode to account for new page */
	ip->di_nblocks += xlen;

	return rc;
}

/*--------------------------------------------------------------------
 * NAME: xtAppend
 *
 * FUNCTION: Append an extent to the specified file
 *
 * PARAMETERS:
 *      dev_ptr - Device handle
 *      di      - Inode to add extent to
 *      offset  - offset of extent to add
 *      blkno   - block number of start of extent to add
 *      nblocks - number of blocks in extent to add
 *      xpage   - xtree page to add extent to
 *      aggr_block_size - aggregate block size in bytes
 *
 * NOTES: xpage points to its parent in the xtree and its rightmost child (if it
 *      has one).  It also points to the buffer for the page.
 *
 * RETURNS: 0 for success; Other indicates failure
 */
static int xtAppend(FILE *dev_ptr,
		    struct dinode *di,
		    int64_t offset,
		    int64_t blkno,
		    int nblocks, struct xtree_buf *xpage, int aggr_block_size)
{
	int rc = 0;
	int index;
	xad_t *xad;
	xtpage_t *cur_page;

	cur_page = xpage->page;
	index = cur_page->header.nextindex;

	/* insert entry for new extent */
	if (index == cur_page->header.maxentry) {
		/*
		 * There is not room in this page to add the entry; Need to
		 * create a new page
		 */
		if (cur_page->header.flag & BT_ROOT) {
			/* This is the root of the xtree; need to split root */
			rc = xtSplitRoot(dev_ptr, di, xpage, offset, nblocks,
					 blkno);
		} else {
			/*
			 * Non-root page: add new page at this level, xtSplitPage()
			 * calls xtAppend again to propogate up the new page entry
			 */
			rc = xtSplitPage(di, xpage, offset, nblocks, blkno,
					 dev_ptr, aggr_block_size);
		}
	} else {
		/* There is room to add the entry to this page */
		xad = &cur_page->xad[index];
		XADoffset(xad, offset);
		XADlength(xad, nblocks);
		XADaddress(xad, blkno);

		/* advance next available entry index */
		++cur_page->header.nextindex;

		rc = 0;
	}

	return rc;
}

/*--------------------------------------------------------------------
 * NAME: add_bad_block
 *
 * FUNCTION: Add an extent of <thisblk> to the <bb_inode> inode
 *
 * PRE CONDITIONS: badblock_pages has been initialized
 *
 * PARAMETERS:
 *      dev_ptr - Device handle
 *      thisblk - block number of bad block to add
 *      aggr_block_size - Size of an aggregate block
 *      bb_inode        - Inode to add bad block to
 *
 * RETURNS: 0 for success; Other indicates failure
 */
static int add_bad_block(FILE *dev_ptr, int64_t thisblk, int aggr_block_size,
			 struct dinode *bb_inode)
{
	int rc = 0;

	/* Mark block allocated in map */
	rc = markit(thisblk, ALLOC | BADBLOCK);
	if (rc != 0) {
		return (rc);
	}
	/* Add to inode: add an extent for this block to the inode's tree */
	rc = xtAppend(dev_ptr, bb_inode, bb_inode->di_size / aggr_block_size,
		      thisblk, 1, badblock_pages, aggr_block_size);

	if (!rc) {		/* append was successful */
		bb_inode->di_size += aggr_block_size;
		bb_inode->di_nblocks++;
	}

	return rc;
}

/*--------------------------------------------------------------------
 * NAME: verify_last_blocks
 *
 * FUNCTION: Verify blocks in aggregate not initialized
 *
 * PARAMETERS:
 *      dev_ptr - Device handle
 *      aggr_block_size - aggregate block size in bytes
 *      bb_inode        - Inode for bad blocks
 *
 * NOTES: Any bad blocks found will be added to the bad block inode
 *
 * RETURNS: 0 for success; Other indicates failure
 */
#define L2MEGABYTE      20
#define MEGABYTE        (1 << L2MEGABYTE)

/* Define a parameter array for messages */
#define MAXPARMS        1
#define MAXSTR          128
static char *msg_parms[MAXPARMS];
static char msgstr[MAXSTR];

int verify_last_blocks(FILE *dev_ptr, int aggr_block_size,
		       struct dinode *bb_inode)
{
	int rc = 0;
	int error;
	void *buffer = NULL;
	int bufsize = PSIZE << 5;
	int nbufblks;
	int64_t nblocks, nb;
	int64_t blkno, thisblk;
	int percent, section, index;
	bool write_inode = false;
	struct xtree_buf *curpage;
	long flags;

	if (badblock_pages == NULL) {
		/*
		 * Initialize list of xtree append buffers
		 */
		badblock_pages = malloc(sizeof (struct xtree_buf));
		if (badblock_pages == NULL) {
			message_user(MSG_OSO_INSUFF_MEMORY, NULL, 0, OSO_MSG);
			return (ENOMEM);
		}
		badblock_pages->down = badblock_pages->up = NULL;
		badblock_pages->page = (xtpage_t *) & bb_inode->di_btroot;
	}

	/* Allocate and clear a buffer */
	while ((bufsize >= aggr_block_size) &&
#ifdef HAVE_POSIX_MEMALIGN
	       posix_memalign(&buffer, aggr_block_size, bufsize))
#else
#ifdef HAVE_MEMALIGN
			(buffer = memalign(aggr_block_size, bufsize)) == NULL)
#else
			(buffer = valloc(bufsize)) == NULL)
#endif
#endif
		bufsize >>= 1;

	if (buffer == NULL) {
		message_user(MSG_OSO_INSUFF_MEMORY, NULL, 0, OSO_MSG);
		return (ENOMEM);
	}
	memset(buffer, 0, bufsize);
	nbufblks = bufsize / aggr_block_size;

#ifdef O_DIRECT
	/*
	 * Must do direct-io to avoid the page cache
	 */
	flags = fcntl(fileno(dev_ptr), F_GETFL);
	fcntl(fileno(dev_ptr), F_SETFL, flags | O_DIRECT);
#endif

	/*
	 * Starting from the last allocated block to the end of the aggregate
	 * write the empty buffer to disk.
	 */
	blkno = last_allocated + 1;
	nblocks = control_page->dn_mapsize - blkno;
	section =
	    MAX(control_page->dn_mapsize >> 7, MEGABYTE / aggr_block_size);
	for (index = section; nblocks > 0; index += nb) {
		if (index > section) {
			percent = blkno * 100 / control_page->dn_mapsize;
			sprintf(msgstr, "%d", percent);
			msg_parms[0] = msgstr;
			message_user(MSG_OSO_PERCENT_FORMAT, msg_parms, 1,
				     OSO_MSG);
			fprintf(stdout, "\r");
			fflush(stdout);
			index = 0;
		}
		nb = MIN(nblocks, nbufblks);
		error = ujfs_rw_diskblocks(dev_ptr, blkno * aggr_block_size,
					   nb * aggr_block_size, buffer, PUT);
		/*
		 * most devices don't report an error on write, so we have to
		 * verify explicitly to be sure.
		 */
		if (error == 0) {
			error =
			    ujfs_rw_diskblocks(dev_ptr, blkno * aggr_block_size,
					       nb * aggr_block_size, buffer,
					       GET);
		}

		if (error != 0) {
			/*
			 * At least one of the blocks we just tried to write was
			 * bad.  To narrow down the problem, we will write each
			 * block individually and add any bad ones to our bad
			 * block inode.
			 */
			for (thisblk = blkno; thisblk < blkno + nb; thisblk++) {
				error =
				    ujfs_rw_diskblocks(dev_ptr,
						       thisblk *
						       aggr_block_size,
						       aggr_block_size, buffer,
						       PUT);

				/*
				 * most devices don't report an error on write,
				 * so we have to verify explicitly to be sure.
				 */
				if (error == 0) {
					error =
					    ujfs_rw_diskblocks(dev_ptr,
							       thisblk *
							       aggr_block_size,
							       aggr_block_size,
							       buffer, GET);
				}

				if (error != 0) {
					/* add_bad_block may do unaligned I/O */
#ifdef O_DIRECT
					fcntl(fileno(dev_ptr), F_SETFL, flags);
#endif

					/* Add this block to bad list */
					if ((rc =
					     add_bad_block(dev_ptr, thisblk,
							   aggr_block_size,
							   bb_inode)))
						continue;
					write_inode = true;
#ifdef O_DIRECT
					fcntl(fileno(dev_ptr), F_SETFL,
					      flags | O_DIRECT);
#endif

					/*
					 * In case we allocated blocks for our
					 * addressing structure after our current
					 * bad block, we need to move our block
					 * number up so we don't overwrite any
					 * changes we have just done.
					 */
					thisblk = MAX(last_allocated, thisblk);
				}
			}

			/*
			 * In case we allocated blocks for the bad block map
			 * inode's addressing structure, skip past them so we
			 * don't wipe out our work.
			 */
			blkno += nb;
			if (blkno != thisblk) {
				blkno = thisblk;
				nblocks = control_page->dn_mapsize - blkno;
			} else {
				nblocks -= nb;
			}
		} else {
			blkno += nb;
			nblocks -= nb;
		}
	}
#ifdef O_DIRECT
	fcntl(fileno(dev_ptr), F_SETFL, flags);
#endif
	msg_parms[0] = "100";
	message_user(MSG_OSO_PERCENT_FORMAT, msg_parms, 1, OSO_MSG);
	fprintf(stdout, "\n");

	free(buffer);

	if (write_inode == true) {
		/* We added bad blocks, flush pages to disk */
		curpage = badblock_pages;

		while (!(curpage->page->header.flag & BT_ROOT)) {
			blkno = addressPXD(&(curpage->page->header.self));

			/* swap if on big endian machine */
			ujfs_swap_xtpage_t(curpage->page);
			rc = ujfs_rw_diskblocks(dev_ptr,
						blkno * aggr_block_size, PSIZE,
						curpage->page, PUT);
			ujfs_swap_xtpage_t(curpage->page);

			if (rc != 0)
				return rc;

			curpage = curpage->up;
		}

		/* Write the bad block inode itself */
		rc = ujfs_rwinode(dev_ptr, bb_inode, BADBLOCK_I, PUT,
				  aggr_block_size, AGGREGATE_I, type_jfs);
	}
	return rc;
}
