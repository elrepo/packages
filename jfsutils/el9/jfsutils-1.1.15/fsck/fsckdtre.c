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

/*
 * for inline unicode functions
 */
#include "jfs_byteorder.h"
#include "jfs_unicode.h"
#include "jfs_filsys.h"
#include "devices.h"
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

#define LEFT_KEY_LOWER -1
#define KEYS_MATCH      0
#define LEFT_KEY_HIGHER 1

struct fsck_Dtree_info {
	dtroot_t *dtr_ptr;
	dtpage_t *dtp_ptr;
	pxd_t *pxd_ptr;
	int8_t *dtstbl;
	int16_t last_dtidx;
	int16_t freelist_first_dtidx;
	int16_t freelist_count;
	struct dtslot *slots;
	int8_t slot_map[DTPAGEMAXSLOT];
	int64_t ext_addr;
	uint32_t ext_length;
	struct dtreeQelem *this_Qel;
	struct dtreeQelem *next_Qel;
	int64_t last_node_addr;
	int8_t last_level;
	int8_t leaf_level;
	int8_t leaf_seen;
	int16_t max_slotidx;
	int16_t this_key_idx;
	int16_t last_key_idx;
	uint32_t key_len[2];
	UniChar key[2][JFS_NAME_MAX];
};

int direntry_get_objnam_node(uint32_t, int8_t *, struct dtslot *, int, int *,
			     UniChar *, int8_t *);

int dTree_binsrch_internal_page(struct fsck_Dtree_info *, UniChar *,
				uint32_t, int8_t, int8_t *, int8_t *, int8_t *,
				struct fsck_inode_record *);

int dTree_binsrch_leaf(struct fsck_Dtree_info *, UniChar *, uint32_t,
		       int8_t, int8_t *, int8_t *, int8_t *, int8_t *,
		       struct fsck_inode_record *);

int dTree_key_compare(UniChar *, uint8_t, UniChar *, uint8_t, int *);

int dTree_key_compare_leaflvl(UniChar *, uint8_t, UniChar *, uint8_t, int8_t *);

int dTree_key_compare_prntchld(struct fsck_Dtree_info *, UniChar *, uint8_t,
			       UniChar *, uint8_t, int8_t *);

int dTree_key_compare_samelvl(UniChar *, uint8_t, UniChar *, uint8_t, int8_t *);

int dTree_key_extract(struct fsck_Dtree_info *, int, UniChar *,
		      uint32_t *, int8_t, int8_t, struct fsck_inode_record *);

int dTree_key_extract_cautiously(struct fsck_Dtree_info *, int, UniChar *,
				 uint32_t *, int8_t, int8_t,
				 struct fsck_inode_record *);

int dTree_key_extract_record(struct fsck_Dtree_info *, int, UniChar *,
			     uint32_t *, int8_t, int8_t,
			     struct fsck_inode_record *);

int dTree_key_to_upper(UniChar *, UniChar *, int32_t);

int dTree_node_first_key(struct fsck_Dtree_info *, struct fsck_inode_record *,
			 struct fsck_ino_msg_info *, int);

int dTree_node_first_in_level(struct fsck_Dtree_info *,
			      struct fsck_inode_record *,
			      struct fsck_ino_msg_info *, int);

int dTree_node_last_in_level(struct fsck_Dtree_info *,
			     struct fsck_inode_record *,
			     struct fsck_ino_msg_info *, int);
int dTree_node_not_first_in_level(struct fsck_Dtree_info *,
				  struct fsck_inode_record *,
				  struct fsck_ino_msg_info *, int);

int dTree_node_not_last_in_level(struct fsck_Dtree_info *,
				 struct fsck_inode_record *,
				 struct fsck_ino_msg_info *, int);

int dTree_node_size_check(struct fsck_Dtree_info *, int8_t, int8_t, int8_t,
			  struct fsck_inode_record *,
			  struct fsck_ino_msg_info *, int);

int dTree_process_internal_slots(struct fsck_Dtree_info *,
				 struct fsck_inode_record *,
				 struct fsck_ino_msg_info *, int);

int dTree_process_leaf_slots(struct fsck_Dtree_info *,
			     struct fsck_inode_record *, struct dinode *,
			     struct fsck_ino_msg_info *, int);

int dTree_verify_slot_freelist(struct fsck_Dtree_info *,
			       struct fsck_inode_record *,
			       struct fsck_ino_msg_info *, int);

int process_valid_dir_node(int8_t *, struct dtslot *, int,
			   struct fsck_inode_record *,
			   struct fsck_ino_msg_info *, int);

/* VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV */

/*****************************************************************************
 * NAME: direntry_add
 *
 * FUNCTION: Add an entry to a directory.
 *
 * PARAMETERS:
 *      parent_inoptr  - input - pointer to the directory inode, in an
 *                               fsck buffer, to which the entry should
 *                               be added.
 *      child_inonum   - input - inode number to put in the new directory
 *                               entry.
 *      child_name     - input - pointer to the file name to put in the
 *                               new directory entry.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int direntry_add(struct dinode *parent_inoptr, uint32_t child_inonum,
		 UniChar * child_name)
{
	int ad_rc = FSCK_OK;
	struct component_name uniname_struct;

	uniname_struct.namlen = UniStrlen(child_name);
	uniname_struct.name = child_name;

	ad_rc = fsck_dtInsert(parent_inoptr, &uniname_struct, &child_inonum);
	if (ad_rc == FSCK_OK) {
		ad_rc = inode_put(parent_inoptr);
	} else {
		if (ad_rc < 0) {
			fsck_send_msg(fsck_INTERNALERROR, FSCK_INTERNAL_ERROR_60,
				      ad_rc, child_inonum, parent_inoptr->di_number);
			ad_rc = FSCK_INTERNAL_ERROR_60;
		} else {
			if (ad_rc == FSCK_BLKSNOTAVAILABLE) {
				fsck_send_msg(fsck_CANTRECONINSUFSTG, 3);
			}
		}
	}

	return (ad_rc);
}

/*****************************************************************************
 * NAME: direntry_get_inonum
 *
 * FUNCTION:  Get the inode number for the file whose name is given.
 *
 * PARAMETERS:
 *      parent_inonum   - input - the inode number of a directory containing
 *                                an entry for the inode whose name is
 *                                desired.
 *      obj_name_length - input - the length of the file name in mixed case
 *      obj_name        - input - pointer to the file name in mixed case
 *      obj_NAME_length - input - the length of the file name in upper case
 *      obj_NAME        - input - pointer to the file name in upper case
 *      found_inonum    - input - pointer to a variable in which to return
 *                                the inode number stored in the (found) entry
 *
 * NOTES:	A case insensitive search is conducted.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int direntry_get_inonum(uint32_t parent_inonum,
			int obj_name_length,
			UniChar * obj_name,
			int obj_NAME_length,
			UniChar * obj_NAME, uint32_t * found_inonum)
{
	int gdi_rc = FSCK_OK;
	struct dinode *root_inoptr;
	int is_aggregate = 0;
	int alloc_ifnull = 0;
	int which_table = FILESYSTEM_I;
	struct dtslot *slot_ptr;
	int8_t entry_found;
	struct fsck_inode_record *root_inorecptr;

	gdi_rc = inode_get(is_aggregate, which_table, ROOT_I, &root_inoptr);

	if (gdi_rc == FSCK_OK) {
		/* got the root inode */
		gdi_rc =
		    get_inorecptr(is_aggregate, alloc_ifnull, ROOT_I,
				  &root_inorecptr);

		if ((gdi_rc == FSCK_OK) && (root_inorecptr == NULL)) {
			gdi_rc = FSCK_INTERNAL_ERROR_46;
		} else if (gdi_rc == FSCK_OK) {
			gdi_rc =
			    dTree_search(root_inoptr, obj_name, obj_name_length,
					 obj_NAME, obj_NAME_length, &slot_ptr,
					 &entry_found, root_inorecptr);
		}

		if ((gdi_rc == FSCK_OK) && (entry_found)) {
			*found_inonum = ((struct ldtentry *) slot_ptr)->inumber;
		}
	}
	return (gdi_rc);
}

/*****************************************************************************
 * NAME: direntry_get_objnam
 *
 * FUNCTION: Find the file name for the given inode number in the given
 *           directory inode.
 *
 * PARAMETERS:
 *      parent_inonum     - input - the inode number of the directory
 *                                  containing an entry for the object
 *      obj_inonum        - input - the inode number of the object for
 *                                  which the file name is desired
 *      found_name_length - input - pointer to a variable in which to return
 *                                  the length of the object name
 *      found_name        - input - pointer to a buffer in which to return
 *                                  the object name
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int direntry_get_objnam(uint32_t parent_inonum,
			uint32_t obj_inonum,
			int *found_name_length, UniChar * found_name)
{
	int dgon_rc = FSCK_OK;
	int is_aggregate = 0;
	int which_it = FILESYSTEM_I;	/* in release 1 there's exactly 1 fileset */
	uint32_t parent_inoidx;
	struct dinode *parent_inoptr;
	dtpage_t *leaf_ptr;
	int64_t leaf_offset;
	int8_t dir_inline;
	int8_t dir_rootleaf;
	dtroot_t *dtroot_ptr;
	int8_t *dtstbl;
	struct dtslot *slots;
	int last_dtidx;
	int8_t entry_found;

	/* assume no match will be found */
	*found_name_length = 0;
	parent_inoidx = (uint32_t) parent_inonum;

	dgon_rc =
	    inode_get(is_aggregate, which_it, parent_inoidx, &parent_inoptr);

	if (dgon_rc != FSCK_OK) {
		/* but we read it before!  */
		/* this is fatal */
		dgon_rc = FSCK_FAILED_CANTREAD_DIRNOW;
	} else {
		/* got the parent inode */
		dgon_rc =
		    find_first_dir_leaf(parent_inoptr, &leaf_ptr, &leaf_offset,
					&dir_inline, &dir_rootleaf);
		if (dgon_rc != FSCK_OK) {
			/* we already verified the dir! */
			dgon_rc = FSCK_FAILED_DIRGONEBAD;
		} else {
			/* we found and read the first leaf */
			if (dir_rootleaf) {
				/* rootleaf directory tree */
				dtroot_ptr =
				    (dtroot_t *) & (parent_inoptr->di_btroot);
				dtstbl =
				    (int8_t *) & (dtroot_ptr->header.stbl[0]);
				slots = &(dtroot_ptr->slot[0]);
				last_dtidx = dtroot_ptr->header.nextindex - 1;
				dgon_rc =
				    direntry_get_objnam_node(obj_inonum, dtstbl,
							     slots, last_dtidx,
							     found_name_length,
							     found_name,
							     &entry_found);
			} else {
				/* it's a separate node and probably first in a chain */
				/* try the first leaf */
				dtstbl =
				    (int8_t *) & (leaf_ptr->
						  slot[leaf_ptr->header.
						       stblindex]);
				slots = &(leaf_ptr->slot[0]);
				last_dtidx = leaf_ptr->header.nextindex - 1;
				dgon_rc =
				    direntry_get_objnam_node(obj_inonum, dtstbl,
							     slots, last_dtidx,
							     found_name_length,
							     found_name,
							     &entry_found);
				/* try the remaining leaves */
				while ((dgon_rc == FSCK_OK) && (!entry_found)
				       && (leaf_ptr->header.next != 0)) {
					dgon_rc =
					    dnode_get(leaf_ptr->header.next,
						      BYTESPERPAGE, &leaf_ptr);
					if (dgon_rc != FSCK_OK) {
						/* this is fatal */
						dgon_rc =
						    FSCK_FAILED_READ_NODE4;
					} else {
						/* got the sibling leaf node */
						dtstbl =
						    (int8_t *) & (leaf_ptr->
								  slot
								  [leaf_ptr->
								   header.
								   stblindex]);
						slots = &(leaf_ptr->slot[0]);
						last_dtidx =
						    leaf_ptr->header.nextindex -
						    1;
						dgon_rc =
						    direntry_get_objnam_node
						    (obj_inonum, dtstbl, slots,
						     last_dtidx,
						     found_name_length,
						     found_name, &entry_found);
					}
				}
			}
		}
	}

	if ((dgon_rc == FSCK_OK) && (!entry_found)) {
		/*
		 * but we saw this entry earlier!
		 */
		dgon_rc = FSCK_FAILED_DIRENTRYGONE;
	}
	return (dgon_rc);
}

/*****************************************************************************
 * NAME: direntry_get_objnam_node
 *
 * FUNCTION: Find the file name, in the given directory leaf node, of the
 *           object whose inode number is given.
 *
 * PARAMETERS:
 *      child_inonum       - input - the inode number in the directory entry
 *      dtstbl             - input - pointer to the sorted entry index table
 *                                   in the directory node
 *      slots              - input - pointer to slot[0] in the directory node
 *      last_dtidx         - input - last valid entry in the directory
 *                                   node's sorted entry index table
 *      found_name_length  - input - pointer to a variable in which to return
 *                                   the length of the filename being returned
 *                                   in *found_name
 *      found_name         - input - pointer to a buffer in which to return
 *                                   the filename extracted from the node
 *      entry_found        - input - pointer to a variable in which to return
 *                                   !0 if an entry is found with inode number
 *                                      child_inonum
 *                                    0 if no entry is found with inode number
 *                                      child_inonum
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int direntry_get_objnam_node(uint32_t child_inonum,
			     int8_t * dtstbl,
			     struct dtslot *slots,
			     int last_dtidx,
			     int *found_name_length,
			     UniChar * found_name, int8_t * entry_found)
{
	int dgonn_rc = FSCK_OK;
	int dtidx;
	struct ldtentry *entry_ptr = NULL;
	struct dtslot *contin_entry_ptr;
	int seg_length, UniChars_left, seg_max;

	if (sb_ptr->s_flag & JFS_DIR_INDEX)
		seg_max = DTLHDRDATALEN;
	else
		seg_max = DTLHDRDATALEN_LEGACY;
	/* assume it's not here */
	*entry_found = 0;

	/*
	 * see if this leaf has the entry for the requested child
	 */
	for (dtidx = 0; ((dgonn_rc == FSCK_OK) && (dtidx <= last_dtidx)
			 && (!(*entry_found))); dtidx++) {
		/* for each entry in the dtstbl index to slots */
		entry_ptr = (struct ldtentry *) &(slots[dtstbl[dtidx]]);
		if (entry_ptr->inumber == child_inonum) {
			*entry_found = -1;
		}
	}

	/*
	 * if the child's entry was found, construct its name
	 */
	if (*entry_found) {
		/* the child's entry was found */
		UniChars_left = entry_ptr->namlen;
		if (UniChars_left > seg_max) {
			seg_length = seg_max;
			UniChars_left -= seg_max;
		} else {
			seg_length = UniChars_left;
			UniChars_left = 0;
		}
		memcpy((void *) &(found_name[*found_name_length]),
		       (void *) &(entry_ptr->name[0]),
		       (size_t) (seg_length * sizeof (UniChar))
		    );
		*found_name_length = seg_length;

		if (entry_ptr->next != -1) {
			/* name is continued */
			contin_entry_ptr =
			    (struct dtslot *) &(slots[entry_ptr->next]);
		} else {
			contin_entry_ptr = NULL;
		}

		seg_max = DTSLOTDATALEN;
		while ((contin_entry_ptr != NULL)
		       && ((*found_name_length) <= JFS_NAME_MAX)) {
			/* name is continued */
			if (UniChars_left > seg_max) {
				seg_length = seg_max;
				UniChars_left -= seg_max;
			} else {
				seg_length = UniChars_left;
				UniChars_left = 0;
			}

			memcpy((void *) &(found_name[*found_name_length]),
			       (void *) &(contin_entry_ptr->name[0]),
			       (size_t) (seg_length * sizeof (UniChar))
			    );
			*found_name_length += seg_length;
			if (contin_entry_ptr->next != -1) {
				/* still more */
				contin_entry_ptr = (struct dtslot *)
				    &(slots[contin_entry_ptr->next]);
			} else {
				contin_entry_ptr = NULL;
			}
		}

		if (contin_entry_ptr != NULL) {
			dgonn_rc = FSCK_FAILED_DIRGONEBAD2;
		}
	}
	return (dgonn_rc);
}

