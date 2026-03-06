// SPDX-License-Identifier: BSD-3-Clause
/* Copyright (c) 2022-2024 Broadcom Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 */

#include <linux/stddef.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/hashtable.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/tcp.h>
#include <net/inet_hashtables.h>
#include <net/inet6_hashtables.h>

#include "bnxt_compat.h"
#include "bnxt_hsi.h"
#include "bnxt.h"
#include "bnxt_hwrm.h"
#include "bnxt_mpc.h"
#include "bnxt_tfc.h"
#include "cfa_bld_p70_mpc.h"

#define BNXT_MPC_RX_US_SLEEP 100 /* milli seconds */
#define BNXT_MPC_RX_RETRY    10
#define BNXT_MPC_TIMEOUT     (BNXT_MPC_RX_US_SLEEP * BNXT_MPC_RX_RETRY)
#define BNXT_TFC_MPC_TX_RETRIES             150
#define BNXT_TFC_MPC_TX_RETRY_DELAY_MIN_US  500
#define BNXT_TFC_MPC_TX_RETRY_DELAY_MAX_US 1000

#define BNXT_TFC_DISP_BUF_SIZE	128

#define BNXT_TFC_PR_W_1BYTES	1
#define BNXT_TFC_PR_W_2BYTES	2
#define BNXT_TFC_PR_W_4BYTES	4
/*
 * bnxt_tfc_buf_dump: Pretty-prints a buffer using the following options
 *
 * Parameters:
 * hdr       - A header that is printed as-is
 * msg       - This is a pointer to the uint8_t buffer to be dumped
 * prtwidth  - The width of the items to be printed in bytes,
 *             allowed options 1, 2, 4
 *             Defaults to 1 if either:
 *             1) any other value
 *             2) if buffer length is not a multiple of width
 * linewidth - The length of the lines printed (in items)
 */
void bnxt_tfc_buf_dump(struct bnxt *bp, char *hdr,
		       uint8_t *msg, int msglen,
		       int prtwidth, int linewidth)
{
	char msg_line[BNXT_TFC_DISP_BUF_SIZE];
	int msg_i = 0, i;
	uint16_t *sw_msg = (uint16_t *)msg;
	uint32_t *lw_msg = (uint32_t *)msg;

	if (hdr)
		netdev_dbg(bp->dev, "%s", hdr);

	if (msglen % prtwidth) {
		netdev_dbg(bp->dev, "msglen[%u] not aligned on width[%u]\n",
			   msglen, prtwidth);
		prtwidth = 1;
		linewidth = 16;
	}

	for (i = 0; i < msglen / prtwidth; i++) {
		if ((i % linewidth == 0) && i)
			netdev_dbg(bp->dev, "%s\n", msg_line);
		if (i % linewidth == 0) {
			msg_i = 0;
			msg_i += snprintf(&msg_line[msg_i], (sizeof(msg_line) - msg_i),
					  "%04x: ", i * prtwidth);
		}
		switch (prtwidth) {
		case BNXT_TFC_PR_W_2BYTES:
			msg_i += snprintf(&msg_line[msg_i], (sizeof(msg_line) - msg_i),
					  "%04x ", sw_msg[i]);
			break;

		case BNXT_TFC_PR_W_4BYTES:
			msg_i += snprintf(&msg_line[msg_i], (sizeof(msg_line) - msg_i),
					  "%08x ", lw_msg[i]);
			break;

		case BNXT_TFC_PR_W_1BYTES:
		default:
			msg_i += snprintf(&msg_line[msg_i], (sizeof(msg_line) - msg_i),
					  "%02x ", msg[i]);
			break;
		}
	}
	netdev_dbg(bp->dev, "%s\n", msg_line);
}

void bnxt_free_tfc_mpc_info(struct bnxt *bp)
{
	struct bnxt_tfc_mpc_info *tfc_info;

	if (!bp)
		return;

	tfc_info = bp->tfc_info;

	if (tfc_info && tfc_info->mpc_cache) {
		kmem_cache_destroy(tfc_info->mpc_cache);
		tfc_info->mpc_cache = NULL;
	}

	kfree(bp->tfc_info);
	bp->tfc_info = NULL;
}

