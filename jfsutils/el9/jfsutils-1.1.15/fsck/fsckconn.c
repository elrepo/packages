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
#include "xfsckint.h"

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
  * For directory entry processing
  *
  *      defined in xchkdsk.c
  */
extern uint32_t key_len[2];
extern UniChar key[2][JFS_NAME_MAX];
extern UniChar ukey[2][JFS_NAME_MAX];

extern int32_t Uni_Name_len;
extern UniChar Uni_Name[JFS_NAME_MAX];

/* VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV
 *
 * The following are internal to this file
 *
 */
int adjust_parent(uint32_t, struct fsck_inode_record *, uint32_t);

int reset_parents(struct fsck_inode_record *, uint32_t);

/* VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV */

/*****************************************************************************
 * NAME: adjust_parent
 *
 * FUNCTION: Add an fsck inode extension record to the parent inode's
 *           fsck inode record so that the directory entry for the
 *           given child will be removed.
 *
 * PARAMETERS:
 *      child_ino        - input - ordinal number of child inode
 *      child_inorecptr  - input - pointer to an fsck record describing the
 *                                 child inode
 *      parent_ino       - input - ordinal number of parent inode
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int adjust_parent(uint32_t child_ino, struct fsck_inode_record *child_inorecptr,
		  uint32_t parent_ino)
{
	int adjp_rc = FSCK_OK;
	struct fsck_inode_ext_record *this_ext;
	struct fsck_inode_record *parent_inorecptr;
	int is_aggregate = 0;
	int alloc_ifnull = 0;

	adjp_rc =
	    get_inorecptr(is_aggregate, alloc_ifnull, parent_ino,
			  &parent_inorecptr);

	if ((adjp_rc == FSCK_OK) && (parent_inorecptr == NULL)) {
		adjp_rc = FSCK_INTERNAL_ERROR_4;
		fsck_send_msg(fsck_INTERNALERROR, adjp_rc, child_ino, parent_ino, 0);

	} else if (adjp_rc == FSCK_OK) {
		/* located the parent's inode record */
		adjp_rc = get_inode_extension(&this_ext);
		if (adjp_rc == FSCK_OK) {
			/* got extension */
			this_ext->ext_type = rmv_direntry_extension;
			this_ext->inonum = child_ino;
			this_ext->next = parent_inorecptr->ext_rec;
			this_ext->ino_type = child_inorecptr->inode_type;
			parent_inorecptr->ext_rec = this_ext;
			parent_inorecptr->adj_entries = 1;
			agg_recptr->corrections_needed = 1;
		}
	}
	return (adjp_rc);
}

