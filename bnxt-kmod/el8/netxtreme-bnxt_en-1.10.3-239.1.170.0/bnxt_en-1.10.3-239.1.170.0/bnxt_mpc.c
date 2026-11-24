/* Broadcom NetXtreme-C/E network driver.
 *
 * Copyright (c) 2022-2025 Broadcom Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 */

#include <linux/stddef.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/slab.h>

#include "bnxt_compat.h"
#include "bnxt_hsi.h"
#include "bnxt.h"
#include "bnxt_mpc.h"
#include "bnxt_crypto.h"
#include "bnxt_ktls.h"
#include "bnxt_tfc.h"

void bnxt_alloc_mpc_info(struct bnxt *bp, u8 mpc_chnls_cap)
{
	if (mpc_chnls_cap) {
		if (!bp->mpc_info)
			bp->mpc_info = kzalloc(sizeof(*bp->mpc_info),
					       GFP_KERNEL);
	} else {
		bnxt_free_mpc_info(bp);
	}
	if (bp->mpc_info)
		bp->mpc_info->mpc_chnls_cap = mpc_chnls_cap;
}

void bnxt_free_mpc_info(struct bnxt *bp)
{
	kfree(bp->mpc_info);
	bp->mpc_info = NULL;
}

int bnxt_mpc_tx_rings_in_use(struct bnxt *bp)
{
	struct bnxt_mpc_info *mpc = bp->mpc_info;
	int i, mpc_tx = 0;

	if (!mpc)
		return 0;
	for (i = 0; i < BNXT_MPC_TYPE_MAX; i++)
		mpc_tx += mpc->mpc_ring_count[i];
	return mpc_tx;
}

int bnxt_mpc_cp_rings_in_use(struct bnxt *bp)
{
	struct bnxt_mpc_info *mpc = bp->mpc_info;

	if (!mpc)
		return 0;
	return mpc->mpc_cp_rings;
}

bool bnxt_napi_has_mpc(struct bnxt *bp, int i)
{
	struct bnxt_mpc_info *mpc = bp->mpc_info;

	if (!mpc)
		return false;

	return i < mpc->mpc_cp_rings;
}

static bool bnxt_set_one_mpc_cp_ring(struct bnxt_tx_ring_info *txr,
				     struct bnxt_napi *bnapi,
				     u32 mpc_type,
				     struct bnxt_cp_ring_info *cpr)
{
	if (txr->bnapi == bnapi && !txr->tx_cpr) {
		txr->tx_cpr = cpr;
		txr->tx_napi_idx = mpc_type;
		bnapi->tx_mpc_ring[mpc_type] = txr;
		return true;
	}
	return false;
}

void bnxt_set_mpc_cp_ring(struct bnxt *bp, int bnapi_idx,
			  struct bnxt_cp_ring_info *cpr)
{
	struct bnxt_mpc_info *mpc = bp->mpc_info;
	struct bnxt_napi *bnapi;
	int i, j;

	bnapi = bp->bnapi[bnapi_idx];
	for (i = 0; i < BNXT_MPC_TYPE_MAX; i++) {
		int num = mpc->mpc_ring_count[i];

		for (j = 0; j < num; j++) {
			struct bnxt_tx_ring_info *txr = &mpc->mpc_rings[i][j];

			if (txr->persistent)
				continue;
			if (bnxt_set_one_mpc_cp_ring(txr, bnapi, i, cpr))
				break;
		}
	}
	cpr->cp_ring_type = BNXT_CP_TYPE_MP;
}

void bnxt_trim_mpc_rings(struct bnxt *bp)
{
	struct bnxt_mpc_info *mpc = bp->mpc_info;
	int max = bp->tx_nr_rings_per_tc;
	u8 max_cp = 0;
	int i;

	if (!mpc)
		return;

	for (i = 0; i < BNXT_MPC_TYPE_MAX; i++) {
		mpc->mpc_ring_count[i] = min_t(u8, mpc->mpc_ring_count[i], max);
		max_cp = max(max_cp, mpc->mpc_ring_count[i]);
	}
	mpc->mpc_cp_rings = max_cp;
}

enum bnxt_mpc_type {
	BNXT_MPC_CRYPTO,
	BNXT_MPC_CFA,
};

static void __bnxt_set_dflt_mpc_rings(struct bnxt *bp, enum bnxt_mpc_type type,
				      int *avail, int avail_cp)
{
	struct bnxt_mpc_info *mpc = bp->mpc_info;
	int dflt1, dflt2;
	int idx1, idx2;
	int min1, min2;
	int val1, val2;

	if (type == BNXT_MPC_CRYPTO) {
		min1 = BNXT_MIN_MPC_TCE;
		min2 = BNXT_MIN_MPC_RCE;
		dflt1 = BNXT_DFLT_MPC_TCE;
		dflt2 = BNXT_DFLT_MPC_RCE;
		idx1 = BNXT_MPC_TCE_TYPE;
		idx2 = BNXT_MPC_RCE_TYPE;
	} else {
		min1 = BNXT_MIN_MPC_TE_CFA;
		min2 = BNXT_MIN_MPC_RE_CFA;
		dflt1 = BNXT_DFLT_MPC_TE_CFA;
		dflt2 = BNXT_DFLT_MPC_RE_CFA;
		idx1 = BNXT_MPC_TE_CFA_TYPE;
		idx2 = BNXT_MPC_RE_CFA_TYPE;
	}
	if (*avail < (min1 + min2))
		return;

	val1 = min_t(int, *avail / 2, bp->tx_nr_rings_per_tc);
	val2 = val1;

	val1 = min_t(int, val1, dflt1);
	val2 = min_t(int, val2, dflt2);

	if (avail_cp < min1 || avail_cp < min2)
		return;

	val1 = min(val1, avail_cp);
	val2 = min(val2, avail_cp);

	mpc->mpc_ring_count[idx1] = val1;
	mpc->mpc_ring_count[idx2] = val2;

	*avail = *avail - val1 - val2;
}

