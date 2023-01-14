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
#include <ctype.h>
#include <getopt.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
/* defines and includes common among the jfs_fsck modules */
#include "xfsckint.h"
#include "xchkdsk.h"
#include "fsck_message.h"	/* message text, all messages, in english */
#include "jfs_byteorder.h"
#include "jfs_unicode.h"
#include "jfs_version.h"	/* version number and date for utils */
#include "logform.h"
#include "logredo.h"
#include "message.h"
#include "super.h"
#include "utilsubs.h"

int64_t ondev_jlog_byte_offset;

/* + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + +
 *
 * superblock buffer and pointer
 *
 *    values are assigned by the xchkdsk routine
 */
struct superblock aggr_superblock;
struct superblock *sb_ptr;

/* + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + +
 *
 * fsck aggregate info structure and pointer
 *
 *    values are assigned by the xchkdsk routine
 */
struct fsck_agg_record agg_record;
struct fsck_agg_record *agg_recptr;

/* + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + +
 *
 * fsck block map info structure and pointer
 *
 *    values are assigned by the xchkdsk routine
 */
struct fsck_bmap_record bmap_record;
struct fsck_bmap_record *bmap_recptr;

/* + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + +
 *
 * For message processing
 *
 *    values are assigned by the xchkdsk routine
 */

char *Vol_Label;
char *program_name;

struct tm *fsck_DateTime = NULL;
char time_stamp[20];

/* + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + +
 *
 * For directory entry processing
 *
 */
int32_t key_len[2];
UniChar key[2][JFS_NAME_MAX];
UniChar ukey[2][JFS_NAME_MAX];

int32_t Uni_Name_len;
UniChar Uni_Name[JFS_NAME_MAX];
int32_t Str_Name_len;
char Str_Name[JFS_NAME_MAX];

/* + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + +
 *
 * Device information.
 *
 *     values are assigned when (if) the device is opened.
 */
FILE *Dev_IOPort;
uint32_t Dev_blksize;
int32_t Dev_SectorSize;
char log_device[512] = { 0 };

/* + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + +
 *
 * Unicode path strings information.
 *
 *     values are assigned when the fsck aggregate record is initialized.
 *     accessed via addresses in the fack aggregate record.
 */
UniChar uni_LSFN_NAME[11] =
    { 'L', 'O', 'S', 'T', '+', 'F', 'O', 'U', 'N', 'D' };
UniChar uni_lsfn_name[11] =
    { 'l', 'o', 's', 't', '+', 'f', 'o', 'u', 'n', 'd' };

/* + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + +
 *
 * fsckwsp error handling fields
 *
 *     values are assigned when the fsck workspace storage is
 *     allocated.
 */
int wsp_dynstg_action;
int wsp_dynstg_object;

/* VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV
 *
 * The following are internal to this file
 *
 */
int check_parents_and_first_refs(void);
int create_lost_and_found(void);
int final_processing(void);
int initial_processing(int, char **);
void parse_parms(int, char **);
int phase0_processing(void);
int phase1_processing(void);
int phase2_processing(void);
int phase3_processing(void);
int phase4_processing(void);
int phase5_processing(void);
int phase6_processing(void);
int phase7_processing(void);
int phase8_processing(void);
int phase9_processing(void);
int repair_fs_inodes(void);
int report_problems_setup_repairs(void);
int resolve_lost_and_found(void);
int validate_fs_inodes(void);
int verify_parms(void);
void ask_continue(void);
void fsck_usage(void);

/* VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV */

/* exit value */
int exit_value = FSCK_OK;

/*****************************************************************************
 * NAME: main (jfs_fsck)
 *
 * FUNCTION: Entry point for jfs check/repair of aggregate
 *
 * INTERFACE:
 *            jfs_fsck <device name>
 *
 *                         [ -a ]
 *                         autocheck mode
 *                         replay the transaction log and quit fsck unless
 *                         aggregate state is dirty or log replay failed
 *
 *                         [ -f ]
 *                         force check
 *                         replay the tranaction log and force checking
 *
 *                         [ -j journal_device ]
 *                         specify the external journal device
 *
 *                         [ -n ]
 *                         read only check
 *                         report but do not repair problems
 *
 *                         [ --omit_journal_replay ]
 *                         omit replay of the transaction log
 *
 *                         [ -p ]
 *                         preen
 *                         same functionality as -a
 *
 *                         [ --replay_journal_only ]
 *                         only replay the transaction log
 *
 *                         [ -v ]
 *                         verbose messaging
 *
 *                         [ -V ]
 *                         version information
 *                         print version information and exit
 *
 * RETURNS:
 *      success:                   FSCK_OK (0)
 *      log successfully replayed: FSCK_CORRECTED (1)
 *      errors corrected:          FSCK_CORRECTED (1)
 *      errors uncorrected:        FSCK_ERRORS_UNCORRECTED (4)
 *      operational error:         FSCK_OP_ERROR (8)
 *      usage error:               FSCK_USAGE_ERROR (16)
 */
int main(int argc, char **argv)
{

	int rc = FSCK_OK;
	time_t Current_Time;

	/*
	 * some basic initializations
	 */
	sb_ptr = &aggr_superblock;
	agg_recptr = &agg_record;
	bmap_recptr = &bmap_record;

#ifdef _JFS_DEBUG
	printf("sb_ptr = %p   agg_recptr = %p   bmap_recptr = %p\n", sb_ptr,
	       agg_recptr, bmap_recptr);
#endif

	if (argc && **argv)
		program_name = *argv;
	else
		program_name = "jfs_fsck";

	printf("%s version %s, %s\n", program_name, VERSION, JFSUTILS_DATE);

	wsp_dynstg_action = dynstg_unknown;
	wsp_dynstg_object = dynstg_unknown;

	/* init workspace aggregate record
	 * (the parms will be recorded in it)
	 */
	rc = init_agg_record();
	/*
	 * Allocate the multi-purpose buffer now so that it can be
	 * used during superblock verification.
	 *
	 * This must be done at least before calling logredo to ensure
	 * that the malloc() will succeed.
	 * (In autocheck mode, logredo is likely to eat up all the
	 * low memory.  We don't want to use the alloc_wrksp routine
	 * because we want a page boundary without having to burn
	 * 4096 extra bytes.
	 */
	if ((rc = alloc_vlarge_buffer()) != FSCK_OK) {
		/* alloc_vlarge_buffer not OK */
		exit_value = FSCK_OP_ERROR;
		goto main_exit;
	}

	if ((rc = initial_processing(argc, argv)) != FSCK_OK) {
		/*
		 * Something very wrong has happened.  We're not
		 * even sure we're checking a JFS file system!
		 * Appropriate messages should already be logged.
		 */
		/* initial_processing sets exit value if unsuccessful */
		goto main_exit;
	}

	if (!agg_recptr->stdout_redirected) {
		/* begin the "running" indicator */
		fsck_hbeat_start();
	}
#ifdef CLEARBADBLOCK
	/*
	 * If they specified Clear Bad Blocks List only (aka /B),
	 * release everything that's allocated, close everything
	 * that's open, and then initiate the requested processing.
	 */
	if ((agg_recptr->parm_options[UFS_CHKDSK_CLRBDBLKLST])
	    && (!agg_recptr->fsck_is_done)) {
		/* bad block list processing only */
		/*
		 * this path is taken only when -f not specified, so
		 * fsck processing is readonly, but the clrbblks
		 * processing requires fsck to do some things it only
		 * permits when processing readwrite.  So we reset the
		 * switches temporarily and take care what routines we call.
		 */
		agg_recptr->processing_readwrite = 1;
		agg_recptr->processing_readonly = 0;
		/*
		 * JFS Clear Bad Blocks List processing
		 *
		 * If things go well, this will issue messages and
		 * write to the service log.
		 */
		rc = establish_wsp_block_map_ctl();

		/*
		 * terminate fsck service logging
		 */
		fscklog_end();
		/*
		 * restore the original values.
		 */
		agg_recptr->processing_readwrite = 0;
		agg_recptr->processing_readonly = 1;
		/*
		 * release any workspace that has been allocated
		 */
		workspace_release();
		/*
		 * Close (Unlock) the device
		 */
		if (agg_recptr->device_is_open) {
			close_volume();
		}
		/*
		 * Then exit
		 */
		if (!agg_recptr->stdout_redirected) {
			/* end the "running" indicator */
			fsck_hbeat_stop();
		}

		return (rc);
	}
#endif

	if (agg_recptr->fsck_is_done)
		goto phases_complete;
	rc = phase0_processing();
	if (agg_recptr->fsck_is_done)
		goto phases_complete;
	/*
	 * If -n flag was specified, disable write processing now
	 */
	if (agg_recptr->parm_options[UFS_CHKDSK_LEVEL0]) {
		agg_recptr->processing_readonly = 1;
		agg_recptr->processing_readwrite = 0;
	}
	rc = phase1_processing();
	if (agg_recptr->fsck_is_done)
		goto phases_complete;
	rc = phase2_processing();
	if (agg_recptr->fsck_is_done)
		goto phases_complete;
	rc = phase3_processing();
	if (agg_recptr->fsck_is_done)
		goto phases_complete;
	rc = phase4_processing();
	if (agg_recptr->fsck_is_done)
		goto phases_complete;
	rc = phase5_processing();
	if (agg_recptr->fsck_is_done)
		goto phases_complete;
	rc = phase6_processing();
	if (agg_recptr->fsck_is_done)
		goto phases_complete;
	rc = phase7_processing();
	if (agg_recptr->fsck_is_done)
		goto phases_complete;
	rc = phase8_processing();
	if (agg_recptr->fsck_is_done)
		goto phases_complete;
	rc = phase9_processing();

      phases_complete:
	if (!agg_recptr->superblk_ok) {
		/* superblock is bad */
		exit_value = FSCK_ERRORS_UNCORRECTED;
		goto close_vol;
	}

	/* we at least have a superblock */
	if ((rc == FSCK_OK) && (!(agg_recptr->fsck_is_done))) {
		/* not fleeing an error and not making a speedy exit */

		/* finish up and display some information */
		rc = final_processing();

		/* flush the I/O buffers to complete any pending writes */
		if (rc == FSCK_OK) {
			rc = blkmap_flush();
		} else {
			blkmap_flush();
		}

		if (rc == FSCK_OK) {
			rc = blktbl_dmaps_flush();
		} else {
			blktbl_dmaps_flush();
		}

		if (rc == FSCK_OK) {
			rc = blktbl_Ln_pages_flush();
		} else {
			blktbl_Ln_pages_flush();
		}

		if (rc == FSCK_OK) {
			rc = iags_flush();
		} else {
			iags_flush();
		}

		if (rc == FSCK_OK) {
			rc = inodes_flush();
		} else {
			inodes_flush();
		}

		if (rc == FSCK_OK) {
			rc = mapctl_flush();
		} else {
			mapctl_flush();
		}

		if (rc == FSCK_OK) {
			rc = flush_index_pages();
		} else {
			flush_index_pages();
		}
	}
	/*
	 * last chance to write to the wsp block map control page...
	 */
	Current_Time = time(NULL);
	fsck_DateTime = localtime(&Current_Time);

	sprintf(time_stamp, "%d/%d/%d %d:%02d:%02d",
		fsck_DateTime->tm_mon + 1,
		fsck_DateTime->tm_mday, (fsck_DateTime->tm_year + 1900),
		fsck_DateTime->tm_hour, fsck_DateTime->tm_min,
		fsck_DateTime->tm_sec);

	if (agg_recptr->processing_readwrite) {
		/* on-device fsck workspace block map */
		if (agg_recptr->blkmp_ctlptr != NULL) {
			memcpy(&agg_recptr->blkmp_ctlptr->hdr.end_time[0],
			       &time_stamp[0], 20);
			agg_recptr->blkmp_ctlptr->hdr.return_code = rc;
			blkmap_put_ctl_page(agg_recptr->blkmp_ctlptr);
		}
	}
	if (rc == FSCK_OK) {
		/* either all ok or nothing fatal */
		if (agg_recptr->processing_readonly) {
			/* remind the caller not to take
			 * any messages issued too seriously
			 */
			fsck_send_msg(fsck_READONLY);
			if (agg_recptr->corrections_needed ||
			    agg_recptr->corrections_approved) {
				fsck_send_msg(fsck_ERRORSDETECTED);
				exit_value = FSCK_ERRORS_UNCORRECTED;
			}
		}
		/* may write to superblocks again */
		rc = agg_clean_or_dirty();
	}

	if (agg_recptr->ag_modified) {
		/* wrote to it at least once */
		fsck_send_msg(fsck_MODIFIED);
	}
	if (agg_recptr->ag_dirty) {
		exit_value = FSCK_ERRORS_UNCORRECTED;
	}

	/*
	 * Log fsck exit
	 */
	fsck_send_msg(fsck_SESSEND, time_stamp, rc, exit_value);

	/*
	 * terminate fsck service logging
	 */
	fscklog_end();
        /*
	 * release all workspace that has been allocated
	 */
	if (rc == FSCK_OK) {
		rc = workspace_release();
	} else {
		workspace_release();
	}

      close_vol:
	/*
	 * Close (Unlock) the device
	 */
	if (agg_recptr->device_is_open) {
		if (rc == FSCK_OK) {
			rc = close_volume();
		} else {
			close_volume();
		}
	}

	if (!agg_recptr->stdout_redirected) {
		/* end the "running" indicator */
		fsck_hbeat_stop();
	}

      main_exit:
	return (exit_value);
}


