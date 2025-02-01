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
#include <time.h>
/* defines and includes common among the fsck.jfs modules */
#include "xfsckint.h"
#include "devices.h"
#include "diskmap.h"
#include "jfs_byteorder.h"
#include "message.h"
#include "super.h"
#include "utilsubs.h"

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
  * For message processing
  *
  *      defined in xchkdsk.c
  */
extern char *Vol_Label;

/* VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV
 *
 * The following are internal to this file
 *
 */

int backout_ait_part1(int);

int backout_valid_agg_inode(int, uint32_t, struct fsck_ino_msg_info *);

int first_ref_check_other_ait(void);

int record_ait_part1_again(int);

int record_other_ait(void);

int rootdir_tree_bad(struct dinode *, int *);

int validate_super(int);

int validate_super_2ndaryAI(int);

int verify_agg_fileset_inode(struct dinode *, uint32_t, int,
			     struct fsck_ino_msg_info *);

int verify_ait_inode(struct dinode *, int, struct fsck_ino_msg_info *);

int verify_ait_part1(int);

int verify_ait_part2(int);

int verify_badblk_inode(struct dinode *, int, struct fsck_ino_msg_info *);

int verify_bmap_inode(struct dinode *, int, struct fsck_ino_msg_info *);

int verify_fs_super_ext(struct dinode *, struct fsck_ino_msg_info *, int *);

int verify_log_inode(struct dinode *, int, struct fsck_ino_msg_info *);

int verify_metadata_data(struct dinode *, uint32_t, struct fsck_inode_record *,
			 struct fsck_ino_msg_info *);

int verify_repair_fs_rootdir(struct dinode *, struct fsck_ino_msg_info *,
			     int *);

/* VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV  */

/*****************************************************************************
 * NAME: agg_clean_or_dirty
 *
 * FUNCTION:  Compare the superblock state field (s_state) with fsck's
 *            conclusions about the current state (clean | dirty) of
 *            the aggregate.  If write access, attempt to update the
 *            state field if the superblock is incorrect.  If read-only,
 *            notify the caller if the superblock is incorrect.
 *
 * PARAMETERS:  none
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int agg_clean_or_dirty()
{
	int acod_rc = FSCK_OK;

	if (!agg_recptr->ag_dirty) {
		/* aggregate is actually clean now */
		/* announce this happy news */
		fsck_send_msg(fsck_AGGCLN);

		if (agg_recptr->processing_readonly) {
			/* don't have write access */
			if ((sb_ptr->s_state & FM_DIRTY) == FM_DIRTY) {
				/* but isn't marked clean */
				fsck_send_msg(fsck_AGGCLNNOTDRTY);
			}
		} else {
			/* do have write access to the aggregate */
			fsck_send_msg(fsck_ALLFXD);
			sb_ptr->s_state = FM_CLEAN;
			acod_rc = replicate_superblock();
			if (acod_rc == FSCK_OK) {
				fsck_send_msg(fsck_AGGMRKDCLN);
			}
		}
	} else {
		/* aggregate is actually dirty now */
		fsck_send_msg(fsck_AGGDRTY);

		if ((sb_ptr->s_state & FM_DIRTY) != FM_DIRTY) {
			/* but isn't marked dirty */
			if (agg_recptr->processing_readonly) {
				/* don't have write access */
				fsck_send_msg(fsck_AGGDRTYNOTCLN, Vol_Label);
			} else {
				/* do have write access to the aggregate */
				/*
				 * in keeping with the policy of protecting the system
				 * from a potential panic due to a dirty file system,
				 * if we have write access we'll mark the file system
				 * dirty without asking permission.
				 */
				sb_ptr->s_state = FM_DIRTY;
				acod_rc = replicate_superblock();
				if (acod_rc == FSCK_OK) {
					fsck_send_msg(fsck_AGGMRKDDRTY);
				}
			}
		}
	}

	return (acod_rc);
}

/*****************************************************************************
 * NAME: backout_ait_part1
 *
 * FUNCTION:  Unrecord, in the fsck workspace block map, all storage allocated
 *            to inodes in part 1 (inodes 0 through 15) of the specified
 *            (primary or secondary) aggregate inode table.
 *
 * PARAMETERS:
 *      which_ait  - input - the Aggregate Inode Table on which to perform
 *                           the function.  { fsck_primary | fsck_secondary }
 *
 * NOTES:  o The caller to this routine must ensure that the
 *           calls made by backout_ait_part1 to inode_get()
 *           will not require device I/O.
 *           That is, the caller must ensure that the aggregate
 *           inode extent containing part1 of the target AIT
 *           resides in the fsck inode buffer before calling
 *           this routine.  (See inode_get() for more info.)
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int backout_ait_part1(int which_ait)
{
	int baitp1_rc = FSCK_OK;
	struct fsck_ino_msg_info ino_msg_info;
	struct fsck_ino_msg_info *msg_info_ptr;

	msg_info_ptr = &ino_msg_info;
	msg_info_ptr->msg_inopfx = fsck_aggr_inode;
	msg_info_ptr->msg_inotyp = fsck_metadata;

	agg_recptr->inode_stamp = -1;

	msg_info_ptr->msg_inonum = AGGREGATE_I;
	baitp1_rc =
	    backout_valid_agg_inode(which_ait, AGGREGATE_I, msg_info_ptr);

	msg_info_ptr->msg_inonum = BMAP_I;
	if (baitp1_rc == FSCK_OK) {
		baitp1_rc =
		    backout_valid_agg_inode(which_ait, BMAP_I, msg_info_ptr);

		msg_info_ptr->msg_inonum = LOG_I;
		if (baitp1_rc == FSCK_OK) {
			baitp1_rc =
			    backout_valid_agg_inode(which_ait, LOG_I,
						    msg_info_ptr);

			msg_info_ptr->msg_inonum = BADBLOCK_I;
			if (baitp1_rc == FSCK_OK) {
				baitp1_rc =
				    backout_valid_agg_inode(which_ait,
							    BADBLOCK_I,
							    msg_info_ptr);
			}
		}
	}

	return (baitp1_rc);
}

/*****************************************************************************
 * NAME: backout_valid_agg_inode
 *
 * FUNCTION:  Unrecord, in the fsck workspace block map, storage allocated to
 *            the specified aggregate inode, assuming that all data structures
 *            associated with the inode are consistent.  (E.g., the B+ Tree
 *            is at least internally consistent.)
 *
 * PARAMETERS:
 *      which_ait     - input - the Aggregate Inode Table on which to perform
 *                              the function.  { fsck_primary | fsck_secondary }
 *      inoidx        - input - ordinal number of the inode (i.e., inode number
 *                              as an int32_t)
 *      msg_info_ptr  - input - pointer to a data area with data needed to
 *                              issue messages about the inode
 *
 * NOTES:  o The caller to this routine must ensure that the
 *           calls made by backout_ait_part1 to inode_get()
 *           will not require device I/O.
 *           That is, the caller must ensure that the aggregate
 *           inode extent containing part1 of the target AIT
 *           resides in the fsck inode buffer before calling
 *           this routine.  (See inode_get() for more info.)
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int backout_valid_agg_inode(int which_ait, uint32_t inoidx,
			    struct fsck_ino_msg_info *msg_info_ptr)
{
	int bvai_rc = FSCK_OK;
	int agg_inode = -1;
	int alloc_ifnull = 0;
	struct dinode *inoptr;
	struct fsck_inode_record *inorecptr;

	bvai_rc = inode_get(agg_inode, which_ait, inoidx, &inoptr);

	if (bvai_rc != FSCK_OK) {
		/* didn't get the inode  */
		bvai_rc = FSCK_FAILED_REREAD_AGGINO;
	} else {
		/* got the inode */
		bvai_rc =
		    get_inorecptr(agg_inode, alloc_ifnull, inoidx, &inorecptr);
		if ((bvai_rc == FSCK_OK) && (inorecptr == NULL)) {
			bvai_rc = FSCK_INTERNAL_ERROR_22;
			fsck_send_msg(fsck_INTERNALERROR, bvai_rc, 0, 0, 0);
		} else if (bvai_rc == FSCK_OK) {
			bvai_rc =
			    unrecord_valid_inode(inoptr, inoidx, inorecptr,
						 msg_info_ptr);
		}
	}

	return (bvai_rc);
}

/*****************************************************************************
 * NAME: fatal_dup_check
 *
 * FUNCTION:  Determine whether any blocks are allocated to more than one
 *            aggregate metadata object.  (If so, the aggregate is too
 *            far gone for fsck to correct it, or even to analyze it with
 *            any confidence.)
 *
 * PARAMETERS:  none
 *
 * NOTES:
 *       This routine is called after all the following has been
 *       completed:
 *          - all metadata (aggregate and fileset) has been validated
 *          - all inode extents have been recorded
 *          - all fixed metadata has been recorded
 *          - the block map and inode map have been recorded
 *
 *       A similar check is done in validate_select_agg_inode_table
 *       when an apparently valid part of the table has been identified.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int fatal_dup_check()
{
	struct dupall_blkrec *dupblk_ptr;
	int64_t first_in_range, this_blknum, last_blknum;
	uint32_t range_size;

	if (agg_recptr->dup_alloc_lst == NULL)
		return FSCK_OK;

	/* duplicate allocations detected during metadata validation */

	dupblk_ptr = agg_recptr->dup_alloc_lst;

	first_in_range = dupblk_ptr->first_blk;
	last_blknum = dupblk_ptr->last_blk;
	dupblk_ptr = dupblk_ptr->next;

	while (dupblk_ptr != NULL) {
		/* for all multiply allocated blocks */
		this_blknum = dupblk_ptr->first_blk;
		if (last_blknum != (this_blknum - 1)) {
			range_size = last_blknum - first_in_range + 1;
			fsck_send_msg(fsck_DUPBLKMDREF, range_size,
				      (long long) first_in_range);
			first_in_range = this_blknum;
		}
		last_blknum = dupblk_ptr->last_blk;
		dupblk_ptr = dupblk_ptr->next;
	}
	range_size = last_blknum - first_in_range + 1;
	fsck_send_msg(fsck_DUPBLKMDREF, range_size,
		      (long long) first_in_range);

	fsck_send_msg(fsck_DUPBLKMDREFS);

	agg_recptr->ag_dirty = 1;

	return (FSCK_DUPMDBLKREF);
}

/*****************************************************************************
 * NAME: first_ref_check_agg_metadata
 *
 * FUNCTION:  Determine whether the storage allocated for aggregate metadata
 *            includes a reference to any multiply-allocated aggregate blocks
 *            for which the first reference is still unresolved.
 *
 * PARAMETERS:  none
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int first_ref_check_agg_metadata()
{
	int frcam_rc = FSCK_OK;
	uint32_t ino_idx;
	struct dinode *ino_ptr;
	int aggregate_inode = -1;	/* going for aggregate inodes only */
	int alloc_ifnull = 0;
	int which_ait;
	struct fsck_inode_record *inorec_ptr;
	struct fsck_ino_msg_info ino_msg_info;
	struct fsck_ino_msg_info *msg_info_ptr;

	msg_info_ptr = &ino_msg_info;
	msg_info_ptr->msg_inopfx = fsck_aggr_inode;
	msg_info_ptr->msg_inotyp = fsck_metadata;

	/*
	 * check ait part 1 inodes for first references
	 */
	(agg_recptr->primary_ait_4part1) ? (which_ait = fsck_primary)
	    : (which_ait = fsck_secondary);

	/* try for the self inode */
	ino_idx = AGGREGATE_I;
	frcam_rc = inode_get(aggregate_inode, which_ait, ino_idx, &ino_ptr);

	if (frcam_rc == FSCK_OK) {
		/* got the self inode  */
		msg_info_ptr->msg_inonum = ino_idx;
		frcam_rc =
		    get_inorecptr(aggregate_inode, alloc_ifnull, ino_idx,
				  &inorec_ptr);
		if ((frcam_rc == FSCK_OK) && (inorec_ptr == NULL)) {
			frcam_rc = FSCK_INTERNAL_ERROR_25;
			fsck_send_msg(fsck_INTERNALERROR, frcam_rc, 0, 0, 0);
		} else if (frcam_rc == FSCK_OK) {
			frcam_rc = first_ref_check_inode(ino_ptr, ino_idx,
							 inorec_ptr,
							 msg_info_ptr);
		}
	} else {
		/* couldn't read the inode!
		 * (We read it successfully a little while ago)
		 */
		frcam_rc = FSCK_FAILED_SELF_READ3;
	}

	if ((frcam_rc == FSCK_OK) && (agg_recptr->unresolved_1stref_count > 0)) {
		/* no errors and still have 1st refs to resolve */
		/* try for the blockmap inode */
		ino_idx = BMAP_I;
		frcam_rc =
		    inode_get(aggregate_inode, which_ait, ino_idx, &ino_ptr);

		if (frcam_rc == FSCK_OK) {
			/* got the block map inode */
			msg_info_ptr->msg_inonum = ino_idx;
			frcam_rc =
			    get_inorecptr(aggregate_inode, alloc_ifnull,
					  ino_idx, &inorec_ptr);
			if ((frcam_rc == FSCK_OK) && (inorec_ptr == NULL)) {
				frcam_rc = FSCK_INTERNAL_ERROR_26;
				fsck_send_msg(fsck_INTERNALERROR, frcam_rc, 0, 0, 0);
			} else if (frcam_rc == FSCK_OK) {
				frcam_rc =
				    first_ref_check_inode(ino_ptr, ino_idx,
							  inorec_ptr,
							  msg_info_ptr);
			}
		} else {
			/* couldn't read the inode!
			 * (We read it successfully a little while ago)
			 */
			frcam_rc = FSCK_FAILED_BMAP_READ2;
		}
	}
	if ((frcam_rc == FSCK_OK) && (agg_recptr->unresolved_1stref_count > 0)) {
		/* no errors and still have 1st refs to resolve */
		/* try for the journal inode */
		ino_idx = LOG_I;
		frcam_rc =
		    inode_get(aggregate_inode, which_ait, ino_idx, &ino_ptr);

		if (frcam_rc == FSCK_OK) {
			/* got the journal inode */
			msg_info_ptr->msg_inonum = ino_idx;
			frcam_rc =
			    get_inorecptr(aggregate_inode, alloc_ifnull,
					  ino_idx, &inorec_ptr);
			if ((frcam_rc == FSCK_OK) && (inorec_ptr == NULL)) {
				frcam_rc = FSCK_INTERNAL_ERROR_23;
				fsck_send_msg(fsck_INTERNALERROR, frcam_rc, 0, 0, 0);
			} else if (frcam_rc == FSCK_OK) {
				frcam_rc =
				    first_ref_check_inode(ino_ptr, ino_idx,
							  inorec_ptr,
							  msg_info_ptr);
			}
		} else {
			/* couldn't read the inode!
			 * (We read it successfully a little while ago)
			 */
			frcam_rc = FSCK_FAILED_LOG_READ2;
		}
	}
	if ((frcam_rc == FSCK_OK) && (agg_recptr->unresolved_1stref_count > 0)) {
		/* no errors and still have 1st refs to resolve */
		/* try for the bad block inode */
		ino_idx = BADBLOCK_I;
		frcam_rc =
		    inode_get(aggregate_inode, which_ait, ino_idx, &ino_ptr);

		if (frcam_rc == FSCK_OK) {
			/* got the bad block inode */
			msg_info_ptr->msg_inonum = ino_idx;
			frcam_rc =
			    get_inorecptr(aggregate_inode, alloc_ifnull,
					  ino_idx, &inorec_ptr);
			if ((frcam_rc == FSCK_OK) && (inorec_ptr == NULL)) {
				frcam_rc = FSCK_INTERNAL_ERROR_58;
				fsck_send_msg(fsck_INTERNALERROR, frcam_rc, 0, 0, 0);
			} else if (frcam_rc == FSCK_OK) {
				frcam_rc =
				    first_ref_check_inode(ino_ptr, ino_idx,
							  inorec_ptr,
							  msg_info_ptr);
			}
		} else {
			/* couldn't read the inode!
			 * (We read it successfully a little while ago)
			 */
			frcam_rc = FSCK_FAILED_BADBLK_READ2;
		}
	}
	/*
	 * check ait part 2 inodes for first references
	 */
	(agg_recptr->primary_ait_4part2) ? (which_ait = fsck_primary)
	    : (which_ait = fsck_secondary);

	if ((frcam_rc == FSCK_OK) && (agg_recptr->unresolved_1stref_count > 0)) {
		/* no errors and still have 1st refs to resolve */
		/* read the aggregate inode */
		ino_idx = FILESYSTEM_I;
		frcam_rc =
		    inode_get(aggregate_inode, which_ait, ino_idx, &ino_ptr);

		if (frcam_rc == FSCK_OK) {
			/* got the fileset inode */
			msg_info_ptr->msg_inonum = ino_idx;
			frcam_rc =
			    get_inorecptr(aggregate_inode, alloc_ifnull,
					  ino_idx, &inorec_ptr);
			if ((frcam_rc == FSCK_OK) && (inorec_ptr == NULL)) {
				frcam_rc = FSCK_INTERNAL_ERROR_28;
				fsck_send_msg(fsck_INTERNALERROR, frcam_rc, 0, 0, 0);
			} else if (frcam_rc == FSCK_OK) {
				frcam_rc =
				    first_ref_check_inode(ino_ptr, ino_idx,
							  inorec_ptr,
							  msg_info_ptr);
			}
		} else {
			/* couldn't read the inode!
			 * (We read it successfully a little while ago)
			 */
			frcam_rc = FSCK_FAILED_AGFS_READ3;
		}
	}
	if ((frcam_rc == FSCK_OK) && (agg_recptr->unresolved_1stref_count > 0)) {
		/* no errors and still have 1st refs to resolve */
		frcam_rc = first_ref_check_other_ait();
	}

	return (frcam_rc);
}

