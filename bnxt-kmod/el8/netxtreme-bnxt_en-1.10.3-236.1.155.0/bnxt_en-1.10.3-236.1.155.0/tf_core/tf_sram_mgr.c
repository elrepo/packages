// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */
#include <linux/types.h>
#include <linux/vmalloc.h>
#include <linux/netdevice.h>
#include <linux/errno.h>
#include "tf_sram_mgr.h"
#include "tf_core.h"
#include "tf_rm.h"
#include "tf_util.h"
/***************************
 * Internal Data Structures
 ***************************/

/**
 * TF SRAM block info
 *
 * @prev:		Previous block
 * @next:		Next block
 * @in_use_mask:	Bitmap indicating which slices are in use
 *			If a bit is set, it indicates the slice
 *			in the row is in use.
 * @block_id:		Block id - this is a 64B offset
 *
 * Contains all the information about a particular 64B SRAM
 * block and the slices within it.
 */
struct tf_sram_block {
	struct tf_sram_block	*prev;
	struct tf_sram_block	*next;
	u8			in_use_mask;
	u16			block_id;
};

/**
 * TF SRAM block list
 *
 * @head:	Pointer to head of linked list of blocks.
 * @tail:	Pointer to tail of linked list of blocks.
 * @cnt:	Total count of blocks
 * @first_not_full_block:	First non-full block in the list
 * @size:	Entry slice size for this list
 *
 * List of 64B SRAM blocks used for fixed size slices (8, 16, 32, 64B)
 */
struct tf_sram_slice_list {
	struct tf_sram_block	*head;
	struct tf_sram_block	*tail;
	u32			cnt;
	struct tf_sram_block	*first_not_full_block;
	enum tf_sram_slice_size	size;
};

/* TF SRAM bank info consists of lists of different slice sizes per bank */
struct tf_sram_bank_info {
	struct tf_sram_slice_list slice[TF_SRAM_SLICE_SIZE_MAX];
};

/* SRAM banks consist of SRAM bank information */
struct tf_sram_bank {
	struct tf_sram_bank_info bank[TF_SRAM_BANK_ID_MAX];
};

/* SRAM banks consist of SRAM bank information */
struct tf_sram {
	struct tf_sram_bank dir[TF_DIR_MAX];
};

/* Internal functions */

/* Get slice size in string format */
const char *tf_sram_slice_2_str(enum tf_sram_slice_size slice_size)
{
	switch (slice_size) {
	case TF_SRAM_SLICE_SIZE_8B:
		return "8B slice";
	case TF_SRAM_SLICE_SIZE_16B:
		return "16B slice";
	case TF_SRAM_SLICE_SIZE_32B:
		return "32B slice";
	case TF_SRAM_SLICE_SIZE_64B:
		return "64B slice";
	default:
		return "Invalid slice size";
	}
}

/* Get bank in string format */
const char *tf_sram_bank_2_str(enum tf_sram_bank_id bank_id)
{
	switch (bank_id) {
	case TF_SRAM_BANK_ID_0:
		return "bank_0";
	case TF_SRAM_BANK_ID_1:
		return "bank_1";
	case TF_SRAM_BANK_ID_2:
		return "bank_2";
	case TF_SRAM_BANK_ID_3:
		return "bank_3";
	default:
		return "Invalid bank_id";
	}
}

/* TF SRAM get slice list */
static int tf_sram_get_slice_list(struct tf_sram *sram,
				  struct tf_sram_slice_list **slice_list,
				  enum tf_sram_slice_size slice_size,
				  enum tf_dir dir,
				  enum tf_sram_bank_id bank_id)
{
	if (!sram || !slice_list)
		return -EINVAL;

	*slice_list = &sram->dir[dir].bank[bank_id].slice[slice_size];
	return 0;
}

u16 tf_sram_bank_2_base_offset[TF_SRAM_BANK_ID_MAX] = {
	0,
	2048,
	4096,
	6144
};

/* Translate a block id and bank_id to an 8B offset */
static void tf_sram_block_id_2_offset(enum tf_sram_bank_id bank_id,
				      u16 block_id, u16 *offset)
{
	*offset = (block_id + tf_sram_bank_2_base_offset[bank_id]) << 3;
}

/* Translates an 8B offset and bank_id to a block_id */
static void tf_sram_offset_2_block_id(enum tf_sram_bank_id bank_id,
				      u16 offset, u16 *block_id,
				      u16 *slice_offset)
{
	*slice_offset = offset & 0x7;
	*block_id = ((offset & ~0x7) >> 3) -
			tf_sram_bank_2_base_offset[bank_id];
}