/*****************************************************************************
 * NAME: check_parents_and_first_refs
 *
 * FUNCTION:  If any aggregate blocks are multiply allocated, find the
 *            first reference for each.  Verify that the parent inode
 *            number stored in each directory inode matches the parent
 *            inode observed by fsck for that inode.
 *
 * PARAMETERS:  none
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int check_parents_and_first_refs()
{
	int rc = FSCK_OK;
	uint32_t ino_idx;
	int aggregate_inode = 0;	/* going for fileset inodes only */
	int alloc_ifnull = 0;
	int which_it = 0;	/* in release 1 there is only fileset 0 */
	int inode_already_read, done;
	int dir_w_hlinks_seen = 0;
	int dir_w_incrrct_prnt_seen = 0;
	int unalloc_ino_w_prnts_seen = 0;
	int unconnected_inode_seen = 0;
	struct dinode *inoptr;
	struct fsck_inode_ext_record *this_ext;
	struct fsck_inode_record *this_inorec;
	struct fsck_inode_record *parent_inorec;

	struct fsck_ino_msg_info ino_msg_info;
	struct fsck_ino_msg_info *msg_info_ptr;

	msg_info_ptr = &ino_msg_info;
	msg_info_ptr->msg_inopfx = fsck_fset_inode;	/* all fileset owned */

	rc = get_inorecptr_first(aggregate_inode, &ino_idx, &this_inorec);

	while ((rc == FSCK_OK) && (this_inorec != NULL)) {
		msg_info_ptr->msg_inonum = ino_idx;
		if (this_inorec->inode_type == directory_inode) {
			msg_info_ptr->msg_inotyp = fsck_directory;
		} else if (this_inorec->inode_type == symlink_inode) {
			msg_info_ptr->msg_inotyp = fsck_symbolic_link;
		} else if (this_inorec->inode_type == char_special_inode) {
			msg_info_ptr->msg_inotyp = fsck_char_special;
		} else if (this_inorec->inode_type == block_special_inode) {
			msg_info_ptr->msg_inotyp = fsck_block_special;
		} else if (this_inorec->inode_type == FIFO_inode) {
			msg_info_ptr->msg_inotyp = fsck_FIFO;
		} else if (this_inorec->inode_type == SOCK_inode) {
			msg_info_ptr->msg_inotyp = fsck_SOCK;
		} else {	/* a regular file */
			msg_info_ptr->msg_inotyp = fsck_file;
		}

		if (this_inorec->in_use)
			goto inode_in_use;

		/* not in use.  A record allocated means some
		 * directory thinks this inode is its parent
		 */
		done = 0;
		if ((this_inorec->parent_inonum != ROOT_I)
		    || (!agg_recptr->rootdir_rebuilt)) {
			/*
			 * either this parent isn't the root or else
			 * the root dir has not been rebuilt
			 */
			rc = get_inorecptr(aggregate_inode,  alloc_ifnull,
					   this_inorec->parent_inonum,
					   &parent_inorec);
			if ((parent_inorec->in_use)
			    && (!parent_inorec->ignore_alloc_blks)
			    && (!parent_inorec->selected_to_rls)) {
				/*
				 * parent inode in use and not
				 * selected to release
				 */
				this_inorec->unxpctd_prnts = 1;
				agg_recptr->corrections_needed = 1;
				unalloc_ino_w_prnts_seen = 1;
				done = -1;
				if (agg_recptr->processing_readonly) {
					/* won't be able to fix this */
					fsck_send_msg(fsck_ROUALINOREF,
						      fsck_ref_msg(msg_info_ptr->msg_inopfx),
						      ino_idx);
				}
			}
		}
		this_ext = this_inorec->ext_rec;
		while ((this_ext != NULL) && (!done)) {
			if ( (this_ext->ext_type == parent_extension) &&
			     ((this_inorec->parent_inonum != ROOT_I) ||
			      (!agg_recptr->rootdir_rebuilt)) ) {
				/*
				 * either this parent isn't the root or
				 * else the root dir hasn't been rebuilt
				 */
				rc = get_inorecptr(aggregate_inode,
						   alloc_ifnull,
						   this_ext->inonum,
						   &parent_inorec);
				if ((parent_inorec->in_use) &&
				    (!parent_inorec->ignore_alloc_blks) &&
				    (!parent_inorec->selected_to_rls)) {
					/*
					 * parent inode in use and
					 * not selected to release
					 */
					this_inorec->unxpctd_prnts = 1;
					agg_recptr->corrections_needed = 1;
					unalloc_ino_w_prnts_seen = 1;
					done = -1;
					if (agg_recptr->processing_readonly) {
						agg_recptr->ag_dirty = 1;
						fsck_send_msg(fsck_ROUALINOREF,
							      fsck_ref_msg(msg_info_ptr->msg_inopfx),
							      ino_idx);
					}
				}
			}
			this_ext = this_ext->next;
		}
		goto get_next;

      inode_in_use:
		/* inode is in use */
		inode_already_read = 0;
		if ((agg_recptr->unresolved_1stref_count != 0) &&
		    (!this_inorec->ignore_alloc_blks)) {
			/*
			 * there are unresolved 1st references
			 * to multiply allocated blocks, and
			 * blocks for this inode are reflected
			 * in the current workspace block map
			 */
			rc = inode_get(aggregate_inode, which_it,
				       ino_idx, &inoptr);
			if (rc == FSCK_OK) {
				/* got the inode */
				inode_already_read = 1;
				rc = first_ref_check_inode(inoptr, ino_idx,
						this_inorec, msg_info_ptr);
			}
		}
		if (rc != FSCK_OK)
			goto get_next;

		if ((this_inorec->parent_inonum == 0) &&
		    (!this_inorec->unxpctd_prnts) &&
		    (ino_idx >= FILESET_OBJECT_I)) {
			/*
			 * no parent recorded and not a dir with
			 * unexpected parents and not a metadata inode
			 */
			if (agg_recptr->processing_readonly) {
				/*
				 * won't be reconnecting this
				 */
				fsck_send_msg(fsck_ROUNCONNIO,
					      fsck_ref_msg(msg_info_ptr->msg_inopfx),
					      ino_idx);
			}
		} else if (this_inorec->inode_type == directory_inode) {
			/*
			 * a directory
			 */
			if (!inode_already_read) {
				/* need to read the inode */
				rc = inode_get(aggregate_inode, which_it,
					       ino_idx, &inoptr);
			}
			/*
			 * if this is a directory with 'unexpected
			 * parents' (aka illegal hard links) then
			 * the inode number which was stored in
			 * parent_inonum has already been stored in
			 * an extension record.  Save the parent
			 * inode number stored in the on-disk inode
			 * for use in messages.
			 */
			if (rc != FSCK_OK)
				goto get_next;

			if (this_inorec->unxpctd_prnts) {
				/*
				 * unexpected parents seen
				 */
				this_inorec->parent_inonum = inoptr->di_parent;
				dir_w_hlinks_seen = 1;
				if (agg_recptr->processing_readonly)  {
					agg_recptr->ag_dirty = 1;
					fsck_send_msg(fsck_RODIRWHLKS,
						      fsck_ref_msg(msg_info_ptr->msg_inopfx),
						      ino_idx);
				}
				/*
				 * Otherwise, make sure a parent
				 * was seen and it's the one whose
				 * inode number is stored in the
				 * on-disk inode.
				 */
			} else if ((this_inorec->parent_inonum != 0) &&
				   (this_inorec->parent_inonum !=
				    inoptr->di_parent)) {
				/*
				 * the stored parent number is wrong
				 */
				this_inorec->crrct_prnt_inonum = 1;
				dir_w_incrrct_prnt_seen = 1;
				agg_recptr->corrections_needed = 1;
				agg_recptr->corrections_approved = 1;
				if (agg_recptr->processing_readonly) {
					agg_recptr->ag_dirty = 1;
					fsck_send_msg(fsck_ROINCINOREF,
						      fsck_ref_msg(msg_info_ptr->msg_inopfx),
						      ino_idx,
						      fsck_ref_msg(msg_info_ptr->msg_inopfx),
						      this_inorec->parent_inonum);
				}
			}
		}

      get_next:
		if (rc == FSCK_OK) {
			rc = get_inorecptr_next(aggregate_inode, &ino_idx,
					        &this_inorec);
		}
	}

	if (agg_recptr->processing_readwrite) {
		/* we can fix these */
		if (unalloc_ino_w_prnts_seen) {
			fsck_send_msg(fsck_WILLFIXROUALINOREFS);
		}
		if (unconnected_inode_seen) {
			fsck_send_msg(fsck_WILLFIXROUNCONNIOS);
		}
		if (dir_w_hlinks_seen) {
			fsck_send_msg(fsck_WILLFIXRODIRSWHLKS);
		}
		if (dir_w_incrrct_prnt_seen) {
			fsck_send_msg(fsck_WILLFIXROINCINOREFS);
		}
	} else {
		/* don't have write access */
		if (unalloc_ino_w_prnts_seen) {
			fsck_send_msg(fsck_ROUALINOREFS);
		}
		if (unconnected_inode_seen) {
			fsck_send_msg(fsck_ROUNCONNIOS);
		}
		if (dir_w_hlinks_seen) {
			fsck_send_msg(fsck_RODIRSWHLKS);
		}
		if (dir_w_incrrct_prnt_seen) {
			fsck_send_msg(fsck_ROINCINOREFS);
		}
	}

	return (rc);
}

