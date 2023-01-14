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
#include <string.h>
/* defines and includes common among the fsck.jfs modules */
#include "xfsckint.h"
#include "jfs_byteorder.h"
#include "jfs_unicode.h"
#include "unicode_to_utf8.h"

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

extern int32_t Uni_Name_len;
extern UniChar Uni_Name[];
extern int32_t Str_Name_len;
extern char Str_Name[];

/*****************************************************************************
 * NAME: backout_ACL
 *
 * FUNCTION: Unrecord all storage allocated for the access control list
 *               (ACL) of the current inode.
 *
 * PARAMETERS:
 *      ino_ptr      - input - pointer to the current inode
 *      ino_recptr  - input - pointer to an fsck inode record describing the
 *                            current inode
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int backout_ACL(struct dinode *ino_ptr, struct fsck_inode_record *ino_recptr)
{
	int bacl_rc = FSCK_OK;
	uint32_t block_count = 0;
	uint32_t extent_length;
	int64_t extent_address;
	int8_t extent_is_valid;
	/*
	 * the following will be passed to extent_unrecord() which will
	 * ignore them.
	 */
	int8_t is_EA = 0;
	int8_t is_ACL = 1;
	struct fsck_ino_msg_info *msg_info_ptr = NULL;

	/*
	 * if the ACL is in an out-of-line extent, release the blocks
	 * allocated for it.
	 */
	if (ino_ptr->di_acl.flag == DXD_EXTENT) {
		extent_length = lengthDXD(&(ino_ptr->di_acl));
		extent_address = addressDXD(&(ino_ptr->di_acl));
		bacl_rc =
		    process_extent(ino_recptr, extent_length, extent_address,
				   is_EA, is_ACL, msg_info_ptr, &block_count,
				   &extent_is_valid, FSCK_UNRECORD);
	}

	/*
	 * backout the blocks in the ACL extent from the running totals for
	 * fileset and inode, but not for the object
	 * represented by the object (because they were never added to that).
	 */
	agg_recptr->blocks_this_fset -= block_count;
	agg_recptr->this_inode.all_blks -= block_count;

	return (bacl_rc);
}

/*****************************************************************************
 * NAME: backout_EA
 *
 * FUNCTION: Unrecord all storage allocated for the extended attributes
 *           (ea) of the current inode.
 *
 * PARAMETERS:
 *      ino_ptr     - input - pointer to the current inode
 *      ino_recptr  - input - pointer to an fsck inode record describing the
 *                            current inode
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int backout_EA(struct dinode *ino_ptr, struct fsck_inode_record *ino_recptr)
{
	int bea_rc = FSCK_OK;
	uint32_t block_count = 0;
	uint32_t extent_length;
	int64_t extent_address;
	int8_t extent_is_valid;
	/*
	 * the following will be passed to extent_unrecord() which will
	 * ignore them.
	 */
	int8_t is_EA = 1;
	int8_t is_ACL = 0;
	struct fsck_ino_msg_info *msg_info_ptr = NULL;

	/*
	 * if the EA is in an out-of-line extent, release the blocks
	 * allocated for it.
	 */
	if (ino_ptr->di_ea.flag == DXD_EXTENT) {
		extent_length = lengthDXD(&(ino_ptr->di_ea));
		extent_address = addressDXD(&(ino_ptr->di_ea));
		bea_rc =
		    process_extent(ino_recptr, extent_length, extent_address,
				   is_EA, is_ACL, msg_info_ptr, &block_count,
				   &extent_is_valid, FSCK_UNRECORD);
	}

	/*
	 * backout the blocks in the EA extent from the running totals for
	 * fileset and inode, but not for the object
	 * represented by the object (because they were never added to that).
	 */
	agg_recptr->blocks_this_fset -= block_count;
	agg_recptr->this_inode.all_blks -= block_count;

	return (bea_rc);
}

/*****************************************************************************
 * NAME: clear_ACL_field
 *
 * FUNCTION: Unrecord all storage allocated for the access control list
 *           (ACL) of the current inode.  Clear the inode ACL field to show
 *           the inode owns no ACL.
 *
 * PARAMETERS:
 *      ino_recptr  - input - pointer to an fsck inode record describing the
 *                            current inode
 *      ino_ptr     - input - pointer to the current inode
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int clear_ACL_field(struct fsck_inode_record *ino_recptr,
		    struct dinode *ino_ptr)
{
	int caf_rc = FSCK_OK;
	dxd_t *dxd_ptr;
	uint32_t block_count = 0;
	uint32_t extent_length;
	int64_t extent_address;
	int8_t extent_is_valid;
	/*
	 * the following will be passed to extent_unrecord() which will
	 * ignore them.
	 */
	int8_t is_EA = 0;
	int8_t is_ACL = -1;
	struct fsck_ino_msg_info *msg_info_ptr = NULL;

	/* locate the EA field in the inode */
	dxd_ptr = &(ino_ptr->di_acl);

	/*
	 * if the ACL is in an out-of-line extent, release the blocks
	 * allocated for it.
	 */
	if ((dxd_ptr->flag == DXD_EXTENT) && (!ino_recptr->ignore_acl_blks)
	    && (!ino_recptr->ignore_alloc_blks)) {
		/* out of line single extent and not flagged to ignore  */
		extent_length = lengthDXD(dxd_ptr);
		extent_address = addressDXD(dxd_ptr);
		caf_rc =
		    process_extent(ino_recptr, extent_length, extent_address,
				   is_EA, is_ACL, msg_info_ptr, &block_count,
				   &extent_is_valid, FSCK_UNRECORD);
		ino_ptr->di_nblocks -= block_count;
		agg_recptr->blocks_for_acls -= block_count;
	}
	/*
	 * Clear the ACL field
	 */
	if (caf_rc == FSCK_OK) {
		dxd_ptr->flag = DXD_CORRUPT;
		/* clear the data length */
		DXDlength(dxd_ptr, 0);
		/* clear the data address */
		DXDaddress(dxd_ptr, 0);

		agg_recptr->blocks_this_fset -= block_count;
	}
	return (caf_rc);
}

/*****************************************************************************
 * NAME: clear_EA_field
 *
 * FUNCTION: Unrecord all storage allocated for the extended attributes
 *           (ea) of the current inode.  Clear the inode ea field to show
 *           the inode owns no ea.
 *
 * PARAMETERS:
 *      ino_recptr  - input - pointer to an fsck inode record describing the
 *                            current inode
 *      ino_ptr     - input - pointer to the current inode
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int clear_EA_field(struct fsck_inode_record *ino_recptr, struct dinode *ino_ptr)
{
	int cef_rc = FSCK_OK;
	dxd_t *dxd_ptr;
	uint32_t block_count = 0;
	uint32_t extent_length;
	int64_t extent_address;
	int8_t extent_is_valid;
	/*
	 * the following will be passed to extent_unrecord() which will
	 * ignore them.
	 */
	int8_t is_EA = -1;
	int8_t is_ACL = 0;
	struct fsck_ino_msg_info *msg_info_ptr = NULL;

	/* locate the EA field in the inode */
	dxd_ptr = &(ino_ptr->di_ea);

	/*
	 * if the EA is in an out-of-line extent, release the blocks
	 * allocated for it.
	 */
	if ((dxd_ptr->flag == DXD_EXTENT) && (!ino_recptr->ignore_ea_blks)
	    && (!ino_recptr->ignore_alloc_blks)) {
		/* out of line single extent and not flagged to ignore  */
		extent_length = lengthDXD(dxd_ptr);
		extent_address = addressDXD(dxd_ptr);
		cef_rc =
		    process_extent(ino_recptr, extent_length, extent_address,
				   is_EA, is_ACL, msg_info_ptr, &block_count,
				   &extent_is_valid, FSCK_UNRECORD);
		agg_recptr->blocks_for_eas -= block_count;
	}
	/*
	 * Clear the EA field
	 */
	if (cef_rc == FSCK_OK) {
		dxd_ptr->flag = 0;
		/* clear the data length */
		DXDlength(dxd_ptr, 0);
		/* clear the data address */
		DXDaddress(dxd_ptr, 0);

		ino_ptr->di_nblocks -= block_count;
		agg_recptr->blocks_this_fset -= block_count;
	}
	return (cef_rc);
}

