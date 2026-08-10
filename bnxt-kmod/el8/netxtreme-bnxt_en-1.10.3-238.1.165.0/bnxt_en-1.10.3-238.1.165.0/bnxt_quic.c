// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
/* Broadcom NetXtreme-C/E network driver.
 *
 * Copyright (c) 2024 Broadcom Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 */

#if defined(HAVE_BNXT_QUIC) && IS_ENABLED(CONFIG_TLS_DEVICE)
#include <net/tls.h>
#include "bnxt_compat.h"
#include "bnxt.h"
#include "bnxt_mpc.h"
#include "bnxt_ktls.h"
#include "bnxt_quic.h"

/* Compile-time check: Key mask bit encoding must match internal key_valid encoding.
 * The UAPI BNXT_QUIC_KEY_* bits use the same layout as the kernel's
 * quic_key_to_bit() helper: RX phase 0 = bit 0, RX phase 1 = bit 1,
 * TX phase 0 = bit 2, TX phase 1 = bit 3.
 */
static_assert(BNXT_QUIC_KEY_RX_PHASE_0 == (1U << 0), "RX_PHASE_0 bit mismatch");
static_assert(BNXT_QUIC_KEY_TX_PHASE_0 == (1U << 2), "TX_PHASE_0 bit mismatch");

static inline unsigned int quic_flow_hash_v4(__be32 src_ip, __be32 dst_ip,
					     __be16 src_port, __be16 dst_port)
{
	u32 hashval;

	hashval = jhash_3words(src_ip, dst_ip, (u32)(src_port << 16 | dst_port), 0);

	return hashval & ((1 << BNXT_QUIC_FLTR_HASH_SIZE) - 1);
}

static inline unsigned int quic_flow_hash_v6(struct in6_addr *src_ip6, struct in6_addr *dst_ip6,
					     __be16 src_port, __be16 dst_port)
{
	u32 hashval;
	u32 key[10];

	memcpy(&key[0], src_ip6, sizeof(struct in6_addr));
	memcpy(&key[4], dst_ip6, sizeof(struct in6_addr));
	key[8] = (u32)(src_port << 16 | dst_port);
	key[9] = 0;
	hashval = jhash2(key, 10, 0);

	return hashval & ((1 << BNXT_QUIC_FLTR_HASH_SIZE) - 1);
}

static inline unsigned int bnxt_quic_flow_info_hash(const struct bnxt_quic_flow_info *flow_info)
{
	unsigned int hash;

	if (flow_info->daddr.ss_family == AF_INET) {
		struct sockaddr_in *src = (struct sockaddr_in *)&flow_info->saddr;
		struct sockaddr_in *dst = (struct sockaddr_in *)&flow_info->daddr;

		hash = quic_flow_hash_v4(src->sin_addr.s_addr, dst->sin_addr.s_addr,
					 src->sin_port, dst->sin_port);
	} else {
		struct sockaddr_in6 *src_ip6 = (struct sockaddr_in6 *)&flow_info->saddr;
		struct sockaddr_in6 *dst_ip6 = (struct sockaddr_in6 *)&flow_info->daddr;

		hash = quic_flow_hash_v6(&src_ip6->sin6_addr, &dst_ip6->sin6_addr,
					 src_ip6->sin6_port, dst_ip6->sin6_port);
	}
	return hash;
}

/**
 * bnxt_quic_extract_flow_key - Extract 5-tuple from connection_info
 * @key: Destination flow key structure
 * @conn_info: Source connection info with full parameters
 *
 * Extracts only the 5-tuple needed for flow lookup, discarding
 * crypto parameters to save memory.
 */
/**
 * bnxt_quic_flow_key_hash - Calculate hash from flow key
 * @key: Flow key containing 5-tuple
 *
 * Return: Hash value for hash table lookup
 */
static inline unsigned int bnxt_quic_flow_key_hash(const struct bnxt_quic_flow_key *key)
{
	if (key->family == AF_INET) {
		return quic_flow_hash_v4(key->v4.saddr, key->v4.daddr,
					 key->v4.sport, key->v4.dport);
	} else {
		/* Cast away const - hash function doesn't modify the data */
		return quic_flow_hash_v6((struct in6_addr *)&key->v6.saddr,
					 (struct in6_addr *)&key->v6.daddr,
					 key->v6.sport, key->v6.dport);
	}
}

/**
 * bnxt_quic_flow_lookup_v4_with_hash - IPv4 hash lookup with pre-computed hash
 * @bp: device handle
 * @hash: pre-computed hash value
 * @src_ip: source IP address
 * @dst_ip: destination IP address
 * @src_port: source port
 * @dst_port: destination port
 *
 * IPv4-specific lookup function that uses a pre-computed hash value.
 * This eliminates redundant hash calculations when the caller already
 * has the hash.
 *
 * Caller must hold rcu_read_lock().
 *
 * Returns: Pointer to flow structure, or NULL if not found
 */
static struct bnxt_quic_crypto_info *
bnxt_quic_flow_lookup_v4_with_hash(struct bnxt *bp, unsigned int hash,
				   __be32 src_ip, __be32 dst_ip,
				   __be16 src_port, __be16 dst_port)
{
	struct bnxt_tls_info *quic = bp->quic_info;
	struct bnxt_quic_crypto_info *quic_flow;

	/* Caller must hold RCU read lock */
	hash_for_each_possible_rcu(quic->quic_tx_fltr_tbl, quic_flow, node, hash) {
		if (quic_flow->flow_key.family != AF_INET)
			continue;

		if (quic_flow->flow_key.v4.saddr == src_ip &&
		    quic_flow->flow_key.v4.daddr == dst_ip &&
		    quic_flow->flow_key.v4.sport == src_port &&
		    quic_flow->flow_key.v4.dport == dst_port) {
			return quic_flow;
		}
	}

	return NULL;
}

/**
 * bnxt_quic_flow_lookup_v6_with_hash - IPv6 hash lookup with pre-computed hash
 * @bp: device handle
 * @hash: pre-computed hash value
 * @src_ip6: source IPv6 address
 * @dst_ip6: destination IPv6 address
 * @src_port: source port
 * @dst_port: destination port
 *
 * IPv6-specific lookup function that uses a pre-computed hash value.
 * This eliminates redundant hash calculations when the caller already
 * has the hash.
 *
 * Caller must hold rcu_read_lock().
 *
 * Returns: Pointer to flow structure, or NULL if not found
 */
static struct bnxt_quic_crypto_info *
bnxt_quic_flow_lookup_v6_with_hash(struct bnxt *bp, unsigned int hash,
				   struct in6_addr *src_ip6,
				   struct in6_addr *dst_ip6,
				   __be16 src_port, __be16 dst_port)
{
	struct bnxt_tls_info *quic = bp->quic_info;
	struct bnxt_quic_crypto_info *quic_flow;

	/* Caller must hold RCU read lock */
	hash_for_each_possible_rcu(quic->quic_tx_fltr_tbl, quic_flow, node, hash) {
		if (quic_flow->flow_key.family != AF_INET6)
			continue;

		if (!memcmp(&quic_flow->flow_key.v6.saddr, src_ip6, sizeof(*src_ip6)) &&
		    !memcmp(&quic_flow->flow_key.v6.daddr, dst_ip6, sizeof(*dst_ip6)) &&
		    quic_flow->flow_key.v6.sport == src_port &&
		    quic_flow->flow_key.v6.dport == dst_port) {
			return quic_flow;
		}
	}
	return NULL;
}

/* QUIC Header Form bit. 1 for long header, 0 for short header */
#define QUIC_HEADER_FORM_BIT BIT(7)		/* 0x80 */
/* QUIC Fixed bit. Must be 1 for QUIC version 1 */
#define QUIC_BIT BIT(6)				/* 0x40 */
/* QUIC Reserved bits (bits 4-3). Must be 0 per RFC 9000 */
#define QUIC_RESERVED_BITS (BIT(4) | BIT(3))	/* 0x18 */
/* Mask to extract Key Phase bit from the first byte */
#define QUIC_KEY_PHASE_MASK BIT(2)		/* 0x04 */
/* Shift to extract Key Phase bit from the first byte */
#define QUIC_KEY_PHASE_SFT 2
/* Mask to extract Packet Number Length bits from the first byte */
#define QUIC_PN_LEN_MASK (BIT(1) | BIT(0))	/* 0x03 */

