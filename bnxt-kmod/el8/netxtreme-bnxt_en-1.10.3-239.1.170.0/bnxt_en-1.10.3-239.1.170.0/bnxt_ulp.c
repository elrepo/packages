/* Broadcom NetXtreme-C/E network driver.
 *
 * Copyright (c) 2016-2018 Broadcom Limited
 * Copyright (c) 2018-2024 Broadcom Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 */

#ifdef HAVE_NETDEV_LOCK
#include <net/netdev_lock.h>
#endif

#include "bnxt_compat.h"
#include "bnxt_hsi.h"
#include "bnxt.h"
#include "bnxt_hwrm.h"
#include "bnxt_ulp.h"
#include "bnxt_coredump.h"
#include "ulp_udcc.h"
#include "bnxt_ulp_flow.h"
#include "bnxt_vfr.h"
#include "bnxt_dcqcn.h"
#include "tfc_v3/tfc_vf2pf_msg.h"
#include "tfc.h"

static DEFINE_IDA(bnxt_aux_dev_ids);

struct bnxt_aux_device {
	const char *name;
};

static void bnxt_auxdev_set_state(struct bnxt *bp, int idx, int state)
{
	WRITE_ONCE(bp->auxdev_state[idx], state);
}

static bool bnxt_auxdev_is_init(struct bnxt *bp, int idx)
{
	return (READ_ONCE(bp->auxdev_state[idx]) == BNXT_ADEV_STATE_INIT);
}

static bool bnxt_auxdev_is_active(struct bnxt *bp, int idx)
{
	return (READ_ONCE(bp->auxdev_state[idx]) == BNXT_ADEV_STATE_ADD);
}

static struct bnxt_aux_device bnxt_aux_devices[__BNXT_AUXDEV_MAX] = {{
	.name		= "rdma",
}, {
	.name		= "fwctl",
}};

static void bnxt_fill_msix_vecs(struct bnxt *bp, struct bnxt_msix_entry *ent)
{
	struct bnxt_en_dev *edev = bp->edev[BNXT_AUXDEV_RDMA];
	int num_msix, i;

	num_msix = edev->ulp_tbl->msix_requested;
	for (i = 0; i < num_msix; i++) {
		ent[i].vector = bp->irq_tbl[i].vector;
		ent[i].ring_idx = i;
		if (bp->flags & BNXT_FLAG_CHIP_P5_PLUS)
			ent[i].db_offset = bp->db_offset;
		else
			ent[i].db_offset = i * 0x80;
	}
}

u16 bnxt_calc_dflt_ulp_stat_ctxs_reserved(struct bnxt *bp)
{
	u16 stat_ctxs = BNXT_MIN_ROCE_STAT_CTXS;

	if (BNXT_PF(bp) && !bp->pf.port_id &&
	    bp->port_count > 1)
		stat_ctxs++;

	/* Reserve one stat_ctx for mirror on RoCE support */
	if (BNXT_MIRROR_ON_ROCE_CAP(bp))
		stat_ctxs++;

	/* Reserve one stat_ctx for RoCE Timestamp support */
	if (BNXT_ROCE_CMPL_TS_SUPPORTED(bp))
		stat_ctxs++;

	/* One stat_ctx per configured ring.so it can be restored */
	stat_ctxs += bp->ulp_num_rings;

	return stat_ctxs;
}

u16 bnxt_get_ulp_resc(struct bnxt *bp, enum bnxt_ulp_resc_type type)
{
	if (!bp->edev[BNXT_AUXDEV_RDMA])
		return 0;

	return bp->edev[BNXT_AUXDEV_RDMA]->ulp_resc[type];
}

void bnxt_set_ulp_resc(struct bnxt *bp,
		       enum bnxt_ulp_resc_type type,
		       u16 num)
{
	if (!bp->edev[BNXT_AUXDEV_RDMA])
		return;

	if (type == BNXT_ULP_RES_MSIX)
		bp->roce_num_msix = num;

	if (type == BNXT_ULP_RES_STAT_CTX)
		bp->roce_num_stat_ctx = num;

	bp->edev[BNXT_AUXDEV_RDMA]->ulp_resc[type] = num;
}

int bnxt_get_ulp_msix_num_in_use(struct bnxt *bp)
{
	if (bnxt_ulp_registered(bp->edev[BNXT_AUXDEV_RDMA]))
		return bnxt_get_ulp_resc(bp, BNXT_ULP_RES_MSIX);
	return 0;
}

int bnxt_get_ulp_stat_ctxs_in_use(struct bnxt *bp)
{
	if (bnxt_ulp_registered(bp->edev[BNXT_AUXDEV_RDMA]))
		return bnxt_get_ulp_resc(bp, BNXT_ULP_RES_STAT_CTX);
	return 0;
}

void bnxt_set_dflt_ulp_stat_ctxs(struct bnxt *bp)
{
	struct bnxt_en_dev *edev = bp->edev[BNXT_AUXDEV_RDMA];
	u16 num;

	if (!edev)
		return;

	num = bnxt_calc_dflt_ulp_stat_ctxs_reserved(bp);
	bnxt_set_ulp_resc(bp, BNXT_ULP_RES_STAT_CTX, num);
}

int bnxt_get_ulp_num_rings(struct bnxt *bp)
{
	return (BNXT_ROCE_CMPL_TS_SUPPORTED(bp)) ? BNXT_MAX_ROCE_TS_QPS : 0;
}

