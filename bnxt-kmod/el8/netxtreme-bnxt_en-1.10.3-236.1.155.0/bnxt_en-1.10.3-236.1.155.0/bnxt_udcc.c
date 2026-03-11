/* Broadcom NetXtreme-C/E network driver.
 *
 * Copyright (c) 2023-2024 Broadcom Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 */

#include <linux/stddef.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/bitmap.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/hashtable.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/tcp.h>
#include <net/inet_hashtables.h>
#include <net/inet6_hashtables.h>
#include "bnxt_compat.h"
#include "bnxt_hsi.h"
#include "bnxt.h"
#include "bnxt_hwrm.h"
#include "ulp_generic_flow_offload.h"
#include "ulp_udcc.h"
#include "bnxt_tf_ulp.h"
#include "bnxt_udcc.h"
#include "bnxt_debugfs.h"
#include "bnxt_nic_flow.h"

#if defined(CONFIG_BNXT_FLOWER_OFFLOAD)

static int bnxt_tf_ulp_flow_delete(struct bnxt *bp, struct bnxt_udcc_session_entry *entry);

int bnxt_hwrm_udcc_cfg(struct bnxt *bp, u32 enables, u8 mode, u8 padcnt)
{
	struct hwrm_udcc_cfg_input *req;
	int rc;

	netdev_dbg(bp->dev, "UDCC enables 0x%x mode: %d padcnt: %d\n",
		   enables, mode, padcnt);

	rc = hwrm_req_init(bp, req, HWRM_UDCC_CFG);
	if (rc)
		return rc;

	req->target_id = cpu_to_le16(0xffff);
	req->enables = cpu_to_le32(enables);

	if (enables & UDCC_CFG_REQ_ENABLES_UDCC_MODE)
		req->udcc_mode = mode;

	if (enables & UDCC_CFG_REQ_ENABLES_PROBE_PAD_CNT_CFG)
		req->probe_pad_cnt_cfg = padcnt;

	return hwrm_req_send(bp, req);
}

int bnxt_hwrm_udcc_qcfg(struct bnxt *bp)
{
	struct hwrm_udcc_qcfg_output *resp;
	struct hwrm_udcc_qcfg_input *req;
	int rc;

	if (BNXT_VF(bp) || !BNXT_UDCC_CAP(bp))
		return -EOPNOTSUPP;

	rc = hwrm_req_init(bp, req, HWRM_UDCC_QCFG);
	if (rc)
		return rc;

	req->target_id = cpu_to_le16(0xffff);

	resp = hwrm_req_hold(bp, req);
	rc = hwrm_req_send(bp, req);
	if (rc)
		goto udcc_qcfg_exit;

	bp->udcc_info->mode = resp->udcc_mode;
	bp->udcc_info->hybrid_mode = resp->udcc_hybrid_mode;
	bp->udcc_info->pad_cnt = resp->probe_pad_cnt_cfg;

udcc_qcfg_exit:
	hwrm_req_drop(bp, req);
	return rc;
}

int bnxt_start_udcc_worker(struct bnxt *bp)
{
	struct bnxt_udcc_info *udcc = bp->udcc_info;
	char *name;
	int rc = 0;

	if (BNXT_VF(bp) || !BNXT_UDCC_CAP(bp))
		return 0;

	if (!udcc)
		return 0;

	name = kasprintf(GFP_KERNEL, "%s-udcc-wq", dev_name(bp->dev->dev.parent));
	if (!name)
		return -ENOMEM;

	udcc->bnxt_udcc_wq = create_singlethread_workqueue(name);
	if (!udcc->bnxt_udcc_wq) {
		netdev_err(bp->dev, "Unable to create udcc workqueue.\n");
		rc = -ENOMEM;
	}

	kfree(name);
	return rc;
}

void bnxt_stop_udcc_worker(struct bnxt *bp)
{
	struct bnxt_udcc_info *udcc = bp->udcc_info;

	if (BNXT_VF(bp) || !BNXT_UDCC_CAP(bp))
		return;

	if (!udcc)
		return;

	if (udcc->bnxt_udcc_wq)
		destroy_workqueue(udcc->bnxt_udcc_wq);
}

int bnxt_alloc_udcc_info(struct bnxt *bp)
{
	struct bnxt_udcc_info *udcc = bp->udcc_info;
	struct hwrm_udcc_qcaps_output *resp;
	struct hwrm_func_qcaps_input *req;
	int rc;

	if (BNXT_VF(bp) || !BNXT_UDCC_CAP(bp))
		return 0;

	/* default probe cfg 0x3, udcc mode cfg not supported */
	rc = bnxt_hwrm_udcc_cfg(bp,
				UDCC_CFG_REQ_ENABLES_PROBE_PAD_CNT_CFG,
				0 /* udcc mode*/,
				UDCC_CFG_REQ_PROBE_PAD_CNT_CFG_THREE);
	/* Ignore failure, just debug log it */
	if (rc)
		netdev_dbg(bp->dev, "UDCC probe pad count cfg failed(%d)\n", rc);

	if (udcc)
		return 0;

	rc = hwrm_req_init(bp, req, HWRM_UDCC_QCAPS);
	if (rc)
		return rc;

	req->fid = cpu_to_le16(0xffff);
	resp = hwrm_req_hold(bp, req);
	rc = hwrm_req_send(bp, req);
	if (rc)
		goto exit;

	udcc = kzalloc(sizeof(*udcc), GFP_KERNEL);
	if (!udcc)
		goto exit;

	udcc->max_sessions = le16_to_cpu(resp->max_sessions);
	udcc->max_comp_cfg_xfer = le16_to_cpu(resp->max_comp_cfg_xfer);
	udcc->max_comp_data_xfer = le16_to_cpu(resp->max_comp_data_xfer);
	udcc->session_type = resp->session_type;
	udcc->flags = le16_to_cpu(resp->flags);
	mutex_init(&udcc->session_db_lock);
	bp->udcc_info = udcc;

	rc = bnxt_hwrm_udcc_qcfg(bp);
	if (rc)
		goto free_udcc;

	if (BNXT_UDCC_DCQCN_EN(bp))
		netdev_info(bp->dev, "%s Adaptive DCQCN enabled with max %d sessions per adapter\n",
			    udcc->session_type ? "Per-QP" : "Per-DestIP",
			    udcc->max_sessions);
	else if (udcc->mode)
		netdev_info(bp->dev, "%s UDCC enabled!!! in %s with max %d sessions per adapter\n",
			    udcc->session_type ? "Per-QP" : "Per-DestIP",
			    udcc->hybrid_mode ? "Hybrid mode" :
			    "Non-Hybrid mode",
			    udcc->max_sessions);
	else
		netdev_info(bp->dev, "UDCC disabled!!!\n");

	goto exit; /* success */

free_udcc:
	kfree(udcc);
	bp->udcc_info = NULL;
exit:
	hwrm_req_drop(bp, req);
	return rc;
}

