/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#ifndef _ULP_TC_PARSER_H_
#define _ULP_TC_PARSER_H_

#include <linux/netdevice.h>
#include <net/netevent.h>
#include <linux/inetdevice.h>
#include <linux/if_vlan.h>
#if defined(HAVE_TC_FLOW_CLS_OFFLOAD) || defined(HAVE_TC_CLS_FLOWER_OFFLOAD)
#include <net/flow_dissector.h>
#include <net/pkt_cls.h>
#include <net/tc_act/tc_gact.h>
#include <net/tc_act/tc_skbedit.h>
#include <net/tc_act/tc_mirred.h>
#include <net/tc_act/tc_vlan.h>
#include <net/tc_act/tc_pedit.h>
#include <net/tc_act/tc_csum.h>
#ifdef HAVE_TC_MPLS_H
#include <net/tc_act/tc_mpls.h>
#endif
#ifdef HAVE_TCF_TUNNEL
#include <net/tc_act/tc_tunnel_key.h>
#endif
#include <net/vxlan.h>
#endif /* HAVE_TC_FLOW_CLS_OFFLOAD || HAVE_TC_CLS_FLOWER_OFFLOAD */

#include "bnxt_compat.h"
#include "bnxt_hsi.h"
#include "bnxt.h"
#include "bnxt_hwrm.h"
#include "bnxt_sriov.h"
#include "bnxt_tc_compat.h"
#include "bnxt_tc.h"
#include "bnxt_vfr.h"
#include "tf_core.h"
#include "ulp_template_db_enum.h"
#include "ulp_template_struct.h"
#include "ulp_mapper.h"
#include "bnxt_tf_common.h"

#if defined(CONFIG_BNXT_FLOWER_OFFLOAD) || defined(CONFIG_BNXT_CUSTOM_FLOWER_OFFLOAD)
/* defines to be used in the tunnel header parsing */
#define BNXT_ULP_ENCAP_IPV4_VER_HLEN_TOS	2
#define BNXT_ULP_ENCAP_IPV4_ID_PROTO		6
#define BNXT_ULP_ENCAP_IPV4_DEST_IP		4
#define BNXT_ULP_ENCAP_IPV4_SIZE		12
#define BNXT_ULP_ENCAP_IPV6_VTC_FLOW		4
#define BNXT_ULP_ENCAP_IPV6_PROTO_TTL		2
#define BNXT_ULP_ENCAP_IPV6_DO			2
#define BNXT_ULP_ENCAP_IPV6_SIZE		24
#define BNXT_ULP_ENCAP_UDP_SIZE			4
#define BNXT_ULP_INVALID_SVIF_VAL		-1U
#define BNXT_ULP_MPLS_DECAP_MAX			2
#define BNXT_ULP_MPLS_PUSH_DEF_TTL		64

#define	BNXT_ULP_GET_IPV6_VER(vtcf)		\
			(((vtcf) & BNXT_ULP_PARSER_IPV6_VER_MASK) >> 28)
#define	BNXT_ULP_GET_IPV6_TC(vtcf)		\
			(((vtcf) & BNXT_ULP_PARSER_IPV6_TC) >> 20)
#define	BNXT_ULP_GET_IPV6_FLOWLABEL(vtcf)	\
			((vtcf) & BNXT_ULP_PARSER_IPV6_FLOW_LABEL)
#define	BNXT_ULP_PARSER_IPV6_VER_MASK		0xf0000000
#define BNXT_ULP_IPV6_DFLT_VER			0x60000000
#define	BNXT_ULP_PARSER_IPV6_TC			0x0ff00000
#define	BNXT_ULP_PARSER_IPV6_FLOW_LABEL		0x000fffff
#define BNXT_ULP_DEFAULT_TTL                    64

