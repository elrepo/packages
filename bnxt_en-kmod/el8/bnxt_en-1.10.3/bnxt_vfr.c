/* Broadcom NetXtreme-C/E network driver.
 *
 * Copyright (c) 2016-2018 Broadcom Limited
 * Copyright (c) 2018-2025 Broadcom Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 */
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/rtnetlink.h>
#include <linux/jhash.h>
#ifdef HAVE_TC_SETUP_TYPE
#include <net/pkt_cls.h>
#endif

#include "bnxt_compat.h"
#include "bnxt_hsi.h"
#include "bnxt.h"
#include "bnxt_hwrm.h"
#include "bnxt_sriov.h"
#include "bnxt_vfr.h"
#include "bnxt_devlink.h"
#include "bnxt_tc.h"
#include "bnxt_ulp_flow.h"
#include "bnxt_tf_common.h"
#include "bnxt_nic_flow.h"
#include "tfc.h"
#include "tfc_debug.h"
#include "ulp_nic_flow.h"

/* Synchronize TF ULP port operations.
 * TBD: Revisit this global lock and consider making this a per-adapter lock.
 */
DEFINE_MUTEX(tf_port_lock);

#if defined(CONFIG_VF_REPS) || defined(CONFIG_BNXT_CUSTOM_FLOWER_OFFLOAD)
/* This function removes a FID from the AFM session and designates whether
 * it is an endpoint or representor to the firmware based on the type field
 * passed into the HWRM message.
 */
int bnxt_hwrm_release_afm_func(struct bnxt *bp, u16 fid, u16 rfid,
			       u8 type, u32 flags)
{
	struct hwrm_cfa_release_afm_func_input *req;
	int rc;

	rc = hwrm_req_init(bp, req, HWRM_CFA_RELEASE_AFM_FUNC);

	req->fid = cpu_to_le16(fid);
	req->rfid = cpu_to_le16(rfid);
	req->flags = cpu_to_le16(flags);
	req->type = type;

	rc = hwrm_req_send(bp, req);
	return rc;
}

static int bnxt_tf_release_afm_func(struct bnxt *bp)
{
	int rc;

	if (BNXT_CHIP_P7(bp)) {
		/* Need to release the fid from AFM control
		 */
		rc = bnxt_hwrm_release_afm_func(bp, bp->pf.fw_fid,
						bp->pf.fw_fid,
						CFA_RELEASE_AFM_FUNC_REQ_TYPE_RFID,
						0);
		if (rc) {
			netdev_dbg(bp->dev, "Failed in hwrm release afm func:%u rc=%d\n",
				   bp->pf.fw_fid, rc);
			return rc;
		}
		netdev_dbg(bp->dev, "Released RFID:%d\n", bp->pf.fw_fid);
	}
	return 0;
}

/* For Thor2 only:
 * This function closes and re-opens the PF interface, to serve
 * the below two requirements.
 * - After tf_init(), vnic metadata_format must be set based on
 *   whether switchdev mode is enabled or not. If switchdev mode
 *   is enabled, it should be set to format 3.
 * - After tf_deinit(), NIC L2 filters should be recreated. These
 *   were freed by AFM on devlink enable when TF tookover the
 *   function.
 *
 * On Thor, resources are hard carved and L2 filters are not
 * removed when TF is running.
 */
static void bnxt_tf_toggle_netif(struct bnxt *bp)
{
	int rc;

	if (!BNXT_CHIP_P7(bp))
		return;

	netdev_lock(bp->dev);
	/* Close and re-open so that L2 filters are restored */
	if (netif_running(bp->dev)) {
		bnxt_close_nic(bp, false, false);
		rc = bnxt_open_nic(bp, false, false);
		if (rc)
			netdev_dbg(bp->dev, "re-open for filters failed(%d)\n", rc);
	}
	netdev_unlock(bp->dev);
}

static void bnxt_tf_l2_filter_populate(struct bnxt *bp)
{
	struct bnxt_vnic_info *vnic = &bp->vnic_info[BNXT_VNIC_DEFAULT];
	struct bnxt_l2_filter *fltr;
	int i, off;

	if (!BNXT_CHIP_P7(bp))
		return;

	if (!vnic) {
		netdev_dbg(bp->dev, "VNIC is NULL, skip populate\n");
		return;
	}

	mutex_lock(&bp->ntp_lock);
	for (i = 1, off = 0; i < vnic->uc_filter_count; i++, off += ETH_ALEN) {
		fltr = vnic->l2_filters[i];
		if (fltr)
			bnxt_tf_l2_filter_create(bp, fltr);
	}
	mutex_unlock(&bp->ntp_lock);
}

/* This function initializes Truflow feature which enables host based
 * flow offloads. The flag argument provides information about the TF
 * consumer and a reference to the consumer is set in bp->tf_flags.
 * The initialization is done only once when the first consumer calls
 * this function.
 */
int bnxt_tf_port_init(struct bnxt *bp, u16 flag)
{
	int rc;

	mutex_lock(&tf_port_lock);
	if ((flag & BNXT_TF_FLAG_SWITCHDEV || flag & BNXT_TF_FLAG_DEVLINK) &&
	    bnxt_get_current_flow_cnt(bp)) {
		netdev_err(bp->dev,
			   "Cleanup existing filters before changing the setting\n");
		rc = -EBUSY;
		goto skip;
	}

	/* Delete any veb filter first before creating */
	if (bp->tf_flags & BNXT_TF_FLAG_DEVLINK)
		bnxt_ulp_pf_veb_flow_delete(bp);

	if (bp->tf_flags & BNXT_TF_FLAG_INITIALIZED) {
		/* TF already initialized; just set the in-use flag
		 * for the specific consumer and return success.
		 */
		rc = 0;
		goto exit;
	}

	rc = bnxt_tf_release_afm_func(bp);
	if (rc)
		goto exit;

	rc = bnxt_ulp_port_init(bp);
	if (rc)
		goto exit;

	/* create the veb filter only if devlink is enabled */
	if ((!flag && bp->tf_flags & BNXT_TF_FLAG_DEVLINK &&
	     ((bp->tf_flags & BNXT_TF_FLAG_SWITCHDEV) == 0)) ||
	    flag & BNXT_TF_FLAG_DEVLINK)
		rc = bnxt_ulp_pf_veb_flow_create(bp);
exit:
	if (!rc) {
		bp->tf_flags |= flag;
		if (!(bp->tf_flags & BNXT_TF_FLAG_INITIALIZED))
			bp->tf_flags |= BNXT_TF_FLAG_INITIALIZED;
	} else {
		netdev_err(bp->dev, "Failed to initialize Truflow feature rc=%d\n", rc);
	}
skip:
	mutex_unlock(&tf_port_lock);
	if (!rc && flag == BNXT_TF_FLAG_SWITCHDEV)
		bnxt_tf_toggle_netif(bp);

	if (!rc)
		bnxt_tf_l2_filter_populate(bp);
	return rc;
}

/* regardless of truflow mode on this port the table
 * scope memory should be cleaned up on a PF when either
 * a func reset (due to caps change) on the PF occurs
 * or firmware reset because table scope memory allocated
 * on behalf of child DPDK VFs is done in the kernel PF
 * L2 driver.  This should only cleanup non-local table
 * scopes.  Local table scopes are created when truflow
 * mode is enabled.
 */
void bnxt_tf_tbl_scope_cleanup(struct bnxt *bp)
{
	if (BNXT_CHIP_P7(bp) && BNXT_PF(bp))
		tfc_tbl_scope_cleanup(bp->tfp);
}

