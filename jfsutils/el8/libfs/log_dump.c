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
#include <fcntl.h>
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

extern int LogOpenMode;

#define LOGDMP_OK 	0
#define LOGDMP_FAILED	-1

#define MAKEDEV(__x,__y)        (dev_t)(((__x)<<16) | (__y))

#define LOGPNTOB(x)  ((x)<<L2LOGPSIZE)

#define LOG2NUM(NUM, L2NUM)\
{\
        if ((NUM) <= 0)\
                L2NUM = -1;\
        else\
        if ((NUM) == 1)\
                L2NUM = 0;\
        else\
        {\
                L2NUM = 0;\
                while ( (NUM) > 1 )\
                {\
                        L2NUM++;\
                        (NUM) >>= 1;\
                }\
        }\
}

/* + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + +
 *
 *       things for the log.
 */
int32_t logend;			/* address of the end of last log record */
struct logsuper logsup;		/* log super block */
int32_t numdoblk;		/* number of do blocks used     */
int32_t numnodofile;		/* number of nodo file blocks used  */

/* + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + +
 *
 * The output file.
 *
 */

FILE *outfp;

#define  output_filename  "./jfslog.dmp"

int logdmp_outfile_is_open = 0;

/* + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + +
 *
 *      open file system aggregate/lv array
 *
 * logredo() processes a single log.
 * at the first release, logredo will process a single log
 * related to one aggregate. But the future release, logredo needs to
 * to process one single log related to multiple agreegates.
 * In both cases, the aggregate(logical volume) where the log stays
 * will be different from  the file system aggregate/lv.
 *
 * There will be one imap for the aggregate inode allocation map
 * and a list of imap pointers to multiple fileset inode allocation maps.
 *
 * There is one block allocation map per aggregate and shared by all the
 * filesets within the aggregate.
 *
 * the log and related aggregates (logical volumes) are all in
 * the same volume group, i.e., each logical volume is uniquely specified
 * by their minor number with the same major number,
 * the maximum number of lvs in a volume group is NUMMINOR (256).
 */

/*
 * We only deal with the log here.  No need for vopen array
 */
struct vopen volume;

/* + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + +
 *
 *      file system page buffer cache
 *
 * for k > 0, bufhdr[k] describes contents of buffer[k-1].
 * bufhdr[0] is reserved as anchor for free/lru list:
 * bufhdr[0].next points to the MRU buffer (head),
 * bufhdr[0].prev points to the LRU buffer (tail);
 */

/* buffer header table */
struct bufhdr {
	int16_t next;		/* 2: next on free/lru list */
	int16_t prev;		/* 2: previous on free/lru list */
	int16_t hnext;		/* 2: next on hash chain */
	int16_t hprev;		/* 2: previous on hash chain */
	char modify;		/* 1: buffer was modified */
	char inuse;		/* 1: buffer on hash chain */
	int16_t reserve;	/* 2 */
	int32_t vol;		/* 4: minor of agrregate/lv number */
	pxd_t pxd;		/* 8: on-disk page pxd */
} bufhdr[NBUFPOOL];		/* (24) */

/* buffer table */
struct bufpool {
	char bytes[PSIZE];
} buffer[NBUFPOOL - 1];

/*
 *      log page buffer cache
 *
 * log has its own 4 page buffer pool.
 */
uint8_t afterdata[LOGPSIZE * 2];	/* buffer to read in redopage data */

/*
 * Miscellaneous
 */
caddr_t prog;			/* Program name */
int32_t mntcnt, bufsize;
char *mntinfo;
int32_t retcode;		/* return code from logredo    */

/*
 * external references
 */
extern char *optarg;
extern int optind;
extern int initMaps(int32_t);
extern int updateMaps(int);
extern int findEndOfLog(void);
extern int logRead(int32_t, struct lrd *, char *);
extern int logredoInit(void);
extern int alloc_wrksp(uint32_t, int, int, void **);	/* defined in fsckwsp.c */

/*
 * forward references
 */
int open_outfile(void);
int ldmp_readSuper(FILE *, struct superblock *);
int ldmp_isLogging(caddr_t, int32_t, char *, int32_t);
int ldmp_logError(int, int);
int usage(void);

int disp_updatemap(struct lrd *);
int disp_redopage(struct lrd *);
int disp_noredopage(struct lrd *);
int disp_noredoinoext(struct lrd *);

void ldmp_xdump(char *, int);
int ldmp_x_scmp(char *, char *);
void ldmp_x_scpy(char *, char *);
int prtdesc(struct lrd *);

