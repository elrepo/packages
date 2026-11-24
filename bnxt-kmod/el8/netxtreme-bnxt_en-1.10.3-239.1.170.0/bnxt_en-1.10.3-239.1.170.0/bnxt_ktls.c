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
#include <linux/bitmap.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/hashtable.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/tcp.h>
#include <linux/skbuff.h>
#ifdef HAVE_SKBUFF_REF
#include <linux/skbuff_ref.h>
#endif
#include <net/inet_hashtables.h>
#include <net/inet6_hashtables.h>
#ifdef HAVE_KTLS
#include <net/tls.h>
#endif

#include "bnxt_compat.h"
#include "bnxt_hsi.h"
#include "bnxt.h"
#include "bnxt_hwrm.h"
#include "bnxt_mpc.h"
#include "bnxt_crypto.h"
#include "bnxt_ktls.h"
#include "bnxt_quic.h"
#include "tfc.h"
#include "tfc_util.h"
#include "bnxt_tf_ulp.h"
#include "ulp_nic_flow.h"
#include "ulp_generic_flow_offload.h"

#if ((defined(HAVE_KTLS) || defined(HAVE_BNXT_QUIC)) && \
     IS_ENABLED(CONFIG_TLS_DEVICE) && (LINUX_VERSION_CODE >= KERNEL_VERSION(5,0,0)))

/**
 * bnxt_alloc_ktls_info - Allocate and initialize kTLS offload context
 * @bp: pointer to bnxt device
 * @resp: pointer to firmware capability response
 *
 * Allocates the main kTLS info structure and initializes:
 * - Hash table for filter tracking
 * - Atomic counters for statistics
 * - Filter management structures
 *
 * This function is called during device initialization when firmware
 * reports kTLS offload capability. If allocation fails, kTLS offload
 * will not be available but the device will still function.
 *
 * Context: Process context during device probe/initialization
 */
void bnxt_alloc_ktls_info(struct bnxt *bp, struct hwrm_func_qcaps_output *resp)
{
	struct bnxt_tls_info *ktls = bp->ktls_info;

	if (BNXT_VF(bp))
		return;
	if (!ktls) {
		ktls = kzalloc(sizeof(*ktls), GFP_KERNEL);
		if (!ktls)
			return;

		ktls->counters = kzalloc(sizeof(atomic64_t) * BNXT_KTLS_MAX_COUNTERS,
					 GFP_KERNEL);
		if (!ktls->counters) {
			kfree(ktls);
			return;
		}

		hash_init(ktls->filter_tbl);
		spin_lock_init(&ktls->filter_lock);
		atomic_set(&ktls->filter_pending, 0);

		atomic_set(&ktls->pending, 0);

		bp->ktls_info = ktls;
	}
	ktls->max_filter = le16_to_cpu(resp->max_crypto_rx_flow_filters);
	if (!ktls->max_filter)
		ktls->max_filter = BNXT_MAX_TLS_FILTER;
}

static u32 bnxt_get_tls_max_filter(struct bnxt *bp,
				   enum bnxt_crypto_l4_type type)
{
	struct bnxt_tls_info *tls = (type == BNXT_CRYPTO_L4_TYPE_KTLS ?
				     bp->ktls_info : bp->quic_info);

	if (bnxt_ulp_bp_ptr2_cntxt_get(bp))
		return tls->max_filter_tf;
	return tls->max_filter;
}

/**
 * bnxt_clear_cfa_tls_filters_tbl - Clear all CFA TLS filter entries
 * @bp: pointer to bnxt device
 *
 * Clears all filter entries from the hash table and frees them via
 * kfree_rcu(). This function is called during device shutdown or reset.
 *
 * Filters are freed asynchronously via RCU to allow concurrent readers
 * to complete safely. The actual memory will be freed after an RCU
 * grace period.
 *
 * Context: Process context during shutdown/reset
 */
void bnxt_clear_cfa_tls_filters_tbl(struct bnxt *bp)
{
	struct bnxt_tls_info *ktls = bp->ktls_info;
	struct bnxt_kfltr_info *kfltr;
	struct hlist_node *tmp_node;
	int bkt;

	if (!ktls)
		return;

	spin_lock(&ktls->filter_lock);
	hash_for_each_safe(ktls->filter_tbl, bkt, tmp_node, kfltr, hash) {
		hash_del_rcu(&kfltr->hash);
		kfree_rcu(kfltr, rcu);
	}
	ktls->filter_count = 0;
	spin_unlock(&ktls->filter_lock);
}

/**
 * bnxt_free_ktls_info - Free kTLS offload resources
 * @bp: pointer to bnxt device
 *
 * Frees all resources associated with kTLS offload:
 * - Key context structures and bitmaps
 * - Filter hash table entries
 * - Crypto command context cache
 * - Atomic counters
 * - Main kTLS info structure
 *
 * Context: Process context during device shutdown/removal
 */
void bnxt_free_ktls_info(struct bnxt *bp)
{
	struct bnxt_tls_info *ktls = bp->ktls_info;

	if (!ktls)
		return;

	bnxt_clear_cfa_tls_filters_tbl(bp);
	kmem_cache_destroy(ktls->mpc_cache);
	kfree(ktls->counters);
	kfree(ktls);
	bp->ktls_info = NULL;
}

#define BNXT_TLS_FLTR_FLAGS					\
	(CFA_TLS_FILTER_ALLOC_REQ_ENABLES_L2_FILTER_ID |	\
	 CFA_TLS_FILTER_ALLOC_REQ_ENABLES_ETHERTYPE |		\
	 CFA_TLS_FILTER_ALLOC_REQ_ENABLES_IPADDR_TYPE |		\
	 CFA_TLS_FILTER_ALLOC_REQ_ENABLES_SRC_IPADDR |		\
	 CFA_TLS_FILTER_ALLOC_REQ_ENABLES_DST_IPADDR |		\
	 CFA_TLS_FILTER_ALLOC_REQ_ENABLES_IP_PROTOCOL |		\
	 CFA_TLS_FILTER_ALLOC_REQ_ENABLES_SRC_PORT |		\
	 CFA_TLS_FILTER_ALLOC_REQ_ENABLES_DST_PORT |		\
	 CFA_TLS_FILTER_ALLOC_REQ_ENABLES_KID |			\
	 CFA_TLS_FILTER_ALLOC_REQ_ENABLES_DST_ID)

