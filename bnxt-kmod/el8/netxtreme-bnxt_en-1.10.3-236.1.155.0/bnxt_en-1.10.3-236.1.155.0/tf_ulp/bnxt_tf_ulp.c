// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#include "ulp_linux.h"
#include "bnxt_compat.h"
#include "bnxt_hsi.h"
#include "bnxt.h"
#include "bnxt_vfr.h"
#include "bnxt_tf_ulp.h"
#include "bnxt_ulp_flow.h"
#include "bnxt_tf_common.h"
#include "bnxt_tfc.h"
#include "tf_core.h"
#include "tfc.h"
#include "tf_ext_flow_handle.h"

#include "ulp_template_db_enum.h"
#include "ulp_template_struct.h"
#include "ulp_mark_mgr.h"
#include "ulp_fc_mgr.h"
#include "ulp_flow_db.h"
#include "ulp_mapper.h"
#include "ulp_matcher.h"
#include "ulp_port_db.h"
#include "bnxt_tfc.h"
#include "bnxt_nic_flow.h"
#include "bnxt_ktls.h"

#if defined(CONFIG_BNXT_FLOWER_OFFLOAD) || defined(CONFIG_BNXT_CUSTOM_FLOWER_OFFLOAD)
/* Linked list of all TF sessions. */
HLIST_HEAD(bnxt_ulp_session_list);
/* Mutex to synchronize bnxt_ulp_session_list operations. */
DEFINE_MUTEX(bnxt_ulp_global_mutex);

/* Spin lock to protect context global list */
u32 bnxt_ulp_ctxt_lock_created;
DEFINE_MUTEX(bnxt_ulp_ctxt_lock);
static HLIST_HEAD(ulp_cntx_list);

/* Allow the deletion of context only for the bnxt device that
 * created the session.
 */
bool
ulp_ctx_deinit_allowed(struct bnxt_ulp_context *ulp_ctx)
{
	if (!ulp_ctx || !ulp_ctx->cfg_data)
		return false;

	if (!ulp_ctx->cfg_data->ref_cnt) {
		netdev_dbg(ulp_ctx->bp->dev, "ulp ctx shall initiate deinit\n");
		return true;
	}

	return false;
}

int
bnxt_ulp_devid_get(struct bnxt *bp,
		   enum bnxt_ulp_device_id  *ulp_dev_id)
{
	if (BNXT_CHIP_P7(bp))
		*ulp_dev_id = BNXT_ULP_DEVICE_ID_THOR2;
	else if (BNXT_CHIP_P5(bp))
		*ulp_dev_id = BNXT_ULP_DEVICE_ID_THOR;
	else if (BNXT_CHIP_P4(bp))
		*ulp_dev_id = BNXT_ULP_DEVICE_ID_WH_PLUS;
	else
		return -ENODEV;

	return 0;
}

struct bnxt_ulp_app_capabilities_info *
bnxt_ulp_app_cap_list_get(u32 *num_entries)
{
	if (!num_entries)
		return NULL;
	*num_entries = BNXT_ULP_APP_CAP_TBL_MAX_SZ;
	return ulp_app_cap_info_list;
}

struct bnxt_ulp_resource_resv_info *
bnxt_ulp_resource_resv_list_get(u32 *num_entries)
{
	if (!num_entries)
		return NULL;
	*num_entries = BNXT_ULP_RESOURCE_RESV_LIST_MAX_SZ;
	return ulp_resource_resv_list;
}

struct bnxt_ulp_resource_resv_info *
bnxt_ulp_app_resource_resv_list_get(u32 *num_entries)
{
	if (!num_entries)
		return NULL;
	*num_entries = BNXT_ULP_APP_RESOURCE_RESV_LIST_MAX_SZ;
	return ulp_app_resource_resv_list;
}

struct bnxt_ulp_glb_resource_info *
bnxt_ulp_app_glb_resource_info_list_get(u32 *num_entries)
{
	if (!num_entries)
		return NULL;
	*num_entries = BNXT_ULP_APP_GLB_RESOURCE_TBL_MAX_SZ;
	return ulp_app_glb_resource_tbl;
}

/* Function to set the number for vxlan_ip (custom vxlan) port into the context */
int
bnxt_ulp_cntxt_ecpri_udp_port_set(struct bnxt_ulp_context *ulp_ctx,
				  u32 ecpri_udp_port)
{
	if (!ulp_ctx || !ulp_ctx->cfg_data)
		return -EINVAL;

	ulp_ctx->cfg_data->ecpri_udp_port = ecpri_udp_port;

	return 0;
}

/* Function to retrieve the vxlan_ip (custom vxlan) port from the context. */
unsigned int
bnxt_ulp_cntxt_ecpri_udp_port_get(struct bnxt_ulp_context *ulp_ctx)
{
	if (!ulp_ctx || !ulp_ctx->cfg_data)
		return 0;

	return (unsigned int)ulp_ctx->cfg_data->ecpri_udp_port;
}

/* Function to set the number for vxlan_ip (custom vxlan) port into the context */
int
bnxt_ulp_cntxt_vxlan_ip_port_set(struct bnxt_ulp_context *ulp_ctx,
				 u32 vxlan_ip_port)
{
	if (!ulp_ctx || !ulp_ctx->cfg_data)
		return -EINVAL;

	ulp_ctx->cfg_data->vxlan_ip_port = vxlan_ip_port;
	if (vxlan_ip_port)
		ulp_ctx->cfg_data->ulp_flags |= BNXT_ULP_STATIC_VXLAN_SUPPORT;

	return 0;
}

/* Function to retrieve the vxlan_ip (custom vxlan) port from the context. */
unsigned int
bnxt_ulp_cntxt_vxlan_ip_port_get(struct bnxt_ulp_context *ulp_ctx)
{
	if (!ulp_ctx || !ulp_ctx->cfg_data)
		return 0;

	return (unsigned int)ulp_ctx->cfg_data->vxlan_ip_port;
}

/* Function to set the number for vxlan_gpe next_proto into the context */
u32
bnxt_ulp_vxlan_gpe_next_proto_set(struct bnxt_ulp_context *ulp_ctx,
				  u8 tunnel_next_proto)
{
	if (!ulp_ctx || !ulp_ctx->cfg_data)
		return -EINVAL;

	ulp_ctx->cfg_data->tunnel_next_proto = tunnel_next_proto;

	return 0;
}

/* Function to retrieve the vxlan_gpe next_proto from the context. */
uint8_t
bnxt_ulp_vxlan_gpe_next_proto_get(struct bnxt_ulp_context *ulp_ctx)
{
	if (!ulp_ctx || !ulp_ctx->cfg_data)
		return 0;

	return ulp_ctx->cfg_data->tunnel_next_proto;
}

/* Function to set the number for vxlan port into the context */
int
bnxt_ulp_cntxt_vxlan_port_set(struct bnxt_ulp_context *ulp_ctx,
			      u32 vxlan_port)
{
	if (!ulp_ctx || !ulp_ctx->cfg_data)
		return -EINVAL;

	ulp_ctx->cfg_data->vxlan_port = vxlan_port;
	if (vxlan_port)
		ulp_ctx->cfg_data->ulp_flags |= BNXT_ULP_STATIC_VXLAN_SUPPORT;

	return 0;
}

/* Function to retrieve the vxlan port from the context. */
unsigned int
bnxt_ulp_cntxt_vxlan_port_get(struct bnxt_ulp_context *ulp_ctx)
{
	if (!ulp_ctx || !ulp_ctx->cfg_data)
		return 0;

	return (unsigned int)ulp_ctx->cfg_data->vxlan_port;
}

int
bnxt_ulp_default_app_priority_set(struct bnxt_ulp_context *ulp_ctx,
				  u32 prio)
{
	if (!ulp_ctx || !ulp_ctx->cfg_data)
		return -EINVAL;

	ulp_ctx->cfg_data->default_priority = prio;
	return 0;
}

