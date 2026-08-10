/* Broadcom NetXtreme-C/E network driver.
 *
 * Copyright (c) 2024 Broadcom Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 */
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#ifdef HAVE_NDO_XDP
#include <linux/bpf.h>
#ifdef HAVE_BPF_TRACE
#include <linux/bpf_trace.h>
#endif
#include <linux/filter.h>
#endif
#ifdef CONFIG_PAGE_POOL
#ifdef HAVE_PAGE_POOL_HELPERS_H
#include <net/page_pool/helpers.h>
#else
#include <net/page_pool.h>
#endif
#endif
#include "bnxt_compat.h"
#include "bnxt_hsi.h"
#include "bnxt.h"
#include "bnxt_xdp.h"
#include "bnxt_xsk.h"

#if defined(CONFIG_XDP_SOCKETS) && defined(HAVE_NDO_BPF) && defined(HAVE_XSK_SUPPORT)
int bnxt_xsk_wakeup(struct net_device *dev, u32 queue_id, u32 flags)
{
	struct bnxt *bp = netdev_priv(dev);
	struct bnxt_cp_ring_info *cpr;
	struct bnxt_rx_ring_info *rxr;
	struct bnxt_tx_ring_info *txr;
	struct bnxt_napi *bnapi;

	if (!test_bit(BNXT_STATE_OPEN, &bp->state))
		return -ENETDOWN;

	if (queue_id >= bp->rx_nr_rings || queue_id >= bp->tx_nr_rings_xdp)
		return -EINVAL;

	rxr = &bp->rx_ring[queue_id];
	txr = &bp->tx_ring[queue_id];

	if (!rxr->xsk_pool && !txr->xsk_pool)
		return -ENXIO;

	bnapi = bp->bnapi[queue_id];
	cpr = &bnapi->cp_ring;
	if (!napi_if_scheduled_mark_missed(&bnapi->napi)) {
		cpr->sw_stats->xsk_stats.xsk_wakeup++;
		napi_schedule(&bnapi->napi);
	}

	return 0;
}

static void bnxt_xsk_disable_rx_ring(struct bnxt *bp, u16 queue_id)
{
	struct bnxt_rx_ring_info *rxr;
	struct bnxt_vnic_info *vnic;
	struct bnxt_napi *bnapi;

	rxr = &bp->rx_ring[queue_id];
	bnapi = rxr->bnapi;
	vnic = &bp->vnic_info[BNXT_VNIC_NTUPLE];

#ifdef HAVE_XDP_RXQ_INFO
	if (xdp_rxq_info_is_reg(&rxr->xdp_rxq))
		xdp_rxq_info_unreg(&rxr->xdp_rxq);
#endif
	vnic->mru = 0;
	bnxt_hwrm_vnic_update(bp, vnic, VNIC_UPDATE_REQ_ENABLES_MRU_VALID);
	napi_disable_locked(&bnapi->napi);
	bnxt_free_one_rx_ring(bp, rxr);
	bnxt_hwrm_rx_ring_free(bp, rxr, 0);
}

