// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#include "bnxt_compat.h"
#include "bnxt.h"
#include "bnxt_tfc.h"
#include "ulp_tc_parser.h"
#include "ulp_linux.h"
#include "bnxt_ulp.h"
#include "bnxt_tf_common.h"
#include "ulp_matcher.h"
#include "ulp_utils.h"
#include "ulp_port_db.h"
#include "ulp_flow_db.h"
#include "ulp_mapper.h"
#include "ulp_template_db_tbl.h"

#if defined(CONFIG_BNXT_FLOWER_OFFLOAD) || defined(CONFIG_BNXT_CUSTOM_FLOWER_OFFLOAD)
#ifdef CONFIG_BNXT_FLOWER_OFFLOAD
/* Local defines for the parsing functions */
#define ULP_VLAN_PRIORITY_SHIFT		13 /* First 3 bits */
#define ULP_VLAN_PRIORITY_MASK		0x700
#define ULP_VLAN_TAG_MASK		0xFFF /* Last 12 bits*/
#define ULP_UDP_PORT_VXLAN		4789
#define ULP_UDP_PORT_VXLAN_MASK		0XFFFF

static int bnxt_tc_set_dscp(struct bnxt *bp,
			    struct ulp_tc_parser_params *params,
			    bool ipv6, u32 val, u32 mask);

struct ulp_parser_vxlan {
	u8 flags;
	u8 rsvd0[3];
	u8 vni[3];
	u8 rsvd1;
};

struct tc_match {
	void *key;
	void *mask;
};

/* Utility function to copy field spec items */
static struct ulp_tc_hdr_field *ulp_tc_parser_fld_copy(struct ulp_tc_hdr_field
						       *field,
						       const void *buffer,
						       u32 size)
{
	field->size = size;
	memcpy(field->spec, buffer, field->size);
	field++;
	return field;
}

/* Utility function to update the field_bitmap */
static void ulp_tc_parser_field_bitmap_update(struct ulp_tc_parser_params
					       *params, u32 idx,
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
			ULP_COMP_FLD_IDX_WR(params,
					    BNXT_ULP_CF_IDX_WC_MATCH, 1);
	} else {
		ULP_INDEX_BITMAP_RESET(params->fld_bitmap.bits, idx);
	}
}

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

static int ulp_tc_prsr_fld_size_validate(struct ulp_tc_parser_params *params,
					 u32 *idx, u32 size)
{
	if (params->field_idx + size >= BNXT_ULP_PROTO_HDR_MAX)
		return -EINVAL;
	*idx = params->field_idx;
	params->field_idx += size;
	return 0;
}

/* Function to handle the update of proto header based on field values */
static void ulp_tc_l2_proto_type_update(struct ulp_tc_parser_params *param,
					u16 type, u32 in_flag)
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
	} else if (type == cpu_to_be16(ETH_P_IPV6))  {
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

/* The ulp_hdr_info[] table is indexed by the dissector key_id values in
 * ascending order. However parsing the headers in that sequence may not
 * be desirable. For example, we might want to process the eth header
 * first before parsing the IP addresses, as the parser might expect
 * certain header bits to be set before processing the next layer headers.
 * The below table prescribes the sequence that we want to parse the
 * headers in.
 */
static int ulp_hdr_parse_sequence[] = {
	FLOW_DISSECTOR_KEY_ENC_CONTROL,
	FLOW_DISSECTOR_KEY_ENC_IP,
	FLOW_DISSECTOR_KEY_ENC_IPV4_ADDRS,
	FLOW_DISSECTOR_KEY_ENC_IPV6_ADDRS,
	FLOW_DISSECTOR_KEY_ENC_PORTS,
	FLOW_DISSECTOR_KEY_ENC_KEYID,

	FLOW_DISSECTOR_KEY_CONTROL,
	FLOW_DISSECTOR_KEY_BASIC,
	FLOW_DISSECTOR_KEY_ETH_ADDRS,
	FLOW_DISSECTOR_KEY_VLAN,
	FLOW_DISSECTOR_KEY_MPLS,
	FLOW_DISSECTOR_KEY_IP,
	FLOW_DISSECTOR_KEY_IPV4_ADDRS,
	FLOW_DISSECTOR_KEY_IPV6_ADDRS,
	FLOW_DISSECTOR_KEY_PORTS,
	FLOW_DISSECTOR_KEY_TCP
};

#define	NUM_DISSECTOR_KEYS	\
	(sizeof(ulp_hdr_parse_sequence) / sizeof(int))

static unsigned int ulp_supported_keys =
	(BIT(FLOW_DISSECTOR_KEY_CONTROL) |
	 BIT(FLOW_DISSECTOR_KEY_BASIC) |
	 BIT(FLOW_DISSECTOR_KEY_ETH_ADDRS) |
	 BIT(FLOW_DISSECTOR_KEY_VLAN) |
	 BIT(FLOW_DISSECTOR_KEY_MPLS) |
	 BIT(FLOW_DISSECTOR_KEY_IP) |
	 BIT(FLOW_DISSECTOR_KEY_IPV4_ADDRS) |
	 BIT(FLOW_DISSECTOR_KEY_IPV6_ADDRS) |
	 BIT(FLOW_DISSECTOR_KEY_PORTS) |
	 BIT(FLOW_DISSECTOR_KEY_TCP) |
	 BIT(FLOW_DISSECTOR_KEY_ENC_CONTROL) |
	 BIT(FLOW_DISSECTOR_KEY_ENC_IP) |
	 BIT(FLOW_DISSECTOR_KEY_ENC_IPV4_ADDRS) |
	 BIT(FLOW_DISSECTOR_KEY_ENC_IPV6_ADDRS) |
	 BIT(FLOW_DISSECTOR_KEY_ENC_PORTS) |
	 BIT(FLOW_DISSECTOR_KEY_ENC_KEYID));
#endif

#if defined(CONFIG_BNXT_FLOWER_OFFLOAD) || defined(CONFIG_BNXT_CUSTOM_FLOWER_OFFLOAD)
/* Function to handle the post processing of the computed
 * fields for the interface.
 */
static void bnxt_ulp_comp_fld_intf_update(struct ulp_tc_parser_params
					  *params)
{
	enum bnxt_ulp_direction_type dir;
	u16 port_id, parif, svif, vf_roce;
	u16 hw_lag_en;
	u32 ifindex;
	u32 mtype;
	u8 udcc;

	/* get the direction details */
	dir = ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_DIRECTION);

	/* read the port id details */
	port_id = ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_INCOMING_IF);
	if (ulp_port_db_dev_port_to_ulp_index(params->ulp_ctx, port_id,
					      &ifindex)) {
		netdev_dbg(params->ulp_ctx->bp->dev,
			   "ParseErr:Portid is not valid\n");
		return;
	}

	/* Set VF ROCE Support*/
	if (ulp_port_db_vf_roce_get(params->ulp_ctx, port_id, &vf_roce)) {
		netdev_dbg(params->ulp_ctx->bp->dev, "ParseErr:port_id %d is not valid\n",
			   port_id);
		return;
	}
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_VF_ROCE_EN, vf_roce);

	/* Set Hw lag Support*/
	if (ulp_port_db_hw_lag_get(params->ulp_ctx, port_id, &hw_lag_en)) {
		netdev_dbg(params->ulp_ctx->bp->dev, "ParseErr:port_id %d is not valid\n",
			   port_id);
		return;
	}
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_HW_LAG_EN, hw_lag_en);

	/* Set UDCC Support*/
	if (ulp_port_db_udcc_get(params->ulp_ctx, port_id, &udcc)) {
		netdev_dbg(params->ulp_ctx->bp->dev, "ParseErr:port_id %d is not valid\n",
			   port_id);
		return;
	}
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_UDCC_EN, udcc);

	if (dir == BNXT_ULP_DIR_INGRESS) {
		/* Set port PARIF */
		if (ulp_port_db_parif_get(params->ulp_ctx, ifindex,
					  BNXT_ULP_DRV_FUNC_PARIF, &parif)) {
			netdev_dbg(params->ulp_ctx->bp->dev,
				   "ParseErr:ifindex is not valid\n");
			return;
		}
		/* Note:
		 * We save the drv_func_parif into CF_IDX of phy_port_parif,
		 * since that index is currently referenced by ingress templates
		 * for datapath flows. If in the future we change the parser to
		 * save it in the CF_IDX of drv_func_parif we also need to update
		 * the template.
		 */
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_PHY_PORT_PARIF,
				    parif);

		/* Set port SVIF */
		if (ulp_port_db_svif_get(params->ulp_ctx, ifindex, BNXT_ULP_PHY_PORT_SVIF, &svif)) {
			netdev_dbg(params->ulp_ctx->bp->dev, "ParseErr:ifindex is not valid\n");
			return;
		}
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_PHY_PORT_SVIF, svif);
	} else {
		/* Get the match port type */
		mtype = ULP_COMP_FLD_IDX_RD(params,
					    BNXT_ULP_CF_IDX_MATCH_PORT_TYPE);
		if (mtype == BNXT_ULP_INTF_TYPE_VF_REP) {
			ULP_COMP_FLD_IDX_WR(params,
					    BNXT_ULP_CF_IDX_MATCH_PORT_IS_VFREP,
					    1);
			/* Set VF func PARIF */
			if (ulp_port_db_parif_get(params->ulp_ctx, ifindex,
						  BNXT_ULP_VF_FUNC_PARIF,
						  &parif)) {
				netdev_dbg(params->ulp_ctx->bp->dev,
					   "ParseErr:ifindex is not valid\n");
				return;
			}
			ULP_COMP_FLD_IDX_WR(params,
					    BNXT_ULP_CF_IDX_VF_FUNC_PARIF,
					    parif);

		} else {
			/* Set DRV func PARIF */
			if (ulp_port_db_parif_get(params->ulp_ctx, ifindex,
						  BNXT_ULP_DRV_FUNC_PARIF,
						  &parif)) {
				netdev_dbg(params->ulp_ctx->bp->dev,
					   "ParseErr:ifindex is not valid\n");
				return;
			}
			ULP_COMP_FLD_IDX_WR(params,
					    BNXT_ULP_CF_IDX_DRV_FUNC_PARIF,
					    parif);
		}
		if (mtype == BNXT_ULP_INTF_TYPE_PF) {
			ULP_COMP_FLD_IDX_WR(params,
					    BNXT_ULP_CF_IDX_MATCH_PORT_IS_PF,
					    1);
		}
	}
}

static int ulp_post_process_normal_flow(struct ulp_tc_parser_params *params)
{
	enum bnxt_ulp_intf_type match_port_type, act_port_type;
	enum bnxt_ulp_direction_type dir;
	u32 act_port_set;

	/* Get the computed details */
	dir = ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_DIRECTION);
	match_port_type = ULP_COMP_FLD_IDX_RD(params,
					      BNXT_ULP_CF_IDX_MATCH_PORT_TYPE);
	act_port_type = ULP_COMP_FLD_IDX_RD(params,
					    BNXT_ULP_CF_IDX_ACT_PORT_TYPE);
	act_port_set = ULP_COMP_FLD_IDX_RD(params,
					   BNXT_ULP_CF_IDX_ACT_PORT_IS_SET);

	/* set the flow direction in the proto and action header */
	if (dir == BNXT_ULP_DIR_EGRESS) {
		ULP_BITMAP_SET(params->hdr_bitmap.bits,
			       BNXT_ULP_FLOW_DIR_BITMASK_EGR);
		ULP_BITMAP_SET(params->act_bitmap.bits,
			       BNXT_ULP_FLOW_DIR_BITMASK_EGR);
	} else {
		ULP_BITMAP_SET(params->hdr_bitmap.bits,
			       BNXT_ULP_FLOW_DIR_BITMASK_ING);
		ULP_BITMAP_SET(params->act_bitmap.bits,
			       BNXT_ULP_FLOW_DIR_BITMASK_ING);
	}

	/* Evaluate the VF to VF flag */
	if (act_port_set && act_port_type == BNXT_ULP_INTF_TYPE_VF_REP &&
	    match_port_type == BNXT_ULP_INTF_TYPE_VF_REP)
		ULP_BITMAP_SET(params->act_bitmap.bits,
			       BNXT_ULP_ACT_BIT_VF_TO_VF);
	/* Update the decrement ttl computational fields */
	if (ULP_BITMAP_ISSET(params->act_bitmap.bits,
			     BNXT_ULP_ACT_BIT_DEC_TTL)) {
		/* Check that vxlan proto is included and vxlan decap
		 * action is not set then decrement tunnel ttl.
		 * Similarly add GRE and NVGRE in future.
		 */
		if ((ULP_BITMAP_ISSET(params->hdr_bitmap.bits,
				      BNXT_ULP_HDR_BIT_T_VXLAN) &&
		    !ULP_BITMAP_ISSET(params->act_bitmap.bits,
				      BNXT_ULP_ACT_BIT_VXLAN_DECAP))) {
			ULP_COMP_FLD_IDX_WR(params,
					    BNXT_ULP_CF_IDX_ACT_T_DEC_TTL, 1);
		} else {
			ULP_COMP_FLD_IDX_WR(params,
					    BNXT_ULP_CF_IDX_ACT_DEC_TTL, 1);
		}
	}

	/* Merge the hdr_fp_bit into the proto header bit */
	params->hdr_bitmap.bits |= params->hdr_fp_bit.bits;

	/* Update the comp fld fid */
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_FID, params->fid);

	/* Update the comp fld app_priority */
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_APP_PRIORITY, params->priority);

	/* Update the comp fld em_for_ipv6 */
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_EM_FOR_TC,
			    SUPPORT_CFA_EM_FOR_TC);

	/* set the L2 context usage shall change it later */
	ULP_BITMAP_SET(params->cf_bitmap, BNXT_ULP_CF_BIT_L2_CNTXT_ID);

	/* Update the computed interface parameters */
	bnxt_ulp_comp_fld_intf_update(params);

	/* TBD: Handle the flow rejection scenarios */
	return 0;
}

/* Function to handle the post processing of the parsing details */
int bnxt_ulp_tc_parser_post_process(struct ulp_tc_parser_params *params)
{
	ulp_post_process_normal_flow(params);

	/* TBD: Do we need tunnel post processing in kernel mode ? */
	return BNXT_TF_RC_NORMAL;
}

/* Function to compute the flow direction based on the match port details */
static void bnxt_ulp_tc_parser_direction_compute(struct ulp_tc_parser_params
						  *params)
{
	enum bnxt_ulp_intf_type match_port_type;

	/* Get the match port type */
	match_port_type = ULP_COMP_FLD_IDX_RD(params,
					      BNXT_ULP_CF_IDX_MATCH_PORT_TYPE);

	/* If ingress flow and matchport is vf rep then dir is egress*/
	if ((params->dir_attr & BNXT_ULP_FLOW_ATTR_INGRESS) &&
	    match_port_type == BNXT_ULP_INTF_TYPE_VF_REP) {
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_DIRECTION,
				    BNXT_ULP_DIR_EGRESS);
	} else {
		/* Assign the input direction */
		if (params->dir_attr & BNXT_ULP_FLOW_ATTR_INGRESS)
			ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_DIRECTION,
					    BNXT_ULP_DIR_INGRESS);
		else
			ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_DIRECTION,
					    BNXT_ULP_DIR_EGRESS);
	}
}

static int ulp_tc_parser_svif_set(struct ulp_tc_parser_params *params,
				  u32 ifindex, u16 mask)
{
	struct ulp_tc_hdr_field *hdr_field;
	enum bnxt_ulp_svif_type svif_type;
	enum bnxt_ulp_intf_type port_type;
	enum bnxt_ulp_direction_type dir;
	u16 svif;

	/* SVIF already set, multiple source not supported */
	if (ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_SVIF_FLAG) !=
	    BNXT_ULP_INVALID_SVIF_VAL)
		return BNXT_TF_RC_ERROR;

	/* Get port type details */
	port_type = ulp_port_db_port_type_get(params->ulp_ctx, ifindex);
	if (port_type == BNXT_ULP_INTF_TYPE_INVALID)
		return BNXT_TF_RC_ERROR;

	/* Update the match port type */
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_MATCH_PORT_TYPE, port_type);

	/* compute the direction */
	bnxt_ulp_tc_parser_direction_compute(params);

	/* Get the computed direction */
	dir = ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_DIRECTION);
	if (dir == BNXT_ULP_DIR_INGRESS) {
		svif_type = BNXT_ULP_PHY_PORT_SVIF;
		if (port_type == BNXT_ULP_INTF_TYPE_VF_REP) {
			/* Save the VF func svif in the computed fields */
			ulp_port_db_svif_get(params->ulp_ctx, ifindex, BNXT_ULP_VF_FUNC_SVIF,
					     &svif);
			ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_VF_FUNC_SVIF, svif);
		} else {
			/* Save the driver func svif in the computed fields */
			ulp_port_db_svif_get(params->ulp_ctx, ifindex, BNXT_ULP_DRV_FUNC_SVIF,
					     &svif);
			ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_DRV_FUNC_SVIF, svif);
		}
	} else {
		if (port_type == BNXT_ULP_INTF_TYPE_VF_REP)
			svif_type = BNXT_ULP_VF_FUNC_SVIF;
		else
			svif_type = BNXT_ULP_DRV_FUNC_SVIF;
	}
	ulp_port_db_svif_get(params->ulp_ctx, ifindex, svif_type,
			     &svif);
	svif = cpu_to_be16(svif);
	hdr_field = &params->hdr_field[BNXT_ULP_PROTO_HDR_FIELD_SVIF_IDX];
	memcpy(hdr_field->spec, &svif, sizeof(svif));
	memcpy(hdr_field->mask, &mask, sizeof(mask));
	hdr_field->size = sizeof(svif);
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_SVIF_FLAG,
			    be16_to_cpu(svif));
	return BNXT_TF_RC_SUCCESS;
}

int ulp_tc_parser_implicit_match_port_process(struct ulp_tc_parser_params
					       *params)
{
	int rc = BNXT_TF_RC_ERROR;
	u16 svif_mask = 0xFFFF;
	u16 port_id = 0;
	u32 ifindex;

	if (ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_SVIF_FLAG) !=
	    BNXT_ULP_INVALID_SVIF_VAL)
		return BNXT_TF_RC_SUCCESS;

	/* SVIF not set. So get the port id */
	port_id = ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_INCOMING_IF);

	if (ulp_port_db_dev_port_to_ulp_index(params->ulp_ctx, port_id,
					      &ifindex))
		return rc;

	/* Update the SVIF details */
	rc = ulp_tc_parser_svif_set(params, ifindex, svif_mask);

	/* If no ETH header match added for some chain filters,
	 * add the SVIF as the only match header bit.
	 */
	if (!(ULP_BITMAP_ISSET(params->hdr_bitmap.bits, BNXT_ULP_HDR_BIT_O_ETH)) &&
	    !(ULP_BITMAP_ISSET(params->hdr_bitmap.bits, BNXT_ULP_HDR_BIT_O_L2_FILTER)))
		ULP_BITMAP_SET(params->hdr_bitmap.bits, BNXT_ULP_HDR_BIT_SVIF);

	return rc;
}
#endif

#ifdef CONFIG_BNXT_FLOWER_OFFLOAD
static void bnxt_ulp_flow_rule_match(struct flow_rule *rule, unsigned int key,
				     void *match)
{
	switch (key) {
	case FLOW_DISSECTOR_KEY_CONTROL:
		flow_rule_match_control(rule, match);
		break;
	case FLOW_DISSECTOR_KEY_BASIC:
		flow_rule_match_basic(rule, match);
		break;
	case FLOW_DISSECTOR_KEY_IPV4_ADDRS:
		flow_rule_match_ipv4_addrs(rule, match);
		break;
	case FLOW_DISSECTOR_KEY_IPV6_ADDRS:
		flow_rule_match_ipv6_addrs(rule, match);
		break;
	case FLOW_DISSECTOR_KEY_PORTS:
		flow_rule_match_ports(rule, match);
		break;
	case FLOW_DISSECTOR_KEY_ETH_ADDRS:
		flow_rule_match_eth_addrs(rule, match);
		break;
	case FLOW_DISSECTOR_KEY_VLAN:
		flow_rule_match_vlan(rule, match);
		break;
	case FLOW_DISSECTOR_KEY_IP:
		flow_rule_match_ip(rule, match);
		break;
	case FLOW_DISSECTOR_KEY_TCP:
		flow_rule_match_tcp(rule, match);
		break;
	case FLOW_DISSECTOR_KEY_ENC_KEYID:
		flow_rule_match_enc_keyid(rule, match);
		break;
	case FLOW_DISSECTOR_KEY_ENC_IPV4_ADDRS:
		flow_rule_match_enc_ipv4_addrs(rule, match);
		break;
	case FLOW_DISSECTOR_KEY_ENC_IPV6_ADDRS:
		flow_rule_match_enc_ipv6_addrs(rule, match);
		break;
	case FLOW_DISSECTOR_KEY_ENC_CONTROL:
		flow_rule_match_enc_control(rule, match);
		break;
	case FLOW_DISSECTOR_KEY_ENC_PORTS:
		flow_rule_match_enc_ports(rule, match);
		break;
	case FLOW_DISSECTOR_KEY_ENC_IP:
		flow_rule_match_enc_ip(rule, match);
		break;
	case FLOW_DISSECTOR_KEY_MPLS:
		flow_rule_match_mpls(rule, match);
		break;
	}
}