void bnxt_set_ulp_num_rings(struct bnxt *bp, int num)
{
	struct bnxt_en_dev *edev = bp->edev[BNXT_AUXDEV_RDMA];

	if (!edev)
		return;

	/* The non-zero path reserves resources for TS QPs and only applies to
	 * devices that support RoCE completion timestamps. The zero path is a
	 * cleanup (reset to base values) that is valid for any device, so it is
	 * not gated by BNXT_ROCE_CMPL_TS_SUPPORTED().
	 */
	if (num && !BNXT_ROCE_CMPL_TS_SUPPORTED(bp))
		return;

	/* This function is used to reserve resources for TS QPs.
	 * Assume this is called before adding any other ULP resources
	 * so reset the resource array to base values.
	 */
	edev->ulp_resc[BNXT_ULP_RES_RX] = num * 2; /* 2RX per TS QP */
	edev->ulp_resc[BNXT_ULP_RES_TX] = num * 1; /* 1TX per TS QP */
	edev->ulp_resc[BNXT_ULP_RES_CP] = num * 2; /* 2CP per TS QP */
	edev->ulp_resc[BNXT_ULP_RES_VNIC] = num * 1; /* 1VNIC per TS QP */
	edev->ulp_resc[BNXT_ULP_RES_RSS_CTX] = 0;
	bnxt_set_dflt_ulp_stat_ctxs(bp);
}

/* Restore devlink-configured ULP resources after edev recreation.*/
static void bnxt_restore_ulp_rings(struct bnxt *bp)
{
	struct bnxt_en_dev *edev = bp->edev[BNXT_AUXDEV_RDMA];
	u16 nr = bp->ulp_num_rings;
	u16 nv = bp->ulp_num_vnics;

	if (!edev || BNXT_ROCE_CMPL_TS_SUPPORTED(bp))
		return;
	if (!(bp->flags & BNXT_FLAG_CHIP_P5_PLUS))
		return;
	if (!nr && !nv)
		return;

	/* ulp_num_rings param: RX/TX/CP rings and per-ring stat ctxs */
	edev->ulp_resc[BNXT_ULP_RES_RX] = nr;
	edev->ulp_resc[BNXT_ULP_RES_TX] = nr;
	edev->ulp_resc[BNXT_ULP_RES_CP] = 2 * nr;
	bnxt_set_dflt_ulp_stat_ctxs(bp);

	/* ulp_num_vnics param: VNICs and RSS contexts */
	edev->ulp_resc[BNXT_ULP_RES_VNIC] = nv;
	edev->ulp_resc[BNXT_ULP_RES_RSS_CTX] = nv;
}

void bnxt_en_dev_lock(struct bnxt *bp)
{
	if (!bp->edev[BNXT_AUXDEV_RDMA])
		return;

	mutex_lock(&bp->edev[BNXT_AUXDEV_RDMA]->en_dev_lock);
}

void bnxt_en_dev_unlock(struct bnxt *bp)
{
	if (!bp->edev[BNXT_AUXDEV_RDMA])
		return;

	mutex_unlock(&bp->edev[BNXT_AUXDEV_RDMA]->en_dev_lock);
}

int bnxt_register_dev(struct bnxt_en_dev *edev,
		      struct bnxt_ulp_ops *ulp_ops, void *handle)
{
	struct net_device *dev = edev->net;
	struct bnxt *bp = netdev_priv(dev);
	unsigned int max_stat_ctxs;
	struct bnxt_ulp *ulp;
	int rc = 0;

	if (!bp->irq_tbl)
		return -ENODEV;
	max_stat_ctxs = bnxt_get_max_func_stat_ctxs(bp);
	if (max_stat_ctxs <= BNXT_MIN_ROCE_STAT_CTXS ||
	    bp->cp_nr_rings == max_stat_ctxs)
		return -ENOMEM;

	ulp = edev->ulp_tbl;
	ulp->handle = handle;
	rcu_assign_pointer(ulp->ulp_ops, ulp_ops);

	edev->ulp_tbl->msix_requested = bnxt_get_ulp_resc(bp, BNXT_ULP_RES_MSIX);

	bnxt_fill_msix_vecs(bp, bp->edev[BNXT_AUXDEV_RDMA]->msix_entries);
	return rc;
}
EXPORT_SYMBOL(bnxt_register_dev);

void bnxt_unregister_dev(struct bnxt_en_dev *edev)
{
	struct net_device *dev = edev->net;
	struct bnxt *bp = netdev_priv(dev);
	struct bnxt_ulp *ulp;

	ulp = edev->ulp_tbl;
	edev->ulp_tbl->msix_requested = 0;

	if (ulp->max_async_event_id)
		bnxt_hwrm_func_drv_rgtr(bp, NULL, 0, true);

	RCU_INIT_POINTER(ulp->ulp_ops, NULL);
	synchronize_rcu();
	ulp->max_async_event_id = 0;
	ulp->async_events_bmap = NULL;
	return;
}
EXPORT_SYMBOL(bnxt_unregister_dev);