static int bnxt_xsk_enable_rx_ring(struct bnxt *bp,  u16 queue_id)
{
	struct bnxt_rx_ring_info *rxr;
	struct bnxt_vnic_info *vnic;
	struct bnxt_napi *bnapi;
	int rc, i;
	u32 prod;

	rxr = &bp->rx_ring[queue_id];
	bnapi = rxr->bnapi;
	vnic = &bp->vnic_info[BNXT_VNIC_NTUPLE];

#ifdef HAVE_XDP_RXQ_INFO
	rc = xdp_rxq_info_reg(&rxr->xdp_rxq, bp->dev, queue_id, 0);
	if (rc < 0)
		return rc;

	rxr->xsk_pool = xsk_get_pool_from_qid(bp->dev, queue_id);
	if (BNXT_RING_RX_ZC_MODE(rxr) && rxr->xsk_pool) {
		rc = xdp_rxq_info_reg_mem_model(&rxr->xdp_rxq,
						MEM_TYPE_XSK_BUFF_POOL, NULL);
		xsk_pool_set_rxq_info(rxr->xsk_pool, &rxr->xdp_rxq);
		netdev_dbg(bp->dev, "%s(): AF_XDP_ZC flag set for rxring:%d\n", __func__, queue_id);
	} else {
		rc = xdp_rxq_info_reg_mem_model(&rxr->xdp_rxq,
						MEM_TYPE_PAGE_POOL, rxr->page_pool);
		netdev_dbg(bp->dev, "%s(): AF_XDP_ZC flag RESET for rxring:%d\n",
			   __func__, queue_id);
	}
#endif
	rxr->rx_next_cons = 0;
	bnxt_hwrm_rx_ring_alloc(bp, rxr);

	rxr->rx_prod = 0;
	prod = rxr->rx_prod;
	for (i = 0; i < bp->rx_ring_size; i++) {
		if (bnxt_alloc_rx_data(bp, rxr, prod, GFP_KERNEL)) {
			netdev_warn(bp->dev, "init'ed rx ring %d with %d/%d skbs only\n",
				    queue_id, i, bp->rx_ring_size);
			break;
		}
		prod = NEXT_RX(prod);
	}
	rxr->rx_prod = prod;
	netdev_dbg(bp->dev, "%s: XDP db_key 0x%llX, rx_prod 0x%x, queue_id %d\n",
		   __func__, rxr->rx_db.db_key64, rxr->rx_prod, queue_id);
	bnxt_db_write(bp, &rxr->rx_db, rxr->rx_prod);
	napi_enable_locked(&bnapi->napi);
	vnic->mru = bp->dev->mtu + VLAN_ETH_HLEN;
	bnxt_hwrm_vnic_update(bp, vnic, VNIC_UPDATE_REQ_ENABLES_MRU_VALID);

	return rc;
}

static bool bnxt_check_xsk_q_in_dflt_vnic(struct bnxt *bp, u16 queue_id)
{
	u16 tbl_size, i;

	tbl_size = bnxt_get_rxfh_indir_size(bp->dev);

	for (i = 0; i < tbl_size; i++) {
		if (queue_id == bp->rss_indir_tbl[i]) {
			netdev_err(bp->dev,
				   "queue_id: %d is in default RSS context, not supported\n",
				   queue_id);
			return true;
		}
	}
	return false;
}

static int bnxt_validate_xsk(struct bnxt *bp, u16 queue_id)
{
#ifndef CONFIG_BNXT_CUSTOM_FLOWER_OFFLOAD
	if (!(bp->flags & BNXT_FLAG_RFS)) {
		netdev_err(bp->dev,
			   "nTUPLE feature needs to be on for AF_XDP support\n");
		return -EOPNOTSUPP;
	}
#endif
	if (bp->num_rss_ctx) {
		netdev_err(bp->dev,
			   "AF_XDP not supported with additional RSS contexts\n");
		return -EOPNOTSUPP;
	}

	if (bnxt_check_xsk_q_in_dflt_vnic(bp, queue_id))
		return -EOPNOTSUPP;

	return 0;
}