static struct flow_dissector_key_eth_addrs eth_addr_null = {
	{ 0, 0, 0, 0, 0, 0 },		/* dst */
	{ 0, 0, 0, 0, 0, 0 }		/* src */
};

/* Return true if eth addrs should be added implicitly.
 * Otherwise, return false.
 */
static bool bnxt_ulp_tc_is_implicit_eth_addrs(struct ulp_tc_parser_params
					      *params,
					      enum flow_dissector_key_id key,
					      unsigned int used_keys)
{
	/* ETH_ADDRS key is present in used_keys ? or have we
	 * already added eth addrs implicitly ?
	 */
	if ((used_keys & BIT(FLOW_DISSECTOR_KEY_ETH_ADDRS)) ||
	    params->implicit_eth_parsed)
		return false;

	switch (key) {
	case FLOW_DISSECTOR_KEY_VLAN:
	case FLOW_DISSECTOR_KEY_IP:
		return true;
	case FLOW_DISSECTOR_KEY_IPV4_ADDRS:
		if (params->addr_type ==  FLOW_DISSECTOR_KEY_IPV4_ADDRS)
			return true;
		break;
	case FLOW_DISSECTOR_KEY_IPV6_ADDRS:
		if (params->addr_type ==  FLOW_DISSECTOR_KEY_IPV6_ADDRS)
			return true;
		break;
	case FLOW_DISSECTOR_KEY_BASIC:
		if (!params->addr_type &&
		    (params->n_proto == cpu_to_be16(ETH_P_IP) ||
		     params->n_proto == cpu_to_be16(ETH_P_IPV6)))
			return true;
		break;
	default:
		break;
	}
	return false;
}

static int bnxt_ulp_tc_parse_implicit_eth_addrs(struct bnxt *bp,
						struct ulp_tc_parser_params
						*params)
{
	struct bnxt_ulp_tc_hdr_info *hdr_info;
	struct tc_match match;
	int rc;

	hdr_info = &ulp_hdr_info[FLOW_DISSECTOR_KEY_ETH_ADDRS];
	match.key = &eth_addr_null;
	match.mask = &eth_addr_null;

	rc = hdr_info->proto_hdr_func(bp, params, &match);
	if (rc != BNXT_TF_RC_SUCCESS)
		return rc;
	params->implicit_eth_parsed = true;
	return rc;
}

static struct flow_dissector_key_ipv4_addrs ipv4_addr_null = { 0, 0 };

static struct flow_dissector_key_ipv6_addrs ipv6_addr_null = {
	{{{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }}},	/* src */
	{{{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }}}	/* dst */
};

static struct flow_dissector_key_ip ip_ctrl_null = { 0, 0 };

static bool bnxt_ulp_tc_is_implicit_ip_ctrl(struct ulp_tc_parser_params
					    *params,
					    enum flow_dissector_key_id key,
					    unsigned int used_keys)
{
	if (((key == FLOW_DISSECTOR_KEY_IPV4_ADDRS &&
	      params->addr_type == FLOW_DISSECTOR_KEY_IPV4_ADDRS) ||
	     (key == FLOW_DISSECTOR_KEY_IPV6_ADDRS &&
	      params->addr_type == FLOW_DISSECTOR_KEY_IPV6_ADDRS)) &&
	    (used_keys & BIT(FLOW_DISSECTOR_KEY_IP)) == 0)
		return true;

	return false;
}

static bool bnxt_ulp_tc_is_implicit_tnl_ip_ctrl(struct ulp_tc_parser_params
						*params,
						enum flow_dissector_key_id key,
						unsigned int used_keys)
{
	if (((key == FLOW_DISSECTOR_KEY_ENC_IPV4_ADDRS &&
	      params->tnl_addr_type == FLOW_DISSECTOR_KEY_IPV4_ADDRS) ||
	     (key == FLOW_DISSECTOR_KEY_ENC_IPV6_ADDRS &&
	      params->tnl_addr_type == FLOW_DISSECTOR_KEY_IPV6_ADDRS)) &&
	    (used_keys & BIT(FLOW_DISSECTOR_KEY_ENC_IP)) == 0)
		return true;

	return false;
}

static bool bnxt_ulp_tc_is_implicit_ipv4_addrs(enum flow_dissector_key_id key,
					       unsigned int used_keys,
					       u16 n_proto)
{
	if (key == FLOW_DISSECTOR_KEY_IP &&
	    (used_keys & BIT(FLOW_DISSECTOR_KEY_IPV4_ADDRS)) == 0 &&
	    n_proto == cpu_to_be16(ETH_P_IP))
		return true;

	return false;
}

static bool bnxt_ulp_tc_is_implicit_ipv6_addrs(enum flow_dissector_key_id key,
					       unsigned int used_keys,
					       u16 n_proto)
{
	if (key == FLOW_DISSECTOR_KEY_IP &&
	    (used_keys & BIT(FLOW_DISSECTOR_KEY_IPV6_ADDRS)) == 0 &&
	    n_proto == cpu_to_be16(ETH_P_IPV6))
		return true;

	return false;
}

static bool bnxt_ulp_tc_is_implicit_tnl_ipv4_addrs(enum
						   flow_dissector_key_id key,
						   unsigned int used_keys,
						   u16 n_proto)
{
	if (key == FLOW_DISSECTOR_KEY_ENC_IP &&
	    (used_keys & BIT(FLOW_DISSECTOR_KEY_ENC_IPV4_ADDRS)) == 0 &&
	    n_proto == cpu_to_be16(ETH_P_IP))
		return true;

	return false;
}

static bool bnxt_ulp_tc_is_implicit_ipv4(struct ulp_tc_parser_params *params,
					 unsigned int used_keys)
{
	if (!params->implicit_ipv4_parsed &&
	    params->n_proto == cpu_to_be16(ETH_P_IP) &&
	    (used_keys & BIT(FLOW_DISSECTOR_KEY_IP)) == 0 &&
	    (used_keys & BIT(FLOW_DISSECTOR_KEY_IPV4_ADDRS)) == 0)
		return true;

	return false;
}

static bool bnxt_ulp_tc_is_implicit_ipv6(struct ulp_tc_parser_params *params,
					 unsigned int used_keys)
{
	if (!params->implicit_ipv6_parsed &&
	    params->n_proto == cpu_to_be16(ETH_P_IPV6) &&
	    (used_keys & BIT(FLOW_DISSECTOR_KEY_IP)) == 0 &&
	    (used_keys & BIT(FLOW_DISSECTOR_KEY_IPV6_ADDRS)) == 0)
		return true;

	return false;
}

static int bnxt_ulp_add_implicit_ip_ctrl(struct bnxt *bp,
					 struct ulp_tc_parser_params *params,
					 enum flow_dissector_key_id key)
{
	struct bnxt_ulp_tc_hdr_info *hdr_info;
	struct tc_match match;

	hdr_info = &ulp_hdr_info[key];
	match.key = &ip_ctrl_null;
	match.mask = &ip_ctrl_null;

	return hdr_info->proto_hdr_func(bp, params, &match);
}

static int bnxt_ulp_tc_parse_implicit_ip_ctrl(struct bnxt *bp,
					      struct ulp_tc_parser_params
					      *params)
{
	return bnxt_ulp_add_implicit_ip_ctrl(bp, params,
					     FLOW_DISSECTOR_KEY_IP);
}

static int bnxt_ulp_tc_parse_implicit_tnl_ip_ctrl(struct bnxt *bp,
						  struct ulp_tc_parser_params
						  *params)
{
	return bnxt_ulp_add_implicit_ip_ctrl(bp, params,
					     FLOW_DISSECTOR_KEY_ENC_IP);
}

static int bnxt_ulp_tc_parse_implicit_ipv4_addrs(struct bnxt *bp,
						 struct ulp_tc_parser_params
						 *params)
{
	struct bnxt_ulp_tc_hdr_info *hdr_info;
	struct tc_match match;

	hdr_info = &ulp_hdr_info[FLOW_DISSECTOR_KEY_IPV4_ADDRS];
	match.key = &ipv4_addr_null;
	match.mask = &ipv4_addr_null;

	/* addr_type is implicit in this case; i.e, set to zero
	 * in KEY_CONTROL; so set it before invoking the handler.
	 */
	params->addr_type = FLOW_DISSECTOR_KEY_IPV4_ADDRS;

	return hdr_info->proto_hdr_func(bp, params, &match);
}

static int bnxt_ulp_tc_parse_implicit_ipv6_addrs(struct bnxt *bp,
						 struct ulp_tc_parser_params
						 *params)
{
	struct bnxt_ulp_tc_hdr_info *hdr_info;
	struct tc_match match;

	hdr_info = &ulp_hdr_info[FLOW_DISSECTOR_KEY_IPV6_ADDRS];
	match.key = &ipv6_addr_null;
	match.mask = &ipv6_addr_null;

	/* addr_type is implicit in this case; i.e, set to zero
	 * in KEY_CONTROL; so set it before invoking the handler.
	 */
	params->addr_type = FLOW_DISSECTOR_KEY_IPV6_ADDRS;

	return hdr_info->proto_hdr_func(bp, params, &match);
}

static int bnxt_ulp_tc_parse_implicit_ipv4(struct bnxt *bp,
					   struct ulp_tc_parser_params
					   *params)
{
	int rc;

	rc = bnxt_ulp_tc_parse_implicit_ip_ctrl(bp, params);
	if (rc != BNXT_TF_RC_SUCCESS)
		return rc;

	rc = bnxt_ulp_tc_parse_implicit_ipv4_addrs(bp, params);
	if (rc != BNXT_TF_RC_SUCCESS)
		return rc;
	params->implicit_ipv4_parsed = true;
	return rc;
}

static int bnxt_ulp_tc_parse_implicit_ipv6(struct bnxt *bp,
					   struct ulp_tc_parser_params
					   *params)
{
	int rc;

	rc = bnxt_ulp_tc_parse_implicit_ip_ctrl(bp, params);
	if (rc != BNXT_TF_RC_SUCCESS)
		return rc;

	rc = bnxt_ulp_tc_parse_implicit_ipv6_addrs(bp, params);
	if (rc != BNXT_TF_RC_SUCCESS)
		return rc;
	params->implicit_ipv6_parsed = true;
	return rc;
}

static struct flow_dissector_key_ports tcp_ports_null = {{ 0 }};
static struct flow_dissector_key_tcp tcp_ctrl_null = { 0 };

static bool bnxt_ulp_tc_is_implicit_tcp_ctrl(enum flow_dissector_key_id key,
					     unsigned int used_keys)
{
	if (key == FLOW_DISSECTOR_KEY_PORTS &&
	    (used_keys & BIT(FLOW_DISSECTOR_KEY_TCP)) == 0)
		return true;

	return false;
}

static bool bnxt_ulp_tc_is_implicit_tcp_ports(enum flow_dissector_key_id key,
					      unsigned int used_keys)
{
	if (key == FLOW_DISSECTOR_KEY_TCP &&
	    (used_keys & BIT(FLOW_DISSECTOR_KEY_PORTS)) == 0)
		return true;

	return false;
}

static int bnxt_ulp_tc_parse_implicit_tcp_ctrl(struct bnxt *bp,
					       struct ulp_tc_parser_params
					       *params)
{
	struct bnxt_ulp_tc_hdr_info *hdr_info;
	struct tc_match match;

	hdr_info = &ulp_hdr_info[FLOW_DISSECTOR_KEY_TCP];
	match.key = &tcp_ctrl_null;
	match.mask = &tcp_ctrl_null;

	return hdr_info->proto_hdr_func(bp, params, &match);
}

static int bnxt_ulp_tc_parse_implicit_tcp_ports(struct bnxt *bp,
						struct ulp_tc_parser_params
						*params)
{
	struct bnxt_ulp_tc_hdr_info *hdr_info;
	struct tc_match match;

	hdr_info = &ulp_hdr_info[FLOW_DISSECTOR_KEY_PORTS];
	match.key = &tcp_ports_null;
	match.mask = &tcp_ports_null;

	return hdr_info->proto_hdr_func(bp, params, &match);
}

static int bnxt_ulp_tc_resolve_tnl_ipv4(struct bnxt *bp,
					struct ulp_tc_parser_params *params,
					struct flow_rule *rule)
{
	struct flow_match_ipv4_addrs match = { 0 };
	struct bnxt_tc_l2_key l2_info = { 0 };
	struct ip_tunnel_key tun_key = { 0 };
	int rc;

	flow_rule_match_enc_ipv4_addrs(rule, &match);

	/* If we are not matching on tnl_sip, use PF's mac as tnl_dmac */
	if (!match.mask->src) {
		ether_addr_copy(params->tnl_dmac, bp->dev->dev_addr);
		eth_zero_addr(params->tnl_smac);
		return BNXT_TF_RC_SUCCESS;
	}

	/* Resolve tnl hdrs only if we are matching on tnl_sip */
	tun_key.u.ipv4.dst = match.key->src;
	tun_key.tp_dst = 4789;

	rc = bnxt_tc_resolve_ipv4_tunnel_hdrs(bp, NULL, &tun_key, &l2_info,
					      NULL);
	if (rc != 0)
		return BNXT_TF_RC_ERROR;

	ether_addr_copy(params->tnl_dmac, l2_info.smac);
	ether_addr_copy(params->tnl_smac, l2_info.dmac);

	return BNXT_TF_RC_SUCCESS;
}

static int bnxt_ulp_tc_resolve_tnl_ipv6(struct bnxt *bp,
					struct ulp_tc_parser_params *params,
					struct flow_rule *rule)
{
	struct flow_match_ipv6_addrs match = { 0 };
	struct bnxt_tc_l2_key l2_info = { 0 };
	struct ip_tunnel_key tun_key = { 0 };
	int rc;

	flow_rule_match_enc_ipv6_addrs(rule, &match);

	/* If we are not matching on tnl_sip, use PF's mac as tnl_dmac */
	if (!match.mask->src.s6_addr32[0] && !match.mask->src.s6_addr32[1] &&
	    !match.mask->src.s6_addr32[2] && !match.mask->src.s6_addr32[3]) {
		ether_addr_copy(params->tnl_dmac, bp->dev->dev_addr);
		eth_zero_addr(params->tnl_smac);
		return BNXT_TF_RC_SUCCESS;
	}

	/* Resolve tnl hdrs only if we are matching on tnl_sip */
	tun_key.u.ipv6.dst = match.key->src;
	tun_key.tp_dst = 4789;

	rc = bnxt_tc_resolve_ipv6_tunnel_hdrs(bp, NULL, &tun_key, &l2_info,
					      NULL);
	if (rc)
		return BNXT_TF_RC_ERROR;

	ether_addr_copy(params->tnl_dmac, l2_info.smac);
	ether_addr_copy(params->tnl_smac, l2_info.dmac);

	return BNXT_TF_RC_SUCCESS;
}

static int bnxt_ulp_tc_resolve_tnl_hdrs(struct bnxt *bp,
					struct ulp_tc_parser_params *params,
					struct flow_rule *rule)
{
	if (params->tnl_addr_type == FLOW_DISSECTOR_KEY_IPV4_ADDRS)
		return bnxt_ulp_tc_resolve_tnl_ipv4(bp, params, rule);
	else if (params->tnl_addr_type == FLOW_DISSECTOR_KEY_IPV6_ADDRS)
		return bnxt_ulp_tc_resolve_tnl_ipv6(bp, params, rule);
	else
		return BNXT_TF_RC_ERROR;
}

static bool bnxt_ulp_tc_is_l4_key(enum flow_dissector_key_id key)
{
	if (key == FLOW_DISSECTOR_KEY_PORTS || key == FLOW_DISSECTOR_KEY_TCP)
		return true;

	return false;
}

static int bnxt_ulp_tc_parse_pre_process(struct bnxt *bp,
					 struct ulp_tc_parser_params *params,
					 enum flow_dissector_key_id key,
					 unsigned int used_keys)
{
	int rc = BNXT_TF_RC_SUCCESS;

	if (bnxt_ulp_tc_is_implicit_eth_addrs(params, key, used_keys)) {
		rc = bnxt_ulp_tc_parse_implicit_eth_addrs(bp, params);
		if (rc != BNXT_TF_RC_SUCCESS)
			return rc;
	}

	if (bnxt_ulp_tc_is_implicit_tnl_ip_ctrl(params, key, used_keys)) {
		rc = bnxt_ulp_tc_parse_implicit_tnl_ip_ctrl(bp, params);
		if (rc != BNXT_TF_RC_SUCCESS)
			return rc;
	}

	if (bnxt_ulp_tc_is_implicit_ip_ctrl(params, key, used_keys)) {
		rc = bnxt_ulp_tc_parse_implicit_ip_ctrl(bp, params);
		if (rc != BNXT_TF_RC_SUCCESS)
			return rc;
	}

	if (bnxt_ulp_tc_is_l4_key(key)) {
		if (bnxt_ulp_tc_is_implicit_eth_addrs(params,
						      FLOW_DISSECTOR_KEY_BASIC,
						      used_keys))
			bnxt_ulp_tc_parse_implicit_eth_addrs(bp, params);
		if (bnxt_ulp_tc_is_implicit_ipv4(params, used_keys))
			bnxt_ulp_tc_parse_implicit_ipv4(bp, params);
		else if (bnxt_ulp_tc_is_implicit_ipv6(params, used_keys))
			bnxt_ulp_tc_parse_implicit_ipv6(bp, params);
	}

	if (params->ip_proto == IPPROTO_TCP &&
	    bnxt_ulp_tc_is_implicit_tcp_ports(key, used_keys)) {
		rc = bnxt_ulp_tc_parse_implicit_tcp_ports(bp, params);
		if (rc != BNXT_TF_RC_SUCCESS)
			return rc;
	}

	return rc;
}

#ifdef HAVE_FLOW_DISSECTOR_KEY_VLAN_TPID
static int bnxt_ulp_tc_parse_vlan_tpid(struct bnxt *bp,
				       struct ulp_tc_parser_params *params,
				       struct flow_rule *rule)
{
	struct flow_match_vlan match;

	flow_rule_match_vlan(rule, &match);
	params->vlan_tpid = match.key->vlan_tpid;
	params->vlan_tpid_mask = match.mask->vlan_tpid;

	return BNXT_TF_RC_SUCCESS;
}
#else	/* HAVE_FLOW_DISSECTOR_KEY_VLAN_TPID */
static int bnxt_ulp_tc_parse_vlan_tpid(struct bnxt *bp,
				       struct ulp_tc_parser_params *params,
				       struct flow_rule *rule)
{
	return BNXT_TF_RC_ERROR;
}
#endif	/* HAVE_FLOW_DISSECTOR_KEY_VLAN_TPID */

static int bnxt_ulp_tc_parse_post_process(struct bnxt *bp,
					  struct flow_rule *rule,
					  struct ulp_tc_parser_params *params,
					  enum flow_dissector_key_id key,
					  unsigned int used_keys)
{
	int rc = BNXT_TF_RC_SUCCESS;

	/* Resolve tnl L2 headers before parsing other tnl keys */
	if (key == FLOW_DISSECTOR_KEY_ENC_CONTROL) {
		rc = bnxt_ulp_tc_resolve_tnl_hdrs(bp, params, rule);
		if (rc != BNXT_TF_RC_SUCCESS)
			return rc;
	}

	/* Pre process the tpid so eth handler can set it */
	if (key == FLOW_DISSECTOR_KEY_BASIC &&
	    (used_keys & BIT(FLOW_DISSECTOR_KEY_VLAN))) {
		rc = bnxt_ulp_tc_parse_vlan_tpid(bp, params, rule);
		if (rc != BNXT_TF_RC_SUCCESS)
			return rc;
	}

	if (bnxt_ulp_tc_is_implicit_tnl_ipv4_addrs(key, used_keys,
						   params->n_proto)) {
		rc = bnxt_ulp_tc_parse_implicit_ipv4_addrs(bp, params);
		if (rc != BNXT_TF_RC_SUCCESS)
			return rc;
	}

	if (bnxt_ulp_tc_is_implicit_ipv4_addrs(key, used_keys,
					       params->n_proto)) {
		rc = bnxt_ulp_tc_parse_implicit_ipv4_addrs(bp, params);
		if (rc != BNXT_TF_RC_SUCCESS)
			return rc;
	}

	if (bnxt_ulp_tc_is_implicit_ipv6_addrs(key, used_keys,
					       params->n_proto)) {
		rc = bnxt_ulp_tc_parse_implicit_ipv6_addrs(bp, params);
		if (rc != BNXT_TF_RC_SUCCESS)
			return rc;
	}

	if (params->ip_proto == IPPROTO_TCP &&
	    bnxt_ulp_tc_is_implicit_tcp_ctrl(key, used_keys)) {
		rc = bnxt_ulp_tc_parse_implicit_tcp_ctrl(bp, params);
		if (rc != BNXT_TF_RC_SUCCESS)
			return rc;
	}

	return rc;
}

