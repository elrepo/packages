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
#include <stdio.h>
#include <time.h>
#include "jfs_endian.h"
#include "devices.h"

#if __BYTE_ORDER == __BIG_ENDIAN
/*--------------------------------------------------------------------
 * NAME: ujfs_swap_dbmap
 *
 * FUNCTION: Change endianness of dbmap structure
 *
 * PARAMETERS:
 *      dbm_t         - dbmap structure
 *
 * RETURNS: NONE
 */
void ujfs_swap_dbmap(struct dbmap *dbm_t)
{
	int i;

	/* struct dbmap in jfs_dmap.h */

	dbm_t->dn_mapsize = __le64_to_cpu(dbm_t->dn_mapsize);
	dbm_t->dn_nfree = __le64_to_cpu(dbm_t->dn_nfree);
	dbm_t->dn_l2nbperpage = __le32_to_cpu(dbm_t->dn_l2nbperpage);
	dbm_t->dn_numag = __le32_to_cpu(dbm_t->dn_numag);
	dbm_t->dn_maxlevel = __le32_to_cpu(dbm_t->dn_maxlevel);
	dbm_t->dn_maxag = __le32_to_cpu(dbm_t->dn_maxag);
	dbm_t->dn_agpref = __le32_to_cpu(dbm_t->dn_agpref);
	dbm_t->dn_aglevel = __le32_to_cpu(dbm_t->dn_aglevel);
	dbm_t->dn_agheigth = __le32_to_cpu(dbm_t->dn_agheigth);
	dbm_t->dn_agwidth = __le32_to_cpu(dbm_t->dn_agwidth);
	dbm_t->dn_agstart = __le32_to_cpu(dbm_t->dn_agstart);
	dbm_t->dn_agl2size = __le32_to_cpu(dbm_t->dn_agl2size);
	for (i = 0; i < MAXAG; i++)
		dbm_t->dn_agfree[i] = __le64_to_cpu(dbm_t->dn_agfree[i]);
	dbm_t->dn_agsize = __le64_to_cpu(dbm_t->dn_agsize);
}

/*--------------------------------------------------------------------
 * NAME: ujfs_swap_dinode
 *
 * FUNCTION: Change endianness of dinode structure
 *
 * PARAMETERS:
 *      dinode         - dinode structure
 *
 * RETURNS: NONE
 */
