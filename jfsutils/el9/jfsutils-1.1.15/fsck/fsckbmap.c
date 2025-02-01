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
#include <string.h>
/* defines and includes common among the fsck.jfs modules */
#include "xfsckint.h"
#include "diskmap.h"

 /* + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + +
  *
  * superblock buffer pointer
  *
  *      defined in xchkdsk.c
  */
extern struct superblock *sb_ptr;

 /* + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + +
  *
  * fsck aggregate info structure pointer
  *
  *      defined in xchkdsk.c
  */
extern struct fsck_agg_record *agg_recptr;

 /* + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + +
  *
  * fsck block map info structure pointer
  *
  *      defined in xchkdsk.c
  */
extern struct fsck_bmap_record *bmap_recptr;

/* VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV
 *
 * The following are internal to this file
 *
 */

struct fsck_stree_proc_parms {
	dmtree_t *buf_tree;
	int8_t *buf_stree;
	int8_t *wsp_stree;
	int32_t nleafs;
	int32_t l2nleafs;
	int32_t leafidx;
	int8_t budmin;
	int page_level;
	uint32_t page_ordno;
	int8_t *lfval_error;
	int8_t *intval_error;
};

int ctlpage_rebuild(int8_t);

int ctlpage_verify(int8_t);

int dmap_pwmap_rebuild(uint32_t *);

int dmap_pmap_verify(uint32_t *);

int dmap_tree_rebuild(int8_t *);

int dmap_tree_verify(int8_t *);

int dmappg_rebuild(int8_t *);

int dmappg_verify(int8_t *);

int init_bmap_info(void);

int Ln_tree_rebuild(int, int64_t, struct dmapctl **, int8_t *);

int Ln_tree_verify(int, int64_t, struct dmapctl **, int8_t *);

int stree_rebuild(struct fsck_stree_proc_parms *, int8_t *);

int stree_verify(struct fsck_stree_proc_parms *, int8_t *);

int verify_blkall_summary_msgs(void);

/* VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV */

/*****************************************************************************
 * NAME: ctlpage_rebuild
 *
 * FUNCTION: Rebuild the control page of the filesystem block map.
 *
 * PARAMETERS:
 *      max_buddy  - input - the data value in the root of the highest
 *                           Lx page in the map.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int ctlpage_rebuild(int8_t max_buddy)
{
	int bcr_rc = FSCK_OK;
	int32_t highest_active_AG = 0, num_active_AGs = 0, num_inactive_AGs;
	int64_t avg_free, actAG_free, inactAG_free;
	int32_t l2nl, n, agidx, index, aglevel, agheight, agwidth, agstart;

	bcr_rc = mapctl_get(bmap_recptr->bmpctl_agg_fsblk_offset,
			    (void **) &(bmap_recptr->bmpctl_bufptr));

	if (bcr_rc == FSCK_OK) {

		/* swap if on big endian machine */
		ujfs_swap_dbmap((struct dbmap *) bmap_recptr->bmpctl_bufptr);

		bmap_recptr->bmpctl_bufptr->dn_mapsize =
		    bmap_recptr->total_blocks;
		bmap_recptr->bmpctl_bufptr->dn_nfree = bmap_recptr->free_blocks;
		bmap_recptr->bmpctl_bufptr->dn_l2nbperpage =
		    agg_recptr->log2_blksperpg;
		bmap_recptr->bmpctl_bufptr->dn_numag = agg_recptr->num_ag;

		bmap_recptr->bmpctl_bufptr->dn_maxlevel =
		    BMAPSZTOLEV(agg_recptr->sb_agg_fsblk_length);

		/* check out the active AGs */
		for (agidx = 0; (agidx < MAXAG); agidx++) {
			if (bmap_recptr->AGActive[agidx]) {
				highest_active_AG = agidx;
				num_active_AGs++;
			}
		}	/* end check out the active AGs */

		bmap_recptr->bmpctl_bufptr->dn_maxag = highest_active_AG;

		num_inactive_AGs = agg_recptr->num_ag - num_active_AGs;
		inactAG_free = num_inactive_AGs * sb_ptr->s_agsize;
		actAG_free = bmap_recptr->free_blocks - inactAG_free;
		avg_free = actAG_free / num_active_AGs;

		if ((bmap_recptr->bmpctl_bufptr->dn_agpref > highest_active_AG)
		    || (bmap_recptr->bmpctl_bufptr->dn_agfree[bmap_recptr->
		     bmpctl_bufptr->dn_agpref] < avg_free)) {
			/* preferred AG is not valid */
			if (avg_free == 0) {
				bmap_recptr->bmpctl_bufptr->dn_agpref = 0;
			} else {
				bmap_recptr->bmpctl_bufptr->dn_agpref = -1;
				for (agidx = 0; ((agidx < MAXAG) &&
						 (bmap_recptr->bmpctl_bufptr->
						  dn_agpref < 0)); agidx++) {
					if (bmap_recptr->AGFree_tbl[agidx] >=
					    avg_free) {
						bmap_recptr->bmpctl_bufptr->
						    dn_agpref = agidx;
					}
				}
			}
		}
		aglevel = BMAPSZTOLEV(sb_ptr->s_agsize);
		l2nl =
		    agg_recptr->log2_blksperag - (L2BPERDMAP +
						  aglevel * L2LPERCTL);
		agheight = l2nl >> 1;
		agwidth = 1 << (l2nl - (agheight << 1));
		for (index = 5 - agheight, agstart = 0, n = 1; index > 0;
		     index--) {
			agstart += n;
			n <<= 2;
		}

		bmap_recptr->bmpctl_bufptr->dn_aglevel = aglevel;
		bmap_recptr->bmpctl_bufptr->dn_agheigth = agheight;
		bmap_recptr->bmpctl_bufptr->dn_agwidth = agwidth;
		bmap_recptr->bmpctl_bufptr->dn_agstart = agstart;
		bmap_recptr->bmpctl_bufptr->dn_agl2size =
		    agg_recptr->log2_blksperag;
		bmap_recptr->bmpctl_bufptr->dn_agsize = sb_ptr->s_agsize;
		bmap_recptr->bmpctl_bufptr->dn_maxfreebud = max_buddy;

		for (agidx = 0; (agidx < MAXAG); agidx++) {
			/* rebuild the free list */
			bmap_recptr->bmpctl_bufptr->dn_agfree[agidx] =
			    bmap_recptr->AGFree_tbl[agidx];
		}

		/* swap if on big endian machine */
		ujfs_swap_dbmap(bmap_recptr->bmpctl_bufptr);

		/*
		 * write the updated control page back onto the device
		 */
		bcr_rc = mapctl_put((void *) bmap_recptr->bmpctl_bufptr);
	}
	return (bcr_rc);
}