/* Find a matching block_id within the slice list */
static struct tf_sram_block *tf_sram_find_block(u16 block_id,
						struct tf_sram_slice_list
						*slice_list)
{
	struct tf_sram_block *block;
	u32 cnt;

	cnt = slice_list->cnt;
	block = slice_list->head;

	while (cnt > 0 && block) {
		if (block->block_id == block_id)
			return block;
		block = block->next;
		cnt--;
	}
	return NULL;
}

/* Given the current block get the next block within the slice list
 * List is not changed.
 */
static struct tf_sram_block *tf_sram_get_next_block(struct tf_sram_block *block)
{
	struct tf_sram_block *nblock;

	if (block)
		nblock = block->next;
	else
		nblock = NULL;
	return nblock;
}

/* Free an allocated slice from a block and if the block is empty,
 * return an indication so that the block can be freed.
 */
static int tf_sram_free_slice(enum tf_sram_slice_size slice_size,
			      u16 slice_offset, struct tf_sram_block *block,
			      bool *block_is_empty)
{
	u8 slice_mask = 0;
	int rc = 0;
	u8 shift;

	if (!block || !block_is_empty)
		return -EINVAL;

	switch (slice_size) {
	case TF_SRAM_SLICE_SIZE_8B:
		shift = slice_offset >> 0;
		WARN_ON(!(shift < 8));
		slice_mask = 1 << shift;
		break;

	case TF_SRAM_SLICE_SIZE_16B:
		shift = slice_offset >> 1;
		WARN_ON(!(shift < 4));
		slice_mask = 1 << shift;
		break;

	case TF_SRAM_SLICE_SIZE_32B:
		shift = slice_offset >> 2;
		WARN_ON(!(shift < 2));
		slice_mask = 1 << shift;
		break;

	case TF_SRAM_SLICE_SIZE_64B:
	default:
		shift = slice_offset >> 0;
		WARN_ON(!(shift < 1));
		slice_mask = 1 << shift;
		break;
	}

	if ((block->in_use_mask & slice_mask) == 0) {
		rc = -EINVAL;
		netdev_dbg(NULL,
			   "block_id(0x%x) slice(%d) was not allocated\n",
			   block->block_id, slice_offset);
		return rc;
	}

	block->in_use_mask &= ~slice_mask;

	if (block->in_use_mask == 0)
		*block_is_empty = true;
	else
		*block_is_empty = false;

	return rc;
}

/* TF SRAM get next slice
 * Gets the next slice_offset available in the block
 * and updates the in_use_mask.
 */
static int tf_sram_get_next_slice_in_block(struct tf_sram_block *block,
					   enum tf_sram_slice_size slice_size,
					   u16 *slice_offset,
					   bool *block_is_full)
{
	u8 shift, max_slices, mask, i, full_mask;
	int rc, free_id = -1;

	if (!block || !slice_offset || !block_is_full)
		return -EINVAL;

	switch (slice_size) {
	case TF_SRAM_SLICE_SIZE_8B:
		shift      = 0;
		max_slices = 8;
		full_mask  = 0xff;
		break;
	case TF_SRAM_SLICE_SIZE_16B:
		shift      = 1;
		max_slices = 4;
		full_mask  = 0xf;
		break;
	case TF_SRAM_SLICE_SIZE_32B:
		shift      = 2;
		max_slices = 2;
		full_mask  = 0x3;
		break;
	case TF_SRAM_SLICE_SIZE_64B:
	default:
		shift      = 0;
		max_slices = 1;
		full_mask  = 1;
		break;
	}

	mask = block->in_use_mask;

	for (i = 0; i < max_slices; i++) {
		if ((mask & 1) == 0) {
			free_id = i;
			block->in_use_mask |= 1 << free_id;
			break;
		}
		mask = mask >> 1;
	}

	if (block->in_use_mask == full_mask)
		*block_is_full = true;
	else
		*block_is_full = false;

	if (free_id >= 0) {
		*slice_offset = free_id << shift;
		rc = 0;
	} else {
		*slice_offset = 0;
		rc = -ENOMEM;
	}

	return rc;
}

/* TF SRAM get indication as to whether the slice offset is
 * allocated in the block.
 */
