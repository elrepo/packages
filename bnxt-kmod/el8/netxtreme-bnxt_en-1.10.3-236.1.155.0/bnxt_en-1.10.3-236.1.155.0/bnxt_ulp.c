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

static DEFINE_IDA(bnxt_rdma_aux_dev_ids);
static DEFINE_IDA(bnxt_fwctl_aux_dev_ids);

struct bnxt_aux_device {
	const char *name;
	const u32 id;
	u32 (*alloc_ida)(void);
	void (*free_ida)(struct bnxt_aux_priv *priv);
	void (*release)(struct device *dev);
	void (*set_priv)(struct bnxt *bp, struct bnxt_aux_priv *priv);
	struct bnxt_aux_priv *(*get_priv)(struct bnxt *bp);
	void (*set_edev)(struct bnxt *bp, struct bnxt_en_dev *edev);
	struct bnxt_en_dev *(*get_edev)(struct bnxt *bp);
	struct auxiliary_device *(*get_auxdev)(struct bnxt *bp);
};

static void bnxt_aux_dev_release(struct device *dev);
static void bnxt_fwctl_aux_dev_release(struct device *dev);

static void bnxt_rdma_aux_dev_set_priv(struct bnxt *bp, struct bnxt_aux_priv *priv)
{
	bp->aux_priv_rdma = priv;
}

static struct bnxt_aux_priv *bnxt_rdma_aux_dev_get_priv(struct bnxt *bp)
{
	return bp->aux_priv_rdma;
}

static struct auxiliary_device *bnxt_rdma_aux_dev_get_auxdev(struct bnxt *bp)
{
	return &bp->aux_priv_rdma->aux_dev;
}

static void bnxt_rdma_aux_dev_set_edev(struct bnxt *bp, struct bnxt_en_dev *edev)
{
	bp->edev_rdma = edev;
}

static struct bnxt_en_dev *bnxt_rdma_aux_dev_get_edev(struct bnxt *bp)
{
	return bp->edev_rdma;
}

static u32 bnxt_rdma_aux_dev_alloc_ida(void)
{
	return ida_alloc(&bnxt_rdma_aux_dev_ids, GFP_KERNEL);
}

static void bnxt_rdma_aux_dev_free_ida(struct bnxt_aux_priv *aux_priv)
{
	ida_free(&bnxt_rdma_aux_dev_ids, aux_priv->id);
}

static void bnxt_fwctl_aux_dev_set_priv(struct bnxt *bp, struct bnxt_aux_priv *priv)
{
	bp->aux_priv_fwctl = priv;
}

static struct bnxt_aux_priv *bnxt_fwctl_aux_dev_get_priv(struct bnxt *bp)
{
	return bp->aux_priv_fwctl;
}

static struct auxiliary_device *bnxt_fwctl_aux_dev_get_auxdev(struct bnxt *bp)
{
	return &bp->aux_priv_fwctl->aux_dev;
}

static void bnxt_fwctl_aux_dev_set_edev(struct bnxt *bp, struct bnxt_en_dev *edev)
{
	bp->edev_fwctl = edev;
}

static struct bnxt_en_dev *bnxt_fwctl_aux_dev_get_edev(struct bnxt *bp)
{
	return bp->edev_fwctl;
}

static u32 bnxt_fwctl_aux_dev_alloc_ida(void)
{
	return ida_alloc(&bnxt_fwctl_aux_dev_ids, GFP_KERNEL);
}

static void bnxt_fwctl_aux_dev_free_ida(struct bnxt_aux_priv *aux_priv)
{
	ida_free(&bnxt_fwctl_aux_dev_ids, aux_priv->id);
}

static struct bnxt_aux_device bnxt_aux_devices[__BNXT_AUXDEV_MAX] = {{
	.name		= "rdma",
	.alloc_ida	= bnxt_rdma_aux_dev_alloc_ida,
	.free_ida	= bnxt_rdma_aux_dev_free_ida,
	.release	= bnxt_aux_dev_release,
	.set_priv       = bnxt_rdma_aux_dev_set_priv,
	.get_priv	= bnxt_rdma_aux_dev_get_priv,
	.set_edev       = bnxt_rdma_aux_dev_set_edev,
	.get_edev	= bnxt_rdma_aux_dev_get_edev,
	.get_auxdev	= bnxt_rdma_aux_dev_get_auxdev,
}, {
	.name		= "fwctl",
	.alloc_ida	= bnxt_fwctl_aux_dev_alloc_ida,
	.free_ida	= bnxt_fwctl_aux_dev_free_ida,
	.release	= bnxt_fwctl_aux_dev_release,
	.set_priv       = bnxt_fwctl_aux_dev_set_priv,
	.get_priv	= bnxt_fwctl_aux_dev_get_priv,
	.set_edev       = bnxt_fwctl_aux_dev_set_edev,
	.get_edev	= bnxt_fwctl_aux_dev_get_edev,
	.get_auxdev	= bnxt_fwctl_aux_dev_get_auxdev,

}};