/*****************************************************************************
 * NAME: ctlpage_verify
 *
 * FUNCTION: Verify the control page of the filesystem block map.
 *
 * PARAMETERS:
 *      max_buddy  - input - the data value which should be in the root
 *                           of the highest Lx page in the map.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int ctlpage_verify(int8_t max_buddy)
{
	int bcv_rc = FSCK_OK;
	unsigned agidx;
	int32_t max_level = 0;
	int32_t highest_active_AG = 0;
	int32_t l2nl, n, index, aglevel, agheight, agwidth, agstart;

	bcv_rc = mapctl_get(bmap_recptr->bmpctl_agg_fsblk_offset,
			    (void **) &(bmap_recptr->bmpctl_bufptr));

	if (bcv_rc == FSCK_OK) {
		/* got the control page in the buffer */

		/* swap if on big endian machine */
		ujfs_swap_dbmap((struct dbmap *) bmap_recptr->bmpctl_bufptr);

		if (bmap_recptr->bmpctl_bufptr->dn_mapsize != bmap_recptr->total_blocks) {
			/* bad number of blocks in the aggregate */
			bmap_recptr->ctl_other_error = -1;
			fsck_send_msg(fsck_BMAPCASB);
		}
		if (bmap_recptr->bmpctl_bufptr->dn_nfree != bmap_recptr->free_blocks) {
			/* bad number of free blocks in the aggregate */
			bmap_recptr->ctl_other_error = -1;
			fsck_send_msg(fsck_BMAPCNF);
		}
		if (bmap_recptr->bmpctl_bufptr->dn_l2nbperpage != agg_recptr->log2_blksperpg) {
			/* bad log2( blocks per page ) */
			bmap_recptr->ctl_other_error = -1;
			fsck_send_msg(fsck_BMAPCL2BPP);
		}
		if (bmap_recptr->bmpctl_bufptr->dn_numag != agg_recptr->num_ag) {
			/* bad number of alloc groups */
			bmap_recptr->ctl_other_error = -1;
			fsck_send_msg(fsck_BMAPCNAG);
		}
		max_level = BMAPSZTOLEV(agg_recptr->sb_agg_fsblk_length);
		if (bmap_recptr->bmpctl_bufptr->dn_maxlevel != max_level) {
			/* bad maximum block map level */
			bmap_recptr->ctl_other_error = -1;
			fsck_send_msg(fsck_BMAPCMXLVL);
		}
		for (agidx = 0; (agidx < MAXAG); agidx++) {
			/* check out the active AGs */
			if (bmap_recptr->AGActive[agidx]) {
				highest_active_AG = agidx;
			}
		}
		/*
		 * format does not include blocks allocated to the bad block inode
		 * when it determines the dn_maxag.  Subsequent activity may or
		 * may not have altered dn_maxag based on blocks allocated to the
		 * bad block inode.  All we know for sure is that dn_maxag shouldn't
		 * be larger than appropriate for the highest allocated block.
		 */
		if (bmap_recptr->bmpctl_bufptr->dn_maxag > highest_active_AG) {
			/* bad highest active alloc group */
			bmap_recptr->ctl_other_error = -1;
			fsck_send_msg(fsck_BMAPCMAAG);
		}
		if (bmap_recptr->bmpctl_bufptr->dn_agpref > highest_active_AG) {
			/* bad preferred alloc group */
			bmap_recptr->ctl_other_error = -1;
			fsck_send_msg(fsck_BMAPCPAG);
		}
		aglevel = BMAPSZTOLEV(sb_ptr->s_agsize);
		l2nl =
		    agg_recptr->log2_blksperag - (L2BPERDMAP +
						  aglevel * L2LPERCTL);
		agheight = l2nl >> 1;
		agwidth = 1 << (l2nl - (agheight << 1));
		for (index = 5 - agheight, agstart = 0, n = 1; index > 0;
		     index--) {
			agstart += n;
			n <<= 2;
		}		/* end for index */

		if (bmap_recptr->bmpctl_bufptr->dn_aglevel != aglevel) {
			/* bad level holding an AG */
			bmap_recptr->ctl_other_error = -1;
			fsck_send_msg(fsck_BMAPCDMCLAG);
		}
		if (bmap_recptr->bmpctl_bufptr->dn_agheigth != agheight) {
			/* bad dmapctl height holding an AG */
			bmap_recptr->ctl_other_error = -1;
			fsck_send_msg(fsck_BMAPCDMCLH);
		}
		if (bmap_recptr->bmpctl_bufptr->dn_agwidth != agwidth) {
			/* bad width at level holding an AG */
			bmap_recptr->ctl_other_error = -1;
			fsck_send_msg(fsck_BMAPCDMCLW);
		}
		if (bmap_recptr->bmpctl_bufptr->dn_agstart != agstart) {
			/* bad start idx at level holding an AG */
			bmap_recptr->ctl_other_error = -1;
			fsck_send_msg(fsck_BMAPCDMCSTI);
		}
		if (bmap_recptr->bmpctl_bufptr->dn_agl2size != agg_recptr->log2_blksperag) {
			/* bad log2(fsblks per AG) */
			bmap_recptr->ctl_other_error = -1;
			fsck_send_msg(fsck_BMAPCL2BPAG);
		}
		if (bmap_recptr->bmpctl_bufptr->dn_agsize != sb_ptr->s_agsize) {
			/* bad fsblks per AG */
			bmap_recptr->ctl_other_error = -1;
			fsck_send_msg(fsck_BMAPCBPAG);
		}
		if (bmap_recptr->bmpctl_bufptr->dn_maxfreebud != max_buddy) {
			/* bad max free buddy system */
			bmap_recptr->ctl_other_error = -1;
			fsck_send_msg(fsck_BMAPCBMXB);
		}
		for (agidx = 0; (agidx < MAXAG); agidx++) {
			/* verify the free list */
			if (bmap_recptr->bmpctl_bufptr->dn_agfree[agidx] !=
			    bmap_recptr->AGFree_tbl[agidx]) {
				/* bad free blocks in the AG */
				bmap_recptr->ctl_fctl_error = -1;
				fsck_send_msg(fsck_BMAPCAGNF, agidx);
			}
		}
	}
	return (bcv_rc);
}

/*****************************************************************************
 * NAME: dmap_pwmap_rebuild
 *
 * FUNCTION:  Rebuild the pmap in the current block map dmap page.
 *
 * PARAMETERS:
 *      pmap_freeblks  - input - pointer to a variable in which the number
 *                               of free blocks described by the dmap page
 *                               will be returned.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int dmap_pwmap_rebuild(uint32_t * pmap_freeblks)
{
	int bdpr_rc = FSCK_OK;
	uint32_t bitmask;
	int64_t wsp_pagenum;
	uint32_t wsp_byteoffset;
	struct fsck_blk_map_page *wsp_page;
	uint32_t *wsp_bits = NULL;
	int32_t map_wordidx, word_bitidx;
	int8_t max_buddy;

	/*
	 * locate the section of the workspace bit map which corresponds
	 * to this dmap's pmap
	 */
	bdpr_rc = blkmap_find_bit(bmap_recptr->dmap_1stblk, &wsp_pagenum,
				  &wsp_byteoffset, &bitmask);
	if (bdpr_rc == FSCK_OK) {
		bdpr_rc = blkmap_get_page(wsp_pagenum, &wsp_page);
		if (bdpr_rc == FSCK_OK) {
			wsp_bits =
			    (uint32_t *) ((char *) wsp_page + wsp_byteoffset);
		}
	}

	*pmap_freeblks = 0;

	if (bdpr_rc == FSCK_OK) {
		for (map_wordidx = 0; (map_wordidx < LPERDMAP); map_wordidx++) {
			max_buddy =
			    ujfs_maxbuddy((char *) &(wsp_bits[map_wordidx]));
			bmap_recptr->dmap_wsp_sleafs[map_wordidx] = max_buddy;

			bmap_recptr->dmap_bufptr->wmap[map_wordidx] =
			    wsp_bits[map_wordidx];
			/*
			 * copy the word from workspace to buffer,
			 * into both the working map and the permanent map.
			 */
			bmap_recptr->dmap_bufptr->pmap[map_wordidx] =
			    wsp_bits[map_wordidx];
			/*
			 * count the free blocks described by the word
			 */
			bitmask = 0x80000000u;
			for (word_bitidx = 0; (word_bitidx < DBWORD);
			     word_bitidx++) {

				if (!(wsp_bits[map_wordidx] & bitmask)) {
					/* it's free */
					(*pmap_freeblks)++;
					agg_recptr->free_blocks_in_aggregate++;
				} else {
					/* it's allocated */
					agg_recptr->blocks_used_in_aggregate++;
				}	/* end else it's allocated */

				bitmask = bitmask >> 1;	/* advance to the next bit */
			}
		}
	}
	return (bdpr_rc);
}

