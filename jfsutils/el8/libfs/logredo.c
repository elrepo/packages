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
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>

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
#include "logform.h"
#include "devices.h"
#include "debug.h"
#include "utilsubs.h"
#include "fsck_message.h"		/* for chkdsk message logging facility */

/* + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + +
 *
 *   L O C A L   M A C R O    D E F I N I T I O N S
 *
 */
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
 *    R E M E M B E R    M E M O R Y    A L L O C    F A I L U R E
 *
 */
int32_t Insuff_memory_for_maps = 0;
char *available_stg_addr = NULL;
int32_t available_stg_bytes = 0;
char *bmap_stg_addr = NULL;
int32_t bmap_stg_bytes = 0;

/* + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + +
 *
 *    S T U F F    F O R    T H E    L O G
 *
 */
struct logsuper logsup;		/* log super block */
int32_t numdoblk;		/* number of do blocks used     */
int32_t numnodofile;		/* number of nodo file blocks used  */
int32_t numExtDtPg = 0;		/* number of extended dtpage blocks used  */

/*
 *      open file system aggregate/lv array
 *
 * logredo() processes a single log.
 *
 * In the first release, logredo will process a single log which relates
 * to the single fileset in a single aggregate.  In some future release,
 * a single log may be used for multiple filesets which may or may not all
 * reside in the same aggregate.
 *
 */
struct vopen vopen[MAX_ACTIVE];
struct log_info Log;
struct {
	uuid_t uuid;
	FILE *fp;
} primary_vol;
extern int LogOpenMode;		/* logdump sets this to O_RDONLY */

/*
 * if this flag is set then the primary superblock is
 * corrupt.  The secondary superblock is good, but chkdsk
 * wasn't able to fix the primary version.  logredo can
 * run, but must use the secondary version of the
 * aggregate superblock
 */
int32_t use_2ndary_agg_superblock;
/*
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
int end_of_transaction = 0;

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
extern int doCommit(struct lrd *);
extern int doExtDtPg(void);
extern int doNoRedoFile(struct lrd *, uint32_t);
extern int doNoRedoPage(struct lrd *);
extern int doNoRedoInoExt(struct lrd *);
extern int doAfter(struct lrd *, int32_t);
extern int doUpdateMap(struct lrd *);
extern int alloc_wrksp(uint32_t, int, int, void **);

extern FILE * open_by_label(uuid_t, int, int, char *, int *);
extern char log_device[];
/*
 * forward references
 */
int doMount(struct lrd *);
int openVol(int32_t);
int updateSuper(int vol);
int rdwrSuper(FILE *, struct superblock *, int32_t);
int bflush(int32_t, struct bufpool *);
int logOpen(void);
int fsError(int, int, int64_t);
int logError(int, int);
static int recoverExtendFS(FILE *);
int alloc_storage(int32_t, void **, int32_t *);
int alloc_dmap_bitrec(struct dmap_bitmaps **);

/*
 * debug control
 */
#ifdef _JFS_DEBUG
int32_t dflag = 1;
time_t *Tp;
uint32_t tp_start, tp_end;
int xdump(char *, int);
int x_scmp(char *, char *);
void x_scpy(char *, char *);
int prtdesc(struct lrd *);
#else
int32_t dflag = 0;
#endif

