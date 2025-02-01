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
#ifndef H_LOGREDO
#define H_LOGREDO

/*
 * NOTE:
 *	If logredo passes rc > 0 to fsck,
 *		fsck will do a full check and repair process
 *
 *	but if logredo passes rc < 0 to fsck,
 *		fsck will do a full check and repair process AND
 *		fsck will reformat the log
 */
#define MINOR_ERROR     1	/* error that will not abort the program */
#define MAJOR_ERROR    -2	/* return code to force abort of logredo */
#define REFORMAT_ERROR -3	/* return code to force logform of log  */

#define ENOMEM0    100		/* insufficient dynamic storage, location 0 */
#define ENOMEM1    101		/* insufficient dynamic storage, location 1 */
#define ENOMEM2    102		/* insufficient dynamic storage, location 2 */
#define ENOMEM3    103		/* insufficient dynamic storage, location 3 */
#define ENOMEM4    104		/* insufficient dynamic storage, location 4 */
#define ENOMEM5    105		/* insufficient dynamic storage, location 5 */
#define ENOMEM6    106		/* insufficient dynamic storage, location 6 */
#define ENOMEM7    107		/* insufficient dynamic storage, location 7 */
#define ENOMEM8    108		/* insufficient dynamic storage, location 8 */
#define ENOMEM9    109		/* insufficient dynamic storage, location 9 */
#define ENOMEM10   110		/* insufficient dynamic storage, location 10 */
#define ENOMEM11   111		/* insufficient dynamic storage, location 11 */
#define ENOMEM12   112		/* insufficient dynamic storage, location 12 */
#define ENOMEM25   175		/* insufficient dynamic storage for BMap */

#define BFLUSH_WRITEERROR	 201
#define BLOCK_OUTOFRANGE	 202
#define BMAP_READERROR1		 203
#define BMAP_READERROR2		 204
#define BMAP_READERROR3		 205
#define BMAP_READERROR4		 206
#define BMAP_READERROR5		 207
#define BMAP_READERROR6		 208
#define BMAP_WRITEERROR1	 209
#define BMAP_WRITEERROR2	 210
#define BMAP_WRONGTOTPAGES	 211
#define BREAD_BMAPINIT		 212
#define BREAD_IMAPINIT		 213
#define CANTOPEN_INLINELOG 	-214
#define CANTOPEN_OUTLINELOG 	-215
#define CANTREAD_2NDFSSUPER	 216
#define CANTREAD_PRIMFSSUPER	 217
#define CANTWRITE_2NDFSSUPER 	 218
#define CANTWRITE_PRIMFSSUPER 	 219
#define DMAP_UPDATEFAIL		 220
#define DOSFSCTL_ERROR 		-221
#define DTPAGE_BADSLOTNEXTIDX1 	-222
#define DTPAGE_BADSLOTNEXTIDX2 	-223
#define DTPAGE_BADSTBLENTRY1 	-224
#define DTPAGE_BADSTBLENTRY2 	-225
#define DTPAGE_READERROR1	 226
#define DTREE_READERROR1	 227
#define EXTENDFS_FAILRECOV	 228
#define FSSUPER_READERROR1	 229
#define FSSUPER_READERROR2       230
#define ILLEGAL_LEAF_IND0  	 231
#define ILLEGAL_LEAF_IND1  	 232
#define IMAP_READERROR1		 233
#define IMAP_READERROR2		 234
#define IMAP_READERROR3	         235
#define IMAP_READERROR4		 236
#define IMAP_READERROR5		 237
#define IMAP_READERROR6		 238
#define IMAP_WRITEERROR1	 239
#define IMAP_WRITEERROR2	 240
#define IMAP_WRONGNPAGES	 241
#define INOEXT_READERROR1	 242
#define INOEXT_READERROR2	 243
#define INVALID_LOGEND 		-244
#define INVALID_NEXTADDR 	-245
#define JFS_BLKSIZE_ERROR 	-246
#define JFS_LOGSIZE_ERROR 	-247
#define JFS_L2BLKSIZE_ERROR 	-248
#define JFS_VERSION_ERROR 	-249
#define JLOG_LOGWRAP 		-250
#define JLOG_NOCOMFREE 		-251
#define JLOG_READERROR1 	-252
#define JLOG_WRITEERROR1 	-253
#define LOG_OPEN_ERROR 		-254
#define LOGSUPER_BADLOGLOC 	-255
#define LOGSUPER_LOGNOTINLINE 	-256
#define LOGSUPER_BADMAGIC 	-257
#define LOGSUPER_BADSERIAL 	-258
#define LOGSUPER_BADVERSION 	-259
#define LOGSUPER_READ_ERROR 	-260
#define LOGSUPER_WRITEERROR1 	-261
#define LOG_WRAPPED_TWICE 	-262
#define NEXTADDR_SAME 		-263
#define NOTREBUILDING_BMAP	 264
#define NOTREBUILDING_IMAP	 265
#define NOT_FSDEV_ERROR  	 266
#define NOT_INLINELOG_ERROR  	 267
#define NOT_LOG_FILE_ERROR 	-268
#define UNRECOG_LOGRECTYP	-269
#define XTPAGE_READERROR1	 270
#define BMAP_READERROR7 	 271
#define READLOGERROR		-272
#define LOG_IN_USE		 273
#define UPDATEPAGE_BAD_RANGE	 274