/* Function to retrieve the default app priority from the context. */
unsigned int
bnxt_ulp_default_app_priority_get(struct bnxt_ulp_context *ulp_ctx)
{
	if (!ulp_ctx || !ulp_ctx->cfg_data)
		return 0;

	return (unsigned int)ulp_ctx->cfg_data->default_priority;
}

int
bnxt_ulp_max_def_priority_set(struct bnxt_ulp_context *ulp_ctx,
			      u32 prio)
{
	if (!ulp_ctx || !ulp_ctx->cfg_data)
		return -EINVAL;

	ulp_ctx->cfg_data->max_def_priority = prio;
	return 0;
}

unsigned int
bnxt_ulp_max_def_priority_get(struct bnxt_ulp_context *ulp_ctx)
{
	if (!ulp_ctx || !ulp_ctx->cfg_data)
		return 0;

	return (unsigned int)ulp_ctx->cfg_data->max_def_priority;
}

static int
bnxt_ulp_force_mirror_en_set(struct bnxt_ulp_context *ulp_ctx,
			     bool force_mirror_en)
{
	if (!ulp_ctx || !ulp_ctx->cfg_data)
		return -EINVAL;

	ulp_ctx->cfg_data->force_mirror_en = force_mirror_en;
	return 0;
}

/* Function to retrieve the force mirror enable setting from the context. */
static int
bnxt_ulp_force_mirror_en_get(struct bnxt_ulp_context *ulp_ctx,
			     bool *force_mirror_en)
{
	if (!ulp_ctx || !ulp_ctx->cfg_data)
		return -EINVAL;

	*force_mirror_en = ulp_ctx->cfg_data->force_mirror_en;
	return 0;
}

int
bnxt_ulp_min_flow_priority_set(struct bnxt_ulp_context *ulp_ctx, u32 prio)
{
	if (!ulp_ctx || !ulp_ctx->cfg_data)
		return -EINVAL;

	ulp_ctx->cfg_data->min_flow_priority = prio;
	return 0;
}

unsigned int
bnxt_ulp_min_flow_priority_get(struct bnxt_ulp_context *ulp_ctx)
{
	if (!ulp_ctx || !ulp_ctx->cfg_data)
		return 0;

	return ulp_ctx->cfg_data->min_flow_priority;
}

int
bnxt_ulp_max_flow_priority_set(struct bnxt_ulp_context *ulp_ctx, u32 prio)
{
	if (!ulp_ctx || !ulp_ctx->cfg_data)
		return -EINVAL;

	ulp_ctx->cfg_data->max_flow_priority = prio;
	return 0;
}

unsigned int
bnxt_ulp_max_flow_priority_get(struct bnxt_ulp_context *ulp_ctx)
{
	if (!ulp_ctx || !ulp_ctx->cfg_data)
		return 0;

	return ulp_ctx->cfg_data->max_flow_priority;
}

/* The function to initialize bp flags with truflow features */
static int
ulp_dparms_dev_port_intf_update(struct bnxt *bp,
				struct bnxt_ulp_context *ulp_ctx)
{
	enum bnxt_ulp_flow_mem_type mtype;

	if (bnxt_ulp_cntxt_mem_type_get(ulp_ctx, &mtype))
		return -EINVAL;
	/* Update the bp flag with gfid flag */
	if (mtype == BNXT_ULP_FLOW_MEM_TYPE_EXT)
		bp->tf_flags |= BNXT_TF_FLAG_GFID_ENABLE;

	return 0;
}

/* Initialize the state of an ULP session.
 * If the state of an ULP session is not initialized, set it's state to
 * initialized. If the state is already initialized, do nothing.
 */
static void
ulp_context_initialized(struct bnxt_ulp_session_state *session, bool *init)
{
	mutex_lock(&session->bnxt_ulp_mutex);
	if (!session->bnxt_ulp_init) {
		session->bnxt_ulp_init = true;
		*init = false;
	} else {
		*init = true;
	}
	mutex_unlock(&session->bnxt_ulp_mutex);
}

/* Check if an ULP session is already allocated for a specific PCI
 * domain & bus. If it is already allocated simply return the session
 * pointer, otherwise allocate a new session.
 */
static struct bnxt_ulp_session_state *
ulp_get_session(struct bnxt *bp)
{
	struct bnxt_ulp_session_state *session;
	struct hlist_node *node;

	hlist_for_each_entry_safe(session, node, &bnxt_ulp_session_list, next) {
		if (bp->flags & BNXT_FLAG_DSN_VALID) {
			if (!memcmp(session->dsn, bp->dsn, sizeof(bp->dsn)))
				return session;
		} else {
			if (!memcmp(session->bsn, bp->board_serialno,
				    sizeof(bp->board_serialno)))
				return session;
		}
	}
	return NULL;
}

/* Allocate and Initialize an ULP session and set it's state to INITIALIZED.
 * If it's already initialized simply return the already existing session.
 */
static struct bnxt_ulp_session_state *
ulp_session_init(struct bnxt *bp,
		 bool *init)
{
	struct bnxt_ulp_session_state	*session;

	mutex_lock(&bnxt_ulp_global_mutex);
	session = ulp_get_session(bp);
	if (!session) {
		/* Not Found the session  Allocate a new one */
		session = vzalloc(sizeof(*session));
		if (!session) {
			mutex_unlock(&bnxt_ulp_global_mutex);
			return NULL;

		} else {
			/* Add it to the queue */
			if (bp->flags & BNXT_FLAG_DSN_VALID)
				memcpy(session->dsn, bp->dsn, sizeof(bp->dsn));
			else
				memcpy(session->bsn, bp->board_serialno,
				       sizeof(bp->board_serialno));
			mutex_init(&session->bnxt_ulp_mutex);
			hlist_add_head(&session->next, &bnxt_ulp_session_list);
		}
	}
	ulp_context_initialized(session, init);
	mutex_unlock(&bnxt_ulp_global_mutex);
	return session;
}

/* When a device is closed, remove it's associated session from the global
 * session list.
 */
static void
ulp_session_deinit(struct bnxt_ulp_session_state *session)
{
	if (!session)
		return;

	if (!session->cfg_data) {
		mutex_lock(&bnxt_ulp_global_mutex);
		hlist_del(&session->next);
		mutex_destroy(&session->bnxt_ulp_mutex);
		vfree(session);
		mutex_unlock(&bnxt_ulp_global_mutex);
	}
}

/* Internal function to delete all the flows belonging to the given port */
static void
bnxt_ulp_flush_port_flows(struct bnxt *bp)
{
	u16 func_id;

	/* it is assumed that port is either TVF or PF */
	if (ulp_port_db_port_func_id_get(bp->ulp_ctx,
					 bp->pf.fw_fid,
					 &func_id)) {
		netdev_dbg(bp->dev, "Invalid argument\n");
		return;
	}
	(void)ulp_flow_db_function_flow_flush(bp->ulp_ctx, func_id);
}

static const struct bnxt_ulp_core_ops *
bnxt_ulp_port_func_ops_get(struct bnxt *bp)
{
	const struct bnxt_ulp_core_ops *func_ops;
	enum bnxt_ulp_device_id  dev_id;
	int rc;

	rc = bnxt_ulp_devid_get(bp, &dev_id);
	if (rc)
		return NULL;

	switch (dev_id) {
	case BNXT_ULP_DEVICE_ID_THOR2:
		func_ops = &bnxt_ulp_tfc_core_ops;
		break;
	case BNXT_ULP_DEVICE_ID_THOR:
	case BNXT_ULP_DEVICE_ID_WH_PLUS:
		func_ops = &bnxt_ulp_tf_core_ops;
		break;
	default:
		func_ops = NULL;
		break;
	}
	return func_ops;
}

/* Entry point for Truflow feature initialization.
 */
