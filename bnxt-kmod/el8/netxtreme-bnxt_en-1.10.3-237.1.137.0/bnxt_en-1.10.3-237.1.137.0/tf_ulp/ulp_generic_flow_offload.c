// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2023-2023 Broadcom
 * All rights reserved.
 */

#include <linux/vmalloc.h>
#include <linux/if_ether.h>

#include "bnxt_compat.h"
#include "bnxt_hsi.h"
#include "bnxt.h"
#include "bnxt_tfc.h"
#include "bnxt_tf_common.h"
#include "bnxt_nic_flow.h"
#include "bnxt_ulp_flow.h"
#include "ulp_tc_parser.h"
#include "ulp_matcher.h"
#include "ulp_flow_db.h"
#include "ulp_mapper.h"
#include "ulp_fc_mgr.h"
#include "ulp_port_db.h"
#include "ulp_template_debug_proto.h"
#include "ulp_generic_flow_offload.h"
#include "cfa_types.h"

#if defined(CONFIG_BNXT_FLOWER_OFFLOAD)

#define BNXT_ULP_GEN_UDP_PORT_VXLAN_MASK	0XFFFF

/* Utility function to validate field size*/
static int bnxt_ulp_gen_prsr_fld_size_validate(struct ulp_tc_parser_params
					       *params, u32 *idx, u32 size)
{
	if (params->field_idx + size >= BNXT_ULP_PROTO_HDR_MAX)
		return -EINVAL;

	*idx = params->field_idx;
	params->field_idx += size;
	return BNXT_TF_RC_SUCCESS;
}

/* Utility function to update the field_bitmap */
static void bnxt_ulp_gen_parser_field_bitmap_update(struct ulp_tc_parser_params
						    *params,
						    u32 idx,
						    enum bnxt_ulp_prsr_action
						    prsr_act)
{
	struct ulp_tc_hdr_field *field;

	field = &params->hdr_field[idx];
	if (ulp_bitmap_notzero(field->mask, field->size)) {
		ULP_INDEX_BITMAP_SET(params->fld_bitmap.bits, idx);
		if (!(prsr_act & ULP_PRSR_ACT_MATCH_IGNORE))
			ULP_INDEX_BITMAP_SET(params->fld_s_bitmap.bits, idx);
		/* Not exact match */
		if (!ulp_bitmap_is_ones(field->mask, field->size))
			ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_WC_MATCH,
					    1);
	} else {
		ULP_INDEX_BITMAP_RESET(params->fld_bitmap.bits, idx);
	}
}

/* Utility function to copy field spec and masks items */
static void bnxt_ulp_gen_prsr_fld_mask(struct ulp_tc_parser_params *params,
				       u32 *idx,
				       u32 size,
				       const void *spec_buff,
				       const void *mask_buff,
				       enum bnxt_ulp_prsr_action prsr_act)
{
	struct ulp_tc_hdr_field *field = &params->hdr_field[*idx];

	/* update the field size */
	field->size = size;

	/* copy the mask specifications only if mask is not null */
	if (!(prsr_act & ULP_PRSR_ACT_MASK_IGNORE) && mask_buff) {
		memcpy(field->mask, mask_buff, size);
		bnxt_ulp_gen_parser_field_bitmap_update(params, *idx, prsr_act);
	}

	/* copy the protocol specifications only if mask is not null */
	if (spec_buff && mask_buff && ulp_bitmap_notzero(mask_buff, size))
		memcpy(field->spec, spec_buff, size);

	/* Increment the index */
	*idx = *idx + 1;
}

/* Set the direction based on the source interface */
static inline void bnxt_ulp_gen_set_dir_attributes(struct bnxt *bp, struct ulp_tc_parser_params
						   *params, enum bnxt_ulp_gen_direction dir)
{
	/* Set the flow attributes. */
	if (dir == BNXT_ULP_GEN_RX)
		params->dir_attr |= BNXT_ULP_FLOW_ATTR_INGRESS;
	else
		params->dir_attr |= BNXT_ULP_FLOW_ATTR_EGRESS;
}

static void
bnxt_ulp_gen_init_cf_header_bitmap(struct bnxt_ulp_mapper_parms *params)
{
	uint64_t hdr_bits = 0;

	/* Remove the internal tunnel bits */
	hdr_bits = params->hdr_bitmap->bits;
	ULP_BITMAP_RESET(hdr_bits, BNXT_ULP_HDR_BIT_F2);

	/* Add untag bits */
	if (!ULP_BITMAP_ISSET(hdr_bits, BNXT_ULP_HDR_BIT_OO_VLAN))
		ULP_BITMAP_SET(hdr_bits, BNXT_ULP_HDR_BIT_OO_UNTAGGED);
	if (!ULP_BITMAP_ISSET(hdr_bits, BNXT_ULP_HDR_BIT_OI_VLAN))
		ULP_BITMAP_SET(hdr_bits, BNXT_ULP_HDR_BIT_OI_UNTAGGED);
	if (!ULP_BITMAP_ISSET(hdr_bits, BNXT_ULP_HDR_BIT_IO_VLAN))
		ULP_BITMAP_SET(hdr_bits, BNXT_ULP_HDR_BIT_IO_UNTAGGED);
	if (!ULP_BITMAP_ISSET(hdr_bits, BNXT_ULP_HDR_BIT_II_VLAN))
		ULP_BITMAP_SET(hdr_bits, BNXT_ULP_HDR_BIT_II_UNTAGGED);

	/* Add non-tunnel bit */
	if (!ULP_BITMAP_ISSET(params->cf_bitmap, BNXT_ULP_CF_BIT_IS_TUNNEL))
		ULP_BITMAP_SET(hdr_bits, BNXT_ULP_HDR_BIT_NON_TUNNEL);

	/* Add l2 only bit */
	if ((!ULP_BITMAP_ISSET(params->cf_bitmap, BNXT_ULP_CF_BIT_IS_TUNNEL) &&
	     !ULP_BITMAP_ISSET(hdr_bits, BNXT_ULP_HDR_BIT_O_IPV4) &&
	     !ULP_BITMAP_ISSET(hdr_bits, BNXT_ULP_HDR_BIT_O_IPV6)) ||
	    (ULP_BITMAP_ISSET(params->cf_bitmap, BNXT_ULP_CF_BIT_IS_TUNNEL) &&
	     !ULP_BITMAP_ISSET(hdr_bits, BNXT_ULP_HDR_BIT_I_IPV4) &&
	     !ULP_BITMAP_ISSET(hdr_bits, BNXT_ULP_HDR_BIT_I_IPV6))) {
		ULP_BITMAP_SET(hdr_bits, BNXT_ULP_HDR_BIT_L2_ONLY);
		ULP_BITMAP_SET(params->cf_bitmap, BNXT_ULP_CF_BIT_L2_ONLY);
	}

	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_PROFILE_BITMAP, hdr_bits);

	/* Update the l4 protocol bits */
	if ((ULP_BITMAP_ISSET(hdr_bits, BNXT_ULP_HDR_BIT_O_TCP) ||
	     ULP_BITMAP_ISSET(hdr_bits, BNXT_ULP_HDR_BIT_O_UDP) ||
	     ULP_BITMAP_ISSET(hdr_bits, BNXT_ULP_HDR_BIT_O_BTH))) {
		ULP_BITMAP_RESET(hdr_bits, BNXT_ULP_HDR_BIT_O_TCP);
		ULP_BITMAP_RESET(hdr_bits, BNXT_ULP_HDR_BIT_O_UDP);
		ULP_BITMAP_RESET(hdr_bits, BNXT_ULP_HDR_BIT_O_BTH);
		ULP_BITMAP_SET(hdr_bits, BNXT_ULP_HDR_BIT_O_L4_FLOW);
	}

	if ((ULP_BITMAP_ISSET(hdr_bits, BNXT_ULP_HDR_BIT_I_TCP) ||
	     ULP_BITMAP_ISSET(hdr_bits, BNXT_ULP_HDR_BIT_I_UDP) ||
	     ULP_BITMAP_ISSET(hdr_bits, BNXT_ULP_HDR_BIT_I_BTH))) {
		ULP_BITMAP_RESET(hdr_bits, BNXT_ULP_HDR_BIT_I_TCP);
		ULP_BITMAP_RESET(hdr_bits, BNXT_ULP_HDR_BIT_I_UDP);
		ULP_BITMAP_RESET(hdr_bits, BNXT_ULP_HDR_BIT_I_BTH);
		ULP_BITMAP_SET(hdr_bits, BNXT_ULP_HDR_BIT_I_L4_FLOW);
	}

	/*update the comp field header bits */
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_HDR_BITMAP, hdr_bits);
}

static void
bnxt_ulp_gen_init_mapper_params(struct bnxt_ulp_mapper_parms *mparms,
				struct ulp_tc_parser_params *params,
				enum bnxt_ulp_fdb_type flow_type)
{
	u32 ulp_flags = 0;

	memset(mparms, 0, sizeof(*mparms));

