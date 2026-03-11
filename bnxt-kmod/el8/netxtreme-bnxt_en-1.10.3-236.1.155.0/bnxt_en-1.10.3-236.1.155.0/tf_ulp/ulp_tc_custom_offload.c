// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2023-2023 Broadcom
 * All rights reserved.
 */

#ifdef CONFIG_BNXT_CUSTOM_FLOWER_OFFLOAD
#include <linux/vmalloc.h>

#include "bnxt_compat.h"
#include "bnxt_hsi.h"
#include "bnxt.h"
#include "bnxt_tf_common.h"
#include "bnxt_ulp_flow.h"
#include "ulp_tc_parser.h"
#include "ulp_matcher.h"
#include "ulp_flow_db.h"
#include "ulp_mapper.h"
#include "ulp_fc_mgr.h"
#include "ulp_port_db.h"
#include "ulp_tc_parser.h"
#include "ulp_template_debug_proto.h"
#include "ulp_tc_custom_offload.h"
#include "ulp_tc_rte_flow.h"

static inline void
bnxt_custom_ulp_set_dir_attributes(struct bnxt *bp, struct ulp_tc_parser_params
				   *params, u16 src_fid)
{
	/* Set the flow attributes.
	 * TBD: This logic might need some port-process fixing for the
	 * vxlan-decap case.
	 */
	if (bp->pf.fw_fid == src_fid)
		params->dir_attr |= BNXT_ULP_FLOW_ATTR_INGRESS;
	else
		params->dir_attr |= BNXT_ULP_FLOW_ATTR_EGRESS;
}

static void
bnxt_custom_ulp_init_mapper_params(struct bnxt_ulp_mapper_parms *mparms,
				   struct ulp_tc_parser_params *params,
				   enum bnxt_ulp_fdb_type flow_type)
{
	memset(mparms, 0, sizeof(*mparms));

	mparms->flow_type = flow_type;
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
	mparms->parent_flow = params->parent_flow;
	mparms->child_flow = params->child_flow;
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
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_FLOW_SIG_ID,
			    params->flow_sig_id);
}

static int
bnxt_custom_ulp_alloc_mapper_encap_cparams(struct bnxt_ulp_mapper_parms **mparms_dyn,
					   struct bnxt_ulp_mapper_parms *mparms)
{
	struct bnxt_ulp_mapper_parms *parms = NULL;

	parms = vzalloc(sizeof(*parms));
	if (!parms)
		goto err;
	memcpy(parms, mparms, sizeof(*parms));

	parms->hdr_bitmap = vzalloc(sizeof(*parms->hdr_bitmap));
	if (!parms->hdr_bitmap)
		goto err_cparm;

	parms->enc_hdr_bitmap = vzalloc(sizeof(*parms->enc_hdr_bitmap));
	if (!parms->enc_hdr_bitmap)
		goto err_hdr_bitmap;

	parms->hdr_field =  vzalloc(sizeof(*parms->hdr_field) * BNXT_ULP_PROTO_HDR_MAX);
	if (!parms->hdr_field)
		goto err_enc_hdr_bitmap;

	parms->enc_field =  vzalloc(sizeof(*parms->enc_field) * BNXT_ULP_PROTO_HDR_ENCAP_MAX);
	if (!parms->enc_field)
		goto err_hdr_field;

	parms->comp_fld = vzalloc(sizeof(*parms->comp_fld) * BNXT_ULP_CF_IDX_LAST);
	if (!parms->comp_fld)
		goto err_enc_field;

	parms->act_bitmap = vzalloc(sizeof(*parms->act_bitmap));
	if (!parms->act_bitmap)
		goto err_comp_fld;

	parms->act_prop = vzalloc(sizeof(*parms->act_prop));
	if (!parms->act_prop)
		goto err_act;

	parms->fld_bitmap = vzalloc(sizeof(*parms->fld_bitmap));
	if (!parms->fld_bitmap)
		goto err_act_prop;

	memcpy(parms->hdr_bitmap, mparms->hdr_bitmap, sizeof(*parms->hdr_bitmap));
	memcpy(parms->enc_hdr_bitmap, mparms->enc_hdr_bitmap,
	       sizeof(*parms->enc_hdr_bitmap));
	memcpy(parms->hdr_field, mparms->hdr_field,
	       sizeof(*parms->hdr_field) * BNXT_ULP_PROTO_HDR_MAX);
	memcpy(parms->enc_field, mparms->enc_field,
	       sizeof(*parms->enc_field) * BNXT_ULP_PROTO_HDR_ENCAP_MAX);
	memcpy(parms->comp_fld, mparms->comp_fld,
	       sizeof(*parms->comp_fld) * BNXT_ULP_CF_IDX_LAST);
	memcpy(parms->act_bitmap, mparms->act_bitmap, sizeof(*parms->act_bitmap));
	memcpy(parms->act_prop, mparms->act_prop, sizeof(*parms->act_prop));
	memcpy(parms->fld_bitmap, mparms->fld_bitmap, sizeof(*parms->fld_bitmap));

	*mparms_dyn = parms;
	return 0;

err_act_prop:
	vfree(parms->act_prop);
err_act:
	vfree(parms->act_bitmap);
err_comp_fld:
	vfree(parms->comp_fld);
err_enc_field:
	vfree(parms->enc_field);
err_hdr_field:
	vfree(parms->hdr_field);
err_enc_hdr_bitmap:
	vfree(parms->enc_hdr_bitmap);
err_hdr_bitmap:
	vfree(parms->hdr_bitmap);
err_cparm:
	vfree(parms);
err:
	return -ENOMEM;
}

static int ulp_rte_prsr_fld_size_validate(struct ulp_tc_parser_params *params,
					  u32 *idx, u32 size)
{
	if (params->field_idx + size >= BNXT_ULP_PROTO_HDR_MAX)
		return -EINVAL;
	*idx = params->field_idx;
	params->field_idx += size;
	return 0;
}

/* Utility function to update the field_bitmap */
static void ulp_tc_parser_field_bitmap_update(struct ulp_tc_parser_params
					      *params, u32 idx,
					      enum bnxt_ulp_prsr_action prsr_act)
{
	struct ulp_tc_hdr_field *field;

	field = &params->hdr_field[idx];
	if (ulp_bitmap_notzero(field->mask, field->size)) {
		ULP_INDEX_BITMAP_SET(params->fld_bitmap.bits, idx);
		if (!(prsr_act & ULP_PRSR_ACT_MATCH_IGNORE))
			ULP_INDEX_BITMAP_SET(params->fld_s_bitmap.bits, idx);
		/* Not exact match */
		if (!ulp_bitmap_is_ones(field->mask, field->size))
			ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_WC_MATCH, 1);
	} else {
		ULP_INDEX_BITMAP_RESET(params->fld_bitmap.bits, idx);
	}
}

#define ulp_deference_struct(x, y) ((x) ? &((x)->y) : NULL)
/* Utility function to copy field spec and masks items */
static void ulp_tc_prsr_fld_mask(struct ulp_tc_parser_params *params,
				 u32 *idx, u32 size, const void *spec_buff,
				 const void *mask_buff,
				 enum bnxt_ulp_prsr_action prsr_act)
{
	struct ulp_tc_hdr_field *field = &params->hdr_field[*idx];

	/* update the field size */
	field->size = size;

	/* copy the mask specifications only if mask is not null */
	if (!(prsr_act & ULP_PRSR_ACT_MASK_IGNORE) && mask_buff) {
		memcpy(field->mask, mask_buff, size);
		ulp_tc_parser_field_bitmap_update(params, *idx, prsr_act);
	}

	/* copy the protocol specifications only if mask is not null*/
	if (spec_buff && mask_buff && ulp_bitmap_notzero(mask_buff, size))
		memcpy(field->spec, spec_buff, size);

	/* Increment the index */
	*idx = *idx + 1;
}

