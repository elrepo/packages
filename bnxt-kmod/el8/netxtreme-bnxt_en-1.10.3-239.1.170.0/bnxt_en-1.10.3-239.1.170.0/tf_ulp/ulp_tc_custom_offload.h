/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023-2023 Broadcom
 * All rights reserved.
 */

#ifndef _ULP_TC_CUSTOM_OFFLOAD_H_
#define _ULP_TC_CUSTOM_OFFLOAD_H_

#ifdef CONFIG_BNXT_CUSTOM_FLOWER_OFFLOAD
#include "ulp_tc_rte_flow.h"

enum bnxt_rte_flow_item_type {
	BNXT_RTE_FLOW_ITEM_TYPE_END = (u32)INT_MIN,
	BNXT_RTE_FLOW_ITEM_TYPE_VXLAN_DECAP,
	BNXT_RTE_FLOW_ITEM_TYPE_LAST
};

enum bnxt_rte_flow_action_type {
	BNXT_RTE_FLOW_ACTION_TYPE_END = (u32)INT_MIN,
	BNXT_RTE_FLOW_ACTION_TYPE_VXLAN_DECAP,
	BNXT_RTE_FLOW_ACTION_TYPE_LAST
};

/* Local defines for the parsing functions */
#define ULP_VLAN_PRIORITY_SHIFT		13 /* First 3 bits */
#define ULP_VLAN_PRIORITY_MASK		0x700
#define ULP_VLAN_TAG_MASK		0xFFF /* Last 12 bits*/
#define ULP_UDP_PORT_VXLAN		4789
#define ULP_UDP_PORT_VXLAN_MASK		0XFFFF

/* Ethernet frame types */
#define RTE_ETHER_TYPE_IPV4	0x0800 /**< IPv4 Protocol. */
#define RTE_ETHER_TYPE_IPV6	0x86DD /**< IPv6 Protocol. */
#define RTE_ETHER_TYPE_ARP	0x0806 /**< Arp Protocol. */
#define RTE_ETHER_TYPE_RARP	0x8035 /**< Reverse Arp Protocol. */
#define RTE_ETHER_TYPE_VLAN	0x8100 /**< IEEE 802.1Q VLAN tagging. */
#define RTE_ETHER_TYPE_QINQ	0x88A8 /**< IEEE 802.1ad QinQ tagging. */
#define RTE_ETHER_TYPE_QINQ1	0x9100 /**< Deprecated QinQ VLAN. */
#define RTE_ETHER_TYPE_QINQ2	0x9200 /**< Deprecated QinQ VLAN. */
#define RTE_ETHER_TYPE_QINQ3	0x9300 /**< Deprecated QinQ VLAN. */
#define RTE_ETHER_TYPE_PPPOE_DISCOVERY	0x8863 /**< PPPoE Discovery Stage. */
#define RTE_ETHER_TYPE_PPPOE_SESSION	0x8864 /**< PPPoE Session Stage. */
#define RTE_ETHER_TYPE_ETAG		0x893F /**< IEEE 802.1BR E-Tag. */
#define RTE_ETHER_TYPE_1588		0x88F7
/**< IEEE 802.1AS 1588 Precise Time Protocol. */
#define RTE_ETHER_TYPE_SLOW	0x8809 /**< Slow protocols (LACP and Marker). */
#define RTE_ETHER_TYPE_TEB	0x6558 /**< Transparent Ethernet Bridging. */
#define RTE_ETHER_TYPE_LLDP	0x88CC /**< LLDP Protocol. */
#define RTE_ETHER_TYPE_MPLS	0x8847 /**< MPLS ethertype. */
#define RTE_ETHER_TYPE_MPLSM	0x8848 /**< MPLS multicast ethertype. */
#define RTE_ETHER_TYPE_ECPRI	0xAEFE /**< eCPRI ethertype (.1Q supported). */

#ifdef ULP_APP_TOS_PROTO_SUPPORT
#undef ULP_APP_TOS_PROTO_SUPPORT
#endif
#define ULP_APP_TOS_PROTO_SUPPORT(x)	1

/* Flow Parser Header Information Structure */
struct bnxt_ulp_rte_hdr_info {
	enum bnxt_ulp_hdr_type hdr_type;
	/* Flow Parser Protocol Header Function Prototype */
	int (*proto_hdr_func)(const struct rte_flow_item *item_list,
			      struct ulp_tc_parser_params *params);
};

/* Flow Parser Action Information Structure */
struct bnxt_ulp_rte_act_info {
	enum bnxt_ulp_act_type act_type;
	/* Flow Parser Protocol Action Function Prototype */
	int (*proto_act_func)(const struct rte_flow_action *action_item,
			      struct ulp_tc_parser_params *params);
};

int
bnxt_custom_ulp_flow_create(struct bnxt *bp, u16 src_fid,
			    const struct rte_flow_item pattern[],
			    const struct rte_flow_action actions[],
			    struct bnxt_ulp_flow_info *flow_info);
#endif
#endif
