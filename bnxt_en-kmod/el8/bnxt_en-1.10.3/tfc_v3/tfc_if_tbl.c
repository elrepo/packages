// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2023 Broadcom
 * All rights reserved.
 */
#include <linux/types.h>
#include "bnxt_compat.h"
#include "bnxt.h"
#include "bnxt_hwrm.h"
#include "tfc.h"
#include "tfc_msg.h"
#include "tfc_util.h"

int tfc_if_tbl_set(struct tfc *tfcp, u16 fid,
		   const struct tfc_if_tbl_info *tbl_info,
		   const u8 *data, u8 data_sz_in_bytes)
{
	struct bnxt *bp = tfcp->bp;
	u16 sid;
	int rc;

	if (!tbl_info) {
		netdev_dbg(bp->dev, "%s: tbl_info is NULL\n", __func__);
		return -EINVAL;
	}

	if (tbl_info->dir >= CFA_DIR_MAX) {
		netdev_dbg(bp->dev, "%s: Invalid cfa dir: %d\n", __func__, tbl_info->dir);
		return -EINVAL;
	}

	if (tbl_info->rsubtype >= CFA_RSUBTYPE_IF_TBL_MAX) {
		netdev_dbg(bp->dev, "%s: Invalid if tbl subtype: %d\n", __func__,
			   tbl_info->rsubtype);
		return -EINVAL;
	}

	if (!BNXT_PF(bp) && !BNXT_VF_IS_TRUSTED(bp)) {
		netdev_dbg(bp->dev, "%s: bp not PF or trusted VF\n", __func__);
		return -EINVAL;
	}

	rc = tfo_sid_get(tfcp->tfo, &sid);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Failed to retrieve SID, rc:%d\n",
			   __func__, rc);
		return rc;
	}

	rc = tfc_msg_if_tbl_set(tfcp, fid, sid, tbl_info->dir,
				tbl_info->rsubtype, tbl_info->id,
				data_sz_in_bytes, data);
	if (rc)
		netdev_dbg(bp->dev, "%s: hwrm failed: %s:%s %d %d\n", __func__,
			   tfc_dir_2_str(tbl_info->dir),
			   tfc_if_tbl_2_str(tbl_info->rsubtype), tbl_info->id, rc);

	return rc;
}

int tfc_if_tbl_get(struct tfc *tfcp, u16 fid,
		   const struct tfc_if_tbl_info *tbl_info,
		   u8 *data, u8 *data_sz_in_bytes)
{
	struct bnxt *bp = tfcp->bp;
	u16 sid;
	int rc;

	if (!tbl_info) {
		netdev_dbg(bp->dev, "%s: tbl_info is NULL\n", __func__);
		return -EINVAL;
	}

	if (tbl_info->dir >= CFA_DIR_MAX) {
		netdev_dbg(bp->dev, "%s: Invalid cfa dir: %d\n", __func__, tbl_info->dir);
		return -EINVAL;
	}

	if (tbl_info->rsubtype >= CFA_RSUBTYPE_IF_TBL_MAX) {
		netdev_dbg(bp->dev, "%s: Invalid if tbl subtype: %d\n", __func__,
			   tbl_info->rsubtype);
		return -EINVAL;
	}

	if (!BNXT_PF(bp) && !BNXT_VF_IS_TRUSTED(bp)) {
		netdev_dbg(bp->dev, "%s: bp not PF or trusted VF\n", __func__);
		return -EINVAL;
	}

	rc = tfo_sid_get(tfcp->tfo, &sid);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Failed to retrieve SID, rc:%d\n",
			   __func__, rc);
		return rc;
	}

	rc = tfc_msg_if_tbl_get(tfcp, fid, sid, tbl_info->dir,
				tbl_info->rsubtype, tbl_info->id,
				data_sz_in_bytes, data);
	if (rc)
		netdev_dbg(bp->dev, "%s: hwrm failed: %s:%s %d %d\n", __func__,
			   tfc_dir_2_str(tbl_info->dir),
			   tfc_if_tbl_2_str(tbl_info->rsubtype), tbl_info->id, rc);
	return rc;
}