/* error types
 */
#define MAPERR    0
#define DBTYPE    1
#define INOTYPE   2
#define READERR   3
#define UNKNOWNR  4
#define LOGEND    5
#define SERIALNO  6
#define OPENERR   7
#define IOERROR   8
#define LOGRCERR  9

#define PB_READ    0
#define PB_UPDATE  1

/* log.location */
#define INLINELOG	0x1
#define OUTLINELOG	0x2

#define NBUFPOOL  128

/*
 * the following are used to map the bmap_wsp array elements
 * which describe block map pages
 */
struct dmap_bitmaps {
	uint32_t wmap[LPERDMAP];	/* 1024: bits of the working map    */
	uint32_t pmap[LPERDMAP];	/* 1024: bits of the persistent map */
};				/* - 2048 -                         */

struct bmap_wsp {
	int64_t page_offset;	/* aggregate offset for the page */
	struct dmap_bitmaps *dmap_bitmaps;	/*
						   * IF this record describes a dmap page,
						   * AND IF that dmap page is affected by
						   * transactions processed in this
						   * logredo session, THEN this field is
						   * a ptr to bit maps for the page.
						   * Otherwise this field is null.
						 */
	uint32_t available;	/* unused at this time  */
};

/*
 * the following are used to map the imap_wsp array elements
 * which describe the inode map pages
 */
struct iag_data {
	uint32_t wmap[EXTSPERIAG];	/* 512: working allocation map    */
	uint32_t pmap[EXTSPERIAG];	/* 512: persistent allocation map */
	pxd_t inoext[EXTSPERIAG];	/* 1024: inode extent addresses   */
};				/* - 2048 -                       */

struct imap_wsp {
	int64_t page_offset;	/* aggregate offset for the page */
	struct iag_data *imap_data;	/* IF this record describes an IAG
					   *    AND IF that IAG is affected by
					   *    transactions processed in this
					   *    logredo session, THEN this field
					   *    is a ptr to data for the IAG.
					   *    Otherwise this field is null.
					 */
	uint32_t available;	/* unused at this time  */
};

/*
 * Information about the log device
 */
