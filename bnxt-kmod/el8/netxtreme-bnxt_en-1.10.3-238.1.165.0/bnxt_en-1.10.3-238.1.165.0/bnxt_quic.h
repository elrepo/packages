/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
 * Broadcom NetXtreme-C/E network driver.
 *
 * Copyright (c) 2024 Broadcom Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 */

#ifndef BNXT_QUIC_H
#define BNXT_QUIC_H

#include <linux/bnxt_quic_usr_include.h>
#include "bnxt_ulp.h"

/* QUIC offload uses separate, independent KID pools from kTLS.
 * Hardware maintains distinct resource pools for QUIC (524K) and kTLS (204K).
 * These limits are independent - QUIC can use 524K keys while kTLS uses 204K simultaneously.
 */
#define BNXT_MAX_QUIC_TX_CRYPTO_KEYS		524300
#define BNXT_MAX_QUIC_RX_CRYPTO_KEYS		524300

enum bnxt_quic_counters {
	/* Atomic counters (control path and errors) */

	/* Active flows counters (atomic) */
	BNXT_QUIC_ACTIVE_FLOWS = 0,	/* Active flows (increments on add, decrements on delete) */

	/* TX flow control counters (atomic) */
	BNXT_QUIC_TX_ADD,		/* TX flow added */
	BNXT_QUIC_TX_DEL,		/* TX flow deleted */

	/* RX flow control counters (atomic) */
	BNXT_QUIC_RX_ADD,		/* RX flow added */
	BNXT_QUIC_RX_DEL,		/* RX flow deleted */

	/* Error counters for debugging */
	BNXT_QUIC_ERR_ADD_FLOW,		/* Add flow failed */
	BNXT_QUIC_ERR_DEL_FLOW,		/* Delete flow failed */
	BNXT_QUIC_ERR_DUPLICATE_FLOW,	/* Duplicate flow (application tried to add duplicate) */
	BNXT_QUIC_ERR_NO_MEM,		/* Memory allocation (no memory for flow allocation) */
	BNXT_QUIC_ERR_KEY_CTX_ALLOC,	/* Key context allocation (KID pool exhausted) */
	BNXT_QUIC_ERR_CRYPTO_CMD,	/* Crypto command (firmware rejected operation) */
	BNXT_QUIC_ERR_FILTER_ALLOC,	/* Filter allocation (RX filter table full) */
	BNXT_QUIC_ERR_INVALID_PARAM,	/* Invalid param (bad cipher, addresses, etc.) */
	BNXT_QUIC_ERR_DEVICE_BUSY,	/* Device busy (not open) */
	BNXT_QUIC_ERR_FLOW_NOT_FOUND,	/* Flow not found (non-existent flow) */

	/* Marker for first per-CPU counter */
	BNXT_QUIC_PERCPU_START,		/* Per-CPU counter start index */

	/* Per-CPU counters (hot data path) */

	/* TX per-CPU counters */
	BNXT_QUIC_TX_HW_PKT = BNXT_QUIC_PERCPU_START,	/* TX packets encrypted by hardware */
	BNXT_QUIC_TX_LOOKUP_FLOW_MISS,	/* Flow lookup miss in TX path */
	BNXT_QUIC_TX_LOOKUP_KEY_PHASE_MISS,	/* Flow lookup miss due to key phase mismatch */

	/* RX per-CPU counters */
	BNXT_QUIC_RX_HW_PKT,		/* RX packets processed by hardware */
	BNXT_QUIC_RX_PAYLOAD_DECRYPTED,	/* RX packets with payload successfully decrypted */
	BNXT_QUIC_RX_HDR_DECRYPTED,	/* RX packets with header successfully decrypted */
	BNXT_QUIC_RX_KEY_PHASE_MISMATCH,	/* RX packets with key phase mismatch */
	BNXT_QUIC_RX_RUNT,		/* RX packets that are runt (too small for valid QUIC) */
	BNXT_QUIC_RX_SHORT_HDR,		/* RX packets that are short header */
	BNXT_QUIC_RX_LONG_HDR,		/* RX packets that are long header */