/* Function to handle the update of proto header based on field values */
static void
ulp_rte_l2_proto_type_update(struct ulp_tc_parser_params *param,
			     uint16_t type, uint32_t in_flag,
			     uint32_t has_vlan, uint32_t has_vlan_mask)
{
#define ULP_RTE_ETHER_TYPE_ROE	0xfc3d

	if (type == cpu_to_be16(RTE_ETHER_TYPE_IPV4)) {
		if (in_flag) {
			ULP_BITMAP_SET(param->hdr_fp_bit.bits,
				       BNXT_ULP_HDR_BIT_I_IPV4);
			ULP_COMP_FLD_IDX_WR(param, BNXT_ULP_CF_IDX_I_L3, 1);
		} else {
			ULP_BITMAP_SET(param->hdr_fp_bit.bits,
				       BNXT_ULP_HDR_BIT_O_IPV4);
			ULP_COMP_FLD_IDX_WR(param, BNXT_ULP_CF_IDX_O_L3, 1);
		}
	} else if (type == cpu_to_be16(RTE_ETHER_TYPE_IPV6)) {
		if (in_flag) {
			ULP_BITMAP_SET(param->hdr_fp_bit.bits,
				       BNXT_ULP_HDR_BIT_I_IPV6);
			ULP_COMP_FLD_IDX_WR(param, BNXT_ULP_CF_IDX_I_L3, 1);
		} else {
			ULP_BITMAP_SET(param->hdr_fp_bit.bits,
				       BNXT_ULP_HDR_BIT_O_IPV6);
			ULP_COMP_FLD_IDX_WR(param, BNXT_ULP_CF_IDX_O_L3, 1);
		}
	} else if (type == cpu_to_be16(RTE_ETHER_TYPE_VLAN)) {
		has_vlan_mask = 1;
		has_vlan = 1;
	} else if (type == cpu_to_be16(RTE_ETHER_TYPE_ECPRI)) {
		/* Update the hdr_bitmap with eCPRI */
		ULP_BITMAP_SET(param->hdr_fp_bit.bits, BNXT_ULP_HDR_BIT_O_ECPRI);
	} else if (type == cpu_to_be16(ULP_RTE_ETHER_TYPE_ROE)) {
		/* Update the hdr_bitmap with RoE */
		ULP_BITMAP_SET(param->hdr_fp_bit.bits, BNXT_ULP_HDR_BIT_O_ROE);
	}

	if (has_vlan_mask) {
		if (in_flag) {
			ULP_COMP_FLD_IDX_WR(param, BNXT_ULP_CF_IDX_I_HAS_VTAG,
					    has_vlan);
			ULP_COMP_FLD_IDX_WR(param,
					    BNXT_ULP_CF_IDX_I_VLAN_NO_IGNORE,
					    1);
		} else {
			ULP_COMP_FLD_IDX_WR(param, BNXT_ULP_CF_IDX_O_HAS_VTAG,
					    has_vlan);
			ULP_COMP_FLD_IDX_WR(param,
					    BNXT_ULP_CF_IDX_O_VLAN_NO_IGNORE,
					    1);
		}
	}
}

/* Internal Function to identify broadcast or multicast packets */
static int32_t
ulp_rte_parser_is_bcmc_addr(const struct rte_ether_addr *eth_addr)
{
	if (rte_is_multicast_ether_addr(eth_addr))
		return 0;

	if (rte_is_broadcast_ether_addr(eth_addr)) {
		netdev_dbg(NULL, "No support for bcast addr offload\n");
		return 1;
	}
	return 0;
}

/* Function to handle the parsing of RTE Flow item Ethernet Header. */
static int32_t
ulp_rte_eth_hdr_handler(const struct rte_flow_item *item,
			struct ulp_tc_parser_params *params)
{
	const struct rte_flow_item_eth *eth_spec = item->spec;
	const struct rte_flow_item_eth *eth_mask = item->mask;
	uint32_t idx = 0, dmac_idx = 0;
	uint32_t size;
	uint16_t eth_type = 0;
	uint32_t inner_flag = 0;
	uint32_t has_vlan = 0, has_vlan_mask = 0;
	struct bnxt *bp = params->ulp_ctx->bp;

	/* Perform validations */
	if (eth_spec) {
		/* Avoid multicast and broadcast addr */
		if (ulp_rte_parser_is_bcmc_addr(&eth_spec->dst))
			return BNXT_TF_RC_PARSE_ERR;

		if (ulp_rte_parser_is_bcmc_addr(&eth_spec->src))
			return BNXT_TF_RC_PARSE_ERR;

		eth_type = eth_spec->type;
		has_vlan = eth_spec->has_vlan;
	}
	if (eth_mask) {
		eth_type &= eth_mask->type;
		has_vlan_mask = eth_mask->has_vlan;
	}

	if (ulp_rte_prsr_fld_size_validate(params, &idx,
					   BNXT_ULP_PROTO_HDR_ETH_NUM)) {
		netdev_err(bp->dev, "Error parsing protocol header\n");
		return BNXT_TF_RC_ERROR;
	}
	/*
	 * Copy the rte_flow_item for eth into hdr_field using ethernet
	 * header fields
	 */
	dmac_idx = idx;
	size = sizeof(((struct rte_flow_item_eth *)NULL)->dst.addr_bytes);
	ulp_tc_prsr_fld_mask(params, &idx, size,
			     ulp_deference_struct(eth_spec, dst.addr_bytes),
			     ulp_deference_struct(eth_mask, dst.addr_bytes),
			     ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_eth *)NULL)->src.addr_bytes);
	ulp_tc_prsr_fld_mask(params, &idx, size,
			     ulp_deference_struct(eth_spec, src.addr_bytes),
			     ulp_deference_struct(eth_mask, src.addr_bytes),
			     ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_eth *)NULL)->type);
	ulp_tc_prsr_fld_mask(params, &idx, size,
			     ulp_deference_struct(eth_spec, type),
			     ulp_deference_struct(eth_mask, type),
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
	    ULP_BITMAP_ISSET(params->hdr_bitmap.bits,
			     BNXT_ULP_HDR_BIT_O_TCP)) {
		ULP_BITMAP_SET(params->hdr_bitmap.bits, BNXT_ULP_HDR_BIT_I_ETH);
		inner_flag = 1;
	} else {
		ULP_BITMAP_SET(params->hdr_bitmap.bits, BNXT_ULP_HDR_BIT_O_ETH);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_TUN_OFF_DMAC_ID,
				    dmac_idx);
	}
	/* Update the field protocol hdr bitmap */
	ulp_rte_l2_proto_type_update(params, eth_type, inner_flag,
				     has_vlan, has_vlan_mask);

	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow item Vlan Header. */
static int32_t
ulp_rte_vlan_hdr_handler(const struct rte_flow_item *item,
			 struct ulp_tc_parser_params *params)
{
	const struct rte_flow_item_vlan *vlan_spec = item->spec;
	const struct rte_flow_item_vlan *vlan_mask = item->mask;
	struct ulp_tc_hdr_bitmap	*hdr_bit;
	uint32_t idx = 0;
	uint16_t vlan_tag = 0, priority = 0;
	uint16_t vlan_tag_mask = 0, priority_mask = 0;
	uint32_t outer_vtag_num;
	uint32_t inner_vtag_num;
	uint16_t eth_type = 0;
	uint32_t inner_flag = 0;
	uint32_t size;
	struct bnxt *bp = params->ulp_ctx->bp;

	if (vlan_spec) {
		vlan_tag = ntohs(vlan_spec->tci);
		priority = htons(vlan_tag >> ULP_VLAN_PRIORITY_SHIFT);
		vlan_tag &= ULP_VLAN_TAG_MASK;
		vlan_tag = htons(vlan_tag);
		eth_type = vlan_spec->inner_type;
	}

	if (vlan_mask) {
		vlan_tag_mask = ntohs(vlan_mask->tci);
		priority_mask = htons(vlan_tag_mask >> ULP_VLAN_PRIORITY_SHIFT);
		vlan_tag_mask &= 0xfff;

		/*
		 * the storage for priority and vlan tag is 2 bytes
		 * The mask of priority which is 3 bits if it is all 1's
		 * then make the rest bits 13 bits as 1's
		 * so that it is matched as exact match.
		 */
		if (priority_mask == ULP_VLAN_PRIORITY_MASK)
			priority_mask |= ~ULP_VLAN_PRIORITY_MASK;
		if (vlan_tag_mask == ULP_VLAN_TAG_MASK)
			vlan_tag_mask |= ~ULP_VLAN_TAG_MASK;
		vlan_tag_mask = htons(vlan_tag_mask);
	}

	if (ulp_rte_prsr_fld_size_validate(params, &idx,
					   BNXT_ULP_PROTO_HDR_S_VLAN_NUM)) {
		netdev_err(bp->dev, "Error parsing protocol header\n");
		return BNXT_TF_RC_ERROR;
	}