/*****************************************************************************
 * NAME: create_lost_and_found
 *
 * FUNCTION:  During previous processing, fsck observed at least one inode
 *            to be available, and saved the ordinal number of an available
 *            inode in the fsck aggregate record.  Initialize that inode
 *            (and the fsck inode record describing it) for use as
 *            /lost+found/
 *
 * PARAMETERS:  none
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int create_lost_and_found()
{
	int claf_rc = FSCK_OK;
	int intermed_rc = FSCK_OK;
	struct fsck_inode_record *new_inorecptr;

	int aggregate_inode = 0;	/* going for fileset inodes only */
	int alloc_ifnull = -1;
	int which_it = FILESYSTEM_I;	/* in release 1 there is only fileset 0 */
	struct dinode *inoptr;
	int ixpxd_unequal = 0;
	int is_aggregate = 0;	/* aggregate has no dirs       */

	/*
	 * find/allocate the fsck workspace inode record
	 * for this inode.
	 */
	claf_rc = get_inorecptr(aggregate_inode, alloc_ifnull,
				agg_recptr->avail_inonum, &new_inorecptr);

	if (claf_rc != FSCK_OK)
	    goto claf_setup_exit;

	/*
	 * initialize the workspace inode record for
	 * the new lost+found/
	 */
	new_inorecptr->in_use = 1;
	new_inorecptr->inode_type = directory_inode;
	new_inorecptr->link_count = 0;
	new_inorecptr->parent_inonum = 0;
	new_inorecptr->selected_to_rls = 0;
	new_inorecptr->crrct_link_count = 0;
	new_inorecptr->crrct_prnt_inonum = 0;
	new_inorecptr->adj_entries = 0;
	new_inorecptr->clr_ea_fld = 0;
	new_inorecptr->clr_acl_fld = 0;
	new_inorecptr->inlineea_on = 0;
	new_inorecptr->inlineea_off = 0;
	new_inorecptr->inline_data_err = 0;
	new_inorecptr->ignore_alloc_blks = 0;
	new_inorecptr->reconnect = 0;
	new_inorecptr->unxpctd_prnts = 0;
	new_inorecptr->involved_in_dups = 0;
	/*
	 * get the inode to be used for lost+found
	 */
	claf_rc = inode_get(aggregate_inode, which_it,
			    agg_recptr->avail_inonum, &inoptr);

	if (claf_rc != FSCK_OK)
		goto claf_setup_exit;

	/* the new lost+found inode is in the buffer */
	ixpxd_unequal = memcmp((void *) &inoptr->di_ixpxd,
			       (void *) &agg_recptr->ino_ixpxd, sizeof (pxd_t));

	if ((inoptr->di_inostamp == agg_recptr->inode_stamp) &&
	    (!ixpxd_unequal) &&
	    (inoptr->di_number == agg_recptr->avail_inonum) &&
	    (inoptr->di_fileset == agg_recptr->ino_fsnum)) {
		/*
		 * inode has been used before
		 */
		inoptr->di_gen++;
	} else {
		/* this inode hasn't been used before */
		/* clear it */
		memset(inoptr, 0, sizeof (struct dinode));
		/*
		 * initialize the inode
		 */
		inoptr->di_inostamp = agg_recptr->inode_stamp;
		inoptr->di_fileset = agg_recptr->ino_fsnum;
		inoptr->di_number = agg_recptr->avail_inonum;
		inoptr->di_gen = 1;

		memcpy((void *) &(inoptr->di_ixpxd),
		       (void *) &(agg_recptr->ino_ixpxd), sizeof (pxd_t));
	}

	inoptr->di_mode = (IDIRECTORY | IFJOURNAL | IFDIR |
			   IREAD | IWRITE | IEXEC);
	inoptr->di_parent = ROOT_I;
	/* one from root and one from self */
	inoptr->di_nlink = 2;
	inoptr->di_nblocks = 0;
	inoptr->di_size = IDATASIZE;

	DXDlength(&(inoptr->di_acl), (int32_t) 0);
	DXDaddress(&(inoptr->di_acl), (int64_t) 0);
	inoptr->di_acl.flag = 0;
	inoptr->di_acl.size = 0;
	DXDlength(&(inoptr->di_ea), (int32_t) 0);
	DXDaddress(&(inoptr->di_ea), (int64_t) 0);
	inoptr->di_ea.flag = 0;
	inoptr->di_ea.size = 0;
	inoptr->di_next_index = 2;
	inoptr->di_acltype = 0;
	inoptr->di_atime.tv_sec = (uint32_t) time(NULL);
	inoptr->di_ctime.tv_sec = inoptr->di_atime.tv_sec;
	inoptr->di_mtime.tv_sec = inoptr->di_atime.tv_sec;
	inoptr->di_otime.tv_sec = inoptr->di_atime.tv_sec;
	/*
	 * initialize the d-tree
	 */
	init_dir_tree((dtroot_t *) & (inoptr->di_btroot));

	/*
	 * write the inode
	 */
	claf_rc = inode_put(inoptr);
	if (claf_rc != FSCK_OK)
		goto claf_setup_exit;

	new_inorecptr->parent_inonum = ROOT_I;
	/* The inode is correct.  After
	 * this we'll start accumulating adjustments
	 */
	new_inorecptr->link_count = 0;
	/*
	 * add an entry for it to the root directory
	 */
	intermed_rc = inode_get(is_aggregate, which_it, ROOT_I, &inoptr);

	if (intermed_rc == FSCK_OK) {
		intermed_rc = direntry_add(inoptr, agg_recptr->avail_inonum,
					   agg_recptr->UniChar_lsfn_name);
	}

	if (intermed_rc == FSCK_OK) {
		/* added the entry */
		/* increment the link count in the root
		 * inode because we just added a subdirectory.
		 * (Subdirectories point back to parent.)
		 */
		inoptr->di_nlink += 1;
	} else if (intermed_rc < 0) {
		/* it was fatal */
		claf_rc = intermed_rc;
	} else {
		/* not successful, but not fatal */
		claf_rc = FSCK_CANT_EXTEND_ROOTDIR;
		new_inorecptr->in_use = 0;
	}

      claf_setup_exit:
	if (claf_rc != FSCK_OK) {
		/* failed to create */
		agg_recptr->lsfn_ok = 0;
		new_inorecptr->in_use = 0;
		fsck_send_msg(fsck_LSFNCNTCRE);
	}
	return (claf_rc);
}

/***************************************************************************
 * NAME: final_processing
 *
 * FUNCTION:  If processing read/write, replicate the superblock and the
 *            aggregate inode structures (i.e., the Aggregate Inode Map
 *            and the Aggregate Inode Table).
 *
 *            Notify the user about various things.
 *
 * PARAMETERS:  none
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int final_processing()
{
	int pf_rc = FSCK_OK;
	int64_t observed_total_blocks, recorded_total_blocks;
	int64_t reportable_total_blocks;
	int64_t kbytes_for_dirs, kbytes_for_files;
	int64_t kbytes_for_eas, kbytes_for_acls;
	int64_t kbytes_total, kbytes_free, kbytes_reserved;
	int64_t block_surprise;

	/*
	 * report extra or missing aggregate blocks
	 *
	 * Note that since the agg_record is instantiated in the
	 * module (and not a malloc'ed space) it is still available
	 * after the workspace has been released.
	 *
	 */
	reportable_total_blocks = agg_recptr->blocks_used_in_aggregate +
				  agg_recptr->free_blocks_in_aggregate;
	observed_total_blocks = reportable_total_blocks -
				agg_recptr->ondev_jlog_fsblk_length -
				agg_recptr->ondev_fscklog_fsblk_length -
				agg_recptr->ondev_wsp_fsblk_length;
	/* size in aggregate blocks */
	recorded_total_blocks = sb_ptr->s_size * Dev_blksize / sb_ptr->s_bsize;

	if (observed_total_blocks > recorded_total_blocks) {
		block_surprise = observed_total_blocks - recorded_total_blocks;
		fsck_send_msg(fsck_XTRABLKS, (long long) block_surprise);
	} else if (recorded_total_blocks > observed_total_blocks) {
		block_surprise = recorded_total_blocks - observed_total_blocks;
		fsck_send_msg(fsck_MSSNGBLKS, (long long) block_surprise);
	}

	if (agg_recptr->processing_readwrite) {
		/*
		 * Make sure the s_logdev is up to date in the superblock.
		 */
		if ((Log.location & OUTLINELOG) && Log.devnum)
			sb_ptr->s_logdev = Log.devnum;

		/* refresh the redundancy of the
		 * aggregate superblock (and verify
		 * successful write to the one we
		 * haven't been using)
		 */
		pf_rc = replicate_superblock();
	}
	/*
	 * finish up processing
	 */
	fsck_send_msg(fsck_FSSMMRY3);

	/*
	 * log the summary messages originally defined
	 */
	fsck_send_msg(fsck_FSSMMRY4, (long long) agg_recptr->blocks_for_inodes);

	fsck_send_msg(fsck_FSSMMRY5, (long long) agg_recptr->inodes_in_aggregate);

	fsck_send_msg(fsck_FSSMMRY6, (long long) agg_recptr->files_in_aggregate);

	fsck_send_msg(fsck_FSSMMRY9, (long long) agg_recptr->dirs_in_aggregate);

	fsck_send_msg(fsck_FSSMMRY7, (long long) reportable_total_blocks);

	fsck_send_msg(fsck_FSSMMRY8, (long long) agg_recptr->free_blocks_in_aggregate);

	/*
	 * issue (and log) the standard messages
	 */

	/* number of blocks times bytes per block divided by bytes per kilobyte */
	kbytes_total = reportable_total_blocks
	    << agg_recptr->log2_blksize >> log2BYTESPERKBYTE;

	/* blocks for file inodes times bytes per block */
	/* plus bytes for the file inodes themselves */
	/* divided by bytes per kilobyte */
	kbytes_for_dirs =
	    ((agg_recptr->blocks_for_dirs << agg_recptr->log2_blksize)
	     + (agg_recptr->dirs_in_aggregate << log2INODESIZE)
	    ) >> log2BYTESPERKBYTE;

	/* blocks for file inodes times bytes per block */
	/* plus bytes for the file inodes themselves    */
	/* divided by bytes per kilobyte */
	kbytes_for_files =
	    ((agg_recptr->blocks_for_files << agg_recptr->log2_blksize)
	     + (agg_recptr->files_in_aggregate << log2INODESIZE)
	    ) >> log2BYTESPERKBYTE;

	/* number of blocks times bytes per block divided by bytes per kilobyte */
	kbytes_for_eas = agg_recptr->blocks_for_eas
	    << agg_recptr->log2_blksize >> log2BYTESPERKBYTE;

	/* number of blocks times bytes per block divided by bytes per kilobyte */
	kbytes_for_acls = agg_recptr->blocks_for_acls
	    << agg_recptr->log2_blksize >> log2BYTESPERKBYTE;

	/* number of blocks times bytes per block divided by bytes per kilobyte */
	kbytes_free = agg_recptr->free_blocks_in_aggregate
	    << agg_recptr->log2_blksize >> log2BYTESPERKBYTE;

	/* everything else is reserved */
	kbytes_reserved =
	    kbytes_total - -kbytes_for_dirs - kbytes_for_files -
	    kbytes_for_eas - kbytes_for_acls - kbytes_free;

	fsck_send_msg(fsck_STDSUMMARY1, (long long) kbytes_total);

	fsck_send_msg(fsck_STDSUMMARY2, (long long) kbytes_for_dirs,
		      (long long) agg_recptr->dirs_in_aggregate);

	fsck_send_msg(fsck_STDSUMMARY3, (long long) kbytes_for_files,
		      (long long) agg_recptr->files_in_aggregate);

	fsck_send_msg(fsck_STDSUMMARY4, (long long) kbytes_for_eas);

	fsck_send_msg(fsck_STDSUMMARY4A, (long long) kbytes_for_acls);

	fsck_send_msg(fsck_STDSUMMARY5, (long long) kbytes_reserved);

	fsck_send_msg(fsck_STDSUMMARY6, (long long) kbytes_free);

	if (pf_rc != FSCK_OK) {
		agg_recptr->fsck_is_done = 1;
	}

	return (pf_rc);
}

