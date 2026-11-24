// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2014-2021 Broadcom
 * All rights reserved.
 */

#include <linux/types.h>
#include <linux/vmalloc.h>
#include "ulp_linux.h"
#include "bnxt_compat.h"
#include "bnxt_hsi.h"
#include "bnxt.h"
#include "bnxt_tfc.h"
#include "bnxt_tf_ulp.h"
#include "bnxt_tf_ulp_p7.h"
#include "bnxt_tf_common.h"
#include "ulp_fc_mgr.h"
#include "ulp_flow_db.h"
#include "ulp_template_db_enum.h"
#include "ulp_template_struct.h"
#include "tfc.h"
#include "tfc_debug.h"
#include "tfc_action_handle.h"

#ifdef CONFIG_BNXT_FLOWER_OFFLOAD
/* Need to create device parms for these values and handle
 * alignment dynamically.
 */
#define ULP_FC_TFC_PKT_CNT_OFFS 0
#define ULP_FC_TFC_BYTE_CNT_OFFS 1
#define ULP_TFC_CNTR_READ_BYTES 32
#define ULP_TFC_CNTR_ALIGN 32
#define ULP_TFC_ACT_WORD_SZ 32

static int
ulp_tf_fc_tfc_update_accum_stats(struct bnxt_ulp_context *ctxt,
				 struct bnxt_ulp_fc_info *fc_info,
				 struct bnxt_ulp_device_params *dparms)
{
	/* Accumulation is not supported, just return success */
	return 0;
}


static int
ulp_tf_fc_tfc_flow_stat_get(struct bnxt_ulp_context *ctxt,
			    struct ulp_flow_db_res_params *res,
			    u64 *packets, u64 *bytes)
{
	u16 data_size = ULP_TFC_CNTR_READ_BYTES;
	struct tfc_cmm_clr cmm_clr = { 0 };
	struct tfc_cmm_info cmm_info;
	dma_addr_t data_pa;
	struct tfc *tfcp;
	void *data_va;
	u16 word_size;
	u64 *data64;
	int rc = 0;

	tfcp = bnxt_ulp_cntxt_tfcp_get(ctxt, BNXT_ULP_SESSION_TYPE_DEFAULT);
	if (!tfcp) {
		netdev_dbg(ctxt->bp->dev, "Failed to get tf object\n");
		return -EINVAL;
	}

	/* Ensure that data is large enough to read words */
	word_size = (data_size + ULP_TFC_ACT_WORD_SZ - 1) / ULP_TFC_ACT_WORD_SZ;
	if (word_size * ULP_TFC_ACT_WORD_SZ > data_size) {
		netdev_dbg(ctxt->bp->dev, "Insufficient size %d for stat get\n",
			   data_size);
		return -EINVAL;
	}

	data_va = dma_zalloc_coherent(&ctxt->bp->pdev->dev, ULP_TFC_CNTR_READ_BYTES,
				      &data_pa, GFP_KERNEL);
	if (!data_va)
		return -ENOMEM;

	data64 = data_va;
	cmm_info.rsubtype = CFA_RSUBTYPE_CMM_ACT;
	cmm_info.act_handle = res->resource_hndl;
	cmm_info.dir = (enum cfa_dir)res->direction;
	/* Read and Clear the hw stat if requested */
	cmm_clr.clr = true;
	cmm_clr.offset_in_byte = 0;
	cmm_clr.sz_in_byte = sizeof(data64[ULP_FC_TFC_PKT_CNT_OFFS]) +
		sizeof(data64[ULP_FC_TFC_BYTE_CNT_OFFS]);
	rc = tfc_act_get(tfcp, NULL, &cmm_info, &cmm_clr, data_pa, &word_size);
	if (rc) {
		netdev_dbg(ctxt->bp->dev,
			   "Failed to read stat memory hndl=%llu\n",
			   res->resource_hndl);
		goto cleanup;
	}
	if (data64[ULP_FC_TFC_PKT_CNT_OFFS])
		*packets = data64[ULP_FC_TFC_PKT_CNT_OFFS];

	if (data64[ULP_FC_TFC_BYTE_CNT_OFFS])
		*bytes = data64[ULP_FC_TFC_BYTE_CNT_OFFS];

cleanup:
	dma_free_coherent(&ctxt->bp->pdev->dev, ULP_TFC_CNTR_READ_BYTES,
			  data_va, data_pa);
	return rc;
}

const struct bnxt_ulp_fc_core_ops ulp_fc_tfc_core_ops = {
	.ulp_flow_stat_get = ulp_tf_fc_tfc_flow_stat_get,
	.ulp_flow_stats_accum_update = ulp_tf_fc_tfc_update_accum_stats
};
#endif /* CONFIG_BNXT_FLOWER_OFFLOAD */
