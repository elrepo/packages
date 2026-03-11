/* Broadcom NetXtreme-C/E network driver.
 *
 * Copyright (c) 2022-2025 Broadcom Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 */

#ifndef BNXT_KTLS_H
#define BNXT_KTLS_H

#include <linux/hashtable.h>

#define BNXT_MAX_TX_CRYPTO_KEYS		204800
#define BNXT_MAX_RX_CRYPTO_KEYS		204800

#define BNXT_MAX_TX_CRYPTO_KEYS_PRE_233FW	65535
#define BNXT_MAX_RX_CRYPTO_KEYS_PRE_233FW	65535

#define BNXT_TX_CRYPTO_KEY_TYPE	FUNC_KEY_CTX_ALLOC_REQ_KEY_CTX_TYPE_TX
#define BNXT_RX_CRYPTO_KEY_TYPE	FUNC_KEY_CTX_ALLOC_REQ_KEY_CTX_TYPE_RX

#define BNXT_PARTITION_CAP_BITS						\
	  (FUNC_QCAPS_RESP_XID_PARTITION_CAP_TX_CK |			\
	   FUNC_QCAPS_RESP_XID_PARTITION_CAP_RX_CK)

#define BNXT_PARTITION_CAP(resp)					\
	((le32_to_cpu((resp)->flags_ext2) &				\
	  FUNC_QCAPS_RESP_FLAGS_EXT2_KEY_XID_PARTITION_SUPPORTED) &&	\
	 ((le16_to_cpu((resp)->xid_partition_cap) &			\
	   BNXT_PARTITION_CAP_BITS) == BNXT_PARTITION_CAP_BITS))

#define BNXT_KID_BATCH_SIZE	128

struct bnxt_kid_info {
	struct list_head	list;
	u32			start_id;
	u32			count;
	DECLARE_BITMAP(ids, BNXT_KID_BATCH_SIZE);
};

struct bnxt_kctx {
	struct list_head	list;
	/* to serialize update to the linked list and total_alloc */
	spinlock_t		lock;
	u8			type;
	u8			epoch;
	u32			total_alloc;
	u32			max_ctx;
	atomic_t		alloc_pending;
#define BNXT_KCTX_ALLOC_PENDING_MAX	8
	wait_queue_head_t	alloc_pending_wq;
	unsigned long		*partition_bmap;
	unsigned int		next;
};

#define BNXT_KID_HW_MASK	0xffffff
#define BNXT_KID_HW(kid)	((kid) & BNXT_KID_HW_MASK)
#define BNXT_KID_EPOCH_MASK	0xff000000
#define BNXT_KID_EPOCH_SHIFT	24
#define BNXT_KID_EPOCH(kid)	(((kid) & BNXT_KID_EPOCH_MASK) >>	\
				 BNXT_KID_EPOCH_SHIFT)

#define BNXT_SET_KID(kctx, kid)						\
	((kid) | ((kctx)->epoch << BNXT_KID_EPOCH_SHIFT))

#define BNXT_KCTX_ALLOC_OK(kctx)	\
	(atomic_read(&((kctx)->alloc_pending)) < BNXT_KCTX_ALLOC_PENDING_MAX)

struct bnxt_kfltr_info {
	u32			kid;
	__le64			filter_id;
	struct hlist_node	hash;
	struct rcu_head		rcu;
};

#define BNXT_MAX_CRYPTO_KEY_TYPE	(BNXT_RX_CRYPTO_KEY_TYPE + 1)

enum bnxt_ktls_counters {
	BNXT_KTLS_TX_ADD = 0,
	BNXT_KTLS_TX_DEL,
	BNXT_KTLS_TX_HW_PKT,
	BNXT_KTLS_TX_SW_PKT,
	BNXT_KTLS_TX_OOO,
	BNXT_KTLS_TX_RETRANS,
	BNXT_KTLS_TX_REPLAY,
	BNXT_KTLS_TX_REC_ERR,
	BNXT_KTLS_TX_SEQ_FWD,
	BNXT_KTLS_RX_ADD,
	BNXT_KTLS_RX_DEL,
	BNXT_KTLS_RX_HW_PKT,
	BNXT_KTLS_RX_SW_PKT,
	BNXT_KTLS_RX_RESYNC_REQ,
	BNXT_KTLS_RX_RESYNC_ACK,
	BNXT_KTLS_RX_RESYNC_DISCARD,
	BNXT_KTLS_RX_RESYNC_NAK,
	BNXT_KTLS_MAX_COUNTERS,
};