int bnxt_hwrm_cfa_tls_filter_alloc(struct bnxt *bp, struct sock *sk,
				   u32 kid, enum bnxt_crypto_l4_type type,
				   u64 conn_id)
{
	struct hwrm_cfa_tls_filter_alloc_output *resp;
	struct hwrm_cfa_tls_filter_alloc_input *req;
	struct bnxt_tls_info *tls = (type == BNXT_CRYPTO_L4_TYPE_KTLS ?
				     bp->ktls_info : bp->quic_info);
	struct inet_sock *inet = inet_sk(sk);
	struct bnxt_ulp_context *ulp_ctx;
	struct bnxt_l2_filter *l2_fltr;
	struct bnxt_kfltr_info *kfltr;
	int rc;

	ulp_ctx = bnxt_ulp_bp_ptr2_cntxt_get(bp);
	if (BNXT_TF_FLAG_IN_USE(bp) && !ulp_ctx) {
		netdev_dbg(bp->dev, "%s: ULP is not inited in TF mode\n", __func__);
		return -ENODEV;
	}

	kfltr = kzalloc(sizeof(*kfltr), GFP_KERNEL);
	if (!kfltr)
		return -ENOMEM;

	if (ulp_ctx && BNXT_CHIP_P7(bp) && BNXT_PF(bp)) {
		rc = bnxt_tf_tls_flow_create(bp, sk, &kfltr->filter_id, NULL,
					     BNXT_KID_HW(kid), type);
		if (rc) {
			kfree(kfltr);
		} else {
			kfltr->kid = kid;
			spin_lock(&tls->filter_lock);
			tls->filter_count++;
			hash_add_rcu(tls->filter_tbl, &kfltr->hash, kid);
			spin_unlock(&tls->filter_lock);
		}
		return rc;
	}

	rc = hwrm_req_init(bp, req, HWRM_CFA_TLS_FILTER_ALLOC);
	if (rc) {
		kfree(kfltr);
		return rc;
	}

	req->enables = cpu_to_le32(BNXT_TLS_FLTR_FLAGS);

	l2_fltr = bp->vnic_info[BNXT_VNIC_DEFAULT].l2_filters[0];
	req->l2_filter_id = l2_fltr->base.l2_filter_id;
	req->dst_id = cpu_to_le16(bp->vnic_info[BNXT_VNIC_DEFAULT].fw_vnic_id);
	req->kid = cpu_to_le32(BNXT_KID_HW(kid));

	if (type == BNXT_CRYPTO_L4_TYPE_QUIC) {
		req->quic_dst_connect_id = cpu_to_le64(conn_id);
		req->enables |= cpu_to_le32(CFA_TLS_FILTER_ALLOC_REQ_ENABLES_QUIC_DST_CONNECT_ID);
	}
	if (sk->sk_protocol == IPPROTO_TCP)
		req->ip_protocol = CFA_TLS_FILTER_ALLOC_REQ_IP_PROTOCOL_TCP;
	else
		req->ip_protocol = CFA_TLS_FILTER_ALLOC_REQ_IP_PROTOCOL_UDP;
	req->src_port = inet->inet_dport;
	req->dst_port = inet->inet_sport;

	switch (sk->sk_family) {
	case AF_INET:
	default:
		req->ethertype = htons(ETH_P_IP);
		req->ip_addr_type = CFA_TLS_FILTER_ALLOC_REQ_IP_ADDR_TYPE_IPV4;
		req->src_ipaddr[0] = inet->inet_daddr;
		req->dst_ipaddr[0] = inet->inet_saddr;
		break;
	case AF_INET6: {
		struct ipv6_pinfo *inet6 = inet6_sk(sk);

		req->ethertype = htons(ETH_P_IPV6);
		req->ip_addr_type = CFA_TLS_FILTER_ALLOC_REQ_IP_ADDR_TYPE_IPV6;
		memcpy(req->src_ipaddr, &sk->sk_v6_daddr, sizeof(req->src_ipaddr));
		memcpy(req->dst_ipaddr, &inet6->saddr, sizeof(req->dst_ipaddr));
		break;
	}
	}
	resp = hwrm_req_hold(bp, req);
	rc = hwrm_req_send(bp, req);
	if (rc) {
		if (rc == -EEXIST)
			netdev_warn(bp->dev,
				    "TLS filter already exists, offload failed\n");
		else
			netdev_warn(bp->dev,
				    "TLS filter alloc failed, rc %d, filter count %d, max %d\n",
				    rc, tls->filter_count,
				    bnxt_get_tls_max_filter(bp, type));
		kfree(kfltr);
	} else {
		kfltr->kid = kid;
		kfltr->filter_id = resp->tls_filter_id;
		spin_lock(&tls->filter_lock);
		tls->filter_count++;
		hash_add_rcu(tls->filter_tbl, &kfltr->hash, kid);
		spin_unlock(&tls->filter_lock);
	}
	hwrm_req_drop(bp, req);
	return rc;
}

int bnxt_hwrm_cfa_tls_filter_free(struct bnxt *bp, u32 kid,
				  enum bnxt_crypto_l4_type type)
{
	struct bnxt_tls_info *tls = (type == BNXT_CRYPTO_L4_TYPE_KTLS ?
				     bp->ktls_info : bp->quic_info);
	struct hwrm_cfa_tls_filter_free_input *req;
	struct bnxt_ulp_context *ulp_ctx;
	struct bnxt_kfltr_info *kfltr;
	bool found = false;
	int rc;

	ulp_ctx = bnxt_ulp_bp_ptr2_cntxt_get(bp);
	if (BNXT_TF_FLAG_IN_USE(bp) && !ulp_ctx) {
		netdev_dbg(bp->dev, "%s: ULP is not inited in TF mode\n", __func__);
		return -ENODEV;
	}

	rcu_read_lock();
	hash_for_each_possible_rcu(tls->filter_tbl, kfltr, hash, kid) {
		if (kfltr->kid == kid) {
			found = true;
			break;
		}
	}
	rcu_read_unlock();
	if (!found)
		return -ENOENT;

	if (ulp_ctx && BNXT_CHIP_P7(bp) && BNXT_PF(bp)) {
		rc = bnxt_tf_tls_flow_del(bp, kfltr->filter_id);
		spin_lock(&tls->filter_lock);
		tls->filter_count--;
		hash_del_rcu(&kfltr->hash);
		spin_unlock(&tls->filter_lock);
		kfree_rcu(kfltr, rcu);
		return rc;
	}

	rc = hwrm_req_init(bp, req, HWRM_CFA_TLS_FILTER_FREE);
	if (rc)
		return rc;

	req->tls_filter_id = kfltr->filter_id;
	rc = hwrm_req_send(bp, req);

	spin_lock(&tls->filter_lock);
	tls->filter_count--;
	hash_del_rcu(&kfltr->hash);
	spin_unlock(&tls->filter_lock);
	kfree_rcu(kfltr, rcu);
	return rc;
}

#define BNXT_XMIT_CRYPTO_RETRY_MAX	10
#define BNXT_XMIT_CRYPTO_MIN_TMO	100
#define BNXT_XMIT_CRYPTO_MAX_TMO	150

static void bnxt_ktls_free_crypto_ctx(struct bnxt *bp, struct bnxt_crypto_cmd_ctx *ctx)
{
	if (ctx->is_quic)
		kmem_cache_free(bp->quic_info->mpc_cache, ctx);
	else
		kmem_cache_free(bp->ktls_info->mpc_cache, ctx);
}