/*****************************************************************************
 * NAME: display_path
 *
 * FUNCTION: Issue a message to display the given inode path.
 *
 * PARAMETERS:
 *      inoidx      - input - ordinal number of the inode as an integer
 *      inopfx      - input - index (into message catalog) of prefix for
 *                            inode number when displayed in message
 *                            { A | <blank> }
 *      ino_parent  - input - the inode number for the (parent) directory
 *                            whose entry to the current inode is described
 *                            by the contents of inopath.
 *      inopath     - input - pointer to the UniCharacter path which is
 *                            to be displayed.
 *      ino_recptr  - input - pointer to an fsck inode record describing the
 *                            current inode
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int display_path(uint32_t inoidx, int inopfx, uint32_t ino_parent,
		 char *inopath, struct fsck_inode_record *ino_recptr)
{
	int dip_rc = FSCK_OK;

	if ((!ino_recptr->unxpctd_prnts)
	    || (!(ino_recptr->inode_type == directory_inode))) {
		/* not directory w/ mult parents */
		if (ino_recptr->inode_type == directory_inode) {
			fsck_send_msg(fsck_INOPATHOK, fsck_ref_msg(fsck_directory),
				      fsck_ref_msg(inopfx), inoidx, inopath);
		} else if (ino_recptr->inode_type == char_special_inode) {
			fsck_send_msg(fsck_INOPATHOK, fsck_ref_msg(fsck_char_special),
				      fsck_ref_msg(inopfx), inoidx, inopath);
		} else if (ino_recptr->inode_type == block_special_inode) {
			fsck_send_msg(fsck_INOPATHOK, fsck_ref_msg(fsck_block_special),
				      fsck_ref_msg(inopfx), inoidx, inopath);
		} else if (ino_recptr->inode_type == FIFO_inode) {
			fsck_send_msg(fsck_INOPATHOK, fsck_ref_msg(fsck_FIFO),
				      fsck_ref_msg(inopfx), inoidx, inopath);
		} else if (ino_recptr->inode_type == SOCK_inode) {
			fsck_send_msg(fsck_INOPATHOK, fsck_ref_msg(fsck_SOCK),
				      fsck_ref_msg(inopfx), inoidx, inopath);
		} else {
			/* regular file */
			fsck_send_msg(fsck_INOPATHOK, fsck_ref_msg(fsck_file),
				      fsck_ref_msg(inopfx), inoidx, inopath);
		}
	} else {
		/* else a directory w/ multiple parents */
		if (ino_parent == ino_recptr->parent_inonum) {
			/* expected parent */
			fsck_send_msg(fsck_INOPATHCRCT, inopath,
				      fsck_ref_msg(inopfx), inoidx);
		} else {
			/* this is an illegal hard link */
			fsck_send_msg(fsck_INOPATHBAD, fsck_ref_msg(fsck_directory),
				      fsck_ref_msg(inopfx), inoidx, inopath);
		}
	}

	return (dip_rc);
}

/*****************************************************************************
 * NAME: display_paths
 *
 * FUNCTION:  Display all paths to the specified inode.
 *
 * PARAMETERS:
 *      inoidx        - input - ordinal number of the inode as an integer
 *      ino_recptr    - input - pointer to an fsck inode record describing the
 *                              current inode
 *      msg_info_ptr  - input - pointer to a record with information needed
 *                              to issue messages about the current inode
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int display_paths(uint32_t inoidx, struct fsck_inode_record *ino_recptr,
		  struct fsck_ino_msg_info *msg_info_ptr)
{
	int dips_rc = FSCK_OK;
	int intermed_rc;
	uint32_t this_parent = 0;
	struct fsck_inode_ext_record *this_ext = NULL;
	char *this_path_ptr;
	char **this_path_ptr_ptr;
	struct fsck_inode_record *parent_inorecptr;
	int a_path_displayed = 0;

	this_path_ptr_ptr = &this_path_ptr;

	if (inoidx == 2) {
		/* it's the root directory */
		intermed_rc = get_path(inoidx, this_parent, this_path_ptr_ptr,
				       ino_recptr);
		a_path_displayed = -1;
		dips_rc = display_path(inoidx, msg_info_ptr->msg_inopfx,
				       this_parent, this_path_ptr, ino_recptr);
		goto out;

	} else if (ino_recptr->parent_inonum == 0)
		goto out;

	/* at least one parent was observed */
	/*
	 * if this is a directory with illegal hard links then the
	 * inode number in the fsck inode record is the one stored in
	 * the inode on disk.  This routine displays only messages
	 * for parents observed by fsck.
	 */
	if ((ino_recptr->inode_type == directory_inode)
	    && (ino_recptr->unxpctd_prnts)) {
		/* dir with multiple parents */
		this_ext = ino_recptr->ext_rec;
		while ((this_ext != NULL)
		       && (this_ext->ext_type != parent_extension)) {
			this_ext = this_ext->next;
		}

		if (this_ext == NULL) {
			/* something is terribly wrong! */
			dips_rc = FSCK_INTERNAL_ERROR_1;
		}
	} else {
		/* not a dir with multiple parents */
		/*
		 * the 1st parent observed is in the inode record.  Any
		 * others are in extension records.
		 */
		this_parent = ino_recptr->parent_inonum;
		if ((this_parent != ROOT_I) || (!agg_recptr->rootdir_rebuilt)) {
			/*
			 * either this parent isn't the root or else
			 * the root dir has not been rebuilt
			 */
			intermed_rc =
			    get_inorecptr(0, 0, this_parent, &parent_inorecptr);
			if (intermed_rc != FSCK_OK) {
				dips_rc = intermed_rc;
			} else if ((parent_inorecptr->in_use)
				   && (!parent_inorecptr->selected_to_rls)
				   && (!parent_inorecptr->ignore_alloc_blks)) {
				/* got parent record and parent ok so far */
				intermed_rc =
				    get_path(inoidx, this_parent,
					     this_path_ptr_ptr, ino_recptr);
				if (intermed_rc != FSCK_OK) {
					/* unable to obtain 1st path */
					fsck_send_msg(fsck_INOCNTGETPATH,
						      fsck_ref_msg(msg_info_ptr->msg_inopfx),
						      this_parent,
						      fsck_ref_msg(msg_info_ptr->msg_inotyp),
						      fsck_ref_msg(msg_info_ptr->msg_inopfx),
						      inoidx);
					if (intermed_rc < 0) {
						/* it's fatal */
						dips_rc = intermed_rc;
					}
				} else {
					/* 1st path obtained */
					a_path_displayed = -1;
					dips_rc =
					    display_path(inoidx,
							 msg_info_ptr->
							 msg_inopfx,
							 this_parent,
							 this_path_ptr,
							 ino_recptr);

					/*
					 * if there are any more paths to the
					 * inode, find the next parent
					 */
					this_ext = ino_recptr->ext_rec;
					while ((this_ext != NULL)
					       && (this_ext->ext_type !=
						   parent_extension)) {
						this_ext = this_ext->next;
					}
				}
			}
		}
	}

	while ((dips_rc == FSCK_OK) && (this_ext != NULL)) {
		/* there may be more parents */
		if (this_ext->ext_type == parent_extension) {
			/* parent extension */
			this_parent = this_ext->inonum;
			if ((this_parent != ROOT_I)
			    || (!agg_recptr->rootdir_rebuilt)) {
				/*
				 * either this parent isn't the root or else
				 * the root dir has not been rebuilt
				 */
				intermed_rc =
				    get_inorecptr(0, 0, this_parent,
						  &parent_inorecptr);
				if (intermed_rc != 0) {	/* it's fatal */
					dips_rc = intermed_rc;
				} else if ((parent_inorecptr->in_use)
					   && (!parent_inorecptr->
					       selected_to_rls)
					   && (!parent_inorecptr->
					       ignore_alloc_blks)) {
					/* got parent record and parent seems
					 * ok so far */
					intermed_rc =
					    get_path(inoidx, this_parent,
						     this_path_ptr_ptr,
						     ino_recptr);
					if (intermed_rc == FSCK_OK) {
						/* next path obtained */
						a_path_displayed = -1;
						dips_rc =
						    display_path(inoidx,
								 msg_info_ptr->
								 msg_inopfx,
								 this_parent,
								 this_path_ptr,
								 ino_recptr);
					} else {
						/* unable to obtain next path */
						fsck_send_msg
						    (fsck_INOCNTGETPATH,
						     fsck_ref_msg(msg_info_ptr->msg_inopfx),
						     this_parent,
						     fsck_ref_msg(msg_info_ptr->msg_inotyp),
						     fsck_ref_msg(msg_info_ptr->msg_inopfx),
						     inoidx);
						if (intermed_rc < 0) {
							dips_rc = intermed_rc;
						}
					}
				}
			}
		}
		this_ext = this_ext->next;

	}
	/*
	 * if nothing unexpected happened but we
	 * couldn't display a path, issue a message
	 * and go on.
	 */
      out:
	if ((dips_rc == FSCK_OK) && (!a_path_displayed)) {
		fsck_send_msg(fsck_INOCANTNAME, fsck_ref_msg(msg_info_ptr->msg_inotyp),
			      fsck_ref_msg(msg_info_ptr->msg_inopfx), inoidx);
	}

	return (dips_rc);
}

