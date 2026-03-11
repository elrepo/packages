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
#include "bnxt_tf_ulp.h"
#include "bnxt_tf_ulp_p5.h"
#include "bnxt_ulp_flow.h"
#include "bnxt_tf_common.h"
#include "ulp_tc_parser.h"
#include "tf_core.h"
#include "tf_ext_flow_handle.h"

#include "ulp_template_db_enum.h"
#include "ulp_template_struct.h"
#include "ulp_mark_mgr.h"
#include "ulp_fc_mgr.h"
#include "ulp_flow_db.h"
#include "ulp_mapper.h"
#include "ulp_matcher.h"
#include "ulp_port_db.h"

#ifdef CONFIG_BNXT_FLOWER_OFFLOAD
/* Function to set the tfp session details from the ulp context. */
static int
bnxt_tf_ulp_cntxt_tfp_set(struct bnxt_ulp_context *ulp,
			  enum bnxt_ulp_session_type s_type,
			  struct tf *tfp)
{
	u32 idx = 0;
	enum bnxt_ulp_tfo_type tfo_type = BNXT_ULP_TFO_TYPE_P5;

	if (!ulp)
		return -EINVAL;

	if (ULP_MULTI_SHARED_IS_SUPPORTED(ulp)) {
		if (s_type & BNXT_ULP_SESSION_TYPE_SHARED)
			idx = 1;
		else if (s_type & BNXT_ULP_SESSION_TYPE_SHARED_WC)
			idx = 2;

	} else {
		if ((s_type & BNXT_ULP_SESSION_TYPE_SHARED) ||
		    (s_type & BNXT_ULP_SESSION_TYPE_SHARED_WC))
			idx = 1;
	}

	ulp->g_tfp[idx] = tfp;

	if (!tfp) {
		u32 i = 0;

		while (i < BNXT_ULP_SESSION_MAX && !ulp->g_tfp[i])
			i++;
		if (i == BNXT_ULP_SESSION_MAX)
			ulp->tfo_type = BNXT_ULP_TFO_TYPE_INVALID;
	} else {
		ulp->tfo_type = tfo_type;
	}
	netdev_dbg(ulp->bp->dev, "%s Setting tfo_type %d session tpye %d\n",
		   __func__, tfo_type, s_type);
	return 0;
}

/* Function to get the tfp session details from the ulp context. */
void *
bnxt_tf_ulp_cntxt_tfp_get(struct bnxt_ulp_context *ulp,
			  enum bnxt_ulp_session_type s_type)
{
	u32 idx = 0;

	if (!ulp)
		return NULL;

	if (ulp->tfo_type != BNXT_ULP_TFO_TYPE_P5) {
		netdev_dbg(ulp->bp->dev, "Wrong tf type %d != %d\n",
			   ulp->tfo_type, BNXT_ULP_TFO_TYPE_P5);
		return NULL;
	}

	if (ULP_MULTI_SHARED_IS_SUPPORTED(ulp)) {
		if (s_type & BNXT_ULP_SESSION_TYPE_SHARED)
			idx = 1;
		else if (s_type & BNXT_ULP_SESSION_TYPE_SHARED_WC)
			idx = 2;
	} else {
		if ((s_type & BNXT_ULP_SESSION_TYPE_SHARED) ||
		    (s_type & BNXT_ULP_SESSION_TYPE_SHARED_WC))
			idx = 1;
	}
	return (struct tf *)ulp->g_tfp[idx];
}

struct tf *bnxt_get_tfp_session(struct bnxt *bp, enum bnxt_session_type type)
{
	struct tf *tfp = bp->tfp;

	return (type >= BNXT_SESSION_TYPE_LAST) ?
		&tfp[BNXT_SESSION_TYPE_REGULAR] : &tfp[type];
}

struct tf *
bnxt_ulp_bp_tfp_get(struct bnxt *bp, enum bnxt_ulp_session_type type)
{
	enum bnxt_session_type btype;

	if (type & BNXT_ULP_SESSION_TYPE_SHARED)
		btype = BNXT_SESSION_TYPE_SHARED_COMMON;
	else if (type & BNXT_ULP_SESSION_TYPE_SHARED_WC)
		btype = BNXT_SESSION_TYPE_SHARED_WC;
	else
		btype = BNXT_SESSION_TYPE_REGULAR;

	return bnxt_get_tfp_session(bp, btype);
}

static int
ulp_tf_named_resources_calc(struct bnxt_ulp_context *ulp_ctx,
			    struct bnxt_ulp_glb_resource_info *info,
			    u32 num,
			    enum bnxt_ulp_session_type stype,
			    struct tf_session_resources *res)
{
	u32 dev_id = BNXT_ULP_DEVICE_ID_LAST, res_type, i;
	enum tf_dir dir;
	int rc = 0;
	u8 app_id;

	if (!ulp_ctx || !info || !res || !num)
		return -EINVAL;

	rc = bnxt_ulp_cntxt_app_id_get(ulp_ctx, &app_id);
	if (rc) {
		netdev_dbg(ulp_ctx->bp->dev, "Unable to get the app id from ulp.\n");
		return -EINVAL;
	}

	rc = bnxt_ulp_cntxt_dev_id_get(ulp_ctx, &dev_id);
	if (rc) {
		netdev_dbg(ulp_ctx->bp->dev, "Unable to get the dev id from ulp.\n");
		return -EINVAL;
	}

	for (i = 0; i < num; i++) {
		if (dev_id != info[i].device_id || app_id != info[i].app_id)
			continue;
		/* check to see if the session type matches only then include */
		if ((stype || info[i].session_type) &&
		    !(info[i].session_type & stype))
			continue;

		dir = info[i].direction;
		res_type = info[i].resource_type;

		switch (info[i].resource_func) {
		case BNXT_ULP_RESOURCE_FUNC_IDENTIFIER:
			res->ident_cnt[dir].cnt[res_type]++;
			break;
		case BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE:
			res->tbl_cnt[dir].cnt[res_type]++;
			break;
		case BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE:
			res->tcam_cnt[dir].cnt[res_type]++;
			break;
		case BNXT_ULP_RESOURCE_FUNC_EM_TABLE:
			res->em_cnt[dir].cnt[res_type]++;
			break;
		default:
			netdev_dbg(ulp_ctx->bp->dev, "Unknown resource func (0x%x)\n,",
				   info[i].resource_func);
			continue;
		}
	}

	return 0;
}

static int
ulp_tf_unnamed_resources_calc(struct bnxt_ulp_context *ulp_ctx,
			      struct bnxt_ulp_resource_resv_info *info,
			      u32 num,
			      enum bnxt_ulp_session_type stype,
			      struct tf_session_resources *res)
{
	u32 dev_id, res_type, i;
	enum tf_dir dir;
	int rc = 0;
	u8 app_id;

	if (!ulp_ctx || !res || !info || num == 0)
		return -EINVAL;

	rc = bnxt_ulp_cntxt_app_id_get(ulp_ctx, &app_id);
	if (rc) {
		netdev_dbg(ulp_ctx->bp->dev, "Unable to get the app id from ulp.\n");
		return -EINVAL;
	}

	rc = bnxt_ulp_cntxt_dev_id_get(ulp_ctx, &dev_id);
	if (rc) {
		netdev_dbg(ulp_ctx->bp->dev, "Unable to get the dev id from ulp.\n");
		return -EINVAL;
	}

