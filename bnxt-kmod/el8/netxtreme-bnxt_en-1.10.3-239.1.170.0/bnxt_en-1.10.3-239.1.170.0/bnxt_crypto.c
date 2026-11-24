// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2026 Broadcom Inc. */

#include <linux/stddef.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/pci.h>

#include "bnxt_compat.h"
#include "bnxt_hsi.h"
#include "bnxt.h"
#include "bnxt_hwrm.h"
#include "bnxt_mpc.h"
#include "bnxt_crypto.h"
#include "bnxt_ktls.h"
#include "bnxt_quic.h"

#ifdef CONFIG_BNXT_CRYPTO

static u32 bnxt_get_max_crypto_key_ctx(struct bnxt *bp, int key_type)
{
	u32 fw_maj = BNXT_FW_MAJ(bp);

	if (key_type == BNXT_TX_CRYPTO_KEY_TYPE)
		return (fw_maj < 233) ? BNXT_MAX_TX_CRYPTO_KEYS_PRE_233FW :
		       BNXT_MAX_TX_CRYPTO_KEYS;

	return (fw_maj < 233) ? BNXT_MAX_RX_CRYPTO_KEYS_PRE_233FW :
	       BNXT_MAX_RX_CRYPTO_KEYS;
}

/**
 * bnxt_alloc_crypto_info - Allocate and initialize crypto offload context
 * @bp: pointer to bnxt device
 * @resp: pointer to firmware capability response
 *
 * Allocates the main crypto info structure
 *
 * This function is called during device initialization when firmware
 * reports crypto offload capability. If allocation fails, crypto offload
 * will not be available but the device will still function.
 *
 * Context: Process context
 */
void bnxt_alloc_crypto_info(struct bnxt *bp,
			    struct hwrm_func_qcaps_output *resp)
{
	u16 max_keys = le16_to_cpu(resp->max_key_ctxs_alloc);
	struct bnxt_crypto_info *crypto = bp->crypto_info;
	u32 flags_ext, flags_ext2;

	if (BNXT_VF(bp))
		return;
	if (!crypto) {
		bool partition_mode = false;
		struct bnxt_kctx *kctx;
		u16 batch_sz = 0;
		int i;

		crypto = kzalloc(sizeof(*crypto), GFP_KERNEL);
		if (!crypto) {
			netdev_warn(bp->dev,
				    "Unable to allocate crypto info\n");
			return;
		}
		if (BNXT_PARTITION_CAP(resp)) {
			batch_sz = le16_to_cpu(resp->ctxs_per_partition);
			if (batch_sz && batch_sz <= BNXT_KID_BATCH_SIZE)
				partition_mode = true;
		}
		for (i = 0; i < BNXT_MAX_CRYPTO_KEY_TYPE; i++) {
			kctx = &crypto->kctx[i];
			kctx->type = i;
			kctx->max_ctx = bnxt_get_max_crypto_key_ctx(bp, i);
			INIT_LIST_HEAD(&kctx->list);
			spin_lock_init(&kctx->lock);
			atomic_set(&kctx->alloc_pending, 0);
			init_waitqueue_head(&kctx->alloc_pending_wq);
			if (partition_mode) {
				unsigned int bmap_sz;

				bmap_sz = DIV_ROUND_UP(kctx->max_ctx, batch_sz);
				kctx->partition_max = bmap_sz;
				kctx->partition_bmap = bitmap_zalloc(bmap_sz,
								GFP_KERNEL);
				/* this key context will not operate in
				 * partition mode
				 */
				if (!kctx->partition_bmap)
					partition_mode = false;
			}
		}
		crypto->partition_mode = partition_mode;
		crypto->ctxs_per_partition = batch_sz;

		bp->crypto_info = crypto;
	}
	crypto->max_key_ctxs_alloc = max_keys;