/*****************************************************************************
 * NAME: report_problems_setup_repairs
 *
 * FUNCTION:  For each inode in the fileset, if fsck has determined that
 *            any repairs are needed, get/verify permission to perform
 *            the repair and, if permission has been given, adjust the
 *            other inodes for implied repairs as appropriate.  (E.g.,
 *            if inode a is released, then each directory inode parent
 *            of a needs to have the entry for a removed.)
 *
 * PARAMETERS:  none
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int report_problems_setup_repairs()
{
	int rc = FSCK_OK;
	uint32_t ino_idx;
	int aggregate_inode = 0;	/* going for fileset inodes only */
	struct fsck_inode_ext_record *this_ext;
	struct fsck_inode_ext_record *ext_list;
	struct fsck_inode_record *this_inorec;
	int8_t other_adjustments;
	struct fsck_ino_msg_info ino_msg_info;
	struct fsck_ino_msg_info *msg_info_ptr;

	msg_info_ptr = &ino_msg_info;
	msg_info_ptr->msg_inopfx = fsck_fset_inode;	/* all fileset owned */
	rc = get_inorecptr_first(aggregate_inode, &ino_idx, &this_inorec);

	while ((rc == FSCK_OK) && (this_inorec != NULL)) {
		if (!((this_inorec->selected_to_rls) ||
		    (this_inorec->clr_ea_fld) ||
		    (this_inorec->clr_acl_fld) ||
		    (this_inorec->inlineea_on) ||
		    (this_inorec->inlineea_off) ||
		    (this_inorec->inline_data_err) ||
		    (this_inorec->cant_chkea) ||
		    (this_inorec->adj_entries) ||
		    (this_inorec->rebuild_dirtable) ||
		    ((this_inorec->unxpctd_prnts) && (!this_inorec->in_use))))
			goto rpsr_next;

		/*
		 * a record is allocated and flagged
		 * for some repair (other than directory
		 * with illegal hard links) or warning
		 */
		msg_info_ptr->msg_inonum = ino_idx;
		if (this_inorec->inode_type == directory_inode) {
			msg_info_ptr->msg_inotyp = fsck_directory;
		} else if (this_inorec->inode_type == symlink_inode) {
			msg_info_ptr->msg_inotyp = fsck_symbolic_link;
		} else if (this_inorec->inode_type == char_special_inode) {
			msg_info_ptr->msg_inotyp = fsck_char_special;
		} else if (this_inorec->inode_type == block_special_inode) {
			msg_info_ptr->msg_inotyp = fsck_block_special;
		} else if (this_inorec->inode_type == FIFO_inode) {
			msg_info_ptr->msg_inotyp = fsck_FIFO;
		} else if (this_inorec->inode_type == SOCK_inode) {
			msg_info_ptr->msg_inotyp = fsck_SOCK;
		} else {
			/* a regular file */
			msg_info_ptr->msg_inotyp = fsck_file;
		}

		rc = display_paths(ino_idx, this_inorec, msg_info_ptr);
		if (rc != FSCK_OK)
			goto rpsr_next;

		if (!this_inorec->in_use) {
			/* not in use.  */
			if (this_inorec->unxpctd_prnts) {
				/* but with parents */
				if (agg_recptr->processing_readwrite) {
					/* we can fix this */
					rc = adjust_parents(this_inorec, ino_idx);
					agg_recptr->corrections_approved = 1;
					fsck_send_msg(fsck_WILLRMVBADREF);
				} else {
					/* don't have write access */
					fsck_send_msg(fsck_INOBADREF);
					agg_recptr->ag_dirty = 1;
				}
			} else {
				/* shouldn't have created a record for it! */
				rc = FSCK_INTERNAL_ERROR_2;
			}
			goto rpsr_next;
		}
		/* inode is in use */
		if (this_inorec->selected_to_rls) {
			/* selected to release */
			/*
			 * explain the problem(s)
			 */
			if (this_inorec->inode_type == unrecognized_inode) {
				fsck_send_msg(fsck_BADINOFORMATO);
			}

			if (this_inorec->ignore_alloc_blks) {
				/* corrupt tree */
				if (this_inorec->inode_type == file_inode) {
					fsck_send_msg(fsck_BADINODATAFORMAT);
				} else if (this_inorec->inode_type ==
					   directory_inode)
				{
					fsck_send_msg(fsck_BADINODATAFORMATD);
				} else {
					fsck_send_msg(fsck_BADINODATAFORMATO);
				}
			}
			if (this_inorec->inline_data_err) {
				/* invalid inline data spec */
				if (this_inorec->inode_type == file_inode) {
					fsck_send_msg(fsck_BADINODATAFORMAT);
				} else if (this_inorec->inode_type ==
					   directory_inode)
				{
					fsck_send_msg(fsck_BADINODATAFORMATD);
				} else {
					fsck_send_msg(fsck_BADINODATAFORMATO);
				}
			}
			if (this_inorec->involved_in_dups) {
				if (this_inorec->inode_type == file_inode) {
					fsck_send_msg(fsck_BADINOCLAIMSDUPSF);
				} else if (this_inorec->inode_type ==
					   directory_inode)
				{
					fsck_send_msg(fsck_BADINOCLAIMSDUPSD);
				} else {
					fsck_send_msg(fsck_BADINOCLAIMSDUPSO);
				}
			}
			/*
			 * notify of intentions (if any)
			 */
			if (agg_recptr->processing_readwrite) {
				/* we can fix this */
				rc =adjust_parents(this_inorec, ino_idx);
				agg_recptr->corrections_approved = 1;
				fsck_send_msg(fsck_WILLRELEASEINO,
					      fsck_ref_msg(msg_info_ptr->msg_inotyp),
					      fsck_ref_msg(msg_info_ptr->msg_inopfx),
					      msg_info_ptr->msg_inonum);
			} else {
				/* we don't have write access */
				this_inorec->selected_to_rls = 0;
				/*
				 * If the tree is corrupt, or the type is unrecognized,
				 * keeping it makes the filesystem dirty.
				 * Otherwise it's just the alleged owner of some
				 * multiply allocated block(s).  In the latter case, the
				 * aggregate isn't necessarily dirty just because we don't
				 * release this particular inode.  If all other alleged
				 * owners are released then this one becomes OK.  After
				 * all releases are done we'll check to see if any block(s)
				 * are still multiply allocated, and if so we'll mark the
				 * aggregate dirty.
				 */
				if ((this_inorec->inode_type ==
				     unrecognized_inode) ||
				    (this_inorec->ignore_alloc_blks))
				{
					agg_recptr->ag_dirty =1;
				}
				fsck_send_msg(fsck_CANTREPAIRINO,
					      fsck_ref_msg(msg_info_ptr->msg_inotyp),
					      fsck_ref_msg(msg_info_ptr->msg_inopfx),
					      msg_info_ptr->msg_inonum);
			}
		}
		if ( (rc != FSCK_OK)
		     || this_inorec->ignore_alloc_blks
		     || this_inorec->selected_to_rls )
			goto rpsr_next;

		/* not corrupt and */
		/* either never selected to release or
		 * selected and release declined
		 */
		if ((rc == FSCK_OK) && (this_inorec->clr_ea_fld)) {
			/* clear EA */
			if (agg_recptr->processing_readwrite) {
				/* we can fix this */
				agg_recptr->corrections_approved = 1;
				fsck_send_msg(fsck_WILLCLEAREA);
			} else {
				/* we don't have write access */
				this_inorec->clr_ea_fld = 0;
				agg_recptr->ag_dirty = 1;
				fsck_send_msg(fsck_INOEA);
			}
		}
		if ((rc == FSCK_OK) && (this_inorec->clr_acl_fld)) {
			/* clear ACL */
			if (agg_recptr->processing_readwrite) {
				/* we can fix this */
				agg_recptr->corrections_approved = 1;
				fsck_send_msg(fsck_WILLCLEARACL);
			} else {
				/* we don't have write access */
				this_inorec->clr_acl_fld = 0;
				agg_recptr->ag_dirty = 1;
				fsck_send_msg(fsck_INOACL);
			}
		}
		if ((rc == FSCK_OK) && (this_inorec->inlineea_off)) {
			/* turn off sect 4 avail flag */
			if (agg_recptr->processing_readwrite) {
				/* we can fix this */
				agg_recptr->corrections_approved = 1;
				fsck_send_msg(fsck_WILLFIXINOMINOR);
			} else {
				/* we don't have write access */
				this_inorec->inlineea_off = 0;
				agg_recptr->ag_dirty = 1;
				fsck_send_msg(fsck_INOMINOR);
			}
		}
		if ((rc == FSCK_OK) && (this_inorec->inlineea_on)) {
			/* turn on sect 4 avail flag */
			if (agg_recptr->processing_readwrite) {
				/* we can fix this */
				agg_recptr->corrections_approved = 1;
				fsck_send_msg(fsck_WILLFIXINOMINOR);
			} else {
				/* we don't have write access */
				this_inorec->inlineea_on = 0;
				fsck_send_msg(fsck_INOMINOR);
			}
		}
		if ((rc == FSCK_OK) && (this_inorec->adj_entries)) {
			/* adjust dir entries */
			other_adjustments = 0;
			ext_list = this_inorec->ext_rec;
			this_inorec->ext_rec = NULL;
			while (ext_list != NULL) {
				this_ext = ext_list;
				ext_list = ext_list->next;
				if ((this_ext->ext_type ==
				     add_direntry_extension) ||
				    (this_ext->ext_type ==
				     rmv_direntry_extension)) {
					other_adjustments = -1;
					agg_recptr->corrections_approved = 1;
				}
				if (this_ext->ext_type !=
				    rmv_badentry_extension) {
					this_ext->next = this_inorec->ext_rec;
					this_inorec->ext_rec = this_ext;
				} else {
					/* it represents a bad entry */
					if (agg_recptr->processing_readwrite) {
						/* we can fix this */
						agg_recptr->
						    corrections_approved = 1;
						this_ext->next =
						    this_inorec->ext_rec;
						this_inorec->ext_rec = this_ext;
						other_adjustments = -1;
						fsck_send_msg
						    (fsck_WILLRMVBADENTRY,
						     fsck_ref_msg(msg_info_ptr->msg_inopfx),
						     this_ext->inonum);
					} else {
						/* we don't have write access */
						release_inode_extension
						    (this_ext);
						agg_recptr->ag_dirty = 1;
						fsck_send_msg
						    (fsck_BADDIRENTRY,
						     fsck_ref_msg(msg_info_ptr->msg_inopfx),
						     this_ext->inonum);
					}
				}
			}
			if (!other_adjustments) {
				this_inorec->adj_entries = 0;
			} else {
				this_inorec->adj_entries = 1;
				agg_recptr->corrections_needed = 1;
			}
		}
		if ((rc == FSCK_OK) && (this_inorec->cant_chkea)) {
			/*
			 * wasn't able to check the EA format
			 */
			fsck_send_msg(fsck_CANTCHKEA);
		}
		if ((rc == FSCK_OK) && (this_inorec->rebuild_dirtable)) {
			if (agg_recptr->processing_readwrite) {
				/* we can fix this */
				agg_recptr->corrections_approved = 1;
				fsck_send_msg(fsck_WILLFIX_DI_TABLE);
			} else {
				/* we don't have write access */
				this_inorec->rebuild_dirtable = 0;
				agg_recptr->ag_dirty = 1;
				fsck_send_msg(fsck_DI_TABLE);
			}
		}

      rpsr_next:
		if (rc == FSCK_OK) {
			rc = get_inorecptr_next(aggregate_inode, &ino_idx,
						&this_inorec);
		}
	}
	return (rc);
}

