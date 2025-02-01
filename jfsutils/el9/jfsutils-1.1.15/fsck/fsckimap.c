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
#include <errno.h>
#include <string.h>
/* defines and includes common among the fsck.jfs modules */
#include "xfsckint.h"
#include "jfs_byteorder.h"
#include "devices.h"
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

/* VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV
 *
 * The following are internal to this file
 *
 */
struct fsck_iag_info {
	struct iag *iagptr;
	struct dinomap *iamctlptr;
	struct fsck_iam_record *iamrecptr;
	struct fsck_iag_record *iagtbl;
	struct fsck_ag_record *agtbl;
	int8_t agg_inotbl;
	struct fsck_imap_msg_info *msg_info_ptr;
};

struct xtree_buf {
	struct xtree_buf *down;	/* next rightmost child */
	struct xtree_buf *up;	/* parent */
	xtpage_t *page;
};

static struct xtree_buf *fsim_node_pages;

/* --------------------------------------------------------------*/

int agfrext_lists_scan(int, int, int, struct fsck_iag_info *, int *,
		       struct fsck_imap_msg_info *);

int agfrext_lists_validation(int, int, struct fsck_ag_record *, int *,
			     struct fsck_imap_msg_info *);

int agfrino_lists_scan(int, int, int, struct fsck_iag_info *, int *,
		       struct fsck_imap_msg_info *);

int agfrino_lists_validation(int, int, struct fsck_ag_record *, int *,
			     struct fsck_imap_msg_info *);

int AIM_check(xtpage_t *, char *, int *);

int AIM_replication(int8_t, xtpage_t *, char *, int8_t *);

int AIS_inode_check(struct dinode *, struct dinode *, int32_t, int32_t, int *);

int AIS_inode_replication(int8_t, struct dinode *, struct dinode *);

int first_refchk_inoexts(int, int, struct fsck_inode_record *,
			 struct fsck_ino_msg_info *);

int FSIM_add_extents(xtpage_t *, struct dinode *, int8_t *);

int FSIM_check(struct dinode *, struct dinode *, int *);

int FSIM_replication(int8_t, struct dinode *, struct dinode *, int8_t *);

int iag_alloc_rebuild(int32_t, struct fsck_iag_info *);

int iag_alloc_scan(int32_t *, int32_t *, struct fsck_iag_info *,
		   struct fsck_imap_msg_info *);

int iag_alloc_ver(int *, int, int32_t, struct fsck_iag_info *,
		  struct fsck_imap_msg_info *);

int iagfr_list_scan(int, int, int, struct fsck_iag_info *, int *,
		    struct fsck_imap_msg_info *);

int iagfr_list_validation(int *, struct fsck_iam_record *,
			  struct fsck_imap_msg_info *);

int iags_finish_lists(int, int, int, struct fsck_iag_info *);

int iags_rebuild(int, int, int, struct fsck_iag_info *,
		 struct fsck_imap_msg_info *);

int iags_validation(int, int, int, int *, struct fsck_iag_info *,
		    struct fsck_imap_msg_info *);

int iamap_rebuild(int, int, int, struct fsck_iag_info *,
		  struct fsck_imap_msg_info *);

int iamap_validation(int, int, int, struct fsck_iag_info *,
		     struct fsck_imap_msg_info *);

int IM_compare_leaf(xtpage_t *, xtpage_t *, int *);

int record_dupchk_inoexts(int, int, int32_t *, struct fsck_inode_record *,
			  struct fsck_ino_msg_info *);

int record_imap_info(void);

int xtAppend(struct dinode *, int64_t, int64_t, int32_t, struct xtree_buf *);

int xtSplitPage(struct dinode *, struct xtree_buf *, int64_t, int32_t, int64_t);

int xtSplitRoot(struct dinode *, struct xtree_buf *, int64_t, int32_t, int64_t);

/* VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV */

/****************************************************************
 * NAME: agfrext_lists_scan
 *
 * FUNCTION:  Scan the Allocation Group free extent lists for the given
 *            inode allocation table.  Count the number of iags on each
 *            list.  Validate the list structure.
 *
 * PARAMETERS:
 *     is_aggregate     - input - !0 => aggregate owned
 *                                 0 => fileset owned
 *     which_it         - input - ordinal number of the aggregate inode
 *                                describing the inode table
 *     which_ait        - input - the aggregate inode table from which
 *                                the it inode should be read
 *                                { fsck_primary | fsck_secondary }
 *     iagiptr          - input - pointer to a data area describing the
 *                                current iag
 *     errors_detected  - input - pointer to a variable in which to return
 *                                !0 if errors are detected
 *                                 0 if no errors are detected
 *     msg_info_ptr     - input - pointer to data needed to issue messages
 *                                about the current inode allocation map
 *
 * NOTES:  This routine does NOT attempt to determine whether the iags
 *         on the list belong on the list.  It only verifies that the
 *         list is structurally correct, i.e., that the forward and
 *         backward pointers are consistent.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int agfrext_lists_scan(int is_aggregate,
		       int which_it,
		       int which_ait,
		       struct fsck_iag_info *iagiptr,
		       int *errors_detected,
		       struct fsck_imap_msg_info *msg_info_ptr)
{
	int intermed_rc = FSCK_OK;
	uint32_t agidx;
	struct iagctl *agg_agrec;
	struct fsck_ag_record *wsp_agrec;
	int end_of_list = -1;
	int32_t this_iagno, prev_iagno;

	for (agidx = 0; agidx < MAXAG; agidx++) {

		agg_agrec = &(iagiptr->iamctlptr->in_agctl[agidx]);
		wsp_agrec = &(iagiptr->agtbl[agidx]);

		/* in case list is empty... */
		wsp_agrec->frext_list_last = end_of_list;
		wsp_agrec->frext_list_first = agg_agrec->extfree;
		wsp_agrec->frext_list_len = 0;
		prev_iagno = end_of_list;
		this_iagno = wsp_agrec->frext_list_first;

		while ((this_iagno != end_of_list) &&
		       (!wsp_agrec->frext_list_bad)) {
			intermed_rc = iag_get(is_aggregate, which_it, which_ait,
					      this_iagno, &(iagiptr->iagptr));
			/*
			 * we consider an error here to be an error in the
			 * chain.  If it's really something more serious it
			 * will come up again when we go through all allocated
			 * iag's sequentially.
			 */
			if (intermed_rc != FSCK_OK) {
				wsp_agrec->frext_list_bad = -1;
				break;
			}
			/* got the iag */
			if (iagiptr->iagptr->extfreeback != prev_iagno) {
				/* bad back chain */
				wsp_agrec->frext_list_bad = -1;
				break;
			}
				/* back chain is correct */
				prev_iagno = this_iagno;
				this_iagno =
				    iagiptr->iagptr->extfreefwd;
				wsp_agrec->frext_list_len++;
		}

		if (wsp_agrec->frext_list_bad) {
			/* found a problem */
			agg_recptr->ag_dirty = 1;
			*errors_detected = -1;
			fsck_send_msg(fsck_BADAGFELIST,
				      fsck_ref_msg(msg_info_ptr->msg_mapowner),
				      agidx, "1");
		} else {
			wsp_agrec->frext_list_last = prev_iagno;
		}
	}

	return FSCK_OK;
}

/****************************************************************
 * NAME: agfrext_lists_validation
 *
 * FUNCTION:  Compare the results of the Allocation Group free extent lists
 *            scan with the results of validating the iags.  If the number
 *            of iags seen on a list during the list scan does not equal
 *            the number of iags which appear to be on the list (i.e., which
 *            have non-initialized values for forward and back pointers)
 *            as seen during iag validation, then the list is not structurally
 *            consistent.
 *
 * PARAMETERS:
 *     is_aggregate     - input - !0 => aggregate owned
 *                                 0 => fileset owned
 *     which_it         - input - ordinal number of the aggregate inode
 *                                describing the inode table
 *     agtbl            - input - pointer to the fsck workspace allocation
 *                                group table for the specified inode
 *                                table
 *     errors_detected  - input - pointer to a variable in which to return
 *                                !0 if errors are detected
 *                                 0 if no errors are detected
 *     msg_info_ptr     - input - pointer to data needed to issue messages
 *                                about the current inode allocation map
 *
 * NOTES: o This routine is only called in the read-only path.
 *
 *        o This routine is NOT called if any structural errors have
 *          already been detected in the list.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int agfrext_lists_validation(int is_aggregate,
			     int which_it,
			     struct fsck_ag_record *agtbl,
			     int *errors_detected,
			     struct fsck_imap_msg_info *msg_info_ptr)
{
	uint32_t agidx;
	struct fsck_ag_record *wsp_agrec;

	for (agidx = 0; agidx < MAXAG; agidx++) {

		wsp_agrec = &(agtbl[agidx]);

		if (!wsp_agrec->frino_list_bad) {
			/* the list itself looked ok */
			if (wsp_agrec->frext_list_len > 0) {
				/*
				 * fsck observed fewer iag's which belong on
				 * this list than it counted when it scanned
				 * the list.  (fsck has already issued messages
				 * about these iag's)
				 */
				*errors_detected = -1;
				agg_recptr->ag_dirty = 1;
			} else if (wsp_agrec->frext_list_len < 0) {
				/*
				 * fsck observed more iag's which belong on
				 * this list and which appear to be on the list
				 * than it counted when it scanned the list.
				 * So the chain has somehow lost some of its
				 * links.
				 */
				*errors_detected = -1;
				agg_recptr->ag_dirty = 1;

				fsck_send_msg(fsck_BADAGFELIST1,
					      fsck_ref_msg(msg_info_ptr->msg_mapowner),
					      agidx);
			}
		}
	}

	return (FSCK_OK);
}

/****************************************************************
 * NAME: agfrino_lists_scan
 *
 * FUNCTION:  Scan the Allocation Group free inode lists for the given
 *            inode allocation table.  Count the number of iags on each
 *            list.  Validate the list structure.
 *
 * PARAMETERS:
 *     is_aggregate     - input - !0 => aggregate owned
 *                                 0 => fileset owned
 *     which_it         - input - ordinal number of the aggregate inode
 *                                describing the inode table
 *     which_ait        - input - the aggregate inode table from which
 *                                the it inode should be read
 *                                { fsck_primary | fsck_secondary }
 *     iagiptr          - input - pointer to a data area describing the
 *                                current iag
 *     errors_detected  - input - pointer to a variable in which to return
 *                                !0 if errors are detected
 *                                 0 if no errors are detected
 *     msg_info_ptr     - input - pointer to data needed to issue messages
 *                                about the current inode allocation map
 *
 * NOTES:  This routine does NOT attempt to determine whether the iags
 *         on the list belong on the list.  It only verifies that the
 *         list is structurally correct, i.e., that the forward and
 *         backward pointers are consistent.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int agfrino_lists_scan(int is_aggregate,
		       int which_it,
		       int which_ait,
		       struct fsck_iag_info *iagiptr,
		       int *errors_detected,
		       struct fsck_imap_msg_info *msg_info_ptr)
{
	int intermed_rc = FSCK_OK;
	uint32_t agidx;
	struct iagctl *agg_agrec;
	struct fsck_ag_record *wsp_agrec;
	int end_of_list = -1;
	int32_t this_iagno, prev_iagno;

	for (agidx = 0; agidx < MAXAG; agidx++) {

		agg_agrec = &(iagiptr->iamctlptr->in_agctl[agidx]);
		wsp_agrec = &(iagiptr->agtbl[agidx]);

		/* in case list is empty... */
		wsp_agrec->frino_list_last = end_of_list;
		wsp_agrec->frino_list_first = agg_agrec->inofree;
		wsp_agrec->frino_list_len = 0;
		prev_iagno = end_of_list;
		this_iagno = wsp_agrec->frino_list_first;

		while ((this_iagno != end_of_list) &&
		       (!wsp_agrec->frino_list_bad)) {

			intermed_rc = iag_get(is_aggregate, which_it, which_ait,
					      this_iagno, &(iagiptr->iagptr));
			/*
			 * we consider an error here to be an error in the
			 * chain.  If it's really something more serious it
			 * will come up again when we go through all allocated
			 * iag's sequentially.
			 */
			if (intermed_rc != FSCK_OK) {
				wsp_agrec->frino_list_bad = -1;
				break;
			}
			/* got the iag */
			if (iagiptr->iagptr->inofreeback != prev_iagno)
				/* bad back chain */
				wsp_agrec->frino_list_bad = -1;
			else {
				/* back chain is correct */
				prev_iagno = this_iagno;
				this_iagno = iagiptr->iagptr->inofreefwd;
				wsp_agrec->frino_list_len++;
			}
		}

		if (wsp_agrec->frino_list_bad) {
			/* found a problem */
			agg_recptr->ag_dirty = 1;
			*errors_detected = -1;

			fsck_send_msg(fsck_BADAGFILIST,
				      fsck_ref_msg(msg_info_ptr->msg_mapowner),
				      agidx, "1");
		} else
			wsp_agrec->frino_list_last = prev_iagno;
	}

	return (FSCK_OK);
}

/****************************************************************
 * NAME: agfrino_lists_validation
 *
 * FUNCTION:  Compare the results of the Allocation Group free inode lists
 *            scan with the results of validating the iags.  If the number
 *            of iags seen on a list during the list scan does not equal
 *            the number of iags which appear to be on the list (i.e., which
 *            have non-initialized values for forward and back pointers)
 *            as seen during iag validation, then the list is not structurally
 *            consistent.
 *
 * PARAMETERS:
 *     is_aggregate     - input - !0 => aggregate owned
 *                                 0 => fileset owned
 *     which_it         - input - ordinal number of the aggregate inode
 *                                describing the inode table
 *     agtbl            - input - pointer to the fsck workspace allocation
 *                                group table for the specified inode
 *                                table
 *     errors_detected  - input - pointer to a variable in which to return
 *                                !0 if errors are detected
 *                                 0 if no errors are detected
 *     msg_info_ptr     - input - pointer to data needed to issue messages
 *                                about the current inode allocation map
 *
 * NOTES: o This routine is only called in the read-only path.
 *
 *        o This routine is NOT called if any structural errors have
 *          already been detected in the list.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int agfrino_lists_validation(int is_aggregate,
			     int which_it,
			     struct fsck_ag_record *agtbl,
			     int *errors_detected,
			     struct fsck_imap_msg_info *msg_info_ptr)
{
	uint32_t agidx;
	struct fsck_ag_record *wsp_agrec;

	for (agidx = 0; agidx < MAXAG; agidx++) {

		wsp_agrec = &(agtbl[agidx]);

		if (!wsp_agrec->frino_list_bad) {
			/* the list itself looked ok */
			if (wsp_agrec->frino_list_len > 0) {
				/*
				 * fsck observed fewer iag's which belong on
				 * this list than it counted when it scanned
				 * the list.  (fsck has already issued
				 * messages about these iag's)
				 */
				*errors_detected = -1;
				agg_recptr->ag_dirty = 1;
			} else if (wsp_agrec->frino_list_len < 0) {
				/*
				 * fsck observed more iag's which belong on
				 * this list and which appear to be on the list
				 * than it counted when it scanned the list.
				 * So the chain has somehow lost some of its
				 * links.
				 */
				*errors_detected = -1;
				agg_recptr->ag_dirty = 1;

				fsck_send_msg(fsck_BADAGFILIST1,
					      fsck_ref_msg(msg_info_ptr->msg_mapowner),
					      agidx);
			}
		}
	}

	return (FSCK_OK);
}

