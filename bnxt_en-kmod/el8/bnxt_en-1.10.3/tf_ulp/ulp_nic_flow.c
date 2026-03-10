// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2024 Broadcom
 * All rights reserved.
 */

#include <linux/vmalloc.h>
#include <linux/if_ether.h>
#include <linux/atomic.h>
#include <linux/ipv6.h>
#include <linux/in6.h>
#include <linux/err.h>
#include <linux/limits.h>
#include "bnxt_compat.h"
#include "bnxt_hsi.h"
#include "bnxt.h"
#include "bnxt_vfr.h"
#include "bnxt_tf_ulp.h"
#include "bnxt_udcc.h"
#include "tfc.h"
#include "tfc_util.h"
#include "ulp_nic_flow.h"
#include "ulp_generic_flow_offload.h"
#include "ulp_flow_db.h"

#if defined(CONFIG_BNXT_FLOWER_OFFLOAD)

static int roce_lpbk_flow_create(struct bnxt *bp,
				 u32 *flow_id, u64 *flow_cnt_hndl,
				 bool is_v6, enum cfa_dir dir)
{
	struct bnxt_ulp_gen_bth_hdr bth_spec = { 0 }, bth_mask = { 0 };
	struct bnxt_ulp_gen_ipv6_hdr v6_spec = { 0 }, v6_mask = { 0 };
	struct bnxt_ulp_gen_ipv4_hdr v4_spec = { 0 }, v4_mask = { 0 };
	struct bnxt_ulp_gen_l2_hdr_parms l2_parms = { 0 };
	struct bnxt_ulp_gen_l3_hdr_parms l3_parms = { 0 };
	struct bnxt_ulp_gen_l4_hdr_parms l4_parms = { 0 };
	struct bnxt_ulp_gen_action_parms actions = { 0 };
	struct bnxt_ulp_gen_flow_parms parms = { 0 };
	struct bnxt_ulp_gen_eth_hdr eth_spec = { 0 };
	struct bnxt_ulp_gen_eth_hdr eth_mask = { 0 };
	u16 etype_spec = is_v6 ? cpu_to_be16(ETH_P_IPV6) :
		cpu_to_be16(ETH_P_IP);
	u16 etype_mask = cpu_to_be16(0xffff);
	u8 dst_spec[ETH_ALEN] = { 0 };
	u8 dst_mask[ETH_ALEN] = { 0 };
	u8 l4_proto = IPPROTO_UDP;
	u8 l4_proto_mask = 0xff;
	int rc = 0;

	ether_addr_copy(dst_spec, bp->dev->dev_addr);
	eth_broadcast_addr(dst_mask);
	eth_spec.dst = &dst_spec[0];
	eth_mask.dst = &dst_mask[0];
	eth_spec.type = &etype_spec;
	eth_mask.type = &etype_mask;
	l2_parms.eth_spec = &eth_spec;
	l2_parms.eth_mask = &eth_mask;
	l2_parms.type = BNXT_ULP_GEN_L2_L2_HDR;

	if (is_v6) {
		/* Pack the L3 Data */
		v6_spec.proto6 = &l4_proto;
		v6_mask.proto6 = &l4_proto_mask;
		v6_spec.dip6 = NULL;
		v6_mask.dip6 = NULL;
		v6_spec.sip6 = NULL;
		v6_mask.sip6 = NULL;

		l3_parms.type = BNXT_ULP_GEN_L3_IPV6;
		l3_parms.v6_spec = &v6_spec;
		l3_parms.v6_mask = &v6_mask;
	} else {
		/* Pack the L3 Data */
		v4_spec.proto = &l4_proto;
		v4_mask.proto = &l4_proto_mask;
		v4_spec.dip = NULL;
		v4_mask.dip = NULL;
		v4_spec.sip = NULL;
		v4_mask.sip = NULL;

		l3_parms.type = BNXT_ULP_GEN_L3_IPV4;
		l3_parms.v4_spec = &v4_spec;
		l3_parms.v4_mask = &v4_mask;
	}

	/* Pack the L4 Data */
	l4_parms.type = BNXT_ULP_GEN_L4_BTH;
	bth_spec.op_code = NULL;
	bth_mask.op_code = NULL;
	bth_spec.dst_qpn = NULL;
	bth_mask.dst_qpn = NULL;
	l4_parms.bth_spec = &bth_spec;
	l4_parms.bth_mask = &bth_mask;

	/* Pack the actions - NIC template will use RoCE VNIC always by default */
	if (dir == CFA_DIR_RX) {
		actions.enables = BNXT_ULP_GEN_ACTION_ENABLES_COUNT;
	} else {
		actions.enables = BNXT_ULP_GEN_ACTION_ENABLES_COUNT |
			BNXT_ULP_GEN_ACTION_ENABLES_REDIRECT_LOOPBACK;
	}
	actions.dst_fid = bp->pf.fw_fid;

	if (dir == CFA_DIR_RX)
		parms.dir = BNXT_ULP_GEN_RX;
	else
		parms.dir = BNXT_ULP_GEN_TX;
	parms.flow_id = flow_id;
	parms.counter_hndl = flow_cnt_hndl;
	parms.l2 = &l2_parms;
	parms.l3 = &l3_parms;
	parms.l4 = &l4_parms;
	parms.actions = &actions;

	/* if DMAC is our PF mac then we just send all these packets to LPBK */
	parms.lkup_strength = FLOW_LKUP_STRENGTH_HI;
	parms.priority = ULP_NIC_FLOW_ROCE_LPBK_PRI;
	rc = bnxt_ulp_gen_flow_create(bp, bp->pf.fw_fid, &parms);
	if (rc)
		return rc;

	netdev_dbg(bp->dev, "%s: NIC FLOW ROCE LPBK (%s) (%s) add flow_id: %d, ctr: 0x%llx\n",
		   __func__,
		   (is_v6 ? "v6" : "v4"),
		   tfc_dir_2_str(dir),
		   *flow_id,
		   *flow_cnt_hndl);
	return rc;
}

