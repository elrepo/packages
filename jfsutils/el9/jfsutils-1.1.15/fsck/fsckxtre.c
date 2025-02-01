/*
 *   Copyright (C) International Business Machines Corp., 2000-2004
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
#include <errno.h>
#include <string.h>
/* defines and includes common among the fsck.jfs modules */
#include "xfsckint.h"
#include "jfs_byteorder.h"

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

/* VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV
 *
 * The following are internal to this file
 *
 */

struct fsck_Xtree_info {
	xtpage_t *xtp_ptr;
	xad_t *xad_ptr;
	int64_t ext_addr;
	uint32_t ext_length;
	struct treeQelem *this_Qel;
	struct treeQelem *next_Qel;
	int64_t this_key;
	int64_t last_key;
	int64_t last_node_addr;
	int8_t last_level;
	int8_t dense_file;
	int8_t leaf_seen;
};

int xTree_binsrch_page(xtpage_t *, int64_t, int8_t *, int16_t *, int8_t *);

int xTree_process_internal_extents(xtpage_t *, struct fsck_inode_record *,
				   struct treeQelem *,
				   struct fsck_ino_msg_info *, int);

int xTree_node_first_key(struct fsck_Xtree_info *, struct fsck_inode_record *,
			 struct fsck_ino_msg_info *, int);

int xTree_node_first_in_level(struct fsck_Xtree_info *,
			      struct fsck_inode_record *,
			      struct fsck_ino_msg_info *, int);

int xTree_node_last_in_level(struct fsck_Xtree_info *,
			     struct fsck_inode_record *,
			     struct fsck_ino_msg_info *, int);

int xTree_node_not_first_in_level(struct fsck_Xtree_info *,
				  struct fsck_inode_record *,
				  struct fsck_ino_msg_info *, int);

int xTree_node_not_last_in_level(struct fsck_Xtree_info *,
				 struct fsck_inode_record *,
				 struct fsck_ino_msg_info *, int);

int xTree_process_leaf_extents(xtpage_t *, struct fsck_inode_record *,
			       struct treeQelem *, struct fsck_ino_msg_info *,
			       int8_t, int);

/* VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV */

/*****************************************************************************
 * NAME: find_first_leaf
 *
 * FUNCTION:  Get the ordinal number of the aggregate block containing the
 *            first leaf node in the B+ Tree, type xTree, rooted in the
 *            given inode.
 *
 * PARAMETERS:
 *      inoptr           - input - pointer to the inode in which the xTree
 *                                 is rooted
 *      addr_leaf_ptr    - input - pointer to a variable in which to return
 *                                 the address of the leaf in an fsck buffer.
 *      leaf_agg_offset  - input - offset, from the beginning of the
 *                                 aggregate, in aggregate blocks, of the
 *                                 leftmost leaf in the xTree
 *      is_inline        - input - pointer to a variable in which to return:
 *                                 !0 if the inode's data is inline (no leaf)
 *                                  0 if the inode's data is not inline
 *      is_rootleaf      - input - pointer to a variable in which to return:
 *                                 !0 if the xTree's root node is a leaf
 *                                  0 if the xTree's root node is an internal
 *                                        node
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int find_first_leaf(struct dinode *inoptr,
		    xtpage_t ** addr_leaf_ptr,
		    int64_t * leaf_agg_offset,
		    int8_t * is_inline, int8_t * is_rootleaf)
{
	int ffl_rc = FSCK_OK;
	xtpage_t *xtpg_ptr;
	xad_t *xad_ptr;
	int64_t first_child_addr = 0;

	*is_rootleaf = 0;	/* assume inode has no data */
	*is_inline = 0;		/* assume inode has no data */
	*addr_leaf_ptr = NULL;	/* assume inode has no data */
	*leaf_agg_offset = 0;	/* assume inode has no data */
	xtpg_ptr = (xtpage_t *) & (inoptr->di_btroot);

	if (xtpg_ptr->header.flag & BT_LEAF) {
		/* it's a root-leaf */
		*is_rootleaf = -1;
		*leaf_agg_offset = addressPXD(&(inoptr->di_ixpxd));
	} else {
		/* it's a tree */
		while ((ffl_rc == FSCK_OK) && (*addr_leaf_ptr == NULL)) {
			if (xtpg_ptr->header.flag & BT_LEAF) {
				/* found it!  */
				*addr_leaf_ptr = xtpg_ptr;
				*leaf_agg_offset = first_child_addr;
			} else {
				/* keep moving down the tree */
				xad_ptr = &(xtpg_ptr->xad[XTENTRYSTART]);
				first_child_addr = addressXAD(xad_ptr);
				ffl_rc = node_get(first_child_addr, &xtpg_ptr);
			}
		}
	}

	return (ffl_rc);
}