/****************************************************************
 * NAME: AIM_check
 *
 * FUNCTION:  Verify that the Secondary Aggregate Inode Map is correct
 *            and that it is a logical equivalent to the Primary Aggregate
 *            Inode Map.
 *
 * PARAMETERS:
 *     secondary_btroot        - input - pointer to the root of the B+ Tree
 *                                       root in of the secondary aggregate
 *                                       inode table inode in an fsck buffer.
 *     temp_bufptr             - input - pointer to a temporary I/O buffer,
 *                                       large enough for an inode extent.
 *     inconsistency_detected  - input - pointer to a variable in which
 *                                       to return !0 if errors are detected
 *                                                  0 if no errors are detected
 *
 *
 * NOTES:  o This routine reads the Primary Aggregate Inode Map into the
 *           the first page of the fsck temporary inode buffer and reads
 *           the Secondary Aggregate Inode Map into the 3rd page of the
 *           fsck temporary inode buffer.
 *
 *         o In release 1, there is always exactly 1 iag in the Aggregate
 *           Inode Map and so the inode which represents the map always has
 *           a root-leaf B+ tree rooted in it.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int AIM_check(xtpage_t * secondary_btroot, char *temp_bufptr,
	      int *inconsistency_detected)
{
	int intermed_rc = FSCK_OK;
	char *primary_bufptr = NULL, *secondary_bufptr = NULL;
	struct dinomap *primary_ctlptr, *secondary_ctlptr;
	struct iag *primary_iagptr;
	struct iag *secondary_iagptr;
	uint8_t expected_flag;
	int64_t sb_aim2_addr, btrt_aim2_addr;
	uint32_t sb_aim2_length, btrt_aim2_length;
	int64_t byte_offset;
	uint32_t bytes_wanted, bytes_read;

	expected_flag = (BT_ROOT | BT_LEAF | DXD_INDEX);
	sb_aim2_addr = addressPXD(&(sb_ptr->s_aim2));
	sb_aim2_length = lengthPXD(&(sb_ptr->s_aim2));
	btrt_aim2_addr = addressXAD(&(secondary_btroot->xad[XTENTRYSTART]));
	btrt_aim2_length = lengthXAD(&(secondary_btroot->xad[XTENTRYSTART]));

	if ((secondary_btroot->header.flag != expected_flag) ||
	    (secondary_btroot->header.nextindex != (XTENTRYSTART + 1)) ||
	    (sb_aim2_addr != btrt_aim2_addr) ||
	    (sb_aim2_length != btrt_aim2_length))
		goto inconsistency_found;

	/* descriptors match */
	primary_bufptr = temp_bufptr;
	secondary_bufptr = (char *) (temp_bufptr + (2 * BYTESPERPAGE));

	byte_offset = AIMAP_OFF;
	bytes_wanted = sb_aim2_length * sb_ptr->s_bsize;
	intermed_rc =
	    readwrite_device(byte_offset, bytes_wanted, &bytes_read,
			     (void *) primary_bufptr, fsck_READ);
	if ((intermed_rc != FSCK_OK) || (bytes_wanted != bytes_read))
		goto inconsistency_found;

	/* primary AIM is in the buffer */
	byte_offset = sb_aim2_addr * sb_ptr->s_bsize;
	bytes_wanted = sb_aim2_length * sb_ptr->s_bsize;
	intermed_rc = readwrite_device(byte_offset, bytes_wanted, &bytes_read,
				       (void *) secondary_bufptr, fsck_READ);
	if ((intermed_rc != FSCK_OK) || (bytes_wanted != bytes_read))
		goto inconsistency_found;

	/* endian - swap not needed as these 2 structs are read
	 * from disk and checked for inconsistencies.
	 * the AIMaps are in the temp buffer */
	primary_ctlptr = (struct dinomap *) primary_bufptr;
	secondary_ctlptr = (struct dinomap *) secondary_bufptr;

	if ((primary_ctlptr->in_freeiag != secondary_ctlptr->in_freeiag) ||
	    (primary_ctlptr->in_nextiag != secondary_ctlptr->in_nextiag) ||
	    (primary_ctlptr->in_numinos != secondary_ctlptr->in_numinos) ||
	    (primary_ctlptr->in_numfree != secondary_ctlptr->in_numfree) ||
	    (primary_ctlptr->in_nbperiext != secondary_ctlptr->in_nbperiext) ||
	    (primary_ctlptr->in_l2nbperiext !=
	     secondary_ctlptr->in_l2nbperiext))
		goto inconsistency_found;

	if (memcmp((void *) &(primary_ctlptr->in_agctl[0]),
		   (void *) &(secondary_ctlptr->in_agctl[0]),
		   (sizeof (struct iagctl) * MAXAG)))
		goto inconsistency_found;

	/*
	 * the AIMaps are in the temp buffer and
	 * the control pages are consistent
	 */
	primary_iagptr = (struct iag *) (primary_bufptr + BYTESPERPAGE);
	secondary_iagptr = (struct iag *) (secondary_bufptr + BYTESPERPAGE);

	if ((primary_iagptr->agstart != secondary_iagptr->agstart) ||
	    (primary_iagptr->iagnum != secondary_iagptr->iagnum) ||
	    (primary_iagptr->inofreefwd != secondary_iagptr->inofreefwd) ||
	    (primary_iagptr->inofreeback != secondary_iagptr->inofreeback) ||
	    (primary_iagptr->extfreefwd != secondary_iagptr->extfreefwd) ||
	    (primary_iagptr->extfreeback != secondary_iagptr->extfreeback) ||
	    (primary_iagptr->iagfree != secondary_iagptr->iagfree) ||
	    (primary_iagptr->nfreeinos != secondary_iagptr->nfreeinos) ||
	    (primary_iagptr->nfreeexts != secondary_iagptr->nfreeexts))
		goto inconsistency_found;

	if (memcmp((void *) &(primary_iagptr->inosmap[0]),
		   (void *) &(secondary_iagptr->inosmap[0]),
		   (sizeof (int32_t) * SMAPSZ)))
		goto inconsistency_found;

	if (memcmp((void *) &(primary_iagptr->extsmap[0]),
		   (void *) &(secondary_iagptr->extsmap[0]),
		   (sizeof (int32_t) * SMAPSZ)))
		goto inconsistency_found;

	if (memcmp((void *) &(primary_iagptr->pmap[0]),
		   (void *) &(secondary_iagptr->pmap[0]),
		   (sizeof (int32_t) * EXTSPERIAG)))
		goto inconsistency_found;

	if (memcmp((void *) &(secondary_iagptr->inoext[0]),
		   (void *) &(sb_ptr->s_ait2), sizeof (pxd_t)))
		goto inconsistency_found;

	return (FSCK_OK);

inconsistency_found:
	/*
	 * future recover capability is compromised
	 *
	 * Note that we're in read-only mode or we wouldn't be checking
	 * this (because when we have write access we always rebuild it)
	 */
	fsck_send_msg(fsck_INCONSIST2NDRY, "1");
	*inconsistency_detected = 1;

	return (FSCK_OK);
}

/****************************************************************
 * NAME: AIM_replication
 *
 * FUNCTION:  Rebuild specified target Aggregate Inode Map so that it is
 *            a logical equivalent to the specified source Aggregate Inode
 *            Map.
 *
 * PARAMETERS:
 *     source_is_primary    - input - !0 => replicate from primary to secondary
 *                                     0 => replicate from secondary to primary
 *     target_btroot        - input - pointer to the B+ Tree root in the
 *                                    aggregate inode allocation table (either
 *                                    primary or secondary) inode which is
 *                                    the target of the replication.
 *     temp_bufptr          - input - pointer to a temporary I/O buffer,
 *                                    large enough for an inode extent.
 *     replication_failure  - input - pointer to a variable in which to return
 *                                   !0 if errors occurred during replication
 *                                    0 if the replication was successful
 *
 * NOTES:  o This routine considers the buffer for the Primary Aggregate
 *           Inode Map to begin at the beginning of the fsck temporary
 *           inode buffer.  This routine considers the buffer for the
 *           Secondary Aggregate Inode Map to begin at the 3rd page
 *           of the fsck temporary inode buffer.
 *
 *         o This routine reads the source Aggregate Inode Map (either
 *           primary or secondary) into its buffer (see note above) and
 *           rebuilds the target Aggregate Inode Map (either secondary
 *           or primary) in its buffer.
 *
 *         o In release 1, there is always exactly 1 iag in the Aggregate
 *           Inode Map and so the inode which represents the map always has
 *           a root-leaf B+ tree rooted in it.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int AIM_replication(int8_t source_is_primary,
		    xtpage_t * target_btroot,
		    char *temp_bufptr, int8_t * replication_failure)
{
	int aimr_rc = FSCK_OK;
	int intermed_rc = FSCK_OK;
	char *target_bufptr, *source_bufptr;
	struct iag *target_iagptr;
	int64_t source_byte_offset = 0, target_byte_offset = 0;
	uint32_t bytes_wanted, bytes_transferred;

	target_btroot->header.flag = (BT_ROOT | BT_LEAF | DXD_INDEX);
	target_btroot->header.nextindex = XTENTRYSTART + 1;

	source_bufptr = temp_bufptr;
	target_bufptr = (char *) (temp_bufptr + (2 * BYTESPERPAGE));
	target_iagptr = (struct iag *) (target_bufptr + BYTESPERPAGE);

	if (source_is_primary) {
		source_byte_offset = AIMAP_OFF;
		target_byte_offset =
		    addressPXD(&(sb_ptr->s_aim2)) * sb_ptr->s_bsize;

		XADaddress(&(target_btroot->xad[XTENTRYSTART]),
			   addressPXD(&(sb_ptr->s_aim2)));
		XADlength(&(target_btroot->xad[XTENTRYSTART]),
			  lengthPXD(&(sb_ptr->s_aim2)));
	} else {
		source_byte_offset =
		    addressPXD(&(sb_ptr->s_aim2)) * sb_ptr->s_bsize;
		target_byte_offset = AIMAP_OFF;

		XADaddress(&(target_btroot->xad[XTENTRYSTART]),
			   (AIMAP_OFF / sb_ptr->s_bsize));
		XADlength(&(target_btroot->xad[XTENTRYSTART]),
			  lengthPXD(&sb_ptr->s_aim2));
	}

	bytes_wanted = lengthPXD(&(sb_ptr->s_aim2)) * sb_ptr->s_bsize;

	intermed_rc = readwrite_device(source_byte_offset,
				       bytes_wanted, &bytes_transferred,
				       (void *) source_bufptr, fsck_READ);
	if ((intermed_rc != FSCK_OK) || (bytes_wanted != bytes_transferred)) {
		aimr_rc = FSCK_FAILED_CANTREADAIMNOW;
		/*
		 * message to the user
		 */
		fsck_send_msg(fsck_URCVREAD, fsck_ref_msg(fsck_metadata), Vol_Label);
		/*
		 * message to the debugger
		 */
		fsck_send_msg(fsck_ERRONAGG, FSCK_FAILED_CANTREADAIMNOW,
			      intermed_rc, fsck_READ,
			      (long long) source_byte_offset,
			      bytes_wanted,
			      bytes_transferred);

		goto aimr_exit;
	}

	/* the source AIMap is in the temp buffer */
	/*
	 * copy the source over the target, then adjust the field(s) which
	 * should be different.
	 */
	memcpy((void *) target_bufptr, (void *) source_bufptr, bytes_wanted);

	memcpy((void *) &(target_iagptr->inoext[0]),
	       (void *) &(sb_ptr->s_ait2), sizeof (pxd_t));

	if (!source_is_primary) {
		XADaddress(&(target_iagptr->inoext[0]),
			   (AITBL_OFF / sb_ptr->s_bsize));
	}

	/* endian - don't need to swap either read or write in this routine.
	 * Basically, source struct is read from disk, copied to target,
	 * changes made, and written back.
	 *
	 * now write the redundant AIMap to the device
	 */
	aimr_rc = readwrite_device(target_byte_offset,
				   bytes_wanted, &bytes_transferred,
				   (void *) target_bufptr, fsck_WRITE);
	agg_recptr->ag_modified = 1;
	if ((aimr_rc != FSCK_OK) || (bytes_wanted != bytes_transferred)) {
		/* some or all didn't make it to the device. */
		agg_recptr->ait_aim_update_failed = 1;
		if (!source_is_primary) {
			/* we're trying to repair the primary
			 * table and can't -- that makes this a
			 * dirty file system.
			 */
			agg_recptr->ag_dirty = 1;
			fsck_send_msg(fsck_CANTREPAIRAIS, "1");
		} else {
			/* trying to repair the secondary table and can't
			 *
			 * We won't stop fsck and we won't mark the file system
			 * dirty on this condition, but we'll issue a warning
			 * and mark the superblock to prevent future attempts
			 * to maintain the flawed table.
			 */
			*replication_failure = -1;
			fsck_send_msg(fsck_INCONSIST2NDRY1, "1");
		}
	}
aimr_exit:
	return (aimr_rc);
}

/****************************************************************
 * NAME: AIS_inode_check
 *
 * FUNCTION: Compare the specified Primary Inode Allocation Table inode
 *           to its (specified) Secondary Inode Allocation Table counterpart
 *           to ensure that they are logically equivalent.
 *
 * PARAMETERS:
 *     primary_inoptr          - input - pointer to an inode in the primary
 *                                       aggregate inode allocation table
 *                                       in an fsck buffer
 *     secondary_inoptr        - input - pointer to the equivalent inode in
 *                                       the secondary aggregate inode
 *                                       allocation table in an fsck buffer
 *     tree_offset             - input - the offset of the B+ Tree rooted
 *                                       in the inode
 *     tree_size               - input - the number of inodes bytes occupied
 *                                       by the B+ tree rooted in the inode
 *     inconsistency_detected  - input - pointer to a variable in which
 *                                       to return !0 if errors are detected
 *                                                  0 if no errors are detected
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int AIS_inode_check(struct dinode *primary_inoptr,
		    struct dinode *secondary_inoptr,
		    int32_t tree_offset,
		    int32_t tree_size, int *inconsistency_detected)
{
	xtpage_t *primary_root, *secondary_root;
	int32_t result1, result2, result3, result4, result5, result6;

	*inconsistency_detected =
	    ((primary_inoptr->di_inostamp != secondary_inoptr->di_inostamp)
	     || (primary_inoptr->di_fileset != secondary_inoptr->di_fileset)
	     || (primary_inoptr->di_number != secondary_inoptr->di_number)
	     || (primary_inoptr->di_gen != secondary_inoptr->di_gen)
	     || (primary_inoptr->di_size != secondary_inoptr->di_size)
	     || (primary_inoptr->di_nblocks != secondary_inoptr->di_nblocks)
	     || (primary_inoptr->di_nlink != secondary_inoptr->di_nlink)
	     || (primary_inoptr->di_uid != secondary_inoptr->di_uid)
	     || (primary_inoptr->di_gid != secondary_inoptr->di_gid)
	     || (primary_inoptr->di_mode != secondary_inoptr->di_mode)
	     || (primary_inoptr->di_next_index !=
		 secondary_inoptr->di_next_index));

	if (!(*inconsistency_detected)) {
		if (tree_offset == 0) {
			result1 = 0;
		} else {
			/* tree compare needed */
			primary_root =
			    (xtpage_t *) (((char *) primary_inoptr) +
					  tree_offset);
			secondary_root =
			    (xtpage_t *) (((char *) primary_inoptr) +
					  tree_offset);
			result1 =
			    memcmp((void *) primary_root,
				   (void *) secondary_root, tree_size);
		}

		result2 = memcmp((void *) &(primary_inoptr->di_atime),
				 (void *) &(secondary_inoptr->di_atime),
				 sizeof (primary_inoptr->di_atime));
		result3 = memcmp((void *) &(primary_inoptr->di_ctime),
				 (void *) &(secondary_inoptr->di_ctime),
				 sizeof (primary_inoptr->di_ctime));
		result4 = memcmp((void *) &(primary_inoptr->di_mtime),
				 (void *) &(secondary_inoptr->di_mtime),
				 sizeof (primary_inoptr->di_mtime));
		result5 = memcmp((void *) &(primary_inoptr->di_otime),
				 (void *) &(secondary_inoptr->di_otime),
				 sizeof (primary_inoptr->di_otime));
		result6 = memcmp((void *) &(primary_inoptr->di_ea),
				 (void *) &(secondary_inoptr->di_ea),
				 sizeof (primary_inoptr->di_ea));

		*inconsistency_detected = (result1 || result2 || result3 ||
					   result4 || result5 || result6);
	}
	if (!(*inconsistency_detected)) {
		*inconsistency_detected = memcmp((void *) &(sb_ptr->s_ait2),
						 (void *) &(secondary_inoptr->
							    di_ixpxd),
						 sizeof (pxd_t));
	}
	if ((*inconsistency_detected)) {
		/* future recover capability is compromised */
		/*
		 * Note that we're in read-only mode or we wouldn't be checking
		 * this (because when we have write access we always rebuild it)
		 */
		fsck_send_msg(fsck_INCONSIST2NDRY, "2");
	}
	return (FSCK_OK);
}

/****************************************************************
 * NAME: AIS_inode_replication
 *
 * FUNCTION: Rebuild the specified target inode by making it the logical
 *           equivalent of the specified source inode.
 *
 * PARAMETERS:
 *     source_is_primary  - input - !0 => replicate from primary to secondary
 *                                   0 => replicate from secondary to primary
 *     target_inoptr      - input - address of an inode in the aggregate
 *                                  inode allocation table (primary or
 *                                  secondary) which is the target of the
 *                                  replication
 *     source_inoptr      - input - address of the equivalent inode in the
 *                                  aggregate inode allocation table (secondary
 *                                  or primary) which is the source of the
 *                                  secondary) which is the target of the
 *                                  replication
 *
 * NOTES:  o The caller to this routine must ensure that the first extent
 *           of the primary aggregate inode allocation table is at the
 *           beginning of the fsck temporary inode buffer and that the
 *           first extent of the secondary aggregate inode allocation map
 *           immediately follows it in the fsck temporary inode buffer.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int AIS_inode_replication(int8_t source_is_primary,
			  struct dinode *target_inoptr,
			  struct dinode *source_inoptr)
{
	int aisir_rc = FSCK_OK;
	int64_t ext_offset;

	/*
	 * copy the source over the target, then adjust the field(s) which
	 * should be different.
	 */
	memcpy((void *) target_inoptr, (void *) source_inoptr,
	       sizeof (struct dinode));

	if (source_is_primary) {
		memcpy((void *) &(target_inoptr->di_ixpxd),
		       (void *) &(sb_ptr->s_ait2), sizeof (pxd_t));
	} else {
		ext_offset = AITBL_OFF / sb_ptr->s_bsize;
		PXDaddress(&(target_inoptr->di_ixpxd), ext_offset);
		PXDlength(&(target_inoptr->di_ixpxd),
			  lengthPXD(&sb_ptr->s_ait2));
	}

	return (aisir_rc);
}