int bnxt_xmit_crypto_cmd(struct bnxt *bp, struct bnxt_tx_ring_info *txr,
			 void *cmd, uint len, uint tmo, struct bnxt_tls_info *tls)
{
	struct bnxt_crypto_cmd_ctx *ctx = NULL;
	unsigned long tmo_left, handle = 0;
	int rc, retry = 0;

	if (tmo) {
		u32 kid = CE_CMD_KID(cmd);

		ctx = kmem_cache_alloc(tls->mpc_cache, GFP_KERNEL);
		if (!ctx)
			return -ENOMEM;
		init_completion(&ctx->cmp);
		handle = (unsigned long)ctx;
		ctx->kid = kid;
		ctx->client = txr->tx_ring_struct.mpc_chnl_type;
		ctx->status = 0;
		ctx->is_quic = (tls == bp->quic_info);
		/* One reference for this caller, one for the handle stored in
		 * the TX buf ring.  The latter is dropped by
		 * bnxt_ktls_mpc_cmp() when the command is completed normally
		 * or after timeout.
		 */
		refcount_set(&ctx->refcnt, 2);
		retry = BNXT_XMIT_CRYPTO_RETRY_MAX;
		might_sleep();
	}
	do {
		spin_lock_bh(&txr->tx_lock);
		rc = bnxt_start_xmit_mpc(bp, txr, cmd, len, handle);
		spin_unlock_bh(&txr->tx_lock);
		if (rc == -EBUSY && tmo && retry)
			usleep_range(BNXT_XMIT_CRYPTO_MIN_TMO,
				     BNXT_XMIT_CRYPTO_MAX_TMO);
		else
			break;
	} while (retry--);
	if (rc || !tmo) {
		/* We can free the ctx right away if bnxt_start_xmit_mpc()
		 * failed.  We will never get a completion.
		 */
		if (ctx)
			bnxt_ktls_free_crypto_ctx(bp, ctx);
		if (rc)
			netdev_warn(bp->dev,
				    "MPC transmit failed, ring idx %d, op 0x%x, kid 0x%x, rc %d\n",
				    txr->bnapi->index, CE_CMD_OP(cmd), CE_CMD_KID(cmd), rc);
		return rc;
	}

	tmo_left = wait_for_completion_timeout(&ctx->cmp, msecs_to_jiffies(tmo));
	if (!tmo_left) {
		netdev_warn(bp->dev, "kTLS MP cmd %08x timed out\n",
			    *((u32 *)cmd));
		bnxt_mpc_timeout(bp, txr);
		rc = -ETIMEDOUT;
		goto xmit_done;
	}
	if (ctx->status == BNXT_CMD_CTX_COMPLETED &&
	    CE_CMPL_STATUS(&ctx->ce_cmp) == CE_CMPL_STATUS_OK)
		rc = 0;
	else
		rc = -EIO;
xmit_done:
	if (rc)
		netdev_warn(bp->dev,
			    "MPC transmit failed, ring idx %d, op 0x%x, kid 0x%x, status 0x%x\n",
			    txr->bnapi->index, CE_CMD_OP(cmd), CE_CMD_KID(cmd),
			    ctx->status);
	if (refcount_dec_and_test(&ctx->refcnt))
		bnxt_ktls_free_crypto_ctx(bp, ctx);
	return rc;
}

static void bnxt_copy_tls_mp_data(u8 *dst, u8 *src, int bytes)
{
	int i;

	for (i = 0; i < bytes; i++)
		dst[-i] = src[i];
}

static int bnxt_crypto_add(struct bnxt *bp,
			   enum tls_offload_ctx_dir direction,
			   struct tls_crypto_info *crypto_info, u32 tcp_seq_no,
			   u32 kid, enum bnxt_crypto_l4_type type)
{
	struct bnxt_tx_ring_info *txr;
	struct ce_add_cmd cmd = {0};
	u32 data;

	if (direction == TLS_OFFLOAD_CTX_DIR_TX) {
		txr = bnxt_select_mpc_ring(bp, BNXT_MPC_TCE_TYPE);
		cmd.ctx_kind = CE_ADD_CMD_CTX_KIND_CK_TX;
	} else {
		txr = bnxt_select_mpc_ring(bp, BNXT_MPC_RCE_TYPE);
		cmd.ctx_kind = CE_ADD_CMD_CTX_KIND_CK_RX;
	}

	data = CE_ADD_CMD_OPCODE_ADD | (BNXT_KID_HW(kid) << CE_ADD_CMD_KID_SFT);
	switch (crypto_info->cipher_type) {
	case TLS_CIPHER_AES_GCM_128: {
		struct tls12_crypto_info_aes_gcm_128 *aes;

		aes = (void *)crypto_info;
		data |= CE_ADD_CMD_ALGORITHM_AES_GCM_128;
		if (crypto_info->version == TLS_1_3_VERSION)
			data |= CE_ADD_CMD_VERSION_TLS1_3;
		memcpy(&cmd.session_key, aes->key, sizeof(aes->key));
		memcpy(&cmd.salt, aes->salt, sizeof(aes->salt));
		memcpy(&cmd.addl_iv, aes->iv, sizeof(aes->iv));
		bnxt_copy_tls_mp_data(&cmd.record_seq_num_end, aes->rec_seq,
				      sizeof(aes->rec_seq));
		break;
	}
	case TLS_CIPHER_AES_GCM_256: {
		struct tls12_crypto_info_aes_gcm_256 *aes;

		aes = (void *)crypto_info;
		data |= CE_ADD_CMD_ALGORITHM_AES_GCM_256;
		if (crypto_info->version == TLS_1_3_VERSION)
			data |= CE_ADD_CMD_VERSION_TLS1_3;
		memcpy(&cmd.session_key, aes->key, sizeof(aes->key));
		memcpy(&cmd.salt, aes->salt, sizeof(aes->salt));
		memcpy(&cmd.addl_iv, aes->iv, sizeof(aes->iv));
		bnxt_copy_tls_mp_data(&cmd.record_seq_num_end, aes->rec_seq,
				      sizeof(aes->rec_seq));
		break;
	}
	}
	cmd.ver_algo_kid_opcode = cpu_to_le32(data);
	cmd.pkt_tcp_seq_num = cpu_to_le32(tcp_seq_no);
	cmd.tls_header_tcp_seq_num = cmd.pkt_tcp_seq_num;
	return bnxt_xmit_crypto_cmd(bp, txr, &cmd, sizeof(cmd),
				    BNXT_MPC_TMO_MSECS, bp->ktls_info);
}

int bnxt_crypto_del(struct bnxt *bp, u8 type, u8 kind, u32 kid)
{
	struct bnxt_tx_ring_info *txr;
	struct ce_delete_cmd cmd = {0};
	u32 data;

	if (test_bit(BNXT_STATE_IN_FW_RESET, &bp->state) &&
	    test_bit(BNXT_STATE_FW_FATAL_COND, &bp->state))
		return 0;

	txr = bnxt_select_mpc_ring(bp, type);
	if (!txr)
		return -ENODEV;
	if (kind == BNXT_CTX_KIND_CK_TX)
		data = CE_DELETE_CMD_CTX_KIND_CK_TX;
	else if (kind == BNXT_CTX_KIND_CK_RX)
		data = CE_DELETE_CMD_CTX_KIND_CK_RX;
	else
		return -EINVAL;

	data |= CE_DELETE_CMD_OPCODE_DEL |
		(BNXT_KID_HW(kid) << CE_DELETE_CMD_KID_SFT);

	cmd.ctx_kind_kid_opcode = cpu_to_le32(data);
	return bnxt_xmit_crypto_cmd(bp, txr, &cmd, sizeof(cmd),
				    BNXT_MPC_TMO_MSECS, bp->ktls_info);
}

