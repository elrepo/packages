// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
/* Broadcom NetXtreme-C/E network driver.
 *
 * Copyright (c) 2024 Broadcom Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 */

#ifdef HAVE_KTLS
#include <net/tls.h>
#endif
#include "bnxt_compat.h"
#include "bnxt.h"
#include "bnxt_mpc.h"
#include "bnxt_ktls.h"
#include "bnxt_quic.h"

#ifdef HAVE_BNXT_QUIC

struct bnxt_quic_crypto_info quic_flow;

static inline unsigned int quic_flow_hash_v4(__be32 src_ip, __be32 dst_ip,
					     __be16 src_port, __be16 dst_port);

static inline unsigned int quic_flow_hash_v6(struct in6_addr *src_ip6, struct in6_addr *dst_ip6,
					     __be16 src_port, __be16 dst_port);

static struct bnxt_quic_crypto_info *bnxt_quic_flow_lookup_v4(struct bnxt *bp,
							      __be32 src_ip, __be32 dst_ip,
							      __be16 src_port, __be16 dst_port)
{
	struct bnxt_quic_crypto_info *quic_flow;
	struct bnxt_tls_info *quic;
	unsigned int hash;

	hash = quic_flow_hash_v4(src_ip, dst_ip, src_port, dst_port);

	quic = bp->quic_info;

	rcu_read_lock_bh();
	hash_for_each_possible_rcu(quic->quic_tx_fltr_tbl, quic_flow, node, hash) {
		struct bnxt_quic_connection_info *info = &quic_flow->connection_info;
		struct sockaddr_in *src = (struct sockaddr_in *)&info->saddr;
		struct sockaddr_in *dst = (struct sockaddr_in *)&info->daddr;

		if (src->sin_addr.s_addr == src_ip &&
		    dst->sin_addr.s_addr == dst_ip &&
		    htons(info->sport) == src_port &&
		    htons(info->dport) == dst_port) {
			rcu_read_unlock_bh();
			return quic_flow;
		}
	}
	rcu_read_unlock_bh();

	return NULL;
}

static struct bnxt_quic_crypto_info *bnxt_quic_flow_lookup_v6(struct bnxt *bp,
							      struct in6_addr *src_ip6,
							      struct in6_addr *dst_ip6,
							      __be16 src_port, __be16 dst_port)
{
	struct bnxt_quic_crypto_info *quic_flow;
	struct bnxt_tls_info *quic;
	unsigned int hash;

	hash = quic_flow_hash_v6(src_ip6, dst_ip6, src_port, dst_port);

	quic = bp->quic_info;

	rcu_read_lock_bh();
	hash_for_each_possible_rcu(quic->quic_tx_fltr_tbl, quic_flow, node, hash) {
		struct bnxt_quic_connection_info *info = &quic_flow->connection_info;
		struct sockaddr_in6 *src_in6 = (struct sockaddr_in6 *)&info->saddr;
		struct sockaddr_in6 *dst_in6 = (struct sockaddr_in6 *)&info->daddr;

		if (!memcmp(&src_in6->sin6_addr, src_ip6, sizeof(*src_ip6)) &&
		    !memcmp(&dst_in6->sin6_addr, dst_ip6, sizeof(*src_ip6)) &&
		    htons(info->sport) == src_port &&
		    htons(info->dport) == dst_port) {
			rcu_read_unlock_bh();
			return quic_flow;
		}
	}
	rcu_read_unlock_bh();

	return NULL;
}

struct sk_buff *bnxt_quic_xmit(struct bnxt *bp, struct bnxt_tx_ring_info *txr,
			       struct sk_buff *skb, __le32 *lflags, u32 *kid)
{
	struct bnxt_quic_crypto_info *quic_flow = NULL;
	struct bnxt_tls_info *quic = bp->quic_info;
	struct udphdr *udph = udp_hdr(skb);

