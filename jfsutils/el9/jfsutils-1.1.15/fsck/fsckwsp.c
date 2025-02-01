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
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

/* defines and includes common among the fsck.jfs modules */
#include "xfsckint.h"
#include "devices.h"
#include "message.h"

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
 * fsck block map info structure and pointer
 *
 *      defined in xchkdsk.c
 */
extern struct fsck_bmap_record *bmap_recptr;

/* + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + +
 *
 * For message processing
 *
 *      defined in xchkdsk.c
 */
extern struct tm *fsck_DateTime;

extern char *Vol_Label;

extern UniChar uni_LSFN_NAME[];
extern UniChar uni_lsfn_name[];

/* + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + +
 *
 * Unicode path strings information.
 *
 *     values are assigned when the fsck aggregate record is initialized.
 *     accessed via addresses in the fack aggregate record.
 */
extern UniChar uni_LSFN_NAME[11];
extern UniChar uni_lsfn_name[11];

/* VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV
 *
 * The following are internal to this file
 *
 */

int alloc_wsp_extent(uint32_t, int);

void dupall_extract_blkrec(struct dupall_blkrec *);

struct dupall_blkrec *dupall_find_blkrec(int64_t, int64_t);

struct dupall_blkrec *dupall_get_blkrec(void);

int dupall_insert_blkrec(int64_t, int64_t, struct dupall_blkrec **);

int establish_wsp_block_map(void);

int extent_1stref_chk(int64_t, int64_t, int8_t, int8_t,
		      struct fsck_ino_msg_info *, struct fsck_inode_record *);

int extent_record_dupchk(int64_t, int64_t, int8_t, int8_t, int8_t,
			 struct fsck_ino_msg_info *,
			 struct fsck_inode_record *);

int fsblk_count_avail(uint32_t *, int32_t *, int32_t *, int32_t, int32_t *);

int fsblk_next_avail(uint32_t *, int32_t, int32_t, int32_t *, int32_t *, int *);

int inorec_agg_search(uint32_t, struct fsck_inode_record **);

int inorec_agg_search_insert(uint32_t, struct fsck_inode_record **);

int inorec_fs_search(uint32_t, struct fsck_inode_record **);

int inorec_fs_search_insert(uint32_t, struct fsck_inode_record **);

void locate_inode(uint32_t, int32_t *, int32_t *, int32_t *);

/*
 * The following are used for reporting storage related errors
 */
extern int wsp_dynstg_action;
extern int wsp_dynstg_object;

/* VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV */

/****************************************************************************
 * NAME: alloc_vlarge_buffer
 *
 * FUNCTION: Allocate the very large multi-use buffer
 *
 *
 * PARAMETERS:	none
 *
 * NOTES:	This must be called before logredo since the purpose
 *		is to ensure a buffer which has been obtained via
 *		malloc(), whether we're called from the command line
 *		or during autocheck.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int alloc_vlarge_buffer()
{
	int avb_rc = FSCK_OK;

	if ((agg_recptr->vlarge_buf_ptr =
	     (char *) malloc(VLARGE_BUFSIZE)) != NULL) {
		agg_recptr->vlarge_buf_length = VLARGE_BUFSIZE;
		agg_recptr->vlarge_current_use = NOT_CURRENTLY_USED;
	} else {
		avb_rc = ENOMEM;
		fsck_send_msg(MSG_OSO_INSUFF_MEMORY);
	}

	return (avb_rc);
}

/****************************************************************************
 * NAME: alloc_wrksp
 *
 * FUNCTION:  Allocates and initializes (to guarantee the storage is backed)
 *            dynamic storage for the caller.
 *
 * PARAMETERS:
 *      length         - input - the number of bytes of storage which are needed
 *      dynstg_object  - input - a constant (see xfsck.h) identifying the purpose
 *                               for which the storage is needed (Used in error
 *                               message if the request cannot be satisfied.
 *      addr_wrksp_ptr - input - the address of a variable in which this routine
 *                               will return the address of the dynamic storage
 *                               allocated for the caller
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int alloc_wrksp(uint32_t length, int dynstg_object, int for_logredo,
		void **addr_wrksp_ptr)
{
	int awsp_rc = FSCK_OK;
	char *wsp_ptr = NULL;
	struct wsp_ext_rec *this_fer;
	uint32_t bytes_avail;
	uint32_t min_length;

	*addr_wrksp_ptr = NULL;
	/* round up to an 8 byte boundary */
	min_length = ((length + 7) / 8) * 8;

	while ((wsp_ptr == NULL) && (awsp_rc == FSCK_OK)) {
		this_fer = agg_recptr->wsp_extent_list;

		while ((this_fer != NULL) && (wsp_ptr == NULL)
		       && (awsp_rc == FSCK_OK)) {
			if ((for_logredo) && !(this_fer->for_logredo)) {
				/*
				 * requestor is logredo and
				 * fer describes an allocation not for logredo
				 */
				this_fer = this_fer->next;
			} else {
				/* this fer describes an eligible allocation */
				bytes_avail =
				    this_fer->extent_length -
				    this_fer->last_byte_used;
				if (bytes_avail >= min_length) {
					/* there's enough here */
					wsp_ptr =
					    this_fer->extent_addr +
					    this_fer->last_byte_used + 1;
					this_fer->last_byte_used += min_length;
				} else {
					/* try the next fer */
					this_fer = this_fer->next;
				}
			}
		}

		if ((awsp_rc == FSCK_OK) && (wsp_ptr == NULL)) {
			/*
			 * nothing fatal but we didn't find the
			 * storage yet
			 */
			/*
			 * will allocate some number of memory segments
			 *  and put the fer describing it on the beginning
			 *  of the list.
			 */
			awsp_rc = alloc_wsp_extent(min_length, for_logredo);
		}
	}

	if (awsp_rc == FSCK_OK) {
		/* we allocated virtual storage */
		/*
		 * now initialize the storage
		 */
		memset((void *) wsp_ptr, 0, length);

		/* set the return value */
		*addr_wrksp_ptr = (void *) wsp_ptr;
	}
	return (awsp_rc);
}

/****************************************************************************
 * NAME: alloc_wsp_extent
 *
 * FUNCTION: Extend the workspace
 *
 *           For optimum use of storage, we'll allocate a whole segment.
 *           Then the alloc_wrksp routine portions it out as needed.
 *
 * PARAMETERS:
 *      minimum_length - input - minimum number of bytes of contiguous storage
 *                               needed
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int alloc_wsp_extent(uint32_t minimum_length, int for_logredo)
{
	int awe_rc = FSCK_OK;
	struct wsp_ext_rec *new_fer;
	size_t extent_length = MEMSEGSIZE;
	char *extent_addr = NULL;
	int8_t from_high_memory = 0;

	/*
	 * the user has specified the minimum needed.  We must allocate
	 * at least 16 more than that because we're going to use 16 bytes
	 * at the beginning to keep track of it.
	 */
	while (extent_length < (minimum_length + 16)) {
		extent_length += MEMSEGSIZE;
	}

	wsp_dynstg_object = dynstg_iobufs;
	wsp_dynstg_action = dynstg_allocation;

	extent_addr = (char *) malloc(extent_length);

	if (extent_addr == NULL) {
		/* allocation failure */
		awe_rc = FSCK_FAILED_DYNSTG_EXHAUST4;
		if (!for_logredo) {
			fsck_send_msg(fsck_EXHDYNSTG, wsp_dynstg_action,
				      dynstg_wspext);
		}
	} else {
		/* got the dynamic storage  */
		/*
		 * use the first 16 bytes of it to keep track of it
		 */
		new_fer = (struct wsp_ext_rec *) extent_addr;
		new_fer->extent_length = extent_length;
		new_fer->for_logredo = for_logredo;
		new_fer->from_high_memory = from_high_memory;
		new_fer->extent_addr = extent_addr;
		new_fer->last_byte_used = sizeof (struct wsp_ext_rec) - 1;

		new_fer->next = agg_recptr->wsp_extent_list;
		agg_recptr->wsp_extent_list = new_fer;
	}
	return (awe_rc);
}

/****************************************************************************
 * NAME: blkall_mark_free
 *
 * FUNCTION: Adjust the fsck workspace to show the indicated blocks are no
 *           longer used.
 *
 * PARAMETERS:
 *      first_block  - input - ordinal number of the first filesystem block
 *                             whose owner count is to be adjusted.
 *      last_block   - input - ordinal number of the last filesystem block
 *                             whose owner count is to be adjusted.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 *
 * NOTE:
 *	This could be written to be more efficient, but it's a big
 *	improvement over how it used to be.
 */
int blkall_mark_free(int64_t first_block, int64_t last_block)
{
	int64_t blk_num;
	int ddo_rc = FSCK_OK;
	int64_t page_num;
	int64_t last_page_num = -1;
	uint32_t word_offset;
	uint32_t bit_mask;
	struct fsck_blk_map_page *this_page;
	uint32_t *this_word;

	for (blk_num = first_block; blk_num <= last_block; blk_num++) {
		blkmap_find_bit(blk_num, &page_num, &word_offset, &bit_mask);
		if (page_num != last_page_num) {
			if (last_page_num != -1)
				blkmap_put_page(last_page_num);
			ddo_rc = blkmap_get_page(page_num, &this_page);
			if (ddo_rc)
				return ddo_rc;
			last_page_num = page_num;
		}
		this_word = (uint32_t *) ((char *) this_page + word_offset);
		/* mark it not allocated */
		(*this_word) &= ~bit_mask;
	}

	if (last_page_num != -1)
		blkmap_put_page(last_page_num);

	return (FSCK_OK);
}

/****************************************************************************
 * NAME: blkall_increment_owners
 *
 * FUNCTION: Adjust the fsck workspace to show one more owner for the
 *           indicated block.
 *
 * PARAMETERS:
 *      first_block  - input - ordinal number of the first filesystem block
 *                             whose owner count is to be adjusted.
 *      last_block   - input - ordinal number of the first filesystem block
 *                             whose owner count is to be adjusted.
 *	msg_info_ptr - input - information needed to issue messages for this
 *                             extent.  If NULL, no messages will be issued
 *
 * RETURNS:
 *      success: 0, or 1 if multiply-allocated blocks found
 *      failure: something less than 0
 */
int blkall_increment_owners(int64_t first_block,
			    int64_t last_block,
			    struct fsck_ino_msg_info *msg_info_ptr)
{
	int dio_rc;
	int64_t blk_num;
	int64_t page_num;
	int64_t last_page_num = -1;
	uint32_t word_offset;
	uint32_t bit_mask;
	struct fsck_blk_map_page *this_page;
	uint32_t *this_word;
	int is_a_dup = 0;
	int64_t first_in_dup_range = 0;
	int32_t size_of_dup_range = 0;

	for (blk_num = first_block; blk_num <= last_block; blk_num++) {
		blkmap_find_bit(blk_num, &page_num, &word_offset, &bit_mask);
		if (page_num != last_page_num) {
			if (last_page_num != -1)
				blkmap_put_page(last_page_num);
			dio_rc = blkmap_get_page(page_num, &this_page);
			if (dio_rc)
				return dio_rc;
			last_page_num = page_num;
		}
		this_word = (uint32_t *) ((char *) this_page + word_offset);

		if (((*this_word) & bit_mask) != bit_mask) {
			/*
			 * not allocated yet
			 */
			/* mark it allocated */
			(*this_word) |= bit_mask;

			/* Record previously found duplicate range */
			if (size_of_dup_range) {
				if (msg_info_ptr)
				    fsck_send_msg(fsck_DUPBLKREF,
					size_of_dup_range,
					(long long) first_in_dup_range,
					fsck_ref_msg(msg_info_ptr->msg_inotyp),
					fsck_ref_msg(msg_info_ptr->msg_inopfx),
					msg_info_ptr->msg_inonum);
				dio_rc = dupall_insert_blkrec(
						first_in_dup_range,
						first_in_dup_range +
							size_of_dup_range - 1,
						NULL);
				if (dio_rc)
					return dio_rc;
				agg_recptr->dup_block_count++;
				agg_recptr->unresolved_1stref_count++;
				size_of_dup_range = 0;
				first_in_dup_range = 0;
				is_a_dup = 1;
			}
		} else {
			/* already allocated */
			if (!size_of_dup_range++)
				first_in_dup_range = blk_num;
		}
	}

	if (last_page_num != -1)
		blkmap_put_page(last_page_num);

	/* Record duplicate range */
	if (size_of_dup_range) {
		if (msg_info_ptr)
		    fsck_send_msg(fsck_DUPBLKREF, size_of_dup_range,
				  (long long) first_in_dup_range,
				  fsck_ref_msg(msg_info_ptr->msg_inotyp),
				  fsck_ref_msg(msg_info_ptr->msg_inopfx),
				  msg_info_ptr->msg_inonum);
		dio_rc = dupall_insert_blkrec(first_in_dup_range,
				first_in_dup_range + size_of_dup_range - 1,
				NULL);
		if (dio_rc)
			return dio_rc;
		agg_recptr->dup_block_count++;
		agg_recptr->unresolved_1stref_count++;
		is_a_dup = 1;
	}

	return (is_a_dup);
}

