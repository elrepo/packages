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
/*
 *  format log - called from mkfs or logredo
 */
#include <config.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "jfs_types.h"
#include "jfs_endian.h"
#include "devices.h"
#include "jfs_logmgr.h"
#include "message.h"

/*
 * NAME:	jfs_logform
 * FUNCTION:	format file system log
 *
 * RETURN:	0  -	successful
 *		-1 -	error occur
 *
 */
int jfs_logform(FILE *fp,		/* this is a file descriptor
				 * for an opened device to write log.
				 */
		int aggr_blk_size,	/* aggregate block size in bytes */
		int s_l2bsize,	/* log2 of aggr block size in bytes */
		uint s_flag,	/* fs superblock s_flag is passed in */
		int64_t log_start,	/* offset of the start of inline log in
					 * number of aggr blocks. For outline
					 * log it is set as zero
					 */
		int log_len,	/* inline log length in number of aggr
				 * blks.  For outline log, it is zero
				 */
		uuid_t uuid,	/* extern log: uuid of log, if null
				 * create new, else verify that
				 * this device's uuid matches
				 */
		char *label)
{				/* volume label for external log, none if NULL */
	int64_t log_len_in_bytes;
	int npages, rc, k;
	char logpages[4 * LOGPSIZE];
	struct logpage *logp;	/* array of 4 log pages */
	static struct logsuper *log_sup;
	struct lrd *lrd_ptr;
	int64_t log_begin;	/* the location of the beginning of the log
				 * inside of the file system. ( in bytes )
				 */
	int64_t log_contwt;
	int16_t inlinelog = (s_flag & JFS_INLINELOG);
	int Working_counter;
	char *Working[5];

	log_sup = calloc(1, LOGPSIZE);
	if (log_sup == NULL) {
		printf("logform: calloc failed!\n");
		return -1;
	}

	/* find the log superblock location
	 */
	log_begin = log_start << s_l2bsize;

#define LOGBUFSIZE	4 * LOGPSIZE
	logp = (struct logpage *) &logpages;
	Working[0] = "   |\r";
	Working[1] = "   /\r";
	Working[2] = "   -\r";
	Working[3] = "   \\\r";

	if (inlinelog) {
		log_len_in_bytes = ((int64_t) log_len) << s_l2bsize;
		npages = log_len_in_bytes / LOGPSIZE;
	} else {
		if (ujfs_get_dev_size(fp, &log_len_in_bytes))
			return -1;

#define MAX_LOG_SIZE 128*1048576	/* 128 MB */
		if (log_len_in_bytes > MAX_LOG_SIZE) {
			printf("External journal size is excessive.\n"
			       "Working size set to 128 MB.\n");
			log_len_in_bytes = MAX_LOG_SIZE;
		}

		npages = log_len_in_bytes / LOGPSIZE;

		if (uuid_is_null(uuid))
			/* NULL uuid indicates format new log */
			uuid_generate(uuid);
		else {
			/* Verify existing uuid matches uuid passed in */
			rc = ujfs_rw_diskblocks(fp, (log_begin + LOGPSIZE),
						(unsigned) LOGPSIZE, (char *) log_sup, GET);
			if (rc)
				return -1;
			ujfs_swap_logsuper(log_sup);

			if (!uuid_compare(log_sup->uuid, uuid)) {
				printf("Invalid log device\n");
				return -1;
			}
		}
	}
	/* npages must be an even multiple of 4 since we write pages 4 at
	 * a time.
	 */
	npages &= ~3;

	/*
	 * init log superblock: log page 1
	 */
	log_sup->magic = LOGMAGIC;
	log_sup->version = LOGVERSION;
	log_sup->state = LOGREDONE;
	/* Assign fs s_flag to log superblock.
	 * Currently s_flag carries the inlinelog info and commit option
	 * ( i.e. group commit or lazy commit, etc.. )
	 */
	log_sup->flag = s_flag;
	log_sup->size = npages;
	log_sup->bsize = aggr_blk_size;
	log_sup->l2bsize = s_l2bsize;
	log_sup->end = 2 * LOGPSIZE + LOGPHDRSIZE + LOGRDSIZE;

	if (uuid)
		uuid_copy(log_sup->uuid, uuid);
	else
		uuid_clear(log_sup->uuid);	/* Inline log */

	if (label) {
		strncpy((char *) &log_sup->label, label, sizeof (log_sup->label));
	}

	for (k = 0; k < MAX_ACTIVE; k++)
		uuid_clear(log_sup->active[k]);

	/* swap if on big endian machine */
	ujfs_swap_logsuper(log_sup);

	rc = ujfs_rw_diskblocks(fp, (log_begin + LOGPSIZE),
				(unsigned) LOGPSIZE, (char *) log_sup, PUT);
	if (rc != 0)
		return -1;

	/*
	 * init device pages 2 to npages-1 as log data pages:
	 *
	 * log page sequence number (lpsn) initialization:
	 * the N (= npages-2) data pages of the log is maintained as
	 * a circular file for the log records;
	 * lpsn grows by 1 monotonically as each log page is written
	 * to the circular file of the log;
	 * Since the AIX DUMMY log record is dropped for this XJFS,
	 * and setLogpage() will not reset the page number even if
	 * the eor is equal to LOGPHDRSIZE. In order for binary search
	 * still work in find log end process, we have to simulate the
	 * log wrap situation at the log format time.
	 * The 1st log page written will have the highest lpsn. Then
	 * the succeeding log pages will have ascending order of
	 * the lspn starting from 0, ... (N-2)
	 */

	/*
	   initialize 1st 2 log pages to be written: lpsn = N-1, 0
	   and also a SYNCPT log record is written to the N-1 page

	   Since the log is always an even number of meg, if we
	   write 2 pages before entering the loop, we are assured
	   that the log will end after a 4 page buffer.
	 */

	logp[0].h.eor = logp[0].t.eor = LOGPHDRSIZE + LOGRDSIZE;
	logp[0].h.page = logp[0].t.page = npages - 3;
	lrd_ptr = (struct lrd *) &logp[0].data;
	lrd_ptr->logtid = 0;
	lrd_ptr->backchain = 0;
	/* swap log page data here if needed */
	lrd_ptr->type = __le16_to_cpu(LOG_SYNCPT);
	lrd_ptr->length = 0;
	lrd_ptr->log.syncpt.sync = 0;

	logp[1].h.eor = logp[1].t.eor = LOGPHDRSIZE;
	logp[1].h.page = logp[1].t.page = 0;
	lrd_ptr = (struct lrd *) &logp[1].data;
	lrd_ptr->logtid = 0;
	lrd_ptr->backchain = 0;
	/* swap log page data here if needed */
	lrd_ptr->type = __le16_to_cpu(LOG_SYNCPT);
	lrd_ptr->length = 0;
	lrd_ptr->log.syncpt.sync = 0;

	/* swap if on big endian machine */
	ujfs_swap_logpage(logp, 2);

	rc = ujfs_rw_diskblocks(fp, (log_begin + 2 * LOGPSIZE),
				(unsigned) 2 * LOGPSIZE, (char *) &(logp[0]), PUT);

	ujfs_swap_logpage(logp, 2);	/* Swap back */

	if (rc != 0)
		return -1;

	/* initialize buffer to write 4 pages at a time */
	logp[0].h.eor = logp[0].t.eor = LOGPHDRSIZE;

	logp[2].h.eor = logp[2].t.eor = LOGPHDRSIZE;
	lrd_ptr = (struct lrd *) &logp[2].data;
	lrd_ptr->logtid = 0;
	lrd_ptr->backchain = 0;
	/* swap log page data here if needed */
	lrd_ptr->type = __le16_to_cpu(LOG_SYNCPT);
	lrd_ptr->length = 0;
	lrd_ptr->log.syncpt.sync = 0;

	logp[3].h.eor = logp[3].t.eor = LOGPHDRSIZE;
	lrd_ptr = (struct lrd *) &logp[3].data;
	lrd_ptr->logtid = 0;
	lrd_ptr->backchain = 0;
	/* swap log page data here if needed */
	lrd_ptr->type = __le16_to_cpu(LOG_SYNCPT);
	lrd_ptr->length = 0;
	lrd_ptr->log.syncpt.sync = 0;

	/* initialize succeeding log  pages: lpsn = 1, 2, ..., (N-2) */
	Working_counter = 0;
	log_contwt = log_begin + LOGBUFSIZE;
	for (k = 1; k < npages - 4; k += 4) {
		logp[0].h.page = logp[0].t.page = k;
		logp[1].h.page = logp[1].t.page = k + 1;
		logp[2].h.page = logp[2].t.page = k + 2;
		logp[3].h.page = logp[3].t.page = k + 3;

		/* swap if on big endian machine */
		ujfs_swap_logpage(logp, 4);

		rc = ujfs_rw_diskblocks(fp, log_contwt,
					(unsigned) LOGBUFSIZE, (char *) &(logp[0]), PUT);
		if (rc != 0)
			return -1;

		log_contwt += LOGBUFSIZE;

		/* display heartbeat, but only if output not redirected */
		if (isatty(STDOUT_FILENO)) {
			Working_counter++;
			switch (Working_counter) {
			case (100):
				printf("%s", Working[0]);
				fflush(stdout);
				break;
			case (200):
				printf("%s", Working[1]);
				fflush(stdout);
				break;
			case (300):
				printf("%s", Working[2]);
				fflush(stdout);
				break;
			case (400):
				printf("%s", Working[3]);
				fflush(stdout);
				Working_counter = 0;
				break;
			default:
				break;
			}
		}
	}

	ujfs_flush_dev(fp);

	return (0);

}
