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
 *   COMPONENT_NAME: jfs_fscklog (formerly xchklog)
 *
 *      The jfs_fscklog tool provides 2 basic functions - fsck.jfs
 *      service log extraction and display.  The code here extracts
 *      the log.
 *
 */
#include <config.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "devices.h"
#include "diskmap.h"
#include "fsckwsp.h"
#include "fsck_message.h"
#include "jfs_endian.h"
#include "jfs_fscklog.h"
#include "super.h"
#include "utilsubs.h"
#include "xfsck.h"

#define fsck_READ  1
#define fsck_WRITE 2

extern char file_name[128];

FILE *outfp;

extern struct fscklog_record fscklog_record;
extern struct fscklog_record *local_recptr;

 /* + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + +
  *
  * superblock I/O buffer and pointer
  *
  */
struct superblock aggr_superblock;
struct superblock *sb_ptr;

 /* + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + +
  *
  * input:  fsck service log I/O buffer
  * output: fsck extracted service log I/O buffer
  *
  */
char fscklog_buffer[FSCKLOG_BUFSIZE];
char xchklog_buffer[XCHKLOG_BUFSIZE];

 /* + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + +
  *
  * For message processing
  *
  *    values are assigned by the main jfs_fscklog routine
  */
extern char *Vol_Label;

 /* + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + +
  *
  * Device information.
  *
  *     values are assigned when (if) the device is opened.
  */
FILE *Dev_IOPort;
unsigned Dev_blksize;
int Dev_SectorSize;

/* VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV
 *
 * The following are internal to this file
 *
 */
void extract_service_log(void);

int xchklog_final_processing(void);

int xchklog_fscklog_fill_buffer(void);

int xchklog_initial_processing(struct fscklog_record *);

int open_device_read(const char *);

int open_outfile(void);

int readwrite_device(int64_t, unsigned, unsigned *, void *, int);

void record_msg(char *);

int validate_super(int);

int validate_superblock(void);

/*****************************************************************************
 * NAME: xchklog
 *
 * FUNCTION: Entry point for jfs read aggregate fsck service log
 *
 * PARAMETERS:
 *           local_recptr - fscklog record
 *
 * RETURNS:
 *      success: 0
 *      failure: something else
 */
int xchklog(struct fscklog_record *local_recptr)
{
	int rc = 0;

	/*
	 * some basic initializations
	 */
	sb_ptr = &aggr_superblock;

	rc = xchklog_initial_processing(local_recptr);

	if (rc == 0) {
		/* we have a buffer and the device is open */
		extract_service_log();
	}

	if (rc == 0) {
		rc = xchklog_final_processing();
	} else {
		xchklog_final_processing();
	}

	return (rc);
}

/***************************************************************************
 * NAME: extract_service_log
 *
 * FUNCTION:
 *
 * PARAMETERS:  none
 *
 * RETURNS:
 *      success: 0
 *      failure: something else
 */