/*
 * NAME:        jfs_logredo()
 *
 * FUNCTION:	Replay all transactions committed since the most
 *		recent synch point.
 *
 * NOTES:
 *	>>>>>> 	The log replay is accomplished in one pass over the
 *		log, reading backwards from logend to the first synch
 *		point record encountered.  This means that the log
 *		entries are read and processed in LIFO (Last-In-First-Out)
 *		order.  In other words, the records logged latest in
 *		time are the first records processed during log replay.
 *
 *	>>>>>> 	Inodes, index trees, and directory trees
 *
 *		Inodes, index tree structures, and directory tree
 *		structures are handled by processing committed redopage
 *		records which have not been superceded by noredo records.
 *		This processing copies data from the log record into the
 *		appropriate disk extent page(s).
 *
 *		To ensure that only the last (in time) updates to any
 *		given disk page are applied during log replay, logredo
 *		maintains a record (union structure summary1/summary2),
 *		for each disk page which it has processed, of which
 *		portions have been updated by log records encountered.
 *
 *	>>>>>> 	Inode Allocation Map processing

 *		The xtree for the Inode Allocation Map is journaled, and
 *		a careful write is used to update it during commit
 *		processing.
 * The imap index tree is also duplicated at the known location. (TBD)
 * So at logredo time, the xtree for imap is always readable and correct.
 * This is the basic requirement from logredo.
 *
 * the inode map control page (struct dinomap) is only flushed to disk at
 * the umount time. For iag, pmap will go to disk at commit time.
 * iagnum will not change in run-time.
 * agstart field will stable without extendfs utility. It is TBD for
 * how to handle agstart when extendfs utility is available.
 * Other fields ( wmap. inosmap, extsmap ino free list pointers,
 * ino ext free list pointers ) are at working status ( i.e they are
 * updated in run-time. So the following
 * meta-data of the imap need to be reconstructed at the logredo time:
 *  1) IAGs, the pmap of imap and inoext array are contained in IAGs.
 *  2) AG Free inode list
 *  3) AG Free Inode Extent list
 *  4) IAG Free list
 *
 * There are two imaps need to take care of :
 *   1) aggregate imap
 *   2) fileset imap
 * For the first release, the aggregate imap is stable and we only
 * need to deal with the fileset imap.
 *
 * Block Allocation Map (bmap file) is for an aggregate/lv. There are
 * three fields related to the size of bmap file.
 *  1) superblock.s_size: This field indicates aggregate size. It
 *                        tells number of sector-size blocks for this
 *                        aggregate. The size of aggregate determines
 *                        the size of its bmap file.
 *                        Since the aggregate's superblock is updated
 *                        using sync-write, superblock.s_size is trustable
 *                        at logredo time.
 *               note1:   mkfs reserves the fsck space. So s_size really
 *                        inidcate (size_of_aggregate - fsck_reserve_space)
 *               note2:   At the mkfs time, "-s" parameter could be used
 *                        to indicate how large the aggregate/filesystem is.
 *                        One lv contains at most one aggregate/filesystem.
 *                        If "-s" gives the value is smaller than the size
 *                        of lv, it is ok. The space is just wasted.
 *
 *                        Without "-s" parameter, mkfs wil use the whole
 *                        size of lv to make an aggregate/filesystem.
 *                        That is usually the case. So we can also say
 *                        an aggregate/lv. "-s" is often used for test.
 *
 *  2) dbmap.dn_mapsize: This field also indicates aggregate/lv size.
 *                        It tells number of aggre. blocks in the
 *                        aggregate/lv. Without extendfs, this field should
 *                        be equivalent to superblock.s_size.
 *                        With extendfs, this field may not be updated
 *                        before a system crash happens. So logredo
 *                        need to update it.
 *  3) dinode.di_size:  For an inode of bmap file, this field indicates
 *                        the logical size of the file. I.e. it contains
 *                        the offset value of the last byte written
 *                        in the file plus one.
 *                        So di_size will include the bmap control page,
 *                        the dmap control pages and dmap pages.
 *                        In the JFS, if a file is a sparse file, the logical
 *                        size is different from its physical size.
 *                        The bmap file is a sparse file if the total of
 *                        dmap pages is  ( < 1024) or ( < 1024 * 1024).
 *                        In that case, physically L1.0, and/or L2 does
 *                        not exist, but di_size will include their page
 *                        size.
 *
 *              Note:     The di_size does NOT contain the logical
 *                        structure of the file, i.e. the space allocated
 *                        for the xtree stuff is not indicated in di_size.
 *                        It is indicated in di_nblocks.
 *
 *                        In addition, the mkfs always put one more dmap
 *                        page into the bmap file for preparing extendfs.
 *                        This hidden dmap page cannot be figured out from
 *                        superblock.s_size, but di_size includes it. Any
 *                        dmapctl pages caused by this hidden dmap page
 *                        are also included in di_size.
 *
 * The bmap control page, dmap control pages and dmap pages are all
 * needed to rebuild at logredo time.
 *
 * In overall, the following actions are taken at logredo time:
 *   1) apply log rec data to the specified page.
 *   2) initialize freelist for dtree page or root.
 *   3) rebuilt imap
 *   4) rebuilt bmap
 *   in addition, in order to ensure the log record only applying to a
 *   certain portion of page one time, logredo will start NoRedoFile,
 *   NoRedoExtent/NoRedoPage filter in the process for accuracy and
 *   efficiency.
 *
 *  The three log rec types: REDOPAGE, NOREDOPAGE, NOREDOINOEXT, and
 *  UPDATEMAP, are the main force to initiate these actions.  See
 *  comments on doAfter(), updatePage(), doNoRedoPage(), doNoRedoInoExt,
 *  and doUpdateMap() for detailed information.
 *
 * If the aggregate/lv has state of FM_DIRTY, then fsck will run
 * after the logredo process since logredo could not get 100%
 * recovery. Currently bmap rebuild is slow ( 1 min per 32 GB),
 * so logredo will NOT rebuild imap and bmap if fsck will do it
 * anyway. But logredo still read maps in and mark them for starting
 * NoRedoExtent/NoRedoPage filter.
 *
 * The maps are rebuilt in the following way:
 * at the init phase, storage is allocated for the whole map file for
 * both imap and bmap. Reading in the map files from the disk.
 * The wmap is inited to zero. At the logredo time, the wmap is used
 * to track the bits in pmap. In the beginning of the logredo process
 * the allocation status of every block is in doubt. As log records
 * are processed, the allocation state is determined and the bit of pmap
 * is updated. This fact is recorded in the corresponding bits in wmap.
 * So a pmap bit is only updated once at logredo time and only updated
 * by the latest in time log record.
 * At the end of logredo, the control information, the freelist, etc.
 * are built from the value of pmap; then pmap is copied to wmap and
 * the whole map is written back to disk.
 *
 * the status field s_state in the superblock of each file-system is
 * set to FM_CLEAN provided the initial status was either FM_CLEAN
 * or FM_MOUNT and logredo processing was successful. If an error
 * is detected in logredo the status is set to FM_LOGREDO. the status
 * is not changed if its initial value was FM_MDIRTY. fsck should be
 * run to clean-up the probable damage if the status after logredo
 * is either FM_LOGREDO or FM_MDIRTY.
 *
 *  The log record has the format:
 *   <LogRecordData><LogRecLRD>
 *  At logredo time, the log is read backward. So for every log rec,
 *  we read LogRecLRD, which tells how long the LogRecordData is.
 *  see comments on updatePage() for detailed info of log record format.
 *
 *.....................................................................
 * The logredo handles the log-within-file-system (aka inline log) issue:
 *.....................................................................
 * For AIX, we always deal with the outline log, i.e. the log resides
 * in a separate logical volume. A log is associated with one volume
 * group and can be shared by many file systems with this volume group.
 * In AIX, the logredo received a device name. It then determines if
 * this device is a log name  or a filesystem name. If it is a filesustem
 * name, get the log minor number for this filesystem. If it is a log name,
 * get its minor number.
 *
 * XJFS decided to put log inside the file system
 *
 * For supporting the inline log, the above AIX logic should be changed.
 *
 * Here is the outline:
 *
 * When the logredo received a device name, it first read the SIZE_OF_SUPER
 * bytes from SUPER1_OFF  offset to see if it is a file system superblock.
 * If yes, check the s_flag to see if it has a inline log or outline log.
 * for an inline log the s_logdev should match the input device name's
 * major and minor number. If not, an error is returned and logredo exit.
 * If no error, the logredo read the log superblock according the log info
 * in the fs superblock.
 * If the device name does not represent a filesystem device, then logredo
 * read the LOGPSIZE bytes from the log page 1 location. If it indicates
 * a log device, then open the filesystems according to the log superblock's
 * active list. For each filesystem in the active list, read its superblock
 * if one of the superblock indicates that it uses an inline log, return
 * an error. It is a system code bug if some filesystems use inline log
 * and some use outline log.
 * If the superblock indicates it used an outline log, check the superblock's
 * s_logdev to match the input device name's major and minor numbers.
 * If one of them does not match, return error. -- It is a system code bug,
 * if some match and some not match; -- It should either match all or non of
 * them match. The AIX logredo never check s_logdev with the input log device.
 * We should check here.
 *
 * for outline log, logredo will be called once to cover all the file
 * systems in the log superblock's active list.
 * For inline log, logredo will be called many times. Each time is for
 * one file system. The log superblock's active list has nothing. The
 * logmajor and logminor contains file system's major and minor number.
 *
 *.....................................................................
 * logredo handles support EA:
 *.....................................................................
 * There is 16-byte EA descriptor which is located in the section I of
 * dinode.
 * The EA can be inline or outline. If it is inlineEA then the data will
 * occupy the section IV of the dinode. The dxd_t.flag will indicate so.
 * If it is outlineEA, dxd_t.flag will indicate so and the single extent
 * is described by EA descriptor.
 *
 * The section IV of dinode has 128 byte. It is shared by the xtroot and
 * inlineEA. The sharing is in FCFS style. If xtree gets the section IV,
 * xtree will never give it away even if xtree is shrink or split.
 * If inlineEA gets it, there is a chance that later inlineEA is freed and
 * so xtree still can get it.
 *
 * for outlineEA, the XJFS will syncly write the data portion out so there
 * is no log rec for the data, but there is still an INODE log rec for EA
 * descriptor changes and there is a UPDATEMAP log rec for the allocated
 * pxd. If an outlineEA is freed, there are also two log records for it:
 * one is INODE with EA descriptor zeroed out, another is the UPDATEMAP
 * log rec for the freed pxd.
 * For inlineEA, it has to be recorded in the log rec. It is not in a
 * separate log rec. Just one additional segment is added into the
 * INODE log rec. So an INODE log rec can have at most three segments:
 * when the parent and child inodes are in the same page, then there are
 * one segment for parent base inode; one segment for child base inode;
 * and maybe the third one for the child inlineEA data.
 *....................................................................
 * 32-bit vs 64-bit
 * At the first release. assume that a file system will not be larger
 * than 32-bit.
 *....................................................................
 * TBD:
 * the method for handling crashes in the middle of extending a file
 * system is as follows. the size of a filesystem is established from
 * the superblock.s_size field (i.e the sizes in the diskmap
 * and inodemaps are ignored). in extendfs (jfs_cntl.c) the superblock
 * is not updated before the maps have been extended and the new inodes
 * formatted to zeros. no allocations in the new part of the filesystem
 * occur prior to the change in map sizes. if a crash occurs just
 * before updating the superblock, the map sizes will be their old
 * values. in this case the maps as files may be bigger than necessary.
 * if the crash occurs just after writing the super block, the map sizes
 * are fixed up here.
 */