/*****************************************************************************
 * NAME: first_ref_check_inode
 *
 * FUNCTION:  Determine whether storage allocated to the given inode
 *            includes any multiply-allocated blocks for which the
 *            first reference is still unresolved.
 *
 * PARAMETERS:
 *      inoptr       - input - pointer to the current inode
 *      inoidx       - input - ordinal number of the inode as an integer
 *      inorec_ptr   - input - pointer to an fsck inode record describing the
 *                             current inode
 *      msginfo_ptr  - input - pointer to a record with information needed
 *                             to issue messages about the current inode
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int first_ref_check_inode(struct dinode *inoptr, uint32_t inoidx,
			  struct fsck_inode_record *inorec_ptr,
			  struct fsck_ino_msg_info *msginfo_ptr)
{
	int frcvi_rc = FSCK_OK;
	uint32_t block_count;
	int64_t first_fsblk;
	uint32_t num_fsblks;
	int8_t extent_is_valid;
	dxd_t *dxd_ptr;
	int is_EA;
	int is_ACL;

	/*
	 * check the extent (if any) containing the EA
	 */
	if (inoptr->di_ea.flag == DXD_EXTENT) {
		/* there is an ea to record */
		dxd_ptr = &(inoptr->di_ea);
		first_fsblk = addressDXD(dxd_ptr);
		num_fsblks = lengthDXD(dxd_ptr);
		is_EA = -1;
		is_ACL = 0;
		msginfo_ptr->msg_dxdtyp = fsck_EA;

		frcvi_rc =
		    process_extent(inorec_ptr, num_fsblks, first_fsblk, is_EA,
				   is_ACL, msginfo_ptr, &block_count,
				   &extent_is_valid, FSCK_QUERY);
	}

	/*
	 * check the extent (if any) containing the ACL
	 */
	if (inoptr->di_acl.flag == DXD_EXTENT) {
		/* there is an ACL to record */
		dxd_ptr = &(inoptr->di_acl);
		first_fsblk = addressDXD(dxd_ptr);
		num_fsblks = lengthDXD(dxd_ptr);

		is_EA = 0;
		is_ACL = -1;
		msginfo_ptr->msg_dxdtyp = fsck_ACL;

		frcvi_rc =
		    process_extent(inorec_ptr, num_fsblks, first_fsblk, is_EA,
				   is_ACL, msginfo_ptr, &block_count,
				   &extent_is_valid, FSCK_QUERY);
	}
	/*
	 * check the extents (if any) described as data
	 */
	if (frcvi_rc == FSCK_OK) {
		if (inorec_ptr->inode_type == directory_inode) {
			frcvi_rc =
			    process_valid_dir_data(inoptr, inoidx, inorec_ptr,
						   msginfo_ptr, FSCK_QUERY);
		} else if (ISREG(inoptr->di_mode) || ISLNK(inoptr->di_mode)) {
			frcvi_rc =
			    process_valid_data(inoptr, inoidx, inorec_ptr,
					       msginfo_ptr, FSCK_QUERY);
		}
	}

	return (frcvi_rc);
}

/*****************************************************************************
 * NAME: get_path
 *
 * FUNCTION:  Construct the unicode path from the root directory through
 *            the entry in the specified parent directory.
 *
 * PARAMETERS:
 *      inode_idx       - input - ordinal number of the inode as an integer
 *      parent_inonum   - input - the inode number for the (parent) directory
 *                                whose entry to the current inode is to be
 *                                described by the contents of inopath.
 *      addr_path_addr  - input - pointer to a variable in which to return
 *                                the address of the path (in UniChars)
 *      ino_recptr      - input - pointer to an fsck inode record describing
 *                                the current inode
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int get_path(uint32_t inode_idx, uint32_t parent_inonum, char **addr_path_addr,
	     struct fsck_inode_record *ino_recptr)
{
	int gip_rc = FSCK_OK;
	uint32_t inode_inonum;
	uint32_t this_parent_inonum;
	int bytes_used;
	int this_name_length = 0;
	int length_in_UniChars;
	UniChar uniname[256];
	char *path_ptr;
	int path_idx;
	int path_completed = 0;
	uint32_t parent_idx;
	struct fsck_inode_record *parent_inorecptr;
	int aggregate_inode = 0;
	int alloc_ifnull = 0;

	*addr_path_addr = NULL;

	bytes_used = sizeof (char);
	path_idx = JFS_PATH_MAX - 1;
	agg_recptr->path_buffer[path_idx] = '\0';
	path_ptr = &(agg_recptr->path_buffer[path_idx]);

	/* change the inode index to an inode number */
	inode_inonum = (uint32_t) inode_idx;
	/* the first parent must be given since the object may not be a
	 * directory */
	this_parent_inonum = parent_inonum;
	if (inode_idx == 2) {
		/* it's the root directory */
		path_completed = -1;
		path_idx--;
		path_ptr--;
		agg_recptr->path_buffer[path_idx] = agg_recptr->delim_char;
	}
	while ((!path_completed) && (gip_rc == FSCK_OK)) {

		gip_rc =
		    direntry_get_objnam(this_parent_inonum, inode_inonum,
					&length_in_UniChars, uniname);
		uniname[length_in_UniChars] = 0;

		if (gip_rc != FSCK_OK) {
			/* didn't get the name */
			path_completed = -1;
			if (path_idx != (JFS_PATH_MAX - 1)) {
				/* we got part of the path */
				/* remove the foreslash from the
				 * beginning of the path we have at this
				 * beginning as now assembled it implies
				 * that the unconnected dir parent is
				 * connected to the fileset root directory.
				 */
				path_ptr++;
			}
			break;
		}
		this_name_length =
		    Unicode_String_to_UTF8_String((uint8_t *) Str_Name, uniname,
						  256);
		Str_Name[this_name_length] = '\0';

		if ((bytes_used + this_name_length + sizeof (char)) >
		    JFS_PATH_MAX) {
			/* the path is beyond the legal length */
			path_completed = -1;
			if (path_idx == (JFS_PATH_MAX - 1)) {
				/* the very first segment
				 * is too long to be valid
				 */
				gip_rc = FSCK_FAILED_DIRENTRYBAD;
			} else {
				/* we got part of the path */
				/* remove the foreslash from the
				 * beginning of the path we have at
				 * this point since as now assembled
				 * it implies that the unconnected dir
				 * parent is connected to the fileset
				 * root directory.
				 */
				path_ptr++;
			}
			break;
		}

		bytes_used += this_name_length;
		path_idx -= this_name_length;
		path_ptr -= this_name_length;
		Str_Name_len = this_name_length;
		memcpy((void *) path_ptr, (void *) &Str_Name, Str_Name_len);
		bytes_used += sizeof (char);
		path_idx--;
		path_ptr--;
		agg_recptr->path_buffer[path_idx] = agg_recptr->delim_char;
		/*
		 * assume that we'll find a parent dir for the
		 * path segment just copied into the path buffer.
		 */
		if (this_parent_inonum == ROOT_I) {
			path_completed = -1;
			break;
		}

		/* haven't gotten up to root yet */
		parent_idx = (uint32_t) this_parent_inonum;
		inode_inonum = this_parent_inonum;
		gip_rc =
		    get_inorecptr(aggregate_inode, alloc_ifnull, parent_idx,
				  &parent_inorecptr);
		if ((gip_rc == FSCK_OK) && (parent_inorecptr == NULL))
			gip_rc = FSCK_INTERNAL_ERROR_21;
		if (gip_rc != FSCK_OK)
			break;

		this_parent_inonum = parent_inorecptr->parent_inonum;

		if ((this_parent_inonum == 0)
		    || (parent_inorecptr->selected_to_rls)
		    || (!(parent_inorecptr->in_use))
		    || (parent_inorecptr->inode_type == metadata_inode)) {
			path_completed = -1;
			/* remove the foreslash from the beginning
			 * of the path we have at this point since
			 * as now assembled it implies that the
			 * unconnected dir parent is connected to
			 * the fileset root directory.
			 */
			path_ptr++;
		}
	}

	if (gip_rc == FSCK_OK)
		/* indicate where to find the 1st char of the path just
		 * assembled */
		*addr_path_addr = path_ptr;

	return (gip_rc);
}

/*****************************************************************************
 * NAME: in_inode_data_check
 *
 * FUNCTION:  Verify that the fields in the current inode which describe
 *            inline data (that is, storage within the inode itself) do
 *            not overlap.
 *
 * PARAMETERS:
 *      msg_info_ptr  - input - pointer to a record with information needed
 *                              to issue messages about the current inode
 *
 * NOTES:  The data regarding inline data for the inode is stored in
 *         the global aggregate record, fields in the this_inode record.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int in_inode_data_check(struct fsck_inode_record *inorecptr,
			struct fsck_ino_msg_info *msg_info_ptr)
{
	int iidc_rc = FSCK_OK;
	int16_t size16;
	struct dinode an_inode;

	/*
	 * if in-inode data (or description of data) overflows
	 * this then the EA can NOT be inline
	 */
	size16 = sizeof (an_inode.di_inlinedata);
	if (agg_recptr->this_inode.in_inode_data_length > size16) {
		/* extra long inline data */
		if (agg_recptr->this_inode.ea_inline) {
			/* conflict */
			inorecptr->selected_to_rls = 1;
			inorecptr->inline_data_err = 1;
			inorecptr->clr_ea_fld = 1;
			agg_recptr->corrections_needed = 1;
			fsck_send_msg(fsck_INOINLINECONFLICT,
				      fsck_ref_msg(msg_info_ptr->msg_inotyp),
				      fsck_ref_msg(msg_info_ptr->msg_inopfx),
				      msg_info_ptr->msg_inonum,
				      fsck_longdata_and_otherinline);
		}
		if (agg_recptr->this_inode.acl_inline) {
			/* conflict */
			inorecptr->selected_to_rls = 1;
			inorecptr->inline_data_err = 1;
			inorecptr->clr_acl_fld = 1;
			agg_recptr->corrections_needed = 1;
			fsck_send_msg(fsck_INOINLINECONFLICT,
				      fsck_ref_msg(msg_info_ptr->msg_inotyp),
				      fsck_ref_msg(msg_info_ptr->msg_inopfx),
				      msg_info_ptr->msg_inonum,
				      fsck_longdata_and_otherinline);
		}
	} else {
		if (agg_recptr->this_inode.ea_inline
		    && agg_recptr->this_inode.acl_inline) {
			/* conflict */
			inorecptr->clr_ea_fld = 1;
			inorecptr->clr_acl_fld = 1;
			agg_recptr->corrections_needed = 1;
			fsck_send_msg(fsck_INOINLINECONFLICT,
				      fsck_ref_msg(msg_info_ptr->msg_inotyp),
				      fsck_ref_msg(msg_info_ptr->msg_inopfx),
				      msg_info_ptr->msg_inonum,
				      fsck_longdata_and_otherinline);
		}
	}

	return (iidc_rc);
}