/****************************************************************************
 * NAME: blkall_split_blkrec
 *
 * FUNCTION: Split the current duplicate block record
 *
 * PARAMETERS:
 *      blkrec        - input - duplicate block record to be split
 *      first_block   - input - first block of range whose intersection with
 *                              blkrec causes the split
 *      last_block    - input - last block of range whose intersection with
 *                              blkrec causes the split
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int blkall_split_blkrec(struct dupall_blkrec *blkrec,
		     int64_t first_block,
		     int64_t last_block)
{
	int bsb_rc = FSCK_OK;
	struct dupall_blkrec *new_blkrec;
	int64_t temp;

	if (blkrec->first_blk < first_block) {
		temp = blkrec->last_blk;
		blkrec->last_blk = first_block - 1;
		bsb_rc = dupall_insert_blkrec(first_block, temp, &new_blkrec);
		if (bsb_rc)
			return bsb_rc;
		new_blkrec->first_ref_resolved = blkrec->first_ref_resolved;
		new_blkrec->owner_count = blkrec->owner_count;
		agg_recptr->dup_block_count++;
		if (!new_blkrec->first_ref_resolved)
			agg_recptr->unresolved_1stref_count++;

		blkrec = new_blkrec;
	}

	if (blkrec->last_blk > last_block) {
		temp = blkrec->last_blk;
		blkrec->last_blk = last_block;
		bsb_rc = dupall_insert_blkrec(first_block, temp, &new_blkrec);
		if (bsb_rc)
			return bsb_rc;
		new_blkrec->first_ref_resolved = blkrec->first_ref_resolved;
		new_blkrec->owner_count = blkrec->owner_count;
		agg_recptr->dup_block_count++;
		if (!new_blkrec->first_ref_resolved)
			agg_recptr->unresolved_1stref_count++;
	}

	return FSCK_OK;
}

/****************************************************************************
 * NAME: dire_buffer_alloc
 *
 * FUNCTION:  Allocate an I/O buffer for use during directory entry insertion
 *            and removal processing.
 *
 * PARAMETERS:
 *      addr_dnode_ptr  - input - pointer to a variable in which to return
 *                                the address of the allocated buffer (or
 *                                NULL if no buffer could be allocated)
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int dire_buffer_alloc(dtpage_t ** addr_dnode_ptr)
{
	int rba_rc = FSCK_OK;
	uint32_t bufrec_length, bytes_available;
	struct recon_buf_record *bufrec_ptr;

	if (agg_recptr->recon_buf_stack != NULL) {
		/* stack not empty */
		bufrec_ptr = agg_recptr->recon_buf_stack;
		agg_recptr->recon_buf_stack = bufrec_ptr->stack_next;

		bufrec_ptr->stack_next = NULL;
		bufrec_ptr->dnode_blkoff = 0;
		bufrec_ptr->dnode_byteoff = 0;
		*addr_dnode_ptr = &(bufrec_ptr->dnode_buf);
	} else {
		/* the stack is empty */
		bufrec_length = sizeof (struct recon_buf_record);
		bytes_available = agg_recptr->recon_buf_extent->extent_length -
		    agg_recptr->recon_buf_extent->last_byte_used;

		if (bytes_available < bufrec_length) {
			/* we've used up a whole
			 * extent of dynamic storage -- something
			 * strange is going on
			 */
			*addr_dnode_ptr = NULL;
			rba_rc = FSCK_INSUFDSTG4RECON;
		} else {
			/* there is enough dynamic storage for another one */
			bufrec_ptr = (struct recon_buf_record *)
			    (agg_recptr->recon_buf_extent->extent_addr +
			     agg_recptr->recon_buf_extent->last_byte_used + 1);
			agg_recptr->recon_buf_extent->last_byte_used +=
			    bufrec_length;
			/*
			 * now initialize the record
			 */
			wsp_dynstg_object = dynstg_recondnodebuf;
			wsp_dynstg_action = dynstg_initialization;
			memset((void *) bufrec_ptr, 0, bufrec_length);
			*addr_dnode_ptr = &(bufrec_ptr->dnode_buf);
		}
	}
	return (rba_rc);
}

/****************************************************************************
 * NAME: dire_buffer_release
 *
 * FUNCTION:  Deallocate (make available for reuse) an I/O buffer for used
 *            during directory entry insertion and removal processing.
 *
 * PARAMETERS:
 *      dnode_ptr  - input - the address of the buffer to release
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int dire_buffer_release(dtpage_t * dnode_ptr)
{
	int rbr_rc = FSCK_OK;
	struct recon_buf_record *bufrec_ptr;

	bufrec_ptr = (struct recon_buf_record *) dnode_ptr;
	bufrec_ptr->stack_next = agg_recptr->recon_buf_stack;
	agg_recptr->recon_buf_stack = bufrec_ptr;

	return (rbr_rc);
}

/****************************************************************************
 * NAME: directory_buffers_alloc
 *
 * FUNCTION:  Allocate storage for use as I/O buffers while inserting and
 *            removing directory entries during file system repair processing.
 * FUNCTION: 	Make use of the VeryLarge Multi-Use Buffer for
 *		I/O buffers while inserting and removing directory entries
 *		during file system repair processing.
 *
 * NOTES:  	The directory buffers are the only use of the VeryLarge Buffer
 *		during Phase 6 processing.
 *
 * PARAMETERS:  none
 *
 * NOTES:  The address of the storage allocated for this purpose is stored
 *         in the aggregate record, field: recon_buf_extent
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int directory_buffers_alloc()
{
	int dba_rc = FSCK_OK;

	agg_recptr->vlarge_current_use = USED_FOR_DIRPAGE_BUFS;
	agg_recptr->recon_buf_extent =
	    (struct wsp_ext_rec *) agg_recptr->vlarge_buf_ptr;

	agg_recptr->recon_buf_extent->next = NULL;
	agg_recptr->recon_buf_extent->extent_length =
	    agg_recptr->vlarge_buf_length;
	agg_recptr->recon_buf_extent->extent_addr =
	    (char *) agg_recptr->recon_buf_extent;
	agg_recptr->recon_buf_extent->last_byte_used =
	    sizeof (struct wsp_ext_rec) - 1;

	return (dba_rc);
}

/****************************************************************************
 * NAME: directory_buffers_release
 *
 * FUNCTION:  Free storage which was allocated for use as I/O buffers while
 *            inserting and removing directory entries during file system
 *            repair processing.
 *
 * PARAMETERS:  none
 *
 * NOTES:  The address of the storage allocated for this purpose is stored
 *         in the aggregate record, field: recon_buf_extent
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int directory_buffers_release()
{
	int dbr_rc = FSCK_OK;

	if (agg_recptr->recon_buf_extent != NULL) {
		/* something is allocated */
		agg_recptr->recon_buf_extent = NULL;
		agg_recptr->vlarge_current_use = NOT_CURRENTLY_USED;
	}
	return (dbr_rc);
}

/****************************************************************************
 * NAME: dtreeQ_dequeue
 *
 * FUNCTION:  If the directory tree queue is not empty, remove the front
 *            element and return a pointer to it.  Otherwise, return NULL.
 *
 * PARAMETERS:
 *      dtreeQ_elptr - input - pointer to a variable in which the address of
 *                             the front queue element should be returned
 *
 * NOTES:  The directory tree queue is described in the aggregate record,
 *         fields: dtreeQ_front, dtreeQ_back
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int dtreeQ_dequeue(struct dtreeQelem **dtreeQ_elptr)
{
	int dQd_rc = FSCK_OK;

	*dtreeQ_elptr = agg_recptr->dtreeQ_front;

	if (agg_recptr->dtreeQ_back == agg_recptr->dtreeQ_front) {
		/* empty */
		agg_recptr->dtreeQ_back = agg_recptr->dtreeQ_front = NULL;
	} else {
		/* not empty */
		agg_recptr->dtreeQ_front = agg_recptr->dtreeQ_front->next;
		agg_recptr->dtreeQ_front->prev = NULL;
	}
	return (dQd_rc);
}

/****************************************************************************
 * NAME: dtreeQ_enqueue
 *
 * FUNCTION:  Adds the given element to the back of the directory tree queue.
 *
 * PARAMETERS:
 *      dtreeQ_elptr - input - address of the element to add to the queue.
 *
 * NOTES:  The directory tree queue is described in the aggregate record,
 *         fields: dtreeQ_front, dtreeQ_back
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int dtreeQ_enqueue(struct dtreeQelem *dtreeQ_elptr)
{
	int dQe_rc = FSCK_OK;

	if (agg_recptr->dtreeQ_back == NULL) {
		/* empty queue */
		agg_recptr->dtreeQ_back = agg_recptr->dtreeQ_front =
		    dtreeQ_elptr;
		dtreeQ_elptr->prev = dtreeQ_elptr->next = NULL;
	} else {
		/* queue not empty */
		dtreeQ_elptr->next = NULL;
		dtreeQ_elptr->prev = agg_recptr->dtreeQ_back;
		agg_recptr->dtreeQ_back->next = dtreeQ_elptr;
		agg_recptr->dtreeQ_back = dtreeQ_elptr;
	}

	return (dQe_rc);
}

/****************************************************************************
 * NAME: dtreeQ_get_elem
 *
 * FUNCTION: Allocates workspace storage for an fsck directory tree queue element
 *
 * PARAMETERS:
 *      addr_dtreeQ_ptr  - input - pointer to a variable in which the address
 *                                 of the new element should be returned.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int dtreeQ_get_elem(struct dtreeQelem **addr_dtreeQ_ptr)
{
	int dge_rc = FSCK_OK;
	int I_am_logredo = 0;

	if (agg_recptr->free_dtreeQ != NULL) {
		/* free list isn't empty */
		*addr_dtreeQ_ptr = agg_recptr->free_dtreeQ;
		agg_recptr->free_dtreeQ = agg_recptr->free_dtreeQ->next;
		memset((void *) (*addr_dtreeQ_ptr), 0, dtreeQ_elem_length);
	} else {
		/* else the free list is empty */
		dge_rc = alloc_wrksp(dtreeQ_elem_length, dynstg_dtreeQ_elem,
				     I_am_logredo, (void **) addr_dtreeQ_ptr);
	}

	return (dge_rc);
}

/****************************************************************************
 * NAME: dtreeQ_rel_elem
 *
 * FUNCTION: Makes an fsck directory tree queue element available for reuse
 *
 * PARAMETERS:
 *      dtreeQ_elptr  - input - the address of the element to release
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int dtreeQ_rel_elem(struct dtreeQelem *dtreeQ_elptr)
{
	int dQre_rc = FSCK_OK;

	dtreeQ_elptr->next = agg_recptr->free_dtreeQ;
	agg_recptr->free_dtreeQ = dtreeQ_elptr;

	return (dQre_rc);
}

/****************************************************************************
 * NAME: dupall_extract_blkrec
 *
 * FUNCTION: Remove the given (previously found) record from the list of
 *           duplicate allocation block records.
 *
 * PARAMETERS:
 *      block_recptr  - input - the address of the record to remove
 *
 * NOTES:  The duplicate allocation list is described in the aggregate record,
 *         field: dup_alloc_lst
 */
void dupall_extract_blkrec(struct dupall_blkrec *block_recptr)

{
	/*
	 * remove it from the list of multiply-allocated block
	 */

	if (block_recptr->prev)
		block_recptr->prev->next = block_recptr->next;
	else	/* first in list */
		agg_recptr->dup_alloc_lst = block_recptr->next;

	if (block_recptr->next)
		block_recptr->next->prev = block_recptr->prev;

	/*
	 * release it for reuse
	 */
	block_recptr->next = agg_recptr->free_dupall_blkrec;
	agg_recptr->free_dupall_blkrec = block_recptr;
}

/****************************************************************************
 * NAME: dupall_find_blkrec
 *
 * FUNCTION:  Search for a record with the given block number in the duplicate
 *            allocation list.
 *
 * PARAMETERS:
 *      first_block  - input - ordinal number of the first filesystem block
 *      last_block  - input - ordinal number of the last filesystem block
 *
 * NOTES:  The duplicate allocation list is described in the aggregate record,
 *         field: dup_alloc_lst
 *
 * RETURNS:
 *      success: record found
 *      failure: NULL
 */
struct dupall_blkrec *dupall_find_blkrec(int64_t first_block,
					 int64_t last_block)
{
	struct dupall_blkrec *this_blkrec;

	for (this_blkrec = agg_recptr->dup_alloc_lst;
	     this_blkrec && this_blkrec->first_blk <= last_block;
	     this_blkrec = this_blkrec->next) {
		if (this_blkrec->last_blk >= first_block)
			return this_blkrec;
	}
	return NULL;
}

/****************************************************************************
 * NAME: dupall_get_blkrec
 *
 * FUNCTION: Allocates workspace storage for a duplicate allocation
 *           block record.
 *
 * PARAMETERS:
 *      none
 *
 * RETURNS:
 *      success: address of record
 *      failure: NULL
 */
struct dupall_blkrec *dupall_get_blkrec(void)
{
	struct dupall_blkrec *addr_blkrec_ptr;
	int dgb_rc = FSCK_OK;

	if (agg_recptr->free_dupall_blkrec != NULL) {
		/* free list isn't empty */
		addr_blkrec_ptr = agg_recptr->free_dupall_blkrec;
		agg_recptr->free_dupall_blkrec = addr_blkrec_ptr->next;
	} else {
		/* else the free list is empty */
		dgb_rc = alloc_wrksp(dupall_blkrec_length, dynstg_dupall_blkrec,
				     0, (void **) &addr_blkrec_ptr);
	}
	if (dgb_rc == FSCK_OK)
		return addr_blkrec_ptr;

	return NULL;
}

/*****************************************************************************
 * NAME: dupall_insert_blkrec
 *
 * FUNCTION: Allocate a duplicate allocation record for the given blocks and
 *           insert it into the sorted, doubly-linked list of duplicate
 *           allocation records.
 *
 * PARAMETERS:
 *      first_block - input - the first block number for which the record is
 *                            to be allocated.
 *      last_block - input - the last block number for which the record is
 *                           to be allocated.
 *
 * NOTES:  The duplicate allocation list is described in the aggregate record,
 *         field: dup_alloc_lst
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int dupall_insert_blkrec(int64_t first_block, int64_t last_block,
			 struct dupall_blkrec **blkrec)
{
	struct dupall_blkrec *new_blkrec;
	struct dupall_blkrec *prev_blkrec;
	struct dupall_blkrec *this_blkrec;
	int dib_done = 0;

	new_blkrec = dupall_get_blkrec();

	if (new_blkrec == NULL)
		return FSCK_FAILED_DYNSTG_EXHAUST1;

	if (blkrec)
		*blkrec = new_blkrec;

	/* got a block record */
	new_blkrec->first_blk = first_block;
	new_blkrec->last_blk = last_block;
	new_blkrec->owner_count = 2;

	if (agg_recptr->dup_alloc_lst == NULL) {
		/* list now empty */
		new_blkrec->next = NULL;
		new_blkrec->prev = NULL;
		agg_recptr->dup_alloc_lst = new_blkrec;

		return FSCK_OK;
	}

	/* list not empty */
	if (agg_recptr->dup_alloc_lst->first_blk > first_block) {
		/*
		 * goes at front
		 */
		new_blkrec->next = agg_recptr->dup_alloc_lst;
		new_blkrec->prev = NULL;
		agg_recptr->dup_alloc_lst = new_blkrec;

		return FSCK_OK;
	}

	/* doesn't go at the front */
	prev_blkrec = agg_recptr->dup_alloc_lst;
	this_blkrec = agg_recptr->dup_alloc_lst->next;
	while (!dib_done) {
		if (this_blkrec == NULL) {
			/* goes at the end */
			new_blkrec->next = NULL;
			new_blkrec->prev = prev_blkrec;
			prev_blkrec->next = new_blkrec;
			dib_done = 1;
		} else if (this_blkrec->first_blk > first_block) {
			/*
			 * goes in front of this one
			 */
			new_blkrec->next = this_blkrec;
			new_blkrec->prev = prev_blkrec;
			prev_blkrec->next = new_blkrec;
			this_blkrec->prev = new_blkrec;
			dib_done = 1;
		} else {
			/* try the next one */
			prev_blkrec = this_blkrec;
			this_blkrec = this_blkrec->next;
		}
	}
	return FSCK_OK;
}