int bnxt_alloc_tfc_mpc_info(struct bnxt *bp)
{
	struct bnxt_tfc_mpc_info *tfc_info =
		(struct bnxt_tfc_mpc_info *)(bp->tfc_info);
	char name[32];

	if (!tfc_info) {
		tfc_info = kzalloc(sizeof(*tfc_info), GFP_KERNEL);
		if (!tfc_info)
			return -ENOMEM;

		bp->tfc_info = (void *)tfc_info;
	}

	if (tfc_info->mpc_cache)
		return 0;

	snprintf(name, sizeof(name), "bnxt_tfc-%s", dev_name(&bp->pdev->dev));
	tfc_info->mpc_cache = kmem_cache_create(name,
						sizeof(struct bnxt_tfc_cmd_ctx),
						0, 0, NULL);
	if (!tfc_info->mpc_cache) {
		bnxt_free_tfc_mpc_info(bp);
		return -ENOMEM;
	}

	return 0;
}

int bnxt_mpc_cmd_cmpl(struct bnxt *bp,
		      struct bnxt_mpc_mbuf *out_msg,
		      struct bnxt_tfc_cmd_ctx *ctx)
{
	struct bnxt_tfc_mpc_info *tfc = (struct bnxt_tfc_mpc_info *)bp->tfc_info;
	uint tmo = BNXT_MPC_TIMEOUT;
	unsigned long tmo_left;
	int rc;

	/* Handle firmware reset: `test_bit()` isn't fully race-safe, but if
	 * `BNXT_STATE_IN_FW_RESET` is set, it indicates a firmware reset is in progress.
	 * If this flag is missed due to a race, the active MPC request might eventually
	 * timeout, and subsequent requests in the batch will be blocked.
	 *
	 * The critical change here is to *always* proceed to `xmit_done`, regardless
	 * of a timeout or direct blocking by firmware reset. This ensures that `ctx`
	 * is released and `tfc->pending` is decremented for each flow. This cleanup is
	 * vital: the SC thread will be terminated later by the firmware reset process,
	 * and failing to release corresponding resources now leads to leaks and blockages
	 * (non-zero `tfc->pending` keeps the device in a busy state), which can
	 * prevent the firmware reset process itself from completing. By freeing these
	 * resources, the system can successfully complete the firmware reset.
	 */
	if (test_bit(BNXT_STATE_IN_FW_RESET, &bp->state) ||
	    test_bit(BNXT_STATE_FW_FATAL_COND, &bp->state)) {
		netdev_dbg(bp->dev, "Skip mpc completion due to FW reset %lx\n",
			   bp->state);
		ctx->tfc_cmp.opaque = BNXT_INV_TMPC_OPAQUE;
		ctx = NULL; /* The completion thread will free the context */
		rc = -EIO;
		goto xmit_done;
	}

	tmo_left = wait_for_completion_timeout(&ctx->cmp, msecs_to_jiffies(tmo));
	if (!tmo_left) {
		ctx->tfc_cmp.opaque = BNXT_INV_TMPC_OPAQUE;
		ctx = NULL; /* The completion thread will free the context */
		netdev_dbg(bp->dev, "TFC MPC timed out bp state = %lx\n",
			   bp->state);
		rc = -ETIMEDOUT;
		goto xmit_done;
	}
	if (TFC_CMPL_STATUS(&ctx->tfc_cmp) == TFC_CMPL_STATUS_OK) {
		/* Copy response/completion back into out_msg */
		netdev_dbg(bp->dev, "%s: TFC MPC cmd completed\n", __func__);
		memcpy(out_msg->msg_data, &ctx->tfc_cmp, sizeof(ctx->tfc_cmp));
		rc = 0;
	} else {
		if ((TFC_CMPL_STATUS(&ctx->tfc_cmp) >> TFC_CMPL_STATUS_SFT) ==
		    CFA_MPC_EM_ABORT)
			netdev_err(bp->dev,
				   "MPC failed, no static bucket available\n");
		else if ((TFC_CMPL_STATUS(&ctx->tfc_cmp) >> TFC_CMPL_STATUS_SFT) ==
			 CFA_MPC_EM_DUPLICATE)
			netdev_err(bp->dev,
				   "MPC failed, duplicate entry\n");
		else
			netdev_err(bp->dev, "MPC status code [%lu]\n",
				   TFC_CMPL_STATUS(&ctx->tfc_cmp) >> TFC_CMPL_STATUS_SFT);
		rc = -EIO;
	}

xmit_done:
	if (ctx)
		kmem_cache_free(tfc->mpc_cache, ctx);
	atomic_dec(&tfc->pending);
	return rc;
}