void bnxt_set_dflt_mpc_rings(struct bnxt *bp)
{
	struct bnxt_hw_resc *hw_resc = &bp->hw_resc;
	struct bnxt_mpc_info *mpc = bp->mpc_info;
	int avail, mpc_cp, i;
	int avail_cp;

	if (!mpc)
		return;

	for (i = 0; i < BNXT_MPC_TYPE_MAX; i++)
		mpc->mpc_ring_count[i] = 0;
	mpc->mpc_cp_rings = 0;

	avail = hw_resc->max_tx_rings - bp->tx_nr_rings;

	avail_cp = hw_resc->max_cp_rings - bp->tx_nr_rings -
		   bp->rx_nr_rings;

	if (BNXT_MPC_CRYPTO_CAPABLE(bp))
		__bnxt_set_dflt_mpc_rings(bp, BNXT_MPC_CRYPTO, &avail, avail_cp);

	if (BNXT_MPC_CFA_CAPABLE(bp))
		__bnxt_set_dflt_mpc_rings(bp, BNXT_MPC_CFA, &avail, avail_cp);

	for (i = 0, mpc_cp = 0; i < BNXT_MPC_TYPE_MAX; i++) {
		if (mpc_cp < mpc->mpc_ring_count[i])
			mpc_cp = mpc->mpc_ring_count[i];
	}
	mpc->mpc_cp_rings = mpc_cp;
}

void bnxt_init_mpc_ring_struct(struct bnxt *bp)
{
	struct bnxt_mpc_info *mpc = bp->mpc_info;
	int i, j;

	if (!BNXT_MPC_CRYPTO_CAPABLE(bp) && !BNXT_MPC_CFA_CAPABLE(bp))
		return;

	for (i = 0; i < BNXT_MPC_TYPE_MAX; i++) {
		int num = mpc->mpc_ring_count[i];
		struct bnxt_tx_ring_info *txr;

		txr = mpc->mpc_rings[i];
		if (!txr)
			continue;
		for (j = 0; j < num; j++) {
			struct bnxt_ring_mem_info *rmem;
			struct bnxt_ring_struct *ring;

			txr = &mpc->mpc_rings[i][j];

			txr->tx_ring_struct.ring_mem.flags =
				BNXT_RMEM_RING_PTE_FLAG;
			txr->bnapi = bp->bnapi[j];
			txr->txq_index = j;

			ring = &txr->tx_ring_struct;
			rmem = &ring->ring_mem;
			rmem->nr_pages = bp->tx_nr_pages;
			rmem->page_size = HW_TXBD_RING_SIZE;
			rmem->pg_arr = (void **)txr->tx_desc_ring;
			rmem->dma_arr = txr->tx_desc_mapping;
			rmem->vmem_size = SW_MPC_TXBD_RING_SIZE *
					  bp->tx_nr_pages;
			rmem->vmem = (void **)&txr->tx_buf_ring;
		}
	}
}

struct bnxt_tx_ring_info *bnxt_select_mpc_ring(struct bnxt *bp, int ring_type)
{
	struct bnxt_mpc_info *mpc = bp->mpc_info;
	int n;

	if (ring_type >= BNXT_MPC_TYPE_MAX)
		return NULL;

	n = smp_processor_id() % mpc->mpc_ring_count[ring_type];
	return &mpc->mpc_rings[ring_type][n];
}

int bnxt_alloc_mpcs(struct bnxt *bp)
{
	struct bnxt_mpc_info *mpc = bp->mpc_info;
	int i;

	if (!BNXT_MPC_CRYPTO_CAPABLE(bp) && !BNXT_MPC_CFA_CAPABLE(bp))
		return 0;

	for (i = 0; i < BNXT_MPC_TYPE_MAX; i++) {
		int num = mpc->mpc_ring_count[i];
		struct bnxt_tx_ring_info *txr;

		if (!num)
			continue;
		txr = kcalloc(num, sizeof(*txr), GFP_KERNEL);
		if (!txr)
			return -ENOMEM;
		mpc->mpc_rings[i] = txr;
	}

	return 0;
}

static int bnxt_alloc_one_mpc_for_nq(struct bnxt *bp, struct bnxt_napi *bnapi)
{
	if (!bnxt_napi_has_mpc(bp, bnapi->index))
		return 0;
	bnapi->tx_mpc_ring = kcalloc(BNXT_MPC_TYPE_MAX,
				     sizeof(*bnapi->tx_mpc_ring),
				     GFP_KERNEL);
	if (!bnapi->tx_mpc_ring)
		return -ENOMEM;
	return 0;
}

int bnxt_alloc_mpcs_for_nq(struct bnxt *bp)
{
	int i, rc;

	if (!bp->mpc_info)
		return 0;

	for (i = 0; i < bp->cp_nr_rings; i++) {
		struct bnxt_napi *bnapi = bp->bnapi[i];

		if (bnapi->tx_mpc_ring)
			continue;
		rc = bnxt_alloc_one_mpc_for_nq(bp, bnapi);
		if (rc)
			return rc;
	}
	return 0;
}