static int bnxt_set_dflt_ulp_msix(struct bnxt *bp)
{
	u16 roce_msix;

	bp->hw_resc.max_roce_msix = min_t(u16, bp->hw_resc.max_roce_msix,
					  BNXT_MAX_ROCE_MSIX);

	roce_msix = bp->hw_resc.max_roce_msix;

	if (!roce_msix) {
		bp->hw_resc.max_roce_msix = BNXT_MAX_ROCE_MSIX;
		if (BNXT_VF(bp))
			roce_msix = BNXT_ROCE_MSIX_VF_DEFAULT;
		else
			roce_msix = BNXT_ROCE_MSIX_DEFAULT;
	}

	if (!BNXT_VF(bp) && bp->port_partition_type)
		roce_msix = min_t(u16, roce_msix, BNXT_MAX_ROCE_MSIX_NPAR_PF);

#ifdef BNXT_FPGA
	roce_msix = min_t(u16, roce_msix, BNXT_MAX_ROCE_MSIX_PF - 1);
#endif
	/* NQ MSIX vectors should match the number of CPUs plus 1 more for
	 * the CREQ MSIX, up to the default.
	 */
	return min_t(int, roce_msix, num_online_cpus() + 1);
}

int bnxt_send_msg(struct bnxt_en_dev *edev,
		  struct bnxt_fw_msg *fw_msg)
{
	struct net_device *dev = edev->net;
	struct bnxt *bp = netdev_priv(dev);
	struct output *resp;
	struct input *req;
	u32 resp_len;
	int rc;

	if (bp->fw_reset_state)
		return -EBUSY;

	rc = hwrm_req_init(bp, req, 0 /* don't care */);
	if (rc)
		return rc;

	rc = hwrm_req_replace(bp, req, fw_msg->msg, fw_msg->msg_len);
	if (rc)
		goto drop_req;

	hwrm_req_timeout(bp, req, fw_msg->timeout);
	resp = hwrm_req_hold(bp, req);
	rc = hwrm_req_send_silent(bp, req);
	resp_len = le16_to_cpu(resp->resp_len);
	if (resp_len) {
		if (fw_msg->resp_max_len < resp_len)
			resp_len = fw_msg->resp_max_len;

		memcpy(fw_msg->resp, resp, resp_len);
	}
drop_req:
	hwrm_req_drop(bp, req);
	return rc;
}
EXPORT_SYMBOL(bnxt_send_msg);

/*
 * In kernels where native Auxbus infrastructure support is not there,
 * invoke the auxiliary_driver shutdown function.
 */
#ifndef HAVE_AUXILIARY_DRIVER
void bnxt_ulp_shutdown(struct bnxt *bp)
{
	struct bnxt_aux_priv *bnxt_aux = bp->aux_priv[BNXT_AUXDEV_RDMA];
	struct bnxt_en_dev *edev = bp->edev[BNXT_AUXDEV_RDMA];

	if (!edev)
		return;

	if (bnxt_aux) {
		struct auxiliary_device *adev;

		adev = &bnxt_aux->aux_dev;
		if (adev->dev.driver) {
			struct auxiliary_driver *adrv;

			adrv = to_auxiliary_drv(adev->dev.driver);
			if (adrv->shutdown)
				adrv->shutdown(adev);
		}
	}
}
#endif

void bnxt_ulp_irq_stop(struct bnxt *bp)
{
	struct bnxt_en_dev *edev = bp->edev[BNXT_AUXDEV_RDMA];
	struct bnxt_ulp_ops *ops;
	bool reset = false;

	if (!edev)
		return;

	if (bnxt_ulp_registered(edev)) {
		struct bnxt_ulp *ulp = edev->ulp_tbl;

		if (!ulp->msix_requested)
			return;

		ops = netdev_lock_dereference(ulp->ulp_ops, bp->dev);
		if (!ops || !ops->ulp_irq_stop)
			return;
		if (test_bit(BNXT_STATE_FW_RESET_DET, &bp->state))
			reset = true;
		edev->en_state = bp->state;
		ops->ulp_irq_stop(ulp->handle, reset);
	}
}

void bnxt_ulp_irq_restart(struct bnxt *bp, int err)
{
	struct bnxt_en_dev *edev = bp->edev[BNXT_AUXDEV_RDMA];
	struct bnxt_ulp_ops *ops;

	if (!edev)
		return;

	if (bnxt_ulp_registered(edev)) {
		struct bnxt_ulp *ulp = edev->ulp_tbl;
		struct bnxt_msix_entry *ent = NULL;

		if (!ulp->msix_requested)
			return;

		ops = netdev_lock_dereference(ulp->ulp_ops, bp->dev);
		if (!ops || !ops->ulp_irq_restart)
			return;

		if (!err) {
			ent = kcalloc(ulp->msix_requested, sizeof(*ent),
				      GFP_KERNEL);
			if (!ent)
				return;
			bnxt_fill_msix_vecs(bp, ent);
		}
		edev->en_state = bp->state;
		ops->ulp_irq_restart(ulp->handle, ent);
		kfree(ent);
	}
}

static void bnxt_ulp_fill_dump_hdr(struct bnxt *bp, void *buf, u32 seg_id,
				   u32 seg_len)
{
	struct bnxt_coredump_segment_hdr seg_hdr;

	bnxt_fill_coredump_seg_hdr(bp, &seg_hdr, NULL, seg_len, 0, 0, 0,
				   DRV_COREDUMP_COMP_ID, seg_id);
	memcpy(buf, &seg_hdr, sizeof(seg_hdr));
}