/* regardless of truflow mode on this port the tf object
 * should be reinit for this port.  The tf object is required
 * on a PF to support DPDK table scope creation for child VFs
 * in DPDK.  These DPDK child VF table scopes are not locally
 * owned.
 */
int bnxt_tfo_init(struct bnxt *bp)
{
	int rc = 0;

	if (BNXT_CHIP_P7(bp) && BNXT_PF(bp)) {
		mutex_lock(&tf_port_lock);
		rc = bnxt_ulp_tfo_init(bp);
		if (rc)
			netdev_err(bp->dev, "Failed to allocate Truflow structure\n");
		mutex_unlock(&tf_port_lock);
	}
	return rc;
}

/* regardless of truflow mode on this port the tf object
 * should be deinit for this port when either a func_reset
 * (due to caps change) or a firmware reset.
 */
void bnxt_tfo_deinit(struct bnxt *bp)
{
	if (BNXT_CHIP_P7(bp) && BNXT_PF(bp)) {
		mutex_lock(&tf_port_lock);
		bnxt_ulp_tfo_deinit(bp);
		mutex_unlock(&tf_port_lock);
	}
}

static bool bnxt_is_tf_busy(struct bnxt *bp)
{
	return (bp->tf_flags &
		(BNXT_TF_FLAG_SWITCHDEV |
		 BNXT_TF_FLAG_DEVLINK));
}

/* Uninitialize TF. The flag argument represents the TF consumer
 * so that the reference held in bp->tf_flags earlier can be
 * released. TF is uninitialized when there are no more active
 * consumers. The flag value of NONE(0) overrides this logic and
 * uninits regardless of any active consumers (e.g during rmmod).
 */
void bnxt_tf_port_deinit(struct bnxt *bp, u16 flag)
{
	mutex_lock(&tf_port_lock);

	/* Not initialized; nothing to do */
	if (!(bp->tf_flags & BNXT_TF_FLAG_INITIALIZED))
		goto done;

	/* Clear in-use flag for the specific consumer */
	if (flag)
		bp->tf_flags &= ~flag;

	/* Are there other TF consumers? */
	if (bnxt_is_tf_busy(bp) && flag) {
		/* Current mode is switchdev transistion to devlink mode */
		/* will happen when legacy mode is enabled */
		if (bp->tf_flags & BNXT_TF_FLAG_DEVLINK)
			bnxt_ulp_pf_veb_flow_create(bp);
		goto done;
	}

	/* Ok to deinit */
	bnxt_ulp_pf_veb_flow_delete(bp);
	bnxt_ulp_port_deinit(bp);
	bp->tf_flags &= ~BNXT_TF_FLAG_INITIALIZED;

done:
	mutex_unlock(&tf_port_lock);
	if (flag != BNXT_TF_FLAG_NONE)
		bnxt_tf_toggle_netif(bp);
}

void bnxt_tf_devlink_toggle(struct bnxt *bp)
{
	if (BNXT_PF(bp) && BNXT_TRUFLOW_EN(bp) &&
	    (bp->tf_flags & BNXT_TF_FLAG_DEVLINK)) {
		/* LAG transitions require TF toggle */
		bnxt_tf_port_deinit(bp, BNXT_TF_FLAG_NONE);
		bnxt_tf_port_init(bp, BNXT_TF_FLAG_NONE);
		netdev_dbg(bp->dev, "devlink truflow toggled\n");
	}
}

void bnxt_custom_tf_port_init(struct bnxt *bp)
{
#ifdef CONFIG_BNXT_CUSTOM_FLOWER_OFFLOAD
	if (bnxt_tc_is_switchdev_mode(bp))
		return;
	if (BNXT_PF(bp) && BNXT_TRUFLOW_EN(bp))
		bnxt_tf_port_init(bp, BNXT_TF_FLAG_NONE);
#endif
	return;
}

void bnxt_custom_tf_port_deinit(struct bnxt *bp)
{
#ifdef CONFIG_BNXT_CUSTOM_FLOWER_OFFLOAD
	if (bnxt_tc_is_switchdev_mode(bp))
		return;
	if (BNXT_PF(bp) && BNXT_TRUFLOW_EN(bp))
		bnxt_tf_port_deinit(bp, BNXT_TF_FLAG_NONE);
#endif
	return;
}

int bnxt_devlink_tf_port_init(struct bnxt *bp)
{
	if (bp->dl_param_truflow)
		return 0;

	if (BNXT_PF(bp) && BNXT_TRUFLOW_EN(bp))
		return bnxt_tf_port_init(bp, BNXT_TF_FLAG_DEVLINK);

	return -EOPNOTSUPP;
}

void bnxt_devlink_tf_port_deinit(struct bnxt *bp)
{
	if (!bp->dl_param_truflow)
		return;

	if (BNXT_PF(bp) && BNXT_TRUFLOW_EN(bp))
		bnxt_tf_port_deinit(bp, BNXT_TF_FLAG_DEVLINK);
}

#endif

#ifdef CONFIG_VF_REPS

#define CFA_HANDLE_INVALID		0xffff
#define VF_IDX_INVALID			0xffff

static int hwrm_cfa_vfr_alloc(struct bnxt *bp, u16 vf_idx,
			      u32 *tx_cfa_action, u16 *rx_cfa_code)
{
	struct hwrm_cfa_vfr_alloc_output *resp;
	struct hwrm_cfa_vfr_alloc_input *req;
	int rc;

	rc = hwrm_req_init(bp, req, HWRM_CFA_VFR_ALLOC);
	if (!rc) {
		req->vf_id = cpu_to_le16(vf_idx);
		sprintf(req->vfr_name, "vfr%d", vf_idx);

		resp = hwrm_req_hold(bp, req);
		rc = hwrm_req_send(bp, req);
		if (!rc) {
			*tx_cfa_action = le16_to_cpu(resp->tx_cfa_action);
			*rx_cfa_code = le16_to_cpu(resp->rx_cfa_code);
			netdev_dbg(bp->dev, "tx_cfa_action=0x%x, rx_cfa_code=0x%x",
				   *tx_cfa_action, *rx_cfa_code);
		}
		hwrm_req_drop(bp, req);
	}
	if (rc)
		netdev_info(bp->dev, "%s error rc=%d\n", __func__, rc);
	return rc;
}

static int hwrm_cfa_vfr_free(struct bnxt *bp, u16 vf_idx)
{
	struct hwrm_cfa_vfr_free_input *req;
	int rc;

	rc = hwrm_req_init(bp, req, HWRM_CFA_VFR_FREE);
	if (!rc) {
		sprintf(req->vfr_name, "vfr%d", vf_idx);
		rc = hwrm_req_send(bp, req);
	}
	if (rc)
		netdev_info(bp->dev, "%s error rc=%d\n", __func__, rc);
	return rc;
}

static int bnxt_hwrm_vfr_qcfg(struct bnxt *bp, struct bnxt_vf_rep *vf_rep,
			      u16 *max_mtu)
{
	struct hwrm_func_qcfg_output *resp;
	struct hwrm_func_qcfg_input *req;
	struct bnxt_vf_info *vf;
	u16 mtu;
	int rc;

	rc = hwrm_req_init(bp, req, HWRM_FUNC_QCFG);
	if (rc)
		return rc;

	rcu_read_lock();
	vf = rcu_dereference(bp->pf.vf);
	if (!vf) {
		rcu_read_unlock();
		return -EINVAL;
	}
	req->fid = vf[vf_rep->vf_idx].fw_fid;
	rcu_read_unlock();
	resp = hwrm_req_hold(bp, req);
	rc = hwrm_req_send(bp, req);
	if (!rc) {
		mtu = le16_to_cpu(resp->max_mtu_configured);
		if (!mtu)
			*max_mtu = BNXT_MAX_MTU;
		else
			*max_mtu = mtu;
	}
	hwrm_req_drop(bp, req);

	return rc;
}