/**************************************************************************
 * NAME: first_ref_check_fixed_metadata
 *
 * FUNCTION: Certain aggregate metadata is not described by any inode.
 *           This routine determines whether any of the blocks occupied
 *           by this aggregate are multiply-allocated and still have
 *           unresolved first reference.
 *
 * PARAMETERS:  none
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int first_ref_check_fixed_metadata()
{
	int frcfm_rc = FSCK_OK;
	int64_t start_block, end_block;
	uint32_t length_in_agg_blocks;
	int64_t wsp_blocks_described;

	/*
	 * the reserved space (at the beginning of the aggregate)
	 */
	start_block = 0;
	length_in_agg_blocks = AGGR_RSVD_BYTES / sb_ptr->s_bsize;
	end_block = start_block + length_in_agg_blocks - 1;
	frcfm_rc = blkall_ref_check(start_block, end_block);

	if (frcfm_rc || (agg_recptr->unresolved_1stref_count == 0))
		return frcfm_rc;
	/*
	 * the superblocks
	 */
	length_in_agg_blocks = SIZE_OF_SUPER / sb_ptr->s_bsize;
	/*
	 * primary
	 */
	start_block = SUPER1_OFF / sb_ptr->s_bsize;
	end_block = start_block + length_in_agg_blocks - 1;
	frcfm_rc = blkall_ref_check(start_block, end_block);

	if (frcfm_rc || (agg_recptr->unresolved_1stref_count == 0))
		return frcfm_rc;
	/*
	 * secondary
	 */
	start_block = SUPER2_OFF / sb_ptr->s_bsize;
	end_block = start_block + length_in_agg_blocks - 1;
	frcfm_rc = blkall_ref_check(start_block, end_block);

	if (frcfm_rc || (agg_recptr->unresolved_1stref_count == 0))
		return frcfm_rc;

	/*
	 * note that the fsck workspace and journal log (at the end of the
	 * aggregate) are not described by the block map (neither the
	 * Aggregate Block Allocation Map nor the fsck Workspace Block Map)
	 */
	/*
	 * the "phantom blocks" described by the last dmap page
	 */
	/*
	 * the first page is a control page and scratch area.
	 */
	wsp_blocks_described = BITSPERBYTE *
		(agg_recptr->ondev_wsp_byte_length - BYTESPERPAGE);
	if (wsp_blocks_described > agg_recptr->sb_agg_fsblk_length) {
		/*
		 * the dmaps do describe more blocks than
		 * actually exist
		 */
		/* since this is
		 * the number of blocks and since blocks are
		 * numbered starting at 0, this is the block
		 * number of the first phantom block;
		 */
		start_block = agg_recptr->sb_agg_fsblk_length;
		end_block = wsp_blocks_described - 1;
		frcfm_rc = blkall_ref_check(start_block, end_block);
	}
	return (frcfm_rc);
}

/*****************************************************************************
 * NAME: first_ref_check_fs_metadata
 *
 * FUNCTION: Determine whether any blocks occupied by fileset metadata are
 *           are multiply-allocated and still have unresolved first
 *           reference.
 *
 * PARAMETERS:  none
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int first_ref_check_fs_metadata()
{
	int frcfsm_rc = FSCK_OK;
	uint32_t ino_idx;
	struct dinode *ino_ptr;
	int aggregate_inode = 0;	/* going for fileset inodes only */
	int alloc_ifnull = 0;
	int which_fsit;		/* which fileset? */
	struct fsck_inode_record *inorec_ptr;
	struct fsck_ino_msg_info ino_msg_info;
	struct fsck_ino_msg_info *msg_info_ptr;

	msg_info_ptr = &ino_msg_info;
	msg_info_ptr->msg_inopfx = fsck_fset_inode;	/* all fileset owned */
	msg_info_ptr->msg_inotyp = fsck_metadata;

	/*
	 * in release 1 there is exactly 1 fileset
	 */
	which_fsit = 0;

	/* try for the fileset superinode extension */
	ino_idx = FILESET_EXT_I;
	frcfsm_rc = inode_get(aggregate_inode, which_fsit, ino_idx, &ino_ptr);

	if (frcfsm_rc == FSCK_OK) {
		/* got the inode  */
		msg_info_ptr->msg_inonum = ino_idx;
		frcfsm_rc =
		    get_inorecptr(aggregate_inode, alloc_ifnull, ino_idx,
				  &inorec_ptr);
		if ((frcfsm_rc == FSCK_OK) && inorec_ptr) {
			frcfsm_rc = first_ref_check_inode(ino_ptr, ino_idx,
							  inorec_ptr,
							  msg_info_ptr);
		}
	} else {
		/* couldn't read the inode!
		 * (We read it successfully a little while ago)
		 */
		frcfsm_rc = FSCK_FAILED_FSSIEXT_READ2;
	}

	if ((frcfsm_rc == FSCK_OK) && (agg_recptr->unresolved_1stref_count > 0)) {
		/* no errors and still have 1st refs to resolve */
		/* try for the root directory inode */
		ino_idx = ROOT_I;
		frcfsm_rc =
		    inode_get(aggregate_inode, which_fsit, ino_idx, &ino_ptr);

		if (frcfsm_rc == FSCK_OK) {
			/* got the root dir inode */
			msg_info_ptr->msg_inonum = ino_idx;
			frcfsm_rc =
			    get_inorecptr(aggregate_inode, alloc_ifnull,
					  ino_idx, &inorec_ptr);
			if ((frcfsm_rc == FSCK_OK) && (inorec_ptr == NULL)) {
				frcfsm_rc = FSCK_INTERNAL_ERROR_29;
				fsck_send_msg(fsck_INTERNALERROR, frcfsm_rc, 0, 0, 0);
			} else if (frcfsm_rc == FSCK_OK) {
				frcfsm_rc =
				    first_ref_check_inode(ino_ptr, ino_idx,
							  inorec_ptr,
							  msg_info_ptr);
			}
			if ((frcfsm_rc == FSCK_OK)
			    && (inorec_ptr->selected_to_rls)) {
				/* routine doesn't
				 * understand that the root directory is
				 * the special case of a directory which
				 * is also special metadata and might
				 * flag it for release.
				 */
				inorec_ptr->selected_to_rls = 0;
			}
		} else {
			/* couldn't read the inode!
			 * (We read it successfully a little while ago)
			 */
			frcfsm_rc = FSCK_FAILED_FSRTDIR_READ2;
		}
	}
	return (frcfsm_rc);
}

/**************************************************************************
 * NAME: first_ref_check_other_ait
 *
 * FUNCTION: Determine whether any blocks occupied by the "other ait"
 *           are multiply-allocated and still have unresolved first
 *           reference.  To be more specific, if the primary ait is
 *           being used for fsck processing, the secondary ait is the
 *           "other ait"...and vice versa.
 *
 * PARAMETERS:  none
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int first_ref_check_other_ait()
{
	int frcoa_rc = FSCK_OK;
	int64_t start_block, end_block;
	uint32_t length_in_agg_blocks;

	/*
	 * first extent of the agg inode map
	 */
	if (agg_recptr->primary_ait_4part1) {
		/* primary already recorded */
		start_block = addressPXD(&(sb_ptr->s_aim2));
		length_in_agg_blocks = lengthPXD(&(sb_ptr->s_aim2));
	} else {
		/* secondary already recorded */
		start_block = AIMAP_OFF / sb_ptr->s_bsize;
		length_in_agg_blocks =
		    (AITBL_OFF - AIMAP_OFF) / sb_ptr->s_bsize;
	}

	end_block = start_block + length_in_agg_blocks - 1;
	frcoa_rc = blkall_ref_check(start_block, end_block);

	/*
	 * first extent of the agg inode table
	 */
	if (agg_recptr->primary_ait_4part1) {
		/* primary already recorded */
		start_block = addressPXD(&(sb_ptr->s_ait2));
		length_in_agg_blocks = lengthPXD(&(sb_ptr->s_ait2));
	} else {
		/* secondary already recorded */
		start_block = AITBL_OFF / sb_ptr->s_bsize;
		length_in_agg_blocks = INODE_EXTENT_SIZE / sb_ptr->s_bsize;
	}

	end_block = start_block + length_in_agg_blocks - 1;
	frcoa_rc = blkall_ref_check(start_block, end_block);

	return (frcoa_rc);
}

/*****************************************************************************
 * NAME: record_ait_part1_again
 *
 * FUNCTION:  Record, in the fsck workspace block map, all storage allocated
 *            to inodes in part 1 (inodes 0 through 15) of the specified
 *            (primary or secondary) aggregate inode table.  Do this with
 *            the knowledge that all these inodes have been verified
 *            completely correct.
 *
 * PARAMETERS:
 *      which_ait  - input - the Aggregate Inode Table on which to perform
 *                           the function.  { fsck_primary | fsck_secondary }
 *
 * NOTES:  o The caller to this routine must ensure that the
 *           calls made by record_ait_part1_again to inode_get()
 *           will not require device I/O.
 *           That is, the caller must ensure that the aggregate
 *           inode extent containing part1 of the target AIT
 *           resides in the fsck inode buffer before calling
 *           this routine.  (See inode_get() for more info)
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int record_ait_part1_again(int which_ait)
{
	int raitp1a_rc = FSCK_OK;
	struct fsck_inode_record *inorecptr;
	uint32_t ino_idx;
	struct dinode *ino_ptr;
	int aggregate_inode = -1;	/* going for aggregate inodes only */
	int alloc_ifnull = 0;

	struct fsck_ino_msg_info msg_info;
	struct fsck_ino_msg_info *msg_info_ptr;

	msg_info_ptr = &msg_info;
	msg_info_ptr->msg_inopfx = fsck_aggr_inode;
	msg_info_ptr->msg_inotyp = fsck_metadata;

	/* try for the self inode */
	ino_idx = AGGREGATE_I;
	msg_info_ptr->msg_inonum = ino_idx;
	raitp1a_rc = inode_get(aggregate_inode, which_ait, ino_idx, &ino_ptr);

	if (raitp1a_rc == FSCK_OK) {
		/* got the self inode  */
		agg_recptr->inode_stamp = ino_ptr->di_inostamp;

		raitp1a_rc =
		    get_inorecptr(aggregate_inode, alloc_ifnull, ino_idx,
				  &inorecptr);
		if ((raitp1a_rc == FSCK_OK) && (inorecptr == NULL)) {
			raitp1a_rc = FSCK_INTERNAL_ERROR_39;
			fsck_send_msg(fsck_INTERNALERROR, raitp1a_rc, 0, 0, 0);
		} else if (raitp1a_rc == FSCK_OK) {
			inorecptr->in_use = 0;
			inorecptr->selected_to_rls = 0;
			inorecptr->crrct_link_count = 0;
			inorecptr->crrct_prnt_inonum = 0;
			inorecptr->adj_entries = 0;
			inorecptr->cant_chkea = 0;
			inorecptr->clr_ea_fld = 0;
			inorecptr->clr_acl_fld = 0;
			inorecptr->inlineea_on = 0;
			inorecptr->inlineea_off = 0;
			inorecptr->inline_data_err = 0;
			inorecptr->ignore_alloc_blks = 0;
			inorecptr->reconnect = 0;
			inorecptr->unxpctd_prnts = 0;
			inorecptr->badblk_inode = 0;
			inorecptr->involved_in_dups = 0;
			inorecptr->inode_type = metadata_inode;
			inorecptr->link_count = 0;
			inorecptr->parent_inonum = 0;
			inorecptr->ext_rec = NULL;
			raitp1a_rc =
			    record_valid_inode(ino_ptr, ino_idx, inorecptr,
					       msg_info_ptr);
		}

		if (raitp1a_rc == FSCK_OK) {
			/* recorded it successfully */
			inorecptr->in_use = 1;
		}
	}
	if (raitp1a_rc == FSCK_OK) {
		/* self inode recorded ok */
		/* try for the blockmap inode */
		ino_idx = BMAP_I;
		msg_info_ptr->msg_inonum = ino_idx;
		raitp1a_rc =
		    inode_get(aggregate_inode, which_ait, ino_idx, &ino_ptr);

		if (raitp1a_rc == FSCK_OK) {
			/* got the block map inode */
			raitp1a_rc =
			    get_inorecptr(aggregate_inode, alloc_ifnull,
					  ino_idx, &inorecptr);
			if ((raitp1a_rc == FSCK_OK) && (inorecptr == NULL)) {
				raitp1a_rc = FSCK_INTERNAL_ERROR_43;
				fsck_send_msg(fsck_INTERNALERROR, raitp1a_rc, 0, 0, 0);
			} else if (raitp1a_rc == FSCK_OK) {
				inorecptr->in_use = 0;
				inorecptr->selected_to_rls = 0;
				inorecptr->crrct_link_count = 0;
				inorecptr->crrct_prnt_inonum = 0;
				inorecptr->adj_entries = 0;
				inorecptr->cant_chkea = 0;
				inorecptr->clr_ea_fld = 0;
				inorecptr->clr_acl_fld = 0;
				inorecptr->inlineea_on = 0;
				inorecptr->inlineea_off = 0;
				inorecptr->inline_data_err = 0;
				inorecptr->ignore_alloc_blks = 0;
				inorecptr->reconnect = 0;
				inorecptr->unxpctd_prnts = 0;
				inorecptr->badblk_inode = 0;
				inorecptr->involved_in_dups = 0;
				inorecptr->inode_type = metadata_inode;
				inorecptr->link_count = 0;
				inorecptr->parent_inonum = 0;
				inorecptr->ext_rec = NULL;
				raitp1a_rc =
				    record_valid_inode(ino_ptr, ino_idx,
						       inorecptr, msg_info_ptr);
			}

			if (raitp1a_rc == FSCK_OK) {
				/* recorded it successfully */
				inorecptr->in_use = 1;
			}
		}
	}
	if (raitp1a_rc == FSCK_OK) {
		/* self and bmap inodes recorded ok */
		/* try for the journal log inode */
		ino_idx = LOG_I;
		msg_info_ptr->msg_inonum = ino_idx;
		raitp1a_rc =
		    inode_get(aggregate_inode, which_ait, ino_idx, &ino_ptr);

		if (raitp1a_rc == FSCK_OK) {
			/* got the journal log inode */
			raitp1a_rc =
			    get_inorecptr(aggregate_inode, alloc_ifnull,
					  ino_idx, &inorecptr);
			if ((raitp1a_rc == FSCK_OK) && (inorecptr == NULL)) {
				raitp1a_rc = FSCK_INTERNAL_ERROR_44;
				fsck_send_msg(fsck_INTERNALERROR, raitp1a_rc, 0, 0, 0);
			} else if (raitp1a_rc == FSCK_OK) {
				inorecptr->in_use = 0;
				inorecptr->selected_to_rls = 0;
				inorecptr->crrct_link_count = 0;
				inorecptr->crrct_prnt_inonum = 0;
				inorecptr->adj_entries = 0;
				inorecptr->cant_chkea = 0;
				inorecptr->clr_ea_fld = 0;
				inorecptr->clr_acl_fld = 0;
				inorecptr->inlineea_on = 0;
				inorecptr->inlineea_off = 0;
				inorecptr->inline_data_err = 0;
				inorecptr->ignore_alloc_blks = 0;
				inorecptr->reconnect = 0;
				inorecptr->unxpctd_prnts = 0;
				inorecptr->badblk_inode = 0;
				inorecptr->involved_in_dups = 0;
				inorecptr->inode_type = metadata_inode;
				inorecptr->link_count = 0;
				inorecptr->parent_inonum = 0;
				inorecptr->ext_rec = NULL;
				raitp1a_rc =
				    record_valid_inode(ino_ptr, ino_idx,
						       inorecptr, msg_info_ptr);
			}

			if (raitp1a_rc == FSCK_OK) {
				/* recorded it successfully */
				inorecptr->in_use = 1;
			}
		}
	}
	if (raitp1a_rc == FSCK_OK) {
		/* self, bmap, and journal inodes recorded ok */
		/* try for the bad block inode */
		ino_idx = BADBLOCK_I;
		msg_info_ptr->msg_inonum = ino_idx;
		raitp1a_rc =
		    inode_get(aggregate_inode, which_ait, ino_idx, &ino_ptr);

		if (raitp1a_rc == FSCK_OK) {
			/* got the bad block inode */
			raitp1a_rc =
			    get_inorecptr(aggregate_inode, alloc_ifnull,
					  ino_idx, &inorecptr);
			if ((raitp1a_rc == FSCK_OK) && (inorecptr == NULL)) {
				raitp1a_rc = FSCK_INTERNAL_ERROR_59;
				fsck_send_msg(fsck_INTERNALERROR, raitp1a_rc, 0, 0, 0);
			} else if (raitp1a_rc == FSCK_OK) {
				inorecptr->in_use = 0;
				inorecptr->selected_to_rls = 0;
				inorecptr->crrct_link_count = 0;
				inorecptr->crrct_prnt_inonum = 0;
				inorecptr->adj_entries = 0;
				inorecptr->cant_chkea = 0;
				inorecptr->clr_ea_fld = 0;
				inorecptr->clr_acl_fld = 0;
				inorecptr->inlineea_on = 0;
				inorecptr->inlineea_off = 0;
				inorecptr->inline_data_err = 0;
				inorecptr->ignore_alloc_blks = 0;
				inorecptr->reconnect = 0;
				inorecptr->unxpctd_prnts = 0;
				inorecptr->badblk_inode = 0;
				inorecptr->involved_in_dups = 0;
				inorecptr->inode_type = metadata_inode;
				inorecptr->link_count = 0;
				inorecptr->parent_inonum = 0;
				inorecptr->ext_rec = NULL;
				raitp1a_rc =
				    record_valid_inode(ino_ptr, ino_idx,
						       inorecptr, msg_info_ptr);
			}

			if (raitp1a_rc == FSCK_OK) {
				/* recorded it successfully */
				inorecptr->in_use = 1;
			}
		}
	}
	return (raitp1a_rc);
}

/**************************************************************************
 * NAME: record_fixed_metadata
 *
 * FUNCTION: Certain aggregate metadata is not described by any inode.
 *           This routine marks, in the fsck workspace block map, that
 *           the blocks occupied by this aggregate metadata are in use.
 *
 * PARAMETERS:  none
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int record_fixed_metadata()
{
	int rfm_rc = FSCK_OK;
	int64_t start_block, end_block;
	uint32_t length_in_agg_blocks;
	int64_t wsp_blocks_described;

	/*
	 * the reserved space (at the beginning of the aggregate)
	 */
	start_block = 0;
	length_in_agg_blocks = AGGR_RSVD_BYTES / sb_ptr->s_bsize;
	end_block = start_block + length_in_agg_blocks - 1;
	rfm_rc = blkall_increment_owners(start_block, end_block, NULL);

	/*
	 * the superblocks
	 */
	if (rfm_rc == FSCK_OK) {
		/* go ahead with superblocks */
		length_in_agg_blocks = SIZE_OF_SUPER / sb_ptr->s_bsize;
		/*
		 * primary
		 */
		start_block = SUPER1_OFF / sb_ptr->s_bsize;
		end_block = start_block + length_in_agg_blocks - 1;
		rfm_rc = blkall_increment_owners(start_block, end_block, NULL);
		/*
		 * secondary
		 */
		if (rfm_rc == FSCK_OK) {
			start_block = SUPER2_OFF / sb_ptr->s_bsize;
			end_block = start_block + length_in_agg_blocks - 1;
			rfm_rc = blkall_increment_owners(start_block, end_block,
							 NULL);
		}
	}
	/*
	 * note that the fsck workspace and journal log (at the end of the
	 * aggregate) are not described by the block map (neither the
	 * Aggregate Block Allocation Map nor the fsck Workspace Block Map)
	 */
	/*
	 * the "phantom blocks" described by the last dmap page
	 */
	if (rfm_rc == FSCK_OK) {
		/*
		 * the first page is a control page and scratch area.
		 */
		wsp_blocks_described =
		    BITSPERBYTE * (agg_recptr->ondev_wsp_byte_length -
				   BYTESPERPAGE);
		if (wsp_blocks_described > agg_recptr->sb_agg_fsblk_length) {
			/*
			 * the dmaps do describe more blocks than
			 * actually exist
			 */
			/* since this is
			 * the number of blocks and since blocks are
			 * numbered starting at 0, this is the block
			 * number of the first phantom block;
			 */
			start_block = agg_recptr->sb_agg_fsblk_length;
			end_block = wsp_blocks_described - 1;
			rfm_rc = blkall_increment_owners(start_block, end_block,
							 NULL);
		}
	}
	return (rfm_rc);
}