/*****************************************************************************
 * NAME: initial_processing
 *
 * FUNCTION: Parse and verify invocation parameters.  Open the device and
 *           verify that it contains a JFS file system.  Check and repair
 *           the superblock.  Initialize the fsck aggregate record.  Refresh
 *           the boot sector on the volume.  Issue some opening messages.
 *
 *
 * PARAMETERS:  as specified to main()
 *
 * NOTES: sets exit_value if other than OK
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int initial_processing(int argc, char **argv)
{
	int pi_rc = FSCK_OK;
	int iml_rc = FSCK_OK;
	int64_t fsblk_offset_bak;
	int64_t byte_offset_bak;
	time_t Current_Time;
	char message_parm[MAXPARMLEN];

	/*
	 * Initiate fsck service logging
	 */
	iml_rc = fscklog_start();

	/*
	 * Log the beginning of the fsck session
	 */
	Current_Time = time(NULL);
	fsck_DateTime = localtime(&Current_Time);
	sprintf(message_parm, "%d/%d/%d %d:%02d:%02d",
		fsck_DateTime->tm_mon + 1, fsck_DateTime->tm_mday,
		(fsck_DateTime->tm_year + 1900), fsck_DateTime->tm_hour,
		fsck_DateTime->tm_min, fsck_DateTime->tm_sec);
	fsck_send_msg(fsck_SESSSTART, message_parm);

	/*
	 * Process the parameters given by the user
	 */

	/* parse the parms and record them in the aggregate wsp record */
	parse_parms(argc, argv);
	if ((pi_rc = Is_Device_Mounted(Vol_Label)) != FSCK_OK) {
		switch (pi_rc) {
		case MSG_JFS_VOLUME_IS_MOUNTED_RO:
			/* is mounted read only */
			if ((agg_recptr->parm_options[UFS_CHKDSK_DEBUG]) ||
			    (agg_recptr->parm_options[UFS_CHKDSK_VERBOSE]))
				printf("\nFSCK  Device %s is currently mounted "
				       "READ ONLY.\n", Vol_Label);
			break;

		case MSG_JFS_MNT_LIST_ERROR:
			/* setmntent failed */
			if ((agg_recptr->parm_options[UFS_CHKDSK_DEBUG]) ||
			    (agg_recptr->parm_options[UFS_CHKDSK_VERBOSE]))
				printf("\njfs_fsck cannot access file system "
				       "description file to determine mount\n"
				       "status and file system type of device "
				       "%s.  jfs_fsck will continue.\n\n",
				       Vol_Label);
			break;

		case MSG_JFS_VOLUME_IS_MOUNTED:
			if (agg_recptr->parm_options[UFS_CHKDSK_LEVEL0]) {
				/* read only */
				fsck_send_msg(fsck_FSMNTD);
				fsck_send_msg(fsck_MNTFSYS2);
			} else {
				/* is mounted */
				printf("\n%s is mounted.\n\nWARNING!!!\n"
				       "Running fsck on a mounted file system\n"
				       "may cause SEVERE file system damage."
				       "\n\n", Vol_Label);
				ask_continue();
			}
			break;

		case MSG_JFS_NOT_JFS:
			/* is not JFS */
			printf("\n%s is mounted and the file system is not type"
			       " JFS.\n\nWARNING!!!\nRunning jfs_fsck on a "
			       "mounted file system\nor on a file system other "
			       "than JFS\nmay cause SEVERE file system damage."
			       "\n\n", Vol_Label);
			ask_continue();
			break;

		default:
			ask_continue();
			break;
		}
	}


	/* the parms are good */
	pi_rc = verify_parms();

	/*
	 * Open the device and verify that it contains a valid JFS aggregate
	 * If it does, check/repair the superblock.
	 */
	if (pi_rc != FSCK_OK) {
		/* verify_parms returned bad rc */
		exit_value = FSCK_USAGE_ERROR;
		goto ip_exit;
	}

	/* parms are good */
	fsck_send_msg(fsck_DRIVEID, Vol_Label);
	pi_rc = open_volume(Vol_Label);
	if (pi_rc != FSCK_OK) {
		/* device open failed */
		fsck_send_msg(fsck_CNTRESUPB);
		exit_value = FSCK_OP_ERROR;
		goto ip_exit;
	}
	/* device is open */
	agg_recptr->device_is_open = 1;
	pi_rc = validate_repair_superblock();
	if (pi_rc != FSCK_OK) {
		/* superblock invalid */
	       	exit_value = FSCK_OP_ERROR;
		goto ip_exit;
	}

	fsck_send_msg(fsck_DRIVETYPE);
	/*
	 * add some stuff to the agg_record which is based on
	 * superblock fields
	 */
	agg_recptr->log2_blksize = log2shift(sb_ptr->s_bsize);
	agg_recptr->blksperpg = BYTESPERPAGE / sb_ptr->s_bsize;
	agg_recptr->log2_blksperpg = log2shift(agg_recptr->blksperpg);
	agg_recptr->log2_blksperag = log2shift(sb_ptr->s_agsize);
	/*highest is the last one before the in-aggregate journal log */
	agg_recptr->highest_valid_fset_datablk =
	    addressPXD(&(sb_ptr->s_fsckpxd)) - 1;
	/* lowest is the first after the secondary aggreg inode table */
	agg_recptr->lowest_valid_fset_datablk =  addressPXD(&(sb_ptr->s_ait2)) +
	    (INODE_EXTENT_SIZE / sb_ptr->s_bsize) + 1;
	/*
	 *  agg size in logical blks is
	 *    (size in phys blks times phys blk size divided by log blk size)
	 *  number of AGs in the aggregate:
	 *     (agg size in log blks plus AG size in log blks minus 1)
	 *     divided by (AG size in log blks)
	 */
	agg_recptr->num_ag = ((sb_ptr->s_size * sb_ptr->s_pbsize /
	      sb_ptr->s_bsize) + sb_ptr->s_agsize - 1) / sb_ptr->s_agsize;
	agg_recptr->sb_agg_fsblk_length = sb_ptr->s_size *
	    sb_ptr->s_pbsize / sb_ptr->s_bsize;
	/* length of the on-device journal log */
	agg_recptr->ondev_jlog_fsblk_length = lengthPXD(&(sb_ptr->s_logpxd));
	/* aggregate block offset of the on-device journal log */
	agg_recptr->ondev_jlog_fsblk_offset = addressPXD(&(sb_ptr->s_logpxd));
	ondev_jlog_byte_offset =
	    agg_recptr->ondev_jlog_fsblk_offset * sb_ptr->s_bsize;
	/* length of the on-device fsck service log */
	agg_recptr->ondev_fscklog_byte_length =
	    sb_ptr->s_fsckloglen * sb_ptr->s_bsize;
	/* length of the on-device fsck service log */
	agg_recptr->ondev_fscklog_fsblk_length = sb_ptr->s_fsckloglen;
	/* length of the on-device fsck workspace */
	agg_recptr->ondev_wsp_fsblk_length = lengthPXD(&(sb_ptr->s_fsckpxd)) -
	    agg_recptr->ondev_fscklog_fsblk_length;
	/* length of the on-device fsck workspace */
	agg_recptr->ondev_wsp_byte_length =
	    agg_recptr->ondev_wsp_fsblk_length * sb_ptr->s_bsize;
	/* aggregate block offset of the on-device fsck workspace */
	agg_recptr->ondev_wsp_fsblk_offset = addressPXD(&(sb_ptr->s_fsckpxd));
	/* byte offset of the on-device fsck workspace */
	agg_recptr->ondev_wsp_byte_offset =
	    agg_recptr->ondev_wsp_fsblk_offset * sb_ptr->s_bsize;
	/* aggregate block offset of the on-device fsck workspace */
	agg_recptr->ondev_fscklog_fsblk_offset =
	    agg_recptr->ondev_wsp_fsblk_offset +
	    agg_recptr->ondev_wsp_fsblk_length;
	/* byte offset of the on-device fsck workspace */
	agg_recptr->ondev_fscklog_byte_offset =
	    agg_recptr->ondev_wsp_byte_offset +
	    agg_recptr->ondev_wsp_byte_length;
	/*
	 * The offsets now assume the prior log (the one to overwrite) is
	 * 1st in the aggregate fsck service log space.  Adjust if needed.
	 */
	if (sb_ptr->s_fscklog == 0) {
		/* first time ever for this aggregate */
		fsblk_offset_bak = agg_recptr->ondev_fscklog_fsblk_offset;
		byte_offset_bak = agg_recptr->ondev_fscklog_byte_offset;
		/*
		 * initialize the 2nd service log space
		 *
		 * (we'll actually write the log to the 1st space, so
		 * we'll initialize it below)
		 */
		agg_recptr->ondev_fscklog_fsblk_offset +=
		    agg_recptr->ondev_fscklog_fsblk_length / 2;
		agg_recptr->ondev_fscklog_byte_offset +=
		    agg_recptr->ondev_fscklog_byte_length / 2;
		agg_recptr->fscklog_agg_offset =
		    agg_recptr->ondev_fscklog_byte_offset;
		fscklog_init();
		sb_ptr->s_fscklog = 1;
		agg_recptr->ondev_fscklog_fsblk_offset = fsblk_offset_bak;
		agg_recptr->ondev_fscklog_byte_offset = byte_offset_bak;
	} else if (sb_ptr->s_fscklog == 1) {
		/* the 1st is most recent */
		sb_ptr->s_fscklog = 2;
		agg_recptr->ondev_fscklog_fsblk_offset +=
		    agg_recptr->ondev_fscklog_fsblk_length / 2;
		agg_recptr->ondev_fscklog_byte_offset +=
		    agg_recptr->ondev_fscklog_byte_length / 2;
	} else {
		/* the 1st is the one to overwrite */
		sb_ptr->s_fscklog = 1;
	}
	agg_recptr->fscklog_agg_offset = agg_recptr->ondev_fscklog_byte_offset;
	/*
	 * Initialize the service log
	 */
	fscklog_init();
	/* from the user's perspective, these are in use (by jfs) */
	agg_recptr->blocks_used_in_aggregate =
	    agg_recptr->ondev_wsp_fsblk_length +
	    agg_recptr->ondev_fscklog_fsblk_length +
	    agg_recptr->ondev_jlog_fsblk_length;
	agg_recptr->superblk_ok = 1;
	if ((!agg_recptr->parm_options[UFS_CHKDSK_LEVEL0]) &&
	    (agg_recptr->processing_readonly)) {
		/* user did not specify check only but we can only
		 * do check because we don't have write access
		 */
		fsck_send_msg(fsck_WRSUP);
	}

      ip_exit:
	return (pi_rc);
}

/*****************************************************************************
 * NAME: parse_parms
 *
 * FUNCTION:  Parse the invocation parameters.  If any unrecognized
 *            parameters are detected, or if any required parameter is
 *            omitted, issue a message and exit.
 *
 * PARAMETERS:  as specified to main()
 *
 * RETURNS:  If there is an error in parse_parms, it calls fsck_usage()
 *           to remind the user of command format and proper options.
 *           fsck_usage then exits with exit code FSCK_USAGE_ERROR.
 */
void parse_parms(int argc, char **argv)
{

	int c;
	char *device_name = NULL;
	FILE *file_p = NULL;
	char *short_opts = "adfj:noprvVy";
	struct option long_opts[] = {
		{ "omit_journal_replay", no_argument, NULL, 'o'},
		{ "replay_journal_only", no_argument, NULL, 'J'},
		{ NULL, 0, NULL, 0} };

	while ((c = getopt_long(argc, argv, short_opts, long_opts, NULL))
		!= EOF) {
		switch (c) {
		case 'r':
		/*************************
		 * interactive autocheck *
		 *************************/
			/*
			 * jfs_fsck does not support interactive checking,
			 * so -r is undocumented.  However, for backwards
			 * compatibility, -r is supported here and functions
			 * similarly as -p.
			 */
		case 'a':
		case 'p':
		/*******************
		 * preen autocheck *
		 *******************/
			agg_recptr->parm_options[UFS_CHKDSK_LEVEL2] = -1;
			agg_recptr->parm_options[UFS_CHKDSK_IFDIRTY] = -1;
			break;

#if 0
		case 'b':
		/*******************************************
		 * Clear LVM Bad Block List utility option *
		 *******************************************/
			agg_recptr->parm_options[UFS_CHKDSK_CLRBDBLKLST] = -1;
			break;

		case 'c':
		/******************
		 * IfDirty option *
		 ******************/
			agg_recptr->parm_options[UFS_CHKDSK_IFDIRTY] = -1;
			break;
#endif

		case 'f':
		/********************************
		 * Force check after log replay *
		 ********************************/
			agg_recptr->parm_options[UFS_CHKDSK_LEVEL3] = -1;
			break;

		case 'j':
		/**************************
		 * Specify journal device *
		 **************************/
			strncpy(log_device, optarg, sizeof (log_device) - 1);
			break;

		case 'J':
		/***********************
		 * Replay journal only *
		 ***********************/
			agg_recptr->parm_options_logredo_only = 1;
			break;

		case 'n':
		/***********************************
		 * Level0 (no write access) option *
		 ***********************************/
			agg_recptr->parm_options[UFS_CHKDSK_LEVEL0] = -1;
			break;

		case 'o':
		/************************************
		 * Omit logredo() processing option *
		 ************************************/
			agg_recptr->parm_options[UFS_CHKDSK_SKIPLOGREDO] = -1;
			agg_recptr->parm_options_nologredo = 1;
			break;

		case 'd':
		/****************
		 * Debug option *
		 ****************/
			/* undocumented at this point, it is similar to -v */
			dbg_output = 1;
		case 'v':
		/******************
		 * Verbose option *
		 ******************/
			agg_recptr->parm_options[UFS_CHKDSK_VERBOSE] = -1;
			break;

		case 'V':
		/**********************
		 * print version only *
		 **********************/
			exit(FSCK_OK);
			break;

		case 'y':
		/******************************
		 * 'yes to everything' option *
		 ******************************/
			/*
			 * jfs_fsck does not support interactive checking,
			 * so the -y option isn't necessary here.  However,
			 * in striving to have options similar to those
			 * of e2fsck, we will let -y be the same as the
			 * default -p (unless it is overridden), since
			 * the functionality is similar for both -y and -p.
			 */
			break;

		default:
			fsck_usage();
		}
	}

	if (agg_recptr->parm_options_logredo_only &&
	    (agg_recptr->parm_options_nologredo ||
	     agg_recptr->parm_options[UFS_CHKDSK_LEVEL3] ||
	     agg_recptr->parm_options[UFS_CHKDSK_LEVEL0]) ) {
		printf("\nError: --replay_journal_only cannot be used "
		       "with -f, -n, or --omit_journal_replay.\n");
		fsck_usage();
	}

	if (optind != argc - 1) {
		printf("\nError: Device not specified or command format error\n");
		fsck_usage();
	}

	device_name = argv[optind];

	file_p = fopen(device_name, "r");
	if (file_p) {
		fclose(file_p);
	} else {
		printf("\nError: Cannot open device %s\n", device_name);
		fsck_usage();
	}

	Vol_Label = device_name;

	return;
}

/*****************************************************************************
 * NAME: phase0_processing
 *
 * FUNCTION:  Log Redo processing.
 *
 * PARAMETERS:  none
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int phase0_processing()
{
	int p0_rc = FSCK_OK;
	int64_t agg_blks;
	int32_t use_2ndary_superblock = 0;

	agg_recptr->logredo_rc = FSCK_OK;
	/*
	 * if this flag is set then the primary superblock is
	 * corrupt.  The secondary superblock is good, but we
	 * weren't able to fix the primary version.  logredo can
	 * run, but must use the secondary version of the
	 * aggregate superblock
	 */
	if (agg_recptr->cant_write_primary_sb == 1) {
		use_2ndary_superblock = -1;
	}

	/*
	 * start off phase 0
	 */
	fsck_send_msg(fsck_FSSMMRY1, sb_ptr->s_bsize);
	/* aggregate size in fs blocks, by the user's point of view. */
	agg_blks =
	    agg_recptr->sb_agg_fsblk_length +
	    agg_recptr->ondev_jlog_fsblk_length +
	    agg_recptr->ondev_fscklog_fsblk_length +
	    agg_recptr->ondev_wsp_fsblk_length;
	fsck_send_msg(fsck_FSSMMRY2, (long long) agg_blks);
	/*
	 * logredo processing
	 */
	if ((agg_recptr->processing_readwrite) &&
	    (!agg_recptr->parm_options_nologredo)) {
		/* read/write access AND user didn't say not to need to invoke logredo */
		fsck_send_msg(fsck_PHASE0);

#ifdef _JFS_DEBUG
		printf("JFS fsck calling logredo \n");
#endif
		/*
		 * write the superblock to commit any changes we have made in it
		 */
		if (use_2ndary_superblock) {
			/* put 2ndary */
			ujfs_put_superblk(Dev_IOPort, sb_ptr, 0);
		} else {
			/* put primary  */
			ujfs_put_superblk(Dev_IOPort, sb_ptr, 1);
		}

		agg_recptr->logredo_rc =
		    jfs_logredo(Vol_Label, Dev_IOPort, use_2ndary_superblock);

#ifdef _JFS_DEBUG
		printf("JFS fsck back from logredo, rc = %ld \n",
		       agg_recptr->logredo_rc);
#endif
		/* If superblock is clean and log in use, it's okay */
		if ((agg_recptr->logredo_rc == LOG_IN_USE) &&
		    (sb_ptr->s_state == FM_CLEAN))
			agg_recptr->logredo_rc = FSCK_OK;

		if (agg_recptr->logredo_rc != FSCK_OK) {
			/* logredo failed */
			fsck_send_msg(fsck_LOGREDOFAIL, agg_recptr->logredo_rc);
		} else {
			fsck_send_msg(fsck_LOGREDORC, agg_recptr->logredo_rc);
		}
		/*
		 * logredo may change the superblock, so read it in again
		 */
		if (use_2ndary_superblock) {
			/* get 2ndary */
			ujfs_get_superblk(Dev_IOPort, sb_ptr, 0);
		} else {
			/* get primary  */
			ujfs_get_superblk(Dev_IOPort, sb_ptr, 1);
		}
	}
	if (agg_recptr->parm_options[UFS_CHKDSK_IFDIRTY] &&
	    (!agg_recptr->parm_options_nologredo) &&
	    ((sb_ptr->s_state & FM_DIRTY) != FM_DIRTY)
	     && (agg_recptr->logredo_rc == FSCK_OK)
	    && ((sb_ptr->s_flag & JFS_BAD_SAIT) != JFS_BAD_SAIT)) {
		/*
		 * user specified 'only if dirty'
		 * and didn't specify 'omit logredo()'
		 * and logredo was successful
		 * and the aggregate is clean
		 */
		agg_recptr->fsck_is_done = 1;
		exit_value = FSCK_OK;
	} else if (agg_recptr->parm_options_logredo_only) {
		/*
		 * User only wants to run logredo, no matter what.
		 * Make sure we leave a dirty superblock marked dirty
		 */
		if (sb_ptr->s_state & FM_DIRTY)
			agg_recptr->ag_dirty = 1;
		agg_recptr->fsck_is_done = 1;
		exit_value = FSCK_OK;
	}
	/*
	 * if things look ok so far, make storage allocated by logredo()
	 * available to fsck processing.
	 */
	if (p0_rc == FSCK_OK) {
		p0_rc = release_logredo_allocs();
	}

	if (p0_rc != FSCK_OK) {
		agg_recptr->fsck_is_done = 1;
		exit_value = FSCK_OP_ERROR;
	}

	/*
	 * If we're done, make sure s_logdev is up to date in the superblock.
	 * Otherwise, s_logdev will be updated in final_processing.
	 */
	if (agg_recptr->fsck_is_done && (Log.location & OUTLINELOG)
	    && Log.devnum) {
		sb_ptr->s_logdev = Log.devnum;
		/*
		 * write the superblock to commit the above change
		 */
		if (use_2ndary_superblock) {
			/* put 2ndary */
			ujfs_put_superblk(Dev_IOPort, sb_ptr, 0);
		} else {
			/* put primary  */
			ujfs_put_superblk(Dev_IOPort, sb_ptr, 1);
		}
	}

	return (p0_rc);
}

