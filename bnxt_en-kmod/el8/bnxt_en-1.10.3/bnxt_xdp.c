/* Broadcom NetXtreme-C/E network driver.
 *
 * Copyright (c) 2016-2018 Broadcom Limited
 * Copyright (c) 2018-2024 Broadcom Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/if_vlan.h>
#ifdef HAVE_NDO_XDP
#include <linux/bpf.h>
#ifdef HAVE_BPF_TRACE
#include <linux/bpf_trace.h>
#endif
#include <linux/filter.h>
#endif
#ifdef HAVE_NETDEV_LOCK
#include <net/netdev_lock.h>
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

DEFINE_STATIC_KEY_FALSE(bnxt_xdp_locking_key);

struct bnxt_sw_tx_bd *bnxt_xmit_bd(struct bnxt *bp,
				   struct bnxt_tx_ring_info *txr,
				   dma_addr_t mapping, u32 len,
				   struct xdp_buff *xdp)
{
	struct bnxt_sw_tx_bd *tx_buf;
	struct tx_bd *txbd;
	int num_frags = 0;
	u32 flags;
	u16 prod;
	struct skb_shared_info *sinfo;
#ifdef HAVE_XDP_MULTI_BUFF
	int i;
#endif

	if (xdp && xdp_buff_has_frags(xdp)) {
		sinfo = xdp_get_shared_info_from_buff(xdp);
		num_frags = sinfo->nr_frags;
	}

	/* fill up the first buffer */
	prod = txr->tx_prod;
	tx_buf = &txr->tx_buf_ring[RING_TX(bp, prod)];

	tx_buf->nr_frags = num_frags;
	if (xdp)
		tx_buf->page = virt_to_head_page(xdp->data);

	txbd = &txr->tx_desc_ring[TX_RING(bp, prod)][TX_IDX(prod)];
	flags = (len << TX_BD_LEN_SHIFT) | TX_BD_CNT(num_frags + 1) |
		bnxt_lhint_arr[len >> 9];
	txbd->tx_bd_len_flags_type = cpu_to_le32(flags);
	txbd->tx_bd_haddr = cpu_to_le64(mapping);
	txbd->tx_bd_opaque = SET_TX_OPAQUE(bp, txr, prod, 1 + num_frags);
#ifdef HAVE_XDP_MULTI_BUFF
	/* now let us fill up the frags into the next buffers */
	for (i = 0; i < num_frags ; i++) {
		skb_frag_t *frag = &sinfo->frags[i];
		struct bnxt_sw_tx_bd *frag_tx_buf;
		dma_addr_t frag_mapping;
		int frag_len;

		prod = NEXT_TX(prod);
		WRITE_ONCE(txr->tx_prod, prod);

		/* first fill up the first buffer */
		frag_tx_buf = &txr->tx_buf_ring[RING_TX(bp, prod)];
		frag_tx_buf->page = skb_frag_page(frag);

		txbd = &txr->tx_desc_ring[TX_RING(bp, prod)][TX_IDX(prod)];

		frag_len = skb_frag_size(frag);
		flags = frag_len << TX_BD_LEN_SHIFT;
		txbd->tx_bd_len_flags_type = cpu_to_le32(flags);
		frag_mapping = page_pool_get_dma_addr(skb_frag_page(frag)) +
			       skb_frag_off(frag);
		txbd->tx_bd_haddr = cpu_to_le64(frag_mapping);

		len = frag_len;
	}

#endif
	flags &= ~TX_BD_LEN;
	txbd->tx_bd_len_flags_type = cpu_to_le32(((len) << TX_BD_LEN_SHIFT) | flags |
			TX_BD_FLAGS_PACKET_END);
	prod = NEXT_TX(prod);
	WRITE_ONCE(txr->tx_prod, prod);

	/* Sync TX BD */
	wmb();
	return tx_buf;
}

#ifdef HAVE_NDO_XDP
bool bnxt_xdp_attached(struct bnxt *bp, struct bnxt_rx_ring_info *rxr)
{
	struct bpf_prog *xdp_prog = READ_ONCE(rxr->xdp_prog);

	return !!xdp_prog;
}