int bnxt_hwrm_udcc_session_query(struct bnxt *bp, u32 session_id,
				 struct hwrm_udcc_session_query_output *resp_out)
{
	struct hwrm_udcc_session_query_input *req;
	struct hwrm_udcc_session_query_output *resp;
	int rc;

	rc = hwrm_req_init(bp, req, HWRM_UDCC_SESSION_QUERY);
	if (rc)
		return rc;

	req->session_id = cpu_to_le16(session_id);

	resp = hwrm_req_hold(bp, req);
	rc = hwrm_req_send(bp, req);
	if (rc)
		goto udcc_query_exit;

	memcpy(resp_out, resp, sizeof(struct hwrm_udcc_session_query_output));

udcc_query_exit:
	hwrm_req_drop(bp, req);
	return rc;
}

static int bnxt_hwrm_udcc_session_qcfg(struct bnxt *bp, struct bnxt_udcc_session_entry *entry)
{
	struct hwrm_udcc_session_qcfg_output *resp;
	struct hwrm_udcc_session_qcfg_input *req;
	int rc;

	rc = hwrm_req_init(bp, req, HWRM_UDCC_SESSION_QCFG);
	if (rc)
		return rc;

	req->session_id = cpu_to_le16(entry->session_id);

	resp = hwrm_req_hold(bp, req);
	rc = hwrm_req_send(bp, req);
	if (rc)
		goto udcc_qcfg_exit;

	ether_addr_copy(entry->dest_mac, resp->dest_mac);
	ether_addr_copy(entry->src_mac, resp->src_mac);
	memcpy(entry->dst_ip.s6_addr32, resp->dest_ip, sizeof(resp->dest_ip));
	entry->dest_qp_num = le32_to_cpu(resp->dest_qp_num);
	entry->src_qp_num = le32_to_cpu(resp->src_qp_num);

udcc_qcfg_exit:
	hwrm_req_drop(bp, req);
	return rc;
}

static int bnxt_hwrm_udcc_session_cfg(struct bnxt *bp, struct bnxt_udcc_session_entry *entry)
{
	struct bnxt_bond_info *binfo = bp->bond_info;
	struct hwrm_udcc_session_cfg_input *req;
	int rc = 0;

	if (binfo && binfo->fw_lag_id != BNXT_INVALID_LAG_ID &&
	    bp->pf.fw_fid != BNXT_FIRST_PF_FID) {
		netdev_dbg(bp->dev, "Only PF0 can update session cfg lag id=%d\n",
			   binfo->fw_lag_id);
		return rc;
	}

	rc = hwrm_req_init(bp, req, HWRM_UDCC_SESSION_CFG);
	if (rc)
		return rc;

	req->session_id = cpu_to_le16(entry->session_id);
	if (entry->state != UDCC_SESSION_CFG_REQ_SESSION_STATE_ENABLED) {
		req->enables =  cpu_to_le32(UDCC_SESSION_CFG_REQ_ENABLES_SESSION_STATE);
		goto session_state;
	}
	req->enables = cpu_to_le32(UDCC_SESSION_CFG_REQ_ENABLES_SESSION_STATE |
				   UDCC_SESSION_CFG_REQ_ENABLES_DEST_MAC |
				   UDCC_SESSION_CFG_REQ_ENABLES_SRC_MAC |
				   UDCC_SESSION_CFG_REQ_ENABLES_TX_STATS_RECORD |
				   UDCC_SESSION_CFG_REQ_ENABLES_RX_STATS_RECORD);
	if (is_valid_ether_addr(entry->dst_mac_mod) &&
	    is_valid_ether_addr(entry->src_mac_mod)) {
		ether_addr_copy(req->dest_mac, entry->dst_mac_mod);
		ether_addr_copy(req->src_mac, entry->src_mac_mod);
	} else {
		ether_addr_copy(req->dest_mac, entry->dest_mac);
		ether_addr_copy(req->src_mac, entry->src_mac);
	}
	req->tx_stats_record = cpu_to_le32((u32)entry->tx_counter_hndl);
	req->rx_stats_record = cpu_to_le32((u32)entry->rx_counter_hndl);

session_state:
	req->session_state = entry->state;
	return hwrm_req_send(bp, req);
}

/* This function converts the provided tfc action handle to the UDCC
 * action handle required by the firmware.  The action handle consists
 * of an 8 byte offset in the lower 26 bits and the table scope id in
 * the upper 6 bits.
 */
static int bnxt_tfc_counter_update(struct bnxt *bp, u64 *counter_hndl)
{
	u64 val = 0;
	u8 tsid = 0;
	int rc = 0;

	rc = bnxt_ulp_cntxt_tsid_get(bp->ulp_ctx, &tsid);
	if (rc) {
		netdev_dbg(bp->dev, "%s:Invalid tsid, cannot update counter_hndl rc=%d\n",
			   __func__, rc);
		return rc;
	}
	val = *counter_hndl;
	/* 32B offset to 8B offset */
	val = val << ACT_OFFS_DIV;
	val &= ACT_OFFS_MASK;
	val |= (tsid & TSID_MASK) << TSID_SHIFT;

	*counter_hndl = val;
	netdev_dbg(bp->dev, "%s:counter_hndl update tsid(%d) counter_hndl(%llx)\n",
		   __func__, tsid, *counter_hndl);
	return rc;
}

static u8 bnxt_ulp_gen_l3_ipv4_addr_em_mask[] = {
	0xff, 0xff, 0xff, 0xff
};

static u8 bnxt_ulp_gen_l3_ipv6_addr_em_mask[] = {
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff
};

static int bnxt_udcc_flow_create_p7(struct bnxt *bp,
				    struct bnxt_udcc_session_entry *entry,
				    enum cfa_dir dir)
{
	struct bnxt_ulp_gen_bth_hdr bth_spec = { 0 }, bth_mask = { 0 };
	struct bnxt_ulp_gen_ipv6_hdr v6_spec = { 0 }, v6_mask = { 0 };
	struct bnxt_ulp_gen_ipv4_hdr v4_spec = { 0 }, v4_mask = { 0 };
	bool per_qp_session = BNXT_UDCC_SESSION_PER_QP(bp);
	struct bnxt_ulp_gen_l2_hdr_parms l2_parms = { 0 };
	struct bnxt_ulp_gen_l3_hdr_parms l3_parms = { 0 };
	struct bnxt_ulp_gen_l4_hdr_parms l4_parms = { 0 };
	struct bnxt_ulp_gen_action_parms actions = { 0 };
	struct bnxt_ulp_gen_flow_parms parms = { 0 };