void bnxt_free_mpcs(struct bnxt *bp)
{
	struct bnxt_mpc_info *mpc = bp->mpc_info;
	int i;

	if (!mpc)
		return;

	for (i = 0; i < BNXT_MPC_TYPE_MAX; i++) {
		kfree(mpc->mpc_rings[i]);
		mpc->mpc_rings[i] = NULL;
	}
}

static void bnxt_free_one_mpc_for_nq(struct bnxt *bp, struct bnxt_napi *bnapi)
{
	kfree(bnapi->tx_mpc_ring);
	bnapi->tx_mpc_ring = NULL;
}

void bnxt_reset_mpc_cpr(struct bnxt *bp)
{
	struct bnxt_mpc_info *mpc = bp->mpc_info;
	int i;

	if (!mpc || !mpc->mpc_rings[0])
		return;

	for (i = 0; i < BNXT_MPC_TYPE_MAX; i++) {
		int num = mpc->mpc_ring_count[i], j;

		for (j = 0; j < num; j++) {
			struct bnxt_tx_ring_info *txr = &mpc->mpc_rings[i][j];

			if (txr->persistent)
				continue;
			txr->tx_cpr = NULL;
		}
	}
}

void bnxt_free_mpcs_for_nq(struct bnxt *bp)
{
	struct bnxt_mpc_info *mpc = bp->mpc_info;
	int i;

	if (!bp->bnapi || !mpc)
		return;
	for (i = 0; i < bp->cp_nr_rings; i++) {
		struct bnxt_napi *bnapi = bp->bnapi[i];

		if (BNXT_MPC0_NAPI(bnapi))
			continue;
		bnxt_free_one_mpc_for_nq(bp, bnapi);
	}
}

static int bnxt_alloc_one_mpc_ring(struct bnxt *bp,
				   struct bnxt_tx_ring_info *txr,
				   int chnl_type)
{
	struct bnxt_ring_struct *ring;
	int rc;

	ring = &txr->tx_ring_struct;
	rc = bnxt_alloc_ring(bp, &ring->ring_mem);
	if (rc)
		return rc;
	ring->queue_id = BNXT_MPC_QUEUE_ID;
	ring->mpc_chnl_type = chnl_type;
	/* for stats context */
	ring->grp_idx = txr->bnapi->index;
	spin_lock_init(&txr->tx_lock);

	return 0;
}

int bnxt_alloc_mpc_rings(struct bnxt *bp)
{
	struct bnxt_mpc_info *mpc = bp->mpc_info;
	int i, j;

	if (!mpc)
		return 0;

	for (i = 0; i < BNXT_MPC_TYPE_MAX; i++) {
		int num = mpc->mpc_ring_count[i], rc;

		for (j = 0; j < num; j++) {
			struct bnxt_tx_ring_info *txr = &mpc->mpc_rings[i][j];

			if (txr->persistent)
				continue;
			rc = bnxt_alloc_one_mpc_ring(bp, txr, i);
			if (rc)
				return rc;
		}
	}
	return 0;
}

void bnxt_free_mpc_rings(struct bnxt *bp)
{
	struct bnxt_mpc_info *mpc = bp->mpc_info;
	int i, j;

	if (!mpc)
		return;

	for (i = 0; i < BNXT_MPC_TYPE_MAX; i++) {
		int num = mpc->mpc_ring_count[i];

		if (!mpc->mpc_rings[i])
			continue;
		for (j = 0; j < num; j++) {
			struct bnxt_tx_ring_info *txr = &mpc->mpc_rings[i][j];
			struct bnxt_ring_struct *ring = &txr->tx_ring_struct;

			if (txr->persistent)
				continue;
			bnxt_free_ring(bp, &ring->ring_mem);
		}
	}
}

void bnxt_init_mpc_rings(struct bnxt *bp)
{
	struct bnxt_mpc_info *mpc = bp->mpc_info;
	int i, j;

	if (!mpc)
		return;

	for (i = 0; i < BNXT_MPC_TYPE_MAX; i++) {
		int num = mpc->mpc_ring_count[i];

		for (j = 0; j < num; j++) {
			struct bnxt_tx_ring_info *txr = &mpc->mpc_rings[i][j];
			struct bnxt_ring_struct *ring = &txr->tx_ring_struct;

			if (txr->persistent)
				continue;
			txr->tx_prod = 0;
			txr->tx_cons = 0;
			txr->tx_hw_cons = 0;
			WRITE_ONCE(txr->dev_state, 0);
			ring->fw_ring_id = INVALID_HW_RING_ID;
		}
	}
}

static int bnxt_hwrm_one_mpc_ring_alloc(struct bnxt *bp,
					struct bnxt_tx_ring_info *txr)
{
	struct bnxt_cp_ring_info *cpr = txr->tx_cpr;
	struct bnxt_ring_struct *ring;
	int rc;

	ring = &cpr->cp_ring_struct;
	if (ring->fw_ring_id == INVALID_HW_RING_ID) {
		rc = bnxt_hwrm_cp_ring_alloc_p5(bp, cpr);
		if (rc)
			return rc;
	}
	/* tx_idx is not used for MPC rings on P7+ chips */
	return bnxt_hwrm_tx_ring_alloc(bp, txr, 0);
}