u32 bnxt_get_ulp_dump(struct bnxt *bp, u32 dump_flag, void *buf, u32 *segs)
{
	struct bnxt_en_dev *edev = bp->edev[BNXT_AUXDEV_RDMA];
	struct bnxt_ulp_dump *dump;
	struct bnxt_ulp_ops *ops;
	struct bnxt_ulp *ulp;
	u32 i, dump_len = 0;

	*segs = 0;
	bnxt_en_dev_lock(bp);
	if (!bnxt_ulp_registered(edev))
		goto unlock;

	ulp = edev->ulp_tbl;
	ops = rcu_dereference_protected(ulp->ulp_ops,
					lockdep_is_held(&edev->en_dev_lock));
	if (!ops || !ops->ulp_get_dump_info || !ops->ulp_get_dump_data)
		goto unlock;

	dump = &ulp->ulp_dump;
	if (!buf) {
		memset(dump, 0, sizeof(*dump));
		ops->ulp_get_dump_info(ulp->handle, dump_flag, dump);
		if (dump->segs > BNXT_ULP_MAX_DUMP_SEGS) {
			netdev_warn_once(bp->dev, "ULP reported invalid dump segs: %u\n",
					 dump->segs);
			dump->segs = 0;
			goto unlock;
		}
		for (i = 0; i < dump->segs; i++) {
			dump_len += dump->seg_tbl[i].seg_len;
			dump_len += BNXT_SEG_HDR_LEN;
		}
	} else {
		for (i = 0; i < dump->segs; i++) {
			struct bnxt_ulp_dump_tbl *tbl = &dump->seg_tbl[i];
			u32 seg_len = tbl->seg_len;
			u32 seg_id = tbl->seg_id;

			bnxt_ulp_fill_dump_hdr(bp, buf, seg_id, seg_len);
			buf += BNXT_SEG_HDR_LEN;
			dump_len += BNXT_SEG_HDR_LEN;
			ops->ulp_get_dump_data(ulp->handle, seg_id, buf,
					       seg_len);
			buf += seg_len;
			dump_len += seg_len;
		}
	}
	*segs = dump->segs;
unlock:
	bnxt_en_dev_unlock(bp);
	return dump_len;
}

void bnxt_ulp_async_events(struct bnxt *bp, struct hwrm_async_event_cmpl *cmpl)
{
	u16 event_id = le16_to_cpu(cmpl->event_id);
	struct bnxt_en_dev *edev = bp->edev[BNXT_AUXDEV_RDMA];
	struct bnxt_ulp_ops *ops;
	struct bnxt_ulp *ulp;

	if (!(edev && rcu_access_pointer(edev->ulp_tbl->ulp_ops)))
		return;
	ulp = edev->ulp_tbl;

	rcu_read_lock();

	ops = rcu_dereference(ulp->ulp_ops);
	if (!ops || !ops->ulp_async_notifier)
		goto exit_unlock_rcu;
	if (!ulp->async_events_bmap || event_id > ulp->max_async_event_id)
		goto exit_unlock_rcu;

	/* Read max_async_event_id first before testing the bitmap. */
	smp_rmb();

	if (test_bit(event_id, ulp->async_events_bmap))
		ops->ulp_async_notifier(ulp->handle, cmpl);
exit_unlock_rcu:
	rcu_read_unlock();
}

void bnxt_register_async_events(struct bnxt_en_dev *edev,
				unsigned long *events_bmap, u16 max_id)
{
	struct net_device *dev = edev->net;
	struct bnxt *bp = netdev_priv(dev);
	struct bnxt_ulp *ulp;

	ulp = edev->ulp_tbl;

	ulp->async_events_bmap = events_bmap;
	/* Make sure bnxt_ulp_async_events() sees this order */
	smp_wmb();
	ulp->max_async_event_id = max_id;
	bnxt_hwrm_func_drv_rgtr(bp, events_bmap, max_id + 1, true);
}
EXPORT_SYMBOL(bnxt_register_async_events);

void bnxt_force_mirror_en_cfg(struct bnxt_en_dev *edev, bool enable)
{
	struct bnxt *bp = netdev_priv(edev->net);

	bnxt_tf_force_mirror_en_cfg(bp, enable);
}
EXPORT_SYMBOL(bnxt_force_mirror_en_cfg);

void bnxt_force_mirror_en_get(struct bnxt_en_dev *edev, bool *tf_en, bool *force_mirror_en)
{
	struct bnxt *bp = netdev_priv(edev->net);

	*tf_en = false;
	*force_mirror_en = false;
	bnxt_tf_force_mirror_en_get(bp, tf_en, force_mirror_en);
}
EXPORT_SYMBOL(bnxt_force_mirror_en_get);

void bnxt_ulp_update_tf_flag(struct bnxt *bp)
{
	struct bnxt_en_dev *edev = bp->edev[BNXT_AUXDEV_RDMA];

	if (!edev)
		return;

	if (bp->tf_flags & BNXT_TF_FLAG_INITIALIZED)
		edev->flags |= BNXT_EN_FLAG_TF_ENABLED;
	else
		edev->flags &= ~BNXT_EN_FLAG_TF_ENABLED;
}

