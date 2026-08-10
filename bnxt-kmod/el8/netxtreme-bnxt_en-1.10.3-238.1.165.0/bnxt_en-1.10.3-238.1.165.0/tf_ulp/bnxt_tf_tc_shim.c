/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023-2023 Broadcom
 * All rights reserved.
 */

#include "ulp_linux.h"
#include "bnxt_compat.h"
#include "bnxt_hsi.h"
#include "bnxt.h"
#include "bnxt_tfc.h"
#include "bnxt_hwrm.h"
#include "bnxt_ethtool.h"
#include "bnxt_tf_common.h"
#include "bnxt_tf_tc_shim.h"
#include "ulp_mapper.h"
#include "ulp_template_debug_proto.h"
#include "ulp_udcc.h"

#if defined(CONFIG_BNXT_FLOWER_OFFLOAD)
int bnxt_ulp_tf_v6_subnet_add(struct bnxt *bp, u8 *byte_key, u8 *byte_mask,
			      u8 *byte_data, u16 *subnet_hndl)
{
	u8 v6dst[16] = { 0 };
	u8 v6msk[16] = { 0 };
	u8 dmac[6] = { 0 };
	u8 smac[6] = { 0 };
	u16 src_fid;

	memcpy(&src_fid, byte_key, sizeof(src_fid));
	memcpy(v6dst, byte_key + sizeof(src_fid), sizeof(v6dst));
	memcpy(v6msk, byte_mask + sizeof(src_fid), sizeof(v6msk));
	memcpy(dmac, byte_data, sizeof(dmac));
	memcpy(smac, byte_data + sizeof(dmac), sizeof(smac));

	return bnxt_ulp_udcc_v6_subnet_add(bp,
					   &src_fid, v6dst, v6msk,
					   dmac, smac,
					   subnet_hndl);
}

int bnxt_ulp_tf_v6_subnet_del(struct bnxt *bp, u16 subnet_hndl)
{
	return bnxt_ulp_udcc_v6_subnet_del(bp, subnet_hndl);
}
#endif /* #if defined(CONFIG_BNXT_FLOWER_OFFLOAD) */

#if defined(CONFIG_BNXT_FLOWER_OFFLOAD) || defined(CONFIG_BNXT_CUSTOM_FLOWER_OFFLOAD)

#define ULP_GLOBAL_TUNNEL_UDP_PORT_SHIFT 32
#define ULP_GLOBAL_TUNNEL_UDP_PORT_MASK  ((uint16_t)0xffff)
#define ULP_GLOBAL_TUNNEL_PORT_ID_SHIFT  16
#define ULP_GLOBAL_TUNNEL_PORT_ID_MASK   ((uint16_t)0xffff)
#define ULP_GLOBAL_TUNNEL_UPARID_SHIFT   8
#define ULP_GLOBAL_TUNNEL_UPARID_MASK    ((uint16_t)0xff)
#define ULP_GLOBAL_TUNNEL_TYPE_SHIFT     0
#define ULP_GLOBAL_TUNNEL_TYPE_MASK      ((uint16_t)0xff)

/* Extracts the dpdk port id and tunnel type from the handle */
static void
bnxt_tc_global_reg_hndl_to_data(uint64_t handle, uint16_t *port_id,
				uint8_t *upar_id, uint8_t *type,
				uint16_t *udp_port)
{
	*type    = (handle >> ULP_GLOBAL_TUNNEL_TYPE_SHIFT) &
		   ULP_GLOBAL_TUNNEL_TYPE_MASK;
	*upar_id = (handle >> ULP_GLOBAL_TUNNEL_UPARID_SHIFT) &
		   ULP_GLOBAL_TUNNEL_UPARID_MASK;
	*port_id    = (handle >> ULP_GLOBAL_TUNNEL_PORT_ID_SHIFT) &
		   ULP_GLOBAL_TUNNEL_PORT_ID_MASK;
	*udp_port = (handle >> ULP_GLOBAL_TUNNEL_UDP_PORT_SHIFT) &
		   ULP_GLOBAL_TUNNEL_UDP_PORT_MASK;
}

