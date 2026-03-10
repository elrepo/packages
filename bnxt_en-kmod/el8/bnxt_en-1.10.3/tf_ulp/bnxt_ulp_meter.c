// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#include "ulp_linux.h"
#include "bnxt_compat.h"
#include "bnxt_hsi.h"
#include "bnxt.h"
#include "bnxt_tfc.h"
#include "bnxt_tf_ulp.h"
#include "ulp_template_db_enum.h"
#include "ulp_template_struct.h"
#include "ulp_tc_parser.h"
#include "ulp_mapper.h"
#include "ulp_matcher.h"
#include "ulp_port_db.h"
#include "ulp_template_debug_proto.h"

/* Meter init status */
void bnxt_ulp_init_mapper_params(struct bnxt_ulp_mapper_parms *mparms,
				 struct ulp_tc_parser_params *params,
				 enum bnxt_ulp_fdb_type flow_type);

#ifdef CONFIG_BNXT_FLOWER_OFFLOAD

/* Internal api to setup global config.
 * returns 0 on success.
 */
static int bnxt_meter_global_cfg_update(struct bnxt *bp, enum tf_dir dir,
					enum tf_global_config_type type,
					u32 offset, u32 value, u32 set_flag)
{
	struct tf_global_cfg_parms parms = { 0 };
	u32 global_cfg = 0;
	struct tf *tfp;
	int rc = 0;

	parms.dir = dir,
	parms.type = type,
	parms.offset = offset,
	parms.config = (u8 *)&global_cfg,
	parms.config_sz_in_bytes = sizeof(global_cfg);

	tfp = bnxt_ulp_bp_tfp_get(bp, BNXT_ULP_SESSION_TYPE_DEFAULT);
	rc = tf_get_global_cfg(tfp, &parms);
	if (rc) {
		netdev_dbg(bp->dev, "Failed to get global cfg 0x%x rc:%d\n",
			   type, rc);
		return rc;
	}

	if (set_flag)
		global_cfg |= value;
	else
		global_cfg &= ~value;

	rc = tf_set_global_cfg(tfp, &parms);
	if (rc) {
		netdev_dbg(bp->dev, "Failed to set global cfg 0x%x rc:%d\n",
			   type, rc);
		return rc;
	}
	return rc;
}

#define BNXT_THOR_FMTCR_NUM_MET_MET_1K (0x7UL << 20)
#define BNXT_THOR_FMTCR_REMAP (0x1UL << 24)
#define BNXT_THOR_FMTCR_CNTRS_ENABLE (0x1UL << 25)
#define BNXT_THOR_FMTCR_INTERVAL_1K (1024)
#define BNXT_THOR_ACT_MTR_DROP_ON_RED_BIT BIT(14)
#define BNXT_THOR_FMTCR_INTERVAL_0  (0)