/*****************************************************************************
 * NAME: dmap_pmap_verify
 *
 * FUNCTION:  Verify the pmap in the current block map dmap page.
 *
 * PARAMETERS:
 *      pmap_freeblks  - input - pointer to a variable in which the number
 *                               of free blocks described by the dmap page
 *                               will be returned.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int dmap_pmap_verify(uint32_t * pmap_freeblks)
{
	int bdpv_rc = FSCK_OK;
	uint32_t bitmask;
	int64_t wsp_pagenum;
	uint32_t wsp_byteoffset;
	struct fsck_blk_map_page *wsp_page;
	uint32_t *wsp_bits = NULL;
	int32_t map_wordidx, word_bitidx;
	int8_t max_buddy;
	uint32_t unmarked_range_first_ordno = 0;
	int32_t unmarked_range_wordidx = 0;
	int32_t unmarked_range_bitidx = 0;
	int64_t size_of_unmarked_range = 0;
	uint32_t marked_range_first_ordno = 0;
	int32_t marked_range_wordidx = 0;
	int32_t marked_range_bitidx = 0;
	int64_t size_of_marked_range = 0;

	/*
	 * locate the section of the workspace bit map which corresponds
	 * to this dmap's pmap
	 */
	bdpv_rc = blkmap_find_bit(bmap_recptr->dmap_1stblk, &wsp_pagenum,
				  &wsp_byteoffset, &bitmask);
	if (bdpv_rc == FSCK_OK) {
		bdpv_rc = blkmap_get_page(wsp_pagenum, &wsp_page);
		if (bdpv_rc == FSCK_OK) {	/* got the page */
			wsp_bits =
			    (uint32_t *) ((char *) wsp_page + wsp_byteoffset);
		}
	}

	*pmap_freeblks = 0;

	if (bdpv_rc != FSCK_OK)
		goto bdpv_exit;

	for (map_wordidx = 0; (map_wordidx < LPERDMAP); map_wordidx++) {
		max_buddy = ujfs_maxbuddy((char *) &(wsp_bits[map_wordidx]));
		bmap_recptr->dmap_wsp_sleafs[map_wordidx] = max_buddy;
		bitmask = 0x80000000u;
		for (word_bitidx = 0; (word_bitidx < DBWORD); word_bitidx++) {
			if (wsp_bits[map_wordidx] & bitmask) {
				agg_recptr->blocks_used_in_aggregate++;
				if (!(bmap_recptr->dmap_bufptr->
				      pmap[map_wordidx] & bitmask)) {
					/* pmap says not */
					bmap_recptr->dmap_pmap_error = -1;
					if (size_of_unmarked_range == 0) {
						if (size_of_marked_range != 0) {
							fsck_send_msg
							    (fsck_PMAPSBOFF,
							     (long long) size_of_marked_range,
							     marked_range_first_ordno,
							     marked_range_wordidx,
							     marked_range_bitidx);
							size_of_marked_range = 0;
						}
						unmarked_range_first_ordno =
						    bmap_recptr->dmappg_ordno;
						unmarked_range_wordidx =
						    map_wordidx;
						unmarked_range_bitidx =
						    word_bitidx;
						size_of_unmarked_range = 1;
					} else {
						/* not the first in the range */
						size_of_unmarked_range++;
					}
				} else {
					/* pmap agrees */
					if (size_of_marked_range != 0) {
						/* marked range ended */
						fsck_send_msg(fsck_PMAPSBOFF,
							      (long long) size_of_marked_range,
							      marked_range_first_ordno,
							      marked_range_wordidx,
							      marked_range_bitidx);
						size_of_marked_range = 0;
					}
					if (size_of_unmarked_range != 0) {
						/* unmarked range ended */
						fsck_send_msg(fsck_PMAPSBON,
							      (long long) size_of_unmarked_range,
							      unmarked_range_first_ordno,
							      unmarked_range_wordidx,
							      unmarked_range_bitidx);
						size_of_unmarked_range = 0;
					}
				}
			} else {
				/* fsck says not allocated */
				(*pmap_freeblks)++;
				agg_recptr->free_blocks_in_aggregate++;
				if ((bmap_recptr->dmap_bufptr->pmap[map_wordidx]
				    & bitmask)) {
					/* pmap says yes */
					bmap_recptr->dmap_pmap_error = -1;
					if (size_of_marked_range == 0) {
						if (size_of_unmarked_range != 0) {
							fsck_send_msg
							    (fsck_PMAPSBON,
							     (long long) size_of_unmarked_range,
							     unmarked_range_first_ordno,
							     unmarked_range_wordidx,
							     unmarked_range_bitidx);
							size_of_unmarked_range
							    = 0;
						}
						marked_range_first_ordno =
						    bmap_recptr->dmappg_ordno;
						marked_range_wordidx =
						    map_wordidx;
						marked_range_bitidx =
						    word_bitidx;
						size_of_marked_range = 1;
					} else {
						/* not the first in the range */
						size_of_marked_range++;
					}
				} else {
					/* pmap agrees */
					if (size_of_marked_range != 0) {
						fsck_send_msg(fsck_PMAPSBOFF,
							      (long long) size_of_marked_range,
							      marked_range_first_ordno,
							      marked_range_wordidx,
							      marked_range_bitidx);
						size_of_marked_range = 0;
					}
					if (size_of_unmarked_range != 0) {
						/* unmarked range ended */
						fsck_send_msg(fsck_PMAPSBON,
							      (long long) size_of_unmarked_range,
							      unmarked_range_first_ordno,
							      unmarked_range_wordidx,
							      unmarked_range_bitidx);
						size_of_unmarked_range = 0;
					}
				}
			}
			/* advance to the next bit */
			bitmask = bitmask >> 1;
		}
	}
	if (size_of_marked_range != 0) {
		/* marked range ended */
		fsck_send_msg(fsck_PMAPSBOFF,(long long)size_of_marked_range,
			      marked_range_first_ordno,marked_range_wordidx,
			      marked_range_bitidx);
		size_of_marked_range = 0;
	}
	if (size_of_unmarked_range != 0) {
		/* unmarked range ended */
		fsck_send_msg(fsck_PMAPSBON,(long long)size_of_unmarked_range,
			      unmarked_range_first_ordno,unmarked_range_wordidx,
			      unmarked_range_bitidx);
		size_of_unmarked_range = 0;
	}
      bdpv_exit:
	return (bdpv_rc);
}

/*****************************************************************************
 * NAME: dmap_tree_rebuild
 *
 * FUNCTION:  Rebuild the tree in the current block map dmap page.
 *
 * PARAMETERS:
 *      root_data  - input - pointer to a variable in which the data value
 *                           stored in the root of the tree will be returned.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int dmap_tree_rebuild(int8_t * root_data)
{
	int bdsr_rc = FSCK_OK;
	struct fsck_stree_proc_parms stree_proc_parms;
	struct fsck_stree_proc_parms *prms_ptr;

	prms_ptr = &stree_proc_parms;

#define ppbt    prms_ptr->buf_tree

	ppbt = (dmtree_t *) & (bmap_recptr->dmap_bufptr->tree);

	ppbt->dmt_nleafs = prms_ptr->nleafs = LPERDMAP;
	ppbt->dmt_l2nleafs = prms_ptr->l2nleafs = L2LPERDMAP;
	ppbt->dmt_leafidx = prms_ptr->leafidx = LEAFIND;
	ppbt->dmt_height = 4;
	ppbt->dmt_budmin = prms_ptr->budmin = BUDMIN;

	prms_ptr->buf_stree = &(ppbt->dmt_stree[0]);
	prms_ptr->wsp_stree = bmap_recptr->dmap_wsp_stree;

	bdsr_rc = stree_rebuild(prms_ptr, root_data);

	return (bdsr_rc);
}

/*****************************************************************************
 * NAME: dmap_tree_verify
 *
 * FUNCTION:  Verify the tree in the current block map dmap page.
 *
 * PARAMETERS:
 *      root_data  - input - pointer to a variable in which the data value
 *                           which should be stored in the root of the tree
 *                           will be returned.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int dmap_tree_verify(int8_t * root_data)
{
	int bdsv_rc = FSCK_OK;
	int8_t tree_spec_errors = 0;
	struct fsck_stree_proc_parms stree_proc_parms;
	struct fsck_stree_proc_parms *prms_ptr;

	prms_ptr = &stree_proc_parms;

	prms_ptr->buf_tree = (dmtree_t *) & (bmap_recptr->dmap_bufptr->tree);

	if (prms_ptr->buf_tree->dmt_nleafs != LPERDMAP) {
		/* wrong number of leafs */
		bmap_recptr->dmap_other_error = -1;
		tree_spec_errors = -1;
		fsck_send_msg(fsck_BMAPBADNLF, fsck_ref_msg(fsck_dmap), bmap_recptr->dmappg_ordno);
	}
	if (prms_ptr->buf_tree->dmt_l2nleafs != L2LPERDMAP) {
		/* wrong log2(nleafs) */
		bmap_recptr->dmap_other_error = -1;
		tree_spec_errors = -1;
		fsck_send_msg(fsck_BMAPBADL2NLF, fsck_ref_msg(fsck_dmap), bmap_recptr->dmappg_ordno);
	}
	if (prms_ptr->buf_tree->dmt_leafidx != LEAFIND) {
		/* wrong 1st leaf index */
		bmap_recptr->dmap_other_error = -1;
		tree_spec_errors = -1;
		fsck_send_msg(fsck_BMAPBADLFI, fsck_ref_msg(fsck_dmap), bmap_recptr->dmappg_ordno);
	}
	if (prms_ptr->buf_tree->dmt_height != 4) {
		/* wrong stree height */
		bmap_recptr->dmap_other_error = -1;
		tree_spec_errors = -1;
		fsck_send_msg(fsck_BMAPBADHT, fsck_ref_msg(fsck_dmap), bmap_recptr->dmappg_ordno);
	}
	if (prms_ptr->buf_tree->dmt_budmin != BUDMIN) {
		/* wrong min buddy value */
		bmap_recptr->dmap_other_error = -1;
		tree_spec_errors = -1;
		fsck_send_msg(fsck_BMAPBADBMN, fsck_ref_msg(fsck_dmap), bmap_recptr->dmappg_ordno);
	}

	/*
	 * if we found errors in the fields which specify the summary
	 * tree then we won't take the time to verify the tree itself.
	 *
	 * (The errors already detected would corrupt the summary tree,
	 *  so info about the bad tree would only be noise at this point.)
	 */
	if (!tree_spec_errors) {
		/* tree specification fields are ok */
		prms_ptr->buf_stree = &(prms_ptr->buf_tree->dmt_stree[0]);
		prms_ptr->wsp_stree = bmap_recptr->dmap_wsp_stree;
		prms_ptr->nleafs = LPERDMAP;
		prms_ptr->l2nleafs = L2LPERDMAP;
		prms_ptr->leafidx = LEAFIND;
		prms_ptr->budmin = BUDMIN;
		prms_ptr->page_level = fsck_dmap;
		prms_ptr->page_ordno = bmap_recptr->dmappg_ordno;
		prms_ptr->lfval_error = &(bmap_recptr->dmap_slfv_error);
		prms_ptr->intval_error = &(bmap_recptr->dmap_slnv_error);

		bdsv_rc = stree_verify(prms_ptr, root_data);
	}
	return (bdsv_rc);
}