static inline u8 *quic_pkt_start(const struct sk_buff *skb)
{
	return (u8 *)(udp_hdr(skb) + 1);
}

/**
 * bnxt_quic_xmit - Process outgoing QUIC packet for hardware offload
 * @bp: pointer to bnxt device
 * @txr: pointer to TX ring
 * @skb: socket buffer containing the packet
 * @lflags: pointer to TX BD flags to be updated
 * @kid: pointer to key ID to be filled in
 *
 * Examines outgoing UDP packets to determine if they are QUIC short header
 * packets that can be hardware-offloaded. If a matching flow is found,
 * updates the TX descriptor flags and key ID for hardware encryption.
 *
 * This function performs lockless RCU-protected hash table lookup for
 * optimal performance in the TX hot path.
 *
 * Return: The skb pointer (always returns input skb)
 */
struct sk_buff *bnxt_quic_xmit(struct bnxt *bp, struct bnxt_tx_ring_info *txr,
			       struct sk_buff *skb, __le32 *lflags, u32 *kid)
{
	struct bnxt_quic_crypto_info *quic_flow = NULL;
	struct bnxt_tls_info *quic = bp->quic_info;
	struct udphdr *udph = udp_hdr(skb);
	u8 quic_hdr_type, *hdr_ptr;
	unsigned int udp_hdr_end;
	u8 key_phase;

	/* Check if flow is programmed. */
	if (!quic)
		goto exit;

	/* Fastpath: Skip lookup when no flows are configured.
	 * Flow structures are RCU-protected and remain valid until rcu_read_unlock().
	 */
	if (!READ_ONCE(quic->quic_flows_active))
		goto exit;

	/* Calculate QUIC header start position.
	 * This is reused for both length validation and linearity check.
	 */
	udp_hdr_end = skb_transport_offset(skb) + sizeof(*udph);

	/* Optimized packet size and linearity check.
	 *
	 * For linear packets, we check if at least 2 bytes of QUIC payload
	 * are in the linear portion: (udp_hdr_end + 2 <= skb_headlen(skb))
	 *
	 * This single check validates:
	 * 1. Packet has minimum 2 bytes of QUIC payload
	 * 2. Those bytes are in the linear portion (fast path)
	 *
	 * For non-linear packets, we fall back to UDP length check.
	 */
	if (likely(udp_hdr_end + 2 <= skb_headlen(skb))) {
		/* Fast path: Linear packet with sufficient QUIC payload.
		 * Direct pointer access to QUIC header byte.
		 */
		quic_hdr_type = *quic_pkt_start(skb);
	} else {
		u8 hdr_byte;

		/* Slow path: Non-linear or potentially too small.
		 * First, validate minimum size using UDP header.
		 */
		if (unlikely(ntohs(udph->len) < sizeof(*udph) + 2))
			goto exit;

		/* Non-linear path: Copy 1 byte using skb_header_pointer.
		 * This is cheaper than hash lookup, so we do it first.
		 */
		hdr_ptr = skb_header_pointer(skb, udp_hdr_end, 1, &hdr_byte);
		if (unlikely(!hdr_ptr)) {
			/* Malformed packet - header not accessible */
			goto exit;
		}
		quic_hdr_type = *hdr_ptr;
	}

	/* Validate QUIC v1 Short Header format (RFC 9000).
	 *
	 * This check filters out:
	 * - Long headers (handshake packets - not offloadable)
	 * - Already encrypted packets (Fixed bit = 1)
	 * - Invalid packets (Reserved bits != 0)
	 *
	 * Bit definitions:
	 * Bit 7 (0x80): Header Form (0=short, 1=long)
	 * Bit 6 (0x40): Fixed Bit (must be 1 for valid QUIC v1)
	 * Bit 4-3 (0x18): Reserved (must be 0 per RFC 9000)
	 *
	 * We only offload: Short Header with Fixed=0, Reserved=0
	 * (Application signals "encrypt me", HW sets Fixed=1 after)
	 */
	if (quic_hdr_type & (QUIC_HEADER_FORM_BIT | QUIC_BIT | QUIC_RESERVED_BITS))
		goto exit;

	/* It could be a short header QUIC packet, lookup the flow.
	 * Hold RCU read lock across entire flow usage to ensure flow
	 * remains valid while we access its fields.
	 */
	rcu_read_lock();

	if (skb->protocol == htons(ETH_P_IP)) {
		struct iphdr *ip4h = ip_hdr(skb);
		unsigned int hash;

		hash = quic_flow_hash_v4(ip4h->saddr, ip4h->daddr,
					 udph->source, udph->dest);

		quic_flow = bnxt_quic_flow_lookup_v4_with_hash(bp, hash,
							       ip4h->saddr, ip4h->daddr,
							       udph->source, udph->dest);
	} else if (skb->protocol == htons(ETH_P_IPV6)) {
		struct ipv6hdr *ip6h = ipv6_hdr(skb);
		unsigned int hash;

		hash = quic_flow_hash_v6(&ip6h->saddr, &ip6h->daddr,
					 udph->source, udph->dest);

		quic_flow = bnxt_quic_flow_lookup_v6_with_hash(bp, hash,
							       &ip6h->saddr, &ip6h->daddr,
							       udph->source, udph->dest);
	}

	if (!quic_flow) {
		rcu_read_unlock();
		BNXT_QUIC_INC_TX_LOOKUP_FLOW_MISS(quic);
		goto exit;
	}

	key_phase = (quic_hdr_type & QUIC_KEY_PHASE_MASK) >> QUIC_KEY_PHASE_SFT;

	/* Verify key is valid for this phase */
	if (!quic_key_is_valid(quic_flow, TLS_OFFLOAD_CTX_DIR_TX, key_phase)) {
		rcu_read_unlock();
		BNXT_QUIC_INC_TX_LOOKUP_KEY_PHASE_MISS(quic);
		goto exit;
	}

	/* It's a short header packet to be encrypted
	 * Hardware will automatically set the fixed bit to 1
	 * during quic offload.
	 */
	*kid = READ_ONCE(quic_flow->kid[TLS_OFFLOAD_CTX_DIR_TX][key_phase]);

	rcu_read_unlock();

	*lflags |= cpu_to_le32(TX_BD_FLAGS_CRYPTO_EN |
			       BNXT_TX_KID_LO(*kid));
	BNXT_QUIC_INC_TX_HW_PKT(quic);
exit:
	return skb;
}