int bnxt_flow_meter_init(struct bnxt *bp)
{
	struct bnxt_ulp_context *ulp_ctx;
	int rc = 0;

	ulp_ctx = bnxt_ulp_bp_ptr2_cntxt_get(bp);
	if (!ulp_ctx || !ulp_ctx->cfg_data) {
		netdev_dbg(bp->dev, "ULP Context is not initialized\n");
		return -EINVAL;
	}

	/* Meters are supported only for DSCP Remap feature */
	if (!ULP_DSCP_REMAP_IS_ENABLED(ulp_ctx->cfg_data->ulp_flags)) {
		netdev_dbg(bp->dev, "DSCP_REMAP Capability is not enabled\n");
		return -EOPNOTSUPP;
	}

	/* Enable metering. Set the meter global configuration register.
	 * Set number of meter to 1K. Disable the drop counter for now.
	 */
	rc = bnxt_meter_global_cfg_update(bp, TF_DIR_RX, TF_METER_CFG,
					  0,
					  BNXT_THOR_FMTCR_NUM_MET_MET_1K |
					  BNXT_THOR_FMTCR_REMAP,
					  1);
	if (rc) {
		netdev_dbg(bp->dev, "Failed to set rx meter configuration\n");
		return rc;
	}

	rc = bnxt_meter_global_cfg_update(bp, TF_DIR_TX, TF_METER_CFG,
					  0,
					  BNXT_THOR_FMTCR_NUM_MET_MET_1K |
					  BNXT_THOR_FMTCR_REMAP,
					  1);
	if (rc) {
		netdev_dbg(bp->dev, "Failed to set tx meter configuration\n");
		return rc;
	}

	/* Set meter refresh rate to 1024 clock cycle. This value works for
	 * most bit rates especially for high rates.
	 */
	rc = bnxt_meter_global_cfg_update(bp, TF_DIR_RX, TF_METER_INTERVAL_CFG,
					  0, BNXT_THOR_FMTCR_INTERVAL_1K, 1);
	if (rc) {
		netdev_dbg(bp->dev, "Failed to set rx meter interval\n");
		return rc;
	}

	rc = bnxt_meter_global_cfg_update(bp, TF_DIR_TX, TF_METER_INTERVAL_CFG,
					  0, BNXT_THOR_FMTCR_INTERVAL_0, 1);
	if (rc) {
		netdev_dbg(bp->dev, "Failed to set tx meter interval\n");
		return rc;
	}

	/* act meter drop on red bit 1-drop 0-dont drop, set it to not drop */
	rc = bnxt_meter_global_cfg_update(bp, TF_DIR_TX, TF_ACT_MTR_CFG,
					  0, BNXT_THOR_ACT_MTR_DROP_ON_RED_BIT,
					  0);
	if (rc) {
		netdev_dbg(bp->dev, "Failed to disable tx meter drop on red\n");
		return rc;
	}

	ulp_ctx->cfg_data->meter_initialized = 1;
	netdev_dbg(bp->dev, "Flow meter has been initialized\n");
	return rc;
}

int bnxt_flow_meter_deinit(struct bnxt *bp)
{
	struct bnxt_ulp_context *ulp_ctx;
	int rc;

	ulp_ctx = bnxt_ulp_bp_ptr2_cntxt_get(bp);
	if (!ulp_ctx || !ulp_ctx->cfg_data) {
		netdev_dbg(bp->dev, "ULP Context is not initialized\n");
		return -EINVAL;
	}

	/* Meters are supported only for DSCP Remap feature */
	if (!ULP_DSCP_REMAP_IS_ENABLED(ulp_ctx->cfg_data->ulp_flags)) {
		netdev_dbg(bp->dev, "DSCP_REMAP Capability is not enabled\n");
		return -EOPNOTSUPP;
	}

	/* act meter drop on red bit 1-drop 0-dont drop, reset it drop */
	rc = bnxt_meter_global_cfg_update(bp, TF_DIR_TX, TF_ACT_MTR_CFG,
					  0, BNXT_THOR_ACT_MTR_DROP_ON_RED_BIT,
					  1);
	if (rc) {
		netdev_dbg(bp->dev, "Failed to enable tx meter drop on red\n");
		return rc;
	}

	ulp_ctx->cfg_data->meter_initialized = 0;
	netdev_dbg(bp->dev, "Flow meter has been de-initialized\n");
	return rc;
}

/* Calculate mantissa and exponent for cir / eir reg. */
#define BNXT_CPU_CLOCK 800
#define MEGA 1000000
#define NUM_BIT_PER_BYTE 8
static void bnxt_ulp_flow_meter_xir_calc(u64 xir, u32 *reg)
{
	u8 *swap = 0;
	u16 m = 0;
	u16 e = 0;
	u64 temp;

	/* Special case xir == 0 ? both exp and matissa are 0. */
	if (xir == 0) {
		*reg = 0;
		return;
	}

	/* e = floor(log2(cir)) + 27
	 * a (MBps) = xir (bps) / MEGA
	 * b (MBpc) = a (MBps) / CPU_CLOCK (Mcps)
	 * e = floor(log2(b)) + 27
	 */
	temp = xir * (1 << 24) / (BNXT_CPU_CLOCK >> 3) / MEGA;
	e = ilog2(temp);

	/* m = round(b/2^(e-27) - 1) * 2048
	 *   = round(b*2^(27-e) - 1) * 2^11
	 *   = round(b*2^(38-e) - 2^11)
	 */
	m = xir * (1 << (38 - e)) / BNXT_CPU_CLOCK / MEGA - (1 << 11);
	*reg = ((m & 0x7FF) << 6) | (e & 0x3F);
	swap = (u8 *)reg;
	*reg = swap[0] << 16 | swap[1] << 8 | swap[2];
}