/**************************************************************************
 * NAME: record_other_ait
 *
 * FUNCTION: Record the blocks occupied by the "other ait" in the fsck
 *           workspace block map.  To be more specific, if the primary
 *           ait is being used for fsck processing, the secondary ait
 *           is the "other ait"...and vice versa.
 *
 * PARAMETERS:  none
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int record_other_ait()
{
	int roa_rc = FSCK_OK;
	int64_t start_block, end_block;
	uint32_t length_in_agg_blocks;

	/*
	 * first extent of the agg inode map
	 */
	if (agg_recptr->primary_ait_4part1) {
		/* primary already recorded */
		start_block = addressPXD(&(sb_ptr->s_aim2));
		length_in_agg_blocks = lengthPXD(&(sb_ptr->s_aim2));
	} else {
		/* secondary already recorded */
		start_block = AIMAP_OFF / sb_ptr->s_bsize;
		length_in_agg_blocks =
		    (AITBL_OFF - AIMAP_OFF) / sb_ptr->s_bsize;
	}

	end_block = start_block + length_in_agg_blocks - 1;
	roa_rc = blkall_increment_owners(start_block, end_block, NULL);

	/*
	 * first extent of the agg inode table
	 */
	if (agg_recptr->primary_ait_4part1) {
		/* primary already recorded */
		start_block = addressPXD(&(sb_ptr->s_ait2));
		length_in_agg_blocks = lengthPXD(&(sb_ptr->s_ait2));
	} else {
		/* secondary already recorded */
		start_block = AITBL_OFF / sb_ptr->s_bsize;
		length_in_agg_blocks = INODE_EXTENT_SIZE / sb_ptr->s_bsize;
	}

	end_block = start_block + length_in_agg_blocks - 1;
	roa_rc = blkall_increment_owners(start_block, end_block, NULL);

	return (roa_rc);
}

/*****************************************************************************
 * NAME: replicate_superblock
 *
 * FUNCTION: Refresh both the primary and secondary superblocks in the
 *           aggregate from the correct (and possibly updated) superblock
 *           in the fsck buffer.
 *
 * PARAMETERS:  none
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int replicate_superblock()
{
	int rs_rc = FSCK_OK;

	/* write from the buffer to the primary superblock */
	rs_rc = ujfs_put_superblk(Dev_IOPort, sb_ptr, 1);
	/* have to assume something got written */
	agg_recptr->ag_modified = 1;
	if (rs_rc != FSCK_OK) {
		/* not good here is really bad */
		agg_recptr->cant_write_primary_sb = 1;
		agg_recptr->ag_dirty = 1;
		/* mark the superblock in the buffer
		 * to show the aggregate is dirty
		 * (in case it isn't already marked
		 * that way)
		 */
		sb_ptr->s_state |= FM_DIRTY;
		fsck_send_msg(fsck_CNTWRTSUPP);
	} else {
		/* wrote to the primary superblock */
		agg_recptr->cant_write_primary_sb = 0;
	}

	/* write from the buffer to the secondary superblock */
	rs_rc = ujfs_put_superblk(Dev_IOPort, sb_ptr, 0);
	/* have to assume something got written */
	agg_recptr->ag_modified = 1;
	if (rs_rc == FSCK_OK) {
		/* wrote to secondary ok */
		agg_recptr->cant_write_secondary_sb = 0;
	} else {
		/* not good here is pretty bad */
		agg_recptr->cant_write_secondary_sb = 1;
		fsck_send_msg(fsck_CNTWRTSUPS);

		if ((sb_ptr->s_state & FM_DIRTY) != FM_DIRTY) {
			/* superblk not marked dirty now */
			/*
			 * This means, among other things, that we just
			 * did a successful write to the primary superblock
			 * and that we marked the primary to say the aggregate
			 * is clean.
			 */
			sb_ptr->s_state |= FM_DIRTY;
			/* write from the buffer to the primary superblock */
			rs_rc = ujfs_put_superblk(Dev_IOPort, sb_ptr, 1);
			/* have to assume something got written */
			agg_recptr->ag_modified = 1;
			if (rs_rc != FSCK_OK) {
				/* not good here is a disaster */
				/*
				 * We may have just taken an aggregate marked dirty and
				 * changed it to clean, but now we discover that it really
				 * does have a serious problem.  And all we can do about
				 * it is issue the strongest warning we can think up.
				 */
				agg_recptr->cant_write_primary_sb = 1;

				fsck_send_msg(fsck_CNTWRTSUPP);

				fsck_send_msg(fsck_AGGDRTYNOTCLN, Vol_Label);
			}
		}
	}

	if ((agg_recptr->cant_write_primary_sb)
	    && (agg_recptr->cant_write_secondary_sb)) {
		/* both bad */
		rs_rc = FSCK_FAILED_BTHSBLK_WRITE;
	} else if (agg_recptr->cant_write_primary_sb) {
		/* primary bad */
		rs_rc = FSCK_FAILED_PSBLK_WRITE;
	} else if (agg_recptr->cant_write_secondary_sb) {
		/* secondary bad */
		rs_rc = FSCK_FAILED_SSBLK_WRITE;
	}
	return (rs_rc);
}

/*****************************************************************************
 * NAME: rootdir_tree_bad
 *
 * FUNCTION:  This routine is called if the B+ Tree rooted in the fileset
 *            root directory (aggregate inode FILESET_I) is found to be
 *            corrupt.  If the user approves the repair, it makes the
 *            root directory B+ tree a correct, empty tree.
 *
 * PARAMETERS:
 *      inoptr         - input - pointer to the inode in an fsck buffer
 *      inode_updated  - input - pointer to a variable in which to return
 *                               !0 if the inode (in the buffer) has been
 *                                  modified by this routine
 *                                0 if the inode (in the buffer) has not been
 *                                  modified by this routine
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int rootdir_tree_bad(struct dinode *inoptr, int *inode_updated)
{
	int rtb_rc = FSCK_OK;

	*inode_updated = 0;

	fsck_send_msg(fsck_RIBADTREE);
	init_dir_tree((dtroot_t *) & (inoptr->di_btroot));
	inoptr->di_next_index = 2;
	inoptr->di_nblocks = 0;
	inoptr->di_nlink = 2;
	inoptr->di_size = IDATASIZE;
	*inode_updated = 1;
	agg_recptr->rootdir_rebuilt = 1;
	fsck_send_msg(fsck_RICRETREE);

	return (rtb_rc);
}

/*****************************************************************************
 * NAME: validate_fs_metadata
 *
 * FUNCTION: Verify the metadata inodes for all filesets in the
 *           aggregate.
 *
 * PARAMETERS:  none
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int validate_fs_metadata()
{
	int vfm_rc = FSCK_OK;
	int intermed_rc = FSCK_OK;

	struct fsck_ino_msg_info ino_msg_info;
	struct fsck_ino_msg_info *msg_info_ptr;

	int which_fsit, which_ait;
	uint32_t ino_idx;
	struct dinode *ino_ptr;
	int aggregate_inode = 0;	/* going for fileset inodes only */
	int alloc_ifnull = -1;
	int inode_updated;

	struct fsck_inode_record *inorecptr;

	msg_info_ptr = &ino_msg_info;
	/* all fileset owned */
	msg_info_ptr->msg_inopfx = fsck_fset_inode;
	msg_info_ptr->msg_inotyp = fsck_metadata;

	if (agg_recptr->primary_ait_4part2) {
		which_ait = fsck_primary;
	} else {
		which_ait = fsck_secondary;
	}

	vfm_rc = ait_special_read_ext1(which_ait);

	if (vfm_rc != FSCK_OK) {
		/* read failed */
		report_readait_error(vfm_rc, FSCK_FAILED_CANTREADAITEXTE,
				     which_ait);
		vfm_rc = FSCK_FAILED_CANTREADAITEXTE;
		goto vfm_exit;
	}

	/*
	 * In release 1 there's exactly 1 fileset
	 */
	which_fsit = FILESYSTEM_I;

	/* read the fileset superinode extension */
	ino_idx = FILESET_EXT_I;
	intermed_rc = inode_get(aggregate_inode, which_fsit, ino_idx, &ino_ptr);
	if (intermed_rc != FSCK_OK) {
		/* can't get the inode */
		//vfm_rc = FSCK_CANTREADFSEXT;
		//goto vfm_exit;
		goto read_root; /* Who really cares? */
	}

	/* got superinode extension inode  */
	msg_info_ptr->msg_inonum = FILESET_EXT_I;
	intermed_rc = verify_fs_super_ext(ino_ptr, msg_info_ptr,&inode_updated);
	if (intermed_rc < 0) {
		/* something really really bad happened */
		//vfm_rc = intermed_rc;
		//goto vfm_exit;
		goto read_root;
	}

	if (intermed_rc != FSCK_OK) {
		/* inode is bad */
		//vfm_rc = FSCK_FSETEXTBAD;
		//goto vfm_exit;
		goto read_root;
	}

	/* superinode extension inode is ok */
	if (inode_updated) {
		/* need to write the superinode extension */
		vfm_rc = inode_put(ino_ptr);
	}
	//if (vfm_rc != FSCK_OK)
	//	goto vfm_exit;
read_root:
	/* read the root directory inode */
	ino_idx = ROOT_I;
	intermed_rc = inode_get(aggregate_inode, which_fsit, ino_idx,
		      &ino_ptr);
	if (intermed_rc < 0) {
		/* something really really bad happened */
		vfm_rc = intermed_rc;
		goto vfm_exit;
	}
	if (intermed_rc != FSCK_OK) {
		/* can't get the inode */
		vfm_rc = FSCK_CANTREADFSRTDR;
		goto vfm_exit;
	}
	/* got root directory inode  */
	msg_info_ptr->msg_inonum = ROOT_I;
	msg_info_ptr->msg_inotyp = fsck_directory;
	intermed_rc = verify_repair_fs_rootdir(ino_ptr,
				msg_info_ptr, &inode_updated);

	if (intermed_rc != FSCK_OK) {
		/* inode is bad. Couldn't (or
		 * wasn't allowed to) repair it
		 */
		vfm_rc = FSCK_FSRTDRBAD;
		goto vfm_exit;
	}
	/* root directory is good */
	if (inode_updated) {
		/* need to write the root directory */
		vfm_rc = inode_put(ino_ptr);
	}
	/*
	 * now get records as placeholders for
	 * the 2 reserved fileset inodes
	 */
	vfm_rc = get_inorecptr(aggregate_inode, alloc_ifnull,
			       FILESET_RSVD_I, &inorecptr);
	if ((vfm_rc == FSCK_OK) && (inorecptr == NULL)) {
		vfm_rc = FSCK_INTERNAL_ERROR_34;
		fsck_send_msg(fsck_INTERNALERROR, vfm_rc, 0, 0, 0);
	} else if (vfm_rc == FSCK_OK) {
		/* got first record */
		inorecptr->inode_type = metadata_inode;
		inorecptr->in_use = 1;
		vfm_rc = get_inorecptr(aggregate_inode, alloc_ifnull,
				       ACL_I, &inorecptr);
		if ((vfm_rc == FSCK_OK) && (inorecptr == NULL)) {
			vfm_rc = FSCK_INTERNAL_ERROR_35;
			fsck_send_msg(fsck_INTERNALERROR, vfm_rc, 0, 0, 0);
		} else if (vfm_rc == FSCK_OK) {
			/* got second record */
			inorecptr->inode_type = metadata_inode;
			inorecptr->in_use = 1;
		}
	}

      vfm_exit:
	return (vfm_rc);
}

/*****************************************************************************
 * NAME: validate_repair_superblock
 *
 * FUNCTION:  Verify that the primary superblock is valid.  If it is,
 *            the secondary superblock will be refreshed later in
 *            processing.  If the primary superblock is not valid,
 *            verify that the secondary superblock is valid.  If the
 *            secondary superblock is found to be valid, copy it
 *            over the primary superblock on the device so that
 *            logredo will find a valid primary superblock.
 *
 * PARAMETERS:  none
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int validate_repair_superblock()
{
	int vrsb_rc = FSCK_OK;
	int intermed_rc = FSCK_OK;
	int primary_sb_bad = 1;	/* assume a problem with primary */
	int secondary_sb_bad = 1;	/* assume a problem with secondary */
	int which_sb = 0;

	/* get primary  */
	vrsb_rc = ujfs_get_superblk(Dev_IOPort, sb_ptr, 1);

	if (vrsb_rc != FSCK_OK) {
		/* if read primary fails */
		fsck_send_msg(fsck_CNTRESUPP);
	} else {
		/* got primary superblock */
		which_sb = fsck_primary;
		primary_sb_bad = validate_super(fsck_primary);
	}

	if (primary_sb_bad) {
		/* can't use the primary superblock */
		/* get 2ndary */
		vrsb_rc = ujfs_get_superblk(Dev_IOPort, sb_ptr, 0);

		if (vrsb_rc != FSCK_OK) {
			fsck_send_msg(fsck_CNTRESUPS);
		} else {
			/* got secondary superblock */
			which_sb = fsck_secondary;
			secondary_sb_bad = validate_super(fsck_secondary);
		}

		if (!secondary_sb_bad) {
			/* secondary is ok */
			if (agg_recptr->processing_readonly) {
				agg_recptr->ag_dirty = 1;
				agg_recptr->cant_write_primary_sb = 1;
				fsck_send_msg(fsck_BDSBNWRTACC);
			} else {
				/* else processing read/write */
				sb_ptr->s_state = (sb_ptr->s_state | FM_DIRTY);
				/* correct the primary superblock */
				intermed_rc =
				    ujfs_put_superblk(Dev_IOPort, sb_ptr, 1);
				/* must assume something got written */
				agg_recptr->ag_modified = 1;
				if (intermed_rc == FSCK_OK) {
					agg_recptr->ag_modified = 1;
				} else {
					/* write primary superblock failed */
					/*
					 * we won't bail out on this condition (so we
					 * don't  want to pass back the return code),
					 * but it does leave the aggregate dirty
					 */
					agg_recptr->ag_dirty = 1;
					agg_recptr->cant_write_primary_sb = 1;

					fsck_send_msg(fsck_CNTWRTSUPP);
				}
			}
		} else {
			/* can't use the secondary superblock either */
			agg_recptr->ag_dirty = 1;
			vrsb_rc = FSCK_FAILED_BTHSBLK_BAD;

			if ((primary_sb_bad == FSCK_BADSBMGC)
			    && (secondary_sb_bad == FSCK_BADSBMGC)) {
				printf
				    ("\nThe superblock does not describe a correct jfs file system.\n"
				     "\nIf device %s is valid and contains a jfs file system,\n"
				     "then both the primary and secondary superblocks are corrupt\n"
				     "and cannot be repaired, and fsck cannot continue.\n"
				     "\nOtherwise, make sure the entered device %s is correct.\n\n",
				     Vol_Label, Vol_Label);
			} else {
				fsck_send_msg(fsck_BDSBBTHCRRPT);
			}

			if (Is_Device_Type_JFS(Vol_Label) == MSG_JFS_NOT_JFS) {
				fsck_send_msg(fsck_NOTJFSINFSTAB, Vol_Label);
			}

		}
	}
	if ((!primary_sb_bad) || (!secondary_sb_bad)) {
		/* the buffer holds a valid superblock */
		/* aggregate block size */
		agg_recptr->ag_blk_size = sb_ptr->s_bsize;

		if (which_sb == fsck_primary) {
			fsck_send_msg(fsck_SBOKP);
		} else {
			fsck_send_msg(fsck_SBOKS);
		}
		if ((sb_ptr->s_flag & JFS_SPARSE) == JFS_SPARSE) {
			fsck_send_msg(fsck_SPARSEFILSYS);
		}
	}

	return (vrsb_rc);
}