int jfs_logredo(caddr_t pathname, FILE *fp, int32_t use_2nd_aggSuper)
{
	int rc;
	int k, logaddr, nextaddr, lastaddr, nlogrecords;
	int syncrecord = 0;
	struct lrd ld;
	int lowest_lr_byte = 2 * LOGPSIZE + LOGPHDRSIZE;
	int highest_lr_byte = 0;
	int log_has_wrapped = 0;
	int logend;
	int in_use;

	/*
	 * store away the indicator of which aggregate superblock
	 * to use
	 */
	use_2ndary_agg_superblock = use_2nd_aggSuper;

	/*
	 * loop until we get enough memory to read vmount struct
	 */
	mntinfo = (char *) &bufsize;
	bufsize = sizeof (int);

	/*
	 * validate that the log is not currently in use;
	 */
	rc = findLog(fp, &in_use);
	if (rc < 0) {
		fsck_send_msg(lrdo_DEVOPNREADERROR);
		return (rc);
	}

	/* recover from extendfs() ? */
	if (Log.location & INLINELOG && (vopen[0].status & FM_EXTENDFS)) {
		fsck_send_msg(lrdo_REXTNDBEGIN);
		rc = recoverExtendFS(fp);
		fsck_send_msg(lrdo_REXTNDDONE);
		return rc;
	}

	/*
	 * validate log superblock
	 *
	 * aggregate block size is for log file as well.
	 */
	rc = ujfs_rw_diskblocks(Log.fp,
				(uint64_t) (Log.xaddr +
					    LOGPNTOB(LOGSUPER_B)),
				(unsigned) sizeof (struct logsuper), (char *) &logsup, GET);
	if (rc != 0) {
		fsck_send_msg(lrdo_CANTREADLOGSUP);
		rc = LOGSUPER_READ_ERROR;
		goto error_out;
	}
	ujfs_swap_logsuper(&logsup);

	if (logsup.magic != LOGMAGIC) {
		fsck_send_msg(lrdo_LOGSUPBADMGC);
		rc = NOT_LOG_FILE_ERROR;
		goto error_out;
	}

	if (logsup.version > LOGVERSION) {
		fsck_send_msg(lrdo_LOGSUPBADVER);
		rc = JFS_VERSION_ERROR;
		goto error_out;
	}

	if (Log.location & OUTLINELOG) {
		struct stat st;

		if ((rc = fstat(fileno(Log.fp), &st)))
			goto error_out;

		Log.devnum = st.st_rdev;

		if (in_use) {
			fsck_send_msg(lrdo_LOGINUSE);
			return LOG_IN_USE;
		}
	}

	if (logsup.state == LOGREDONE) {
		fsck_send_msg(lrdo_ALREADYREDONE);
		if (Log.location & INLINELOG)
			if ((rc = updateSuper(0)) != 0) {
				fsck_send_msg(lrdo_CANTUPDLOGSUP);
				return (rc);
			}
		return (0);
	}

	Log.size = logsup.size;
	Log.serial = logsup.serial;

	/*
	 * find the end of log
	 */
	logend = findEndOfLog();

	if (logend < 0) {
		fsck_send_msg(lrdo_LOGEND, logend);

		fsck_send_msg(lrdo_LOGENDBAD1);
		logError(LOGEND, 0);
		ujfs_swap_logsuper(&logsup);
		rc = ujfs_rw_diskblocks(Log.fp,
					(Log.xaddr + LOGPNTOB(LOGSUPER_B)),
					(unsigned long) LOGPSIZE, (char *) &logsup, PUT);
		rc = logend;
		goto error_out;
	}

	/*
	 * allocate/initialize logredo runtime data structures and
	 * initialize each file system associated with the log based on
	 * the contents of its superblock
	 */
	if ((rc = logredoInit()) != 0) {
		fsck_send_msg(lrdo_INITFAILED, rc, errno);
		goto error_out;
	}

	highest_lr_byte = logsup.size * LOGPSIZE - LOGRDSIZE;

	if ((logend < lowest_lr_byte) || (logend > highest_lr_byte)) {
		fsck_send_msg(lrdo_LOGEND, logend);

		fsck_send_msg(lrdo_LOGENDBAD2);
		rc = INVALID_LOGEND;
		goto error_out;
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
		DBG_TRACE(("Logaddr=%x\nNextaddr=%x\n", logaddr, nextaddr))
		    nlogrecords += 1;
		/*
		 *
		 * Validate the nextaddr as much as possible
		 *
		 */
		if (nextaddr < 0) {
			fsck_send_msg(lrdo_NEXTADDRINVALID);
			rc = nextaddr;
			goto error_out;
		}

		if ((nextaddr < lowest_lr_byte)
		    || (nextaddr > highest_lr_byte)) {
			fsck_send_msg(lrdo_NEXTADDROUTRANGE, nextaddr);
			rc = INVALID_NEXTADDR;
			goto error_out;
		}

		if (nextaddr == logaddr) {
			fsck_send_msg(lrdo_NEXTADDRSAME, nextaddr);
			rc = NEXTADDR_SAME;
			goto error_out;
		}

		if (nextaddr > logaddr) {
			if (log_has_wrapped) {
				fsck_send_msg(lrdo_LOGWRAPPED);
				rc = LOG_WRAPPED_TWICE;
				goto error_out;
			} else {
				log_has_wrapped = -1;
			}
		}
		/*
		 *
		 * The addresses seem ok.  Process the current record.
		 *
		 */
		switch (ld.type) {
		case LOG_COMMIT:
			rc = doCommit(&ld);
			if (rc) {
				fsck_send_msg(lrdo_BADCOMMIT, logaddr);

				goto error_out;
			}
			break;
		case LOG_MOUNT:
			fsck_send_msg(lrdo_MOUNTRECORD, logaddr);

			rc = doMount(&ld);
			if (rc) {
				fsck_send_msg(lrdo_BADMOUNT, logaddr);

				goto error_out;
			}
			break;

		case LOG_SYNCPT:
			fsck_send_msg(lrdo_SYNCRECORD, logaddr);

			rc = 0;
			if (lastaddr == 0) {
				syncrecord = logaddr;
				lastaddr = (ld.log.syncpt.sync == 0)
				    ? logaddr : ld.log.syncpt.sync;
			}
			break;

		case LOG_REDOPAGE:
			DBG_TRACE(("jfs_logredo:Case Log_redoPage"))
			    rc = doAfter(&ld, logaddr);
			if (rc) {
				fsck_send_msg(lrdo_BADREDOPAGE, logaddr);
				goto error_out;
			}
			break;

		case LOG_NOREDOPAGE:
			DBG_TRACE(("jfs_logredo:Case Log_noredopage"))
			    rc = doNoRedoPage(&ld);
			if (rc) {
				fsck_send_msg(lrdo_BADNOREDOPAGE, logaddr);
				goto error_out;
			}
			break;

		case LOG_NOREDOINOEXT:
			DBG_TRACE(("jfs_logredo:Case Log_noredoinoext"))
			    rc = doNoRedoInoExt(&ld);
			if (rc) {
				fsck_send_msg(lrdo_BADNOREDOINOEXT, logaddr);
				goto error_out;
			}
			break;

		case LOG_UPDATEMAP:
			rc = doUpdateMap(&ld);
			if (rc) {
				fsck_send_msg(lrdo_BADUPDATEMAP, logaddr);
				goto error_out;
			}
			break;

		default:
			fsck_send_msg(lrdo_UNKNOWNTYPE, logaddr);
			rc = UNRECOG_LOGRECTYP;
			goto error_out;
			break;
		}

		if (rc < 0) {
			fsck_send_msg(lrdo_ERRORNEEDREFORMAT);
			goto error_out;
		}

		if (rc != 0) {
			fsck_send_msg(lrdo_ERRORCANTCONTIN);
			goto error_out;
		}

		/*
		 * If the transaction just completed was the last
		 * for the current transaction, then flush the
		 * buffers.
		 */
		if (end_of_transaction != 0) {
			for (k = 1; k < NBUFPOOL; k++) {
				if ((rc = bflush(k, &buffer[k - 1])) != 0)
					goto error_out;
			}
			end_of_transaction = 0;
		}

	} while (logaddr != lastaddr);
	/*
	 * If any 'dtpage extend' records were processed, then we need
	 * to go back and rebuild their freelists.  This cannot be done
	 * when the 'dtpage extend' record is processed, since there may
	 * be records processed later which affect the previous (shorter)
	 * version of the dtpage.  Only after all these records are processed
	 * can we safely and accurately rebuild the freelist.
	 */
	if (numExtDtPg != 0) {
		rc = doExtDtPg();
	}

	/*
	 * flush data page buffer cache
	 */
	for (k = 1; k < NBUFPOOL; k++) {
		if ((rc = bflush(k, &buffer[k - 1])) != 0)
			break;
	}

	/*
	 *      finalize file systems
	 *
	 * update allocation map and superblock of file systems
	 * of volumes which are open if they were modified here.
	 * i.e. if they were not previously unmounted cleanly.
	 */
	for (k = 0; k < MAX_ACTIVE; k++) {
		if (vopen[k].state != VOPEN_OPEN)
			continue;

		if ((rc = updateMaps(k)) != 0) {
			fsck_send_msg(lrdo_ERRORCANTUPDMAPS);
			goto error_out;
		}

		/* Make sure all changes are committed to disk before we
		 * mark the superblock clean
		 */
		ujfs_flush_dev(vopen[k].fp);

		if ((rc = updateSuper(k)) != 0) {
			fsck_send_msg(lrdo_ERRORCANTUPDFSSUPER);
			goto error_out;
		}

		/* sync superblock before journal is finalized */
		ujfs_flush_dev(vopen[k].fp);
	}

	/*
	 *      finalize log.
	 *
	 * clear active list.
	 * If this is a fully replayed log then it can be moved to earlier
	 * versions of the operating system.  Therefore switch the magic
	 * number to the earliest level.
	 */
	if (logsup.state != LOGREADERR) {
		for (k = 0; k < MAX_ACTIVE; k++)
			uuid_clear(logsup.active[k]);

		logsup.end = logend;
		logsup.state = LOGREDONE;
		logsup.magic = LOGMAGIC;
	}
	ujfs_swap_logsuper(&logsup);
	rc = ujfs_rw_diskblocks(Log.fp, (Log.xaddr + LOGPNTOB(LOGSUPER_B)),
				LOGPSIZE, (char *) &logsup, PUT);

	/*
	 * now log some info for the curious
	 */
	fsck_send_msg(lrdo_LOGEND, logend);

	fsck_send_msg(lrdo_RPTSYNCNUM, syncrecord);

	fsck_send_msg(lrdo_RPTSYNCADDR, lastaddr);

	fsck_send_msg(lrdo_RPTNUMLOGREC, nlogrecords);

	fsck_send_msg(lrdo_RPTNUMDOBLK, numdoblk);

	fsck_send_msg(lrdo_RPTNUMNODOBLK, numnodofile);

      error_out:

	if (rc > 0) {
		rc = rc * (-1);
	}

	/*
	 * If everything went ok except that we didn't have
	 * enough memory to deal with the block map, tell chkdsk
	 * to be sure to do a full check and repair, but that a log
	 * format is not necessary
	 */
	if ((rc == 0) && Insuff_memory_for_maps) {
		rc = ENOMEM25;
	}

	return (rc);
}