/* Calculate mantissa and exponent for cbs / ebs reg */
static void bnxt_ulp_flow_meter_xbs_calc(u64 xbs, u16 *reg)
{
	u16 m = 0;
	u16 e = 0;

	if (xbs == 0) {
		*reg = 0;
		return;
	}

	/* e = floor(log2(xbs)) + 1 */
	e = ilog2(xbs) + 1;

	/* m = round(xbs/2^(e-1) - 1) * 128
	 *   = round(xbs*2^(1-e) - 1) * 2^7
	 *   = round(xbs*2^(8-e) - 2^7)
	 */
	m = xbs / (1 << (e - 8)) - (1 << 7);
	*reg = ((m & 0x7F) << 5) | (e & 0x1F);
	*reg = cpu_to_be16(*reg);
}

/* Parse the meter profile. */
static int bnxt_ulp_meter_profile_alloc(struct bnxt *bp,
					struct ulp_tc_act_prop *act_prop,
					u64 cir, u64 eir, u64 cbs, u64 ebs)
{
	bool alg_rfc2698 = false;
	u32 cir_reg, eir_reg;
	u16 cbs_reg, ebs_reg;
	bool cbnd = true;
	bool ebnd = true;
	bool pm = false;

	/* The CBS and EBS must be configured so that at least one
	 * of them is larger than 0.  It is recommended that when
	 * the value of the CBS or the EBS is larger than 0, it
	 * is larger than or equal to the size of the largest possible
	 * IP packet in the stream.
	 */
	if (cbs == 0 && ebs == 0) {
		netdev_dbg(bp->dev,
			   "CBS & EBS cannot both be 0; one of them should be > MTU\n");
		return -EINVAL;
	}

	bnxt_ulp_flow_meter_xir_calc(cir, &cir_reg);
	memcpy(&act_prop->act_details[BNXT_ULP_ACT_PROP_IDX_METER_PROF_CIR],
	       &cir_reg,
	       BNXT_ULP_ACT_PROP_SZ_METER_PROF_CIR);

	bnxt_ulp_flow_meter_xir_calc(eir, &eir_reg);
	memcpy(&act_prop->act_details[BNXT_ULP_ACT_PROP_IDX_METER_PROF_EIR],
	       &eir_reg,
	       BNXT_ULP_ACT_PROP_SZ_METER_PROF_EIR);

	bnxt_ulp_flow_meter_xbs_calc(cbs, &cbs_reg);
	memcpy(&act_prop->act_details[BNXT_ULP_ACT_PROP_IDX_METER_PROF_CBS],
	       &cbs_reg,
	       BNXT_ULP_ACT_PROP_SZ_METER_PROF_CBS);

	bnxt_ulp_flow_meter_xbs_calc(ebs, &ebs_reg);
	memcpy(&act_prop->act_details[BNXT_ULP_ACT_PROP_IDX_METER_PROF_EBS],
	       &ebs_reg,
	       BNXT_ULP_ACT_PROP_SZ_METER_PROF_EBS);

	memcpy(&act_prop->act_details[BNXT_ULP_ACT_PROP_IDX_METER_PROF_RFC2698],
	       &alg_rfc2698,
	       BNXT_ULP_ACT_PROP_SZ_METER_PROF_RFC2698);

	memcpy(&act_prop->act_details[BNXT_ULP_ACT_PROP_IDX_METER_PROF_PM],
	       &pm,
	       BNXT_ULP_ACT_PROP_SZ_METER_PROF_PM);

	memcpy(&act_prop->act_details[BNXT_ULP_ACT_PROP_IDX_METER_PROF_CBND],
	       &cbnd,
	       BNXT_ULP_ACT_PROP_SZ_METER_PROF_CBND);

	memcpy(&act_prop->act_details[BNXT_ULP_ACT_PROP_IDX_METER_PROF_EBND],
	       &ebnd,
	       BNXT_ULP_ACT_PROP_SZ_METER_PROF_EBND);

	return 0;
}

