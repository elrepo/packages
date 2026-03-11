/*
 * Copyright (c) 2015-2025, Broadcom. All rights reserved.  The term
 * Broadcom refers to Broadcom Inc. and/or its subsidiaries.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * BSD license below:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Description: Main component of the bnxt_re driver
 */

#include <linux/inetdevice.h>
#include <linux/if_bonding.h>

#include "bnxt_re.h"
#include "configfs.h"
#ifdef ENABLE_DEBUGFS
#include "debugfs.h"
#endif

#include "ib_verbs.h"
#include "bnxt_re-abi.h"
#include "bnxt.h"
#include "hdbr.h"
#include "hw_counters.h"
#include "bnxt_dcb.h"

static char version[] =
		"Broadcom NetXtreme-C/E RoCE Driver " ROCE_DRV_MODULE_NAME \
		" v" ROCE_DRV_MODULE_VERSION " (" ROCE_DRV_MODULE_RELDATE ")\n";

#define BNXT_RE_DESC	"Broadcom NetXtreme RoCE"

MODULE_AUTHOR("Eddie Wai <eddie.wai@broadcom.com>");
MODULE_DESCRIPTION(BNXT_RE_DESC " Driver");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_VERSION(ROCE_DRV_MODULE_VERSION);

#if defined(HAVE_IB_UMEM_DMABUF) && !defined(HAVE_IB_UMEM_DMABUF_PINNED)
MODULE_IMPORT_NS(DMA_BUF);
#endif

DEFINE_MUTEX(bnxt_re_mutex); /* mutex lock for driver */

unsigned int restrict_stats = 0;
module_param(restrict_stats, uint, 0);
MODULE_PARM_DESC(restrict_stats, "Restrict stats query frequency to ethtool coalesce value. Disabled by default");

unsigned int min_tx_depth = 1;
module_param(min_tx_depth, uint, 0);
MODULE_PARM_DESC(min_tx_depth, "Minimum TX depth - Default is 1");

unsigned int cmdq_shadow_qd = RCFW_CMD_NON_BLOCKING_SHADOW_QD;
module_param_named(cmdq_shadow_qd, cmdq_shadow_qd, uint, 0644);
MODULE_PARM_DESC(cmdq_shadow_qd, "Perf Stat Debug: Shadow QD Range (1-64) - Default is 64");

#define BNXT_RE_EVENT_UDCC_SESSION_ID(data1)							\
	(((data1) & ASYNC_EVENT_UDCC_SESSION_CHANGE_EVENT_DATA1_UDCC_SESSION_ID_MASK) >>	\
	 ASYNC_EVENT_UDCC_SESSION_CHANGE_EVENT_DATA1_UDCC_SESSION_ID_SFT)

#define BNXT_RE_EVENT_UDCC_SESSION_OPCODE(data2)						\
	(((data2) & ASYNC_EVENT_UDCC_SESSION_CHANGE_EVENT_DATA2_SESSION_ID_OP_CODE_MASK) >>	\
	 ASYNC_EVENT_UDCC_SESSION_CHANGE_EVENT_DATA2_SESSION_ID_OP_CODE_SFT)

#define BNXT_RE_UDCC_SESSION_CREATE	0
#define BNXT_RE_UDCC_SESSION_DELETE	1

/* globals */
struct list_head bnxt_re_dev_list = LIST_HEAD_INIT(bnxt_re_dev_list);

/* Global variable to distinguish driver unload and PCI removal */
static u32 gmod_exit;

static void bnxt_re_task(struct work_struct *work_task);

static struct workqueue_struct *bnxt_re_wq;

static int bnxt_re_update_fw_lag_info(struct bnxt_re_bond_info *binfo,
			       struct bnxt_re_dev *rdev,
			       bool aggr_en);
static int bnxt_re_query_hwrm_intf_version(struct bnxt_re_dev *rdev);

static int bnxt_re_init_cc_param(struct bnxt_re_dev *rdev);
static void bnxt_re_clear_cc_param(struct bnxt_re_dev *rdev);
static int bnxt_re_hwrm_dbr_pacing_qcfg(struct bnxt_re_dev *rdev);
static int bnxt_re_ib_init(struct bnxt_re_dev *rdev);
static void bnxt_re_ib_init_2(struct bnxt_re_dev *rdev);
static void bnxt_re_dispatch_event(struct ib_device *ibdev, struct ib_qp *qp,
				   u8 port_num, enum ib_event_type event);
static struct bnxt_re_bond_info *binfo_from_slave_ndev(struct net_device *netdev);

static void bnxt_re_update_fifo_occup_slabs(struct bnxt_re_dev *rdev,
					    u32 fifo_occup)
{
	if (fifo_occup > rdev->dbg_stats->dbq.fifo_occup_water_mark)
		rdev->dbg_stats->dbq.fifo_occup_water_mark = fifo_occup;

	if (fifo_occup > 8 * rdev->pacing_algo_th)
		rdev->dbg_stats->dbq.fifo_occup_slab_4++;
	else if (fifo_occup > 4 * rdev->pacing_algo_th)
		rdev->dbg_stats->dbq.fifo_occup_slab_3++;
	else if (fifo_occup > 2 * rdev->pacing_algo_th)
		rdev->dbg_stats->dbq.fifo_occup_slab_2++;
	else if (fifo_occup > rdev->pacing_algo_th)
		rdev->dbg_stats->dbq.fifo_occup_slab_1++;
}

static void bnxt_re_update_do_pacing_slabs(struct bnxt_re_dev *rdev)
{
	struct bnxt_qplib_db_pacing_data *pacing_data = rdev->qplib_res.pacing_data;

	if (pacing_data->do_pacing > rdev->dbg_stats->dbq.do_pacing_water_mark)
		rdev->dbg_stats->dbq.do_pacing_water_mark = pacing_data->do_pacing;

	if (pacing_data->do_pacing > 16 * rdev->dbr_def_do_pacing)
		rdev->dbg_stats->dbq.do_pacing_slab_5++;
	else if (pacing_data->do_pacing > 8 * rdev->dbr_def_do_pacing)
		rdev->dbg_stats->dbq.do_pacing_slab_4++;
	else if (pacing_data->do_pacing > 4 * rdev->dbr_def_do_pacing)
		rdev->dbg_stats->dbq.do_pacing_slab_3++;
	else if (pacing_data->do_pacing > 2 * rdev->dbr_def_do_pacing)
		rdev->dbg_stats->dbq.do_pacing_slab_2++;
	else if (pacing_data->do_pacing > rdev->dbr_def_do_pacing)
		rdev->dbg_stats->dbq.do_pacing_slab_1++;
}

static bool bnxt_re_is_qp1_qp(struct bnxt_re_qp *qp)
{
	return qp->ib_qp.qp_type == IB_QPT_GSI;
}

static struct bnxt_re_qp *bnxt_re_get_qp1_qp(struct bnxt_re_dev *rdev)
{
	struct bnxt_re_qp *qp;

	mutex_lock(&rdev->qp_lock);
	list_for_each_entry(qp, &rdev->qp_list, list) {
		if (bnxt_re_is_qp1_qp(qp)) {
			mutex_unlock(&rdev->qp_lock);
			return qp;
		}
	}
	mutex_unlock(&rdev->qp_lock);
	return NULL;
}

/* SR-IOV helper functions */
static void bnxt_re_get_sriov_func_type(struct bnxt_re_dev *rdev)
{
	struct bnxt_en_dev *en_dev = rdev->en_dev;

	if (rdev->binfo)
		return;

	rdev->is_virtfn = !!BNXT_EN_VF(en_dev);
}

/* Set the maximum number of each resource that the driver actually wants
 * to allocate. This may be up to the maximum number the firmware has
 * reserved for the function. The driver may choose to allocate fewer
 * resources than the firmware maximum.
 */
static void bnxt_re_limit_pf_res(struct bnxt_re_dev *rdev)
{
	struct bnxt_qplib_max_res dev_res = {};
	struct bnxt_qplib_chip_ctx *cctx;
	struct bnxt_qplib_dev_attr *attr;
	struct bnxt_qplib_ctx *hctx;
	int i;

	attr = rdev->dev_attr;
	hctx = rdev->qplib_res.hctx;
	cctx = rdev->chip_ctx;

	bnxt_qplib_max_res_supported(cctx, &rdev->qplib_res, &dev_res, false);
	if (!_is_chip_p5_plus(cctx)) {
		hctx->qp_ctx.max = min_t(u32, dev_res.max_qp, attr->max_qp);
		hctx->mrw_ctx.max = min_t(u32, dev_res.max_mr, attr->max_mr);
		/* To accommodate 16k MRs and 16k AHs,
		 * driver has to allocate 32k backing store memory
		 */
		hctx->mrw_ctx.max *= 2;
		hctx->srq_ctx.max = min_t(u32, dev_res.max_srq, attr->max_srq);
		hctx->cq_ctx.max = min_t(u32, dev_res.max_cq, attr->max_cq);
		for (i = 0; i < MAX_TQM_ALLOC_REQ; i++)
			hctx->tqm_ctx.qcount[i] = attr->tqm_alloc_reqs[i];
	} else {
		hctx->qp_ctx.max = attr->max_qp ? attr->max_qp : dev_res.max_qp;
		hctx->mrw_ctx.max = attr->max_mr ? attr->max_mr : dev_res.max_mr;
		hctx->srq_ctx.max = attr->max_srq ? attr->max_srq : dev_res.max_srq;
		hctx->cq_ctx.max = attr->max_cq ? attr->max_cq : dev_res.max_cq;
	}
}

static void bnxt_re_limit_vf_res(struct bnxt_re_dev *rdev,
				 struct bnxt_qplib_vf_res *vf_res,
				 u32 num_vf)
{
	struct bnxt_qplib_chip_ctx *cctx = rdev->chip_ctx;
	struct bnxt_qplib_max_res dev_res = {};

	memset(vf_res, 0, sizeof(*vf_res));

	bnxt_qplib_max_res_supported(cctx, &rdev->qplib_res, &dev_res, true);
	vf_res->max_qp = dev_res.max_qp / num_vf;
	vf_res->max_srq = dev_res.max_srq / num_vf;
	vf_res->max_cq = dev_res.max_cq / num_vf;
	/*
	 * MR and AH shares the same backing store, the value specified
	 * for max_mrw is split into half by the FW for MR and AH
	 */
	vf_res->max_mrw = dev_res.max_mr * 2 / num_vf;
	vf_res->max_gid = BNXT_RE_MAX_GID_PER_VF;
}

static void bnxt_re_dettach_irq(struct bnxt_re_dev *rdev)
{
	struct bnxt_qplib_rcfw *rcfw = NULL;
	struct bnxt_qplib_nq *nq;
	bool kill_tasklet;
	int indx;

	kill_tasklet = test_bit(BNXT_STATE_PCI_CHANNEL_IO_FROZEN, &rdev->en_dev->en_state);
	rcfw = &rdev->rcfw;

	if (!test_bit(BNXT_RE_FLAG_SETUP_NQ, &rdev->flags))
		goto skip_nq;

	for (indx = 0; indx < rdev->nqr->max_init; indx++) {
		nq = &rdev->nqr->nq[indx];
		mutex_lock(&nq->lock);
		bnxt_qplib_nq_stop_irq(nq, kill_tasklet);
		mutex_unlock(&nq->lock);
	}

skip_nq:
	if (test_bit(BNXT_RE_FLAG_ALLOC_RCFW, &rdev->flags))
		bnxt_qplib_rcfw_stop_irq(rcfw, kill_tasklet);
}

int bnxt_re_get_pri_dscp_settings(struct bnxt_re_dev *rdev,
				  u16 target_id,
				  struct bnxt_re_tc_rec *tc_rec)
{
	struct bnxt_re_dscp2pri *d2p;
	int rc = 0;
	int i;

	rc = bnxt_re_hwrm_pri2cos_qcfg(rdev, tc_rec, target_id);
	if (rc)
		return rc;

	tc_rec->dscp_valid = 0;
	tc_rec->cnp_dscp_bv = 0;
	tc_rec->roce_dscp_bv = 0;
	rc = bnxt_re_query_hwrm_dscp2pri(rdev, target_id);
	if (rc)
		return rc;
	d2p = rdev->d2p;
	for (i = 0; i < rdev->d2p_count; i++) {
		if (d2p[i].pri == tc_rec->roce_prio) {
			tc_rec->roce_dscp = d2p[i].dscp;
			tc_rec->roce_dscp_bv |= (1ull << d2p[i].dscp);
			tc_rec->dscp_valid |= (1 << ROCE_DSCP_VALID);
		} else if (d2p[i].pri == tc_rec->cnp_prio) {
			tc_rec->cnp_dscp = d2p[i].dscp;
			tc_rec->cnp_dscp_bv |= (1ull << d2p[i].dscp);
			tc_rec->dscp_valid |= (1 << CNP_DSCP_VALID);
		}
	}

	return 0;
}

struct bnxt_re_dcb_work {
	struct work_struct work;
	struct bnxt_re_dev *rdev;
	struct hwrm_async_event_cmpl cmpl;
};

static void bnxt_re_uninit_dest_ah_wq(struct bnxt_re_dev *rdev)
{
	u8 retry = BNXT_RE_AH_UNINIT_RETRY;
	struct workqueue_struct *wq;

	if (!rdev->dest_ah_wq)
		return;

	wq = rdev->dest_ah_wq;
	rdev->dest_ah_wq = NULL;
	while ((refcount_read(&rdev->pos_destah_cnt) > 1) && retry--)
		msleep(BNXT_RE_AH_DEST_DELAY);

	destroy_workqueue(wq);
}

static void bnxt_re_init_dest_ah_wq(struct bnxt_re_dev *rdev)
{
	rdev->dest_ah_wq = create_workqueue("bnxt_re_dest_ah");
}

static void bnxt_re_init_dcb_wq(struct bnxt_re_dev *rdev)
{
	rdev->dcb_wq = create_singlethread_workqueue("bnxt_re_dcb_wq");
}

static void bnxt_re_uninit_dcb_wq(struct bnxt_re_dev *rdev)
{
	if (!rdev->dcb_wq)
		return;
	flush_workqueue(rdev->dcb_wq);
	destroy_workqueue(rdev->dcb_wq);
	rdev->dcb_wq = NULL;
}

static void bnxt_re_init_aer_wq(struct bnxt_re_dev *rdev)
{
	rdev->aer_wq = create_singlethread_workqueue("bnxt_re_aer_wq");
}

static void bnxt_re_uninit_aer_wq(struct bnxt_re_dev *rdev)
{
	if (!rdev->aer_wq)
		return;
	flush_workqueue(rdev->aer_wq);
	destroy_workqueue(rdev->aer_wq);
	rdev->aer_wq = NULL;
}

static void bnxt_re_init_udcc_wq(struct bnxt_re_dev *rdev)
{
	if (bnxt_qplib_udcc_supported(rdev->chip_ctx)) {
		struct net_device *netdev = rdev->binfo ? rdev->binfo->slave1 : rdev->netdev;
		char *name;

		name = kasprintf(GFP_KERNEL, "%s-re-udcc-wq",
				 dev_name(netdev->dev.parent));
		if (!name)
			return;

		rdev->udcc_wq = create_singlethread_workqueue(name);
		kfree(name);
	}
}

static void bnxt_re_uninit_udcc_wq(struct bnxt_re_dev *rdev)
{
	if (!rdev->udcc_wq)
		return;
	flush_workqueue(rdev->udcc_wq);
	destroy_workqueue(rdev->udcc_wq);
	rdev->udcc_wq = NULL;
}

static int bnxt_re_update_qp1_tos_dscp(struct bnxt_re_dev *rdev)
{
	struct bnxt_re_qp *qp;

	if (!_is_chip_p5_plus(rdev->chip_ctx))
		return 0;

	qp = bnxt_re_get_qp1_qp(rdev);
	if (!qp)
		return 0;

	qp->qplib_qp.modify_flags = CMDQ_MODIFY_QP_MODIFY_MASK_TOS_DSCP;
	qp->qplib_qp.tos_dscp = rdev->cc_param.qp1_tos_dscp;

	return bnxt_qplib_modify_qp(&rdev->qplib_res, &qp->qplib_qp);
}

static void bnxt_re_reconfigure_dscp(struct bnxt_re_dev *rdev)
{
	struct bnxt_qplib_cc_param *cc_param;
	struct bnxt_re_tc_rec *tc_rec;
	bool update_cc = false;
	u8 dscp_user;
	int rc;

	cc_param = &rdev->cc_param;
	tc_rec = &rdev->tc_rec[0];

	if (!(cc_param->roce_dscp_user || cc_param->cnp_dscp_user))
		return;

	if (cc_param->cnp_dscp_user) {
		dscp_user = (cc_param->cnp_dscp_user & 0x3f);
		if ((tc_rec->cnp_dscp_bv & (1ul << dscp_user)) &&
		    (cc_param->alt_tos_dscp != dscp_user)) {
			cc_param->alt_tos_dscp = dscp_user;
			cc_param->mask |= CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_ALT_TOS_DSCP;
			update_cc = true;
		}
	}

	if (cc_param->roce_dscp_user) {
		dscp_user = (cc_param->roce_dscp_user & 0x3f);
		if ((tc_rec->roce_dscp_bv & (1ul << dscp_user)) &&
		    (cc_param->tos_dscp != dscp_user)) {
			cc_param->tos_dscp = dscp_user;
			cc_param->mask |= CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_TOS_DSCP;
			update_cc = true;
		}
	}

	if (update_cc) {
		rc = bnxt_qplib_modify_cc(&rdev->qplib_res, cc_param);
		if (rc)
			dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	}
}

static void bnxt_re_dcb_wq_task(struct work_struct *work)
{
	struct bnxt_qplib_cc_param *cc_param;
	struct bnxt_re_tc_rec *tc_rec;
	struct bnxt_re_dev *rdev;
	struct bnxt_re_dcb_work *dcb_work =
			container_of(work, struct bnxt_re_dcb_work, work);
	int rc;

	rdev = dcb_work->rdev;
	if (!rdev)
		goto exit;

	mutex_lock(&rdev->cc_lock);

	cc_param = &rdev->cc_param;
	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query ccparam rc:%d", rc);
		goto fail;
	}
	tc_rec = &rdev->tc_rec[0];
	rc = bnxt_re_get_pri_dscp_settings(rdev, -1, tc_rec);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to get pri2cos rc:%d", rc);
		goto fail;
	}

	/*
	 * Upon the receival of DCB Async event:
	 *   If roce_dscp or cnp_dscp or both (which user configured using configfs)
	 *   is in the list, re-program the value using modify_roce_cc command
	 */
	bnxt_re_reconfigure_dscp(rdev);

	cc_param->roce_pri = tc_rec->roce_prio;
	if (cc_param->qp1_tos_dscp != cc_param->tos_dscp) {
		cc_param->qp1_tos_dscp = cc_param->tos_dscp;
		rc = bnxt_re_update_qp1_tos_dscp(rdev);
		if (rc) {
			dev_err(rdev_to_dev(rdev), "%s:Failed to modify QP1 rc:%d",
				__func__, rc);
			goto fail;
		}
	}

fail:
	mutex_unlock(&rdev->cc_lock);
exit:
	kfree(dcb_work);
}

static int bnxt_re_hwrm_dbr_pacing_broadcast_event(struct bnxt_re_dev *rdev)
{
	struct hwrm_func_dbr_pacing_broadcast_event_output resp = {0};
	struct hwrm_func_dbr_pacing_broadcast_event_input req = {0};
	struct bnxt_en_dev *en_dev = rdev->en_dev;
	struct bnxt_fw_msg fw_msg = {};
	int rc;

	bnxt_re_init_hwrm_hdr((void *)&req, HWRM_FUNC_DBR_PACING_BROADCAST_EVENT, -1);
	bnxt_re_fill_fw_msg(&fw_msg, (void *)&req, sizeof(req), (void *)&resp,
			    sizeof(resp), BNXT_RE_HWRM_CMD_TIMEOUT(rdev));
	rc = bnxt_send_msg(en_dev, &fw_msg);
	if (rc) {
		dev_dbg(rdev_to_dev(rdev),
			"Failed to send dbr pacing broadcast event rc:%d", rc);
		return rc;
	}
	return 0;
}

static int bnxt_re_hwrm_dbr_pacing_nqlist_query(struct bnxt_re_dev *rdev)
{
	struct hwrm_func_dbr_pacing_nqlist_query_output resp = {0};
	struct hwrm_func_dbr_pacing_nqlist_query_input req = {0};
	struct bnxt_dbq_nq_list *nq_list = &rdev->nq_list;
	struct bnxt_en_dev *en_dev = rdev->en_dev;
	struct bnxt_fw_msg fw_msg = {};
	bool primary_found = false;
	struct bnxt_qplib_nq *nq;
	int rc, i, j = 1;
	u16 *nql_ptr;

	nq = &rdev->nqr->nq[0];

	bnxt_re_init_hwrm_hdr((void *)&req, HWRM_FUNC_DBR_PACING_NQLIST_QUERY, -1);
	bnxt_re_fill_fw_msg(&fw_msg, (void *)&req, sizeof(req), (void *)&resp,
			    sizeof(resp), BNXT_RE_HWRM_CMD_TIMEOUT(rdev));
	rc = bnxt_send_msg(en_dev, &fw_msg);
	if (rc) {
		dev_dbg(rdev_to_dev(rdev),
			"Failed to send dbr pacing nq list query rc:%d", rc);
		return rc;
	}
	nq_list->num_nql_entries = le32_to_cpu(resp.num_nqs);
	nql_ptr = &resp.nq_ring_id0;
	/* populate the nq_list of the primary function with list received
	 * from FW. Fill the NQ IDs of secondary functions from index 1 to
	 * num_nql_entries - 1. Fill the  nq_list->nq_id[0] with the
	 * nq_id of the primary pf
	 */
	for (i = 0; i < nq_list->num_nql_entries; i++) {
		u16 nq_id = *nql_ptr;

		dev_dbg(rdev_to_dev(rdev),
			"nq_list->nq_id[%d] = %d\n", i, nq_id);
		if (nq_id != nq->ring_id) {
			nq_list->nq_id[j] = nq_id;
			j++;
		} else {
			primary_found = true;
			nq_list->nq_id[0] = nq->ring_id;
		}
		nql_ptr++;
	}
	if (primary_found)
		bnxt_qplib_dbr_pacing_set_primary_pf(rdev->chip_ctx, 1);
	else
		dev_err(rdev_to_dev(rdev),
			"%s primary NQ id missing", __func__);

	return 0;
}

static void __wait_for_fifo_occupancy_below_th(struct bnxt_re_dev *rdev)
{
	struct bnxt_qplib_db_pacing_data *pacing_data = rdev->qplib_res.pacing_data;
	u32 read_val, fifo_occup;
	bool first_read = true;
	u32 retry_fifo_check = 1000;

	/* loop shouldn't run infintely as the occupancy usually goes
	 * below pacing algo threshold as soon as pacing kicks in.
	 */
	while (1) {
		read_val = readl(rdev->en_dev->bar0 + rdev->dbr_db_fifo_reg_off);
		fifo_occup = pacing_data->fifo_max_depth -
			     ((read_val & pacing_data->fifo_room_mask) >>
			      pacing_data->fifo_room_shift);
		/* Fifo occupancy cannot be greater the MAX FIFO depth */
		if (fifo_occup > pacing_data->fifo_max_depth)
			break;

		if (first_read) {
			bnxt_re_update_fifo_occup_slabs(rdev, fifo_occup);
			first_read = false;
		}
		if (fifo_occup < pacing_data->pacing_th)
			break;
		if (!retry_fifo_check--) {
			dev_info_once(rdev_to_dev(rdev),
				      "%s: read_val = 0x%x fifo_occup = 0x%xfifo_max_depth = 0x%x pacing_th = 0x%x\n",
				      __func__, read_val, fifo_occup, pacing_data->fifo_max_depth,
				      pacing_data->pacing_th);
			rdev->dbg_stats->dbq.do_pacing_retry++;
			break;
		}
	}
}

static void bnxt_re_set_default_pacing_data(struct bnxt_re_dev *rdev)
{
	struct bnxt_qplib_db_pacing_data *pacing_data = rdev->qplib_res.pacing_data;

	pacing_data->do_pacing = rdev->dbr_def_do_pacing;
	pacing_data->pacing_th = rdev->pacing_algo_th;
	pacing_data->alarm_th =
		pacing_data->pacing_th * BNXT_RE_PACING_ALARM_TH_MULTIPLE;
}

#define CAG_RING_MASK 0x7FF
#define CAG_RING_SHIFT 17
#define WATERMARK_MASK 0xFFF
#define WATERMARK_SHIFT	0

static bool bnxt_re_check_if_dbq_intr_triggered(struct bnxt_re_dev *rdev)
{
	u32 read_val;
	int j;

	for (j = 0; j < 10; j++) {
		read_val = readl(rdev->en_dev->bar0 + rdev->dbr_aeq_arm_reg_off);
		dev_dbg(rdev_to_dev(rdev), "AEQ ARM status = 0x%x\n",
			read_val);
		if (!read_val)
			return true;
	}
	return false;
}

int bnxt_re_set_dbq_throttling_reg(struct bnxt_re_dev *rdev, u16 nq_id, u32 throttle)
{
	u32 cag_ring_water_mark = 0, read_val;
	u32 throttle_val;

	/* Convert throttle percentage to value */
	throttle_val = (rdev->qplib_res.pacing_data->fifo_max_depth * throttle) / 100;

	if (bnxt_qplib_dbr_pacing_ext_en(rdev->chip_ctx)) {
		cag_ring_water_mark = (nq_id & CAG_RING_MASK) << CAG_RING_SHIFT |
				      (throttle_val & WATERMARK_MASK);
		writel(cag_ring_water_mark, rdev->en_dev->bar0 + rdev->dbr_throttling_reg_off);
		read_val = readl(rdev->en_dev->bar0 + rdev->dbr_throttling_reg_off);
		dev_dbg(rdev_to_dev(rdev),
			"%s: dbr_throttling_reg_off read_val = 0x%x\n",
			__func__, read_val);
		if (read_val != cag_ring_water_mark) {
			dev_dbg(rdev_to_dev(rdev),
				"nq_id = %d write_val=0x%x read_val=0x%x\n",
				nq_id, cag_ring_water_mark, read_val);
			return 1;
		}
	}
	writel(1, rdev->en_dev->bar0 + rdev->dbr_aeq_arm_reg_off);
	return 0;
}

static void bnxt_re_set_dbq_throttling_for_non_primary(struct bnxt_re_dev *rdev)
{
	struct bnxt_dbq_nq_list *nq_list;
	struct bnxt_qplib_nq *nq;
	int i;

	nq_list = &rdev->nq_list;
	/* Run a loop for other Active functions if this is primary function */
	if (bnxt_qplib_dbr_pacing_is_primary_pf(rdev->chip_ctx)) {
		dev_dbg(rdev_to_dev(rdev), "%s:  nq_list->num_nql_entries= %d\n",
			__func__, nq_list->num_nql_entries);
		nq = &rdev->nqr->nq[0];
		for (i = nq_list->num_nql_entries - 1; i > 0; i--) {
			u16 nq_id = nq_list->nq_id[i];

			dev_dbg(rdev_to_dev(rdev),
				"%s: nq_id = %d cur_fn_ring_id = %d\n",
				__func__, nq_id, nq->ring_id);
			if (bnxt_re_set_dbq_throttling_reg
					(rdev, nq_id, 0))
				break;
			bnxt_re_check_if_dbq_intr_triggered(rdev);
		}
	}
}

static void bnxt_re_handle_dbr_nq_pacing_notification(struct bnxt_re_dev *rdev)
{
	struct bnxt_qplib_nq *nq;
	int rc = 0;

	nq = &rdev->nqr->nq[0];

	dev_dbg(rdev_to_dev(rdev), "%s: Query NQ list for DBR pacing",
		__func__);

	/* Query the NQ list*/
	rc = bnxt_re_hwrm_dbr_pacing_nqlist_query(rdev);
	if (rc) {
		dev_err(rdev_to_dev(rdev),
			"Failed to Query NQ list rc= %d", rc);
		return;
	}
	/*Configure GRC access for Throttling and aeq_arm register */
	writel(rdev->chip_ctx->dbr_aeq_arm_reg &
		BNXT_GRC_BASE_MASK,
		rdev->en_dev->bar0 + BNXT_GRCPF_REG_WINDOW_BASE_OUT + 28);

	rdev->dbr_throttling_reg_off =
		(rdev->chip_ctx->dbr_throttling_reg &
		 BNXT_GRC_OFFSET_MASK) + 0x8000;
	rdev->dbr_aeq_arm_reg_off =
		(rdev->chip_ctx->dbr_aeq_arm_reg &
		 BNXT_GRC_OFFSET_MASK) + 0x8000;

	bnxt_re_set_dbq_throttling_reg(rdev, nq->ring_id, rdev->dbq_watermark);
}

static void bnxt_re_dbr_drop_recov_task(struct work_struct *work)
{
	struct bnxt_re_dbr_drop_recov_work *dbr_recov_work =
			container_of(work, struct bnxt_re_dbr_drop_recov_work, work);
	struct bnxt_re_res_list *res_list;
	u64 start_time, diff_time_msec;
	struct bnxt_re_ucontext *uctx;
	bool user_dbr_drop_recov;
	struct bnxt_re_dev *rdev;
	struct bnxt_re_srq *srq;
	u32 user_recov_pend = 0;
	struct bnxt_re_qp *qp;
	struct bnxt_re_cq *cq;
	int i;

	rdev = dbr_recov_work->rdev;
	if (!rdev)
		goto exit;

	if (test_bit(BNXT_RE_FLAG_ERR_DEVICE_DETACHED, &rdev->flags))
		goto exit;

	if (dbr_recov_work->curr_epoch != rdev->dbr_evt_curr_epoch) {
		rdev->dbr_sw_stats->dbr_drop_recov_event_skips++;
		dev_dbg(rdev_to_dev(rdev), "%s: Ignore DBR recov evt epoch %d (latest ep %d)\n",
			__func__, dbr_recov_work->curr_epoch, rdev->dbr_evt_curr_epoch);
		goto exit;
	}

	user_dbr_drop_recov = rdev->user_dbr_drop_recov;
	rdev->dbr_recovery_on = true;

	/* CREQ */
	bnxt_qplib_replay_db(&rdev->rcfw.creq.creq_db.dbinfo, false);

	/* NQ */
	for (i = 0; i < rdev->nqr->max_init; i++)
		bnxt_qplib_replay_db(&rdev->nqr->nq[i].nq_db.dbinfo, false);

	/* ARM_ENA for all userland CQs */
	res_list = &rdev->res_list[BNXT_RE_RES_TYPE_CQ];
	spin_lock(&res_list->lock);
	list_for_each_entry(cq, &res_list->head, res_list) {
		if (cq->umem)
			bnxt_qplib_replay_db(&cq->qplib_cq.dbinfo, true);
	}
	spin_unlock(&res_list->lock);

	/* ARM_ENA for all userland SRQs */
	res_list = &rdev->res_list[BNXT_RE_RES_TYPE_SRQ];
	spin_lock(&res_list->lock);
	list_for_each_entry(srq, &res_list->head, res_list) {
		if (srq->qplib_srq.is_user)
			bnxt_qplib_replay_db(&srq->qplib_srq.dbinfo, true);
	}
	spin_unlock(&res_list->lock);

	if (!user_dbr_drop_recov)
		goto skip_user_recovery;

	/* Notify all uusrlands */
	res_list = &rdev->res_list[BNXT_RE_RES_TYPE_UCTX];
	spin_lock(&res_list->lock);
	list_for_each_entry(uctx, &res_list->head, res_list) {
		uint32_t *user_epoch = uctx->dbr_recov_cq_page;

		if (!user_epoch || !uctx->dbr_recov_cq) {
			dev_dbg(rdev_to_dev(rdev), "%s: %d Found %s = NULL during DBR recovery\n",
				__func__, __LINE__, (user_epoch) ? "dbr_recov_cq" : "user_epoch");
			continue;
		}

		*user_epoch = dbr_recov_work->curr_epoch;
		if (uctx->dbr_recov_cq->ib_cq.comp_handler)
			(*uctx->dbr_recov_cq->ib_cq.comp_handler)
				(&uctx->dbr_recov_cq->ib_cq,
				 uctx->dbr_recov_cq->ib_cq.cq_context);
	}
	spin_unlock(&res_list->lock);

skip_user_recovery:
	/* ARM_ENA and Cons update DBs for Kernel CQs */
	res_list = &rdev->res_list[BNXT_RE_RES_TYPE_CQ];
	spin_lock(&res_list->lock);
	list_for_each_entry(cq, &res_list->head, res_list) {
		if (!cq->umem) {
			bnxt_qplib_replay_db(&cq->qplib_cq.dbinfo, true);
			bnxt_qplib_replay_db(&cq->qplib_cq.dbinfo, false);
		}
	}
	spin_unlock(&res_list->lock);

	/* ARM_ENA and Cons update DBs for Kernel SRQs */
	res_list = &rdev->res_list[BNXT_RE_RES_TYPE_SRQ];
	spin_lock(&res_list->lock);
	list_for_each_entry(srq, &res_list->head, res_list) {
		if (!srq->qplib_srq.is_user) {
			bnxt_qplib_replay_db(&srq->qplib_srq.dbinfo, true);
			bnxt_qplib_replay_db(&srq->qplib_srq.dbinfo, false);
		}
	}
	spin_unlock(&res_list->lock);

	/* QP */
	res_list = &rdev->res_list[BNXT_RE_RES_TYPE_QP];
	spin_lock(&res_list->lock);
	list_for_each_entry(qp, &res_list->head, res_list) {
		struct bnxt_qplib_q *q;
		/* Do nothing for user QPs */
		if (qp->qplib_qp.is_user)
			continue;

		/* Replay SQ */
		q = &qp->qplib_qp.sq;
		bnxt_qplib_replay_db(&q->dbinfo, false);

		/* Check if RQ exists */
		if (!qp->qplib_qp.rq.max_wqe)
			continue;

		/* Replay RQ */
		q = &qp->qplib_qp.rq;
		bnxt_qplib_replay_db(&q->dbinfo, false);
	}
	spin_unlock(&res_list->lock);

	if (!user_dbr_drop_recov)
		goto dbr_compl;

	/* Check whether all user-lands completed the recovery */
	start_time = get_jiffies_64();
	for (i = 0; i < rdev->user_dbr_drop_recov_timeout; i++) {
		user_recov_pend = 0;
		res_list = &rdev->res_list[BNXT_RE_RES_TYPE_UCTX];
		spin_lock(&res_list->lock);
		list_for_each_entry(uctx, &res_list->head, res_list) {
			uint32_t *epoch = uctx->dbr_recov_cq_page;

			if (!epoch || !uctx->dbr_recov_cq)
				continue;

			/*
			 * epoch[0] = user_epoch
			 * epoch[1] = user_epoch_ack
			 */
			if (epoch[0] != epoch[1])
				user_recov_pend++;
		}
		spin_unlock(&res_list->lock);

		if (!user_recov_pend)
			break;

		diff_time_msec = jiffies_to_msecs(get_jiffies_64() - start_time);
		if (diff_time_msec >= rdev->user_dbr_drop_recov_timeout)
			break;

		usleep_range(1000, 1500);
	}

	if (user_recov_pend) {
		rdev->dbr_sw_stats->dbr_drop_recov_timeouts++;
		rdev->dbr_sw_stats->dbr_drop_recov_timeout_users += user_recov_pend;
		dev_dbg(rdev_to_dev(rdev), "DBR recovery timeout for %d users\n", user_recov_pend);
		goto pacing_exit;
	}

dbr_compl:
	bnxt_dbr_complete(rdev->en_dev, dbr_recov_work->curr_epoch);
pacing_exit:
	rdev->dbr_recovery_on = false;
exit:
	kfree(dbr_recov_work);
}

static void bnxt_re_dbq_wq_task(struct work_struct *work)
{
	struct bnxt_re_dbq_work *dbq_work =
			container_of(work, struct bnxt_re_dbq_work, work);
	struct bnxt_re_dev *rdev;

	rdev = dbq_work->rdev;

	if (!rdev)
		goto exit;
	switch (dbq_work->event) {
	case BNXT_RE_DBQ_EVENT_SCHED:
		dev_dbg(rdev_to_dev(rdev), "%s: Handle DBQ Pacing event\n",
			__func__);
		if (!bnxt_qplib_dbr_pacing_ext_en(rdev->chip_ctx))
			bnxt_re_hwrm_dbr_pacing_broadcast_event(rdev);
		else
			bnxt_re_pacing_alert(rdev);
		break;
	case BNXT_RE_DBR_PACING_EVENT:
		dev_dbg(rdev_to_dev(rdev), "%s: Sched interrupt/pacing worker",
			__func__);
		if (bnxt_qplib_dbr_pacing_chip_p7_en(rdev->chip_ctx))
			bnxt_re_pacing_alert(rdev);
		else
			bnxt_re_hwrm_dbr_pacing_qcfg(rdev);
		break;
	case BNXT_RE_DBR_NQ_PACING_NOTIFICATION:
		if (!rdev->is_virtfn) {
			bnxt_re_handle_dbr_nq_pacing_notification(rdev);
			/* Issue a broadcast event to notify other functions
			 * that primary changed
			 */
			bnxt_re_hwrm_dbr_pacing_broadcast_event(rdev);
		}
		break;
	}
exit:
	kfree(dbq_work);
}

static void bnxt_re_udcc_task(struct work_struct *work)
{
	struct bnxt_re_udcc_work *udcc_work =
			container_of(work, struct bnxt_re_udcc_work, work);
	struct bnxt_re_dev *rdev;

	rdev = udcc_work->rdev;
	if (!rdev)
		goto exit;
	if (udcc_work->session_id >= BNXT_RE_UDCC_MAX_SESSIONS)
		goto exit;
	switch (udcc_work->session_opcode) {
	case BNXT_RE_UDCC_SESSION_CREATE:
		bnxt_re_debugfs_create_udcc_session(rdev, udcc_work->session_id);
		break;
	case BNXT_RE_UDCC_SESSION_DELETE:
		bnxt_re_debugfs_delete_udcc_session(rdev, udcc_work->session_id);
		break;
	default:
		break;
	}
exit:
	kfree(udcc_work);
}

static bool bnxt_re_is_qp1_or_shadow_qp(struct bnxt_re_dev *rdev,
					struct bnxt_re_qp *qp)
{
	if (rdev->gsi_ctx.gsi_qp_mode == BNXT_RE_GSI_MODE_ALL)
		return (qp->ib_qp.qp_type == IB_QPT_GSI) ||
			(qp == rdev->gsi_ctx.gsi_sqp);
	else
		return (qp->ib_qp.qp_type == IB_QPT_GSI);
}

