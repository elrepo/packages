// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2023 Broadcom
 * All rights reserved.
 */

#include <linux/errno.h>
#include <linux/stddef.h>
#include <linux/string.h>
#include "bnxt_compat.h"
#include "cfa_util.h"
#include "cfa_tpm.h"
#include "bitalloc.h"

static u32 cfa_tpm_size(u16 max_pools)
{
	return (sizeof(struct cfa_tpm) + BITALLOC_SIZEOF(max_pools) +
		max_pools * sizeof(u16));
}

int cfa_tpm_query(u16 max_pools, u32 *tpm_db_size)
{
	if (!tpm_db_size) {
		netdev_err(NULL, "tpm_db_size = %p\n", tpm_db_size);
		return -EINVAL;
	}

	if (!CFA_CHECK_BOUNDS(max_pools, CFA_TPM_MIN_POOLS,
			      CFA_TPM_MAX_POOLS)) {
		netdev_err(NULL, "max_pools = %d\n", max_pools);
		return -EINVAL;
	}

	*tpm_db_size = cfa_tpm_size(max_pools);

	return 0;
}

int cfa_tpm_open(void *tpm, u32 tpm_db_size, u16 max_pools)
{
	int i;
	struct cfa_tpm *ctx = (struct cfa_tpm *)tpm;

	if (!tpm) {
		netdev_err(NULL, "tpm = %p\n", tpm);
		return -EINVAL;
	}

	if (!(CFA_CHECK_BOUNDS(max_pools, CFA_TPM_MIN_POOLS,
			       CFA_TPM_MAX_POOLS) &&
	      tpm_db_size >= cfa_tpm_size(max_pools))) {
		netdev_err(NULL, "max_pools = %d tpm_db_size = %d\n", max_pools, tpm_db_size);
		return -EINVAL;
	}

	memset(tpm, 0, tpm_db_size);

	ctx->signature = CFA_TPM_SIGNATURE;
	ctx->max_pools = max_pools;
	ctx->pool_ba = (struct bitalloc *)(ctx + 1);
	ctx->fid_tbl = (u16 *)((u8 *)ctx->pool_ba +
				    BITALLOC_SIZEOF(max_pools));

	if (bnxt_ba_init(ctx->pool_ba, max_pools, true))
		return -EINVAL;

	for (i = 0; i < max_pools; i++)
		ctx->fid_tbl[i] = CFA_INVALID_FID;

	return 0;
}

void cfa_tpm_close(void *tpm)
{
	struct cfa_tpm *ctx = (struct cfa_tpm *)tpm;

	/* Free the bitmap allocated by bnxt_ba_init in cfa_tpm_open */
	if (tpm) {
		if (ctx->signature != CFA_TPM_SIGNATURE)
			netdev_dbg(NULL, "Invalid TPM signature\n");
		if (ctx->pool_ba)
			bnxt_ba_deinit(ctx->pool_ba);
		memset(tpm, 0, cfa_tpm_size(ctx->max_pools));
	}
}

int cfa_tpm_alloc(void *tpm, u16 *pool_id)
{
	int rc;
	struct cfa_tpm *ctx = (struct cfa_tpm *)tpm;

	if (!tpm || !pool_id ||
	    ctx->signature != CFA_TPM_SIGNATURE) {
		netdev_err(NULL, "tpm = %p, pool_id = %p\n", tpm, pool_id);
		return -EINVAL;
	}

	rc = bnxt_ba_alloc(ctx->pool_ba);

	if (rc < 0)
		return -ENOMEM;

	*pool_id = rc;

	ctx->fid_tbl[rc] = CFA_INVALID_FID;

	return 0;
}

int cfa_tpm_free(void *tpm, u16 pool_id)
{
	struct cfa_tpm *ctx = (struct cfa_tpm *)tpm;

	if (!tpm || ctx->signature != CFA_TPM_SIGNATURE) {
		netdev_err(NULL, "tpm = %p, pool_id = %d\n", tpm, pool_id);
		return -EINVAL;
	}

	if (ctx->fid_tbl[pool_id] != CFA_INVALID_FID) {
		netdev_err(NULL, "A function (%d) is still using the pool (%d)\n",
			   ctx->fid_tbl[pool_id], pool_id);
		return -EINVAL;
	}

	return bnxt_ba_free(ctx->pool_ba, pool_id);
}

int cfa_tpm_fid_add(void *tpm, u16 pool_id, u16 fid)
{
	struct cfa_tpm *ctx = (struct cfa_tpm *)tpm;

	if (!tpm || ctx->signature != CFA_TPM_SIGNATURE) {
		netdev_err(NULL, "tpm = %p, pool_id = %d\n", tpm, pool_id);
		return -EINVAL;
	}

	if (!bnxt_ba_inuse(ctx->pool_ba, pool_id)) {
		netdev_err(NULL, "Pool id (%d) was not allocated\n", pool_id);
		return -EINVAL;
	}

	if (ctx->fid_tbl[pool_id] != CFA_INVALID_FID &&
	    ctx->fid_tbl[pool_id] != fid) {
		netdev_err(NULL, "A function id %d was already set to the pool %d\n",
			   fid, ctx->fid_tbl[pool_id]);
		return -EINVAL;
	}

	ctx->fid_tbl[pool_id] = fid;

	return 0;
}