static void bnxt_fill_msix_vecs(struct bnxt *bp, struct bnxt_msix_entry *ent)
{
	struct bnxt_en_dev *edev = bp->edev_rdma;
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

int bnxt_get_ulp_msix_num(struct bnxt *bp)
{
	if (bp->edev_rdma)
		return bp->edev_rdma->ulp_num_msix_vec;
	return 0;
}

void bnxt_set_ulp_msix_num(struct bnxt *bp, int num)
{
	if (bp->edev_rdma)
		bp->edev_rdma->ulp_num_msix_vec = num;
}

int bnxt_get_ulp_msix_num_in_use(struct bnxt *bp)
{
	if (bnxt_ulp_registered(bp->edev_rdma))
		return bp->edev_rdma->ulp_num_msix_vec;
	return 0;
}

int bnxt_get_ulp_stat_ctxs(struct bnxt *bp)
{
	if (bp->edev_rdma)
		return bp->edev_rdma->ulp_num_ctxs;
	return 0;
}

void bnxt_set_ulp_stat_ctxs(struct bnxt *bp, int num_ulp_ctx)
{
	if (bp->edev_rdma)
		bp->edev_rdma->ulp_num_ctxs = num_ulp_ctx;
}

int bnxt_get_ulp_stat_ctxs_in_use(struct bnxt *bp)
{
	if (bnxt_ulp_registered(bp->edev_rdma))
		return bp->edev_rdma->ulp_num_ctxs;
	return 0;
}

void bnxt_set_dflt_ulp_stat_ctxs(struct bnxt *bp)
{
	if (bp->edev_rdma) {
		bp->edev_rdma->ulp_num_ctxs = BNXT_MIN_ROCE_STAT_CTXS;
		/* Reserve one additional stat_ctx for PF0 (except
		 * on 1-port NICs) as it also creates one stat_ctx
		 * for PF1 in case of RoCE bonding.
		 */
		if (BNXT_PF(bp) && !bp->pf.port_id &&
		    bp->port_count > 1)
			bp->edev_rdma->ulp_num_ctxs++;

		/*
		 * Reserve one stat_ctx for mirror on RoCE support
		 */
		if (BNXT_MIRROR_ON_ROCE_CAP(bp))
			bp->edev_rdma->ulp_num_ctxs++;
	}
}

int bnxt_register_dev(struct bnxt_en_dev *edev,
		      struct bnxt_ulp_ops *ulp_ops, void *handle)
{
	struct net_device *dev = edev->net;
	struct bnxt *bp = netdev_priv(dev);
	unsigned int max_stat_ctxs;
	struct bnxt_ulp *ulp;
	int rc = 0;

	netdev_lock(dev);
	if (!bp->irq_tbl) {
		rc = -ENODEV;
		goto exit;
	}
	max_stat_ctxs = bnxt_get_max_func_stat_ctxs(bp);
	if (max_stat_ctxs <= BNXT_MIN_ROCE_STAT_CTXS ||
	    bp->cp_nr_rings == max_stat_ctxs) {
		rc = -ENOMEM;
		goto exit;
	}

	ulp = edev->ulp_tbl;
	ulp->handle = handle;
	rcu_assign_pointer(ulp->ulp_ops, ulp_ops);

	if (test_bit(BNXT_STATE_OPEN, &bp->state))
		bnxt_hwrm_vnic_cfg(bp, &bp->vnic_info[0], 0);

	edev->ulp_tbl->msix_requested = bnxt_get_ulp_msix_num(bp);

	bnxt_fill_msix_vecs(bp, bp->edev_rdma->msix_entries);
	edev->flags |= BNXT_EN_FLAG_MSIX_REQUESTED;
exit:
	netdev_unlock(dev);
	return rc;
}
EXPORT_SYMBOL(bnxt_register_dev);

void bnxt_unregister_dev(struct bnxt_en_dev *edev)
{
	struct net_device *dev = edev->net;
	struct bnxt *bp = netdev_priv(dev);
	struct bnxt_ulp *ulp;

	ulp = edev->ulp_tbl;
	netdev_lock(dev);
	if (ulp->msix_requested)
		edev->flags &= ~BNXT_EN_FLAG_MSIX_REQUESTED;
	edev->ulp_tbl->msix_requested = 0;

	if (ulp->max_async_event_id)
		bnxt_hwrm_func_drv_rgtr(bp, NULL, 0, true);

	RCU_INIT_POINTER(ulp->ulp_ops, NULL);
	synchronize_rcu();
	ulp->max_async_event_id = 0;
	ulp->async_events_bmap = NULL;
	netdev_unlock(dev);
	return;
}
EXPORT_SYMBOL(bnxt_unregister_dev);

static int bnxt_set_dflt_ulp_msix(struct bnxt *bp)
{
	int roce_msix = BNXT_MAX_ROCE_MSIX;

	if (BNXT_VF(bp))
		roce_msix = BNXT_MAX_ROCE_MSIX_VF;
	else if (bp->port_partition_type)
		roce_msix = BNXT_MAX_ROCE_MSIX_NPAR_PF;

#ifdef BNXT_FPGA
	roce_msix = min(roce_msix, BNXT_MAX_ROCE_MSIX_PF - 1);
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
	rc = hwrm_req_send(bp, req);
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

void bnxt_ulp_stop(struct bnxt *bp)
{
	struct bnxt_aux_priv *bnxt_aux = bp->aux_priv_rdma;
	struct bnxt_en_dev *edev = bp->edev_rdma;

	if (!edev)
		return;
	mutex_lock(&edev->en_dev_lock);
	/* This check is needed for RoCE lag case */
	if (!bnxt_ulp_registered(edev)) {
		mutex_unlock(&edev->en_dev_lock);
		return;
	}

	edev->flags |= BNXT_EN_FLAG_ULP_STOPPED;
	edev->en_state = bp->state;
	if (bnxt_aux) {
		struct auxiliary_device *adev;

		adev = &bnxt_aux->aux_dev;
		if (adev->dev.driver) {
			const struct auxiliary_driver *adrv;
			pm_message_t pm = {};

			adrv = to_auxiliary_drv(adev->dev.driver);
			if (adrv->suspend)
				adrv->suspend(adev, pm);
		}
	}
	if (bp->edev_fwctl)
		bp->edev_fwctl->flags |= BNXT_EN_FLAG_ULP_STOPPED;
	mutex_unlock(&edev->en_dev_lock);
}

void bnxt_ulp_start(struct bnxt *bp, int err)
{
	struct bnxt_aux_priv *bnxt_aux = bp->aux_priv_rdma;
	struct bnxt_en_dev *edev = bp->edev_rdma;

	if (!edev)
		return;

	edev->flags &= ~BNXT_EN_FLAG_ULP_STOPPED;
	edev->en_state = bp->state;

	if (err)
		return;

	mutex_lock(&edev->en_dev_lock);
	/* This check is needed for RoCE lag case */
	if (!bnxt_ulp_registered(edev)) {
		mutex_unlock(&edev->en_dev_lock);
		return;
	}

	bnxt_fill_msix_vecs(bp, bp->edev_rdma->msix_entries);

	if (bnxt_aux) {
		struct auxiliary_device *adev;

		adev = &bnxt_aux->aux_dev;
		if (adev->dev.driver) {
			const struct auxiliary_driver *adrv;

			adrv = to_auxiliary_drv(adev->dev.driver);
			if (adrv->resume)
				adrv->resume(adev);
		}
	}
	if (bp->edev_fwctl)
		bp->edev_fwctl->flags &= ~BNXT_EN_FLAG_ULP_STOPPED;
	mutex_unlock(&edev->en_dev_lock);
}

/*
 * In kernels where native Auxbus infrastructure support is not there,
 * invoke the auxiliary_driver shutdown function.
 */
#ifndef HAVE_AUXILIARY_DRIVER
void bnxt_ulp_shutdown(struct bnxt *bp)
{
	struct bnxt_aux_priv *bnxt_aux = bp->aux_priv_rdma;
	struct bnxt_en_dev *edev = bp->edev_rdma;

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
	struct bnxt_en_dev *edev = bp->edev_rdma;
	struct bnxt_ulp_ops *ops;
	bool reset = false;

	if (!edev)
		return;

	if (bnxt_ulp_registered(bp->edev_rdma)) {
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
	struct bnxt_en_dev *edev = bp->edev_rdma;
	struct bnxt_ulp_ops *ops;

	if (!edev)
		return;

	if (bnxt_ulp_registered(bp->edev_rdma)) {
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
	struct bnxt_en_dev *edev = bp->edev_rdma;
	struct bnxt_ulp_dump *dump;
	struct bnxt_ulp_ops *ops;
	struct bnxt_ulp *ulp;
	u32 i, dump_len = 0;

	*segs = 0;
	if (!edev || !bnxt_ulp_registered(edev))
		return 0;

	ulp = edev->ulp_tbl;
	ops = rtnl_dereference(ulp->ulp_ops);
	if (!ops || !ops->ulp_get_dump_info || !ops->ulp_get_dump_data)
		return 0;

	dump = &ulp->ulp_dump;
	if (!buf) {
		memset(dump, 0, sizeof(*dump));
		ops->ulp_get_dump_info(ulp->handle, dump_flag, dump);
		if (dump->segs > BNXT_ULP_MAX_DUMP_SEGS)
			return 0;
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
	return dump_len;
}

void bnxt_ulp_async_events(struct bnxt *bp, struct hwrm_async_event_cmpl *cmpl)
{
	u16 event_id = le16_to_cpu(cmpl->event_id);
	struct bnxt_en_dev *edev = bp->edev_rdma;
	struct bnxt_ulp_ops *ops;
	struct bnxt_ulp *ulp;

	if (!bnxt_ulp_registered(edev))
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

void bnxt_dbr_complete(struct bnxt_en_dev *edev, u32 epoch)
{
	struct bnxt *bp = netdev_priv(edev->net);

	bnxt_dbr_recovery_done(bp, epoch, BNXT_ROCE_ULP);
}
EXPORT_SYMBOL(bnxt_dbr_complete);

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
			entry->peer_entry = bentry;
			rc = bnxt_tf_ulp_dcqcn_flow_create(peer_bp, bentry);
			if (rc)
				goto error;
		}
	}

	rc = bnxt_tf_ulp_dcqcn_flow_create(bp, entry);
	if (rc)
		goto error;

	return rc;
error:
	netdev_dbg(bp->dev, "DCQCN create session(%d) failed(%d)\n",
		   entry->session_id, rc);
	bnxt_en_ulp_dcqcn_flow_delete(edev, entry);
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
			peer_bp = (struct bnxt *)netdev_priv(netdev);
			if (entry->peer_entry) {
				rc = bnxt_tf_ulp_dcqcn_flow_delete(peer_bp,
								   entry->peer_entry);
				kfree(entry->peer_entry);
			}
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

void bnxt_aux_device_uninit(struct bnxt *bp, enum bnxt_ulp_auxdev_type auxdev_type)
{
	struct bnxt_aux_priv *aux_priv;
	struct auxiliary_device *adev;

	if (auxdev_type >= __BNXT_AUXDEV_MAX) {
		netdev_warn(bp->dev, "Failed to uninit: unrecognized auxiliary device\n");
		return;
	}
	/* Skip if no auxiliary device init was done. */
	if (!bnxt_aux_devices[auxdev_type].get_priv(bp))
		return;

	aux_priv = bnxt_aux_devices[auxdev_type].get_priv(bp);
	adev = &aux_priv->aux_dev;
	auxiliary_device_uninit(adev);
}

static void bnxt_aux_dev_release(struct device *dev)
{
	struct bnxt_aux_priv *aux_priv =
		container_of(dev, struct bnxt_aux_priv, aux_dev.dev);
	struct bnxt *bp = netdev_priv(aux_priv->edev->net);

	ida_free(&bnxt_rdma_aux_dev_ids, aux_priv->id);
	kfree(aux_priv->edev->ulp_tbl);
	kfree(aux_priv->edev);
	bp->edev_rdma = NULL;
	kfree(bp->aux_priv_rdma);
	bp->aux_priv_rdma = NULL;
}

void bnxt_aux_device_del(struct bnxt *bp, enum bnxt_ulp_auxdev_type auxdev_type)
{
	if (auxdev_type >= __BNXT_AUXDEV_MAX) {
		netdev_warn(bp->dev, "Failed to del: unrecognized auxiliary device\n");
		return;
	}
	if (!bnxt_aux_devices[auxdev_type].get_edev(bp))
		return;

	auxiliary_device_delete(bnxt_aux_devices[auxdev_type].get_auxdev(bp));
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
	edev->bar0 = bp->bar0;
	edev->port_partition_type = bp->port_partition_type;
	edev->port_count = bp->port_count;
	edev->pf_port_id = bp->pf.port_id;
	edev->hw_ring_stats_size = bp->hw_ring_stats_size;
	edev->ulp_version = BNXT_ULP_VERSION;
	edev->en_dbr = &bp->dbr;
	edev->hdbr_info = &bp->hdbr_info;
	/* Update chip type used for roce pre-init purposes */
	edev->chip_num = bp->chip_num;
	memcpy(edev->board_part_number, bp->board_partno, BNXT_VPD_FLD_LEN - 1);
}

void bnxt_aux_device_add(struct bnxt *bp, enum bnxt_ulp_auxdev_type auxdev_type)
{
	struct auxiliary_device *aux_dev;
	int rc;

	if (auxdev_type >= __BNXT_AUXDEV_MAX) {
		netdev_warn(bp->dev, "Failed to add: unrecognized auxiliary device\n");
		return;
	}
	if (!bnxt_aux_devices[auxdev_type].get_edev(bp))
		return;

	aux_dev = bnxt_aux_devices[auxdev_type].get_auxdev(bp);
	rc = auxiliary_device_add(aux_dev);
	if (rc) {
		netdev_warn(bp->dev, "Failed to add auxiliary device for auxdev type %d\n",
			    auxdev_type);
		auxiliary_device_uninit(aux_dev);
		if (auxdev_type == BNXT_AUXDEV_RDMA)
			bp->flags &= ~BNXT_FLAG_ROCE_CAP;
	}
}

static void bnxt_fwctl_aux_dev_release(struct device *dev)
{
	struct bnxt_aux_priv *aux_priv =
		container_of(dev, struct bnxt_aux_priv, aux_dev.dev);
	struct bnxt *bp = netdev_priv(aux_priv->edev->net);

	ida_free(&bnxt_fwctl_aux_dev_ids, aux_priv->id);
	kfree(aux_priv->edev);
	bp->edev_fwctl = NULL;
	kfree(bp->aux_priv_fwctl);
	bp->aux_priv_fwctl = NULL;
}

void bnxt_aux_device_init(struct bnxt *bp,
			  enum bnxt_ulp_auxdev_type auxdev_type)
{
	struct auxiliary_device *aux_dev;
	struct bnxt_aux_priv *aux_priv;
	struct bnxt_en_dev *edev;
	struct bnxt_ulp *ulp;
	int rc;

	if (auxdev_type >= __BNXT_AUXDEV_MAX) {
		netdev_warn(bp->dev, "Failed to init: unrecognized auxiliary device\n");
		return;
	}
	if (auxdev_type == BNXT_AUXDEV_RDMA &&
	    !(bp->flags & BNXT_FLAG_ROCE_CAP))
		return;

	aux_priv = kzalloc(sizeof(*bp->aux_priv_rdma), GFP_KERNEL);
	if (!aux_priv)
		goto exit;

	aux_priv->id = bnxt_aux_devices[auxdev_type].alloc_ida();
	if (aux_priv->id < 0) {
		netdev_warn(bp->dev, "ida alloc failed for %d auxiliary device\n", auxdev_type);
		kfree(aux_priv);
		goto exit;
	}

	aux_dev = &aux_priv->aux_dev;
	aux_dev->id = aux_priv->id;
	aux_dev->name = bnxt_aux_devices[auxdev_type].name;
	aux_dev->dev.parent = &bp->pdev->dev;
	aux_dev->dev.release = bnxt_aux_devices[auxdev_type].release;

	rc = auxiliary_device_init(aux_dev);
	if (rc) {
		bnxt_aux_devices[auxdev_type].free_ida(aux_priv);
		kfree(aux_priv);
		goto exit;
	}
	bnxt_aux_devices[auxdev_type].set_priv(bp, aux_priv);

	/* From this point, all cleanup will happen via the .release callback &
	 * any error unwinding will need to include a call to
	 * auxiliary_device_uninit.
	 */
	edev = kzalloc(sizeof(*edev), GFP_KERNEL);
	if (!edev)
		goto aux_dev_uninit;

	aux_priv->edev = edev;

	ulp = kzalloc(sizeof(*ulp), GFP_KERNEL);
	if (!ulp)
		goto aux_dev_uninit;

	edev->ulp_tbl = ulp;
	bnxt_aux_devices[auxdev_type].set_edev(bp, edev);
	bnxt_set_edev_info(edev, bp);
	if (auxdev_type == BNXT_AUXDEV_RDMA)
		bp->ulp_num_msix_want = bnxt_set_dflt_ulp_msix(bp);

	return;

aux_dev_uninit:
	auxiliary_device_uninit(aux_dev);
exit:
	if (auxdev_type == BNXT_AUXDEV_RDMA)
		bp->flags &= ~BNXT_FLAG_ROCE_CAP;
}