static int roce_flow_create(struct bnxt *bp,
			    u32 *flow_id, u64 *flow_cnt_hndl,
			    bool is_v6, enum cfa_dir dir)
{
	struct bnxt_ulp_gen_bth_hdr bth_spec = { 0 }, bth_mask = { 0 };
	struct bnxt_ulp_gen_ipv6_hdr v6_spec = { 0 }, v6_mask = { 0 };
	struct bnxt_ulp_gen_ipv4_hdr v4_spec = { 0 }, v4_mask = { 0 };
	struct bnxt_ulp_gen_l2_hdr_parms l2_parms = { 0 };
	struct bnxt_ulp_gen_l3_hdr_parms l3_parms = { 0 };
	struct bnxt_ulp_gen_l4_hdr_parms l4_parms = { 0 };
	struct bnxt_ulp_gen_action_parms actions = { 0 };
	struct bnxt_ulp_gen_flow_parms parms = { 0 };
	u8 l4_proto = IPPROTO_UDP;
	u8 l4_proto_mask = 0xff;
	int rc = 0;

	l2_parms.type = BNXT_ULP_GEN_L2_L2_HDR;

	if (is_v6) {
		/* Pack the L3 Data */
		v6_spec.proto6 = &l4_proto;
		v6_mask.proto6 = &l4_proto_mask;
		v6_spec.dip6 = NULL;
		v6_mask.dip6 = NULL;
		v6_spec.sip6 = NULL;
		v6_mask.sip6 = NULL;

		l3_parms.type = BNXT_ULP_GEN_L3_IPV6;
		l3_parms.v6_spec = &v6_spec;
		l3_parms.v6_mask = &v6_mask;
	} else {
		/* Pack the L3 Data */
		v4_spec.proto = &l4_proto;
		v4_mask.proto = &l4_proto_mask;
		v4_spec.dip = NULL;
		v4_mask.dip = NULL;
		v4_spec.sip = NULL;
		v4_mask.sip = NULL;

		l3_parms.type = BNXT_ULP_GEN_L3_IPV4;
		l3_parms.v4_spec = &v4_spec;
		l3_parms.v4_mask = &v4_mask;
	}

	/* Pack the L4 Data */
	l4_parms.type = BNXT_ULP_GEN_L4_BTH;
	bth_spec.op_code = NULL;
	bth_mask.op_code = NULL;
	bth_spec.dst_qpn = NULL;
	bth_mask.dst_qpn = NULL;
	l4_parms.bth_spec = &bth_spec;
	l4_parms.bth_mask = &bth_mask;

	/* Pack the actions - NIC template will use RoCE VNIC always by default */
	if (dir == CFA_DIR_RX) {
		actions.enables = BNXT_ULP_GEN_ACTION_ENABLES_COUNT;
	} else {
		actions.enables = BNXT_ULP_GEN_ACTION_ENABLES_COUNT |
			BNXT_ULP_GEN_ACTION_ENABLES_REDIRECT;
	}
	actions.dst_fid = bp->pf.fw_fid;

	if (dir == CFA_DIR_RX)
		parms.dir = BNXT_ULP_GEN_RX;
	else
		parms.dir = BNXT_ULP_GEN_TX;
	parms.flow_id = flow_id;
	parms.counter_hndl = flow_cnt_hndl;
	parms.l2 = &l2_parms;
	parms.l3 = &l3_parms;
	parms.l4 = &l4_parms;
	parms.actions = &actions;

	/* must be lower priority than ROCE CNP */
	parms.lkup_strength = FLOW_LKUP_STRENGTH_LO;
	parms.priority = ULP_NIC_FLOW_ROCE_PRI;
	rc = bnxt_ulp_gen_flow_create(bp, bp->pf.fw_fid, &parms);
	if (rc)
		return rc;

	netdev_dbg(bp->dev, "%s: NIC FLOW ROCE(%s) (%s) add flow_id: %d, ctr: 0x%llx\n",
		   __func__,
		   (is_v6 ? "v6" : "v4"),
		   tfc_dir_2_str(dir),
		   *flow_id,
		   *flow_cnt_hndl);
	return rc;
}

static int roce_cnp_flow_create(struct bnxt *bp,
				u32 *cnp_flow_id, u64 *cnp_flow_cnt_hndl,
				bool is_v6, enum cfa_dir dir)
{
	struct bnxt_ulp_gen_bth_hdr bth_spec = { 0 }, bth_mask = { 0 };
	struct bnxt_ulp_gen_ipv6_hdr v6_spec = { 0 }, v6_mask = { 0 };
	struct bnxt_ulp_gen_ipv4_hdr v4_spec = { 0 }, v4_mask = { 0 };
	struct bnxt_ulp_gen_l2_hdr_parms l2_parms = { 0 };
	struct bnxt_ulp_gen_l3_hdr_parms l3_parms = { 0 };
	struct bnxt_ulp_gen_l4_hdr_parms l4_parms = { 0 };
	struct bnxt_ulp_gen_action_parms actions = { 0 };
	struct bnxt_ulp_gen_flow_parms parms = { 0 };
	u16 op_code = cpu_to_be16(0x81); /* RoCE CNP */
	u16 op_code_mask = cpu_to_be16(0xffff);
	u8 l4_proto = IPPROTO_UDP;
	u8 l4_proto_mask = 0xff;
	int rc = 0;

	l2_parms.type = BNXT_ULP_GEN_L2_L2_HDR;

	if (is_v6) {
		/* Pack the L3 Data */
		v6_spec.proto6 = &l4_proto;
		v6_mask.proto6 = &l4_proto_mask;
		v6_spec.dip6 = NULL;
		v6_mask.dip6 = NULL;
		v6_spec.sip6 = NULL;
		v6_mask.sip6 = NULL;

		l3_parms.type = BNXT_ULP_GEN_L3_IPV6;
		l3_parms.v6_spec = &v6_spec;
		l3_parms.v6_mask = &v6_mask;
	} else {
		/* Pack the L3 Data */
		v4_spec.proto = &l4_proto;
		v4_mask.proto = &l4_proto_mask;
		v4_spec.dip = NULL;
		v4_mask.dip = NULL;
		v4_spec.sip = NULL;
		v4_mask.sip = NULL;

		l3_parms.type = BNXT_ULP_GEN_L3_IPV4;
		l3_parms.v4_spec = &v4_spec;
		l3_parms.v4_mask = &v4_mask;
	}

	/* Pack the L4 Data */
	bth_spec.op_code = &op_code;
	bth_mask.op_code = &op_code_mask;
	bth_spec.dst_qpn = NULL;
	bth_mask.dst_qpn = NULL;
	l4_parms.type = BNXT_ULP_GEN_L4_BTH;
	l4_parms.bth_spec = &bth_spec;
	l4_parms.bth_mask = &bth_mask;

	/* Pack the actions - NIC template will use RoCE VNIC always by default */
	if (dir == CFA_DIR_RX) {
		actions.enables = BNXT_ULP_GEN_ACTION_ENABLES_COUNT;
	} else {
		actions.enables = BNXT_ULP_GEN_ACTION_ENABLES_COUNT |
			BNXT_ULP_GEN_ACTION_ENABLES_REDIRECT;
	}
	actions.dst_fid = bp->pf.fw_fid;

	if (dir == CFA_DIR_RX)
		parms.dir = BNXT_ULP_GEN_RX;
	else
		parms.dir = BNXT_ULP_GEN_TX;

	parms.flow_id = cnp_flow_id;
	parms.counter_hndl = cnp_flow_cnt_hndl;
	parms.l2 = &l2_parms;
	parms.l3 = &l3_parms;
	parms.l4 = &l4_parms;
	parms.actions = &actions;
	/* must be higher priority than ROCE non-CNP */
	if (!BNXT_UDCC_DCQCN_EN(bp))
		parms.lkup_strength = FLOW_LKUP_STRENGTH_HI;
	parms.priority = ULP_NIC_FLOW_ROCE_CNP_PRI;

	rc = bnxt_ulp_gen_flow_create(bp, bp->pf.fw_fid, &parms);
	if (rc)
		return rc;

	netdev_dbg(bp->dev, "%s: ROCE(%s) CNP(%s) flow_id: %d, ctr: 0x%llx\n",
		   __func__,
		   (is_v6 ? "v6" : "v4"),
		   tfc_dir_2_str(dir),
		   *cnp_flow_id,
		   *cnp_flow_cnt_hndl);

	return rc;
}

