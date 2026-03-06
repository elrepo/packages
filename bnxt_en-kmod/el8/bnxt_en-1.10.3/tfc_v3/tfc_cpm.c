// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#include <linux/types.h>
#include "bnxt_compat.h"
#include "bnxt.h"
#include "tfc.h"
#include "tfc_cpm.h"

/* Per pool entry
 */
struct cpm_pool_entry {
	bool valid;
	struct tfc_cmm *cmm;
	u32 used_count;
	bool all_used;
	struct cpm_pool_use *pool_use;
};

/* Pool use list entry
 */
struct cpm_pool_use {
	u16 pool_id;
	struct cpm_pool_use *prev;
	struct cpm_pool_use *next;
};

/* tfc_cpm
 *
 * This is the main CPM data struct
 */
struct tfc_cpm {
	struct cpm_pool_entry *pools;
	u16 available_pool_id; /* pool with highest use count, i.e. most used entries */
	bool pool_valid; /* pool has free entries */
	u32 pool_size; /* number of entries in each pool */
	u32 max_pools; /* maximum number of pools */
	u32 next_index; /* search index */
	struct cpm_pool_use *pool_use_list; /* Ordered list of pool usage */
};

#define CPM_DEBUG 0

#if (CPM_DEBUG == 1)
static void show_list(char *str, struct tfc_cpm *cpm)
{
	struct cpm_pool_use *pu = cpm->pool_use_list;

	netdev_dbg(NULL, "%s - ", str);
	while (!pu) {
		netdev_dbg(NULL,
			   "PU(%p) id:%d(u:%d au:%d) p:0x%p n:0x%p\n",
			   pu, pu->pool_id,
			   cpm->pools[pu->pool_id].used_count,
			   cpm->pools[pu->pool_id].all_used,
			   pu->prev, pu->next);

		pu = pu->next;
	}
}
#endif

static int cpm_insert_pool_id(struct tfc_cpm *cpm, u16 pool_id)
{
	struct cpm_pool_entry *pool = &cpm->pools[pool_id];
	struct cpm_pool_use *pool_use = cpm->pool_use_list;
	struct cpm_pool_use *new_pool_use;
	struct cpm_pool_use *prev = NULL;

	if (!pool->valid) {
		netdev_dbg(NULL, "%s: Pool ID:0x%x is invalid\n", __func__, pool_id);
		return -EINVAL;
	}

	/* Find where in insert new entry */
	while (pool_use) {
		if (cpm->pools[pool_use->pool_id].valid &&
		    cpm->pools[pool_use->pool_id].used_count > pool->used_count) {
			pool_use = pool_use->next;
			prev =	pool_use;
		} else {
			break;
		}
	}

	/* Alloc new entry */
	new_pool_use = vzalloc(sizeof(*new_pool_use));
	new_pool_use->pool_id = pool_id;
	new_pool_use->prev = NULL;
	new_pool_use->next = NULL;
	pool->pool_use = new_pool_use;

	if (!pool_use) {			/* Empty list */
		cpm->pool_use_list = new_pool_use;
	} else if (!prev) {			/* Start of list */
		cpm->pool_use_list = new_pool_use;
		new_pool_use->next = pool_use;
		pool_use->prev = new_pool_use;
	} else {				/* Within list */
		prev->next = new_pool_use;
		new_pool_use->next = pool_use;
		new_pool_use->prev = prev;
	}

	cpm->available_pool_id = cpm->pool_use_list->pool_id;
	cpm->pool_valid = true;
#if (CPM_DEBUG == 1)
	show_list("Insert", cpm);
#endif
	return 0;
}

