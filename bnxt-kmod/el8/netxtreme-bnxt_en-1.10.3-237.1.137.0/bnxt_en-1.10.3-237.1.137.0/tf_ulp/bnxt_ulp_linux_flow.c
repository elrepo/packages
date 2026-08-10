// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#include "bnxt_compat.h"
#include "bnxt_hsi.h"
#include "bnxt.h"
#include "bnxt_tfc.h"
#include "bnxt_tf_common.h"
#include "bnxt_ulp_flow.h"
#include "ulp_tc_parser.h"
#include "ulp_matcher.h"
#include "ulp_flow_db.h"
#include "ulp_mapper.h"
#include "ulp_fc_mgr.h"
#include "ulp_sc_mgr.h"
#include "ulp_port_db.h"
#include "ulp_template_debug_proto.h"

#ifdef CONFIG_BNXT_FLOWER_OFFLOAD
static inline void bnxt_ulp_set_dir_attributes(struct bnxt *bp,
					       struct ulp_tc_parser_params *params,
					       u16 src_fid)
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

static int
bnxt_ulp_set_prio_attribute(struct bnxt *bp,
			    struct ulp_tc_parser_params *params,
			    u32 priority)
{
	u32 max_p = bnxt_ulp_max_flow_priority_get(params->ulp_ctx);
	u32 min_p = bnxt_ulp_min_flow_priority_get(params->ulp_ctx);

	if (max_p < min_p) {
		if (priority > min_p || priority < max_p) {
			netdev_dbg(bp->dev, "invalid prio %d, not in range %u:%u\n",
				   priority, max_p, min_p);
			return -EINVAL;
		}
		params->priority = priority;
	} else {
		if (priority > max_p || priority < min_p) {
			netdev_dbg(bp->dev, "invalid prio %d, not in range %u:%u\n",
				   priority, min_p, max_p);
			return -EINVAL;
		}
		params->priority = max_p - priority;
	}
	/* flows with priority zero is considered as highest and put in EM */
	if (priority >=
	    bnxt_ulp_default_app_priority_get(params->ulp_ctx) &&
	    priority <= bnxt_ulp_max_def_priority_get(params->ulp_ctx)) {
		ULP_BITMAP_SET(params->cf_bitmap, BNXT_ULP_CF_BIT_DEF_PRIO);
		/* priority 2 (ipv4) and 3 (ipv6) will be passed by OVS-TC.
		 * Consider them highest priority for EM and set to max
		 * priority.
		 */
		params->priority = max_p;
	}
	return 0;
}

static inline void
bnxt_ulp_init_parser_cf_defaults(struct ulp_tc_parser_params *params,
				 u16 port_id)
{
	/* Set up defaults for Comp field */
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_INCOMING_IF, port_id);
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_DEV_PORT_ID, port_id);
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_SVIF_FLAG,
			    BNXT_ULP_INVALID_SVIF_VAL);
}

static void
bnxt_ulp_init_cf_header_bitmap(struct bnxt_ulp_mapper_parms *params)
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

void bnxt_ulp_init_mapper_params(struct bnxt_ulp_mapper_parms *mparms,
				 struct ulp_tc_parser_params *params,
				 enum bnxt_ulp_fdb_type flow_type)
{
	u32 ulp_flags = 0;

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
	bnxt_ulp_init_cf_header_bitmap(mparms);

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

	/* Update the socket direct svif when socket_direct feature enabled. */
	if (ULP_BITMAP_ISSET(bnxt_ulp_feature_bits_get(params->ulp_ctx),
			     BNXT_ULP_FEATURE_BIT_SOCKET_DIRECT)) {
		enum bnxt_ulp_intf_type intf_type;
		uint16_t svif;

		/* For ingress flow on trusted_vf port or PF */
		intf_type = bnxt_get_interface_type(params->ulp_ctx->bp);

		if (intf_type == BNXT_ULP_INTF_TYPE_TRUSTED_VF ||
		    intf_type == BNXT_ULP_INTF_TYPE_PF) {
			/* Get the socket direct svif of the given dev port */
			if (unlikely(ulp_port_db_port_socket_direct_svif_get(params->ulp_ctx,
									     params->port_id,
									     &svif))) {
				netdev_dbg(params->ulp_ctx->bp->dev, "Invalid port id %u\n",
					   params->port_id);
				return;
			}

			/* Set comp_fld for the socket direct svif */
			ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_SOCKET_DIRECT_SVIF, svif);
		}
	}
}