	flags_ext = le32_to_cpu(resp->flags_ext);
	flags_ext2 = le32_to_cpu(resp->flags_ext2);
	if (flags_ext & FUNC_QCAPS_RESP_FLAGS_EXT_KTLS_SUPPORTED)
		bnxt_alloc_ktls_info(bp, resp);
	else
		bnxt_free_ktls_info(bp);

	if ((flags_ext2 & FUNC_QCAPS_RESP_FLAGS_EXT2_QUIC_SUPPORTED) &&
	    BNXT_FW_MAJ(bp) >= 239)
		bnxt_alloc_quic_info(bp, resp);
	else
		bnxt_free_quic_info(bp);
}

static int bnxt_crypto_del_cmn(struct bnxt *bp, u8 type, u8 kind, u32 kid)
{
	if (kind == BNXT_CTX_KIND_CK_TX || kind == BNXT_CTX_KIND_CK_RX)
		if (bp->ktls_info)
			return bnxt_crypto_del(bp, type, kind, kid);
	if (kind == BNXT_CTX_KIND_CK_QUIC_TX ||
	    kind == BNXT_CTX_KIND_CK_QUIC_RX)
		if (bp->quic_info)
			return bnxt_quic_crypto_del(bp, type, kind, kid);
	return 0;
}

static void bnxt_crypto_del_all_kids(struct bnxt *bp, struct bnxt_kid_info *kid)
{
	int i, rc;

	for (i = 0; i < kid->count; i++) {
		if (!test_bit(i, kid->ids)) {
			rc = bnxt_crypto_del_cmn(bp, kid->type, kid->kind,
						 kid->start_id + i);
			if (!rc)
				set_bit(i, kid->ids);
		}
	}
}

void bnxt_crypto_del_all(struct bnxt *bp)
{
	struct bnxt_crypto_info *crypto = bp->crypto_info;
	struct bnxt_kid_info *kid;
	struct bnxt_kctx *kctx;
	int i;

	if (!crypto)
		return;

	/* Shutting down, no need to protect the lists. */
	for (i = 0; i < BNXT_MAX_CRYPTO_KEY_TYPE; i++) {
		kctx = &crypto->kctx[i];
		list_for_each_entry(kid, &kctx->list, list)
			bnxt_crypto_del_all_kids(bp, kid);
		kctx->epoch++;
	}
}

/**
 * bnxt_clear_crypto - Clear all crypto key contexts
 * @bp: pointer to bnxt device
 *
 * Clears all key context allocations during shutdown or firmware reset.
 * Frees all key info structures and bitmaps, and increments the epoch
 * counter to invalidate any outstanding key references.
 *
 * This function assumes serialization (called during shutdown) and does
 * not use locking.
 *
 * Context: Process context during shutdown/reset
 */
void bnxt_clear_crypto(struct bnxt *bp)
{
	struct bnxt_crypto_info *crypto = bp->crypto_info;
	struct bnxt_kid_info *kid, *tmp;
	struct bnxt_kctx *kctx;
	int i;

	if (!crypto)
		return;

	/* Shutting down or FW reset, no need to protect the lists. */
	for (i = 0; i < BNXT_MAX_CRYPTO_KEY_TYPE; i++) {
		kctx = &crypto->kctx[i];
		list_for_each_entry_safe(kid, tmp, &kctx->list, list) {
			list_del(&kid->list);
			kfree(kid);
		}
		bitmap_free(kctx->partition_bmap);
		kctx->partition_bmap = NULL;
		kctx->total_alloc = 0;
		kctx->epoch++;
	}
}

/**
 * bnxt_free_crypto_info - Free crypto offload resources
 * @bp: pointer to bnxt device
 *
 * Frees all resources associated with crypto offload
 *
 * Context: Process context during device shutdown/removal
 */
void bnxt_free_crypto_info(struct bnxt *bp)
{
	struct bnxt_crypto_info *crypto = bp->crypto_info;

	bnxt_free_ktls_info(bp);
	bnxt_free_quic_info(bp);
	if (!crypto)
		return;
	bnxt_clear_crypto(bp);
	kfree(crypto);
	bp->crypto_info = NULL;
}