	/* Check if flow is programmed. */
	if (quic) {
		if (skb->protocol == htons(ETH_P_IP)) {
			struct iphdr *ip4h = ip_hdr(skb);

			quic_flow = bnxt_quic_flow_lookup_v4(bp, ip4h->saddr, ip4h->daddr,
							     udph->source, udph->dest);
		} else if (skb->protocol == htons(ETH_P_IPV6)) {
			struct ipv6hdr *ip6h = ipv6_hdr(skb);

			quic_flow = bnxt_quic_flow_lookup_v6(bp, &ip6h->saddr, &ip6h->daddr,
							     udph->source, udph->dest);
		}

		if (!quic_flow)
			goto exit;

		*kid = quic_flow->tx_kid;
		*lflags |= cpu_to_le32(TX_BD_FLAGS_CRYPTO_EN |
				       BNXT_TX_KID_LO(*kid));
		atomic64_inc(&quic->counters[BNXT_QUIC_TX_HW_PKT]);
	}
exit:
	return skb;
}

static int bnxt_quic_crypto_tx_add(struct bnxt *bp,
				   struct bnxt_quic_crypto_info *quic_flow,
				   u32 kid)
{
	struct bnxt_quic_connection_info *conn_info;
	struct bnxt_mpc_info *mpc = bp->mpc_info;
	struct bnxt_tx_ring_info *txr;
	struct quic_ce_add_cmd cmd = {0};
	u32 data;

	if (!mpc)
		return 0;

	txr = &mpc->mpc_rings[BNXT_MPC_TCE_TYPE][0];
	conn_info = &quic_flow->connection_info;

	data = CE_ADD_CMD_OPCODE_ADD | (kid << CE_ADD_CMD_KID_SFT) | CE_ADD_CMD_VERSION_QUIC;
	switch (conn_info->cipher) {
	case TLS_CIPHER_AES_GCM_128:
		data |= CE_ADD_CMD_ALGORITHM_AES_GCM_128;
		if (conn_info->version == TLS_1_3_VERSION)
			data |= CE_ADD_CMD_VERSION_TLS1_3;
		memcpy(&cmd.session_key, conn_info->tx_data_key, sizeof(conn_info->tx_data_key));
		memcpy(&cmd.hp_key, conn_info->tx_hdr_key, sizeof(conn_info->tx_hdr_key));
		memcpy(&cmd.iv, conn_info->tx_iv, sizeof(conn_info->tx_iv));
		break;
	case TLS_CIPHER_AES_GCM_256:
		data |= CE_ADD_CMD_ALGORITHM_AES_GCM_256;
		if (quic_flow->connection_info.version == TLS_1_3_VERSION)
			data |= CE_ADD_CMD_VERSION_TLS1_3;
		memcpy(&cmd.session_key, conn_info->tx_data_key, sizeof(conn_info->tx_data_key));
		memcpy(&cmd.hp_key, conn_info->tx_hdr_key, sizeof(conn_info->tx_hdr_key));
		memcpy(&cmd.iv, conn_info->tx_iv, sizeof(conn_info->tx_iv));
		break;
	default:
		return -EINVAL;

	}

	cmd.ver_algo_kid_opcode = cpu_to_le32(data);
	cmd.pkt_number = cpu_to_le32(conn_info->pkt_number);
	conn_info->dst_conn_id_width = sizeof(conn_info->rx_conn_id);
	data = QUIC_CE_ADD_CMD_CTX_KIND_CK_TX << QUIC_CE_ADD_CMD_CTX_KIND_SFT;
	data |= conn_info->dst_conn_id_width << QUIC_CE_ADD_CMD_DST_CID_SFT;
	cmd.ctx_kind_dst_cid_width_key_phase = cpu_to_le32(data);
	return bnxt_xmit_crypto_cmd(bp, txr, &cmd, sizeof(cmd),
				    BNXT_MPC_TMO_MSECS, bp->quic_info);
}

