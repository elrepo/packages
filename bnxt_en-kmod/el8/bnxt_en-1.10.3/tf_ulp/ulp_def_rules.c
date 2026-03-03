// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#include "ulp_linux.h"
#include "bnxt_compat.h"
#include "bnxt_hsi.h"
#include "bnxt.h"
#include "bnxt_tfc.h"
#include "bnxt_vfr.h"
#include "bnxt_tf_common.h"
#include "ulp_template_struct.h"
#include "ulp_template_db_enum.h"
#include "ulp_template_db_field.h"
#include "ulp_utils.h"
#include "ulp_port_db.h"
#include "ulp_flow_db.h"
#include "ulp_mapper.h"
#include "ulp_generic_flow_offload.h"

#if defined(CONFIG_BNXT_FLOWER_OFFLOAD) || defined(CONFIG_BNXT_CUSTOM_FLOWER_OFFLOAD)
struct bnxt_ulp_def_param_handler {
	int (*vfr_func)(struct bnxt_ulp_context *ulp_ctx,
			struct ulp_tlv_param *param,
			struct bnxt_ulp_mapper_parms *mapper_params);
};

static int
ulp_set_vf_roce_en_in_comp_fld(struct bnxt_ulp_context *ulp_ctx, u32 port_id,
			       struct bnxt_ulp_mapper_parms *mapper_params)
{
	u16 vf_roce;
	int rc;

	rc = ulp_port_db_vf_roce_get(ulp_ctx, port_id, &vf_roce);
	if (rc)
		return rc;

	ULP_COMP_FLD_IDX_WR(mapper_params, BNXT_ULP_CF_IDX_VF_ROCE_EN,
			    vf_roce);
	return 0;
}

static int
ulp_set_udcc_en_in_comp_fld(struct bnxt_ulp_context *ulp_ctx, u32 port_id,
			    struct bnxt_ulp_mapper_parms *mapper_params)
{
	u8 udcc = 0;
	int rc;

	rc = ulp_port_db_udcc_get(ulp_ctx, port_id, &udcc);
	if (rc)
		return rc;

	ULP_COMP_FLD_IDX_WR(mapper_params, BNXT_ULP_CF_IDX_UDCC_EN,
			    udcc);
	return 0;
}

static int
ulp_set_hw_lag_en_in_comp_fld(struct bnxt_ulp_context *ulp_ctx, u32 port_id,
			      struct bnxt_ulp_mapper_parms *mapper_params)
{
	u16 hw_lag_en = 0;

	/* Set hw lag Support*/
	ulp_port_db_hw_lag_get(ulp_ctx, port_id, &hw_lag_en);

	netdev_dbg(ulp_ctx->bp->dev, "%s: hw_lag_en = %d\n", __func__,
		   hw_lag_en);
	ULP_COMP_FLD_IDX_WR(mapper_params, BNXT_ULP_CF_IDX_HW_LAG_EN, hw_lag_en);
	return 0;
}

static int
ulp_set_svif_in_comp_fld(struct bnxt_ulp_context *ulp_ctx,
			 u32  ifindex, u8 svif_type,
			 struct bnxt_ulp_mapper_parms *mapper_params)
{
	u16 svif;
	u8 idx;
	int rc;

	rc = ulp_port_db_svif_get(ulp_ctx, ifindex, svif_type, &svif);
	if (rc)
		return rc;

	if (svif_type == BNXT_ULP_PHY_PORT_SVIF)
		idx = BNXT_ULP_CF_IDX_PHY_PORT_SVIF;
	else if (svif_type == BNXT_ULP_DRV_FUNC_SVIF)
		idx = BNXT_ULP_CF_IDX_DRV_FUNC_SVIF;
	else
		idx = BNXT_ULP_CF_IDX_VF_FUNC_SVIF;

	ULP_COMP_FLD_IDX_WR(mapper_params, idx, svif);

	return 0;
}

static int
ulp_set_spif_in_comp_fld(struct bnxt_ulp_context *ulp_ctx,
			 u32  ifindex, u8 spif_type,
			 struct bnxt_ulp_mapper_parms *mapper_params)
{
	u16 spif;
	u8 idx;
	int rc;

	rc = ulp_port_db_spif_get(ulp_ctx, ifindex, spif_type, &spif);
	if (rc)
		return rc;

	if (spif_type == BNXT_ULP_PHY_PORT_SPIF)
		idx = BNXT_ULP_CF_IDX_PHY_PORT_SPIF;
	else if (spif_type == BNXT_ULP_DRV_FUNC_SPIF)
		idx = BNXT_ULP_CF_IDX_DRV_FUNC_SPIF;
	else
		idx = BNXT_ULP_CF_IDX_VF_FUNC_SPIF;

	ULP_COMP_FLD_IDX_WR(mapper_params, idx, spif);

	return 0;
}