static int bnxt_xdp_enable_pool(struct bnxt *bp, struct xsk_buff_pool *pool,
				u16 queue_id)
{
	struct bpf_prog *xdp_prog = READ_ONCE(bp->xdp_prog);
	struct device *dev = &bp->pdev->dev;
	struct bnxt_rx_ring_info *rxr;
	bool needs_reset;
	int rc;

	rc = bnxt_validate_xsk(bp, queue_id);
	if (rc)
		return rc;

	rxr = &bp->rx_ring[queue_id];
	rc = xsk_pool_dma_map(pool, dev, DMA_ATTR_SKIP_CPU_SYNC | DMA_ATTR_WEAK_ORDERING);
	if (rc) {
		netdev_err(bp->dev, "Failed to map xsk pool\n");
		return rc;
	}

	set_bit(queue_id, bp->af_xdp_zc_qs);
	/* Check if XDP program is already attached, in which case
	 * need to explicitly quiesce traffic, free the regular path
	 * resources and reallocate AF_XDP resources for the rings.
	 * Otherwise, in the normal case, resources for AF_XDP will
	 * get created anyway as part of the XDP program attach
	 */
	needs_reset = netif_running(bp->dev) && xdp_prog;

	if (needs_reset) {
		/* Check to differentiate b/n Tx/Rx only modes */
		if (xsk_buff_can_alloc(pool, bp->rx_ring_size)) {
			bnxt_xsk_disable_rx_ring(bp, queue_id);
			rxr->flags |= BNXT_RING_FLAG_AF_XDP_ZC;
			bnxt_xsk_enable_rx_ring(bp, queue_id);
		} else {
			struct bnxt_tx_ring_info *txr = &bp->tx_ring[queue_id];
			struct bnxt_napi *bnapi;

			bnapi = bp->bnapi[queue_id];
			bnxt_lock_napi(bnapi);
			txr->xsk_pool = xsk_get_pool_from_qid(bp->dev, queue_id);
			bnxt_unlock_napi(bnapi);
		}
	}

	return rc;
}

static int bnxt_xdp_disable_pool(struct bnxt *bp, u16 queue_id)
{
	struct bpf_prog *xdp_prog = READ_ONCE(bp->xdp_prog);
	struct bnxt_rx_ring_info *rxr;
	struct bnxt_tx_ring_info *txr;
	struct xsk_buff_pool *pool;
	struct bnxt_napi *bnapi;
	bool needs_reset;

	pool = xsk_get_pool_from_qid(bp->dev, queue_id);
	if (!pool)
		return -EINVAL;

	if (!bp->bnapi ||
	    test_bit(BNXT_STATE_NAPI_DISABLED, &bp->state)) {
		xsk_pool_dma_unmap(pool, DMA_ATTR_SKIP_CPU_SYNC | DMA_ATTR_WEAK_ORDERING);
		return 0;
	}

	rxr = &bp->rx_ring[queue_id];
	txr = &bp->tx_ring[queue_id];

	bnapi = bp->bnapi[queue_id];

	bnxt_lock_napi(bnapi);
	clear_bit(queue_id, bp->af_xdp_zc_qs);
	xsk_pool_dma_unmap(pool, DMA_ATTR_SKIP_CPU_SYNC | DMA_ATTR_WEAK_ORDERING);

	needs_reset = netif_running(bp->dev) && xdp_prog;

	if (needs_reset) {
		if (xsk_buff_can_alloc(pool, bp->rx_ring_size)) {
			bnxt_xsk_disable_rx_ring(bp, queue_id);
			rxr->flags &= ~BNXT_RING_FLAG_AF_XDP_ZC;
			bnxt_xsk_enable_rx_ring(bp, queue_id);
		}
	}
	txr->xsk_pool = NULL;

	bnxt_unlock_napi(bnapi);
	return 0;
}

int bnxt_xdp_setup_pool(struct bnxt *bp, struct xsk_buff_pool *pool,
			u16 queue_id)
{
	if (queue_id >= bp->rx_nr_rings)
		return -EINVAL;

	return pool ? bnxt_xdp_enable_pool(bp, pool, queue_id) :
		bnxt_xdp_disable_pool(bp, queue_id);
}

/* returns the following:
 * true    - packet consumed by XDP and new buffer is allocated.
 * false   - packet should be passed to the stack.
 */