/*****************************************************************************
 * NAME: direntry_remove
 *
 * FUNCTION: Issue an fsck message, depending on the message's protocol
 *           according to the fsck message arrays (above).  Log the
 *           message to fscklog if logging is in effect.
 *
 * PARAMETERS:
 *      parent_inoptr  - input - pointer to the directory inode in an fsck
 *                               buffer
 *      child_inonum   - input - the inode number in the directory entry
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int direntry_remove(struct dinode *parent_inoptr, uint32_t child_inonum)
{
	int rd_rc = FSCK_OK;
	UniChar uniname[JFS_NAME_MAX];
	int uniname_length;
	struct component_name uniname_struct;

	rd_rc = direntry_get_objnam(parent_inoptr->di_number, child_inonum,
				    &uniname_length, &(uniname[0]));

	if (rd_rc == FSCK_OK) {
		uniname_struct.namlen = uniname_length;
		uniname_struct.name = &(uniname[0]);

		rd_rc =
		    fsck_dtDelete(parent_inoptr, &uniname_struct,
				  &child_inonum);
		if (rd_rc == FSCK_OK) {
			rd_rc = inode_put(parent_inoptr);
		} else {
			fsck_send_msg(fsck_INTERNALERROR, FSCK_INTERNAL_ERROR_61,
				      rd_rc, child_inonum, parent_inoptr->di_number);
			rd_rc = FSCK_INTERNAL_ERROR_61;
		}
	} else {
		fsck_send_msg(fsck_INTERNALERROR, FSCK_INTERNAL_ERROR_62,
			      rd_rc, child_inonum, parent_inoptr->di_number);
		rd_rc = FSCK_INTERNAL_ERROR_62;
	}

	return (rd_rc);
}

/*****************************************************************************
 * NAME: dTree_binsrch_internal_page
 *
 * FUNCTION: Perform a binary search, on the given dTree internal node, for
 *           the entry which is parent/grandparent/... to the leaf entry
 *           containing the given filename.
 *
 * PARAMETERS:
 *      dtiptr            - input - pointer to an fsck record describing the
 *                                  directory tree
 *      given_name        - input - pointer to the name to search for
 *      given_name_len    - input - number of characters in given_name
 *      is_root           - input - !0 => specified node is a B+ Tree root
 *                                   0 => specified node is not a B+ Tree root
 *      no_key_match      - input - pointer to a variable in which to return
 *                                  an indication of whether the search has
 *                                  been completed because it has been
 *                                  determined that no entry in the directory
 *                                  matches given_name
 *                                  !0 if the search should be ended, No
 *                                     match found
 *                                   0 if either the search should continue or
 *                                     a match has been found
 *      slot_selected     - input - pointer to a variable in which to return
 *                                  an indication of whether the search has
 *                                  been completed at this level by finding
 *                                  a match, a prefix match, or reason to
 *                                  believe we may still find a match.
 *                                  !0 if a slot has been selected
 *                                   0 if either the search should continue or
 *                                     it has been determined that there is
 *                                     no match in the directory
 *      selected_slotidx  - input - pointer to a variable in which to return
 *                                  the number n such that slot[n] contains
 *                                  (or begins) the key which is a match or
 *                                  a prefix match for given_name
 *      inorecptr         - input - pointer to an fsck inode record describing
 *                                  the inode
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int dTree_binsrch_internal_page(struct fsck_Dtree_info *dtiptr,
				UniChar * given_name,
				uint32_t given_name_len,
				int8_t is_root,
				int8_t * no_key_match,
				int8_t * slot_selected,
				int8_t * selected_slotidx,
				struct fsck_inode_record *inorecptr)
{
	int dbip_rc = FSCK_OK;
	UniChar *this_name;
	uint32_t *this_name_len;
	int lowidx, mididx, highidx;
	int prev_idx, next_idx;
	int8_t is_leaf = 0;
	int outcome;

	this_name = &(key[1][0]);
	this_name_len = &(key_len[1]);

	*no_key_match = 0;
	*slot_selected = 0;
	*selected_slotidx = 0;

	lowidx = 0;
	highidx = dtiptr->last_dtidx;

	dbip_rc =
	    dTree_key_extract(dtiptr, lowidx, this_name, this_name_len, is_root,
			      is_leaf, inorecptr);
	/*
	 * note that we can proceed with (some) confidence since we never
	 * search a directory until after we've verified it's structure
	 */
	if (dbip_rc == FSCK_OK) {
		dbip_rc =
		    dTree_key_compare(given_name, given_name_len, this_name,
				      (*this_name_len), &outcome);
		if (outcome == LEFT_KEY_LOWER) {
			/* given key < 1st in this node */
			*no_key_match = -1;
		} else if (outcome == KEYS_MATCH) {
			/* given key == 1st in this node */
			*slot_selected = -1;
			*selected_slotidx = dtiptr->dtstbl[lowidx];
		} else {
			/* given key > 1st in this node */
			dbip_rc =
			    dTree_key_extract(dtiptr, highidx, this_name,
					      this_name_len, is_root, is_leaf,
					      inorecptr);
			/*
			 * note that we can proceed with (some) confidence
			 * since we never search a directory until after we've
			 * verified it's structure
			 */
			if (dbip_rc == FSCK_OK) {
				dbip_rc =
				    dTree_key_compare(given_name,
						      given_name_len, this_name,
						      (*this_name_len),
						      &outcome);
				if (outcome == LEFT_KEY_HIGHER) {
					/* given key > last in the node */
					*slot_selected = -1;
					*selected_slotidx =
					    dtiptr->dtstbl[highidx];
				} else if (outcome == KEYS_MATCH) {
					*slot_selected = -1;
					*selected_slotidx =
					    dtiptr->dtstbl[highidx];
				}
			}
		}
	}
	/*
	 * Find the first key equal or, if no exact match exists, find the
	 * last key lower.
	 */
	while ((!(*slot_selected)) && (!(*no_key_match))
	       && (dbip_rc == FSCK_OK)) {
		/* haven't chosen one but haven't ruled anything out */
		mididx = ((highidx - lowidx) >> 1) + lowidx;
		dbip_rc =
		    dTree_key_extract(dtiptr, mididx, this_name, this_name_len,
				      is_root, is_leaf, inorecptr);
		/*
		 * note that we can proceed with (some) confidence since we never
		 * search a directory until after we've verified it's structure
		 */
		if (dbip_rc == FSCK_OK) {
			dbip_rc =
			    dTree_key_compare(given_name, given_name_len,
					      this_name, (*this_name_len),
					      &outcome);
			if (dbip_rc == FSCK_OK) {
				if (outcome == KEYS_MATCH) {
					/* given name == mid key */
					*slot_selected = -1;
					*selected_slotidx =
					    dtiptr->dtstbl[mididx];
				} else if (outcome == LEFT_KEY_HIGHER) {
					/* given name > mid key */
					next_idx = mididx + 1;
					dbip_rc =
					    dTree_key_extract(dtiptr, next_idx,
							      this_name,
							      this_name_len,
							      is_root, is_leaf,
							      inorecptr);
					/*
					 * note that we can proceed with (some)
					 * confidence since we never search a directory
					 * until after we've verified it's structure
					 */
					if (dbip_rc == FSCK_OK) {
						/* got next key */
						dbip_rc =
						    dTree_key_compare
						    (given_name, given_name_len,
						     this_name,
						     (*this_name_len),
						     &outcome);
						if (dbip_rc == FSCK_OK) {
							if (outcome ==
							    LEFT_KEY_LOWER) {
								/* the next one is higher */
								*slot_selected =
								    -1;
								*selected_slotidx
								    =
								    dtiptr->
								    dtstbl
								    [mididx];
							} else if (outcome ==
								   KEYS_MATCH) {
								/* since we've done the
								 * extract and compare might as well see if
								 * we lucked into a match
								 */
								*slot_selected =
								    -1;
								*selected_slotidx
								    =
								    dtiptr->
								    dtstbl
								    [next_idx];
							} else {
								/* not on or just before the money */
								/* this key is higher than the middle */
								lowidx = mididx;
							}
						}
					}
				} else {
					/* given name < mid key */
					prev_idx = mididx - 1;
					dbip_rc =
					    dTree_key_extract(dtiptr, prev_idx,
							      this_name,
							      this_name_len,
							      is_root, is_leaf,
							      inorecptr);
					/*
					 * note that we can proceed with (some)
					 * confidence since we never search a directory
					 * until after we've verified it's structure
					 */
					if (dbip_rc == FSCK_OK) {
						dbip_rc =
						    dTree_key_compare
						    (given_name, given_name_len,
						     this_name,
						     (*this_name_len),
						     &outcome);
						if (dbip_rc == FSCK_OK) {
							if (outcome ==
							    LEFT_KEY_HIGHER) {
								/* the prev one is lower */
								*slot_selected =
								    -1;
								*selected_slotidx
								    =
								    dtiptr->
								    dtstbl
								    [prev_idx];
							} else if (outcome ==
								   KEYS_MATCH) {
								/* since we've done the
								 * extract and compare might as well see if
								 * we stumbled onto a match
								 */
								*slot_selected =
								    -1;
								*selected_slotidx
								    =
								    dtiptr->
								    dtstbl
								    [prev_idx];
							} else {
								/* not on or just after the money */
								/* this key is lower than the middle */
								highidx =
								    mididx;
							}
						}
					}
				}
			}
		}
	}
	return (dbip_rc);
}

/*****************************************************************************
 * NAME: dTree_binsrch_leaf
 *
 * FUNCTION: Perform a binary search, on the given dTree leaf node, for the
 *           entry (if any) which contains the given filename.
 *
 * PARAMETERS:
 *      dtiptr            - input - pointer to an fsck record describing the
 *                                  directory tree
 *      given_name        - input - pointer to the name to search for
 *      given_name_len    - input - number of characters in given_name
 *      is_root           - input - !0 => specified node is a B+ Tree root
 *                                   0 => specified node is not a B+ Tree root
 *      no_key_match      - input - pointer to a variable in which to return
 *                                  an indication of whether the search has
 *                                  been completed because it has been
 *                                  determined that no entry in the directory
 *                                  matches given_name
 *                                  !0 if the search should be ended, No
 *                                     match found
 *                                   0 if either the search should continue or
 *                                     a match has been found
 *      key_matched       - input - pointer to a variable in which to return
 *                                   !0 if an exact match has been found
 *                                    0 if no match has been found
 *      slot_selected     - input - pointer to a variable in which to return
 *                                  an indication of whether the search has
 *                                  been completed at this level by finding
 *                                  a match or reason to reason to believe
 *                                  we may still find a match.
 *                                  !0 if a slot has been selected
 *                                   0 if either the search should continue or
 *                                     it has been determined that there is
 *                                     no match in the directory
 *      selected_slotidx  - input - pointer to a variable in which to return
 *                                  the number n such that slot[n] contains
 *                                  (or begins) the key which is a match for
 *                                  given_name
 *      inorecptr         - input - pointer to an fsck inode record describing
 *                                  the inode
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int dTree_binsrch_leaf(struct fsck_Dtree_info *dtiptr,
		       UniChar * given_name,
		       uint32_t given_name_len,
		       int8_t is_root,
		       int8_t * no_key_match,
		       int8_t * key_matched,
		       int8_t * slot_selected,
		       int8_t * selected_slotidx,
		       struct fsck_inode_record *inorecptr)
{
	int dbl_rc = FSCK_OK;

	UniChar *this_name;
	uint32_t *this_name_len;
	int lowidx, mididx, highidx;
	int prev_idx, next_idx;
	int8_t is_leaf = -1;
	int outcome;

	this_name = &(key[1][0]);
	this_name_len = &(key_len[1]);

	*no_key_match = 0;
	*slot_selected = 0;
	*selected_slotidx = 0;

	lowidx = 0;
	highidx = dtiptr->last_dtidx;

	dbl_rc =
	    dTree_key_extract(dtiptr, lowidx, this_name, this_name_len, is_root,
			      is_leaf, inorecptr);
	/*
	 * note that we can proceed with (some) confidence since we never
	 * search a directory until after we've verified it's structure
	 */

	if (dbl_rc == FSCK_OK) {
		/*
		 * If this is a case insensitive search, we need to fold the
		 * extracted key to upper case before doing the comparison.
		 * The given key should already be in all upper case.
		 */
		dTree_key_to_upper(this_name, &(ukey[0][0]), (*this_name_len));
		dbl_rc = dTree_key_compare(given_name, given_name_len,
					   &(ukey[0][0]), (*this_name_len),
					   &outcome);

		if (outcome == LEFT_KEY_LOWER) {
			/* given key < 1st in this node */
			*no_key_match = -1;
		} else if (outcome == KEYS_MATCH) {
			/* given key == 1st in this node */
			*no_key_match = 0;
			*key_matched = -1;
			*slot_selected = -1;
			*selected_slotidx = dtiptr->dtstbl[lowidx];
		} else {
			/* given key > 1st in this node */
			dbl_rc =
			    dTree_key_extract(dtiptr, highidx, this_name,
					      this_name_len, is_root, is_leaf,
					      inorecptr);
			/*
			 * note that we can proceed with (some)
			 * confidence since we never search a directory
			 * until after we've verified it's structure
			 */
			if (dbl_rc == FSCK_OK) {
				/*
				 * If this is a case insensitive search, we need to fold the
				 * extracted key to upper case before doing the comparison.
				 * The given key should already be in all upper case.
				 */
				dTree_key_to_upper(this_name, &(ukey[0][0]),
						   (*this_name_len));
				dbl_rc =
				    dTree_key_compare(given_name,
						      given_name_len,
						      &(ukey[0][0]),
						      (*this_name_len),
						      &outcome);
				if (outcome == LEFT_KEY_HIGHER) {
					/* given key > last in the node */
					*no_key_match = -1;
				} else if (outcome == KEYS_MATCH) {
					*no_key_match = 0;
					*key_matched = -1;
					*slot_selected = -1;
					*selected_slotidx =
					    dtiptr->dtstbl[highidx];
				}
			}
		}
	}
	/*
	 * Try to find a name match
	 */
	while ((!(*slot_selected)) && (!(*no_key_match)) && (dbl_rc == FSCK_OK)) {
		/*
		 * haven't chosen one, haven't seen a match,
		 * but haven't ruled anything out
		 */
		mididx = ((highidx - lowidx) >> 1) + lowidx;
		dbl_rc =
		    dTree_key_extract(dtiptr, mididx, this_name, this_name_len,
				      is_root, is_leaf, inorecptr);
		/*
		 * note that we can proceed with (some) confidence since we never
		 * search a directory until after we've verified it's structure
		 */
		if (dbl_rc == FSCK_OK) {
			/*
			 * If this is a case insensitive search, we need to fold the
			 * extracted key to upper case before doing the comparison.
			 * The given key should already be in all upper case.
			 */
			dTree_key_to_upper(this_name, &(ukey[0][0]),
					   (*this_name_len));
			dbl_rc =
			    dTree_key_compare(given_name, given_name_len,
					      &(ukey[0][0]), (*this_name_len),
					      &outcome);

			if (dbl_rc == FSCK_OK) {
				if (outcome == KEYS_MATCH) {
					/* given name == mid key */
					*no_key_match = 0;
					*key_matched = -1;
					*slot_selected = -1;
					*selected_slotidx =
					    dtiptr->dtstbl[mididx];
				} else if (outcome == LEFT_KEY_HIGHER) {
					/* given name > mid key */
					next_idx = mididx + 1;
					dbl_rc =
					    dTree_key_extract(dtiptr, next_idx,
							      this_name,
							      this_name_len,
							      is_root, is_leaf,
							      inorecptr);
					/*
					 * note that we can proceed with (some)
					 * confidence since we never search a directory
					 * until after we've verified it's structure
					 */
					if (dbl_rc == FSCK_OK) {
						/*
						 * If this is a case insensitive search, we need to fold the
						 * extracted key to upper case before doing the comparison.
						 * The given key should already be in all upper case.
						 */
						dTree_key_to_upper(this_name,
								   &(ukey[0]
								     [0]),
								   (*this_name_len));
						dbl_rc =
						    dTree_key_compare
						    (given_name, given_name_len,
						     &(ukey[0][0]),
						     (*this_name_len),
						     &outcome);
						if (dbl_rc == FSCK_OK) {
							if (outcome ==
							    LEFT_KEY_LOWER) {
								/* the next one is higher */
								*no_key_match =
								    -1;
							} else if (outcome ==
								   KEYS_MATCH) {
								/* since we've done the
								 * extract and compare might as well see if
								 * we lucked into a match
								 */
								*no_key_match =
								    0;
								*key_matched =
								    -1;
								*slot_selected =
								    -1;
								*selected_slotidx
								    =
								    dtiptr->
								    dtstbl
								    [next_idx];
							} else {
								/* not on or just before the money */
								/* this key is higher than the middle */
								lowidx = mididx;
							}
						}	/* end nothing untoward */
					}
				} else {
					/* given name < mid key */
					prev_idx = mididx - 1;
					dbl_rc =
					    dTree_key_extract(dtiptr, prev_idx,
							      this_name,
							      this_name_len,
							      is_root, is_leaf,
							      inorecptr);
					/*
					 * note that we can proceed with (some)
					 * confidence since we never search a directory
					 * until after we've verified it's structure
					 */
					if (dbl_rc == FSCK_OK) {
						/*
						 * If this is a case insensitive search, we need to fold the
						 * extracted key to upper case before doing the comparison.
						 * The given key should already be in all upper case.
						 */
						dTree_key_to_upper(this_name,
								   &(ukey[0]
								     [0]),
								   (*this_name_len));
						dbl_rc =
						    dTree_key_compare
						    (given_name, given_name_len,
						     &(ukey[0][0]),
						     (*this_name_len),
						     &outcome);
						if (dbl_rc == FSCK_OK) {
							if (outcome ==
							    LEFT_KEY_HIGHER) {
								/* the prev one is lower */
								*no_key_match =
								    -1;
							} else if (outcome ==
								   KEYS_MATCH) {
								/* since we've done the
								 * extract and compare might as well see if
								 * we stumbled onto a match
								 */
								*no_key_match =
								    0;
								*key_matched =
								    -1;
								*slot_selected =
								    -1;
								*selected_slotidx
								    =
								    dtiptr->
								    dtstbl
								    [prev_idx];
							} else {
								/* not on or just after the money */
								/* this key is lower than the middle */
								highidx =
								    mididx;
							}
						}
					}
				}
			}
		}
	}
	return (dbl_rc);
}

