// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#include <linux/vmalloc.h>

#include "bnxt_compat.h"
#include "bnxt_hsi.h"
#include "bnxt.h"
#include "ulp_utils.h"
#include "bnxt_tf_ulp.h"
#include "ulp_template_db_enum.h"
#include "ulp_template_struct.h"
#include "ulp_template_debug.h"
#include "ulp_template_debug_proto.h"

#if defined(CONFIG_BNXT_FLOWER_OFFLOAD) || defined(CONFIG_BNXT_CUSTOM_FLOWER_OFFLOAD)
#ifdef TC_BNXT_TRUFLOW_DEBUG

const char *ulp_tc_hdr_comp_field_names[] = {
	"BNXT_ULP_CF_IDX_NOT_USED",
	"BNXT_ULP_CF_IDX_MPLS_TAG_NUM",
	"BNXT_ULP_CF_IDX_O_VTAG_NUM",
	"BNXT_ULP_CF_IDX_O_HAS_VTAG",
	"BNXT_ULP_CF_IDX_O_ONE_VTAG",
	"BNXT_ULP_CF_IDX_O_TWO_VTAGS",
	"BNXT_ULP_CF_IDX_I_VTAG_NUM",
	"BNXT_ULP_CF_IDX_I_HAS_VTAG",
	"BNXT_ULP_CF_IDX_I_ONE_VTAG",
	"BNXT_ULP_CF_IDX_I_TWO_VTAGS",
	"BNXT_ULP_CF_IDX_INCOMING_IF",
	"BNXT_ULP_CF_IDX_DIRECTION",
	"BNXT_ULP_CF_IDX_SVIF_FLAG",
	"BNXT_ULP_CF_IDX_O_L3",
	"BNXT_ULP_CF_IDX_I_L3",
	"BNXT_ULP_CF_IDX_O_L4",
	"BNXT_ULP_CF_IDX_I_L4",
	"BNXT_ULP_CF_IDX_O_L4_SRC_PORT",
	"BNXT_ULP_CF_IDX_O_L4_DST_PORT",
	"BNXT_ULP_CF_IDX_I_L4_SRC_PORT",
	"BNXT_ULP_CF_IDX_I_L4_DST_PORT",
	"BNXT_ULP_CF_IDX_O_L4_SRC_PORT_MASK",
	"BNXT_ULP_CF_IDX_O_L4_DST_PORT_MASK",
	"BNXT_ULP_CF_IDX_I_L4_SRC_PORT_MASK",
	"BNXT_ULP_CF_IDX_I_L4_DST_PORT_MASK",
	"BNXT_ULP_CF_IDX_O_L4_FB_SRC_PORT",
	"BNXT_ULP_CF_IDX_O_L4_FB_DST_PORT",
	"BNXT_ULP_CF_IDX_I_L4_FB_SRC_PORT",
	"BNXT_ULP_CF_IDX_I_L4_FB_DST_PORT",
	"BNXT_ULP_CF_IDX_O_L3_FB_PROTO_ID",
	"BNXT_ULP_CF_IDX_I_L3_FB_PROTO_ID",
	"BNXT_ULP_CF_IDX_O_L3_PROTO_ID",
	"BNXT_ULP_CF_IDX_I_L3_PROTO_ID",
	"BNXT_ULP_CF_IDX_O_L3_TTL",
	"BNXT_ULP_CF_IDX_DEV_PORT_ID",
	"BNXT_ULP_CF_IDX_DRV_FUNC_SVIF",
	"BNXT_ULP_CF_IDX_DRV_FUNC_SPIF",
	"BNXT_ULP_CF_IDX_DRV_FUNC_PARIF",
	"BNXT_ULP_CF_IDX_DRV_FUNC_VNIC",
	"BNXT_ULP_CF_IDX_DRV_FUNC_PHY_PORT",
	"BNXT_ULP_CF_IDX_VF_FUNC_SVIF",
	"BNXT_ULP_CF_IDX_VF_FUNC_SPIF",
	"BNXT_ULP_CF_IDX_VF_FUNC_PARIF",
	"BNXT_ULP_CF_IDX_VF_FUNC_VNIC",
	"BNXT_ULP_CF_IDX_VNIC",
	"BNXT_ULP_CF_IDX_PHY_PORT_SVIF",
	"BNXT_ULP_CF_IDX_PHY_PORT_SPIF",
	"BNXT_ULP_CF_IDX_PHY_PORT_PARIF",
	"BNXT_ULP_CF_IDX_PHY_PORT_VPORT",
	"BNXT_ULP_CF_IDX_ACT_ENCAP_IPV4_FLAG",
	"BNXT_ULP_CF_IDX_ACT_ENCAP_IPV6_FLAG",
	"BNXT_ULP_CF_IDX_ACT_DEC_TTL",
	"BNXT_ULP_CF_IDX_ACT_T_DEC_TTL",
	"BNXT_ULP_CF_IDX_ACT_PORT_IS_SET",
	"BNXT_ULP_CF_IDX_ACT_PORT_TYPE",
	"BNXT_ULP_CF_IDX_ACT_MIRR_PORT_IS_SET",
	"BNXT_ULP_CF_IDX_ACT_MIRR_PORT_TYPE",
	"BNXT_ULP_CF_IDX_MATCH_PORT_TYPE",
	"BNXT_ULP_CF_IDX_MATCH_PORT_IS_VFREP",
	"BNXT_ULP_CF_IDX_MATCH_PORT_IS_PF",
	"BNXT_ULP_CF_IDX_VF_TO_VF",
	"BNXT_ULP_CF_IDX_L3_HDR_CNT",
	"BNXT_ULP_CF_IDX_L4_HDR_CNT",
	"BNXT_ULP_CF_IDX_VFR_MODE",
	"BNXT_ULP_CF_IDX_L3_TUN",
	"BNXT_ULP_CF_IDX_L3_TUN_DECAP",
	"BNXT_ULP_CF_IDX_FID",
	"BNXT_ULP_CF_IDX_HDR_SIG_ID",
	"BNXT_ULP_CF_IDX_FLOW_SIG_ID",
	"BNXT_ULP_CF_IDX_WC_MATCH",
	"BNXT_ULP_CF_IDX_WC_IS_HA_HIGH_REG",
	"BNXT_ULP_CF_IDX_TUNNEL_ID",
	"BNXT_ULP_CF_IDX_TUN_OFF_DIP_ID",
	"BNXT_ULP_CF_IDX_TUN_OFF_DMAC_ID",
	"BNXT_ULP_CF_IDX_OO_VLAN_FB_VID",
	"BNXT_ULP_CF_IDX_OI_VLAN_FB_VID",
	"BNXT_ULP_CF_IDX_IO_VLAN_FB_VID",
	"BNXT_ULP_CF_IDX_II_VLAN_FB_VID",
	"BNXT_ULP_CF_IDX_SOCKET_DIRECT",
	"BNXT_ULP_CF_IDX_SOCKET_DIRECT_VPORT",
	"BNXT_ULP_CF_IDX_TUNNEL_SPORT",
	"BNXT_ULP_CF_IDX_VF_META_FID",
	"BNXT_ULP_CF_IDX_DEV_ACT_PORT_ID",
	"BNXT_ULP_CF_IDX_DEV_ACT_MIRR_PORT_ID",
	"BNXT_ULP_CF_IDX_O_VLAN_NO_IGNORE",
	"BNXT_ULP_CF_IDX_I_VLAN_NO_IGNORE",
	"BNXT_ULP_CF_IDX_HA_SUPPORT_DISABLED",
	"BNXT_ULP_CF_IDX_FUNCTION_ID",
	"BNXT_ULP_CF_IDX_CHAIN_ID_METADATA",
	"BNXT_ULP_CF_IDX_SRV6_UPAR_ID",
	"BNXT_ULP_CF_IDX_SRV6_T_ID",
	"BNXT_ULP_CF_IDX_GENERIC_SIZE",
	"BNXT_ULP_CF_IDX_APP_PRIORITY",
	"BNXT_ULP_CF_IDX_MIRROR_COPY_ING_OR_EGR",
	"BNXT_ULP_CF_IDX_EM_FOR_TC",
	"BNXT_ULP_CF_IDX_L2_CUSTOM_UPAR_ID",
	"BNXT_ULP_CF_IDX_CUSTOM_GRE_EN",
	"BNXT_ULP_CF_IDX_UPAR_HIGH_EN",
	"BNXT_ULP_CF_IDX_MP_NPORTS",
	"BNXT_ULP_CF_IDX_MP_PORT_A",
	"BNXT_ULP_CF_IDX_MP_VNIC_A",
	"BNXT_ULP_CF_IDX_MP_VPORT_A",
	"BNXT_ULP_CF_IDX_MP_MDATA_A",
	"BNXT_ULP_CF_IDX_MP_A_IS_VFREP",
	"BNXT_ULP_CF_IDX_MP_PORT_B",
	"BNXT_ULP_CF_IDX_MP_VNIC_B",
	"BNXT_ULP_CF_IDX_MP_VPORT_B",
	"BNXT_ULP_CF_IDX_MP_MDATA_B",
	"BNXT_ULP_CF_IDX_MP_B_IS_VFREP",
	"BNXT_ULP_CF_IDX_VXLAN_IP_UPAR_ID",
	"BNXT_ULP_CF_IDX_ACT_REJ_COND_EN",
	"BNXT_ULP_CF_IDX_HDR_BITMAP",
	"BNXT_ULP_CF_IDX_PROFILE_BITMAP",
	"BNXT_ULP_CF_IDX_VF_ROCE_EN",
	"BNXT_ULP_CF_IDX_LAST"
};