/****************************************************************
 * NAME: AIS_redundancy_check
 *
 * FUNCTION:  Verify that the Secondary Aggregate Inode structures are
 *            logically equivalent to the Primary Aggregate Inode structures.
 *
 * PARAMETERS:  none
 *
 * NOTES:  o The "aggregate inode structures" are the Aggregate Inode Map
 *           and the Aggregate Inode Table.
 *
 *         o This routine reads the first extent of the Primary Aggregate
 *           Inode Table into the first page of the fsck temporary inode
 *           buffer and reads the first extent of the Secondary Aggregate
 *           Inode table into the regular fsck inode buffer.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int AIS_redundancy_check()
{
	int aisrc_rc = FSCK_OK;
	struct dinode *primary_inoptr, *secondary_inoptr;
	xtpage_t *secondary_ait_root = NULL;
	char *primary_bufptr, *secondary_bufptr;
	int32_t inode_offset;
	int inconsistency_detected = 0;
	int32_t tree_offset, treesize;
	struct fsck_inode_record temp_inorec;
	struct fsck_inode_record *temp_inorecptr = &temp_inorec;
	struct fsck_ino_msg_info temp_msg_info;
	struct fsck_ino_msg_info *temp_msg_infoptr = &temp_msg_info;

	aisrc_rc = temp_inode_buf_alloc(&primary_bufptr);
	if (aisrc_rc != FSCK_OK)
		goto aisrc_exit;

	secondary_bufptr = agg_recptr->ino_buf_ptr;
	tree_offset = (int32_t) &(((struct dinode *)0)->di_btroot);
	treesize =
	    sizeof (primary_inoptr->di_DASD) + sizeof (primary_inoptr->di_dxd) +
	    sizeof (primary_inoptr->di_inlinedata);

	/* allocated the temp buffer */
	aisrc_rc = ait_special_read_ext1(fsck_primary);
	if (aisrc_rc != FSCK_OK) {
		/* read failed */
		report_readait_error(aisrc_rc, FSCK_FAILED_CANTREADAITEXT1,
				     fsck_primary);
		aisrc_rc = FSCK_FAILED_CANTREADAITEXT1;
		goto aisrc_exit;
	}
	/* primary is in the inode buffer */
	memcpy((void *) primary_bufptr, (void *) agg_recptr->ino_buf_ptr,
	       INODE_IO_BUFSIZE);

	aisrc_rc = ait_special_read_ext1(fsck_secondary);
	if (aisrc_rc != FSCK_OK) {
		/* read failed */
		report_readait_error(aisrc_rc, FSCK_FAILED_CANTREADAITEXT2,
				     fsck_secondary);
		aisrc_rc = FSCK_FAILED_CANTREADAITEXT2;
		goto aisrc_exit;
	}

	/* the 2 inode extents are in memory */
	/*
	 * the self inode
	 */
	inode_offset = AGGREGATE_I * sizeof (struct dinode);
	primary_inoptr = (struct dinode *) (primary_bufptr + inode_offset);
	secondary_inoptr = (struct dinode *) (secondary_bufptr + inode_offset);

	aisrc_rc = AIS_inode_check(primary_inoptr, secondary_inoptr, 0, 0,
				   &inconsistency_detected);
	if ((aisrc_rc != FSCK_OK) || inconsistency_detected)
		goto aisrc_exit;

	secondary_ait_root = (xtpage_t *) & (secondary_inoptr->di_btroot);
	/*
	 * the block map inode
	 */
	inode_offset = BMAP_I * sizeof (struct dinode);
	primary_inoptr = (struct dinode *) (primary_bufptr + inode_offset);
	secondary_inoptr = (struct dinode *) (secondary_bufptr + inode_offset);
	aisrc_rc = AIS_inode_check(primary_inoptr, secondary_inoptr,
				   tree_offset, treesize,
				   &inconsistency_detected);
	if ((aisrc_rc != FSCK_OK) || inconsistency_detected)
		goto aisrc_exit;

	/*
	 * the journal log inode
	 */
	inode_offset = LOG_I * sizeof (struct dinode);
	primary_inoptr = (struct dinode *) (primary_bufptr + inode_offset);
	secondary_inoptr = (struct dinode *) (secondary_bufptr + inode_offset);
	aisrc_rc = AIS_inode_check(primary_inoptr, secondary_inoptr,
				   tree_offset, treesize,
				   &inconsistency_detected);
	if ((aisrc_rc != FSCK_OK) || inconsistency_detected)
		goto aisrc_exit;
	/*
	 * the bad block inode
	 */
	inode_offset = BADBLOCK_I * sizeof (struct dinode);
	primary_inoptr = (struct dinode *) (primary_bufptr + inode_offset);
	secondary_inoptr = (struct dinode *) (secondary_bufptr + inode_offset);
	aisrc_rc = AIS_inode_check(primary_inoptr, secondary_inoptr,
				   tree_offset, treesize,
				   &inconsistency_detected);
	if ((aisrc_rc != FSCK_OK) || inconsistency_detected)
		goto aisrc_exit;

	/* and finally the filesystem inode */
	inode_offset = FILESYSTEM_I * sizeof (struct dinode);
	primary_inoptr = (struct dinode *) (primary_bufptr + inode_offset);
	secondary_inoptr = (struct dinode *) (secondary_bufptr + inode_offset);
	aisrc_rc = AIS_inode_check(primary_inoptr, secondary_inoptr, 0, 0,
				   &inconsistency_detected);
	if ((aisrc_rc != FSCK_OK) || inconsistency_detected)
		goto aisrc_exit;

	temp_inorecptr->inonum = FILESYSTEM_I;
	temp_inorecptr->in_use = 0;
	temp_inorecptr->selected_to_rls = 0;
	temp_inorecptr->crrct_link_count = 0;
	temp_inorecptr->crrct_prnt_inonum = 0;
	temp_inorecptr->adj_entries = 0;
	temp_inorecptr->cant_chkea = 0;
	temp_inorecptr->clr_ea_fld = 0;
	temp_inorecptr->clr_acl_fld = 0;
	temp_inorecptr->inlineea_on = 0;
	temp_inorecptr->inlineea_off = 0;
	temp_inorecptr->inline_data_err = 0;
	temp_inorecptr->ignore_alloc_blks = 0;
	temp_inorecptr->reconnect = 0;
	temp_inorecptr->unxpctd_prnts = 0;
	temp_inorecptr->badblk_inode = 0;
	temp_inorecptr->involved_in_dups = 0;
	temp_inorecptr->inode_type = metadata_inode;
	temp_inorecptr->link_count = 0;
	temp_inorecptr->parent_inonum = 0;
	temp_inorecptr->ext_rec = NULL;
	temp_msg_infoptr->msg_inonum = FILESYSTEM_I;
	temp_msg_infoptr->msg_inopfx = fsck_aggr_inode;
	temp_msg_infoptr->msg_inotyp = fsck_metadata;
	temp_msg_infoptr->msg_dxdtyp = 0;
	/*
	 * need to verify the tree structure in the secondary
	 * aggregate fileset inode and record/dupcheck the
	 * tree nodes (but not the data extents described by
	 * the leaf node(s)).
	 */
	aisrc_rc = xTree_processing(secondary_inoptr, FILESYSTEM_I,
				    temp_inorecptr, temp_msg_infoptr,
				    FSCK_FSIM_RECORD_DUPCHECK);
	if (temp_inorecptr->involved_in_dups ||
	    temp_inorecptr->selected_to_rls ||
	    temp_inorecptr->ignore_alloc_blks) {
		inconsistency_detected = -1;
		aisrc_rc = xTree_processing(secondary_inoptr, FILESYSTEM_I,
					    temp_inorecptr, temp_msg_infoptr,
					    FSCK_FSIM_UNRECORD);
		/*
		 * Note that we're in read-only mode or we wouldn't be
		 * checking this (because when we have write access we
		 * always rebuild it)
		 */
		fsck_send_msg(fsck_INCONSIST2NDRY, "4");
	} else {
		/* the tree is good and its nodes have been recorded */
		aisrc_rc = FSIM_check(primary_inoptr, secondary_inoptr,
				      &inconsistency_detected);
	}
	if ((aisrc_rc != FSCK_OK) || inconsistency_detected)
		goto aisrc_exit;

	/*
	 * no problems detected in the AIT --
	 * now verify the secondary AIM
	 */
	aisrc_rc = AIM_check(secondary_ait_root, primary_bufptr,
			     &inconsistency_detected);
aisrc_exit:
	temp_inode_buf_release(primary_bufptr);
	return (aisrc_rc);
}

/****************************************************************
 * NAME: AIS_replication
 *
 * FUNCTION:  Rebuild the aggregate inode structures so that the Secondary
 *            Aggregate Inode structures are logically equivalent to the
 *            Primary Aggregate Inode structures.
 *
 * PARAMETERS:  none
 *
 * NOTES:  o Since the roles of source and target AIT can change
 *           during this routine, we read both source and target
 *           so that when we write we preserve the portion of the
 *           target extent which is really a source.
 *           (This is simpler than attempting to write individual
 *           inodes and dealing with the question of device block
 *           size larger than an inode.)
 *
 *         o The "aggregate inode structures" are the Aggregate Inode Map
 *           and the Aggregate Inode Table.
 *
 *         o This routine reads the first extent of one Aggregate Inode
 *           Table into the fsck temporary inode buffer and reads the
 *           first extent of the other Aggregate Inode Table into the
 *           regular fsck inode buffer.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int AIS_replication()
{
	int aisr_rc = FSCK_OK;
	int intermed_rc = FSCK_OK;
	struct dinode *source_inoptr, *target_inoptr;
	int64_t target_byte_offset = 0;
	xtpage_t *target_ait_root = 0;
	char *source_bufptr, *target_bufptr;
	int32_t inode_offset;
	int8_t source_is_primary, extents_in_memory;
	int8_t replication_failed = 0;
	uint32_t bytes_transferred;

	aisr_rc = temp_inode_buf_alloc(&source_bufptr);

	target_bufptr = agg_recptr->ino_buf_ptr;

	if (aisr_rc != FSCK_OK)
		goto aisr_exit;

	/* allocated the temp buffer */
	/*
	 * the filesystem inode could have different source-target than the
	 *  rest of the table, so deal with it first.
	 */
	extents_in_memory = 0;
	if (agg_recptr->primary_ait_4part2) {
		/*
		 * primary for fileset inode source
		 */
		source_is_primary = -1;
		aisr_rc = ait_special_read_ext1(fsck_primary);
		if (aisr_rc != FSCK_OK) {
			/* read failed */
			report_readait_error(aisr_rc,
					     FSCK_FAILED_CANTREADAITEXT3,
					     fsck_primary);
			aisr_rc = FSCK_FAILED_CANTREADAITEXT3;
			goto aisr_release;
		}
	} else {
		/* secondary for fileset inode source */
		source_is_primary = 0;
		aisr_rc = ait_special_read_ext1(fsck_secondary);
		if (aisr_rc != FSCK_OK) {
			/* read failed */
			report_readait_error(aisr_rc,
					     FSCK_FAILED_CANTREADAITEXT4,
					     fsck_secondary);
			aisr_rc = FSCK_FAILED_CANTREADAITEXT4;
			goto aisr_release;
		}
	}
	/* source is in the inode buffer */
	memcpy((void *) source_bufptr, (void *) agg_recptr->ino_buf_ptr,
	       INODE_IO_BUFSIZE);

	if (agg_recptr->primary_ait_4part2) {
		/* secondary for fileset inode target */
		aisr_rc = ait_special_read_ext1(fsck_secondary);
		if (aisr_rc != FSCK_OK) {
			/* read failed */
			report_readait_error(aisr_rc,
					     FSCK_FAILED_CANTREADAITEXT5,
					     fsck_secondary);
			aisr_rc = FSCK_FAILED_CANTREADAITEXT5;
			goto aisr_release;
		}
		target_byte_offset =
			addressPXD(&(sb_ptr->s_ait2)) * sb_ptr->s_bsize;
	} else {
		/* primary for fileset inode target */
		aisr_rc = ait_special_read_ext1(fsck_primary);
		if (aisr_rc != FSCK_OK) {
			/* read failed */
			report_readait_error(aisr_rc,
					     FSCK_FAILED_CANTREADAITEXT6,
					     fsck_primary);
			aisr_rc = FSCK_FAILED_CANTREADAITEXT6;
			goto aisr_release;
		}
		target_byte_offset = AITBL_OFF;
	}

	/* the 2 inode extents for the fileset inode are in memory */
	extents_in_memory = -1;
	inode_offset = FILESYSTEM_I * sizeof (struct dinode);
	source_inoptr = (struct dinode *) (source_bufptr + inode_offset);
	target_inoptr = (struct dinode *) (target_bufptr + inode_offset);

	aisr_rc = AIS_inode_replication(source_is_primary,
					target_inoptr, source_inoptr);
	if (aisr_rc != FSCK_OK)
		goto aisr_release;

	aisr_rc = init_xtree_root(target_inoptr);
	if (aisr_rc != FSCK_OK)
		goto aisr_release;

	aisr_rc = FSIM_replication(source_is_primary, target_inoptr,
				   source_inoptr, &replication_failed);
	if ((aisr_rc == FSCK_OK) && (replication_failed)) {
		if (!source_is_primary) {
			/* we're trying to repair the primary
			 * table and can't -- that makes this a
			 * dirty file system.
			 */
			agg_recptr->ag_dirty = 1;
			fsck_send_msg(fsck_CANTREPAIRAIS, "2");
		} else {
			/* trying to repair the secondary table and can't
			 *
			 * We won't stop fsck and we won't  the file system
			 * dirty on this condition, but we'll issue a warning
			 * and  the superblock to prevent future attempts to
			 * maintain the flawed table.
			 */
			fsck_send_msg(fsck_INCONSIST2NDRY1, "2");
		}
	}
	if ((aisr_rc != FSCK_OK) || replication_failed)
		goto aisr_release;

	if (agg_recptr->primary_ait_4part1 != agg_recptr->primary_ait_4part2) {
		/*
		 * we need to switch source and target
		 */

		/* swap if on big endian machine */
		ujfs_swap_inoext((struct dinode *) target_bufptr, PUT,
				 sb_ptr->s_flag);

		intermed_rc = readwrite_device(target_byte_offset,
					       INODE_EXTENT_SIZE,
					       &bytes_transferred,
					       (void *) target_bufptr,
					       fsck_WRITE);
		/* swap back */
		ujfs_swap_inoext((struct dinode *) target_bufptr, GET,
				 sb_ptr->s_flag);

		agg_recptr->ag_modified = 1;
		if ((intermed_rc != FSCK_OK) ||
		    (bytes_transferred != INODE_EXTENT_SIZE)) {
			/*
			 * some or all didn't make it to the device.
			 *
			 */
			agg_recptr->ait_aim_update_failed = 1;

			if (!source_is_primary) {
				/* we're trying to repair the primary
				 * table and can't -- that makes this a
				 * dirty file system.
				 */
				agg_recptr->ag_dirty = 1;
				fsck_send_msg(fsck_CANTREPAIRAIS, "3");
			} else {
				/* trying to repair the secondary table and
				 * can't
				 *
				 * We won't stop fsck and we won't  the file
				 * system dirty on this condition, but we'll
				 * issue a warning and  the superblock to
				 * prevent future attempts to maintain the
				 * flawed table.
				 */
				replication_failed = -1;
				fsck_send_msg(fsck_INCONSIST2NDRY1, "3");
			}
		}
		extents_in_memory = 0;
		if (agg_recptr->primary_ait_4part1) {
			/* primary for remaining inodes source */
			source_is_primary = -1;
			aisr_rc = ait_special_read_ext1(fsck_primary);
			if (aisr_rc != FSCK_OK) {
				/* read failed */
				report_readait_error(aisr_rc,
						    FSCK_FAILED_CANTREADAITEXT7,
						    fsck_primary);
				aisr_rc = FSCK_FAILED_CANTREADAITEXT7;
				goto aisr_release;
			}
		} else {
			/* secondary for remaining inodes source */
			source_is_primary = 0;
			aisr_rc = ait_special_read_ext1(fsck_secondary);
			if (aisr_rc != FSCK_OK) {
				/* read failed */
				report_readait_error(aisr_rc,
						    FSCK_FAILED_CANTREADAITEXT8,
						    fsck_secondary);
				aisr_rc = FSCK_FAILED_CANTREADAITEXT8;
				goto aisr_release;
			}
		}
		/* source is in the inode buffer */
		memcpy((void *) source_bufptr, (void *) agg_recptr->ino_buf_ptr,
		       INODE_IO_BUFSIZE);

		if (agg_recptr->primary_ait_4part1) {
			/* secondary for remaining inodes target */
			intermed_rc = ait_special_read_ext1(fsck_secondary);
			target_byte_offset =
				addressPXD(&(sb_ptr->s_ait2)) * sb_ptr->s_bsize;
			if (intermed_rc != FSCK_OK) {
				/*
				 * message to debugger
				 */
				fsck_send_msg(fsck_ERRONAITRD, FSCK_CANTREADAITEXT3,
					      intermed_rc, fsck_secondary);
			}
		} else {
			/* primary for remaining inodes target */
			intermed_rc = ait_special_read_ext1(fsck_primary);
			target_byte_offset = AITBL_OFF;
			if (intermed_rc != FSCK_OK) {
				/*
				 * message to debugger
				 */
				fsck_send_msg(fsck_ERRONAITRD, FSCK_CANTREADAITEXT4,
					      intermed_rc, fsck_primary);
			}
		}

		if (intermed_rc == FSCK_OK)
			/* the 2 inode extents for the
			 * remaining inodes are in memory
			 */
			extents_in_memory = -1;
	}

	if (replication_failed || !extents_in_memory)
		goto aisr_release;

	/* we have the extents for the remaining AIT inodes */
	/*
	 * the self inode
	 */
	inode_offset = AGGREGATE_I * sizeof (struct dinode);
	source_inoptr = (struct dinode *) (source_bufptr + inode_offset);
	target_inoptr = (struct dinode *) (target_bufptr + inode_offset);

	aisr_rc = AIS_inode_replication(source_is_primary, target_inoptr,
					source_inoptr);
	if (aisr_rc != FSCK_OK)
		goto aisr_release;

	/* aggregate inode replicated in buffer */
	target_ait_root = (xtpage_t *) & (target_inoptr->di_btroot);
	/*
	 * the bmap inode
	 */
	inode_offset = BMAP_I * sizeof (struct dinode);
	source_inoptr = (struct dinode *) (source_bufptr + inode_offset);
	target_inoptr = (struct dinode *) (target_bufptr + inode_offset);

	aisr_rc = AIS_inode_replication(source_is_primary, target_inoptr,
					source_inoptr);
	if (aisr_rc != FSCK_OK)
		goto aisr_release;

	/*
	 * the journal log inode
	 */
	inode_offset = LOG_I * sizeof (struct dinode);
	source_inoptr = (struct dinode *) (source_bufptr + inode_offset);
	target_inoptr = (struct dinode *) (target_bufptr + inode_offset);

	aisr_rc = AIS_inode_replication(source_is_primary, target_inoptr,
					source_inoptr);
	if (aisr_rc != FSCK_OK)
		goto aisr_release;

	/*
	 * the bad block inode
	 */
	inode_offset = BADBLOCK_I * sizeof (struct dinode);
	source_inoptr = (struct dinode *) (source_bufptr + inode_offset);
	target_inoptr = (struct dinode *) (target_bufptr + inode_offset);

	aisr_rc = AIS_inode_replication(source_is_primary, target_inoptr,
					source_inoptr);
	if (aisr_rc != FSCK_OK)
		goto aisr_release;

	/* the ait is replicated in the buffer
	 *
	 * now finish up the self inode by replicating the aggregate inode
	 * map which it describes.
	 */
	aisr_rc = AIM_replication(source_is_primary, target_ait_root,
				  source_bufptr, &replication_failed);

	if (aisr_rc != FSCK_OK)
		goto aisr_release;

	/* ait replication in buffer now complete */

	/* swap if on big endian machine */
	ujfs_swap_inoext((struct dinode *) target_bufptr, PUT, sb_ptr->s_flag);
	intermed_rc = readwrite_device(target_byte_offset, INODE_EXTENT_SIZE,
				       &bytes_transferred,
				       (void *) target_bufptr, fsck_WRITE);
	/* swap back */
	ujfs_swap_inoext((struct dinode *) target_bufptr, GET, sb_ptr->s_flag);
	agg_recptr->ag_modified = 1;
	if ((intermed_rc != FSCK_OK) ||
	    (bytes_transferred != INODE_EXTENT_SIZE)) {
		/*
		 * some or all didn't make it to the device.
		 */
		agg_recptr->ait_aim_update_failed = 1;
		if (!source_is_primary) {
			/* we're trying to repair the primary
			 * table and can't -- that makes this a
			 * dirty file system.
			 */
			agg_recptr->ag_dirty = 1;
			fsck_send_msg(fsck_CANTREPAIRAIS, "4");
		} else {
			/* trying to repair the secondary table and can't
			 *
			 * We won't stop fsck and we won't  the file system
			 * dirty on this condition, but we'll issue a warning
			 * and  the superblock to prevent future attempts to
			 * maintain the flawed table.
			 */
			replication_failed = -1;
			fsck_send_msg(fsck_INCONSIST2NDRY1, "4");
		}
	}
      aisr_release:
	temp_inode_buf_release(source_bufptr);

	if (aisr_rc == FSCK_OK) {
		if (replication_failed) {
			sb_ptr->s_flag |= JFS_BAD_SAIT;
		} else {
			sb_ptr->s_flag &= (~JFS_BAD_SAIT);
		}
	}

      aisr_exit:
	return (aisr_rc);
}