void bnxt_xdp_buff_init(struct bnxt *bp, struct bnxt_rx_ring_info *rxr,
			u16 cons, u8 *data_ptr, unsigned int len,
			struct xdp_buff *xdp)
{
	struct bnxt_sw_rx_bd *rx_buf;
	u32 buflen = BNXT_RX_PAGE_SIZE;
	struct pci_dev *pdev;
	dma_addr_t mapping;
	u32 offset;

	pdev = bp->pdev;
	rx_buf = &rxr->rx_buf_ring[cons];
	offset = bp->rx_offset;

	mapping = rx_buf->mapping - bp->rx_dma_offset;
	dma_sync_single_for_cpu(&pdev->dev, mapping + offset, len, bp->rx_dir);

	xdp_init_buff(xdp, buflen, &rxr->xdp_rxq);
	xdp_prepare_buff(xdp, data_ptr - offset, offset, len, true);
}

void __bnxt_xmit_xdp(struct bnxt *bp, struct bnxt_tx_ring_info *txr,
		     dma_addr_t mapping, u32 len, u16 rx_prod,
		     struct xdp_buff *xdp)
{
	struct bnxt_sw_tx_bd *tx_buf;

	tx_buf = bnxt_xmit_bd(bp, txr, mapping, len, xdp);
	tx_buf->rx_prod = rx_prod;
	tx_buf->action = XDP_TX;
	txr->xdp_tx_pending++;
}

#ifdef HAVE_XDP_FRAME
static void __bnxt_xmit_xdp_redirect(struct bnxt *bp,
				     struct bnxt_tx_ring_info *txr,
				     dma_addr_t mapping, u32 len,
				     struct xdp_frame *xdpf)
{
	struct bnxt_sw_tx_bd *tx_buf;

	tx_buf = bnxt_xmit_bd(bp, txr, mapping, len, NULL);
	tx_buf->action = XDP_REDIRECT;
	tx_buf->xdpf = xdpf;
	dma_unmap_addr_set(tx_buf, mapping, mapping);
	dma_unmap_len_set(tx_buf, len, len);
}
#endif

void bnxt_tx_int_xdp(struct bnxt *bp, struct bnxt_napi *bnapi, int budget)
{
	struct bnxt_tx_ring_info *txr = bnapi->tx_ring[0];
#ifdef HAVE_XSK_SUPPORT
	struct bnxt_cp_ring_info *cpr = &bnapi->cp_ring;
#endif
	struct bnxt_rx_ring_info *rxr = bnapi->rx_ring;
	bool rx_doorbell_needed = false;
	u16 tx_hw_cons = txr->tx_hw_cons;
	struct bnxt_sw_tx_bd *tx_buf;
	u16 tx_cons = txr->tx_cons;
	u16 last_tx_cons = tx_cons;
	int i, frags, xsk_tx = 0;

	if (!budget)
		return;

	while (RING_TX(bp, tx_cons) != tx_hw_cons) {
		tx_buf = &txr->tx_buf_ring[RING_TX(bp, tx_cons)];

		if (tx_buf->action == XDP_REDIRECT) {
			struct pci_dev *pdev = bp->pdev;

			dma_unmap_single(&pdev->dev,
					dma_unmap_addr(tx_buf, mapping),
					dma_unmap_len(tx_buf, len),
					DMA_TO_DEVICE);
#ifdef HAVE_XDP_FRAME
			xdp_return_frame(tx_buf->xdpf);
#endif
			tx_buf->action = 0;
			tx_buf->xdpf = NULL;
		} else if (tx_buf->action == XDP_TX) {
			tx_buf->action = 0;
			rx_doorbell_needed = true;
			last_tx_cons = tx_cons;

			frags = tx_buf->nr_frags;
			for (i = 0; i < frags; i++) {
				tx_cons = NEXT_TX(tx_cons);
				tx_buf = &txr->tx_buf_ring[RING_TX(bp, tx_cons)];
#ifdef CONFIG_PAGE_POOL
				page_pool_recycle_direct(rxr->page_pool, tx_buf->page);
#else
				__free_page(tx_buf->page);
#endif
			}
			txr->xdp_tx_pending--;
		} else if (tx_buf->action == BNXT_XSK_TX) {
			rx_doorbell_needed = false;
			xsk_tx++;
		} else {
			bnxt_sched_reset_txr(bp, txr, tx_cons);
			return;
		}
		tx_cons = NEXT_TX(tx_cons);
	}
	bnapi->events &= ~BNXT_TX_CMP_EVENT;
	WRITE_ONCE(txr->tx_cons, tx_cons);

#ifdef HAVE_XSK_SUPPORT
	if (txr->xsk_pool && xsk_tx) {
		xsk_tx_completed(txr->xsk_pool, xsk_tx);
		cpr->sw_stats->xsk_stats.xsk_tx_completed += xsk_tx;
	}
	if (txr->xsk_pool && xsk_uses_need_wakeup(txr->xsk_pool))
		xsk_set_tx_need_wakeup(txr->xsk_pool);
#endif
	if (rx_doorbell_needed) {
		if (!txr->xdp_tx_pending) {
			netdev_dbg(bp->dev, "%s: XDP db_key 0x%llX, rx_prod 0x%x\n",
				   __func__, rxr->rx_db.db_key64, rxr->rx_prod);
			bnxt_db_write(bp, &rxr->rx_db, rxr->rx_prod);
		} else {
			tx_buf = &txr->tx_buf_ring[RING_TX(bp, last_tx_cons)];
			netdev_dbg(bp->dev, "%s: XDP tx_buf db_key 0x%llX, rx_prod 0x%x\n",
				   __func__, rxr->rx_db.db_key64, tx_buf->rx_prod);
			bnxt_db_write(bp, &rxr->rx_db, tx_buf->rx_prod);
		}
	}
}