const char *ulp_tc_hdr_bth_field_names[] = {
	"BTH Opcode",
	"BTH Dest QP",
	"BTH Flags"
};

const char *ulp_tc_hdr_svif_names[] = {
	"Wild Card",
	"SVIF",
};

const char *ulp_tc_hdr_eth_field_names[] = {
	"Dst Mac",
	"Src Mac",
	"Ether Type",
};

const char *ulp_tc_hdr_vlan_field_names[] = {
	"Priority",
	"Vlan Id",
	"Vlan-Ether Type",
};

const char *ulp_tc_hdr_ipv4_field_names[] = {
	"Version",
	"Type of Service",
	"Length",
	"Fragment Id",
	"Fragment Offset",
	"TTL",
	"Next Proto",
	"Checksum",
	"Src Addr",
	"Dst Addr"
};

const char *ulp_tc_hdr_ipv6_field_names[] = {
	"Version",
	"Traffic Class",
	"Flow Label",
	"Length",
	"Proto",
	"Hop limits",
	"Src Addr",
	"Dst Addr"
};

const char *ulp_tc_hdr_udp_field_names[] = {
	"Src Port",
	"Dst Port",
	"Length",
	"Checksum"
};

const char *ulp_tc_hdr_vxlan_field_names[] = {
	"Vxlan Flags",
	"Reserved",
	"VNI",
	"Reserved"
};

const char *ulp_tc_hdr_ifa_field_names[] = {
	"GNS"
};

const char *ulp_tc_hdr_tcp_field_names[] = {
	"Src Port",
	"Dst Port",
	"Sent Seq",
	"Recv Ack",
	"Data Offset",
	"Tcp flags",
	"Rx Window",
	"Checksum",
	"URP",
};

const char *ulp_tc_hdr_icmp_field_names[] = {
	"icmp type",
	"icmp code",
	"icmp cksum",
	"icmp ident",
	"icmp seq num"
};

const char *ulp_tc_hdr_ecpri_field_names[] = {
	"eCPRI type",
	"eCPRI id"
};

const char *ulp_mapper_resource_func_names[] = {
	[BNXT_ULP_RESOURCE_FUNC_INVALID] = "Invalid Table",
	[BNXT_ULP_RESOURCE_FUNC_EM_TABLE] = "EM Table",
	[BNXT_ULP_RESOURCE_FUNC_CMM_TABLE] = "CMM Table",
	[BNXT_ULP_RESOURCE_FUNC_CMM_STAT] = "CMM STAT",
	[BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE] = "Tcam Table",
	[BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE] = "Index Table",
	[BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE] = "Generic Table",
	[BNXT_ULP_RESOURCE_FUNC_IDENTIFIER] = "Identifier table",
	[BNXT_ULP_RESOURCE_FUNC_IF_TABLE] = "Interface Table",
	[BNXT_ULP_RESOURCE_FUNC_HW_FID] = "FID Table",
	[BNXT_ULP_RESOURCE_FUNC_PARENT_FLOW] = "Parent Flow",
	[BNXT_ULP_RESOURCE_FUNC_CHILD_FLOW] = "Child Flow",
	[BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE] = "Control Table",
	[BNXT_ULP_RESOURCE_FUNC_VNIC_TABLE] = "Vnic Table",
	[BNXT_ULP_RESOURCE_FUNC_GLOBAL_REGISTER_TABLE] = "Global Reg Table",
	[BNXT_ULP_RESOURCE_FUNC_UDCC_V6SUBNET_TABLE] = "v6 Subnet Table",
	[BNXT_ULP_RESOURCE_FUNC_KEY_RECIPE_TABLE] = "Key Recipe Table",
	[BNXT_ULP_RESOURCE_FUNC_ALLOCATOR_TABLE] = "Allocator Table",
	[BNXT_ULP_RESOURCE_FUNC_STATS_CACHE] = "Stats Cache Table",
};

const char *ulp_mapper_res_ulp_global_names[] = {
	[BNXT_ULP_RESOURCE_SUB_TYPE_GLOBAL_REGISTER_CUST_VXLAN] =
	"Custom VxLAN",
	[BNXT_ULP_RESOURCE_SUB_TYPE_GLOBAL_REGISTER_CUST_ECPRI] =
	"Custom eCPRI",
	[BNXT_ULP_RESOURCE_SUB_TYPE_GLOBAL_REGISTER_CUST_VXLAN_GPE] =
	"Custom Vxlan GPE",
	[BNXT_ULP_RESOURCE_SUB_TYPE_GLOBAL_REGISTER_CUST_VXLAN_GPE_V6] =
	"Custom Vxlan GPEv6",
	[BNXT_ULP_RESOURCE_SUB_TYPE_GLOBAL_REGISTER_CUST_VXLAN_IP] =
	"Custom Vxlan IP",
	[BNXT_ULP_RESOURCE_SUB_TYPE_GLOBAL_REGISTER_CUST_GENEVE] =
	"Custom Geneve",
};