static int
ulp_set_parif_in_comp_fld(struct bnxt_ulp_context *ulp_ctx,
			  u32  ifindex, u8 parif_type,
			  struct bnxt_ulp_mapper_parms *mapper_params)
{
	u16 parif;
	u8 idx;
	int rc;

	rc = ulp_port_db_parif_get(ulp_ctx, ifindex, parif_type, &parif);
	if (rc)
		return rc;

	if (parif_type == BNXT_ULP_PHY_PORT_PARIF)
		idx = BNXT_ULP_CF_IDX_PHY_PORT_PARIF;
	else if (parif_type == BNXT_ULP_DRV_FUNC_PARIF)
		idx = BNXT_ULP_CF_IDX_DRV_FUNC_PARIF;
	else
		idx = BNXT_ULP_CF_IDX_VF_FUNC_PARIF;

	ULP_COMP_FLD_IDX_WR(mapper_params, idx, parif);

	return 0;
}

static int
ulp_set_vport_in_comp_fld(struct bnxt_ulp_context *ulp_ctx, u32 ifindex,
			  struct bnxt_ulp_mapper_parms *mapper_params)
{
	u16 vport;
	int rc;

	rc = ulp_port_db_vport_get(ulp_ctx, ifindex, &vport);
	if (rc)
		return rc;

	ULP_COMP_FLD_IDX_WR(mapper_params, BNXT_ULP_CF_IDX_PHY_PORT_VPORT,
			    vport);
	return 0;
}

static int
ulp_set_lag_vport_in_comp_fld(struct bnxt_ulp_context *ulp_ctx, u32 ifindex,
			      struct bnxt_ulp_mapper_parms *mapper_params)
{
	u16 vport;
	int rc;

	rc = ulp_port_db_lag_vport_get(ulp_ctx, ifindex, &vport);
	if (rc)
		return rc;

	ULP_COMP_FLD_IDX_WR(mapper_params, BNXT_ULP_CF_IDX_PHY_PORT_LAG_VPORT,
			    vport);
	return 0;
}

static int
ulp_set_vnic_in_comp_fld(struct bnxt_ulp_context *ulp_ctx,
			 u32  ifindex, u8 vnic_type,
			 struct bnxt_ulp_mapper_parms *mapper_params)
{
	u16 vnic;
	u8 idx;
	int rc;

	rc = ulp_port_db_default_vnic_get(ulp_ctx, ifindex, vnic_type, &vnic);
	if (rc)
		return rc;

	if (vnic_type == BNXT_ULP_DRV_FUNC_VNIC)
		idx = BNXT_ULP_CF_IDX_DRV_FUNC_VNIC;
	else
		idx = BNXT_ULP_CF_IDX_VF_FUNC_VNIC;

	ULP_COMP_FLD_IDX_WR(mapper_params, idx, vnic);

	return 0;
}

static int
ulp_set_vlan_in_act_prop(struct bnxt_ulp_context *ulp_ctx, u16 port_id,
			 struct bnxt_ulp_mapper_parms *mapper_params)
{
	struct ulp_tc_act_prop *act_prop = mapper_params->act_prop;

	if (ULP_BITMAP_ISSET(mapper_params->act_bitmap->bits,
			     BNXT_ULP_ACT_BIT_SET_VLAN_VID)) {
		netdev_dbg(ulp_ctx->bp->dev,
			   "VLAN already set, multiple VLANs unsupported\n");
		return BNXT_TF_RC_ERROR;
	}

	port_id = cpu_to_be16(port_id);

	ULP_BITMAP_SET(mapper_params->act_bitmap->bits,
		       BNXT_ULP_ACT_BIT_SET_VLAN_VID);

	memcpy(&act_prop->act_details[BNXT_ULP_ACT_PROP_IDX_ENCAP_VTAG],
	       &port_id, sizeof(port_id));

	return 0;
}

static int
ulp_set_mark_in_act_prop(struct bnxt_ulp_context *ulp_ctx, u16 port_id,
			 struct bnxt_ulp_mapper_parms *mapper_params)
{
	if (ULP_BITMAP_ISSET(mapper_params->act_bitmap->bits,
			     BNXT_ULP_ACT_BIT_MARK)) {
		netdev_dbg(ulp_ctx->bp->dev,
			   "MARK already set, multiple MARKs unsupported\n");
		return BNXT_TF_RC_ERROR;
	}

	ULP_COMP_FLD_IDX_WR(mapper_params, BNXT_ULP_CF_IDX_DEV_PORT_ID,
			    port_id);

	return 0;
}

static int
ulp_df_dev_port_handler(struct bnxt_ulp_context *ulp_ctx,
			struct ulp_tlv_param *param,
			struct bnxt_ulp_mapper_parms *mapper_params)
{
	u16 port_id;
	u32 ifindex;
	int rc;

	port_id = (((u16)param->value[0]) << 8) | (u16)param->value[1];