static int tf_sram_is_slice_allocated_in_block(struct tf_sram_block *block,
					       enum tf_sram_slice_size
					       slice_size, u16 slice_offset,
					       bool *is_allocated)
{
	u8 slice_mask = 0;
	int rc = 0;
	u8 shift;

	if (!block || !is_allocated)
		return -EINVAL;

	*is_allocated = false;

	switch (slice_size) {
	case TF_SRAM_SLICE_SIZE_8B:
		shift = slice_offset >> 0;
		WARN_ON(!(shift < 8));
		slice_mask = 1 << shift;
		break;

	case TF_SRAM_SLICE_SIZE_16B:
		shift = slice_offset >> 1;
		WARN_ON(!(shift < 4));
		slice_mask = 1 << shift;
		break;

	case TF_SRAM_SLICE_SIZE_32B:
		shift = slice_offset >> 2;
		WARN_ON(!(shift < 2));
		slice_mask = 1 << shift;
		break;

	case TF_SRAM_SLICE_SIZE_64B:
	default:
		shift = slice_offset >> 0;
		WARN_ON(!(shift < 1));
		slice_mask = 1 << shift;
		break;
	}

	if ((block->in_use_mask & slice_mask) == 0) {
		netdev_dbg(NULL,
			   "block_id(0x%x) slice(%d) was not allocated\n",
			   block->block_id, slice_offset);
		*is_allocated = false;
	} else {
		*is_allocated = true;
	}

	return rc;
}

/* Get the block count */
static u32 tf_sram_get_block_cnt(struct tf_sram_slice_list *slice_list)
{
	return slice_list->cnt;
}

/* Free a block data structure - does not free to the RM */
static void tf_sram_free_block(struct tf_sram_slice_list *slice_list,
			       struct tf_sram_block *block)
{
	if (slice_list->head == block && slice_list->tail == block) {
		slice_list->head = NULL;
		slice_list->tail = NULL;
	} else if (slice_list->head == block) {
		slice_list->head = block->next;
		slice_list->head->prev = NULL;
	} else if (slice_list->tail == block) {
		slice_list->tail = block->prev;
		slice_list->tail->next = NULL;
	} else {
		block->prev->next = block->next;
		block->next->prev = block->prev;
	}
	vfree(block);
	slice_list->cnt--;
}

/* Free the entire slice_list */
static void tf_sram_free_slice_list(struct tf_sram_slice_list *slice_list)
{
	struct tf_sram_block *nblock, *block;
	u32 i, block_cnt;

	block_cnt = tf_sram_get_block_cnt(slice_list);
	block = slice_list->head;

	for (i = 0; i < block_cnt; i++) {
		nblock = block->next;
		tf_sram_free_block(slice_list, block);
		block = nblock;
	}
}

/* Allocate a single SRAM block from memory and add it to the slice list */
static struct tf_sram_block *tf_sram_alloc_block(struct tf_sram_slice_list
						 *slice_list, u16 block_id)
{
	struct tf_sram_block *block;

	block = vzalloc(sizeof(*block));
	if (!block)
		return NULL;

	block->block_id = block_id;

	if (!slice_list->head) {
		slice_list->head = block;
		slice_list->tail = block;
		block->next = NULL;
		block->prev = NULL;
	} else {
		block->next = slice_list->head;
		block->prev = NULL;
		block->next->prev = block;
		slice_list->head = block->next->prev;
	}
	slice_list->cnt++;
	return block;
}

/* Find the first not full block in the slice list */
static void tf_sram_find_first_not_full_block(struct tf_sram_slice_list
					      *slice_list,
					      enum tf_sram_slice_size
					      slice_size,
					      struct tf_sram_block
					      **first_not_full_block)
{
	struct tf_sram_block *block = slice_list->head;
	u8 slice_mask, mask;

	switch (slice_size) {
	case TF_SRAM_SLICE_SIZE_8B:
		slice_mask = 0xff;
		break;

	case TF_SRAM_SLICE_SIZE_16B:
		slice_mask = 0xf;
		break;

	case TF_SRAM_SLICE_SIZE_32B:
		slice_mask = 0x3;
		break;

	case TF_SRAM_SLICE_SIZE_64B:
	default:
		slice_mask = 0x1;
		break;
	}

	*first_not_full_block = NULL;

	while (block) {
		mask = block->in_use_mask & slice_mask;
		if (mask != slice_mask) {
			*first_not_full_block = block;
			break;
		}
		block = block->next;
	}
}

static void tf_sram_dump_block(struct tf_sram_block *block)
{
	netdev_dbg(NULL, "block_id(0x%x) in_use_mask(0x%02x)\n",
		   block->block_id, block->in_use_mask);
}

/* External functions */
int tf_sram_mgr_bind(void **sram_handle)
{
	struct tf_sram *sram;
	int rc = 0;

	if (!sram_handle)
		return -EINVAL;

	sram = vzalloc(sizeof(*sram));
	if (!sram)
		return -ENOMEM;

	*sram_handle = sram;
	return rc;
}