static int bnxt_vf_rep_open(struct net_device *dev)
{
	struct bnxt_vf_rep *vf_rep = netdev_priv(dev);
	struct bnxt *bp = vf_rep->bp;

	/* Enable link and TX only if the parent PF is open. */
	if (netif_running(bp->dev)) {
		netif_carrier_on(dev);
		netif_tx_start_all_queues(dev);
	}
	return 0;
}

static int bnxt_vf_rep_close(struct net_device *dev)
{
	netif_carrier_off(dev);
	netif_tx_disable(dev);

	return 0;
}

static netdev_tx_t bnxt_vf_rep_xmit(struct sk_buff *skb,
				    struct net_device *dev)
{
	struct bnxt_vf_rep *vf_rep = netdev_priv(dev);
	int rc, len = skb->len;

	skb_dst_drop(skb);
	dst_hold((struct dst_entry *)vf_rep->dst);
	skb_dst_set(skb, (struct dst_entry *)vf_rep->dst);
	skb->dev = vf_rep->dst->u.port_info.lower_dev;

	rc = dev_queue_xmit(skb);
	if (!rc) {
		vf_rep->tx_stats.packets++;
		vf_rep->tx_stats.bytes += len;
	}
	return rc;
}

static void bnxt_vf_rep_get_stats64(struct net_device *dev,
				    struct rtnl_link_stats64 *stats)
{
	struct bnxt_vf_rep *vf_rep = netdev_priv(dev);

	if (!vf_rep || !vf_rep->bp)
		return;

	stats->rx_packets = vf_rep->rx_stats.packets;
	stats->rx_bytes = vf_rep->rx_stats.bytes;
	stats->tx_packets = vf_rep->tx_stats.packets;
	stats->tx_bytes = vf_rep->tx_stats.bytes;
}

#ifdef CONFIG_BNXT_FLOWER_OFFLOAD
#ifdef HAVE_TC_SETUP_TYPE
#ifdef HAVE_TC_SETUP_BLOCK
static LIST_HEAD(bnxt_vf_block_cb_list);

static int bnxt_vf_rep_setup_tc_block_cb(enum tc_setup_type type,
					 void *type_data,
					 void *cb_priv)
{
	struct bnxt_vf_rep *vf_rep = cb_priv;
	struct bnxt *bp = vf_rep->bp;
	u16 vf_fid;

	vf_fid = bnxt_vf_target_id(&bp->pf, vf_rep->vf_idx);
	if (vf_fid == INVALID_HW_RING_ID)
		return -EINVAL;

	if (!bnxt_tc_flower_enabled(vf_rep->bp))
		return -EOPNOTSUPP;

	switch (type) {
	case TC_SETUP_CLSFLOWER:
#ifdef HAVE_TC_CB_EGDEV
		return bnxt_tc_setup_flower(bp, vf_fid, type_data,
					    BNXT_TC_DEV_INGRESS);
#else
		return bnxt_tc_setup_flower(bp, vf_fid, type_data);
#endif

#if defined(HAVE_TC_MATCHALL_FLOW_RULE) && defined(HAVE_FLOW_ACTION_POLICE)
	case TC_SETUP_CLSMATCHALL:
		return bnxt_tc_setup_matchall(bp, vf_fid, type_data);
#endif
	default:
		return -EOPNOTSUPP;
	}
}

#endif /* HAVE_TC_SETUP_BLOCK */

static int bnxt_vf_rep_setup_tc(struct net_device *dev, enum tc_setup_type type,
				void *type_data)
{
	struct bnxt_vf_rep *vf_rep = netdev_priv(dev);

	switch (type) {
#ifdef HAVE_TC_SETUP_BLOCK
	case TC_SETUP_BLOCK:
		return flow_block_cb_setup_simple(type_data,
						  &bnxt_vf_block_cb_list,
						  bnxt_vf_rep_setup_tc_block_cb,						  vf_rep, vf_rep, true);
#else
	case TC_SETUP_CLSFLOWER: {
		struct bnxt *bp = vf_rep->bp;
		u16 vf_fid;

		vf_fid = bnxt_vf_target_id(&bp->pf, vf_rep->vf_idx);
		if (vf_fid == INVALID_HW_RING_ID)
			return -EINVAL;
#ifdef HAVE_TC_CB_EGDEV
		return bnxt_tc_setup_flower(bp, vf_fid, type_data,
					    BNXT_TC_DEV_INGRESS);
#else
		return bnxt_tc_setup_flower(bp, vf_fid, type_data);
#endif
	}
#endif
	default:
		return -EOPNOTSUPP;
	}
}

#else /* HAVE_TC_SETUP_TYPE */

#ifdef HAVE_CHAIN_INDEX
static int bnxt_vf_rep_setup_tc(struct net_device *dev, u32 handle,
				u32 chain_index, __be16 proto,
				struct tc_to_netdev *ntc)
#else
static int bnxt_vf_rep_setup_tc(struct net_device *dev, u32 handle,
				__be16 proto, struct tc_to_netdev *ntc)
#endif
{
	struct bnxt_vf_rep *vf_rep = netdev_priv(dev);
	u16 vf_fid;

	vf_fid = bnxt_vf_target_id(&vf_rep->bp->pf, vf_rep->vf_idx);
	if (vf_fid == INVALID_HW_RING_ID)
		return -EINVAL;

	if (!bnxt_tc_flower_enabled(vf_rep->bp))
		return -EOPNOTSUPP;

	switch (ntc->type) {
	case TC_SETUP_CLSFLOWER:
#ifdef HAVE_TC_CB_EGDEV
		return bnxt_tc_setup_flower(vf_rep->bp,
					    vf_fid,
					    ntc->cls_flower,
					    BNXT_TC_DEV_INGRESS);
#else
		return bnxt_tc_setup_flower(vf_rep->bp,
					    vf_fid,
					    ntc->cls_flower);
#endif
	default:
		return -EOPNOTSUPP;
	}
}
#endif /* HAVE_TC_SETUP_TYPE */

#ifdef HAVE_TC_CB_EGDEV
static int bnxt_vf_rep_tc_cb_egdev(enum tc_setup_type type, void *type_data,
				   void *cb_priv)
{
	struct bnxt_vf_rep *vf_rep = cb_priv;
	struct bnxt *bp = vf_rep->bp;
	u16 vf_fid;

	vf_fid = bnxt_vf_target_id(&bp->pf, vf_rep->vf_idx);
	if (vf_fid == INVALID_HW_RING_ID)
		return -EINVAL;

	if (!bnxt_tc_flower_enabled(vf_rep->bp))
		return -EOPNOTSUPP;

	switch (type) {
	case TC_SETUP_CLSFLOWER:
		return bnxt_tc_setup_flower(bp, vf_fid, type_data,
					    BNXT_TC_DEV_EGRESS);
	default:
		return -EOPNOTSUPP;
	}
}
#else

#ifdef HAVE_TC_SETUP_TYPE
static int bnxt_vf_rep_tc_cb_egdev(enum tc_setup_type type, void *type_data,
				   void *cb_priv)
#else
static int bnxt_vf_rep_tc_cb_egdev(int type, void *type_data, void *cb_priv)
#endif /* HAVE_TC_SETUP_TYPE */
{
	return 0;
}
#endif /* HAVE_TC_CB_EGDEV */

