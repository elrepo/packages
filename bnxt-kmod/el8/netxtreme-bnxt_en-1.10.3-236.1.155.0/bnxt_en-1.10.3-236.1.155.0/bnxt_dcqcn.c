// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2025 Broadcom
 * All rights reserved.
 */
#include "bnxt_compat.h"
#include "bnxt_tf_ulp.h"
#include "ulp_generic_flow_offload.h"
#include "bnxt_dcqcn.h"

#if defined(CONFIG_BNXT_FLOWER_OFFLOAD)

/* This function converts the provided tfc action handle to the dcqcn
 * action handle required by the firmware.  The action handle consists
 * of an 8 byte offset in the lower 26 bits and the table scope id in
 * the upper 6 bits.
 */
static int bnxt_tfc_counter_update(struct bnxt *bp, u64 *counter_hndl)
{
	u64 val = 0;
	u8 tsid = 0;
	int rc;

	rc = bnxt_ulp_cntxt_tsid_get(bp->ulp_ctx, &tsid);
	if (rc) {
		netdev_dbg(bp->dev, "Counter update err(%d), tsid get failed\n",
			   rc);
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

static int bnxt_roce_monitor_flow_create_p7(struct bnxt *bp,
					    struct bnxt_dcqcn_session_entry *entry,
					    enum cfa_dir dir)
{
	struct bnxt_ulp_gen_ipv6_hdr v6_spec = { 0 }, v6_mask = { 0 };
	struct bnxt_ulp_gen_ipv4_hdr v4_spec = { 0 }, v4_mask = { 0 };
	struct bnxt_ulp_gen_ifa_hdr ifa_spec = { 0 }, ifa_mask = { 0 };
	struct bnxt_ulp_gen_l2_hdr_parms l2_parms = { 0 };
	struct bnxt_ulp_gen_l3_hdr_parms l3_parms = { 0 };
	struct bnxt_ulp_gen_tun_hdr_parms tun_parms = { 0 };
	struct bnxt_ulp_gen_action_parms actions = { 0 };
	struct bnxt_ulp_gen_flow_parms parms = { 0 };

	u32 src_qpn = cpu_to_be32(entry->src_qp_num);
	u32 dst_qpn = cpu_to_be32(entry->dest_qp_num);
	u8 l4_proto = entry->l4_proto;
	u8 l4_proto_mask = 0xff;
	/* gns is 4bit, but we push this into tid which 32bits */
	u32 ifa_gns = cpu_to_be32(entry->ifa_gns);
	u32 ifa_gns_mask = cpu_to_be32(0xf);
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
			v4_mask.sip = NULL; /* (u32 *)bnxt_ulp_gen_l3_ipv4_addr_em_mask; */
		} else {
			v4_spec.dip = (u32 *)&entry->dst_ip.s6_addr32[3];
			v4_mask.dip = NULL; /* (u32 *)bnxt_ulp_gen_l3_ipv4_addr_em_mask; */
			v4_spec.sip = NULL;
			v4_mask.sip = NULL;
		}
		l3_parms.type = BNXT_ULP_GEN_L3_IPV4;
		l3_parms.v4_spec = &v4_spec;
		l3_parms.v4_mask = &v4_mask;
		netdev_dbg(bp->dev, "DCQCN Add(%s) flow for session_id: %d IP %pI4 sQPn %x dQPn %x monitor vnic_id %x\n",
			   dir == CFA_DIR_RX ? "rx" : "tx",
			   entry->session_id,
			   (u32 *)&entry->dst_ip.s6_addr32[3],
			   src_qpn, dst_qpn, entry->vnic_id);
	} else {
		/* Pack the L3 Data */
		v6_spec.proto6 = &l4_proto;
		v6_mask.proto6 = &l4_proto_mask;
		if (dir == CFA_DIR_RX) {
			v6_spec.dip6 = NULL;
			v6_mask.dip6 = NULL;
			v6_spec.sip6 = entry->dst_ip.s6_addr;
			v6_mask.sip6 = NULL; /* bnxt_ulp_gen_l3_ipv6_addr_em_mask; */
		} else {
			v6_spec.dip6 = entry->dst_ip.s6_addr;
			v6_mask.dip6 = NULL; /* bnxt_ulp_gen_l3_ipv6_addr_em_mask; */
			v6_spec.sip6 = NULL;
			v6_mask.sip6 = NULL;
		}
		l3_parms.type = BNXT_ULP_GEN_L3_IPV6;
		l3_parms.v6_spec = &v6_spec;
		l3_parms.v6_mask = &v6_mask;
		netdev_dbg(bp->dev, "DCQCN Add(%s) flow for session_id: %d IP %pI6 sQPn %x dQPn %x monitor vnic_id %x\n",
			   dir == CFA_DIR_RX ? "rx" : "tx",
			   entry->session_id,
			   entry->dst_ip.s6_addr,
			   src_qpn, dst_qpn, entry->vnic_id);
	}

	ifa_spec.gns = &ifa_gns;
	ifa_mask.gns = &ifa_gns_mask;
	tun_parms.type = BNXT_ULP_GEN_TUN_IFA;
	tun_parms.ifa_spec = &ifa_spec;
	tun_parms.ifa_mask = &ifa_mask;

	if (dir == CFA_DIR_RX) {
		/* Pack the actions VNIC, should be RoCE MONITOR VNIC */
		actions.vnic = entry->vnic_id;
		actions.enables = BNXT_ULP_GEN_ACTION_ENABLES_COUNT |
			BNXT_ULP_GEN_ACTION_ENABLES_VNIC;
	} else {
		actions.enables = BNXT_ULP_GEN_ACTION_ENABLES_REDIRECT |
			BNXT_ULP_GEN_ACTION_ENABLES_COUNT;

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
	actions.dst_fid = bp->pf.fw_fid;

	if (dir == CFA_DIR_RX) {
		parms.dir = BNXT_ULP_GEN_RX;
		if (entry->v4_dst) {
			parms.flow_id = &entry->v4_rx_flow_id;
			parms.counter_hndl = &entry->v4_rx_counter_hndl;
		} else {
			parms.flow_id = &entry->v6_rx_flow_id;
			parms.counter_hndl = &entry->v6_rx_counter_hndl;
		}
	} else {
		parms.dir = BNXT_ULP_GEN_TX;
		if (entry->v4_dst) {
			parms.flow_id = &entry->v4_tx_flow_id;
			parms.counter_hndl = &entry->v4_tx_counter_hndl;
		} else {
			parms.flow_id = &entry->v6_tx_flow_id;
			parms.counter_hndl = &entry->v6_tx_counter_hndl;
		}
	}

	parms.l2 = &l2_parms;
	parms.l3 = &l3_parms;
	parms.l4 = NULL; /* No L4 data is parsed */
	parms.tun = &tun_parms; /* IFA tunnel params */
	parms.actions = &actions;
	parms.priority = 2;

	rc = bnxt_ulp_gen_flow_create(bp, bp->pf.fw_fid, &parms);
	if (rc) {
		netdev_dbg(bp->dev, "DCQCN(%s) monitor flow for session_id=%d creation error(%d)\n",
			   dir == CFA_DIR_RX ? "rx" : "tx",
			   entry->session_id,
			   rc);
		return rc;
	}
	rc = bnxt_tfc_counter_update(bp, parms.counter_hndl);

	netdev_dbg(bp->dev, "DCQCN Add(%s) flow for session_id: %d flow_id: %d cntr: 0x%llx\n",
		   dir == CFA_DIR_RX ? "rx" : "tx",
		   entry->session_id,
		   *parms.flow_id,
		   *parms.counter_hndl);
	return rc;
}

static int bnxt_roce_flow_create_p7(struct bnxt *bp,
				    struct bnxt_dcqcn_session_entry *entry,
				    enum cfa_dir dir)
{
	struct bnxt_ulp_gen_bth_hdr bth_spec = { 0 }, bth_mask = { 0 };
	struct bnxt_ulp_gen_ipv6_hdr v6_spec = { 0 }, v6_mask = { 0 };
	struct bnxt_ulp_gen_ipv4_hdr v4_spec = { 0 }, v4_mask = { 0 };
	/*bool per_qp_session = BNXT_UDCC_SESSION_PER_QP(bp); */
	/* For DCQCN only per qp session is used */
	bool per_qp_session = true;
	struct bnxt_ulp_gen_l2_hdr_parms l2_parms = { 0 };
	struct bnxt_ulp_gen_l3_hdr_parms l3_parms = { 0 };
	struct bnxt_ulp_gen_l4_hdr_parms l4_parms = { 0 };
	struct bnxt_ulp_gen_action_parms actions = { 0 };
	struct bnxt_ulp_gen_flow_parms parms = { 0 };

	/* These would normally be preset and passed to the upper layer */
	/* u32 dst_qpn = cpu_to_be32(entry->dest_qp_num); */
	u16 op_code = cpu_to_be16(BNXT_ROCE_CNP_OPCODE);
	u32 src_qpn = cpu_to_be32(entry->src_qp_num);
	u32 dst_qpn = cpu_to_be32(entry->dest_qp_num);
	u16 op_code_mask = cpu_to_be16(0xffff);
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
		netdev_dbg(bp->dev, "DCQCN Add(%s) flow for session_id: %d IP %pI4 sQPn %x dQPn %x\n",
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
		netdev_dbg(bp->dev, "DCQCN Add(%s) flow for session_id: %d IP %pI6 sQPn %x dQPn %x\n",
			   dir == CFA_DIR_RX ? "rx" : "tx",
			   entry->session_id,
			   entry->dst_ip.s6_addr,
			   src_qpn, dst_qpn);
	}

	/* Pack the roce opcode */
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
	} else {
		actions.enables = BNXT_ULP_GEN_ACTION_ENABLES_REDIRECT |
			BNXT_ULP_GEN_ACTION_ENABLES_COUNT;

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
	actions.dst_fid = bp->pf.fw_fid;

	if (dir == CFA_DIR_RX) {
		parms.dir = BNXT_ULP_GEN_RX;
		if (entry->v4_dst) {
			parms.flow_id = &entry->v4_rx_flow_id;
			parms.counter_hndl = &entry->v4_rx_counter_hndl;
		} else {
			parms.flow_id = &entry->v6_rx_flow_id;
			parms.counter_hndl = &entry->v6_rx_counter_hndl;
		}
	} else {
		parms.dir = BNXT_ULP_GEN_TX;
		if (entry->v4_dst) {
			parms.flow_id = &entry->v4_tx_flow_id;
			parms.counter_hndl = &entry->v4_tx_counter_hndl;
		} else {
			parms.flow_id = &entry->v6_tx_flow_id;
			parms.counter_hndl = &entry->v6_tx_counter_hndl;
		}
	}

	parms.l2 = &l2_parms;
	parms.l3 = &l3_parms;
	parms.l4 = &l4_parms;
	parms.actions = &actions;
	parms.enable_em = true;

	rc = bnxt_ulp_gen_flow_create(bp, bp->pf.fw_fid, &parms);
	if (rc) {
		netdev_dbg(bp->dev, "DCQCN(%s) RoCE flow for session_id=%d creation error(%d)\n",
			   dir == CFA_DIR_RX ? "rx" : "tx",
			   entry->session_id,
			   rc);
		return rc;
	}
	rc = bnxt_tfc_counter_update(bp, parms.counter_hndl);

	netdev_dbg(bp->dev, "DCQCN Add(%s) flow for session_id: %d flow_id: %d cntr: 0x%llx\n",
		   dir == CFA_DIR_RX ? "rx" : "tx",
		   entry->session_id,
		   *parms.flow_id,
		   *parms.counter_hndl);
	return rc;
}

/* The dip gets encoded as the RoCEv2 GID. The third integer
 * should be FFFF0000 if the encoded address is IPv4.
 * Example: GID:		::ffff:171.16.10.1
 */
#define	BNXT_DCQCN_DIP_V4_MASK	0xFFFF0000
static bool bnxt_is_dcqcn_dip_ipv4(struct bnxt *bp, struct in6_addr *dip)
{
	netdev_dbg(bp->dev, "%s: s6_addr32[0]: 0x%x s6_addr32[1]: 0x%x\n",
		   __func__, dip->s6_addr32[0], dip->s6_addr32[1]);
	netdev_dbg(bp->dev, "%s: s6_addr32[2]: 0x%x s6_addr32[3]: 0x%x\n",
		   __func__, dip->s6_addr32[2], dip->s6_addr32[3]);

	return((dip->s6_addr32[2] & BNXT_DCQCN_DIP_V4_MASK) ==
	       BNXT_DCQCN_DIP_V4_MASK);
}

static int bnxt_roce_flows_create_p7(struct bnxt *bp,
				     struct bnxt_dcqcn_session_entry *entry)
{
	int rc;

	entry->v4_dst = bnxt_is_dcqcn_dip_ipv4(bp, &entry->dst_ip);
	rc = bnxt_roce_flow_create_p7(bp, entry, CFA_DIR_RX);
	if (rc)
		return rc;

	return bnxt_roce_flow_create_p7(bp, entry, CFA_DIR_TX);
}

/* Add two Rx flows v4 and v6 implicitly, Tx is default out the port */
static int bnxt_roce_monitor_flows_create_p7(struct bnxt *bp,
					     struct bnxt_dcqcn_session_entry *entry)
{
	int rc;

	/* Rx IPv4 */
	entry->v4_dst = true;
	rc = bnxt_roce_monitor_flow_create_p7(bp, entry, CFA_DIR_RX);
	if (rc) {
		netdev_dbg(bp->dev, "DCQCN Rx IPv4 monitor flow create failed rc=%d\n", rc);
		return rc;
	}

	/* Rx IPv6 */
	entry->v4_dst = false;
	rc = bnxt_roce_monitor_flow_create_p7(bp, entry, CFA_DIR_RX);
	if (rc)
		netdev_dbg(bp->dev, "DCQCN Rx IPv6 monitor flow create failed rc=%d\n", rc);

	return rc;
}

/* Add the TF flows for Rx/Tx */
int bnxt_tf_ulp_dcqcn_flow_create(struct bnxt *bp,
				  struct bnxt_dcqcn_session_entry *entry)
{
	int rc;

	if (!BNXT_CHIP_P7(bp)) {
		netdev_dbg(bp->dev, "DCQCN flow create not supported on this device\n");
		return -EOPNOTSUPP;
	}

	/* Truflow is not initialized; we should not create session */
	if (!(bp->tf_flags & BNXT_TF_FLAG_INITIALIZED)) {
		netdev_warn(bp->dev, "DCQCN: Truflow is not initialized\n");
		return -EPERM;
	}

	switch (entry->type) {
	case DCQCN_FLOW_TYPE_ROCE:
		rc = bnxt_roce_flows_create_p7(bp, entry);
		if (rc)
			goto cleanup;
		break;
	case DCQCN_FLOW_TYPE_ROCE_MONITOR:
		rc = bnxt_roce_monitor_flows_create_p7(bp, entry);
		if (rc)
			goto cleanup;
		break;
	default:
		netdev_dbg(bp->dev, "DCQCN unsupported flow type(%d)\n",
			   entry->type);
		rc = -EINVAL;
		break;
	}

	return rc;

cleanup:
	bnxt_tf_ulp_dcqcn_flow_delete(bp, entry);
	return rc;
}

/* Delete the TF flows for Rx/Tx */
int bnxt_tf_ulp_dcqcn_flow_delete(struct bnxt *bp,
				  struct bnxt_dcqcn_session_entry *entry)
{
	int rc = 0;

	if (entry->v4_rx_flow_id) {
		rc = bnxt_ulp_gen_flow_destroy(bp, bp->pf.fw_fid,
					       entry->v4_rx_flow_id);
		netdev_dbg(bp->dev,
			   "DCQCN Delete Rx v4 flow_id: %d session: %d rc: %d\n",
			   entry->v4_rx_flow_id, entry->session_id, rc);
		entry->v4_rx_flow_id = 0;
		entry->v4_rx_counter_hndl = 0;
	}

	if (entry->v6_rx_flow_id) {
		rc = bnxt_ulp_gen_flow_destroy(bp, bp->pf.fw_fid,
					       entry->v6_rx_flow_id);
		netdev_dbg(bp->dev,
			   "DCQCN Delete Rx v6 flow_id: %d session: %d rc: %d\n",
			   entry->v6_rx_flow_id, entry->session_id, rc);
		entry->v6_rx_flow_id = 0;
		entry->v6_rx_counter_hndl = 0;
	}

	if (entry->v4_tx_flow_id) {
		rc = bnxt_ulp_gen_flow_destroy(bp, bp->pf.fw_fid,
					       entry->v4_tx_flow_id);
		netdev_dbg(bp->dev,
			   "DCQCN Delete Tx v4 flow_id: %d session: %d rc: %d\n",
			   entry->v4_tx_flow_id, entry->session_id, rc);
		entry->v4_tx_flow_id = 0;
		entry->v4_tx_counter_hndl = 0;
	}

	if (entry->v6_tx_flow_id) {
		rc = bnxt_ulp_gen_flow_destroy(bp, bp->pf.fw_fid,
					       entry->v6_tx_flow_id);
		netdev_dbg(bp->dev,
			   "DCQCN Delete Tx v6 flow_id: %d session: %d rc: %d\n",
			   entry->v6_tx_flow_id, entry->session_id, rc);
		entry->v6_tx_flow_id = 0;
		entry->v6_tx_counter_hndl = 0;
	}

	return rc;
}

#else

int bnxt_tf_ulp_dcqcn_flow_create(struct bnxt *bp,
				  struct bnxt_dcqcn_session_entry *entry)
{
	return 0;
}

int bnxt_tf_ulp_dcqcn_flow_delete(struct bnxt *bp,
				  struct bnxt_dcqcn_session_entry *entry)
{
	return 0;
}

#endif /* if defined(CONFIG_BNXT_FLOWER_OFFLOAD) */