/****************************************************************************
 * NAME: establish_agg_workspace
 *
 * FUNCTION: Obtain storage for and initialize the fsck aggregate workspace.
 *
 * PARAMETERS:  none
 *
 * NOTES: The various parts of the workspace are described in the aggregate
 *        record.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int establish_agg_workspace()
{
	int eaw_rc = FSCK_OK;
	uint32_t mapsize_bytes;
	int I_am_logredo = 0;

	/*
	 * establish the fsck workspace block map
	 */
	eaw_rc = establish_wsp_block_map();

	if (eaw_rc == FSCK_OK) {
		/* block map has been established */
		agg_recptr->agg_imap.num_iags = 1;
		agg_recptr->agg_imap.bkd_inodes = INOSPEREXT;
		agg_recptr->agg_imap.unused_bkd_inodes = INOSPEREXT - 4;
		/* in release 1 there is always
		 * exactly one extent of inodes allocated
		 * for the aggregate
		 */
		agg_recptr->inode_count = INOSPEREXT;
		/*
		 * now establish the fsck aggregate imap workspace
		 */
		mapsize_bytes =
		    agg_recptr->agg_imap.num_iags *
		    sizeof (struct fsck_iag_record);
		eaw_rc =
		    alloc_wrksp(mapsize_bytes, dynstg_agg_iagtbl, I_am_logredo,
				(void **) &(agg_recptr->agg_imap.iag_tbl));
		if (eaw_rc == FSCK_OK) {
			/* AIM workspace established */
			/*
			 * now establish the fsck aggregate inode table workspace
			 *
			 * (since there is always exactly one inode extent, we don't
			 * bother with an IAG table of pointers to extent address tables
			 * or with an extent address table of pointers to inode record
			 * address tables.)
			 */
			eaw_rc =
			    alloc_wrksp(inode_tbl_length, dynstg_ait_inotbl,
					I_am_logredo,
					(void **) &(agg_recptr->AIT_ext0_tbl));
			memcpy((void *) &(agg_recptr->AIT_ext0_tbl->eyecatcher),
			       (void *) "InodeTbl", 8);
		}
	}
	return (eaw_rc);
}

/****************************************************************************
 * NAME: establish_ea_iobuf
 *
 * FUNCTION: 	Make use of the VeryLarge Multi-Use Buffer
 *		for reading and validating EA data.
 *
 * PARAMETERS:  none
 *
 * NOTES:  	The ea I/O buffer is the only user of the VeryLarge Buffer
 *		during Phase 1 processing.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int establish_ea_iobuf()
{
	int eei_rc = FSCK_OK;

	agg_recptr->vlarge_current_use = USED_FOR_EA_BUF;
	agg_recptr->ea_buf_ptr = agg_recptr->vlarge_buf_ptr;

	wsp_dynstg_action = dynstg_initialization;
	memset((void *) (agg_recptr->ea_buf_ptr), '\0', EA_IO_BUFSIZE);

	agg_recptr->ea_buf_length = agg_recptr->vlarge_buf_length;
	agg_recptr->ea_buf_data_len = 0;
	agg_recptr->ea_agg_offset = 0;

	return (eei_rc);
}

/****************************************************************************
 * NAME: establish_fs_workspace
 *
 * FUNCTION: Obtain storage for and initialize the fsck file sets workspace.
 *
 * PARAMETERS:  none
 *
 * NOTES: The various parts of the workspace are described in the aggregate
 *        record.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int establish_fs_workspace()
{
	int efsw_rc = FSCK_OK;
	uint32_t mapsize_bytes;
	uint32_t buffer_size;
	int aggregate_inode, which_ait = 0;
	uint32_t inoidx;
	struct dinode *inoptr;
	int I_am_logredo = 0;
	struct IAG_tbl_t *IAGtbl;
	struct inode_ext_tbl_t *inoexttbl;
	struct inode_tbl_t *inotbl;

	/*
	 * allocate a buffer in which path names can be constructed
	 */
	buffer_size = (JFS_PATH_MAX + 2) * sizeof (char);
	efsw_rc = alloc_wrksp(buffer_size, dynstg_fsit_map,
			      I_am_logredo,
			      (void **) &(agg_recptr->path_buffer));

	if (efsw_rc == FSCK_OK) {
		/* got it */
		agg_recptr->path_buffer_length = buffer_size;
		/*
		 * Figure out how many IAGs have been allocated for the fileset.
		 * (Note that in release 1 there is always exactly 1 fileset in the
		 * aggregate)
		 *
		 * At this point the aggregate inode describing the fileset has been
		 * validated.  The data described by that inode is 1 page of control
		 * information plus some number of IAGs.  di_size is the number of
		 * bytes allocated for that data.
		 */
		if (agg_recptr->primary_ait_4part2) {
			which_ait = fsck_primary;
			efsw_rc = ait_special_read_ext1(fsck_primary);
			if (efsw_rc != FSCK_OK) {
				/* read failed */
				report_readait_error(efsw_rc,
						     FSCK_FAILED_CANTREADAITEXTC,
						     fsck_primary);
				efsw_rc = FSCK_FAILED_CANTREADAITEXTC;
			}
		} else {
			which_ait = fsck_secondary;
			efsw_rc = ait_special_read_ext1(fsck_secondary);
			if (efsw_rc != FSCK_OK) {
				/* read failed */
				report_readait_error(efsw_rc,
						     FSCK_FAILED_CANTREADAITEXTD,
						     fsck_secondary);
				efsw_rc = FSCK_FAILED_CANTREADAITEXTD;
			}
		}
	}
	if (efsw_rc != FSCK_OK)
		goto efsw_exit;

	/* got the first AIT extent */
	aggregate_inode = -1;
	inoidx = FILESYSTEM_I;
	efsw_rc = inode_get(aggregate_inode, which_ait, inoidx, &inoptr);

	if (efsw_rc != FSCK_OK)
		goto efsw_exit;

	/* got the fileset IT inode */
	agg_recptr->fset_imap.num_iags =
	    (inoptr->di_size / SIZE_OF_MAP_PAGE) - 1;
	/*
	 * a high estimate of the inodes
	 * allocated for the fileset
	 */
	agg_recptr->fset_inode_count =
	    agg_recptr->fset_imap.num_iags * INOSPERIAG;
	/*
	 * now establish the fsck fileset imap workspace
	 */
	if (efsw_rc != FSCK_OK)
		goto efsw_exit;

	/* inode map established */
	mapsize_bytes = agg_recptr->fset_imap.num_iags *
	    sizeof (struct fsck_iag_record);
	efsw_rc = alloc_wrksp(mapsize_bytes, dynstg_agg_iagtbl, I_am_logredo,
			      (void **) &(agg_recptr->fset_imap.iag_tbl));
	if (efsw_rc != FSCK_OK)
		goto efsw_exit;

	/* inode map workspace allocated */
	/*
	 * now establish the fsck fileset imap workspace
	 *
	 * We start out knowing that IAG 0, extent 0 is allocated and
	 * has an inode in use.  We'll allocate enough to cover that.
	 */
	mapsize_bytes = 8 + agg_recptr->fset_imap.num_iags *
	    sizeof (struct inode_ext_tbl_t *);
	efsw_rc = alloc_wrksp(mapsize_bytes, dynstg_fsit_iagtbl,
			      I_am_logredo, (void **) &IAGtbl);
	if (efsw_rc != FSCK_OK)
		goto efsw_exit;

	/* we got the IAG table */
	memcpy((void *)&(IAGtbl->eyecatcher), (void *) "FSAITIAG", 8);
	agg_recptr->FSIT_IAG_tbl = IAGtbl;
	efsw_rc = alloc_wrksp(inode_ext_tbl_length, dynstg_fsit_inoexttbl,
			      I_am_logredo, (void **) &inoexttbl);
	if (efsw_rc != FSCK_OK)
		goto efsw_exit;

	/* we got the inode extent table */
	memcpy((void *)&(inoexttbl->eyecatcher), (void *)"FSAITEXT", 8);
	IAGtbl->inoext_tbl[0] = inoexttbl;
	efsw_rc = alloc_wrksp(inode_tbl_length, dynstg_fsit_inotbl,
			      I_am_logredo, (void **) &inotbl);
	if (efsw_rc == FSCK_OK) {
		/* we got the inode table */
		memcpy((void *)&(inotbl->eyecatcher), (void *)"FSAITINO", 8);
		inoexttbl->inotbl[0] = inotbl;
	}

      efsw_exit:
	return (efsw_rc);
}

/****************************************************************************
 * NAME: establish_io_buffers
 *
 * FUNCTION:  Allocate storage for dedicated I/O buffers.
 *
 * PARAMETERS:  none
 *
 * NOTES:  The I/O buffers are described in the aggregate record.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int establish_io_buffers()
{
	int eiob_rc = FSCK_OK;
	int I_am_logredo = 0;

	eiob_rc = alloc_wrksp(IAG_IO_BUFSIZE, dynstg_iobufs,
			      I_am_logredo,
			      (void **) &(agg_recptr->iag_buf_ptr));
	if (eiob_rc == FSCK_OK) {
		/* successful IAG allocation */
		agg_recptr->iag_buf_length = sizeof (struct iag);
		agg_recptr->iag_buf_data_len = 0;
		agg_recptr->iag_agg_offset = 0;
		agg_recptr->iag_buf_write = 0;
		agg_recptr->bmapdm_buf_ptr = agg_recptr->iag_buf_ptr;
		agg_recptr->bmapdm_buf_length = IAG_IO_BUFSIZE;
		agg_recptr->bmapdm_buf_data_len = 0;
		agg_recptr->bmapdm_agg_offset = 0;
		agg_recptr->bmapdm_buf_write = 0;
	}
	if (eiob_rc == FSCK_OK) {
		/* successful IAG allocation */
		eiob_rc = alloc_wrksp(INODE_IO_BUFSIZE, dynstg_iobufs,
				      I_am_logredo,
				      (void **) &(agg_recptr->ino_buf_ptr));
	}
	if (eiob_rc == FSCK_OK) {
		/* successful inode allocation */
		agg_recptr->ino_buf_length = INODE_IO_BUFSIZE;
		agg_recptr->ino_buf_data_len = 0;
		agg_recptr->ino_buf_agg_offset = 0;
		agg_recptr->ino_buf_write = 0;

		eiob_rc = alloc_wrksp(NODE_IO_BUFSIZE, dynstg_iobufs,
				      I_am_logredo,
				      (void **) &(agg_recptr->node_buf_ptr));
	}
	if (eiob_rc == FSCK_OK) {
		/* successful node allocation */
		agg_recptr->node_buf_length = NODE_IO_BUFSIZE;
		agg_recptr->node_buf_data_len = 0;
		agg_recptr->node_agg_offset = 0;
		agg_recptr->node_buf_write = 0;

		eiob_rc = alloc_wrksp(NODE_IO_BUFSIZE, dynstg_iobufs,
				      I_am_logredo,
				      (void **) &(agg_recptr->dnode_buf_ptr));
	}
	if (eiob_rc == FSCK_OK) {
		/* successful dnode allocation */
		agg_recptr->dnode_buf_length = NODE_IO_BUFSIZE;
		agg_recptr->dnode_buf_data_len = 0;
		agg_recptr->dnode_agg_offset = 0;
		agg_recptr->dnode_buf_write = 0;

		eiob_rc = alloc_wrksp(MAPLEAF_IO_BUFSIZE, dynstg_iobufs,
				      I_am_logredo,
				      (void **) &(agg_recptr->mapleaf_buf_ptr));
	}
	if (eiob_rc == FSCK_OK) {
		/* successful mapleaf allocation */
		agg_recptr->mapleaf_buf_length = MAPLEAF_IO_BUFSIZE;
		agg_recptr->mapleaf_buf_data_len = 0;
		agg_recptr->mapleaf_agg_offset = 0;
		agg_recptr->mapleaf_buf_write = 0;

		eiob_rc = alloc_wrksp(MAPCTL_IO_BUFSIZE, dynstg_iobufs,
				      I_am_logredo,
				      (void **) &(agg_recptr->mapctl_buf_ptr));
	}
	if (eiob_rc == FSCK_OK) {
		/* successful map control allocation */
		agg_recptr->mapctl_buf_length = MAPCTL_IO_BUFSIZE;
		agg_recptr->mapctl_buf_data_len = 0;
		agg_recptr->mapctl_agg_offset = 0;
		agg_recptr->mapctl_buf_write = 0;
		eiob_rc = alloc_wrksp(BMAPLV_IO_BUFSIZE, dynstg_iobufs,
				      I_am_logredo,
				      (void **) &(agg_recptr->bmaplv_buf_ptr));
	}
	if (eiob_rc == FSCK_OK) {
		/* successful map level allocation */
		agg_recptr->bmaplv_buf_length = BMAPLV_IO_BUFSIZE;
		agg_recptr->bmaplv_buf_data_len = 0;
		agg_recptr->bmaplv_agg_offset = 0;
		agg_recptr->bmaplv_buf_write = 0;
	}
	return (eiob_rc);
}