int
bnxt_ulp_port_init(struct bnxt *bp)
{
	struct bnxt_ulp_session_state *session;
	enum bnxt_ulp_device_id dev_id;
	bool initialized;
	u32 ulp_flags;
	enum cfa_app_type app_type = CFA_APP_TYPE_TF;
	int rc = 0;
	struct bnxt_tls_info *ktls = bp->ktls_info;

	if (!BNXT_TRUFLOW_EN(bp)) {
		netdev_dbg(bp->dev,
			   "Skip ULP init for port:%d, truflow is not enabled\n",
			   bp->pf.fw_fid);
		return -EINVAL;
	}

	rc = bnxt_ulp_devid_get(bp, &dev_id);
	if (rc) {
		netdev_dbg(bp->dev, "Unsupported device %x\n", rc);
		return rc;
	}

	if (bp->ulp_ctx) {
		netdev_dbg(bp->dev, "ulp ctx already allocated\n");
		return rc;
	}

	rc = bnxt_hwrm_port_mac_qcfg(bp);
	if (rc)
		return rc;

	bp->ulp_ctx = vzalloc(sizeof(struct bnxt_ulp_context));
	if (!bp->ulp_ctx)
		return -ENOMEM;

	rc = bnxt_ulp_cntxt_bp_set(bp->ulp_ctx, bp);
	if (rc) {
		netdev_dbg(bp->dev, "Failed to set bp in ulp_ctx\n");
		vfree(bp->ulp_ctx);
		return -EIO;
	}

	/* Max number of ntuple filter is different for Truflow and AFM. */
	bp->max_fltr = BNXT_TF_MAX_FLTR;
	if (ktls)
		ktls->max_filter_tf = BNXT_MAX_TF_TLS_FILTER;

	/* This shouldn't fail, unless we have a unknown device */
	((struct bnxt_ulp_context *)bp->ulp_ctx)->ops = bnxt_ulp_port_func_ops_get(bp);
	if (!((struct bnxt_ulp_context *)bp->ulp_ctx)->ops) {
		netdev_dbg(bp->dev, "Failed to get ulp ops\n");
		vfree(bp->ulp_ctx);
		bp->ulp_ctx = NULL;
		return -EIO;
	}

	if (!BNXT_CHIP_P7(bp)) {
		/* Thor needs to initialize tfp structure during ulp init only.
		 * Thor2 has done this at bnxt open due to requirements regarding
		 * table scopes which is shared by truflow and cfa.
		 */
		bp->tfp = vzalloc(sizeof(*bp->tfp) * BNXT_SESSION_TYPE_LAST);
		if (!bp->tfp) {
			vfree(bp->ulp_ctx);
			return -ENOMEM;
		}
	}

	/* Multiple uplink ports can be associated with a single vswitch.
	 * Make sure only the port that is started first will initialize
	 * the TF session.
	 */
	session = ulp_session_init(bp, &initialized);
	if (!session) {
		netdev_dbg(bp->dev, "Failed to initialize the tf session\n");
		rc = -EIO;
		goto jump_to_error;
	}

	if (initialized) {
		/* If ULP is already initialized for a specific domain then
		 * simply assign the ulp context to this netdev as well.
		 */
		 rc = ((struct bnxt_ulp_context *)bp->ulp_ctx)->ops->ulp_ctx_attach(bp, session,
										    app_type);
		if (rc) {
			netdev_dbg(bp->dev, "Failed to attach the ulp context\n");
			goto jump_to_error;
		}
	} else {
		rc = ((struct bnxt_ulp_context *)bp->ulp_ctx)->ops->ulp_init(bp, session,
									     app_type);
		if (rc) {
			netdev_dbg(bp->dev, "Failed to initialize the ulp init\n");
			goto jump_to_error;
		}
	}

	/* Update bnxt driver flags */
	rc = ulp_dparms_dev_port_intf_update(bp, bp->ulp_ctx);
	if (rc) {
		netdev_dbg(bp->dev, "Failed to update driver flags\n");
		goto jump_to_error;
	}

	/* update the port database for the given interface */
	rc = ulp_port_db_dev_port_intf_update(bp->ulp_ctx, bp, NULL);
	if (rc) {
		netdev_dbg(bp->dev, "Failed to update port database\n");
		goto jump_to_error;
	}

	/* create the default rules */
	rc = bnxt_ulp_create_df_rules(bp);
	if (rc) {
		netdev_dbg(bp->dev, "Failed to create default flow\n");
		goto jump_to_error;
	}

	/* set the unicast mode */
	if (bnxt_ulp_cntxt_ptr2_ulp_flags_get(bp->ulp_ctx, &ulp_flags)) {
		netdev_dbg(bp->dev, "Error in getting ULP context flags\n");
		goto jump_to_error;
	}

	/* NIC flow support */
	if (ULP_APP_NIC_FLOWS_SUPPORTED((struct bnxt_ulp_context *)bp->ulp_ctx)) {
		rc = bnxt_nic_flows_init(bp);
		if (rc) {
			netdev_dbg(bp->dev, "Failed to open nic flows:%d\n", rc);
			bnxt_nic_flows_deinit(bp);
			goto jump_to_error;
		}
	}

	return rc;

jump_to_error:
	bnxt_ulp_port_deinit(bp);
	return rc;
}

/* When a port is de-initialized. This functions clears up
 * the port specific details.
 */
void
bnxt_ulp_port_deinit(struct bnxt *bp)
{
	struct bnxt_ulp_context *ulp_ctx = bp->ulp_ctx;
	struct bnxt_ulp_session_state *session;
	struct bnxt_hw_resc *hw_resc;
	struct bnxt_tls_info *ktls = bp->ktls_info;


	if (!BNXT_TRUFLOW_EN(bp)) {
		netdev_dbg(bp->dev,
			   "Skip ULP deinit for port:%d, truflow is not enabled\n",
			   bp->pf.fw_fid);
		return;
	}

	if (!BNXT_PF(bp) && !BNXT_VF_IS_TRUSTED(bp)) {
		netdev_dbg(bp->dev,
			   "Skip ULP deinit port:%d, not a TVF or PF\n",
			   bp->pf.fw_fid);
		return;
	}

	if (!bp->ulp_ctx) {
		netdev_dbg(bp->dev, "ulp ctx already de-allocated\n");
		return;
	}

	netdev_dbg(bp->dev, "BNXT Port:%d ULP port deinit\n",
		   bp->pf.fw_fid);

	bnxt_nic_flows_deinit(bp);

	/* Get the session details  */
	mutex_lock(&bnxt_ulp_global_mutex);
	session = ulp_get_session(bp);
	mutex_unlock(&bnxt_ulp_global_mutex);
	/* session not found then just exit */
	if (!session) {
		/* Free the ulp context */
		vfree(bp->ulp_ctx);
		vfree(bp->tfp);

		bp->ulp_ctx = NULL;
		bp->tfp = NULL;
		return;
	}

	/* Check the reference count to deinit or deattach */
	if (ulp_ctx->cfg_data && ulp_ctx->cfg_data->ref_cnt) {
		ulp_ctx->cfg_data->ref_cnt--;
		if (ulp_ctx->cfg_data->ref_cnt) {
			/* free the port details */
			/* Free the default flow rule associated to this port */
			bnxt_ulp_destroy_df_rules(bp, false);

			/* free flows associated with this port */
			bnxt_ulp_flush_port_flows(bp);

			/* close the session associated with this port */
			((struct bnxt_ulp_context *)bp->ulp_ctx)->ops->ulp_ctx_detach(bp, session);
		} else {
			/* Free the default flow rule associated to this port */
			bnxt_ulp_destroy_df_rules(bp, true);

			/* free flows associated with this port */
			bnxt_ulp_flush_port_flows(bp);

			/* Perform ulp ctx deinit */
			((struct bnxt_ulp_context *)bp->ulp_ctx)->ops->ulp_deinit(bp, session);
		}
	}

	/* Set the max number of ntuple filter back to the number from AFM */
	hw_resc = &bp->hw_resc;
	bp->max_fltr = hw_resc->max_rx_em_flows + hw_resc->max_rx_wm_flows +
		       BNXT_L2_FLTR_MAX_FLTR;
	/* Older firmware may not report these filters properly */
	if (bp->max_fltr < BNXT_MAX_FLTR)
		bp->max_fltr = BNXT_MAX_FLTR;

	if (ktls)
		ktls->max_filter_tf = 0;

	/* Free the ulp context in the context entry list */
	bnxt_ulp_cntxt_list_del(bp->ulp_ctx);

	/* clean up the session */
	ulp_session_deinit(session);

	/* Free the ulp context */
	vfree(bp->ulp_ctx);
	if (!BNXT_CHIP_P7(bp)) {
		/* Only free resources for Thor. Thor2 remains
		 * available for table scope operations.
		 */
		vfree(bp->tfp);
		bp->tfp = NULL;
	}
	bp->ulp_ctx = NULL;
}