static int roce_ack_flow_create(struct bnxt *bp,
				u32 *cnp_flow_id, u64 *cnp_flow_cnt_hndl,
				bool is_v6, enum cfa_dir dir)
{
	struct bnxt_ulp_gen_bth_hdr bth_spec = { 0 }, bth_mask = { 0 };
	struct bnxt_ulp_gen_ipv6_hdr v6_spec = { 0 }, v6_mask = { 0 };
	struct bnxt_ulp_gen_ipv4_hdr v4_spec = { 0 }, v4_mask = { 0 };
	struct bnxt_ulp_gen_l2_hdr_parms l2_parms = { 0 };
	struct bnxt_ulp_gen_l3_hdr_parms l3_parms = { 0 };
	struct bnxt_ulp_gen_l4_hdr_parms l4_parms = { 0 };
	struct bnxt_ulp_gen_action_parms actions = { 0 };
	struct bnxt_ulp_gen_flow_parms parms = { 0 };
	u16 op_code = cpu_to_be16(0x11); /* RoCE ACK */
	u16 op_code_mask = cpu_to_be16(0xffff);
	u8 l4_proto = IPPROTO_UDP;
	u8 l4_proto_mask = 0xff;
	int rc = 0;

	l2_parms.type = BNXT_ULP_GEN_L2_L2_HDR;

	if (is_v6) {
		/* Pack the L3 Data */
		v6_spec.proto6 = &l4_proto;
		v6_mask.proto6 = &l4_proto_mask;
		v6_spec.dip6 = NULL;
		v6_mask.dip6 = NULL;
		v6_spec.sip6 = NULL;
		v6_mask.sip6 = NULL;

		l3_parms.type = BNXT_ULP_GEN_L3_IPV6;
		l3_parms.v6_spec = &v6_spec;
		l3_parms.v6_mask = &v6_mask;
	} else {
		/* Pack the L3 Data */
		v4_spec.proto = &l4_proto;
		v4_mask.proto = &l4_proto_mask;
		v4_spec.dip = NULL;
		v4_mask.dip = NULL;
		v4_spec.sip = NULL;
		v4_mask.sip = NULL;

		l3_parms.type = BNXT_ULP_GEN_L3_IPV4;
		l3_parms.v4_spec = &v4_spec;
		l3_parms.v4_mask = &v4_mask;
	}

	/* Pack the L4 Data */
	bth_spec.op_code = &op_code;
	bth_mask.op_code = &op_code_mask;
	bth_spec.dst_qpn = NULL;
	bth_mask.dst_qpn = NULL;
	l4_parms.type = BNXT_ULP_GEN_L4_BTH;
	l4_parms.bth_spec = &bth_spec;
	l4_parms.bth_mask = &bth_mask;

	/* Pack the actions - NIC template will use RoCE VNIC always by default */
	if (dir == CFA_DIR_RX) {
		actions.enables = BNXT_ULP_GEN_ACTION_ENABLES_COUNT;
	} else {
		actions.enables = BNXT_ULP_GEN_ACTION_ENABLES_COUNT |
			BNXT_ULP_GEN_ACTION_ENABLES_REDIRECT;
	}
	actions.dst_fid = bp->pf.fw_fid;

	if (dir == CFA_DIR_RX)
		parms.dir = BNXT_ULP_GEN_RX;
	else
		parms.dir = BNXT_ULP_GEN_TX;

	parms.flow_id = cnp_flow_id;
	parms.counter_hndl = cnp_flow_cnt_hndl;
	parms.l2 = &l2_parms;
	parms.l3 = &l3_parms;
	parms.l4 = &l4_parms;
	parms.actions = &actions;

	if (!BNXT_UDCC_DCQCN_EN(bp))
		parms.lkup_strength = FLOW_LKUP_STRENGTH_HI;
	parms.priority = ULP_NIC_FLOW_ROCE_CNP_PRI;

	rc = bnxt_ulp_gen_flow_create(bp, bp->pf.fw_fid, &parms);
	if (rc)
		return rc;

	netdev_dbg(bp->dev, "%s: ROCE(%s) CNP ACK(%s) flow_id: %d, ctr: 0x%llx\n",
		   __func__,
		   (is_v6 ? "v6" : "v4"),
		   tfc_dir_2_str(dir),
		   *cnp_flow_id,
		   *cnp_flow_cnt_hndl);

	return rc;
}

