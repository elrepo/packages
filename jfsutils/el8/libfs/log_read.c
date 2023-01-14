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
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <errno.h>
#include "jfs_types.h"
#include "jfs_endian.h"
#include "jfs_filsys.h"
#include "jfs_superblock.h"
#include "jfs_dinode.h"
#include "jfs_dtree.h"
#include "jfs_xtree.h"
#include "jfs_logmgr.h"
#include "jfs_dmap.h"
#include "jfs_imap.h"
#include "logredo.h"
#include "devices.h"
#include "debug.h"
#include "fsck_message.h"		/* for fsck message logging facility */

 /* + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + +
  *
  *    R E M E M B E R    M E M O R Y    A L L O C    F A I L U R E
  *
  */
extern int32_t Insuff_memory_for_maps;
extern char *available_stg_addr;
extern int32_t available_stg_bytes;
extern char *bmap_stg_addr;
extern int32_t bmap_stg_bytes;

 /* + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + +
  *
  *   L O C A L   M A C R O    D E F I N I T I O N S
  *
  */
#define BTOLOGPN(x)  ((unsigned)(x) >> L2LOGPSIZE)

#define LOGPNTOB(x)  ((x)<<L2LOGPSIZE)
#define min(A,B) ((A) < (B) ? (A) : (B))

 /* + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + +
  *
  *    S T U F F    F O R    T H E    L O G
  *
  *       externals defined in logredo.c
  */
int32_t loglastp;		/* last page of log read       */
int32_t lognumread;		/* number of log pages read    */

/*
 *      open file system aggregate/lv array
 *     Defined in logredo.c
 */
extern struct vopen vopen[];	/* (88) */

/*
 *      log page buffer cache
 *
 * log has its own 4 page buffer pool.
 *   externals defined in logredo.c
 */

int nextrep;			/* next log buffer pool slot to replace */
int logptr[4];			/* log pages currently held in logp */
struct logpage logp[4];		/* log page buffer pool  */

/*
 * external references
 */
extern int logError(int, int);

/*
 * forward references
 */
int findEndOfLog(void);
int pageVal(int, int *, int *);
int getLogpage(int);
int setLogpage(int32_t pno, int32_t *, int32_t *, int32_t);
int logRead(int32_t, struct lrd *, char *);
int moveWords(int32_t, int32_t *, int32_t *, int32_t *);

/*
 * NAME:        findEndOfLog()
 *
 * FUNCTION:    Returns the address of the end of the last record in the log.
 *              (i.e. the address of the byte following its descriptor).
 *
 *        Note: At the first release, log page is not written in a ping pong
 *              manner, so the logend is the binary search result
 *
 *              The end of the log is found by finding the page with the
 *              highest page number and for which h.eor == t.eor and
 *              h.eor > 8 (i.e. a record ends on the page).
 *              Page numbers are compared by comparing their difference
 *              with zero (necessary because page numbers are allowed to wrap.)
 *
 * RETURNS:     >0              - byte offset of last record
 *              REFORMAT_ERROR(-3)      - i/o error, reformat the log
 *              MAJOR_ERROR(-2) - other major errors other than EIO.
 */