/*
 * NAME:        doMount(ld)
 *
 * FUNCTION:    a log mount record is the first-in-time record which is
 *              put in the log so it is the last we want to process in
 *              logredo. so we mark volume as cleanly unmounted in vopen
 *              array. the mount record is imperative when the volume
 *              is a newly made filesystem.
 */
int doMount(struct lrd *ld)
{				/* pointer to record descriptor */
	int vol, status;

	vol = ld->aggregate;

	status = vopen[vol].status;
	DBG_TRACE(("Logredo:domount: status=%d\n", status))

	    if (!(status & (FM_LOGREDO | FM_DIRTY)))
		vopen[vol].status = FM_CLEAN;

	return (0);
}

/*
 * NAME:        openVol(vol)
 *
 * FUNCTION:    open the aggregate/volume specified.
 *              check if it was cleanly unmounted. also check log
 *              serial number. initialize disk and inode mpas.
 */
int openVol(int vol)
{				/* device minor number of aggregate/lv */
	int rc, l2agsize, agsize;
	int64_t fssize;		/* number of aggr blks in the aggregate/lv */
	struct superblock sb;
	int aggsb_numpages;

	if (Log.location & OUTLINELOG) {
		/* First check if this is the already opened volume */
		if (!uuid_compare(vopen[vol].uuid, primary_vol.uuid))
			vopen[vol].fp = primary_vol.fp;
		else {
			vopen[vol].fp = open_by_label(vopen[vol].uuid, 0, 0,
						      NULL, NULL);
			if (vopen[vol].fp == NULL)
				return ENOENT;
		}
	}

	/* read superblock of the aggregate/volume */
	if ((rc = rdwrSuper(vopen[vol].fp, &sb, PB_READ)) != 0) {
		fsck_send_msg(lrdo_CANTREADFSSUPER);

		fsError(READERR, vol, SUPER1_B);
		vopen[vol].state = VOPEN_CLOSED;
		return (FSSUPER_READERROR1);
	}

	/* check magic number and initialize version specific
	 * values in the vopen struct for this vol.
	 */
	if (strncmp(sb.s_magic, JFS_MAGIC, (unsigned) strlen(JFS_MAGIC))) {
		fsck_send_msg(lrdo_FSSUPERBADMAGIC);
		vopen[vol].state = VOPEN_CLOSED;
		return (LOGSUPER_BADMAGIC);
	}
	if (sb.s_version > JFS_VERSION) {
		fsck_send_msg(lrdo_FSSUPERBADMAGIC);
		vopen[vol].state = VOPEN_CLOSED;
		return (LOGSUPER_BADVERSION);
	}

	if (Log.location & OUTLINELOG && (sb.s_flag & (JFS_INLINELOG == JFS_INLINELOG))) {
		fsck_send_msg(lrdo_FSSUPERBADLOGLOC);
		vopen[vol].state = VOPEN_CLOSED;
		return (LOGSUPER_BADLOGLOC);
	}
	vopen[vol].lblksize = sb.s_bsize;
	vopen[vol].l2bsize = sb.s_l2bsize;
	vopen[vol].l2bfactor = sb.s_l2bfactor;
	fssize = sb.s_size >> sb.s_l2bfactor;
	vopen[vol].fssize = fssize;
	vopen[vol].agsize = sb.s_agsize;
	/* LOG2NUM will alter agsize, so use local var (Then why don't we
	   fix LOG2NUM?) */
	agsize = vopen[vol].agsize;
	LOG2NUM(agsize, l2agsize);
	vopen[vol].numag = fssize >> l2agsize;
	if (fssize & (vopen[vol].agsize - 1))
		vopen[vol].numag += 1;
	vopen[vol].l2agsize = l2agsize;

	if (Log.location & INLINELOG) {
		/*
		 * Now that the aggregate superblock has been read, do some
		 * more validation of the log superblock
		 */
		if (logsup.bsize != vopen[vol].lblksize) {
			fsck_send_msg(lrdo_LOGSUPBADBLKSZ);
			return JFS_BLKSIZE_ERROR;
		}

		if (logsup.l2bsize != vopen[vol].l2bsize) {
			fsck_send_msg(lrdo_LOGSUPBADL2BLKSZ);
			return JFS_L2BLKSIZE_ERROR;
		}

		aggsb_numpages = lengthPXD(&sb.s_logpxd) * logsup.bsize / LOGPSIZE;
		if (logsup.size != aggsb_numpages) {
			fsck_send_msg(lrdo_LOGSUPBADLOGSZ);
			return JFS_LOGSIZE_ERROR;
		}
	}
	/*
	 *set lbperpage in vopen.
	 */
	vopen[vol].lbperpage = PSIZE >> vopen[vol].l2bsize;

	/*
	 * was it cleanly umounted ?
	 */
	if (sb.s_state == FM_CLEAN) {
		vopen[vol].status = FM_CLEAN;
		vopen[vol].state = VOPEN_CLOSED;
		return (0);
	}

	/*
	 * get status of volume
	 */
	vopen[vol].status = sb.s_state;
	vopen[vol].is_fsdirty = (sb.s_state & FM_DIRTY);

	/*
	 *check log serial number
	 */
	if (sb.s_logserial != Log.serial) {
		fsck_send_msg(lrdo_FSSUPERBADLOGSER);
		vopen[vol].state = VOPEN_CLOSED;
		fsError(SERIALNO, vol, SUPER1_B);
		return (LOGSUPER_BADSERIAL);
	}

	/* initialize the disk and inode maps
	 */
	if ((rc = initMaps(vol)) != 0) {
		fsck_send_msg(lrdo_INITMAPSFAIL);
		fsError(MAPERR, vol, 0);
		return (rc);
	}
	vopen[vol].state = VOPEN_OPEN;
	return 0;
}