static int bnxt_quic_crypto_tx_add(struct bnxt *bp,
				   const struct bnxt_quic_connection_info *conn_info,
				   u32 kid, u8 key_phase)
{
	struct bnxt_mpc_info *mpc = bp->mpc_info;
	struct quic_ce_add_cmd cmd = {0};
	struct bnxt_tx_ring_info *txr;
	u32 data;

	if (!mpc)
		return -ENODEV;

	txr = &mpc->mpc_rings[BNXT_MPC_TCE_TYPE][0];

	data = CE_ADD_CMD_OPCODE_ADD | (BNXT_KID_HW(kid) << CE_ADD_CMD_KID_SFT) |
	       CE_ADD_CMD_VERSION_QUIC;

	switch (conn_info->cipher) {
	case TLS_CIPHER_AES_GCM_128:
		data |= CE_ADD_CMD_ALGORITHM_AES_GCM_128;
		memcpy(&cmd.session_key, conn_info->tx_data_key, sizeof(conn_info->tx_data_key));
		memcpy(&cmd.hp_key, conn_info->tx_hdr_key, sizeof(conn_info->tx_hdr_key));
		memcpy(&cmd.iv, conn_info->tx_iv, sizeof(conn_info->tx_iv));
		break;
	case TLS_CIPHER_AES_GCM_256:
		data |= CE_ADD_CMD_ALGORITHM_AES_GCM_256;
		memcpy(&cmd.session_key, conn_info->tx_data_key, sizeof(conn_info->tx_data_key));
		memcpy(&cmd.hp_key, conn_info->tx_hdr_key, sizeof(conn_info->tx_hdr_key));
		memcpy(&cmd.iv, conn_info->tx_iv, sizeof(conn_info->tx_iv));
		break;
	default:
		return -EINVAL;

	}

	cmd.ver_algo_kid_opcode = cpu_to_le32(data);
	cmd.pkt_number = cpu_to_le64(conn_info->pkt_number);

	data = QUIC_CE_ADD_CMD_CTX_KIND_CK_TX << QUIC_CE_ADD_CMD_CTX_KIND_SFT;
	data |= conn_info->dst_conn_id_width << QUIC_CE_ADD_CMD_DST_CID_SFT;
	data |= key_phase & QUIC_CE_ADD_CMD_DATA_MSG_KEY_PHASE;
	cmd.ctx_kind_dst_cid_width_key_phase = cpu_to_le32(data);
	return bnxt_xmit_crypto_cmd(bp, txr, &cmd, sizeof(cmd),
				    BNXT_MPC_TMO_MSECS, bp->quic_info);
}

static int bnxt_quic_crypto_rx_add(struct bnxt *bp,
				   const struct bnxt_quic_connection_info *conn_info,
				   u32 kid, u8 key_phase)
{
	struct bnxt_mpc_info *mpc = bp->mpc_info;
	struct quic_ce_add_cmd cmd = {0};
	struct bnxt_tx_ring_info *txr;
	u32 data = 0, data1 = 0;

	if (!mpc)
		return -ENODEV;

	txr = &mpc->mpc_rings[BNXT_MPC_RCE_TYPE][0];

	data = CE_ADD_CMD_OPCODE_ADD | (BNXT_KID_HW(kid) << CE_ADD_CMD_KID_SFT) |
	       CE_ADD_CMD_VERSION_QUIC;

	data1 = QUIC_CE_ADD_CMD_CTX_KIND_CK_RX << QUIC_CE_ADD_CMD_CTX_KIND_SFT;
	data1 |= conn_info->dst_conn_id_width << QUIC_CE_ADD_CMD_DST_CID_SFT;
	data1 |= key_phase & QUIC_CE_ADD_CMD_DATA_MSG_KEY_PHASE;

	switch (conn_info->cipher) {
	case TLS_CIPHER_AES_GCM_128:
		data |= CE_ADD_CMD_ALGORITHM_AES_GCM_128;
		break;

	case TLS_CIPHER_AES_GCM_256:
		data |= CE_ADD_CMD_ALGORITHM_AES_GCM_256;
		break;
	default:
		return -EINVAL;
	}
	cmd.ver_algo_kid_opcode = cpu_to_le32(data);
	cmd.ctx_kind_dst_cid_width_key_phase = cpu_to_le32(data1);
	memcpy(&cmd.session_key, conn_info->rx_data_key, sizeof(conn_info->rx_data_key));
	memcpy(&cmd.hp_key, conn_info->rx_hdr_key, sizeof(conn_info->rx_hdr_key));
	memcpy(&cmd.iv, conn_info->rx_iv, sizeof(conn_info->rx_iv));

	cmd.pkt_number = cpu_to_le64(conn_info->pkt_number);

	return bnxt_xmit_crypto_cmd(bp, txr, &cmd, sizeof(cmd),
				    BNXT_MPC_TMO_MSECS, bp->quic_info);
}

static int bnxt_quic_crypto_add(struct bnxt *bp,
				const struct bnxt_quic_connection_info *conn_info,
				enum tls_offload_ctx_dir dir, u32 kid,
				u8 key_phase)
{
	if (dir == TLS_OFFLOAD_CTX_DIR_RX)
		return bnxt_quic_crypto_rx_add(bp, conn_info, kid, key_phase);
	else
		return bnxt_quic_crypto_tx_add(bp, conn_info, kid, key_phase);
}

/* Build minimal temporary socket from connection_info for RX filter allocation.
 * This avoids allocating a socket in the common path (TX flows don't need it),
 * and only creates it when actually needed (RX flows only).
 */
static int bnxt_quic_hwrm_cfa_filter_alloc(struct bnxt *bp,
					   const struct bnxt_quic_connection_info *info,
					   u32 kid)
{
	struct inet_sock *inet_tmp;
	struct inet_sock *inet;
	struct sock *sk;
	int rc;

	/* Build minimal socket structure (allocated to avoid large stack frame) */
	inet_tmp = kzalloc(sizeof(*inet_tmp), GFP_KERNEL);
	if (!inet_tmp)
		return -ENOMEM;

	sk = (struct sock *)inet_tmp;
	sk->sk_protocol = IPPROTO_UDP;
	sk->sk_family = info->flow.daddr.ss_family;
	inet = inet_tmp;

	if (info->flow.daddr.ss_family == AF_INET) {
		struct sockaddr_in *src = (struct sockaddr_in *)&info->flow.saddr;
		struct sockaddr_in *dst = (struct sockaddr_in *)&info->flow.daddr;

		inet->inet_dport = dst->sin_port;
		inet->inet_sport = src->sin_port;
		inet->inet_daddr = dst->sin_addr.s_addr;
		inet->inet_saddr = src->sin_addr.s_addr;
	} else if (info->flow.daddr.ss_family == AF_INET6) {
		struct sockaddr_in6 *src_in6 = (struct sockaddr_in6 *)&info->flow.saddr;
		struct sockaddr_in6 *dst_in6 = (struct sockaddr_in6 *)&info->flow.daddr;

		inet->inet_dport = dst_in6->sin6_port;
		inet->inet_sport = src_in6->sin6_port;
		sk->sk_v6_daddr = dst_in6->sin6_addr;
		sk->sk_v6_rcv_saddr = src_in6->sin6_addr;
	}

	rc = bnxt_hwrm_cfa_tls_filter_alloc(bp, sk, kid, BNXT_CRYPTO_TYPE_QUIC,
					    info->rx_conn_id);
	kfree(inet_tmp);
	return rc;
}

static int bnxt_quic_crypto_del(struct bnxt *bp,
				enum tls_offload_ctx_dir direction, u32 kid)
{
	struct bnxt_mpc_info *mpc = bp->mpc_info;
	struct quic_ce_delete_cmd cmd = {0};
	struct bnxt_tx_ring_info *txr;
	u32 data;

	if (!mpc)
		return -ENODEV;

	if (test_bit(BNXT_STATE_IN_FW_RESET, &bp->state) &&
	    test_bit(BNXT_STATE_FW_FATAL_COND, &bp->state))
		return 0;

	if (direction == TLS_OFFLOAD_CTX_DIR_RX) {
		/* RX offload path untested and disabled
		 * so no flows can be added
		 */
		txr = &mpc->mpc_rings[BNXT_MPC_RCE_TYPE][0];
		data = CE_DELETE_CMD_CTX_KIND_QUIC_RX;
	} else {
		txr = &mpc->mpc_rings[BNXT_MPC_TCE_TYPE][0];
		data = CE_DELETE_CMD_CTX_KIND_QUIC_TX;
	}

	data |= CE_DELETE_CMD_OPCODE_DEL | (BNXT_KID_HW(kid) << CE_DELETE_CMD_KID_SFT);

	cmd.ctx_kind_kid_opcode = cpu_to_le32(data);
	return bnxt_xmit_crypto_cmd(bp, txr, &cmd, sizeof(cmd),
				    BNXT_MPC_TMO_MSECS, bp->quic_info);
}