bool bnxt_rx_xsk(struct bnxt *bp, struct bnxt_rx_ring_info *rxr, u16 cons,
		 struct xdp_buff *xdp, u8 **data_ptr, unsigned int *len, u8 *event)
{
	struct bpf_prog *xdp_prog = READ_ONCE(bp->xdp_prog);
	struct bnxt_cp_ring_info *cpr;
	struct bnxt_tx_ring_info *txr;
	struct bnxt_sw_rx_bd *rx_buf;
	struct bnxt_napi *bnapi;
	struct pci_dev *pdev;
	dma_addr_t mapping;
	u32 tx_needed = 1;
	void *orig_data;
	u32 tx_avail;
	u32 offset;
	u32 act;

	if (!xdp_prog)
		return false;

	pdev = bp->pdev;
	offset = bp->rx_offset;

	txr = rxr->bnapi->tx_ring[0];
	xdp->data_end = xdp->data + *len;

	orig_data = xdp->data;

	xsk_buff_dma_sync_for_cpu(xdp);

	act = bpf_prog_run_xdp(xdp_prog, xdp);

	tx_avail = bnxt_tx_avail(bp, txr);
	/* If there are pending XDP_TX packets, we must not update the rx
	 * producer yet because some RX buffers may still be on the TX ring.
	 */
	if (txr->xdp_tx_pending)
		*event &= ~BNXT_RX_EVENT;

	tx_avail = bnxt_tx_avail(bp, txr);
	/* If there are pending XDP_TX packets, we must not update the rx
	 * producer yet because some RX buffers may still be on the TX ring.
	 */
	if (txr->xdp_tx_pending)
		*event &= ~BNXT_RX_EVENT;

#if XDP_PACKET_HEADROOM
	*len = xdp->data_end - xdp->data;
	if (orig_data != xdp->data) {
		offset = xdp->data - xdp->data_hard_start;
		*data_ptr = xdp->data_hard_start + offset;
	}
#endif
	bnapi = rxr->bnapi;
	cpr = &bnapi->cp_ring;

	switch (act) {
	case XDP_PASS:
		return false;

	case XDP_TX:
		rx_buf = &rxr->rx_buf_ring[cons];
		mapping = rx_buf->mapping - bp->rx_dma_offset;
		*event = 0;

		if (tx_avail < tx_needed) {
			trace_xdp_exception(bp->dev, xdp_prog, act);
			bnxt_reuse_rx_data(rxr, cons, xdp);
			return true;
		}

		dma_sync_single_for_device(&pdev->dev, mapping + offset, *len,
					   bp->rx_dir);

		*event &= ~BNXT_RX_EVENT;
		*event |= BNXT_TX_EVENT;
		/* Pass NULL as xdp->data here is buffer from the XSK pool i.e userspace */
		__bnxt_xmit_xdp(bp, txr, mapping + offset, *len,
				NEXT_RX(rxr->rx_prod), NULL);
		bnxt_reuse_rx_data(rxr, cons, xdp);
		return true;

	case XDP_REDIRECT:
		/* if we are calling this here then we know that the
		 * redirect is coming from a frame received by the
		 * bnxt_en driver.
		 */

		/* if we are unable to allocate a new buffer, abort and reuse */
		if (bnxt_alloc_rx_data(bp, rxr, rxr->rx_prod, GFP_ATOMIC)) {
			trace_xdp_exception(bp->dev, xdp_prog, act);
			bnxt_reuse_rx_data(rxr, cons, xdp);
			cpr->sw_stats->xsk_stats.xsk_rx_alloc_fail++;
			return true;
		}

		if (xdp_do_redirect(bp->dev, xdp, xdp_prog)) {
			trace_xdp_exception(bp->dev, xdp_prog, act);
			cpr->sw_stats->xsk_stats.xsk_rx_redirect_fail++;
			bnxt_reuse_rx_data(rxr, cons, xdp);
			return true;
		}

		*event |= BNXT_REDIRECT_EVENT;
		cpr->sw_stats->xsk_stats.xsk_rx_success++;
		break;
	default:
		bpf_warn_invalid_xdp_action(bp->dev, xdp_prog, act);
		fallthrough;
	case XDP_ABORTED:
		trace_xdp_exception(bp->dev, xdp_prog, act);
		fallthrough;
	case XDP_DROP:
		break;
	}
	return true;
}