	/* These would normally be preset and passed to the upper layer */
	/* u32 dst_qpn = cpu_to_be32(entry->dest_qp_num); */
	u16 op_code = cpu_to_be16(BNXT_ROCE_CNP_OPCODE);
	u16 op_code_mask = cpu_to_be16(0xffff);
	u32 src_qpn = cpu_to_be32(entry->src_qp_num);
	u32 dst_qpn = cpu_to_be32(entry->dest_qp_num);
	u32 msk_qpn = cpu_to_be32(0xffffffff);
	u8 l4_proto = IPPROTO_UDP;
	u8 l4_proto_mask = 0xff;
	int rc;

	l2_parms.type = BNXT_ULP_GEN_L2_L2_HDR;
	if (entry->v4_dst) {
		/* Pack the L3 Data */
		v4_spec.proto = &l4_proto;
		v4_mask.proto = &l4_proto_mask;
		if (dir == CFA_DIR_RX) {
			v4_spec.dip = NULL;
			v4_mask.dip = NULL;
			v4_spec.sip = (u32 *)&entry->dst_ip.s6_addr32[3];
			v4_mask.sip = (u32 *)bnxt_ulp_gen_l3_ipv4_addr_em_mask;
		} else {
			v4_spec.dip = (u32 *)&entry->dst_ip.s6_addr32[3];
			v4_mask.dip = (u32 *)bnxt_ulp_gen_l3_ipv4_addr_em_mask;
			v4_spec.sip = NULL;
			v4_mask.sip = NULL;
		}
		l3_parms.type = BNXT_ULP_GEN_L3_IPV4;
		l3_parms.v4_spec = &v4_spec;
		l3_parms.v4_mask = &v4_mask;
		netdev_dbg(bp->dev, "UDCC Add(%s) flow for session_id: %d IP %pI4 sQPn %x dQPn %x\n",
			   dir == CFA_DIR_RX ? "rx" : "tx",
			   entry->session_id,
			   (u32 *)&entry->dst_ip.s6_addr32[3],
			   src_qpn, dst_qpn);
	} else {
		/* Pack the L3 Data */
		v6_spec.proto6 = &l4_proto;
		v6_mask.proto6 = &l4_proto_mask;
		if (dir == CFA_DIR_RX) {
			v6_spec.dip6 = NULL;
			v6_mask.dip6 = NULL;
			v6_spec.sip6 = entry->dst_ip.s6_addr;
			v6_mask.sip6 = bnxt_ulp_gen_l3_ipv6_addr_em_mask;
		} else {
			v6_spec.dip6 = entry->dst_ip.s6_addr;
			v6_mask.dip6 = bnxt_ulp_gen_l3_ipv6_addr_em_mask;
			v6_spec.sip6 = NULL;
			v6_mask.sip6 = NULL;
		}
		l3_parms.type = BNXT_ULP_GEN_L3_IPV6;
		l3_parms.v6_spec = &v6_spec;
		l3_parms.v6_mask = &v6_mask;
		netdev_dbg(bp->dev, "UDCC Add(%s) flow for session_id: %d IP %pI6 sQPn %x dQPn %x\n",
			   dir == CFA_DIR_RX ? "rx" : "tx",
			   entry->session_id,
			   entry->dst_ip.s6_addr,
			   src_qpn, dst_qpn);
	}

	if (dir == CFA_DIR_RX) {
		/* CNP for rx */
		bth_spec.op_code = &op_code;
		bth_mask.op_code = &op_code_mask;
	} else {
		/* N/A for tx */
		bth_spec.op_code = NULL;
		bth_mask.op_code = NULL;
	}

	/* Initialize QPn */
	bth_spec.dst_qpn = NULL;
	bth_mask.dst_qpn = NULL;
	if (per_qp_session) {
		if (dir == CFA_DIR_RX)
			bth_spec.dst_qpn = &src_qpn;
		else
			bth_spec.dst_qpn = &dst_qpn;
		bth_mask.dst_qpn = &msk_qpn;
	}
	l4_parms.type = BNXT_ULP_GEN_L4_BTH;
	l4_parms.bth_spec = &bth_spec;
	l4_parms.bth_mask = &bth_mask;

	if (dir == CFA_DIR_RX) {
		/* Pack the actions NIC template will use RoCE VNIC by default */
		actions.enables = BNXT_ULP_GEN_ACTION_ENABLES_COUNT;
		if (!BNXT_UDCC_DCQCN_EN(bp) && !BNXT_UDCC_HYBRID_MODE(bp))
			actions.enables |= BNXT_ULP_GEN_ACTION_ENABLES_DROP;
	} else {
		actions.enables = BNXT_ULP_GEN_ACTION_ENABLES_REDIRECT |
			BNXT_ULP_GEN_ACTION_ENABLES_COUNT;

		if (BNXT_UDCC_DCQCN_EN(bp)) {
			actions.enables |= BNXT_ULP_GEN_ACTION_ENABLES_SET_SMAC |
				BNXT_ULP_GEN_ACTION_ENABLES_SET_DMAC;

			if (is_valid_ether_addr(entry->dst_mac_mod) &&
			    is_valid_ether_addr(entry->src_mac_mod)) {
				ether_addr_copy(actions.dmac, entry->dst_mac_mod);
				ether_addr_copy(actions.smac, entry->src_mac_mod);
			} else {
				/* PF case (non-switchdev): zero smac and dmac modify.
				 * Just use the smac dmac given by FW in the entry.
				 */
				ether_addr_copy(actions.dmac, entry->dest_mac);
				ether_addr_copy(actions.smac, entry->src_mac);
			}
		}
	}
	actions.dst_fid = bp->pf.fw_fid;

	if (dir == CFA_DIR_RX) {
		parms.dir = BNXT_ULP_GEN_RX;
		parms.flow_id = &entry->rx_flow_id;
		parms.counter_hndl = &entry->rx_counter_hndl;
	} else {
		parms.dir = BNXT_ULP_GEN_TX;
		parms.flow_id = &entry->tx_flow_id;
		parms.counter_hndl = &entry->tx_counter_hndl;
	}
	parms.l2 = &l2_parms;
	parms.l3 = &l3_parms;
	parms.l4 = &l4_parms;
	parms.actions = &actions;
	parms.priority = 2; /* must be higher priority than NIC flow CNP */
	parms.enable_em = true;