static int _bnxt_quic_dev_add(struct bnxt *bp,
			      struct bnxt_quic_crypto_info *quic_flow,
			      const struct bnxt_quic_connection_info *conn_info,
			      enum tls_offload_ctx_dir direction,
			      u8 key_phase)
{
	struct bnxt_tls_info *quic;
	struct bnxt_kctx *kctx;
	u8 new_valid;
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
		atomic64_inc(&quic->counters[BNXT_QUIC_ERR_DEVICE_BUSY]);
		return -ENODEV;
	}

	/* RX offload path untested and disabled
	 * so no flows can be added
	 */
	if (direction == TLS_OFFLOAD_CTX_DIR_RX)
		kctx = &quic->rck;
	else
		kctx = &quic->tck;

	rc = bnxt_key_ctx_alloc_one(bp, kctx, &kid, BNXT_CRYPTO_TYPE_QUIC);
	if (rc) {
		atomic64_inc(&quic->counters[BNXT_QUIC_ERR_KEY_CTX_ALLOC]);
		goto exit;
	}

	rc = bnxt_quic_crypto_add(bp, conn_info, direction, kid, key_phase);
	if (rc) {
		atomic64_inc(&quic->counters[BNXT_QUIC_ERR_CRYPTO_CMD]);
		goto bnxt_quic_dev_add_err;
	}

	WRITE_ONCE(quic_flow->kid[direction][key_phase], kid);

	if (direction == TLS_OFFLOAD_CTX_DIR_RX) {
		rc = bnxt_quic_hwrm_cfa_filter_alloc(bp, conn_info, kid);
		if (rc) {
			atomic64_inc(&quic->counters[BNXT_QUIC_ERR_FILTER_ALLOC]);
			bnxt_quic_crypto_del(bp, direction, kid);
			goto bnxt_quic_dev_add_err;
		}
		atomic64_inc(&quic->counters[BNXT_QUIC_RX_ADD]);
	} else {
		atomic64_inc(&quic->counters[BNXT_QUIC_TX_ADD]);
	}

	/* Mark key as valid ONLY after all hardware operations succeed.
	 * This prevents race where TX path finds key marked valid before
	 * hardware is fully configured.
	 *
	 * Memory barrier is REQUIRED here because:
	 * 1. For NEW flows: Redundant with hash_add_rcu() but harmless
	 * 2. For EXISTING flows (adding 2nd key): Flow already published in hash,
	 *    being modified in-place. TX path can access concurrently under RCU.
	 *    Must ensure kid[] write visible before key_valid write.
	 *
	 * RCU only provides ordering at publish/dereference points, not for
	 * in-place modifications to already-published structures.
	 */
	smp_wmb();  /* Ensure KID and hardware setup visible before key_valid */
	new_valid = quic_flow->key_valid | quic_key_to_bit(direction, key_phase);
	WRITE_ONCE(quic_flow->key_valid, new_valid);

bnxt_quic_dev_add_err:
	if (rc) {
		bnxt_free_one_kctx(kctx, kid);
		atomic64_inc(&quic->counters[BNXT_QUIC_ERR_ADD_FLOW]);
	}
exit:
	atomic_dec(&quic->pending);
	return rc;
}

static void bnxt_quic_insert_hash(struct bnxt_quic_crypto_info *quic_flow)
{
	struct bnxt *bp = quic_flow->bp;
	struct bnxt_tls_info *quic;
	unsigned int hash;

	quic = bp->quic_info;

	hash = bnxt_quic_flow_key_hash(&quic_flow->flow_key);

	atomic64_inc(&quic->counters[BNXT_QUIC_ACTIVE_FLOWS]);
	hash_add_rcu(quic->quic_tx_fltr_tbl, &quic_flow->node, hash);
	if (!READ_ONCE(quic->quic_flows_active))
		WRITE_ONCE(quic->quic_flows_active, true);

	netdev_dbg(bp->dev, "Inserted new QUIC TX flow into database.\n");
}

static void bnxt_quic_remove_hash(struct bnxt_quic_crypto_info *quic_flow)
{
	struct bnxt *bp = quic_flow->bp;
	struct bnxt_tls_info *quic;

	quic = bp->quic_info;

	if (quic_flow->flow_key.family == AF_INET) {
		netdev_dbg(bp->dev,
			   "[IPv4] saddr: %pI4 daddr: %pI4 sport: %d dport: %d\n",
			   &quic_flow->flow_key.v4.saddr,
			   &quic_flow->flow_key.v4.daddr,
			   ntohs(quic_flow->flow_key.v4.sport),
			   ntohs(quic_flow->flow_key.v4.dport));
	} else if (quic_flow->flow_key.family == AF_INET6) {
		netdev_dbg(bp->dev,
			   "[IPv6] saddr: %pI6 daddr: %pI6 sport: %u dport: %u\n",
			   &quic_flow->flow_key.v6.saddr,
			   &quic_flow->flow_key.v6.daddr,
			   ntohs(quic_flow->flow_key.v6.sport),
			   ntohs(quic_flow->flow_key.v6.dport));
	}

	hash_del_rcu(&quic_flow->node);
	atomic64_dec(&quic->counters[BNXT_QUIC_ACTIVE_FLOWS]);
	if (atomic64_read(&quic->counters[BNXT_QUIC_ACTIVE_FLOWS]) == 0)
		WRITE_ONCE(quic->quic_flows_active, false);

	/* No synchronize_rcu() here. We defer cleanup to kfree_rcu callback.
	 * This allows the delete operation to return immediately
	 * instead of blocking.
	 */

	netdev_dbg(bp->dev, "Removed QUIC TX flow from database.\n");
}

/**
 * bnxt_quic_dev_add - Add one or more keys specified by key_mask
 * @quic_flow: flow structure (may be new or existing)
 * @conn_info: connection info with crypto material and key_mask
 * @is_new_flow: true if this is a newly allocated flow (needs hash insert)
 *
 * Iterates over each bit in conn_info->key_mask and programs the
 * corresponding direction+phase into hardware. All bits must refer
 * to the same phase (enforced by bnxt_quic_validate_flow).
 *
 * For new flows, the flow is inserted into the hash table after the
 * first key is successfully programmed. On any failure, previously
 * programmed keys in this call are NOT rolled back -- partial adds
 * leave the flow in a consistent state with those keys installed.
 *
 * Return: 0 on success, negative errno on failure
 */
static int bnxt_quic_dev_add(struct bnxt_quic_crypto_info *quic_flow,
			     const struct bnxt_quic_connection_info *conn_info,
			     bool is_new_flow)
{
	bool hash_inserted = false;
	u8 dir, phase;
	int rc;

	for (dir = 0; dir < BNXT_QUIC_NUM_DIRECTIONS; dir++) {
		for (phase = 0; phase < BNXT_QUIC_NUM_KEY_PHASES; phase++) {
			u8 key_bit = quic_key_to_bit(dir, phase);

			/* Skip if this direction+phase not requested */
			if (!(conn_info->key_mask & key_bit))
				continue;

			rc = _bnxt_quic_dev_add(quic_flow->bp, quic_flow,
						conn_info, dir, phase);
			if (rc)
				return rc;

			/* Insert into hash table after first successful key
			 * on a new flow. Subsequent keys reuse the entry.
			 */
			if (is_new_flow && !hash_inserted) {
				bnxt_quic_insert_hash(quic_flow);
				hash_inserted = true;
			}
		}
	}

	return 0;
}

#define QUIC_RETRY_MAX	100
static int _bnxt_quic_dev_del(struct bnxt *bp,
			      struct bnxt_quic_crypto_info *quic_flow,
			      enum tls_offload_ctx_dir dir,
			      u8 key_phase)
{
	struct bnxt_tls_info *quic;
	struct bnxt_kctx *kctx;
	int retry_cnt = 0;
	u32 kid;
	int rc;

	quic = bp->quic_info;

	kid = quic_flow->kid[dir][key_phase];

	kctx = (dir == TLS_OFFLOAD_CTX_DIR_RX) ? &quic->rck : &quic->tck;

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

	if (rc)
		atomic64_inc(&bp->quic_info->counters[BNXT_QUIC_ERR_DEL_FLOW]);
	return rc;
}