static int bnxt_ulp_tc_parse_hdr_key(struct bnxt *bp, struct flow_rule *rule,
				     struct ulp_tc_parser_params *params,
				     enum flow_dissector_key_id key,
				     unsigned int used_keys)
{
	struct bnxt_ulp_tc_hdr_info *hdr_info = &ulp_hdr_info[key];
	int rc = BNXT_TF_RC_PARSE_ERR;
	struct tc_match match;

	if (hdr_info->hdr_type == BNXT_ULP_HDR_TYPE_NOT_SUPPORTED) {
		netdev_dbg(bp->dev,
			   "Truflow parser does not support type %d\n", key);
		return rc;
	}

	rc = bnxt_ulp_tc_parse_pre_process(bp, params, key, used_keys);
	if (rc != BNXT_TF_RC_SUCCESS)
		return rc;

	bnxt_ulp_flow_rule_match(rule, key, &match);

	/* call the registered callback handler */
	rc = hdr_info->proto_hdr_func(bp, params, &match);
	if (rc != BNXT_TF_RC_SUCCESS)
		return rc;

	rc = bnxt_ulp_tc_parse_post_process(bp, rule, params, key, used_keys);
	if (rc != BNXT_TF_RC_SUCCESS)
		return rc;

	return rc;
}

static int bnxt_ulp_tc_validate_keys(struct bnxt *bp, unsigned int used_keys)
{
	unsigned int keys;

	/* KEY_CONTROL and KEY_BASIC are mandatory to form a meaningful key */
	if ((used_keys & BIT(FLOW_DISSECTOR_KEY_CONTROL)) == 0 ||
	    (used_keys & BIT(FLOW_DISSECTOR_KEY_BASIC)) == 0) {
		netdev_dbg(bp->dev, "%s: Invalid keys: 0x%x\n",
			   __func__, used_keys);
		return -EINVAL;
	}

	keys = used_keys & ~(ulp_supported_keys);
	if (keys) {
		netdev_dbg(bp->dev, "%s: Unsupported keys: 0x%x\n",
			   __func__, keys);
		return -EOPNOTSUPP;
	}

	return 0;
}

/* Function to handle the parsing of TC Flows and placing
 * the TC flow match fields into the ulp structures.
 */
int bnxt_ulp_tc_parser_hdr_parse(struct bnxt *bp,
				 struct flow_cls_offload *tc_flow_cmd,
				 struct ulp_tc_parser_params *params)
{
	struct flow_rule *rule = flow_cls_offload_flow_rule(tc_flow_cmd);
	struct flow_dissector *dissector = rule->match.dissector;
	unsigned int used_keys = dissector->used_keys;
	enum flow_dissector_key_id key;
	int rc;
	int i;

	rc = bnxt_ulp_tc_validate_keys(bp, used_keys);
	if (rc)
		return rc;

	netdev_dbg(bp->dev, "%s: Used keys:0x%x\n", __func__, used_keys);
	params->field_idx = BNXT_ULP_PROTO_HDR_SVIF_NUM;

	/* Parse all the keys in the rule */
	for (i = 0; i < NUM_DISSECTOR_KEYS; i++)  {
		key = ulp_hdr_parse_sequence[i];

		/* Key not present in the rule ? */
		if (!flow_rule_match_key(rule, key))
			continue;

		rc = bnxt_ulp_tc_parse_hdr_key(bp, rule, params, key,
					       used_keys);
		if (rc != BNXT_TF_RC_SUCCESS)
			return rc;
	}

	if (bnxt_ulp_tc_is_implicit_eth_addrs(params, FLOW_DISSECTOR_KEY_BASIC,
					      used_keys))
		bnxt_ulp_tc_parse_implicit_eth_addrs(bp, params);
	if (bnxt_ulp_tc_is_implicit_ipv4(params, used_keys))
		bnxt_ulp_tc_parse_implicit_ipv4(bp, params);
	else if (bnxt_ulp_tc_is_implicit_ipv6(params, used_keys))
		bnxt_ulp_tc_parse_implicit_ipv6(bp, params);

	/* update the implied SVIF */
	return ulp_tc_parser_implicit_match_port_process(params);
}

/* Function to handle the implicit action port id */
int ulp_tc_parser_implicit_act_port_process(struct bnxt *bp,
					    struct ulp_tc_parser_params
					    *params)
{
#ifdef HAVE_FLOW_OFFLOAD_H
	struct flow_action_entry implicit_port_act;
#else
	struct tcf_mirred implicit_port_act;
#endif
	/* Read the action port set bit */
	if (ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_ACT_PORT_IS_SET)) {
		/* Already set, so just exit */
		return BNXT_TF_RC_SUCCESS;
	}

#ifdef HAVE_FLOW_OFFLOAD_H
	implicit_port_act.dev = bp->dev;
#else
	implicit_port_act.tcfm_dev = bp->dev;
#endif
	return ulp_tc_redirect_act_handler(bp, params, &implicit_port_act);
}

#ifdef HAVE_FLOW_OFFLOAD_H
static int ulp_tc_parser_process_classid(struct bnxt *bp,
					 struct ulp_tc_parser_params *params,
					 u32 classid)
{
	struct ulp_tc_act_prop *act = &params->act_prop;
	u16 queue_id = TC_H_MIN(classid);
	u32 mtype;

	mtype = ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_MATCH_PORT_TYPE);
	if (mtype != BNXT_ULP_INTF_TYPE_PF) {
		netdev_dbg(bp->dev, "Queue action on invalid port type: %d\n",
			   mtype);
		return BNXT_TF_RC_PARSE_ERR_NOTSUPP;
	}

	netdev_dbg(bp->dev, "%s: classid: 0x%x queue_id: %d\n",
		   __func__, classid, queue_id);
	memcpy(&act->act_details[BNXT_ULP_ACT_PROP_IDX_QUEUE_INDEX],
	       &queue_id, BNXT_ULP_ACT_PROP_SZ_QUEUE_INDEX);
	ULP_BITMAP_SET(params->act_bitmap.bits, BNXT_ULP_ACT_BIT_QUEUE);

	return BNXT_TF_RC_SUCCESS;
}

static int ulp_tc_parer_implicit_dscp_process(struct bnxt *bp,
					      struct ulp_tc_parser_params
						*params);

/* Function to handle the parsing of TC Flows and placing
 * the TC flow actions into the ulp structures.
 */
int bnxt_ulp_tc_parser_act_parse(struct bnxt *bp,
				 struct flow_cls_offload *tc_flow_cmd,
				 struct ulp_tc_parser_params *params)
{
	struct flow_rule *rule = flow_cls_offload_flow_rule(tc_flow_cmd);
	struct netlink_ext_ack *extack = tc_flow_cmd->common.extack;
	struct flow_action *flow_action = &rule->action;
	struct bnxt_ulp_tc_act_info *act_info;
	struct flow_action_entry *act;
	int rc = BNXT_TF_RC_ERROR;
	int i;

	if (!flow_action_has_entries(flow_action) && !tc_flow_cmd->classid) {
		netdev_dbg(bp->dev, "no actions\n");
		return rc;
	}

	if (!flow_action_basic_hw_stats_check(flow_action, extack))
		return rc;

	if (tc_flow_cmd->classid) {
		rc = ulp_tc_parser_process_classid(bp, params,
						   tc_flow_cmd->classid);
		if (rc == BNXT_TF_RC_SUCCESS)
			goto done;
		return rc;
	}

	/* Parse all the actions in the rule */
	flow_action_for_each(i, act, flow_action) {
		act_info = &ulp_act_info[act->id];
		if (act_info->act_type == BNXT_ULP_ACT_TYPE_NOT_SUPPORTED) {
			netdev_dbg(bp->dev,
				   "Truflow parser does not support act %d\n",
				   act->id);
			return rc;
		}

		if (act_info->proto_act_func) {
			if (act_info->proto_act_func(bp, params, act) !=
				BNXT_TF_RC_SUCCESS) {
				return rc;
			}
		}
	}

done:
	/* Set count action in the action bitmap */
	ULP_BITMAP_SET(params->act_bitmap.bits, BNXT_ULP_ACT_BIT_COUNT);

	ulp_tc_parer_implicit_dscp_process(bp, params);

	/* update the implied port details */
	if (!ULP_BITMAP_ISSET(params->act_bitmap.bits, BNXT_ULP_ACT_BIT_QUEUE))
		ulp_tc_parser_implicit_act_port_process(bp, params);

	return BNXT_TF_RC_SUCCESS;
}

#else	/* HAVE_FLOW_OFFLOAD_H */

static enum flow_action_id tcf_exts_to_act_id(const struct tc_action *tc_act)
{
	enum flow_action_id act_id;

	if (is_tcf_gact_shot(tc_act)) {				/* Drop */
		act_id = FLOW_ACTION_DROP;
	} else if (is_tcf_mirred_egress_redirect(tc_act)) {	/* Redirect */
		act_id = FLOW_ACTION_REDIRECT;
	} else if (is_tcf_tunnel_set(tc_act)) {			/* Tnl encap */
		act_id = FLOW_ACTION_TUNNEL_ENCAP;
	} else if (is_tcf_tunnel_release(tc_act)) {		/* Tnl decap */
		act_id = FLOW_ACTION_TUNNEL_DECAP;
	} else if (is_tcf_pedit(tc_act)) {			/* Pkt edit */
		act_id = FLOW_ACTION_MANGLE;
	} else if (is_tcf_csum(tc_act)) {			/* Checksum */
		act_id = FLOW_ACTION_CSUM;
	} else if (is_tcf_vlan(tc_act)) {
		switch (tcf_vlan_action(tc_act)) {
		case TCA_VLAN_ACT_PUSH:				/* VLAN Push */
			act_id = FLOW_ACTION_VLAN_PUSH;
			break;
		case TCA_VLAN_ACT_POP:				/* VLAN Pop */
			act_id = FLOW_ACTION_VLAN_POP;
			break;
		default:
			act_id = FLOW_ACTION_INVALID;
			break;
		}
	} else if (is_tcf_gact_goto_chain(tc_act)) {
		act_id = FLOW_ACTION_GOTO;
	} else {
		act_id = FLOW_ACTION_INVALID;
	}

	return act_id;
}

/* Function to handle the parsing of TC Flows and placing
 * the TC flow actions into the ulp structures.
 */
int bnxt_ulp_tc_parser_act_parse(struct bnxt *bp,
				 struct flow_cls_offload *tc_flow_cmd,
				 struct ulp_tc_parser_params *params)
{
	struct tcf_exts *tc_exts = tc_flow_cmd->exts;
	struct bnxt_ulp_tc_act_info *act_info;
	enum flow_action_id act_id;
	struct tc_action *tc_act;
#ifndef HAVE_TC_EXTS_FOR_ACTION
	LIST_HEAD(tc_actions);
#else
	int i;
#endif

	if (!tcf_exts_has_actions(tc_exts)) {
		netdev_info(bp->dev, "no actions");
		return -EINVAL;
	}

#ifndef HAVE_TC_EXTS_FOR_ACTION
	tcf_exts_to_list(tc_exts, &tc_actions);
	list_for_each_entry(tc_act, &tc_actions, list) {
#else
	tcf_exts_for_each_action(i, tc_act, tc_exts) {
#endif
		act_id = tcf_exts_to_act_id(tc_act);
		act_info = &ulp_act_info[act_id];

		if (act_info->act_type == BNXT_ULP_ACT_TYPE_NOT_SUPPORTED) {
			netdev_dbg(bp->dev,
				   "Truflow parser does not support act %d\n",
				   act_id);
			return BNXT_TF_RC_ERROR;
		}

		if (act_info->proto_act_func) {
			if (act_info->proto_act_func(bp, params, tc_act) !=
				BNXT_TF_RC_SUCCESS)
				return BNXT_TF_RC_ERROR;
		}
	}

	/* Set count action in the action bitmap */
	ULP_BITMAP_SET(params->act_bitmap.bits, BNXT_ULP_ACT_BIT_COUNT);

	/* update the implied port details */
	ulp_tc_parser_implicit_act_port_process(bp, params);

	return BNXT_TF_RC_SUCCESS;
}
#endif	/* HAVE_FLOW_OFFLOAD_H */

int ulp_tc_control_key_handler(struct bnxt *bp,
			       struct ulp_tc_parser_params *params,
			       void *match_arg)
{
	struct flow_match_control *match = match_arg;

	params->addr_type = match->key->addr_type;
	netdev_dbg(bp->dev, "Control key: addr_type: %d\n", params->addr_type);

	return BNXT_TF_RC_SUCCESS;
}

int ulp_tc_tnl_control_key_handler(struct bnxt *bp,
				   struct ulp_tc_parser_params *params,
				   void *match_arg)
{
	struct flow_match_control *match = match_arg;

	params->tnl_addr_type = match->key->addr_type;
	netdev_dbg(bp->dev, "Tunnel Control key: addr_type: %d\n",
		   params->tnl_addr_type);

	return BNXT_TF_RC_SUCCESS;
}

#define	BNXT_ULP_IS_ETH_TYPE_ARP(params)	\
	(cpu_to_be16((params)->n_proto) == ETH_P_ARP)
int ulp_tc_basic_key_handler(struct bnxt *bp,
			     struct ulp_tc_parser_params *params,
			     void *match_arg)
{
	struct flow_match_basic *match = match_arg;

	params->n_proto = match->key->n_proto;
	if (BNXT_ULP_IS_ETH_TYPE_ARP(params)) {
		netdev_dbg(bp->dev, "ARP flow offload not supported\n");
		return BNXT_TF_RC_PARSE_ERR_NOTSUPP;
	}
	params->n_proto_mask = match->mask->n_proto;
	params->ip_proto = match->key->ip_proto;
	params->ip_proto_mask = match->mask->ip_proto;
	netdev_dbg(bp->dev, "Basic key: n_proto: 0x%x ip_proto: %d\n",
		   cpu_to_be16(params->n_proto), params->ip_proto);

	return BNXT_TF_RC_SUCCESS;
}

int ulp_tc_eth_addr_handler(struct bnxt *bp,
			    struct ulp_tc_parser_params *params,
			    void *match_arg)
{
	struct flow_match_eth_addrs *match = match_arg;
	u32 inner_flag = 0;
	bool allow_bc_mc;
	u32 idx = 0;
	u32 size;

	allow_bc_mc = bnxt_ulp_validate_bcast_mcast(bp);

	if (!allow_bc_mc && (is_multicast_ether_addr(match->key->dst) ||
			     is_broadcast_ether_addr(match->key->dst))) {
		netdev_dbg(bp->dev,
			   "Broadcast/Multicast flow offload unsupported\n");
		return BNXT_TF_RC_PARSE_ERR_NOTSUPP;
	}

	if (!allow_bc_mc && (is_multicast_ether_addr(match->key->src) ||
			     is_broadcast_ether_addr(match->key->src))) {
		netdev_dbg(bp->dev,
			   "Broadcast/Multicast flow offload unsupported\n");
		return BNXT_TF_RC_PARSE_ERR_NOTSUPP;
	}

	if (ulp_tc_prsr_fld_size_validate(params, &idx,
					  BNXT_ULP_PROTO_HDR_ETH_NUM)) {
		netdev_dbg(bp->dev, "Error parsing protocol header\n");
		return BNXT_TF_RC_ERROR;
	}

	/* Copy the key item for eth into hdr_field using ethernet
	 * header fields
	 */
	size = sizeof(match->key->dst);
	ulp_tc_prsr_fld_mask(params, &idx, size, match->key->dst,
			     match->mask->dst, ULP_PRSR_ACT_DEFAULT);

	size = sizeof(match->key->src);
	ulp_tc_prsr_fld_mask(params, &idx, size, match->key->src,
			     match->mask->src, ULP_PRSR_ACT_DEFAULT);

	size = sizeof(params->n_proto);
	ulp_tc_prsr_fld_mask(params, &idx, size, &params->n_proto,
			     &params->n_proto_mask, ULP_PRSR_ACT_MATCH_IGNORE);

	/* Parser expects the ethernet and vlan headers in wire format.
	 * So, when the vlan header is present, we set the tpid here
	 * and the vlan hdr parser sets the eth_type. Otherwise, we set
	 * the eth_type.
	 */
	if (params->vlan_tpid) {
		ulp_tc_prsr_fld_mask(params, &idx, size, &params->vlan_tpid,
				     &params->vlan_tpid_mask,
				     ULP_PRSR_ACT_MATCH_IGNORE);
	} else {
		ulp_tc_prsr_fld_mask(params, &idx, size, &params->n_proto,
				     &params->n_proto_mask,
				     ULP_PRSR_ACT_MATCH_IGNORE);
	}

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
	}

	/* Update the field protocol hdr bitmap */
	if (!params->vlan_tpid) {
		ulp_tc_l2_proto_type_update(params, params->n_proto,
					    inner_flag);
	}

	return BNXT_TF_RC_SUCCESS;
}

int ulp_tc_vlan_handler(struct bnxt *bp, struct ulp_tc_parser_params *params,
			void *match_arg)
{
	struct flow_match_vlan *match = match_arg;
	u16 vlan_tag_mask = 0, priority_mask = 0;
	struct ulp_tc_hdr_bitmap *hdr_bit;
	u16 vlan_tag = 0, priority = 0;
	u32 outer_vtag_num;
	u32 inner_vtag_num;
	u32 inner_flag = 0;
	u32 idx = 0;
	u32 size;

	if (match->key) {
		priority = htons(match->key->vlan_priority);
		vlan_tag = htons(match->key->vlan_id);
	}

	if (match->mask) {
		priority_mask = htons(match->mask->vlan_priority);
		vlan_tag_mask = match->mask->vlan_id;
		vlan_tag_mask &= 0xfff;

		/* The storage for priority and vlan tag is 2 bytes.
		 * The mask of priority which is 3 bits, if it is all 1's
		 * then make the rest bits 13 bits as 1's so that it is
		 * matched as exact match.
		 */
		if (priority_mask == ULP_VLAN_PRIORITY_MASK)
			priority_mask |= ~ULP_VLAN_PRIORITY_MASK;
		if (vlan_tag_mask == ULP_VLAN_TAG_MASK)
			vlan_tag_mask |= ~ULP_VLAN_TAG_MASK;
		vlan_tag_mask = htons(vlan_tag_mask);
	}

	if (ulp_tc_prsr_fld_size_validate(params, &idx,
					  BNXT_ULP_PROTO_HDR_S_VLAN_NUM)) {
		netdev_dbg(bp->dev, "Error parsing protocol header\n");
		return BNXT_TF_RC_ERROR;
	}

	size = sizeof(vlan_tag);
	/* The priority field is ignored since OVS is setting it as
	 * wild card match and it is not supported. This is a work
	 * around and shall be addressed in the future.
	 */
	ulp_tc_prsr_fld_mask(params, &idx, size, &priority, &priority_mask,
			     ULP_PRSR_ACT_MASK_IGNORE);

	ulp_tc_prsr_fld_mask(params, &idx, size, &vlan_tag, &vlan_tag_mask,
			     ULP_PRSR_ACT_DEFAULT);

	/* Parser expects the ethernet and vlan headers in wire format.
	 * So, when the vlan header is present, we set the eth_type here
	 * and the eth hdr parser would have set the tpid.
	 */
	size = sizeof(params->n_proto);
	ulp_tc_prsr_fld_mask(params, &idx, size, &params->n_proto,
			     &params->n_proto_mask, ULP_PRSR_ACT_MATCH_IGNORE);

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
		if (match->mask && vlan_tag_mask)
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
		if (match->mask && vlan_tag_mask)
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
		if (match->mask && vlan_tag_mask)
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
		if (match->mask && vlan_tag_mask)
			ULP_COMP_FLD_IDX_WR(params,
					    BNXT_ULP_CF_IDX_II_VLAN_FB_VID, 1);
		inner_flag = 1;
	} else {
		netdev_dbg(bp->dev, "%s: VLAN hdr found without eth\n",
			   __func__);
		return BNXT_TF_RC_ERROR;
	}

	ulp_tc_l2_proto_type_update(params, params->n_proto, inner_flag);
	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the update of proto header based on field values */
static void ulp_tc_l3_proto_type_update(struct ulp_tc_parser_params *param,
					u8 proto, u32 in_flag)
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

