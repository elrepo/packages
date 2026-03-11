// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2023 Broadcom
 * All rights reserved.
 */

#include <linux/types.h>
#include <linux/vmalloc.h>
#include "tfc.h"
#include "tfo.h"
#include "bnxt_compat.h"
#include "bnxt.h"
#include "bnxt_mpc.h"
#include "cfa_bld_mpcops.h"
#include "tfc_priv.h"

/* The tfc_open and tfc_close APIs may only be used for setting TFC software
 * state.  They are never used to modify the HW state.  That is, they are not
 * allowed to send HWRM messages.
 */

int tfc_open(struct tfc *tfcp)
{
	struct bnxt *bp = tfcp->bp;
	bool is_pf;
	int rc;

	/* Initialize the TF object */
	if (tfcp->tfo) {
		netdev_dbg(bp->dev, "%s: tfc_opened already.\n", __func__);
		return -EINVAL;
	}

	rc = tfc_bp_is_pf(tfcp, &is_pf);
	if (rc)
		return rc;

	tfo_open(&tfcp->tfo, bp, is_pf);

	return 0;
}

int tfc_close(struct tfc *tfcp)
{
	struct bnxt *bp = tfcp->bp;
	bool valid;
	u16 sid;
	u8 tsid;
	int rc = 0;

	/* Nullify the TF object */
	if (tfcp->tfo) {
		if (tfo_sid_get(tfcp->tfo, &sid) == 0) {
			/* If no error, then there is a valid SID which means
			 * that the FID is still associated with the SID.
			 */
			netdev_dbg(bp->dev,
				   "%s: There is still a session associated with this object.\n",
				   __func__);
		}

		for (tsid = 0; tsid < TFC_TBL_SCOPE_MAX; tsid++) {
			rc = tfo_ts_get(tfcp->tfo, tsid, NULL, NULL, &valid, NULL);
			if (!rc && valid) {
				netdev_dbg(bp->dev,
					   "%s: There is a tsid %d still associated\n",
					   __func__, tsid);
			}
		}
		tfo_close(&tfcp->tfo, bp);
	}
	return rc;
}
