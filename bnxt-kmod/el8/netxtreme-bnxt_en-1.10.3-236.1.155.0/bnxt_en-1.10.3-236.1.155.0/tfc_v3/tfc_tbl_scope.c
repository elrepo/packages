// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2023 Broadcom
 * All rights reserved.
 */

#include <linux/types.h>
#include <linux/vmalloc.h>
#include <linux/pci.h>

#include "bnxt_compat.h"
#include "bnxt.h"
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
			netdev_dbg(bp->dev, "No mapping for page: %d table: %16p\n", i, tp);
			continue;
		}

		dma_free_coherent(&bp->pdev->dev, tp->pg_size,
				  tp->pg_va_tbl[i], tp->pg_pa_tbl[i]);
		tp->pg_va_tbl[i] = NULL;
	}

	tp->pg_count = 0;
	kfree(tp->pg_va_tbl);
	tp->pg_va_tbl = NULL;
	kfree(tp->pg_pa_tbl);
	tp->pg_pa_tbl = NULL;
}

static int alloc_pg_tbl(struct bnxt *bp, struct tfc_ts_page_tbl *tp, u32 pg_count,
			u32 pg_size)
{
	u32 i;

	tp->pg_va_tbl =
		kcalloc(pg_count, sizeof(void *), GFP_KERNEL);
	if (!tp->pg_va_tbl)
		return -ENOMEM;

	tp->pg_pa_tbl =
		kzalloc(pg_count * sizeof(void *), GFP_KERNEL);
	if (!tp->pg_pa_tbl) {
		kfree(tp->pg_va_tbl);
		return -ENOMEM;
	}

	tp->pg_count = 0;
	tp->pg_size = pg_size;

	for (i = 0; i < pg_count; i++) {
		tp->pg_va_tbl[i] = dma_alloc_coherent(&bp->pdev->dev, pg_size,
						      &tp->pg_pa_tbl[i], GFP_KERNEL);
		if (!tp->pg_va_tbl[i])
			goto cleanup;

		tp->pg_count++;
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

static int alloc_page_table(struct bnxt *bp, struct tfc_ts_mem_cfg *mem_cfg, u32 page_size)
{
	struct tfc_ts_page_tbl *tp;
	int i, rc;

	for (i = 0; i < mem_cfg->num_lvl; i++) {
		tp = &mem_cfg->pg_tbl[i];

		rc = alloc_pg_tbl(bp, tp, mem_cfg->page_cnt[i], page_size);
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
	u32 i, j, k = 0;

	for (i = 0; i < tp->pg_count; i++) {
		pg_va = tp->pg_va_tbl[i];

		for (j = 0; j < MAX_PAGE_PTRS(tp->pg_size); j++) {
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

static int alloc_link_pbl(struct bnxt *bp, struct tfc_ts_mem_cfg *mem_cfg, u32 page_size)
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

	netdev_dbg(bp->dev, "EEM: lvls: %d sz: %016llu pgs: %016llu l0: %u l1: %u l2: %u\n",
		   mem_cfg->num_lvl, mem_cfg->num_data_pages * page_size,
		   mem_cfg->num_data_pages, mem_cfg->page_cnt[TFC_TS_PT_LVL_0],
		   mem_cfg->page_cnt[TFC_TS_PT_LVL_1],
		   mem_cfg->page_cnt[TFC_TS_PT_LVL_2]);

	/* tf_em_alloc_page_table -> tf_em_alloc_pg_tbl */
	rc = alloc_page_table(bp, mem_cfg, page_size);
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

/**
 * Remove all associated pools owned by a function from TPM
 *
 * @tfcp: Pointer to TFC handle
 * @fid: function
 * @tsid: Table scope identifier
 * @pool_cnt: Pointer to the number of pools still associated with other fids.
 *
 * Returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
static int tbl_scope_tpm_fid_rem(struct tfc *tfcp, u16 fid, u8 tsid,
				 u16 *pool_cnt)
{
	enum cfa_scope_type scope_type;
	enum cfa_region_type region;
	struct bnxt *bp = tfcp->bp;
	u16 pool_id, lfid, max_fid;
	bool valid, is_pf;
	u16 found_cnt = 0;
	enum cfa_dir dir;
	void *tim, *tpm;
	int rc;

	if (!pool_cnt) {
		netdev_dbg(bp->dev, "%s: Invalid pool_cnt pointer\n", __func__);
		return -EINVAL;
	}
	rc = tfc_bp_is_pf(tfcp, &is_pf);
	if (rc)
		return rc;

	if (!is_pf) {
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
	rc = tfc_bp_vf_max(tfcp, &max_fid);
	if (rc)
		return rc;

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
				     &parms->max_lkup_static_bucket_exp);
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
	bool is_pf;
	u16 pfid;

	if (!parms) {
		netdev_dbg(bp->dev, "%s: Invalid parms pointer\n", __func__);
		return -EINVAL;
	}

	if (tfo_ts_validate(tfcp->tfo, tsid, &valid) != 0) {
		netdev_dbg(bp->dev, "%s: Invalid tsid(%d) object\n", __func__, tsid);
		return -EINVAL;
	}

	if (parms->local && !valid) {
		netdev_dbg(bp->dev, "%s: tsid(%d) not allocated\n", __func__, tsid);
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

	rc = tfc_get_pfid(tfcp, &pfid);
	if (rc)
		return rc;

	rc = tfc_bp_is_pf(tfcp, &is_pf);
	if (rc)
		return rc;

	/* If we are running on a PF, we will allocate memory locally */
	if (is_pf) {
		struct tbl_scope_pools_create_parms cparms;
		cfg_done = false;
		cfg_cnt = 0;

		for (dir = 0; dir < CFA_DIR_MAX; dir++) {
			for (region = 0; region < CFA_REGION_TYPE_MAX; region++) {

				if (cfg_cnt == 3)
					cfg_done = true;

				if (region == CFA_REGION_TYPE_LKUP) {
					mem_cfg.rec_cnt = parms->lkup_rec_cnt[dir];
					mem_cfg.lkup_rec_start_offset =
						1 << parms->static_bucket_cnt_exp[dir];
					mem_cfg.entry_size = RECORD_SIZE;
				}
				netdev_dbg(bp->dev, "Alloc %s table\n",
					   tfc_ts_region_2_str(region, dir));

				rc = alloc_link_pbl(bp, &mem_cfg,
						    parms->pbl_page_sz_in_bytes);
				if (rc)
					goto cleanup;

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
								  cfg_done);
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
			u16 max_vf;

			rc = tfc_bp_vf_max(tfcp, &max_vf);
			if (rc)
				return rc;
			if (fid > max_vf) {
				netdev_dbg(bp->dev, "%s fid out of range %d\n",
					   __func__, fid);
				return -EINVAL;
			}

			for (region = 0; region < CFA_REGION_TYPE_MAX; region++) {
				for (dir = 0; dir < CFA_DIR_MAX; dir++) {
					rc = tfc_tbl_scope_pool_alloc(tfcp,
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
		}
	} else /* this is a VF */ {
		/* If first or !shared, send message to PF to allocate the memory */
		if (parms->first || parms->scope_type == CFA_SCOPE_TYPE_NON_SHARED) {
			struct tfc_vf2pf_tbl_scope_mem_alloc_cfg_cmd req = { { 0 } };
			struct tfc_vf2pf_tbl_scope_mem_alloc_cfg_resp resp = { { 0 } };
			u16 fid;

			rc = tfc_get_fid(tfcp, &fid);
			if (rc)
				return rc;

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
	return rc;
}

static int tbl_scope_host_mem_free(struct tfc *tfcp, u8 tsid, bool is_pf)
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
			if (is_pf) {
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
	bool local, is_pf = false;
	int rc;

	if (tfo_ts_validate(tfcp->tfo, tsid, NULL) != 0) {
		netdev_dbg(bp->dev, "%s: tsid(%d) invalid\n", __func__, tsid);
		return -EINVAL;
	}

	rc = tfo_ts_get(tfcp->tfo, tsid, &scope_type, NULL, NULL, NULL);
	if (rc)
		return rc;

	rc = tfc_bp_is_pf(tfcp, &is_pf);
	if (rc)
		return rc;

	/* Lookup any memory config to get local */
	rc = tfo_ts_get_mem_cfg(tfcp->tfo, tsid, CFA_DIR_RX, CFA_REGION_TYPE_LKUP,
				&local, &mem_cfg);
	if (rc)
		return rc;

	if (!is_pf) {
		struct tfc_vf2pf_tbl_scope_mem_free_cmd req = {{ 0 }};
		struct tfc_vf2pf_tbl_scope_mem_free_resp resp = {{ 0 }};
		u16 fid;

		rc = tfc_get_fid(tfcp, &fid);
		if (rc)
			return rc;

		req.hdr.type = TFC_VF2PF_TYPE_TBL_SCOPE_MEM_FREE_CMD;
		req.hdr.fid = fid;
		req.tsid = tsid;

		rc = tfc_vf2pf_mem_free(tfcp, &req, &resp);
		if (rc) {
			netdev_dbg(bp->dev, "%s: tfc_vf2pf_mem_free failed\n", __func__);
			/* continue cleanup regardless */
		}
		netdev_dbg(bp->dev, "%s: tsid: %d, status %d\n", __func__, resp.tsid, resp.status);
	}
	if (scope_type != CFA_SCOPE_TYPE_NON_SHARED && is_pf) {
		u16 pool_cnt;
		u16 max_vf;

		rc = tfc_bp_vf_max(tfcp, &max_vf);
		if (rc)
			return rc;

		if (fid > max_vf) {
			netdev_dbg(bp->dev, "%s: invalid fid 0x%x\n", __func__, fid);
			return -EINVAL;
		}
		rc = tbl_scope_tpm_fid_rem(tfcp, fid, tsid, &pool_cnt);
		if (rc) {
			netdev_dbg(bp->dev, "%s: error getting tsid(%d) pools status %d\n",
				   __func__, tsid, rc);
			return rc;
		}
		/* Then if there are still fids present, return */
		if (pool_cnt) {
			netdev_dbg(bp->dev, "%s: tsid(%d) fids still present pool_cnt(%d)\n",
				   __func__, tsid, pool_cnt);
			return 0;
		}
	}

	tbl_scope_host_mem_free(tfcp, tsid, is_pf);

	if (is_pf) {
		rc = tbl_scope_pools_destroy(tfcp, tsid);
		if (rc) {
			netdev_dbg(bp->dev, "%s: tsid(%d)  pool err(%d) continuing\n",
				   __func__, tsid, rc);
		}
	}
	/* cleanup state */
	rc = tfo_ts_set(tfcp->tfo, tsid, CFA_SCOPE_TYPE_INVALID, CFA_APP_TYPE_INVALID, false, 0);

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

int tfc_tbl_scope_pool_alloc(struct tfc *tfcp, u16 fid, u8 tsid, enum cfa_region_type region,
			     enum cfa_dir dir, u8 *pool_sz_exp, u16 *pool_id)
{
	struct bnxt *bp = tfcp->bp;
	void *tim, *tpm;
	bool is_pf;
	int rc;

	if (!pool_id) {
		netdev_dbg(bp->dev, "%s: Invalid pool_id pointer\n", __func__);
		return -EINVAL;
	}

	if (tfo_ts_validate(tfcp->tfo, tsid, NULL) != 0) {
		netdev_dbg(bp->dev, "%s: tsid(%d) invalid\n", __func__, tsid);
		return -EINVAL;
	}

	rc = tfc_bp_is_pf(tfcp, &is_pf);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Failed to get PF status\n", __func__);
		return -EINVAL;
	}

	if (is_pf) {
		rc = tfo_tim_get(tfcp->tfo, &tim, tsid);
		if (rc) {
			netdev_dbg(bp->dev, "%s: Failed to get TIM\n", __func__);
			return -EINVAL;
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
	} else { /* !PF */
		struct tfc_vf2pf_tbl_scope_pool_alloc_cmd req = { { 0 } };
		struct tfc_vf2pf_tbl_scope_pool_alloc_resp resp = { { 0 } };
		uint16_t fid;

		rc = tfc_get_fid(tfcp, &fid);
		if (rc)
			return rc;

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

int tfc_tbl_scope_pool_free(struct tfc *tfcp, u16 fid, u8 tsid,
			    enum cfa_region_type region, enum cfa_dir dir,
			    u16 pool_id)
{
	struct bnxt *bp = tfcp->bp;
	void *tim, *tpm;
	bool is_pf;
	int rc;

	if (tfo_ts_validate(tfcp->tfo, tsid, NULL) != 0) {
		netdev_dbg(bp->dev, "%s: tsid(%d) invalid\n", __func__, tsid);
		return -EINVAL;
	}

	rc = tfc_bp_is_pf(tfcp, &is_pf);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Failed to get PF status\n", __func__);
		return -EINVAL;
	}

	if (is_pf) {
		rc = tfo_tim_get(tfcp->tfo, &tim, tsid);
		if (rc)
			return -EINVAL;

		rc = cfa_tim_tpm_inst_get(tim, tsid, region, dir, &tpm);
		if (rc)
			return -EINVAL;

		rc = cfa_tpm_fid_rem(tpm, pool_id, fid);
		if (rc)
			return -EINVAL;

		rc = cfa_tpm_free(tpm, pool_id);
		return rc;

	} else {
		/* Pools are currently only deleted on the VF when the
		 * VF calls tfc_tbl_scope_mem_free() if shared.
		 */
	}

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

static void tbl_scope_cleanup(struct tfc *tfcp, u8 tsid, bool is_pf)
{
	struct bnxt *bp = tfcp->bp;
	int rc;

	/* free memory and cleanup scope */
	netdev_dbg(bp->dev, "freeing tsid(%d) memory\n", tsid);
	rc = tbl_scope_host_mem_free(tfcp, tsid, is_pf);
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
	bnxt_debug_tf_delete(bp);
}

void tfc_tbl_scope_cleanup(struct tfc *tfcp)
{
	bool is_pf = true, ts_valid = false;
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
				tbl_scope_cleanup(tfcp, tsid, is_pf);
		}
	}
}

int tfc_tbl_scope_func_reset(struct tfc *tfcp, u16 fid)
{
	enum cfa_scope_type scope_type;
	void *tim = NULL, *tpm = NULL;
	enum cfa_region_type region;
	struct bnxt *bp = tfcp->bp;
	u16 pool_id, found_cnt = 0;
	enum cfa_app_type app;
	dma_addr_t data_pa;
	bool valid, is_pf;
	enum cfa_dir dir;
	void *data_va;
	u8 tsid;
	int rc;

	rc = tfc_bp_is_pf(tfcp, &is_pf);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Failed to get PF status\n", __func__);
		return -EINVAL;
	}
	if (!is_pf) {
		netdev_dbg(bp->dev, "%s: only valid for PF\n", __func__);
		return -EINVAL;
	}

	data_va = dma_zalloc_coherent(&bp->pdev->dev, TFC_MPC_MAX_TABLE_READ_BYTES,
				      &data_pa, GFP_KERNEL);

	if (!data_va) {
		netdev_dbg(bp->dev, "%s: Failed to allocate data buffer\n", __func__);
		return -ENOMEM;
	}

	for (tsid = 1; tsid < TFC_TBL_SCOPE_MAX; tsid++) {
		rc = tfo_ts_get(tfcp->tfo, tsid, &scope_type, &app, &valid, NULL);
		if (rc)
			continue; /* TS is not used, move on to the next */

		rc = tfo_tim_get(tfcp->tfo, &tim, tsid);
		if (rc) {
			netdev_dbg(bp->dev, "%s: Failed to get TIM\n", __func__);
			continue;
		}

		if (scope_type == CFA_SCOPE_TYPE_NON_SHARED || !valid)
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

					if (data_va)
						goto cleanup;
				}

				rc = cfa_tpm_srchm_by_fid(tpm, CFA_SRCH_MODE_FIRST, fid, &pool_id);
				if (rc) /* FID not used */
					continue;

				do {
					found_cnt++;

					/* Remove EM entries associated with this pool */
					if (region == CFA_REGION_TYPE_LKUP)
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
				tbl_scope_cleanup(tfcp, tsid, is_pf);
		}
	}
cleanup:
	dma_free_coherent(&bp->pdev->dev, TFC_MPC_MAX_TABLE_READ_BYTES,
			  data_va, data_pa);
	return rc;
}
