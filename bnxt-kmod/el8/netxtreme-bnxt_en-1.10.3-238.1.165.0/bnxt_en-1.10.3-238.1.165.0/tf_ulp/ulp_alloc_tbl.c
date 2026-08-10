// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2014-2023 Broadcom
 * All rights reserved.
 */
#include <linux/types.h>
#include "bnxt_compat.h"
#include "bnxt_hsi.h"
#include "bnxt.h"
#include "bnxt_tfc.h"
#include "tf_core.h"
#include "ulp_mapper.h"
#include "ulp_alloc_tbl.h"

#if defined(CONFIG_BNXT_FLOWER_OFFLOAD) || defined(CONFIG_BNXT_CUSTOM_FLOWER_OFFLOAD)
/* Retrieve the allocator table  initialization parameters for the tbl_idx */
static const struct bnxt_ulp_allocator_tbl_params*
ulp_allocator_tbl_params_get(struct bnxt_ulp_context *ulp_ctx, u32 tbl_idx)
{
	const struct bnxt_ulp_allocator_tbl_params *alloc_tbl;
	struct bnxt_ulp_device_params *dparms;
	u32 dev_id;

	if (tbl_idx >= BNXT_ULP_ALLOCATOR_TBL_MAX_SZ) {
		netdev_dbg(ulp_ctx->bp->dev, "Allocator table out of bounds %d\n",
			   tbl_idx);
		return NULL;
	}

	if (bnxt_ulp_cntxt_dev_id_get(ulp_ctx, &dev_id))
		return NULL;

	dparms = bnxt_ulp_device_params_get(dev_id);
	if (!dparms) {
		netdev_dbg(ulp_ctx->bp->dev, "Failed to get device parms\n");
		return NULL;
	}

	alloc_tbl = &dparms->allocator_tbl_params[tbl_idx];
	return alloc_tbl;
}

/*
 * Initialize the allocator table list
 *
 * mapper_data [in] Pointer to the mapper data and the allocator table is
 * part of it
 *
 * returns 0 on success
 */
int
ulp_allocator_tbl_list_init(struct bnxt_ulp_context *ulp_ctx,
			    struct bnxt_ulp_mapper_data *mapper_data)
{
	const struct bnxt_ulp_allocator_tbl_params *tbl;
	struct ulp_allocator_tbl_entry *entry;
	u32 idx, pool_size;

	/* Allocate the generic tables. */
	for (idx = 0; idx < BNXT_ULP_ALLOCATOR_TBL_MAX_SZ; idx++) {
		tbl = ulp_allocator_tbl_params_get(ulp_ctx, idx);
		if (!tbl) {
			netdev_dbg(ulp_ctx->bp->dev, "Failed to get alloc table parm %d\n",
				   idx);
			return -EINVAL;
		}
		entry = &mapper_data->alloc_tbl[idx];

		/* Allocate memory for result data and key data */
		if (tbl->num_entries != 0) {
			/* assign the name */
			entry->alloc_tbl_name = tbl->name;
			entry->num_entries = tbl->num_entries;
			pool_size = BITALLOC_SIZEOF(tbl->num_entries);
			/* allocate the big chunk of memory */
			entry->ulp_bitalloc = vzalloc(pool_size);
			if (!entry->ulp_bitalloc)
				return -ENOMEM;
			if (bnxt_ba_init(entry->ulp_bitalloc, entry->num_entries, true))
				return -ENOMEM;
		} else {
			netdev_dbg(ulp_ctx->bp->dev, "%s:Unused alloc tbl entry is %d\n",
				   tbl->name, idx);
			continue;
		}
	}
	return 0;
}

/*
 * Free the allocator table list
 *
 * mapper_data [in] Pointer to the mapper data and the generic table is
 * part of it
 *
 * returns 0 on success
 */