/* Allocate a meter profile for the specified color.
 * The byte values of EBS and CBS for each color are
 * defined such that they result in the following
 * register values.
 *
 * Meter EBS/CBS Bytes to Register value mapping:
 * 4 Bytes      : 0x3
 * 8 Bytes      : 0x4
 * 131072 Bytes : 0x12
 * 262144 Bytes : 0x13
 */
#define GREEN_CBS	262144
#define GREEN_EBS	131072
#define YELLOW_CBS	4
#define YELLOW_EBS	131072
#define RED_CBS		4
#define	RED_EBS		8

static int bnxt_ulp_meter_profile_alloc_color(struct bnxt *bp,
					      struct ulp_tc_act_prop *act_prop,
					      enum bnxt_ulp_meter_color color,
					      u64 cir, u64 eir)
{
	bool alg_rfc2698 = true;
	u16 cbs_reg, ebs_reg;
	u32 cir_reg, eir_reg;
	bool cbnd = true;
	bool ebnd = true;
	bool ebsm = true;
	bool cbsm = true;
	bool pm = false;
	u64 cbs, ebs;

	switch (color) {
	case MTR_PROF_CLR_GREEN:
		cbs = GREEN_CBS;
		ebs = GREEN_EBS;
		break;
	case MTR_PROF_CLR_YELLOW:
		cbs = YELLOW_CBS;
		ebs = YELLOW_EBS;
		break;
	case MTR_PROF_CLR_RED:
		cbs = RED_CBS;
		ebs = RED_EBS;
		break;
	default:
		return -EINVAL;
	}

	netdev_info(bp->dev, "%s: color: %d\n", __func__, color);
	bnxt_ulp_flow_meter_xir_calc(cir, &cir_reg);
	memcpy(&act_prop->act_details[BNXT_ULP_ACT_PROP_IDX_METER_PROF_CIR],
	       &cir_reg,
	       BNXT_ULP_ACT_PROP_SZ_METER_PROF_CIR);

	bnxt_ulp_flow_meter_xir_calc(eir, &eir_reg);
	memcpy(&act_prop->act_details[BNXT_ULP_ACT_PROP_IDX_METER_PROF_EIR],
	       &eir_reg,
	       BNXT_ULP_ACT_PROP_SZ_METER_PROF_EIR);

	bnxt_ulp_flow_meter_xbs_calc(cbs, &cbs_reg);
	memcpy(&act_prop->act_details[BNXT_ULP_ACT_PROP_IDX_METER_PROF_CBS],
	       &cbs_reg,
	       BNXT_ULP_ACT_PROP_SZ_METER_PROF_CBS);

	bnxt_ulp_flow_meter_xbs_calc(ebs, &ebs_reg);
	memcpy(&act_prop->act_details[BNXT_ULP_ACT_PROP_IDX_METER_PROF_EBS],
	       &ebs_reg,
	       BNXT_ULP_ACT_PROP_SZ_METER_PROF_EBS);

	memcpy(&act_prop->act_details[BNXT_ULP_ACT_PROP_IDX_METER_PROF_RFC2698],
	       &alg_rfc2698,
	       BNXT_ULP_ACT_PROP_SZ_METER_PROF_RFC2698);

	memcpy(&act_prop->act_details[BNXT_ULP_ACT_PROP_IDX_METER_PROF_PM],
	       &pm,
	       BNXT_ULP_ACT_PROP_SZ_METER_PROF_PM);

	memcpy(&act_prop->act_details[BNXT_ULP_ACT_PROP_IDX_METER_PROF_CBND],
	       &cbnd,
	       BNXT_ULP_ACT_PROP_SZ_METER_PROF_CBND);

	memcpy(&act_prop->act_details[BNXT_ULP_ACT_PROP_IDX_METER_PROF_EBND],
	       &ebnd,
	       BNXT_ULP_ACT_PROP_SZ_METER_PROF_EBND);

	memcpy(&act_prop->act_details[BNXT_ULP_ACT_PROP_IDX_METER_PROF_CBSM],
	       &cbsm,
	       BNXT_ULP_ACT_PROP_SZ_METER_PROF_CBSM);

	memcpy(&act_prop->act_details[BNXT_ULP_ACT_PROP_IDX_METER_PROF_EBSM],
	       &ebsm,
	       BNXT_ULP_ACT_PROP_SZ_METER_PROF_EBSM);

	return 0;
}