	mparms->flow_type = flow_type;
	mparms->ulp_ctx = params->ulp_ctx;
	mparms->app_priority = params->priority;
	mparms->class_tid = params->class_id;
	mparms->act_tid = params->act_tmpl;
	mparms->func_id = params->func_id;
	mparms->hdr_bitmap = &params->hdr_bitmap;
	mparms->enc_hdr_bitmap = &params->enc_hdr_bitmap;
	mparms->hdr_field = params->hdr_field;
	mparms->enc_field = params->enc_field;
	mparms->comp_fld = params->comp_fld;
	mparms->act_bitmap = &params->act_bitmap;
	mparms->act_prop = &params->act_prop;
	mparms->flow_id = params->fid;
	mparms->fld_bitmap = &params->fld_bitmap;
	mparms->flow_pattern_id = params->flow_pattern_id;
	mparms->act_pattern_id = params->act_pattern_id;
	mparms->wc_field_bitmap = params->wc_field_bitmap;
	mparms->app_id = params->app_id;
	mparms->tun_idx = params->tun_idx;
	mparms->cf_bitmap = params->cf_bitmap;
	mparms->exclude_field_bitmap = params->exclude_field_bitmap;

	/* update the signature fields into the computed field list */
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_HDR_SIG_ID,
			    params->class_info_idx);

	/* update the header bitmap */
	bnxt_ulp_gen_init_cf_header_bitmap(mparms);

	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_FLOW_SIG_ID,
			    params->flow_sig_id);
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_FUNCTION_ID,
			    params->func_id);

	if (bnxt_ulp_cntxt_ptr2_ulp_flags_get(params->ulp_ctx, &ulp_flags))
		return;

	/* Update the socket direct flag */
	if (ULP_BITMAP_ISSET(params->hdr_bitmap.bits,
			     BNXT_ULP_HDR_BIT_SVIF_IGNORE)) {
		uint32_t ifindex;
		uint16_t vport;

		/* Get the port db ifindex */
		if (ulp_port_db_dev_port_to_ulp_index(params->ulp_ctx,
						      params->port_id,
						      &ifindex)) {
			netdev_dbg(params->ulp_ctx->bp->dev, "Invalid port id %u\n",
				   params->port_id);
			return;
		}
		/* Update the phy port of the other interface */
		if (ulp_port_db_vport_get(params->ulp_ctx, ifindex, &vport)) {
			netdev_dbg(params->ulp_ctx->bp->dev, "Invalid port if index %u\n",
				   ifindex);
			return;
		}
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_SOCKET_DIRECT_VPORT,
				    (vport == 1) ? 2 : 1);
	}
}

/* Function to handle the update of proto header based on field values */
static void bnxt_ulp_gen_l2_proto_type_update(struct ulp_tc_parser_params
					      *param, u16 type, u32 in_flag)
{
	if (type == cpu_to_be16(ETH_P_IP)) {
		if (in_flag) {
			ULP_BITMAP_SET(param->hdr_fp_bit.bits,
				       BNXT_ULP_HDR_BIT_I_IPV4);
			ULP_COMP_FLD_IDX_WR(param, BNXT_ULP_CF_IDX_I_L3, 1);
		} else {
			ULP_BITMAP_SET(param->hdr_fp_bit.bits,
				       BNXT_ULP_HDR_BIT_O_IPV4);
			ULP_COMP_FLD_IDX_WR(param, BNXT_ULP_CF_IDX_O_L3, 1);
		}
	} else if (type == cpu_to_be16(ETH_P_IPV6)) {
		if (in_flag) {
			ULP_BITMAP_SET(param->hdr_fp_bit.bits,
				       BNXT_ULP_HDR_BIT_I_IPV6);
			ULP_COMP_FLD_IDX_WR(param, BNXT_ULP_CF_IDX_I_L3, 1);
		} else {
			ULP_BITMAP_SET(param->hdr_fp_bit.bits,
				       BNXT_ULP_HDR_BIT_O_IPV6);
			ULP_COMP_FLD_IDX_WR(param, BNXT_ULP_CF_IDX_O_L3, 1);
		}
	}
}

/* Function to handle the update of proto header based on field values */
static void bnxt_ulp_gen_l3_proto_type_update(struct ulp_tc_parser_params
					      *param, u8 proto, u32 in_flag)
{
	if (proto == IPPROTO_UDP) {
		if (in_flag) {
			ULP_BITMAP_SET(param->hdr_fp_bit.bits,
				       BNXT_ULP_HDR_BIT_I_UDP);
			ULP_COMP_FLD_IDX_WR(param, BNXT_ULP_CF_IDX_I_L4, 1);
		} else {
			ULP_BITMAP_SET(param->hdr_fp_bit.bits,
				       BNXT_ULP_HDR_BIT_O_UDP);
			ULP_COMP_FLD_IDX_WR(param, BNXT_ULP_CF_IDX_O_L4, 1);
		}
	} else if (proto == IPPROTO_TCP) {
		if (in_flag) {
			ULP_BITMAP_SET(param->hdr_fp_bit.bits,
				       BNXT_ULP_HDR_BIT_I_TCP);
			ULP_COMP_FLD_IDX_WR(param, BNXT_ULP_CF_IDX_I_L4, 1);
		} else {
			ULP_BITMAP_SET(param->hdr_fp_bit.bits,
				       BNXT_ULP_HDR_BIT_O_TCP);
			ULP_COMP_FLD_IDX_WR(param, BNXT_ULP_CF_IDX_O_L4, 1);
		}
	} else if (proto == IPPROTO_GRE) {
		ULP_BITMAP_SET(param->hdr_bitmap.bits, BNXT_ULP_HDR_BIT_T_GRE);
	} else if (proto == IPPROTO_ICMP) {
		if (ULP_COMP_FLD_IDX_RD(param, BNXT_ULP_CF_IDX_L3_TUN))
			ULP_BITMAP_SET(param->hdr_bitmap.bits,
				       BNXT_ULP_HDR_BIT_I_ICMP);
		else
			ULP_BITMAP_SET(param->hdr_bitmap.bits,
				       BNXT_ULP_HDR_BIT_O_ICMP);
	}
	if (proto) {
		if (in_flag) {
			ULP_COMP_FLD_IDX_WR(param,
					    BNXT_ULP_CF_IDX_I_L3_FB_PROTO_ID,
					    1);
			ULP_COMP_FLD_IDX_WR(param,
					    BNXT_ULP_CF_IDX_I_L3_PROTO_ID,
					    proto);
		} else {
			ULP_COMP_FLD_IDX_WR(param,
					    BNXT_ULP_CF_IDX_O_L3_FB_PROTO_ID,
					    1);
			ULP_COMP_FLD_IDX_WR(param,
					    BNXT_ULP_CF_IDX_O_L3_PROTO_ID,
					    proto);
		}
	}
}

static int bnxt_ulp_gen_l2_l2_handler(struct bnxt *bp,
				      struct ulp_tc_parser_params *params,
				      struct bnxt_ulp_gen_eth_hdr *eth_spec,
				      struct bnxt_ulp_gen_eth_hdr *eth_mask)
{
	u32 idx = 0, dmac_idx = 0;
	u32 inner_flag = 0;
	u32 size;
	u16 eth_type = 0;

	if (eth_spec && eth_spec->type) {
		/* TODO: Perform validations BC, MC etc. */
		eth_type = *eth_spec->type;
	}
	if (eth_mask && eth_mask->type) {
		/* TODO: Perform validations BC, MC etc. */
		eth_type &= *eth_mask->type;
	}

	if (bnxt_ulp_gen_prsr_fld_size_validate(params, &idx,
						BNXT_ULP_PROTO_HDR_ETH_NUM)) {
		netdev_dbg(bp->dev, "Error parsing protocol header\n");
		return BNXT_TF_RC_ERROR;
	}
	/* Copy the flow for eth into hdr_field */
	dmac_idx = idx;
	size = ETH_ALEN;
	bnxt_ulp_gen_prsr_fld_mask(params, &idx, size,
				   (eth_spec) ? eth_spec->dst : NULL,
				   (eth_mask) ? eth_mask->dst : NULL,
				   ULP_PRSR_ACT_DEFAULT);

	size = ETH_ALEN;
	bnxt_ulp_gen_prsr_fld_mask(params, &idx, size,
				   (eth_spec) ? eth_spec->src : NULL,
				   (eth_mask) ? eth_mask->src : NULL,
				   ULP_PRSR_ACT_DEFAULT);

	size = sizeof(*eth_spec->type);
	bnxt_ulp_gen_prsr_fld_mask(params, &idx, size,
				   (eth_spec) ? eth_spec->type : NULL,
				   (eth_mask) ? eth_mask->type : NULL,
				   ULP_PRSR_ACT_DEFAULT);