void ujfs_swap_dinode(struct dinode *di, int32_t mode, uint32_t sb_flag)
{
	dtpage_t *p;

	/* struct dinode in jfs_dinode.h */

	di->di_inostamp = __le32_to_cpu(di->di_inostamp);
	di->di_fileset = __le32_to_cpu(di->di_fileset);
	di->di_number = __le32_to_cpu(di->di_number);
	di->di_gen = __le32_to_cpu(di->di_gen);

	di->di_size = __le64_to_cpu(di->di_size);
	di->di_nblocks = __le64_to_cpu(di->di_nblocks);
	di->di_nlink = __le32_to_cpu(di->di_nlink);
	di->di_uid = __le32_to_cpu(di->di_uid);
	di->di_gid = __le32_to_cpu(di->di_gid);

	/* struct timestruc_t */
	di->di_atime.tv_sec = __le32_to_cpu(di->di_atime.tv_sec);
	di->di_atime.tv_nsec = __le32_to_cpu(di->di_atime.tv_nsec);

	/* struct timestruc_t */
	di->di_ctime.tv_sec = __le32_to_cpu(di->di_ctime.tv_sec);
	di->di_ctime.tv_nsec = __le32_to_cpu(di->di_ctime.tv_nsec);

	/* struct timestruc_t */
	di->di_mtime.tv_sec = __le32_to_cpu(di->di_mtime.tv_sec);
	di->di_mtime.tv_nsec = __le32_to_cpu(di->di_mtime.tv_nsec);

	/* struct timestruc_t */
	di->di_otime.tv_sec = __le32_to_cpu(di->di_otime.tv_sec);
	di->di_otime.tv_nsec = __le32_to_cpu(di->di_otime.tv_nsec);

	/* dxd_t   di_acl; */
	di->di_acl.size = __le32_to_cpu(di->di_acl.size);

	/* dxd_t   di_ea; */
	di->di_ea.size = __le32_to_cpu(di->di_ea.size);

	di->di_acltype = __le32_to_cpu(di->di_acltype);

	if (mode == GET) {
		/* if reading from disk, swap before use in 'if' that follows */
		di->di_mode = __le32_to_cpu(di->di_mode);
		di->di_next_index = __le32_to_cpu(di->di_next_index);
	}

	if (ISDIR(di->di_mode)) {
		/* cast di_btroot to dtree and swap it */
		p = (dtpage_t *) & (di->di_btroot);
		if (p->header.flag & BT_ROOT)
			ujfs_swap_dtpage_t(p, sb_flag);

		if ((sb_flag & JFS_DIR_INDEX) == JFS_DIR_INDEX) {
			if (di->di_next_index > MAX_INLINE_DIRTABLE_ENTRY + 1)
				/* cast di_dirtable to xtree and swap it */
				ujfs_swap_xtpage_t((xtpage_t *) & (di->di_dirtable));
		}
	} else if (ISREG(di->di_mode) || ISLNK(di->di_mode)) {
		/* cast di_btroot to xtree and swap it */
		ujfs_swap_xtpage_t((xtpage_t *) & (di->di_btroot));
	}

	if (mode == PUT) {
		di->di_mode = __le32_to_cpu(di->di_mode);
		di->di_next_index = __le32_to_cpu(di->di_next_index);
	}
}

/*--------------------------------------------------------------------
 * NAME: ujfs_swap_dinomap
 *
 * FUNCTION: Change endianness of struct dinomap structure
 *
 * PARAMETERS:
 *      dim_t         - struct dinomap structure
 *
 * RETURNS: NONE
 */
void ujfs_swap_dinomap(struct dinomap *dim_t)
{
	int i;

	/* struct dinomap in jfs_imap.h */

	dim_t->in_freeiag = __le32_to_cpu(dim_t->in_freeiag);
	dim_t->in_nextiag = __le32_to_cpu(dim_t->in_nextiag);
	dim_t->in_numinos = __le32_to_cpu(dim_t->in_numinos);
	dim_t->in_numfree = __le32_to_cpu(dim_t->in_numfree);
	dim_t->in_nbperiext = __le32_to_cpu(dim_t->in_nbperiext);
	dim_t->in_l2nbperiext = __le32_to_cpu(dim_t->in_l2nbperiext);
	dim_t->in_diskblock = __le32_to_cpu(dim_t->in_diskblock);
	dim_t->in_maxag = __le32_to_cpu(dim_t->in_maxag);

	for (i = 0; i < MAXAG; i++) {
		dim_t->in_agctl[i].inofree = __le32_to_cpu(dim_t->in_agctl[i].inofree);
		dim_t->in_agctl[i].extfree = __le32_to_cpu(dim_t->in_agctl[i].extfree);
		dim_t->in_agctl[i].numinos = __le32_to_cpu(dim_t->in_agctl[i].numinos);
		dim_t->in_agctl[i].numfree = __le32_to_cpu(dim_t->in_agctl[i].numfree);
	}
}

/*--------------------------------------------------------------------
 * NAME: ujfs_swap_dmap
 *
 * FUNCTION: Change endianness of dmap structure
 *
 * PARAMETERS:
 *      dm_t         - dmap structure
 *
 * RETURNS: NONE
 */
