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
/*
 *   COMPONENT_NAME: jfs_fscklog (formerly xchkdmp)
 *
 *      The jfs_fscklog tool provides 2 basic functions - fsck.jfs
 *      service log extraction and display.  The code here displays
 *      the extracted log.
 *
 */
/*
 *       Define the following to avoid including
 *       a bunch of unnecessary header files
 */
#define _JFS_XCHKDMP

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "fscklog.h"
#include "fsck_message.h"
#include "jfs_fscklog.h"

extern char *MsgText[];

extern struct fscklog_record fscklog_record;
extern struct fscklog_record *local_recptr;

extern char file_name[128];
FILE *infp;

/* + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + +
 *
 * input:  fsck service log I/O buffer
 * output: fsck extracted service log I/O buffer
 *
 */
char xchklog_buffer[XCHKLOG_BUFSIZE];

/* + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + +
 *
 * For message processing
 *
 *    values are assigned by the main jfs_fscklog routine
 */
extern char *msgprms[10];
extern short msgprmidx[10];

/* VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV
 *
 * The following are internal to this file
 *
 */
void dump_service_log(void);

int xchkdmp_final_processing(void);

int xchkdmp_fscklog_fill_buffer(void);

int open_infile(void);

/*****************************************************************************
 * NAME: xchkdmp
 *
 * FUNCTION: Entry point for displaying aggregate fsck.jfs service log
 *
 * PARAMETERS:
 *           local_recptr - fscklog record
 *
 * RETURNS:
 *      success: 0
 *      failure: something else
 */
int xchkdmp(struct fscklog_record *local_recptr)
{
	int rc = 0;

	/*
	 * Initialize the fscklog control block
	 */
	local_recptr->infile_buf_length = XCHKLOG_BUFSIZE;
	local_recptr->infile_buf_ptr = xchklog_buffer;
	local_recptr->highest_msg_num =
	    fsck_highest_msgid_defined + JFSCHKLOG_FIRSTMSGNUM;

	rc = open_infile();

	if (rc == 0) {
		/* we have a buffer and the device is open */
		dump_service_log();
	}

	if (rc == 0) {
		/* no errors yet */
		rc = xchkdmp_final_processing();
	} else {
		/* something already went wrong */
		xchkdmp_final_processing();
	}

	return (rc);
}

/***************************************************************************
 * NAME: dump_service_log
 *
 * FUNCTION:
 *
 * PARAMETERS:  none
 *
 * NOTES:  o  The first read from the input file is done when the
 *            file is opened.  (This is done so that the eyecatcher
 *            can be read to verify that the input file is actually
 *            an extracted JFS fsck service log.)
 *
 * RETURNS:
 *      success: 0
 *      failure: something else
 */
void dump_service_log()
{
	char *msg_txt;
	int dsl_rc = 0;
	char *log_entry;
	int log_entry_pos;
	int bytes_left;
	struct chklog_entry_hdr *hdptr;

	local_recptr->fscklog_last_read = 0;

	while ((!local_recptr->fscklog_last_read) && (dsl_rc == 0)) {

		local_recptr->this_buffer_last_message = 0;
		while ((!local_recptr->this_buffer_last_message)
		       && (dsl_rc == 0)) {

			hdptr = (struct chklog_entry_hdr *)
			    (local_recptr->infile_buf_ptr +
			     local_recptr->infile_buf_data_len);
			if (hdptr->entry_length == 0) {
				/* no more entries in this buffer */
				local_recptr->this_buffer_last_message = -1;
			} else {
				/* this entry_length != 0 */
				log_entry = (char *) hdptr;
				log_entry_pos =
				    sizeof (struct chklog_entry_hdr);

				/*
				 * Each FSCKLOG_BUFSIZE boundary starts a new
				 * log record.  Otherwise, we depend on a valid
				 * entry_length to locate the next message.
				 */
				bytes_left = local_recptr->infile_buf_length -
				    local_recptr->infile_buf_data_len;
				if ((bytes_left < 0) ||
				    (hdptr->entry_length < 0) ||
				    (hdptr->entry_length > bytes_left)) {
					/* this isn't a valid log record length */
					send_msg(fsck_CHKLOGINVALRCD);
					local_recptr->this_buffer_last_message =
					    -1;
				} else {
					/* the record looks ok */
					msg_txt = &log_entry[log_entry_pos];
					printf(msg_txt);
					/*
					 * set up for the next record
					 */
					local_recptr->infile_buf_data_len +=
					    hdptr->entry_length;
					if (local_recptr->infile_buf_data_len >=
					    local_recptr->infile_buf_length) {
						local_recptr->
						    this_buffer_last_message =
						    -1;
					}
				}
			}
		}
		/*
		 * read some more from the input file
		 */
		if ((!local_recptr->fscklog_last_read)
		    && (dsl_rc == 0)) {
			dsl_rc = xchkdmp_fscklog_fill_buffer();
		}
	}
	return;
}