static int roce_probe_flow_create(struct bnxt *bp,
				  u32 *cnp_flow_id, u64 *cnp_flow_cnt_hndl,
				  bool is_v6, enum cfa_dir dir, u16 pad_cnt)
{
	struct bnxt_ulp_gen_bth_hdr bth_spec = { 0 }, bth_mask = { 0 };
	struct bnxt_ulp_gen_ipv6_hdr v6_spec = { 0 }, v6_mask = { 0 };
	struct bnxt_ulp_gen_ipv4_hdr v4_spec = { 0 }, v4_mask = { 0 };
	struct bnxt_ulp_gen_l2_hdr_parms l2_parms = { 0 };
	struct bnxt_ulp_gen_l3_hdr_parms l3_parms = { 0 };
	struct bnxt_ulp_gen_l4_hdr_parms l4_parms = { 0 };
	struct bnxt_ulp_gen_action_parms actions = { 0 };
	struct bnxt_ulp_gen_flow_parms parms = { 0 };
	u32 dst_qpn = cpu_to_be32(0x1); /* RoCE QP 1 for PROBE */
	u32 dst_qpn_mask = cpu_to_be32(0xffffffff);
	u16 flags_mask = cpu_to_be16(0x3);
	u16 flags = cpu_to_be16(pad_cnt);
	u8 l4_proto = IPPROTO_UDP;
	u8 l4_proto_mask = 0xff;
	int rc = 0;

	l2_parms.type = BNXT_ULP_GEN_L2_L2_HDR;

	if (is_v6) {
		/* Pack the L3 Data */
		v6_spec.proto6 = &l4_proto;
		v6_mask.proto6 = &l4_proto_mask;
		v6_spec.dip6 = NULL;
		v6_mask.dip6 = NULL;
		v6_spec.sip6 = NULL;
		v6_mask.sip6 = NULL;

		l3_parms.type = BNXT_ULP_GEN_L3_IPV6;
		l3_parms.v6_spec = &v6_spec;
		l3_parms.v6_mask = &v6_mask;
	} else {
		/* Pack the L3 Data */
		v4_spec.proto = &l4_proto;
		v4_mask.proto = &l4_proto_mask;
		v4_spec.dip = NULL;
		v4_mask.dip = NULL;
		v4_spec.sip = NULL;
		v4_mask.sip = NULL;

		l3_parms.type = BNXT_ULP_GEN_L3_IPV4;
		l3_parms.v4_spec = &v4_spec;
		l3_parms.v4_mask = &v4_mask;
	}

	/* Pack the L4 Data */
	bth_spec.dst_qpn = &dst_qpn;
	bth_mask.dst_qpn = &dst_qpn_mask;
	bth_spec.op_code = NULL;
	bth_mask.op_code = NULL;
	bth_spec.bth_flags = &flags;
	bth_mask.bth_flags = &flags_mask;

	l4_parms.type = BNXT_ULP_GEN_L4_BTH;
	l4_parms.bth_spec = &bth_spec;
	l4_parms.bth_mask = &bth_mask;

	/* Pack the actions - NIC template will use RoCE VNIC always by default */
	if (dir == CFA_DIR_RX) {
		actions.enables = BNXT_ULP_GEN_ACTION_ENABLES_COUNT |
			BNXT_ULP_GEN_ACTION_ENABLES_DROP;
	} else {
		actions.enables = BNXT_ULP_GEN_ACTION_ENABLES_COUNT |
			BNXT_ULP_GEN_ACTION_ENABLES_REDIRECT;
	}
	actions.dst_fid = bp->pf.fw_fid;

	if (dir == CFA_DIR_RX)
		parms.dir = BNXT_ULP_GEN_RX;
	else
		parms.dir = BNXT_ULP_GEN_TX;

	parms.flow_id = cnp_flow_id;
	parms.counter_hndl = cnp_flow_cnt_hndl;
	parms.l2 = &l2_parms;
	parms.l3 = &l3_parms;
	parms.l4 = &l4_parms;
	parms.actions = &actions;
	/* must be higher priority than ROCE non-CNP */
	parms.priority = ULP_NIC_FLOW_ROCE_PROBE_PRI;

	rc = bnxt_ulp_gen_flow_create(bp, bp->pf.fw_fid, &parms);
	if (rc)
		return rc;

	netdev_dbg(bp->dev, "%s: ROCE(%s) PROBE(%s) flow_id: %d, ctr: 0x%llx\n",
		   __func__,
		   (is_v6 ? "v6" : "v4"),
		   tfc_dir_2_str(dir),
		   *cnp_flow_id,
		   *cnp_flow_cnt_hndl);
	return rc;
}

int bnxt_ulp_nic_flows_roce_add(struct bnxt *bp,
				struct ulp_nic_flows *flows,
				enum cfa_dir dir)
{
	bool is_v6;
	int rc;

	if (!flows) {
		netdev_info(bp->dev, "%s bad parameters\n", __func__);
		return -EINVAL;
	}

	memset(flows, 0, sizeof(struct ulp_nic_flows));

	rc = roce_flow_create(bp,
			      &flows->id[NIC_FLOW_TYPE_ROCE_V4],
			      &flows->cnt_hndl[NIC_FLOW_TYPE_ROCE_V4],
			      is_v6 = false, dir);
	if (rc)
		goto cleanup;
	rc = roce_flow_create(bp,
			      &flows->id[NIC_FLOW_TYPE_ROCE_V6],
			      &flows->cnt_hndl[NIC_FLOW_TYPE_ROCE_V6],
			      is_v6 = true, dir);
	if (rc)
		goto cleanup;

	rc = roce_cnp_flow_create(bp,
				  &flows->id[NIC_FLOW_TYPE_ROCE_V4_CNP],
				  &flows->cnt_hndl[NIC_FLOW_TYPE_ROCE_V4_CNP],
				  is_v6 = false, dir);
	if (rc)
		goto cleanup;

	rc = roce_cnp_flow_create(bp,
				  &flows->id[NIC_FLOW_TYPE_ROCE_V6_CNP],
				  &flows->cnt_hndl[NIC_FLOW_TYPE_ROCE_V6_CNP],
				  is_v6 = true, dir);
	if (rc)
		goto cleanup;