bool bnxt_xsk_xmit(struct bnxt *bp, struct bnxt_napi *bnapi, int budget)
{
	struct bnxt_tx_ring_info *txr = bnapi->tx_ring[0];
	struct bnxt_cp_ring_info *cpr;
	int cpu = smp_processor_id();
	struct bnxt_sw_tx_bd *tx_buf;
	struct netdev_queue *txq;
	u16 prod = txr->tx_prod;
	bool xsk_more = true;
	struct tx_bd *txbd;
	dma_addr_t mapping;
	int i, xsk_tx = 0;
	int num_frags = 0;
	u32 len, flags;

	cpr = &bnapi->cp_ring;
	txq = netdev_get_tx_queue(bp->dev, txr->txq_index);

	__netif_tx_lock(txq, cpu);

	for (i = 0; i < budget; i++) {
		struct xdp_desc desc;

		if (bnxt_tx_avail(bp, txr) < 2) {
			cpr->sw_stats->xsk_stats.xsk_tx_ring_full++;
			xsk_more = false;
			break;
		}

		if (!xsk_tx_peek_desc(txr->xsk_pool, &desc)) {
			xsk_more = false;
			break;
		}

		mapping = xsk_buff_raw_get_dma(txr->xsk_pool, desc.addr);
		len = desc.len;

		xsk_buff_raw_dma_sync_for_device(txr->xsk_pool, mapping, desc.len);

		tx_buf = &txr->tx_buf_ring[RING_TX(bp, prod)];
		tx_buf->action = BNXT_XSK_TX;

		txbd = &txr->tx_desc_ring[TX_RING(bp, prod)][TX_IDX(prod)];
		flags = (len << TX_BD_LEN_SHIFT) | TX_BD_CNT(num_frags + 1) |
			bnxt_lhint_arr[len >> 9];
		txbd->tx_bd_len_flags_type = cpu_to_le32(flags);
		txbd->tx_bd_opaque = SET_TX_OPAQUE(bp, txr, prod, 1 + num_frags);
		txbd->tx_bd_haddr = cpu_to_le64(mapping);

		dma_unmap_addr_set(tx_buf, mapping, mapping);
		dma_unmap_len_set(tx_buf, len, len);

		flags &= ~TX_BD_LEN;
		txbd->tx_bd_len_flags_type = cpu_to_le32(((len) << TX_BD_LEN_SHIFT) |
						flags | TX_BD_FLAGS_PACKET_END);
		prod = NEXT_TX(prod);
		txr->tx_prod = prod;
		xsk_tx++;
	}

	if (xsk_tx) {
		/* write the doorbell */
		wmb();
		xsk_tx_release(txr->xsk_pool);
		netdev_dbg(bp->dev, "%s: db_key 0x%llX, txr-prod 0x%x txq_index %d\n",
			   __func__, txr->tx_db.db_key64, prod, txr->txq_index);
		bnxt_db_write(bp, &txr->tx_db, prod);
		cpr->sw_stats->xsk_stats.xsk_tx_sent_pkts += xsk_tx;
	}

	__netif_tx_unlock(txq);
	return xsk_more;
}
#else
bool bnxt_rx_xsk(struct bnxt *bp, struct bnxt_rx_ring_info *rxr, u16 cons,
		 struct xdp_buff *xdp, u8 **data_ptr, unsigned int *len, u8 *event)
{
	return false;
}

int bnxt_xsk_wakeup(struct net_device *dev, u32 queue_id, u32 flags)
{
	return 0;
}

int bnxt_xdp_setup_pool(struct bnxt *bp, struct xsk_buff_pool *pool,
			u16 queue_id)
{
	return 0;
}

bool bnxt_xsk_xmit(struct bnxt *bp, struct bnxt_napi *bnapi, int budget)
{
	return false;
}
#endif