	rc = ulp_port_db_dev_port_to_ulp_index(ulp_ctx, port_id, &ifindex);
	if (rc) {
		netdev_dbg(ulp_ctx->bp->dev,
			   "Invalid port id %d\n", port_id);
		return BNXT_TF_RC_ERROR;
	}

	/* Set port SVIF */
	rc = ulp_set_svif_in_comp_fld(ulp_ctx, ifindex, BNXT_ULP_PHY_PORT_SVIF,
				      mapper_params);
	if (rc)
		return rc;

	/* Set DRV Func SVIF */
	rc = ulp_set_svif_in_comp_fld(ulp_ctx, ifindex, BNXT_ULP_DRV_FUNC_SVIF,
				      mapper_params);
	if (rc)
		return rc;

	/* Set VF Func SVIF */
	rc = ulp_set_svif_in_comp_fld(ulp_ctx, ifindex, BNXT_ULP_VF_FUNC_SVIF,
				      mapper_params);
	if (rc)
		return rc;

	/* Set port SPIF */
	rc = ulp_set_spif_in_comp_fld(ulp_ctx, ifindex, BNXT_ULP_PHY_PORT_SPIF,
				      mapper_params);
	if (rc)
		return rc;

	/* Set DRV Func SPIF */
	rc = ulp_set_spif_in_comp_fld(ulp_ctx, ifindex, BNXT_ULP_DRV_FUNC_SPIF,
				      mapper_params);
	if (rc)
		return rc;

	/* Set VF Func SPIF */
	rc = ulp_set_spif_in_comp_fld(ulp_ctx, ifindex, BNXT_ULP_DRV_FUNC_SPIF,
				      mapper_params);
	if (rc)
		return rc;

	/* Set port PARIF */
	rc = ulp_set_parif_in_comp_fld(ulp_ctx, ifindex,
				       BNXT_ULP_PHY_PORT_PARIF, mapper_params);
	if (rc)
		return rc;

	/* Set DRV Func PARIF */
	rc = ulp_set_parif_in_comp_fld(ulp_ctx, ifindex,
				       BNXT_ULP_DRV_FUNC_PARIF, mapper_params);
	if (rc)
		return rc;

	/* Set VF Func PARIF */
	rc = ulp_set_parif_in_comp_fld(ulp_ctx, ifindex, BNXT_ULP_VF_FUNC_PARIF,
				       mapper_params);
	if (rc)
		return rc;

	/* Set uplink VNIC */
	rc = ulp_set_vnic_in_comp_fld(ulp_ctx, ifindex, true, mapper_params);
	if (rc)
		return rc;

	/* Set VF VNIC */
	rc = ulp_set_vnic_in_comp_fld(ulp_ctx, ifindex, false, mapper_params);
	if (rc)
		return rc;

	/* Set VPORT */
	rc = ulp_set_vport_in_comp_fld(ulp_ctx, ifindex, mapper_params);
	if (rc)
		return rc;

	/* Set LAG VPORT */
	rc = ulp_set_lag_vport_in_comp_fld(ulp_ctx, ifindex, mapper_params);
	if (rc)
		return rc;

	/* Set VLAN */
	rc = ulp_set_vlan_in_act_prop(ulp_ctx, port_id, mapper_params);
	if (rc)
		return rc;

	/* Set MARK */
	rc = ulp_set_mark_in_act_prop(ulp_ctx, port_id, mapper_params);
	if (rc)
		return rc;

	return 0;
}

struct bnxt_ulp_def_param_handler ulp_def_handler_tbl[] = {
	[BNXT_ULP_DF_PARAM_TYPE_DEV_PORT_ID] = {
			.vfr_func = ulp_df_dev_port_handler }
};

static void
ulp_setup_default_meter_action(struct bnxt *bp,
			       struct ulp_tc_act_prop *act_prop,
			       struct ulp_tc_hdr_bitmap *act_bitmap)
{
	struct bnxt_ulp_context *ulp_ctx = bp->ulp_ctx;
	struct bnxt_ulp_dscp_remap *dscp_remap = &ulp_ctx->cfg_data->dscp_remap;
	u32 tmp_meter_id;

	if (!dscp_remap->dscp_remap_initialized ||
	    !BNXT_ULP_DSCP_INSERT_CAP(dscp_remap))
		return;

	tmp_meter_id = cpu_to_be32(dscp_remap->meter_id[MTR_PROF_CLR_RED]);
	memcpy(&act_prop->act_details[BNXT_ULP_ACT_PROP_IDX_METER],
	       &tmp_meter_id, BNXT_ULP_ACT_PROP_SZ_METER);
	ULP_BITMAP_SET(act_bitmap->bits, BNXT_ULP_ACT_BIT_METER);
}

static void
ulp_set_npar_enabled_in_comp_fld(struct bnxt_ulp_context *ulp_ctx,
				 struct bnxt_ulp_mapper_parms *mapper_params)
{
	if (!BNXT_NPAR(ulp_ctx->bp))
		return;