/*****************************************************************************
 * NAME: inode_is_in_use
 *
 * FUNCTION:  Determine whether the specified inode is currently being
 *            used to represent a file system object.
 *
 * PARAMETERS:
 *      inode_ptr  - input - pointer to the current inode
 *      inode_num  - input - ordinal number of the inode in the internal
 *                           JFS format
 *
 * RETURNS:
 *      0:  if inode is not in use
 *      1:  if inode is in use
 */
int inode_is_in_use(struct dinode *inode_ptr, uint32_t inode_num)
{
	int iiiu_result;
	int ixpxd_unequal = 0;

	ixpxd_unequal =
	    memcmp((void *) &(inode_ptr->di_ixpxd),
		   (void *) &(agg_recptr->ino_ixpxd), sizeof (pxd_t));

	iiiu_result = ((inode_ptr->di_inostamp == agg_recptr->inode_stamp)
		       && (inode_ptr->di_number == inode_num)
		       && (inode_ptr->di_fileset == agg_recptr->ino_fsnum)
		       && (!ixpxd_unequal) && (inode_ptr->di_nlink != 0));

	return (iiiu_result);
}

/*****************************************************************************
 * NAME: parent_count
 *
 * FUNCTION: Count the number of directory entries fsck has observed which
 *           refer to the specified inode.
 *
 * PARAMETERS:
 *      this_inorec  - input - pointer to an fsck inode record describing the
 *                             current inode
 *
 * RETURNS:
 *      the number of parent directories observed for the inode
 */
int parent_count(struct fsck_inode_record *this_inorec)
{
	int pc_result = 0;
	struct fsck_inode_ext_record *this_ext;

	if (this_inorec->parent_inonum != 0) {
		pc_result++;
	}

	this_ext = this_inorec->ext_rec;
	while (this_ext != NULL) {
		/* extension records to check */
		if (this_ext->ext_type == parent_extension) {
			pc_result++;
		}
		this_ext = this_ext->next;
	}

	return (pc_result);
}

/*****************************************************************************
 * NAME: record_valid_inode
 *
 * FUNCTION:  Record, in the fsck workspace block map, all aggregate blocks
 *            allocated to the specified inode.  The inode structures have
 *            already been validated, no error checking is done.
 *
 * PARAMETERS:
 *      inoptr        - input - pointer to the current inode
 *      inoidx        - input - ordinal number of the inode as an integer
 *      inorecptr     - input - pointer to an fsck inode record describing the
 *                              current inode
 *      msg_info_ptr  - input - pointer to a record with information needed
 *                              to issue messages about the current inode
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int record_valid_inode(struct dinode *inoptr, uint32_t inoidx,
		       struct fsck_inode_record *inorecptr,
		       struct fsck_ino_msg_info *msg_info_ptr)
{
	int rvi_rc = FSCK_OK;
	int64_t first_fsblk, last_fsblk;
	uint32_t num_fsblks;
	dxd_t *dxd_ptr;

	/*
	 * record the extent (if any) containing the EA
	 */
	if (inoptr->di_ea.flag == DXD_EXTENT) {
		/* there is an ea to record */
		dxd_ptr = &(inoptr->di_ea);
		first_fsblk = addressDXD(dxd_ptr);
		num_fsblks = lengthDXD(dxd_ptr);
		last_fsblk = first_fsblk + num_fsblks - 1;
		extent_record(first_fsblk, last_fsblk);
		agg_recptr->this_inode.all_blks += num_fsblks;
		agg_recptr->blocks_this_fset += num_fsblks;
	}
	/*
	 * record the extent (if any) containing the ACL
	 */
	if (inoptr->di_acl.flag == DXD_EXTENT) {
		/* there is an acl to record */
		dxd_ptr = &(inoptr->di_acl);
		first_fsblk = addressDXD(dxd_ptr);
		num_fsblks = lengthDXD(dxd_ptr);
		last_fsblk = first_fsblk + num_fsblks - 1;
		extent_record(first_fsblk, last_fsblk);
		agg_recptr->this_inode.all_blks += num_fsblks;
		agg_recptr->blocks_this_fset += num_fsblks;
	}
	/*
	 * record the extents (if any) described as data
	 */
	process_valid_data(inoptr, inoidx, inorecptr, msg_info_ptr,
			   FSCK_RECORD);

	return (rvi_rc);
}

/*****************************************************************************
 * NAME: release_inode
 *
 * FUNCTION:  Release all aggregate blocks allocated to the specified inode.
 *            Reset the link count, in the inode on the device, to zero
 *            to make it available for reuse.
 *
 * PARAMETERS:
 *      inoidx      - input - ordinal number of the inode as an integer
 *      ino_recptr  - input - pointer to an fsck inode record describing the
 *                            current inode
 *      inoptr      - input - pointer to the current inode
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int release_inode(uint32_t inoidx, struct fsck_inode_record *ino_recptr,
		  struct dinode *inoptr)
{
	int ri_rc = FSCK_OK;
	struct dinode *this_inode;

	int aggregate_inode = 0;	/* going for fileset inodes only */
	int which_it = 0;	/* in release 1 there is only fileset 0 */
	struct fsck_ino_msg_info ino_msg_info;
	struct fsck_ino_msg_info *msg_info_ptr;

	msg_info_ptr = &ino_msg_info;
	msg_info_ptr->msg_inopfx = fsck_fset_inode;
	msg_info_ptr->msg_inonum = inoidx;
	if (ino_recptr->inode_type == directory_inode) {
		msg_info_ptr->msg_inotyp = fsck_directory;
	} else if (ino_recptr->inode_type == symlink_inode) {
		msg_info_ptr->msg_inotyp = fsck_symbolic_link;
	} else if (ino_recptr->inode_type == char_special_inode) {
		msg_info_ptr->msg_inotyp = fsck_char_special;
	} else if (ino_recptr->inode_type == block_special_inode) {
		msg_info_ptr->msg_inotyp = fsck_block_special;
	} else if (ino_recptr->inode_type == FIFO_inode) {
		msg_info_ptr->msg_inotyp = fsck_FIFO;
	} else if (ino_recptr->inode_type == SOCK_inode) {
		msg_info_ptr->msg_inotyp = fsck_SOCK;
	} else {
		/* a regular file */
		msg_info_ptr->msg_inotyp = fsck_file;
	}

	if (ino_recptr->in_use) {
		/* the inode is 'in use' */
		ri_rc =
		    inode_get(aggregate_inode, which_it, inoidx, &this_inode);

		if (ri_rc == FSCK_OK) {
			/* inode read successfully */
			this_inode->di_nlink = 0;
			ri_rc = inode_put(this_inode);

			if ((ri_rc == FSCK_OK)
			    && (!ino_recptr->ignore_alloc_blks)) {
				ri_rc =
				    unrecord_valid_inode(this_inode, inoidx,
							 ino_recptr,
							 msg_info_ptr);
			}
		}
	}
	return (ri_rc);
}

/*****************************************************************************
 * NAME: unrecord_valid_inode
 *
 * FUNCTION: Unrecord, in the fsck workspace block map, all aggregate blocks
 *           allocated to the specified inode.  The inode structures have
 *           already been validated, no error checking is done.
 *
 * PARAMETERS:
 *      inoptr        - input - pointer to the current inode
 *      inoidx        - input - ordinal number of the inode as an integer
 *      inorecptr     - input - pointer to an fsck inode record describing
 *                              the current inode
 *      msg_info_ptr  - input - pointer to a record with information needed
 *                              to issue messages about the current inode
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int unrecord_valid_inode(struct dinode *inoptr, uint32_t inoidx,
			 struct fsck_inode_record *inorecptr,
			 struct fsck_ino_msg_info *msg_info_ptr)
{
	int uvi_rc = FSCK_OK;
	int64_t ea_blocks = 0;
	int64_t acl_blocks = 0;

	/*
	 * unrecord the extent (if any) containing the EA
	 */
	if ((inoptr->di_ea.flag == DXD_EXTENT) && (!inorecptr->ignore_ea_blks)) {
		ea_blocks = lengthDXD(&(inoptr->di_ea));
		agg_recptr->blocks_for_eas -= ea_blocks;
		uvi_rc = backout_EA(inoptr, inorecptr);
	}

	/*
	 * unrecord the extent (if any) containing the ACL
	 */
	if ((inoptr->di_acl.flag == DXD_EXTENT)
	    && (!inorecptr->ignore_acl_blks)) {
		acl_blocks = lengthDXD(&(inoptr->di_acl));
		agg_recptr->blocks_for_acls -= acl_blocks;
		uvi_rc = backout_ACL(inoptr, inorecptr);
	}

	/*
	 * unrecord the extents (if any) describing data
	 *
	 * note that the tree is valid or we'd be ignoring these allocated
	 * blocks.
	 */
	if (uvi_rc == FSCK_OK) {
		if (inorecptr->inode_type == directory_inode) {
			agg_recptr->blocks_for_dirs -=
			    inoptr->di_nblocks - ea_blocks;
			uvi_rc =
			    process_valid_dir_data(inoptr, inoidx, inorecptr,
						   msg_info_ptr, FSCK_UNRECORD);
		} else {
			agg_recptr->blocks_for_files -=
			    inoptr->di_nblocks - ea_blocks;
			uvi_rc =
			    process_valid_data(inoptr, inoidx, inorecptr,
					       msg_info_ptr, FSCK_UNRECORD);
		}
	}

	return (uvi_rc);
}