static int
bnxt_ulp_alloc_mapper_encap_mparams(struct bnxt_ulp_mapper_parms **mparms_dyn,
				    struct bnxt_ulp_mapper_parms *mparms)
{
	struct bnxt_ulp_mapper_parms *parms = NULL;

	parms = vzalloc(sizeof(*parms));
	if (!parms)
		goto err;

	memcpy(parms, mparms, sizeof(*parms));

	parms->hdr_bitmap = vzalloc(sizeof(*parms->hdr_bitmap));
	if (!parms->hdr_bitmap)
		goto err_mparm;

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
err_mparm:
	vfree(parms);
err:
	return -ENOMEM;
}

void bnxt_ulp_free_mapper_encap_mparams(void *mapper_mparms)
{
	struct bnxt_ulp_mapper_parms *parms = mapper_mparms;

	vfree(parms->act_prop);
	vfree(parms->act_bitmap);
	vfree(parms->comp_fld);
	vfree(parms->enc_field);
	vfree(parms->hdr_field);
	vfree(parms->enc_hdr_bitmap);
	vfree(parms->hdr_bitmap);
	vfree(parms);
}

/* Function to create the ulp flow. */
int bnxt_ulp_flow_create(struct bnxt *bp, u16 src_fid,
			 struct flow_cls_offload *tc_flow_cmd,
			 struct bnxt_ulp_flow_info *flow_info)
{
	struct bnxt_ulp_mapper_parms *encap_parms = NULL;
	struct bnxt_ulp_mapper_parms mparms = { 0 };
	struct ulp_tc_parser_params *params = NULL;
	struct bnxt_ulp_context *ulp_ctx;
	int rc, ret = BNXT_TF_RC_ERROR;
	u32 chain_index;
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
	bnxt_ulp_set_dir_attributes(bp, params, src_fid);