void extract_service_log()
{
	char *msg_txt;
	int dsl_rc = 0;
	char *log_entry;
	int log_entry_pos;
	int bytes_left;
	struct fscklog_entry_hdr *hdptr;

	local_recptr->fscklog_last_read = 0;

	while ((!local_recptr->fscklog_last_read) && (dsl_rc == 0)) {

		dsl_rc = xchklog_fscklog_fill_buffer();
		local_recptr->this_buffer_last_message = 0;
		while ((!local_recptr->this_buffer_last_message)
		       && (dsl_rc == 0)) {

			hdptr = (struct fscklog_entry_hdr *)
			    (local_recptr->infile_buf_ptr +
			     local_recptr->infile_buf_data_len);
			if (hdptr->entry_length == 0) {
				/* no more entries in this buffer */
				local_recptr->this_buffer_last_message = -1;
			} else {
				/* this entry_length != 0 */

				/* swap if on big endian machine */
				ujfs_swap_fscklog_entry_hdr(hdptr);

				log_entry = (char *) hdptr;
				log_entry_pos =
				    sizeof (struct fscklog_entry_hdr);

				/*
				 * Each FSCKLOG_BUFSIZE boundary starts a new
				 * log record.  Otherwise, we depend on a valid
				 * entry_length to locate the next message.
				 */
				bytes_left = local_recptr->infile_buf_length -
				    local_recptr->infile_buf_data_len;
				if ((bytes_left < 0)
				    || (hdptr->entry_length < 0)
				    || (hdptr->entry_length > bytes_left)) {
					/* this isn't a valid log record length */
					send_msg(fsck_CHKLOGINVALRCD);
					local_recptr->this_buffer_last_message =
					    -1;
				} else {
					/* the log record length looks ok */
					msg_txt = &log_entry[log_entry_pos];

					/*
					 * record the message in the output file
					 */
					record_msg(msg_txt);

					/*
					 * set up for the next record
					 */
					local_recptr->infile_buf_data_len +=
					    hdptr->entry_length;
					/*if (hdptr->msg_num == fsck_SESSEND) {
						local_recptr->
						    this_buffer_last_message =
						    -1;
						    }*/
					if (local_recptr->infile_buf_data_len >=
					    local_recptr->infile_buf_length) {
						local_recptr->
						    this_buffer_last_message =
						    -1;
					}
				}
			}
		}
	}

	return;
}

/***************************************************************************
 * NAME: xchklog_final_processing
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
int xchklog_final_processing()
{
	int pf_rc = 0;

	/*
	 * Close the device
	 */
	if (local_recptr->device_is_open) {
		pf_rc = fclose(Dev_IOPort);
	}

	/*
	 * Close the output file
	 */
	if (local_recptr->outfile_is_open) {
		/*
		 * flush the buffer if necessary
		 */
		if (local_recptr->outfile_buf_data_len != 0) {
			fwrite((const void *) (local_recptr->outfile_buf_ptr),
			       sizeof (char), local_recptr->outfile_buf_length,
			       outfp);
		}
		fclose(outfp);
	}

	return (pf_rc);
}

/*****************************************************************************
 * NAME: fscklog_fill_buffer
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
int xchklog_fscklog_fill_buffer()
{
	int flfb_rc = 0;
	int io_rc = 0;
	unsigned bytes_read = 0;
	unsigned log_bytes_left;

	io_rc = readwrite_device(local_recptr->infile_agg_offset,
				 local_recptr->infile_buf_length,
				 &bytes_read,
				 (void *) local_recptr->infile_buf_ptr,
				 fsck_READ);

	if ((io_rc != 0)
	    || (bytes_read != (unsigned) local_recptr->infile_buf_length)) {
		/*
		 * write failed or didn't read
		 * correct number of bytes
		 */
		send_msg(fsck_URCVWRT, fsck_ref_msg(fsck_metadata), Vol_Label);

		send_msg(fsck_ERRONLOG, FSCK_BADREAD_FSCKLOG, io_rc, fsck_READ,
			 (long long) local_recptr->infile_agg_offset,
			 (long int) local_recptr->infile_buf_length,
			 (long int) bytes_read);
	}
	/*
	 * We want to reset the buffer no matter what.  If is useful
	 * to keep going because the next section may be readable.
	 */
	local_recptr->infile_agg_offset += local_recptr->infile_buf_length;
	local_recptr->infile_log_offset += local_recptr->infile_buf_length;
	local_recptr->infile_buf_data_len = 0;

	log_bytes_left = (local_recptr->ondev_fscklog_byte_length / 2) -
	    local_recptr->infile_log_offset;
	if (log_bytes_left < local_recptr->infile_buf_length) {
		/* this is the last one */
		local_recptr->fscklog_last_read = -1;
	}
	return (flfb_rc);
}

/*****************************************************************************
 * NAME: xchklog_initial_processing
 *
 * FUNCTION: Parse and verify invocation parameters.
 *           Open the device and verify that it contains a JFS file system.
 *           Read the chkdsk workspace control page.
 *           Calculate interesting aggregate offsets.
 *
 *
 * PARAMETERS:  as specified to main()
 *
 * NOTES:
 *
 * RETURNS:
 *      success: 0
 *      failure: something else
 */