	/* Update the protocol hdr bitmap */
	if (ULP_BITMAP_ISSET(params->hdr_bitmap.bits,
			     BNXT_ULP_HDR_BIT_O_ETH) ||
	    ULP_BITMAP_ISSET(params->hdr_bitmap.bits,
			     BNXT_ULP_HDR_BIT_O_IPV4) ||
	    ULP_BITMAP_ISSET(params->hdr_bitmap.bits,
			     BNXT_ULP_HDR_BIT_O_IPV6) ||
	    ULP_BITMAP_ISSET(params->hdr_bitmap.bits,
			     BNXT_ULP_HDR_BIT_O_UDP) ||
	    ULP_BITMAP_ISSET(params->hdr_bitmap.bits, BNXT_ULP_HDR_BIT_O_TCP)) {
		ULP_BITMAP_SET(params->hdr_bitmap.bits, BNXT_ULP_HDR_BIT_I_ETH);
		inner_flag = 1;
	} else {
		ULP_BITMAP_SET(params->hdr_bitmap.bits, BNXT_ULP_HDR_BIT_O_ETH);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_TUN_OFF_DMAC_ID,
				    dmac_idx);
	}

	/* Update the field protocol hdr bitmap */
	bnxt_ulp_gen_l2_proto_type_update(params, eth_type, inner_flag);

	return BNXT_TF_RC_SUCCESS;
}

static int bnxt_ulp_gen_l2_handler(struct bnxt *bp,
				   struct ulp_tc_parser_params *params,
				   struct bnxt_ulp_gen_l2_hdr_parms *parms)
{
	if (!parms) {
		netdev_dbg(bp->dev, "ERR: Nothing to do for L2\n");
		return BNXT_TF_RC_ERROR;
	}

	if (parms->type == BNXT_ULP_GEN_L2_L2_HDR)
		return bnxt_ulp_gen_l2_l2_handler(bp,
						  params,
						  parms->eth_spec,
						  parms->eth_mask);

	if (parms->type == BNXT_ULP_GEN_L2_L2_FILTER_ID)
		netdev_dbg(bp->dev, "ERR: L2_FILTER_ID unsupported\n");

	return BNXT_TF_RC_PARSE_ERR;
}

static int bnxt_ulp_gen_l3_v6_handler(struct bnxt *bp,
				      struct ulp_tc_parser_params *params,
				      struct bnxt_ulp_gen_ipv6_hdr *ipv6_spec,
				      struct bnxt_ulp_gen_ipv6_hdr *ipv6_mask)
{
	struct ulp_tc_hdr_bitmap *hdr_bitmap = &params->hdr_bitmap;
	u32 ver_spec = 0, ver_mask = 0;
	u32 lab_spec = 0, lab_mask = 0;
	u32 tc_spec = 0, tc_mask = 0;
	u32 idx = 0, dip_idx = 0;
	u32 size, vtc_flow;
	u32 inner_flag = 0;
	u8 proto_mask = 0;
	u8 proto = 0;
	u32 cnt;

	/* validate there is no 3rd L3 header */
	cnt = ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_L3_HDR_CNT);
	if (cnt == 2) {
		netdev_dbg(bp->dev,
			   "Parse Err:Third L3 header not supported\n");
		return BNXT_TF_RC_ERROR;
	}

	if (bnxt_ulp_gen_prsr_fld_size_validate(params, &idx,
						BNXT_ULP_PROTO_HDR_IPV6_NUM)) {
		netdev_dbg(bp->dev, "Error parsing protocol header\n");
		return BNXT_TF_RC_ERROR;
	}

	if (ipv6_spec) {
		vtc_flow = (ipv6_spec->vtc_flow) ? *ipv6_spec->vtc_flow : 0;
		ver_spec = (BNXT_ULP_GET_IPV6_VER(vtc_flow));
		tc_spec = (BNXT_ULP_GET_IPV6_TC(vtc_flow));
		lab_spec = (BNXT_ULP_GET_IPV6_FLOWLABEL(vtc_flow));
		proto = (ipv6_spec->proto6) ? *ipv6_spec->proto6 : 0;
	}

	if (ipv6_mask) {
		vtc_flow = (ipv6_mask->vtc_flow) ? *(ipv6_mask->vtc_flow) : 0;
		ver_mask = (BNXT_ULP_GET_IPV6_VER(vtc_flow));
		tc_mask = (BNXT_ULP_GET_IPV6_TC(vtc_flow));
		lab_mask = (BNXT_ULP_GET_IPV6_FLOWLABEL(vtc_flow));

		proto_mask = (ipv6_mask->proto6) ? *ipv6_mask->proto6 : 0;
		proto &= proto_mask;
	}

	/* version */
	size = sizeof(*ipv6_spec->vtc_flow);
	bnxt_ulp_gen_prsr_fld_mask(params, &idx, size, &ver_spec, &ver_mask,
				   ULP_PRSR_ACT_DEFAULT);

	/* traffic class */
	bnxt_ulp_gen_prsr_fld_mask(params, &idx, size, &tc_spec, &tc_mask,
				   ULP_PRSR_ACT_DEFAULT);

	/* flow label: Ignore for matching templates */
	bnxt_ulp_gen_prsr_fld_mask(params, &idx, size, &lab_spec, &lab_mask,
				   ULP_PRSR_ACT_MASK_IGNORE);

	/* payload length */
	size = sizeof(*ipv6_spec->payload_len);
	bnxt_ulp_gen_prsr_fld_mask(params, &idx, size,
				   (ipv6_spec) ? ipv6_spec->payload_len : NULL,
				   (ipv6_mask) ? ipv6_mask->payload_len : NULL,
				   ULP_PRSR_ACT_DEFAULT);

	/* next_proto_id: Ignore proto for matching templates */
	size = sizeof(*ipv6_spec->proto6);
	bnxt_ulp_gen_prsr_fld_mask(params, &idx, size,
				   (ipv6_spec) ? ipv6_spec->proto6 : NULL,
				   (ipv6_mask) ? ipv6_mask->proto6 : NULL,
				   ULP_PRSR_ACT_DEFAULT);

	/* hop limit (ttl) */
	size = sizeof(*ipv6_spec->hop_limits);
	bnxt_ulp_gen_prsr_fld_mask(params, &idx, size,
				   (ipv6_spec) ? ipv6_spec->hop_limits : NULL,
				   (ipv6_mask) ? ipv6_mask->hop_limits : NULL,
				   ULP_PRSR_ACT_DEFAULT);

	size = 16;
	bnxt_ulp_gen_prsr_fld_mask(params, &idx, size,
				   (ipv6_spec) ? ipv6_spec->sip6 : NULL,
				   (ipv6_mask) ? ipv6_mask->sip6 : NULL,
				   ULP_PRSR_ACT_DEFAULT);

	dip_idx = idx;
	size = 16;
	bnxt_ulp_gen_prsr_fld_mask(params, &idx, size,
				   (ipv6_spec) ? ipv6_spec->dip6 : NULL,
				   (ipv6_mask) ? ipv6_mask->dip6 : NULL,
				   ULP_PRSR_ACT_DEFAULT);

	/* Set the ipv6 header bitmap and computed l3 header bitmaps */
	if (ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_IPV4) ||
	    ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_IPV6) ||
	    ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_L3_TUN)) {
		ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_I_IPV6);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_I_L3, 1);
		inner_flag = 1;
	} else {
		ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_IPV6);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_L3, 1);
		/* Update the tunnel offload dest ip offset */
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_TUN_OFF_DIP_ID,
				    dip_idx);
	}

	/* Update the field protocol hdr bitmap */
	if (proto_mask)
		bnxt_ulp_gen_l3_proto_type_update(params, proto, inner_flag);
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_L3_HDR_CNT, ++cnt);

	netdev_dbg(bp->dev, "%s: l3-hdr-cnt: %d l3-proto/mask 0x%x/0x%x\n",
		   __func__, cnt, proto, proto_mask);

	return BNXT_TF_RC_SUCCESS;
}

static int bnxt_ulp_gen_l3_v4_handler(struct bnxt *bp,
				      struct ulp_tc_parser_params *params,
				      struct bnxt_ulp_gen_ipv4_hdr *ipv4_spec,
				      struct bnxt_ulp_gen_ipv4_hdr *ipv4_mask)
{
	struct ulp_tc_hdr_bitmap *hdr_bitmap = &params->hdr_bitmap;
	u32 inner_flag = 0;
	u8 proto_mask = 0;
	u16 val16 = 0;
	u8 proto = 0;
	u8 val8 = 0;
	u32 idx = 0;
	u32 size;
	u32 cnt;