/*****************************************************************************
 * NAME: dTree_key_compare
 *
 * FUNCTION: Compare the two strings which are given.
 *
 * PARAMETERS:
 *      left_key       - input - pointer to the first in a pair of keys to
 *                               compare
 *      left_key_len   - input - number of UniChars in left_key
 *      right_key      - input - pointer to the second in a pair of keys to
 *                               compare
 *      right_key_len  - input - number of UniChars in right_key
 *      keys_relation  - input - pointer to a variable in which to return
 *                               { LEFT_KEY_LOWER | KEYS_MATCH | LEFT_KEY_HIGHER }
 *
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int dTree_key_compare(UniChar * left_key,
		      uint8_t left_key_num_unichars,
		      UniChar * right_key,
		      uint8_t right_key_num_unichars, int *keys_relation)
{
	int dkc_rc = FSCK_OK;
	int outcome;
	int left_key_len, right_key_len;

	left_key_len = left_key_num_unichars;
	right_key_len = right_key_num_unichars;

	if (right_key_len < left_key_len) {
		/* right key is shorter */

		outcome =
		    UniStrncmp((void *) left_key, (void *) right_key,
			       right_key_len);
		if (outcome < 0) {
			/* right key is alphabetically greater */
			*keys_relation = LEFT_KEY_LOWER;
		} else {
			*keys_relation = LEFT_KEY_HIGHER;
		}

	} else if (right_key_len > left_key_len) {
		/* right key is longer */

		outcome =
		    UniStrncmp((void *) left_key, (void *) right_key,
			       left_key_len);
		if (outcome <= 0) {
			/* right key is alphabetically greater */
			*keys_relation = LEFT_KEY_LOWER;
		} else {
			*keys_relation = LEFT_KEY_HIGHER;
		}
	} else {
		/* keys same length */
		outcome =
		    UniStrncmp((void *) left_key, (void *) right_key,
			       left_key_len);

		if (outcome < 0)
			*keys_relation = LEFT_KEY_LOWER;
		else if (outcome > 0)
			*keys_relation = LEFT_KEY_HIGHER;
		else
			*keys_relation = KEYS_MATCH;

	}

	return (dkc_rc);
}

/*****************************************************************************
 * NAME: dTree_key_compare_leaflvl
 *
 * FUNCTION: Compare the 2 given strings according to the rules for
 *           sibling entries in a leaf node.
 *
 * PARAMETERS:
 *      left_key       - input - pointer to the first in a pair of keys to
 *                               compare
 *      left_key_len   - input - number of UniChars in left_key
 *      right_key      - input - pointer to the second in a pair of keys to
 *                               compare
 *      right_key_len  - input - number of UniChars in right_key
 *      keys_ok        - input - pointer to a variable in which to return
 *                               !0 if the relation between left_key and
 *                                  right_key, on the dTree leaf level,
 *                                  is valid
 *                                0 if the relation between left_key and
 *                                  right_key, on the dTree leaf level,
 *                                  is not valid
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int dTree_key_compare_leaflvl(UniChar * left_key,
			      uint8_t left_key_len,
			      UniChar * right_key,
			      uint8_t right_key_len, int8_t * keys_ok)
{
	int dkcl_rc = FSCK_OK;
	int outcome;

	/* assume incorrect relation between the keys */
	*keys_ok = 0;

	if (sb_ptr->s_flag & JFS_OS2) {	/* case is preserved but ignored */
		dkcl_rc =
		    dTree_key_to_upper(left_key, &(ukey[0][0]),
				       (int32_t) left_key_len);
		if (dkcl_rc == FSCK_OK) {
			dkcl_rc = dTree_key_to_upper(right_key, &(ukey[1][0]),
						     (int32_t) right_key_len);
			if (dkcl_rc == FSCK_OK) {
				dkcl_rc =
				    dTree_key_compare(&(ukey[0][0]),
						      left_key_len,
						      &(ukey[1][0]),
						      right_key_len, &outcome);
			}
		}

	} else {
		/* case sensitive */
		dkcl_rc = dTree_key_compare(left_key, left_key_len,
					    right_key, right_key_len, &outcome);
	}

	if (dkcl_rc == FSCK_OK) {
		if ((outcome == KEYS_MATCH) || (outcome == LEFT_KEY_LOWER)) {
			/* right key greater */
			*keys_ok = -1;
		}
	}
	return (dkcl_rc);
}

/*****************************************************************************
 * NAME: dTree_key_compare_prntchld
 *
 * FUNCTION: Compare the two given strings according to the rules for
 *           a parent entry key and the first (sorted) entry key in the
 *           node described by the parent entry.
 *
 * PARAMETERS:
 *      dtiptr          - input - pointer to an fsck record describing the
 *                                directory tree
 *      parent_key      - input - pointer to the key extracted from the
 *                                parent node entry
 *      parent_key_len  - input - number of UniChars in parent_key
 *      child_key       - input - pointer to the key extracted from the first
 *                                (sorted) entry in the child node described
 *                                by the entry from which parent_key was
 *                                taken
 *      child_key_len   - input - number of UniChars in child_key
 *      keys_ok         - input - pointer to a variable in which to return
 *                                !0 if the relation between parent_key and
 *                                   child_key, where child_key is the first
 *                                   key in the child node, is valid
 *                                 0 if the relation between parent_key and
 *                                   child_key, where child_key is the first
 *                                   key in the child node, is not valid
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int dTree_key_compare_prntchld(struct fsck_Dtree_info *dtiptr,
			       UniChar * parent_key,
			       uint8_t parent_key_len,
			       UniChar * child_key,
			       uint8_t child_key_len, int8_t * keys_ok)
{
	int dnfk_rc = FSCK_OK;
	int outcome;

	/* assume incorrect relation between the keys */
	*keys_ok = 0;

	if (dtiptr->leaf_level == dtiptr->this_Qel->node_level) {
		/*
		 * the child is a leaf so its key is mixed case
		 */
		dnfk_rc =
		    dTree_key_to_upper(child_key, &(ukey[0][0]),
				       (int32_t) child_key_len);
		if (dnfk_rc == FSCK_OK) {
			dnfk_rc = dTree_key_compare(parent_key, parent_key_len,
						    &(ukey[0][0]),
						    child_key_len, &outcome);
		}

	} else {
		/* the child is not a leaf */
		dnfk_rc = dTree_key_compare(parent_key, parent_key_len,
					    child_key, child_key_len, &outcome);
	}

	if ((dnfk_rc == FSCK_OK)
	    && ((outcome == KEYS_MATCH) || (outcome == LEFT_KEY_LOWER))) {
		/* parent is less than or equal to first child */
		*keys_ok = -1;
	}

	return (dnfk_rc);
}

/*****************************************************************************
 * NAME: dTree_key_compare_samelvl
 *
 * FUNCTION: Compare the 2 given strings according to the rules for
 *           sibling entries in an internal node.
 *
 * PARAMETERS:
 *      left_key       - input - pointer to the first in a pair of keys to
 *                               compare
 *      left_key_len   - input - number of UniChars in left_key
 *      right_key      - input - pointer to the second in a pair of keys to
 *                               compare
 *      right_key_len  - input - number of UniChars in right_key
 *      keys_ok        - input - pointer to a variable in which to return
 *                               !0 if the relation between left_key and
 *                                  right_key, on the same dTree level,
 *                                  is valid
 *                                0 if the relation between left_key and
 *                                  right_key, on the same dTree level,
 *                                  is not valid
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int dTree_key_compare_samelvl(UniChar * left_key,
			      uint8_t left_key_len,
			      UniChar * right_key,
			      uint8_t right_key_len, int8_t * keys_ok)
{
	int dkcs_rc = FSCK_OK;
	int outcome;

	/* assume incorrect relation between the keys */
	*keys_ok = 0;

	dkcs_rc =
	    dTree_key_compare(left_key, left_key_len, right_key, right_key_len,
			      &outcome);

	if (dkcs_rc == FSCK_OK) {
		if (outcome == LEFT_KEY_LOWER) {
			/* right key greater */
			*keys_ok = -1;
		}
	}
	return (dkcs_rc);
}

/**************************************************************************
 * NAME:  dTree_key_extract
 *
 * FUNCTION: Extract the specified directory entry (either internal or
 *           leaf) key and concatenate it's segments (if more than one).
 *           Assume the directory structure has already been validated.
 *
 * PARAMETERS:
 *      dtiptr       - input - pointer to an fsck record describing the
 *                             directory tree
 *      start_dtidx  - input - index of the entry in the directory node's
 *                             sorted entry index table containing the
 *                             slot number of the (first segment of the)
 *                             key to extract
 *      key_space    - input - pointer to a buffer in which to return
 *                             the directory key (a complete filename if
 *                             the node is a leaf) extracted
 *      key_length   - input - pointer to a variable in which to return
 *                             the length of the directory key being returned
 *                             in *key_space
 *      is_root      - input - !0 => the specified node is a B+ Tree root
 *                              0 => the specified node is not a B+ Tree root
 *      is_leaf      - input - !0 => the specified node is a leaf node
 *                              0 => the specified node is not a leaf node
 *      inorecptr    - input - pointer to an fsck inode record describing
 *                             the inode
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int dTree_key_extract(struct fsck_Dtree_info *dtiptr,
		      int start_dtidx,
		      UniChar * key_space,
		      uint32_t * key_length,
		      int8_t is_root,
		      int8_t is_leaf, struct fsck_inode_record *inorecptr)
{
	int dek_rc = FSCK_OK;
	int this_slotidx, next_slotidx;
	struct idtentry *ientry_ptr;
	struct ldtentry *lentry_ptr;
	UniChar *name_seg;
	int seg_length, UniChars_left = 0, seg_max;
	struct dtslot *contin_entry_ptr;

	this_slotidx = dtiptr->dtstbl[start_dtidx];

	if (is_leaf) {
		lentry_ptr = (struct ldtentry *) &(dtiptr->slots[this_slotidx]);
		name_seg = &(lentry_ptr->name[0]);
		*key_length = lentry_ptr->namlen;
		next_slotidx = lentry_ptr->next;
		if (sb_ptr->s_flag & JFS_DIR_INDEX)
			seg_max = DTLHDRDATALEN;
		else
			seg_max = DTLHDRDATALEN_LEGACY;
	} else {
		ientry_ptr = (struct idtentry *) &(dtiptr->slots[this_slotidx]);
		name_seg = &(ientry_ptr->name[0]);
		*key_length = ientry_ptr->namlen;
		next_slotidx = ientry_ptr->next;
		seg_max = DTIHDRDATALEN;
	}

	if ((*key_length) > JFS_NAME_MAX) {
		/* name too long */
		inorecptr->ignore_alloc_blks = 1;
	} else {
		UniChars_left = *key_length;
		*key_length = 0;
	}

	while ((dek_rc == FSCK_OK) && (this_slotidx != -1)
	       && (!inorecptr->ignore_alloc_blks)) {

		if ((this_slotidx > dtiptr->max_slotidx)
		    || (this_slotidx < DTENTRYSTART)) {
			/* idx out of bounds */
			inorecptr->ignore_alloc_blks = 1;
		} else {
			/* else no reason to think there's a problem */
			if (UniChars_left > seg_max) {
				/* this isn't the last */
				seg_length = seg_max;
				UniChars_left = UniChars_left - seg_max;
			} else {
				seg_length = UniChars_left;
				UniChars_left = 0;
			}
			/* copy this section of the name into the buffer */
			memcpy((void *) &(key_space[*key_length]),
			       (void *) &(name_seg[0]),
			       (seg_length * sizeof (UniChar))
			    );
			*key_length += seg_length;

			this_slotidx = next_slotidx;
			if (next_slotidx != -1) {
				/* it's not the end of chain marker */
				contin_entry_ptr =
				    &(dtiptr->slots[this_slotidx]);
				name_seg = &(contin_entry_ptr->name[0]);
				seg_length = contin_entry_ptr->cnt;
				next_slotidx = contin_entry_ptr->next;
				seg_max = DTSLOTDATALEN;
			}
		}
	}

	return (dek_rc);
}

/*****************************************************************************
 * NAME:  dTree_key_extract_cautiously
 *
 * FUNCTION: Extract the specified directory entry (either internal or
 *           leaf) key and concatenate it's segments (if more than one).
 *           Do not assume the directory structure has been validated.
 *
 * PARAMETERS:
 *      dtiptr       - input - pointer to an fsck record describing the
 *                             directory tree
 *      start_dtidx  - input - index of the entry in the directory node's
 *                             sorted entry index table containing the
 *                             slot number of the (first segment of the)
 *                             key to extract
 *      key_space    - input - pointer to a buffer in which to return
 *                             the directory key (a complete filename if
 *                             the node is a leaf) extracted
 *      key_length   - input - pointer to a variable in which to return
 *                             the length of the directory key being returned
 *                             in *key_space
 *      is_root      - input - !0 => the specified node is a B+ Tree root
 *                              0 => the specified node is not a B+ Tree root
 *      is_leaf      - input - !0 => the specified node is a leaf node
 *                              0 => the specified node is not a leaf node
 *      inorecptr    - input - pointer to an fsck inode record describing
 *                             the inode
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int dTree_key_extract_cautiously(struct fsck_Dtree_info *dtiptr,
				 int start_dtidx,
				 UniChar * key_space,
				 uint32_t * key_length,
				 int8_t is_root,
				 int8_t is_leaf,
				 struct fsck_inode_record *inorecptr)
{
	int deck_rc = FSCK_OK;
	int8_t temp_slot_map[DTPAGEMAXSLOT];
	int this_slotidx, next_slotidx, this_charidx;
	struct idtentry *ientry_ptr;
	struct ldtentry *lentry_ptr;
	UniChar *name_seg;
	int seg_length, UniChars_left = 0, seg_max;
	struct dtslot *contin_entry_ptr;

	memset((void *) &(temp_slot_map[0]), 0, DTPAGEMAXSLOT);

	this_slotidx = dtiptr->dtstbl[start_dtidx];

	if (is_leaf) {
		lentry_ptr = (struct ldtentry *) &(dtiptr->slots[this_slotidx]);
		name_seg = &(lentry_ptr->name[0]);
		*key_length = lentry_ptr->namlen;
		next_slotidx = lentry_ptr->next;
		if (sb_ptr->s_flag & JFS_DIR_INDEX)
			seg_max = DTLHDRDATALEN;
		else
			seg_max = DTLHDRDATALEN_LEGACY;
	} else {
		ientry_ptr = (struct idtentry *) &(dtiptr->slots[this_slotidx]);
		name_seg = &(ientry_ptr->name[0]);
		*key_length = ientry_ptr->namlen;
		next_slotidx = ientry_ptr->next;
		seg_max = DTIHDRDATALEN;
	}

	if ((*key_length) > JFS_NAME_MAX) {
		/* name too long */
		inorecptr->ignore_alloc_blks = 1;
	} else {
		UniChars_left = *key_length;
		*key_length = 0;
	}

	while ((deck_rc == FSCK_OK) && (this_slotidx != -1)
	       && (!inorecptr->ignore_alloc_blks)) {

		if ((this_slotidx > dtiptr->max_slotidx)
		    || (this_slotidx < DTENTRYSTART)
		    || (temp_slot_map[this_slotidx] != 0)) {
			/* index is out of bounds OR index was seen earlier this key chain */
			/* bad chain */
			inorecptr->ignore_alloc_blks = 1;
		} else {
			/* else no reason to think there's a problem */
			/* mark the slot used */
			temp_slot_map[this_slotidx] = -1;
			if (UniChars_left > seg_max) {
				/* this isn't the last */
				seg_length = seg_max;
				UniChars_left = UniChars_left - seg_max;
			} else {
				seg_length = UniChars_left;
				UniChars_left = 0;
			}
			/* copy this section of the name into the buffer */
			memcpy((void *) &(key_space[*key_length]),
			       (void *) &(name_seg[0]),
			       (seg_length * sizeof (UniChar)));
			*key_length += seg_length;

			this_slotidx = next_slotidx;
			if (next_slotidx != -1) {
				/* it's not the end of chain marker */
				if (UniChars_left == 0)
					/* too many segments */
					inorecptr->ignore_alloc_blks = 1;
				else {
					contin_entry_ptr =
					    &(dtiptr->slots[this_slotidx]);
					name_seg = &(contin_entry_ptr->name[0]);
					seg_length = contin_entry_ptr->cnt;
					next_slotidx = contin_entry_ptr->next;
					seg_max = DTSLOTDATALEN;
				}
			}
		}
	}

	/*
	 * check for a null character embedded in the name
	 */
	this_charidx = 0;
	while ((deck_rc == FSCK_OK) && (!inorecptr->ignore_alloc_blks)
	       && (this_charidx < *key_length)) {
		if (((unsigned short *) key_space)[this_charidx] ==
		    (unsigned short) NULL) {
			inorecptr->ignore_alloc_blks = 1;
		} else {
			this_charidx++;
		}
	}

	return (deck_rc);
}