	rc = bnxt_ulp_gen_flow_create(bp, bp->pf.fw_fid, &parms);
	if (rc) {
		netdev_warn(bp->dev, "UDCC TFC flow creation failed rc=%d\n", rc);
		return rc;
	}
	rc = bnxt_tfc_counter_update(bp, parms.counter_hndl);

	netdev_dbg(bp->dev, "UDCC Add(%s) flow for session_id: %d flow_id: %d cntr: 0x%llx\n",
		   dir == CFA_DIR_RX ? "rx" : "tx",
		   entry->session_id,
		   *parms.flow_id,
		   *parms.counter_hndl);
	return rc;
}

static int bnxt_udcc_flows_create_p7(struct bnxt *bp, struct bnxt_udcc_session_entry *entry)
{
	int rc;

	rc = bnxt_udcc_flow_create_p7(bp, entry, CFA_DIR_RX);
	if (rc)
		return rc;

	return bnxt_udcc_flow_create_p7(bp, entry, CFA_DIR_TX);
}

static int bnxt_udcc_rx_flow_create_v6(struct bnxt *bp,
				       struct bnxt_udcc_session_entry *entry)
{
	struct bnxt_ulp_gen_bth_hdr bth_spec = { 0 }, bth_mask = { 0 };
	struct bnxt_ulp_gen_ipv6_hdr v6_spec = { 0 }, v6_mask = { 0 };
	bool per_qp_session = BNXT_UDCC_SESSION_PER_QP(bp);
	struct bnxt_ulp_gen_l2_hdr_parms l2_parms = { 0 };
	struct bnxt_ulp_gen_l3_hdr_parms l3_parms = { 0 };
	struct bnxt_ulp_gen_l4_hdr_parms l4_parms = { 0 };
	struct bnxt_ulp_gen_action_parms actions = { 0 };
	struct bnxt_ulp_gen_flow_parms parms = { 0 };

	/* These would normally be preset and passed to the upper layer */
	u32 src_qpn = cpu_to_be32(entry->src_qp_num);
	u32 msk_qpn = cpu_to_be32(0xffffffff);
	u16 op_code = cpu_to_be16(0x81); /* RoCE CNP */
	u16 op_code_mask = cpu_to_be16(0xffff);
	u8 l4_proto = IPPROTO_UDP;
	u8 l4_proto_mask = 0xff;
	int rc;

	/* Pack the L2 Data - Don't fill l2_spec for now */
	l2_parms.type = BNXT_ULP_GEN_L2_L2_HDR;

	/* Pack the L3 Data */
	v6_spec.proto6 = &l4_proto;
	v6_mask.proto6 = &l4_proto_mask;
	v6_spec.dip6 = NULL;
	v6_mask.dip6 = NULL;
	v6_spec.sip6 = entry->dst_ip.s6_addr;
	v6_mask.sip6 = bnxt_ulp_gen_l3_ipv6_addr_em_mask;

	l3_parms.type = BNXT_ULP_GEN_L3_IPV6;
	l3_parms.v6_spec = &v6_spec;
	l3_parms.v6_mask = &v6_mask;

	/* Pack the L4 Data */
	bth_spec.op_code = &op_code;
	bth_mask.op_code = &op_code_mask;
	bth_spec.dst_qpn = NULL;
	bth_mask.dst_qpn = NULL;
	if (per_qp_session) {
		bth_spec.dst_qpn = &src_qpn;
		bth_mask.dst_qpn = &msk_qpn;
	}
	l4_parms.type = BNXT_ULP_GEN_L4_BTH;
	l4_parms.bth_spec = &bth_spec;
	l4_parms.bth_mask = &bth_mask;

	/* Pack the actions */
	actions.enables = BNXT_ULP_GEN_ACTION_ENABLES_REDIRECT |
		BNXT_ULP_GEN_ACTION_ENABLES_DROP |
		BNXT_ULP_GEN_ACTION_ENABLES_COUNT;
	actions.dst_fid = bp->pf.fw_fid;

	parms.dir = BNXT_ULP_GEN_RX;
	parms.flow_id = &entry->rx_flow_id;
	parms.counter_hndl = &entry->rx_counter_hndl;
	parms.l2 = &l2_parms;
	parms.l3 = &l3_parms;
	parms.l4 = &l4_parms;
	parms.actions = &actions;

	rc = bnxt_ulp_gen_flow_create(bp, bp->pf.fw_fid, &parms);
	if (rc)
		return rc;
	netdev_dbg(bp->dev, "UDCC Add Rx flow for session_id: %d flow_id: %d, counter: 0x%llx\n",
		   entry->session_id,
		   entry->rx_flow_id,
		   entry->rx_counter_hndl);

	return rc;
}

static int bnxt_udcc_tx_flow_create_v6(struct bnxt *bp,
				       struct bnxt_udcc_session_entry *entry)
{
	struct bnxt_ulp_gen_bth_hdr bth_spec = { 0 }, bth_mask = { 0 };
	struct bnxt_ulp_gen_ipv6_hdr v6_spec = { 0 }, v6_mask = { 0 };
	bool per_qp_session = BNXT_UDCC_SESSION_PER_QP(bp);
	struct bnxt_ulp_gen_l2_hdr_parms l2_parms = { 0 };
	struct bnxt_ulp_gen_l3_hdr_parms l3_parms = { 0 };
	struct bnxt_ulp_gen_l4_hdr_parms l4_parms = { 0 };
	struct bnxt_ulp_gen_action_parms actions = { 0 };
	struct bnxt_ulp_gen_flow_parms parms = { 0 };

	/* These would normally be preset and passed to the upper layer */
	u32 dst_qpn = cpu_to_be32(entry->dest_qp_num);
	u32 msk_qpn = cpu_to_be32(0xffffffff);
	u8 l4_proto = IPPROTO_UDP;
	u8 l4_proto_mask = 0xff;
	int rc;

	/* Pack the L2 Data - Don't fill l2_spec for now */
	l2_parms.type = BNXT_ULP_GEN_L2_L2_HDR;

	/* Pack the L3 Data */
	v6_spec.proto6 = &l4_proto;
	v6_mask.proto6 = &l4_proto_mask;
	v6_spec.sip6 = NULL;
	v6_mask.sip6 = NULL;
	v6_spec.dip6 = entry->dst_ip.s6_addr;
	v6_mask.dip6 = bnxt_ulp_gen_l3_ipv6_addr_em_mask;

	l3_parms.type = BNXT_ULP_GEN_L3_IPV6;
	l3_parms.v6_spec = &v6_spec;
	l3_parms.v6_mask = &v6_mask;