int bnxt_mpc_send(struct bnxt *bp,
		  struct bnxt_mpc_mbuf *in_msg,
		  struct bnxt_mpc_mbuf *out_msg,
		  uint32_t *opaque,
		  int type,
		  struct tfc_mpc_batch_info_t *batch_info)
{
	struct bnxt_tfc_mpc_info *tfc = (struct bnxt_tfc_mpc_info *)bp->tfc_info;
	struct bnxt_mpc_info *mpc = bp->mpc_info;
	struct bnxt_tfc_cmd_ctx *ctx = NULL;
	unsigned long tmo_left, handle = 0;
	struct bnxt_tx_ring_info *txr;
	uint tmo = BNXT_MPC_TIMEOUT;
	struct bnxt_napi *bnapi;
	int rc = 0;

	if (test_bit(BNXT_STATE_IN_FW_RESET, &bp->state) ||
	    test_bit(BNXT_STATE_FW_FATAL_COND, &bp->state)) {
		netdev_dbg(bp->dev, "%s: firmware reset in progress %lx\n", __func__, bp->state);
		return -EIO;
	}

	if (!mpc || !tfc) {
		netdev_dbg(bp->dev, "%s: mpc[%p], tfc[%p]\n", __func__, mpc, tfc);
		return -1;
	}

	if (out_msg->cmp_type != MPC_CMP_TYPE_MID_PATH_SHORT &&
	    out_msg->cmp_type != MPC_CMP_TYPE_MID_PATH_LONG)
		return -1;

	atomic_inc(&tfc->pending);
	/* Make sure that bnxt_free_persistent_mpc_rings() sees
	 * pending before we check MPC0_NAPI flag.
	 */
	smp_mb__after_atomic();
	if (!bp->bnapi) {
		atomic_dec(&tfc->pending);
		netdev_dbg(bp->dev, "%s: bp->bnapi is not initialized\n", __func__);
		return -EAGAIN;
	}
	bnapi = bp->bnapi[BNXT_NQ0_NAPI_IDX];
	if (!bnapi || !BNXT_MPC0_NAPI(bnapi)) {
		atomic_dec(&tfc->pending);
		return -EAGAIN;
	}

	if (in_msg->chnl_id == RING_ALLOC_REQ_MPC_CHNLS_TYPE_TE_CFA)
		txr = &mpc->mpc_rings[BNXT_MPC_TE_CFA_TYPE][0];
	else
		txr = &mpc->mpc_rings[BNXT_MPC_RE_CFA_TYPE][0];

	if (!txr) {
		netdev_err(bp->dev, "%s: No Tx rings\n", __func__);
		rc = -EINVAL;
		goto xmit_done;
	}

	if (!netif_running(bp->dev) && !txr->persistent) {
		netdev_err(bp->dev, "%s: No persistent Tx rings\n", __func__);
		rc = -EINVAL;
		goto xmit_done;
	}

	if (tmo) {
		ctx = kmem_cache_alloc(tfc->mpc_cache, GFP_KERNEL);
		if (!ctx) {
			rc = -ENOMEM;
			goto xmit_done;
		}
		init_completion(&ctx->cmp);
		handle = (unsigned long)ctx;
		ctx->tfc_cmp.opaque = *opaque;
		might_sleep();
	}

	spin_lock(&txr->tx_lock);
	rc = bnxt_start_xmit_mpc(bp, txr, in_msg->msg_data,
				 in_msg->msg_size, handle);
	spin_unlock(&txr->tx_lock);
	if (rc || !tmo)
		goto xmit_done;