int tf_sram_mgr_unbind(void *sram_handle)
{
	struct tf_sram_slice_list *slice_list;
	enum tf_sram_slice_size slice_size;
	enum tf_sram_bank_id bank_id;
	struct tf_sram *sram;
	enum tf_dir dir;
	int rc = 0;

	if (!sram_handle)
		return -EINVAL;

	sram = (struct tf_sram *)sram_handle;

	for (dir = 0; dir < TF_DIR_MAX; dir++) {
		/* For each bank */
		for (bank_id = TF_SRAM_BANK_ID_0;
		     bank_id < TF_SRAM_BANK_ID_MAX;
		     bank_id++) {
			/* For each slice size */
			for (slice_size = TF_SRAM_SLICE_SIZE_8B;
			     slice_size < TF_SRAM_SLICE_SIZE_MAX;
			     slice_size++) {
				rc = tf_sram_get_slice_list(sram, &slice_list,
							    slice_size, dir,
							    bank_id);
				if (rc) {
					/* Log error */
					netdev_dbg(NULL,
						   "No SRAM slice list:%d\n",
						   rc);
					return rc;
				}
				if (tf_sram_get_block_cnt(slice_list))
					tf_sram_free_slice_list(slice_list);
			}
		}
	}

	vfree(sram);

	/* Freeing of the RM resources is handled by the table manager */
	return rc;
}

int tf_sram_mgr_alloc(void *sram_handle, struct tf_sram_mgr_alloc_parms *parms)
{
	struct tf_rm_allocate_parms aparms = { 0 };
	struct tf_sram_slice_list *slice_list;
	u16 block_id, slice_offset = 0;
	struct tf_sram_block *block;
	struct tf_sram *sram;
	bool block_is_full;
	u16 block_offset;
	int rc = 0;
	u32 index;

	if (!sram_handle || !parms || !parms->sram_offset)
		return -EINVAL;

	sram = (struct tf_sram *)sram_handle;

	/* Check the current slice list */
	rc = tf_sram_get_slice_list(sram, &slice_list, parms->slice_size,
				    parms->dir, parms->bank_id);
	if (rc) {
		/* Log error */
		netdev_dbg(NULL, "No SRAM slice list:%d\n", rc);
		return rc;
	}

	/* If the list is empty or all entries are full allocate a new block */
	if (!slice_list->first_not_full_block) {
		/* Allocate and insert a new block */
		aparms.index = &index;
		aparms.subtype = parms->tbl_type;
		aparms.rm_db = parms->rm_db;
		rc = tf_rm_allocate(&aparms);
		if (rc)
			return rc;

		block_id = index;
		block = tf_sram_alloc_block(slice_list, block_id);
	} else {
		/* Block exists */
		block =
		 (struct tf_sram_block *)(slice_list->first_not_full_block);
	}
	rc = tf_sram_get_next_slice_in_block(block,
					     parms->slice_size,
					     &slice_offset,
					     &block_is_full);

	/* Find the new first non-full block in the list */
	tf_sram_find_first_not_full_block(slice_list,
					  parms->slice_size,
					  &slice_list->first_not_full_block);

	tf_sram_block_id_2_offset(parms->bank_id, block->block_id, &block_offset);

	*parms->sram_offset = block_offset + slice_offset;
	return rc;
}

int tf_sram_mgr_free(void *sram_handle, struct tf_sram_mgr_free_parms *parms)
{
	struct tf_rm_free_parms fparms = { 0 };
	struct tf_sram_slice_list *slice_list;
	struct tf_sram_block *block;
	u16 block_id, slice_offset;
	struct tf_sram *sram;
	bool block_is_empty;
	int rc = 0;

	if  (!sram_handle || !parms)
		return -EINVAL;

	sram = (struct tf_sram *)sram_handle;

	/* Check the current slice list */
	rc = tf_sram_get_slice_list(sram, &slice_list, parms->slice_size,
				    parms->dir, parms->bank_id);
	if (rc) {
		/* Log error */
		netdev_dbg(NULL, "No SRAM slice list:%d\n", rc);
		return rc;
	}

	/* Determine the block id and slice offset from the SRAM offset */
	tf_sram_offset_2_block_id(parms->bank_id, parms->sram_offset, &block_id,
				  &slice_offset);

	/* Search the list of blocks for the matching block id */
	block = tf_sram_find_block(block_id, slice_list);
	if (!block) {
		netdev_dbg(NULL, "block not found 0x%x\n", block_id);
		return rc;
	}

	/* If found, search for the matching SRAM slice in use. */
	rc = tf_sram_free_slice(parms->slice_size, slice_offset,
				block, &block_is_empty);
	if (rc) {
		netdev_dbg(NULL, "Error freeing slice (%d)\n", rc);
		return rc;
	}
	/* If the block is empty, free the block to the RM */
	if (block_is_empty) {
		fparms.rm_db = parms->rm_db;
		fparms.subtype = parms->tbl_type;
		fparms.index = block_id;
		rc = tf_rm_free(&fparms);

		if (rc) {
			netdev_dbg(NULL,
				   "Free block_id(%d) failed error(%d)\n",
				   block_id, rc);
		}
		/* Free local entry regardless */
		tf_sram_free_block(slice_list, block);

		/* Clear the not full block to set it again */
		slice_list->first_not_full_block = NULL;
	}
	if (slice_list->first_not_full_block)
		return rc;

	/* set the non full block so it can be used in next alloc */
	tf_sram_find_first_not_full_block(slice_list,
					  parms->slice_size,
					  &slice_list->first_not_full_block);

	return rc;
}

