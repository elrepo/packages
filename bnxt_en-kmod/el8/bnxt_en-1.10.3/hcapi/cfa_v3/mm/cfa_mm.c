// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/string.h>
#include "sys_util.h"
#include "cfa_util.h"
#include "cfa_types.h"
#include "cfa_mm.h"
#include "bnxt_compat.h"

#define CFA_MM_SIGNATURE 0xCFA66C89

#define CFA_MM_INVALID8 U8_MAX
#define CFA_MM_INVALID16 U16_MAX
#define CFA_MM_INVALID32 U32_MAX
#define CFA_MM_INVALID64 U64_MAX

#define CFA_MM_MAX_RECORDS (64 * 1024 * 1024)
#define CFA_MM_MAX_CONTIG_RECORDS 8
#define CFA_MM_RECORDS_PER_BYTE 8
#define CFA_MM_MIN_RECORDS_PER_BLOCK 8

/* CFA Records block
 *
 * Structure used to store the CFA record block info
 */
struct cfa_mm_blk {
	/* Index of the previous block in the list */
	u32 prev_blk_idx;
	/* Index of the next block in the list */
	u32 next_blk_idx;
	/* Number of free records available in the block */
	u16 num_free_records;
	/* Location of first free record in the block */
	u16 first_free_record;
	/* Number of contiguous records */
	u16 num_contig_records;
	/* Reserved for future use */
	u16 reserved;
};

/* CFA Record block list
 *
 *  Structure used to store CFA Record block list info
 */
struct cfa_mm_blk_list {
	/* Index of the first block in the list */
	u32 first_blk_idx;
	/* Index of the last block in the list */
	u32 last_blk_idx;
};

/* CFA memory manager Database
 *
 *  Structure used to store CFA memory manager database info
 */
struct cfa_mm {
	/* Signature of the CFA Memory Manager Database */
	u32 signature;
	/* Maximum number of CFA Records */
	u32 max_records;
	/* Number of CFA Records in use*/
	u32 records_in_use;
	/* Number of Records per block */
	u16 records_per_block;
	/* Maximum number of contiguous records */
	u16 max_contig_records;
	/**
	 * Block list table stores the info of lists of blocks
	 * for various numbers of contiguous records
	 */
	struct cfa_mm_blk_list *blk_list_tbl;
	/**
	 * Block table stores the info about the blocks of CFA Records
	 */
	struct cfa_mm_blk *blk_tbl;
	/**
	 * Block bitmap table stores bit maps for the blocks of CFA Records
	 */
	u8 *blk_bmap_tbl;
};

static void cfa_mm_db_info(u32 max_records, u16 max_contig_records,
			   u16 *records_per_block, u32 *num_blocks,
			   u16 *num_lists, u32 *db_size)
{
	*records_per_block =
		MAX(CFA_MM_MIN_RECORDS_PER_BLOCK, max_contig_records);

	*num_blocks = (max_records / (*records_per_block));

	*num_lists = CFA_ALIGN_LN2(max_contig_records) + 1;

	*db_size = sizeof(struct cfa_mm) +
		   ((*num_blocks) * NUM_ALIGN_UNITS(*records_per_block,
						    CFA_MM_RECORDS_PER_BYTE)) +
		   ((*num_blocks) * sizeof(struct cfa_mm_blk)) +
		   ((*num_lists) * sizeof(struct cfa_mm_blk_list));
}

int cfa_mm_query(struct cfa_mm_query_parms *parms)
{
	u16 max_contig_records, num_lists, records_per_block;
	u32 max_records, num_blocks;

	if (!parms) {
		netdev_dbg(NULL, "parms = %p\n", parms);
		return -EINVAL;
	}

	max_records = parms->max_records;
	max_contig_records = (u16)parms->max_contig_records;

	/* Align to max_contig_records */
	max_records = (max_records + (max_contig_records - 1)) &
		      ~(max_contig_records - 1);

	if (!(CFA_CHECK_BOUNDS(max_records, 1, CFA_MM_MAX_RECORDS) &&
	      is_power_of_2(max_contig_records) &&
	      CFA_CHECK_BOUNDS(max_contig_records, 1,
			       CFA_MM_MAX_CONTIG_RECORDS))) {
		netdev_dbg(NULL, "parms = %p, max_records = %d, max_contig_records = %d\n",
			   parms, parms->max_records,
			   parms->max_contig_records);
		return -EINVAL;
	}

	cfa_mm_db_info(max_records, max_contig_records, &records_per_block,
		       &num_blocks, &num_lists, &parms->db_size);

	return 0;
}