void ujfs_swap_dmap(struct dmap *dm_t)
{
	int i;

	/* struct dmap in jfs_dmap.h */

	dm_t->nblocks = __le32_to_cpu(dm_t->nblocks);
	dm_t->nfree = __le32_to_cpu(dm_t->nfree);
	dm_t->start = __le64_to_cpu(dm_t->start);

	/* struct dmaptree tree; */
	dm_t->tree.nleafs = __le32_to_cpu(dm_t->tree.nleafs);
	dm_t->tree.l2nleafs = __le32_to_cpu(dm_t->tree.l2nleafs);
	dm_t->tree.leafidx = __le32_to_cpu(dm_t->tree.leafidx);
	dm_t->tree.height = __le32_to_cpu(dm_t->tree.height);

	for (i = 0; i < LPERDMAP; i++) {
		dm_t->wmap[i] = __le32_to_cpu(dm_t->wmap[i]);
		dm_t->pmap[i] = __le32_to_cpu(dm_t->pmap[i]);
	}
}

/*--------------------------------------------------------------------
 * NAME: ujfs_swap_dmapctl
 *
 * FUNCTION: Change endianness of dmapctl structure
 *
 * PARAMETERS:
 *      dmc_t         - dmapctl structure
 *
 * RETURNS: NONE
 */
void ujfs_swap_dmapctl(struct dmapctl *dmc_t)
{
	/* struct dmapctl in jfs_dmap.h */

	dmc_t->nleafs = __le32_to_cpu(dmc_t->nleafs);
	dmc_t->l2nleafs = __le32_to_cpu(dmc_t->l2nleafs);
	dmc_t->leafidx = __le32_to_cpu(dmc_t->leafidx);
	dmc_t->height = __le32_to_cpu(dmc_t->height);
}

/*--------------------------------------------------------------------
 * NAME: ujfs_swap_dtpage_t
 *
 * FUNCTION: Change endianness of dtpage_t structure
 *
 * PARAMETERS:
 *      dtp_t         - dtpage_t structure
 *
 * RETURNS: NONE
 */
void ujfs_swap_dtpage_t(dtpage_t * dtp_t, uint32_t sb_flag)
{
	struct dtslot *dtslot;
	int index, i, j, DtlHdrDataLen, len;
	int8_t *stbl;
	int lastslot;

	/* dtpage_t in jfs_dtree.h */

	if (dtp_t->header.flag & BT_ROOT) {	/* root page */
		dtroot_t *dtroot = (dtroot_t *) dtp_t;
		dtroot->header.idotdot = __le32_to_cpu(dtroot->header.idotdot);
		stbl = dtroot->header.stbl;
		lastslot = DTROOTMAXSLOT - 1;
	} else {		/* non-root page */
		dtp_t->header.next = __le64_to_cpu(dtp_t->header.next);
		dtp_t->header.prev = __le64_to_cpu(dtp_t->header.prev);

		stbl = (int8_t *) & dtp_t->slot[dtp_t->header.stblindex];
		lastslot = DTPAGEMAXSLOT - 1;
	}
	/*
	 * Make sure we don't dereference beyond the end of stbl.
	 * We can keep quiet here.  The problem will be reported elsewhere.
	 */
	if (dtp_t->header.nextindex > lastslot + 1)
		return;

	if (dtp_t->header.flag & BT_LEAF) {
		struct ldtentry *ldtentry;

		for (i = 0; i < dtp_t->header.nextindex; i++) {
			/*
			   * leaf node entry head-only segment
			 */
			index = stbl[i];
			/* Don't let bad dtree make us go outside of page */
			if (index > lastslot)
				continue;
			ldtentry = (struct ldtentry *) &dtp_t->slot[index];
			ldtentry->inumber = __le32_to_cpu(ldtentry->inumber);

			if ((sb_flag & JFS_DIR_INDEX) == JFS_DIR_INDEX) {
				DtlHdrDataLen = DTLHDRDATALEN;
				ldtentry->index =
					__le32_to_cpu(ldtentry->index);
			} else
				DtlHdrDataLen = DTLHDRDATALEN_LEGACY;

			len = ldtentry->namlen;
			for (j = 0; j < MIN(ldtentry->namlen, DtlHdrDataLen); j++)
				ldtentry->name[j] = __le16_to_cpu(ldtentry->name[j]);
			len -= DtlHdrDataLen;

			index = ldtentry->next;
			/*
			   * additional segments
			 */
			while ((index != -1) && (len > 0)) {
				if (index > lastslot)
					break;
				dtslot = &dtp_t->slot[index];
				for (j = 0; j < DTSLOTDATALEN; j++)
					dtslot->name[j] = __le16_to_cpu(dtslot->name[j]);
				len -= DTSLOTDATALEN;
				index = dtslot->next;
			}
		}
	} else {		/* BT_INTERNAL */
		struct idtentry *idtentry;

		for (i = 0; i < dtp_t->header.nextindex; i++) {
			/*
			   * internal node entry head-only segment
			 */
			index = stbl[i];
			/* Don't let bad dtree make us go outside of page */
			if (index > lastslot)
				continue;
			idtentry = (struct idtentry *) &dtp_t->slot[index];
			len = idtentry->namlen;
			for (j = 0; j < MIN(idtentry->namlen, DTIHDRDATALEN); j++)
				idtentry->name[j] = __le16_to_cpu(idtentry->name[j]);
			len -= DTIHDRDATALEN;
			index = idtentry->next;
			/*
			 * additional segments
			 */
			while ((index != -1) && (len > 0)) {
				if (index > lastslot)
					break;
				dtslot = &dtp_t->slot[index];
				for (j = 0; j < DTSLOTDATALEN; j++)
					dtslot->name[j] = __le16_to_cpu(dtslot->name[j]);
				len -= DTSLOTDATALEN;
				index = dtslot->next;
			}
		}
	}
}