/*****************************************************************************
 * NAME: dmappg_rebuild
 *
 * FUNCTION: Rebuild the current dmap page.
 *
 * PARAMETERS:
 *      stree_root_data  - input - pointer to a variable in which to return
 *                                 the the data value in the root of the
 *                                 tree of the rebuilt page.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int dmappg_rebuild(int8_t * stree_root_data)
{
	int bdmpr_rc = FSCK_OK;
	uint32_t nblocks;
	uint32_t dmap_freeblks;
	uint32_t ag_num;

	/*
	 * get the page to verify into the I/O buffer
	 */
	bdmpr_rc =
	    blktbl_dmap_get(bmap_recptr->dmap_1stblk,
			    &(bmap_recptr->dmap_bufptr));

	if (bdmpr_rc == FSCK_OK) {
		/* the page is in the buffer */
		bmap_recptr->dmap_bufptr->start = bmap_recptr->dmap_1stblk;
		nblocks =
		    MIN(BPERDMAP,
			(bmap_recptr->total_blocks - bmap_recptr->dmap_1stblk));
		bmap_recptr->dmap_bufptr->nblocks = nblocks;
		bdmpr_rc = dmap_pwmap_rebuild(&dmap_freeblks);
		/*
		 * subtract out any blocks which don't really exist but are
		 * described by a dmap (phantom blocks always appear to be in use)
		 */
		agg_recptr->blocks_used_in_aggregate =
		    agg_recptr->blocks_used_in_aggregate - (BPERDMAP - nblocks);
		if (bdmpr_rc == FSCK_OK) {
			/* nothing strange during pmap rebld */
			bmap_recptr->free_blocks += dmap_freeblks;
			ag_num = bmap_recptr->dmap_1stblk >>
			    agg_recptr->log2_blksperag;
			bmap_recptr->AGFree_tbl[ag_num] += dmap_freeblks;

			if (dmap_freeblks != nblocks) {
				/* not all of them are free */
				bmap_recptr->AGActive[ag_num] = -1;
			}
			bmap_recptr->dmap_bufptr->nfree = dmap_freeblks;
			bdmpr_rc = dmap_tree_rebuild(stree_root_data);
			/*
			 * write the page back to the device
			 */
			if (bdmpr_rc == FSCK_OK) {
				bdmpr_rc =
				    blktbl_dmap_put(bmap_recptr->dmap_bufptr);
			}
		}
	}
	return (bdmpr_rc);
}

/*****************************************************************************
 * NAME: dmappg_verify
 *
 * FUNCTION: Validate the current dmap page.
 *
 * PARAMETERS:
 *      stree_root_data  - input - pointer to a variable in which to return
 *                                 the the data value which should be stored
 *                                 in the root of the tree of the page being
 *                                 validated (and may also be the one stored
 *                                 in the root of that tree)
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int dmappg_verify(int8_t * stree_root_data)
{
	int bdmpv_rc = FSCK_OK;
	uint32_t nblocks;
	uint32_t dmap_freeblks;
	uint32_t ag_num;

	/*
	 * get the page to verify into the I/O buffer
	 */
	bdmpv_rc =
	    blktbl_dmap_get(bmap_recptr->dmap_1stblk,
			    &(bmap_recptr->dmap_bufptr));

	if (bdmpv_rc == FSCK_OK) {
		/* the page is in the buffer */
		if (bmap_recptr->dmap_bufptr->start !=
		    bmap_recptr->dmap_1stblk) {
			/* bad starting block */
			bmap_recptr->dmap_other_error = -1;
			fsck_send_msg(fsck_DMAPBADSTRT, bmap_recptr->dmappg_ordno);
		}
		nblocks =
		    MIN(BPERDMAP,
			(bmap_recptr->total_blocks - bmap_recptr->dmap_1stblk));
		if (bmap_recptr->dmap_bufptr->nblocks != nblocks) {
			/* bad number of blocks */
			bmap_recptr->dmap_other_error = -1;
			fsck_send_msg(fsck_DMAPBADNBLK, bmap_recptr->dmappg_ordno);
		}
		bdmpv_rc = dmap_pmap_verify(&dmap_freeblks);
		/*
		 * subtract out any blocks which don't really exist but are
		 * described by a dmap (phantom blocks always appear to be in use)
		 */
		agg_recptr->blocks_used_in_aggregate =
		    agg_recptr->blocks_used_in_aggregate - (BPERDMAP - nblocks);
		if (bdmpv_rc == FSCK_OK) {
			/* nothing strange during pmap ver */
			bmap_recptr->free_blocks += dmap_freeblks;
			ag_num = bmap_recptr->dmap_1stblk >>
			    agg_recptr->log2_blksperag;
			bmap_recptr->AGFree_tbl[ag_num] += dmap_freeblks;
			if (dmap_freeblks != nblocks) {
				/* not all of them are free */
				bmap_recptr->AGActive[ag_num] = -1;
			}
			if (bmap_recptr->dmap_bufptr->nfree != dmap_freeblks) {
				/* bad number of free blocks */
				bmap_recptr->dmap_other_error = -1;
				fsck_send_msg(fsck_DMAPBADNFREE, bmap_recptr->dmappg_ordno);
			}
			bdmpv_rc = dmap_tree_verify(stree_root_data);
		}
	}
	return (bdmpv_rc);
}