/***************************************************************************
 * NAME: xchkdmp_final_processing
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
 *      success: 0
 *      failure: something else
 */
int xchkdmp_final_processing()
{
	int pf_rc = 0;

	/*
	 * Close the input file
	 */
	if (local_recptr->infile_is_open) {
		pf_rc = fclose(infp);
	}

	return (pf_rc);
}

/****************************************************************************
 * NAME: xchkdmp_fscklog_fill_buffer
 *
 * FUNCTION:  If the current fsck session has write access to the aggregate,
 *            and if the in-aggregate fsck log is not full, write the
 *            contents of the current fscklog buffer into the in-aggregate
 *            fsck log.
 *
 * PARAMETERS:  none
 *
 * NOTES:  o Unlike most _put_ routines in this module, fscklog_put_buffer
 *           actually writes to the device.  This is done because the fsck
 *           log contains information which provides crucial serviceability
 *           should the fsck session be interrupted.
 *
 *         o Errors here are recorded in the control page of the fsck
 *           in-aggregate workspace but never affect other fsck processing.
 *
 * RETURNS:
 *      success: 0
 *      failure: something else
 */
int xchkdmp_fscklog_fill_buffer()
{
	int flfb_rc = 0;
	unsigned bytes_read = 0;

	bytes_read = fread((void *) local_recptr->infile_buf_ptr,
			   sizeof (char), XCHKLOG_BUFSIZE, infp);

	if (bytes_read != XCHKLOG_BUFSIZE) {
		if (feof(infp)) {
			local_recptr->fscklog_last_read = -1;
		}
	}
	/*
	 * We want to reset the buffer no matter what.
	 * It is useful to keep going because the next section may be
	 * readable.
	 */
	local_recptr->infile_buf_data_len = 0;

	return (flfb_rc);
}

/*****************************************************************************
 * NAME: open_infile
 *
 * FUNCTION:  Open the input file.
 *
 * PARAMETERS:  none
 *
 * RETURNS:
 *      success: 0
 *      failure: something else
 */
int open_infile()
{
	int openif_rc = 0;
	unsigned bytes_read = 0;
	int eyecatcher_incorrect = 0;

	if (!local_recptr->file_name_specified) {
		/* no input file name specified */
		if (local_recptr->which_log == NEWLOG) {
			memcpy((void *) file_name, (void *) default_logfile_new,
			       default_logfile_name_len);
		} else {
			memcpy((void *) file_name, (void *) default_logfile_old,
			       default_logfile_name_len);
		}
	}

	infp = fopen(file_name, "r");
	if (infp == NULL) {
		/* input file open failed */
		openif_rc = XCHKDMP_CANTOPENINFILE;
		send_msg(fsck_XCHKDMPOPNFAIL, file_name);
	} else {
		/* input file is open */
		local_recptr->infile_is_open = -1;
	}

	if (local_recptr->infile_is_open) {
		/*
		 * read in the first buffer of data
		 */
		bytes_read = fread((void *) local_recptr->infile_buf_ptr,
				   sizeof (char), XCHKLOG_BUFSIZE, infp);
		if (bytes_read != XCHKLOG_BUFSIZE) {
			/* there should be at least this much */
			openif_rc = XCHKDMP_MTORBADREAD;
			send_msg(fsck_XCHKDMPMTORBADREAD);
		} else {
			/* read was successful */
			eyecatcher_incorrect =
			    memcmp((void *) local_recptr->infile_buf_ptr,
				   (void *) jfs_chklog_eyecatcher, 16);
			if (eyecatcher_incorrect) {
				/* incorrect 1st 16 chars */
				openif_rc = XCHKDMP_BADFORMAT;
				send_msg(fsck_XCHKDMPBADFORMAT);
			} else {
				local_recptr->infile_buf_data_len = 16;
			}
		}
	}
	return (openif_rc);
}
