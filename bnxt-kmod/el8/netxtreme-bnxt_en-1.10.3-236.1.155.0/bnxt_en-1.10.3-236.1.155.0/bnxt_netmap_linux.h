/*
 * netmap support for Broadcom bnxt Ethernet driver on Linux
 *
 * Copyright (C) 2015-2024 British Broadcasting Corporation. All rights reserved.
 *
 * Author: Stuart Grace, BBC Research & Development
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 *   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *   ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 *   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 *   OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 *   OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 *   SUCH DAMAGE.
 *
 * Some portions are:
 *
 *   Copyright (C) 2012-2014 Matteo Landi, Luigi Rizzo. All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 *   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *   ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 *   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 *   OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 *   OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 *   SUCH DAMAGE.
 *
 * Some portions are:
 *
 *	Copyright (c) 2018-2023 Broadcom Inc.
 *
 *       Redistribution and use in source and binary forms, with or
 *       without modification, are permitted provided that the following
 *       conditions are met:
 *
 *        - Redistributions of source code must retain the above
 *          copyright notice, this list of conditions and the following
 *          disclaimer.
 *
 *        - Redistributions in binary form must reproduce the above
 *          copyright notice, this list of conditions and the following
 *          disclaimer in the documentation and/or other materials
 *          provided with the distribution.
 *
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 *   BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 *   ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 *   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *   SOFTWARE.
 */

#ifndef __BNXT_NETMAP_LINUX_H__
#define __BNXT_NETMAP_LINUX_H__

#include <bsd_glue.h>
#include <net/netmap.h>
#include <dev/netmap/netmap_kern.h>

#ifdef NETMAP_BNXT_MAIN

#define NM_BNXT_ADAPTER bnxt

/* No: of shadow AGG rings; for now stick to 1 ==> same size as normal ring */
#define AGG_NM_RINGS	1

/*
 * Register/unregister. We are already under netmap lock.
 * Only called on the first register or the last unregister.
 */
int bnxt_netmap_reg(struct netmap_adapter *na, int onoff)
{
	struct ifnet *ifp = na->ifp;
	struct NM_BNXT_ADAPTER *bp = netdev_priv(ifp);
	int err = 0;

	nm_prinf("bnxt switching %s native netmap mode", onoff ? "into" : "out of");

	if (netif_running(ifp))
		bnxt_close_nic(bp, true, false);
	/* enable or disable flags and callbacks in na and ifp */
	if (onoff) {
		nm_set_native_flags(na);
		if (!(bp->flags & BNXT_FLAG_JUMBO)) {
			bp->flags &= ~BNXT_FLAG_AGG_RINGS;
			bp->flags |= BNXT_FLAG_NO_AGG_RINGS;
			if (bp->flags & BNXT_FLAG_LRO) {
				bp->dev->hw_features &= ~NETIF_F_LRO;
				bp->dev->features &= ~NETIF_F_LRO;
				netdev_update_features(bp->dev);
			}
		}
		bp->flags |= BNXT_FLAG_DIM;
	} else {
		bp->flags |= BNXT_FLAG_AGG_RINGS;
		bp->flags &= ~BNXT_FLAG_NO_AGG_RINGS;
		if (bp->flags & BNXT_FLAG_LRO) {
			bp->dev->hw_features |= NETIF_F_LRO;
			bp->dev->features |= NETIF_F_LRO;
			netdev_update_features(bp->dev);
		}
		bp->flags &= ~(BNXT_FLAG_DIM);
		nm_clear_native_flags(na);
	}

	if (netif_running(ifp))
		return bnxt_open_nic(bp, true, false);
	return err;
}

void bnxt_netmap_txflush(struct bnxt_tx_ring_info *txr)
{
	struct bnxt *bp = txr->bnapi->bp;
	struct bnxt_cp_ring_info *cpr2;
	struct bnxt_db_info *db;
	u32 raw_cons, tgl = 0;
	struct tx_cmp *txcmp;
	u16 cons;

	cpr2 = txr->tx_cpr;
	raw_cons = cpr2->cp_raw_cons;

	while (1) {
		u8 cmp_type;

		cons = RING_CMP(raw_cons);
		txcmp = &cpr2->cp_desc_ring[CP_RING(cons)][CP_IDX(cons)];

		if (!TX_CMP_VALID(txcmp, raw_cons))
			break;

		/* The valid test of the entry must be done first before
		 * reading any further.
		 */
		dma_rmb();
		cmp_type = TX_CMP_TYPE(txcmp);
		if (cmp_type == CMP_TYPE_TX_L2_CMP ||
		    cmp_type == CMP_TYPE_TX_L2_COAL_CMP) {
			u32 opaque = txcmp->tx_cmp_opaque;

			if (cmp_type == CMP_TYPE_TX_L2_COAL_CMP)
				txr->tx_hw_cons = TX_CMP_SQ_CONS_IDX(txcmp);
			else
				txr->tx_hw_cons = TX_OPAQUE_IDX(opaque);
			raw_cons = NEXT_RAW_CMP(raw_cons);
		}
	}

	if (raw_cons != cpr2->cp_raw_cons) {
		tgl = cpr2->toggle;
		db = &cpr2->cp_db;
		cpr2->cp_raw_cons = raw_cons;
		/* barrier - before arming the cq */
		wmb();
		bnxt_writeq(bp, db->db_key64 | DBR_TYPE_CQ_ARMALL | DB_TOGGLE(tgl) |
			    DB_RING_IDX(db, cpr2->cp_raw_cons), db->doorbell);
	}
}