	/*
	 * Copy the rte_flow_item for vlan into hdr_field using Vlan
	 * header fields
	 */
	size = sizeof(((struct rte_flow_item_vlan *)NULL)->tci);
	/*
	 * The priority field is ignored since OVS is setting it as
	 * wild card match and it is not supported. This is a work
	 * around and shall be addressed in the future.
	 */
	ulp_tc_prsr_fld_mask(params, &idx, size,
			     &priority,
			     (vlan_mask) ? &priority_mask : NULL,
			     ULP_PRSR_ACT_MASK_IGNORE);

	ulp_tc_prsr_fld_mask(params, &idx, size,
			     &vlan_tag,
			     (vlan_mask) ? &vlan_tag_mask : NULL,
			     ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_vlan *)NULL)->inner_type);
	ulp_tc_prsr_fld_mask(params, &idx, size,
			     ulp_deference_struct(vlan_spec, inner_type),
			     ulp_deference_struct(vlan_mask, inner_type),
			     ULP_PRSR_ACT_MATCH_IGNORE);

	/* Get the outer tag and inner tag counts */
	outer_vtag_num = ULP_COMP_FLD_IDX_RD(params,
					     BNXT_ULP_CF_IDX_O_VTAG_NUM);
	inner_vtag_num = ULP_COMP_FLD_IDX_RD(params,
					     BNXT_ULP_CF_IDX_I_VTAG_NUM);

	/* Update the hdr_bitmap of the vlans */
	hdr_bit = &params->hdr_bitmap;
	if (ULP_BITMAP_ISSET(hdr_bit->bits, BNXT_ULP_HDR_BIT_O_ETH) &&
	    !ULP_BITMAP_ISSET(hdr_bit->bits, BNXT_ULP_HDR_BIT_I_ETH) &&
	    !outer_vtag_num) {
		/* Update the vlan tag num */
		outer_vtag_num++;
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_VTAG_NUM,
				    outer_vtag_num);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_HAS_VTAG, 1);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_ONE_VTAG, 1);
		ULP_BITMAP_SET(params->hdr_bitmap.bits,
			       BNXT_ULP_HDR_BIT_OO_VLAN);
		if (vlan_mask && vlan_tag_mask)
			ULP_COMP_FLD_IDX_WR(params,
					    BNXT_ULP_CF_IDX_OO_VLAN_FB_VID, 1);

	} else if (ULP_BITMAP_ISSET(hdr_bit->bits, BNXT_ULP_HDR_BIT_O_ETH) &&
		   !ULP_BITMAP_ISSET(hdr_bit->bits, BNXT_ULP_HDR_BIT_I_ETH) &&
		   outer_vtag_num == 1) {
		/* update the vlan tag num */
		outer_vtag_num++;
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_VTAG_NUM,
				    outer_vtag_num);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_TWO_VTAGS, 1);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_ONE_VTAG, 0);
		ULP_BITMAP_SET(params->hdr_bitmap.bits,
			       BNXT_ULP_HDR_BIT_OI_VLAN);
		if (vlan_mask && vlan_tag_mask)
			ULP_COMP_FLD_IDX_WR(params,
					    BNXT_ULP_CF_IDX_OI_VLAN_FB_VID, 1);

	} else if (ULP_BITMAP_ISSET(hdr_bit->bits, BNXT_ULP_HDR_BIT_O_ETH) &&
		   ULP_BITMAP_ISSET(hdr_bit->bits, BNXT_ULP_HDR_BIT_I_ETH) &&
		   !inner_vtag_num) {
		/* update the vlan tag num */
		inner_vtag_num++;
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_I_VTAG_NUM,
				    inner_vtag_num);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_I_HAS_VTAG, 1);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_I_ONE_VTAG, 1);
		ULP_BITMAP_SET(params->hdr_bitmap.bits,
			       BNXT_ULP_HDR_BIT_IO_VLAN);
		if (vlan_mask && vlan_tag_mask)
			ULP_COMP_FLD_IDX_WR(params,
					    BNXT_ULP_CF_IDX_IO_VLAN_FB_VID, 1);
		inner_flag = 1;
	} else if (ULP_BITMAP_ISSET(hdr_bit->bits, BNXT_ULP_HDR_BIT_O_ETH) &&
		   ULP_BITMAP_ISSET(hdr_bit->bits, BNXT_ULP_HDR_BIT_I_ETH) &&
		   inner_vtag_num == 1) {
		/* update the vlan tag num */
		inner_vtag_num++;
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_I_VTAG_NUM,
				    inner_vtag_num);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_I_TWO_VTAGS, 1);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_I_ONE_VTAG, 0);
		ULP_BITMAP_SET(params->hdr_bitmap.bits,
			       BNXT_ULP_HDR_BIT_II_VLAN);
		if (vlan_mask && vlan_tag_mask)
			ULP_COMP_FLD_IDX_WR(params,
					    BNXT_ULP_CF_IDX_II_VLAN_FB_VID, 1);
		inner_flag = 1;
	} else {
		netdev_err(bp->dev, "Error Parsing:Vlan hdr found without eth\n");
		return BNXT_TF_RC_ERROR;
	}
	/* Update the field protocol hdr bitmap */
	ulp_rte_l2_proto_type_update(params, eth_type, inner_flag, 1, 1);
	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the update of proto header based on field values */
static void
ulp_rte_l3_proto_type_update(struct ulp_tc_parser_params *param,
			     uint8_t proto, uint32_t in_flag)
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

/* Function to handle the parsing of RTE Flow item IPV4 Header. */
static int32_t
ulp_rte_ipv4_hdr_handler(const struct rte_flow_item *item,
			 struct ulp_tc_parser_params *params)
{
	const struct rte_flow_item_ipv4 *ipv4_spec = item->spec;
	const struct rte_flow_item_ipv4 *ipv4_mask = item->mask;
	struct ulp_tc_hdr_bitmap *hdr_bitmap = &params->hdr_bitmap;
	uint32_t idx = 0, dip_idx = 0;
	uint32_t size;
	uint8_t proto = 0;
	uint8_t proto_mask = 0;
	uint32_t inner_flag = 0;
	uint32_t cnt;
	struct bnxt *bp = params->ulp_ctx->bp;