enum bnxt_ulp_prsr_action {
	ULP_PRSR_ACT_DEFAULT = 0,
	ULP_PRSR_ACT_MATCH_IGNORE = 1,
	ULP_PRSR_ACT_MASK_IGNORE = 2,
	ULP_PRSR_ACT_SPEC_IGNORE = 4
};

void
bnxt_ulp_init_mapper_params(struct bnxt_ulp_mapper_parms *mparms,
			    struct ulp_tc_parser_params *params,
			    enum bnxt_ulp_fdb_type flow_type);

/* Function to handle the parsing of the RTE port id. */
int
ulp_tc_parser_implicit_match_port_process(struct ulp_tc_parser_params *param);

/* Function to handle the implicit action port id */
int ulp_tc_parser_implicit_act_port_process(struct bnxt *bp,
					    struct ulp_tc_parser_params *params);

/* Functions to handle the parsing of TC Flows and placing
 * the TC flow match fields into the ulp structures.
 */
#ifdef CONFIG_BNXT_FLOWER_OFFLOAD
int bnxt_ulp_tc_parser_hdr_parse(struct bnxt *bp,
				 struct flow_cls_offload *tc_flow_cmd,
				 struct ulp_tc_parser_params *params);
#endif
int ulp_tc_control_key_handler(struct bnxt *bp,
			       struct ulp_tc_parser_params *params,
			       void *match_arg);
int ulp_tc_basic_key_handler(struct bnxt *bp,
			     struct ulp_tc_parser_params *params,
			     void *match_arg);
int ulp_tc_eth_addr_handler(struct bnxt *bp,
			    struct ulp_tc_parser_params *params,
			    void *match_arg);
int ulp_tc_ip_ctrl_handler(struct bnxt *bp,
			   struct ulp_tc_parser_params *params,
			   void *match_arg);
int ulp_tc_ipv4_addr_handler(struct bnxt *bp,
			     struct ulp_tc_parser_params *params,
			     void *match_arg);
int ulp_tc_ipv6_addr_handler(struct bnxt *bp,
			     struct ulp_tc_parser_params *params,
			     void *match_arg);
int ulp_tc_l4_ports_handler(struct bnxt *bp,
			    struct ulp_tc_parser_params *params,
			    void *match_arg);
int ulp_tc_tcp_ctrl_handler(struct bnxt *bp,
			    struct ulp_tc_parser_params *params,
			    void *match_arg);
int bnxt_ulp_tc_parser_post_process(struct ulp_tc_parser_params *params);
#ifdef CONFIG_BNXT_FLOWER_OFFLOAD
int bnxt_ulp_tc_parser_act_parse(struct bnxt *bp,
				 struct flow_cls_offload *tc_flow_cmd,
				 struct ulp_tc_parser_params *params);
#endif
int ulp_tc_redirect_act_handler(struct bnxt *bp,
				struct ulp_tc_parser_params *params,
				void *action_arg);
int ulp_tc_ingress_mirror_act_handler(struct bnxt *bp,
				      struct ulp_tc_parser_params *params,
				      void *action_arg);
int ulp_tc_egress_mirror_act_handler(struct bnxt *bp,
				     struct ulp_tc_parser_params *params,
				     void *action_arg);
int ulp_tc_tunnel_encap_act_handler(struct bnxt *bp,
				    struct ulp_tc_parser_params *params,
				    void *action_arg);
int ulp_tc_mangle_act_handler(struct bnxt *bp,
			      struct ulp_tc_parser_params *params,
			      void *act);
int ulp_tc_csum_act_handler(struct bnxt *bp,
			    struct ulp_tc_parser_params *params,
			    void *act);
int ulp_tc_drop_act_handler(struct bnxt *bp,
			    struct ulp_tc_parser_params *params,
			    void *act);
int ulp_tc_goto_act_handler(struct bnxt *bp,
			    struct ulp_tc_parser_params *params,
			    void *act);
int ulp_tc_tnl_control_key_handler(struct bnxt *bp,
				   struct ulp_tc_parser_params *params,
				   void *match_arg);