/*
 * Reconcile kernel and user view of the transmit ring.
 *
 * Userspace wants to send packets up to the one before ring->head,
 * kernel knows kring->nr_hwcur is the first unsent packet.
 *
 * Here we push packets out (as many as possible), and possibly
 * reclaim buffers from previously completed transmission.
 *
 * ring->tail is updated on return.
 * ring->head is never used here.
 *
 * The caller (netmap) guarantees that there is only one instance
 * running at any time. Any interference with other driver
 * methods should be handled by the individual drivers.
 */
int bnxt_netmap_txsync(struct netmap_kring *kring, int flags)
{
	u_int const lim = kring->nkr_num_slots - 1;
	struct netmap_ring *ring = kring->ring;
	struct netmap_adapter *na = kring->na;
	struct bnxt_cp_ring_info *cpr2;
	u_int const head = kring->rhead;
	struct ifnet *ifp = na->ifp;
	u_int nm_i;	/* index into the netmap ring */
	u_int n;
	/*
	 * interrupts on every tx packet are expensive so request
	 * them every half ring, or where NS_REPORT is set
	 */
	u_int tosync;
	/* device-specific */
	struct NM_BNXT_ADAPTER *bp = netdev_priv(ifp);
	u16 prod = 0, cons, hw_cons, nr_frags = 0;
	struct bnxt_tx_ring_info *txr;
	struct bnxt_sw_tx_bd *tx_buf;
	struct tx_bd *txbd, *txbd0;
	u32 raw_cons, tgl = 0;
	struct tx_cmp *txcmp;
	struct bnxt_db_info *db;
	u16 prod0;

	if (!netif_carrier_ok(ifp) || !netif_running(ifp))
		return 0;

	txr = &bp->tx_ring[bp->tx_ring_map[kring->ring_id]];
	if (unlikely(!txr)) {
		nm_prlim(1, "ring %s is missing (txr=%p)", kring->name, txr);
		return -ENXIO;
	}

	/*
	 * First part: process new packets to send.
	 * nm_i is the current index in the netmap ring,
	 *
	 * If we have packets to send (kring->nr_hwcur != kring->rhead)
	 * iterate over the netmap ring, fetch length and update
	 * the corresponding slot in the NIC ring. Some drivers also
	 * need to update the buffer's physical address in the NIC slot
	 * even NS_BUF_CHANGED is not set (PNMB computes the addresses).
	 *
	 * The netmap_reload_map() calls is especially expensive,
	 * even when (as in this case) the tag is 0, so do only
	 * when the buffer has actually changed.
	 *
	 * If possible do not set the report/intr bit on all slots,
	 * but only a few times per ring or when NS_REPORT is set.
	 *
	 * Finally, on 10G and faster drivers, it might be useful
	 * to prefetch the next slot and txr entry.
	 */

	nm_i = kring->nr_hwcur;
	if (nm_i != head) {	/* we have new packets to send */
		nm_prdis("new pkts to send nm_i: %d head: %d\n", nm_i, head);
		__builtin_prefetch(&ring->slot[nm_i]);

		for (n = 0; nm_i != head; n++) {
			struct netmap_slot *slot = &ring->slot[nm_i];
			u_int len = slot->len, bd0_len;
			uint64_t paddr;
			uint64_t offset = nm_get_offset(kring, slot);

			/* device-specific */
			if (bnxt_tx_avail(bp, txr) < 1) {
				nm_prinf("NO TX AVAIL!\n");
				break;
			}
			prod = txr->tx_prod; /* producer index */
			txbd = &txr->tx_desc_ring[TX_RING(bp, prod)][TX_IDX(prod)];

			tx_buf = &txr->tx_buf_ring[RING_TX(bp, prod)];
			/* prefetch for next round */
			__builtin_prefetch(&ring->slot[nm_i + 1]);
			__builtin_prefetch(&txr->tx_desc_ring[TX_RING(bp, prod + 1)][TX_IDX(prod + 1)]);

			PNMB(na, slot, &paddr);
			NM_CHECK_ADDR_LEN_OFF(na, len, offset);

			/* Fill the slot in the NIC ring. */
			txbd->tx_bd_haddr = cpu_to_le64(paddr + offset);
			netmap_sync_map_dev(na, (bus_dma_tag_t)na->pdev, &paddr, len, NR_TX);

			flags = (len << TX_BD_LEN_SHIFT) |
				TX_BD_CNT(nr_frags + 1) |
				bnxt_lhint_arr[len >> 9];
			txbd->tx_bd_len_flags_type = cpu_to_le32(flags);
			txbd0 = txbd;
			prod0 = prod;
			bd0_len = len;
			if (slot->flags & NS_MOREFRAG) {
				nr_frags++;
				for (;;) {
					nm_i = nm_next(nm_i, lim);
					/* remember that we have to ask for a
					 * report each time we move past half a
					 * ring
					 */
					if (nm_i == head) {
						/* XXX should we accept incomplete packets? */
						return -EINVAL;
					}
					slot = &ring->slot[nm_i];
					len = slot->len;
					PNMB(na, slot, &paddr);
					offset = nm_get_offset(kring, slot);
					NM_CHECK_ADDR_LEN_OFF(na, len, offset);
					prod = NEXT_TX(prod);
					txbd = &txr->tx_desc_ring[TX_RING(bp, prod)][TX_IDX(prod)];
					txbd->tx_bd_haddr = cpu_to_le64(paddr + offset);
					flags = len << TX_BD_LEN_SHIFT;
					txbd->tx_bd_len_flags_type = cpu_to_le32(flags);
					netmap_sync_map_dev(na, (bus_dma_tag_t)na->pdev,
							    &paddr, len, NR_TX);
					if (!(slot->flags & NS_MOREFRAG))
						break;
					nr_frags++;
				}
				tx_buf->nr_frags = nr_frags;
				nr_frags = 0;

				flags = (bd0_len << TX_BD_LEN_SHIFT) |
					TX_BD_CNT(tx_buf->nr_frags + 1) |
					bnxt_lhint_arr[bd0_len >> 9];
				txbd0->tx_bd_len_flags_type = cpu_to_le32(flags);
			}
			slot->flags &= ~(NS_REPORT | NS_BUF_CHANGED | NS_MOREFRAG);

			flags &= ~TX_BD_LEN;
			txbd->tx_bd_len_flags_type = cpu_to_le32(((len) << TX_BD_LEN_SHIFT) |
							flags | TX_BD_FLAGS_PACKET_END);
			prod = NEXT_TX(prod);
			txbd0->tx_bd_opaque = SET_TX_OPAQUE(bp, txr, prod0,
							    tx_buf->nr_frags);
			txr->tx_prod = prod;
			nm_i = nm_next(nm_i, lim);
		}
		kring->nr_hwcur = head;

		/* synchronize the NIC ring */
		nm_prdis("calling bnxt_txr_db_kick with prod:%d cons: %d nr_hwtail: %d\n",
			 prod, txr->tx_cons, kring->nr_hwtail);
		bnxt_txr_db_kick(bp, txr, prod);
	}
	/*
	 * Second part: reclaim buffers for completed transmissions.
	 */
	cpr2 = txr->tx_cpr;
	raw_cons = cpr2->cp_raw_cons;

	while (1) {
		u8 cmp_type;

		cons = RING_CMP(raw_cons);
		txcmp = &cpr2->cp_desc_ring[CP_RING(cons)][CP_IDX(cons)];

		if (!TX_CMP_VALID(txcmp, raw_cons))
			break;

		/* The valid test of the entry must be done first before
		 * reading any further.
		 */
		dma_rmb();
		cmp_type = TX_CMP_TYPE(txcmp);
		if (cmp_type == CMP_TYPE_TX_L2_CMP ||
		    cmp_type == CMP_TYPE_TX_L2_COAL_CMP) {
			u32 opaque = txcmp->tx_cmp_opaque;

			if (cmp_type == CMP_TYPE_TX_L2_COAL_CMP)
				txr->tx_hw_cons = TX_CMP_SQ_CONS_IDX(txcmp);
			else
				txr->tx_hw_cons = TX_OPAQUE_IDX(opaque);
			raw_cons = NEXT_RAW_CMP(raw_cons);
		}
	}

	if (raw_cons != cpr2->cp_raw_cons) {
		tgl = cpr2->toggle;
		db = &cpr2->cp_db;
		cpr2->cp_raw_cons = raw_cons;
		/* barrier - before arming the cq */
		wmb();
		bnxt_writeq(bp, db->db_key64 | DBR_TYPE_CQ_ARMALL | DB_TOGGLE(tgl) |
				DB_RING_IDX(db, cpr2->cp_raw_cons),
				db->doorbell);
	}

	tosync = nm_next(kring->nr_hwtail, lim);
	hw_cons = txr->tx_hw_cons;
	cons = txr->tx_cons;
	n = 0;

	while (RING_TX(bp, cons) != hw_cons) {
		/* some tx completed, increment avail */
		/* sync all buffers that we are returning to userspace */
		struct netmap_slot *slot = &ring->slot[tosync];
		struct bnxt_sw_tx_bd *tx_buf;
		uint64_t paddr;
		int j, last;

		(void)PNMB_O(kring, slot, &paddr);
		tx_buf = &txr->tx_buf_ring[RING_TX(bp, cons)];
		netmap_sync_map_cpu(na, (bus_dma_tag_t)na->pdev, &paddr, slot->len, NR_TX);
		tosync = nm_next(tosync, lim);
		kring->nr_hwtail = nm_prev(tosync, lim);

		last = tx_buf->nr_frags;

		for (j = 0; j < last; j++) {
			slot = &ring->slot[tosync];
			(void)PNMB_O(kring, slot, &paddr);
			cons = NEXT_TX(cons);
			netmap_sync_map_cpu(na, (bus_dma_tag_t)na->pdev, &paddr, slot->len, NR_TX);
			tosync = nm_next(tosync, lim);
			kring->nr_hwtail = nm_prev(tosync, lim);
		}

		cons = NEXT_TX(cons);

		n++;
	}

	if (n) {
		nm_prdis("tx_completed [%d] kring->nr_hwtail: %d\n", n, kring->nr_hwtail);
		txr->tx_cons = cons;
	}

	return 0;
}