int findEndOfLog()
{

	int rc;
	int32_t left, right, pmax, pval, eormax, eorval, k;

	/* binary search for logend */
	left = 2;		/* first page containing log records
				   since page 0 is never used, page 1 is
				   log superblock */
	right = Log.size - 1;	/* last page containing log records */

	if ((rc = pageVal(left, &eormax, &pmax)) < 0) {
		fsck_send_msg(lrdo_FEOLPGV1FAIL, rc);
		return (rc);
	}

	while ((right - left) > 1) {
		k = (left + right) >> 1;
		if ((rc = pageVal(k, &eorval, &pval)) < 0) {
			fsck_send_msg(lrdo_FEOLPGV2FAIL, rc);
			return (rc);
		}

		if (pval - pmax > 0) {
			left = k;
			pmax = pval;
			eormax = eorval;
		} else
			right = k;
	}
	if ((rc = pageVal(right, &eorval, &pval)) < 0) {
		fsck_send_msg(lrdo_FEOLPGV3FAIL, rc);
		return (rc);
	}

	/*
	 * the last thing to determine is whether it is the first page of
	 * the last long log record and system was crashed when its second
	 * page is written. If the eor of the chosen page is LOGPHDRSIZE,
	 * then this page contains a partial log record, ( otherwise, the
	 * the long log record's second page should be chosen ).
	 * This page should be thrown away. its previous page will be
	 * the real last log page.
	 */

	if ((pval - pmax) > 0) {
		if (eorval == LOGPHDRSIZE) {
			if ((rc = pageVal(right - 1, &eorval, &pval)) < 0) {
				fsck_send_msg(lrdo_FEOLPGV4FAIL, rc);
				return (rc);
			}
			return (LOGPNTOB(right - 1) + eorval);
		} else
			return (LOGPNTOB(right) + eorval);
	} else {
		if (eormax == LOGPHDRSIZE) {
			left = (left == 2) ? Log.size - 1 : left - 1;
			if ((rc = pageVal(left, &eormax, &pmax)) < 0) {
				fsck_send_msg(lrdo_FEOLPGV4AFAIL, rc);
				return (rc);
			}
		}
		return (LOGPNTOB(left) + eormax);
	}
}

/*
 * NAME:        pageVal(pno, eor, pmax)
 *
 * FUNCTION:    Read the page into the log buffer pool and call setLogpage
 *              to form consistent log page.
 *
 * RETURNS:     0                       - ok
 *              REFORMAT_ERROR(-3)      - I/O error, reformat the log
 *              MAJOR_ERROR(-2)         - other major errors other than EIO.
 */
int pageVal(int pno,		/* page number in log           */
	    int *eor,		/* corresponding eor value      */
	    int *pmax)
{				/* pointer to returned page number */
	int buf0;		/* logp[] buffer element number         */

	/* Read the page into the log buffer pool. */
	if ((buf0 = getLogpage(pno)) < 0) {
		fsck_send_msg(lrdo_PVGETPGFAIL, pno, buf0);

		return (buf0);
	}
	return (setLogpage(pno, eor, pmax, buf0));
}

/*
 * NAME:        getLogpage(pno)
 *
 * FUNCTION:    if the specified log page is in buffer pool, return its
 *              index. Otherwise read log page into buffer pool.
 *
 * PARAMETERS:  pno -   log page number to look for.
 *
 * RETURNS:     0 - 3   - index of the buffer pool the page located
 *              REFORMAT_ERROR(-3)      - I/O error, reformat the log
 *              MAJOR_ERROR(-2)         - other major errors other than EIO.
 */
int getLogpage(int pno)
{				/* page of log */
	int k, rc;

	/*
	 * is it in buffer pool ?
	 */
	for (k = 0; k <= 3; k++)
		if (logptr[k] == pno)
			return (k);

	/*
	 * read page into buffer pool into next slot
	 * don't have to use llseek() here.  log dev will never be > 2 gig
	 */
	nextrep = (nextrep + 1) % 4;
	if (Log.location & INLINELOG)
		rc = ujfs_rw_diskblocks(Log.fp,
					(uint64_t) (Log.xaddr + LOGPNTOB(pno)),
					(unsigned) LOGPSIZE, (char *) &logp[nextrep], GET);
	else
		rc = ujfs_rw_diskblocks(Log.fp,
					(uint64_t) LOGPNTOB(pno),
					(unsigned) LOGPSIZE, (char *) &logp[nextrep], GET);

	if (rc != 0) {
		return (JLOG_READERROR1);
	}

	logptr[nextrep] = pno;
	return (nextrep);
}