static int ulp_tc_ipv4_ctrl_handler(struct bnxt *bp,
				    struct ulp_tc_parser_params *params,
				    void *match_arg)
{
	struct flow_match_ip *match = match_arg;
	u16 val16 = 0;
	u8 val8 = 0;
	u32 idx = 0;
	u32 size;
	u32 cnt;

	/* validate there are no 3rd L3 header */
	cnt = ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_L3_HDR_CNT);
	if (cnt == 2) {
		netdev_dbg(bp->dev, "Parse Err:Third L3 header not supported\n");
		return BNXT_TF_RC_ERROR;
	}

	if (ulp_tc_prsr_fld_size_validate(params, &idx,
					  BNXT_ULP_PROTO_HDR_IPV4_NUM - 2)) {
		netdev_dbg(bp->dev, "Error parsing protocol header\n");
		return BNXT_TF_RC_ERROR;
	}

	/* version_ihl */
	size = sizeof(val8);
	ulp_tc_prsr_fld_mask(params, &idx, size, &val8, &val8,
			     ULP_PRSR_ACT_DEFAULT);

	/* tos: Ignore for matching templates with tunnel flows */
	size = sizeof(match->key->tos);
	ulp_tc_prsr_fld_mask(params, &idx, size, &match->key->tos,
			     &match->mask->tos,
			     params->tnl_addr_type ? ULP_PRSR_ACT_MATCH_IGNORE :
			     ULP_PRSR_ACT_DEFAULT);

	/* total_length */
	size = sizeof(val16);
	ulp_tc_prsr_fld_mask(params, &idx, size, &val16, &val16,
			     ULP_PRSR_ACT_DEFAULT);

	/* packet_id */
	size = sizeof(val16);
	ulp_tc_prsr_fld_mask(params, &idx, size, &val16, &val16,
			     ULP_PRSR_ACT_DEFAULT);

	/* fragment_offset */
	size = sizeof(val16);
	ulp_tc_prsr_fld_mask(params, &idx, size, &val16, &val16,
			     ULP_PRSR_ACT_DEFAULT);

	/* ttl */
	size = sizeof(match->key->ttl);
	ulp_tc_prsr_fld_mask(params, &idx, size, &match->key->ttl,
			     &match->mask->ttl, ULP_PRSR_ACT_DEFAULT);

	/* next_proto_id: Ignore proto for matching templates */
	size = sizeof(params->ip_proto);
	ulp_tc_prsr_fld_mask(params, &idx, size, &params->ip_proto,
			     &params->ip_proto_mask,
			     ULP_PRSR_ACT_MATCH_IGNORE);

	/* hdr_checksum */
	size = sizeof(val16);
	ulp_tc_prsr_fld_mask(params, &idx, size, &val16, &val16,
			     ULP_PRSR_ACT_DEFAULT);

	return BNXT_TF_RC_SUCCESS;
}

static int ulp_tc_ipv6_ctrl_handler(struct bnxt *bp,
				    struct ulp_tc_parser_params *params,
				    void *match_arg)
{
	struct flow_match_ip *match = match_arg;
	u32 val32 = 0;
	u16 val16 = 0;
	u32 idx = 0;
	u32 size;
	u32 cnt;

	/* validate there are no 3rd L3 header */
	cnt = ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_L3_HDR_CNT);
	if (cnt == 2) {
		netdev_dbg(bp->dev, "Parse Err:Third L3 header not supported\n");
		return BNXT_TF_RC_ERROR;
	}

	if (ulp_tc_prsr_fld_size_validate(params, &idx,
					  BNXT_ULP_PROTO_HDR_IPV6_NUM - 2)) {
		netdev_dbg(bp->dev, "Error parsing protocol header\n");
		return BNXT_TF_RC_ERROR;
	}

	/* version */
	size = sizeof(val32);
	ulp_tc_prsr_fld_mask(params, &idx, size, &val32, &val32,
			     ULP_PRSR_ACT_DEFAULT);

	/* traffic class: Ignore for matching templates with tunnel flows */
	size = sizeof(match->key->tos);
	ulp_tc_prsr_fld_mask(params, &idx, size, &match->key->tos,
			     &match->mask->tos,
			     params->tnl_addr_type ? ULP_PRSR_ACT_MATCH_IGNORE :
			     ULP_PRSR_ACT_DEFAULT);

	/* flow label: Ignore for matching templates */
	size = sizeof(val32);
	ulp_tc_prsr_fld_mask(params, &idx, size, &val32, &val32,
			     ULP_PRSR_ACT_MASK_IGNORE);

	/* payload length */
	size = sizeof(val16);
	ulp_tc_prsr_fld_mask(params, &idx, size, &val16, &val16,
			     ULP_PRSR_ACT_DEFAULT);

	/* next_proto_id: Ignore proto for matching templates */
	size = sizeof(params->ip_proto);
	ulp_tc_prsr_fld_mask(params, &idx, size, &params->ip_proto,
			     &params->ip_proto_mask,
			     ULP_PRSR_ACT_MATCH_IGNORE);
	/* hop limit (ttl) */
	size = sizeof(match->key->ttl);
	ulp_tc_prsr_fld_mask(params, &idx, size, &match->key->ttl,
			     &match->mask->ttl, ULP_PRSR_ACT_DEFAULT);

	return BNXT_TF_RC_SUCCESS;
}

int ulp_tc_ip_ctrl_handler(struct bnxt *bp,
			   struct ulp_tc_parser_params *params,
			   void *match_arg)
{
	if (params->n_proto == cpu_to_be16(ETH_P_IP))
		return ulp_tc_ipv4_ctrl_handler(bp, params, match_arg);
	if (params->n_proto == cpu_to_be16(ETH_P_IPV6))
		return ulp_tc_ipv6_ctrl_handler(bp, params, match_arg);
	return BNXT_TF_RC_ERROR;
}

/* Function to handle the parsing of IPV4 Header. */
static int ulp_tc_parse_ipv4_addr(struct bnxt *bp,
				  struct ulp_tc_parser_params *params,
				  void *match_arg)
{
	struct ulp_tc_hdr_bitmap *hdr_bitmap = &params->hdr_bitmap;
	struct flow_match_ipv4_addrs *match = match_arg;
	u32 inner_flag = 0;
	u8 proto = 0;
	u32 idx = 0;
	u32 size;
	u32 cnt;

	/* validate there is no 3rd L3 header */
	cnt = ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_L3_HDR_CNT);
	if (cnt == 2) {
		netdev_dbg(bp->dev, "Parse Err:Third L3 header not supported\n");
		return BNXT_TF_RC_ERROR;
	}

	if (ulp_tc_prsr_fld_size_validate(params, &idx,
					  BNXT_ULP_PROTO_HDR_IPV4_NUM - 8)) {
		netdev_dbg(bp->dev, "Error parsing protocol header\n");
		return BNXT_TF_RC_ERROR;
	}

	size = sizeof(match->key->src);
	ulp_tc_prsr_fld_mask(params, &idx, size, &match->key->src,
			     &match->mask->src, ULP_PRSR_ACT_DEFAULT);

	size = sizeof(match->key->dst);
	ulp_tc_prsr_fld_mask(params, &idx, size, &match->key->dst,
			     &match->mask->dst, ULP_PRSR_ACT_DEFAULT);

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

	/* Some of the applications may set the protocol field
	 * in the IPv4 match but don't set the mask. So, consider
	 * the mask in the proto value calculation.
	 */
	proto = params->ip_proto & params->ip_proto_mask;

	/* Update the field protocol hdr bitmap */
	ulp_tc_l3_proto_type_update(params, proto, inner_flag);
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_L3_HDR_CNT, ++cnt);
	netdev_dbg(bp->dev, "%s: l3-hdr-cnt: %d\n", __func__, cnt);

	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of IPV6 Header. */
static int ulp_tc_parse_ipv6_addr(struct bnxt *bp,
				  struct ulp_tc_parser_params *params,
				  void *match_arg)
{
	struct ulp_tc_hdr_bitmap *hdr_bitmap = &params->hdr_bitmap;
	struct flow_match_ipv6_addrs *match = match_arg;
	u32 inner_flag = 0;
	u8 proto = 0;
	u32 idx = 0;
	u32 size;
	u32 cnt;

	/* validate there is no 3rd L3 header */
	cnt = ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_L3_HDR_CNT);
	if (cnt == 2) {
		netdev_dbg(bp->dev, "Parse Err:Third L3 header not supported\n");
		return BNXT_TF_RC_ERROR;
	}

	if (ulp_tc_prsr_fld_size_validate(params, &idx,
					  BNXT_ULP_PROTO_HDR_IPV6_NUM - 6)) {
		netdev_dbg(bp->dev, "Error parsing protocol header\n");
		return BNXT_TF_RC_ERROR;
	}

	size = sizeof(match->key->src);
	ulp_tc_prsr_fld_mask(params, &idx, size, &match->key->src,
			     &match->mask->src, ULP_PRSR_ACT_DEFAULT);

	size = sizeof(match->key->dst);
	ulp_tc_prsr_fld_mask(params, &idx, size, &match->key->dst,
			     &match->mask->dst, ULP_PRSR_ACT_DEFAULT);

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
	}

	/* Some of the applications may set the protocol field
	 * in the IPv6 match but don't set the mask. So, consider
	 * the mask in the proto value calculation.
	 */
	proto = params->ip_proto & params->ip_proto_mask;

	/* Update the field protocol hdr bitmap */
	ulp_tc_l3_proto_type_update(params, proto, inner_flag);
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_L3_HDR_CNT, ++cnt);
	netdev_dbg(bp->dev, "%s: l3-hdr-cnt: %d\n", __func__, cnt);

	return BNXT_TF_RC_SUCCESS;
}

int ulp_tc_ipv4_addr_handler(struct bnxt *bp,
			     struct ulp_tc_parser_params *params,
			     void *match_arg)
{
	/* Dissector keys are set for both IPV4 and IPV6. Check addr_type
	 * (from KEY_CONTROL which is already processed) to resolve this.
	 */
	if (params->addr_type == FLOW_DISSECTOR_KEY_IPV4_ADDRS)
		return ulp_tc_parse_ipv4_addr(bp, params, match_arg);

	return BNXT_TF_RC_SUCCESS;
}

int ulp_tc_ipv6_addr_handler(struct bnxt *bp,
			     struct ulp_tc_parser_params *params,
			     void *match_arg)
{
	/* Dissector keys are set for both IPV4 and IPV6. Check addr_type
	 * (from KEY_CONTROL which is already processed) to resolve this.
	 */
	if (params->addr_type == FLOW_DISSECTOR_KEY_IPV6_ADDRS)
		return ulp_tc_parse_ipv6_addr(bp, params, match_arg);

	return BNXT_TF_RC_SUCCESS;
}

static void ulp_tc_l4_proto_type_update(struct ulp_tc_parser_params *params,
					u16 src_port, u16 src_mask,
					u16 dst_port, u16 dst_mask,
					enum bnxt_ulp_hdr_bit hdr_bit)
{
	u16 static_port = 0;

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
			    cpu_to_be16(dst_port));

	/* vxlan static customized port */
	if (ULP_APP_STATIC_VXLAN_PORT_EN(params->ulp_ctx)) {
		static_port = bnxt_ulp_cntxt_vxlan_ip_port_get(params->ulp_ctx);
		if (!static_port)
			static_port = bnxt_ulp_cntxt_vxlan_port_get(params->ulp_ctx);

		/* if udp and equal to static vxlan port then set tunnel bits */
		if (static_port && dst_port == cpu_to_be16(static_port)) {
			ULP_COMP_FLD_IDX_WR(params,
					    BNXT_ULP_CF_IDX_L3_TUN, 1);
			ULP_BITMAP_SET(params->cf_bitmap,
				       BNXT_ULP_CF_BIT_IS_TUNNEL);
		}
	} else {
		/* if dynamic Vxlan is enabled then skip dport checks */
		if (ULP_APP_DYNAMIC_VXLAN_PORT_EN(params->ulp_ctx))
			return;

		/* Vxlan and GPE port check */
		if (dst_port == cpu_to_be16(ULP_UDP_PORT_VXLAN)) {
			ULP_BITMAP_SET(params->hdr_fp_bit.bits,
				       BNXT_ULP_HDR_BIT_T_VXLAN);
			ULP_COMP_FLD_IDX_WR(params,
					    BNXT_ULP_CF_IDX_L3_TUN, 1);
			ULP_BITMAP_SET(params->cf_bitmap,
				       BNXT_ULP_CF_BIT_IS_TUNNEL);
		}
	}
}

static int ulp_tc_udp_handler(struct bnxt *bp,
			      struct ulp_tc_parser_params *params,
			      void *match_arg)
{
	struct ulp_tc_hdr_bitmap *hdr_bitmap = &params->hdr_bitmap;
	enum bnxt_ulp_hdr_bit out_l4 = BNXT_ULP_HDR_BIT_O_UDP;
	struct flow_match_ports *match = match_arg;
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

	if (match->key) {
		sport = match->key->src;
		dport = match->key->dst;
	}
	if (match->mask) {
		sport_mask = match->mask->src;
		dport_mask = match->mask->dst;
	}

	if (ulp_tc_prsr_fld_size_validate(params, &idx,
					  BNXT_ULP_PROTO_HDR_UDP_NUM)) {
		netdev_dbg(bp->dev, "Error parsing protocol header\n");
		return BNXT_TF_RC_ERROR;
	}

	size = sizeof(match->key->src);
	ulp_tc_prsr_fld_mask(params, &idx, size, &match->key->src,
			     &match->mask->src, ULP_PRSR_ACT_DEFAULT);

	size = sizeof(match->key->dst);
	ulp_tc_prsr_fld_mask(params, &idx, size, &match->key->dst,
			     &match->mask->dst, ULP_PRSR_ACT_DEFAULT);

	size = sizeof(dgram_len);
	ulp_tc_prsr_fld_mask(params, &idx, size, &dgram_len, &dgram_len,
			     ULP_PRSR_ACT_DEFAULT);

	size = sizeof(dgram_cksum);
	ulp_tc_prsr_fld_mask(params, &idx, size, &dgram_cksum, &dgram_cksum,
			     ULP_PRSR_ACT_DEFAULT);

	/* Set the udp header bitmap and computed l4 header bitmaps */
	if (ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_UDP) ||
	    ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_TCP) ||
	    ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_L3_TUN))
		out_l4 = BNXT_ULP_HDR_BIT_I_UDP;

	ulp_tc_l4_proto_type_update(params, sport, sport_mask, dport,
				    dport_mask, out_l4);
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_L4_HDR_CNT, ++cnt);

	return BNXT_TF_RC_SUCCESS;
}

int ulp_tc_tcp_ctrl_handler(struct bnxt *bp,
			    struct ulp_tc_parser_params *params,
			    void *match_arg)
{
	struct flow_match_tcp *match = match_arg;
	u32 val32 = 0;
	u16 val16 = 0;
	u8 val8 = 0;
	u32 idx = 0;
	u32 size;
	u32 cnt;

	cnt = ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_L4_HDR_CNT);
	if (cnt == 2) {
		netdev_dbg(bp->dev,
			   "Parse Err:Third L4 header not supported\n");
		return BNXT_TF_RC_ERROR;
	}

	if (ulp_tc_prsr_fld_size_validate(params, &idx,
					  BNXT_ULP_PROTO_HDR_TCP_NUM - 2)) {
		netdev_dbg(bp->dev, "Error parsing protocol header\n");
		return BNXT_TF_RC_ERROR;
	}

	/* seq num */
	size = sizeof(val32);
	ulp_tc_prsr_fld_mask(params, &idx, size, &val32, &val32,
			     ULP_PRSR_ACT_DEFAULT);

	/* ack num */
	size = sizeof(val32);
	ulp_tc_prsr_fld_mask(params, &idx, size, &val32, &val32,
			     ULP_PRSR_ACT_DEFAULT);

	/* data offset */
	size = sizeof(val8);
	ulp_tc_prsr_fld_mask(params, &idx, size, &val8, &val8,
			     ULP_PRSR_ACT_DEFAULT);

	/* flags */
	size = sizeof(match->key->flags);
	ulp_tc_prsr_fld_mask(params, &idx, size, &match->key->flags,
			     &match->mask->flags, ULP_PRSR_ACT_DEFAULT);

	/* rx window */
	size = sizeof(val16);
	ulp_tc_prsr_fld_mask(params, &idx, size, &val16, &val16,
			     ULP_PRSR_ACT_DEFAULT);

	/* cksum */
	size = sizeof(val16);
	ulp_tc_prsr_fld_mask(params, &idx, size, &val16, &val16,
			     ULP_PRSR_ACT_DEFAULT);

	/* urg ptr */
	size = sizeof(val16);
	ulp_tc_prsr_fld_mask(params, &idx, size, &val16, &val16,
			     ULP_PRSR_ACT_DEFAULT);

	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_L4_HDR_CNT, ++cnt);
	return BNXT_TF_RC_SUCCESS;
}

static int ulp_tc_tcp_ports_handler(struct bnxt *bp,
				    struct ulp_tc_parser_params *params,
				    void *match_arg)
{
	struct ulp_tc_hdr_bitmap *hdr_bitmap = &params->hdr_bitmap;
	enum bnxt_ulp_hdr_bit out_l4 = BNXT_ULP_HDR_BIT_O_TCP;
	struct flow_match_ports *match = match_arg;
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

	if (match->key) {
		sport = match->key->src;
		dport = match->key->dst;
	}
	if (match->mask) {
		sport_mask = match->mask->src;
		dport_mask = match->mask->dst;
	}

	if (ulp_tc_prsr_fld_size_validate(params, &idx,
					  BNXT_ULP_PROTO_HDR_TCP_NUM - 7)) {
		netdev_dbg(bp->dev, "Error parsing protocol header\n");
		return BNXT_TF_RC_ERROR;
	}

	size = sizeof(match->key->src);
	ulp_tc_prsr_fld_mask(params, &idx, size, &match->key->src,
			     &match->mask->src, ULP_PRSR_ACT_DEFAULT);

	size = sizeof(match->key->dst);
	ulp_tc_prsr_fld_mask(params, &idx, size, &match->key->dst,
			     &match->mask->dst, ULP_PRSR_ACT_DEFAULT);

	if (ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_UDP) ||
	    ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_TCP) ||
	    ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_L3_TUN))
		out_l4 = BNXT_ULP_HDR_BIT_I_TCP;

	ulp_tc_l4_proto_type_update(params, sport, sport_mask, dport,
				    dport_mask, out_l4);
	return BNXT_TF_RC_SUCCESS;
}

int ulp_tc_l4_ports_handler(struct bnxt *bp,
			    struct ulp_tc_parser_params *params,
			    void *match_arg)
{
	int rc = BNXT_TF_RC_ERROR;

	if (params->ip_proto != IPPROTO_TCP && params->ip_proto != IPPROTO_UDP)
		return rc;

	if (params->ip_proto == IPPROTO_UDP)
		rc = ulp_tc_udp_handler(bp, params, match_arg);
	else if (params->ip_proto == IPPROTO_TCP)
		rc = ulp_tc_tcp_ports_handler(bp, params, match_arg);

	return rc;
}

int ulp_tc_tnl_ip_ctrl_handler(struct bnxt *bp,
			       struct ulp_tc_parser_params *params,
			       void *match_arg)
{
	struct tc_match match;

	struct flow_dissector_key_eth_addrs key = {{0}};
	struct flow_dissector_key_eth_addrs mask = {{0}};

	ether_addr_copy(key.dst, params->tnl_dmac);
	if (!is_zero_ether_addr(key.dst))
		eth_broadcast_addr(mask.dst);

	ether_addr_copy(key.src, params->tnl_smac);
	if (!is_zero_ether_addr(key.src))
		eth_broadcast_addr(mask.src);

	match.key = &key;
	match.mask = &mask;

	/* This will be overwritten when basic key is parsed later.
	 * Setting here so eth_addr_handler() can use it to build
	 * tnl eth hdr match.
	 */
	if (params->tnl_addr_type == FLOW_DISSECTOR_KEY_IPV4_ADDRS)
		params->n_proto = cpu_to_be16(ETH_P_IP);
	else if (params->tnl_addr_type == FLOW_DISSECTOR_KEY_IPV6_ADDRS)
		params->n_proto = cpu_to_be16(ETH_P_IPV6);
	else
		return BNXT_TF_RC_ERROR;

	params->n_proto_mask = 0xffff;
	ulp_tc_eth_addr_handler(bp, params, &match);

	return ulp_tc_ip_ctrl_handler(bp, params, match_arg);
}

int ulp_tc_tnl_ipv4_addr_handler(struct bnxt *bp,
				 struct ulp_tc_parser_params *params,
				 void *match_arg)
{
	/* Dissector keys are set for both IPV4 and IPV6. Check tnl_addr_type
	 * (from KEY_CONTROL which is already processed) to resolve this.
	 */
	if (params->tnl_addr_type == FLOW_DISSECTOR_KEY_IPV4_ADDRS)
		return ulp_tc_parse_ipv4_addr(bp, params, match_arg);

	return BNXT_TF_RC_SUCCESS;
}

int ulp_tc_tnl_ipv6_addr_handler(struct bnxt *bp,
				 struct ulp_tc_parser_params *params,
				 void *match_arg)
{
	/* Dissector keys are set for both IPV4 and IPV6. Check tnl_addr_type
	 * (from KEY_CONTROL which is already processed) to resolve this.
	 */
	if (params->tnl_addr_type == FLOW_DISSECTOR_KEY_IPV6_ADDRS)
		return ulp_tc_parse_ipv6_addr(bp, params, match_arg);

	return BNXT_TF_RC_SUCCESS;
}

int ulp_tc_tnl_l4_ports_handler(struct bnxt *bp,
				struct ulp_tc_parser_params *params,
				void *match_arg)
{
	return ulp_tc_udp_handler(bp, params, match_arg);
}