int __bnxt_netmap_rxsync(struct netmap_kring *kring, int flags)
{
	u_int const lim = kring->nkr_num_slots - 1;
	struct netmap_adapter *na = kring->na;
	struct netmap_ring *ring = kring->ring;
	u_int const head = kring->rhead;
	u_int stop_i = nm_prev(head, lim); /* stop reclaiming here */
	u_int ring_nr = kring->ring_id;
	struct ifnet *ifp = na->ifp;
	uint16_t slot_flags = 0;
	u_int nm_i = 0; /* index into the netmap ring */

	/* device-specific */
	struct NM_BNXT_ADAPTER *bp = netdev_priv(ifp);
	u32 cp_cons, tmp_raw_cons = 0, real_cons = 0;
	struct bnxt_rx_ring_info *rxr;
	struct bnxt_cp_ring_info *cpr;
	u32 lflags, work_done = 0;
	struct rx_cmp_ext *rxcmp1;
	struct bnxt_db_info *db;
	struct rx_cmp *rxcmp;
	u32 tgl = 0, len;
	uint64_t paddr;

	rxr = &bp->rx_ring[kring->ring_id];
	cpr = rxr->rx_cpr;

	/*
	 * First part: reclaim buffers that userspace has released:
	 * (from kring->nr_hwcur to second last [*] slot before ring->head)
	 * and make the buffers available for reception.
	 * As usual nm_i is the index in the netmap ring.
	 * [*] IMPORTANT: we must leave one free slot in the ring
	 * to avoid ring empty/full confusion in userspace.
	 */

	nm_i = kring->nr_hwcur;
	stop_i = nm_prev(kring->rhead, lim);

	if (nm_i != stop_i) {
		struct netmap_slot *slot;
		u32 prod = rxr->rx_prod;
		struct rx_bd *rxbd;
		uint64_t offset;
		void *addr;

		while (nm_i != stop_i) {
			slot = &ring->slot[nm_i];
			offset = nm_get_offset(kring, slot);
			addr = PNMB(na, slot, &paddr); /* find phys address */

			if (unlikely(addr == NETMAP_BUF_BASE(na))) { /* bad buf */
				nm_prinf("Resetting RX ring %u\n", ring_nr);
				goto ring_reset;
			}

			if (slot->flags & NS_BUF_CHANGED)
				slot->flags &= ~NS_BUF_CHANGED;

			rxbd = &rxr->rx_desc_ring[RX_RING(bp, prod)][RX_IDX(prod)];
			netmap_sync_map_dev(na, (bus_dma_tag_t)na->pdev, &paddr,
					    NETMAP_BUF_SIZE(na), NR_RX);
			rxbd->rx_bd_haddr = cpu_to_le64(paddr + offset);
			prod = NEXT_RX(prod);
			nm_i = nm_next(nm_i, lim);
		}
		rxr->rx_prod = prod;
		netdev_dbg(bp->dev, "%s: db_key 0x%llX, rxr->rx_prod 0x%x\n",
			   __func__, rxr->rx_db.db_key64, rxr->rx_prod);
		bnxt_db_write(bp, &rxr->rx_db, rxr->rx_prod);
		kring->nr_hwcur = nm_i;
	}
	/*
	 * Second part: import newly received packets.
	 * We are told about received packets by CQEs in the CQ.
	 *
	 * nm_i is the index of the next free slot in the netmap ring:
	 */
	rmb();
	real_cons = cpr->cp_raw_cons;
	cp_cons = RING_CMP(real_cons);
	nm_i = kring->nr_hwtail;
	stop_i = nm_prev(kring->nr_hwcur, lim);

	while (nm_i != stop_i) {
		rxcmp = (struct rx_cmp *)&cpr->cp_desc_ring[CP_RING(cp_cons)][CP_IDX(cp_cons)];
		tmp_raw_cons = NEXT_RAW_CMP(real_cons);
		cp_cons = RING_CMP(tmp_raw_cons);

		rxcmp1 = (struct rx_cmp_ext *)
			&cpr->cp_desc_ring[CP_RING(cp_cons)][CP_IDX(cp_cons)];

		if (!RX_CMP_VALID(rxcmp1, tmp_raw_cons))
			break;

		dma_rmb();
		lflags = le32_to_cpu(rxcmp->rx_cmp_len_flags_type);
		len = lflags >> RX_CMP_LEN_SHIFT;
		ring->slot[nm_i].len = len;
		ring->slot[nm_i].flags = slot_flags;
		PNMB_O(kring, &ring->slot[nm_i], &paddr);
		netmap_sync_map_cpu(na, (bus_dma_tag_t)na->pdev,
				    &paddr, len, NR_RX);

		nm_i = nm_next(nm_i, lim);
		tmp_raw_cons = NEXT_RAW_CMP(tmp_raw_cons);
		cp_cons = RING_CMP(tmp_raw_cons);
		real_cons = tmp_raw_cons;
		work_done++;
	}

	if (work_done) {
		kring->nr_hwtail = nm_i;
		cpr->cp_raw_cons = real_cons;
		tgl = cpr->toggle;
		db = &cpr->cp_db;
		/* barrier - TBD revisit? */
		wmb();
		bnxt_writeq(bp, db->db_key64 | DBR_TYPE_CQ_ARMALL | DB_TOGGLE(tgl) |
				DB_RING_IDX(db, cpr->cp_raw_cons),
				db->doorbell);
		kring->nr_kflags &= ~NKR_PENDINTR;
	}
	return 0;
ring_reset:
	return netmap_ring_reinit(kring);
}