/*****************************************************************************
 * NAME: phase1_processing
 *
 * FUNCTION:  Initialize the fsck workspace.  Process the aggregate-owned
 *            inodes.  Process fileset special inodes.
 *
 *            If any aggregate block is now multiply-allocated, then it
 *            is allocated to more than 1 special inode.  Exit.
 *
 *            Process all remaining inodes.  Count links from directories
 *            to their child inodes.
 *
 * PARAMETERS:  none
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int phase1_processing()
{
	int p1_rc = FSCK_OK;

	fsck_send_msg(fsck_PHASE1);

	if ((p1_rc = establish_io_buffers()) != FSCK_OK)
		goto p1_exit;

	/* I/O buffers established */
	/* establish workspace related to the aggregate */
	if ((p1_rc = establish_agg_workspace()) != FSCK_OK)
		goto p1_exit;

	/* aggregate workspace established */
	if ((p1_rc = record_fixed_metadata()) != FSCK_OK)
		goto p1_exit;

	/* fixed metadata recorded */
	/*
	 * note that this processing will use the vlarge
	 * buffer and then return it for reuse
	 */
	if ((p1_rc = validate_select_agg_inode_table()) != FSCK_OK)
		goto p1_exit;

	/* we have an ait to work with */
	/* establish workspace related to the fileset */
	if ((p1_rc = establish_fs_workspace()) != FSCK_OK)
		goto p1_exit;

	if ((p1_rc = record_dupchk_inode_extents()) != FSCK_OK)
		goto p1_exit;

	/* fs workspace established */
	/* claim the vlarge buffer for
	 * validating EAs.  We do this now because
	 * it IS possible that the root directory (validated
	 * in the call that follows) has an EA attached.
	 */
	establish_ea_iobuf();

	if ((p1_rc = allocate_dir_index_buffers()) != FSCK_OK)
		goto p1_exit;

	/* verify the metadata
	 * inodes for all filesets in the aggregate
	 */
	if ((p1_rc = validate_fs_metadata()) != FSCK_OK)
		goto p1_exit;

	/* check for blocks allocated
	 * to 2 or more metadata objects
	 */
	if ((p1_rc = fatal_dup_check()) != FSCK_OK)
		goto p1_exit;

	/* validate the fileset inodes */
	p1_rc = validate_fs_inodes();

	/* return the vlarge buffer for reuse */
	agg_recptr->ea_buf_ptr = NULL;
	agg_recptr->ea_buf_length = 0;
	agg_recptr->vlarge_current_use = NOT_CURRENTLY_USED;

      p1_exit:
	if (p1_rc != FSCK_OK) {
		agg_recptr->fsck_is_done = 1;
		exit_value = FSCK_OP_ERROR;
	}
	return (p1_rc);
}

/*****************************************************************************
 * NAME: phase2_processing
 *
 * FUNCTION:  Scan the inodes.  If any inode has more than 1 link from
 *	      any single directory then, in Release I of JFS, the
 *	      directory must be corrupt.
 *
 *	      Count the link from each directory inode to its parent inode.
 *            Verify that the link count stored in each in-use inode matches
 *            the link count observed by fsck.  If not, get (once to cover
 *            all incorrect link counts) permission to correct them.
 *
 * PARAMETERS:  none
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int phase2_processing()
{
	int p2_rc = FSCK_OK;

	fsck_send_msg(fsck_PHASE2);

	p2_rc = check_dir_integrity();
	if (p2_rc == FSCK_OK) {
		p2_rc = check_link_counts();
	}

	if (p2_rc != FSCK_OK) {
		agg_recptr->fsck_is_done = 1;
		exit_value = FSCK_OP_ERROR;
	}
	return (p2_rc);
}

/*****************************************************************************
 * NAME: phase3_processing
 *
 * FUNCTION:  If any mulitply-allocated blocks have been detected, find
 *            the first reference to each.  For each in-use directory inode,
 *            verify that it has exactly 1 parent and that the parent inode
 *            number stored in the inode matches the parent observed by
 *            fsck.
 *
 * PARAMETERS:  none
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int phase3_processing()
{
	int p3_rc = FSCK_OK;

	fsck_send_msg(fsck_PHASE3);
	if (agg_recptr->unresolved_1stref_count > 0) {
		/*
		 * there are unresolved first references to
		 * multiply-allocated blocks
		 */

		/* see if any first
		 * references are by aggregate fixed metadata
		 */
		p3_rc = first_ref_check_fixed_metadata();
	}
	if ((p3_rc == FSCK_OK) && (agg_recptr->unresolved_1stref_count > 0)) {
		/*
		 * there are still unresolved first references
		 * to multiply-allocated blocks
		 */

		/* see if any first references are by aggregate metadata inodes */
		p3_rc = first_ref_check_agg_metadata();
	}
	if ((p3_rc == FSCK_OK) && (agg_recptr->unresolved_1stref_count > 0)) {
		/*
		 * there are still unresolved first references
		 * to multiply-allocated blocks
		 */

		/* see if any first references are by fileset metadata inodes */
		p3_rc = first_ref_check_fs_metadata();
	}
	if ((p3_rc == FSCK_OK) && (agg_recptr->unresolved_1stref_count > 0)) {
		/*
		 * there are still unresolved first references
		 * to multiply-allocated blocks
		 */

		/* see if any first references are by
		 * inode extents (described in the IAGs)
		 */
		p3_rc = first_ref_check_inode_extents();
	}
	if (p3_rc == FSCK_OK) {
		/* nothing fatal yet */
		p3_rc = check_parents_and_first_refs();
	}
	if (p3_rc != FSCK_OK) {
		agg_recptr->fsck_is_done = 1;
		exit_value = FSCK_OP_ERROR;
	}

	return (p3_rc);
}

/*****************************************************************************
 * NAME: phase4_processing
 *
 * FUNCTION:  For each inode record, in fsck's workspace inode map, which
 *            has been flagged for some repair, get permission to perform
 *            the repair and adjust the map for repairs implied by approved
 *            repairs.  (E.g., if an inode is approved for release, then
 *            any directory entry for the inode is approved for removal,
 *            by implication.)
 *
 * PARAMETERS:  none
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int phase4_processing()
{
	int p4_rc = FSCK_OK;

	/*
	 * issue this message whether or not there are any corrections to ask
	 * about so that when the next message comes out the user will know
	 * that notifications are completed.
	 */
	fsck_send_msg(fsck_PHASE4);
	if (agg_recptr->corrections_needed || agg_recptr->warning_pending) {
		/*
		 * Get permission to perform indicated repairs.
		 */
		p4_rc = report_problems_setup_repairs();
	}
	if (p4_rc != FSCK_OK) {
		agg_recptr->fsck_is_done = 1;
		exit_value = FSCK_OP_ERROR;
	}
	return (p4_rc);
}

/*****************************************************************************
 * NAME: phase5_processing
 *
 * FUNCTION: Detect problems related to inode connectedness.  Identify
 *           each unconnected, in-use inode and flag it for reconnection.
 *           Identify each directory inode with multiple parents and get
 *           permission to repair it.
 *
 * PARAMETERS:  none
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int phase5_processing()
{
	int p5_rc = FSCK_OK;

	fsck_send_msg(fsck_PHASE5);
	if (agg_recptr->processing_readwrite) {
		/* have read/write access */
		p5_rc = check_connectedness();

		if (p5_rc != FSCK_OK) {
			agg_recptr->fsck_is_done = 1;
			exit_value = FSCK_OP_ERROR;
		}
	}
	return (p5_rc);
}

/*****************************************************************************
 * NAME: phase6_processing
 *
 * FUNCTION: Perform all approved inode corrections.
 *
 * PARAMETERS:  none
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int phase6_processing()
{
	int p6_rc = FSCK_OK;
	int intermed_rc;

	fsck_send_msg(fsck_PHASE6);
	if (!agg_recptr->processing_readwrite ||
	    !agg_recptr->corrections_approved)
		goto p6_exit;

	/*
	 * we're about to write repairs.  if something stops
	 * after we start and before we finish, we'll leave the
	 * filesystem in an inconsistent state.
	 * Mark the superblock (and write it to the disk) to
	 * show the filesystem is unmountable.  If we complete
	 * successfully and all errors are corrected, we'll
	 * be marking it clean later on.
	 */
	sb_ptr->s_state |= FM_DIRTY;
	/* write to the primary superblock on the device. */
	ujfs_put_superblk(Dev_IOPort, sb_ptr, 1);
	/*
	 * log the fact the the superblock
	 * has just been marked dirty
	 */
	fsck_send_msg(fsck_REPAIRSINPROGRESS);
	if (agg_recptr->inode_reconn_extens != NULL) {
		/*
		 * there are inodes to reconnect
		 */
		intermed_rc = resolve_lost_and_found();
		if (intermed_rc < 0) {
			/* something fatal */
			p6_rc = intermed_rc;
		}
	}
	if (p6_rc != FSCK_OK)
		goto p6_setup_exit;

	/* nothing fatal */
	/* Allocate space for
	 * use during directory entry processing.
	 * (used both to add and to delete entries,
	 * so not restricted to reconnect processing)
	 */
	p6_rc = directory_buffers_alloc();
	if (p6_rc != FSCK_OK)
		goto p6_setup_exit;

	/* reconnect buffers initialized */
	p6_rc = repair_fs_inodes();
	if (p6_rc == FSCK_OK) {
		if (agg_recptr->dup_alloc_lst != NULL) {
			/*
			 * there are unresolved duplicate block
			 * allocations
			 */
			agg_recptr->ag_dirty = 1;
		}
		if (agg_recptr->inode_reconn_extens != NULL) {
			/*
			 * there's something to reconnect
			 */
			/*
			 * Does /lost+found/ need anything before we start?
			 */
			if ((agg_recptr->lsfn_ok) &&
			    (agg_recptr->avail_inonum ==
			     agg_recptr->lsfn_inonum)) {
				/*
				 * /lost+found/ needs to be created
				 */
				intermed_rc = create_lost_and_found();
				if (intermed_rc > 0) {
					/* can't reconnect anything */
					agg_recptr->lsfn_ok = 0;
				} else if(intermed_rc < 0) {
					p6_rc = intermed_rc;
				}
			}
			if ((p6_rc == FSCK_OK) && (agg_recptr->lsfn_ok)) {
				p6_rc = reconnect_fs_inodes();
			}
		}
	}
	intermed_rc = directory_buffers_release();

      p6_setup_exit:
	if (p6_rc != FSCK_OK) {
		agg_recptr->fsck_is_done = 1;
		exit_value = FSCK_ERRORS_UNCORRECTED;
	} else {
		exit_value = FSCK_CORRECTED;
	}

      p6_exit:
	return (p6_rc);
}