/*****************************************************************************
 * NAME: validate_select_agg_inode_table
 *
 * FUNCTION:  Verify the inodes in the Aggregate Inode Table.  If all
 *            inodes in the Primary Aggregate Inode Table are valid,
 *            select it.  Otherwise, if all inodes in the Secondary
 *            Aggregate Inode Table are valid, select it.  Otherwise,
 *            if inodes 0 through 15 are valid in one table and inodes
 *            16 through 31 are valid in the other, select the valid
 *            0 through 15 and the valid 16 through 31.
 *
 * PARAMETERS:  none
 *
 * NOTES:  o Aggregate inodes 0 through 15 describe the aggregate and
 *           aggregate metadata.  Aggregate inodes 16 through 31 each
 *           describe a fileset in the aggregate (In release 1, there
 *           is only 1 fileset in each aggregate and it is described
 *           by aggregate inode 16).  When neither Aggregate Inode
 *           Table is completely valid, this suggests the division:
 *                 "part 1" is aggregate inodes 0 through 15
 *                 "part 2" is aggregate inodes 16 through 31
 *
 *         o While we naturally prefer to use only the Primary Aggregate
 *           Inode Table and, failing that, to use only the Secondary
 *           Aggregate Inode Table, in the interests of avoiding loss
 *           of user data, fsck will continue if it can find a valid
 *           part 1 and a valid part 2.
 *
 *         o Since this routine is invoked before the fsck workspace
 *           has been completely initialized, this routine ensures
 *           that the fsck I/O buffers contain the data needed by
 *           any routine which it invokes.
 *
 *           That is, since the workspace does not contain all the
 *           information for inode_get (et al) to calculate device
 *           offsets (needed to perform I/O), this routine ensures
 *           that any invocations of inode_get by routines invoked
 *           here will find the target data already in the fsck
 *           inode buffer.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int validate_select_agg_inode_table()
{
	int vsait_rc = FSCK_OK;
	int intermed_rc = FSCK_OK;

	int aggregate_inode = -1;
	int alloc_ifnull = -1;
	struct fsck_inode_record *inorecptr;
	int cant_read_primary = 0;
	int cant_read_secondary = 0;
	int primary_part1_good = 0;
	int primary_part2_good = 0;
	int primary_part1_bad = 0;
	int primary_part2_bad = 0;
	int secondary_part1_good = 0;
	int secondary_part2_good = 0;
	int secondary_part1_bad = 0;
	int secondary_part2_bad = 0;
	uint32_t primary_inode_stamp = 0;
	uint32_t secondary_inode_stamp = 0;
	uint32_t unknown_stamp = (uint32_t) (-1);
	/*
	 * try for part 1 and part 2 both from the primary aggregate inode table
	 */
	intermed_rc = ait_special_read_ext1(fsck_primary);
	if (intermed_rc != FSCK_OK) {
		/* don't have it in the buffer */
		primary_part1_bad = -1;
		primary_part2_bad = -1;
		cant_read_primary = -1;
	} else {
		/* got the 1st extent of primary AIT into the inode buffer */
		agg_recptr->inode_stamp = unknown_stamp;
		intermed_rc = verify_ait_part1(fsck_primary);
		primary_inode_stamp = agg_recptr->inode_stamp;
		if (intermed_rc < 0) {
			/* something fatal */
			vsait_rc = intermed_rc;
		} else if (intermed_rc > 0) {
			/* primary table part 1 is bad */
			primary_part1_bad = -1;
		} else {
			/* primary table, part 1 is good */
			primary_part1_good = -1;
			intermed_rc = verify_ait_part2(fsck_primary);
			if (intermed_rc < 0) {
				/* something fatal */
				vsait_rc = intermed_rc;
			} else if (intermed_rc > 0) {
				/* primary table part 2 is bad */
				primary_part2_bad = -1;
				vsait_rc = backout_ait_part1(fsck_primary);
			} else {
				/* primary table, part 2 is good */
				primary_part2_good = -1;
			}
		}
	}
	/*
	 * if can't have both part 1 and part 2 from the primary, try for
	 * part 1 and part 2 both from the secondary aggregate inode table
	 */
	if ((vsait_rc == FSCK_OK) && (primary_part1_bad || primary_part2_bad)) {
		/* go for secondary ait */
		intermed_rc = ait_special_read_ext1(fsck_secondary);
		if (intermed_rc != FSCK_OK) {
			/* don't have it in the buffer */
			secondary_part1_bad = -1;
			secondary_part2_bad = -1;
			cant_read_secondary = -1;
		} else {
			/* got the 1st extent of secondary AIT into the inode buffer */
			agg_recptr->inode_stamp = unknown_stamp;
			intermed_rc = verify_ait_part1(fsck_secondary);
			secondary_inode_stamp = agg_recptr->inode_stamp;
			if (intermed_rc < 0) {
				/* something fatal */
				vsait_rc = intermed_rc;
			} else if (intermed_rc > 0) {
				/* secondary table part 1 is bad */
				secondary_part1_bad = 1;
			} else {
				/* secondary table, part 1 is good */
				secondary_part1_good = 1;
				intermed_rc = verify_ait_part2(fsck_secondary);
				if (intermed_rc < 0) {
					/* something fatal */
					vsait_rc = intermed_rc;
				} else if (intermed_rc > 0) {
					/* secondary table part 2 is bad */
					secondary_part2_bad = 1;
					vsait_rc =
					    backout_ait_part1(fsck_secondary);
				} else {
					/* secondary table, part 2 is good */
					secondary_part2_good = 1;
				}
			}
		}
	}
	if ((vsait_rc == FSCK_OK) && (primary_part1_good && primary_part2_good)) {
		/* normal case, nothing amiss */
		agg_recptr->primary_ait_4part1 = 1;
		agg_recptr->primary_ait_4part2 = 1;
		agg_recptr->inode_stamp = primary_inode_stamp;
	} else if ((vsait_rc == FSCK_OK)
		   && (secondary_part1_good && secondary_part2_good)) {
		/* first safety net held up */
		agg_recptr->primary_ait_4part1 = 0;
		agg_recptr->primary_ait_4part2 = 0;
		agg_recptr->inode_stamp = secondary_inode_stamp;
	} else {
		/* multiple points of failure. */
		/*
		 * try to go on by using part1 from one table and part 2 from the other
		 */
		if (vsait_rc == FSCK_OK) {
			/* nothing fatal */
			if (primary_part1_good && (!secondary_part2_good)
			    && (!secondary_part2_bad)) {
				/*
				 * primary part 1 is good and haven't checked
				 * secondary part 2 yet
				 */
				agg_recptr->inode_stamp = primary_inode_stamp;
				intermed_rc =
				    ait_special_read_ext1(fsck_primary);
				if (intermed_rc == FSCK_OK) {
					vsait_rc =
					    record_ait_part1_again
					    (fsck_primary);
				} else {
					vsait_rc = FSCK_FAILED_CANTREADAITEXT4;
				}
				if (vsait_rc == FSCK_OK) {
					/* primary part1 re-recorded ok */
					intermed_rc =
					    ait_special_read_ext1
					    (fsck_secondary);
					if (intermed_rc != FSCK_OK) {
						/* didn't get it */
						secondary_part2_bad = 1;
						cant_read_secondary = -1;
					} else {
						/* got the 1st extent of secondary AIT into the buffer */
						intermed_rc =
						    verify_ait_part2
						    (fsck_secondary);
						if (intermed_rc < 0) {
							/* something fatal */
							vsait_rc = intermed_rc;
						} else if (intermed_rc > 0) {
							/* secondary table part 2 is bad */
							secondary_part2_bad = 1;
						} else {
							/* secondary table, part 2 is good */
							secondary_part2_good =
							    -1;
						}
					}
				}
			} else if (secondary_part1_good && (!primary_part2_good)
				   && (!primary_part2_bad)) {
				/*
				 * secondary part 1 is good and haven't
				 * checked primary part 2 yet
				 */
				agg_recptr->inode_stamp = secondary_inode_stamp;
				intermed_rc =
				    ait_special_read_ext1(fsck_primary);
				if (intermed_rc != FSCK_OK) {
					/* didn't get it */
					primary_part2_bad = -1;
					cant_read_primary = -1;
				} else {
					/* got the 1st extent of primary AIT into the buffer */
					intermed_rc =
					    verify_ait_part2(fsck_primary);
					if (intermed_rc < 0) {
						/* something fatal */
						vsait_rc = intermed_rc;
					} else if (intermed_rc > 0) {
						/* primary table part 2 is bad */
						primary_part2_bad = 1;
					} else {
						/* primary table, part 2 is good */
						primary_part2_good = 1;
						intermed_rc =
						    ait_special_read_ext1
						    (fsck_secondary);
						if (intermed_rc == FSCK_OK) {
							vsait_rc =
							    record_ait_part1_again
							    (fsck_secondary);
						} else {
							vsait_rc =
							    FSCK_FAILED_CANTREADAITEXT5;
						}
					}
				}
			}
		}
		if (vsait_rc == FSCK_OK) {
			if (primary_part1_good && secondary_part2_good) {
				agg_recptr->primary_ait_4part1 = 1;
				agg_recptr->primary_ait_4part2 = 0;
			} else if (secondary_part1_good && primary_part2_good) {
				agg_recptr->primary_ait_4part1 = 0;
				agg_recptr->primary_ait_4part2 = 1;
			} else {
				/* either both have bad part 1 or both have bad part 2 */
				vsait_rc = FSCK_FAILED_BOTHAITBAD;
			}
		}
	}
	if (vsait_rc == FSCK_OK) {
		/*
		 * get a record as placeholder for the reserved
		 * aggregate inode
		 */
		vsait_rc =
		    get_inorecptr(aggregate_inode, alloc_ifnull, 0, &inorecptr);
		if ((vsait_rc == FSCK_OK) && (inorecptr == NULL)) {
			vsait_rc = FSCK_INTERNAL_ERROR_36;
			fsck_send_msg(fsck_INTERNALERROR, vsait_rc, 0, 0, 0);
		} else if (vsait_rc == FSCK_OK) {
			/* got the record */
			inorecptr->inode_type = metadata_inode;
			inorecptr->in_use = 1;
		}
	} else {
		if (cant_read_primary && cant_read_secondary) {
			/* this is fatal */
			vsait_rc = FSCK_FAILED_CANTREADAITS;
		}
	}
	/*
	 * Deal with the Aggregate Inode Map (and Table) not chosen
	 *
	 * If we're processing read-only and the primary versions are
	 * ok, we need to verify that the secondary versions are
	 * correctly redundant to the primary versions.
	 */
	if (vsait_rc == FSCK_OK) {
		/*  a table is chosen */
		if (agg_recptr->processing_readwrite) {
			/*
			 * have write access so we'll be refreshing
			 * the redundant version later on -- for now
			 * just reserve the blocks for it.
			 */
			vsait_rc = record_other_ait();
		} else {
			/* processing read-only */
			if (primary_part1_good && primary_part2_good) {
				/*
				 * need to verify that the secondary table and
				 * map are correct redundant copies of the
				 * primary table and map.
				 */
				vsait_rc = AIS_redundancy_check();
			} else {
				/* either part1 or part2 of primary are invalid */
				agg_recptr->ag_dirty = 1;
			}
			if (vsait_rc == FSCK_OK) {
				/* if it isn't correct a message has
				 * been issued.  Record the blocks they
				 * way that we were unable to record the
				 * occupy to avoid misleading error messages
				 * later when we verify the block allocation
				 * map.
				 */
				vsait_rc = record_other_ait();
			}
		}
	}
	/*
	 * report problems detected (if any)
	 */
	if (cant_read_primary) {
		fsck_send_msg(fsck_CANTREADAITP);
	} else if (primary_part1_bad || primary_part2_bad) {
		fsck_send_msg(fsck_ERRORSINAITP);
	}
	if (cant_read_secondary) {
		fsck_send_msg(fsck_CANTREADAITS);
	} else if (secondary_part1_bad || secondary_part2_bad) {
		fsck_send_msg(fsck_ERRORSINAITS);
	}
	if (cant_read_primary && cant_read_secondary) {
		agg_recptr->ag_dirty = 1;
		fsck_send_msg(fsck_CANTCONTINUE);
	} else if (primary_part1_bad && secondary_part1_bad) {
		agg_recptr->ag_dirty = 1;
		fsck_send_msg(fsck_CANTCONTINUE);
	} else if (primary_part2_bad && secondary_part2_bad) {
		agg_recptr->ag_dirty = 1;
		fsck_send_msg(fsck_CANTCONTINUE);
	}
	return (vsait_rc);
}

/*****************************************************************************
 * NAME: validate_super
 *
 * FUNCTION:  This routine validates the JFS superblock currently in the
 *            buffer.  If any problem is detected, the which_superblock
 *            input parm is used to tailor the message issued to notify
 *            the user.
 *
 * PARAMETERS:
 *      which_super  - input - specifies the superblock which is in the
 *                             buffer { fsck_primary | fsck_secondary }
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int validate_super(int which_super)
{
	int vs_rc = 0;		/* assume the superblock is ok */
	int64_t bytes_on_device;
	int64_t agg_blks_in_aggreg = 0, agg_blks_on_device =
	    0, dev_blks_on_device;
	int64_t fsck_start_from_pxd, fsck_blkmap_start_blks;
	uint32_t fsck_length_from_pxd, fsck_blkmap_size_blks,
	    fsck_blkmap_size_pages;
	int64_t jlog_start_from_pxd;
	uint32_t jlog_length_from_pxd;
	int32_t agl2size;
	uint32_t expected_flag = JFS_GROUPCOMMIT;
	uint32_t agsize;
	int bad_bsize = 0;

	if (memcmp(sb_ptr->s_magic, JFS_MAGIC, sizeof (sb_ptr->s_magic)) != 0) {
		vs_rc = FSCK_BADSBMGC;
		fsck_send_msg(fsck_BADSBMGC, fsck_ref_msg(which_super));
	} else if (sb_ptr->s_version > JFS_VERSION) {
		vs_rc = FSCK_BADSBVRSN;
		fsck_send_msg(fsck_BADSBVRSN, fsck_ref_msg(which_super));
	} else {
		/* the magic number and version number are correct so it
		 * probably is a JFS superblock with the format we are expecting
		 */
		/* get physical device size */
		ujfs_get_dev_size(Dev_IOPort, &bytes_on_device);

		dev_blks_on_device = bytes_on_device / Dev_blksize;
		if (sb_ptr->s_pbsize != Dev_blksize) {
			vs_rc = FSCK_BADSBOTHR1;
			fsck_send_msg(fsck_BADSBOTHR, "1", fsck_ref_msg(which_super));
		}
		if (sb_ptr->s_l2pbsize != log2shift(Dev_blksize)) {
			vs_rc = FSCK_BADSBOTHR2;
			fsck_send_msg(fsck_BADSBOTHR, "2", fsck_ref_msg(which_super));
		}
		if (!inrange(sb_ptr->s_bsize, 512, 4096)) {
			bad_bsize = -1;
			vs_rc = FSCK_BADSBOTHR3;
			fsck_send_msg(fsck_BADSBBLSIZ, fsck_ref_msg(which_super));
		} else {
			/* else the filesystem block size is a legal value */
			if (sb_ptr->s_l2bsize != log2shift(sb_ptr->s_bsize)) {
				vs_rc = FSCK_BADSBOTHR4;
				fsck_send_msg(fsck_BADSBOTHR, "4", fsck_ref_msg(which_super));
			}
			if (sb_ptr->s_l2bfactor !=
			    log2shift(sb_ptr->s_bsize / Dev_blksize)) {
				vs_rc = FSCK_BADSBOTHR5;
				fsck_send_msg(fsck_BADSBOTHR, "5", fsck_ref_msg(which_super));
			}
			if (sb_ptr->s_bsize < Dev_blksize) {
				bad_bsize = -1;
				vs_rc = FSCK_BLSIZLTLVBLSIZ;
				fsck_send_msg(fsck_BLSIZLTLVBLSIZ, fsck_ref_msg(which_super));
			}
		}

		if (!bad_bsize) {
			/* the blocksize looks ok */
			agg_blks_on_device = bytes_on_device / sb_ptr->s_bsize;

			if (sb_ptr->s_size > dev_blks_on_device) {
				vs_rc = FSCK_BADSBFSSIZ;
				fsck_send_msg(fsck_BADSBFSSIZ, fsck_ref_msg(which_super));
			}
#ifdef	_JFS_DFS_LFS

			s_size_inbytes = sb_ptr->s_size * Dev_blksize;
			sum_inbytes = (int64_t) (sb_ptr->totalUsable * 1024) +
			    (int64_t) (sb_ptr->minFree * 1024);
			if ((sum_inbytes > s_size_inbytes) ||
			    ((s_size_inbytes - sum_inbytes) >= 1024)
			    ) {
				/* the sum is greater or the difference is at least 1K */
				vs_rc = FSCK_BADBLKCTTTL;
				fsck_send_msg(fsck_BADBLKCTTTL, fsck_ref_msg(which_super));
			}
#endif				/* _JFS_DFS_LFS */

			if (((sb_ptr->s_flag & JFS_OS2) == JFS_OS2)
			    || ((sb_ptr->s_flag & JFS_LINUX) == JFS_LINUX)) {
				/* must have JFS_OS2 or JFS_LINUX */
			} else {
				vs_rc = FSCK_BADSBOTHR6;
				fsck_send_msg(fsck_BADSBOTHR, "6", fsck_ref_msg(which_super));
			}

			if ((sb_ptr->s_flag & expected_flag) != expected_flag) {
				vs_rc = FSCK_BADSBOTHR6;
				fsck_send_msg(fsck_BADSBOTHR, "6", fsck_ref_msg(which_super));
			}
			if (sb_ptr->s_agsize < (1 << L2BPERDMAP)) {
				vs_rc = FSCK_BADSBAGSIZ;
				fsck_send_msg(fsck_BADSBAGSIZ, fsck_ref_msg(which_super));
			} else {
				/* else the alloc group size is possibly correct */
				agg_blks_in_aggreg =
				    sb_ptr->s_size * sb_ptr->s_pbsize /
				    sb_ptr->s_bsize;
				agl2size =
				    ujfs_getagl2size(agg_blks_in_aggreg,
						     sb_ptr->s_bsize);
				agsize = (int64_t) 1 << agl2size;
				if (sb_ptr->s_agsize != agsize) {
					vs_rc = FSCK_BADAGFSSIZ;
					fsck_send_msg(fsck_BADSBAGSIZ, fsck_ref_msg(which_super));
				}
			}
		}
		if (!vs_rc) {
			/*
			 * check out the fsck in-aggregate workspace
			 */
			fsck_length_from_pxd = lengthPXD(&(sb_ptr->s_fsckpxd));
			fsck_start_from_pxd = addressPXD(&(sb_ptr->s_fsckpxd));
			agg_blks_in_aggreg = fsck_length_from_pxd +
			    (sb_ptr->s_size * sb_ptr->s_pbsize /
			     sb_ptr->s_bsize);
			if (agg_blks_in_aggreg > agg_blks_on_device) {
				/* wsp length is bad */
				vs_rc = FSCK_BADSBFWSL1;
				fsck_send_msg(fsck_BADSBFWSL1, fsck_ref_msg(which_super));
			} else {
				/* wsp length is plausible */
				fsck_blkmap_size_pages =
				    ((agg_blks_in_aggreg +
				      (BITSPERPAGE - 1)) / BITSPERPAGE) + 1 +
				    50;
				/* size in aggregate blocks */
				fsck_blkmap_size_blks =
				    ((int64_t)fsck_blkmap_size_pages <<
				     L2PSIZE) /
				    sb_ptr->s_bsize;
				/*
				 * aggregate block offset of the fsck
				 * workspace in the aggregate.
				 */
				fsck_blkmap_start_blks =
				    agg_blks_in_aggreg - fsck_blkmap_size_blks;
				if (fsck_length_from_pxd !=
				    fsck_blkmap_size_blks) {
					/*
					 * length of fsck in-aggregate workspace
					 * is incorrect
					 */
					vs_rc = FSCK_BADSBFWSL;
					fsck_send_msg(fsck_BADSBFWSL, fsck_ref_msg(which_super));
				}
				if (fsck_start_from_pxd !=
				    fsck_blkmap_start_blks) {
					/*
					 * address of fsck in-aggregate workspace
					 * is incorrect
					 */
					vs_rc = FSCK_BADSBFWSA;
					fsck_send_msg(fsck_BADSBFWSA, fsck_ref_msg(which_super));
				}
			}
		}
		if (!vs_rc) {
			/*
			 * check out the in-aggregate journal log
			 *
			 * if there is one it starts at the end of the fsck
			 * in-aggregate workspace.
			 */
			jlog_length_from_pxd = lengthPXD(&(sb_ptr->s_logpxd));
			jlog_start_from_pxd = addressPXD(&(sb_ptr->s_logpxd));
			if (jlog_start_from_pxd != 0) {
				/* there's one in there */
				if (jlog_start_from_pxd != agg_blks_in_aggreg) {
					/*
					 * address of in-aggregate journal log
					 * is incorrect
					 */
					vs_rc = FSCK_BADSBFJLA;
					fsck_send_msg(fsck_BADSBFJLA, fsck_ref_msg(which_super));
				}
				agg_blks_in_aggreg += jlog_length_from_pxd;
				if (agg_blks_in_aggreg > agg_blks_on_device) {
					/* log length is bad */
					vs_rc = FSCK_BADSBFJLL;
					fsck_send_msg(fsck_BADSBFJLL, fsck_ref_msg(which_super));
				}
			}
		}
		if (!vs_rc) {
			/*
			 * check out the descriptors for
			 * the Secondary Agg Inode Table and the Secondary Agg Inode Map
			 */
			vs_rc = validate_super_2ndaryAI(which_super);
		}
	}
	/* end else the magic number and version number are correct so it
	 * probably is a JFS superblock with the format we are expecting
	 */
	return (vs_rc);
}