struct bnxt_tls_info {
	u16			max_key_ctxs_alloc;
	u16			ctxs_per_partition;
	u8			partition_mode:1;

	struct bnxt_kctx	kctx[BNXT_MAX_CRYPTO_KEY_TYPE];

	struct kmem_cache	*mpc_cache;
	atomic_t		pending;

	DECLARE_HASHTABLE(filter_tbl, 8);
	/* to serialize adding to and deleting from the filter_tbl */
	spinlock_t		filter_lock;
	u32			filter_count;
	atomic_t		filter_pending;
	u32			max_filter;
#define BNXT_MAX_TLS_FILTER	460
	u32			max_filter_tf;
#define BNXT_MAX_TF_TLS_FILTER	204800

#define BNXT_QUIC_FLTR_HASH_SIZE 18
	DECLARE_HASHTABLE(quic_tx_fltr_tbl, BNXT_QUIC_FLTR_HASH_SIZE);
	spinlock_t              quic_fltr_lock; /* for hash table add, del */
	int                     quic_tx_fltr_count;

	atomic64_t		*counters;
};

#define tck	kctx[BNXT_TX_CRYPTO_KEY_TYPE]
#define rck	kctx[BNXT_RX_CRYPTO_KEY_TYPE]

struct bnxt_ktls_offload_ctx_tx {
	u32		tcp_seq_no;
	u32		kid;
};

struct bnxt_ktls_offload_ctx_rx {
	u32		kid;
	/* to protect resync state */
	spinlock_t	resync_lock;
	u32		resync_tcp_seq_no;
	u32		bytes_since_resync;
	unsigned long	resync_timestamp;
	u8		resync_pending:1;
};

#define BNXT_KTLS_RESYNC_TMO		msecs_to_jiffies(2500)
#define BNXT_KTLS_MAX_RESYNC_BYTES	32768

#define BNXT_KTLS_MAX_REPLAY_MSS	9000

struct ce_add_cmd {
	__le32	ver_algo_kid_opcode;
	#define CE_ADD_CMD_OPCODE_MASK			0xfUL
	#define CE_ADD_CMD_OPCODE_SFT			0
	#define CE_ADD_CMD_OPCODE_ADD			 0x1UL
	#define CE_ADD_CMD_KID_MASK			0xfffff0UL
	#define CE_ADD_CMD_KID_SFT			4
	#define CE_ADD_CMD_ALGORITHM_MASK		0xf000000UL
	#define CE_ADD_CMD_ALGORITHM_SFT		24
	#define CE_ADD_CMD_ALGORITHM_AES_GCM_128	 0x1000000UL
	#define CE_ADD_CMD_ALGORITHM_AES_GCM_256	 0x2000000UL
	#define CE_ADD_CMD_VERSION_MASK			0xf0000000UL
	#define CE_ADD_CMD_VERSION_SFT			28
	#define CE_ADD_CMD_VERSION_TLS1_2		 (0x0UL << 28)
	#define CE_ADD_CMD_VERSION_TLS1_3		 (0x1UL << 28)
	u8	ctx_kind;
	#define CE_ADD_CMD_CTX_KIND_MASK		0x1fUL
	#define CE_ADD_CMD_CTX_KIND_SFT			0
	#define CE_ADD_CMD_CTX_KIND_CK_TX		 0x11UL
	#define CE_ADD_CMD_CTX_KIND_CK_RX		 0x12UL
	u8	unused0[3];
	u8	salt[4];
	u8	unused1[4];
	__le32	pkt_tcp_seq_num;
	__le32	tls_header_tcp_seq_num;
	u8	record_seq_num[8];
	u8	session_key[32];
	u8	addl_iv[8];
};

#define record_seq_num_end	record_seq_num[7]