const char *ulp_mapper_res_key_recipe_names[] = {
	[BNXT_ULP_RESOURCE_SUB_TYPE_KEY_RECIPE_TABLE_EM] =
		"EM Key Recipe",
	[BNXT_ULP_RESOURCE_SUB_TYPE_KEY_RECIPE_TABLE_WM] =
		"WC Key Recipe"
};

const char *ulp_mapper_res_index_names[] = {
	[BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL] = "Normal",
	[BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_VFR_CFA_ACTION] = "CFA Action",
	[BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_INT_COUNT] = "Internal counter",
	[BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_INT_COUNT_ACC] = "Agg Counter",
	[BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_EXT_COUNT] = "External Counter",
	[BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_CFA_TBLS] = "CFA Metadata Prof",
};

const char *ulp_mapper_res_generic_names[] = {
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_L2_CNTXT_TCAM] = "L2 Ctxt",
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_PROFILE_TCAM] = "Prof Tcam",
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SHARED_MIRROR] = "Mirror Tbl",
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_MAC_ADDR_CACHE] =
		"Mac Addr Cache",
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_PORT_TABLE] = "Port Tbl",
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_TUNNEL_CACHE] =
		"Tunnel Cache",
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SOURCE_PROPERTY_CACHE] =
		"Source Property Tbl",
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_VXLAN_ENCAP_REC_CACHE] =
		"Vxlan Encap Record Tbl",
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SOCKET_DIRECT_CACHE] =
		"Socket Direct Cache",
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SOURCE_PROPERTY_IPV6_CACHE] =
		"v6 Source Property Tbl",
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_L2_ENCAP_REC_CACHE] =
		"L2 Encap Record Tbl",
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SRV6_ENCAP_REC_CACHE] =
		"SRV6 Encap Record Tbl",
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_VXLAN_ENCAP_REC_CACHE] =
		"Vxlan Encap Record Tbl",
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_VXLAN_ENCAP_IPV6_REC_CACHE] =
		"IPv6 Encap Record Tbl",
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SOCKET_DIRECT_CACHE] =
		"Socket Direct Cache",
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_METER_PROFILE_TBL_CACHE] =
		"Meter Profile Tbl Cache",
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SHARED_METER_TBL_CACHE] =
		"Meter Tbl Cache",
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_TABLE_SCOPE_CACHE] =
		"Table Scope Cache",
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_GENEVE_ENCAP_REC_CACHE] =
		"Geneve Encap Record Cache",
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_PROTO_HEADER] =
		"Protocol Header Cache",
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_EM_FLOW_CONFLICT] =
		"EM Flow Conflict",
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_HDR_OVERLAP] =
		"Hdr Bitmap Overlap Cache",
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_FLOW_CHAIN_CACHE] =
		"Flow Chain Cache",
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_FLOW_CHAIN_L2_CNTXT] =
		"Flow Chain L2 context",
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_TUNNEL_GPARSE_CACHE] =
	"Tunnel Gparse Cache",
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_MULTI_FLOW_TUNNEL_CACHE] =
	"Multiflow Tunnel Cache",
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_L2_FILTER] =
	"L2 Filter",
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_IFA_TUNNEL_CACHE] =
	"IFA Tunnel Cache",
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_L2_CNTXT_ID_CACHE] =
	"L2 Ctxt Id Cache",

};

/* Utility Function to dump a simple buffer of a given length. */
static void dump_hex(struct bnxt_ulp_context *ulp_ctx,
		     u8 *ptr, u32 size)
{
	u8 *lbuffer_ptr;
	u8 *lbuffer;
	int ret;
	u32 i;

	lbuffer = vzalloc(1024);
	if (!lbuffer)
		return;

	lbuffer_ptr = lbuffer;

	ret = sprintf((char *)lbuffer_ptr, "\t\t\t");
	lbuffer_ptr += ret;
	for (i = 0; i < size; i++, ptr++) {
		if (i && !(i % 16)) {
			ret = sprintf((char *)lbuffer_ptr, "\t\t\t\t");
			lbuffer_ptr += ret;
		}
		ret = sprintf((char *)lbuffer_ptr, "0x%02x ", *ptr);
		lbuffer_ptr += ret;
		if ((i & 0x0F) == 0x0F) {
			ret = sprintf((char *)lbuffer_ptr, "\n");
			lbuffer_ptr += ret;
		}
	}
	if (size & 0x0F)
		sprintf((char *)lbuffer_ptr, "\n");
	netdev_info(ulp_ctx->bp->dev, "%s", lbuffer);

	vfree(lbuffer);
}

/* Utility Function to dump the computed field properties */
static void ulp_parser_comp_field_dump(struct ulp_tc_parser_params *params,
				       const char *field_names[],
				       u32 count_list)
{
	u32 idx = 0;

	netdev_info(params->ulp_ctx->bp->dev, "Default computed fields\n");
	for (idx = 0; idx < count_list; idx++) {
		netdev_info(params->ulp_ctx->bp->dev, "\t%s =\n",
			    field_names[idx]);
		dump_hex(params->ulp_ctx, (u8 *)&params->comp_fld[idx],
			 sizeof(u64));
	}
}

/* Utility Function to dump the field properties.*/
static void ulp_parser_field_dump(struct bnxt_ulp_context *ulp_ctx,
				  struct ulp_tc_hdr_field  *hdr_field,
				  const char *field_names[],
				  u32 start_idx, u32 count_list)
{
	u32 f_idx = 0, idx = 0;

	for (f_idx = start_idx; f_idx < (start_idx + count_list); f_idx++) {
		if (hdr_field[f_idx].size) {
			netdev_info(ulp_ctx->bp->dev, "\t%s = %d\n",
				    field_names[idx], f_idx);
			dump_hex(ulp_ctx, hdr_field[f_idx].spec,
				 hdr_field[f_idx].size);
			dump_hex(ulp_ctx, hdr_field[f_idx].mask,
				 hdr_field[f_idx].size);
		}
		idx++;
	}
}

/* Utility Function to dump the field properties.*/
static inline void ulp_parser_vlan_dump(struct bnxt_ulp_context *ulp_ctx,
					struct ulp_tc_hdr_field *hdr_field,
					u32 f_idx)
{
	ulp_parser_field_dump(ulp_ctx, hdr_field, ulp_tc_hdr_vlan_field_names,
			      f_idx, BNXT_ULP_PROTO_HDR_S_VLAN_NUM);
}