/*****************************************************************************
 * NAME:  dTree_key_extract_record
 *
 * FUNCTION: Extract the specified directory entry (either internal or
 *           leaf) key and concatenate it's segments (if more than one).
 *           Directory structure validation is in progress.
 *
 * PARAMETERS:
 *      dtiptr       - input - pointer to an fsck record describing the
 *                             directory tree
 *      start_dtidx  - input - index of the entry in the directory node's
 *                             sorted entry index table containing the
 *                             slot number of the (first segment of the)
 *                             key to extract
 *      key_space    - input - pointer to a buffer in which to return
 *                             the directory key (a complete filename if
 *                             the node is a leaf) extracted
 *      key_length   - input - pointer to a variable in which to return
 *                             the length of the directory key being returned
 *                             in *key_space
 *      is_root      - input - !0 => the specified node is a B+ Tree root
 *                              0 => the specified node is not a B+ Tree root
 *      is_leaf      - input - !0 => the specified node is a leaf node
 *                              0 => the specified node is not a leaf node
 *      inorecptr    - input - pointer to an fsck inode record describing
 *                             the inode
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int dTree_key_extract_record(struct fsck_Dtree_info *dtiptr,
			     int start_dtidx,
			     UniChar * key_space,
			     uint32_t * key_length,
			     int8_t is_root,
			     int8_t is_leaf,
			     struct fsck_inode_record *inorecptr)
{
	int derk_rc = FSCK_OK;
	int this_slotidx, next_slotidx, this_charidx;
	struct idtentry *ientry_ptr;
	struct ldtentry *lentry_ptr;
	UniChar *name_seg;
	int seg_length, UniChars_left = 0, seg_max;
	struct dtslot *contin_entry_ptr;

	this_slotidx = dtiptr->dtstbl[start_dtidx];

	if (is_leaf) {
		lentry_ptr = (struct ldtentry *) &(dtiptr->slots[this_slotidx]);
		name_seg = &(lentry_ptr->name[0]);
		*key_length = lentry_ptr->namlen;
		next_slotidx = lentry_ptr->next;
		if (sb_ptr->s_flag & JFS_DIR_INDEX)
			seg_max = DTLHDRDATALEN;
		else
			seg_max = DTLHDRDATALEN_LEGACY;
	} else {
		ientry_ptr = (struct idtentry *) &(dtiptr->slots[this_slotidx]);
		name_seg = &(ientry_ptr->name[0]);
		*key_length = ientry_ptr->namlen;
		next_slotidx = ientry_ptr->next;
		seg_max = DTIHDRDATALEN;
	}

	if ((*key_length) > JFS_NAME_MAX) {
		/* name too long */
		inorecptr->ignore_alloc_blks = 1;
	} else {
		UniChars_left = *key_length;
		*key_length = 0;
	}

	while ((derk_rc == FSCK_OK) && (this_slotidx != -1)
	       && (!inorecptr->ignore_alloc_blks)) {

		if ((this_slotidx > dtiptr->max_slotidx)
		    || (this_slotidx < DTENTRYSTART)
		    || (dtiptr->slot_map[this_slotidx] != 0)) {
			/* index is out of bounds OR index was seen in a previous key chain */
			/* bad chain */
			inorecptr->ignore_alloc_blks = 1;
		} else {
			/* else no reason to think there's a problem */
			/* mark the slot used */
			dtiptr->slot_map[this_slotidx] = -1;
			if (UniChars_left > seg_max) {
				/* this isn't the last */
				seg_length = seg_max;
				UniChars_left = UniChars_left - seg_max;
			} else {
				seg_length = UniChars_left;
				UniChars_left = 0;
			}
			/* copy this section of the name into the buffer */
			memcpy((void *) &(key_space[*key_length]),
			       (void *) &(name_seg[0]),
			       (seg_length * sizeof (UniChar)));
			*key_length += seg_length;

			this_slotidx = next_slotidx;
			if (next_slotidx != -1) {
				/* it's not the end of chain marker */
				if (UniChars_left == 0)
					/* too many segments */
					inorecptr->ignore_alloc_blks = 1;
				else {
					contin_entry_ptr =
					    &(dtiptr->slots[this_slotidx]);
					name_seg = &(contin_entry_ptr->name[0]);
					seg_length = contin_entry_ptr->cnt;
					next_slotidx = contin_entry_ptr->next;
					seg_max = DTSLOTDATALEN;
				}
			}
		}
	}

	/*
	 * check for a null character embedded in the name
	 */
	this_charidx = 0;
	while ((derk_rc == FSCK_OK) && (!inorecptr->ignore_alloc_blks)
	       && (this_charidx < *key_length)) {
		if (((unsigned short *) key_space)[this_charidx] ==
		    (unsigned short) NULL) {
			inorecptr->ignore_alloc_blks = 1;
		} else {
			this_charidx++;
		}
	}

	return (derk_rc);
}

/*****************************************************************************
 * NAME: dTree_key_to_upper
 *
 * FUNCTION: Fold the given mixed-case string to upper case.
 *
 * PARAMETERS:
 *      given_name     - input - pointer to the name which is to be folded to
 *                               upper case
 *      name_in_upper  - input - pointer to a buffer in which to return the
 *                               string which results from folding given_name
 *                               to upper case
 *      name_len       - input - the number of UniChars in given_name
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int dTree_key_to_upper(UniChar * given_name, UniChar * name_in_upper,
		       int32_t name_len)
{
	int dktu_rc = FSCK_OK;
	int charidx;

	for (charidx = 0; (charidx < name_len); charidx++) {

		if (sb_ptr->s_flag & JFS_OS2)
			/* only upper case if case-insensitive support is wanted */
			name_in_upper[charidx] =
			    UniToupper(given_name[charidx]);
		else
			name_in_upper[charidx] = given_name[charidx];
	}
	return (dktu_rc);
}

/*****************************************************************************
 * NAME: dTree_node_first_key
 *
 * FUNCTION:  Assists dTree_processing.
 *
 * PARAMETERS:
 *      dtiptr          - input - pointer to an fsck record describing the
 *                                directory tree
 *      inorecptr       - input - pointer to an fsck inode record describing
 *                                the inode
 *      msg_info_ptr    - input - pointer to data needed to issue messages
 *                                about the inode
 *      desired_action  - input - { FSCK_RECORD | FSCK_RECORD_DUPCHECK |
 *                                  FSCK_UNRECORD | FSCK_QUERY }
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int dTree_node_first_key(struct fsck_Dtree_info *dtiptr,
			 struct fsck_inode_record *inorecptr,
			 struct fsck_ino_msg_info *msg_info_ptr,
			 int desired_action)
{
	int dnfk_rc = FSCK_OK;
	int8_t keys_ok;

	dnfk_rc = dTree_key_compare_prntchld(dtiptr,
					     &(dtiptr->this_Qel->node_key[0]),
					     dtiptr->this_Qel->node_key_len,
					     &(dtiptr->
					       key[dtiptr->this_key_idx][0]),
					     dtiptr->key_len[dtiptr->
							     this_key_idx],
					     &keys_ok);
	if (dnfk_rc == FSCK_OK) {
		if (!keys_ok) {
			/* invalid key in first slot */
			inorecptr->ignore_alloc_blks = 1;
			if (desired_action == FSCK_RECORD_DUPCHECK) {
				/* not reported yet */
				fsck_send_msg(fsck_BADKEYS,
					      fsck_ref_msg(msg_info_ptr->msg_inotyp),
					      fsck_ref_msg(msg_info_ptr->msg_inopfx),
					      msg_info_ptr->msg_inonum,
					      1);
			}
		} else {
			/* 1st slot may be ok */
			if (dtiptr->last_level == dtiptr->this_Qel->node_level) {
				/*
				 * the node is not 1st in its level
				 */
				if (dtiptr->leaf_level ==
				    dtiptr->this_Qel->node_level) {
					/* it's a leaf */
					dnfk_rc =
					    dTree_key_compare_leaflvl(&(dtiptr->
									key
									[dtiptr->
									 last_key_idx]
									[0]),
								      dtiptr->
								      key_len
								      [dtiptr->
								       last_key_idx],
								      &(dtiptr->
									key
									[dtiptr->
									 this_key_idx]
									[0]),
								      dtiptr->
								      key_len
								      [dtiptr->
								       this_key_idx],
								      &keys_ok);
				} else {
					/* not a leaf */
					dnfk_rc =
					    dTree_key_compare_samelvl(&(dtiptr->
									key
									[dtiptr->
									 last_key_idx]
									[0]),
								      dtiptr->
								      key_len
								      [dtiptr->
								       last_key_idx],
								      &(dtiptr->
									key
									[dtiptr->
									 this_key_idx]
									[0]),
								      dtiptr->
								      key_len
								      [dtiptr->
								       this_key_idx],
								      &keys_ok);
				}
				if (!keys_ok) {
					/* keys out of sort order! */
					inorecptr->ignore_alloc_blks = 1;
					if (desired_action ==
					    FSCK_RECORD_DUPCHECK) {
						/* not reported */
						fsck_send_msg(fsck_BADKEYS,
							      fsck_ref_msg(msg_info_ptr->msg_inotyp),
							      fsck_ref_msg(msg_info_ptr->msg_inopfx),
							      msg_info_ptr->msg_inonum,
							      2);
					}
				}
			}
		}
	}
	return (dnfk_rc);
}

/*****************************************************************************
 * NAME: dTree_node_first_in_level
 *
 * FUNCTION:  Assists dTree_processing.
 *
 * PARAMETERS:
 *      dtiptr          - input - pointer to an fsck record describing the
 *                                directory tree
 *      inorecptr       - input - pointer to an fsck inode record describing
 *                                the inode
 *      msg_info_ptr    - input - pointer to data needed to issue messages
 *                                about the inode
 *      desired_action  - input - { FSCK_RECORD | FSCK_RECORD_DUPCHECK |
 *                                  FSCK_UNRECORD | FSCK_QUERY }
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int dTree_node_first_in_level(struct fsck_Dtree_info *dtiptr,
			      struct fsck_inode_record *inorecptr,
			      struct fsck_ino_msg_info *msg_info_ptr,
			      int desired_action)
{
	int dnfil_rc = FSCK_OK;

	if (dtiptr->dtp_ptr->header.prev != 0) {
		/* bad back pointer! */
		inorecptr->ignore_alloc_blks = 1;
		if (desired_action == FSCK_RECORD_DUPCHECK) {
			/* not reported */
			fsck_send_msg(fsck_BADBSBLCHN,
				      fsck_ref_msg(msg_info_ptr->msg_inotyp),
				      fsck_ref_msg(msg_info_ptr->msg_inopfx),
				      msg_info_ptr->msg_inonum);
		}
	}
	return (dnfil_rc);
}

/*****************************************************************************
 * NAME: dTree_node_last_in_level
 *
 * FUNCTION:  Assists dTree_processing.
 *
 * PARAMETERS:
 *      dtiptr          - input - pointer to an fsck record describing the
 *                                directory tree
 *      inorecptr       - input - pointer to an fsck inode record describing
 *                                the inode
 *      msg_info_ptr    - input - pointer to data needed to issue messages
 *                                about the inode
 *      desired_action  - input - { FSCK_RECORD | FSCK_RECORD_DUPCHECK |
 *                                  FSCK_UNRECORD | FSCK_QUERY }
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int dTree_node_last_in_level(struct fsck_Dtree_info *dtiptr,
			     struct fsck_inode_record *inorecptr,
			     struct fsck_ino_msg_info *msg_info_ptr,
			     int desired_action)
{
	int dnlil_rc = FSCK_OK;

	if (dtiptr->dtp_ptr->header.next != 0) {
		/* bad forward pointer! */
		inorecptr->ignore_alloc_blks = 1;
		if (desired_action == FSCK_RECORD_DUPCHECK) {
			/* not reported */
			fsck_send_msg(fsck_BADFSBLCHN,
				      fsck_ref_msg(msg_info_ptr->msg_inotyp),
				      fsck_ref_msg(msg_info_ptr->msg_inopfx),
				      msg_info_ptr->msg_inonum);
		}
	}
	return (dnlil_rc);
}

/*****************************************************************************
 * NAME: dTree_node_not_first_in_level
 *
 * FUNCTION:  Assists dTree_processing.
 *
 * PARAMETERS:
 *      dtiptr          - input - pointer to an fsck record describing the
 *                                directory tree
 *      inorecptr       - input - pointer to an fsck inode record describing
 *                                the inode
 *      msg_info_ptr    - input - pointer to data needed to issue messages
 *                                about the inode
 *      desired_action  - input - { FSCK_RECORD | FSCK_RECORD_DUPCHECK |
 *                                  FSCK_UNRECORD | FSCK_QUERY }
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int dTree_node_not_first_in_level(struct fsck_Dtree_info *dtiptr,
				  struct fsck_inode_record *inorecptr,
				  struct fsck_ino_msg_info *msg_info_ptr,
				  int desired_action)
{
	int dnnfil_rc = FSCK_OK;

	if (dtiptr->dtp_ptr->header.prev != dtiptr->last_node_addr) {
		/* bad back pointer! */
		inorecptr->ignore_alloc_blks = 1;
		if (desired_action == FSCK_RECORD_DUPCHECK) {
			/* not reported */
			fsck_send_msg(fsck_BADBSBLCHN,
				      fsck_ref_msg(msg_info_ptr->msg_inotyp),
				      fsck_ref_msg(msg_info_ptr->msg_inopfx),
				      msg_info_ptr->msg_inonum);
		}
	}
	return (dnnfil_rc);
}

/*****************************************************************************
 * NAME: dTree_node_not_last_in_level
 *
 * FUNCTION:  Assists dTree_processing.
 *
 * PARAMETERS:
 *      dtiptr          - input - pointer to an fsck record describing the
 *                                directory tree
 *      inorecptr       - input - pointer to an fsck inode record describing
 *                                the inode
 *      msg_info_ptr    - input - pointer to data needed to issue messages
 *                                about the inode
 *      desired_action  - input - { FSCK_RECORD | FSCK_RECORD_DUPCHECK |
 *                                  FSCK_UNRECORD | FSCK_QUERY }
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int dTree_node_not_last_in_level(struct fsck_Dtree_info *dtiptr,
				 struct fsck_inode_record *inorecptr,
				 struct fsck_ino_msg_info *msg_info_ptr,
				 int desired_action)
{
	int dnnlil_rc = FSCK_OK;
	int8_t is_leaf = 0;
	int8_t is_root = 0;	/* it can't be the root */

	if (dtiptr->dtp_ptr->header.next != dtiptr->next_Qel->node_addr) {
		/* bad forward pointer! */
		inorecptr->ignore_alloc_blks = 1;
		if (desired_action == FSCK_RECORD_DUPCHECK) {
			/* not reported */
			fsck_send_msg(fsck_BADFSBLCHN,
				      fsck_ref_msg(msg_info_ptr->msg_inotyp),
				      fsck_ref_msg(msg_info_ptr->msg_inopfx),
				      msg_info_ptr->msg_inonum);
		}
	} else {
		/* forward pointer looks fine */
		if (dtiptr->leaf_level == dtiptr->this_Qel->node_level) {
			/* it's a leaf */
			is_leaf = -1;
		}
		dnnlil_rc =
		    dTree_key_extract_cautiously(dtiptr, dtiptr->last_dtidx,
						 &(dtiptr->
						   key[dtiptr->
						       last_key_idx][0]),
						 &(dtiptr->
						   key_len[dtiptr->
							   last_key_idx]),
						 is_root, is_leaf, inorecptr);
		/*
		 * get the last key in the directory. Since this is the first
		 * time we've extracted it, we need to guard against a loop
		 * even though we aren't recording it now.
		 */
		if ((dnnlil_rc == FSCK_OK) && (inorecptr->ignore_alloc_blks)) {
			/* no mishaps but the tree is bad */
			if (desired_action == FSCK_RECORD_DUPCHECK) {
				/* not reported yet */
				fsck_send_msg(fsck_BADKEYS,
					      fsck_ref_msg(msg_info_ptr->msg_inotyp),
					      fsck_ref_msg(msg_info_ptr->msg_inopfx),
					      msg_info_ptr->msg_inonum,
					      13);
			}
		}
	}

	return (dnnlil_rc);
}

/**************************************************************************
 * NAME:  dTree_node_size_check
 *
 * FUNCTION: Validate the size of the given dTree node.
 *
 * PARAMETERS:
 *      dtiptr          - input - pointer to an fsck record describing the
 *                                directory tree
 *      is_leaf         - input - !0 => the node is a leaf node
 *                                 0 => the node is an internal node
 *      first_in_level  - input - !0 => the node is the leftmost in its level
 *                                 0 => the node is not leftmost in its level
 *      last_in_level   - input - !0 => the node is the rightmost in its level
 *                                 0 => the node is not rightmost in its level
 *      inorecptr       - input - pointer to an fsck inode record describing
 *                                the inode
 *      msg_info_ptr    - input - pointer to data needed to issue messages
 *                                about the inode
 *      desired_action  - input - { FSCK_RECORD | FSCK_RECORD_DUPCHECK |
 *                                  FSCK_UNRECORD | FSCK_QUERY }
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int dTree_node_size_check(struct fsck_Dtree_info *dtiptr,
			  int8_t is_leaf,
			  int8_t first_in_level,
			  int8_t last_in_level,
			  struct fsck_inode_record *inorecptr,
			  struct fsck_ino_msg_info *msg_info_ptr,
			  int desired_action)
{
	int dnsc_rc = FSCK_OK;
	uint32_t ext_length;
	uint32_t acceptable_size;
	uint8_t total_slots;

	ext_length = dtiptr->this_Qel->node_size * sb_ptr->s_bsize;
	total_slots = dtiptr->dtp_ptr->header.maxslot;
	if (ext_length == BYTESPERPAGE) {
		/* it is exactly 1 page long */
		if (total_slots != (BYTESPERPAGE / DTSLOTSIZE)) {
			/* but max slots doesn't work out right */
			inorecptr->ignore_alloc_blks = 1;
			if (desired_action == FSCK_RECORD_DUPCHECK) {
				/* not reported */
				fsck_send_msg(fsck_BADDINONODESIZ,
					      fsck_ref_msg(msg_info_ptr->msg_inopfx),
					      msg_info_ptr->msg_inonum);
			}
		}
	} else {
		/* it isn't exactly 1 page long */
		/*
		 * the only valid directory node which can have a length
		 * different from 1 page is the node which is the only
		 * non-root node in the tree
		 */
		if ((!is_leaf) || (!(dtiptr->this_Qel->node_level == 1)) ||
		    (!first_in_level) || (!last_in_level)) {
			/* this node does not qualify */
			inorecptr->ignore_alloc_blks = 1;
			if (desired_action == FSCK_RECORD_DUPCHECK) {
				/* not reported */
				fsck_send_msg(fsck_BADDINOODDNODESIZ,
					      fsck_ref_msg(msg_info_ptr->msg_inopfx),
					      msg_info_ptr->msg_inonum);
			}
		} else {
			/* it might be ok */
			/* start at fs blocksize */
			acceptable_size = sb_ptr->s_bsize;
			while ((acceptable_size < BYTESPERPAGE)
			       && (ext_length != acceptable_size)) {
				/* double it */
				acceptable_size = acceptable_size << 1;
			}
			if (ext_length != acceptable_size) {
				/* invalid size */
				inorecptr->ignore_alloc_blks = 1;
				if (desired_action == FSCK_RECORD_DUPCHECK) {
					/* not reported */
					fsck_send_msg(fsck_BADDINOODDNODESIZ,
						      fsck_ref_msg(msg_info_ptr->msg_inopfx),
						      msg_info_ptr->msg_inonum);
				}
			} else {
				/* the size is ok */
				if (total_slots != (ext_length / DTSLOTSIZE)) {
					/* but max slots doesn't work out right */
					inorecptr->ignore_alloc_blks = 1;
					if (desired_action ==
					    FSCK_RECORD_DUPCHECK) {
						/* not reported */
						fsck_send_msg
						    (fsck_BADDINONODESIZ,
						     fsck_ref_msg(msg_info_ptr->msg_inopfx),
						     msg_info_ptr->msg_inonum);
					}
				}
			}
		}
	}

	return (dnsc_rc);
}

