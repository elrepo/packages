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
#ifndef _H_UJFS_ENDIAN
#define _H_UJFS_ENDIAN

#include "jfs_types.h"
#include "jfs_byteorder.h"
#include "jfs_superblock.h"
#include "jfs_dmap.h"
#include "jfs_imap.h"
#include "jfs_dinode.h"
#include "jfs_logmgr.h"
#include "fsckwsp.h"

#if __BYTE_ORDER == __BIG_ENDIAN
void ujfs_swap_dbmap(struct dbmap *);
void ujfs_swap_dinode(struct dinode *, int32_t, uint32_t);
void ujfs_swap_dinomap(struct dinomap *);
void ujfs_swap_dmap(struct dmap *);
void ujfs_swap_dmapctl(struct dmapctl *);
void ujfs_swap_dtpage_t(dtpage_t *, uint32_t);
void ujfs_swap_fsck_blk_map_hdr(struct fsck_blk_map_hdr *);
void ujfs_swap_fsck_blk_map_page(struct fsck_blk_map_page *);
void ujfs_swap_fscklog_entry_hdr(struct fscklog_entry_hdr *);
void ujfs_swap_iag(struct iag *);
void ujfs_swap_logpage(struct logpage *, uint8_t);
void ujfs_swap_logsuper(struct logsuper *);
void ujfs_swap_lrd(struct lrd *);
void ujfs_swap_superblock(struct superblock *);
void ujfs_swap_xtpage_t(xtpage_t *);

static inline void ujfs_swap_inoext(struct dinode *ptr, int32_t mode, uint32_t flag)
{
	int i;
	for (i = 0; i < INOSPEREXT; i++, ptr++)
		ujfs_swap_dinode(ptr, mode, flag);
}

#define swap_multiple(swap_func, ptr, num)	\
do {						\
	int i;					\
	for (i = 0; i < num; i++, (ptr)++)	\
		swap_func(ptr);			\
} while (0)

#else				/* Little endian */

#define ujfs_swap_dbmap(dbmap)			do {} while (0)
#define ujfs_swap_dinode(dinode, mode, flag)	do {} while (0)
#define ujfs_swap_dinomap(dinomap)		do {} while (0)
#define ujfs_swap_dmap(dmap)			do {} while (0)
#define ujfs_swap_dmapctl(dmapctl)		do {} while (0)
#define ujfs_swap_dtpage_t(dtpage, flag)	do {} while (0)
#define ujfs_swap_fsck_blk_map_hdr(map_hdr)	do {} while (0)
#define ujfs_swap_fsck_blk_map_page(map_page)	do {} while (0)
#define ujfs_swap_fscklog_entry_hdr(ent_hdr)	do {} while (0)
#define ujfs_swap_iag(iag)			do {} while (0)
#define ujfs_swap_inoext(inoext, mode, flag)	do {} while (0)
#define ujfs_swap_logpage(logpage, pages)	do {} while (0)
#define ujfs_swap_logsuper(logsuper)		do {} while (0)
#define ujfs_swap_lrd(lrd)			do {} while (0)
#define ujfs_swap_superblock(sb)		do {} while (0)
#define ujfs_swap_xtpage_t(xtpage)		do {} while (0)
#define swap_multiple(swap_func, ptr, num)	do {} while (0)

#endif

#endif				/* _H_UJFS_ENDIAN */