/*
 * NAME:        setLogpage(pno, eor, pmax, buf)
 *
 * FUNCTION:    Forms consistent log page and returns eor and pmax values.
 *
 *              During the first release the following conditions are
 *              assumed:
 *              1) No corrupted write during power failure
 *              2) No split write
 *              3) No out-of-order sector write
 *
 *              If the header and trailer in the page are not equal, a
 *              system crash happened during this page write. It
 *              is reconciled as follows:
 *
 *              1) if h.page != t.page, the smaller value is taken and
 *                 the eor fields set to LOGPHDSIZE.
 *                 reason: This can happen when a old page is over-written
 *                 by a new page and the system crashed. So this page
 *                 should be considered not written.
 *              2) if h.eor != t.eor, the smaller value is taken.
 *                 reason: The last log page was rewritten for each
 *                 commit record. A system crash happened during the
 *                 page rewriting. Since we assume that no corrupted write
 *                 no split write and out-of-order sector write, the
 *                 previous successfuly writing is still good
 *              3) if no record ends on the page (eor = 8), still return it.
 *                 Let the caller determine whether a) a good long log record
 *                 ends on the next log page. or b) it is the first page of the
 *                 last long log record and system was crashed when its second
 *                 page is written.
 *
 *
 * RETURNS:     0                       - ok
 *              REFORMAT_ERROR(-3)      - I/O error, reformat log
 *              MAJOR_ERROR(-2)         - other major error
 */
int setLogpage(int32_t pno,	/* page number of log           */
	       int32_t *eor,	/* log header eor to return     */
	       int32_t *pmax,	/* log header page number to return */
	       int32_t buf)
{				/* logp[] index number for page */
	int rc;
	int32_t diff1, diff2;

	/* check that header and trailer are the same */
	if ((diff1 = (__le32_to_cpu(logp[buf].h.page) - __le32_to_cpu(logp[buf].t.page))) != 0) {
		if (diff1 > 0)
			/* Both little-endian */
			logp[buf].h.page = logp[buf].t.page;
		else
			/* Both little-endian */
			logp[buf].t.page = logp[buf].h.page;

		logp[buf].h.eor = logp[buf].t.eor = __cpu_to_le16(LOGPHDRSIZE);
		/* empty page */
	}

	if ((diff2 = (__le16_to_cpu(logp[buf].h.eor) - __le16_to_cpu(logp[buf].t.eor))) != 0) {
		if (diff2 > 0)
			/* Both little-endian */
			logp[buf].h.eor = logp[buf].t.eor;
		else
			/* Both little-endian */
			logp[buf].t.eor = logp[buf].h.eor;
	}

	/* if any difference write the page out */
	if (diff1 || diff2) {
		rc = ujfs_rw_diskblocks(Log.fp,
					(uint64_t) (Log.xaddr + LOGPNTOB(pno)),
					(unsigned long) LOGPSIZE, (char *) &logp[buf], PUT);
		if (rc != 0) {
			fsck_send_msg(lrdo_SLPWRITEFAIL, pno, rc);

			return (JLOG_WRITEERROR1);
		}
	}

	/*
	 * At this point, it is still possible that logp[buf].h.eor
	 * is LOGPHDRSIZE, but we return it anyway. The caller will make
	 * decision.
	 */

	*eor = __le16_to_cpu(logp[buf].h.eor);
	*pmax = __le32_to_cpu(logp[buf].h.page);

	return (0);
}

 /*
    * NAME:        logRead(logaddr , ld, dataptr)
    *
    * FUNCTION:    reads the log record addressed by logaddr and
    *              returns the address of the preceding log record.
    *
    * PARAMETERS:  logaddr -  address of the end of log record to read
    *                                 Note: log is read backward, so this is
    *                                 the address starting to read
    *              ld      - pointer to a log record descriptor
    *              dataptr - pointer to data buffer
    *
    * RETURNS:     < 0     - there is an i/o error in reading
    *              > 0     - the address of the end of the preceding log record
  */
