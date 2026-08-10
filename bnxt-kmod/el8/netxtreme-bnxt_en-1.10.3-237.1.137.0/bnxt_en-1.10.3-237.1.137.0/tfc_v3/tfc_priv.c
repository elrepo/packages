// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2023 Broadcom
 * All rights reserved.
 */

#include <linux/types.h>
#include "bnxt_compat.h"
#include "bnxt.h"
#include "tfc.h"
#include "tfc_priv.h"

u16
tfc_get_fid(struct tfc *tfcp)
{
	struct bnxt *bp = tfcp->bp;

	if (BNXT_VF(bp))
		return bp->vf.fw_fid;

	return bp->pf.fw_fid;
}

bool
tfc_bp_is_pf(struct tfc *tfcp)
{
	struct bnxt *bp = tfcp->bp;

	return !!(BNXT_PF(bp));
}

u16
tfc_bp_vf_max(struct tfc *tfcp)
{
	struct bnxt *bp = tfcp->bp;

	if (!BNXT_PF(bp)) {
		netdev_dbg(bp->dev, "%s: not a PF\n", __func__);
		return 0;
	}

	/* If not sriov, no vfs enabled */
	if (bp->pf.max_vfs)
		return bp->pf.first_vf_id + bp->pf.max_vfs;
	else
		return bp->pf.fw_fid;

}