/* Below are the access functions to access internal data of ulp context. */
/* Function to set the Mark DB into the context */
int
bnxt_ulp_cntxt_ptr2_mark_db_set(struct bnxt_ulp_context *ulp_ctx,
				struct bnxt_ulp_mark_tbl *mark_tbl)
{
	if (!ulp_ctx || !ulp_ctx->cfg_data)
		return -EINVAL;

	ulp_ctx->cfg_data->mark_tbl = mark_tbl;

	return 0;
}

/* Function to retrieve the Mark DB from the context. */
struct bnxt_ulp_mark_tbl *
bnxt_ulp_cntxt_ptr2_mark_db_get(struct bnxt_ulp_context *ulp_ctx)
{
	if (!ulp_ctx || !ulp_ctx->cfg_data)
		return NULL;

	return ulp_ctx->cfg_data->mark_tbl;
}

bool bnxt_ulp_cntxt_shared_session_enabled(struct bnxt_ulp_context *ulp_ctx)
{
	return ULP_SHARED_SESSION_IS_ENABLED(ulp_ctx->cfg_data->ulp_flags);
}

bool
bnxt_ulp_cntxt_multi_shared_session_enabled(struct bnxt_ulp_context *ulp_ctx)
{
	return ULP_MULTI_SHARED_IS_SUPPORTED(ulp_ctx);
}

int
bnxt_ulp_cntxt_app_id_set(struct bnxt_ulp_context *ulp_ctx, u8 app_id)
{
	if (!ulp_ctx)
		return -EINVAL;
	ulp_ctx->cfg_data->app_id = app_id;
	netdev_dbg(ulp_ctx->bp->dev, "%s: Truflow APP ID is %d\n", __func__,
		   app_id & ~BNXT_ULP_APP_ID_SET_CONFIGURED);
	return 0;
}

int
bnxt_ulp_cntxt_app_id_get(struct bnxt_ulp_context *ulp_ctx, u8 *app_id)
{
	/* Default APP id is zero */
	if (!ulp_ctx || !app_id)
		return -EINVAL;
	*app_id = ulp_ctx->cfg_data->app_id & ~BNXT_ULP_APP_ID_SET_CONFIGURED;
	netdev_dbg(ulp_ctx->bp->dev, "%s: Truflow APP ID is %d\n", __func__,
		   ulp_ctx->cfg_data->app_id & ~BNXT_ULP_APP_ID_SET_CONFIGURED);
	return 0;
}

/* Function to set the device id of the hardware. */
int
bnxt_ulp_cntxt_dev_id_set(struct bnxt_ulp_context *ulp_ctx,
			  u32 dev_id)
{
	if (ulp_ctx && ulp_ctx->cfg_data) {
		ulp_ctx->cfg_data->dev_id = dev_id;
		return 0;
	}

	return -EINVAL;
}

/* Function to get the device id of the hardware. */
int
bnxt_ulp_cntxt_dev_id_get(struct bnxt_ulp_context *ulp_ctx,
			  u32 *dev_id)
{
	if (ulp_ctx && ulp_ctx->cfg_data) {
		*dev_id = ulp_ctx->cfg_data->dev_id;
		return 0;
	}
	*dev_id = BNXT_ULP_DEVICE_ID_LAST;
	return -EINVAL;
}

int
bnxt_ulp_cntxt_mem_type_set(struct bnxt_ulp_context *ulp_ctx,
			    enum bnxt_ulp_flow_mem_type mem_type)
{
	if (ulp_ctx && ulp_ctx->cfg_data) {
		ulp_ctx->cfg_data->mem_type = mem_type;
		return 0;
	}

	return -EINVAL;
}

int
bnxt_ulp_cntxt_mem_type_get(struct bnxt_ulp_context *ulp_ctx,
			    enum bnxt_ulp_flow_mem_type *mem_type)
{
	if (ulp_ctx && ulp_ctx->cfg_data) {
		*mem_type = ulp_ctx->cfg_data->mem_type;
		return 0;
	}

	*mem_type = BNXT_ULP_FLOW_MEM_TYPE_LAST;
	return -EINVAL;
}

/* Function to get the table scope id of the EEM table. */
int
bnxt_ulp_cntxt_tbl_scope_id_get(struct bnxt_ulp_context *ulp_ctx,
				u32 *tbl_scope_id)
{
	if (ulp_ctx && ulp_ctx->cfg_data) {
		*tbl_scope_id = ulp_ctx->cfg_data->tbl_scope_id;
		return 0;
	}

	return -EINVAL;
}

/* Function to set the table scope id of the EEM table. */
int
bnxt_ulp_cntxt_tbl_scope_id_set(struct bnxt_ulp_context *ulp_ctx,
				u32 tbl_scope_id)
{
	if (ulp_ctx && ulp_ctx->cfg_data) {
		ulp_ctx->cfg_data->tbl_scope_id = tbl_scope_id;
		return 0;
	}

	return -EINVAL;
}

/* Function to set the v3 table scope id, only works for tfc objects */
int
bnxt_ulp_cntxt_tsid_set(struct bnxt_ulp_context *ulp_ctx, uint8_t tsid)
{
	if (ulp_ctx && ulp_ctx->tfo_type == BNXT_ULP_TFO_TYPE_P7) {
		ulp_ctx->tsid = tsid;
		ULP_BITMAP_SET(ulp_ctx->tfo_flags, BNXT_ULP_TFO_TSID_FLAG);
		return 0;
	}
	return -EINVAL;
}

/* Function to reset the v3 table scope id, only works for tfc objects */
void
bnxt_ulp_cntxt_tsid_reset(struct bnxt_ulp_context *ulp_ctx)
{
	if (ulp_ctx && ulp_ctx->tfo_type == BNXT_ULP_TFO_TYPE_P7)
		ULP_BITMAP_RESET(ulp_ctx->tfo_flags, BNXT_ULP_TFO_TSID_FLAG);
}

/* Function to set the v3 table scope id, only works for tfc objects */
int
bnxt_ulp_cntxt_tsid_get(struct bnxt_ulp_context *ulp_ctx, uint8_t *tsid)
{
	if (ulp_ctx && tsid &&
	    ulp_ctx->tfo_type == BNXT_ULP_TFO_TYPE_P7 &&
	    ULP_BITMAP_ISSET(ulp_ctx->tfo_flags, BNXT_ULP_TFO_TSID_FLAG)) {
		*tsid = ulp_ctx->tsid;
		return 0;
	}
	return -EINVAL;
}

/* Function to set the v3 session id, only works for tfc objects */
int
bnxt_ulp_cntxt_sid_set(struct bnxt_ulp_context *ulp_ctx,
		       uint16_t sid)
{
	if (ulp_ctx && ulp_ctx->tfo_type == BNXT_ULP_TFO_TYPE_P7) {
		ulp_ctx->sid = sid;
		ULP_BITMAP_SET(ulp_ctx->tfo_flags, BNXT_ULP_TFO_SID_FLAG);
		return 0;
	}
	return -EINVAL;
}