/**
 * bnxt_hwrm_reserve_pf_key_ctxs - Reserve key contexts with firmware
 * @bp: pointer to bnxt device
 * @req: pointer to HWRM function config request
 *
 * Populates the firmware request with key context reservation parameters
 * for kTLS and QUIC combined. Calculates the minimum of driver
 * requirements and firmware capabilities.
 *
 * Context: Process context during device configuration
 */
void bnxt_hwrm_reserve_pf_key_ctxs(struct bnxt *bp,
				   struct hwrm_func_cfg_input *req)
{
	struct bnxt_crypto_info *crypto = bp->crypto_info;
	struct bnxt_hw_resc *hw_resc = &bp->hw_resc;
	struct bnxt_hw_crypto_resc *crypto_resc;
	u32 tx, rx;

	if (!crypto)
		return;

	crypto_resc = &hw_resc->crypto_resc;
	tx = min(BNXT_TCK(crypto).max_ctx, crypto_resc->max_tx_key_ctxs);
	rx = min(BNXT_RCK(crypto).max_ctx, crypto_resc->max_rx_key_ctxs);
	req->num_crypto_tx_key_ctxs = cpu_to_le32(tx);
	req->num_crypto_rx_key_ctxs = cpu_to_le32(rx);
	req->enables |= cpu_to_le32(FUNC_CFG_REQ_ENABLES_CRYPTO_TX_KEY_CTXS |
				    FUNC_CFG_REQ_ENABLES_CRYPTO_RX_KEY_CTXS);
}

static int __bnxt_partition_alloc(struct bnxt_kctx *kctx, u32 *id)
{
	unsigned int next, max = kctx->partition_max;

	next = find_next_zero_bit(kctx->partition_bmap, max, kctx->next);
	if (next >= max)
		next = find_first_zero_bit(kctx->partition_bmap, max);
	if (next >= max)
		return -ENOSPC;
	*id = next;
	kctx->next = next;
	return 0;
}

static int bnxt_partition_alloc(struct bnxt_kctx *kctx, u32 *id)
{
	int rc;

	do {
		rc = __bnxt_partition_alloc(kctx, id);
		if (rc)
			return rc;
	} while (test_and_set_bit(*id, kctx->partition_bmap));
	return 0;
}

static int bnxt_key_ctx_store(struct bnxt_kctx *kctx, __le32 *key_buf, u32 num,
			      bool contig, u8 kind, u32 *id)
{
	struct bnxt_kid_info *kid;
	u32 i;

	for (i = 0; i < num; ) {
		kid = kzalloc(sizeof(*kid), GFP_KERNEL);
		if (!kid)
			return -ENOMEM;
		kid->start_id = le32_to_cpu(key_buf[i]);
		kid->type = kctx->type;
		kid->kind = kind;
		if (contig)
			kid->count = num;
		else
			kid->count = 1;
		bitmap_set(kid->ids, 0, kid->count);
		if (id && !i) {
			clear_bit(0, kid->ids);
			*id = BNXT_SET_KID(kctx, kid->start_id);
		}
		spin_lock(&kctx->lock);
		list_add_tail_rcu(&kid->list, &kctx->list);
		kctx->total_alloc += kid->count;
		spin_unlock(&kctx->lock);
		i += kid->count;
	}
	return 0;
}

static u8 bnxt_key_ctx_kind_to_type(struct bnxt *bp, u8 kind)
{
	switch (kind) {
	case BNXT_CTX_KIND_CK_TX:
		return FUNC_KEY_CTX_ALLOC_REQ_KEY_CTX_TYPE_TX;
	case BNXT_CTX_KIND_CK_RX:
		return FUNC_KEY_CTX_ALLOC_REQ_KEY_CTX_TYPE_RX;
	case BNXT_CTX_KIND_CK_QUIC_TX:
		return FUNC_KEY_CTX_ALLOC_REQ_KEY_CTX_TYPE_QUIC_TX;
	case BNXT_CTX_KIND_CK_QUIC_RX:
		return FUNC_KEY_CTX_ALLOC_REQ_KEY_CTX_TYPE_QUIC_RX;
	default:
		netdev_warn(bp->dev, "Invalid key ctx kind %u\n", kind);
		return BNXT_INVALID_KEY_CTX_TYPE;
	}
}