/*
 * NAME:         updateSuper(vol)
 *
 * FUNCTION:     updates primary aggregate/lv's superblock status and
 *               writes it out.
 */
int updateSuper(int vol)
{				/* device minor number of aggregate/lv */
	int rc, status;
	struct superblock sb;

	/* read in superblock of the volume */
	if ((rc = rdwrSuper(vopen[vol].fp, &sb, PB_READ)) != 0) {
		fsck_send_msg(lrdo_READFSSUPERFAIL);
		return (FSSUPER_READERROR2);
	}

	/* mark superblock state. write it out */
	status = vopen[vol].status;
	if (status & (FM_DIRTY | FM_LOGREDO))
		sb.s_state = status & ~FM_EXTENDFS;
	else
		sb.s_state = FM_CLEAN;

	if ((rc = rdwrSuper(vopen[vol].fp, &sb, PB_UPDATE)) != 0) {
		fsck_send_msg(lrdo_WRITEFSSUPERFAIL);
	}

	return (rc);
}

/*
 * NAME:        rdwrSuper(fp, sb, rwflag)
 *
 * FUNCTION:    read or write the superblock for the file system described
 *              by the file descriptor of the opened aggregate/lv.
 *              for read, if a read of primary superblock is failed,
 *              try to read the secondary superblock. report error only
 *              when both reads failed.
 *              for write, any write failure should be reported.
 */
int rdwrSuper(FILE *fp, struct superblock * sb, int32_t rwflag)
{
	int rc;
	uint64_t super_offset;
	union {
		struct superblock super;
		char block[PSIZE];
	} super;

	if (use_2ndary_agg_superblock) {
		super_offset = SUPER2_OFF;
	} else {
		super_offset = SUPER1_OFF;
	}
	/*
	 * seek to the postion of the primary superblock.
	 * since at this time we don't know the aggregate/lv
	 * logical block size yet, we have to use the fixed
	 * byte offset address super_offset to seek for.
	 */

	/*
	 * read super block
	 */
	if (rwflag == PB_READ) {
		rc = ujfs_rw_diskblocks(fp, super_offset,
					(unsigned) SIZE_OF_SUPER, super.block, GET);
		if (rc != 0) {
			if (!use_2ndary_agg_superblock) {
				fsck_send_msg(lrdo_READFSPRIMSBFAIL);
				return (CANTREAD_PRIMFSSUPER);
			} else {
				fsck_send_msg(lrdo_READFS2NDSBFAIL);
				return (CANTREAD_2NDFSSUPER);
			}
		}

		*sb = super.super;

		ujfs_swap_superblock(sb);

		/*
		 * write superblock
		 */
	} else {		/* PB_UPDATE */
		/* ? memset(super.block, 0, SIZE_OF_SUPER); */
		super.super = *sb;

		ujfs_swap_superblock(&super.super);

		/*
		 * write whichever superblock we're working with.
		 * chkdsk will take care of replicating it.
		 */
		rc = ujfs_rw_diskblocks(fp, super_offset,
					(unsigned) SIZE_OF_SUPER, super.block, PUT);
		if (rc != 0) {
			if (!use_2ndary_agg_superblock) {
				fsck_send_msg(lrdo_WRITEFSPRIMSBFAIL);
				return (CANTWRITE_PRIMFSSUPER);
			} else {
				fsck_send_msg(lrdo_WRITEFS2NDSBFAIL);
				return (CANTWRITE_2NDFSSUPER);
			}
		}
	}

	return (0);
}

/*
 * NAME:        bflush()
 *
 * FUNCTION:    write out appropriate portion of buffer page if its modified.
 *              Note that a dtree page may not be 4k, depending on the length
 *              field specified in pxd. Write out only length that is needed.
 */
int bflush(int32_t k,		/*  The index in bufhdr that describes buf */
	   struct bufpool *buf)
{				/* pointer to buffer pool page */
	FILE *fp = NULL;
	int rc;
	int32_t vol;
	int32_t nbytes;
	int64_t blkno;

	/* nothing to do ? */
	if (bufhdr[k].modify == 0)
		return (0);

	/* write it out */
	vol = bufhdr[k].vol;
	fp = vopen[vol].fp;
	blkno = addressPXD(&bufhdr[k].pxd);
	nbytes = lengthPXD(&bufhdr[k].pxd) << vopen[vol].l2bsize;
	rc = ujfs_rw_diskblocks(fp,
				(uint64_t) (blkno << vopen[vol].l2bsize),
				(unsigned) nbytes, (char *) buf, PUT);
	if (rc != 0) {
		fsck_send_msg(lrdo_BUFFLUSHFAIL);
		return (BFLUSH_WRITEERROR);
	}

	bufhdr[k].modify = 0;

	return (0);
}

/*
 * NAME:        findLog()
 *
 * FUNCTION:    open the device to see if it's a valid filesystem
 * 		or journal.  If it is a filesystem, determine whether
 * 		the log is inline or external.  If external, find
 * 		the log device.
 *
 */