	if (dir == CFA_DIR_RX) {
		rc = bnxt_hwrm_udcc_qcfg(bp);
		if (rc && rc != -EOPNOTSUPP)
			goto cleanup;

		if (!rc && bp->udcc_info->pad_cnt) {
			netdev_dbg(bp->dev, "UDCC probe config pad count 0x%x\n",
				   bp->udcc_info->pad_cnt);
			rc = roce_probe_flow_create(bp,
						    &flows->id[NIC_FLOW_TYPE_ROCE_V4_PROBE],
						    &flows->cnt_hndl[NIC_FLOW_TYPE_ROCE_V4_PROBE],
						    is_v6 = false, dir,
						    (u16)bp->udcc_info->pad_cnt);
			if (rc)
				goto cleanup;

			rc = roce_probe_flow_create(bp,
						    &flows->id[NIC_FLOW_TYPE_ROCE_V6_PROBE],
						    &flows->cnt_hndl[NIC_FLOW_TYPE_ROCE_V6_PROBE],
						    is_v6 = true, dir,
						    (u16)bp->udcc_info->pad_cnt);
			if (rc)
				goto cleanup;
		} else {	/* rc == -EOPNOTSUPP || !pad_cnt */
			rc = 0;
		}
	}

	if (dir == CFA_DIR_TX) {
		rc = roce_ack_flow_create(bp,
					  &flows->id[NIC_FLOW_TYPE_ROCE_V4_ACK],
					  &flows->cnt_hndl[NIC_FLOW_TYPE_ROCE_V4_ACK],
					  is_v6 = false, dir);
		if (rc)
			goto cleanup;

		rc = roce_ack_flow_create(bp,
					  &flows->id[NIC_FLOW_TYPE_ROCE_V6_ACK],
					  &flows->cnt_hndl[NIC_FLOW_TYPE_ROCE_V6_ACK],
					  is_v6 = true, dir);
		if (rc)
			goto cleanup;

		rc = roce_lpbk_flow_create(bp,
					   &flows->id[NIC_FLOW_TYPE_ROCE_V4_LPBK],
					   &flows->cnt_hndl[NIC_FLOW_TYPE_ROCE_V4_LPBK],
					   is_v6 = false, dir);
		if (rc)
			goto cleanup;

		rc = roce_lpbk_flow_create(bp,
					   &flows->id[NIC_FLOW_TYPE_ROCE_V6_LPBK],
					   &flows->cnt_hndl[NIC_FLOW_TYPE_ROCE_V6_LPBK],
					   is_v6 = true, dir);
		if (rc)
			goto cleanup;
	}

	return rc;

cleanup:
	bnxt_ulp_nic_flows_roce_del(bp, flows, dir);

	return rc;
}

int bnxt_ulp_nic_flows_roce_del(struct bnxt *bp,
				struct ulp_nic_flows *flows,
				enum cfa_dir dir)
{
	enum ulp_nic_flow_type type;
	int rc_save = 0, rc = 0;

	for (type = 0; type < NIC_FLOW_TYPE_MAX; type++) {
		if (flows->id[type]) {
			rc = bnxt_ulp_gen_flow_destroy(bp, bp->pf.fw_fid, flows->id[type]);
			if (rc) {
				netdev_dbg(bp->dev, "%s: delete flow_id(%s): %d failed %d\n",
					   __func__, tfc_dir_2_str(dir),
					   flows->id[type], rc);
				rc_save = rc;
			}
		}
	}
	return rc_save;
}

static u8 bnxt_tf_ntuple_l4_flow_get_type(struct bnxt_ulp_gen_l4_hdr_parms *params)
{
	u8 fc_type = BNXT_FLOW_CLASS_EM;

	if (params->type == BNXT_ULP_GEN_L4_TCP) {
		if (params->tcp_spec->sport[0] == 0x0000 || params->tcp_spec->dport[0] == 0x0000 ||
		    (params->tcp_spec->sport && params->tcp_mask->sport[0] != 0xffff) ||
		    (params->tcp_spec->dport && params->tcp_mask->dport[0] != 0xffff))
			fc_type = BNXT_FLOW_CLASS_WC;
	} else if (params->type == BNXT_ULP_GEN_L4_UDP) {
		if (params->udp_spec->sport[0] == 0x0000 || params->udp_spec->dport[0] == 0x0000 ||
		    (params->udp_spec->sport && params->udp_mask->sport[0] != 0xffff) ||
		    (params->udp_spec->dport && params->udp_mask->dport[0] != 0xffff))
			fc_type = BNXT_FLOW_CLASS_WC;
	}

	return fc_type;
}

static u8 bnxt_tf_ntuple_l3_flow_get_type(struct bnxt_ulp_gen_l3_hdr_parms *params)
{
	u8 fc_type = BNXT_FLOW_CLASS_EM;

	if (params->type == BNXT_ULP_GEN_L3_IPV4) {
		if (!memcmp(params->v4_spec->sip, ip_all_zero, L3_ADDR_MAX_SIZE) ||
		    !memcmp(params->v4_spec->dip, ip_all_zero, L3_ADDR_MAX_SIZE) ||
		    (params->v4_spec->sip[0] && params->v4_mask->sip[0] != 0xffffffff) ||
		    (params->v4_spec->dip[0] && params->v4_mask->sip[0] != 0xffffffff) ||
		    !params->v4_spec->proto)
			fc_type = BNXT_FLOW_CLASS_WC;
	} else if (params->type == BNXT_ULP_GEN_L3_IPV6) {
		if (!memcmp(params->v6_spec->sip6, ip_all_zero, L3_ADDR_MAX_SIZE) ||
		    !memcmp(params->v6_spec->dip6, ip_all_zero, L3_ADDR_MAX_SIZE) ||
		    (memcmp(params->v6_spec->sip6, ip_all_zero, L3_ADDR_MAX_SIZE) &&
		     memcmp(params->v6_mask->sip6, ipv6_all_mask, L3_ADDR_MAX_SIZE)) ||
		    (memcmp(params->v6_spec->dip6, ip_all_zero, L3_ADDR_MAX_SIZE) &&
		     memcmp(params->v6_mask->dip6, ipv6_all_mask, L3_ADDR_MAX_SIZE)) ||
		    !params->v6_spec->proto6)
			fc_type = BNXT_FLOW_CLASS_WC;
	}
	return fc_type;
}