static int bnxt_quic_dev_del(struct bnxt_quic_crypto_info *quic_flow,
			     u8 dir,
			     u8 key_phase)
{
	u8 key_bit = quic_key_to_bit(dir, key_phase);
	struct bnxt *bp = quic_flow->bp;
	u8 new_valid;
	int rc;

	/* Validate that this key was actually added.
	 * Prevent deleting invalid keys, double-deletes, and hardware corruption.
	 */
	if (!quic_key_is_valid(quic_flow, dir, key_phase)) {
		netdev_dbg(bp->dev,
			   "Cannot delete key: dir=%u phase=%u not installed (key_valid=0x%x)\n",
			   dir, key_phase, READ_ONCE(quic_flow->key_valid));
		atomic64_inc(&bp->quic_info->counters[BNXT_QUIC_ERR_FLOW_NOT_FOUND]);
		return -ENOENT;
	}

	/* Mark key as invalid BEFORE deleting from hardware.
	 * This prevents race where TX path uses KID after it's been freed.
	 *
	 * Memory barrier is REQUIRED here because flow is already published
	 * in hash table and TX path can access it concurrently under RCU.
	 * Must ensure key_valid write visible before hardware deletion.
	 *
	 * Memory barrier ensures key_valid update visible before hardware operations.
	 */
	new_valid = quic_flow->key_valid & ~key_bit;
	WRITE_ONCE(quic_flow->key_valid, new_valid);
	smp_wmb();  /* Ensure key_valid update visible before hardware operations */

	/* Delete the specific key from hardware */
	rc = _bnxt_quic_dev_del(bp, quic_flow, dir, key_phase);

	if (dir == TLS_OFFLOAD_CTX_DIR_RX) {
		bnxt_hwrm_cfa_tls_filter_free(bp, quic_flow->kid[dir][key_phase],
					      BNXT_CRYPTO_TYPE_QUIC);
	}

	/* Only delete flow if ALL keys are gone */
	if (!quic_has_any_keys(quic_flow)) {
		/* Last key deleted - remove from hash table and free */
		bnxt_quic_remove_hash(quic_flow);
		kfree_rcu(quic_flow, rcu);
	}

	return rc;
}

/**
 * bnxt_quic_flush_flow_keys - Delete all keys for a single flow
 * @bp: pointer to bnxt device
 * @quic_flow: flow to delete keys from
 * @is_shutdown: true if called during device shutdown, false for ioctl
 *
 * Iterates through all possible keys (TX/RX × phase 0/1) for a flow
 * and deletes any that are currently installed.
 *
 * Context: Process context, called from flush with RTNL held
 */
static void bnxt_quic_flush_flow_keys(struct bnxt *bp,
				      struct bnxt_quic_crypto_info *quic_flow,
				      bool is_shutdown)
{
	struct bnxt_tls_info *quic = bp->quic_info;
	u8 dir, phase;

	/* Delete all keys in this flow */
	for (dir = 0; dir < BNXT_QUIC_NUM_DIRECTIONS; dir++) {
		for (phase = 0; phase < BNXT_QUIC_NUM_KEY_PHASES; phase++) {
			if (!quic_key_is_valid(quic_flow, dir, phase))
				continue;

			if (!is_shutdown) {
				/* Normal ioctl flush - use proper cleanup */
				int rc = _bnxt_quic_dev_del(bp, quic_flow, dir, phase);

				if (rc)
					netdev_dbg(bp->dev,
						   "Failed to flush: dir=%u phase=%u rc=%d\n",
						   dir,
						   phase, rc);
			} else {
				/* Shutdown path - direct hardware cleanup */
				u32 kid = quic_flow->kid[dir][phase];
				struct bnxt_kctx *kctx;

				kctx = (dir == TLS_OFFLOAD_CTX_DIR_RX) ? &quic->rck : &quic->tck;

				bnxt_quic_crypto_del(bp, dir, kid);
				bnxt_free_one_kctx(kctx, kid);
				if (dir == TLS_OFFLOAD_CTX_DIR_RX)
					bnxt_hwrm_cfa_tls_filter_free(bp, kid,
								      BNXT_CRYPTO_TYPE_QUIC);
			}
		}
	}
}

/**
 * bnxt_quic_flush_flows - Flush all QUIC flows
 * @bp: pointer to bnxt device
 * @is_shutdown: true if called during device shutdown, false for ioctl
 *
 * Removes all active QUIC flows and frees associated resources.
 * Uses a three-phase approach: clear active flag, wait for RCU grace period,
 * then directly clean up flows.
 *
 * Return: Number of flows flushed
 * Context: Process context (ioctl or shutdown)
 */
static int bnxt_quic_flush_flows(struct bnxt *bp, bool is_shutdown)
{
	struct bnxt_tls_info *quic = bp->quic_info;
	struct bnxt_quic_crypto_info *quic_flow;
	struct hlist_node *tmp;
	int bkt;
	int flushed = 0;

	if (!quic)
		return 0;

	/* Quick check if there are any flows */
	if (atomic64_read(&quic->counters[BNXT_QUIC_ACTIVE_FLOWS]) == 0)
		return 0;

	/* Clear active flag to stop new TX path lookups.
	 * After this, TX path will skip flow lookup entirely.
	 */
	WRITE_ONCE(quic->quic_flows_active, false);

	/* Wait for in-flight TX packets to complete.
	 * After this point, no readers can access the flow structures.
	 * The RTNL lock prevents concurrent add/del/flush ioctls.
	 * Therefore, we can safely traverse and delete without spinlock!
	 */
	synchronize_rcu();

	/* Traverse hash table and free all flows.
	 * NO SPINLOCK NEEDED because:
	 * 1. quic_flows_active = false → TX path won't find flows
	 * 2. synchronize_rcu() complete → No in-flight TX readers
	 * 3. RTNL lock held (ioctl) or shutdown path → No concurrent ioctls
	 */
	hash_for_each_safe(quic->quic_tx_fltr_tbl, bkt, tmp, quic_flow, node) {
		hash_del(&quic_flow->node);
		atomic64_dec(&quic->counters[BNXT_QUIC_ACTIVE_FLOWS]);
		flushed++;

		/* Delete all keys in this flow */
		bnxt_quic_flush_flow_keys(bp, quic_flow, is_shutdown);

		/* Free the flow structure */
		kfree(quic_flow);

		/* Yield CPU periodically during ioctl to remain responsive.
		 * Skip during shutdown - we want to complete cleanup quickly.
		 */
		if (!is_shutdown && (flushed % 128 == 0))
			cond_resched();
	}

	return flushed;
}

/**
 * bnxt_alloc_quic_info - Allocate and initialize QUIC offload context
 * @bp: pointer to bnxt device
 * @resp: pointer to firmware capability response
 *
 * Allocates the main QUIC info structure and initializes:
 * - Hash table for TX flow tracking
 * - Key context structures for TX and RX
 * - Atomic counters and per-CPU statistics
 * - Partition mode support if firmware supports it
 *
 * This function is called during device initialization when firmware
 * reports QUIC offload capability. If allocation fails, QUIC offload
 * will not be available but the device will still function.
 *
 * Context: Process context during device probe/initialization
 */
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

		/* Allocate per-CPU statistics for hot-path packet counters.
		 * This eliminates cache line bouncing on multi-core systems,
		 * significantly improving performance for packet processing.
		 */
		quic->percpu_stats = alloc_percpu(struct bnxt_tls_percpu_stats);
		if (!quic->percpu_stats) {
			kfree(quic->counters);
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

		hash_init(quic->quic_tx_fltr_tbl);

		atomic_set(&quic->pending, 0);

		bp->quic_info = quic;
	}
	quic->max_key_ctxs_alloc = max_keys;
}