int xchklog_initial_processing(struct fscklog_record *local_recptr)
{
	int pi_rc = 0;

	/*
	 * Initialize the fscklog control block
	 */
	local_recptr->infile_buf_length = FSCKLOG_BUFSIZE;
	local_recptr->infile_buf_ptr = fscklog_buffer;
	local_recptr->outfile_buf_length = XCHKLOG_BUFSIZE;
	local_recptr->outfile_buf_ptr = xchklog_buffer;

	/*
	 * Open the device and verify that it contains a valid JFS aggregate
	 * If it does, check/repair the superblock.
	 */
	pi_rc = open_device_read(Vol_Label);

	if (pi_rc != 0) {
		/*device open failed */
		send_msg(fsck_CNTRESUPB);
	} else {
		/* device is open */
		local_recptr->device_is_open = 1;
		pi_rc = validate_superblock();
		if (pi_rc == 0) {
			/* a valid superblock */

			/*
			 * add some stuff to the local record which is based on
			 * superblock fields
			 */

			/* length of the on-device fsck service log */
			local_recptr->ondev_fscklog_byte_length =
			    sb_ptr->s_fsckloglen * sb_ptr->s_bsize;
			/* length of the on-device fsck service log */
			local_recptr->ondev_fscklog_fsblk_length =
			    sb_ptr->s_fsckloglen;
			/* length of the on-device fsck workspace */
			local_recptr->ondev_wsp_fsblk_length =
			    lengthPXD(&(sb_ptr->s_fsckpxd)) -
			    local_recptr->ondev_fscklog_fsblk_length;
			/* length of the on-device fsck workspace */
			local_recptr->ondev_wsp_byte_length =
			    local_recptr->ondev_wsp_fsblk_length *
			    sb_ptr->s_bsize;
			/* aggregate block offset of the on-device fsck workspace */
			local_recptr->ondev_wsp_fsblk_offset =
			    addressPXD(&(sb_ptr->s_fsckpxd));
			/* byte offset of the on-device fsck workspace */
			local_recptr->ondev_wsp_byte_offset =
			    local_recptr->ondev_wsp_fsblk_offset *
			    sb_ptr->s_bsize;
			/* aggregate block offset of the on-device fsck workspace */
			local_recptr->ondev_fscklog_fsblk_offset =
			    local_recptr->ondev_wsp_fsblk_offset +
			    local_recptr->ondev_wsp_fsblk_length;
			/* byte offset of the on-device fsck workspace */
			local_recptr->ondev_fscklog_byte_offset =
			    local_recptr->ondev_wsp_byte_offset +
			    local_recptr->ondev_wsp_byte_length;
			/*
			 * The offsets now assume the most recent log is 1st in the
			 * aggregate fsck service log space.  Adjust if needed.
			 */
			if (local_recptr->which_log == NEWLOG) {
				/* most recent wanted */
				if (sb_ptr->s_fscklog == 2) {
					/* the 2nd is most recent */
					local_recptr->
					    ondev_fscklog_fsblk_offset +=
					    local_recptr->
					    ondev_fscklog_fsblk_length / 2;
					local_recptr->
					    ondev_fscklog_byte_offset +=
					    local_recptr->
					    ondev_fscklog_byte_length / 2;
				}
			} else {
				/* previous log wanted */
				if (sb_ptr->s_fscklog != 2) {
					/* the 2nd is not most recent */
					local_recptr->
					    ondev_fscklog_fsblk_offset +=
					    local_recptr->
					    ondev_fscklog_fsblk_length / 2;
					local_recptr->
					    ondev_fscklog_byte_offset +=
					    local_recptr->
					    ondev_fscklog_byte_length / 2;
				}
			}
			local_recptr->infile_agg_offset =
			    local_recptr->ondev_fscklog_byte_offset;

			pi_rc = open_outfile();
		}
	}

	if (local_recptr->which_log == NEWLOG) {
		send_msg(fsck_CHKLOGNEW);
	} else {
		send_msg(fsck_CHKLOGOLD);
	}

	return (pi_rc);
}

