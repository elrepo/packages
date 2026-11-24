// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2023 Broadcom
 * All rights reserved.
 */

#include <linux/types.h>
#include <linux/vmalloc.h>
#include <linux/pci.h>

#include "bnxt_compat.h"
#include "bnxt.h"
#include "bnxt_sriov.h"
#include "bnxt_tfc.h"
#include "bnxt_debugfs.h"

#include "tfc.h"

#include "tfc_priv.h"
#include "tfc_msg.h"
#include "tfc_em.h"
#include "cfa_types.h"
#include "cfa_tim.h"
#include "cfa_tpm.h"
#include "tfo.h"
#include "bnxt_compat.h"
#include "bnxt.h"
#include "tfc_cpm.h"
#include "cfa_mm.h"
#include "cfa_bld_mpc_field_ids.h"
#include "tfc_vf2pf_msg.h"
#include "tfc_util.h"

/* These values are for Thor2. Take care to adjust them appropriately when
 * support for additional HW is added.
 */
#define ENTRIES_PER_BUCKET 6	/* Max number of entries for a single bucket */
#define LREC_SIZE 16		/* sizes in bytes */
#define RECORD_SIZE 32

/* Page alignments must be some power of 2.  These bits define the powers of 2
 * that are valid for page alignments.  It is taken from
 * cfa_hw_ts_pbl_page_size.
 */
#define VALID_PAGE_ALIGNMENTS 0x40753000

#define MAX_PAGE_PTRS(page_size) ((page_size) / sizeof(void *))

#define BITS_IN_VAR(x) (sizeof(x) * 8)

/*
 * On-demand (dynamic) table-scope record backing.
 *
 * When enabled, the lookup (EM) region's static bucket pointer array and the
 * full PBL page-table (index) hierarchy are allocated up front at table-scope
 * config time, but the record data pages are allocated on demand when a pool
 * is allocated (tfc_tbl_scope_pool_alloc()).  This lets a function back only
 * the pools it actually uses instead of the entire region upfront.
 *
 * Controlled at build time via the Makefile variable enable_dynamic_backing
 * (default 1 = enabled).  To disable: make enable_dynamic_backing=0
 *
 * The hardware/firmware behaviors this relies on (PCDU PBL re-walk after
 * BS_CFG_ALL_DONE, PTU negative-cache handling, and PTE LAST marker
 * placement) are documented in docs/tfc_dynamic_pool_backing_design.md
 * and must be validated on silicon.  Pages are reclaimed in bulk at
 * table-scope teardown (tfc_tbl_scope_mem_free()); pool free does not
 * unback records to avoid any stale PTU translation hazard at runtime.
 */
#ifdef BNXT_TS_DYNAMIC_BACKING
static const bool bnxt_ts_dynamic_backing = true;
#else
static const bool bnxt_ts_dynamic_backing;
#endif

/* Sentinel for alloc_link_pbl(): back every data page (legacy behavior). */
#define TFC_TS_BACK_ALL_PAGES  U32_MAX

/* Private functions */

/* Calculate the smallest power of 2 that is >= x.  The return value is the
 * exponent of 2.
 */
static inline unsigned int next_pow2(unsigned int x)
{
	/* This algorithm calculates the nearest power of 2 greater than or
	 * equal to x:
	 * The function __builtin_clz returns the number of leading 0-bits in
	 * an unsigned int.
	 * Subtract this from the number of bits in x to get the power of 2.  In
	 * the examples below, an int is assumed to have 32 bits.
	 *
	 * Example 1:
	 *    x == 2
	 *    __builtin_clz(1) = 31
	 *    32 - 31 = 1
	 *    2^1 = 2
	 * Example 2:
	 *    x = 63
	 *    __builtin_clz(62) = 26
	 *    32 - 26 = 6
	 *    2^6 = 64
	 */
	return x == 1 ? 1 : (BITS_IN_VAR(x) - __builtin_clz(x - 1));
}

/* Calculate the largest power of 2 that is less than x.  The return value is
 * the exponent of 2.
 */
static inline unsigned int prev_pow2(unsigned int x)
{
	/* This algorithm calculates the nearest power of 2 less than x:
	 * The function __builtin_clz returns the number of leading 0-bits in
	 * an unsigned int.
	 * Subtract this from one less than the number of bits in x to get
	 * the power of 2.  In the examples below, an int is assumed to have
	 * 32 bits.
	 *
	 * Example 1:
	 *    x = 2
	 *    __builtin_clz(1) = 31
	 *    31 - 31 = 0
	 *    2^0 = 1
	 * Example 2:
	 *    x = 63
	 *    __builtin_clz(62) = 26
	 *    31 - 26 = 5
	 *    2^5 = 32
	 * Example 3:
	 *   x = 64
	 *    __builtin_clz(63) = 26
	 *    31 - 26 = 5
	 *    2^5 = 32
	 */
	return x == 1 ? 0 : (BITS_IN_VAR(x) - 1 - __builtin_clz(x - 1));
}

static inline u32 roundup32(u32 x, u32 y)
{
	return (((x + y - 1) / y) * y);
}

static inline u64 roundup64(u64 x, u64 y)
{
	return (((x + y - 1) / y) * y);
}

/**
 * This function calculates how many buckets and records are required for a
 * given flow_cnt and factor.
 *
 * @flow_cnt: The total number of flows for which to compute memory
 * @key_sz_in_bytes: The lookup key size in bytes
 * @scope_type: Shared-app or global table scopes cannot have dynamic buckets.
 * @factor: This indicates a multiplier factor for determining the static and dynamic
 *	    bucket counts.  The larger the factor, the more buckets will be allocated.
 * @lkup_rec_cnt: The total number of lookup records to allocate (includes buckets)
 * @static_bucket_cnt_exp: The log2 of the number of static buckets to allocate.
 *			   For example if 1024 static buckets, 1024=2^10,
			   so the value 10 would be returned.
 * @dynamic_bucket_cnt: The number of dynamic buckets to allocate
 *
 * Return 0 if successful, -EINVAL if not.
 */
static int calc_lkup_rec_cnt(struct bnxt *bp, u32 flow_cnt, u16 key_sz_in_bytes,
			     enum cfa_scope_type scope_type,
			     enum tfc_tbl_scope_bucket_factor factor,
			     u32 *lkup_rec_cnt, u8 *static_bucket_cnt_exp,
			     u32 *dynamic_bucket_cnt)
{
	unsigned int entry_size;
	unsigned int flow_adj;	  /* flow_cnt adjusted for factor */
	unsigned int key_rec_cnt;

	flow_cnt = 1 << next_pow2(flow_cnt);
	switch (factor) {
	case TFC_TBL_SCOPE_BUCKET_FACTOR_1:
		flow_adj = flow_cnt;
		break;
	case TFC_TBL_SCOPE_BUCKET_FACTOR_2:
		flow_adj = flow_cnt * 2;
		break;
	case TFC_TBL_SCOPE_BUCKET_FACTOR_4:
		flow_adj = flow_cnt * 4;
		break;
	case TFC_TBL_SCOPE_BUCKET_FACTOR_8:
		flow_adj = flow_cnt * 8;
		break;
	case TFC_TBL_SCOPE_BUCKET_FACTOR_16:
		flow_adj = flow_cnt * 16;
		break;
	case TFC_TBL_SCOPE_BUCKET_FACTOR_32:
		flow_adj = flow_cnt * 32;
		break;
	case TFC_TBL_SCOPE_BUCKET_FACTOR_64:
		flow_adj = flow_cnt * 64;
		break;
	default:
		netdev_dbg(bp->dev, "%s: Invalid factor (%u)\n", __func__, factor);
		return -EINVAL;
	}

	if (key_sz_in_bytes <= RECORD_SIZE - LREC_SIZE) {
		entry_size = 1;
	} else if (key_sz_in_bytes <= RECORD_SIZE * 2 - LREC_SIZE) {
		entry_size = 2;
	} else if (key_sz_in_bytes <= RECORD_SIZE * 3 - LREC_SIZE) {
		entry_size = 3;
	} else if (key_sz_in_bytes <= RECORD_SIZE * 4 - LREC_SIZE) {
		entry_size = 4;
	} else {
		netdev_dbg(bp->dev, "%s: Key size (%u) cannot be larger than (%u)\n", __func__,
			   key_sz_in_bytes, RECORD_SIZE * 4 - LREC_SIZE);
		return -EINVAL;
	}
	key_rec_cnt = flow_cnt * entry_size;

#ifdef DYNAMIC_BUCKETS_SUPPORTED
	if (scope_type != CFA_SCOPE_TYPE_NON_SHARED) {
#endif
		*static_bucket_cnt_exp =
			next_pow2(flow_adj / ENTRIES_PER_BUCKET);
		*dynamic_bucket_cnt = 0;
#ifdef DYNAMIC_BUCKETS_SUPPORTED
	} else {
		*static_bucket_cnt_exp =
			prev_pow2(flow_cnt / ENTRIES_PER_BUCKET);
		*dynamic_bucket_cnt =
			(flow_adj - flow_cnt) / ENTRIES_PER_BUCKET;
	}
#endif

	*lkup_rec_cnt = key_rec_cnt + (1 << *static_bucket_cnt_exp) +
		*dynamic_bucket_cnt;

	return 0;
}

static int calc_act_rec_cnt(struct bnxt *bp, u32 *act_rec_cnt, u32 flow_cnt,
			    u16 act_rec_sz_in_bytes)
{
	flow_cnt = 1 << next_pow2(flow_cnt);
	if (act_rec_sz_in_bytes % RECORD_SIZE) {
		netdev_dbg(bp->dev, "%s: Action record size (%u) must be a multiple of %u\n",
			   __func__, act_rec_sz_in_bytes, RECORD_SIZE);
		return -EINVAL;
	}

	*act_rec_cnt = flow_cnt * (act_rec_sz_in_bytes / RECORD_SIZE);

	return 0;
}

/* Using a #define for the number of bits since the size of an int can depend
 * upon the processor.
 */
#define BITS_IN_UINT (sizeof(unsigned int) * 8)

static int calc_pool_sz_exp(struct bnxt *bp, u8 *pool_sz_exp, u32 rec_cnt,
			    u32 max_pools)
{
	unsigned int recs_per_region = rec_cnt / max_pools;

	if (recs_per_region == 0) {
		netdev_dbg(bp->dev, "%s: rec_cnt (%u) must be larger than max_pools (%u)\n",
			   __func__, rec_cnt, max_pools);
		return -EINVAL;
	}

	*pool_sz_exp = prev_pow2(recs_per_region + 1);

	return 0;
}

static int calc_rec_start_offset(struct bnxt *bp, u32 *start_offset, u32 bucket_cnt_exp)
{
	*start_offset = 1 << bucket_cnt_exp;

	return 0;
}

static void free_pg_tbl(struct bnxt *bp, struct tfc_ts_page_tbl *tp)
{
	u32 i;

	for (i = 0; i < tp->pg_count; i++) {
		if (!tp->pg_va_tbl[i]) {
			/* Slots in [0, pg_backed) must be present in static
			 * mode; a NULL here is unexpected.  Slots beyond
			 * pg_backed are lazily deferred and may be NULL if
			 * no pool was ever allocated for them; that is normal.
			 */
			if (i < tp->pg_backed)
				netdev_dbg(bp->dev,
					   "No mapping for page: %d table: %16p\n",
					   i, tp);
			continue;
		}

		dma_free_coherent(tp->dma_dev, tp->pg_size,
				  tp->pg_va_tbl[i], tp->pg_pa_tbl[i]);
		tp->pg_va_tbl[i] = NULL;
	}

	tp->pg_count = 0;
	kfree(tp->pg_va_tbl);
	tp->pg_va_tbl = NULL;
	kfree(tp->pg_pa_tbl);
	tp->pg_pa_tbl = NULL;
}

/*
 * Allocate one page-table level.
 *
 * @slots:		Total number of page slots in this level for the full
 *			region (== the size of the pg_va_tbl/pg_pa_tbl arrays
 *			and the value stored in tp->pg_count).
 * @pages_to_alloc:	Number of leaf data pages to actually back now, starting
 *			at index 0.  Index pages (non-leaf levels) always pass
 *			pages_to_alloc == slots.  When dynamic backing is enabled,
 *			the data level passes fewer; entries
 *			[pages_to_alloc, slots) are left NULL and are populated
 *			on demand by pbl_back_range().  free_pg_tbl() tolerates
 *			the NULL (deferred) entries.
 */
static int alloc_pg_tbl(struct bnxt *bp, struct tfc_ts_page_tbl *tp, u32 slots,
			u32 pages_to_alloc, u32 pg_size)
{
	u32 i;
	void *va;
	dma_addr_t pa;

	/* Reset pg_count early so free_pg_tbl is safe on any subsequent error
	 * path, including when mem_cfg is reused across alloc_link_pbl calls.
	 */
	tp->pg_count = 0;

	tp->pg_va_tbl =
		kcalloc(slots, sizeof(void *), GFP_KERNEL);
	if (!tp->pg_va_tbl)
		return -ENOMEM;

	tp->pg_pa_tbl =
		kcalloc(slots, sizeof(dma_addr_t), GFP_KERNEL);
	if (!tp->pg_pa_tbl) {
		kfree(tp->pg_va_tbl);
		tp->pg_va_tbl = NULL;
		return -ENOMEM;
	}

	tp->pg_size = pg_size;
	/* Snapshot the allocating device so free_pg_tbl always uses the
	 * same device, even if a different PF's bp is passed at free time
	 * (hw_lag mode: table scope shared across two PF instances).
	 */
	tp->dma_dev = &bp->pdev->dev;

	/* pg_count is the total number of slots in this level for the full
	 * region, even when some leaf pages are deferred.  link_page_table()
	 * and the LAST/NEXT_TO_LAST markers rely on the full extent.
	 * pg_backed tracks how many of those slots are statically allocated so
	 * free_pg_tbl() can distinguish unexpected NULLs from dynamic ones.
	 */
	tp->pg_count = slots;
	tp->pg_backed = min(pages_to_alloc, slots);

	for (i = 0; i < pages_to_alloc; i++) {
		va = dma_alloc_coherent(&bp->pdev->dev, pg_size, &pa, GFP_KERNEL);
		if (!va)
			goto cleanup;
		tp->pg_va_tbl[i] = va;
		tp->pg_pa_tbl[i] = pa;
	}

	return 0;

cleanup:
	free_pg_tbl(bp, tp);
	return -ENOMEM;
}