#define bnxt_cb_egdev		((void *)bnxt_vf_rep_tc_cb_egdev)

#endif /* CONFIG_BNXT_FLOWER_OFFLOAD */

struct net_device *bnxt_get_vf_rep(struct bnxt *bp, u16 cfa_code)
{
	u16 vf_idx;

	if (cfa_code && bp->cfa_code_map && BNXT_PF(bp)) {
		vf_idx = bp->cfa_code_map[cfa_code];
		if (vf_idx != VF_IDX_INVALID)
			return bp->vf_reps[vf_idx]->dev;
	}
	return NULL;
}

struct net_device *bnxt_tf_get_vf_rep(struct bnxt *bp,
				      struct rx_cmp_ext *rxcmp1,
				      struct bnxt_tpa_info *tpa_info)
{
	u32 mark_id = 0;
	u16 vf_idx;
	int rc;

	if (bp->cfa_code_map && BNXT_PF(bp)) {
		if (BNXT_CHIP_P7(bp))
			rc = bnxt_ulp_get_mark_from_cfacode_p7(bp, rxcmp1, tpa_info,
							       &mark_id);
		else
			rc = bnxt_ulp_get_mark_from_cfacode(bp, rxcmp1, tpa_info,
							    &mark_id);
		if (rc)
			return NULL;
		/* mark_id is endpoint vf's fw fid */
		vf_idx = bp->cfa_code_map[mark_id];
		if (vf_idx != VF_IDX_INVALID)
			return bp->vf_reps[vf_idx]->dev;
	}

	return NULL;
}

void bnxt_vf_rep_rx(struct bnxt *bp, struct sk_buff *skb)
{
	struct bnxt_vf_rep *vf_rep = netdev_priv(skb->dev);

	vf_rep->rx_stats.bytes += skb->len;
	vf_rep->rx_stats.packets++;

	netif_receive_skb(skb);
}

static int bnxt_vf_rep_get_phys_port_name(struct net_device *dev, char *buf,
					  size_t len)
{
	struct bnxt_vf_rep *vf_rep = netdev_priv(dev);
	int rc;

	if (!vf_rep || !vf_rep->bp || !vf_rep->bp->pdev)
		return -EINVAL;

	rc = snprintf(buf, len, "pf%dvf%d", vf_rep->bp->pf.fw_fid - 1,
		      vf_rep->vf_idx);
	if (rc >= len)
		return -EOPNOTSUPP;

	return 0;
}

static void bnxt_vf_rep_get_drvinfo(struct net_device *dev,
				    struct ethtool_drvinfo *info)
{
	strscpy(info->driver, DRV_MODULE_NAME, sizeof(info->driver));
	strscpy(info->version, DRV_MODULE_VERSION, sizeof(info->version));
}

#ifdef HAVE_NDO_GET_PORT_PARENT_ID
static int bnxt_vf_rep_get_port_parent_id(struct net_device *dev,
					  struct netdev_phys_item_id *ppid)
{
	struct bnxt_vf_rep *vf_rep = netdev_priv(dev);

	/* as only PORT_PARENT_ID is supported currently use common code
	 * between PF and VF-rep for now.
	 */
	return bnxt_get_port_parent_id(vf_rep->bp->dev, ppid);
}

#else

static int bnxt_vf_rep_port_attr_get(struct net_device *dev,
				     struct switchdev_attr *attr)
{
	struct bnxt_vf_rep *vf_rep = netdev_priv(dev);

	/* as only PORT_PARENT_ID is supported currently use common code
	 * between PF and VF-rep for now.
	 */
	return bnxt_port_attr_get(vf_rep->bp, attr);
}

static const struct switchdev_ops bnxt_vf_rep_switchdev_ops = {
	.switchdev_port_attr_get	= bnxt_vf_rep_port_attr_get
};
#endif

static const char *const bnxt_vf_rep_stats_str[] = {
	"vport_rx_packets",
	"vport_rx_bytes",
	"vport_tx_packets",
	"vport_tx_bytes",
	"vport_rx_errors",
	"vport_rx_discards",
	"vport_tx_discards",
	"vport_rx_tpa_pkt",
	"vport_rx_tpa_bytes",
	"vport_rx_tpa_errors"
};

#define	BNXT_VF_REP_NUM_COUNTERS	ARRAY_SIZE(bnxt_vf_rep_stats_str)
static int bnxt_get_vf_rep_sset_count(struct net_device *dev, int sset)
{
	switch (sset) {
	case ETH_SS_STATS:
		return BNXT_VF_REP_NUM_COUNTERS;
	default:
		return -EOPNOTSUPP;
	}
}

static void bnxt_get_vf_rep_strings(struct net_device *dev, u32 stringset, u8 *buf)
{
	u32 i;

	switch (stringset) {
	case ETH_SS_STATS:
		for (i = 0; i < BNXT_VF_REP_NUM_COUNTERS; i++) {
			sprintf(buf, "%s", bnxt_vf_rep_stats_str[i]);
			buf += ETH_GSTRING_LEN;
		}
		break;
	default:
		netdev_err(dev, "%s invalid request %x\n", __func__, stringset);
		break;
	}
}

static void __bnxt_get_vf_rep_ethtool_stats(u64 *buf, u64 *sw)
{
	buf[0] += bnxt_add_ring_rx_pkts(sw);
	buf[1] += bnxt_add_ring_rx_bytes(sw);
	buf[2] += bnxt_add_ring_tx_pkts(sw);
	buf[3] += bnxt_add_ring_tx_bytes(sw);
	buf[4] += BNXT_GET_EXT_RING_STATS64(sw, rx_error_pkts);
	buf[5] += BNXT_GET_EXT_RING_STATS64(sw, rx_discard_pkts);
	buf[6] += BNXT_GET_EXT_RING_STATS64(sw, tx_error_pkts) +
			BNXT_GET_EXT_RING_STATS64(sw, tx_discard_pkts);
	buf[7] += BNXT_GET_EXT_RING_STATS64(sw, rx_tpa_pkt);
	buf[8] += BNXT_GET_EXT_RING_STATS64(sw, rx_tpa_bytes);
	buf[9] += BNXT_GET_EXT_RING_STATS64(sw, rx_tpa_errors);
}

static void bnxt_get_vf_rep_ethtool_stats(struct net_device *dev,
					  struct ethtool_stats *stats, u64 *buf)
{
	int buf_size = BNXT_VF_REP_NUM_COUNTERS * sizeof(u64);
	struct bnxt_vf_rep *vf_rep = netdev_priv(dev);
	struct bnxt_vf_stat_ctx *ctx;
	struct bnxt_vf_info *vf;
	u64 *sw;

	if (!vf_rep || !vf_rep->bp)
		return;

	memset(buf, 0, buf_size);

	rcu_read_lock();
	vf = rcu_dereference(vf_rep->bp->pf.vf);
	if (!vf) {
		rcu_read_unlock();
		return;
	}

	if (vf_rep->bp->stats_cap & BNXT_STATS_CAP_VF_EJECTION) {
		list_for_each_entry_rcu(ctx,
					&vf[vf_rep->vf_idx].stat_ctx_list,
					node) {
			sw = ctx->stats.sw_stats;
			__bnxt_get_vf_rep_ethtool_stats(buf, sw);
		}
	} else {
		sw = vf[vf_rep->vf_idx].stats.sw_stats;
		__bnxt_get_vf_rep_ethtool_stats(buf, sw);
	}
	rcu_read_unlock();
}