/* bnxt_re_stop_user_qps_nonfatal -	Move all kernel qps to flush list
 * @rdev     -   rdma device instance
 *
 * This function will move all the kernel QPs to flush list.
 * Calling this function at appropriate interface will flush all pending
 * data path commands to be completed with FLUSH_ERR from
 * bnxt_qplib_process_flush_list poll context.
 *
 */
static void bnxt_re_stop_user_qps_nonfatal(struct bnxt_re_dev *rdev)
{
	struct bnxt_qplib_qp *qpl_qp;
	struct ib_qp_attr qp_attr;
	int num_qps_stopped = 0;
	int mask = IB_QP_STATE;
	struct bnxt_re_qp *qp;

	if (test_bit(BNXT_RE_FLAG_ERR_DEVICE_DETACHED, &rdev->flags))
		return;

restart:
	dev_dbg(rdev_to_dev(rdev), "from %s %d num_qps_stopped %d\n",
		__func__, __LINE__, num_qps_stopped);

	mutex_lock(&rdev->qp_lock);
	list_for_each_entry(qp, &rdev->qp_list, list) {
		qpl_qp = &qp->qplib_qp;
		if (!qpl_qp->is_user ||
		    bnxt_re_is_qp1_or_shadow_qp(rdev, qp) || qp->is_dv_qp)
			continue;
		/* This is required to move further in list otherwise,
		 * we will not be able to complete the list due to budget.
		 * label:restart will start iteration from head once again.
		 */
		if (qpl_qp->state == CMDQ_MODIFY_QP_NEW_STATE_RESET ||
		    qpl_qp->state == CMDQ_MODIFY_QP_NEW_STATE_ERR)
			continue;

		qp_attr.qp_state = IB_QPS_ERR;
		bnxt_re_modify_qp(&qp->ib_qp, &qp_attr, mask, NULL);

		bnxt_re_dispatch_event(&rdev->ibdev, &qp->ib_qp, 1, IB_EVENT_QP_FATAL);
		/*
		 * 1. Release qp_lock after a budget to unblock other verb
		 *    requests (like qp_destroy) from stack.
		 * 2. Traverse through the qp_list freshly as addition / deletion
		 *    might have happened since qp_lock is getting released here.
		 */
		if (++num_qps_stopped % BNXT_RE_STOP_QPS_BUDGET == 0) {
			mutex_unlock(&rdev->qp_lock);
			schedule();
			goto restart;
		}
	}
	mutex_unlock(&rdev->qp_lock);
	dev_dbg(rdev_to_dev(rdev), "from %s %d num_qps_stopped %d\n",
		__func__, __LINE__, num_qps_stopped);
}

/* bnxt_re_drain_kernel_qps_fatal -	Move all kernel qps to flush list
 * @rdev     -   rdma device instance
 *
 * This function will move all the kernel QPs to flush list.
 * Calling this function at appropriate interface will flush all pending
 * data path commands to be completed with FLUSH_ERR from
 * bnxt_qplib_process_flush_list poll context.
 *
 */
static void bnxt_re_drain_kernel_qps_fatal(struct bnxt_re_dev *rdev)
{
	struct bnxt_qplib_qp *qpl_qp;
	struct bnxt_re_qp *qp;
	unsigned long flags;

	if (!test_bit(BNXT_RE_FLAG_ERR_DEVICE_DETACHED, &rdev->flags))
		return;

	mutex_lock(&rdev->qp_lock);

	list_for_each_entry(qp, &rdev->qp_list, list) {
		qpl_qp = &qp->qplib_qp;
		qpl_qp->state =	CMDQ_MODIFY_QP_NEW_STATE_ERR;
		qpl_qp->cur_qp_state = qpl_qp->state;
		if (!qpl_qp->is_user) {
			flags = bnxt_re_lock_cqs(qp);
			bnxt_qplib_add_flush_qp(qpl_qp);
			bnxt_re_unlock_cqs(qp, flags);

			/* If we don't have any new submission from ULP,
			 * driver will not have a chance to call completion handler.
			 * If ULP has not properly handled drain_qp it can happen.
			 */
			bnxt_re_handle_cqn(&qp->scq->qplib_cq);
			bnxt_re_handle_cqn(&qp->rcq->qplib_cq);
		}
	}

	mutex_unlock(&rdev->qp_lock);
}

static void bnxt_re_aer_wq_task(struct work_struct *work)
{
	struct bnxt_re_aer_work *aer_work =
			container_of(work, struct bnxt_re_aer_work, work);
	struct bnxt_re_dev *rdev = aer_work->rdev;

	if (rdev) {
		/* If dbq interrupt is scheduled, wait for it to finish */
		while(atomic_read(&rdev->dbq_intr_running))
			usleep_range(1, 10);

		if (rdev->dbq_wq)
			flush_workqueue(rdev->dbq_wq);
		cancel_work_sync(&rdev->dbq_fifo_check_work);
		cancel_delayed_work_sync(&rdev->dbq_pacing_work);
		bnxt_re_drain_kernel_qps_fatal(aer_work->rdev);
	}

	kfree(aer_work);
}

static void bnxt_re_get_bar_maps(struct bnxt_re_dev *rdev)
{
	struct bnxt_en_dev *en_dev = rdev->en_dev;
	int i;

	if (!bnxt_qplib_peer_mmap_supported(rdev->chip_ctx))
		return;

	/* Invalidate the BAR count till new address is updated */
	atomic_set(&rdev->qplib_res.bar_cnt, 0);
	for (i = 0; i < en_dev->bar_cnt; i++) {
		rdev->qplib_res.bar_addr[i].hv_bar_addr = en_dev->bar_addr[i].hv_bar_addr;
		rdev->qplib_res.bar_addr[i].vm_bar_addr = en_dev->bar_addr[i].vm_bar_addr;
		rdev->qplib_res.bar_addr[i].bar_size = en_dev->bar_addr[i].bar_size;

		dev_dbg(rdev_to_dev(rdev),
			"HPA: 0x%llx\n", rdev->qplib_res.bar_addr[i].hv_bar_addr);
		dev_dbg(rdev_to_dev(rdev),
			"GPA: 0x%llx\n", rdev->qplib_res.bar_addr[i].vm_bar_addr);
		dev_dbg(rdev_to_dev(rdev),
			"size: 0x%llx\n", rdev->qplib_res.bar_addr[i].bar_size);
	}
	atomic_set(&rdev->qplib_res.bar_cnt, en_dev->bar_cnt);
}

static void bnxt_re_mmap_wq_task(struct work_struct *work)
{
	struct bnxt_re_dev *rdev = container_of(work, struct bnxt_re_dev,
						peer_mmap_work.work);

	dev_dbg(rdev_to_dev(rdev), "ATS Memory MAP available\n");
	bnxt_re_get_bar_maps(rdev);
}

static void bnxt_re_async_notifier(void *handle, struct hwrm_async_event_cmpl *cmpl)
{
	struct bnxt_re_en_dev_info *en_info = auxiliary_get_drvdata(handle);
	struct bnxt_re_dbr_drop_recov_work *dbr_recov_work;
	struct bnxt_re_udcc_work *udcc_work;
	struct bnxt_re_dcb_work *dcb_work;
	struct bnxt_re_dbq_work *dbq_work;
	struct bnxt_re_aer_work *aer_work;
	struct bnxt_re_dev *rdev;
	u32 err_type;
	u16 event_id;
	u32 data1;
	u32 data2;

	if (!cmpl) {
		dev_err(NULL, "Async event, bad completion\n");
		return;
	}

	rdev = en_info->rdev;
	if (!rdev) {
		dev_dbg(NULL, "Async event, bad rdev or netdev\n");
		return;
	}

	event_id = le16_to_cpu(cmpl->event_id);
	data1 = le32_to_cpu(cmpl->event_data1);
	data2 = le32_to_cpu(cmpl->event_data2);

	if (test_bit(BNXT_RE_FLAG_ERR_DEVICE_DETACHED, &rdev->flags) ||
	    !test_bit(BNXT_RE_FLAG_NETDEV_REGISTERED, &rdev->flags)) {
		dev_dbg(NULL, "Async event, device already detached\n");
		return;
	}
	dev_dbg(rdev_to_dev(rdev), "Async event_id = %d data1 = %d data2 = %d",
		event_id, data1, data2);

	switch (event_id) {
	case ASYNC_EVENT_CMPL_EVENT_ID_DCB_CONFIG_CHANGE:
		/* Not handling the event in older FWs */
		if (!is_qport_service_type_supported(rdev))
			break;
		if (!rdev->dcb_wq)
			break;
		dcb_work = kzalloc(sizeof(*dcb_work), GFP_ATOMIC);
		if (!dcb_work)
			break;

		dcb_work->rdev = rdev;
		memcpy(&dcb_work->cmpl, cmpl, sizeof(*cmpl));
		INIT_WORK(&dcb_work->work, bnxt_re_dcb_wq_task);
		queue_work(rdev->dcb_wq, &dcb_work->work);
		break;
	case ASYNC_EVENT_CMPL_EVENT_ID_RESET_NOTIFY:
		if (EVENT_DATA1_RESET_NOTIFY_FATAL(data1)) {
			/* Set rcfw flag to control commands send to Bono */
			set_bit(ERR_DEVICE_DETACHED, &rdev->rcfw.cmdq.flags);
			/* Set bnxt_re flag to control commands send via L2 driver */
			set_bit(BNXT_RE_FLAG_ERR_DEVICE_DETACHED, &rdev->flags);
			wake_up_all(&rdev->rcfw.cmdq.waitq);
			if (rdev->dbr_pacing)
				bnxt_re_set_pacing_dev_state(rdev);
		}
		if (!rdev->aer_wq)
			break;
		aer_work = kzalloc(sizeof(*aer_work), GFP_ATOMIC);
		if (!aer_work)
			break;

		aer_work->rdev = rdev;
		INIT_WORK(&aer_work->work, bnxt_re_aer_wq_task);
		queue_work(rdev->aer_wq, &aer_work->work);
		break;
	case ASYNC_EVENT_CMPL_EVENT_ID_DOORBELL_PACING_THRESHOLD:
		if (!rdev->dbr_pacing)
			break;
		dbq_work = kzalloc(sizeof(*dbq_work), GFP_ATOMIC);
		if (!dbq_work)
			goto unlock;
		dbq_work->rdev = rdev;
		dbq_work->event = BNXT_RE_DBR_PACING_EVENT;
		INIT_WORK(&dbq_work->work, bnxt_re_dbq_wq_task);
		queue_work(rdev->dbq_wq, &dbq_work->work);
		rdev->dbr_sw_stats->dbq_int_recv++;
		break;
	case ASYNC_EVENT_CMPL_EVENT_ID_DOORBELL_PACING_NQ_UPDATE:
		if (!rdev->dbr_pacing)
			break;

		dbq_work = kzalloc(sizeof(*dbq_work), GFP_ATOMIC);
		if (!dbq_work)
			goto unlock;
		dbq_work->rdev = rdev;
		dbq_work->event = BNXT_RE_DBR_NQ_PACING_NOTIFICATION;
		INIT_WORK(&dbq_work->work, bnxt_re_dbq_wq_task);
		queue_work(rdev->dbq_wq, &dbq_work->work);
		break;

	case ASYNC_EVENT_CMPL_EVENT_ID_ERROR_REPORT:
		err_type = BNXT_RE_EVENT_ERROR_REPORT_TYPE(data1);

		if (err_type == BNXT_RE_ASYNC_ERR_REP_BASE(TYPE_DOORBELL_DROP_THRESHOLD) &&
		    rdev->dbr_drop_recov) {
			rdev->dbr_sw_stats->dbr_drop_recov_events++;
			rdev->dbr_evt_curr_epoch = BNXT_RE_EVENT_DBR_EPOCH(data1);

			dbr_recov_work = kzalloc(sizeof(*dbr_recov_work), GFP_ATOMIC);
			if (!dbr_recov_work)
				goto unlock;
			dbr_recov_work->rdev = rdev;
			dbr_recov_work->curr_epoch = rdev->dbr_evt_curr_epoch;
			INIT_WORK(&dbr_recov_work->work, bnxt_re_dbr_drop_recov_task);
			queue_work(rdev->dbr_drop_recov_wq, &dbr_recov_work->work);
		}
		break;
	case ASYNC_EVENT_CMPL_EVENT_ID_UDCC_SESSION_CHANGE:
		if (!bnxt_qplib_udcc_supported(rdev->chip_ctx))
			break;
		udcc_work = kzalloc(sizeof(*udcc_work), GFP_ATOMIC);
		if (!udcc_work)
			break;
		udcc_work->session_id = BNXT_RE_EVENT_UDCC_SESSION_ID(data1);
		udcc_work->session_opcode = BNXT_RE_EVENT_UDCC_SESSION_OPCODE(data2);
		udcc_work->rdev = rdev;
		INIT_WORK(&udcc_work->work, bnxt_re_udcc_task);
		queue_work(rdev->udcc_wq, &udcc_work->work);
		break;
	case ASYNC_EVENT_CMPL_EVENT_ID_PEER_MMAP_CHANGE:
		INIT_DELAYED_WORK(&rdev->peer_mmap_work, bnxt_re_mmap_wq_task);
		schedule_delayed_work(&rdev->peer_mmap_work,
				      msecs_to_jiffies(PEER_MMAP_WORK_SCHED_DELAY));
		break;
	default:
		break;
	}
unlock:
	return;
}

static void bnxt_re_db_fifo_check(struct work_struct *work)
{
	struct bnxt_re_dev *rdev = container_of(work, struct bnxt_re_dev,
						dbq_fifo_check_work);
	struct bnxt_qplib_db_pacing_data *pacing_data;
	u32 pacing_save;

	if (!mutex_trylock(&rdev->dbq_lock))
		return;
	pacing_data = rdev->qplib_res.pacing_data;
	pacing_save = rdev->do_pacing_save;
	__wait_for_fifo_occupancy_below_th(rdev);
	cancel_delayed_work_sync(&rdev->dbq_pacing_work);
	if (rdev->dbr_recovery_on)
		goto recovery_on;
	if (pacing_save > rdev->dbr_def_do_pacing) {
		/* Double the do_pacing value during the congestion */
		pacing_save = pacing_save << 1;
	} else {
		/*
		 * when a new congestion is detected increase the do_pacing
		 * by 8 times. And also increase the pacing_th by 4 times. The
		 * reason to increase pacing_th is to give more space for the
		 * queue to oscillate down without getting empty, but also more
		 * room for the queue to increase without causing another alarm.
		 */
		pacing_save = pacing_save << 3;
		pacing_data->pacing_th = rdev->pacing_algo_th * 4;
	}

	if (pacing_save > BNXT_RE_MAX_DBR_DO_PACING)
		pacing_save = BNXT_RE_MAX_DBR_DO_PACING;

	pacing_data->do_pacing = pacing_save;
	rdev->do_pacing_save = pacing_data->do_pacing;
	pacing_data->alarm_th =
		pacing_data->pacing_th * BNXT_RE_PACING_ALARM_TH_MULTIPLE;
recovery_on:
	schedule_delayed_work(&rdev->dbq_pacing_work,
			      msecs_to_jiffies(rdev->dbq_pacing_time));
	rdev->dbr_sw_stats->dbq_pacing_alerts++;
	mutex_unlock(&rdev->dbq_lock);
}

static void bnxt_re_pacing_timer_exp(struct work_struct *work)
{
	struct bnxt_re_dev *rdev = container_of(work, struct bnxt_re_dev,
						dbq_pacing_work.work);
	struct bnxt_qplib_db_pacing_data *pacing_data;
	u32 read_val, fifo_occup;
	struct bnxt_qplib_nq *nq;

	if (!mutex_trylock(&rdev->dbq_lock))
		return;

	pacing_data = rdev->qplib_res.pacing_data;
	read_val = readl(rdev->en_dev->bar0 + rdev->dbr_db_fifo_reg_off);
	fifo_occup = pacing_data->fifo_max_depth -
		     ((read_val & pacing_data->fifo_room_mask) >>
		      pacing_data->fifo_room_shift);

	if (fifo_occup > pacing_data->pacing_th)
		goto restart_timer;

	/*
	 * Instead of immediately going back to the default do_pacing
	 * reduce it by 1/8 times and restart the timer.
	 */
	pacing_data->do_pacing = pacing_data->do_pacing - (pacing_data->do_pacing >> 3);
	pacing_data->do_pacing = max_t(u32, rdev->dbr_def_do_pacing, pacing_data->do_pacing);
	/*
	 * If the fifo_occup is less than the interrupt enable threshold
	 * enable the interrupt on the primary PF.
	 */
	if (rdev->dbq_int_disable && fifo_occup < rdev->pacing_en_int_th) {
		if (bnxt_qplib_dbr_pacing_is_primary_pf(rdev->chip_ctx)) {
			nq = &rdev->nqr->nq[0];
			bnxt_re_set_dbq_throttling_reg(rdev, nq->ring_id,
						       rdev->dbq_watermark);
			rdev->dbr_sw_stats->dbq_int_en++;
			rdev->dbq_int_disable = false;
		}
	}
	if (pacing_data->do_pacing <= rdev->dbr_def_do_pacing) {
		bnxt_re_set_default_pacing_data(rdev);
		rdev->dbr_sw_stats->dbq_pacing_complete++;
		goto dbq_unlock;
	}
restart_timer:
	schedule_delayed_work(&rdev->dbq_pacing_work,
			      msecs_to_jiffies(rdev->dbq_pacing_time));
	bnxt_re_update_do_pacing_slabs(rdev);
	rdev->dbr_sw_stats->dbq_pacing_resched++;
dbq_unlock:
	rdev->do_pacing_save = pacing_data->do_pacing;
	mutex_unlock(&rdev->dbq_lock);
}

void bnxt_re_pacing_alert(struct bnxt_re_dev *rdev)
{
	struct bnxt_qplib_db_pacing_data *pacing_data;

	if (!rdev->dbr_pacing)
		return;
	mutex_lock(&rdev->dbq_lock);
	pacing_data = rdev->qplib_res.pacing_data;

	/*
	 * Increase the alarm_th to max so that other user lib instances do not
	 * keep alerting the driver.
	 */
	pacing_data->alarm_th = pacing_data->fifo_max_depth;
	pacing_data->do_pacing = BNXT_RE_MAX_DBR_DO_PACING;
	cancel_work_sync(&rdev->dbq_fifo_check_work);
	schedule_work(&rdev->dbq_fifo_check_work);
	mutex_unlock(&rdev->dbq_lock);
}

void bnxt_re_schedule_dbq_event(struct bnxt_qplib_res *res)
{
	struct bnxt_re_dbq_work *dbq_work;
	struct bnxt_re_dev *rdev;

	rdev = container_of(res, struct bnxt_re_dev, qplib_res);

	atomic_set(&rdev->dbq_intr_running, 1);

	if (test_bit(BNXT_RE_FLAG_ERR_DEVICE_DETACHED, &rdev->flags))
		goto exit;
	/* Run the loop to send dbq event to other functions
	 * for newer FW
	 */
	if (bnxt_qplib_dbr_pacing_ext_en(rdev->chip_ctx))
		bnxt_re_set_dbq_throttling_for_non_primary(rdev);

	dbq_work = kzalloc(sizeof(*dbq_work), GFP_ATOMIC);
	if (!dbq_work)
		goto exit;
	dbq_work->rdev = rdev;
	dbq_work->event = BNXT_RE_DBQ_EVENT_SCHED;
	INIT_WORK(&dbq_work->work, bnxt_re_dbq_wq_task);
	queue_work(rdev->dbq_wq, &dbq_work->work);
	rdev->dbr_sw_stats->dbq_int_recv++;
	rdev->dbq_int_disable = true;
exit:
	atomic_set(&rdev->dbq_intr_running, 0);
}

static int bnxt_re_handle_start(struct auxiliary_device *adev)
{
	struct bnxt_re_en_dev_info *en_info = auxiliary_get_drvdata(adev);
	struct bnxt_re_en_dev_info *en_info2 = NULL;
	struct bnxt_re_bond_info *info = NULL;
	struct bnxt_re_dev *rdev = NULL;
	bool init_ib_required = true;
	struct net_device *real_dev;
	struct bnxt_en_dev *en_dev;
	struct net_device *netdev;
	bool is_primary = true;
	int rc = 0;

	netdev = en_info->en_dev->net;
	if (en_info->rdev) {
		dev_info(rdev_to_dev(en_info->rdev),
			 "%s: Device is already added adev %p rdev: %p\n",
			 __func__, adev, en_info->rdev);
		if (en_info->binfo_valid) {
			/* Decrement the reference count of bond netdev */
			dev_put(en_info->binfo.master);
		}
		return 0;
	}

	en_dev = en_info->en_dev;
	real_dev = rdma_vlan_dev_real_dev(netdev);
	if (!real_dev)
		real_dev = netdev;
	if (en_info->binfo_valid) {
		info = kzalloc(sizeof(*info), GFP_KERNEL);
		if (!info) {
			/* Decrement the reference count of bond netdev */
			dev_put(en_info->binfo.master);
			bnxt_unregister_dev(en_dev);
			clear_bit(BNXT_RE_FLAG_EN_DEV_NETDEV_REG, &en_info->flags);
			return -ENOMEM;
		}
		memcpy(info, &en_info->binfo, sizeof(*info));
		real_dev = info->master;
	}

	if (test_bit(BNXT_RE_FLAG_EN_DEV_SECONDARY_DEV, &en_info->flags))
		is_primary = false;

	rc = bnxt_re_add_device(&rdev, real_dev, info,
				en_info->gsi_mode,
				BNXT_RE_POST_RECOVERY_INIT,
				en_info->wqe_mode,
				adev, is_primary);

	if (rc) {
		/* Add device failed. Unregister the device.
		 * This has to be done explicitly as
		 * bnxt_re_stop would not have unregistered
		 */
		bnxt_unregister_dev(en_dev);
		clear_bit(BNXT_RE_FLAG_EN_DEV_NETDEV_REG, &en_info->flags);
		if (en_info->binfo_valid) {
			/* Decrement the reference count of bond netdev */
			dev_put(info->master);
			kfree(info);
			en_info->binfo_valid = false;
		}
		return rc;
	}
	rtnl_lock();
	if (en_info->binfo_valid) {
		info->rdev = rdev;
		en_info->binfo_valid = false;
		/* Decrement the reference count of bond netdev */
		dev_put(info->master);
		/* In case secondary rdev recovered first */
		if (info->aux_dev2)
			en_info2 = auxiliary_get_drvdata(info->aux_dev2);
		if (en_info2)
			info->rdev_peer = en_info2->rdev;
	} else if (test_bit(BNXT_RE_FLAG_EN_DEV_SECONDARY_DEV, &en_info->flags)) {
		/* In case primary rdev recovered first */
		info = binfo_from_slave_ndev(real_dev);
		if (info)
			info->rdev_peer = rdev;
		init_ib_required = false;
	}
	bnxt_re_get_link_speed(rdev);
	rtnl_unlock();

	if (init_ib_required) {
		rc = bnxt_re_ib_init(rdev);
		if (rc) {
			dev_err(rdev_to_dev(rdev), "Failed ib_init\n");
			return rc;
		}
		bnxt_re_ib_init_2(rdev);
	}

	/* Reset active_port_map so that worker can update f/w using SET_LINK_AGGR_MODE */
	if (rdev->binfo)
		rdev->binfo->active_port_map = 0;

	return rc;
}

static void bnxt_re_stop(struct auxiliary_device *adev)
{
	struct bnxt_re_en_dev_info *en_info = auxiliary_get_drvdata(adev);
	struct bnxt_en_dev *en_dev;
	struct bnxt_re_dev *rdev;

	mutex_lock(&bnxt_re_mutex);
	rdev = en_info->rdev;
	if (!rdev)
		goto exit;

	if (!bnxt_re_is_rdev_valid(rdev))
		goto exit;

	/*
	 * Check if fw has undergone reset or is in a fatal condition.
	 * If so, set flags so that no further commands are sent down to FW
	 */

	en_dev = rdev->en_dev;
	if (test_bit(BNXT_STATE_FW_FATAL_COND, &en_dev->en_state) ||
	    test_bit(BNXT_STATE_FW_RESET_DET, &en_dev->en_state) ||
	    test_bit(BNXT_STATE_PCI_CHANNEL_IO_FROZEN, &en_dev->en_state)) {
		/* Set rcfw flag to control commands send to Bono */
		set_bit(ERR_DEVICE_DETACHED, &rdev->rcfw.cmdq.flags);
		/* Set bnxt_re flag to control commands send via L2 driver */
		set_bit(BNXT_RE_FLAG_ERR_DEVICE_DETACHED, &rdev->flags);
		wake_up_all(&rdev->rcfw.cmdq.waitq);
		bnxt_re_dispatch_event(&rdev->ibdev, NULL, 1,
				       IB_EVENT_DEVICE_FATAL);
	}

	if (test_bit(BNXT_RE_FLAG_STOP_IN_PROGRESS, &rdev->flags))
		goto exit;
	set_bit(BNXT_RE_FLAG_STOP_IN_PROGRESS, &rdev->flags);

	en_info->wqe_mode = rdev->chip_ctx->modes.wqe_mode;
	en_info->gsi_mode = rdev->gsi_ctx.gsi_qp_mode;

	if (rdev->binfo) {
		memcpy(&en_info->binfo, rdev->binfo, sizeof(*rdev->binfo));
		en_info->binfo_valid = true;
		/* To prevent bonding driver from being unloaded,
		 * increment reference count of bond netdevice
		 */
		dev_hold(rdev->binfo->master);
	}

	if (rdev->dbr_pacing)
		bnxt_re_set_pacing_dev_state(rdev);

	dev_info(rdev_to_dev(rdev), "%s: L2 driver notified to stop en_state 0x%lx",
		 __func__, en_dev->en_state);

	if (!test_bit(BNXT_RE_FLAG_EN_DEV_SECONDARY_DEV, &en_info->flags))
		bnxt_re_ib_uninit(rdev);
	bnxt_re_remove_device(rdev, BNXT_RE_PRE_RECOVERY_REMOVE, rdev->adev);
exit:
	mutex_unlock(&bnxt_re_mutex);

	/* TODO: Handle return values when bnxt_en supports */
	return;
}

static void bnxt_re_start(struct auxiliary_device *adev)
{
	/* TODO: Handle return values
	 * when bnxt_en supports it
	 */
	mutex_lock(&bnxt_re_mutex);
	if (bnxt_re_handle_start(adev))
		dev_err(NULL, "Failed to start RoCE device");
	mutex_unlock(&bnxt_re_mutex);
	return;
}

static void bnxt_re_vf_res_config(struct bnxt_re_dev *rdev)
{
	struct bnxt_qplib_ctx *hctx;
	u32 num_vfs;

	/* For Thor2, VF creation is not dependent on LAG*/
	if (!BNXT_RE_CHIP_P7(rdev->chip_ctx->chip_num) && rdev->binfo)
		return;

	/*
	 * Use the total VF count since the actual VF count may not be
	 * available at this point.
	 */
	num_vfs = pci_sriov_get_totalvfs(rdev->en_dev->pdev);
	if (!num_vfs)
		return;

	hctx = rdev->qplib_res.hctx;
	bnxt_re_limit_vf_res(rdev, &hctx->vf_res, num_vfs);
	bnxt_qplib_set_func_resources(&rdev->qplib_res);
}

/* In kernels which has native Auxiliary bus support, auxiliary bus
 * subsystem will invoke shutdown. Else, bnxt_en driver will invoke
 * bnxt_ulp_shutdown directly.
 */
static void bnxt_re_shutdown(struct auxiliary_device *adev)
{
	struct bnxt_re_en_dev_info *en_info = auxiliary_get_drvdata(adev);
	struct bnxt_re_dev *rdev;

	mutex_lock(&bnxt_re_mutex);
	rdev = en_info->rdev;
	if (!rdev || !bnxt_re_is_rdev_valid(rdev))
		goto exit;

	bnxt_re_ib_uninit(rdev);
	bnxt_re_remove_device(rdev, BNXT_RE_COMPLETE_REMOVE, rdev->adev);
exit:
	mutex_unlock(&bnxt_re_mutex);
	return;
}

static void bnxt_re_stop_irq(void *handle, bool reset)
{
	struct bnxt_re_en_dev_info *en_info = auxiliary_get_drvdata(handle);
	struct bnxt_re_dev *rdev;

	rdev = en_info->rdev;
	if (!rdev)
		return;

	if (!test_bit(BNXT_RE_FLAG_SETUP_NQ, &rdev->flags))
		goto skip_dispatch;

	if (reset) {
		set_bit(ERR_DEVICE_DETACHED, &rdev->rcfw.cmdq.flags);
		set_bit(BNXT_RE_FLAG_ERR_DEVICE_DETACHED, &rdev->flags);
		wake_up_all(&rdev->rcfw.cmdq.waitq);
		if (rdev->dbr_pacing)
			bnxt_re_set_pacing_dev_state(rdev);
		bnxt_re_dispatch_event(&rdev->ibdev, NULL, 1,
				       IB_EVENT_DEVICE_FATAL);
	}

skip_dispatch:
	bnxt_re_dettach_irq(rdev);
}

static void bnxt_re_start_irq(void *handle, struct bnxt_msix_entry *ent)
{
	struct bnxt_re_en_dev_info *en_info = auxiliary_get_drvdata(handle);
	struct bnxt_msix_entry *msix_ent = NULL;
	struct bnxt_qplib_rcfw *rcfw = NULL;
	struct bnxt_re_dev *rdev;
	struct bnxt_qplib_nq *nq;
	int indx, rc, vec;

	rdev = en_info->rdev;
	if (!rdev)
		return;
	if (test_bit(BNXT_RE_FLAG_ERR_DEVICE_DETACHED, &rdev->flags))
		return;

	if (!test_bit(BNXT_RE_FLAG_SETUP_NQ, &rdev->flags))
		return;
	msix_ent = rdev->nqr->msix_entries;
	rcfw = &rdev->rcfw;

	if (!ent) {
		/* Not setting the f/w timeout bit in rcfw.
		 * During the driver unload the first command
		 * to f/w will timeout and that will set the
		 * timeout bit.
		 */
		dev_err(rdev_to_dev(rdev), "Failed to re-start IRQs\n");
		return;
	}

	/* Vectors may change after restart, so update with new vectors
	 * in device structure.
	 */
	for (indx = 0; indx < rdev->nqr->num_msix; indx++)
		rdev->nqr->msix_entries[indx].vector = ent[indx].vector;

	if (test_bit(BNXT_RE_FLAG_ALLOC_RCFW, &rdev->flags)) {
		rc = bnxt_qplib_rcfw_start_irq(rcfw, msix_ent[BNXT_RE_AEQ_IDX].vector,
					       false);
		if (rc) {
			dev_warn(rdev_to_dev(rdev),
				 "Failed to reinit CREQ\n");
			return;
		}
	}
	for (indx = 0 ; indx < rdev->nqr->max_init; indx++) {
		nq = &rdev->nqr->nq[indx];
		vec = indx + 1;
		rc = bnxt_qplib_nq_start_irq(nq, indx, msix_ent[vec].vector,
					     false);
		if (rc) {
			dev_warn(rdev_to_dev(rdev),
				 "Failed to reinit NQ index %d\n", indx);
			return;
		}
	}
}

static u32 bnxt_re_pbl_log(struct bnxt_re_dev *rdev,
			   struct qdump_element *element, void *buf,
			   u32 buf_len)
{
	u32 i, j, len = 0;

	for (i = 0; i <= element->level; i++) {
		len += snprintf(buf + len, buf_len - len,
				"PBL lvl %d pgsz %d count %d\n",
				i, element->pbl[i].pg_size,
				element->pbl[i].pg_count);
		if (len >= buf_len)
			return buf_len;
		for (j = 0; j < element->pbl[i].pg_count; j++) {
			len += snprintf(buf + len, buf_len - len,
					"\tidx %d addr 0x%llx\n", j,
					element->pbl[i].pg_map_arr[j]);
			if (len >= buf_len)
				return buf_len;
		}
	}
	return len;
}

static u32 bnxt_re_qp_ulp_log(struct bnxt_re_dev *rdev,
			      struct qdump_element *element, void *buf,
			      u32 buf_len)
{
	u32 i, j, offset, slots, stride, len;
	u64 *dump;

	stride = element->stride;
	slots = (element->len / stride);

	len = snprintf(buf, buf_len,
		       "****** %s : Slots %d***prod %d cons %d *******\n",
		       element->des, slots, element->prod, element->cons);
	if (len >= buf_len)
		return buf_len;
	len += bnxt_re_pbl_log(rdev, element, buf + len, buf_len - len);
	for (i = 0; i < slots; i++) {
		len += snprintf(buf + len, buf_len - len, "%02d:", i);
		if (len >= buf_len)
			return buf_len;
		offset = (i * stride);
		dump = (u64 *)&element->buf[offset];
		for (j = 0; j < (stride / sizeof(*dump)); j++) {
			if (dump[j] == 0)
				len += snprintf(buf + len, buf_len - len,
						"\t%01llx\n", dump[j]);
			else
				len += snprintf(buf + len, buf_len - len,
						"\t%016llx\n", dump[j]);
			if (len >= buf_len)
				return buf_len;
		}
	}
	return len;
}

u32 bnxt_re_dump_single_qpinfo(struct qdump_array *qdump, void *buf, u32 buf_len)
{
	struct qdump_qpinfo *qpinfo = &qdump->qpinfo;
	u32 len = 0;

	/* DONOT CHANGE THIS PRINT. ADD NEW PRINT IF REQUIRED */
	len += snprintf(buf + len, buf_len - len,
		       "qp_handle 0x%.8x 0x%.8x (xid %d) dest_qp (xid %d) state %s is_usr %d "
		       "scq_handle 0x%llx rcq_handle 0x%llx (scq_id %d) (rcq_id %d)\n",
		       (u32)(qpinfo->qp_handle >> 32),
		       (u32)(qpinfo->qp_handle & 0xFFFFFFFF),
		       qpinfo->id, qpinfo->dest_qpid,
		       __to_qp_state_str(qpinfo->state),
		       qpinfo->is_user, qpinfo->scq_handle,
		       qpinfo->rcq_handle, qpinfo->scq_id,
		       qpinfo->rcq_id);

	return len;
}

u32 bnxt_re_dump_single_qp(struct bnxt_re_dev *rdev, struct qdump_array *qdump,
			   void *buf, u32 buf_len)
{
	struct qdump_qpinfo *qpinfo = &qdump->qpinfo;
	u32 len = 0;

	len += snprintf(buf + len, buf_len - len, "\nqp_id : 0x%x (%d)\n",
			qpinfo->id, qpinfo->id);
	if (len >= buf_len)
		goto dump_full;

	len += bnxt_re_qp_ulp_log(rdev, &qdump->sqd, buf + len,
				  buf_len - len);
	if (len >= buf_len)
		goto dump_full;
	len += bnxt_re_qp_ulp_log(rdev, &qdump->rqd, buf + len,
				  buf_len - len);
	if (len >= buf_len)
		goto dump_full;

	if (qdump->scqd.len)
		len += bnxt_re_qp_ulp_log(rdev, &qdump->scqd, buf + len,
					  buf_len - len);
	else
		len += snprintf(buf + len, buf_len - len,
				"**SendCompQueue id %d handle 0x%llx dumped already!**\n",
				qpinfo->scq_id, qpinfo->scq_handle);
	if (len >= buf_len)
		goto dump_full;

	if (qdump->rcqd.len)
		len += bnxt_re_qp_ulp_log(rdev, &qdump->rcqd, buf + len,
					  buf_len - len);
	else
		len += snprintf(buf + len, buf_len - len,
				"**RecvCompQueue id %d handle 0x%llx dumped already!**\n",
				qpinfo->rcq_id, qpinfo->rcq_handle);
	if (len >= buf_len)
		goto dump_full;
	len += snprintf(buf + len, buf_len - len,
			"**************QP %d End****************************\n",
			qpinfo->id);
dump_full:
	return len;
}

/*
 * bnxt_re_snapdump_qp - Log QP dump to the coredump buffer
 * @rdev	-	Pointer to RoCE device instance
 * @buf		-	Pointer to dump buffer
 * @buf_len	-	Buffer length
 *
 * This function will invoke ULP logger to capture snapshot of
 * SQ/RQ/SCQ/RCQ of a QP in ASCII format.
 *
 * This is key data collection (smaller in size).
 *
 * Returns: Buffer length
 */
static u32 bnxt_re_snapdump_qp(struct bnxt_re_dev *rdev, void *buf, u32 buf_len)
{
	u32 index, len = 0, count = 0;
	struct qdump_array *qdump;

	if (!rdev->qdump_head.qdump)
		return 0;

	mutex_lock(&rdev->qdump_head.lock);
	/* Some device level key information */
	len += snprintf(buf + len, buf_len - len,
		       "roce_context_destroy_sb %d\n", rdev->rcfw.roce_context_destroy_sb);
	if (len >= buf_len)
		goto dump_full;

	index = rdev->qdump_head.index;
	while (count < rdev->qdump_head.max_elements) {
		count++;
		index = (!index) ? (rdev->qdump_head.max_elements - 1) : (index - 1);
		qdump = &rdev->qdump_head.qdump[index];
		if (!qdump->valid || qdump->is_mr)
			continue;

		len += bnxt_re_dump_single_qpinfo(qdump, buf + len, buf_len - len);
		if (len >= buf_len)
			goto dump_full;
	}
	mutex_unlock(&rdev->qdump_head.lock);
	return len;

dump_full:
	mutex_unlock(&rdev->qdump_head.lock);
	return buf_len;
}

/*
 * bnxt_re_snapdump_qp_ext - Log QP dump to the coredump buffer
 * @rdev	-	Pointer to RoCE device instance
 * @buf		-	Pointer to dump buffer
 * @buf_len	-	Buffer length
 *
 * This function will invoke ULP logger to capture snapshot of
 * SQ/RQ/SCQ/RCQ of a QP in ASCII format.
 *
 * This is extended data collection.
 *
 * Returns: Buffer length
 */
static u32 bnxt_re_snapdump_qp_ext(struct bnxt_re_dev *rdev, void *buf, u32 buf_len)
{
	u32 index, len = 0, count = 0;
	struct qdump_array *qdump;

	if (!rdev->qdump_head.qdump)
		return 0;

	mutex_lock(&rdev->qdump_head.lock);
	index = rdev->qdump_head.index;
	while (count < rdev->qdump_head.max_elements) {
		count++;
		index = (!index) ? (rdev->qdump_head.max_elements - 1) : (index - 1);
		qdump = &rdev->qdump_head.qdump[index];
		if (!qdump->valid || qdump->is_mr)
			continue;

		len += bnxt_re_dump_single_qp(rdev, qdump, buf + len,  buf_len - len);
		if (len >= buf_len)
			goto dump_full;
	}
	mutex_unlock(&rdev->qdump_head.lock);
	return len;

dump_full:
	mutex_unlock(&rdev->qdump_head.lock);
	return buf_len;
}