/*--------------------------------------------------------------------
 * NAME: ujfs_swap_fsck_blk_map_hdr
 *
 * FUNCTION: Change endianness of fsck_blk_map_hdr structure
 *
 * PARAMETERS:
 *      fsck_bmap_h         - fsck block map header structure
 *
 * RETURNS: NONE
 */
void ujfs_swap_fsck_blk_map_hdr(struct fsck_blk_map_hdr *fsck_bmap_h)
{
	int i;

	/* fsck_blk_map_hdr in libfs/fsckwsp.h */

	/* struct fsckcbbl_record  cbblrec */
	fsck_bmap_h->cbblrec.cbbl_retcode = __le32_to_cpu(fsck_bmap_h->cbblrec.cbbl_retcode);
	fsck_bmap_h->cbblrec.fs_blksize = __le32_to_cpu(fsck_bmap_h->cbblrec.fs_blksize);
	fsck_bmap_h->cbblrec.lv_blksize = __le32_to_cpu(fsck_bmap_h->cbblrec.lv_blksize);
	fsck_bmap_h->cbblrec.fs_lv_ratio = __le32_to_cpu(fsck_bmap_h->cbblrec.fs_lv_ratio);
	fsck_bmap_h->cbblrec.fs_last_metablk = __le64_to_cpu(fsck_bmap_h->cbblrec.fs_last_metablk);
	fsck_bmap_h->cbblrec.fs_first_wspblk = __le64_to_cpu(fsck_bmap_h->cbblrec.fs_first_wspblk);
	fsck_bmap_h->cbblrec.total_bad_blocks =
	    __le32_to_cpu(fsck_bmap_h->cbblrec.total_bad_blocks);
	fsck_bmap_h->cbblrec.resolved_blocks = __le32_to_cpu(fsck_bmap_h->cbblrec.resolved_blocks);
	fsck_bmap_h->cbblrec.reloc_extents = __le32_to_cpu(fsck_bmap_h->cbblrec.reloc_extents);
	fsck_bmap_h->cbblrec.reloc_blocks = __le64_to_cpu(fsck_bmap_h->cbblrec.reloc_blocks);
	fsck_bmap_h->cbblrec.LVM_lists = __le32_to_cpu(fsck_bmap_h->cbblrec.LVM_lists);

	fsck_bmap_h->hdr.last_entry_pos = __le32_to_cpu(fsck_bmap_h->hdr.last_entry_pos);
	fsck_bmap_h->hdr.next_entry_pos = __le32_to_cpu(fsck_bmap_h->hdr.next_entry_pos);
	fsck_bmap_h->hdr.return_code = __le32_to_cpu(fsck_bmap_h->hdr.return_code);

	fsck_bmap_h->hdr.fscklog_agg_offset = __le64_to_cpu(fsck_bmap_h->hdr.fscklog_agg_offset);
	fsck_bmap_h->hdr.num_logwrite_errors = __le32_to_cpu(fsck_bmap_h->hdr.num_logwrite_errors);

	/* struct fscklog_error logerr[125] */
	for (i = 0; i < 125; i++) {
		fsck_bmap_h->hdr.logerr[i].err_offset =
		    __le64_to_cpu(fsck_bmap_h->hdr.logerr[i].err_offset);
		fsck_bmap_h->hdr.logerr[i].bytes_written =
		    __le32_to_cpu(fsck_bmap_h->hdr.logerr[i].bytes_written);
		fsck_bmap_h->hdr.logerr[i].io_retcode =
		    __le32_to_cpu(fsck_bmap_h->hdr.logerr[i].io_retcode);
	}
}