static bool bnxt_ktls_cipher_supported(struct bnxt *bp,
				       struct tls_crypto_info *crypto_info)
{
	u16 type = crypto_info->cipher_type;
	u16 version = crypto_info->version;

	if ((type == TLS_CIPHER_AES_GCM_128 ||
	     type == TLS_CIPHER_AES_GCM_256) &&
	    (version == TLS_1_2_VERSION ||
	     version == TLS_1_3_VERSION))
		return true;
	return false;
}

static void bnxt_set_ktls_ctx_rx(struct tls_context *tls_ctx,
				 struct bnxt_ktls_offload_ctx_rx *kctx_rx)
{
	struct bnxt_ktls_offload_ctx_rx **rx =
		__tls_driver_ctx(tls_ctx, TLS_OFFLOAD_CTX_DIR_RX);

	*rx = kctx_rx;
}

static struct bnxt_ktls_offload_ctx_rx *
bnxt_get_ktls_ctx_rx(struct tls_context *tls_ctx)
{
	struct bnxt_ktls_offload_ctx_rx **rx =
		__tls_driver_ctx(tls_ctx, TLS_OFFLOAD_CTX_DIR_RX);

	return *rx;
}

static int bnxt_ktls_dev_add(struct net_device *dev, struct sock *sk,
			     enum tls_offload_ctx_dir direction,
			     struct tls_crypto_info *crypto_info,
			     u32 start_offload_tcp_sn)
{
	struct bnxt_ktls_offload_ctx_rx *kctx_rx = NULL;
	struct bnxt_ktls_offload_ctx_tx *kctx_tx;
	struct bnxt *bp = netdev_priv(dev);
	struct bnxt_crypto_info *crypto;
	struct tls_context *tls_ctx;
	struct bnxt_tls_info *ktls;
	struct bnxt_kctx *kctx;
	u8 kind;
	u32 kid;
	int rc;

	BUILD_BUG_ON(sizeof(struct bnxt_ktls_offload_ctx_tx) >
		     TLS_DRIVER_STATE_SIZE_TX);
	BUILD_BUG_ON(sizeof(struct bnxt_ktls_offload_ctx_rx *) >
		     TLS_DRIVER_STATE_SIZE_RX);

	crypto = bp->crypto_info;
	ktls = bp->ktls_info;
	if (!crypto || !ktls)
		return -ENODEV;

	if (!bnxt_ktls_cipher_supported(bp, crypto_info)) {
		atomic64_inc(&ktls->counters[BNXT_KTLS_ERR_INVALID_CIPHER]);
		return -EOPNOTSUPP;
	}

	atomic_inc(&ktls->pending);
	/* Make sure bnxt_close_nic() sees pending before we check the
	 * BNXT_STATE_OPEN flag.
	 */
	smp_mb__after_atomic();
	if (!test_bit(BNXT_STATE_OPEN, &bp->state)) {
		atomic64_inc(&ktls->counters[BNXT_KTLS_ERR_STATE_NOT_OPEN]);
		rc = -ENODEV;
		goto exit;
	}

	tls_ctx = tls_get_ctx(sk);
	if (direction == TLS_OFFLOAD_CTX_DIR_TX) {
		kctx_tx = __tls_driver_ctx(tls_ctx, TLS_OFFLOAD_CTX_DIR_TX);
		kctx = &BNXT_TCK(crypto);
		kind = BNXT_CTX_KIND_CK_TX;
	} else {
		if (test_bit(BNXT_STATE_TF_MODE_CHANGE, &bp->state)) {
			atomic64_inc(&ktls->counters[BNXT_KTLS_ERR_DEVICE_BUSY]);
			rc = -EBUSY;
			goto exit;
		}
		atomic_inc(&ktls->filter_pending);

		/* Fast, conservative check for filter limit before proceeding.
		 *
		 * This check uses filter_count (actual allocated filters) plus
		 * filter_pending (threads attempting allocation, including this one)
		 * to determine if we're at or near the limit.
		 *
		 * The check is intentionally conservative and may reject allocation
		 * attempts before we actually reach the hardware limit:
		 * - Provides fast check before expensive allocation operations
		 * - Better to reject early than exceed hardware resource limits
		 * - filter_pending accounts for all in-flight allocation attempts
		 * - No lock contention on the fast path
		 */
		if (ktls->filter_count + atomic_read(&ktls->filter_pending) >
		    bnxt_get_tls_max_filter(bp, BNXT_CRYPTO_L4_TYPE_KTLS)) {
			atomic_dec(&ktls->filter_pending);
			atomic64_inc(&ktls->counters[BNXT_KTLS_ERR_FILTER_LIMIT]);
			rc = -ENOSPC;
			goto exit;
		}
		kctx_rx = kzalloc(sizeof(*kctx_rx), GFP_KERNEL);
		if (!kctx_rx) {
			atomic_dec(&ktls->filter_pending);
			atomic64_inc(&ktls->counters[BNXT_KTLS_ERR_NO_MEM]);
			rc = -ENOMEM;
			goto exit;
		}

		spin_lock_init(&kctx_rx->resync_lock);
		bnxt_set_ktls_ctx_rx(tls_ctx, kctx_rx);
		kctx = &BNXT_RCK(crypto);
		kind = BNXT_CTX_KIND_CK_RX;
	}
	rc = bnxt_key_ctx_alloc_one(bp, kctx, kind, &kid);
	if (rc) {
		atomic64_inc(&ktls->counters[BNXT_KTLS_ERR_KEY_CTX_ALLOC]);
		goto free_ctx_rx;
	}
	rc = bnxt_crypto_add(bp, direction, crypto_info, start_offload_tcp_sn,
			     kid, BNXT_CRYPTO_L4_TYPE_KTLS);
	if (rc) {
		atomic64_inc(&ktls->counters[BNXT_KTLS_ERR_CRYPTO_CMD]);
		goto free_kctx;
	}
	if (direction == TLS_OFFLOAD_CTX_DIR_TX) {
		kctx_tx->kid = kid;
		kctx_tx->tcp_seq_no = start_offload_tcp_sn;
		atomic64_inc(&ktls->counters[BNXT_KTLS_TX_ADD]);
	} else {
		kctx_rx->kid = kid;
		rc = bnxt_hwrm_cfa_tls_filter_alloc(bp, sk, kid,
						    BNXT_CRYPTO_L4_TYPE_KTLS,
						    0);
		if (rc) {
			int err = bnxt_crypto_del(bp, kctx->type, kind, kid);

			atomic64_inc(&ktls->counters[BNXT_KTLS_ERR_FILTER_ALLOC]);
			/* If unable to free, keep the KID */
			if (err)
				goto free_ctx_rx;
			goto free_kctx;
		}
		atomic64_inc(&ktls->counters[BNXT_KTLS_RX_ADD]);
	}
free_kctx:
	if (rc)
		bnxt_free_one_kctx(kctx, kid);
free_ctx_rx:
	if (rc)
		kfree(kctx_rx);
	if (direction == TLS_OFFLOAD_CTX_DIR_RX)
		atomic_dec(&ktls->filter_pending);
exit:
	atomic_dec(&ktls->pending);
	return rc;
}