/*
 * bnxt_re_snapdump_mr - Log PBL of MR to the coredump buffer
 * @rdev	-	Pointer to RoCE device instance
 * @buf		-	Pointer to dump buffer
 * @buf_len	-	Buffer length
 *
 * This function will invoke ULP logger to capture PBL list of
 * Memory Region in ASCII format.
 *
 * Returns: Buffer length
 */
static u32 bnxt_re_snapdump_mr(struct bnxt_re_dev *rdev, void *buf, u32 buf_len)
{
	u32 index, count = 0, len = 0;
	struct qdump_array *qdump;

	if (!rdev->qdump_head.qdump)
		return 0;

	mutex_lock(&rdev->qdump_head.lock);
	index = rdev->qdump_head.index;
	while (count < rdev->qdump_head.max_elements) {
		count++;
		index = (!index) ? (rdev->qdump_head.max_elements - 1) : (index - 1);
		qdump = &rdev->qdump_head.qdump[index];
		if (!qdump->valid || !qdump->is_mr)
			continue;

		len += snprintf(buf + len, buf_len - len,
				"**MemoryRegion: type %d lkey 0x%x rkey 0x%x tot_sz %llu **\n",
				qdump->mrinfo.type, qdump->mrinfo.lkey,
				qdump->mrinfo.rkey, qdump->mrinfo.total_size);
		if (len >= buf_len)
			goto dump_full;
		len += bnxt_re_pbl_log(rdev, &qdump->mrd, buf + len,
				       buf_len - len);
		if (len >= buf_len)
			goto dump_full;
	}
	mutex_unlock(&rdev->qdump_head.lock);
	return len;

dump_full:
	mutex_unlock(&rdev->qdump_head.lock);
	return buf_len;
}

static const struct {
	long offset;
	char *str;
} bnxt_re_ext_rstat_arr[] = {
	{ offsetof(struct bnxt_re_ext_rstat, tx.atomic_req) / 8,
	  "tx_atomic_req: " },
	{ offsetof(struct bnxt_re_ext_rstat, rx.atomic_req) / 8,
	  "rx_atomic_req: " },
	{ offsetof(struct bnxt_re_ext_rstat, tx.read_req) / 8,
	  "tx_read_req: " },
	{ offsetof(struct bnxt_re_ext_rstat, tx.read_resp) / 8,
	  "tx_read_resp: " },
	{ offsetof(struct bnxt_re_ext_rstat, rx.read_req) / 8,
	  "rx_read_req: " },
	{ offsetof(struct bnxt_re_ext_rstat, rx.read_resp) / 8,
	  "rx_read_resp: " },
	{ offsetof(struct bnxt_re_ext_rstat, tx.write_req) / 8,
	  "tx_write_req: " },
	{ offsetof(struct bnxt_re_ext_rstat, rx.write_req) / 8,
	  "rx_write_req: " },
	{ offsetof(struct bnxt_re_ext_rstat, tx.send_req) / 8,
	  "tx_send_req: " },
	{ offsetof(struct bnxt_re_ext_rstat, rx.send_req) / 8,
	  "rx_send_req: " },
	{ offsetof(struct bnxt_re_ext_rstat, grx.rx_pkts) / 8,
	  "rx_good_pkts: " },
	{ offsetof(struct bnxt_re_ext_rstat, grx.rx_bytes) / 8,
	  "rx_good_bytes: " },
};

/**
 * bnxt_re_snapdump_udcc - Collect RoCE debug data for coredump.
 *
 * This function will dump UDCC sessions data to coredump
 *
 * @rdev	-   rdma device instance
 * @buf		-   Pointer to dump buffer
 * @buf_len	-   Buffer length
 *
 * Returns: Dumped length of session data
 */
static u32 bnxt_re_snapdump_udcc(struct bnxt_re_dev *rdev, void *buf, u32 buf_len)
{
	struct hwrm_struct_data_udcc_rtt_bucket_count *rtt;
	struct bnxt_re_udcc_cfg *udcc = &rdev->udcc_cfg;
	struct hwrm_udcc_session_query_output *sq_op;
	struct bnxt_re_udcc_data_head *dhead;
	struct bnxt_re_session_info *ses;
	int count = 0, index, i;
	u32 len;

	dhead = &rdev->udcc_data_head;
	if (!rdev->rcfw.roce_udcc_session_destroy_sb ||
	    !dhead->udcc_data_arr)
		return 0;

	len = snprintf(buf, buf_len,
		       "******* UDCC destroyed session data - start ********\n");
	mutex_lock(&dhead->lock);
	index = dhead->index;
	while (count < dhead->max_elements) {
		if (len >= buf_len)
			break;

		index = (!index) ? (dhead->max_elements - 1) : (index - 1);
		ses = &rdev->udcc_data_head.ses[index];
		sq_op = ses->udcc_data;
		count++;

		if (!ses->valid_session)
			continue;

		len += snprintf(buf + len, buf_len - len,
				"Session_id %u\tmin_rtt_ns %u max_rtt_ns %u"
				" cur_rate_mbps %u tx_event_count %u"
				" cnp_rx_event_count %u rtt_req_count %u"
				" rtt_resp_count %u tx_bytes_count  %u"
				" tx_packets_count  %u\n"
				"\t\tinit_probes_sent %u term_probes_recv  %u"
				" cnp_packets_recv  %u"
				" rto_event_recv %u seq_err_nak_recv %u"
				" qp_count  %u tx_event_detect_count %u\n",
				ses->session_id,
				sq_op->min_rtt_ns,
				sq_op->max_rtt_ns,
				sq_op->cur_rate_mbps,
				sq_op->tx_event_count,
				sq_op->cnp_rx_event_count,
				sq_op->rtt_req_count,
				sq_op->rtt_resp_count,
				sq_op->tx_bytes_count,
				sq_op->tx_packets_count,
				sq_op->init_probes_sent,
				sq_op->term_probes_recv,
				sq_op->cnp_packets_recv,
				sq_op->rto_event_recv,
				sq_op->seq_err_nak_recv,
				sq_op->qp_count,
				sq_op->tx_event_detect_count);

		if (!is_udcc_session_rtt_present(ses->resp_flags))
			continue;

		rtt = ses->udcc_data + (ses->udcc_sess_sz * BNXT_UDCC_SESSION_SZ_UNITS);
		for (i = 0; i < udcc->rtt_num_of_buckets; i++)
			len += snprintf(buf + len, buf_len - len,
					"\t\tRTT bucket[%d] Count :\t%u\n",
					rtt[i].bucket_idx, rtt[i].count);
	}
	mutex_unlock(&dhead->lock);

	if (len < buf_len)
		len += snprintf(buf + len, buf_len - len,
				"****** UDCC destroyed session data - end ********\n");
	return len;
}

/* bnxt_re_snapdump - Collect RoCE debug data for coredump.
 * @rdev	-   rdma device instance
 * @buf		-	Pointer to dump buffer
 * @buf_len	-	Buffer length
 *
 * This function will dump RoCE debug data to the coredump.
 *
 * Returns: Nothing
 *
 */
static void bnxt_re_snapdump(struct bnxt_re_dev *rdev, void *buf, u32 buf_len)
{
	struct bnxt_re_ext_rstat *ext_s;
	u64 *ctr;
	u32 len;
	int i;

	/* Make sure most imp data is captured before anything else */
	len = bnxt_re_snapdump_qp(rdev, buf, buf_len);
	if (len >= buf_len)
		return;

	len += bnxt_re_snapdump_udcc(rdev, buf + len, buf_len - len);
	if (len >= buf_len)
		return;

	len += bnxt_re_snapdump_qp_ext(rdev, buf + len, buf_len - len);
	if (len >= buf_len)
		return;

	len += bnxt_re_snapdump_mr(rdev, buf + len, buf_len - len);
	if (len >= buf_len)
		return;

	ext_s = &rdev->stats.dstat.ext_rstat[0];
	ctr = (u64 *)ext_s;
	for (i = 0; i < ARRAY_SIZE(bnxt_re_ext_rstat_arr); i++) {
		len += snprintf(buf + len, buf_len - len, "%s %llu\n",
				bnxt_re_ext_rstat_arr[i].str,
				*(ctr + bnxt_re_ext_rstat_arr[i].offset));
		if (len >= buf_len)
			return;
	}
}

#define BNXT_SEGMENT_ROCE	255
#define BNXT_SEGMENT_QP_CTX	256
#define BNXT_SEGMENT_SRQ_CTX	257
#define BNXT_SEGMENT_CQ_CTX	258
#define BNXT_SEGMENT_MR_CTX	270

static void bnxt_re_dump_ctx(struct bnxt_re_dev *rdev, u32 seg_id, void *buf,
			     u32 buf_len)
{
	int ctx_index, i;
	void *ctx_data;
	u16 ctx_size;

	switch (seg_id) {
	case BNXT_SEGMENT_QP_CTX:
		ctx_data = rdev->rcfw.qp_ctxm_data;
		ctx_size = rdev->rcfw.qp_ctxm_size;
		ctx_index = rdev->rcfw.qp_ctxm_data_index;
		break;
	case BNXT_SEGMENT_CQ_CTX:
		ctx_data = rdev->rcfw.cq_ctxm_data;
		ctx_size = rdev->rcfw.cq_ctxm_size;
		ctx_index = rdev->rcfw.cq_ctxm_data_index;
		break;
	case BNXT_SEGMENT_MR_CTX:
		ctx_data = rdev->rcfw.mrw_ctxm_data;
		ctx_size = rdev->rcfw.mrw_ctxm_size;
		ctx_index = rdev->rcfw.mrw_ctxm_data_index;
		break;
	case BNXT_SEGMENT_SRQ_CTX:
		ctx_data = rdev->rcfw.srq_ctxm_data;
		ctx_size = rdev->rcfw.srq_ctxm_size;
		ctx_index = rdev->rcfw.srq_ctxm_data_index;
		break;
	default:
		return;
	}

	if (!ctx_data || (ctx_size * CTXM_DATA_INDEX_MAX) > buf_len)
		return;

	for (i = ctx_index; i < CTXM_DATA_INDEX_MAX + ctx_index; i++) {
		memcpy(buf, ctx_data + ((i % CTXM_DATA_INDEX_MAX) * ctx_size),
		       ctx_size);
		buf += ctx_size;
	}
}

#define BNXT_RE_TRACE_DUMP_SIZE	0x2000000

/* bnxt_re_get_dump_info - ULP callback from L2 driver to collect dump info
 * @handle	- en_dev information. L2 and RoCE device information
 * @dump_flags	- ethtool dump flags
 * @dump	- ulp structure containing all coredump segment info
 *
 * This function is the callback from the L2 driver to provide the list of
 * dump segments for the ethtool coredump.
 *
 * Returns: Nothing
 *
 */
static void bnxt_re_get_dump_info(void *handle, u32 dump_flags,
				  struct bnxt_ulp_dump *dump)
{
	struct bnxt_re_en_dev_info *en_info = auxiliary_get_drvdata(handle);
	struct bnxt_ulp_dump_tbl *tbl = dump->seg_tbl;
	struct bnxt_re_dev *rdev = en_info->rdev;

	dump->segs = 5;
	tbl[0].seg_id = BNXT_SEGMENT_QP_CTX;
	tbl[0].seg_len = rdev->rcfw.qp_ctxm_size * CTXM_DATA_INDEX_MAX;
	tbl[1].seg_id = BNXT_SEGMENT_CQ_CTX;
	tbl[1].seg_len = rdev->rcfw.cq_ctxm_size * CTXM_DATA_INDEX_MAX;
	tbl[2].seg_id = BNXT_SEGMENT_MR_CTX;
	tbl[2].seg_len = rdev->rcfw.mrw_ctxm_size * CTXM_DATA_INDEX_MAX;
	tbl[3].seg_id = BNXT_SEGMENT_SRQ_CTX;
	tbl[3].seg_len = rdev->rcfw.srq_ctxm_size * CTXM_DATA_INDEX_MAX;
	tbl[4].seg_id = BNXT_SEGMENT_ROCE;
	tbl[4].seg_len = BNXT_RE_TRACE_DUMP_SIZE;
}

/* bnxt_re_get_dump_data - ULP callback from L2 driver to collect dump data
 * @handle	- en_dev information. L2 and RoCE device information
 * @seg_id	- segment ID of the dump
 * @buf		- dump buffer pointer
 * @len		- length of the buffer
 *
 * This function is the callback from the L2 driver to fill the buffer with
 * coredump data for each segment.
 *
 * Returns: Nothing
 *
 */
static void bnxt_re_get_dump_data(void *handle, u32 seg_id, void *buf, u32 len)
{
	struct bnxt_re_en_dev_info *en_info = auxiliary_get_drvdata(handle);
	struct bnxt_re_dev *rdev = en_info->rdev;

	if (seg_id == BNXT_SEGMENT_ROCE)
		return bnxt_re_snapdump(rdev, buf, len);

	bnxt_re_dump_ctx(rdev, seg_id, buf, len);
}

/*
 * Except for ulp_async_notifier and ulp_sriov_cfg, the remaining ulp_ops
 * below are called with rtnl_lock held
 */
static struct bnxt_ulp_ops bnxt_re_ulp_ops = {
	.ulp_async_notifier = bnxt_re_async_notifier,
	.ulp_irq_stop = bnxt_re_stop_irq,
	.ulp_irq_restart = bnxt_re_start_irq,
	.ulp_get_dump_info = bnxt_re_get_dump_info,
	.ulp_get_dump_data = bnxt_re_get_dump_data,
};

static inline const char *bnxt_re_netevent(unsigned long event)
{
	BNXT_RE_NETDEV_EVENT(event, NETDEV_UP);
	BNXT_RE_NETDEV_EVENT(event, NETDEV_DOWN);
	BNXT_RE_NETDEV_EVENT(event, NETDEV_CHANGE);
	BNXT_RE_NETDEV_EVENT(event, NETDEV_UNREGISTER);
	BNXT_RE_NETDEV_EVENT(event, NETDEV_CHANGEMTU);
	BNXT_RE_NETDEV_EVENT(event, NETDEV_CHANGEADDR);
	BNXT_RE_NETDEV_EVENT(event, NETDEV_CHANGEINFODATA);
	BNXT_RE_NETDEV_EVENT(event, NETDEV_BONDING_INFO);
	return "Unknown";
}

/* RoCE -> Net driver */

/* Driver registration routines used to let the networking driver (bnxt_en)
 * to know that the RoCE driver is now installed */

static int bnxt_re_register_netdev(struct bnxt_re_dev *rdev)
{
	struct bnxt_en_dev *en_dev = rdev->en_dev;
	int rc;

	rc = bnxt_register_dev(en_dev, &bnxt_re_ulp_ops, rdev->adev);
	if (rc)
		dev_err(rdev_to_dev(rdev),
			"Failed to register with Ethernet driver, rc = %d", rc);

	return rc;
}

static int bnxt_re_alloc_nqr_mem(struct bnxt_re_dev *rdev)
{
	rdev->nqr = kzalloc(sizeof(*rdev->nqr), GFP_KERNEL);
	if (!rdev->nqr)
		return -ENOMEM;

	return 0;
}

static void bnxt_re_free_nqr_mem(struct bnxt_re_dev *rdev)
{
	kfree(rdev->nqr);
	rdev->nqr = NULL;
}

static void bnxt_re_set_db_offset(struct bnxt_re_dev *rdev)
{
	struct bnxt_qplib_chip_ctx *cctx;
	struct bnxt_en_dev *en_dev;
	struct bnxt_qplib_res *res;

	res = &rdev->qplib_res;
	en_dev = rdev->en_dev;
	cctx = rdev->chip_ctx;

	if (_is_chip_p5_plus(cctx)) {
		res->dpi_tbl.ucreg.offset = en_dev->l2_db_offset;
		res->dpi_tbl.wcreg.offset = en_dev->l2_db_size;
	} else {
		/* L2 doesn't support db_offset value for Wh+ */
		res->dpi_tbl.ucreg.offset = res->is_vf ? BNXT_QPLIB_DBR_VF_DB_OFFSET :
							 BNXT_QPLIB_DBR_PF_DB_OFFSET;
		res->dpi_tbl.wcreg.offset = res->dpi_tbl.ucreg.offset + PAGE_SIZE;
	}
}

static void bnxt_re_set_drv_mode(struct bnxt_re_dev *rdev, u8 mode)
{
	struct bnxt_qplib_chip_ctx *cctx;
	struct bnxt_en_dev *en_dev;

	en_dev = rdev->en_dev;
	cctx = rdev->chip_ctx;
	cctx->modes.wqe_mode = _is_chip_p5_plus(rdev->chip_ctx) ?
					mode : BNXT_QPLIB_WQE_MODE_STATIC;

	dev_dbg(rdev_to_dev(rdev),
		"Configuring te_bypass mode - 0x%x",
	       ((struct bnxt_re_en_dev_info *)
		auxiliary_get_drvdata(rdev->adev))->te_bypass);

	cctx->modes.te_bypass = ((struct bnxt_re_en_dev_info *)
				 auxiliary_get_drvdata(rdev->adev))->te_bypass;

	if (_is_chip_p7_plus(rdev->chip_ctx)) {
		cctx->modes.toggle_bits |= BNXT_QPLIB_CQ_TOGGLE_BIT;
		cctx->modes.toggle_bits |= BNXT_QPLIB_SRQ_TOGGLE_BIT;
		/*
		 * In Thor+, as per HW requirement if HW LAG is enabled, TE Bypass needs
		 * to be disabled regardless of the bond device created or not.
		 */
		if (BNXT_EN_HW_LAG(rdev->en_dev))
			cctx->modes.te_bypass = 0;
	}

	if (bnxt_re_hwrm_qcaps(rdev))
		dev_err(rdev_to_dev(rdev),
			"Failed to query hwrm qcaps\n");

	rdev->roce_mode = en_dev->flags & BNXT_EN_FLAG_ROCE_CAP;
	dev_dbg(rdev_to_dev(rdev),
		"RoCE is supported on the device - caps:0x%x",
		rdev->roce_mode);
	/* For Wh+, support only RoCE v2 now */
	if (!_is_chip_p5_plus(rdev->chip_ctx))
		rdev->roce_mode = BNXT_RE_FLAG_ROCEV2_CAP;
	cctx->hw_stats_size = en_dev->hw_ring_stats_size;
	/* Enabling advanced option in cat apply by default */
	cctx->modes.cc_pr_mode = BNXT_RE_CONFIGFS_SHOW_ADV_CC_PARAMS;
}

static void bnxt_re_destroy_chip_ctx(struct bnxt_re_dev *rdev)
{
	struct bnxt_qplib_chip_ctx *chip_ctx;
	struct bnxt_qplib_res *res;

	if (!rdev->chip_ctx)
		return;

	res = &rdev->qplib_res;
	bnxt_qplib_unmap_db_bar(res);

	kfree(res->hctx);
	res->rcfw = NULL;
	kfree(rdev->dev_attr);
	rdev->dev_attr = NULL;

	chip_ctx = rdev->chip_ctx;
	rdev->chip_ctx = NULL;
	res->cctx = NULL;
	res->hctx = NULL;
	res->pdev = NULL;
	res->netdev = NULL;
	kfree(chip_ctx);
}

static int bnxt_re_setup_chip_ctx(struct bnxt_re_dev *rdev, u8 wqe_mode)
{
	struct bnxt_qplib_chip_ctx *chip_ctx;
	struct bnxt_en_dev *en_dev;
	int rc;

	en_dev = rdev->en_dev;
	/* Supply pci device to qplib */
	rdev->qplib_res.pdev = en_dev->pdev;
	rdev->qplib_res.netdev = rdev->netdev;
	rdev->qplib_res.en_dev = en_dev;

	chip_ctx = kzalloc(sizeof(*chip_ctx), GFP_KERNEL);
	if (!chip_ctx)
		return -ENOMEM;
	rdev->chip_ctx = chip_ctx;
	rdev->qplib_res.cctx = chip_ctx;
	rc = bnxt_re_query_hwrm_intf_version(rdev);
	if (rc)
		goto fail;
	rdev->dev_attr = kzalloc(sizeof(*rdev->dev_attr), GFP_KERNEL);
	if (!rdev->dev_attr) {
		rc = -ENOMEM;
		goto fail;
	}
	rdev->qplib_res.dattr = rdev->dev_attr;
	rdev->qplib_res.rcfw = &rdev->rcfw;
	rdev->qplib_res.is_vf = rdev->is_virtfn;

	rdev->qplib_res.hctx = kzalloc(sizeof(*rdev->qplib_res.hctx),
				       GFP_KERNEL);
	if (!rdev->qplib_res.hctx) {
		rc = -ENOMEM;
		goto fail;
	}
	bnxt_re_set_drv_mode(rdev, wqe_mode);

	bnxt_re_set_db_offset(rdev);
	rc = bnxt_qplib_map_db_bar(&rdev->qplib_res);
	if (rc)
		goto fail;

	rc = bnxt_qplib_enable_atomic_ops_to_root(en_dev, rdev->is_virtfn);
	if (rc)
		dev_dbg(rdev_to_dev(rdev),
			"platform doesn't support global atomics");

	return 0;
fail:
	kfree(rdev->chip_ctx);
	rdev->chip_ctx = NULL;

	kfree(rdev->dev_attr);
	rdev->dev_attr = NULL;

	kfree(rdev->qplib_res.hctx);
	rdev->qplib_res.hctx = NULL;
	return rc;
}

static u16 bnxt_re_get_rtype(struct bnxt_re_dev *rdev) {
	return _is_chip_p5_plus(rdev->chip_ctx) ?
	       RING_ALLOC_REQ_RING_TYPE_NQ :
	       RING_ALLOC_REQ_RING_TYPE_ROCE_CMPL;
}

static int bnxt_re_net_ring_free(struct bnxt_re_dev *rdev, u16 fw_ring_id)
{
	int rc = -EINVAL;
	struct hwrm_ring_free_input req = {0};
	struct hwrm_ring_free_output resp;
	struct bnxt_en_dev *en_dev = rdev->en_dev;
	struct bnxt_fw_msg fw_msg = {};

	/* To avoid unnecessary error messages during recovery.
         * HW is anyway in error state. So dont send down the command */
	if (test_bit(BNXT_RE_FLAG_ERR_DEVICE_DETACHED, &rdev->flags))
		return 0;

	/* allocation had failed, no need to issue hwrm */
	if (fw_ring_id == 0xffff)
		return 0;

	bnxt_re_init_hwrm_hdr((void *)&req, HWRM_RING_FREE, -1);
	req.ring_type = bnxt_re_get_rtype(rdev);
	req.ring_id = cpu_to_le16(fw_ring_id);
	bnxt_re_fill_fw_msg(&fw_msg, (void *)&req, sizeof(req), (void *)&resp,
			    sizeof(resp), BNXT_RE_HWRM_CMD_TIMEOUT(rdev));
	rc = bnxt_send_msg(en_dev, &fw_msg);
	if (rc) {
		dev_dbg(rdev_to_dev(rdev),
			"Failed to free HW ring with rc = 0x%x", rc);
		return rc;
	}
	dev_dbg(rdev_to_dev(rdev), "HW ring freed with id = 0x%x\n",
		fw_ring_id);

	return rc;
}

static int bnxt_re_net_ring_alloc(struct bnxt_re_dev *rdev,
				  struct bnxt_re_ring_attr *ring_attr,
				  u16 *fw_ring_id)
{
	struct bnxt_qplib_chip_ctx *cctx = rdev->chip_ctx;
	int rc = -EINVAL;
	struct hwrm_ring_alloc_input req = {0};
	struct hwrm_ring_alloc_output resp;
	struct bnxt_en_dev *en_dev = rdev->en_dev;
	struct bnxt_fw_msg fw_msg = {};

	bnxt_re_init_hwrm_hdr((void *)&req, HWRM_RING_ALLOC, -1);
	req.flags = cpu_to_le16(ring_attr->flags);
	req.enables = 0;
	req.page_tbl_addr =  cpu_to_le64(ring_attr->dma_arr[0]);
	if (ring_attr->pages > 1) {
		/* Page size is in log2 units */
		req.page_size = BNXT_PAGE_SHIFT;
		req.page_tbl_depth = 1;
	} else {
		req.page_size = 4; /* FIXME: hardconding */
		req.page_tbl_depth = 0;
	}

	if (cctx->modes.st_tag_supported) {
		req.steering_tag = cpu_to_le16(BNXT_RE_STEERING_TO_HOST);
		req.enables |= cpu_to_le32(RING_ALLOC_REQ_ENABLES_STEERING_TAG_VALID);
	}

	req.fbo = 0;
	/* Association of ring index with doorbell index and MSIX number */
	req.logical_id = cpu_to_le16(ring_attr->lrid);
	req.length = cpu_to_le32(ring_attr->depth + 1);
	req.ring_type = ring_attr->type;
	req.int_mode = ring_attr->mode;
	bnxt_re_fill_fw_msg(&fw_msg, (void *)&req, sizeof(req), (void *)&resp,
			    sizeof(resp), BNXT_RE_HWRM_CMD_TIMEOUT(rdev));
	rc = bnxt_send_msg(en_dev, &fw_msg);
	if (rc) {
		dev_dbg(rdev_to_dev(rdev),
			"Failed to allocate HW ring with rc = 0x%x", rc);
		return rc;
	}
	*fw_ring_id = le16_to_cpu(resp.ring_id);
	dev_dbg(rdev_to_dev(rdev),
		"HW ring allocated with id = 0x%x at slot 0x%x",
		resp.ring_id, ring_attr->lrid);

	return rc;
}

static int bnxt_re_net_stats_ctx_free(struct bnxt_re_dev *rdev,
				      u32 fw_stats_ctx_id, u16 tid)
{
	struct bnxt_en_dev *en_dev = rdev->en_dev;
	struct hwrm_stat_ctx_free_input req = {0};
	struct hwrm_stat_ctx_free_output resp;
	struct bnxt_fw_msg fw_msg = {};
	int rc = -EINVAL;

	/* To avoid unnecessary error messages during recovery.
         * HW is anyway in error state. So dont send down the command */
	if (test_bit(BNXT_RE_FLAG_ERR_DEVICE_DETACHED, &rdev->flags))
		return 0;
	bnxt_re_init_hwrm_hdr((void *)&req, HWRM_STAT_CTX_FREE, tid);
	req.stat_ctx_id = cpu_to_le32(fw_stats_ctx_id);
	bnxt_re_fill_fw_msg(&fw_msg, (void *)&req, sizeof(req), (void *)&resp,
			    sizeof(resp), BNXT_RE_HWRM_CMD_TIMEOUT(rdev));
	rc = bnxt_send_msg(en_dev, &fw_msg);
	if (rc) {
		dev_dbg(rdev_to_dev(rdev),
			"Failed to free HW stats ctx with rc = 0x%x", rc);
		return rc;
	}
	dev_dbg(rdev_to_dev(rdev),
		"HW stats ctx freed with id = 0x%x", fw_stats_ctx_id);

	return rc;
}

static int bnxt_re_net_stats_ctx_alloc(struct bnxt_re_dev *rdev, u16 tid,
				       struct bnxt_qplib_stats *stat)
{
	struct bnxt_qplib_chip_ctx *cctx = rdev->chip_ctx;
	struct hwrm_stat_ctx_alloc_output resp = {};
	struct hwrm_stat_ctx_alloc_input req = {};
	struct bnxt_en_dev *en_dev = rdev->en_dev;
	struct bnxt_fw_msg fw_msg = {};
	struct bnxt_qplib_ctx *hctx;
	int rc = 0;

	hctx = rdev->qplib_res.hctx;
	stat->fw_id = INVALID_STATS_CTX_ID;

	bnxt_re_init_hwrm_hdr((void *)&req, HWRM_STAT_CTX_ALLOC, tid);
	req.update_period_ms = cpu_to_le32(1000);
	req.stats_dma_length = rdev->chip_ctx->hw_stats_size;
	req.stats_dma_addr = cpu_to_le64(stat->dma_handle);
	req.stat_ctx_flags = STAT_CTX_ALLOC_REQ_STAT_CTX_FLAGS_ROCE;

	if (cctx->modes.st_tag_supported) {
		req.steering_tag = cpu_to_le16(BNXT_RE_STEERING_TO_HOST);
		req.flags |= cpu_to_le32(STAT_CTX_ALLOC_REQ_FLAGS_STEERING_TAG_VALID);
	}

	bnxt_re_fill_fw_msg(&fw_msg, (void *)&req, sizeof(req), (void *)&resp,
			    sizeof(resp), BNXT_RE_HWRM_CMD_TIMEOUT(rdev));
	rc = bnxt_send_msg(en_dev, &fw_msg);
	if (rc) {
		dev_dbg(rdev_to_dev(rdev),
			"Failed to allocate HW stats ctx, rc = 0x%x", rc);
		return rc;
	}
	stat->fw_id = le32_to_cpu(resp.stat_ctx_id);
	dev_dbg(rdev_to_dev(rdev), "HW stats ctx allocated with id = 0x%x",
		stat->fw_id);

	return rc;
}

static void bnxt_re_net_unregister_async_event(struct bnxt_re_dev *rdev)
{
	struct bnxt_re_en_dev_info *en_info;
	u32 *event_bitmap;

	if (test_bit(BNXT_RE_FLAG_ERR_DEVICE_DETACHED, &rdev->flags))
		return;

	en_info = auxiliary_get_drvdata(rdev->adev);

	event_bitmap = en_info->event_bitmap;
	memset(event_bitmap, 0, sizeof(en_info->event_bitmap));

	bnxt_register_async_events(rdev->en_dev, (unsigned long *)event_bitmap,
				   ASYNC_EVENT_CMPL_EVENT_ID_PEER_MMAP_CHANGE);
}

static void bnxt_re_net_register_async_event(struct bnxt_re_dev *rdev)
{
	struct bnxt_re_en_dev_info *en_info;
	u32 *event_bitmap;

	en_info = auxiliary_get_drvdata(rdev->adev);

	event_bitmap = en_info->event_bitmap;

	if (rdev->is_virtfn) {
		event_bitmap[0] |= BIT(ASYNC_EVENT_CMPL_EVENT_ID_DCB_CONFIG_CHANGE) |
				   BIT(ASYNC_EVENT_CMPL_EVENT_ID_RESET_NOTIFY);
		event_bitmap[2] |= BIT(ASYNC_EVENT_CMPL_EVENT_ID_ERROR_REPORT - 64) |
				   BIT(ASYNC_EVENT_CMPL_EVENT_ID_UDCC_SESSION_CHANGE - 64);

		bnxt_register_async_events(rdev->en_dev, (unsigned long *)event_bitmap,
					   ASYNC_EVENT_CMPL_EVENT_ID_UDCC_SESSION_CHANGE);
		return;
	}

	event_bitmap[0] |= BIT(ASYNC_EVENT_CMPL_EVENT_ID_DCB_CONFIG_CHANGE) |
			   BIT(ASYNC_EVENT_CMPL_EVENT_ID_RESET_NOTIFY);
	event_bitmap[2] |= BIT(ASYNC_EVENT_CMPL_EVENT_ID_ERROR_REPORT - 64) |
			   BIT(ASYNC_EVENT_CMPL_EVENT_ID_DOORBELL_PACING_THRESHOLD - 64) |
			   BIT(ASYNC_EVENT_CMPL_EVENT_ID_DOORBELL_PACING_NQ_UPDATE - 64) |
			   BIT(ASYNC_EVENT_CMPL_EVENT_ID_UDCC_SESSION_CHANGE - 64) |
			   BIT(ASYNC_EVENT_CMPL_EVENT_ID_PEER_MMAP_CHANGE - 64);

	bnxt_register_async_events(rdev->en_dev, (unsigned long *)event_bitmap,
				   ASYNC_EVENT_CMPL_EVENT_ID_PEER_MMAP_CHANGE);
}

static int bnxt_re_query_hwrm_intf_version(struct bnxt_re_dev *rdev)
{
	struct bnxt_en_dev *en_dev = rdev->en_dev;
	struct hwrm_ver_get_output resp = {0};
	struct hwrm_ver_get_input req = {0};
	struct bnxt_qplib_chip_ctx *cctx;
	struct bnxt_fw_msg fw_msg = {};
	int rc = 0;

	bnxt_re_init_hwrm_hdr((void *)&req, HWRM_VER_GET, -1);
	req.hwrm_intf_maj = HWRM_VERSION_MAJOR;
	req.hwrm_intf_min = HWRM_VERSION_MINOR;
	req.hwrm_intf_upd = HWRM_VERSION_UPDATE;
	bnxt_re_fill_fw_msg(&fw_msg, (void *)&req, sizeof(req), (void *)&resp,
			    sizeof(resp), DFLT_HWRM_CMD_TIMEOUT);
	rc = bnxt_send_msg(en_dev, &fw_msg);
	if (rc) {
		dev_dbg(rdev_to_dev(rdev),
			"Failed to query HW version, rc = 0x%x", rc);
		return rc;
	}
	cctx = rdev->chip_ctx;
	cctx->hwrm_intf_ver = (u64) le16_to_cpu(resp.hwrm_intf_major) << 48 |
			      (u64) le16_to_cpu(resp.hwrm_intf_minor) << 32 |
			      (u64) le16_to_cpu(resp.hwrm_intf_build) << 16 |
				    le16_to_cpu(resp.hwrm_intf_patch);

	cctx->hwrm_cmd_max_timeout = le16_to_cpu(resp.max_req_timeout);

	if (!cctx->hwrm_cmd_max_timeout)
		cctx->hwrm_cmd_max_timeout = RCFW_FW_STALL_MAX_TIMEOUT;

	cctx->chip_num = le16_to_cpu(resp.chip_num);
	cctx->chip_rev = resp.chip_rev;
	cctx->chip_metal = resp.chip_metal;
	return 0;
}

void bnxt_re_hwrm_free_vnic(struct bnxt_re_dev *rdev, u16 vnic_id)
{
	struct bnxt_en_dev *en_dev = rdev->en_dev;
	struct hwrm_vnic_free_input req = {};
	struct bnxt_fw_msg fw_msg = {};
	int rc;

	bnxt_re_init_hwrm_hdr((void *)&req, HWRM_VNIC_FREE, -1);

	req.vnic_id = cpu_to_le32(vnic_id);
	bnxt_re_fill_fw_msg(&fw_msg, (void *)&req, sizeof(req), NULL,
			    0, BNXT_RE_HWRM_CMD_TIMEOUT(rdev));
	rc = bnxt_send_msg(en_dev, &fw_msg);
	if (rc)
		dev_dbg(rdev_to_dev(rdev),
			"Failed to free vnic %d, rc = %d\n", vnic_id, rc);
}

int bnxt_re_hwrm_alloc_vnic(struct bnxt_re_dev *rdev, u16 *vnic_id, bool is_vnic_valid)
{
	struct bnxt_en_dev *en_dev = rdev->en_dev;
	struct hwrm_vnic_alloc_output resp = {};
	struct hwrm_vnic_alloc_input req = {};
	struct bnxt_fw_msg fw_msg = {};
	int rc;

	bnxt_re_init_hwrm_hdr((void *)&req, HWRM_VNIC_ALLOC, -1);

	if (is_vnic_valid) {
		req.vnic_id = cpu_to_le16(*vnic_id);
		req.flags = cpu_to_le32(VNIC_ALLOC_REQ_FLAGS_VNIC_ID_VALID);
	}

	bnxt_re_fill_fw_msg(&fw_msg, (void *)&req, sizeof(req), (void *)&resp,
			    sizeof(resp), BNXT_RE_HWRM_CMD_TIMEOUT(rdev));
	rc = bnxt_send_msg(en_dev, &fw_msg);
	if (rc)
		dev_dbg(rdev_to_dev(rdev),
			"Failed to alloc vnic %d, rc = %d\n", *vnic_id, rc);
	else
		*vnic_id = le16_to_cpu(resp.vnic_id);

	return rc;
}

int bnxt_re_hwrm_cfg_vnic(struct bnxt_re_dev *rdev, u32 qp_id, u16 vnic_id)
{
	struct bnxt_en_dev *en_dev = rdev->en_dev;
	struct hwrm_vnic_cfg_input req = {};
	struct bnxt_fw_msg fw_msg = {};
	int rc;

	bnxt_re_init_hwrm_hdr((void *)&req, HWRM_VNIC_CFG, -1);

	req.flags = cpu_to_le32(VNIC_CFG_REQ_FLAGS_ROCE_ONLY_VNIC_MODE);
	req.enables = cpu_to_le32(VNIC_CFG_REQ_ENABLES_QP_ID | VNIC_CFG_REQ_ENABLES_MRU);
	req.vnic_id = cpu_to_le16(vnic_id);
	req.qp_id = cpu_to_le32(qp_id);
	req.mru = cpu_to_le16(rdev->netdev->mtu + VLAN_ETH_HLEN);

	bnxt_re_fill_fw_msg(&fw_msg, (void *)&req, sizeof(req), NULL,
			    0, BNXT_RE_HWRM_CMD_TIMEOUT(rdev));
	rc = bnxt_send_msg(en_dev, &fw_msg);
	if (rc)
		dev_dbg(rdev_to_dev(rdev),
			"Failed to cfg vnic, rc = %d\n", rc);

	return rc;
}

int bnxt_re_hwrm_qcfg(struct bnxt_re_dev *rdev)
{
	struct bnxt_en_dev *en_dev = rdev->en_dev;
	struct hwrm_func_qcfg_output resp = {};
	struct hwrm_func_qcfg_input req = {};
	struct bnxt_fw_msg fw_msg = {};
	int rc;

	bnxt_re_init_hwrm_hdr((void *)&req, HWRM_FUNC_QCFG, -1);
	req.fid = cpu_to_le16(0xffff);
	bnxt_re_fill_fw_msg(&fw_msg, (void *)&req, sizeof(req), (void *)&resp,
			    sizeof(resp), BNXT_RE_HWRM_CMD_TIMEOUT(rdev));
	rc = bnxt_send_msg(en_dev, &fw_msg);
	if (rc) {
		dev_dbg(rdev_to_dev(rdev),
			"Failed to query func cfg, rc = %d\n", rc);
		return rc;
	}

	rdev->mirror_vnic_id = le16_to_cpu(resp.mirror_vnic_id);

	return rc;
}