int cfa_mm_open(void *cmm, struct cfa_mm_open_parms *parms)
{
	u16 max_contig_records, num_lists, records_per_block;
	struct cfa_mm *context = (struct cfa_mm *)cmm;
	u32 max_records, num_blocks, db_size, i;

	if (!cmm || !parms) {
		netdev_dbg(NULL, "cmm = %p, parms = %p\n", cmm, parms);
		return -EINVAL;
	}

	max_records = parms->max_records;
	max_contig_records = (u16)parms->max_contig_records;

	/* Align to max_contig_records */
	max_records = (max_records + (max_contig_records - 1)) &
		      ~(max_contig_records - 1);

	if (!(CFA_CHECK_BOUNDS(max_records, 1, CFA_MM_MAX_RECORDS) &&
	      is_power_of_2(max_contig_records) &&
	      CFA_CHECK_BOUNDS(max_contig_records, 1,
			       CFA_MM_MAX_CONTIG_RECORDS))) {
		netdev_dbg(NULL, "cmm = %p, parms = %p, db_mem_size = %d, ",
			   cmm, parms, parms->db_mem_size);
		netdev_dbg(NULL, "max_records = %d max_contig_records = %d\n",
			   max_records, max_contig_records);
		return -EINVAL;
	}

	cfa_mm_db_info(max_records, max_contig_records, &records_per_block,
		       &num_blocks, &num_lists, &db_size);

	if (parms->db_mem_size < db_size) {
		netdev_dbg(NULL, "cmm = %p, parms = %p, db_mem_size = %d, ",
			   cmm, parms, parms->db_mem_size);
		netdev_dbg(NULL, "max_records = %d max_contig_records = %d\n",
			   max_records, max_contig_records);
		return -EINVAL;
	}

	memset(context, 0, parms->db_mem_size);

	context->signature = CFA_MM_SIGNATURE;
	context->max_records = max_records;
	context->records_in_use = 0;
	context->records_per_block = records_per_block;
	context->max_contig_records = max_contig_records;

	context->blk_list_tbl = (struct cfa_mm_blk_list *)(context + 1);
	context->blk_tbl =
		(struct cfa_mm_blk *)(context->blk_list_tbl + num_lists);
	context->blk_bmap_tbl = (u8 *)(context->blk_tbl + num_blocks);

	context->blk_list_tbl[0].first_blk_idx = 0;
	context->blk_list_tbl[0].last_blk_idx = 0;

	for (i = 1; i < num_lists; i++) {
		context->blk_list_tbl[i].first_blk_idx = CFA_MM_INVALID32;
		context->blk_list_tbl[i].last_blk_idx = CFA_MM_INVALID32;
	}

	for (i = 0; i < num_blocks; i++) {
		context->blk_tbl[i].prev_blk_idx = i - 1;
		context->blk_tbl[i].next_blk_idx = i + 1;
		context->blk_tbl[i].num_free_records = records_per_block;
		context->blk_tbl[i].first_free_record = 0;
		context->blk_tbl[i].num_contig_records = 0;
	}

	context->blk_tbl[num_blocks - 1].next_blk_idx = CFA_MM_INVALID32;

	memset(context->blk_bmap_tbl, 0,
	       num_blocks * NUM_ALIGN_UNITS(records_per_block,
					    CFA_MM_RECORDS_PER_BYTE));

	return 0;
}

int cfa_mm_close(void *cmm)
{
	struct cfa_mm *context = (struct cfa_mm *)cmm;
	u16 num_lists, records_per_block;
	u32 db_size, num_blocks;

	if (!cmm || context->signature != CFA_MM_SIGNATURE) {
		netdev_err(NULL, "cmm = %p\n", cmm);
		return -EINVAL;
	}

	cfa_mm_db_info(context->max_records, context->max_contig_records,
		       &records_per_block, &num_blocks, &num_lists, &db_size);

	memset(cmm, 0, db_size);

	return 0;
}

