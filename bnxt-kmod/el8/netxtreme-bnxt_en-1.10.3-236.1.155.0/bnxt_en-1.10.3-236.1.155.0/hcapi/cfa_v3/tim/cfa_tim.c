// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#include <linux/errno.h>
#include <linux/stddef.h>
#include <linux/string.h>
#include "bnxt_compat.h"
#include "cfa_util.h"
#include "cfa_types.h"
#include "cfa_tim.h"

static u32 cfa_tim_size(u8 max_tbl_scopes, u8 max_regions)
{
	return (sizeof(struct cfa_tim) +
		(max_tbl_scopes * max_regions * CFA_DIR_MAX) * sizeof(void *));
}

int cfa_tim_query(u8 max_tbl_scopes, u8 max_regions,
		  u32 *tim_db_size)
{
	if (!tim_db_size) {
		netdev_err(NULL, "tim_db_size = %p\n", tim_db_size);
		return -EINVAL;
	}

	*tim_db_size = cfa_tim_size(max_tbl_scopes, max_regions);

	return 0;
}

int cfa_tim_open(void *tim, u32 tim_db_size, u8 max_tbl_scopes,
		 u8 max_regions)
{
	struct cfa_tim *ctx = (struct cfa_tim *)tim;

	if (!tim) {
		netdev_err(NULL, "tim = %p\n", tim);
		return -EINVAL;
	}
	if (tim_db_size < cfa_tim_size(max_tbl_scopes, max_regions)) {
		netdev_err(NULL, "max_tbl_scopes = %d, max_regions = %d\n",
			   max_tbl_scopes, max_regions);
		return -EINVAL;
	}

	memset(tim, 0, tim_db_size);

	ctx->signature = CFA_TIM_SIGNATURE;
	ctx->max_tsid = max_tbl_scopes;
	ctx->max_regions = max_regions;
	ctx->tpm_tbl = (void **)(ctx + 1);

	return 0;
}

int cfa_tim_close(void *tim)
{
	struct cfa_tim *ctx = (struct cfa_tim *)tim;

	if (!tim || ctx->signature != CFA_TIM_SIGNATURE) {
		netdev_err(NULL, "tim = %p\n", tim);
		return -EINVAL;
	}

	memset(tim, 0, cfa_tim_size(ctx->max_tsid, ctx->max_regions));

	return 0;
}

int cfa_tim_tpm_inst_set(void *tim, u8 tsid, u8 region_id,
			 int dir, void *tpm_inst)
{
	struct cfa_tim *ctx = (struct cfa_tim *)tim;

	if (!tim || ctx->signature != CFA_TIM_SIGNATURE) {
		netdev_err(NULL, "tim = %p\n", tim);
		return -EINVAL;
	}

	if (!(CFA_CHECK_UPPER_BOUNDS(tsid, ctx->max_tsid - 1) &&
	      CFA_CHECK_UPPER_BOUNDS(region_id, ctx->max_regions - 1))) {
		netdev_err(NULL, "tsid = %d, region_id = %d\n", tsid, region_id);
		return -EINVAL;
	}

	ctx->tpm_tbl[CFA_TIM_MAKE_INDEX(tsid, region_id, dir,
					ctx->max_regions, ctx->max_tsid)] = tpm_inst;
	return 0;
}

int cfa_tim_tpm_inst_get(void *tim, u8 tsid, u8 region_id,
			 int dir, void **tpm_inst)
{
	struct cfa_tim *ctx = (struct cfa_tim *)tim;

	*tpm_inst = NULL;

	if (!tim || ctx->signature != CFA_TIM_SIGNATURE) {
		netdev_err(NULL, "tim = %p\n", tim);
		return -EINVAL;
	}

	if (!(CFA_CHECK_UPPER_BOUNDS(tsid, ctx->max_tsid - 1) &&
	      CFA_CHECK_UPPER_BOUNDS(region_id, ctx->max_regions - 1))) {
		netdev_err(NULL, "tsid = %d, region_id = %d\n", tsid, region_id);
		return -EINVAL;
	}

	*tpm_inst = ctx->tpm_tbl[CFA_TIM_MAKE_INDEX(tsid, region_id, dir, ctx->max_regions,
			ctx->max_tsid)];
	return 0;
}