struct log_info {
	FILE *fp;			/* file descriptor */
	int32_t serial;		/* log serial number */
	int location;		/* INLINELOG or OUTLINELOG */
	int64_t xaddr;		/* log address in bytes offset  */
	int64_t size;		/* log size in 4K blocks  */
	int bsize;		/* Logical Block size */
	int l2bsize;		/* log2 of bsize */
	uuid_t uuid;		/* 128-bit device uuid */
	int32_t devnum;		/* Kernel-style device number */
};
extern struct log_info Log;

/*
 *      open file system aggregate/lv array
 */
struct vopen {
	dev_t device;		/* device major/minor */
	int state;		/* see below */
	int next_hash;		/* Next vopen index in hash list */
	FILE *fp;			/* file descriptor for the opened aggregate */
	uuid_t uuid;		/* 128-bit device uuid */
	int32_t status;		/* status of  aggregate/volume */
	int64_t fssize;		/* fs size, total number of aggre. blks */
	int32_t lblksize;	/* aggre. blksize (ie fs blksize) in bytes */
	int16_t lbperpage;	/* aggregate blocks per page */
	int16_t is_fsdirty;	/* true if superblock.state is FM_DIRTY */
	int16_t l2bsize;	/* log2 of s_bsize */
	int16_t l2bfactor;	/* log2(s_bsize/physical block size) */
	int32_t agsize;		/* num of aggre. blks per alloc group  */
	int32_t l2agsize;	/* Log2 of agsize   */
	int32_t numag;		/* total num of ags in this aggregate. */
	struct dinomap *aggimap_ctrl;	/* pointer to the control page of
					   aggregate inode map */
	struct iag *aggimap_iag;	/* ptr to aggregate inode map iag pages */
	struct fsimap_lst {
		int32_t fileset;	/* fileset number, which is the inode
					 * number of this fileset imap inode in
					 * the aggregate inode table.
					 */
		int imap_page_count;	/* number of pages in the imap,
					   * including the control page
					 */
		struct dinomap *fsimapctrl;	/* ptr to ctrl page of fileset imap */

		struct imap_wsp *imap_wsp;	/*
						   * ptr to an array of IAG workspaces
						 */
		struct iag *iag_pbuf;	/* ptr to an iag page buffer */
		int imap_pagenum;	/* page offset, in inode map,
					 * of buffer contents
					 */
		struct iag *iag_pbuf2;	/* ptr to another iag page buffer */
		int imap_pagenum2;	/* page offset, in inode map,
					 * of buffer contents
					 */
		struct fsimap_lst *next;	/* ptr to next fileset imap */
	} fsimap_lst;		/* the list of multiple fileset imap */

	int bmap_page_count;	/* number of pages in the bmap,
				 * including the control page
				 */
	struct dbmap *bmap_ctl;	/* ptr to control page of aggregate block
				 * allocate map (i.e. bmap) file
				 */
	struct bmap_wsp *bmap_wsp;	/*
					   * ptr to an array of blockmap page workspaces
					 */
	struct dmap *dmap_pbuf;	/* ptr to a dmap page buffer */
	int dmap_pagenum;	/* page offset, in block map, of
				 * buffer contents
				 */
	struct dmapctl *L0_pbuf;	/* ptr to a dmap page buffer */
	int L0_pagenum;		/* page offset, in block map, of
				 * buffer contents
				 */
	struct dmapctl *L1_pbuf;	/* ptr to a dmap page buffer */
	int L1_pagenum;		/* page offset, in block map, of
				 * buffer contents
				 */
	struct dmapctl *L2_pbuf;	/* ptr to a dmap page buffer */
	int L2_pagenum;		/* page offset, in block map, of
				 * buffer contents
				 */
};

/* vopen state */
#define VOPEN_EMPTY	0	/* Unused slot */
#define VOPEN_OPEN	1	/* Device is open */
#define VOPEN_CLOSED	2	/* Device is closed */

#define  fsimap_iag     fsimap_lst.fsimapiag

int jfs_logredo(caddr_t, FILE *, int32_t);
int findLog(FILE *, int *);

#endif				/* H_LOGREDO */