static void free_page_table(struct bnxt *bp, struct tfc_ts_mem_cfg *mem_cfg)
{
	struct tfc_ts_page_tbl *tp;
	int i;

	for (i = 0; i < mem_cfg->num_lvl; i++) {
		tp = &mem_cfg->pg_tbl[i];
		netdev_dbg(bp->dev, "EEM: Freeing page table: lvl %d cnt %u\n", i, tp->pg_count);

		free_pg_tbl(bp, tp);
	}

	mem_cfg->l0_addr = NULL;
	mem_cfg->l0_dma_addr = 0;
	mem_cfg->num_lvl = 0;
	mem_cfg->num_data_pages = 0;
}

static int alloc_page_table(struct bnxt *bp, struct tfc_ts_mem_cfg *mem_cfg,
			    u32 page_size, u32 backed_data_pages)
{
	int data_lvl = mem_cfg->num_lvl - 1;
	struct tfc_ts_page_tbl *tp;
	int i, rc;

	for (i = 0; i < mem_cfg->num_lvl; i++) {
		u32 slots = mem_cfg->page_cnt[i];
		u32 to_alloc = slots;

		/* Index (non-leaf) levels are always fully allocated.  For the
		 * data (leaf) level, back only the requested number of pages;
		 * the remainder are populated on demand by pbl_back_range().
		 */
		if (i == data_lvl && backed_data_pages < slots)
			to_alloc = backed_data_pages;

		tp = &mem_cfg->pg_tbl[i];

		rc = alloc_pg_tbl(bp, tp, slots, to_alloc, page_size);
		if (rc) {
			netdev_dbg(bp->dev, "Failed to allocate page table: lvl: %d, rc:%d\n", i,
				   rc);
			goto cleanup;
		}
	}
	return 0;

cleanup:
	free_page_table(bp, mem_cfg);
	return rc;
}

static u32 page_tbl_pgcnt(u32 num_pages, u32 page_size)
{
	return roundup32(num_pages, MAX_PAGE_PTRS(page_size)) /
	       MAX_PAGE_PTRS(page_size);
	return 0;
}

static void size_page_tbls(int max_lvl, u64 num_data_pages,
			   u32 page_size, u32 *page_cnt)
{
	if (max_lvl == TFC_TS_PT_LVL_0) {
		page_cnt[TFC_TS_PT_LVL_0] = num_data_pages;
	} else if (max_lvl == TFC_TS_PT_LVL_1) {
		page_cnt[TFC_TS_PT_LVL_1] = num_data_pages;
		page_cnt[TFC_TS_PT_LVL_0] =
			page_tbl_pgcnt(page_cnt[TFC_TS_PT_LVL_1], page_size);
	} else if (max_lvl == TFC_TS_PT_LVL_2) {
		page_cnt[TFC_TS_PT_LVL_2] = num_data_pages;
		page_cnt[TFC_TS_PT_LVL_1] =
			page_tbl_pgcnt(page_cnt[TFC_TS_PT_LVL_2], page_size);
		page_cnt[TFC_TS_PT_LVL_0] =
			page_tbl_pgcnt(page_cnt[TFC_TS_PT_LVL_1], page_size);
	} else {
		return;
	}
}

static int num_pages_get(struct tfc_ts_mem_cfg *mem_cfg, u32 page_size)
{
	u64 max_page_ptrs = MAX_PAGE_PTRS(page_size);
	u64 lvl_data_size = page_size;
	int lvl = TFC_TS_PT_LVL_0;
	u64 data_size;

	mem_cfg->num_data_pages = 0;
	data_size = (u64)mem_cfg->rec_cnt * mem_cfg->entry_size;

	while (lvl_data_size < data_size) {
		lvl++;

		if (lvl == TFC_TS_PT_LVL_1)
			lvl_data_size = max_page_ptrs * page_size;
		else if (lvl == TFC_TS_PT_LVL_2)
			lvl_data_size =
				max_page_ptrs * max_page_ptrs * page_size;
		else
			return -ENOMEM;
	}

	mem_cfg->num_data_pages = roundup64(data_size, page_size) / page_size;
	mem_cfg->num_lvl = lvl + 1;

	return 0;
}

static void link_page_table(struct tfc_ts_page_tbl *tp,
			    struct tfc_ts_page_tbl *tp_next, bool set_pte_last)
{
	u64 *pg_pa = tp_next->pg_pa_tbl, *pg_va, valid;
	void **next_va = tp_next->pg_va_tbl;
	u32 i, j, k = 0;

	for (i = 0; i < tp->pg_count; i++) {
		pg_va = tp->pg_va_tbl[i];

		for (j = 0; j < MAX_PAGE_PTRS(tp->pg_size); j++) {
			/* Only the data (leaf) link (set_pte_last) can be
			 * partial.  A deferred data page has a NULL VA: leave
			 * its PTE invalid; pbl_back_range() fills it in (with
			 * the correct LAST/NEXT_TO_LAST marker) when the owning
			 * pool is allocated.
			 */
			if (set_pte_last && !next_va[k]) {
				pg_va[j] = 0;
				if (++k >= tp_next->pg_count)
					return;
				continue;
			}

			if (k == tp_next->pg_count - 2 && set_pte_last)
				valid = PTU_PTE_NEXT_TO_LAST | PTU_PTE_VALID;
			else if (k == tp_next->pg_count - 1 && set_pte_last)
				valid = PTU_PTE_LAST | PTU_PTE_VALID;
			else
				valid = PTU_PTE_VALID;

			pg_va[j] = cpu_to_le64(pg_pa[k] | valid);
			if (++k >= tp_next->pg_count)
				return;
		}
	}
}

static void setup_page_table(struct tfc_ts_mem_cfg *mem_cfg)
{
	struct tfc_ts_page_tbl *tp_next;
	struct tfc_ts_page_tbl *tp;
	bool set_pte_last = 0;
	int i;

	for (i = 0; i < mem_cfg->num_lvl - 1; i++) {
		tp = &mem_cfg->pg_tbl[i];
		tp_next = &mem_cfg->pg_tbl[i + 1];
		if (i == mem_cfg->num_lvl - 2)
			set_pte_last = 1;
		link_page_table(tp, tp_next, set_pte_last);
	}

	mem_cfg->l0_addr = mem_cfg->pg_tbl[TFC_TS_PT_LVL_0].pg_va_tbl[0];
	mem_cfg->l0_dma_addr = mem_cfg->pg_tbl[TFC_TS_PT_LVL_0].pg_pa_tbl[0];
}

static void unlink_and_free(struct bnxt *bp, struct tfc_ts_mem_cfg *mem_cfg, u32 page_size)
{
	/* tf_em_free_page_table */
	struct tfc_ts_page_tbl *tp;
	int i;

	for (i = 0; i < mem_cfg->num_lvl; i++) {
		tp = &mem_cfg->pg_tbl[i];
		netdev_dbg(bp->dev, "EEM: Freeing page table: size %u lvl %d cnt %u\n",
			   page_size, i, tp->pg_count);

		/* tf_em_free_pg_tbl */
		free_pg_tbl(bp, tp);
	}

	mem_cfg->l0_addr = NULL;
	mem_cfg->l0_dma_addr = 0;
	mem_cfg->num_lvl = 0;
	mem_cfg->num_data_pages = 0;
}

/* Number of data (leaf) pages needed to hold @rec_cnt records. */
static u32 recs_to_pages(u32 rec_cnt, u32 page_size)
{
	return roundup64((u64)rec_cnt * RECORD_SIZE, page_size) / page_size;
}

/*
 * Decide whether on-demand record backing is usable for a region.
 *
 * Requires the module knob to be on and every pool to be a whole number of
 * pages so that per-pool backing never shares a leaf page across pools (which
 * would make per-pool alloc/rollback unsafe).
 *
 * For the lookup region, rec_start_offset is the size of the static bucket
 * array; it must also be a whole number of pages since the bucket pages are
 * allocated upfront and record pages are deferred.  Pass 0 for the action
 * region where there is no static-bucket concept.
 */
static bool ts_dynamic_ok(u32 rec_start_offset, u8 pool_sz_exp, u32 page_size)
{
	u32 recs_per_page = page_size / RECORD_SIZE;

	if (!bnxt_ts_dynamic_backing)
		return false;
	if (!recs_per_page)
		return false;
	if (rec_start_offset % recs_per_page)
		return false;
	if ((1U << pool_sz_exp) % recs_per_page)
		return false;

	return true;
}

/*
 * Compute the [first_page, first_page + page_cnt) range of data (leaf) pages
 * that back the records owned by @pool_id in a region.  LKUP records start
 * after the static bucket array (mem_cfg->lkup_rec_start_offset); ACT records
 * start at offset 0.  The record-to-pool mapping mirrors CREATE_OFFSET():
 * entry = (pool_id << pool_sz_exp) | record_offset.
 */
static void pool_page_range(const struct tfc_ts_mem_cfg *mem_cfg,
			    enum cfa_region_type region, u16 pool_id,
			    u8 pool_sz_exp, u32 page_size,
			    u32 *first_page, u32 *page_cnt)
{
	u32 rec_base = (region == CFA_REGION_TYPE_LKUP) ?
		       mem_cfg->lkup_rec_start_offset : 0;
	u32 start_rec = rec_base + ((u32)pool_id << pool_sz_exp);
	u32 pool_recs = 1U << pool_sz_exp;
	u32 recs_per_page = page_size / RECORD_SIZE;
	u32 first = start_rec / recs_per_page;
	u32 last = (start_rec + pool_recs - 1) / recs_per_page;

	*first_page = first;
	*page_cnt = last - first + 1;
}

/* Forward declaration for the alloc rollback path. */
static void pbl_unback_range(struct bnxt *bp, struct tfc_ts_mem_cfg *mem_cfg,
			     u32 page_size, u32 first_page, u32 page_cnt);

/*
 * Back a range of data (leaf) pages on demand: allocate each page and patch
 * its physical address (with VALID and, where applicable, LAST/NEXT_TO_LAST
 * markers) into the already-allocated parent index page.  The page-table
 * pointer arrays are shared with the copy stored in the tfo db, so the device
 * sees the new PTEs on its next PBL walk without any further HWRM message.
 */
static int pbl_back_range(struct bnxt *bp, struct tfc_ts_mem_cfg *mem_cfg,
			  u32 page_size, u32 first_page, u32 page_cnt)
{
	int data_lvl = mem_cfg->num_lvl - 1;
	struct tfc_ts_page_tbl *data;
	struct tfc_ts_page_tbl *parent;
	u32 ptrs = MAX_PAGE_PTRS(page_size);
	u32 backed = 0;
	u32 p;

	/* A single-level table has no parent index page to patch and is never
	 * deferred; nothing to do.
	 */
	if (data_lvl < 1)
		return 0;

	data = &mem_cfg->pg_tbl[data_lvl];
	parent = &mem_cfg->pg_tbl[data_lvl - 1];

	netdev_dbg(bp->dev,
		   "dynamic-back: alloc pages [%u, %u) page_sz=%u total=%llu B\n",
		   first_page, first_page + page_cnt, page_size,
		   (u64)page_cnt * page_size);

	for (p = first_page; p < first_page + page_cnt && p < data->pg_count; p++) {
		dma_addr_t pa;
		u64 *ppte;
		u64 valid;
		void *va;

		if (data->pg_va_tbl[p])		/* already backed */
			continue;

		va = dma_alloc_coherent(data->dma_dev, page_size, &pa, GFP_KERNEL);
		if (!va)
			goto err;

		data->pg_va_tbl[p] = va;
		data->pg_pa_tbl[p] = pa;

		if (p == data->pg_count - 1)
			valid = PTU_PTE_LAST | PTU_PTE_VALID;
		else if (p == data->pg_count - 2)
			valid = PTU_PTE_NEXT_TO_LAST | PTU_PTE_VALID;
		else
			valid = PTU_PTE_VALID;

		ppte = parent->pg_va_tbl[p / ptrs];
		ppte[p % ptrs] = cpu_to_le64(pa | valid);
		backed++;
	}

	/* Order the PTE writes before the records are used (the caller
	 * programs flows into this pool after we return).
	 */
	if (backed) {
		dma_wmb();
		netdev_dbg(bp->dev,
			   "dynamic-back: backed %u new pages (%llu B), %u already present\n",
			   backed, (u64)backed * page_size,
			   page_cnt - backed);
	}

	return 0;

err:
	/* Roll back only the pages allocated in this call.  Pools are backed
	 * and freed atomically, so [first_page, p) are exactly our new pages.
	 */
	pbl_unback_range(bp, mem_cfg, page_size, first_page, p - first_page);
	return -ENOMEM;
}