int bnxt_hwrm_mpc_ring_alloc(struct bnxt *bp)
{
	struct bnxt_mpc_info *mpc = bp->mpc_info;
	int i, j, rc;

	if (!mpc)
		return 0;

	for (i = 0; i < BNXT_MPC_TYPE_MAX; i++) {
		int num = mpc->mpc_ring_count[i];

		for (j = 0; j < num; j++) {
			struct bnxt_tx_ring_info *txr = &mpc->mpc_rings[i][j];

			if (txr->persistent)
				continue;
			rc = bnxt_hwrm_one_mpc_ring_alloc(bp, txr);
			if (rc)
				return rc;
		}
	}
	return 0;
}

void bnxt_hwrm_mpc_ring_free(struct bnxt *bp, bool close_path)
{
	struct bnxt_mpc_info *mpc = bp->mpc_info;
	int i, j;

	if (!mpc)
		return;

	for (i = 0; i < BNXT_MPC_TYPE_MAX; i++) {
		int num = mpc->mpc_ring_count[i];

		if (!mpc->mpc_rings[i])
			continue;
		for (j = 0; j < num; j++) {
			struct bnxt_tx_ring_info *txr = &mpc->mpc_rings[i][j];

			if (txr->persistent)
				continue;
			bnxt_hwrm_tx_ring_free(bp, txr, close_path);
		}
	}
}

int bnxt_start_xmit_mpc(struct bnxt *bp, struct bnxt_tx_ring_info *txr,
			void *data, uint len, unsigned long handle)
{
	u32 bds, total_bds, bd_space, free_size;
	struct bnxt_sw_mpc_tx_bd *tx_buf;
	struct tx_bd *txbd;
	u16 prod;

	if (READ_ONCE(txr->dev_state) == BNXT_DEV_STATE_CLOSING)
		return -ENODEV;

	bds = DIV_ROUND_UP(len, sizeof(*txbd));
	total_bds = bds + 1;
	free_size = bnxt_tx_avail(bp, txr);
	if (free_size < total_bds)
		return -EBUSY;

	prod = txr->tx_prod;
	txbd = &txr->tx_desc_ring[TX_RING(bp, prod)][TX_IDX(prod)];
	tx_buf = &txr->tx_mpc_buf_ring[RING_TX(bp, prod)];
	tx_buf->handle = handle;
	tx_buf->inline_bds = total_bds;

	txbd->tx_bd_len_flags_type =
		cpu_to_le32((len << TX_BD_LEN_SHIFT) | TX_BD_TYPE_MPC_TX_BD |
			    TX_BD_CNT(total_bds));
	txbd->tx_bd_opaque = SET_TX_OPAQUE(bp, txr, prod, total_bds);

	prod = NEXT_TX(prod);
	txbd = &txr->tx_desc_ring[TX_RING(bp, prod)][TX_IDX(prod)];
	bd_space = TX_DESC_CNT - TX_IDX(prod);
	if (bd_space < bds) {
		uint len0 = bd_space * sizeof(*txbd);

		memcpy(txbd, data, len0);
		prod += bd_space;
		txbd = &txr->tx_desc_ring[TX_RING(bp, prod)][TX_IDX(prod)];
		bds -= bd_space;
		len -= len0;
		data += len0;
	}
	memcpy(txbd, data, len);
	prod += bds;
	txr->tx_prod = prod;

	/* Sync BD data before updating doorbell */
	wmb();

	netdev_dbg(bp->dev, "%s: db_key 0x%llX, txr prod 0x%x tx_bd_opaque %d, txq_index %d\n",
		   __func__, txr->tx_db.db_key64, prod, txbd->tx_bd_opaque, txr->txq_index);
	bnxt_db_write(bp, &txr->tx_db, prod);

	return 0;
}

/* Returns true if the ring is successfully marked as closing.  It
 * means that there will be no more MPC transmissions and NAPI will now
 * complete any MPC completions on the completion ring with NULL handles
 * to signal abort.
 */
static bool bnxt_disable_mpc_ring(struct bnxt_mpc_info *mpc, int mpc_ring)
{
	struct bnxt_tx_ring_info *txr;
	bool disabled = false;
	int i;

	for (i = 0; i < BNXT_MPC_TYPE_MAX; i++) {
		if (mpc_ring >= mpc->mpc_ring_count[i])
			continue;
		txr = &mpc->mpc_rings[i][mpc_ring];
		spin_lock_bh(&txr->tx_lock);
		if (!READ_ONCE(txr->dev_state)) {
			disabled = true;
			WRITE_ONCE(txr->dev_state, BNXT_DEV_STATE_CLOSING);
		}
		spin_unlock_bh(&txr->tx_lock);
		if (!disabled)
			break;
	}
	/* Make sure napi polls see @dev_state change */
	if (disabled)
		synchronize_net();
	return disabled;
}

static void bnxt_enable_mpc_ring(struct bnxt_mpc_info *mpc, int mpc_ring)
{
	struct bnxt_tx_ring_info *txr;
	int i;

	for (i = 0; i < BNXT_MPC_TYPE_MAX; i++) {
		if (mpc_ring >= mpc->mpc_ring_count[i])
			continue;
		txr = &mpc->mpc_rings[i][mpc_ring];
		WRITE_ONCE(txr->dev_state, 0);
	}
}

static void bnxt_clear_one_mpc_entries(struct bnxt *bp,
				       struct bnxt_tx_ring_info *txr)
{
	struct bnxt_sw_mpc_tx_bd *tx_buf;
	unsigned long handle;
	int i, max_idx;
	u32 client;

	max_idx = bp->tx_nr_pages * TX_DESC_CNT;