#if defined(CONFIG_BNXT_FLOWER_OFFLOAD)
int bnxt_udcc_subnet_check(struct bnxt_en_dev *edev, void *dest_ip, u8 *dmac, u8 *smac)
{
	struct net_device *dev = edev->net;
	struct bnxt *bp = netdev_priv(dev);
	int rc;

	rc = bnxt_ulp_udcc_v6_subnet_check(bp, bp->pf.fw_fid,
					   (struct in6_addr *)dest_ip, dmac, smac);
	if (rc == -ENOENT)
		rc = 0;
	return rc;
}
#else
int bnxt_udcc_subnet_check(struct bnxt_en_dev *edev, void *dest_ip, u8 *dmac, u8 *smac)
{
	return -EPERM;
}
#endif
EXPORT_SYMBOL(bnxt_udcc_subnet_check);

#if defined(CONFIG_BNXT_FLOWER_OFFLOAD)
int bnxt_rdma_pkt_capture_set(struct bnxt_en_dev *edev, u8 enable)
{
	struct net_device *netdev = edev->net;
	struct bnxt *bp = netdev_priv(netdev);
	struct bnxt_bond_info *binfo;
	unsigned int idx;
	int rc;

	binfo = bp->bond_info;

	/* FW should be capable of ROCE Mirror feature */
	if (!BNXT_MIRROR_ON_ROCE_CAP(bp))
		return 0;

	if (!binfo || !binfo->bond_active)
		/* Enable/disable the mirroring of the flows for packet capture */
		return bnxt_ulp_set_mirror(bp, enable);

	/* Enable/disable the mirroring for each bond for packet capture */
	for_each_set_bit(idx, &binfo->peers, BNXT_PORTS_MAX) {
		bp = netdev_priv(binfo->p_netdev[idx]);
		rc = bnxt_ulp_set_mirror(bp, enable);
		if (rc)
			return rc;
	}

	return 0;
}
#else
int bnxt_rdma_pkt_capture_set(struct bnxt_en_dev *edev, u8 enable)
{
	return -EPERM;
}
#endif
EXPORT_SYMBOL(bnxt_rdma_pkt_capture_set);

void bnxt_set_soft_roce_icrc(struct bnxt_en_dev *edev, bool enable)
{
	struct bnxt *bp = netdev_priv(edev->net);

	WRITE_ONCE(bp->soft_roce_icrc_en, enable);
}
EXPORT_SYMBOL(bnxt_set_soft_roce_icrc);

int bnxt_tfc_oem_process(struct bnxt_en_dev *edev, u32 *oem_data,
			 u32 *resp, u16 *resp_len)
{
	struct net_device *dev = edev->net;
	struct bnxt *bp = netdev_priv(dev);
	struct tfc *tfcp = bp->tfp;
	int rc;

	if (!BNXT_PF(bp)) {
		netdev_err(bp->dev, "%s: Not a PF. TFC OEM command not supported\n", __func__);
		return -EOPNOTSUPP;
	}

	if (!tfcp) {
		netdev_err(bp->dev, "%s: TFC not initialized (tfcp is NULL)\n", __func__);
		return -ENODEV;
	}

	rc = tfc_oem_cmd_process(tfcp, oem_data, resp, resp_len);
	return rc;
}
EXPORT_SYMBOL(bnxt_tfc_oem_process);

int bnxt_ulp_tfc_bfid_reset(struct bnxt_en_dev *edev, u16 fid)
{
	struct bnxt *bp = netdev_priv(edev->net);

	return tfc_tbl_scope_func_reset(bp->tfp, fid);
}
EXPORT_SYMBOL(bnxt_ulp_tfc_bfid_reset);

/* Tell the RoCE driver whether the TruFlow side can install the per-QP RoCE
 * DCQCN flow. The RoCE UDP source port overwrite depends on that flow to
 * restore the DMAC firmware overwrites, so the RoCE driver must refuse the
 * request when this returns false.
 */
bool bnxt_en_ulp_dcqcn_flow_supported(struct bnxt_en_dev *edev)
{
#if defined(CONFIG_BNXT_FLOWER_OFFLOAD)
	struct bnxt *bp = netdev_priv(edev->net);

	return bnxt_tf_ulp_dcqcn_flow_supported(bp);
#else
	return false;
#endif
}
EXPORT_SYMBOL(bnxt_en_ulp_dcqcn_flow_supported);

int bnxt_en_ulp_dcqcn_flow_create(struct bnxt_en_dev *edev,
				  struct bnxt_dcqcn_session_entry *entry)
{
#if defined(CONFIG_BNXT_FLOWER_OFFLOAD)
	struct bnxt_dcqcn_session_entry *bentry;
	struct net_device *dev = edev->net;
	struct bnxt *bp = netdev_priv(dev);
	struct bnxt_bond_info *binfo;
	struct net_device *netdev;
	struct bnxt *peer_bp;
	unsigned int idx;
	int rc;