	/* validate there is no 3rd L3 header */
	cnt = ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_L3_HDR_CNT);
	if (cnt == 2) {
		netdev_dbg(bp->dev,
			   "Parse Err:Third L3 header not supported\n");
		return BNXT_TF_RC_ERROR;
	}

	if (bnxt_ulp_gen_prsr_fld_size_validate(params, &idx,
						BNXT_ULP_PROTO_HDR_IPV4_NUM)) {
		netdev_dbg(bp->dev, "Error parsing protocol header\n");
		return BNXT_TF_RC_ERROR;
	}

	if (ipv4_spec)
		proto = ipv4_spec->proto ? *ipv4_spec->proto : 0;

	if (ipv4_mask) {
		proto_mask = ipv4_mask->proto ? *ipv4_mask->proto : 0;
		proto &= proto_mask;
	}

	/* version_ihl */
	size = sizeof(val8);
	bnxt_ulp_gen_prsr_fld_mask(params, &idx, size, &val8, &val8,
				   ULP_PRSR_ACT_DEFAULT);

	/* tos: Ignore for matching templates with tunnel flows */
	size = sizeof(val8);
	bnxt_ulp_gen_prsr_fld_mask(params, &idx, size,
				   &val8, &val8, params->tnl_addr_type ?
				   ULP_PRSR_ACT_MATCH_IGNORE :
				   ULP_PRSR_ACT_DEFAULT);

	/* total_length */
	size = sizeof(val16);
	bnxt_ulp_gen_prsr_fld_mask(params, &idx, size, &val16, &val16,
				   ULP_PRSR_ACT_DEFAULT);

	/* packet_id */
	size = sizeof(val16);
	bnxt_ulp_gen_prsr_fld_mask(params, &idx, size, &val16, &val16,
				   ULP_PRSR_ACT_DEFAULT);

	/* fragment_offset */
	size = sizeof(val16);
	bnxt_ulp_gen_prsr_fld_mask(params, &idx, size, &val16, &val16,
				   ULP_PRSR_ACT_DEFAULT);

	/* ttl */
	size = sizeof(val8);
	bnxt_ulp_gen_prsr_fld_mask(params, &idx, size, &val8, &val8,
				   ULP_PRSR_ACT_DEFAULT);

	/* next_proto_id: Ignore proto for matching templates */
	size = sizeof(val8);
	bnxt_ulp_gen_prsr_fld_mask(params, &idx, size,
				   (ipv4_spec) ? ipv4_spec->proto : NULL,
				   (ipv4_mask) ? ipv4_mask->proto : NULL,
				   ULP_PRSR_ACT_DEFAULT);

	/* hdr_checksum */
	size = sizeof(val16);
	bnxt_ulp_gen_prsr_fld_mask(params, &idx, size, &val16, &val16,
				   ULP_PRSR_ACT_DEFAULT);

	size = sizeof(u32);
	bnxt_ulp_gen_prsr_fld_mask(params, &idx, size,
				   (ipv4_spec) ? ipv4_spec->sip : NULL,
				   (ipv4_mask) ? ipv4_mask->sip : NULL,
				   ULP_PRSR_ACT_DEFAULT);

	size = sizeof(u32);
	bnxt_ulp_gen_prsr_fld_mask(params, &idx, size,
				   (ipv4_spec) ? ipv4_spec->dip : NULL,
				   (ipv4_mask) ? ipv4_mask->dip : NULL,
				   ULP_PRSR_ACT_DEFAULT);

	/* Set the ipv4 header bitmap and computed l3 header bitmaps */
	if (ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_IPV4) ||
	    ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_IPV6) ||
	    ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_L3_TUN)) {
		ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_I_IPV4);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_I_L3, 1);
		inner_flag = 1;
	} else {
		ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_IPV4);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_L3, 1);
	}

	/* Update the field protocol hdr bitmap */
	if (proto_mask)
		bnxt_ulp_gen_l3_proto_type_update(params, proto, inner_flag);
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_L3_HDR_CNT, ++cnt);

	netdev_dbg(bp->dev, "%s: l3-hdr-cnt: %d l3-proto/mask 0x%x/0x%x\n",
		   __func__, cnt, proto, proto_mask);
	return BNXT_TF_RC_SUCCESS;
}

static int bnxt_ulp_gen_l3_handler(struct bnxt *bp,
				   struct ulp_tc_parser_params *params,
				   struct bnxt_ulp_gen_l3_hdr_parms *parms)
{
	int rc = BNXT_TF_RC_ERROR;

	if (!parms) {
		netdev_dbg(bp->dev, "ERR: Nothing to do for L3\n");
		return BNXT_TF_RC_ERROR;
	}

	if (parms->type == BNXT_ULP_GEN_L3_IPV4)
		return bnxt_ulp_gen_l3_v4_handler(bp,
						  params,
						  parms->v4_spec,
						  parms->v4_mask);

	if (parms->type == BNXT_ULP_GEN_L3_IPV6)
		return bnxt_ulp_gen_l3_v6_handler(bp,
						  params,
						  parms->v6_spec,
						  parms->v6_mask);

	return rc;
}

static void bnxt_ulp_gen_l4_proto_type_update(struct ulp_tc_parser_params
					      *params, u16 src_port,
					      u16 src_mask, u16 dst_port,
					      u16 dst_mask,
					      enum bnxt_ulp_hdr_bit hdr_bit)
{
	int static_port = 0;

	switch (hdr_bit) {
	case BNXT_ULP_HDR_BIT_I_UDP:
	case BNXT_ULP_HDR_BIT_I_TCP:
		ULP_BITMAP_SET(params->hdr_bitmap.bits, hdr_bit);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_I_L4, 1);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_I_L4_SRC_PORT,
				    (u64)be16_to_cpu(src_port));
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_I_L4_DST_PORT,
				    (u64)be16_to_cpu(dst_port));
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_I_L4_SRC_PORT_MASK,
				    (u64)be16_to_cpu(src_mask));
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_I_L4_DST_PORT_MASK,
				    (u64)be16_to_cpu(dst_mask));
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_I_L3_FB_PROTO_ID,
				    1);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_I_L4_FB_SRC_PORT,
				    !!(src_port & src_mask));
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_I_L4_FB_DST_PORT,
				    !!(dst_port & dst_mask));
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_I_L3_PROTO_ID,
				    (hdr_bit == BNXT_ULP_HDR_BIT_I_UDP) ?
				    IPPROTO_UDP : IPPROTO_TCP);
		break;
	case BNXT_ULP_HDR_BIT_O_UDP:
	case BNXT_ULP_HDR_BIT_O_TCP:
		ULP_BITMAP_SET(params->hdr_bitmap.bits, hdr_bit);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_L4, 1);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_L4_SRC_PORT,
				    (u64)be16_to_cpu(src_port));
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_L4_DST_PORT,
				    (u64)be16_to_cpu(dst_port));
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_L4_SRC_PORT_MASK,
				    (u64)be16_to_cpu(src_mask));
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_L4_DST_PORT_MASK,
				    (u64)be16_to_cpu(dst_mask));
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_L3_FB_PROTO_ID,
				    1);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_L4_FB_SRC_PORT,
				    !!(src_port & src_mask));
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_L4_FB_DST_PORT,
				    !!(dst_port & dst_mask));
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_L3_PROTO_ID,
				    (hdr_bit == BNXT_ULP_HDR_BIT_O_UDP) ?
				    IPPROTO_UDP : IPPROTO_TCP);
		break;
	default:
		break;
	}

	/* If it is not udp port then there is no need to set tunnel bits */
	if (hdr_bit != BNXT_ULP_HDR_BIT_O_UDP)
		return;

	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_TUNNEL_PORT,
			    be16_to_cpu(dst_port));

	/* vxlan static customized port */
	if (ULP_APP_STATIC_VXLAN_PORT_EN(params->ulp_ctx)) {
		static_port = bnxt_ulp_cntxt_vxlan_ip_port_get(params->ulp_ctx);
		if (!static_port)
			static_port =
			bnxt_ulp_cntxt_vxlan_port_get(params->ulp_ctx);

		/* if udp and equal to static vxlan port then set tunnel bits*/
		if (static_port && dst_port == cpu_to_be16(static_port)) {
			ULP_BITMAP_SET(params->hdr_fp_bit.bits,
				       BNXT_ULP_HDR_BIT_T_VXLAN);
			ULP_BITMAP_SET(params->cf_bitmap,
				       BNXT_ULP_CF_BIT_IS_TUNNEL);
		}
	} else {
		/* if dynamic Vxlan is enabled then skip dport checks */
		if (ULP_APP_DYNAMIC_VXLAN_PORT_EN(params->ulp_ctx))
			return;

		/* Vxlan port check */
		if (dst_port == cpu_to_be16(BNXT_ULP_GEN_UDP_PORT_VXLAN)) {
			ULP_BITMAP_SET(params->hdr_fp_bit.bits,
				       BNXT_ULP_HDR_BIT_T_VXLAN);
			ULP_BITMAP_SET(params->cf_bitmap,
				       BNXT_ULP_CF_BIT_IS_TUNNEL);
			/*ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_L3_TUN, 1);*/
		}
	}
}