	ULP_COMP_FLD_IDX_WR(mapper_params, BNXT_ULP_CF_IDX_NPAR_ENABLED, 1);
}

/* Function to create default rules for the following paths
 * 1) Device PORT to App
 * 2) App to Device PORT
 * 3) VF Representor to VF
 * 4) VF to VF Representor
 *
 * @bp : Ptr to bnxt structure.
 * @param_list: Ptr to a list of parameters (Currently, only ifindex).
 * @ulp_class_tid: Class template ID number.
 * @flow_id: Ptr to flow identifier.
 *
 * Returns 0 on success or negative number on failure.
 */
int
ulp_default_flow_create(struct bnxt *bp,
			struct ulp_tlv_param *param_list,
			u32 ulp_class_tid,
			u16 port_id,
			u32 *flow_id)
{
	struct bnxt_ulp_mapper_parms mapper_params = { 0 };
	struct ulp_tc_act_prop act_prop = {{ 0 }};
	struct ulp_tc_hdr_bitmap act = { 0 };
	struct ulp_tc_hdr_field *hdr_field;
	struct bnxt_ulp_context *ulp_ctx;
	u32 type, ulp_flags = 0, fid;
	u16 static_port = 0;
	u64 *comp_fld;
	int rc = 0;

	hdr_field = vzalloc(sizeof(*hdr_field) * BNXT_ULP_PROTO_HDR_MAX);
	if (!hdr_field)
		return -ENOMEM;

	comp_fld = vzalloc(sizeof(u64) * BNXT_ULP_CF_IDX_LAST);
	if (!comp_fld) {
		rc = -ENOMEM;
		goto err1;
	}

	if (ulp_class_tid == BNXT_ULP_DF_TPL_DEFAULT_VFR)
		ulp_setup_default_meter_action(bp, &act_prop, &act);

	mapper_params.hdr_field = hdr_field;
	mapper_params.act_bitmap = &act;
	mapper_params.act_prop = &act_prop;
	mapper_params.comp_fld = comp_fld;
	mapper_params.class_tid = ulp_class_tid;
	mapper_params.flow_type = BNXT_ULP_FDB_TYPE_DEFAULT;
	mapper_params.port_id = bp->pf.fw_fid;

	ulp_ctx = bp->ulp_ctx;
	if (!ulp_ctx) {
		netdev_dbg(bp->dev,
			   "ULP context is not initialized. Failed to create dflt flow.\n");
		rc = -EINVAL;
		goto err1;
	}

	/* update the vf rep flag */
	if (bnxt_ulp_cntxt_ptr2_ulp_flags_get(ulp_ctx, &ulp_flags)) {
		netdev_dbg(bp->dev, "Error in getting ULP context flags\n");
		rc = -EINVAL;
		goto err1;
	}
	if (ULP_VF_REP_IS_ENABLED(ulp_flags))
		ULP_COMP_FLD_IDX_WR(&mapper_params,
				    BNXT_ULP_CF_IDX_VFR_MODE, 1);

	type = param_list->type;
	while (type < BNXT_ULP_DF_PARAM_TYPE_LAST) {
		if (ulp_def_handler_tbl[type].vfr_func) {
			rc = ulp_def_handler_tbl[type].vfr_func(ulp_ctx,
								param_list,
								&mapper_params);
			if (rc) {
				netdev_dbg(bp->dev,
					   "Failed to create default flow.\n");
				goto err1;
			}
		}

		param_list++;
		type = param_list->type;
	}

	/* Get the function id */
	if (ulp_port_db_port_func_id_get(ulp_ctx,
					 port_id,
					 &mapper_params.func_id)) {
		netdev_dbg(bp->dev, "conversion of port to func id failed\n");
		goto err1;
	}

	/* update the VF meta function id  */
	ULP_COMP_FLD_IDX_WR(&mapper_params, BNXT_ULP_CF_IDX_VF_META_FID,
			    BNXT_ULP_META_VF_FLAG | mapper_params.func_id);

	/* update the vxlan port */
	if (ULP_APP_STATIC_VXLAN_PORT_EN(ulp_ctx)) {
		static_port = bnxt_ulp_cntxt_vxlan_port_get(ulp_ctx);
		if (static_port) {
			ULP_COMP_FLD_IDX_WR(&mapper_params,
					    BNXT_ULP_CF_IDX_TUNNEL_PORT,
					    static_port);
			ULP_BITMAP_SET(mapper_params.cf_bitmap,
				       BNXT_ULP_CF_BIT_STATIC_VXLAN_PORT);
		} else {
			static_port = bnxt_ulp_cntxt_vxlan_ip_port_get(ulp_ctx);
			ULP_COMP_FLD_IDX_WR(&mapper_params,
					    BNXT_ULP_CF_IDX_TUNNEL_PORT,
					    static_port);
			ULP_BITMAP_SET(mapper_params.cf_bitmap,
				       BNXT_ULP_CF_BIT_STATIC_VXLAN_IP_PORT);
		}
	}

	/* update the Roce Mirror capability */
	if (BNXT_MIRROR_ON_ROCE_CAP(bp) ||
	    ulp_port_db_mirror_vnic_enabled(ulp_ctx, port_id))
		ULP_BITMAP_SET(mapper_params.cf_bitmap,
			       BNXT_ULP_CF_BIT_ROCE_MIRROR_SUPPORT);

	/* Set VF_ROCE */
	rc = ulp_set_vf_roce_en_in_comp_fld(ulp_ctx, port_id, &mapper_params);
	if (rc)
		goto err1;

	/* Set UDCC */
	rc = ulp_set_udcc_en_in_comp_fld(ulp_ctx, port_id, &mapper_params);
	if (rc)
		goto err1;

	/* Set hw_lag_en */
	rc = ulp_set_hw_lag_en_in_comp_fld(ulp_ctx, port_id, &mapper_params);
	if (rc)
		goto err1;

	netdev_dbg(bp->dev, "Creating default flow with template id: %u\n",
		   ulp_class_tid);

	/* Set NPAR Enabled in the computed fields */
	ulp_set_npar_enabled_in_comp_fld(ulp_ctx, &mapper_params);

	/* Protect flow creation */
	mutex_lock(&ulp_ctx->cfg_data->flow_db_lock);
	rc = ulp_flow_db_fid_alloc(ulp_ctx, mapper_params.flow_type,
				   mapper_params.func_id, &fid);
	if (rc) {
		netdev_dbg(bp->dev, "Unable to allocate flow table entry\n");
		goto err2;
	}

	mapper_params.flow_id = fid;
	rc = ulp_mapper_flow_create(ulp_ctx, &mapper_params, NULL);
	if (rc)
		goto err3;

	mutex_unlock(&ulp_ctx->cfg_data->flow_db_lock);
	*flow_id = fid;
	vfree(hdr_field);
	vfree(comp_fld);
	return 0;

err3:
	ulp_flow_db_fid_free(ulp_ctx, mapper_params.flow_type, fid);
err2:
	mutex_unlock(&ulp_ctx->cfg_data->flow_db_lock);
err1:
	vfree(hdr_field);
	vfree(comp_fld);
	netdev_dbg(bp->dev, "Failed to create default flow.\n");
	return rc;
}

