// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#include "bnxt_compat.h"
#include "bnxt.h"
#include "bnxt_tfc.h"
#include "ulp_template_db_enum.h"
#include "ulp_template_struct.h"
#include "ulp_tc_parser.h"

#ifdef CONFIG_BNXT_FLOWER_OFFLOAD
/* The below array is the list of parsing functions for each of the flow
 * dissector keys that are supported.
 * NOTE: Updating this table with new keys requires that the corresponding
 * key seqence also be updated in the table ulp_hdr_parse_sequence[] in
 * ulp_tc_parser.c.
 */
struct bnxt_ulp_tc_hdr_info ulp_hdr_info[] = {
	[FLOW_DISSECTOR_KEY_CONTROL] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_SUPPORTED,
	.proto_hdr_func          = ulp_tc_control_key_handler
	},
	[FLOW_DISSECTOR_KEY_BASIC] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_SUPPORTED,
	.proto_hdr_func          = ulp_tc_basic_key_handler
	},
	[FLOW_DISSECTOR_KEY_IPV4_ADDRS] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_SUPPORTED,
	.proto_hdr_func          = ulp_tc_ipv4_addr_handler
	},
	[FLOW_DISSECTOR_KEY_IPV6_ADDRS] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_SUPPORTED,
	.proto_hdr_func          = ulp_tc_ipv6_addr_handler
	},
	[FLOW_DISSECTOR_KEY_PORTS] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_SUPPORTED,
	.proto_hdr_func          = ulp_tc_l4_ports_handler
	},
	[FLOW_DISSECTOR_KEY_ETH_ADDRS] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_SUPPORTED,
	.proto_hdr_func          = ulp_tc_eth_addr_handler
	},
	[FLOW_DISSECTOR_KEY_VLAN] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_SUPPORTED,
	.proto_hdr_func          = ulp_tc_vlan_handler
	},
	[FLOW_DISSECTOR_KEY_TCP] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_SUPPORTED,
	.proto_hdr_func          = ulp_tc_tcp_ctrl_handler
	},
	[FLOW_DISSECTOR_KEY_IP] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_SUPPORTED,
	.proto_hdr_func          = ulp_tc_ip_ctrl_handler
	},
	[FLOW_DISSECTOR_KEY_ENC_KEYID] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_SUPPORTED,
	.proto_hdr_func          = ulp_tc_tnl_key_handler
	},
	[FLOW_DISSECTOR_KEY_ENC_IPV4_ADDRS] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_SUPPORTED,
	.proto_hdr_func          = ulp_tc_tnl_ipv4_addr_handler
	},
	[FLOW_DISSECTOR_KEY_ENC_IPV6_ADDRS] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_SUPPORTED,
	.proto_hdr_func          = ulp_tc_tnl_ipv6_addr_handler
	},
	[FLOW_DISSECTOR_KEY_ENC_CONTROL] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_SUPPORTED,
	.proto_hdr_func          = ulp_tc_tnl_control_key_handler
	},
	[FLOW_DISSECTOR_KEY_ENC_PORTS] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_SUPPORTED,
	.proto_hdr_func          = ulp_tc_tnl_l4_ports_handler
	},
	[FLOW_DISSECTOR_KEY_ENC_IP] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_SUPPORTED,
	.proto_hdr_func          = ulp_tc_tnl_ip_ctrl_handler
	},
	[FLOW_DISSECTOR_KEY_MPLS] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_SUPPORTED,
	.proto_hdr_func          = ulp_tc_tnl_mpls_handler
	},
	[FLOW_DISSECTOR_KEY_MAX] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	}
};

/* This structure has to be indexed based on the TC flow_action_id.
 * The below array is list of parsing functions for each of the
 * flow actions that are supported.
 */
struct bnxt_ulp_tc_act_info ulp_act_info[] = {
	[FLOW_ACTION_DROP] = {
	.act_type                = BNXT_ULP_ACT_TYPE_SUPPORTED,
	.proto_act_func          = ulp_tc_drop_act_handler
	},
	[FLOW_ACTION_GOTO] = {
	.act_type                = BNXT_ULP_ACT_TYPE_SUPPORTED,
	.proto_act_func          = ulp_tc_goto_act_handler
	},
	[FLOW_ACTION_TUNNEL_ENCAP] = {
	.act_type                = BNXT_ULP_ACT_TYPE_SUPPORTED,
	.proto_act_func          = ulp_tc_tunnel_encap_act_handler
	},
	[FLOW_ACTION_TUNNEL_DECAP] = {
	.act_type                = BNXT_ULP_ACT_TYPE_SUPPORTED,
	.proto_act_func          = ulp_tc_tunnel_decap_act_handler
	},
	[FLOW_ACTION_REDIRECT] = {
	.act_type                = BNXT_ULP_ACT_TYPE_SUPPORTED,
	.proto_act_func          = ulp_tc_redirect_act_handler
	},
	[FLOW_ACTION_MIRRED] = {
	.act_type                = BNXT_ULP_ACT_TYPE_SUPPORTED,
	.proto_act_func          = ulp_tc_egress_mirror_act_handler
	},
#if defined(HAVE_FLOW_ACTION_MIRRED_INGRESS)
	[FLOW_ACTION_MIRRED_INGRESS] = {
	.act_type                = BNXT_ULP_ACT_TYPE_SUPPORTED,
	.proto_act_func          = ulp_tc_ingress_mirror_act_handler
	},
#endif
	[FLOW_ACTION_MANGLE] = {
	.act_type                = BNXT_ULP_ACT_TYPE_SUPPORTED,
	.proto_act_func          = ulp_tc_mangle_act_handler
	},
	[FLOW_ACTION_CSUM] = {
	.act_type                = BNXT_ULP_ACT_TYPE_SUPPORTED,
	.proto_act_func          = ulp_tc_csum_act_handler
	},
	[FLOW_ACTION_VLAN_PUSH] = {
	.act_type                = BNXT_ULP_ACT_TYPE_SUPPORTED,
	.proto_act_func          = ulp_tc_vlan_push_act_handler
	},
	[FLOW_ACTION_VLAN_POP] = {
	.act_type                = BNXT_ULP_ACT_TYPE_SUPPORTED,
	.proto_act_func          = ulp_tc_vlan_pop_act_handler
	},
	[FLOW_ACTION_ADD] = {
	.act_type                = BNXT_ULP_ACT_TYPE_SUPPORTED,
	.proto_act_func          = ulp_tc_mangle_act_handler
	},
	[FLOW_ACTION_MPLS_POP] = {
	.act_type                = BNXT_ULP_ACT_TYPE_SUPPORTED,
	.proto_act_func          = ulp_tc_mpls_pop_act_handler
	},
	[FLOW_ACTION_MPLS_PUSH] = {
	.act_type                = BNXT_ULP_ACT_TYPE_SUPPORTED,
	.proto_act_func          = ulp_tc_mpls_push_act_handler
	},
	[NUM_FLOW_ACTIONS] = {
	.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
	.proto_act_func          = NULL
	}
};

#endif	/* CONFIG_BNXT_FLOWER_OFFLOAD */