	for (i = 0; i < num; i++) {
		if (app_id != info[i].app_id || dev_id != info[i].device_id)
			continue;

		/* check to see if the session type matches only then include */
		if ((stype || info[i].session_type) &&
		    !(info[i].session_type & stype))
			continue;

		dir = info[i].direction;
		res_type = info[i].resource_type;

		switch (info[i].resource_func) {
		case BNXT_ULP_RESOURCE_FUNC_IDENTIFIER:
			res->ident_cnt[dir].cnt[res_type] = info[i].count;
			break;
		case BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE:
			res->tbl_cnt[dir].cnt[res_type] = info[i].count;
			break;
		case BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE:
			res->tcam_cnt[dir].cnt[res_type] = info[i].count;
			break;
		case BNXT_ULP_RESOURCE_FUNC_EM_TABLE:
			res->em_cnt[dir].cnt[res_type] = info[i].count;
			break;
		default:
			netdev_dbg(ulp_ctx->bp->dev, "Unsupported resource\n");
			return -EINVAL;
		}
	}
	return 0;
}

static int
ulp_tf_resources_get(struct bnxt_ulp_context *ulp_ctx,
		     enum bnxt_ulp_session_type stype,
		     struct tf_session_resources *res)
{
	struct bnxt_ulp_resource_resv_info *unnamed = NULL;
	int rc = 0;
	u32 unum;

	if (!ulp_ctx || !res)
		return -EINVAL;

	/* use DEFAULT_NON_HA instead of DEFAULT resources if HA is disabled */
	if (ULP_APP_HA_IS_DYNAMIC(ulp_ctx))
		stype = ulp_ctx->cfg_data->def_session_type;

	unnamed = bnxt_ulp_resource_resv_list_get(&unum);
	if (!unnamed) {
		netdev_dbg(ulp_ctx->bp->dev, "Unable to get resource resv list.\n");
		return -EINVAL;
	}

	rc = ulp_tf_unnamed_resources_calc(ulp_ctx, unnamed, unum, stype, res);
	if (rc)
		netdev_dbg(ulp_ctx->bp->dev, "Unable to calc resources for session.\n");

	return rc;
}

static int
ulp_tf_shared_session_resources_get(struct bnxt_ulp_context *ulp_ctx,
				    enum bnxt_ulp_session_type stype,
				    struct tf_session_resources *res)
{
	struct bnxt_ulp_resource_resv_info *unnamed;
	struct bnxt_ulp_glb_resource_info *named;
	u32 unum = 0, nnum = 0;
	int rc;

	if (!ulp_ctx || !res)
		return -EINVAL;

	/* Make sure the resources are zero before accumulating. */
	memset(res, 0, sizeof(struct tf_session_resources));

	/* Shared resources are comprised of both named and unnamed resources.
	 * First get the unnamed counts, and then add the named to the result.
	 */
	/* Get the baseline counts */
	unnamed = bnxt_ulp_app_resource_resv_list_get(&unum);
	if (unum) {
		rc = ulp_tf_unnamed_resources_calc(ulp_ctx, unnamed,
						   unum, stype, res);
		if (rc) {
			netdev_dbg(ulp_ctx->bp->dev,
				   "Unable to calc resources for shared session.\n");
			return -EINVAL;
		}
	}

	/* Get the named list and add the totals */
	named = bnxt_ulp_app_glb_resource_info_list_get(&nnum);
	/* No need to calc resources, none to calculate */
	if (!nnum)
		return 0;

	rc = ulp_tf_named_resources_calc(ulp_ctx, named, nnum, stype, res);
	if (rc)
		netdev_dbg(ulp_ctx->bp->dev, "Unable to calc named resources\n");

	return rc;
}

/* Function to set the hot upgrade support into the context */
static int
ulp_tf_multi_shared_session_support_set(struct bnxt *bp,
					enum bnxt_ulp_device_id devid,
					u32 fw_hu_update)
{
	struct bnxt_ulp_context *ulp_ctx = bp->ulp_ctx;
	struct tf_get_version_parms v_params = { 0 };
	int32_t new_fw = 0;
	struct tf *tfp;
	int32_t rc = 0;

	v_params.device_type = bnxt_ulp_cntxt_convert_dev_id(bp, devid);
	v_params.bp = bp;

	tfp = bnxt_ulp_bp_tfp_get(bp, BNXT_ULP_SESSION_TYPE_DEFAULT);
	rc = tf_get_version(tfp, &v_params);
	if (rc) {
		netdev_dbg(bp->dev, "Unable to get tf version.\n");
		return rc;
	}

	if (v_params.major == 1 && v_params.minor == 0 &&
	    v_params.update == 1) {
		new_fw = 1;
	}
	/* if the version update is greater than 0 then set support for
	 * multiple version
	 */
	if (new_fw) {
		ulp_ctx->cfg_data->ulp_flags |= BNXT_ULP_MULTI_SHARED_SUPPORT;
		ulp_ctx->cfg_data->hu_session_type =
			BNXT_ULP_SESSION_TYPE_SHARED;
	}
	if (!new_fw && fw_hu_update) {
		ulp_ctx->cfg_data->ulp_flags &= ~BNXT_ULP_HIGH_AVAIL_ENABLED;
		ulp_ctx->cfg_data->hu_session_type =
			BNXT_ULP_SESSION_TYPE_SHARED |
			BNXT_ULP_SESSION_TYPE_SHARED_OWC;
	}

	if (!new_fw && !fw_hu_update) {
		ulp_ctx->cfg_data->hu_session_type =
			BNXT_ULP_SESSION_TYPE_SHARED |
			BNXT_ULP_SESSION_TYPE_SHARED_OWC;
	}

	return rc;
}