/* Function to dump the Pattern header bitmaps and fields. */
void ulp_parser_hdr_info_dump(struct ulp_tc_parser_params *params)
{
	struct ulp_tc_hdr_bitmap *hdr_bitmap = &params->hdr_bitmap;
	struct ulp_tc_hdr_field *hdr_field = params->hdr_field;
	struct bnxt_ulp_context *ulp_ctx = params->ulp_ctx;
	u32 idx = 0, f_idx = 0;
	u32 num_idx;
	u64 hdr_bit;

	netdev_info(ulp_ctx->bp->dev,
		    "Configured Header Protocols for matching\n");
	if (ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_FLOW_DIR_BITMASK_EGR))
		netdev_info(ulp_ctx->bp->dev, "It is a Egress Flow - %x\n",
			    params->dir_attr);
	else
		netdev_info(ulp_ctx->bp->dev, "It is a Ingress Flow - %x\n",
			    params->dir_attr);
	ulp_parser_comp_field_dump(params, ulp_tc_hdr_comp_field_names,
				   BNXT_ULP_CF_IDX_LAST);

	num_idx = sizeof(bnxt_ulp_hdr_bit_names) /
		sizeof(bnxt_ulp_hdr_bit_names[0]);

	/* Print the svif details, there is no bitmap for this field */
	ulp_parser_field_dump(params->ulp_ctx,
			      hdr_field, ulp_tc_hdr_svif_names, f_idx,
			      BNXT_ULP_PROTO_HDR_SVIF_NUM);
	f_idx += BNXT_ULP_PROTO_HDR_SVIF_NUM;

	for (idx = 0; idx < num_idx; idx++) {
		hdr_bit = 1UL << idx;
		if (!ULP_BITMAP_ISSET(hdr_bitmap->bits, hdr_bit))
			continue;

		netdev_info(params->ulp_ctx->bp->dev, "%s\n",
			    bnxt_ulp_hdr_bit_names[idx]);
		if (ULP_BITMAP_ISSET(hdr_bit, BNXT_ULP_HDR_BIT_O_ETH)) {
			ulp_parser_field_dump(ulp_ctx, hdr_field,
					      ulp_tc_hdr_eth_field_names,
					      f_idx,
					      BNXT_ULP_PROTO_HDR_ETH_NUM);
			f_idx += BNXT_ULP_PROTO_HDR_ETH_NUM;
		} else if (ULP_BITMAP_ISSET(hdr_bit, BNXT_ULP_HDR_BIT_I_ETH)) {
			ulp_parser_field_dump(ulp_ctx, hdr_field,
					      ulp_tc_hdr_eth_field_names,
					      f_idx,
					      BNXT_ULP_PROTO_HDR_ETH_NUM);
			f_idx += BNXT_ULP_PROTO_HDR_ETH_NUM;
		} else if (ULP_BITMAP_ISSET(hdr_bit,
					    BNXT_ULP_HDR_BIT_OO_VLAN)) {
			ulp_parser_vlan_dump(ulp_ctx, hdr_field, f_idx);
			f_idx += BNXT_ULP_PROTO_HDR_S_VLAN_NUM;
		} else if (ULP_BITMAP_ISSET(hdr_bit,
					    BNXT_ULP_HDR_BIT_OI_VLAN)) {
			ulp_parser_vlan_dump(ulp_ctx, hdr_field, f_idx);
			f_idx += BNXT_ULP_PROTO_HDR_S_VLAN_NUM;
		} else if (ULP_BITMAP_ISSET(hdr_bit,
					    BNXT_ULP_HDR_BIT_IO_VLAN)) {
			ulp_parser_vlan_dump(ulp_ctx, hdr_field, f_idx);
			f_idx += BNXT_ULP_PROTO_HDR_S_VLAN_NUM;
		} else if (ULP_BITMAP_ISSET(hdr_bit,
					    BNXT_ULP_HDR_BIT_II_VLAN)) {
			ulp_parser_vlan_dump(ulp_ctx, hdr_field, f_idx);
			f_idx += BNXT_ULP_PROTO_HDR_S_VLAN_NUM;
		} else if (ULP_BITMAP_ISSET(hdr_bit, BNXT_ULP_HDR_BIT_O_IPV4) ||
			   ULP_BITMAP_ISSET(hdr_bit, BNXT_ULP_HDR_BIT_I_IPV4)) {
			ulp_parser_field_dump(ulp_ctx, hdr_field,
					      ulp_tc_hdr_ipv4_field_names,
					      f_idx,
					      BNXT_ULP_PROTO_HDR_IPV4_NUM);
			f_idx += BNXT_ULP_PROTO_HDR_IPV4_NUM;
		} else if (ULP_BITMAP_ISSET(hdr_bit, BNXT_ULP_HDR_BIT_O_IPV6) ||
			   ULP_BITMAP_ISSET(hdr_bit, BNXT_ULP_HDR_BIT_I_IPV6)) {
			ulp_parser_field_dump(ulp_ctx, hdr_field,
					      ulp_tc_hdr_ipv6_field_names,
					      f_idx,
					      BNXT_ULP_PROTO_HDR_IPV6_NUM);
			f_idx += BNXT_ULP_PROTO_HDR_IPV6_NUM;
		} else if (ULP_BITMAP_ISSET(hdr_bit, BNXT_ULP_HDR_BIT_O_UDP) ||
			   ULP_BITMAP_ISSET(hdr_bit, BNXT_ULP_HDR_BIT_I_UDP)) {
			ulp_parser_field_dump(ulp_ctx, hdr_field,
					      ulp_tc_hdr_udp_field_names,
					      f_idx,
					      BNXT_ULP_PROTO_HDR_UDP_NUM);
			f_idx += BNXT_ULP_PROTO_HDR_UDP_NUM;
		} else if (ULP_BITMAP_ISSET(hdr_bit, BNXT_ULP_HDR_BIT_O_BTH)) {
			ulp_parser_field_dump(ulp_ctx, hdr_field,
					      ulp_tc_hdr_bth_field_names,
					      f_idx,
					      BNXT_ULP_PROTO_HDR_BTH_NUM);
			f_idx += BNXT_ULP_PROTO_HDR_BTH_NUM;
		} else if (ULP_BITMAP_ISSET(hdr_bit, BNXT_ULP_HDR_BIT_O_TCP) ||
			   ULP_BITMAP_ISSET(hdr_bit, BNXT_ULP_HDR_BIT_I_TCP)) {
			ulp_parser_field_dump(ulp_ctx, hdr_field,
					      ulp_tc_hdr_tcp_field_names,
					      f_idx,
					      BNXT_ULP_PROTO_HDR_TCP_NUM);
			f_idx += BNXT_ULP_PROTO_HDR_TCP_NUM;
		} else if (ULP_BITMAP_ISSET(hdr_bit,
					    BNXT_ULP_HDR_BIT_T_VXLAN)) {
			ulp_parser_field_dump(ulp_ctx, hdr_field,
					      ulp_tc_hdr_vxlan_field_names,
					      f_idx,
					      BNXT_ULP_PROTO_HDR_VXLAN_NUM);
			f_idx += BNXT_ULP_PROTO_HDR_VXLAN_NUM;
		} else if (ULP_BITMAP_ISSET(hdr_bit,
					    BNXT_ULP_HDR_BIT_T_IFA)) {
			ulp_parser_field_dump(ulp_ctx, hdr_field,
					      ulp_tc_hdr_ifa_field_names,
					      f_idx,
					      BNXT_ULP_PROTO_HDR_IFA_NUM);
			f_idx += BNXT_ULP_PROTO_HDR_IFA_NUM;
		} else if (ULP_BITMAP_ISSET(hdr_bit, BNXT_ULP_HDR_BIT_O_ICMP) ||
			   ULP_BITMAP_ISSET(hdr_bit, BNXT_ULP_HDR_BIT_I_ICMP)) {
			ulp_parser_field_dump(ulp_ctx, hdr_field,
					      ulp_tc_hdr_icmp_field_names,
					      f_idx,
					      BNXT_ULP_PROTO_HDR_ICMP_NUM);
			f_idx += BNXT_ULP_PROTO_HDR_ICMP_NUM;
		} else if (ULP_BITMAP_ISSET(hdr_bit, BNXT_ULP_HDR_BIT_O_ECPRI)) {
			ulp_parser_field_dump(ulp_ctx, hdr_field,
					      ulp_tc_hdr_ecpri_field_names,
					      f_idx,
					      BNXT_ULP_PROTO_HDR_ECPRI_NUM);
			f_idx += BNXT_ULP_PROTO_HDR_ECPRI_NUM;
		}
	}
	netdev_info(ulp_ctx->bp->dev, "*************************************\n");
}