int bnxt_hwrm_key_ctx_alloc(struct bnxt *bp, struct bnxt_kctx *kctx, u8 kind,
			    u32 num, u32 *id)
{
	struct bnxt_crypto_info *crypto = bp->crypto_info;
	struct hwrm_func_key_ctx_alloc_output *resp;
	struct hwrm_func_key_ctx_alloc_input *req;
	dma_addr_t mapping;
	int pending_count;
	__le32 *key_buf;
	bool contig;
	u8 type;
	int rc;

	type = bnxt_key_ctx_kind_to_type(bp, kind);
	if (!crypto || type == BNXT_INVALID_KEY_CTX_TYPE)
		return -EINVAL;

	num = min_t(u32, num, crypto->max_key_ctxs_alloc);
	rc = hwrm_req_init(bp, req, HWRM_FUNC_KEY_CTX_ALLOC);
	if (rc)
		return rc;

	if (crypto->partition_mode) {
		u32 partition_id;

		num = crypto->ctxs_per_partition;
		rc = bnxt_partition_alloc(kctx, &partition_id);
		if (rc)
			goto key_alloc_exit;
		req->partition_start_xid = cpu_to_le32(partition_id * num);
	} else {
		key_buf = hwrm_req_dma_slice(bp, req, num * 4, &mapping);
		if (!key_buf) {
			rc = -ENOMEM;
			goto key_alloc_exit;
		}
		req->dma_bufr_size_bytes = cpu_to_le32(num * 4);
		req->host_dma_addr = cpu_to_le64(mapping);
	}
	resp = hwrm_req_hold(bp, req);

	req->key_ctx_type = type;
	req->num_key_ctxs = cpu_to_le16(num);

	pending_count = atomic_inc_return(&kctx->alloc_pending);
	rc = hwrm_req_send(bp, req);
	atomic_dec(&kctx->alloc_pending);
	if (rc)
		goto key_alloc_exit_wake;

	num = le16_to_cpu(resp->num_key_ctxs_allocated);
	contig =
		resp->flags & FUNC_KEY_CTX_ALLOC_RESP_FLAGS_KEY_CTXS_CONTIGUOUS;
	if (crypto->partition_mode)
		key_buf = &resp->partition_start_xid;
	rc = bnxt_key_ctx_store(kctx, key_buf, num, contig, kind, id);

key_alloc_exit_wake:
	if (pending_count >= BNXT_KCTX_ALLOC_PENDING_MAX)
		wake_up_all(&kctx->alloc_pending_wq);
key_alloc_exit:
	hwrm_req_drop(bp, req);
	return rc;
}

bool bnxt_kid_valid(struct bnxt_kctx *kctx, u32 id)
{
	struct bnxt_kid_info *kid;
	bool valid = false;
	u32 epoch;

	epoch = BNXT_KID_EPOCH(id);
	if (epoch != kctx->epoch)
		return false;

	id = BNXT_KID_HW(id);
	rcu_read_lock();
	list_for_each_entry_rcu(kid, &kctx->list, list) {
		if (id >= kid->start_id && id < kid->start_id + kid->count) {
			if (!test_bit(id - kid->start_id, kid->ids)) {
				valid = true;
				break;
			}
		}
	}
	rcu_read_unlock();
	return valid;
}