/*****************************************************************************
 * NAME: adjust_parents
 *
 * FUNCTION: Add an fsck inode extension record, to the fsck inode record
 *           of each of the given inode's parent inodes, so that all
 *           directory entries for the given inode will be removed.  The
 *           exception is that, if the given inode is a directory inode,
 *           the expected parent may be omitted from this processing.
 *
 * PARAMETERS:
 *      ino_recptr  - input - pointer to an fsck record describing the inode
 *      ino_idx     - input - ordinal number of the inode
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int adjust_parents(struct fsck_inode_record *ino_recptr, uint32_t ino_idx)
{
	int adjps_rc = FSCK_OK;
	struct fsck_inode_ext_record *this_ext;
	struct fsck_inode_ext_record *rest_of_list;
	int keep_primary_parent = 0;

	/*
	 * if this is a directory with illegal hard links then the
	 * inode number in the fsck inode record is the one stored in
	 * the inode on disk.  Then if the inode isn't being released
	 * and any observed parent matches the stored parent, that
	 * parent will not be adjusted.
	 */
	if ((ino_recptr->inode_type == directory_inode)
	    && (ino_recptr->unxpctd_prnts)) {
		/* dir with multiple parents */
		if (!ino_recptr->selected_to_rls) {
			/* not being released */
			if ((ino_recptr->parent_inonum != ROOT_I)
			    || (!agg_recptr->rootdir_rebuilt)) {
				keep_primary_parent = 1;
			}
		}
	} else {
		/* not a dir with multiple parents -- this must be either an
		 * inode approved for released or an unallocated inode with
		 * parents observed.
		 */
		/*
		 * the 1st parent observed is in the inode record.  Any others are
		 * in extension records.
		 */
		if (ino_recptr->parent_inonum != 0) {
			if ((ino_recptr->parent_inonum != ROOT_I)
			    || (!agg_recptr->rootdir_rebuilt)) {
				/* either this parent isn't the root or
				   else the root dir has not been rebuilt */
				adjps_rc =
				    adjust_parent(ino_idx, ino_recptr,
						  ino_recptr->parent_inonum);
				ino_recptr->parent_inonum = 0;	/* clear it */
			}
		}
	}

	/*
	 * detach the extensions list from the inode record
	 */
	this_ext = ino_recptr->ext_rec;
	ino_recptr->ext_rec = NULL;

	while ((adjps_rc == FSCK_OK) && (this_ext != NULL)) {
		/* there may be more parents */
		rest_of_list = this_ext->next;
		if (this_ext->ext_type != parent_extension) {
			/* not a parent */
			this_ext->next = ino_recptr->ext_rec;
			ino_recptr->ext_rec = this_ext;
		} else {
			/* parent extension */
			if ((this_ext->inonum == ROOT_I)
			    && (agg_recptr->rootdir_rebuilt)) {
				/*
				 * This parent is the root and the root dir has
				 * been rebuilt.  This is equivalent to a parent
				 * marked for release.
				 */
				release_inode_extension(this_ext);

			} else if ((keep_primary_parent) &&
				   (this_ext->inonum ==
				    ino_recptr->parent_inonum)) {
				/*
				 * We're keeping the entry for the expected parent.
				 * Just drop the extension record and clear the
				 * 'unexpected parents' flag.  When we finish this
				 * routine the inode will be all set.
				 */
				release_inode_extension(this_ext);
				ino_recptr->unxpctd_prnts = 0;

			} else {
				/* either not keeping the 'primary'
				   or else this isn't it */
				adjps_rc =
				    adjust_parent(ino_idx, ino_recptr,
						  this_ext->inonum);
				release_inode_extension(this_ext);
			}
		}

		this_ext = rest_of_list;
	}
	return (adjps_rc);
}

