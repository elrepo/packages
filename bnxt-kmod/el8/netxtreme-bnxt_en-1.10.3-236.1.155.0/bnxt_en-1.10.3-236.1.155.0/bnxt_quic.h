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

#include "bnxt_quic_usr_include.h"
#include "bnxt_ulp.h"

#define BNXT_MAX_QUIC_TX_CRYPTO_KEYS		524300
#define BNXT_MAX_QUIC_RX_CRYPTO_KEYS		524300

#define BNXT_CONN_ID_MAX_LEN          20
#define BNXT_MAX_KEY_SIZE		32
#define BNXT_IV_SIZE			12
#define SIOCDEVQUICFLOWADD SIOCDEVPRIVATE
#define SIOCDEVQUICFLOWDEL (SIOCDEVPRIVATE + 1)

enum bnxt_quic_counters {
	BNXT_QUIC_TX_ADD = 0,
	BNXT_QUIC_TX_DEL,
	BNXT_QUIC_TX_HW_PKT,
	BNXT_QUIC_TX_SW_PKT,
	BNXT_QUIC_RX_ADD,
	BNXT_QUIC_RX_DEL,
	BNXT_QUIC_RX_HW_PKT,
	BNXT_QUIC_RX_SW_PKT,
	BNXT_QUIC_RX_PAYLOAD_DECRYPTED,
	BNXT_QUIC_RX_HDR_DECRYPTED,
	BNXT_QUIC_RX_KEY_PHASE_MISMATCH,
	BNXT_QUIC_RX_RUNT,
	BNXT_QUIC_RX_SHORT_HDR,
	BNXT_QUIC_RX_LONG_HDR,
	BNXT_QUIC_MAX_COUNTERS,
};

struct bnxt_quic_crypto_info {
	struct bnxt *bp;
	struct hlist_node	node;
	struct bnxt_quic_connection_info connection_info;
	u32 rx_kid;
	u32 tx_kid;
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

static inline bool bnxt_quic_busy(struct bnxt *bp)
{
	return bp->quic_info && atomic_read(&bp->quic_info->pending) > 0;
}

int bnxt_quic_init(struct bnxt *bp);
void bnxt_free_quic_info(struct bnxt *bp);
void bnxt_alloc_quic_info(struct bnxt *bp, struct hwrm_func_qcaps_output *resp);
void bnxt_get_quic_dst_conect_id(struct bnxt *bp,
				 struct hwrm_cfa_tls_filter_alloc_input *req);
void bnxt_quic_rx(struct bnxt *bp, struct sk_buff *skb, u8 *data_ptr,
		  unsigned int len, struct rx_cmp *rxcmp,
		  struct rx_cmp_ext *rxcmp1);
struct sk_buff *bnxt_quic_xmit(struct bnxt *bp, struct bnxt_tx_ring_info *txr,
			       struct sk_buff *skb, __le32 *lflags, u32 *kid);
int bnxt_siocdevprivate(struct net_device *dev, struct ifreq *ifr,
			void __user *useraddr, int cmd);
#endif