static int ulp_tc_vxlan_handler(struct bnxt *bp,
				struct ulp_tc_parser_params *params,
				void *match_arg)
{
	struct ulp_parser_vxlan vxlan_mask = { 0x00, { 0x00, 0x00, 0x00 },
					       { 0xff, 0xff, 0xff }, 0x00 };
	struct ulp_tc_hdr_bitmap *hdr_bitmap = &params->hdr_bitmap;
	struct flow_match_enc_keyid *match = match_arg;
	struct ulp_parser_vxlan vxlan_key = { 0 };
	struct bnxt_ulp_context *ulp_ctx = bp->ulp_ctx;
	u16 dport, static_port;
	u32 vni_mask;
	u32 idx = 0;
	u32 size;
	u32 vni;

	if (ulp_tc_prsr_fld_size_validate(params, &idx,
					  BNXT_ULP_PROTO_HDR_VXLAN_NUM)) {
		netdev_dbg(bp->dev, "Error parsing protocol header\n");
		return BNXT_TF_RC_ERROR;
	}

	/* Update if the outer headers have any partial masks */
	if (!ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_WC_MATCH))
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_OUTER_EM_ONLY, 1);

	vni = match->key->keyid;
	vni = be32_to_cpu(vni);
	vni_mask = match->mask->keyid;

	netdev_dbg(bp->dev, "%s: vni: 0x%x mask: 0x%x\n", __func__,
		   vni, vni_mask);

	vxlan_key.vni[0] = (vni >> 16) & 0xff;
	vxlan_key.vni[1] = (vni >> 8) & 0xff;
	vxlan_key.vni[2] = vni & 0xff;
	vxlan_key.flags = 0x08;

	size = sizeof(vxlan_key.flags);
	ulp_tc_prsr_fld_mask(params, &idx, size, &vxlan_key.flags,
			     &vxlan_mask.flags, ULP_PRSR_ACT_DEFAULT);

	size = sizeof(vxlan_key.rsvd0);
	ulp_tc_prsr_fld_mask(params, &idx, size, &vxlan_key.rsvd0,
			     &vxlan_mask.rsvd0, ULP_PRSR_ACT_DEFAULT);

	size = sizeof(vxlan_key.vni);
	ulp_tc_prsr_fld_mask(params, &idx, size, &vxlan_key.vni,
			     &vxlan_mask.vni, ULP_PRSR_ACT_DEFAULT);

	size = sizeof(vxlan_key.rsvd1);
	ulp_tc_prsr_fld_mask(params, &idx, size, &vxlan_key.rsvd1,
			     &vxlan_mask.rsvd1, ULP_PRSR_ACT_DEFAULT);

	/* Update the hdr_bitmap with vxlan */
	ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_T_VXLAN);

	/* Set bits here due to DYNAMIC VXLAN feature */
	ULP_BITMAP_SET(params->hdr_fp_bit.bits, BNXT_ULP_HDR_BIT_T_VXLAN);
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_L3_TUN, 1);
	ULP_BITMAP_SET(params->cf_bitmap, BNXT_ULP_CF_BIT_IS_TUNNEL);

	dport = ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_O_L4_DST_PORT);
	if (!dport) {
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_L4_DST_PORT,
				    ULP_UDP_PORT_VXLAN);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_L4_DST_PORT_MASK,
				    ULP_UDP_PORT_VXLAN_MASK);
	}

	/* vxlan static customized port */
	if (ULP_APP_STATIC_VXLAN_PORT_EN(ulp_ctx)) {
		static_port = bnxt_ulp_cntxt_vxlan_ip_port_get(ulp_ctx);
		if (!static_port)
			static_port = bnxt_ulp_cntxt_vxlan_port_get(ulp_ctx);

		/* validate that static ports match if not reject */
		if (dport != 0 && dport != cpu_to_be16(static_port)) {
			netdev_dbg(bp->dev, "ParseErr:vxlan port is not valid\n");
			return BNXT_TF_RC_PARSE_ERR;
		} else if (dport == 0) {
			ULP_COMP_FLD_IDX_WR(params,
					    BNXT_ULP_CF_IDX_TUNNEL_PORT,
					    cpu_to_be16(static_port));
		}
	} else {
		/* dynamic vxlan support */
		if (ULP_APP_DYNAMIC_VXLAN_PORT_EN(params->ulp_ctx)) {
			if (dport == 0) {
				netdev_dbg(bp->dev,
					   "ParseErr:vxlan port is null\n");
				return BNXT_TF_RC_PARSE_ERR;
			}
			/* set the dynamic vxlan port check */
			ULP_BITMAP_SET(params->cf_bitmap,
				       BNXT_ULP_CF_BIT_DYNAMIC_VXLAN_PORT);
			ULP_COMP_FLD_IDX_WR(params,
					    BNXT_ULP_CF_IDX_TUNNEL_PORT, dport);
		} else if (dport != 0 && dport != ULP_UDP_PORT_VXLAN) {
			/* set the dynamic vxlan port check */
			ULP_BITMAP_SET(params->cf_bitmap,
				       BNXT_ULP_CF_BIT_DYNAMIC_VXLAN_PORT);
			ULP_COMP_FLD_IDX_WR(params,
					    BNXT_ULP_CF_IDX_TUNNEL_PORT, dport);
		}
	}
	return BNXT_TF_RC_SUCCESS;
}

int ulp_tc_tnl_key_handler(struct bnxt *bp,
			   struct ulp_tc_parser_params *params,
			   void *match_arg)
{
	return ulp_tc_vxlan_handler(bp, params, match_arg);
}

int ulp_tc_tnl_mpls_handler(struct bnxt *bp,
			    struct ulp_tc_parser_params *params,
			    void *match_arg)
{
#ifdef HAVE_FLOW_DISSECTOR_KEY_MPLS_LSE
	struct ulp_tc_hdr_bitmap *hdr_bitmap = &params->hdr_bitmap;
	struct flow_match_mpls *match = match_arg;
	u32 key, mask, size;
	u32 idx = 0, lse_idx = 0;
	u32 tag_count = 0;

	if (ulp_tc_prsr_fld_size_validate(params, &idx,
					  BNXT_ULP_PROTO_HDR_MPLS_NUM)) {
		netdev_dbg(bp->dev, "Error parsing mpls protocol header\n");
		return BNXT_TF_RC_ERROR;
	}

	if (!match->mask->used_lses)
		return BNXT_TF_RC_SUCCESS;

	for (lse_idx = 0; lse_idx < FLOW_DIS_MPLS_MAX; lse_idx++) {
		/* if bit is not set then skip */
		if (!(match->mask->used_lses & 1 << lse_idx))
			continue;

		tag_count++;
		size = sizeof(key);
		key = match->key->ls[lse_idx].mpls_label;
		mask = match->mask->ls[lse_idx].mpls_label;
		key = htonl(key);
		mask = htonl(mask);

		/* update the mpls label */
		ulp_tc_prsr_fld_mask(params, &idx, size, &key,
				     &mask, ULP_PRSR_ACT_DEFAULT);

		/* update the mpls bottom of stack */
		key = match->key->ls[lse_idx].mpls_bos;
		mask = match->mask->ls[lse_idx].mpls_bos;
		size = 1;

		/* update the mpls label */
		ulp_tc_prsr_fld_mask(params, &idx, size, &key,
				     &mask, ULP_PRSR_ACT_DEFAULT);

		/* Update the hdr_bitmap with MPLS */
		if (tag_count == 1) {
			ULP_BITMAP_SET(hdr_bitmap->bits,
				       BNXT_ULP_HDR_BIT_T_MPLS);
		} else if (tag_count == 2) {
			ULP_BITMAP_SET(hdr_bitmap->bits,
				       BNXT_ULP_HDR_BIT_T_MPLS_INNER);
		} else {
			netdev_dbg(bp->dev, "Error cannot support matching more than 2 MPLS tags\n");
			return BNXT_TF_RC_ERROR;
		}
	}
	ULP_BITMAP_SET(params->cf_bitmap, BNXT_ULP_CF_BIT_IS_TUNNEL);
	return BNXT_TF_RC_SUCCESS;
#else
	netdev_dbg(bp->dev, "MPLS pattern for older kernel is not supported\n");
	return BNXT_TF_RC_ERROR;
#endif
}

#endif

#if defined(CONFIG_BNXT_FLOWER_OFFLOAD) || defined(CONFIG_BNXT_CUSTOM_FLOWER_OFFLOAD)
/* Function to handle the parsing of action ports. */
int ulp_tc_parser_act_port_set(struct ulp_tc_parser_params *param, u32 ifindex, bool lag)
{
	struct ulp_tc_act_prop *act = &param->act_prop;
	enum bnxt_ulp_intf_type port_type;
	enum bnxt_ulp_direction_type dir;
	u32 vnic_type;
	u16 pid_s;
	u32 pid;

	/* Get the direction */
	dir = ULP_COMP_FLD_IDX_RD(param, BNXT_ULP_CF_IDX_DIRECTION);
	port_type = ULP_COMP_FLD_IDX_RD(param, BNXT_ULP_CF_IDX_ACT_PORT_TYPE);
	if (dir == BNXT_ULP_DIR_EGRESS) {
		if (lag) {
			/* For egress direction, fill lag vport */
			if (ulp_port_db_lag_vport_get(param->ulp_ctx, ifindex, &pid_s))
				return BNXT_TF_RC_ERROR;
		} else {
			/* For egress direction, fill vport */
			if (ulp_port_db_vport_get(param->ulp_ctx, ifindex, &pid_s))
				return BNXT_TF_RC_ERROR;
		}
		pid = pid_s;
		pid = cpu_to_be32(pid);
		memcpy(&act->act_details[BNXT_ULP_ACT_PROP_IDX_VPORT],
		       &pid, BNXT_ULP_ACT_PROP_SZ_VPORT);
		if (port_type == BNXT_ULP_INTF_TYPE_VF_REP) {
			if (ulp_port_db_default_vnic_get(param->ulp_ctx,
							 ifindex,
							 BNXT_ULP_VF_FUNC_VNIC,
							 &pid_s))
				return BNXT_TF_RC_ERROR;
			pid = pid_s;

			/* Allows use of func_opcode with VNIC */
			ULP_COMP_FLD_IDX_WR(param, BNXT_ULP_CF_IDX_VNIC, pid);
		}
	} else {
		/* For ingress direction, fill vnic */
		if (port_type == BNXT_ULP_INTF_TYPE_VF_REP)
			vnic_type = BNXT_ULP_VF_FUNC_VNIC;
		else
			vnic_type = BNXT_ULP_DRV_FUNC_VNIC;

		if (ulp_port_db_default_vnic_get(param->ulp_ctx, ifindex,
						 vnic_type, &pid_s))
			return BNXT_TF_RC_ERROR;

		pid = pid_s;
		pid = cpu_to_be32(pid);
		memcpy(&act->act_details[BNXT_ULP_ACT_PROP_IDX_VNIC],
		       &pid, BNXT_ULP_ACT_PROP_SZ_VNIC);
	}

	/* Update the action port set bit */
	ULP_COMP_FLD_IDX_WR(param, BNXT_ULP_CF_IDX_ACT_PORT_IS_SET, 1);
	return BNXT_TF_RC_SUCCESS;
}
#endif

#ifdef CONFIG_BNXT_FLOWER_OFFLOAD
static int ulp_tc_parser_mirr_act_port_set(struct ulp_tc_parser_params *param,
					   u32 ifindex)
{
	struct ulp_tc_act_prop *act = &param->act_prop;
	enum bnxt_ulp_intf_type port_type;
	u32 vnic_type;
	u16 pid_s;
	u32 pid;

	/* Fill in vport */
	if (ulp_port_db_vport_get(param->ulp_ctx, ifindex, &pid_s))
		return BNXT_TF_RC_ERROR;

	pid = pid_s;
	pid = cpu_to_be32(pid);
	memcpy(&act->act_details[BNXT_ULP_ACT_PROP_IDX_MIRR_VPORT],
	       &pid, BNXT_ULP_ACT_PROP_SZ_MIRR_VPORT);
	ULP_COMP_FLD_IDX_WR(param, BNXT_ULP_CF_IDX_MIRR_VPORT, pid_s);

	/* ... and vnic */
	port_type = ULP_COMP_FLD_IDX_RD(param,
					BNXT_ULP_CF_IDX_ACT_MIRR_PORT_TYPE);
	if (port_type == BNXT_ULP_INTF_TYPE_VF_REP)
		vnic_type = BNXT_ULP_VF_FUNC_VNIC;
	else
		vnic_type = BNXT_ULP_DRV_FUNC_VNIC;

	if (ulp_port_db_default_vnic_get(param->ulp_ctx, ifindex,
					 vnic_type, &pid_s))
		return BNXT_TF_RC_ERROR;

	pid = pid_s;
	pid = cpu_to_be32(pid);
	memcpy(&act->act_details[BNXT_ULP_ACT_PROP_IDX_MIRR_VNIC],
	       &pid, BNXT_ULP_ACT_PROP_SZ_MIRR_VNIC);
	ULP_COMP_FLD_IDX_WR(param, BNXT_ULP_CF_IDX_MIRR_VNIC, pid_s);

	/* Update the action port set bit */
	ULP_COMP_FLD_IDX_WR(param, BNXT_ULP_CF_IDX_ACT_MIRR_PORT_IS_SET, 1);

	return BNXT_TF_RC_SUCCESS;
}

#ifndef HAVE_FLOW_OFFLOAD_H
static struct net_device *tcf_redir_dev(struct bnxt *bp,
					struct tc_action *tc_act)
{
#ifdef HAVE_TCF_MIRRED_DEV
	struct net_device *dev = tcf_mirred_dev(tc_act);
#else
	int ifindex = tcf_mirred_ifindex(tc_act);
	struct net_device *dev;

	dev = __dev_get_by_index(dev_net(bp->dev), ifindex);
#endif
	return dev;
}
#endif	/* !HAVE_FLOW_OFFLOAD_H */

static struct net_device *ulp_tc_get_redir_dev(struct bnxt *bp,
					       void *action_arg)
{
#ifdef HAVE_FLOW_OFFLOAD_H
	struct flow_action_entry *action = action_arg;

	return action->dev;
#else
	struct tc_action *action = action_arg;

	return tcf_redir_dev(bp, action);
#endif
}

int ulp_tc_redirect_act_handler(struct bnxt *bp,
				struct ulp_tc_parser_params *params,
				void *action_arg)
{
	struct ulp_tc_hdr_bitmap *act = &params->act_bitmap;
	enum bnxt_ulp_intf_type intf_type;
	struct net_device *redir_dev;
	u32 ifindex;
	u16 dst_fid;

	redir_dev = ulp_tc_get_redir_dev(bp, action_arg);
	if (!redir_dev) {
		netdev_dbg(bp->dev, "no dev in mirred action\n");
		return BNXT_TF_RC_ERROR;
	}

	if (ULP_BITMAP_ISSET(act->bits, BNXT_ULP_ACT_BIT_VXLAN_ENCAP))
		dst_fid = bp->pf.fw_fid;
	else
		dst_fid = bnxt_flow_get_dst_fid(bp, redir_dev);

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

#ifndef HAVE_FLOW_OFFLOAD_H
static struct net_device *tcf_mirror_dev(struct bnxt *bp,
					 struct tc_action *tc_act)
{
#ifdef HAVE_TCF_MIRRED_DEV
	struct net_device *dev = tcf_mirred_dev(tc_act);
#else
	int ifindex = tcf_mirred_ifindex(tc_act);
	struct net_device *dev;

	dev = __dev_get_by_index(dev_net(bp->dev), ifindex);
#endif
	return dev;
}
#endif	/* !HAVE_FLOW_OFFLOAD_H */

static struct net_device *ulp_tc_get_mirror_dev(struct bnxt *bp,
						void *action_arg)
{
#ifdef HAVE_FLOW_OFFLOAD_H
	struct flow_action_entry *action = action_arg;

	return action->dev;
#else
	struct tc_action *action = action_arg;

	return tcf_mirror_dev(bp, action);
#endif
}

static int ulp_tc_mirror_act_handler(struct bnxt *bp,
				     struct ulp_tc_parser_params *params,
				     void *action_arg)
{
	struct ulp_tc_hdr_bitmap *act = &params->act_bitmap;
	enum bnxt_ulp_intf_type intf_type;
	struct net_device *mirred_dev;
	u32 ifindex;
	u16 dst_fid;

	mirred_dev = ulp_tc_get_mirror_dev(bp, action_arg);
	if (!mirred_dev) {
		netdev_err(bp->dev, "no dev in mirred action\n");
		return BNXT_TF_RC_ERROR;
	}

	if (ULP_BITMAP_ISSET(act->bits, BNXT_ULP_ACT_BIT_VXLAN_ENCAP))
		dst_fid = bp->pf.fw_fid;
	else
		dst_fid = bnxt_flow_get_dst_fid(bp, mirred_dev);

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

	if (!ULP_BITMAP_ISSET(act->bits, BNXT_ULP_ACT_BIT_SHARED_SAMPLE)) {
		netdev_dbg(bp->dev, "%s: mirror ifindex[%u], intf_type[%u], dst_fid[%u]\n",
			   __func__, ifindex, intf_type, dst_fid);

		/* Set the mirror action port */
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_ACT_MIRR_PORT_TYPE, intf_type);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_DEV_ACT_MIRR_PORT_ID, dst_fid);

		/* Set the shared_sample bit */
		ULP_BITMAP_SET(params->act_bitmap.bits, BNXT_ULP_ACT_BIT_SHARED_SAMPLE);

		return ulp_tc_parser_mirr_act_port_set(params, ifindex);
	}

	netdev_dbg(bp->dev,
		   "%s: mirror->redirect ifindex[%u], intf_type[%u], dst_fid[%u]\n",
		   __func__, ifindex, intf_type, dst_fid);

	/* Override the action port, as this is a 2nd mirror destination */
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_ACT_PORT_TYPE, intf_type);
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_DEV_ACT_PORT_ID, dst_fid);

	return ulp_tc_parser_act_port_set(params, ifindex, false /* lag */);
}

int ulp_tc_ingress_mirror_act_handler(struct bnxt *bp,
				      struct ulp_tc_parser_params *params,
				      void *action_arg)
{
	netdev_dbg(bp->dev, "mirred action: ingress mirror\n");
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_MIRROR_COPY_ING_OR_EGR, 0);

	return ulp_tc_mirror_act_handler(bp, params, action_arg);
}

int ulp_tc_egress_mirror_act_handler(struct bnxt *bp,
				     struct ulp_tc_parser_params *params,
				     void *action_arg)
{
	netdev_dbg(bp->dev, "mirred action: egress mirror\n");
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_MIRROR_COPY_ING_OR_EGR, 1);

	return ulp_tc_mirror_act_handler(bp, params, action_arg);
}

static void ulp_encap_copy_eth(struct ulp_tc_parser_params *params,
			       struct bnxt_tc_l2_key *l2_info,
			       u16 eth_type)
{
	struct ulp_tc_hdr_field *field;
	u32 size;

	field = &params->enc_field[BNXT_ULP_ENC_FIELD_ETH_DMAC];
	size = sizeof(l2_info->dmac);

	field = ulp_tc_parser_fld_copy(field, l2_info->dmac, size);
	field = ulp_tc_parser_fld_copy(field, l2_info->smac, size);

	size = sizeof(eth_type);
	field = ulp_tc_parser_fld_copy(field, &eth_type, size);

	ULP_BITMAP_SET(params->enc_hdr_bitmap.bits, BNXT_ULP_HDR_BIT_O_ETH);
}

static void ulp_encap_copy_ipv4(struct ulp_tc_parser_params *params,
				struct ip_tunnel_key *tun_key)
{
	struct ulp_tc_act_prop *ap = &params->act_prop;
	struct ulp_tc_hdr_field *field;
	u32 ip_size, ip_type;
	u16 val16;
	u32 size;
	u8 val8;

	ip_size = cpu_to_be32(BNXT_ULP_ENCAP_IPV4_SIZE);
	ip_type = cpu_to_be32(BNXT_ULP_ETH_IPV4);

	/* Update the ip size details */
	memcpy(&ap->act_details[BNXT_ULP_ACT_PROP_IDX_ENCAP_IP_SZ],
	       &ip_size, sizeof(u32));

	/* update the ip type */
	memcpy(&ap->act_details[BNXT_ULP_ACT_PROP_IDX_ENCAP_L3_TYPE],
	       &ip_type, sizeof(u32));

	/* update the computed field to notify it is ipv4 header */
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_ACT_ENCAP_IPV4_FLAG, 1);

	field = &params->enc_field[BNXT_ULP_ENC_FIELD_IPV4_IHL];

	/* version_ihl */
	val8 = 0x45;
	size = sizeof(val8);
	field = ulp_tc_parser_fld_copy(field, &val8, size);

	/* tos */
	size = sizeof(tun_key->tos);
	field = ulp_tc_parser_fld_copy(field, &tun_key->tos, size);

	/* packet_id */
	val16 = 0;
	size = sizeof(val16);
	field = ulp_tc_parser_fld_copy(field, &val16, size);

	/* fragment_offset */
	size = sizeof(val16);
	field = ulp_tc_parser_fld_copy(field, &val16, size);

	/* ttl */
	size = sizeof(tun_key->ttl);
	if (!tun_key->ttl)
		val8 = BNXT_ULP_DEFAULT_TTL;
	else
		val8 = tun_key->ttl;
	field = ulp_tc_parser_fld_copy(field, &val8, size);

	/* next_proto_id */
	val8 = 0;
	size = sizeof(val8);
	field = ulp_tc_parser_fld_copy(field, &val8, size);

	size = sizeof(tun_key->u.ipv4.src);
	field = ulp_tc_parser_fld_copy(field, &tun_key->u.ipv4.src, size);

	size = sizeof(tun_key->u.ipv4.dst);
	field = ulp_tc_parser_fld_copy(field, &tun_key->u.ipv4.dst, size);

	ULP_BITMAP_SET(params->enc_hdr_bitmap.bits, BNXT_ULP_HDR_BIT_O_IPV4);
}