/* Packs the dpdk port id and tunnel type in the handle */
static void
bnxt_tc_global_reg_data_to_hndl(uint16_t port_id, uint8_t upar_id,
				uint8_t type, uint16_t udp_port,
				uint64_t *handle)
{
	*handle = 0;
	*handle	|=  (udp_port & ULP_GLOBAL_TUNNEL_UDP_PORT_MASK);
	*handle	<<= ULP_GLOBAL_TUNNEL_UDP_PORT_SHIFT;
	*handle	|=  (port_id & ULP_GLOBAL_TUNNEL_PORT_ID_MASK) <<
		ULP_GLOBAL_TUNNEL_PORT_ID_SHIFT;
	*handle	|= (upar_id & ULP_GLOBAL_TUNNEL_UPARID_MASK) <<
		ULP_GLOBAL_TUNNEL_UPARID_SHIFT;
	*handle |= (type & ULP_GLOBAL_TUNNEL_TYPE_MASK);
}

/* Sets or resets the tunnel ports.
 * If dport == 0, then the port_id and type are retrieved from the handle.
 * otherwise, the incoming port_id, type, and dport are used.
 * The type is enum ulp_mapper_ulp_global_tunnel_type
 */
int
bnxt_tc_global_tunnel_set(struct bnxt_ulp_context *ulp_ctx,
			  u16 port_id, u8 type,
			  u16 udp_port, u64 *handle)
{
	struct bnxt *bp = ulp_ctx->bp;
	u16 ludp_port = udp_port;
	u8 ltype, lupar_id = 0;
	u32 *ulp_flags;
	int rc = 0;

	if (!udp_port) {
		/* Free based on the handle */
		if (!handle) {
			netdev_dbg(bp->dev, "Free with invalid handle\n");
			return -EINVAL;
		}
		bnxt_tc_global_reg_hndl_to_data(*handle, &port_id,
						&lupar_id, &ltype, &ludp_port);
	}

	ulp_mapper_global_register_tbl_dump(ulp_ctx, type, udp_port);

	if (udp_port)
		bnxt_tc_global_reg_data_to_hndl(port_id, lupar_id,
						type, udp_port, handle);

	ulp_flags = &ulp_ctx->cfg_data->ulp_flags;
	if (type == BNXT_ULP_RESOURCE_SUB_TYPE_GLOBAL_REGISTER_CUST_VXLAN) {
		if (udp_port)
			*ulp_flags |= BNXT_ULP_DYNAMIC_VXLAN_SUPPORT;
		else
			*ulp_flags &= ~BNXT_ULP_DYNAMIC_VXLAN_SUPPORT;
	}

	return rc;
}
static int bnxt_get_vnic_info_idx(struct bnxt *bp)
{
	int idx;

	for (idx = 0; idx < bp->nr_vnics; idx++) {
		if (bp->vnic_info[idx].fw_vnic_id == INVALID_HW_RING_ID)
			return idx;
	}

	return -EINVAL;
}

static void bnxt_clear_queue_vnic(struct bnxt *bp, u16 vnic_id)
{
	int i, nr_ctxs;

	if (!bp->vnic_info)
		return;

	if (!(bp->flags & BNXT_FLAG_CHIP_P5_PLUS))
		return;

	/* before free the vnic, undo the vnic tpa settings */
	if (bp->flags & BNXT_FLAG_TPA)
		bnxt_hwrm_vnic_set_tpa(bp, &bp->vnic_info[vnic_id], 0);

	bnxt_hwrm_vnic_free_one(bp, &bp->vnic_info[vnic_id]);

	nr_ctxs = bnxt_get_nr_rss_ctxs(bp, bp->rx_nr_rings);
	for (i = 0; i < nr_ctxs; i++) {
		if (bp->vnic_info[vnic_id].fw_rss_cos_lb_ctx[i] != INVALID_HW_RING_ID) {
			bnxt_hwrm_vnic_ctx_free_one(bp, &bp->vnic_info[vnic_id], i);
			bp->rsscos_nr_ctxs--;
		}
	}
}