	/* Set NPAR Enabled in the computed fields */
	if (BNXT_NPAR(ulp_ctx->bp))
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_NPAR_ENABLED, 1);

	if (bnxt_ulp_set_prio_attribute(bp, params, tc_flow_cmd->common.prio))
		goto flow_error;

	bnxt_ulp_init_parser_cf_defaults(params, src_fid);

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

	/* Parse the tc flow pattern */
	ret = bnxt_ulp_tc_parser_hdr_parse(bp, tc_flow_cmd, params);
	if (ret != BNXT_TF_RC_SUCCESS)
		goto free_fid;

	/* Parse the tc flow action */
	ret = bnxt_ulp_tc_parser_act_parse(bp, tc_flow_cmd, params);
	if (ret != BNXT_TF_RC_SUCCESS)
		goto free_fid;

	params->fid = fid;
	params->func_id = func_id;
	params->port_id = src_fid;

	chain_index = tc_flow_cmd->common.chain_index;
	if (chain_index) {
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_CHAIN_ID_METADATA,
				    chain_index);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_GROUP_ID,
				    cpu_to_le32(chain_index));
		ULP_BITMAP_SET(params->cf_bitmap, BNXT_ULP_CF_BIT_GROUP_ID);

		netdev_dbg(bp->dev, "%s: Chain metadata: 0x%x chain: %u\n",
			   __func__,
			   (chain_index + ULP_THOR_SYM_CHAIN_META_VAL),
			   chain_index);
	}
	params->match_chain_id = chain_index;

	netdev_dbg(bp->dev, "Flow prio: %u chain: %u\n",
		   params->priority, params->match_chain_id);

	/* Perform the tc flow post process */
	ret = bnxt_ulp_tc_parser_post_process(params);
	if (ret == BNXT_TF_RC_ERROR)
		goto free_fid;
	else if (ret == BNXT_TF_RC_FID)
		goto return_fid;

	/* Dump the tc flow pattern */
	ulp_parser_hdr_info_dump(params);
	/* Dump the tc flow action */
	ulp_parser_act_info_dump(params);

	ret = ulp_matcher_pattern_match(params, &params->class_id);
	if (ret != BNXT_TF_RC_SUCCESS)
		goto free_fid;

	ret = ulp_matcher_action_match(params, &params->act_tmpl);
	if (ret != BNXT_TF_RC_SUCCESS)
		goto free_fid;

	bnxt_ulp_init_mapper_params(&mparms, params,
				    BNXT_ULP_FDB_TYPE_REGULAR);
	/* Call the ulp mapper to create the flow in the hardware. */
	ret = ulp_mapper_flow_create(ulp_ctx, &mparms, NULL);
	if (ret)
		goto free_fid;

	if (params->tnl_key) {
		ret = bnxt_ulp_alloc_mapper_encap_mparams(&encap_parms,
							  &mparms);
		if (ret)
			goto mapper_destroy;
	}

	if (ULP_BITMAP_ISSET(params->act_bitmap.bits, BNXT_ULP_ACT_BIT_METER)) {
		flow_info->dscp_remap = params->dscp_remap_val;
	} else {
		flow_info->dscp_remap = BNXT_ULP_DSCP_INVALID;
	}