#define MTR_PROF_DEFAULT_CIR	128000000
#define MTR_PROF_DEFAULT_EIR	128000000
#define MTR_PROF_DEFAULT_CBS	131072
#define MTR_PROF_DEFAULT_EBS	131072

static struct bnxt_ulp_mapper_parms mapper_mparms = { 0 };
static struct ulp_tc_parser_params pparams = {{ 0 }};

/* Add MTR profile. */
int bnxt_flow_meter_profile_add(struct bnxt *bp, u32 meter_profile_id, u32 dir,
				enum bnxt_ulp_meter_color color)
{
	struct ulp_tc_act_prop *act_prop;
	struct bnxt_ulp_context *ulp_ctx;
	u32 tmp_profile_id;
	u32 act_tid;
	int rc;

	ulp_ctx = bnxt_ulp_bp_ptr2_cntxt_get(bp);
	if (!ulp_ctx) {
		netdev_dbg(bp->dev, "ULP Context is not initialized\n");
		return -EINVAL;
	}

	act_prop = &pparams.act_prop;

	/* Initialize the parser params */
	memset(&pparams, 0, sizeof(struct ulp_tc_parser_params));
	pparams.ulp_ctx = ulp_ctx;
	pparams.act_bitmap.bits = BNXT_ULP_ACT_BIT_METER_PROFILE;
	pparams.act_bitmap.bits |= (dir == BNXT_ULP_FLOW_ATTR_INGRESS ?
					BNXT_ULP_FLOW_DIR_BITMASK_ING :
					BNXT_ULP_FLOW_DIR_BITMASK_EGR);
	pparams.app_id = 1;
	pparams.dir_attr |= dir;

	tmp_profile_id = cpu_to_be32(meter_profile_id);
	memcpy(&act_prop->act_details[BNXT_ULP_ACT_PROP_IDX_METER_PROF_ID],
	       &tmp_profile_id,
	       BNXT_ULP_ACT_PROP_SZ_METER_PROF_ID);

	if (color != MTR_PROF_CLR_INVALID) {
		rc = bnxt_ulp_meter_profile_alloc_color(bp, act_prop, color,
							0, 0);
	} else {
		rc = bnxt_ulp_meter_profile_alloc(bp, act_prop,
						  MTR_PROF_DEFAULT_CIR,
						  MTR_PROF_DEFAULT_EIR,
						  MTR_PROF_DEFAULT_CBS,
						  MTR_PROF_DEFAULT_EBS);
	}
	if (rc)
		return rc;

	ulp_parser_act_info_dump(&pparams);
	rc = ulp_matcher_action_match(&pparams, &act_tid);
	if (rc != BNXT_TF_RC_SUCCESS)
		return rc;

	bnxt_ulp_init_mapper_params(&mapper_mparms, &pparams,
				    BNXT_ULP_FDB_TYPE_REGULAR);
	mapper_mparms.act_tid = act_tid;

	rc = ulp_mapper_flow_create(ulp_ctx, &mapper_mparms, NULL);
	if (rc)
		return rc;

	netdev_dbg(bp->dev, "Flow meter profile %d is created\n", meter_profile_id);
	return 0;
}