	/* validate there are no 3rd L3 header */
	cnt = ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_L3_HDR_CNT);
	if (cnt == 2) {
		netdev_err(bp->dev, "Parse Err:Third L3 header not supported\n");
		return BNXT_TF_RC_ERROR;
	}

	if (ulp_rte_prsr_fld_size_validate(params, &idx,
					   BNXT_ULP_PROTO_HDR_IPV4_NUM)) {
		netdev_err(bp->dev, "Error parsing protocol header\n");
		return BNXT_TF_RC_ERROR;
	}

	/*
	 * Copy the rte_flow_item for ipv4 into hdr_field using ipv4
	 * header fields
	 */
	size = sizeof(((struct rte_flow_item_ipv4 *)NULL)->hdr.version_ihl);
	ulp_tc_prsr_fld_mask(params, &idx, size,
			     ulp_deference_struct(ipv4_spec, hdr.version_ihl),
			     ulp_deference_struct(ipv4_mask, hdr.version_ihl),
			     ULP_PRSR_ACT_DEFAULT);

	/*
	 * The tos field is ignored since OVS is setting it as wild card
	 * match and it is not supported. An application can enable tos support.
	 */
	size = sizeof(((struct rte_flow_item_ipv4 *)NULL)->hdr.type_of_service);
	ulp_tc_prsr_fld_mask(params, &idx, size,
			     ulp_deference_struct(ipv4_spec, hdr.type_of_service),
			     ulp_deference_struct(ipv4_mask, hdr.type_of_service),
			     ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_ipv4 *)NULL)->hdr.total_length);
	ulp_tc_prsr_fld_mask(params, &idx, size,
			     ulp_deference_struct(ipv4_spec, hdr.total_length),
			     ulp_deference_struct(ipv4_mask, hdr.total_length),
			     ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_ipv4 *)NULL)->hdr.packet_id);
	ulp_tc_prsr_fld_mask(params, &idx, size,
			     ulp_deference_struct(ipv4_spec, hdr.packet_id),
			     ulp_deference_struct(ipv4_mask, hdr.packet_id),
			     ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_ipv4 *)NULL)->hdr.fragment_offset);
	ulp_tc_prsr_fld_mask(params, &idx, size,
			     ulp_deference_struct(ipv4_spec, hdr.fragment_offset),
			     ulp_deference_struct(ipv4_mask, hdr.fragment_offset),
			     ULP_PRSR_ACT_MASK_IGNORE);

	size = sizeof(((struct rte_flow_item_ipv4 *)NULL)->hdr.time_to_live);
	ulp_tc_prsr_fld_mask(params, &idx, size,
			     ulp_deference_struct(ipv4_spec, hdr.time_to_live),
			     ulp_deference_struct(ipv4_mask, hdr.time_to_live),
			     ULP_PRSR_ACT_DEFAULT);

	/* Ignore proto for matching templates */
	size = sizeof(((struct rte_flow_item_ipv4 *)NULL)->hdr.next_proto_id);
	ulp_tc_prsr_fld_mask(params, &idx, size,
			     ulp_deference_struct(ipv4_spec, hdr.next_proto_id),
			     ulp_deference_struct(ipv4_mask, hdr.next_proto_id),
			     (ULP_APP_TOS_PROTO_SUPPORT(params->ulp_ctx)) ?
			      ULP_PRSR_ACT_DEFAULT : ULP_PRSR_ACT_MATCH_IGNORE);

	if (ipv4_spec)
		proto = ipv4_spec->hdr.next_proto_id;

	size = sizeof(((struct rte_flow_item_ipv4 *)NULL)->hdr.hdr_checksum);
	ulp_tc_prsr_fld_mask(params, &idx, size,
			     ulp_deference_struct(ipv4_spec, hdr.hdr_checksum),
			     ulp_deference_struct(ipv4_mask, hdr.hdr_checksum),
			     ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_ipv4 *)NULL)->hdr.src_addr);
	ulp_tc_prsr_fld_mask(params, &idx, size,
			     ulp_deference_struct(ipv4_spec, hdr.src_addr),
			     ulp_deference_struct(ipv4_mask, hdr.src_addr),
			     ULP_PRSR_ACT_DEFAULT);

	dip_idx = idx;
	size = sizeof(((struct rte_flow_item_ipv4 *)NULL)->hdr.dst_addr);
	ulp_tc_prsr_fld_mask(params, &idx, size,
			     ulp_deference_struct(ipv4_spec, hdr.dst_addr),
			     ulp_deference_struct(ipv4_mask, hdr.dst_addr),
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
		/* Update the tunnel offload dest ip offset */
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_TUN_OFF_DIP_ID,
				    dip_idx);
	}

	/* Some of the PMD applications may set the protocol field
	 * in the IPv4 spec but don't set the mask. So, consider
	 * the mask in the proto value calculation.
	 */
	if (ipv4_mask) {
		proto &= ipv4_mask->hdr.next_proto_id;
		proto_mask = ipv4_mask->hdr.next_proto_id;
	}

	/* Update the field protocol hdr bitmap */
	if (proto_mask)
		ulp_rte_l3_proto_type_update(params, proto, inner_flag);
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_L3_HDR_CNT, ++cnt);
	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow item IPV6 Header */
static int32_t
ulp_rte_ipv6_hdr_handler(const struct rte_flow_item *item,
			 struct ulp_tc_parser_params *params)
{
	const struct rte_flow_item_ipv6	*ipv6_spec = item->spec;
	const struct rte_flow_item_ipv6	*ipv6_mask = item->mask;
	struct ulp_tc_hdr_bitmap *hdr_bitmap = &params->hdr_bitmap;
	uint32_t idx = 0, dip_idx = 0;
	uint32_t size, vtc_flow;
	uint32_t ver_spec = 0, ver_mask = 0;
	uint32_t tc_spec = 0, tc_mask = 0;
	uint32_t lab_spec = 0, lab_mask = 0;
	uint8_t proto = 0;
	uint8_t proto_mask = 0;
	uint32_t inner_flag = 0;
	uint32_t cnt;
	struct bnxt *bp = params->ulp_ctx->bp;

	/* validate there are no 3rd L3 header */
	cnt = ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_L3_HDR_CNT);
	if (cnt == 2) {
		netdev_err(bp->dev, "Parse Err:Third L3 header not supported\n");
		return BNXT_TF_RC_ERROR;
	}

	if (ulp_rte_prsr_fld_size_validate(params, &idx,
					   BNXT_ULP_PROTO_HDR_IPV6_NUM)) {
		netdev_err(bp->dev, "Error parsing protocol header\n");
		return BNXT_TF_RC_ERROR;
	}

	/*
	 * Copy the rte_flow_item for ipv6 into hdr_field using ipv6
	 * header fields
	 */
	if (ipv6_spec) {
		vtc_flow = ntohl(ipv6_spec->hdr.vtc_flow);
		ver_spec = htonl(BNXT_ULP_GET_IPV6_VER(vtc_flow));
		tc_spec = htonl(BNXT_ULP_GET_IPV6_TC(vtc_flow));
		lab_spec = htonl(BNXT_ULP_GET_IPV6_FLOWLABEL(vtc_flow));
		proto = ipv6_spec->hdr.proto;
	}

	if (ipv6_mask) {
		vtc_flow = ntohl(ipv6_mask->hdr.vtc_flow);
		ver_mask = htonl(BNXT_ULP_GET_IPV6_VER(vtc_flow));
		tc_mask = htonl(BNXT_ULP_GET_IPV6_TC(vtc_flow));
		lab_mask = htonl(BNXT_ULP_GET_IPV6_FLOWLABEL(vtc_flow));

		/* Some of the PMD applications may set the protocol field
		 * in the IPv6 spec but don't set the mask. So, consider
		 * the mask in proto value calculation.
		 */
		proto &= ipv6_mask->hdr.proto;
		proto_mask = ipv6_mask->hdr.proto;
	}

	size = sizeof(((struct rte_flow_item_ipv6 *)NULL)->hdr.vtc_flow);
	ulp_tc_prsr_fld_mask(params, &idx, size, &ver_spec, &ver_mask,
			     ULP_PRSR_ACT_DEFAULT);
	/*
	 * The TC and flow label field are ignored since OVS is
	 * setting it for match and it is not supported.
	 * This is a work around and
	 * shall be addressed in the future.
	 */
	ulp_tc_prsr_fld_mask(params, &idx, size, &tc_spec, &tc_mask,
			     (ULP_APP_TOS_PROTO_SUPPORT(params->ulp_ctx)) ?
			      ULP_PRSR_ACT_DEFAULT : ULP_PRSR_ACT_MASK_IGNORE);
	ulp_tc_prsr_fld_mask(params, &idx, size, &lab_spec, &lab_mask,
			     ULP_PRSR_ACT_MASK_IGNORE);

	size = sizeof(((struct rte_flow_item_ipv6 *)NULL)->hdr.payload_len);
	ulp_tc_prsr_fld_mask(params, &idx, size,
			     ulp_deference_struct(ipv6_spec, hdr.payload_len),
			     ulp_deference_struct(ipv6_mask, hdr.payload_len),
			     ULP_PRSR_ACT_DEFAULT);

	/* Ignore proto for template matching */
	size = sizeof(((struct rte_flow_item_ipv6 *)NULL)->hdr.proto);
	ulp_tc_prsr_fld_mask(params, &idx, size,
			     ulp_deference_struct(ipv6_spec, hdr.proto),
			     ulp_deference_struct(ipv6_mask, hdr.proto),
			     (ULP_APP_TOS_PROTO_SUPPORT(params->ulp_ctx)) ?
			      ULP_PRSR_ACT_DEFAULT : ULP_PRSR_ACT_MATCH_IGNORE);

	size = sizeof(((struct rte_flow_item_ipv6 *)NULL)->hdr.hop_limits);
	ulp_tc_prsr_fld_mask(params, &idx, size,
			     ulp_deference_struct(ipv6_spec, hdr.hop_limits),
			     ulp_deference_struct(ipv6_mask, hdr.hop_limits),
			     ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_ipv6 *)NULL)->hdr.src_addr);
	ulp_tc_prsr_fld_mask(params, &idx, size,
			     ulp_deference_struct(ipv6_spec, hdr.src_addr),
			     ulp_deference_struct(ipv6_mask, hdr.src_addr),
			     ULP_PRSR_ACT_DEFAULT);

	dip_idx =  idx;
	size = sizeof(((struct rte_flow_item_ipv6 *)NULL)->hdr.dst_addr);
	ulp_tc_prsr_fld_mask(params, &idx, size,
			     ulp_deference_struct(ipv6_spec, hdr.dst_addr),
			     ulp_deference_struct(ipv6_mask, hdr.dst_addr),
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
		ulp_rte_l3_proto_type_update(params, proto, inner_flag);
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_L3_HDR_CNT, ++cnt);

	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the update of proto header based on field values */