/*--------------------------------------------------------------------
 * NAME: ujfs_swap_fsck_blk_map_page
 *
 * FUNCTION: Change endianness of fsck_blk_map_page structure
 *
 * PARAMETERS:
 *      fsck_bmap_p         - fsck block map page structure
 *
 * RETURNS: NONE
 */
void ujfs_swap_fsck_blk_map_page(struct fsck_blk_map_page *fsck_bmap_p)
{
	int i;

	/* fsck_blk_map_page in libfs/fsckwsp.h */

	for (i = 0; i < 1024; i++)
		fsck_bmap_p->fsck_blkmap_words[i] =
		    __le32_to_cpu(fsck_bmap_p->fsck_blkmap_words[i]);
}

/*--------------------------------------------------------------------
 * NAME: ujfs_swap_iag
 *
 * FUNCTION: Change endianness of iag structure
 *
 * PARAMETERS:
 *      ia_t         - iag structure
 *
 * RETURNS: NONE
 */
void ujfs_swap_iag(struct iag *ia_t)
{
	int i;

	/* iag in jfs_logmgr.h */

	ia_t->agstart = __le64_to_cpu(ia_t->agstart);
	ia_t->iagnum = __le32_to_cpu(ia_t->iagnum);
	ia_t->inofreefwd = __le32_to_cpu(ia_t->inofreefwd);
	ia_t->inofreeback = __le32_to_cpu(ia_t->inofreeback);
	ia_t->extfreefwd = __le32_to_cpu(ia_t->extfreefwd);
	ia_t->extfreeback = __le32_to_cpu(ia_t->extfreeback);
	ia_t->iagfree = __le32_to_cpu(ia_t->iagfree);

	for (i = 0; i < SMAPSZ; i++) {
		ia_t->inosmap[i] = __le32_to_cpu(ia_t->inosmap[i]);
		ia_t->extsmap[i] = __le32_to_cpu(ia_t->extsmap[i]);
	}

	ia_t->nfreeinos = __le32_to_cpu(ia_t->nfreeinos);
	ia_t->nfreeexts = __le32_to_cpu(ia_t->nfreeexts);

	for (i = 0; i < EXTSPERIAG; i++) {
		ia_t->wmap[i] = __le32_to_cpu(ia_t->wmap[i]);
		ia_t->pmap[i] = __le32_to_cpu(ia_t->pmap[i]);
	}
}

/*--------------------------------------------------------------------
 * NAME: ujfs_swap_logpage
 *
 * FUNCTION: Change endianness of logpage structure
 *
 * PARAMETERS:
 *      lpage_t         - logpage structure
 *      numpages        - number of pages to swap
 *
 * RETURNS: NONE
 */