/* Maximum number of retries when waiting for device to become ready.
 * Matches QUIC_RETRY_MAX for consistency across kTLS and QUIC paths.
 * At 100ms per retry, this allows up to 10 seconds for device to open.
 */
#define KTLS_RETRY_MAX	100

static void bnxt_ktls_dev_del(struct net_device *dev,
			      struct tls_context *tls_ctx,
			      enum tls_offload_ctx_dir direction)
{
	struct bnxt_ktls_offload_ctx_tx *kctx_tx;
	struct bnxt_ktls_offload_ctx_rx *kctx_rx;
	struct bnxt *bp = netdev_priv(dev);
	struct bnxt_crypto_info *crypto;
	struct bnxt_tls_info *ktls;
	struct bnxt_kctx *kctx;
	int retry_cnt = 0;
	u8 kind;
	u32 kid;

	crypto = bp->crypto_info;
	ktls = bp->ktls_info;
	if (!crypto || !ktls)
		return;
retry:
	while (!test_bit(BNXT_STATE_OPEN, &bp->state)) {
		if (!netif_running(dev))
			return;
		/* Prevent infinite loop if device is down but netif_running()
		 * returns true. Match QUIC's retry limit for consistency.
		 */
		if (retry_cnt > KTLS_RETRY_MAX) {
			atomic64_inc(&ktls->counters[BNXT_KTLS_ERR_RETRY_EXCEEDED]);
			netdev_warn(dev, "%s retry max %d exceeded, state %lx\n",
				    __func__, retry_cnt, bp->state);
			return;
		}
		retry_cnt++;
		msleep(100);
	}
	atomic_inc(&ktls->pending);
	/* Make sure bnxt_close_nic() sees pending before we check the
	 * BNXT_STATE_OPEN flag.
	 */
	smp_mb__after_atomic();
	if (!test_bit(BNXT_STATE_OPEN, &bp->state)) {
		atomic_dec(&ktls->pending);
		goto retry;
	}

	if (direction == TLS_OFFLOAD_CTX_DIR_TX) {
		kctx_tx = __tls_driver_ctx(tls_ctx, TLS_OFFLOAD_CTX_DIR_TX);
		kid = kctx_tx->kid;
		kctx = &BNXT_TCK(crypto);
		kind = BNXT_CTX_KIND_CK_TX;
		atomic64_inc(&ktls->counters[BNXT_KTLS_TX_DEL]);
	} else {
		kctx_rx = bnxt_get_ktls_ctx_rx(tls_ctx);
		bnxt_set_ktls_ctx_rx(tls_ctx, NULL);
		kid = kctx_rx->kid;
		kctx = &BNXT_RCK(crypto);
		kind = BNXT_CTX_KIND_CK_RX;
		bnxt_hwrm_cfa_tls_filter_free(bp, kid, BNXT_CRYPTO_L4_TYPE_KTLS);
		/* synchronize with NAPI */
		synchronize_net();
		kfree(kctx_rx);
		atomic64_inc(&ktls->counters[BNXT_KTLS_RX_DEL]);
	}
	if (bnxt_kid_valid(kctx, kid) &&
	    !bnxt_crypto_del(bp, kctx->type, kind, kid))
		bnxt_free_one_kctx(kctx, kid);
	atomic_dec(&ktls->pending);
}

static int
bnxt_ktls_dev_resync(struct net_device *dev, struct sock *sk, u32 seq,
		     u8 *rcd_sn, enum tls_offload_ctx_dir direction)
{
	struct bnxt_ktls_offload_ctx_rx *kctx_rx;
	struct ce_resync_resp_ack_cmd cmd = {0};
	struct bnxt *bp = netdev_priv(dev);
	struct bnxt_tx_ring_info *txr;
	struct bnxt_tls_info *ktls;
	struct tls_context *tls_ctx;
	u32 data;
	int rc;

	if (direction == TLS_OFFLOAD_CTX_DIR_TX)
		return -EOPNOTSUPP;

	ktls = bp->ktls_info;
	atomic_inc(&ktls->pending);
	/* Make sure bnxt_close_nic() sees pending before we check the
	 * BNXT_STATE_OPEN flag.
	 */
	smp_mb__after_atomic();
	if (!test_bit(BNXT_STATE_OPEN, &bp->state)) {
		atomic_dec(&ktls->pending);
		return -ENODEV;
	}
	txr = bnxt_select_mpc_ring(bp, BNXT_MPC_RCE_TYPE);
	tls_ctx = tls_get_ctx(sk);
	kctx_rx = bnxt_get_ktls_ctx_rx(tls_ctx);
	if (!kctx_rx)
		return -ENODEV;
	spin_lock_bh(&kctx_rx->resync_lock);
	if (!kctx_rx->resync_pending || seq != kctx_rx->resync_tcp_seq_no) {
		spin_unlock_bh(&kctx_rx->resync_lock);
		atomic64_inc(&ktls->counters[BNXT_KTLS_RX_RESYNC_DISCARD]);
		atomic_dec(&ktls->pending);
		return 0;
	}
	kctx_rx->resync_pending = false;
	spin_unlock_bh(&kctx_rx->resync_lock);
	data = CE_RESYNC_RESP_ACK_CMD_OPCODE_RESYNC |
	       (BNXT_KID_HW(kctx_rx->kid) << CE_RESYNC_RESP_ACK_CMD_KID_SFT);
	cmd.resync_status_kid_opcode = cpu_to_le32(data);
	cmd.resync_record_tcp_seq_num = cpu_to_le32(seq - TLS_HEADER_SIZE + 1);
	bnxt_copy_tls_mp_data(&cmd.resync_record_seq_num_end, rcd_sn,
			      sizeof(cmd.resync_record_seq_num));
	rc = bnxt_xmit_crypto_cmd(bp, txr, &cmd, sizeof(cmd), 0, ktls);
	atomic64_inc(&ktls->counters[BNXT_KTLS_RX_RESYNC_ACK]);
	atomic_dec(&ktls->pending);
	return rc;
}

static const struct tlsdev_ops bnxt_ktls_ops = {
	.tls_dev_add = bnxt_ktls_dev_add,
	.tls_dev_del = bnxt_ktls_dev_del,
	.tls_dev_resync = bnxt_ktls_dev_resync,
};

int bnxt_ktls_init(struct bnxt *bp)
{
	struct bnxt_crypto_info *crypto = bp->crypto_info;
	struct bnxt_tls_info *ktls = bp->ktls_info;
	struct net_device *dev = bp->dev;
	char name[32];
	int rc;

	if (!crypto || !ktls)
		return 0;

	rc = bnxt_hwrm_key_ctx_alloc(bp, &BNXT_TCK(crypto), BNXT_CTX_KIND_CK_TX,
				     BNXT_KID_BATCH_SIZE, NULL);
	if (rc)
		return rc;

	rc = bnxt_hwrm_key_ctx_alloc(bp, &BNXT_RCK(crypto), BNXT_CTX_KIND_CK_RX,
				     BNXT_KID_BATCH_SIZE, NULL);
	if (rc)
		return rc;

	snprintf(name, sizeof(name), "bnxt_ktls-%s", dev_name(&bp->pdev->dev));
	ktls->mpc_cache = kmem_cache_create(name,
					    sizeof(struct bnxt_crypto_cmd_ctx),
					    0, SLAB_HWCACHE_ALIGN, NULL);
	if (!ktls->mpc_cache)
		return -ENOMEM;

	dev->tlsdev_ops = &bnxt_ktls_ops;
	dev->hw_features |= NETIF_F_HW_TLS_TX | NETIF_F_HW_TLS_RX;
	dev->features |= NETIF_F_HW_TLS_TX | NETIF_F_HW_TLS_RX;
	return 0;
}