/* Query function capabilities using common hwrm */
int bnxt_re_hwrm_qcaps(struct bnxt_re_dev *rdev)
{
	u32 flags, flags_ext, flags_ext2, flags_ext3;
	struct bnxt_en_dev *en_dev = rdev->en_dev;
	struct hwrm_func_qcaps_output resp = {0};
	struct hwrm_func_qcaps_input req = {0};
	struct bnxt_qplib_chip_ctx *cctx;
	struct bnxt_fw_msg fw_msg = {};
	int rc;

	cctx = rdev->chip_ctx;
	bnxt_re_init_hwrm_hdr((void *)&req, HWRM_FUNC_QCAPS, -1);
	req.fid = cpu_to_le16(0xffff);
	bnxt_re_fill_fw_msg(&fw_msg, (void *)&req, sizeof(req), (void *)&resp,
			    sizeof(resp), BNXT_RE_HWRM_CMD_TIMEOUT(rdev));
	rc = bnxt_send_msg(en_dev, &fw_msg);
	if (rc) {
		dev_dbg(rdev_to_dev(rdev),
			"Failed to query capabilities, rc = %#x", rc);
		return rc;
	}

	flags = le32_to_cpu(resp.flags);
	flags_ext = le32_to_cpu(resp.flags_ext);
	flags_ext2 = le32_to_cpu(resp.flags_ext2);
	flags_ext3 = le32_to_cpu(resp.flags_ext3);
	/*
	 * Check if WCB push enabled for Thor.
	 * For Thor2 and future chips, push mode enablement check is through
	 * RoCE specific device attribute query.
	 */
	if (flags & FUNC_QCAPS_RESP_FLAGS_WCB_PUSH_MODE)
		cctx->modes.db_push_mode = BNXT_RE_PUSH_MODE_WCB;
	cctx->modes.dbr_pacing =
		!!(flags_ext & FUNC_QCAPS_RESP_FLAGS_EXT_DBR_PACING_SUPPORTED);
	cctx->modes.dbr_pacing_ext =
		!!(flags_ext2 &
		   FUNC_QCAPS_RESP_FLAGS_EXT2_DBR_PACING_EXT_SUPPORTED);
	cctx->modes.dbr_drop_recov =
		!!(flags_ext2 &
		   FUNC_QCAPS_RESP_FLAGS_EXT2_SW_DBR_DROP_RECOVERY_SUPPORTED);
	cctx->modes.dbr_pacing_chip_p7 =
		!!(flags_ext2 &
		   FUNC_QCAPS_RESP_FLAGS_EXT2_DBR_PACING_V0_SUPPORTED);
	cctx->modes.udcc_supported =
		!!(flags_ext2 &
		   FUNC_QCAPS_RESP_FLAGS_EXT2_UDCC_SUPPORTED);
	cctx->modes.multiple_llq =
		!!(flags_ext2 &
		   FUNC_QCAPS_RESP_FLAGS_EXT2_MULTI_LOSSLESS_QUEUES_SUPPORTED);
	cctx->modes.roce_mirror =
		!!(flags_ext3 &
		   FUNC_QCAPS_RESP_FLAGS_EXT3_MIRROR_ON_ROCE_SUPPORTED);

	cctx->modes.st_tag_supported =
		!!(flags_ext2 &
		   FUNC_QCAPS_RESP_FLAGS_EXT2_STEERING_TAG_SUPPORTED);
	cctx->modes.gen_udp_src_supported =
		!!(flags_ext3 &
		   FUNC_QCAPS_RESP_FLAGS_EXT3_CHANGE_UDP_SRCPORT_SUPPORT);
	cctx->modes.secure_ats_supported =
		!!(flags_ext3 &
		   FUNC_QCAPS_RESP_FLAGS_EXT3_PCIE_SECURE_ATS_SUPPORTED);
	cctx->modes.peer_mmap_supported =
		!!(flags_ext2 &
		   FUNC_QCAPS_RESP_FLAGS_EXT2_PEER_MMAP_SUPPORTED);

	dev_dbg(rdev_to_dev(rdev),
		"%s: cctx->modes dbr_pacing = %d dbr_pacing_ext = %d udcc = %d roce_mirror %d "
		"gen_udp_src_supported %d\n",
		__func__, cctx->modes.dbr_pacing, cctx->modes.dbr_pacing_ext,
		cctx->modes.udcc_supported, cctx->modes.roce_mirror,
		cctx->modes.gen_udp_src_supported);
	return 0;
}

static int bnxt_re_hwrm_dbr_pacing_qcfg(struct bnxt_re_dev *rdev)
{
	struct bnxt_qplib_db_pacing_data *pacing_data = rdev->qplib_res.pacing_data;
	struct hwrm_func_dbr_pacing_qcfg_output resp = {0};
	struct hwrm_func_dbr_pacing_qcfg_input req = {0};
	struct bnxt_en_dev *en_dev = rdev->en_dev;
	struct bnxt_qplib_chip_ctx *cctx;
	struct bnxt_fw_msg fw_msg = {};
	u32 primary_nq_id;
	int rc;

	cctx = rdev->chip_ctx;
	bnxt_re_init_hwrm_hdr((void *)&req, HWRM_FUNC_DBR_PACING_QCFG, -1);
	bnxt_re_fill_fw_msg(&fw_msg, (void *)&req, sizeof(req), (void *)&resp,
			    sizeof(resp), BNXT_RE_HWRM_CMD_TIMEOUT(rdev));
	rc = bnxt_send_msg(en_dev, &fw_msg);
	if (rc) {
		dev_dbg(rdev_to_dev(rdev),
			"Failed to query dbr pacing config, rc = %#x", rc);
		return rc;
	}

	if (!rdev->is_virtfn) {
		primary_nq_id = le32_to_cpu(resp.primary_nq_id);
		if (primary_nq_id == 0xffffffff &&
		    !bnxt_qplib_dbr_pacing_ext_en(rdev->chip_ctx))
			bnxt_qplib_dbr_pacing_set_primary_pf(rdev->chip_ctx, 1);

		if (bnxt_qplib_dbr_pacing_ext_en(rdev->chip_ctx)) {
			struct bnxt_qplib_nq *nq;

			nq = &rdev->nqr->nq[0];
			/* Reset the primary capability */
			if (nq->ring_id != primary_nq_id)
				bnxt_qplib_dbr_pacing_set_primary_pf(rdev->chip_ctx, 0);
		}

		if ((resp.dbr_throttling_aeq_arm_reg &
		     FUNC_DBR_PACING_QCFG_RESP_DBR_THROTTLING_AEQ_ARM_REG_ADDR_SPACE_MASK) ==
		    FUNC_DBR_PACING_QCFG_RESP_DBR_THROTTLING_AEQ_ARM_REG_ADDR_SPACE_GRC) {
			cctx->dbr_aeq_arm_reg = resp.dbr_throttling_aeq_arm_reg &
				~FUNC_DBR_PACING_QCFG_RESP_DBR_STAT_DB_FIFO_REG_ADDR_SPACE_MASK;
			cctx->dbr_throttling_reg = cctx->dbr_aeq_arm_reg - 4;
		}
	}

	if ((resp.dbr_stat_db_fifo_reg &
	     FUNC_DBR_PACING_QCFG_RESP_DBR_STAT_DB_FIFO_REG_ADDR_SPACE_MASK) ==
	    FUNC_DBR_PACING_QCFG_RESP_DBR_STAT_DB_FIFO_REG_ADDR_SPACE_GRC)
		cctx->dbr_stat_db_fifo =
		resp.dbr_stat_db_fifo_reg &
		~FUNC_DBR_PACING_QCFG_RESP_DBR_STAT_DB_FIFO_REG_ADDR_SPACE_MASK;

	pacing_data->fifo_max_depth = le32_to_cpu(resp.dbr_stat_db_max_fifo_depth);
	if (!pacing_data->fifo_max_depth)
		pacing_data->fifo_max_depth = BNXT_RE_MAX_FIFO_DEPTH(cctx);
	pacing_data->fifo_room_mask = le32_to_cpu(resp.dbr_stat_db_fifo_reg_fifo_room_mask);
	pacing_data->fifo_room_shift = resp.dbr_stat_db_fifo_reg_fifo_room_shift;
	dev_dbg(rdev_to_dev(rdev),
		"%s: nq:0x%x primary_pf:%d db_fifo:0x%x aeq_arm:0x%x",
		__func__, resp.primary_nq_id, cctx->modes.dbr_primary_pf,
		 cctx->dbr_stat_db_fifo, cctx->dbr_aeq_arm_reg);
	return 0;
}

static int bnxt_re_hwrm_dbr_pacing_cfg(struct bnxt_re_dev *rdev, bool enable)
{
	struct hwrm_func_dbr_pacing_cfg_output resp = {0};
	struct hwrm_func_dbr_pacing_cfg_input req = {0};
	struct bnxt_en_dev *en_dev = rdev->en_dev;
	struct bnxt_fw_msg fw_msg = {};
	int rc;

	if (test_bit(BNXT_RE_FLAG_ERR_DEVICE_DETACHED, &rdev->flags))
		return 0;

	bnxt_re_init_hwrm_hdr((void *)&req, HWRM_FUNC_DBR_PACING_CFG, -1);
	if (enable) {
		req.flags = FUNC_DBR_PACING_CFG_REQ_FLAGS_DBR_NQ_EVENT_ENABLE;
		req.enables =
		cpu_to_le32(FUNC_DBR_PACING_CFG_REQ_ENABLES_PRIMARY_NQ_ID_VALID |
			    FUNC_DBR_PACING_CFG_REQ_ENABLES_PACING_THRESHOLD_VALID);
	} else {
		req.flags = FUNC_DBR_PACING_CFG_REQ_FLAGS_DBR_NQ_EVENT_DISABLE;
	}
	req.primary_nq_id = cpu_to_le32(rdev->dbq_nq_id);
	req.pacing_threshold = cpu_to_le32(rdev->dbq_watermark);
	dev_dbg(rdev_to_dev(rdev), "%s: nq_id = 0x%x pacing_threshold = 0x%x",
		__func__, req.primary_nq_id, req.pacing_threshold);
	bnxt_re_fill_fw_msg(&fw_msg, (void *)&req, sizeof(req), (void *)&resp,
			    sizeof(resp), BNXT_RE_HWRM_CMD_TIMEOUT(rdev));
	rc = bnxt_send_msg(en_dev, &fw_msg);
	if (rc) {
		dev_err(rdev_to_dev(rdev),
			"Failed to set dbr pacing config, rc = %#x", rc);
		return rc;
	}
	return 0;
}

/* Net -> RoCE driver */

/* Device */
struct bnxt_re_dev *bnxt_re_from_netdev(struct net_device *netdev)
{
	struct bnxt_re_dev *rdev;

	rcu_read_lock();
	list_for_each_entry_rcu(rdev, &bnxt_re_dev_list, list) {
		if (rdev->netdev == netdev) {
			rcu_read_unlock();
			dev_dbg(rdev_to_dev(rdev),
				"netdev (%p) found, ref_count = 0x%x",
				netdev, atomic_read(&rdev->ref_count));
			return rdev;
		}
		if (rdev->binfo) {
			if (rdev->binfo->slave1 == netdev ||
			    rdev->binfo->slave2 == netdev) {
				rcu_read_unlock();
				dev_dbg(rdev_to_dev(rdev),
					"Bond netdev (%p) found, ref_count = 0x%x",
					netdev, atomic_read(&rdev->ref_count));
				return rdev;
			}
		}
	}
	rcu_read_unlock();
	return NULL;
}

int bnxt_re_read_context_allowed(struct bnxt_re_dev *rdev)
{
	if (!_is_chip_p5_plus(rdev->chip_ctx) ||
	    rdev->rcfw.res->cctx->hwrm_intf_ver < HWRM_VERSION_READ_CTX)
		return -EOPNOTSUPP;
	return 0;
}

#ifdef HAVE_RDMA_RESTRACK_OPS
static int bnxt_re_fill_res_mr_entry(struct sk_buff *msg, struct ib_mr *ib_mr)
{
	struct bnxt_qplib_hwq *mr_hwq;
	struct nlattr *table_attr;
	struct bnxt_re_mr *mr;

	table_attr = nla_nest_start(msg, RDMA_NLDEV_ATTR_DRIVER);
	if (!table_attr)
		return -EMSGSIZE;

	mr = to_bnxt_re(ib_mr, struct bnxt_re_mr, ib_mr);
	mr_hwq = &mr->qplib_mr.hwq;

	if (rdma_nl_put_driver_string(msg, "owner",
				      mr_hwq->is_user ?  "user" : "kernel"))
		goto err;
	if (rdma_nl_put_driver_u32(msg, "page_size",
				   mr_hwq->qe_ppg * mr_hwq->element_size))
		goto err;
	if (rdma_nl_put_driver_u32(msg, "max_elements", mr_hwq->max_elements))
		goto err;
	if (rdma_nl_put_driver_u32(msg, "element_size", mr_hwq->element_size))
		goto err;
	if (rdma_nl_put_driver_u64_hex(msg, "hwq", (unsigned long)mr_hwq))
		goto err;
	if (rdma_nl_put_driver_u64_hex(msg, "va", mr->qplib_mr.va))
		goto err;

	nla_nest_end(msg, table_attr);
	return 0;

err:
	nla_nest_cancel(msg, table_attr);
	return -EMSGSIZE;
}