/*****************************************************************************
 * NAME: check_connectedness
 *
 * FUNCTION:  Verify that, after approved corrections are made, all inodes
 *            in use will be connected to the root directory tree.
 *
 * PARAMETERS:  none
 *
 * NOTES:  o A directory inode must have exactly one parent inode.
 *         o A non-directory inode must have at least one parent inode.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int check_connectedness()
{
	int cc_rc = FSCK_OK;
	uint32_t ino_idx;
	struct fsck_inode_record *this_inorec;
	struct fsck_inode_ext_record *new_ext;
	int aggregate_inode = 0;

	struct fsck_ino_msg_info ino_msg_info;
	struct fsck_ino_msg_info *msg_info_ptr;

	msg_info_ptr = &ino_msg_info;
	/* all fileset owned */
	msg_info_ptr->msg_inopfx = fsck_fset_inode;

	/*
	 * detect orphan inodes, including ones which we're about to
	 * orphan by releasing inodes.
	 *
	 * if any non-orphan inode was flagged as a directory with illegal
	 * hard links, see if it's going to be true after we release inodes.
	 * If not, make sure the remaining link matches the one stored in
	 * the inode.
	 */
	cc_rc = get_inorecptr_first(aggregate_inode, &ino_idx, &this_inorec);

	while ((cc_rc == FSCK_OK) && (this_inorec != NULL)
	       && (ino_idx < FILESET_OBJECT_I)) {
		/*
		 * not interesting until we get past the root inode
		 * and the special fileset inodes.
		 */
		cc_rc =
		    get_inorecptr_next(aggregate_inode, &ino_idx, &this_inorec);
	}

	while ((cc_rc == FSCK_OK) && (this_inorec != NULL)) {

		if ((this_inorec->in_use) && (!this_inorec->selected_to_rls)) {
			/* inode in use and not selected to release */
			msg_info_ptr->msg_inonum = ino_idx;
			if (this_inorec->inode_type == directory_inode) {
				msg_info_ptr->msg_inotyp = fsck_directory;

			} else if (this_inorec->inode_type == symlink_inode) {
				msg_info_ptr->msg_inotyp = fsck_symbolic_link;

			} else if (this_inorec->inode_type ==
				   char_special_inode) {
				msg_info_ptr->msg_inotyp = fsck_char_special;

			} else if (this_inorec->inode_type ==
				   block_special_inode) {
				msg_info_ptr->msg_inotyp = fsck_block_special;

			} else if (this_inorec->inode_type == FIFO_inode) {
				msg_info_ptr->msg_inotyp = fsck_FIFO;

			} else if (this_inorec->inode_type == SOCK_inode) {
				msg_info_ptr->msg_inotyp = fsck_SOCK;

			} else {
				/* a regular file */
				msg_info_ptr->msg_inotyp = fsck_file;
			}

			if (this_inorec->parent_inonum == 0) {
				/* no parents were observed by fsck */
				fsck_send_msg(fsck_INONOPATHS,
					      fsck_ref_msg(msg_info_ptr->msg_inopfx),
					      msg_info_ptr->msg_inonum);
			} else {
				/* at least one parent observed by fsck */
				/*
				 * make adjustments to child records for
				 * parents which will be released
				 */
				cc_rc = reset_parents(this_inorec, ino_idx);
			}

			if (cc_rc == FSCK_OK) {
				if (this_inorec->parent_inonum == 0) {
					/* unconnected !! */
					/*
					 * do not issue a message to inform the user about
					 * this condition since it is a side effect of
					 * the (approved) release of some other inode(s)
					 */
					this_inorec->reconnect = 1;
					agg_recptr->corrections_approved = 1;
					cc_rc = get_inode_extension(&new_ext);
					if (cc_rc == FSCK_OK) {
						/* got an extension record */
						new_ext->ext_type =
						    add_direntry_extension;
						new_ext->inonum = ino_idx;
						new_ext->ino_type =
						    this_inorec->inode_type;
						new_ext->next =
						    agg_recptr->
						    inode_reconn_extens;
						agg_recptr->
						    inode_reconn_extens =
						    new_ext;
						/* increment for the link from
						 * parent after reconnect
						 */
						this_inorec->link_count++;
					}
				} else {
					/* else still connected */
					if (this_inorec->unxpctd_prnts) {
						/* multiple parents */
						cc_rc =
						    display_paths(ino_idx,
								  this_inorec,
								  msg_info_ptr);

						if (cc_rc == FSCK_OK) {

							if (agg_recptr->
							    processing_readwrite)
							{
								cc_rc =
								    adjust_parents
								    (this_inorec,
								     ino_idx);
								fsck_send_msg
								    (fsck_WILLFIXDIRWHDLKS,
								     fsck_ref_msg(msg_info_ptr->msg_inopfx),
								     msg_info_ptr->msg_inonum);

							} else {
								/* no write access */
								this_inorec->
								    unxpctd_prnts
								    = 0;
								agg_recptr->
								    ag_dirty =
								    1;
								fsck_send_msg
								    (fsck_DIRWHDLKS,
								     fsck_ref_msg(msg_info_ptr->msg_inopfx),
								     msg_info_ptr->msg_inonum);
							}
						}
					} else if ((cc_rc == FSCK_OK)
						   && (this_inorec->
						       crrct_prnt_inonum)) {
						/*
						 * a single parent but not the one
						 * named in the implied '..' entry
						 */
						cc_rc =
						    display_paths(ino_idx,
								  this_inorec,
								  msg_info_ptr);

						if (agg_recptr->
						    processing_readwrite) {
							fsck_send_msg
							    (fsck_WILLFIXINCREF,
							     fsck_ref_msg(msg_info_ptr->msg_inopfx),
							     msg_info_ptr->msg_inonum,
							     fsck_ref_msg(msg_info_ptr->msg_inopfx),
							     this_inorec->parent_inonum);
						} else {
							/* no write access */
							this_inorec->
							    crrct_prnt_inonum =
							    0;
							agg_recptr->ag_dirty =
							    1;
							fsck_send_msg
							    (fsck_INCINOREF,
							     fsck_ref_msg(msg_info_ptr->msg_inopfx),
							     msg_info_ptr->msg_inonum,
							     fsck_ref_msg(msg_info_ptr->msg_inopfx),
							     this_inorec->parent_inonum);
						}
					}
				}
			}
		}
		if (cc_rc == FSCK_OK) {
			cc_rc =
			    get_inorecptr_next(aggregate_inode, &ino_idx,
					       &this_inorec);
		}
	}
	return (cc_rc);
}