static int bnxt_alloc_one_kctx(struct bnxt_kctx *kctx, u8 kind, u32 *id)
{
	struct bnxt_kid_info *kid;
	int rc = -ENOMEM;

	rcu_read_lock();
	list_for_each_entry_rcu(kid, &kctx->list, list) {
		u32 idx = 0;

		if (kid->kind != kind)
			continue;
		do {
			idx = find_next_bit(kid->ids, kid->count, idx);
			if (idx >= kid->count)
				break;
			if (test_and_clear_bit(idx, kid->ids)) {
				*id = BNXT_SET_KID(kctx, kid->start_id + idx);
				rc = 0;
				goto alloc_done;
			}
		} while (1);
	}

alloc_done:
	rcu_read_unlock();
	return rc;
}

void bnxt_free_one_kctx(struct bnxt_kctx *kctx, u32 id)
{
	struct bnxt_kid_info *kid;

	id = BNXT_KID_HW(id);
	rcu_read_lock();
	list_for_each_entry_rcu(kid, &kctx->list, list) {
		if (id >= kid->start_id && id < kid->start_id + kid->count) {
			set_bit(id - kid->start_id, kid->ids);
			break;
		}
	}
	rcu_read_unlock();
}

#define BNXT_KCTX_ALLOC_RETRY_MAX	3

int bnxt_key_ctx_alloc_one(struct bnxt *bp, struct bnxt_kctx *kctx, u8 kind,
			   u32 *id)
{
	int rc, retry = 0;

	while (retry++ < BNXT_KCTX_ALLOC_RETRY_MAX) {
		rc = bnxt_alloc_one_kctx(kctx, kind, id);
		if (!rc)
			return 0;

		if ((kctx->total_alloc + BNXT_KID_BATCH_SIZE) > kctx->max_ctx)
			return -ENOSPC;

		if (!BNXT_KCTX_ALLOC_OK(kctx)) {
			wait_event(kctx->alloc_pending_wq,
				   BNXT_KCTX_ALLOC_OK(kctx));
			continue;
		}
		rc = bnxt_hwrm_key_ctx_alloc(bp, kctx, kind,
					     BNXT_KID_BATCH_SIZE, id);
		if (!rc)
			return 0;
	}
	return -EAGAIN;
}

static int bnxt_set_partition_mode(struct bnxt *bp)
{
	struct hwrm_func_cfg_input *req;
	int rc;

	rc = bnxt_hwrm_func_cfg_short_req_init(bp, &req);
	if (rc)
		return rc;
	req->fid = cpu_to_le16(0xffff);
	req->enables2 = cpu_to_le32(FUNC_CFG_REQ_ENABLES2_XID_PARTITION_CFG);
	req->xid_partition_cfg =
		cpu_to_le16(FUNC_CFG_REQ_XID_PARTITION_CFG_TX_CK |
			    FUNC_CFG_REQ_XID_PARTITION_CFG_RX_CK);
	return hwrm_req_send(bp, req);
}

int bnxt_crypto_init(struct bnxt *bp)
{
	struct bnxt_crypto_info *crypto = bp->crypto_info;
	struct bnxt_hw_resc *hw_resc = &bp->hw_resc;
	struct bnxt_hw_crypto_resc *crypto_resc;
	int rc;

	if (!crypto)
		return 0;

	crypto_resc = &hw_resc->crypto_resc;
	BNXT_TCK(crypto).max_ctx = crypto_resc->resv_tx_key_ctxs;
	BNXT_RCK(crypto).max_ctx = crypto_resc->resv_rx_key_ctxs;

	if (!BNXT_TCK(crypto).max_ctx && !BNXT_RCK(crypto).max_ctx) {
		bnxt_free_crypto_info(bp);
		return -ENODEV;
	}

	if (crypto->partition_mode) {
		rc = bnxt_set_partition_mode(bp);
		if (rc)
			crypto->partition_mode = false;
	}

	if (bnxt_ktls_init(bp))
		bnxt_free_ktls_info(bp);
	if (bnxt_quic_init(bp))
		bnxt_free_quic_info(bp);
	return 0;
}
#endif