static void ulp_parser_action_prop_dump(struct bnxt_ulp_context *ulp_ctx,
					struct ulp_tc_act_prop	*act_prop,
					u32 start_idx, u32 dump_size)
{
	netdev_info(ulp_ctx->bp->dev, "\t%s =\n",
		    bnxt_ulp_tc_parser_action_prop_names[start_idx]);
	dump_hex(ulp_ctx, &act_prop->act_details[start_idx], dump_size);
}

/* Function to dump the Action header bitmaps and properties. */
void ulp_parser_act_info_dump(struct ulp_tc_parser_params *params)
{
	struct ulp_tc_hdr_bitmap *act_bitmap = &params->act_bitmap;
	struct ulp_tc_act_prop *act_prop = &params->act_prop;
	u32 num_idx = 0;
	u32 idx = 0;
	u64 act_bit;

	netdev_info(params->ulp_ctx->bp->dev,
		    "Configured actions for matching\n");
	netdev_info(params->ulp_ctx->bp->dev, "Default computed fields\n");
	ulp_parser_action_prop_dump(params->ulp_ctx, act_prop,
				    BNXT_ULP_ACT_PROP_IDX_ENCAP_TUN_SZ,
				    BNXT_ULP_ACT_PROP_SZ_ENCAP_TUN_SZ);
	ulp_parser_action_prop_dump(params->ulp_ctx, act_prop,
				    BNXT_ULP_ACT_PROP_IDX_ENCAP_IP_SZ,
				    BNXT_ULP_ACT_PROP_SZ_ENCAP_IP_SZ);
	ulp_parser_action_prop_dump(params->ulp_ctx, act_prop,
				    BNXT_ULP_ACT_PROP_IDX_ENCAP_VTAG_SZ,
				    BNXT_ULP_ACT_PROP_SZ_ENCAP_VTAG_SZ);
	ulp_parser_action_prop_dump(params->ulp_ctx, act_prop,
				    BNXT_ULP_ACT_PROP_IDX_ENCAP_VTAG_TYPE,
				    BNXT_ULP_ACT_PROP_SZ_ENCAP_VTAG_TYPE);
	ulp_parser_action_prop_dump(params->ulp_ctx, act_prop,
				    BNXT_ULP_ACT_PROP_IDX_ENCAP_VTAG_NUM,
				    BNXT_ULP_ACT_PROP_SZ_ENCAP_VTAG_NUM);
	ulp_parser_action_prop_dump(params->ulp_ctx, act_prop,
				    BNXT_ULP_ACT_PROP_IDX_ENCAP_L3_TYPE,
				    BNXT_ULP_ACT_PROP_SZ_ENCAP_L3_TYPE);
	ulp_parser_action_prop_dump(params->ulp_ctx, act_prop,
				    BNXT_ULP_ACT_PROP_IDX_VNIC,
				    BNXT_ULP_ACT_PROP_SZ_VNIC);
	ulp_parser_action_prop_dump(params->ulp_ctx, act_prop,
				    BNXT_ULP_ACT_PROP_IDX_VPORT,
				    BNXT_ULP_ACT_PROP_SZ_VPORT);

	num_idx = sizeof(bnxt_ulp_action_bit_names) /
		 sizeof(bnxt_ulp_action_bit_names[0]);

	for (idx = 0; idx < num_idx; idx++) {
		enum bnxt_ulp_act_prop_idx	tmp_act_p;
		enum bnxt_ulp_act_prop_sz	tmp_act_sz;

		act_bit = 1UL << idx;
		if (!ULP_BITMAP_ISSET(act_bitmap->bits, act_bit))
			continue;

		netdev_info(params->ulp_ctx->bp->dev, "%s\n",
			    bnxt_ulp_action_bit_names[idx]);
		if (ULP_BITMAP_ISSET(act_bit, BNXT_ULP_ACT_BIT_MARK)) {
			ulp_parser_action_prop_dump(params->ulp_ctx, act_prop,
						    BNXT_ULP_ACT_PROP_IDX_MARK,
						    BNXT_ULP_ACT_PROP_SZ_MARK);
		} else if (ULP_BITMAP_ISSET(act_bit,
					    BNXT_ULP_ACT_BIT_VXLAN_ENCAP)) {
			tmp_act_p = BNXT_ULP_ACT_PROP_IDX_ENCAP_L2_DMAC;
			tmp_act_sz = BNXT_ULP_ACT_PROP_IDX_LAST -
			    BNXT_ULP_ACT_PROP_IDX_ENCAP_L2_DMAC;
			netdev_info(params->ulp_ctx->bp->dev,
				    "size %d and %d\n", tmp_act_p, tmp_act_sz);
			tmp_act_sz = BNXT_ULP_ACT_PROP_SZ_ENCAP_L2_DMAC;
			ulp_parser_action_prop_dump(params->ulp_ctx, act_prop,
						    tmp_act_p, tmp_act_sz);
			tmp_act_p = BNXT_ULP_ACT_PROP_IDX_ENCAP_L2_SMAC;
			tmp_act_sz = BNXT_ULP_ACT_PROP_SZ_ENCAP_L2_SMAC;
			ulp_parser_action_prop_dump(params->ulp_ctx, act_prop,
						    tmp_act_p, tmp_act_sz);
			tmp_act_p = BNXT_ULP_ACT_PROP_IDX_ENCAP_VTAG;
			tmp_act_sz = BNXT_ULP_ACT_PROP_SZ_ENCAP_VTAG;
			ulp_parser_action_prop_dump(params->ulp_ctx, act_prop,
						    tmp_act_p, tmp_act_sz);
			tmp_act_p = BNXT_ULP_ACT_PROP_IDX_ENCAP_IP;
			tmp_act_sz = BNXT_ULP_ACT_PROP_SZ_ENCAP_IP;
			ulp_parser_action_prop_dump(params->ulp_ctx, act_prop,
						    tmp_act_p, tmp_act_sz);
			tmp_act_p = BNXT_ULP_ACT_PROP_IDX_ENCAP_IP_SRC;
			tmp_act_sz = BNXT_ULP_ACT_PROP_SZ_ENCAP_IP_SRC;
			ulp_parser_action_prop_dump(params->ulp_ctx, act_prop,
						    tmp_act_p, tmp_act_sz);
			tmp_act_p = BNXT_ULP_ACT_PROP_IDX_ENCAP_UDP;
			tmp_act_sz = BNXT_ULP_ACT_PROP_SZ_ENCAP_UDP;
			ulp_parser_action_prop_dump(params->ulp_ctx, act_prop,
						    tmp_act_p, tmp_act_sz);
			tmp_act_p = BNXT_ULP_ACT_PROP_IDX_ENCAP_TUN;
			tmp_act_sz = BNXT_ULP_ACT_PROP_SZ_ENCAP_TUN;
			ulp_parser_action_prop_dump(params->ulp_ctx, act_prop,
						    tmp_act_p, tmp_act_sz);
		} else if (ULP_BITMAP_ISSET(act_bit,
					    BNXT_ULP_ACT_BIT_COUNT)) {
			ulp_parser_action_prop_dump(params->ulp_ctx, act_prop,
						    BNXT_ULP_ACT_PROP_IDX_COUNT,
						    BNXT_ULP_ACT_PROP_SZ_COUNT);
		} else if (ULP_BITMAP_ISSET(act_bit,
					    BNXT_ULP_ACT_BIT_PUSH_VLAN)) {
			tmp_act_p = BNXT_ULP_ACT_PROP_IDX_PUSH_VLAN;
			tmp_act_sz = BNXT_ULP_ACT_PROP_SZ_PUSH_VLAN;
			ulp_parser_action_prop_dump(params->ulp_ctx, act_prop,
						    tmp_act_p,
						    tmp_act_sz);
		} else if (ULP_BITMAP_ISSET(act_bit,
					    BNXT_ULP_ACT_BIT_SET_IPV4_SRC)) {
			tmp_act_p = BNXT_ULP_ACT_PROP_IDX_SET_IPV4_SRC;
			tmp_act_sz = BNXT_ULP_ACT_PROP_SZ_SET_IPV4_SRC;
			ulp_parser_action_prop_dump(params->ulp_ctx, act_prop,
						    tmp_act_p,
						    tmp_act_sz);
		} else if (ULP_BITMAP_ISSET(act_bit,
					    BNXT_ULP_ACT_BIT_SET_IPV4_DST)) {
			tmp_act_p = BNXT_ULP_ACT_PROP_IDX_SET_IPV4_DST;
			tmp_act_sz = BNXT_ULP_ACT_PROP_SZ_SET_IPV4_DST;
			ulp_parser_action_prop_dump(params->ulp_ctx, act_prop,
						    tmp_act_p,
						    tmp_act_sz);
		} else if (ULP_BITMAP_ISSET(act_bit,
					    BNXT_ULP_ACT_BIT_SET_TP_SRC)) {
			tmp_act_p = BNXT_ULP_ACT_PROP_IDX_SET_TP_SRC;
			tmp_act_sz = BNXT_ULP_ACT_PROP_SZ_SET_TP_SRC;
			ulp_parser_action_prop_dump(params->ulp_ctx, act_prop,
						    tmp_act_p,
						    tmp_act_sz);
		} else if (ULP_BITMAP_ISSET(act_bit,
					    BNXT_ULP_ACT_BIT_SET_TP_DST)) {
			tmp_act_p = BNXT_ULP_ACT_PROP_IDX_SET_TP_DST;
			tmp_act_sz = BNXT_ULP_ACT_PROP_SZ_SET_TP_DST;
			ulp_parser_action_prop_dump(params->ulp_ctx, act_prop,
						    tmp_act_p,
						    tmp_act_sz);
		} else if (ULP_BITMAP_ISSET(act_bit,
					    BNXT_ULP_ACT_BIT_METER_PROFILE)) {
			tmp_act_p = BNXT_ULP_ACT_PROP_IDX_METER_PROF_ID;
			tmp_act_sz = BNXT_ULP_ACT_PROP_SZ_METER_PROF_ID;
			ulp_parser_action_prop_dump(params->ulp_ctx, act_prop,
						    tmp_act_p,
						    tmp_act_sz);
			tmp_act_p = BNXT_ULP_ACT_PROP_IDX_METER_PROF_CIR;
			tmp_act_sz = BNXT_ULP_ACT_PROP_SZ_METER_PROF_CIR;
			ulp_parser_action_prop_dump(params->ulp_ctx, act_prop,
						    tmp_act_p,
						    tmp_act_sz);
			tmp_act_p = BNXT_ULP_ACT_PROP_IDX_METER_PROF_EIR;
			tmp_act_sz = BNXT_ULP_ACT_PROP_SZ_METER_PROF_EIR;
			ulp_parser_action_prop_dump(params->ulp_ctx, act_prop,
						    tmp_act_p,
						    tmp_act_sz);
			tmp_act_p = BNXT_ULP_ACT_PROP_IDX_METER_PROF_CBS;
			tmp_act_sz = BNXT_ULP_ACT_PROP_SZ_METER_PROF_CBS;
			ulp_parser_action_prop_dump(params->ulp_ctx, act_prop,
						    tmp_act_p,
						    tmp_act_sz);
			tmp_act_p = BNXT_ULP_ACT_PROP_IDX_METER_PROF_EBS;
			tmp_act_sz = BNXT_ULP_ACT_PROP_SZ_METER_PROF_EBS;
			ulp_parser_action_prop_dump(params->ulp_ctx, act_prop,
						    tmp_act_p,
						    tmp_act_sz);
			tmp_act_p = BNXT_ULP_ACT_PROP_IDX_METER_PROF_RFC2698;
			tmp_act_sz = BNXT_ULP_ACT_PROP_SZ_METER_PROF_RFC2698;
			ulp_parser_action_prop_dump(params->ulp_ctx, act_prop,
						    tmp_act_p,
						    tmp_act_sz);
			tmp_act_p = BNXT_ULP_ACT_PROP_IDX_METER_PROF_PM;
			tmp_act_sz = BNXT_ULP_ACT_PROP_SZ_METER_PROF_PM;
			ulp_parser_action_prop_dump(params->ulp_ctx, act_prop,
						    tmp_act_p,
						    tmp_act_sz);
			tmp_act_p = BNXT_ULP_ACT_PROP_IDX_METER_PROF_EBND;
			tmp_act_sz = BNXT_ULP_ACT_PROP_SZ_METER_PROF_EBND;
			ulp_parser_action_prop_dump(params->ulp_ctx, act_prop,
						    tmp_act_p,
						    tmp_act_sz);
			tmp_act_p = BNXT_ULP_ACT_PROP_IDX_METER_PROF_CBND;
			tmp_act_sz = BNXT_ULP_ACT_PROP_SZ_METER_PROF_CBND;
			ulp_parser_action_prop_dump(params->ulp_ctx, act_prop,
						    tmp_act_p,
						    tmp_act_sz);
			tmp_act_p = BNXT_ULP_ACT_PROP_IDX_METER_PROF_EBSM;
			tmp_act_sz = BNXT_ULP_ACT_PROP_SZ_METER_PROF_EBSM;
			ulp_parser_action_prop_dump(params->ulp_ctx, act_prop,
						    tmp_act_p,
						    tmp_act_sz);
			tmp_act_p = BNXT_ULP_ACT_PROP_IDX_METER_PROF_CBSM;
			tmp_act_sz = BNXT_ULP_ACT_PROP_SZ_METER_PROF_CBSM;
			ulp_parser_action_prop_dump(params->ulp_ctx, act_prop,
						    tmp_act_p,
						    tmp_act_sz);
			tmp_act_p = BNXT_ULP_ACT_PROP_IDX_METER_PROF_CF;
			tmp_act_sz = BNXT_ULP_ACT_PROP_SZ_METER_PROF_CF;
			ulp_parser_action_prop_dump(params->ulp_ctx, act_prop,
						    tmp_act_p,
						    tmp_act_sz);
		} else if (ULP_BITMAP_ISSET(act_bit,
					    BNXT_ULP_ACT_BIT_SHARED_METER)) {
			tmp_act_p = BNXT_ULP_ACT_PROP_IDX_METER_PROF_ID;
			tmp_act_sz = BNXT_ULP_ACT_PROP_SZ_METER_PROF_ID;
			ulp_parser_action_prop_dump(params->ulp_ctx, act_prop,
						    tmp_act_p,
						    tmp_act_sz);
			tmp_act_p = BNXT_ULP_ACT_PROP_IDX_METER_INST_ID;
			tmp_act_sz = BNXT_ULP_ACT_PROP_SZ_METER_INST_ID;
			ulp_parser_action_prop_dump(params->ulp_ctx, act_prop,
						    tmp_act_p,
						    tmp_act_sz);
			tmp_act_p = BNXT_ULP_ACT_PROP_IDX_METER_INST_MTR_VAL;
			tmp_act_sz = BNXT_ULP_ACT_PROP_SZ_METER_INST_MTR_VAL;
			ulp_parser_action_prop_dump(params->ulp_ctx, act_prop,
						    tmp_act_p,
						    tmp_act_sz);
			tmp_act_p = BNXT_ULP_ACT_PROP_IDX_METER_INST_ECN_RMP_EN;
			tmp_act_sz = BNXT_ULP_ACT_PROP_SZ_METER_INST_ECN_RMP_EN;
			ulp_parser_action_prop_dump(params->ulp_ctx, act_prop,
						    tmp_act_p,
						    tmp_act_sz);

		}
	}
	netdev_info(params->ulp_ctx->bp->dev, "******************************************\n");
}