void bnxt_ktls_mpc_cmp(struct bnxt *bp, u32 client, unsigned long handle,
		       struct bnxt_cmpl_entry cmpl[], u32 entries)
{
	struct bnxt_crypto_cmd_ctx *ctx;
	struct ce_cmpl *cmp = NULL;
	u32 len, kid;

	if (likely(cmpl && entries > 0))
		cmp = cmpl[0].cmpl;
	if (!handle || handle == BNXT_INV_MPC_HDL)
		return;

	ctx = (void *)handle;
	ctx->status = BNXT_CMD_CTX_COMPLETED;
	if (unlikely(!cmpl)) {
		ctx->status |= BNXT_CMD_CTX_RESET;
		goto cmp_done;
	}
	if (unlikely(entries != 1)) {
		netdev_warn(bp->dev, "Invalid entries %d with handle %lx cmpl %08x in %s()\n",
			    entries, handle, cmp ? *(u32 *)cmp : 0, __func__);
		ctx->status |= BNXT_CMD_CTX_ERROR;
		goto cmp_done;
	}
	kid = CE_CMPL_KID(cmp);
	if (ctx->kid != kid || ctx->client != client) {
		netdev_warn(bp->dev,
			    "Invalid CE cmpl 0x%08x with entries %d for client %d with status 0x%x, expected kid 0x%x and client %d\n",
			    *(u32 *)cmp, entries, client, ctx->status, ctx->kid,
			    ctx->client);
		ctx->status |= BNXT_CMD_CTX_ERROR;
	}
	len = min_t(u32, cmpl[0].len, sizeof(ctx->ce_cmp));
	memcpy(&ctx->ce_cmp, cmpl[0].cmpl, len);
cmp_done:
	complete(&ctx->cmp);
	if (refcount_dec_and_test(&ctx->refcnt))
		bnxt_ktls_free_crypto_ctx(bp, ctx);
}

static void bnxt_ktls_pre_xmit(struct bnxt *bp, struct bnxt_tx_ring_info *txr,
			       u32 kid, struct crypto_prefix_cmd *pre_cmd)
{
	struct bnxt_sw_tx_bd *tx_buf;
	struct tx_bd_presync *psbd;
	u32 bd_space, space;
	u8 *pcmd;
	u16 prod;

	prod = txr->tx_prod;
	tx_buf = &txr->tx_buf_ring[RING_TX(bp, prod)];

	psbd = (void *)&txr->tx_desc_ring[TX_RING(bp, prod)][TX_IDX(prod)];
	psbd->tx_bd_len_flags_type = CRYPTO_PRESYNC_BD_CMD;
	psbd->tx_bd_kid = cpu_to_le32(BNXT_KID_HW(kid));
	psbd->tx_bd_opaque =
		SET_TX_OPAQUE(bp, txr, prod, CRYPTO_PREFIX_CMD_BDS + 1);

	prod = NEXT_TX(prod);
	pcmd = (void *)&txr->tx_desc_ring[TX_RING(bp, prod)][TX_IDX(prod)];
	bd_space = TX_DESC_CNT - TX_IDX(prod);
	space = bd_space * sizeof(struct tx_bd);
	if (space >= CRYPTO_PREFIX_CMD_SIZE) {
		memcpy(pcmd, pre_cmd, CRYPTO_PREFIX_CMD_SIZE);
		prod += CRYPTO_PREFIX_CMD_BDS;
	} else {
		memcpy(pcmd, pre_cmd, space);
		prod += bd_space;
		pcmd = (void *)&txr->tx_desc_ring[TX_RING(bp, prod)][TX_IDX(prod)];
		memcpy(pcmd, (u8 *)pre_cmd + space,
		       CRYPTO_PREFIX_CMD_SIZE - space);
		prod += CRYPTO_PREFIX_CMD_BDS - bd_space;
	}
	txr->tx_prod = prod;
	tx_buf->is_push = 1;
	tx_buf->inline_data_bds = CRYPTO_PREFIX_CMD_BDS - 1;
}

static struct sk_buff *
bnxt_ktls_tx_replay(struct bnxt *bp, struct bnxt_tx_ring_info *txr,
		    struct sk_buff *skb, struct tls_record_info *record,
		    u32 replay_len)
{
	int headlen, headroom;
	struct sk_buff *nskb;
	struct ipv6hdr *ip6h;
	struct tcphdr *th;
	struct iphdr *iph;
	int remaining, i;

	headlen = skb_headlen(skb);
	headroom = skb_headroom(skb);
	nskb = alloc_skb(headlen + headroom, GFP_ATOMIC);
	if (!nskb)
		return NULL;

	skb_reserve(nskb, headroom);
	skb_put(nskb, headlen);
	memcpy(nskb->data, skb->data, headlen);
	skb_copy_header(nskb, skb);
	th = tcp_hdr(nskb);
	th->seq = htonl(tls_record_start_seq(record));
	if (skb->protocol == htons(ETH_P_IPV6)) {
		ip6h = ipv6_hdr(nskb);
		ip6h->payload_len = htons(replay_len + __tcp_hdrlen(th));
		skb_shinfo(nskb)->gso_type = SKB_GSO_TCPV6;
	} else {
		iph = ip_hdr(nskb);
		iph->tot_len = htons(replay_len + __tcp_hdrlen(th) +
				     ip_hdrlen(nskb));
		skb_shinfo(nskb)->gso_type = SKB_GSO_TCPV4;
	}
	/* The exact MSS does not matter because the replay packet never
	 * gets on the wire.  It just needs to be smaller than the max
	 * jumbo frame.
	 */
	if (replay_len > BNXT_KTLS_MAX_REPLAY_MSS)
		skb_shinfo(nskb)->gso_size = BNXT_KTLS_MAX_REPLAY_MSS;
	else
		skb_gso_reset(nskb);

	remaining = replay_len;
	for (i = 0; remaining > 0 && i < record->num_frags; i++) {
		skb_frag_t *frag = &skb_shinfo(nskb)->frags[i];
		int len;

		len = skb_frag_size(&record->frags[i]) >= remaining ?
				    remaining :
				    skb_frag_size(&record->frags[i]);

		skb_frag_page_copy(frag, &record->frags[i]);
		__skb_frag_ref(frag);
		skb_frag_off_copy(frag, &record->frags[i]);
		skb_frag_size_set(frag, len);
		nskb->data_len += len;
		nskb->len += len;
		remaining -= len;
	}
	if (remaining) {
		dev_kfree_skb_any(nskb);
		return NULL;
	}
	skb_shinfo(nskb)->nr_frags = i;
	return nskb;
}