/*****************************************************************************
 * NAME: init_bmap_info
 *
 * FUNCTION:  Initialize the bmap fsck global data area, pointed to by
 *            bmap_recptr, used to control JFS bmap processing.
 *
 * PARAMETERS:  none
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int init_bmap_info()
{
	int bii_rc = FSCK_OK;
	unsigned agidx;

	memset(bmap_recptr, 0, sizeof (struct fsck_bmap_record));

	memcpy((void *) &(bmap_recptr->eyecatcher), (void *) "bmaprecd", 8);
	memcpy((void *) &(bmap_recptr->bmpctlinf_eyecatcher),
	       (void *) "bmap ctl", 8);
	memcpy((void *) &(bmap_recptr->AGinf_eyecatcher), (void *) "AG info ",
	       8);
	memcpy((void *) &(bmap_recptr->dmapinf_eyecatcher), (void *) "dmapinfo",
	       8);
	memcpy((void *) &(bmap_recptr->L0inf_eyecatcher), (void *) "L0 info ",
	       8);
	memcpy((void *) &(bmap_recptr->L1inf_eyecatcher), (void *) "L1 info ",
	       8);
	memcpy((void *) &(bmap_recptr->L2inf_eyecatcher), (void *) "L2 info ",
	       8);

	bmap_recptr->bmpctl_bufptr =
	    (struct dbmap *) agg_recptr->mapctl_buf_ptr;
	bmap_recptr->bmpctl_agg_fsblk_offset = BMAP_OFF / sb_ptr->s_bsize;

	bmap_recptr->AGFree_tbl = &(agg_recptr->blkmap_wsp.AG_free[0]);
	bmap_recptr->dmap_wsp_stree =
	    &(agg_recptr->blkmap_wsp.dmap_wsp_tree[0]);
	bmap_recptr->dmap_wsp_sleafs =
	    &(agg_recptr->blkmap_wsp.dmap_wsp_leafs[0]);
	bmap_recptr->L0_wsp_stree = &(agg_recptr->blkmap_wsp.L0_wsp_tree[0]);
	bmap_recptr->L0_wsp_sleafs = &(agg_recptr->blkmap_wsp.L0_wsp_leafs[0]);
	bmap_recptr->L0_bufptr =
	    (struct dmapctl *) &(agg_recptr->bmaplv_buf_ptr);
	bmap_recptr->L1_wsp_stree = &(agg_recptr->blkmap_wsp.L1_wsp_tree[0]);
	bmap_recptr->L1_wsp_sleafs = &(agg_recptr->blkmap_wsp.L1_wsp_leafs[0]);
	bmap_recptr->L1_bufptr =
	    (struct dmapctl *) &(agg_recptr->bmaplv_buf_ptr);
	bmap_recptr->L2_wsp_stree = &(agg_recptr->blkmap_wsp.L2_wsp_tree[0]);
	bmap_recptr->L2_wsp_sleafs = &(agg_recptr->blkmap_wsp.L2_wsp_leafs[0]);
	bmap_recptr->L2_bufptr =
	    (struct dmapctl *) &(agg_recptr->bmaplv_buf_ptr);

	bmap_recptr->total_blocks =
	    sb_ptr->s_size * sb_ptr->s_pbsize / sb_ptr->s_bsize;
	bmap_recptr->dmappg_count =
	    (bmap_recptr->total_blocks + BPERDMAP - 1) / BPERDMAP;
	bmap_recptr->L0pg_count =
	    (bmap_recptr->dmappg_count + LPERCTL - 1) / LPERCTL;
	if (bmap_recptr->L0pg_count > 1) {
		bmap_recptr->L1pg_count =
		    (bmap_recptr->L0pg_count + LPERCTL - 1) / LPERCTL;
		if (bmap_recptr->L1pg_count > 1) {
			bmap_recptr->L2pg_count =
			    (bmap_recptr->L1pg_count + LPERCTL - 1) / LPERCTL;
		} else {
			bmap_recptr->L2pg_count = 0;
		}
	} else {
		bmap_recptr->L1pg_count = 0;
	}

	bmap_recptr->free_blocks = 0;
	bmap_recptr->ctl_fctl_error = 0;
	bmap_recptr->ctl_other_error = 0;

	for (agidx = 0; (agidx < MAXAG); agidx++) {
		bmap_recptr->AGActive[agidx] = 0;
	}

	bmap_recptr->dmappg_ordno = bmap_recptr->L0pg_ordno = 0;
	bmap_recptr->L1pg_ordno = bmap_recptr->L2pg_1stblk = 0;

	bmap_recptr->dmappg_idx = bmap_recptr->L0pg_idx = 0;
	bmap_recptr->L1pg_idx = 0;

	bmap_recptr->dmap_1stblk = bmap_recptr->L0pg_1stblk = 0;
	bmap_recptr->L1pg_1stblk = bmap_recptr->L2pg_1stblk = 0;

	bmap_recptr->dmap_pmap_error = 0;

	bmap_recptr->dmap_slfv_error = bmap_recptr->L0pg_slfv_error = 0;
	bmap_recptr->L1pg_slfv_error = bmap_recptr->L2pg_slfv_error = 0;

	bmap_recptr->dmap_slnv_error = bmap_recptr->L0pg_slnv_error = 0;
	bmap_recptr->L1pg_slnv_error = bmap_recptr->L2pg_slnv_error = 0;

	bmap_recptr->dmap_other_error = bmap_recptr->L0pg_other_error = 0;
	bmap_recptr->L1pg_other_error = bmap_recptr->L2pg_other_error = 0;

	return (bii_rc);
}

/*****************************************************************************
 * NAME: Ln_tree_rebuild
 *
 * FUNCTION:  Rebuild the tree in the current summary page.
 *
 * PARAMETERS:
 *      level         - input - Summary level of the bmap summary page
 *                              containing the tree to rebuild
 *      first_blk     - input - First aggregate block described by the bmap
 *                              summary page containing the tree to rebuild
 *      addr_buf_ptr  - input - pointer to a variable in which to return the
 *                              address of the buffer in which the page has
 *                              been rebuilt (and then written to the
 *                              aggregate)
 *      root_data     - input - pointer to a variable in which to return the
 *                              data value in the root of the rebuilt tree.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int Ln_tree_rebuild(int level, int64_t first_blk, struct dmapctl **addr_buf_ptr,
		    int8_t * root_data)
{
	int blsr_rc = FSCK_OK;
	struct fsck_stree_proc_parms stree_proc_parms;
	struct fsck_stree_proc_parms *prms_ptr;

	/*
	 * get the page to verify into the I/O buffer
	 */
	blsr_rc = blktbl_Ln_page_get(level, first_blk, addr_buf_ptr);

	if (blsr_rc == FSCK_OK) {
		/* the page is in the buffer */
		prms_ptr = &stree_proc_parms;
		/* these are just big trees */
		prms_ptr->buf_tree = (dmtree_t *) * addr_buf_ptr;

		switch (level) {
		case 0:{
				prms_ptr->wsp_stree = bmap_recptr->L0_wsp_stree;
				break;
			}
		case 1:{
				prms_ptr->wsp_stree = bmap_recptr->L1_wsp_stree;
				break;
			}
		default:{
				prms_ptr->wsp_stree = bmap_recptr->L2_wsp_stree;
				break;
			}
		}

		prms_ptr->buf_tree->dmt_nleafs = prms_ptr->nleafs = LPERCTL;
		prms_ptr->buf_tree->dmt_l2nleafs = prms_ptr->l2nleafs =
		    L2LPERCTL;
		prms_ptr->buf_tree->dmt_leafidx = prms_ptr->leafidx =
		    CTLLEAFIND;
		prms_ptr->buf_tree->dmt_height = 5;
		prms_ptr->buf_tree->dmt_budmin = L2BPERDMAP + level * L2LPERCTL;
		prms_ptr->budmin = prms_ptr->buf_tree->dmt_budmin;

		prms_ptr->buf_stree = &(prms_ptr->buf_tree->dmt_stree[0]);

		blsr_rc = stree_rebuild(prms_ptr, root_data);

		/*
		 * put the page back into the file
		 */
		if (blsr_rc == FSCK_OK) {
			blsr_rc = blktbl_Ln_page_put(*addr_buf_ptr);
		}
	}
	return (blsr_rc);
}