/****************************************************************************
 * NAME: establish_wsp_block_map
 *
 * FUNCTION: If the in-aggregate fsck workspace is available for use,
 *           initialize it as the fsck workspace block map.
 *
 *           Otherwise, obtain and initialize dynamic storage for the fsck
 *           workspace block map.
 *
 * PARAMETERS:  none
 *
 * NOTES:  The fsck workspace block map is described in the aggregate record.
 *
 *         If the in-aggregate fsck workspace is used, the aggregate record
 *         fields describe the dedicated I/O buffer used for the fsck workspace
 *         block map.  Otherwise, they describe the entire workspace block
 *         map in dynamic storage.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int establish_wsp_block_map()
{
	int ewbm_rc = FSCK_OK;
	int64_t blkmap_size_bytes;
	int64_t blkmap_size_in_pages;
	int64_t idx;
	int64_t this_device_offset;

	int I_am_logredo = 0;

	ewbm_rc = establish_wsp_block_map_ctl();

	if (ewbm_rc != FSCK_OK)
		goto ewbm_exit;

	/* allocated and initialized blk map ctl page */
	blkmap_size_bytes = agg_recptr->ondev_wsp_byte_length;
	agg_recptr->blkmp_pagecount = blkmap_size_bytes / BYTESPERPAGE;
	/*
	 * whether or not we actually write to the on-disk
	 * fsck workspace, this buffer represents it logically.
	 */
	agg_recptr->blkmp_agg_offset =
	    agg_recptr->ondev_wsp_byte_offset + BYTESPERPAGE;
	agg_recptr->blkmp_blkmp_offset = 0;
	agg_recptr->blkmp_buf_data_len = 0;
	agg_recptr->blkmp_buf_write = 0;
	agg_recptr->blkmp_blkmp_offset = 0;
	agg_recptr->blkmp_buf_data_len = 0;
	agg_recptr->blkmp_buf_write = 0;

	if (agg_recptr->processing_readonly) {
		/* can't touch the aggregate */
		ewbm_rc = alloc_wrksp(blkmap_size_bytes, dynstg_blkmap,
				      I_am_logredo,
				      (void **) &(agg_recptr->blkmp_buf_ptr));
		if (ewbm_rc == FSCK_OK) {
			/* allocated and initialized block map */
			wsp_dynstg_object = 0;
			wsp_dynstg_action = 0;
			agg_recptr->blkmp_buf_length = blkmap_size_bytes;
			agg_recptr->blkmp_buf_data_len =
			    agg_recptr->blkmp_buf_length;
		}
		goto ewbm_exit;
	}
	/* use storage reserved for fsck in the aggregate */
	ewbm_rc = alloc_wrksp(BLKMP_IO_BUFSIZE, dynstg_blkmap_buf, I_am_logredo,
			      (void **) &(agg_recptr->blkmp_buf_ptr));
	if (ewbm_rc != FSCK_OK)
		goto ewbm_exit;

	/* allocated and initialized block map */
	agg_recptr->blkmp_buf_length = BLKMP_IO_BUFSIZE;
	agg_recptr->blkmp_buf_data_len = agg_recptr->blkmp_buf_length;
	ewbm_rc = blkmap_put_ctl_page(agg_recptr->blkmp_ctlptr);
	if (ewbm_rc != FSCK_OK)
		goto ewbm_exit;

	/* successful write to Block Map Control Page */
	/* this is guaranteed (by mkfs) to be an even number */
	blkmap_size_in_pages = blkmap_size_bytes / BYTESPERPAGE;
	/* Swap to little-endian */
	ujfs_swap_fsck_blk_map_page(agg_recptr->blkmp_buf_ptr);
	for (idx = 1; ((idx < blkmap_size_in_pages) &&
	     (ewbm_rc == FSCK_OK)); idx++) {
		/* for each map page (after the control page) */
		this_device_offset = agg_recptr->ondev_wsp_byte_offset +
		    (idx * BYTESPERPAGE);
		/*
		 * write the initialized buffer page to
		 * the map page on disk
		 */
		ewbm_rc = ujfs_rw_diskblocks(Dev_IOPort, this_device_offset,
				    BYTESPERPAGE,
				    (void *) agg_recptr->blkmp_buf_ptr, PUT);
		if (ewbm_rc != FSCK_OK) {
			/* I/O failure */
			/*
			 * message to user
			 */
			fsck_send_msg(fsck_URCVWRT, fsck_ref_msg(fsck_metadata),
				      Vol_Label, 1);
			/*
			 * message to debugger
			 */
			fsck_send_msg(fsck_ERRONWSP, ewbm_rc, ewbm_rc, fsck_WRITE,
				      (long long)this_device_offset,
				      BYTESPERPAGE, -1);
		}
	}
	/* Swap back to cpu-endian */
	ujfs_swap_fsck_blk_map_page(agg_recptr->blkmp_buf_ptr);

      ewbm_exit:
	return (ewbm_rc);
}

/****************************************************************************
 * NAME: establish_wsp_block_map_ctl
 *
 * FUNCTION: If the in-aggregate fsck workspace is available for use,
 *           initialize the first page as the fsck workspace block map control
 *           page.
 *
 *           Otherwise, obtain and initialize dynamic storage for the fsck
 *           workspace block map control page.
 *
 * PARAMETERS:  none
 *
 * NOTES:  The fsck workspace block map is described in the aggregate record.
 *
 *         If the in-aggregate fsck workspace is used, the aggregate record
 *         fields describe the dedicated I/O buffer used for the fsck workspace
 *         block map.  Otherwise, they describe the entire workspace block
 *         map in dynamic storage.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int establish_wsp_block_map_ctl()
{
	int ewbmc_rc = FSCK_OK;
	time_t Current_Time;
	int64_t this_device_offset = 0;
	int I_am_logredo = 0;

	ewbmc_rc =
	    alloc_wrksp(sizeof (struct fsck_blk_map_hdr), dynstg_blkmap_hdr,
			I_am_logredo, (void **) &(agg_recptr->blkmp_ctlptr));

	if (ewbmc_rc == FSCK_OK) {
		/* allocated and initialized blk map ctl page */
		/* fill eyecatcher */
		strncpy(agg_recptr->blkmp_ctlptr->hdr.eyecatcher,
			fbmh_eyecatcher_string, strlen(fbmh_eyecatcher_string));
		agg_recptr->blkmp_ctlptr->hdr.super_buff_addr = (char *) sb_ptr;
		agg_recptr->blkmp_ctlptr->hdr.agg_record_addr =
		    (char *) agg_recptr;
		agg_recptr->blkmp_ctlptr->hdr.bmap_record_addr =
		    (char *) bmap_recptr;
		agg_recptr->blkmp_ctlptr->hdr.fscklog_full =
		    agg_recptr->fscklog_full;
		agg_recptr->blkmp_ctlptr->hdr.fscklog_buf_allocated =
		    agg_recptr->fscklog_buf_allocated;
		agg_recptr->blkmp_ctlptr->hdr.fscklog_buf_alloc_err =
		    agg_recptr->fscklog_buf_alloc_err;
		agg_recptr->blkmp_ctlptr->hdr.fscklog_agg_offset =
		    agg_recptr->ondev_fscklog_byte_offset;

		Current_Time = time(NULL);
		fsck_DateTime = localtime(&Current_Time);
		sprintf(&(agg_recptr->blkmp_ctlptr->hdr.start_time[0]),
			"%d/%d/%d %d:%02d:%02d", fsck_DateTime->tm_mon + 1,
			fsck_DateTime->tm_mday,
			(fsck_DateTime->tm_year + 1900),
			fsck_DateTime->tm_hour, fsck_DateTime->tm_min,
			fsck_DateTime->tm_sec);

		if (!(agg_recptr->processing_readonly)) {
			/*
			 * use storage reserved for fsck in the
			 * aggregate
			 */
			ewbmc_rc =
			    blkmap_put_ctl_page(agg_recptr->blkmp_ctlptr);
			if (ewbmc_rc != FSCK_OK) {
				/* I/O failure */
				/*
				 * message to user
				 */
				fsck_send_msg(fsck_URCVWRT, fsck_ref_msg(fsck_metadata),
					      Vol_Label, 1);
				/*
				 * message to debugger
				 */
				fsck_send_msg(fsck_ERRONWSP, ewbmc_rc, ewbmc_rc, fsck_WRITE,
					      (long long) this_device_offset,
					      BYTESPERPAGE, -1);
			}
		}
	}
	return (ewbmc_rc);
}

/*****************************************************************************
 * NAME: extent_1stref_chk
 *
 * FUNCTION:  Determine whether the given extent contains the first reference
 *            to a multiply-allocated block.  If it does, perform duplicate
 *            allocation processing on the owning inode.
 *
 * PARAMETERS:
 *      first_block   - input - ordinal number of the first block in the extent
 *                              to check
 *      last_block    - input - ordinal number of the last block in the extent
 *                              to check
 *      is_EA         - input - !0 => the extent contains an inode's EA
 *                               0 => the extent contains something else
 *      msg_info_ptr  - input - pointer to a data area containing information
 *                              needed to issue messages for this extent
 *      ino_recptr    - input - pointer to the fsck inode record describing the
 *                              inode to which this extent is allocated
 *
 * NOTES:  As fsck scans the inodes sequentially, recording the blocks
 *         allocated, it doesn't know a particular block is multiply-allocated
 *         until the second reference is detected.  At that time the first
 *         reference to the block is unresolved since no list of owners is
 *         built (only a count of owners, in which a 1 in the bit map
 *         represents a count of 1).
 *
 *         After all inodes have been scanned and their block allocations
 *         recorded, if any multiply-allocated blocks have been detected, the
 *         inodes are scanned sequentially again until all first references to
 *         multiply-allocated blocks are resolved.  This routine is invoked
 *         during that rescan.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int extent_1stref_chk(int64_t first_block,
		      int64_t last_block,
		      int8_t is_EA,
		      int8_t is_ACL,
		      struct fsck_ino_msg_info *msg_info_ptr,
		      struct fsck_inode_record *ino_recptr)
{
	int eq_rc = FSCK_OK;
	uint32_t size_of_dup_range = 0;
	int dups_detected = 0;
	struct dupall_blkrec *blkrec;

	while (agg_recptr->unresolved_1stref_count) {
		blkrec = dupall_find_blkrec(first_block, last_block);
		if (!blkrec)
			break;
		if (blkrec->first_ref_resolved) {
			if (blkrec->last_blk <= last_block) {
				first_block = blkrec->last_blk + 1;
				continue;
			}
			break;
		}
		/*
		 * If the record goes outside this extent, we need to split
		 * it, since we are only interested in the intersection of
		 * the record and this extent
		 */
		if ((blkrec->first_blk < first_block) ||
		    (blkrec->last_blk > last_block)) {
			eq_rc = blkall_split_blkrec(blkrec, first_block,
						    last_block);
			if (eq_rc)
				return eq_rc;
			/*
			 * Check if the split caused the current record to
			 * precede the extent we are processing
			 */
			if (blkrec->first_blk < first_block)
				continue;
		}
		dups_detected = 1;
		blkrec->first_ref_resolved = 1;
		agg_recptr->unresolved_1stref_count--;

		if (msg_info_ptr) {
			size_of_dup_range =
				blkrec->last_blk - blkrec->first_blk + 1;
			fsck_send_msg(fsck_DUPBLKREF, size_of_dup_range,
				      (long long) blkrec->first_blk,
				      fsck_ref_msg(msg_info_ptr->msg_inotyp),
				      fsck_ref_msg(msg_info_ptr->msg_inopfx),
				      msg_info_ptr->msg_inonum);
		}
	}

	if (dups_detected && msg_info_ptr) {
		ino_recptr->involved_in_dups = 1;
		fsck_send_msg(fsck_DUPBLKREFS,
			      fsck_ref_msg(msg_info_ptr->msg_inopfx),
			      msg_info_ptr->msg_inonum);
		if (!(inode_is_metadata(ino_recptr))) {
			agg_recptr->corrections_needed = 1;
			if (is_EA)
				ino_recptr->clr_ea_fld = 1;
			else if (is_ACL)
				ino_recptr->clr_acl_fld = 1;
			else
				ino_recptr->selected_to_rls = 1;
		}
	}
	return (FSCK_OK);
}

/****************************************************************************
 * NAME: blkall_ref_check
 *
 * FUNCTION: Determine whether the given blocks are multiply-allocated and, if
 *           so, whether the first reference to it is unresolved.  (In this
 *           case, the current reference must be the first reference.)
 *
 * PARAMETERS:
 *      first_block - input - ordinal number of the first filesystem block to
 *                            be checked
 *      last_block  - input - ordinal number of the last filesystem block to
 *                            be checked
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int blkall_ref_check(int64_t first_block, int64_t last_block)
{
	return extent_1stref_chk(first_block, last_block, 0, 0, 0, 0);
}

/*****************************************************************************
 * NAME: extent_record_dupchk
 *
 * FUNCTION: Validate that the block number in the given extent are valid
 *           within the range valid for the aggregate (after the reserved
 *           space and fixed metadata and before the fsck workspace),
 *           record that the blocks are allocated, determine whether any
 *           prior allocations of the blocks have been recorded and, if
 *           so, perform duplicate allocation processing on the owning inode.
 *
 * PARAMETERS:
 *      first_block   - input - ordinal number of the first block in the extent
 *                              to check
 *      last_block    - input - ordinal number of the last block in the extent
 *                              to check
 *      is_EA         - input - !0 => the extent contains an inode's EA
 *                               0 => the extent contains something else
 *	msg_info_ptr  - input - information needed to issue messages for this
 *                              extent.  If NULL, no messages will be issued
 *      ino_recptr    - input - pointer to the fsck inode record describing the
 *                              inode to which this extent is allocated
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int extent_record_dupchk(int64_t first_block,
			 int64_t last_block,
			 int8_t range_adj,
			 int8_t is_EA,
			 int8_t is_ACL,
			 struct fsck_ino_msg_info *msg_info_ptr,
			 struct fsck_inode_record *ino_recptr)
{
	int erd_rc = FSCK_OK;
	int dups_detected = 0;
	struct dupall_blkrec *this_blkrec;

	if (range_adj) {
		/* the xad described an invalid range */
		fsck_send_msg(fsck_BADBLKNO,
			      fsck_ref_msg(msg_info_ptr->msg_dxdtyp),
			      fsck_ref_msg(msg_info_ptr->msg_inotyp),
			      fsck_ref_msg(msg_info_ptr->msg_inopfx),
			      msg_info_ptr->msg_inonum);
	}

	/*
	 * Look for duplicate blocks already detected for this extent
	 */
	while (first_block <= last_block) {
		this_blkrec = dupall_find_blkrec(first_block, last_block);
		if (!this_blkrec)
			break;

		/*
		 * If this record goes beyond the extent, we need to split it.
		 */
		if ((this_blkrec->first_blk < first_block) ||
		    (this_blkrec->last_blk > last_block)) {
			erd_rc = blkall_split_blkrec(this_blkrec, first_block,
						     last_block);
			if (erd_rc)
				return erd_rc;
			/*
			 * Check if the split caused the current record to
			 * precede the extent we are processing
			 */
			if (this_blkrec->first_blk < first_block)
				continue;
		}

		/*
		 * Take care of the blocks preceding this record (if any)
		 */
		if (first_block < this_blkrec->first_blk) {
			erd_rc = blkall_increment_owners(first_block,
						this_blkrec->first_blk - 1,
						msg_info_ptr);
			if (erd_rc < 0)
				return erd_rc;
			else if (erd_rc)
				dups_detected = 1;
		}

		this_blkrec->owner_count++;
		first_block = this_blkrec->last_blk + 1;
	}

	/*
	 * Take care of any remaining blocks
	 */
	if (first_block <= last_block) {
		erd_rc = blkall_increment_owners(first_block, last_block,
						 msg_info_ptr);
		if (erd_rc < 0)
			return erd_rc;
		else if (erd_rc)
			dups_detected = 1;
	}
	if (dups_detected && msg_info_ptr) {
		/* claims at least 1 multiply allocated block */
		ino_recptr->involved_in_dups = 1;
		fsck_send_msg(fsck_DUPBLKREFS,
			      fsck_ref_msg(msg_info_ptr->msg_inopfx),
			      msg_info_ptr->msg_inonum);
		if (!(inode_is_metadata(ino_recptr))) {
			agg_recptr->corrections_needed = 1;
			if (is_EA)
				ino_recptr->clr_ea_fld = 1;
			else if (is_ACL)
				ino_recptr->clr_acl_fld = 1;
			else
				ino_recptr->selected_to_rls = 1;
		}
	}
	return (FSCK_OK);
}

