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
#include <stdlib.h>
#include <string.h>

#include "xpeek.h"
#include "jfs_filsys.h"
#include "jfs_superblock.h"
#include "jfs_logmgr.h"
#include "fsckcbbl.h"
#include "fsckwsp.h"
#include "jfs_endian.h"
#include "super.h"

/* global data */
extern int64_t fsckwsp_offset;	/* defined in xpeek.c    */
extern int64_t jlog_super_offset;	/* defined in xpeek.c    */

/* forward references */
int display_cbblfsck(struct fsck_blk_map_hdr *);
int display_fsck_wsphdr(struct fsck_blk_map_hdr *);
int display_logsuper(struct logsuper *);

int get_fsckwsphdr(struct fsck_blk_map_hdr *);
int put_fsckwsphdr(struct fsck_blk_map_hdr *);

/*************************************************************
 *  cbblfsck()
 *
 */
void cbblfsck()
{
	struct fsck_blk_map_hdr fsckwsp;

	if (get_fsckwsphdr(&fsckwsp)) {
		fputs("cbblfsck: error returned from get_fsckwsphdr()\n\n", stderr);
		return;
	}
	if (display_cbblfsck(&fsckwsp) == XPEEK_CHANGED)
		if (put_fsckwsphdr(&fsckwsp))
			fputs("cbblfsck: error returned from put_fsckwsphdr()\n\n", stderr);
	return;
}				/* end cbblfsck() */

/*************************************************************
 *  fsckwsphdr()
 *
 */
void fsckwsphdr()
{
	struct fsck_blk_map_hdr fsckwsp;

	if (get_fsckwsphdr(&fsckwsp)) {
		fputs("fsckwsphdr: error returned from get_fsckwsphdr()\n\n", stderr);
		return;
	}
	if (display_fsck_wsphdr(&fsckwsp) == XPEEK_CHANGED)
		if (put_fsckwsphdr(&fsckwsp))
			fputs("fsckwsphdr: error returned from put_fsckwsphdr()\n\n", stderr);
	return;
}				/* end fsckwsphdr() */

/*************************************************************
 *  logsuper()
 *
 */
void logsuper()
{
	char buffer[PSIZE];
	struct logsuper *lsp;

	lsp = (struct logsuper *) & (buffer[0]);

	if (ujfs_get_logsuper(fp, lsp)) {
		fputs("logsuper: error returned from ujfs_get_logsuper()\n\n", stderr);
		return;
	}
	if (display_logsuper(lsp) == XPEEK_CHANGED)
		if (ujfs_put_logsuper(fp, lsp))
			fputs("logsuper: error returned from ujfs_put_logsuper()\n\n", stderr);
	return;
}				/* end logsuper() */

/*
 * ============================================================
 */

/*************************************************************
 *  display_cbblfsck()
 *
 *****************  Sample output of display_cbblfsck()

[1] eyecatcher:		'fsckcbbl'	    [13] bufptr_eyecatch:	'cbblbufs'
[2] cbbl_retcode: 	0		    [14] cbbl_agg_recptr:	0x00067f40
[3] fs_blksize: 	1024		    [15] ImapInoPtr: 		0x00067cb0
[4] lv_blksize:		512		    [16] ImapCtlPtr:		0x00061a70
[5] fs_lv_ratio: 	2
[6] fs_last_metablk:	0x00000000000008a8    (0d02216)
[7] fs_first_wspblk:	0x00000000001fba2b    (0d02079275)
[8] total_bad_blocks:	20
[9] resolved_blocks:	18		    [17] ImapLeafPtr:		0x00060a60
[10] reloc_extents:     14		    [18] iagPtr:		0x00066ca0
[11] reloc_blocks:	1028		    [19] InoExtPtr:		0x00060a70
[12] LVM_lists: 	3

 */