/**
 * bnxt_clear_quic - Clear all QUIC key contexts
 * @bp: pointer to bnxt device
 *
 * Clears all key context allocations during shutdown or firmware reset.
 * Frees all key info structures and bitmaps, and increments the epoch
 * counter to invalidate any outstanding key references.
 *
 * This function mirrors bnxt_clear_ktls() and must be called alongside
 * it in all firmware reset and shutdown paths to ensure QUIC state is
 * properly cleaned up.
 *
 * This function assumes serialization (called during shutdown) and does
 * not use locking.
 *
 * Context: Process context during shutdown/reset
 */
void bnxt_clear_quic(struct bnxt *bp)
{
	struct bnxt_tls_info *quic = bp->quic_info;
	struct bnxt_quic_crypto_info *quic_flow;
	struct bnxt_kid_info *kid, *tmp;
	struct hlist_node *htmp;
	struct bnxt_kctx *kctx;
	int i, bkt;

	if (!quic)
		return;

	/* Purge all stale QUIC flows from the hash table (software only).
	 * After FW reset, firmware no longer knows about these flows.
	 * Send MPC commands to delete them will timeout -- just free the
	 * driver-side tracking structures.
	 */
	WRITE_ONCE(quic->quic_flows_active, false);
	hash_for_each_safe(quic->quic_tx_fltr_tbl, bkt, htmp, quic_flow, node) {
		hash_del(&quic_flow->node);
		atomic64_dec(&quic->counters[BNXT_QUIC_ACTIVE_FLOWS]);
		kfree(quic_flow);
	}

	/* Shutting down or FW reset, no need to protect the lists. */
	for (i = 0; i < BNXT_MAX_CRYPTO_KEY_TYPE; i++) {
		kctx = &quic->kctx[i];
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
 * bnxt_free_quic_info - Free QUIC offload resources
 * @bp: pointer to bnxt device
 *
 * Frees all resources associated with QUIC offload:
 * - Flush any remaining flows (hardware + software cleanup)
 * - Key context structures and bitmaps
 * - Hash table entries (should be empty)
 * - CFA filter table
 * - Crypto command context cache
 * - Per-CPU statistics
 * - Atomic counters
 * - Main QUIC info structure
 *
 * Called during device shutdown/removal. At this point, __bnxt_close_nic()
 * has already:
 * - Cleared BNXT_STATE_OPEN
 * - Called synchronize_net()
 * - Waited for bnxt_drv_busy() (which includes bnxt_quic_busy())
 *
 * So TX path is stopped and all pending operations are complete.
 *
 * Context: Process context during device shutdown/removal
 */
void bnxt_free_quic_info(struct bnxt *bp)
{
	struct bnxt_tls_info *quic = bp->quic_info;
	int flushed;

	if (!quic)
		return;

	/* Flush all remaining QUIC flows. */
	flushed = bnxt_quic_flush_flows(bp, true);
	if (flushed)
		netdev_dbg(bp->dev, "Flushed %d QUIC flows during shutdown\n", flushed);

	bnxt_clear_quic(bp);
	bnxt_clear_cfa_tls_filters_tbl(bp);
	kmem_cache_destroy(quic->mpc_cache);
	free_percpu(quic->percpu_stats);
	kfree(quic->counters);
	kfree(quic);
	bp->quic_info = NULL;
}

/**
 * bnxt_quic_init - Initialize QUIC offload hardware resources
 * @bp: pointer to bnxt device
 *
 * Allocates hardware resources for QUIC offload:
 * - TX and RX key contexts from firmware
 * - Crypto command context cache (with SLAB_HWCACHE_ALIGN)
 * - Partition mode configuration if supported
 *
 * This function is called after firmware resource reservation has been
 * completed. If initialization fails, QUIC offload will not be available
 * but the device will continue to function normally.
 *
 * The crypto command context cache is cache-aligned to prevent false
 * sharing and improve performance on multi-core systems.
 *
 * Return: 0 on success, negative error code on failure
 * Context: Process context during device initialization
 */
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
					    0, SLAB_HWCACHE_ALIGN, NULL);
	if (!quic->mpc_cache)
		return -ENOMEM;
	return 0;
}

/**
 * bnxt_quic_get_percpu_counter - Aggregate per-CPU counter values
 * @quic: pointer to QUIC info structure
 * @type: counter type to aggregate
 *
 * Aggregates per-CPU counter values for statistics reporting. This is
 * called from the slow path (stats queries) to sum up counter values
 * across all CPUs.
 *
 * For hot-path packet counters (TX_HW_PKT, RX_HW_PKT, etc.), we use
 * per-CPU storage to eliminate cache line bouncing during packet
 * processing. This function reads all per-CPU values and returns the sum.
 *
 * For control-path counters (not stored per-CPU), returns the atomic
 * counter value directly.
 *
 * Return: Aggregated counter value
 * Context: Any context (uses READ_ONCE for safe concurrent access)
 */
u64 bnxt_quic_get_percpu_counter(struct bnxt_tls_info *quic,
				 enum bnxt_quic_counters type)
{
	u64 total = 0;
	int cpu;

	if (!quic || !quic->percpu_stats)
		return 0;

	for_each_possible_cpu(cpu) {
		struct bnxt_tls_percpu_stats *stats;

		stats = per_cpu_ptr(quic->percpu_stats, cpu);

		switch (type) {
		case BNXT_QUIC_TX_HW_PKT:
			total += READ_ONCE(stats->tx_hw_pkt);
			break;
		case BNXT_QUIC_TX_LOOKUP_FLOW_MISS:
			total += READ_ONCE(stats->tx_lookup_flow_miss);
			break;
		case BNXT_QUIC_TX_LOOKUP_KEY_PHASE_MISS:
			total += READ_ONCE(stats->tx_lookup_key_phase_miss);
			break;
		case BNXT_QUIC_RX_HW_PKT:
			total += READ_ONCE(stats->rx_hw_pkt);
			break;
		case BNXT_QUIC_RX_PAYLOAD_DECRYPTED:
			total += READ_ONCE(stats->rx_payload_decrypted);
			break;
		case BNXT_QUIC_RX_HDR_DECRYPTED:
			total += READ_ONCE(stats->rx_hdr_decrypted);
			break;
		case BNXT_QUIC_RX_LONG_HDR:
			total += READ_ONCE(stats->rx_long_hdr);
			break;
		case BNXT_QUIC_RX_SHORT_HDR:
			total += READ_ONCE(stats->rx_short_hdr);
			break;
		case BNXT_QUIC_RX_KEY_PHASE_MISMATCH:
			total += READ_ONCE(stats->rx_key_phase_mismatch);
			break;
		case BNXT_QUIC_RX_RUNT:
			total += READ_ONCE(stats->rx_runt);
			break;
		default:
			/* For non-percpu counters, return atomic value */
			if (quic->counters)
				return atomic64_read(&quic->counters[type]);
			break;
		}
	}

	return total;
}

/**
 * bnxt_quic_rx - Process received QUIC packet metadata
 * @bp: pointer to bnxt device
 * @skb: socket buffer containing the packet
 * @data_ptr: pointer to packet data
 * @len: length of packet data
 * @rxcmp: RX completion descriptor
 * @rxcmp1: Extended RX completion descriptor
 *
 * Processes QUIC metadata from hardware for received packets. Updates
 * per-CPU statistics for various QUIC packet types and decryption status.
 *
 * This function runs in NAPI (softirq) context and is highly optimized:
 * - No locks (per-CPU counters only)
 * - Simple flag checks
 * - Minimal cache footprint
 *
 * Hardware provides metadata indicating:
 * - Payload/header decryption status
 * - Long vs short header packets
 * - Key phase mismatches
 * - Runt packets
 *
 * Context: NAPI/softirq context (RX path)
 */
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
		BNXT_QUIC_INC_RX_PAYLOAD_DECRYPTED(quic);
	if (qmd & QUIC_METADATA_MSG_FLAGS_HDR_DECRYPTED)
		BNXT_QUIC_INC_RX_HDR_DECRYPTED(quic);
	if (qmd & QUIC_METADATA_MSG_FLAGS_HEADER_TYPE)
		BNXT_QUIC_INC_RX_LONG_HDR(quic);
	else
		BNXT_QUIC_INC_RX_SHORT_HDR(quic);
	if (qmd & QUIC_METADATA_MSG_FLAGS_KEY_PHASE_MISMATCH)
		BNXT_QUIC_INC_RX_KEY_PHASE_MISMATCH(quic);
	if (qmd & QUIC_METADATA_MSG_FLAGS_RUNT)
		BNXT_QUIC_INC_RX_RUNT(quic);

	BNXT_QUIC_INC_RX_HW_PKT(quic);
}