/*****************************************************************************
 * NAME: open_device_read
 *
 * FUNCTION:  Open the specified device for read access.
 *
 * PARAMETERS:
 *      Device  - input - the device specification
 *
 * NOTES:
 *
 * RETURNS:
 *      success: 0
 *      failure: something else
 */
int open_device_read(const char *Device)
{
	Dev_IOPort = fopen(Device, "r");
	if (Dev_IOPort == NULL) {
		send_msg(fsck_DEVOPENRDRC, ERROR_FILE_NOT_FOUND);
		return ERROR_FILE_NOT_FOUND;
	}

	Dev_blksize = Dev_SectorSize = PBSIZE;
	return 0;
}

/*****************************************************************************
 * NAME: open_outfile
 *
 * FUNCTION:  Open the output file.
 *
 * PARAMETERS:
 *      Device  - input - the device specification
 *
 * RETURNS:
 *      success: 0
 *      failure: something else
 */
int open_outfile()
{
	int openof_rc = 0;

	if (!local_recptr->file_name_specified) {
		/* no output file name given */
		if (local_recptr->which_log == NEWLOG) {
			/* most recent wanted */
			memcpy((void *) file_name, (void *) default_logfile_new,
			       default_logfile_name_len);
		} else {
			/* prior log wanted */
			memcpy((void *) file_name, (void *) default_logfile_old,
			       default_logfile_name_len);
		}
	}

	outfp = fopen(file_name, "w");
	if (outfp == NULL) {
		/* output file open failed */
		openof_rc = XCHKLOG_CANTOPENOUTFILE;
		send_msg(fsck_XCHKLOGOPNFAIL, file_name);
	} else {
		/* output file is open */
		local_recptr->outfile_is_open = -1;

		/*
		 * write the eyecatcher into the output buffer
		 */
		memcpy((void *) (local_recptr->outfile_buf_ptr),
		       (void *) jfs_chklog_eyecatcher, 16);
		local_recptr->outfile_buf_data_len = 16;

		/*
		 * announce the output file name
		 */
		if (local_recptr->which_log == NEWLOG) {
			/* most recent */
			send_msg(fsck_XCHKLOGNEW, file_name);
		} else {
			/* previous */
			send_msg(fsck_XCHKLOGOLD, file_name);
		}
	}
	return (openof_rc);
}

/*****************************************************************************
 * NAME: readwrite_device
 *
 * FUNCTION:  Read data from or write data to the device on which the
 *            aggregate resides.
 *
 * PARAMETERS:
 *      dev_offset           - input - the offset, in bytes, into the aggregate
 *                                     of the data to read or to which to write
 *                                     the data.
 *      requested_data_size  - input - the number of bytes requested
 *      actual_data_size     - input - pointer to a variable in which to return
 *                                     the number of bytes actually read or
 *                                     written
 *      data_buffer          - input - the address of the buffer in which to
 *                                     put the data or from which to write
 *                                     the data
 *      mode                 - input - { fsck_READ | fsck_WRITE }
 *
 * NOTES:  This routine also exists in fsck code.  When there is time,
 *	   examine both copies for differences and put the combined
 *         copy in libfs to eliminate extra code.
 *
 * RETURNS:
 *      success: 0
 *      failure: something else
 */
int readwrite_device(int64_t dev_offset,
		     unsigned requested_data_size,
		     unsigned *actual_data_size, void *data_buffer, int mode)
{
	int rwdb_rc = 0;

	if ((dev_offset % Dev_SectorSize)
	    || (requested_data_size % Dev_SectorSize)) {
		rwdb_rc = FSCK_FAILED_SEEK;
	} else {
		switch (mode) {
		case fsck_READ:
			rwdb_rc =
			    ujfs_rw_diskblocks(Dev_IOPort, dev_offset,
					       requested_data_size, data_buffer,
					       GET);
			break;
		case fsck_WRITE:
			rwdb_rc =
			    ujfs_rw_diskblocks(Dev_IOPort, dev_offset,
					       requested_data_size, data_buffer,
					       PUT);
			break;
		default:
			rwdb_rc = FSCK_INTERNAL_ERROR_3;
			break;
		}
	}

	if (rwdb_rc == 0)
		*actual_data_size = requested_data_size;
	else
		*actual_data_size = 0;

	return (rwdb_rc);
}