static int bnxt_quic_crypto_rx_add(struct bnxt *bp,
				   struct bnxt_quic_crypto_info *quic_flow,
				   u32 kid)
{
	struct bnxt_quic_connection_info *conn_info;
	struct bnxt_mpc_info *mpc = bp->mpc_info;
	struct quic_ce_add_cmd cmd = {0};
	struct bnxt_tx_ring_info *txr;
	u32 data = 0, data1 = 0;

	if (!mpc)
		return 0;

	txr = &mpc->mpc_rings[BNXT_MPC_RCE_TYPE][0];
	conn_info = &quic_flow->connection_info;

	data = CE_ADD_CMD_OPCODE_ADD | (kid << CE_ADD_CMD_KID_SFT) | CE_ADD_CMD_VERSION_QUIC;

	data1 = QUIC_CE_ADD_CMD_CTX_KIND_CK_RX << QUIC_CE_ADD_CMD_CTX_KIND_SFT;
	data1 |= quic_flow->connection_info.dst_conn_id_width << QUIC_CE_ADD_CMD_DST_CID_SFT;

	switch (conn_info->cipher) {
	case TLS_CIPHER_AES_GCM_128:
		data |= CE_ADD_CMD_ALGORITHM_AES_GCM_128;
		if (conn_info->version == TLS_1_3_VERSION)
			data |= CE_ADD_CMD_VERSION_TLS1_3;
		break;

	case TLS_CIPHER_AES_GCM_256:
		data |= CE_ADD_CMD_ALGORITHM_AES_GCM_256;
		if (conn_info->version == TLS_1_3_VERSION)
			data |= CE_ADD_CMD_VERSION_TLS1_3;
		break;
	default:
		return -EINVAL;
	}
	cmd.ver_algo_kid_opcode = cpu_to_le32(data);
	cmd.ctx_kind_dst_cid_width_key_phase = cpu_to_le32(data1);
	memcpy(&cmd.session_key, conn_info->rx_data_key, sizeof(conn_info->rx_data_key));
	memcpy(&cmd.hp_key, conn_info->rx_hdr_key, sizeof(conn_info->rx_hdr_key));
	memcpy(&cmd.iv, conn_info->rx_iv, sizeof(conn_info->rx_iv));

	cmd.pkt_number = cpu_to_le32(conn_info->pkt_number);

	return bnxt_xmit_crypto_cmd(bp, txr, &cmd, sizeof(cmd),
				    BNXT_MPC_TMO_MSECS, bp->quic_info);
}

static int bnxt_quic_crypto_add(struct bnxt *bp, struct bnxt_quic_crypto_info *quic_flow,
				enum tls_offload_ctx_dir dir, u32 kid)
{
	if (dir == TLS_OFFLOAD_CTX_DIR_RX)
		return bnxt_quic_crypto_rx_add(bp, quic_flow, kid);
	else
		return bnxt_quic_crypto_tx_add(bp, quic_flow, kid);
}

static void bnxt_quic_fill_sk(struct sock *sk, struct bnxt_quic_crypto_info *quic_flow)
{
	struct bnxt_quic_connection_info *info = &quic_flow->connection_info;
	struct inet_sock *inet = inet_sk(sk);

	sk->sk_protocol = IPPROTO_UDP;
	sk->sk_family = info->daddr.ss_family;
	inet->inet_dport = htons(info->dport);
	inet->inet_sport = htons(info->sport);

	if (info->daddr.ss_family == AF_INET) {
		struct sockaddr_in *src = (struct sockaddr_in *)&info->saddr;
		struct sockaddr_in *dst = (struct sockaddr_in *)&info->daddr;

		inet->inet_daddr = dst->sin_addr.s_addr;
		inet->inet_saddr = src->sin_addr.s_addr;
	} else if (info->daddr.ss_family == AF_INET6) {
		struct sockaddr_in6 *src_in6 = (struct sockaddr_in6 *)&info->saddr;
		struct sockaddr_in6 *dst_in6 = (struct sockaddr_in6 *)&info->daddr;

		sk->sk_v6_daddr = dst_in6->sin6_addr;
		sk->sk_v6_rcv_saddr = src_in6->sin6_addr;
	}
}

static int bnxt_quic_crypto_del(struct bnxt *bp,
				enum tls_offload_ctx_dir direction, u32 kid)
{
	struct bnxt_mpc_info *mpc = bp->mpc_info;
	struct quic_ce_delete_cmd cmd = {0};
	struct bnxt_tx_ring_info *txr;
	u32 data;

	if (direction == TLS_OFFLOAD_CTX_DIR_RX) {
		txr = &mpc->mpc_rings[BNXT_MPC_RCE_TYPE][0];
		data = CE_DELETE_CMD_CTX_KIND_QUIC_RX;
	} else {
		txr = &mpc->mpc_rings[BNXT_MPC_TCE_TYPE][0];
		data = CE_DELETE_CMD_CTX_KIND_QUIC_TX;
	}

	data |= CE_DELETE_CMD_OPCODE_DEL | (kid << CE_DELETE_CMD_KID_SFT);

	cmd.ctx_kind_kid_opcode = cpu_to_le32(data);
	return bnxt_xmit_crypto_cmd(bp, txr, &cmd, sizeof(cmd),
				    BNXT_MPC_TMO_MSECS, bp->quic_info);
}