#define SLOT_SWAP(s1, s2) do { \
	u32 tmp; \
	tmp = (s1)->buf_idx; \
	(s1)->buf_idx = (s2)->buf_idx; \
	(s2)->buf_idx = tmp; \
	(s1)->flags |= NS_BUF_CHANGED; \
	(s2)->flags |= NS_BUF_CHANGED; \
} while (0)

int bnxt_netmap_rxsync_jumbo(struct netmap_kring *kring, int flags)
{
	u_int const lim = kring->nkr_num_slots - 1;
	struct netmap_adapter *na = kring->na;
	struct netmap_ring *ring = kring->ring;
	struct netmap_kring *base_kring;
	struct netmap_ring *base_nmring;
	struct netmap_kring *agg_kring;
	struct netmap_ring *agg_nmring;
	u_int const head = kring->rhead;
	u_int stop_i = nm_prev(head, lim); /* stop reclaiming here */
	struct ifnet *ifp = na->ifp;
	uint16_t slot_flags = 0;
	uint32_t rx_ring_id = 0;
	u_int nm_i = 0; /* index into the netmap ring */

	/* device-specific */
	struct NM_BNXT_ADAPTER *bp = netdev_priv(ifp);
	u32 cp_cons, tmp_raw_cons = 0, real_cons = 0;
	struct bnxt_rx_ring_info *rxr;
	struct bnxt_cp_ring_info *cpr;
	u32 lflags, work_done = 0;
	struct rx_cmp_ext *rxcmp1;
	struct bnxt_db_info *db;

	/* jumbo specific */
	u32 tgl = 0, len, misc, total_frag_len = 0;
	u16 rx_prod, rx_agg_prod, rx_sw_agg_prod;
	struct rx_cmp *rxcmp;
	struct rx_bd *rxbd;
	uint64_t paddr;
	u8 agg_bufs;
	int i;

	/* 0,3,6,N... are the actual rings that will be used by app/userspace
	 * while [1,2, 4,5, N+1,N+2...] are the shadow rings that map to the base HW
	 * ring and AGG rings respectively
	 */
	if ((kring->ring_id % (2 + AGG_NM_RINGS)) != 0)
		return 0;

	rx_ring_id = kring->ring_id / (2 + AGG_NM_RINGS);
	rxr = &bp->rx_ring[rx_ring_id];
	cpr = rxr->rx_cpr;

	base_kring = na->rx_rings[kring->ring_id + 1];
	base_nmring = base_kring->ring;

	agg_kring = na->rx_rings[kring->ring_id + 2];
	agg_nmring = agg_kring->ring;

	if (unlikely(kring->nr_mode == NKR_NETMAP_OFF) ||
	    base_kring->nr_mode == NKR_NETMAP_OFF || agg_kring->nr_mode == NKR_NETMAP_OFF)
		return 0;

	/*
	 * First part: reclaim buffers that userspace has released:
	 * (from kring->nr_hwcur to second last [*] slot before ring->head)
	 * and make the buffers available for reception.
	 * For ring N+0 nothing to be done for the buffers that userspace has released.
	 * Those are not to be published to the hardware RX ring because the buffer refill
	 * has happened at slot swap time. So a simple kring->nr_hwcur = kring->rhead
	 * should be enough. Also, since tail, head and cur are frozen for rings N+1 and N+2,
	 * rxsync would be a NOP for those.
	 * In the end, all real work happens in the "import newly received packets" part of the
	 * rxsync for ring N+0.
	 */

	kring->nr_hwcur = kring->rhead;

	/*
	 * Second part: import newly received packets.
	 * We are told about received packets by CQEs in the CQ.
	 *
	 * nm_i is the index of the next free slot in the netmap ring:
	 */
	rmb();
	real_cons = cpr->cp_raw_cons;
	cp_cons = RING_CMP(real_cons);
	nm_i = kring->nr_hwtail;
	stop_i = nm_prev(kring->nr_hwcur, lim);

	while (nm_i != stop_i) {
		rx_agg_prod = rxr->rx_agg_prod;
		rx_sw_agg_prod = rxr->rx_sw_agg_prod;

		rx_prod = rxr->rx_prod;

		rxcmp = (struct rx_cmp *)&cpr->cp_desc_ring[CP_RING(cp_cons)][CP_IDX(cp_cons)];
		tmp_raw_cons = NEXT_RAW_CMP(real_cons);
		cp_cons = RING_CMP(tmp_raw_cons);

		rxcmp1 = (struct rx_cmp_ext *)
			&cpr->cp_desc_ring[CP_RING(cp_cons)][CP_IDX(cp_cons)];

		if (!RX_CMP_VALID(rxcmp1, tmp_raw_cons))
			break;

		dma_rmb();

		lflags = le32_to_cpu(rxcmp->rx_cmp_len_flags_type);
		len = lflags >> RX_CMP_LEN_SHIFT;
		misc = le32_to_cpu(rxcmp->rx_cmp_misc_v1);
		agg_bufs = (misc & RX_CMP_AGG_BUFS) >> RX_CMP_AGG_BUFS_SHIFT;

		if (agg_bufs) {
			int space = stop_i - nm_i;

			if (!bnxt_agg_bufs_valid(bp, cpr, agg_bufs, &tmp_raw_cons))
				break;

			if (space < 0)
				space += kring->nkr_num_slots;
			if (space < agg_bufs) {
				nm_prinf(" Not enough space!! space_rem: %d agg_bufs: %d\n",
					 space, agg_bufs);
				break;
			}
			slot_flags |= NS_MOREFRAG;
		}

		BUG_ON(rxcmp->rx_cmp_opaque > lim);
		SLOT_SWAP(&ring->slot[nm_i], &base_nmring->slot[rxcmp->rx_cmp_opaque]);
		/* Now that the SLOT SWAP is done, refill the base HW ring BD
		 * with the new address got from the application ring
		 */
		rxbd = &rxr->rx_desc_ring[RX_RING(bp, rx_prod)][RX_IDX(rx_prod)];
		PNMB_O(base_kring, &base_nmring->slot[rxcmp->rx_cmp_opaque], &paddr);
		rxbd->rx_bd_haddr = cpu_to_le64(paddr);
		rxbd->rx_bd_opaque = RING_RX(bp, rx_prod);

		ring->slot[nm_i].len = len;
		ring->slot[nm_i].flags = slot_flags;
		PNMB_O(kring, &ring->slot[nm_i], &paddr);
		netmap_sync_map_cpu(na, (bus_dma_tag_t)na->pdev,
				    &paddr, len, NR_RX);
		nm_prdis("BEG kring->nr_hwtail: %d slot[%d].len: %d flags: %d agg_bufs: %d rx_cmp_opaque: %d\n",
			 kring->nr_hwtail, nm_i, len, ring->slot[nm_i].flags, agg_bufs, rxcmp->rx_cmp_opaque);
		nm_i = nm_next(nm_i, lim);
		if (agg_bufs) {
			cp_cons = NEXT_CMP(cp_cons);
			for (i = 0; i < agg_bufs; i++) {
				u16 cons, frag_len;
				struct rx_agg_cmp *agg;

				agg = bnxt_get_agg(bp, cpr, cp_cons, i);
				cons = agg->rx_agg_cmp_opaque;
				frag_len = (le32_to_cpu(agg->rx_agg_cmp_len_flags_type) &
						RX_AGG_CMP_LEN) >> RX_AGG_CMP_LEN_SHIFT;
				agg_nmring = agg_kring->ring;
				BUG_ON(cons > lim);
				SLOT_SWAP(&ring->slot[nm_i], &agg_nmring->slot[cons]);
				/* Now that the SLOT SWAP is done, refill the AGG HW ring BD
				 * with the new address got from the application ring
				 */
				rxbd = &rxr->rx_agg_desc_ring[RX_AGG_RING(bp, rx_agg_prod)][RX_IDX(rx_agg_prod)];
				PNMB_O(agg_kring, &agg_nmring->slot[cons], &paddr);
				rxbd->rx_bd_haddr = cpu_to_le64(paddr);
				rxbd->rx_bd_opaque = rx_sw_agg_prod;

				slot_flags = (i < (agg_bufs - 1)) ? NS_MOREFRAG : 0;
				ring->slot[nm_i].len = frag_len;
				ring->slot[nm_i].flags = slot_flags;
				PNMB_O(kring, &ring->slot[nm_i], &paddr);
				netmap_sync_map_cpu(na, (bus_dma_tag_t)na->pdev,
						    &paddr, len, NR_RX);
				total_frag_len += frag_len;
				nm_prdis("slot[%d].len: %d flags: %d agg_ring_cons: %d bd_opaque: %d rx_agg_prod: %d\n",
					 nm_i, ring->slot[nm_i].len, ring->slot[nm_i].flags, cons, rxbd->rx_bd_opaque, rx_agg_prod);
				nm_i = nm_next(nm_i, lim);
				rx_agg_prod = NEXT_RX_AGG(rx_agg_prod);
				rx_sw_agg_prod = RING_RX_AGG(bp, NEXT_RX_AGG(rx_sw_agg_prod));
			}
			rxr->rx_agg_prod = rx_agg_prod;
			rxr->rx_sw_agg_prod = rx_sw_agg_prod;
		}
		tmp_raw_cons = NEXT_RAW_CMP(tmp_raw_cons);
		cp_cons = RING_CMP(tmp_raw_cons);
		real_cons = tmp_raw_cons;
		rxr->rx_prod = NEXT_RX(rx_prod);
		work_done++;
	}

	if (work_done) {
		kring->nr_hwtail = nm_i;
		cpr->cp_raw_cons = real_cons;
		tgl = cpr->toggle;
		db = &cpr->cp_db;
		/* barrier - TBD revisit? */
		wmb();
		bnxt_writeq(bp, db->db_key64 | DBR_TYPE_CQ_ARMALL | DB_TOGGLE(tgl) |
			    DB_RING_IDX(db, cpr->cp_raw_cons), db->doorbell);
		kring->nr_kflags &= ~NKR_PENDINTR;
		netdev_dbg(bp->dev, "%s: db_key 0x%llX, rx_prod 0x%x, agg db_key 0x%llX, "
			   "rx_agg_prod 0x%x\n", __func__, rxr->rx_db.db_key64, rxr->rx_prod,
			   rxr->rx_agg_db.db_key64, rxr->rx_agg_prod);
		bnxt_db_write(bp, &rxr->rx_db, rxr->rx_prod);
		bnxt_db_write(bp, &rxr->rx_agg_db, rxr->rx_agg_prod);
		nm_prdis("END cp_raw_cons: %d kring->nr_hwtail : %d rx_prod: %d rx_agg_prod: %d\n",
			 cpr->cp_raw_cons, kring->nr_hwtail, rxr->rx_prod, rxr->rx_agg_prod);
	}
	return 0;
}