static int bnxt_ktls_tx_ooo(struct bnxt *bp, struct bnxt_tx_ring_info *txr,
			    struct sk_buff *skb, u32 payload_len, u32 seq,
			    struct tls_context *tls_ctx)
{
	struct bnxt_tls_info *ktls = bp->ktls_info;
	struct tls_offload_context_tx *tx_tls_ctx;
	struct bnxt_ktls_offload_ctx_tx *kctx_tx;
	u32 hdr_tcp_seq, end_seq, total_bds;
	struct crypto_prefix_cmd pcmd = {};
	struct tls_record_info *record;
	struct sk_buff *nskb = NULL;
	unsigned long flags;
	bool fwd = false;
	u64 rec_sn;
	u8 *hdr;
	int rc;

	tx_tls_ctx = tls_offload_ctx_tx(tls_ctx);
	kctx_tx = __tls_driver_ctx(tls_ctx, TLS_OFFLOAD_CTX_DIR_TX);
	end_seq = seq + skb->len - skb_tcp_all_headers(skb);
	if (unlikely(after(seq, kctx_tx->tcp_seq_no) ||
		     after(end_seq, kctx_tx->tcp_seq_no))) {
		fwd = true;
		pcmd.flags = CRYPTO_PREFIX_CMD_FLAGS_UPDATE_IN_ORDER_VAR_LE;
		atomic64_inc(&ktls->counters[BNXT_KTLS_TX_SEQ_FWD]);
	}

	spin_lock_irqsave(&tx_tls_ctx->lock, flags);
	record = tls_get_record(tx_tls_ctx, seq, &rec_sn);
	if (!record || !record->num_frags) {
		rc = -EPROTO;
		atomic64_inc(&ktls->counters[BNXT_KTLS_TX_REC_ERR]);
		goto unlock_exit;
	}
	hdr_tcp_seq = tls_record_start_seq(record);
	hdr = skb_frag_address_safe(&record->frags[0]);

	total_bds = CRYPTO_PRESYNC_BDS + skb_shinfo(skb)->nr_frags + 2;
	if (bnxt_tx_avail(bp, txr) < total_bds) {
		rc = -ENOSPC;
		goto unlock_exit;
	}

	pcmd.header_tcp_seq_num = cpu_to_le32(hdr_tcp_seq);
	pcmd.start_tcp_seq_num = cpu_to_le32(seq);
	pcmd.end_tcp_seq_num = cpu_to_le32(seq + payload_len - 1);
	if (tls_ctx->prot_info.version == TLS_1_2_VERSION) {
		u32 nonce_bytes = tls_ctx->prot_info.iv_size;
		u32 retrans_off = seq - hdr_tcp_seq;

		if (retrans_off > 5 && retrans_off < 5 + nonce_bytes)
			nonce_bytes = retrans_off - 5;
		memcpy(pcmd.explicit_nonce, hdr + 5, nonce_bytes);
	}
	memcpy(&pcmd.record_seq_num[0], &rec_sn, sizeof(rec_sn));

	/* retransmission includes tag bytes */
	if (before(record->end_seq - tls_ctx->prot_info.tag_size,
		   seq + payload_len)) {
		u32 replay_len = seq - hdr_tcp_seq;

		nskb = bnxt_ktls_tx_replay(bp, txr, skb, record, replay_len);
		if (!nskb) {
			rc = -ENOMEM;
			goto unlock_exit;
		}
		total_bds += skb_shinfo(nskb)->nr_frags + 2;
		if (bnxt_tx_avail(bp, txr) < total_bds) {
			dev_kfree_skb_any(nskb);
			rc = -ENOSPC;
			goto unlock_exit;
		}
	}
	rc = 0;
	atomic64_inc(&ktls->counters[BNXT_KTLS_TX_RETRANS]);
	bnxt_ktls_pre_xmit(bp, txr, kctx_tx->kid, &pcmd);

	if (nskb) {
		struct netdev_queue *txq;
		u32 kid = BNXT_KID_HW(kctx_tx->kid);
		__le32 lflags;
		int txq_map;

		txq_map = skb_get_queue_mapping(nskb);
		txq = netdev_get_tx_queue(bp->dev, txq_map);
		lflags = cpu_to_le32(TX_BD_FLAGS_CRYPTO_EN |
				     BNXT_TX_KID_LO(kid));
		__bnxt_start_xmit(bp, txq, txr, nskb, lflags, kid);
		atomic64_inc(&ktls->counters[BNXT_KTLS_TX_REPLAY]);
	}
	if (fwd)
		kctx_tx->tcp_seq_no = end_seq;

unlock_exit:
	spin_unlock_irqrestore(&tx_tls_ctx->lock, flags);
	return rc;
}

struct sk_buff *bnxt_ktls_xmit(struct bnxt *bp, struct bnxt_tx_ring_info *txr,
			       struct sk_buff *skb, __le32 *lflags, u32 *kid)
{
	struct bnxt_tls_info *ktls = bp->ktls_info;
	struct bnxt_ktls_offload_ctx_tx *kctx_tx;
	struct tls_context *tls_ctx;
	u32 seq;

	if (!IS_ENABLED(CONFIG_TLS_DEVICE) || !skb->sk ||
	    !tls_is_skb_tx_device_offloaded(skb))
		return skb;

	seq = ntohl(tcp_hdr(skb)->seq);
	tls_ctx = tls_get_ctx(skb->sk);
	kctx_tx = __tls_driver_ctx(tls_ctx, TLS_OFFLOAD_CTX_DIR_TX);
	if (kctx_tx->tcp_seq_no == seq) {
		kctx_tx->tcp_seq_no += skb->len - skb_tcp_all_headers(skb);
		*kid = BNXT_KID_HW(kctx_tx->kid);
		*lflags |= cpu_to_le32(TX_BD_FLAGS_CRYPTO_EN |
				       BNXT_TX_KID_LO(*kid));
		atomic64_inc(&ktls->counters[BNXT_KTLS_TX_HW_PKT]);
	} else {
		u32 payload_len;
		int rc;

		payload_len = skb->len - skb_tcp_all_headers(skb);
		if (!payload_len)
			return skb;

		atomic64_inc(&ktls->counters[BNXT_KTLS_TX_OOO]);

		rc = bnxt_ktls_tx_ooo(bp, txr, skb, payload_len, seq, tls_ctx);
		if (rc) {
			atomic64_inc(&ktls->counters[BNXT_KTLS_TX_SW_PKT]);
			return tls_encrypt_skb(skb);
		}
		*kid = BNXT_KID_HW(kctx_tx->kid);
		*lflags |= cpu_to_le32(TX_BD_FLAGS_CRYPTO_EN |
				       BNXT_TX_KID_LO(*kid));
		return skb;
	}
	return skb;
}

static void bnxt_ktls_resync_nak(struct bnxt *bp, u32 kid, u32 seq)
{
	struct bnxt_tls_info *ktls = bp->ktls_info;
	struct ce_resync_resp_ack_cmd cmd = {0};
	struct bnxt_tx_ring_info *txr;
	u32 data;

	txr = bnxt_select_mpc_ring(bp, BNXT_MPC_RCE_TYPE);
	data = CE_RESYNC_RESP_ACK_CMD_OPCODE_RESYNC |
	       (BNXT_KID_HW(kid) << CE_RESYNC_RESP_ACK_CMD_KID_SFT) |
	       CE_RESYNC_RESP_ACK_CMD_RESYNC_STATUS_NAK;
	cmd.resync_status_kid_opcode = cpu_to_le32(data);
	cmd.resync_record_tcp_seq_num = cpu_to_le32(seq - TLS_HEADER_SIZE + 1);
	bnxt_xmit_crypto_cmd(bp, txr, &cmd, sizeof(cmd), 0, ktls);
	atomic64_inc(&ktls->counters[BNXT_KTLS_RX_RESYNC_NAK]);
}