static int _bnxt_quic_dev_add(struct bnxt *bp, struct sock *sk,
			      struct bnxt_quic_crypto_info *quic_flow,
			      enum tls_offload_ctx_dir direction)
{
	struct bnxt_tls_info *quic;
	struct bnxt_kctx *kctx;
	u32 kid;
	int rc;

	if (!bp->quic_info)
		return -EINVAL;

	quic = bp->quic_info;
	atomic_inc(&quic->pending);
	/* Make sure bnxt_close_nic() sees pending before we check the
	 * BNXT_STATE_OPEN flag.
	 */
	smp_mb__after_atomic();
	if (!test_bit(BNXT_STATE_OPEN, &bp->state)) {
		atomic_dec(&quic->pending);
		return -ENODEV;
	}

	if (direction == TLS_OFFLOAD_CTX_DIR_RX)
		kctx = &quic->rck;
	else
		kctx = &quic->tck;

	rc = bnxt_key_ctx_alloc_one(bp, kctx, &kid, BNXT_CRYPTO_TYPE_QUIC);
	if (rc)
		goto exit;

	rc = bnxt_quic_crypto_add(bp, quic_flow, direction, kid);
	if (rc)
		goto bnxt_quic_dev_add_err;

	if (direction == TLS_OFFLOAD_CTX_DIR_RX) {
		quic_flow->rx_kid = kid;
		rc = bnxt_hwrm_cfa_tls_filter_alloc(bp, sk, kid, BNXT_CRYPTO_TYPE_QUIC,
						    quic_flow->connection_info.rx_conn_id);
		if (!rc)
			atomic64_inc(&quic->counters[BNXT_QUIC_RX_ADD]);
		else
			bnxt_quic_crypto_del(bp, direction, kid);
	} else {
		quic_flow->tx_kid = kid;
		atomic64_inc(&quic->counters[BNXT_QUIC_TX_ADD]);
	}

bnxt_quic_dev_add_err:
	if (rc)
		bnxt_free_one_kctx(kctx, kid);
exit:
	atomic_dec(&quic->pending);
	return rc;
}

static inline unsigned int quic_flow_hash_v4(__be32 src_ip, __be32 dst_ip,
					     __be16 src_port, __be16 dst_port)
{
	unsigned long hashval;

	hashval = jhash_3words(src_ip, dst_ip, (u32)(src_port << 16 | dst_port), 0);
	return hashval % (1 << BNXT_QUIC_FLTR_HASH_SIZE);
}

static inline unsigned int quic_flow_hash_v6(struct in6_addr *src_ip6, struct in6_addr *dst_ip6,
					     __be16 src_port, __be16 dst_port)
{
	unsigned long hashval;
	u32 key[10];

	memcpy(&key[0], src_ip6, sizeof(struct in6_addr));
	memcpy(&key[4], dst_ip6, sizeof(struct in6_addr));
	key[8] = (u32)(src_port << 16 | dst_port);
	key[9] = 0;
	hashval = jhash2(key, 10, 0);

	return hashval % (1 << BNXT_QUIC_FLTR_HASH_SIZE);
}

static void bnxt_quic_insert_hash(struct sock *sk, struct bnxt_quic_crypto_info *quic_flow)
{
	struct inet_sock *inet = inet_sk(sk);
	struct bnxt *bp = quic_flow->bp;
	struct bnxt_tls_info *quic;
	unsigned int hash;

	quic = bp->quic_info;

	if (sk->sk_family == AF_INET) {
		hash = quic_flow_hash_v4(inet->inet_saddr, inet->inet_daddr,
					 inet->inet_sport, inet->inet_dport);
		netdev_dbg(bp->dev, "%s() [IPv4] hash = %d saddr: %pI4 daddr: %pI4 sport: %d dport: %d\n",
			   __func__, hash, &inet->inet_saddr, &inet->inet_daddr,
			   ntohs(inet->inet_sport), ntohs(inet->inet_dport));
	} else if (sk->sk_family == AF_INET6) {
		struct in6_addr *src_ip6 = &sk->sk_v6_rcv_saddr;
		struct in6_addr *dst_ip6 = &sk->sk_v6_daddr;

		hash = quic_flow_hash_v6(src_ip6, dst_ip6,
					 inet->inet_sport, inet->inet_dport);
		netdev_dbg(bp->dev, "%s() [IPv6] hash = %u saddr: %pI6 daddr: %pI6 sport: %u dport: %u\n",
			   __func__, hash, src_ip6, dst_ip6,
			   ntohs(inet->inet_sport), ntohs(inet->inet_dport));
	}

	spin_lock_bh(&quic->quic_fltr_lock);
	quic->quic_tx_fltr_count++;
	hash_add_rcu(quic->quic_tx_fltr_tbl, &quic_flow->node, hash);
	spin_unlock_bh(&quic->quic_fltr_lock);

	netdev_info(bp->dev, "Inserted new QUIC TX flow into database.\n");
}