	BNXT_QUIC_MAX_COUNTERS		/* Maximum number of counters */
};

#define BNXT_QUIC_NUM_KEY_PHASES 2 /* Phase 0 and Phase 1 */
#define BNXT_QUIC_NUM_DIRECTIONS 2 /* TX and RX */
#define BNXT_QUIC_NUM_KEYS (BNXT_QUIC_NUM_DIRECTIONS * BNXT_QUIC_NUM_KEY_PHASES)

/**
 * struct bnxt_quic_flow_key - Minimal 5-tuple for flow lookup
 * @ipv4/ipv6: Source and destination IP addresses
 * @sport: Source port
 * @dport: Destination port
 * @family: Address family (AF_INET or AF_INET6)
 *
 * This structure contains only the information needed for hash table
 * lookup.
 */
struct bnxt_quic_flow_key {
	union {
		struct {
			__be32 saddr;
			__be32 daddr;
			__be16 sport;
			__be16 dport;
		} v4;
		struct {
			struct in6_addr saddr;
			struct in6_addr daddr;
			__be16 sport;
			__be16 dport;
		} v6;
	};
	u8 family;  /* AF_INET or AF_INET6 */
} __packed;

/**
 * struct bnxt_quic_crypto_info - Per-flow QUIC offload state
 * @bp: Back pointer to device
 * @node: Hash table linkage
 * @flow_key: Minimal 5-tuple for lookup (replaces full connection_info)
 * @kid: Hardware key IDs [direction][key_phase]
 * @key_valid: Bitmask of installed keys
 * @rcu: RCU cleanup
 */
struct bnxt_quic_crypto_info {
	struct bnxt *bp;
	struct hlist_node	node;
	struct bnxt_quic_flow_key flow_key;
	u32 kid[BNXT_QUIC_NUM_DIRECTIONS][BNXT_QUIC_NUM_KEY_PHASES];
	u8 key_valid;
	#define QUIC_KEY_RX_PHASE_0  BIT(0)        /* 0x01 */
	#define QUIC_KEY_RX_PHASE_1  BIT(1)        /* 0x02 */
	#define QUIC_KEY_TX_PHASE_0  BIT(2)        /* 0x04 */
	#define QUIC_KEY_TX_PHASE_1  BIT(3)        /* 0x08 */
	struct rcu_head		rcu;		/* For asynchronous RCU cleanup */
};

struct quic_ce_add_cmd {
	__le32	ver_algo_kid_opcode;
	#define CE_ADD_CMD_VERSION_QUIC			(0x4UL << 28)
	__le32	ctx_kind_dst_cid_width_key_phase;
	#define QUIC_CE_ADD_CMD_DATA_MSG_KEY_PHASE	1
	#define QUIC_CE_ADD_CMD_DST_CID_SFT		1
	#define QUIC_CE_ADD_CMD_CTX_KIND_SFT		6
	#define QUIC_CE_ADD_CMD_CTX_KIND_CK_TX		0x14UL
	#define QUIC_CE_ADD_CMD_CTX_KIND_CK_RX		0x15UL
	u8	unused1[8];
	u8	iv[12];
	u8	unused2[4];
	u8	session_key[32];
	u8	hp_key[32];
	__le64	pkt_number;
};