static int
ulp_tf_cntxt_app_caps_init(struct bnxt *bp,
			   u8 app_id, u32 dev_id)
{
	struct bnxt_ulp_context *ulp_ctx = bp->ulp_ctx;
	struct bnxt_ulp_app_capabilities_info *info;
	uint32_t num = 0, fw = 0;
	bool found = false;
	uint16_t i;

	if (ULP_APP_DEV_UNSUPPORTED_ENABLED(ulp_ctx->cfg_data->ulp_flags)) {
		netdev_dbg(bp->dev, "APP ID %d, Device ID: 0x%x not supported.\n",
			   app_id, dev_id);
		return -EINVAL;
	}

	info = bnxt_ulp_app_cap_list_get(&num);
	if (!info || !num) {
		netdev_dbg(bp->dev, "Failed to get app capabilities.\n");
		return -EINVAL;
	}

	for (i = 0; i < num; i++) {
		if (info[i].app_id != app_id || info[i].device_id != dev_id)
			continue;
		found = true;
		if (info[i].flags & BNXT_ULP_APP_CAP_SHARED_EN)
			ulp_ctx->cfg_data->ulp_flags |=
				BNXT_ULP_SHARED_SESSION_ENABLED;
		if (info[i].flags & BNXT_ULP_APP_CAP_HOT_UPGRADE_EN)
			ulp_ctx->cfg_data->ulp_flags |=
				BNXT_ULP_HIGH_AVAIL_ENABLED;
		if (info[i].flags & BNXT_ULP_APP_CAP_UNICAST_ONLY)
			ulp_ctx->cfg_data->ulp_flags |=
				BNXT_ULP_APP_UNICAST_ONLY;
		if (info[i].flags & BNXT_ULP_APP_CAP_IP_TOS_PROTO_SUPPORT)
			ulp_ctx->cfg_data->ulp_flags |=
				BNXT_ULP_APP_TOS_PROTO_SUPPORT;
		if (info[i].flags & BNXT_ULP_APP_CAP_BC_MC_SUPPORT)
			ulp_ctx->cfg_data->ulp_flags |=
				BNXT_ULP_APP_BC_MC_SUPPORT;
		if (info[i].flags & BNXT_ULP_APP_CAP_SOCKET_DIRECT) {
			/* Enable socket direction only if MR is enabled in fw*/
			if (BNXT_MR(bp)) {
				ulp_ctx->cfg_data->ulp_flags |=
					BNXT_ULP_APP_SOCKET_DIRECT;
				netdev_dbg(bp->dev,
					   "Socket Direct feature is enabled\n");
			}
		}
		if (info[i].flags & BNXT_ULP_APP_CAP_SRV6)
			ulp_ctx->cfg_data->ulp_flags |=
				BNXT_ULP_APP_SRV6;

		if (info[i].flags & BNXT_ULP_APP_CAP_L2_ETYPE)
			ulp_ctx->cfg_data->ulp_flags |=
				BNXT_ULP_APP_L2_ETYPE;

		if (info[i].flags & BNXT_ULP_APP_CAP_DSCP_REMAP)
			ulp_ctx->cfg_data->ulp_flags |=
				BNXT_ULP_APP_DSCP_REMAP_ENABLED;

		bnxt_ulp_cntxt_vxlan_ip_port_set(ulp_ctx, info[i].vxlan_ip_port);
		bnxt_ulp_cntxt_vxlan_port_set(ulp_ctx, info[i].vxlan_port);
		bnxt_ulp_cntxt_ecpri_udp_port_set(ulp_ctx, info[i].ecpri_udp_port);
		bnxt_ulp_vxlan_gpe_next_proto_set(ulp_ctx, info[i].tunnel_next_proto);
		bnxt_ulp_num_key_recipes_set(ulp_ctx,
					     info[i].num_key_recipes_per_dir);

		/* set the shared session support from firmware */
		fw = info[i].upgrade_fw_update;
		if (ULP_HIGH_AVAIL_IS_ENABLED(ulp_ctx->cfg_data->ulp_flags) &&
		    ulp_tf_multi_shared_session_support_set(bp, dev_id, fw)) {
			netdev_dbg(bp->dev,
				   "Unable to get shared session support\n");
			return -EINVAL;
		}
		ulp_ctx->cfg_data->ha_pool_id = info[i].ha_pool_id;
		bnxt_ulp_default_app_priority_set(ulp_ctx,
						  info[i].default_priority);
		bnxt_ulp_max_def_priority_set(ulp_ctx,
					      info[i].max_def_priority);
		bnxt_ulp_min_flow_priority_set(ulp_ctx,
					       info[i].min_flow_priority);
		bnxt_ulp_max_flow_priority_set(ulp_ctx,
					       info[i].max_flow_priority);
		/* Update the capability feature bits*/
		if (bnxt_ulp_cap_feat_process(info[i].feature_bits,
					      info[i].default_feature_bits,
					      &ulp_ctx->cfg_data->feature_bits))
			return -EINVAL;

		bnxt_ulp_cntxt_ptr2_default_class_bits_set(ulp_ctx,
							   info[i].default_class_bits);
		bnxt_ulp_cntxt_ptr2_default_act_bits_set(ulp_ctx,
							 info[i].default_act_bits);
	}
	if (!found) {
		netdev_dbg(bp->dev, "APP ID %d, Device ID: 0x%x not supported.\n",
			   app_id, dev_id);
		ulp_ctx->cfg_data->ulp_flags |= BNXT_ULP_APP_DEV_UNSUPPORTED;
		return -EINVAL;
	}

	return 0;
}

static inline u32
ulp_tf_session_idx_get(enum bnxt_ulp_session_type session_type) {
	if (session_type & BNXT_ULP_SESSION_TYPE_SHARED)
		return 1;
	else if (session_type & BNXT_ULP_SESSION_TYPE_SHARED_WC)
		return 2;
	return 0;
}

/* Function to set the tfp session details in session */
static int
ulp_tf_session_tfp_set(struct bnxt_ulp_session_state *session,
		       enum bnxt_ulp_session_type session_type,
		       struct tf *tfp)
{
	u32 idx = ulp_tf_session_idx_get(session_type);
	struct tf *local_tfp;
	int rc = 0;

	if (!session->session_opened[idx]) {
		local_tfp = vzalloc(sizeof(*tfp));
		if (!local_tfp)
			return -ENOMEM;
		local_tfp->session = tfp->session;
		session->g_tfp[idx] = local_tfp;
		session->session_opened[idx] = 1;
	}
	return rc;
}

/* Function to get the tfp session details in session */
static struct tf_session_info *
ulp_tf_session_tfp_get(struct bnxt_ulp_session_state *session,
		       enum bnxt_ulp_session_type session_type)
{
	u32 idx = ulp_tf_session_idx_get(session_type);
	struct tf *local_tfp = session->g_tfp[idx];

	if (session->session_opened[idx])
		return local_tfp->session;
	return NULL;
}

static u32
ulp_tf_session_is_open(struct bnxt_ulp_session_state *session,
		       enum bnxt_ulp_session_type session_type)
{
	u32 idx = ulp_tf_session_idx_get(session_type);

	return session->session_opened[idx];
}

/* Function to reset the tfp session details in session */
static void
ulp_tf_session_tfp_reset(struct bnxt_ulp_session_state *session,
			 enum bnxt_ulp_session_type session_type)
{
	u32 idx = ulp_tf_session_idx_get(session_type);

	if (session->session_opened[idx]) {
		session->session_opened[idx] = 0;
		vfree(session->g_tfp[idx]);
		session->g_tfp[idx] = NULL;
	}
}

static void
ulp_tf_ctx_shared_session_close(struct bnxt *bp,
				enum bnxt_ulp_session_type session_type,
				struct bnxt_ulp_session_state *session)
{
	struct tf *tfp;
	int rc;

	tfp = bnxt_tf_ulp_cntxt_tfp_get(bp->ulp_ctx, session_type);
	if (!tfp) {
		/* Log it under debug since this is likely a case of the
		 * shared session not being created.  For example, a failed
		 * initialization.
		 */
		netdev_dbg(bp->dev, "Failed to get shared tfp on close\n");
		return;
	}
	rc = tf_close_session(tfp);
	if (rc)
		netdev_dbg(bp->dev, "Failed to close the shared session rc=%d\n", rc);

	bnxt_tf_ulp_cntxt_tfp_set(bp->ulp_ctx, session_type, NULL);
	ulp_tf_session_tfp_reset(session, session_type);
}

static void
ulp_tf_get_ctrl_chan_name(struct bnxt *bp,
			  struct tf_open_session_parms *params)
{
	struct net_device *dev = bp->dev;

	memset(params->ctrl_chan_name, '\0', TF_SESSION_NAME_MAX);

	if ((strlen(dev_name(dev->dev.parent)) >=
	     strlen(params->ctrl_chan_name)) ||
	    (strlen(dev_name(dev->dev.parent)) >=
	     sizeof(params->ctrl_chan_name))) {
		strncpy(params->ctrl_chan_name, dev_name(dev->dev.parent),
			TF_SESSION_NAME_MAX - 1);
		/* Make sure the string is terminated */
		params->ctrl_chan_name[TF_SESSION_NAME_MAX - 1] = '\0';
		return;
	}

	strcpy(params->ctrl_chan_name, dev_name(dev->dev.parent));
}