/*****************************************************************************
 * NAME: check_dir_integrity
 *
 * FUNCTION:  Verify that no directory has more than 1 entry for any
 *            single inode.  If a directory does, then that directory
 *            is corrupt.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int check_dir_integrity()
{
	int cdi_rc = FSCK_OK;
	uint32_t ino_idx;
	struct fsck_inode_record *this_inorec;
	struct fsck_inode_record *that_inorec;
	int aggregate_inode = 0;
	int alloc_ifnull = 0;
	struct fsck_inode_ext_record *this_ext;
	struct fsck_inode_ext_record *that_ext;
	int dup_parent_detected = 0;

	struct fsck_ino_msg_info ino_msg_info;
	struct fsck_ino_msg_info *msg_info_ptr;

	msg_info_ptr = &ino_msg_info;
	/* all fileset owned */
	msg_info_ptr->msg_inopfx = fsck_fset_inode;

	/*
	 * Verify that no inode has multiple links from the same
	 * directory.
	 */
	cdi_rc = get_inorecptr_first(aggregate_inode, &ino_idx, &this_inorec);

	while ((cdi_rc == FSCK_OK) && (this_inorec != NULL)
	       && (ino_idx < FILESET_OBJECT_I)) {
		/*
		 * not interesting until we get past the root inode
		 * and the special fileset inodes.
		 */
		cdi_rc =
		    get_inorecptr_next(aggregate_inode, &ino_idx, &this_inorec);
	}

	while ((cdi_rc == FSCK_OK) && (this_inorec != NULL)) {
		if ((this_inorec->in_use) && (!this_inorec->selected_to_rls)
		    && (this_inorec->inode_type == directory_inode)) {
			/* directory inode in use and not selected to release */
			msg_info_ptr->msg_inotyp = fsck_directory;
			msg_info_ptr->msg_inonum = ino_idx;

			if (this_inorec->parent_inonum != 0) {
				/* at least 1 parent observed by fsck */
				if (this_inorec->ext_rec) {
					/* maybe more parents have been observed */
					/*
					 * get the first entry in the extensions
					 * list on the inode record
					 */
					this_ext = this_inorec->ext_rec;

					while ((cdi_rc == FSCK_OK)
					       && (this_ext != NULL)) {
						/* there may be more parents */
						if (this_ext->ext_type ==
						    parent_extension) {
							/* a parent */
							cdi_rc =
							    get_inorecptr
							    (aggregate_inode,
							     alloc_ifnull,
							     this_ext->inonum,
							     &that_inorec);
							if ((cdi_rc == FSCK_OK)
							    && (!that_inorec->
								selected_to_rls))
							{
								/* parent isn't marked for release (yet) */
								dup_parent_detected
								    = 0;
								if (this_ext->
								    inonum ==
								    this_inorec->
								    parent_inonum)
								{
									dup_parent_detected
									    =
									    -1;
								} else {
									/* need to check for dups in rest of list */
									that_ext
									    =
									    this_ext->
									    next;
									while ((!dup_parent_detected) && (that_ext != NULL)) {
										if (that_ext->ext_type == parent_extension)
											/* another parent extension */
											if (this_ext->inonum == that_ext->inonum) {
												dup_parent_detected
												    =
												    -1;
											}
										that_ext
										    =
										    that_ext->
										    next;
									}
								}
								if (dup_parent_detected) {
									/*
									 * mark the parent's inode record for release
									 */
									that_inorec->
									    selected_to_rls
									    = 1;
									/*
									 * notify the user that the directory is bad
									 */
									fsck_send_msg
									    (fsck_BADKEYS,
									     fsck_ref_msg(fsck_directory),
									     fsck_ref_msg(msg_info_ptr->msg_inopfx),
									     this_ext->inonum,
									     37);
								}
							}
						}
						this_ext = this_ext->next;
					}
				}
			}
		}
		if (cdi_rc == FSCK_OK) {
			cdi_rc =
			    get_inorecptr_next(aggregate_inode, &ino_idx,
					       &this_inorec);
		}
	}

	return (cdi_rc);
}