static int bnxt_quic_dev_add(struct bnxt_quic_crypto_info *quic_flow)
{
	struct sock *sk;
	int rc;

	sk = kmalloc(sizeof(*sk), GFP_KERNEL);
	if (!sk)
		return -ENOMEM;

	bnxt_quic_fill_sk(sk, quic_flow);
	if (quic_flow->connection_info.offload_dir == TLS_OFFLOAD_CTX_DIR_RX) {
		rc = _bnxt_quic_dev_add(quic_flow->bp, sk, quic_flow, TLS_OFFLOAD_CTX_DIR_RX);
		if (rc)
			netdev_err(quic_flow->bp->dev, "%s Rx failed\n", __func__);
	} else if (quic_flow->connection_info.offload_dir == TLS_OFFLOAD_CTX_DIR_TX) {
		rc = _bnxt_quic_dev_add(quic_flow->bp, sk, quic_flow, TLS_OFFLOAD_CTX_DIR_TX);
		if (!rc)
			bnxt_quic_insert_hash(sk, quic_flow);
		else
			netdev_err(quic_flow->bp->dev, "%s Tx failed\n", __func__);
	}
	kfree(sk);

	return rc;
}

#define QUIC_RETRY_MAX	20
static int _bnxt_quic_dev_del(struct bnxt *bp,
			      struct bnxt_quic_crypto_info *quic_flow,
			      enum tls_offload_ctx_dir dir)
{
	struct bnxt_tls_info *quic;
	struct bnxt_kctx *kctx;
	int retry_cnt = 0;
	u32 kid;
	int rc;

	quic = bp->quic_info;
	if (dir == TLS_OFFLOAD_CTX_DIR_RX) {
		kctx = &quic->rck;
		kid = quic_flow->rx_kid;
	} else {
		kctx = &quic->tck;
		kid = quic_flow->tx_kid;
	}

retry:
	atomic_inc(&quic->pending);
	/* Make sure bnxt_close_nic() sees pending before we check the
	 * BNXT_STATE_OPEN flag.
	 */
	smp_mb__after_atomic();
	while (!test_bit(BNXT_STATE_OPEN, &bp->state)) {
		atomic_dec(&quic->pending);
		if (!netif_running(bp->dev))
			return 0;
		if (retry_cnt > QUIC_RETRY_MAX) {
			netdev_warn(bp->dev, "%s retry max %d exceeded, state %lx\n",
				    __func__, retry_cnt, bp->state);
			return 0;
		}
		retry_cnt++;
		msleep(100);
		goto retry;
	}

	rc = bnxt_quic_crypto_del(bp, dir, kid);
	if (dir == TLS_OFFLOAD_CTX_DIR_RX)
		atomic64_inc(&quic->counters[BNXT_QUIC_RX_DEL]);
	else
		atomic64_inc(&quic->counters[BNXT_QUIC_TX_DEL]);

	atomic_dec(&quic->pending);

	bnxt_free_one_kctx(kctx, kid);
	return rc;
}

static int bnxt_quic_dev_del(struct bnxt_quic_crypto_info *quic_flow)
{
	struct bnxt *bp = quic_flow->bp;
	struct bnxt_tls_info *quic;
	int rc;

	quic = bp->quic_info;

	if (quic_flow->connection_info.offload_dir == TLS_OFFLOAD_CTX_DIR_TX) {
		rc = _bnxt_quic_dev_del(bp, quic_flow, TLS_OFFLOAD_CTX_DIR_TX);
		if (!rc)
			quic->quic_tx_fltr_count--;
	} else {
		rc = _bnxt_quic_dev_del(bp, quic_flow, TLS_OFFLOAD_CTX_DIR_RX);
		bnxt_hwrm_cfa_tls_filter_free(bp, quic_flow->rx_kid, BNXT_CRYPTO_TYPE_QUIC);
	}
	kfree(quic_flow);

	return rc;
}