static int cpm_sort_pool_id(struct tfc_cpm *cpm, u16 pool_id)
{
	struct cpm_pool_entry *pool = &cpm->pools[pool_id];
	struct cpm_pool_use *pool_use = pool->pool_use;
	struct cpm_pool_use *prev, *next;

	/* Does entry need to move up, down or stay where it is?
	 *
	 * The list is ordered by:
	 *	  Head:	 - Most used, but not full
	 *			 - ....next most used but not full
	 *			 - least used
	 *	  Tail:	 - All used
	 */
	while (1) {
		if (pool_use->prev &&
		    cpm->pools[pool_use->prev->pool_id].valid && !pool->all_used &&
		    (cpm->pools[pool_use->prev->pool_id].all_used ||
		     cpm->pools[pool_use->prev->pool_id].used_count < pool->used_count)) {
			/* Move up */
			prev = pool_use->prev;
			pool_use->prev->next = pool_use->next;
			/* May be at the end of the list */
			if (pool_use->next)
				pool_use->next->prev = pool_use->prev;
			pool_use->next = pool_use->prev;

			if (pool_use->prev->prev) {
				pool_use->prev->prev->next = pool_use;
				pool_use->prev = pool_use->prev->prev;
			} else {
				/* Moved to head of the list */
				pool_use->prev->prev = pool_use;
				pool_use->prev = NULL;
				cpm->pool_use_list = pool_use;
			}

			prev->prev = pool_use;
		} else if (pool_use->next && cpm->pools[pool_use->next->pool_id].valid &&
			   (pool->all_used || (!cpm->pools[pool_use->next->pool_id].all_used &&
			    (cpm->pools[pool_use->next->pool_id].used_count > pool->used_count)))) {
			/* Move down */
			next = pool_use->next;
			pool_use->next->prev = pool_use->prev;
			if (pool_use->prev) /* May be at the start of the list */
				pool_use->prev->next = pool_use->next;
			else
				cpm->pool_use_list = pool_use->next;

			pool_use->prev = pool_use->next;

			if (pool_use->next->next) {
				pool_use->next->next->prev = pool_use;
				pool_use->next = pool_use->next->next;
			} else {
				/* Moved to end of the list */
				pool_use->next->next = pool_use;
				pool_use->next = NULL;
			}

			next->next = pool_use;
		} else {
			/* Nothing to do */
			break;
		}
#if (CPM_DEBUG == 1)
		show_list("Sort", cpm);
#endif
	}

	if (cpm->pools[cpm->pool_use_list->pool_id].all_used) {
		cpm->available_pool_id = TFC_CPM_INVALID_POOL_ID;
		cpm->pool_valid = false;
	} else {
		cpm->available_pool_id = cpm->pool_use_list->pool_id;
		cpm->pool_valid = true;
	}

	return 0;
}

int tfc_cpm_open(struct tfc_cpm **cpm, u32 max_pools)
{
	/* Allocate CPM struct */
	*cpm = vzalloc(sizeof(**cpm));
	if (!*cpm) {
		netdev_dbg(NULL, "%s: cpm alloc error %d\n", __func__, -ENOMEM);
		*cpm = NULL;
		return -ENOMEM;
	}

	/* Allocate CPM pools array */
	(*cpm)->pools = vzalloc(sizeof(*(*cpm)->pools) * max_pools);
	if (!(*cpm)->pools) {
		netdev_dbg(NULL, "%s: pools alloc error %d\n", __func__, -ENOMEM);
		vfree(*cpm);
		*cpm = NULL;

		return -ENOMEM;
	}

	/* Init pool entries by setting all fields to zero */
	memset((*cpm)->pools, 0, sizeof(struct cpm_pool_entry) * max_pools);

	/* Init remaining CPM fields */
	(*cpm)->pool_valid = false;
	(*cpm)->available_pool_id = 0;
	(*cpm)->max_pools = max_pools;
	(*cpm)->pool_use_list = NULL;

	return	0;
}

int tfc_cpm_close(struct tfc_cpm *cpm)
{
	struct cpm_pool_use *cpm_current;
	struct cpm_pool_use *next;

	if (!cpm) {
		netdev_dbg(NULL, "%s: CPM is NULL\n", __func__);
		return -EINVAL;
	}

	cpm_current = cpm->pool_use_list;
	while (cpm_current) {
		next = cpm_current->next;
		vfree(cpm_current);
		cpm_current = next;
	}

	vfree(cpm->pools);
	vfree(cpm);

	return 0;
}

int tfc_cpm_set_pool_size(struct tfc_cpm *cpm, u32 pool_sz_in_records)
{
	if (!cpm) {
		netdev_dbg(NULL, "%s: CPM is NULL\n", __func__);
		return -EINVAL;
	}

	cpm->pool_size = pool_sz_in_records;
	return 0;
}