static void
ulp_rte_l4_proto_type_update(struct ulp_tc_parser_params *params,
			     uint16_t src_port, uint16_t src_mask,
			     uint16_t dst_port, uint16_t dst_mask,
			     enum bnxt_ulp_hdr_bit hdr_bit)
{
	switch (hdr_bit) {
	case BNXT_ULP_HDR_BIT_I_UDP:
	case BNXT_ULP_HDR_BIT_I_TCP:
		ULP_BITMAP_SET(params->hdr_bitmap.bits, hdr_bit);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_I_L4, 1);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_I_L4_SRC_PORT,
				    (uint64_t)be16_to_cpu(src_port));
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_I_L4_DST_PORT,
				    (uint64_t)be16_to_cpu(dst_port));
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_I_L4_SRC_PORT_MASK,
				    (uint64_t)be16_to_cpu(src_mask));
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_I_L4_DST_PORT_MASK,
				    (uint64_t)be16_to_cpu(dst_mask));
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
				    (uint64_t)be16_to_cpu(src_port));
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_L4_DST_PORT,
				    (uint64_t)be16_to_cpu(dst_port));
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_L4_SRC_PORT_MASK,
				    (uint64_t)be16_to_cpu(src_mask));
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_L4_DST_PORT_MASK,
				    (uint64_t)be16_to_cpu(dst_mask));
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

	if (hdr_bit == BNXT_ULP_HDR_BIT_O_UDP && dst_port ==
	    cpu_to_be16(ULP_UDP_PORT_VXLAN)) {
		ULP_BITMAP_SET(params->hdr_fp_bit.bits,
			       BNXT_ULP_HDR_BIT_T_VXLAN);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_L3_TUN, 1);
		ULP_BITMAP_SET(params->cf_bitmap, BNXT_ULP_CF_BIT_IS_TUNNEL);
	}
}

/* Function to handle the parsing of RTE Flow item UDP Header. */
static int32_t
ulp_rte_udp_hdr_handler(const struct rte_flow_item *item,
			struct ulp_tc_parser_params *params)
{
	const struct rte_flow_item_udp *udp_spec = item->spec;
	const struct rte_flow_item_udp *udp_mask = item->mask;
	struct ulp_tc_hdr_bitmap *hdr_bitmap = &params->hdr_bitmap;
	uint32_t idx = 0;
	uint32_t size;
	uint16_t dport = 0, sport = 0;
	uint16_t dport_mask = 0, sport_mask = 0;
	uint32_t cnt;
	enum bnxt_ulp_hdr_bit out_l4 = BNXT_ULP_HDR_BIT_O_UDP;
	struct bnxt *bp = params->ulp_ctx->bp;

	cnt = ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_L4_HDR_CNT);
	if (cnt == 2) {
		netdev_err(bp->dev, "Parse Err:Third L4 header not supported\n");
		return BNXT_TF_RC_ERROR;
	}

	if (udp_spec) {
		sport = udp_spec->hdr.src_port;
		dport = udp_spec->hdr.dst_port;
	}
	if (udp_mask) {
		sport_mask = udp_mask->hdr.src_port;
		dport_mask = udp_mask->hdr.dst_port;
	}

	if (ulp_rte_prsr_fld_size_validate(params, &idx,
					   BNXT_ULP_PROTO_HDR_UDP_NUM)) {
		netdev_err(bp->dev, "Error parsing protocol header\n");
		return BNXT_TF_RC_ERROR;
	}

	/*
	 * Copy the rte_flow_item for ipv4 into hdr_field using ipv4
	 * header fields
	 */
	size = sizeof(((struct rte_flow_item_udp *)NULL)->hdr.src_port);
	ulp_tc_prsr_fld_mask(params, &idx, size,
			     ulp_deference_struct(udp_spec, hdr.src_port),
			     ulp_deference_struct(udp_mask, hdr.src_port),
			     ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_udp *)NULL)->hdr.dst_port);
	ulp_tc_prsr_fld_mask(params, &idx, size,
			     ulp_deference_struct(udp_spec, hdr.dst_port),
			     ulp_deference_struct(udp_mask, hdr.dst_port),
			     ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_udp *)NULL)->hdr.dgram_len);
	ulp_tc_prsr_fld_mask(params, &idx, size,
			     ulp_deference_struct(udp_spec, hdr.dgram_len),
			     ulp_deference_struct(udp_mask, hdr.dgram_len),
			     ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_udp *)NULL)->hdr.dgram_cksum);
	ulp_tc_prsr_fld_mask(params, &idx, size,
			     ulp_deference_struct(udp_spec, hdr.dgram_cksum),
			     ulp_deference_struct(udp_mask, hdr.dgram_cksum),
			     ULP_PRSR_ACT_DEFAULT);

	/* Set the udp header bitmap and computed l4 header bitmaps */
	if (ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_UDP) ||
	    ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_TCP) ||
	    ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_L3_TUN))
		out_l4 = BNXT_ULP_HDR_BIT_I_UDP;

	ulp_rte_l4_proto_type_update(params, sport, sport_mask, dport,
				     dport_mask, out_l4);
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_L4_HDR_CNT, ++cnt);
	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow item TCP Header. */
static int32_t
ulp_rte_tcp_hdr_handler(const struct rte_flow_item *item,
			struct ulp_tc_parser_params *params)
{
	const struct rte_flow_item_tcp *tcp_spec = item->spec;
	const struct rte_flow_item_tcp *tcp_mask = item->mask;
	struct ulp_tc_hdr_bitmap *hdr_bitmap = &params->hdr_bitmap;
	uint32_t idx = 0;
	uint16_t dport = 0, sport = 0;
	uint16_t dport_mask = 0, sport_mask = 0;
	uint32_t size;
	uint32_t cnt;
	enum bnxt_ulp_hdr_bit out_l4 = BNXT_ULP_HDR_BIT_O_TCP;
	struct bnxt *bp = params->ulp_ctx->bp;

	cnt = ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_L4_HDR_CNT);
	if (cnt == 2) {
		netdev_err(bp->dev, "Parse Err:Third L4 header not supported\n");
		return BNXT_TF_RC_ERROR;
	}

	if (tcp_spec) {
		sport = tcp_spec->hdr.src_port;
		dport = tcp_spec->hdr.dst_port;
	}
	if (tcp_mask) {
		sport_mask = tcp_mask->hdr.src_port;
		dport_mask = tcp_mask->hdr.dst_port;
	}

	if (ulp_rte_prsr_fld_size_validate(params, &idx,
					   BNXT_ULP_PROTO_HDR_TCP_NUM)) {
		netdev_err(bp->dev, "Error parsing protocol header\n");
		return BNXT_TF_RC_ERROR;
	}

	/*
	 * Copy the rte_flow_item for ipv4 into hdr_field using ipv4
	 * header fields
	 */
	size = sizeof(((struct rte_flow_item_tcp *)NULL)->hdr.src_port);
	ulp_tc_prsr_fld_mask(params, &idx, size,
			     ulp_deference_struct(tcp_spec, hdr.src_port),
			     ulp_deference_struct(tcp_mask, hdr.src_port),
			     ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_tcp *)NULL)->hdr.dst_port);
	ulp_tc_prsr_fld_mask(params, &idx, size,
			     ulp_deference_struct(tcp_spec, hdr.dst_port),
			     ulp_deference_struct(tcp_mask, hdr.dst_port),
			     ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_tcp *)NULL)->hdr.sent_seq);
	ulp_tc_prsr_fld_mask(params, &idx, size,
			     ulp_deference_struct(tcp_spec, hdr.sent_seq),
			     ulp_deference_struct(tcp_mask, hdr.sent_seq),
			     ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_tcp *)NULL)->hdr.recv_ack);
	ulp_tc_prsr_fld_mask(params, &idx, size,
			     ulp_deference_struct(tcp_spec, hdr.recv_ack),
			     ulp_deference_struct(tcp_mask, hdr.recv_ack),
			     ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_tcp *)NULL)->hdr.data_off);
	ulp_tc_prsr_fld_mask(params, &idx, size,
			     ulp_deference_struct(tcp_spec, hdr.data_off),
			     ulp_deference_struct(tcp_mask, hdr.data_off),
			     ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_tcp *)NULL)->hdr.tcp_flags);
	ulp_tc_prsr_fld_mask(params, &idx, size,
			     ulp_deference_struct(tcp_spec, hdr.tcp_flags),
			     ulp_deference_struct(tcp_mask, hdr.tcp_flags),
			     ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_tcp *)NULL)->hdr.rx_win);
	ulp_tc_prsr_fld_mask(params, &idx, size,
			     ulp_deference_struct(tcp_spec, hdr.rx_win),
			     ulp_deference_struct(tcp_mask, hdr.rx_win),
			     ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_tcp *)NULL)->hdr.cksum);
	ulp_tc_prsr_fld_mask(params, &idx, size,
			     ulp_deference_struct(tcp_spec, hdr.cksum),
			     ulp_deference_struct(tcp_mask, hdr.cksum),
			     ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_tcp *)NULL)->hdr.tcp_urp);
	ulp_tc_prsr_fld_mask(params, &idx, size,
			     ulp_deference_struct(tcp_spec, hdr.tcp_urp),
			     ulp_deference_struct(tcp_mask, hdr.tcp_urp),
			     ULP_PRSR_ACT_DEFAULT);

	/* Set the udp header bitmap and computed l4 header bitmaps */
	if (ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_UDP) ||
	    ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_TCP) ||
	    ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_L3_TUN))
		out_l4 = BNXT_ULP_HDR_BIT_I_TCP;

	ulp_rte_l4_proto_type_update(params, sport, sport_mask, dport,
				     dport_mask, out_l4);
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_L4_HDR_CNT, ++cnt);
	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow item Vxlan Header. */