void bnxt_xdp_buff_frags_free(struct bnxt_rx_ring_info *rxr,
			      struct xdp_buff *xdp)
{
	struct skb_shared_info *shinfo;
	int i;

	if (!xdp || !xdp_buff_has_frags(xdp))
		return;
	shinfo = xdp_get_shared_info_from_buff(xdp);
	if (!shinfo)
		return;

	for (i = 0; i < shinfo->nr_frags; i++) {
		struct page *page = skb_frag_page(&shinfo->frags[i]);

#ifdef CONFIG_PAGE_POOL
		page_pool_recycle_direct(rxr->page_pool, page);
#else
		__free_page(page);
#endif
	}
	shinfo->nr_frags = 0;
}

/* returns the following:
 * true    - packet consumed by XDP and new buffer is allocated.
 * false   - packet should be passed to the stack.
 */
bool bnxt_rx_xdp(struct bnxt *bp, struct bnxt_rx_ring_info *rxr, u16 cons,
		 struct xdp_buff *xdp, struct page *page, u8 **data_ptr,
		 unsigned int *len, u8 *event)
{
	struct bpf_prog *xdp_prog = READ_ONCE(rxr->xdp_prog);
	struct bnxt_tx_ring_info *txr;
	struct bnxt_sw_rx_bd *rx_buf;
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
	/* BNXT_RX_PAGE_MODE(bp) when XDP enabled */
	orig_data = xdp->data;

	act = bpf_prog_run_xdp(xdp_prog, xdp);

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

	switch (act) {
	case XDP_PASS:
		return false;

	case XDP_TX:
		rx_buf = &rxr->rx_buf_ring[cons];
		mapping = rx_buf->mapping - bp->rx_dma_offset;

		if (unlikely(xdp_buff_has_frags(xdp))) {
			struct skb_shared_info *sinfo = xdp_get_shared_info_from_buff(xdp);

			tx_needed += sinfo->nr_frags;
		}

		if (tx_avail < tx_needed) {
			trace_xdp_exception(bp->dev, xdp_prog, act);
			bnxt_xdp_buff_frags_free(rxr, xdp);
			bnxt_reuse_rx_data(rxr, cons, page);
			return true;
		}

		dma_sync_single_for_device(&pdev->dev, mapping + offset, *len,
					   bp->rx_dir);

		*event &= ~BNXT_RX_EVENT;
		*event |= BNXT_TX_EVENT;
		__bnxt_xmit_xdp(bp, txr, mapping + offset, *len,
				NEXT_RX(rxr->rx_prod), xdp);
		bnxt_reuse_rx_data(rxr, cons, page);
		return true;
	case XDP_REDIRECT:
		/* if we are calling this here then we know that the
		 * redirect is coming from a frame received by the
		 * bnxt_en driver.
		 */
#ifndef HAVE_PAGE_POOL_GET_DMA_ADDR
		rx_buf = &rxr->rx_buf_ring[cons];
		mapping = rx_buf->mapping - bp->rx_dma_offset;
		dma_unmap_page_attrs(&pdev->dev, mapping,
				     BNXT_RX_PAGE_SIZE, bp->rx_dir,
				     DMA_ATTR_WEAK_ORDERING);
#endif
		/* if we are unable to allocate a new buffer, abort and reuse */
		if (bnxt_alloc_rx_data(bp, rxr, rxr->rx_prod, GFP_ATOMIC)) {
			trace_xdp_exception(bp->dev, xdp_prog, act);
			bnxt_xdp_buff_frags_free(rxr, xdp);
			bnxt_reuse_rx_data(rxr, cons, page);
			return true;
		}

		if (xdp_do_redirect(bp->dev, xdp, xdp_prog)) {
			trace_xdp_exception(bp->dev, xdp_prog, act);
#ifdef CONFIG_PAGE_POOL
			page_pool_recycle_direct(rxr->page_pool, page);
#else
			__free_page(page);
#endif
			return true;
		}

		*event |= BNXT_REDIRECT_EVENT;
		break;
	default:
		bpf_warn_invalid_xdp_action(bp->dev, xdp_prog, act);
		fallthrough;
	case XDP_ABORTED:
		trace_xdp_exception(bp->dev, xdp_prog, act);
		fallthrough;
	case XDP_DROP:
		bnxt_xdp_buff_frags_free(rxr, xdp);
		bnxt_reuse_rx_data(rxr, cons, page);
		break;
	}
	return true;
}