/* Allocate a block idx from the free list */
static u32 cfa_mm_blk_alloc(struct cfa_mm *context)
{
	struct cfa_mm_blk_list *free_list;
	u32 blk_idx;

	free_list = context->blk_list_tbl;

	blk_idx = free_list->first_blk_idx;

	if (blk_idx == CFA_MM_INVALID32) {
		netdev_err(NULL, "Out of record blocks\n");
		return CFA_MM_INVALID32;
	}

	free_list->first_blk_idx =
		context->blk_tbl[free_list->first_blk_idx].next_blk_idx;

	if (free_list->first_blk_idx != CFA_MM_INVALID32) {
		context->blk_tbl[free_list->first_blk_idx].prev_blk_idx =
			CFA_MM_INVALID32;
	}

	context->blk_tbl[blk_idx].prev_blk_idx = CFA_MM_INVALID32;
	context->blk_tbl[blk_idx].next_blk_idx = CFA_MM_INVALID32;

	return blk_idx;
}

/* Return a block index to the free list */
static void cfa_mm_blk_free(struct cfa_mm *context, u32 blk_idx)
{
	struct cfa_mm_blk_list *free_list = context->blk_list_tbl;

	context->blk_tbl[blk_idx].prev_blk_idx = CFA_MM_INVALID32;
	context->blk_tbl[blk_idx].next_blk_idx = free_list->first_blk_idx;
	context->blk_tbl[blk_idx].num_free_records = context->records_per_block;
	context->blk_tbl[blk_idx].first_free_record = 0;
	context->blk_tbl[blk_idx].num_contig_records = 0;

	if (free_list->first_blk_idx != CFA_MM_INVALID32) {
		context->blk_tbl[free_list->first_blk_idx].prev_blk_idx =
			blk_idx;
	}

	free_list->first_blk_idx = blk_idx;
}

/* Insert at the top of a non-free list */
static void cfa_mm_blk_insert(struct cfa_mm *context,
			      struct cfa_mm_blk_list *blk_list,
			      u32 blk_idx)
{
	/* there are no entries in the list so init all to this one */
	if (blk_list->first_blk_idx == CFA_MM_INVALID32) {
		blk_list->first_blk_idx = blk_idx;
		blk_list->last_blk_idx = blk_idx;
	} else {
		struct cfa_mm_blk *blk_info = &context->blk_tbl[blk_idx];

		blk_info->prev_blk_idx = CFA_MM_INVALID32;
		blk_info->next_blk_idx = blk_list->first_blk_idx;
		context->blk_tbl[blk_list->first_blk_idx].prev_blk_idx =
			blk_idx;
		blk_list->first_blk_idx = blk_idx;
	}
}

/* insert at the bottom of a non-free list */
static void cfa_mm_blk_insert_last(struct cfa_mm *context,
				   struct cfa_mm_blk_list *blk_list,
				   uint32_t blk_idx)
{
	if (blk_list->last_blk_idx == CFA_MM_INVALID32) {
		blk_list->first_blk_idx = blk_idx;
		blk_list->last_blk_idx = blk_idx;
	} else {
		struct cfa_mm_blk *blk_info = &context->blk_tbl[blk_idx];

		blk_info->prev_blk_idx = blk_list->last_blk_idx;
		blk_info->next_blk_idx = CFA_MM_INVALID32;
		context->blk_tbl[blk_list->last_blk_idx].next_blk_idx =
			blk_idx;
		blk_list->last_blk_idx = blk_idx;
	}
}