static const struct ethtool_ops bnxt_vf_rep_ethtool_ops = {
	.get_drvinfo		= bnxt_vf_rep_get_drvinfo,
	.get_ethtool_stats      = bnxt_get_vf_rep_ethtool_stats,
	.get_strings            = bnxt_get_vf_rep_strings,
	.get_sset_count         = bnxt_get_vf_rep_sset_count,
};

static const struct net_device_ops bnxt_vf_rep_netdev_ops = {
#ifdef HAVE_NDO_SETUP_TC_RH
	.ndo_size               = sizeof(const struct net_device_ops),
#endif
	.ndo_open		= bnxt_vf_rep_open,
	.ndo_stop		= bnxt_vf_rep_close,
	.ndo_start_xmit		= bnxt_vf_rep_xmit,
	.ndo_get_stats64	= bnxt_vf_rep_get_stats64,
#ifdef CONFIG_BNXT_FLOWER_OFFLOAD
#ifdef HAVE_NDO_SETUP_TC_RH
	.extended.ndo_setup_tc_rh	= bnxt_vf_rep_setup_tc,
#else
	.ndo_setup_tc		= bnxt_vf_rep_setup_tc,
#endif
#endif
#ifdef HAVE_NDO_GET_PORT_PARENT_ID
	.ndo_get_port_parent_id	= bnxt_vf_rep_get_port_parent_id,
#endif
#ifdef HAVE_EXT_GET_PHYS_PORT_NAME
	.extended.ndo_get_phys_port_name = bnxt_vf_rep_get_phys_port_name
#else
	.ndo_get_phys_port_name = bnxt_vf_rep_get_phys_port_name
#endif
};

bool bnxt_dev_is_vf_rep(struct net_device *dev)
{
	return dev->netdev_ops == &bnxt_vf_rep_netdev_ops;
}

bool bnxt_tf_can_enable_vf_trust(struct bnxt *bp)
{
	return bnxt_ulp_can_enable_vf_trust(bp);
}

int bnxt_tf_config_promisc_mirror(struct bnxt *bp, struct bnxt_vnic_info *vnic)
{
	bool stat = false;

	/* If roce mirror cap is enabled then roce driver enables packet capture */
	if (BNXT_MIRROR_ON_ROCE_CAP(bp))
		return 0;

	/* Conditional check for promiscuous mode on/off */
	if ((vnic->rx_mask & CFA_L2_SET_RX_MASK_REQ_MASK_PROMISCUOUS))
		stat = true;

	/* RoCE mirror enable/disable processing on Thor/Thor2 */
	return bnxt_ulp_set_mirror(bp, stat);
}

void bnxt_tf_force_mirror_en_cfg(struct bnxt *bp, bool enable)
{
	if (BNXT_CHIP_P7(bp))
		bnxt_tf_ulp_force_mirror_en_cfg_p7(bp, enable);
}

void bnxt_tf_force_mirror_en_get(struct bnxt *bp, bool *tf_en,
				 bool *force_mirror_en)
{
	if (BNXT_CHIP_P7(bp))
		bnxt_tf_ulp_force_mirror_en_get_p7(bp, tf_en, force_mirror_en);
}

int bnxt_hwrm_cfa_pair_exists(struct bnxt *bp, void *vfr)
{
	struct hwrm_cfa_pair_info_output *resp;
	struct hwrm_cfa_pair_info_input *req;
	struct bnxt_vf_rep *rep_bp = vfr;
	int rc;

	if (!(BNXT_PF(bp) || BNXT_VF_IS_TRUSTED(bp))) {
		netdev_dbg(bp->dev,
			   "Not a PF or trusted VF. Command not supported\n");
		return -EOPNOTSUPP;
	}

	rc = hwrm_req_init(bp, req, HWRM_CFA_PAIR_INFO);
	if (rc)
		return rc;

	rc = snprintf(req->pair_name, sizeof(req->pair_name), "%svfr%d",
		      dev_name(rep_bp->bp->dev->dev.parent), rep_bp->vf_idx);

	if (rc >= sizeof(req->pair_name) || rc < 0)
		return -EINVAL;

	req->flags = cpu_to_le32(CFA_PAIR_INFO_REQ_FLAGS_LOOKUP_TYPE);

	resp = hwrm_req_hold(bp, req);
	rc = hwrm_req_send(bp, req);
	/* CFA_PAIR_EXISTS command will succeed even though there is no
	 * CFA_PAIR, the proper check to see if CFA_PAIR exists or not
	 * is to look at the resp->pair_name.
	 */
	if (!rc && !strlen(resp->pair_name))
		rc = -EINVAL;
	hwrm_req_drop(bp, req);

	return rc;
}

int bnxt_hwrm_cfa_pair_free(struct bnxt *bp, void *vfr)
{
	struct hwrm_cfa_pair_free_input *req;
	struct bnxt_vf_rep *rep_bp = vfr;
	int rc;

	if (!(BNXT_PF(bp) || BNXT_VF_IS_TRUSTED(bp))) {
		netdev_dbg(bp->dev,
			   "Not a PF or trusted VF. Command not supported\n");
		return 0;
	}

	rc = hwrm_req_init(bp, req, HWRM_CFA_PAIR_FREE);
	if (rc)
		return rc;

	rc = snprintf(req->pair_name, sizeof(req->pair_name), "%svfr%d",
		      dev_name(rep_bp->bp->dev->dev.parent), rep_bp->vf_idx);

	if (rc >= sizeof(req->pair_name) || rc < 0)
		return -EINVAL;

	req->pair_mode = cpu_to_le16(CFA_PAIR_FREE_REQ_PAIR_MODE_REP2FN_TRUFLOW);
	req->pf_b_id = rep_bp->bp->pf.fw_fid - 1;
	req->vf_id =  cpu_to_le16(rep_bp->vf_idx);

	rc = hwrm_req_send(bp, req);
	if (rc)
		return rc;

	netdev_dbg(bp->dev, "VFR %d freed\n", rep_bp->vf_idx);
	return 0;
}

static void __bnxt_tf_free_one_vf_rep(struct bnxt *bp,
				      struct bnxt_vf_rep *vf_rep)
{
	if (BNXT_CHIP_P7(bp))
		bnxt_ulp_free_vf_rep_p7(bp, vf_rep);
	else
		bnxt_ulp_free_vf_rep(bp, vf_rep);
}

/* Called when the parent PF interface is closed. */
void bnxt_vf_reps_close(struct bnxt *bp)
{
	struct bnxt_vf_rep *vf_rep;
	u16 num_vfs, i;

	if (!bnxt_tc_is_switchdev_mode(bp))
		return;

	if (!bp->cfa_code_map)
		return;

	num_vfs = pci_num_vf(bp->pdev);
	for (i = 0; i < num_vfs; i++) {
		if (bnxt_is_trusted_vf(bp, &bp->pf.vf[i]))
			continue;
		vf_rep = bp->vf_reps[i];
		if (netif_running(vf_rep->dev))
			bnxt_vf_rep_close(vf_rep->dev);
	}
}

/* Called when the parent PF interface is opened (re-opened) */
void bnxt_vf_reps_open(struct bnxt *bp)
{
	int i;

	if (!bnxt_tc_is_switchdev_mode(bp))
		return;

	if (!bp->cfa_code_map)
		return;

	for (i = 0; i < pci_num_vf(bp->pdev); i++) {
		/* Skip the iteration if vf is trusted */
		if (bnxt_is_trusted_vf(bp, &bp->pf.vf[i]))
			continue;
		/* Open the VF-Rep only if it is allocated in the FW */
		if (bp->vf_reps[i]->tx_cfa_action != CFA_HANDLE_INVALID)
			bnxt_vf_rep_open(bp->vf_reps[i]->dev);
	}
}