/*
 * Free a range of data (leaf) pages previously backed by pbl_back_range() and
 * invalidate their parent PTEs.  Used only for the alloc rollback path; steady
 * state reclaim happens in bulk at table-scope teardown.
 */
static void pbl_unback_range(struct bnxt *bp, struct tfc_ts_mem_cfg *mem_cfg,
			     u32 page_size, u32 first_page, u32 page_cnt)
{
	int data_lvl = mem_cfg->num_lvl - 1;
	struct tfc_ts_page_tbl *data;
	struct tfc_ts_page_tbl *parent;
	u32 ptrs = MAX_PAGE_PTRS(page_size);
	u32 p;

	if (data_lvl < 1)
		return;

	data = &mem_cfg->pg_tbl[data_lvl];
	parent = &mem_cfg->pg_tbl[data_lvl - 1];

	for (p = first_page; p < first_page + page_cnt && p < data->pg_count; p++) {
		u64 *ppte;

		if (!data->pg_va_tbl[p])
			continue;

		/* Invalidate the PTE before freeing so the device stops walking
		 * into the page.
		 */
		ppte = parent->pg_va_tbl[p / ptrs];
		ppte[p % ptrs] = 0;
		dma_wmb();

		dma_free_coherent(data->dma_dev, page_size,
				  data->pg_va_tbl[p], data->pg_pa_tbl[p]);
		data->pg_va_tbl[p] = NULL;
		data->pg_pa_tbl[p] = 0;
	}
}

/*
 * On-demand back the records for a just-allocated pool.  No-op when dynamic
 * backing is disabled or the alignment predicate fails (pages already present
 * from static allocation at scope config time).
 *
 * For the lookup region the upfront static-bucket pages are already present;
 * only the per-pool record pages are deferred.  For the action region no pages
 * are present at config time, so the first pool alloc backs them all.
 */
static int ts_pool_back(struct tfc *tfcp, u8 tsid, enum cfa_dir dir,
			enum cfa_region_type region, u16 pool_id)
{
	struct tfc_ts_mem_cfg mem_cfg;
	struct bnxt *bp = tfcp->bp;
	u32 first_page, page_cnt;
	u16 max_contig_rec;
	u8 pool_sz_exp;
	u32 page_size;
	int rc;

	rc = tfo_ts_get_mem_cfg(tfcp->tfo, tsid, dir, region, NULL, &mem_cfg);
	if (rc)
		return rc;

	page_size = mem_cfg.pg_tbl[TFC_TS_PT_LVL_0].pg_size;
	if (!page_size)
		return 0;	/* region not backed locally (e.g. VF) */

	rc = tfo_ts_get_pool_info(tfcp->tfo, tsid, dir, region,
				  &max_contig_rec, &pool_sz_exp);
	if (rc)
		return rc;

	/* rec_start_offset is 0 for the action region (no static buckets). */
	if (!ts_dynamic_ok(mem_cfg.lkup_rec_start_offset, pool_sz_exp, page_size))
		return 0;	/* static mode: pages already present */

	pool_page_range(&mem_cfg, region, pool_id, pool_sz_exp, page_size,
			&first_page, &page_cnt);

	netdev_dbg(bp->dev,
		   "%s: tsid(%d) %s pool_id=%u rec_base=%u pool_sz_exp=%u -> rec_range=[%u, %u) pages=[%u, %u) alloc=%llu B\n",
		   __func__, tsid, tfc_ts_region_2_str(region, dir),
		   pool_id, mem_cfg.lkup_rec_start_offset, pool_sz_exp,
		   mem_cfg.lkup_rec_start_offset + ((u32)pool_id << pool_sz_exp),
		   mem_cfg.lkup_rec_start_offset + ((u32)(pool_id + 1) << pool_sz_exp),
		   first_page, first_page + page_cnt,
		   (u64)page_cnt * page_size);

	rc = pbl_back_range(bp, &mem_cfg, page_size, first_page, page_cnt);
	if (rc)
		netdev_dbg(bp->dev,
			   "%s: tsid(%d) %s pool(%d) back %u pages @%u failed:%d\n",
			   __func__, tsid, tfc_ts_region_2_str(region, dir),
			   pool_id, page_cnt, first_page, rc);
	return rc;
}

/*
 * Allocate and link the PBL for a region.
 *
 * @backed_data_pages:	Number of leaf data pages to back immediately, starting
 *			at page 0.  Pass TFC_TS_BACK_ALL_PAGES to back the whole
 *			region (legacy/static behavior).  When fewer pages are
 *			requested, the index hierarchy is fully built but the
 *			trailing data pages are deferred and later populated by
 *			pbl_back_range().
 */
static int alloc_link_pbl(struct bnxt *bp, struct tfc_ts_mem_cfg *mem_cfg,
			  u32 page_size, u32 backed_data_pages)
{
	int rc;

	/* tf_em_size_page_tbl_lvl */
	rc = num_pages_get(mem_cfg, page_size);
	if (rc) {
		netdev_dbg(bp->dev, "EEM: Failed to size page table levels\n");
		netdev_dbg(bp->dev, "data-sz: %016llu page-sz: %u\n",
			   (u64)mem_cfg->rec_cnt * mem_cfg->entry_size, page_size);
		return rc;
	}

	/* tf_em_size_page_tbls */
	size_page_tbls(mem_cfg->num_lvl - 1, mem_cfg->num_data_pages, page_size,
		       mem_cfg->page_cnt);

	netdev_dbg(bp->dev, "EEM: lvls: %d sz: %016llu pgs: %016llu l0: %u l1: %u l2: %u backed: %u\n",
		   mem_cfg->num_lvl, mem_cfg->num_data_pages * page_size,
		   mem_cfg->num_data_pages, mem_cfg->page_cnt[TFC_TS_PT_LVL_0],
		   mem_cfg->page_cnt[TFC_TS_PT_LVL_1],
		   mem_cfg->page_cnt[TFC_TS_PT_LVL_2],
		   backed_data_pages);

	/* tf_em_alloc_page_table -> tf_em_alloc_pg_tbl */
	rc = alloc_page_table(bp, mem_cfg, page_size, backed_data_pages);
	if (rc)
		goto cleanup;

	/* tf_em_setup_page_table */
	setup_page_table(mem_cfg);

	return 0;

cleanup:
	unlink_and_free(bp, mem_cfg, page_size);
	return rc;
}

/**
 * Free TPM instances for scope
 *
 * @tfcp: Pointer to TFC handle
 * @tsid: Table scope identifier
 *
 * Returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
static int tbl_scope_pools_destroy(struct tfc *tfcp, u8 tsid)
{
	enum cfa_region_type region;
	struct bnxt *bp = tfcp->bp;
	void *tim, *tpm;
	int dir, rc;

	if (tfo_ts_validate(tfcp->tfo, tsid, NULL) != 0) {
		netdev_dbg(bp->dev, "%s: tsid(%d) invalid\n", __func__, tsid);
		return -EINVAL;
	}

	rc = tfo_tim_get(tfcp->tfo, &tim, tsid);
	if (rc)
		return -EINVAL;

	/* Free TIM, TPM instances for the given tsid. */
	if (tim) {
		for (region = 0; region < CFA_REGION_TYPE_MAX; region++) {
			for (dir = 0; dir < CFA_DIR_MAX; dir++) {
				rc = cfa_tim_tpm_inst_get(tim, tsid, region, dir, &tpm);
				if (rc) {
					netdev_dbg(bp->dev, "No TPM tsid(%d) region(%s)\n",
						   tsid, tfc_ts_region_2_str(region, dir));
					continue;
				}

				if (tpm) {
					rc = cfa_tim_tpm_inst_set(tim, tsid, region, dir, NULL);
					cfa_tpm_close(tpm);
					kfree(tpm);
				}
			}
		}
	}

	return rc;
}

/* tbl_scope_pools_create_parms contains the parameters for creating pools.
 */
struct tbl_scope_pools_create_parms {
	/* Indicates non-shared, shared-app or global scope. */
	enum cfa_scope_type scope_type;
	/* The number of pools the table scope will be divided into. (set
	 * to 1 if non-shared).
	 */
	u16 max_pools;
	/* The size of each individual lookup record pool expressed as:
	 * log2(max_records/max_pools).	 For example if 1024 records and 2 pools
	 * 1024/2=512=2^9, so the value 9 would be entered.
	 */
	u8 lkup_pool_sz_exp[CFA_DIR_MAX];
	/* The size of each individual action record pool expressed as:
	 * log2(max_records/max_pools).	 For example if 1024 records and 2 pools
	 * 1024/2=512=2^9, so the value 9 would be entered.
	 */
	u8 act_pool_sz_exp[CFA_DIR_MAX];
};

/**
 * Allocate and store TPM and TIM for shared-app or global scope
 *
 * Dynamically allocate and store TPM instances for shared-app or global scope
 *
 * @tfcp: Pointer to TFC handle
 * @tsid: Table scope identifier
 * @params: Parameters for allocate and store TPM instances for shared-app or
 *          global scope
 *
 * Returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
static int tbl_scope_pools_create(struct tfc *tfcp, u8 tsid,
				  struct tbl_scope_pools_create_parms *parms)
{
	void *tpms[CFA_DIR_MAX][CFA_REGION_TYPE_MAX];
	enum cfa_region_type region;
	struct bnxt *bp = tfcp->bp;
	void *tim = NULL;
	u32 tpm_db_size;
	int dir, rc;

	/* Dynamically allocate and store base addresses for TIM,
	 * TPM instances for the given tsid
	 */

	if (tfo_ts_validate(tfcp->tfo, tsid, NULL) != 0) {
		netdev_dbg(bp->dev, "%s: tsid(%d) invalid\n", __func__, tsid);
		return -EINVAL;
	}

	rc = tfo_tim_get(tfcp->tfo, &tim, tsid);
	if (rc) {
		netdev_dbg(bp->dev, "%s: tsid(%d) tim get error\n", __func__, tsid);
		return -EINVAL;
	}

	rc = cfa_tpm_query(parms->max_pools, &tpm_db_size);
	if (rc) {
		netdev_dbg(bp->dev, "%s: tsid(%d) tpm query error\n", __func__, tsid);
		return -EINVAL;
	}

	/* Clean up any existing TPMs for this tsid before allocating new ones */
	(void)tbl_scope_pools_destroy(tfcp, tsid);

	memset(tpms, 0, sizeof(void *) * CFA_DIR_MAX * CFA_REGION_TYPE_MAX);

	/* Allocate pool managers */
	for (region = 0; region < CFA_REGION_TYPE_MAX; region++) {
		for (dir = 0; dir < CFA_DIR_MAX; dir++) {
			tpms[dir][region] = kzalloc(tpm_db_size, GFP_KERNEL);
			if (!tpms[dir][region])
				goto cleanup;

			rc = cfa_tpm_open(tpms[dir][region], tpm_db_size, parms->max_pools);
			if (rc) {
				netdev_dbg(bp->dev, "%s: cfa_tpm_open() fails\n", __func__);
				goto cleanup;
			}
			rc = cfa_tpm_pool_size_set(tpms[dir][region],
						   (region == CFA_REGION_TYPE_LKUP ?
						    parms->lkup_pool_sz_exp[dir] :
						    parms->act_pool_sz_exp[dir]));
			if (rc) {
				netdev_dbg(bp->dev, "%s: cfa_tpm_pool_sz_set() fails\n", __func__);
				goto cleanup;
			}
			rc = cfa_tim_tpm_inst_set(tim, tsid, region, dir, tpms[dir][region]);
			if (rc) {
				netdev_dbg(bp->dev, "%s: cfa_tim_tpm_inst_set() fails\n", __func__);
				goto cleanup;
			}
			tpms[dir][region] = NULL;
		}
	}

	return 0;

 cleanup:
	/* Save the original error code before cleanup */
	{
		int cleanup_rc = rc;

		/* Clean up any TPMs that were added to TIM */
		if (tim)
			(void)tbl_scope_pools_destroy(tfcp, tsid);

		/* Clean up any TPMs that were allocated but not added to TIM */
		for (region = 0; region < CFA_REGION_TYPE_MAX; region++) {
			for (dir = 0; dir < CFA_DIR_MAX; dir++) {
				if (tpms[dir][region]) {
					cfa_tpm_close(tpms[dir][region]);
					kfree(tpms[dir][region]);
					tpms[dir][region] = NULL;
				}
			}
		}
		/* Restore the original error code */
		rc = cleanup_rc;
	}
	netdev_dbg(bp->dev, "%s: fails for tsid(%d)\n", __func__, tsid);
	return rc;
}

/*
 * Determine whether tsid's backing store was allocated locally by the PF
 * for its own use. Such a scope must never be touched via the VF2PF
 * channel, independent of any per-pool fid ownership check.
 *
 * Returns 0 for SUCCESS, negative error value for FAILURE (errno.h).
 * Callers must treat a non-zero return as authorization failure rather
 * than assume *is_pf_local, so a lookup error fails closed.
 */
static int tbl_scope_is_pf_local(struct tfc *tfcp, u8 tsid, bool *is_pf_local)
{
	struct tfc_ts_mem_cfg mem_cfg;

	return tfo_ts_get_mem_cfg(tfcp->tfo, tsid, CFA_DIR_RX, CFA_REGION_TYPE_LKUP,
				   is_pf_local, &mem_cfg);
}

/* True if fid belongs to one of bp's VFs. */
static bool tbl_scope_fid_is_vf(struct bnxt *bp, u16 fid)
{
	u32 first_vf_id = bp->pf.first_vf_id;

	if (!bp->pf.active_vfs)
		return false;

	return fid >= first_vf_id && fid < first_vf_id + bp->pf.active_vfs;
}