int cfa_tpm_fid_rem(void *tpm, u16 pool_id, u16 fid)
{
	struct cfa_tpm *ctx = (struct cfa_tpm *)tpm;

	if (!tpm || ctx->signature != CFA_TPM_SIGNATURE) {
		netdev_err(NULL, "tpm = %p, pool_id = %d\n", tpm, pool_id);
		return -EINVAL;
	}

	if (!bnxt_ba_inuse(ctx->pool_ba, pool_id)) {
		netdev_err(NULL, "Pool id (%d) was not allocated\n", pool_id);
		return -EINVAL;
	}

	if (ctx->fid_tbl[pool_id] == CFA_INVALID_FID ||
	    ctx->fid_tbl[pool_id] != fid) {
		netdev_err(NULL, "The function id %d was not set to the pool %d\n", fid, pool_id);
		return -EINVAL;
	}

	ctx->fid_tbl[pool_id] = CFA_INVALID_FID;

	return 0;
}

int cfa_tpm_srch_by_pool(void *tpm, u16 pool_id, u16 *fid)
{
	struct cfa_tpm *ctx = (struct cfa_tpm *)tpm;

	if (!tpm || ctx->signature != CFA_TPM_SIGNATURE || !fid ||
	    pool_id >= ctx->max_pools) {
		netdev_err(NULL, "tpm = %p, pool_id = %d, fid = %p\n", tpm, pool_id, fid);
		return -EINVAL;
	}

	if (!bnxt_ba_inuse(ctx->pool_ba, pool_id)) {
		netdev_err(NULL, "Pool id (%d) was not allocated\n", pool_id);
		return -EINVAL;
	}

	if (ctx->fid_tbl[pool_id] == CFA_INVALID_FID) {
		netdev_err(NULL, "A function id was not set to the pool (%d)\n", pool_id);
		return -EINVAL;
	}

	*fid = ctx->fid_tbl[pool_id];

	return 0;
}

int cfa_tpm_srchm_by_fid(void *tpm, enum cfa_srch_mode srch_mode, u16 fid,
			 u16 *pool_id)
{
	struct cfa_tpm *ctx = (struct cfa_tpm *)tpm;
	u16 i;

	if (!tpm || ctx->signature != CFA_TPM_SIGNATURE || !pool_id) {
		netdev_err(NULL, "tpm = %p, pool_id = %p fid = %d\n", tpm, pool_id, fid);
		return -EINVAL;
	}

	if (srch_mode == CFA_SRCH_MODE_FIRST)
		ctx->next_index = 0;

	for (i = ctx->next_index; i < ctx->max_pools; i++) {
		if (ctx->fid_tbl[i] == fid) {
			ctx->next_index = i + 1;
			*pool_id = i;
			return 0;
		}
	}

	ctx->next_index = ctx->max_pools;

	return -ENOENT;
}

int cfa_tpm_fid_cnt_get(void *tpm, u16 *fid_cnt)
{
	struct cfa_tpm *ctx = (struct cfa_tpm *)tpm;
	u16 my_fid_cnt;
	u16 i;

	if (!tpm || ctx->signature != CFA_TPM_SIGNATURE || !fid_cnt) {
		netdev_err(NULL, "tpm = %p, fid_cnt = %p\n", tpm, fid_cnt);
		return -EINVAL;
	}

	my_fid_cnt = 0;
	for (i = 0; i < ctx->max_pools; i++) {
		if (ctx->fid_tbl[i] != CFA_INVALID_FID) {
			ctx->next_index = i + 1;
			netdev_err(NULL, "fid(%d) in use\n", ctx->fid_tbl[i]);
			my_fid_cnt++;
		}
	}
	*fid_cnt = my_fid_cnt;
	return 0;
}

int cfa_tpm_pool_size_set(void *tpm, u8 pool_sz_exp)
{
	struct cfa_tpm *ctx = (struct cfa_tpm *)tpm;

	if (!tpm || ctx->signature != CFA_TPM_SIGNATURE) {
		netdev_err(NULL, "tpm = %p\n", tpm);
		return -EINVAL;
	}

	ctx->pool_sz_exp = pool_sz_exp;

	return 0;
}

int cfa_tpm_pool_size_get(void *tpm, u8 *pool_sz_exp)
{
	struct cfa_tpm *ctx = (struct cfa_tpm *)tpm;

	if (!tpm || ctx->signature != CFA_TPM_SIGNATURE || !pool_sz_exp) {
		netdev_err(NULL, "tpm = %p, pool_sz_exp = %p\n", tpm, pool_sz_exp);
		return -EINVAL;
	}

	*pool_sz_exp = ctx->pool_sz_exp;

	return 0;
}