static int bnxt_re_fill_res_mr_entry_raw(struct sk_buff *msg, struct ib_mr *ib_mr)
{
	struct bnxt_re_dev *rdev;
	struct bnxt_re_mr *mr;
	int err, len;
	void *data;

	mr = to_bnxt_re(ib_mr, struct bnxt_re_mr, ib_mr);
	rdev = mr->rdev;

	err = bnxt_re_read_context_allowed(rdev);
	if (err)
		return err;

	len = _is_chip_p7_plus(rdev->chip_ctx) ? BNXT_RE_CONTEXT_TYPE_MRW_SIZE_P7 :
						 BNXT_RE_CONTEXT_TYPE_MRW_SIZE_P5;
	data = kzalloc(len, GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	err = bnxt_qplib_read_context(&rdev->rcfw, CMDQ_READ_CONTEXT_TYPE_MRW,
				      mr->qplib_mr.lkey, len, data);
	if (!err)
		err = nla_put(msg, RDMA_NLDEV_ATTR_RES_RAW, len, data);

	kfree(data);
	return err;
}

static int bnxt_re_fill_res_cq_entry(struct sk_buff *msg, struct ib_cq *ib_cq)
{
	struct bnxt_qplib_hwq *cq_hwq;
	struct nlattr *table_attr;
	struct bnxt_re_cq *cq;

	cq = to_bnxt_re(ib_cq, struct bnxt_re_cq, ib_cq);
	cq_hwq = &cq->qplib_cq.hwq;

	table_attr = nla_nest_start(msg, RDMA_NLDEV_ATTR_DRIVER);
	if (!table_attr)
		return -EMSGSIZE;

	if (cq->is_dbr_soft_cq) {
		if (rdma_nl_put_driver_string(msg, "owner", "user (dbr)"))
			goto err;
		nla_nest_end(msg, table_attr);
		return 0;
	}

	if (rdma_nl_put_driver_u32(msg, "nq_budget", cq->qplib_cq.nq->budget))
		goto err;
	if (rdma_nl_put_driver_u32(msg, "nq_msix_vec", cq->qplib_cq.nq->msix_vec))
		goto err;
	if (rdma_nl_put_driver_u32(msg, "cnq_events", cq->qplib_cq.cnq_events))
		goto err;
	if (rdma_nl_put_driver_u32(msg, "cq_id", cq->qplib_cq.id))
		goto err;
	if (rdma_nl_put_driver_u32(msg, "cq_depth", cq_hwq->depth))
		goto err;
	if (rdma_nl_put_driver_u32(msg, "max_elements", cq_hwq->max_elements))
		goto err;
	if (rdma_nl_put_driver_u32(msg, "element_size", cq_hwq->element_size))
		goto err;
	if (rdma_nl_put_driver_u32(msg, "max_wqe", cq->qplib_cq.max_wqe))
		goto err;

	nla_nest_end(msg, table_attr);
	return 0;

err:
	nla_nest_cancel(msg, table_attr);
	return -EMSGSIZE;
}

static int bnxt_re_fill_res_cq_entry_raw(struct sk_buff *msg, struct ib_cq *ib_cq)
{
	struct bnxt_re_dev *rdev;
	int err, len, len_extra;
	struct bnxt_re_cq *cq;
	void *data;

	cq = to_bnxt_re(ib_cq, struct bnxt_re_cq, ib_cq);
	rdev = cq->rdev;

	err = bnxt_re_read_context_allowed(rdev);
	if (err)
		return err;

	len = _is_chip_p7_plus(rdev->chip_ctx) ? BNXT_RE_CONTEXT_TYPE_CQ_SIZE_P7 :
						 BNXT_RE_CONTEXT_TYPE_CQ_SIZE_P5;

	/* Extra data is added by driver to get additional info */
	len_extra = sizeof(u32 *);

	data = kzalloc(len + len_extra, GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	if (!cq->is_dbr_soft_cq)
		err = bnxt_qplib_read_context(&rdev->rcfw,
					      CMDQ_READ_CONTEXT_TYPE_CQ,
					      cq->qplib_cq.id, len,
					      data + len_extra);
	else
		dev_dbg(rdev_to_dev(rdev), "cq id %d is driver internal soft cq\n",
			cq->qplib_cq.id);

	*((u32 *)data) = cq->qplib_cq.id;

	if (!err)
		err = nla_put(msg, RDMA_NLDEV_ATTR_RES_RAW, len, data);

	kfree(data);
	return err;
}

static int bnxt_re_fill_res_qp_entry(struct sk_buff *msg, struct ib_qp *ib_qp)
{
	struct bnxt_qplib_qp *qplib_qp;
	struct nlattr *table_attr;
	struct bnxt_re_qp *qp;

	table_attr = nla_nest_start(msg, RDMA_NLDEV_ATTR_DRIVER);
	if (!table_attr)
		return -EMSGSIZE;

	qp = to_bnxt_re(ib_qp, struct bnxt_re_qp, ib_qp);
	qplib_qp = &qp->qplib_qp;

	if (rdma_nl_put_driver_string(msg, "owner",
				      qplib_qp->is_user ?  "user" : "kernel"))
		goto err;

	if (rdma_nl_put_driver_u32(msg, "sq_max_wqe", qplib_qp->sq.max_wqe))
		goto err;
	if (rdma_nl_put_driver_u32(msg, "sq_max_sge", qplib_qp->sq.max_sge))
		goto err;
	if (rdma_nl_put_driver_u32(msg, "sq_wqe_size", qplib_qp->sq.wqe_size))
		goto err;
	if (rdma_nl_put_driver_u32(msg, "sq_swq_start", qplib_qp->sq.swq_start))
		goto err;
	if (rdma_nl_put_driver_u32(msg, "sq_swq_last", qplib_qp->sq.swq_last))
		goto err;
	if (rdma_nl_put_driver_u32(msg, "rq_max_wqe", qplib_qp->rq.max_wqe))
		goto err;
	if (rdma_nl_put_driver_u32(msg, "rq_max_sge", qplib_qp->rq.max_sge))
		goto err;
	if (rdma_nl_put_driver_u32(msg, "rq_wqe_size", qplib_qp->rq.wqe_size))
		goto err;
	if (rdma_nl_put_driver_u32(msg, "rq_swq_start", qplib_qp->rq.swq_start))
		goto err;
	if (rdma_nl_put_driver_u32(msg, "rq_swq_last", qplib_qp->rq.swq_last))
		goto err;
	if (rdma_nl_put_driver_u32(msg, "timeout", qplib_qp->timeout))
		goto err;

	nla_nest_end(msg, table_attr);
	return 0;

err:
	nla_nest_cancel(msg, table_attr);
	return -EMSGSIZE;
}

static int bnxt_re_fill_res_qp_entry_raw(struct sk_buff *msg, struct ib_qp *ibqp)
{
	struct bnxt_re_dev *rdev = to_bnxt_re_dev(ibqp->device, ibdev);
	int err, len, len_extra;
	void *data;

	err = bnxt_re_read_context_allowed(rdev);
	if (err)
		return err;

	len = _is_chip_p7_plus(rdev->chip_ctx) ? BNXT_RE_CONTEXT_TYPE_QPC_SIZE_P7 :
						 BNXT_RE_CONTEXT_TYPE_QPC_SIZE_P5;
	/* Extra data is added by driver to get additional info */
	len_extra = sizeof(u32 *);

	data = kzalloc(len + len_extra, GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	err = bnxt_qplib_read_context(&rdev->rcfw, CMDQ_READ_CONTEXT_TYPE_QPC,
				      ibqp->qp_num, len, data + len_extra);
	*((u32 *)data) = ibqp->qp_num;

	if (!err)
		err = nla_put(msg, RDMA_NLDEV_ATTR_RES_RAW, len, data);

	kfree(data);
	return err;
}

#ifdef HAVE_IB_RES_SRQ_ENTRY
static int bnxt_re_fill_res_srq_entry(struct sk_buff *msg, struct ib_srq *ib_srq)
{
	struct nlattr *table_attr;
	struct bnxt_re_srq *srq;

	table_attr = nla_nest_start(msg, RDMA_NLDEV_ATTR_DRIVER);
	if (!table_attr)
		return -EMSGSIZE;

	srq = to_bnxt_re(ib_srq, struct bnxt_re_srq, ib_srq);

	if (rdma_nl_put_driver_u32_hex(msg, "wqe_size", srq->qplib_srq.wqe_size))
		goto err;
	if (rdma_nl_put_driver_u32_hex(msg, "max_wqe", srq->qplib_srq.max_wqe))
		goto err;
	if (rdma_nl_put_driver_u32_hex(msg, "max_sge", srq->qplib_srq.max_sge))
		goto err;
	if (rdma_nl_put_driver_u32_hex(msg, "srq_limit", srq->qplib_srq.srq_limit_drv))
		goto err;

	nla_nest_end(msg, table_attr);
	return 0;

err:
	nla_nest_cancel(msg, table_attr);
	return -EMSGSIZE;
}

#ifdef HAVE_IB_RES_SRQ_ENTRY_RAW
static int bnxt_re_fill_res_srq_entry_raw(struct sk_buff *msg, struct ib_srq *ib_srq)
{
	struct bnxt_re_dev *rdev;
	struct bnxt_re_srq *srq;
	int err, len;
	void *data;

	srq = to_bnxt_re(ib_srq, struct bnxt_re_srq, ib_srq);
	rdev = srq->rdev;

	err = bnxt_re_read_context_allowed(rdev);
	if (err)
		return err;

	len = _is_chip_p7_plus(rdev->chip_ctx) ? BNXT_RE_CONTEXT_TYPE_SRQ_SIZE_P7 :
						 BNXT_RE_CONTEXT_TYPE_SRQ_SIZE_P5;

	data = kzalloc(len, GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	err = bnxt_qplib_read_context(&rdev->rcfw, CMDQ_READ_CONTEXT_TYPE_SRQ,
				      srq->qplib_srq.id, len, data);
	if (!err)
		err = nla_put(msg, RDMA_NLDEV_ATTR_RES_RAW, len, data);

	kfree(data);
	return err;
}
#endif /*HAVE_IB_RES_SRQ_ENTRY_RAW*/
#endif /*HAVE_IB_RES_SRQ_ENTRY*/
#endif /*HAVE_RDMA_RESTRACK_OPS*/

#ifdef HAVE_IB_SET_DEV_OPS
static const struct ib_device_ops bnxt_re_dev_ops = {
#ifdef HAVE_IB_OWNER_IN_DEVICE_OPS
	.owner			= THIS_MODULE,
	.driver_id 		= RDMA_DRIVER_BNXT_RE,

#ifdef HAVE_UAPI_DEF
	.uverbs_abi_ver		= BNXT_RE_ABI_VERSION_UVERBS_IOCTL,
#else
	.uverbs_abi_ver		= BNXT_RE_ABI_VERSION,
#endif

#endif
	.query_device		= bnxt_re_query_device,
	.modify_device		= bnxt_re_modify_device,

	.query_port		= bnxt_re_query_port,
#ifdef HAVE_IB_GET_PORT_IMMUTABLE
	.get_port_immutable	= bnxt_re_get_port_immutable,
#endif
#ifdef HAVE_IB_GET_DEV_FW_STR
	.get_dev_fw_str		= bnxt_re_query_fw_str,
#endif
	.query_pkey		= bnxt_re_query_pkey,
	.query_gid		= bnxt_re_query_gid,
#ifdef HAVE_IB_GET_NETDEV
	.get_netdev		= bnxt_re_get_netdev,
#endif
#ifdef HAVE_IB_ADD_DEL_GID
	.add_gid		= bnxt_re_add_gid,
	.del_gid		= bnxt_re_del_gid,
#endif
#ifdef HAVE_IB_MODIFY_GID
	.modify_gid		= bnxt_re_modify_gid,
#endif
	.get_link_layer		= bnxt_re_get_link_layer,

	.alloc_pd		= bnxt_re_alloc_pd,
	.dealloc_pd		= bnxt_re_dealloc_pd,

	.create_ah		= bnxt_re_create_ah,
#ifdef HAVE_IB_CREATE_USER_AH
	.create_user_ah		= bnxt_re_create_ah,
#endif
	.query_ah		= bnxt_re_query_ah,
	.destroy_ah		= bnxt_re_destroy_ah,

	.create_srq		= bnxt_re_create_srq,
	.modify_srq		= bnxt_re_modify_srq,
	.query_srq		= bnxt_re_query_srq,
	.destroy_srq		= bnxt_re_destroy_srq,
	.post_srq_recv		= bnxt_re_post_srq_recv,

	.create_qp		= bnxt_re_create_qp,
	.modify_qp		= bnxt_re_modify_qp,
	.query_qp		= bnxt_re_query_qp,
	.destroy_qp		= bnxt_re_destroy_qp,

	.create_flow		= bnxt_re_create_flow,
	.destroy_flow		= bnxt_re_destroy_flow,

	.post_send		= bnxt_re_post_send,
	.post_recv		= bnxt_re_post_recv,

	.create_cq		= bnxt_re_create_cq,
	.modify_cq		= bnxt_re_modify_cq,	/* Need ? */
	.destroy_cq		= bnxt_re_destroy_cq,
	.resize_cq		= bnxt_re_resize_cq,
	.poll_cq		= bnxt_re_poll_cq,
	.req_notify_cq		= bnxt_re_req_notify_cq,

	.get_dma_mr		= bnxt_re_get_dma_mr,
	.dereg_mr		= bnxt_re_dereg_mr,
#ifdef HAVE_IB_ALLOC_MR
	.alloc_mr		= bnxt_re_alloc_mr,
#endif
#ifdef HAVE_IB_MAP_MR_SG
	.map_mr_sg		= bnxt_re_map_mr_sg,
#endif
	.alloc_mw		= bnxt_re_alloc_mw,
	.dealloc_mw		= bnxt_re_dealloc_mw,
	.reg_user_mr		= bnxt_re_reg_user_mr,
#ifdef HAVE_IB_UMEM_DMABUF
	.reg_user_mr_dmabuf     = bnxt_re_reg_user_mr_dmabuf,
#endif
#ifdef HAVE_IB_REREG_USER_MR
	/*
	 * TODO: Workaround to mask an issue rereg_user_mr handler
	 * .rereg_user_mr		= bnxt_re_rereg_user_mr,
	 */
#endif
#ifdef HAVE_DISASSOCIATE_UCNTX
	/*
	 * Have to be populated for both old and new kernels
	 * to allow rmmoding driver with app running
	 */
	.disassociate_ucontext	= bnxt_re_disassociate_ucntx,
#endif
	.alloc_ucontext		= bnxt_re_alloc_ucontext,
	.dealloc_ucontext	= bnxt_re_dealloc_ucontext,
	.mmap			= bnxt_re_mmap,
#ifdef HAVE_RDMA_XARRAY_MMAP_V2
	.mmap_free		= bnxt_re_mmap_free,
#endif
	.process_mad		= bnxt_re_process_mad,
#ifdef HAVE_AH_ALLOC_IN_IB_CORE
	INIT_RDMA_OBJ_SIZE(ib_ah, bnxt_re_ah, ib_ah),
#endif
#ifdef HAVE_PD_ALLOC_IN_IB_CORE
        INIT_RDMA_OBJ_SIZE(ib_pd, bnxt_re_pd, ib_pd),
#endif
#ifdef HAVE_SRQ_CREATE_IN_IB_CORE
        INIT_RDMA_OBJ_SIZE(ib_srq, bnxt_re_srq, ib_srq),
#endif
#ifdef HAVE_CQ_ALLOC_IN_IB_CORE
	INIT_RDMA_OBJ_SIZE(ib_cq, bnxt_re_cq, ib_cq),
#endif
#ifdef HAVE_QP_ALLOC_IN_IB_CORE
	INIT_RDMA_OBJ_SIZE(ib_qp, bnxt_re_qp, ib_qp),
#endif
#ifdef HAVE_UCONTEXT_ALLOC_IN_IB_CORE
        INIT_RDMA_OBJ_SIZE(ib_ucontext, bnxt_re_ucontext, ib_uctx),
#endif
#ifdef HAVE_ALLOC_HW_PORT_STATS
	.alloc_hw_port_stats	= bnxt_re_alloc_hw_port_stats,
#else
	.alloc_hw_stats		= bnxt_re_alloc_hw_stats,
#endif
	.get_hw_stats		= bnxt_re_get_hw_stats,
};

#ifdef HAVE_RDMA_RESTRACK_OPS
static const struct ib_device_ops restrack_ops = {
	.fill_res_cq_entry = bnxt_re_fill_res_cq_entry,
	.fill_res_cq_entry_raw = bnxt_re_fill_res_cq_entry_raw,
	.fill_res_qp_entry = bnxt_re_fill_res_qp_entry,
	.fill_res_qp_entry_raw = bnxt_re_fill_res_qp_entry_raw,
	.fill_res_mr_entry = bnxt_re_fill_res_mr_entry,
	.fill_res_mr_entry_raw = bnxt_re_fill_res_mr_entry_raw,
#ifdef HAVE_IB_RES_SRQ_ENTRY
	.fill_res_srq_entry = bnxt_re_fill_res_srq_entry,
#ifdef HAVE_IB_RES_SRQ_ENTRY_RAW
	.fill_res_srq_entry_raw = bnxt_re_fill_res_srq_entry_raw,
#endif
#endif
};
#endif

#endif /* HAVE_IB_SET_DEV_OPS */
#ifdef HAVE_RDMA_SET_DEVICE_SYSFS_GROUP
static ssize_t hw_rev_show(struct device *device, struct device_attribute *attr,
			   char *buf)
{
	struct bnxt_re_dev *rdev = to_bnxt_re_dev(device, ibdev.dev);

#ifdef HAS_SYSFS_EMIT
	return sysfs_emit(buf, "0x%x\n", rdev->en_dev->pdev->revision);
#else
	return scnprintf(buf, PAGE_SIZE, "0x%x\n", rdev->en_dev->pdev->revision);
#endif
}
static DEVICE_ATTR_RO(hw_rev);

static ssize_t hca_type_show(struct device *device,
			     struct device_attribute *attr, char *buf)
{
	struct bnxt_re_dev *rdev = to_bnxt_re_dev(device, ibdev.dev);

#ifdef HAS_SYSFS_EMIT
	return sysfs_emit(buf, "0x%x\n", rdev->en_dev->pdev->device);
#else
	return scnprintf(buf, PAGE_SIZE, "0x%x\n", rdev->en_dev->pdev->device);
#endif
}
static DEVICE_ATTR_RO(hca_type);

static ssize_t board_id_show(struct device *device,
			     struct device_attribute *attr, char *buf)
{
	struct bnxt_re_dev *rdev = to_bnxt_re_dev(device, ibdev.dev);
	char buffer[BNXT_VPD_PN_FLD_LEN] = {};

	if (!rdev->is_virtfn)
		memcpy(buffer, rdev->en_dev->board_part_number,
		       BNXT_VPD_PN_FLD_LEN - 1);
	else
		scnprintf(buffer, BNXT_VPD_PN_FLD_LEN,
			  "0x%x-VF", rdev->en_dev->pdev->device);

#ifdef HAS_SYSFS_EMIT
	return sysfs_emit(buf, "%s\n", buffer);
#else
	return scnprintf(buf, PAGE_SIZE, "%s\n", buffer);
#endif
}
static DEVICE_ATTR_RO(board_id);

static struct attribute *bnxt_re_attributes[] = {
	&dev_attr_hw_rev.attr,
	&dev_attr_hca_type.attr,
	&dev_attr_board_id.attr,
	NULL
};
static const struct attribute_group bnxt_re_dev_attr_group = {
        .attrs = bnxt_re_attributes,
};
#else
static ssize_t show_rev(struct device *device, struct device_attribute *attr,
			char *buf)
{
	struct bnxt_re_dev *rdev = to_bnxt_re_dev(device, ibdev.dev);

	return scnprintf(buf, PAGE_SIZE, "0x%x\n", rdev->en_dev->pdev->revision);
}


static ssize_t show_hca(struct device *device, struct device_attribute *attr,
			char *buf)
{
	struct bnxt_re_dev *rdev = to_bnxt_re_dev(device, ibdev.dev);

	return scnprintf(buf, PAGE_SIZE, "0x%x\n", rdev->en_dev->pdev->device);
}

static ssize_t show_board_id(struct device *device, struct device_attribute *attr,
			     char *buf)
{
	struct bnxt_re_dev *rdev = to_bnxt_re_dev(device, ibdev.dev);
	char buffer[BNXT_VPD_PN_FLD_LEN] = {};

	if (!rdev->is_virtfn)
		memcpy(buffer, rdev->en_dev->board_part_number,
		       BNXT_VPD_PN_FLD_LEN - 1);
	else
		scnprintf(buffer, BNXT_VPD_PN_FLD_LEN,
			  "0x%x-VF", rdev->en_dev->pdev->device);

	return scnprintf(buf, PAGE_SIZE, "%s\n", buffer);
}

#ifndef HAVE_IB_GET_DEV_FW_STR
static ssize_t show_fw_ver(struct device *device, struct device_attribute *attr,
			   char *buf)
{
	struct bnxt_re_dev *rdev = to_bnxt_re_dev(device, ibdev.dev);

	return scnprintf(buf, PAGE_SIZE, "%d.%d.%d.%d\n",
			 rdev->dev_attr->fw_ver[0], rdev->dev_attr->fw_ver[1],
			 rdev->dev_attr->fw_ver[2], rdev->dev_attr->fw_ver[3]);
}

static DEVICE_ATTR(fw_rev, 0444, show_fw_ver, NULL);
#endif
static DEVICE_ATTR(hw_rev, 0444, show_rev, NULL);
static DEVICE_ATTR(hca_type, 0444, show_hca, NULL);
static DEVICE_ATTR(board_id, 0444, show_board_id, NULL);

static struct device_attribute *bnxt_re_attributes[] = {
	&dev_attr_hw_rev,
#ifndef HAVE_IB_GET_DEV_FW_STR
	&dev_attr_fw_rev,
#endif
	&dev_attr_hca_type,
	&dev_attr_board_id
};
#endif

static int bnxt_re_register_ib(struct bnxt_re_dev *rdev)
{
	struct ib_device *ibdev = &rdev->ibdev;
	int ret = 0;

	/* ib device init */
#ifndef HAVE_IB_OWNER_IN_DEVICE_OPS
	ibdev->owner = THIS_MODULE;
#ifdef HAVE_UAPI_DEF
	ibdev->uverbs_abi_ver = BNXT_RE_ABI_VERSION_UVERBS_IOCTL;
#else
	ibdev->uverbs_abi_ver = BNXT_RE_ABI_VERSION;
#endif
#ifdef HAVE_RDMA_DRIVER_ID
	ibdev->driver_id = RDMA_DRIVER_BNXT_RE;
#endif
#endif
	ibdev->node_type = RDMA_NODE_IB_CA;
	strscpy(ibdev->node_desc, BNXT_RE_DESC " HCA",
		strlen(BNXT_RE_DESC) + 5);
	ibdev->phys_port_cnt = 1;

	addrconf_addr_eui48((u8 *)&ibdev->node_guid, rdev->netdev->dev_addr);

	ibdev->num_comp_vectors	= rdev->nqr->max_init;
	bnxt_re_set_dma_device(ibdev, rdev);
	/*
	 * P8 devices do not support reserved local key. Do not advertise
	 * reserved local key for p8 devices. This allows the IB stack to cover
	 * the kernel address space with MR.
	 */
	if (!_is_chip_p8(rdev->chip_ctx))
		ibdev->local_dma_lkey = BNXT_QPLIB_RSVD_LKEY;

#ifdef HAVE_IB_UVERBS_CMD_MASK_IN_DRIVER
	/* User space */
	ibdev->uverbs_cmd_mask =
			(1ull << IB_USER_VERBS_CMD_GET_CONTEXT)		|
			(1ull << IB_USER_VERBS_CMD_QUERY_DEVICE)	|
			(1ull << IB_USER_VERBS_CMD_QUERY_PORT)		|
			(1ull << IB_USER_VERBS_CMD_ALLOC_PD)		|
			(1ull << IB_USER_VERBS_CMD_DEALLOC_PD)		|
			(1ull << IB_USER_VERBS_CMD_REG_MR)		|
			(1ull << IB_USER_VERBS_CMD_DEREG_MR)		|
			(1ull << IB_USER_VERBS_CMD_CREATE_COMP_CHANNEL) |
			(1ull << IB_USER_VERBS_CMD_CREATE_CQ)		|
			(1ull << IB_USER_VERBS_CMD_DESTROY_CQ)		|
			(1ull << IB_USER_VERBS_CMD_CREATE_QP)		|
			(1ull << IB_USER_VERBS_CMD_MODIFY_QP)		|
			(1ull << IB_USER_VERBS_CMD_QUERY_QP)		|
			(1ull << IB_USER_VERBS_CMD_DESTROY_QP)		|
			/*
			 * (1ull << IB_USER_VERBS_CMD_REREG_MR)		|
			 */
			(1ull << IB_USER_VERBS_CMD_RESIZE_CQ)		|
			(1ull << IB_USER_VERBS_CMD_CREATE_SRQ)		|
			(1ull << IB_USER_VERBS_CMD_MODIFY_SRQ)		|
			(1ull << IB_USER_VERBS_CMD_QUERY_SRQ)		|
			(1ull << IB_USER_VERBS_CMD_DESTROY_SRQ)		|
			(1ull << IB_USER_VERBS_CMD_ALLOC_MW)		|
			(1ull << IB_USER_VERBS_CMD_DEALLOC_MW)		|
			(1ull << IB_USER_VERBS_CMD_CREATE_AH)		|
			(1ull << IB_USER_VERBS_CMD_MODIFY_AH)		|
			(1ull << IB_USER_VERBS_CMD_QUERY_AH)		|
			(1ull << IB_USER_VERBS_CMD_DESTROY_AH);

#ifdef HAVE_IB_USER_VERBS_EX_CMD_MODIFY_QP
	ibdev->uverbs_ex_cmd_mask = (1ull << IB_USER_VERBS_EX_CMD_MODIFY_QP);
#endif
#endif
	ibdev->uverbs_cmd_mask |= (1ull << IB_USER_VERBS_CMD_POLL_CQ);

	/* REQ_NOTIFY_CQ is directly handled in libbnxt_re.
	 * POLL_CQ is processed only as part of a RESIZE_CQ operation;
	 * the library uses this to let the kernel driver know that
	 * RESIZE_CQ is complete and memory from the previous CQ can be
	 * unmapped.
	 */
#ifdef HAVE_RDMA_SET_DEVICE_SYSFS_GROUP
	rdma_set_device_sysfs_group(ibdev, &bnxt_re_dev_attr_group);
#endif
#ifdef HAVE_IB_DEVICE_SET_NETDEV
	ret = ib_device_set_netdev(&rdev->ibdev, rdev->netdev, 1);
        if (ret)
                return ret;
#endif

#ifdef HAVE_IB_SET_DEV_OPS
	ib_set_device_ops(ibdev, &bnxt_re_dev_ops);
#ifdef HAVE_RDMA_RESTRACK_OPS
	ib_set_device_ops(ibdev, &restrack_ops);
#endif
#else
	/* Kernel verbs */
	ibdev->query_device		= bnxt_re_query_device;
	ibdev->modify_device		= bnxt_re_modify_device;

	ibdev->query_port		= bnxt_re_query_port;
#ifdef HAVE_IB_GET_PORT_IMMUTABLE
	ibdev->get_port_immutable	= bnxt_re_get_port_immutable;
#endif
#ifdef HAVE_IB_GET_DEV_FW_STR
	ibdev->get_dev_fw_str		= bnxt_re_query_fw_str;
#endif
	ibdev->query_pkey		= bnxt_re_query_pkey;
	ibdev->query_gid		= bnxt_re_query_gid;
#ifdef HAVE_IB_GET_NETDEV
	ibdev->get_netdev		= bnxt_re_get_netdev;
#endif
#ifdef HAVE_IB_ADD_DEL_GID
	ibdev->add_gid			= bnxt_re_add_gid;
	ibdev->del_gid			= bnxt_re_del_gid;
#endif
#ifdef HAVE_IB_MODIFY_GID
	ibdev->modify_gid		= bnxt_re_modify_gid;
#endif
	ibdev->get_link_layer		= bnxt_re_get_link_layer;

	ibdev->alloc_pd			= bnxt_re_alloc_pd;
	ibdev->dealloc_pd		= bnxt_re_dealloc_pd;

	ibdev->create_ah		= bnxt_re_create_ah;
	ibdev->query_ah			= bnxt_re_query_ah;
	ibdev->destroy_ah		= bnxt_re_destroy_ah;

	ibdev->create_srq		= bnxt_re_create_srq;
	ibdev->modify_srq		= bnxt_re_modify_srq;
	ibdev->query_srq		= bnxt_re_query_srq;
	ibdev->destroy_srq		= bnxt_re_destroy_srq;
	ibdev->post_srq_recv		= bnxt_re_post_srq_recv;

	ibdev->create_qp		= bnxt_re_create_qp;
	ibdev->modify_qp		= bnxt_re_modify_qp;
	ibdev->query_qp			= bnxt_re_query_qp;
	ibdev->destroy_qp		= bnxt_re_destroy_qp;

	ibdev->create_flow		= bnxt_re_create_flow,
	ibdev->destroy_flow		= bnxt_re_destroy_flow,

	ibdev->post_send		= bnxt_re_post_send;
	ibdev->post_recv		= bnxt_re_post_recv;

	ibdev->create_cq		= bnxt_re_create_cq;
	ibdev->modify_cq		= bnxt_re_modify_cq;	/* Need ? */
	ibdev->destroy_cq		= bnxt_re_destroy_cq;
	ibdev->resize_cq		= bnxt_re_resize_cq;
	ibdev->poll_cq			= bnxt_re_poll_cq;
	ibdev->req_notify_cq		= bnxt_re_req_notify_cq;

	ibdev->get_dma_mr		= bnxt_re_get_dma_mr;
	ibdev->dereg_mr			= bnxt_re_dereg_mr;
#ifdef HAVE_IB_ALLOC_MR
	ibdev->alloc_mr			= bnxt_re_alloc_mr;
#endif
#ifdef HAVE_IB_MAP_MR_SG
	ibdev->map_mr_sg		= bnxt_re_map_mr_sg;
#endif
	ibdev->alloc_mw			= bnxt_re_alloc_mw;
	ibdev->dealloc_mw		= bnxt_re_dealloc_mw;

	ibdev->reg_user_mr		= bnxt_re_reg_user_mr;
#ifdef HAVE_IB_UMEM_DMABUF
	ibdev->reg_user_mr_dmabuf       = bnxt_re_reg_user_mr_dmabuf;
#endif
#ifdef HAVE_IB_REREG_USER_MR
	/*
	 * TODO: Workaround to mask an issue in rereg_user_mr handler
	 * ibdev->rereg_user_mr		= bnxt_re_rereg_user_mr;
	 */
#endif
#ifdef HAVE_DISASSOCIATE_UCNTX
	ibdev->disassociate_ucontext	= bnxt_re_disassociate_ucntx;
#endif
	ibdev->alloc_ucontext		= bnxt_re_alloc_ucontext;
	ibdev->dealloc_ucontext		= bnxt_re_dealloc_ucontext;
	ibdev->mmap			= bnxt_re_mmap;
	ibdev->process_mad		= bnxt_re_process_mad;
#endif /* HAVE_IB_SET_DEV_OPS */

#ifndef HAVE_IB_ALLOC_MR
	/* TODO: Workaround to uninitialized the kobj */
	ibdev->dev.kobj.state_initialized = 0;
#endif
#ifdef HAVE_UAPI_DEF
	if (IS_ENABLED(CONFIG_INFINIBAND_USER_ACCESS))
		ibdev->driver_def = bnxt_re_uapi_defs;
#endif
	ret = ib_register_device_compat(rdev);
	return ret;
}

static void bnxt_re_dev_dealloc(struct bnxt_re_dev *rdev)
{
	int i = BNXT_RE_REF_WAIT_COUNT;

	dev_dbg(rdev_to_dev(rdev), "%s:Remove the device %p\n", __func__, rdev);
	/* Wait for rdev refcount to come down */
	while ((atomic_read(&rdev->ref_count) > 1) && i--)
		msleep(100);

	if (atomic_read(&rdev->ref_count) > 1)
		dev_err(rdev_to_dev(rdev),
			"Failed waiting for ref count to deplete %d",
			atomic_read(&rdev->ref_count));

	atomic_set(&rdev->ref_count, 0);
	dev_put(rdev->netdev);
	rdev->netdev = NULL;
	if (rdev->binfo) {
		kfree(rdev->binfo);
		rdev->binfo = NULL;
	}
	synchronize_rcu();

#ifdef RDMA_CORE_CAP_PROT_ROCE_UDP_ENCAP
	kfree(rdev->gid_map);
#endif
	kfree(rdev->dbg_stats);
	kfree(rdev->d2p);
	kfree(rdev->lossless_qid);
	ib_dealloc_device(&rdev->ibdev);
}

static void bnxt_re_res_list_init(struct bnxt_re_dev *rdev)
{
	struct bnxt_re_res_list *res;
	int i;

	for (i = 0; i < BNXT_RE_RES_TYPE_MAX; i++) {
		res = &rdev->res_list[i];

		INIT_LIST_HEAD(&res->head);
		spin_lock_init(&res->lock);
	}
}

static struct bnxt_re_dev *bnxt_re_dev_alloc(struct net_device *netdev,
					   struct bnxt_en_dev *en_dev)
{
	struct bnxt_re_dev *rdev;
#ifdef RDMA_CORE_CAP_PROT_ROCE_UDP_ENCAP
	u32 count;
#endif
	/* Allocate bnxt_re_dev instance here */
	rdev = (struct bnxt_re_dev *)compat_ib_alloc_device(sizeof(*rdev));
	if (!rdev) {
		dev_err(NULL, "%s: bnxt_re_dev allocation failure!",
			ROCE_DRV_MODULE_NAME);
		return NULL;
	}
	/* Default values */
	atomic_set(&rdev->ref_count, 0);
	refcount_set(&rdev->pos_destah_cnt, 1);
	rdev->netdev = netdev;
	dev_hold(rdev->netdev);
	rdev->en_dev = en_dev;
	INIT_LIST_HEAD(&rdev->qp_list);
	mutex_init(&rdev->qp_lock);
	mutex_init(&rdev->cc_lock);
	mutex_init(&rdev->dbq_lock);
	bnxt_re_clear_rsors_stat(&rdev->stats.rsors);
	rdev->cosq[0] = rdev->cosq[1] = 0xFFFF;
	rdev->min_tx_depth = 1;
	rdev->stats.stats_query_sec = 1;
	/* Disable priority vlan as the default mode is DSCP based PFC */
	rdev->cc_param.disable_prio_vlan_tx = 1;
	rdev->ugid_index = BNXT_RE_INVALID_UGID_INDEX;

	/* Initialize worker for DBR Pacing */
	INIT_WORK(&rdev->dbq_fifo_check_work, bnxt_re_db_fifo_check);
	INIT_DELAYED_WORK(&rdev->dbq_pacing_work, bnxt_re_pacing_timer_exp);
#ifdef RDMA_CORE_CAP_PROT_ROCE_UDP_ENCAP
	rdev->gid_map = kzalloc(sizeof(*(rdev->gid_map)) *
				  BNXT_RE_MAX_SGID_ENTRIES,
				  GFP_KERNEL);
	if (!rdev->gid_map)
		goto free_ibdev;
	for(count = 0; count < BNXT_RE_MAX_SGID_ENTRIES; count++)
		rdev->gid_map[count] = -1;
#endif /* RDMA_CORE_CAP_PROT_ROCE_UDP_ENCAP */
	rdev->dbg_stats = kzalloc(sizeof(*rdev->dbg_stats), GFP_KERNEL);
	if (!rdev->dbg_stats)
		goto free_ibdev;
	rdev->d2p = kcalloc(MAX_DSCP_PRI_TUPLE, sizeof(*(rdev->d2p)), GFP_KERNEL);
	if (!rdev->d2p)
		goto free_dbg_stats;
	rdev->lossless_qid = kcalloc(MAX_LOSS_LESS_QUEUES, sizeof(*(rdev->lossless_qid)),
				     GFP_KERNEL);
	if (!rdev->lossless_qid)
		goto free_d2p;
	bnxt_re_res_list_init(rdev);
	rdev->cq_coalescing.enable = 1;
	rdev->cq_coalescing.buf_maxtime = BNXT_QPLIB_CQ_COAL_DEF_BUF_MAXTIME;
	if (BNXT_RE_CHIP_P7(en_dev->chip_num)) {
		rdev->cq_coalescing.normal_maxbuf = BNXT_QPLIB_CQ_COAL_DEF_NORMAL_MAXBUF_P7;
		rdev->cq_coalescing.during_maxbuf = BNXT_QPLIB_CQ_COAL_DEF_DURING_MAXBUF_P7;
	} else {
		rdev->cq_coalescing.normal_maxbuf = BNXT_QPLIB_CQ_COAL_DEF_NORMAL_MAXBUF_P5;
		rdev->cq_coalescing.during_maxbuf = BNXT_QPLIB_CQ_COAL_DEF_DURING_MAXBUF_P5;
	}
	rdev->cq_coalescing.en_ring_idle_mode = BNXT_QPLIB_CQ_COAL_DEF_EN_RING_IDLE_MODE;

	return rdev;
free_d2p:
	kfree(rdev->d2p);
free_dbg_stats:
	kfree(rdev->dbg_stats);
free_ibdev:
	ib_dealloc_device(&rdev->ibdev);
	return NULL;
}

static int bnxt_re_handle_unaffi_async_event(
		struct creq_func_event *unaffi_async)
{
	switch (unaffi_async->event) {
	case CREQ_FUNC_EVENT_EVENT_TX_WQE_ERROR:
	case CREQ_FUNC_EVENT_EVENT_TX_DATA_ERROR:
	case CREQ_FUNC_EVENT_EVENT_RX_WQE_ERROR:
	case CREQ_FUNC_EVENT_EVENT_RX_DATA_ERROR:
	case CREQ_FUNC_EVENT_EVENT_CQ_ERROR:
	case CREQ_FUNC_EVENT_EVENT_TQM_ERROR:
	case CREQ_FUNC_EVENT_EVENT_CFCQ_ERROR:
	case CREQ_FUNC_EVENT_EVENT_CFCS_ERROR:
	case CREQ_FUNC_EVENT_EVENT_CFCC_ERROR:
	case CREQ_FUNC_EVENT_EVENT_CFCM_ERROR:
	case CREQ_FUNC_EVENT_EVENT_TIM_ERROR:
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int bnxt_re_handle_qp_async_event(void *qp_event, struct bnxt_re_qp *qp)
{
	struct bnxt_re_srq *srq = NULL;
	struct creq_qp_error_notification *err_event;
	struct ib_event event;
	unsigned int flags;

	if (qp->qplib_qp.srq) {
		srq = to_bnxt_re(qp->qplib_qp.srq, struct bnxt_re_srq,
				 qplib_srq);
		set_bit(SRQ_FLAGS_CAPTURE_SNAPDUMP, &srq->qplib_srq.flags);
	}

	set_bit(QP_FLAGS_CAPTURE_SNAPDUMP, &qp->qplib_qp.flags);
	set_bit(CQ_FLAGS_CAPTURE_SNAPDUMP, &qp->scq->qplib_cq.flags);
	set_bit(CQ_FLAGS_CAPTURE_SNAPDUMP, &qp->rcq->qplib_cq.flags);

	if (qp->qplib_qp.state == CMDQ_MODIFY_QP_NEW_STATE_ERR &&
	    !qp->qplib_qp.is_user) {
		flags = bnxt_re_lock_cqs(qp);
		bnxt_qplib_add_flush_qp(&qp->qplib_qp);
		bnxt_re_unlock_cqs(qp, flags);
	}
	memset(&event, 0, sizeof(event));
	event.device = &qp->rdev->ibdev;
	event.element.qp = &qp->ib_qp;
	event.event = IB_EVENT_QP_FATAL;

	err_event = qp_event;

	switch (err_event->req_err_state_reason) {
	case CREQ_QP_ERROR_NOTIFICATION_REQ_ERR_STATE_REASON_REQ_OPCODE_ERROR:
	case CREQ_QP_ERROR_NOTIFICATION_REQ_ERR_STATE_REASON_REQ_TIMEOUT_RETRY_LIMIT:
	case CREQ_QP_ERROR_NOTIFICATION_REQ_ERR_STATE_REASON_REQ_RNR_TIMEOUT_RETRY_LIMIT:
	case CREQ_QP_ERROR_NOTIFICATION_REQ_ERR_STATE_REASON_REQ_NAK_ARRIVAL_2:
	case CREQ_QP_ERROR_NOTIFICATION_REQ_ERR_STATE_REASON_REQ_NAK_ARRIVAL_3:
	case CREQ_QP_ERROR_NOTIFICATION_REQ_ERR_STATE_REASON_REQ_INVALID_READ_RESP:
	case CREQ_QP_ERROR_NOTIFICATION_REQ_ERR_STATE_REASON_REQ_ILLEGAL_BIND:
	case CREQ_QP_ERROR_NOTIFICATION_REQ_ERR_STATE_REASON_REQ_ILLEGAL_FAST_REG:
	case CREQ_QP_ERROR_NOTIFICATION_REQ_ERR_STATE_REASON_REQ_ILLEGAL_INVALIDATE:
	case CREQ_QP_ERROR_NOTIFICATION_REQ_ERR_STATE_REASON_REQ_RETRAN_LOCAL_ERROR:
	case CREQ_QP_ERROR_NOTIFICATION_REQ_ERR_STATE_REASON_REQ_AV_DOMAIN_ERROR:
	case CREQ_QP_ERROR_NOTIFICATION_REQ_ERR_STATE_REASON_REQ_PROD_WQE_MSMTCH_ERROR:
	case CREQ_QP_ERROR_NOTIFICATION_REQ_ERR_STATE_REASON_REQ_PSN_RANGE_CHECK_ERROR:
		event.event = IB_EVENT_QP_ACCESS_ERR;
		break;
	case CREQ_QP_ERROR_NOTIFICATION_REQ_ERR_STATE_REASON_REQ_NAK_ARRIVAL_1:
	case CREQ_QP_ERROR_NOTIFICATION_REQ_ERR_STATE_REASON_REQ_NAK_ARRIVAL_4:
	case CREQ_QP_ERROR_NOTIFICATION_REQ_ERR_STATE_REASON_REQ_READ_RESP_LENGTH:
	case CREQ_QP_ERROR_NOTIFICATION_REQ_ERR_STATE_REASON_REQ_WQE_FORMAT_ERROR:
	case CREQ_QP_ERROR_NOTIFICATION_REQ_ERR_STATE_REASON_REQ_ORRQ_FORMAT_ERROR:
	case CREQ_QP_ERROR_NOTIFICATION_REQ_ERR_STATE_REASON_REQ_INVALID_AVID_ERROR:
	case CREQ_QP_ERROR_NOTIFICATION_REQ_ERR_STATE_REASON_REQ_SERV_TYPE_ERROR:
	case CREQ_QP_ERROR_NOTIFICATION_REQ_ERR_STATE_REASON_REQ_INVALID_OP_ERROR:
		event.event = IB_EVENT_QP_REQ_ERR;
		break;
	case CREQ_QP_ERROR_NOTIFICATION_REQ_ERR_STATE_REASON_REQ_RX_MEMORY_ERROR:
	case CREQ_QP_ERROR_NOTIFICATION_REQ_ERR_STATE_REASON_REQ_TX_MEMORY_ERROR:
	case CREQ_QP_ERROR_NOTIFICATION_REQ_ERR_STATE_REASON_REQ_CMP_ERROR:
	case CREQ_QP_ERROR_NOTIFICATION_REQ_ERR_STATE_REASON_REQ_CQ_LOAD_ERROR:
	case CREQ_QP_ERROR_NOTIFICATION_REQ_ERR_STATE_REASON_REQ_TX_PCI_ERROR:
	case CREQ_QP_ERROR_NOTIFICATION_REQ_ERR_STATE_REASON_REQ_RX_PCI_ERROR:
	case CREQ_QP_ERROR_NOTIFICATION_REQ_ERR_STATE_REASON_REQ_RETX_SETUP_ERROR:
		event.event = IB_EVENT_QP_FATAL;
		break;

	default:
		break;
	}

	switch (err_event->res_err_state_reason) {
	case CREQ_QP_ERROR_NOTIFICATION_RES_ERR_STATE_REASON_RES_EXCEED_MAX:
	case CREQ_QP_ERROR_NOTIFICATION_RES_ERR_STATE_REASON_RES_PAYLOAD_LENGTH_MISMATCH:
	case CREQ_QP_ERROR_NOTIFICATION_RES_ERR_STATE_REASON_RES_OPCODE_ERROR:
	case CREQ_QP_ERROR_NOTIFICATION_RES_ERR_STATE_REASON_RES_PSN_SEQ_ERROR_RETRY_LIMIT:
	case CREQ_QP_ERROR_NOTIFICATION_RES_ERR_STATE_REASON_RES_RX_INVALID_R_KEY:
	case CREQ_QP_ERROR_NOTIFICATION_RES_ERR_STATE_REASON_RES_RX_DOMAIN_ERROR:
	case CREQ_QP_ERROR_NOTIFICATION_RES_ERR_STATE_REASON_RES_RX_NO_PERMISSION:
	case CREQ_QP_ERROR_NOTIFICATION_RES_ERR_STATE_REASON_RES_RX_RANGE_ERROR:
	case CREQ_QP_ERROR_NOTIFICATION_RES_ERR_STATE_REASON_RES_TX_INVALID_R_KEY:
	case CREQ_QP_ERROR_NOTIFICATION_RES_ERR_STATE_REASON_RES_TX_DOMAIN_ERROR:
	case CREQ_QP_ERROR_NOTIFICATION_RES_ERR_STATE_REASON_RES_TX_NO_PERMISSION:
	case CREQ_QP_ERROR_NOTIFICATION_RES_ERR_STATE_REASON_RES_TX_RANGE_ERROR:
	case CREQ_QP_ERROR_NOTIFICATION_RES_ERR_STATE_REASON_RES_UNALIGN_ATOMIC:
	case CREQ_QP_ERROR_NOTIFICATION_RES_ERR_STATE_REASON_RES_PSN_NOT_FOUND:
	case CREQ_QP_ERROR_NOTIFICATION_RES_ERR_STATE_REASON_RES_EXCEEDS_WQE:
	case CREQ_QP_ERROR_NOTIFICATION_RES_ERR_STATE_REASON_RES_WQE_FORMAT_ERROR:
	case CREQ_QP_ERROR_NOTIFICATION_RES_ERR_STATE_REASON_RES_UNSUPPORTED_OPCODE:
	case CREQ_QP_ERROR_NOTIFICATION_RES_ERR_STATE_REASON_RES_REM_INVALIDATE:
	case CREQ_QP_ERROR_NOTIFICATION_RES_ERR_STATE_REASON_RES_INVALID_DUP_RKEY:
	case CREQ_QP_ERROR_NOTIFICATION_RES_ERR_STATE_REASON_RES_IRRQ_FORMAT_ERROR:
		event.event = IB_EVENT_QP_ACCESS_ERR;
		break;
	case CREQ_QP_ERROR_NOTIFICATION_RES_ERR_STATE_REASON_RES_IRRQ_OFLOW:
	case CREQ_QP_ERROR_NOTIFICATION_RES_ERR_STATE_REASON_RES_CMP_ERROR:
	case CREQ_QP_ERROR_NOTIFICATION_RES_ERR_STATE_REASON_RES_CQ_LOAD_ERROR:
	case CREQ_QP_ERROR_NOTIFICATION_RES_ERR_STATE_REASON_RES_TX_PCI_ERROR:
	case CREQ_QP_ERROR_NOTIFICATION_RES_ERR_STATE_REASON_RES_RX_PCI_ERROR:
	case CREQ_QP_ERROR_NOTIFICATION_RES_ERR_STATE_REASON_RES_MEMORY_ERROR:
		event.event = IB_EVENT_QP_FATAL;
		break;
	case CREQ_QP_ERROR_NOTIFICATION_RES_ERR_STATE_REASON_RES_SRQ_LOAD_ERROR:
	case CREQ_QP_ERROR_NOTIFICATION_RES_ERR_STATE_REASON_RES_SRQ_ERROR:
		if (srq)
			event.event = IB_EVENT_SRQ_ERR;
		break;
	default:
		break;
	}

	if (err_event->res_err_state_reason || err_event->req_err_state_reason) {
		dev_err_ratelimited(rdev_to_dev(qp->rdev),
				    "%s: (%s) qp_id %d (0x%x) sq cons %d rq cons %d\n",
				    __func__, qp->qplib_qp.is_user ? "user" : "kernel",
				    qp->qplib_qp.id, qp->qplib_qp.id,
				    err_event->sq_cons_idx,
				    err_event->rq_cons_idx);
		dev_err_ratelimited(rdev_to_dev(qp->rdev),
				    "requestor: (%d) %-8s : (%d) %s\n",
				    err_event->req_slow_path_state,
				    __to_qp_state_str(err_event->req_slow_path_state),
				    err_event->req_err_state_reason,
				    req_err_reason_str(err_event->req_err_state_reason));
		dev_err_ratelimited(rdev_to_dev(qp->rdev),
				    "responder: (%d) %-8s : (%d) %s\n",
				    err_event->res_slow_path_state,
				    __to_qp_state_str(err_event->res_slow_path_state),
				    err_event->res_err_state_reason,
				    res_err_reason_str(err_event->res_err_state_reason));
	} else {
		if (srq)
			event.event = IB_EVENT_QP_LAST_WQE_REACHED;
	}

	if (event.event == IB_EVENT_SRQ_ERR && srq->ib_srq.event_handler)  {
		(*srq->ib_srq.event_handler)(&event,
					     srq->ib_srq.srq_context);
	} else if (event.device && qp->ib_qp.event_handler) {
		qp->ib_qp.event_handler(&event, qp->ib_qp.qp_context);
	}

	if (_is_chip_p7(qp->rdev->chip_ctx)) {
		qp->req_err_state_reason = err_event->req_err_state_reason;
		qp->res_err_state_reason = err_event->res_err_state_reason;
		qp->sq_cons_idx = err_event->sq_cons_idx;
		qp->rq_cons_idx = err_event->rq_cons_idx;
		schedule_work(&qp->dump_slot_task);
	}

	return 0;
}

static int bnxt_re_handle_cq_async_error(void *event, struct bnxt_re_cq *cq)
{
	struct creq_cq_error_notification *cqerr;
	bool send = false;

	cqerr = event;
	switch (cqerr->cq_err_reason) {
	case CREQ_CQ_ERROR_NOTIFICATION_CQ_ERR_REASON_REQ_CQ_INVALID_ERROR:
	case CREQ_CQ_ERROR_NOTIFICATION_CQ_ERR_REASON_REQ_CQ_OVERFLOW_ERROR:
	case CREQ_CQ_ERROR_NOTIFICATION_CQ_ERR_REASON_REQ_CQ_LOAD_ERROR:
	case CREQ_CQ_ERROR_NOTIFICATION_CQ_ERR_REASON_RES_CQ_INVALID_ERROR:
	case CREQ_CQ_ERROR_NOTIFICATION_CQ_ERR_REASON_RES_CQ_OVERFLOW_ERROR:
	case CREQ_CQ_ERROR_NOTIFICATION_CQ_ERR_REASON_RES_CQ_LOAD_ERROR:
		send = true;
	default:
		break;
	}

	if (send && cq->ib_cq.event_handler) {
		struct ib_event ibevent = {};

		ibevent.event = IB_EVENT_CQ_ERR;
		ibevent.element.cq = &cq->ib_cq;
		ibevent.device = &cq->rdev->ibdev;

		dev_err_once(rdev_to_dev(cq->rdev),
			     "%s err reason %d\n", __func__, cqerr->cq_err_reason);
		dev_dbg(rdev_to_dev(cq->rdev),
			"%s err reason %d\n", __func__, cqerr->cq_err_reason);
		cq->ib_cq.event_handler(&ibevent, cq->ib_cq.cq_context);
	}

	cq->qplib_cq.is_cq_err_event = true;

	return 0;
}

static int bnxt_re_handle_affi_async_event(
		struct creq_qp_event *affi_async, void *obj)
{
	struct bnxt_qplib_qp *qplqp;
	struct bnxt_qplib_cq *qplcq;
	struct bnxt_re_qp *qp;
	struct bnxt_re_cq *cq;
	int rc = 0;
	u8 event;

	if (!obj)
		return rc; /* QP was already dead, still return success */

	event = affi_async->event;
	switch (event) {
	case CREQ_QP_EVENT_EVENT_QP_ERROR_NOTIFICATION:
		qplqp = obj;
		qp = container_of(qplqp, struct bnxt_re_qp, qplib_qp);
		/*FIXME: QP referencing */
		rc = bnxt_re_handle_qp_async_event(affi_async, qp);
		break;
	case CREQ_QP_EVENT_EVENT_CQ_ERROR_NOTIFICATION:
		qplcq = obj;
		cq = container_of(qplcq, struct bnxt_re_cq, qplib_cq);
		rc = bnxt_re_handle_cq_async_error(affi_async, cq);
		break;
	default:
		rc = -EINVAL;
	}

	return rc;
}

static int bnxt_re_aeq_handler(struct bnxt_qplib_rcfw *rcfw,
			       void *aeqe, void *obj)
{
	struct creq_func_event *unaffi_async;
	struct creq_qp_event *affi_async;
	u8 type;
	int rc;

	type = ((struct creq_base *)aeqe)->type;
	if (type == CREQ_BASE_TYPE_FUNC_EVENT) {
		unaffi_async = aeqe;
		rc = bnxt_re_handle_unaffi_async_event(unaffi_async);
	} else {
		affi_async = aeqe;
		rc = bnxt_re_handle_affi_async_event(affi_async, obj);
	}

	return rc;
}

static int bnxt_re_srqn_handler(struct bnxt_qplib_nq *nq,
				struct bnxt_qplib_srq *handle, u8 event)
{
	struct bnxt_re_srq *srq = to_bnxt_re(handle, struct bnxt_re_srq,
					     qplib_srq);
	struct ib_event ib_event;

	if (srq == NULL) {
		dev_err(NULL, "%s: SRQ is NULL, SRQN not handled",
			ROCE_DRV_MODULE_NAME);
		return -EINVAL;
	}
	ib_event.device = &srq->rdev->ibdev;
	ib_event.element.srq = &srq->ib_srq;

	if (srq->ib_srq.event_handler) {
		if (event == NQ_SRQ_EVENT_EVENT_SRQ_THRESHOLD_EVENT)
			ib_event.event = IB_EVENT_SRQ_LIMIT_REACHED;

		/* Lock event_handler? */
		(*srq->ib_srq.event_handler)(&ib_event,
					     srq->ib_srq.srq_context);
	}

	return 0;
}

static int bnxt_re_cqn_handler(struct bnxt_qplib_nq *nq,
			       struct bnxt_qplib_cq *handle)
{
	struct bnxt_re_cq *cq = to_bnxt_re(handle, struct bnxt_re_cq,
					   qplib_cq);

	if (cq == NULL) {
		dev_err(NULL, "%s: CQ is NULL, CQN not handled",
			ROCE_DRV_MODULE_NAME);
		return -EINVAL;
	}

	if (cq->ib_cq.comp_handler) {
		/* Lock comp_handler? */
		(*cq->ib_cq.comp_handler)(&cq->ib_cq, cq->ib_cq.cq_context);
	}

	return 0;
}

struct bnxt_qplib_nq *bnxt_re_get_nq(struct bnxt_re_dev *rdev)
{
	int min, indx;

	mutex_lock(&rdev->nqr->load_lock);
	for (indx = 0, min = 0; indx < rdev->nqr->max_init; indx++) {
		if (rdev->nqr->nq[min].load > rdev->nqr->nq[indx].load)
			min = indx;
	}
	rdev->nqr->nq[min].load++;
	mutex_unlock(&rdev->nqr->load_lock);

	return &rdev->nqr->nq[min];
}

void bnxt_re_put_nq(struct bnxt_re_dev *rdev, struct bnxt_qplib_nq *nq)
{
	mutex_lock(&rdev->nqr->load_lock);
	nq->load--;
	mutex_unlock(&rdev->nqr->load_lock);
}

static bool bnxt_re_check_min_attr(struct bnxt_re_dev *rdev)
{
	struct bnxt_qplib_dev_attr *attr;

	attr = rdev->dev_attr;

	if (!attr->max_cq || !attr->max_qp ||
	    !attr->max_sgid || !attr->max_mr) {
		dev_err(rdev_to_dev(rdev),"Insufficient RoCE resources");
		dev_dbg(rdev_to_dev(rdev),
			"max_cq = %d, max_qp = %d, max_dpi = %d, max_sgid = %d, max_mr = %d",
			attr->max_cq, attr->max_qp, attr->max_dpi,
			attr->max_sgid, attr->max_mr);
		return false;
	}
	return true;
}

static void bnxt_re_dispatch_event(struct ib_device *ibdev, struct ib_qp *qp,
				   u8 port_num, enum ib_event_type event)
{
	struct ib_event ib_event;

	ib_event.device = ibdev;
	if (qp) {
		ib_event.element.qp = qp;
		ib_event.event = event;
		if (qp->event_handler)
			qp->event_handler(&ib_event, qp->qp_context);
	} else {
		ib_event.element.port_num = port_num;
		ib_event.event = event;
		ib_dispatch_event(&ib_event);
	}

	dev_dbg(rdev_to_dev(to_bnxt_re_dev(ibdev, ibdev)),
		"ibdev %p Event 0x%x port_num 0x%x", ibdev, event, port_num);
}

static int bnxt_re_update_gid(struct bnxt_re_dev *rdev)
{
	struct bnxt_qplib_sgid_tbl *sgid_tbl = &rdev->qplib_res.sgid_tbl;
	struct bnxt_qplib_gid gid;
	u8 smac[ETH_ALEN] = {0};
	u16 gid_idx, index;
	int rc = 0;

	if (!test_bit(BNXT_RE_FLAG_IBDEV_REGISTERED, &rdev->flags))
		return 0;

	for (index = 0; index < sgid_tbl->active; index++) {
		gid_idx = sgid_tbl->hw_id[index];

		if (!memcmp(&sgid_tbl->tbl[index], &bnxt_qplib_gid_zero,
			    sizeof(bnxt_qplib_gid_zero)))
			continue;
		/* Need to modify the VLAN enable setting of non VLAN GID only
		 * as setting is done for VLAN GID while adding GID
		 *
		 * If disable_prio_vlan_tx is enable, then we'll need to remove the
		 * vlan entry from the sgid_tbl.
		 */
		if (sgid_tbl->vlan[index] == true)
			continue;

		memcpy(&gid, &sgid_tbl->tbl[index].gid, sizeof(gid));
		memcpy(&smac, &sgid_tbl->tbl[index].smac, sizeof(smac));

		rc = bnxt_qplib_update_sgid(sgid_tbl, &gid, gid_idx, smac);
	}

	return rc;
}

static void bnxt_re_get_pri_and_update_gid(struct bnxt_re_dev *rdev)
{
	u8 prio_map = 0;

	/* Get priority for roce */
	prio_map = bnxt_re_get_priority_mask(rdev,
					     (IEEE_8021QAZ_APP_SEL_ETHERTYPE |
					      IEEE_8021QAZ_APP_SEL_DGRAM));
	if (prio_map == rdev->cur_prio_map)
		return;

	rdev->cur_prio_map = prio_map;
	if ((prio_map == 0 && rdev->qplib_res.prio == true) ||
	    (prio_map != 0 && rdev->qplib_res.prio == false)) {
		if (!rdev->cc_param.disable_prio_vlan_tx) {
			rdev->qplib_res.prio = prio_map ? true : false;
			bnxt_re_update_gid(rdev);
		}
	}
}

static void bnxt_re_clear_cc(struct bnxt_re_dev *rdev)
{
	struct bnxt_qplib_cc_param *cc_param = &rdev->cc_param;

	mutex_lock(&rdev->cc_lock);
	if (!is_qport_service_type_supported(rdev))
		bnxt_re_clear_dscp(rdev);

	if (_is_chip_p7_plus(rdev->chip_ctx)) {
		cc_param->mask = CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_TOS_DSCP;
	} else {
		cc_param->mask = (CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_ENABLE_CC |
				  CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_TOS_ECN);

		if (!is_qport_service_type_supported(rdev))
			cc_param->mask |=
			(CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_ALT_VLAN_PCP |
			 CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_ALT_TOS_DSCP |
			 CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_TOS_DSCP);
	}

	if (bnxt_qplib_modify_cc(&rdev->qplib_res, cc_param))
		dev_err(rdev_to_dev(rdev), "Failed to modify cc\n");
	mutex_unlock(&rdev->cc_lock);
}

static int bnxt_re_setup_cc(struct bnxt_re_dev *rdev)
{
	struct bnxt_qplib_cc_param *cc_param = &rdev->cc_param;
	int rc;

	mutex_lock(&rdev->cc_lock);
	/* set the default parameters for enabling CC */
	cc_param->tos_ecn = 0x1;
	cc_param->enable = 0x1;
	cc_param->mask = (CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_ENABLE_CC |
			  CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_TOS_ECN);

	if (!is_qport_service_type_supported(rdev))
		cc_param->mask |=
		(CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_ALT_VLAN_PCP |
		 CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_ALT_TOS_DSCP |
		 CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_TOS_DSCP);

	rc = bnxt_qplib_modify_cc(&rdev->qplib_res, cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to modify cc\n");
		mutex_unlock(&rdev->cc_lock);
		return rc;
	}
	if (!is_qport_service_type_supported(rdev)) {
		rc = bnxt_re_setup_dscp(rdev);
		if (rc)
			goto clear;
	}

	/* Reset the programming mask */
	cc_param->mask = 0;
	if (cc_param->qp1_tos_dscp != cc_param->tos_dscp) {
		cc_param->qp1_tos_dscp = cc_param->tos_dscp;
		rc = bnxt_re_update_qp1_tos_dscp(rdev);
		if (rc) {
			dev_err(rdev_to_dev(rdev), "%s:Failed to modify QP1:%d",
				__func__, rc);
			goto clear;
		}
	}

	mutex_unlock(&rdev->cc_lock);
	return 0;

clear:
	mutex_unlock(&rdev->cc_lock);
	bnxt_re_clear_cc(rdev);
	return rc;
}

int bnxt_re_query_hwrm_dscp2pri(struct bnxt_re_dev *rdev,
				u16 target_id)
{
	struct hwrm_queue_dscp2pri_qcfg_input req = {};
	struct hwrm_queue_dscp2pri_qcfg_output resp;
	struct bnxt_en_dev *en_dev = rdev->en_dev;
	struct bnxt_re_dscp2pri *dscp2pri;
	struct bnxt_fw_msg fw_msg = {};
	dma_addr_t dma_handle;
	u16 data_len, count;
	int rc = 0, i;
	u8 *kmem;

	data_len = MAX_DSCP_PRI_TUPLE * sizeof(*dscp2pri);
	bnxt_re_init_hwrm_hdr((void *)&req, HWRM_QUEUE_DSCP2PRI_QCFG, target_id);
	req.port_id = (target_id == 0xFFFF) ? en_dev->pf_port_id : 1;

	kmem = dma_zalloc_coherent(&en_dev->pdev->dev, data_len, &dma_handle,
				   GFP_KERNEL);
	if (!kmem) {
		dev_err(rdev_to_dev(rdev),
			"dma_zalloc_coherent failure, length = %u\n",
			(unsigned)data_len);
		return -ENOMEM;
	}
	req.dest_data_addr = cpu_to_le64(dma_handle);
	req.dest_data_buffer_size = cpu_to_le16(data_len);
	bnxt_re_fill_fw_msg(&fw_msg, (void *)&req, sizeof(req), (void *)&resp,
			    sizeof(resp), BNXT_RE_HWRM_CMD_TIMEOUT(rdev));
	rc = bnxt_send_msg(en_dev, &fw_msg);
	if (rc)
		goto out;

	/* Upload the DSCP-MASK-PRI tuple(s) */
	dscp2pri = (struct bnxt_re_dscp2pri *)kmem;
	count = le16_to_cpu(resp.entry_cnt);
	for (i = 0; i < count && i < MAX_DSCP_PRI_TUPLE; i++) {
		rdev->d2p[i].dscp = dscp2pri->dscp;
		rdev->d2p[i].mask = dscp2pri->mask;
		rdev->d2p[i].pri = dscp2pri->pri;
		dscp2pri++;
	}
	rdev->d2p_count = count;
out:
	dma_free_coherent(&en_dev->pdev->dev, data_len, kmem, dma_handle);
	return rc;
}

int bnxt_re_prio_vlan_tx_update(struct bnxt_re_dev *rdev)
{
	/* Remove the VLAN from the GID entry */
	if (rdev->cc_param.disable_prio_vlan_tx)
		rdev->qplib_res.prio = false;
	else
		rdev->qplib_res.prio = true;

	return bnxt_re_update_gid(rdev);
}

int bnxt_re_set_hwrm_dscp2pri(struct bnxt_re_dev *rdev,
			      struct bnxt_re_dscp2pri *d2p, u16 count,
			      u16 target_id)
{
	struct hwrm_queue_dscp2pri_cfg_input req = {};
	struct bnxt_en_dev *en_dev = rdev->en_dev;
	struct hwrm_queue_dscp2pri_cfg_output resp;
	struct bnxt_re_dscp2pri *dscp2pri;
	struct bnxt_fw_msg fw_msg = {};
	int i, rc, data_len = 3 * 256;
	dma_addr_t dma_handle;
	u8 *kmem;

	bnxt_re_init_hwrm_hdr((void *)&req, HWRM_QUEUE_DSCP2PRI_CFG, target_id);
	req.port_id = (target_id == 0xFFFF) ? en_dev->pf_port_id : 1;

	kmem = dma_alloc_coherent(&en_dev->pdev->dev, data_len, &dma_handle,
				  GFP_KERNEL);
	if (!kmem) {
		dev_err(rdev_to_dev(rdev),
			"dma_alloc_coherent failure, length = %u\n",
			(unsigned)data_len);
		return -ENOMEM;
	}
	req.src_data_addr = cpu_to_le64(dma_handle);

	/* Download the DSCP-MASK-PRI tuple(s) */
	dscp2pri = (struct bnxt_re_dscp2pri *)kmem;
	for (i = 0; i < count; i++) {
		dscp2pri->dscp = d2p[i].dscp;
		dscp2pri->mask = d2p[i].mask;
		dscp2pri->pri = d2p[i].pri;
		dscp2pri++;
	}

	req.entry_cnt = cpu_to_le16(count);
	bnxt_re_fill_fw_msg(&fw_msg, (void *)&req, sizeof(req), (void *)&resp,
			    sizeof(resp), BNXT_RE_HWRM_CMD_TIMEOUT(rdev));
	rc = bnxt_send_msg(en_dev, &fw_msg);
	dma_free_coherent(&en_dev->pdev->dev, data_len, kmem, dma_handle);
	return rc;
}

int bnxt_re_query_hwrm_qportcfg(struct bnxt_re_dev *rdev,
			struct bnxt_re_tc_rec *tc_rec, u16 tid)
{
	u8 max_tc, tc, *qptr, *type_ptr0, *type_ptr1;
	struct hwrm_queue_qportcfg_output resp = {0};
	struct hwrm_queue_qportcfg_input req = {0};
	struct bnxt_en_dev *en_dev = rdev->en_dev;
	struct bnxt_fw_msg fw_msg = {};
	u8 *tmp_type, cos_id, i = 0;
	bool def_init = false;
	bool def_roce = false;
	bool def_cnp = false;
	int rc;

	bnxt_re_init_hwrm_hdr((void *)&req, HWRM_QUEUE_QPORTCFG, tid);
	req.port_id = (tid == 0xFFFF) ? en_dev->pf_port_id : 1;
	if (BNXT_EN_ASYM_Q(en_dev))
		req.flags = cpu_to_le32(QUEUE_QPORTCFG_REQ_FLAGS_PATH_RX);

	bnxt_re_fill_fw_msg(&fw_msg, (void *)&req, sizeof(req), (void *)&resp,
			    sizeof(resp), BNXT_RE_HWRM_CMD_TIMEOUT(rdev));
	rc = bnxt_send_msg(en_dev, &fw_msg);
	if (rc)
		return rc;

	if (!resp.max_configurable_queues)
		return -EINVAL;

        max_tc = resp.max_configurable_queues;
	tc_rec->max_tc = max_tc;

	if (resp.queue_cfg_info & QUEUE_QPORTCFG_RESP_QUEUE_CFG_INFO_USE_PROFILE_TYPE)
		tc_rec->serv_type_enabled = true;

	qptr = &resp.queue_id0;
	type_ptr0 = &resp.queue_id0_service_profile_type;
	type_ptr1 = &resp.queue_id1_service_profile_type;
	for (tc = 0; tc < max_tc; tc++) {
		tmp_type = tc ? type_ptr1 + (tc - 1) : type_ptr0;

		cos_id = *qptr++;
		/* RoCE CoS queue is the first cos queue.
		 * For MP12 and MP17 order is 405 and 141015.
		 */
		if (is_bnxt_roce_queue(rdev, *qptr, *tmp_type)) {
			if (!def_roce) {
				tc_rec->cos_id_roce = cos_id;
				tc_rec->tc_roce = tc;
				def_roce = true;
			}
			/* Set lossless_qids only if the device is
			 * primary.
			 */
			if (tid == 0xFFFF) {
				rdev->lossless_qid[i] = cos_id;
				rdev->lossless_q_count++;
			}
			i++;
		} else if (is_bnxt_cnp_queue(rdev, *qptr, *tmp_type)) {
			if (!def_cnp) {
				tc_rec->cos_id_cnp = cos_id;
				tc_rec->tc_cnp = tc;
				def_cnp = true;
			}
		} else if (!def_init) {
			def_init = true;
			tc_rec->tc_def = tc;
			tc_rec->cos_id_def = cos_id;
		}
		qptr++;
	}

	return rc;
}

int bnxt_re_hwrm_cos2bw_qcfg(struct bnxt_re_dev *rdev, u16 target_id,
			     struct bnxt_re_cos2bw_cfg *cfg)
{
	struct bnxt_en_dev *en_dev = rdev->en_dev;
	struct hwrm_queue_cos2bw_qcfg_output resp;
        struct hwrm_queue_cos2bw_qcfg_input req = {0};
	struct bnxt_fw_msg fw_msg = {};
	int rc, indx;
	void *data;

	bnxt_re_init_hwrm_hdr((void *)&req, HWRM_QUEUE_COS2BW_QCFG, target_id);
	req.port_id = (target_id == 0xFFFF) ? en_dev->pf_port_id : 1;

	bnxt_re_fill_fw_msg(&fw_msg, (void *)&req, sizeof(req), (void *)&resp,
			    sizeof(resp), BNXT_RE_HWRM_CMD_TIMEOUT(rdev));
	rc = bnxt_send_msg(en_dev, &fw_msg);
	if (rc)
		return rc;
	data = &resp.queue_id0 + offsetof(struct bnxt_re_cos2bw_cfg,
					  queue_id);
	for (indx = 0; indx < 8; indx++, data += (sizeof(cfg->cfg))) {
		memcpy(&cfg->cfg, data, sizeof(cfg->cfg));
		if (indx == 0)
			cfg->queue_id = resp.queue_id0;
		cfg++;
	}

	return rc;
}

int bnxt_re_hwrm_cos2bw_cfg(struct bnxt_re_dev *rdev, u16 target_id,
			    struct bnxt_re_cos2bw_cfg *cfg)
{
	struct bnxt_en_dev *en_dev = rdev->en_dev;
	struct bnxt_queue_cos2bw_cfg_input req = {0};
	struct hwrm_queue_cos2bw_cfg_output resp = {0};
	struct bnxt_fw_msg fw_msg = {};
	int indx;
	int rc;

	bnxt_re_init_hwrm_hdr((void *)&req, HWRM_QUEUE_COS2BW_CFG, target_id);
	req.port_id = (target_id == 0xFFFF) ? en_dev->pf_port_id : 1;

	/* Chimp wants enable bit to retain previous
	 * config done by L2 driver
	 */
	for (indx = 0; indx < 8; indx++) {
		if (cfg[indx].queue_id < 40) {
			req.enables |= cpu_to_le32(
				QUEUE_COS2BW_CFG_REQ_ENABLES_COS_QUEUE_ID0_VALID <<
				indx);
		}

		if (indx == 0) {
			req.queue_id0 = cfg[0].queue_id;
			req.queue_id0_min_bw = cfg[0].min_bw;
			req.queue_id0_max_bw = cfg[0].max_bw;
			req.queue_id0_tsa_assign = cfg[0].tsa;
			req.queue_id0_pri_lvl = cfg[0].pri_lvl;
			req.queue_id0_bw_weight = cfg[0].bw_weight;
		} else {
			memcpy(&req.cfg[indx - 1], &cfg[indx].cfg, sizeof(cfg[indx].cfg));

		}
	}

	memset(&resp, 0, sizeof(resp));
	bnxt_re_fill_fw_msg(&fw_msg, (void *)&req, sizeof(req), (void *)&resp,
			    sizeof(resp), BNXT_RE_HWRM_CMD_TIMEOUT(rdev));
	rc = bnxt_send_msg(en_dev, &fw_msg);
	return rc;
}

int bnxt_re_hwrm_pri2cos_qcfg(struct bnxt_re_dev *rdev,
			      struct bnxt_re_tc_rec *tc_rec,
			      u16 target_id)
{
	struct hwrm_queue_pri2cos_qcfg_output resp = {0};
	struct hwrm_queue_pri2cos_qcfg_input req = {};
	struct bnxt_en_dev *en_dev = rdev->en_dev;
	struct bnxt_fw_msg fw_msg = {};
	u8 *pri2cos, queue_id;
	int rc, i;

	tc_rec->prio_valid = 0;
	tc_rec->roce_prio = 0;
	tc_rec->cnp_prio = 0;

	bnxt_re_init_hwrm_hdr((void *)&req, HWRM_QUEUE_PRI2COS_QCFG,
			      target_id);

	req.flags = cpu_to_le32(QUEUE_PRI2COS_QCFG_REQ_FLAGS_IVLAN);

	if (BNXT_EN_ASYM_Q(en_dev))
		req.flags |= cpu_to_le32(QUEUE_PRI2COS_QCFG_REQ_FLAGS_PATH_RX);
	bnxt_re_fill_fw_msg(&fw_msg, (void *)&req, sizeof(req), (void *)&resp,
			    sizeof(resp), BNXT_RE_HWRM_CMD_TIMEOUT(rdev));
	rc = bnxt_send_msg(en_dev, &fw_msg);
	if (rc)
		return rc;

	pri2cos = &resp.pri0_cos_queue_id;
	for (i = 0; i < IEEE_8021QAZ_MAX_TCS; i++) {
		queue_id = pri2cos[i];
		if (queue_id == tc_rec->cos_id_cnp) {
			tc_rec->cnp_prio = i;
			tc_rec->prio_valid |= (1 << CNP_PRIO_VALID);
		} else if (queue_id == tc_rec->cos_id_roce) {
			tc_rec->roce_prio = i;
			tc_rec->prio_valid |= (1 << ROCE_PRIO_VALID);
		}
		rdev->p2cos[i] = pri2cos[i];
	}
	return rc;
}

int bnxt_re_hwrm_pri2cos_cfg(struct bnxt_re_dev *rdev,
			     u16 target_id, u16 port_id,
			     u8 *cos_id_map, u8 pri_map)
{
	struct hwrm_queue_pri2cos_cfg_output resp = {0};
	struct hwrm_queue_pri2cos_cfg_input req = {};
	struct bnxt_en_dev *en_dev = rdev->en_dev;
	struct bnxt_fw_msg fw_msg = {};
	u32 flags = 0;
	u8 *pri2cos;
	int rc = 0;
	int i;

	bnxt_re_init_hwrm_hdr((void *)&req, HWRM_QUEUE_PRI2COS_CFG, target_id);
	flags = (QUEUE_PRI2COS_CFG_REQ_FLAGS_PATH_BIDIR |
		 QUEUE_PRI2COS_CFG_REQ_FLAGS_IVLAN);
	req.flags = cpu_to_le32(flags);
	req.port_id = port_id;

	pri2cos = &req.pri0_cos_queue_id;

	for (i = 0; i < 8; i++) {
		if (pri_map & (1 << i)) {
			req.enables |= cpu_to_le32
			(QUEUE_PRI2COS_CFG_REQ_ENABLES_PRI0_COS_QUEUE_ID << i);
			pri2cos[i] = cos_id_map[i];
		}
	}

	bnxt_re_fill_fw_msg(&fw_msg, (void *)&req, sizeof(req), (void *)&resp,
			    sizeof(resp), BNXT_RE_HWRM_CMD_TIMEOUT(rdev));
	rc = bnxt_send_msg(en_dev, &fw_msg);

	return rc;
}

static int bnxt_re_cnp_pri2cos_cfg(struct bnxt_re_dev *rdev, u16 target_id, bool reset)
{
	struct bnxt_en_dev *en_dev = rdev->en_dev;
	struct bnxt_re_tc_rec *tc_rec;
	u8 cos_id_map[8] = {0};
	u8 pri_map = 0;
	u16 port_id;
	u8 cnp_pri;

	cnp_pri = rdev->cc_param.alt_vlan_pcp;

	if (target_id == 0xFFFF) {
		port_id = en_dev->pf_port_id;
		tc_rec = &rdev->tc_rec[0];
	} else {
		port_id = 1;
		tc_rec = &rdev->tc_rec[1];
	}

	pri_map |= (1 << cnp_pri);
	if (reset)
		cos_id_map[cnp_pri] = tc_rec->cos_id_def;
	else
		cos_id_map[cnp_pri] = tc_rec->cos_id_cnp;

	return bnxt_re_hwrm_pri2cos_cfg(rdev, target_id, port_id,
					cos_id_map, pri_map);
}

static void bnxt_re_put_stats_ctx(struct bnxt_re_dev *rdev)
{
	struct bnxt_qplib_ctx *hctx;
	struct bnxt_qplib_res *res;
	u16 tid = 0xffff;

	res = &rdev->qplib_res;
	hctx = res->hctx;

	if (test_and_clear_bit(BNXT_RE_FLAG_STATS_CTX_ALLOC, &rdev->flags)) {
		bnxt_re_net_stats_ctx_free(rdev, hctx->stats.fw_id, tid);
		bnxt_qplib_free_stat_mem(res, &hctx->stats);
	}
}

static void bnxt_re_put_stats2_ctx(struct bnxt_re_dev *rdev)
{
	struct bnxt_qplib_ctx *hctx;
	struct bnxt_qplib_res *res;
	u16 tid;

	res = &rdev->qplib_res;
	hctx = res->hctx;

	if (test_and_clear_bit(BNXT_RE_FLAG_STATS_CTX2_ALLOC,
			       &rdev->flags)) {
		if (rdev->binfo) {
			tid = PCI_FUNC(rdev->binfo->pdev2->devfn) + 1;
			bnxt_re_net_stats_ctx_free(rdev, hctx->stats2.fw_id,
						   tid);
			bnxt_qplib_free_stat_mem(res, &hctx->stats2);
		}
	}
}

static void bnxt_re_put_stats3_ctx(struct bnxt_re_dev *rdev)
{
	struct bnxt_qplib_ctx *hctx;
	struct bnxt_qplib_res *res;

	res = &rdev->qplib_res;
	hctx = res->hctx;

	if (test_and_clear_bit(BNXT_RE_FLAG_STATS_CTX3_ALLOC, &rdev->flags)) {
		bnxt_re_net_stats_ctx_free(rdev, hctx->stats3.fw_id, 0xffff);
		bnxt_qplib_free_stat_mem(res, &hctx->stats3);
	}
}

static int bnxt_re_get_stats_ctx(struct bnxt_re_dev *rdev)
{
	struct bnxt_qplib_ctx *hctx;
	struct bnxt_qplib_res *res;
	u16 tid = 0xffff;
	int rc;

	res = &rdev->qplib_res;
	hctx = res->hctx;

	rc = bnxt_qplib_alloc_stat_mem(res->pdev, rdev->chip_ctx, &hctx->stats);
	if (rc)
		return -ENOMEM;
	rc = bnxt_re_net_stats_ctx_alloc(rdev, tid, &hctx->stats);
	if (rc)
		goto free_stat_mem;
	set_bit(BNXT_RE_FLAG_STATS_CTX_ALLOC, &rdev->flags);

	return 0;

free_stat_mem:
	bnxt_qplib_free_stat_mem(res, &hctx->stats);

	return rc;
}

static int bnxt_re_get_stats2_ctx(struct bnxt_re_dev *rdev)
{
	struct bnxt_qplib_ctx *hctx;
	struct bnxt_qplib_res *res;
	u16 tid;
	int rc;

	if (!rdev->binfo)
		return 0;

	res = &rdev->qplib_res;
	hctx = res->hctx;

	rc = bnxt_qplib_alloc_stat_mem(res->pdev, rdev->chip_ctx, &hctx->stats2);
	if (rc)
		return -ENOMEM;

	tid = (PCI_FUNC(rdev->binfo->pdev2->devfn) + 1);
	rc = bnxt_re_net_stats_ctx_alloc(rdev, tid, &hctx->stats2);
	if (rc)
		goto free_stat_mem;
	dev_dbg(rdev_to_dev(rdev), " LAG second stat context %x\n",
		rdev->qplib_res.hctx->stats2.fw_id);
	set_bit(BNXT_RE_FLAG_STATS_CTX2_ALLOC, &rdev->flags);

	return 0;

free_stat_mem:
	bnxt_qplib_free_stat_mem(res, &hctx->stats2);

	return rc;
}

static int bnxt_re_get_stats3_ctx(struct bnxt_re_dev *rdev)
{
	struct bnxt_qplib_ctx *hctx;
	struct bnxt_qplib_res *res;
	int rc;

	if (!rdev->rcfw.roce_mirror)
		return 0;

	res = &rdev->qplib_res;
	hctx = res->hctx;

	rc = bnxt_qplib_alloc_stat_mem(res->pdev, rdev->chip_ctx, &hctx->stats3);
	if (rc)
		return rc;

	rc = bnxt_re_net_stats_ctx_alloc(rdev, 0xffff, &hctx->stats3);
	if (rc)
		goto free_stat_mem;

	set_bit(BNXT_RE_FLAG_STATS_CTX3_ALLOC, &rdev->flags);

	return 0;

free_stat_mem:
	bnxt_qplib_free_stat_mem(res, &hctx->stats3);

	return rc;
}

static int bnxt_re_update_dev_attr(struct bnxt_re_dev *rdev)
{
	int rc;

	rc = bnxt_qplib_get_dev_attr(&rdev->rcfw);
	if (rc)
		return rc;
	bnxt_qplib_query_version(&rdev->rcfw);
	if (!bnxt_re_check_min_attr(rdev))
		return -EINVAL;
	return 0;
}

static int bnxt_re_set_port_cnp_ets(struct bnxt_re_dev *rdev, u16 portid, bool reset)
{
	struct bnxt_re_cos2bw_cfg ets_cfg[8];
	struct bnxt_re_cos2bw_cfg *cnp_ets_cfg;
	struct bnxt_re_tc_rec *tc_rec;
	int indx, rc;

	rc = bnxt_re_hwrm_cos2bw_qcfg(rdev, portid, ets_cfg);
	if (rc)
		goto bail;

	tc_rec = (portid == 0xFFFF) ? &rdev->tc_rec[0] : &rdev->tc_rec[1];
	indx = tc_rec->cos_id_cnp - tc_rec->cos_id_roce;
	cnp_ets_cfg = &ets_cfg[indx];
	cnp_ets_cfg->queue_id = tc_rec->cos_id_cnp;
	cnp_ets_cfg->tsa = QUEUE_COS2BW_QCFG_RESP_QUEUE_ID0_TSA_ASSIGN_SP;
	cnp_ets_cfg->pri_lvl = 7; /* Max priority for CNP. */

	/* Configure cnp ets to strict */
	rc = bnxt_re_hwrm_cos2bw_cfg(rdev, portid, ets_cfg);
	if (rc)
		goto bail;

	rc = bnxt_re_cnp_pri2cos_cfg(rdev, portid, reset);
bail:
	return rc;
}

static int bnxt_re_setup_port_cnp_cos(struct bnxt_re_dev *rdev, u16 portid, bool reset)
{
	int rc;
	struct bnxt_re_tc_rec *tc_rec;

	/* Query CNP cos id */
	tc_rec = (portid == 0xFFFF) ?
		       &rdev->tc_rec[0] : &rdev->tc_rec[1];
	rc = bnxt_re_query_hwrm_qportcfg(rdev, tc_rec, portid);
	if (rc)
		return rc;
	/* config ETS */
	rc = bnxt_re_set_port_cnp_ets(rdev, portid, reset);

	return rc;
}

int bnxt_re_setup_cnp_cos(struct bnxt_re_dev *rdev, bool reset)
{
	u16 portid;
	int rc;

	portid = 0xFFFF;
	rc = bnxt_re_setup_port_cnp_cos(rdev, portid, reset);
	if (rc) {
		dev_err(rdev_to_dev(rdev),
			"Failed to setup cnp cos for pci function %d\n",
			bnxt_re_dev_pcifn_id(rdev));
		return rc;
	}

	if (rdev->binfo) {
		portid = 2;
		rc = bnxt_re_setup_port_cnp_cos(rdev, portid, reset);
		if (rc)
			dev_err(rdev_to_dev(rdev),
				"Failed to setup cnp cos for pci function %d\n",
				bnxt_re_dev_pcifn_id(rdev));
	}

	return rc;
}

static void bnxt_re_free_tbls(struct bnxt_re_dev *rdev)
{
	bnxt_qplib_clear_tbls(&rdev->qplib_res);
	bnxt_qplib_free_tbls(&rdev->qplib_res);
}

static int bnxt_re_alloc_init_tbls(struct bnxt_re_dev *rdev)
{
	int rc;

	rc = bnxt_qplib_alloc_tbls(&rdev->qplib_res);
	if (rc)
		return rc;
	set_bit(BNXT_RE_FLAG_TBLS_ALLOCINIT, &rdev->flags);

	return 0;
}

static void bnxt_re_clean_nqs(struct bnxt_re_dev *rdev)
{
	struct bnxt_qplib_nq *nq;
	int i;

	if (!rdev->nqr->max_init)
		return;

	for (i = (rdev->nqr->max_init - 1) ; i >= 0; i--) {
		nq = &rdev->nqr->nq[i];
		bnxt_qplib_disable_nq(nq);
		bnxt_re_net_ring_free(rdev, nq->ring_id);
		bnxt_qplib_free_nq_mem(nq);
	}
	rdev->nqr->max_init = 0;
}

static int bnxt_re_setup_nqs(struct bnxt_re_dev *rdev)
{
	struct bnxt_re_ring_attr rattr = {};
	struct bnxt_qplib_nq *nq;
	int rc, i;
	int depth;
	u32 offt;
	u16 vec;

	mutex_init(&rdev->nqr->load_lock);

	/*
	 * TODO: Optimize the depth based on the
	 * number of NQs.
	 */
	depth = BNXT_QPLIB_NQE_MAX_CNT;
	for (i = 0; i < rdev->nqr->num_msix - 1; i++) {
		nq = &rdev->nqr->nq[i];
		vec = rdev->nqr->msix_entries[i + 1].vector;
		offt = rdev->nqr->msix_entries[i + 1].db_offset;
		nq->hwq.max_elements = depth;
		rc = bnxt_qplib_alloc_nq_mem(&rdev->qplib_res, nq);
		if (rc) {
			dev_err(rdev_to_dev(rdev),
				"Failed to get mem for NQ %d, rc = 0x%x",
				i, rc);
			goto fail_mem;
		}

		rattr.dma_arr = nq->hwq.pbl[PBL_LVL_0].pg_map_arr;
		rattr.pages = nq->hwq.pbl[rdev->nqr->nq[i].hwq.level].pg_count;
		rattr.type = bnxt_re_get_rtype(rdev);
		rattr.mode = RING_ALLOC_REQ_INT_MODE_MSIX;
		rattr.depth = nq->hwq.max_elements - 1;
		rattr.lrid = rdev->nqr->msix_entries[i + 1].ring_idx;

		/* Set DBR pacing capability on the first NQ ring only */
		if (!i && bnxt_qplib_dbr_pacing_ext_en(rdev->chip_ctx) && !rdev->is_virtfn)
			rattr.flags = RING_ALLOC_REQ_FLAGS_NQ_DBR_PACING;
		else
			rattr.flags = 0;

		rc = bnxt_re_net_ring_alloc(rdev, &rattr, &nq->ring_id);
		if (rc) {
			nq->ring_id = 0xffff; /* Invalid ring-id */
			dev_err(rdev_to_dev(rdev),
				"Failed to get fw id for NQ %d, rc = 0x%x",
				i, rc);
			goto fail_ring;
		}

		rc = bnxt_qplib_enable_nq(nq, i, vec, offt,
					  &bnxt_re_cqn_handler,
					  &bnxt_re_srqn_handler);
		if (rc) {
			dev_err(rdev_to_dev(rdev),
				"Failed to enable NQ %d, rc = 0x%x", i, rc);
			goto fail_en;
		}
	}

	rdev->nqr->max_init = i;
	return 0;
fail_en:
	/* *nq was i'th nq */
	bnxt_re_net_ring_free(rdev, nq->ring_id);
fail_ring:
	bnxt_qplib_free_nq_mem(nq);
fail_mem:
	rdev->nqr->max_init = i;
	return rc;
}

static void bnxt_re_sysfs_destroy_file(struct bnxt_re_dev *rdev)
{
#ifndef HAVE_RDMA_SET_DEVICE_SYSFS_GROUP
	int i;

	for (i = 0; i < ARRAY_SIZE(bnxt_re_attributes); i++)
		device_remove_file(&rdev->ibdev.dev, bnxt_re_attributes[i]);
#endif
}

static int bnxt_re_sysfs_create_file(struct bnxt_re_dev *rdev)
{
#ifndef HAVE_RDMA_SET_DEVICE_SYSFS_GROUP
	int i, j, rc = 0;

	for (i = 0; i < ARRAY_SIZE(bnxt_re_attributes); i++) {
		rc = device_create_file(&rdev->ibdev.dev,
					bnxt_re_attributes[i]);
		if (rc) {
			dev_err(rdev_to_dev(rdev),
				"Failed to create IB sysfs with rc = 0x%x", rc);
			/* Must clean up all created device files */
			for (j = 0; j < i; j++)
				device_remove_file(&rdev->ibdev.dev,
						   bnxt_re_attributes[j]);
			clear_bit(BNXT_RE_FLAG_IBDEV_REGISTERED, &rdev->flags);
			ib_unregister_device(&rdev->ibdev);
			return rc;
		}
	}
#endif
	return 0;
}

/* worker thread for polling periodic events. Now used for QoS programming*/
static void bnxt_re_worker(struct work_struct *work)
{
	struct bnxt_re_dev *rdev = container_of(work, struct bnxt_re_dev,
						worker.work);
	u8 active_port_map = 0;
	int rc;

	/* QoS is in 30s cadence for PFs*/
	if (!rdev->is_virtfn && !rdev->worker_30s--) {
		bnxt_re_get_pri_and_update_gid(rdev);
		rdev->worker_30s = 30;
	}

	/* TBD - Remove bnxt_re_mutex.
	 * Below code is required for udev rename.
	 * ib_device_rename->device_rename, handles renaming kobj
	 * of sysfs but not for debugfs.
	 * Eventually, use pci device name based debugfs entry
	 * instead of bnxt_re name based debugfs.
	 */
	if (mutex_trylock(&bnxt_re_mutex)) {
		if (test_bit(BNXT_RE_FLAG_IBDEV_REGISTERED, &rdev->flags))
			bnxt_re_rename_debugfs_entry(rdev);
		mutex_unlock(&bnxt_re_mutex);
	}

	if (rdev->binfo) {
		if (!test_bit(BNXT_RE_FLAG_IBDEV_REGISTERED, &rdev->flags))
			goto resched;
		if (rtnl_trylock()) {
			active_port_map =
				bnxt_re_get_bond_link_status(rdev->binfo);
			rtnl_unlock();
			if (rdev->binfo->active_port_map != active_port_map) {
				rdev->binfo->active_port_map = active_port_map;
				dev_info(rdev_to_dev(rdev),
					 "Updating lag device from worker active_port_map = 0x%x",
					 rdev->binfo->active_port_map);
				bnxt_re_update_fw_lag_info(rdev->binfo, rdev,
							   true);
			}
		}
	}

	if (!rdev->stats.stats_query_sec)
		goto resched;

	if (test_bit(BNXT_RE_FLAG_ISSUE_CFA_FLOW_STATS, &rdev->flags) &&
	    ((rdev->is_virtfn && !_is_chip_num_p8(rdev->en_dev->chip_num)) ||
	    (!_is_ext_stats_supported(rdev->dev_attr->dev_cap_flags) &&
		!bnxt_ext_stats_v2_supported(rdev->dev_attr->dev_cap_ext_flags)))) {
		if (!(rdev->stats.stats_query_counter++ %
		      rdev->stats.stats_query_sec)) {
			rc = bnxt_re_get_qos_stats(rdev);
			if (rc && rc != -ENOMEM)
				clear_bit(BNXT_RE_FLAG_ISSUE_CFA_FLOW_STATS,
					  &rdev->flags);
			}
	}

	if (test_bit(BNXT_RE_FLAG_ISSUE_ROCE_STATS, &rdev->flags))
		bnxt_re_get_roce_data_stats(rdev);

resched:
	schedule_delayed_work(&rdev->worker, msecs_to_jiffies(1000));
}

static int bnxt_re_alloc_dbr_sw_stats_mem(struct bnxt_re_dev *rdev)
{
	if (!(rdev->dbr_drop_recov || rdev->dbr_pacing))
		return 0;

	rdev->dbr_sw_stats = kzalloc(sizeof(*rdev->dbr_sw_stats), GFP_KERNEL);
	if (!rdev->dbr_sw_stats)
		return -ENOMEM;

	return 0;
}

static void bnxt_re_free_dbr_sw_stats_mem(struct bnxt_re_dev *rdev)
{
	kfree(rdev->dbr_sw_stats);
	rdev->dbr_sw_stats = NULL;
}

static int bnxt_re_initialize_dbr_drop_recov(struct bnxt_re_dev *rdev)
{
	rdev->dbr_drop_recov_wq =
		create_singlethread_workqueue("bnxt_re_dbr_drop_recov");
	if (!rdev->dbr_drop_recov_wq) {
		dev_err(rdev_to_dev(rdev), "DBR Drop Revov wq alloc failed!");
		return -EINVAL;
	}
	rdev->dbr_drop_recov = true;

	/* Enable configfs setting dbr_drop_recov by default*/
	rdev->user_dbr_drop_recov = true;

	rdev->user_dbr_drop_recov_timeout = BNXT_RE_DBR_RECOV_USERLAND_TIMEOUT;
	return 0;
}

static void bnxt_re_deinitialize_dbr_drop_recov(struct bnxt_re_dev *rdev)
{
	if (rdev->dbr_drop_recov_wq) {
		flush_workqueue(rdev->dbr_drop_recov_wq);
		destroy_workqueue(rdev->dbr_drop_recov_wq);
		rdev->dbr_drop_recov_wq = NULL;
	}
	rdev->dbr_drop_recov = false;
}

static int bnxt_re_initialize_dbr_pacing(struct bnxt_re_dev *rdev)
{
	int rc;

	/* Allocate a page for app use */
	rdev->dbr_pacing_page = (void *)__get_free_page(GFP_KERNEL);
	if (!rdev->dbr_pacing_page) {
		dev_err(rdev_to_dev(rdev), "DBR page allocation failed!");
		return -ENOMEM;
	}
	memset((u8 *)rdev->dbr_pacing_page, 0, PAGE_SIZE);
	rdev->qplib_res.pacing_data = (struct bnxt_qplib_db_pacing_data *)rdev->dbr_pacing_page;
	rc = bnxt_re_hwrm_dbr_pacing_qcfg(rdev);
	if (rc) {
		dev_err(rdev_to_dev(rdev),
			"Failed to query dbr pacing config %d\n", rc);
		goto fail;
	}
	/* Create a work queue for scheduling dbq event */
	rdev->dbq_wq = create_singlethread_workqueue("bnxt_re_dbq");
	if (!rdev->dbq_wq) {
		dev_err(rdev_to_dev(rdev), "DBQ wq alloc failed!");
		rc = -ENOMEM;
		goto fail;
	}
	/* MAP grc window 2 for reading db fifo depth */
	writel(rdev->chip_ctx->dbr_stat_db_fifo & BNXT_GRC_BASE_MASK,
	       rdev->en_dev->bar0 + BNXT_GRCPF_REG_WINDOW_BASE_OUT + 4);
	rdev->dbr_db_fifo_reg_off =
		(rdev->chip_ctx->dbr_stat_db_fifo & BNXT_GRC_OFFSET_MASK) +
		0x2000;
	rdev->qplib_res.pacing_data->grc_reg_offset = rdev->dbr_db_fifo_reg_off;

	rdev->dbr_pacing_bar =
		pci_resource_start(rdev->qplib_res.pdev, 0) +
		rdev->dbr_db_fifo_reg_off;

	/* Percentage of DB FIFO */
	rdev->dbq_watermark = BNXT_RE_PACING_DBQ_THRESHOLD;
	rdev->pacing_en_int_th = BNXT_RE_PACING_EN_INT_THRESHOLD;
	rdev->pacing_algo_th = BNXT_RE_PACING_ALGO_THRESHOLD(rdev->chip_ctx);
	rdev->dbq_pacing_time = BNXT_RE_DBR_INT_TIME;
	rdev->dbr_def_do_pacing = BNXT_RE_DBR_DO_PACING_NO_CONGESTION;
	rdev->do_pacing_save = rdev->dbr_def_do_pacing;
	bnxt_re_set_default_pacing_data(rdev);
	dev_dbg(rdev_to_dev(rdev), "Initialized db pacing\n");

	return 0;
fail:
	free_page((u64)rdev->dbr_pacing_page);
	rdev->dbr_pacing_page = NULL;
	return rc;
}

static void bnxt_re_deinitialize_dbr_pacing(struct bnxt_re_dev *rdev)
{
	if (rdev->dbq_wq)
		flush_workqueue(rdev->dbq_wq);

	cancel_work_sync(&rdev->dbq_fifo_check_work);
	cancel_delayed_work_sync(&rdev->dbq_pacing_work);

	if (rdev->dbq_wq) {
		destroy_workqueue(rdev->dbq_wq);
		rdev->dbq_wq = NULL;
	}

	if (rdev->dbr_pacing_page)
		free_page((u64)rdev->dbr_pacing_page);
	rdev->dbr_pacing_page = NULL;
	rdev->dbr_pacing = false;
}

/* enable_dbr_pacing needs to be done only for older FWs
 * where host selects primary function. ie. pacing_ext
 * flags is not set.
 */
int bnxt_re_enable_dbr_pacing(struct bnxt_re_dev *rdev)
{
	struct bnxt_qplib_nq *nq;

	nq = &rdev->nqr->nq[0];
	rdev->dbq_nq_id = nq->ring_id;

	if (!bnxt_qplib_dbr_pacing_ext_en(rdev->chip_ctx) &&
	    !bnxt_qplib_dbr_pacing_chip_p7_en(rdev->chip_ctx) &&
	    bnxt_qplib_dbr_pacing_is_primary_pf(rdev->chip_ctx)) {
		if (bnxt_re_hwrm_dbr_pacing_cfg(rdev, true))
			return -EIO;

		/* MAP grc window 8 for ARMing the NQ DBQ */
		writel(rdev->chip_ctx->dbr_aeq_arm_reg &
		       BNXT_GRC_BASE_MASK,
		       rdev->en_dev->bar0 + BNXT_GRCPF_REG_WINDOW_BASE_OUT + 28);
		rdev->dbr_aeq_arm_reg_off =
		(rdev->chip_ctx->dbr_aeq_arm_reg &
		 BNXT_GRC_OFFSET_MASK) + 0x8000;
		writel(1, rdev->en_dev->bar0 + rdev->dbr_aeq_arm_reg_off);
	}

	return 0;
}

/* disable_dbr_pacing needs to be done only for older FWs
 * where host selects primary function. ie. pacing_ext
 * flags is not set.
 */

int bnxt_re_disable_dbr_pacing(struct bnxt_re_dev *rdev)
{
	int rc = 0;

	if (!bnxt_qplib_dbr_pacing_ext_en(rdev->chip_ctx) &&
	    !bnxt_qplib_dbr_pacing_chip_p7_en(rdev->chip_ctx) &&
	    bnxt_qplib_dbr_pacing_is_primary_pf(rdev->chip_ctx))
		rc = bnxt_re_hwrm_dbr_pacing_cfg(rdev, false);

	return rc;
}

static void bnxt_re_uninit_mmap_work(struct bnxt_re_dev *rdev)
{
	if (delayed_work_pending(&rdev->peer_mmap_work))
		cancel_delayed_work_sync(&rdev->peer_mmap_work);
}

static void bnxt_re_clean_qpdump(struct bnxt_re_dev *rdev)
{
	struct qdump_array *qdump;
	int i;

	if (!rdev->qdump_head.qdump)
		return;

	mutex_lock(&rdev->qdump_head.lock);
	for (i = 0; i < rdev->qdump_head.max_elements; i++) {
		qdump = &rdev->qdump_head.qdump[i];
		if (qdump->valid) {
			bnxt_re_free_qpdump(&qdump->sqd);
			bnxt_re_free_qpdump(&qdump->rqd);
			bnxt_re_free_qpdump(&qdump->scqd);
			bnxt_re_free_qpdump(&qdump->rcqd);
			bnxt_re_free_qpdump(&qdump->mrd);
			qdump->valid = false;
			qdump->is_mr = false;
		}
	}
	mutex_unlock(&rdev->qdump_head.lock);

	vfree(rdev->qdump_head.qdump);
	vfree(rdev->qp_dump_buf);
	rdev->qdump_head.index = 0;
}

static void bnxt_re_clean_udcc_dump(struct bnxt_re_dev *rdev)
{
	vfree(rdev->udcc_data_head.udcc_data_arr);
	rdev->udcc_data_head.udcc_data_arr = NULL;
}

/* bnxt_re_ib_uninit -	Uninitialize from IB stack
 * @rdev     -   rdma device instance
 *
 * If Firmware is responding, stop user qps (moving qps to error state.).
 * If Firmware is not responding (error case), start drain kernel qps
 * Eventually, unregister device with IB stack.
 *
 */
void bnxt_re_ib_uninit(struct bnxt_re_dev *rdev)
{
	if (test_bit(BNXT_RE_FLAG_IBDEV_REGISTERED, &rdev->flags)) {
		bnxt_re_stop_user_qps_nonfatal(rdev);
		bnxt_re_drain_kernel_qps_fatal(rdev);
		bnxt_re_sysfs_destroy_file(rdev);
		ib_unregister_device(&rdev->ibdev);
		clear_bit(BNXT_RE_FLAG_IBDEV_REGISTERED, &rdev->flags);
		return;
	}
}

/* When DEL_GID fails, driver is not freeing GID ctx memory.
 * To avoid the memory leak, free the memory during unload
 */
static void bnxt_re_free_gid_ctx(struct bnxt_re_dev *rdev)
{
	struct bnxt_qplib_sgid_tbl *sgid_tbl = &rdev->qplib_res.sgid_tbl;
	struct bnxt_re_gid_ctx *ctx, **ctx_tbl;
	int i;

	if (!sgid_tbl->active)
		return;

	ctx_tbl = sgid_tbl->ctx;
	for (i = 0; i < sgid_tbl->max; i++) {
		if (sgid_tbl->hw_id[i] == 0xFFFF)
			continue;

		ctx = ctx_tbl[i];
		kfree(ctx);
	}
}

static void bnxt_re_dev_uninit(struct bnxt_re_dev *rdev, u8 op_type)
{
	struct bnxt_qplib_dpi *kdpi;
	int rc, wait_count = BNXT_RE_RES_FREE_WAIT_COUNT;

	if (test_and_clear_bit(BNXT_RE_FLAG_WORKER_REG, &rdev->flags))
		cancel_delayed_work_sync(&rdev->worker);

	bnxt_re_net_unregister_async_event(rdev);

#ifdef IB_PEER_MEM_MOD_SUPPORT
	if (rdev->peer_dev) {
		ib_peer_mem_remove_device(rdev->peer_dev);
		rdev->peer_dev = NULL;
	}
#endif

	bnxt_re_put_stats3_ctx(rdev);
	bnxt_re_put_stats2_ctx(rdev);
	if (test_and_clear_bit(BNXT_RE_FLAG_DEV_LIST_INITIALIZED,
			       &rdev->flags)) {
		/* did the caller hold the lock? */
		list_del_rcu(&rdev->list);
	}

	bnxt_re_uninit_dest_ah_wq(rdev);
	bnxt_re_uninit_resolve_wq(rdev);
	bnxt_re_uninit_dcb_wq(rdev);
	bnxt_re_uninit_aer_wq(rdev);
	bnxt_re_uninit_udcc_wq(rdev);
	bnxt_re_uninit_mmap_work(rdev);
	bnxt_re_deinitialize_dbr_drop_recov(rdev);

	if (bnxt_qplib_dbr_pacing_en(rdev->chip_ctx))
		(void)bnxt_re_disable_dbr_pacing(rdev);

	if (test_and_clear_bit(BNXT_RE_FLAG_PER_PORT_DEBUG_INFO, &rdev->flags))
		bnxt_re_debugfs_rem_port(rdev);

	bnxt_re_debugfs_rem_pdev(rdev);

	/* Wait for ULPs to release references */
	while (atomic_read(&rdev->stats.rsors.cq_count) && --wait_count)
		usleep_range(500, 1000);
	if (!wait_count)
		dev_err(rdev_to_dev(rdev),
			"CQ resources not freed by stack, count = 0x%x",
			atomic_read(&rdev->stats.rsors.cq_count));

	kdpi = &rdev->dpi_privileged;
	if (kdpi->umdbr) /* kernel DPI was allocated with success */
		bnxt_qplib_dealloc_kernel_dpi(kdpi);

	bnxt_re_hdbr_uninit(rdev);

	/* Protect the device uninitialization and start_irq/stop_irq L2
	 * callbacks with rtnl lock to avoid race condition between these calls
	 */
	rtnl_lock();
	if (test_and_clear_bit(BNXT_RE_FLAG_SETUP_NQ, &rdev->flags))
		bnxt_re_clean_nqs(rdev);
	rtnl_unlock();

	bnxt_re_deinit_countersets(rdev);

	bnxt_re_free_gid_ctx(rdev);
	if (test_and_clear_bit(BNXT_RE_FLAG_TBLS_ALLOCINIT, &rdev->flags))
		bnxt_re_free_tbls(rdev);
	if (test_and_clear_bit(BNXT_RE_FLAG_RCFW_CHANNEL_INIT, &rdev->flags)) {
		rc = bnxt_qplib_deinit_rcfw(&rdev->rcfw);
		if (rc)
			dev_warn(rdev_to_dev(rdev),
				 "Failed to deinitialize fw, rc = 0x%x", rc);
	}

	bnxt_re_put_stats_ctx(rdev);

	if (test_and_clear_bit(BNXT_RE_FLAG_ALLOC_CTX, &rdev->flags))
		bnxt_qplib_free_hwctx(&rdev->qplib_res);

	rtnl_lock();
	if (test_and_clear_bit(BNXT_RE_FLAG_RCFW_CHANNEL_EN, &rdev->flags))
		bnxt_qplib_disable_rcfw_channel(&rdev->rcfw);

	if (rdev->dbr_pacing)
		bnxt_re_deinitialize_dbr_pacing(rdev);

	if (test_and_clear_bit(BNXT_RE_FLAG_NET_RING_ALLOC, &rdev->flags))
		bnxt_re_net_ring_free(rdev, rdev->rcfw.creq.ring_id);

	rdev->nqr->num_msix = 0;
	rtnl_unlock();

	bnxt_re_free_dbr_sw_stats_mem(rdev);

	if (test_and_clear_bit(BNXT_RE_FLAG_ALLOC_RCFW, &rdev->flags))
		bnxt_qplib_free_rcfw_channel(&rdev->qplib_res);

	bnxt_re_free_nqr_mem(rdev);
	bnxt_re_destroy_chip_ctx(rdev);
	bnxt_re_clean_qpdump(rdev);
	bnxt_re_clean_udcc_dump(rdev);

	if (op_type != BNXT_RE_PRE_RECOVERY_REMOVE) {
		if (test_and_clear_bit(BNXT_RE_FLAG_NETDEV_REGISTERED,
				       &rdev->flags))
			bnxt_unregister_dev(rdev->en_dev);
	}
}

static int bnxt_re_hwrm_udcc_qcaps(struct bnxt_re_dev *rdev)
{
	struct bnxt_re_udcc_cfg *udcc = &rdev->udcc_cfg;
	struct hwrm_udcc_qcaps_output resp = {};
	struct hwrm_func_qcaps_input req = {};
	struct bnxt_fw_msg fw_msg = {};
	int rc;

	if (rdev->is_virtfn || !bnxt_qplib_udcc_supported(rdev->chip_ctx))
		return 0;

	bnxt_re_init_hwrm_hdr((void *)&req, HWRM_UDCC_QCAPS, -1);
	bnxt_re_fill_fw_msg(&fw_msg, (void *)&req, sizeof(req), (void *)&resp,
			    sizeof(resp), BNXT_RE_HWRM_CMD_TIMEOUT(rdev));
	req.fid = cpu_to_le16(0xffff);
	rc = bnxt_send_msg(rdev->en_dev, &fw_msg);
	if (rc) {
		dev_dbg(rdev_to_dev(rdev), "Failed to query udcc caps rc:%d\n", rc);
		return rc;
	}

	udcc->max_sessions = le16_to_cpu(resp.max_sessions);
	udcc->min_sessions = le16_to_cpu(resp.min_sessions);
	udcc->max_comp_cfg_xfer = le16_to_cpu(resp.max_comp_cfg_xfer);
	udcc->max_comp_data_xfer = le16_to_cpu(resp.max_comp_data_xfer);
	udcc->flags = le16_to_cpu(resp.flags);
	udcc->rtt_num_of_buckets = resp.rtt_num_of_buckets;
	udcc->session_type = resp.session_type;


	dev_dbg(rdev_to_dev(rdev),
		"udcc: max_sessions:%d, max_comp_cfg_xfer: %d, max_comp_data_xfer:%d\n",
		udcc->max_sessions, udcc->max_comp_cfg_xfer, udcc->max_comp_data_xfer);

	return 0;
}

static int bnxt_re_init_qpdump(struct bnxt_re_dev *rdev)
{
	rdev->qdump_head.max_elements = CTXM_DATA_INDEX_MAX;
	rdev->qdump_head.index = 0;
	rdev->snapdump_dbg_lvl = BNXT_RE_SNAPDUMP_ERR;
	mutex_init(&rdev->qdump_head.lock);
	rdev->qdump_head.qdump = vzalloc(rdev->qdump_head.max_elements *
					 sizeof(struct qdump_array));
	if (!rdev->qdump_head.qdump)
		return -ENOMEM;

	rdev->qp_dump_buf = vzalloc(BNXT_RE_LIVE_QPDUMP_SZ);
	if (!rdev->qp_dump_buf) {
		vfree(rdev->qdump_head.qdump);
		rdev->qdump_head.qdump = NULL;
		return -ENOMEM;
	}

	return 0;
}

static void bnxt_re_init_udcc_dump(struct bnxt_re_dev *rdev)
{
	int index;

	if (!is_roce_udcc_session_destroy_sb_enabled(rdev->qplib_res.dattr->dev_cap_ext_flags2))
		return;

	rdev->udcc_data_head.max_elements = BNXT_RE_UDCC_MAX_SESSIONS;
	rdev->udcc_data_head.index = 0;
	mutex_init(&rdev->udcc_data_head.lock);
	rdev->udcc_data_head.udcc_data_arr = vzalloc(rdev->udcc_data_head.max_elements *
						     rdev->rcfw.udcc_session_sb_sz);

	for (index = 0; index < rdev->udcc_data_head.max_elements; index++)
		rdev->udcc_data_head.ses[index].udcc_data =
				(rdev->udcc_data_head.udcc_data_arr +
				(index * rdev->rcfw.udcc_session_sb_sz));
}

int bnxt_re_init_countersets(struct bnxt_re_dev *rdev)
{
	int rc;

	if (!bnxt_ext_stats_v2_supported(rdev->dev_attr->dev_cap_ext_flags))
		return 0;

	bnxt_qplib_init_stats_ext_ctx(&rdev->qplib_res);

	rc = bnxt_qplib_alloc_stats_ext_ctx(&rdev->qplib_res);
	if (rc)
		return -ENOMEM;

	return rc;
}

void bnxt_re_deinit_countersets(struct bnxt_re_dev *rdev)
{
	if (!bnxt_ext_stats_v2_supported(rdev->dev_attr->dev_cap_ext_flags))
		return;

	bnxt_qplib_dealloc_stats_ext_ctx(&rdev->qplib_res);
}

static int bnxt_re_dev_init(struct bnxt_re_dev *rdev, u8 op_type, u8 wqe_mode, bool is_primary)
{
	struct bnxt_re_ring_attr rattr = {};
	struct bnxt_qplib_creq_ctx *creq;
	struct bnxt_re_tc_rec *tc_rec;
	int vec, offset;
	int rc = 0;

	if (op_type != BNXT_RE_POST_RECOVERY_INIT) {
		/* Registered a new RoCE device instance to netdev */
		rc = bnxt_re_register_netdev(rdev);
		if (rc)
			return -EINVAL;
	}
	set_bit(BNXT_RE_FLAG_NETDEV_REGISTERED, &rdev->flags);

	if (rdev->en_dev->ulp_tbl->msix_requested < BNXT_RE_MIN_MSIX) {
		dev_err(rdev_to_dev(rdev),
			"RoCE requires minimum 2 MSI-X vectors, but only %d reserved\n",
			rdev->en_dev->ulp_tbl->msix_requested);
		bnxt_unregister_dev(rdev->en_dev);
		clear_bit(BNXT_RE_FLAG_NETDEV_REGISTERED, &rdev->flags);
		return -EINVAL;
	}
	dev_dbg(rdev_to_dev(rdev), "Got %d MSI-X vectors\n",
		rdev->en_dev->ulp_tbl->msix_requested);

	bnxt_re_get_sriov_func_type(rdev);

	rc = bnxt_re_setup_chip_ctx(rdev, wqe_mode);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to get chip context rc 0x%x", rc);
		bnxt_unregister_dev(rdev->en_dev);
		clear_bit(BNXT_RE_FLAG_NETDEV_REGISTERED, &rdev->flags);
		rc = -EINVAL;
		return rc;
	}

	rc = bnxt_re_alloc_nqr_mem(rdev);
	if (rc) {
		bnxt_re_destroy_chip_ctx(rdev);
		bnxt_unregister_dev(rdev->en_dev);
		clear_bit(BNXT_RE_FLAG_NETDEV_REGISTERED, &rdev->flags);
		return rc;
	}

	/* Protect the device initialization and start_irq/stop_irq L2 callbacks
	 * with rtnl lock to avoid race condition between these calls
	 */
	rtnl_lock();
	rdev->nqr->num_msix = rdev->en_dev->ulp_tbl->msix_requested;
	memcpy(rdev->nqr->msix_entries, rdev->en_dev->msix_entries,
	       sizeof(struct bnxt_msix_entry) * rdev->nqr->num_msix);

	/* Establish RCFW Communication Channel to initialize the context
	   memory for the function and all child VFs */
	rc = bnxt_qplib_alloc_rcfw_channel(&rdev->qplib_res);
	if (rc) {
		dev_err(rdev_to_dev(rdev),
			"Failed to alloc mem for rcfw, rc = %#x\n", rc);
		goto release_rtnl;
	}
	set_bit(BNXT_RE_FLAG_ALLOC_RCFW, &rdev->flags);

	creq = &rdev->rcfw.creq;
	rattr.dma_arr = creq->hwq.pbl[PBL_LVL_0].pg_map_arr;
	rattr.pages = creq->hwq.pbl[creq->hwq.level].pg_count;
	rattr.type = bnxt_re_get_rtype(rdev);
	rattr.mode = RING_ALLOC_REQ_INT_MODE_MSIX;
	rattr.depth = BNXT_QPLIB_CREQE_MAX_CNT - 1;
	rattr.lrid = rdev->nqr->msix_entries[BNXT_RE_AEQ_IDX].ring_idx;
	rc = bnxt_re_net_ring_alloc(rdev, &rattr, &creq->ring_id);
	if (rc) {
		creq->ring_id = 0xffff;
		dev_err(rdev_to_dev(rdev),
			"Failed to allocate CREQ fw id with rc = 0x%x", rc);
		goto release_rtnl;
	}
	set_bit(BNXT_RE_FLAG_NET_RING_ALLOC, &rdev->flags);

	/* Program the NQ ID for DBQ notification */
	if (bnxt_qplib_dbr_pacing_chip_p7_en(rdev->chip_ctx) ||
	    bnxt_qplib_dbr_pacing_en(rdev->chip_ctx) ||
	    bnxt_qplib_dbr_pacing_ext_en(rdev->chip_ctx)) {
		rc = bnxt_re_initialize_dbr_pacing(rdev);
		if (!rc)
			rdev->dbr_pacing = true;
		else
			rdev->dbr_pacing = false;
		dev_dbg(rdev_to_dev(rdev), "%s: initialize db pacing ret %d\n",
			__func__, rc);
	}

	vec = rdev->nqr->msix_entries[BNXT_RE_AEQ_IDX].vector;
	offset = rdev->nqr->msix_entries[BNXT_RE_AEQ_IDX].db_offset;
	rc = bnxt_qplib_enable_rcfw_channel(&rdev->rcfw, vec, offset,
					    &bnxt_re_aeq_handler);
	if (rc) {
		dev_err(rdev_to_dev(rdev),
			"Failed to enable RCFW channel with rc = 0x%x", rc);
		goto release_rtnl;
	}
	set_bit(BNXT_RE_FLAG_RCFW_CHANNEL_EN, &rdev->flags);

	rc = bnxt_re_update_dev_attr(rdev);
	if (rc)
		goto release_rtnl;
	bnxt_re_limit_pf_res(rdev);
	if (!rdev->is_virtfn && !_is_chip_p5_plus(rdev->chip_ctx)) {
		rc = bnxt_qplib_alloc_hwctx(&rdev->qplib_res);
		if (rc) {
			dev_err(rdev_to_dev(rdev),
				"Failed to alloc hw contexts, rc = 0x%x", rc);
			goto release_rtnl;
		}
		set_bit(BNXT_RE_FLAG_ALLOC_CTX, &rdev->flags);
	}

	rc = bnxt_re_get_stats_ctx(rdev);
	if (rc)
		goto release_rtnl;

	rc = bnxt_qplib_init_rcfw(&rdev->rcfw, rdev->is_virtfn);
	if (rc) {
		dev_err(rdev_to_dev(rdev),
			"Failed to initialize fw with rc = 0x%x", rc);
		goto release_rtnl;
	}
	set_bit(BNXT_RE_FLAG_RCFW_CHANNEL_INIT, &rdev->flags);

	/* Based resource count on the 'new' device caps */
	rc = bnxt_re_update_dev_attr(rdev);
	if (rc)
		goto release_rtnl;
	rc = bnxt_re_alloc_init_tbls(rdev);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "tbls alloc-init failed rc = %#x",
			rc);
		goto release_rtnl;
	}
	rc = bnxt_re_setup_nqs(rdev);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "NQs alloc-init failed rc = %#x\n",
			rc);
		if (rdev->nqr->max_init == 0)
			goto release_rtnl;

		dev_warn(rdev_to_dev(rdev),
			"expected nqs %d available nqs %d\n",
			rdev->nqr->num_msix, rdev->nqr->max_init);
	}
	set_bit(BNXT_RE_FLAG_SETUP_NQ, &rdev->flags);
	rtnl_unlock();

	bnxt_qplib_alloc_kernel_dpi(&rdev->qplib_res, &rdev->dpi_privileged);

	rc = bnxt_re_hdbr_init(rdev);
	if (rc)
		goto fail;

	if (rdev->dbr_pacing)
		bnxt_re_enable_dbr_pacing(rdev);

	if (rdev->chip_ctx->modes.dbr_drop_recov)
		bnxt_re_initialize_dbr_drop_recov(rdev);

	rc = bnxt_re_alloc_dbr_sw_stats_mem(rdev);
	if (rc)
		goto fail;

	tc_rec = &rdev->tc_rec[0];
	rc =  bnxt_re_query_hwrm_qportcfg(rdev, tc_rec, 0xFFFF);
	if (rc) {
		dev_err(rdev_to_dev(rdev),
			"Failed to query port config rc:%d", rc);
		return rc;
	}

	if (rdev->binfo) {
		tc_rec = &rdev->tc_rec[1];
		rc =  bnxt_re_query_hwrm_qportcfg(rdev, tc_rec, 2);
		if (rc) {
			dev_err(rdev_to_dev(rdev),
				"Failed to query port config(LAG) rc:%d", rc);
			return rc;
		}
	}
	/* Query f/w defaults of CC params */
	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &rdev->cc_param);
	if (rc)
		dev_warn(rdev_to_dev(rdev),
			 "Failed to query CC defaults\n");

	/* This block of code is needed for error recovery support */
	if (!rdev->is_virtfn) {
		if (!_is_def_cc_param_supported(rdev->dev_attr->dev_cap_ext_flags2)) {
			rc = bnxt_re_init_cc_param(rdev);
			if (rc)
				dev_err(rdev_to_dev(rdev), "Failed to initialize Flow control");
			else
				set_bit(BNXT_RE_FLAG_INIT_CC_PARAM, &rdev->flags);
		}

		if (_is_chip_p5_plus(rdev->chip_ctx) &&
		    !(rdev->qplib_res.en_dev->flags & BNXT_EN_FLAG_ROCE_VF_RES_MGMT))
			bnxt_re_vf_res_config(rdev);
	}
	INIT_DELAYED_WORK(&rdev->worker, bnxt_re_worker);
	set_bit(BNXT_RE_FLAG_WORKER_REG, &rdev->flags);
	schedule_delayed_work(&rdev->worker, msecs_to_jiffies(1000));

	bnxt_re_init_dest_ah_wq(rdev);
	bnxt_re_init_dcb_wq(rdev);
	bnxt_re_init_aer_wq(rdev);
	bnxt_re_init_resolve_wq(rdev);
	bnxt_re_init_udcc_wq(rdev);
	bnxt_re_hwrm_udcc_qcaps(rdev);

	if (is_primary)
		bnxt_re_debugfs_add_pdev(rdev);

	list_add_tail_rcu(&rdev->list, &bnxt_re_dev_list);
	set_bit(BNXT_RE_FLAG_DEV_LIST_INITIALIZED, &rdev->flags);

	rc = bnxt_re_get_stats2_ctx(rdev);
	if (rc)
		goto fail;

	rc = bnxt_re_get_stats3_ctx(rdev);
	if (rc)
		goto fail;

	if (_is_chip_p5_plus(rdev->chip_ctx)) {
		bnxt_re_init_qpdump(rdev);
		bnxt_re_init_udcc_dump(rdev);
	}

	hash_init(rdev->cq_hash);
	if (rdev->chip_ctx->modes.toggle_bits & BNXT_QPLIB_SRQ_TOGGLE_BIT)
		hash_init(rdev->srq_hash);

	rc = bnxt_re_init_countersets(rdev);
	if (rc)
		goto fail;

	return rc;
release_rtnl:
	rtnl_unlock();
fail:
	bnxt_re_dev_uninit(rdev, BNXT_RE_COMPLETE_REMOVE);

	return rc;
}