static void bnxt_ulp_gen_bth_proto_type_update(struct ulp_tc_parser_params
					       *params, u16 op_code,
					       u16 op_code_mask, u16 dst_qpn,
					       u16 dst_qpn_mask,
					       enum bnxt_ulp_hdr_bit hdr_bit)
{
	switch (hdr_bit) {
	case BNXT_ULP_HDR_BIT_I_BTH:
		ULP_BITMAP_SET(params->hdr_bitmap.bits, hdr_bit);
		ULP_BITMAP_RESET(params->hdr_bitmap.bits,
				 BNXT_ULP_HDR_BIT_I_UDP);
		ULP_BITMAP_RESET(params->hdr_fp_bit.bits,
				 BNXT_ULP_HDR_BIT_I_UDP);
		break;
	case BNXT_ULP_HDR_BIT_O_BTH:
		ULP_BITMAP_SET(params->hdr_bitmap.bits, hdr_bit);
		ULP_BITMAP_RESET(params->hdr_bitmap.bits,
				 BNXT_ULP_HDR_BIT_O_UDP);
		ULP_BITMAP_RESET(params->hdr_fp_bit.bits,
				 BNXT_ULP_HDR_BIT_O_UDP);
		break;
	default:
		break;
	}
}

static int bnxt_ulp_gen_l4_udp_handler(struct bnxt *bp,
				       struct ulp_tc_parser_params *params,
				       struct bnxt_ulp_gen_udp_hdr *spec,
				       struct bnxt_ulp_gen_udp_hdr *mask)
{
	struct ulp_tc_hdr_bitmap *hdr_bitmap = &params->hdr_bitmap;
	enum bnxt_ulp_hdr_bit out_l4 = BNXT_ULP_HDR_BIT_O_UDP;
	u16 dport_mask = 0, sport_mask = 0;
	u16 dport = 0, sport = 0;
	u16 dgram_cksum = 0;
	u16 dgram_len = 0;
	u32 idx = 0;
	u32 size;
	u32 cnt;

	cnt = ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_L4_HDR_CNT);
	if (cnt == 2) {
		netdev_dbg(bp->dev,
			   "Parse Err:Third L4 header not supported\n");
		return BNXT_TF_RC_ERROR;
	}

	if (spec) {
		sport = (spec->sport) ? *spec->sport : 0;
		dport = (spec->dport) ? *spec->dport : 0;
	}
	if (mask) {
		sport_mask = (mask->sport) ? *mask->sport : 0;
		dport_mask = (mask->dport) ? *mask->dport : 0;
	}

	if (bnxt_ulp_gen_prsr_fld_size_validate(params, &idx,
						BNXT_ULP_PROTO_HDR_UDP_NUM)) {
		netdev_dbg(bp->dev, "Error parsing protocol header\n");
		return BNXT_TF_RC_ERROR;
	}

	size = sizeof(*spec->sport);
	bnxt_ulp_gen_prsr_fld_mask(params, &idx, size, spec->sport,
				   mask->sport, ULP_PRSR_ACT_DEFAULT);

	size = sizeof(*spec->dport);
	bnxt_ulp_gen_prsr_fld_mask(params, &idx, size, spec->dport,
				   mask->dport, ULP_PRSR_ACT_DEFAULT);

	size = sizeof(dgram_len);
	bnxt_ulp_gen_prsr_fld_mask(params, &idx, size, &dgram_len, &dgram_len,
				   ULP_PRSR_ACT_DEFAULT);

	size = sizeof(dgram_cksum);
	bnxt_ulp_gen_prsr_fld_mask(params, &idx, size, &dgram_cksum,
				   &dgram_cksum, ULP_PRSR_ACT_DEFAULT);

	/* Set the udp header bitmap and computed l4 header bitmaps */
	if (ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_UDP) ||
	    ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_TCP) ||
	    ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_L3_TUN))
		out_l4 = BNXT_ULP_HDR_BIT_I_UDP;

	bnxt_ulp_gen_l4_proto_type_update(params, sport, sport_mask, dport,
					  dport_mask, out_l4);
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_L4_HDR_CNT, ++cnt);

	return BNXT_TF_RC_SUCCESS;
}

static int bnxt_ulp_gen_l4_tcp_handler(struct bnxt *bp,
				       struct ulp_tc_parser_params *params,
				       struct bnxt_ulp_gen_tcp_hdr *spec,
				       struct bnxt_ulp_gen_tcp_hdr *mask)
{
	struct ulp_tc_hdr_bitmap *hdr_bitmap = &params->hdr_bitmap;
	enum bnxt_ulp_hdr_bit out_l4 = BNXT_ULP_HDR_BIT_O_TCP;
	u16 dport_mask = 0, sport_mask = 0;
	u16 dport = 0, sport = 0;
	u32 idx = 0;
	u32 size;
	u32 cnt;

	cnt = ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_L4_HDR_CNT);
	if (cnt == 2) {
		netdev_dbg(bp->dev,
			   "Parse Err:Third L4 header not supported\n");
		return BNXT_TF_RC_ERROR;
	}

	if (spec) {
		sport = (spec->sport) ? *spec->sport : 0;
		dport = (spec->dport) ? *spec->dport : 0;
	}
	if (mask) {
		sport_mask = (mask->sport) ? *mask->sport : 0;
		dport_mask = (mask->dport) ? *mask->dport : 0;
	}

	if (bnxt_ulp_gen_prsr_fld_size_validate(params, &idx,
						BNXT_ULP_PROTO_HDR_TCP_NUM -
						7)) {
		netdev_dbg(bp->dev, "Error parsing protocol header\n");
		return BNXT_TF_RC_ERROR;
	}

	size = sizeof(*spec->sport);
	bnxt_ulp_gen_prsr_fld_mask(params, &idx, size, spec->sport,
				   mask->sport, ULP_PRSR_ACT_DEFAULT);

	size = sizeof(*spec->dport);
	bnxt_ulp_gen_prsr_fld_mask(params, &idx, size, spec->dport,
				   mask->dport, ULP_PRSR_ACT_DEFAULT);

	if (ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_UDP) ||
	    ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_TCP) ||
	    ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_L3_TUN))
		out_l4 = BNXT_ULP_HDR_BIT_I_TCP;

	bnxt_ulp_gen_l4_proto_type_update(params, sport, sport_mask, dport,
					  dport_mask, out_l4);
	return BNXT_TF_RC_SUCCESS;
}

static int bnxt_ulp_gen_l4_roce_handler(struct bnxt *bp,
					struct ulp_tc_parser_params *params,
					struct bnxt_ulp_gen_bth_hdr *spec,
					struct bnxt_ulp_gen_bth_hdr *mask)
{
	enum bnxt_ulp_hdr_bit out_l4 = BNXT_ULP_HDR_BIT_O_BTH;
	u16 dst_qpn_mask = 0, op_code_mask = 0;
	u16 dst_qpn = 0, op_code = 0;
	u32 idx = 0;
	u32 size;
	u32 cnt;

	cnt = ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_L4_HDR_CNT);
	if (cnt == 2) {
		netdev_dbg(bp->dev,
			   "Parse Err:Third L4 header not supported\n");
		return BNXT_TF_RC_ERROR;
	}

	if (spec) {
		op_code = (spec->op_code) ? *spec->op_code : 0;
		dst_qpn = (spec->dst_qpn) ? *spec->dst_qpn : 0;
	}
	if (mask) {
		op_code_mask = (mask->op_code) ? *mask->op_code : 0;
		dst_qpn_mask = (mask->dst_qpn) ? *mask->dst_qpn : 0;
	}

	if (bnxt_ulp_gen_prsr_fld_size_validate(params, &idx,
						BNXT_ULP_PROTO_HDR_BTH_NUM)) {
		netdev_dbg(bp->dev, "Error parsing protocol header\n");
		return BNXT_TF_RC_ERROR;
	}

	if (spec->op_code)
		netdev_dbg(bp->dev,
			   "L4 header idx %d opcde 0x%x\n", idx, *spec->op_code);

	if (spec->op_code && *spec->op_code == cpu_to_be16(BNXT_ROCE_CNP_OPCODE))
		ULP_BITMAP_SET(params->cf_bitmap, BNXT_ULP_CF_BIT_ROCE_CNP);

	size = sizeof(*spec->op_code);
	bnxt_ulp_gen_prsr_fld_mask(params, &idx, size, spec->op_code,
				   mask->op_code, ULP_PRSR_ACT_DEFAULT);

	if (spec->dst_qpn)
		netdev_dbg(bp->dev,
			   "L4 header idx %d qpn 0x%x\n", idx, *spec->dst_qpn);
	size = sizeof(*spec->dst_qpn);
	bnxt_ulp_gen_prsr_fld_mask(params, &idx, size, spec->dst_qpn,
				   mask->dst_qpn, ULP_PRSR_ACT_DEFAULT);

	if (spec->bth_flags)
		netdev_dbg(bp->dev,
			   "L4 header idx %d bth_flags 0x%x\n", idx, *spec->bth_flags);
	size = sizeof(*spec->bth_flags);
	bnxt_ulp_gen_prsr_fld_mask(params, &idx, size, spec->bth_flags,
				   mask->bth_flags, ULP_PRSR_ACT_DEFAULT);

	if (ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_L3_TUN))
		out_l4 = BNXT_ULP_HDR_BIT_I_BTH;

	bnxt_ulp_gen_bth_proto_type_update(params, op_code, op_code_mask,
					   dst_qpn, dst_qpn_mask, out_l4);
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_L4_HDR_CNT, ++cnt);

	return BNXT_TF_RC_SUCCESS;
}