/*****************************************************************************
 * NAME: extent_record
 *
 * FUNCTION:  Record that each of the blocks in the given extent is allocated
 *            to some inode.
 *
 * PARAMETERS:
 *      first_block   - input - ordinal number of the first block in the extent
 *                              to check
 *      last_block    - input - ordinal number of the last block in the extent
 *                              to check
 *
 * NOTES:  Originally, this was intended to be a streamlined version of
 *         extent_record_dupchk, called only when the extent was known to
 *         not be multilply allocated.  However, it really wasn't any more
 *         efficient, so its simpler to just call extent_record_dupchk here.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int extent_record(int64_t first_block, int64_t last_block)
{
	return extent_record_dupchk(first_block, last_block, 0, 0, 0, 0, 0);
}

/*****************************************************************************
 * NAME: extent_unrecord
 *
 * FUNCTION:  Decrement, in the fsck workspace block record, the owner count
 *            for each block in the given extent.
 *
 * PARAMETERS:
 *      first_block   - input - ordinal number of the first block in the extent
 *                              to check
 *      last_block    - input - ordinal number of the last block in the extent
 *                              to check
 *
 * NOTES:  Under certain circumstances, it is necessary to back out the record
 *         of an inode's ownership the recording of some extent already verified
 *         valid.
 *
 *         This function could be accomplished using other routines which
 *         include extent validation code, but, for performance reasons,
 *         this streamlined routine exists to minimize processing time.
 *
 *         Examples of these circumstances include:
 *          o Storage allocated for an inode's EA is valid, but the B+Tree
 *            rooted in the inode is structurally corrupt.  Then the portions
 *            of the tree which were recorded before the corruption was detected
 *            were backed out using routines which include validation code, and
 *            finally this routine is called to 'unrecord' the storage allocated
 *            for the EA.
 *
 *          o The B+Tree rooted in an inode was verified structurally correct,
 *            but the di_nblocks in the inode was found to be inconsistent with
 *            the tree.  In this case we assume the tree to be corrupt and
 *            back it out of the fsck workspace block map immediately.  This
 *            routine would be used for that purpose since the tree has already
 *            been verified structurally correct.
 *
 *          o An inode has been found to be valid, but claims ownership of at
 *            least one block claimed by another inode. At least one of the
 *            inodes is actually damaged.  The user has given permission to
 *            delete this inode, and so we need to decrement the number of
 *            owners for each block allocated to this inode.  This routine
 *            would be used for that purpose since the tree has already been
 *            verified structurally correct.
 *
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int extent_unrecord(int64_t first_block, int64_t last_block)
{
	int eu_rc = FSCK_OK;
	struct dupall_blkrec *this_blkrec;

	while (first_block <= last_block) {
		this_blkrec = dupall_find_blkrec(first_block, last_block);
		if (!this_blkrec)
			break;

		/*
		 * If this record goes beyond the extent, we need to split it.
		 */
		if ((this_blkrec->first_blk < first_block) ||
		    (this_blkrec->last_blk > last_block)) {
			eu_rc = blkall_split_blkrec(this_blkrec, first_block,
						    last_block);
			if (eu_rc)
				return eu_rc;
			/*
			 * Check if the split caused the current record to
			 * precede the extent we are processing
			 */
			if (this_blkrec->first_blk < first_block)
				continue;
		}

		/*
		 * Take care of the blocks preceding this record (if any)
		 */
		if (first_block < this_blkrec->first_blk) {
			eu_rc = blkall_mark_free(first_block,
						 this_blkrec->first_blk - 1);
			if (eu_rc)
				return eu_rc;
		}

		this_blkrec->owner_count--;
		if (this_blkrec->owner_count == 1) {
			/* No longer a duplicate */
			if (!this_blkrec->first_ref_resolved)
				agg_recptr->unresolved_1stref_count--;
			agg_recptr->dup_block_count--;

			dupall_extract_blkrec(this_blkrec);
		}
		first_block = this_blkrec->last_blk + 1;
	}

	/*
	 * Take care of any remaining blocks
	 */
	if (first_block <= last_block)
		eu_rc = blkall_mark_free(first_block, last_block);

	return (eu_rc);
}

/****************************************************************************
 * NAME: fsblk_count_avail
 *
 * FUNCTION:  Count the number of contiguous aggregate blocks which are
 *            available, according to the fsck workspace block map, starting
 *            with the given (available) aggregate block.
 *
 * PARAMETERS:
 *      wspbits     - input - pointer to a page in the fsck workspace block map
 *      wordidx     - input - the ordinal number, in the page pointed to by
 *                            wspbits, of the word containing the bit representing
 *                            some particular aggregate block.
 *                            at routine entry: the block represented is the available
 *                                              block with which counting should start
 *                            at routine exit: the block represented is the 1st block
 *                                             AFTER the last block counted
 *      bitidx      - input - the ordinal number, in the word identified by wordidx,
 *                            of the bit representing some particular aggregate block.
 *                            at routine entry: the block represented is the available
 *                                              block with which counting should start
 *                            at routine exit: the block represented is the 1st block
 *                                             AFTER the last block counted
 *      num_wanted  - input - number of blocks wanted.  (i.e., when to stop counting
 *                            even if the run of contiguous, available blocks has
 *                            not ended.
 *      num_avail   - input - number of contiguous, available blocks counted starting
 *                            with the block described by wspbits, wordidx, and
 *                            bitidx when the routine was entered.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int fsblk_count_avail(uint32_t * wspbits,
		      int32_t * wordidx,
		      int32_t * bitidx, int32_t num_wanted, int32_t * num_avail)
{
	int fbca_rc = FSCK_OK;
	int done_counting = 0;
	uint32_t bitmask;

	*num_avail = 0;
	while (((*wordidx) < LPERDMAP) && (!done_counting)) {
		bitmask = 0x80000000u;
		bitmask = bitmask >> (*bitidx);
		while (((*bitidx) < DBWORD) && (!done_counting)) {
			if (wspbits[*wordidx] & bitmask) {
				/* this one's not available */
				done_counting = -1;
			} else {
				/* this one's available */
				(*num_avail)++;
				if ((*num_avail) == num_wanted) {
					done_counting = -1;
				} else {
					bitmask = bitmask >> 1;
				}
				(*bitidx)++;
			}
		}		/* end while bitidx */
		if (!done_counting) {
			*bitidx = 0;
			*wordidx += 1;
		}
	}

	return (fbca_rc);
}

/****************************************************************************
 * NAME: fsblk_next_avail
 *
 * FUNCTION:  Find the next available aggregate block, according to the
 *            fsck workspace block map, starting with the given block.
 *
 * PARAMETERS:
 *      wspbits     - input - pointer to a page in the fsck workspace block map
 *      startword   - input - the ordinal number, in the page pointed to by
 *                            wspbits, of the word containing the bit representing
 *                            the aggregate block at which to start searching
 *      startbit    - input - the ordinal number, in the word identified by wordidx,
 *                            of the bit representing the aggregate block at which
 *                            to start searching
 *      foundword   - input - the ordinal number, in the page pointed to by
 *                            wspbits, of the word containing the bit representing
 *                            the available aggregate block found, if any
 *      foundbit    - input - the ordinal number, in the word identified by wordidx,
 *                            of the bit representing the available aggregate block,
 *                            if any
 *      block_found - input - pointer to a variable in which the search results are
 *                            returned.  !0 => an available block was found in the
 *                                             given page at/after the specified start
 *                                        0 => no available block was found in the
 *                                             given page at/after the specified start
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int fsblk_next_avail(uint32_t * wspbits,
		     int32_t startword,
		     int32_t startbit,
		     int32_t * foundword, int32_t * foundbit, int *block_found)
{
	int fbna_rc = FSCK_OK;
	int32_t wordidx, bitidx, firstbit;
	uint32_t bitmask;
	uint32_t mask_all_on = 0xFFFFFFFFu;

	*block_found = 0;
	firstbit = startbit;
	for (wordidx = startword; ((wordidx < LPERDMAP) && (!(*block_found)));
	     wordidx++) {
		if (wspbits[wordidx] != mask_all_on) {
			/* a zero in this map word */
			bitmask = 0x80000000u;
			bitmask = bitmask >> firstbit;
			for (bitidx = firstbit;
			     ((bitidx < DBWORD) && (!(*block_found)));
			     bitidx++) {
				if (!(wspbits[wordidx] & bitmask)) {
					/* it's available */
					*foundword = wordidx;
					*foundbit = bitidx;
					*block_found = -1;
				} else {
					/* it's in use */
					bitmask = bitmask >> 1;
				}
			}
		}
		firstbit = 0;
	}
	return (fbna_rc);
}

/****************************************************************************
 * NAME: fsck_alloc_fsblks
 *
 * FUNCTION: Allocate storage in the aggregate.
 *
 * PARAMETERS:
 *      blocks_wanted  - input - the number of contiguous blocks of storage
 *                               wanted
 *      blocks         - input - pointer to a variable in which the ordinal
 *                               number of the first block allocated will be
 *                               returned  (or 0 if the storage cannot be
 *                               allocated)
 *
 * NOTES: o This routine is only called when fsck has write access to the
 *          aggregate.
 *
 *        o This routine can not be called before the end of Phase 1 (that is,
 *          not before all block allocations existing in the aggregate have
 *          been recorded in the fsck workspace block map).
 *
 *        o The optimum time to call this routine is after all inode repairs
 *          have been performed (a step performed in Phase 6) since aggregates
 *          blocks may be made available by releasing inodes and/or clearing
 *          extents allocated for EAs.
 *
 *        o This routine can not be called after the beginning of Phase 8 (that
 *          is, not after fsck begins to rebuild the aggregate block map from
 *          the information in the fsck workspace block map).
 *
 *        o Currently, this routine is only called
 *           - during inode reconnect processing (the last step in Phase 6) to
 *              create new internal nodes for the directory to which the inode(s)
 *              is(are) reconnected.
 *           - during replication of the Aggregate Inode Map (Phase 7) when
 *              building the tree for the fileset AIM inode.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int fsck_alloc_fsblks(int32_t blocks_wanted, int64_t * blocks)
{
	int fafsb_rc = FSCK_OK;
	int64_t wsp_pagenum = 0;
	struct fsck_blk_map_page *wsp_page = NULL;
	int32_t this_word = 0, this_bit = 0;
	int32_t found_word = -1, found_bit = -1;
	int found_a_block;
	int32_t blocks_found = 0;
	uint32_t *wsp_bits;

	*blocks = 0;

	while ((wsp_pagenum < agg_recptr->blkmp_pagecount) &&
	       (blocks_found != blocks_wanted) && (fafsb_rc == FSCK_OK)) {
		fafsb_rc = blkmap_get_page(wsp_pagenum, &wsp_page);
		if (fafsb_rc == FSCK_OK) {
			/* got a page */
			wsp_bits = (uint32_t *) wsp_page;
			this_word = 0;
			this_bit = 0;
			found_a_block = 0;
			blocks_found = 0;
			fafsb_rc =
			    fsblk_next_avail(wsp_bits, this_word, this_bit,
					     &found_word, &found_bit,
					     &found_a_block);
			while ((found_a_block) && (fafsb_rc == FSCK_OK)
			       && (blocks_found != blocks_wanted)) {
				this_word = found_word;
				this_bit = found_bit;
				blocks_found = 0;

				fafsb_rc =
				    fsblk_count_avail(wsp_bits, &found_word,
						      &found_bit, blocks_wanted,
						      &blocks_found);
				if (fafsb_rc == FSCK_OK) {
					/* nothing bizarre happened */
					if (blocks_found == blocks_wanted) {
						/* success! */
						*blocks =
						    (wsp_pagenum <<
						     log2BITSPERPAGE) +
						    (this_word <<
						     log2BITSPERDWORD) +
						    this_bit;
					} else {
						/* word containing 1st 1 after the zeroes */
						this_word = found_word;
						/* bit postion in that word */
						this_bit = found_bit;
						found_a_block = 0;
						fafsb_rc =
						    fsblk_next_avail(wsp_bits,
								     this_word,
								     this_bit,
								     &found_word,
								     &found_bit,
								     &found_a_block);
					}
				}
			}
			if ((fafsb_rc == FSCK_OK) && (!found_a_block)) {
				/* no avail block found */
				/* maybe in the next page */
				wsp_pagenum++;
			}
		}
	}

	if (fafsb_rc == FSCK_OK) {
		/* nothing fatal along the way */
		if ((*blocks) == 0) {
			/* didn't find the blocks */
			fafsb_rc = FSCK_BLKSNOTAVAILABLE;
			fsck_send_msg(fsck_EXHFILSYSSTG, blocks_wanted);
		} else if ((*blocks) > agg_recptr->highest_valid_fset_datablk) {
			/*
			 * the first available blocks were in the work area
			 */
			*blocks = 0;
			fafsb_rc = FSCK_BLKSNOTAVAILABLE;
			fsck_send_msg(fsck_EXHFILSYSSTG, blocks_wanted);
		} else {
			/* we found the requested blocks */
			/*
			 * allocate these blocks for the caller
			 * by marking them 'in use' in the fsck
			 * workspace block map
			 */
			fafsb_rc =
			    extent_record(*blocks,
					  (*blocks + blocks_wanted - 1));
		}
	}
	return (fafsb_rc);
}

/****************************************************************************
 * NAME: fsck_dealloc_fsblks
 *
 * FUNCTION: Deallocate storage in the aggregate.
 *
 * PARAMETERS:
 *      blk_length  - input - the number of contiguous aggregate blocks
 *                            to release
 *      first_blk   - input - the ordinal number of the first allocated
 *                            aggregate block to release
 *
 * NOTES: o This routine is only called when fsck has write access to the
 *          aggregate.
 *
 *        o This routine can only be called to release blocks whose allocation
 *          has already been recorded in the fsck workspace block map.
 *
 *        o This routine can not be called after the beginning of Phase 8 (that
 *          is, not after fsck begins to rebuild the aggregate block map from
 *          the information in the fsck workspace block map).
 *
 *        o Currently, this routine is only called during Phase 6, during inode
 *          repair processing to release internal nodes from a directory while
 *          removing a directory entry and during reconnect processing to back
 *          out partial processing for directory entry add which cannot be
 *          completed because of a storage allocation failure.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int fsck_dealloc_fsblks(int32_t blk_length, int64_t first_blk)
{
	int fdfsb_rc = FSCK_OK;

	/*
	   * deallocate these blocks by marking
	   * them 'available' in the fsck workspace
	   * block map
	 */
	fdfsb_rc = extent_unrecord(first_blk, (first_blk + blk_length - 1));

	return (fdfsb_rc);
}

/****************************************************************************
 * NAME: fscklog_end
 *
 * FUNCTION:  Put the last buffer in the fsck service log.  Cleanup the
 *            first record in any remaining pages in this service log
 *            on the device so the log reader will know when to stop.
 *
 * PARAMETERS:  none
 *
 * NOTES:     If we don't have write access then there's nothing to do.
 *
 * RETURNS:
 *      success: FSCK_OK
 */