	binfo = bp->bond_info;
	if (binfo && binfo->bond_active &&
	    binfo->fw_lag_id != BNXT_INVALID_LAG_ID) {
		for_each_set_bit(idx, &binfo->peers, BNXT_PORTS_MAX) {
			if (bp->pf.port_id == idx)
				continue;
			netdev = binfo->p_netdev[idx];
			if (!netdev)
				continue;
			peer_bp = (struct bnxt *)netdev_priv(netdev);
			bentry = kzalloc(sizeof(*bentry), GFP_KERNEL);
			if (!bentry) {
				rc = -ENOMEM;
				goto error;
			}
			/* Copy the session information */
			memcpy(bentry, entry, sizeof(*entry));
			/* The peer copy must not inherit the parent's
			 * peer_entry pointer, otherwise a teardown of the
			 * peer copy would free memory it does not own.
			 */
			bentry->peer_entry = NULL;
			rc = bnxt_tf_ulp_dcqcn_flow_create(peer_bp, bentry);
			if (rc) {
				kfree(bentry);
				goto error;
			}
			/* peer_entry only tracks a single peer copy. Free any
			 * stale copy before overwriting so retries/multi-peer
			 * bonds do not leak (kfree(NULL) is safe).
			 */
			kfree(entry->peer_entry);
			entry->peer_entry = bentry;
		}
	}

	rc = bnxt_tf_ulp_dcqcn_flow_create(bp, entry);
	if (rc)
		goto error;

	return rc;
error:
	netdev_dbg(bp->dev, "DCQCN create session(%d) failed(%d)\n",
		   entry->session_id, rc);
	(void)bnxt_en_ulp_dcqcn_flow_delete(edev, entry);
	return rc;

#else
	return -EOPNOTSUPP;
#endif
}
EXPORT_SYMBOL(bnxt_en_ulp_dcqcn_flow_create);

int bnxt_en_ulp_dcqcn_flow_delete(struct bnxt_en_dev *edev,
				  struct bnxt_dcqcn_session_entry *entry)
{
#if defined(CONFIG_BNXT_FLOWER_OFFLOAD)
	struct net_device *dev = edev->net;
	struct bnxt *bp = netdev_priv(dev);
	struct bnxt_bond_info *binfo;
	struct net_device *netdev;
	struct bnxt *peer_bp;
	unsigned int idx;
	int rc = 0;

	binfo = bp->bond_info;
	if (binfo && binfo->bond_active &&
	    binfo->fw_lag_id != BNXT_INVALID_LAG_ID) {
		for_each_set_bit(idx, &binfo->peers, BNXT_PORTS_MAX) {
			if (bp->pf.port_id == idx)
				continue;
			netdev = binfo->p_netdev[idx];
			if (!netdev)
				continue;
			if (!entry->peer_entry)
				continue;
			peer_bp = (struct bnxt *)netdev_priv(netdev);
			rc = bnxt_tf_ulp_dcqcn_flow_delete(peer_bp,
							   entry->peer_entry);
			/* Free the peer copy exactly once and clear the
			 * pointer. The session entry is long lived and is
			 * reused/retried (e.g. fake_dmac changes and the
			 * create() error path which re-invokes delete()), so
			 * leaving a dangling peer_entry here results in a
			 * double free (BUG at mm/slub.c __slab_free).
			 */
			kfree(entry->peer_entry);
			entry->peer_entry = NULL;
		}
		if (rc)
			netdev_dbg(bp->dev, "DCQCN peer delete session(%d) failed(%d)\n",
				   entry->session_id, rc);
	}

	rc = bnxt_tf_ulp_dcqcn_flow_delete(bp, entry);
	if (rc)
		netdev_dbg(bp->dev, "DCQCN delete session(%d) failed(%d)\n",
			   entry->session_id, rc);

	return rc;
#else
	return -EOPNOTSUPP;
#endif
}
EXPORT_SYMBOL(bnxt_en_ulp_dcqcn_flow_delete);

void bnxt_ulp_get_stats(struct bnxt_en_dev *edev, struct bnxt_en_ulp_stats *stats)
{
	struct net_device *dev = edev->net;
	struct bnxt *bp = netdev_priv(dev);
	u64 *sw_stats;

	if (!(bp->stats_cap & BNXT_STATS_CAP_PORT_EXT))
		return;
	netdev_lock(dev);
	if (!bp->rx_port_stats_ext.sw_stats) {
		netdev_unlock(dev);
		return;
	}
	sw_stats = bp->rx_port_stats_ext.sw_stats;
	stats->link_down_events =
		*(sw_stats + BNXT_RX_STATS_EXT_OFFSET(link_down_events));
	stats->rx_pcs_symbol_err =
		*(sw_stats + BNXT_RX_STATS_EXT_OFFSET(rx_pcs_symbol_err));
	stats->rx_discard_bytes_cos =
		*(sw_stats + BNXT_RX_STATS_EXT_OFFSET(rx_discard_bytes_cos0));
	netdev_unlock(dev);
}
EXPORT_SYMBOL(bnxt_ulp_get_stats);

void bnxt_ulp_get_peer_bar_maps(struct bnxt_en_dev *edev)
{
	struct bnxt *bp = netdev_priv(edev->net);

	bnxt_hwrm_get_peer_bar_maps(bp);
}
EXPORT_SYMBOL(bnxt_ulp_get_peer_bar_maps);

void bnxt_aux_devices_uninit(struct bnxt *bp)
{
	struct bnxt_aux_priv *aux_priv;
	struct auxiliary_device *adev;
	int idx;

	mutex_lock(&bp->auxdev_lock);
	for (idx = 0; idx < __BNXT_AUXDEV_MAX; idx++) {
		if (bnxt_auxdev_is_init(bp, idx)) {
			aux_priv = bp->aux_priv[idx];
			adev = &aux_priv->aux_dev;
			auxiliary_device_uninit(adev);
			bnxt_auxdev_set_state(bp, idx, BNXT_ADEV_STATE_NONE);
		}
	}
	mutex_unlock(&bp->auxdev_lock);
}