/*
 * Reconcile kernel and user view of the receive ring.
 * Same as for the txsync, this routine must be efficient.
 * The caller guarantees a single invocations, but races against
 * the rest of the driver should be handled here.
 *
 * When called, userspace has released buffers up to ring->head
 * (last one excluded).
 *
 * If (flags & NAF_FORCE_READ) also check for incoming packets irrespective
 * of whether or not we received an interrupt.
 */
int bnxt_netmap_rxsync(struct netmap_kring *kring, int flags)
{
	u_int const lim = kring->nkr_num_slots - 1;
	struct netmap_adapter *na = kring->na;
	u_int const head = kring->rhead;
	struct ifnet *ifp = na->ifp;

	/* device-specific */
	struct NM_BNXT_ADAPTER *bp = netdev_priv(ifp);

	if (!netif_carrier_ok(ifp) || !netif_running(ifp))
		return 0;

	if (unlikely(head > lim))
		return netmap_ring_reinit(kring);

	if (!(bp->flags & BNXT_FLAG_JUMBO))
		return __bnxt_netmap_rxsync(kring, flags);

	return bnxt_netmap_rxsync_jumbo(kring, flags);
}

/*
 * if in netmap mode, attach the netmap buffers to the ring and return true.
 * Otherwise return false.
 */