/* Function to reset the v3 session id, only works for tfc objects
 * There isn't a known invalid value for sid, so this is necessary
 */
void
bnxt_ulp_cntxt_sid_reset(struct bnxt_ulp_context *ulp_ctx)
{
	if (ulp_ctx && ulp_ctx->tfo_type == BNXT_ULP_TFO_TYPE_P7)
		ULP_BITMAP_RESET(ulp_ctx->tfo_flags, BNXT_ULP_TFO_SID_FLAG);
}

/* Function to get the v3 session id, only works for tfc objects */
int
bnxt_ulp_cntxt_sid_get(struct bnxt_ulp_context *ulp_ctx,
		       uint16_t *sid)
{
	if (ulp_ctx && sid &&
	    ulp_ctx->tfo_type == BNXT_ULP_TFO_TYPE_P7 &&
	    ULP_BITMAP_ISSET(ulp_ctx->tfo_flags, BNXT_ULP_TFO_SID_FLAG)) {
		*sid = ulp_ctx->sid;
		return 0;
	}
	return -EINVAL;
}

/* Function to set the number of shared clients */
int
bnxt_ulp_cntxt_num_shared_clients_set(struct bnxt_ulp_context *ulp, bool incr)
{
	if (!ulp || !ulp->cfg_data)
		return 0;

	if (incr)
		ulp->cfg_data->num_shared_clients++;
	else if (ulp->cfg_data->num_shared_clients)
		ulp->cfg_data->num_shared_clients--;

	netdev_dbg(ulp->bp->dev, "%d:clients(%d)\n", incr,
		   ulp->cfg_data->num_shared_clients);

	return 0;
}

int
bnxt_ulp_cntxt_bp_set(struct bnxt_ulp_context *ulp, struct bnxt *bp)
{
	if (!ulp) {
		netdev_dbg(bp->dev, "Invalid arguments\n");
		return -EINVAL;
	}
	ulp->bp = bp;
	return 0;
}

struct bnxt*
bnxt_ulp_cntxt_bp_get(struct bnxt_ulp_context *ulp)
{
	if (!ulp) {
		netdev_dbg(NULL, "Invalid arguments\n");
		return NULL;
	}
	return ulp->bp;
}

int
bnxt_ulp_cntxt_fid_get(struct bnxt_ulp_context *ulp, uint16_t *fid)
{
	if (!ulp || !fid)
		return -EINVAL;

	*fid = ulp->bp->pf.fw_fid;
	return 0;
}

void
bnxt_ulp_cntxt_ptr2_default_class_bits_set(struct bnxt_ulp_context *ulp_ctx,
					   u64 bits)
{
	if (!ulp_ctx || !ulp_ctx->cfg_data)
		return;
	ulp_ctx->cfg_data->default_class_bits = bits;
}

u64
bnxt_ulp_cntxt_ptr2_default_class_bits_get(struct bnxt_ulp_context *ulp_ctx)
{
	if (!ulp_ctx || !ulp_ctx->cfg_data)
		return 0;
	return ulp_ctx->cfg_data->default_class_bits;
}

void
bnxt_ulp_cntxt_ptr2_default_act_bits_set(struct bnxt_ulp_context *ulp_ctx,
					 u64 bits)
{
	if (!ulp_ctx || !ulp_ctx->cfg_data)
		return;
	ulp_ctx->cfg_data->default_act_bits = bits;
}

u64
bnxt_ulp_cntxt_ptr2_default_act_bits_get(struct bnxt_ulp_context *ulp_ctx)
{
	if (!ulp_ctx || !ulp_ctx->cfg_data)
		return 0;
	return ulp_ctx->cfg_data->default_act_bits;
}

/**
 * Get the device table entry based on the device id.
 *
 * @dev_id: The device id of the hardware
 *
 * Returns the pointer to the device parameters.
 */
struct bnxt_ulp_device_params *
bnxt_ulp_device_params_get(u32 dev_id)
{
	if (dev_id < BNXT_ULP_MAX_NUM_DEVICES)
		return &ulp_device_params[dev_id];
	return NULL;
}

/* Function to set the flow database to the ulp context. */
int
bnxt_ulp_cntxt_ptr2_flow_db_set(struct bnxt_ulp_context	*ulp_ctx,
				struct bnxt_ulp_flow_db	*flow_db)
{
	if (!ulp_ctx || !ulp_ctx->cfg_data)
		return -EINVAL;

	ulp_ctx->cfg_data->flow_db = flow_db;
	return 0;
}

/* Function to get the flow database from the ulp context. */
struct bnxt_ulp_flow_db	*
bnxt_ulp_cntxt_ptr2_flow_db_get(struct bnxt_ulp_context	*ulp_ctx)
{
	if (!ulp_ctx || !ulp_ctx->cfg_data)
		return NULL;

	return ulp_ctx->cfg_data->flow_db;
}

/* Function to get the ulp context from eth device. */
struct bnxt_ulp_context	*
bnxt_ulp_bp_ptr2_cntxt_get(struct bnxt *bp)
{
	if (!bp)
		return NULL;

	return bp->ulp_ctx;
}

int
bnxt_ulp_cntxt_ptr2_mapper_data_set(struct bnxt_ulp_context *ulp_ctx,
				    void *mapper_data)
{
	if (!ulp_ctx || !ulp_ctx->cfg_data)
		return -EINVAL;

	ulp_ctx->cfg_data->mapper_data = mapper_data;
	return 0;
}

void *
bnxt_ulp_cntxt_ptr2_mapper_data_get(struct bnxt_ulp_context *ulp_ctx)
{
	if (!ulp_ctx || !ulp_ctx->cfg_data)
		return NULL;

	return ulp_ctx->cfg_data->mapper_data;
}

int
bnxt_ulp_cntxt_ptr2_matcher_data_set(struct bnxt_ulp_context *ulp_ctx,
				     void *matcher_data)
{
	if (!ulp_ctx || !ulp_ctx->cfg_data)
		return -EINVAL;

	ulp_ctx->cfg_data->matcher_data = matcher_data;
	return 0;
}

void *
bnxt_ulp_cntxt_ptr2_matcher_data_get(struct bnxt_ulp_context *ulp_ctx)
{
	if (!ulp_ctx || !ulp_ctx->cfg_data)
		return NULL;

	return ulp_ctx->cfg_data->matcher_data;
}

/* Function to set the port database to the ulp context. */
int
bnxt_ulp_cntxt_ptr2_port_db_set(struct bnxt_ulp_context	*ulp_ctx,
				struct bnxt_ulp_port_db	*port_db)
{
	if (!ulp_ctx || !ulp_ctx->cfg_data)
		return -EINVAL;

	ulp_ctx->cfg_data->port_db = port_db;
	return 0;
}

/* Function to get the port database from the ulp context. */
struct bnxt_ulp_port_db *
bnxt_ulp_cntxt_ptr2_port_db_get(struct bnxt_ulp_context	*ulp_ctx)
{
	if (!ulp_ctx || !ulp_ctx->cfg_data)
		return NULL;

	return ulp_ctx->cfg_data->port_db;
}

/* Function to set the flow counter info into the context */
int
bnxt_ulp_cntxt_ptr2_fc_info_set(struct bnxt_ulp_context *ulp_ctx,
				struct bnxt_ulp_fc_info *ulp_fc_info)
{
	if (!ulp_ctx || !ulp_ctx->cfg_data)
		return -EINVAL;

	ulp_ctx->cfg_data->fc_info = ulp_fc_info;

	return 0;
}

/* Function to retrieve the flow counter info from the context. */
struct bnxt_ulp_fc_info *
bnxt_ulp_cntxt_ptr2_fc_info_get(struct bnxt_ulp_context *ulp_ctx)
{
	if (!ulp_ctx || !ulp_ctx->cfg_data)
		return NULL;

	return ulp_ctx->cfg_data->fc_info;
}