/*****************************************************************************
 * NAME: init_xtree_root
 *
 * FUNCTION:  Initialize the btroot in the given inode as an empty (big)
 *                 xtree root.  Adjust di_nblocks and di_size to match.
 *
 * PARAMETERS:
 *      inoptr           - input - pointer to the inode in which the xTree
 *                                 root should be initialized
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int init_xtree_root(struct dinode *inoptr)
{
	int ixr_rc = FSCK_OK;
	xtpage_t *xtpg_ptr;

	xtpg_ptr = (xtpage_t *) & (inoptr->di_btroot);
	xtpg_ptr->header.flag = (DXD_INDEX | BT_ROOT | BT_LEAF);
	xtpg_ptr->header.maxentry = XTROOTMAXSLOT;
	xtpg_ptr->header.nextindex = XTENTRYSTART;
	inoptr->di_nblocks = 0;
	inoptr->di_size = 0;

	return (ixr_rc);
}

/*****************************************************************************
 * NAME: process_valid_data
 *
 * FUNCTION:  Perform the desired action on the xTree rooted in the given
 *            inode, assume that the xTree has a valid structure.  (I.e.,
 *            that the tree has already been validated.)
 *
 * PARAMETERS:
 *      inoptr          - input - pointer to the inode in which the xTree is
 *                                rooted
 *      inoidx          - input - ordinal number of the inode
 *      inorecptr       - input - pointer to the fsck inode record describing
 *                                the inode in which the xTree is rooted
 *      msg_info_ptr    - input - pointer to data needed for messages about
 *                                the inode in which the xTree is rooted
 *      desired_action  - input - { FSCK_RECORD | FSCK_UNRECORD | FSCK_QUERY }
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int process_valid_data(struct dinode *inoptr,
		       uint32_t inoidx,
		       struct fsck_inode_record *inorecptr,
		       struct fsck_ino_msg_info *msg_info_ptr,
		       int desired_action)
{
	int pvd_rc = FSCK_OK;
	int xad_idx;
	xtpage_t *xtpage_ptr;
	xtpage_t *this_xtpage;
	xad_t *xad_ptr;
	int64_t node_addr_fsblks;
	int64_t first_child_addr;
	int64_t first_fsblk;
	int64_t num_fsblks;
	int8_t extent_is_valid;
	int8_t is_EA = 0;
	int8_t is_ACL = 0;
	int8_t is_rootnode;
	uint32_t block_count;

	if (ISDIR(inoptr->di_mode))
		xtpage_ptr = (xtpage_t *) & (inoptr->di_dirtable);
	else
		xtpage_ptr = (xtpage_t *) & (inoptr->di_btroot);

	is_rootnode = -1;

	if (xtpage_ptr->header.flag == 0)
		goto out;

	/* there is data for this inode */
	while ((pvd_rc == FSCK_OK) && (xtpage_ptr != NULL)) {
		/* while not done processing the tree */
		/*
		 * this node is a first child.  if it isn't a leaf, get
		 * the address of its first child
		 */
		if (xtpage_ptr->header.flag & BT_LEAF) {
			/* it's a leaf */
			first_child_addr = 0;
		} else {	/* else it's not a leaf */
			/* the first child */
			xad_ptr = &(xtpage_ptr->xad[XTENTRYSTART]);
			first_child_addr = addressXAD(xad_ptr);
		}

		/*
		 * process the current level
		 */
		/* first node in the level */
		this_xtpage = xtpage_ptr;
		while ((pvd_rc == FSCK_OK) && (this_xtpage != NULL)) {
			/* process all nodes on the level */
			for (xad_idx = XTENTRYSTART;
			     ((xad_idx < this_xtpage->header.nextindex)
			      && (pvd_rc == FSCK_OK)); xad_idx++) {
				/* for each xad in the xtpage */
				xad_ptr = &(this_xtpage->xad[xad_idx]);
				first_fsblk = addressXAD(xad_ptr);
				num_fsblks = lengthXAD(xad_ptr);
				pvd_rc =
				    process_extent(inorecptr,
						   num_fsblks,
						   first_fsblk, is_EA,
						   is_ACL, msg_info_ptr,
						   &block_count,
						   &extent_is_valid,
						   desired_action);
				if ((desired_action == FSCK_RECORD) ||
				    (desired_action == FSCK_RECORD_DUPCHECK)) {
					agg_recptr->blocks_this_fset +=
					    block_count;
					agg_recptr->this_inode.all_blks +=
					    block_count;
					if (first_child_addr == 0) {
						/* this is a leaf */
						agg_recptr->
						    this_inode.data_blks +=
						    block_count;
					}
				} else if (desired_action == FSCK_UNRECORD) {
					agg_recptr->blocks_this_fset -=
					    block_count;
					agg_recptr->this_inode.all_blks -=
					    block_count;
					if (first_child_addr == 0) {
						/* this is a leaf */
						agg_recptr->
						    this_inode.data_blks -=
						    block_count;
					}
				}
			}

			if (is_rootnode) {
				/* root has no siblings */
				is_rootnode = 0;
				this_xtpage = NULL;
			} else if (this_xtpage->header.next == ((int64_t) 0)) {
				/* this is rightmost */
				this_xtpage = NULL;
			} else {
				/* else there is a right sibling/cousin */
				node_addr_fsblks = this_xtpage->header.next;
				pvd_rc =
				    node_get(node_addr_fsblks, &this_xtpage);
			}
		}
		/*
		 * if not done, go down to the next level of the tree
		 */
		if (first_child_addr == 0) {
			/* done! */
			xtpage_ptr = NULL;
		} else {
			/* get the first child/cousin in the next level */
			pvd_rc = node_get(first_child_addr, &xtpage_ptr);
		}
	}
      out:
	return (pvd_rc);
}

/*****************************************************************************
 * NAME: xTree_binsrch_page
 *
 * FUNCTION:  Perform a binary search on the xad's in the given xTree node
 *
 * PARAMETERS:
 *      xtpg_ptr       - input - pointer to the xTree node to search
 *      given_offset   - input - offset to match to an xad key
 *      xad_selected   - input - pointer to a variable in which to return:
 *                               !0 if the search was successful
 *                                0 if the search was not successful
 *      selected_idx   - input - the ordinal value of xad, within the node,
 *                               of the xad whose key matches given_offset
 *                               (if any)
 *      not_allocated  - input - * currently unused *
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int xTree_binsrch_page(xtpage_t * xtpg_ptr,
		       int64_t given_offset,
		       int8_t * xad_selected,
		       int16_t * selected_idx, int8_t * not_allocated)
{
	int xbp_rc = FSCK_OK;
	int16_t lowidx, mididx, highidx;
	int64_t this_offset;

	lowidx = XTENTRYSTART;
	highidx = xtpg_ptr->header.nextindex - 1;
	*xad_selected = 0;

	if (lowidx > highidx)
		return -EIO;

	while ((!(*xad_selected)) && (xbp_rc == FSCK_OK)) {

		if ((highidx == lowidx) || ((highidx - lowidx) == 1)) {
			/* at most 1 apart */
			if (given_offset < offsetXAD(&xtpg_ptr->xad[highidx])) {
				*selected_idx = lowidx;
				*xad_selected = -1;
			} else {
				*selected_idx = highidx;
				*xad_selected = -1;
			}
		} else {
			/* far enough apart to continue algorithm */
			mididx = ((highidx - lowidx) >> 1) + lowidx;
			this_offset = offsetXAD(&(xtpg_ptr->xad[mididx]));

			if (given_offset == this_offset) {
				/* it's a match */
				*selected_idx = mididx;
				*xad_selected = -1;
			} else if (given_offset < this_offset) {
				/* this one is greater */
				if (given_offset >
				    offsetXAD(&(xtpg_ptr->xad[mididx - 1]))) {
					/* the one before this one is less */
					*selected_idx = mididx - 1;
					*xad_selected = -1;
				} else {
					/* the one before is not less */
					/* reset the range */
					highidx = mididx;
				}
			} else {
				/* this one is less */
				if (given_offset <
				    offsetXAD(&(xtpg_ptr->xad[mididx + 1]))) {
					/* the one after this one is greater */
					*selected_idx = mididx;
					*xad_selected = -1;
				} else {
					/* the one after is not greater */
					/* reset the range */
					lowidx = mididx;
				}
			}
		}
	}

	return (xbp_rc);
}