static int bnxt_ulp_gen_l4_handler(struct bnxt *bp,
				   struct ulp_tc_parser_params *params,
				   struct bnxt_ulp_gen_l4_hdr_parms *parms)
{
	if (!parms) {
		netdev_dbg(bp->dev, "ERR: Nothing to do for L4\n");
		return BNXT_TF_RC_ERROR;
	}

	if (parms->type == BNXT_ULP_GEN_L4_UDP)
		return bnxt_ulp_gen_l4_udp_handler(bp,
						   params,
						   parms->udp_spec,
						   parms->udp_mask);

	if (parms->type == BNXT_ULP_GEN_L4_TCP)
		return bnxt_ulp_gen_l4_tcp_handler(bp,
						   params,
						   parms->tcp_spec,
						   parms->tcp_mask);

	if (parms->type == BNXT_ULP_GEN_L4_BTH)
		return bnxt_ulp_gen_l4_roce_handler(bp,
						    params,
						    parms->bth_spec,
						    parms->bth_mask);

	return BNXT_TF_RC_ERROR;
}

static int bnxt_ulp_gen_tun_ifa_handler(struct bnxt *bp,
					struct ulp_tc_parser_params *params,
					struct bnxt_ulp_gen_ifa_hdr *spec,
					struct bnxt_ulp_gen_ifa_hdr *mask)
{
	u16 gns_mask = 0;
	u16 gns = 0;
	u32 idx = 0;
	u32 size;

	if (spec) {
		gns = (spec->gns) ? *spec->gns : 0;
		netdev_dbg(bp->dev, "GNS SPEC 0x%x\n", gns);
	}
	if (mask) {
		gns_mask = (mask->gns) ? *mask->gns : 0;
		netdev_dbg(bp->dev, "GNS MASK 0x%x\n", gns_mask);
	}

	if (bnxt_ulp_gen_prsr_fld_size_validate(params, &idx,
						BNXT_ULP_PROTO_HDR_IFA_NUM)) {
		netdev_dbg(bp->dev, "Error parsing ifa tunnel header\n");
		return BNXT_TF_RC_ERROR;
	}

	size = 4;
	bnxt_ulp_gen_prsr_fld_mask(params, &idx, size, spec->gns,
				   mask->gns, ULP_PRSR_ACT_DEFAULT);

	ULP_BITMAP_SET(params->hdr_bitmap.bits, BNXT_ULP_HDR_BIT_T_IFA);
	ULP_BITMAP_SET(params->cf_bitmap, BNXT_ULP_CF_BIT_IS_TUNNEL);
	return BNXT_TF_RC_SUCCESS;
}

static int bnxt_ulp_gen_tun_handler(struct bnxt *bp,
				    struct ulp_tc_parser_params *params,
				    struct bnxt_ulp_gen_tun_hdr_parms *parms)
{
	if (!parms) {
		netdev_dbg(bp->dev, "ERR: Nothing to do for TUNNEL\n");
		return BNXT_TF_RC_ERROR;
	}

	if (parms->type == BNXT_ULP_GEN_TUN_IFA)
		return bnxt_ulp_gen_tun_ifa_handler(bp,
						    params,
						    parms->ifa_spec,
						    parms->ifa_mask);

	return BNXT_TF_RC_ERROR;
}

static int bnxt_ulp_gen_hdr_parser(struct bnxt *bp,
				   struct ulp_tc_parser_params *params,
				   struct bnxt_ulp_gen_flow_parms *parms)
{
	int32_t rc = 0;

	if (!parms) {
		netdev_dbg(bp->dev, "ERR: Flow add parms is NULL\n");
		return -EINVAL;
	}

	params->field_idx = BNXT_ULP_PROTO_HDR_SVIF_NUM;

	if (parms->l2)
		rc = bnxt_ulp_gen_l2_handler(bp, params, parms->l2);
	if (rc) {
		netdev_dbg(bp->dev, "ERR: L2 Handler error = %d\n", rc);
		return rc;
	}
	if (parms->l3)
		rc = bnxt_ulp_gen_l3_handler(bp, params, parms->l3);
	if (rc) {
		netdev_dbg(bp->dev, "ERR: L3 Handler error = %d\n", rc);
		return rc;
	}
	if (parms->l4)
		rc = bnxt_ulp_gen_l4_handler(bp, params, parms->l4);
	if (rc) {
		netdev_dbg(bp->dev, "ERR: L4 Handler error = %d\n", rc);
		return rc;
	}
	if (parms->tun)
		rc = bnxt_ulp_gen_tun_handler(bp, params, parms->tun);
	if (rc) {
		netdev_dbg(bp->dev, "ERR: TUN Handler error = %d\n", rc);
		return rc;
	}

	/* update the implied SVIF */
	rc = ulp_tc_parser_implicit_match_port_process(params);
	return rc;
}

static int bnxt_ulp_gen_act_kid_handler(struct bnxt *bp,
					struct ulp_tc_parser_params *params,
					struct bnxt_ulp_gen_action_parms *parms)
{
	struct ulp_tc_act_prop *act = &params->act_prop;

	if (!parms) {
		netdev_dbg(bp->dev, "ERR:  NULL parms for KID action\n");
		return BNXT_TF_RC_ERROR;
	}

	ULP_BITMAP_SET(params->act_bitmap.bits, BNXT_ULP_ACT_BIT_UPDATE_KID);
	memcpy(&act->act_details[BNXT_ULP_ACT_PROP_IDX_UPDATE_KID],
	       &parms->kid, BNXT_ULP_ACT_PROP_SZ_UPDATE_KID);

	return BNXT_TF_RC_SUCCESS;
}

static int bnxt_ulp_gen_act_drop_handler(struct bnxt *bp,
					 struct ulp_tc_parser_params *params,
					 struct bnxt_ulp_gen_action_parms
					 *parms)
{
	if (!parms) {
		netdev_dbg(bp->dev, "ERR:  NULL parms for DROP action\n");
		return BNXT_TF_RC_ERROR;
	}

	/* Update the hdr_bitmap with drop */
	ULP_BITMAP_SET(params->act_bitmap.bits, BNXT_ULP_ACT_BIT_DROP);
	return BNXT_TF_RC_SUCCESS;
}

static int bnxt_ulp_gen_act_redirect_handler(struct bnxt *bp, struct ulp_tc_parser_params
					     *params, struct bnxt_ulp_gen_action_parms
					     *parms)
{
	enum bnxt_ulp_intf_type intf_type;
	u32 ifindex;
	u16 dst_fid;

	if (!parms) {
		netdev_dbg(bp->dev, "ERR:  NULL parms for REDIRECT action\n");
		return BNXT_TF_RC_ERROR;
	}

	dst_fid = parms->dst_fid;

	/* Get the port db ifindex */
	if (ulp_port_db_dev_port_to_ulp_index(params->ulp_ctx, dst_fid,
					      &ifindex)) {
		netdev_dbg(bp->dev, "Invalid destination fid %d\n", dst_fid);
		return BNXT_TF_RC_ERROR;
	}

	/* Get the intf type */
	intf_type = ulp_port_db_port_type_get(params->ulp_ctx, ifindex);
	if (!intf_type) {
		netdev_dbg(bp->dev, "Invalid port type\n");
		return BNXT_TF_RC_ERROR;
	}

	/* Set the action port */
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_ACT_PORT_TYPE, intf_type);
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_DEV_ACT_PORT_ID, dst_fid);

	return ulp_tc_parser_act_port_set(params, ifindex, !parms->ignore_lag);
}

static int bnxt_ulp_gen_act_count_handler(struct bnxt *bp,
					  struct ulp_tc_parser_params *params,
					  struct bnxt_ulp_gen_action_parms
					  *parms)
{
	if (!parms) {
		netdev_dbg(bp->dev, "ERR:  NULL parms for COUNT action\n");
		return BNXT_TF_RC_ERROR;
	}

	/* Update the hdr_bitmap with count */
	ULP_BITMAP_SET(params->act_bitmap.bits, BNXT_ULP_ACT_BIT_COUNT);
	return BNXT_TF_RC_SUCCESS;
}

static int bnxt_ulp_gen_act_modify_smac_handler(struct bnxt *bp, struct ulp_tc_parser_params
						*params, struct bnxt_ulp_gen_action_parms
						*parms)
{
	struct ulp_tc_act_prop *act = &params->act_prop;

	if (!parms) {
		netdev_dbg(bp->dev,
			   "ERR:  NULL parms for Modify SMAC action\n");
		return BNXT_TF_RC_ERROR;
	}

	memcpy(&act->act_details[BNXT_ULP_ACT_PROP_IDX_SET_MAC_SRC],
	       parms->smac, BNXT_ULP_ACT_PROP_SZ_SET_MAC_SRC);

	/* Update the hdr_bitmap with set mac src */
	ULP_BITMAP_SET(params->act_bitmap.bits, BNXT_ULP_ACT_BIT_SET_MAC_SRC);
	return BNXT_TF_RC_SUCCESS;
}