/*****************************************************************************
 * NAME: validate_ACL
 *
 * FUNCTION: Determine whether the structures in the specified inode to
 *           describe ACL data owned by the inode are consistent and (as
 *           far as fsck can tell) correct.
 *
 * PARAMETERS:
 *      inoptr        - input - pointer to the current inode
 *      inoidx        - input - ordinal number of the inode as an integer
 *      inorecptr     - input - pointer to an fsck inode record describing
 *                              the current inode
 *      msg_info_ptr  - input - pointer to a record with information needed
 *                              to issue messages about the current inode
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int validate_ACL(struct dinode *inoptr, uint32_t inoidx,
		 struct fsck_inode_record *inorecptr,
		 struct fsck_ino_msg_info *msg_info_ptr)
{
	int vacl_rc = FSCK_OK;
	dxd_t *dxd_ptr;
	uint32_t recorded_length, shortest_valid, longest_valid;
	uint32_t ext_length;
	int64_t ext_address;
	int8_t extent_is_valid = 0;
	uint16_t size16;
	struct dinode an_inode;

	dxd_ptr = &(inoptr->di_acl);
	msg_info_ptr->msg_dxdtyp = fsck_ACL;

	if (dxd_ptr->flag == 0)
		goto out;

	/* there is an ACL for this inode */
	if ((dxd_ptr->flag != DXD_EXTENT) && (dxd_ptr->flag != DXD_INLINE)
	    && (dxd_ptr->flag != DXD_CORRUPT)) {
		/* not a single extent AND not inline AND not already
		 * reported */
		fsck_send_msg(fsck_BADINODXDFLDD,
			      fsck_ref_msg(msg_info_ptr->msg_inotyp),
			      fsck_ref_msg(msg_info_ptr->msg_inopfx),
			      msg_info_ptr->msg_inonum,
			      fsck_ref_msg(msg_info_ptr->msg_dxdtyp));
		inorecptr->clr_acl_fld = 1;
		inorecptr->ignore_acl_blks = 1;
		agg_recptr->corrections_needed = 1;

		goto out;
	}
	/* else the acl flag is ok */
	if (dxd_ptr->flag == DXD_INLINE) {
		/* ACL is inline  */
		size16 = sizeof (an_inode.di_inlineea);
		agg_recptr->this_inode.acl_inline = 1;
		agg_recptr->this_inode.inline_acl_length =
		    (uint16_t) dxd_ptr->size;
		agg_recptr->this_inode.inline_acl_offset =
		    (uint16_t) addressDXD(dxd_ptr);

		if ((dxd_ptr->size == 0)
		    || (dxd_ptr->size >
			(size16 - agg_recptr->this_inode.inline_acl_offset))) {
			/*
			 * the length extends
			 * beyond the end of the inode
			 */
			fsck_send_msg(fsck_BADINODXDFLDL,
				      fsck_ref_msg(msg_info_ptr->msg_inotyp),
				      fsck_ref_msg(msg_info_ptr->msg_inopfx),
				      msg_info_ptr->msg_inonum,
				      fsck_ref_msg(msg_info_ptr->msg_dxdtyp));

			inorecptr->clr_acl_fld = 1;
			agg_recptr->corrections_needed = 1;
		}
	} else if (dxd_ptr->flag == DXD_EXTENT) {
		/* else the ACL is a single extent */
		ext_length = lengthDXD(dxd_ptr);
		shortest_valid = (ext_length - 1) * sb_ptr->s_bsize + 1;
		longest_valid = ext_length * sb_ptr->s_bsize;
		if ((ext_length == 0) || (dxd_ptr->size < shortest_valid)
		    || (dxd_ptr->size > longest_valid)) {
			/* invalid length */
			fsck_send_msg(fsck_BADINODXDFLDL,
				      fsck_ref_msg(msg_info_ptr->msg_inotyp),
				      fsck_ref_msg(msg_info_ptr->msg_inopfx),
				      msg_info_ptr->msg_inonum,
				      fsck_ref_msg(msg_info_ptr->msg_dxdtyp));

			inorecptr->clr_acl_fld = 1;
			inorecptr->ignore_acl_blks = 1;
			agg_recptr->corrections_needed = 1;
			recorded_length = 0;
			extent_is_valid = 0;
		} else {
			/* length and size might be ok */
			agg_recptr->this_inode.acl_blks = ext_length;
			ext_address = addressDXD(dxd_ptr);
			vacl_rc =
			    process_extent(inorecptr, ext_length, ext_address,
					   0, -1, msg_info_ptr,
					   &recorded_length, &extent_is_valid,
					   FSCK_RECORD_DUPCHECK);
			/*
			 * add the blocks in the ACL extent to the running
			 * totals for the fileset and inode, but not for
			 * the object represented by the object.
			 */
			agg_recptr->blocks_this_fset += recorded_length;
			agg_recptr->this_inode.all_blks += recorded_length;
		}

		if (!extent_is_valid) {
			inorecptr->clr_acl_fld = 1;
			agg_recptr->corrections_needed = 1;
		}
	}
      out:
	return (vacl_rc);
}

/*****************************************************************************
 * NAME: validate_data
 *
 * FUNCTION: Determine whether the structures in, or rooted in, the specified
 *           non-directory inode to describe data owned by the inode are
 *           consistent and (as far as fsck can tell) correct.
 *
 * PARAMETERS:
 *      inoptr        - input - pointer to the current inode
 *      inoidx        - input - ordinal number of the inode as an integer
 *      inorecptr     - input - pointer to an fsck inode record describing
 *                              the current inode
 *      msg_info_ptr  - input - pointer to a record with information needed
 *                              to issue messages about the current inode
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int validate_data(struct dinode *inoptr, uint32_t inoidx,
		  struct fsck_inode_record *inorecptr,
		  struct fsck_ino_msg_info *msg_info_ptr)
{
	int vd_rc = FSCK_OK;
	int intermed_rc = FSCK_OK;
	dxd_t *dxd_ptr;
	xtpage_t *xtp_ptr;

	dxd_ptr = &(inoptr->di_dxd);
	/* the data root dxd */
	msg_info_ptr->msg_dxdtyp = fsck_objcontents;

	/*
	 * Symbolic link is a special case.  If the value fits within the
	 * inode, the dxd appears to be an empty xtree, but it's unused.
	 * In this case, ignore the dxd.
	 */
	if (ISLNK(inoptr->di_mode) && (inoptr->di_size < IDATASIZE)) {
		/* Null terminator stored but not accounted for in di_size */
		agg_recptr->this_inode.in_inode_data_length =
			inoptr->di_size + 1;
		goto out;
	}

	/*
	 * examine the data field
	 */
	if (dxd_ptr->flag == 0)
		goto out;

	if ((dxd_ptr->flag == (DXD_INDEX | BT_ROOT | BT_LEAF))
	    || (dxd_ptr->flag == (DXD_INDEX | BT_ROOT | BT_INTERNAL))) {
		/*
		 * to be valid, it has to be a B-tree node,
		 * either root-leaf or root-internal
		 */
		/*
		 * figure out how much space the root occupies in the inode
		 * itself
		 */
		xtp_ptr = (xtpage_t *) (&inoptr->di_btroot);
		agg_recptr->this_inode.in_inode_data_length =
		    (xtp_ptr->header.maxentry - 2) * sizeof (xad_t);
		/*
		 * the dxd actually starts 32 bytes (== 2 * length of
		 * an xad) before the boundary.
		 * the 0th and 1st entries in the xad array are
		 * really the header
		 */

		/*
		 * validate the tree contents and record the extents it
		 * describes until and unless the tree is found to be corrupt
		 */
		vd_rc =
		    xTree_processing(inoptr, inoidx, inorecptr, msg_info_ptr,
				     FSCK_RECORD_DUPCHECK);

		if (vd_rc <= FSCK_OK)
			goto out;

		/* nothing fatal */
		if (inorecptr->selected_to_rls &&
		    inode_is_metadata(inorecptr)) {
			vd_rc = FSCK_BADMDDATAIDX;
		} else if (inorecptr->ignore_alloc_blks) {
			/* the tree info can't be used */
			if (inode_is_metadata(inorecptr)) {
				vd_rc = FSCK_BADMDDATAIDX;
			}
			/*
			 * reverse the notations made when recording the
			 * extents for the tree.  Again, stop when the point
			 * of corruption is found since that's where the
			 * recording process was stopped.
			 */
			intermed_rc =
			    xTree_processing(inoptr, inoidx, inorecptr,
					     msg_info_ptr, FSCK_UNRECORD);
			if (intermed_rc < 0) {
				vd_rc = intermed_rc;
				goto out;
			}
			if (intermed_rc != FSCK_OK) {
				if (vd_rc == FSCK_OK) {
					vd_rc = intermed_rc;
				}
			}
			if (!inorecptr->ignore_ea_blks) {
				intermed_rc = backout_EA(inoptr, inorecptr);
				if (intermed_rc < 0) {
					vd_rc = intermed_rc;
					goto out;
				}
				if (intermed_rc != FSCK_OK) {
					if (vd_rc == FSCK_OK) {
						vd_rc = intermed_rc;
					}
				}
				if (!inorecptr->ignore_acl_blks) {
					intermed_rc =
					    backout_ACL(inoptr, inorecptr);
					if (intermed_rc < 0) {
						vd_rc = intermed_rc;
						goto out;
					}
					if (intermed_rc != FSCK_OK) {
						if (vd_rc == FSCK_OK) {
							vd_rc = intermed_rc;
						}
					}
				}
			}
		}
	} else {
		/* else not B+ Tree index */
		/*
		 * the data root is not valid...the info cannot be trusted
		 */
		if (inode_is_metadata(inorecptr)) {
			vd_rc = FSCK_BADMDDATA;
		} else {
			inorecptr->selected_to_rls = 1;
			inorecptr->ignore_alloc_blks = 1;
			agg_recptr->corrections_needed = 1;
		}
		fsck_send_msg(fsck_BADINODXDFLDD,
			      fsck_ref_msg(msg_info_ptr->msg_inotyp),
			      fsck_ref_msg(msg_info_ptr->msg_inopfx),
			      msg_info_ptr->msg_inonum,
			      fsck_ref_msg(msg_info_ptr->msg_dxdtyp));
	}
      out:
	return (vd_rc);
}