/*****************************************************************************
 * NAME: xTree_node_first_key
 *
 * FUNCTION:  Helper routine for xTree_processing
 *
 * PARAMETERS:
 *      xtiptr          - input - pointer to an fsck record describing the
 *                                xTree
 *      inorecptr       - input - pointer to the fsck inode record describing
 *                                the inode in which the xTree is rooted
 *      msg_info_ptr    - input - pointer to data needed for messages about
 *                                the inode in which the xTree is rooted
 *      desired_action  - input - { FSCK_RECORD | FSCK_RECORD_DUPCHECK |
 *                                  FSCK_UNRECORD | FSCK_QUERY }
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int xTree_node_first_key(struct fsck_Xtree_info *xtiptr,
			 struct fsck_inode_record *inorecptr,
			 struct fsck_ino_msg_info *msg_info_ptr,
			 int desired_action)
{
	int xnfk_rc = FSCK_OK;
	/*
	 * the key in the 1st xad must match the key in the parent
	 * node's xad describing this node
	 */
	if (xtiptr->this_Qel->node_first_offset &&
	    (xtiptr->this_key != xtiptr->this_Qel->node_first_offset)) {
		/* invalid key in 1st xad */
		inorecptr->ignore_alloc_blks = 1;
		if (desired_action == FSCK_RECORD_DUPCHECK) {
			/* not reported yet */
			fsck_send_msg(fsck_BADKEYS,
				      fsck_ref_msg(msg_info_ptr->msg_inotyp),
				      fsck_ref_msg(msg_info_ptr->msg_inopfx),
				      msg_info_ptr->msg_inonum,
				      5);
		}
		goto out;
	}

	/* 1st xad might be ok */
	if (xtiptr->last_level != xtiptr->this_Qel->node_level)
		goto out;

	/* not 1st in level */
	if ((xtiptr->dense_file) && (xtiptr->xtp_ptr->header.flag & BT_LEAF)) {
		/* a leaf node in a dense file */
		if (xtiptr->this_key != (xtiptr->last_key + 1)) {
			/* a gap in a dense file */
			inorecptr->ignore_alloc_blks = 1;
			if (desired_action == FSCK_RECORD_DUPCHECK) {
				/* not reported yet */
				fsck_send_msg(fsck_BADINOINTERNGAP,
					      fsck_ref_msg(msg_info_ptr->msg_inopfx),
					      msg_info_ptr->msg_inonum,
					      (long long) xtiptr->last_key);
			}
		}
	} else {
		/* not a leaf node in a dense file */
		if (xtiptr->this_key <= xtiptr->last_key) {
			/* the extents overlap! */
			inorecptr->ignore_alloc_blks = 1;
			if (desired_action == FSCK_RECORD_DUPCHECK) {
				/* not reported yet */
				fsck_send_msg(fsck_BADKEYS,
					      fsck_ref_msg(msg_info_ptr->msg_inotyp),
					      fsck_ref_msg(msg_info_ptr->msg_inopfx),
					      msg_info_ptr->msg_inonum, 6);
			}
		}
	}

      out:
	return (xnfk_rc);
}

/*****************************************************************************
 * NAME: xTree_node_first_in_level
 *
 * FUNCTION:  Helper routine for xTree_processing
 *
 * PARAMETERS:
 *      xtiptr          - input - pointer to an fsck record describing the
 *                                xTree
 *      inorecptr       - input - pointer to the fsck inode record describing
 *                                the inode in which the xTree is rooted
 *      msg_info_ptr    - input - pointer to data needed for messages about
 *                                the inode in which the xTree is rooted
 *      desired_action  - input - { FSCK_RECORD | FSCK_RECORD_DUPCHECK |
 *                                  FSCK_UNRECORD | FSCK_QUERY }
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int xTree_node_first_in_level(struct fsck_Xtree_info *xtiptr,
			      struct fsck_inode_record *inorecptr,
			      struct fsck_ino_msg_info *msg_info_ptr,
			      int desired_action)
{
	int xnfil_rc = FSCK_OK;

	if (xtiptr->xtp_ptr->header.prev != 0) {
		/* bad back ptr! */
		inorecptr->ignore_alloc_blks = 1;

		if (desired_action == FSCK_RECORD_DUPCHECK) {
			/* not reported yet */
			fsck_send_msg(fsck_BADBSBLCHN,
				      fsck_ref_msg(msg_info_ptr->msg_inotyp),
				      fsck_ref_msg(msg_info_ptr->msg_inopfx),
				      msg_info_ptr->msg_inonum);
		}
	}
	return (xnfil_rc);
}

/*****************************************************************************
 * NAME: xTree_node_last_in_level
 *
 * FUNCTION:  Helper routine for xTree_processing
 *
 * PARAMETERS:
 *      xtiptr          - input - pointer to an fsck record describing the
 *                                xTree
 *      inorecptr       - input - pointer to the fsck inode record describing
 *                                the inode in which the xTree is rooted
 *      msg_info_ptr    - input - pointer to data needed for messages about
 *                                the inode in which the xTree is rooted
 *      desired_action  - input - { FSCK_RECORD | FSCK_RECORD_DUPCHECK |
 *                                  FSCK_UNRECORD | FSCK_QUERY }
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int xTree_node_last_in_level(struct fsck_Xtree_info *xtiptr,
			     struct fsck_inode_record *inorecptr,
			     struct fsck_ino_msg_info *msg_info_ptr,
			     int desired_action)
{
	int xnlil_rc = FSCK_OK;

	if (xtiptr->xtp_ptr->header.next != 0) {
		/* bad forward ptr! */
		inorecptr->ignore_alloc_blks = 1;
		if (desired_action == FSCK_RECORD_DUPCHECK) {
			/* not reported yet */
			fsck_send_msg(fsck_BADFSBLCHN,
				      fsck_ref_msg(msg_info_ptr->msg_inotyp),
				      fsck_ref_msg(msg_info_ptr->msg_inopfx),
				      msg_info_ptr->msg_inonum);
		}
	}
	return (xnlil_rc);
}

/*****************************************************************************
 * NAME: xTree_node_not_first_in_level
 *
 * FUNCTION:  Helper routine for xTree_processing
 *
 * PARAMETERS:
 *      xtiptr          - input - pointer to an fsck record describing the
 *                                xTree
 *      inorecptr       - input - pointer to the fsck inode record describing
 *                                the inode in which the xTree is rooted
 *      msg_info_ptr    - input - pointer to data needed for messages about
 *                                the inode in which the xTree is rooted
 *      desired_action  - input - { FSCK_RECORD | FSCK_RECORD_DUPCHECK |
 *                                  FSCK_UNRECORD | FSCK_QUERY }
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int xTree_node_not_first_in_level(struct fsck_Xtree_info *xtiptr,
				  struct fsck_inode_record *inorecptr,
				  struct fsck_ino_msg_info *msg_info_ptr,
				  int desired_action)
{
	int xnnfil_rc = FSCK_OK;

	if (xtiptr->xtp_ptr->header.prev != xtiptr->last_node_addr) {
		/* bad back ptr! */
		inorecptr->ignore_alloc_blks = 1;

		if (desired_action == FSCK_RECORD_DUPCHECK) {
			/* not reported yet */
			fsck_send_msg(fsck_BADBSBLCHN,
				      fsck_ref_msg(msg_info_ptr->msg_inotyp),
				      fsck_ref_msg(msg_info_ptr->msg_inopfx),
				      msg_info_ptr->msg_inonum);
		}
	}
	return (xnnfil_rc);
}