int ulp_tc_tnl_ip_ctrl_handler(struct bnxt *bp,
			       struct ulp_tc_parser_params *params,
			       void *match_arg);
int ulp_tc_tnl_ipv4_addr_handler(struct bnxt *bp,
				 struct ulp_tc_parser_params *params,
				 void *match_arg);
int ulp_tc_tnl_ipv6_addr_handler(struct bnxt *bp,
				 struct ulp_tc_parser_params *params,
				 void *match_arg);
int ulp_tc_tnl_l4_ports_handler(struct bnxt *bp,
				struct ulp_tc_parser_params *params,
				void *match_arg);
int ulp_tc_tnl_key_handler(struct bnxt *bp,
			   struct ulp_tc_parser_params *params,
			   void *match_arg);
int ulp_tc_tunnel_decap_act_handler(struct bnxt *bp,
				    struct ulp_tc_parser_params *params,
				    void *action_arg);
int ulp_tc_vlan_handler(struct bnxt *bp, struct ulp_tc_parser_params *params,
			void *match_arg);
int ulp_tc_tnl_mpls_handler(struct bnxt *bp, struct ulp_tc_parser_params *params,
			    void *match_arg);
int ulp_tc_vlan_push_act_handler(struct bnxt *bp,
				 struct ulp_tc_parser_params *params,
				 void *action_arg);
int ulp_tc_vlan_pop_act_handler(struct bnxt *bp,
				struct ulp_tc_parser_params *params,
				void *action_arg);
int
ulp_tc_set_mac_src_act_handler(struct bnxt *bp,
			       struct ulp_tc_parser_params *params);

int
ulp_tc_set_mac_dst_act_handler(struct bnxt *bp,
			       struct ulp_tc_parser_params *params);

int
ulp_tc_meter_act_handler(struct bnxt *bp,
			 struct ulp_tc_parser_params *params);
int ulp_tc_mpls_pop_act_handler(struct bnxt *bp,
				struct ulp_tc_parser_params *params,
				void *action_arg);
int ulp_tc_mpls_push_act_handler(struct bnxt *bp,
				 struct ulp_tc_parser_params *params,
				 void *action_arg);

int ulp_tc_parser_implicit_match_port_process(struct ulp_tc_parser_params *params);
int ulp_tc_parser_implicit_act_port_process(struct bnxt *bp,
					    struct ulp_tc_parser_params *params);
int ulp_tc_parser_act_port_set(struct ulp_tc_parser_params *param, u32 ifindex, bool lag);
#ifdef CONFIG_BNXT_FLOWER_OFFLOAD
int bnxt_flow_meter_profile_add(struct bnxt *bp, u32 meter_profile_id, u32 dir,
				enum bnxt_ulp_meter_color color);
int bnxt_flow_meter_profile_delete(struct bnxt *bp, u32 meter_profile_id, u32 dir);
int bnxt_flow_meter_create(struct bnxt *bp, u32 meter_profile_id, u32 meter_id, u32 dir);
int bnxt_flow_meter_destroy(struct bnxt *bp, u32 meter_id, u32 dir);
int bnxt_tc_clear_dscp(struct bnxt *bp, struct bnxt_ulp_context *ulp_ctx,
		       u16 vf_id, u32 dscp_remap_val);
void bnxt_tc_uninit_dscp_remap(struct bnxt *bp, struct bnxt_ulp_dscp_remap
				      *dscp_remap, u32 dir);
int bnxt_tc_init_dscp_remap(struct bnxt *bp, struct bnxt_ulp_dscp_remap
			    *dscp_remap, u32 dir);
#endif

#endif	/* CONFIG_BNXT_FLOWER_OFFLOAD || CONFIG_BNXT_CUSTOM_FLOWER_OFFLOAD */

#endif /* _ULP_TC_PARSER_H_ */