static int bnxt_vnic_queue_action_free(struct bnxt *bp, u16 vnic_id)
{
	struct bnxt_vnic_info *vnic_info;
	u16 vnic_idx = vnic_id;
	int rc = -EINVAL;

	/* validate the given vnic idx */
	if (vnic_idx >= bp->nr_vnics) {
		netdev_err(bp->dev, "invalid vnic idx %d\n", vnic_idx);
		return rc;
	}

	/* validate the vnic info */
	vnic_info = &bp->vnic_info[vnic_idx];
	if (vnic_info && !vnic_info->ref_cnt) {
		netdev_err(bp->dev, "Invalid vnic idx, no queues being used\n");
		return rc;
	}

	if (vnic_info->ref_cnt) {
		vnic_info->ref_cnt--;
		if (!vnic_info->ref_cnt)
			bnxt_clear_queue_vnic(bp, vnic_idx);
	}
	return 0;
}

static int
bnxt_setup_queue_vnic(struct bnxt *bp, u16 vnic_id, u16 q_index)
{
	int rc, nr_ctxs, i;

	/* It's a queue action, so only one queue */
	rc = bnxt_hwrm_vnic_alloc(bp, &bp->vnic_info[vnic_id], q_index, 1);
	if (rc) {
		rc = -EINVAL;
		goto cleanup;
	}

	rc = bnxt_hwrm_vnic_cfg(bp, &bp->vnic_info[vnic_id], q_index);
	if (rc) {
		rc = -EINVAL;
		goto vnic_free;
	}

	rc = bnxt_hwrm_vnic_set_tpa(bp, &bp->vnic_info[vnic_id], bp->flags & BNXT_FLAG_TPA);
	if (rc) {
		rc = -EINVAL;
		goto vnic_free;
	}

	if (bp->flags & BNXT_FLAG_AGG_RINGS) {
		rc = bnxt_hwrm_vnic_set_hds(bp, &bp->vnic_info[vnic_id]);
		if (rc) {
			netdev_info(bp->dev, "hwrm vnic %d set hds failure rc: %x\n",
				    vnic_id, rc);
			goto clear_tpa;
		}
	}

	/* Even though this vnic is going to have only one queue, RSS is still
	 * enabled as the RX completion handler expects a valid RSS hash in the
	 * rx completion.
	 */
	bp->vnic_info[vnic_id].flags |=
			BNXT_VNIC_RSS_FLAG | BNXT_VNIC_MCAST_FLAG | BNXT_VNIC_UCAST_FLAG;
	nr_ctxs = bnxt_get_nr_rss_ctxs(bp, bp->rx_nr_rings);
	for (i = 0; i < nr_ctxs; i++) {
		rc = bnxt_hwrm_vnic_ctx_alloc(bp, &bp->vnic_info[vnic_id], i);
		if (rc) {
			netdev_err(bp->dev, "hwrm vnic %d ctx %d alloc failure rc: %x\n",
				   vnic_id, i, rc);
			break;
		}
		bp->rsscos_nr_ctxs++;
	}
	if (i < nr_ctxs)
		goto ctx_free;

	rc = bnxt_hwrm_vnic_set_rss_p5(bp, &bp->vnic_info[vnic_id], true);
	if (rc) {
		netdev_info(bp->dev, "Failed to enable RSS on vnic_id %d\n", vnic_id);
		goto ctx_free;
	}

	return 0;

ctx_free:
	for (i = 0; i < nr_ctxs; i++) {
		if (bp->vnic_info[vnic_id].fw_rss_cos_lb_ctx[i] != INVALID_HW_RING_ID)
			bnxt_hwrm_vnic_ctx_free_one(bp, &bp->vnic_info[vnic_id], i);
	}
clear_tpa:
	bnxt_hwrm_vnic_set_tpa(bp, &bp->vnic_info[vnic_id], 0);
vnic_free:
	bnxt_hwrm_vnic_free_one(bp, &bp->vnic_info[vnic_id]);
cleanup:
	return rc;
}