static void bnxt_aux_dev_release(struct device *dev)
{
	struct bnxt_aux_priv *aux_priv =
		container_of(dev, struct bnxt_aux_priv, aux_dev.dev);
	struct bnxt *bp = netdev_priv(aux_priv->edev->net);

	kfree(aux_priv->edev->ulp_tbl);
	bp->edev[aux_priv->id] = NULL;
	kfree(aux_priv->edev);
	bp->aux_priv[aux_priv->id] = NULL;
	kfree(aux_priv);
}

void bnxt_aux_devices_del(struct bnxt *bp)
{
	struct bnxt_en_dev *edev;
	int idx;

	mutex_lock(&bp->auxdev_lock);
	for (idx = 0; idx < __BNXT_AUXDEV_MAX; idx++) {
		if (bnxt_auxdev_is_active(bp, idx)) {
			edev = bp->edev[idx];
			edev->en_state = bp->state;
			auxiliary_device_delete(&bp->aux_priv[idx]->aux_dev);
			bnxt_auxdev_set_state(bp, idx, BNXT_ADEV_STATE_INIT);
		}
	}
	mutex_unlock(&bp->auxdev_lock);
}

static inline void bnxt_set_edev_info(struct bnxt_en_dev *edev, struct bnxt *bp)
{
	edev->net = bp->dev;
	edev->pdev = bp->pdev;
	edev->l2_db_size = bp->db_size;
	edev->l2_db_size_nc = bp->db_size_nc;
	edev->l2_db_offset = bp->db_offset;
	mutex_init(&edev->en_dev_lock);

	if (bp->flags & BNXT_FLAG_ROCEV1_CAP)
		edev->flags |= BNXT_EN_FLAG_ROCEV1_CAP;
	if (bp->flags & BNXT_FLAG_ROCEV2_CAP)
		edev->flags |= BNXT_EN_FLAG_ROCEV2_CAP;
	if (bp->is_asym_q)
		edev->flags |= BNXT_EN_FLAG_ASYM_Q;
	if (bp->flags & BNXT_FLAG_MULTI_HOST)
		edev->flags |= BNXT_EN_FLAG_MULTI_HOST;
	if (bp->flags & BNXT_FLAG_MULTI_ROOT)
		edev->flags |= BNXT_EN_FLAG_MULTI_ROOT;
	if (BNXT_VF(bp))
		edev->flags |= BNXT_EN_FLAG_VF;
	if (bp->fw_cap & BNXT_FW_CAP_HW_LAG_SUPPORTED)
		edev->flags |= BNXT_EN_FLAG_HW_LAG;
	if (BNXT_ROCE_VF_RESC_CAP(bp))
		edev->flags |= BNXT_EN_FLAG_ROCE_VF_RES_MGMT;
	if (BNXT_SW_RES_LMT(bp))
		edev->flags |= BNXT_EN_FLAG_SW_RES_LMT;
	if (BNXT_CACHEALIGN_CMPL_SUPPORTED(bp))
		edev->flags |= BNXT_EN_FLAG_CACHEALIGN_CMPL;
	if (bp->tf_flags & BNXT_TF_FLAG_INITIALIZED)
		edev->flags |= BNXT_EN_FLAG_TF_ENABLED;
	edev->bar0 = bp->bar0;
	edev->port_partition_type = bp->port_partition_type;
	edev->port_count = bp->port_count;
	edev->pf_port_id = bp->pf.port_id;
	edev->pf_fw_id = bp->pf.fw_fid;
	edev->hw_ring_stats_size = bp->hw_ring_stats_size;
	edev->hwrm_max_ext_req_len = bp->hwrm_max_ext_req_len;
	edev->ulp_version = BNXT_ULP_VERSION;
	edev->hdbr_info = &bp->hdbr_info;
	/* Update chip type used for roce pre-init purposes */
	edev->chip_num = bp->chip_num;
	memcpy(edev->board_part_number, bp->board_partno, BNXT_VPD_FLD_LEN - 1);
	bnxt_set_ulp_resc(bp, BNXT_ULP_RES_MSIX, bp->roce_num_msix);
	bnxt_set_ulp_resc(bp, BNXT_ULP_RES_STAT_CTX, bp->roce_num_stat_ctx);
}

void bnxt_aux_devices_add(struct bnxt *bp)
{
	struct auxiliary_device *aux_dev;
	struct bnxt_en_dev *edev;
	int rc, idx;

	mutex_lock(&bp->auxdev_lock);
	for (idx = 0; idx < __BNXT_AUXDEV_MAX; idx++) {
		if (bnxt_auxdev_is_init(bp, idx)) {
			edev = bp->edev[idx];
			edev->en_state = bp->state;
			aux_dev = &bp->aux_priv[idx]->aux_dev;
			rc = auxiliary_device_add(aux_dev);
			if (rc) {
				netdev_warn(bp->dev, "Failed to add auxiliary device for auxdev type %d\n",
					    idx);
				auxiliary_device_uninit(aux_dev);
				if (idx == BNXT_AUXDEV_RDMA)
					bp->flags &= ~BNXT_FLAG_ROCE_CAP;
				continue;
			}
			bnxt_auxdev_set_state(bp, idx, BNXT_ADEV_STATE_ADD);
		}
	}
	mutex_unlock(&bp->auxdev_lock);
}