/* delete from anywhere in the list */
static void cfa_mm_blk_delete(struct cfa_mm *context,
			      struct cfa_mm_blk_list *blk_list,
			      u32 blk_idx)
{
	struct cfa_mm_blk *blk_info = &context->blk_tbl[blk_idx];

	if (blk_list->first_blk_idx == CFA_MM_INVALID32)
		return;

	if (blk_list->last_blk_idx == blk_idx) {
		blk_list->last_blk_idx = blk_info->prev_blk_idx;
		if (blk_list->last_blk_idx != CFA_MM_INVALID32) {
			context->blk_tbl[blk_list->last_blk_idx].next_blk_idx =
				CFA_MM_INVALID32;
		}
	}

	if (blk_list->first_blk_idx == blk_idx) {
		blk_list->first_blk_idx = blk_info->next_blk_idx;
		if (blk_list->first_blk_idx != CFA_MM_INVALID32) {
			context->blk_tbl[blk_list->first_blk_idx].prev_blk_idx =
				CFA_MM_INVALID32;
		}
		return;
	}

	if (blk_info->prev_blk_idx != CFA_MM_INVALID32) {
		context->blk_tbl[blk_info->prev_blk_idx].next_blk_idx =
			blk_info->next_blk_idx;
	}

	if (blk_info->next_blk_idx != CFA_MM_INVALID32) {
		context->blk_tbl[blk_info->next_blk_idx].prev_blk_idx =
			blk_info->prev_blk_idx;
	}
}

/* Returns true if the bit in the bitmap is set to 'val' else returns false */
static bool cfa_mm_test_bit(u8 *bmap, u16 index, u8 val)
{
	u8 shift;

	bmap += index / CFA_MM_RECORDS_PER_BYTE;
	index %= CFA_MM_RECORDS_PER_BYTE;

	shift = CFA_MM_RECORDS_PER_BYTE - (index + 1);
	if (val) {
		if ((*bmap >> shift) & 0x1)
			return true;
	} else {
		if (!((*bmap >> shift) & 0x1))
			return true;
	}

	return false;
}

static int cfa_mm_test_and_set_bits(u8 *bmap, u16 start,
				    u16 count, u8 val)
{
	u8 mask[NUM_ALIGN_UNITS(CFA_MM_MAX_CONTIG_RECORDS,
				     CFA_MM_RECORDS_PER_BYTE) + 1];
	u16 i, j, nbits;

	bmap += start / CFA_MM_RECORDS_PER_BYTE;
	start %= CFA_MM_RECORDS_PER_BYTE;

	if ((start + count - 1) < CFA_MM_RECORDS_PER_BYTE) {
		nbits = CFA_MM_RECORDS_PER_BYTE - (start + count);
		mask[0] = (u8)(((u16)1 << count) - 1);
		mask[0] <<= nbits;
		if (val) {
			if (*bmap & mask[0])
				return -EINVAL;
			*bmap |= mask[0];
		} else {
			if ((*bmap & mask[0]) != mask[0])
				return -EINVAL;
			*bmap &= ~(mask[0]);
		}
		return 0;
	}

	i = 0;

	nbits = CFA_MM_RECORDS_PER_BYTE - start;
	mask[i++] = (u8)(((u16)1 << nbits) - 1);

	count -= nbits;

	while (count > CFA_MM_RECORDS_PER_BYTE) {
		count -= CFA_MM_RECORDS_PER_BYTE;
		mask[i++] = 0xff;
	}

	mask[i] = (u8)(((u16)1 << count) - 1);
	mask[i++] <<= (CFA_MM_RECORDS_PER_BYTE - count);

	for (j = 0; j < i; j++) {
		if (val) {
			if (bmap[j] & mask[j])
				return -EINVAL;
		} else {
			if ((bmap[j] & mask[j]) != mask[j])
				return -EINVAL;
		}
	}

	for (j = 0; j < i; j++) {
		if (val)
			bmap[j] |= mask[j];
		else
			bmap[j] &= ~(mask[j]);
	}

	return 0;
}