static void __bnxt_free_one_vf_rep(struct bnxt *bp, struct bnxt_vf_rep *vf_rep)
{
	if (!vf_rep)
		return;

	if (vf_rep->dst) {
		dst_release((struct dst_entry *)vf_rep->dst);
		vf_rep->dst = NULL;
	}
	if (vf_rep->tx_cfa_action != CFA_HANDLE_INVALID) {
		if (BNXT_TRUFLOW_EN(bp))
			__bnxt_tf_free_one_vf_rep(bp, vf_rep);
		else
			hwrm_cfa_vfr_free(bp, vf_rep->vf_idx);
		vf_rep->tx_cfa_action = CFA_HANDLE_INVALID;
	}
}

static void __bnxt_vf_reps_destroy(struct bnxt *bp)
{
	u16 num_vfs = pci_num_vf(bp->pdev);
	struct bnxt_vf_rep *vf_rep;
	int i;

	for (i = 0; i < num_vfs; i++) {
		vf_rep = bp->vf_reps[i];
		if (vf_rep) {
			__bnxt_free_one_vf_rep(bp, vf_rep);
			if (vf_rep->dev) {
				/* if register_netdev failed, then netdev_ops
				 * would have been set to NULL
				 */
				if (vf_rep->dev->netdev_ops) {
#ifdef CONFIG_BNXT_FLOWER_OFFLOAD
					bnxt_unreg_egdev(vf_rep->dev,
							 bnxt_cb_egdev,
							 (void *)vf_rep);
#endif
					unregister_netdev(vf_rep->dev);
				}
				free_netdev(vf_rep->dev);
			}
			bp->vf_reps[i] = NULL;
		}
	}

	kfree(bp->vf_reps);
	bp->vf_reps = NULL;
}

void bnxt_vf_reps_destroy(struct bnxt *bp)
{
	bool closed = false;

	if (!bnxt_tc_is_switchdev_mode(bp))
		return;

	if (!bp->vf_reps)
		return;

	/* Ensure that parent PF's and VF-reps' RX/TX has been quiesced
	 * before proceeding with VF-rep cleanup.
	 */
	netdev_lock(bp->dev);
	if (netif_running(bp->dev)) {
		bnxt_close_nic(bp, false, false);
		closed = true;
	}
	/* un-publish cfa_code_map so that RX path can't see it anymore */
	kfree(bp->cfa_code_map);
	bp->cfa_code_map = NULL;

	if (closed) {
		/* Temporarily set legacy mode to avoid re-opening
		 * representors and restore switchdev mode after that.
		 */
		bp->eswitch_mode = DEVLINK_ESWITCH_MODE_LEGACY;
		bnxt_open_nic(bp, false, false);
		bp->eswitch_mode = DEVLINK_ESWITCH_MODE_SWITCHDEV;
	}
	netdev_unlock(bp->dev);

	/* Need to call vf_reps_destroy() outside of netdev instance lock
	 * as unregister_netdev takes it
	 */
	__bnxt_vf_reps_destroy(bp);
}

/* Free the VF-Reps in firmware, during firmware hot-reset processing.
 * Note that the VF-Rep netdevs are still active (not unregistered) during
 * this process.
 */
void bnxt_vf_reps_free(struct bnxt *bp)
{
	u16 num_vfs = pci_num_vf(bp->pdev);
	int i;

	if (!bnxt_tc_is_switchdev_mode(bp))
		return;

	for (i = 0; i < num_vfs; i++)
		__bnxt_free_one_vf_rep(bp, bp->vf_reps[i]);
}

int bnxt_hwrm_cfa_pair_alloc(struct bnxt *bp, void *vfr)
{
	struct hwrm_cfa_pair_alloc_input *req;
	struct bnxt_vf_rep *rep_bp = vfr;
	int rc;

	if (!(BNXT_PF(bp) || BNXT_VF_IS_TRUSTED(bp))) {
		netdev_dbg(bp->dev,
			   "Not a PF or trusted VF. Command not supported\n");
		return -EINVAL;
	}

	rc = hwrm_req_init(bp, req, HWRM_CFA_PAIR_ALLOC);
	if (rc)
		return rc;

	req->pair_mode = cpu_to_le16(CFA_PAIR_ALLOC_REQ_PAIR_MODE_REP2FN_TRUFLOW);
	rc = snprintf(req->pair_name, sizeof(req->pair_name), "%svfr%d",
		      dev_name(rep_bp->bp->dev->dev.parent), rep_bp->vf_idx);

	if (rc >= sizeof(req->pair_name) || rc < 0)
		return -EINVAL;

	req->pf_b_id = rep_bp->bp->pf.fw_fid - 1;
	req->vf_b_id =  cpu_to_le16(rep_bp->vf_idx);
	req->vf_a_id = cpu_to_le16(rep_bp->bp->pf.fw_fid);
	req->host_b_id = 1; /* TBD - Confirm if this is OK */

	rc = hwrm_req_send(bp, req);
	if (rc)
		return rc;

	netdev_dbg(bp->dev, "VFR %d allocated\n", rep_bp->vf_idx);
	return rc;
}

static int bnxt_alloc_tf_vf_rep(struct bnxt *bp, struct bnxt_vf_rep *vf_rep,
				u16 *cfa_code_map)
{
	struct bnxt_vf_info *vf;
	int rc;

	if (!BNXT_CHIP_P7(bp)) {
		rc = bnxt_ulp_alloc_vf_rep(bp, vf_rep);
		if (rc)
			return rc;
	}

	rcu_read_lock();
	vf = rcu_dereference(bp->pf.vf);
	if (vf)
		cfa_code_map[vf[vf_rep->vf_idx].fw_fid] = vf_rep->vf_idx;
	rcu_read_unlock();

	/* ulp_mapper_bd_act_set requires cfa_code_map to be set up
	 * so it can locate the vfr. So the allocation for vf reps for
	 * P7 is called after the vf idx is set up in the code map.
	 */
	if (BNXT_CHIP_P7(bp)) {
		rc = bnxt_ulp_alloc_vf_rep_p7(bp, vf_rep);
		if (rc)
			return rc;
	}

	return 0;
}

static int bnxt_vfrep_cfact_update(struct bnxt *bp, struct bnxt_vf_rep *vf_rep)
{
	vf_rep->dst = metadata_dst_alloc(0, METADATA_HW_PORT_MUX, GFP_KERNEL);
	if (!vf_rep->dst)
		return -ENOMEM;

	/* only cfa_action is needed to mux a packet while TXing */
	vf_rep->dst->u.port_info.port_id = vf_rep->tx_cfa_action;
	vf_rep->dst->u.port_info.lower_dev = bp->dev;

	return 0;
}

static int bnxt_alloc_vf_rep(struct bnxt *bp, struct bnxt_vf_rep *vf_rep,
			     u16 *cfa_code_map)
{
	int rc;

	if (!BNXT_TRUFLOW_EN(bp)) {
		/* get cfa handles from FW */
		if (hwrm_cfa_vfr_alloc(bp, vf_rep->vf_idx, &vf_rep->tx_cfa_action,
				       &vf_rep->rx_cfa_code))
			return -ENOLINK;
		cfa_code_map[vf_rep->rx_cfa_code] = vf_rep->vf_idx;
	} else {
		rc = bnxt_alloc_tf_vf_rep(bp, vf_rep, cfa_code_map);
		if (rc)
			return rc;
	}

	if (!BNXT_CHIP_P7(bp))
		return bnxt_vfrep_cfact_update(bp, vf_rep);

	return 0;
}