int bnxt_tf_ntuple_flow_create(struct bnxt *bp,
			       struct bnxt_ntuple_filter *fltr,
			       u32 *flow_id, u64 *flow_cnt_hndl,
			       enum cfa_dir dir)
{
	bool cap_ring_dst = bp->fw_cap & BNXT_FW_CAP_CFA_RFS_RING_TBL_IDX_V2;
	struct bnxt_ulp_gen_udp_hdr udp_spec = { }, udp_mask = { };
	struct bnxt_ulp_gen_tcp_hdr tcp_spec = { }, tcp_mask = { };
	struct bnxt_ulp_gen_bth_hdr bth_spec = { }, bth_mask = { };
	struct bnxt_ulp_gen_ipv6_hdr v6_spec = { }, v6_mask = { };
	struct bnxt_ulp_gen_ipv4_hdr v4_spec = { }, v4_mask = { };
	struct bnxt_ulp_gen_l2_hdr_parms l2_parms = { };
	struct bnxt_ulp_gen_l3_hdr_parms l3_parms = { };
	struct bnxt_ulp_gen_l4_hdr_parms l4_parms = { };
	struct bnxt_ulp_gen_action_parms actions = { };
	struct bnxt_ulp_gen_flow_parms parms = { };
	struct bnxt_ulp_gen_eth_hdr eth_spec = { };
	struct bnxt_ulp_gen_eth_hdr eth_mask = { };
	struct bnxt_flow_masks *masks = &fltr->fmasks;

	struct flow_keys *keys = &fltr->fkeys;
	u16 etype_mask = cpu_to_be16(0xffff);
	u8 l4_proto = keys->basic.ip_proto;
	u8 dst_spec[ETH_ALEN] = { };
	u8 dst_mask[ETH_ALEN] = { };
	struct bnxt_vnic_info *vnic;
	u8 l4_proto_mask = 0xff;
	u16 etype_spec;
	int rc = 0;
	bool is_v6;
	u8 fc_type;

	ether_addr_copy(dst_spec, bp->dev->dev_addr);

	is_v6 = keys->basic.n_proto == htons(ETH_P_IPV6);
	etype_spec = is_v6 ? cpu_to_be16(ETH_P_IPV6) :
		cpu_to_be16(ETH_P_IP);
	eth_broadcast_addr(dst_mask);
	eth_spec.dst = &dst_spec[0];
	eth_mask.dst = &dst_mask[0];
	eth_spec.type = &etype_spec;
	eth_mask.type = &etype_mask;
	l2_parms.eth_spec = &eth_spec;
	l2_parms.eth_mask = &eth_mask;
	l2_parms.type = BNXT_ULP_GEN_L2_L2_HDR;

	if (is_v6) {
		/* Pack the L3 Data */
		v6_spec.proto6 = &l4_proto;
		v6_mask.proto6 = &l4_proto_mask;
		v6_spec.dip6 = (u8 *)&keys->addrs.v6addrs.dst;
		v6_mask.dip6 = (u8 *)&masks->addrs.v6addrs.dst;
		v6_spec.sip6 = (u8 *)&keys->addrs.v6addrs.src;
		v6_mask.sip6 = (u8 *)&masks->addrs.v6addrs.src;

		l3_parms.type = BNXT_ULP_GEN_L3_IPV6;
		l3_parms.v6_spec = &v6_spec;
		l3_parms.v6_mask = &v6_mask;
	} else {
		/* Pack the L3 Data */
		v4_spec.proto = &l4_proto;
		v4_mask.proto = &l4_proto_mask;
		v4_spec.dip = &keys->addrs.v4addrs.dst;
		v4_mask.dip = &masks->addrs.v4addrs.dst;
		v4_spec.sip = &keys->addrs.v4addrs.src;
		v4_mask.sip = &masks->addrs.v4addrs.src;

		l3_parms.type = BNXT_ULP_GEN_L3_IPV4;
		l3_parms.v4_spec = &v4_spec;
		l3_parms.v4_mask = &v4_mask;
	}
	fc_type = bnxt_tf_ntuple_l3_flow_get_type(&l3_parms);

	/* Pack the L4 Data */
	bth_spec.op_code = NULL;
	bth_mask.op_code = NULL;
	bth_spec.dst_qpn = NULL;
	bth_mask.dst_qpn = NULL;
	if (l4_proto == IPPROTO_UDP) {
		l4_parms.type = BNXT_ULP_GEN_L4_UDP;
		tcp_spec.sport = NULL;
		tcp_spec.dport = NULL;
		tcp_mask.sport = NULL;
		tcp_mask.dport = NULL;
		udp_spec.sport = &keys->ports.src;
		udp_spec.dport = &keys->ports.dst;
		udp_mask.sport = &masks->ports.src;
		udp_mask.dport = &masks->ports.dst;
	} else if (l4_proto == IPPROTO_TCP) {
		l4_parms.type = BNXT_ULP_GEN_L4_TCP;
		udp_spec.sport = NULL;
		udp_spec.dport = NULL;
		udp_mask.sport = NULL;
		udp_mask.dport = NULL;
		tcp_spec.sport = &keys->ports.src;
		tcp_spec.dport = &keys->ports.dst;
		tcp_mask.sport = &masks->ports.src;
		tcp_mask.dport = &masks->ports.dst;
	}
	l4_parms.bth_spec = &bth_spec;
	l4_parms.bth_mask = &bth_mask;
	l4_parms.tcp_spec = &tcp_spec;
	l4_parms.tcp_mask = &tcp_mask;
	l4_parms.udp_spec = &udp_spec;
	l4_parms.udp_mask = &udp_mask;
	if (fc_type == BNXT_FLOW_CLASS_EM)
		fc_type = bnxt_tf_ntuple_l4_flow_get_type(&l4_parms);

	/* Pack the actions - NIC template will use RoCE VNIC always by default */
	if (fltr->base.flags & BNXT_ACT_DROP) {
		actions.enables = BNXT_ULP_GEN_ACTION_ENABLES_DROP;
		actions.drop = true;
	} else if (cap_ring_dst) {
		if (fltr->base.flags & BNXT_ACT_RSS_CTX) {
			bnxt_get_rssctx_vnic_id(bp, &actions.vnic, fltr);
			actions.enables = BNXT_ULP_GEN_ACTION_ENABLES_VNIC;
		} else {
			actions.enables = BNXT_ULP_GEN_ACTION_ENABLES_RING_TBL_IDX;
			actions.ring_tbl_index = fltr->base.rxq;
			actions.enables |= BNXT_ULP_GEN_ACTION_ENABLES_REDIRECT;
			actions.dst_fid = bp->pf.fw_fid;
			actions.enables |= BNXT_ULP_GEN_ACTION_ENABLES_COUNT;
		}
	} else {
		vnic = &bp->vnic_info[fltr->base.rxq + 1];
		actions.vnic = cpu_to_be16(vnic->fw_vnic_id);
		actions.enables = BNXT_ULP_GEN_ACTION_ENABLES_VNIC;
	}