int findLog(FILE *fp, int *in_use)
{
	struct logsuper logsup;
	struct superblock sb;

	*in_use = 0;
	/*
	 * try the LV as file system with in-line log
	 */
	if (rdwrSuper(fp, &sb, PB_READ)) {
		fsck_send_msg(lrdo_NOTAFSDEV);
		return NOT_FSDEV_ERROR;
	}

	/*
	 * is the LV a file system ?
	 */
	if (memcmp(sb.s_magic, JFS_MAGIC, sizeof (sb.s_magic)) == 0) {
		/*
		 * does file system contains its in-line log ?
		 */
		if ((sb.s_flag & JFS_INLINELOG) == JFS_INLINELOG) {
			Log.location = INLINELOG;
			Log.fp = fp;
			//Log.status = sb.s_state;
			Log.l2bsize = sb.s_l2bsize;
			Log.xaddr = addressPXD(&sb.s_logpxd) << sb.s_l2bsize;

			/* vopen[0] represents fs if inline log */
			vopen[0].status = sb.s_state;
			vopen[0].fp = fp;

			return 0;
		}
		/* Save fp and uuid */
		primary_vol.fp = fp;
		uuid_copy(primary_vol.uuid, sb.s_uuid);

		/*
		 * External log
		 *
		 * First check device specified on
		 * command line
		 */
		Log.xaddr = 0;
		if (log_device[0]) {
			Log.fp = NULL;
			if (LogOpenMode != O_RDONLY) {
				Log.fp = fopen_excl(log_device, "r+");
				if (Log.fp == NULL)
					*in_use = 1;
			}
			if (Log.fp == NULL) {
				Log.fp = fopen(log_device, "r");
				if (Log.fp == NULL) {
					printf("Invalid journal specified (%s)\n",
					       log_device);
					goto by_uuid;
				}
			}
			ujfs_rw_diskblocks(Log.fp, LOGPNTOB(LOGSUPER_B),
					   sizeof (struct logsuper), &logsup, GET);
			ujfs_swap_logsuper(&logsup);
			if ((logsup.magic != LOGMAGIC) || (uuid_compare(logsup.uuid, sb.s_loguuid))) {
				fclose(Log.fp);
				*in_use = 0;
				goto by_uuid;
			}
			Log.location = OUTLINELOG;
			return 0;
		}
	      by_uuid:
		Log.fp = open_by_label(sb.s_loguuid, 0, 1, NULL, in_use);

		if (Log.fp != NULL) {
			Log.location |= OUTLINELOG;
			return 0;
		}

		return NOT_INLINELOG_ERROR;
	}
	/*
	 * is this an external log?
	 */
	ujfs_rw_diskblocks(fp, LOGPNTOB(LOGSUPER_B), sizeof (struct logsuper), &logsup, GET);
	ujfs_swap_logsuper(&logsup);
	if (logsup.magic != LOGMAGIC) {
		fsck_send_msg(lrdo_NOTAFSDEV);
		return NOT_FSDEV_ERROR;
	}
	Log.fp = fp;
	Log.location = OUTLINELOG;

	return 0;
}

extern void exit(int);

/*
 * NAME:        fsError(type,vol,bn)
 *
 * FUNCTION:    error handling code for the specified
 *              aggregate/lv (filesystem).
 */
int fsError(int type,		/* error types */
	    int vol,		/* the minor number of the aggregate/lv */
	    int64_t bn)
{				/* aggregate block No.  */

	fsck_send_msg(lrdo_ERRORONVOL, vol);

	retcode = -1;
	vopen[vol].status = FM_LOGREDO;

	switch (type) {
	case OPENERR:
		fsck_send_msg(lrdo_OPENFAILED);
		break;
	case MAPERR:
		fsck_send_msg(lrdo_CANTINITMAPS);
		break;
	case DBTYPE:
		fsck_send_msg(lrdo_BADDISKBLKNUM, (long long) bn);
		break;
	case INOTYPE:
		fsck_send_msg(lrdo_BADINODENUM, (long long) bn);
		break;
	case READERR:
		fsck_send_msg(lrdo_CANTREADBLK, (long long) bn);
		break;
	case SERIALNO:
		fsck_send_msg(lrdo_BADLOGSER);
		break;
	case IOERROR:
		fsck_send_msg(lrdo_IOERRREADINGBLK, (long long) bn);
		break;
	case LOGRCERR:
		fsck_send_msg(lrdo_BADUPDMAPREC, (long long) bn);
		break;
	}
	return (0);
}

/*
 *      logError(type)
 *
 * error handling for log read errors.
 */
int logError(int type, int logaddr)
{
	int k;
	retcode = -1;
	logsup.state = LOGREADERR;
	switch (type) {
	case LOGEND:
		fsck_send_msg(lrdo_FINDLOGENDFAIL);
		break;
	case READERR:
		fsck_send_msg(lrdo_LOGREADFAIL, logaddr);
		break;
	case UNKNOWNR:
		fsck_send_msg(lrdo_UNRECOGTYPE, logaddr);
		break;
	case IOERROR:
		fsck_send_msg(lrdo_IOERRONLOG, logaddr);
		break;
	case LOGWRAP:
		fsck_send_msg(lrdo_LOGWRAP);
	}

	/* mark all open volumes in error
	 */
	for (k = 0; k < MAX_ACTIVE; k++) {
		if ((vopen[k].state == VOPEN_OPEN) && vopen[k].status != FM_CLEAN)
			vopen[k].status = FM_LOGREDO;
	}
	return (0);
}

/*
 *	recoverExtendFS()
 *
 * function: recover crash while in extendfs() for inline log;
 *
 * note: fs superblock fields remains pre-extendfs state,
 * while that bmap file, fsck and inline log area may be in
 * unknown state;
 *
 * at entry, only log type/lv has been validated;
 * for inline log: vopen[0], fs fp = log fp;
 */