static int
ulp_tf_ctx_shared_session_open(struct bnxt *bp,
			       enum bnxt_ulp_session_type session_type,
			       struct bnxt_ulp_session_state *session)
{
	struct bnxt_ulp_context	*ulp_ctx = bp->ulp_ctx;
	uint ulp_dev_id = BNXT_ULP_DEVICE_ID_LAST;
	struct tf_session_resources *resources;
	struct tf_open_session_parms parms;
	struct tf *tfp;
	u8 pool_id;
	int rc = 0;
	size_t nb;
	u8 app_id;

	memset(&parms, 0, sizeof(parms));
	ulp_tf_get_ctrl_chan_name(bp, &parms);

	resources = &parms.resources;

	/* Need to account for size of ctrl_chan_name and 1 extra for Null
	 * terminator
	 */
	nb = sizeof(parms.ctrl_chan_name) - strlen(parms.ctrl_chan_name) - 1;

	/* Build the ctrl_chan_name with shared token.
	 */
	pool_id = ulp_ctx->cfg_data->ha_pool_id;
	if (!bnxt_ulp_cntxt_multi_shared_session_enabled(bp->ulp_ctx)) {
		strncat(parms.ctrl_chan_name, "-tf_shared", nb);
	} else if (bnxt_ulp_cntxt_multi_shared_session_enabled(bp->ulp_ctx)) {
		if (session_type == BNXT_ULP_SESSION_TYPE_SHARED) {
			strncat(parms.ctrl_chan_name, "-tf_shared", nb);
		} else if (session_type == BNXT_ULP_SESSION_TYPE_SHARED_WC) {
			char session_pool_name[64];

			sprintf(session_pool_name, "-tf_shared-pool%d",
				pool_id);

			if (nb >= strlen(session_pool_name)) {
				strncat(parms.ctrl_chan_name, session_pool_name, nb);
			} else {
				netdev_dbg(bp->dev, "No space left for session_name\n");
				return -EINVAL;
			}
		}
	}

	rc = ulp_tf_shared_session_resources_get(bp->ulp_ctx, session_type,
						 resources);
	if (rc) {
		netdev_dbg(bp->dev,
			   "Failed to get shared session resources: %d\n", rc);
		return rc;
	}

	rc = bnxt_ulp_cntxt_app_id_get(bp->ulp_ctx, &app_id);
	if (rc) {
		netdev_dbg(bp->dev, "Unable to get the app id from ulp\n");
		return rc;
	}

	rc = bnxt_ulp_cntxt_dev_id_get(bp->ulp_ctx, &ulp_dev_id);
	if (rc) {
		netdev_dbg(bp->dev, "Unable to get device id from ulp.\n");
		return rc;
	}

	tfp = bnxt_ulp_bp_tfp_get(bp, session_type);
	parms.device_type = bnxt_ulp_cntxt_convert_dev_id(bp, ulp_dev_id);
	parms.bp = bp;

	/* Open the session here, but the collect the resources during the
	 * mapper initialization.
	 */
	rc = tf_open_session(tfp, &parms);
	if (rc)
		return rc;

	if (parms.shared_session_creator)
		netdev_dbg(bp->dev, "Shared session creator\n");
	else
		netdev_dbg(bp->dev, "Shared session attached\n");

	/* Save the shared session in global data */
	rc = ulp_tf_session_tfp_set(session, session_type, tfp);
	if (rc) {
		netdev_dbg(bp->dev, "Failed to add shared tfp to session\n");
		return rc;
	}

	rc = bnxt_tf_ulp_cntxt_tfp_set(bp->ulp_ctx, session_type, tfp);
	if (rc) {
		netdev_dbg(bp->dev,
			   "Failed to add shared tfp to ulp: %d\n", rc);
		return rc;
	}

	return rc;
}

static int
ulp_tf_ctx_shared_session_attach(struct bnxt *bp,
				 struct bnxt_ulp_session_state *ses)
{
	enum bnxt_ulp_session_type type;
	struct tf *tfp;
	int rc = 0;

	/* Simply return success if shared session not enabled */
	if (bnxt_ulp_cntxt_shared_session_enabled(bp->ulp_ctx)) {
		type = BNXT_ULP_SESSION_TYPE_SHARED;
		tfp = bnxt_ulp_bp_tfp_get(bp, type);
		tfp->session = ulp_tf_session_tfp_get(ses, type);
		rc = ulp_tf_ctx_shared_session_open(bp, type, ses);
	}

	if (bnxt_ulp_cntxt_multi_shared_session_enabled(bp->ulp_ctx)) {
		type = BNXT_ULP_SESSION_TYPE_SHARED_WC;
		tfp = bnxt_ulp_bp_tfp_get(bp, type);
		tfp->session = ulp_tf_session_tfp_get(ses, type);
		rc = ulp_tf_ctx_shared_session_open(bp, type, ses);
	}

	if (!rc)
		bnxt_ulp_cntxt_num_shared_clients_set(bp->ulp_ctx, true);

	return rc;
}

static void
ulp_tf_ctx_shared_session_detach(struct bnxt *bp)
{
	struct tf *tfp;

	if (bnxt_ulp_cntxt_shared_session_enabled(bp->ulp_ctx)) {
		tfp = bnxt_ulp_bp_tfp_get(bp, BNXT_ULP_SESSION_TYPE_SHARED);
		if (tfp->session) {
			tf_close_session(tfp);
			tfp->session = NULL;
		}
	}
	if (bnxt_ulp_cntxt_multi_shared_session_enabled(bp->ulp_ctx)) {
		tfp = bnxt_ulp_bp_tfp_get(bp, BNXT_ULP_SESSION_TYPE_SHARED_WC);
		if (tfp->session) {
			tf_close_session(tfp);
			tfp->session = NULL;
		}
	}
	bnxt_ulp_cntxt_num_shared_clients_set(bp->ulp_ctx, false);
}

/* Initialize an ULP session.
 * An ULP session will contain all the resources needed to support flow
 * offloads. A session is initialized as part of switchdev mode transition.
 * A single vswitch instance can have multiple uplinks which means
 * switchdev mode transitino will be called for each of these devices.
 * ULP session manager will make sure that a single ULP session is only
 * initialized once. Apart from this, it also initializes MARK database,
 * EEM table & flow database. ULP session manager also manages a list of
 * all opened ULP sessions.
 */
static int
ulp_tf_ctx_session_open(struct bnxt *bp,
			struct bnxt_ulp_session_state *session)
{
	u32 ulp_dev_id = BNXT_ULP_DEVICE_ID_LAST;
	struct tf_session_resources *resources;
	struct tf_open_session_parms params = {{ 0 }};
	struct net_device *dev = bp->dev;
	struct tf *tfp;
	int rc = 0;
	u8 app_id;

	memset(&params, 0, sizeof(params));
	memset(params.ctrl_chan_name, '\0', TF_SESSION_NAME_MAX);
	if ((strlen(dev_name(dev->dev.parent)) >= strlen(params.ctrl_chan_name)) ||
	    (strlen(dev_name(dev->dev.parent)) >= sizeof(params.ctrl_chan_name))) {
		strncpy(params.ctrl_chan_name, dev_name(dev->dev.parent), TF_SESSION_NAME_MAX - 1);
		/* Make sure the string is terminated */
		params.ctrl_chan_name[TF_SESSION_NAME_MAX - 1] = '\0';
	} else {
		strcpy(params.ctrl_chan_name, dev_name(dev->dev.parent));
	}