	if (dir == CFA_DIR_RX)
		parms.dir = BNXT_ULP_GEN_RX;
	else
		parms.dir = BNXT_ULP_GEN_TX;
	parms.flow_id = flow_id;
	if (flow_cnt_hndl)
		parms.counter_hndl = flow_cnt_hndl;
	parms.l2 = &l2_parms;
	parms.l3 = &l3_parms;
	parms.l4 = &l4_parms;
	parms.actions = &actions;
	if (fc_type == BNXT_FLOW_CLASS_EM) {
		parms.priority = ULP_NIC_FLOW_NTUPLE_PRI;
		parms.lkup_strength = FLOW_LKUP_STRENGTH_HI;
		parms.enable_em = true;
	}

	rc = bnxt_ulp_gen_flow_create(bp, bp->pf.fw_fid, &parms);
	if (rc)
		return rc;

	netdev_dbg(bp->dev, "%s: NIC NTUPLE FLOW (%s) (%s) add flow_id: %d, ctr: 0x%llx\n",
		   __func__,
		   (is_v6 ? "v6" : "v4"),
		   tfc_dir_2_str(dir),
		   *flow_id,
		   flow_cnt_hndl ? *flow_cnt_hndl : 0);
	return rc;
}

int bnxt_tf_ntuple_flow_del(struct bnxt *bp, struct bnxt_ntuple_filter *fltr)
{
	struct bnxt_vnic_info *vnic0 = &bp->vnic_info[0];
	int rc_save = 0;
	int rc = 0;
	int i;

	for (i = 0; i < vnic0->uc_filter_count; i++) {
		rc = bnxt_ulp_gen_flow_destroy(bp, bp->pf.fw_fid,
					       fltr->base.ntp_filter_id[i]);
		if (rc) {
			netdev_dbg(bp->dev, "%s: delete flow_id: %lld failed %d\n",
				   __func__, fltr->base.ntp_filter_id[i], rc);
			rc_save = rc;
		}
	}
	return rc_save;
}

int bnxt_tf_tls_flow_create(struct bnxt *bp, struct sock *sk,
			    u64 *flow_id, u64 *flow_cnt_hndl,
			    u32 kid, u8 type)
{
	struct bnxt_ulp_gen_udp_hdr udp_spec = { }, udp_mask = { };
	struct bnxt_ulp_gen_tcp_hdr tcp_spec = { }, tcp_mask = { };
	struct bnxt_ulp_gen_ipv6_hdr v6_spec = { }, v6_mask = { };
	struct bnxt_ulp_gen_ipv4_hdr v4_spec = { }, v4_mask = { };
	struct bnxt_ulp_gen_bth_hdr bth_spec = { }, bth_mask = { };
	struct bnxt_ulp_gen_l2_hdr_parms l2_parms = { };
	struct bnxt_ulp_gen_l3_hdr_parms l3_parms = { };
	struct bnxt_ulp_gen_l4_hdr_parms l4_parms = { };
	struct bnxt_ulp_gen_action_parms actions = { };
	struct bnxt_ulp_gen_flow_parms parms = { };
	struct bnxt_ulp_gen_eth_hdr eth_spec = { };
	struct bnxt_ulp_gen_eth_hdr eth_mask = { };
	uint32_t mask = 0xffffffff;
	uint16_t port_mask = 0xffff;

	struct inet_sock *inet = inet_sk(sk);
	u16 etype_mask = cpu_to_be16(0xffff);
	u8 l4_proto = sk->sk_protocol;
	u8 dst_spec[ETH_ALEN] = { };
	u8 dst_mask[ETH_ALEN] = { };
	u8 l4_proto_mask = 0xff;
	u16 etype_spec;
	bool is_v6 = false;
	int rc = 0;

	is_v6 = sk->sk_family == htons(ETH_P_IPV6);
	etype_spec = is_v6 ? cpu_to_be16(ETH_P_IPV6) :
		cpu_to_be16(ETH_P_IP);
	ether_addr_copy(dst_spec, bp->dev->dev_addr);

	eth_broadcast_addr(dst_mask);
	eth_spec.dst = &dst_spec[0];
	eth_mask.dst = &dst_mask[0];
	eth_spec.type = &etype_spec;
	eth_mask.type = &etype_mask;
	l2_parms.eth_spec = &eth_spec;
	l2_parms.eth_mask = &eth_mask;
	l2_parms.type = BNXT_ULP_GEN_L2_L2_HDR;

	if (is_v6) {
		struct ipv6_pinfo *inet6 = inet6_sk(sk);

		/* Pack the L3 Data */
		v6_spec.proto6 = &l4_proto;
		v6_mask.proto6 = &l4_proto_mask;
		v6_spec.dip6 = (u8 *)&inet6->saddr;
		v6_mask.dip6 = (u8 *)ipv6_all_mask;
		v6_spec.sip6 = (u8 *)&sk->sk_v6_daddr;
		v6_mask.sip6 = (u8 *)ipv6_all_mask;

		l3_parms.type = BNXT_ULP_GEN_L3_IPV6;
		l3_parms.v6_spec = &v6_spec;
		l3_parms.v6_mask = &v6_mask;
	} else {
		/* Pack the L3 Data */
		v4_spec.proto = &l4_proto;
		v4_mask.proto = &l4_proto_mask;
		v4_spec.dip = &inet->inet_saddr;
		v4_mask.dip = &mask;
		v4_spec.sip = &inet->inet_daddr;
		v4_mask.sip = &mask;

		l3_parms.type = BNXT_ULP_GEN_L3_IPV4;
		l3_parms.v4_spec = &v4_spec;
		l3_parms.v4_mask = &v4_mask;
	}

	/* Pack the L4 Data */
	bth_spec.op_code = NULL;
	bth_mask.op_code = NULL;
	bth_spec.dst_qpn = NULL;
	bth_mask.dst_qpn = NULL;
	if (l4_proto == IPPROTO_UDP) {
		l4_parms.type = BNXT_ULP_GEN_L4_UDP;
		tcp_spec.sport = NULL;
		tcp_spec.dport = NULL;
		tcp_mask.sport = NULL;
		tcp_mask.dport = NULL;
		udp_spec.sport = &inet->inet_dport;
		udp_spec.dport = &inet->inet_sport;
		udp_mask.sport = &port_mask;
		udp_mask.dport = &port_mask;
	} else if (l4_proto == IPPROTO_TCP) {
		l4_parms.type = BNXT_ULP_GEN_L4_TCP;
		udp_spec.sport = NULL;
		udp_spec.dport = NULL;
		udp_mask.sport = NULL;
		udp_mask.dport = NULL;
		tcp_spec.sport = &inet->inet_dport;
		tcp_spec.dport = &inet->inet_sport;
		tcp_mask.sport = &port_mask;
		tcp_mask.dport = &port_mask;
	}
	l4_parms.bth_spec = &bth_spec;
	l4_parms.bth_mask = &bth_mask;
	l4_parms.tcp_spec = &tcp_spec;
	l4_parms.tcp_mask = &tcp_mask;
	l4_parms.udp_spec = &udp_spec;
	l4_parms.udp_mask = &udp_mask;

	actions.vnic = cpu_to_le16(bp->vnic_info[BNXT_VNIC_DEFAULT].fw_vnic_id);
	actions.enables = BNXT_ULP_GEN_ACTION_ENABLES_VNIC;
	actions.kid = cpu_to_be32(kid);
	actions.enables |= BNXT_ULP_GEN_ACTION_ENABLES_KID;
	/* TODO: quic_dst_connection_id */
	if (type == BNXT_CRYPTO_TYPE_QUIC)
		netdev_err(bp->dev, "%s: Pass the quic_dst_connect_id\n", __func__);
	parms.dir = BNXT_ULP_GEN_RX;

	parms.flow_id = (u32 *)flow_id;
	if (flow_cnt_hndl)
		parms.counter_hndl = flow_cnt_hndl;
	parms.l2 = &l2_parms;
	parms.l3 = &l3_parms;
	parms.l4 = &l4_parms;
	parms.actions = &actions;
	parms.priority = ULP_NIC_FLOW_KTLS_PRI;
	parms.lkup_strength = FLOW_LKUP_STRENGTH_HI;
	parms.enable_em = true;

	rc = bnxt_ulp_gen_flow_create(bp, bp->pf.fw_fid, &parms);
	if (rc)
		return rc;

	netdev_dbg(bp->dev, "%s: NIC KTLS FLOW (%s) (%s) add flow_id: %lld, ctr: 0x%llx\n",
		   __func__,
		   (is_v6 ? "v6" : "v4"),
		   type == BNXT_CRYPTO_TYPE_KTLS ? "KTLS" : "QUIC",
		   *flow_id,
		   flow_cnt_hndl ? *flow_cnt_hndl : 0);
	return rc;
}