/* Function to destroy default rules for the following paths
 * 1) Device PORT to App
 * 2) App to Device PORT
 * 3) VF Representor to VF
 * 4) VF to VF Representor
 *
 * @bp : Ptr to bnxt structure.
 * @flow_id: Flow identifier.
 *
 * Returns 0 on success or negative number on failure.
 */
int
ulp_default_flow_destroy(struct bnxt *bp, u32 flow_id)
{
	struct bnxt_ulp_context *ulp_ctx;
	int rc = 0;

	ulp_ctx = bnxt_ulp_bp_ptr2_cntxt_get(bp);
	if (!ulp_ctx) {
		netdev_dbg(bp->dev, "ULP context is not initialized\n");
		return -EINVAL;
	}

	if (!flow_id) {
		netdev_dbg(bp->dev, "invalid flow id zero\n");
		return rc;
	}

	mutex_lock(&ulp_ctx->cfg_data->flow_db_lock);
	rc = ulp_mapper_flow_destroy(ulp_ctx, BNXT_ULP_FDB_TYPE_DEFAULT,
				     flow_id, NULL);
	if (rc)
		netdev_dbg(bp->dev, "Failed to destroy flow.\n");
	mutex_unlock(&ulp_ctx->cfg_data->flow_db_lock);

	return rc;
}

void
bnxt_ulp_destroy_df_rules(struct bnxt *bp, bool global)
{
	struct bnxt_ulp_context *ulp_ctx = bp->ulp_ctx;
	struct bnxt_ulp_df_rule_info *info;
	u16 fid;

	if (!BNXT_TRUFLOW_EN(bp) ||
	    bnxt_dev_is_vf_rep(bp->dev))
		return;

	if (!ulp_ctx || !ulp_ctx->cfg_data)
		return;

	/* PF's tx_cfa_action is used to hint the adapter about
	 * which action record pointer to use when sending the
	 * packet out of the port (software path of Truflow).
	 * If this is not cleared then the adapter will try to
	 * use a stale action record pointer which will black hole
	 * the packets. tx_cfa_action is set during the creation
	 * of ULP default rules.
	 */
	bp->tx_cfa_action = 0;

	/* Delete default rules per port */
	if (!global) {
		fid = bp->pf.fw_fid;
		info = &ulp_ctx->cfg_data->df_rule_info[fid];
		if (!info->valid)
			return;

		ulp_default_flow_destroy(bp,
					 info->def_port_flow_id);
		memset(info, 0, sizeof(struct bnxt_ulp_df_rule_info));
		return;
	}

	/* Delete default rules for all ports */
	for (fid = 0; fid < TC_MAX_ETHPORTS; fid++) {
		info = &ulp_ctx->cfg_data->df_rule_info[fid];
		if (!info->valid)
			continue;

		ulp_default_flow_destroy(bp,
					 info->def_port_flow_id);
		memset(info, 0, sizeof(struct bnxt_ulp_df_rule_info));
	}
}