	rc = bnxt_ulp_cntxt_app_id_get(bp->ulp_ctx, &app_id);
	if (rc) {
		netdev_dbg(bp->dev, "Unable to get the app id from ulp.\n");
		return -EINVAL;
	}

	rc = bnxt_ulp_cntxt_dev_id_get(bp->ulp_ctx, &ulp_dev_id);
	if (rc) {
		netdev_dbg(bp->dev, "Unable to get device id from ulp.\n");
		return rc;
	}

	params.device_type = bnxt_ulp_cntxt_convert_dev_id(bp, ulp_dev_id);
	resources = &params.resources;
	rc = ulp_tf_resources_get(bp->ulp_ctx,
				  BNXT_ULP_SESSION_TYPE_DEFAULT,
				  resources);
	if (rc)
		return rc;

	params.bp = bp;

	tfp = bnxt_ulp_bp_tfp_get(bp, BNXT_ULP_SESSION_TYPE_DEFAULT);
	rc = tf_open_session(tfp, &params);
	if (rc) {
		netdev_dbg(bp->dev, "Failed to open TF session - %s, rc = %d\n",
			   params.ctrl_chan_name, rc);
		return -EINVAL;
	}
	rc = ulp_tf_session_tfp_set(session, BNXT_ULP_SESSION_TYPE_DEFAULT, tfp);
	if (rc) {
		netdev_dbg(bp->dev, "Failed to set TF session - %s, rc = %d\n",
			   params.ctrl_chan_name, rc);
		return -EINVAL;
	}
	return rc;
}

/* Close the ULP session.
 * It takes the ulp context pointer.
 */
static void ulp_tf_ctx_session_close(struct bnxt *bp,
				     struct bnxt_ulp_session_state *session)
{
	struct tf *tfp;

	/* close the session in the hardware */
	if (ulp_tf_session_is_open(session, BNXT_ULP_SESSION_TYPE_DEFAULT)) {
		tfp = bnxt_ulp_bp_tfp_get(bp, BNXT_ULP_SESSION_TYPE_DEFAULT);
		tf_close_session(tfp);
	}
	ulp_tf_session_tfp_reset(session, BNXT_ULP_SESSION_TYPE_DEFAULT);
}

static void
ulp_tf_init_tbl_scope_parms(struct bnxt *bp,
			    struct tf_alloc_tbl_scope_parms *params)
{
	struct bnxt_ulp_device_params *dparms;
	u32 dev_id;
	int rc = 0;

	rc = bnxt_ulp_cntxt_dev_id_get(bp->ulp_ctx, &dev_id);
	if (rc)
		/* TBD: For now, just use default. */
		dparms = 0;
	else
		dparms = bnxt_ulp_device_params_get(dev_id);

	/* Set the flush timer for EEM entries. The value is in 100ms intervals,
	 * so 100 is 10s.
	 */
	params->hw_flow_cache_flush_timer = 100;

	if (!dparms) {
		params->rx_max_key_sz_in_bits = BNXT_ULP_DFLT_RX_MAX_KEY;
		params->rx_max_action_entry_sz_in_bits =
			BNXT_ULP_DFLT_RX_MAX_ACTN_ENTRY;
		params->rx_mem_size_in_mb = BNXT_ULP_DFLT_RX_MEM;
		params->rx_num_flows_in_k = BNXT_ULP_RX_NUM_FLOWS;

		params->tx_max_key_sz_in_bits = BNXT_ULP_DFLT_TX_MAX_KEY;
		params->tx_max_action_entry_sz_in_bits =
			BNXT_ULP_DFLT_TX_MAX_ACTN_ENTRY;
		params->tx_mem_size_in_mb = BNXT_ULP_DFLT_TX_MEM;
		params->tx_num_flows_in_k = BNXT_ULP_TX_NUM_FLOWS;
	} else {
		params->rx_max_key_sz_in_bits = BNXT_ULP_DFLT_RX_MAX_KEY;
		params->rx_max_action_entry_sz_in_bits =
			BNXT_ULP_DFLT_RX_MAX_ACTN_ENTRY;
		params->rx_mem_size_in_mb = BNXT_ULP_DFLT_RX_MEM;
		params->rx_num_flows_in_k =
			dparms->ext_flow_db_num_entries / 1024;

		params->tx_max_key_sz_in_bits = BNXT_ULP_DFLT_TX_MAX_KEY;
		params->tx_max_action_entry_sz_in_bits =
			BNXT_ULP_DFLT_TX_MAX_ACTN_ENTRY;
		params->tx_mem_size_in_mb = BNXT_ULP_DFLT_TX_MEM;
		params->tx_num_flows_in_k =
			dparms->ext_flow_db_num_entries / 1024;
	}
	netdev_dbg(bp->dev, "Table Scope initialized with %uK flows.\n",
		   params->rx_num_flows_in_k);
}

/* Initialize Extended Exact Match host memory. */
static int
ulp_tf_eem_tbl_scope_init(struct bnxt *bp)
{
	struct tf_alloc_tbl_scope_parms params = {0};
	struct bnxt_ulp_device_params *dparms;
	enum bnxt_ulp_flow_mem_type mtype;
	struct tf *tfp = NULL;
	u32 dev_id;
	int rc = 0;

	/* Get the dev specific number of flows that needed to be supported. */
	if (bnxt_ulp_cntxt_dev_id_get(bp->ulp_ctx, &dev_id)) {
		netdev_dbg(bp->dev, "Invalid device id\n");
		return -EINVAL;
	}

	dparms = bnxt_ulp_device_params_get(dev_id);
	if (!dparms) {
		netdev_dbg(bp->dev, "could not fetch the device params\n");
		return -ENODEV;
	}

	if (bnxt_ulp_cntxt_mem_type_get(bp->ulp_ctx, &mtype))
		return -EINVAL;
	if (mtype != BNXT_ULP_FLOW_MEM_TYPE_EXT) {
		netdev_dbg(bp->dev, "Table Scope alloc is not required\n");
		return 0;
	}

	ulp_tf_init_tbl_scope_parms(bp, &params);
	tfp = bnxt_ulp_bp_tfp_get(bp, BNXT_ULP_SESSION_TYPE_DEFAULT);
	rc = tf_alloc_tbl_scope(tfp, &params);
	if (rc) {
		netdev_dbg(bp->dev, "Unable to allocate eem table scope rc = %d\n", rc);
		return rc;
	}

	netdev_dbg(bp->dev, "TableScope=0x%0x %d\n",
		   params.tbl_scope_id,
		   params.tbl_scope_id);

	rc = bnxt_ulp_cntxt_tbl_scope_id_set(bp->ulp_ctx, params.tbl_scope_id);
	if (rc) {
		netdev_dbg(bp->dev, "Unable to set table scope id\n");
		return rc;
	}

	return 0;
}