/*****************************************************************************
 * NAME: Ln_tree_verify
 *
 * FUNCTION:  Verify the tree in the current summary page.
 *
 * PARAMETERS:
 *      level         - input - Summary level of the bmap summary page
 *                              containing the tree to verify
 *      first_blk     - input - First aggregate block described by the bmap
 *                              summary page containing the tree to verify
 *      addr_buf_ptr  - input - pointer to a variable in which to return the
 *                              address of the buffer into which the page has
 *                              been read so that its contents can be verified
 *      root_data     - input - pointer to a variable in which to return the
 *                              data value which should be in the root of the
 *                              tree....and may actually be stored there.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int Ln_tree_verify(int level, int64_t first_blk, struct dmapctl **addr_buf_ptr,
		   int8_t * root_data)
{
	int blsv_rc = FSCK_OK;
	int8_t *other_errors;
	int8_t tree_spec_errors = 0;
	struct fsck_stree_proc_parms stree_proc_parms;
	struct fsck_stree_proc_parms *prms_ptr;

	/*
	 * get the page to verify into the I/O buffer
	 */
	blsv_rc = blktbl_Ln_page_get(level, first_blk, addr_buf_ptr);

	if (blsv_rc == FSCK_OK) {
		/* the page is in the buffer */
		prms_ptr = &stree_proc_parms;
		/* these are just big trees */
		prms_ptr->buf_tree = (dmtree_t *) * addr_buf_ptr;

		switch (level) {
		case 0:{
				prms_ptr->wsp_stree = bmap_recptr->L0_wsp_stree;
				prms_ptr->page_ordno = bmap_recptr->L0pg_ordno;
				prms_ptr->lfval_error =
				    &(bmap_recptr->L0pg_slfv_error);
				prms_ptr->intval_error =
				    &(bmap_recptr->L0pg_slnv_error);
				prms_ptr->page_level = fsck_L0;
				other_errors = &(bmap_recptr->L0pg_other_error);
				break;
			}
		case 1:{
				prms_ptr->wsp_stree = bmap_recptr->L1_wsp_stree;
				prms_ptr->page_ordno = bmap_recptr->L1pg_ordno;
				prms_ptr->lfval_error =
				    &(bmap_recptr->L1pg_slfv_error);
				prms_ptr->intval_error =
				    &(bmap_recptr->L1pg_slnv_error);
				prms_ptr->page_level = fsck_L1;
				other_errors = &(bmap_recptr->L1pg_other_error);
				break;
			}
		default:{
				prms_ptr->wsp_stree = bmap_recptr->L2_wsp_stree;
				prms_ptr->page_ordno = 0;
				prms_ptr->lfval_error =
				    &(bmap_recptr->L2pg_slfv_error);
				prms_ptr->intval_error =
				    &(bmap_recptr->L2pg_slnv_error);
				prms_ptr->page_level = fsck_L2;
				other_errors = &(bmap_recptr->L2pg_other_error);
				break;
			}
		}

		if (prms_ptr->buf_tree->dmt_nleafs != LPERCTL) {
			/* wrong number of leafs */
			*other_errors = -1;
			tree_spec_errors = -1;
			fsck_send_msg(fsck_BMAPBADNLF, fsck_ref_msg(prms_ptr->page_level),
				      prms_ptr->page_ordno);
		}
		if (prms_ptr->buf_tree->dmt_l2nleafs != L2LPERCTL) {
			/* wrong log2(nleafs) */
			*other_errors = -1;
			tree_spec_errors = -1;
			fsck_send_msg(fsck_BMAPBADL2NLF, fsck_ref_msg(prms_ptr->page_level),
				      prms_ptr->page_ordno);
		}
		if (prms_ptr->buf_tree->dmt_leafidx != CTLLEAFIND) {
			/* wrong 1st leaf index */
			*other_errors = -1;
			tree_spec_errors = -1;
			fsck_send_msg(fsck_BMAPBADLFI, fsck_ref_msg(prms_ptr->page_level),
				      prms_ptr->page_ordno);
		}
		if (prms_ptr->buf_tree->dmt_height != 5) {
			/* wrong stree height */
			*other_errors = -1;
			tree_spec_errors = -1;
			fsck_send_msg(fsck_BMAPBADHT, fsck_ref_msg(prms_ptr->page_level),
				      prms_ptr->page_ordno);
		}
		if (prms_ptr->buf_tree->dmt_budmin != (L2BPERDMAP + level * L2LPERCTL)) {
			/* wrong min buddy value */
			*other_errors = -1;
			tree_spec_errors = -1;
			fsck_send_msg(fsck_BMAPBADBMN, fsck_ref_msg(prms_ptr->page_level),
				      prms_ptr->page_ordno);
		}

		/*
		 * if we found errors in the fields which specify the summary
		 * tree then we won't take the time to verify the tree itself.
		 *
		 * (The errors already detected would corrupt the summary tree,
		 *  so info about the bad tree would only be noise at this point.)
		 */
		if (!tree_spec_errors) {
			/* tree specification fields are ok */
			prms_ptr->buf_stree =
			    &(prms_ptr->buf_tree->dmt_stree[0]);
			prms_ptr->nleafs = LPERCTL;
			prms_ptr->budmin = (L2BPERDMAP + level * L2LPERCTL);
			prms_ptr->l2nleafs = L2LPERCTL;
			prms_ptr->leafidx = CTLLEAFIND;

			blsv_rc = stree_verify(prms_ptr, root_data);

		}
	}
	return (blsv_rc);
}

/*****************************************************************************
 * NAME: rebuild_blkall_map
 *
 * FUNCTION:  Rebuild the JFS Aggregate Block Map in the aggregate.
 *
 * PARAMETERS:  none
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int rebuild_blkall_map()
{
	int rbam_rc = FSCK_OK;
	int8_t sumtree_root_data;
	uint32_t leafidx;

#define MAXIDX (LPERCTL - 1)

	rbam_rc = init_bmap_info();

	/*
	 * since the dmap I/O buffer is really the same storage as the
	 * IAG I/O buffer, flush out any pending writes that may remain
	 * from IAG processing.
	 */
	rbam_rc = iags_flush();

	/*
	 * rebuild the dmap pages.  Rebuild each L0 and L1 page
	 * if and when the information for is complete.
	 */
	while ((rbam_rc == FSCK_OK)
	       && (bmap_recptr->dmappg_ordno < bmap_recptr->dmappg_count)) {

		rbam_rc = dmappg_rebuild(&sumtree_root_data);
		if (rbam_rc != FSCK_OK)
			continue;

		/*
		 * the data in the dmap summary tree root goes into a leaf of
		 * the current L0 page
		 */
		bmap_recptr->L0_wsp_sleafs[bmap_recptr->dmappg_idx] =
		    sumtree_root_data;
		/* move to next dmap page */
		bmap_recptr->dmappg_ordno++;
		bmap_recptr->dmap_1stblk += BPERDMAP;
		if (bmap_recptr->dmappg_idx < MAXIDX) {
			/* still gathering info about this L0 page */
			bmap_recptr->dmappg_idx++;
			continue;
		}

		/* we have all the info needed for the current L0 page */
		bmap_recptr->dmappg_idx = 0;
		rbam_rc =
		    Ln_tree_rebuild(0, bmap_recptr->L0pg_1stblk,
				    &(bmap_recptr->L0_bufptr),
				    &sumtree_root_data);
		if (rbam_rc == FSCK_OK) {
			/*
			 * the data in the L0 summary tree root goes into
			 * a leaf of the current L1 page
			 */
			bmap_recptr->L1_wsp_sleafs[bmap_recptr->
						   L0pg_idx] =
			    sumtree_root_data;
			/* move to the next L0 page */
			bmap_recptr->L0pg_ordno++;
			bmap_recptr->L0pg_1stblk =
			    bmap_recptr->dmap_1stblk;
			if (bmap_recptr->L0pg_idx < MAXIDX) {
				/* still gathering info about this L1 page */
				bmap_recptr->L0pg_idx++;
			} else {
				/* we have all the info needed for the current L1 page */
				bmap_recptr->L0pg_idx = 0;
				rbam_rc =
				    Ln_tree_rebuild(1,
						    bmap_recptr->
						    L1pg_1stblk,
						    &
						    (bmap_recptr->
						     L1_bufptr),
						    &sumtree_root_data);
				if (rbam_rc == FSCK_OK) {
					/*
					 * the data in the L1 summary tree root goes into
					 * a leaf of the current L2 page
					 */
					bmap_recptr->
					    L2_wsp_sleafs
					    [bmap_recptr->
					     L1pg_idx] =
					    sumtree_root_data;
					/* move to the next L1 page */
					bmap_recptr->L1pg_ordno++;
					bmap_recptr->
					    L1pg_1stblk =
					    bmap_recptr->
					    dmap_1stblk;
					/*
					 * note that there is always AT MOST a single L2 page
					 */
					bmap_recptr->L1pg_idx++;
				}
			}
		}
	}

	/*
	 * finish up the partial pages
	 */

	if (rbam_rc == FSCK_OK) {
		if (bmap_recptr->dmappg_idx != 0) {
			/* there's a partial L0 page */
			for (leafidx = bmap_recptr->dmappg_idx;
			     (leafidx <= MAXIDX); leafidx++) {
				bmap_recptr->L0_wsp_sleafs[leafidx] = -1;
			}
			rbam_rc = Ln_tree_rebuild(0, bmap_recptr->L0pg_1stblk,
						  &(bmap_recptr->L0_bufptr),
						  &sumtree_root_data);
			if (rbam_rc == FSCK_OK) {
				/*
				 * the data in the L0 summary tree root goes into
				 * a leaf of the current L1 page
				 */
				bmap_recptr->L1_wsp_sleafs[bmap_recptr->
							   L0pg_idx] =
				    sumtree_root_data;
				bmap_recptr->L0pg_idx++;
			}
		}
	}
	if (rbam_rc == FSCK_OK) {
		if ((bmap_recptr->L1pg_count > 0) && (bmap_recptr->L0pg_idx != 0)) {
			/* there's enough data for an L1 level, and there's a partial L1 page */
			for (leafidx = bmap_recptr->L0pg_idx;
			     (leafidx <= MAXIDX); leafidx++) {
				bmap_recptr->L1_wsp_sleafs[leafidx] = -1;
			}
			rbam_rc = Ln_tree_rebuild(1, bmap_recptr->L1pg_1stblk,
						  &(bmap_recptr->L1_bufptr),
						  &sumtree_root_data);
			if (rbam_rc == FSCK_OK) {
				/*
				 * the data in the L0 summary tree root goes into
				 * a leaf of the current L1 page
				 */
				bmap_recptr->L2_wsp_sleafs[bmap_recptr->
							   L1pg_idx] =
				    sumtree_root_data;
				bmap_recptr->L1pg_idx++;
			}
		}
	}
	if (rbam_rc == FSCK_OK) {
		if ((bmap_recptr->L2pg_count > 0) && (bmap_recptr->L1pg_idx != 0)) {
			/* there's enough data for an L2 level, and there's a partial L2 page */
			for (leafidx = bmap_recptr->L1pg_idx;
			     (leafidx <= MAXIDX); leafidx++) {
				bmap_recptr->L2_wsp_sleafs[leafidx] = -1;
			}
			rbam_rc = Ln_tree_rebuild(2, bmap_recptr->L2pg_1stblk,
						  &(bmap_recptr->L2_bufptr),
						  &sumtree_root_data);
		}
	}
	/*
	 * Now go verify the Block Allocation Map Control page
	 */
	if (rbam_rc == FSCK_OK) {
		rbam_rc = ctlpage_rebuild(sumtree_root_data);
	}
	return (rbam_rc);
}