static int32_t
ulp_rte_vxlan_hdr_handler(const struct rte_flow_item *item,
			  struct ulp_tc_parser_params *params)
{
	const struct rte_flow_item_vxlan *vxlan_spec = item->spec;
	const struct rte_flow_item_vxlan *vxlan_mask = item->mask;
	struct ulp_tc_hdr_bitmap *hdr_bitmap = &params->hdr_bitmap;
	struct bnxt_ulp_context *ulp_ctx = params->ulp_ctx;
	struct bnxt *bp = params->ulp_ctx->bp;
	uint16_t dport, stat_port;
	uint32_t idx = 0;
	uint32_t size;

	if (ulp_rte_prsr_fld_size_validate(params, &idx,
					   BNXT_ULP_PROTO_HDR_VXLAN_NUM)) {
		netdev_err(bp->dev, "Error parsing protocol header\n");
		return BNXT_TF_RC_ERROR;
	}

	/* Update if the outer headers have any partial masks */
	if (!ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_WC_MATCH))
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_OUTER_EM_ONLY, 1);

	/*
	 * Copy the rte_flow_item for vxlan into hdr_field using vxlan
	 * header fields
	 */
	size = sizeof(((struct rte_flow_item_vxlan *)NULL)->flags);
	ulp_tc_prsr_fld_mask(params, &idx, size,
			     ulp_deference_struct(vxlan_spec, flags),
			     ulp_deference_struct(vxlan_mask, flags),
			     ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_vxlan *)NULL)->rsvd0);
	ulp_tc_prsr_fld_mask(params, &idx, size,
			     ulp_deference_struct(vxlan_spec, rsvd0),
			     ulp_deference_struct(vxlan_mask, rsvd0),
			     ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_vxlan *)NULL)->vni);
	ulp_tc_prsr_fld_mask(params, &idx, size,
			     ulp_deference_struct(vxlan_spec, vni),
			     ulp_deference_struct(vxlan_mask, vni),
			     ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_vxlan *)NULL)->rsvd1);
	ulp_tc_prsr_fld_mask(params, &idx, size,
			     ulp_deference_struct(vxlan_spec, rsvd1),
			     ulp_deference_struct(vxlan_mask, rsvd1),
			     ULP_PRSR_ACT_DEFAULT);

	/* Update the hdr_bitmap with vxlan */
	ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_T_VXLAN);
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_L3_TUN, 1);
	ULP_BITMAP_SET(params->cf_bitmap, BNXT_ULP_CF_BIT_IS_TUNNEL);

	/* if l4 protocol header updated it then reset it */
	ULP_BITMAP_RESET(params->hdr_fp_bit.bits, BNXT_ULP_HDR_BIT_T_VXLAN_GPE);

	dport = ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_O_L4_DST_PORT);
	if (!dport) {
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_L4_DST_PORT,
				    ULP_UDP_PORT_VXLAN);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_L4_DST_PORT_MASK,
				    ULP_UDP_PORT_VXLAN_MASK);
	}

	/* vxlan static customized port */
	if (ULP_APP_STATIC_VXLAN_PORT_EN(ulp_ctx)) {
		stat_port = bnxt_ulp_cntxt_vxlan_ip_port_get(ulp_ctx);
		if (!stat_port)
			stat_port = bnxt_ulp_cntxt_vxlan_port_get(ulp_ctx);

		/* validate that static ports match if not reject */
		if (dport != 0 && dport != cpu_to_be16(stat_port)) {
			netdev_dbg(ulp_ctx->bp->dev,
				   "ParseErr:vxlan port is not valid\n");
			return BNXT_TF_RC_PARSE_ERR;
		} else if (dport == 0) {
			ULP_COMP_FLD_IDX_WR(params,
					    BNXT_ULP_CF_IDX_TUNNEL_PORT,
					    cpu_to_be16(stat_port));
		}
	} else {
		/* dynamic vxlan support */
		if (ULP_APP_DYNAMIC_VXLAN_PORT_EN(params->ulp_ctx)) {
			if (dport == 0) {
				netdev_dbg(ulp_ctx->bp->dev,
					   "ParseErr:vxlan port is null\n");
				return BNXT_TF_RC_PARSE_ERR;
			}
			/* set the dynamic vxlan port check */
			ULP_BITMAP_SET(params->cf_bitmap,
				       BNXT_ULP_CF_BIT_DYNAMIC_VXLAN_PORT);
			ULP_COMP_FLD_IDX_WR(params,
					    BNXT_ULP_CF_IDX_TUNNEL_PORT, dport);
		} else if (dport != 0 && dport != BNXT_ULP_GEN_UDP_PORT_VXLAN) {
			/* set the dynamic vxlan port check */
			ULP_BITMAP_SET(params->cf_bitmap,
				       BNXT_ULP_CF_BIT_DYNAMIC_VXLAN_PORT);
			ULP_COMP_FLD_IDX_WR(params,
					    BNXT_ULP_CF_IDX_TUNNEL_PORT, dport);
		}
	}
	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow item GRE Header. */
static int32_t
ulp_rte_gre_hdr_handler(const struct rte_flow_item *item,
			struct ulp_tc_parser_params *params)
{
	const struct rte_flow_item_gre *gre_spec = item->spec;
	const struct rte_flow_item_gre *gre_mask = item->mask;
	struct ulp_tc_hdr_bitmap *hdr_bitmap = &params->hdr_bitmap;
	uint32_t idx = 0;
	uint32_t size;
	struct bnxt *bp = params->ulp_ctx->bp;

	if (ulp_rte_prsr_fld_size_validate(params, &idx,
					   BNXT_ULP_PROTO_HDR_GRE_NUM)) {
		netdev_err(bp->dev, "Error parsing protocol header\n");
		return BNXT_TF_RC_ERROR;
	}

	size = sizeof(((struct rte_flow_item_gre *)NULL)->c_rsvd0_ver);
	ulp_tc_prsr_fld_mask(params, &idx, size,
			     ulp_deference_struct(gre_spec, c_rsvd0_ver),
			     ulp_deference_struct(gre_mask, c_rsvd0_ver),
			     ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_gre *)NULL)->protocol);
	ulp_tc_prsr_fld_mask(params, &idx, size,
			     ulp_deference_struct(gre_spec, protocol),
			     ulp_deference_struct(gre_mask, protocol),
			     ULP_PRSR_ACT_DEFAULT);

	/* Update the hdr_bitmap with GRE */
	ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_T_GRE);
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_L3_TUN, 1);
	ULP_BITMAP_SET(params->cf_bitmap, BNXT_ULP_CF_BIT_IS_TUNNEL);
	return BNXT_TF_RC_SUCCESS;
}