/*****************************************************************************
 * NAME: dTree_process_internal_slots
 *
 * FUNCTION: Perform the specified action on the slots in the specified
 *           dTree internal node.
 *
 * PARAMETERS:
 *      dtiptr          - input - pointer to an fsck record describing the
 *                                directory tree
 *      inorecptr       - input - pointer to an fsck inode record describing
 *                                the inode
 *      msg_info_ptr    - input - pointer to data needed to issue messages
 *                                about the inode
 *      desired_action  - input - { FSCK_RECORD | FSCK_RECORD_DUPCHECK |
 *                                  FSCK_UNRECORD | FSCK_QUERY }
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int dTree_process_internal_slots(struct fsck_Dtree_info *dtiptr,
				 struct fsck_inode_record *inorecptr,
				 struct fsck_ino_msg_info *msg_info_ptr,
				 int desired_action)
{
	int dpis_rc = FSCK_OK;
	int8_t this_key, last_key;
	int dtidx;
	struct dtreeQelem *new_Qelptr;
	uint32_t ext_length, adjusted_length;
	int64_t ext_addr;
	int8_t ext_ok, key_ok;
	int8_t first_entry = -1;
	int8_t is_EA = 0;
	int8_t is_ACL = 0;
	int8_t is_leaf = 0;
	int8_t is_root = 0;
	struct idtentry *idtptr;

	if (dtiptr->this_Qel->node_level == 0) {
		is_root = -1;
	}
	this_key = 0;
	last_key = 1;
	key_len[this_key] = 0;
	key_len[last_key] = 0;
	for (dtidx = 0;
	     ((dpis_rc == FSCK_OK) &&
	      (!inorecptr->ignore_alloc_blks) && (dtidx <= dtiptr->last_dtidx));
	     dtidx++) {

		dpis_rc = dTree_key_extract_record(dtiptr, dtidx,
						   &(key[this_key][0]),
						   &(key_len[this_key]),
						   is_root, is_leaf, inorecptr);
		if (dpis_rc == FSCK_OK) {
			if (inorecptr->ignore_alloc_blks) {
				/* but the tree is bad */
				if (desired_action == FSCK_RECORD_DUPCHECK) {
					/* not reported yet */
					fsck_send_msg(fsck_BADKEYS,
						      fsck_ref_msg(msg_info_ptr->msg_inotyp),
						      fsck_ref_msg(msg_info_ptr->msg_inopfx),
						      msg_info_ptr->msg_inonum,
						      14);
				}
			} else {
				/* got the key value */
				/*
				 * the key for the first entry is verified elsewhere
				 */
				if (first_entry) {
					first_entry = 0;
				} else {
					/* it's not the first entry on the page */
					dpis_rc =
					    dTree_key_compare_samelvl(&
								      (key
								       [last_key]
								       [0]),
key_len[last_key], &(key[this_key]
		     [0]), key_len[this_key], &key_ok);
					if (dpis_rc == FSCK_OK) {
						if (!key_ok) {
							/* but the key is bad */
							inorecptr->
							    ignore_alloc_blks =
							    1;
							if (desired_action ==
							    FSCK_RECORD_DUPCHECK)
							{
								/* not reported yet */
								fsck_send_msg
								    (fsck_BADKEYS,
								     fsck_ref_msg(msg_info_ptr->msg_inotyp),
								     fsck_ref_msg(msg_info_ptr->msg_inopfx),
								     msg_info_ptr->msg_inonum,
								     3);
							}
						}
					}
				}
			}
		}
		if ((dpis_rc == FSCK_OK) && (!inorecptr->ignore_alloc_blks)) {
			/* the key is good */
			idtptr =
			    (struct idtentry *) &(dtiptr->
						  slots[dtiptr->dtstbl[dtidx]]);
			ext_addr = addressPXD(&(idtptr->xd));
			ext_length = lengthPXD(&(idtptr->xd));
			dpis_rc =
			    process_extent(inorecptr, ext_length, ext_addr,
					   is_EA, is_ACL, msg_info_ptr,
					   &adjusted_length, &ext_ok,
					   desired_action);
			if ((dpis_rc == FSCK_OK) && (ext_ok)) {
				if ((desired_action == FSCK_RECORD) ||
				    (desired_action == FSCK_RECORD_DUPCHECK)) {
					agg_recptr->blocks_this_fset +=
					    adjusted_length;
					agg_recptr->this_inode.all_blks +=
					    adjusted_length;
					if (!(sb_ptr->s_flag & JFS_DIR_INDEX))
						agg_recptr->this_inode.data_size +=
						    adjusted_length * sb_ptr->s_bsize;
				} else if (desired_action == FSCK_UNRECORD) {
					agg_recptr->blocks_this_fset -=
					    adjusted_length;
					agg_recptr->this_inode.all_blks -=
					    adjusted_length;
				}
				dpis_rc = dtreeQ_get_elem(&new_Qelptr);
				if (dpis_rc == FSCK_OK) {
					/* got a Queue element */
					new_Qelptr->node_pxd = idtptr->xd;
					new_Qelptr->node_addr = ext_addr;
					new_Qelptr->node_level =
					    dtiptr->this_Qel->node_level + 1;
					new_Qelptr->node_size = ext_length;
					new_Qelptr->node_key_len =
					    key_len[this_key];
					memcpy((void *)
					       &(new_Qelptr->node_key[0]),
					       (void *) &(key[this_key][0]),
					       key_len[this_key] *
					       sizeof (UniChar));
					dpis_rc = dtreeQ_enqueue(new_Qelptr);
					if (this_key) {
						/* this_key == 1 */
						this_key = 0;
						last_key = 1;
					} else {
						/* this_key == 0 */
						this_key = 1;
						last_key = 0;
					}
					key_len[this_key] = 0;
				}
			}
		}
	}

	return (dpis_rc);
}

/*****************************************************************************
 * NAME: dTree_process_leaf_slots
 *
 * FUNCTION: Perform the specified action on the slots in the specified
 *           dTree leaf node.
 *
 * PARAMETERS:
 *      dtiptr          - input - pointer to an fsck record describing the
 *                                directory tree
 *      inorecptr       - input - pointer to an fsck inode record describing
 *                                the inode
 *      inoptr          - input - the dinode of the directory inode
 *      msg_info_ptr    - input - pointer to data needed to issue messages
 *                                about the inode
 *      desired_action  - input - { FSCK_RECORD | FSCK_RECORD_DUPCHECK |
 *                                  FSCK_UNRECORD | FSCK_QUERY }
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int dTree_process_leaf_slots(struct fsck_Dtree_info *dtiptr,
			     struct fsck_inode_record *inorecptr,
			     struct dinode *inoptr,
			     struct fsck_ino_msg_info *msg_info_ptr,
			     int desired_action)
{
	int dpls_rc = FSCK_OK;
	int8_t this_key, last_key;
	int dtidx;
	int8_t key_ok;
	int8_t is_leaf = -1;
	int8_t is_root = 0;
	int8_t first_entry = -1;
	struct ldtentry *ldtptr;
	uint32_t child_inoidx;
	struct fsck_inode_record *child_inorecptr;
	struct fsck_inode_ext_record *this_ext;
	int is_aggregate = 0;
	int alloc_ifnull = -1;
	int NumChars = 0;
	char message_parm[MAXPARMLEN];
	int inonum = inoptr->di_number;

	if (dtiptr->this_Qel->node_level == 0) {
		is_root = -1;
	}
	this_key = 0;
	last_key = 1;
	key_len[this_key] = 0;
	key_len[last_key] = 0;
	for (dtidx = 0; (dtidx <= dtiptr->last_dtidx); dtidx++) {
		dpls_rc = dTree_key_extract_record(dtiptr, dtidx,
						   &(key[this_key][0]),
						   &(key_len[this_key]),
						   is_root, is_leaf, inorecptr);
		if (dpls_rc != FSCK_OK)
			break;

		if (inorecptr->ignore_alloc_blks) {
			/* but the tree is bad */
			if (desired_action == FSCK_RECORD_DUPCHECK) {
				/* not reported yet */
				fsck_send_msg(fsck_BADKEYS,
					      fsck_ref_msg(msg_info_ptr->
							   msg_inotyp),
					      fsck_ref_msg(msg_info_ptr->
							   msg_inopfx),
					      msg_info_ptr->msg_inonum, 15);
			}
			break;
		}
		/* got the key value */
		/*
		 * the key for the first entry is verified elsewhere
		 */
		if (first_entry)
			first_entry = 0;
		else {
			/* it's not the first entry on the page */
			dpls_rc = dTree_key_compare_leaflvl(&(key[last_key][0]),
							    key_len[last_key],
							    &(key[this_key][0]),
							    key_len[this_key],
							    &key_ok);
			if (dpls_rc != FSCK_OK)
				break;

			if (!key_ok) {
				/* but the key is bad */
				inorecptr->ignore_alloc_blks = 1;
				if (desired_action == FSCK_RECORD_DUPCHECK) {
					/* not reported yet */
					fsck_send_msg(fsck_BADKEYS,
						      fsck_ref_msg
						      (msg_info_ptr->
						       msg_inotyp),
						      fsck_ref_msg
						      (msg_info_ptr->
						       msg_inopfx),
						      msg_info_ptr->msg_inonum,
						      4);
				}
				break;
			}
		}
		/* the key is good */
		if (this_key) {
			this_key = 0;
			last_key = 1;
		} else {
			this_key = 1;
			last_key = 0;
		}
		key_len[this_key] = 0;
		/*
		 * If the desired action is anything besides RECORD_DUPCHECK
		 * then we're really only interested in things that indicate
		 * a bad B+ tree.  Since bad inode references in the directory
		 * entries don't imply anything about the tree itself, we only
		 * want to check them if we're doing RECORD_DUPCHECK.
		 */
		if (desired_action != FSCK_RECORD_DUPCHECK)
			continue;

		/* the inode references haven't been checked yet */
		ldtptr =
		    (struct ldtentry *)&(dtiptr->slots[dtiptr->dtstbl[dtidx]]);
		child_inoidx = (uint32_t) ldtptr->inumber;
		if (child_inoidx >= agg_recptr->fset_inode_count) {
			/* it can't be right because it's out of range */
			dpls_rc = get_inode_extension(&this_ext);
			if (dpls_rc != FSCK_OK)
				break;

			this_ext->ext_type = rmv_badentry_extension;
			this_ext->inonum = ldtptr->inumber;
			this_ext->next = inorecptr->ext_rec;
			inorecptr->ext_rec = this_ext;
			inorecptr->adj_entries = 1;
			key[this_key][key_len[last_key]] = '\0';

#ifdef ORIGINAL
			if (jfs_strfromUCS(message_parm, key[last_key],
					   MAXPARMLEN, uconv_object)
			    != ULS_SUCCESS)
				message_parm = "(Conversion Failed)";
#else
			NumChars = Unicode_String_to_UTF8_String(
						(uint8_t *)message_parm,
						key[last_key], MAXPARMLEN);
			message_parm[NumChars] = '\0';
#endif

			fsck_send_msg(fsck_BADINOREF,
				      fsck_ref_msg(msg_info_ptr->msg_inopfx),
				      msg_info_ptr->msg_inonum, message_parm,
				      " ", child_inoidx);
		} else if (child_inoidx < FILESET_OBJECT_I) {
			/*
			 * it can't be right because it's in the range
			 * reserved for special metadata and the root dir
			 */
			dpls_rc = get_inode_extension(&this_ext);
			if (dpls_rc != FSCK_OK)
				break;

			this_ext->ext_type = rmv_badentry_extension;
			this_ext->inonum = ldtptr->inumber;
			this_ext->next = inorecptr->ext_rec;
			inorecptr->ext_rec = this_ext;
			inorecptr->adj_entries = 1;
			key[this_key][key_len[last_key]] = '\0';
			NumChars = Unicode_String_to_UTF8_String(
						(uint8_t *)message_parm,
						key[last_key], MAXPARMLEN);
			message_parm[NumChars] = '\0';
			fsck_send_msg(fsck_ILLINOREF,
				      fsck_ref_msg(msg_info_ptr->msg_inopfx),
				      msg_info_ptr->msg_inonum, message_parm,
				      " ", child_inoidx);
		} else {
			/* it might be ok */
			dpls_rc = get_inorecptr(is_aggregate, alloc_ifnull,
						child_inoidx, &child_inorecptr);
			if (dpls_rc != FSCK_OK)
				break;

			/* got a record for the child */
			child_inorecptr->link_count += 1;
			if (child_inorecptr->parent_inonum == 0) {
				/* no parent recorded yet */
				child_inorecptr->parent_inonum = inonum;
			} else {
				/* this is not the first parent seen */
				dpls_rc = get_inode_extension(&this_ext);
				if (dpls_rc != FSCK_OK)
					break;
				/* got extension */
				this_ext->ext_type = parent_extension;
				this_ext->inonum = inonum;
				this_ext->next = child_inorecptr->ext_rec;
				child_inorecptr->ext_rec = this_ext;
			}
			verify_dir_index(inorecptr, inoptr, dtiptr->this_Qel,
					 dtidx, ldtptr->index);
		}
	}

	return (dpls_rc);
}