int bnxt_netmap_configure_tx_ring(struct NM_BNXT_ADAPTER *adapter,
				  int ring_nr)
{
	struct netmap_adapter *na = NA(adapter->dev);
	struct bnxt_tx_ring_info *txr;
	struct netmap_slot *slot;

	slot = netmap_reset(na, NR_TX, ring_nr, 0);
	if (!slot)
		return 0; /* not in native netmap mode */

	txr = &adapter->tx_ring[adapter->tx_ring_map[ring_nr]];
	txr->tx_cpr->netmapped = 1;
	txr->bnapi->cp_ring.netmapped = 1;
	/*
	 * On some cards we would set up the slot addresses now.
	 * But on bnxt, the address will be written to the WQ when
	 * each packet arrives in bnxt_netmap_txsync
	 */

	return 1;
}

int bnxt_netmap_configure_rx_ring(struct NM_BNXT_ADAPTER *adapter, struct bnxt_rx_ring_info *rxr)
{
	/*
	 * In netmap mode, we must preserve the buffers made
	 * available to userspace before the if_init()
	 * (this is true by default on the TX side, because
	 * init makes all buffers available to userspace).
	 */
	struct netmap_adapter *na = NA(adapter->dev);
	struct netmap_slot *slot;
	int count = 0, i;
	int lim, ring_nr = rxr->netmap_idx;
	struct rx_bd *rxbd;
	u32 prod;
	struct ifnet *ifp = na->ifp;
	struct NM_BNXT_ADAPTER *bp = netdev_priv(ifp);

	slot = netmap_reset(na, NR_RX, ring_nr, 0);
	if (!slot)
		return 0; /* not in native netmap mode */

	lim = na->num_rx_desc - 1 - nm_kr_rxspace(na->rx_rings[ring_nr]);
	rxr->rx_prod = 0;
	prod = rxr->rx_prod;

	/* Add this so that even if the NM ring reset fails
	 * the netmapped flag is set and we will not timeout ring_free
	 * during teardown
	 */
	rxr->rx_cpr->netmapped = 1;
	if (bp->flags & BNXT_FLAG_JUMBO) {
		slot = netmap_reset(na, NR_RX, ring_nr + 1, 0);
		if (!slot)
			return 0; /* not in native netmap mode */

		while (count < lim) {
			uint64_t paddr;

			rxbd = &rxr->rx_desc_ring[RX_RING(bp, prod)][RX_IDX(prod)];
			PNMB_O(na->rx_rings[ring_nr + 1], &slot[count], &paddr);
			rxbd->rx_bd_haddr = cpu_to_le64(paddr);
			rxbd->rx_bd_opaque = prod;
			prod = NEXT_RX(prod);
			count++;
		}
		nm_prdis("populated %d Rx bufs in ring %d rxr: %p lim = %d",
			 count, ring_nr + 1, rxr, lim);
		rxr->rx_prod = prod;
		rxr->rx_next_cons = 0;

		rxr->rx_agg_prod = 0;
		prod = rxr->rx_agg_prod;
		for (i = 0; i < AGG_NM_RINGS; i++) {
			slot = netmap_reset(na, NR_RX, ring_nr + 2 + i, 0);
			if (!slot)
				return 0; /* not in native netmap mode */

			count = 0;
			while (count < lim) {
				uint64_t paddr;

				rxbd = &rxr->rx_agg_desc_ring[RX_AGG_RING(bp, prod)][RX_IDX(prod)];
				PNMB_O(na->rx_rings[ring_nr + 2 + i], &slot[count], &paddr);
				rxbd->rx_bd_haddr = cpu_to_le64(paddr);
				rxbd->rx_bd_opaque = prod;
				prod = NEXT_RX_AGG(prod);
				count++;
			}
			nm_prdis("populated %d Rx AGG bufs in ring %d prod = %d",
				 count, ring_nr + 2 + i, prod);
		}
		rxr->rx_agg_prod = prod;
		rxr->rx_sw_agg_prod = prod;
	} else {
		while (count < lim) {
			uint64_t paddr;

			rxbd = &rxr->rx_desc_ring[RX_RING(bp, prod)][RX_IDX(prod)];
			PNMB_O(na->rx_rings[ring_nr], slot + count, &paddr);
			rxbd->rx_bd_haddr = cpu_to_le64(paddr);
			rxbd->rx_bd_opaque = prod;
			prod = NEXT_RX(prod);
			count++;
		}
		nm_prdis("populated %d Rx bufs in ring %d lim = %d", count, ring_nr, lim);
	}

	/* ensure wqes are visible to device before updating doorbell record */
	wmb();
	if (bp->flags & BNXT_FLAG_JUMBO) {
		netdev_dbg(bp->dev, "%s: BNXT_FLAG_JUMBO db_key 0x%llX, rx_agg_prod 0x%x\n",
			   __func__, rxr->rx_agg_prod.db_key64, rxr->rx_agg_prod);
		bnxt_db_write(bp, &rxr->rx_agg_db, rxr->rx_agg_prod);
	}
	netdev_dbg(bp->dev, "%s: db_key 0x%llX, rx_prod 0x%x\n",
		   __func__, rxr->rx_db.db_key64, rxr->rx_prod);
	bnxt_db_write(bp, &rxr->rx_db, rxr->rx_prod);

	return 1;
}