/****************************************************************
 * NAME: first_ref_check_inode_extents
 *
 * FUNCTION:  Determine whether any inode extent in the aggregate (i.e.,
 *            in either the Aggregate Inode Table or in the Fileset Inode
 *            Table) contains a reference to any multiply-allocated
 *            aggregate block whose first reference has not yet been
 *            resolved.
 *
 * PARAMETERS:  none
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int first_ref_check_inode_extents()
{
	int rdie_rc = FSCK_OK;
	int is_aggregate;
	int which_it;
	struct fsck_inode_record *inorecptr;

	struct fsck_ino_msg_info msg_info;
	struct fsck_ino_msg_info *msg_info_ptr;

	msg_info_ptr = &msg_info;
	msg_info_ptr->msg_inonum = AGGREGATE_I;
	msg_info_ptr->msg_inopfx = fsck_aggr_inode;
	msg_info_ptr->msg_inotyp = fsck_metaIAG;

	/*
	 * check the inode extents in the Aggregate Inode Allocation Map
	 * for first references to multiply allocated blocks.
	 */
	is_aggregate = -1;
	if (agg_recptr->primary_ait_4part1)
		which_it = fsck_primary;
	else
		which_it = fsck_secondary;

	rdie_rc = get_inorecptr(is_aggregate, 0, AGGREGATE_I, &inorecptr);

	if ((rdie_rc == FSCK_OK) && (inorecptr == NULL))
		rdie_rc = FSCK_INTERNAL_ERROR_18;
	if (rdie_rc != FSCK_OK)
		goto rdie_exit;

	rdie_rc = first_refchk_inoexts(is_aggregate, which_it, inorecptr,
				       msg_info_ptr);
	if (rdie_rc != FSCK_OK)
		goto rdie_exit;
	/*
	 * check the inode extents in the Fileset Inode Allocation Maps
	 * for first references to multiply allocated blocks.
	 *
	 * (In release 1 there is exactly 1 fileset)
	 */
	which_it = FILESYSTEM_I;
	/* aggregate inode */
	is_aggregate = -1;
	rdie_rc = get_inorecptr(is_aggregate, 0, FILESYSTEM_I, &inorecptr);
	if ((rdie_rc == FSCK_OK) && (inorecptr == NULL))
		rdie_rc = FSCK_INTERNAL_ERROR_19;
	if (rdie_rc != FSCK_OK)
		goto rdie_exit;

	/* fileset IAGs */
	is_aggregate = 0;
	rdie_rc = first_refchk_inoexts(is_aggregate, which_it, inorecptr,
				       msg_info_ptr);
rdie_exit:
	return (rdie_rc);
}

/****************************************************************
 * NAME: first_refchk_inoexts
 *
 * FUNCTION:  Check all inode extents described by IAGs in the specified
 *            inode table to determine whether they contain a reference
 *            to any multiply-allocated aggregate block whose first reference
 *            has not yet been resolved.
 *
 * PARAMETERS:
 *     is_aggregate  - input - !0 => aggregate owned
 *                              0 => fileset owned
 *     which_it      - input - ordinal number of the aggregate inode
 *                             describing the inode table
 *     inorecptr     - input - pointer to an fsck inode record which
 *                             describes the current inode allocation
 *                             table inode
 *     msg_info_ptr  - input - pointer to data needed to issue messages
 *                             about the current inode
 *
 * NOTES:  o Since this routine completes the fsck workspace
 *           initialization needed by inode_get() (et al), this
 *           routine ensures fsck I/O buffers contain the inode
 *           it needs before invoking inode_get().
 *           (inode_get() is invoked to locate the inode within
 *           the fsck I/O buffer.)
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int first_refchk_inoexts(int is_aggregate,
			 int which_it,
			 struct fsck_inode_record *inorecptr,
			 struct fsck_ino_msg_info *msg_info_ptr)
{
	int ifrie_rc = FSCK_OK;
	struct iag *iagptr;
	uint32_t ext_idx;
	uint32_t ext_idx_max;
	int64_t ext_addr;
	uint32_t ext_len;
	int which_ait;
	int8_t is_EA = 0;
	int8_t is_ACL = 0;
	int8_t extent_is_valid;
	uint32_t adjusted_length;

	ext_idx_max = EXTSPERIAG - 1;
	if (is_aggregate) {
		if (agg_recptr->primary_ait_4part1) {
			which_ait = fsck_primary;
		} else {
			which_ait = fsck_secondary;
		}
	} else {
		if (agg_recptr->primary_ait_4part2) {
			which_ait = fsck_primary;
		} else {
			which_ait = fsck_secondary;
		}
	}
	ifrie_rc = iag_get_first(is_aggregate, which_it, which_ait, &iagptr);
	while ((iagptr != NULL) && (agg_recptr->unresolved_1stref_count > 0)
	       && (ifrie_rc == FSCK_OK)) {
		for (ext_idx = 0; ((ext_idx <= ext_idx_max)
				   && (agg_recptr->unresolved_1stref_count > 0)
				   && (ifrie_rc == FSCK_OK)); ext_idx++) {

			ext_addr = addressPXD(&(iagptr->inoext[ext_idx]));
			if (ext_addr != 0) {
				/* the extent is allocated */
				ext_len = lengthPXD(&(iagptr->inoext[ext_idx]));
				ifrie_rc =
				    process_extent(inorecptr, ext_len, ext_addr,
						   is_EA, is_ACL, msg_info_ptr,
						   &adjusted_length,
						   &extent_is_valid,
						   FSCK_QUERY);
			}
		}
		if (ifrie_rc == FSCK_OK) {
			ifrie_rc = iag_get_next(&iagptr);
		}
	}
	return (ifrie_rc);
}

/*--------------------------------------------------------------------
 * NAME: FSIM_add_extents
 *
 * FUNCTION: Add an extent of <thisblk> to the <bb_inode> inode
 *
 * NOTES:
 *	o It is not necessary to mark the extent in the block map
 *           since it is an extent already owned by the source
 *           FileSet Inode Map.
 *
 * PARAMETERS:
 *      thisblk - block number of bad block to add
 *      bb_inode        - Inode to add bad block to
 *
 * RETURNS: 0 for success; Other indicates failure
 */
int FSIM_add_extents(xtpage_t * src_leaf_ptr,
		     struct dinode *target_inode, int8_t * replication_failed)
{
	int fsimae_rc = FSCK_OK;
	int32_t xad_idx, ext_length, ext_bytes;
	int64_t ext_addr;

	*replication_failed = 0;

	for (xad_idx = XTENTRYSTART;
	     ((fsimae_rc == FSCK_OK) && (!*replication_failed) &&
	      (xad_idx < src_leaf_ptr->header.nextindex)); xad_idx++) {
		ext_addr = addressXAD(&(src_leaf_ptr->xad[xad_idx]));
		ext_length = lengthXAD(&(src_leaf_ptr->xad[xad_idx]));
		ext_bytes = ext_length << agg_recptr->log2_blksize;

		fsimae_rc =
		    xtAppend(target_inode,
			     target_inode->di_size / sb_ptr->s_bsize, ext_addr,
			     ext_length, fsim_node_pages);

		target_inode->di_size += ext_bytes;
		target_inode->di_nblocks += ext_length;
	}

	if (fsimae_rc > 0) {
		/* an error, but not fatal */
		*replication_failed = -1;
		fsimae_rc = FSCK_OK;
	}

	return (fsimae_rc);
}

/****************************************************************
 * NAME: FSIM_check
 *
 * FUNCTION: Compare the specified Primary Fileset Inode Map to its
 *                (specified) Secondary Fileset Inode Map counterpart to
 *                ensure that they are logically equivalent.
 *
 * PARAMETERS:
 *     primary_inoptr          - input - pointer to an inode in the primary
 *                                       aggregate inode allocation table
 *                                       in an fsck buffer
 *     secondary_inoptr        - input - pointer to the equivalent inode in
 *                                       the secondary aggregate inode
 *                                       allocation table in an fsck buffer
 *     inconsistency_detected  - input - pointer to a variable in which
 *                                       to return !0 if errors are detected
 *                                                  0 if no errors are detected
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int FSIM_check(struct dinode *primary_inoptr,
	       struct dinode *secondary_inoptr, int *inconsistency_detected)
{
	int fsimc_rc = FSCK_OK;
	xtpage_t *primary_nodeptr, *secondary_nodeptr, *leaf_node;
	int8_t primary_is_inline, primary_is_rootleaf;
	int8_t secondary_is_inline, secondary_is_rootleaf;
	int leaf_length;
	int64_t node_agg_offset;
	/*
	 * allocate 2 buffers to contain the node
	 */
	fsimc_rc = temp_node_buf_alloc((char **) (&agg_recptr->prim_nodeptr));
	if (fsimc_rc != FSCK_OK)
		goto fsimc_set_exit;

	/* first node buffer allocated */
	primary_nodeptr = agg_recptr->prim_nodeptr;
	fsimc_rc = temp_node_buf_alloc((char **) (&agg_recptr->second_nodeptr));
	if (fsimc_rc != FSCK_OK)
		goto fsimc_set_exit;

	/* second node buffer allocated */
	secondary_nodeptr = agg_recptr->second_nodeptr;
	fsimc_rc = find_first_leaf(primary_inoptr, &leaf_node, &node_agg_offset,
				   &primary_is_inline, &primary_is_rootleaf);
	if (fsimc_rc != FSCK_OK) {
		*inconsistency_detected = -1;
		fsimc_rc = FSCK_OK;
		goto fsimc_set_exit;
	}
	/* got first leaf of primary fs inode map
	 *
	 * copy it into the temp primary node buffer
	 */
	if (primary_is_rootleaf) {
		leaf_length = XTROOTMAXSLOT * sizeof (xad_t);
		memcpy((void *) primary_nodeptr,
		       (void *) &(primary_inoptr->di_btroot), leaf_length);
	} else {
		leaf_length = XTPAGE_SIZE;
		memcpy((void *) primary_nodeptr, (void *) leaf_node,
		       leaf_length);
	}
	fsimc_rc = find_first_leaf(secondary_inoptr, &leaf_node,
				   &node_agg_offset, &secondary_is_inline,
				   &secondary_is_rootleaf);
	if (fsimc_rc != FSCK_OK) {
		*inconsistency_detected = -1;
		fsimc_rc = FSCK_OK;
		goto fsimc_set_exit;
	}
	/* got first leaf of secondary fs inode map
	 *
	 * copy it into the temp secondary node buffer
	 */
	if (secondary_is_rootleaf) {
		leaf_length = XTROOTMAXSLOT * sizeof (xad_t);
		memcpy((void *) secondary_nodeptr,
		       (void *) &(secondary_inoptr->di_btroot), leaf_length);
	} else {
		leaf_length = XTPAGE_SIZE;
		memcpy((void *) secondary_nodeptr, (void *) leaf_node,
		       leaf_length);
	}

	if (*inconsistency_detected)
		goto fsimc_set_exit;

	if ((primary_is_rootleaf != secondary_is_rootleaf) ||
	    (primary_is_inline != secondary_is_inline)) {
		*inconsistency_detected = -1;
		goto fsimc_set_exit;
	}

	/* either both or neither is a rootleaf */
	if (primary_is_rootleaf) {
		/* they're root leaf nodes */
		fsimc_rc = IM_compare_leaf(primary_nodeptr,
					   secondary_nodeptr,
					   inconsistency_detected);
		goto fsimc_set_exit;
	}

	/* they're not root leaf nodes */
	while ((primary_nodeptr != NULL) && (secondary_nodeptr != NULL)) {
		fsimc_rc = IM_compare_leaf(primary_nodeptr, secondary_nodeptr,
					   inconsistency_detected);
		if ((fsimc_rc != FSCK_OK) || *inconsistency_detected)
			break;

		/* leafs compare as equal */
		if (primary_nodeptr->header.next == 0)
			primary_nodeptr = NULL;
		else {
			/* primary leaf has a next */
			node_agg_offset = primary_nodeptr->header.next;
			fsimc_rc = ait_node_get(node_agg_offset,
						primary_nodeptr);
			if (fsimc_rc != FSCK_OK) {
				if (fsimc_rc > 0) {
					/* error but not fatal */
					*inconsistency_detected = -1;
					fsimc_rc = FSCK_OK;
				}
				break;
			}
		}
		if (secondary_nodeptr->header.next == 0)
			secondary_nodeptr = NULL;
		else {
			/* secondary leaf has a next */
			node_agg_offset = secondary_nodeptr->header.next;
			fsimc_rc = ait_node_get(node_agg_offset,
						secondary_nodeptr);
			if (fsimc_rc != FSCK_OK) {
				if (fsimc_rc > 0) {
					/* error but not fatal */
					*inconsistency_detected = -1;
					fsimc_rc = FSCK_OK;
				}
				break;
			}
		}
	}
	if ((primary_nodeptr != NULL) || (secondary_nodeptr != NULL))
		/* on exit these should both be NULL */
		*inconsistency_detected = -1;

      fsimc_set_exit:
	if ((*inconsistency_detected)) {
		/* future recovery capability is compromised
		 *
		 * Note that we're in read-only mode or we wouldn't be checking
		 * this (because when we have write access we always rebuild it
		 */
		fsck_send_msg(fsck_INCONSIST2NDRY, "3");
	}
	return (fsimc_rc);
}

/****************************************************************
 * NAME: FSIM_replication
 *
 * FUNCTION: Replicate the Fileset Inode Map from the given source Aggregate
 *	     Map to the given target Aggregate Inode Map so that they are
 *	     logically equivalent.  That is, so that they have independent
 *	     B+Trees, but the leaf level of the 2 trees point to the same "data"
 *	     extents (control page and IAGs).
 *
 * PARAMETERS:
 *	source_is_primary - input - 0 if secondary FSIM is being replicated
 *					 into primary FSIM
 *				    !0 if primary FSIM is being replicated into
 *					 secondary FSIM
 *	source_inoptr	- input - pointer to an inode in the primary
 *				  aggregate inode allocation table in an fsck
 *				  buffer
 *	target_inoptr	- input - pointer to the equivalent inode in the
 *				  secondary aggregate inode allocation table in
 *				  an fsck buffer
 *	replication_failed - input - ptr to a variable in which to return 0
 *				     if the FSIM is successfully replicated and
 *				     !0 if the FSIM is not successfully
 *				     replicated
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int FSIM_replication(int8_t source_is_primary,
		     struct dinode *target_inoptr,
		     struct dinode *source_inoptr, int8_t * replication_failed)
{
	int fsimr_rc = FSCK_OK;
	xtpage_t *source_nodeptr, *leaf_node;
	int8_t source_is_inline, source_is_rootleaf;
	int leaf_length = 0;
	int64_t node_agg_offset;
	struct xtree_buf *curpage;
	int I_am_logredo = 0;

	/*
	 * allocate a buffer to contain the source leaf nodes
	 */
	fsimr_rc = temp_node_buf_alloc((char **) (&agg_recptr->prim_nodeptr));
	if (fsimr_rc != FSCK_OK)
		goto fsimr_exit;

	/* node buffer allocated */
	source_nodeptr = agg_recptr->prim_nodeptr;
	fsimr_rc = find_first_leaf(source_inoptr,
				   &leaf_node, &node_agg_offset,
				   &source_is_inline, &source_is_rootleaf);
	if (fsimr_rc != FSCK_OK) {
		*replication_failed = -1;
		fsimr_rc = FSCK_OK;
	} else {
		/* got first leaf of primary fs inode map */
		/*
		 * copy it into the temp primary node buffer
		 */
		if (source_is_rootleaf) {
			leaf_length = XTROOTMAXSLOT * sizeof (xad_t);
			memcpy((void *) source_nodeptr,
			       (void *) &(source_inoptr->di_btroot),
			       leaf_length);
		} else {
			/* root is internal */
			leaf_length = XTPAGE_SIZE;
			memcpy((void *) source_nodeptr,
			       (void *) leaf_node, leaf_length);
			/*
			 * Initialize list of xtree append buffers
			 */
			fsimr_rc = alloc_wrksp(sizeof (struct xtree_buf),
					       dynstg_xtreebuf, I_am_logredo,
					       (void **) &fsim_node_pages);
			if (fsimr_rc == FSCK_OK) {
				fsim_node_pages->down =
				    fsim_node_pages->up = NULL;
				fsim_node_pages->page =
				    (xtpage_t *) & target_inoptr->di_btroot;
			}
		}
	}

	if (*replication_failed || (fsimr_rc != FSCK_OK))
		goto fsimr_exit;

	if (source_is_rootleaf) {
		target_inoptr->di_size = source_inoptr->di_size;
		target_inoptr->di_nblocks = source_inoptr->di_nblocks;
		memcpy((void *) &(target_inoptr->di_btroot),
		       (void *) &(source_inoptr->di_btroot), leaf_length);
		goto fsimr_exit;
	}

	/* source is not a rootleaf */
	target_inoptr->di_size = 0;
	target_inoptr->di_nblocks = 0;
	while ((source_nodeptr != NULL) && (!(*replication_failed)) &&
	       (fsimr_rc == FSCK_OK)) {
		/*
		 * add the extents described in this xtpage to the
		 * target inode
		 */
		fsimr_rc = FSIM_add_extents(source_nodeptr, target_inoptr,
					    replication_failed);
		if ((fsimr_rc == FSCK_OK) && !(*replication_failed)) {
			/* leaf replicated */
			if (source_nodeptr->header.next == 0) {
				source_nodeptr = NULL;
			} else {
				/* source leaf has a next */
				node_agg_offset = source_nodeptr->header.next;
				fsimr_rc = ait_node_get(node_agg_offset,
							source_nodeptr);
				if (fsimr_rc != FSCK_OK) {
					if (fsimr_rc > 0) {
						/* error but not fatal */
						*replication_failed = -1;
						fsimr_rc = FSCK_OK;
					}
				}
			}
		}
	}
	/*
	 * If any pages were allocated for the xtree rooted
	 * in the FileSet Inode Map (FSIM) then we need to
	 * write them to the device
	 */
	curpage = fsim_node_pages;
	while ((!(curpage->page->header.flag & BT_ROOT)) &&
	       (!(*replication_failed)) && (fsimr_rc == FSCK_OK)) {
		node_agg_offset = addressPXD(&(curpage->page->header.self));
		fsimr_rc = ait_node_put(node_agg_offset, curpage->page);
		if (fsimr_rc != 0) {
			*replication_failed = -1;
		} else {
			curpage = curpage->up;
		}
	}

      fsimr_exit:
	return (fsimr_rc);
}