static int bnxt_re_ib_init(struct bnxt_re_dev *rdev)
{
	int rc = 0;

	rc = bnxt_re_register_ib(rdev);
	if (rc) {
		dev_err(rdev_to_dev(rdev),
			"Register IB failed with rc = 0x%x", rc);
		return rc;
	}
	rc = bnxt_re_sysfs_create_file(rdev);
	if (rc) {
		bnxt_re_ib_uninit(rdev);
		return rc;
	}

	set_bit(BNXT_RE_FLAG_IBDEV_REGISTERED, &rdev->flags);
	set_bit(BNXT_RE_FLAG_ISSUE_ROCE_STATS, &rdev->flags);
	set_bit(BNXT_RE_FLAG_ISSUE_CFA_FLOW_STATS, &rdev->flags);
	bnxt_re_dispatch_event(&rdev->ibdev, NULL, 1, IB_EVENT_PORT_ACTIVE);
	bnxt_re_dispatch_event(&rdev->ibdev, NULL, 1, IB_EVENT_GID_CHANGE);

	return rc;
}

/* wrapper for ib_init funcs */
int _bnxt_re_ib_init(struct bnxt_re_dev *rdev)
{
	return bnxt_re_ib_init(rdev);
}

/* wrapper for aux init funcs */
int _bnxt_re_ib_init2(struct bnxt_re_dev *rdev)
{
	bnxt_re_ib_init_2(rdev);
	return 0; /* add return for future proof */
}