static int
bnxt_create_port_app_df_rule(struct bnxt *bp, u8 flow_type,
			     u32 *flow_id)
{
	u16 fid = bp->pf.fw_fid;
	struct ulp_tlv_param param_list[] = {
		{
			.type = BNXT_ULP_DF_PARAM_TYPE_DEV_PORT_ID,
			.length = 2,
			.value = {(fid >> 8) & 0xff, fid & 0xff}
		},
		{
			.type = BNXT_ULP_DF_PARAM_TYPE_LAST,
			.length = 0,
			.value = {0}
		}
	};

	if (!flow_type) {
		*flow_id = 0;
		return 0;
	}

	return ulp_default_flow_create(bp, param_list, flow_type,
				       fid, flow_id);
}

int
bnxt_ulp_create_df_rules(struct bnxt *bp)
{
	struct bnxt_ulp_context *ulp_ctx = bp->ulp_ctx;
	struct bnxt_ulp_df_rule_info *info;
	int rc = 0;
	u8 fid;

	if (!BNXT_TRUFLOW_EN(bp) || bnxt_dev_is_vf_rep(bp->dev) || !ulp_ctx)
		return 0;

	fid = bp->pf.fw_fid;
	info = &ulp_ctx->cfg_data->df_rule_info[fid];

	rc = bnxt_create_port_app_df_rule(bp,
					  BNXT_ULP_DF_TPL_DEFAULT_UPLINK_PORT,
					  &info->def_port_flow_id);
	if (rc) {
		netdev_dbg(bp->dev,
			   "Failed to create port to app default rule\n");
		return rc;
	}

	/* If the template already set the bd_action, skip this.
	 * This is handled differently between Thor and Thor2.
	 */
	if (!BNXT_CHIP_P7(bp) || !bp->tx_cfa_action) {
		rc = ulp_default_flow_db_cfa_action_get(ulp_ctx,
							info->def_port_flow_id,
							&bp->tx_cfa_action);
	}

	if (rc)
		bp->tx_cfa_action = 0;

	netdev_dbg(bp->dev, "Default flow id %d Tx cfa action is 0x%x\n",
		   info->def_port_flow_id, bp->tx_cfa_action);
	info->valid = true;
	return 0;
}

/*
 * Function to execute a specific template, this does not create flow id
 *
 * bp [in] Ptr to bnxt
 * param_list [in] Ptr to a list of parameters (Currently, only DPDK port_id).
 * ulp_class_tid [in] Class template ID number.
 *
 * Returns 0 on success or negative number on failure.
 */
int32_t
ulp_flow_template_process(struct bnxt *bp,
			  struct ulp_tlv_param *param_list,
			  u64 *comp_fld,
			  u32 ulp_class_tid,
			  u16 port_id,
			  u32 flow_id)
{
	struct bnxt_ulp_mapper_parms mapper_params = { 0 };
	struct ulp_tc_act_prop act_prop = {{ 0 }};
	struct ulp_tc_hdr_bitmap act = { 0 };
	struct ulp_tc_hdr_field *hdr_field;
	struct bnxt_ulp_context	*ulp_ctx;
	u32 type;
	int rc = 0;

	if (!comp_fld)
		return -EINVAL;

	hdr_field = vzalloc(sizeof(*hdr_field) * BNXT_ULP_PROTO_HDR_MAX);
	if (!hdr_field)
		return -ENOMEM;

	memset(&mapper_params, 0, sizeof(mapper_params));
	memset(&act_prop, 0, sizeof(act_prop));

	mapper_params.hdr_field = hdr_field;
	mapper_params.act_bitmap = &act;
	mapper_params.act_prop = &act_prop;
	mapper_params.comp_fld = comp_fld;
	mapper_params.class_tid = ulp_class_tid;
	mapper_params.port_id = port_id;

	ulp_ctx = bp->ulp_ctx;
	if (!ulp_ctx) {
		netdev_dbg(bp->dev,
			   "ULP is not init'ed. Fail to create custom flow.\n");
		rc = -EINVAL;
		goto err1;
	}

	type = param_list->type;
	while (type != BNXT_ULP_DF_PARAM_TYPE_LAST) {
		if (ulp_def_handler_tbl[type].vfr_func) {
			rc = ulp_def_handler_tbl[type].vfr_func(ulp_ctx,
								param_list,
								&mapper_params);
			if (rc) {
				netdev_dbg(bp->dev,
					   "Failed to create custom flow\n");
				goto err1;
			}
		}

		param_list++;
		type = param_list->type;
	}

	/* update the Roce Mirror capability */
	if (BNXT_MIRROR_ON_ROCE_CAP(bp) ||
	    ulp_port_db_mirror_vnic_enabled(ulp_ctx, port_id))
		ULP_BITMAP_SET(mapper_params.cf_bitmap,
			       BNXT_ULP_CF_BIT_ROCE_MIRROR_SUPPORT);

	/* Protect flow creation */
	mutex_lock(&ulp_ctx->cfg_data->flow_db_lock);

	mapper_params.flow_id = flow_id;
	rc = ulp_mapper_flow_create(ulp_ctx, &mapper_params,
				    NULL);

	mutex_unlock(&ulp_ctx->cfg_data->flow_db_lock);
err1:
	vfree(hdr_field);
	return rc;
}