/*****************************************************************************
 * NAME: stree_rebuild
 *
 * FUNCTION:  Rebuild the specified summary tree.
 *
 * PARAMETERS:
 *      prms_ptr   - input - pointer to a data area describing the tree
 *                           to rebuild and containing the dmap which the
 *                           tree summarizes.
 *      root_data  - input - pointer to a variable in which to return the
 *                           data value stored in the root node of the tree
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int stree_rebuild(struct fsck_stree_proc_parms *prms_ptr, int8_t * root_data)
{
	int bsr_rc = FSCK_OK;
	uint32_t node_idx, last_leaf_idx;

	/*
	 * copy the leaf data into the buffer
	 */
	last_leaf_idx = prms_ptr->leafidx + prms_ptr->nleafs - 1;
	for (node_idx = prms_ptr->leafidx; (node_idx <= last_leaf_idx);
	     node_idx++) {
		prms_ptr->buf_stree[node_idx] = prms_ptr->wsp_stree[node_idx];
	}

	/*
	 * build the summary tree from the "raw" leaf values
	 */
	*root_data =
	    ujfs_adjtree(prms_ptr->buf_stree, prms_ptr->l2nleafs,
			 prms_ptr->budmin);

	return (bsr_rc);
}

/*****************************************************************************
 * NAME: stree_verify
 *
 * FUNCTION:  Verify the specified summary tree.
 *
 * PARAMETERS:
 *      prms_ptr   - input - pointer to a data area describing the tree
 *                           to verify and containing the dmap which the
 *                           tree summarizes.
 *      root_data  - input - pointer to a variable in which to return the
 *                           data value which should be in the root node
 *                           of the tree (and may in fact be in the root
 *                           node of the tree)
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int stree_verify(struct fsck_stree_proc_parms *prms_ptr, int8_t * root_data)
{
	int bsv_rc = FSCK_OK;
	uint32_t node_idx, last_leaf_idx;

	/*
	 * build the summary tree from the "raw" leaf values
	 */
	*root_data =
	    ujfs_adjtree(prms_ptr->wsp_stree, prms_ptr->l2nleafs,
			 prms_ptr->budmin);

	/*
	 * Now see if the tree in the buffer matches the one we just
	 * built in the workspace.
	 *
	 * We distinguish between incorrect internal nodes and incorrect
	 * leaf nodes because they can be symptoms of different problems.
	 */
	for (node_idx = 0; (node_idx < prms_ptr->leafidx); node_idx++) {
		if (prms_ptr->buf_stree[node_idx] != prms_ptr->wsp_stree[node_idx]) {
			/* they don't match! */
			*(prms_ptr->intval_error) = -1;
			fsck_send_msg(fsck_BMAPBADLNV, node_idx, fsck_ref_msg(prms_ptr->page_level),
				      prms_ptr->page_ordno);
		}
	}

	last_leaf_idx = prms_ptr->leafidx + prms_ptr->nleafs - 1;
	for (node_idx = prms_ptr->leafidx; (node_idx <= last_leaf_idx);
	     node_idx++) {
		if (prms_ptr->buf_stree[node_idx] != prms_ptr->wsp_stree[node_idx]) {
			/* they don't match! */
			*(prms_ptr->lfval_error) = -1;
			fsck_send_msg(fsck_BMAPBADLFV, node_idx, fsck_ref_msg(prms_ptr->page_level),
				      prms_ptr->page_ordno);
		}
	}

	return (bsv_rc);
}