void bnxt_alloc_quic_info(struct bnxt *bp, struct hwrm_func_qcaps_output *resp)
{
	u16 max_keys = le16_to_cpu(resp->max_key_ctxs_alloc);
	struct bnxt_tls_info *quic = bp->quic_info;

	if (BNXT_VF(bp))
		return;

	if (!quic) {
		bool partition_mode = false;
		struct bnxt_kctx *kctx;
		u16 batch_sz = 0;
		int i;

		quic = kzalloc(sizeof(*quic), GFP_KERNEL);
		if (!quic)
			return;

		quic->counters = kzalloc(sizeof(atomic64_t) * BNXT_QUIC_MAX_COUNTERS,
					 GFP_KERNEL);
		if (!quic->counters) {
			kfree(quic);
			return;
		}

		if (BNXT_PARTITION_CAP(resp)) {
			batch_sz = le16_to_cpu(resp->ctxs_per_partition);
			if (batch_sz && batch_sz <= BNXT_KID_BATCH_SIZE)
				partition_mode = true;
		}
		for (i = 0; i < BNXT_MAX_CRYPTO_KEY_TYPE; i++) {
			kctx = &quic->kctx[i];
			kctx->type = i + FUNC_KEY_CTX_ALLOC_REQ_KEY_CTX_TYPE_QUIC_TX;
			if (i == BNXT_TX_CRYPTO_KEY_TYPE)
				kctx->max_ctx = BNXT_MAX_QUIC_TX_CRYPTO_KEYS;
			else
				kctx->max_ctx = BNXT_MAX_QUIC_RX_CRYPTO_KEYS;
			INIT_LIST_HEAD(&kctx->list);
			spin_lock_init(&kctx->lock);
			atomic_set(&kctx->alloc_pending, 0);
			init_waitqueue_head(&kctx->alloc_pending_wq);
			if (partition_mode) {
				int bmap_sz;

				bmap_sz = DIV_ROUND_UP(kctx->max_ctx, batch_sz);
				kctx->partition_bmap = bitmap_zalloc(bmap_sz, GFP_KERNEL);
				if (!kctx->partition_bmap)
					partition_mode = false;
			}
		}
		quic->partition_mode = partition_mode;
		quic->ctxs_per_partition = batch_sz;

		hash_init(quic->filter_tbl);
		spin_lock_init(&quic->filter_lock);

		atomic_set(&quic->pending, 0);

		bp->quic_info = quic;
	}
	quic->max_key_ctxs_alloc = max_keys;
}

void bnxt_free_quic_info(struct bnxt *bp)
{
	struct bnxt_tls_info *quic = bp->quic_info;
	struct bnxt_kid_info *kid, *tmp;
	struct bnxt_kctx *kctx;
	int i;

	if (!quic)
		return;

	/* Shutting down, no need to protect the lists. */
	for (i = 0; i < BNXT_MAX_CRYPTO_KEY_TYPE; i++) {
		kctx = &quic->kctx[i];
		list_for_each_entry_safe(kid, tmp, &kctx->list, list) {
			list_del(&kid->list);
			kfree(kid);
		}
		bitmap_free(kctx->partition_bmap);
	}
	bnxt_clear_cfa_tls_filters_tbl(bp);
	kmem_cache_destroy(quic->mpc_cache);
	kfree(quic->counters);
	kfree(quic);
	bp->quic_info = NULL;
}