/*****************************************************************************
 * NAME: validate_dir_data
 *
 * FUNCTION: Determine whether the structures in, or rooted in, the
 *           specified directory inode to describe data owned by the
 *           inode are consistent and (as far as fsck can tell)
 *           correct.
 *
 * PARAMETERS:
 *      inoptr        - input - pointer to the current inode
 *      inoidx        - input - ordinal number of the inode as an integer
 *      inorecptr     - input - pointer to an fsck inode record describing the
 *                              current inode
 *      msg_info_ptr  - input - pointer to a record with information needed
 *                              to issue messages about the current inode
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int validate_dir_data(struct dinode *inoptr, uint32_t inoidx,
		      struct fsck_inode_record *inorecptr,
		      struct fsck_ino_msg_info *msg_info_ptr)
{
	int vdd_rc = FSCK_OK;
	int intermed_rc = FSCK_OK;
	int dtree_rc = FSCK_OK;
	dxd_t *dxd_ptr;
	int8_t save_ignore_alloc_blks;
	int8_t xt_ignore_alloc_blks = 0;

	dxd_ptr = &(inoptr->di_dxd);
	/* the data root dxd */
	msg_info_ptr->msg_dxdtyp = fsck_objcontents;

	/*
	 * examine the data field
	 */
	if (dxd_ptr->flag == 0)
		goto out;

	/* Work around bug that used to clear the DXD_INDEX bit */
	dxd_ptr->flag |= DXD_INDEX;

	/*
	 * to be valid, it has to be a B-tree node,
	 * either root-leaf or root-internal
	 */
	if ((dxd_ptr->flag != (DXD_INDEX | BT_ROOT | BT_LEAF))
	    && (dxd_ptr->flag != (DXD_INDEX | BT_ROOT | BT_INTERNAL))) {
		/*
		 * the data root is not valid...the info cannot be trusted
		 */
		inorecptr->selected_to_rls = 1;
		inorecptr->ignore_alloc_blks = 1;
		agg_recptr->corrections_needed = 1;

		fsck_send_msg(fsck_BADINODXDFLDD,
			      fsck_ref_msg(msg_info_ptr->msg_inotyp),
			      fsck_ref_msg(msg_info_ptr->msg_inopfx),
			      msg_info_ptr->msg_inonum,
			      fsck_ref_msg(msg_info_ptr->msg_dxdtyp));

		goto out;
	}

	/*
	 * figure out how much space the root occupies in the inode itself
	 */
	agg_recptr->this_inode.in_inode_data_length =
	    (DTROOTMAXSLOT - 1) * sizeof (struct dtslot);
	/*
	 * The root actually starts 32 bytes (aka the length
	 * of 1 slot) before the boundary.
	 * the 1st slot is really the header
	 */
	/*
	 * process the xtree if the new directory table is supported
	 * and if the directory table data is not inline (index > 14)
	 */
	if (sb_ptr->s_flag & JFS_DIR_INDEX)
		inorecptr->check_dir_index = 1;

	if ((sb_ptr->s_flag & JFS_DIR_INDEX) &&
	    inoptr->di_next_index > MAX_INLINE_DIRTABLE_ENTRY + 1) {
		/*
		 * validate the tree contents and record the extents it
		 * describes until and unless the tree is found to be corrupt
		 */
		vdd_rc =
		    xTree_processing(inoptr, inoidx, inorecptr, msg_info_ptr,
				     FSCK_RECORD_DUPCHECK);

		if (vdd_rc < FSCK_OK)
			goto out;

	}

	/*
	 * Check an error bit to see if there was something
	 * wrong but non-fatal with xTree_processing.  If
	 * so, reset the bit to give dTree_processing a
	 * clean slate to start with.  Set a flag to
	 * indicate that the directory index table is to be rebuilt.
	 */
	if (inorecptr->ignore_alloc_blks) {
		inorecptr->check_dir_index = 0;
		inorecptr->rebuild_dirtable = 1;
		inorecptr->ignore_alloc_blks = 0;
		inorecptr->selected_to_rls = 0;
		agg_recptr->corrections_needed = 1;
		xt_ignore_alloc_blks = 1;
	}
	/*
	 * validate the tree contents and record the extents it
	 * describes until and unless the tree is found to be corrupt
	 */
	dtree_rc =
	    dTree_processing(inoptr, inoidx, inorecptr, msg_info_ptr,
			     FSCK_RECORD_DUPCHECK);

	if (dtree_rc < FSCK_OK)
		goto dtree_out;

	if (inorecptr->ignore_alloc_blks) {
		/* the tree info can't be used */
		/*
		 * reverse the notations made when recording the extents
		 * for the tree.  Again, stop when the point of corruption
		 * is found since that's where the recording process was
		 * stopped.
		 */
		intermed_rc =
		    dTree_processing(inoptr, inoidx, inorecptr, msg_info_ptr,
				     FSCK_UNRECORD);
		if (intermed_rc < 0) {
			dtree_rc = intermed_rc;
			goto dtree_out;
		}
		if (intermed_rc != FSCK_OK) {
			if (dtree_rc == FSCK_OK) {
				dtree_rc = intermed_rc;
			}
		}
		if (!inorecptr->ignore_ea_blks) {
			intermed_rc = backout_EA(inoptr, inorecptr);
			if (intermed_rc < 0) {
				dtree_rc = intermed_rc;
				goto dtree_out;
			}
			if (intermed_rc != FSCK_OK) {
				if (dtree_rc == FSCK_OK) {
					dtree_rc = intermed_rc;
				}
			}
			if (!inorecptr->ignore_acl_blks) {
				intermed_rc = backout_ACL(inoptr, inorecptr);
				if (intermed_rc < 0) {
					dtree_rc = intermed_rc;
					goto dtree_out;
				}
				if (intermed_rc != FSCK_OK) {
					if (dtree_rc == FSCK_OK) {
						dtree_rc = intermed_rc;
					}
				}
			}
		}
	}
	if ((sb_ptr->s_flag & JFS_DIR_INDEX) &&
	    (inoptr->di_next_index > MAX_INLINE_DIRTABLE_ENTRY + 1) &&
	    (inorecptr->ignore_alloc_blks || inorecptr->rebuild_dirtable)) {

		/*
		 * ignore_alloc_blks needs to exactly reflect the results
		 * of the xTree_processing step
		 */
		save_ignore_alloc_blks = inorecptr->ignore_alloc_blks;
		inorecptr->ignore_alloc_blks = xt_ignore_alloc_blks;
		/*
		 * reverse the notations made when recording the
		 * extents for the tree.  Again, stop when the point
		 * of corruption is found since that's where the
		 * recording process was stopped.
		 */
		intermed_rc =
		    xTree_processing(inoptr, inoidx, inorecptr,
				     msg_info_ptr, FSCK_UNRECORD);
		if (intermed_rc < 0) {
			vdd_rc = intermed_rc;
			goto out;
		}
		if (intermed_rc != FSCK_OK) {
			if (vdd_rc == FSCK_OK) {
				vdd_rc = intermed_rc;
			}
		}
		inorecptr->ignore_alloc_blks = save_ignore_alloc_blks;

		/*
		 * If we don't fix di_nblocks now, the inode will be
		 * thrown away (even if dtree is good).
		 */
		inoptr->di_nblocks = agg_recptr->this_inode.all_blks;
	}
      dtree_out:
	/*
	 * return the worse of the rc's for xtree and dtree processing
	 */
	if (dtree_rc != FSCK_OK) {
		vdd_rc = dtree_rc;
	}

      out:
	return (vdd_rc);
}