static int bnxt_ulp_gen_act_modify_dmac_handler(struct bnxt *bp, struct ulp_tc_parser_params
						*params, struct bnxt_ulp_gen_action_parms
						*parms)
{
	struct ulp_tc_act_prop *act = &params->act_prop;

	if (!parms) {
		netdev_dbg(bp->dev,
			   "ERR:  NULL parms for Modify DMAC action\n");
		return BNXT_TF_RC_ERROR;
	}

	memcpy(&act->act_details[BNXT_ULP_ACT_PROP_IDX_SET_MAC_DST],
	       parms->dmac, BNXT_ULP_ACT_PROP_SZ_SET_MAC_DST);

	/* Update the hdr_bitmap with set mac dst */
	ULP_BITMAP_SET(params->act_bitmap.bits, BNXT_ULP_ACT_BIT_SET_MAC_DST);
	return BNXT_TF_RC_SUCCESS;
}

static int bnxt_ulp_gen_act_vnic_handler(struct bnxt *bp,
					 struct ulp_tc_parser_params *params,
					 struct bnxt_ulp_gen_action_parms *parms)
{
	struct ulp_tc_act_prop *act = &params->act_prop;
	enum bnxt_ulp_direction_type dir;
	u32 pid;

	if (!parms) {
		netdev_dbg(bp->dev, "ERR:  NULL parms for vnic action\n");
		return BNXT_TF_RC_ERROR;
	}

	/* Not supporting VF to VF for now */
	dir = ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_DIRECTION);
	if (dir != BNXT_ULP_DIR_INGRESS) {
		netdev_dbg(bp->dev, "ERR:  not supporting vf to vf\n");
		return BNXT_TF_RC_ERROR;
	}

	pid = parms->vnic;
	/* Allows use of func_opcode with VNIC */
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_VNIC, pid);

	pid = cpu_to_be32(pid);
	memcpy(&act->act_details[BNXT_ULP_ACT_PROP_IDX_VNIC],
	       &pid, BNXT_ULP_ACT_PROP_SZ_VNIC);

	/* treat the vnic as if the port was explicitly set */
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_ACT_PORT_IS_SET, 1);

	return BNXT_TF_RC_SUCCESS;
}

static int bnxt_ulp_gen_act_queue_handler(struct bnxt *bp,
					  struct ulp_tc_parser_params *params,
					  struct bnxt_ulp_gen_action_parms *parms)
{
	struct ulp_tc_act_prop *act = &params->act_prop;
	enum bnxt_ulp_direction_type dir;
	u16 queue = 0;

	if (params->dir_attr == BNXT_ULP_FLOW_ATTR_INGRESS)
		dir = BNXT_ULP_DIR_INGRESS;
	else
		dir = BNXT_ULP_DIR_EGRESS;

	if (dir != BNXT_ULP_DIR_INGRESS) {
		netdev_dbg(bp->dev, "ERR: queue action is ingress only\n");
		return BNXT_TF_RC_ERROR;
	}
	queue = cpu_to_be16(parms->queue);
	/* Copy the queue into the specific action properties */
	memcpy(&act->act_details[BNXT_ULP_ACT_PROP_IDX_QUEUE_INDEX],
	       &queue, BNXT_ULP_ACT_PROP_SZ_QUEUE_INDEX);

	/* set the queue action header bit */
	ULP_BITMAP_SET(params->act_bitmap.bits, BNXT_ULP_ACT_BIT_QUEUE);

	return BNXT_TF_RC_SUCCESS;
}

static int bnxt_ulp_gen_act_loopback_handler(struct bnxt *bp,
					     struct ulp_tc_parser_params *params,
					     struct bnxt_ulp_gen_action_parms
					     *parms)
{
	if (!parms) {
		netdev_dbg(bp->dev, "ERR:  NULL parms for COUNT action\n");
		return BNXT_TF_RC_ERROR;
	}

	/* set the comp field */
	ULP_COMP_FLD_IDX_WR(params,
			    BNXT_ULP_CF_IDX_LOOPBACK_VPORT,
			    1);
	return BNXT_TF_RC_SUCCESS;
}

static int bnxt_ulp_gen_act_ring_tbl_idx_handler(struct bnxt *bp,
						 struct ulp_tc_parser_params *params,
						 struct bnxt_ulp_gen_action_parms *parms)
{
	struct ulp_tc_act_prop *act = &params->act_prop;
	enum bnxt_ulp_direction_type dir;
	u16 ring_tbl_index = 0;

	if (params->dir_attr == BNXT_ULP_FLOW_ATTR_INGRESS)
		dir = BNXT_ULP_DIR_INGRESS;
	else
		dir = BNXT_ULP_DIR_EGRESS;

	if (dir != BNXT_ULP_DIR_INGRESS) {
		netdev_dbg(bp->dev, "ERR: ring tbl idx action is ingress only\n");
		return BNXT_TF_RC_ERROR;
	}
	ring_tbl_index = cpu_to_be16(parms->ring_tbl_index);
	/* Copy the ring table index into the specific action properties */
	memcpy(&act->act_details[BNXT_ULP_ACT_PROP_IDX_RING_TBL_IDX],
	       &ring_tbl_index, BNXT_ULP_ACT_PROP_SZ_RING_TBL_IDX);

	/* set the ring table index action property bit */
	ULP_BITMAP_SET(params->act_bitmap.bits, BNXT_ULP_ACT_BIT_RING_TBL_IDX);

	return BNXT_TF_RC_SUCCESS;
}

static int bnxt_ulp_gen_act_parser(struct bnxt *bp,
				   struct ulp_tc_parser_params *params,
				   struct bnxt_ulp_gen_flow_parms *parms)
{
	int rc = 0;
	u64 actions;

	if (!parms) {
		netdev_dbg(bp->dev, "ERR: Flow actions parms is NULL\n");
		return -EINVAL;
	}

	if (parms->actions)
		actions = parms->actions->enables;
	else
		return -EIO;

	if (actions & BNXT_ULP_GEN_ACTION_ENABLES_KID)
		rc = bnxt_ulp_gen_act_kid_handler(bp, params, parms->actions);
	if (rc) {
		netdev_dbg(bp->dev, "ERR: KID Action Handler error = %d\n", rc);
		return rc;
	}

	if (actions & BNXT_ULP_GEN_ACTION_ENABLES_DROP)
		rc = bnxt_ulp_gen_act_drop_handler(bp, params, parms->actions);
	if (rc) {
		netdev_dbg(bp->dev, "ERR: DROP Action Handler error = %d\n",
			   rc);
		return rc;
	}

	if (actions & BNXT_ULP_GEN_ACTION_ENABLES_REDIRECT)
		rc = bnxt_ulp_gen_act_redirect_handler(bp, params,
						       parms->actions);
	if (rc) {
		netdev_dbg(bp->dev, "ERR: REDIRECT Action Handler error = %d\n",
			   rc);
		return rc;
	}

	if (actions & BNXT_ULP_GEN_ACTION_ENABLES_VNIC)
		rc = bnxt_ulp_gen_act_vnic_handler(bp, params, parms->actions);
	if (rc) {
		netdev_dbg(bp->dev, "ERR: VNIC Action Handler error = %d\n",
			   rc);
		return rc;
	}

	if (actions & BNXT_ULP_GEN_ACTION_ENABLES_COUNT)
		rc = bnxt_ulp_gen_act_count_handler(bp, params, parms->actions);
	if (rc) {
		netdev_dbg(bp->dev, "ERR: COUNT Action Handler error = %d\n",
			   rc);
		return rc;
	}

	if (actions & BNXT_ULP_GEN_ACTION_ENABLES_SET_SMAC)
		rc = bnxt_ulp_gen_act_modify_smac_handler(bp, params,
							  parms->actions);
	if (rc) {
		netdev_dbg(bp->dev,
			   "ERR: Modify SMAC Action Handler error = %d\n", rc);
		return rc;
	}

	if (actions & BNXT_ULP_GEN_ACTION_ENABLES_SET_DMAC)
		rc = bnxt_ulp_gen_act_modify_dmac_handler(bp, params,
							  parms->actions);
	if (rc) {
		netdev_dbg(bp->dev,
			   "ERR: Modify DMAC Action Handler error = %d\n", rc);
		return rc;
	}

	if (actions & BNXT_ULP_GEN_ACTION_ENABLES_QUEUE)
		rc = bnxt_ulp_gen_act_queue_handler(bp, params, parms->actions);
	if (rc) {
		netdev_dbg(bp->dev, "ERR: QUEUE Action Handler error = %d\n",
			   rc);
		return rc;
	}