/* Free Extended Exact Match host memory */
static int
ulp_tf_eem_tbl_scope_deinit(struct bnxt *bp, struct bnxt_ulp_context *ulp_ctx)
{
	struct tf_free_tbl_scope_parms params = { 0 };
	struct bnxt_ulp_device_params *dparms;
	enum bnxt_ulp_flow_mem_type mtype;
	struct tf *tfp;
	u32 dev_id;
	int rc = 0;

	if (!ulp_ctx || !ulp_ctx->cfg_data)
		return -EINVAL;

	tfp = bnxt_tf_ulp_cntxt_tfp_get(ulp_ctx, BNXT_ULP_SESSION_TYPE_DEFAULT);
	if (!tfp) {
		netdev_dbg(bp->dev, "Failed to get the truflow pointer\n");
		return -EINVAL;
	}

	/* Get the dev specific number of flows that needed to be supported. */
	if (bnxt_ulp_cntxt_dev_id_get(bp->ulp_ctx, &dev_id)) {
		netdev_dbg(bp->dev, "Unable to get the dev id from ulp.\n");
		return -EINVAL;
	}

	dparms = bnxt_ulp_device_params_get(dev_id);
	if (!dparms) {
		netdev_dbg(bp->dev, "could not fetch the device params\n");
		return -ENODEV;
	}

	if (bnxt_ulp_cntxt_mem_type_get(ulp_ctx, &mtype))
		return -EINVAL;
	if (mtype != BNXT_ULP_FLOW_MEM_TYPE_EXT) {
		netdev_dbg(bp->dev, "Table Scope free is not required\n");
		return 0;
	}

	rc = bnxt_ulp_cntxt_tbl_scope_id_get(ulp_ctx, &params.tbl_scope_id);
	if (rc) {
		netdev_dbg(bp->dev, "Failed to get the table scope id\n");
		return -EINVAL;
	}

	rc = tf_free_tbl_scope(tfp, &params);
	if (rc) {
		netdev_dbg(bp->dev, "Unable to free table scope\n");
		return -EINVAL;
	}
	return rc;
}

/* The function to free and deinit the ulp context data. */
static int
ulp_tf_ctx_deinit(struct bnxt *bp,
		  struct bnxt_ulp_session_state *session)
{
	struct bnxt_ulp_context *ulp_ctx = bp->ulp_ctx;

	/* close the tf session */
	ulp_tf_ctx_session_close(bp, session);

	/* The shared session must be closed last. */
	if (bnxt_ulp_cntxt_shared_session_enabled(bp->ulp_ctx))
		ulp_tf_ctx_shared_session_close(bp, BNXT_ULP_SESSION_TYPE_SHARED,
						session);

	if (bnxt_ulp_cntxt_multi_shared_session_enabled(bp->ulp_ctx))
		ulp_tf_ctx_shared_session_close(bp,
						BNXT_ULP_SESSION_TYPE_SHARED_WC,
						session);

	bnxt_ulp_cntxt_num_shared_clients_set(bp->ulp_ctx, false);

	/* Free the contents */
	vfree(session->cfg_data);
	ulp_ctx->cfg_data = NULL;
	session->cfg_data = NULL;
	return 0;
}

/* The function to allocate and initialize the ulp context data. */
static int
ulp_tf_ctx_init(struct bnxt *bp,
		struct bnxt_ulp_session_state *session)
{
	struct bnxt_ulp_context *ulp_ctx = bp->ulp_ctx;
	enum bnxt_ulp_session_type stype;
	struct bnxt_ulp_data *ulp_data;
	enum bnxt_ulp_device_id devid;
	struct tf *tfp;
	u8 app_id = 0;
	int rc = 0;

	/* Initialize the context entries list */
	bnxt_ulp_cntxt_list_init();

	/* Allocate memory to hold ulp context data. */
	ulp_data = vzalloc(sizeof(*ulp_data));
	if (!ulp_data)
		goto error_deinit;

	/* Increment the ulp context data reference count usage. */
	ulp_ctx->cfg_data = ulp_data;
	session->cfg_data = ulp_data;
	ulp_data->ref_cnt++;
	ulp_data->ulp_flags |= BNXT_ULP_VF_REP_ENABLED;

	/* Add the context to the context entries list */
	rc = bnxt_ulp_cntxt_list_add(ulp_ctx);
	if (rc) {
		netdev_dbg(bp->dev, "Failed to add the context list entry\n");
		goto error_deinit;
	}

	rc = bnxt_ulp_devid_get(bp, &devid);
	if (rc) {
		netdev_dbg(bp->dev, "Unable to get the dev id from ulp.\n");
		goto error_deinit;
	}

	rc = bnxt_ulp_cntxt_dev_id_set(bp->ulp_ctx, devid);
	if (rc) {
		netdev_dbg(bp->dev, "Unable to set device for ULP init.\n");
		goto error_deinit;
	}

	if (!(bp->app_id & BNXT_ULP_APP_ID_SET_CONFIGURED)) {
		bp->app_id = BNXT_ULP_APP_ID_CONFIG;
		bp->app_id |= BNXT_ULP_APP_ID_SET_CONFIGURED;
	}
	app_id = bp->app_id & ~BNXT_ULP_APP_ID_SET_CONFIGURED;

	rc = bnxt_ulp_cntxt_app_id_set(ulp_ctx, app_id);
	if (rc) {
		netdev_dbg(bp->dev, "Unable to set app_id for ULP init.\n");
		goto error_deinit;
	}

	rc = ulp_tf_cntxt_app_caps_init(bp, app_id, devid);
	if (rc) {
		netdev_dbg(bp->dev, "Unable to set caps for app(%x)/dev(%x)\n",
			   app_id, devid);
		goto error_deinit;
	}

	/* Shared session must be created before regular
	 * session but after the ulp_ctx is valid.
	 */
	if (bnxt_ulp_cntxt_shared_session_enabled(ulp_ctx)) {
		rc = ulp_tf_ctx_shared_session_open(bp, BNXT_ULP_SESSION_TYPE_SHARED, session);
		if (rc) {
			netdev_dbg(bp->dev,
				   "Unable to open shared session: %d\n", rc);
			goto error_deinit;
		}
	}

	/* Multiple session support */
	if (bnxt_ulp_cntxt_multi_shared_session_enabled(bp->ulp_ctx)) {
		stype = BNXT_ULP_SESSION_TYPE_SHARED_WC;
		rc = ulp_tf_ctx_shared_session_open(bp, stype, session);
		if (rc) {
			netdev_dbg(bp->dev,
				   "Unable to open shared wc session (%d)\n",
				   rc);
			goto error_deinit;
		}
	}
	bnxt_ulp_cntxt_num_shared_clients_set(ulp_ctx, true);

	/* Open the ulp session. */
	rc = ulp_tf_ctx_session_open(bp, session);
	if (rc)
		goto error_deinit;

	tfp = bnxt_ulp_bp_tfp_get(bp, BNXT_ULP_SESSION_TYPE_DEFAULT);
	bnxt_tf_ulp_cntxt_tfp_set(ulp_ctx, BNXT_ULP_SESSION_TYPE_DEFAULT, tfp);

	return rc;

error_deinit:
	session->session_opened[BNXT_ULP_SESSION_TYPE_DEFAULT] = 1;
	(void)ulp_tf_ctx_deinit(bp, session);
	return rc;
}