int tfc_cpm_get_pool_size(struct tfc_cpm *cpm, u32 *pool_sz_in_records)
{
	if (!cpm) {
		netdev_dbg(NULL, "%s: CPM is NULL\n", __func__);
		return -EINVAL;
	}

	*pool_sz_in_records = cpm->pool_size;
	return 0;
}

int tfc_cpm_set_cmm_inst(struct tfc_cpm *cpm, u16 pool_id, struct tfc_cmm *cmm)
{
	struct cpm_pool_entry *pool;

	if (!cpm) {
		netdev_dbg(NULL, "%s: CPM is NULL\n", __func__);
		return -EINVAL;
	}

	pool = &cpm->pools[pool_id];

	if (pool->valid && cmm) {
		netdev_dbg(NULL, "%s: Pool ID:0x%x is already in use\n", __func__, pool_id);
		return -EBUSY;
	}

	pool->cmm = cmm;
	pool->used_count = 0;
	pool->all_used = false;
	pool->pool_use = NULL;

	if (!cmm) {
		pool->valid = false;
	} else {
		pool->valid = true;
		cpm_insert_pool_id(cpm, pool_id);
	}

	return 0;
}

int tfc_cpm_get_cmm_inst(struct tfc_cpm *cpm, u16 pool_id, struct tfc_cmm **cmm)
{
	struct cpm_pool_entry *pool;

	if (!cpm) {
		netdev_dbg(NULL, "%s: CPM is NULL\n", __func__);
		return -EINVAL;
	}

	pool = &cpm->pools[pool_id];

	if (!pool->valid) {
		netdev_dbg(NULL, "%s: Pool ID:0x%x is not valid\n", __func__, pool_id);
		return -EINVAL;
	}

	*cmm = pool->cmm;
	return 0;
}

int tfc_cpm_get_avail_pool(struct tfc_cpm *cpm, u16 *pool_id)
{
	if (!cpm) {
		netdev_dbg(NULL, "%s: CPM is NULL\n", __func__);
		return -EINVAL;
	}

	if (!cpm->pool_valid) {
		netdev_dbg(NULL, "%s: pool is invalid\n", __func__);
		return -EINVAL;
	}
	*pool_id = cpm->available_pool_id;

	return 0;
}

int tfc_cpm_set_usage(struct tfc_cpm *cpm, u16 pool_id, u32 used_count, bool all_used)
{
	struct cpm_pool_entry *pool;

	if (!cpm) {
		netdev_dbg(NULL, "%s: CPM is NULL\n", __func__);
		return -EINVAL;
	}

	pool = &cpm->pools[pool_id];

	if (!pool->valid) {
		netdev_dbg(NULL, "%s: Pool ID:0x%x is invalid\n", __func__, pool_id);
		return -EINVAL;
	}

	if (used_count > cpm->pool_size) {
		netdev_dbg(NULL, "%s: Number of entries(%d) exceeds pool_size (%d)\n",
			   __func__, used_count, cpm->pool_size);
		return -EINVAL;
	}

	pool->all_used = all_used;
	pool->used_count = used_count;

	/* Update ordered list of pool_ids */
	cpm_sort_pool_id(cpm, pool_id);

	return 0;
}

int tfc_cpm_srchm_by_configured_pool(struct tfc_cpm *cpm, enum cfa_srch_mode srch_mode,
				     u16 *pool_id, struct tfc_cmm **cmm)
{
	u32 i;

	if (!cpm) {
		netdev_dbg(NULL, "%s: CPM is NULL\n",
			   __func__);
		return -EINVAL;
	}

	if (!pool_id) {
		netdev_dbg(NULL, "%s: pool_id ptr is NULL\n",
			   __func__);
		return -EINVAL;
	}

	if (!cmm) {
		netdev_dbg(NULL, "%s: cmm ptr is NULL\n",
			   __func__);
		return -EINVAL;
	}
	*pool_id = TFC_CPM_INVALID_POOL_ID;
	*cmm = NULL;

	if (srch_mode == CFA_SRCH_MODE_FIRST)
		cpm->next_index = 0;

	for (i = cpm->next_index; i < cpm->max_pools; i++) {
		if (cpm->pools[i].cmm) {
			*pool_id = i;
			*cmm = cpm->pools[i].cmm;
			cpm->next_index = i + 1;
			return 0;
		}
	}
	cpm->next_index = cpm->max_pools;
	return -ENOENT;
}