	if (actions & BNXT_ULP_GEN_ACTION_ENABLES_REDIRECT_LOOPBACK)
		rc = bnxt_ulp_gen_act_loopback_handler(bp, params, parms->actions);
	if (rc) {
		netdev_dbg(bp->dev, "ERR: LOOPBACK Action Handler error = %d\n",
			   rc);
		return rc;
	}

	if (actions & BNXT_ULP_GEN_ACTION_ENABLES_RING_TBL_IDX)
		rc = bnxt_ulp_gen_act_ring_tbl_idx_handler(bp, params, parms->actions);
	if (rc) {
		netdev_dbg(bp->dev, "ERR: Ring TBL IDX Action Handler error = %d\n",
			   rc);
		return rc;
	}
	return rc;
}

int bnxt_ulp_gen_flow_create(struct bnxt *bp,
			     u16 src_fid,
			     struct bnxt_ulp_gen_flow_parms *flow_parms)
{
	struct bnxt_ulp_mapper_parms mapper_mparms = { 0 };
	struct ulp_tc_parser_params *parser_params = NULL;
	struct bnxt_ulp_context *ulp_ctx;
	int rc, tf_rc = BNXT_TF_RC_ERROR;
	unsigned long lastused;
	u64 packets, bytes;
	u16 func_id;
	u32 fid;

	/* Initialize the parser parser_params */
	parser_params = vzalloc(sizeof(*parser_params));
	if (!parser_params)
		goto flow_error;

	/* Get the ULP Context */
	ulp_ctx = bnxt_ulp_bp_ptr2_cntxt_get(bp);
	if (!ulp_ctx) {
		netdev_dbg(bp->dev, "ULP context is not initialized\n");
		goto flow_error;
	}
	parser_params->ulp_ctx = ulp_ctx;

	/* Get the ULP APP id */
	if (bnxt_ulp_cntxt_app_id_get
	    (parser_params->ulp_ctx, &parser_params->app_id)) {
		netdev_dbg(bp->dev, "Failed to get the app id\n");
		goto flow_error;
	}

	/* Set the flow attributes */
	bnxt_ulp_gen_set_dir_attributes(bp, parser_params, flow_parms->dir);

	/* Set NPAR Enabled in the computed fields */
	if (BNXT_NPAR(ulp_ctx->bp))
		ULP_COMP_FLD_IDX_WR(parser_params, BNXT_ULP_CF_IDX_NPAR_ENABLED, 1);

	/* Copy the device port id and direction for further processing */
	ULP_COMP_FLD_IDX_WR(parser_params, BNXT_ULP_CF_IDX_INCOMING_IF,
			    src_fid);
	ULP_COMP_FLD_IDX_WR(parser_params, BNXT_ULP_CF_IDX_DEV_PORT_ID,
			    src_fid);
	ULP_COMP_FLD_IDX_WR(parser_params, BNXT_ULP_CF_IDX_SVIF_FLAG,
			    BNXT_ULP_INVALID_SVIF_VAL);

	/* Get the function id */
	if (ulp_port_db_port_func_id_get(ulp_ctx, src_fid, &func_id)) {
		netdev_dbg(bp->dev, "Conversion of port to func id failed src_fid(%d)\n",
			   src_fid);
		goto flow_error;
	}

	/* Protect flow creation */
	mutex_lock(&ulp_ctx->cfg_data->flow_db_lock);

	/* Allocate a Flow ID to attach all resources for the flow.
	 * Once allocated, all errors have to walk the list of resources and
	 * free each of them.
	 */
	rc = ulp_flow_db_fid_alloc(ulp_ctx, BNXT_ULP_FDB_TYPE_REGULAR,
				   func_id, &fid);
	if (rc) {
		netdev_dbg(bp->dev, "Unable to allocate flow table entry\n");
		goto release_lock;
	}

	/* Parse the flow headers */
	rc = bnxt_ulp_gen_hdr_parser(bp, parser_params, flow_parms);
	if (rc) {
		netdev_dbg(bp->dev, "ERR: Failed to parse headers\n");
		goto free_fid;
	}

	/* Parse the flow action */
	rc = bnxt_ulp_gen_act_parser(bp, parser_params, flow_parms);
	if (rc) {
		netdev_dbg(bp->dev, "ERR: Failed to parse actions\n");
		goto free_fid;
	}

	/* DEF_PIRO bit is used by OVS-TC. It is not used by generic ntuple.
	 * It has to be set to put entry in EM.
	 */
	if (flow_parms->enable_em)
		ULP_BITMAP_SET(parser_params->cf_bitmap, BNXT_ULP_CF_BIT_DEF_PRIO);

	parser_params->fid = fid;
	parser_params->func_id = func_id;
	parser_params->port_id = src_fid;
	parser_params->priority = flow_parms->priority;
	if (flow_parms->lkup_strength)
		ULP_COMP_FLD_IDX_WR(parser_params,
				    BNXT_ULP_CF_IDX_LOOKUP_STRENGTH,
				    flow_parms->lkup_strength);

	netdev_dbg(bp->dev, "Flow prio: %u strength: %u func_id: %u APP ID %u\n",
		   parser_params->priority, flow_parms->lkup_strength, func_id,
		   parser_params->app_id);

	/* Perform the flow post process */
	tf_rc = bnxt_ulp_tc_parser_post_process(parser_params);
	if (tf_rc == BNXT_TF_RC_ERROR)
		goto free_fid;
	else if (tf_rc == BNXT_TF_RC_FID)
		goto return_fid;

	/* Dump the flow pattern */
	ulp_parser_hdr_info_dump(parser_params);
	/* Dump the flow action */
	ulp_parser_act_info_dump(parser_params);

	tf_rc =
	    ulp_matcher_pattern_match(parser_params, &parser_params->class_id);
	if (tf_rc != BNXT_TF_RC_SUCCESS)
		goto free_fid;

	tf_rc =
	    ulp_matcher_action_match(parser_params, &parser_params->act_tmpl);
	if (tf_rc != BNXT_TF_RC_SUCCESS)
		goto free_fid;

	bnxt_ulp_gen_init_mapper_params(&mapper_mparms, parser_params,
					BNXT_ULP_FDB_TYPE_REGULAR);

	/* Call the ulp mapper to create the flow in the hardware. */
	tf_rc = ulp_mapper_flow_create(ulp_ctx, &mapper_mparms, NULL);
	if (tf_rc)
		goto free_fid;

 return_fid:
	/* Setup return vals for caller */
	if (flow_parms->flow_id)
		*flow_parms->flow_id = fid;

	vfree(parser_params);
	mutex_unlock(&ulp_ctx->cfg_data->flow_db_lock);

	/* Setup return HW counter id for caller, if requested */
	if (flow_parms->counter_hndl)
		ulp_tf_fc_mgr_query_count_get(ulp_ctx,
					      fid, &packets,
					      &bytes, &lastused,
					      flow_parms->counter_hndl);
	return BNXT_TF_RC_SUCCESS;

 free_fid:
	vfree(parser_params->tnl_key);
	vfree(parser_params->neigh_key);
	ulp_flow_db_fid_free(ulp_ctx, BNXT_ULP_FDB_TYPE_REGULAR, fid);
 release_lock:
	mutex_unlock(&ulp_ctx->cfg_data->flow_db_lock);
 flow_error:
	vfree(parser_params);
	if ((tf_rc == -ENOSPC) || (tf_rc == -EEXIST))
		return tf_rc;
	else
		return (tf_rc ==
			BNXT_TF_RC_PARSE_ERR_NOTSUPP) ? -EOPNOTSUPP : -EIO;
}

int bnxt_ulp_gen_flow_destroy(struct bnxt *bp, u16 src_fid, u32 flow_id)
{
	struct bnxt_ulp_context *ulp_ctx;
	u16 func_id;
	int rc;

	ulp_ctx = bnxt_ulp_bp_ptr2_cntxt_get(bp);
	if (!ulp_ctx) {
		netdev_dbg(bp->dev, "ULP context is not initialized\n");
		return -ENOENT;
	}

	if (ulp_port_db_port_func_id_get(ulp_ctx, src_fid, &func_id)) {
		netdev_dbg(bp->dev, "Conversion of port to func id failed\n");
		return -EINVAL;
	}

	rc = ulp_flow_db_validate_flow_func(ulp_ctx, flow_id, func_id);
	if (rc)
		return rc;

	mutex_lock(&ulp_ctx->cfg_data->flow_db_lock);
	rc = ulp_mapper_flow_destroy(ulp_ctx, BNXT_ULP_FDB_TYPE_REGULAR,
				     flow_id, NULL);
	mutex_unlock(&ulp_ctx->cfg_data->flow_db_lock);

	return rc;
}

void bnxt_ulp_gen_flow_query_count(struct bnxt *bp,
				   u32 flow_id,
				   u64 *packets,
				   u64 *bytes, unsigned long *lastused)
{
	ulp_tf_fc_mgr_query_count_get(bp->ulp_ctx, flow_id, packets, bytes,
				      lastused, NULL);
}

#endif /*if defined(CONFIG_BNXT_FLOWER_OFFLOAD) */