int display_cbblfsck(struct fsck_blk_map_hdr *fsckwsp_ptr)
{
	int rc = XPEEK_OK;
	struct fsckcbbl_record *cbblrec;
	char cmdline[512];
	int field;
	char *token;
	char s1[9];
	char s2[9];

	cbblrec = &(fsckwsp_ptr->cbblrec);

      changed:

	strncpy(s1, cbblrec->eyecatcher, 8);
	s1[8] = '\0';
	strncpy(s2, cbblrec->bufptr_eyecatcher, 8);
	s2[8] = '\0';

	printf("[1] eyecatcher:	\t'%s'\t", s1);
	printf("[13] bufptr_eyecatch:\t'%s'\n", s2);
	printf("[2] cbbl_retcode:\t%d\t\t", cbblrec->cbbl_retcode);
	printf("[14] cbbl_agg_recptr:\t0x%8p\n", cbblrec->clrbblks_agg_recptr);
	printf("[3] fs_blksize:\t\t%d\t\t", cbblrec->fs_blksize);
	printf("[15] ImapInoPtr:\t0x%8p\n", cbblrec->ImapInoPtr);
	printf("[4] lv_blksize:\t\t%d\t\t", cbblrec->lv_blksize);
	printf("[16] ImapCtlPtr:\t0x%8p\n", cbblrec->ImapCtlPtr);
	printf("[5] fs_lv_ratio:\t%d\n", cbblrec->fs_lv_ratio);
	printf("[6] last_metablk:\t0x%016llx    (0d0%lld)\n",
	       (long long) cbblrec->fs_last_metablk, (long long) cbblrec->fs_last_metablk);
	printf("[7] first_wspblk:\t0x%016llx    (0d0%lld)\n",
	       (long long) cbblrec->fs_first_wspblk, (long long) cbblrec->fs_first_wspblk);
	printf("[8] total_bad_blocks:\t%0d\n", cbblrec->total_bad_blocks);
	printf("[9] resolved_blocks:\t%d\t\t", cbblrec->resolved_blocks);
	printf("[17] ImapLeafPtr:\t0x%8p\n", cbblrec->ImapLeafPtr);
	printf("[10] reloc_extents:\t%d\t\t", cbblrec->reloc_extents);
	printf("[18] iagPtr:\t\t0x%8p\n", cbblrec->iagPtr);
	printf("[11] reloc_blocks:\t%lld\t\t", (long long) cbblrec->reloc_blocks);
	printf("[19] InoExtPtr:\t\t0x%8p\n", cbblrec->InoExtPtr);
	printf("[12] LVM_lists:\t\t%d\n", cbblrec->LVM_lists);

      retry:
	fputs("display_cbblfsck: [m]odify or e[x]it: ", stdout);
	fgets(cmdline, 512, stdin);
	token = strtok(cmdline, " 	\n");
	if (token == 0 || token[0] != 'm')
		return rc;

	field = m_parse(cmdline, 19, &token);
	if (field == 0)
		goto retry;

	switch (field) {
	case 1:
		strncpy(cbblrec->eyecatcher, token, 8);
		break;
	case 2:
		cbblrec->cbbl_retcode = strtol(token, 0, 8);
		break;
	case 3:
		cbblrec->fs_blksize = strtol(token, 0, 0);
		break;
	case 4:
		cbblrec->lv_blksize = strtol(token, 0, 0);
		break;
	case 5:
		cbblrec->fs_lv_ratio = strtol(token, 0, 0);
		break;
	case 6:
		cbblrec->fs_last_metablk = strtoull(token, 0, 16);
		break;
	case 7:
		cbblrec->fs_first_wspblk = strtoull(token, 0, 16);
		break;
	case 8:
		cbblrec->total_bad_blocks = strtol(token, 0, 0);
		break;
	case 9:
		cbblrec->resolved_blocks = strtol(token, 0, 0);
		break;
	case 10:
		cbblrec->reloc_extents = strtol(token, 0, 0);
		break;
	case 11:
		cbblrec->reloc_blocks = strtol(token, 0, 0);
		break;
	case 12:
		cbblrec->LVM_lists = strtoul(token, 0, 0);
		break;
	case 13:
		strncpy(cbblrec->bufptr_eyecatcher, token, 8);
		break;
	case 14:
		cbblrec->clrbblks_agg_recptr = (void *) strtoul(token, 0, 16);
		break;
	case 15:
		cbblrec->ImapInoPtr = (void *) strtoul(token, 0, 16);
		break;
	case 16:
		cbblrec->ImapCtlPtr = (void *) strtoul(token, 0, 16);
		break;
	case 17:
		cbblrec->ImapLeafPtr = (void *) strtoul(token, 0, 16);
		break;
	case 18:
		cbblrec->iagPtr = (void *) strtoul(token, 0, 16);
		break;
	case 19:
		cbblrec->InoExtPtr = (void *) strtoul(token, 0, 16);
		break;
	}
	rc = XPEEK_CHANGED;
	goto changed;
}				/* end display_cbblfsck() */

/*************************************************************
 *  display_fsck_wsphdr()
 *
 *
 *****************  Sample output of display_fsck_wsphdr()

[1] eyecatcher:			'wspblkmp'
[2] last_entry_pos:		0
[3] next_entry_pos: 		0
[4] start_time: 		2/5/999.11.2
[5] end_time:	 		2/5/999.11.4
[6] return_code:		0
[7] super_buff_addr:		0x164f0be0
[8] agg_record_addr:		0x164f4cd0
[9] bmap_record_addr:		0x16463ce0
[10] fscklog_agg_offset:	0x000000003f786600	(0d01064855040)
[11] fscklog_full:		0
[12] fscklog_buf_allocated:	-1
[13] fscklog_buf_alloc_err:	0
[14] num_logwrite_errors:	0

 */