int fscklog_end()
{
	int fle_rc = FSCK_OK;
	int32_t buffer_bytes_left;
	/*
	 * If there's a partial buffer, write it to the device
	 */
	if (agg_recptr->fscklog_buf_data_len > 0) {
		/*
		 * there's something in there to be written
		 */
		buffer_bytes_left = agg_recptr->fscklog_buf_length -
		    agg_recptr->fscklog_buf_data_len;
		agg_recptr->fscklog_last_msghdr->entry_length +=
		    __le16_to_cpu((int16_t) buffer_bytes_left);
		fscklog_put_buffer();
	}
	/* disable logging for the duration */
	agg_recptr->fscklog_buf_allocated = 0;

	return (fle_rc);
}

/****************************************************************************
 * NAME: fscklog_init
 *
 * FUNCTION:  Initialize the current fsck service log
 *
 * PARAMETERS:  none
 *
 * NOTES:	- If we have write access to the aggregate, write
 *		   nulls over all pages in the log so that garbage
 *		   from a prior run does not end up appended to the
 *		   current service log.
 *
 *		- This MUST be called before logredo since logredo
 *		   will write messages to the fsck service log.
 *
 * RETURNS:
 *      success: FSCK_OK
 */
int fscklog_init()
{
	int fli_rc = FSCK_OK;
	int64_t log_bytes_left;
	struct fsck_blk_map_page *tmpbuf_addr = NULL;
	struct fsck_blk_map_page *tmp_buf_ptr;
	uint32_t tmp_buf_data_len;
	int64_t tmp_agg_offset;
	int64_t tmp_log_offset;

	if (agg_recptr->processing_readwrite) {
		/* have write access */
		/*
		 * this is safe because we do it before calling logredo
		 */
		tmpbuf_addr =
		    (struct fsck_blk_map_page *) malloc(FSCKLOG_BUFSIZE);

		if (tmpbuf_addr == NULL) {
			/* didn't get the space */
			/* log this fact so that any residual messages will be ignored */
			fsck_send_msg(fsck_CANTINITSVCLOG);
		} else {
			/* temp buffer allocated */
			agg_recptr->initializing_fscklog = 1;
			memset((void *) tmpbuf_addr, 0, FSCKLOG_BUFSIZE);
			/*
			 * save the current fscklog values
			 */
			tmp_buf_ptr = agg_recptr->fscklog_buf_ptr;
			tmp_buf_data_len = agg_recptr->fscklog_buf_data_len;
			tmp_agg_offset = agg_recptr->fscklog_agg_offset;
			tmp_log_offset = agg_recptr->fscklog_log_offset;
			/*
			 * store values to describe the temp buffer
			 */
			agg_recptr->fscklog_buf_ptr = tmpbuf_addr;
			agg_recptr->fscklog_buf_data_len = FSCKLOG_BUFSIZE;

			log_bytes_left =
			    (agg_recptr->ondev_fscklog_byte_length / 2) -
			    agg_recptr->fscklog_log_offset;
			while (log_bytes_left >= agg_recptr->fscklog_buf_length) {
				fscklog_put_buffer();
				log_bytes_left =
				    (agg_recptr->ondev_fscklog_byte_length /
				     2) - agg_recptr->fscklog_log_offset;
			}	/* end while */

			free((void *) tmpbuf_addr);
			/*
			 * restore the actual fscklog values
			 */
			agg_recptr->fscklog_buf_ptr = tmp_buf_ptr;
			agg_recptr->fscklog_buf_data_len = tmp_buf_data_len;
			agg_recptr->fscklog_agg_offset = tmp_agg_offset;
			agg_recptr->fscklog_log_offset = tmp_log_offset;

			agg_recptr->initializing_fscklog = 0;
		}
	}
	return (fli_rc);
}

/****************************************************************************
 * NAME: fscklog_start
 *
 * FUNCTION:  Allocate an I/O buffer and log the fsck start.
 *
 * PARAMETERS:  none
 *
 * NOTES:     Even if we don't have write access to the
 *            aggregate we will do all parts of logging except
 *            actually writing to the disk.  This is to
 *            provide diagnostic information if a dump is
 *            taken during fsck execution.
 *
 * RETURNS:
 *      success: FSCK_OK
 */
int fscklog_start()
{
	int fls_rc = FSCK_OK;
	int iml_rc = FSCK_OK;
	int I_am_logredo = 0;

	iml_rc = alloc_wrksp(FSCKLOG_BUFSIZE, dynstg_iobufs,
			     I_am_logredo,
			     (void **) &(agg_recptr->fscklog_buf_ptr));

	if (iml_rc == FSCK_OK) {
		/* successful fsck service log buffer alloc */
		agg_recptr->fscklog_buf_length = FSCKLOG_BUFSIZE;
		agg_recptr->fscklog_buf_data_len = 0;
		agg_recptr->fscklog_log_offset = 0;
		agg_recptr->fscklog_full = 0;
		agg_recptr->fscklog_buf_allocated = -1;
		agg_recptr->fscklog_buf_alloc_err = 0;
	} else {
		agg_recptr->fscklog_buf_allocated = 0;
		agg_recptr->fscklog_buf_alloc_err = -1;
	}

	return (fls_rc);
}

/****************************************************************************
 * NAME: get_inode_extension
 *
 * FUNCTION: Allocates workspace storage for an fsck inode extension record
 *
 * PARAMETERS:
 *      inoext_ptr  - input - pointer to a variable in which the address of the
 *                            new inode extension will be returned.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int get_inode_extension(struct fsck_inode_ext_record **inoext_ptr)
{
	int gir_rc = FSCK_OK;
	int I_am_logredo = 0;

	if (agg_recptr->free_inode_extens != NULL) {
		*inoext_ptr = agg_recptr->free_inode_extens;
		agg_recptr->free_inode_extens =
		    agg_recptr->free_inode_extens->next;
		memset(*inoext_ptr, 0, sizeof (struct fsck_inode_ext_record));
	} else {
		gir_rc = alloc_wrksp(sizeof (struct fsck_inode_ext_record),
				     dynstg_inoextrec, I_am_logredo,
				     (void **) inoext_ptr);
	}

	return (gir_rc);
}

/****************************************************************************
 * NAME: get_inorecptr
 *
 * FUNCTION: Return a pointer to the fsck inode record describing the
 *           specified inode.
 *
 *           If no such record exists, then if allocation is specified,
 *           allocate one, insert it into the fsck workspace inode record
 *           structures, and return the address of the new record.  Otherwise,
 *           (no such record exists but allocation was not specified) return
 *           NULL.
 *
 * PARAMETERS:
 *      is_aggregate    - input - !0 => the requested inode is owned by the aggregate
 *                                 0 => the requested inode is owned by the fileset
 *      alloc           - input - !0 => do allocate a record if none has yet been
 *                                      allocated for the inode
 *                                 0 => do not allocate a record if none has been
 *                                      allocated for the inode
 *      inonum          - input - ordinal number of the inode whose fsck inode
 *                                record is wanted
 *      addr_inorecptr  - input - pointer to a variable in which the address of
 *                                of the found (or newly allocated) record will
 *                                be returned
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int get_inorecptr(int is_aggregate,
		  int alloc,
		  uint32_t inonum, struct fsck_inode_record **addr_inorecptr)
{
	int gir_rc = FSCK_OK;

	if (is_aggregate) {
		/* for an aggregate inode */
		if (alloc) {
			/* request is to allocate if not found */
			gir_rc =
			    inorec_agg_search_insert(inonum, addr_inorecptr);
		} else {
			/* search only */
			gir_rc = inorec_agg_search(inonum, addr_inorecptr);
		}
	} else {
		/* for a fileset inode */
		if (alloc) {
			/* request is to allocate if not found */
			gir_rc =
			    inorec_fs_search_insert(inonum, addr_inorecptr);
		} else {
			/* search only */
			gir_rc = inorec_fs_search(inonum, addr_inorecptr);
		}
	}

	return (gir_rc);
}

/****************************************************************************
 * NAME: get_inorecptr_first
 *
 * FUNCTION: Return a pointer to the fsck inode record describing the inode
 *           which has the lowest ordinal number of all allocated inodes in
 *           the specified group (i.e., either of all allocated aggregate
 *           inodes or of all allocated fileset inodes).
 *
 *           Initialize the balanced binary sort tree header record for a
 *           sequential traversal of the nodes in the tree.
 *
 * PARAMETERS:
 *      is_aggregate    - input - !0 => the requested inode is owned by the aggregate
 *                                 0 => the requested inode is owned by the fileset
 *      inonum          - input - pointer to a variable in which to return the ordinal
 *                                number of the inode whose fsck inode record address
 *                                is being returned in addr_inorecptr
 *      addr_inorecptr  - input - pointer to a variable in which the address of
 *                                of the fsck inode record will be returned
 *
 * NOTES: o This routine should not be called before the end of phase 1 during
 *          which an fsck inode record is allocated to describe each inode in
 *          the aggregate (both those owned by the aggregate and those owned by
 *          the fileset in the aggregate).
 *
 *         o At entry to this routine, if the nodes in the specified balanced binary
 *           sort tree have not yet been linked into a sorted list, this list is
 *           created for the specified balanced binary sort tree.
 *
 *         o The fsck balanced binary sort tree header record contains the
 *           fields describing the sorted list of nodes in the tree, which
 *           are used to initialize a traversal and to remember the current
 *           list position of the traversal.
 *
 *         o Currently, this routine is only called for the balanced binary sort tree
 *           containing fsck inode records describing fileset owned inodes.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int get_inorecptr_first(int is_aggregate,
			uint32_t * inonum,
			struct fsck_inode_record **addr_inorecptr)
{
	int girf_rc = FSCK_OK;
	int32_t iagidx, extidx, inoidx;
	struct inode_ext_tbl_t *inoexttbl;
	struct inode_tbl_t *inotbl;

	/*
	 * find first active aggregate inode record
	 */
	if (is_aggregate) {	/* for an aggregate inode */
		if (agg_recptr->AIT_ext0_tbl == NULL) {
			girf_rc = FSCK_INTERNAL_ERROR_68;

			fsck_send_msg(fsck_INTERNALERROR, girf_rc, 0, 0, 0);
		} else {
			/*
			 * the first allocated aggregate inode is inode 1,
			 * by definition.
			 */
			inoidx = 1;
			*addr_inorecptr =
			    agg_recptr->AIT_ext0_tbl->inorectbl[inoidx];
			agg_recptr->agg_last_inoidx = inoidx;
		}

		/*
		 * find first active fileset inode record
		 */
	} else {
		/* for a fileset inode */
		if (agg_recptr->FSIT_IAG_tbl == NULL) {
			girf_rc = FSCK_INTERNAL_ERROR_69;
			fsck_send_msg(fsck_INTERNALERROR, girf_rc, 0, 0, 0);
		} else {
			/* the table is allocated */
			/*
			 * IAG 0 is always the first active IAG in the
			 * fileset and extent 0 is always the first active
			 * extent since inode 2 (the root dir) is always
			 * the first active inode.
			 */
			iagidx = 0;
			extidx = 0;
			inoidx = 2;

			inoexttbl =
			    agg_recptr->FSIT_IAG_tbl->inoext_tbl[iagidx];
			inotbl = inoexttbl->inotbl[extidx];
			*addr_inorecptr = inotbl->inorectbl[inoidx];
			/*
			 * set things up for find next
			 */
			agg_recptr->fs_last_iagidx = iagidx;
			agg_recptr->fs_last_extidx = extidx;
			agg_recptr->fs_last_inoidx = inoidx;
		}
	}

	if (girf_rc == FSCK_OK) {
		/* got one */
		*inonum = (*addr_inorecptr)->inonum;
	}
	return (girf_rc);
}

/****************************************************************************
 * NAME: get_inorecptr_next
 *
 * FUNCTION: Return a pointer to the fsck inode record describing the inode
 *           which has the ordinal number greater than the inode record most
 *           recently returned in the current sequential traversal, and which
 *           describes the lowest ordinal inode number of all records not yet
 *           returned in the current traversal.
 *
 *           That is, return a pointer to the fsck inode record describing
 *           the 'next' inode in the group (i.e., either next among the
 *           aggregate-owned inodes or next among the fileset-owned inodes).
 *
 * PARAMETERS:
 *      is_aggregate    - input - !0 => the requested inode is owned by the aggregate
 *                                 0 => the requested inode is owned by the fileset
 *      inonum          - input - pointer to a variable in which to return the ordinal
 *                                number of the inode whose fsck inode record address
 *                                is being returned in addr_inorecptr
 *      addr_inorecptr  - input - pointer to a variable in which the address of
 *                                of the fsck inode record will be returned.  If
 *                                the most recently returned (in the current
 *                                traversal) record described the last inode in
 *                                the group, NULL is returned.
 *
 * NOTES:  o This routine is called iteratively in order to traverse the
 *           inodes in the specified group in ascending key order efficiently.
 *
 *         o EACH sequential traversal of the inodes in a group (ie inodes
 *           owned by the aggregate or inodes owned by the fileset) MUST be
 *           initialized by an invocation of routine get_inorecptr_first.
 *
 *         o The fsck balanced binary sort tree header record contains the
 *           fields describing the sorted list of nodes in the tree, which
 *           are used to initialize a traversal and to remember the current
 *           list position of the traversal.
 *
 *         o fsck inode records can be accessed randomly (via routine
 *           get_inorecptr which accepts inode number as key) independently.
 *           That is, invocations of get_inorecptr may be intermingled with
 *           invocations of this routine (get_inorecptr_next) with no impact
 *           on the sequential traversal in progress.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int get_inorecptr_next(int is_aggregate,
		       uint32_t * inonum,
		       struct fsck_inode_record **addr_inorecptr)
{
	int girn_rc = FSCK_OK;
	int32_t iagidx, extidx, inoidx;
	int32_t extidx_init, inoidx_init;
	struct inode_ext_tbl_t *inoexttbl;
	struct inode_tbl_t *inotbl;

	/*
	 * find next active aggregate inode record
	 */
	if (is_aggregate) {
		/* for an aggregate inode */
		if (agg_recptr->AIT_ext0_tbl == NULL) {
			girn_rc = FSCK_INTERNAL_ERROR_54;
			fsck_send_msg(fsck_INTERNALERROR, girn_rc, 0, 0, 0);
		} else {
			*addr_inorecptr = NULL;
			for (inoidx = agg_recptr->agg_last_inoidx + 1;
			     ((inoidx < INOSPEREXT)
			      && (*addr_inorecptr == NULL)); inoidx++) {
				agg_recptr->agg_last_inoidx = inoidx;
				if (agg_recptr->AIT_ext0_tbl->
				    inorectbl[inoidx] != NULL) {
					*addr_inorecptr =
					    agg_recptr->AIT_ext0_tbl->
					    inorectbl[inoidx];
				}
			}
		}
		goto girn_set_exit;
	}

	if (agg_recptr->FSIT_IAG_tbl == NULL) {
		girn_rc = FSCK_INTERNAL_ERROR_55;
		fsck_send_msg(fsck_INTERNALERROR, girn_rc, 0, 0, 0);
		goto girn_set_exit;
	}

	/* the table is allocated */
	extidx_init = agg_recptr->fs_last_extidx;
	inoidx_init = agg_recptr->fs_last_inoidx + 1;
	*addr_inorecptr = NULL;
	for (iagidx = agg_recptr->fs_last_iagidx;
	     ((iagidx < agg_recptr->fset_imap.num_iags) &&
	      (*addr_inorecptr == NULL)); iagidx++) {
		agg_recptr->fs_last_iagidx = iagidx;
		if (agg_recptr->FSIT_IAG_tbl->
		    inoext_tbl[iagidx] != NULL) {
			inoexttbl =
			    agg_recptr->FSIT_IAG_tbl->inoext_tbl[iagidx];
			for (extidx = extidx_init; ((extidx < EXTSPERIAG)
			      && (*addr_inorecptr == NULL)); extidx++) {
				agg_recptr->fs_last_extidx = extidx;
				if (inoexttbl->inotbl[extidx] != NULL) {
					inotbl = inoexttbl->inotbl[extidx];
					for (inoidx = inoidx_init;
					     ((inoidx < INOSPEREXT) &&
					      (*addr_inorecptr == NULL));
					     inoidx++) {
						agg_recptr->fs_last_inoidx =
						    inoidx;
						if (inotbl->inorectbl[inoidx] !=
						    NULL) {
							*addr_inorecptr =
							    inotbl->inorectbl
							    [inoidx];
						}
					}
				}
				inoidx_init = 0;
			}
		}
		extidx_init = 0;
	}

      girn_set_exit:
	if (((*addr_inorecptr) != NULL) && (girn_rc == FSCK_OK)) {
		/* got one */
		*inonum = (*addr_inorecptr)->inonum;
	}

	return (girn_rc);
}