/* Allocate the VF-Reps in firmware, during firmware hot-reset processing.
 * Note that the VF-Rep netdevs are still active (not unregistered) during
 * this process.
 */
int bnxt_vf_reps_alloc(struct bnxt *bp)
{
	u16 *cfa_code_map = bp->cfa_code_map, num_vfs = pci_num_vf(bp->pdev);
	struct bnxt_vf_rep *vf_rep;
	int rc, i;

	if (!bnxt_tc_is_switchdev_mode(bp))
		return -EINVAL;

	if (!cfa_code_map)
		return -EINVAL;

	for (i = 0; i < MAX_CFA_CODE; i++)
		cfa_code_map[i] = VF_IDX_INVALID;

	for (i = 0; i < num_vfs; i++) {
		/* Skip VFR creation after FW crash recovery only if
		 * trust is enabled and before crash VFR netdev was
		 * not active else it might result in kernl panic.
		 */
		if (bnxt_is_trusted_vf(bp, &bp->pf.vf[i]) && !bp->vf_reps[i])
			continue;
		vf_rep = bp->vf_reps[i];
		vf_rep->vf_idx = i;

		rc = bnxt_alloc_vf_rep(bp, vf_rep, cfa_code_map);
		if (rc)
			goto err;
	}

	return 0;

err:
	netdev_info(bp->dev, "%s error=%d\n", __func__, rc);
	bnxt_vf_reps_free(bp);
	return rc;
}

/* Use the OUI of the PF's perm addr and report the same mac addr
 * for the same VF-rep each time
 */
static void bnxt_vf_rep_eth_addr_gen(const u8 *src_mac, u16 vf_idx, u8 *mac)
{
	u32 addr;

	ether_addr_copy(mac, src_mac);

	addr = jhash(src_mac, ETH_ALEN, 0) + vf_idx;
	mac[3] = (u8)(addr & 0xFF);
	mac[4] = (u8)((addr >> 8) & 0xFF);
	mac[5] = (u8)((addr >> 16) & 0xFF);
}

static void bnxt_vf_rep_netdev_init(struct bnxt *bp, struct bnxt_vf_rep *vf_rep,
				    struct net_device *dev)
{
	struct net_device *pf_dev = bp->dev;
	u16 max_mtu;

	dev->netdev_ops = &bnxt_vf_rep_netdev_ops;
	dev->ethtool_ops = &bnxt_vf_rep_ethtool_ops;
#ifndef HAVE_NDO_GET_PORT_PARENT_ID
	SWITCHDEV_SET_OPS(dev, &bnxt_vf_rep_switchdev_ops);
#endif
	/* Just inherit all the features of the parent PF as the VF-R
	 * uses the RX/TX rings of the parent PF
	 */
	dev->hw_features = pf_dev->hw_features;
	dev->gso_partial_features = pf_dev->gso_partial_features;
	dev->vlan_features = pf_dev->vlan_features;
	dev->hw_enc_features = pf_dev->hw_enc_features;
	dev->features |= pf_dev->features;
	bnxt_vf_rep_eth_addr_gen(bp->dev->dev_addr, vf_rep->vf_idx,
				 dev->perm_addr);
	eth_hw_addr_set(dev, dev->perm_addr);
	/* Set VF-Rep's max-mtu to the corresponding VF's max-mtu */
	if (!bnxt_hwrm_vfr_qcfg(bp, vf_rep, &max_mtu))
#ifdef HAVE_NET_DEVICE_EXT
		dev->extended->max_mtu = max_mtu;
	dev->extended->min_mtu = ETH_ZLEN;
#else
		dev->max_mtu = max_mtu;
	dev->min_mtu = ETH_ZLEN;
#endif
}

int bnxt_vf_reps_create(struct bnxt *bp)
{
	u16 *cfa_code_map = NULL, num_vfs = pci_num_vf(bp->pdev);
	struct bnxt_vf_rep *vf_rep;
	struct net_device *dev;
	int rc, i;

	if (!(bp->flags & BNXT_FLAG_DSN_VALID))
		return -ENODEV;

	bp->vf_reps = kcalloc(num_vfs, sizeof(vf_rep), GFP_KERNEL);
	if (!bp->vf_reps)
		return -ENOMEM;

	/* storage for cfa_code to vf-idx mapping */
	cfa_code_map = kmalloc_array(MAX_CFA_CODE, sizeof(*bp->cfa_code_map),
				     GFP_KERNEL);
	if (!cfa_code_map) {
		rc = -ENOMEM;
		goto err;
	}
	for (i = 0; i < MAX_CFA_CODE; i++)
		cfa_code_map[i] = VF_IDX_INVALID;

	if (BNXT_CHIP_P7(bp)) {
		/* ONLY for THOR2, publish cfa_code_map before all VFs are
		 * initialized, so default rules can run and use it when required.
		 * Note: code maps are inited to "invalid" by default.
		 */
		bp->cfa_code_map = cfa_code_map;
	}

	for (i = 0; i < num_vfs; i++) {
		if (bnxt_is_trusted_vf(bp, &bp->pf.vf[i]))
			continue;

		dev = alloc_etherdev(sizeof(*vf_rep));
		if (!dev) {
			rc = -ENOMEM;
			goto err;
		}

		vf_rep = netdev_priv(dev);
		bp->vf_reps[i] = vf_rep;
		vf_rep->dev = dev;
		vf_rep->bp = bp;
		vf_rep->vf_idx = i;
		vf_rep->tx_cfa_action = CFA_HANDLE_INVALID;

		if (BNXT_TRUFLOW_EN(bp))
			bnxt_vf_rep_netdev_init(bp, vf_rep, dev);

		rc = bnxt_alloc_vf_rep(bp, vf_rep, cfa_code_map);
		if (rc) {
			if (BNXT_TRUFLOW_EN(bp))
				vf_rep->dev->netdev_ops = NULL;
			goto err;
		}

		if (!BNXT_TRUFLOW_EN(bp))
			bnxt_vf_rep_netdev_init(bp, vf_rep, dev);

		rc = register_netdev(dev);
		if (rc) {
			/* no need for unregister_netdev in cleanup */
			dev->netdev_ops = NULL;
			goto err;
		}
#ifdef CONFIG_BNXT_FLOWER_OFFLOAD
		bnxt_reg_egdev(vf_rep->dev, bnxt_cb_egdev, (void *)vf_rep,
			       vf_rep->vf_idx);
#endif
	}

	/* publish cfa_code_map only after all VF-reps have been initialized */
	bp->cfa_code_map = cfa_code_map;
	netif_keep_dst(bp->dev);
	return 0;

err:
	netdev_err(bp->dev, "Failed to initialize SWITCHDEV mode, rc[%d]\n", rc);
	kfree(cfa_code_map);
	__bnxt_vf_reps_destroy(bp);
	return rc;
}

/* Devlink related routines */
int bnxt_dl_eswitch_mode_get(struct devlink *devlink, u16 *mode)
{
	struct bnxt *bp = bnxt_get_bp_from_dl(devlink);

	*mode = bp->eswitch_mode;
	return 0;
}

#ifdef HAVE_ESWITCH_MODE_SET_EXTACK
int bnxt_dl_eswitch_mode_set(struct devlink *devlink, u16 mode,
			     struct netlink_ext_ack *extack)
