/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2025 Broadcom
 * All rights reserved.
 */

#ifndef BNXT_DCQCN_H
#define BNXT_DCQCN_H

enum bnxt_dcqcn_flow_type {
	DCQCN_FLOW_TYPE_ROCE = 0,
	DCQCN_FLOW_TYPE_ROCE_MONITOR,
	DCQCN_FLOW_TYPE_MAX
};

struct bnxt_dcqcn_session_entry {
	u32			session_id;
	u32			v4_rx_flow_id;
	u32			v6_rx_flow_id;
	u32			v4_tx_flow_id;
	u32			v6_tx_flow_id;
	u64			v4_rx_counter_hndl;
	u64			v6_rx_counter_hndl;
	u64			v4_tx_counter_hndl;
	u64			v6_tx_counter_hndl;
	u8			dest_mac[ETH_ALEN];
	u8			src_mac[ETH_ALEN];
	u8			dst_mac_mod[ETH_ALEN];
	u8			src_mac_mod[ETH_ALEN];
	struct in6_addr		dst_ip;
	struct in6_addr		src_ip;
	u8                      l4_proto;
	u32			src_qp_num;
	u32			dest_qp_num;
	struct bnxt		*bp;
	bool			v4_dst;
	bool			skip_subnet_checking;
	enum bnxt_dcqcn_flow_type type;
	u16			vnic_id;
	u32			ifa_gns;
	struct bnxt_dcqcn_session_entry *peer_entry;

};

/* Add the DCQCN flows */
int bnxt_tf_ulp_dcqcn_flow_create(struct bnxt *bp,
				  struct bnxt_dcqcn_session_entry *entry);

/* Delete the DCQCN flows */
int bnxt_tf_ulp_dcqcn_flow_delete(struct bnxt *bp,
				  struct bnxt_dcqcn_session_entry *entry);

#endif /* #ifndef BNXT_DCQCN_H */