#ifdef HAVE_XDP_FRAME
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,13,0)
int bnxt_xdp_xmit(struct net_device *dev, int num_frames,
		  struct xdp_frame **frames, u32 flags)
{
	struct bnxt *bp = netdev_priv(dev);
	struct bpf_prog *xdp_prog = READ_ONCE(bp->xdp_prog);
	struct pci_dev *pdev = bp->pdev;
	struct bnxt_tx_ring_info *txr;
	dma_addr_t mapping;
	int nxmit = 0;
	int ring;
	int i;

	if (!test_bit(BNXT_STATE_OPEN, &bp->state) ||
	    !bp->tx_nr_rings_xdp ||
	    !xdp_prog)
		return -EINVAL;

	ring = smp_processor_id() % bp->tx_nr_rings_xdp;
	txr = &bp->tx_ring[ring];

	if (READ_ONCE(txr->dev_state) == BNXT_DEV_STATE_CLOSING)
		return -EINVAL;

	if (static_branch_unlikely(&bnxt_xdp_locking_key))
		spin_lock(&txr->tx_lock);

	for (i = 0; i < num_frames; i++) {
		struct xdp_frame *xdp = frames[i];

		if (!bnxt_tx_avail(bp, txr))
			break;

		mapping = dma_map_single(&pdev->dev, xdp->data, xdp->len,
					 DMA_TO_DEVICE);

		if (dma_mapping_error(&pdev->dev, mapping))
			break;

		__bnxt_xmit_xdp_redirect(bp, txr, mapping, xdp->len, xdp);
		nxmit++;
	}

	if (flags & XDP_XMIT_FLUSH) {
		/* Sync BD data before updating doorbell */
		wmb();
		netdev_dbg(bp->dev, "%s: XDP_XMIT_FLUSH db_key 0x%llX, tx_prod 0x%x\n",
			   __func__, txr->tx_db.db_key64, txr->tx_prod);
		bnxt_db_write(bp, &txr->tx_db, txr->tx_prod);
	}

	if (static_branch_unlikely(&bnxt_xdp_locking_key))
		spin_unlock(&txr->tx_lock);