/****************************************************************
 * NAME: iag_alloc_rebuild
 *
 * FUNCTION: Rebuild the specified Inode Allocation Group (iag) using data
 *           based on fsck's observations of the aggregate.
 *
 * PARAMETERS:
 *     iagnum   - input - ordinal number of the current iag
 *     iagiptr  - input - pointer to a data area describing the
 *                        current iag
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int iag_alloc_rebuild(int32_t iagnum, struct fsck_iag_info *iagiptr)
{
	int iar_rc = FSCK_OK;
	uint32_t agnum;
	/*
	 * rebuild IAG's pmap and wmap
	 */
	memcpy((void *) &(iagiptr->iagptr->pmap[0]),
	       (void *) &(agg_recptr->amap[0]), AMAPSIZE);
	memcpy((void *) &(iagiptr->iagptr->wmap[0]),
	       (void *) &(agg_recptr->amap[0]), AMAPSIZE);
	/*
	 * rebuild the IAG's summary maps
	 */
	memcpy((void *) &(iagiptr->iagptr->extsmap[0]),
	       (void *) &(agg_recptr->fextsumm[0]), SMAPSIZE);
	memcpy((void *) &(iagiptr->iagptr->inosmap[0]),
	       (void *) &(agg_recptr->finosumm[0]), SMAPSIZE);
	/*
	 * reset the IAG's "free counts"
	 */
	iagiptr->iagptr->nfreeinos =
	    iagiptr->iagtbl[iagnum].unused_backed_inodes;
	iagiptr->iagptr->nfreeexts = iagiptr->iagtbl[iagnum].unbacked_extents;
	/*
	 * add it to the various lists, as appropriate
	 *
	 * (In the case of doubly linked list, only the forward pointers
	 *  are set here.  Resetting back pointers involves I/O and will be
	 *  done later.)
	 */
	if (iagiptr->iagtbl[iagnum].unbacked_extents == EXTSPERIAG) {
		/* this iag belongs on the imap free IAG list */
		iagiptr->iagptr->iagfree = iagiptr->iamctlptr->in_freeiag;
		iagiptr->iamctlptr->in_freeiag = iagnum;
		iagiptr->iagptr->extfreefwd = -1;
		iagiptr->iagptr->extfreeback = -1;
		iagiptr->iagptr->inofreefwd = -1;
		iagiptr->iagptr->inofreeback = -1;
	} else {
		/* this iag does not belong on the imap free IAG list */
		iagiptr->iagptr->iagfree = -1;
		agnum = iagiptr->iagtbl[iagnum].AG_num;
		if ((iagiptr->iagtbl[iagnum].unbacked_extents > 0) &&
		    (iagiptr->iagtbl[iagnum].unbacked_extents < EXTSPERIAG)) {
			/* this iag belongs on the AG free extent list */
			iagiptr->iagptr->extfreefwd =
			    iagiptr->iamctlptr->in_agctl[agnum].extfree;
			iagiptr->iamctlptr->in_agctl[agnum].extfree = iagnum;
		} else {
			/* this iag doesn't belong on the AG free extent list */
			iagiptr->iagptr->extfreefwd = -1;
			iagiptr->iagptr->extfreeback = -1;
		}
		if (iagiptr->iagtbl[iagnum].unused_backed_inodes > 0) {
			/* this iag belongs on the AG free inode list */
			iagiptr->iagptr->inofreefwd =
			    iagiptr->iamctlptr->in_agctl[agnum].inofree;
			iagiptr->iamctlptr->in_agctl[agnum].inofree = iagnum;
		} else {
			/* this iag does not belong on the AG free inode list */
			iagiptr->iagptr->inofreefwd = -1;
			iagiptr->iagptr->inofreeback = -1;
		}
	}
	return (iar_rc);
}

/****************************************************************
 * NAME: iag_alloc_scan
 *
 * FUNCTION: Scan data collected by fsck which describes the inodes in
 *           the range described by the current iag.  Summarize this
 *           data in a form which will be convenient for use when verifying
 *           or rebuilding the iag.
 *
 * PARAMETERS:
 *     iag_alloc_inodes  - input - pointer to a variable in which to return
 *                                 a count of inodes which are described
 *                                 by the current iag and are both backed and
 *                                 in use in the filesystem
 *     iag_alloc_exts    - input - pointer to a variable in which to return
 *                                 a count of the inode extents which are
 *                                 backed and described by the current iag
 *     iagiptr           - input - pointer to a data area describing the
 *                                 current iag
 *     msg_info_ptr      - input - pointer to data needed to issue messages
 *                                 about the current inode allocation map
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int iag_alloc_scan(int32_t * iag_alloc_inodes,
		   int32_t * iag_alloc_exts,
		   struct fsck_iag_info *iagiptr,
		   struct fsck_imap_msg_info *msg_info_ptr)
{
	int ias_rc = FSCK_OK;
	int32_t iag_smapidx;
	int32_t iag_extidx, smap_extidx;
	int32_t inoidx, agg_inoidx;
	int32_t ext_alloc_inodes;
	struct fsck_inode_record *this_inorecptr;
	int64_t ext_addr;
	uint32_t ext_length;
	uint32_t map_mask = 0x80000000;
	int alloc_ifnull = 0;
	/*
	 * build maps for this iag
	 */
	iag_extidx = 0;
	*iag_alloc_inodes = 0;
	*iag_alloc_exts = 0;
	agg_inoidx = (iagiptr->iamrecptr->num_iags - 1) * INOSPERIAG;
	for (iag_smapidx = 0; (iag_smapidx < SMAPSZ); iag_smapidx++) {
		for (smap_extidx = 0; (smap_extidx < EXTSPERSUM);
		     smap_extidx++, iag_extidx++) {
			ext_addr =
			    addressPXD(&(iagiptr->iagptr->inoext[iag_extidx]));
			if (ext_addr == 0) {
				/* extent not allocated */
				agg_recptr->amap[iag_extidx] = 0;
				/* turn off the bit */
				agg_recptr->fextsumm[iag_smapidx] &=
				    ~(map_mask >> smap_extidx);
				/* turn on the bit */
				agg_recptr->finosumm[iag_smapidx] |=
				    (map_mask >> smap_extidx);
				agg_inoidx += INOSPEREXT;

				continue;
			}
			/* extent is allocated */
			(*iag_alloc_exts)++;
			/* turn on the bit */
			agg_recptr->fextsumm[iag_smapidx] |=
			    (map_mask >> smap_extidx);
			ext_alloc_inodes = 0;
			for (inoidx = 0; (inoidx < INOSPEREXT);
			     inoidx++, agg_inoidx++) {
				ias_rc =
				    get_inorecptr(iagiptr->agg_inotbl,
						  alloc_ifnull,
						  agg_inoidx, &this_inorecptr);
				if (ias_rc != FSCK_OK)
					goto ias_exit;
				if (this_inorecptr == NULL) {
					/* backed but not referenced */
					/* turn off the bit */
					agg_recptr->amap[iag_extidx] &=
					    ~(map_mask >> inoidx);
					continue;
				}
				/* either in use or referenced */
				if ((this_inorecptr->in_use)
				    && ((agg_recptr->processing_readonly) ||
					(!this_inorecptr->selected_to_rls))) {
					/*
					 * inode allocated and won't be
					 * released (this session)
					 */
					agg_recptr->amap[iag_extidx]
					    |= (map_mask >> inoidx);
					/* turn on the bit */
					ext_alloc_inodes++;
					if ((this_inorecptr->inode_type ==
					     file_inode)
					    || (this_inorecptr->inode_type ==
						block_special_inode)
					    || (this_inorecptr->inode_type ==
						char_special_inode)
					    || (this_inorecptr->inode_type ==
						FIFO_inode)
					    || (this_inorecptr->inode_type ==
						SOCK_inode)) {
						agg_recptr->
						    files_in_aggregate++;
					} else if (this_inorecptr->inode_type ==
						   directory_inode) {
						agg_recptr->dirs_in_aggregate++;
					}
				} else {
					/* inode backed but not allocated */
					agg_recptr->amap[iag_extidx]
					    &= ~(map_mask >> inoidx);
					/* turn off the bit */
				}
			}
			if (ias_rc != FSCK_OK)
				goto ias_exit;
			if ((ext_alloc_inodes == 0) &&
			    (agg_recptr->processing_readwrite)) {
				/*
				 * we have write access (so we'll be rebuilding
				 * the IAGs, not just verifying them) and none
				 * of the inodes in this extent are allocated.
				 *
				 * Release this inode extent.
				 *
				 * turn off the bit because extent slot
				 * available */
				agg_recptr->fextsumm[iag_smapidx] &=
				    ~(map_mask >> smap_extidx);
				/* turn on the bit because no inodes available
				 */
				agg_recptr->finosumm[iag_smapidx] |=
				    (map_mask >> smap_extidx);
				ext_length = lengthPXD(
					&(iagiptr->iagptr->inoext[iag_extidx]));
				ias_rc = extent_unrecord(ext_addr,
						(ext_addr + ext_length - 1));
				*iag_alloc_exts -= 1;
				PXDaddress(
					&(iagiptr->iagptr->inoext[iag_extidx]),
					0);
				PXDlength(
					&(iagiptr->iagptr->inoext[iag_extidx]),
					0);
				if (ias_rc != FSCK_OK)
					goto ias_exit;
			} else {
				/* read only or at least one inode is allocated
				 */
				*iag_alloc_inodes += ext_alloc_inodes;
				if (ext_alloc_inodes == INOSPEREXT) {
					/* all in use */
					agg_recptr->finosumm[iag_smapidx] |=
					    (map_mask >> smap_extidx);
					/*
					 * turn on the bit
					 */
				} else {
					/* some backed but not in use */
					agg_recptr->finosumm[iag_smapidx] &=
					    ~(map_mask >> smap_extidx);
					/*
					 * turn off the bit
					 */
				}
			}
		}
	}
      ias_exit:
	return (ias_rc);
}

/****************************************************************
 * NAME: iag_alloc_ver
 *
 * FUNCTION: Verify that the specified Inode Allocation Group (iag) is
 *           correct according to fsck's observations of the aggregate.
 *
 * PARAMETERS:
 *     errors_detected  - input - pointer to a variable in which to return
 *                                !0 if errors are detected
 *                                 0 if no errors are detected
 *     agstrt_is_bad    - input - !0 => the iag cannot be associated with
 *                                      a particular allocation group
 *                                 0 => the iag appears to be associated
 *                                      with a valid allocation group
 *     iagnum           - input - ordinal number of the current iag
 *     iagiptr          - input - pointer to a data area describing the
 *                                current iag
 *     msg_info_ptr     - input - pointer to data needed to issue messages
 *                                about the current inode allocation map
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int iag_alloc_ver(int *errors_detected,
		  int agstrt_is_bad,
		  int32_t iagnum,
		  struct fsck_iag_info *iagiptr,
		  struct fsck_imap_msg_info *msg_info_ptr)
{
	int outcome;
	uint32_t agnum;
	/*
	 * check out the IAG's pmap
	 */
	outcome = memcmp((void *) &(agg_recptr->amap[0]),
			 (void *) &(iagiptr->iagptr->pmap[0]), AMAPSIZE);
	if (outcome != 0) {
		/* pmaps don't match! */
		*errors_detected = -1;
		fsck_send_msg(fsck_BADIAGPMAP,
			      fsck_ref_msg(msg_info_ptr->msg_mapowner),
			      iagnum);
	}
	/*
	 * check out the IAG's free extent summary map
	 */
	outcome = memcmp((void *) &(agg_recptr->fextsumm[0]),
			 (void *) &(iagiptr->iagptr->extsmap[0]), SMAPSIZE);
	if (outcome != 0) {
		/* free extent maps don't match! */
		*errors_detected = -1;
		fsck_send_msg(fsck_BADIAGFIES, fsck_ref_msg(msg_info_ptr->msg_mapowner),
			      iagnum);
	}
	/*
	 * check out the IAG's (extents with) free inodes summary map
	 */
	outcome = memcmp((void *) &(agg_recptr->finosumm[0]),
			 (void *) &(iagiptr->iagptr->inosmap[0]), SMAPSIZE);
	if (outcome != 0) {
		/* free inode maps don't match! */
		*errors_detected = -1;
		fsck_send_msg(fsck_BADIAGFIS, fsck_ref_msg(msg_info_ptr->msg_mapowner),
			      iagnum);
	}
	/*
	 * check out the IAG's "free counts"
	 */
	if (iagiptr->iagptr->nfreeinos !=
	    iagiptr->iagtbl[iagnum].unused_backed_inodes) {
		/* free inode count is wrong */
		*errors_detected = -1;
		fsck_send_msg(fsck_BADIAGNFINO, fsck_ref_msg(msg_info_ptr->msg_mapowner),
			      iagnum);
	}
	if (iagiptr->iagptr->nfreeexts !=
	    iagiptr->iagtbl[iagnum].unbacked_extents) {
		/* free extent count is wrong */
		*errors_detected = -1;
		fsck_send_msg(fsck_BADIAGNFEXT, fsck_ref_msg(msg_info_ptr->msg_mapowner),
			      iagnum);
	}
	/*
	 * verify *inclusion in* / *exclusion from* the various IAG lists
	 */
	if (!iagiptr->iamrecptr->friag_list_bad) {
		/* the list looked good when we scanned it */
		if (iagiptr->iagtbl[iagnum].unbacked_extents == EXTSPERIAG) {
			/* this iag belongs on the imap free IAG list */
			if ((iagiptr->iagptr->iagfree == -1) &&
			    (iagiptr->iamrecptr->friag_list_last != iagnum)) {
				/* not on the list! */
				*errors_detected = -1;
				fsck_send_msg(fsck_BADIAGFLIST,
					      fsck_ref_msg(msg_info_ptr->msg_mapowner),
					      "2");
			} else {
				/* it is on the list */
				/* we've seen one of them */
				iagiptr->iamrecptr->friag_list_len--;
			}
		} else {
			/* this iag does not belong on the imap free IAG list */
			if ((iagiptr->iagptr->iagfree != -1) ||
			    (iagiptr->iamrecptr->friag_list_last == iagnum)) {
				/* on the list! */
				/* we've seen one of them */
				iagiptr->iamrecptr->friag_list_len--;
				*errors_detected = -1;
				fsck_send_msg(fsck_BADIAGFLIST,
					      fsck_ref_msg(msg_info_ptr->msg_mapowner),
					      "3");
			}
		}
	}
	if (agstrt_is_bad)
		goto iav_exit;

	/* we can associate an AG with this IAG */
	agnum = iagiptr->iagtbl[iagnum].AG_num;
	if (!iagiptr->agtbl[agnum].frext_list_bad) {
		/* the list looked valid when we scanned it */
		if ((iagiptr->iagtbl[iagnum].unbacked_extents > 0) &&
		    (iagiptr->iagtbl[iagnum].unbacked_extents < EXTSPERIAG)) {
			/* this iag belongs on the AG free extent list */
			if ((iagiptr->iagptr->extfreefwd == -1) &&
			    (iagiptr->iagptr->extfreeback == -1) &&
			    ((iagiptr->agtbl[agnum].frext_list_first != iagnum)
			     || (iagiptr->agtbl[agnum].frext_list_last !=
				 iagnum))) {
				/* not on the list! */
				*errors_detected = -1;
				fsck_send_msg(fsck_BADAGFELIST,
					      fsck_ref_msg(msg_info_ptr->msg_mapowner),
					      iagnum, "2");
			} else
				/* it is on the list */
				/* we've seen one of them */
				iagiptr->agtbl[agnum].frext_list_len--;
		} else {
			/* this iag doesn't belong on the AG free extent list */
			if ((iagiptr->iagptr->extfreefwd != -1) ||
			    (iagiptr->iagptr->extfreeback != -1) ||
			    ((iagiptr->agtbl[agnum].frext_list_first == iagnum)
			     && (iagiptr->agtbl[agnum].frext_list_last ==
				 iagnum))) {
				/* on the list! */
				/* we've seen one of them */
				iagiptr->agtbl[agnum].frext_list_len--;
				*errors_detected = -1;
				fsck_send_msg(fsck_BADAGFELIST,
					      fsck_ref_msg(msg_info_ptr->msg_mapowner),
					      iagnum, "3");
			}
		}
	}
	if (!iagiptr->agtbl[agnum].frino_list_bad) {
		/* the list looked valid when we scanned it */
		if (iagiptr->iagtbl[iagnum].unused_backed_inodes > 0) {
			/* this iag belongs on the AG free inode list */
			if ((iagiptr->iagptr->inofreefwd == -1) &&
			    (iagiptr->iagptr->inofreeback == -1) &&
			    ((iagiptr->agtbl[agnum].frino_list_first != iagnum)
			     || (iagiptr->agtbl[agnum].frino_list_last !=
				 iagnum))) {
				/* not on the list! */
				*errors_detected = -1;
				fsck_send_msg(fsck_BADAGFILIST,
					      fsck_ref_msg(msg_info_ptr->msg_mapowner),
					      iagnum, "3");
			} else
				/* it is on the list */
				/* we've seen one of them */
				iagiptr->agtbl[agnum].frino_list_len--;
		} else {
			/* this iag does not belong on the AG free inode list */
			if ((iagiptr->iagptr->inofreefwd != -1) ||
			    (iagiptr->iagptr->inofreeback != -1) ||
			    ((iagiptr->agtbl[agnum].frino_list_first == iagnum)
			     && (iagiptr->agtbl[agnum].frino_list_last ==
				 iagnum))) {
				/* on the list! */
				/* we've seen one of them */
				iagiptr->agtbl[agnum].frino_list_len--;
				*errors_detected = -1;
				fsck_send_msg(fsck_BADAGFILIST,
					      fsck_ref_msg(msg_info_ptr->msg_mapowner),
					      iagnum, "3");
			}
		}
	}
