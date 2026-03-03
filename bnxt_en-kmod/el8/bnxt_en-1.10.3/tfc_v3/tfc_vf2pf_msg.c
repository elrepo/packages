// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2023 Broadcom
 * All rights reserved.
 */

#include <linux/types.h>
#include "bnxt_compat.h"
#include "bnxt.h"
#include "bnxt_hwrm.h"
#include "bnxt_sriov.h"
#include "bnxt_debugfs.h"
#include "tfc_vf2pf_msg.h"
#include "tfc_util.h"

/* Logging defines */
#define TFC_VF2PF_MSG_DEBUG  0

int
tfc_vf2pf_mem_alloc(struct tfc *tfcp,
		    struct tfc_vf2pf_tbl_scope_mem_alloc_cfg_cmd *req,
		    struct tfc_vf2pf_tbl_scope_mem_alloc_cfg_resp *resp)
{
	struct bnxt *bp = tfcp->bp;
	int rc;

	if (!req) {
		netdev_dbg(bp->dev, "%s: Invalid req pointer\n", __func__);
		return -EINVAL;
	}

	if (!resp) {
		netdev_dbg(bp->dev, "%s: Invalid resp pointer\n", __func__);
		return -EINVAL;
	}

	rc = bnxt_hwrm_tf_oem_cmd(bp, (u32 *)req, sizeof(*req), (u32 *)resp, sizeof(*resp));
	return rc;
}

int
tfc_vf2pf_mem_free(struct tfc *tfcp,
		   struct tfc_vf2pf_tbl_scope_mem_free_cmd *req,
		   struct tfc_vf2pf_tbl_scope_mem_free_resp *resp)
{
	struct bnxt *bp = tfcp->bp;
	int rc;

	if (!req) {
		netdev_dbg(bp->dev, "%s: Invalid req pointer\n", __func__);
		return -EINVAL;
	}

	if (!resp) {
		netdev_dbg(bp->dev, "%s: Invalid resp pointer\n", __func__);
		return -EINVAL;
	}

	rc = bnxt_hwrm_tf_oem_cmd(bp, (u32 *)req, sizeof(*req), (u32 *)resp, sizeof(*resp));
	return rc;
}

int
tfc_vf2pf_pool_alloc(struct tfc *tfcp,
		     struct tfc_vf2pf_tbl_scope_pool_alloc_cmd *req,
		     struct tfc_vf2pf_tbl_scope_pool_alloc_resp *resp)
{
	struct bnxt *bp = tfcp->bp;

	if (!req) {
		netdev_dbg(bp->dev, "%s: Invalid req pointer\n", __func__);
		return -EINVAL;
	}

	if (!resp) {
		netdev_dbg(bp->dev, "%s: Invalid resp pointer\n", __func__);
		return -EINVAL;
	}

	return  bnxt_hwrm_tf_oem_cmd(bp, (uint32_t *)req, sizeof(*req),
				  (uint32_t *)resp, sizeof(*resp));
}

int
tfc_vf2pf_pool_free(struct tfc *tfcp,
		    struct tfc_vf2pf_tbl_scope_pool_free_cmd *req,
		    struct tfc_vf2pf_tbl_scope_pool_free_resp *resp)
{
	struct bnxt *bp = tfcp->bp;

	if (!req) {
		netdev_dbg(bp->dev, "%s: Invalid req pointer\n", __func__);
		return -EINVAL;
	}
	if (!resp) {
		netdev_dbg(bp->dev, "%s: Invalid resp pointer\n", __func__);
		return -EINVAL;
	}

	return  bnxt_hwrm_tf_oem_cmd(bp, (uint32_t *)req, sizeof(*req),
				  (uint32_t *)resp, sizeof(*resp));
}