	for (i = 0; i < max_idx; i++) {
		tx_buf = &txr->tx_mpc_buf_ring[i];
		handle = tx_buf->handle;
		if (handle && handle != BNXT_INV_MPC_HDL) {
			client = txr->tx_ring_struct.mpc_chnl_type;
			if (client == BNXT_MPC_TCE_TYPE ||
			    client == BNXT_MPC_RCE_TYPE)
				bnxt_ktls_mpc_cmp(bp, client, handle, NULL, 0);
			else if (client == BNXT_MPC_TE_CFA_TYPE ||
				 client == BNXT_MPC_RE_CFA_TYPE)
				bnxt_tfc_mpc_cmp(bp, client, handle, NULL, 0);
			tx_buf->handle = BNXT_INV_MPC_HDL;
		}
	}
}

void bnxt_free_mpc_entries(struct bnxt *bp)
{
	struct bnxt_mpc_info *mpc = bp->mpc_info;
	int i, j;

	if (!mpc)
		return;

	for (i = 0; i < BNXT_MPC_TYPE_MAX; i++) {
		if (!mpc->mpc_rings[i])
			continue;

		for (j = 0; j < mpc->mpc_ring_count[i]; j++) {
			struct bnxt_tx_ring_info *txr = &mpc->mpc_rings[i][j];

			/* Not freed by close; the free path drains first. */
			if (txr->persistent)
				continue;
			if (txr->tx_mpc_buf_ring)
				bnxt_clear_one_mpc_entries(bp, txr);
		}
	}
}

static void bnxt_mpc_ring_stop(struct bnxt *bp, struct bnxt_mpc_info *mpc,
			       int mpc_ring)
{
	struct bnxt_tx_ring_info *txr;
	struct bnxt_cp_ring_info *cpr;
	int i;

	for (i = 0; i < BNXT_MPC_TYPE_MAX; i++) {
		if (mpc->mpc_ring_count[i] > mpc_ring) {
			txr = &mpc->mpc_rings[i][mpc_ring];
			bnxt_hwrm_tx_ring_free(bp, txr, true);
		}
	}
	/* CP ring must be freed at the end to guarantee that the HWRM_DONE
	 * responses for HWRM_RING_FREE can still be seen on the CP ring.
	 */
	for (i = 0; i < BNXT_MPC_TYPE_MAX; i++) {
		if (mpc->mpc_ring_count[i] > mpc_ring) {
			txr = &mpc->mpc_rings[i][mpc_ring];
			cpr = txr->tx_cpr;
			if (cpr)
				bnxt_hwrm_cp_ring_free(bp, cpr);
		}
	}
	/* No new DMA to the CP ring at this point, but we need to wait
	 * for any in-flight NAPI poll to finish before we zero the
	 * completion entries in the CP ring.
	 */
	synchronize_net();
	for (i = 0; i < BNXT_MPC_TYPE_MAX; i++) {
		if (mpc->mpc_ring_count[i] > mpc_ring) {
			txr = &mpc->mpc_rings[i][mpc_ring];
			cpr = txr->tx_cpr;
			if (cpr)
				bnxt_clear_one_cp_ring(bp, cpr);
			bnxt_clear_one_mpc_entries(bp, txr);
		}
	}
}

static int bnxt_mpc_ring_start(struct bnxt *bp, struct bnxt_mpc_info *mpc,
			       int mpc_ring)
{
	struct bnxt_tx_ring_info *txr;
	int i, rc;

	for (i = 0; i < BNXT_MPC_TYPE_MAX; i++) {
		if (mpc->mpc_ring_count[i] > mpc_ring) {
			txr = &mpc->mpc_rings[i][mpc_ring];
			txr->tx_prod = 0;
			txr->tx_cons = 0;
			txr->tx_hw_cons = 0;
			rc = bnxt_hwrm_one_mpc_ring_alloc(bp, txr);
			if (rc)
				return rc;
		}
	}
	return 0;
}

/* Called from bnxt_sp_task() with netdev_lock held.  If the device is not
 * open there is nothing to do: the open path will re-allocate and
 * re-initialize all MPC rings.
 */
void bnxt_mpc_ring_reset_task(struct bnxt *bp)
{
	struct bnxt_mpc_info *mpc = bp->mpc_info;
	struct bnxt_tx_ring_info *txr;
	int i, j;

	if (!mpc || !test_bit(BNXT_STATE_OPEN, &bp->state))
		return;

	for (i = 0; i < BNXT_MPC_TYPE_MAX; i++) {
		for (j = 0; j < mpc->mpc_ring_count[i]; j++) {
			txr = &mpc->mpc_rings[i][j];
			if (READ_ONCE(txr->dev_state) != BNXT_DEV_STATE_CLOSING)
				continue;

			/* Resets the whole ring (both types) */
			netdev_warn(bp->dev, "Resetting MPC ring %d\n", j);
			bnxt_mpc_ring_stop(bp, mpc, j);
			if (bnxt_mpc_ring_start(bp, mpc, j)) {
				netdev_err(bp->dev, "Error starting MPC ring %d, resetting device\n",
					   j);
				bnxt_mpc_ring_stop(bp, mpc, j);
				bnxt_reset_task(bp, true);
				/* bnxt_reset_task() will clear everything */
				return;
			}
			bnxt_enable_mpc_ring(mpc, j);
		}
	}
}