void ujfs_swap_logpage(struct logpage *lpage_t, uint8_t numpages)
{
	uint8_t i;
	struct logpage *lp_t;

	/* struct logpage in jfs_logmgr.h */

	lp_t = lpage_t;

	for (i = 0; i < numpages; i++) {

		lp_t->h.page = __le32_to_cpu(lp_t->h.page);
		lp_t->h.rsrvd = __le16_to_cpu(lp_t->h.rsrvd);
		lp_t->h.eor = __le16_to_cpu(lp_t->h.eor);

		lp_t->t.page = __le32_to_cpu(lp_t->t.page);
		lp_t->t.rsrvd = __le16_to_cpu(lp_t->t.rsrvd);
		lp_t->t.eor = __le16_to_cpu(lp_t->t.eor);

		lp_t++;
	}
}

/*--------------------------------------------------------------------
 * NAME: ujfs_swap_logsuper
 *
 * FUNCTION: Change endianness of logsuper structure
 *
 * PARAMETERS:
 *      lsup_t         - logsuper structure
 *
 * RETURNS: NONE
 */
void ujfs_swap_logsuper(struct logsuper *lsup_t)
{
	/* struct logsuper in jfs_logmgr.h */

	lsup_t->magic = __le32_to_cpu(lsup_t->magic);
	lsup_t->version = __le32_to_cpu(lsup_t->version);
	lsup_t->serial = __le32_to_cpu(lsup_t->serial);
	lsup_t->size = __le32_to_cpu(lsup_t->size);
	lsup_t->bsize = __le32_to_cpu(lsup_t->bsize);
	lsup_t->l2bsize = __le32_to_cpu(lsup_t->l2bsize);
	lsup_t->flag = __le32_to_cpu(lsup_t->flag);
	lsup_t->state = __le32_to_cpu(lsup_t->state);
	lsup_t->end = __le32_to_cpu(lsup_t->end);
}

/*--------------------------------------------------------------------
 * NAME: ujfs_swap_lrd
 *
 * FUNCTION: Change endianness of lrd structure
 *
 * PARAMETERS:
 *      lrd             - lrd structure
 *
 * RETURNS: NONE
 */
void ujfs_swap_lrd(struct lrd *lrd)
{
	lrd->logtid = __le32_to_cpu(lrd->logtid);
	lrd->backchain = __le32_to_cpu(lrd->backchain);
	lrd->type = __le16_to_cpu(lrd->type);
	lrd->length = __le16_to_cpu(lrd->length);
	lrd->aggregate = __le32_to_cpu(lrd->aggregate);

	switch (lrd->type) {
	case LOG_COMMIT:
		break;
	case LOG_SYNCPT:
		lrd->log.syncpt.sync = __le32_to_cpu(lrd->log.syncpt.sync);
		break;
	case LOG_MOUNT:
		break;
	case LOG_REDOPAGE:
	case LOG_NOREDOPAGE:
		lrd->log.redopage.fileset = __le32_to_cpu(lrd->log.redopage.fileset);
		lrd->log.redopage.inode = __le32_to_cpu(lrd->log.redopage.inode);
		lrd->log.redopage.type = __le16_to_cpu(lrd->log.redopage.type);
		lrd->log.redopage.l2linesize = __le16_to_cpu(lrd->log.redopage.l2linesize);
		break;
	case LOG_UPDATEMAP:
		lrd->log.updatemap.fileset = __le32_to_cpu(lrd->log.updatemap.fileset);
		lrd->log.updatemap.inode = __le32_to_cpu(lrd->log.updatemap.inode);
		lrd->log.updatemap.type = __le16_to_cpu(lrd->log.updatemap.type);
		lrd->log.updatemap.nxd = __le16_to_cpu(lrd->log.updatemap.nxd);
		break;
	case LOG_NOREDOINOEXT:
		lrd->log.noredoinoext.fileset = __le32_to_cpu(lrd->log.noredoinoext.fileset);
		lrd->log.noredoinoext.iagnum = __le32_to_cpu(lrd->log.noredoinoext.iagnum);
		lrd->log.noredoinoext.inoext_idx = __le32_to_cpu(lrd->log.noredoinoext.inoext_idx);
		break;
	}
}