struct quic_ce_delete_cmd {
	__le32  ctx_kind_kid_opcode;
	#define CE_DELETE_CMD_OPCODE_MASK		0xfUL
	#define CE_DELETE_CMD_OPCODE_SFT		0
	#define CE_DELETE_CMD_OPCODE_DEL		0x2UL
	#define CE_DELETE_CMD_KID_MASK			0xfffff0UL
	#define CE_DELETE_CMD_KID_SFT			4
	#define CE_DELETE_CMD_CTX_KIND_MASK		0x1f000000UL
	#define CE_DELETE_CMD_CTX_KIND_SFT		24
	#define CE_DELETE_CMD_CTX_KIND_CK_TX		(0x11UL << 24)
	#define CE_DELETE_CMD_CTX_KIND_CK_RX		(0x12UL << 24)
	#define CE_DELETE_CMD_CTX_KIND_QUIC_TX		(0x14UL << 24)
	#define CE_DELETE_CMD_CTX_KIND_QUIC_RX		(0x15UL << 24)
};

/* quic_metadata_msg (size:256b/32B) */
struct quic_metadata_msg {
	u32	md_type_link_flags_kid_lo;
	/* This field classifies the data present in the meta-data. */
	#define QUIC_METADATA_MSG_MD_TYPE_MASK			0x1fUL
	#define QUIC_METADATA_MSG_MD_TYPE_SFT			0
	/* This setting is used for QUIC packets. */
	#define QUIC_METADATA_MSG_MD_TYPE_QUIC			0x3UL
	#define QUIC_METADATA_MSG_MD_TYPE_LAST \
		QUIC_METADATA_MSG_MD_TYPE_QUIC
	/*
	 * This field indicates where the next metadata block starts. It is
	 * counted in 16B units. A value of zero indicates that there is no
	 * metadata.
	 */
	#define QUIC_METADATA_MSG_LINK_MASK                     0x1e0UL
	#define QUIC_METADATA_MSG_LINK_SFT                      5
	/* These are flags present in the metadata. */
	#define QUIC_METADATA_MSG_FLAGS_MASK			0x1fffe00UL
	#define QUIC_METADATA_MSG_FLAGS_SFT                     9
	/*
	 * A value of 1 implies that the packet was decrypted by HW. Otherwise
	 * the packet is passed on as it came in on the wire.
	 */
	#define QUIC_METADATA_MSG_FLAGS_PAYLOAD_DECRYPTED	0x200UL
	/*
	 * A value of 1 indicates that the header was decrypted by HW. Since
	 * there are cases where the header is decrypted but the payload is
	 * not, separate bits are provided. There will never be a case where
	 * the header was not decrypted and the payload was decrypted.
	 */
	#define QUIC_METADATA_MSG_FLAGS_HDR_DECRYPTED		0x400UL
	/*
	 * A value of 1 indicates that the PN decoding algorithm resulted in
	 * a PN that underflowed the lower limit of the PN window and was
	 * adjusted by adding a full window size.
	 */
	#define QUIC_METADATA_MSG_FLAGS_PN_UNDERFLOW_WINDOW	0x800UL
	/*
	 * A value of 1 indicates that the PN decoding algorithm resulted in
	 * a PN that overflowed the upper limit of the PN window and was
	 * adjusted by subtracting a full window size.
	 */
	#define QUIC_METADATA_MSG_FLAGS_PN_OVERFLOW_WINDOW	0x1000UL
	/* This field indicates the status of tag authentication. */
	#define QUIC_METADATA_MSG_FLAGS_TAG_AUTH_STATUS_MASK	0x6000UL
	#define QUIC_METADATA_MSG_FLAGS_TAG_AUTH_STATUS_SFT      13
	/*
	 * This enumeration is set when there is no tags present in the
	 * packet.
	 */
	#define QUIC_METADATA_MSG_FLAGS_TAG_AUTH_STATUS_NONE	(0x0UL << 13)
	/*
	 * This enumeration states that there is at least one tag in the
	 * packet and every tag is valid.
	 */
	#define QUIC_METADATA_MSG_FLAGS_TAG_AUTH_STATUS_SUCCESS	(0x1UL << 13)
	/*
	 * This enumeration states that there is at least one tag in the
	 * packet and at least one of the tag is invalid. The entire packet
	 * is sent decrypted to the host.
	 */
	#define QUIC_METADATA_MSG_FLAGS_TAG_AUTH_STATUS_FAILURE \
		(0x2UL << 13)
	#define QUIC_METADATA_MSG_FLAGS_TAG_AUTH_STATUS_LAST \
		QUIC_METADATA_MSG_FLAGS_TAG_AUTH_STATUS_FAILURE
	/*
	 * Short header packet number size 0: 8-bits 1: 16-bits 2: 24-bits
	 * 3: 32-bits
	 */
	#define QUIC_METADATA_MSG_FLAGS_PN_SIZE_MASK		0x18000UL
	#define QUIC_METADATA_MSG_FLAGS_PN_SIZE_SFT		15
	/*
	 * A value of 1 indicates that the packet experienced a context
	 * load error. In this case, the packet is sent to the host without
	 * the header or payload decrypted and the context is not updated.
	 */
	#define QUIC_METADATA_MSG_FLAGS_CTX_LOAD_ERR		0x20000UL
	/*
	 * A value of 1 indicates that the packet was a runt (i.e. <21B).
	 * In this case, the packet is sent to the host without the header
	 * or payload decrypted and the context is not updated.
	 */
	#define QUIC_METADATA_MSG_FLAGS_RUNT			0x40000UL
	/*
	 * A value of 1 indicates that a key phase mismatch was detected.
	 * In this case, the packet is sent to the host without the payload
	 * decrypted, the header is decrypted and the context is not
	 * updated.
	 */
	#define QUIC_METADATA_MSG_FLAGS_KEY_PHASE_MISMATCH	0x80000UL
	/* QUIC header type 0: Short header type 1: Long header type */
	#define QUIC_METADATA_MSG_FLAGS_HEADER_TYPE		0x100000UL
	/*
	 * This value indicates the lower 7-bit of the Crypto Key ID
	 * associated with this operation.
	 */
	#define QUIC_METADATA_MSG_KID_LO_MASK			0xfe000000UL
	#define QUIC_METADATA_MSG_KID_LO_SFT			25
	u16	kid_hi;
	/*
	 * This value indicates the upper 13-bit of the Crypto Key ID
	 * associated with this operation.
	 */
	#define QUIC_METADATA_MSG_KID_HI_MASK			0x1fffUL
	#define QUIC_METADATA_MSG_KID_HI_SFT			0
	/* This field is unused in this context. */
	u16	metadata_0;
	u64	packet_num;
	/*
	 * This is the QUIC packet number that was processed by the HW.
	 * It is in little endian format.
	 */
	#define QUIC_METADATA_MSG_PACKET_NUM_MASK		0x3fffffffffffffffUL
	#define QUIC_METADATA_MSG_PACKET_NUM_SFT		0
	/* This field is unused in this context. */
	u64	metadata_2;
	/* This field is unused in this context. */
	u64	metadata_3;
};