	/* Pack the L4 Data */
	bth_spec.op_code = NULL;
	bth_mask.op_code = NULL;
	bth_spec.dst_qpn = NULL;
	bth_mask.dst_qpn = NULL;
	if (per_qp_session) {
		bth_spec.dst_qpn = &dst_qpn;
		bth_mask.dst_qpn = &msk_qpn;
	}
	l4_parms.type = BNXT_ULP_GEN_L4_BTH;
	l4_parms.bth_spec = &bth_spec;
	l4_parms.bth_mask = &bth_mask;

	/* Pack the actions */
	actions.enables = BNXT_ULP_GEN_ACTION_ENABLES_REDIRECT |
		BNXT_ULP_GEN_ACTION_ENABLES_SET_SMAC |
		BNXT_ULP_GEN_ACTION_ENABLES_SET_DMAC |
		BNXT_ULP_GEN_ACTION_ENABLES_COUNT;

	actions.dst_fid = bp->pf.fw_fid;
	if (is_valid_ether_addr(entry->dst_mac_mod) &&
	    is_valid_ether_addr(entry->src_mac_mod)) {
		ether_addr_copy(actions.dmac, entry->dst_mac_mod);
		ether_addr_copy(actions.smac, entry->src_mac_mod);
	} else {
		/* PF case (non-switchdev): zero smac and dmac modify.
		 * Just use the smac dmac given by FW in the entry.
		 */
		ether_addr_copy(actions.dmac, entry->dest_mac);
		ether_addr_copy(actions.smac, entry->src_mac);
	}

	parms.dir = BNXT_ULP_GEN_TX;
	parms.flow_id = &entry->tx_flow_id;
	parms.counter_hndl = &entry->tx_counter_hndl;
	parms.l2 = &l2_parms;
	parms.l3 = &l3_parms;
	parms.l4 = &l4_parms;
	parms.actions = &actions;

	rc = bnxt_ulp_gen_flow_create(bp, bp->pf.fw_fid, &parms);
	if (rc)
		return rc;
	netdev_dbg(bp->dev, "UDCC Add Tx flow for session_id: %d flow_id: %d, counter: 0x%llx\n",
		   entry->session_id,
		   entry->tx_flow_id,
		   entry->tx_counter_hndl);

	return rc;
}

static int bnxt_udcc_rx_flow_create_v4(struct bnxt *bp,
				       struct bnxt_udcc_session_entry *entry)
{
	struct bnxt_ulp_gen_bth_hdr bth_spec = { 0 }, bth_mask = { 0 };
	struct bnxt_ulp_gen_ipv4_hdr v4_spec = { 0 }, v4_mask = { 0 };
	bool per_qp_session = BNXT_UDCC_SESSION_PER_QP(bp);
	struct bnxt_ulp_gen_l2_hdr_parms l2_parms = { 0 };
	struct bnxt_ulp_gen_l3_hdr_parms l3_parms = { 0 };
	struct bnxt_ulp_gen_l4_hdr_parms l4_parms = { 0 };
	struct bnxt_ulp_gen_action_parms actions = { 0 };
	struct bnxt_ulp_gen_flow_parms parms = { 0 };

	/* These would normally be preset and passed to the upper layer */
	u32 src_qpn = cpu_to_be32(entry->src_qp_num);
	u32 msk_qpn = cpu_to_be32(0xffffffff);
	u16 op_code = cpu_to_be16(0x81); /* RoCE CNP */
	u16 op_code_mask = cpu_to_be16(0xffff);
	u8 l4_proto = IPPROTO_UDP;
	u8 l4_proto_mask = 0xff;
	int rc;

	/* Pack the L2 Data - Don't fill l2_spec for now */
	l2_parms.type = BNXT_ULP_GEN_L2_L2_HDR;

	/* Pack the L3 Data */
	v4_spec.proto = &l4_proto;
	v4_mask.proto = &l4_proto_mask;
	v4_spec.dip = NULL;
	v4_mask.dip = NULL;
	v4_spec.sip = (u32 *)&entry->dst_ip.s6_addr32[3];
	v4_mask.sip = (u32 *)bnxt_ulp_gen_l3_ipv4_addr_em_mask;

	l3_parms.type = BNXT_ULP_GEN_L3_IPV4;
	l3_parms.v4_spec = &v4_spec;
	l3_parms.v4_mask = &v4_mask;

	/* Pack the L4 Data */
	bth_spec.op_code = &op_code;
	bth_mask.op_code = &op_code_mask;
	bth_spec.dst_qpn = NULL;
	bth_mask.dst_qpn = NULL;
	if (per_qp_session) {
		bth_spec.dst_qpn = &src_qpn;
		bth_mask.dst_qpn = &msk_qpn;
	}
	l4_parms.type = BNXT_ULP_GEN_L4_BTH;
	l4_parms.bth_spec = &bth_spec;
	l4_parms.bth_mask = &bth_mask;

	/* Pack the actions */
	actions.enables = BNXT_ULP_GEN_ACTION_ENABLES_REDIRECT |
		BNXT_ULP_GEN_ACTION_ENABLES_DROP |
		BNXT_ULP_GEN_ACTION_ENABLES_COUNT;
	actions.dst_fid = bp->pf.fw_fid;

	parms.dir = BNXT_ULP_GEN_RX;
	parms.flow_id = &entry->rx_flow_id;
	parms.counter_hndl = &entry->rx_counter_hndl;
	parms.l2 = &l2_parms;
	parms.l3 = &l3_parms;
	parms.l4 = &l4_parms;
	parms.actions = &actions;

	rc = bnxt_ulp_gen_flow_create(bp, bp->pf.fw_fid, &parms);
	if (rc)
		return rc;
	netdev_dbg(bp->dev, "UDCC Add Rx flow for session_id: %d flow_id: %d, counter: 0x%llx\n",
		   entry->session_id,
		   entry->rx_flow_id,
		   entry->rx_counter_hndl);

	return rc;
}

static int bnxt_udcc_tx_flow_create_v4(struct bnxt *bp,
				       struct bnxt_udcc_session_entry *entry)
{
	struct bnxt_ulp_gen_bth_hdr bth_spec = { 0 }, bth_mask = { 0 };
	struct bnxt_ulp_gen_ipv4_hdr v4_spec = { 0 }, v4_mask = { 0 };
	bool per_qp_session = BNXT_UDCC_SESSION_PER_QP(bp);
	struct bnxt_ulp_gen_l2_hdr_parms l2_parms = { 0 };
	struct bnxt_ulp_gen_l3_hdr_parms l3_parms = { 0 };
	struct bnxt_ulp_gen_l4_hdr_parms l4_parms = { 0 };
	struct bnxt_ulp_gen_action_parms actions = { 0 };
	struct bnxt_ulp_gen_flow_parms parms = { 0 };