/*****************************************************************************
 * NAME: record_msg
 *
 * FUNCTION: Record an fsck service log message in the output file,
 *           formatted for the common fsck service log display tool.
 *
 * PARAMETERS:
 *      ?                 - input -
 *      ?                 - returned -
 *
 * NOTES:  Any message designated fsck_debug is english-only, and
 *         is issued in english, even if it is sent to stdout.
 *
 *         All other messages are local-language (in the nls context)
 *         and, when sent to stdout or stderr, are issued in the
 *         local language.
 *
 *         Regardless of the language used to send a message to
 *         stdout and stderr, messages are ALWAYS LOGGED IN ENGLISH.
 *
 * RETURNS:
 *	nothing
 */
void record_msg(char *msg_txt)
{
	char *msg;
	char log_entry[4096];
	int entry_length = sizeof (struct chklog_entry_hdr);
	struct chklog_entry_hdr *hdptr;
	int buffer_bytes_left;

	hdptr = (struct chklog_entry_hdr *) log_entry;
	msg = &log_entry[entry_length];
	strcpy(msg, msg_txt);
	entry_length += strlen(msg_txt);
	/* add null terminator to string */
	log_entry[entry_length++] = '\0';

	/*
	 * round the length up so the next entry will
	 * start on a doubleword (4 byte) boundary
	 */
	entry_length = ((entry_length + 3) / 4) * 4;

	hdptr->entry_length = entry_length;

	buffer_bytes_left =
	    local_recptr->outfile_buf_length -
	    local_recptr->outfile_buf_data_len;
	if (buffer_bytes_left < entry_length) {
		/* won't fit */
		local_recptr->last_msghdr->entry_length += buffer_bytes_left;
		fwrite((const void *) (local_recptr->outfile_buf_ptr),
		       sizeof (char), local_recptr->outfile_buf_length, outfp);
		/* clear the buffer */
		memset((void *) (local_recptr->outfile_buf_ptr), 0,
		       local_recptr->outfile_buf_length);
		local_recptr->outfile_buf_data_len = 0;
	}
	local_recptr->last_msghdr = (struct fscklog_entry_hdr *)
	    (local_recptr->outfile_buf_ptr +
	     local_recptr->outfile_buf_data_len);
	memcpy((void *) local_recptr->last_msghdr, (void *) hdptr,
	       entry_length);

	local_recptr->outfile_buf_data_len += entry_length;

	return;
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
 *      which_super  - input - specifies the superblock on which is in the
 *                             buffer { fsck_primary | fsck_secondary }
 *
 * NOTES:  This routine also exists in fsck code.  When there is time,
 *	   examine both copies for differences and put the combined
 *         copy in libfs to eliminate extra code.
 *
 * RETURNS:
 *      success: 0
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
	int agl2size;
	unsigned expected_flag = JFS_GROUPCOMMIT;
	unsigned agsize;
	int bad_bsize = 0;

	if (memcmp(sb_ptr->s_magic, JFS_MAGIC, sizeof (sb_ptr->s_magic)) != 0) {
		vs_rc = FSCK_BADSBMGC;
		send_msg(fsck_BADSBMGC, fsck_ref_msg(which_super));
	} else if (sb_ptr->s_version > JFS_VERSION) {
		vs_rc = FSCK_BADSBVRSN;
		send_msg(fsck_BADSBVRSN, fsck_ref_msg(which_super));
	} else {
		/* the magic number and version number are correct so it
		 * probably is a JFS superblock with the format we are expecting
		 */

		/* get physical device size */
		ujfs_get_dev_size(Dev_IOPort, &bytes_on_device);

		dev_blks_on_device = bytes_on_device / Dev_blksize;

		if (sb_ptr->s_pbsize != Dev_blksize) {
			vs_rc = FSCK_BADSBOTHR1;
			send_msg(fsck_BADSBOTHR, "1", fsck_ref_msg(which_super));
		}
		if (sb_ptr->s_l2pbsize != log2shift(Dev_blksize)) {
			vs_rc = FSCK_BADSBOTHR2;
			send_msg(fsck_BADSBOTHR, "2", fsck_ref_msg(which_super));
		}
		if (!inrange(sb_ptr->s_bsize, 512, 4096)) {
			bad_bsize = -1;
			vs_rc = FSCK_BADSBOTHR3;
			send_msg(fsck_BADSBBLSIZ, fsck_ref_msg(which_super));
		} else {
			/* else the filesystem block size is a legal value */
			if (sb_ptr->s_l2bsize != log2shift(sb_ptr->s_bsize)) {
				vs_rc = FSCK_BADSBOTHR4;
				send_msg(fsck_BADSBOTHR, "4",
					 fsck_ref_msg(which_super));
			}
			if (sb_ptr->s_l2bfactor !=
			    log2shift(sb_ptr->s_bsize / Dev_blksize)) {
				vs_rc = FSCK_BADSBOTHR5;
				send_msg(fsck_BADSBOTHR, "5",
					 fsck_ref_msg(which_super));
			}
			if (sb_ptr->s_bsize < Dev_blksize) {
				bad_bsize = -1;
				vs_rc = FSCK_BLSIZLTLVBLSIZ;
				send_msg(fsck_BLSIZLTLVBLSIZ,
					 fsck_ref_msg(which_super));
			}
		}

		if (!bad_bsize) {
			agg_blks_on_device = bytes_on_device / sb_ptr->s_bsize;
		}

		if (sb_ptr->s_size > dev_blks_on_device) {
			vs_rc = FSCK_BADSBFSSIZ;
			send_msg(fsck_BADSBFSSIZ, fsck_ref_msg(which_super));
		}
#ifdef	_JFS_DFS_LFS
		s_size_inbytes = sb_ptr->s_size * Dev_blksize;
		sum_inbytes = (int64_t) (sb_ptr->totalUsable * 1024) +
		    (int64_t) (sb_ptr->minFree * 1024);
		if ((sum_inbytes > s_size_inbytes)
		    || ((s_size_inbytes - sum_inbytes) >= 1024)) {
			/* the sum is greater or the difference is at least 1K */
			vs_rc = FSCK_BADBLKCTTTL;
			send_msg(fsck_BADBLKCTTTL, fsck_ref_msg(which_super));
		}
#endif
		/* must have JFS_OS2 or JFS_LINUX */
		if (!(((sb_ptr->s_flag & JFS_OS2) == JFS_OS2)
		      || ((sb_ptr->s_flag & JFS_LINUX) == JFS_LINUX))) {
			vs_rc = FSCK_BADSBOTHR6;
			send_msg(fsck_BADSBOTHR, "6", fsck_ref_msg(which_super));
		}

		if ((sb_ptr->s_flag & expected_flag) != expected_flag) {
			vs_rc = FSCK_BADSBOTHR6;
			send_msg(fsck_BADSBOTHR, "6", fsck_ref_msg(which_super));
		}
		if (sb_ptr->s_agsize < (1 << L2BPERDMAP)) {
			vs_rc = FSCK_BADSBAGSIZ;
			send_msg(fsck_BADSBAGSIZ, fsck_ref_msg(which_super));
		} else {
			/* else the alloc group size is possibly correct */
			agg_blks_in_aggreg =
			    sb_ptr->s_size * sb_ptr->s_pbsize / sb_ptr->s_bsize;
			agl2size =
			    ujfs_getagl2size(agg_blks_in_aggreg,
					     sb_ptr->s_bsize);
			/* get the allocation group size */
			agsize = (int64_t) 1 << agl2size;
			if (sb_ptr->s_agsize != agsize) {
				vs_rc = FSCK_BADAGFSSIZ;
				send_msg(fsck_BADSBAGSIZ, fsck_ref_msg(which_super));
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
				send_msg(fsck_BADSBFWSL1, fsck_ref_msg(which_super));
			} else {
				fsck_blkmap_size_pages =
				    ((agg_blks_in_aggreg +
				      (BITSPERPAGE - 1)) / BITSPERPAGE) + 1 +
				    50;
				/* size in aggregate blocks */
				fsck_blkmap_size_blks =
				    (fsck_blkmap_size_pages << L2PSIZE) /
				    sb_ptr->s_bsize;
				/*
				 * aggregate block offset of the
				 * fsck workspace in the aggregate.
				 */
				fsck_blkmap_start_blks =
				    agg_blks_in_aggreg - fsck_blkmap_size_blks;
				if (fsck_length_from_pxd !=
				    fsck_blkmap_size_blks) {
					/*
					 * length of fsck in-aggregate
					 * workspace is incorrect
					 */
					vs_rc = FSCK_BADSBFWSL;
					send_msg(fsck_BADSBFWSL,
						 fsck_ref_msg(which_super));
				}
				if (fsck_start_from_pxd !=
				    fsck_blkmap_start_blks) {
					/*
					 * address of fsck in-aggregate
					 * workspace is incorrect
					 */
					vs_rc = FSCK_BADSBFWSA;
					send_msg(fsck_BADSBFWSA,
						 fsck_ref_msg(which_super));
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
					 * address of in-aggregate
					 * journal log is incorrect
					 */
					vs_rc = FSCK_BADSBFJLA;
					send_msg(fsck_BADSBFJLA,
						 fsck_ref_msg(which_super));
				}
				agg_blks_in_aggreg += jlog_length_from_pxd;
				if (agg_blks_in_aggreg > agg_blks_on_device) {
					/* log length is bad */
					vs_rc = FSCK_BADSBFJLL;
					send_msg(fsck_BADSBFJLL,
						 fsck_ref_msg(which_super));
				}
			}
		}
	}
	return (vs_rc);
}