/*****************************************************************************
 * NAME: phase7_processing
 *
 * FUNCTION: Rebuild (or verify, processing read-only) the Aggregate Inode
 *           Allocation Map, the Aggregate Inode Allocation Table, the
 *           Fileset Inode Allocation Map, and the Fileset Inode Allocation
 *           Table.
 *
 * PARAMETERS:  none
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int phase7_processing()
{
	int p7_rc = FSCK_OK;

	if (agg_recptr->processing_readwrite) {
		/* have read/write access */
		fsck_send_msg(fsck_PHASE7R);
		p7_rc = rebuild_agg_iamap();
		if (p7_rc >= FSCK_OK) {
			p7_rc = rebuild_fs_iamaps();
			if (p7_rc >= FSCK_OK) {
				/*
				 * note that this processing will use the vlarge
				 * buffer and then return it for reuse
				 */

				/*
				 * refresh the redundancy of the
				 * aggregate inode table and map
				 */
				p7_rc = AIS_replication();
			}
		}
	} else {
		/* else processing read only */
		fsck_send_msg(fsck_PHASE7V);
		p7_rc = verify_agg_iamap();
		if (p7_rc >= FSCK_OK) {
			p7_rc = verify_fs_iamaps();
		}
	}
	if (p7_rc != FSCK_OK) {
		agg_recptr->fsck_is_done = 1;
		exit_value = FSCK_ERRORS_UNCORRECTED;
	}
	return (p7_rc);
}

/*****************************************************************************
 * NAME: phase8_processing
 *
 * FUNCTION:  Rebuild (or verify, processing read-only) the Aggregate Block
 *            Allocation Map.
 *
 * PARAMETERS:  none
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int phase8_processing()
{
	int p8_rc = FSCK_OK;

	if (agg_recptr->processing_readwrite) {
		/* have read/write access */
		fsck_send_msg(fsck_PHASE8R);
		p8_rc = rebuild_blkall_map();
	} else {
		/* else processing read only */
		fsck_send_msg(fsck_PHASE8V);
		p8_rc = verify_blkall_map();
	}
	if (p8_rc != FSCK_OK) {
		agg_recptr->fsck_is_done = 1;
		exit_value = FSCK_ERRORS_UNCORRECTED;
	}
	return (p8_rc);
}

/*****************************************************************************
 * NAME: phase9_processing
 *
 * FUNCTION:  If logredo failed during its processing, reformat the journal
 *            log.
 *
 * PARAMETERS:  none
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int phase9_processing()
{
	int p9_rc = FSCK_OK;
	int logformat_called = 0;

	agg_recptr->logformat_rc = FSCK_OK;

	if (!agg_recptr->processing_readwrite ||
	    ((agg_recptr->logredo_rc >= 0) &&
	     !agg_recptr->parm_options_nologredo))
		goto p9_set_exit;

	/* log needs reformat */
	fsck_send_msg(fsck_PHASE9);
	if (sb_ptr->s_flag & JFS_INLINELOG) {
		/* log is inline */
		logformat_called = -1;
#ifdef _JFS_DEBUG
		printf("JFS fsck calling logformat \n");
#endif
		agg_recptr->logformat_rc =
		    jfs_logform(Dev_IOPort, sb_ptr->s_bsize, sb_ptr->s_l2bsize,
				sb_ptr->s_flag, addressPXD(&(sb_ptr->s_logpxd)),
				lengthPXD(&(sb_ptr->s_logpxd)), NULL, NULL);
	} else if (Log.fp && !uuid_is_null(sb_ptr->s_loguuid)) {
		/* External log */
		logformat_called = -1;
#ifdef _JFS_DEBUG
		printf("JFS fsck calling logformat \n");
#endif
		agg_recptr->logformat_rc =
		    jfs_logform(Log.fp, Log.bsize, Log.l2bsize,
				sb_ptr->s_flag, 0, 0, sb_ptr->s_loguuid, NULL);
	} else
		/* Extern log was never opened or null s_loguuid */
		printf("External log could not be found\n");

	if (logformat_called) {
#ifdef _JFS_DEBUG
		printf("JFS fsck back from logformat, rc = %ld \n",
		       agg_recptr->logformat_rc);
#endif
		if (agg_recptr->logformat_rc != FSCK_OK) {
			/* logredo failed */
			fsck_send_msg(fsck_LOGFORMATFAIL, agg_recptr->logformat_rc);
		} else {
			fsck_send_msg(fsck_LOGFORMATRC, agg_recptr->logformat_rc);
		}
	}

      p9_set_exit:
	if (p9_rc != FSCK_OK) {
		agg_recptr->fsck_is_done = 1;
		exit_value = FSCK_OP_ERROR;
	}
	return (p9_rc);
}

/*****************************************************************************
 * NAME: repair_fs_inodes
 *
 * FUNCTION:  Perform approved repairs on fileset inodes, including the
 *            implied repairs.
 *
 * PARAMETERS:  none
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int repair_fs_inodes()
{
	int rfsi_rc = FSCK_OK;
	uint32_t ino_idx;
	struct fsck_inode_ext_record *this_ext;
	struct fsck_inode_ext_record *ext_list;
	struct fsck_inode_record *this_inorec;
	int aggregate_inode = 0;	/* going for fileset inodes only */
	int which_it = 16;	/* in release 1 there is only fileset 0 */
	struct dinode *ino_ptr;
	struct fsck_ino_msg_info ino_msg_info;
	struct fsck_ino_msg_info *msg_info_ptr;

	msg_info_ptr = &ino_msg_info;
	/* all fileset owned */
	msg_info_ptr->msg_inopfx = fsck_fset_inode;

	rfsi_rc = get_inorecptr_first(aggregate_inode, &ino_idx, &this_inorec);

	while ((rfsi_rc == FSCK_OK) && (this_inorec != NULL)) {
		if (!((this_inorec->in_use)
		    && ((this_inorec->selected_to_rls)
			|| (this_inorec->crrct_prnt_inonum)
			|| (this_inorec->adj_entries)
			|| (this_inorec->clr_ea_fld)
			|| (this_inorec->clr_acl_fld)
			|| (this_inorec->inlineea_on)
			|| (this_inorec->inlineea_off)
			|| (this_inorec->rebuild_dirtable)
			|| (this_inorec->reconnect)
			|| (this_inorec->link_count != 0))))
			goto rfsi_get_next;

		/* flagged for repair */
		rfsi_rc = inode_get(aggregate_inode, which_it,
				    ino_idx, &ino_ptr);
		if (rfsi_rc != FSCK_OK)
			goto rfsi_get_next;

		msg_info_ptr->msg_inonum = ino_idx;
		if (this_inorec->inode_type == directory_inode) {
			msg_info_ptr->msg_inotyp = fsck_directory;
		} else if (this_inorec->inode_type == symlink_inode) {
			msg_info_ptr->msg_inotyp = fsck_symbolic_link;
		} else if (this_inorec->inode_type == char_special_inode) {
			msg_info_ptr->msg_inotyp = fsck_char_special;
		} else if (this_inorec->inode_type == block_special_inode) {
			msg_info_ptr->msg_inotyp = fsck_block_special;
		} else if (this_inorec->inode_type == FIFO_inode) {
			msg_info_ptr->msg_inotyp = fsck_FIFO;
		} else if (this_inorec->inode_type == SOCK_inode) {
			msg_info_ptr->msg_inotyp = fsck_SOCK;
		} else {
			/* a regular, block special, or char special file */
			msg_info_ptr->msg_inotyp = fsck_file;
		}
		if (this_inorec->selected_to_rls) {
			/* release the inode */
			rfsi_rc = release_inode(ino_idx, this_inorec, ino_ptr);
			if (rfsi_rc == FSCK_OK) {
				fsck_send_msg(fsck_INOCLRD,
					      fsck_ref_msg(msg_info_ptr->msg_inopfx),
					      msg_info_ptr->msg_inonum);
			}
			goto rfsi_inode_put;
		}
		/* not releasing the inode */
		if (this_inorec->link_count != 0) {
			/*
			 * correct/adjust link count
			 */
			ino_ptr->di_nlink += this_inorec->link_count;
			fsck_send_msg(fsck_INOLKCTFXD,
				      fsck_ref_msg(msg_info_ptr->msg_inopfx),
				      msg_info_ptr->msg_inonum);
		}
		if (this_inorec->crrct_prnt_inonum) {
			/*
			 * correct parent inode number
			 */
			ino_ptr->di_parent = this_inorec->parent_inonum;
			fsck_send_msg(fsck_INCINOREFCRCT,
				      fsck_ref_msg(msg_info_ptr->msg_inopfx),
				      msg_info_ptr->msg_inonum);
		}
		if (this_inorec->reconnect) {
			/*
			 * reconnect
			 */
			if (this_inorec->inode_type ==
			    directory_inode) {
				/*
				 * only directory inodes point back to their parent
				 */
				if (agg_recptr->lsfn_ok) {
					/* /lost+found/ resolved */
					ino_ptr->di_parent =
					    agg_recptr->lsfn_inonum;
				} else {
					/* couldn't resolve /lost+found/ */
					ino_ptr->di_parent = 0;
				}
			}
			/*
			 * N.B. No message is issued at this time since
			 *      reconnect processing for the inode is
			 *      not completed.  When all other inode
			 *      corrections have been processed, a directory
			 *      entry will be added to /lost+found/ for
			 *      each inode needing reconnection.  At that
			 *      time a message will be issued (assuming
			 *      the verbose messaging switch is
			 *      set appropriately for that message).
			 */
		}
		if ((rfsi_rc == FSCK_OK) && (this_inorec->clr_ea_fld)) {
			/* clear EA */
			rfsi_rc = clear_EA_field(this_inorec, ino_ptr);
			if (rfsi_rc == FSCK_OK) {
				fsck_send_msg(fsck_INOEACLRD,
					      fsck_ref_msg(msg_info_ptr->msg_inopfx),
					      msg_info_ptr->msg_inonum);
			}
		}
		if ((rfsi_rc == FSCK_OK)
		    && (this_inorec->clr_acl_fld)) {
			/* clear ACL */
			rfsi_rc = clear_ACL_field(this_inorec, ino_ptr);
			if (rfsi_rc == FSCK_OK) {
				fsck_send_msg(fsck_INOACLCLRD,
					      fsck_ref_msg(msg_info_ptr->msg_inopfx),
					      msg_info_ptr->msg_inonum);
			}
		}
		if ((rfsi_rc == FSCK_OK)
		    && (this_inorec->inlineea_on)) {
			/* sect 4 available flag on */
			ino_ptr->di_mode |= INLINEEA;
			fsck_send_msg(fsck_INOMINORFXD,
				      fsck_ref_msg(msg_info_ptr->msg_inopfx),
				      msg_info_ptr->msg_inonum);
		}
		if ((rfsi_rc == FSCK_OK)
		    && (this_inorec->inlineea_off)) {
			/* sect 4 available flag off */
			ino_ptr->di_mode &= ~INLINEEA;
			fsck_send_msg(fsck_INOMINORFXD,
				      fsck_ref_msg(msg_info_ptr->msg_inopfx),
				      msg_info_ptr->msg_inonum);
		}
		if ((rfsi_rc == FSCK_OK) && (this_inorec->rebuild_dirtable)) {
			rfsi_rc = rebuild_dir_index(ino_ptr, this_inorec);
			if (rfsi_rc == FSCK_OK)
				fsck_send_msg(fsck_DI_TABLE_FXD,
					fsck_ref_msg(msg_info_ptr->msg_inopfx),
					msg_info_ptr->msg_inonum);
		}
		if (!((rfsi_rc == FSCK_OK)
		    && (this_inorec->adj_entries)))
			goto rfsi_inode_put;

		/* adjust  dir entries */
		ext_list = this_inorec->ext_rec;
		this_inorec->ext_rec = NULL;
		while ((ext_list != NULL) && (rfsi_rc == FSCK_OK)) {
			this_ext = ext_list;
			ext_list = ext_list->next;
			if ((this_ext->ext_type == rmv_badentry_extension) ||
			    (this_ext->ext_type == rmv_direntry_extension)) {
				/*
				 * n.b. that the link count adjustment
				 *      has already been handled.
				 */
				rfsi_rc = direntry_remove(ino_ptr,
							  this_ext->inonum);
				if (rfsi_rc == FSCK_OK) {
					fsck_send_msg(fsck_INOREFRMV,
						      fsck_ref_msg(msg_info_ptr->msg_inopfx),
						      msg_info_ptr->msg_inonum,
						      fsck_ref_msg(msg_info_ptr->msg_inopfx),
						      this_ext->inonum);
					release_inode_extension(this_ext);
				}
			} else {
				this_ext->next = this_inorec->ext_rec;
				this_inorec->ext_rec = this_ext;
			}
		}

      rfsi_inode_put:
		if (rfsi_rc == FSCK_OK) {
			/* write it back to the device */
			rfsi_rc = inode_put(ino_ptr);
		}

      rfsi_get_next:
		if (rfsi_rc == FSCK_OK) {
			rfsi_rc = get_inorecptr_next(aggregate_inode, &ino_idx,
						     &this_inorec);
		}
	}

	return (rfsi_rc);
}