return_fid:
	flow_info->flow_id = fid;
	if (params->tnl_key) {
		flow_info->mparms = encap_parms;
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
	ulp_mapper_flow_destroy(ulp_ctx, mparms.flow_type,
				mparms.flow_id, NULL);
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

/* Function to destroy the ulp flow. */
int bnxt_ulp_flow_destroy(struct bnxt *bp, u32 flow_id, u16 src_fid,
			  u32 dscp_remap)
{
	struct bnxt_ulp_context *ulp_ctx;
	u16 func_id;
	u16 vf_id;
	int ret;

	ulp_ctx = bnxt_ulp_bp_ptr2_cntxt_get(bp);
	if (!ulp_ctx) {
		netdev_dbg(bp->dev, "ULP context is not initialized\n");
		return -ENOENT;
	}

	if (ulp_port_db_port_func_id_get(ulp_ctx, src_fid, &func_id)) {
		netdev_dbg(bp->dev, "Conversion of port to func id failed\n");
		return -EINVAL;
	}

	ret = ulp_flow_db_validate_flow_func(ulp_ctx, flow_id, func_id);
	if (ret)
		return ret;

	mutex_lock(&ulp_ctx->cfg_data->flow_db_lock);
	ret = ulp_mapper_flow_destroy(ulp_ctx, BNXT_ULP_FDB_TYPE_REGULAR,
				      flow_id, NULL);

	if (dscp_remap == BNXT_ULP_DSCP_INVALID)
		goto done;
	ret = ulp_port_db_vf_id_get(ulp_ctx, src_fid, &vf_id);
	if (ret)
		goto done;
	bnxt_tc_clear_dscp(bp, ulp_ctx, vf_id, dscp_remap);

done:
	mutex_unlock(&ulp_ctx->cfg_data->flow_db_lock);
	return ret;
}

void bnxt_ulp_flow_query_count(struct bnxt *bp, u32 flow_id, u64 *packets,
			       u64 *bytes, unsigned long *lastused)
{
	struct bnxt_ulp_context *ulp_ctx = bp->ulp_ctx;
	enum bnxt_ulp_device_id  dev_id;
	int rc;

	rc = bnxt_ulp_cntxt_dev_id_get(ulp_ctx, &dev_id);
	if (rc)
		return;

	if (dev_id == BNXT_ULP_DEVICE_ID_THOR2)
		ulp_sc_mgr_query_count_get(ulp_ctx, flow_id,
					   packets, bytes,
					   lastused);
	else
		ulp_tf_fc_mgr_query_count_get(ulp_ctx, flow_id,
					      packets, bytes,
					      lastused, NULL);
}

int
bnxt_ulp_update_flow_encap_record(struct bnxt *bp, u8 *tnl_dmac, void *mparms,
				  u32 *flow_id)
{
	struct bnxt_ulp_mapper_parms *parms = mparms;
	struct bnxt_ulp_context *ulp_ctx = bp->ulp_ctx;
	struct ulp_tc_hdr_field *field;
	u32 local_flow_id;
	u16 func_id;
	int ret;

	if (!mparms) {
		netdev_dbg(bp->dev, "Function %s: pointer is NULL\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&ulp_ctx->cfg_data->flow_db_lock);
	ret = ulp_mapper_flow_destroy(bp->ulp_ctx, BNXT_ULP_FDB_TYPE_REGULAR, *flow_id, NULL);
	if (ret)
		goto err;

	/* Get the function id */
	if (ulp_port_db_port_func_id_get(ulp_ctx,
					 bp->pf.port_id,
					 &func_id)) {
		netdev_dbg(bp->dev, "conversion of port to func id failed\n");
		goto err;
	}

	netdev_dbg(bp->dev, "Function %s: flow destroy successful\n", __func__);
	field = &parms->enc_field[BNXT_ULP_ENC_FIELD_ETH_DMAC];
	memcpy(field->spec, tnl_dmac, ETH_ALEN);
	ret = ulp_flow_db_fid_alloc(ulp_ctx, BNXT_ULP_FDB_TYPE_REGULAR,
				    func_id, &local_flow_id);
	if (ret) {
		netdev_dbg(bp->dev, "Function %s: flow_id alloc failed\n", __func__);
		goto invalidate_flow_id;
	}
	*flow_id = local_flow_id;
	parms->flow_id = local_flow_id;
	ret = ulp_mapper_flow_create(bp->ulp_ctx, parms, NULL);
	if (!ret)
		goto done;
	netdev_dbg(bp->dev, "Function %s flow_create failed\n", __func__);
	ulp_flow_db_fid_free(ulp_ctx, BNXT_ULP_FDB_TYPE_REGULAR,
			     *flow_id);
invalidate_flow_id:
	/* flow_id == 0 means invalid flow id. Invalidate the flow_id
	 * when the flow creation under the hood fails, so that when
	 * the user deletes the flow, we will not try to delete it
	 * again in the hardware
	 */
	*flow_id = 0;
err:
done:
	mutex_unlock(&ulp_ctx->cfg_data->flow_db_lock);
	return ret;
}

bool bnxt_ulp_flow_chain_validate(struct bnxt *bp, u16 src_fid,
				  struct flow_cls_offload *tc_flow_cmd)
{
	u32 chain = tc_flow_cmd->common.chain_index;
	struct bnxt_ulp_context *ulp_ctx;
	u8 app_id;

	ulp_ctx = bnxt_ulp_bp_ptr2_cntxt_get(bp);
	if (!ulp_ctx) {
		netdev_dbg(bp->dev, "%s: ULP context is not initialized\n",
			   __func__);
		return false;
	}

	if (bnxt_ulp_cntxt_app_id_get(ulp_ctx, &app_id)) {
		netdev_dbg(bp->dev, "%s: Failed to get the app id\n", __func__);
		return false;
	}

	if (!chain)
		return true;

	/* non-zero chain */
	if (app_id != 0 && app_id != 1) {
		netdev_dbg(bp->dev,
			   "%s: Flow chaining is unsupported, app:%u chain:%u\n",
			   __func__, app_id, chain);
		return false;
	}
	return true;
}

#endif /* CONFIG_BNXT_FLOWER_OFFLOAD */