/*****************************************************************************
 * NAME: dTree_processing
 *
 * FUNCTION: Perform the specified action on the dTree rooted in the
 *           specified inode.
 *
 * PARAMETERS:
 *      inoptr          - input - pointer to the directory inode in an fsck
 *                                buffer
 *      inoidx          - input - ordinal number of the inode
 *      inorecptr       - input - pointer to an fsck inode record describing
 *                                the inode
 *      msg_info_ptr    - input - pointer to data needed to issue messages
 *                                about the inode
 *      desired_action  - input - { FSCK_RECORD | FSCK_RECORD_DUPCHECK |
 *                                  FSCK_UNRECORD | FSCK_QUERY }
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int dTree_processing(struct dinode *inoptr,
		     uint32_t inoidx,
		     struct fsck_inode_record *inorecptr,
		     struct fsck_ino_msg_info *msg_info_ptr, int desired_action)
{
	int dp_rc = FSCK_OK;
	int slotidx;
	int8_t old_ignore_alloc_blks = 0;
	int ixpxd_unequal = 0;
	struct fsck_Dtree_info dtinfo;
	struct fsck_Dtree_info *dtiptr;
	int8_t first_in_level = 0;
	int8_t last_in_level = 0;
	int8_t is_leaf = 0;
	int8_t is_root = -1;
	int8_t dtstbl_last_slot;
	uint32_t nodesize_in_bytes;
	int8_t msg_reason = 0;

	dtiptr = &dtinfo;
	dtiptr->this_Qel = NULL;
	dtiptr->next_Qel = NULL;

	/* -1 so the root will be recognized as 1st node in level 0 */
	dtiptr->last_level = -1;

	/* so we won't get a match until the actual leaf level is found */
	dtiptr->leaf_level = -1;

	dtiptr->this_key_idx = 0;
	dtiptr->last_key_idx = 1;
	dtiptr->key_len[dtiptr->this_key_idx] = 0;
	dtiptr->key_len[dtiptr->last_key_idx] = 0;
	dtiptr->leaf_seen = 0;
	/* set the flags for all slots in the current page to 'not used' */
	memset((void *)&(dtiptr->slot_map[0]), 0, DTPAGEMAXSLOT);
	dtiptr->dtr_ptr = (dtroot_t *) & (inoptr->di_btroot);
	dtiptr->max_slotidx = DTROOTMAXSLOT - 1;
	if (dtiptr->dtr_ptr->header.nextindex == 0)
		goto out;
	/* there is at least 1 entry */
	if (desired_action != FSCK_RECORD_DUPCHECK) {
		/* not the first pass */
		/*
		 * The first time through we stopped processing allocated
		 * blocks if and when we discovered the tree to be corrupt.
		 * On a 2nd pass we want to stop at the same place.
		 */
		if (inorecptr->ignore_alloc_blks) {
			/* the bit is on */
			/* set the flag */
			old_ignore_alloc_blks = -1;
			/* turn the bit off */
			inorecptr->ignore_alloc_blks = 0;
		}
	}
	dtiptr->key_len[dtiptr->last_key_idx] = 0;
	dtiptr->dtstbl = (int8_t *) & (dtiptr->dtr_ptr->header.stbl[0]);
	dtiptr->slots = &(dtiptr->dtr_ptr->slot[0]);
	dtiptr->freelist_first_dtidx = dtiptr->dtr_ptr->header.freelist;
	dtiptr->freelist_count = dtiptr->dtr_ptr->header.freecnt;
	dtiptr->last_dtidx = dtiptr->dtr_ptr->header.nextindex - 1;
	/*
	 * Do a sanity check to make sure this looks like a
	 * DTree root node
	 */
	if (dtiptr->last_dtidx > dtiptr->max_slotidx) {
		inorecptr->ignore_alloc_blks = 1;
		msg_reason = 16;
	} else if (dtiptr->freelist_count < 0) {
		inorecptr->ignore_alloc_blks = 1;
		msg_reason = 17;
	} else if (dtiptr->freelist_count > dtiptr->max_slotidx) {
		inorecptr->ignore_alloc_blks = 1;
		msg_reason = 18;
	} else if (dtiptr->freelist_first_dtidx < -1) {
		inorecptr->ignore_alloc_blks = 1;
		msg_reason = 19;
	} else if (dtiptr->freelist_first_dtidx > dtiptr->max_slotidx) {
		inorecptr->ignore_alloc_blks = 1;
		msg_reason = 20;
	} else if ((dtiptr->last_dtidx == dtiptr->max_slotidx) &&
		   (dtiptr->freelist_count != 0)) {
		inorecptr->ignore_alloc_blks = 1;
		msg_reason = 21;
	} else if ((dtiptr->last_dtidx == dtiptr->max_slotidx) &&
		   (dtiptr->freelist_first_dtidx != -1)) {
		inorecptr->ignore_alloc_blks = 1;
		msg_reason = 22;
	} else if ((dtiptr->freelist_first_dtidx != -1)
		   && (dtiptr->freelist_count == 0)) {
		inorecptr->ignore_alloc_blks = 1;
		msg_reason = 23;
	} else if ((dtiptr->freelist_first_dtidx == -1)
		   && (dtiptr->freelist_count != 0)) {
		inorecptr->ignore_alloc_blks = 1;
		msg_reason = 24;
	}
	if (inorecptr->ignore_alloc_blks && (msg_reason != 0)) {
		if (desired_action == FSCK_RECORD_DUPCHECK) {
			/* not reported yet */
			fsck_send_msg(fsck_BADKEYS,
				      fsck_ref_msg(msg_info_ptr->msg_inotyp),
				      fsck_ref_msg(msg_info_ptr->msg_inopfx),
				      msg_info_ptr->msg_inonum, msg_reason);
		}
		goto out;
	}

	/* the root looks like it's probably a root */
	/*
	 * mark all slots occupied by the header as being in use
	 */
	for (slotidx = 0; (slotidx < DTENTRYSTART); slotidx++) {
		dtiptr->slot_map[slotidx] = -1;
	}
	/*
	 * get a queue element and set it up for the root
	 */
	dp_rc = dtreeQ_get_elem(&dtiptr->this_Qel);
	if (dp_rc != FSCK_OK)
		goto out;

	/* got a queue element */
	dtiptr->this_Qel->node_level = 0;
	if (dtiptr->dtr_ptr->header.flag & BT_LEAF) {
		/* root leaf */
		dtiptr->leaf_seen = -1;
		dtiptr->leaf_level = dtiptr->this_Qel->node_level;
		is_leaf = -1;
	} else if (!(dtiptr->dtr_ptr->header.flag & BT_INTERNAL)) {
		/* but it's not an internal node either! */
		if (desired_action == FSCK_RECORD_DUPCHECK) {
			/* not reported yet */
			fsck_send_msg(fsck_BADKEYS,
				      fsck_ref_msg(msg_info_ptr->msg_inotyp),
				      fsck_ref_msg(msg_info_ptr->msg_inopfx),
				      msg_info_ptr->msg_inonum, 11);
		}
	}
	/*
	 * get the first key, but don't mark the slot_map
	 */
	dp_rc = dTree_key_extract_cautiously(dtiptr, 0,
				&(dtiptr->key[dtiptr->this_key_idx][0]),
				&(dtiptr->key_len[dtiptr->this_key_idx]),
				is_root, is_leaf, inorecptr);
	/*
	 * get the first key in the directory.  Since this is the first time
	 * we've extracted it, we need to guard against a loop even though we
	 * aren't recording it now.
	 */
	if (dp_rc != FSCK_OK)
		goto out;

	if (inorecptr->ignore_alloc_blks) {
		/* but directory is bad */
		if (desired_action == FSCK_RECORD_DUPCHECK) {
			/* not reported yet */
			fsck_send_msg(fsck_BADKEYS,
				      fsck_ref_msg(msg_info_ptr->msg_inotyp),
				      fsck_ref_msg(msg_info_ptr->msg_inopfx),
				      msg_info_ptr->msg_inonum, 40);
		}
	} else {
		/* we got the first key */
		if (is_leaf)
			dp_rc = dTree_process_leaf_slots(dtiptr, inorecptr,
							 inoptr, msg_info_ptr,
							 desired_action);
		else
			dp_rc = dTree_process_internal_slots(dtiptr, inorecptr,
							     msg_info_ptr,
							     desired_action);
		if ((dp_rc == FSCK_OK) && (!inorecptr->ignore_alloc_blks)) {
			/* node is looking good */
			dtiptr->key_len[dtiptr->last_key_idx] = 0;
			dp_rc = dTree_verify_slot_freelist(dtiptr, inorecptr,
							   msg_info_ptr,
							   desired_action);
		}
	}

	while ((dp_rc == FSCK_OK) && (!inorecptr->ignore_alloc_blks) &&
	       (agg_recptr->dtreeQ_back != NULL)) {
		/* nothing fatal and tree looks ok and queue not empty */
		dp_rc = dtreeQ_dequeue(&dtiptr->next_Qel);
		if (dp_rc != FSCK_OK)
			goto out;
		/* got another element from the queue */
		if (dtiptr->this_Qel->node_level !=
		    dtiptr->next_Qel->node_level) {
			/* it's the last in this level */
			last_in_level = -1;
			if (!is_root)
				dp_rc = dTree_node_last_in_level(dtiptr,
								 inorecptr,
								 msg_info_ptr,
								 desired_action);
		} else {
			/* it's not the last in its level */
			last_in_level = 0;
			dp_rc = dTree_node_not_last_in_level(dtiptr, inorecptr,
							     msg_info_ptr,
							     desired_action);
		}
		if ((dp_rc != FSCK_OK) || (inorecptr->ignore_alloc_blks))
			break;

		dtiptr->last_level = dtiptr->this_Qel->node_level;
		dtiptr->last_node_addr = dtiptr->this_Qel->node_addr;
		dp_rc = dtreeQ_rel_elem(dtiptr->this_Qel);
		if (dp_rc != FSCK_OK) {
			/* don't try again after loop! */
			dtiptr->this_Qel = NULL;
			break;
		}
		/* released the older element */
		/* promote newer element */
		dtiptr->this_Qel = dtiptr->next_Qel;
		/* to avoid releasing it twice */
		dtiptr->next_Qel = NULL;
		is_root = 0;
		dp_rc = dnode_get(dtiptr->this_Qel->node_addr, BYTESPERPAGE,
				  &dtiptr->dtp_ptr);
		if (dp_rc != FSCK_OK) {
			/* read failed */
			/* this isn't an fsck failure --
			 * it's a symptom of a bad dtree
			 */
			dp_rc = FSCK_OK;
			inorecptr->ignore_alloc_blks = 1;
			if (desired_action == FSCK_RECORD_DUPCHECK) {
				/* not reported yet */
				fsck_send_msg(fsck_BADKEYS,
					 fsck_ref_msg(msg_info_ptr->msg_inotyp),
					 fsck_ref_msg(msg_info_ptr->msg_inopfx),
					 msg_info_ptr->msg_inonum, 38);
			}
			break;
		}
		/* got the new node */
		/* returns 0 if equal */
		ixpxd_unequal =
		    memcmp((void *)&(dtiptr->dtp_ptr->header.self),
			   (void *)&(dtiptr->this_Qel->node_pxd),
			   sizeof(pxd_t));
		if (ixpxd_unequal) {
			/* bad self pxd in header */
			inorecptr->ignore_alloc_blks = 1;
			if (desired_action == FSCK_RECORD_DUPCHECK)
				/* not reported yet */
				fsck_send_msg(fsck_BADINONODESELF,
					 fsck_ref_msg(msg_info_ptr->msg_inotyp),
					 fsck_ref_msg(msg_info_ptr->msg_inopfx),
					 msg_info_ptr->msg_inonum);
			break;
		} else if (dtiptr->dtp_ptr->header.nextindex == 0) {
			/* an empty non-root node */
			inorecptr->ignore_alloc_blks = 1;
			if (desired_action == FSCK_RECORD_DUPCHECK)
				/* not reported yet */
				fsck_send_msg(fsck_BADINOMTNODE,
					 fsck_ref_msg(msg_info_ptr->msg_inotyp),
					 fsck_ref_msg(msg_info_ptr->msg_inopfx),
					 msg_info_ptr->msg_inonum);
			break;
		}
		/* node is not empty */
		dtiptr->max_slotidx = dtiptr->dtp_ptr->header.maxslot - 1;
		if (dtiptr->dtp_ptr->header.flag & BT_LEAF) {
			/* a leaf */
			dtiptr->leaf_seen = -1;
			dtiptr->leaf_level = dtiptr->this_Qel->node_level;
			is_leaf = -1;
		} else if (!(dtiptr->dtp_ptr->header.flag & BT_INTERNAL)) {
			/* but it's not an internal node either! */
			if (desired_action == FSCK_RECORD_DUPCHECK) {
				/* not reported yet */
				fsck_send_msg(fsck_BADKEYS,
					 fsck_ref_msg(msg_info_ptr->msg_inotyp),
					 fsck_ref_msg(msg_info_ptr->msg_inopfx),
					 msg_info_ptr->msg_inonum, 41);
			}
		}

		/*
		 * set the flags for all slots in the
		 * current page to 'not used'
		 */
		memset((void *)&(dtiptr->slot_map[0]), 0, DTPAGEMAXSLOT);
		dtiptr->dtstbl =
		    (int8_t *) & (dtiptr->dtp_ptr->
				  slot[dtiptr->dtp_ptr->header.stblindex]);
		dtiptr->slots = &(dtiptr->dtp_ptr->slot[0]);
		dtiptr->freelist_first_dtidx = dtiptr->dtp_ptr->header.freelist;
		dtiptr->freelist_count = dtiptr->dtp_ptr->header.freecnt;
		dtiptr->last_dtidx = dtiptr->dtp_ptr->header.nextindex - 1;
		/*
		 * mark all slots occupied by the header as being in use
		 */
		for (slotidx = 0; (slotidx < DTENTRYSTART); slotidx++) {
			dtiptr->slot_map[slotidx] = -1;
		}
		/*
		 * figure out which slots are occupied by the dtstbl and
		 * mark them as being in use
		 */
		nodesize_in_bytes =
		    dtiptr->this_Qel->node_size * sb_ptr->s_bsize;
		if (nodesize_in_bytes > DTHALFPGNODEBYTES) {
			dtstbl_last_slot = (DTHALFPGNODETSLOTS << 1) +
			    dtiptr->dtp_ptr->header.stblindex - 1;
		} else if (nodesize_in_bytes == DTHALFPGNODEBYTES) {
			dtstbl_last_slot = DTHALFPGNODETSLOTS +
			    dtiptr->dtp_ptr->header.stblindex - 1;
		} else {
			dtstbl_last_slot = (DTHALFPGNODETSLOTS >> 1) +
			    dtiptr->dtp_ptr->header.stblindex - 1;
		}
		/*
		 * Do a sanity check to make sure this looks like a
		 * DTree root node
		 */
		if ((dtiptr->dtp_ptr->header.maxslot != DT8THPGNODESLOTS)
		    && (dtiptr->dtp_ptr->header.maxslot != DTQTRPGNODESLOTS)
		    && (dtiptr->dtp_ptr->header.maxslot != DTHALFPGNODESLOTS)
		    && (dtiptr->dtp_ptr->header.maxslot != DTFULLPGNODESLOTS)) {
			inorecptr->ignore_alloc_blks = 1;
			msg_reason = 25;
		} else if (dtiptr->last_dtidx > dtiptr->max_slotidx) {
			inorecptr->ignore_alloc_blks = 1;
			msg_reason = 26;
		} else if (dtiptr->freelist_count < 0) {
			inorecptr->ignore_alloc_blks = 1;
			msg_reason = 27;
		} else if (dtiptr->freelist_count > dtiptr->max_slotidx) {
			inorecptr->ignore_alloc_blks = 1;
			msg_reason = 28;
		} else if (dtiptr->freelist_first_dtidx < -1) {
			inorecptr->ignore_alloc_blks = 1;
			msg_reason = 29;
		} else if (dtiptr->freelist_first_dtidx > dtiptr->max_slotidx) {
			inorecptr->ignore_alloc_blks = 1;
			msg_reason = 30;
		} else if ((dtiptr->last_dtidx == dtiptr->max_slotidx)
			   && (dtiptr->freelist_count != 0)) {
			inorecptr->ignore_alloc_blks = 1;
			msg_reason = 31;
		} else if ((dtiptr->last_dtidx == dtiptr->max_slotidx)
			   && (dtiptr->freelist_first_dtidx != -1)) {
			inorecptr->ignore_alloc_blks = 1;
			msg_reason = 32;
		} else if ((dtiptr->freelist_first_dtidx != -1)
			   && (dtiptr->freelist_count == 0)) {
			inorecptr->ignore_alloc_blks = 1;
			msg_reason = 33;
		} else if ((dtiptr->freelist_first_dtidx == -1)
			   && (dtiptr->freelist_count != 0)) {
			inorecptr->ignore_alloc_blks = 1;
			msg_reason = 34;
		} else if (dtiptr->dtp_ptr->header.stblindex >
			   dtiptr->max_slotidx) {
			inorecptr->ignore_alloc_blks = 1;
			msg_reason = 35;
		} else if (dtiptr->dtp_ptr->header.stblindex < 1) {
			inorecptr->ignore_alloc_blks = 1;
			msg_reason = 36;
		}
		if (inorecptr->ignore_alloc_blks && (msg_reason != 0)) {
			if (desired_action == FSCK_RECORD_DUPCHECK)
				/* not reported yet */
				fsck_send_msg(fsck_BADKEYS,
					 fsck_ref_msg(msg_info_ptr->msg_inotyp),
					 fsck_ref_msg(msg_info_ptr->msg_inopfx),
					 msg_info_ptr->msg_inonum, msg_reason);
			break;
		}
		/* the node looks like it's probably a real node */
		for (slotidx = dtiptr->dtp_ptr->header.stblindex;
		     (slotidx <= dtstbl_last_slot); slotidx++)
			dtiptr->slot_map[slotidx] = -1;
		/*
		 * get the first key, but don't mark the slot_map
		 */
		dp_rc =
		    dTree_key_extract_cautiously(dtiptr, 0,
				 &(dtiptr->key[dtiptr->this_key_idx][0]),
				 &(dtiptr->key_len[dtiptr->this_key_idx]),
				 is_root, is_leaf, inorecptr);
		/*
		 * get the first key in the directory.
		 * Since this is the first time we've
		 * extracted it, we need to guard against
		 * a loop even though we aren't recording
		 * it now.
		 */
		if (dp_rc != FSCK_OK)
			break;
		if (inorecptr->ignore_alloc_blks) {
			/* but directory is bad */
			if (desired_action == FSCK_RECORD_DUPCHECK) {
				/* not reported yet */
				fsck_send_msg(fsck_BADKEYS,
					 fsck_ref_msg(msg_info_ptr->msg_inotyp),
					 fsck_ref_msg(msg_info_ptr->msg_inopfx),
					 msg_info_ptr->msg_inonum, 12);
			}
			break;
		}
		/* we got the first key */
		dp_rc =
		    dTree_node_first_key(dtiptr, inorecptr, msg_info_ptr,
					 desired_action);
		if ((dp_rc != FSCK_OK) || inorecptr->ignore_alloc_blks)
			break;

		/* tree still interesting */
		if (dtiptr->dtp_ptr->header.flag & BT_LEAF) {
			is_leaf = -1;
		} else if (!(dtiptr->dtp_ptr->header.flag & BT_INTERNAL)) {
			/* but it's not an internal node either! */
			if (desired_action == FSCK_RECORD_DUPCHECK)
				/* not reported yet */
				fsck_send_msg(fsck_BADKEYS,
					 fsck_ref_msg(msg_info_ptr->msg_inotyp),
					 fsck_ref_msg(msg_info_ptr->msg_inopfx),
					 msg_info_ptr->msg_inonum, 42);
		} else
			is_leaf = 0;

		/* tree still looks ok */
		if (is_leaf)
			dp_rc =
			    dTree_process_leaf_slots(dtiptr, inorecptr,
						     inoptr, msg_info_ptr,
						     desired_action);
		else {
			/* an internal node */
			if (dtiptr->last_level != dtiptr->this_Qel->node_level) {
				/* this node is the first in a
				   new level of internal nodes.  */
				/* This is the size of the leaf
				 * nodes, so we only want the total
				 * described by the LAST level of
				 * internal nodes.
				 */
				if (!(sb_ptr->s_flag & JFS_DIR_INDEX))
					agg_recptr->this_inode.data_size = 0;
			}
			/* This is the size of the leaf
			   nodes, so we only want ... */
			dp_rc = dTree_process_internal_slots(dtiptr, inorecptr,
							     msg_info_ptr,
							     desired_action);
		}
		if ((dp_rc != FSCK_OK) || inorecptr->ignore_alloc_blks)
			break;

		if (dtiptr->last_level != dtiptr->this_Qel->node_level) {
			/* this is a new level */
			first_in_level = -1;
			dtiptr->key_len[dtiptr->last_key_idx] = 0;
			if (!is_root)
				dp_rc =
				    dTree_node_first_in_level(dtiptr, inorecptr,
							      msg_info_ptr,
							      desired_action);
		} else {
			/* not 1st in level */
			first_in_level = 0;
			dp_rc =
			    dTree_node_not_first_in_level(dtiptr, inorecptr,
							  msg_info_ptr,
							  desired_action);
		}
		if ((dp_rc != FSCK_OK) || inorecptr->ignore_alloc_blks)
			break;

		dp_rc = dTree_node_size_check(dtiptr, is_leaf, first_in_level,
					      last_in_level, inorecptr,
					      msg_info_ptr, desired_action);
		if ((dp_rc != FSCK_OK) || inorecptr->ignore_alloc_blks)
			break;

		dp_rc = dTree_verify_slot_freelist(dtiptr, inorecptr,
						   msg_info_ptr,
						   desired_action);
		if ((dp_rc != FSCK_OK) || inorecptr->ignore_alloc_blks)
			break;

		if (agg_recptr->dtreeQ_back == NULL) {
			/* nothing fatal and tree looks ok and queue is now
			 * empty
			 */
			last_in_level = -1;
			if (!is_root)
				dp_rc =
				    dTree_node_last_in_level(dtiptr, inorecptr,
							     msg_info_ptr,
							     desired_action);
		}
	}

	/*
	 * there's at least 1 more Q element to release for this node, and
	 * if the tree is bad there may still be some on the queue as well.
	 *
	 * (If there's a processing error all the dynamic storage is going
	 * to be released so there's no point in preparing these elements
	 * for reuse.)
	 */
	if (dp_rc == FSCK_OK) {
		if (dtiptr->this_Qel != NULL) {
			dp_rc = dtreeQ_rel_elem(dtiptr->this_Qel);
		}
	}
	if (dp_rc == FSCK_OK) {
		if (dtiptr->next_Qel != NULL) {
			dp_rc = dtreeQ_rel_elem(dtiptr->next_Qel);
		}
	}
	agg_recptr->dtreeQ_back = NULL;
	while ((dp_rc == FSCK_OK) && (agg_recptr->dtreeQ_front != NULL)) {
		dtiptr->this_Qel = agg_recptr->dtreeQ_front;
		agg_recptr->dtreeQ_front = dtiptr->this_Qel->next;
		dp_rc = dtreeQ_rel_elem(dtiptr->this_Qel);
	}

	if (dp_rc == FSCK_OK) {
		if (desired_action != FSCK_RECORD_DUPCHECK) {
			/* we altered the corrupt tree bit */
			if (old_ignore_alloc_blks &&
			    !inorecptr->ignore_alloc_blks)
				/*
				 * the flag is set but the bit didn't get
				 * turned back on.  This means that the first
				 * time we went through this tree we decided
				 * it was corrupt but this time it looked ok.
				 */
				dp_rc = FSCK_INTERNAL_ERROR_10;
			else if (!old_ignore_alloc_blks &&
				 inorecptr->ignore_alloc_blks)
				/*
				 * the flag is off but the bit got turned on.
				 * This means that the first time we went
				 * through this tree it looked ok but this
				 * time we decided that it is corrupt.
				 */
				dp_rc = FSCK_INTERNAL_ERROR_11;
		}
	}
      out:
	return (dp_rc);
}