/* Delete meter profile */
int bnxt_flow_meter_profile_delete(struct bnxt *bp, u32 meter_profile_id, u32 dir)
{
	struct ulp_tc_act_prop *act_prop;
	struct bnxt_ulp_context *ulp_ctx;
	u32 tmp_profile_id;
	u32 act_tid;
	int rc;

	ulp_ctx = bnxt_ulp_bp_ptr2_cntxt_get(bp);
	if (!ulp_ctx || !ulp_ctx->cfg_data) {
		netdev_dbg(bp->dev, "ULP Context is not initialized\n");
		return -EINVAL;
	}

	if (!ulp_ctx->cfg_data->meter_initialized) {
		netdev_dbg(bp->dev, "Meter is not initialized\n");
		return -EOPNOTSUPP;
	}

	act_prop = &pparams.act_prop;

	/* Initialize the parser params */
	memset(&pparams, 0, sizeof(struct ulp_tc_parser_params));
	pparams.ulp_ctx = ulp_ctx;
	pparams.act_bitmap.bits = BNXT_ULP_ACT_BIT_METER_PROFILE;
	pparams.act_bitmap.bits |= BNXT_ULP_ACT_BIT_DELETE;
	pparams.act_bitmap.bits |= (dir == BNXT_ULP_FLOW_ATTR_INGRESS ?
					BNXT_ULP_FLOW_DIR_BITMASK_ING :
					BNXT_ULP_FLOW_DIR_BITMASK_EGR);
	pparams.app_id = 1;
	pparams.dir_attr |= dir;

	tmp_profile_id = cpu_to_be32(meter_profile_id);
	memcpy(&act_prop->act_details[BNXT_ULP_ACT_PROP_IDX_METER_PROF_ID],
	       &tmp_profile_id,
	       BNXT_ULP_ACT_PROP_SZ_METER_PROF_ID);

	ulp_parser_act_info_dump(&pparams);
	rc = ulp_matcher_action_match(&pparams, &act_tid);
	if (rc != BNXT_TF_RC_SUCCESS)
		return rc;

	bnxt_ulp_init_mapper_params(&mapper_mparms, &pparams,
				    BNXT_ULP_FDB_TYPE_REGULAR);
	mapper_mparms.act_tid = act_tid;

	rc = ulp_mapper_flow_create(ulp_ctx, &mapper_mparms, NULL);
	if (rc)
		return rc;

	netdev_dbg(bp->dev, "Flow meter profile %d deleted\n",
		   meter_profile_id);
	return 0;
}

/* Create meter */
int bnxt_flow_meter_create(struct bnxt *bp, u32 meter_profile_id, u32 meter_id, u32 dir)
{
	struct ulp_tc_act_prop *act_prop;
	struct bnxt_ulp_context *ulp_ctx;
	u32 tmp_meter_id, tmp_profile_id;
	bool meter_en = true;
	u32 act_tid;
	int rc;

	ulp_ctx = bnxt_ulp_bp_ptr2_cntxt_get(bp);
	if (!ulp_ctx || !ulp_ctx->cfg_data) {
		netdev_dbg(bp->dev, "ULP Context is not initialized\n");
		return -EINVAL;
	}

	if (!ulp_ctx->cfg_data->meter_initialized) {
		netdev_dbg(bp->dev, "Meter is not initialized\n");
		return -EOPNOTSUPP;
	}

	act_prop = &pparams.act_prop;

	/* Initialize the parser params */
	memset(&pparams, 0, sizeof(struct ulp_tc_parser_params));
	pparams.ulp_ctx = ulp_ctx;
	pparams.act_bitmap.bits = BNXT_ULP_ACT_BIT_SHARED_METER;
	pparams.act_bitmap.bits |= (dir == BNXT_ULP_FLOW_ATTR_INGRESS ?
					BNXT_ULP_FLOW_DIR_BITMASK_ING :
					BNXT_ULP_FLOW_DIR_BITMASK_EGR);
	pparams.app_id = 1;
	pparams.dir_attr |= dir;

	tmp_meter_id = cpu_to_be32(meter_id);
	memcpy(&act_prop->act_details[BNXT_ULP_ACT_PROP_IDX_METER_INST_ID],
	       &tmp_meter_id,
	       BNXT_ULP_ACT_PROP_SZ_METER_INST_ID);

	tmp_profile_id = cpu_to_be32(meter_profile_id);
	memcpy(&act_prop->act_details[BNXT_ULP_ACT_PROP_IDX_METER_PROF_ID],
	       &tmp_profile_id,
	       BNXT_ULP_ACT_PROP_SZ_METER_PROF_ID);

	memcpy(&act_prop->act_details[BNXT_ULP_ACT_PROP_IDX_METER_INST_MTR_VAL],
	       &meter_en,
	       BNXT_ULP_ACT_PROP_SZ_METER_INST_MTR_VAL);

	ulp_parser_act_info_dump(&pparams);
	rc = ulp_matcher_action_match(&pparams, &act_tid);
	if (rc != BNXT_TF_RC_SUCCESS)
		return rc;

	bnxt_ulp_init_mapper_params(&mapper_mparms, &pparams,
				    BNXT_ULP_FDB_TYPE_REGULAR);
	mapper_mparms.act_tid = act_tid;

	rc = ulp_mapper_flow_create(ulp_ctx, &mapper_mparms, NULL);
	if (rc)
		return rc;

	netdev_dbg(bp->dev, "Flow meter %d is created\n", meter_id);
	return 0;
}