static inline u8 quic_key_to_bit(u8 dir, u8 key_phase)
{
	return BIT((dir * BNXT_QUIC_NUM_KEY_PHASES) + key_phase);
}

static inline bool quic_key_is_valid(struct bnxt_quic_crypto_info *flow,
				     u8 dir, u8 key_phase)
{
	return !!(READ_ONCE(flow->key_valid) & quic_key_to_bit(dir, key_phase));
}

static inline bool quic_has_any_keys(struct bnxt_quic_crypto_info *flow)
{
	return (READ_ONCE(flow->key_valid) != 0);
}

/**
 * bnxt_quic_copy_to_flow_key - Copy 5-tuple from flow_info to compressed flow_key
 * @flow_key: Destination compressed flow key
 * @flow_info: Source flow info containing addresses and ports
 *
 * Extracts only the 5-tuple (IP addresses + ports) from the flow_info
 * structure and stores it in the compressed format.
 */
static inline void bnxt_quic_copy_to_flow_key(struct bnxt_quic_flow_key *flow_key,
					      const struct bnxt_quic_flow_info *flow_info)
{
	flow_key->family = flow_info->daddr.ss_family;

	if (flow_info->daddr.ss_family == AF_INET) {
		struct sockaddr_in *src = (struct sockaddr_in *)&flow_info->saddr;
		struct sockaddr_in *dst = (struct sockaddr_in *)&flow_info->daddr;

		flow_key->v4.saddr = src->sin_addr.s_addr;
		flow_key->v4.daddr = dst->sin_addr.s_addr;
		flow_key->v4.sport = src->sin_port;
		flow_key->v4.dport = dst->sin_port;
	} else {  /* AF_INET6 */
		struct sockaddr_in6 *src = (struct sockaddr_in6 *)&flow_info->saddr;
		struct sockaddr_in6 *dst = (struct sockaddr_in6 *)&flow_info->daddr;

		flow_key->v6.saddr = src->sin6_addr;
		flow_key->v6.daddr = dst->sin6_addr;
		flow_key->v6.sport = src->sin6_port;
		flow_key->v6.dport = dst->sin6_port;
	}
}

