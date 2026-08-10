/* Broadcom NetXtreme-C/E network driver.
 *
 * Copyright (c) 2025 Broadcom Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 */

#ifndef BNXT_TTXP_H
#define BNXT_TTXP_H

/* Timed TX Pacing support */

#define BNXT_MAX_TTXP_RATES	128

struct bnxt_ttxp {
	u8 profile_id;
	u16 rate_factor;
	u32 rates[BNXT_MAX_TTXP_RATES];
	u32 *ring_rates;
};

#define BNXT_TTXP_PENDING(txr) (!!(txr)->pacing_rate ^ (txr)->pacing_enabled)
#define BNXT_TTXP_DISABLED(txr) (!(txr)->pacing_rate && !(txr)->pacing_enabled)

void bnxt_ttxp_init(struct bnxt *bp);
void bnxt_ttxp_free(struct bnxt *bp);
int bnxt_set_tx_maxrate(struct net_device *dev, int queue, u32 rate);
#endif /* BNXT_TTXP_H */