int cfa_mm_alloc(void *cmm, struct cfa_mm_alloc_parms *parms)
{
	struct cfa_mm *context = (struct cfa_mm *)cmm;
	struct cfa_mm_blk_list *blk_list;
	u32 i, cnt, blk_idx, record_idx;
	struct cfa_mm_blk *blk_info;
	u16 list_idx, num_records;
	u8 *blk_bmap;
	int ret = 0;

	if (!cmm || !parms ||
	    context->signature != CFA_MM_SIGNATURE) {
		netdev_dbg(NULL, "cmm = %p parms = %p\n", cmm, parms);
		return -EINVAL;
	}

	if (!(CFA_CHECK_BOUNDS(parms->num_contig_records, 1,
			       context->max_contig_records) &&
			       is_power_of_2(parms->num_contig_records))) {
		netdev_dbg(NULL, "cmm = %p parms = %p num_records = %d\n", cmm,
			   parms, parms->num_contig_records);
		return -EINVAL;
	}

	list_idx = CFA_ALIGN_LN2(parms->num_contig_records);

	blk_list = context->blk_list_tbl + list_idx;

	num_records = 1 << (list_idx - 1);

	if (context->records_in_use + num_records > context->max_records) {
		netdev_err(NULL, "Requested number (%d) of records not available\n",
			   num_records);
		ret = -ENOMEM;
		goto cfa_mm_alloc_exit;
	}

	if (blk_list->first_blk_idx == CFA_MM_INVALID32) {
		blk_idx = cfa_mm_blk_alloc(context);
		if (blk_idx == CFA_MM_INVALID32) {
			ret = -ENOMEM;
			goto cfa_mm_alloc_exit;
		}

		cfa_mm_blk_insert(context, blk_list, blk_idx);

		blk_info = &context->blk_tbl[blk_idx];

		blk_info->num_contig_records = num_records;
	} else {
		blk_idx = blk_list->first_blk_idx;
		blk_info = &context->blk_tbl[blk_idx];
	}

	while (blk_info->num_free_records < num_records) {
		/*
		 * All non-full entries precede full entries so
		 * upon seeing the first full entry, allocate
		 * new block as this means, all following records
		 * are full.
		 */
		if (blk_info->next_blk_idx == CFA_MM_INVALID32 ||
		    !blk_info->num_free_records) {
			blk_idx = cfa_mm_blk_alloc(context);
			if (blk_idx == CFA_MM_INVALID32) {
				ret = -ENOMEM;
				goto cfa_mm_alloc_exit;
			}

			cfa_mm_blk_insert(context, blk_list, blk_idx);

			blk_info = &context->blk_tbl[blk_idx];

			blk_info->num_contig_records = num_records;
		} else {
			blk_idx = blk_info->next_blk_idx;
			blk_info = &context->blk_tbl[blk_idx];
		}
	}

	blk_bmap = context->blk_bmap_tbl + blk_idx *
						   context->records_per_block /
						   CFA_MM_RECORDS_PER_BYTE;

	record_idx = blk_info->first_free_record;

	if (cfa_mm_test_and_set_bits(blk_bmap, record_idx, num_records, 1)) {
		netdev_dbg(NULL,
			   "Records are already allocated. record_idx = %d, num_records = %d\n",
			   record_idx, num_records);
		return -EINVAL;
	}

	parms->record_offset =
		(blk_idx * context->records_per_block) + record_idx;

	parms->num_contig_records = num_records;

	blk_info->num_free_records -= num_records;

	if (!blk_info->num_free_records) {
		/* move block to the end of the list if it is full */
		cfa_mm_blk_delete(context, blk_list, blk_idx);
		cfa_mm_blk_insert_last(context, blk_list, blk_idx);
		blk_info->first_free_record = context->records_per_block;
	} else {
		cnt = NUM_ALIGN_UNITS(context->records_per_block,
				      CFA_MM_RECORDS_PER_BYTE);

		for (i = (record_idx + num_records) / CFA_MM_RECORDS_PER_BYTE;
		     i < cnt; i++) {
			if (blk_bmap[i] != 0xff) {
				u8 bmap = blk_bmap[i];

				blk_info->first_free_record =
					i * CFA_MM_RECORDS_PER_BYTE;
				while (bmap & 0x80) {
					bmap <<= 1;
					blk_info->first_free_record++;
				}
				break;
			}
		}
	}

	context->records_in_use += num_records;

	ret = 0;

cfa_mm_alloc_exit:

	parms->used_count = context->records_in_use;

	parms->all_used = (context->records_in_use >= context->max_records);

	return ret;
}