/* Function to dump the error field during matching. */
void ulp_matcher_act_field_dump(struct bnxt_ulp_context *ulp_ctx, u32 idx,
				u32 jdx, u32 mask_id)
{
	netdev_info(ulp_ctx->bp->dev, "Match failed template=%d,field=%s,mask=%s\n",
		    idx,
		    bnxt_ulp_tc_template_field_names[(jdx +
						      (idx + 1) * 1)],
		    bnxt_ulp_flow_matcher_field_mask_opcode_names[mask_id]);
}

/* Function to dump the blob during the mapper processing. */
void ulp_mapper_field_dump(struct bnxt_ulp_context *ulp_ctx, const char *name,
			   struct bnxt_ulp_mapper_field_info *fld,
			   struct ulp_blob *blob, u16 write_idx, u8 *val,
			   u32 field_size)
{
	u32 len = 0, slen = 0;
	u32 ret = 0, idx = 0;
	u8 *lbuffer_ptr;
	u8 lbuffer[64];

	lbuffer_ptr = lbuffer;

	if (!val || !blob)
		return;

	slen = field_size;
	if (slen % 8)
		len = (slen / 8) + 1;
	else
		len = (slen / 8);

	memset(lbuffer, 0, sizeof(lbuffer));
	while (len > 0 && idx < 32) {
		ret = sprintf((char *)lbuffer_ptr, "%02x", val[idx]);
		lbuffer_ptr += ret;
		len--;
		idx++;
	}

	netdev_info(ulp_ctx->bp->dev,
		    "%-16s %-20s, bits = %-3d and pos = %-3d val = 0x%s\n",
		   name, fld->description, slen, write_idx, lbuffer);
#ifdef TC_BNXT_TRUFLOW_DEBUG_DETAIL
	dump_hex(ulp_ctx, (u8 *)blob->data, (write_idx + slen + 7) / 8);
#endif
}