	return nxmit;
}
#else
int bnxt_xdp_xmit(struct net_device *dev, int num_frames,
		  struct xdp_frame **frames, u32 flags)
{
	struct bnxt *bp = netdev_priv(dev);
	struct bpf_prog *xdp_prog = READ_ONCE(bp->xdp_prog);
	struct pci_dev *pdev = bp->pdev;
	struct bnxt_tx_ring_info *txr;
	dma_addr_t mapping;
	int drops = 0;
	int ring;
	int i;

	if (!test_bit(BNXT_STATE_OPEN, &bp->state) ||
	    !bp->tx_nr_rings_xdp ||
	    !xdp_prog)
		return -EINVAL;

	ring = smp_processor_id() % bp->tx_nr_rings_xdp;
	txr = &bp->tx_ring[ring];

	if (READ_ONCE(txr->dev_state) == BNXT_DEV_STATE_CLOSING)
		return -EINVAL;

	if (static_branch_unlikely(&bnxt_xdp_locking_key))
		spin_lock(&txr->tx_lock);

	for (i = 0; i < num_frames; i++) {
		struct xdp_frame *xdp = frames[i];

		if (!bnxt_tx_avail(bp, txr)) {
			xdp_return_frame_rx_napi(xdp);
			drops++;
			continue;
		}

		mapping = dma_map_single(&pdev->dev, xdp->data, xdp->len,
					 DMA_TO_DEVICE);

		if (dma_mapping_error(&pdev->dev, mapping)) {
			xdp_return_frame_rx_napi(xdp);
			drops++;
			continue;
		}
		__bnxt_xmit_xdp_redirect(bp, txr, mapping, xdp->len, xdp);
	}

	if (flags & XDP_XMIT_FLUSH) {
		/* Sync BD data before updating doorbell */
		wmb();
		netdev_dbg(bp->dev, "%s: XDPXMIT_FLUSH db_key 0x%llX, tx_prod 0x%x\n",
			   __func__, txr->tx_db.db_key64, txr->tx_prod);
		bnxt_db_write(bp, &txr->tx_db, txr->tx_prod);
	}

	if (static_branch_unlikely(&bnxt_xdp_locking_key))
		spin_unlock(&txr->tx_lock);

	return num_frames - drops;
}
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(5,13,0) */
#endif