#if defined(HAVE_BNXT_QUIC) && IS_ENABLED(CONFIG_TLS_DEVICE)

static inline bool bnxt_quic_busy(struct bnxt *bp)
{
	return bp->quic_info && atomic_read(&bp->quic_info->pending) > 0;
}

int bnxt_quic_init(struct bnxt *bp);
void bnxt_clear_quic(struct bnxt *bp);
void bnxt_quic_del_all(struct bnxt *bp);
void bnxt_free_quic_info(struct bnxt *bp);
void bnxt_alloc_quic_info(struct bnxt *bp, struct hwrm_func_qcaps_output *resp);
void bnxt_get_quic_dst_conect_id(struct bnxt *bp,
				 struct hwrm_cfa_tls_filter_alloc_input *req);
u64 bnxt_quic_get_percpu_counter(struct bnxt_tls_info *quic,
				 enum bnxt_quic_counters type);
void bnxt_quic_rx(struct bnxt *bp, struct sk_buff *skb, u8 *data_ptr,
		  unsigned int len, struct rx_cmp *rxcmp,
		  struct rx_cmp_ext *rxcmp1);
struct sk_buff *bnxt_quic_xmit(struct bnxt *bp, struct bnxt_tx_ring_info *txr,
			       struct sk_buff *skb, __le32 *lflags, u32 *kid);
int bnxt_siocdevprivate(struct net_device *dev, struct ifreq *ifr,
			void __user *useraddr, int cmd);

#else /* HAVE_BNXT_QUIC */

static inline bool bnxt_quic_busy(struct bnxt *bp)
{
	return 0;
}

static inline int bnxt_quic_init(struct bnxt *bp)
{
	return 0;
}

static inline void bnxt_clear_quic(struct bnxt *bp)
{
}

static inline void bnxt_quic_del_all(struct bnxt *bp)
{
}

static inline void bnxt_alloc_quic_info(struct bnxt *bp, struct hwrm_func_qcaps_output *resp)
{
}

static inline void bnxt_free_quic_info(struct bnxt *bp)
{
}

static inline void bnxt_get_quic_dst_conect_id(struct bnxt *bp,
					       struct hwrm_cfa_tls_filter_alloc_input *req)
{
}

static inline u64 bnxt_quic_get_percpu_counter(struct bnxt_tls_info *quic,
					       enum bnxt_quic_counters type)
{
	return 0;
}

static inline void bnxt_quic_rx(struct bnxt *bp, struct sk_buff *skb, u8 *data_ptr,
				unsigned int len, struct rx_cmp *rxcmp,
				struct rx_cmp_ext *rxcmp1)
{
}

static inline struct sk_buff *bnxt_quic_xmit(struct bnxt *bp, struct bnxt_tx_ring_info *txr,
					     struct sk_buff *skb, __le32 *lflags, u32 *kid)
{
	return skb;
}

#endif /* HAVE_BNXT_QUIC */

#endif
