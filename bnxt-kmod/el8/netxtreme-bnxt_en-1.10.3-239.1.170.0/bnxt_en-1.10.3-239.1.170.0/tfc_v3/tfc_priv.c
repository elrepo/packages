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

