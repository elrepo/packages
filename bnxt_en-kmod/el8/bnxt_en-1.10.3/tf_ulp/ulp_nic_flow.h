/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Broadcom
 * All rights reserved.
 */

#ifndef _ULP_NIC_FLOW_H_
#define _ULP_NIC_FLOW_H_

enum ulp_nic_flow_type {
	NIC_FLOW_TYPE_ROCE_V4 = 0,
	NIC_FLOW_TYPE_ROCE_V6,
	NIC_FLOW_TYPE_ROCE_V4_CNP,
	NIC_FLOW_TYPE_ROCE_V6_CNP,
	NIC_FLOW_TYPE_ROCE_V4_PROBE, /* rx only */
	NIC_FLOW_TYPE_ROCE_V6_PROBE, /* rx only */
	NIC_FLOW_TYPE_ROCE_V4_ACK, /* tx only */
	NIC_FLOW_TYPE_ROCE_V6_ACK, /* tx only */
	NIC_FLOW_TYPE_ROCE_V4_LPBK, /* tx only */
	NIC_FLOW_TYPE_ROCE_V6_LPBK, /* tx only */
	NIC_FLOW_TYPE_MAX
};

struct ulp_nic_flows {
	u32	id[NIC_FLOW_TYPE_MAX];
	u64	cnt_hndl[NIC_FLOW_TYPE_MAX];
};

/* ROCE lowest priority */
#define ULP_NIC_FLOW_ROCE_PRI     1
/* ROCE CNP priority */
#define ULP_NIC_FLOW_ROCE_CNP_PRI 2
/* ROCE PROBE priority */
#define ULP_NIC_FLOW_ROCE_PROBE_PRI 3
/* ROCE LBPK priority */
#define ULP_NIC_FLOW_ROCE_LPBK_PRI 4
/* NTUPLE priority */
#define ULP_NIC_FLOW_NTUPLE_PRI     1
/* KTLS priority */
#define ULP_NIC_FLOW_KTLS_PRI     1

enum flow_lkup_strength {
	FLOW_LKUP_STRENGTH_LO = 0,
	FLOW_LKUP_STRENGTH_M1,
	FLOW_LKUP_STRENGTH_M2,
	FLOW_LKUP_STRENGTH_HI
};

#define BNXT_FLOW_CLASS_WC		0
#define BNXT_FLOW_CLASS_EM		1
#define L3_ADDR_MAX_SIZE		16

static const u8 ip_all_zero[L3_ADDR_MAX_SIZE] = { };
static const u8 ipv6_all_mask[L3_ADDR_MAX_SIZE] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
					     0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

/* Add per DMAC RoCE and RoCE CNP flows
 * @flows[out]: pointer to the handles for the rx or tx nic flows
 * @flow_info[in]: the direction to configure
 * return 0 on success and - on failure
 */
int bnxt_ulp_nic_flows_roce_add(struct bnxt *bp,
				struct ulp_nic_flows *flows, enum cfa_dir dir);

/* Delete per DMAC RoCE and RoCE CNP flows
 * @flows[out]: flows to delete
 * @flow_info[in]: the direction these flows are associated with
 * return 0 on success and - on failure
 */
int bnxt_ulp_nic_flows_roce_del(struct bnxt *bp,
				struct ulp_nic_flows *flows, enum cfa_dir dir);

int bnxt_tf_ntuple_flow_create(struct bnxt *bp,
			       struct bnxt_ntuple_filter *fltr,
			       u32 *flow_id, u64 *flow_cnt_hndl,
			       enum cfa_dir dir);
int bnxt_tf_ntuple_flow_del(struct bnxt *bp, struct bnxt_ntuple_filter *fltr);
int bnxt_tf_tls_flow_create(struct bnxt *bp, struct sock *sk,
			    u64 *flow_id, u64 *flow_cnt_hndl,
			    u32 kid, u8 type);
int bnxt_tf_tls_flow_del(struct bnxt *bp, __le64 filter_id);

int bnxt_tf_l2_filter_create(struct bnxt *bp,
			     struct bnxt_l2_filter *fltr);
int bnxt_tf_l2_filter_delete(struct bnxt *bp,
			     struct bnxt_l2_filter *fltr);

#endif /* #ifndef _ULP_NIC_FLOW_H_ */
