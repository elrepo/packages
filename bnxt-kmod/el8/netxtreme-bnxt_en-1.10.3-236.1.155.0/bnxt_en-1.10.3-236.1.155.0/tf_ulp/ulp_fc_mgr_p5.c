// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2014-2023 Broadcom
 * All rights reserved.
 */

#include "bnxt_compat.h"
#include "bnxt_hsi.h"
#include "bnxt.h"
#include "bnxt_tfc.h"
#include "bnxt_tf_ulp.h"
#include "bnxt_tf_ulp_p5.h"
#include "bnxt_tf_common.h"
#include "ulp_fc_mgr.h"
#include "ulp_flow_db.h"
#include "ulp_template_db_enum.h"
#include "ulp_template_struct.h"
#include "tf_tbl.h"

#ifdef CONFIG_BNXT_FLOWER_OFFLOAD
int
ulp_tf_fc_tf_flow_stat_get(struct bnxt_ulp_context *ctxt,
			   struct ulp_flow_db_res_params *res,
			   u64 *packets, u64 *bytes)
{
	struct tf_get_tbl_entry_parms parms = { 0 };
	struct bnxt_ulp_device_params *dparms;
	struct bnxt *bp;
	u32 dev_id = 0;
	struct tf *tfp;
	u64 stats = 0;
	int rc = 0;

	tfp = bnxt_tf_ulp_cntxt_tfp_get(ctxt,
					ulp_flow_db_shared_session_get(res));
	if (!tfp)
		return -EINVAL;

	bp = tfp->bp;

	if (bnxt_ulp_cntxt_dev_id_get(ctxt, &dev_id)) {
		netdev_dbg(ctxt->bp->dev, "Failed to get device id\n");
		return -EINVAL;
	}

	dparms = bnxt_ulp_device_params_get(dev_id);
	if (!dparms) {
		netdev_dbg(bp->dev, "Failed to device parms\n");
		return -EINVAL;
	}
	parms.dir = res->direction;
	parms.type = TF_TBL_TYPE_ACT_STATS_64;
	parms.idx = res->resource_hndl;
	parms.data_sz_in_bytes = sizeof(u64);
	parms.data = (u8 *)&stats;
	rc = tf_get_tbl_entry(tfp, &parms);
	if (rc) {
		netdev_dbg(bp->dev,
			   "Get failed for id:0x%x rc:%d\n",
			   parms.idx, rc);
		return rc;
	}

	*packets = FLOW_CNTR_PKTS(stats, dparms);
	*bytes = FLOW_CNTR_BYTES(stats, dparms);

	return rc;
}

int
ulp_get_single_flow_stat(struct bnxt_ulp_context *ctxt,
			 struct tf *tfp,
			 struct bnxt_ulp_fc_info *fc_info,
			 enum tf_dir dir,
			 u32 hw_cntr_id,
			 struct bnxt_ulp_device_params *dparms)
{
	struct sw_acc_counter *sw_acc_tbl_entry = NULL;
	struct tf_get_tbl_entry_parms parms = { 0 };
	struct bnxt *bp = tfp->bp;
	u32 sw_cntr_indx = 0;
	u64 stats = 0;
	u64 delta_pkts = 0;
	u64 delta_bytes = 0;
	u64 cur_pkts = 0;
	u64 cur_bytes = 0;
	int rc = 0;

	parms.dir = dir;
	parms.type = TF_TBL_TYPE_ACT_STATS_64;
	parms.idx = hw_cntr_id;
	/* TODO:
	 * Size of an entry needs to obtained from template
	 */
	parms.data_sz_in_bytes = sizeof(u64);
	parms.data = (u8 *)&stats;
	rc = tf_get_tbl_entry(tfp, &parms);
	if (rc) {
		netdev_dbg(bp->dev,
			   "Get failed for id:0x%x rc:%d\n",
			   parms.idx, rc);
		return rc;
	}

	/* TBD - Get PKT/BYTE COUNT SHIFT/MASK from Template */
	sw_cntr_indx = hw_cntr_id - fc_info->shadow_hw_tbl[dir].start_idx;
	sw_acc_tbl_entry = &fc_info->sw_acc_tbl[dir][sw_cntr_indx];
	/* Some applications may accumulate the flow counters while some
	 * may not. In cases where the application is accumulating the counters
	 * the PMD need not do the accumulation itself and viceversa to report
	 * the correct flow counters.
	 */

	cur_pkts = FLOW_CNTR_PKTS(stats, dparms);
	cur_bytes = FLOW_CNTR_BYTES(stats, dparms);

	delta_pkts = ((cur_pkts - sw_acc_tbl_entry->pkt_count_last_polled) &
		      FLOW_CNTR_PKTS_MAX(dparms));
	delta_bytes = ((cur_bytes - sw_acc_tbl_entry->byte_count_last_polled) &
		       FLOW_CNTR_BYTES_MAX(dparms));

	sw_acc_tbl_entry->pkt_count += delta_pkts;
	sw_acc_tbl_entry->byte_count += delta_bytes;

	netdev_dbg(bp->dev,
		   " STATS_64 dir %d for id:0x%x cc:%llu tot:%llu lp:%llu dp:0x%llx\n",
		   dir, parms.idx,
		   cur_pkts,
		   sw_acc_tbl_entry->pkt_count,
		   sw_acc_tbl_entry->pkt_count_last_polled,
		   delta_pkts);

	/* Update the last polled */
	sw_acc_tbl_entry->pkt_count_last_polled = cur_pkts;
	sw_acc_tbl_entry->byte_count_last_polled = cur_bytes;

	return rc;
}

const struct bnxt_ulp_fc_core_ops ulp_fc_tf_core_ops = {
		.ulp_flow_stat_get = ulp_tf_fc_tf_flow_stat_get,
		.ulp_flow_stats_accum_update = NULL,
};
#endif /* CONFIG_BNXT_FLOWER_OFFLOAD */