struct ce_delete_cmd {
	__le32  ctx_kind_kid_opcode;
	#define CE_DELETE_CMD_OPCODE_MASK		0xfUL
	#define CE_DELETE_CMD_OPCODE_SFT		0
	#define CE_DELETE_CMD_OPCODE_DEL		 0x2UL
	#define CE_DELETE_CMD_KID_MASK			0xfffff0UL
	#define CE_DELETE_CMD_KID_SFT			4
	#define CE_DELETE_CMD_CTX_KIND_MASK		0x1f000000UL
	#define CE_DELETE_CMD_CTX_KIND_SFT		24
	#define CE_DELETE_CMD_CTX_KIND_CK_TX		 (0x11UL << 24)
	#define CE_DELETE_CMD_CTX_KIND_CK_RX		 (0x12UL << 24)
};

struct ce_resync_resp_ack_cmd {
	__le32	resync_status_kid_opcode;
	#define CE_RESYNC_RESP_ACK_CMD_OPCODE_MASK	0xfUL
	#define CE_RESYNC_RESP_ACK_CMD_OPCODE_SFT	0
	#define CE_RESYNC_RESP_ACK_CMD_OPCODE_RESYNC	 0x3UL
	#define CE_RESYNC_RESP_ACK_CMD_KID_MASK		0xfffff0UL
	#define CE_RESYNC_RESP_ACK_CMD_KID_SFT		4
	#define CE_RESYNC_RESP_ACK_CMD_RESYNC_STATUS	0x1000000UL
	#define CE_RESYNC_RESP_ACK_CMD_RESYNC_STATUS_ACK (0x0UL << 24)
	#define CE_RESYNC_RESP_ACK_CMD_RESYNC_STATUS_NAK (0x1UL << 24)
	__le32	resync_record_tcp_seq_num;
	u8	resync_record_seq_num[8];
};

#define resync_record_seq_num_end	resync_record_seq_num[7]

#define CE_CMD_OP_MASK			0x00000fU
#define CE_CMD_KID_MASK			0xfffff0U
#define CE_CMD_KID_SFT			4

#define CE_CMD_OP(cmd_p)					\
	(*(u32 *)(cmd_p) & CE_CMD_OP_MASK)

#define CE_CMD_KID(cmd_p)					\
	((*(u32 *)(cmd_p) & CE_CMD_KID_MASK) >> CE_CMD_KID_SFT)

#define BNXT_KMPC_OPAQUE(client, kid)				\
	(((client) << 24) | (kid))

#define BNXT_INV_KMPC_OPAQUE	0xffffffff

struct ce_cmpl {
	__le16	client_subtype_type;
	#define CE_CMPL_TYPE_MASK			0x3fUL
	#define CE_CMPL_TYPE_SFT			0
	#define CE_CMPL_TYPE_MID_PATH_SHORT		 0x1eUL
	#define CE_CMPL_SUBTYPE_MASK			0xf00UL
	#define CE_CMPL_SUBTYPE_SFT			8
	#define CE_CMPL_SUBTYPE_SOLICITED		 (0x0UL << 8)
	#define CE_CMPL_SUBTYPE_ERR			 (0x1UL << 8)
	#define CE_CMPL_SUBTYPE_RESYNC			 (0x2UL << 8)
	#define CE_CMPL_MP_CLIENT_MASK			0xf000UL
	#define CE_CMPL_MP_CLIENT_SFT			12
	#define CE_CMPL_MP_CLIENT_TCE			 (0x0UL << 12)
	#define CE_CMPL_MP_CLIENT_RCE			 (0x1UL << 12)
	__le16	status;
	#define CE_CMPL_STATUS_MASK			0xfUL
	#define CE_CMPL_STATUS_SFT			0
	#define CE_CMPL_STATUS_OK			 0x0UL
	#define CE_CMPL_STATUS_CTX_LD_ERR		 0x1UL
	#define CE_CMPL_STATUS_FID_CHK_ERR		 0x2UL
	#define CE_CMPL_STATUS_CTX_VER_ERR		 0x3UL
	#define CE_CMPL_STATUS_DST_ID_ERR		 0x4UL
	#define CE_CMPL_STATUS_MP_CMD_ERR		 0x5UL
	u32	opaque;
	__le32	v;
	#define CE_CMPL_V           0x1UL
	__le32	kid;
	#define CE_CMPL_KID_MASK    0xfffffUL
	#define CE_CMPL_KID_SFT     0
};

#define CE_CMPL_STATUS(ce_cmpl)						\
	(le16_to_cpu((ce_cmpl)->status) & CE_CMPL_STATUS_MASK)