static int bnxt_mpc_ring_reset(struct bnxt *bp, int mpc_ring)
{
	struct bnxt_mpc_info *mpc = bp->mpc_info;

	if (!mpc)
		return 0;
	if (mpc_ring >= mpc->mpc_cp_rings)
		return -EINVAL;

	if (!bnxt_disable_mpc_ring(mpc, mpc_ring))
		return 0;

	/* If the device is going down, the reopen will re-allocate and
	 * re-initialize all MPC rings.  Otherwise defer the reset to
	 * bnxt_sp_task() to safely hold the netdev_lock.
	 */
	if (test_bit(BNXT_STATE_OPEN, &bp->state))
		bnxt_queue_sp_work(bp, BNXT_MPC_RESET_SP_EVENT);
	return 0;
}

int bnxt_mpc_timeout(struct bnxt *bp, struct bnxt_tx_ring_info *txr)
{
	if (!txr)
		return -EINVAL;
	if (txr->tx_ring_struct.queue_id == BNXT_MPC_QUEUE_ID)
		return bnxt_mpc_ring_reset(bp, txr->txq_index);
	return -EINVAL;
}

static bool bnxt_mpc_unsolicit(struct mpc_cmp *mpcmp)
{
	u32 client = MPC_CMP_CLIENT_TYPE(mpcmp);

	if (client != MPC_CMP_CLIENT_TCE && client != MPC_CMP_CLIENT_RCE &&
	    client != MPC_CMP_CLIENT_TE_CFA && client != MPC_CMP_CLIENT_RE_CFA)
		return false;
	return MPC_CMP_UNSOLICIT_SUBTYPE(mpcmp);
}

static void bnxt_adv_mpc_cons(struct bnxt *bp, struct bnxt_tx_ring_info *txr)
{
	struct bnxt_sw_mpc_tx_bd *mpc_buf;
	u16 tx_cons = txr->tx_cons;

	mpc_buf = &txr->tx_mpc_buf_ring[RING_TX(bp, tx_cons)];
	do {
		tx_cons += mpc_buf->inline_bds;
		txr->tx_cons = tx_cons;
		txr->tx_hw_cons = RING_TX(bp, tx_cons);
		if (tx_cons == txr->tx_prod)
			break;
		mpc_buf = &txr->tx_mpc_buf_ring[RING_TX(bp, tx_cons)];
	} while (mpc_buf->handle == BNXT_INV_MPC_HDL);
}

int bnxt_mpc_cmp(struct bnxt *bp, struct bnxt_cp_ring_info *cpr, u32 *raw_cons)
{
	struct bnxt_cmpl_entry cmpl_entry_arr[2];
	struct bnxt_napi *bnapi = cpr->bnapi;
	u16 cons = RING_CMP(cpr, *raw_cons);
	struct mpc_cmp *mpcmp, *mpcmp1;
	u32 tmp_raw_cons = *raw_cons;
	unsigned long handle = 0;
	u32 client, cmpl_num;
	u8 type;

	mpcmp = (struct mpc_cmp *)
		&cpr->cp_desc_ring[CP_RING(cons)][CP_IDX(cons)];
	type = MPC_CMP_CMP_TYPE(mpcmp);
	cmpl_entry_arr[0].cmpl = mpcmp;
	cmpl_entry_arr[0].len = sizeof(*mpcmp);
	cmpl_num = 1;
	if (type == MPC_CMP_TYPE_MID_PATH_LONG) {
		tmp_raw_cons = NEXT_RAW_CMP(tmp_raw_cons);
		cons = RING_CMP(cpr, tmp_raw_cons);
		mpcmp1 = (struct mpc_cmp *)
			 &cpr->cp_desc_ring[CP_RING(cons)][CP_IDX(cons)];

		if (!MPC_CMP_VALID(cpr, mpcmp1, tmp_raw_cons))
			return -EBUSY;
		/* The valid test of the entry must be done first before
		 * reading any further.
		 */
		dma_rmb();
		if (mpcmp1 == mpcmp + 1) {
			cmpl_entry_arr[cmpl_num - 1].len += sizeof(*mpcmp1);
		} else {
			cmpl_entry_arr[cmpl_num].cmpl = mpcmp1;
			cmpl_entry_arr[cmpl_num].len = sizeof(*mpcmp1);
			cmpl_num++;
		}
	}
	client = MPC_CMP_CLIENT_TYPE(mpcmp) >> MPC_CMP_CLIENT_SFT;

	if (!bnxt_mpc_unsolicit(mpcmp)) {
		struct bnxt_sw_mpc_tx_bd *mpc_buf;
		struct bnxt_tx_ring_info *txr;
		u16 tx_cons, idx;
		u32 opaque;

		opaque = mpcmp->mpc_cmp_opaque;
		txr = bnapi->tx_mpc_ring[client];
		tx_cons = txr->tx_cons;
		idx = TX_OPAQUE_IDX(opaque);
		if (TX_OPAQUE_RING(opaque) != txr->tx_napi_idx)
			netdev_warn(bp->dev, "Wrong opaque %x, expected ring %x, idx %x\n",
				    opaque, txr->tx_napi_idx, tx_cons);
		mpc_buf = &txr->tx_mpc_buf_ring[idx];
		if (!READ_ONCE(txr->dev_state)) {
			handle = mpc_buf->handle;
			mpc_buf->handle = BNXT_INV_MPC_HDL;
		}
		if (RING_TX(bp, tx_cons) == idx)
			bnxt_adv_mpc_cons(bp, txr);
	}
	if (client == BNXT_MPC_TCE_TYPE || client == BNXT_MPC_RCE_TYPE)
		bnxt_ktls_mpc_cmp(bp, client, handle, cmpl_entry_arr, cmpl_num);
	else if (client == BNXT_MPC_TE_CFA_TYPE || client == BNXT_MPC_RE_CFA_TYPE)
		bnxt_tfc_mpc_cmp(bp, client, handle, cmpl_entry_arr, cmpl_num);
	*raw_cons = tmp_raw_cons;
	return 0;
}