/* The function to initialize ulp dparms with devargs */
static int
ulp_tf_dparms_init(struct bnxt *bp, struct bnxt_ulp_context *ulp_ctx)
{
	struct bnxt_ulp_device_params *dparms;
	u32 dev_id = BNXT_ULP_DEVICE_ID_LAST;

	if (!bp->max_num_kflows) {
		/* Defaults to Internal */
		bnxt_ulp_cntxt_mem_type_set(ulp_ctx,
					    BNXT_ULP_FLOW_MEM_TYPE_INT);
		return 0;
	}

	/* The max_num_kflows were set, so move to external */
	if (bnxt_ulp_cntxt_mem_type_set(ulp_ctx, BNXT_ULP_FLOW_MEM_TYPE_EXT))
		return -EINVAL;

	if (bnxt_ulp_cntxt_dev_id_get(ulp_ctx, &dev_id)) {
		netdev_dbg(bp->dev, "Failed to get device id\n");
		return -EINVAL;
	}

	dparms = bnxt_ulp_device_params_get(dev_id);
	if (!dparms) {
		netdev_dbg(bp->dev, "Failed to get device parms\n");
		return -EINVAL;
	}

	/* num_flows = max_num_kflows * 1024 */
	dparms->ext_flow_db_num_entries = bp->max_num_kflows * 1024;
	/* GFID =  2 * num_flows */
	dparms->mark_db_gfid_entries = dparms->ext_flow_db_num_entries * 2;
	netdev_dbg(bp->dev, "Set the number of flows = %lld\n", dparms->ext_flow_db_num_entries);

	return 0;
}

static int
ulp_tf_ctx_attach(struct bnxt *bp,
		  struct bnxt_ulp_session_state *session,
		  enum cfa_app_type app_type)
{
	struct bnxt_ulp_context *ulp_ctx = bp->ulp_ctx;
	u32 flags, dev_id = BNXT_ULP_DEVICE_ID_LAST;
	bool dscp_insert = false;
	struct tf *tfp;
	int rc = 0;
	u8 app_id;

	/* Increment the ulp context data reference count usage. */
	ulp_ctx->cfg_data = session->cfg_data;
	ulp_ctx->cfg_data->ref_cnt++;

	/* update the session details in bnxt tfp */
	tfp = bnxt_ulp_bp_tfp_get(bp, BNXT_ULP_SESSION_TYPE_DEFAULT);
	if (!tfp) {
		netdev_dbg(bp->dev, "Failed to get tfp entry\n");
		return -EINVAL;
	}
	tfp->session = ulp_tf_session_tfp_get(session, BNXT_ULP_SESSION_TYPE_DEFAULT);

	/* Add the context to the context entries list */
	rc = bnxt_ulp_cntxt_list_add(ulp_ctx);
	if (rc) {
		netdev_dbg(bp->dev, "Failed to add the context list entry\n");
		return -EINVAL;
	}

	/* The supported flag will be set during the init. Use it now to
	 * know if we should go through the attach.
	 */
	rc = bnxt_ulp_cntxt_app_id_get(ulp_ctx, &app_id);
	if (rc) {
		netdev_dbg(bp->dev, "Unable to get the app id from ulp.\n");
		return -EINVAL;
	}

	rc = bnxt_ulp_devid_get(bp, &dev_id);
	if (rc) {
		netdev_dbg(bp->dev, "Unable to get the dev id from ulp.\n");
		return -EINVAL;
	}

	flags = ulp_ctx->cfg_data->ulp_flags;
	if (ULP_APP_DEV_UNSUPPORTED_ENABLED(flags)) {
		netdev_dbg(bp->dev,
			   "%s: APP ID %d, Device ID: 0x%x not supported.\n",
			   __func__, app_id, dev_id);
		return -EINVAL;
	}

	/* Create a TF Client */
	rc = ulp_tf_ctx_session_open(bp, session);
	if (rc) {
		netdev_dbg(bp->dev, "Failed to open ctxt session, rc:%d\n", rc);
		tfp->session = NULL;
		return rc;
	}
	tfp = bnxt_ulp_bp_tfp_get(bp, BNXT_ULP_SESSION_TYPE_DEFAULT);
	bnxt_tf_ulp_cntxt_tfp_set(ulp_ctx, BNXT_ULP_SESSION_TYPE_DEFAULT, tfp);

	/* Attach to the shared session, must be called after the
	 * ulp_ctx_attach in order to ensure that ulp data is available
	 * for attaching.
	 */
	rc = ulp_tf_ctx_shared_session_attach(bp, session);
	if (rc) {
		netdev_dbg(bp->dev,
			   "Failed attach to shared session: %d\n",
			   rc);
	}

	/* Changing sriov_dscp_insert nvm config while TF is
	 * already running on another port is not allowed.
	 */
	bnxt_hwrm_get_sriov_dscp_insert(bp, -1, &dscp_insert);
	if (ulp_ctx->cfg_data->dscp_remap.sriov_dscp_insert !=
	    dscp_insert) {
		netdev_warn(bp->dev,
			    "Inconsistent DSCP insert mode: %d\n",
			    dscp_insert);
		return -EINVAL;
	}
	return rc;
}

static void
ulp_tf_ctx_detach(struct bnxt *bp,
		  struct bnxt_ulp_session_state *session)
{
	struct tf *tfp;

	tfp = bnxt_ulp_bp_tfp_get(bp, BNXT_ULP_SESSION_TYPE_DEFAULT);
	if (tfp && tfp->session) {
		tf_close_session(tfp);
		tfp->session = NULL;
	}

	/* always detach/close shared after the session. */
	ulp_tf_ctx_shared_session_detach(bp);
}

/* Internal api to enable NAT feature.
 * Set set_flag to 1 to set the value or zero to reset the value.
 * returns 0 on success.
 */
static int
ulp_tf_global_cfg_update(struct bnxt *bp,
			 enum tf_dir dir,
			 enum tf_global_config_type type,
			 u32 offset,
			 u32 value,
			 u32 set_flag)
{
	struct tf_global_cfg_parms parms = { 0 };
	u32 global_cfg = 0;
	int rc;

	/* Initialize the params */
	parms.dir = dir,
	parms.type = type,
	parms.offset = offset,
	parms.config = (u8 *)&global_cfg,
	parms.config_sz_in_bytes = sizeof(global_cfg);

	rc = tf_get_global_cfg(bp->tfp, &parms);
	if (rc) {
		netdev_dbg(bp->dev, "Failed to get global cfg 0x%x rc:%d\n",
			   type, rc);
		return rc;
	}

	if (set_flag)
		global_cfg |= value;
	else
		global_cfg &= ~value;

	/* SET the register RE_CFA_REG_ACT_TECT */
	rc = tf_set_global_cfg(bp->tfp, &parms);
	if (rc) {
		netdev_dbg(bp->dev, "Failed to set global cfg 0x%x rc:%d\n",
			   type, rc);
		return rc;
	}
	return rc;
}

/*
 * When a port is deinit'ed by dpdk. This function is called
 * and this function clears the ULP context and rest of the
 * infrastructure associated with it.
 */
static void
ulp_tf_deinit(struct bnxt *bp,
	      struct bnxt_ulp_session_state *session)
{
	struct bnxt_ulp_context *ulp_ctx = bp->ulp_ctx;

	if (!bp->ulp_ctx || !ulp_ctx->cfg_data)
		return;

	bnxt_tc_uninit_dscp_remap(bp, &ulp_ctx->cfg_data->dscp_remap,
				  BNXT_ULP_FLOW_ATTR_EGRESS);

	/* cleanup the eem table scope */
	ulp_tf_eem_tbl_scope_deinit(bp, ulp_ctx);

	/* cleanup the flow database */
	ulp_flow_db_deinit(ulp_ctx);

	/* Delete the Mark database */
	ulp_mark_db_deinit(ulp_ctx);

	/* cleanup the ulp mapper */
	ulp_mapper_deinit(ulp_ctx);

	/* cleanup the ulp matcher */
	ulp_matcher_deinit(ulp_ctx);

	/* Delete the Flow Counter Manager */
	ulp_fc_mgr_deinit(ulp_ctx);

	/* Delete the Port database */
	ulp_port_db_deinit(ulp_ctx);