/*****************************************************************************
 * NAME: check_link_counts
 *
 * FUNCTION:  Count links from child directories to their parents.
 *
 *	      Verify that the link count stored in each in-use inode
 *	      matches the number of links fsck observed for the inode.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int check_link_counts()
{
	int clc_rc = FSCK_OK;
	uint32_t ino_idx;
	int num_parents;
	int invalid_count_seen = 0;
	int low_stored_count_seen = 0;
	struct fsck_inode_ext_record *this_ext;
	struct fsck_inode_record *this_inorec;
	struct fsck_inode_record *parent_inorec;
	int done_looking = 0;
	int aggregate_inode = 0;
	int alloc_ifnull = 0;

	struct fsck_ino_msg_info ino_msg_info;
	struct fsck_ino_msg_info *msg_info_ptr;

	msg_info_ptr = &ino_msg_info;
	/* all fileset owned */
	msg_info_ptr->msg_inopfx = fsck_fset_inode;

	/*
	 * count links from child directories to their parents
	 *
	 * (These can't be counted when the parent-child relationship
	 * is observed because that observation occurs while processing
	 * the parent and until the child is processed we don't know
	 * whether the child is a directory or not.)
	 */
	clc_rc = get_inorecptr_first(aggregate_inode, &ino_idx, &this_inorec);

	while ((clc_rc == FSCK_OK) && (this_inorec != NULL)) {
		if ((this_inorec->in_use) && (!this_inorec->selected_to_rls)
		    && (!this_inorec->ignore_alloc_blks)
		    && (this_inorec->inode_type == directory_inode)) {
			/* inode is in use, not being released, and is type directory */
			/* for the self entry */
			this_inorec->link_count++;
			if ((this_inorec->parent_inonum == ROOT_I)
			    && (agg_recptr->rootdir_rebuilt)) {
				/*
				 * special case: if the parent is root and root was
				 * rebuilt, then don't increment parent
				 */
				if (this_inorec->inonum == ROOT_I) {
					/*
					 * special case: if this IS the root, then it's
					 * link from itself to itself DOES count
					 */
					this_inorec->link_count++;
				}
			} else if (this_inorec->parent_inonum != 0) {
				/* not an orphan */
				clc_rc =
				    get_inorecptr(aggregate_inode, alloc_ifnull,
						  this_inorec->parent_inonum,
						  &parent_inorec);
				if ((clc_rc != FSCK_OK)
				    || (parent_inorec == NULL)) {
					clc_rc = FSCK_INTERNAL_ERROR_13;
					fsck_send_msg(fsck_INTERNALERROR, clc_rc, ino_idx,
						      this_inorec->parent_inonum, 0);
				} else {
					/* handle the first (and usually the only) parent. */
					parent_inorec->link_count++;
				}
				if ((clc_rc == FSCK_OK)
				    && (this_inorec->ext_rec != NULL)) {
					/* there might be more parents */
					num_parents = parent_count(this_inorec);
					if (num_parents > 1) {
						/* directory with illegal links */
						this_inorec->unxpctd_prnts = 1;
						agg_recptr->corrections_needed =
						    1;
						/*
						 * Create an extension record for the parent inode
						 * number now stored in the child inode record.
						 * When we traverse the aggregate on-disk we'll copy
						 * the stored value into this field of the inode record
						 * for use when displaying paths to the inode.
						 */
						clc_rc =
						    get_inode_extension
						    (&this_ext);
						if (clc_rc == FSCK_OK) {
							/* got extension record */
							this_ext->ext_type =
							    parent_extension;
							this_ext->inonum =
							    this_inorec->
							    parent_inonum;
							this_ext->next =
							    this_inorec->
							    ext_rec;
							this_inorec->ext_rec =
							    this_ext;
							this_inorec->
							    parent_inonum = 0;
							/* already counted the first
							 * one, back when it was in the
							 * workspace inode record itself
							 */
							this_ext =
							    this_ext->next;
							while ((clc_rc ==
								FSCK_OK)
							       && (this_ext !=
								   NULL)) {
								/* exten records to check */
								if (this_ext->
								    ext_type ==
								    parent_extension)
								{
									clc_rc =
									    get_inorecptr
									    (aggregate_inode,
									     alloc_ifnull,
									     this_ext->
									     inonum,
									     &parent_inorec);
									if ((clc_rc != FSCK_OK)
									    ||
									    (parent_inorec
									     ==
									     NULL))
									{
										clc_rc
										    =
										    FSCK_INTERNAL_ERROR_14;
										fsck_send_msg
										    (fsck_INTERNALERROR,
										     clc_rc,
										     ino_idx,
										     this_ext->inonum,
										     0);
									} else {
										parent_inorec->
										    link_count++;
									}
								}
								this_ext =
								    this_ext->
								    next;
							}
						}
					}
				}
			}
		}
		if (clc_rc == FSCK_OK) {
			clc_rc =
			    get_inorecptr_next(aggregate_inode, &ino_idx,
					       &this_inorec);
		}
	}

	/*
	 * verify that stored link counts match observed link counts
	 *
	 * We have added each observed link and subtracted the stored
	 * count.  If the stored count is correct the result is 0.
	 */
	if (clc_rc == FSCK_OK) {
		clc_rc =
		    get_inorecptr_first(aggregate_inode, &ino_idx,
					&this_inorec);

		while ((clc_rc == FSCK_OK) && (this_inorec != NULL)
		       && (!done_looking)) {

			if ((this_inorec->in_use)
			    && (!this_inorec->selected_to_rls)
			    && (!this_inorec->ignore_alloc_blks)) {
				/* inode is in use and not being released */
				if (this_inorec->link_count != 0) {
					/* stored link count doesn't match fsck's observations */
					if (this_inorec->parent_inonum == 0) {
						/* inode is an orphan */
						this_inorec->crrct_link_count =
						    1;
					} else {
						/* not an orphan */
						this_inorec->crrct_link_count =
						    1;
						if (this_inorec->link_count > 0) {
							low_stored_count_seen =
							    1;
						}
						invalid_count_seen = 1;

						fsck_send_msg(fsck_BADINOLKCT,
							      fsck_ref_msg(msg_info_ptr->msg_inopfx),
							      ino_idx);
					}
				}
			}
			if (clc_rc == FSCK_OK) {
				clc_rc =
				    get_inorecptr_next(aggregate_inode,
						       &ino_idx, &this_inorec);
			}
		}

		if ((clc_rc == FSCK_OK) && (invalid_count_seen)) {

			if (agg_recptr->processing_readwrite) {
				agg_recptr->corrections_approved = 1;
				fsck_send_msg(fsck_WILLFIXLINKCTS);
			} else {
				/* no write access */
				if (low_stored_count_seen) {
					agg_recptr->ag_dirty = 1;
				}
				/*
				 * reset all link counts (in the fsck workspace) to
				 * zero so that we won't accidentally correct them
				 * while doing link count adjustments.
				 *
				 * (Link count adjustments are side effects of approved
				 * repairs.  For example, if a directory inode is
				 * released, the link count of its parent directory
				 * is decremented.)
				 */
				clc_rc =
				    get_inorecptr_first(aggregate_inode,
							&ino_idx, &this_inorec);

				while ((clc_rc == FSCK_OK)
				       && (this_inorec != NULL)) {

					if (this_inorec->in_use) {
						this_inorec->crrct_link_count =
						    0;
						this_inorec->link_count = 0;
					}
					clc_rc =
					    get_inorecptr_next(aggregate_inode,
							       &ino_idx,
							       &this_inorec);
				}
				fsck_send_msg(fsck_BADLINKCTS);
			}
		}
	}
	return (clc_rc);
}