/*****************************************************************************
 * NAME: xTree_node_not_last_in_level
 *
 * FUNCTION:  Helper routine for xTree_processing
 *
 * PARAMETERS:
 *      xtiptr          - input - pointer to an fsck record describing the
 *                                xTree
 *      inorecptr       - input - pointer to the fsck inode record describing
 *                                the inode in which the xTree is rooted
 *      msg_info_ptr    - input - pointer to data needed for messages about
 *                                the inode in which the xTree is rooted
 *      desired_action  - input - { FSCK_RECORD | FSCK_RECORD_DUPCHECK |
 *                                  FSCK_UNRECORD | FSCK_QUERY }
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int xTree_node_not_last_in_level(struct fsck_Xtree_info *xtiptr,
				 struct fsck_inode_record *inorecptr,
				 struct fsck_ino_msg_info *msg_info_ptr,
				 int desired_action)
{
	int xnnlil_rc = FSCK_OK;

	if (xtiptr->xtp_ptr->header.next != xtiptr->next_Qel->node_addr) {
		/* bad forward ptr! */
		inorecptr->ignore_alloc_blks = 1;
		if (desired_action == FSCK_RECORD_DUPCHECK) {
			/* not reported yet */
			fsck_send_msg(fsck_BADFSBLCHN,
				      fsck_ref_msg(msg_info_ptr->msg_inotyp),
				      fsck_ref_msg(msg_info_ptr->msg_inopfx),
				      msg_info_ptr->msg_inonum);
		}
	} else {
		/* forward sibling pointer is correct */
		if (xtiptr->this_Qel->last_ext_uneven) {
			/* last extent described
			 * by this node is not an even number of
			 * 4096 pages but it can't be the last extent
			 * allocated to the inode
			 */
			inorecptr->ignore_alloc_blks = 1;
			if (desired_action == FSCK_RECORD_DUPCHECK) {
				/* not reported yet */
				xtiptr->xad_ptr =
				    &(xtiptr->
				      xtp_ptr->xad[xtiptr->xtp_ptr->header.
						   nextindex - 1]);
				xtiptr->this_key = offsetXAD(xtiptr->xad_ptr);
				xtiptr->ext_length = lengthXAD(xtiptr->xad_ptr);
				fsck_send_msg(fsck_BADINOODDINTRNEXT,
					      fsck_ref_msg(msg_info_ptr->msg_inotyp),
					      fsck_ref_msg(msg_info_ptr->msg_inopfx),
					      msg_info_ptr->msg_inonum,
					      (long long) xtiptr->this_key,
					      xtiptr->ext_length);
			}
		}
	}

	return (xnnlil_rc);
}

/*****************************************************************************
 * NAME: xTree_process_internal_extents
 *
 * FUNCTION:  Helper routine for xTree_processing
 *
 * PARAMETERS:
 *      xtpg_ptr        - input - pointer to the internal node in an fsck
 *                                buffer
 *      ino_recptr      - input - pointer to the fsck inode record describing
 *                                the inode in which the xTree is rooted
 *      Q_elptr         - input - address of an fsck Q element pointer
 *                                describing the internal node
 *      msg_info_ptr    - input - pointer to data needed for messages about
 *                                the inode in which the xTree is rooted
 *      desired_action  - input - { FSCK_RECORD | FSCK_RECORD_DUPCHECK |
 *                                  FSCK_UNRECORD | FSCK_QUERY }
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int xTree_process_internal_extents(xtpage_t * xtpg_ptr,
				   struct fsck_inode_record *ino_recptr,
				   struct treeQelem *Q_elptr,
				   struct fsck_ino_msg_info *msg_info_ptr,
				   int desired_action)
{
	int xpie_rc = FSCK_OK;
	int64_t last_key, this_key;
	uint32_t xadidx;
	struct treeQelem *new_Qelptr;
	uint32_t ext_length, adjusted_length;
	int64_t ext_addr;
	int8_t ext_ok;
	int8_t is_EA = 0;
	int8_t is_ACL = 0;
	xad_t *xad_ptr;
	uint8_t flag_mask;

	flag_mask =
	    ~(XAD_NEW | XAD_EXTENDED | XAD_COMPRESSED | XAD_NOTRECORDED |
	      XAD_COW);

	last_key = -1;

	for (xadidx = XTENTRYSTART;
	     ((xadidx < xtpg_ptr->header.nextindex) &&
	      (xpie_rc == FSCK_OK) && (!ino_recptr->ignore_alloc_blks));
	     xadidx++) {
		xad_ptr = &(xtpg_ptr->xad[xadidx]);

		if ((xad_ptr->flag & flag_mask)) {
			/* bad flag value */
			ino_recptr->ignore_alloc_blks = 1;
			goto out;
		}

		this_key = offsetXAD(xad_ptr);
		if (this_key <= last_key) {
			/* these keys MUST ascend */
			ino_recptr->ignore_alloc_blks = 1;
			if (desired_action == FSCK_RECORD_DUPCHECK) {
				/* first detection */
				fsck_send_msg(fsck_BADKEYS,
					      fsck_ref_msg(msg_info_ptr->msg_inotyp),
					      fsck_ref_msg(msg_info_ptr->msg_inopfx),
					      msg_info_ptr->msg_inonum,
					      7);
			}
			goto out;
		}
		/* key looks ok from here */
		last_key = this_key;
		ext_addr = addressXAD(xad_ptr);
		ext_length = lengthXAD(xad_ptr);
		xpie_rc = process_extent(ino_recptr, ext_length, ext_addr,
					 is_EA, is_ACL, msg_info_ptr,
					 &adjusted_length, &ext_ok,
					 desired_action);
		if ((xpie_rc != FSCK_OK) || !ext_ok)
			goto out;

		/* extent is good */
		if ((desired_action == FSCK_RECORD) ||
		    (desired_action == FSCK_RECORD_DUPCHECK)) {
			agg_recptr->blocks_this_fset += adjusted_length;
			agg_recptr->this_inode.all_blks += adjusted_length;
		} else if (desired_action == FSCK_UNRECORD) {
			agg_recptr->blocks_this_fset -= adjusted_length;
			agg_recptr->this_inode.all_blks -= adjusted_length;
		}

		xpie_rc = treeQ_get_elem(&new_Qelptr);
		if (xpie_rc != FSCK_OK)
			goto out;

		/* got a queue element */
		new_Qelptr->node_level = Q_elptr->node_level + 1;
		new_Qelptr->node_addr = ext_addr;
		PXDaddress(&(new_Qelptr->node_pxd), ext_addr);
		PXDlength(&(new_Qelptr->node_pxd), ext_length);
		new_Qelptr->node_first_offset = this_key;
		xpie_rc = treeQ_enqueue(new_Qelptr);
	}

      out:
	return (xpie_rc);
}