static void bnxt_re_dev_unreg(struct bnxt_re_dev *rdev)
{
	bnxt_re_dev_dealloc(rdev);
}

static int bnxt_re_check_bond_in_vf_parent(struct bnxt_en_dev *en_dev)
{
	struct bnxt_re_dev *rdev;
	struct pci_dev *physfn;

	physfn = pci_physfn(en_dev->pdev);
	rcu_read_lock();
	list_for_each_entry_rcu(rdev, &bnxt_re_dev_list, list) {
		if (rdev->binfo && BNXT_EN_VF(en_dev)) {
			if ((rdev->binfo->pdev1 == physfn) ||
			    (rdev->binfo->pdev2 == physfn)) {
				rcu_read_unlock();
				return 1;
			}
		}
	}
	rcu_read_unlock();
	return 0;
}

static int bnxt_re_dev_reg(struct bnxt_re_dev **rdev, struct net_device *netdev,
			   struct bnxt_en_dev *en_dev)
{
	dev_dbg(NULL, "%s: netdev = %p\n", __func__, netdev);

	/* For Thor2, VF ROCE is not dependent on the PF Lag*/
	if (!BNXT_RE_CHIP_P7(en_dev->chip_num) &&
	    bnxt_re_check_bond_in_vf_parent(en_dev)) {
		dev_err(NULL, "RoCE disabled, LAG is configured on PFs\n");
		return -EINVAL;
	}

	/*
	 * Note:
	 * The first argument to bnxt_re_dev_alloc() is 'netdev' and
	 * not 'realdev', since in the case of bonding we want to
	 * register the bonded virtual netdev (master) to the ib stack.
	 * And 'en_dev' (for L2/PCI communication) is the first slave
	 * device (PF0 on the card).
	 * In the case of a regular netdev, both netdev and the en_dev
	 * correspond to the same device.
	 */
	*rdev = bnxt_re_dev_alloc(netdev, en_dev);
	if (!*rdev) {
		dev_err(NULL, "%s: netdev %p not handled",
			ROCE_DRV_MODULE_NAME, netdev);
		return -ENOMEM;
	}
	bnxt_re_hold(*rdev);

	return 0;
}

static void bnxt_re_update_speed_for_bond(struct bnxt_re_dev *rdev)
{
	if (rdev->binfo->aggr_mode !=
	    CMDQ_SET_LINK_AGGR_MODE_AGGR_MODE_ACTIVE_BACKUP)
		rdev->espeed *= 2;
}

void bnxt_re_get_link_speed(struct bnxt_re_dev *rdev)
{
#ifdef HAVE_ETHTOOL_GLINKSETTINGS_25G
	struct ethtool_link_ksettings lksettings;
#else
	struct ethtool_cmd ecmd;
#endif
	/* Using physical netdev for getting speed
	 * in case of bond devices.
	 * No change for normal interface.
	 */
	struct net_device *netdev = rdev->en_dev->net;

#ifdef HAVE_ETHTOOL_GLINKSETTINGS_25G
	if (netdev->ethtool_ops && netdev->ethtool_ops->get_link_ksettings) {
		memset(&lksettings, 0, sizeof(lksettings));
		netdev->ethtool_ops->get_link_ksettings(netdev, &lksettings);
		rdev->espeed = lksettings.base.speed;
#ifdef HAVE_ETHTOOL_LINK_KSETTINGS_LANES
		rdev->lanes = lksettings.lanes;
#endif
	}
#else
	if (netdev->ethtool_ops && netdev->ethtool_ops->get_settings) {
		memset(&ecmd, 0, sizeof(ecmd));
		netdev->ethtool_ops->get_settings(netdev, &ecmd);
		rdev->espeed = ecmd.speed;
	}
#endif
	/* Store link speed as sl_espeed. espeed can change for bond device */
	rdev->sl_espeed = rdev->espeed;
	if (rdev->binfo)
		bnxt_re_update_speed_for_bond(rdev);
}

static bool bnxt_re_bond_update_reqd(struct netdev_bonding_info *netdev_binfo,
				     struct bnxt_re_bond_info *binfo,
				     struct bnxt_re_dev *rdev,
				     struct net_device *sl_netdev)
{
	int rc = 0;
	struct bnxt_re_bond_info tmp_binfo;

	memcpy(&tmp_binfo, binfo, sizeof(*binfo));

	rc = bnxt_re_get_port_map(netdev_binfo, binfo, sl_netdev);
	if (rc) {
		dev_dbg(rdev_to_dev(binfo->rdev),
			"%s: Error in receiving port state rdev = %p",
			__func__, binfo->rdev);
		return false;
	}

	if (binfo->aggr_mode == tmp_binfo.aggr_mode &&
	    binfo->active_port_map == tmp_binfo.active_port_map) {
		dev_dbg(rdev_to_dev(binfo->rdev),
			"%s: No need to update rdev=%p active_port_map=%#x",
			__func__, binfo->rdev, binfo->active_port_map);
		return false;
	}

	return true;
}

static int bnxt_re_update_fw_lag_info(struct bnxt_re_bond_info *binfo,
			       struct bnxt_re_dev *rdev,
			       bool aggr_en)
{
	struct bnxt_qplib_ctx *hctx;
	int rc = 0;

	dev_dbg(rdev_to_dev(binfo->rdev), "%s: port_map for rdev=%p bond=%#x",
		__func__, binfo->rdev, binfo->active_port_map);

	/* Send LAG info to BONO */
	hctx = rdev->qplib_res.hctx;
	if (aggr_en) {
		rc = bnxt_qplib_set_link_aggr_mode(&binfo->rdev->qplib_res,
						   binfo->aggr_mode,
						   BNXT_RE_MEMBER_PORT_MAP,
						   binfo->active_port_map,
						   aggr_en, hctx->stats2.fw_id);
		if (rc)
			dev_err(rdev_to_dev(binfo->rdev),
				"%s: setting link aggr mode rc = %d\n", __func__, rc);

		dev_info(rdev_to_dev(binfo->rdev),
			 "binfo->aggr_mode = %d binfo->active_port_map = 0x%x\n",
			 binfo->aggr_mode, binfo->active_port_map);
	} else {
		rc = bnxt_qplib_set_link_aggr_mode(&binfo->rdev->qplib_res,
						   0, 0, 0,
						   aggr_en, hctx->stats2.fw_id);
		if (rc)
			dev_err(rdev_to_dev(binfo->rdev),
				"%s: disable link aggr rc = %d\n", __func__, rc);
	}

	return rc;
}

void bnxt_re_remove_device(struct bnxt_re_dev *rdev, u8 op_type,
			   struct auxiliary_device *aux_dev)
{
	struct bnxt_re_en_dev_info *en_info;
	struct bnxt_qplib_cmdq_ctx *cmdq;
	struct bnxt_qplib_rcfw *rcfw;

	rcfw = &rdev->rcfw;
	cmdq = &rcfw->cmdq;
	if (test_bit(FIRMWARE_STALL_DETECTED, &cmdq->flags))
		set_bit(BNXT_RE_FLAG_ERR_DEVICE_DETACHED, &rdev->flags);

	dev_dbg(rdev_to_dev(rdev), "%s: Removing rdev: %p op_type %d\n",
		__func__, rdev, op_type);

	if (op_type != BNXT_RE_PRE_RECOVERY_REMOVE) {
		if (test_and_clear_bit(BNXT_RE_FLAG_INIT_CC_PARAM, &rdev->flags))
			bnxt_re_clear_cc_param(rdev);
	}
	bnxt_re_dev_uninit(rdev, op_type);
	en_info = auxiliary_get_drvdata(aux_dev);
	if (en_info) {
		rtnl_lock();
		en_info->rdev = NULL;
		rtnl_unlock();
		if (op_type != BNXT_RE_PRE_RECOVERY_REMOVE) {
			clear_bit(BNXT_RE_FLAG_EN_DEV_PRIMARY_DEV, &en_info->flags);
			clear_bit(BNXT_RE_FLAG_EN_DEV_SECONDARY_DEV, &en_info->flags);
			clear_bit(BNXT_RE_FLAG_EN_DEV_NETDEV_REG, &en_info->flags);
		}
	}
	bnxt_re_dev_unreg(rdev);
}

static void bnxt_re_update_en_info_rdev(struct bnxt_re_dev *rdev,
					struct bnxt_re_en_dev_info *en_info,
					struct bnxt_re_bond_info *binfo)
{
	struct bnxt_en_dev *en_dev = en_info->en_dev;
	struct bnxt_re_en_dev_info *en_info2 = NULL;

	/* Before updating the rdev pointer in bnxt_re_en_dev_info structure,
	 * take the rtnl lock to avoid accessing invalid rdev pointer from
	 * L2 ULP callbacks. This is applicable in all the places where rdev
	 * pointer is updated in bnxt_re_en_dev_info.
	 */
	rtnl_lock();
	en_info->rdev = rdev;
	/*
	 * If this is a bond interface, update second aux_dev's
	 * en_info->rdev also with this newly created rdev
	 */
	if (binfo && !BNXT_EN_HW_LAG(en_dev)) {
		if (binfo->aux_dev2)
			en_info2 = auxiliary_get_drvdata(binfo->aux_dev2);

		if (en_info2)
			en_info2->rdev = rdev;
	}
	rtnl_unlock();
}

int bnxt_re_add_device(struct bnxt_re_dev **rdev,
		       struct net_device *netdev,
		       struct bnxt_re_bond_info *info,
		       u8 qp_mode, u8 op_type, u8 wqe_mode,
		       struct auxiliary_device *aux_dev,
		       bool is_primary)
{
	struct bnxt_re_en_dev_info *en_info;
	struct bnxt_en_dev *en_dev;
	int rc = 0;

	en_info = auxiliary_get_drvdata(aux_dev);
	en_dev = en_info->en_dev;

	rc = bnxt_re_dev_reg(rdev, netdev, en_dev);
	if (rc) {
		dev_dbg(NULL, "Failed to create add device for netdev %p\n",
			netdev);
		return rc;
	}

	/* Set Bonding info for handling bond devices */
	(*rdev)->binfo = info;
	/* Inherit gsi_qp_mode only if info is valid
	 * and qp_mode specified is invalid
	 */
	(*rdev)->gsi_ctx.gsi_qp_mode = (info && !qp_mode) ?
					info->gsi_qp_mode : qp_mode;
	wqe_mode = (info && wqe_mode == BNXT_QPLIB_WQE_MODE_INVALID) ?
		    info->wqe_mode : wqe_mode;
	(*rdev)->adev = aux_dev;
	bnxt_re_update_en_info_rdev(*rdev, en_info, info);
	rc = bnxt_re_dev_init(*rdev, op_type, wqe_mode, is_primary);
	if (rc) {
		bnxt_re_dev_unreg(*rdev);
		*rdev = NULL;
		bnxt_re_update_en_info_rdev(*rdev, en_info, info);
		return rc;
	}

	bnxt_re_get_bar_maps(*rdev);
	dev_dbg(rdev_to_dev(*rdev), "%s: Added rdev: %p\n", __func__, *rdev);
	set_bit(BNXT_RE_FLAG_EN_DEV_NETDEV_REG, &en_info->flags);
	return 0;
}

struct bnxt_re_dev *bnxt_re_get_peer_pf(struct bnxt_re_dev *rdev)
{
	struct pci_dev *pdev_in = rdev->en_dev->pdev;
	int tmp_bus_num, bus_num = pdev_in->bus->number;
	int tmp_dev_num, dev_num = PCI_SLOT(pdev_in->devfn);
	int tmp_func_num, func_num = PCI_FUNC(pdev_in->devfn);
	unsigned int domain_num = pci_domain_nr(pdev_in->bus);
	struct bnxt_re_dev *tmp_rdev;
	unsigned int tmp_domain_num;

	rcu_read_lock();
	list_for_each_entry_rcu(tmp_rdev, &bnxt_re_dev_list, list) {
		tmp_domain_num = pci_domain_nr(tmp_rdev->en_dev->pdev->bus);
		tmp_bus_num = tmp_rdev->en_dev->pdev->bus->number;
		tmp_dev_num = PCI_SLOT(tmp_rdev->en_dev->pdev->devfn);
		tmp_func_num = PCI_FUNC(tmp_rdev->en_dev->pdev->devfn);

		if (domain_num == tmp_domain_num && bus_num == tmp_bus_num &&
		    dev_num == tmp_dev_num && func_num != tmp_func_num) {
			rcu_read_unlock();
			return tmp_rdev;
		}
	}
	rcu_read_unlock();
	return NULL;
}

static u8 bnxt_re_get_bond_mode(struct bnxt_re_dev *rdev,
				struct netdev_bonding_info *netdev_binfo)
{
	if (BNXT_EN_HW_LAG(rdev->en_dev)) {
		switch (netdev_binfo->master.bond_mode) {
		case BOND_MODE_ACTIVEBACKUP:
			return CMDQ_SET_LINK_AGGR_MODE_AGGR_MODE_ACTIVE_BACKUP;
		case BOND_MODE_ROUNDROBIN:
			return CMDQ_SET_LINK_AGGR_MODE_AGGR_MODE_ACTIVE_ACTIVE;
		case BOND_MODE_XOR:
			return CMDQ_SET_LINK_AGGR_MODE_AGGR_MODE_BALANCE_XOR;
		case BOND_MODE_8023AD:
			return CMDQ_SET_LINK_AGGR_MODE_AGGR_MODE_802_3_AD;
		}
	}
	/* On older chips, Firmware supports only 2 modes */
	if (netdev_binfo->master.bond_mode == BOND_MODE_ACTIVEBACKUP)
		return CMDQ_SET_LINK_AGGR_MODE_AGGR_MODE_ACTIVE_BACKUP;
	return CMDQ_SET_LINK_AGGR_MODE_AGGR_MODE_ACTIVE_ACTIVE;
}

static struct bnxt_re_bond_info *bnxt_re_alloc_lag(struct netdev_bonding_info *netdev_binfo,
						   struct bnxt_re_dev *rdev,
						   u8 gsi_mode, u8 wqe_mode)
{
	struct bnxt_re_bond_info *info = NULL;
	struct bnxt_re_en_dev_info *en_info;
	struct bnxt_re_tc_rec *tc_rec;
	struct bnxt_re_dev *rdev_peer;
	struct net_device *master;
	int rc = 0;

	rtnl_lock();
	master = netdev_master_upper_dev_get(rdev->netdev);
	if (!master) {
		rtnl_unlock();
		return info;
	}

	/*
	 * Hold the netdev ref count till the LAG creation to avoid freeing.
	 * LAG creation is in scheduled task, and there is a possibility of
	 * bond getting removed simultaneously.
	 */
	dev_hold(master);
	rtnl_unlock();

	rdev_peer = bnxt_re_get_peer_pf(rdev);
	if (!rdev_peer)
		goto exit;

	dev_dbg(rdev_to_dev(rdev), "%s: Slave1 rdev: %p\n", __func__, rdev);
	dev_dbg(rdev_to_dev(rdev_peer), "%s: Slave2 rdev: %p\n", __func__, rdev_peer);
	dev_dbg(rdev_to_dev(rdev), "%s: adev Slave1: %p\n", __func__, rdev->adev);
	dev_dbg(rdev_to_dev(rdev_peer), "%s: adev Slave2: %p\n",
		__func__, rdev_peer->adev);
	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info)
		goto exit;

	info->master = master;
	info->aggr_mode = bnxt_re_get_bond_mode(rdev, netdev_binfo);
	if (BNXT_RE_IS_PORT0(rdev)) {
		info->slave1 = rdev->netdev;
		info->slave2 = rdev_peer->netdev;
		info->pdev1 = rdev->en_dev->pdev;
		info->pdev2 = rdev_peer->en_dev->pdev;
		info->gsi_qp_mode = rdev->gsi_ctx.gsi_qp_mode;
		info->wqe_mode = rdev->chip_ctx->modes.wqe_mode;
		info->aux_dev1 = rdev->adev;
		info->aux_dev2 = rdev_peer->adev;
	} else {
		info->slave2 = rdev->netdev;
		info->slave1 = rdev_peer->netdev;
		info->pdev2 = rdev->en_dev->pdev;
		info->pdev1 = rdev_peer->en_dev->pdev;
		info->gsi_qp_mode = rdev_peer->gsi_ctx.gsi_qp_mode;
		info->wqe_mode = rdev->chip_ctx->modes.wqe_mode;
		info->aux_dev2 = rdev->adev;
		info->aux_dev1 = rdev_peer->adev;
	}

	bnxt_re_ib_uninit(rdev);
	dev_dbg(rdev_to_dev(rdev), "Removing device 1\n");
	bnxt_re_put(rdev);
	bnxt_re_remove_device(rdev, BNXT_RE_COMPLETE_REMOVE, rdev->adev);
	bnxt_re_ib_uninit(rdev_peer);
	dev_dbg(rdev_to_dev(rdev_peer), "Removing device 2\n");
	bnxt_re_remove_device(rdev_peer, BNXT_RE_COMPLETE_REMOVE, rdev_peer->adev);

	rc = bnxt_re_add_device(&info->rdev, info->master, info,
				gsi_mode, BNXT_RE_COMPLETE_INIT, wqe_mode,
				info->aux_dev1, true);
	if (rc) {
		info = ERR_PTR(-EIO);
		goto exit;
	}

	if (BNXT_EN_HW_LAG(info->rdev->en_dev)) {
		rc = bnxt_re_add_device(&info->rdev_peer, info->slave2, NULL,
					gsi_mode, BNXT_RE_COMPLETE_INIT, wqe_mode,
					info->aux_dev2, false);
		if (rc) {
			bnxt_re_remove_device(info->rdev, BNXT_RE_COMPLETE_REMOVE,
					      info->aux_dev1);
			info = ERR_PTR(-EIO);
			goto exit;
		}
	}

	rc = bnxt_re_ib_init(info->rdev);
	if (rc) {
		bnxt_re_ib_uninit(info->rdev);
		bnxt_re_remove_device(info->rdev, BNXT_RE_COMPLETE_REMOVE,
				      info->aux_dev1);
		if (BNXT_EN_HW_LAG(info->rdev->en_dev))
			bnxt_re_remove_device(info->rdev_peer,
					      BNXT_RE_COMPLETE_REMOVE,
					      info->aux_dev2);
		info = ERR_PTR(-EIO);
		goto exit;
	}

	bnxt_re_ib_init_2(info->rdev);

	dev_dbg(rdev_to_dev(info->rdev), "Added bond device rdev %p\n",
		info->rdev);
	rdev = info->rdev;

	tc_rec = &rdev->tc_rec[0];
	rc = bnxt_re_get_pri_dscp_settings(rdev, -1, tc_rec);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to get pri2cos rc:%d", rc);

	en_info = auxiliary_get_drvdata(info->aux_dev1);
	set_bit(BNXT_RE_FLAG_EN_DEV_PRIMARY_DEV, &en_info->flags);

	en_info = auxiliary_get_drvdata(info->aux_dev2);
	set_bit(BNXT_RE_FLAG_EN_DEV_SECONDARY_DEV, &en_info->flags);
	rtnl_lock();
	if (!BNXT_EN_HW_LAG(info->rdev->en_dev))
		en_info->rdev = rdev;
	/* Query link speed.. in rtnl context */
	bnxt_re_get_link_speed(info->rdev);
	rtnl_unlock();