/*****************************************************************************
 * NAME: validate_super_2ndaryAI
 *
 * FUNCTION:  This routine validates, in the current superblock, the
 *            descriptors for the Secondary Aggregate Inode Table and
 *            the Secondary Aggregate Inode Map.
 *
 *            If any problem is detected, the which_superblock input parm
 *            is used to tailor the message issued to notify the user.
 *
 * PARAMETERS:
 *      which_super  - input - specifies the superblock which is in the
 *                             buffer { fsck_primary | fsck_secondary }
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int validate_super_2ndaryAI(int which_super)
{
	int vs2AI_rc = 0;	/* assume the superblock is ok */
	int intermed_rc = 0;
	int32_t AIM_bytesize, AIT_bytesize, selfIno_bytesize;
	int32_t expected_AIM_bytesize, expected_AIT_bytesize;
	int64_t AIM_byte_addr = 0, AIT_byte_addr = 0, fsckwsp_addr = 0;
	int64_t selfIno_addr = 0, other_sb_AIM_byte_addr =
	    0, other_sb_AIT_byte_addr = 0;
	int64_t byte_addr_diff, offset_other_super;
	struct dinode *AggInodes = NULL;
	uint32_t bufsize, datasize;
	xtpage_t *selfIno_xtree;
	xad_t *selfIno_xad;
	struct superblock *other_sb_ptr;

	bufsize = PGSPERIEXT * BYTESPERPAGE;
	expected_AIM_bytesize = 2 * BYTESPERPAGE;
	AIM_bytesize = lengthPXD(&(sb_ptr->s_aim2)) * sb_ptr->s_bsize;
	if (AIM_bytesize != expected_AIM_bytesize) {
		vs2AI_rc = FSCK_BADSBOTHR7;
		fsck_send_msg(fsck_BADSBOTHR, "7", fsck_ref_msg(which_super));
		goto vs2AI_exit;
	}

	/* AIM size ok */
	expected_AIT_bytesize = 4 * BYTESPERPAGE;
	AIT_bytesize = lengthPXD(&(sb_ptr->s_ait2)) * sb_ptr->s_bsize;
	if (AIT_bytesize != expected_AIT_bytesize) {
		vs2AI_rc = FSCK_BADSBOTHR8;
		fsck_send_msg(fsck_BADSBOTHR, "8", fsck_ref_msg(which_super));
		goto vs2AI_exit;
	}

	/* AIT size ok */
	AIM_byte_addr = addressPXD(&(sb_ptr->s_aim2)) * sb_ptr->s_bsize;
	AIT_byte_addr = addressPXD(&(sb_ptr->s_ait2)) * sb_ptr->s_bsize;
	byte_addr_diff = AIT_byte_addr - AIM_byte_addr;
	if (byte_addr_diff != AIM_bytesize) {
		vs2AI_rc = FSCK_BADSBOTHR9;
		fsck_send_msg(fsck_BADSBOTHR, "9", fsck_ref_msg(which_super));
		goto vs2AI_exit;
	}

	/* relative addrs of AIT and AIM are ok */
	fsckwsp_addr = addressPXD(&(sb_ptr->s_fsckpxd)) * sb_ptr->s_bsize;
	byte_addr_diff = fsckwsp_addr - AIT_byte_addr;
	if (byte_addr_diff <= AIT_bytesize) {
		vs2AI_rc = FSCK_BADSBOTHR10;
		fsck_send_msg(fsck_BADSBOTHR, "10", fsck_ref_msg(which_super));
		goto vs2AI_exit;
	}

	/* relative addrs of fsck workspace and AIT are possible */
	/*
	 * Allocate a buffer then read in the alleged secondary
	 * AIT.  The self inode should describe the AIM.
	 */
	agg_recptr->vlarge_current_use = USED_FOR_SUPER_VALIDATION;
	AggInodes = (struct dinode *) agg_recptr->vlarge_buf_ptr;
	intermed_rc = readwrite_device(AIT_byte_addr, bufsize, &datasize,
				       (void *) AggInodes, fsck_READ);
	if (intermed_rc != FSCK_OK) {
		vs2AI_rc = FSCK_BADSBOTHR11;
		fsck_send_msg(fsck_BADSBOTHR, "11", fsck_ref_msg(which_super));
		goto vs2AI_exit;
	}

	/* alleged secondary AIT is in the buffer */
	/*
	 * Check the "data" extent in the self inode in the
	 * alleged Secondary Aggregate Inode Table.
	 *
	 * If it should describes the AIM, all is well.
	 *
	 * If it does NOT describe the AIM, then it might be a bad
	 * superblock, and it might be a bad AIT.  Read the other
	 * superblock and compare the AIM and AIT descriptors.
	 * If they match, assume the superblock is ok but the AIT
	 * is bad.
	 * N.B. we can fix a bad AIM and a bad AIT.  we cannot do
	 *       even continue fsck without a good superblock.
	 */
	selfIno_xtree = (xtpage_t *) &(AggInodes[AGGREGATE_I].di_btroot);
	selfIno_xad = &(selfIno_xtree->xad[XTENTRYSTART]);
	selfIno_bytesize = lengthXAD(selfIno_xad) * sb_ptr->s_bsize;
	selfIno_addr = addressXAD(selfIno_xad) * sb_ptr->s_bsize;
	if ((selfIno_bytesize != AIM_bytesize) ||
	    (selfIno_addr != AIM_byte_addr)) {
		/* inode doesn't describe AIM */
		if (which_super == fsck_primary) {
			offset_other_super = SUPER2_OFF;
		} else {
			offset_other_super = SUPER1_OFF;
		}
		other_sb_ptr = (struct superblock *) AggInodes;
		intermed_rc = readwrite_device(offset_other_super,
					       SIZE_OF_SUPER, &datasize,
					       (void *)other_sb_ptr, fsck_READ);
		if (intermed_rc != FSCK_OK) {
			vs2AI_rc = FSCK_BADSBOTHR12;
			fsck_send_msg(fsck_BADSBOTHR, "12", fsck_ref_msg(which_super));
		} else {
			/* other superblock has been read */
			other_sb_AIM_byte_addr =
			    addressPXD(&other_sb_ptr->s_aim2) * sb_ptr->s_bsize;
			other_sb_AIT_byte_addr =
			    addressPXD(&other_sb_ptr->s_ait2) * sb_ptr->s_bsize;
			if ((AIM_byte_addr != other_sb_AIM_byte_addr) ||
			    (AIT_byte_addr != other_sb_AIT_byte_addr)) {
				vs2AI_rc = FSCK_BADSBOTHR13;
				fsck_send_msg(fsck_BADSBOTHR, "13", fsck_ref_msg(which_super));
			}
		}
	}

      vs2AI_exit:
	agg_recptr->vlarge_current_use = NOT_CURRENTLY_USED;

	return (vs2AI_rc);
}

/*****************************************************************************
 * NAME: verify_agg_fileset_inode
 *
 * FUNCTION:  Verify the structures associated with and the content of
 *            the aggregate fileset inode, aggregate inode 16,  whose
 *            data is the Fileset Inode Map.
 *
 * PARAMETERS:
 *      inoptr        - input - pointer to the inode in an fsck buffer
 *      inoidx        - input - ordinal number of the inode (i.e., inode number
 *                              as an int32_t)
 *      which_ait     - input - the Aggregate Inode Table on which to perform
 *                              the function.  { fsck_primary | fsck_secondary }
 *      msg_info_ptr  - input - pointer to a data area with data needed to
 *                              issue messages about the inode
 *
 * NOTES:  o Inode number and inode index are input parameters to facilitate
 *           multiple filesets per aggregate in a later release.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int verify_agg_fileset_inode(struct dinode *inoptr,
			     uint32_t inoidx,
			     int which_ait,
			     struct fsck_ino_msg_info *msg_info_ptr)
{
	int vafsi_rc = FSCK_OK;
	int inode_invalid = 0;
	int ixpxd_unequal = 0;
	struct fsck_inode_record *inorecptr;
	int aggregate_inode = -1;
	int alloc_ifnull = -1;
	uint32_t unknown_stamp = (uint32_t) (-1);

	if ((agg_recptr->inode_stamp != unknown_stamp)
	    && (inoptr->di_inostamp != agg_recptr->inode_stamp)) {
		/*
		 * we got a key from the corresponding AIT but
		 * the one in this inode doesn't match -- so
		 * this is either trashed or residual
		 */
		inode_invalid = 1;
		fsck_send_msg(fsck_BADINOSTAMP,
			      fsck_ref_msg(msg_info_ptr->msg_inotyp),
			      fsck_ref_msg(msg_info_ptr->msg_inopfx),
			      msg_info_ptr->msg_inonum);
	}
	if (inoptr->di_fileset != AGGREGATE_I) {
		/* unexpected fileset # */
		inode_invalid = 1;
		fsck_send_msg(fsck_BADINOOTHR, "1",
			      fsck_ref_msg(msg_info_ptr->msg_inotyp),
			      fsck_ref_msg(msg_info_ptr->msg_inopfx),
			      msg_info_ptr->msg_inonum);
	}
	if (inoptr->di_number != inoidx) {
		/* unexpected inode # */
		inode_invalid = 1;
		fsck_send_msg(fsck_BADINOOTHR, "2",
			      fsck_ref_msg(msg_info_ptr->msg_inotyp),
			      fsck_ref_msg(msg_info_ptr->msg_inopfx),
			      msg_info_ptr->msg_inonum);
	}
	if (inoptr->di_gen != 1) {
		/* incorrect generation # */
		inode_invalid = 1;
		fsck_send_msg(fsck_BADINOOTHR, "2a",
			      fsck_ref_msg(msg_info_ptr->msg_inotyp),
			      fsck_ref_msg(msg_info_ptr->msg_inopfx),
			      msg_info_ptr->msg_inonum);
	}
	ixpxd_unequal =
	    memcmp((void *) &(inoptr->di_ixpxd),
		   (void *) &(agg_recptr->ino_ixpxd), sizeof (pxd_t));
	if (ixpxd_unequal) {
		/* incorrect extent descriptor */
		inode_invalid = 1;
		fsck_send_msg(fsck_BADINOOTHR, "3",
			      fsck_ref_msg(msg_info_ptr->msg_inotyp),
			      fsck_ref_msg(msg_info_ptr->msg_inopfx),
			      msg_info_ptr->msg_inonum);
	}
	if ((inoptr->di_mode & (IFJOURNAL | IFREG)) != (IFJOURNAL | IFREG)) {
		/* incorrect mode */
		inode_invalid = 1;
		fsck_send_msg(fsck_BADINOOTHR, "4",
			      fsck_ref_msg(msg_info_ptr->msg_inotyp),
			      fsck_ref_msg(msg_info_ptr->msg_inopfx),
			      msg_info_ptr->msg_inonum);
	}
	if (inoptr->di_nlink != 1) {
		/* incorrect # of links */
		inode_invalid = 1;
		fsck_send_msg(fsck_BADINOOTHR, "5",
			      fsck_ref_msg(msg_info_ptr->msg_inotyp),
			      fsck_ref_msg(msg_info_ptr->msg_inopfx),
			      msg_info_ptr->msg_inonum);
	}
	if (!(inoptr->di_dxd.flag & BT_ROOT)) {
		/* not flagged as B+ Tree root */
		inode_invalid = 1;
		fsck_send_msg(fsck_BADINOOTHR, "6",
			      fsck_ref_msg(msg_info_ptr->msg_inotyp),
			      fsck_ref_msg(msg_info_ptr->msg_inopfx),
			      msg_info_ptr->msg_inonum);
	}
	/*
	 * If any problems detected so far, don't bother trying to validate
	 * the B+ Tree
	 */
	if (!inode_invalid) {
		/* self inode looks ok so far */
		vafsi_rc =
		    get_inorecptr(aggregate_inode, alloc_ifnull, inoidx,
				  &inorecptr);
		if ((vafsi_rc == FSCK_OK) && (inorecptr == NULL)) {
			vafsi_rc = FSCK_INTERNAL_ERROR_37;
			fsck_send_msg(fsck_INTERNALERROR, vafsi_rc, 0, 0, 0);
		} else if (vafsi_rc == FSCK_OK) {
			vafsi_rc =
			    verify_metadata_data(inoptr, inoidx, inorecptr,
						 msg_info_ptr);
			if (inorecptr->ignore_alloc_blks
			    || (vafsi_rc != FSCK_OK)) {
				inode_invalid = -1;
			}
		}
	}
	/*
	 * wrap it all up for this inode
	 */
	if ((inode_invalid) && (vafsi_rc == FSCK_OK)) {
		vafsi_rc = FSCK_AGGFSINOBAD;
	}
	if (inode_invalid) {
		/* this one's corrupt */
		if (which_ait == fsck_primary) {
			fsck_send_msg(fsck_BADMETAINOP,
				      fsck_ref_msg(msg_info_ptr->msg_inopfx),
				      msg_info_ptr->msg_inonum);
		} else {
			fsck_send_msg(fsck_BADMETAINOS,
				      fsck_ref_msg(msg_info_ptr->msg_inopfx),
				      msg_info_ptr->msg_inonum);
		}
	}
	return (vafsi_rc);
}

/*****************************************************************************
 * NAME: verify_ait_inode
 *
 * FUNCTION:  Verify the structures associated with and the content of
 *            the aggregate "self" inode, aggregate inode 1, whose
 *            data is the Fileset Inode Map.
 *
 * PARAMETERS:
 *      inoptr        - input - pointer to the inode in an fsck buffer
 *      which_ait     - input - the Aggregate Inode Table on which to perform
 *                              the function.  { fsck_primary | fsck_secondary }
 *      msg_info_ptr  - input - pointer to a data area with data needed to
 *                              issue messages about the inode
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int verify_ait_inode(struct dinode *inoptr, int which_ait,
		     struct fsck_ino_msg_info *msg_info_ptr)
{
	int vai_rc = FSCK_OK;
	int inode_invalid = 0;
	int ixpxd_unequal = 0;
	struct fsck_inode_record *inorecptr;
	int aggregate_inode = -1;
	int alloc_ifnull = -1;

	if (inoptr->di_fileset != AGGREGATE_I) {
		/* unexpected fileset # */
		inode_invalid = 1;
		fsck_send_msg(fsck_BADINOOTHR, "7",
			      fsck_ref_msg(msg_info_ptr->msg_inotyp),
			      fsck_ref_msg(msg_info_ptr->msg_inopfx),
			      msg_info_ptr->msg_inonum);
	}
	if (inoptr->di_fileset != AGGREGATE_I) {
		/* unexpected fileset # */
		inode_invalid = 1;
		fsck_send_msg(fsck_BADINOOTHR, "8",
			      fsck_ref_msg(msg_info_ptr->msg_inotyp),
			      fsck_ref_msg(msg_info_ptr->msg_inopfx),
			      msg_info_ptr->msg_inonum);
	}
	if (inoptr->di_number != AGGREGATE_I) {
		/* unexpected inode # */
		inode_invalid = 1;
		fsck_send_msg(fsck_BADINOOTHR, "9",
			      fsck_ref_msg(msg_info_ptr->msg_inotyp),
			      fsck_ref_msg(msg_info_ptr->msg_inopfx),
			      msg_info_ptr->msg_inonum);
	}
	if (inoptr->di_gen != 1) {
		/* incorrect generation # */
		inode_invalid = 1;
		fsck_send_msg(fsck_BADINOOTHR, "10",
			      fsck_ref_msg(msg_info_ptr->msg_inotyp),
			      fsck_ref_msg(msg_info_ptr->msg_inopfx),
			      msg_info_ptr->msg_inonum);
	}
	ixpxd_unequal =
	    memcmp((void *) &(inoptr->di_ixpxd),
		   (void *) &(agg_recptr->ino_ixpxd), (sizeof (pxd_t)));
	if (ixpxd_unequal) {
		/* incorrect extent descriptor */
		inode_invalid = 1;
		fsck_send_msg(fsck_BADINOOTHR, "11",
			      fsck_ref_msg(msg_info_ptr->msg_inotyp),
			      fsck_ref_msg(msg_info_ptr->msg_inopfx),
			      msg_info_ptr->msg_inonum);
	}
	if ((inoptr->di_mode & (IFJOURNAL | IFREG)) != (IFJOURNAL | IFREG)) {
		/* incorrect mode */
		inode_invalid = 1;
		fsck_send_msg(fsck_BADINOOTHR, "12",
			      fsck_ref_msg(msg_info_ptr->msg_inotyp),
			      fsck_ref_msg(msg_info_ptr->msg_inopfx),
			      msg_info_ptr->msg_inonum);
	}
	if (inoptr->di_nlink != 1) {
		/* incorrect # of links */
		inode_invalid = 1;
		fsck_send_msg(fsck_BADINOOTHR, "13",
			      fsck_ref_msg(msg_info_ptr->msg_inotyp),
			      fsck_ref_msg(msg_info_ptr->msg_inopfx),
			      msg_info_ptr->msg_inonum);
	}
	if (!(inoptr->di_dxd.flag & BT_ROOT)) {
		/* not flagged as B+ Tree root */
		inode_invalid = 1;
		fsck_send_msg(fsck_BADINOOTHR, "14",
			      fsck_ref_msg(msg_info_ptr->msg_inotyp),
			      fsck_ref_msg(msg_info_ptr->msg_inopfx),
			      msg_info_ptr->msg_inonum);
	}
	/*
	 * If any problems detected so far, don't bother trying to validate
	 * the B+ Tree
	 */
	if (!inode_invalid) {
		/* self inode looks ok so far */
		vai_rc =
		    get_inorecptr(aggregate_inode, alloc_ifnull, AGGREGATE_I,
				  &inorecptr);
		if ((vai_rc == FSCK_OK) && (inorecptr == NULL)) {
			vai_rc = FSCK_INTERNAL_ERROR_38;
			fsck_send_msg(fsck_INTERNALERROR, vai_rc, 0, 0, 0);
		} else if (vai_rc == FSCK_OK) {
			vai_rc =
			    verify_metadata_data(inoptr, AGGREGATE_I, inorecptr,
						 msg_info_ptr);
			if (inorecptr->ignore_alloc_blks || (vai_rc != FSCK_OK)) {
				inode_invalid = -1;
			}
		}
	}
	if ((!inode_invalid) && (vai_rc == FSCK_OK)) {
		agg_recptr->inode_stamp = inoptr->di_inostamp;
	}
	/*
	 * wrap it all up for this inode
	 */
	if ((inode_invalid) && (vai_rc == FSCK_OK)) {
		vai_rc = FSCK_AGGAITINOBAD;
	}
	if (inode_invalid) {
		/* this one's corrupt */
		if (which_ait == fsck_primary) {
			fsck_send_msg(fsck_BADMETAINOP,
				      fsck_ref_msg(msg_info_ptr->msg_inopfx),
				      msg_info_ptr->msg_inonum);
		} else {
			fsck_send_msg(fsck_BADMETAINOS,
				      fsck_ref_msg(msg_info_ptr->msg_inopfx),
				      msg_info_ptr->msg_inonum);
		}
	}
	return (vai_rc);
}