/* Function to set the flow counter info into the context */
int
bnxt_ulp_cntxt_ptr2_sc_info_set(struct bnxt_ulp_context *ulp_ctx,
				struct bnxt_ulp_sc_info *ulp_sc_info)
{
	if (!ulp_ctx || !ulp_ctx->cfg_data)
		return -EINVAL;

	ulp_ctx->cfg_data->sc_info = ulp_sc_info;

	return 0;
}

/* Function to retrieve the flow counter info from the context. */
struct bnxt_ulp_sc_info *
bnxt_ulp_cntxt_ptr2_sc_info_get(struct bnxt_ulp_context *ulp_ctx)
{
	if (!ulp_ctx || !ulp_ctx->cfg_data)
		return NULL;

	return ulp_ctx->cfg_data->sc_info;
}

/* Function to get the ulp flags from the ulp context. */
int
bnxt_ulp_cntxt_ptr2_ulp_flags_get(struct bnxt_ulp_context *ulp_ctx,
				  u32 *flags)
{
	if (!ulp_ctx || !ulp_ctx->cfg_data)
		return -1;

	*flags =  ulp_ctx->cfg_data->ulp_flags;
	return 0;
}

/* Function to get the ulp vfr info from the ulp context. */
struct bnxt_ulp_vfr_rule_info*
bnxt_ulp_cntxt_ptr2_ulp_vfr_info_get(struct bnxt_ulp_context *ulp_ctx,
				     u32 port_id)
{
	if (!ulp_ctx || !ulp_ctx->cfg_data || port_id >= BNXT_TC_MAX_PORTS)
		return NULL;

	return &ulp_ctx->cfg_data->vfr_rule_info[port_id];
}

int
bnxt_ulp_cntxt_list_init(void)
{
	/* Create the cntxt spin lock only once*/
	if (!bnxt_ulp_ctxt_lock_created)
		mutex_init(&bnxt_ulp_ctxt_lock);
	bnxt_ulp_ctxt_lock_created = 1;
	return 0;
}

int
bnxt_ulp_cntxt_list_add(struct bnxt_ulp_context *ulp_ctx)
{
	struct ulp_context_list_entry	*entry;

	entry = vzalloc(sizeof(*entry));
	if (!entry)
		return -ENOMEM;

	mutex_lock(&bnxt_ulp_ctxt_lock);
	entry->ulp_ctx = ulp_ctx;
	hlist_add_head(&entry->next, &ulp_cntx_list);
	mutex_unlock(&bnxt_ulp_ctxt_lock);

	return 0;
}

void
bnxt_ulp_cntxt_list_del(struct bnxt_ulp_context *ulp_ctx)
{
	struct ulp_context_list_entry	*entry;
	struct hlist_node *node;

	mutex_lock(&bnxt_ulp_ctxt_lock);
	hlist_for_each_entry_safe(entry, node, &ulp_cntx_list, next) {
		if (entry && entry->ulp_ctx == ulp_ctx) {
			hlist_del(&entry->next);
			vfree(entry);
			break;
		}
	}
	mutex_unlock(&bnxt_ulp_ctxt_lock);
}

struct bnxt_ulp_context *
bnxt_ulp_cntxt_entry_lookup(void *cfg_data)
{
	struct ulp_context_list_entry	*entry;
	struct hlist_node *node;

	/* take a lock and get the first ulp context available */
	hlist_for_each_entry_safe(entry, node, &ulp_cntx_list, next) {
		if (entry && entry->ulp_ctx &&
		    entry->ulp_ctx->cfg_data == cfg_data)
			return entry->ulp_ctx;
	}

	return NULL;
}

void
bnxt_ulp_cntxt_lock_acquire(void)
{
	mutex_lock(&bnxt_ulp_ctxt_lock);
}

void
bnxt_ulp_cntxt_lock_release(void)
{
	mutex_unlock(&bnxt_ulp_ctxt_lock);
}

/* Function to convert ulp dev id to regular dev id. */
u32
bnxt_ulp_cntxt_convert_dev_id(struct bnxt *bp, u32 ulp_dev_id)
{
	enum tf_device_type type = 0;

	switch (ulp_dev_id) {
	case BNXT_ULP_DEVICE_ID_WH_PLUS:
		type = TF_DEVICE_TYPE_P4;
		break;
	case BNXT_ULP_DEVICE_ID_THOR:
		type = TF_DEVICE_TYPE_P5;
		break;
	default:
		netdev_dbg(bp->dev, "Invalid device id\n");
		break;
	}
	return type;
}

/* CFA code retrieval for THOR2
 * This process differs from THOR in that the code is kept in the
 * metadata field instead of the errors_v2 field.
 */
int
bnxt_ulp_get_mark_from_cfacode_p7(struct bnxt *bp, struct rx_cmp_ext *rxcmp1,
				  struct bnxt_tpa_info *tpa_info, u32 *mark_id)
{
	bool gfid = false;
	u32 vfr_flag;
	u32 cfa_code;
	u32 meta_fmt;
	u32 flags2;
	u32 meta;
	int rc;

	if (rxcmp1) {
		cfa_code = RX_CMP_CFA_V3_CODE(rxcmp1);
		flags2 = le32_to_cpu(rxcmp1->rx_cmp_flags2);
		meta = le32_to_cpu(rxcmp1->rx_cmp_meta_data);
	} else {
		cfa_code = le16_to_cpu(tpa_info->cfa_code);
		flags2 = le32_to_cpu(tpa_info->flags2);
		meta = le32_to_cpu(tpa_info->metadata);
	}

	/* The flags field holds extra bits of info from [6:4]
	 * which indicate if the flow is in TCAM or EM or EEM
	 */
	meta_fmt = (flags2 & BNXT_CFA_META_FMT_MASK) >>
		BNXT_CFA_META_FMT_SHFT;
	switch (meta_fmt) {
	case 0:
		if (BNXT_GFID_ENABLED(bp))
			/* Not an LFID or GFID, a flush cmd. */
			goto skip_mark;
		break;
	case 4:
		fallthrough;
	case 5:
		/* EM/TCAM case
		 * Assume that EM doesn't support Mark due to GFID
		 * collisions with EEM.  Simply return without setting the mark
		 * in the mbuf.
		 */
		/* If it is not EM then it is a TCAM entry, so it is an LFID.
		 * The TCAM IDX and Mode can also be determined
		 * by decoding the meta_data. We are not
		 * using these for now.
		 */
		if (BNXT_CFA_META_EM_TEST(meta)) {
			/*This is EM hit {EM(1), GFID[27:16], 19'd0 or vtag } */
			gfid = true;
			meta >>= BNXT_RX_META_CFA_CODE_SHIFT;
			cfa_code |= meta << BNXT_CFA_CODE_META_SHIFT;
		}
		break;
	case 6:
		fallthrough;
	case 7:
		/* EEM Case, only using gfid in EEM for now. */
		gfid = true;

		/* For EEM flows, The first part of cfa_code is 16 bits.
		 * The second part is embedded in the
		 * metadata field from bit 19 onwards. The driver needs to
		 * ignore the first 19 bits of metadata and use the next 12
		 * bits as higher 12 bits of cfa_code.
		 */
		meta >>= BNXT_RX_META_CFA_CODE_SHIFT;
		cfa_code |= meta << BNXT_CFA_CODE_META_SHIFT;
		break;
	default:
		/* For other values, the cfa_code is assumed to be an LFID. */
		break;
	}

	rc = ulp_mark_db_mark_get(bp->ulp_ctx, gfid,
				  cfa_code, &vfr_flag, mark_id);
	if (!rc) {
		/* mark_id is the fw_fid of the endpoint vf's and
		 * it is used to identify the VFR.
		 */
		if (vfr_flag)
			return 0;
	}

skip_mark:
	return -EINVAL;
}