int display_fsck_wsphdr(struct fsck_blk_map_hdr *wp)
{
	int rc = XPEEK_OK;
	char cmdline[512];
	int field;
	char *token;
	char s1[9];

      changed:

	strncpy(s1, wp->hdr.eyecatcher, 8);
	s1[8] = '\0';

	printf("[1] eyecatcher:\t\t\t'%s'\n", s1);
	printf("[2] last_entry_pos:\t\t%d\n", wp->hdr.last_entry_pos);
	printf("[3] next_entry_pos:\t\t%d\n", wp->hdr.next_entry_pos);
	printf("[4] start_time:\t\t\t%s\n", wp->hdr.start_time);
	printf("[5] end_time:\t\t\t%s\n", wp->hdr.end_time);
	printf("[6] return_code:\t\t%d\n", wp->hdr.return_code);
	printf("[7] super_buff_addr:\t\t%8p\n", wp->hdr.super_buff_addr);
	printf("[8] agg_record_addr:\t\t0x%8p\n", wp->hdr.agg_record_addr);
	printf("[9] bmap_record_addr:\t\t0x%8p\n", wp->hdr.bmap_record_addr);
	printf("[10] fscklog_agg_offset:\t0x%016llx    (0d0%lld)\n",
	       (long long) wp->hdr.fscklog_agg_offset, (long long) wp->hdr.fscklog_agg_offset);
	printf("[11] fscklog_full:\t\t%d\n", wp->hdr.fscklog_full);
	printf("[12] fscklog_buf_allocated:\t%d\n", wp->hdr.fscklog_buf_allocated);
	printf("[13] fscklog_buf_alloc_err:\t%d\n", wp->hdr.fscklog_buf_alloc_err);
	printf("[14] num_logwrite_errors:\t%d\n", wp->hdr.num_logwrite_errors);

      retry:
	fputs("display_fsck_wsphdr: [m]odify or e[x]it: ", stdout);
	fgets(cmdline, 512, stdin);
	token = strtok(cmdline, " 	\n");
	if (token == 0 || token[0] != 'm')
		return rc;

	field = m_parse(cmdline, 14, &token);
	if (field == 0)
		goto retry;

	switch (field) {
	case 1:
		strncpy(wp->hdr.eyecatcher, token, 8);
		break;
	case 2:
		wp->hdr.last_entry_pos = strtol(token, 0, 0);
		break;
	case 3:
		wp->hdr.next_entry_pos = strtol(token, 0, 0);
		break;
	case 4:
		strncpy(wp->hdr.start_time, token, 16);
		break;
	case 5:
		strncpy(wp->hdr.end_time, token, 16);
		break;
	case 6:
		wp->hdr.return_code = strtol(token, 0, 0);
		break;
	case 7:
		wp->hdr.super_buff_addr = (char *) strtoul(token, 0, 16);
		break;
	case 8:
		wp->hdr.agg_record_addr = (char *) strtoul(token, 0, 16);
		break;
	case 9:
		wp->hdr.bmap_record_addr = (char *) strtoul(token, 0, 16);
		break;
	case 10:
		wp->hdr.fscklog_agg_offset = strtoull(token, 0, 16);
		break;
	case 11:
		wp->hdr.fscklog_full = strtol(token, 0, 0);
		break;
	case 12:
		wp->hdr.fscklog_buf_allocated = strtol(token, 0, 0);
		break;
	case 13:
		wp->hdr.fscklog_buf_alloc_err = strtol(token, 0, 0);
		break;
	case 14:
		wp->hdr.num_logwrite_errors = strtol(token, 0, 8);
		break;
	}
	rc = XPEEK_CHANGED;
	goto changed;
}				/* end display_fsck_wsphdr() */

/*************************************************************
 *  display_logsuper()
 *
 *
 *****************  Sample output of display_logsuper()

[1]  magic:			0x87654321
[2]  version:			1
[3]  serial:			5
[4]  log size (# blocks):	32768	(at 4096 bytes/log block)
[5]  agg block size:		4096	(bytes)
[6]  log2(agg blk size):	12
[7]  flag:			0x10200100
[8]  state:			0x00000000  LOGMOUNT
[9]  end:			0x5dd4  (d 24020)
[10] uuid:			5a845eb7-31ee-451f-8c5d-ef517b08c9d5
[11] label:			'                '
active file systems:
[]  active[0]:	c60fb757-05e5-4014-a1ea-6f2ad08073b2

 */