/*****************************************************************************
 * NAME: verify_ait_part1
 *
 * FUNCTION:  Validate the inodes in "part 1" (inodes 0 through 15) of
 *            the specified Aggregate Inode Table.
 *
 * PARAMETERS:
 *      which_ait  - input - the Aggregate Inode Table on which to perform
 *                           the function.  { fsck_primary | fsck_secondary }
 *
 * NOTES:  o The caller to this routine must ensure that the
 *           calls made by verify_ait_part1 to inode_get()
 *           will not require device I/O.
 *           That is, the caller must ensure that the aggregate
 *           inode extent containing part1 of the target AIT
 *           resides in the fsck inode buffer before calling
 *           this routine.  (See inode_get() for more info.)
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int verify_ait_part1(int which_ait)
{
	int vaitp1_rc = FSCK_OK;
	int intermed_rc = FSCK_OK;
	struct fsck_ino_msg_info ino_msg_info;
	struct fsck_ino_msg_info *msg_info_ptr;
	struct fsck_inode_record *inorecptr;
	uint32_t ino_idx;
	struct dinode *ino_ptr;
	int aggregate_inode = -1;	/* going for aggregate inodes only */
	int alloc_ifnull = 0;

	msg_info_ptr = &ino_msg_info;
	msg_info_ptr->msg_inopfx = fsck_aggr_inode;
	msg_info_ptr->msg_inotyp = fsck_metadata;

	/* try for the self inode */
	ino_idx = AGGREGATE_I;
	intermed_rc = inode_get(aggregate_inode, which_ait, ino_idx, &ino_ptr);
	if (intermed_rc != FSCK_OK) {
		/* can't get inode  */
		vaitp1_rc = FSCK_CANTREADSELFINO;
		goto vaitp1_exit;
	}

	/* got aggregate inode  */
	msg_info_ptr->msg_inonum = AGGREGATE_I;
	intermed_rc =
	    verify_ait_inode(ino_ptr, which_ait, msg_info_ptr);
	if (intermed_rc == FSCK_OK) {
		vaitp1_rc = get_inorecptr(aggregate_inode, alloc_ifnull,
					  ino_idx, &inorecptr);
		if ((vaitp1_rc == FSCK_OK) && (inorecptr == NULL)) {
			vaitp1_rc = FSCK_INTERNAL_ERROR_27;
			fsck_send_msg(fsck_INTERNALERROR, vaitp1_rc, 0, 0, 0);
		} else if ((vaitp1_rc == FSCK_OK) &&
			   (inorecptr->involved_in_dups)) {
			/*
			 * duplicate allocation(s) detected
			 * while validating the inode
			 */
			vaitp1_rc = unrecord_valid_inode(ino_ptr, AGGREGATE_I,
						inorecptr, msg_info_ptr);
			if (vaitp1_rc == FSCK_OK) {
				vaitp1_rc = FSCK_DUPMDBLKREF;
			}
		}
	}
	if (vaitp1_rc != FSCK_OK)
		goto vaitp1_exit;

	if (intermed_rc != FSCK_OK) {
		/* self inode is bad */
		vaitp1_rc = FSCK_SELFINOBAD;
		goto vaitp1_exit;
	}

	/* self inode is good */
	/* try for the blockmap inode */
	ino_idx = BMAP_I;
	intermed_rc = inode_get(aggregate_inode, which_ait, ino_idx, &ino_ptr);
	if (intermed_rc != FSCK_OK) {
		/* can't get block map inode */
		vaitp1_rc = backout_valid_agg_inode(which_ait, AGGREGATE_I,
						    msg_info_ptr);
		if (vaitp1_rc == FSCK_OK) {
			vaitp1_rc = FSCK_CANTREADBMINO;
		}
		goto vaitp1_exit;
	}

	/* got block map inode */
	msg_info_ptr->msg_inonum = BMAP_I;
	intermed_rc = verify_bmap_inode(ino_ptr, which_ait, msg_info_ptr);
	if (intermed_rc == FSCK_OK) {
		vaitp1_rc = get_inorecptr(aggregate_inode, alloc_ifnull, BMAP_I,
					  &inorecptr);
		if ((vaitp1_rc == FSCK_OK) && (inorecptr == NULL)) {
			vaitp1_rc = FSCK_INTERNAL_ERROR_30;
			fsck_send_msg(fsck_INTERNALERROR, vaitp1_rc, 0, 0, 0);
		} else if ((vaitp1_rc == FSCK_OK) &&
			   (inorecptr->involved_in_dups)) {
			/*
			 * duplicate allocation(s) detected
			 * while validating the inode
			 */
			vaitp1_rc = unrecord_valid_inode(ino_ptr, BMAP_I,
						    inorecptr, msg_info_ptr);
			if (vaitp1_rc == FSCK_OK) {
				vaitp1_rc = FSCK_DUPMDBLKREF;
			}
		}
	}
	if ((intermed_rc != FSCK_OK) && (vaitp1_rc == FSCK_OK)) {
		/* block map inode is bad */
		msg_info_ptr->msg_inonum = AGGREGATE_I;
		vaitp1_rc = backout_valid_agg_inode(which_ait, AGGREGATE_I,
						    msg_info_ptr);
		if (vaitp1_rc == FSCK_OK) {
			vaitp1_rc = FSCK_BMINOBAD;
		}
		goto vaitp1_exit;
	}

	if (vaitp1_rc != FSCK_OK)
		goto vaitp1_exit;

	/* block map inode is good */
	/* try for the journal inode */
	ino_idx = LOG_I;
	intermed_rc = inode_get(aggregate_inode, which_ait, ino_idx, &ino_ptr);
	if (intermed_rc != FSCK_OK) {
		/* can't get journal inode */
		if (vaitp1_rc == FSCK_OK) {
			msg_info_ptr->msg_inonum = AGGREGATE_I;
			vaitp1_rc = backout_valid_agg_inode(which_ait,
					    AGGREGATE_I, msg_info_ptr);
			if (vaitp1_rc == FSCK_OK) {
				msg_info_ptr->msg_inonum = BMAP_I;
				vaitp1_rc = backout_valid_agg_inode(which_ait,
							BMAP_I, msg_info_ptr);
				if (vaitp1_rc == FSCK_OK) {
					vaitp1_rc = FSCK_CANTREADLOGINO;
				}
			}
		}
		goto vaitp1_exit;
	}

	/* got journal inode */
	msg_info_ptr->msg_inonum = LOG_I;
	intermed_rc = verify_log_inode(ino_ptr, which_ait, msg_info_ptr);
	if (intermed_rc == FSCK_OK) {
		vaitp1_rc = get_inorecptr(aggregate_inode, alloc_ifnull, LOG_I,
					  &inorecptr);
		if ((vaitp1_rc == FSCK_OK) && (inorecptr == NULL)) {
			vaitp1_rc = FSCK_INTERNAL_ERROR_31;
			fsck_send_msg(fsck_INTERNALERROR, vaitp1_rc, 0, 0, 0);
		} else if ((vaitp1_rc == FSCK_OK) &&
			   (inorecptr->involved_in_dups)) {
			/*
			 * duplicate allocation(s) detected
			 * while validating the inode
			 */
			vaitp1_rc = unrecord_valid_inode(ino_ptr, LOG_I,
						    inorecptr, msg_info_ptr);
			if (vaitp1_rc == FSCK_OK) {
				vaitp1_rc = FSCK_DUPMDBLKREF;
			}
		}
	}
	if ((vaitp1_rc == FSCK_OK) && (intermed_rc != FSCK_OK)) {
		/* journal inode is bad */
		msg_info_ptr->msg_inonum = AGGREGATE_I;
		vaitp1_rc = backout_valid_agg_inode(which_ait, AGGREGATE_I,
						    msg_info_ptr);
		if (vaitp1_rc == FSCK_OK) {
			msg_info_ptr->msg_inonum = BMAP_I;
			vaitp1_rc = backout_valid_agg_inode(which_ait, BMAP_I,
							    msg_info_ptr);
		}
		if (vaitp1_rc == FSCK_OK) {
			vaitp1_rc = FSCK_LOGINOBAD;
		}
		goto vaitp1_exit;
	}

	/* journal inode is good */
	/* try for the bad block inode */
	ino_idx = BADBLOCK_I;
	intermed_rc = inode_get(aggregate_inode, which_ait, ino_idx, &ino_ptr);
	if (intermed_rc != FSCK_OK) {
		/* can't get bad block inode */
		if (vaitp1_rc == FSCK_OK) {
			msg_info_ptr->msg_inonum = AGGREGATE_I;
			vaitp1_rc = backout_valid_agg_inode(which_ait,
						AGGREGATE_I, msg_info_ptr);
			if (vaitp1_rc == FSCK_OK) {
				msg_info_ptr->msg_inonum = BMAP_I;
				vaitp1_rc = backout_valid_agg_inode(which_ait,
							BMAP_I, msg_info_ptr);
				if (vaitp1_rc == FSCK_OK) {
					msg_info_ptr->msg_inonum = LOG_I;
					vaitp1_rc =
					    backout_valid_agg_inode(which_ait,
							LOG_I, msg_info_ptr);
					if (vaitp1_rc == FSCK_OK) {
						vaitp1_rc = FSCK_CANTREADBBINO;
					}
				}
			}
		}
		goto vaitp1_exit;
	}

	/* got bad block inode */
	msg_info_ptr->msg_inonum = BADBLOCK_I;
	intermed_rc = verify_badblk_inode(ino_ptr, which_ait, msg_info_ptr);
	if (intermed_rc == FSCK_OK) {
		vaitp1_rc = get_inorecptr(aggregate_inode, alloc_ifnull,
					  BADBLOCK_I, &inorecptr);
		if ((vaitp1_rc == FSCK_OK) && (inorecptr == NULL)) {
			vaitp1_rc = FSCK_INTERNAL_ERROR_56;
			fsck_send_msg(fsck_INTERNALERROR, vaitp1_rc, 0, 0, 0);
		} else if ((vaitp1_rc == FSCK_OK) &&
			   (inorecptr->involved_in_dups)) {
			/*
			 * duplicate allocation(s) detected
			 * while validating the inode
			 */
			vaitp1_rc = unrecord_valid_inode(ino_ptr,  BADBLOCK_I,
						    inorecptr,  msg_info_ptr);
			if (vaitp1_rc == FSCK_OK) {
				vaitp1_rc = FSCK_DUPMDBLKREF;
			}
		}
	}
	if ((vaitp1_rc == FSCK_OK) && (intermed_rc != FSCK_OK)) {
		/* bad block inode is bad */
		msg_info_ptr->msg_inonum = AGGREGATE_I;
		vaitp1_rc = backout_valid_agg_inode(which_ait,  AGGREGATE_I,
						    msg_info_ptr);
		if (vaitp1_rc == FSCK_OK) {
			msg_info_ptr->msg_inonum = BMAP_I;
			vaitp1_rc = backout_valid_agg_inode(which_ait, BMAP_I,
							    msg_info_ptr);
			if (vaitp1_rc == FSCK_OK) {
				msg_info_ptr->msg_inonum = LOG_I;
				vaitp1_rc = backout_valid_agg_inode(which_ait,
							LOG_I, msg_info_ptr);
				if (vaitp1_rc == FSCK_OK) {
					vaitp1_rc = FSCK_BBINOBAD;
				}
			}
		}
	}

      vaitp1_exit:
	return (vaitp1_rc);
}

/*****************************************************************************
 * NAME: verify_ait_part2
 *
 * FUNCTION:  Validate the inodes in "part 2" (inodes 16 through 31) of
 *            the specified Aggregate Inode Table.
 *
 * PARAMETERS:
 *      which_ait  - input - the Aggregate Inode Table on which to perform
 *                           the function.  { fsck_primary | fsck_secondary }
 *
 * NOTES:  o The caller to this routine must ensure that the
 *           calls made by verify_ait_part2 to inode_get()
 *           will not require device I/O.
 *           That is, the caller must ensure that the aggregate
 *           inode extent containing part1 of the target AIT
 *           resides in the fsck inode buffer before calling
 *           this routine.  (See inode_get() for more info.)
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int verify_ait_part2(int which_ait)
{
	int vaitp2_rc = FSCK_OK;
	int intermed_rc = FSCK_OK;
	struct fsck_ino_msg_info ino_msg_info;
	struct fsck_ino_msg_info *msg_info_ptr;
	struct fsck_inode_record *inorecptr;
	uint32_t ino_idx;
	struct dinode *ino_ptr;
	int aggregate_inode = -1;	/* going for aggregate inodes only */
	int alloc_ifnull = 0;

	msg_info_ptr = &ino_msg_info;
	msg_info_ptr->msg_inopfx = fsck_aggr_inode;
	msg_info_ptr->msg_inotyp = fsck_metadata;
	/*
	 * In release 1 there is always exactly 1 fileset, described
	 * by aggregate inode FILESYSTEM_I
	 */
	/* read the aggregate inode */
	ino_idx = FILESYSTEM_I;
	intermed_rc = inode_get(aggregate_inode, which_ait, ino_idx, &ino_ptr);
	if (intermed_rc != FSCK_OK) {
		/* can't get the inode */
		vaitp2_rc = FSCK_CANTREADAGGFSINO;
	} else {
		/* else got aggregate inode  */
		msg_info_ptr->msg_inonum = FILESYSTEM_I;
		vaitp2_rc =
		    verify_agg_fileset_inode(ino_ptr, ino_idx, which_ait,
					     msg_info_ptr);
		if (vaitp2_rc == FSCK_OK) {
			vaitp2_rc = get_inorecptr(aggregate_inode, alloc_ifnull,
						  FILESYSTEM_I, &inorecptr);
			if ((vaitp2_rc == FSCK_OK) && (inorecptr == NULL)) {
				vaitp2_rc = FSCK_INTERNAL_ERROR_32;
				fsck_send_msg(fsck_INTERNALERROR, vaitp2_rc, 0, 0, 0);
			} else if ((vaitp2_rc == FSCK_OK)
				   && (inorecptr->involved_in_dups)) {
				/*
				 * duplicate allocation(s) detected
				 * while validating the inode
				 */
				vaitp2_rc =
				    unrecord_valid_inode(ino_ptr, FILESYSTEM_I,
							 inorecptr,
							 msg_info_ptr);
				if (vaitp2_rc == FSCK_OK) {
					vaitp2_rc = FSCK_DUPMDBLKREF;
				}
			}
		}
	}
	return (vaitp2_rc);
}

/*****************************************************************************
 * NAME: verify_badblk_inode
 *
 * FUNCTION:  Verify the structures associated with and the content of
 *            the aggregate bad block inode, aggregate inode 4, whose
 *            data is the collection of bad blocks detected in the
 *            aggregate during mkfs processing.
 *
 * PARAMETERS:
 *      inoptr        - input - pointer to the inode in an fsck buffer
 *      which_ait     - input - the Aggregate Inode Table on which to perform
 *                              the function.  { fsck_primary | fsck_secondary }
 *      msg_info_ptr  - input - pointer to a data area with data needed to
 *                              issue messages about the inode
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int verify_badblk_inode(struct dinode *inoptr,
			int which_ait, struct fsck_ino_msg_info *msg_info_ptr)
{
	int vbbi_rc = FSCK_OK;
	int inode_invalid = 0;
	int ixpxd_unequal = 0;
	struct fsck_inode_record *inorecptr;
	int aggregate_inode = -1;
	int alloc_ifnull = -1;

	if (inoptr->di_inostamp != agg_recptr->inode_stamp) {
		/*
		 * doesn't match the key -- so
		 * this is either trashed or residual
		 */
		inode_invalid = 1;
		fsck_send_msg(fsck_BADINOSTAMP,
			      fsck_ref_msg(msg_info_ptr->msg_inotyp),
			      fsck_ref_msg(msg_info_ptr->msg_inopfx),
			      msg_info_ptr->msg_inonum);
	}
	if (inoptr->di_fileset != AGGREGATE_I) {
		/* unexpected fileset # */
		inode_invalid = 1;
		fsck_send_msg(fsck_BADINOOTHR, "15",
			      fsck_ref_msg(msg_info_ptr->msg_inotyp),
			      fsck_ref_msg(msg_info_ptr->msg_inopfx),
			      msg_info_ptr->msg_inonum);
	}
	if (inoptr->di_number != BADBLOCK_I) {
		/* unexpected inode # */
		inode_invalid = 1;
		fsck_send_msg(fsck_BADINOOTHR, "16",
			      fsck_ref_msg(msg_info_ptr->msg_inotyp),
			      fsck_ref_msg(msg_info_ptr->msg_inopfx),
			      msg_info_ptr->msg_inonum);
	}
	if (inoptr->di_gen != 1) {
		/* incorrect generation # */
		inode_invalid = 1;
		fsck_send_msg(fsck_BADINOOTHR, "17",
			      fsck_ref_msg(msg_info_ptr->msg_inotyp),
			      fsck_ref_msg(msg_info_ptr->msg_inopfx),
			      msg_info_ptr->msg_inonum);
	}
	ixpxd_unequal =
	    memcmp((void *) &(inoptr->di_ixpxd),
		   (void *) &(agg_recptr->ino_ixpxd), (sizeof (pxd_t)));
	if (ixpxd_unequal) {
		/* incorrect extent descriptor */
		inode_invalid = 1;
		fsck_send_msg(fsck_BADINOOTHR, "18",
			      fsck_ref_msg(msg_info_ptr->msg_inotyp),
			      fsck_ref_msg(msg_info_ptr->msg_inopfx),
			      msg_info_ptr->msg_inonum);
	}
	if ((inoptr->di_mode & (IFJOURNAL | IFREG | ISPARSE)) !=
	    (IFJOURNAL | IFREG | ISPARSE)) {
		inode_invalid = 1;
		fsck_send_msg(fsck_BADINOOTHR, "19",
			      fsck_ref_msg(msg_info_ptr->msg_inotyp),
			      fsck_ref_msg(msg_info_ptr->msg_inopfx),
			      msg_info_ptr->msg_inonum);
	}
	if (inoptr->di_nlink != 1) {
		/* incorrect # of links */
		inode_invalid = 1;
		fsck_send_msg(fsck_BADINOOTHR, "20",
			      fsck_ref_msg(msg_info_ptr->msg_inotyp),
			      fsck_ref_msg(msg_info_ptr->msg_inopfx),
			      msg_info_ptr->msg_inonum);
	}
	if (!(inoptr->di_dxd.flag & BT_ROOT)) {
		/* not flagged as B+ Tree root */
		inode_invalid = 1;
		fsck_send_msg(fsck_BADINOOTHR, "21",
			      fsck_ref_msg(msg_info_ptr->msg_inotyp),
			      fsck_ref_msg(msg_info_ptr->msg_inopfx),
			      msg_info_ptr->msg_inonum);
	}
	/*
	 * If any problems detected so far, don't bother trying to validate
	 * the B+ Tree
	 */
	if (!inode_invalid) {
		/* self inode looks ok so far */
		vbbi_rc =
		    get_inorecptr(aggregate_inode, alloc_ifnull, BADBLOCK_I,
				  &inorecptr);
		if ((vbbi_rc == FSCK_OK) && (inorecptr == NULL)) {
			vbbi_rc = FSCK_INTERNAL_ERROR_57;
			fsck_send_msg(fsck_INTERNALERROR, vbbi_rc, 0, 0, 0);
		} else if (vbbi_rc == FSCK_OK) {
			/* no problems so far */
			vbbi_rc =
			    verify_metadata_data(inoptr, BADBLOCK_I, inorecptr,
						 msg_info_ptr);
			if (inorecptr->ignore_alloc_blks
			    || (vbbi_rc != FSCK_OK)) {
				inode_invalid = -1;
			}
		}
	}
	/*
	 * wrap it all up for this inode
	 */
	if ((inode_invalid) && (vbbi_rc == FSCK_OK)) {
		vbbi_rc = FSCK_BMINOBAD;
	}
	if (inode_invalid) {
		/* this one's corrupt */
		if (which_ait == fsck_primary) {
			fsck_send_msg(fsck_BADMETAINOP,
				      fsck_ref_msg(msg_info_ptr->msg_inopfx),
				      msg_info_ptr->msg_inonum);
		} else {
			fsck_send_msg(fsck_BADMETAINOS,
				      fsck_ref_msg(msg_info_ptr->msg_inopfx),
				      msg_info_ptr->msg_inonum);
		}
	}
	return (vbbi_rc);
}