bool bnxt_ulp_can_enable_vf_trust(struct bnxt *bp)
{
	struct bnxt_ulp_context *ulp_ctx = bnxt_ulp_bp_ptr2_cntxt_get(bp);
	u8 app_id;
	int rc;

	/* Return if tf is not enabled */
	if (!(bp->tf_flags & BNXT_TF_FLAG_INITIALIZED))
		return true;

	if (!ulp_ctx) {
		netdev_dbg(bp->dev, "%s: ULP context is not initialized\n",
			   __func__);
		return true;
	}
	rc = bnxt_ulp_cntxt_app_id_get(ulp_ctx, &app_id);
	if (rc) {
		netdev_dbg(ulp_ctx->bp->dev, "%s: Unable to get the app id from ulp\n",
			   __func__);
		return true;
	}
	/* For app1 restrict trust functionality if switchdev is on
	 * otherwise it may cause issues in FW recovery.
	 */
	if (app_id == 1 && bnxt_tc_is_switchdev_mode(bp)) {
		netdev_dbg(bp->dev, "Disable switchdev mode before enabling trusted mode on VFs\n");
		return false;
	}
	return true;
}

/* Specified value in bool variable(stat) will clear/set the mirror */
int bnxt_ulp_set_mirror(struct bnxt *bp, bool stat)
{
	struct bnxt_ulp_context *ulp_ctx = bnxt_ulp_bp_ptr2_cntxt_get(bp);
	bool allow_mirror_op = false;
	bool force_mirror_en;
	enum tf_dir i;
	int rc;

	/* Return if tf is not enabled or function is VF*/
	if (!(bp->tf_flags & BNXT_TF_FLAG_INITIALIZED) || BNXT_VF(bp))
		return 0;

	if (!ulp_ctx) {
		netdev_dbg(bp->dev, "%s: ULP context is not initialized\n",
			   __func__);
		return -EINVAL;
	}

	/* P7 has a potential issue when mirroring and VFs and VEB enabled */
	if (BNXT_CHIP_P7(bp)) {
		rc = bnxt_ulp_force_mirror_en_get(ulp_ctx, &force_mirror_en);
		if (rc) {
			netdev_dbg(bp->dev, "Unable to get force mirror enable\n");
			return rc;
		}
		if (force_mirror_en)
			allow_mirror_op = true;
		/* If force mirror enable is not set, we can also enable the mirror
		 * if !VEB (no NPAR) and no SRIOV enabled (no active VFs)
		 */
		if (!BNXT_NPAR(bp) && BNXT_PF(bp) && !bp->pf.active_vfs)
			allow_mirror_op = true;

		/* Always allow disable of the mirror and disable force_mirror_en
		 * whenever the mirror is disabled.
		 */
		if (!stat) {
			allow_mirror_op = true;
			bnxt_ulp_force_mirror_en_set(ulp_ctx, false);
		}
	} else { /* not P7, allow mirror always */
		allow_mirror_op = true;
	}

	if (allow_mirror_op) {
		for (i = TF_DIR_RX; i <= TF_DIR_TX; i++) {
			/* Call appropriate direction mirror template
			 * 1. port id will be passed in
			 * 2. stat will be passed in to enable/disable
			 */
			rc = bnxt_ulp_mirror_op(bp, i, stat);
			if (rc)
				return rc;
		}
	}

	return 0;
}

int bnxt_tf_ulp_force_mirror_en_cfg_p7(struct bnxt *bp, bool enable)
{
	struct bnxt_ulp_context *ulp_ctx = bnxt_ulp_bp_ptr2_cntxt_get(bp);

	/* Return if tf is not enabled or function is VF */
	if (!(bp->tf_flags & BNXT_TF_FLAG_INITIALIZED) || BNXT_VF(bp))
		return 0;

	return bnxt_ulp_force_mirror_en_set(ulp_ctx, enable);
}

void bnxt_tf_ulp_force_mirror_en_get_p7(struct bnxt *bp, bool *tf_en,
					bool *force_mirror_en)
{
	struct bnxt_ulp_context *ulp_ctx = bnxt_ulp_bp_ptr2_cntxt_get(bp);

	/* Return if tf is not enabled or function is VF */
	if (!(bp->tf_flags & BNXT_TF_FLAG_INITIALIZED) || BNXT_VF(bp)) {
		*tf_en = false;
		return;
	}

	bnxt_ulp_force_mirror_en_get(ulp_ctx, force_mirror_en);
}

int
bnxt_ulp_get_mark_from_cfacode(struct bnxt *bp, struct rx_cmp_ext *rxcmp1,
			       struct bnxt_tpa_info *tpa_info, u32 *mark_id)
{
	bool gfid = false;
	u32 vfr_flag;
	u32 cfa_code;
	u32 meta_fmt;
	u32 flags2;
	u32 meta;
	int rc;

	if (rxcmp1) {
		cfa_code = RX_CMP_CFA_CODE(rxcmp1);
		flags2 = le32_to_cpu(rxcmp1->rx_cmp_flags2);
		meta = le32_to_cpu(rxcmp1->rx_cmp_meta_data);
	} else {
		cfa_code = le16_to_cpu(tpa_info->cfa_code);
		flags2 = le32_to_cpu(tpa_info->flags2);
		meta = le32_to_cpu(tpa_info->metadata);
	}

	/* The flags field holds extra bits of info from [6:4]
	 * which indicate if the flow is in TCAM or EM or EEM
	 */
	meta_fmt = (flags2 & BNXT_CFA_META_FMT_MASK) >>
		BNXT_CFA_META_FMT_SHFT;
	switch (meta_fmt) {
	case 0:
		if (BNXT_GFID_ENABLED(bp))
			/* Not an LFID or GFID, a flush cmd. */
			goto skip_mark;
		break;
	case 4:
		fallthrough;
	case 5:
		/* EM/TCAM case
		 * Assume that EM doesn't support Mark due to GFID
		 * collisions with EEM.  Simply return without setting the mark
		 * in the mbuf.
		 */
		/* If it is not EM then it is a TCAM entry, so it is an LFID.
		 * The TCAM IDX and Mode can also be determined
		 * by decoding the meta_data. We are not
		 * using these for now.
		 */
		if (BNXT_CFA_META_EM_TEST(meta)) {
			/*This is EM hit {EM(1), GFID[27:16], 19'd0 or vtag } */
			gfid = true;
			meta >>= BNXT_RX_META_CFA_CODE_SHIFT;
			cfa_code |= meta << BNXT_CFA_CODE_META_SHIFT;
		}
		break;
	case 6:
		fallthrough;
	case 7:
		/* EEM Case, only using gfid in EEM for now. */
		gfid = true;

		/* For EEM flows, The first part of cfa_code is 16 bits.
		 * The second part is embedded in the
		 * metadata field from bit 19 onwards. The driver needs to
		 * ignore the first 19 bits of metadata and use the next 12
		 * bits as higher 12 bits of cfa_code.
		 */
		meta >>= BNXT_RX_META_CFA_CODE_SHIFT;
		cfa_code |= meta << BNXT_CFA_CODE_META_SHIFT;
		break;
	default:
		/* For other values, the cfa_code is assumed to be an LFID. */
		break;
	}

	rc = ulp_mark_db_mark_get(bp->ulp_ctx, gfid,
				  cfa_code, &vfr_flag, mark_id);
	if (!rc) {
		/* mark_id is the fw_fid of the endpoint vf's and
		 * it is used to identify the VFR.
		 */
		if (vfr_flag)
			return 0;
	}

skip_mark:
	return -EINVAL;
}

int bnxt_ulp_alloc_vf_rep(struct bnxt *bp, void *vfr)
{
	int rc;

	rc = ulp_port_db_dev_port_intf_update(bp->ulp_ctx, bp, vfr);
	if (rc) {
		netdev_dbg(bp->dev, "Failed to update port database\n");
		return -EINVAL;
	}

	rc = bnxt_hwrm_cfa_pair_exists(bp, vfr);
	if (!rc)
		bnxt_hwrm_cfa_pair_free(bp, vfr);

	rc = bnxt_ulp_create_vfr_default_rules(vfr);
	if (rc) {
		netdev_dbg(bp->dev, "Failed to create VFR default rules\n");
		return rc;
	}

	rc = bnxt_hwrm_cfa_pair_alloc(bp, vfr);
	if (rc) {
		netdev_dbg(bp->dev, "CFA_PAIR_ALLOC hwrm command failed\n");
		return rc;
	}

	return 0;
}

