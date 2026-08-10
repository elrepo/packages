/* Broadcom NetXtreme-C/E network driver.
 *
 * Copyright (c) 2025 Broadcom Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 */

#include <linux/netdevice.h>

#include "bnxt_compat.h"
#include "bnxt_hsi.h"
#include "bnxt.h"
#include "bnxt_hwrm.h"
#include "bnxt_ttxp.h"

/* Timed TX Pacing support */

static int bnxt_hwrm_ttx_pacing_rate_query(struct bnxt *bp)
{
	struct hwrm_func_ttx_pacing_rate_query_output *resp;
	struct hwrm_func_ttx_pacing_rate_query_input *req;
	struct bnxt_ttxp *ttxp = bp->ttxp;
	int rc, i;

	if (!ttxp)
		return 0;

	rc = hwrm_req_init(bp, req, HWRM_FUNC_TTX_PACING_RATE_QUERY);
	if (rc)
		return rc;
	resp = hwrm_req_hold(bp, req);
	req->profile_id = ttxp->profile_id;
	rc = hwrm_req_send(bp, req);
	if (!rc) {
		for (i = 0; i < BNXT_MAX_TTXP_RATES; i++)
			ttxp->rates[i] = cpu_to_le32(resp->rates[i]);
	}
	hwrm_req_drop(bp, req);
	return rc;
}

static int bnxt_hwrm_ttx_pacing_rate_prof_query(struct bnxt *bp)
{
	struct hwrm_func_ttx_pacing_rate_prof_query_output *resp;
	struct hwrm_func_ttx_pacing_rate_prof_query_input *req;
	struct bnxt_ttxp *ttxp = bp->ttxp;
	int rc;

	if (!ttxp)
		return 0;

	rc = hwrm_req_init(bp, req, HWRM_FUNC_TTX_PACING_RATE_PROF_QUERY);
	if (rc)
		return rc;
	resp = hwrm_req_hold(bp, req);
	rc = hwrm_req_send(bp, req);
	if (!rc) {
		ttxp->profile_id = resp->active_prof_id;
		switch (ttxp->profile_id) {
		case FUNC_TTX_PACING_RATE_PROF_QUERY_RESP_60M:
		case FUNC_TTX_PACING_RATE_PROF_QUERY_RESP_50G:
			/* FW rates in Kbps */
			ttxp->rate_factor = 1000;
			break;
		default:
			rc = -ERANGE;
		}
	}
	hwrm_req_drop(bp, req);
	return rc;
}

void bnxt_ttxp_init(struct bnxt *bp)
{
	struct bnxt_hw_resc *hw_resc = &bp->hw_resc;
	struct bnxt_ttxp *ttxp;
	int rc;

	if (!(bp->fw_cap & BNXT_FW_CAP_TIMED_TX_PACING) ||
	    !hw_resc->max_tx_rings) {
		bnxt_ttxp_free(bp);
		return;
	}
	ttxp = kzalloc(sizeof(*ttxp), GFP_KERNEL);
	if (!ttxp)
		return;
	ttxp->ring_rates = kcalloc(hw_resc->max_tx_rings,
				   sizeof(*ttxp->ring_rates), GFP_KERNEL);
	if (!ttxp->ring_rates) {
		kfree(ttxp);
		return;
	}
	bp->ttxp = ttxp;
	rc = bnxt_hwrm_ttx_pacing_rate_prof_query(bp);
	if (!rc)
		rc = bnxt_hwrm_ttx_pacing_rate_query(bp);
	if (rc)
		bnxt_ttxp_free(bp);
}

void bnxt_ttxp_free(struct bnxt *bp)
{
	if (bp->ttxp) {
		kfree(bp->ttxp->ring_rates);
		bp->ttxp->ring_rates = NULL;
	}
	kfree(bp->ttxp);
	bp->ttxp = NULL;
}

int bnxt_set_tx_maxrate(struct net_device *dev, int queue, u32 rate)
{
	struct bnxt *bp = netdev_priv(dev);
	struct bnxt_ttxp *ttxp = bp->ttxp;
	u32 hw_rate = 0, best_rate = 0;
	struct bnxt_tx_ring_info *txr;
	int i;

	if (!ttxp)
		return -EOPNOTSUPP;
#if defined(HAVE_ETF_QOPT_OFFLOAD)
	if (bp->etf_tx_ring_map) {
		if (test_bit(queue, bp->etf_tx_ring_map)) {
			netdev_warn(dev, "Tx queue %d already has ETF enabled.\n",
				    queue);
			return -EOPNOTSUPP;
		}
	}
#endif
	if (rate == netdev_get_tx_queue(dev, queue)->tx_maxrate)
		return 0;
	if (rate / 8 * ttxp->rate_factor > TX_BD_TIMED_RATE_MASK)
		return -ERANGE;
	if (!rate)
		goto maxrate_set;

	hw_rate = rate * ttxp->rate_factor;
	for (i = 0; i < BNXT_MAX_TTXP_RATES; i++) {
		if (ttxp->rates[i] && ttxp->rates[i] <= hw_rate)
			best_rate = ttxp->rates[i];
		else if (i)
			break;
	}
	if (best_rate != hw_rate)
		return -ERANGE;

maxrate_set:
	ttxp->ring_rates[queue] = hw_rate;
	if (test_bit(BNXT_STATE_OPEN, &bp->state)) {
		txr = &bp->tx_ring[bp->tx_ring_map[queue]];
		/* BD rates are in bytes per sec. */
		txr->pacing_rate = hw_rate / 8;
		if (hw_rate)
			txr->pacing_enabled = false;
	}
	return 0;
}