static int
tfc_vf2pf_mem_alloc_process(struct tfc *tfcp,
			    u32 *oem_data,
			    u32 *resp_data,
			    u16 *resp_len)
{
	struct tfc_vf2pf_tbl_scope_mem_alloc_cfg_resp *resp =
		(struct tfc_vf2pf_tbl_scope_mem_alloc_cfg_resp *)resp_data;
	struct tfc_vf2pf_tbl_scope_mem_alloc_cfg_cmd *req =
		(struct tfc_vf2pf_tbl_scope_mem_alloc_cfg_cmd *)oem_data;
	struct tfc_tbl_scope_mem_alloc_parms ma_parms;
	u16 data_len = sizeof(*resp);
	struct bnxt *bp = tfcp->bp;
	int dir, rc;

	if (*resp_len < data_len) {
		netdev_dbg(bp->dev, "%s: resp_data buffer is too small\n", __func__);
		return -EINVAL;
	}

	/* This block of code is for testing purpose. Will be removed later */
	netdev_dbg(bp->dev, "%s: Table scope mem alloc cfg cmd:\n", __func__);
	netdev_dbg(bp->dev, "\ttsid: 0x%x, max_pools: 0x%x\n", req->tsid, req->max_pools);
	for (dir = CFA_DIR_RX; dir < CFA_DIR_MAX; dir++) {
		netdev_dbg(bp->dev, "\tsbuckt_cnt_exp: 0x%x, dbucket_cnt: 0x%x\n",
			   req->static_bucket_cnt_exp[dir], req->dynamic_bucket_cnt[dir]);
		netdev_dbg(bp->dev, "\tlkup_rec_cnt: 0x%x, lkup_pool_sz_exp: 0x%x\n",
			   req->lkup_rec_cnt[dir], req->lkup_pool_sz_exp[dir]);
		netdev_dbg(bp->dev, "\tact_pool_sz_exp: 0x%x, lkup_rec_start_offset: 0x%x\n",
			   req->act_pool_sz_exp[dir], req->lkup_rec_start_offset[dir]);
	}

	memset(&ma_parms, 0, sizeof(struct tfc_tbl_scope_mem_alloc_parms));

	for (dir = CFA_DIR_RX; dir < CFA_DIR_MAX; dir++) {
		ma_parms.static_bucket_cnt_exp[dir] = req->static_bucket_cnt_exp[dir];
		ma_parms.dynamic_bucket_cnt[dir] = req->dynamic_bucket_cnt[dir];
		ma_parms.lkup_rec_cnt[dir] = req->lkup_rec_cnt[dir];
		ma_parms.act_rec_cnt[dir] = req->act_rec_cnt[dir];
		ma_parms.act_pool_sz_exp[dir] = req->act_pool_sz_exp[dir];
		ma_parms.lkup_pool_sz_exp[dir] = req->lkup_pool_sz_exp[dir];
		ma_parms.lkup_rec_start_offset[dir] = req->lkup_rec_start_offset[dir];
	}
	/* Obtain from driver page definition (4k for DPDK) */
	ma_parms.pbl_page_sz_in_bytes = BNXT_PAGE_SIZE;
	/* First is meaningless on the PF, set to 0 */
	ma_parms.first = 0;

	/* This is not for local use if we are getting a message from the VF */
	ma_parms.local = false;
	ma_parms.max_pools = req->max_pools;
	ma_parms.scope_type = req->scope_type;
	rc = tfc_tbl_scope_mem_alloc(tfcp, req->hdr.fid, req->tsid, &ma_parms);
	if (!rc) {
		netdev_dbg(bp->dev, "%s: tsid(%d) PF allocation succeeds\n", __func__, req->tsid);
	} else {
		netdev_dbg(bp->dev, "%s: tsid(%d) PF allocation fails (%d)\n", __func__, req->tsid,
			   rc);
	}

	rc = bnxt_debug_tf_create(bp, req->tsid);
	if (rc)
		netdev_dbg(bp->dev, "%s: port(%d) tsid(%d) Failed to create debugfs entry\n",
			   __func__, bp->pf.port_id, req->tsid);

	*resp_len = cpu_to_le16(data_len);
	resp->hdr.type = TFC_VF2PF_TYPE_TBL_SCOPE_MEM_ALLOC_CFG_CMD;
	resp->tsid = req->tsid;
	resp->status = rc;
	return rc;
}

static int
tfc_vf2pf_mem_free_process(struct tfc *tfcp, u32 *oem_data, u32 *resp_data, u16 *resp_len)
{
	struct tfc_vf2pf_tbl_scope_mem_free_resp *resp =
		(struct tfc_vf2pf_tbl_scope_mem_free_resp *)resp_data;
	struct tfc_vf2pf_tbl_scope_mem_free_cmd *req =
		(struct tfc_vf2pf_tbl_scope_mem_free_cmd *)oem_data;
	u16 data_len = sizeof(*resp);
	struct bnxt *bp = tfcp->bp;
	int rc;

	if (*resp_len < data_len) {
		netdev_dbg(bp->dev, "%s: resp_data buffer is too small\n", __func__);
		return -EINVAL;
	}

	/* This block of code is for testing purpose. Will be removed later */
	netdev_dbg(bp->dev, "%s: Table scope mem free cfg cmd:\n", __func__);
	netdev_dbg(bp->dev, "\ttsid: 0x%x\n", req->tsid);

	rc = tfc_tbl_scope_mem_free(tfcp, req->hdr.fid, req->tsid);
	if (!rc) {
		netdev_dbg(bp->dev, "%s: tsid(%d) PF free succeeds\n",
			   __func__, req->tsid);
	} else {
		netdev_dbg(bp->dev, "%s: tsid(%d) PF free fails (%d)\n",
			   __func__, req->tsid, rc);
	}

	bnxt_debug_tf_delete(bp);

	*resp_len = cpu_to_le16(data_len);
	resp->hdr.type = TFC_VF2PF_TYPE_TBL_SCOPE_MEM_FREE_CMD;
	resp->tsid = req->tsid;
	resp->status = rc;
	return rc;
}

