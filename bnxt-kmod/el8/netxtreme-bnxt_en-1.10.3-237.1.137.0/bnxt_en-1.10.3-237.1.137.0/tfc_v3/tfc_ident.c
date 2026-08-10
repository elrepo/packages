// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#include <linux/types.h>
#include "tfc.h"

#include "tfc_msg.h"
#include "cfa_types.h"
#include "tfo.h"
#include "tfc_util.h"
#include "bnxt_compat.h"
#include "bnxt.h"

int tfc_identifier_alloc(struct tfc *tfcp, u16 fid, enum cfa_track_type tt,
			 struct tfc_identifier_info *ident_info)
{
	struct bnxt *bp = tfcp->bp;
	u16 sid;
	int rc;

	if (!ident_info) {
		netdev_dbg(bp->dev, "%s: Invalid ident_info pointer\n", __func__);
		return -EINVAL;
	}

	rc = tfo_sid_get(tfcp->tfo, &sid);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Failed to retrieve SID, rc:%d\n",
			   __func__, rc);
		return rc;
	}

	rc = tfc_msg_identifier_alloc(tfcp, ident_info->dir,
				      ident_info->rsubtype, tt,
				      fid, sid, &ident_info->id);
	if (rc)
		netdev_dbg(bp->dev, "%s: hwrm failed %s:%s, rc:%d\n",
			   __func__, tfc_dir_2_str(ident_info->dir),
			   tfc_ident_2_str(ident_info->rsubtype), rc);

	return rc;
}

int tfc_identifier_free(struct tfc *tfcp, u16 fid,
			const struct tfc_identifier_info *ident_info)
{
	struct bnxt *bp = tfcp->bp;
	u16 sid;
	int rc;

	if (!ident_info) {
		netdev_dbg(bp->dev, "%s: Invalid ident_info pointer\n", __func__);
		return -EINVAL;
	}

	rc = tfo_sid_get(tfcp->tfo, &sid);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Failed to retrieve SID, rc:%d\n",
			   __func__, rc);
		return rc;
	}

	rc = tfc_msg_identifier_free(tfcp, ident_info->dir,
				     ident_info->rsubtype,
				     fid, sid, ident_info->id);
	if (rc)
		netdev_dbg(bp->dev, "%s: hwrm failed  %s:%s:%d, rc:%d\n",
			   __func__, tfc_dir_2_str(ident_info->dir),
			   tfc_ident_2_str(ident_info->rsubtype), ident_info->id, rc);

	return rc;
}