/*
 * Check whether fid is authorized to allocate/configure pools in tsid.
 *
 * VF2PF pool_alloc and mem_alloc requests must not be granted against a
 * tsid the requesting fid never joined -- otherwise any VF can supply an
 * in-range tsid it merely guessed and steal or exhaust another tenant's
 * shared table-scope pools. A fid is authorized if it already owns at
 * least one pool anywhere in tsid, or if no fid owns any pool in tsid
 * yet, in which case the first fid to touch it becomes its owner.
 *
 * CFA_SCOPE_TYPE_GLOBAL and CFA_SCOPE_TYPE_SHARED_APP tsids skip the
 * pool-ownership check for bp's own PF fid or one of its trusted VFs
 * (bnxt_is_trusted_fid()): a GLOBAL tsid is shared by an HW-LAG bonded
 * PF and its trusted VFs no matter which of them allocated the first
 * pool, and a SHARED_APP tsid is deliberately provisioned to be joined
 * by more than one trusted VF (e.g. multiple DPDK instances on the same
 * PF), so the first one to allocate a pool must not permanently lock
 * the others out. scope_type alone is not a trust signal -- any VF can
 * set it via tfc_vf2pf_mem_alloc_process() -- so an untrusted fid on a
 * GLOBAL- or SHARED_APP-flagged tsid still reaches the ownership walk
 * below.
 *
 * Caller must hold tfc_global_mutex: this walks the same tim/tpm state
 * that pool alloc/free mutate, and the check must stay atomic with the
 * allocation that follows it, or two racing fids can both be authorized
 * against the same not-yet-claimed tsid.
 *
 * @tim: Already-fetched TIM handle for tsid (tfo_tim_get()), so callers
 *	 that need it again right after this check (e.g. to look up a
 *	 TPM for the actual allocation) don't fetch it twice.
 *
 * Returns 0 if fid is authorized, -EPERM if it is not, or another
 * negative error value if the tpm lookup itself failed (errno.h).
 */
static int tbl_scope_fid_is_authorized(struct tfc *tfcp, u16 fid, u8 tsid, void *tim)
{
	struct bnxt *bp = tfcp->bp;
	enum cfa_scope_type scope_type;
	enum cfa_region_type region;
	bool any_fid_used = false;
	enum cfa_dir dir;
	int rc;

	if (tfo_ts_validate(tfcp->tfo, tsid, NULL) != 0) {
		netdev_dbg(bp->dev, "%s: tsid(%d) invalid\n", __func__, tsid);
		return -EINVAL;
	}

	if (!tbl_scope_fid_is_vf(bp, fid))
		return 0;

	rc = tfo_ts_get(tfcp->tfo, tsid, &scope_type, NULL, NULL, NULL);
	if (rc)
		return rc;

	if ((scope_type == CFA_SCOPE_TYPE_GLOBAL || scope_type == CFA_SCOPE_TYPE_SHARED_APP) &&
	    bnxt_is_trusted_fid(bp, fid))
		return 0;

	for (dir = 0; dir < CFA_DIR_MAX; dir++) {
		for (region = 0; region < CFA_REGION_TYPE_MAX; region++) {
			u16 fid_cnt = 0, pool_id;
			void *tpm = NULL;

			rc = cfa_tim_tpm_inst_get(tim, tsid, region, dir, &tpm);
			if (rc)
				return rc;
			if (!tpm)
				continue;

			if (cfa_tpm_srchm_by_fid(tpm, CFA_SRCH_MODE_FIRST, fid, &pool_id) == 0)
				return 0;

			rc = cfa_tpm_fid_cnt_get(tpm, &fid_cnt);
			if (rc)
				return rc;
			if (fid_cnt)
				any_fid_used = true;
		}
	}

	/* Nobody owns a pool in tsid yet; let the first fid to ask claim it. */
	return any_fid_used ? -EPERM : 0;
}

/**
 * Remove all associated pools owned by a function from TPM
 *
 * @tfcp: Pointer to TFC handle
 * @fid: function
 * @tsid: Table scope identifier
 * @max_fid: Highest fid to search when counting pools
 * @pool_cnt: Pointer to the number of pools still associated with other fids.
 *
 * Returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
static int tbl_scope_tpm_fid_rem(struct tfc *tfcp, u16 fid, u8 tsid,
				 u16 max_fid, u16 *pool_cnt)
{
	enum cfa_scope_type scope_type;
	enum cfa_region_type region;
	struct bnxt *bp = tfcp->bp;
	u16 pool_id, lfid;
	u16 found_cnt = 0;
	enum cfa_dir dir;
	void *tim, *tpm;
	bool valid;
	int rc;

	if (!pool_cnt) {
		netdev_dbg(bp->dev, "%s: Invalid pool_cnt pointer\n", __func__);
		return -EINVAL;
	}
	if (!tfc_bp_is_pf(tfcp)) {
		netdev_dbg(bp->dev, "%s: only valid for PF\n", __func__);
		return -EINVAL;
	}
	rc = tfo_ts_get(tfcp->tfo, tsid, &scope_type, NULL, &valid, NULL);
	if (!valid || scope_type == CFA_SCOPE_TYPE_NON_SHARED) {
		netdev_dbg(bp->dev, "%s: tsid(%d) valid(%s) scope_type(%s)\n",
			   __func__, tsid, valid ? "TRUE" : "FALSE",
			   tfc_scope_type_2_str(scope_type));
		return -EINVAL;
	}

	rc = tfo_tim_get(tfcp->tfo, &tim, tsid);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Failed to get TIM\n", __func__);
		return -EINVAL;
	}

	for (dir = 0; dir < CFA_DIR_MAX; dir++) {
		for (region = 0; region < CFA_REGION_TYPE_MAX; region++) {
			/* Get the TPM and then check to see if the fid is associated
			 * with any of the pools
			 */
			rc = cfa_tim_tpm_inst_get(tim, tsid, region, dir, &tpm);
			if (rc) {
				netdev_dbg(bp->dev, "%s: Failed to get TPM for tsid:%d dir:%d\n",
					   __func__, tsid, dir);
				return -EINVAL;
			}
			rc = cfa_tpm_srchm_by_fid(tpm, CFA_SRCH_MODE_FIRST, fid, &pool_id);
			if (rc) /* FID not used */
				continue;
			netdev_dbg(bp->dev, "%s: tsid(%d) fid(%d) region(%s) pool_id(%d)\n",
				   __func__, tsid, fid, tfc_ts_region_2_str(region, dir),
				   pool_id);
			do {
				/* Remove fid from pool */
				rc = cfa_tpm_fid_rem(tpm, pool_id, fid);
				if (rc)
					netdev_dbg(bp->dev,
						   "%s: cfa_tpm_fid_rem() failed for fid:%d pool:%d\n",
						   __func__, fid, pool_id);

				rc = cfa_tpm_free(tpm, pool_id);
				if (rc)
					netdev_dbg(bp->dev, "%s: tpm free failed tsid(%d) pool_id(%d)\n",
						   __func__, tsid, pool_id);

				rc = cfa_tpm_srchm_by_fid(tpm,
							  CFA_SRCH_MODE_NEXT,
							  fid, &pool_id);
				if (!rc)
					netdev_dbg(bp->dev, "%s: tsid(%d) fid(%d) region(%s) pool_id(%d)\n",
						   __func__, tsid, fid,
						   tfc_ts_region_2_str(region, dir),
						   pool_id);
			} while (!rc);
		}
	}

	for (dir = 0; dir < CFA_DIR_MAX; dir++) {
		for (region = 0; region < CFA_REGION_TYPE_MAX; region++) {
			/* Get the TPM and then check to see if the fid is associated
			 * with any of the pools
			 */
			rc = cfa_tim_tpm_inst_get(tim, tsid, region, dir, &tpm);
			if (rc) {
				netdev_dbg(bp->dev, "%s: Failed to get TPM for tsid:%d dir:%d\n",
					   __func__, tsid, dir);
				return -EINVAL;
			}
			if (!tpm)
				continue;
			for (lfid = BNXT_FIRST_PF_FID; lfid <= max_fid; lfid++) {
				rc = cfa_tpm_srchm_by_fid(tpm, CFA_SRCH_MODE_FIRST,
							  lfid, &pool_id);
				if (rc) /* FID not used */
					continue;
				netdev_dbg(bp->dev, "%s: tsid(%d) fid(%d) region(%s) pool_id(%d)\n",
					   __func__, tsid, lfid, tfc_ts_region_2_str(region, dir),
					   pool_id);
				do {
					found_cnt++;
					rc = cfa_tpm_srchm_by_fid(tpm,
								  CFA_SRCH_MODE_NEXT,
								  lfid, &pool_id);
					if (!rc) {
						netdev_dbg(bp->dev, "%s: tsid(%d) fid(%d) region(%s) pool_id(%d)\n",
							   __func__, tsid, lfid,
							   tfc_ts_region_2_str(region, dir),
							   pool_id);
					}
				} while (!rc);
			}
		}
	}
	*pool_cnt = found_cnt;
	return 0;
}

/* Public APIs */
int tfc_tbl_scope_qcaps(struct tfc *tfcp,
			struct tfc_tbl_scope_qcaps_parms *parms)
{
	struct bnxt *bp = tfcp->bp;
	int rc;

	if (!parms) {
		netdev_dbg(bp->dev, "%s: Invalid parms pointer\n", __func__);
		return -EINVAL;
	}
	rc = tfc_msg_tbl_scope_qcaps(tfcp, &parms->tbl_scope_cap,
				     &parms->global_cap,
				     &parms->locked_cap,
				     &parms->max_lkup_rec_cnt,
				     &parms->max_act_rec_cnt,
				     &parms->max_lkup_static_bucket_exp,
				     &parms->flow_scale_override);
	if (rc)
		netdev_dbg(bp->dev, "%s: table scope qcaps message failed, rc:%d\n", __func__, rc);

	return rc;
}

int tfc_tbl_scope_size_query(struct tfc *tfcp,
			     struct tfc_tbl_scope_size_query_parms *parms)
{
	struct bnxt *bp = tfcp->bp;
	enum cfa_dir dir;
	int rc;

	if (!parms) {
		netdev_dbg(bp->dev, "%s: Invalid parms pointer\n", __func__);
		return -EINVAL;
	}

	if (parms->factor > TFC_TBL_SCOPE_BUCKET_FACTOR_MAX) {
		netdev_dbg(bp->dev, "%s: Invalid factor %u\n", __func__, parms->factor);
		return -EINVAL;
	}

	if (!is_power_of_2(parms->max_pools)) {
		netdev_dbg(bp->dev, "%s: Invalid max_pools %u not pow2\n",
			   __func__, parms->max_pools);
		return -EINVAL;
	}

	for (dir = CFA_DIR_RX; dir < CFA_DIR_MAX; dir++) {
		rc = calc_lkup_rec_cnt(bp, parms->flow_cnt[dir],
				       parms->key_sz_in_bytes[dir],
				       parms->scope_type, parms->factor,
				       &parms->lkup_rec_cnt[dir],
				       &parms->static_bucket_cnt_exp[dir],
				       &parms->dynamic_bucket_cnt[dir]);
		if (rc)
			break;

		rc = calc_act_rec_cnt(bp, &parms->act_rec_cnt[dir],
				      parms->flow_cnt[dir],
				      parms->act_rec_sz_in_bytes[dir]);
		if (rc)
			break;

		rc = calc_pool_sz_exp(bp, &parms->lkup_pool_sz_exp[dir],
				      parms->lkup_rec_cnt[dir]  -
				      (1 << parms->static_bucket_cnt_exp[dir]),
				      parms->max_pools);
		if (rc)
			break;

		rc = calc_pool_sz_exp(bp, &parms->act_pool_sz_exp[dir],
				      parms->act_rec_cnt[dir],
				      parms->max_pools);
		if (rc)
			break;

		rc = calc_rec_start_offset(bp, &parms->lkup_rec_start_offset[dir],
					   parms->static_bucket_cnt_exp[dir]);
		if (rc)
			break;
	}

	return rc;
}

int tfc_tbl_scope_id_alloc(struct tfc *tfcp, enum cfa_scope_type scope_type,
			   enum cfa_app_type app_type, u8 *tsid,
			   bool *first)
{
	struct bnxt *bp = tfcp->bp;
	bool valid = true;
	int rc;

	if (!tsid) {
		netdev_dbg(bp->dev, "%s: Invalid tsid pointer\n", __func__);
		return -EINVAL;
	}
	if (!first) {
		netdev_dbg(bp->dev, "%s: Invalid first pointer\n", __func__);
		return -EINVAL;
	}
	if (app_type >= CFA_APP_TYPE_INVALID) {
		netdev_dbg(bp->dev, "%s: Invalid app type\n", __func__);
		return -EINVAL;
	}
	rc = tfc_msg_tbl_scope_id_alloc(tfcp, bp->pf.fw_fid, scope_type, app_type, tsid, first);
	if (rc) {
		netdev_dbg(bp->dev, "%s: table scope ID alloc message failed, rc:%d\n",
			   __func__, rc);
	} else {
		rc = tfo_ts_set(tfcp->tfo, *tsid, scope_type, app_type, valid, 0);
	}
	return rc;
}