static int
tfc_vf2pf_pool_alloc_process(struct tfc *tfcp,
			     u32 *oem_data,
			     u32 *resp_data,
			     u16 *resp_len)
{
	struct tfc_vf2pf_tbl_scope_pool_alloc_resp *resp =
		(struct tfc_vf2pf_tbl_scope_pool_alloc_resp *)resp_data;
	struct tfc_vf2pf_tbl_scope_pool_alloc_cmd *req =
		(struct tfc_vf2pf_tbl_scope_pool_alloc_cmd *)oem_data;
	u16 data_len = sizeof(*resp);
	struct bnxt *bp = tfcp->bp;
	u8 pool_sz_exp = 0;
	u16 pool_id = 0;
	int rc;

	if (*resp_len < data_len) {
		netdev_dbg(bp->dev, "%s: resp_data buffer is too small\n", __func__);
		return -EINVAL;
	}

	/* This block of code is for testing purpose. Will be removed later */
	netdev_dbg(bp->dev, "%s: Table scope pool alloc cmd:\n", __func__);
	netdev_dbg(bp->dev, "\ttsid: 0x%x, region:%s fid(%d)\n", req->tsid,
		   tfc_ts_region_2_str(req->region, req->dir), req->hdr.fid);

	rc = tfc_tbl_scope_pool_alloc(tfcp, req->hdr.fid, req->tsid, req->region,
				      req->dir, &pool_sz_exp, &pool_id);
	if (!rc) {
		netdev_dbg(bp->dev, "%s: tsid(%d) PF pool_alloc(%d) succeeds\n",
			   __func__, req->tsid, pool_id);
	} else {
		netdev_dbg(bp->dev, "%s: tsid(%d) PF pool_alloc fails (%d)\n",
			   __func__, req->tsid, rc);
	}
	*resp_len = cpu_to_le16(data_len);
	resp->hdr.type = TFC_VF2PF_TYPE_TBL_SCOPE_POOL_ALLOC_CMD;
	resp->tsid = req->tsid;
	resp->pool_sz_exp = pool_sz_exp;
	resp->pool_id = pool_id;
	resp->status = rc;
	return rc;
}

static int
tfc_vf2pf_pool_free_process(struct tfc *tfcp,
			    u32 *oem_data,
			    u32 *resp_data,
			    u16 *resp_len)
{
	struct tfc_vf2pf_tbl_scope_pool_free_resp *resp =
		(struct tfc_vf2pf_tbl_scope_pool_free_resp *)resp_data;
	struct tfc_vf2pf_tbl_scope_pool_free_cmd *req =
		(struct tfc_vf2pf_tbl_scope_pool_free_cmd *)oem_data;
	u16 data_len = sizeof(*resp);
	struct bnxt *bp = tfcp->bp;
	int rc;

	if (*resp_len < data_len) {
		netdev_dbg(bp->dev, "%s: resp_data buffer is too small\n", __func__);
		return -EINVAL;
	}

	/* This block of code is for testing purpose. Will be removed later */
	netdev_dbg(bp->dev, "%s: Table scope pool free cfg cmd:\n", __func__);
	netdev_dbg(bp->dev, "\ttsid: 0x%x\n", req->tsid);

	rc = tfc_tbl_scope_pool_free(tfcp, req->hdr.fid, req->tsid, req->region,
				     req->dir, req->pool_id);
	if (!rc) {
		netdev_dbg(bp->dev, "%s: tsid(%d) PF free succeeds\n", __func__, req->tsid);
	} else {
		netdev_dbg(bp->dev, "%s: tsid(%d) PF free fails (%d)\n", __func__, req->tsid,
			   rc);
	}
	*resp_len = cpu_to_le16(data_len);
	resp->hdr.type = TFC_VF2PF_TYPE_TBL_SCOPE_MEM_FREE_CMD;
	resp->tsid = req->tsid;
	resp->status = rc;
	return rc;
}

int
tfc_oem_cmd_process(struct tfc *tfcp, u32 *oem_data, u32 *resp, u16 *resp_len)
{
	struct tfc_vf2pf_hdr *oem = (struct tfc_vf2pf_hdr *)oem_data;
	int rc;

	switch (oem->type) {
	case TFC_VF2PF_TYPE_TBL_SCOPE_MEM_ALLOC_CFG_CMD:
		rc = tfc_vf2pf_mem_alloc_process(tfcp, oem_data, resp, resp_len);
		break;
	case TFC_VF2PF_TYPE_TBL_SCOPE_MEM_FREE_CMD:
		rc = tfc_vf2pf_mem_free_process(tfcp, oem_data, resp, resp_len);
		break;

	case TFC_VF2PF_TYPE_TBL_SCOPE_POOL_ALLOC_CMD:
		rc = tfc_vf2pf_pool_alloc_process(tfcp, oem_data, resp, resp_len);
		break;

	case TFC_VF2PF_TYPE_TBL_SCOPE_POOL_FREE_CMD:
		rc = tfc_vf2pf_pool_free_process(tfcp, oem_data, resp, resp_len);
		break;
	case TFC_VF2PF_TYPE_TBL_SCOPE_PFID_QUERY_CMD:
	default:
		rc = -EPERM;
		break;
	}

	return rc;
}