static int bnxt_xdp_set(struct bnxt *bp, struct bpf_prog *prog)
{
	struct net_device *dev = bp->dev;
	int tx_xdp = 0, tx_cp, rc, tc;
	struct bpf_prog *old;

	netdev_assert_locked(dev);

#ifndef HAVE_XDP_MULTI_BUFF
	if (prog && bp->dev->mtu > BNXT_MAX_PAGE_MODE_MTU(bp)) {
		netdev_warn(dev, "MTU %d larger than largest XDP supported MTU %d.\n",
#else
	if (prog && !prog->aux->xdp_has_frags &&
	    bp->dev->mtu > BNXT_MAX_PAGE_MODE_MTU(bp)) {
		netdev_warn(dev, "MTU %d larger than %d without XDP frag support.\n",
#endif
			    bp->dev->mtu, BNXT_MAX_PAGE_MODE_MTU(bp));
		return -EOPNOTSUPP;
	}
	if (prog && bp->flags & BNXT_FLAG_HDS) {
		netdev_warn(dev, "XDP is disallowed when HDS is enabled.\n");
		return -EOPNOTSUPP;
	}
	if (!(bp->flags & BNXT_FLAG_SHARED_RINGS)) {
		netdev_warn(dev, "ethtool rx/tx channels must be combined to support XDP.\n");
		return -EOPNOTSUPP;
	}
	if (prog)
		tx_xdp = bp->rx_nr_rings;

	tc = bp->num_tc;
	if (!tc)
		tc = 1;
	rc = bnxt_check_rings(bp, bp->tx_nr_rings_per_tc, bp->rx_nr_rings,
			      true, tc, tx_xdp);
	if (rc) {
		netdev_warn(dev, "Unable to reserve enough TX rings to support XDP.\n");
		return rc;
	}
	if (netif_running(dev))
		bnxt_close_nic(bp, true, false);

	old = xchg(&bp->xdp_prog, prog);
	if (old)
		bpf_prog_put(old);

	if (prog) {
		bnxt_set_rx_skb_mode(bp, true);
		xdp_features_set_redirect_target_locked(dev, true);
	} else {
		xdp_features_clear_redirect_target_locked(dev);
		bnxt_set_rx_skb_mode(bp, false);
	}
	bp->tx_nr_rings_xdp = tx_xdp;
	bp->tx_nr_rings = bp->tx_nr_rings_per_tc * tc + tx_xdp;
	tx_cp = bnxt_num_tx_to_cp(bp, bp->tx_nr_rings);
	bp->cp_nr_rings = max_t(int, tx_cp, bp->rx_nr_rings);
	bnxt_set_tpa_flags(bp);
	bnxt_set_ring_params(bp, true);

	if (netif_running(dev))
		return bnxt_open_nic(bp, true, false);

	return 0;
}

int bnxt_xdp(struct net_device *dev, struct netdev_bpf *xdp)
{
	struct bnxt *bp = netdev_priv(dev);
	int rc;

	switch (xdp->command) {
	case XDP_SETUP_PROG:
		rc = bnxt_xdp_set(bp, xdp->prog);
		break;
#ifdef HAVE_XDP_QUERY_PROG
	case XDP_QUERY_PROG:
#ifdef HAVE_PROG_ATTACHED
		xdp->prog_attached = !!bp->xdp_prog;
#endif
#ifdef HAVE_IFLA_XDP_PROG_ID
		xdp->prog_id = bp->xdp_prog ? bp->xdp_prog->aux->id : 0;
#endif
		rc = 0;
		break;
#endif /* HAVE_XDP_QUERY_PROG */
#ifdef HAVE_XSK_SUPPORT
	case XDP_SETUP_XSK_POOL:
		netdev_info(bp->dev, "%s(): XDP_SETUP_XSK_POOL on queue_id: %d\n",
			    __func__, xdp->xsk.queue_id);
		return bnxt_xdp_setup_pool(bp, xdp->xsk.pool, xdp->xsk.queue_id);
#endif
	default:
		rc = -EINVAL;
		break;
	}
	return rc;
}

#ifdef HAVE_XDP_MULTI_BUFF
struct sk_buff *
bnxt_xdp_build_skb(struct bnxt *bp, struct sk_buff *skb, u8 num_frags,
		   struct page_pool *pool, struct xdp_buff *xdp,
		   struct rx_cmp_ext *rxcmp1)
{
	struct skb_shared_info *sinfo = xdp_get_shared_info_from_buff(xdp);

	if (!skb || !sinfo)
		return NULL;
	skb_checksum_none_assert(skb);
	if (RX_CMP_L4_CS_OK(rxcmp1)) {
		if (bp->dev->features & NETIF_F_RXCSUM) {
			skb->ip_summed = CHECKSUM_UNNECESSARY;
			skb->csum_level = RX_CMP_ENCAP(rxcmp1);
		}
	}
	xdp_update_skb_shared_info(skb, num_frags,
				   sinfo->xdp_frags_size,
				   BNXT_RX_PAGE_SIZE * sinfo->nr_frags,
				   xdp_buff_is_frag_pfmemalloc(xdp));
	return skb;
}
#endif
#else
void bnxt_tx_int_xdp(struct bnxt *bp, struct bnxt_napi *bnapi, int budget)
{
}

bool bnxt_rx_xdp(struct bnxt *bp, struct bnxt_rx_ring_info *rxr, u16 cons,
		 struct xdp_buff *xdp, void *page, u8 **data_ptr,
		 unsigned int *len, u8 *event)
{
	return false;
}

bool bnxt_xdp_attached(struct bnxt *bp, struct bnxt_rx_ring_info *rxr)
{
	return false;
}

void bnxt_xdp_buff_init(struct bnxt *bp, struct bnxt_rx_ring_info *rxr,
			u16 cons, u8 *data_ptr, unsigned int len,
			struct xdp_buff *xdp)
{
}
#endif