void tfc_mem_alloc_parms_dump(struct bnxt *bp, u8 tsid, u16 fid,
			      const struct tfc_tbl_scope_mem_alloc_parms *parms)
{
	u64 rx_lkup = (u64)parms->lkup_rec_cnt[CFA_DIR_RX] * RECORD_SIZE;
	u64 tx_lkup = (u64)parms->lkup_rec_cnt[CFA_DIR_TX] * RECORD_SIZE;
	u64 rx_act  = (u64)parms->act_rec_cnt[CFA_DIR_RX]  * RECORD_SIZE;
	u64 tx_act  = (u64)parms->act_rec_cnt[CFA_DIR_TX]  * RECORD_SIZE;
	u64 total   = rx_lkup + tx_lkup + rx_act + tx_act;

	netdev_dbg(bp->dev, "%s: tsid=%u fid=%u local=%d max_pools=%u page_sz=%u\n",
		   __func__, tsid, fid, parms->local, parms->max_pools,
		   parms->pbl_page_sz_in_bytes);
	netdev_dbg(bp->dev,
		   "%s: RX lkup_rec_cnt=%u static_bkt_exp=%u lkup_pool_sz_exp=%u lkup_rec_start=%u\n",
		   __func__,
		   parms->lkup_rec_cnt[CFA_DIR_RX],
		   parms->static_bucket_cnt_exp[CFA_DIR_RX],
		   parms->lkup_pool_sz_exp[CFA_DIR_RX],
		   parms->lkup_rec_start_offset[CFA_DIR_RX]);
	netdev_dbg(bp->dev,
		   "%s: TX lkup_rec_cnt=%u static_bkt_exp=%u lkup_pool_sz_exp=%u lkup_rec_start=%u\n",
		   __func__,
		   parms->lkup_rec_cnt[CFA_DIR_TX],
		   parms->static_bucket_cnt_exp[CFA_DIR_TX],
		   parms->lkup_pool_sz_exp[CFA_DIR_TX],
		   parms->lkup_rec_start_offset[CFA_DIR_TX]);
	netdev_dbg(bp->dev,
		   "%s: RX act_rec_cnt=%u act_pool_sz_exp=%u\n",
		   __func__,
		   parms->act_rec_cnt[CFA_DIR_RX],
		   parms->act_pool_sz_exp[CFA_DIR_RX]);
	netdev_dbg(bp->dev,
		   "%s: TX act_rec_cnt=%u act_pool_sz_exp=%u\n",
		   __func__,
		   parms->act_rec_cnt[CFA_DIR_TX],
		   parms->act_pool_sz_exp[CFA_DIR_TX]);
	netdev_dbg(bp->dev,
		   "%s: memory: RX_lkup=%llu MB TX_lkup=%llu MB RX_act=%llu MB TX_act=%llu MB total=%llu MB (%llu GB)\n",
		    __func__,
		    rx_lkup >> 20, tx_lkup >> 20,
		    rx_act  >> 20, tx_act  >> 20,
		    total   >> 20, total   >> 30);
}

/* Unlocked core of tfc_tbl_scope_pool_alloc() */
static int __tfc_tbl_scope_pool_alloc(struct tfc *tfcp, u16 fid, u8 tsid,
				      enum cfa_region_type region, enum cfa_dir dir,
				      u8 *pool_sz_exp, u16 *pool_id);

int tfc_tbl_scope_mem_alloc(struct tfc *tfcp, u16 fid, u8 tsid,
			    struct tfc_tbl_scope_mem_alloc_parms *parms)
{
	struct tfc_ts_mem_cfg mem_cfg;
	enum cfa_region_type region;
	struct bnxt *bp = tfcp->bp;
	bool valid, cfg_done;
	u64 base_addr;
	u8 pbl_level;
	int dir, rc;
	u32 page_sz;
	u8 cfg_cnt;
	void *tim;

	if (!parms) {
		netdev_dbg(bp->dev, "%s: Invalid parms pointer\n", __func__);
		return -EINVAL;
	}

	if (!is_power_of_2(parms->max_pools)) {
		netdev_dbg(bp->dev, "%s: Invalid max_pools %u not pow2\n",
			   __func__, parms->max_pools);
		return -EINVAL;
	}

	/* Normalize page size to a power of 2 */
	page_sz = 1 << next_pow2(parms->pbl_page_sz_in_bytes);
	if (parms->pbl_page_sz_in_bytes != page_sz ||
	    (page_sz & VALID_PAGE_ALIGNMENTS) == 0) {
		netdev_dbg(bp->dev, "%s: Invalid page size %d\n", __func__,
			   parms->pbl_page_sz_in_bytes);
		return -EINVAL;
	}

	memset(&mem_cfg, 0, sizeof(mem_cfg));

	mutex_lock(&tfc_global_mutex);

	if (tfo_ts_validate(tfcp->tfo, tsid, &valid) != 0) {
		netdev_dbg(bp->dev, "%s: Invalid tsid(%d) object\n", __func__,
			   tsid);
		rc = -EINVAL;
		goto unlock;
	}

	if (parms->local && !valid) {
		netdev_dbg(bp->dev, "%s: tsid(%d) not allocated\n", __func__,
			   tsid);
		rc = -EINVAL;
		goto unlock;
	}

	/* If we are running on a PF, we will allocate memory locally */
	if (tfc_bp_is_pf(tfcp)) {
		struct tbl_scope_pools_create_parms cparms;
		cfg_done = false;
		cfg_cnt = 0;

		/* If scope is already valid and not local, do not reallocate */
		if (valid && !parms->local) {
			bool is_pf_local = false;

			/*
			 * parms->local is only ever set by the PF's own
			 * allocation path, never by a VF2PF request, so this
			 * only rejects VF requests targeting the PF's own
			 * scope.
			 */
			rc = tbl_scope_is_pf_local(tfcp, tsid, &is_pf_local);
			if (rc) {
				netdev_err(bp->dev, "%s: tsid(%d) pf-local lookup failed rc(%d)\n",
					   __func__, tsid, rc);
				goto unlock;
			}
			if (is_pf_local && fid != bp->pf.fw_fid) {
				netdev_err(bp->dev,
					   "%s: fid(%d) not authorized for PF-local tsid(%d)\n",
					   __func__, fid, tsid);
				rc = -EPERM;
				goto unlock;
			}

			rc = tfo_tim_get(tfcp->tfo, &tim, tsid);
			if (rc) {
				netdev_err(bp->dev, "%s: tsid(%d) Failed to get TIM rc(%d)\n",
					   __func__, tsid, rc);
				goto unlock;
			}

			rc = tbl_scope_fid_is_authorized(tfcp, fid, tsid, tim);
			if (rc) {
				netdev_err(bp->dev, "%s: fid(%d) not authorized for tsid(%d) rc(%d)\n",
					   __func__, fid, tsid, rc);
				goto unlock;
			}

			rc = tfo_ts_get(tfcp->tfo, tsid, &cparms.scope_type, NULL, NULL,
					&cparms.max_pools);

			if (rc) {
				netdev_err(bp->dev, "tsid(%d) partial config issue\n", tsid);
				goto cleanup;
			}
			for (dir = 0; dir < CFA_DIR_MAX; dir++) {
				rc = tfo_ts_get_pool_info(tfcp->tfo, tsid, dir,
							  CFA_REGION_TYPE_LKUP,
							  NULL,
							  &cparms.lkup_pool_sz_exp[dir]);
				if (rc) {
					netdev_dbg(bp->dev, "tsid(%d):(%s) rc(%d)\n", tsid,
						   tfc_ts_region_2_str(CFA_REGION_TYPE_LKUP, dir),
						   rc);
					break;
				}
				rc = tfo_ts_get_pool_info(tfcp->tfo, tsid, dir,
							  CFA_REGION_TYPE_ACT,
							  NULL,
							  &cparms.act_pool_sz_exp[dir]);
				if (rc) {
					netdev_dbg(bp->dev, "tsid(%d):(%s) rc(%d)\n", tsid,
						   tfc_ts_region_2_str(CFA_REGION_TYPE_ACT, dir),
						   rc);
					break;
				}
			}
			if (rc == 0) {
				mutex_unlock(&tfc_global_mutex);
				return 0;
			}
		}

		for (dir = 0; dir < CFA_DIR_MAX; dir++) {
			for (region = 0; region < CFA_REGION_TYPE_MAX; region++) {
				u32 backed_pages = TFC_TS_BACK_ALL_PAGES;

				if (cfg_cnt == 3)
					cfg_done = true;

				if (region == CFA_REGION_TYPE_LKUP) {
					mem_cfg.rec_cnt = parms->lkup_rec_cnt[dir];
					mem_cfg.lkup_rec_start_offset =
						1 << parms->static_bucket_cnt_exp[dir];
					mem_cfg.entry_size = RECORD_SIZE;

					/* Lazy backing: allocate only the static
					 * bucket pages now; lookup record pages
					 * are backed on demand at pool alloc time.
					 */
					if (ts_dynamic_ok(mem_cfg.lkup_rec_start_offset,
							  parms->lkup_pool_sz_exp[dir],
							  parms->pbl_page_sz_in_bytes))
						backed_pages =
							recs_to_pages(mem_cfg.lkup_rec_start_offset,
								      parms->pbl_page_sz_in_bytes);
				} else {
					mem_cfg.rec_cnt = parms->act_rec_cnt[dir];
					mem_cfg.lkup_rec_start_offset = 0;
					mem_cfg.entry_size = RECORD_SIZE;

					/* Lazy backing: action region has no
					 * static-bucket concept so no pages need
					 * to be present at scope config time.
					 * Back all records on demand at pool alloc.
					 */
					if (ts_dynamic_ok(0,
							  parms->act_pool_sz_exp[dir],
							  parms->pbl_page_sz_in_bytes))
						backed_pages = 0;
				}

				rc = alloc_link_pbl(bp, &mem_cfg,
						    parms->pbl_page_sz_in_bytes,
						    backed_pages);
				if (rc)
					goto cleanup;

				/*
				 * Report how much host memory was committed for
				 * this region right now.  For a dynamically-backed
				 * lookup region this is only the static-bucket
				 * data pages plus the PBL index pages; the
				 * record data pages are deferred to pool alloc.
				 * For a statically backed region the full region
				 * is present immediately.
				 *
				 * Index pages: all levels except the data leaf.
				 * Data pages backed now: min(backed_pages,
				 *                           num_data_pages).
				 */
				{
					u32 idx_pages = 0;
					u32 data_backed;
					u64 idx_bytes, data_bytes;
					u32 data_lvl = mem_cfg.num_lvl - 1;
					int lvl;

					for (lvl = 0; lvl < data_lvl; lvl++)
						idx_pages += mem_cfg.page_cnt[lvl];

					data_backed = min_t(u32, backed_pages,
							    (u32)mem_cfg.num_data_pages);
					idx_bytes  = (u64)idx_pages  * parms->pbl_page_sz_in_bytes;
					data_bytes = (u64)data_backed * parms->pbl_page_sz_in_bytes;

					if (backed_pages == TFC_TS_BACK_ALL_PAGES) {
						netdev_dbg(bp->dev,
							   "%s: %s static: data=%llu B (%u pgs) index=%llu B (%u pgs) total=%llu B\n",
							   __func__,
							   tfc_ts_region_2_str(region, dir),
							   data_bytes, data_backed,
							   idx_bytes, idx_pages,
							   data_bytes + idx_bytes);
					} else if (region == CFA_REGION_TYPE_LKUP) {
						u32 deferred = (u32)mem_cfg.num_data_pages
								- data_backed;
						u64 deferred_bytes = (u64)deferred *
								     parms->pbl_page_sz_in_bytes;

						netdev_dbg(bp->dev,
							   "%s: %s dynamic: static_bkt=%llu B (%u pgs) deferred=%llu B (%u pgs) index=%llu B (%u pgs) upfront_total=%llu B\n",
							   __func__,
							   tfc_ts_region_2_str(region, dir),
							   data_bytes, data_backed,
							   deferred_bytes, deferred,
							   idx_bytes, idx_pages,
							   data_bytes + idx_bytes);
					} else {
						u32 deferred = (u32)mem_cfg.num_data_pages
								- data_backed;
						u64 deferred_bytes = (u64)deferred *
								     parms->pbl_page_sz_in_bytes;

						netdev_dbg(bp->dev,
							   "%s: %s dynamic: upfront=0 B deferred=%llu B (%u pgs) index=%llu B (%u pgs) upfront_total=%llu B\n",
							   __func__,
							   tfc_ts_region_2_str(region, dir),
							   deferred_bytes, deferred,
							   idx_bytes, idx_pages,
							   idx_bytes);
					}
				}

				base_addr = mem_cfg.l0_dma_addr;
				pbl_level = mem_cfg.num_lvl - 1;

				rc = tfc_msg_backing_store_cfg_v2(tfcp, tsid, dir,
								  region,
								  base_addr,
								  pbl_level,
								  parms->pbl_page_sz_in_bytes,
								  mem_cfg.rec_cnt,
								  region == CFA_REGION_TYPE_LKUP ?
								  parms->static_bucket_cnt_exp[dir]
								  : 0,
								  parms->ct, cfg_done);
				if (rc) {
					netdev_dbg(bp->dev,
						   "%s: backing store cfg err region(%s) rc:%d\n",
						   __func__, tfc_ts_region_2_str(region, dir), rc);
					goto cleanup;
				}
				/* Set scope type and valid in local state */
				valid = true;
				rc = tfo_ts_set(tfcp->tfo, tsid, parms->scope_type, CFA_APP_TYPE_TF,
						valid, parms->max_pools);
				if (rc)
					goto cleanup;

				/* Set mem_cfg only *after* setting the scope type for correct db */
				rc = tfo_ts_set_mem_cfg(tfcp->tfo, tsid, dir, region,
							parms->local, &mem_cfg);
				if (rc) {
					netdev_err(bp->dev, "%s: tsid(%d) set_mem_cfg failed:%d\n",
						   __func__, tsid, rc);
					goto cleanup;
				}

				rc = tfo_ts_set_pool_info(tfcp->tfo, tsid, dir, region,
							  0, region == CFA_REGION_TYPE_LKUP ?
							  parms->lkup_pool_sz_exp[dir] :
							  parms->act_pool_sz_exp[dir]);
				if (rc) {
					netdev_dbg(bp->dev, "%s: set pool tsid(%d:%s) fail rc(%d)\n",
						   __func__, tsid, tfc_ts_region_2_str(region, dir),
						   rc);
					goto cleanup;
				}

				cfg_cnt++;
			}
			cparms.scope_type = parms->scope_type;
			cparms.max_pools = parms->max_pools;

			cparms.lkup_pool_sz_exp[dir] = parms->lkup_pool_sz_exp[dir];
			cparms.act_pool_sz_exp[dir] = parms->act_pool_sz_exp[dir];

			rc = tbl_scope_pools_create(tfcp, tsid, &cparms);
			if (rc) {
				netdev_dbg(bp->dev, "%s: pools create failed\n", __func__);
				goto cleanup;
			}
		}

		/* If not shared, allocate the single pool_id in each region
		 * so that we can save the associated fid for the table scope
		 */
		if (parms->scope_type == CFA_SCOPE_TYPE_NON_SHARED) {
			u16 pool_id;
			enum cfa_region_type region;

			/* Call the unlocked core directly: */
			for (region = 0; region < CFA_REGION_TYPE_MAX; region++) {
				for (dir = 0; dir < CFA_DIR_MAX; dir++) {
					rc = __tfc_tbl_scope_pool_alloc(tfcp,
									fid,
									tsid,
									region,
									dir,
									NULL,
									&pool_id);
					if (rc)
						goto cleanup;
					/* only 1 pool available */
					if (pool_id != 0)
						goto cleanup;
				}
			}

			rc = tfo_ts_set_owner_fid(tfcp->tfo, tsid, fid);
			if (rc)
				goto cleanup;
		}
	} else /* this is a VF */ {
		/* If first or !shared, send message to PF to allocate the memory */
		if (parms->first || parms->scope_type == CFA_SCOPE_TYPE_NON_SHARED) {
			struct tfc_vf2pf_tbl_scope_mem_alloc_cfg_cmd req = { { 0 } };
			struct tfc_vf2pf_tbl_scope_mem_alloc_cfg_resp resp = { { 0 } };
			u16 fid = tfc_get_fid(tfcp);

			req.hdr.type = TFC_VF2PF_TYPE_TBL_SCOPE_MEM_ALLOC_CFG_CMD;
			req.hdr.fid = fid;
			req.tsid = tsid;
			req.max_pools = parms->max_pools;
			req.scope_type = parms->scope_type;
			for (dir = CFA_DIR_RX; dir < CFA_DIR_MAX; dir++) {
				req.static_bucket_cnt_exp[dir] = parms->static_bucket_cnt_exp[dir];
				req.dynamic_bucket_cnt[dir] = parms->dynamic_bucket_cnt[dir];
				req.lkup_rec_cnt[dir] = parms->lkup_rec_cnt[dir];
				req.lkup_pool_sz_exp[dir] = parms->lkup_pool_sz_exp[dir];
				req.act_pool_sz_exp[dir] = parms->act_pool_sz_exp[dir];
				req.act_rec_cnt[dir] = parms->act_rec_cnt[dir];
				req.lkup_rec_start_offset[dir] = parms->lkup_rec_start_offset[dir];
			}

			rc = tfc_vf2pf_mem_alloc(tfcp, &req, &resp);
			if (rc) {
				netdev_dbg(bp->dev, "%s: tfc_vf2pf_mem_alloc failed\n", __func__);
				goto cleanup;
			}

			netdev_dbg(bp->dev, "%s: tsid: %d, status %d\n", __func__,
				   resp.tsid, resp.status);
		}
		/* Set scope type and valid in local state */
		valid = true;
		rc = tfo_ts_set(tfcp->tfo, tsid, parms->scope_type, CFA_APP_TYPE_TF,
				valid, parms->max_pools);

		/* Save off info for later use */
		for (dir = CFA_DIR_RX; dir < CFA_DIR_MAX; dir++) {
			for (region = 0; region < CFA_REGION_TYPE_MAX; region++) {
				if (region == CFA_REGION_TYPE_LKUP) {
					mem_cfg.rec_cnt = parms->lkup_rec_cnt[dir];
					mem_cfg.lkup_rec_start_offset =
						1 << parms->static_bucket_cnt_exp[dir];
				} else {
					mem_cfg.rec_cnt = parms->act_rec_cnt[dir];
				}
				mem_cfg.entry_size = RECORD_SIZE;

				rc = tfo_ts_set_mem_cfg(tfcp->tfo,
							tsid,
							dir,
							region,
							true,
							&mem_cfg);
				if (rc)
					goto cleanup;

				rc = tfo_ts_set_pool_info(tfcp->tfo, tsid, dir,
							  region, 0,
							  region == CFA_REGION_TYPE_LKUP ?
							  parms->lkup_pool_sz_exp[dir] :
							  parms->act_pool_sz_exp[dir]);
				if (rc) {
					netdev_dbg(bp->dev, "%s: get tsid(%d:%s) rc(%d)\n",
						   __func__, tsid, tfc_ts_region_2_str(region, dir),
						   rc);
					goto cleanup;
				}
			}
		}
	}

unlock:
	mutex_unlock(&tfc_global_mutex);
	return rc;

cleanup:
	for (dir = 0; dir < CFA_DIR_MAX; dir++) {
		for (region = 0; region < CFA_REGION_TYPE_MAX; region++) {
			rc = tfo_ts_get_mem_cfg(tfcp->tfo, tsid, dir, region, 0, &mem_cfg);
			if (!rc)
				unlink_and_free(bp, &mem_cfg, parms->pbl_page_sz_in_bytes);

			memset(&mem_cfg, 0, sizeof(mem_cfg));

			(void)tfo_ts_set_mem_cfg(tfcp->tfo, tsid, dir,
						 region,
						 parms->local,
						 &mem_cfg);
		}
	}
	mutex_unlock(&tfc_global_mutex);
	return rc;
}