int display_logsuper(struct logsuper * lsp)
{
	int rc = XPEEK_OK;
	char cmdline[512];
	int field;
	char *token;
	char *state;
	char uuid_unparsed[37];
	int i, uuid_rc;
	bool active_fs = false;

      changed:
	switch (lsp->state) {
	case LOGMOUNT:
		state = "LOGMOUNT";
		break;
	case LOGREDONE:
		state = "LOGREDONE";
		break;
	case LOGWRAP:
		state = "LOGWRAP";
		break;
	case LOGREADERR:
		state = "LOGREADERR";
		break;
	default:
		state = "Unknown State";
		break;
	}

	printf("[1]  magic:\t\t\t0x%x\n", lsp->magic);
	printf("[2]  version:\t\t\t%d\n", lsp->version);
	printf("[3]  serial:\t\t\t%d\n", lsp->serial);
	printf("[4]  log size (# blocks):\t%d\t(at 4096 bytes/log block)\n", lsp->size);
	printf("[5]  agg block size:\t\t%d\t(bytes)\n", lsp->bsize);
	printf("[6]  log2(agg blk size):\t%d\n", lsp->l2bsize);
	printf("[7]  flag:\t\t\t0x%08x\n", lsp->flag);
	printf("[8]  state:\t\t\t0x%08x  %s\n", lsp->state, state);
	printf("[9]  end:\t\t\t0x%x  (d %d)\n", lsp->end, lsp->end);
	uuid_unparse(lsp->uuid, uuid_unparsed);
	printf("[10] uuid:\t\t\t%s\n", uuid_unparsed);
	printf("[11] label:\t\t\t'%16s'\n", lsp->label);
	printf("active file systems:\n");
	for (i = 0; i < MAX_ACTIVE; i++) {
		uuid_unparse(lsp->active[i], uuid_unparsed);
		/* only print active (non-zero uuid) file systems */
		if (strncmp(uuid_unparsed, "00000000-0000-0000-0000-000000000000", 36)) {
			printf("[]  active[%d]:\t%s\n", i, uuid_unparsed);
			active_fs = true;
		}
	}

	if (!active_fs) {
		printf("None active.\n");
	}

      retry:
	fputs("\ndisplay_logsuper: [m]odify or e[x]it: ", stdout);
	fgets(cmdline, 512, stdin);
	token = strtok(cmdline, " 	\n");
	if (token == 0 || token[0] != 'm')
		return rc;

	field = m_parse(cmdline, 11, &token);
	if (field == 0)
		goto retry;

	switch (field) {
	case 1:
		lsp->magic = strtoul(token, 0, 16);
		break;
	case 2:
		lsp->version = strtol(token, 0, 0);
		break;
	case 3:
		lsp->serial = strtol(token, 0, 0);
		break;
	case 4:
		lsp->size = strtol(token, 0, 0);
		break;
	case 5:
		lsp->bsize = strtol(token, 0, 0);
		break;
	case 6:
		lsp->l2bsize = strtol(token, 0, 0);
		break;
	case 7:
		lsp->flag = strtoul(token, 0, 16);
		break;
	case 8:
		lsp->state = strtoul(token, 0, 16);
		break;
	case 9:
		lsp->end = strtoul(token, 0, 16);
		break;
	case 10:
		strncpy(uuid_unparsed, token, 36);
		uuid_rc = uuid_parse(uuid_unparsed, lsp->uuid);
		if (uuid_rc) {
			fputs
			    ("\ndisplay_logsuper: uuid_parse() FAILED.  uuid entered in improper format.\n",
			     stderr);
			goto retry;
		}
		break;
	case 11:
		strncpy(lsp->label, token, 16);
		break;
	}
	rc = XPEEK_CHANGED;
	goto changed;
}				/* end display_logsuper() */

/*************************************************************
 *  get_fsckwsphdr()
 *
 */
int get_fsckwsphdr(struct fsck_blk_map_hdr *wsphdr_ptr)
{
	if (xRead(fsckwsp_offset, PSIZE, (char *) wsphdr_ptr)) {
		fputs("get_fsckwsphdr: error returned from xRead(fsckwsp_offset)\n\n", stderr);
		return (-2);
	}

	/* swap if on big endian machine */
	ujfs_swap_fsck_blk_map_hdr(wsphdr_ptr);

	return (0);
}				/* end get_fsckwsphdr() */

/*************************************************************
 *  put_fsckwsphdr()
 *
 */
int put_fsckwsphdr(struct fsck_blk_map_hdr * wsphdr_ptr)
{

	/* swap if on big endian machine */
	ujfs_swap_fsck_blk_map_hdr(wsphdr_ptr);

	if (xWrite(fsckwsp_offset, PSIZE, (char *) wsphdr_ptr)) {
		fputs("put_fsckwsphdr: error returned from xWrite(fsckwsp_offset)\n\n", stderr);
		/* swap back if on big endian machine */
		ujfs_swap_fsck_blk_map_hdr(wsphdr_ptr);
		return (-4);
	}

	/* swap back if on big endian machine */
	ujfs_swap_fsck_blk_map_hdr(wsphdr_ptr);

	return (0);
}				/* end put_fsckwsphdr() */