/*****************************************************************************
 * NAME: xTree_process_leaf_extents
 *
 * FUNCTION:  Helper routine for xTree_processing
 *
 * PARAMETERS:
 *      xtpg_ptr        - input - pointer to the leaf node in an fsck buffer
 *      inorecptr       - input - pointer to the fsck inode record describing
 *                                the inode in which the xTree is rooted
 *      Q_elptr         - input - address of an fsck Q element pointer
 *                                describing the leaf
 *      msg_info_ptr    - input - pointer to data needed for messages about the
 *                                inode in which the xTree is rooted
 *      dense_file      - input - !0 => the xTree describes a dense file
 *                                 0 => the xTree describes a file which may
 *                                      be sparse
 *      desired_action  - input - { FSCK_RECORD | FSCK_RECORD_DUPCHECK |
 *                                  FSCK_UNRECORD | FSCK_QUERY }
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int xTree_process_leaf_extents(xtpage_t * xtpg_ptr,
			       struct fsck_inode_record *ino_recptr,
			       struct treeQelem *Q_elptr,
			       struct fsck_ino_msg_info *msg_info_ptr,
			       int8_t dense_file, int desired_action)
{
	int xple_rc = FSCK_OK;
	int64_t last_key, this_key;
	uint32_t xadidx;
	uint32_t ext_length, adjusted_length;
	int64_t ext_addr;
	int8_t ext_ok;
	int8_t is_EA = 0;
	int8_t is_ACL = 0;
	xad_t *xad_ptr;
	uint32_t ext_pages;
	uint8_t flag_mask;

	flag_mask =
	    ~(XAD_NEW | XAD_EXTENDED | XAD_COMPRESSED | XAD_NOTRECORDED |
	      XAD_COW);

	last_key = -1;

	for (xadidx = XTENTRYSTART;
	     ((xadidx < xtpg_ptr->header.nextindex) &&
	      (xple_rc == FSCK_OK) && (!ino_recptr->ignore_alloc_blks));
	     xadidx++) {

		xad_ptr = &(xtpg_ptr->xad[xadidx]);

		if ((xad_ptr->flag & flag_mask)) {
			/* bad flag value */
			ino_recptr->ignore_alloc_blks = 1;
		}

		this_key = offsetXAD(xad_ptr);

		if ((last_key != -1) && (!ino_recptr->ignore_alloc_blks)) {
			/* not the first key */
			if (this_key <= last_key) {
				/* these keys MUST ascend */
				ino_recptr->ignore_alloc_blks = 1;
				if (desired_action == FSCK_RECORD_DUPCHECK) {
					/* first detection */
					fsck_send_msg(fsck_BADKEYS,
						      fsck_ref_msg(msg_info_ptr->msg_inotyp),
						      fsck_ref_msg(msg_info_ptr->msg_inopfx),
						      msg_info_ptr->msg_inonum,
						      8);
				}
				goto out;
			}
			/* the keys do ascend */
			if ((dense_file)
			    && (this_key != (last_key + 1))) {
				/* a dense file with a gap! */
				ino_recptr->ignore_alloc_blks = 1;
				if (desired_action == FSCK_RECORD_DUPCHECK) {
					/* first detection */
					fsck_send_msg(fsck_BADINOINTERNGAP,
						      fsck_ref_msg(msg_info_ptr->msg_inopfx),
						      msg_info_ptr->msg_inonum,
						      msg_info_ptr->msg_inonum);
				}
				goto out;
			}
		}

		ext_addr = addressXAD(xad_ptr);
		ext_length = lengthXAD(xad_ptr);
		last_key = this_key + ext_length - 1;
		agg_recptr->this_inode.data_size =
		    (last_key + 1) * sb_ptr->s_bsize;
		/*
		 * all extents (except the very last one for the inode) must
		 * be in full (4096 byte) pages.
		 */
		ext_pages = ext_length >> agg_recptr->log2_blksperpg;
		if ((ext_length != (ext_pages << agg_recptr->log2_blksperpg))
		    && (!(ino_recptr->badblk_inode))) {
			/*
			 * this one is an odd size and isn't
			 * owned by the bad block inode
			 */
			if (xadidx == (xtpg_ptr->header.nextindex - 1)) {
				/*
				 * this is the last extent for the node
				 * and might be the last for the inode
				 */
				Q_elptr->last_ext_uneven = -1;
			} else {
				/* not the last extent for the xtpage */
				ino_recptr->ignore_alloc_blks = 1;
				if (desired_action == FSCK_RECORD_DUPCHECK) {
					/* first detection */
					fsck_send_msg
					    (fsck_BADINOODDINTRNEXT,
					     fsck_ref_msg(msg_info_ptr->msg_inotyp),
					     fsck_ref_msg(msg_info_ptr->msg_inopfx),
					     msg_info_ptr->msg_inonum,
					     (long long) this_key,
					     ext_length);
				}
				goto out;
			}
		}
		xple_rc = process_extent(ino_recptr, ext_length, ext_addr,
					 is_EA, is_ACL, msg_info_ptr,
					 &adjusted_length, &ext_ok,
					 desired_action);
		if ((desired_action == FSCK_RECORD)
		    || (desired_action == FSCK_RECORD_DUPCHECK)) {
			agg_recptr->blocks_this_fset += adjusted_length;
			agg_recptr->this_inode.all_blks += adjusted_length;
			agg_recptr->this_inode.data_blks += adjusted_length;
		} else if (desired_action == FSCK_UNRECORD) {
			agg_recptr->blocks_this_fset -= adjusted_length;
			agg_recptr->this_inode.all_blks -= adjusted_length;
			agg_recptr->this_inode.data_blks -= adjusted_length;
		}
	}

      out:
	return (xple_rc);
}