static void ulp_encap_copy_ipv6(struct ulp_tc_parser_params *params,
				struct ip_tunnel_key *tun_key)
{
	struct ulp_tc_act_prop *ap = &params->act_prop;
	u32 ip_size, ip_type, val32, size;
	struct ulp_tc_hdr_field *field;
	u8 val8;

	ip_size = cpu_to_be32(BNXT_ULP_ENCAP_IPV6_SIZE);
	ip_type = cpu_to_be32(BNXT_ULP_ETH_IPV6);

	/* Update the ip size details */
	memcpy(&ap->act_details[BNXT_ULP_ACT_PROP_IDX_ENCAP_IP_SZ],
	       &ip_size, sizeof(u32));

	/* update the ip type */
	memcpy(&ap->act_details[BNXT_ULP_ACT_PROP_IDX_ENCAP_L3_TYPE],
	       &ip_type, sizeof(u32));

	/* update the computed field to notify it is ipv4 header */
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_ACT_ENCAP_IPV6_FLAG, 1);

	/* Version (4b), Traffic Class (8b), Flow Label (20b) */
	field = &params->enc_field[BNXT_ULP_ENC_FIELD_IPV6_VTC_FLOW];
	val32 = cpu_to_be32((tun_key->tos << 4) | 6);
	val32 |= tun_key->label;
	size = sizeof(val32);
	field = ulp_tc_parser_fld_copy(field, &val32, size);

	/* next_proto_id */
	val8 = 0;
	size = sizeof(val8);
	field = ulp_tc_parser_fld_copy(field, &val8, size);

	/* hop limit */
	size = sizeof(tun_key->ttl);
	val8 = tun_key->ttl ? tun_key->ttl : BNXT_ULP_DEFAULT_TTL;
	field = ulp_tc_parser_fld_copy(field, &val8, size);

	size = sizeof(tun_key->u.ipv6.src);
	field = ulp_tc_parser_fld_copy(field, &tun_key->u.ipv6.src, size);

	size = sizeof(tun_key->u.ipv6.dst);
	field = ulp_tc_parser_fld_copy(field, &tun_key->u.ipv6.dst, size);

	ULP_BITMAP_SET(params->enc_hdr_bitmap.bits, BNXT_ULP_HDR_BIT_O_IPV6);
}

static void ulp_encap_copy_udp(struct ulp_tc_parser_params *params,
			       struct ip_tunnel_key *tun_key)
{
	struct ulp_tc_hdr_field *field;
	u8 type = IPPROTO_UDP;
	u32 size;

	field = &params->enc_field[BNXT_ULP_ENC_FIELD_UDP_SPORT];
	size = sizeof(tun_key->tp_src);
	field = ulp_tc_parser_fld_copy(field, &tun_key->tp_src, size);

	/* update the computational field */
	if (tun_key->tp_src)
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_TUNNEL_SPORT, 1);

	size = sizeof(tun_key->tp_dst);
	field = ulp_tc_parser_fld_copy(field, &tun_key->tp_dst, size);

	ULP_BITMAP_SET(params->enc_hdr_bitmap.bits, BNXT_ULP_HDR_BIT_O_UDP);

	/* Update the ip header protocol */
	field = &params->enc_field[BNXT_ULP_ENC_FIELD_IPV4_PROTO];
	ulp_tc_parser_fld_copy(field, &type, sizeof(type));
	field = &params->enc_field[BNXT_ULP_ENC_FIELD_IPV6_PROTO];
	ulp_tc_parser_fld_copy(field, &type, sizeof(type));
}

static void ulp_encap_copy_vxlan(struct ulp_tc_parser_params *params,
				 struct ip_tunnel_key *tun_key)
{
	struct ulp_tc_hdr_bitmap *act = &params->act_bitmap;
	struct ulp_tc_act_prop *ap = &params->act_prop;
	struct ulp_parser_vxlan ulp_vxlan = { 0 };
	struct ulp_tc_hdr_field *field;
	u32 vxlan_size;
	u32 size;
	u32 vni;

	vni = tunnel_id_to_key32(tun_key->tun_id);
	vni = be32_to_cpu(vni);

	netdev_dbg(params->ulp_ctx->bp->dev, "%s: vni: 0x%x\n", __func__, vni);

	ulp_vxlan.vni[0] = (vni >> 16) & 0xff;
	ulp_vxlan.vni[1] = (vni >> 8) & 0xff;
	ulp_vxlan.vni[2] = vni & 0xff;
	ulp_vxlan.flags = 0x08;

	vxlan_size = sizeof(ulp_vxlan);
	vxlan_size = cpu_to_be32(vxlan_size);
	memcpy(&ap->act_details[BNXT_ULP_ACT_PROP_IDX_ENCAP_TUN_SZ],
	       &vxlan_size, sizeof(u32));

	/* update the hdr_bitmap with vxlan */
	ULP_BITMAP_SET(act->bits, BNXT_ULP_ACT_BIT_VXLAN_ENCAP);

	field = &params->enc_field[BNXT_ULP_ENC_FIELD_VXLAN_FLAGS];
	size = sizeof(ulp_vxlan.flags);
	field = ulp_tc_parser_fld_copy(field, &ulp_vxlan.flags, size);

	size = sizeof(ulp_vxlan.rsvd0);
	field = ulp_tc_parser_fld_copy(field, &ulp_vxlan.rsvd0, size);

	size = sizeof(ulp_vxlan.vni);
	field = ulp_tc_parser_fld_copy(field, &ulp_vxlan.vni, size);

	size = sizeof(ulp_vxlan.rsvd1);
	field = ulp_tc_parser_fld_copy(field, &ulp_vxlan.rsvd1, size);

	ULP_BITMAP_SET(params->enc_hdr_bitmap.bits, BNXT_ULP_HDR_BIT_T_VXLAN);
}

/* Save encap action details in parser params, so it can be returned to
 * the caller of bnxt_ulp_flow_create() for neighbor update processing.
 * This memory will be owned and released by the caller.
 */
static int ulp_tc_save_encap_info(struct ulp_tc_parser_params *params,
				  struct ip_tunnel_key *tun_key,
				  struct bnxt_tc_neigh_key *neigh_key,
				  struct bnxt_tc_l2_key *l2_info)
{
	params->tnl_key = vzalloc(sizeof(*tun_key));
	if (!params->tnl_key)
		return -ENOMEM;

	params->neigh_key = vzalloc(sizeof(*neigh_key));
	if (!params->neigh_key) {
		vfree(params->tnl_key);
		return -ENOMEM;
	}

	*((struct ip_tunnel_key *)params->tnl_key) = *tun_key;
	*((struct bnxt_tc_neigh_key *)params->neigh_key) = *neigh_key;

	ether_addr_copy(params->tnl_dmac, l2_info->dmac);
	ether_addr_copy(params->tnl_smac, l2_info->smac);
	params->tnl_ether_type = l2_info->ether_type;

	return 0;
}

static int ulp_tc_tunnel_encap_ipv4(struct bnxt *bp,
				    struct ulp_tc_parser_params *params,
				    struct ip_tunnel_key *tun_key)
{
	struct bnxt_tc_neigh_key neigh_key = { 0 };
	struct bnxt_tc_l2_key l2_info = { 0 };
	int rc;

	rc = bnxt_tc_resolve_ipv4_tunnel_hdrs(bp, NULL, tun_key, &l2_info,
					      &neigh_key);
	if (rc != 0)
		return BNXT_TF_RC_ERROR;

	ulp_encap_copy_eth(params, &l2_info, cpu_to_be16(ETH_P_IP));
	ulp_encap_copy_ipv4(params, tun_key);
	ulp_encap_copy_udp(params, tun_key);
	ulp_encap_copy_vxlan(params, tun_key);

	l2_info.ether_type = ETH_P_IP;
	ulp_tc_save_encap_info(params, tun_key, &neigh_key, &l2_info);
	return BNXT_TF_RC_SUCCESS;
}

static int ulp_tc_tunnel_encap_ipv6(struct bnxt *bp,
				    struct ulp_tc_parser_params *params,
				    struct ip_tunnel_key *tun_key)
{
	struct bnxt_tc_neigh_key neigh_key = { 0 };
	struct bnxt_tc_l2_key l2_info = { 0 };
	int rc;

	rc = bnxt_tc_resolve_ipv6_tunnel_hdrs(bp, NULL, tun_key, &l2_info,
					      &neigh_key);
	if (rc)
		return BNXT_TF_RC_ERROR;

	ulp_encap_copy_eth(params, &l2_info, cpu_to_be16(ETH_P_IPV6));
	ulp_encap_copy_ipv6(params, tun_key);
	ulp_encap_copy_udp(params, tun_key);
	ulp_encap_copy_vxlan(params, tun_key);

	l2_info.ether_type = ETH_P_IPV6;
	ulp_tc_save_encap_info(params, tun_key, &neigh_key, &l2_info);

	return BNXT_TF_RC_SUCCESS;
}

static struct ip_tunnel_info *ulp_tc_get_tun_info(void *action_arg)

{
#ifdef HAVE_FLOW_OFFLOAD_H
	struct flow_action_entry *action = action_arg;

	return (struct ip_tunnel_info *)action->tunnel;
#else
	struct tc_action *action = action_arg;

	return tcf_tunnel_info(action);
#endif
}

int ulp_tc_tunnel_encap_act_handler(struct bnxt *bp,
				    struct ulp_tc_parser_params *params,
				    void *action_arg)
{
	struct ip_tunnel_info *tun_info = ulp_tc_get_tun_info(action_arg);
	struct ip_tunnel_key encap_key = tun_info->key;
	int rc = BNXT_TF_RC_ERROR;

	switch (ip_tunnel_info_af(tun_info)) {
	case AF_INET:
		rc = ulp_tc_tunnel_encap_ipv4(bp, params, &encap_key);
		break;
	case AF_INET6:
		rc = ulp_tc_tunnel_encap_ipv6(bp, params, &encap_key);
		break;
	default:
		break;
	}

	return rc;
}

int ulp_tc_tunnel_decap_act_handler(struct bnxt *bp,
				    struct ulp_tc_parser_params *params,
				    void *action_arg)
{
	/* Update the hdr_bitmap with vxlan */
	ULP_BITMAP_SET(params->act_bitmap.bits, BNXT_ULP_ACT_BIT_VXLAN_DECAP);

	/* Update computational fields with tunnel decap info */
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_L3_TUN_DECAP, 1);
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_L3_TUN, 1);
	ULP_BITMAP_SET(params->cf_bitmap, BNXT_ULP_CF_BIT_IS_TUNNEL);

	return BNXT_TF_RC_SUCCESS;
}

static void ulp_tc_get_vlan_info(void *action_arg, __be16 *proto, u16 *vid,
				 u8 *prio)
{
#ifdef HAVE_FLOW_OFFLOAD_H
	struct flow_action_entry *action = action_arg;

	*proto = action->vlan.proto;
	*vid = action->vlan.vid;
	*prio = action->vlan.prio;
#else
	struct tc_action *action = action_arg;

	*proto = tcf_vlan_push_proto(action);
	*vid = tcf_vlan_push_vid(action);
	*prio = tcf_vlan_push_prio(action);
#endif
}

int ulp_tc_vlan_push_act_handler(struct bnxt *bp,
				 struct ulp_tc_parser_params *params,
				 void *action_arg)
{
	struct ulp_tc_act_prop *act = &params->act_prop;
	__be16 proto;
	u16 vid;
	u8 prio;

	ulp_tc_get_vlan_info(action_arg, &proto, &vid, &prio);
	netdev_dbg(bp->dev, "%s: tpid: 0x%x vid: 0x%x pcp: 0x%x\n", __func__,
		   proto, vid, prio);

	/* set tpid */
	memcpy(&act->act_details[BNXT_ULP_ACT_PROP_IDX_PUSH_VLAN],
	       &proto, BNXT_ULP_ACT_PROP_SZ_PUSH_VLAN);
	ULP_BITMAP_SET(params->act_bitmap.bits, BNXT_ULP_ACT_BIT_PUSH_VLAN);

	/* set vid */
	vid = cpu_to_be16(vid);
	memcpy(&act->act_details[BNXT_ULP_ACT_PROP_IDX_SET_VLAN_VID],
	       &vid, BNXT_ULP_ACT_PROP_SZ_SET_VLAN_VID);
	ULP_BITMAP_SET(params->act_bitmap.bits, BNXT_ULP_ACT_BIT_SET_VLAN_VID);

	/* set pcp */
	memcpy(&act->act_details[BNXT_ULP_ACT_PROP_IDX_SET_VLAN_PCP],
	       &prio, BNXT_ULP_ACT_PROP_SZ_SET_VLAN_PCP);
	ULP_BITMAP_SET(params->act_bitmap.bits,
		       BNXT_ULP_ACT_BIT_SET_VLAN_PCP);

	return BNXT_TF_RC_SUCCESS;
}

int ulp_tc_vlan_pop_act_handler(struct bnxt *bp,
				struct ulp_tc_parser_params *params,
				void *action_arg)
{
	/* Update the act_bitmap with pop */
	ULP_BITMAP_SET(params->act_bitmap.bits, BNXT_ULP_ACT_BIT_POP_VLAN);
	return BNXT_TF_RC_SUCCESS;
}

static u32 ulp_tc_get_chain_index(void *action_arg)
{
#ifdef HAVE_FLOW_OFFLOAD_H
	struct flow_action_entry *action = action_arg;

	return action->chain_index;
#else
	struct tc_action *action = action_arg;

	return tcf_gact_goto_chain_index(action);
#endif
}

int ulp_tc_goto_act_handler(struct bnxt *bp,
			    struct ulp_tc_parser_params *params,
			    void *action_arg)
{
	u32 chain_id = ulp_tc_get_chain_index(action_arg);
	struct ulp_tc_act_prop *act_prop = &params->act_prop;

	netdev_dbg(bp->dev, "%s: goto chain: %u\n", __func__, chain_id);

	/* Set goto action in the action bitmap */
	ULP_BITMAP_SET(params->act_bitmap.bits, BNXT_ULP_ACT_BIT_GOTO_CHAIN);
	chain_id = cpu_to_be32(chain_id);
	memcpy(&act_prop->act_details[BNXT_ULP_ACT_PROP_IDX_GOTO_CHAIN],
	       &chain_id, BNXT_ULP_ACT_PROP_SZ_GOTO_CHAIN);
	return BNXT_TF_RC_SUCCESS;
}

static int bnxt_tc_set_l3_v4_action_params(struct bnxt *bp, struct ulp_tc_parser_params *params,
					   u32 offset, u32 val, u32 mask)
{
	int rc = 0;

	if (offset ==  offsetof(struct iphdr, saddr)) {
		memcpy(&params->act_prop.act_details[BNXT_ULP_ACT_PROP_IDX_SET_IPV4_SRC],
		       &val, BNXT_ULP_ACT_PROP_SZ_SET_IPV4_SRC);
		/* Update the hdr_bitmap with set ipv4 src */
		ULP_BITMAP_SET(params->act_bitmap.bits, BNXT_ULP_ACT_BIT_SET_IPV4_SRC);
	} else if (offset ==  offsetof(struct iphdr, daddr)) {
		memcpy(&params->act_prop.act_details[BNXT_ULP_ACT_PROP_IDX_SET_IPV4_DST],
		       &val, BNXT_ULP_ACT_PROP_SZ_SET_IPV4_DST);
		/* Update the hdr_bitmap with set ipv4 dst */
		ULP_BITMAP_SET(params->act_bitmap.bits, BNXT_ULP_ACT_BIT_SET_IPV4_DST);
	} else if (!offset) {
		rc = bnxt_tc_set_dscp(bp, params, false, val, mask);
	} else {
		netdev_dbg(bp->dev,
			   "%s: IPv4_hdr: Invalid pedit field\n",
			   __func__);
		return -EINVAL;
	}

	netdev_dbg(bp->dev, "Actions NAT src IP: %pI4 dst ip : %pI4\n",
		   &params->act_prop.act_details[BNXT_ULP_ACT_PROP_IDX_SET_IPV4_SRC],
		   &params->act_prop.act_details[BNXT_ULP_ACT_PROP_IDX_SET_IPV4_DST]);

	return rc;
}

#define	BNXT_TC_FIRST_WORD_SRC_IPV6		0x8
#define	BNXT_TC_SECOND_WORD_SRC_IPV6		0xC
#define	BNXT_TC_THIRD_WORD_SRC_IPV6		0x10
#define	BNXT_TC_FOURTH_WORD_SRC_IPV6		0x14
#define	BNXT_TC_FIRST_WORD_DST_IPV6		0x18
#define	BNXT_TC_SECOND_WORD_DST_IPV6		0x1C
#define	BNXT_TC_THIRD_WORD_DST_IPV6		0x20
#define	BNXT_TC_FOURTH_WORD_DST_IPV6		0x24
#define	BNXT_TC_IPV6_SIZE_IN_EACH_ITERATION	4
#define	BNXT_TC_WORD_DSCP_IPV6			0x0
#define	BNXT_TC_MASK_DSCP_IPV6			0xFC00000
#define	BNXT_TC_SHIFT_DSCP_IPV6			22
#define	BNXT_TC_MASK_DSCP_IPV4			0xFC0000
#define	BNXT_TC_SHIFT_DSCP_V4_TO_V6		4

static u16 bnxt_tc_dscp_resv_rows[] = {
	26,		/* BNXT_TC_DSCP_REMAP_ROW_ROCE_DEFAULT */
	48,		/* BNXT_TC_DSCP_REMAP_ROW_CNP */
	59		/* BNXT_TC_DSCP_REMAP_ROW_ROCE_CUSTOM */
};


static void bnxt_tc_param_set_act_meter(struct ulp_tc_parser_params *params,
					struct bnxt_ulp_dscp_remap *dscp_remap,
					u32 meter_id)
{
	u32 tmp_meter_id;

	tmp_meter_id = cpu_to_be32(meter_id);
	memcpy(&params->act_prop.act_details[BNXT_ULP_ACT_PROP_IDX_METER],
	       &tmp_meter_id, BNXT_ULP_ACT_PROP_SZ_METER);
	ULP_BITMAP_SET(params->act_bitmap.bits, BNXT_ULP_ACT_BIT_METER);

	/* When sriov_dscp_insert is enabled and if the meter is RED,
	 * i.e, default meter for flows without modify-dscp action,
	 * we must not set the DSCP_REMAP CF. This avoids creating
	 * separate WC tcam entries (L2 & RoCE) while setting up the
	 * CFA pipeline.
	 */
	if (BNXT_ULP_DSCP_INSERT_CAP(dscp_remap) && meter_id ==
	    MTR_PROF_CLR_RED + 1)
		return;
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_DSCP_REMAP, 1);
}

static int bnxt_tc_dscp_global_cfg_update(struct bnxt *bp, enum tf_dir dir,
					  u32 *global_cfg, int row, int color,
					  u32 value, u32 set_flag, bool hw_wr)
{
	struct tf_global_cfg_parms parms = { 0 };
	u32 dscp_val = 0;
	u32 dscp_rmp_val;
	u32 size;
	int rc;

	if (set_flag)
		dscp_val = value >> 20;

	/* Setup the specified row to be written; it consists of
	 * 3 fields, each 8-bits. The upper 6-bits of each field
	 * contains the DSCP value for the specific color.
	 *
	 * 31:24 - Unused
	 * 24:16 - Red DSCP
	 * 15:8 - Yellow DSCP
	 * 7:0 - Green DSCP
	 */
	switch (color) {
	case MTR_PROF_CLR_GREEN:
		dscp_rmp_val = set_flag ? dscp_val : 0xffffff00;
		break;
	case MTR_PROF_CLR_YELLOW:
		dscp_rmp_val = set_flag ? (dscp_val << 8) : 0xffff00ff;
		break;
	case MTR_PROF_CLR_RED:
		dscp_rmp_val = set_flag ? (dscp_val << 16) : 0xff00ffff;
		break;
	default:
		return -EINVAL;
	}

	if (set_flag)
		global_cfg[row] |= dscp_rmp_val;
	else
		global_cfg[row] &= dscp_rmp_val;

	netdev_dbg(bp->dev, "%s: Setting dscp: 0x%x dscp_rmp: 0x%x\n",
		   __func__, dscp_val, dscp_rmp_val);

	if (!hw_wr)
		return 0;

	netdev_dbg(bp->dev, "%s: Writing to the global cfg table\n",
		   __func__);

	size = sizeof(u32) * BNXT_DSCP_REMAP_ROWS;
	parms.dir = dir,
	parms.type = TF_DSCP_RMP_CFG,
	parms.offset = 0,
	parms.config = (u8 *)global_cfg,
	parms.config_sz_in_bytes = size;

	rc = tf_set_global_cfg(bp->tfp, &parms);
	if (rc)
		netdev_dbg(bp->dev, "%s: Failed to set global cfg rc:%d\n",
			   __func__, rc);

	return rc;
}