iav_exit:
	return (FSCK_OK);
}

/****************************************************************
 * NAME: iagfr_list_scan
 *
 * FUNCTION:  Scan the iag list for the specified Inode Allocation Map.
 *            Count the number of iags on the list.  Validate the list
 *            structure.
 *
 * PARAMETERS:
 *     is_aggregate     - input - !0 => aggregate owned
 *                                 0 => fileset owned
 *     which_it         - input - ordinal number of the aggregate inode
 *                                describing the inode table
 *     which_ait        - input - the aggregate inode table from which
 *                                the it inode should be read
 *                                { fsck_primary | fsck_secondary }
 *     iagiptr          - input - pointer to a data area describing the
 *                                current iag
 *     errors_detected  - input - pointer to a variable in which to return
 *                                !0 if errors are detected
 *                                 0 if no errors are detected
 *     msg_info_ptr     - input - pointer to data needed to issue messages
 *                                about the current inode allocation map
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int iagfr_list_scan(int is_aggregate,
		    int which_it,
		    int which_ait,
		    struct fsck_iag_info *iagiptr,
		    int *errors_detected,
		    struct fsck_imap_msg_info *msg_info_ptr)
{
	int ifls_rc = FSCK_OK;
	int intermed_rc = FSCK_OK;
	int32_t this_iagno;
	int32_t end_of_list = -1;

	iagiptr->iamrecptr->friag_list_last = end_of_list;
	iagiptr->iamrecptr->friag_list_len = 0;
	this_iagno = iagiptr->iamctlptr->in_freeiag;
	while ((this_iagno != end_of_list) &&
	       (!iagiptr->iamrecptr->friag_list_bad) && (ifls_rc == FSCK_OK)) {
		intermed_rc = iag_get(is_aggregate, which_it, which_ait,
				      this_iagno, &(iagiptr->iagptr));
		/*
		 * we consider an error here to be an error in the chain.  If
		 * it's really something more serious it will come up again
		 * when we go through all allocated iag's sequentially.
		 */
		if (intermed_rc != FSCK_OK) {
			iagiptr->iamrecptr->friag_list_bad = -1;
		} else {
			/* got the iag */
			/* in case it's last... */
			iagiptr->iamrecptr->friag_list_last = this_iagno;
			/* increment the counter */
			iagiptr->iamrecptr->friag_list_len++;
			this_iagno = iagiptr->iagptr->iagfree;
		}
	}

	if (iagiptr->iamrecptr->friag_list_bad) {
		/* found a problem */
		agg_recptr->ag_dirty = 1;
		*errors_detected = -1;
		fsck_send_msg(fsck_BADIAGFLIST,
			      fsck_ref_msg(msg_info_ptr->msg_mapowner), "1");
	}
	return (ifls_rc);
}

/****************************************************************
 * NAME: iagfr_list_validation
 *
 * FUNCTION:  Compare the results of the Inode Allocation Map free iag
 *            list scan with the results of validating the iags.  If
 *            the number of iags seen on the list during list scan does
 *            not match the number of iags which appear to be on the
 *            list (i.e., which have a non-initialized value in their
 *            forward pointer) as seen during iag validation, then the
 *            list is not structurally consistent.
 *
 * PARAMETERS:
 *     errors_detected  - input - pointer to a variable in which to return
 *                                !0 if errors are detected
 *                                 0 if no errors are detected
 *     iamrecptr        - input - pointer to a data area which describes the
 *                                current inode allocation map
 *     msg_info_ptr     - input - pointer to data needed to issue messages
 *                                about the current inode allocation map
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int iagfr_list_validation(int *errors_detected,
			  struct fsck_iam_record *iamrecptr,
			  struct fsck_imap_msg_info *msg_info_ptr)
{
	int iflv_rc = FSCK_OK;

	if (iamrecptr->friag_list_len > 0) {
		/*
		 * fsck observed fewer iag's which belong on the free iag
		 * list than it counted when it scanned the free iag list.
		 * (fsck has already issued messages about these iag's)
		 */
		*errors_detected = -1;
		agg_recptr->ag_dirty = 1;
	} else if (iamrecptr->friag_list_len < 0) {
		/*
		 * fsck observed more iag's which belong on the free iag
		 * list and which appear to be on the list than it counted
		 * when it scanned the free iag list.  So the chain has
		 * somehow lost some of its links.
		 */
		*errors_detected = -1;
		agg_recptr->ag_dirty = 1;
		fsck_send_msg(fsck_BADIAGFL1,
			      fsck_ref_msg(msg_info_ptr->msg_mapowner));
	}
	return (iflv_rc);
}

/****************************************************************
 * NAME: iags_finish_lists
 *
 * FUNCTION:  Complete the rebuild process for the Allocation Group (ag)
 *            free extent lists and free inode lists.  Specifically,
 *            for each ag, set the back pointer in each iag on its
 *            free extent list and in each iag on its free inode list.
 *
 * PARAMETERS:
 *     is_aggregate  - input - !0 => aggregate owned
 *                              0 => fileset owned
 *     which_it      - input - ordinal number of the aggregate inode
 *                             describing the inode table
 *     which_ait     - input - the aggregate inode table from which
 *                             the it inode should be read
 *                             { fsck_primary | fsck_secondary }
 *     iagiptr       - input - pointer to a data area describing the
 *                             current iag
 *
 * NOTES: o When rebuilding the ag free extent lists and the free inode
 *          lists, it is very inconvenient to establish both forward and
 *          backward pointers.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int iags_finish_lists(int is_aggregate, int which_it, int which_ait,
		      struct fsck_iag_info *iagiptr)
{
	int iifl_rc = FSCK_OK;
	int32_t prev_iag, this_iag;
	uint32_t agidx;
	/*
	 * first set the back pointers in the free inode lists
	 */
	for (agidx = 0; ((agidx < MAXAG) && (iifl_rc == FSCK_OK)); agidx++) {
		prev_iag = -1;
		this_iag = iagiptr->iamctlptr->in_agctl[agidx].inofree;
		while ((this_iag != -1) && (iifl_rc == FSCK_OK)) {
			iifl_rc = iag_get(is_aggregate, which_it, which_ait,
					  this_iag, &(iagiptr->iagptr));
			if (iifl_rc == FSCK_OK) {
				/* got the iag */
				iagiptr->iagptr->inofreeback = prev_iag;
				prev_iag = this_iag;
				this_iag = iagiptr->iagptr->inofreefwd;
				iifl_rc = iag_put(iagiptr->iagptr);
			}
		}
	}
	/*
	 * now set the back pointers in the free extent lists
	 */
	for (agidx = 0; ((agidx < MAXAG) && (iifl_rc == FSCK_OK)); agidx++) {
		prev_iag = -1;
		this_iag = iagiptr->iamctlptr->in_agctl[agidx].extfree;
		while ((this_iag != -1) && (iifl_rc == FSCK_OK)) {
			iifl_rc = iag_get(is_aggregate, which_it, which_ait,
					  this_iag, &(iagiptr->iagptr));
			if (iifl_rc == FSCK_OK) {
				/* got the iag */
				iagiptr->iagptr->extfreeback = prev_iag;
				prev_iag = this_iag;
				this_iag = iagiptr->iagptr->extfreefwd;
				iifl_rc = iag_put(iagiptr->iagptr);
			}
		}
	}
	return (iifl_rc);
}

/****************************************************************
 * NAME: iags_rebuild
 *
 * FUNCTION: Rebuild each Inode Allocation Group (iag) in the specified
 *           Inode Allocation Map using data based on fsck's observations
 *           of the aggregate.
 *
 * PARAMETERS:
 *     is_aggregate  - input - !0 => aggregate owned
 *                              0 => fileset owned
 *     which_it      - input - ordinal number of the aggregate inode
 *                             describing the inode table
 *     which_ait     - input - the aggregate inode table from which
 *                             the it inode should be read
 *                             { fsck_primary | fsck_secondary }
 *     iagiptr       - input - pointer to a data area describing the
 *                             current iag
 *     msg_info_ptr  - input - pointer to data needed to issue messages
 *                             about the current inode allocation map
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int iags_rebuild(int is_aggregate,
		 int which_it,
		 int which_ait,
		 struct fsck_iag_info *iagiptr,
		 struct fsck_imap_msg_info *msg_info_ptr)
{
	int iir_rc = FSCK_OK;
	int32_t iag_idx = 0;
	int64_t this_ag;
	int32_t iag_alloc_inodes, iag_alloc_exts;

	iir_rc =
	    iag_get_first(is_aggregate, which_it, which_ait,
			  &(iagiptr->iagptr));
	iagiptr->iamrecptr->num_iags = 0;
	iagiptr->iamrecptr->unused_bkd_inodes = 0;
	iagiptr->iamrecptr->bkd_inodes = 0;
	while ((iagiptr->iagptr != NULL) && (iir_rc == FSCK_OK)) {
		/* increment the iag count for this imap */
		iagiptr->iamrecptr->num_iags++;
		iagiptr->iagptr->iagnum = iag_idx;
		this_ag =
		    iagiptr->iagptr->agstart / ((int64_t) sb_ptr->s_agsize);
		if (iagiptr->iagptr->agstart != (this_ag * sb_ptr->s_agsize)) {
			/* not a valid starting block for an AG */
			this_ag = 0;
			iagiptr->iagptr->agstart = 0;
			fsck_send_msg(fsck_BADIAGAGSTRTCRCTD,
				      fsck_ref_msg(msg_info_ptr->msg_mapowner),
				      iag_idx);
		} else if ((this_ag < 0) || (this_ag > MAXAG)) {
			/* not a valid starting block for an AG */
			this_ag = 0;
			iagiptr->iagptr->agstart = 0;
			fsck_send_msg(fsck_BADIAGAGCRCTD,
				      fsck_ref_msg(msg_info_ptr->msg_mapowner),
				      iag_idx, (long long) this_ag);
		}
		/*
		 * count allocations and build maps for this iag
		 */
		iir_rc =
		    iag_alloc_scan(&iag_alloc_inodes, &iag_alloc_exts, iagiptr,
				   msg_info_ptr);
		if (iir_rc != FSCK_OK)
			break;

		/*
		 * record the info in the iag record
		 */
		iagiptr->iagtbl[iag_idx].backed_inodes =
			iag_alloc_exts * INOSPEREXT;
		iagiptr->iagtbl[iag_idx].unused_backed_inodes =
			(iag_alloc_exts * INOSPEREXT) - iag_alloc_inodes;
		iagiptr->iagtbl[iag_idx].unbacked_extents =
			EXTSPERIAG - iag_alloc_exts;
		/*
		 * update the workspace with the AG-related info.
		 */
		iagiptr->iagtbl[iag_idx].AG_num = this_ag;
		iagiptr->agtbl[this_ag].backed_inodes +=
			iagiptr->iagtbl[iag_idx].backed_inodes;
		iagiptr->agtbl[this_ag].unused_backed_inodes +=
			iagiptr->iagtbl[iag_idx].unused_backed_inodes;
		/*
		 * add info for this iag to imap totals
		 */
		iagiptr->iamrecptr->unused_bkd_inodes +=
			iagiptr->iagtbl[iag_idx].unused_backed_inodes;
		iagiptr->iamrecptr->bkd_inodes +=
			iagiptr->iagtbl[iag_idx].backed_inodes;
		/*
		 * rebuild the allocation maps in the iag
		 */
		iir_rc = iag_alloc_rebuild(iag_idx, iagiptr);
		if (iir_rc != FSCK_OK)
			break;

		/*
		 * put this iag and get the next one
		 */
		iir_rc = iag_put(iagiptr->iagptr);
		if (iir_rc != FSCK_OK)
			break;

		iir_rc = iag_get_next(&(iagiptr->iagptr));
		iag_idx++;	/* increment for next iag */
	}
	return (iir_rc);
}

/****************************************************************
 * NAME: iags_validation
 *
 * FUNCTION: Validate each Inode Allocation Group (iag) in the specified
 *           Inode Allocation Map based on fsck's observations of the
 *           aggregate.
 *
 * PARAMETERS:
 *     is_aggregate     - input - !0 => aggregate owned
 *                                 0 => fileset owned
 *     which_it         - input - ordinal number of the aggregate inode
 *                                describing the inode table
 *     which_ait        - input - the aggregate inode table from which
 *                                the it inode should be read
 *                                { fsck_primary | fsck_secondary }
 *     errors_detected  - input - pointer to a variable in which to return
 *                                !0 if errors are detected
 *                                 0 if no errors are detected
 *     iagiptr          - input - pointer to a data area describing the
 *                                current iag
 *     msg_info_ptr     - input - pointer to data needed to issue messages
 *                                about the current inode allocation map
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int iags_validation(int is_aggregate,
		    int which_it,
		    int which_ait,
		    int *errors_detected,
		    struct fsck_iag_info *iagiptr,
		    struct fsck_imap_msg_info *msg_info_ptr)
{
	int iiv_rc = FSCK_OK;
	int32_t iag_idx = 0;
	int64_t this_ag;
	int errors_in_iag = 0;
	int bad_agstrt_in_iag = 0;
	int32_t iag_alloc_inodes, iag_alloc_exts;
	uint32_t old_iam_bkd_inodes;

	iiv_rc = iag_get_first(is_aggregate, which_it, which_ait,
			       &(iagiptr->iagptr));
	iagiptr->iamrecptr->num_iags = 0;
	iagiptr->iamrecptr->unused_bkd_inodes = 0;
	old_iam_bkd_inodes = iagiptr->iamrecptr->bkd_inodes;
	iagiptr->iamrecptr->bkd_inodes = 0;
	while ((iagiptr->iagptr != NULL) && (iiv_rc == FSCK_OK)) {
		/* increment the iag count for this imap */
		iagiptr->iamrecptr->num_iags++;
		if (iagiptr->iagptr->iagnum != iag_idx) {
			/* iag number is wrong */
			errors_in_iag = -1;
			fsck_send_msg(fsck_BADIAGIAGNUM,
				      fsck_ref_msg(msg_info_ptr->msg_mapowner),
				      iag_idx);
		}
		this_ag =
		    iagiptr->iagptr->agstart / ((int64_t) sb_ptr->s_agsize);
		if (iagiptr->iagptr->agstart != (this_ag * sb_ptr->s_agsize)) {
			/* not a valid starting block for an AG */
			bad_agstrt_in_iag = -1;
			errors_in_iag = -1;
			fsck_send_msg(fsck_BADIAGAGSTRT,
				      fsck_ref_msg(msg_info_ptr->msg_mapowner),
				      iag_idx);
		} else if ((this_ag < 0) || (this_ag > MAXAG)) {
			/* not a valid starting block for an AG */
			bad_agstrt_in_iag = -1;
			errors_in_iag = -1;
			fsck_send_msg(fsck_BADIAGAG,
				      fsck_ref_msg(msg_info_ptr->msg_mapowner),
				      iag_idx, (long long) this_ag);
		}
		/*
		 * count allocations and build maps for this iag
		 */
		iiv_rc = iag_alloc_scan(&iag_alloc_inodes, &iag_alloc_exts,
					iagiptr, msg_info_ptr);
		if (iiv_rc != FSCK_OK)
			break;

		/*
		 * record the info in the iag record
		 */
		iagiptr->iagtbl[iag_idx].backed_inodes =
			iag_alloc_exts * INOSPEREXT;
		iagiptr->iagtbl[iag_idx].unused_backed_inodes =
			(iag_alloc_exts * INOSPEREXT) - iag_alloc_inodes;
		iagiptr->iagtbl[iag_idx].unbacked_extents =
			EXTSPERIAG - iag_alloc_exts;
		/*
		 * if we can associate an AG with the IAG then update the
		 * workspace with the AG-related info.
		 */
		if (!bad_agstrt_in_iag) {
			/* we have an AG */
			iagiptr->iagtbl[iag_idx].AG_num = this_ag;
			iagiptr->agtbl[this_ag].backed_inodes +=
				iagiptr->iagtbl[iag_idx].backed_inodes;
			iagiptr->agtbl[this_ag].unused_backed_inodes +=
				iagiptr->iagtbl[iag_idx].unused_backed_inodes;
		}
		/*
		 * add info for this iag to imap totals
		 */
		iagiptr->iamrecptr->unused_bkd_inodes +=
			iagiptr->iagtbl[iag_idx].unused_backed_inodes;
		iagiptr->iamrecptr->bkd_inodes +=
			iagiptr->iagtbl[iag_idx].backed_inodes;
		/*
		 * verify the allocation maps in this iag
		 */
		iiv_rc = iag_alloc_ver(&errors_in_iag, bad_agstrt_in_iag,
				       iag_idx, iagiptr, msg_info_ptr);
		if (iiv_rc != FSCK_OK)
			break;

		/*
		 * sum it up for this iag and get the next one
		 */
		if (errors_in_iag) {
			/* found some thing(s) wrong! */
			agg_recptr->ag_dirty = 1;
			*errors_detected = -1;
			fsck_send_msg(fsck_BADIAG,
				      fsck_ref_msg(msg_info_ptr->msg_mapowner),
				      iag_idx);
		}
		iiv_rc = iag_get_next(&(iagiptr->iagptr));
		/* increment for next iag */
		iag_idx++;
		/* reset for next iag */
		errors_in_iag = 0;
	}
	if ((old_iam_bkd_inodes != iagiptr->iamrecptr->bkd_inodes) &&
	    (iiv_rc == FSCK_OK)) {
		/*
		 * we got no fatal errors but we came up with a different count
		 * of backed inodes on this pass than when we went through
		 * recording and dupchecking the extents.
		 */
		iiv_rc = FSCK_INTERNAL_ERROR_12;
	}
	return (iiv_rc);
}

