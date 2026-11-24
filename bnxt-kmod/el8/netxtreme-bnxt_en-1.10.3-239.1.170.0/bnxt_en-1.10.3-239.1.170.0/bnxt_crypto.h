/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2026 Broadcom Inc. */

#ifndef BNXT_CRYPTO_H
#define BNXT_CRYPTO_H

#define BNXT_MAX_TX_CRYPTO_KEYS		204800
#define BNXT_MAX_RX_CRYPTO_KEYS		204800

#define BNXT_MAX_TX_CRYPTO_KEYS_PRE_233FW	65535
#define BNXT_MAX_RX_CRYPTO_KEYS_PRE_233FW	65535

enum bnxt_crypto_type {
	BNXT_TX_CRYPTO_KEY_TYPE = FUNC_KEY_CTX_ALLOC_REQ_KEY_CTX_TYPE_TX,
	BNXT_RX_CRYPTO_KEY_TYPE = FUNC_KEY_CTX_ALLOC_REQ_KEY_CTX_TYPE_RX,
	BNXT_MAX_CRYPTO_KEY_TYPE,
};

#define BNXT_TCK(crypto)	((crypto)->kctx[BNXT_TX_CRYPTO_KEY_TYPE])
#define BNXT_RCK(crypto)	((crypto)->kctx[BNXT_RX_CRYPTO_KEY_TYPE])

/* These match the crypto HSI defined kind values */
#define BNXT_CTX_KIND_CK_TX		0x11
#define BNXT_CTX_KIND_CK_RX		0x12
#define BNXT_CTX_KIND_CK_QUIC_TX	0x14
#define BNXT_CTX_KIND_CK_QUIC_RX	0x15

#define BNXT_INVALID_KEY_CTX_TYPE	0xff

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
	u8			type;
	u8			kind;
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
	unsigned int		partition_max;
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

struct bnxt_crypto_info {
	u16			max_key_ctxs_alloc;
	u16			ctxs_per_partition;
	u8			partition_mode:1;

	struct bnxt_kctx	kctx[BNXT_MAX_CRYPTO_KEY_TYPE];
};

#ifdef CONFIG_BNXT_CRYPTO
void bnxt_alloc_crypto_info(struct bnxt *bp,
			    struct hwrm_func_qcaps_output *resp);
void bnxt_crypto_del_all(struct bnxt *bp);
void bnxt_clear_crypto(struct bnxt *bp);
void bnxt_free_crypto_info(struct bnxt *bp);
void bnxt_hwrm_reserve_pf_key_ctxs(struct bnxt *bp,
				   struct hwrm_func_cfg_input *req);
int bnxt_hwrm_key_ctx_alloc(struct bnxt *bp, struct bnxt_kctx *kctx, u8 kind,
			    u32 num, u32 *id);
bool bnxt_kid_valid(struct bnxt_kctx *kctx, u32 id);
void bnxt_free_one_kctx(struct bnxt_kctx *kctx, u32 id);
int bnxt_key_ctx_alloc_one(struct bnxt *bp, struct bnxt_kctx *kctx, u8 kind,
			   u32 *id);
int bnxt_crypto_init(struct bnxt *bp);
#else
static inline void bnxt_alloc_crypto_info(struct bnxt *bp,
					  struct hwrm_func_qcaps_output *resp)
{
}

static inline void bnxt_crypto_del_all(struct bnxt *bp)
{
}

static inline void bnxt_clear_crypto(struct bnxt *bp)
{
}

static inline void bnxt_free_crypto_info(struct bnxt *bp)
{
}

static inline void bnxt_hwrm_reserve_pf_key_ctxs(struct bnxt *bp,
						 struct hwrm_func_cfg_input *req)
{
}

static inline int bnxt_hwrm_key_ctx_alloc(struct bnxt *bp,
					  struct bnxt_kctx *kctx, u8 kind,
					  u32 num, u32 *id)
{
	return -EOPNOTSUPP;
}

static inline bool bnxt_kid_valid(struct bnxt_kctx *kctx, u32 id)
{
	return false;
}

static inline void bnxt_free_one_kctx(struct bnxt_kctx *kctx, u32 id)
{
}

static inline int bnxt_key_ctx_alloc_one(struct bnxt *bp,
					 struct bnxt_kctx *kctx, u8 kind,
					 u32 *id)
{
	return -EOPNOTSUPP;
}

static inline int bnxt_crypto_init(struct bnxt *bp)
{
	return 0;
}
#endif	/* CONFIG_BNXT_CRYPTO */
#endif	/* BNXT_CRYPTO_H */