exit:
	dev_put(master);
	return info;
}

int bnxt_re_schedule_work(struct bnxt_re_dev *rdev, unsigned long event,
			  struct net_device *vlan_dev,
			  struct netdev_bonding_info *netdev_binfo,
			  struct bnxt_re_bond_info *binfo,
			  struct net_device *netdev,
			  struct auxiliary_device *adev,
			  struct net_device *bond_netdev)
{
	struct bnxt_re_work *re_work;

	/* Allocate for the deferred task */
	re_work = kzalloc(sizeof(*re_work), GFP_KERNEL);
	if (!re_work)
		return -ENOMEM;

	re_work->rdev = rdev;
	re_work->event = event;
	re_work->vlan_dev = vlan_dev;
	re_work->adev = adev;
	re_work->bond_netdev = bond_netdev;

	if (netdev_binfo) {
		memcpy(&re_work->netdev_binfo, netdev_binfo,
		       sizeof(*netdev_binfo));
		re_work->binfo = binfo;
		re_work->netdev = netdev;
	}
	INIT_WORK(&re_work->work, bnxt_re_task);
	if (rdev)
		atomic_inc(&rdev->sched_count);
	re_work->netdev = netdev;
	queue_work(bnxt_re_wq, &re_work->work);

	return 0;
}

void bnxt_re_create_base_interface(struct bnxt_re_bond_info *binfo, bool primary)
{
	struct auxiliary_device *aux_dev;
	struct bnxt_re_dev *rdev = NULL;
	struct net_device *net_dev;
	int rc = 0;

	if (primary) {
		aux_dev = binfo->aux_dev1;
		net_dev =  binfo->slave1;
	} else {
		aux_dev = binfo->aux_dev2;
		net_dev =  binfo->slave2;
	}

	rc = bnxt_re_add_device(&rdev, net_dev, NULL,
				binfo->gsi_qp_mode, BNXT_RE_COMPLETE_INIT,
				binfo->wqe_mode, aux_dev, true);
	if (rc) {
		dev_err(NULL, "Failed to add the interface");
		return;
	}
	dev_dbg(rdev_to_dev(rdev), "Added device netdev = %p", net_dev);
	rc = bnxt_re_ib_init(rdev);
	if (rc)
		goto clean_dev;

	bnxt_re_ib_init_2(rdev);
	/* Query link speed.. Already in rtnl context */
	rtnl_lock();
	bnxt_re_get_link_speed(rdev);
	rtnl_unlock();
	return;
clean_dev:
	bnxt_re_remove_device(rdev, BNXT_RE_COMPLETE_REMOVE,
			      aux_dev);
}

int bnxt_re_get_slot_pf_count(struct bnxt_re_dev *rdev)
{
	struct pci_dev *pdev_in = rdev->en_dev->pdev;
	unsigned int domain_num = pci_domain_nr(pdev_in->bus);
	int tmp_bus_num, bus_num = pdev_in->bus->number;
	int tmp_dev_num, dev_num = PCI_SLOT(pdev_in->devfn);
	struct bnxt_re_dev *tmp_rdev;
	unsigned int tmp_domain_num;
	int pf_cnt = 0;

	rcu_read_lock();
	list_for_each_entry_rcu(tmp_rdev, &bnxt_re_dev_list, list) {
		tmp_domain_num = pci_domain_nr(tmp_rdev->en_dev->pdev->bus);
		tmp_bus_num = tmp_rdev->en_dev->pdev->bus->number;
		tmp_dev_num = PCI_SLOT(tmp_rdev->en_dev->pdev->devfn);

		if (domain_num == tmp_domain_num && bus_num == tmp_bus_num &&
		    dev_num == tmp_dev_num)
			pf_cnt++;
	}
	rcu_read_unlock();
	return pf_cnt;
}

void bnxt_re_destroy_lag(struct bnxt_re_dev **rdev)
{
	struct bnxt_re_dev *tmp_rdev = *rdev;
	struct bnxt_re_en_dev_info *en_info;
	struct bnxt_re_bond_info bkup_binfo;
	struct auxiliary_device *aux_dev;

	aux_dev = tmp_rdev->adev;
	dev_dbg(rdev_to_dev(tmp_rdev), "%s: LAG rdev: %p\n", __func__, tmp_rdev);
	dev_dbg(rdev_to_dev(tmp_rdev), "%s: adev LAG: %p\n", __func__, aux_dev);
	memcpy(&bkup_binfo, tmp_rdev->binfo, sizeof(*(tmp_rdev->binfo)));
	dev_dbg(rdev_to_dev(tmp_rdev), "Destroying lag device\n");
	bnxt_re_update_fw_lag_info(tmp_rdev->binfo, tmp_rdev, false);
	bnxt_re_ib_uninit(tmp_rdev);

	if (BNXT_EN_HW_LAG(tmp_rdev->en_dev)) {
		en_info = auxiliary_get_drvdata(bkup_binfo.aux_dev2);
		if (en_info) {
			bnxt_re_remove_device(en_info->rdev,
					      BNXT_RE_COMPLETE_REMOVE, bkup_binfo.aux_dev2);
		}
	}
	bnxt_re_remove_device(tmp_rdev, BNXT_RE_COMPLETE_REMOVE, aux_dev);
	dev_dbg(rdev_to_dev(tmp_rdev), "Destroying lag device DONE\n");
	*rdev = NULL;

	en_info = auxiliary_get_drvdata(bkup_binfo.aux_dev1);
	if (en_info) {
		clear_bit(BNXT_RE_FLAG_EN_DEV_PRIMARY_DEV, &en_info->flags);
		en_info->binfo_valid = false;
	}

	en_info = auxiliary_get_drvdata(bkup_binfo.aux_dev2);
	if (en_info)
		clear_bit(BNXT_RE_FLAG_EN_DEV_SECONDARY_DEV, &en_info->flags);
}

int bnxt_re_create_lag(ifbond *master, ifslave *slave,
		       struct netdev_bonding_info *netdev_binfo,
		       struct net_device *netdev,
		       struct bnxt_re_dev **rdev,
		       u8 gsi_mode, u8 wqe_mode)
{
	struct bnxt_re_bond_info *tmp_binfo = NULL;
	struct net_device *real_dev;
	struct bnxt_re_dev *tmp_rdev;
	bool do_lag;

	real_dev = rdma_vlan_dev_real_dev(netdev);
	if (!real_dev)
		real_dev = netdev;
	tmp_rdev = bnxt_re_from_netdev(real_dev);
	if (!tmp_rdev)
		return -EINVAL;
	bnxt_re_hold(tmp_rdev);

	if (master && slave) {
		do_lag = bnxt_re_is_lag_allowed(master, slave, tmp_rdev);
		if (!do_lag) {
			bnxt_re_put(tmp_rdev);
			return -EINVAL;
		}
		dev_dbg(rdev_to_dev(tmp_rdev), "%s:do_lag = %d\n",
			__func__, do_lag);
	}

	tmp_binfo = bnxt_re_alloc_lag(netdev_binfo, tmp_rdev, gsi_mode, wqe_mode);
	if (!tmp_binfo) {
		bnxt_re_put(tmp_rdev);
		return -ENOMEM;
	}
	/*
	 * EIO is returned only if new device creation failed.
	 * This means older rdev is destroyed already.
	 */

	if (PTR_ERR(tmp_binfo) == -EIO) {
		*rdev = NULL;
		return -EIO;
	}

	/* Current rdev already destroyed */
	dev_dbg(rdev_to_dev(tmp_binfo->rdev), "Scheduling for BOND info\n");
	*rdev = tmp_binfo->rdev;
	/* Save netdev_bonding_info for non notifier contexts. */
	memcpy(&tmp_binfo->nbinfo, netdev_binfo, sizeof(*netdev_binfo));

	return 0;
}

static void bond_fill_ifbond(struct bonding *bond, struct ifbond *info)
{
	info->bond_mode = BOND_MODE(bond);
	info->miimon = bond->params.miimon;
	info->num_slaves = bond->slave_cnt;
}

static void bond_fill_ifslave(struct slave *slave, struct ifslave *info)
{
	strcpy(info->slave_name, slave->dev->name);
	info->link = slave->link;
	info->state = bond_slave_state(slave);
	info->link_failure_count = slave->link_failure_count;
}

static struct bnxt_re_bond_info *binfo_from_slave_ndev(struct net_device *netdev)
{
	struct bnxt_re_bond_info *binfo = NULL;
	struct bnxt_re_dev *rdev;

	rcu_read_lock();
	list_for_each_entry_rcu(rdev, &bnxt_re_dev_list, list) {
		if (rdev->binfo &&
		    (rdev->binfo->slave2 == netdev || rdev->binfo->slave1 == netdev)) {
			binfo = rdev->binfo;
			break;
		}
	}
	rcu_read_unlock();
	return binfo;
}

static int bnxt_re_check_and_create_bond(struct net_device *netdev)
{
	struct netdev_bonding_info binfo = {};
	struct bnxt_re_dev *rdev = NULL;
	struct net_device *master;
	struct list_head *iter;
	struct bonding *bond;
	struct slave *slave;
	bool found = false;
	int rc = -1;

	if (!netif_is_bond_slave(netdev))
		return 0;

	rtnl_lock();
	master = netdev_master_upper_dev_get(netdev);
	rtnl_unlock();
	if (!master)
		return rc;

	bond = netdev_priv(master);
	bond_for_each_slave(bond, slave, iter) {
		found = true;
		break;
	}

	if (found) {
		bond_fill_ifslave(slave, &binfo.slave);
		bond_fill_ifbond(slave->bond, &binfo.master);

		rc = bnxt_re_create_lag(&binfo.master, &binfo.slave,
					&binfo, slave->dev, &rdev,
					BNXT_RE_GSI_MODE_INVALID,
					BNXT_QPLIB_WQE_MODE_INVALID);
	}
	return rc;
}

static void bnxt_re_clear_cc_param(struct bnxt_re_dev *rdev)
{
	struct bnxt_qplib_cc_param *cc_param = &rdev->cc_param;

	if (test_bit(BNXT_RE_FLAG_ERR_DEVICE_DETACHED, &rdev->flags))
		return;

	cc_param->enable = 0;
	cc_param->tos_ecn = 0;

	bnxt_re_clear_cc(rdev);
}

static int bnxt_re_init_cc_param(struct bnxt_re_dev *rdev)
{
	struct bnxt_qplib_cc_param *cc_param = &rdev->cc_param;

	/* Set the  default values of dscp and pri values for RoCE and CNP */
	cc_param->alt_tos_dscp = BNXT_RE_DEFAULT_CNP_DSCP;
	cc_param->alt_vlan_pcp = BNXT_RE_DEFAULT_CNP_PRI;
	cc_param->tos_dscp = BNXT_RE_DEFAULT_ROCE_DSCP;
	cc_param->roce_pri = BNXT_RE_DEFAULT_ROCE_PRI;

	/* CC is not enabled on non p5 adapters at 10G speed */
	if (rdev->sl_espeed == SPEED_10000 &&
	    !_is_chip_p5_plus(rdev->chip_ctx))
		return 0;

	return bnxt_re_setup_cc(rdev);
}

static void bnxt_re_schedule_dcb_work(struct bnxt_re_dev *rdev)
{
	struct bnxt_re_dcb_work *dcb_work;

	if (!rdev->dcb_wq)
		return;
	dcb_work = kzalloc(sizeof(*dcb_work), GFP_KERNEL);
	if (!dcb_work)
		return;

	dcb_work->rdev = rdev;
	INIT_WORK(&dcb_work->work, bnxt_re_dcb_wq_task);
	queue_work(rdev->dcb_wq, &dcb_work->work);
}

/* Handle all deferred netevents tasks */
static void bnxt_re_task(struct work_struct *work)
{
	struct netdev_bonding_info *netdev_binfo = NULL;
	struct bnxt_re_bond_info bkup_binfo;
	struct bnxt_re_work *re_work;
	struct bonding *b_master;
	struct bnxt_re_dev *rdev;
	struct list_head *iter;
	struct slave *b_slave;
	int slave_count = 0;
	ifbond *bond;
	ifslave *secondary;
	int rc;

	re_work = container_of(work, struct bnxt_re_work, work);

	mutex_lock(&bnxt_re_mutex);
	rdev = re_work->rdev;

	/*
	 * If the previous rdev is deleted due to bond creation
	 * do not handle the event
	 */
	if (!bnxt_re_is_rdev_valid(rdev))
		goto exit;

	/* Get the latest peer memory map, in cases where the async notification is lost*/
	bnxt_re_get_bar_maps(rdev);

	/* Ignore the event, if the device is not registered with IB stack. This
	 * is to avoid handling any event while the device is added/removed.
	 */
	if (rdev && !test_bit(BNXT_RE_FLAG_IBDEV_REGISTERED, &rdev->flags)) {
		dev_dbg(rdev_to_dev(rdev), "%s: Ignoring netdev event 0x%lx",
			__func__, re_work->event);
		goto done;
	}

	/* Extra check to silence coverity. We shouldn't handle any event
	 * when rdev is NULL.
	 */
	if (!rdev)
		goto exit;

	dev_dbg(rdev_to_dev(rdev), "Scheduled work for event 0x%lx",
		re_work->event);

	switch (re_work->event) {
	case NETDEV_UP:
		bnxt_re_dispatch_event(&rdev->ibdev, NULL, 1,
				       IB_EVENT_PORT_ACTIVE);
		bnxt_re_net_register_async_event(rdev);
		bnxt_re_schedule_dcb_work(rdev);
		break;

	case NETDEV_DOWN:
		bnxt_qplib_dbr_pacing_set_primary_pf(rdev->chip_ctx, 0);
		bnxt_re_dispatch_event(&rdev->ibdev, NULL, 1,
				       IB_EVENT_PORT_ERR);
		break;

	case NETDEV_CHANGE:
		if (bnxt_re_get_link_state(rdev) == IB_PORT_DOWN) {
			bnxt_re_dispatch_event(&rdev->ibdev, NULL, 1,
					       IB_EVENT_PORT_ERR);
			break;
		} else if (bnxt_re_get_link_state(rdev) == IB_PORT_ACTIVE) {
			bnxt_re_dispatch_event(&rdev->ibdev, NULL, 1,
					       IB_EVENT_PORT_ACTIVE);
		}

		if (!bnxt_qplib_query_cc_param(&rdev->qplib_res,
					       &rdev->cc_param) &&
		    !_is_chip_p7_plus(rdev->chip_ctx)) {
			/*
			 *  Disable CC for 10G speed
			 * for non p5 devices
			 */
			if (rdev->sl_espeed == SPEED_10000 &&
			    !_is_chip_p5_plus(rdev->chip_ctx)) {
				if (rdev->cc_param.enable) {
					rdev->cc_param.enable = 0;
					rdev->cc_param.tos_ecn = 0;
					bnxt_re_clear_cc(rdev);
				}
			} else {
				if (!rdev->cc_param.enable &&
				    rdev->cc_param.admin_enable)
					bnxt_re_setup_cc(rdev);
			}
		}
		bnxt_re_schedule_dcb_work(rdev);
		break;

	case NETDEV_UNREGISTER:
		dev_dbg(rdev_to_dev(rdev), "%s: LAG rdev/adev: %p/%p\n",
			__func__, rdev, rdev->adev);
		memcpy(&bkup_binfo, rdev->binfo, sizeof(*(rdev->binfo)));
		bnxt_re_destroy_lag(&rdev);
		bnxt_re_create_base_interface(&bkup_binfo, true);
		bnxt_re_create_base_interface(&bkup_binfo, false);
		break;

	case NETDEV_BONDING_INFO:
		netdev_binfo = &re_work->netdev_binfo;
		bond = &netdev_binfo->master;
		secondary = &netdev_binfo->slave;
		if (rdev->binfo) {
			memcpy(&bkup_binfo, rdev->binfo, sizeof(*(rdev->binfo)));
			/* Change in bond state */
			dev_dbg(rdev_to_dev(rdev),
				"Change in Bond state rdev = %p\n", rdev);
			if (bond->num_slaves != 2) {
				bnxt_re_destroy_lag(&rdev);
				bnxt_re_create_base_interface(&bkup_binfo, true);
				bnxt_re_create_base_interface(&bkup_binfo, false);
			}
		} else {
			/* Check BOND needs to be created or not  rdev will be valid
			 * Pass gsi_mode invalid, we want to inherit from PF0
			 */
			rc = bnxt_re_create_lag(bond, secondary,
						netdev_binfo, re_work->netdev, &rdev,
						BNXT_RE_GSI_MODE_INVALID,
						BNXT_QPLIB_WQE_MODE_INVALID);
			if (rc)
				dev_warn(rdev_to_dev(rdev), "%s: failed to create lag %d\n",
					 __func__, rc);
			goto exit;
		}
		break;

	case NETDEV_CHANGEINFODATA:
		if (!netif_is_bond_master(re_work->netdev))
			break;
		rtnl_lock();
		b_master = netdev_priv(re_work->netdev);
		slave_count = 0;
		bond_for_each_slave(b_master, b_slave, iter)
			slave_count++;
		rtnl_unlock();
		if (slave_count == 2)
			break;
		if (rdev->binfo) {
			dev_dbg(rdev_to_dev(rdev),
				"Delete lag %s since one interface is de-slaved\n",
				rdev->dev_name);
			memcpy(&bkup_binfo, rdev->binfo, sizeof(*(rdev->binfo)));
			bnxt_re_destroy_lag(&rdev);
			bnxt_re_create_base_interface(&bkup_binfo, true);
			bnxt_re_create_base_interface(&bkup_binfo, false);
		}
		break;

	default:
		break;
	}
done:
	if (rdev) {
		/* memory barrier to guarantee task completion
		 * before decrementing sched count
		 */
		smp_mb__before_atomic();
		atomic_dec(&rdev->sched_count);
	}
exit:
	if (re_work->event == NETDEV_BONDING_INFO)
		dev_put(re_work->bond_netdev);
	kfree(re_work);
	mutex_unlock(&bnxt_re_mutex);
}

/*
    "Notifier chain callback can be invoked for the same chain from
    different CPUs at the same time".

    For cases when the netdev is already present, our call to the
    register_netdevice_notifier() will actually get the rtnl_lock()
    before sending NETDEV_REGISTER and (if up) NETDEV_UP
    events.

    But for cases when the netdev is not already present, the notifier
    chain is subjected to be invoked from different CPUs simultaneously.

    This is protected by the netdev_mutex.
*/
static int bnxt_re_netdev_event(struct notifier_block *notifier,
				unsigned long event, void *ptr)
{
	struct netdev_notifier_bonding_info *notifier_info = ptr;
	struct net_device *real_dev, *netdev, *bond_netdev;
	struct netdev_bonding_info *netdev_binfo = NULL;
	struct bnxt_re_dev *rdev = NULL;
	ifbond *master;
	ifslave *slave;
	int rc;

	netdev = netdev_notifier_info_to_dev(ptr);
	real_dev = rdma_vlan_dev_real_dev(netdev);
	if (!real_dev)
		real_dev = netdev;
	/* In case of bonding,this will be bond's rdev */
	rdev = bnxt_re_from_netdev(real_dev);

	netdev_dbg(netdev, "%s: Event = %s (0x%lx), rdev %s (real_dev %s)\n",
		   __func__, bnxt_re_netevent(event), event,
		   rdev ? rdev->netdev ? rdev->netdev->name : "->netdev = NULL" : "= NULL",
		   (real_dev == netdev) ? "= netdev" : real_dev->name);

	if (!rdev || gmod_exit)
		goto exit;

	if (!test_bit(BNXT_RE_FLAG_IBDEV_REGISTERED, &rdev->flags))
		goto exit;

	bnxt_re_hold(rdev);
	if (rdev->binfo && (rdev->binfo->slave1 == real_dev ||
			    rdev->binfo->slave2 == real_dev)) {
		dev_dbg(rdev_to_dev(rdev),
			"Event %#lx on slave interface = %p\n",
			event, rdev);
		/*
		 * Handle NETDEV_CHANGE event,
		 * to update PFC/ECN settings on BONO
		 * without a shut/noshut
		 */
		if (event == NETDEV_CHANGE)
			goto handle_event;

		if (event != NETDEV_BONDING_INFO) {
			dev_dbg(rdev_to_dev(rdev),
				"Ignoring event real_dev = %p master=%p slave1=%p slave2=%p\n",
				real_dev, rdev->binfo->master, rdev->binfo->slave1,
				rdev->binfo->slave2);
			goto done;
		} else {
			dev_dbg(rdev_to_dev(rdev),
				"Handle event real_dev = %p master=%p slave1=%p slave2=%p\n",
				real_dev, rdev->binfo->master, rdev->binfo->slave1,
				rdev->binfo->slave2);
			netdev_binfo = &notifier_info->bonding_info;
			goto handle_event;
		}
	}

	if (real_dev != netdev) {
		switch (event) {
		case NETDEV_UP:
			bnxt_re_schedule_work(rdev, event, netdev, NULL, NULL,
					      NULL, NULL, NULL);
			break;
		case NETDEV_DOWN:
			break;
		default:
			break;
		}
		goto done;
	}
handle_event:
	switch (event) {
	case NETDEV_CHANGEADDR:
		if (!_is_chip_p5_plus(rdev->chip_ctx))
			bnxt_re_update_shadow_ah(rdev);
		addrconf_addr_eui48((u8 *)&rdev->ibdev.node_guid,
				    rdev->netdev->dev_addr);
		break;

	case NETDEV_CHANGE:
		/*
		 * In bonding case bono requires to
		 * send this HWRM after PFC/ECN config
		 * avoiding a shut/noshut workaround
		 * For other case handle as usual
		 */
		if (rdev->binfo) {
			u8 active_port_map;
			/* Update the latest link information */
			active_port_map = bnxt_re_get_bond_link_status(rdev->binfo);
			if (active_port_map) {
				rdev->binfo->active_port_map = active_port_map;
				bnxt_re_update_fw_lag_info(rdev->binfo, rdev, true);
			}
		}
		bnxt_re_get_link_speed(rdev);
		bnxt_re_schedule_work(rdev, event, NULL, NULL, NULL, NULL, NULL, NULL);
		break;

	case NETDEV_CHANGEMTU:
		/*
		 * In bonding case bono requires to
		 * resend this HWRM after MTU change
		 */
		if (rdev && rdev->binfo && netif_is_bond_master(real_dev))
			bnxt_re_update_fw_lag_info(rdev->binfo, rdev, true);
		goto done;

	case NETDEV_UNREGISTER:
		/* netdev notifier will call NETDEV_UNREGISTER again later since
		 * we are still holding the reference to the netdev
		 */

		/*
		 *  Workaround to avoid ib_unregister hang. Check for module
		 *  reference and dont free up the device if the reference
		 *  is non zero. Checking only for PF functions.
		 */

		if (rdev && !rdev->is_virtfn && module_refcount(THIS_MODULE) >  0) {
			dev_info(rdev_to_dev(rdev),
				 "bnxt_re:Unreg recvd when module refcnt > 0");
			dev_info(rdev_to_dev(rdev),
				 "bnxt_re:Close all apps using bnxt_re devs");
			dev_info(rdev_to_dev(rdev),
				 "bnxt_re:Remove the configfs entry created for the device");
			dev_info(rdev_to_dev(rdev),
				 "bnxt_re:Refer documentation for details");
			goto done;
		}

		if (atomic_read(&rdev->sched_count) > 0)
			goto done;

		/*
		 * Schedule ib device unregistration only for LAG.
		 */
		if (!rdev->unreg_sched && rdev->binfo) {
			bnxt_re_schedule_work(rdev, NETDEV_UNREGISTER,
					      NULL, NULL, NULL, NULL, NULL, NULL);
			rdev->unreg_sched = true;
			goto done;
		}

		break;

	case NETDEV_BONDING_INFO:
		netdev_binfo = &notifier_info->bonding_info;
		master = &netdev_binfo->master;
		slave = &netdev_binfo->slave;

		dev_info(rdev_to_dev(rdev), "Bonding Info Received: rdev: %p\n", rdev);
		dev_info(rdev_to_dev(rdev), "\tMaster: mode: %d num_slaves:%d\n",
			 master->bond_mode, master->num_slaves);
		dev_info(rdev_to_dev(rdev), "\tSlave: id: %d name:%s link:%d state:%d\n",
			 slave->slave_id, slave->slave_name,
			 slave->link, slave->state);
		/*
		 * If bond is already created an two secondary interface is available
		 * handle the link change as early as possible. Else, schedule it to
		 * the bnxt_re_task
		 */
		if (rdev->binfo && master->num_slaves == 2) {
			/* Change in bond state */
			dev_dbg(rdev_to_dev(rdev),
				"Change in Bond state rdev = %p\n", rdev);
			if (slave->link == BOND_LINK_UP) {
				dev_dbg(rdev_to_dev(rdev),
					"LAG: Skip link up\n");
				dev_dbg(rdev_to_dev(rdev),
					"LAG: Handle from worker\n");
				goto done;
			}
			dev_dbg(rdev_to_dev(rdev), "Updating lag device\n");
			if (bnxt_re_bond_update_reqd
			    (netdev_binfo, rdev->binfo, rdev, real_dev))
				bnxt_re_update_fw_lag_info
					(rdev->binfo, rdev, true);
		} else if (rdev->binfo || master->num_slaves == 2) {
			bond_netdev = netdev_master_upper_dev_get(real_dev);
			if (!bond_netdev)
				goto done;
			dev_hold(bond_netdev);
			rc = bnxt_re_schedule_work(rdev, event, NULL, netdev_binfo,
						   NULL, real_dev, NULL, bond_netdev);
			if (rc)
				dev_put(bond_netdev);
		}
		break;

	case NETDEV_CHANGEINFODATA:
		/*
		 * This event has to be handled to destroy lag interface
		 * when a user de-slaves an interface from bond
		 */
		bnxt_re_schedule_work(rdev, event, NULL,
				      NULL, NULL, real_dev, NULL, NULL);
		break;
	default:
		break;
	}
done:
	if (rdev)
		bnxt_re_put(rdev);
exit:
	return NOTIFY_DONE;
}

static struct notifier_block bnxt_re_netdev_notifier = {
	.notifier_call = bnxt_re_netdev_event
};

#define BNXT_ADEV_NAME "bnxt_en"

static int bnxt_re_suspend(struct auxiliary_device *adev, pm_message_t state)
{
	bnxt_re_stop(adev);
	return 0;
}

static int bnxt_re_resume(struct auxiliary_device *adev)
{
	bnxt_re_start(adev);
	return 0;
}

static void bnxt_re_remove_bond_interface(struct bnxt_re_dev *rdev,
					  struct auxiliary_device *adev,
					  bool primary_dev)
{
	struct bnxt_re_bond_info *info = NULL;
	struct bnxt_re_bond_info bkup_binfo;

	dev_dbg(rdev_to_dev(rdev), "%s: Removing adev: %p rdev %p\n",
		__func__, adev, rdev);

	info = binfo_from_slave_ndev(rdev->en_dev->net);

	if (info) {
		memcpy(&bkup_binfo, info, sizeof(*info));
		bnxt_re_destroy_lag(&bkup_binfo.rdev);
		if (!gmod_exit) {
			if (primary_dev)
				bnxt_re_create_base_interface(&bkup_binfo, false);
			else
				bnxt_re_create_base_interface(&bkup_binfo, true);
		}
	}
	auxiliary_set_drvdata(adev, NULL);
}

static void bnxt_re_remove_base_interface(struct bnxt_re_dev *rdev,
					  struct auxiliary_device *adev)
{
	dev_dbg(rdev_to_dev(rdev), "%s: adev: %p\n", __func__, adev);

	bnxt_re_ib_uninit(rdev);
	bnxt_re_remove_device(rdev, BNXT_RE_COMPLETE_REMOVE, adev);
	auxiliary_set_drvdata(adev, NULL);
}

/*
 *  bnxt_re_remove  -	Removes the roce aux device
 *  @adev  -  aux device pointer
 *
 * This function removes the roce device. This gets
 * called in the mod exit path and pci unbind path.
 * If the rdev is bond interface, destroys the lag
 * in module exit path, and in pci unbind case
 * destroys the lag and recreates other base interface.
 * If the device is already removed in error recovery
 * path, it just unregister with the L2.
 */
static AUDEV_REM_RET bnxt_re_remove(struct auxiliary_device *adev)
{
	struct bnxt_re_en_dev_info *en_info = auxiliary_get_drvdata(adev);
	struct bnxt_en_dev *en_dev;
	struct bnxt_re_dev *rdev;
	bool primary_dev = false;
	bool secondary_dev = false;

	en_dev = en_info->en_dev;

	mutex_lock(&en_dev->en_dev_lock);
	mutex_lock(&bnxt_re_mutex);
	rdev = en_info->rdev;

	if (rdev && bnxt_re_is_rdev_valid(rdev)) {
		if (pci_channel_offline(rdev->rcfw.pdev))
			set_bit(ERR_DEVICE_DETACHED, &rdev->rcfw.cmdq.flags);
		if (test_bit(BNXT_RE_FLAG_EN_DEV_PRIMARY_DEV, &en_info->flags))
			primary_dev = true;
		if (test_bit(BNXT_RE_FLAG_EN_DEV_SECONDARY_DEV, &en_info->flags))
			secondary_dev = true;

		/*
		 * en_dev_info of primary device and secondary device have the
		 * same rdev pointer when LAG is configured. This rdev pointer
		 * is rdev of bond interface.
		 */
		if (primary_dev || secondary_dev) {
			/* removal of bond primary or secondary interface */
			bnxt_re_remove_bond_interface(rdev, adev, primary_dev);
		} else {
			/* removal of non bond interface */
			bnxt_re_remove_base_interface(rdev, adev);
		}
	} else {
		/* device is removed from ulp stop, unregister the net dev */
		if (test_bit(BNXT_RE_FLAG_EN_DEV_NETDEV_REG, &en_info->flags))
			bnxt_unregister_dev(en_dev);
	}
	kfree(en_info);
	mutex_unlock(&bnxt_re_mutex);
	mutex_unlock(&en_dev->en_dev_lock);
#ifdef HAVE_AUDEV_REM_RET_INT
	return 0;
#else
	return;
#endif
}

static void bnxt_re_ib_init_2(struct bnxt_re_dev *rdev)
{
	int rc;

	rc = bnxt_re_get_device_stats(rdev);
	if (rc)
		dev_err(rdev_to_dev(rdev),
			"Failed initial device stat query");

	bnxt_re_net_register_async_event(rdev);

#ifdef IB_PEER_MEM_MOD_SUPPORT
	rdev->peer_dev = ib_peer_mem_add_device(&rdev->ibdev);
#endif
}

static int bnxt_re_probe(struct auxiliary_device *adev,
			 const struct auxiliary_device_id *id)
{
	struct bnxt_aux_priv *aux_priv =
		container_of(adev, struct bnxt_aux_priv, aux_dev);
	struct bnxt_re_en_dev_info *en_info;
	struct bnxt_en_dev *en_dev = NULL;
	struct bnxt_re_dev *rdev;
	int rc = -ENODEV;

	if (aux_priv)
		en_dev = aux_priv->edev;

	if (!en_dev)
		return rc;

	if (en_dev->ulp_version != BNXT_ULP_VERSION) {
		dev_err(NULL, "%s: probe error: bnxt_en ulp version magic %x is not compatible!\n",
			ROCE_DRV_MODULE_NAME, en_dev->ulp_version);
		return -EINVAL;
	}

	en_info = kzalloc(sizeof(*en_info), GFP_KERNEL);
	if (!en_info)
		return -ENOMEM;
	en_info->en_dev = en_dev;

	/* Use parents chip_type info in pre-init state to assign defaults */
	en_info->wqe_mode = BNXT_QPLIB_WQE_MODE_STATIC;
	if (_is_chip_num_p7_plus(en_dev->chip_num))
		en_info->wqe_mode = BNXT_QPLIB_WQE_MODE_VARIABLE;

	auxiliary_set_drvdata(adev, en_info);

	mutex_lock(&en_dev->en_dev_lock);
	mutex_lock(&bnxt_re_mutex);
	rc = bnxt_re_add_device(&rdev, en_dev->net, NULL,
				BNXT_RE_GSI_MODE_ALL,
				BNXT_RE_COMPLETE_INIT,
				en_info->wqe_mode,
				adev, true);
	if (rc) {
		kfree(en_info);
		mutex_unlock(&bnxt_re_mutex);
		mutex_unlock(&en_dev->en_dev_lock);
		return rc;
	}

	rc = bnxt_re_ib_init(rdev);
	if (rc)
		goto err;

	bnxt_re_ib_init_2(rdev);
	rc = bnxt_re_get_pri_dscp_settings(rdev, -1, rdev->tc_rec);
	if (rc)
		goto err;

	dev_dbg(rdev_to_dev(rdev), "%s: adev: %p wqe_mode: %s\n", __func__, adev,
		(en_info->wqe_mode == BNXT_QPLIB_WQE_MODE_VARIABLE) ?
		 "Variable" : "Static");

	rc = bnxt_re_check_and_create_bond(rdev->netdev);
	if (rc)
		dev_dbg(rdev_to_dev(rdev), "%s: failed to create lag. rc = %d",
			__func__, rc);
	mutex_unlock(&bnxt_re_mutex);
	mutex_unlock(&en_dev->en_dev_lock);

	return 0;

err:
	mutex_unlock(&bnxt_re_mutex);
	mutex_unlock(&en_dev->en_dev_lock);
	bnxt_re_remove(adev);

	return rc;
}

static const struct auxiliary_device_id bnxt_re_id_table[] = {
	{ .name = BNXT_ADEV_NAME ".rdma", },
	{},
};

MODULE_DEVICE_TABLE(auxiliary, bnxt_re_id_table);

static struct auxiliary_driver bnxt_re_driver = {
	.name = "rdma",
	.probe = bnxt_re_probe,
	.remove = bnxt_re_remove,
	.shutdown = bnxt_re_shutdown,
	.suspend = bnxt_re_suspend,
	.resume = bnxt_re_resume,
	.id_table = bnxt_re_id_table,
};

static int __init bnxt_re_mod_init(void)
{
	int rc = 0;

	pr_info("%s: %s", ROCE_DRV_MODULE_NAME, version);

	bnxt_re_wq = create_singlethread_workqueue("bnxt_re");
	if (!bnxt_re_wq)
		return -ENOMEM;

#ifdef ENABLE_DEBUGFS
	bnxt_re_debugfs_init();
#endif
#ifdef HAVE_CONFIGFS_ENABLED
	bnxt_re_configfs_init();
#endif
	rc = bnxt_re_register_netdevice_notifier(&bnxt_re_netdev_notifier);
	if (rc) {
		dev_err(NULL, "%s: Cannot register to netdevice_notifier",
			ROCE_DRV_MODULE_NAME);
		goto err_netdev;
	}

	INIT_LIST_HEAD(&bnxt_re_dev_list);

	rc = auxiliary_driver_register(&bnxt_re_driver);
	if (rc) {
		pr_err("%s: Failed to register auxiliary driver\n",
		       ROCE_DRV_MODULE_NAME);
		goto err_auxdrv;
	}

	return 0;

err_auxdrv:
	bnxt_re_unregister_netdevice_notifier(&bnxt_re_netdev_notifier);

err_netdev:
#ifdef HAVE_CONFIGFS_ENABLED
	bnxt_re_configfs_exit();
#endif
#ifdef ENABLE_DEBUGFS
	bnxt_re_debugfs_remove();
#endif
	destroy_workqueue(bnxt_re_wq);

	return rc;
}

static void __exit bnxt_re_mod_exit(void)
{
	rtnl_lock();
	gmod_exit = 1;
	rtnl_unlock();
	auxiliary_driver_unregister(&bnxt_re_driver);

	bnxt_re_unregister_netdevice_notifier(&bnxt_re_netdev_notifier);

#ifdef HAVE_CONFIGFS_ENABLED
	bnxt_re_configfs_exit();
#endif
#ifdef ENABLE_DEBUGFS
	bnxt_re_debugfs_remove();
#endif
	if (bnxt_re_wq)
		destroy_workqueue(bnxt_re_wq);
}

module_init(bnxt_re_mod_init);
module_exit(bnxt_re_mod_exit);