int
bnxt_ulp_pf_veb_flow_create(struct bnxt *bp)
{
	struct bnxt_ulp_context *ulp_ctx = bp->ulp_ctx;
	u16 port_id = bp->pf.fw_fid;
	struct ulp_tlv_param param_list[] = {
		{
			.type = BNXT_ULP_DF_PARAM_TYPE_DEV_PORT_ID,
			.length = 2,
			.value = {(port_id >> 8) & 0xff, port_id & 0xff}
		},
		{
			.type = BNXT_ULP_DF_PARAM_TYPE_LAST,
			.length = 0,
			.value = {0}
		}
	};
	u32 flow_id = 0;
	u64 *comp_fld;
	int rc = 0;

	/* Not PF, or not P7plus or TF not enabled, then exit */
	if (!BNXT_PF(bp) || !BNXT_CHIP_P7_PLUS(bp) || !bp->ulp_ctx)
		return rc; /* no failure */

	comp_fld = vzalloc(sizeof(u64) * BNXT_ULP_CF_IDX_LAST);
	if (!comp_fld)
		return -ENOMEM;

	/* allocate the flow id */
	if (ulp_flow_db_reg_fid_alloc_with_lock(bp, &flow_id)) {
		vfree(comp_fld);
		return -ENOMEM;
	}

	if (ulp_flow_template_process(bp, param_list, comp_fld,
				      BNXT_ULP_TEMPLATE_PF_VEB_FILTER,
				      port_id, flow_id)) {
		rc = -EIO;
		ulp_flow_db_reg_fid_free_with_lock(bp, flow_id);
		netdev_dbg(bp->dev, "Failed to create veb filter\n");
	} else {
		/* Store the flow id */
		ulp_ctx->veb_flow_id = flow_id;
	}
	vfree(comp_fld);
	return rc;
}

int bnxt_ulp_pf_veb_flow_delete(struct bnxt *bp)
{
	struct bnxt_ulp_context *ulp_ctx = bp->ulp_ctx;
	int rc = 0;

	/* Not PF, or not P7plus or TF not enabled, then exit */
	if (!BNXT_PF(bp) || !BNXT_CHIP_P7_PLUS(bp) || !ulp_ctx)
		return rc; /* no failure */

	if (ulp_ctx->veb_flow_id) {
		rc = bnxt_ulp_gen_flow_destroy(bp, bp->pf.fw_fid,
					       ulp_ctx->veb_flow_id);
		ulp_ctx->veb_flow_id = 0;
	}
	return rc;
}

#ifdef CONFIG_VF_REPS

static int
bnxt_create_port_vfr_default_rule(struct bnxt *bp,
				  u8 flow_type,
				  u16 vfr_port_id,
				  u32 *flow_id)
{
	struct ulp_tlv_param param_list[] = {
		{
			.type = BNXT_ULP_DF_PARAM_TYPE_DEV_PORT_ID,
			.length = 2,
			.value = {(vfr_port_id >> 8) & 0xff, vfr_port_id & 0xff}
		},
		{
			.type = BNXT_ULP_DF_PARAM_TYPE_LAST,
			.length = 0,
			.value = {0}
		}
	};
	return ulp_default_flow_create(bp, param_list, flow_type,
				       vfr_port_id, flow_id);
}