/*
 * This table has to be indexed based on the rte_flow_item_type that is part of
 * DPDK. The below array is list of parsing functions for each of the flow items
 * that are supported.
 */
struct bnxt_ulp_rte_hdr_info rte_ulp_hdr_info[] = {
	[RTE_FLOW_ITEM_TYPE_END] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_END,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_VOID] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_INVERT] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_ANY] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_PF] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_VF] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_PHY_PORT] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_PORT_ID] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_RAW] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_ETH] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_SUPPORTED,
	.proto_hdr_func          = ulp_rte_eth_hdr_handler
	},
	[RTE_FLOW_ITEM_TYPE_VLAN] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_SUPPORTED,
	.proto_hdr_func          = ulp_rte_vlan_hdr_handler
	},
	[RTE_FLOW_ITEM_TYPE_IPV4] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_SUPPORTED,
	.proto_hdr_func          = ulp_rte_ipv4_hdr_handler
	},
	[RTE_FLOW_ITEM_TYPE_IPV6] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_SUPPORTED,
	.proto_hdr_func          = ulp_rte_ipv6_hdr_handler
	},
	[RTE_FLOW_ITEM_TYPE_ICMP] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_UDP] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_SUPPORTED,
	.proto_hdr_func          = ulp_rte_udp_hdr_handler
	},
	[RTE_FLOW_ITEM_TYPE_TCP] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_SUPPORTED,
	.proto_hdr_func          = ulp_rte_tcp_hdr_handler
	},
	[RTE_FLOW_ITEM_TYPE_SCTP] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_VXLAN] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_SUPPORTED,
	.proto_hdr_func          = ulp_rte_vxlan_hdr_handler
	},
	[RTE_FLOW_ITEM_TYPE_E_TAG] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_NVGRE] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_MPLS] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_GRE] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_SUPPORTED,
	.proto_hdr_func          = ulp_rte_gre_hdr_handler
	},
	[RTE_FLOW_ITEM_TYPE_FUZZY] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_GTP] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_GTPC] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_GTPU] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_ESP] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_GENEVE] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_VXLAN_GPE] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_ARP_ETH_IPV4] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_IPV6_EXT] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_IPV6_ROUTE_EXT] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_ICMP6] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_ICMP6_ND_NS] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_ICMP6_ND_NA] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_ICMP6_ND_OPT] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_ICMP6_ND_OPT_SLA_ETH] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_ICMP6_ND_OPT_TLA_ETH] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_MARK] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_META] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_GRE_KEY] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_GTP_PSC] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_PPPOES] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_PPPOED] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_PPPOE_PROTO_ID] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_NSH] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_IGMP] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_AH] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_HIGIG2] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_ECPRI] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_SUPPORTED,
	.proto_hdr_func          = NULL
	},
};

/*
 * Function to handle the parsing of RTE Flows and placing
 * the RTE flow items into the ulp structures.
 */
static int32_t
bnxt_ulp_custom_tc_parser_hdr_parse(struct bnxt *bp,
				    const struct rte_flow_item pattern[],
				    struct ulp_tc_parser_params *params)
{
	const struct rte_flow_item *item = pattern;
	struct bnxt_ulp_rte_hdr_info *hdr_info;

	params->field_idx = BNXT_ULP_PROTO_HDR_SVIF_NUM;

	/* Parse all the items in the pattern */
	while (item && item->type != RTE_FLOW_ITEM_TYPE_END) {
		hdr_info = &rte_ulp_hdr_info[item->type];
		if (hdr_info->hdr_type == BNXT_ULP_HDR_TYPE_NOT_SUPPORTED) {
			goto hdr_parser_error;
		} else if (hdr_info->hdr_type == BNXT_ULP_HDR_TYPE_SUPPORTED) {
			/* call the registered callback handler */
			if (hdr_info->proto_hdr_func) {
				if (hdr_info->proto_hdr_func(item, params) !=
				    BNXT_TF_RC_SUCCESS) {
					return BNXT_TF_RC_ERROR;
				}
			}
		}
		item++;
	}
	/* update the implied SVIF */
	return ulp_tc_parser_implicit_match_port_process(params);

hdr_parser_error:
	netdev_err(bp->dev, "Truflow parser does not support type %d\n", item->type);
	return BNXT_TF_RC_PARSE_ERR;
}

/* Function to handle the parsing of RTE Flow action queue. */
static int32_t
ulp_rte_queue_act_handler(const struct rte_flow_action *action_item,
			  struct ulp_tc_parser_params *param)
{
	const struct rte_flow_action_queue *q_info;
	struct ulp_tc_act_prop *ap = &param->act_prop;

	if (!action_item || !action_item->conf) {
		netdev_err(NULL, "Parse Err: invalid queue configuration\n");
		return BNXT_TF_RC_ERROR;
	}

	q_info = action_item->conf;
	/* Copy the queue into the specific action properties */
	memcpy(&ap->act_details[BNXT_ULP_ACT_PROP_IDX_QUEUE_INDEX],
	       &q_info->index, BNXT_ULP_ACT_PROP_SZ_QUEUE_INDEX);

	/* set the queue action header bit */
	ULP_BITMAP_SET(param->act_bitmap.bits, BNXT_ULP_ACT_BIT_QUEUE);

	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow action count. */
static int32_t
ulp_rte_count_act_handler(const struct rte_flow_action *action_item,
			  struct ulp_tc_parser_params *params)
{
	struct ulp_tc_act_prop *act_prop = &params->act_prop;
	const struct rte_flow_action_count *act_count;

	act_count = action_item->conf;
	if (act_count) {
		memcpy(&act_prop->act_details[BNXT_ULP_ACT_PROP_IDX_COUNT],
		       &act_count->id,
		       BNXT_ULP_ACT_PROP_SZ_COUNT);
	}