static void bnxt_ktls_rx_resync_exp(struct bnxt *bp,
				    struct bnxt_ktls_offload_ctx_rx *kctx_rx,
				    u32 bytes)
{
	u32 tcp_seq_no;

	spin_lock_bh(&kctx_rx->resync_lock);
	if (!kctx_rx->resync_pending)
		goto unlock;
	kctx_rx->bytes_since_resync += bytes;
	if (kctx_rx->bytes_since_resync > BNXT_KTLS_MAX_RESYNC_BYTES &&
	    time_after(jiffies, kctx_rx->resync_timestamp +
		       BNXT_KTLS_RESYNC_TMO)) {
		kctx_rx->resync_pending = false;
		tcp_seq_no = kctx_rx->resync_tcp_seq_no;
		spin_unlock_bh(&kctx_rx->resync_lock);
		bnxt_ktls_resync_nak(bp, kctx_rx->kid, tcp_seq_no);
		return;
	}
unlock:
	spin_unlock_bh(&kctx_rx->resync_lock);
}

void bnxt_ktls_rx(struct bnxt *bp, struct sk_buff *skb, u8 *data_ptr,
		  unsigned int len, struct rx_cmp *rxcmp,
		  struct rx_cmp_ext *rxcmp1)
{
	struct bnxt_tls_info *ktls = bp->ktls_info;
	unsigned int off = BNXT_METADATA_OFF(len);
	struct bnxt_ktls_offload_ctx_rx *kctx_rx;
	struct tls_metadata_base_msg *md;
	struct tls_context *tls_ctx;
	u32 md_data;

	md = (struct tls_metadata_base_msg *)(data_ptr + off);
	md_data = le32_to_cpu(md->md_type_link_flags_kid_lo);
	if (md_data & TLS_METADATA_BASE_MSG_FLAGS_DECRYPTED) {
		skb->decrypted = true;
		atomic64_inc(&ktls->counters[BNXT_KTLS_RX_HW_PKT]);
	} else {
		u32 misc = le32_to_cpu(rxcmp->rx_cmp_misc_v1);
		struct tls_metadata_resync_msg *resync_msg;
		u32 payload_off, tcp_seq, md_type;
		struct net_device *dev = bp->dev;
		struct net *net = dev_net(dev);
		u8 agg_bufs, *l3_ptr;
		struct tcphdr *th;
		struct sock *sk;

		payload_off = RX_CMP_PAYLOAD_OFF(misc);
		agg_bufs = (misc & RX_CMP_AGG_BUFS) >> RX_CMP_AGG_BUFS_SHIFT;
		/* No payload */
		if (payload_off == len && !agg_bufs)
			return;

		l3_ptr = data_ptr + RX_CMP_INNER_L3_OFF(rxcmp1);
		if (RX_CMP_IS_IPV6(rxcmp1)) {
			struct ipv6hdr *ip6h = (struct ipv6hdr *)l3_ptr;
			u8 *nextp = (u8 *)(ip6h + 1);
			u8 nexthdr = ip6h->nexthdr;

			while (ipv6_ext_hdr(nexthdr)) {
				struct ipv6_opt_hdr *hp;

				hp = (struct ipv6_opt_hdr *)nextp;
				if (nexthdr == NEXTHDR_AUTH)
					nextp += ipv6_authlen(hp);
				else
					nextp += ipv6_optlen(hp);
				nexthdr = hp->nexthdr;
			}
			th = (struct tcphdr *)nextp;
			sk = __inet6_lookup_established(net,
					net->ipv4.tcp_death_row.hashinfo,
					&ip6h->saddr, th->source, &ip6h->daddr,
					ntohs(th->dest), dev->ifindex, 0);
		} else {
			struct iphdr *iph = (struct iphdr *)l3_ptr;

			th = (struct tcphdr *)(l3_ptr + iph->ihl * 4);
			sk = inet_lookup_established(net,
					net->ipv4.tcp_death_row.hashinfo,
					iph->saddr, th->source, iph->daddr,
					th->dest, dev->ifindex);
		}
		if (!sk)
			goto rx_done_no_sk;

		if (!tls_is_sk_rx_device_offloaded(sk))
			goto rx_done;

		tls_ctx = tls_get_ctx(sk);
		kctx_rx = bnxt_get_ktls_ctx_rx(tls_ctx);
		if (!kctx_rx)
			goto rx_done;

		md_type = md_data & TLS_METADATA_BASE_MSG_MD_TYPE_MASK;
		if (md_type != TLS_METADATA_BASE_MSG_MD_TYPE_TLS_RESYNC) {
			bnxt_ktls_rx_resync_exp(bp, kctx_rx, len - payload_off);
			goto rx_done;
		}

		resync_msg = (struct tls_metadata_resync_msg *)md;
		tcp_seq = le32_to_cpu(resync_msg->resync_record_tcp_seq_num);
		tcp_seq += TLS_HEADER_SIZE - 1;

		spin_lock_bh(&kctx_rx->resync_lock);
		kctx_rx->resync_pending = true;
		kctx_rx->resync_tcp_seq_no = tcp_seq;
		kctx_rx->bytes_since_resync = 0;
		kctx_rx->resync_timestamp = jiffies;
		spin_unlock_bh(&kctx_rx->resync_lock);

		tls_offload_rx_resync_request(sk, htonl(tcp_seq));
		atomic64_inc(&ktls->counters[BNXT_KTLS_RX_RESYNC_REQ]);
rx_done:
		sock_gen_put(sk);
rx_done_no_sk:
		atomic64_inc(&ktls->counters[BNXT_KTLS_RX_SW_PKT]);
	}
}
#else	/* HAVE_KTLS */

void bnxt_alloc_ktls_info(struct bnxt *bp, struct hwrm_func_qcaps_output *resp)
{
}

void bnxt_clear_cfa_tls_filters_tbl(struct bnxt *bp)
{
}

void bnxt_free_ktls_info(struct bnxt *bp)
{
}

int bnxt_ktls_init(struct bnxt *bp)
{
	return 0;
}

void bnxt_ktls_mpc_cmp(struct bnxt *bp, u32 client, unsigned long handle,
		       struct bnxt_cmpl_entry cmpl[], u32 entries)
{
}

struct sk_buff *bnxt_ktls_xmit(struct bnxt *bp, struct bnxt_tx_ring_info *txr,
			       struct sk_buff *skb, __le32 *lflags, u32 *kid)
{
	return skb;
}

void bnxt_ktls_rx(struct bnxt *bp, struct sk_buff *skb, u8 *data_ptr,
		  unsigned int len, struct rx_cmp *rxcmp,
		  struct rx_cmp_ext *rxcmp1)
{
}
#endif	/* HAVE_KTLS */