static int recoverExtendFS(FILE *fp)
{
	struct superblock *sbp;
	struct dinode *dip1, *dip2;
	struct dbmap *bgcp;
	xtpage_t *p;
	int64_t lmchild = 0, xaddr, xoff, barrier, t64, agsize;
	uint8_t lmxflag;
	int32_t i;
	char *dip, *bp;
	pxd_t temp_pxd;

	/*
	 * read bmap global control page
	 */
	/* read superblock yet again */
	sbp = (struct superblock *) &buffer[0];
	if (rdwrSuper(fp, sbp, PB_READ))
		goto errout;

	/* read primary block allocation map inode */
	dip = (char *) &buffer[1];
	if (ujfs_rw_diskblocks(fp, AITBL_OFF, PSIZE, dip, GET)) {
		fsck_send_msg(lrdo_EXTFSREADFSSUPERFAIL);
		goto errout;
	}

	/* locate the inode in the buffer page */
	dip1 = (struct dinode *) dip;
	dip1 += BMAP_I;

	bp = (char *) &buffer[2];	/* utility buffer */

	/* start from root in dinode */
	p = (xtpage_t *) & dip1->di_btroot;
	/* is this page leaf ? */
	if (p->header.flag & BT_LEAF)
		goto rdbgcp;

	/* traverse down leftmost child node to leftmost leaf of xtree */
	do {
		/* read in the leftmost child page */
		t64 = addressXAD(&p->xad[XTENTRYSTART]) << sbp->s_l2bsize;
		if (ujfs_rw_diskblocks(fp, t64, PSIZE, bp, GET)) {
			fsck_send_msg(lrdo_EXTFSREADBLKMAPINOFAIL);
			goto errout;
		}

		p = (xtpage_t *) bp;
		/* is this page leaf ? */
		if (p->header.flag & BT_LEAF)
			break;
	} while (1);

      rdbgcp:
	t64 = addressXAD(&p->xad[XTENTRYSTART]) << sbp->s_l2bsize;
	if (ujfs_rw_diskblocks(fp, t64, PSIZE, bp, GET)) {
		fsck_send_msg(lrdo_EXTFSREADBLKFAIL1, (long long) t64);
		goto errout;
	}
	bgcp = (struct dbmap *) bp;

	/*
	 * recover to pre- or post-extendfs state ?:
	 */
	if (__le64_to_cpu(bgcp->dn_mapsize) > (sbp->s_size >> sbp->s_l2bfactor)) {
		agsize = __le64_to_cpu(bgcp->dn_agsize);
		goto postx;
	}

	/*
	 *    recover pre-extendfs state
	 */
	/*
	 * reset block allocation map inode (xtree root)
	 */
	/* read 2ndary block allocation map inode */
	t64 = addressPXD(&sbp->s_ait2) << sbp->s_l2bsize;
	if (ujfs_rw_diskblocks(fp, t64, PSIZE, bp, GET)) {
		fsck_send_msg(lrdo_EXTFSREADBLKFAIL2, (long long) t64);
		goto errout;
	}
	dip2 = (struct dinode *) bp;
	dip2 += BMAP_I;

	/*
	 * Reset primary bam inode with 2ndary bam inode
	 *
	 * Not forgetting to reset di_ixpxd since they are in different
	 * inode extents.
	 */
	memcpy((void *) &temp_pxd, (void *) &(dip1->di_ixpxd), sizeof (pxd_t));
	memcpy(dip1, dip2, DISIZE);
	memcpy((void *) &(dip1->di_ixpxd), (void *) &temp_pxd, sizeof (pxd_t));

	if (ujfs_rw_diskblocks(fp, AITBL_OFF, PSIZE, dip, PUT)) {
		fsck_send_msg(lrdo_EXTFSWRITEBLKFAIL1, AITBL_OFF);
		goto errout;
	}

	/*
	 * backout bmap file to fs size:
	 *
	 * trim xtree to range specified by i_size:
	 * xtree has been grown in append mode and
	 * written from right to left, bottom-up;
	 */
	barrier = __le64_to_cpu(dip1->di_size) >> sbp->s_l2bsize;

	/* start with root */
	xaddr = 0;
	p = (xtpage_t *) & dip1->di_btroot;
	lmxflag = p->header.flag;
	p->header.next = 0;
	if (lmxflag & BT_INTERNAL) {
		/* save leftmost child xtpage xaddr */
		lmchild = addressXAD(&p->xad[XTENTRYSTART]);
	}

	/*
	 * scan each level of xtree via leftmost descend
	 */
	while (1) {
		/*
		 * scan each xtpage of current level of xtree
		 */
		while (1) {
			/*
			 * scan each xad in current xtpage
			 */
			for (i = XTENTRYSTART; i < p->header.nextindex; i++) {
				/* test if extent is of interest */
				xoff = offsetXAD(&p->xad[i]);
				if (xoff < barrier)
					continue;

				/*
				 * barrier met in current page
				 */
				assert(i > XTENTRYSTART);
				/* update current page */
				p->header.nextindex = i;
				if (xaddr) {
					/* discard further right sibling
					 * pages
					 */
					p->header.next = 0;
					if (ujfs_rw_diskblocks(fp, t64, PSIZE, p, PUT)) {
						fsck_send_msg(lrdo_EXTFSWRITEBLKFAIL2, (long long) t64);
						goto errout;
					}
				}

				goto nextLevel;
			}	/* end for current xtpage scan */

			/* barrier was not met in current page */

			/* read in next/right sibling xtpage */
			xaddr = p->header.next;
			if (xaddr) {
				if (xaddr >= barrier) {
					p->header.next = 0;
					if (ujfs_rw_diskblocks(fp, t64, PSIZE, p, PUT)) {
						fsck_send_msg(lrdo_EXTFSWRITEBLKFAIL3, (long long) t64);
						break;
					}
				}

				t64 = xaddr << sbp->s_l2bsize;
				if (ujfs_rw_diskblocks(fp, t64, PSIZE, bp, GET)) {
					fsck_send_msg(lrdo_EXTFSREADBLKFAIL3, (long long) t64);
					goto errout;
				}

				p = (xtpage_t *) bp;
			} else
				break;
		}		/* end while current level scan */

		/*
		 * descend: read leftmost xtpage of next lower level of xtree
		 */
	      nextLevel:
		if (lmxflag & BT_INTERNAL) {
			/* get the leftmost child page  */
			xaddr = lmchild;
			t64 = xaddr << sbp->s_l2bsize;
			if (ujfs_rw_diskblocks(fp, t64, PSIZE, bp, GET)) {
				fsck_send_msg(lrdo_EXTFSREADBLKFAIL4, (long long) t64);
				goto errout;
			}

			p = (xtpage_t *) bp;

			lmxflag = p->header.flag;
			if (lmxflag & BT_INTERNAL) {
				/* save leftmost child xtpage xaddr */
				lmchild = addressXAD(&p->xad[XTENTRYSTART]);
			}
		} else
			break;
	}			/* end while level scan */

	/*
	 * reconstruct map;
	 *
	 * readBmap() init blocks beyond fs size in the last
	 * partial dmap page as allocated which might have been
	 * marked as free by extendfs();
	 */
	/* fake log opend/validated */
	Log.serial = sbp->s_logserial;

	/*
	 *  reconstruct maps
	 */
	/* open LV and initialize maps  */
	if (logredoInit()) {
		fsck_send_msg(lrdo_EXTFSINITLOGREDOFAIL);
		goto errout;
	}

	/* bypass log replay */

	/* update/write maps */
	updateMaps(0);

	/*
	 * reformat log
	 *
	 * request reformat original log  (which might have been
	 * overwritten by extendfs() and set superblock clean
	 */
	jfs_logform(fp, sbp->s_bsize, sbp->s_l2bsize, sbp->s_flag,
		    addressPXD(&sbp->s_logpxd), lengthPXD(&sbp->s_logpxd), NULL, NULL);

	/* update superblock */
	updateSuper(0);

	fsck_send_msg(lrdo_REXTNDTOPRE);

	return 0;

	/*
	 *    recover post-extendfs state
	 */
      postx:
	/*
	 * update 2ndary bam inode
	 */
	/* read 2ndary block allocation map inode */
	t64 = addressPXD(&sbp->s_ait2) << sbp->s_l2bsize;
	if (ujfs_rw_diskblocks(fp, t64, PSIZE, bp, GET)) {
		fsck_send_msg(lrdo_EXTFSREADBLKFAIL5, (long long) t64);
		goto errout;
	}
	dip2 = (struct dinode *) bp;
	dip2 += BMAP_I;

	/*
	 * Reset 2ndary bam inode with primary bam inode
	 * Not forgetting to reset di_ixpxd since they are in different
	 * inode extents.
	 */
	memcpy((void *) &temp_pxd, (void *) &(dip2->di_ixpxd), sizeof (pxd_t));
	memcpy(dip2, dip1, DISIZE);
	memcpy((void *) &(dip2->di_ixpxd), (void *) &temp_pxd, sizeof (pxd_t));

	if (ujfs_rw_diskblocks(fp, t64, PSIZE, bp, PUT)) {
		fsck_send_msg(lrdo_EXTFSWRITEBLKFAIL4, (long long) t64);
		goto errout;
	}

	/*
	 * update superblock
	 */
	if (!(sbp->s_state & (FM_DIRTY | FM_LOGREDO)))
		sbp->s_state = FM_CLEAN;
	else
		sbp->s_state &= ~FM_EXTENDFS;
	sbp->s_size = sbp->s_xsize;
	sbp->s_agsize = agsize;
	sbp->s_fsckpxd = sbp->s_xfsckpxd;
	sbp->s_fscklog = 0;
	sbp->s_logpxd = sbp->s_xlogpxd;
	sbp->s_logserial = 1;

	if (rdwrSuper(fp, sbp, PB_UPDATE)) {
		fsck_send_msg(lrdo_EXTFSWRITEFSSUPERFAIL);
		goto errout;
	}

	/*
	 * finalize log
	 *
	 * note: new log is valid;
	 */
	/* read log superblock */
	t64 = (addressPXD(&sbp->s_logpxd) << sbp->s_l2bsize) + LOGPSIZE;
	if (ujfs_rw_diskblocks(fp, t64, LOGPSIZE, &logsup, GET)) {
		fsck_send_msg(lrdo_EXTFSREADLOGSUPFAIL);
		goto errout;
	}

	logsup.end = findEndOfLog();
	logsup.state = LOGREDONE;

	if (ujfs_rw_diskblocks(fp, t64, LOGPSIZE, &logsup, PUT)) {
		fsck_send_msg(lrdo_EXTFSWRITELOGSUPFAIL);
		goto errout;
	}

	fsck_send_msg(lrdo_REXTNDTOPOST);

	return 0;

      errout:
	fsck_send_msg(lrdo_REXTNDFAIL, errno);
	return (EXTENDFS_FAILRECOV);
}