/* --------------------------------------------------------------------
 *
 * NAME:        jfs_logdump()
 *
 * FUNCTION:
 *
 */

int jfs_logdump(caddr_t pathname, FILE *fp, int32_t dump_all)
{
	int rc;
	int32_t logaddr, nextaddr, lastaddr, nlogrecords;
	struct lrd ld;
	int32_t lowest_lr_byte = 2 * LOGPSIZE + LOGPHDRSIZE;
	int32_t highest_lr_byte = 0;
	int log_has_wrapped = 0;
	int in_use;

	rc = open_outfile();

	if (rc == 0) {		/* output file is open */
		/*
		 * loop until we get enough memory to read vmount struct
		 */
		mntinfo = (char *) &bufsize;
		bufsize = sizeof (int);

		/*
		 * Find and open the log
		 */
		LogOpenMode = O_RDONLY;
		rc = findLog(fp, &in_use);

		if (rc != 0) {
			printf("JFS_LOGDUMP:Error occurred when open/read device\n");
			fprintf(outfp, "??????????????????????????????????????????????????????\n");
			fprintf(outfp, "JFS_LOGDUMP:Error occurred when open/read device\n");
			fprintf(outfp, "??????????????????????????????????????????????????????\n");
			return (rc);
		}

		/*
		 * validate log superblock
		 *
		 * aggregate block size is for log file as well.
		 */

		rc = ujfs_rw_diskblocks(Log.fp,
					(uint64_t) (Log.xaddr + LOGPNTOB(LOGSUPER_B)),
					(unsigned) sizeof (struct logsuper), (char *) &logsup, GET);
		if (rc != 0) {
			printf("JFS_LOGDUMP:couldn't read log superblock:failure in %s\n", prog);
			fprintf(outfp, "??????????????????????????????????????????????????????\n");
			fprintf(outfp, "JFS_LOGDUMP:couldn't read log superblock:failure in %s\n",
				prog);
			fprintf(outfp, "??????????????????????????????????????????????????????\n");
			return (LOGSUPER_READ_ERROR);
		}
		ujfs_swap_logsuper(&logsup);

		fprintf(outfp, "JOURNAL SUPERBLOCK: \n");
		fprintf(outfp, "------------------------------------------------------\n");
		fprintf(outfp, "   magic number: x %x \n", logsup.magic);
		fprintf(outfp, "   version     : x %x \n", logsup.version);
		fprintf(outfp, "   serial      : x %x \n", logsup.serial);
		fprintf(outfp, "   size        : t %d pages (4096 bytes/page)\n", logsup.size);
		fprintf(outfp, "   bsize       : t %d bytes/block\n", logsup.bsize);
		fprintf(outfp, "   l2bsize     : t %d \n", logsup.l2bsize);
		fprintf(outfp, "   flag        : x %x \n", logsup.flag);
		fprintf(outfp, "   state       : x %x \n", logsup.state);
		fprintf(outfp, "   end         : x %x \n", logsup.end);
		fprintf(outfp, "\n");
		fprintf(outfp, "======================================================\n");
		fprintf(outfp, "\n");

		if (logsup.magic != LOGMAGIC) {
			fprintf(outfp, "\n");
			fprintf(outfp, "**WARNING** %s: %s is not a log file\n", prog, pathname);
			fprintf(outfp, "\n");
			fprintf(outfp, "======================================================\n");
			fprintf(outfp, "\n");
		}

		if (logsup.version != LOGVERSION) {
			fprintf(outfp, "\n");
			fprintf(outfp, "**WARNING** %s and log file %s version mismatch\n", prog,
				pathname);
			fprintf(outfp, "\n");
			fprintf(outfp, "======================================================\n");
			fprintf(outfp, "\n");
		}

		if (logsup.state == LOGREDONE) {
			fprintf(outfp, "\n");
			fprintf(outfp, "**WARNING** %s and log file %s state is LOGREDONE\n", prog,
				pathname);
			fprintf(outfp, "\n");
			fprintf(outfp, "======================================================\n");
			fprintf(outfp, "\n");
		}

		Log.size = logsup.size;
		Log.serial = logsup.serial;

		/*
		 * find the end of log
		 */
		logend = findEndOfLog();
		if (logend < 0) {
			printf("logend < 0\n");
			ldmp_logError(LOGEND, 0);
			ujfs_swap_logsuper(&logsup);
			rc = ujfs_rw_diskblocks(Log.fp,
						(uint64_t) (Log.xaddr + LOGPNTOB(LOGSUPER_B)),
						(unsigned long) LOGPSIZE, (char *) &logsup, PUT);
			rc = logend;
			goto loopexit;
		}

		highest_lr_byte = logsup.size * LOGPSIZE - LOGRDSIZE;

		if ((logend < lowest_lr_byte) || (logend > highest_lr_byte)) {
			fprintf(outfp, "\n");
			fprintf(outfp,
				"**ERROR** logend address is not valid for a logrec. logend: 0x0%x\n",
				logend);
			fprintf(outfp, "\n");
			fprintf(outfp, "======================================================\n");
			fprintf(outfp, "\n");
			return (INVALID_LOGEND);
		}

		/*
		 *      replay log
		 *
		 * read log backwards and process records as we go.
		 * reading stops at place specified by first SYNCPT we
		 * encounter.
		 */
		nlogrecords = lastaddr = 0;
		nextaddr = logend;
		do {
			logaddr = nextaddr;
			nextaddr = logRead(logaddr, &ld, afterdata);
			fprintf(outfp,
				"logrec d %d   Logaddr= x %x   Nextaddr= x %x   Backchain = x %x\n",
				nlogrecords, logaddr, nextaddr, ld.backchain);
			fprintf(outfp, "\n");
			nlogrecords += 1;
			/*
			 *
			 * Validate the nextaddr as much as possible
			 *
			 */
			if (nextaddr < 0) {
				ldmp_logError(READERR, logaddr);
				if (nextaddr == REFORMAT_ERROR) {
					rc = nextaddr;
					goto loopexit;
				}
				break;
			}
			/*
			 * Certain errors we'll assume signal the end of the log
			 * since we're just dumping everything from the latest
			 * commit record to the earliest valid record.
			 */
			if ((nextaddr < lowest_lr_byte) || (nextaddr > highest_lr_byte)) {
				lastaddr = logaddr;
			}

			if (nextaddr == logaddr) {
				lastaddr = logaddr;
			}

			if (nextaddr > logaddr) {
				if (log_has_wrapped) {
					fprintf(outfp, "\n");
					fprintf(outfp,
						"**ERROR** log wrapped twice. logaddr:0x0%x nextaddr:0x0%x\n",
						logaddr, nextaddr);
					fprintf(outfp, "\n");
					fprintf(outfp,
						"======================================================\n");
					fprintf(outfp, "\n");
					lastaddr = logaddr;
				} else {
					log_has_wrapped = -1;
				}
			}
			/*
			 *
			 * The addresses seem ok.  Process the current record.
			 *
			 */
			if (lastaddr != logaddr) {
				switch (ld.type) {
				case LOG_COMMIT:
					fprintf(outfp,
						"LOG_COMMIT   (type = d %d)   logtid = d %d   aggregate = d %d\n",
						ld.type, ld.logtid, ld.aggregate);
					fprintf(outfp, "\n");
					fprintf(outfp, "\tdata length = d %d\n", ld.length);

					break;

				case LOG_MOUNT:
					fprintf(outfp,
						"++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
					fprintf(outfp,
						"LOG_MOUNT   (type = d %d)   logtid = d %d   aggregate = d %d\n",
						ld.type, ld.logtid, ld.aggregate);
					fprintf(outfp, "\n");
					fprintf(outfp, "\tdata length = d %d\n", ld.length);
					fprintf(outfp,
						"++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
					break;

				case LOG_SYNCPT:
					fprintf(outfp,
						"****************************************************************\n");
					fprintf(outfp,
						"LOG_SYNCPT   (type = d %d)   logtid = d %d    aggregate = d %d\n",
						ld.type, ld.logtid, ld.aggregate);
					fprintf(outfp, "\n");
					fprintf(outfp, "\tdata length = d %d\n", ld.length);
					fprintf(outfp, "\tsync = x %x\n", ld.log.syncpt.sync);
					fprintf(outfp,
						"****************************************************************\n");

					rc = 0;
					if (!dump_all) {	/* user just wants from last synch point forward */
						if (lastaddr == 0) {
							lastaddr = (ld.log.syncpt.sync == 0)
							    ? logaddr : ld.log.syncpt.sync;
						}
					}	/* end user just wants from last synch point forward */
					break;

				case LOG_REDOPAGE:
					fprintf(outfp,
						"LOG_REDOPAGE   (type = d %d)   logtid = d %d    aggregate = d %d\n",
						ld.type, ld.logtid, ld.aggregate);
					fprintf(outfp, "\n");
					fprintf(outfp, "\tdata length = d %d    ", ld.length);
					disp_redopage(&ld);
					break;

				case LOG_NOREDOPAGE:
					fprintf(outfp,
						"LOG_NOREDOPAGE   (type = d %d)   logtid = d %d   aggregate = d %d\n",
						ld.type, ld.logtid, ld.aggregate);
					fprintf(outfp, "\n");
					fprintf(outfp, "\tdata length = d %d    ", ld.length);
					disp_noredopage(&ld);
					break;

				case LOG_NOREDOINOEXT:
					fprintf(outfp,
						"LOG_NOREDOINOEXT (type = d %d)   logtid = d %d   aggregate = d %d\n",
						ld.type, ld.logtid, ld.aggregate);
					fprintf(outfp, "\n");
					fprintf(outfp, "\tdata length = d %d    ", ld.length);
					disp_noredoinoext(&ld);
					break;

				case LOG_UPDATEMAP:
					fprintf(outfp,
						"LOG_UPDATEMAP   (type = d %d)   logtid = d %d   aggregate = d %d\n",
						ld.type, ld.logtid, ld.aggregate);
					fprintf(outfp, "\n");
					fprintf(outfp, "\tdata length = d %d    ", ld.length);
					disp_updatemap(&ld);
					break;

				default:
					fprintf(outfp,
						"*UNRECOGNIZED*   (type = d %d)   logtid = d %d   aggregate = d %d\n",
						ld.type, ld.logtid, ld.aggregate);
					fprintf(outfp, "\n");
					fprintf(outfp, "\tdata length = d %d\n", ld.length);
					fprintf(outfp, "\n");
					fprintf(outfp, "**ERROR** unrecognized log record type\n");
					fprintf(outfp, "\n");
					fprintf(outfp,
						"======================================================\n");
					fprintf(outfp, "\n");
					return (UNRECOG_LOGRECTYP);
				}

				if (rc == 0) {
					fprintf(outfp, "\n");
					if (ld.length > 0) {
						ldmp_xdump((char *) afterdata, ld.length);
					}
				}

				fprintf(outfp, "\n");
				fprintf(outfp,
					"----------------------------------------------------------------------\n");
			}
			/* end if( lastaddr != logaddr )  */
		} while (logaddr != lastaddr);

	      loopexit:

		/*
		 * Close the output file
		 */
		if (logdmp_outfile_is_open) {
			fclose(outfp);
		}

		if (rc == 0) {	/* log has been dumped successfully */
			printf
			    ("JFS_LOGDUMP: The current JFS log has been dumped into ./jfslog.dmp\n");
		} else {
			printf("JFS_LOGDUMP:Failed in %s\n", prog);
			fprintf(outfp, "??????????????????????????????????????????????????????\n");
			fprintf(outfp, "JFS_LOGDUMP:Failed in %s\n", prog);
			fprintf(outfp, "??????????????????????????????????????????????????????\n");
		}
	}
	/* end output file is open */
	return (rc < 0) ? (rc) : (0);
}

/*----------------------------------------------------------------
 *
 * NAME:        ldmp_readSuper(fp, sb)
 *
 * FUNCTION:    read the superblock for the file system described
 *              by the file descriptor of the opened aggregate/lv.
 *              if a read of primary superblock fails,
 *              try to read the secondary superblock. report error only
 *              when both reads failed.
 */
int ldmp_readSuper(FILE *fp,	/* file descriptor */
		   struct superblock * sb)
{				/* superblock of the opened aggregate/lv */
	int rc;

	union {
		struct superblock super;
		char block[PSIZE];
	} super;

	/*
	 * seek to the postion of the primary superblock.
	 * since at this time we don't know the aggregate/lv
	 * logical block size yet, we have to use the fixed
	 * byte offset address SUPER1_OFF to seek for.
	 */

	/*
	 * read super block
	 */
	rc = ujfs_rw_diskblocks(fp, SUPER1_OFF, (unsigned) SIZE_OF_SUPER, super.block, GET);
	if (rc != 0) {
		printf
		    ("ldmp_readSuper: read primary agg superblock failed. errno=%d  Continuing.\n",
		     errno);
		fprintf(outfp, "??????????????????????????????????????????????????????\n");
		fprintf(outfp,
			"ldmp_readSuper: read primary agg superblock failed. errno=%d Continuing\n",
			errno);
		fprintf(outfp, "??????????????????????????????????????????????????????\n");
		/* read failed for the primary superblock:
		 * try to read the secondary superblock
		 */
		rc = ujfs_rw_diskblocks(fp, SUPER2_OFF, (unsigned) SIZE_OF_SUPER, super.block, GET);
		if (rc != 0) {
			printf
			    ("ldmp_readSuper: read 2ndary agg superblock failed. errno=%d  Cannot continue.\n",
			     errno);
			fprintf(outfp, "??????????????????????????????????????????????????????\n");
			fprintf(outfp,
				"ldmp_readSuper: read 2ndary agg superblock failed. errno=%d  Cannot continue.\n",
				errno);
			fprintf(outfp, "??????????????????????????????????????????????????????\n");
			return (MAJOR_ERROR);
		}
	}

	*sb = super.super;

	ujfs_swap_superblock(sb);

	return (0);
}

extern void exit(int);

/*----------------------------------------------------------------
 *
 *      ldmp_logError(type)
 *
 * error handling for log read errors.
 */
int ldmp_logError(int type, int logaddr)
{
	retcode = -1;
	logsup.state = LOGREADERR;

	switch (type) {
	case LOGEND:
		printf("ldmp_logError:find end of log failed \n");
		fprintf(outfp, "??????????????????????????????????????????????????????\n");
		fprintf(outfp, "ldmp_logError:find end of log failed \n");
		fprintf(outfp, "??????????????????????????????????????????????????????\n");
		break;
	case READERR:
		printf("log read failed 0x%x\n", logaddr);
		fprintf(outfp, "??????????????????????????????????????????????????????\n");
		fprintf(outfp, "log read failed 0x%x\n", logaddr);
		fprintf(outfp, "??????????????????????????????????????????????????????\n");
		break;
	case UNKNOWNR:
		printf("unknown log record type \nlog read failed 0x%x\n", logaddr);
		fprintf(outfp, "??????????????????????????????????????????????????????\n");
		fprintf(outfp, "unknown log record type \nlog read failed 0x%x\n", logaddr);
		fprintf(outfp, "??????????????????????????????????????????????????????\n");
		break;
	case IOERROR:
		printf("i/o error log reading page 0x%x\n", logaddr);
		fprintf(outfp, "??????????????????????????????????????????????????????\n");
		fprintf(outfp, "i/o error log reading page 0x%x\n", logaddr);
		fprintf(outfp, "??????????????????????????????????????????????????????\n");
		break;
	case LOGWRAP:
		printf("log wrapped...\n");
		fprintf(outfp, "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\n");
		fprintf(outfp, "log wrapped...\n");
		fprintf(outfp, "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\n");
	}

	return (0);
}

/*----------------------------------------------------------------
 *
 *      ldmp_xdump()
 *
 * hex dump
 */
void ldmp_xdump(char *saddr, int count)
{
#define LINESZ     60
#define ASCIISTRT  40
#define HEXEND     36

	int i, j, k, hexdigit;
	int c;
	char *hexchar;
	char linebuf[LINESZ + 1];
	char prevbuf[LINESZ + 1];
	char *linestart;
	int asciistart;
	char asterisk = ' ';

	hexchar = "0123456789ABCDEF";
	prevbuf[0] = '\0';
	i = (int) saddr % 4;
	if (i != 0)
		saddr = saddr - i;

	for (i = 0; i < count;) {
		for (j = 0; j < LINESZ; j++)
			linebuf[j] = ' ';

		linestart = saddr;
		asciistart = ASCIISTRT;
		for (j = 0; j < HEXEND;) {
			for (k = 0; k < 4; k++) {
				c = *(saddr++) & 0xFF;
				if ((c >= 0x20) && (c <= 0x7e))
					linebuf[asciistart++] = (char) c;
				else
					linebuf[asciistart++] = '.';
				hexdigit = c >> 4;
				linebuf[j++] = hexchar[hexdigit];
				hexdigit = c & 0x0f;
				linebuf[j++] = hexchar[hexdigit];
				i++;
			}
			if (i >= count)
				break;
			linebuf[j++] = ' ';
		}
		linebuf[LINESZ] = '\0';
		if (((j = ldmp_x_scmp(linebuf, prevbuf)) == 0) && (i < count)) {
			if (asterisk == ' ') {
				asterisk = '*';
				fprintf(outfp, "    *\n");
			}
		} else {
			fprintf(outfp, "    %p  %s\n", linestart, linebuf);
			asterisk = ' ';
			ldmp_x_scpy(prevbuf, linebuf);
		}
	}

	return;
}

/*----------------------------------------------------------------
 *
 *      ldmp_x_scmp()
 *
 */
int ldmp_x_scmp(char *s1, char *s2)
{
	while ((*s1) && (*s1 == *s2)) {
		s1++;
		s2++;
	}
	if (*s1 || *s2)
		return (-1);
	else
		return (0);
}

/*----------------------------------------------------------------
 *
 *      ldmp_x_scpy()
 *
 */
void ldmp_x_scpy(char *s1, char *s2)
{
	while ((*s1 = *s2) != '\0') {
		s1++;
		s2++;
	}
}

/***************************************************************************
 *
 * NAME: disp_noredopage
 *
 * FUNCTION:
 *
 * PARAMETERS:  none
 *
 * NOTES:
 *
 * RETURNS:
 *      success: LOGDMP_OK
 *      failure: something else
 */
int disp_noredopage(struct lrd *lrd_ptr)
{
	fprintf(outfp, "fileset = d %d    inode = d %d (x %x)\n",
		lrd_ptr->log.noredopage.fileset,
		lrd_ptr->log.noredopage.inode, lrd_ptr->log.noredopage.inode);

	switch (lrd_ptr->log.noredopage.type) {
	case LOG_INODE:
		fprintf(outfp, "\ttype = d %d NOREDOPAGE:INODE\n", lrd_ptr->log.noredopage.type);
		break;
	case LOG_XTREE:
		fprintf(outfp, "\ttype = d %d NOREDOPAGE:XTREE\n  ", lrd_ptr->log.noredopage.type);
		break;
	case (LOG_XTREE | LOG_NEW):
		fprintf(outfp, "\ttype = d %d NOREDOPAGE:XTREE_NEW\n  ",
			lrd_ptr->log.noredopage.type);
		break;
	case (LOG_BTROOT | LOG_XTREE):
		fprintf(outfp, "\ttype = d %d NOREDOPAGE:BTROOT_XTREE\n  ",
			lrd_ptr->log.noredopage.type);
		break;
	case LOG_DTREE:
		fprintf(outfp, "\ttype = d %d NOREDOPAGE:DTREE\n  ", lrd_ptr->log.noredopage.type);
		break;
	case (LOG_DTREE | LOG_NEW):
		fprintf(outfp, "\ttype = d %d NOREDOPAGE:DTREE_NEW \n ",
			lrd_ptr->log.noredopage.type);
		break;
	case (LOG_DTREE | LOG_EXTEND):
		fprintf(outfp, "\ttype = d %d NOREDOPAGE:DTREE_EXTEND\n  ",
			lrd_ptr->log.noredopage.type);
		break;
	case (LOG_BTROOT | LOG_DTREE):
		fprintf(outfp, "\ttype = d %d NOREDOPAGE:BTROOT_DTREE\n  ",
			lrd_ptr->log.noredopage.type);
		break;
	case (LOG_BTROOT | LOG_DTREE | LOG_NEW):
		fprintf(outfp, "\ttype = d %d NOREDOPAGE:BTROOT_DTREE.NEW\n  ",
			lrd_ptr->log.noredopage.type);
		break;
	case LOG_EA:
		fprintf(outfp, "\ttype = d %d NOREDOPAGE:EA\n", lrd_ptr->log.noredopage.type);
		break;
	case LOG_ACL:
		fprintf(outfp, "\ttype = d %d NOREDOPAGE:ACL\n", lrd_ptr->log.noredopage.type);
		break;
	case LOG_DATA:
		fprintf(outfp, "\ttype = d %d NOREDOPAGE:DATA\n", lrd_ptr->log.noredopage.type);
		break;
/*
    case LOG_NOREDOFILE:
       fprintf( outfp, "\ttype = d %d NOREDOPAGE:NOREDOFILE\n",
                lrd_ptr->log.noredopage.type );
       break;
*/
	default:
		fprintf(outfp, "\ttype = d %d ***UNRECOGNIZED***\n", lrd_ptr->log.noredopage.type);
		break;
	}

	fprintf(outfp, "\tpxd length = d %d   phys offset = x %llx  (d %lld)\n",
		lengthPXD(&(lrd_ptr->log.noredopage.pxd)),
		(long long) addressPXD(&(lrd_ptr->log.noredopage.pxd)),
		(long long) addressPXD(&(lrd_ptr->log.noredopage.pxd)));

	return (LOGDMP_OK);
}				/* end of disp_noredopage() */

/***************************************************************************
 *
 * NAME: disp_noredoinoext
 *
 * FUNCTION:
 *
 * PARAMETERS:  none
 *
 * NOTES:
 *
 * RETURNS:
 *      success: LOGDMP_OK
 *      failure: something else
 */
int disp_noredoinoext(struct lrd * lrd_ptr)
{
	fprintf(outfp, "fileset = d %d  \n", lrd_ptr->log.noredoinoext.fileset);

	fprintf(outfp, "\tiag number = d %d   extent index = d %d\n",
		lrd_ptr->log.noredoinoext.iagnum, lrd_ptr->log.noredoinoext.inoext_idx);

	fprintf(outfp, "\tpxd length = d %d   phys offset = x %llx  (d %lld)\n",
		lengthPXD(&(lrd_ptr->log.noredoinoext.pxd)),
		(long long) addressPXD(&(lrd_ptr->log.noredoinoext.pxd)),
		(long long) addressPXD(&(lrd_ptr->log.noredoinoext.pxd)));

	return (LOGDMP_OK);
}				/* end of disp_noredopage() */

/***************************************************************************
 *
 * NAME: disp_redopage
 *
 * FUNCTION:
 *
 * PARAMETERS:  none
 *
 * NOTES:
 *
 * RETURNS:
 *      success: LOGDMP_OK
 *      failure: something else
 */
int disp_redopage(struct lrd * lrd_ptr)
{
	fprintf(outfp, "fileset = d %d    inode = d %d (x %x)\n",
		lrd_ptr->log.redopage.fileset, lrd_ptr->log.redopage.inode,
		lrd_ptr->log.redopage.inode);

	switch (lrd_ptr->log.redopage.type) {
	case LOG_INODE:
		fprintf(outfp, "\ttype = d %d REDOPAGE:INODE\n", lrd_ptr->log.redopage.type);
		break;
	case LOG_XTREE:
		fprintf(outfp, "\ttype = d %d REDOPAGE:XTREE\n  ", lrd_ptr->log.redopage.type);
		break;
	case (LOG_XTREE | LOG_NEW):
		fprintf(outfp, "\ttype = d %d REDOPAGE:XTREE_NEW\n  ", lrd_ptr->log.redopage.type);
		break;
	case (LOG_BTROOT | LOG_XTREE):
		fprintf(outfp, "\ttype = d %d REDOPAGE:BTROOT_XTREE\n  ",
			lrd_ptr->log.redopage.type);
		break;
	case LOG_DTREE:
		fprintf(outfp, "\ttype = d %d REDOPAGE:DTREE\n  ", lrd_ptr->log.redopage.type);
		break;
	case (LOG_DTREE | LOG_NEW):
		fprintf(outfp, "\ttype = d %d REDOPAGE:DTREE_NEW \n ", lrd_ptr->log.redopage.type);
		break;
	case (LOG_DTREE | LOG_EXTEND):
		fprintf(outfp, "\ttype = d %d REDOPAGE:DTREE_EXTEND\n  ",
			lrd_ptr->log.redopage.type);
		break;
	case (LOG_BTROOT | LOG_DTREE):
		fprintf(outfp, "\ttype = d %d REDOPAGE:BTROOT_DTREE\n  ",
			lrd_ptr->log.redopage.type);
		break;
	case (LOG_BTROOT | LOG_DTREE | LOG_NEW):
		fprintf(outfp, "\ttype = d %d REDOPAGE:BTROOT_DTREE.NEW\n  ",
			lrd_ptr->log.redopage.type);
		break;
	case LOG_EA:
		fprintf(outfp, "\ttype = d %d REDOPAGE:EA\n", lrd_ptr->log.redopage.type);
		break;
	case LOG_ACL:
		fprintf(outfp, "\ttype = d %d REDOPAGE:ACL\n", lrd_ptr->log.redopage.type);
		break;
	case LOG_DATA:
		fprintf(outfp, "\ttype = d %d REDOPAGE:DATA\n", lrd_ptr->log.redopage.type);
		break;
/*
    case LOG_NOREDOFILE:
       fprintf( outfp, "\ttype = d %d REDOPAGE:NOREDOFILE\n",
                lrd_ptr->log.redopage.type );
       break;
*/
	default:
		fprintf(outfp, "\ttype = d %d ***UNRECOGNIZED***\n", lrd_ptr->log.redopage.type);
		break;
	}
	fprintf(outfp, "\tl2linesize = d %d    ", lrd_ptr->log.redopage.l2linesize);
	fprintf(outfp, "pxd length = d %d   phys offset = x %llx  (d %lld)\n",
		lengthPXD(&(lrd_ptr->log.redopage.pxd)),
		(long long) addressPXD(&(lrd_ptr->log.redopage.pxd)),
		(long long) addressPXD(&(lrd_ptr->log.redopage.pxd)));

	return (LOGDMP_OK);
}				/* end of disp_redopage() */

/***************************************************************************
 *
 * NAME: disp_updatemap
 *
 * FUNCTION:
 *
 * PARAMETERS:  none
 *
 * NOTES:
 *
 * RETURNS:
 *      success: LOGDMP_OK
 *      failure: something else
 */
int disp_updatemap(struct lrd * lrd_ptr)
{
	int flag_unrecognized = -1;
	fprintf(outfp, "fileset = d %d    inode = d %d (x %x)\n",
		lrd_ptr->log.updatemap.fileset, lrd_ptr->log.updatemap.inode,
		lrd_ptr->log.updatemap.inode);

	fprintf(outfp, "\ttype = x %x UPDATEMAP: ", lrd_ptr->log.updatemap.type);

	if ((lrd_ptr->log.updatemap.type & LOG_ALLOCXADLIST) == LOG_ALLOCXADLIST) {
		flag_unrecognized = 0;
		fprintf(outfp, " ALLOCXADLIST");
	}
	if ((lrd_ptr->log.updatemap.type & LOG_ALLOCPXDLIST) == LOG_ALLOCPXDLIST) {
		flag_unrecognized = 0;
		fprintf(outfp, " ALLOCPXDLIST");
	}
	if ((lrd_ptr->log.updatemap.type & LOG_ALLOCXAD) == LOG_ALLOCXAD) {
		flag_unrecognized = 0;
		fprintf(outfp, " ALLOCXAD");
	}
	if ((lrd_ptr->log.updatemap.type & LOG_ALLOCPXD) == LOG_ALLOCPXD) {
		flag_unrecognized = 0;
		fprintf(outfp, " ALLOCPXD");
	}
	if ((lrd_ptr->log.updatemap.type & LOG_FREEXADLIST) == LOG_FREEXADLIST) {
		flag_unrecognized = 0;
		fprintf(outfp, " FREEXADLIST");
	}
	if ((lrd_ptr->log.updatemap.type & LOG_FREEPXDLIST) == LOG_FREEPXDLIST) {
		flag_unrecognized = 0;
		fprintf(outfp, " FREEPXDLIST");
	}
	if ((lrd_ptr->log.updatemap.type & LOG_FREEXAD) == LOG_FREEXAD) {
		flag_unrecognized = 0;
		fprintf(outfp, " FREEXAD");
	}
	if ((lrd_ptr->log.updatemap.type & LOG_FREEPXD) == LOG_FREEPXD) {
		flag_unrecognized = 0;
		fprintf(outfp, " FREEPXD");
	}
	if (flag_unrecognized) {
		fprintf(outfp, " *** UNRECOGNIZED ***");
	}

	fprintf(outfp, "\n");

	fprintf(outfp, "\tnxd = d %d  (number of extents)\n", lrd_ptr->log.updatemap.nxd);
	fprintf(outfp, "\tpxd length = d %d   phys offset = x %llx  (d %lld)\n",
		lengthPXD(&(lrd_ptr->log.updatemap.pxd)),
		(long long) addressPXD(&(lrd_ptr->log.updatemap.pxd)),
		(long long) addressPXD(&(lrd_ptr->log.updatemap.pxd)));

	return (LOGDMP_OK);
}				/* end of disp_updatemap() */

/*****************************************************************************
 * NAME: open_outfile
 *
 * FUNCTION:  Open the output file.
 *
 * PARAMETERS:
 *      Device  - input - the device specification
 *
 * NOTES:
 *
 * RETURNS:
 *      success: XCHKLOG_OK
 *      failure: something else
 */
int open_outfile()
{
	int openof_rc = 0;

	outfp = fopen(output_filename, "w");

	if (outfp == NULL) {	/* output file open failed */
		printf("LOG_DUMP: unable to open output file: ./jfslog.dmp\n");
		openof_rc = -1;
	} else {
		logdmp_outfile_is_open = -1;
	}

	return (openof_rc);
}				/* end of open_outfile ( ) */