/*****************************************************************************
 * NAME: dTree_search
 *
 * FUNCTION: Search the dTree rooted in the specified inode for the given
 *           filename.
 *
 * PARAMETERS:
 *      dir_inoptr               - input - pointer to the directory inode in
 *                                         an fsck buffer
 *      given_key                - input - pointer to the filename to find, in
 *                                         mixed case
 *      given_key_length         - input - length of the string in given_key
 *      given_key_folded         - input - pointer to the given_key in all
 *                                         upper case
 *      given_key_folded_length  - input - length of the string in
 *                                         given_key_folded
 *      addr_slot_ptr            - input - pointer to a variable in which to
 *                                         return the address of the slot
 *                                         which contains (or begins) the
 *                                         the key which is a match for
 *                                         given_key
 *      match_found              - input - pointer to a variable in which to
 *                                         return !0 if an entry with given_key
 *                                                   or given_key_folded was
 *                                                   found in the given
 *                                                   directory inode
 *                                                 0 if no entry with given_key
 *                                                   or given_key_folded was
 *                                                   found in the given
 *                                                   directory inode
 *      inorecptr                - input - pointer to an fsck inode record
 *                                         describing the inode
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int dTree_search(struct dinode *dir_inoptr,
		 UniChar * given_key,
		 uint32_t given_key_length,
		 UniChar * given_key_folded,
		 uint32_t given_key_folded_length,
		 struct dtslot **addr_slot_ptr,
		 int8_t * match_found, struct fsck_inode_record *inorecptr)
{
	int ds_rc = FSCK_OK;
	int8_t slot_selected;
	int8_t selected_slotidx;
	int8_t is_root = -1;
	int8_t leaf_found = 0;
	int8_t not_there = 0;
	int8_t key_matched = 0;
	struct idtentry *intentry;
	int64_t node_addr;
	uint32_t node_length;
	struct fsck_Dtree_info dtinfo;
	struct fsck_Dtree_info *dtiptr;

	dtiptr = &dtinfo;

	dtiptr->dtr_ptr = (dtroot_t *) & (dir_inoptr->di_btroot);
	dtiptr->dtstbl = (int8_t *) & (dtiptr->dtr_ptr->header.stbl[0]);
	dtiptr->slots = (struct dtslot *) &(dtiptr->dtr_ptr->slot[0]);

	if (dtiptr->dtr_ptr->header.nextindex > 0) {
		/* the directory isn't empty */
		dtiptr->last_dtidx = dtiptr->dtr_ptr->header.nextindex - 1;
		dtiptr->max_slotidx = DTROOTMAXSLOT - 1;
		if (dtiptr->dtr_ptr->header.flag & BT_LEAF) {
			/* it's a root leaf */
			leaf_found = -1;
		}

		while ((!leaf_found) && (!not_there) && (ds_rc == FSCK_OK)) {
			if (sb_ptr->s_flag & JFS_OS2) {
				/* case is preserved but ignored */
				/* search case insensitive */
				ds_rc =
				    dTree_binsrch_internal_page(dtiptr,
								given_key_folded,
								given_key_folded_length,
								is_root,
								&not_there,
								&slot_selected,
								&selected_slotidx,
								inorecptr);
			} else {
				/* search case sensitive */
				ds_rc =
				    dTree_binsrch_internal_page(dtiptr,
								given_key,
								given_key_length,
								is_root,
								&not_there,
								&slot_selected,
								&selected_slotidx,
								inorecptr);
			}
			if ((slot_selected) && (ds_rc == FSCK_OK)) {
				intentry =
				    (struct idtentry *) &(dtiptr->
							  slots
							  [selected_slotidx]);
				node_addr = addressPXD(&(intentry->xd));
				node_length = lengthPXD(&(intentry->xd));
				ds_rc =
				    dnode_get(node_addr, BYTESPERPAGE,
					      &(dtiptr->dtp_ptr));
				if (ds_rc == FSCK_OK) {
					is_root = 0;
					dtiptr->dtstbl = (int8_t *)
					    & (dtiptr->dtp_ptr->
					       slot[dtiptr->dtp_ptr->header.
						    stblindex]);
					dtiptr->slots =
					    (struct dtslot *) &(dtiptr->
								dtp_ptr->
								slot[0]);
					dtiptr->last_dtidx =
					    dtiptr->dtp_ptr->header.nextindex -
					    1;
					dtiptr->max_slotidx =
					    (node_length * sb_ptr->s_bsize) /
					    DTSLOTSIZE;
					if (dtiptr->dtp_ptr->header.
					    flag & BT_LEAF) {
						leaf_found = -1;
					}
				}
			}
		}

		if ((!not_there) && (ds_rc == FSCK_OK)) {
			/* might be in there */
			if (sb_ptr->s_flag & JFS_OS2) {
				/* case is preserved but ignored */
				/* search case insensitive */
				ds_rc =
				    dTree_binsrch_leaf(dtiptr, given_key_folded,
						       given_key_folded_length,
						       is_root, &not_there,
						       &key_matched,
						       &slot_selected,
						       &selected_slotidx,
						       inorecptr);
			} else {
				/* search case sensitive */
				ds_rc = dTree_binsrch_leaf(dtiptr, given_key,
							   given_key_length,
							   is_root,
							   &not_there,
							   &key_matched,
							   &slot_selected,
							   &selected_slotidx,
							   inorecptr);
			}
		}
	}
	if ((ds_rc == FSCK_OK) && (key_matched)) {
		/* found it! */
		*addr_slot_ptr = &(dtiptr->slots[selected_slotidx]);
		*match_found = -1;
	} else {
		/* no luck */
		*addr_slot_ptr = NULL;
		*match_found = 0;
	}

	return (ds_rc);
}

/*****************************************************************************
 * NAME: dTree_verify_slot_freelist
 *
 * FUNCTION: Verify the structure and contents of the slot freelist in the
 *           specified directory node.
 *
 * PARAMETERS:
 *      dtiptr          - input - pointer to an fsck record describing the
 *                                directory tree
 *      inorecptr       - input - pointer to an fsck inode record describing
 *                                the inode
 *      msg_info_ptr    - input - pointer to data needed to issue messages
 *                                about the inode
 *      desired_action  - input - { FSCK_RECORD | FSCK_RECORD_DUPCHECK |
 *                                  FSCK_UNRECORD | FSCK_QUERY }
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int dTree_verify_slot_freelist(struct fsck_Dtree_info *dtiptr,
			       struct fsck_inode_record *inorecptr,
			       struct fsck_ino_msg_info *msg_info_ptr,
			       int desired_action)
{
	int dvsf_rc = FSCK_OK;
	uint8_t slotidx;
	struct dtslot *slot_ptr;
	int8_t freelist_entry;
	int8_t freelist_size = 0;

	if (dtiptr->freelist_first_dtidx == -1) {
		/* the list is empty */
		if (dtiptr->freelist_count > 0) {
			/* but the counter is nonzero */
			inorecptr->ignore_alloc_blks = 1;
			if (desired_action == FSCK_RECORD_DUPCHECK) {
				/* not reported yet */
				fsck_send_msg(fsck_BADDINOFREELIST1,
					      fsck_ref_msg(msg_info_ptr->msg_inopfx),
					      msg_info_ptr->msg_inonum);
			}
		}
	} else {
		/* the list is not empty */
		freelist_entry = dtiptr->freelist_first_dtidx;
		while ((freelist_entry != -1) &&
		       (dvsf_rc == FSCK_OK) &&
		       (!inorecptr->ignore_alloc_blks) &&
		       (freelist_size <= dtiptr->freelist_count)) {
			if (dtiptr->slot_map[freelist_entry]) {
				/* already marked! */
				inorecptr->ignore_alloc_blks = 1;
				if (desired_action == FSCK_RECORD_DUPCHECK) {
					/* not reported yet */
					fsck_send_msg(fsck_BADDINOFREELIST4,
						      fsck_ref_msg(msg_info_ptr->msg_inopfx),
						      msg_info_ptr->msg_inonum);
				}
			} else {
				/* not claimed yet */
				/* mark this one */
				dtiptr->slot_map[freelist_entry] = -1;
				/* count this one */
				freelist_size++;
				slot_ptr =
				    (struct dtslot *) &(dtiptr->
							slots[freelist_entry]);
				freelist_entry = slot_ptr->next;
			}
		}
	}
	if (!inorecptr->ignore_alloc_blks) {
		/* nothing wrong yet */
		if (freelist_size != dtiptr->freelist_count) {
			/* size is wrong */
			inorecptr->ignore_alloc_blks = 1;
			if (desired_action == FSCK_RECORD_DUPCHECK) {
				/* not reported yet */
				fsck_send_msg(fsck_BADDINOFREELIST2,
					      fsck_ref_msg(msg_info_ptr->msg_inopfx),
					      msg_info_ptr->msg_inonum);
			}
		}
	}
	if (!inorecptr->ignore_alloc_blks) {
		/* still looks good */
		for (slotidx = 0; ((slotidx <= dtiptr->max_slotidx)
				   && (!inorecptr->ignore_alloc_blks));
		     slotidx++) {
			if (dtiptr->slot_map[slotidx] == 0) {
				/* a slot which is not in a key chain and is also not on the free list */
				inorecptr->ignore_alloc_blks = 1;
				if (desired_action == FSCK_RECORD_DUPCHECK) {
					/* not reported yet */
					fsck_send_msg(fsck_BADDINOFREELIST3,
						      fsck_ref_msg(msg_info_ptr->msg_inopfx),
						      msg_info_ptr->msg_inonum);
				}
			}
		}
	}
	return (dvsf_rc);
}

/*****************************************************************************
 * NAME: find_first_dir_leaf
 *
 * FUNCTION: Locate the leftmost leaf node in the dTree rooted in the
 *           given inode.
 *
 * PARAMETERS:
 *      inoptr           - input - pointer to the directory inode in an
 *                                 fsck buffer
 *      addr_leaf_ptr    - input - pointer to a variable in which to return
 *                                 the address, in an fsck buffer, of the
 *                                 inode's left-most leaf.
 *      leaf_agg_offset  - input - pointer to a variable in which to return
 *                                 the offset, in the aggregate, of the
 *                                 leftmost leaf in the B+ Tree rooted in
 *                                 the directory inode.  (This has a
 *                                 special meaning if the directory root is
 *                                 not an internal node. See the code below.)
 *      is_inline        - input - pointer to a variable in which to return
 *                                 !0 if the directory data is within the
 *                                    inode
 *                                  0 if the directory data is not within
 *                                    the inode
 *      is_rootleaf      - input - pointer to a variable in which to return
 *                                 !0 if the B+ Tree rooted in the directory
 *                                    inode has a root leaf
 *                                  0 if the B+ Tree rooted in the directory
 *                                      inode does not have a root leaf
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int find_first_dir_leaf(struct dinode *inoptr,
			dtpage_t ** addr_leaf_ptr,
			int64_t * leaf_agg_offset,
			int8_t * is_inline, int8_t * is_rootleaf)
{
	int ffdl_rc = FSCK_OK;
	dtpage_t *dtpg_ptr;
	dtroot_t *dtroot_ptr;
	struct idtentry *idtentry_ptr;
	int64_t first_child_addr;
	int8_t *dtstbl;

	*is_rootleaf = 0;	/* assume inode has no data */
	*is_inline = 0;		/* assume inode has no data */
	*addr_leaf_ptr = NULL;	/* assume inode has no data */
	*leaf_agg_offset = 0;	/* assume inode has no data */
	dtroot_ptr = (dtroot_t *) & (inoptr->di_btroot);
	if (dtroot_ptr->header.flag & BT_LEAF) {
		/* it's a root-leaf */
		*is_rootleaf = -1;
		*leaf_agg_offset = addressPXD(&(inoptr->di_ixpxd));
	} else {
		/* it's a tree */
		idtentry_ptr = (struct idtentry *)
		    &(dtroot_ptr->slot[dtroot_ptr->header.stbl[0]]);
		/*
		 * the slot number of the entry describing
		 * the first child is in the 0th entry of
		 * the header.stbl array.
		 */
		first_child_addr = addressPXD(&(idtentry_ptr->xd));

		ffdl_rc = dnode_get(first_child_addr, BYTESPERPAGE, &dtpg_ptr);

		while ((ffdl_rc == FSCK_OK) && (*leaf_agg_offset == 0)) {
			if (dtpg_ptr->header.flag & BT_LEAF) {
				/* found it!  */
				*addr_leaf_ptr = dtpg_ptr;
				*leaf_agg_offset = first_child_addr;
			} else {
				/* keep moving down the tree */
				dtstbl =
				    (int8_t *) & (dtpg_ptr->
						  slot[dtpg_ptr->header.
						       stblindex]);
				idtentry_ptr = (struct idtentry *)
				    &(dtpg_ptr->slot[dtstbl[0]]);
				/*
				 * the address of the idtentry describing
				 * the first child
				 */
				first_child_addr =
				    addressPXD(&(idtentry_ptr->xd));
				ffdl_rc =
				    dnode_get(first_child_addr, BYTESPERPAGE,
					      &dtpg_ptr);
			}
		}
	}

	return (ffdl_rc);
}