/*****************************************************************************
 * NAME: verify_bmap_inode
 *
 * FUNCTION:  Verify the structures associated with and the content of
 *            the aggregate block map inode, aggregate inode 2, whose
 *            data is the Aggregate Block Allocation Map.
 *
 * PARAMETERS:
 *      inoptr        - input - pointer to the inode in an fsck buffer
 *      which_ait     - input - the Aggregate Inode Table on which to perform
 *                              the function.  { fsck_primary | fsck_secondary }
 *      msg_info_ptr  - input - pointer to a data area with data needed to
 *                              issue messages about the inode
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int verify_bmap_inode(struct dinode *inoptr, int which_ait,
		      struct fsck_ino_msg_info *msg_info_ptr)
{
	int vbi_rc = FSCK_OK;
	int inode_invalid = 0;
	int ixpxd_unequal = 0;
	struct fsck_inode_record *inorecptr;
	int aggregate_inode = -1;
	int alloc_ifnull = -1;

	if (inoptr->di_inostamp != agg_recptr->inode_stamp) {
		/* doesn't match the key -- so this is either trashed or residual */
		inode_invalid = 1;
		fsck_send_msg(fsck_BADINOSTAMP,
			      fsck_ref_msg(msg_info_ptr->msg_inotyp),
			      fsck_ref_msg(msg_info_ptr->msg_inopfx),
			      msg_info_ptr->msg_inonum);
	}
	if (inoptr->di_fileset != AGGREGATE_I) {
		/* unexpected fileset # */
		inode_invalid = 1;
		fsck_send_msg(fsck_BADINOOTHR, "22",
			      fsck_ref_msg(msg_info_ptr->msg_inotyp),
			      fsck_ref_msg(msg_info_ptr->msg_inopfx),
			      msg_info_ptr->msg_inonum);
	}
	if (inoptr->di_number != BMAP_I) {
		/* unexpected inode # */
		inode_invalid = 1;
		fsck_send_msg(fsck_BADINOOTHR, "23",
			      fsck_ref_msg(msg_info_ptr->msg_inotyp),
			      fsck_ref_msg(msg_info_ptr->msg_inopfx),
			      msg_info_ptr->msg_inonum);
	}
	if (inoptr->di_gen != 1) {
		/* incorrect generation # */
		inode_invalid = 1;
		fsck_send_msg(fsck_BADINOOTHR, "24",
			      fsck_ref_msg(msg_info_ptr->msg_inotyp),
			      fsck_ref_msg(msg_info_ptr->msg_inopfx),
			      msg_info_ptr->msg_inonum);
	}
	ixpxd_unequal =
	    memcmp((void *) &(inoptr->di_ixpxd),
		   (void *) &(agg_recptr->ino_ixpxd), (sizeof (pxd_t)));
	if (ixpxd_unequal) {
		/* incorrect extent descriptor */
		inode_invalid = 1;
		fsck_send_msg(fsck_BADINOOTHR, "25",
			      fsck_ref_msg(msg_info_ptr->msg_inotyp),
			      fsck_ref_msg(msg_info_ptr->msg_inopfx),
			      msg_info_ptr->msg_inonum);
	}
	if ((inoptr->di_mode & (IFJOURNAL | IFREG)) != (IFJOURNAL | IFREG)) {
		inode_invalid = 1;
		fsck_send_msg(fsck_BADINOOTHR, "26",
			      fsck_ref_msg(msg_info_ptr->msg_inotyp),
			      fsck_ref_msg(msg_info_ptr->msg_inopfx),
			      msg_info_ptr->msg_inonum);
	}
	if (inoptr->di_nlink != 1) {
		/* incorrect # of links */
		inode_invalid = 1;
		fsck_send_msg(fsck_BADINOOTHR, "27",
			      fsck_ref_msg(msg_info_ptr->msg_inotyp),
			      fsck_ref_msg(msg_info_ptr->msg_inopfx),
			      msg_info_ptr->msg_inonum);
	}
	if (!(inoptr->di_dxd.flag & BT_ROOT)) {
		/* not flagged as B+ Tree root */
		inode_invalid = 1;
		fsck_send_msg(fsck_BADINOOTHR, "28",
			      fsck_ref_msg(msg_info_ptr->msg_inotyp),
			      fsck_ref_msg(msg_info_ptr->msg_inopfx),
			      msg_info_ptr->msg_inonum);
	}
	/*
	 * If any problems detected so far, don't bother trying to validate
	 * the B+ Tree
	 */
	if (!inode_invalid) {
		/* self inode looks ok so far */
		vbi_rc =
		    get_inorecptr(aggregate_inode, alloc_ifnull, BMAP_I,
				  &inorecptr);
		if ((vbi_rc == FSCK_OK) && (inorecptr == NULL)) {
			vbi_rc = FSCK_INTERNAL_ERROR_40;
			fsck_send_msg(fsck_INTERNALERROR, vbi_rc, 0, 0, 0);
		} else if (vbi_rc == FSCK_OK) {	/* no problems so far */
			vbi_rc =
			    verify_metadata_data(inoptr, BMAP_I, inorecptr,
						 msg_info_ptr);

			if (inorecptr->ignore_alloc_blks || (vbi_rc != FSCK_OK)) {
				inode_invalid = -1;
			}
		}
	}
	/*
	 * wrap it all up for this inode
	 */
	if ((inode_invalid) && (vbi_rc == FSCK_OK)) {
		vbi_rc = FSCK_BMINOBAD;
	}
	if (inode_invalid) {
		/* this one's corrupt */
		if (which_ait == fsck_primary) {
			fsck_send_msg(fsck_BADMETAINOP,
				      fsck_ref_msg(msg_info_ptr->msg_inopfx),
				      msg_info_ptr->msg_inonum);
		} else {
			fsck_send_msg(fsck_BADMETAINOS,
				      fsck_ref_msg(msg_info_ptr->msg_inopfx),
				      msg_info_ptr->msg_inonum);
		}
	}
	return (vbi_rc);
}

/*****************************************************************************
 * NAME: verify_fs_super_ext
 *
 * FUNCTION:  Verify the structures associated with and the content of
 *            the fileset super extension inode, fileset inode 1, whose
 *            data is a logical extension of the aggregate fileset inode.
 *
 * PARAMETERS:
 *      inoptr         - input - pointer to the inode in an fsck buffer
 *      msg_info_ptr   - input - pointer to a data area with data needed to
 *                               issue messages about the inode
 *      inode_changed  - input - pointer to a variable in which to return
 *                               !0 if the inode (in the buffer) has been
 *                                  modified by this routine
 *                                0 if the inode (in the buffer) has not been
 *                                  modified by this routine
 *
 * NOTES:  o In release 1 this inode is allocated and initialized but is
 *           not used.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int verify_fs_super_ext(struct dinode *inoptr,
			struct fsck_ino_msg_info *msg_info_ptr,
			int *inode_changed)
{
	int vfse_rc = FSCK_OK;
	int inode_invalid = 0;
	int ixpxd_unequal = 0;
	struct fsck_inode_record *inorecptr;
	uint32_t inoidx = FILESET_EXT_I;
	int aggregate_inode = 0;	/* this is a fileset inode */
	int alloc_ifnull = -1;

	*inode_changed = 0;	/* assume no changes */
	if (inoptr->di_inostamp != agg_recptr->inode_stamp) {
		/*
		 * doesn't match the key -- so
		 * this is either trashed or residual
		 */
		inode_invalid = 1;
		fsck_send_msg(fsck_BADINOSTAMP,
			      fsck_ref_msg(msg_info_ptr->msg_inotyp),
			      fsck_ref_msg(msg_info_ptr->msg_inopfx),
			      msg_info_ptr->msg_inonum);
	}
	if (inoptr->di_fileset != FILESYSTEM_I) {
		/* unexpected fileset # */
		inode_invalid = 1;
		fsck_send_msg(fsck_BADINOOTHR, "29",
			      fsck_ref_msg(msg_info_ptr->msg_inotyp),
			      fsck_ref_msg(msg_info_ptr->msg_inopfx),
			      msg_info_ptr->msg_inonum);
	}
	if (inoptr->di_number != FILESET_EXT_I) {
		/* unexpected inode # */
		inode_invalid = 1;
		fsck_send_msg(fsck_BADINOOTHR, "30",
			      fsck_ref_msg(msg_info_ptr->msg_inotyp),
			      fsck_ref_msg(msg_info_ptr->msg_inopfx),
			      msg_info_ptr->msg_inonum);
	}
	if (inoptr->di_gen != 1) {
		/* incorrect generation # */
		inode_invalid = 1;
		fsck_send_msg(fsck_BADINOOTHR, "30a",
			      fsck_ref_msg(msg_info_ptr->msg_inotyp),
			      fsck_ref_msg(msg_info_ptr->msg_inopfx),
			      msg_info_ptr->msg_inonum);
	}
	ixpxd_unequal =
	    memcmp((void *) &(inoptr->di_ixpxd),
		   (void *) &(agg_recptr->ino_ixpxd), sizeof (pxd_t));
	if (ixpxd_unequal) {
		/* incorrect extent descriptor */
		inode_invalid = 1;
		fsck_send_msg(fsck_BADINOOTHR, "31",
			      fsck_ref_msg(msg_info_ptr->msg_inotyp),
			      fsck_ref_msg(msg_info_ptr->msg_inopfx),
			      msg_info_ptr->msg_inonum);
	}
	if ((inoptr->di_mode & (IFJOURNAL | IFREG)) != (IFJOURNAL | IFREG)) {
		inode_invalid = 1;
		fsck_send_msg(fsck_BADINOOTHR, "32",
			      fsck_ref_msg(msg_info_ptr->msg_inotyp),
			      fsck_ref_msg(msg_info_ptr->msg_inopfx),
			      msg_info_ptr->msg_inonum);
	}
	if (inoptr->di_nlink != 1) {
		/* incorrect # of links */
		inode_invalid = 1;
		fsck_send_msg(fsck_BADINOOTHR, "33",
			      fsck_ref_msg(msg_info_ptr->msg_inotyp),
			      fsck_ref_msg(msg_info_ptr->msg_inopfx),
			      msg_info_ptr->msg_inonum);
	}
	if (!(inoptr->di_dxd.flag & BT_ROOT)) {
		/* not flagged as B+ Tree root */
		inode_invalid = 1;
		fsck_send_msg(fsck_BADINOOTHR, "34",
			      fsck_ref_msg(msg_info_ptr->msg_inotyp),
			      fsck_ref_msg(msg_info_ptr->msg_inopfx),
			      msg_info_ptr->msg_inonum);
	}
	/*
	 * If any problems detected so far, don't bother trying to validate
	 * the B+ Tree
	 */
	if (!inode_invalid) {
		/* self inode looks ok so far */
		vfse_rc =
		    get_inorecptr(aggregate_inode, alloc_ifnull, inoidx,
				  &inorecptr);
		if ((vfse_rc == FSCK_OK) && (inorecptr == NULL)) {
			vfse_rc = FSCK_INTERNAL_ERROR_41;
			fsck_send_msg(fsck_INTERNALERROR, vfse_rc, 0, 0, 0);
		} else if (vfse_rc == FSCK_OK) {
			/* no problems so far */
			vfse_rc =
			    verify_metadata_data(inoptr, inoidx, inorecptr,
						 msg_info_ptr);
			if (inorecptr->ignore_alloc_blks
			    || (vfse_rc != FSCK_OK)) {
				inode_invalid = -1;
				vfse_rc = FSCK_OK;
			}
		}
	}
	/* at the moment it really doesn't matter
	 * what's in this inode...it's here and
	 * reserved for fileset superinode extension,
	 * but isn't being used and so the data
	 * it contains is irrelevant
	 */
	inode_invalid = 0;
	/*
	 * wrap it all up for this inode
	 */
	if ((inode_invalid) && (vfse_rc == FSCK_OK)) {
		vfse_rc = FSCK_FSETEXTBAD;
	}
	if (inode_invalid) {
		/* this one's corrupt */
		fsck_send_msg(fsck_BADMETAINOF,
			      fsck_ref_msg(msg_info_ptr->msg_inopfx),
			      msg_info_ptr->msg_inonum);
	}
	return (vfse_rc);
}

/*****************************************************************************
 * NAME: verify_log_inode
 *
 * FUNCTION:  Verify the structures associated with and the content of
 *            the aggregate journal inode, aggregate inode 3, whose
 *            data is (or describes) the aggregate's journal, or log,
 *            of transactions.
 *
 * PARAMETERS:
 *      inoptr        - input - pointer to the inode in an fsck buffer
 *      which_ait     - input - the Aggregate Inode Table on which to perform
 *                              the function.  { fsck_primary | fsck_secondary }
 *      msg_info_ptr  - input - pointer to a data area with data needed to
 *                              issue messages about the inode
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int verify_log_inode(struct dinode *inoptr, int which_ait,
		     struct fsck_ino_msg_info *msg_info_ptr)
{
	int vli_rc = FSCK_OK;
	int inode_invalid = 0;
	int ixpxd_unequal = 0;
	struct fsck_inode_record *inorecptr;
	int aggregate_inode = -1;
	int alloc_ifnull = -1;

	if (inoptr->di_inostamp != agg_recptr->inode_stamp) {
		/*
		 * doesn't match the key -- so
		 * this is either trashed or residual
		 */
		inode_invalid = 1;
		fsck_send_msg(fsck_BADINOSTAMP,
			      fsck_ref_msg(msg_info_ptr->msg_inotyp),
			      fsck_ref_msg(msg_info_ptr->msg_inopfx),
			      msg_info_ptr->msg_inonum);
	}
	if (inoptr->di_fileset != AGGREGATE_I) {
		/* unexpected fileset # */
		inode_invalid = 1;
		fsck_send_msg(fsck_BADINOOTHR, "35",
			      fsck_ref_msg(msg_info_ptr->msg_inotyp),
			      fsck_ref_msg(msg_info_ptr->msg_inopfx),
			      msg_info_ptr->msg_inonum);
	}
	if (inoptr->di_number != LOG_I) {
		/* unexpected inode # */
		inode_invalid = 1;
		fsck_send_msg(fsck_BADINOOTHR, "36",
			      fsck_ref_msg(msg_info_ptr->msg_inotyp),
			      fsck_ref_msg(msg_info_ptr->msg_inopfx),
			      msg_info_ptr->msg_inonum);
	}
	if (inoptr->di_gen != 1) {
		/* incorrect generation # */
		inode_invalid = 1;
		fsck_send_msg(fsck_BADINOOTHR, "37",
			      fsck_ref_msg(msg_info_ptr->msg_inotyp),
			      fsck_ref_msg(msg_info_ptr->msg_inopfx),
			      msg_info_ptr->msg_inonum);
	}
	ixpxd_unequal =
	    memcmp((void *) &(inoptr->di_ixpxd),
		   (void *) &(agg_recptr->ino_ixpxd), (sizeof (pxd_t)));
	if (ixpxd_unequal) {
		/* incorrect extent descriptor */
		inode_invalid = 1;
		fsck_send_msg(fsck_BADINOOTHR, "38",
			      fsck_ref_msg(msg_info_ptr->msg_inotyp),
			      fsck_ref_msg(msg_info_ptr->msg_inopfx),
			      msg_info_ptr->msg_inonum);
	}
	if ((inoptr->di_mode & (IFJOURNAL | IFREG)) != (IFJOURNAL | IFREG)) {
		inode_invalid = 1;
		fsck_send_msg(fsck_BADINOOTHR, "39",
			      fsck_ref_msg(msg_info_ptr->msg_inotyp),
			      fsck_ref_msg(msg_info_ptr->msg_inopfx),
			      msg_info_ptr->msg_inonum);
	}
	if (inoptr->di_nlink != 1) {
		/* incorrect # of links */
		inode_invalid = 1;
		fsck_send_msg(fsck_BADINOOTHR, "40",
			      fsck_ref_msg(msg_info_ptr->msg_inotyp),
			      fsck_ref_msg(msg_info_ptr->msg_inopfx),
			      msg_info_ptr->msg_inonum);
	}
	if (!(inoptr->di_dxd.flag & BT_ROOT)) {
		/* not flagged as B+ Tree root */
		inode_invalid = 1;
		fsck_send_msg(fsck_BADINOOTHR, "41",
			      fsck_ref_msg(msg_info_ptr->msg_inotyp),
			      fsck_ref_msg(msg_info_ptr->msg_inopfx),
			      msg_info_ptr->msg_inonum);
	}
	/*
	 * If any problems detected so far, don't bother trying to validate
	 * the B+ Tree
	 */
	if (!inode_invalid) {
		/* log inode looks ok so far */
		vli_rc =
		    get_inorecptr(aggregate_inode, alloc_ifnull, LOG_I,
				  &inorecptr);
		if ((vli_rc == FSCK_OK) && (inorecptr == NULL)) {
			vli_rc = FSCK_INTERNAL_ERROR_42;
			fsck_send_msg(fsck_INTERNALERROR, vli_rc, 0, 0, 0);
		} else if (vli_rc == FSCK_OK) {
			/* no problems so far */
			vli_rc =
			    verify_metadata_data(inoptr, LOG_I, inorecptr,
						 msg_info_ptr);
			if (inorecptr->ignore_alloc_blks || (vli_rc != FSCK_OK)) {
				inode_invalid = -1;
			}
		}
	}
	/*
	 * wrap it all up for this inode
	 */
	if ((inode_invalid) && (vli_rc == FSCK_OK)) {
		vli_rc = FSCK_LOGINOBAD;
	}
	if (inode_invalid) {
		/* this one's corrupt */
		if (which_ait == fsck_primary) {
			fsck_send_msg(fsck_BADMETAINOP,
				      fsck_ref_msg(msg_info_ptr->msg_inopfx),
				      msg_info_ptr->msg_inonum);
		} else {
			fsck_send_msg(fsck_BADMETAINOS,
				      fsck_ref_msg(msg_info_ptr->msg_inopfx),
				      msg_info_ptr->msg_inonum);
		}
	}
	return (vli_rc);
}