int bnxt_netmap_config(struct netmap_adapter *na, struct nm_config_info *info)
{
	struct ifnet *ifp = na->ifp;
	struct NM_BNXT_ADAPTER *bp;

	bp = netdev_priv(ifp);
	info->num_tx_rings = bp->tx_nr_rings_per_tc;
	info->num_rx_rings = bp->rx_nr_rings;
	if (bp->dev->mtu > NETMAP_BUF_SIZE(na) || bp->flags & BNXT_FLAG_JUMBO) {
		info->num_rx_rings = 2 * info->num_rx_rings + info->num_rx_rings * AGG_NM_RINGS;
		info->rx_buf_maxsize = BNXT_RX_PAGE_SIZE;
	} else {
		info->rx_buf_maxsize = NETMAP_BUF_SIZE(na);
	}
	info->num_tx_descs = bp->tx_ring_size + 1;
	info->num_rx_descs = bp->rx_ring_size + 1;

	return 0;
}

/*
 * The attach routine, called at the end of bnxt_create_netdev(),
 * fills the parameters for netmap_attach() and calls it.
 * It cannot fail, in the worst case (such as no memory)
 * netmap mode will be disabled and the driver will only
 * operate in standard mode.
 */
void bnxt_netmap_attach(struct NM_BNXT_ADAPTER *adapter)
{
	struct netmap_adapter na;

	bzero(&na, sizeof(na));

	na.ifp = adapter->dev;
	na.pdev = &adapter->pdev->dev;
	na.na_flags = NAF_MOREFRAG;
	na.num_tx_desc = adapter->tx_ring_size + 1;
	na.num_rx_desc = adapter->rx_ring_size + 1;
	na.nm_txsync = bnxt_netmap_txsync;
	na.nm_rxsync = bnxt_netmap_rxsync;
	na.nm_register = bnxt_netmap_reg;
	na.nm_config = bnxt_netmap_config;

	/* each channel has 1 rx ring and a tx for each tc */
	na.num_tx_rings = adapter->tx_nr_rings_per_tc;
	na.num_rx_rings = adapter->rx_nr_rings;
	na.rx_buf_maxsize = 1500; /* will be overwritten by nm_config */
	netmap_attach(&na);
}

#endif /* NETMAP_BNXT_MAIN */

#endif /* __BNXT_NETMAP_LINUX_H__ */

/* end of file */
