// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2023 Broadcom
 * All rights reserved.
 */

#include <linux/types.h>
#include "bnxt_compat.h"
#include "bnxt.h"
#include "tfc.h"
#include "tfc_priv.h"

int
tfc_get_fid(struct tfc *tfcp, u16 *fw_fid)
{
	struct bnxt *bp = tfcp->bp;

	if (!fw_fid) {
		netdev_dbg(bp->dev, "%s: Invalid fw_fid pointer\n", __func__);
		return -EINVAL;
	}
	if (BNXT_VF(bp))
		*fw_fid = bp->vf.fw_fid;
	else
		*fw_fid = bp->pf.fw_fid;

	return 0;
}

int
tfc_get_pfid(struct tfc *tfcp, u16 *pfid)
{
	struct bnxt *bp = tfcp->bp;

	if (!pfid) {
		netdev_dbg(bp->dev, "%s: Invalid pfid pointer\n", __func__);
		return -EINVAL;
	}

	if (BNXT_VF(bp))
		*pfid = bp->vf.fw_fid;
	else
		*pfid = bp->pf.fw_fid;

	return 0;
}

int
tfc_bp_is_pf(struct tfc *tfcp, bool *is_pf)
{
	struct bnxt *bp = tfcp->bp;

	if (!is_pf) {
		netdev_dbg(bp->dev, "%s: invalid is_pf pointer\n", __func__);
		return -EINVAL;
	}

	if (BNXT_PF(bp)) {
		*is_pf = true;
		return 0;
	}
	*is_pf = false;
	return 0;
}

int tfc_bp_vf_max(struct tfc *tfcp, u16 *max_vf)
{
	struct bnxt *bp = tfcp->bp;

	if (!max_vf) {
		netdev_dbg(bp->dev, "%s: invalid max_vf pointer\n", __func__);
		return -EINVAL;
	}

	if (!BNXT_PF(bp)) {
		netdev_dbg(bp->dev, "%s: not a PF\n", __func__);
		return -EINVAL;
	}

	/* If not sriov, no vfs enabled */
	if (bp->pf.max_vfs)
		*max_vf = bp->pf.first_vf_id + bp->pf.max_vfs;
	else
		*max_vf = bp->pf.fw_fid;

	return 0;
}