#else
int bnxt_dl_eswitch_mode_set(struct devlink *devlink, u16 mode)
#endif
{
	struct bnxt *bp = bnxt_get_bp_from_dl(devlink);
	int rc = 0;

	if (test_bit(BNXT_STATE_IN_FW_RESET, &bp->state)) {
		netdev_info(bp->dev, "FW in reset, ignoring eswitch mode change\n");
		return -EBUSY;
	}

	if (NIC_FLOW_SUPPORTED(bp) && (mode == DEVLINK_ESWITCH_MODE_SWITCHDEV)) {
		/*
		 * Switchdev mode unsupported if NIC flow capable.  Currently NIC flow
		 * is only available on Thor2 with special UDCC build
		 */
		netdev_dbg(bp->dev,
			   "Switchdev mode not supported when NIC flows are enabled\n");
		return -EOPNOTSUPP;
	}

	if (mode == DEVLINK_ESWITCH_MODE_SWITCHDEV) {
		netdev_lock(bp->dev);
		if (bp->sriov_cfg) {
			netdev_info(bp->dev,
				    "SRIOV is being configured, cannot set switchdev mode\n");
			netdev_unlock(bp->dev);
			return -EBUSY;
		}

		if (!bp->dev || bp->dev->reg_state != NETREG_REGISTERED) {
			netdev_unlock(bp->dev);
			return -EINVAL;
		}
		netdev_unlock(bp->dev);
	}

	mutex_lock(&bp->vf_rep_lock);
	if (bp->eswitch_mode == mode) {
		netdev_info(bp->dev, "already in %s eswitch mode\n",
			    mode == DEVLINK_ESWITCH_MODE_LEGACY ?
			    "legacy" : "switchdev");
		rc = -EINVAL;
		goto done;
	}

	switch (mode) {
	case DEVLINK_ESWITCH_MODE_LEGACY:
		if (bnxt_get_current_flow_cnt(bp)) {
			netdev_err(bp->dev,
				   "Cleanup existing filters before changing the setting\n");
			rc = -EBUSY;
			goto done;
		}
		bnxt_vf_reps_destroy(bp);
		if (BNXT_TRUFLOW_EN(bp))
			bnxt_tf_port_deinit(bp, BNXT_TF_FLAG_SWITCHDEV);
		break;

	case DEVLINK_ESWITCH_MODE_SWITCHDEV:
		if (bp->hwrm_spec_code < 0x10803) {
			netdev_warn(bp->dev, "FW does not support SRIOV E-Switch SWITCHDEV mode\n");
			rc = -ENOTSUPP;
			goto done;
		}
		if (bp->eswitch_disabled) {	/* PCI remove in progress */
			netdev_warn(bp->dev, "SWITCHDEV mode transition is disabled\n");
			rc = -EOPNOTSUPP;
			goto done;
		}
		if (BNXT_TRUFLOW_EN(bp)) {
			rc = bnxt_tf_port_init(bp, BNXT_TF_FLAG_SWITCHDEV);
			if (rc)
				goto done;
		}
		/* Create representors for existing VFs */
		if (pci_num_vf(bp->pdev) > 0)
			rc = bnxt_vf_reps_create(bp);
		break;

	default:
		rc = -EINVAL;
		goto done;
	}
done:
	if (!rc)
		bp->eswitch_mode = mode;
	mutex_unlock(&bp->vf_rep_lock);
	return rc;
}

#endif /* CONFIG_VF_REPS */

#if defined(CONFIG_VF_REPS) || defined(CONFIG_BNXT_CUSTOM_FLOWER_OFFLOAD)
int bnxt_hwrm_get_dflt_vnic_svif(struct bnxt *bp, u16 fid,
				 u16 *vnic_id, u16 *svif)
{
	struct hwrm_func_qcfg_output *resp;
	struct hwrm_func_qcfg_input *req;
	u16 svif_info;
	int rc;

	rc = hwrm_req_init(bp, req, HWRM_FUNC_QCFG);
	if (rc)
		return rc;
	req->fid = cpu_to_le16(fid);

	resp = hwrm_req_hold(bp, req);
	rc = hwrm_req_send(bp, req);
	if (rc)
		goto err;

	svif_info = le16_to_cpu(resp->svif_info);
	if (svif && (svif_info & FUNC_QCFG_RESP_SVIF_INFO_SVIF_VALID)) {
		*svif = svif_info & FUNC_QCFG_RESP_SVIF_INFO_SVIF_MASK;
		/* When the VF corresponding to the VFR is down at the time of
		 * VFR conduit creation, the VFR rule will be programmed with
		 * invalid vnic id because FW will return default vnic id as
		 * INVALID when queried through FUNC_QCFG. As a result, when
		 * the VF is brought up, VF won't receive packets because
		 * INVALID vnic id is already programmed.
		 *
		 * Hence, use svif value as vnic id during VFR conduit creation
		 * as both svif and default vnic id values are same and will
		 * never change.
		 */
		if (vnic_id)
			*vnic_id = *svif;
	}

	netdev_dbg(bp->dev, "FID %d SVIF %d VNIC ID %d\n", req->fid, *svif, *vnic_id);
err:
	hwrm_req_drop(bp, req);
	return rc;
}
#endif /* CONFIG_VF_REPS || CONFIG_BNXT_CUSTOM_FLOWER_OFFLOAD */

#ifdef CONFIG_DEBUG_FS
static char *dir_str[] = {"rx", "tx"};

static int bs_show(struct seq_file *m, void *unused)
{
	struct bnxt *bp = dev_get_drvdata(m->private);
	char dir_str_req[5], wc_str[5];
	int tsid;
	int dir;
	int rc;
	bool wc = false;

	rc = sscanf(m->file->f_path.dentry->d_name.name,
		    "%d-%2s-%2s", &tsid, dir_str_req, wc_str);
	if (rc < 3) {
		rc = sscanf(m->file->f_path.dentry->d_name.name,
			    "%d-%s", &tsid, dir_str_req);
		if (rc < 0) {
			seq_puts(m, "Failed to scan file name\n");
			return 0;
		}
		wc = false;
	} else {
		wc = true;
	}

	if (strcmp(dir_str[0], dir_str_req) == 0)
		dir = CFA_DIR_RX;
	else
		dir = CFA_DIR_TX;

	seq_printf(m, "%s ts:%d(%d) dir:%d(%d) - %s\n",
		   wc ? "WC" : "EM", tsid,
		   bp->bs_data[dir].tsid, dir,
		   bp->bs_data[dir].dir,
		   dir_str_req);
	if (wc)
		tfc_wc_show(m, bp->tfp, tsid, dir);
	else
		tfc_em_show(m, bp->tfp, tsid, dir);

	return 0;
}

void bnxt_tf_debugfs_create_files(struct bnxt *bp, u8 tsid, struct dentry *port_dir)
{
	char name[32];
	int dir, wc;

	for (wc = 1; wc >= 0; wc--) {
		for (dir = 0; dir < CFA_DIR_MAX; dir++) {
			/*
			 * Format is: tablescope-dir[-wc]
			 * tablescope-dir: File to read EM debug info
			 * tablescope-dir-wc: File to read WC debug info
			 */
			sprintf(name, "%d-%s%s",
				tsid, dir_str[dir],
				wc ? "-wc" : "");
			bp->bs_data[dir].tsid = tsid;
			bp->bs_data[dir].dir  = dir;
			dev_set_drvdata(&bp->dev->dev, bp);
			if (!debugfs_lookup(name, port_dir))
				debugfs_create_devm_seqfile(&bp->dev->dev,
							    name,
							    port_dir,
							    bs_show);
		}
	}
}
#endif /* CONFIG_DEBUG_FS */