#define CE_CMPL_KID(ce_cmpl)						\
	(le32_to_cpu((ce_cmpl)->kid) & CE_CMPL_KID_MASK)

struct crypto_prefix_cmd {
	__le32	flags;
	#define CRYPTO_PREFIX_CMD_FLAGS_UPDATE_IN_ORDER_VAR	0x1UL
	#define CRYPTO_PREFIX_CMD_FLAGS_FULL_REPLAY_RETRAN	0x2UL
	__le32	header_tcp_seq_num;
	__le32	start_tcp_seq_num;
	__le32	end_tcp_seq_num;
	u8	explicit_nonce[8];
	u8	record_seq_num[8];
};

#define CRYPTO_PREFIX_CMD_FLAGS_UPDATE_IN_ORDER_VAR_LE	\
	cpu_to_le32(CRYPTO_PREFIX_CMD_FLAGS_UPDATE_IN_ORDER_VAR)

#define CRYPTO_PREFIX_CMD_SIZE	((u32)sizeof(struct crypto_prefix_cmd))
#define CRYPTO_PREFIX_CMD_BDS	(CRYPTO_PREFIX_CMD_SIZE / sizeof(struct tx_bd))
#define CRYPTO_PRESYNC_BDS	(CRYPTO_PREFIX_CMD_BDS + 1)

#define CRYPTO_PRESYNC_BD_CMD						\
	(cpu_to_le32((CRYPTO_PREFIX_CMD_SIZE << TX_BD_LEN_SHIFT) |	\
		     TX_BD_CNT(CRYPTO_PRESYNC_BDS) | TX_BD_TYPE_PRESYNC_TX_BD))

#define BNXT_METADATA_OFF(len)	ALIGN(len, 32)

struct bnxt_crypto_cmd_ctx {
	struct completion cmp;
	struct ce_cmpl ce_cmp;
};

static inline bool bnxt_ktls_busy(struct bnxt *bp)
{
	return bp->ktls_info && atomic_read(&bp->ktls_info->pending) > 0;
}

void bnxt_alloc_ktls_info(struct bnxt *bp, struct hwrm_func_qcaps_output *resp);
void bnxt_clear_cfa_tls_filters_tbl(struct bnxt *bp);
void bnxt_clear_ktls(struct bnxt *bp);
void bnxt_free_ktls_info(struct bnxt *bp);
void bnxt_hwrm_reserve_pf_key_ctxs(struct bnxt *bp,
				   struct hwrm_func_cfg_input *req, u8 type);
int bnxt_ktls_init(struct bnxt *bp);
void bnxt_ktls_mpc_cmp(struct bnxt *bp, u32 client, unsigned long handle,
		       struct bnxt_cmpl_entry cmpl[], u32 entries);
struct sk_buff *bnxt_ktls_xmit(struct bnxt *bp, struct bnxt_tx_ring_info *txr,
			       struct sk_buff *skb, __le32 *lflags, u32 *kid);
void bnxt_ktls_rx(struct bnxt *bp, struct sk_buff *skb, u8 *data_ptr,
		  unsigned int len, struct rx_cmp *rxcmp,
		  struct rx_cmp_ext *rxcmp1);
int bnxt_xmit_crypto_cmd(struct bnxt *bp, struct bnxt_tx_ring_info *txr,
			 void *cmd, uint len, uint tmo, struct bnxt_tls_info *tls);
int bnxt_hwrm_cfa_tls_filter_alloc(struct bnxt *bp, struct sock *sk, u32 kid,
				   u8 type, u64 conn_id);
int bnxt_hwrm_cfa_tls_filter_free(struct bnxt *bp, u32 kid, u8 type);
int bnxt_key_ctx_alloc_one(struct bnxt *bp, struct bnxt_kctx *kctx, u32 *id, u8 type);
void bnxt_free_one_kctx(struct bnxt_kctx *kctx, u32 id);
void bnxt_ktls_del_all(struct bnxt *bp);
int bnxt_set_partition_mode(struct bnxt *bp);
int bnxt_hwrm_key_ctx_alloc(struct bnxt *bp, struct bnxt_kctx *kctx, u32 num,
			    u32 *id, u8 type);
#endif