/**
 * bnxt_quic_validate_flow_info - Validate common flow identification fields
 * @bp: device handle
 * @flow_info: flow info with addresses to validate
 *
 * Validates that the address families match and are supported.
 * Shared between add and delete validation.
 *
 * Return: 0 on success, negative error code on failure
 */
static int bnxt_quic_validate_flow_info(struct bnxt *bp,
					const struct bnxt_quic_flow_info *flow_info)
{
	/* Validate address families match */
	if (flow_info->daddr.ss_family != flow_info->saddr.ss_family) {
		netdev_dbg(bp->dev,
			   "Mismatched address families! daddr: %u saddr: %u\n",
			   flow_info->daddr.ss_family, flow_info->saddr.ss_family);
		return -EINVAL;
	}

	/* Validate address family is supported (AF_INET or AF_INET6) */
	if (flow_info->daddr.ss_family != AF_INET &&
	    flow_info->daddr.ss_family != AF_INET6) {
		netdev_dbg(bp->dev, "Invalid destination address family %u!\n",
			   flow_info->daddr.ss_family);
		return -EINVAL;
	}

	return 0;
}

/* Validate QUIC flow connection info for add operations */
static int bnxt_quic_validate_flow(struct bnxt *bp, struct bnxt_quic_connection_info *info)
{
	u8 phase0_bits, phase1_bits;
	int rc;

	rc = bnxt_quic_validate_flow_info(bp, &info->flow);
	if (rc)
		return rc;

	/* Validate key_mask has at least one bit set */
	if (info->key_mask == 0) {
		netdev_dbg(bp->dev, "Invalid key_mask 0x%x: no keys specified\n",
			   info->key_mask);
		return -EINVAL;
	}

	/* Validate key_mask has no undefined bits */
	if (info->key_mask & ~BNXT_QUIC_KEY_ALL) {
		netdev_dbg(bp->dev, "Invalid key_mask 0x%x: undefined bits set\n",
			   info->key_mask);
		return -EINVAL;
	}

	/* Validate all bits in key_mask refer to the same key phase.
	 * A single ADD carries one set of TX keys and one set of RX keys,
	 * both for the same phase. Mixing phases in one call is not allowed.
	 */
	phase0_bits = info->key_mask & (BNXT_QUIC_KEY_RX_PHASE_0 |
					BNXT_QUIC_KEY_TX_PHASE_0);
	phase1_bits = info->key_mask & (BNXT_QUIC_KEY_RX_PHASE_1 |
					BNXT_QUIC_KEY_TX_PHASE_1);
	if (phase0_bits && phase1_bits) {
		netdev_dbg(bp->dev,
			   "Invalid key_mask 0x%x: cannot mix phases in one ADD\n",
			   info->key_mask);
		return -EINVAL;
	}

	/* RX offload not yet supported - reject any RX bits */
	if (info->key_mask & BNXT_QUIC_KEY_RX_ALL) {
		netdev_dbg(bp->dev,
			   "RX offload not supported (key_mask 0x%x)\n",
			   info->key_mask);
		return -EOPNOTSUPP;
	}

	/* Validate the destination Connection ID width.
	 * Hardware supports 0 or 8 bytes for both TX and RX flows.
	 */
	if (info->dst_conn_id_width != 0 && info->dst_conn_id_width != 8) {
		netdev_dbg(bp->dev,
			   "Invalid Destination Connection ID width %u! Expected 0 or 8.\n",
			   info->dst_conn_id_width);
		return -EINVAL;
	}

	/* Validate cipher */
	if (info->cipher != TLS_CIPHER_AES_GCM_128 &&
	    info->cipher != TLS_CIPHER_AES_GCM_256) {
		netdev_dbg(bp->dev, "Unsupported cipher suite 0x%x!\n",
			   info->cipher);
		return -EINVAL;
	}

	return 0;
}

/**
 * bnxt_quic_validate_flow_del - Validate delete parameters
 * @bp: device handle
 * @del_info: delete info to validate
 *
 * Validates the flow identification (addresses) and the key_mask.
 * key_mask must have at least one bit set and no invalid bits.
 *
 * Return: 0 on success, negative error code on failure
 */
static int bnxt_quic_validate_flow_del(struct bnxt *bp,
				       struct bnxt_quic_flow_del_info *del_info)
{
	int rc;

	rc = bnxt_quic_validate_flow_info(bp, &del_info->flow);
	if (rc)
		return rc;

	/* Validate key_mask has at least one bit set */
	if (del_info->key_mask == 0) {
		netdev_dbg(bp->dev, "Invalid key_mask 0x%x: no keys specified\n",
			   del_info->key_mask);
		return -EINVAL;
	}

	/* Validate key_mask has no invalid bits */
	if (del_info->key_mask & ~BNXT_QUIC_KEY_ALL) {
		netdev_dbg(bp->dev, "Invalid key_mask 0x%x: undefined bits set\n",
			   del_info->key_mask);
		return -EINVAL;
	}

	return 0;
}

static int bnxt_quic_ctrl_flow_add(struct bnxt *bp,
				   void __user *useraddr)
{
	struct bnxt_quic_crypto_info *quic_flow, *tmp_flow;
	struct bnxt_quic_connection_info conn_info;
	unsigned int hash;
	int ret;

	/* Copy connection info from userspace */
	if (copy_from_user(&conn_info, useraddr, sizeof(conn_info)))
		return -EFAULT;

	/* Validate all flow parameters */
	ret = bnxt_quic_validate_flow(bp, &conn_info);
	if (ret) {
		atomic64_inc(&bp->quic_info->counters[BNXT_QUIC_ERR_INVALID_PARAM]);
		return ret;
	}

	hash = bnxt_quic_flow_info_hash(&conn_info.flow);

	/* Check if flow with same 5-tuple already exists */
	rcu_read_lock();

	hash_for_each_possible_rcu(bp->quic_info->quic_tx_fltr_tbl, tmp_flow, node, hash) {
		bool port_match = false;
		bool ip_match = false;

		/* Check family match first */
		if (tmp_flow->flow_key.family != conn_info.flow.daddr.ss_family)
			continue;

		if (conn_info.flow.daddr.ss_family == AF_INET) {
			struct sockaddr_in *src1 = (struct sockaddr_in *)&conn_info.flow.saddr;
			struct sockaddr_in *dst1 = (struct sockaddr_in *)&conn_info.flow.daddr;

			ip_match = (src1->sin_addr.s_addr == tmp_flow->flow_key.v4.saddr &&
				    dst1->sin_addr.s_addr == tmp_flow->flow_key.v4.daddr);
			port_match = (src1->sin_port == tmp_flow->flow_key.v4.sport &&
				      dst1->sin_port == tmp_flow->flow_key.v4.dport);
		} else if (conn_info.flow.daddr.ss_family == AF_INET6) {
			struct sockaddr_in6 *src1 = (struct sockaddr_in6 *)&conn_info.flow.saddr;
			struct sockaddr_in6 *dst1 = (struct sockaddr_in6 *)&conn_info.flow.daddr;

			ip_match = (!memcmp(&src1->sin6_addr, &tmp_flow->flow_key.v6.saddr,
				    sizeof(struct in6_addr)) &&
				    !memcmp(&dst1->sin6_addr, &tmp_flow->flow_key.v6.daddr,
				    sizeof(struct in6_addr)));
			port_match = (src1->sin6_port == tmp_flow->flow_key.v6.sport &&
				      dst1->sin6_port == tmp_flow->flow_key.v6.dport);
		}

		if (ip_match && port_match) {
			/* Found existing flow with same 5-tuple.
			 * Check if ANY requested key already exists.
			 */
			if (tmp_flow->key_valid & conn_info.key_mask) {
				/* At least one requested key is already installed */
				rcu_read_unlock();
				atomic64_inc
				  (&bp->quic_info->counters[BNXT_QUIC_ERR_DUPLICATE_FLOW]);
				return -EEXIST;
			}

			/* Flow exists but none of the requested keys do */
			rcu_read_unlock();

			/* Add key(s) to existing flow (don't insert into hash) */
			ret = bnxt_quic_dev_add(tmp_flow, &conn_info, false);
			return ret;
		}
	}

	rcu_read_unlock();

	/* No existing flow - allocate a new flow */
	quic_flow = kzalloc(sizeof(*quic_flow), GFP_KERNEL);
	if (!quic_flow) {
		atomic64_inc(&bp->quic_info->counters[BNXT_QUIC_ERR_NO_MEM]);
		return -ENOMEM;
	}

	/* Initialize new flow */
	quic_flow->bp = bp;
	bnxt_quic_copy_to_flow_key(&quic_flow->flow_key, &conn_info.flow);

	ret = bnxt_quic_dev_add(quic_flow, &conn_info, true);

	if (ret)
		kfree(quic_flow);

	/* coverity[RESOURCE_LEAK:SUPPRESS] */
	return ret;
}