/* Destroy meter */
int bnxt_flow_meter_destroy(struct bnxt *bp, u32 meter_id, u32 dir)
{
	struct ulp_tc_act_prop *act_prop;
	struct bnxt_ulp_context *ulp_ctx;
	u32 tmp_meter_id;
	u32 act_tid;
	int rc;

	ulp_ctx = bnxt_ulp_bp_ptr2_cntxt_get(bp);
	if (!ulp_ctx || !ulp_ctx->cfg_data) {
		netdev_dbg(bp->dev, "ULP Context is not initialized\n");
		return -EINVAL;
	}

	if (!ulp_ctx->cfg_data->meter_initialized) {
		netdev_dbg(bp->dev, "Meter is not initialized\n");
		return -EOPNOTSUPP;
	}

	act_prop = &pparams.act_prop;

	/* Initialize the parser params */
	memset(&pparams, 0, sizeof(struct ulp_tc_parser_params));
	pparams.ulp_ctx = ulp_ctx;
	pparams.act_bitmap.bits = BNXT_ULP_ACT_BIT_SHARED_METER;
	pparams.act_bitmap.bits |= BNXT_ULP_ACT_BIT_DELETE;
	pparams.act_bitmap.bits |= (dir == BNXT_ULP_FLOW_ATTR_INGRESS ?
					BNXT_ULP_FLOW_DIR_BITMASK_ING :
					BNXT_ULP_FLOW_DIR_BITMASK_EGR);
	pparams.app_id = 1;
	pparams.dir_attr |= dir;

	tmp_meter_id = cpu_to_be32(meter_id);
	memcpy(&act_prop->act_details[BNXT_ULP_ACT_PROP_IDX_METER_INST_ID],
	       &tmp_meter_id,
	       BNXT_ULP_ACT_PROP_SZ_METER_INST_ID);

	ulp_parser_act_info_dump(&pparams);
	rc = ulp_matcher_action_match(&pparams, &act_tid);
	if (rc != BNXT_TF_RC_SUCCESS)
		return rc;

	bnxt_ulp_init_mapper_params(&mapper_mparms, &pparams,
				    BNXT_ULP_FDB_TYPE_REGULAR);
	mapper_mparms.act_tid = act_tid;

	rc = ulp_mapper_flow_create(ulp_ctx, &mapper_mparms, NULL);
	if (rc)
		return rc;

	netdev_dbg(bp->dev, "Flow meter %d is deleted\n", meter_id);
	return 0;
}

#else /* CONFIG_BNXT_FLOWER_OFFLOAD */

int bnxt_flow_meter_init(struct bnxt *bp)
{
	return -EOPNOTSUPP;
}

#endif /* CONFIG_BNXT_FLOWER_OFFLOAD */