/****************************************************************************
 * NAME: init_agg_record
 *
 * FUNCTION: initializes the global record, fsck_aggregate
 *
 * PARAMETERS:  none
 *
 * NOTES:  The fsck aggregate record is pointed to by the global variable
 *         agg_recptr.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int init_agg_record()
{
	int iar_rc = FSCK_OK;

	memset(agg_recptr, 0, sizeof (struct fsck_agg_record));
	memcpy((void *) &(agg_recptr->eyecatcher), (void *) "fsckagrc", 8);
	memcpy((void *) &(agg_recptr->this_inode.eyecatcher),
	       (void *) "thisinod", 8);
	memcpy((void *) &(agg_recptr->agg_imap_eyecatcher), (void *) "agg imap",
	       8);
	memcpy((void *) &(agg_recptr->fset_imap_eyecatcher),
	       (void *) "fsetimap", 8);
	memcpy((void *) &(agg_recptr->AIT_eyecatcher), (void *) "agg ITbl", 8);
	memcpy((void *) &(agg_recptr->FSIT_eyecatcher), (void *) "fsetITbl", 8);
	memcpy((void *) &(agg_recptr->flags_eyecatcher), (void *) "aggflags",
	       8);
	memcpy((void *) &(agg_recptr->fais.eyecatcher), (void *) "faisinfo", 8);
	memcpy((void *) &(agg_recptr->vlarge_info_eyecatcher),
	       (void *) "vlargebf", 8);
	memcpy((void *) &(agg_recptr->fscklog_info_eyecatcher),
	       (void *) "fscklog ", 8);
	memcpy((void *) &(agg_recptr->blkmp_info_eyecatcher),
	       (void *) "blkmpbuf", 8);
	memcpy((void *) &(agg_recptr->ea_info_eyecatcher), (void *) "eabuffer",
	       8);
	memcpy((void *) &(agg_recptr->iag_info_eyecatcher), (void *) "iag buf ",
	       8);
	memcpy((void *) &(agg_recptr->mapctl_info_eyecatcher),
	       (void *) "mapctbuf", 8);
	memcpy((void *) &(agg_recptr->maplf_info_eyecatcher),
	       (void *) "maplfbuf", 8);
	memcpy((void *) &(agg_recptr->bmplv_info_eyecatcher),
	       (void *) "bmplvbuf", 8);
	memcpy((void *) &(agg_recptr->bmpdm_info_eyecatcher),
	       (void *) "bmpdmbuf", 8);
	memcpy((void *) &(agg_recptr->inobuf_info_eyecatcher),
	       (void *) "inodebuf", 8);
	memcpy((void *) &(agg_recptr->nodbuf_info_eyecatcher),
	       (void *) "node buf", 8);
	memcpy((void *) &(agg_recptr->dnodbuf_info_eyecatcher),
	       (void *) "dnodebuf", 8);
	memcpy((void *) &(agg_recptr->agg_AGTbl_eyecatcher),
	       (void *) "aggAGTbl", 8);
	memcpy((void *) &(agg_recptr->fset_AGTbl_eyecatcher),
	       (void *) "fs AGTbl", 8);
	memcpy((void *) &(agg_recptr->amap_eyecatcher), (void *) "iagiamap", 8);
	memcpy((void *) &(agg_recptr->fextsumm_eyecatcher), (void *) "fextsumm",
	       8);
	memcpy((void *) &(agg_recptr->finosumm_eyecatcher), (void *) "finosumm",
	       8);

	/* do the conversions from character to UniCharacter */

	agg_recptr->delim_char = '/';
	agg_recptr->UniChar_LSFN_NAME = uni_LSFN_NAME;
	agg_recptr->UniChar_lsfn_name = uni_lsfn_name;
	agg_recptr->agg_imap.ag_tbl = &(agg_recptr->agg_AGTbl[0]);
	agg_recptr->fset_imap.ag_tbl = &(agg_recptr->fset_AGTbl[0]);
	/*
	 * start the messaging level out as 'show everything'
	 * It may be reset lower when the parms have been parsed.
	 */
	agg_recptr->effective_msg_level = fsck_verbose;
	/*
	 * check to see whether standard out has been redirected, and
	 * set the flag accordingly.
	 */
	if (isatty(STDOUT_FILENO))
		agg_recptr->stdout_redirected = 0;
	else
		agg_recptr->stdout_redirected = 1;

	return (iar_rc);
}

/****************************************************************************
 * NAME: inorec_agg_search
 *
 * FUNCTION: Search in the aggregate inode record structures for a record
 *                describing the requested inode.
 *
 *                If found, return the address of the record.
 *
 *                If not found, return a null record address.
 *
 * PARAMETERS:	none
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int inorec_agg_search(uint32_t inonum,
		      struct fsck_inode_record **addr_inorecptr)
{
	int ias_rc = FSCK_OK;

	*addr_inorecptr = NULL;
	if (agg_recptr->AIT_ext0_tbl == NULL) {
		ias_rc = FSCK_INTERNAL_ERROR_17;
		fsck_send_msg(fsck_INTERNALERROR, ias_rc, inonum, 0, 0);
	} else {
		*addr_inorecptr = agg_recptr->AIT_ext0_tbl->inorectbl[inonum];
	}

	return (ias_rc);
}

/****************************************************************************
 * NAME: inorec_agg_search_insert
 *
 * FUNCTION: Search in the aggregate inode record structures for a record
 *                describing the requested inode.
 *
 *                If found, return the address of the record.
 *
 *                If not found, create a record to represent the inode,
 *                insert it into the structure, and return its address.
 *
 * PARAMETERS:	none
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int inorec_agg_search_insert(uint32_t inonum,
			     struct fsck_inode_record **addr_inorecptr)
{
	int iasi_rc = FSCK_OK;
	struct fsck_inode_record *new_inorecptr;
	int I_am_logredo = 0;

	*addr_inorecptr = NULL;
	if (agg_recptr->AIT_ext0_tbl == NULL) {
		iasi_rc = FSCK_INTERNAL_ERROR_48;
		fsck_send_msg(fsck_INTERNALERROR, iasi_rc, inonum, 0, 0);
	} else {
		/* the table is initialized */
		if (agg_recptr->AIT_ext0_tbl->inorectbl[inonum] == NULL) {
			/* not allocated */
			iasi_rc =
			    alloc_wrksp(inode_record_length, dynstg_inorec,
					I_am_logredo, (void **) &new_inorecptr);
			if (iasi_rc == FSCK_OK) {
				new_inorecptr->inonum = inonum;
				agg_recptr->AIT_ext0_tbl->inorectbl[inonum] =
				    new_inorecptr;
			}
		}
		if (iasi_rc == FSCK_OK) {
			*addr_inorecptr =
			    agg_recptr->AIT_ext0_tbl->inorectbl[inonum];
		}
	}

	return (iasi_rc);
}

/****************************************************************************
 * NAME: inorec_fs_search
 *
 * FUNCTION: Search in the fileset inode record structures for a record
 *                describing the requested inode.
 *
 *                If found, return the address of the record.
 *
 *                If not found, return a null record address.
 *
 * PARAMETERS:	none
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int inorec_fs_search(uint32_t inonum, struct fsck_inode_record **addr_inorecptr)
{
	int ifs_rc = FSCK_OK;
	int32_t iag_in_agg, ext_in_iag, ino_in_ext;
	struct inode_ext_tbl_t *inoexttbl;
	struct inode_tbl_t *inotbl;

	*addr_inorecptr = NULL;
	if (agg_recptr->FSIT_IAG_tbl == NULL) {
		ifs_rc = FSCK_INTERNAL_ERROR_49;
		fsck_send_msg(fsck_INTERNALERROR, ifs_rc, inonum, 0, 0);
	} else {
		/* the IAG table is initialized */
		locate_inode(inonum, &iag_in_agg, &ext_in_iag, &ino_in_ext);
		if (iag_in_agg < agg_recptr->fset_imap.num_iags) {
			/* IAG num in range */
			if (agg_recptr->FSIT_IAG_tbl->inoext_tbl[iag_in_agg] !=
			    NULL) {
				/* ext table alloc */
				inoexttbl =
				    agg_recptr->FSIT_IAG_tbl->
				    inoext_tbl[iag_in_agg];
				if (inoexttbl->inotbl[ext_in_iag] != NULL) {
					/* inode table allocated */
					inotbl = inoexttbl->inotbl[ext_in_iag];
					*addr_inorecptr =
					    inotbl->inorectbl[ino_in_ext];
				}
			}
		}
	}

	return (ifs_rc);
}

/****************************************************************************
 * NAME: inorec_fs_search_insert
 *
 * FUNCTION: Search in the fileset inode record structures for a record
 *                describing the requested inode.
 *
 *                If found, return the address of the record.
 *
 *                If not found, create a record to represent the inode,
 *                insert it into the structure, and return its address.
 *
 * NOTES:
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int inorec_fs_search_insert(uint32_t inonum,
			    struct fsck_inode_record **addr_inorecptr)
{
	int ifsi_rc = FSCK_OK;
	int32_t iag_in_agg, ext_in_iag, ino_in_ext;
	struct inode_ext_tbl_t *inoexttbl = 0;
	struct inode_ext_tbl_t *new_inoexttbl;
	struct inode_tbl_t *inotbl = 0;
	struct inode_tbl_t *new_inotbl;
	struct fsck_inode_record *new_inorecptr;
	int I_am_logredo = 0;

	*addr_inorecptr = NULL;
	if (agg_recptr->FSIT_IAG_tbl == NULL) {
		ifsi_rc = FSCK_INTERNAL_ERROR_67;
		fsck_send_msg(fsck_INTERNALERROR, ifsi_rc, inonum, 0, 0);
		goto ifsi_exit;
	}

	/* the IAG table is initialized */
	locate_inode(inonum, &iag_in_agg, &ext_in_iag, &ino_in_ext);
	if (iag_in_agg >= agg_recptr->fset_imap.num_iags)
		goto ifsi_exit;

	/*
	 * the IAG number is in range
	 */
	if (agg_recptr->FSIT_IAG_tbl->inoext_tbl[iag_in_agg] == NULL) {
		/*
		 * extent table not allocated
		 */
		ifsi_rc = alloc_wrksp(inode_ext_tbl_length, dynstg_inoexttbl,
				      I_am_logredo, (void **) &new_inoexttbl);
		if (ifsi_rc == FSCK_OK) {
			memcpy((void *)&(new_inoexttbl->eyecatcher),
			       (void *) "InoExTbl", 8);
			agg_recptr->FSIT_IAG_tbl->inoext_tbl[iag_in_agg] =
			    new_inoexttbl;
		}
	}
	if (ifsi_rc == FSCK_OK) {
		inoexttbl = agg_recptr->FSIT_IAG_tbl->inoext_tbl[iag_in_agg];

		if (inoexttbl->inotbl[ext_in_iag] == NULL) {
			/*
			 * the inode table is not allocated
			 */
			ifsi_rc = alloc_wrksp(inode_tbl_length, dynstg_inotbl,
					      I_am_logredo,
					      (void **) &new_inotbl);
			if (ifsi_rc == FSCK_OK) {
				memcpy((void *)&(new_inotbl->eyecatcher),
				       (void *) "InodeTbl", 8);
				inoexttbl->inotbl[ext_in_iag] = new_inotbl;
			}
		}
	}

	if (ifsi_rc == FSCK_OK) {
		inotbl = inoexttbl->inotbl[ext_in_iag];

		if (inotbl->inorectbl[ino_in_ext] == NULL) {
			/*
			 * the inode record is not allocated
			 */
			ifsi_rc = alloc_wrksp(inode_record_length,
					      dynstg_inorec,
					      I_am_logredo,
					      (void **)&new_inorecptr);
			if (ifsi_rc == FSCK_OK) {
				new_inorecptr->inonum = inonum;
				inotbl->inorectbl[ino_in_ext] = new_inorecptr;
			}
		}
	}

	if (ifsi_rc == FSCK_OK) {
		*addr_inorecptr = inotbl->inorectbl[ino_in_ext];
	}

      ifsi_exit:
	return (ifsi_rc);
}

/****************************************************************************
 * NAME: locate_inode
 *
 * FUNCTION: Given an inode number, calculate the corresponding IAG #,
 *                extent number within the IAG, and inode number within the
 *                extent.
 *
 * PARAMETERS:	none
 *
 * RETURNS:		none
 */