/*****************************************************************************
 * NAME: verify_metadata_data
 *
 * FUNCTION:  Initialize the inode record for and verify the data structures
 *		allocated to a JFS metadata inode.
 *
 * PARAMETERS:
 *      inoptr        - input - pointer to the inode in an fsck buffer
 *      inoidx	   - input - inode number of the inode in the buffer
 *      inorecptr   - input - pointer to record allocated to describe the inode
 *      msg_info_ptr  - input - pointer to a data area with data needed to
 *                              issue messages about the inode
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int verify_metadata_data(struct dinode *inoptr,
			 uint32_t inoidx,
			 struct fsck_inode_record *inorecptr,
			 struct fsck_ino_msg_info *msg_info_ptr)
{
	int vmd_rc = FSCK_OK;
	int8_t bad_size = 0;
	int64_t min_size, max_size;

	/*
	 * clear the workspace area for the current inode
	 */
	memset((void *) (&(agg_recptr->this_inode)), '\0',
	       sizeof (agg_recptr->this_inode));
	memcpy((void *) &(agg_recptr->this_inode.eyecatcher),
	       (void *) "thisinod", 8);
	/*
	 * initialize the inode record for this inode
	 */
	inorecptr->in_use = 1;
	inorecptr->selected_to_rls = 0;
	inorecptr->crrct_link_count = 0;
	inorecptr->crrct_prnt_inonum = 0;
	inorecptr->adj_entries = 0;
	inorecptr->cant_chkea = 0;
	inorecptr->clr_ea_fld = 0;
	inorecptr->clr_acl_fld = 0;
	inorecptr->inlineea_on = 0;
	inorecptr->inlineea_off = 0;
	inorecptr->inline_data_err = 0;
	inorecptr->ignore_alloc_blks = 0;
	inorecptr->reconnect = 0;
	inorecptr->unxpctd_prnts = 0;
	if (inoidx == BADBLOCK_I) {
		inorecptr->badblk_inode = 1;
	} else {
		inorecptr->badblk_inode = 0;
	}
	inorecptr->involved_in_dups = 0;
	inorecptr->inode_type = metadata_inode;
	inorecptr->link_count = 0;
	inorecptr->parent_inonum = 0;
	inorecptr->ext_rec = NULL;
	/*
	 * verify the B+ Tree and record the blocks it occupies and
	 * also the blocks it describes.
	 *
	 * (If the tree is corrupt any recorded blocks will be unrecorded
	 * before control is returned.)
	 */
	vmd_rc = validate_data(inoptr, inoidx, inorecptr, msg_info_ptr);

	if ((!inorecptr->selected_to_rls) && (!inorecptr->ignore_alloc_blks)) {
		/* no problems found in the tree yet */
		if (inoptr->di_nblocks != agg_recptr->this_inode.all_blks) {
			/* number of blocks is wrong.  tree must be bad */
#ifdef _JFS_DEBUG
			printf("bad num blocks: agg ino: %ld(t)  "
			       "di_nblocks = %lld(t)  "
			       "this_inode.all_blks = %lld(t)\n",
			       inoidx, inoptr->di_nblocks,
			       agg_recptr->this_inode.all_blks);
#endif
			inorecptr->selected_to_rls = 1;
			inorecptr->ignore_alloc_blks = 1;
			agg_recptr->corrections_needed = 1;
			bad_size = -1;
		} else {
			/* the data size (in bytes) must not exceed the total size
			 * of the blocks allocated for it and must use at least 1
			 * byte in the last fsblock allocated for it.
			 */
			if (agg_recptr->this_inode.data_size == 0) {
				min_size = 0;
				max_size = IDATASIZE;
			} else {
				/* blocks are allocated to data */
				min_size =
				    agg_recptr->this_inode.data_size -
				    sb_ptr->s_bsize + 1;
				max_size = agg_recptr->this_inode.data_size;
			}

			if ((inoptr->di_size > max_size)
			    || (inoptr->di_size < min_size)) {
				/*
				 * object size (in bytes) is wrong.
				 * tree must be bad.
				 */
#ifdef _JFS_DEBUG
				printf("bad object size: agg ino: %ld(t)  "
				       "minsize = %lld(t)  maxsize = %lld(t)  "
				       "di_size = %lld(t)\n",
				       inoidx, min_size, max_size,
				       inoptr->di_size);
#endif
				inorecptr->selected_to_rls = 1;
				inorecptr->ignore_alloc_blks = 1;
				agg_recptr->corrections_needed = 1;
				bad_size = -1;
			}
		}
	}
	/*
	 * If bad_size is set then we didn't know that
	 * the tree was bad until we looked at the size
	 * fields.  This means that the block usage recorded
	 * for this inode has not been backed out yet.
	 */
	if (bad_size) {
		/* tree is bad by implication */
		/*
		 * remove traces, in the fsck workspace
		 * maps, of the blocks allocated to data
		 * for this inode, whether a single
		 * extent or a B+ Tree
		 */
		process_valid_data(inoptr, inoidx, inorecptr, msg_info_ptr,
				   FSCK_UNRECORD);
	}
	return (vmd_rc);
}

/*****************************************************************************
 * NAME: verify_repair_fs_rootdir
 *
 * FUNCTION:  Verify the structures associated with and the content of
 *            the fileset root directory inode, fileset inode 2, whose
 *            leaves contain the entries in the root directory.  If any
 *            problems are detected then, with the caller's permission,
 *            correct (or reinitialize) the inode.
 *
 * PARAMETERS:
 *      inoptr         - input - pointer to the inode in an fsck buffer
 *      msg_info_ptr   - input - pointer to a data area with data needed to
 *                               issue messages about the inode
 *      inode_changed  - input - pointer to a variable in which to return
 *                               !0 if the inode (in the buffer) has been
 *                                  modified by this routine
 *                                0 if the inode (in the buffer) has not been
 *                                  modified by this routine
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int verify_repair_fs_rootdir(struct dinode *inoptr,
			     struct fsck_ino_msg_info *msg_info_ptr,
			     int *inode_changed)
{
	int vrfr_rc = FSCK_OK;
	int intermed_rc;
	int bad_root_format = 0;
	int bad_root_data_format = 0;
	int ixpxd_unequal = 0;
	int inode_tree_changed = 0;
	mode_t expected_mode;
	struct fsck_inode_record *inorecptr;
	uint32_t inoidx = ROOT_I;
	int aggregate_inode = 0;
	int alloc_ifnull = -1;
	int8_t bad_size = 0;
	int64_t max_size;

	*inode_changed = 0;
	/* set mode to this function now, synch up with mkfs and jfs as needed */
	expected_mode = IFJOURNAL | IFDIR | IREAD | IWRITE | IEXEC;
	ixpxd_unequal = memcmp((void *)&(inoptr->di_ixpxd),
			       (void *)&(agg_recptr->ino_ixpxd),
			       (sizeof (pxd_t)));
	vrfr_rc =
	    get_inorecptr(aggregate_inode, alloc_ifnull, ROOT_I, &inorecptr);

	if (vrfr_rc != FSCK_OK)
		goto vrfr_set_exit;

	/* got an inode record */
	if ((inoptr->di_inostamp != agg_recptr->inode_stamp) ||	/* bad stamp or */
	    (inoptr->di_fileset != FILESYSTEM_I) ||	/* unexpected fileset # or */
	    (inoptr->di_number != ROOT_I) ||	/* unexpected inode # or */
	    (inoptr->di_gen != 1) ||	/* unexpected generation # or */
	    (ixpxd_unequal) /* incorrect extent descriptor */ ) {
		/* inode is not allocated */
		bad_root_format = -1;
		fsck_send_msg(fsck_RIUNALLOC);
		if (agg_recptr->processing_readwrite) {
			/* we can fix this */
			/*
			 * If we get this far, the inode is allocated physically,
			 * just isn't in use
			 */
		memset(inoptr, 0, sizeof (struct dinode));
			inoptr->di_inostamp = agg_recptr->inode_stamp;
			inoptr->di_fileset = FILESYSTEM_I;
			inoptr->di_number = ROOT_I;
			inoptr->di_gen = 1;
			memcpy((void *) &(inoptr->di_ixpxd),
			       (void *) &(agg_recptr->ino_ixpxd),
			       sizeof (pxd_t));
			inoptr->di_mode = expected_mode;
			/* one from itself to itself as
			 * parent to child and one from
			 * itself to itself as child to parent
			 */
			inoptr->di_nlink = 2;
			inoptr->di_atime.tv_sec = (uint32_t) time(NULL);
			inoptr->di_ctime.tv_sec = inoptr->di_atime.tv_sec;
			inoptr->di_mtime.tv_sec = inoptr->di_atime.tv_sec;
			inoptr->di_otime.tv_sec = inoptr->di_atime.tv_sec;
			/*
			 * initialize the d-tree
			 */
			init_dir_tree((dtroot_t *) &(inoptr->di_btroot));
			inoptr->di_next_index = 2;
			*inode_changed = 1;
			fsck_send_msg(fsck_ROOTALLOC);
		} else {
			/* don't have write access */
			vrfr_rc = FSCK_RIUNALLOC;
		}
		goto vrfr_set_exit;
	}

	/* inode is allocated */
	if ((inoptr->di_mode & expected_mode) != expected_mode) {
		/* wrong type */
		bad_root_format = -1;
		fsck_send_msg(fsck_RINOTDIR);
		if (agg_recptr->processing_readwrite) {
			/* we can fix this */
			/*
			 * If this is the root directory with a bad value
			 *      in the mode field,
			 * then below we'll find the B+ Tree is a valid directory
			 *      tree and keep it.
			 *
			 * Else if it really was commandeered for something else,
			 *
			 *    If it was taken for a directory,
			 *    then we'll find a valid directory tree and keep it as
			 *         the root directory.  /lost+found/ will be created
			 *         here and probably get very full.
			 *
			 *    If it was taken for something else
			 *    then we won't find a valid directory tree so we'll
			 *         either initialize the tree or else mark the file
			 *         system dirty.
			 *
			 * Else (it was trashed or was never initialized)
			 *
			 *     we won't find a valid directory tree so we'll either
			 *     initialize the tree or else mark the file system dirty.
			 */
			inoptr->di_mode = expected_mode;
			*inode_changed = 1;
			fsck_send_msg(fsck_ROOTNOWDIR);
		} else {
			/* don't have write access */
			vrfr_rc = FSCK_RINOTDIR;
		}
	}
	if (vrfr_rc == FSCK_OK) {
		/* we've corrected every problem we've seen to this point */
		if (inoptr->di_parent != ROOT_I) {
			/* doesn't link back to itself correctly */
			bad_root_format = -1;
			fsck_send_msg(fsck_RIINCINOREF);
			if (agg_recptr->processing_readwrite) {
				/* we can fix this */
				inoptr->di_parent = ROOT_I;
				*inode_changed = 1;
				fsck_send_msg(fsck_RICRRCTDREF);
			} else {
				/* don't have write access */
				agg_recptr->ag_dirty = 1;
			}
		}
	}
	/*
	 * clear the workspace area for the current inode
	 */
	memset((void *) (&(agg_recptr->this_inode)), '\0',
	       sizeof(agg_recptr->this_inode));
	memcpy((void *) &(agg_recptr->this_inode.eyecatcher),
	       (void *) "thisinod", 8);
	/*
	 * verify the root inode's extended attributes (if any)
	 *
	 * If a problem is found, the user is notified and EA cleared.
	 */
	intermed_rc = validate_EA(inoptr, inoidx, inorecptr, msg_info_ptr);
	if (inorecptr->clr_ea_fld) {
		/* the ea isn't valid */
		clear_EA_field(inorecptr, inoptr);
		*inode_changed = 1;
	}
	/*
	 * verify the root inode's access control list (if any)
	 *
	 * If a problem is found, the user is notified and ACL cleared.
	 */
	intermed_rc = validate_ACL(inoptr, inoidx, inorecptr, msg_info_ptr);
	if (inorecptr->clr_acl_fld) {
		/* the ea isn't valid */
		clear_ACL_field(inorecptr, inoptr);
		*inode_changed = 1;
	}
	if (vrfr_rc != FSCK_OK)
		goto vrfr_set_exit;

	if (!(inoptr->di_dxd.flag & BT_ROOT)) {
		/* not a B+ Tree root */
		bad_root_data_format = -1;
		if (agg_recptr->processing_readwrite) {
			/* we can fix this */
			vrfr_rc = rootdir_tree_bad(inoptr, &inode_tree_changed);
			if (inode_tree_changed) {
				*inode_changed = 1;
			}
		} else {
			/* don't have write access */
			vrfr_rc = FSCK_RIDATAERROR;
		}
		goto vrfr_set_exit;
	}

	/* the tree looks ok from here... */
	/*
	 * check the dtree rooted in the inode
	 */
	/*
	 * verify the B+ Tree and the directory entries
	 * contained in its leaf nodes.  record the blocks
	 * it occupies.
	 */
	intermed_rc = validate_dir_data(inoptr, inoidx, inorecptr,msg_info_ptr);
	if ((!inorecptr->selected_to_rls) &&
	    (!inorecptr->ignore_alloc_blks) &&
	    (intermed_rc == FSCK_OK)) {
		/* no problems found in the tree yet */
		if (inoptr->di_nblocks != agg_recptr->this_inode.all_blks) {
			/*
			 * number of blocks is wrong.  tree must
			 * be bad
			 */
#ifdef _JFS_DEBUG
			printf("bad num blocks: fs ino: %ld(t)  "
			       "di_nblocks = %lld(t)  "
			       "this_inode.all_blks = %lld(t)\n",
			       inoidx, inoptr->di_nblocks,
			       agg_recptr->this_inode.all_blks);
#endif
			bad_size = -1;
		} else {
			/* the data size (in bytes) must not exceed the total size
			 * of the blocks allocated for it.
			 * Blocks allocated for directory index table
			 * make minimum size checking inconclusive
			 */
			if (agg_recptr->this_inode.data_size == 0) {
				max_size = IDATASIZE;
			} else {
				/* blocks are allocated to data */
				max_size = agg_recptr->this_inode.data_size;
			}
			if (inoptr->di_size > max_size) {
				/*
				 * object size (in bytes) is wrong.
				 * tree must be bad.
				 */
#ifdef _JFS_DEBUG
				printf("bad obj size: fs ino: %ld(t)  "
				       "maxsize = %lld(t)  di_size = %lld(t)\n",
				       inoidx, max_size, inoptr->di_size);
#endif
				bad_size = -1;
			}
		}
		/*
		 * If bad_size is set then we didn't know that
		 * the tree was bad until we looked at the size
		 * fields.  This means that the block usage recorded
		 * for this inode has not been backed out yet.
		 */
		if (bad_size) {
			/* tree is bad by implication */
			/*
			 * remove traces, in the fsck workspace
			 * maps, of the blocks allocated to data
			 * for this inode, whether a single
			 * extent or a B+ Tree
			 */
			process_valid_dir_data(inoptr, inoidx, inorecptr,
					       msg_info_ptr, FSCK_UNRECORD);
			bad_root_data_format = -1;
			if (agg_recptr->processing_readwrite) {
				/* we can fix this */
				vrfr_rc = rootdir_tree_bad(inoptr,
							   &inode_tree_changed);
				if (inode_tree_changed) {
					*inode_changed = 1;
				}
			} else {
				/* don't have write access */
				vrfr_rc = FSCK_RIBADTREE;
			}
		} else {
			/* things still look ok */
			intermed_rc = in_inode_data_check(inorecptr,
							  msg_info_ptr);
			if (inorecptr->selected_to_rls) {
				/* nope, it isn't right */
				inorecptr->selected_to_rls = 0;
				/*
				 * remove traces, in the fsck workspace
				 * maps, of the blocks allocated to data
				 * for this inode, whether a single
				 * extent or a B+ Tree
				 */
				process_valid_dir_data(inoptr, inoidx,
					inorecptr,msg_info_ptr, FSCK_UNRECORD);
				bad_root_data_format = -1;
				if (agg_recptr->processing_readwrite) {
					/* we can fix this */
					vrfr_rc = rootdir_tree_bad(inoptr,
							&inode_tree_changed);
					if (inode_tree_changed) {
						*inode_changed = 1;
					}
				} else {
					/* don't have write access */
					vrfr_rc = FSCK_RIBADTREE;
				}
			}
			if (inorecptr->clr_ea_fld) {
				clear_EA_field(inorecptr, inoptr);
				*inode_changed = 1;
			}
			if (inorecptr->clr_acl_fld) {
				clear_ACL_field(inorecptr, inoptr);
				*inode_changed = 1;
			}
		}
	} else {
		/* not a good tree */
		if (vrfr_rc == FSCK_OK) {
			/* but nothing fatal */
			bad_root_data_format = -1;
			if (agg_recptr->processing_readwrite) {
				/* we can fix this */
				vrfr_rc = rootdir_tree_bad(inoptr,
							   &inode_tree_changed);
				if (inode_tree_changed) {
					*inode_changed = 1;
				}
			} else {
				/* don't have write access */
				vrfr_rc = FSCK_RIBADTREE;
			}
		}
	}

      vrfr_set_exit:
	if (bad_root_format) {
		if (agg_recptr->processing_readwrite) {
			/* we have fixed this */
			fsck_send_msg(fsck_WILLFIXRIBADFMT);
		} else {
			/* no write access */
			fsck_send_msg(fsck_RIBADFMT);
			if (vrfr_rc != FSCK_OK) {
				agg_recptr->ag_dirty = 1;
				fsck_send_msg(fsck_CANTCONTINUE);
				fsck_send_msg(fsck_ERRORSDETECTED);
			}
		}
	}
	if (bad_root_data_format) {
		if (agg_recptr->processing_readwrite) {
			/* we have fixed this */
			fsck_send_msg(fsck_WILLFIXRIBADDATFMT);
		} else {
			/* no write access */
			fsck_send_msg(fsck_RIBADDATFMT);
			if (vrfr_rc != FSCK_OK) {
				agg_recptr->ag_dirty = 1;
				fsck_send_msg(fsck_CANTCONTINUE);
				fsck_send_msg(fsck_ERRORSDETECTED);
			}
		}
	}

	if (vrfr_rc == FSCK_OK) {
		vrfr_rc =
		    get_inorecptr(aggregate_inode, alloc_ifnull, inoidx,
				  &inorecptr);
		if ((vrfr_rc == FSCK_OK) && (inorecptr == NULL)) {
			vrfr_rc = FSCK_INTERNAL_ERROR_33;
			fsck_send_msg(fsck_INTERNALERROR, vrfr_rc, 0, 0, 0);
		} else if (vrfr_rc == FSCK_OK) {
			/* got a record to describe it */
			inorecptr->in_use = 1;
			inorecptr->inode_type = directory_inode;
			inorecptr->link_count -= inoptr->di_nlink;
			inorecptr->parent_inonum = ROOT_I;
			inorecptr->ignore_alloc_blks = 0;
		}
	}
	return (vrfr_rc);
}