static bool bnxt_tc_is_row_reserved(struct bnxt_ulp_dscp_remap *dscp_remap,
				    u16 row)
{
	int i;

	if (!BNXT_ULP_DSCP_INSERT_CAP(dscp_remap))
		return false;

	for (i = 0; i < ARRAY_SIZE(bnxt_tc_dscp_resv_rows); i++) {
		if (row == bnxt_tc_dscp_resv_rows[i])
			return true;
	}
	return false;
}

/* If sriov_dscp_insert is enabled, remap rows for CNP and ROCE
 * are reserved and they are not available for allocation.
 * RED slot in all other rows is reserved to reset (zero) dscp
 * for flows without modify-dscp action. This routine initializes
 * the VF remap software table (cache) using these conditions.
 */
static void bnxt_tc_init_dscp_remap_tbl(struct bnxt *bp,
					struct bnxt_ulp_dscp_remap *dscp_remap)
{
	struct bnxt_dscp_remap_vf *dscp_remap_vf;
	enum bnxt_ulp_meter_color color;
	u32 vf_id;
	u32 val;

	for (vf_id = 0; vf_id < BNXT_DSCP_REMAP_ROWS; vf_id++) {
		dscp_remap_vf = dscp_remap->dscp_remap_vf[vf_id];

		for (color = MTR_PROF_CLR_GREEN; color <= MTR_PROF_CLR_RED;
		     color++) {
			if (BNXT_ULP_DSCP_INSERT_CAP(dscp_remap)) {
				if (bnxt_tc_is_row_reserved(dscp_remap, vf_id)) {
					val = vf_id << BNXT_TC_SHIFT_DSCP_IPV6;
					dscp_remap_vf[color].dscp_remap_val =
						val;
				} else if (color == MTR_PROF_CLR_RED) {
					dscp_remap_vf[color].dscp_remap_val =
						0;
				} else {
					dscp_remap_vf[color].dscp_remap_val =
						BNXT_ULP_DSCP_INVALID;
				}
			} else {
				dscp_remap_vf[color].dscp_remap_val =
					BNXT_ULP_DSCP_INVALID;
			}
		}
	}
}

static void bnxt_tc_delete_meter_profiles(struct bnxt *bp, u32 dir,
					  struct bnxt_ulp_dscp_remap
					  *dscp_remap)
{
	enum bnxt_ulp_meter_color color;
	u32 meter_prof_id;

	for (color = MTR_PROF_CLR_GREEN; color <= MTR_PROF_CLR_RED; color++) {
		meter_prof_id = dscp_remap->meter_prof_id[color];
		if (!meter_prof_id)
			continue;
		bnxt_flow_meter_profile_delete(bp, meter_prof_id, dir);
		dscp_remap->meter_prof_id[color] = 0;
	}
}

static int bnxt_tc_setup_meter_profiles(struct bnxt *bp, u32 dir,
					struct bnxt_ulp_dscp_remap *dscp_remap)
{
	enum bnxt_ulp_meter_color color;
	u32 meter_profile_id;
	int rc;

	for (color = MTR_PROF_CLR_GREEN; color <= MTR_PROF_CLR_RED; color++) {
		meter_profile_id = color + 1;	/* non-zero id */
		rc = bnxt_flow_meter_profile_add(bp, meter_profile_id,
						 dir, color);
		if (rc) {
			netdev_dbg(bp->dev,
				   "%s: Failed to create meter profile id: 0x%x\n",
				   __func__, meter_profile_id);
			goto error;
		}
		dscp_remap->meter_prof_id[color] = meter_profile_id;
	}

	return rc;

error:
	bnxt_tc_delete_meter_profiles(bp, dir, dscp_remap);
	return rc;
}

static void bnxt_tc_delete_meters(struct bnxt *bp, u32 dir,
				  struct bnxt_ulp_dscp_remap *dscp_remap)
{
	enum bnxt_ulp_meter_color color;
	u32 meter_id;

	for (color = MTR_PROF_CLR_GREEN; color <= MTR_PROF_CLR_RED; color++) {
		meter_id = dscp_remap->meter_id[color];
		if (!meter_id)
			continue;
		bnxt_flow_meter_destroy(bp, meter_id, dir);
		dscp_remap->meter_id[color] = 0;
	}
}

static int bnxt_tc_setup_meters(struct bnxt *bp, u32 dir,
				struct bnxt_ulp_dscp_remap *dscp_remap)
{
	enum bnxt_ulp_meter_color color;
	int meter_profile_id;
	int meter_id;
	int rc;

	for (color = MTR_PROF_CLR_GREEN; color <= MTR_PROF_CLR_RED; color++) {
		meter_profile_id = dscp_remap->meter_prof_id[color];
		meter_id = color + 1;	/* non-zero id */
		rc = bnxt_flow_meter_create(bp, meter_profile_id,
					    meter_id, dir);
		if (rc) {
			netdev_dbg(bp->dev,
				   "%s: Failed to create meter id: 0x%x\n",
				   __func__, meter_id);
			goto error;
		}
		dscp_remap->meter_id[color] = meter_id;
	}
	return rc;

error:
	bnxt_tc_delete_meters(bp, dir, dscp_remap);
	return rc;
}

static int bnxt_tc_vf_id_get(struct bnxt *bp,
			     struct ulp_tc_parser_params *params,
			     u16 *vf_id)
{
	u16 port_id;
	u32 mtype;
	int rc;

	mtype = ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_MATCH_PORT_TYPE);
	if (mtype != BNXT_ULP_INTF_TYPE_VF_REP) {
		netdev_dbg(bp->dev, "Meter action on invalid port type: %d\n",
			   mtype);
		return BNXT_TF_RC_PARSE_ERR_NOTSUPP;
	}

	port_id = ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_INCOMING_IF);
	rc = ulp_port_db_vf_id_get(params->ulp_ctx, port_id, vf_id);
	if (rc)
		return rc;

	netdev_dbg(bp->dev, "%s: port_id: 0x%x vf_id: %d\n", __func__,
		   port_id, *vf_id);
	return 0;
}

static int bnxt_tc_flow_meter_id_get(struct bnxt *bp,
				     struct bnxt_ulp_dscp_remap *dscp_remap,
				     u16 vf_id, u32 dscp_val, u32 *meter_id,
				     enum bnxt_ulp_meter_color *clr,
				     int *row_idx, bool *update_glb_cfg)
{
	struct bnxt_dscp_remap_vf *dscp_remap_vf;
	enum bnxt_ulp_meter_color color;
	enum bnxt_ulp_meter_color end;
	int row;

	if (bnxt_tc_is_row_reserved(dscp_remap, vf_id))
		return -EINVAL;

	row = BNXT_ULP_DSCP_INSERT_CAP(dscp_remap) ? vf_id : 0;
	end = BNXT_ULP_DSCP_INSERT_CAP(dscp_remap) ? MTR_PROF_CLR_YELLOW :
			MTR_PROF_CLR_RED;

	dscp_remap_vf = dscp_remap->dscp_remap_vf[row];
	for (color = MTR_PROF_CLR_GREEN; color <= end; color++) {
		if (dscp_remap_vf[color].dscp_remap_val == dscp_val) {
			*update_glb_cfg = false;
			goto done;
		}
	}

	for (color = MTR_PROF_CLR_GREEN; color <= end; color++) {
		if (dscp_remap_vf[color].dscp_remap_val ==
		    BNXT_ULP_DSCP_INVALID)
			break;
	}
	if (color > end)
		return -ENOSPC;

	dscp_remap_vf[color].dscp_remap_val = dscp_val;
	*update_glb_cfg = true;

done:
	dscp_remap_vf[color].dscp_remap_ref++;
	*meter_id = dscp_remap->meter_id[color];
	*clr = color;
	*row_idx = row;
	netdev_dbg(bp->dev, "%s: vf_id: %d row: %d color: %d meter: %d\n",
		   __func__, vf_id, row, color, *meter_id);
	return 0;
}

int bnxt_tc_clear_dscp(struct bnxt *bp, struct bnxt_ulp_context *ulp_ctx,
		       u16 vf_id, u32 dscp_remap_val)
{
	struct bnxt_ulp_dscp_remap *dscp_remap = &ulp_ctx->cfg_data->dscp_remap;
	struct bnxt_dscp_remap_vf *dscp_remap_vf;
	enum bnxt_ulp_meter_color color;
	u32 *global_cfg;
	int row;
	int rc;

	if (!dscp_remap->dscp_remap_initialized)
		return -EINVAL;

	row = BNXT_ULP_DSCP_INSERT_CAP(dscp_remap) ? vf_id : 0;

	dscp_remap_vf = dscp_remap->dscp_remap_vf[row];
	for (color = MTR_PROF_CLR_GREEN; color <= MTR_PROF_CLR_RED; color++) {
		if (dscp_remap_vf[color].dscp_remap_val == dscp_remap_val) {
			dscp_remap_vf[color].dscp_remap_ref--;
			break;
		}
	}

	/* Failed to find the entry */
	if (color > MTR_PROF_CLR_RED)
		return -EINVAL;

	netdev_dbg(bp->dev, "%s: DSCP: 0x%x vf_id: %d row: %d color: %d\n",
		   __func__, dscp_remap_val, vf_id, row, color);

	/* Still being referenced by other flows */
	if (dscp_remap_vf[color].dscp_remap_ref)
		return 0;

	/* No need to update global cfg for RED, since
	 * it is zero when sriov_dscp_insert is enabled.
	 */
	if (BNXT_ULP_DSCP_INSERT_CAP(dscp_remap) && color == MTR_PROF_CLR_RED)
		return 0;

	/* Mark the entry free */
	dscp_remap_vf[color].dscp_remap_val = BNXT_ULP_DSCP_INVALID;

	/* No more references; clear global cfg */
	global_cfg = dscp_remap->dscp_global_cfg;
	rc = bnxt_tc_dscp_global_cfg_update(bp, TF_DIR_TX, global_cfg,
					    row, color, dscp_remap_val,
					    0, true);
	if (rc)
		netdev_dbg(bp->dev,
			   "%s: Failed to update global cfg: %d:%d\n",
			   __func__, vf_id, color);
	netdev_dbg(bp->dev,
		   "%s: Updated global cfg: vf_id: %d row: %d color: %d\n",
		   __func__, vf_id, row, color);
	return 0;
}

static void bnxt_tc_uninit_dscp_global_cfg(struct bnxt *bp,
					   struct bnxt_ulp_dscp_remap
						*dscp_remap)
{
	u32 *global_cfg = dscp_remap->dscp_global_cfg;
	enum bnxt_ulp_meter_color color;
	u32 dscp_remap_val;
	u16 vf_id;

	if (!BNXT_ULP_DSCP_INSERT_CAP(dscp_remap))
		return;

	for (vf_id = 0; vf_id < BNXT_DSCP_REMAP_ROWS; vf_id++) {
		dscp_remap_val = 0;
		for (color = MTR_PROF_CLR_GREEN; color <= MTR_PROF_CLR_RED;
		     color++) {
			bnxt_tc_dscp_global_cfg_update(bp, TF_DIR_TX,
						       global_cfg, vf_id,
						       color, dscp_remap_val,
						       0, false);
		}
	}

	/* Update the last entry again, but with wr_hw set to true.
	 * This will flush the entries to hardware.
	 */
	bnxt_tc_dscp_global_cfg_update(bp, TF_DIR_TX, global_cfg,
				       BNXT_DSCP_REMAP_ROWS - 1,
				       MTR_PROF_CLR_RED,
				       dscp_remap_val, 0, true);
}

/* If sriov_dscp_insert is disabled:
 *	Set the three colors in all rows to the respective row number.
 *	Only row zero would be used to offload flows with dscp-remap action.
 *	Input dscp value in packets that match the offloaded flow, needs to
 *	be zero to remap to the offloaded dscp value. A packet with a non-zero
 *	input dscp value that matches the offloaded flow, would	be sent	out
 *	with the same non-zero value since all 3 colors in the row would have
 *	been set to the row number.
 * If sriov_dscp_insert is enabled:
 *	Remap rows for CNP and ROCE are reserved and they are not available
 *	for allocation. RED slot in all other rows is reserved to reset (zero)
 *	dscp for flows without modify-dscp action.
 *
 * This routine initializes the hardware dscp-remap global cfg table using
 * these conditions.
 */
static int bnxt_tc_init_dscp_global_cfg(struct bnxt *bp,
					struct bnxt_ulp_dscp_remap
						*dscp_remap)
{
	u32 *global_cfg = dscp_remap->dscp_global_cfg;
	enum bnxt_ulp_meter_color color;
	u32 dscp_remap_val;
	u16 vf_id;
	u16 i;

	if (!BNXT_ULP_DSCP_INSERT_CAP(dscp_remap)) {
		for (vf_id = 0; vf_id < BNXT_DSCP_REMAP_ROWS; vf_id++) {
			dscp_remap_val = vf_id << BNXT_TC_SHIFT_DSCP_IPV6;
			for (color = MTR_PROF_CLR_GREEN;
			     color <= MTR_PROF_CLR_RED; color++) {
				netdev_dbg(bp->dev,
					   "%s: dscp_remap_val: 0x%x\n",
					   __func__, dscp_remap_val);
				bnxt_tc_dscp_global_cfg_update(bp, TF_DIR_TX,
							       global_cfg,
							       vf_id,
							       color,
							       dscp_remap_val,
							       1, false);
			}
		}
		vf_id = BNXT_DSCP_REMAP_ROWS - 1; /* for hw_wr below */
	} else {
		for (i = 0; i < ARRAY_SIZE(bnxt_tc_dscp_resv_rows); i++) {
			vf_id = bnxt_tc_dscp_resv_rows[i];
			dscp_remap_val = vf_id << BNXT_TC_SHIFT_DSCP_IPV6;
			for (color = MTR_PROF_CLR_GREEN;
			     color <= MTR_PROF_CLR_RED; color++) {
				netdev_dbg(bp->dev,
					   "%s: dscp_remap_val: 0x%x\n",
					   __func__, dscp_remap_val);
				bnxt_tc_dscp_global_cfg_update(bp, TF_DIR_TX,
							       global_cfg,
							       vf_id,
							       color,
							       dscp_remap_val,
							       1, false);
			}
		}
	}

	/* Update the last entry again, but with hw_wr set to true.
	 * This will flush the entries to hardware.
	 */
	return bnxt_tc_dscp_global_cfg_update(bp, TF_DIR_TX, global_cfg, vf_id,
					      MTR_PROF_CLR_RED, dscp_remap_val,
					      1, true);
}

void bnxt_tc_uninit_dscp_remap(struct bnxt *bp,
			       struct bnxt_ulp_dscp_remap *dscp_remap,
			       u32 dir)
{
	if (!dscp_remap->dscp_remap_initialized)
		return;

	bnxt_tc_uninit_dscp_global_cfg(bp, dscp_remap);
	vfree(dscp_remap->dscp_global_cfg);
	dscp_remap->dscp_global_cfg = NULL;
	bnxt_tc_delete_meters(bp, dir, dscp_remap);
	bnxt_tc_delete_meter_profiles(bp, dir, dscp_remap);
	dscp_remap->dscp_remap_initialized = false;
}

/* Initialize dscp-remap framework; it includes the following:
 *	Default meter profiles of three colors: Green, Yellow, Red.
 *	Default meters for these respective colors.
 *	Allocate memory to read/write dscp global config table.
 *	Initialize the VF remap software table (cache).
 *	Write the initial values to the hardware global cfg table.
 */
int bnxt_tc_init_dscp_remap(struct bnxt *bp,
			    struct bnxt_ulp_dscp_remap *dscp_remap,
			    u32 dir)
{
	int rc = 0;

	if (dscp_remap->dscp_remap_initialized)
		return rc;

	rc = bnxt_tc_setup_meter_profiles(bp, dir, dscp_remap);
	if (rc)
		return rc;

	rc = bnxt_tc_setup_meters(bp, dir, dscp_remap);
	if (rc)
		goto error_meter;

	dscp_remap->dscp_global_cfg = vzalloc(sizeof(u32) *
					      BNXT_DSCP_REMAP_ROWS);
	if (!dscp_remap->dscp_global_cfg) {
		rc = -ENOMEM;
		goto error_glb_cfg;
	}

	bnxt_tc_init_dscp_remap_tbl(bp, dscp_remap);
	bnxt_tc_init_dscp_global_cfg(bp, dscp_remap);
	dscp_remap->dscp_remap_initialized = true;
	return rc;

error_glb_cfg:
	bnxt_tc_delete_meters(bp, dir, dscp_remap);
error_meter:
	bnxt_tc_delete_meter_profiles(bp, dir, dscp_remap);
	return rc;
}

static int bnxt_tc_set_dscp(struct bnxt *bp,
			    struct ulp_tc_parser_params *params,
			    bool ipv6, u32 val, u32 mask)
{
	struct bnxt_ulp_dscp_remap *dscp_remap =
		&params->ulp_ctx->cfg_data->dscp_remap;
	enum bnxt_ulp_meter_color color;
	bool update_glb_cfg = false;
	u32 meter_id = 0;
	u32 *global_cfg;
	u32 dir = 0;
	u16 vf_id;
	int row;
	int rc;

	/* Only DSCP (6-bit) supported; ECN (2-bit) must be masked */
	if (ipv6 && cpu_to_be32(mask) != BNXT_TC_MASK_DSCP_IPV6) {
		netdev_dbg(bp->dev, "%s: Invalid IPv6 mask: 0x%x\n",
			   __func__, cpu_to_be32(mask));
		return -EINVAL;
	} else if (!ipv6 && cpu_to_be32(mask) != BNXT_TC_MASK_DSCP_IPV4) {
		netdev_dbg(bp->dev, "%s: Invalid IPv4 mask: 0x%x\n",
			   __func__, cpu_to_be32(mask));
		return -EINVAL;
	}

	/* Only TX supported for now */
	dir = (params->dir_attr & BNXT_ULP_FLOW_ATTR_INGRESS) ?
		BNXT_ULP_FLOW_ATTR_INGRESS : BNXT_ULP_FLOW_ATTR_EGRESS;
	if (dir != BNXT_ULP_FLOW_ATTR_EGRESS) {
		netdev_dbg(bp->dev, "%s: Invalid dir: 0x%x\n", __func__, dir);
		return -EINVAL;
	}

	/* Normalize v4 val/mask to v6 format, since we can only
	 * save it in one format (v6) in the dscp_remap sw table.
	 */
	if (!ipv6) {
		val = cpu_to_be32(val);
		val <<= BNXT_TC_SHIFT_DSCP_V4_TO_V6;

		mask = cpu_to_be32(mask);
		mask <<= BNXT_TC_SHIFT_DSCP_V4_TO_V6;
	} else {
		val = cpu_to_be32(val);
		mask = cpu_to_be32(mask);
	}

	netdev_dbg(bp->dev, "%s: Set DSCP: 0x%x mask: 0x%x\n",
		   __func__, val, mask);

	rc = bnxt_tc_vf_id_get(bp, params, &vf_id);
	if (rc)
		return rc;

	rc = bnxt_tc_flow_meter_id_get(bp, dscp_remap, vf_id, val,
				       &meter_id, &color, &row,
				       &update_glb_cfg);
	if (rc) {
		netdev_dbg(bp->dev, "Failed to allocate a meter-id: %d\n", rc);
		return rc;
	}

	if (update_glb_cfg) {
		global_cfg = dscp_remap->dscp_global_cfg;
		rc = bnxt_tc_dscp_global_cfg_update(bp, TF_DIR_TX, global_cfg,
						    row, color, val, 1,
						    true);
		if (rc)
			return rc;
	}
	bnxt_tc_param_set_act_meter(params, dscp_remap, meter_id);
	params->dscp_remap_val = val;
	netdev_dbg(bp->dev, "%s: DSCP: 0x%x meter: %d\n",
		   __func__, val, meter_id);
	return rc;
}

#ifdef HAVE_FLOW_OFFLOAD_H
/* When dscp_remap is enabled and sriov_dscp_insert option is
 * enabled (input dscp insertion in VF driver), we need to
 * reset the dscp field in the packet to zero before it is
 * sent out the wire. To achieve this, assign the RED color
 * dscp which is already reserved and initialized to zero.
 * The corresponding meter is attached to the flow.
 * Note: This implicit action is needed only for flows
 * wthout the modify-dscp action; otherwise packets would
 * be sent out with the input dscp value as-is. The input
 * dscp value in the skb would have been set to the VF-id.
 */