void locate_inode(uint32_t inonum, int32_t * iag_in_agg, int32_t * ext_in_iag,
		  int32_t * ino_in_ext)
{
	int32_t extinagg;

	*iag_in_agg = inonum >> L2INOSPERIAG;
	extinagg = inonum >> L2INOSPEREXT;
	*ext_in_iag = extinagg - ((*iag_in_agg) * EXTSPERIAG);
	*ino_in_ext = inonum - (extinagg << L2INOSPEREXT);

}

/*****************************************************************************
 * NAME: process_extent
 *
 * FUNCTION:  Verify the given extent, if found to be invalid, record
 *            information in the fsck inode record describing the owning
 *            inode for corrective measures.  Perform the requested action
 *            on the (adjusted if the bounds were invalid) range of aggregate
 *            blocks in the extent.
 *
 * PARAMETERS:
 *      inorecptr        - input - pointer to the fsck inode record describing
 *                                 the inode which owns the extent
 *      extent_length    - input - the number of blocks in the extent, as recorded
 *                                 in the inode structures
 *      extent_addr      - input - the ordinal number of the first block in the
 *                                 extent, as recorded in the inode structures
 *      is_EA            - input -  0 => the extent describes the inode's EA
 *                                 !0 => the extent describes something else for
 *                                       the inode
 *      msg_info_ptr     - input - pointer to a data area containing information
 *                                 needed to issue messages for this extent
 *      adjusted_length  - input - pointer to a variable in which to return the
 *                                 number of blocks on which the action was actually
 *                                 performed (if the range was invalid, its bounds
 *                                 are adjusted as needed to make it valid)
 *      extent_is_valid  - input - pointer to a variable in which the results of
 *                                 validating the extent are returned.
 *                                 !0 => the extent bounds, as passed in, are
 *                                       reasonable for the aggregate
 *                                  0 => the extent bounds, as passed in, are
 *                                       obviously incorrect
 *      desired_action   - input - { FSCK_RECORD | FSCK_RECORD_DUPCHECK |
 *                                   FSCK_UNRECORD | FSCK_QUERY }
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int process_extent(struct fsck_inode_record *inorecptr,
		   uint32_t extent_length,
		   int64_t extent_addr,
		   int8_t is_EA,
		   int8_t is_ACL,
		   struct fsck_ino_msg_info *msg_info_ptr,
		   uint32_t * adjusted_length,
		   int8_t * extent_is_valid, int desired_action)
{
	int ve_rc = FSCK_OK;
	int64_t first_valid;
	int64_t last_valid;
	int8_t range_adjusted = 0;

	if (inorecptr->inode_type == metadata_inode) {
		first_valid = extent_addr;
	} else {
		first_valid =
		    (extent_addr < agg_recptr->lowest_valid_fset_datablk)
		    ? agg_recptr->lowest_valid_fset_datablk : extent_addr;
	}
	last_valid =
	    ((extent_addr + extent_length) >
	     agg_recptr->highest_valid_fset_datablk)
	    ? agg_recptr->highest_valid_fset_datablk : extent_addr +
	    extent_length - 1;

	if (((first_valid > agg_recptr->highest_valid_fset_datablk)
	     && (inorecptr->inode_type != metadata_inode)) ||
	    /*
	     * starts after end of valid fset area AND
	     * isn't a meta data inode OR
	     */
	    ((last_valid < agg_recptr->lowest_valid_fset_datablk)
	     && (inorecptr->inode_type != metadata_inode)) ||
	    /*
	     * ends before the beginning of valid fset area AND
	     * isn't a meta data inode OR
	     */
	    (last_valid < first_valid)) {
		/* ends before it starts */

		*adjusted_length = 0;
		*extent_is_valid = 0;
		if (is_EA) {
			/* this is an extended attributes extent */
			inorecptr->clr_ea_fld = 1;
			inorecptr->ignore_ea_blks = 1;
			agg_recptr->corrections_needed = 1;
		} else if (is_ACL) {
			/* this is an access control list extent */
			inorecptr->clr_acl_fld = 1;
			inorecptr->ignore_acl_blks = 1;
			agg_recptr->corrections_needed = 1;
		} else {
			/* either a node (internal or leaf) or data */
			inorecptr->selected_to_rls = 1;
			inorecptr->ignore_alloc_blks = 1;
			agg_recptr->corrections_needed = 1;
		}
	} else {
		/* not out of the question */
		*adjusted_length = last_valid - first_valid + 1;
		if ((first_valid != extent_addr) ||
		    (last_valid != (extent_addr + extent_length - 1))) {
			/* at least some
			 * blocks are not valid for the fileset
			 */
			range_adjusted = -1;
			*extent_is_valid = 0;
			if (is_EA) {
				/* this is an extended attributes extent */
				inorecptr->clr_ea_fld = 1;
				agg_recptr->corrections_needed = 1;
			} else if (is_ACL) {
				/* this is an access control list extent */
				inorecptr->clr_acl_fld = 1;
				agg_recptr->corrections_needed = 1;
			} else {
				/* either a node (internal or leaf) or data */
				inorecptr->selected_to_rls = 1;
				agg_recptr->corrections_needed = 1;
			}
		} else {
			/* else the extent is ok */
			*extent_is_valid = -1;
		}
		/*
		 * Finally, depending on the parm passed by the caller,
		 *
		 *   either: record the ownership of the blocks which are within
		 *           range and keep a count of multiply allocated blocks.
		 *
		 *   or:     reverse notations made in the workspace for the ownership
		 *           of blocks which are within range and decrement the count
		 *           of multiply allocated blocks.
		 *
		 *   or:     check the extent to see if it contains the first reference
		 *           to any multiply allocated block for which the first
		 *           reference is still unresolved.
		 */
		switch (desired_action) {
		case FSCK_RECORD:
			ve_rc = extent_record(first_valid, last_valid);
			break;
		case FSCK_RECORD_DUPCHECK:
			ve_rc = extent_record_dupchk(first_valid, last_valid,
						     range_adjusted, is_EA,
						     is_ACL, msg_info_ptr,
						     inorecptr);
			break;
		case FSCK_UNRECORD:
			ve_rc = extent_unrecord(first_valid, last_valid);
			break;
		case FSCK_QUERY:
			ve_rc = extent_1stref_chk(first_valid, last_valid,
						  is_EA, is_ACL, msg_info_ptr,
						  inorecptr);
			break;
		default:
			ve_rc = FSCK_INTERNAL_ERROR_7;
		}
	}

	return (ve_rc);
}

/****************************************************************************
 * NAME: release_inode_extension
 *
 * FUNCTION: Make an fsck inode extension record available for reuse.
 *
 * PARAMETERS:
 *      inoext_ptr  - input - address of the extension record to release
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int release_inode_extension(struct fsck_inode_ext_record *inoext_ptr)
{
	int rir_rc = FSCK_OK;

	inoext_ptr->next = agg_recptr->free_inode_extens;
	agg_recptr->free_inode_extens = inoext_ptr;

	return (rir_rc);
}

/****************************************************************************
 * NAME: release_logredo_allocs
 *
 * FUNCTION:  Goes through all storage allocation records and, for each
 *                 record describing an allocation for logredo, marks all storage
 *                 available.
 *
 * PARAMETERS:	none
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int release_logredo_allocs()
{
	int rla_rc = FSCK_OK;
	struct wsp_ext_rec *this_fer;

	this_fer = agg_recptr->wsp_extent_list;

	while ((this_fer != NULL) && (rla_rc == FSCK_OK)) {
		if (this_fer->for_logredo) {
			this_fer->last_byte_used =
			    sizeof (struct wsp_ext_rec) - 1;
		}
		this_fer = this_fer->next;
	}

	return (rla_rc);
}

/****************************************************************************
 * NAME: temp_inode_buf_alloc
 *
 * FUNCTION:  Allocate an I/O buffer for use during metadata replication
 *            processing
 *
 * PARAMETERS:
 *      addr_buf_ptr  - input - pointer to a variable in which to return the
 *                              address of the buffer allocated.
 *
 * NOTES: o This very large buffer is only needed for a short time.  It is
 *          used to hold both the first extent of the primary AIT and the
 *          first extent of the secondary AIT.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int temp_inode_buf_alloc(char **addr_buf_ptr)
{
	int tiba_rc = FSCK_OK;

	agg_recptr->vlarge_current_use = USED_FOR_INOEXT_BUF;
	*addr_buf_ptr = (char *) agg_recptr->vlarge_buf_ptr;

	memset((void *) (*addr_buf_ptr), '\0', VLARGE_BUFSIZE);

	return (tiba_rc);
}

/****************************************************************************
 * NAME: temp_inode_buf_release
 *
 * FUNCTION:  Free storage which was allocated for an I/O buffer for use
 *            during metadata replication processing
 *
 * PARAMETERS:
 *      buf_ptr  - input - address of the buffer to release
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int temp_inode_buf_release(char *buf_ptr)
{
	int tibr_rc = FSCK_OK;

	agg_recptr->vlarge_current_use = NOT_CURRENTLY_USED;

	return (tibr_rc);
}

/****************************************************************************
 * NAME: temp_node_buf_alloc
 *
 * FUNCTION:  Allocate an I/O buffer for use during metadata replication
 *            processing
 *
 * PARAMETERS:
 *      addr_buf_ptr  - input - pointer to a variable in which to return the
 *                              address of the buffer allocated.
 *
 * NOTES: o This very large buffer is only needed for a short time.  It is
 *          used to hold both the first extent of the primary AIT and the
 *          first extent of the secondary AIT.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int temp_node_buf_alloc(char **addr_buf_ptr)
{
	int tnba_rc = FSCK_OK;
	int I_am_logredo = 0;

	tnba_rc = alloc_wrksp(XTPAGE_SIZE, dynstg_tmpinoiobuf,
			      I_am_logredo, (void **) addr_buf_ptr);

	if ((*addr_buf_ptr) == NULL) {
		/* allocation failure */
		tnba_rc = FSCK_FAILED_DYNSTG_EXHAUST7;
		fsck_send_msg(fsck_EXHDYNSTG, wsp_dynstg_action, dynstg_wspext);
	}
	return (tnba_rc);
}

/****************************************************************************
 * NAME: treeQ_dequeue
 *
 * FUNCTION: Removes an element from the front of the fsck tree-queue
 *           and returns a pointer to it.
 *           If the queue is empty, NULL is returned.
 *
 * PARAMETERS:
 *      treeQ_elptr  - input - pointer to a variable in which to return
 *                             the address of the element from the front of
 *                             the queue will be returned (or NULL if the
 *                             queue is empty)
 *
 * NOTES: The fsck xTree queue is described in the aggregate record,
 *        fields: treeQ_front, treeQ_back
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int treeQ_dequeue(struct treeQelem **treeQ_elptr)
{
	int tQd_rc = FSCK_OK;

	*treeQ_elptr = agg_recptr->treeQ_front;
	if (agg_recptr->treeQ_back == agg_recptr->treeQ_front) {
		/* empty */
		agg_recptr->treeQ_back = agg_recptr->treeQ_front = NULL;
	} else {
		/* not empty */
		agg_recptr->treeQ_front = agg_recptr->treeQ_front->next;
		agg_recptr->treeQ_front->prev = NULL;
	}

	return (tQd_rc);
}

/****************************************************************************
 * NAME: treeQ_enqueue
 *
 * FUNCTION: Adds the given element to the back of the fsck tree-queue
 *           stack.
 *
 * PARAMETERS:
 *      treeQ_elptr  - input - the address of the element to add to the queue
 *
 * NOTES: The fsck xTree queue is described in the aggregate record,
 *        fields: treeQ_front, treeQ_back
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int treeQ_enqueue(struct treeQelem *treeQ_elptr)
{
	int tep_rc = FSCK_OK;

	if (agg_recptr->treeQ_back == NULL) {
		/* empty queue */
		agg_recptr->treeQ_back = agg_recptr->treeQ_front = treeQ_elptr;
		treeQ_elptr->prev = treeQ_elptr->next = NULL;
	} else {
		/* queue not empty */
		treeQ_elptr->next = NULL;
		treeQ_elptr->prev = agg_recptr->treeQ_back;
		agg_recptr->treeQ_back->next = treeQ_elptr;
		agg_recptr->treeQ_back = treeQ_elptr;
	}

	return (tep_rc);
}

/****************************************************************************
 * NAME: treeQ_get_elem
 *
 * FUNCTION: Allocates workspace storage for an fsck tree-queue element
 *
 * PARAMETERS:
 *      addr_treeQ_ptr  - input - pointer to a variable in which to return
 *                                the address of the element allocated.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int treeQ_get_elem(struct treeQelem **addr_treeQ_ptr)
{
	int gte_rc = FSCK_OK;
	int I_am_logredo = 0;

	if (agg_recptr->free_treeQ != NULL) {
		/* free list isn't empty */
		*addr_treeQ_ptr = agg_recptr->free_treeQ;
		agg_recptr->free_treeQ = agg_recptr->free_treeQ->next;
		memset((void *) (*addr_treeQ_ptr), 0, treeQ_elem_length);
	} else {
		/* else the free list is empty */
		gte_rc = alloc_wrksp(treeQ_elem_length, dynstg_treeQ_elem,
				     I_am_logredo, (void **) addr_treeQ_ptr);
	}

	return (gte_rc);
}

/****************************************************************************
 *
 * NAME: treeQ_rel_elem
 *
 * FUNCTION: Makes an fsck tree-queue element available for reuse
 *
 * PARAMETERS:
 *      treeQ_elptr  - input - the address of the element to release
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int treeQ_rel_elem(struct treeQelem *treeQ_elptr)
{
	int tre_rc = FSCK_OK;

	treeQ_elptr->next = agg_recptr->free_treeQ;
	agg_recptr->free_treeQ = treeQ_elptr;

	return (tre_rc);
}

/*****************************************************************************
 * NAME: workspace_release
 *
 * FUNCTION:  Release the pool of storage allocated for fsck workspace
 *            storage.
 *
 * PARAMETERS: none
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int workspace_release()
{
	int wr_rc = FSCK_OK;
	struct wsp_ext_rec *this_fer;
	struct wsp_ext_rec *next_fer;
	/*
	 * If the very large buffer is (still) allocated, release it.
	 */
	if (agg_recptr->vlarge_buf_ptr != NULL) {
		free((void *) agg_recptr->vlarge_buf_ptr);
		agg_recptr->vlarge_buf_ptr = NULL;
		agg_recptr->ea_buf_ptr = NULL;
		agg_recptr->recon_buf_extent = NULL;
	}
	/*
	 * release the allocated extents
	 */
	this_fer = agg_recptr->wsp_extent_list;
	while (this_fer != NULL) {
		/* for each extent record */
		/* the one after this one (if any) */
		next_fer = this_fer->next;
		if (!this_fer->from_high_memory) {
			/* free the extent this
			 * fer describes (and occupies)
			 */
			free((void *) this_fer->extent_addr);
		}
		/* go on to the next one in the list */
		this_fer = next_fer;
	}

	return (wr_rc);
}