/****************************************************************
 * NAME: iamap_rebuild
 *
 * FUNCTION:  Rebuild the Inode Allocation Map which describes the specified
 *            Inode Allocation Table using data based on fsck's observations
 *            of the aggregate.
 *
 * PARAMETERS:
 *     is_aggregate  - input - !0 => aggregate owned
 *                              0 => fileset owned
 *     which_it      - input - ordinal number of the aggregate inode
 *                             describing the inode table
 *     which_ait     - input - the aggregate inode table from which
 *                             the it inode should be read
 *                             { fsck_primary | fsck_secondary }
 *     iagiptr       - input - pointer to a data area describing the
 *                             current iag
 *     msg_info_ptr  - input - pointer to data needed to issue messages
 *                             about the current inode allocation map
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int iamap_rebuild(int is_aggregate,
		  int which_it,
		  int which_ait,
		  struct fsck_iag_info *iagiptr,
		  struct fsck_imap_msg_info *msg_info_ptr)
{
	int iamr_rc;
	int agidx;
	/*
	 * initialize the imap lists and counters
	 */
	iagiptr->iamctlptr->in_freeiag = -1;
	for (agidx = 0; (agidx < MAXAG); agidx++) {
		iagiptr->iamctlptr->in_agctl[agidx].inofree = -1;
		iagiptr->iamctlptr->in_agctl[agidx].extfree = -1;
		iagiptr->iamctlptr->in_agctl[agidx].numinos = 0;
		iagiptr->iamctlptr->in_agctl[agidx].numfree = 0;
	}
	/*
	 * set blocks per inode extent fields
	 */
	iagiptr->iamctlptr->in_nbperiext = INODE_EXTENT_SIZE / sb_ptr->s_bsize;
	iagiptr->iamctlptr->in_l2nbperiext =
	    log2shift(iagiptr->iamctlptr->in_nbperiext);
	/*
	 * rebuild the IAGs and collect info to finish the iamap
	 */
	iamr_rc =
	    iags_rebuild(is_aggregate, which_it, which_ait, iagiptr,
			 msg_info_ptr);
	if (iamr_rc != FSCK_OK)
		goto iamr_exit;

	/*
	 * fill in those pesky IAG list back pointers
	 */
	iamr_rc = iags_finish_lists(is_aggregate, which_it, which_ait, iagiptr);
	if (iamr_rc != FSCK_OK)
		goto iamr_exit;

	/*
	 * finish up the control page info and put the control page
	 */
	iagiptr->iamctlptr->in_nextiag = iagiptr->iamrecptr->num_iags;
	iagiptr->iamctlptr->in_numinos = iagiptr->iamrecptr->bkd_inodes;
	iagiptr->iamctlptr->in_numfree = iagiptr->iamrecptr->unused_bkd_inodes;
	for (agidx = 0; (agidx < MAXAG); agidx++) {
		iagiptr->iamctlptr->in_agctl[agidx].numinos =
			iagiptr->agtbl[agidx].backed_inodes;
		iagiptr->iamctlptr->in_agctl[agidx].numfree =
			iagiptr->agtbl[agidx].unused_backed_inodes;
	}
	iamr_rc = inotbl_put_ctl_page(is_aggregate, iagiptr->iamctlptr);

iamr_exit:
	return (iamr_rc);
}

/****************************************************************
 * NAME: iamap_validation
 *
 * FUNCTION:  Validate the Inode Allocation Map which describes the specified
 *            Inode Allocation Table based on fsck's observations of the
 *            aggregate.
 *
 * PARAMETERS:
 *     is_aggregate  - input - !0 => aggregate owned
 *                              0 => fileset owned
 *     which_it      - input - ordinal number of the aggregate inode
 *                             describing the inode table
 *     which_ait     - input - the aggregate inode table from which
 *                             the it inode should be read
 *                             { fsck_primary | fsck_secondary }
 *     iagiptr       - input - pointer to a data area describing the
 *                             current iag
 *     msg_info_ptr  - input - pointer to data needed to issue messages
 *                             about the current inode allocation map
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int iamap_validation(int is_aggregate,
		     int which_it,
		     int which_ait,
		     struct fsck_iag_info *iagiptr,
		     struct fsck_imap_msg_info *msg_info_ptr)
{
	int iamv_rc;
	int imapctl_errors_detected = 0;
	int imap_errors_detected = 0;
	int32_t blksperinoext;
	int agidx;

	blksperinoext = INODE_EXTENT_SIZE / sb_ptr->s_bsize;
	if (iagiptr->iamctlptr->in_nbperiext != blksperinoext) {
		imapctl_errors_detected = -1;
		fsck_send_msg(fsck_BADIAMBPIE, fsck_ref_msg(msg_info_ptr->msg_mapowner));
	}
	if (iagiptr->iamctlptr->in_l2nbperiext != (log2shift(blksperinoext))) {
		imapctl_errors_detected = -1;
		fsck_send_msg(fsck_BADIAML2BPIE, fsck_ref_msg(msg_info_ptr->msg_mapowner));
	}
	iamv_rc = iagfr_list_scan(is_aggregate, which_it, which_ait, iagiptr,
				  &imapctl_errors_detected, msg_info_ptr);
	if (iamv_rc != FSCK_OK)
		goto iamv_exit;

	iamv_rc = agfrino_lists_scan(is_aggregate, which_it, which_ait,
				     iagiptr, &imap_errors_detected,
				     msg_info_ptr);
	if (iamv_rc != FSCK_OK)
		goto iamv_exit;

	iamv_rc = agfrext_lists_scan(is_aggregate, which_it, which_ait,
				     iagiptr, &imap_errors_detected,
				     msg_info_ptr);
	if (iamv_rc != FSCK_OK)
		goto iamv_exit;

	iamv_rc = iags_validation(is_aggregate, which_it, which_ait,
				  &imap_errors_detected, iagiptr, msg_info_ptr);
	if (iamv_rc != FSCK_OK)
		goto iamv_exit;

	if (!iagiptr->iamrecptr->friag_list_bad) {
		/* not already judged invalid */
		iamv_rc = iagfr_list_validation(&imapctl_errors_detected,
						iagiptr->iamrecptr,
						msg_info_ptr);

		if (iamv_rc != FSCK_OK)
			goto iamv_exit;
	}
	iamv_rc = agfrino_lists_validation(is_aggregate, which_it,
					   iagiptr->agtbl,
					   &imap_errors_detected, msg_info_ptr);
	if (iamv_rc != FSCK_OK)
		goto iamv_exit;

	iamv_rc = agfrext_lists_validation(is_aggregate, which_it,
					   iagiptr->agtbl,
					   &imap_errors_detected, msg_info_ptr);
	if (iamv_rc != FSCK_OK)
		goto iamv_exit;
	/*
	 * finish up verifying the control page info
	 */
	if (iagiptr->iamctlptr->in_nextiag != iagiptr->iamrecptr->num_iags) {
		imapctl_errors_detected = -1;
		fsck_send_msg(fsck_BADIAMNXTIAG, fsck_ref_msg(msg_info_ptr->msg_mapowner));
	}
	if (iagiptr->iamctlptr->in_numinos != iagiptr->iamrecptr->bkd_inodes) {
		imapctl_errors_detected = -1;
		fsck_send_msg(fsck_BADIAMNBI, fsck_ref_msg(msg_info_ptr->msg_mapowner));
	}
	if (iagiptr->iamctlptr->in_numfree !=
	    iagiptr->iamrecptr->unused_bkd_inodes) {
		imapctl_errors_detected = -1;
		fsck_send_msg(fsck_BADIAMNFI, fsck_ref_msg(msg_info_ptr->msg_mapowner));
	}
	for (agidx = 0; (agidx < MAXAG); agidx++) {
		if (iagiptr->iamctlptr->in_agctl[agidx].numinos !=
		    iagiptr->agtbl[agidx].backed_inodes) {
			imapctl_errors_detected = -1;
			fsck_send_msg(fsck_BADIAMAGNBI,
				      fsck_ref_msg(msg_info_ptr->msg_mapowner),
				      agidx);
		}
		if (iagiptr->iamctlptr->in_agctl[agidx].numfree !=
		    iagiptr->agtbl[agidx].unused_backed_inodes) {
			imapctl_errors_detected = -1;
			fsck_send_msg(fsck_BADIAMAGNFI,
				      fsck_ref_msg(msg_info_ptr->msg_mapowner),
				      agidx);
		}
	}
	/*
	 * if errors have been detected, issue summary message(s) in
	 * case we're not processing verbose.
	 */
	if (imapctl_errors_detected) {
		agg_recptr->ag_dirty = 1;
		fsck_send_msg(fsck_BADIAMCTL, fsck_ref_msg(msg_info_ptr->msg_mapowner));
	}
	if (imap_errors_detected) {
		agg_recptr->ag_dirty = 1;
		fsck_send_msg(fsck_BADIAM, fsck_ref_msg(msg_info_ptr->msg_mapowner));
	}
iamv_exit:
	return (iamv_rc);
}

/****************************************************************
 * NAME: IM_compare_leaf
 *
 * FUNCTION: Compare the 2 specified Inode Map B+Tree leaf
 *                nodes to ensure that they are logically equivalent.
 *
 * PARAMETERS:
 *	primary_leafptr		- input - pointer to a leaf node in an inode
 *					  in the primary aggregate inode
 *					  allocation table in an fsck buffer
 *	secondary_leafptr	- input - pointer to a leaf node in an inode
 *					  in the secondary aggregate inode
 *					  allocation table in an fsck buffer
 *	inconsistency_detected	- input - pointer to a variable in which
 *					  to return !0 if errors are detected
 *					  0 if no errors are detected
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int IM_compare_leaf(xtpage_t * leaf1ptr, xtpage_t * leaf2ptr,
		    int *inconsistency_detected)
{
	int imcl_rc = FSCK_OK;
	int16_t entry_idx;
	int64_t lf1_offset, lf2_offset;
	int64_t lf1_addr, lf2_addr;
	int32_t lf1_len, lf2_len;

	if ((leaf1ptr->header.flag != leaf2ptr->header.flag) ||
	    (leaf1ptr->header.nextindex != leaf2ptr->header.nextindex) ||
	    (leaf1ptr->header.maxentry != leaf2ptr->header.maxentry)) {
		*inconsistency_detected = -1;
	}

	entry_idx = XTENTRYSTART;
	while ((imcl_rc == FSCK_OK) && (!(*inconsistency_detected)) &&
	       (entry_idx < leaf1ptr->header.nextindex)) {
		lf1_offset = offsetXAD(&(leaf1ptr->xad[entry_idx]));
		lf2_offset = offsetXAD(&(leaf2ptr->xad[entry_idx]));
		if (lf1_offset != lf2_offset) {
			*inconsistency_detected = -1;
		} else {
			/* offsets match */
			lf1_addr = addressXAD(&(leaf1ptr->xad[entry_idx]));
			lf2_addr = addressXAD(&(leaf2ptr->xad[entry_idx]));
			if (lf1_addr != lf2_addr) {
				*inconsistency_detected = -1;
			} else {
				/* addresses match */
				lf1_len =
				    lengthXAD(&(leaf1ptr->xad[entry_idx]));
				lf2_len =
				    lengthXAD(&(leaf2ptr->xad[entry_idx]));
				if (lf1_len != lf2_len) {
					*inconsistency_detected = -1;
				} else {
					/* lengths match */
					entry_idx++;
				}
			}
		}
	}

	return (imcl_rc);
}

/****************************************************************
 * NAME: rebuild_agg_iamap
 *
 * FUNCTION:  Rebuild the Aggregate Inode Allocation Map using data based
 *                 on fsck's observations of the aggregate.
 *
 * PARAMETERS:  none
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int rebuild_agg_iamap()
{
	int raiam_rc = FSCK_OK;
	int is_aggregate = -1;
	int which_it = 0;

	struct fsck_imap_msg_info imap_msg_info;
	struct fsck_imap_msg_info *msg_info_ptr;
	struct fsck_iag_info iag_info;
	struct fsck_iag_info *iag_info_ptr;

	if (agg_recptr->primary_ait_4part1) {
		which_it = fsck_primary;
	} else {
		which_it = fsck_secondary;
	}
	iag_info_ptr = &iag_info;
	iag_info_ptr->iamrecptr = &(agg_recptr->agg_imap);
	iag_info_ptr->iagtbl = agg_recptr->agg_imap.iag_tbl;
	iag_info_ptr->agtbl = agg_recptr->agg_imap.ag_tbl;
	iag_info_ptr->agg_inotbl = is_aggregate;
	msg_info_ptr = &(imap_msg_info);
	msg_info_ptr->msg_mapowner = fsck_aggregate;
	raiam_rc =
	    inotbl_get_ctl_page(is_aggregate, &(iag_info_ptr->iamctlptr));
	if (raiam_rc == FSCK_OK) {
		/* got the imap control page */
		raiam_rc = iamap_rebuild(is_aggregate, which_it, which_it,
					 iag_info_ptr, msg_info_ptr);
	}
	return (raiam_rc);
}

/****************************************************************
 * NAME: rebuild_fs_iamaps
 *
 * FUNCTION:  Rebuild the Fileset Inode Allocation Map using data based
 *            on fsck's observations of the aggregate.
 *
 * PARAMETERS:  none
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int rebuild_fs_iamaps()
{
	int rfsiam_rc = FSCK_OK;
	int is_aggregate = 0;
	/* release 1 there is exactly 1 fileset */
	int which_it = FILESYSTEM_I;
	int which_ait;

	struct fsck_imap_msg_info imap_msg_info;
	struct fsck_imap_msg_info *msg_info_ptr;
	struct fsck_iag_info iag_info;
	struct fsck_iag_info *iag_info_ptr;

	if (agg_recptr->primary_ait_4part2) {
		which_ait = fsck_primary;
	} else {
		which_ait = fsck_secondary;
	}
	iag_info_ptr = &iag_info;
	iag_info_ptr->iamrecptr = &(agg_recptr->fset_imap);
	iag_info_ptr->iagtbl = agg_recptr->fset_imap.iag_tbl;
	iag_info_ptr->agtbl = agg_recptr->fset_imap.ag_tbl;
	iag_info_ptr->agg_inotbl = is_aggregate;
	msg_info_ptr = &(imap_msg_info);
	msg_info_ptr->msg_mapowner = fsck_fileset;
	rfsiam_rc =
	    inotbl_get_ctl_page(is_aggregate, &(iag_info_ptr->iamctlptr));
	if (rfsiam_rc == FSCK_OK) {
		/* got the imap control page */
		rfsiam_rc = iamap_rebuild(is_aggregate, which_it, which_ait,
					  iag_info_ptr, msg_info_ptr);
	}
	return (rfsiam_rc);
}

/****************************************************************
 * NAME: record_dupchk_inode_extents
 *
 * FUNCTION:  For each inode extent in the aggregate (i.e., describing
 *            either the Aggregate Inode Table or in the Fileset Inode
 *            Table), for each block in the extent, record (in the
 *            fsck workspace block map) that the block is allocated
 *            and check to see if this is a duplicate allocation.  (That
 *            is, check to see if any other file system object has
 *            claimed to own that block.)
 *
 * PARAMETERS:  none
 *
 * NOTES:  o Since this routine completes the fsck workspace
 *           initialization needed by inode_get() (et al), this
 *           routine ensures fsck I/O buffers contain the inode
 *           it needs before invoking inode_get().
 *           (inode_get() is invoked to locate the inode within
 *           the fsck I/O buffer.)
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int record_dupchk_inode_extents()
{
	int rdie_rc = FSCK_OK;
	int aggregate_IAGs;
	/* AIT inodes are both aggregate owned */
	int aggregate_IA_Inode = -1;
	int which_it;

	struct fsck_inode_record *inorecptr;
	struct fsck_ino_msg_info ino_msg_info;
	struct fsck_ino_msg_info *msg_info_ptr;

	msg_info_ptr = &(ino_msg_info);
	msg_info_ptr->msg_inonum = AGGREGATE_I;
	msg_info_ptr->msg_inotyp = fsck_metaIAG;
	msg_info_ptr->msg_dxdtyp = fsck_inoext;
	msg_info_ptr->msg_inopfx = fsck_aggr_inode;
	/* inodes representing the Inode Allocation Maps are aggregate inodes */
	rdie_rc = record_imap_info();
	if (rdie_rc == FSCK_OK) {
		/*
		 * record (and check for duplicate allocations) the
		 * inode extents described by aggregate IAGs
		 */
		/* aggregate IAGs */
		aggregate_IAGs = -1;
		if (agg_recptr->primary_ait_4part1) {
			which_it = fsck_primary;
		} else {
			which_it = fsck_secondary;
		}
		msg_info_ptr->msg_inonum = AGGREGATE_I;
		rdie_rc =
		    get_inorecptr(aggregate_IA_Inode, 0, AGGREGATE_I,
				  &inorecptr);
		if ((rdie_rc == FSCK_OK) && (inorecptr == NULL)) {
			rdie_rc = FSCK_INTERNAL_ERROR_45;
		} else if (rdie_rc == FSCK_OK) {
			rdie_rc =
			    record_dupchk_inoexts(aggregate_IAGs, which_it,
						  &(agg_recptr->agg_imap.
						    bkd_inodes), inorecptr,
						  msg_info_ptr);
		}
		if (rdie_rc == FSCK_OK) {
			agg_recptr->inodes_in_aggregate =
			    agg_recptr->agg_imap.bkd_inodes;
			agg_recptr->blocks_for_inodes =
			    (agg_recptr->agg_imap.bkd_inodes *
			     sizeof (struct dinode)) / sb_ptr->s_bsize;
			/*
			 * record (and check for duplicate allocations) the
			 * inode extents described by fileset IAGs
			 *
			 * (In release 1 there is exactly 1 fileset)
			 */
			/* fileset IAGs */
			aggregate_IAGs = 0;
			which_it = FILESYSTEM_I;
			msg_info_ptr->msg_inonum = FILESYSTEM_I;
			rdie_rc =
			    get_inorecptr(aggregate_IA_Inode, 0, FILESYSTEM_I,
					  &inorecptr);
			if ((rdie_rc == FSCK_OK) && (inorecptr == NULL)) {
				rdie_rc = FSCK_INTERNAL_ERROR_20;
			} else if (rdie_rc == FSCK_OK) {
				rdie_rc =
				    record_dupchk_inoexts(aggregate_IAGs,
							  which_it,
							  &(agg_recptr->
							    fset_imap.
							    bkd_inodes),
							  inorecptr,
							  msg_info_ptr);
			}
			if (rdie_rc == FSCK_OK) {
				agg_recptr->inodes_in_aggregate +=
				    agg_recptr->fset_imap.bkd_inodes;
				agg_recptr->blocks_for_inodes +=
				    (agg_recptr->fset_imap.bkd_inodes *
				     sizeof (struct dinode)) / sb_ptr->s_bsize;
			}
		}
	}
	return (rdie_rc);
}