/*****************************************************************************
 * NAME: validate_EA
 *
 * FUNCTION: Determine whether the structures in the specified inode to
 *           describe ea data owned by the inode are consistent and (as
 *           far as fsck can tell) correct.
 *
 * PARAMETERS:
 *      inoptr        - input - pointer to the current inode
 *      inoidx        - input - ordinal number of the inode as an integer
 *      inorecptr     - input - pointer to an fsck inode record describing
 *                              the current inode
 *      msg_info_ptr  - input - pointer to a record with information needed
 *                              to issue messages about the current inode
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int validate_EA(struct dinode *inoptr, uint32_t inoidx,
		struct fsck_inode_record *inorecptr,
		struct fsck_ino_msg_info *msg_info_ptr)
{
	int vea_rc = FSCK_OK;
	int intermed_rc = FSCK_OK;
	dxd_t *dxd_ptr;
	uint32_t recorded_length, shortest_valid, longest_valid;
	uint32_t ext_length;
	int64_t ext_byte_length;
	int64_t ext_address = 0;
	int8_t extent_is_valid = 0;
	int8_t ea_format_bad = 0;
	uint16_t size16;
	struct dinode an_inode;
	unsigned long eafmt_error = 0;

	dxd_ptr = &(inoptr->di_ea);
	msg_info_ptr->msg_dxdtyp = fsck_EA;

	if (dxd_ptr->flag == 0)
		goto out;

	/* there is an EA for this inode */
	if ((dxd_ptr->flag != DXD_EXTENT) && (dxd_ptr->flag != DXD_INLINE)) {
		/* not a single extent AND not inline */
		fsck_send_msg(fsck_BADINODXDFLDD,
			      fsck_ref_msg(msg_info_ptr->msg_inotyp),
			      fsck_ref_msg(msg_info_ptr->msg_inopfx),
			      msg_info_ptr->msg_inonum,
			      fsck_ref_msg(msg_info_ptr->msg_dxdtyp));
		inorecptr->clr_ea_fld = 1;
		inorecptr->ignore_ea_blks = 1;
		agg_recptr->corrections_needed = 1;

		goto out;
	}

	if (dxd_ptr->flag == DXD_INLINE) {
		/* EA is inline  */
		size16 = sizeof (an_inode.di_inlineea);
		agg_recptr->this_inode.ea_inline = 1;
		agg_recptr->this_inode.inline_ea_length = dxd_ptr->size;
		agg_recptr->this_inode.inline_ea_offset =
		    (uint16_t) addressDXD(dxd_ptr);

		if ((dxd_ptr->size == 0)
		    || (dxd_ptr->size >
			(size16 - agg_recptr->this_inode.inline_ea_offset))) {
			/*
			 * the length extends
			 * beyond the end of the inode
			 */
			fsck_send_msg(fsck_BADINODXDFLDL,
				      fsck_ref_msg(msg_info_ptr->msg_inotyp),
				      fsck_ref_msg(msg_info_ptr->msg_inopfx),
				      msg_info_ptr->msg_inonum,
				      fsck_ref_msg(msg_info_ptr->msg_dxdtyp));

			inorecptr->clr_ea_fld = 1;
			agg_recptr->corrections_needed = 1;
		} else {
			/* the inline ea has a valid length.
			 * verify its format. */
			vea_rc =
			    jfs_ValidateFEAList((struct FEALIST *)
						&(inoptr->di_inlineea),
						dxd_ptr->size, &eafmt_error);
			if ((vea_rc != FSCK_OK) || (eafmt_error != 0)) {
				/* ea format is bad */
				ea_format_bad = -1;
			}
		}
	} else {
		/* else the EA is a single extent */
		ext_length = lengthDXD(dxd_ptr);
		shortest_valid = (ext_length - 1) * sb_ptr->s_bsize + 1;
		longest_valid = ext_length * sb_ptr->s_bsize;
		if ((ext_length == 0) || (dxd_ptr->size < shortest_valid)
		    || (dxd_ptr->size > longest_valid)) {
			/* invalid length */
			fsck_send_msg(fsck_BADINODXDFLDL,
				      fsck_ref_msg(msg_info_ptr->msg_inotyp),
				      fsck_ref_msg(msg_info_ptr->msg_inopfx),
				      msg_info_ptr->msg_inonum,
				      fsck_ref_msg(msg_info_ptr->msg_dxdtyp));

			extent_is_valid = 0;
			inorecptr->ignore_ea_blks = 1;
		} else {
			/* length and size might be ok */
			agg_recptr->this_inode.ea_blks = ext_length;
			ext_address = addressDXD(dxd_ptr);
			vea_rc =
			    process_extent(inorecptr, ext_length, ext_address,
					   -1, 0, msg_info_ptr,
					   &recorded_length, &extent_is_valid,
					   FSCK_RECORD_DUPCHECK);
			/*
			 * add the blocks in the EA extent to the running
			 * totals for the filese and inode, but not for the
			 * object represented by the object.
			 */
			agg_recptr->blocks_this_fset += recorded_length;
			agg_recptr->this_inode.all_blks += recorded_length;
		}

		if (!extent_is_valid) {
			inorecptr->clr_ea_fld = 1;
			agg_recptr->corrections_needed = 1;
			goto out;
		}
		/* the extent looks ok so need to check ea data structure */
		ext_byte_length = ext_length * sb_ptr->s_bsize;

		if (ext_byte_length > agg_recptr->ea_buf_length) {
			/* extra large ea  -- can't check it */
			inorecptr->cant_chkea = 1;
			agg_recptr->warning_pending = 1;
			goto out;
		}
		/* regular size ea */
		intermed_rc =
		    ea_get(ext_address, ext_byte_length, agg_recptr->ea_buf_ptr,
			   &(agg_recptr->ea_buf_length),
			   &(agg_recptr->ea_buf_data_len),
			   &(agg_recptr->ea_agg_offset));
		if (intermed_rc != FSCK_OK) {
			fsck_send_msg(fsck_BADINODXDFLDO,
				      fsck_ref_msg(msg_info_ptr->msg_inotyp),
				      fsck_ref_msg(msg_info_ptr->msg_inopfx),
				      msg_info_ptr->msg_inonum,
				      fsck_ref_msg(msg_info_ptr->msg_dxdtyp));

			inorecptr->clr_ea_fld = 1;
			agg_recptr->corrections_needed = 1;

			goto out;
		}
		/* the ea has been read into the regular buffer */
		vea_rc = jfs_ValidateFEAList((struct FEALIST *)
					     agg_recptr->ea_buf_ptr,
					     dxd_ptr->size, &eafmt_error);
		if ((vea_rc != FSCK_OK) || (eafmt_error != 0)) {
			/* ea format is bad */
			ea_format_bad = -1;
		}
	}
      out:
	if (ea_format_bad) {
		/* bad ea but haven't notified anyone */
		fsck_send_msg(fsck_EAFORMATBAD,
			      fsck_ref_msg(msg_info_ptr->msg_inotyp),
			      fsck_ref_msg(msg_info_ptr->msg_inopfx),
			      msg_info_ptr->msg_inonum);
		inorecptr->clr_ea_fld = 1;
		agg_recptr->corrections_needed = 1;
		vea_rc = FSCK_OK;
	}
	return (vea_rc);
}