int cfa_mm_free(void *cmm, struct cfa_mm_free_parms *parms)
{
	struct cfa_mm *context = (struct cfa_mm *)cmm;
	struct cfa_mm_blk_list *blk_list;
	struct cfa_mm_blk *blk_info;
	u16 list_idx, num_records;
	u32 blk_idx, record_idx;
	uint8_t *blk_bmap;

	if (!cmm || !parms ||
	    context->signature != CFA_MM_SIGNATURE) {
		netdev_err(NULL, "cmm = %p parms = %p\n", cmm, parms);
		return -EINVAL;
	}

	if (!(parms->record_offset < context->max_records &&
	      CFA_CHECK_BOUNDS(parms->num_contig_records, 1,
			       context->max_contig_records) &&
			       is_power_of_2(parms->num_contig_records))) {
		netdev_dbg(NULL,
			   "cmm = %p, parms = %p, record_offset = %d, num_contig_records = %d\n",
			   cmm, parms, parms->record_offset, parms->num_contig_records);
		return -EINVAL;
	}

	record_idx = parms->record_offset % context->records_per_block;
	blk_idx = parms->record_offset / context->records_per_block;

	list_idx = CFA_ALIGN_LN2(parms->num_contig_records);

	blk_list = &context->blk_list_tbl[list_idx];

	if (blk_list->first_blk_idx == CFA_MM_INVALID32) {
		netdev_err(NULL, "Records were not allocated\n");
		return -EINVAL;
	}

	num_records = 1 << (list_idx - 1);

	blk_info = &context->blk_tbl[blk_idx];

	if (blk_info->num_contig_records != num_records) {
		netdev_dbg(NULL,
			   "num_contig_records (%d) and num_records (%d) mismatch\n",
			    num_records, blk_info->num_contig_records);
		return -EINVAL;
	}

	blk_bmap = context->blk_bmap_tbl + blk_idx *
						   context->records_per_block /
						   CFA_MM_RECORDS_PER_BYTE;

	if (cfa_mm_test_and_set_bits(blk_bmap, record_idx, num_records, 0)) {
		netdev_dbg(NULL, "Records are not allocated. record_idx = %d, num_records = %d\n",
			   record_idx, num_records);
		return -EINVAL;
	}

	blk_info->num_free_records += num_records;

	if (blk_info->num_free_records >= context->records_per_block) {
		cfa_mm_blk_delete(context, blk_list, blk_idx);
		cfa_mm_blk_free(context, blk_idx);
	} else {
		if (blk_info->num_free_records == num_records) {
			cfa_mm_blk_delete(context, blk_list, blk_idx);
			cfa_mm_blk_insert(context, blk_list, blk_idx);
			blk_info->first_free_record = record_idx;
		} else {
			if (record_idx < blk_info->first_free_record)
				blk_info->first_free_record = record_idx;
		}
	}

	context->records_in_use -= num_records;

	parms->used_count = context->records_in_use;

	return 0;
}

int cfa_mm_entry_size_get(void *cmm, u32 entry_id, u8 *size)
{
	struct cfa_mm *context = (struct cfa_mm *)cmm;
	struct cfa_mm_blk *blk_info;
	u32 blk_idx, record_idx;
	u8 *blk_bmap;

	if (!cmm || !size || context->signature != CFA_MM_SIGNATURE)
		return -EINVAL;

	if (!(entry_id < context->max_records)) {
		netdev_dbg(NULL, "cmm = %p, entry_id = %d\n", cmm, entry_id);
		return -EINVAL;
	}

	blk_idx = entry_id / context->records_per_block;
	blk_info = &context->blk_tbl[blk_idx];
	record_idx = entry_id % context->records_per_block;

	/*
	 * Block is unused if num contig records is 0 and
	 * there are no allocated entries in the block
	 */
	if (blk_info->num_contig_records == 0)
		return -ENOENT;

	/*
	 * Check the entry is indeed allocated. Suffices to check if
	 * the first bit in the bitmap is set.
	 */
	blk_bmap = context->blk_bmap_tbl + blk_idx * context->records_per_block /
						     CFA_MM_RECORDS_PER_BYTE;

	if (cfa_mm_test_bit(blk_bmap, record_idx, 1)) {
		*size = blk_info->num_contig_records;
		return 0;
	} else {
		return -ENOENT;
	}
}