int tf_sram_mgr_dump(void *sram_handle, struct tf_sram_mgr_dump_parms *parms)
{
	struct tf_sram_slice_list *slice_list;
	struct tf_sram_block *block;
	struct tf_sram *sram;
	u32 block_cnt, i;
	int rc = 0;

	if (!sram_handle || !parms)
		return -EINVAL;

	sram = (struct tf_sram *)sram_handle;

	rc = tf_sram_get_slice_list(sram, &slice_list, parms->slice_size,
				    parms->dir, parms->bank_id);
	if (rc)
		return rc;

	if (slice_list->cnt || slice_list->first_not_full_block) {
		netdev_dbg(NULL, "\n********** %s: %s: %s ***********\n",
			   tf_sram_bank_2_str(parms->bank_id),
			   tf_dir_2_str(parms->dir),
			   tf_sram_slice_2_str(parms->slice_size));

		block_cnt = tf_sram_get_block_cnt(slice_list);
		netdev_dbg(NULL, "block_cnt(%d)\n", block_cnt);
		if (slice_list->first_not_full_block)
			netdev_dbg(NULL, "first_not_full_block(0x%x)\n",
				   slice_list->first_not_full_block->block_id);
		block = slice_list->head;
		for (i = 0; i < block_cnt; i++) {
			tf_sram_dump_block(block);
			block = tf_sram_get_next_block(block);
		}
		netdev_dbg(NULL, "*********************************\n");
	}
	return rc;
}

/**
 * Validate an SRAM Slice is allocated
 *
 * Validate whether the SRAM slice is allocated
 *
 * @sram_handle:	Pointer to SRAM handle
 * @parms:		Pointer to the SRAM alloc parameters
 *
 * Returns
 *   - (0) if successful
 *   - (-EINVAL) on failure
 *
 */
int tf_sram_mgr_is_allocated(void *sram_handle,
			     struct tf_sram_mgr_is_allocated_parms *parms)
{
	struct tf_sram_slice_list *slice_list;
	struct tf_sram_block *block;
	u16 block_id, slice_offset;
	struct tf_sram *sram;
	int rc = 0;

	if (!sram_handle || !parms || !parms->is_allocated)
		return -EINVAL;

	sram = (struct tf_sram *)sram_handle;

	/* Check the current slice list */
	rc = tf_sram_get_slice_list(sram, &slice_list, parms->slice_size,
				    parms->dir, parms->bank_id);
	if (rc) {
		/* Log error */
		netdev_dbg(NULL, "No SRAM slice list, rc:%d\n", rc);
		return rc;
	}

	/* If the list is empty, then it cannot be allocated */
	if (!slice_list->cnt) {
		netdev_dbg(NULL, "List is empty for %s:%s:%s\n",
			   tf_dir_2_str(parms->dir),
			   tf_sram_slice_2_str(parms->slice_size),
			   tf_sram_bank_2_str(parms->bank_id));

		parms->is_allocated = NULL;
		goto done;
	}

	/* Determine the block id and slice offset from the SRAM offset */
	tf_sram_offset_2_block_id(parms->bank_id, parms->sram_offset, &block_id,
				  &slice_offset);

	/* Search the list of blocks for the matching block id */
	block = tf_sram_find_block(block_id, slice_list);
	if (!block) {
		netdev_dbg(NULL, "block not found in list 0x%x\n",
			   parms->sram_offset);
		parms->is_allocated = NULL;
		goto done;
	}

	rc = tf_sram_is_slice_allocated_in_block(block,
						 parms->slice_size,
						 slice_offset,
						 parms->is_allocated);
done:
	return rc;
}