int bnxt_tf_tls_flow_del(struct bnxt *bp, __le64 filter_id)
{
	int rc = 0;

	rc = bnxt_ulp_gen_flow_destroy(bp, bp->pf.fw_fid, filter_id);
	if (rc) {
		netdev_dbg(bp->dev, "%s: delete flow_id: %lld failed %d\n",
			   __func__, filter_id, rc);
	}
	return rc;
}

static int
bnxt_tf_l2_flow_create(struct bnxt *bp, u8 *flt_mac_addr, u64 *oflow_id)
{
	u16 port_id = bp->pf.fw_fid;
	struct ulp_tlv_param param_list[] = {
		{
			.type = BNXT_ULP_DF_PARAM_TYPE_DEV_PORT_ID,
			.length = 2,
			.value = {(port_id >> 8) & 0xff, port_id & 0xff}
		},
		{
			.type = BNXT_ULP_DF_PARAM_TYPE_LAST,
			.length = 0,
			.value = {0}
		}
	};
	u32 flow_id = 0;
	u64 *comp_fld;
	u8 *buffer;
	int rc = 0;

	comp_fld = vzalloc(sizeof(u64) * BNXT_ULP_CF_IDX_LAST);
	if (!comp_fld)
		return -ENOMEM;

	/* allocate the flow id */
	if (ulp_flow_db_reg_fid_alloc_with_lock(bp, &flow_id)) {
		vfree(comp_fld);
		return -ENOMEM;
	}

	buffer = (u8 *)&comp_fld[BNXT_ULP_CF_IDX_FILTER_MAC];
	buffer += sizeof(u64) - ETH_ALEN;
	memcpy(buffer, flt_mac_addr, ETH_ALEN);

	if (ulp_flow_template_process(bp, param_list, comp_fld,
				      BNXT_ULP_TEMPLATE_L2_FILTER,
				      port_id, flow_id)) {
		rc = -EIO;
		ulp_flow_db_reg_fid_free_with_lock(bp, flow_id);
	} else {
		*oflow_id = flow_id;
	}

	vfree(comp_fld);
	return rc;
}

int bnxt_tf_l2_filter_create(struct bnxt *bp,
			     struct bnxt_l2_filter *fltr)
{
	int rc = 0;

	/* Not PF, or not P7plus or TF not enabled, then exit */
	if (!BNXT_PF(bp) || !BNXT_CHIP_P7_PLUS(bp) || !bp->ulp_ctx)
		return rc; /* no failure */

	netdev_dbg(bp->dev, "TF ADD: mac_addr %pM l2_filter_id 0x%llx\n",
		   fltr->l2_key.dst_mac_addr, fltr->base.l2_filter_id);

	rc = bnxt_tf_l2_flow_create(bp, fltr->l2_key.dst_mac_addr,
				    &fltr->base.l2_filter_id);
	if (rc)
		netdev_dbg(bp->dev, "%s: add l2 filter failed %d\n",
			   __func__, rc);
	return rc;
}

int bnxt_tf_l2_filter_delete(struct bnxt *bp,
			     struct bnxt_l2_filter *fltr)
{
	int rc = 0;

	/* Not PF, or not P7plus or TF not enabled, then exit */
	if (!BNXT_PF(bp) || !BNXT_CHIP_P7_PLUS(bp) || !bp->ulp_ctx)
		return rc; /* no failure */

	if (fltr->base.l2_filter_id) {
		netdev_dbg(bp->dev, "TF DEL: mac_addr %pM l2_filter_id 0x%llx\n",
			   fltr->l2_key.dst_mac_addr, fltr->base.l2_filter_id);

		rc = bnxt_ulp_gen_flow_destroy(bp, bp->pf.fw_fid,
					       fltr->base.l2_filter_id);
		if (rc)
			netdev_dbg(bp->dev, "%s: delete flow_id: %lld failed %d\n",
				   __func__, fltr->base.l2_filter_id, rc);
	}
	return rc;
}
#endif /* if defined(CONFIG_BNXT_FLOWER_OFFLOAD) */