void ulp_mapper_ident_field_dump(struct bnxt_ulp_context *ulp_ctx,
				 const char *name,
				 struct bnxt_ulp_mapper_ident_info *ident,
				 struct bnxt_ulp_mapper_tbl_info *tbl,
				 int id)
{
	netdev_info(ulp_ctx->bp->dev, "%-16s alloc %-16s, dir= %s, id = 0x%x\n",
		    name, ident->description,
		    (tbl->direction == TF_DIR_RX) ? "RX" : "TX", id);
}

void ulp_mapper_tcam_entry_dump(struct bnxt_ulp_context *ulp_ctx,
				const char *name, u32 idx,
				struct bnxt_ulp_mapper_tbl_info *tbl,
				struct ulp_blob *key, struct ulp_blob *mask,
				struct ulp_blob *result)
{
	netdev_info(ulp_ctx->bp->dev, "%-16s [%s][0x%0x],keysz=%-3d resultsz=%-3d\n",
		    name,
		    (tbl->direction == TF_DIR_RX) ? "RX" : "TX",
		    idx, key->write_idx, result->write_idx);
	dump_hex(ulp_ctx, (u8 *)key->data, (key->bitlen + 7) / 8);
	dump_hex(ulp_ctx, (u8 *)mask->data, (key->bitlen + 7) / 8);
	dump_hex(ulp_ctx, (u8 *)result->data, (key->bitlen + 7) / 8);
}

void ulp_mapper_result_dump(struct bnxt_ulp_context *ulp_ctx, const char *name,
			    struct bnxt_ulp_mapper_tbl_info *tbl,
			    struct ulp_blob *result)
{
	netdev_info(ulp_ctx->bp->dev, "%-16s [%s], bitlen=%-3d\n",
		    name,
		    (tbl->direction == TF_DIR_RX) ? "RX" : "TX",
		    result->write_idx);
	dump_hex(ulp_ctx, (u8 *)result->data, (result->write_idx + 7) / 8);
}

void ulp_mapper_act_dump(struct bnxt_ulp_context *ulp_ctx, const char *name,
			 struct bnxt_ulp_mapper_tbl_info *tbl,
			 struct ulp_blob *data)
{
	netdev_info(ulp_ctx->bp->dev, "%-16s [%s], bitlen=%-3d\n",
		    name,
		    (tbl->direction == TF_DIR_RX) ? "RX" : "TX",
		    data->write_idx);
	dump_hex(ulp_ctx, (u8 *)data->data, (data->write_idx + 7) / 8);
}

void ulp_mapper_em_dump(struct bnxt_ulp_context *ulp_ctx, const char *name,
			struct ulp_blob *key, struct ulp_blob *data,
			struct tf_insert_em_entry_parms *iparms)
{
	netdev_info(ulp_ctx->bp->dev, "%s ins %s[%s] scope=0x%02x keysz=%d recsz=%d\n",
		    name,
		    (iparms->mem == TF_MEM_EXTERNAL) ? "EXT" : "INT",
		    (iparms->dir == TF_DIR_RX) ? "RX" : "TX",
		    iparms->tbl_scope_id,
		    iparms->key_sz_in_bits,
		    iparms->em_record_sz_in_bits);