static int tbl_scope_host_mem_free(struct tfc *tfcp, u8 tsid)
{
	struct tfc_ts_mem_cfg mem_cfg;
	enum cfa_region_type region;
	struct bnxt *bp = tfcp->bp;
	int dir, rc = 0, lrc;
	bool local;

	for (region = 0; region < CFA_REGION_TYPE_MAX; region++) {
		for (dir = 0; dir < CFA_DIR_MAX; dir++) {
			lrc = tfo_ts_get_mem_cfg(tfcp->tfo, tsid, dir, region, &local,
						 &mem_cfg);
			if (lrc) {
				rc = lrc;
				netdev_err(bp->dev, "%s: tsid(%d) db err(%d), continuing\n",
					   __func__, tsid, rc);
				continue;
			}
			/* memory only allocated on PF */
			if (tfc_bp_is_pf(tfcp)) {
				netdev_dbg(bp->dev, "unlink tsid(%d) dir(%d) region(%d)\n",
					   tsid, dir, region);
				unlink_and_free(bp, &mem_cfg, mem_cfg.pg_tbl[0].pg_size);
			}
			memset(&mem_cfg, 0, sizeof(mem_cfg));
			tfo_ts_set_mem_cfg(tfcp->tfo, tsid, dir, region, local,
					   &mem_cfg);
		}
	}
	return rc;
}

int tfc_tbl_scope_mem_free(struct tfc *tfcp, u16 fid, u8 tsid)
{
	enum cfa_scope_type scope_type;
	struct tfc_ts_mem_cfg mem_cfg;
	struct bnxt *bp = tfcp->bp;
	u16 max_fid;
	bool local;
	int rc;

	if (tfo_ts_validate(tfcp->tfo, tsid, NULL) != 0) {
		netdev_dbg(bp->dev, "%s: tsid(%d) invalid\n", __func__, tsid);
		return -EINVAL;
	}

	rc = tfo_get_max_fid(tfcp->tfo, &max_fid);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Failed to get max fid for tsid:%d fid:%d\n",
			   __func__, tsid, fid);
		return rc;
	}

	mutex_lock(&tfc_global_mutex);

	rc = tfo_ts_get(tfcp->tfo, tsid, &scope_type, NULL, NULL, NULL);
	if (rc)
		goto unlock;

	/* Lookup any memory config to get local */
	rc = tfo_ts_get_mem_cfg(tfcp->tfo, tsid, CFA_DIR_RX, CFA_REGION_TYPE_LKUP,
				&local, &mem_cfg);
	if (rc)
		goto unlock;

	if (!tfc_bp_is_pf(tfcp)) {
		struct tfc_vf2pf_tbl_scope_mem_free_cmd req = {{ 0 }};
		struct tfc_vf2pf_tbl_scope_mem_free_resp resp = {{ 0 }};
		u16 fid = tfc_get_fid(tfcp);

		req.hdr.type = TFC_VF2PF_TYPE_TBL_SCOPE_MEM_FREE_CMD;
		req.hdr.fid = fid;
		req.tsid = tsid;

		rc = tfc_vf2pf_mem_free(tfcp, &req, &resp);
		if (rc) {
			netdev_dbg(bp->dev, "%s: tfc_vf2pf_mem_free failed\n", __func__);
			/* continue cleanup regardless */
		}
		netdev_dbg(bp->dev, "%s: tsid: %d, status %d\n", __func__, resp.tsid, resp.status);
	} else {
		if (scope_type != CFA_SCOPE_TYPE_NON_SHARED) {
			u16 pool_cnt;

			rc = tbl_scope_tpm_fid_rem(tfcp, fid, tsid, max_fid, &pool_cnt);
			if (rc) {
				netdev_dbg(bp->dev, "%s: error getting tsid(%d) pools status %d\n",
					   __func__, tsid, rc);
				goto unlock;
			}
			/* Then if there are still fids present, return */
			if (pool_cnt) {
				netdev_dbg(bp->dev, "%s: tsid(%d) fids still present pool_cnt(%d)\n",
					   __func__, tsid, pool_cnt);
				rc = 0;
				goto unlock;
			}
		} else {
			u16 owner_fid;

			/*
			 * Independent of the ownership check below: the PF's
			 * own local scope must never be freed via the VF
			 * channel, regardless of what owner_fid records.
			 */
			if (local && fid != bp->pf.fw_fid) {
				netdev_err(bp->dev,
					   "%s: fid(%d) not authorized to free PF-local tsid(%d)\n",
					   __func__, fid, tsid);
				rc = -EPERM;
				goto unlock;
			}

			/*
			 * A non-shared scope has exactly one owner: the fid
			 * that allocated it, recorded at mem_alloc time.
			 * Without this check, any fid -- including an
			 * unrelated VF -- could free a scope it never owned.
			 */
			rc = tfo_ts_get_owner_fid(tfcp->tfo, tsid, &owner_fid);
			if (rc) {
				netdev_err(bp->dev, "%s: tsid(%d) owner lookup failed rc(%d)\n",
					   __func__, tsid, rc);
				goto unlock;
			}
			if (owner_fid != fid) {
				netdev_err(bp->dev,
					   "%s: fid(%d) not authorized to free tsid(%d)\n",
					   __func__, fid, tsid);
				rc = -EPERM;
				goto unlock;
			}
		}
	}

	tbl_scope_host_mem_free(tfcp, tsid);

	if (tfc_bp_is_pf(tfcp)) {
		rc = tbl_scope_pools_destroy(tfcp, tsid);
		if (rc) {
			netdev_dbg(bp->dev, "%s: tsid(%d)  pool err(%d) continuing\n",
				   __func__, tsid, rc);
		}
	}
	/* cleanup state */
	rc = tfo_ts_set(tfcp->tfo, tsid, CFA_SCOPE_TYPE_INVALID, CFA_APP_TYPE_INVALID, false, 0);

unlock:
	mutex_unlock(&tfc_global_mutex);
	return rc;
}

int tfc_tbl_scope_fid_add(struct tfc *tfcp, u16 fid, u8 tsid, u16 *fid_cnt)
{
	struct bnxt *bp = tfcp->bp;
	int rc;

	if (bp->pf.fw_fid != fid) {
		netdev_dbg(bp->dev, "%s: Invalid fid\n", __func__);
		return -EINVAL;
	}

	if (tfo_ts_validate(tfcp->tfo, tsid, NULL) != 0) {
		netdev_dbg(bp->dev, "%s: tsid(%d) invalid\n", __func__, tsid);
		return -EINVAL;
	}

	rc = tfc_msg_tbl_scope_fid_add(tfcp, fid, tsid, fid_cnt);
	if (rc)
		netdev_dbg(bp->dev, "%s: table scope fid add message failed, rc:%d\n",
			   __func__, rc);

	return rc;
}

int tfc_tbl_scope_fid_rem(struct tfc *tfcp, u16 fid, u8 tsid, u16 *fid_cnt)
{
	struct bnxt *bp = tfcp->bp;
	int rc;

	if (bp->pf.fw_fid != fid) {
		netdev_dbg(bp->dev, "%s: Invalid fid\n", __func__);
		return -EINVAL;
	}

	if (tfo_ts_validate(tfcp->tfo, tsid, NULL) != 0) {
		netdev_dbg(bp->dev, "%s: tsid(%d) invalid\n", __func__, tsid);
		return -EINVAL;
	}

	rc = tfc_msg_tbl_scope_fid_rem(tfcp, fid, tsid, fid_cnt);
	if (rc)
		netdev_dbg(bp->dev, "%s: table scope fid rem message failed, rc:%d\n",
			   __func__, rc);

	return rc;
}

