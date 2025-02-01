/*
 *   Copyright (C) International Business Machines Corp., 2004,2005
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
#include <errno.h>
/* defines and includes common among the fsck.jfs modules */
#include "xfsckint.h"
#include "devices.h"
#include "message.h"

/* + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + +
 *
 * superblock buffer pointer
 *
 *      defined in xchkdsk.c
 */
extern struct superblock *sb_ptr;

/* + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + +
 *
 * fsck aggregate info structure pointer
 *
 *      defined in xchkdsk.c
 */
extern struct fsck_agg_record *agg_recptr;

/* VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV
 *
 * The following are internal to this file
 *
 */

/*
 * Since access to the directory index may not be sequential, keep a number
 * of pages resident
 */
#define NUM_INDEX_BUFS 16

struct dir_index_page {
	long long address;
	struct dir_index_page *next;
	struct dir_index_page *prev;
	int flag;
	struct dir_table_slot table[512];
};

#define DIPAGE_DIRTY 1

struct dir_index_page *dir_index_buffers;

struct dir_index_page *free_di_page;	/* free list */
struct dir_index_page *lru_di_page;	/* least-recently-used */
struct dir_index_page *mru_di_page;	/* most-recently-used */

int allocate_dir_index_buffers(void)
{
	int i;
	struct dir_index_page *page, *last;

	dir_index_buffers = malloc(sizeof(struct dir_index_page) *
				   NUM_INDEX_BUFS);
	if (! dir_index_buffers) {
		fsck_send_msg(MSG_OSO_INSUFF_MEMORY);
		return ENOMEM;
	}

	page = dir_index_buffers;
	last = NULL;

	for (i = 0; i < NUM_INDEX_BUFS; i++) {
		page->address = 0;
		page->next = last;
		page->prev = NULL;	/* Not used on free list */
		page->flag = 0;
		last = page;
		page++;
	}
	free_di_page = last;
	lru_di_page = mru_di_page = NULL;

	return FSCK_OK;
}

static int write_index_page(struct dir_index_page *page)
{
	int rc;

	rc = ujfs_rw_diskblocks(Dev_IOPort, page->address << sb_ptr->s_l2bsize,
				PSIZE, page->table, PUT);

	return rc;
}

static struct dir_table_slot *read_index_page(struct dinode *inoptr,
					      uint cookie)
{
	long long address;
	int64_t blkno;
	int8_t match_found;
	int64_t offset;
	struct dir_index_page *page;
	int rc;
	xad_t *xad_ptr;

	offset = (cookie - 2) * sizeof (struct dir_table_slot);
	blkno = (offset >> L2PSIZE) << (L2PSIZE - sb_ptr->s_l2bsize);

	rc = xTree_search(inoptr, blkno, &xad_ptr, &match_found);

	if (rc || !match_found)
		return NULL;

	address = addressXAD(xad_ptr);

	/* Search cache.  Is it worthwhile to build a hash? */
	for (page = mru_di_page; page; page = page->next) {
		if (page->address == address) {
			/* hit! */
			if (page != mru_di_page) {
				/* remove from linked list */
				if (page == lru_di_page)
					lru_di_page = page->prev;
				else
					page->next->prev = page->prev;
				page->prev->next = page->next;
				/* add back at front (mru) */
				mru_di_page->prev = page;
				page->next = mru_di_page;
				page->prev = NULL;
				mru_di_page = page;
			}
			return page->table;
		}
	}
	if (free_di_page) {
		page = free_di_page;
		free_di_page = page->next;
	} else {
		page = lru_di_page;
		if (page->flag & DIPAGE_DIRTY)
			write_index_page(page);
		lru_di_page = page->prev;
		lru_di_page->next = NULL;
	}
	page->address = address;
	page->flag = 0;
	rc = ujfs_rw_diskblocks(Dev_IOPort, address << sb_ptr->s_l2bsize,
				PSIZE, page->table, GET);
	if (rc) {
		page->next = free_di_page;
		free_di_page = page;
		return NULL;
	}

	/* Insert into lru list */
	page->next = mru_di_page;
	page->prev = NULL;
	mru_di_page = page;
	if (page->next)
		page->next->prev = page;
	else
		lru_di_page = page;

	return page->table;
}

int flush_index_pages()
{
	struct dir_index_page *page;
	int rc = 0, rc2;

	for (page = mru_di_page; page; page = page->next) {
		if (page->flag & DIPAGE_DIRTY) {
			rc2 = write_index_page(page);
			if (!rc)
				rc = rc2;
			page->flag &= ~DIPAGE_DIRTY;
		}
	}
	return rc;
}

static void dirty_index_page(struct dir_table_slot *table)
{
	/* Should only be called on most recent page */
	if (table != mru_di_page->table) {
		/* Really shoudn't happen. */
		printf("You messed up, Shaggy.  write_index_page mismatch.\n");
		return;
	}
	mru_di_page->flag |= DIPAGE_DIRTY;
}

void verify_dir_index(struct fsck_inode_record *inorecptr,
		      struct dinode *inoptr,
		      struct dtreeQelem *this_Qel,
		      int dtindex,
		      uint cookie)
{
	int64_t page_addr;
	struct dir_table_slot *slot;
	struct dir_table_slot *table;

	if (!inorecptr->check_dir_index)
		return;

	/*
	 * FIXME: Don't treat cookie == 0 as an error right now since the
	 * file system will fix this at runtime.  When fsck rebuilds the
	 * index entirely, we will want to go ahead and rebuild if a zero
	 * is found.
	 */
	if (cookie == 0)
		return;

	if ((cookie < 2) || (cookie >= inoptr->di_next_index)) {
		fsck_send_msg(fsck_BAD_COOKIE);
		goto error_found;
	}

	if (inoptr->di_next_index <= (MAX_INLINE_DIRTABLE_ENTRY + 1)) {
		table = inoptr->di_dirtable;
		slot = &table[cookie - 2];
	} else {
		table = read_index_page(inoptr, cookie);
		if (!table) {
			fsck_send_msg(fsck_READ_FAILED);
			goto error_found;
		}
		slot = &table[(cookie - 2) % 512];
	}

	if (this_Qel->node_level)
		page_addr = this_Qel->node_addr;
	else
		page_addr = 0;

	if ((slot->flag == 0) || (slot->slot != dtindex) ||
	    (addressDTS(slot) != page_addr)) {
		fsck_send_msg(fsck_BAD_ENTRY);
		goto error_found;
	}
	return;

error_found:
	inorecptr->check_dir_index = 0;
	inorecptr->rebuild_dirtable = 1;
	agg_recptr->corrections_needed = 1;
	return;
}

void modify_index(struct dinode *inoptr,
		  int64_t page_addr,
		  int dtindex,
		  uint cookie)
{
	struct dir_table_slot *slot;
	struct dir_table_slot *table;

	if (cookie < 2 || (cookie >= inoptr->di_next_index))
		return;

	if (inoptr->di_next_index <= (MAX_INLINE_DIRTABLE_ENTRY + 1)) {
		slot = &inoptr->di_dirtable[cookie - 2];
		DTSaddress(slot, page_addr);
		slot->slot = dtindex;
		return;
	}
	table = read_index_page(inoptr, cookie);
	if (!table)
		return;

	slot = &table[(cookie - 2) % 512];
	DTSaddress(slot, page_addr);
	slot->slot = dtindex;

	dirty_index_page(table);
}