/*****************************************************************************
 * NAME: verify_blkall_map
 *
 * FUNCTION:  Validate the JFS Aggregate Block Map for the aggregate.
 *
 * PARAMETERS:  none
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int verify_blkall_map()
{
	int vbam_rc = FSCK_OK;
	int8_t sumtree_root_data;
	uint32_t leafidx;

#define MAXIDX (LPERCTL - 1)

	vbam_rc = init_bmap_info();

	/*
	 * since the dmap I/O buffer is really the same storage as the
	 * IAG I/O buffer, flush out any pending writes that may remain
	 * from IAG processing.
	 */
	vbam_rc = iags_flush();

	/*
	 * Verify the dmap pages.  Verify each L0 and L1 page
	 * if and when the information for is complete.
	 */
	while ((vbam_rc == FSCK_OK)
	       && (bmap_recptr->dmappg_ordno < bmap_recptr->dmappg_count)) {

		vbam_rc = dmappg_verify(&sumtree_root_data);
		if (vbam_rc != FSCK_OK)
			continue;

		/*
		 * the data in the dmap summary tree root goes into a leaf of
		 * the current L0 page
		 */
		bmap_recptr->L0_wsp_sleafs[bmap_recptr->dmappg_idx] =
		    sumtree_root_data;
		/* move to next dmap page */
		bmap_recptr->dmappg_ordno++;
		bmap_recptr->dmap_1stblk += BPERDMAP;
                if (bmap_recptr->dmappg_idx < MAXIDX) {
			/* still gathering info about this L0 page */
			bmap_recptr->dmappg_idx++;
			continue;
		}
		/* we have all the info needed for the current L0 page */
		bmap_recptr->dmappg_idx = 0;
		vbam_rc = Ln_tree_verify(0, bmap_recptr->L0pg_1stblk,
					 &(bmap_recptr->L0_bufptr),
					 &sumtree_root_data);

		if (vbam_rc != FSCK_OK)
			continue;

		/*
		 * the data in the L0 summary tree root goes into
		 * a leaf of the current L1 page
		 */
		bmap_recptr->L1_wsp_sleafs[bmap_recptr->L0pg_idx] =
		    sumtree_root_data;
		/* move to the next L0 page */
		bmap_recptr->L0pg_ordno++;
		bmap_recptr->L0pg_1stblk = bmap_recptr->dmap_1stblk;
		if (bmap_recptr->L0pg_idx < MAXIDX) {
			/* still gathering info about this L1 page */
			bmap_recptr->L0pg_idx++;
			continue;
		}
		/* we have all the info needed for the current L1 page */
		bmap_recptr->L0pg_idx = 0;
		vbam_rc = Ln_tree_verify(1, bmap_recptr->L1pg_1stblk,
					 &(bmap_recptr->L1_bufptr),
					 &sumtree_root_data);
		if (vbam_rc == FSCK_OK) {
			/*
			 * the data in the L1 summary tree root goes into
			 * a leaf of the current L2 page
			 */
			bmap_recptr->L2_wsp_sleafs[bmap_recptr->L1pg_idx] =
			    sumtree_root_data;
			/* move to the next L1 page */
			bmap_recptr->L1pg_ordno++;
			bmap_recptr->L1pg_1stblk = bmap_recptr->dmap_1stblk;
			/*
			 * note that there is always AT MOST a single L2 page
			 */
			bmap_recptr->L1pg_idx++;
		}
	}
	/*
	 * finish up the partial pages
	 */
	if (vbam_rc == FSCK_OK) {
		if (bmap_recptr->dmappg_idx != 0) {
			/* there's a partial L0 page */
			for (leafidx = bmap_recptr->dmappg_idx;
			     (leafidx <= MAXIDX); leafidx++) {
				bmap_recptr->L0_wsp_sleafs[leafidx] = -1;
			}
			vbam_rc = Ln_tree_verify(0, bmap_recptr->L0pg_1stblk,
						 &(bmap_recptr->L0_bufptr),
						 &sumtree_root_data);
			if (vbam_rc == FSCK_OK) {
				/*
				 * the data in the L0 summary tree root goes into
				 * a leaf of the current L1 page
				 */
				bmap_recptr->L1_wsp_sleafs[bmap_recptr->
							   L0pg_idx] =
				    sumtree_root_data;
				bmap_recptr->L0pg_idx++;
			}
		}
	}
	if (vbam_rc == FSCK_OK) {
		if ((bmap_recptr->L1pg_count > 0) && (bmap_recptr->L0pg_idx != 0)) {
			/* there's enough data for an L1 level, and there's a partial L1 page */
			for (leafidx = bmap_recptr->L0pg_idx;
			     (leafidx <= MAXIDX); leafidx++) {
				bmap_recptr->L1_wsp_sleafs[leafidx] = -1;
			}	/* end for leafidx */

			vbam_rc = Ln_tree_verify(1, bmap_recptr->L1pg_1stblk,
						 &(bmap_recptr->L1_bufptr),
						 &sumtree_root_data);
			if (vbam_rc == FSCK_OK) {
				/*
				 * the data in the L0 summary tree root goes into
				 * a leaf of the current L1 page
				 */
				bmap_recptr->L2_wsp_sleafs[bmap_recptr->
							   L1pg_idx] =
				    sumtree_root_data;
				bmap_recptr->L1pg_idx++;
			}
		}
	}
	if (vbam_rc == FSCK_OK) {
		if ((bmap_recptr->L2pg_count > 0) && (bmap_recptr->L1pg_idx != 0)) {
			/* there's enough data for an L2 level, and there's a partial L2 page */
			for (leafidx = bmap_recptr->L1pg_idx;
			     (leafidx <= MAXIDX); leafidx++) {
				bmap_recptr->L2_wsp_sleafs[leafidx] = -1;
			}

			vbam_rc = Ln_tree_verify(2, bmap_recptr->L2pg_1stblk,
						 &(bmap_recptr->L2_bufptr),
						 &sumtree_root_data);
		}
	}
	/*
	 * Now go verify the Block Allocation Map Control page
	 */
	if (vbam_rc == FSCK_OK) {
		vbam_rc = ctlpage_verify(sumtree_root_data);
	}
	/*
	 * issue summary messages about the Block Allocation Map validation
	 */
	if (vbam_rc == FSCK_OK) {
		vbam_rc = verify_blkall_summary_msgs();
	}
	return (vbam_rc);
}

/*****************************************************************************
 * NAME: verify_blkall_summary_msgs
 *
 * FUNCTION: Issue summary messages with the results of JFS Aggregate Block
 *           Map validation.
 *
 * PARAMETERS:  none
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int verify_blkall_summary_msgs()
{
	int vbsm_rc = FSCK_OK;

	if (bmap_recptr->dmap_pmap_error) {
		fsck_send_msg(fsck_BADDMAPPMAPS);
	}
	if (bmap_recptr->dmap_slfv_error) {
		fsck_send_msg(fsck_BADBMAPSLFV, fsck_ref_msg(fsck_dmap));
	}
	if (bmap_recptr->dmap_slnv_error) {
		fsck_send_msg(fsck_BADBMAPSLNV, fsck_ref_msg(fsck_dmap));
	}
	if (bmap_recptr->dmap_other_error) {
		fsck_send_msg(fsck_BADBMAPSOTHER, fsck_ref_msg(fsck_dmap));
	}
	if (bmap_recptr->L0pg_slfv_error) {
		fsck_send_msg(fsck_BADBMAPSLFV, fsck_ref_msg(fsck_L0));
	}
	if (bmap_recptr->L0pg_slnv_error) {
		fsck_send_msg(fsck_BADBMAPSLNV, fsck_ref_msg(fsck_L0));
	}
	if (bmap_recptr->L0pg_other_error) {
		fsck_send_msg(fsck_BADBMAPSOTHER, fsck_ref_msg(fsck_L0));
	}
	if (bmap_recptr->L1pg_slfv_error) {
		fsck_send_msg(fsck_BADBMAPSLFV, fsck_ref_msg(fsck_L1));
	}
	if (bmap_recptr->L1pg_slnv_error) {
		fsck_send_msg(fsck_BADBMAPSLNV, fsck_ref_msg(fsck_L1));
	}
	if (bmap_recptr->L1pg_other_error) {
		fsck_send_msg(fsck_BADBMAPSOTHER, fsck_ref_msg(fsck_L1));
	}
	if (bmap_recptr->L2pg_slfv_error) {
		fsck_send_msg(fsck_BADBMAPSLFV, fsck_ref_msg(fsck_L2));
	}
	if (bmap_recptr->L2pg_slnv_error) {
		fsck_send_msg(fsck_BADBMAPSLNV, fsck_ref_msg(fsck_L2));
	}
	if (bmap_recptr->L2pg_other_error) {
		fsck_send_msg(fsck_BADBMAPSOTHER, fsck_ref_msg(fsck_L2));
	}
	if (bmap_recptr->ctl_fctl_error) {
		fsck_send_msg(fsck_BADBMAPCAGFCL);
	}
	if (bmap_recptr->ctl_other_error) {
		fsck_send_msg(fsck_BADBMAPCOTH);
	}
	if (bmap_recptr->dmap_pmap_error || bmap_recptr->dmap_slfv_error ||
	    bmap_recptr->dmap_slnv_error || bmap_recptr->dmap_other_error ||
	    bmap_recptr->L0pg_slfv_error || bmap_recptr->L0pg_slnv_error ||
	    bmap_recptr->L0pg_other_error || bmap_recptr->L1pg_slfv_error ||
	    bmap_recptr->L1pg_slnv_error || bmap_recptr->L1pg_other_error ||
	    bmap_recptr->L2pg_slfv_error || bmap_recptr->L2pg_slnv_error ||
	    bmap_recptr->L2pg_other_error) {
		agg_recptr->ag_dirty = 1;
		fsck_send_msg(fsck_BADBLKALLOC);
	}
	if (bmap_recptr->ctl_fctl_error || bmap_recptr->ctl_other_error) {
		agg_recptr->ag_dirty = 1;
		fsck_send_msg(fsck_BADBLKALLOCCTL);
	}
	return (vbsm_rc);
}