void bnxt_aux_devices_init(struct bnxt *bp)
{
	struct auxiliary_device *aux_dev;
	struct bnxt_aux_priv *aux_priv;
	struct bnxt_en_dev *edev;
	struct bnxt_ulp *ulp;
	int rc, idx;

	mutex_lock(&bp->auxdev_lock);
	for (idx = 0; idx < __BNXT_AUXDEV_MAX; idx++) {
		bnxt_auxdev_set_state(bp, idx, BNXT_ADEV_STATE_NONE);

		if (idx == BNXT_AUXDEV_RDMA &&
		    !(bp->flags & BNXT_FLAG_ROCE_CAP))
			continue;

		aux_priv = kzalloc(sizeof(*bp->aux_priv[idx]), GFP_KERNEL);
		if (!aux_priv)
			goto next_auxdev;

		edev = kzalloc(sizeof(*edev), GFP_KERNEL);
		if (!edev)
			goto free_aux_priv;

		ulp = kzalloc(sizeof(*ulp), GFP_KERNEL);
		if (!ulp)
			goto free_edev;

		aux_dev = &aux_priv->aux_dev;
		aux_dev->id = bp->auxdev_id;
		aux_dev->name = bnxt_aux_devices[idx].name;
		aux_dev->dev.parent = &bp->pdev->dev;
		aux_dev->dev.release = bnxt_aux_dev_release;

		rc = auxiliary_device_init(aux_dev);
		if (rc)
			goto free_ulp;

		aux_priv->id = idx;
		aux_priv->edev = edev;
		bp->aux_priv[idx] = aux_priv;
		bnxt_set_edev_info(edev, bp);
		edev->ulp_tbl = ulp;
		bp->edev[idx] = edev;
		if (idx == BNXT_AUXDEV_RDMA) {
			bp->ulp_num_msix_want = bnxt_set_dflt_ulp_msix(bp);
			bnxt_set_ulp_num_rings(bp, BNXT_MAX_ROCE_TS_QPS);
			bnxt_restore_ulp_rings(bp);
		}
		bnxt_auxdev_set_state(bp, idx, BNXT_ADEV_STATE_INIT);

		continue;
free_ulp:
		kfree(ulp);
free_edev:
		kfree(edev);
free_aux_priv:
		kfree(aux_priv);
next_auxdev:
		if (idx == BNXT_AUXDEV_RDMA)
			bp->flags &= ~BNXT_FLAG_ROCE_CAP;
	}
	mutex_unlock(&bp->auxdev_lock);
}

int bnxt_auxdev_id_alloc(struct bnxt *bp)
{
	bp->auxdev_id = ida_alloc(&bnxt_aux_dev_ids, GFP_KERNEL);
	if (bp->auxdev_id < 0)
		return bp->auxdev_id;

	return 0;
}

void bnxt_auxdev_id_free(struct bnxt *bp, int id)
{
	if (bp->auxdev_id >= 0)
		ida_free(&bnxt_aux_dev_ids, id);
}

void bnxt_remove_aux_devices(struct bnxt *bp)
{
	bnxt_aux_devices_del(bp);
	bnxt_aux_devices_uninit(bp);
}

void bnxt_recreate_aux_devices(struct bnxt *bp)
{
	if (bp->auxdev_id >= 0) {
		bnxt_aux_devices_init(bp);
		bnxt_aux_devices_add(bp);
	}
}

/* When HW LAG is created, RoCE driver instance is not registered on L2 PF1.
 * This is because RoCE driver creates only one IB device for the bond device.
 * Check whether the RoCE driver is loaded on the PF1 RDMA auxiliary device to
 * prevent clearing resources reserved for RoCE on PF1 when LAG is created.
 */
static bool bnxt_ulp_lag_registered(struct bnxt_en_dev *edev)
{
	struct bnxt *bp = netdev_priv(edev->net);
	struct bnxt_bond_info *binfo = bp->bond_info;
	struct bnxt_aux_priv *bnxt_aux;
	struct auxiliary_device *adev;

	if (!binfo || !binfo->bond_active)
		return false;

	bnxt_aux = bp->aux_priv[BNXT_AUXDEV_RDMA];
	if (!bnxt_aux)
		return false;

	adev = &bnxt_aux->aux_dev;
	device_lock(&adev->dev);
	/* Auxiliary driver is bound, which means RoCE driver is loaded */
	if (adev->dev.driver) {
		device_unlock(&adev->dev);
		return true;
	}

	device_unlock(&adev->dev);
	return false;
}

bool bnxt_ulp_registered(struct bnxt_en_dev *edev)
{
	struct bnxt *bp;

	if (!edev)
		return false;

	if (rcu_access_pointer(edev->ulp_tbl->ulp_ops))
		return true;

	bp = netdev_priv(edev->net);
	/* Auxiliary device is initialized, yet to add - error recovery scenario */
	if (bnxt_auxdev_is_init(bp, BNXT_AUXDEV_RDMA))
		return true;

	if (bnxt_ulp_lag_registered(edev))
		return true;

	return false;
}