static int bnxt_alloc_mpc_subset_rings(struct bnxt *bp)
{
	struct bnxt_mpc_info *mpc = bp->mpc_info;
	int i, rc;

	if (!mpc)
		return 0;

	for (i = 0; i < BNXT_MPC_TYPE_MAX; i++) {
		struct bnxt_tx_ring_info *txr = &mpc->mpc_rings[i][0];

		rc = bnxt_alloc_one_mpc_ring(bp, txr, i);
		if (rc)
			return rc;
	}
	return 0;
}

static int bnxt_free_mpc_subset_rings(struct bnxt *bp)
{
	struct bnxt_mpc_info *mpc = bp->mpc_info;
	int i;

	if (!mpc)
		return 0;

	for (i = 0; i < BNXT_MPC_TYPE_MAX; i++) {
		struct bnxt_tx_ring_info *txr = &mpc->mpc_rings[i][0];
		struct bnxt_ring_struct *ring = &txr->tx_ring_struct;

		bnxt_free_ring(bp, &ring->ring_mem);
	}
	return 0;
}

static int bnxt_alloc_one_mpc_cp_ring(struct bnxt *bp, struct bnxt_cp_ring_info *cpr2)
{
	struct bnxt_ring_struct *ring;
	int rc;

	cpr2->cp_ring_type = BNXT_CP_TYPE_MP;
	rc = bnxt_alloc_cp_sub_ring(bp, cpr2);
	if (rc)
		return rc;
	ring = &cpr2->cp_ring_struct;
	ring->fw_ring_id = INVALID_HW_RING_ID;
	/* Force bnxt_set_db() to re-register the HDBR doorbell copy region
	 * instead of reusing a stale db_cp left over from a prior HDBR
	 * teardown (e.g. fatal FW reset), since this ring is only rebuilt
	 * while inactive.
	 */
	cpr2->cp_db.db_cp = NULL;
	cpr2->rx_ring_coal.coal_ticks = bp->rx_coal.coal_ticks;
	cpr2->rx_ring_coal.coal_bufs = bp->rx_coal.coal_bufs;

	return 0;
}

static void bnxt_set_mpc_subset_cp_ring(struct bnxt *bp, int bnapi_idx,
					struct bnxt_cp_ring_info *cpr)
{
	struct bnxt_mpc_info *mpc = bp->mpc_info;
	struct bnxt_napi *bnapi;
	int i;

	bnapi = bp->bnapi[bnapi_idx];
	for (i = 0; i < BNXT_MPC_TYPE_MAX; i++) {
		struct bnxt_tx_ring_info *txr = &mpc->mpc_rings[i][0];

		bnxt_set_one_mpc_cp_ring(txr, bnapi, i, cpr);
	}
	cpr->cp_ring_type = BNXT_CP_TYPE_MPCQ0;
}

static int bnxt_hwrm_mpc_subset_ring_alloc(struct bnxt *bp)
{
	struct bnxt_mpc_info *mpc = bp->mpc_info;
	int i, rc;

	if (!mpc)
		return 0;

	for (i = 0; i < BNXT_MPC_TYPE_MAX; i++) {
		struct bnxt_tx_ring_info *txr = &mpc->mpc_rings[i][0];

		rc = bnxt_hwrm_one_mpc_ring_alloc(bp, txr);
		if (rc)
			return rc;
		txr->persistent = 1;
	}
	return 0;
}

static int bnxt_hwrm_mpc_subset_ring_free(struct bnxt *bp)
{
	struct bnxt_mpc_info *mpc = bp->mpc_info;
	int i;

	if (!mpc)
		return 0;

	for (i = 0; i < BNXT_MPC_TYPE_MAX; i++) {
		struct bnxt_tx_ring_info *txr = &mpc->mpc_rings[i][0];

		bnxt_hwrm_tx_ring_free(bp, txr, false);
		txr->persistent = 0;
		txr->tx_cpr = NULL;
	}
	return 0;
}

static void bnxt_free_one_mpc_cp_ring(struct bnxt *bp, struct bnxt_cp_ring_info *cpr2)
{
	struct bnxt_ring_struct *ring;

	ring = &cpr2->cp_ring_struct;
	if (ring->fw_ring_id == INVALID_HW_RING_ID)
		return;

	hwrm_ring_free_send_msg(bp, ring,
				RING_FREE_REQ_RING_TYPE_L2_CMPL,
				INVALID_HW_RING_ID);
	ring->fw_ring_id = INVALID_HW_RING_ID;

	bnxt_free_ring(bp, &ring->ring_mem);
	bnxt_free_cp_arrays(cpr2);
}

/* Tear down the persistent MPC rings and the NQ0 resources behind them.
 *
 * Called with bp->mpc_ring_lock held.  Keeping the teardown body in its own
 * function means the lock is released by the caller on every path, so a
 * return added in here later cannot leak it.  A leaked mpc_ring_lock is
 * unrecoverable: the owning task exits, so every later acquisition blocks
 * forever with no discoverable holder, and only re-probe re-initialises it.
 */