/*****************************************************************************
 * NAME: validate_record_fileset_inode
 *
 * FUNCTION:  Determine whether structures in and/or rooted in the specified
 *            fileset owned inode are consistent and (as far as fsck can tell)
 *            correct.  Record, in the fsck workspace block map, all storage
 *            allocated to the inode.
 *
 * PARAMETERS:
 *      inonum            - input - ordinal number of the inode in the
 *                                  internal JFS format
 *      inoidx            - input - ordinal number of the inode as an integer
 *      inoptr            - input - pointer to the current inode
 *      ino_msg_info_ptr  - input - pointer to a record with information needed
 *                                  to issue messages about the current inode
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int validate_record_fileset_inode(uint32_t inonum, uint32_t inoidx,
				  struct dinode *inoptr,
				  struct fsck_ino_msg_info
				  *ino_msg_info_ptr)
{
	int vrfi_rc = FSCK_OK;
	int intermed_rc = FSCK_OK;
	struct fsck_inode_record *inorecptr;
	int8_t bad_size = 0;
	int aggregate_inode = 0;
	int alloc_ifnull = -1;
	int64_t min_size, max_size;
	int16_t size16;
	int8_t dinode_sect4_avail = 0;
	struct dinode an_inode;

	ino_msg_info_ptr->msg_inonum = inonum;

	vrfi_rc =
	    get_inorecptr(aggregate_inode, alloc_ifnull, inoidx, &inorecptr);

	if (vrfi_rc != FSCK_OK)
		goto out;

	inorecptr->in_use = 1;

	if (!(inode_type_recognized(inoptr))) {
		/* bad type */
		inorecptr->inode_type = unrecognized_inode;
		ino_msg_info_ptr->msg_inotyp = fsck_file;
		inorecptr->selected_to_rls = 1;
		inorecptr->ignore_alloc_blks = 1;
		/* no matter what the user
		 * approves or disapproves, we aren't
		 * going to even look to see which
		 * blocks are allocated to this inode
		 * (except for the blocks it occupies
		 * itself)
		 */
		agg_recptr->corrections_needed = 1;
		fsck_send_msg(fsck_BADINOTYP,
			      fsck_ref_msg(ino_msg_info_ptr->msg_inopfx),
			      ino_msg_info_ptr->msg_inonum);

		goto out;
	}
	/* else type is recognized as valid */
	/*
	 * clear the workspace area for the current inode
	 */
	memset((void *) (&(agg_recptr->this_inode)), '\0',
	       sizeof (agg_recptr->this_inode));
	memcpy((void *) &(agg_recptr->this_inode.eyecatcher),
	       (void *) "thisinod", 8);

	/*
	 * finish filling in the inode's workspace record
	 */
	if (ISDIR(inoptr->di_mode)) {
		inorecptr->inode_type = directory_inode;
		ino_msg_info_ptr->msg_inotyp = fsck_directory;
	} else if (ISLNK(inoptr->di_mode)) {
		inorecptr->inode_type = symlink_inode;
		ino_msg_info_ptr->msg_inotyp = fsck_symbolic_link;
	} else if (ISBLK(inoptr->di_mode)) {
		inorecptr->inode_type = block_special_inode;
		ino_msg_info_ptr->msg_inotyp = fsck_block_special;
	} else if (ISCHR(inoptr->di_mode)) {
		inorecptr->inode_type = char_special_inode;
		ino_msg_info_ptr->msg_inotyp = fsck_char_special;
	} else if (ISFIFO(inoptr->di_mode)) {
		inorecptr->inode_type = FIFO_inode;
		ino_msg_info_ptr->msg_inotyp = fsck_FIFO;
	} else if (ISSOCK(inoptr->di_mode)) {
		inorecptr->inode_type = SOCK_inode;
		ino_msg_info_ptr->msg_inotyp = fsck_SOCK;
	} else {
		/* a regular file */
		inorecptr->inode_type = file_inode;
		ino_msg_info_ptr->msg_inotyp = fsck_file;
	}

	inorecptr->link_count -= inoptr->di_nlink;

	/*
	 * validate the inode's structures
	 */
	/* validate the Extended Attributes if any */
	vrfi_rc = validate_EA(inoptr, inoidx, inorecptr, ino_msg_info_ptr);
	if (vrfi_rc != FSCK_OK)
		goto out;

	/* validate the Access Control List if any */
	vrfi_rc = validate_ACL(inoptr, inoidx, inorecptr, ino_msg_info_ptr);
	if (vrfi_rc != FSCK_OK)
		goto out;

	/* nothing fatal with the EA or ACL */
	if (inorecptr->inode_type == directory_inode) {
		/*
		 * validate the data, if any,  whether inline,
		 * a single extent, or a B+ Tree
		 */
		vrfi_rc = validate_dir_data(inoptr, inoidx, inorecptr,
					    ino_msg_info_ptr);
	} else if (ISREG(inoptr->di_mode) || ISLNK(inoptr->di_mode)) {
		/*
		 * validate the data, if any,  whether
		 * inline, a single extent, or a B+ Tree
		 */
		vrfi_rc =
		    validate_data(inoptr, inoidx, inorecptr, ino_msg_info_ptr);
	}
	if (vrfi_rc != FSCK_OK)
		goto out;

	if (inorecptr->ignore_alloc_blks) {
		inorecptr->selected_to_rls = 1;
		agg_recptr->corrections_needed = 1;
	}

	if (!inorecptr->selected_to_rls) {
		/* not selected to release yet */
		if (inoptr->di_nblocks != agg_recptr->this_inode.all_blks) {
			/*
			 * number of blocks is wrong.  tree must be bad
			 */
#ifdef _JFS_DEBUG
			printf
			    ("inode: %ld (t)   di_nblocks = %lld (t)   "
			     "this_inode.all_blks = %lld (t)\n",
			     inonum, inoptr->di_nblocks,
			     agg_recptr->this_inode.all_blks);
#endif

			fsck_send_msg(fsck_BADKEYS,
				      fsck_ref_msg(ino_msg_info_ptr->msg_inotyp),
				      fsck_ref_msg(ino_msg_info_ptr->msg_inopfx),
				      ino_msg_info_ptr->msg_inonum,
				      9);

			inorecptr->selected_to_rls = 1;
			inorecptr->ignore_alloc_blks = 1;
			agg_recptr->corrections_needed = 1;
			bad_size = -1;
		} else {
			/*
			 * the data size (in bytes) must not exceed the total
			 * size of the blocks allocated for it and must use at
			 * least 1 byte in the last fsblock allocated for it.
			 */
			if (agg_recptr->this_inode.data_size == 0) {
				if (inorecptr->inode_type == directory_inode) {
					min_size = IDATASIZE;
					max_size = IDATASIZE;
				} else {
					/* not a directory */
					min_size = 0;
					max_size = IDATASIZE;
				}
			} else {
				/* blocks are allocated to data */
				min_size =
				    agg_recptr->this_inode.data_size -
				    sb_ptr->s_bsize + 1;
				max_size = agg_recptr->this_inode.data_size;
			}
			/* Don't worry about directory size yet */
			if (!ISDIR(inoptr->di_mode)
			    && ((inoptr->di_size < min_size)
				|| (!(inoptr->di_mode & ISPARSE)
				    && (inoptr->di_size > max_size)))) {
				/* if size is less than min, or if object is
				 * not sparse and size is greater than max,
				 * then object size (in bytes) is is wrong
				 * - tree must be bad.
				 */
#ifdef _JFS_DEBUG
				printf
				    ("inode: %ld (t)   min_size = %lld (t)   "
				     "max_size = %lld (t)  di_size = %lld (t)\n",
				     inonum, min_size, max_size,
				     inoptr->di_size);
#endif
				fsck_send_msg(fsck_BADKEYS,
					      fsck_ref_msg(ino_msg_info_ptr->msg_inotyp),
					      fsck_ref_msg(ino_msg_info_ptr->msg_inopfx),
					      ino_msg_info_ptr->msg_inonum,
					      10);

				inorecptr->selected_to_rls = 1;
				inorecptr->ignore_alloc_blks = 1;
				agg_recptr->corrections_needed = 1;
				bad_size = -1;
			}
		}
	}
	if (!(inorecptr->ignore_alloc_blks)) {
		/* the tree looks ok */
		intermed_rc = in_inode_data_check(inorecptr, ino_msg_info_ptr);
		if (inorecptr->selected_to_rls) {
			fsck_send_msg(fsck_BADKEYS,
				      fsck_ref_msg(ino_msg_info_ptr->msg_inotyp),
				      fsck_ref_msg(ino_msg_info_ptr->msg_inopfx),
				      ino_msg_info_ptr->msg_inonum,
				      39);

			goto out;
		}
		/* not selected to release */
		/* in_inode_data_check came out ok.
		 * now check to be sure the mode bit INLINEEA is set properly
		 *
		 * N.B. if not, we'll correct the mode bit.  We won't
		 *       release the inode for this.
		 */
		dinode_sect4_avail = 0;
		/* if in-inode data (or
		 * description of data) overflows this then
		 * section 4 of the disk inode is NOT available
		 */
		size16 = sizeof (an_inode.di_inlinedata);
		if (agg_recptr->this_inode.in_inode_data_length > size16) {
			/* extra long inline data */
			if ((inoptr->di_mode & INLINEEA) == INLINEEA) {
				inorecptr->inlineea_off = 1;
			}
		} else {
			/* not extra long inline data */
			if ((!(agg_recptr->this_inode.ea_inline)
			     || (inorecptr->clr_ea_fld))
			    && (!(agg_recptr->this_inode.acl_inline)
				|| (inorecptr->clr_acl_fld))) {
				/*
				 * if (either ea isn't inline OR ea being
				 * cleared) AND (either acl isn't inline OR acl
				 * being cleared)
				 */
				dinode_sect4_avail = -1;
			}
			/*
			 * if we know section 4 is (or will be) available but
			 * the flag is off, then flag it to turn the flag on.
			 */
			if ((dinode_sect4_avail)
			    && ((inoptr->di_mode & INLINEEA) != INLINEEA)) {
				inorecptr->inlineea_on = 1;
				agg_recptr->corrections_needed = 1;
			} else if ((!dinode_sect4_avail)
				   && ((inoptr->di_mode & INLINEEA) ==
				       INLINEEA)) {
				/*
				 * if we know section 4 is (or will be)
				 * unavailable but the flag is on, then flag it
				 * to turn the flag off.
				 */
				inorecptr->inlineea_off = 1;
				agg_recptr->corrections_needed = 1;
			}
		}
	} else {
		/* the tree is not valid */
		/*
		 * If bad_size is set then we didn't know that
		 * the tree was bad until we looked at the size
		 * fields.  This means that the block usage recorded
		 * for this inode has not been backed out yet.
		 */
		if (bad_size) {
			/* tree is bad by implication */
			if (!inorecptr->ignore_ea_blks) {
				/* remove traces, in
				 * the fsck workspace maps, of the blocks
				 * allocated to this inode
				 */
				backout_EA(inoptr, inorecptr);
			}
			if (!inorecptr->ignore_acl_blks) {
				/* remove traces, in
				 * the fsck workspace maps, of the blocks
				 * allocated to this inode
				 */
				backout_ACL(inoptr, inorecptr);
			}
			if (inorecptr->inode_type == directory_inode) {
				/*
				 * remove traces, in the fsck workspace
				 * maps, of the blocks allocated to data
				 * for this inode, whether a single
				 * extent or a B+ Tree
				 */
				process_valid_dir_data(inoptr, inoidx,
						       inorecptr,
						       ino_msg_info_ptr,
						       FSCK_UNRECORD);
			} else {
				/*
				 * remove traces, in the fsck workspace
				 * maps, of the blocks allocated to data
				 * for this inode, whether a single
				 * extent or a B+ Tree
				 */
				process_valid_data(inoptr, inoidx, inorecptr,
						   ino_msg_info_ptr,
						   FSCK_UNRECORD);
			}
		}
	}
	if ((vrfi_rc == FSCK_OK) && (!inorecptr->selected_to_rls)) {
		/* looks like a keeper */
		agg_recptr->blocks_for_eas += agg_recptr->this_inode.ea_blks;
		agg_recptr->blocks_for_acls += agg_recptr->this_inode.acl_blks;
		if (inorecptr->inode_type == directory_inode) {
			/* a directory */
			agg_recptr->blocks_for_dirs =
			    agg_recptr->blocks_for_dirs +
			    agg_recptr->this_inode.all_blks -
			    agg_recptr->this_inode.ea_blks -
			    agg_recptr->this_inode.acl_blks;
		} else {
			/* a file */
			agg_recptr->blocks_for_files =
			    agg_recptr->blocks_for_files +
			    agg_recptr->this_inode.all_blks -
			    agg_recptr->this_inode.ea_blks -
			    agg_recptr->this_inode.acl_blks;
		}
	}
      out:
	return (vrfi_rc);
}