int bnxt_ulp_alloc_vf_rep_p7(struct bnxt *bp, void *vfr)
{
	struct bnxt_vf_rep *vf_rep = vfr;
	u16 vfr_fid;
	int rc;

	rc = ulp_port_db_dev_port_intf_update(bp->ulp_ctx, bp, vf_rep);
	if (rc) {
		netdev_dbg(bp->dev, "Failed to update port database\n");
		return -EINVAL;
	}

	vfr_fid = bnxt_vfr_get_fw_func_id(vf_rep);
	rc = bnxt_hwrm_release_afm_func(bp,
					vfr_fid,
					bp->pf.fw_fid,
					CFA_RELEASE_AFM_FUNC_REQ_TYPE_EFID,
					0);

	if (rc) {
		netdev_dbg(bp->dev,
			   "Failed to release EFID:%d from RFID:%d rc=%d\n",
			   vfr_fid, bp->pf.fw_fid, rc);
		goto error_del_rules;
	}
	netdev_dbg(bp->dev, "Released EFID:%d from RFID:%d\n", vfr_fid, bp->pf.fw_fid);

	/* This will add the vfr endpoint to the session. */
	rc = bnxt_ulp_vfr_session_fid_add(bp->ulp_ctx, vfr_fid);
	if (rc)
		goto error_del_rules;
	else
		netdev_dbg(bp->dev, "VFR EFID %d created and initialized\n", vfr_fid);

	/* Create the VFR default rules once we've initialized the VF rep.  */
	rc = bnxt_ulp_create_vfr_default_rules(vf_rep);
	if (rc) {
		netdev_dbg(bp->dev, "Failed to create VFR default rules\n");
		return rc;
	}

	/* bnxt vfrep cfact update */
	vf_rep->dst = metadata_dst_alloc(0, METADATA_HW_PORT_MUX, GFP_KERNEL);
	if (!vf_rep->dst)
		return -ENOMEM;

	/* only cfa_action is needed to mux a packet while TXing */
	vf_rep->dst->u.port_info.port_id = vf_rep->tx_cfa_action;
	vf_rep->dst->u.port_info.lower_dev = bp->dev;

	/* disable TLS on the VFR */
	vf_rep->dev->hw_features &= ~(NETIF_F_HW_TLS_TX | NETIF_F_HW_TLS_RX);
	vf_rep->dev->features &= ~(NETIF_F_HW_TLS_TX | NETIF_F_HW_TLS_RX);

	return rc;

error_del_rules:
	(void)bnxt_ulp_delete_vfr_default_rules(vf_rep);
	return rc;
}

void bnxt_ulp_free_vf_rep(struct bnxt *bp, void *vfr)
{
	int rc;

	rc = bnxt_ulp_delete_vfr_default_rules(vfr);
	if (rc)
		netdev_dbg(bp->dev, "Failed to delete VFR default rules\n");

	bnxt_hwrm_cfa_pair_free(bp, vfr);
}

void bnxt_ulp_free_vf_rep_p7(struct bnxt *bp, void *vfr)
{
	u16 vfr_fid = bnxt_vfr_get_fw_func_id(vfr);
	int rc;

	rc = bnxt_ulp_delete_vfr_default_rules(vfr);
	if (rc)
		netdev_dbg(bp->dev, "Failed to delete VFR default rules\n");

	rc = bnxt_ulp_vfr_session_fid_rem(bp->ulp_ctx, vfr_fid);
	if (rc)
		netdev_dbg(bp->dev,
			   "Failed to remove  VFR EFID %d from session\n", vfr_fid);

	rc = ulp_flow_db_function_flow_flush(bp->ulp_ctx, vfr_fid);
	if (rc)
		netdev_dbg(bp->dev,
			   "Failed to flush flows for %d\n", vfr_fid);
}

/* Function to check if allowing multicast and broadcast flow offload. */
bool
bnxt_ulp_validate_bcast_mcast(struct bnxt *bp)
{
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

	/* app_id=0 supports mc/bc flow offload */
	if (app_id != 0)
		return false;

	return true;
}

/* This function sets the number of key recipes supported
 * Generally, this should be set to the number of flexible keys
 * supported
 */
void
bnxt_ulp_num_key_recipes_set(struct bnxt_ulp_context *ulp_ctx,
			     u16 num_recipes)
{
	if (!ulp_ctx || !ulp_ctx->cfg_data)
		return;
	ulp_ctx->cfg_data->num_key_recipes_per_dir = num_recipes;
}

/* This function gets the number of key recipes supported */
int
bnxt_ulp_num_key_recipes_get(struct bnxt_ulp_context *ulp_ctx)
{
	if (!ulp_ctx || !ulp_ctx->cfg_data)
		return 0;
	return ulp_ctx->cfg_data->num_key_recipes_per_dir;
}

/* This function gets the feature bits */
u64
bnxt_ulp_feature_bits_get(struct bnxt_ulp_context *ulp_ctx)
{
	if (!ulp_ctx || !ulp_ctx->cfg_data)
		return 0;
	return ulp_ctx->cfg_data->feature_bits;
}

/* Add the VF Rep endpoint to the session */
int
bnxt_ulp_vfr_session_fid_add(struct bnxt_ulp_context *ulp_ctx,
			     u16 vfr_fid)
{
	int rc = 0;

	if (!ulp_ctx || !ulp_ctx->ops)
		return -EINVAL;
	if (ulp_ctx->ops->ulp_vfr_session_fid_add)
		rc = ulp_ctx->ops->ulp_vfr_session_fid_add(ulp_ctx, vfr_fid);

	return rc;
}

/* Remove the VF Rep endpoint from the session */
int
bnxt_ulp_vfr_session_fid_rem(struct bnxt_ulp_context *ulp_ctx,
			     u16 vfr_fid)
{
	int rc = 0;

	if (!ulp_ctx || !ulp_ctx->ops)
		return -EINVAL;
	if (ulp_ctx->ops->ulp_vfr_session_fid_rem)
		rc = ulp_ctx->ops->ulp_vfr_session_fid_rem(ulp_ctx, vfr_fid);
	return rc;
}

int
bnxt_ulp_cap_feat_process(u64 feat_bits, u64 def_feat_bits, u64 *out_bits)
{
	u64 bit = TC_BNXT_TF_FEAT_BITS;

	*out_bits = 0;
	/* set default feature bits if none selected */
	if (!bit)
		bit = def_feat_bits;

	if ((feat_bits | bit) != feat_bits) {
		netdev_dbg(NULL, "Invalid TF feature bit is set %llu\n", bit);
		return -EINVAL;
	}
	if ((!!(bit & BNXT_ULP_FEATURE_BIT_PARENT_DMAC) +
	     !!(bit & BNXT_ULP_FEATURE_BIT_PORT_DMAC)) == 2) {
		netdev_dbg(NULL, "Invalid both Port and Parent Mac set\n");
		return -EINVAL;
	}

	if (bit & BNXT_ULP_FEATURE_BIT_PARENT_DMAC)
		netdev_dbg(NULL, "Parent Mac Address Feature is enabled\n");
	if (bit & BNXT_ULP_FEATURE_BIT_PORT_DMAC)
		netdev_dbg(NULL, "Port Mac Address Feature is enabled\n");
	if (bit & BNXT_ULP_FEATURE_BIT_MULTI_TUNNEL_FLOW)
		netdev_dbg(NULL, "Multi Tunnel Flow Feature is enabled\n");

	*out_bits =  bit;
	return 0;
}
#endif /* CONFIG_BNXT_FLOWER_OFFLOAD */