/*****************************************************************************
 * NAME: reset_parents
 *
 * FUNCTION: Adjust the fsck notations about the inode's parent(s) if
 *           the parent(s) are corrupt or approved for release.
 *
 * PARAMETERS:
 *      ino_recptr  - input - pointer to an fsck record describing the inode
 *      ino_idx     - input - ordinal number of the inode
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int reset_parents(struct fsck_inode_record *ino_recptr, uint32_t ino_idx)
{
	int resps_rc = FSCK_OK;
	struct fsck_inode_ext_record *this_ext;
	struct fsck_inode_ext_record *rest_of_list;
	int parent_count = 0;
	uint32_t stored_parent_inonum = 0;
	struct fsck_inode_record *parent_inorecptr;
	int aggregate_inode = 0;
	int alloc_ifnull = 0;
	/*
	 * if this is a directory with illegal hard links the inode
	 * number in the fsck inode record is the one stored in the
	 * inode on disk.
	 */
	if ((ino_recptr->inode_type == directory_inode)
	    && (ino_recptr->unxpctd_prnts)) {
		/* dir with multiple parents */
		/*
		 * Save the value stored in the inode record and then clear it.
		 */
		stored_parent_inonum = ino_recptr->parent_inonum;
		ino_recptr->parent_inonum = 0;
	} else {
		/* not a dir with multiple parents */
		/*
		 * the 1st parent observed is in the inode record.
		 * Any others are in extension records.
		 */
		resps_rc = get_inorecptr(aggregate_inode, alloc_ifnull,
					 ino_recptr->parent_inonum,
					 &parent_inorecptr);
		if ((resps_rc == FSCK_OK) && (parent_inorecptr == NULL)) {
			resps_rc = FSCK_INTERNAL_ERROR_15;
			fsck_send_msg(fsck_INTERNALERROR, resps_rc, ino_idx,
				      ino_recptr->parent_inonum,
				      0);

		} else if (resps_rc == FSCK_OK) {
			if ((ino_recptr->parent_inonum == ROOT_I)
			    && (agg_recptr->rootdir_rebuilt)) {
				/*
				 * special case: if the parent is root and root
				 * was rebuilt, then this is an orphan
				 */
				ino_recptr->parent_inonum = 0;
				ino_recptr->link_count--;
			} else if ((!parent_inorecptr->selected_to_rls)
				   && (!parent_inorecptr->ignore_alloc_blks)) {
				/*
				 * keeping this parent and haven't found the
				 * tree to be corrupt
				 */
				parent_count++;

			} else {
				/* releasing this parent */
				ino_recptr->parent_inonum = 0;
				ino_recptr->link_count--;
			}
		}
	}
	/*
	 * detach the extensions list from the inode record
	 */
	this_ext = ino_recptr->ext_rec;
	ino_recptr->ext_rec = NULL;
	while ((resps_rc == FSCK_OK) && (this_ext != NULL)) {
		/* there may be more parents */
		rest_of_list = this_ext->next;
		if (this_ext->ext_type != parent_extension) {
			/* not a parent */
			this_ext->next = ino_recptr->ext_rec;
			ino_recptr->ext_rec = this_ext;
		} else {
			/* parent extension */
			resps_rc = get_inorecptr(aggregate_inode, alloc_ifnull,
						 this_ext->inonum,
						 &parent_inorecptr);
			if ((resps_rc == FSCK_OK) && (parent_inorecptr == NULL)) {
				resps_rc = FSCK_INTERNAL_ERROR_16;
				fsck_send_msg(fsck_INTERNALERROR, resps_rc, ino_idx,
					      this_ext->inonum, 0);

			} else if (resps_rc == FSCK_OK) {
				if ((ino_recptr->parent_inonum == ROOT_I)
				    && (agg_recptr->rootdir_rebuilt)) {
					/*
					 * special case: if the parent is root and
					 * root was rebuilt, then this is an orphan
					 */
					release_inode_extension(this_ext);
					ino_recptr->link_count--;

				} else if ((!parent_inorecptr->selected_to_rls)
					   && (!parent_inorecptr->
					       ignore_alloc_blks)) {
					/* keeping this parent */
					parent_count++;
					if (ino_recptr->parent_inonum == 0) {
						ino_recptr->parent_inonum =
						    this_ext->inonum;
						release_inode_extension
						    (this_ext);
					} else {
						/* put it back on the list */
						this_ext->next =
						    ino_recptr->ext_rec;
						ino_recptr->ext_rec = this_ext;
					}

				} else {
					/* releasing this parent */
					release_inode_extension(this_ext);
					ino_recptr->link_count--;
				}
			}
		}
		this_ext = rest_of_list;
	}
	/*
	 * at this point, if there is at least 1 observed parent which
	 * is not being released, then a parent inode number is stored in
	 * the inode record and any other parents are described in extension
	 * records.
	 *
	 * if this is not a directory inode, we're done.
	 *
	 * if this is a directory inode, need to recheck for illegal hard
	 * links and incorrect parent inode entry.
	 */
	if ((resps_rc == FSCK_OK)
	    && (ino_recptr->inode_type == directory_inode)) {
		/* a directory */
		if (parent_count == 1) {
			/* 1 parent now */
			if (ino_recptr->unxpctd_prnts) {
				/* entered with multiple links */
				/* reset flag */
				ino_recptr->unxpctd_prnts = 0;
				if (ino_recptr->parent_inonum !=
				    stored_parent_inonum) {
					/*
					 * Remaining parent doesn't match the one
					 * the on-device inode says owns it.
					 */
					ino_recptr->crrct_prnt_inonum = 1;
					agg_recptr->corrections_needed = 1;
					agg_recptr->corrections_approved = 1;
				}
			}
		} else if (parent_count == 0) {
			/* no parents now */
			ino_recptr->crrct_prnt_inonum = 0;
			ino_recptr->unxpctd_prnts = 0;
		} else {
			/* multiple parents still */
			ino_recptr->unxpctd_prnts = 1;
			agg_recptr->corrections_needed = 1;
			resps_rc = get_inode_extension(&this_ext);
			if (resps_rc == FSCK_OK) {
				this_ext->ext_type = parent_extension;
				this_ext->inonum = ino_recptr->parent_inonum;
				this_ext->next = ino_recptr->ext_rec;
				ino_recptr->ext_rec = this_ext;
				ino_recptr->parent_inonum =
				    stored_parent_inonum;
			}
		}
	}
	return (resps_rc);
}