	netdev_info(ulp_ctx->bp->dev, "FlowHdl= %llx FlowID= %llu\n",
		    iparms->flow_handle, iparms->flow_id);

	netdev_info(ulp_ctx->bp->dev, "Key Size %d, Data Size %d\n",
		    key->write_idx, data->write_idx);

	dump_hex(ulp_ctx, iparms->key, (key->write_idx + 7) / 8);
	dump_hex(ulp_ctx, iparms->em_record, (data->write_idx + 7) / 8);
}

void
ulp_mapper_tfc_em_dump(struct bnxt_ulp_context *ulp_ctx, const char *name,
		       struct ulp_blob *data,
		       struct tfc_em_insert_parms *iparms)
{
	netdev_info(ulp_ctx->bp->dev, "%s [%s] keysz=%u recsz=%u\n",
		    name,
		    (iparms->dir == CFA_DIR_RX) ? "RX" : "TX",
		    iparms->key_sz_bits,
		    iparms->lkup_key_sz_words);

	netdev_info(ulp_ctx->bp->dev, "FlowHdl=%llx\n", *iparms->flow_handle);

	dump_hex(ulp_ctx, data->data, (data->write_idx + 7) / 8);
}

void ulp_mapper_blob_dump(struct bnxt_ulp_context *ulp_ctx,
			  struct ulp_blob *blob)
{
	dump_hex(ulp_ctx, blob->data, (blob->write_idx + 7) / 8);
}

void ulp_mapper_table_dump(struct bnxt_ulp_context *ulp_ctx,
			   struct bnxt_ulp_mapper_tbl_info *tbl, u32 idx)
{
	const char *sub_type;

	if (tbl->resource_func == BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE)
		sub_type = ulp_mapper_res_index_names[tbl->resource_sub_type];
	else if (tbl->resource_func == BNXT_ULP_RESOURCE_FUNC_KEY_RECIPE_TABLE)
		sub_type =
			ulp_mapper_res_key_recipe_names[tbl->resource_sub_type];
	else if (tbl->resource_func == BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE)
		sub_type = ulp_mapper_res_generic_names[tbl->resource_sub_type];
	else if (tbl->resource_func ==
		BNXT_ULP_RESOURCE_FUNC_GLOBAL_REGISTER_TABLE)
		sub_type =
			ulp_mapper_res_ulp_global_names[tbl->resource_sub_type];
	else
		sub_type = tbl->description;
	netdev_info(ulp_ctx->bp->dev, "Processing table %-16s:%-16s: %u\n",
		    ulp_mapper_resource_func_names[tbl->resource_func],
		    sub_type, idx);
}

void ulp_mapper_gen_tbl_dump(struct bnxt_ulp_context *ulp_ctx, u32 sub_type,
			     u8 direction, struct ulp_blob *key)
{
	netdev_info(ulp_ctx->bp->dev, "Generic Tbl[%s][%s] - Dump Key\n",
		    ulp_mapper_res_generic_names[sub_type],
		    (direction == TF_DIR_RX) ? "RX" : "TX");
	ulp_mapper_blob_dump(ulp_ctx, key);
}

const char *
ulp_mapper_key_recipe_type_to_str(u32 sub_type)
{
	return ulp_mapper_res_key_recipe_names[sub_type];
}

void
ulp_mapper_global_register_tbl_dump(struct bnxt_ulp_context *ulp_ctx,
				    u32 sub_type, u16 port)
{
	if (port)
		netdev_dbg(ulp_ctx->bp->dev, "Global Register Tbl[%s] Set port %u\n",
			   ulp_mapper_res_ulp_global_names[sub_type], port);
	else
		netdev_dbg(ulp_ctx->bp->dev, "Global Register Tbl[%s] Reset default\n",
			   ulp_mapper_res_ulp_global_names[sub_type]);
}

#else /* TC_BNXT_TRUFLOW_DEBUG */

/* Function to dump the Pattern header bitmaps and fields. */
void ulp_parser_hdr_info_dump(struct ulp_tc_parser_params *params)
{
}

/* Function to dump the Action header bitmaps and properties. */
void ulp_parser_act_info_dump(struct ulp_tc_parser_params *params)
{
}

/* Function to dump the error field during matching. */
void ulp_matcher_act_field_dump(struct bnxt_ulp_context *ulp_ctx, u32 idx,
				u32 jdx, u32 mask_id)
{
}

/* Function to dump the blob during the mapper processing. */
void ulp_mapper_field_dump(struct bnxt_ulp_context *ulp_ctx, const char *name,
			   struct bnxt_ulp_mapper_field_info *fld,
			   struct ulp_blob *blob, u16 write_idx, u8 *val,
			   u32 field_size)
{
}

void ulp_mapper_ident_field_dump(struct bnxt_ulp_context *ulp_ctx,
				 const char *name,
				 struct bnxt_ulp_mapper_ident_info *ident,
				 struct bnxt_ulp_mapper_tbl_info *tbl, int id)
{
}

void ulp_mapper_tcam_entry_dump(struct bnxt_ulp_context *ulp_ctx,
				const char *name, u32 idx,
				struct bnxt_ulp_mapper_tbl_info *tbl,
				struct ulp_blob *key, struct ulp_blob *mask,
				struct ulp_blob *result)
{
}

void ulp_mapper_result_dump(struct bnxt_ulp_context *ulp_ctx, const char *name,
			    struct bnxt_ulp_mapper_tbl_info *tbl,
			    struct ulp_blob *result)
{
}

void ulp_mapper_act_dump(struct bnxt_ulp_context *ulp_ctx, const char *name,
			 struct bnxt_ulp_mapper_tbl_info *tbl,
			 struct ulp_blob *data)
{
}

void ulp_mapper_em_dump(struct bnxt_ulp_context *ulp_ctx, const char *name,
			struct ulp_blob *key, struct ulp_blob *data,
			struct tf_insert_em_entry_parms *iparms)
{
}

void ulp_mapper_tfc_em_dump(struct bnxt_ulp_context *ulp_ctx, const char *name,
			    struct ulp_blob *data,
			    struct tfc_em_insert_parms *iparms)
{
}

void ulp_mapper_blob_dump(struct bnxt_ulp_context *ulp_ctx,
			  struct ulp_blob *blob)
{
}

void ulp_mapper_table_dump(struct bnxt_ulp_context *ulp_ctx,
			   struct bnxt_ulp_mapper_tbl_info *tbl, u32 idx)
{
}

void ulp_mapper_gen_tbl_dump(struct bnxt_ulp_context *ulp_ctx, u32 sub_type,
			     u8 direction, struct ulp_blob *key)
{
}

const char *ulp_mapper_key_recipe_type_to_str(u32 sub_type)
{
	return NULL;
}

void
ulp_mapper_global_register_tbl_dump(struct bnxt_ulp_context *ulp_ctx,
				    u32 sub_type, u16 port)
{
}
#endif /* TC_BNXT_TRUFLOW_DEBUG */
#endif /* CONFIG_BNXT_FLOWER_OFFLOAD */