int tfc_tbl_scope_cpm_alloc(struct tfc *tfcp, u8 tsid,
			    struct tfc_tbl_scope_cpm_alloc_parms *parms)
{
	struct tfc_cmm *cmm_lkup = NULL;
	struct tfc_cmm *cmm_act = NULL;
	enum cfa_scope_type scope_type;
	struct bnxt *bp = tfcp->bp;
	struct tfc_cpm *lkup_cpm;
	struct tfc_cpm *act_cpm;
	u16 lkup_max_contig_rec;
	u16 act_max_contig_rec;
	u8 lkup_pool_sz_exp;
	u8 act_pool_sz_exp;
	int dir, rc;

	if (tfo_ts_validate(tfcp->tfo, tsid, NULL) != 0) {
		netdev_dbg(bp->dev, "%s: tsid(%d) invalid\n", __func__, tsid);
		return -EINVAL;
	}
	if (tfo_ts_get(tfcp->tfo, tsid, &scope_type, NULL, NULL, NULL)) {
		netdev_dbg(bp->dev, "%s: tsid(%d) info get failed\n", __func__, tsid);
		return -EINVAL;
	}

	/* Create 4 CPM instances and set the pool_sz_exp and max_pools for each */
	for (dir = 0; dir < CFA_DIR_MAX; dir++) {
		tfo_ts_get_pool_info(tfcp->tfo, tsid, dir, CFA_REGION_TYPE_LKUP,
				     NULL, &lkup_pool_sz_exp);
		if (rc) {
			netdev_dbg(bp->dev,
				   "%s: Failed to get pool info for tsid(%d:%s_lkup)\n",
				   __func__, tsid, tfc_dir_2_str(dir));
			return -EINVAL;
		}
		tfo_ts_get_pool_info(tfcp->tfo, tsid, dir, CFA_REGION_TYPE_ACT,
				     NULL, &act_pool_sz_exp);
		if (rc) {
			netdev_dbg(bp->dev,
				   "%s: Failed to get act pool info for tsid(%d:%s_act)\n",
				   __func__, tsid, tfc_dir_2_str(dir));
			return -EINVAL;
		}

		lkup_max_contig_rec = parms->lkup_max_contig_rec[dir];
		act_max_contig_rec = parms->act_max_contig_rec[dir];
		tfc_cpm_open(&lkup_cpm, parms->max_pools);
		tfc_cpm_set_pool_size(lkup_cpm, (1 << lkup_pool_sz_exp));
		tfc_cpm_open(&act_cpm, parms->max_pools);
		tfc_cpm_set_pool_size(act_cpm, (1 << act_pool_sz_exp));

		rc = tfo_ts_set_cpm_inst(tfcp->tfo, tsid, bp->pf.fw_fid, dir, CFA_REGION_TYPE_LKUP,
					 lkup_cpm);
		if (rc) {
			netdev_dbg(bp->dev, "%s: failed to set CPM instance\n", __func__);
			return -EINVAL;
		}
		rc = tfo_ts_set_cpm_inst(tfcp->tfo, tsid, bp->pf.fw_fid, dir, CFA_REGION_TYPE_ACT,
					 act_cpm);
		if (rc) {
			netdev_dbg(bp->dev, "%s: failed to set CPM instance\n", __func__);
			return -EINVAL;
		}
		rc = tfo_ts_set_pool_info(tfcp->tfo, tsid, dir, CFA_REGION_TYPE_LKUP,
					  lkup_max_contig_rec, lkup_pool_sz_exp);
		if (rc) {
			netdev_dbg(bp->dev, "%s: failed to set pool info\n", __func__);
			return -EINVAL;
		}
		rc = tfo_ts_set_pool_info(tfcp->tfo, tsid, dir, CFA_REGION_TYPE_ACT,
					  act_max_contig_rec, act_pool_sz_exp);
		if (rc) {
			netdev_dbg(bp->dev, "%s: failed to set pool info\n", __func__);
			return -EINVAL;
		}

		/* If not shared create CMM instance for and populate CPM with pool_id 0
		 * If shared, a pool_id will be allocated during tfc_act_alloc() or
		 * tfc_em_insert() and the CMM instance will be created on the first
		 * call.
		 */
		if (scope_type == CFA_SCOPE_TYPE_NON_SHARED) {
			struct cfa_mm_query_parms qparms;
			struct cfa_mm_open_parms oparms;
			struct tfc_ts_mem_cfg mem_cfg;
			u32 pool_id = 0;

			/* ACTION */
			rc = tfo_ts_get_mem_cfg(tfcp->tfo, tsid, dir, CFA_REGION_TYPE_ACT,
						NULL, &mem_cfg);
			if (rc) {
				netdev_dbg(bp->dev, "%s: tfo_ts_get_mem_cfg() failed: %d\n",
					   __func__, rc);
				return -EINVAL;
			}
			/* override the record size since a single pool because
			 * pool_sz_exp is 0 in this case
			 */
			tfc_cpm_set_pool_size(act_cpm, mem_cfg.rec_cnt);

			/*create CMM instance */
			qparms.max_records = mem_cfg.rec_cnt;
			qparms.max_contig_records = roundup_pow_of_two(act_max_contig_rec);

			rc = cfa_mm_query(&qparms);
			if (rc) {
				netdev_dbg(bp->dev, "%s: cfa_mm_query() failed: %d\n",
					   __func__, rc);
				return -EINVAL;
			}

			cmm_act = vzalloc(qparms.db_size);
			if (!cmm_act) {
				rc = -ENOMEM;
				goto cleanup;
			}
			oparms.db_mem_size = qparms.db_size;
			oparms.max_contig_records = qparms.max_contig_records;
			oparms.max_records = qparms.max_records;
			rc = cfa_mm_open(cmm_act, &oparms);
			if (rc) {
				netdev_dbg(bp->dev, "%s: cfa_mm_open() failed: %d\n",
					   __func__, rc);
				rc = -EINVAL;
				goto cleanup;
			}
			/* Store CMM instance in the CPM for pool_id 0 */
			rc = tfc_cpm_set_cmm_inst(act_cpm, pool_id, cmm_act);
			if (rc) {
				netdev_dbg(bp->dev, "%s: set cpm instance act failed: %d\n",
					   __func__, rc);
				rc = -EINVAL;
				goto cleanup;
			}
			/* LOOKUP */
			rc = tfo_ts_get_mem_cfg(tfcp->tfo, tsid, dir, CFA_REGION_TYPE_LKUP,
						NULL, &mem_cfg);
			if (rc) {
				netdev_dbg(bp->dev, "%s: tfo_ts_get_mem_cfg() failed: %c\n",
					   __func__, rc);
				rc = -EINVAL;
				goto cleanup;
			}
			/* Create lkup pool CMM instance */
			qparms.max_records = mem_cfg.rec_cnt;
			qparms.max_contig_records = roundup_pow_of_two(lkup_max_contig_rec);
			rc = cfa_mm_query(&qparms);
			if (rc) {
				netdev_dbg(bp->dev, "%s: cfa_mm_query() failed: %d\n",
					   __func__, rc);
				rc = -EINVAL;
				goto cleanup;
			}
			cmm_lkup = vzalloc(qparms.db_size);
			if (!cmm_lkup) {
				rc = -ENOMEM;
				goto cleanup;
			}
			oparms.db_mem_size = qparms.db_size;
			oparms.max_contig_records = qparms.max_contig_records;
			oparms.max_records = qparms.max_records;
			rc = cfa_mm_open(cmm_lkup, &oparms);
			if (rc) {
				netdev_dbg(bp->dev, "%s: cfa_mm_open() failed: %d\n",
					   __func__, rc);
				rc = -EINVAL;
				goto cleanup;
			}
			/* override the record size since a single pool because
			 * pool_sz_exp is 0 in this case
			 */
			tfc_cpm_set_pool_size(lkup_cpm, mem_cfg.rec_cnt);

			/* Store CMM instance in the CPM for pool_id 0 */
			rc = tfc_cpm_set_cmm_inst(lkup_cpm, pool_id, cmm_lkup);
			if (rc) {
				netdev_dbg(bp->dev, "%s: tfc_cpm_set_cmm_inst() lkup failed: %d\n",
					   __func__, rc);
				rc = -EINVAL;
				goto cleanup;
			}
		}
	}
	/* Ensure max_pools is set for the CPM instance. (required if not the first instance */
	tfo_ts_set(tfcp->tfo, tsid, scope_type, CFA_APP_TYPE_TF, 1, parms->max_pools);
	return 0;
cleanup:
	vfree(cmm_act);
	vfree(cmm_lkup);

	return rc;
}

int tfc_tbl_scope_cpm_free(struct tfc *tfcp, u8 tsid)
{
	struct bnxt *bp = tfcp->bp;
	enum cfa_region_type region;
	struct tfc_cpm *cpm;
	u16 max_contig_rec;
	int dir, rc = 0;
	u8 pool_sz_exp;

	if (tfo_ts_validate(tfcp->tfo, tsid, NULL) != 0) {
		netdev_dbg(bp->dev, "%s: tsid(%d) invalid\n", __func__, tsid);
		return -EINVAL;
	}

	for (dir = 0; dir < CFA_DIR_MAX; dir++) {
		for (region = 0; region < CFA_REGION_TYPE_MAX; region++) {
			uint16_t pool_id;
			struct tfc_cmm *cmm;
			enum cfa_srch_mode srch_mode;

			rc = tfo_ts_get_pool_info(tfcp->tfo, tsid, dir, region, &max_contig_rec,
						  &pool_sz_exp);
			if (rc) {
				netdev_dbg(bp->dev, "%s: pool info error(%d)\n", __func__, rc);
				continue;
			}
			netdev_dbg(bp->dev, "%s: fid(%d)\n", __func__, bp->pf.fw_fid);
			rc = tfo_ts_get_cpm_inst(tfcp->tfo, tsid, bp->pf.fw_fid, dir, region, &cpm);
			if (rc) {
				netdev_dbg(bp->dev, "%s: cpm inst error(%d)\n", __func__, rc);
				continue;
			}
			srch_mode = CFA_SRCH_MODE_FIRST;
			do {
				rc = tfc_cpm_srchm_by_configured_pool(cpm, srch_mode,
								      &pool_id, &cmm);
				srch_mode = CFA_SRCH_MODE_NEXT;

				if (rc == 0 && cmm) {
					netdev_dbg(bp->dev, "%s: free %s CMM(%p) for pool(%d)\n",
						   __func__, tfc_ts_region_2_str(region, dir),
						   cmm, pool_id);
					cfa_mm_close(cmm);
					vfree(cmm);
				}
			} while (!rc);

			if (cpm)
				tfc_cpm_close(cpm);
			else
				netdev_dbg(bp->dev, "%s: no cpm %s\n", __func__,
					   tfc_ts_region_2_str(region, dir));

			rc = tfo_ts_set_cpm_inst(tfcp->tfo, tsid, bp->pf.fw_fid, dir, region, NULL);
			if (rc)
				netdev_dbg(bp->dev, "%s: cpm inst error(%d)\n", __func__, rc);

			cpm = NULL;
		}
	}

	return rc;
}