/****************************************************************
 * NAME: record_dupchk_inoexts
 *
 * FUNCTION:  For each inode extent in the specified Inode Table (either
 *            the Aggregate Inode Table or in the Fileset Inode Table),
 *            for each block in the extent, record (in the fsck workspace
 *            block map) that the block is allocated and check to see if
 *            this is a duplicate allocation.  (That is, check to see if
 *            any other file system object has claimed to own that block.)
 *
 * PARAMETERS:
 *     is_aggregate        - input - !0 => aggregate owned
 *                                    0 => fileset owned
 *     which_it            - input - ordinal number of the aggregate inode
 *                                   describing the inode table
 *     backed_inode_count  - input - pointer to a variable in which to return
 *                                   the count of backed inodes described
 *                                   by inode extents described by iags in
 *                                   the current inode allocation table.
 *     inorecptr           - input - pointer to an fsck inode record which
 *                                   describes the current inode allocation
 *                                   table inode
 *     msginfoptr          - input - pointer to data needed to issue messages
 *                                   about the current inode
 *
 * NOTES:  o Since this routine completes the fsck workspace
 *           initialization needed by inode_get() (et al), this
 *           routine ensures fsck I/O buffers contain the inode
 *           it needs before invoking inode_get().
 *           (inode_get() is invoked to locate the inode within
 *           the fsck I/O buffer.)
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int record_dupchk_inoexts(int is_aggregate,
			  int which_it,
			  int32_t * backed_inode_count,
			  struct fsck_inode_record *inorecptr,
			  struct fsck_ino_msg_info *msginfoptr)
{
	int irdie_rc = FSCK_OK;
	int which_ait;
	struct iag *iagptr;
	uint32_t ext_idx;
	uint32_t ext_idx_max;
	int64_t ext_addr;
	uint32_t ext_len;
	int8_t is_EA = 0;
	int8_t is_ACL = 0;
	int8_t extent_is_valid;
	uint32_t adjusted_length;
	uint32_t first_inonum, last_inonum;

	ext_idx_max = EXTSPERIAG - 1;
	*backed_inode_count = 0;
	if (is_aggregate) {
		if (agg_recptr->primary_ait_4part1) {
			which_ait = fsck_primary;
		} else {
			which_ait = fsck_secondary;
		}
	} else {
		if (agg_recptr->primary_ait_4part2) {
			which_ait = fsck_primary;
		} else {
			which_ait = fsck_secondary;
		}
	}
	irdie_rc = ait_special_read_ext1(which_ait);
	if (irdie_rc == FSCK_OK) {
		irdie_rc =
		    iag_get_first(is_aggregate, which_it, which_ait, &iagptr);
	} else {
		report_readait_error(irdie_rc, FSCK_FAILED_CANTREADAITEXT9,
				     which_ait);
		irdie_rc = FSCK_FAILED_CANTREADAITEXT9;
	}
	while ((irdie_rc == FSCK_OK) && (iagptr != NULL)) {
		for (ext_idx = 0;
		     ((ext_idx <= ext_idx_max) && (irdie_rc == FSCK_OK));
		     ext_idx++) {

			ext_addr = addressPXD(&(iagptr->inoext[ext_idx]));
			if (ext_addr == 0)
				continue;

			/* the extent is allocated */
			*backed_inode_count += INOSPEREXT;
			ext_len = lengthPXD(&(iagptr->inoext[ext_idx]));
			irdie_rc = process_extent(inorecptr, ext_len, ext_addr,
						  is_EA, is_ACL, msginfoptr,
						  &adjusted_length,
						  &extent_is_valid,
						  FSCK_RECORD_DUPCHECK);
			if (extent_is_valid)
				continue;

			/* yuck! */
			irdie_rc = process_extent(inorecptr, ext_len, ext_addr,
						  is_EA, is_ACL, msginfoptr,
						  &adjusted_length,
						  &extent_is_valid,
						  FSCK_UNRECORD);
			first_inonum = (iagptr->iagnum * INOSPERIAG) +
				       (ext_idx * INOSPEREXT);
			last_inonum = first_inonum + INOSPEREXT - 1;
			/*
			 * notify the user
			 */
			fsck_send_msg(fsck_BADIAMIAGPXDU, first_inonum, last_inonum);
			/*
			 * log more details
			 */
			fsck_send_msg(fsck_BADIAMIAGPXDL, first_inonum, last_inonum,
				      fsck_ref_msg(msginfoptr->msg_inopfx),
				      msginfoptr->msg_inonum,
				      iagptr->iagnum, ext_idx);
			/*
			 * fix it if we can
			 */
			if (agg_recptr->processing_readwrite) {
				/* we can fix this */
				PXDaddress(&(iagptr->inoext[ext_idx]), 0);
				PXDlength(&(iagptr->inoext[ext_idx]), 0);
				irdie_rc = iag_put(iagptr);
				*backed_inode_count -= INOSPEREXT;
				fsck_send_msg(fsck_WILLRELEASEINOS,
					      first_inonum, last_inonum);
			} else {
				/* we don't have write access */
				agg_recptr->ag_dirty = 1;
				irdie_rc = FSCK_FAILED_IAG_CORRUPT_PXD;
				fsck_send_msg(fsck_CANTRECOVERINOS,
					      first_inonum, last_inonum);
			}
		}
		if (irdie_rc == FSCK_OK)
			irdie_rc = iag_get_next(&iagptr);
	}
	return (irdie_rc);
}

/****************************************************************
 * NAME: record_imap_info
 *
 * FUNCTION:  Find and then record, in the fsck global aggregate record,
 *            information describing the Inode Allocation Maps (i.e.,
 *            the Aggregate Inode Allocation Map and the Fileset Inode
 *            Allocation Map) which will be used to expedite subsequent
 *            access to the maps.
 *
 * PARAMETERS:  none
 *
 * NOTES:  o Since this routine completes the fsck workspace
 *           initialization needed by inode_get() (et al), this
 *           routine ensures fsck I/O buffers contain the inode
 *           it needs before invoking inode_get().
 *           (inode_get() is invoked to locate the inode within
 *           the fsck I/O buffer.)
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int record_imap_info()
{
	int rii_rc = FSCK_OK;
	int intermed_rc = FSCK_OK;
	int aggregate_inode = 1;	/* aggregate owned inodes */
	int which_ait;
	uint32_t ino_idx;
	int64_t leaf_fsblk_offset;
	struct dinode *ino_ptr;
	xtpage_t *dummy_page;
	int8_t inline_data;

	if (agg_recptr->primary_ait_4part1) {
		which_ait = fsck_primary;
	} else {
		which_ait = fsck_secondary;
	}
	intermed_rc = ait_special_read_ext1(which_ait);
	if (intermed_rc != FSCK_OK) {
		report_readait_error(intermed_rc, FSCK_FAILED_CANTREADAITEXTA,
				     which_ait);
		rii_rc = FSCK_FAILED_CANTREADAITEXTA;
		goto rii_exit;
	}
	/* got the inode extension needed for part 1 */
	/* the aggregate inode map inode */
	ino_idx = AGGREGATE_I;
	rii_rc = inode_get(aggregate_inode, which_ait, ino_idx, &ino_ptr);
	if (rii_rc != FSCK_OK) {
		/* but we read this before! */
		rii_rc = FSCK_FAILED_SELF_READ2;
		goto rii_exit;
	}
	/* successful read on aggregate inode map inode */
	rii_rc = find_first_leaf(ino_ptr, &dummy_page, &leaf_fsblk_offset,
				 &inline_data,
				 &(agg_recptr->agg_imap.imap_is_rootleaf));
	if (rii_rc != FSCK_OK) {
		/* but it had a valid tree before */
		rii_rc = FSCK_FAILED_SELF_NOWBAD;
		goto rii_exit;
	}
	/* got the info for the aggregate imap */
	agg_recptr->agg_imap.first_leaf_offset = leaf_fsblk_offset *
						 sb_ptr->s_bsize;
	if ((agg_recptr->primary_ait_4part1 && !agg_recptr->primary_ait_4part2)
	    || (!agg_recptr->primary_ait_4part1 &&
		agg_recptr->primary_ait_4part2)) {
		/*
		 * part1 and part2 of the AIT are not being
		 * used from the same table
		 */
		if (agg_recptr->primary_ait_4part2) {
			which_ait = fsck_primary;
		} else {
			which_ait = fsck_secondary;
		}
		intermed_rc = ait_special_read_ext1(which_ait);
		if (intermed_rc != FSCK_OK) {
			report_readait_error(intermed_rc,
					     FSCK_FAILED_CANTREADAITEXTB,
					     which_ait);
			rii_rc = FSCK_FAILED_CANTREADAITEXTB;
			goto rii_exit;
		}
	}
	/* inode extent is in the buffer */
	/* the aggregate inode map inode */
	ino_idx = FILESYSTEM_I;
	rii_rc = inode_get(aggregate_inode, which_ait, ino_idx, &ino_ptr);
	if (rii_rc != FSCK_OK) {
		/* but we read this before! */
		rii_rc = FSCK_FAILED_AGFS_READ2;
		goto rii_exit;
	}
	/* successful read on aggregate inode map inode */
	rii_rc = find_first_leaf(ino_ptr, &dummy_page, &leaf_fsblk_offset,
				 &inline_data,
				 &(agg_recptr->fset_imap.imap_is_rootleaf));
	if (rii_rc != FSCK_OK) {
		/* but it had a valid tree before */
		rii_rc = FSCK_FAILED_AGFS_NOWBAD;
		goto rii_exit;
	}
	/* got the info for the fileset imap */
	agg_recptr->fset_imap.first_leaf_offset = leaf_fsblk_offset *
						  sb_ptr->s_bsize;
rii_exit:
	return (rii_rc);
}

/****************************************************************
 * NAME: verify_agg_iamap
 *
 * FUNCTION:  Verify that the Aggregate Inode Allocation Map is correct
 *            according to fsck's observations of the aggregate.
 *
 * PARAMETERS:  none
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int verify_agg_iamap()
{
	int vaiam_rc = FSCK_OK;
	int is_aggregate = -1;
	int which_it = 0;

	struct fsck_imap_msg_info imap_msg_info;
	struct fsck_imap_msg_info *msg_info_ptr;
	struct fsck_iag_info iag_info;
	struct fsck_iag_info *iag_info_ptr;

	if (agg_recptr->primary_ait_4part1) {
		which_it = fsck_primary;
	} else {
		which_it = fsck_secondary;
	}
	iag_info_ptr = &iag_info;
	iag_info_ptr->iamrecptr = &(agg_recptr->agg_imap);
	iag_info_ptr->iagtbl = agg_recptr->agg_imap.iag_tbl;
	iag_info_ptr->agtbl = agg_recptr->agg_imap.ag_tbl;
	iag_info_ptr->agg_inotbl = is_aggregate;
	msg_info_ptr = &(imap_msg_info);
	msg_info_ptr->msg_mapowner = fsck_aggregate;
	vaiam_rc =
	    inotbl_get_ctl_page(is_aggregate, &(iag_info_ptr->iamctlptr));
	if (vaiam_rc == FSCK_OK) {
		/* got the imap control page */
		vaiam_rc = iamap_validation(is_aggregate, which_it, which_it,
					    iag_info_ptr, msg_info_ptr);
	}
	return (vaiam_rc);
}

/****************************************************************
 * NAME: verify_fs_iamaps
 *
 * FUNCTION:  Verify that the Fileset Inode Allocation Map is correct
 *            according to fsck's observations of the aggregate.
 *
 * PARAMETERS:  none
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int verify_fs_iamaps()
{
	int vfsiam_rc = FSCK_OK;
	int is_aggregate = 0;
	/* release 1 there is exactly 1 fileset */
	int which_it = FILESYSTEM_I;
	int which_ait;

	struct fsck_imap_msg_info imap_msg_info;
	struct fsck_imap_msg_info *msg_info_ptr;
	struct fsck_iag_info iag_info;
	struct fsck_iag_info *iag_info_ptr;

	if (agg_recptr->primary_ait_4part2) {
		which_ait = fsck_primary;
	} else {
		which_ait = fsck_secondary;
	}
	iag_info_ptr = &iag_info;
	iag_info_ptr->iamrecptr = &(agg_recptr->fset_imap);
	iag_info_ptr->iagtbl = agg_recptr->fset_imap.iag_tbl;
	iag_info_ptr->agtbl = agg_recptr->fset_imap.ag_tbl;
	iag_info_ptr->agg_inotbl = is_aggregate;
	msg_info_ptr = &(imap_msg_info);
	msg_info_ptr->msg_mapowner = fsck_fileset;
	vfsiam_rc =
	    inotbl_get_ctl_page(is_aggregate, &(iag_info_ptr->iamctlptr));
	if (vfsiam_rc == FSCK_OK) {
		/* got the imap control page */
		vfsiam_rc = iamap_validation(is_aggregate, which_it, which_ait,
					     iag_info_ptr, msg_info_ptr);
	}
	return (vfsiam_rc);
}

/*--------------------------------------------------------------------
 * NAME: xtAppend
 *
 * FUNCTION: Append an extent to the specified file
 *
 * PARAMETERS:
 *      di      - Inode to add extent to
 *      offset  - offset of extent to add
 *      blkno   - block number of start of extent to add
 *      nblocks - number of blocks in extent to add
 *      xpage   - xtree page to add extent to
 *
 * NOTES: xpage points to its parent in the xtree and its rightmost child (if it
 *      has one).  It also points to the buffer for the page.
 *
 * RETURNS: 0 for success; Other indicates failure
 */
int xtAppend(struct dinode *di,
	     int64_t offset,
	     int64_t blkno, int32_t nblocks, struct xtree_buf *xpage)
{
	int rc = 0;
	int32_t index;
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
			rc = xtSplitRoot(di, xpage, offset, nblocks, blkno);
		} else {
			/*
			 * Non-root page: add new page at this level,
			 * xtSplitPage() calls xtAppend again to propogate up
			 * the new page entry
			 */
			rc = xtSplitPage(di, xpage, offset, nblocks, blkno);
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
 *
 * RETURNS: 0 for success; Other indicates failure
 */
int xtSplitPage(struct dinode *ip,
		struct xtree_buf *xpage,
		int64_t offset, int32_t nblocks, int64_t blkno)
{
	int rc = 0;
	int64_t xaddr;		/* new right page block number */
	xad_t *xad;
	int32_t xlen;
	xtpage_t *lastpage, *newpage;
	int64_t leftbn;

	/* Allocate disk space for the new xtree page */
	xlen = 1 << agg_recptr->log2_blksperpg;
	rc = fsck_alloc_fsblks(xlen, &xaddr);
	if (rc)
		return rc;

	/*
	 * Modify xpage's next entry to point to the new disk space,
	 * write the xpage to disk since we won't be needing it anymore.
	 */
	lastpage = xpage->page;
	lastpage->header.next = xaddr;

	leftbn = addressPXD(&(lastpage->header.self));

	/* swap if on bid endian machine */
	ujfs_swap_xtpage_t(lastpage);
	rc = ujfs_rw_diskblocks(Dev_IOPort, leftbn * sb_ptr->s_bsize, PSIZE,
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
	rc = xtAppend(ip, offset, xaddr, xlen, xpage->up);

	/* Update inode to account for new page */
	ip->di_nblocks += xlen;

	return rc;
}

/*--------------------------------------------------------------------
 * NAME: xtSplitRoot
 *
 * FUNCTION: Split full root of xtree
 *
 * PARAMETERS:
 *      ip      - Inode of xtree
 *      xroot   - Root of xtree
 *      offset  - Offset of extent to add
 *      nblocks - number of blocks for extent to add
 *      blkno   - starting block of extent to add
 *
 * RETURNS: 0 for success; Other indicates failure
 */
int xtSplitRoot(struct dinode *ip,
		struct xtree_buf *xroot,
		int64_t offset, int32_t nblocks, int64_t blkno)
{
	xtpage_t *rootpage;
	xtpage_t *newpage;
	int64_t xaddr;
	int32_t nextindex;
	xad_t *xad;
	int rc;
	struct xtree_buf *newbuf;
	int32_t xlen;
	int I_am_logredo = 0;

	/* Allocate and initialize buffer for new page to accomodate the split
	 */
	rc = alloc_wrksp(sizeof (struct xtree_buf),
			 dynstg_xtreebuf, I_am_logredo, (void **) &newbuf);
	if (rc)
		return (rc);

	newbuf->up = xroot;
	if (xroot->down == NULL) {
		fsim_node_pages = newbuf;
	} else {
		xroot->down->up = newbuf;
	}
	newbuf->down = xroot->down;
	xroot->down = newbuf;
	rc = alloc_wrksp(PSIZE, dynstg_xtreepagebuf, I_am_logredo,
			 (void **) &newpage);
	if (rc)
		return (rc);
	newbuf->page = newpage;

	/* Allocate disk blocks for new page */
	xlen = 1 << agg_recptr->log2_blksperpg;
	rc = fsck_alloc_fsblks(xlen, &xaddr);
	if (rc)
		return rc;

	rootpage = xroot->page;

	/* Initialize new page */
	if ((rootpage->header.flag & BT_LEAF) == BT_LEAF) {
		newpage->header.flag = BT_LEAF;
	} else {
		newpage->header.flag = BT_INTERNAL;
	}
	PXDlength(&(newpage->header.self), xlen);
	PXDaddress(&(newpage->header.self), xaddr);
	newpage->header.nextindex = XTENTRYSTART;
	newpage->header.maxentry = XTPAGEMAXSLOT;

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