	if (batch_info && batch_info->enabled) {
		batch_info->comp_info[batch_info->count].bp = bp;
		memcpy(&batch_info->comp_info[batch_info->count].out_msg,
		       out_msg,
		       sizeof(*out_msg));
		batch_info->comp_info[batch_info->count].ctx = ctx;
		batch_info->comp_info[batch_info->count].type = type;
		batch_info->count++;
		return rc;
	}

	tmo_left = wait_for_completion_timeout(&ctx->cmp, msecs_to_jiffies(tmo));
	if (!tmo_left) {
		ctx->tfc_cmp.opaque = BNXT_INV_TMPC_OPAQUE;
		complete(&ctx->cmp);
		kmem_cache_free(tfc->mpc_cache, ctx);
		ctx = NULL; /* The completion thread will free the context */
		netdev_warn(bp->dev, "TFC MP cmd %08x timed out\n",
			    *((u32 *)in_msg->msg_data));
		rc = -ETIMEDOUT;
		goto xmit_done;
	}
	if (TFC_CMPL_STATUS(&ctx->tfc_cmp) == TFC_CMPL_STATUS_OK) {
		/* Copy response/completion back into out_msg */
		netdev_dbg(bp->dev, "%s: TFC MPC cmd %08x completed\n",
			   __func__, *((u32 *)in_msg->msg_data));
		memcpy(out_msg->msg_data, &ctx->tfc_cmp, sizeof(ctx->tfc_cmp));
		rc = 0;
	} else {
		if ((TFC_CMPL_STATUS(&ctx->tfc_cmp) >> TFC_CMPL_STATUS_SFT) ==
		    CFA_MPC_EM_ABORT)
			netdev_err(bp->dev,
				   "MPC failed, no static bucket available\n");
		else if ((TFC_CMPL_STATUS(&ctx->tfc_cmp) >> TFC_CMPL_STATUS_SFT) ==
			 CFA_MPC_EM_DUPLICATE)
			netdev_err(bp->dev,
				   "MPC failed, duplicate entry\n");
		else
			netdev_err(bp->dev, "MPC status code [%lu]\n",
				   TFC_CMPL_STATUS(&ctx->tfc_cmp) >> TFC_CMPL_STATUS_SFT);

		rc = -EIO;
	}

xmit_done:
	if (ctx)
		kmem_cache_free(tfc->mpc_cache, ctx);
	atomic_dec(&tfc->pending);
	return rc;
}

void bnxt_tfc_mpc_cmp(struct bnxt *bp, u32 client, unsigned long handle,
		      struct bnxt_cmpl_entry cmpl[], u32 entries)
{
	struct bnxt_tfc_mpc_info *tfc = (struct bnxt_tfc_mpc_info *)bp->tfc_info;
	struct bnxt_tfc_cmd_ctx *ctx;
	struct tfc_cmpl *cmp;
	u32 len;

	cmp = cmpl[0].cmpl;
	if (!handle || entries < 1 || entries > 2) {
		if (entries < 1 || entries > 2) {
			netdev_warn(bp->dev, "Invalid entries %d with handle %lx cmpl %08x in %s()\n",
				    entries, handle, *(u32 *)cmp, __func__);
		}
		return;
	}

	if (handle == BNXT_INV_MPC_HDL) {
		netdev_warn(bp->dev, "Invalid handle, aborting completion. handle:%lu\n", handle);
		return;
	}

	ctx = (void *)handle;
	if (ctx->tfc_cmp.opaque == BNXT_INV_TMPC_OPAQUE) {
		/* The context has been marked as invalid */
		netdev_warn(bp->dev, "Dropping invalid context\n");
		complete(&ctx->cmp);
		kmem_cache_free(tfc->mpc_cache, ctx);
	} else {
		if (entries > 1) {
			memcpy(&ctx->tfc_cmp, cmpl[0].cmpl, cmpl[0].len);
			memcpy(&ctx->tfc_cmp.l_cmpl[0], cmpl[1].cmpl, cmpl[1].len);
		} else {
			len = min_t(u32, cmpl[0].len, sizeof(ctx->tfc_cmp));
			memcpy(&ctx->tfc_cmp, cmpl[0].cmpl, len);
		}
		complete(&ctx->cmp);
	}
}