static int bnxt_vnic_queue_action_alloc(struct bnxt *bp, u16 q_index, u16 *vnic_idx, u16 *vnicid)
{
	struct vnic_info_meta *vnic_meta;
	int rc = -EINVAL;
	int idx;

	if (!bp->vnic_meta) {
		netdev_err(bp->dev,
			   "Queue action is invalid while ntuple-filter is on\n");
		return rc;
	}

	/* validate the given queue id */
	if (q_index >= bp->rx_nr_rings) {
		netdev_err(bp->dev, "invalid queue id should be less than %d\n",
			   bp->rx_nr_rings);
		return rc;
	}

	vnic_meta = &bp->vnic_meta[q_index];
	/* Scenario 1: Queue is under use by non-truflow entity */
	if (vnic_meta && !vnic_meta->meta_valid &&
	    vnic_meta->fw_vnic_id != INVALID_HW_RING_ID)
		return -EINVAL;
	/* Scenario 2: Queue is under by truflow entity, just increase the reference count */
	if (vnic_meta && vnic_meta->meta_valid) {
		idx = vnic_meta->vnic_idx;
		goto done;
	}
	/* Scenario 3: New vnic must be allocated*/
	/* Get new vnic */
	idx = bnxt_get_vnic_info_idx(bp);
	if (idx < 0)
		return -EINVAL;

	bp->vnic_info[idx].q_index = q_index;
	rc = bnxt_setup_queue_vnic(bp, idx, q_index);
	if (rc) {
		bp->vnic_info[idx].q_index = INVALID_HW_RING_ID;
		return rc;
	}

	/* Populate all important data only when everything in this routine is successful */
	vnic_meta->meta_valid = true;
	vnic_meta->vnic_idx = idx;
	bp->vnic_info[idx].vnic_meta = vnic_meta;

done:
	bp->vnic_info[idx].ref_cnt++;
	*vnic_idx = (u16)idx;
	*vnicid = bp->vnic_info[idx].fw_vnic_id;

	return 0;
}

int bnxt_queue_action_create(struct bnxt_ulp_mapper_parms *parms,
			     u16 *vnic_idx, u16 *vnic_id)
{
	struct ulp_tc_act_prop *ap = parms->act_prop;
	struct bnxt *bp = parms->ulp_ctx->bp;
	u16 q_index;

	memcpy(&q_index, &ap->act_details[BNXT_ULP_ACT_PROP_IDX_QUEUE_INDEX],
	       BNXT_ULP_ACT_PROP_SZ_QUEUE_INDEX);

	return bnxt_vnic_queue_action_alloc(bp, q_index, vnic_idx, vnic_id);
}

int bnxt_queue_action_delete(struct tf *tfp, u16 vnic_idx)
{
	struct bnxt *bp = NULL;

	bp = tfp->bp;
	if (!bp) {
		netdev_err(NULL, "Invalid bp\n");
		return -EINVAL;
	}
	return bnxt_vnic_queue_action_free(bp, vnic_idx);
}

int bnxt_bd_act_set(struct bnxt *bp, u16 port_id, u32 act)
{
	struct bnxt_ulp_context *ulp_ctx = bp->ulp_ctx;
	struct bnxt_vf_rep *vfr;
	u32 ulp_flags = 0;
	int rc = 0;

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

	if (ULP_VF_REP_IS_ENABLED(ulp_flags)) {
		vfr = netdev_priv(bp->dev);
		if (!vfr)
			return rc;
		vfr->tx_cfa_action = act;
	} else {
		bp->tx_cfa_action = act;
	}
err1:
	return rc;
}
#endif /* CONFIG_BNXT_FLOWER_OFFLOAD */