	/* Update the hdr_bitmap with count */
	ULP_BITMAP_SET(params->act_bitmap.bits, BNXT_ULP_ACT_BIT_COUNT);
	return BNXT_TF_RC_SUCCESS;
}

/*
 * This structure has to be indexed based on the rte_flow_action_type that is
 * part of DPDK. The below array is list of parsing functions for each of the
 * flow actions that are supported.
 */
struct bnxt_ulp_rte_act_info rte_ulp_act_info[] = {
	[RTE_FLOW_ACTION_TYPE_END] = {
		.act_type                = BNXT_ULP_ACT_TYPE_END,
		.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_VOID] = {
		.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
		.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_PASSTHRU] = {
		.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
		.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_JUMP] = {
		.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
		.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_MARK] = {
		.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
		.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_FLAG] = {
		.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
		.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_QUEUE] = {
		.act_type                = BNXT_ULP_ACT_TYPE_SUPPORTED,
		.proto_act_func          = ulp_rte_queue_act_handler
	},
	[RTE_FLOW_ACTION_TYPE_DROP] = {
		.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
		.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_COUNT] = {
		.act_type                = BNXT_ULP_ACT_TYPE_SUPPORTED,
		.proto_act_func          = ulp_rte_count_act_handler
	},
	[RTE_FLOW_ACTION_TYPE_RSS] = {
		.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
		.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_PF] = {
		.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
		.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_VF] = {
		.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
		.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_PORT_ID] = {
		.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
		.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_METER] = {
		.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
		.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_SECURITY] = {
		.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
		.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_OF_DEC_NW_TTL] = {
		.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
		.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_OF_POP_VLAN] = {
		.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
		.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN] = {
		.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
		.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID] = {
		.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
		.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_PCP] = {
		.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
		.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_OF_POP_MPLS] = {
		.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
		.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_OF_PUSH_MPLS] = {
		.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
		.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP] = {
		.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
		.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_VXLAN_DECAP] = {
		.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
		.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_IP_ENCAP] = {
		.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
		.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_IP_DECAP] = {
		.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
		.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_NVGRE_ENCAP] = {
		.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
		.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_NVGRE_DECAP] = {
		.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
		.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_RAW_ENCAP] = {
		.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
		.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_RAW_DECAP] = {
		.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
		.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_SET_IPV4_SRC] = {
		.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
		.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_SET_IPV4_DST] = {
		.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
		.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_SET_IPV6_SRC] = {
		.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
		.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_SET_IPV6_DST] = {
		.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
		.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_SET_TP_SRC] = {
		.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
		.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_SET_TP_DST] = {
		.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
		.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_MAC_SWAP] = {
		.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
		.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_DEC_TTL] = {
		.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
		.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_SET_TTL] = {
		.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
		.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_SET_MAC_SRC] = {
		.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
		.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_SET_MAC_DST] = {
		.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
		.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_INC_TCP_SEQ] = {
		.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
		.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_DEC_TCP_SEQ] = {
		.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
		.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_INC_TCP_ACK] = {
		.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
		.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_DEC_TCP_ACK] = {
		.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
		.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_SAMPLE] = {
		.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
		.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_PORT_REPRESENTOR] = {
		.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
		.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT] = {
		.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
		.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_INDIRECT] = {
		.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
		.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_INDIRECT + 1] = {
		.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
		.proto_act_func          = NULL
	}
};

static int
ulp_tc_custom_parser_implicit_redirect_process(struct bnxt *bp, struct ulp_tc_parser_params *params)
{
	enum bnxt_ulp_intf_type intf_type;
	u32 ifindex;
	u16 dst_fid;

	/* No, SR-IOV. So, dst_fid will always be PF's */
	dst_fid = bp->pf.fw_fid;

	/* Get the port db ifindex */
	if (ulp_port_db_dev_port_to_ulp_index(params->ulp_ctx, dst_fid,
					      &ifindex)) {
		netdev_dbg(bp->dev, "Invalid port id\n");
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

	return ulp_tc_parser_act_port_set(params, ifindex, false /* lag */);
}

/*
 * Function to handle the parsing of RTE Flows and placing
 * the RTE flow actions into the ulp structures.
 */
static int32_t
bnxt_ulp_custom_tc_parser_act_parse(struct bnxt *bp,
				    const struct rte_flow_action actions[],
				    struct ulp_tc_parser_params *params)
{
	const struct rte_flow_action *action_item = actions;
	struct bnxt_ulp_rte_act_info *hdr_info;

	/* Parse all the items in the pattern */
	while (action_item && action_item->type != RTE_FLOW_ACTION_TYPE_END) {
		hdr_info = &rte_ulp_act_info[action_item->type];
		if (hdr_info->act_type == BNXT_ULP_ACT_TYPE_NOT_SUPPORTED) {
			goto act_parser_error;
		} else if (hdr_info->act_type == BNXT_ULP_ACT_TYPE_SUPPORTED) {
			/* call the registered callback handler */
			if (hdr_info->proto_act_func) {
				if (hdr_info->proto_act_func(action_item,
							     params) !=
				    BNXT_TF_RC_SUCCESS) {
					return BNXT_TF_RC_ERROR;
				}
			}
		}
		action_item++;
	}

	if (!ULP_BITMAP_ISSET(params->act_bitmap.bits, BNXT_ULP_ACT_BIT_QUEUE))
		ulp_tc_custom_parser_implicit_redirect_process(bp, params);

	return BNXT_TF_RC_SUCCESS;

act_parser_error:
	netdev_err(NULL, "Truflow parser does not support act %u\n", action_item->type);
	return BNXT_TF_RC_ERROR;
}

/* Function to create the ulp flow. */
int
bnxt_custom_ulp_flow_create(struct bnxt *bp, u16 src_fid,
			    const struct rte_flow_item pattern[],
			    const struct rte_flow_action actions[],
			    struct bnxt_ulp_flow_info *flow_info)
{
	struct bnxt_ulp_mapper_parms *mapper_encap_mparms = NULL;
	struct bnxt_ulp_mapper_parms mapper_mparms = { 0 };
	struct ulp_tc_parser_params *params = NULL;
	struct bnxt_ulp_context *ulp_ctx;
	int rc, ret = BNXT_TF_RC_ERROR;
	u16 func_id;
	u32 fid;

	ulp_ctx = bnxt_ulp_bp_ptr2_cntxt_get(bp);
	if (!ulp_ctx) {
		netdev_dbg(bp->dev, "ULP context is not initialized\n");
		goto flow_error;
	}

	/* Initialize the parser params */
	params = vzalloc(sizeof(*params));
	params->ulp_ctx = ulp_ctx;

	if (bnxt_ulp_cntxt_app_id_get(params->ulp_ctx, &params->app_id)) {
		netdev_dbg(bp->dev, "failed to get the app id\n");
		goto flow_error;
	}

	/* Set the flow attributes */
	bnxt_custom_ulp_set_dir_attributes(bp, params, src_fid);

	/* Set NPAR Enabled in the computed fields */
	if (BNXT_NPAR(ulp_ctx->bp))
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_NPAR_ENABLED, 1);

	/* copy the device port id and direction for further processing */
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_INCOMING_IF, src_fid);
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_DEV_PORT_ID, src_fid);
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_SVIF_FLAG,
			    BNXT_ULP_INVALID_SVIF_VAL);

	/* Get the function id */
	if (ulp_port_db_port_func_id_get(ulp_ctx, src_fid, &func_id)) {
		netdev_dbg(bp->dev, "conversion of port to func id failed\n");
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

	/* Parse the rte flow pattern */
	ret = bnxt_ulp_custom_tc_parser_hdr_parse(bp, pattern, params);
	if (ret != BNXT_TF_RC_SUCCESS)
		goto free_fid;

	/* Parse the rte flow action */
	ret = bnxt_ulp_custom_tc_parser_act_parse(bp, actions, params);
	if (ret != BNXT_TF_RC_SUCCESS)
		goto free_fid;

	params->fid = fid;
	params->func_id = func_id;
	/* TODO: params->priority = tc_flow_cmd->common.prio; */

	netdev_dbg(bp->dev, "Flow prio: %u chain: %u\n",
		   params->priority, params->match_chain_id);

	params->port_id = src_fid;
	/* Perform the rte flow post process */
	ret = bnxt_ulp_tc_parser_post_process(params);
	if (ret == BNXT_TF_RC_ERROR)
		goto free_fid;
	else if (ret == BNXT_TF_RC_FID)
		goto return_fid;

	/* Dump the rte flow pattern */
	ulp_parser_hdr_info_dump(params);
	/* Dump the rte flow action */
	ulp_parser_act_info_dump(params);

	ret = ulp_matcher_pattern_match(params, &params->class_id);
	if (ret != BNXT_TF_RC_SUCCESS)
		goto free_fid;

	ret = ulp_matcher_action_match(params, &params->act_tmpl);
	if (ret != BNXT_TF_RC_SUCCESS)
		goto free_fid;

	bnxt_custom_ulp_init_mapper_params(&mapper_mparms, params, BNXT_ULP_FDB_TYPE_REGULAR);
	/* Call the ulp mapper to create the flow in the hardware. */
	ret = ulp_mapper_flow_create(ulp_ctx, &mapper_mparms, NULL);
	if (ret)
		goto free_fid;

	if (params->tnl_key) {
		ret = bnxt_custom_ulp_alloc_mapper_encap_cparams(&mapper_encap_mparms,
								 &mapper_mparms);
		if (ret)
			goto mapper_destroy;
	}

return_fid:
	flow_info->flow_id = fid;
	if (params->tnl_key) {
		flow_info->mparms = mapper_encap_mparms;
		ether_addr_copy(flow_info->tnl_dmac, params->tnl_dmac);
		ether_addr_copy(flow_info->tnl_smac, params->tnl_smac);
		flow_info->tnl_ether_type = params->tnl_ether_type;
		flow_info->encap_key = params->tnl_key;
		flow_info->neigh_key = params->neigh_key;
	}
	vfree(params);
	mutex_unlock(&ulp_ctx->cfg_data->flow_db_lock);

	return 0;

mapper_destroy:
	ulp_mapper_flow_destroy(ulp_ctx, mapper_mparms.flow_type,
				mapper_mparms.flow_id, NULL);
free_fid:
	vfree(params->tnl_key);
	vfree(params->neigh_key);
	ulp_flow_db_fid_free(ulp_ctx, BNXT_ULP_FDB_TYPE_REGULAR, fid);
release_lock:
	mutex_unlock(&ulp_ctx->cfg_data->flow_db_lock);
flow_error:
	vfree(params);
	if (ret == -ENOSPC)
		return ret;
	else
		return (ret == BNXT_TF_RC_PARSE_ERR_NOTSUPP) ? -EOPNOTSUPP : -EIO;
}

#endif