static void __bnxt_free_persistent_mpc_rings(struct bnxt *bp, bool irq_re_init)
{
	struct bnxt_cp_ring_info *cpr;
	struct bnxt_ring_struct *ring;
	struct bnxt_napi *bnapi;
	struct bnxt_irq *irq;

	bnapi = bp->bnapi[BNXT_NQ0_NAPI_IDX];
	if (!BNXT_MPC0_NAPI(bnapi)) {
		/* A previous bnxt_create_persistent_mpc_rings() could
		 * have failed leaving behind an active stat ctx.
		 */
		goto free_ctx;
	}

	clear_bit(BNXT_NAPI_FLAG_MPC0, &bnapi->flags);
	/* Clear MPC0 flag first before checking if tfc is busy */
	smp_mb__after_atomic();
	while (bnxt_tfc_busy(bp))
		msleep(20);
	cpr = &bnapi->cp_ring;
	ring = &cpr->cp_ring_struct;
	irq = &bp->irq_tbl[ring->map_idx];

	bnxt_hwrm_mpc_subset_ring_free(bp);
	bnxt_free_mpc_subset_rings(bp);

	bnxt_db_nq(bp, &cpr->cp_db, cpr->cp_raw_cons);
	synchronize_irq(irq->vector);
	napi_disable_locked(&bnapi->napi);

	bnxt_free_one_mpc_cp_ring(bp, &bp->mpc_info->mpc_cq0);

	napi_enable_locked(&bnapi->napi);
	bnxt_db_nq_arm(bp, &cpr->cp_db, cpr->cp_raw_cons);

	bp->mpc_info->mpc_cq0.cp_raw_cons = 0;
free_ctx:
	if (irq_re_init) {
		bnxt_free_one_mpc_for_nq(bp, bnapi);
		bnxt_hwrm_mpc0_stat_ctx_free(bp);
		bnxt_free_one_ring_stats(bp, bnapi);
	}
}

void bnxt_free_persistent_mpc_rings(struct bnxt *bp, bool irq_re_init)
{
	if (!bp->bnapi || !BNXT_PF(bp) || BNXT_CHIP_P4(bp))
		return;

	/* Keep new MPC batches out, then let any batch that is already open
	 * run to completion before the rings become unavailable, so that a
	 * batch is never interrupted part way through. This has to happen
	 * before mpc_ring_lock is taken, an open batch needs that lock to
	 * post its remaining commands.
	 */
	atomic_set(&bp->mpc_regen_pending, 1);
	/* Make sure tfc_mpc_batch_start() sees the pending */
	smp_mb__after_atomic();
	while (atomic_read(&bp->mpc_batch_active) > 0)
		msleep(20);

	mutex_lock(&bp->mpc_ring_lock);
	__bnxt_free_persistent_mpc_rings(bp, irq_re_init);
	mutex_unlock(&bp->mpc_ring_lock);

	atomic_set(&bp->mpc_regen_pending, 0);
}

int bnxt_create_persistent_mpc_rings(struct bnxt *bp, bool irq_re_init)
{
	struct bnxt_cp_ring_info *cpr2;
	struct bnxt_napi *bnapi;
	int rc;

	mutex_lock(&bp->mpc_ring_lock);

	if (!(bp->flags & BNXT_FLAG_CHIP_P7_PLUS) || !bp->bnapi) {
		rc = -EOPNOTSUPP;
		goto unlock;
	}

	if (!BNXT_MPC_CRYPTO_CAPABLE(bp) && !BNXT_MPC_CFA_CAPABLE(bp)) {
		rc = -EOPNOTSUPP;
		goto unlock;
	}

	bnapi = bp->bnapi[BNXT_NQ0_NAPI_IDX];
	if (!BNXT_NQ0_NAPI(bnapi)) {
		rc = -EAGAIN;
		goto unlock;
	}

	if (BNXT_MPC0_NAPI(bnapi)) {
		rc = 0;
		goto unlock;
	}

	rc = bnxt_alloc_mpc_subset_rings(bp);
	if (rc)
		goto free_subset_mpcs;

	cpr2 = &bp->mpc_info->mpc_cq0;
	cpr2->bnapi = bnapi;
	rc = bnxt_alloc_one_mpc_cp_ring(bp, cpr2);
	if (rc)
		goto free_subset_mpcs;

	if (irq_re_init) {
		rc = bnxt_alloc_one_mpc_for_nq(bp, bnapi);
		if (rc)
			goto free_subset_mpcs_cq;
		rc = bnxt_alloc_one_ring_stats(bp, bnapi, true);
		if (rc)
			goto free_subset_mpcs_cq;
		rc = bnxt_hwrm_mpc0_stat_ctx_alloc(bp);
		if (rc)
			goto free_stats_mem;
	}
	cpr2->sw_stats = bnapi->cp_ring.sw_stats;
	bnxt_set_mpc_subset_cp_ring(bp, 0, cpr2);
	bnxt_init_mpc_rings(bp);
	rc = bnxt_hwrm_mpc_subset_ring_alloc(bp);
	if (rc)
		goto free_all;
	set_bit(BNXT_NAPI_FLAG_MPC0, &bnapi->flags);
	rc = 0;
	goto unlock;
free_all:
	bnxt_hwrm_mpc0_stat_ctx_free(bp);
free_stats_mem:
	bnxt_free_one_ring_stats(bp, bnapi);
free_subset_mpcs_cq:
	bnxt_free_one_mpc_cp_ring(bp, cpr2);
free_subset_mpcs:
	bnxt_free_mpc_rings(bp);
	bnxt_free_one_mpc_for_nq(bp, bnapi);
unlock:
	mutex_unlock(&bp->mpc_ring_lock);
	return rc;
}