/*****************************************************************************
 * NAME: init_dir_tree
 *
 * FUNCTION: Initialize the dTree rooted at the given address.
 *
 * PARAMETERS:
 *      btroot_ptr  - input - pointer to the root of the B+ Tree rooted in
 *                            the directory inode
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
void init_dir_tree(dtroot_t * btroot_ptr)
{
	int slot_idx;

	btroot_ptr->header.flag = (BT_ROOT | BT_LEAF | DXD_INDEX);
	btroot_ptr->header.nextindex = 0;
	btroot_ptr->header.freecnt = DTROOTMAXSLOT - 1;
	btroot_ptr->header.freelist = 1;
	for (slot_idx = 1; (slot_idx < DTROOTMAXSLOT); slot_idx++) {
		btroot_ptr->slot[slot_idx].next = slot_idx + 1;
	}
	btroot_ptr->slot[DTROOTMAXSLOT - 1].next = -1;

	return;
}

/*****************************************************************************
 * NAME: process_valid_dir_data
 *
 * FUNCTION: Perform the specified action on the nodes of the specified dTree.
 *           Assume that the dTree structure has already been validated.
 *
 * PARAMETERS:
 *      inoptr          - input - pointer to the directory inode in an fsck
 *                                buffer
 *      inoidx          - input - ordinal number of the inode
 *      inorecptr       - input - pointer to an fsck inode record describing
 *                                the inode
 *      msg_info_ptr    - input - pointer to data needed to issue messages
 *                                about the inode
 *      desired_action  - input - { FSCK_RECORD | FSCK_UNRECORD | FSCK_QUERY }
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int process_valid_dir_data(struct dinode *inoptr,
			   uint32_t inoidx,
			   struct fsck_inode_record *inorecptr,
			   struct fsck_ino_msg_info *msg_info_ptr,
			   int desired_action)
{
	int pvdd_rc = FSCK_OK;
	dtroot_t *dtroot_ptr;
	dtpage_t *dtpage_ptr;
	int8_t *dtstbl;
	struct dtslot *slots;
	int last_dtidx;
	struct idtentry *idtentry_ptr;	/* internal node entry */
	int64_t node_addr_fsblks;
	int64_t first_child_addr;
	int8_t all_done = 0;

	dtroot_ptr = (dtroot_t *) & (inoptr->di_btroot);
	if (dtroot_ptr->header.flag != 0) {
		/* there is data for this inode */
			if (!(dtroot_ptr->header.flag & BT_LEAF)) {
				/* not a root-leaf */
				dtstbl =
				    (int8_t *) & (dtroot_ptr->header.stbl[0]);
				slots = &(dtroot_ptr->slot[0]);
				last_dtidx = dtroot_ptr->header.nextindex - 1;
				idtentry_ptr =
				    (struct idtentry *) &(slots[dtstbl[0]]);
				/* slot describing the first child */
				first_child_addr =
				    addressPXD(&(idtentry_ptr->xd));
				pvdd_rc =
				    process_valid_dir_node(dtstbl, slots,
							   last_dtidx,
							   inorecptr,
							   msg_info_ptr,
							   desired_action);
				while ((pvdd_rc == FSCK_OK) && (!all_done)) {
					/* while not done */
					/*
					 * We have the address of the first node in the new level.
					 * Get the first node in the new level.
					 */
					pvdd_rc =
					    dnode_get(first_child_addr,
						      BYTESPERPAGE,
						      &dtpage_ptr);
					if (pvdd_rc == FSCK_OK) {
						/* got the first child */
						if (dtpage_ptr->header.
						    flag & BT_LEAF) {
							/* we're done */
							all_done = -1;
						} else {
							/* not down to the leaf level yet */
							/*
							 * Set up to process this level.
							 */
							dtstbl = (int8_t *)
							    & (dtpage_ptr->
							       slot[dtpage_ptr->
								    header.
								    stblindex]);
							slots =
							    &(dtpage_ptr->
							      slot[0]);
							last_dtidx =
							    dtpage_ptr->header.
							    nextindex - 1;
							/*
							 * save the address of this node's first child
							 */
							idtentry_ptr =
							    (struct idtentry *)
							    &(slots[dtstbl[0]]);
							/* slot describing the first child */
							first_child_addr =
							    addressPXD(&
								       (idtentry_ptr->
									xd));
							/*
							 * process everything described by this node
							 */
							pvdd_rc =
							    process_valid_dir_node
							    (dtstbl, slots,
							     last_dtidx,
							     inorecptr,
							     msg_info_ptr,
							     desired_action);
							/*
							 * process everything described by this node's
							 * siblings and cousins (if any)
							 */
							while ((pvdd_rc ==
								FSCK_OK)
							       && (dtpage_ptr->
								   header.
								   next !=
								   ((int64_t)
								    0))) {
								/* more to process on this level */
								node_addr_fsblks
								    =
								    dtpage_ptr->
								    header.next;

								/*
								 * Get the next node in this level.
								 */
								pvdd_rc =
								    dnode_get
								    (node_addr_fsblks,
								     BYTESPERPAGE,
								     &dtpage_ptr);
								if (pvdd_rc ==
								    FSCK_OK) {
									/* got the node */
									/*
									 * Set up to process this node.
									 */
									dtstbl =
									    (int8_t
									     *)
									    &
									    (dtpage_ptr->
									     slot
									     [dtpage_ptr->
									      header.
									      stblindex]);
									slots =
									    &
									    (dtpage_ptr->
									     slot
									     [0]);
									last_dtidx
									    =
									    dtpage_ptr->
									    header.
									    nextindex
									    - 1;
									pvdd_rc
									    =
									    process_valid_dir_node
									    (dtstbl,
									     slots,
									     last_dtidx,
									     inorecptr,
									     msg_info_ptr,
									     desired_action);
								}
							}
						}
					}
				}
			}
	}
	if ((pvdd_rc == FSCK_OK) &&
	    (sb_ptr->s_flag & JFS_DIR_INDEX) &&
	    (inoptr->di_next_index > MAX_INLINE_DIRTABLE_ENTRY + 1)) {
		pvdd_rc =
		    process_valid_data(inoptr, inoidx, inorecptr, msg_info_ptr,
				       desired_action);
	}

	return (pvdd_rc);
}

/*****************************************************************************
 * NAME: process_valid_dir_node
 *
 * FUNCTION: Perform the specified action on the specified dTree node.
 *           Assume that the node structure has already been validated.
 *
 * PARAMETERS:
 *      dtstbl          - input - pointer to the sorted entry index table
 *                                in the directory node
 *      slots           - input - pointer to slot[0] in the directory node
 *      last_dtidx      - input - last valid entry in the directory node's
 *                                sorted entry index table
 *      inorecptr       - input - pointer to an fsck inode record describing
 *                                the inode
 *      msg_info_ptr    - input - pointer to data needed to issue messages
 *                                about the inode
 *      desired_action  - input - { FSCK_RECORD | FSCK_UNRECORD | FSCK_QUERY }
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int process_valid_dir_node(int8_t * dtstbl,
			   struct dtslot *slots,
			   int last_dtidx,
			   struct fsck_inode_record *inorecptr,
			   struct fsck_ino_msg_info *msg_info_ptr,
			   int desired_action)
{
	int pvdn_rc = FSCK_OK;
	int dtidx;
	struct idtentry *idtentry_ptr;	/* internal node entry */
	int64_t child_addr;
	uint32_t child_length;
	int8_t is_EA = 0;
	int8_t is_ACL = 0;
	int8_t extent_is_valid;
	uint32_t block_count;

	for (dtidx = 0; ((pvdn_rc == FSCK_OK) && (dtidx <= last_dtidx));
	     dtidx++) {
		/* for each entry used in the dtstbl */
		idtentry_ptr = (struct idtentry *) &(slots[dtstbl[dtidx]]);
		/* slot describing the first child */
		child_addr = addressPXD(&(idtentry_ptr->xd));
		child_length = lengthPXD(&(idtentry_ptr->xd));
		pvdn_rc = process_extent(inorecptr, child_length, child_addr,
					 is_EA, is_ACL, msg_info_ptr,
					 &block_count, &extent_is_valid,
					 desired_action);
		agg_recptr->blocks_this_fset -= block_count;
		agg_recptr->this_inode.all_blks -= block_count;
		agg_recptr->this_inode.data_blks -= block_count;
		if ((desired_action == FSCK_RECORD)
		    || (desired_action == FSCK_RECORD_DUPCHECK)) {
			agg_recptr->blocks_this_fset += block_count;
			agg_recptr->this_inode.all_blks += block_count;
			agg_recptr->this_inode.data_blks += block_count;
		} else if (desired_action == FSCK_UNRECORD) {
			agg_recptr->blocks_this_fset -= block_count;
			agg_recptr->this_inode.all_blks -= block_count;
			agg_recptr->this_inode.data_blks -= block_count;
		}
	}

	return (pvdn_rc);
}

/*****************************************************************************
 * NAME: reconnect_fs_inodes
 *
 * FUNCTION: Add a directory entry to /lost+found/ for each unconnected
 *           inode in the list at agg_recptr->inode_reconn_extens
 *
 * PARAMETERS:  none
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int reconnect_fs_inodes()
{
	int rfsi_rc = FSCK_OK;
	int intermed_rc = FSCK_OK;
	char inopfx = ' ';	/* fileset inodes */
	uint dir_inodes_reconnected = 0;
	uint dir_inodes_not_reconnected = 0;
	uint inodes_reconnected = 0;
	uint inodes_not_reconnected = 0;
	char inoname[16];
	char *ino_name;
	struct fsck_inode_ext_record *this_ext;
	struct fsck_inode_record *inorecptr;
	int aggregate_inode = 0;
	int alloc_ifnull = 0;
	int is_aggregate = 0;	/* aggregate has no dirs       */
	int which_it = FILESYSTEM_I;	/* only 1 fileset in release 1 */
	struct dinode *lsfn_inoptr;

	ino_name = &(inoname[0]);
	this_ext = agg_recptr->inode_reconn_extens;
	intermed_rc =
	    inode_get(is_aggregate, which_it, agg_recptr->lsfn_inonum,
		      &lsfn_inoptr);
	if (intermed_rc != FSCK_OK) {
		/* this is unexpected but not fatal to this run or to the filesystem */
		agg_recptr->lsfn_ok = 0;
	}
	while ((rfsi_rc == FSCK_OK) && (this_ext != NULL)) {
		rfsi_rc = get_inorecptr(aggregate_inode, alloc_ifnull,
					this_ext->inonum, &inorecptr);
		if ((rfsi_rc == FSCK_OK) && (inorecptr == NULL)) {
			rfsi_rc = FSCK_INTERNAL_ERROR_47;
		} else if (rfsi_rc == FSCK_OK) {
			if (agg_recptr->lsfn_ok) {
				/* /lost+found/ resolved ok */
				/*
				 * compute a name for the directory entry
				 */
				if (inorecptr->inode_type == directory_inode) {
					sprintf(ino_name, "%s%06d%s",
						msg_defs[fsck_dirpfx].msg_txt,
						this_ext->inonum,
						msg_defs[fsck_dotext].msg_txt);
				} else {
					sprintf(ino_name, "%s%06d%s",
						msg_defs[fsck_inopfx].msg_txt,
						this_ext->inonum,
						msg_defs[fsck_dotext].msg_txt);
				}
				UTF8_String_To_Unicode_String(Uni_Name,
							      ino_name,
							      JFS_NAME_MAX);
				intermed_rc =
				    direntry_add(lsfn_inoptr, this_ext->inonum,
						 Uni_Name);
				if (intermed_rc != FSCK_OK) {
					/* couldn't do it */
					if (intermed_rc < 0) {
						/* something fatal */
						rfsi_rc = intermed_rc;
					} else {
						/* not fatal */
						agg_recptr->lsfn_ok = 0;
						/*
						 * don't move this_ext to next extension record.  execute
						 * the loop for this one again so that it will be
						 * handled as one that can't be reconnected.
						 */
					}
				} else {
					/* entry added */
					if (inorecptr->inode_type ==
					    directory_inode) {
						lsfn_inoptr->di_nlink += 1;
						dir_inodes_reconnected += 1;
						fsck_send_msg(fsck_INOINLSFND,
							      inopfx,
							      this_ext->inonum);
					} else {
						/* not a directory inode */
						inodes_reconnected += 1;
						fsck_send_msg(fsck_INOINLSFNF,
							      inopfx,
							      this_ext->inonum);
					}
					this_ext = this_ext->next;
				}
			} else {
				/* /lost+found/ is not ok */
				if (inorecptr->inode_type == directory_inode) {
					dir_inodes_not_reconnected += 1;
					fsck_send_msg(fsck_MNCNTRCNCTINOD, inopfx,
						      this_ext->inonum);
				} else {
					/* not a directory inode */
					inodes_not_reconnected += 1;
					fsck_send_msg(fsck_MNCNTRCNCTINOF, inopfx,
						      this_ext->inonum);
				}
				this_ext = this_ext->next;
			}
		}
	}

	if ((rfsi_rc == FSCK_OK)
	    && ((dir_inodes_reconnected != 0) || (inodes_reconnected != 0))) {
		rfsi_rc = inode_put(lsfn_inoptr);
	}
	/*
	 * put out some summary messages in case we're not processing verbose
	 */
	if (rfsi_rc >= FSCK_OK) {
		if (dir_inodes_not_reconnected > 0) {
			if (dir_inodes_not_reconnected == 1) {
				fsck_send_msg(fsck_MNCNTRCNCTINOSD,
					      dir_inodes_not_reconnected);
			} else {
				fsck_send_msg(fsck_MNCNTRCNCTINOSDS,
					      dir_inodes_not_reconnected);
			}
		}
		if (dir_inodes_reconnected > 0) {
			if (dir_inodes_reconnected == 1) {
				fsck_send_msg(fsck_INOSINLSFND,
					      dir_inodes_reconnected);
			} else {
				fsck_send_msg(fsck_INOSINLSFNDS,
					      dir_inodes_reconnected);
			}
		}
		if (inodes_not_reconnected > 0) {
			if (inodes_not_reconnected == 1) {
				fsck_send_msg(fsck_MNCNTRCNCTINOSF,
					      inodes_not_reconnected);
			} else {
				fsck_send_msg(fsck_MNCNTRCNCTINOSFS,
					      inodes_not_reconnected);
			}
		}
		if (inodes_reconnected > 0) {
			if (inodes_reconnected == 1) {
				fsck_send_msg(fsck_INOSINLSFNF,
					      inodes_reconnected);
			} else {
				fsck_send_msg(fsck_INOSINLSFNFS,
					      inodes_reconnected);
			}
		}
	}
	return (rfsi_rc);
}

/*****************************************************************************
 * NAME: rebuild_dir_index
 *
 * FUNCTION: If the directory index was found to be bad, rebuild it from
 *           scratch.
 *
 * PARAMETERS:
 *      inoptr          - input - pointer to the directory inode in an fsck
 *                                buffer
 *      inorecptr       - input - pointer to an fsck inode record describing
 *                                the inode
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int rebuild_dir_index(struct dinode *inoptr,
		      struct fsck_inode_record *inorecptr)
{
	int64_t addr;
	struct dtreeQelem *current_Qel;
	int dtidx;
	dtpage_t *dtree;
	int n;
	int rc;
	int8_t *stbl;

	inoptr->di_next_index = 2;
	inoptr->di_size = 1;

	/* Recalculate di_nblocks */
	inoptr->di_nblocks = 0;
	if (inoptr->di_ea.flag == DXD_EXTENT)
		inoptr->di_nblocks += lengthDXD(&inoptr->di_ea);
	if (inoptr->di_acl.flag == DXD_EXTENT)
		inoptr->di_nblocks += lengthDXD(&inoptr->di_acl);

	rc = dtreeQ_get_elem(&current_Qel);
	if (rc)
		return rc;

	current_Qel->node_level = 0;
	dtree = (dtpage_t *)&inoptr->di_btroot;
	stbl = ((dtroot_t *)dtree)->header.stbl;
	addr = 0;

	while (current_Qel) {
		if (dtree->header.flag & BT_LEAF) {
			struct ldtentry *ldtptr;
			for (dtidx = 0; dtidx < dtree->header.nextindex;
			     dtidx++) {
				n = stbl[dtidx];
				ldtptr = (struct ldtentry *)&dtree->slot[n];
				/*
				 * TODO: insert entry into dir index table
				 * and add the real cookie here
				 */
				ldtptr->index = 0;
			}
			if (addr) {	/* non-root */
				ujfs_swap_dtpage_t(dtree, sb_ptr->s_flag);
				dtree->header.flag &= ~BT_SWAPPED;
				rc = ujfs_rw_diskblocks(Dev_IOPort,
						addr << sb_ptr->s_l2bsize,
						PSIZE, dtree, PUT);
				ujfs_swap_dtpage_t(dtree, sb_ptr->s_flag);
				dtree->header.flag |= BT_SWAPPED;
				if (rc)
					return rc;
			}
		} else {
			/* internal node */
			struct idtentry *idtptr;
			struct dtreeQelem *new_Qelptr;

			for (dtidx = 0; dtidx < dtree->header.nextindex;
			     dtidx++) {
				n = stbl[dtidx];
				idtptr = (struct idtentry *)&dtree->slot[n];
				rc = dtreeQ_get_elem(&new_Qelptr);
				if (rc)
					return rc;
				new_Qelptr->node_addr = addressPXD(&idtptr->xd);
				dtreeQ_enqueue(new_Qelptr);
			}
		}
		dtreeQ_rel_elem(current_Qel);
		dtreeQ_dequeue(&current_Qel);
		if (current_Qel) {
			inoptr->di_nblocks++;
			addr = current_Qel->node_addr;
			rc = dnode_get(addr, PSIZE, &dtree);
			if (rc)
				return rc;
			stbl = (int8_t *)&dtree->slot[dtree->header.stblindex];
		}
	}
	return FSCK_OK;
}