	/* These would normally be preset and passed to the upper layer */
	u32 dst_qpn = cpu_to_be32(entry->dest_qp_num);
	u32 msk_qpn = cpu_to_be32(0xffffffff);
	u8 l4_proto = IPPROTO_UDP;
	u8 l4_proto_mask = 0xff;
	int rc;

	/* Pack the L2 Data - Don't fill l2_spec for now */
	l2_parms.type = BNXT_ULP_GEN_L2_L2_HDR;

	/* Pack the L3 Data */
	v4_spec.proto = &l4_proto;
	v4_mask.proto = &l4_proto_mask;
	v4_spec.sip = NULL;
	v4_mask.sip = NULL;
	v4_spec.dip = (u32 *)&entry->dst_ip.s6_addr32[3];
	v4_mask.dip = (u32 *)bnxt_ulp_gen_l3_ipv4_addr_em_mask;

	l3_parms.type = BNXT_ULP_GEN_L3_IPV4;
	l3_parms.v4_spec = &v4_spec;
	l3_parms.v4_mask = &v4_mask;

	/* Pack the L4 Data */
	bth_spec.op_code = NULL;
	bth_mask.op_code = NULL;
	bth_spec.dst_qpn = NULL;
	bth_mask.dst_qpn = NULL;
	if (per_qp_session) {
		bth_spec.dst_qpn = &dst_qpn;
		bth_mask.dst_qpn = &msk_qpn;
	}
	l4_parms.type = BNXT_ULP_GEN_L4_BTH;
	l4_parms.bth_spec = &bth_spec;
	l4_parms.bth_mask = &bth_mask;

	/* Pack the actions */
	actions.enables = BNXT_ULP_GEN_ACTION_ENABLES_REDIRECT |
		BNXT_ULP_GEN_ACTION_ENABLES_SET_SMAC |
		BNXT_ULP_GEN_ACTION_ENABLES_SET_DMAC |
		BNXT_ULP_GEN_ACTION_ENABLES_COUNT;

	actions.dst_fid = bp->pf.fw_fid;
	if (is_valid_ether_addr(entry->dst_mac_mod) &&
	    is_valid_ether_addr(entry->src_mac_mod)) {
		ether_addr_copy(actions.dmac, entry->dst_mac_mod);
		ether_addr_copy(actions.smac, entry->src_mac_mod);
	} else {
		/* PF case (non-switchdev): zero smac and dmac modify.
		 * Just use the smac dmac given by FW in the entry.
		 */
		ether_addr_copy(actions.dmac, entry->dest_mac);
		ether_addr_copy(actions.smac, entry->src_mac);
	}

	parms.dir = BNXT_ULP_GEN_TX;
	parms.flow_id = &entry->tx_flow_id;
	parms.counter_hndl = &entry->tx_counter_hndl;
	parms.l2 = &l2_parms;
	parms.l3 = &l3_parms;
	parms.l4 = &l4_parms;
	parms.actions = &actions;

	rc = bnxt_ulp_gen_flow_create(bp, bp->pf.fw_fid, &parms);
	if (rc)
		return rc;
	netdev_dbg(bp->dev, "UDCC Add Tx flow for session_id: %d flow_id: %d, counter: 0x%llx\n",
		   entry->session_id,
		   entry->tx_flow_id,
		   entry->tx_counter_hndl);

	return rc;
}

static int bnxt_udcc_flows_create_v6(struct bnxt *bp,
				     struct bnxt_udcc_session_entry *entry)
{
	int rc;

	rc = bnxt_udcc_rx_flow_create_v6(bp, entry);
	if (rc)
		return rc;

	rc = bnxt_udcc_tx_flow_create_v6(bp, entry);
	return rc;
}

static int bnxt_udcc_flows_create_v4(struct bnxt *bp,
				     struct bnxt_udcc_session_entry *entry)
{
	int rc;

	rc = bnxt_udcc_rx_flow_create_v4(bp, entry);
	if (rc)
		return rc;

	rc = bnxt_udcc_tx_flow_create_v4(bp, entry);
	return rc;
}

static int bnxt_udcc_flows_create(struct bnxt *bp,
				  struct bnxt_udcc_session_entry *entry)
{
	if (entry->v4_dst)
		return bnxt_udcc_flows_create_v4(bp, entry);

	return bnxt_udcc_flows_create_v6(bp, entry);
}

/* The dip gets encoded as the RoCEv2 GID. The third integer
 * should be FFFF0000 if the encoded address is IPv4.
 * Example: GID:		::ffff:171.16.10.1
 */
#define	BNXT_UDCC_DIP_V4_MASK	0xFFFF0000
static bool bnxt_is_udcc_dip_ipv4(struct bnxt *bp, struct in6_addr *dip)
{
	netdev_dbg(bp->dev, "%s: s6_addr32[0]: 0x%x s6_addr32[1]: 0x%x\n",
		   __func__, dip->s6_addr32[0], dip->s6_addr32[1]);
	netdev_dbg(bp->dev, "%s: s6_addr32[2]: 0x%x s6_addr32[3]: 0x%x\n",
		   __func__, dip->s6_addr32[2], dip->s6_addr32[3]);

	if ((dip->s6_addr32[2] & BNXT_UDCC_DIP_V4_MASK) ==
	    BNXT_UDCC_DIP_V4_MASK)
		return true;

	return false;
}