/*****************************************************************************
 * NAME: xTree_processing
 *
 * FUNCTION: Validate the structure of the xTree rooted in the given inode
 *           and perform the desired_action on the nodes in the xTree.
 *           Stop processing the xTree if and when any symptom of corruption
 *           is detected.
 *
 * PARAMETERS:
 *      inoptr        - input - pointer to the inode in which the xTree is
 *                                   rooted
 *      inoidx        - input - ordinal number of the inode
 *      inorecptr    - input - pointer to the fsck inode record describing
 *                                   the inode
 *      msg_info_ptr    - input - pointer to data needed for messages about
 *                                        the inode
 *      desired_action  - input - { FSCK_RECORD | FSCK_RECORD_DUPCHECK |
 *                                  FSCK_UNRECORD | FSCK_QUERY |
 *                                  FSCK_FSIM_RECORD_DUPCHECK  |
 *                                  FSCK_FSIM_UNRECORD | FSCK_FSIM_QUERY }
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int xTree_processing(struct dinode *inoptr,
		     uint32_t inoidx,
		     struct fsck_inode_record *inorecptr,
		     struct fsck_ino_msg_info *msg_info_ptr, int desired_action)
{
	int xp_rc = FSCK_OK;
	int8_t old_ignore_alloc_blks = 0;
	int ixpxd_unequal = 0;
	int is_root = -1;
	int not_fsim_tree = -1;
	struct fsck_Xtree_info xtinfo;
	struct fsck_Xtree_info *xtiptr;

	xtiptr = &xtinfo;
	xtiptr->this_Qel = NULL;
	xtiptr->next_Qel = NULL;
	xtiptr->last_level = -1;	/* -1 so the root will be recognized
					 * as 1st node in level 0
					 */
	xtiptr->dense_file = 0;
	xtiptr->leaf_seen = 0;

	if (!(inoptr->di_mode & ISPARSE)) {
		xtiptr->dense_file = -1;
	}

	switch (desired_action) {
	case (FSCK_FSIM_RECORD_DUPCHECK):
		not_fsim_tree = 0;
		desired_action = FSCK_RECORD_DUPCHECK;
		break;
	case (FSCK_FSIM_UNRECORD):
		not_fsim_tree = 0;
		desired_action = FSCK_UNRECORD;
		break;
	case (FSCK_FSIM_QUERY):
		not_fsim_tree = 0;
		desired_action = FSCK_QUERY;
		break;
	default:
		break;
	}

	if (ISDIR(inoptr->di_mode))
		xtiptr->xtp_ptr = (xtpage_t *) & (inoptr->di_dirtable);
	else
		xtiptr->xtp_ptr = (xtpage_t *) & (inoptr->di_btroot);

	if ((!ISDIR(inoptr->di_mode)
	     && (xtiptr->xtp_ptr->header.maxentry != XTROOTINITSLOT)
	     && (xtiptr->xtp_ptr->header.maxentry != XTROOTMAXSLOT))
	    || (ISDIR(inoptr->di_mode)
		&& (xtiptr->xtp_ptr->header.maxentry != XTROOTINITSLOT_DIR))) {
		/* bad maxentry field */
		inorecptr->ignore_alloc_blks = 1;
		if (desired_action == FSCK_RECORD_DUPCHECK) {
			/* not reported yet */
			fsck_send_msg(fsck_BADINOOTHR, "45",
				      fsck_ref_msg(msg_info_ptr->msg_inotyp),
				      fsck_ref_msg(msg_info_ptr->msg_inopfx),
				      msg_info_ptr->msg_inonum);
		}		/* end not reported yet */
		goto out;
	}
	if (xtiptr->xtp_ptr->header.nextindex >
	    xtiptr->xtp_ptr->header.maxentry) {
		/* bad nextindex field */
		inorecptr->ignore_alloc_blks = 1;
		if (desired_action == FSCK_RECORD_DUPCHECK) {
			/* not reported yet */
			fsck_send_msg(fsck_BADINOOTHR, "46",
				      fsck_ref_msg(msg_info_ptr->msg_inotyp),
				      fsck_ref_msg(msg_info_ptr->msg_inopfx),
				      msg_info_ptr->msg_inonum);
		}
		goto out;
	}
	if (xtiptr->xtp_ptr->header.nextindex <= XTENTRYSTART)
		goto out;

	/* data length > 0 */
	if (desired_action != FSCK_RECORD_DUPCHECK) {
		/* not the first pass */
		/*
		 * The first time through we stopped processing allocated
		 * blocks if and when we discovered the tree to be corrupt.
		 * On a 2nd pass we want to stop at the same place.
		 */
		if (inorecptr->ignore_alloc_blks) {
			/* the bit is on */
			old_ignore_alloc_blks = -1;
			inorecptr->ignore_alloc_blks = 0;
		}
	}
	xtiptr->this_key = offsetXAD(&(xtiptr->xtp_ptr->xad[XTENTRYSTART]));

	if (xtiptr->dense_file && (xtiptr->this_key != ((int64_t) 0))) {
		/* a dense file with a gap at the front */
		inorecptr->ignore_alloc_blks = 1;
		if (desired_action == FSCK_RECORD_DUPCHECK) {
			/* not reported yet */
			fsck_send_msg(fsck_BADINOFRONTGAP,
				      fsck_ref_msg(msg_info_ptr->msg_inopfx),
				      msg_info_ptr->msg_inonum);
		}
		goto release;
	}

	xp_rc = treeQ_get_elem(&xtiptr->this_Qel);
	if (xp_rc != FSCK_OK)
		goto out;

	xtiptr->this_Qel->node_level = 0;

	if (xtiptr->xtp_ptr->header.flag & BT_LEAF) {
		/* root leaf */
		if (not_fsim_tree) {
			/* not the FileSet Inode Map tree */
			xp_rc = xTree_process_leaf_extents
			    (xtiptr->xtp_ptr, inorecptr, xtiptr->this_Qel,
			     msg_info_ptr, xtiptr->dense_file, desired_action);
		}
		xtiptr->xad_ptr =
		    &(xtiptr->
		      xtp_ptr->xad[xtiptr->xtp_ptr->header.nextindex - 1]);
		agg_recptr->this_inode.data_size =
		    (int64_t) (offsetXAD(xtiptr->xad_ptr) +
			       lengthXAD(xtiptr->xad_ptr)) * sb_ptr->s_bsize;
		/*
		 * By definition, a root-leaf is the last leaf
		 * for the inode
		 */
	} else {		/* root is not a leaf */
		if (xtiptr->xtp_ptr->header.flag & BT_INTERNAL) {
			/* root internal */
			xp_rc =
			    xTree_process_internal_extents
			    (xtiptr->xtp_ptr, inorecptr,
			     xtiptr->this_Qel, msg_info_ptr, desired_action);
		} else {
			/* invalid flag value! */
			inorecptr->ignore_alloc_blks = 1;
			if (desired_action == FSCK_RECORD_DUPCHECK) {
				/* not reported yet */
				fsck_send_msg(fsck_BADINOOTHR, "50",
					      fsck_ref_msg(msg_info_ptr->msg_inotyp),
					      fsck_ref_msg(msg_info_ptr->msg_inopfx),
					      msg_info_ptr->msg_inonum);
			}
		}
	}

	while ((xp_rc == FSCK_OK) && (!inorecptr->ignore_alloc_blks) &&
	       (agg_recptr->treeQ_back != NULL)) {
		/* nothing fatal and tree looks ok and queue not empty */

		xp_rc = treeQ_dequeue(&xtiptr->next_Qel);

		if (xp_rc != FSCK_OK)
			break;

		if (xtiptr->this_Qel->node_level ==
		    xtiptr->next_Qel->node_level)
			/* it's not the last in its level */
			xp_rc = xTree_node_not_last_in_level(xtiptr, inorecptr,
							     msg_info_ptr,
							     desired_action);
		else if (!is_root)
			/* it is the last in its level */
			xp_rc = xTree_node_last_in_level(xtiptr, inorecptr,
							 msg_info_ptr,
							 desired_action);
		if ((xp_rc != FSCK_OK) || inorecptr->ignore_alloc_blks)
			break;

		/*
		 * save some info about the node already processed
		 * and then move on to the new node
		 */
		xtiptr->last_level = xtiptr->this_Qel->node_level;
		xtiptr->last_node_addr = xtiptr->this_Qel->node_addr;
		xtiptr->xad_ptr =
		    &(xtiptr->
		      xtp_ptr->xad[xtiptr->xtp_ptr->header.nextindex - 1]);
		if (xtiptr->xtp_ptr->header.flag & BT_LEAF) {
			/* it's a leaf */
			xtiptr->last_key = offsetXAD(xtiptr->xad_ptr) +
			    lengthXAD(xtiptr->xad_ptr) - 1;
		} else {
			/* it's an internal node */
			xtiptr->last_key = offsetXAD(xtiptr->xad_ptr);
		}

		xp_rc = treeQ_rel_elem(xtiptr->this_Qel);
		if (xp_rc != FSCK_OK)
			break;

		/* released the older element */
		/* promote newer element */
		xtiptr->this_Qel = xtiptr->next_Qel;
		/* to avoid releasing it twice */
		xtiptr->next_Qel = NULL;
		is_root = 0;
		xp_rc = node_get(xtiptr->this_Qel->node_addr, &xtiptr->xtp_ptr);
		if (xp_rc != FSCK_OK) {	/* bad read! */
			inorecptr->ignore_alloc_blks = 1;
			if (desired_action == FSCK_RECORD_DUPCHECK) {
				/* not reported yet */
				fsck_send_msg(fsck_BADINOOTHR, "42",
					      fsck_ref_msg(msg_info_ptr->msg_inotyp),
					      fsck_ref_msg(msg_info_ptr->msg_inopfx),
					      msg_info_ptr->msg_inonum);
			}
			break;
		}

		/* got the new node */
		if (xtiptr->xtp_ptr->header.maxentry != XTPAGEMAXSLOT) {
			/* bad maxentry field */
			inorecptr->ignore_alloc_blks = 1;
			if (desired_action == FSCK_RECORD_DUPCHECK) {
				/* not reported yet */
				fsck_send_msg(fsck_BADINOOTHR, "43",
					      fsck_ref_msg(msg_info_ptr->msg_inotyp),
					      fsck_ref_msg(msg_info_ptr->msg_inopfx),
					      msg_info_ptr->msg_inonum);
			}
		} else if (xtiptr->xtp_ptr->header.nextindex >
			   xtiptr->xtp_ptr->header.maxentry) {
			/* bad nextindex field */
			inorecptr->ignore_alloc_blks = 1;
			if (desired_action == FSCK_RECORD_DUPCHECK) {
				/* not reported yet */
				fsck_send_msg(fsck_BADINOOTHR, "44",
					      fsck_ref_msg(msg_info_ptr->msg_inotyp),
					      fsck_ref_msg(msg_info_ptr->msg_inopfx),
					      msg_info_ptr->msg_inonum);
			}
		}
		if (inorecptr->ignore_alloc_blks)
			break;

		if (xtiptr->last_level != xtiptr->this_Qel->node_level) {
			/* this is a new level */
			xtiptr->last_key = 0;
			xp_rc =
			    xTree_node_first_in_level
			    (xtiptr, inorecptr, msg_info_ptr, desired_action);
		} else {
			xp_rc =
			    xTree_node_not_first_in_level
			    (xtiptr, inorecptr, msg_info_ptr, desired_action);
		}

		ixpxd_unequal = memcmp((void *)
				       &(xtiptr->xtp_ptr->header.self), (void *)
				       &(xtiptr->this_Qel->node_pxd),
				       sizeof (pxd_t));
		if (ixpxd_unequal) {
			/* bad self field in header */
			inorecptr->ignore_alloc_blks = 1;
			if (desired_action == FSCK_RECORD_DUPCHECK) {
				/* not reported yet */
				fsck_send_msg(fsck_BADINONODESELF,
					      fsck_ref_msg(msg_info_ptr->msg_inotyp),
					      fsck_ref_msg(msg_info_ptr->msg_inopfx),
					      msg_info_ptr->msg_inonum);
			}
		} else if (xtiptr->xtp_ptr->header.nextindex > XTENTRYSTART) {
			if (xtiptr->xtp_ptr->header.flag & BT_LEAF) {
				/* it's a leaf */
				xtiptr->xad_ptr
				    =
				    &
				    (xtiptr->xtp_ptr->xad
				     [xtiptr->xtp_ptr->header.nextindex - 1]);
				agg_recptr->this_inode.data_size =
				    (int64_t) (offsetXAD
					       (xtiptr->xad_ptr) +
					       lengthXAD
					       (xtiptr->xad_ptr)) *
				    sb_ptr->s_bsize;
				/*
				 * Just in case this is the
				 * last leaf for the inode
				 */
			}
			xtiptr->xad_ptr = &(xtiptr->xtp_ptr->xad[XTENTRYSTART]);
			xtiptr->this_key = offsetXAD(xtiptr->xad_ptr);
			xtiptr->ext_length = lengthXAD(xtiptr->xad_ptr);
		} else {
			/* an empty non-root node */
			inorecptr->ignore_alloc_blks = 1;
			if (desired_action == FSCK_RECORD_DUPCHECK) {
				/* not reported yet */
				fsck_send_msg(fsck_BADINOMTNODE,
					      fsck_ref_msg(msg_info_ptr->msg_inotyp),
					      fsck_ref_msg(msg_info_ptr->msg_inopfx),
					      msg_info_ptr->msg_inonum);
			}
		}

		if ((xp_rc != FSCK_OK) || inorecptr->ignore_alloc_blks)
			break;

		xp_rc = xTree_node_first_key(xtiptr, inorecptr,
					     msg_info_ptr, desired_action);
		if ((xp_rc != FSCK_OK) || inorecptr->ignore_alloc_blks)
			break;

		if (xtiptr->xtp_ptr->header.flag & BT_LEAF) {
			/* a leaf node */
			if (not_fsim_tree) {
				/* not the FileSet Inode Map tree */
				xp_rc =
				    xTree_process_leaf_extents
				    (xtiptr->xtp_ptr, inorecptr,
				     xtiptr->this_Qel,
				     msg_info_ptr,
				     xtiptr->dense_file, desired_action);
			}
		} else {
			/* not a leaf node */
			if (xtiptr->xtp_ptr->header.flag & BT_INTERNAL) {
				/* an internal node */
				xp_rc =
				    xTree_process_internal_extents
				    (xtiptr->xtp_ptr, inorecptr,
				     xtiptr->this_Qel,
				     msg_info_ptr, desired_action);
			} else {
				/* an invalid flag value! */
				inorecptr->ignore_alloc_blks = 1;
				if (desired_action == FSCK_RECORD_DUPCHECK) {
					/* not reported yet */
					fsck_send_msg(fsck_BADINOOTHR, "51",
						      fsck_ref_msg(msg_info_ptr->msg_inotyp),
						      fsck_ref_msg(msg_info_ptr->msg_inopfx),
						      msg_info_ptr->msg_inonum);
				}
			}
		}
		if ((xp_rc == FSCK_OK) && (!inorecptr->ignore_alloc_blks) &&
		    (agg_recptr->treeQ_back == NULL)) {
			/* nothing fatal and tree looks ok and queue is empty */
			xp_rc = xTree_node_last_in_level(xtiptr, inorecptr,
							 msg_info_ptr,
							 desired_action);
		}
	}

      release:
	/*
	 * there's at least 1 more Q element to release for this node, and
	 * if the tree is bad there may still be some on the queue as well.
	 *
	 * (If there's a processing error all the dynamic storage is going
	 * to be released so there's no point in preparing these elements
	 * for reuse.)
	 */
	if ((xp_rc == FSCK_OK) && (xtiptr->this_Qel != NULL)) {
		xp_rc = treeQ_rel_elem(xtiptr->this_Qel);
	}

	if ((xp_rc == FSCK_OK) && (xtiptr->next_Qel != NULL)) {
		xp_rc = treeQ_rel_elem(xtiptr->next_Qel);
	}

	agg_recptr->treeQ_back = NULL;

	while ((xp_rc == FSCK_OK) && (agg_recptr->treeQ_front != NULL)) {
		xtiptr->this_Qel = agg_recptr->treeQ_front;
		agg_recptr->treeQ_front = xtiptr->this_Qel->next;
		xp_rc = treeQ_rel_elem(xtiptr->this_Qel);
	}			/* end while */

	if (xp_rc == FSCK_OK) {
		/* not planning to quit */
		if (desired_action != FSCK_RECORD_DUPCHECK) {
			/* we altered the corrupt tree bit */
			if (old_ignore_alloc_blks
			    && !inorecptr->ignore_alloc_blks) {
				/*
				 * the flag is set but the bit didn't get
				 * turned back on.  This means that the first
				 * time we went through this tree we decided
				 * it was corrupt but this time it looked ok.
				 */
				xp_rc = FSCK_INTERNAL_ERROR_8;
			} else if (!old_ignore_alloc_blks
				   && inorecptr->ignore_alloc_blks) {
				/*
				 * the flag is off but the bit got turned on.
				 * This means that the first time we went
				 * through this tree it looked ok but this
				 * time we decided that it is corrupt.
				 */
				xp_rc = FSCK_INTERNAL_ERROR_9;
			}
		}
	}
      out:
	return (xp_rc);
}