int bnxt_quic_init(struct bnxt *bp)
{
	struct bnxt_hw_resc *hw_resc = &bp->hw_resc;
	struct bnxt_tls_info *quic = bp->quic_info;
	struct bnxt_hw_tls_resc *tls_resc;
	char name[32];
	int rc;

	if (!quic)
		return 0;

	tls_resc = &hw_resc->tls_resc[BNXT_CRYPTO_TYPE_QUIC];
	quic->tck.max_ctx = tls_resc->resv_tx_key_ctxs;
	quic->rck.max_ctx = tls_resc->resv_rx_key_ctxs;

	if (!quic->tck.max_ctx || !quic->rck.max_ctx)
		return 0;

	if (quic->partition_mode) {
		rc = bnxt_set_partition_mode(bp);
		if (rc)
			quic->partition_mode = false;
	}

	rc = bnxt_hwrm_key_ctx_alloc(bp, &quic->tck, BNXT_KID_BATCH_SIZE, NULL,
				     BNXT_CRYPTO_TYPE_QUIC);
	if (rc)
		return rc;

	rc = bnxt_hwrm_key_ctx_alloc(bp, &quic->rck, BNXT_KID_BATCH_SIZE, NULL,
				     BNXT_CRYPTO_TYPE_QUIC);
	if (rc)
		return rc;

	snprintf(name, sizeof(name), "bnxt_quic-%s", dev_name(&bp->pdev->dev));
	quic->mpc_cache = kmem_cache_create(name,
					    sizeof(struct bnxt_crypto_cmd_ctx),
					    0, 0, NULL);
	if (!quic->mpc_cache)
		return -ENOMEM;
	return 0;
}

void bnxt_quic_rx(struct bnxt *bp, struct sk_buff *skb, u8 *data_ptr,
		  unsigned int len, struct rx_cmp *rxcmp,
		  struct rx_cmp_ext *rxcmp1)
{
	struct bnxt_tls_info *quic = bp->quic_info;
	unsigned int off = BNXT_METADATA_OFF(len);
	struct quic_metadata_msg *quic_md;
	u32 qmd;

	quic_md = (struct quic_metadata_msg *)(data_ptr + off);
	qmd = le32_to_cpu(quic_md->md_type_link_flags_kid_lo);

	if (qmd & QUIC_METADATA_MSG_FLAGS_PAYLOAD_DECRYPTED)
		atomic64_inc(&quic->counters[BNXT_QUIC_RX_PAYLOAD_DECRYPTED]);
	if (qmd & QUIC_METADATA_MSG_FLAGS_HDR_DECRYPTED)
		atomic64_inc(&quic->counters[BNXT_QUIC_RX_HDR_DECRYPTED]);
	if (qmd & QUIC_METADATA_MSG_FLAGS_HEADER_TYPE)
		atomic64_inc(&quic->counters[BNXT_QUIC_RX_LONG_HDR]);
	else
		atomic64_inc(&quic->counters[BNXT_QUIC_RX_SHORT_HDR]);
	if (qmd & QUIC_METADATA_MSG_FLAGS_KEY_PHASE_MISMATCH)
		atomic64_inc(&quic->counters[BNXT_QUIC_RX_KEY_PHASE_MISMATCH]);
	if (qmd & QUIC_METADATA_MSG_FLAGS_RUNT)
		atomic64_inc(&quic->counters[BNXT_QUIC_RX_RUNT]);

	atomic64_inc(&quic->counters[BNXT_QUIC_RX_HW_PKT]);
}