static int bnxt_quic_ctrl_flow_del(struct bnxt *bp,
				   void __user *useraddr)
{
	struct bnxt_quic_crypto_info *quic_flow = NULL;
	struct bnxt_quic_flow_del_info del_info;
	unsigned int hash;
	int deleted = 0;
	u8 dir, phase;
	int ret = 0;

	if (copy_from_user(&del_info, useraddr, sizeof(del_info)))
		return -EFAULT;

	/* Validate delete parameters: addresses and key_mask */
	ret = bnxt_quic_validate_flow_del(bp, &del_info);
	if (ret) {
		atomic64_inc(&bp->quic_info->counters[BNXT_QUIC_ERR_INVALID_PARAM]);
		return ret;
	}

	hash = bnxt_quic_flow_info_hash(&del_info.flow);

	/* Look up the flow by hash/address/port */
	if (del_info.flow.daddr.ss_family == AF_INET) {
		struct sockaddr_in *src = (struct sockaddr_in *)&del_info.flow.saddr;
		struct sockaddr_in *dst = (struct sockaddr_in *)&del_info.flow.daddr;

		quic_flow = bnxt_quic_flow_lookup_v4_with_hash(bp, hash,
							       src->sin_addr.s_addr,
							       dst->sin_addr.s_addr,
							       src->sin_port,
							       dst->sin_port);
	} else {
		struct sockaddr_in6 *src = (struct sockaddr_in6 *)&del_info.flow.saddr;
		struct sockaddr_in6 *dst = (struct sockaddr_in6 *)&del_info.flow.daddr;

		quic_flow = bnxt_quic_flow_lookup_v6_with_hash(bp, hash,
							       &src->sin6_addr,
							       &dst->sin6_addr,
							       src->sin6_port,
							       dst->sin6_port);
	}

	if (!quic_flow) {
		netdev_dbg(bp->dev, "Flow not present in database!\n");
		atomic64_inc(&bp->quic_info->counters[BNXT_QUIC_ERR_FLOW_NOT_FOUND]);
		return -ENOENT;
	}

	/* Delete each key specified in key_mask.
	 * Iterate over all direction/phase combinations and delete those
	 * that are both requested (in key_mask) and installed (in key_valid).
	 *
	 * Note: bnxt_quic_dev_del() will free the flow via kfree_rcu() when
	 * the last key is deleted. We must detect this case and stop iterating
	 * to avoid accessing freed memory.
	 */
	for (dir = 0; dir < BNXT_QUIC_NUM_DIRECTIONS; dir++) {
		for (phase = 0; phase < BNXT_QUIC_NUM_KEY_PHASES; phase++) {
			u8 key_bit = quic_key_to_bit(dir, phase);
			u8 remaining;

			/* Skip if this key not requested for deletion */
			if (!(del_info.key_mask & key_bit))
				continue;

			/* Skip if this key is not installed */
			if (!quic_key_is_valid(quic_flow, dir, phase))
				continue;

			/* Check if this is the last installed key.
			 * After bnxt_quic_dev_del removes it, the flow
			 * will be freed and the pointer becomes invalid.
			 */
			remaining = READ_ONCE(quic_flow->key_valid) & ~key_bit;

			ret = bnxt_quic_dev_del(quic_flow, dir, phase);
			if (ret) {
				netdev_dbg(bp->dev,
					   "Failed to delete key dir=%u phase=%u: %d\n",
					   dir, phase, ret);
				return ret;
			}
			deleted++;

			/* If that was the last key, the flow has been freed.
			 * Stop iterating -- quic_flow pointer is no longer valid.
			 */
			if (!remaining)
				return 0;
		}
	}

	if (deleted == 0) {
		netdev_dbg(bp->dev,
			   "No matching keys found: key_mask=0x%x key_valid=0x%x\n",
			   del_info.key_mask, READ_ONCE(quic_flow->key_valid));
		atomic64_inc(&bp->quic_info->counters[BNXT_QUIC_ERR_FLOW_NOT_FOUND]);
		return -ENOENT;
	}

	return 0;
}

static int bnxt_quic_ctrl_flow_flush(struct bnxt *bp)
{
	int flushed;

	if (!bp->quic_info)
		return -EOPNOTSUPP;

	flushed = bnxt_quic_flush_flows(bp, false);
	netdev_dbg(bp->dev, "Flushed %d QUIC flows\n", flushed);

	return 0;
}

/**
 * bnxt_quic_del_all - Delete all QUIC flows and keys from hardware
 * @bp: pointer to bnxt device
 *
 * Deletes all active QUIC flows and their associated hardware key
 * contexts via the MPC ring.  Must be called while the MPC ring is
 * still alive (i.e. before bnxt_close_nic tears it down).
 *  bnxt_quic_ctrl_flow_flush
 * This mirrors bnxt_ktls_del_all() and must be called alongside it
 * in the bnxt_close() path to ensure firmware crypto contexts and
 * CFA filters are properly released.
 *
 * Context: Process context, RTNL lock held
 */
void bnxt_quic_del_all(struct bnxt *bp)
{
	(void)bnxt_quic_ctrl_flow_flush(bp);
}

/**
 * bnxt_siocdevprivate - Handle QUIC private ioctls
 * @dev: network device
 * @ifr: interface request structure
 * @useraddr: user space address for data
 * @cmd: ioctl command
 *
 * Handles private ioctl commands for QUIC offload:
 * - SIOCDEVQUICFLOWADD: Add a QUIC FLOW
 * - SIOCDEVQUICFLOWDEL: Delete a QUIC FLOW
 * - SIOCDEVQUICFLOWFLUSH: Flush all QUIC FLOWS
 *
 * Return: 0 on success, negative error code on failure
 * Context: Process context
 */
int bnxt_siocdevprivate(struct net_device *dev, struct ifreq *ifr,
			void __user *useraddr, int cmd)
{
	struct bnxt *bp = netdev_priv(dev);
	struct bnxt_tls_info *quic;

	quic = bp->quic_info;
	if (!quic) {
		netdev_dbg(bp->dev, "QUIC info not allocated!\n");
		return -EOPNOTSUPP;
	}

	switch (cmd) {
	case SIOCDEVQUICFLOWADD:
		return bnxt_quic_ctrl_flow_add(bp, useraddr);
	case SIOCDEVQUICFLOWDEL:
		return bnxt_quic_ctrl_flow_del(bp, useraddr);
	case SIOCDEVQUICFLOWFLUSH:
		return bnxt_quic_ctrl_flow_flush(bp);
	default:
		return -EOPNOTSUPP;
	}
}

#endif /* HAVE_BNXT_QUIC */