/*****************************************************************************
 * NAME: xTree_search
 *
 * FUNCTION:  Search the xTree rooted in the given inode for an xad in a
 *            leaf node which describes the given file offset.
 *
 * PARAMETERS:
 *      inoptr        - input - pointer to the inode in which the xTree is
 *                              rooted
 *      given_key     - input - the key (file offset) to match
 *      addr_xad_ptr  - input - pointer to a variable in which to return the
 *                              address of the xad whose key matches given_key
 *      match_found   - input - pointer to a variable in which to return
 *                              !0 if a matching xad is found
 *                               0 if no match is found
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int xTree_search(struct dinode *inoptr,
		 int64_t given_key, xad_t ** addr_xad_ptr, int8_t * match_found)
{
	int xs_rc = FSCK_OK;
	xtpage_t *xtpg_ptr;
	int8_t extent_located = 0;
	int8_t not_there = 0;
	int16_t chosen_idx;
	int8_t xad_chosen;
	int64_t last_offset;

	if (ISDIR(inoptr->di_mode))
		xtpg_ptr = (xtpage_t *) & (inoptr->di_dirtable);
	else
		xtpg_ptr = (xtpage_t *) & (inoptr->di_btroot);

	while ((!extent_located) && (!not_there) && (xs_rc == FSCK_OK)) {

		if (given_key >
		    offsetXAD(&xtpg_ptr->xad[xtpg_ptr->header.nextindex - 1])) {
			/* follows the start of the last allocation described */
			chosen_idx = xtpg_ptr->header.nextindex - 1;
			xad_chosen = -1;
		} else if (given_key <
			   offsetXAD(&(xtpg_ptr->xad[XTENTRYSTART]))) {
			/* precedes the 1st allocation described */
			not_there = -1;
			xad_chosen = 0;
		} else {
			/* it's somewhere in between */
			xs_rc = xTree_binsrch_page(xtpg_ptr, given_key,
						   &xad_chosen,
						   &chosen_idx, &not_there);
		}

		if ((xs_rc == FSCK_OK) && (xad_chosen)) {
			/* picked one */
			if (xtpg_ptr->header.flag & BT_LEAF) {
				/* it's this one or none */
				/* the last offset in the extent described */
				last_offset =
				    offsetXAD(&(xtpg_ptr->xad[chosen_idx]))
				    + lengthXAD(&(xtpg_ptr->xad[chosen_idx]))
				    - 1;
				if (given_key <= last_offset) {
					/* it's in the range described */
					extent_located = -1;
				} else {
					not_there = -1;
				}
			} else {
				/* this xad describes a B+ Tree node on the
				 *next level down
				 */
				/* read in the next node */
				xs_rc =
				    node_get(addressXAD
					     (&xtpg_ptr->xad[chosen_idx]),
					     &xtpg_ptr);
			}
		}
	}

	if ((extent_located) && (xs_rc == FSCK_OK)) {
		/* found it! */
		*addr_xad_ptr = &(xtpg_ptr->xad[chosen_idx]);
		*match_found = -1;
	} else {
		/* no luck */
		*addr_xad_ptr = NULL;
		*match_found = 0;
	}

	return (xs_rc);
}