int bnxt_siocdevprivate(struct net_device *dev, struct ifreq *ifr,
			void __user *useraddr, int cmd)
{
	struct bnxt_quic_crypto_info *quic_flow = NULL, *tmp_flow, del_flow;
	struct bnxt *bp = netdev_priv(dev);
	struct bnxt_tls_info *quic;
	unsigned int hash;
	int ret = 0;

	quic = bp->quic_info;
	if (!quic) {
		netdev_err(bp->dev, "QUIC info not allocated!\n");
		return -EOPNOTSUPP;
	}

	switch (cmd) {
	case SIOCDEVQUICFLOWADD:
		quic_flow = kzalloc(sizeof(*quic_flow), GFP_KERNEL);
		if (!quic_flow)
			return -ENOMEM;

		ret = copy_from_user(&quic_flow->connection_info, ifr->ifr_data,
				     sizeof(struct bnxt_quic_connection_info));
		if (ret) {
			netdev_err(bp->dev, "Failed to copy buffer from user space\n");
			kfree(quic_flow);
			return -EFAULT;
		}

		rcu_read_lock_bh();
		hash_for_each_possible_rcu(quic->quic_tx_fltr_tbl, tmp_flow, node, hash) {
			struct bnxt_quic_connection_info *info = &quic_flow->connection_info;
			struct bnxt_quic_connection_info *info1 = &tmp_flow->connection_info;
			bool ip_match = false;

			if (info->daddr.ss_family == AF_INET) {
				struct sockaddr_in *src1 = (struct sockaddr_in *)&info->saddr;
				struct sockaddr_in *dst1 = (struct sockaddr_in *)&info->daddr;
				struct sockaddr_in *src2 = (struct sockaddr_in *)&info1->saddr;
				struct sockaddr_in *dst2 = (struct sockaddr_in *)&info1->daddr;

				ip_match = (src1->sin_addr.s_addr == src2->sin_addr.s_addr &&
					 dst1->sin_addr.s_addr == dst2->sin_addr.s_addr);
			} else if (info->daddr.ss_family == AF_INET6) {
				struct sockaddr_in6 *src1 = (struct sockaddr_in6 *)&info->saddr;
				struct sockaddr_in6 *dst1 = (struct sockaddr_in6 *)&info->daddr;
				struct sockaddr_in6 *src2 = (struct sockaddr_in6 *)&info1->saddr;
				struct sockaddr_in6 *dst2 = (struct sockaddr_in6 *)&info1->daddr;

				ip_match = (!memcmp(&src1->sin6_addr, &src2->sin6_addr,
					    sizeof(struct in6_addr)) &&
					    !memcmp(&dst1->sin6_addr, &dst2->sin6_addr,
					    sizeof(struct in6_addr)));
			}

			if (ip_match && info->sport == info1->sport &&
			    info->dport == info1->dport) {
				netdev_info(bp->dev,
					    "%s(): Flow already present in database!\n", __func__);
				rcu_read_unlock_bh();
				kfree(quic_flow);
				return -EEXIST;
			}
		}
		rcu_read_unlock_bh();

		quic_flow->bp = bp;
		ret = bnxt_quic_dev_add(quic_flow);
		break;

	case SIOCDEVQUICFLOWDEL:
		ret = copy_from_user(&del_flow.connection_info, ifr->ifr_data,
				     sizeof(struct bnxt_quic_connection_info));
		if (ret) {
			netdev_err(bp->dev, "Failed to copy buffer from user space\n");
			return -EFAULT;
		}
		quic = bp->quic_info;

		struct bnxt_quic_connection_info *info = &del_flow.connection_info;

		if (info->daddr.ss_family == AF_INET) {
			struct sockaddr_in *src = (struct sockaddr_in *)&info->saddr;
			struct sockaddr_in *dst = (struct sockaddr_in *)&info->daddr;

			quic_flow = bnxt_quic_flow_lookup_v4(bp, src->sin_addr.s_addr,
							     dst->sin_addr.s_addr,
							     htons(del_flow.connection_info.sport),
							     htons(del_flow.connection_info.dport));
		} else if (info->daddr.ss_family == AF_INET6) {
			struct sockaddr_in6 *src = (struct sockaddr_in6 *)&info->saddr;
			struct sockaddr_in6 *dst = (struct sockaddr_in6 *)&info->daddr;

			quic_flow = bnxt_quic_flow_lookup_v6(bp, &src->sin6_addr, &dst->sin6_addr,
							     htons(del_flow.connection_info.sport),
							     htons(del_flow.connection_info.dport));
		}

		if (quic_flow) {
			spin_lock_bh(&quic->quic_fltr_lock);
			hash_del_rcu(&quic_flow->node);
			spin_unlock_bh(&quic->quic_fltr_lock);
			synchronize_rcu();
			ret = bnxt_quic_dev_del(quic_flow);
		}
		break;

	default:
		return -EOPNOTSUPP;
	}

	return ret;
}

#else

int bnxt_quic_init(struct bnxt *bp)
{
	return 0;
}

void bnxt_alloc_quic_info(struct bnxt *bp, struct hwrm_func_qcaps_output *resp)
{
}

void bnxt_free_quic_info(struct bnxt *bp)
{
}

void bnxt_get_quic_dst_conect_id(struct bnxt *bp,
				 struct hwrm_cfa_tls_filter_alloc_input *req)
{
}

void bnxt_quic_rx(struct bnxt *bp, struct sk_buff *skb, u8 *data_ptr,
		  unsigned int len, struct rx_cmp *rxcmp,
		  struct rx_cmp_ext *rxcmp1)
{
}

struct sk_buff *bnxt_quic_xmit(struct bnxt *bp, struct bnxt_tx_ring_info *txr,
			       struct sk_buff *skb, __le32 *lflags, u32 *kid)
{
	return skb;
}
#endif