int
ulp_allocator_tbl_list_deinit(struct bnxt_ulp_mapper_data *mapper_data)
{
	struct ulp_allocator_tbl_entry *entry;
	u32 idx;

	/* iterate the generic table. */
	for (idx = 0; idx < BNXT_ULP_ALLOCATOR_TBL_MAX_SZ; idx++) {
		entry = &mapper_data->alloc_tbl[idx];
		if (entry->ulp_bitalloc) {
			bnxt_ba_deinit(entry->ulp_bitalloc);
			vfree(entry->ulp_bitalloc);
			entry->ulp_bitalloc = NULL;
		}
	}
	/* success */
	return 0;
}

/*
 * utility function to calculate the table idx
 *
 * res_sub_type [in] - Resource sub type
 * dir [in] - Direction
 *
 * returns None
 */
static int
ulp_allocator_tbl_idx_calculate(u32 res_sub_type, u32 dir)
{
	int tbl_idx;

	/* Validate for direction */
	if (dir >= TF_DIR_MAX) {
		netdev_dbg(NULL, "invalid argument %x\n", dir);
		return -EINVAL;
	}
	tbl_idx = (res_sub_type << 1) | (dir & 0x1);
	if (tbl_idx >= BNXT_ULP_ALLOCATOR_TBL_MAX_SZ) {
		netdev_dbg(NULL, "invalid table index %x\n", tbl_idx);
		return -EINVAL;
	}
	return tbl_idx;
}

/*
 * allocate a index from allocator
 *
 * mapper_data [in] Pointer to the mapper data and the allocator table is
 * part of it
 *
 * returns index on success or negative number on failure
 */
int
ulp_allocator_tbl_list_alloc(struct bnxt_ulp_mapper_data *mapper_data,
			     u32 res_sub_type, u32 dir,
			     int *alloc_id)
{
	struct ulp_allocator_tbl_entry *entry;
	int idx;

	idx = ulp_allocator_tbl_idx_calculate(res_sub_type, dir);
	if (idx < 0)
		return -EINVAL;

	entry = &mapper_data->alloc_tbl[idx];
	if (!entry->ulp_bitalloc || !entry->num_entries) {
		netdev_dbg(NULL, "invalid table index %x\n", idx);
		return -EINVAL;
	}
	*alloc_id = bnxt_ba_alloc(entry->ulp_bitalloc);

	if (*alloc_id < 0) {
		netdev_dbg(NULL, "unable to alloc index %x\n", idx);
		return -ENOMEM;
	}
	/* Not using zero index */
	*alloc_id += 1;
	return 0;
}

/*
 * free a index in allocator
 *
 * mapper_data [in] Pointer to the mapper data and the allocator table is
 * part of it
 *
 * returns error
 */
int
ulp_allocator_tbl_list_free(struct bnxt *bp,
			    struct bnxt_ulp_mapper_data *mapper_data,
			    u32 res_sub_type, u32 dir,
			    int index)
{
	struct ulp_allocator_tbl_entry *entry;
	int rc;
	int idx;

	idx = ulp_allocator_tbl_idx_calculate(res_sub_type, dir);
	if (idx < 0)
		return -EINVAL;

	entry = &mapper_data->alloc_tbl[idx];
	if (!entry->ulp_bitalloc || !entry->num_entries) {
		netdev_dbg(bp->dev, "invalid table index %x\n", idx);
		return -EINVAL;
	}
	/* not using zero index */
	index -= 1;
	if (index < 0 || index > entry->num_entries) {
		netdev_dbg(bp->dev, "invalid alloc index %x\n", index);
		return -EINVAL;
	}
	rc = bnxt_ba_free(entry->ulp_bitalloc, index);
	if (rc < 0) {
		netdev_dbg(bp->dev, "%s:unable to free index %x\n",
			   entry->alloc_tbl_name, index);
		return -EINVAL;
	}
	return 0;
}
#endif /* defined(CONFIG_BNXT_FLOWER_OFFLOAD) || defined(CONFIG_BNXT_CUSTOM_FLOWER_OFFLOAD) */