int
bnxt_ulp_create_vfr_default_rules(void *vf_rep)
{
	struct bnxt_ulp_vfr_rule_info *info;
	struct bnxt_ulp_context *ulp_ctx;
	struct bnxt_vf_rep *vfr = vf_rep;
	struct bnxt *bp = vfr->bp;
	u16 vfr_port_id;
	int rc;

	if (!bp)
		return -EINVAL;

	ulp_ctx = bp->ulp_ctx;
	vfr_port_id = bp->pf.vf[vfr->vf_idx].fw_fid;

	info = bnxt_ulp_cntxt_ptr2_ulp_vfr_info_get(ulp_ctx, vfr_port_id);
	if (!info) {
		netdev_dbg(bp->dev, "Failed to get vfr ulp context\n");
		return -EINVAL;
	}

	if (info->valid) {
		netdev_dbg(bp->dev, "VFR already allocated\n");
		return -EINVAL;
	}

	memset(info, 0, sizeof(struct bnxt_ulp_vfr_rule_info));
	rc = bnxt_create_port_vfr_default_rule(bp, BNXT_ULP_DF_TPL_DEFAULT_VFR,
					       vfr_port_id,
					       &info->vfr_flow_id);
	if (rc) {
		netdev_dbg(bp->dev, "Failed to create VFR default rule\n");
		goto error;
	}

	/* If the template already set the bd action, skip this.
	 * This is handled differently between Thor and Thor2
	 */
	if (!BNXT_CHIP_P7(bp) || !vfr->tx_cfa_action) {
		rc = ulp_default_flow_db_cfa_action_get(ulp_ctx,
							info->vfr_flow_id,
							&vfr->tx_cfa_action);

		if (rc) {
			netdev_dbg(bp->dev, "Failed to get the tx cfa action\n");
			goto error;
		}
	}
	netdev_dbg(bp->dev, "VFR: Default flow id %d Tx cfa action is 0x%x\n",
		   info->vfr_flow_id, vfr->tx_cfa_action);

	/* Update the other details */
	info->valid = true;
	info->parent_port_id =  bp->pf.vf[vfr->vf_idx].fw_fid;

	return 0;

error:
	if (info->vfr_flow_id)
		ulp_default_flow_destroy(bp, info->vfr_flow_id);

	return rc;
}

int
bnxt_ulp_delete_vfr_default_rules(void *vf_rep)
{
	struct bnxt_ulp_vfr_rule_info *info;
	struct bnxt_ulp_context *ulp_ctx;
	struct bnxt_vf_rep *vfr = vf_rep;
	struct bnxt *bp = vfr->bp;
	u16 vfr_port_id;

	if (!bp || !BNXT_TRUFLOW_EN(bp))
		return 0;

	ulp_ctx = bp->ulp_ctx;
	vfr_port_id = bp->pf.vf[vfr->vf_idx].fw_fid;
	info = bnxt_ulp_cntxt_ptr2_ulp_vfr_info_get(ulp_ctx, vfr_port_id);
	if (!info) {
		netdev_dbg(bp->dev, "Failed to get vfr ulp context\n");
		return -EINVAL;
	}

	if (!info->valid) {
		netdev_dbg(bp->dev, "VFR already freed\n");
		return -EINVAL;
	}
	ulp_default_flow_destroy(bp, info->vfr_flow_id);
	vfr->tx_cfa_action = 0;
	memset(info, 0, sizeof(struct bnxt_ulp_vfr_rule_info));

	return 0;
}

int
bnxt_ulp_mirror_op(struct bnxt *bp, enum tf_dir dir, u8 enable)
{
	u16 port_id = bp->pf.fw_fid;
	struct ulp_tlv_param param_list[] = {
		{
			.type = BNXT_ULP_DF_PARAM_TYPE_DEV_PORT_ID,
			.length = 2,
			.value = {(port_id >> 8) & 0xff, port_id & 0xff}
		},
		{
			.type = BNXT_ULP_DF_PARAM_TYPE_LAST,
			.length = 0,
			.value = {0}
		}
	};
	u64 *comp_fld;
	u32 flow_type;
	int rc = 0;

	if (!BNXT_TRUFLOW_EN(bp) || bnxt_dev_is_vf_rep(bp->dev) ||
	    !bp->ulp_ctx)
		return rc;

	comp_fld = vzalloc(sizeof(u64) * BNXT_ULP_CF_IDX_LAST);
	if (!comp_fld) {
		rc = -ENOMEM;
		goto err1;
	}

	netdev_dbg(bp->dev,
		   "BNXT mirror op\n");

	comp_fld[BNXT_ULP_CF_IDX_MIRROR_OP] = (u64)enable;

	flow_type = (dir == TF_DIR_RX) ? BNXT_ULP_TEMPLATE_ROCE_MIRROR_OP_ING :
					 BNXT_ULP_TEMPLATE_ROCE_MIRROR_OP_EGR;

	if (ulp_flow_template_process(bp, param_list, comp_fld,
				      flow_type, port_id, 0))
		rc = -EIO;

err1:
	vfree(comp_fld);
	return rc;
}

#else

int
bnxt_ulp_create_vfr_default_rules(void *vf_rep)
{
		return -EINVAL;
}

int
bnxt_ulp_delete_vfr_default_rules(void *vf_rep)
{
		return -EINVAL;
}

#endif
#endif /* CONFIG_BNXT_FLOWER_OFFLOAD */