/*****************************************************************************
 * NAME: validate_superblock
 *
 * FUNCTION:  Verify that the primary superblock is valid.
 *            If not, verify that the secondary superblock is valid.
 *
 * PARAMETERS:  none
 *
 * NOTES:  If this routine returns 0 then the superblock
 *         I/O buffer contains a valid superblock.
 *
 *	   This routine also exists in fsck code.  When there is time,
 *	   examine both copies for differences and put the combined
 *         copy in libfs to eliminate extra code.
 *
 * RETURNS:
 *      success: 0
 *      failure: something else
 */
int validate_superblock()
{
	int vsb_rc = 0;
	int primary_sb_bad = 1;
	int secondary_sb_bad = 1;
	int which_sb = 0;

	/* get primary  */
	vsb_rc = ujfs_get_superblk(Dev_IOPort, sb_ptr, 1);

	if (vsb_rc != 0) {
		/* if read primary fails */
		send_msg(fsck_CNTRESUPP);
	} else {
		/* got primary superblock */
		which_sb = fsck_primary;
		primary_sb_bad = validate_super(fsck_primary);
	}

	if (primary_sb_bad) {
		/* can't use the primary superblock */
		send_msg(fsck_SBBADP);

		/* get 2ndary */
		vsb_rc = ujfs_get_superblk(Dev_IOPort, sb_ptr, 0);

		if (vsb_rc != 0) {
			send_msg(fsck_CNTRESUPS);
		} else {
			/* got secondary superblock */
			which_sb = fsck_secondary;
			secondary_sb_bad = validate_super(fsck_secondary);
		}

		if (!secondary_sb_bad) {
			/* secondary is ok */
			vsb_rc = 0;
		} else {
			send_msg(fsck_SBBADS);
		}
	}
	if ((!primary_sb_bad) || (!secondary_sb_bad)) {
		/* the buffer holds a valid superblock */

		/* aggregate block size */
		local_recptr->ag_blk_size = sb_ptr->s_bsize;

		send_msg(fsck_XCHKLOGSBOK, fsck_ref_msg(which_sb));
	}

	return (vsb_rc);
}