/* Unlocked core: caller must hold tfc_global_mutex. */
static int __tfc_tbl_scope_pool_alloc(struct tfc *tfcp, u16 fid, u8 tsid,
				      enum cfa_region_type region, enum cfa_dir dir,
				      u8 *pool_sz_exp, u16 *pool_id)
{
	struct bnxt *bp = tfcp->bp;
	void *tim, *tpm;
	int rc = 0;

	if (tfc_bp_is_pf(tfcp)) {
		bool is_pf_local = false;

		/*
		 * The PF's own local scope must never be touched via the VF
		 * channel. When called for a VF's own scope creation,
		 * is_bs_owner is already false at this point (set by the
		 * mem_alloc caller before this pool is registered), so this
		 * only rejects VF requests targeting the PF's own scope.
		 */
		rc = tbl_scope_is_pf_local(tfcp, tsid, &is_pf_local);
		if (rc) {
			netdev_err(bp->dev, "%s: tsid(%d) pf-local lookup failed rc(%d)\n",
				   __func__, tsid, rc);
			return rc;
		}
		if (is_pf_local && fid != bp->pf.fw_fid) {
			netdev_err(bp->dev, "%s: fid(%d) not authorized for PF-local tsid(%d)\n",
				   __func__, fid, tsid);
			return -EPERM;
		}

		rc = tfo_tim_get(tfcp->tfo, &tim, tsid);
		if (rc) {
			netdev_dbg(bp->dev, "%s: Failed to get TIM\n", __func__);
			return -EINVAL;
		}

		rc = tbl_scope_fid_is_authorized(tfcp, fid, tsid, tim);
		if (rc) {
			netdev_err(bp->dev, "%s: fid(%d) not authorized for tsid(%d) rc(%d)\n",
				   __func__, fid, tsid, rc);
			return rc;
		}

		rc = cfa_tim_tpm_inst_get(tim, tsid, region, dir, &tpm);
		if (rc) {
			netdev_dbg(bp->dev, "%s: Failed to get TPM for tsid:%d region:%d dir:%d\n",
				   __func__, tsid, region, dir);
			return -EINVAL;
		}

		rc = cfa_tpm_alloc(tpm, pool_id);
		if (rc) {
			netdev_dbg(bp->dev, "%s: Failed allocate pool_id %d\n", __func__, rc);
			return -EINVAL;
		}

		if (pool_sz_exp) {
			rc = cfa_tpm_pool_size_get(tpm, pool_sz_exp);
			if (rc) {
				netdev_dbg(bp->dev, "%s: Failed get pool size exp\n", __func__);
				return -EINVAL;
			}
		}
		rc = cfa_tpm_fid_add(tpm, *pool_id, fid);
		if (rc) {
			netdev_dbg(bp->dev, "%s: Failed to set pool_id %d fid 0x%x %d\n",
				   __func__, *pool_id, fid, rc);
			return rc;
		}

		/* On-demand back this pool's records (no-op unless dynamic backing
		 * is enabled for the region).  Undo the TPM allocation on
		 * failure so the pool can be retried.
		 */
		rc = ts_pool_back(tfcp, tsid, dir, region, *pool_id);
		if (rc) {
			(void)cfa_tpm_fid_rem(tpm, *pool_id, fid);
			(void)cfa_tpm_free(tpm, *pool_id);
			return rc;
		}
	} else { /* !PF */
		struct tfc_vf2pf_tbl_scope_pool_alloc_cmd req = { { 0 } };
		struct tfc_vf2pf_tbl_scope_pool_alloc_resp resp = { { 0 } };
		uint16_t fid = tfc_get_fid(tfcp);

		req.hdr.type = TFC_VF2PF_TYPE_TBL_SCOPE_POOL_ALLOC_CMD;
		req.hdr.fid = fid;
		req.tsid = tsid;
		req.dir = dir;
		req.region = region;

		/* Send message to PF to allocate pool */
		rc = tfc_vf2pf_pool_alloc(tfcp, &req, &resp);
		if (rc) {
			netdev_dbg(bp->dev, "%s: tfc_vf2pf_pool_alloc failed\n", __func__);
			return rc;
		}
		*pool_id = resp.pool_id;
		if (pool_sz_exp)
			*pool_sz_exp = resp.pool_sz_exp;
	}
	netdev_dbg(bp->dev, "%s: allocate pool_id(%d) region(%s) fid(%d)\n",
		   __func__, *pool_id, tfc_ts_region_2_str(region, dir), fid);

	return rc;
}

int tfc_tbl_scope_pool_alloc(struct tfc *tfcp, u16 fid, u8 tsid, enum cfa_region_type region,
			     enum cfa_dir dir, u8 *pool_sz_exp, u16 *pool_id)
{
	struct bnxt *bp = tfcp->bp;
	int rc;

	if (!pool_id) {
		netdev_dbg(bp->dev, "%s: Invalid pool_id pointer\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&tfc_global_mutex);

	if (tfo_ts_validate(tfcp->tfo, tsid, NULL) != 0) {
		netdev_dbg(bp->dev, "%s: tsid(%d) invalid\n", __func__, tsid);
		rc = -EINVAL;
		goto unlock;
	}

	rc = __tfc_tbl_scope_pool_alloc(tfcp, fid, tsid, region, dir, pool_sz_exp, pool_id);

unlock:
	mutex_unlock(&tfc_global_mutex);

	return rc;
}

int tfc_tbl_scope_pool_free(struct tfc *tfcp, u16 fid, u8 tsid,
			    enum cfa_region_type region, enum cfa_dir dir,
			    u16 pool_id)
{
	struct bnxt *bp = tfcp->bp;
	void *tim, *tpm;
	int rc = 0;

	mutex_lock(&tfc_global_mutex);

	if (tfo_ts_validate(tfcp->tfo, tsid, NULL) != 0) {
		netdev_dbg(bp->dev, "%s: tsid(%d) invalid\n", __func__, tsid);
		rc = -EINVAL;
		goto unlock;
	}

	if (tfc_bp_is_pf(tfcp)) {
		bool is_pf_local = false;

		/* The PF's own local scope must never be touched via the VF channel. */
		rc = tbl_scope_is_pf_local(tfcp, tsid, &is_pf_local);
		if (rc) {
			netdev_err(bp->dev, "%s: tsid(%d) pf-local lookup failed rc(%d)\n",
				   __func__, tsid, rc);
			goto unlock;
		}
		if (is_pf_local && fid != bp->pf.fw_fid) {
			netdev_err(bp->dev, "%s: fid(%d) not authorized for PF-local tsid(%d)\n",
				   __func__, fid, tsid);
			rc = -EPERM;
			goto unlock;
		}

		rc = tfo_tim_get(tfcp->tfo, &tim, tsid);
		if (rc) {
			rc = -EINVAL;
			goto unlock;
		}

		rc = cfa_tim_tpm_inst_get(tim, tsid, region, dir, &tpm);
		if (rc) {
			rc = -EINVAL;
			goto unlock;
		}

		rc = cfa_tpm_fid_rem(tpm, pool_id, fid);
		if (rc) {
			rc = -EINVAL;
			goto unlock;
		}

		rc = cfa_tpm_free(tpm, pool_id);
	} else {
		/* Pools are currently only deleted on the VF when the
		 * VF calls tfc_tbl_scope_mem_free() if shared.
		 */
	}

unlock:
	mutex_unlock(&tfc_global_mutex);
	return rc;
}

int tfc_tbl_scope_config_state_get(struct tfc *tfcp, u8 tsid, bool *configured)
{
	struct bnxt *bp = tfcp->bp;
	int rc;

	if (tfo_ts_validate(tfcp->tfo, tsid, NULL) != 0) {
		netdev_dbg(bp->dev, "%s: tsid(%d) invalid\n", __func__, tsid);
		return -EINVAL;
	}

	rc = tfc_msg_tbl_scope_config_get(tfcp, tsid, configured);
	if (rc) {
		netdev_dbg(bp->dev, "%s: message failed %d\n", __func__, rc);
		return rc;
	}

	return rc;
}

static void tbl_scope_cleanup(struct tfc *tfcp, u8 tsid)
{
	struct bnxt *bp = tfcp->bp;
	int rc;

	bnxt_debug_tf_delete(bp);

	/* free memory and cleanup scope */
	netdev_dbg(bp->dev, "freeing tsid(%d) memory\n", tsid);
	rc = tbl_scope_host_mem_free(tfcp, tsid);
	if (rc)
		netdev_dbg(bp->dev,
			   "%s: Failed free tsid(%d) backing store\n",
			   __func__, tsid);
	rc = tbl_scope_pools_destroy(tfcp, tsid);
	if (rc)
		netdev_dbg(bp->dev,
			   "%s: Failed to destroy tsid(%d) pools\n",
			   __func__, tsid);
	rc = tfo_ts_set(tfcp->tfo, tsid, CFA_SCOPE_TYPE_INVALID, CFA_APP_TYPE_INVALID,
			false, 0);
	if (rc)
		netdev_dbg(bp->dev,
			   "%s: Failed to reset tsid(%d) state\n",
			   __func__, tsid);
}

void tfc_tbl_scope_cleanup(struct tfc *tfcp)
{
	bool ts_valid = false;
	struct tfc_ts_mem_cfg mem_cfg;
	struct bnxt *bp;
	bool local;
	u16 tsid;
	int rc;

	if (!tfcp || !tfcp->tfo)
		return;

	bp = tfcp->bp;
	if (!BNXT_PF(bp))
		return;

	for (tsid = 0; tsid < TFC_TBL_SCOPE_MAX; tsid++) {
		rc = tfo_ts_validate(tfcp->tfo, tsid, &ts_valid);
		if (rc)
			continue;
		if (ts_valid) {
			/* Lookup any memory config to get local */
			rc = tfo_ts_get_mem_cfg(tfcp->tfo, tsid, CFA_DIR_RX, CFA_REGION_TYPE_LKUP,
						&local, &mem_cfg);
			if (rc)
				continue;

			/* Only cleanup non-local table scopes */
			if (!local)
				tbl_scope_cleanup(tfcp, tsid);
		}
	}
}

/**
 * tfc_tbl_scope_func_reset - Reset table-scope state for a given FID (e.g. on VF FLR).
 * Returns 0 if tfcp/tfo/bp are NULL (TruFlow not available, e.g. during teardown).
 */
int tfc_tbl_scope_func_reset(struct tfc *tfcp, u16 fid)
{
	enum cfa_scope_type scope_type;
	void *tim = NULL, *tpm = NULL;
	enum cfa_region_type region;
	u16 pool_id, found_cnt = 0;
	enum cfa_app_type app;
	dma_addr_t data_pa;
	enum cfa_dir dir;
	struct bnxt *bp;
	struct bnxt_tfc_mpc_info *mpc_info;
	void *data_va;
	bool valid;
	u8 tsid;
	int rc = 0;

	if (!tfcp || !tfcp->tfo || !tfcp->bp) {
		if (tfcp && tfcp->bp)
			netdev_dbg(((struct bnxt *)tfcp->bp)->dev,
				   "%s: TruFlow not available, skip\n", __func__);
		return 0;
	}

	bp = tfcp->bp;
	mpc_info = bp->tfc_info;

	if (!tfc_bp_is_pf(tfcp)) {
		netdev_dbg(bp->dev, "%s: only valid for PF\n", __func__);
		return -EINVAL;
	}

	if (WARN_ON_ONCE(!mpc_info))
		return -EINVAL;

	data_va = dma_zalloc_coherent(&bp->pdev->dev, TFC_MPC_MAX_TABLE_READ_BYTES,
				      &data_pa, GFP_KERNEL);

	if (!data_va) {
		netdev_dbg(bp->dev, "%s: Failed to allocate data buffer\n", __func__);
		return -ENOMEM;
	}

	atomic_inc(&mpc_info->flr_in_progress);
	smp_mb__after_atomic(); /* ensure flag visible before MPC commands are sent */

	for (tsid = 1; tsid < TFC_TBL_SCOPE_MAX; tsid++) {
		rc = tfo_ts_get(tfcp->tfo, tsid, &scope_type, &app, &valid, NULL);
		if (rc)
			continue; /* TS is not used, move on to the next */

		rc = tfo_tim_get(tfcp->tfo, &tim, tsid);
		if (rc) {
			netdev_dbg(bp->dev, "%s: Failed to get TIM\n", __func__);
			continue;
		}

		if (!valid)
			continue; /* TS invalid or not shared, move on */

		for (dir = 0; dir < CFA_DIR_MAX; dir++) {
			for (region = 0; region < CFA_REGION_TYPE_MAX; region++) {
				/* Get the TPM and then check to see if the fid is associated
				 * with any of the pools
				 */
				rc = cfa_tim_tpm_inst_get(tim, tsid, region, dir, &tpm);
				if (rc) {
					netdev_dbg(bp->dev,
						   "%s: Failed to get TPM for tsid:%d dir:%d\n",
						   __func__, tsid, dir);
					goto cleanup;
				}

				rc = cfa_tpm_srchm_by_fid(tpm, CFA_SRCH_MODE_FIRST, fid, &pool_id);
				if (rc) /* FID not used */
					continue;

				do {
					found_cnt++;

					/* Remove EM entries associated with this pool */
					if ((region == CFA_REGION_TYPE_LKUP) &&
					    (scope_type != CFA_SCOPE_TYPE_NON_SHARED))
						rc = tfc_em_delete_entries_by_pool_id(tfcp,
										      tsid,
										      dir,
										      pool_id,
										      0,
										      data_va,
										      data_pa);

					/* Ignore failure as scope may already be removed */
					if (region == CFA_REGION_TYPE_LKUP && rc)
						netdev_dbg(bp->dev,
							   "%s: ignore failure tsid:%d dir:%d pool:%d\n",
							   __func__, tsid, dir, pool_id);

					/* Remove fid from pool */
					rc = cfa_tpm_fid_rem(tpm, pool_id, fid);
					if (rc)
						netdev_dbg(bp->dev,
							   "%s: cfa_tpm_fid_rem() failed for fid:%d pool:%d\n",
							   __func__, fid, pool_id);

					rc = cfa_tpm_free(tpm, pool_id);

					/* Next! */
					rc = cfa_tpm_srchm_by_fid(tpm,
								  CFA_SRCH_MODE_NEXT,
								  fid,
								  &pool_id);
				} while (!rc);
			}
		}
		/* If we cleaned up some pools in the scope (found_cnt) determine
		 * whether there are any fids left in any of the regions.
		 */
		if (found_cnt) {
			bool tsid_in_use = 0;
			u16 fid_cnt;

			for (dir = 0; dir < CFA_DIR_MAX; dir++) {
				for (region = 0; region < CFA_REGION_TYPE_MAX; region++) {
					/* Get the TPM and then check to see if the fid is
					 * associated with any of the pools
					 */
					rc = cfa_tim_tpm_inst_get(tim, tsid, region, dir, &tpm);
					if (rc) {
						netdev_dbg(bp->dev,
							   "%s: Failed to get TPM for tsid:%d dir:%d\n",
							   __func__, tsid, dir);
						continue;
					}
					rc = cfa_tpm_fid_cnt_get(tpm, &fid_cnt);
					if (fid_cnt)
						tsid_in_use = 1;
				}
			}
			if (!tsid_in_use)
				tbl_scope_cleanup(tfcp, tsid);
		}
	}
cleanup:
	atomic_dec(&mpc_info->flr_in_progress);
	dma_free_coherent(&bp->pdev->dev, TFC_MPC_MAX_TABLE_READ_BYTES,
			  data_va, data_pa);
	return rc;
}