/* Insert a new session entry into the database */
static int bnxt_udcc_create_session(struct bnxt *bp, u32 session_id)
{
	struct bnxt_udcc_info *udcc = bp->udcc_info;
	struct bnxt_udcc_session_entry *entry;
	int rc;

	entry = kzalloc(sizeof(*entry), GFP_KERNEL);
	if (!entry)
		return -ENOMEM;

	entry->session_id = session_id;

	/* ====================================================================
	 * 1.Issue HWRM_UDCC_SESSION_QCFG to get the session details
	 *
	 * 2.Use the returned DIP to invoke TF API to get flow_ids/counter_hndls
	 *  for Tx/Tx
	 * a) Use the DIP to query the smac/dmac - TF API
	 * b) Add a Tx flow using DIP, action_param - modify dmac/smac,count
	 * c) Add a Rx flow using DIP as SIP, match: CNP, action: count
	 *
	 * 3. Issue HWRM_UDCC_SESSION_CFG to update the FW
	 */
	rc = bnxt_hwrm_udcc_session_qcfg(bp, entry);
	if (rc)
		goto create_sess_exit1;

	if (BNXT_CHIP_P7(bp)) {
		entry->v4_dst = bnxt_is_udcc_dip_ipv4(bp, &entry->dst_ip);
		rc = bnxt_udcc_flows_create_p7(bp, entry);
		if (rc) {
			netdev_dbg(bp->dev, "UDCC flow create failed rc=%d\n", rc);
			goto create_sess_exit2;
		}
	} else {
		entry->v4_dst = bnxt_is_udcc_dip_ipv4(bp, &entry->dst_ip);
		rc = bnxt_ulp_udcc_v6_subnet_check(bp, bp->pf.fw_fid, &entry->dst_ip,
						   entry->dst_mac_mod,
						   entry->src_mac_mod);
		if (rc) {
			if (rc != -ENOENT) {
				netdev_dbg(bp->dev, "UDCC create session_id: %d, failed rc:%d\n",
					   session_id, rc);
				goto create_sess_exit1;
			}
			entry->skip_subnet_checking = true;
		}
		rc = bnxt_udcc_flows_create(bp, entry);
		if (rc) {
			netdev_dbg(bp->dev, "UDCC flow create failed rc=%d\n", rc);
			goto create_sess_exit2;
		}
	}
	entry->state = UDCC_SESSION_CFG_REQ_SESSION_STATE_ENABLED;
	rc = bnxt_hwrm_udcc_session_cfg(bp, entry);
	if (rc)
		goto create_sess_exit2;

	mutex_lock(&udcc->session_db_lock);
	udcc->session_db[session_id] = entry;
	udcc->session_count++;
	mutex_unlock(&udcc->session_db_lock);

	return 0;
create_sess_exit2:
	bnxt_tf_ulp_flow_delete(bp, entry);
create_sess_exit1:
	entry->state = UDCC_SESSION_CFG_REQ_SESSION_STATE_FLOW_NOT_CREATED;
	bnxt_hwrm_udcc_session_cfg(bp, entry);
	kfree(entry);
	return rc;
}

static int bnxt_tf_ulp_flow_delete(struct bnxt *bp, struct bnxt_udcc_session_entry *entry)
{
	int rc = 0;

	/* Delete the TF flows for Rx/Tx */
	if (entry->rx_flow_id) {
		rc = bnxt_ulp_gen_flow_destroy(bp, bp->pf.fw_fid,
					       entry->rx_flow_id);
		if (!rc) {
			netdev_dbg(bp->dev,
				   "UDCC Delete Rx flow_id: %d session: %d\n",
				   entry->rx_flow_id, entry->session_id);
		} else {
			netdev_dbg(bp->dev,
				   "UDCC Delete Rx flow_id: %d session: %d failed rc: %d\n",
				   entry->rx_flow_id, entry->session_id, rc);
		}
		entry->rx_flow_id = 0;
		entry->rx_counter_hndl = 0;
	}

	if (entry->tx_flow_id) {
		rc = bnxt_ulp_gen_flow_destroy(bp, bp->pf.fw_fid,
					       entry->tx_flow_id);
		if (!rc) {
			netdev_dbg(bp->dev,
				   "UDCC Delete Tx flow_id: %d session: %d\n",
				   entry->tx_flow_id, entry->session_id);
		} else {
			netdev_dbg(bp->dev,
				   "UDCC Delete Tx flow_id: %d session: %d failed rc: %d\n",
				   entry->tx_flow_id, entry->session_id, rc);
		}
		entry->tx_flow_id = 0;
		entry->tx_counter_hndl = 0;
	}

	return rc;
}

static int bnxt_udcc_delete_session(struct bnxt *bp, u32 session_id, bool cleanup)
{
	struct bnxt_udcc_info *udcc = bp->udcc_info;
	struct bnxt_udcc_session_entry *entry;
	int rc = 0;

	mutex_lock(&udcc->session_db_lock);
	entry = udcc->session_db[session_id];
	if (!entry) {
		/* UDCC session entry can be NULL, if the session create had failed,
		 * no need to do anything or report error
		 */
		netdev_dbg(bp->dev, "UDCC entry is NULL for session: %d\n",
			   session_id);
		goto exit;
	}

	rc = bnxt_tf_ulp_flow_delete(bp, entry);
	if (rc) {
		netdev_dbg(bp->dev,
			   "Failed to delete UDCC flows, session: %d\n",
			   session_id);
	}

	/* No need to issue udcc_session_cfg command when
	 * firmware is in reset state.
	 */
	if (test_bit(BNXT_STATE_IN_FW_RESET, &bp->state) || cleanup)
		goto cleanup_udcc_session;

	entry->state = UDCC_SESSION_CFG_REQ_SESSION_STATE_FLOW_HAS_BEEN_DELETED;
	rc = bnxt_hwrm_udcc_session_cfg(bp, entry);
	if (rc) {
		netdev_dbg(bp->dev, "Failed to delete UDCC session: %d\n",
			   session_id);
		goto exit;
	}

cleanup_udcc_session:
	kfree(entry);
	udcc->session_db[session_id] = NULL;
	udcc->session_count--;

	netdev_dbg(bp->dev, "Deleted UDCC session: %d\n", session_id);
exit:
	mutex_unlock(&udcc->session_db_lock);
	return rc;
}

void bnxt_udcc_session_db_cleanup(struct bnxt *bp)
{
	struct bnxt_udcc_info *udcc = bp->udcc_info;
	int i;

	if (!udcc)
		return;

	for (i = 0; i < BNXT_UDCC_MAX_SESSIONS; i++)
		bnxt_udcc_delete_session(bp, i, false);
}

void bnxt_udcc_update_session(struct bnxt *bp, bool suspend)
{
	u8 tf_event;

	if (!bp->udcc_info)
		return;

	if (suspend)
		tf_event = BNXT_UDCC_INFO_TF_EVENT_SUSPEND;
	else
		tf_event = BNXT_UDCC_INFO_TF_EVENT_UNSUSPEND;

	if (test_and_set_bit(tf_event, &bp->udcc_info->tf_events))
		return;
	bnxt_queue_udcc_work(bp, BNXT_UDCC_MAX_SESSIONS + 1,
			     BNXT_UDCC_SESSION_UPDATE, suspend);
}

static void bnxt_udcc_suspend_session(struct bnxt *bp,
				      u8 orig_state,
				      struct bnxt_udcc_session_entry *entry)
{
	int rc;

	bnxt_tf_ulp_flow_delete(bp, entry);
	entry->state = !UDCC_SESSION_CFG_REQ_SESSION_STATE_ENABLED;

	rc = bnxt_hwrm_udcc_session_cfg(bp, entry);
	if (rc) {
		netdev_warn(bp->dev, "UDCC failed to suspend session: %d\n",
			    entry->session_id);
		entry->state = orig_state;
	} else {
		netdev_dbg(bp->dev, "UDCC update session: %d is SUSPENDED\n",
			   entry->session_id);
	}
}