static int ulp_tc_parer_implicit_dscp_process(struct bnxt *bp,
					      struct ulp_tc_parser_params
						*params)
{
	struct bnxt_ulp_context *ulp_ctx = bp->ulp_ctx;
	struct bnxt_dscp_remap_vf *dscp_remap_vf;
	struct bnxt_ulp_dscp_remap *dscp_remap;
	u32 meter_id;
	u16 vf_id;
	u32 dir;
	int rc;

	dscp_remap = &ulp_ctx->cfg_data->dscp_remap;
	if (!dscp_remap->dscp_remap_initialized ||
	    !BNXT_ULP_DSCP_INSERT_CAP(dscp_remap))
		return 0;

	/* METER action is already set; implies flow with modify-dscp
	 * action. No need to implicitly add modify-dscp (meter) action.
	 */
	if (ULP_BITMAP_ISSET(params->act_bitmap.bits,
			     BNXT_ULP_ACT_BIT_METER))
		return 0;

	/* modify-dscp supported only for TX flows */
	dir = (params->dir_attr & BNXT_ULP_FLOW_ATTR_INGRESS) ?
		BNXT_ULP_FLOW_ATTR_INGRESS : BNXT_ULP_FLOW_ATTR_EGRESS;
	if (dir != BNXT_ULP_FLOW_ATTR_EGRESS)
		return 0;

	rc = bnxt_tc_vf_id_get(bp, params, &vf_id);
	if (rc)
		return rc;

	dscp_remap_vf = dscp_remap->dscp_remap_vf[vf_id];
	/* RED must be initialized to zero */
	if (dscp_remap_vf[MTR_PROF_CLR_RED].dscp_remap_val != 0)
		return -EINVAL;

	dscp_remap_vf[MTR_PROF_CLR_RED].dscp_remap_ref++;
	meter_id = dscp_remap->meter_id[MTR_PROF_CLR_RED];

	bnxt_tc_param_set_act_meter(params, dscp_remap, meter_id);
	params->dscp_remap_val = 0;
	netdev_dbg(bp->dev, "%s: Implicit meter action: VF: %d meter: %d\n",
		   __func__, vf_id, meter_id);
	return 0;
}
#endif   /* HAVE_FLOW_OFFLOAD_H */

static int bnxt_tc_set_l3_v6_action_params(struct bnxt *bp, struct ulp_tc_parser_params *params,
					   u32 offset, u32 val, u32 mask)
{
	int rc = 0;

	/* The number of bytes getting copied must be BNXT_TC_IPV6_SIZE_IN_EACH_ITERATION
	 * i.e., 4 bytes only even though this is IPv6 address. Because the IPv6 address
	 * comes from the stack in 4 iterations with each iteration carrying 4 bytes.
	 */

	switch (offset) {
	case BNXT_TC_FIRST_WORD_SRC_IPV6:
		memcpy(&params->act_prop.act_details[BNXT_ULP_ACT_PROP_IDX_SET_IPV6_SRC],
		       &val, BNXT_TC_IPV6_SIZE_IN_EACH_ITERATION);
		break;
	case BNXT_TC_SECOND_WORD_SRC_IPV6:
		memcpy(&params->act_prop.act_details[BNXT_ULP_ACT_PROP_IDX_SET_IPV6_SRC + 4],
		       &val, BNXT_TC_IPV6_SIZE_IN_EACH_ITERATION);
		break;
	case BNXT_TC_THIRD_WORD_SRC_IPV6:
		memcpy(&params->act_prop.act_details[BNXT_ULP_ACT_PROP_IDX_SET_IPV6_SRC + 8],
		       &val, BNXT_TC_IPV6_SIZE_IN_EACH_ITERATION);
		break;
	case BNXT_TC_FOURTH_WORD_SRC_IPV6:
		memcpy(&params->act_prop.act_details[BNXT_ULP_ACT_PROP_IDX_SET_IPV6_SRC + 12],
		       &val, BNXT_TC_IPV6_SIZE_IN_EACH_ITERATION);
		ULP_BITMAP_SET(params->act_bitmap.bits, BNXT_ULP_ACT_BIT_SET_IPV6_SRC);
		netdev_dbg(bp->dev, "Actions NAT src IPv6 addr: %pI6\n",
			   &params->act_prop.act_details[BNXT_ULP_ACT_PROP_IDX_SET_IPV6_SRC]);
		break;
	case BNXT_TC_FIRST_WORD_DST_IPV6:
		memcpy(&params->act_prop.act_details[BNXT_ULP_ACT_PROP_IDX_SET_IPV6_DST],
		       &val, BNXT_TC_IPV6_SIZE_IN_EACH_ITERATION);
		break;
	case BNXT_TC_SECOND_WORD_DST_IPV6:
		memcpy(&params->act_prop.act_details[BNXT_ULP_ACT_PROP_IDX_SET_IPV6_DST + 4],
		       &val, BNXT_TC_IPV6_SIZE_IN_EACH_ITERATION);
		break;
	case BNXT_TC_THIRD_WORD_DST_IPV6:
		memcpy(&params->act_prop.act_details[BNXT_ULP_ACT_PROP_IDX_SET_IPV6_DST + 8],
		       &val, BNXT_TC_IPV6_SIZE_IN_EACH_ITERATION);
		break;
	case BNXT_TC_FOURTH_WORD_DST_IPV6:
		memcpy(&params->act_prop.act_details[BNXT_ULP_ACT_PROP_IDX_SET_IPV6_DST + 12],
		       &val, BNXT_TC_IPV6_SIZE_IN_EACH_ITERATION);
		ULP_BITMAP_SET(params->act_bitmap.bits, BNXT_ULP_ACT_BIT_SET_IPV6_DST);
		netdev_dbg(bp->dev, "Actions NAT dst IPv6 addr: %pI6\n",
			   &params->act_prop.act_details[BNXT_ULP_ACT_PROP_IDX_SET_IPV6_DST]);
		break;
	case BNXT_TC_WORD_DSCP_IPV6:
		rc = bnxt_tc_set_dscp(bp, params, true, val, mask);
		break;
	default:
		return -EINVAL;
	}

	return rc;
}

#define	BNXT_TC_L4_PORT_TYPE_SRC	1
#define	BNXT_TC_L4_PORT_TYPE_DST	2
static int bnxt_tc_set_l4_action_params(struct bnxt *bp, struct ulp_tc_parser_params *params,
					u32 mask, u32 val, u8 port_type)
{
	/* val is a u32 that can carry either src port or dst port value which are u16 each.
	 * If src port extract the value correctly.
	 */
	if (~mask & 0xffff)
		val = val >> 16;

	if (port_type == BNXT_TC_L4_PORT_TYPE_SRC) {
		memcpy(&params->act_prop.act_details[BNXT_ULP_ACT_PROP_IDX_SET_TP_SRC],
		       &val, BNXT_ULP_ACT_PROP_SZ_SET_TP_SRC);
		/* Update the hdr_bitmap with set tp src */
		ULP_BITMAP_SET(params->act_bitmap.bits, BNXT_ULP_ACT_BIT_SET_TP_SRC);
		netdev_dbg(bp->dev, "Actions NAT sport = %d\n", htons(val));
	} else if (port_type == BNXT_TC_L4_PORT_TYPE_DST) {
		memcpy(&params->act_prop.act_details[BNXT_ULP_ACT_PROP_IDX_SET_TP_DST],
		       &val, BNXT_ULP_ACT_PROP_SZ_SET_TP_DST);
		/* Update the hdr_bitmap with set tp dst */
		ULP_BITMAP_SET(params->act_bitmap.bits, BNXT_ULP_ACT_BIT_SET_TP_DST);
		netdev_dbg(bp->dev, "Actions NAT dport = %d\n", htons(val));
	} else {
		return -EINVAL;
	}

	return 0;
}

/* The stack provides the smac/dmac action values to be set, using key and
 * mask in multiple iterations of 4-bytes(u32). This routine consolidates
 * such multiple values into 6-byte smac and dmac values.
 *
 * For example:
 *			Mask/Key	        Offset	Iteration
 *			==========		======	=========
 *	src mac		0xffff0000/0x02010000	4	1
 *	src mac		0xffffffff/0x06050403	8	2
 *	dst mac		0xffffffff/0x0a090807	0	3
 *	dst mac		0x0000ffff/0x00000c0b	4	4
 *
 * The above combination coming from the stack will be consolidated as
 *			==============
 *	src mac:	0x010203040506
 *	dst mac:	0x0708090a0b0c
 */
static int bnxt_tc_set_l2_action_params(struct bnxt *bp, struct ulp_tc_parser_params *params,
					u32 mask, u32 val, u32 offset)
{
	u32 act_offset, size;
	u8 *act_ptr;

	netdev_dbg(bp->dev, "%s: mask: 0x%x val: 0x%x offset: %d\n",
		   __func__, mask, val, offset);

	switch (offset) {
	case 0:				/* dmac: higher 4 bytes */
		act_offset = BNXT_ULP_ACT_PROP_IDX_SET_MAC_DST + offset;
		size = sizeof(val);
		break;

	case 4:
		if (mask == 0xffff) {	/* dmac: lower 2 bytes */
			act_offset = BNXT_ULP_ACT_PROP_IDX_SET_MAC_DST + offset;
			ULP_BITMAP_SET(params->act_bitmap.bits,
				       BNXT_ULP_ACT_BIT_SET_MAC_DST);
		} else {		/* smac: higher 2 bytes */
			act_offset = BNXT_ULP_ACT_PROP_IDX_SET_MAC_SRC;
			val >>= 16;
		}
		size = 2;
		break;

	case 8:				/* smac: lower 4 bytes */
		act_offset = BNXT_ULP_ACT_PROP_IDX_SET_MAC_SRC + 2;
		size = sizeof(val);
		ULP_BITMAP_SET(params->act_bitmap.bits,
			       BNXT_ULP_ACT_BIT_SET_MAC_SRC);
		break;

	default:
		return -EINVAL;
	}

	act_ptr = &params->act_prop.act_details[act_offset];
	memcpy(act_ptr, &val, size);

	return 0;
}

#ifdef HAVE_FLOW_OFFLOAD_H

static int bnxt_tc_parse_pedit(struct bnxt *bp, struct ulp_tc_parser_params *params,
			       void *action)
{
	struct flow_action_entry *act = action;
	u32 mask, val, offset;
	u8 htype;
	int rc;

	offset = act->mangle.offset;
	htype = act->mangle.htype;
	mask = ~act->mangle.mask;
	val = act->mangle.val;

	if (act->id == FLOW_ACTION_ADD) {
		/* Currently, only dec_ttl is supported with action add.
		 * Offset should be ttl, and value AND mask should be 0xff.
		 * Any other value is not supported.
		 */
		if ((offset != offsetof(struct iphdr, ttl)) || ((val & mask) != 0xff)) {
			netdev_dbg(bp->dev,
				   "%s: Unsupported offset %d, mask 0x%x, or value 0x%x for action add ttl.\n",
				   __func__, offset, mask, val);
			return -EINVAL;
		}

		ULP_BITMAP_SET(params->act_bitmap.bits, BNXT_ULP_ACT_BIT_DEC_TTL);
		return 0;
	}

	switch (htype) {
	case FLOW_ACT_MANGLE_HDR_TYPE_ETH:
		rc = bnxt_tc_set_l2_action_params(bp, params, mask, val,
						  offset);
		if (rc)
			return rc;
		break;
	case FLOW_ACT_MANGLE_HDR_TYPE_IP4:
		rc = bnxt_tc_set_l3_v4_action_params(bp, params, offset, val, mask);
		if (rc)
			return rc;
		break;
	case FLOW_ACT_MANGLE_HDR_TYPE_IP6:
		rc = bnxt_tc_set_l3_v6_action_params(bp, params, offset, val, mask);
		if (rc)
			return rc;
		break;
	case FLOW_ACT_MANGLE_HDR_TYPE_TCP:
	case FLOW_ACT_MANGLE_HDR_TYPE_UDP:
		/* offset == 0 means TCP/UDP SPORT/DPORT.
		 * PEDIT on rest of the TCP/UDP headers is not supported.
		 */
		if (offset)
			return -EOPNOTSUPP;
		if (mask & 0xffff) {
			rc = bnxt_tc_set_l4_action_params(bp, params, mask, val,
							  BNXT_TC_L4_PORT_TYPE_SRC);
			if (rc)
				return rc;
		} else {
			rc = bnxt_tc_set_l4_action_params(bp, params, mask, val,
							  BNXT_TC_L4_PORT_TYPE_DST);
			if (rc)
				return rc;
		}
		break;
	default:
		netdev_dbg(bp->dev, "%s: Unsupported pedit hdr type\n",
			   __func__);
		return -EOPNOTSUPP;
	}

	return 0;
}

#else	/* HAVE_FLOW_OFFLOAD_H */

static int bnxt_tc_parse_pedit(struct bnxt *bp, struct ulp_tc_parser_params *params,
			       void *action)
{
	struct tc_action *tc_act = action;
	u32 mask, val, offset;
	int nkeys, j, rc;
	u8 cmd, htype;

	nkeys = tcf_pedit_nkeys(tc_act);
	for (j = 0 ; j < nkeys; j++) {
		cmd = tcf_pedit_cmd(tc_act, j);
		/* L2 rewrite comes as TCA_PEDIT_KEY_EX_CMD_SET type from TC.
		 * Return error, if the TC pedit cmd is not of this type.
		 */
		if (cmd != TCA_PEDIT_KEY_EX_CMD_SET) {
			netdev_err(bp->dev, "%s: pedit cmd not supported\n",
				   __func__);
			return -EINVAL;
		}

		offset = tcf_pedit_offset(tc_act, j);
		htype = tcf_pedit_htype(tc_act, j);
		mask = ~tcf_pedit_mask(tc_act, j);
		val = tcf_pedit_val(tc_act, j);

		switch (htype) {
		case TCA_PEDIT_KEY_EX_HDR_TYPE_ETH:
			rc = bnxt_tc_set_l2_action_params(bp, params, mask, val, offset);
			if (rc)
				return rc;
			break;

		case TCA_PEDIT_KEY_EX_HDR_TYPE_IP4:
			rc = bnxt_tc_set_l3_v4_action_params(bp, params, offset, val, mask);
			if (rc)
				return rc;
			break;

		case TCA_PEDIT_KEY_EX_HDR_TYPE_IP6:
			rc = bnxt_tc_set_l3_v6_action_params(bp, params, offset, val, mask);
			if (rc)
				return rc;
			break;
		case TCA_PEDIT_KEY_EX_HDR_TYPE_TCP:
		case TCA_PEDIT_KEY_EX_HDR_TYPE_UDP:
			/* offset == 0 means TCP/UDP SPORT/DPORT.
			 * PEDIT on rest of the TCP/UDP headers is not supported.
			 */
			if (offset)
				return -EOPNOTSUPP;
			if (mask & 0xffff) {
				rc = bnxt_tc_set_l4_action_params(bp, params, mask, val,
								  BNXT_TC_L4_PORT_TYPE_SRC);
				if (rc)
					return rc;
			} else {
				rc = bnxt_tc_set_l4_action_params(bp, params, mask, val,
								  BNXT_TC_L4_PORT_TYPE_DST);
				if (rc)
					return rc;
			}
			break;
		default:
			netdev_dbg(bp->dev, "%s: Unsupported pedit hdr type\n",
				   __func__);
			return -EOPNOTSUPP;
		}
	}

	return 0;
}

#endif	/* HAVE_FLOW_OFFLOAD_H */

int ulp_tc_mangle_act_handler(struct bnxt *bp,
			      struct ulp_tc_parser_params *params,
			      void *act)
{
	int rc;

	rc = bnxt_tc_parse_pedit(bp, params, act);
	if (rc)
		netdev_dbg(bp->dev, "%s failed, rc: %d\n", __func__, rc);

	return rc;
}

int ulp_tc_csum_act_handler(struct bnxt *bp,
			    struct ulp_tc_parser_params *params,
			    void *act)
{
	return 0;
}

int ulp_tc_drop_act_handler(struct bnxt *bp,
			    struct ulp_tc_parser_params *params,
			    void *act)
{
	/* Set drop action in the action bitmap */
	ULP_BITMAP_SET(params->act_bitmap.bits, BNXT_ULP_ACT_BIT_DROP);

	return 0;
}

static void ulp_tc_get_mpls_pop_info(void *action_arg, __be16 *proto)
{
#ifdef HAVE_FLOW_OFFLOAD_H
	struct flow_action_entry *action = action_arg;

	*proto = action->mpls_pop.proto;
#else
#ifdef HAVE_TC_MPLS_H
	struct tc_action *action = action_arg;

	*proto = tcf_mpls_proto(action);
#else
	*proto = 0;
#endif
#endif
}

static void ulp_tc_get_mpls_push_parms(void *action_arg, __be32 *label,
				       u8 *tc, u8 *bos, u8 *ttl)
{
#ifdef HAVE_FLOW_OFFLOAD_H
	struct flow_action_entry *action = action_arg;

	*label = action->mpls_push.label;
	*tc = action->mpls_push.tc;
	*bos = action->mpls_push.bos;
	*ttl = action->mpls_push.ttl;
#else
#ifdef HAVE_TC_MPLS_H
	struct tc_action *action = action_arg;

	*label = tcf_mpls_label(action);
	*tc = tcf_mpls_tc(action);
	*bos = tcf_mpls_bos(action);
	*ttl = tcf_mpls_ttl(action);
#endif
#endif
	/* If the TC command does not set these values explicitly */
	/* then they are set as all ones */
	if (*tc == 0xFF)
		*tc = 0;
	if (*bos == 0xFF)
		*bos = 0;
	if (*ttl == 0xFF)
		*ttl = BNXT_ULP_MPLS_PUSH_DEF_TTL;
}

int ulp_tc_mpls_pop_act_handler(struct bnxt *bp,
				struct ulp_tc_parser_params *params,
				void *action)
{
	struct ulp_tc_act_prop *act = &params->act_prop;
	u16 proto = 0;
	u8 ct = 0;

	ct = params->act_prop.act_details[BNXT_ULP_ACT_PROP_IDX_POP_MPLS_DEPTH];
	if (ct) {
		netdev_dbg(bp->dev, "Error support up to %d mpls pop\n",
			   BNXT_ULP_MPLS_DECAP_MAX);
		return -EOPNOTSUPP;
	}
	ulp_tc_get_mpls_pop_info(action, &proto);
	if (ULP_BITMAP_ISSET(params->act_bitmap.bits,
			     BNXT_ULP_ACT_BIT_POP_MPLS)) {
		ct++;
		memcpy(&act->act_details[BNXT_ULP_ACT_PROP_IDX_POP_MPLS_DEPTH],
		       &ct, BNXT_ULP_ACT_PROP_SZ_POP_MPLS_DEPTH);
	} else {
		ULP_BITMAP_SET(params->act_bitmap.bits,
			       BNXT_ULP_ACT_BIT_POP_MPLS);
	}
	return 0;
}

int ulp_tc_mpls_push_act_handler(struct bnxt *bp,
				 struct ulp_tc_parser_params *params,
				 void *action)
{
	struct ulp_tc_act_prop *act = &params->act_prop;
	u8 tc = 0, bos = 0, ttl = 0, cnt = 0;
	u32 label = 0, offset = 0;
	u8 *buf_ptr;

	ulp_tc_get_mpls_push_parms(action, &label, &tc, &bos, &ttl);
	label = cpu_to_be32(label);

	cnt = ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_MPLS_PUSH_CNT);
	if (cnt == 1) {
		offset = BNXT_ULP_ACT_PROP_IDX_OF_PUSH_MPLS1_LABEL;
		ULP_BITMAP_SET(params->cf_bitmap, BNXT_ULP_CF_BIT_MPLS_PUSH_1);
	} else if (!cnt) {
		offset = BNXT_ULP_ACT_PROP_IDX_OF_PUSH_MPLS0_LABEL;
	} else {
		netdev_dbg(bp->dev, "Error more than 2 mpls push not supported\n");
		return -EOPNOTSUPP;
	}

	buf_ptr = &act->act_details[offset];
	memcpy(buf_ptr, &label, BNXT_ULP_ACT_PROP_SZ_OF_PUSH_MPLS0_LABEL);
	buf_ptr += BNXT_ULP_ACT_PROP_SZ_OF_PUSH_MPLS0_LABEL;
	memcpy(buf_ptr, &tc, BNXT_ULP_ACT_PROP_SZ_OF_PUSH_MPLS0_TC);
	buf_ptr += BNXT_ULP_ACT_PROP_SZ_OF_PUSH_MPLS0_TC;
	memcpy(buf_ptr, &bos, BNXT_ULP_ACT_PROP_SZ_OF_PUSH_MPLS0_BOS);
	buf_ptr += BNXT_ULP_ACT_PROP_SZ_OF_PUSH_MPLS0_BOS;
	memcpy(buf_ptr, &ttl, BNXT_ULP_ACT_PROP_SZ_OF_PUSH_MPLS0_TTL);
	buf_ptr += BNXT_ULP_ACT_PROP_SZ_OF_PUSH_MPLS0_TTL;

	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_MPLS_PUSH_CNT, ++cnt);
	ULP_BITMAP_SET(params->act_bitmap.bits, BNXT_ULP_ACT_BIT_PUSH_MPLS);
	return 0;
}
#endif /* CONFIG_BNXT_FLOWER_OFFLOAD */
#endif /* CONFIG_BNXT_FLOWER_OFFLOAD || CONFIG_BNXT_CUSTOM_FLOWER_OFFLOAD */