/*****************************************************************************
 * NAME: report_dynstg_error
 *
 * FUNCTION:  Report a failure to allocate dynamic storage.
 *
 * PARAMETERS:  none
 *
 * RETURNS:
 *      success: FSCK_OK
 */
void report_dynstg_error()
{
	fsck_send_msg(fsck_EXHDYNSTG, wsp_dynstg_action, wsp_dynstg_object);
	return;
}

/*****************************************************************************
 * NAME: report_readait_error
 *
 * FUNCTION:  Report failure to read the Aggregate Inode Table
 *
 * PARAMETERS:  none
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
void report_readait_error(int local_rc, int global_rc, int which_it)
{
	/*
	 * message to user
	 */
	fsck_send_msg(fsck_URCVREAD, fsck_ref_msg(fsck_metadata), Vol_Label);
	/*
	 * message to debugger
	 */
	fsck_send_msg(fsck_ERRONAITRD, global_rc, local_rc, which_it);
	return;
}

/*****************************************************************************
 * NAME: resolve_lost_and_found
 *
 * FUNCTION: Determine whether /lost+found/ already exists and, if so,
 *           whether it is a valid directory inode.
 *
 * PARAMETERS:  none
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int resolve_lost_and_found()
{
	int rlaf_rc = FSCK_OK;
	int intermed_rc = FSCK_OK;
	uint32_t found_inonum = 0;
	int found_inoidx;
	int lsfn_length_in_UniChars;
	int LSFN_length_in_UniChars;
	int aggregate_inode = 0;	/* going for fileset inodes only */
	int alloc_ifnull = 0;
	struct fsck_inode_record *inorecptr;
	/*
	 * see if one exists now
	 */
	lsfn_length_in_UniChars = UniStrlen(agg_recptr->UniChar_lsfn_name);
	LSFN_length_in_UniChars = UniStrlen(agg_recptr->UniChar_LSFN_NAME);
	intermed_rc = direntry_get_inonum(ROOT_I,
					  lsfn_length_in_UniChars,
					  agg_recptr->UniChar_lsfn_name,
					  LSFN_length_in_UniChars,
					  agg_recptr->UniChar_LSFN_NAME,
					  &found_inonum);
	if (intermed_rc < 0) {
		rlaf_rc = intermed_rc;
	} else if (found_inonum == 0) {
		/* there isn't one yet */
		fsck_send_msg(fsck_LSFNNOTFOUND);
		if (agg_recptr->avail_inonum == 0) {
			/* and we didn't see any
			 * available inodes laying around
			 */

			/* don't have one  */
			agg_recptr->lsfn_inonum = 0;
			/* we won't reconnect anything */
			agg_recptr->lsfn_ok = 0;
			fsck_send_msg(fsck_LSFNCNTCRE);
		} else {
			/* an available inode was seen */
			agg_recptr->lsfn_inonum = agg_recptr->avail_inonum;
			agg_recptr->lsfn_ok = 1;	/* we'll be able to create l+f */
		}
	} else {
		/* we found an entry for an object named lost+found */
		found_inoidx = (uint32_t) found_inonum;
		agg_recptr->lsfn_inonum = found_inoidx;
		rlaf_rc =
		    get_inorecptr(aggregate_inode, alloc_ifnull, found_inoidx,
				  &inorecptr);
		if ((rlaf_rc == FSCK_OK) && (inorecptr == NULL)) {
			/*
			 * a parent but no record allocated
			 */
			rlaf_rc = FSCK_INTERNAL_ERROR_5;
		} else if (rlaf_rc == FSCK_OK) {
			/* a record is allocated */
			if (!inorecptr->in_use) {
				/*
				 * Inode not in use.
				 * This is another presentation of the
				 * no inode allocated yet case
				 */
				agg_recptr->avail_inonum =
				    agg_recptr->lsfn_inonum;
				agg_recptr->lsfn_ok = 1;
				fsck_send_msg(fsck_LSFNNOTFOUND);
			} else {
				/* inode is in use */
				if (inorecptr->selected_to_rls) {
					/*
					 * we've released it.
					 * This is another presentation of the
					 * no inode allocated yet case
					 */
					agg_recptr->avail_inonum =
					    agg_recptr->lsfn_inonum;
					agg_recptr->lsfn_ok = 1;
					fsck_send_msg(fsck_LSFNNOTFOUND);
				} else if (inorecptr->ignore_alloc_blks) {
					/*
					 * but we must have tried to release it and
					 * been turned down because we discovered
					 * that its data is corrupt.
					 */

					/* don't have one  */
					agg_recptr->lsfn_inonum = 0;
					/* and don't have hope */
					agg_recptr->lsfn_ok = 0;
					/*
					 * no message here because we already told them that
					 * /lost+found/ is screwed up and they wouldn't let
					 * us fix it.
					 */
				} else if (inorecptr->inode_type !=
					   directory_inode) {
					/*
					 * but it must be user data (or corrupt
					 * in some subtle way) because it's not
					 * a directory
					 */

					/* don't have one  */
					agg_recptr->lsfn_inonum = 0;
					/* and don't have hope */
					agg_recptr->lsfn_ok = 0;
					fsck_send_msg(fsck_LSFNNOTDIR);
				} else if ((inorecptr->involved_in_dups) &&
					   (agg_recptr->dup_alloc_lst !=
					    NULL)) {
					/*
					 * This inode lays claim to one or more blocks
					 * found to be multiply-allocated.  There are
					 * now multiply-allocated blocks which we
					 * were not allowed to correct.  It isn't
					 * certain that this inode refers to any
					 * block(s) STILL unresolved, but it might.
					 * Since this filesystem is going to be marked
					 * dirty anyway, don't attempt to reconnect
					 * disconnected inodes.
					 * (N.B. The danger in proceeding is to data
					 * integrity.)
					 */

					/* don't have one  */
					agg_recptr->lsfn_inonum = 0;
					/* and don't have hope */
					agg_recptr->lsfn_ok = 0;
					/*
					 * no message here because the reasoning is too
					 * conservative to explain easily.  They'll be informed
					 * about inodes not reconnected (which isn't the end of
					 * the world anyway).
					 */
				} else {
					/* looks like we're in business here */
					agg_recptr->avail_inonum = 0;
					agg_recptr->lsfn_ok = 1;
				}
			}
		}
	}
	return (rlaf_rc);
}

/*****************************************************************************
 * NAME: validate_fs_inodes
 *
 * FUNCTION:  Verify the fileset inodes and structures rooted in them.
 *
 * PARAMETERS:  none
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int validate_fs_inodes()
{
	int vfi_rc = FSCK_OK;
	uint32_t ino_idx;
	struct dinode *ino_ptr;
	int which_it = FILESYSTEM_I;	/* in release 1 there is only 1 fileset */
	struct fsck_ino_msg_info ino_msg_info;
	struct fsck_ino_msg_info *msg_info_ptr;

	msg_info_ptr = &ino_msg_info;
	/* all fileset owned */
	msg_info_ptr->msg_inopfx = fsck_fset_inode;

	/*
	 * get the first non-metadata inode after
	 * the fileset root directory
	 */
	vfi_rc = inode_get_first_fs(which_it, &ino_idx, &ino_ptr);
	while ((vfi_rc == FSCK_OK) && (ino_ptr != NULL)) {
		/* no fatal errors and haven't seen the last inode */
		if (inode_is_in_use(ino_ptr, (uint32_t) ino_idx)) {
			/* inode is in use */
			vfi_rc =
			    validate_record_fileset_inode((uint32_t) ino_idx,
							  ino_idx, ino_ptr,
							  msg_info_ptr);
		} else {
			/* inode is allocated but is not in use */
			if (!agg_recptr->avail_inode_found) {
				/*
				 * this is the first allocated, available
				 * inode we've seen all day
				 */
				agg_recptr->avail_inonum = (uint32_t) ino_idx;
				agg_recptr->avail_inode_found = 1;
			}
		}
		if (vfi_rc == FSCK_OK) {
			vfi_rc = inode_get_next(&ino_idx, &ino_ptr);
		}
	}
	return (vfi_rc);
}

/*****************************************************************************
 * NAME: verify_parms
 *
 * FUNCTION:  Verify that mutually exclusive invocation parameters were not
 *            specified.  Determine the level of messaging to be used for
 *            this fsck session.
 *
 * PARAMETERS:  none
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int verify_parms()
{
	/*
	 * If -f was chosen, have it override -a, -p, -r by
	 * turning off UFS_CHKDSK_IFDIRTY to force a check
	 * regardless of the outcome after the log is replayed
	 */
	if (agg_recptr->parm_options[UFS_CHKDSK_LEVEL3]) {
		agg_recptr->parm_options[UFS_CHKDSK_LEVEL2] = -1;
		agg_recptr->parm_options[UFS_CHKDSK_LEVEL3] = 0;
		agg_recptr->parm_options[UFS_CHKDSK_IFDIRTY] = 0;
	}

	/*
	 * If the -n flag was specified, turn off -p
	 */
	if (agg_recptr->parm_options[UFS_CHKDSK_LEVEL0]) {
		/*
		 * If -n is specified by itself, don't replay the journal.
		 * If -n is specified with -a, -p, or -f, replay the journal
		 * but don't make any other changes
		 */
		if (agg_recptr->parm_options[UFS_CHKDSK_LEVEL2] == 0)
			agg_recptr->parm_options[UFS_CHKDSK_SKIPLOGREDO] = -1;
		agg_recptr->parm_options[UFS_CHKDSK_LEVEL2] = -1;
	}
	else if (agg_recptr->parm_options[UFS_CHKDSK_LEVEL2]) {
		agg_recptr->parm_options[UFS_CHKDSK_LEVEL1] = 0;
		agg_recptr->parm_options[UFS_CHKDSK_LEVEL2] = 0;
		agg_recptr->parm_options[UFS_CHKDSK_LEVEL3] = -1;
		/*
		 * we'll be doing the Bad Block List function as part
		 * of the -f processing.  Turn off the flag that specifies
		 * it.  This flag is only used when we're in read-only mode.
		 */
		agg_recptr->parm_options[UFS_CHKDSK_CLRBDBLKLST] = 0;
	} else {
		/*
		 * Set default value if none was specified.
		 * set the default to same as -p
		 */
		agg_recptr->parm_options[UFS_CHKDSK_LEVEL2] = -1;
		agg_recptr->parm_options[UFS_CHKDSK_IFDIRTY] = -1;
		fsck_send_msg(fsck_SESSPRMDFLT);
	}
	/*
	 * the parms are valid.  record the messaging level they imply.
	 */
	if ((agg_recptr->parm_options[UFS_CHKDSK_VERBOSE])
	    || (agg_recptr->parm_options[UFS_CHKDSK_DEBUG])) {
		agg_recptr->effective_msg_level = fsck_debug;
		msg_lvl = fsck_debug;
	}
	else {
		agg_recptr->effective_msg_level = fsck_quiet;
		msg_lvl = fsck_quiet;
	}

	return (FSCK_OK);
}

void ask_continue()
{
	int cont = 0;
	int c;

	printf("Do you really want to continue (y/n)? ");
	while (1) {
		fflush(stdout);
		if ((c = getchar()) == EOF)
			break;
		c = toupper(c);
		if (c == 'Y') {
			cont = 1;
			break;
		} else if (c == 'N')
			break;
	}
	if (cont) {
		printf("yes\n\n");
		agg_recptr->aggregate_is_mounted = 1;
	} else {
		printf("no\n\nCheck aborted.\n");
		exit(exit_value);
	}
	return;
}

void fsck_usage()
{
	printf("\nUsage:  %s [-afnpvV] [-j journal_device] [--omit_journal_replay] "
	       "[--replay_journal_only] device\n", program_name);
	printf("\nEmergency help:\n"
	       " -a                 Automatic repair.\n"
	       " -f                 Force check even if file system is marked clean.\n"
	       " -j journal_device  Specify external journal device.\n"
	       " -n                 Check read only, make no changes to the file system.\n"
	       " -p                 Automatic repair.\n"
	       " -v                 Be verbose.\n"
	       " -V                 Print version information only.\n"
	       " --omit_journal_replay    Omit transaction log replay.\n"
	       " --replay_journal_only    Only replay the transaction log.\n");
	exit(FSCK_USAGE_ERROR);
}