/*--------------------------------------------------------------------
 * NAME: ujfs_swap_superblock
 *
 * FUNCTION: Change endianness of superblock structure
 *
 * PARAMETERS:
 *      sblk         - superblock structure
 *
 * RETURNS: NONE
 */
void ujfs_swap_superblock(struct superblock *sblk)
{
	/* superblock in jfs_superblock.h */

	sblk->s_version = __le32_to_cpu(sblk->s_version);
	sblk->s_size = __le64_to_cpu(sblk->s_size);
	sblk->s_bsize = __le32_to_cpu(sblk->s_bsize);
	sblk->s_l2bsize = __le16_to_cpu(sblk->s_l2bsize);
	sblk->s_l2bfactor = __le16_to_cpu(sblk->s_l2bfactor);
	sblk->s_pbsize = __le32_to_cpu(sblk->s_pbsize);
	sblk->s_l2pbsize = __le16_to_cpu(sblk->s_l2pbsize);
	sblk->pad = __le16_to_cpu(sblk->pad);
	sblk->s_agsize = __le32_to_cpu(sblk->s_agsize);
	sblk->s_flag = __le32_to_cpu(sblk->s_flag);
	sblk->s_state = __le32_to_cpu(sblk->s_state);
	sblk->s_compress = __le32_to_cpu(sblk->s_compress);

	sblk->s_logdev = __le32_to_cpu(sblk->s_logdev);
	sblk->s_logserial = __le32_to_cpu(sblk->s_logserial);

	/* struct timestruc_t s_time; */
	sblk->s_time.tv_sec = __le32_to_cpu(sblk->s_time.tv_sec);
	sblk->s_time.tv_nsec = __le32_to_cpu(sblk->s_time.tv_nsec);

	sblk->s_fsckloglen = __le32_to_cpu(sblk->s_fsckloglen);
	sblk->s_xsize = __le64_to_cpu(sblk->s_xsize);
}

/*--------------------------------------------------------------------
 * NAME: ujfs_swap_xtpage_t
 *
 * FUNCTION: Change endianness of xtpage_t structure
 *
 * PARAMETERS:
 *      xtp_t         - xtpage_t structure
 *      mode          - PUT or GET
 *
 * RETURNS: NONE
 */
void ujfs_swap_xtpage_t(xtpage_t * xtp_t)
{
	/* xtpage_t in jfs_xtree.h */

	/* struct xtheader header; */
	xtp_t->header.next = __le64_to_cpu(xtp_t->header.next);
	xtp_t->header.prev = __le64_to_cpu(xtp_t->header.prev);

	xtp_t->header.maxentry = __le16_to_cpu(xtp_t->header.maxentry);
	xtp_t->header.rsrvd2 = __le16_to_cpu(xtp_t->header.rsrvd2);

	xtp_t->header.nextindex = __le16_to_cpu(xtp_t->header.nextindex);
}

/*--------------------------------------------------------------------
 * NAME: ujfs_swap_fscklog_entry_hdr
 *
 * FUNCTION: Change endianness of fscklog_entry_hdr structure
 *
 * PARAMETERS:
 *      hdptr         - pointer to fscklog_entry_hdr structure
 *
 * RETURNS: NONE
 */
void ujfs_swap_fscklog_entry_hdr(struct fscklog_entry_hdr *hdptr)
{
	/* fscklog_entry_hdr in fscklog.h */

	hdptr->entry_length = __le16_to_cpu(hdptr->entry_length);
}
#endif				/* BIG ENDIAN */