static void bnxt_udcc_unsuspend_session(struct bnxt *bp,
					u8 orig_state,
					struct bnxt_udcc_session_entry *entry)
{
	int rc;

	bnxt_udcc_flows_create(bp, entry);
	entry->state = UDCC_SESSION_CFG_REQ_SESSION_STATE_ENABLED;

	rc = bnxt_hwrm_udcc_session_cfg(bp, entry);
	if (rc) {
		netdev_warn(bp->dev, "UDCC failed to unsuspend session: %d\n",
			    entry->session_id);
		entry->state = orig_state;
	} else {
		netdev_dbg(bp->dev, "UDCC update session: %d is UNSUSPENDED\n",
			   entry->session_id);
	}
}

static void __bnxt_udcc_update_session(struct bnxt *bp, bool suspend)
{
	struct bnxt_udcc_info *udcc = bp->udcc_info;
	u8 orig_state;
	bool found;
	int i;

	mutex_lock(&udcc->session_db_lock);
	if (!udcc->session_count)
		goto exit;

	for (i = 0; i < BNXT_UDCC_MAX_SESSIONS; i++) {
		struct bnxt_udcc_session_entry *entry;
		u8 dmac[ETH_ALEN] = {0};
		u8 smac[ETH_ALEN] = {0};

		entry = udcc->session_db[i];
		if (!entry)
			continue;

		if (entry->skip_subnet_checking)
			continue;

		found = !bnxt_ulp_udcc_v6_subnet_check(bp, bp->pf.fw_fid,
						       &entry->dst_ip,
						       dmac,
						       smac);

		orig_state = entry->state;

		if (suspend && found &&
		    orig_state == UDCC_SESSION_CFG_REQ_SESSION_STATE_ENABLED) {
			if ((!ether_addr_equal(entry->dst_mac_mod, dmac)) ||
			    (!ether_addr_equal(entry->src_mac_mod, smac))) {
				/* Update the mod dmac and smac */
				ether_addr_copy(entry->dst_mac_mod, dmac);
				ether_addr_copy(entry->src_mac_mod, smac);

				/* Suspend and Unsuspend if action changed */
				bnxt_udcc_suspend_session(bp, orig_state, entry);
				bnxt_udcc_unsuspend_session(bp, orig_state, entry);
			}
		} else if (suspend && !found &&
			   orig_state == UDCC_SESSION_CFG_REQ_SESSION_STATE_ENABLED) {
			/* Suspend */
			bnxt_udcc_suspend_session(bp, orig_state, entry);
		} else if (!suspend && found &&
			   orig_state == !UDCC_SESSION_CFG_REQ_SESSION_STATE_ENABLED) {
			/* Unsuspend */
			bnxt_udcc_unsuspend_session(bp, orig_state, entry);
		}

	}
exit:
	mutex_unlock(&udcc->session_db_lock);
}

void bnxt_udcc_task(struct work_struct *work)
{
	struct bnxt_udcc_work *udcc_work =
			container_of(work, struct bnxt_udcc_work, work);
	struct bnxt *bp = udcc_work->bp;
	int rc;

	set_bit(BNXT_STATE_IN_UDCC_TASK, &bp->state);
	/* Adding memory barrier to set the IN_UDCC_TASK bit first */
	smp_mb__after_atomic();
	if (!bp->udcc_info->bnxt_udcc_wq) {
		clear_bit(BNXT_STATE_IN_UDCC_TASK, &bp->state);
		return;
	}

	switch (udcc_work->session_opcode) {
	case BNXT_UDCC_SESSION_CREATE:
		rc = bnxt_udcc_create_session(bp, udcc_work->session_id);
		if (rc) {
			netdev_err(bp->dev,
				   "UDCC session_id: %d, session create failed rc=%d\n",
				   udcc_work->session_id, rc);
		}
		break;

	case BNXT_UDCC_SESSION_DELETE:
		rc = bnxt_udcc_delete_session(bp, udcc_work->session_id, false);
		if (rc) {
			netdev_err(bp->dev,
				   "UDCC session_id: %d: session delete failed rc=%d\n",
				   udcc_work->session_id, rc);
		}
		break;
	case BNXT_UDCC_SESSION_UPDATE:
		/* Check whether the BNXT_UDCC_SESSION_UPDATE event is from TF or Firmware.
		 * Clear the tf_events bits only if this event is from TF.
		 */
		if (udcc_work->session_id == BNXT_UDCC_MAX_SESSIONS + 1) {
			/* Since UDCC session update events are not specific to a particular
			 * session, we might end up missing an update for a different session
			 * (e.g different subnet), if we are already in the middle of processing
			 * in __bnxt_udcc_update_session(). To avoid this, clear the bit first
			 * before we enter __bnxt_udcc_update_session() to allow a subsequent
			 * event to schedule the task again.
			 */
			if (udcc_work->session_suspend)
				clear_bit(BNXT_UDCC_INFO_TF_EVENT_SUSPEND,
					  &bp->udcc_info->tf_events);
			else
				clear_bit(BNXT_UDCC_INFO_TF_EVENT_UNSUSPEND,
					  &bp->udcc_info->tf_events);
		}
		__bnxt_udcc_update_session(bp, udcc_work->session_suspend);
		break;
	default:
		netdev_warn(bp->dev, "Invalid UDCC session opcode session_id: %d\n",
			    udcc_work->session_id);
	}

	/* Complete all memory stores before setting bit. */
	smp_mb__before_atomic();
	clear_bit(BNXT_STATE_IN_UDCC_TASK, &bp->state);
	kfree(udcc_work);
}

void bnxt_free_udcc_info(struct bnxt *bp)
{
	struct bnxt_udcc_info *udcc = bp->udcc_info;
	int i;

	if (!udcc)
		return;

	for (i = 0; i < BNXT_UDCC_MAX_SESSIONS; i++)
		bnxt_udcc_delete_session(bp, i, true);

	kfree(udcc);
	bp->udcc_info = NULL;

	netdev_dbg(bp->dev, "%s(): udcc_info freed up!\n", __func__);
}

#else /* if defined(CONFIG_BNXT_FLOWER_OFFLOAD) */

void bnxt_free_udcc_info(struct bnxt *bp)
{
}

int bnxt_alloc_udcc_info(struct bnxt *bp)
{
	return 0;
}

void bnxt_udcc_task(struct work_struct *work)
{
}

void bnxt_udcc_session_db_cleanup(struct bnxt *bp)
{
}

int bnxt_start_udcc_worker(struct bnxt *bp)
{
	return 0;
}

void bnxt_stop_udcc_worker(struct bnxt *bp)
{
}
#endif /* if defined(CONFIG_BNXT_FLOWER_OFFLOAD) */