int logRead(int32_t logaddr,	/* address of log record to read */
	    struct lrd *ld,	/* pointer to a log record descriptor */
	    char *dataptr)
{				/* pointer to buffer.  LOGPSIZE*2 long */
	int buf, off, rc, nwords, pno;

	/* get page containing logaddr into log buffer pool */
	pno = BTOLOGPN(logaddr);
	if (pno != loglastp) {
		loglastp = pno;
		lognumread += 1;
		if (lognumread > Log.size - 2) {
			logError(LOGWRAP, 0);
			fsck_send_msg(lrdo_LRLOGWRAP, lognumread);

			return (JLOG_LOGWRAP);
		}
	}

	buf = getLogpage(pno);
	if (buf < 0) {
		fsck_send_msg(lrdo_LRREADFAIL, pno, buf);

		return (buf);
	}

	/* read the descriptor */
	off = logaddr & (LOGPSIZE - 1);	/* offset just past desc. */
	rc = moveWords(LOGRDSIZE / 4, (int32_t *) ld, &buf, &off);
	if (rc < 0) {
		fsck_send_msg(lrdo_LRMWFAIL1, rc);
		return (rc);
	}
	ujfs_swap_lrd(ld);

	/*
	 * Legacy code used device number in ld->aggegate.  This code only
	 * supported a single volume attached to the journal
	 */
	if (ld->aggregate > MAX_ACTIVE)
		ld->aggregate = 0;

	/* read the data if there is any */
	if (ld->length > 0) {
		if (ld->length > LOGPSIZE * 2) {
			rc = READLOGERROR;
			fsck_send_msg(lrdo_LRMWFAIL3, pno);
			return (rc);
		}

		/* if length is partial word, still read it   */
		nwords = (ld->length + 3) / 4;

		rc = moveWords(nwords, (int32_t *) dataptr, &buf, &off);
		if (rc < 0) {
			fsck_send_msg(lrdo_LRMWFAIL2, rc);
			return (rc);
		}
	}

	return (LOGPNTOB(logptr[buf]) + off);
}

/*
 * NAME:        moveWords()
 *
 * FUNCTION:    moves nwords from buffer pool to target. data
 *              is moved in backwards direction starting at offset.
 *              If partial log record is on the previous page,
 *              or we have exhaust the current page (all bytes were read),
 *              the previous page is read into the buffer pool.
 *              On exit buf will point to this page in the buffer pool
 *              and offset to where the move stopped.
 *
 *              Note: the previous page is fetched whenever
 *              the current page is exhausted (all bytes were read)
 *              even if all the words required to satisfy this move
 *              are on the current page.
 *
 * PARAMETERS:  nwords  - number of 4-byte words to move
 *              target  - address of target (begin address)
 *              buf     - index in buffer pool of current page
 *              offset  - initial offset in buffer pool page, this offset
 *                        includes the page head size
 *
 * RETURNS:     = 0             - ok
 *              < 0             - error returned from getLogpage
 */
int moveWords(int32_t nwords,	/* number of 4-byte words to move */
		  int32_t *target,	/* address of target (begin address) */
		  int32_t *buf,		/* index in buffer pool of curr page */
		  int32_t *offset)
{				/* initial offset in buffer pool page */
	int n, j, words, pno;
	int *ptr;

	j = (*offset - LOGPHDRSIZE) / 4 - 1;	/* index in log page data area
						   of first word to move      */
	words = min(nwords, j + 1);	/* words on this page to move */
	ptr = target + nwords - 1;	/* last word of target */
	for (n = 0; n < words; n++) {
		*ptr = logp[*buf].data[j];
		j = j - 1;
		ptr = ptr - 1;
	}
	*offset = *offset - 4 * words;

	/*
	 * If partial log record is on the previous page,
	 * or we have read all the log records in the current page,
	 * get the previous page
	 */

	while (words != nwords	/* we get less than nwords */
	       || j < 0) {	/* or exhaust the page, so offset is just */
		nwords -= words;
		/* the page head, then j < 0              */
		/* get previous page */
		pno = logptr[*buf];
		pno = pno - 1;
		/* if we hit beginning location of the log, go wrapped,
		   read log record from the end location of the log   */
		if (pno == 1)
			pno = Log.size - 1;
		*buf = getLogpage(pno);
		if (*buf < 0) {
			fsck_send_msg(lrdo_MWREADFAIL, pno, (*buf));

			return (*buf);
		}
		*offset = LOGPSIZE - LOGPTLRSIZE;
		/* index last word of data area */
		j = LOGPSIZE / 4 - 4 - 1;
		/* move rest of nwords if any. */
		for (n = 0; n < nwords; n++) {
			if (j < 0)
				break;
			*ptr = logp[*buf].data[j];
			j = j - 1;
			ptr = ptr - 1;
		}
		words = n;
		*offset = *offset - 4 * nwords;
	}

	return (0);
}