	/* Disable NAT feature */
	(void)ulp_tf_global_cfg_update(bp, TF_DIR_RX, TF_TUNNEL_ENCAP,
					 TF_TUNNEL_ENCAP_NAT,
					 BNXT_ULP_NAT_OUTER_MOST_FLAGS, 0);

	(void)ulp_tf_global_cfg_update(bp, TF_DIR_TX, TF_TUNNEL_ENCAP,
					 TF_TUNNEL_ENCAP_NAT,
					 BNXT_ULP_NAT_OUTER_MOST_FLAGS, 0);

	/* Disable meter feature */
	(void)bnxt_flow_meter_deinit(bp);

	/* free the flow db lock */
	mutex_destroy(&ulp_ctx->cfg_data->flow_db_lock);

	/* Delete the ulp context and tf session and free the ulp context */
	ulp_tf_ctx_deinit(bp, session);
	netdev_dbg(bp->dev, "ulp ctx has been deinitialized\n");
}

static void
ulp_dscp_remap_init(struct bnxt *bp)
{
	struct bnxt_ulp_context *ulp_ctx = bp->ulp_ctx;
	bool dscp_insert = false;

	if (!ulp_ctx->cfg_data->meter_initialized)
		return;

	/* func_qcfg read during the driver load for PF can be stale,
	 * because the nvm configuration can be updated after the driver
	 * load. So we need to read this flag again and update our
	 * own db for PF case only. For VFs we still depend on the
	 * bp->fw_cap_ext to insert dscp or not in the packets.
	 */
	bnxt_hwrm_get_sriov_dscp_insert(bp, -1, &dscp_insert);
	ulp_ctx->cfg_data->dscp_remap.sriov_dscp_insert = dscp_insert;

	/* Initialization of DSCP Remap framework needs to be done after
	 * reading the sriov_dscp_insert capability. Also note that this
	 * capability is per card; so the capability should be enabled
	 * in nvm before TF is initialized on any PF. This initialization
	 * will not be done when TF is initialized on subsequent PFs.
	 */
	bnxt_tc_init_dscp_remap(bp, &ulp_ctx->cfg_data->dscp_remap,
				BNXT_ULP_FLOW_ATTR_EGRESS);
}

/*
 * When a port is initialized by dpdk. This functions is called
 * and this function initializes the ULP context and rest of the
 * infrastructure associated with it.
 */
static int32_t
ulp_tf_init(struct bnxt *bp,
	    struct bnxt_ulp_session_state *session,
	    enum cfa_app_type app_type)
{
	struct bnxt_ulp_context *ulp_ctx = bp->ulp_ctx;
	u32 ulp_dev_id = BNXT_ULP_DEVICE_ID_LAST;
	int rc;

	if (!bp->tfp)
		return -ENOMEM;

	/* Allocate and Initialize the ulp context. */
	rc = ulp_tf_ctx_init(bp, session);
	if (rc) {
		netdev_dbg(bp->dev, "Failed to create the ulp context\n");
		goto jump_to_error;
	}

	mutex_init(&ulp_ctx->cfg_data->flow_db_lock);

	/* Defaults to Internal */
	rc = bnxt_ulp_cntxt_mem_type_set(ulp_ctx, BNXT_ULP_FLOW_MEM_TYPE_INT);
	if (rc) {
		netdev_dbg(bp->dev, "Failed to write mem_type in ulp ctxt\n");
		goto jump_to_error;
	}

	/* Initialize ulp dparms with values devargs passed */
	rc = ulp_tf_dparms_init(bp, bp->ulp_ctx);
	if (rc) {
		netdev_dbg(bp->dev, "Failed to initialize the dparms\n");
		goto jump_to_error;
	}

	/* create the port database */
	rc = ulp_port_db_init(ulp_ctx, bp->port_count);
	if (rc) {
		netdev_dbg(bp->dev, "Failed to create the port database\n");
		goto jump_to_error;
	}

	/* Create the Mark database. */
	rc = ulp_mark_db_init(ulp_ctx);
	if (rc) {
		netdev_dbg(bp->dev, "Failed to create the mark database\n");
		goto jump_to_error;
	}

	/* Create the flow database. */
	rc = ulp_flow_db_init(ulp_ctx);
	if (rc) {
		netdev_dbg(bp->dev, "Failed to create the flow database\n");
		goto jump_to_error;
	}

	/* Create the eem table scope. */
	rc = ulp_tf_eem_tbl_scope_init(bp);
	if (rc) {
		netdev_dbg(bp->dev, "Failed to create the eem scope table\n");
		goto jump_to_error;
	}

	rc = ulp_matcher_init(ulp_ctx);
	if (rc) {
		netdev_dbg(bp->dev, "Failed to initialize ulp matcher\n");
		goto jump_to_error;
	}

	rc = ulp_mapper_init(ulp_ctx);
	if (rc) {
		netdev_dbg(bp->dev, "Failed to initialize ulp mapper\n");
		goto jump_to_error;
	}

	rc = ulp_fc_mgr_init(ulp_ctx);
	if (rc) {
		netdev_dbg(bp->dev, "Failed to initialize ulp flow counter mgr\n");
		goto jump_to_error;
	}

	/* Enable NAT feature. Set the global configuration register
	 * Tunnel encap to enable NAT with the reuse of existing inner
	 * L2 header smac and dmac
	 */
	rc = ulp_tf_global_cfg_update(bp, TF_DIR_RX, TF_TUNNEL_ENCAP,
				      TF_TUNNEL_ENCAP_NAT,
				      BNXT_ULP_NAT_OUTER_MOST_FLAGS, 1);
	if (rc) {
		netdev_dbg(bp->dev, "Failed to set rx global configuration\n");
		goto jump_to_error;
	}

	rc = ulp_tf_global_cfg_update(bp, TF_DIR_TX, TF_TUNNEL_ENCAP,
				      TF_TUNNEL_ENCAP_NAT,
				      BNXT_ULP_NAT_OUTER_MOST_FLAGS, 1);
	if (rc) {
		netdev_dbg(bp->dev, "Failed to set tx global configuration\n");
		goto jump_to_error;
	}

	rc = bnxt_ulp_cntxt_dev_id_get(bp->ulp_ctx, &ulp_dev_id);
	if (rc) {
		netdev_dbg(bp->dev, "Unable to get device id from ulp.\n");
		return rc;
	}

	rc = bnxt_flow_meter_init(bp);
	if (rc) {
		if (rc != -EOPNOTSUPP) {
			netdev_err(bp->dev, "Failed to config meter\n");
			goto jump_to_error;
		}
		rc = 0;
	}

	ulp_dscp_remap_init(bp);

	netdev_dbg(bp->dev, "ulp ctx has been initialized\n");
	return rc;

jump_to_error:
	((struct bnxt_ulp_context *)bp->ulp_ctx)->ops->ulp_deinit(bp, session);
	return rc;
}

const struct bnxt_ulp_core_ops bnxt_ulp_tf_core_ops = {
	.ulp_ctx_attach = ulp_tf_ctx_attach,
	.ulp_ctx_detach = ulp_tf_ctx_detach,
	.ulp_deinit =  ulp_tf_deinit,
	.ulp_init =  ulp_tf_init,
	.ulp_tfp_get = bnxt_tf_ulp_cntxt_tfp_get,
	.ulp_vfr_session_fid_add = NULL,
	.ulp_vfr_session_fid_rem = NULL,
};
#endif /* CONFIG_BNXT_FLOWER_OFFLOAD */