/*
 *
 * NAME:        alloc_dmap_bitrec
 *
 * FUNCTION:    This routine allocates memory by calling the chkdsk
 *		alloc_wrksp() routine (because that will allocate high
 *		memory during autocheck).  If that fails then logredo
 *                   cannot continue bmap processing, so it will set a flag
 *                   and make the storage aleady allocated to the bmap
 *                   available for other uses.
 *		was successfully allocated and there's enough of it left,
 *		this routine will return a piece of it.
 */
int alloc_dmap_bitrec(struct dmap_bitmaps ** dmap_bitrec)
{
	int adb_rc = 0;
	int intermed_rc = 0;

	*dmap_bitrec = NULL;

	intermed_rc = alloc_wrksp((uint32_t) (sizeof (struct dmap_bitmaps)), 0,	/* not meaningful from logredo */
				  -1,	/* I am logredo */
				  (void **) dmap_bitrec);

	if ((intermed_rc != 0) || ((*dmap_bitrec) == NULL)) {
		Insuff_memory_for_maps = -1;
		available_stg_addr = bmap_stg_addr;
		available_stg_bytes = bmap_stg_bytes;
		/*
		 * initialize the storage for its new use
		 */
		memset((void *) available_stg_addr, 0, available_stg_bytes);
	}

	return (adb_rc);
}				/* end alloc_dmap_bitrec() */

/*
 *
 * NAME:        alloc_storage
 *
 * FUNCTION:    This routine allocates memory by calling the chkdsk
 *		alloc_wrksp() routine (because that will allocate high
 *		memory during autocheck).  If that fails and the bmap
 *		was successfully allocated and there's enough of it left,
 *		this routine will return a piece of it.
 */
int alloc_storage(int32_t size_in_bytes, void **addr_stg_ptr, int32_t * bmap_stg_returned)
{
	int as_rc = 0;
	int intermed_rc = 0;

	*bmap_stg_returned = 0;	/* assume we'll get it the usual way */
	*addr_stg_ptr = NULL;

	intermed_rc = alloc_wrksp((uint32_t) size_in_bytes, 0, -1, addr_stg_ptr);

	if ((intermed_rc != 0) || ((*addr_stg_ptr) == NULL)) {
		if ((!Insuff_memory_for_maps) && (bmap_stg_addr != NULL)) {
			/*
			 * we did allocate storage for the bmap
			 * and haven't started cannibalizing it yet
			 */
			Insuff_memory_for_maps = -1;
			available_stg_addr = bmap_stg_addr;
			available_stg_bytes = bmap_stg_bytes;
			/*
			 * initialize the storage for its new use
			 */
			memset((void *) available_stg_addr, 0, available_stg_bytes);
		}
		/* end we did allocate storage for the bmap... */
		if (Insuff_memory_for_maps & (available_stg_bytes != 0)) {
			/*
			 * we may be able to go on anyway
			 */
			if (available_stg_bytes < size_in_bytes) {
				/*
				 * not enough here
				 */
				return (ENOMEM0);
			} else {
				/* we can scavenge the memory we need */
				*addr_stg_ptr = available_stg_addr;
				available_stg_bytes -= size_in_bytes;
				available_stg_addr = (char *) (available_stg_addr + size_in_bytes);
				*bmap_stg_returned = -1;
			}
		} else {
			return (ENOMEM1);
		}
	}

	return (as_rc);
}

#ifdef  _JFS_WIP
/*
 *      nfsisloaded()
 *
 * check whether nfs is loaded
 */
static int nfsisloaded()
{
	int sav_errno;
	int (*entry) ();
	if (entry = load("/usr/sbin/probe", 0, 0))
		return (1);
	if (errno == ENOEXEC) {
		DBG_TRACE(("%s: nfs is not loaded\n", prog))
		    return (0);
	}
	sav_errno = errno;
	DBG_TRACE(("%s: ", prog))
	    errno = sav_errno;
	perror("load");
	return (0);
}
#endif				/* _JFS_WIP */

#ifdef _JFS_DEBUG
/*
 *      xdump()
 *
 * hex dump
 */
xdump(char *saddr, int count)
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
	void x_scpy();
	int x_scmp();
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
		if (((j = x_scmp(linebuf, prevbuf)) == 0) && (i < count)) {
			if (asterisk == ' ') {
				asterisk = '*';
				DBG_TRACE(("    *\n"))
			}
		} else {
			DBG_TRACE(("    %x  %s\n", linestart, linebuf))
			    asterisk = ' ';
			x_scpy(prevbuf, linebuf);
		}
	}
	return (0);
}

int x_scmp(char *s1, char *s2)
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

void x_scpy(char *s1, char *s2)
{
	while ((*s1 = *s2) != '\0') {
		s1++;
		s2++;
	}
}

prtdesc(struct lrd *ld)
{
	switch (ld->log.redopage.type) {
	case LOG_XTREE:
		DBG_TRACE((" REDOPAGE:XTREE\n  "))
		    break;
	case (LOG_XTREE | LOG_NEW):
		DBG_TRACE((" REDOPAGE:XTREE_NEW\n  "))
		    break;
	case (LOG_BTROOT | LOG_XTREE):
		DBG_TRACE((" REDOPAGE:BTROOT_XTREE\n  "))
		    break;
	case LOG_DTREE:
		DBG_TRACE((" REDOPAGE:DTREE\n  "))
		    break;
	case (LOG_DTREE | LOG_NEW):
		DBG_TRACE((" REDOPAGE:DTREE_NEW \n "))
		    break;
	case (LOG_DTREE | LOG_EXTEND):
		DBG_TRACE((" REDOPAGE:DTREE_EXTEND\n  "))
		    break;
	case (LOG_BTROOT | LOG_DTREE):
		DBG_TRACE((" REDOPAGE:BTROOT_DTREE\n  "))
		    break;
	case (LOG_BTROOT | LOG_DTREE | LOG_NEW):
		DBG_TRACE((" REDOPAGE:BTROOT_DTREE.NEW\n  "))
		    break;
	case LOG_INODE:
		/*
		 * logredo() updates imap for alloc of inode.
		 */
		DBG_TRACE((" REDOPAGE:INODE\n  "))
		    break;
	case LOG_EA:
		DBG_TRACE((" REDOPAGE:EA\n  "))
		    break;
	case LOG_DATA:
		DBG_TRACE((" REDOPAGE:DATA\n  "))
		    break;
	}
	return (0);
}
#endif				/* _JFS_DEBUG */
